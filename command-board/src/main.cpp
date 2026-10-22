#include <Arduino.h>
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
#define I2S_MCLK 4        // PCM1808 Master Clock
#define I2S_WS 23
#define I2S_SCK 22
#define I2S_SD_MIC 21     // Dedicated to INMP441
#define I2S_SD_ADC 5      // Dedicated to PCM1808 (D5)

// --- Audio & FFT Config ---
#define SAMPLES 256              // Must be a power of 2
//#define SAMPLING_FREQ 16000      // 16kHz is standard for voice/music sync
#define SAMPLING_FREQ 48000      // 16kHz is standard for voice/music sync
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
float uiSquelchADC = 5.0;    // Low gate for clean line-in
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


void configureI2SForMode(SystemMode mode) {
    // Uninstall the existing driver before reconfiguring
    i2s_driver_uninstall(I2S_PORT);

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLING_FREQ,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, 
        // NOTE: Global communication_format removed from here
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE
    };
    if (mode == MODE_ADC) {
        // PCM1808 Hardware Profile
        //i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT; 
        i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT; // Flips phase to capture LIN
        i2s_config.communication_format = I2S_COMM_FORMAT_STAND_MSB; // Reads Left-Justified
        i2s_config.use_apll = false; // BYPASS APLL BUG: Force standard system clock         
        //i2s_config.use_apll = true;             
        pin_config.mck_io_num = I2S_MCLK;       
        pin_config.data_in_num = I2S_SD_ADC;
    } else { 
        // INMP441 Hardware Profile (Default)
        i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT; 
        i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S; // Reads Standard I2S
        i2s_config.use_apll = false;            
        pin_config.mck_io_num = I2S_PIN_NO_CHANGE; 
        pin_config.data_in_num = I2S_SD_MIC;
    }

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
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
        
        // Reboot the I2S hardware if swapping between audio sources
        if (currentMode == MODE_I2S_MIC || currentMode == MODE_ADC) {
            configureI2SForMode(currentMode);
        }
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

    configureI2SForMode(currentMode); // Initializes default state
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(100); 
    FastLED.clear();
    FastLED.show();
    Serial.println("Command Board Booted. Awaiting Input...");
}

void loop() {
    checkButtons();

            if (currentMode == MODE_I2S_MIC || currentMode == MODE_ADC) {
                size_t bytesIn = 0;
                int32_t rawSample[SAMPLES] = {0}; 
                
                esp_err_t result = i2s_read(I2S_PORT, &rawSample, sizeof(int32_t) * SAMPLES, &bytesIn, portMAX_DELAY);
                if (result == ESP_OK && bytesIn > 0) {
                // --- RAW HARDWARE DIAGNOSTIC DUMPER ---
                Serial.println("\n====================================");
                Serial.printf("MODE: %s\n", (currentMode == MODE_ADC) ? "PCM1808 (ADC)" : "INMP441 (MIC)");
                
                // Print the first 4 raw 32-bit samples exactly as they arrive from the chip
                for (int i = 0; i < 4; i++) {
                    Serial.printf("Sample [%d]: HEX: 0x%08X  |  DEC: %11d\n", i, rawSample[i], rawSample[i]);
                }
                Serial.println("====================================");
                
                // Artificial delay so the serial monitor doesn't scroll at lightspeed
                delay(300); 
        }
     /*   if (result == ESP_OK && bytesIn > 0) {
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
                    // Subtract a heavy noise floor from the lowest bins to erase 120Hz EMI
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

            // 4. Transmit Binary over UART (and print text to Mac for debugging)
            if (millis() - lastTransmission > 25) { // Throttled to ~40fps
                
                // Blast the 21-byte binary struct to the Render Board
                Serial2.write((uint8_t*)&payload, sizeof(payload)); 
                
                // Print a clean summary to your Mac terminal so you can verify the math
                Serial.printf("%s: | BEAT: %d | Raw: %3d | Sm: %3d | Avg: %3d | Bins: [%3d %3d %3d ...]\n", 
                              (currentMode == MODE_ADC) ? "ADC" : "Mic",
                              payload.beat, payload.volumeRaw, payload.volumeSm, payload.sampleAvg,
                              payload.fftResult[0], payload.fftResult[7], payload.fftResult[15]);
                lastTransmission = millis();
            }
        }*/
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