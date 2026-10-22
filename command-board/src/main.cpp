#include <Arduino.h>
#include <esp_err.h>
#include <math.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>
#define FASTLED_INTERNAL
#include <FastLED.h>

#define LED_PIN 12
#define NUM_LEDS 15
CRGB leds[NUM_LEDS];
float peakDot = 0.0; // Tracks the falling peak for the Noise Meter

// --- UART Bridge ---
#define UART_RX_PIN 14
#define UART_TX_PIN 27

// --- Button Pins ---
#define BTN_ANIM 18
#define BTN_I2S  19
#define BTN_ADC  32
#define BTN_GAME 25
#define BTN_CAM  33  // Swapped from 23 to avoid I2S collision

// --- I2S Mic/ADC Pins ---
#define I2S_MCLK 4        // Manually driven by LEDC PWM

// Hardware contract: PCM1808 MD1=LOW, MD0=LOW (slave), FMT=LOW (I2S).
// Set this true ONLY if the PCM1808 FMT pin is actually HIGH.
constexpr bool PCM1808_LEFT_JUSTIFIED = false;
constexpr bool PCM1808_USE_APLL = true;
// Diagnostic mode matches the uploaded file: FFT/LED/UART audio disabled.
// Set to 0 after validating input to enable the existing processing block.
#define AUDIO_DIAGNOSTICS 0
// ADC stereo DMA word to feed to the existing mono FFT (0 or 1).
// Identify the desired input using the slot diagnostics; do not assume LIN.
//xxxconstexpr size_t ADC_FFT_SLOT = 0; <--wrong slot?!!??!
constexpr size_t ADC_FFT_SLOT = 1;
static_assert(ADC_FFT_SLOT < 2, "ADC_FFT_SLOT must be 0 or 1");
#define I2S_WS 23
#define I2S_SCK 22
#define I2S_SD_MIC 21     // Dedicated to INMP441
#define I2S_SD_ADC 5      // Dedicated to PCM1808 (D5)

// --- Audio & FFT Config ---
#define SAMPLES 256              // Must be a power of 2
#define SAMPLING_FREQ 48000      // Requested LRCK; stereo BCK = 64 * fs
#define I2S_PORT I2S_NUM_0

unsigned long seqNum = 0;

struct __attribute__((packed)) WLEDAudioPayload {
    uint8_t mode;           // 1 = I2S Mic
    uint8_t beat;           // 1 if beat detected, 0 otherwise
    uint8_t volumeRaw;      // Unsmoothed total volume (0-255)
    uint8_t volumeSm;       // Smoothed total volume (0-255)
    uint8_t sampleAvg;      // Long-term average volume (0-255)
    uint8_t fftResult[16];  // WLED's standard 16 frequency bins (0-255)
};

// --- USER TUNING KNOBS (Like WLED UI) ---
// Base multiplier (1.0 = normal, 2.0 = double sensitivity)
float uiGainMic = 0.2;       // Gain for INMP441 Microphone
float uiGainADC = 1.0;       // Gain for PCM1808 Line-In
float uiSquelchMic = 120.0;  // High gate to block typing/room noise
//xxxfloat uiSquelchADC = 5.0;    // Low gate for clean line-in
float uiSquelchADC = 0.0;    // Low gate for clean line-in
bool  uiAGCEnabled = true;   // Toggle Automatic Gain Control

// Custom EQ: Deafens the lowest bins to desk thumping/typing, leaves the rest at 1.0 (100%)
//float binEQ[16] = {0.2, 0.4, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
float binEQ[16] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

// --- WLED Audio Engine State ---
float rawAvgFloat = 0.0;
float volumeSmFloat = 0.0;
float sampleAvgFloat = 0.0;
float agcMultiplier = 1.0;
float binSm[16] = {0};
unsigned long lastBeatTime = 0;

// WLED's logarithmic distribution for 128 FFT bins (indices to sum up to)
const int wledBinBoundaries[16] = {2, 3, 4, 5, 7, 9, 12, 16, 21, 28, 37, 48, 63, 81, 103, 127};

double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQ);

// --- System State ---
enum SystemMode { MODE_ANIMATION, MODE_I2S_MIC, MODE_ADC, MODE_VIDEOGAME, MODE_MOCAP };
SystemMode currentMode = MODE_ANIMATION;
unsigned long lastTransmission = 0;
unsigned long lastButtonPress = 0;


bool i2sInstalled = false;
bool i2sReady = false;
unsigned long audioReadyAt = 0;
unsigned long lastAudioDiagnostic = 0;

bool configureI2SForMode(SystemMode mode) {
    i2sReady = false;
    if (i2sInstalled) {
        esp_err_t err = i2s_driver_uninstall(I2S_PORT);
        if (err != ESP_OK) {
            Serial.printf("I2S uninstall failed: %s\n", esp_err_to_name(err));
            return false;
        }
        i2sInstalled = false;
    }
    // No I2S driver needed in animation/game/camera modes.
    if (mode != MODE_ADC && mode != MODE_I2S_MIC) return true;

    const bool adc = (mode == MODE_ADC);
    i2s_config_t config = {};
    config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    config.sample_rate = SAMPLING_FREQ;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    // Capture BOTH ADC channels; retain the existing INMP441 right-slot setting.
    // INMP441 L/R must be HIGH for ONLY_RIGHT (LOW requires ONLY_LEFT).
    config.channel_format = adc ? I2S_CHANNEL_FMT_RIGHT_LEFT
                                : I2S_CHANNEL_FMT_ONLY_RIGHT;
    config.communication_format = (adc && PCM1808_LEFT_JUSTIFIED)
        ? I2S_COMM_FORMAT_STAND_MSB : I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 8;
    config.dma_buf_len = 64;
    config.use_apll = adc && PCM1808_USE_APLL;
    config.tx_desc_auto_clear = false;
    config.fixed_mclk = 0;
    config.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    config.bits_per_chan = I2S_BITS_PER_CHAN_32BIT;

    i2s_pin_config_t pins = {};
    //xxx pins.mck_io_num = adc ? I2S_MCLK : I2S_PIN_NO_CHANGE;
    pins.mck_io_num = I2S_PIN_NO_CHANGE; // Disconnect buggy driver from clock
    pins.bck_io_num = I2S_SCK;
    pins.ws_io_num = I2S_WS;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num = adc ? I2S_SD_ADC : I2S_SD_MIC;

    esp_err_t err = i2s_driver_install(I2S_PORT, &config, 0, nullptr);
    if (err != ESP_OK) {
        Serial.printf("I2S install failed: %s\n", esp_err_to_name(err));
        return false;
    }
    i2sInstalled = true;
    err = i2s_set_pin(I2S_PORT, &pins);
    if (err != ESP_OK) {
        Serial.printf("I2S pin setup failed: %s\n", esp_err_to_name(err));
        // Keep installed tracking accurate if cleanup itself fails.
        const esp_err_t cleanup = i2s_driver_uninstall(I2S_PORT);
        i2sInstalled = (cleanup != ESP_OK);
        return false;
    }
    // Drain continuously during ADC reset/fade-in; do not pause DMA reads.
    audioReadyAt = millis() + 500;
    lastAudioDiagnostic = millis();
    i2sReady = true;
    Serial.printf("I2S configured: %s, %s, requested fs=%d Hz\n",
        adc ? "PCM1808 stereo" : "INMP441 right slot",
        (adc && PCM1808_LEFT_JUSTIFIED) ? "left-justified" : "I2S",
        SAMPLING_FREQ);
    if (adc) {
        Serial.printf("Clock targets: MCLK=%lu Hz on GPIO%d, BCK=%lu Hz, LRCK=%d Hz\n",
            (unsigned long)SAMPLING_FREQ * 256, I2S_MCLK,
            (unsigned long)SAMPLING_FREQ * 64, SAMPLING_FREQ);
        Serial.println("Targets/configuration are not a measurement at the ADC pins.");
    }
    return true;
}

// 24-bit signed PCM occupies the upper 24 bits of each 32-bit DMA word.
// Xtensa ESP32 uses arithmetic right shift for signed int32_t.
void printAudioSlot(const int32_t* words, size_t frames,
                    size_t stride, size_t slot) {
    int32_t lo = INT32_MAX, hi = INT32_MIN;
    double sum = 0.0, sumSquares = 0.0;
    size_t badPadding = 0;
    for (size_t i = 0; i < frames; ++i) {
        const int32_t word = words[i * stride + slot];
        const int32_t sample = word >> 8;
        if (sample < lo) lo = sample;
        if (sample > hi) hi = sample;
        sum += sample;
        sumSquares += (double)sample * sample;
        if (((uint32_t)word & 0xffu) != 0) ++badPadding;
    }
    const double mean = sum / frames;
    const double variance = sumSquares / frames - mean * mean;
    const double acRms = sqrt(variance > 0.0 ? variance : 0.0);
    Serial.printf("slot%u raw=0x%08lX signed24=%ld min=%ld max=%ld AC_RMS=%.1f pad!=0:%u/%u\n",
        (unsigned)slot, (unsigned long)(uint32_t)words[slot],
        (long)(words[slot] >> 8), (long)lo, (long)hi, acRms,
        (unsigned)badPadding, (unsigned)frames);
}

void checkButtons() {
    // Simple 200ms debounce
    if (millis() - lastButtonPress < 200) return;

    SystemMode newMode = currentMode;
    if (digitalRead(BTN_ANIM) == LOW) newMode = MODE_ANIMATION;
    else if (digitalRead(BTN_I2S) == LOW)  newMode = MODE_I2S_MIC;
    else if (digitalRead(BTN_ADC) == LOW)  newMode = MODE_ADC;
    else if (digitalRead(BTN_GAME) == LOW) newMode = MODE_VIDEOGAME;
    else if (digitalRead(BTN_CAM) == LOW)  newMode = MODE_MOCAP;

    if (newMode != currentMode) {
        currentMode = newMode;
        lastButtonPress = millis();
        Serial.printf("Mode Switched to: %d\n", currentMode);
        
        // Also release I2S when leaving an audio mode.
        configureI2SForMode(currentMode);
    }
}

void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

    pinMode(BTN_ANIM, INPUT_PULLUP);
    pinMode(BTN_I2S, INPUT_PULLUP);
    pinMode(BTN_ADC, INPUT_PULLUP);
    pinMode(BTN_GAME, INPUT_PULLUP);
    pinMode(BTN_CAM, INPUT_PULLUP);

    // --- MANUALLY SYNTHESIZE MCLK (12.288 MHz) ---
    ledcSetup(0, 12288000, 2);   
    ledcAttachPin(I2S_MCLK, 0);  
    ledcWrite(0, 2);

    configureI2SForMode(currentMode); // No driver until an audio mode is selected
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(100); 
    FastLED.clear();
    FastLED.show();
    Serial.println("Command Board Booted. Awaiting Input...");
}

void loop() {
    checkButtons();

    if (currentMode == MODE_I2S_MIC || currentMode == MODE_ADC) {
        if (!i2sReady) {
            delay(1); // Configuration failure was printed; buttons still work.
            return;
        }
        const size_t channels = (currentMode == MODE_ADC) ? 2 : 1;
        int32_t words[SAMPLES * 2] = {};
        size_t bytesIn = 0;
        const size_t requested = SAMPLES * channels * sizeof(int32_t);
        const esp_err_t result = i2s_read(I2S_PORT, words, requested,
                                         &bytesIn, pdMS_TO_TICKS(50));
        if (result != ESP_OK || bytesIn != requested) {
            if (millis() - lastAudioDiagnostic >= 1000) {
                Serial.printf("I2S read: %s, bytes=%u/%u\n",
                    esp_err_to_name(result), (unsigned)bytesIn, (unsigned)requested);
                lastAudioDiagnostic = millis();
            }
            return; // Never process unfilled samples as valid audio.
        }
        if ((int32_t)(millis() - audioReadyAt) < 0) return;

#if AUDIO_DIAGNOSTICS
        // Read continuously; throttle printing, not acquisition.
        if (millis() - lastAudioDiagnostic >= 300) {
            Serial.println(currentMode == MODE_ADC ? "PCM1808" : "INMP441");
            for (size_t slot = 0; slot < channels; ++slot) {
                printAudioSlot(words, SAMPLES, channels, slot);
            }
            lastAudioDiagnostic = millis();
        }
#else
        int32_t rawSample[SAMPLES] = {};
        for (size_t i = 0; i < SAMPLES; ++i) {
            rawSample[i] = words[i * channels +
                ((currentMode == MODE_ADC) ? ADC_FFT_SLOT : 0)];
        }
        {
            double mean = 0;

            // 1. DC Offset Removal
            for (int i = 0; i < SAMPLES; i++) {
                vReal[i] = rawSample[i] >> 14; 
                vImag[i] = 0.0;
                mean += vReal[i];
            }
            mean /= SAMPLES;
            for (int i = 0; i < SAMPLES; i++) {
                vReal[i] -= mean; 
            }

            // 2. Perform FFT
            FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
            FFT.compute(FFTDirection::Forward);
            FFT.complexToMagnitude();

                // 3. WLED 16-Bin Mapping & AC Hum Killer & Base Gain/EQ
            WLEDAudioPayload payload = {0};
            payload.mode = (currentMode == MODE_ADC) ? 2 : 1;
    
            float activeGain = (currentMode == MODE_ADC) ? uiGainADC : uiGainMic;
            float activeSquelch = (currentMode == MODE_ADC) ? uiSquelchADC : uiSquelchMic;
            bool useHumKiller = (currentMode == MODE_I2S_MIC);
            float rawTotalVolume = 0;
            float binRaw[16] = {0};
            int currentFftBin = 2; 

            for (int i = 0; i < 16; i++) {
                float binSum = 0;
                int count = 0;
                while (currentFftBin <= wledBinBoundaries[i] && currentFftBin < (SAMPLES/2)) {
                    double mag = vReal[currentFftBin];
                    
                    // --- AC HUM KILLER ---
                    // Empirical low-bin noise gate; 48k/256 gives 187.5 Hz/bin.
                    // This FFT cannot isolate a 120 Hz hum component.
                    double noiseFloor = 500.0;
                    if (useHumKiller && currentFftBin <= 4) {
                        noiseFloor = 2500.0; // Only applied to the Mic
                    }
                    
                    if (isnan(mag) || isinf(mag) || mag < noiseFloor) {
                        mag = 0.0;
                    } else {
                        mag -= noiseFloor;
                    }
                    
                    binSum += mag;
                    count++;
                    currentFftBin++;
                }
                
                    // Average the bucket and apply the DYNAMIC User Gain
                binRaw[i] = ((count > 0) ? (binSum / count) / 40.0 : 0) * activeGain * binEQ[i];
                rawTotalVolume += binRaw[i];
            }

            // ==========================================
            // --- TRUE WLED AUDIO MATH ---
            // ==========================================
            
            // A. Squelch (Noise Gate)
            // Kills all data if the room is quieter than the user's threshold
            if (rawTotalVolume < activeSquelch) {
                rawTotalVolume = 0;
                for(int i=0; i<16; i++) binRaw[i] = 0;
            }

            // B. AGC (Automatic Gain Control) 
            if (uiAGCEnabled) {
                if (rawTotalVolume > 0) {
                    rawAvgFloat = (rawAvgFloat * 0.99) + (rawTotalVolume * 0.01);
                }
                if (rawAvgFloat > 1.0) {
                    agcMultiplier = 127.0 / rawAvgFloat; 
                    if (agcMultiplier > 2.0) agcMultiplier = 2.0; // Capped at 2x to prevent panic in quiet rooms
                    if (agcMultiplier < 0.3) agcMultiplier = 0.3; 
                }
            } else {
                agcMultiplier = 1.0; // Lock multiplier to 1 if AGC is disabled
            }

            // C. Apply AGC and EMA Smoothing to Individual Bins
            float volumeRawFloat = rawTotalVolume * agcMultiplier;
            float attack = 0.4;  
            float decay = 0.15;  

            for (int i = 0; i < 16; i++) {
                float scaledBin = binRaw[i] * agcMultiplier;
                binSm[i] += ((scaledBin > binSm[i]) ? attack : decay) * (scaledBin - binSm[i]);
                
                int finalBin = (int)binSm[i];
                if (finalBin < 2) finalBin = 0; // Micro-squelch for residual AC hum
                payload.fftResult[i] = min(255, finalBin); 
            }

            // D. SCALED Averages & Smoothing (Total Volume)
            sampleAvgFloat = (sampleAvgFloat * 0.99) + (volumeRawFloat * 0.01);
            sampleAvgFloat = max(64.0f, sampleAvgFloat); // Prevents the long-term average from crashing in silence

            volumeSmFloat += ((volumeRawFloat > volumeSmFloat) ? attack : decay) * (volumeRawFloat - volumeSmFloat);

            // E. Classic WLED Beat Detection
            float beatThreshold = 1.4; // 40% above average
            if (volumeRawFloat > 90.0 && volumeRawFloat > (sampleAvgFloat * beatThreshold) && (millis() - lastBeatTime > 250)) {
                payload.beat = 1;
                lastBeatTime = millis();
            } else {
                payload.beat = 0;
            }

            // F. Finalize Payload Variables
            payload.volumeRaw = min(255, (int)volumeRawFloat);
            payload.volumeSm  = min(255, (int)volumeSmFloat);
            payload.sampleAvg = min(255, (int)sampleAvgFloat);

            FastLED.clear();

            // 1. Map the smoothed 0-255 volume to the 15-LED strand
            int litLeds = map(payload.volumeSm, 0, 255, 0, NUM_LEDS);
            if (litLeds > NUM_LEDS) litLeds = NUM_LEDS;

            // 2. Draw the VU bar: Green -> Yellow -> Red
            for (int i = 0; i < litLeds; i++) {
                if (i < 9) {
                    leds[i] = CRGB::Green;
                } else if (i < 13) {
                    leds[i] = CRGB::Yellow;
                } else {
                    leds[i] = CRGB::Red;
                }
            }

                // 3. Calculate the falling Peak Dot
            if (litLeds > peakDot) {
                peakDot = litLeds; // Snap to the loudest sound
            } else {
                peakDot -= 0.15;   // Gravity pulls it down
            }
            if (peakDot < 0) peakDot = 0;

            // 4. Draw the Peak Dot
            // Only draw it if there is actual volume (peakDot >= 1.0)
            if (peakDot >= 1.0) {
                // Subtract 1 to perfectly align with the 0-indexed LED array
                int peakIndex = (int)peakDot - 1; 
                
                if (peakIndex >= 0 && peakIndex < NUM_LEDS) {
                    leds[peakIndex] = payload.beat ? CRGB::White : CRGB::Blue;
                }
            }

            FastLED.show();
            // ==========================================

            /*
            // 4. Transmit Binary over UART (and print text to Mac for debugging)
            if (millis() - lastTransmission > 25) { // Throttled to ~40fps
                
                // Blast the 21-byte binary struct to the Render Board
                //xxx Serial2.write((uint8_t*)&payload, sizeof(payload)); 
                
                // Print a clean summary to your Mac terminal so you can verify the math
                Serial.printf("%s: | BEAT: %d | Raw: %3d | Sm: %3d | Avg: %3d | Bins: [%3d %3d %3d ...]\n", 
                              (currentMode == MODE_ADC) ? "ADC" : "Mic",
                              payload.beat, payload.volumeRaw, payload.volumeSm, payload.sampleAvg,
                              payload.fftResult[0], payload.fftResult[7], payload.fftResult[15]);
                lastTransmission = millis();
            }*/
           // 4. Transmit Text String over UART for Debugging via Render Board
            if (millis() - lastTransmission > 25) { // Throttled to ~40fps
                
                char debugStr[100];
                snprintf(debugStr, sizeof(debugStr), "%s: | BEAT: %d | Raw: %3d | Sm: %3d | Avg: %3d | Bins: [%3d %3d %3d ...]", 
                        (currentMode == MODE_ADC) ? "ADC" : "Mic",
                        payload.beat, payload.volumeRaw, payload.volumeSm, payload.sampleAvg,
                        payload.fftResult[0], payload.fftResult[7], payload.fftResult[15]);

                // Send the text string to the Render Board (it will read and print this via readStringUntil)
                Serial2.println(debugStr); 
                
                // Also print it locally to the Command Board's terminal just in case
                Serial.println(debugStr);
                
                lastTransmission = millis();
            }
        }
#endif
    }
    else {
        // Heartbeat for non-audio modes
        if (millis() - lastTransmission > 1000) {
            seqNum++;
            String modeName = (currentMode == MODE_ANIMATION) ? "ANIM" : 
                              (currentMode == MODE_ADC) ? "ADC" : 
                              (currentMode == MODE_VIDEOGAME) ? "GAME" : "MOCAP";
            
            String msg = "SEQ:" + String(seqNum) + " | MODE:" + modeName + " | STATUS:IDLE";
            Serial2.println(msg);
            Serial.println(msg);
            lastTransmission = millis();
        }
    }
}
