/*
 * AUDIO PROCESSING — EXECUTIVE SUMMARY
 *
 * Capture 31,250 samples per second per channel. Every 256 new samples,
 * analyze a 512-sample window for RMS volume and sixteen frequency bands.
 * Source gain/noise floors calibrate the input; shared envelopes shape display
 * levels. A continuous bass detector reports kicks. A general attack detector
 * measures sudden increases across the linear bands before display shaping;
 * TempoTracker reuses that measurement to estimate BPM and a beat grid.
 * Neither detector identifies an instrument or understands speech syllables.
 *
 * UART sends snapshots at nominal 60 Hz: volume, spectrum, kicks, general
 * attack count/strength/band mask, and tempo/phase/confidence/status. Pending
 * attacks are retained between sends. The renderer chooses artistic thresholds,
 * cooldowns and visual peak holds. Tempo uses history-based phase alignment and
 * lock hysteresis, but half/double-time ambiguity and tempo ramps remain limits.
 * Both boards must use shared protocol version 4. No raw audio crosses UART;
 * Slow AGC scales display levels only; musical-structure classification is absent.
 *
 * Animations use ONE render-side AudioFrame, not this wire payload: flat
 * volume/band values, bassKick/midAttack/highAttack and usable BPM, with
 * beatPulse(N)/beatAngle(N) helpers. The adapter owns event retention,
 * freshness and musical-clock smoothing; effects do not decode packet flags.
 */
 
/*
 * AUDIO PROCESSING PRIMER: FROM WAVEFORM TO ANIMATION PAYLOAD
 *
 * This is our Cubina audio contract. It describes the common RMS/float FFT
 * implementation and how its AudioPayload summary reaches the renderer.
 *
 * 1. THREE CLOCKS OF WORK, NOT ONE
 *
 * The selected ADC/microphone produces 31,250 measurements per second per
 * channel. That is the waveform sample rate, not an LED update rate.
 * acquireAudio() reads 256 NEW samples per channel: 8.192 ms of sound.
 * AudioAnalysis::OverlapWindow retains the previous 256 samples, creating a
 * 512-sample analysis window: 16.384 ms. Adjacent windows overlap by 50%.
 * First publication requires two complete reads; we do not invent half a
 * window by padding startup data with zeros.
 *
 * UARTSender independently sends the latest complete summary at nominal 60 Hz
 * (one update every 16.667 ms). The render board independently displays at
 * up to 60 FPS. These tasks do not share phase or synchronized clocks.
 * The waveform stays here; only its compact summary crosses UART. Not every
 * intermediate FFT result is transmitted. Smoothing carries prior results
 * forward, but is not an exact average bounded by an LED frame interval.
 *
 * 2. WHAT A SAMPLE NUMBER MEANS
 *
 * I2S provides signed 32-bit containers with the audio in the upper bits.
 * Each channel is converted separately to float by dividing by 2147483648.0f.
 * A full-scale positive/negative peak is then approximately +1/-1.
 * We do not sum channels before measuring them: opposite-polarity stereo
 * signals could cancel even though both channels contain substantial sound.
 * float arithmetic also avoids the cost of software double arithmetic in the
 * FFT on the original ESP32. Slot diagnostics may still use double at their
 * much slower debug cadence; that is not the FFT hot path.
 *
 * 3. OVERALL VOLUME: A MEASUREMENT OF WAVEFORM LEVEL
 *
 * Over the full 512-sample window, remove each channel's mean/DC offset, square
 * its remaining values, and average those squares. This is channel power.
 * For stereo, average LEFT POWER and RIGHT POWER, then take the square root:
 *
 *     RMS = sqrt((leftPower + rightPower) / 2)
 *
 * For mono, use the one channel's power. The implementation duplicates that
 * channel internally, yielding the same formula/result. One silent stereo
 * channel reads about 0.707 times two equally strong channels: this is the
 * chosen stereo-average convention, not phase cancellation.
 *
 * RMS is waveform level, not perceived loudness, sound-pressure level, or dB.
 * Crucially, spectrum EQ, band widths, and spectrum gamma do not change volume.
 *
 * 4. THE COMMON CALIBRATION/GATE PATH
 *
 * Mic and ADC use the SAME processing. uiInputGainMic/uiInputGainADC calibrate
 * their different electrical levels. uiMasterSensitivity multiplies either.
 * uiNoiseFloorRMSMic/uiNoiseFloorRMSADC gate quiet input using its RAW RMS,
 * before display gain. Below the selected threshold, display targets are zero.
 * This gate does not identify mains hum or selectively remove a frequency.
 *
 * Let scale = 255 * masterSensitivity * sourceGain / uiFullScaleRMS.
 * volumeInputFloat = measuredRMS * scale * AGC factor (4b) when the gate is open,
 * else zero.
 * That diagnostic VIn is allowed above 255 so overdriving the display is visible.
 * volumeRawFloat is limited to 0..255 BEFORE smoothing for both sources.
 * There is no hidden above-255 envelope and no fabricated minimum average.
 * Increasing these gains changes the visuals, never the speakers or ADC range.
 * The old FFT-sum gain numbers have different units and cannot be copied here.
 *
 * 4b. AUTOMATIC GAIN CONTROL (uiAutoGain, default on)
 *
 * AutoGain tracks the loud end of the recent music and multiplies scale by
 * the factor that brings it to referenceRms: fast when the source gets louder,
 * 0.3 dB/s when it gets quieter, frozen in silence, limited to +18/-30 dB.
 * Only volume and band DISPLAY levels use it. Kicks, attacks and tempo do not.
 *
 * 5. FREQUENCY ANALYSIS: WHAT THE FFT INDEX MEANS
 *
 * The FFT analyzes one 16.384-ms piece of sound. Its frequency positions are
 * spaced by sampleRate / windowLength = 31250 / 512 = 61.035 Hz.
 * Index 1 corresponds to one oscillation in that window (about 61 Hz), index
 * 2 to two oscillations (122 Hz), index 4 to four oscillations (244 Hz), etc.
 * Index 0 describes DC/average voltage and is excluded from musical bands.
 * Sounds between positions contribute too; they do not fall through gaps.
 * The finite window spreads energy between neighboring FFT positions, so
 * these are measurement centers rather than sharp acoustic filter boundaries.
 * A longer window distinguishes nearby frequencies more finely; this short
 * window cannot reliably separate neighboring bass tones such as 50/60 Hz.
 *
 * We select the higher-power channel over each complete window for ONE FFT.
 * That prevents stereo cancellation without doubling FFT work. It can omit
 * content exclusive to the quieter channel; it is not a full stereo spectrum.
 * The selected channel can change on subsequent windows. Both channels still
 * contribute to overall volume and to their independent kick detectors.
 *
 * Copy the selected DC-removed waveform into the FFT working array. Apply a
 * cached Hamming window to reduce boundary leakage, compute ArduinoFFT<float>,
 * then convert complex results to magnitudes. The original overlap histories
 * are separate, so modifying FFT storage cannot corrupt the next window.
 *
 * AudioAnalysis::BandEdges groups positive FFT positions into sixteen bands,
 * from the first useful slot near 61 Hz to the upper spectrum. For each band,
 * sum squared magnitudes, normalize for FFT length/window power, and take
 * the square root. This is an approximate band RMS, not calibrated loudness.
 * Multiply by the common scale and binEQ[band], limit to 0..255, then apply
 * uiSpectrumGamma (0.60 initially) to lift weaker display levels. Gamma is
 * display compression, not AGC; zero remains zero. Then smooth the band.
 * AutoGain (section 4b below) multiplies the common scale for volume and bands.
 *
 * 6. SMOOTHING: WHAT THE THREE VOLUMES AND SIXTEEN BANDS REPRESENT
 *
 * The attack/release controls are milliseconds. Each update uses the AUDIO
 * hop duration (8.192 ms), not the render interval or UART interval:
 *
 *     weight = 1 - exp(-hopSeconds / timeConstantSeconds)
 *     level = previous + weight * (target - previous)
 *
 * Choose the attack constant when rising, release when falling. With current
 * defaults the envelope closes about 63% of a sustained difference in 16 ms
 * while rising, or 50 ms while falling. It does not wait that long to respond.
 * sampleAvg uses an 800-ms constant in both directions. A continuity gap resets
 * history/envelopes rather than pretending missing audio was captured.
 *
 * Payload fields (all display levels below are encoded bytes, 0..255):
 *   mode          Legacy source label: 1 mic, 2 PCM1808 analog, 3 SD.
 *   volumeRaw     Current full-window RMS, calibrated and limited.
 *   volumeSm      That bounded volume through attack/release smoothing.
 *   sampleAvg     Slower average of the same bounded volume.
 *   fftResult[16] Calibrated/compressed/smoothed band levels, low to high.
 *   beat          Whether an acquisition kick event awaits this publication.
 *   tempoBpm100   Estimated musical tempo * 100; zero before an estimate.
 *   tempoPhase    Position within the predicted beat, 0..65535.
 *   tempoConfidence Heuristic rhythm confidence, 0..255 (not probability).
 *   tempoFlags    Audio-active and rhythm-locked bits; separate from kicks.
 *
 * A field called Raw is unsmoothed DISPLAY level, not a raw PCM sample.
 * A frequency band's number identifies its range; its value identifies its
 * strength. 128 means midway up this encoded scale, not half acoustic energy.
 *
 * 7. KICKS ARE A SEPARATE MEASUREMENT, NOT THE FFT'S LOWEST DISPLAY BAND
 *
 * Feed each NEW normalized sample through the existing 35-180 Hz bass filters
 * and fast/slow power-envelope onset detector. Reused overlap samples NEVER
 * enter those filters twice. Each channel maintains continuous history; their
 * detections are combined with a shared 180-ms lockout to avoid duplicate hits.
 * The detector has a startup settling period and its own sensitivity/noise
 * controls. Visual gain, spectrum gamma and binEQ do not change its input.
 * It detects bass attacks, not BPM or the song's underlying repeating pulse.
 *
 * recordKickEvent() increments beatCount and sets beatPending. Publication
 * carries the byte flag and cumulative count; beatPending is then cleared.
 * The count can preserve an event occurring between UART sends even when the
 * latest byte flag has returned to zero. On render, consumeBeat() reports a
 * pending event once; several unconsumed events still coalesce into one.
 *
 * 8. PUBLICATION, PACKET LABELS, AND FAILURE BEHAVIOR
 *
 * CommandState::publishAudio() copies the completed payload/count under its
 * mutex and records local publication time. UARTSender takes a latest-state
 * snapshot without holding that mutex during transmission. SharedProtocol
 * wraps 21 legacy display bytes, the four-byte kick count and six explicit
 * tempo bytes plus seven attack bytes in a 38-byte Audio payload (protocol version 4).
 * The outer header includes version/type, demanded mode, readiness, boot
 * session, mode revision, sequence, and capturedMs. COBS frames it and CRC-16
 * checks integrity. The inner source label mode is NOT the outer Mode enum.
 *
 * A failed/partial read is not FFT input: discard continuity, reset filters
 * and envelopes, and rebuild a full window. Source changes reset these too.
 * The SD source remains an adapter hook requiring a working decoder/provider;
 * this code does not implement external MP3 playback or camera processing.
 *
 * Health samples/s reports samples processed per channel per elapsed time.
 * WorkMax measures post-read work (including display/publication/debug queue),
 * not the I2S wait or outside polling/yield. ReadErrors counts failed/partial
 * reads. These help validate headroom; they do not detect every DMA overflow
 * or reveal how old already-buffered audio is. Host tests cannot establish
 * actual ESP32 throughput. The design uses no additional processing board.
 *
 * TempoTracker reuses linear band RMS before display EQ/gamma/smoothing.
 * Adaptive-gated linear power increases form a novelty curve; a fixed 8.4-second history
 * supports an incremental autocorrelation search over roughly 60-200 BPM.
 * A regular dedicated kick train guides tempo and phase; spectral correlation
 * remains the fallback when kicks are absent or irregular. Hysteresis retains
 * lock through short gaps. Neither detector's thresholds or gain are changed.
 * A calibrated volume floor of 5 distinguishes activity from tiny idle noise.
 * This is an experimental musical clock: half/double-time ambiguity, weak
 * percussion and rapid tempo changes remain limitations. WorkMax includes it.
 * AGC (AudioAnalysis::AutoGain) scales display levels only; detectors and the
 * tempo activity floor keep the fixed scale. No spectrum peak holding exists.
 */

 /*
 * PCM1808 — EXECUTIVE SUMMARY
 *
 * The PCM1808 converts left and right analog audio voltages into digital
 * samples for the ESP32. Its analog circuitry uses 5 V; its digital circuitry
 * uses 3.3 V. On our board, grounding MD0, MD1, and FMT selects slave mode and
 * standard I2S format: the ESP32 supplies the clocks, and the PCM1808 supplies
 * the audio data.
 *
 * Our working configuration uses an 8 MHz system clock, a 31,250 Hz
 * channel-selection clock, and a 2 MHz bit clock. This combination suits the
 * clock-generation approach available with our wiring, avoiding inaccessible
 * native master-clock pins. Each channel supplies signed 24-bit audio inside
 * a 32-bit slot. The ESP32's I2S hardware handles the wire timing; our code
 * reads and normalizes the samples before calculating the visual summaries.
 */

/*
PCM1808 PRIMER -- THIS BOARD, THIS WIRING, THIS FIRMWARE

1. What the ADC does, and what its power pins mean

The PCM1808 turns two analog audio voltages (left and right) into signed,
24-bit PCM samples. PCM is the stream of numbers representing the waveform;
it is not MP3 compression, an FFT, a volume measurement, or beat detection.
Those visual measurements are calculated later by this program.

VDD is the digital supply, nominally 3.3 V. VCC is the analog-circuit supply,
nominally 5 V. The latter powers the converter's analog circuitry; it does
not mean that audio should swing from 0 to 5 V. Full-scale input is nominally
3 V peak-to-peak at the IC with a 5 V analog supply. Input coupling/biasing
belongs to the analog circuit on the breakout board. Analog and digital
grounds need a common reference, also shared with the ESP32.

2. Our actual connections and clock ownership

MD0, MD1, and FMT are soldered to GND. Therefore the PCM1808 is a SLAVE
using standard I2S. Software cannot override those grounded mode pins.

  Breakout label   Signal                  Connection / owner in this file
  SCK              SCKI / MCLK             GPIO4, ESP32 LEDC hardware output
  LRC              LRCK / WS               GPIO23, ESP32 I2S hardware output
  BCK              BCLK                    GPIO22, ESP32 I2S hardware output
  OUT / DOUT       Serial audio data       PCM1808 output -> ESP32 GPIO5 input

The ESP32 is the clock MASTER but the data RECEIVER:
I2S_MODE_MASTER | I2S_MODE_RX. Master does not mean sender of audio samples.
If the PCM1808 were configured as master instead, it would derive BCK and
LRCK from its supplied SCKI; the ESP32 would receive those clocks as slave.
SCKI is an INPUT to the PCM1808 in either mode. The PCM1808 does not create
its own system clock. An external oscillator or controller must provide it.

Naming trap: our breakout's SCK is MCLK, whereas the code name I2S_SCK
means BCK. These are different wires, with different frequencies.

3. Three clocks, three jobs -- all must agree

Think of one stereo frame as a pair of sample numbers: left plus right.
The sample rate fs counts these pairs per second, not individual data bits.

LRCK / WS identifies the channel. With our FMT=LOW standard-I2S format,
LOW identifies left and HIGH identifies right. One COMPLETE LRCK cycle
contains both channels, so its frequency is fs. It changes state twice per
stereo frame; 48 kHz LRCK does not mean only 48,000 transitions per second.

BCK advances the serial stream by one bit per complete clock cycle.
We allocate 32 bit-times per channel, so one frame uses 2 * 32 = 64 BCK
cycles. Only 24 bits per channel contain PCM1808 sample information.
32-bit slots do not make the ADC a 32-bit converter.

SCKI / MCLK runs the converter's internal engine. The supported ratios are
256, 384, or 512 times fs. Values such as 11.2896, 12.288, and 24.576 MHz
are examples, not a list of the only allowed system-clock frequencies.

For our 32-bit stereo slots and chosen 256-fs MCLK:

  fs / LRCK       BCK = 64 * fs       SCKI = 256 * fs
  44,100 Hz       2,822,400 Hz        11,289,600 Hz
  48,000 Hz       3,072,000 Hz        12,288,000 Hz
  31,250 Hz       2,000,000 Hz         8,000,000 Hz  <- this file

These clocks are NOT independent just because they have separate wires.
In master mode, the ADC divides SCKI to make its interface clocks. In slave
mode, we must supply a compatible, synchronized set. The ADC does not need
one particular initial phase between LRCK and SCKI, but their relationship
must not drift. Loss of synchronization can mute its output. A nonzero FFT
or a value of 255 alone cannot establish whether the clocking is correct.

4. APLL: a clock generator inside the ESP32, not inside the PCM1808

PLL means phase-locked loop: a circuit that generates a clock from a
reference clock. The ESP32 has a normal PLL-derived clock tree and a
separate Audio PLL (APLL), tunable for audio-friendly frequencies.
Neither the 80 MHz peripheral clock nor the 160 MHz I2S source is an
80/160 MHz crystal; they are derived clocks. A typical ESP32 board has
a 40 MHz crystal reference.

In this legacy driver, config.use_apll chooses the I2S clock source:
  true  -> use the Audio PLL, then divide it for I2S clocks;
  false -> use the normal PLL-derived I2S source (160 MHz on original ESP32).

APLL can help generate standard audio rates accurately. It is not broken
by definition, does not control gain, and does not choose the I2S data format.
It also does NOT take an incoming GPIO4 clock and lock the ESP32 to it.

Here, GPIO4 is driven by LEDC, a separate PWM peripheral. use_apll affects
I2S BCK/WS generation, not that LEDC output. Selecting APLL cannot magically
synchronize two separately generated clock sets. We leave it false for the
simple PLL-derived clock-divider arrangement described below.

5. Why 31.25 kHz instead of 48 kHz on this wiring?

On the ORIGINAL ESP32, native I2S MCLK output is restricted to GPIO0, GPIO1,
or GPIO3 -- not GPIO2. This is an MCLK-routing restriction, NOT a restriction
on 48 kHz audio or on the BCK/WS pins. GPIO0 is inconvenient on our board and
is a boot-strapping pin. GPIO1/3 are used by the normal UART0 serial link.
Changing drivers cannot remove that original-chip routing restriction.

We kept the SCKI wire on GPIO4 and use LEDC hardware to generate its clock.
Although its name says LED Control, LEDC is a hardware PWM generator; the
CPU is not toggling GPIO4 in a timing loop. This is an alternative clock
output path, not a software emulation of the audio protocol.

At 48 kHz, 256 * fs requires 12.288 MHz SCKI. Generating that with LEDC's
finite divider precision, while I2S independently generates BCK/WS, can
produce mismatched actual ratios. Merely requesting matching numbers is
insufficient. Native I2S MCLK would keep the clocks in one coordinated path.

At 31,250 Hz the arithmetic is much simpler:
  LEDC: 80 MHz APB / 10 = 8 MHz SCKI.
  I2S:  160 MHz / 20 = 8 MHz internal master-clock target;
        8 MHz / 4 = 2 MHz BCK; 2 MHz / 64 = 31,250 Hz LRCK.
Both peripheral paths derive from the ESP32 clock tree, with exactly
representable target ratios. This is why the present workaround is useful.

LEDC detail: ledcSetup(0, 8000000, 2) requests channel 0, 8 MHz, and a
2-bit PWM counter (4 counts per period). Its timer divider is therefore
80 MHz / (8 MHz * 4) = 2.5, which is representable by its fractional divider.
The OVERALL divide-by-10 is integer; not every internal divider is integer.
ledcWrite(0, 2) sets 2 of 4 counts HIGH, requesting 50% duty cycle.

These calculations assume the usual 80 MHz APB LEDC source on this original
ESP32/framework setup. They explain the design; they are not oscilloscope
measurements or a blanket claim of zero jitter/perfect phase lock.
The printed LEDC frequency is a configured/calculated value, not a physical
measurement at the ADC. Avoid changing clock sources or power-management
behavior without revisiting the complete clock relationship.

We are NOT recording at 48 kHz and discarding/resampling samples to 31.25 kHz.
The ADC is clocked to produce 31,250 stereo sample pairs per second from the
start. Analog music from the Bluetooth receiver has no digital sample-rate
requirement imposed on our ADC. The separately connected speakers keep
playing normally; this change neither slows nor changes their playback pitch.

For analysis, Nyquist is 15.625 kHz. The 512-sample FFT window spans
16.384 ms, with approximately 61.04 Hz between FFT frequencies. Each read
adds 256 new samples (8.192 ms); adjacent analysis windows overlap by half. Keep the driver, FFT,
and bass-filter sample rates consistent; SAMPLING_FREQ supplies all three here, including mic mode.
Returning to 48 kHz requires reconsidering SCKI too, not just changing fs.

6. "Justification" on the wire is different from alignment in memory

There are THREE different questions:
  a. Serial format: when does the sample start relative to a WS transition?
  b. Slot width: how many bit-times are reserved for each channel?
  c. Memory layout: where do the meaningful bits land in the CPU word?

(a) With FMT grounded, standard I2S delays the sample's most-significant
bit (MSB, also the sign bit) by ONE BCK period after the WS transition.
The transition announces the next channel before its MSB arrives. With
left-justified format, the MSB starts at the channel boundary without that
one-bit delay. "Left" here means aligned to the beginning of a slot; it does
not mean "read only the left audio channel" or "left speaker".

The ESP32 I2S hardware handles that serial delay. Our matching setting is
I2S_COMM_FORMAT_STAND_I2S because PCM1808_LEFT_JUSTIFIED is false.
Do not manually shift samples by one bit to compensate for standard I2S.
A mismatched receiver format can corrupt signs and apparent amplitude.

(b) We use 32-bit slots for BOTH channels: 64 BCK per stereo frame.
The converter still supplies 24 meaningful bits per sample, MSB first.

(c) In this expected capture layout, a signed 24-bit sample occupies bits
31..8 of an int32_t DMA word; the low 8 bits are padding, not more audio.
Thus a sample value of +1 is represented as 0x00000100 in the DMA word,
and -1 as 0xFFFFFF00. This upper-bit placement is compatible with standard
I2S; it does not imply that the wire protocol is left-justified.

For signed sample counts in [-8388608, 8388607], use:
  int32_t sample24 = sample32 >> 8;
The signed right shift is arithmetic on this Xtensa ESP32: it preserves the
sign of negative samples. Shifting uint32_t instead would lose that sign.
Removing eight padding bits and handling I2S's one-bit wire delay are
different operations. formatAudioSlot() uses the eight-bit shift and checks
padding so the acquired layout can be inspected rather than assumed blindly.

The common processing path converts each signed DMA word to float by dividing
by 2147483648.0f. Full-scale peak therefore corresponds to approximately +/-1.
Volume is RMS of the two channel powers after removing each channel's DC mean.
The single FFT uses the stronger channel over the complete window, avoiding
phase cancellation without doubling FFT work. The separate kick detectors see
each channel once; events from the same stereo hit are combined.
Neither DMA slot number alone proves physical L/R; diagnostics inspect both.

Your i2s_std_slot_config_t / I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG example is
the NEW driver's way of selecting standard I2S and slot parameters. This
file uses the LEGACY <driver/i2s.h> API instead: communication_format,
bits_per_sample, bits_per_chan, and channel_format specify those choices.
Do not paste new-driver structures into this legacy setup. Its ledcSetup /
ledcAttachPin calls also follow the Arduino-ESP32 2.x API.

7. What changing a setting actually changes

FMT wiring / communication_format: serial bit timing, not sensitivity.
MD0/MD1 / I2S_MODE_MASTER: who drives BCK/WS, not who sends audio samples.
use_apll: ESP32 I2S clock source, not ADC mode or LEDC routing.
SAMPLING_FREQ: actual requested acquisition rate and analysis time scale.
uiMasterSensitivity: visual response only, not speaker volume or ADC range.
uiAutoGain: display-only automatic gain control; see AudioAnalysis::AutoGain.
NEW processing (AGC + input correction) is the reference from 2026-11-20;
the serial command "ab old" still selects the previous processing for
debugging. Temporary: in an audio mode the Game button steps back one
audio routine (Mic/BT pressed again steps forward).

References (manufacturer documentation; figures/tables are worth reading):
TI PCM1808 Rev. B: sections 7.3.2-7.3.5, Tables 1-3, Figure 24, section 9
https://www.ti.com/lit/ds/symlink/pcm1808.pdf
Espressif original-ESP32 I2S: clock sources, MCLK pin limit, Philips/MSB format
https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html
Espressif legacy I2S: use_apll and i2s_config_t / i2s_pin_config_t
https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/peripherals/i2s.html
Espressif LEDC: hardware PWM, clocking and frequency/resolution relationship
https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/peripherals/ledc.html
Arduino-ESP32 migration: ledcSetup / ledcAttachPin removed in 3.0
https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html
*/

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "SignalAcquisition.h"
#include "AudioAnalysis.h"
#include "TempoTracker.h"
#include "HitDetector.h"
#include "CommandState.h"
#include "BoardConfig.h"
#include "BluetoothManager.h"
#include "SourceAdapters.h"
#include "ModeButton.h"
#include <esp_err.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>
#include <Preferences.h>
#define FASTLED_INTERNAL
#include <FastLED.h>

#define LED_PIN 12
#define NUM_LEDS 15
CRGB leds[NUM_LEDS];

// --- UART Bridge ---
#define UART_RX_PIN 14
#define UART_TX_PIN 27

// --- Button Pins ---
#define BTN_ANIM 18
#define BTN_I2S 19
#define BTN_ADC 32
#define BTN_GAME 25
#define BTN_CAM 33 // Swapped from 23 to avoid I2S collision

// --- I2S Mic/ADC Pins ---
#define I2S_MCLK 4 // Breakout SCK/SCKI: 8 MHz from LEDC, not native I2S MCLK

// Hardware contract: PCM1808 MD1=LOW, MD0=LOW (slave), FMT=LOW (I2S).
// Set this true ONLY if the PCM1808 FMT pin is actually HIGH.
constexpr bool PCM1808_LEFT_JUSTIFIED = false; // FMT is grounded = Standard I2S
constexpr bool PCM1808_USE_APLL = false; // Normal 160 MHz-derived I2S path; see primer section 4
// Live diagnostics are configured in BoardConfig.h and never bypass FFT/UART.
// ADC channels are normalized separately; visual volume combines their powers.
#define I2S_WS 23     // LRCK/LRC: 31,250 Hz, shared with INMP441
#define I2S_SCK 22    // BCK: 2 MHz, shared with INMP441; NOT breakout SCK/SCKI
#define I2S_SD_MIC 21 // Dedicated to INMP441
#define I2S_SD_ADC 5  // Dedicated to PCM1808 (D5)

// --- Audio & FFT Config ---
// A 512-sample analysis window covers 16.384 ms. Read only 256 NEW samples
// per acquisition call, then reuse the preceding half-window: 50% overlap.
#define SAMPLES AudioAnalysis::WindowSamples
#define AUDIO_HOP_SAMPLES AudioAnalysis::HopSamples
// 48 kHz is supported, but requires matching SCKI (e.g. 12.288 MHz at 256*fs).
// Only NATIVE MCLK routing is limited to GPIO0/1/3, not the sample rate itself.
#define SAMPLING_FREQ 31250 // LRCK=31,250 Hz; BCK=2 MHz; LEDC SCKI=8 MHz
#define I2S_PORT I2S_NUM_0

// COMMON VISUAL INPUT CONTRACT
// ADC/mic readings use the same normalized units: 1.0 is ADC full-scale peak.
// These source multipliers are the two normal sensitivity knobs. They cannot
// change speaker volume. Old spectrum-sum gains have different units and must
// NOT be copied numerically into these new controls.
float uiMasterSensitivity = 1.0f;
float uiInputGainADC = 1.0f;
float uiInputGainMic = 4.0f;

// At unity gain, this RMS maps to 255. These are starting calibration values,
// not measured properties of your source. Adjust source gain first.
float uiFullScaleRMS = 0.02f;
// Measured 2026-11-19: Bluetooth receiver idle hiss/hum reaches ~1.3e-4 RMS;
// the quietest musical passage at the usual volume is ~8e-4. Gate between.
float uiNoiseFloorRMSADC = 0.0002f;
float uiNoiseFloorRMSMic = 0.00010f;
float uiSpectrumGamma = 0.60f;
float uiVolumeAttackMs = 16.0f;
float uiVolumeReleaseMs = 50.0f;
float uiBandAttackMs = 16.0f;
float uiBandReleaseMs = 50.0f;
float uiAverageMs = 800.0f;
float binEQ[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
// NEW PROCESSING = AGC + input correction. The A/B switch compares it with
// the previous fixed, uncorrected processing.
//
// Automatic gain control for display levels (volume and bands) only. The
// reference is the loud-section RMS that, with the settings above, gives the
// look the effects were tuned for. false leaves AGC out of new processing.
bool uiAutoGain = true;
AudioAnalysis::AutoGain::Settings uiAutoGainSettings;
AudioAnalysis::AutoGain autoGain;
// Corner of the ADC module's measured input low-pass (see InputCorrection).
// Set 0 when a flat module is installed. Kicks and the silence gate always
// use the uncorrected input.
float uiInputCorrectionHz = 235.0f;
AudioAnalysis::InputCorrection correctionLeft;
AudioAnalysis::InputCorrection correctionRight;
// TEMPORARY A/B switch: in an audio mode the Game button flips between new
// processing (true) and the previous processing (false). Every source change
// restarts from uiNewProcessing. AGC keeps measuring while off, so switching
// back applies an up-to-date gain immediately. Move to the rotary encoder later.
bool uiNewProcessing = true;
bool newProcessingActive = true;

// CALIBRATION (expedient phase): tools/audio_replay/calibrate.py drives the
// board over USB serial and stores the results in NVS, so a hardware change
// needs a rerun, not a reflash. These compiled values are the fallback; once a
// hardware setup is final, write its calibrated values back here.
struct CalibrationValues {
    float gateRms;       // uiNoiseFloorRMSADC
    float correctionHz;  // uiInputCorrectionHz
    float agcReference;  // uiAutoGainSettings.referenceRms
    float kickMinRms;    // uiKickMinRMS
};
AudioAnalysis::CalibrationMeter calibrationMeter;
bool audioDebugEnabled = true;  // "debug off" pauses the 2 Hz summary during calibration.
float volumeSmFloat = 0.0f;
float sampleAvgFloat = 0.0f;
float binSm[16] = {};

// Health counters belong only to the acquisition task. They report achieved
// throughput and worst processing time, not merely requested sample settings.
struct AudioHealth {
    uint32_t hops = 0;
    uint32_t analyses = 0;
    uint32_t readErrors = 0;
    uint32_t maxDSPUs = 0;
    uint32_t startedMs = 0;
};
AudioHealth audioHealth;

// Bass-kick controls: independent of visual gain and display compression.
float uiKickSensitivity = 1.0f; // Higher = detects subtler bass attacks; 0 = disable
float uiKickMinRMS = 0.0003f;   // Normalized bass noise gate; calibrate in silence
constexpr float KICK_LOW_HZ = 35.0f;
constexpr float KICK_HIGH_HZ = 180.0f;
constexpr uint32_t KICK_MIN_INTERVAL_MS = 180;
constexpr uint32_t KICK_LED_HOLD_MS = 80;
constexpr uint32_t AUDIO_DEBUG_INTERVAL_MS = 50;

uint32_t attackCount = 0; // General attack events; independent of bass kicks.
uint32_t beatCount = 0; // Monotonic during this boot, including between prints
uint32_t lastBeatEventMs = 0;
uint32_t lastTempoKickMs = 0; // Dedicated detector's last kick, for the tempo input only.
uint32_t tempoKickCount = 0;
bool beatPending = false; // Cleared after publishing the acquired block

void recordKickEvent(uint32_t nowMs) {
    ++beatCount;
    lastBeatEventMs = nowMs;
    beatPending = true;
}

// RBJ/W3C lowpass and highpass biquads, Q=1/sqrt(2), transposed direct form II.
struct BassBiquad {
    float b0 = 0;
    float b1 = 0;
    float b2 = 0;
    float a1 = 0;
    float a2 = 0;
    float z1 = 0;
    float z2 = 0;
    void configure(float fs, float hz, bool highpass) {
        const float w = 6.283185307179586f * hz / fs;
        const float c = cosf(w);
        const float alpha = sinf(w) / (2.0f * 0.70710678118f);
        const float invA0 = 1.0f / (1.0f + alpha);
        const float numerator = highpass ? (1.0f + c) : (1.0f - c);
        b0 = 0.5f * numerator * invA0;
        b2 = b0;
        b1 = (highpass ? -numerator : numerator) * invA0;
        a1 = -2.0f * c * invA0;
        a2 = (1.0f - alpha) * invA0;
        z1 = 0.0f;
        z2 = 0.0f;
    }
    float process(float x) {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

class BassKickDetector {
  public:
    float bassRms = 0.0f;
    float onsetRatio = 0.0f; // Fast bass power / recent bass power; steady tone ~= 1
    float triggerRatio = 1.7f;

    void begin(float fs, float lowHz = KICK_LOW_HZ, float highHz = KICK_HIGH_HZ) {
        sampleRate = fs;
        highpass.configure(fs, lowHz, true);
        lowpass.configure(fs, highHz, false);
        lastRisePower = 0.0f;
        fastAlpha = 1.0f - expf(-1.0f / (fs * 0.020f));
        slowAlpha = 1.0f - expf(-1.0f / (fs * 0.350f));
        fastPower = 0.0f;
        slowPower = 0.0f;
        samplesSeen = 0;
        lastEventSample = 0;
        bassRms = 0.0f;
        onsetRatio = 0.0f;
        armed = false;
    }

    // Feed continuous normalized samples BEFORE per-block mean removal or FFT.
    void processSample(float sample) {
        const float y = lowpass.process(highpass.process(sample));
        const float power = y * y;
        fastPower += fastAlpha * (power - fastPower);
        slowPower += slowAlpha * (power - slowPower);
        ++samplesSeen;
    }

    // Inspect every audio block, not merely when a debug line is printed.
    bool finishBlock(float sensitivity, float minRms) {
        const float floorRms = max(0.0000001f, minRms);
        const float reference = max(slowPower, floorRms * floorRms);
        bassRms = sqrtf(max(0.0f, fastPower));
        onsetRatio = min(1000.0f, max(0.0f, fastPower / reference));
        triggerRatio = max(1.15f, 1.0f + 0.7f / max(0.05f, sensitivity));
        // Let filters and the recent-power reference settle after a source change.
        if (samplesSeen < (uint64_t)(sampleRate * 0.5f)) {
            return false;
        }
        if (sensitivity <= 0.0f) {
            armed = false;
            return false;
        }
        const float rearmRatio = min(1.20f, 1.0f + 0.4f * (triggerRatio - 1.0f));
        if (onsetRatio < rearmRatio || bassRms < floorRms) {
            armed = true;
        }
        if (!armed || bassRms < floorRms || onsetRatio < triggerRatio) {
            return false;
        }
        // Consume the onset even during the lockout; do not emit it late.
        armed = false;
        const uint64_t minSamples = (uint64_t)(sampleRate * KICK_MIN_INTERVAL_MS / 1000.0f);
        if (samplesSeen - lastEventSample < minSamples) {
            return false;
        }
        lastEventSample = samplesSeen;
        return true;
    }

    // Power rise since the previous call, relative to recent power (0 when
    // steady or falling). Feeds the NEW tempo's kick-phase arbiter; it does
    // not emit kicks and has no lockout.
    float takeRise(float minRms) {
        const float floorRms = max(0.0000001f, minRms);
        const float reference = max(slowPower, floorRms * floorRms);
        const float rise = max(0.0f, fastPower - lastRisePower) / reference;
        lastRisePower = fastPower;
        return samplesSeen < (uint64_t)(sampleRate * 0.5f) ? 0.0f : min(rise, 50.0f);
    }

  private:
    float lastRisePower = 0.0f;
    BassBiquad highpass, lowpass;
    float sampleRate = SAMPLING_FREQ;
    float fastAlpha = 0.0f;
    float slowAlpha = 0.0f;
    float fastPower = 0.0f;
    float slowPower = 0.0f;
    uint64_t samplesSeen = 0;
    uint64_t lastEventSample = 0;
    bool armed = false;
};

// The detector algorithm/settings are unchanged. Stereo runs one detector per
// channel and ORs their events once per hop; phase opposition cannot erase a kick.
BassKickDetector kickDetector;
BassKickDetector kickDetectorRight;
// NEW tempo only: a narrower kick band (the kick fundamental, below most bass
// notes) whose power rises tell beat from off-beat. Same filter class.
constexpr float KICK_PHASE_LOW_HZ = 40.0f;
constexpr float KICK_PHASE_HIGH_HZ = 100.0f;
// RMS gain of AudioAnalysis::InputCorrection over each display band's FFT
// slots, cached for the current corner. Cheap: recomputed only when the
// corner changes (calibration profile load or serial command).
const float *correctionBandGain(float cornerHz) {
    static float cachedHz = -1.0f;
    static float inverse[16];
    if (cornerHz != cachedHz) {
        const float pole = cornerHz > 0.0f ? expf(-6.283185307f * cornerHz / float(SAMPLING_FREQ)) : 0.0f;
        const float scale = 1.0f / (1.0f - pole);
        for (unsigned band = 0; band < 16; ++band) {
            float power = 0.0f;
            unsigned slots = 0;
            for (unsigned slot = AudioAnalysis::BandEdges[band]; slot < AudioAnalysis::BandEdges[band + 1]; ++slot) {
                const float w = 6.283185307f * slot / float(AudioAnalysis::WindowSamples);
                power += scale * scale * (1.0f - 2.0f * pole * cosf(w) + pole * pole);
                ++slots;
            }
            inverse[band] = sqrtf(max(1.0e-12f, power / max(1u, slots)));
        }
        cachedHz = cornerHz;
    }
    return inverse;
}
bool uiKickPhaseArbiter = true; // NEW tempo's beat/off-beat arbiter (replay A/B).
float debugKickRise = 0.0f;
float debugTempoBands[16] = {}; // Last hop's rhythm bands (host replay diagnostics). // Last hop's kick-band rise (host replay diagnostics).
BassKickDetector kickPhaseLeft;
BassKickDetector kickPhaseRight;
BassBiquad gateHighpassLeft;
BassBiquad gateHighpassRight;
AudioAnalysis::OverlapWindow audioWindow;   // Corrected when new processing is active.
AudioAnalysis::OverlapWindow rawWindow;     // Always uncorrected: logged InRMS and calibration fit.
// The silence gate listens ABOVE ~150 Hz (uncorrected). Measured 2026-11-19:
// in silence the receiver chain emits short ~60 Hz rumble bursts up to about
// -64 dBFS that would otherwise open the gate (and wake the tempo clock),
// while music always has content above 150 Hz, even in breakdowns.
constexpr float GATE_HIGHPASS_HZ = 150.0f;
AudioAnalysis::OverlapWindow gateWindow;
float gateLeft[AUDIO_HOP_SAMPLES];
float gateRight[AUDIO_HOP_SAMPLES];
AudioAnalysis::SpectrumWindow spectrumWindow;
float vReal[SAMPLES];
float vImag[SAMPLES];
float hopLeft[AUDIO_HOP_SAMPLES];
float hopRight[AUDIO_HOP_SAMPLES];
float correctedLeft[AUDIO_HOP_SAMPLES];
float correctedRight[AUDIO_HOP_SAMPLES];
ArduinoFFT<float> FFT(vReal, vImag, SAMPLES, SAMPLING_FREQ);

// --- System State ---
enum SystemMode { MODE_ANIMATION, MODE_I2S_MIC, MODE_ADC, MODE_VIDEOGAME, MODE_MOCAP };
SystemMode currentMode = MODE_ANIMATION;

bool i2sInstalled = false;
// Acquisition task only; fixed storage, no heap. Both run every hop so the
// Game A/B switch compares like with like: OLD is the original tracker,
// NEW adds the kick-phase arbiter. The published rhythm follows the switch.
TempoTracker tempoTracker;    // OLD
TempoTracker tempoTrackerNew; // NEW
HitDetector hitDetector;      // NEW published hits (see HitDetector.h)
HitDetector::Settings uiHitSettings; // Applied on every audio reset.
bool i2sReady = false;
unsigned long audioReadyAt = 0;
unsigned long lastAudioDiagnostic = 0;

bool configureI2SForMode(SystemMode mode) {
    i2sReady = false;
    // Never combine samples from different sources or across a source reset.
    audioWindow.reset();
    rawWindow.reset();
    gateWindow.reset();
    gateHighpassLeft.configure(SAMPLING_FREQ, GATE_HIGHPASS_HZ, true);
    gateHighpassRight.configure(SAMPLING_FREQ, GATE_HIGHPASS_HZ, true);
    correctionLeft.configure(uiInputCorrectionHz);
    correctionRight.configure(uiInputCorrectionHz);
    tempoTracker.reset();
    tempoTrackerNew.settings.kickPhaseArbiter = uiKickPhaseArbiter;
    tempoTrackerNew.reset();
    hitDetector.settings = uiHitSettings;
    hitDetector.reset();
    tempoKickCount = 0;
    // Do not carry smoothing/AGC values from another source or a previous mode.
    autoGain.reset(uiAutoGainSettings);
    newProcessingActive = uiNewProcessing;
    volumeSmFloat = 0.0f;
    sampleAvgFloat = 0.0f;
    for (int i = 0; i < 16; ++i) {
        binSm[i] = 0.0f;
    }
    kickDetector.begin(SAMPLING_FREQ);
    kickDetectorRight.begin(SAMPLING_FREQ);
    kickPhaseLeft.begin(SAMPLING_FREQ, KICK_PHASE_LOW_HZ, KICK_PHASE_HIGH_HZ);
    kickPhaseRight.begin(SAMPLING_FREQ, KICK_PHASE_LOW_HZ, KICK_PHASE_HIGH_HZ);
    audioHealth = {};
    audioHealth.startedMs = millis();
    beatPending = false;
    if (i2sInstalled) {
        esp_err_t err = i2s_driver_uninstall(I2S_PORT);
        if (err != ESP_OK) {
            Serial.printf("I2S uninstall failed: %s\n", esp_err_to_name(err));
            return false;
        }
        i2sInstalled = false;
    }
    // No I2S driver needed in animation/game/camera modes.
    if (mode != MODE_ADC && mode != MODE_I2S_MIC) {
        return true;
    }

    const bool adc = (mode == MODE_ADC);
    i2s_config_t config = {};
    // ESP32 drives BCK/WS but RECEIVES samples; grounded MD0/MD1 make ADC slave.
    config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    config.sample_rate = SAMPLING_FREQ;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    // Capture BOTH ADC channels; retain the existing INMP441 right-slot setting.
    // INMP441 L/R must be HIGH for ONLY_RIGHT (LOW requires ONLY_LEFT).
    config.channel_format = adc ? I2S_CHANNEL_FMT_RIGHT_LEFT : I2S_CHANNEL_FMT_ONLY_RIGHT;
    // This handles the one-BCK I2S delay in hardware; it is unrelated to >> 8.
    config.communication_format =
        (adc && PCM1808_LEFT_JUSTIFIED) ? I2S_COMM_FORMAT_STAND_MSB : I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 8;
    config.dma_buf_len = 64;
    config.use_apll = adc && PCM1808_USE_APLL; // Chooses I2S source, not LEDC's source
    config.tx_desc_auto_clear = false;
    config.fixed_mclk = 0;
    // Internal I2S target/dividers only: this does not drive GPIO4 or set LEDC.
    config.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    config.bits_per_chan = I2S_BITS_PER_CHAN_32BIT;

    i2s_pin_config_t pins = {};
    // Native MCLK is not routed; the separate LEDC setup owns GPIO4.
    pins.mck_io_num = I2S_PIN_NO_CHANGE; // GPIO4 SCKI is driven by LEDC, not I2S MCLK
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
                  (adc && PCM1808_LEFT_JUSTIFIED) ? "left-justified" : "I2S", SAMPLING_FREQ);
    if (adc) {
        Serial.printf("Clock targets: MCLK=%lu Hz on GPIO%d, BCK=%lu Hz, LRCK=%d Hz\n",
                      (unsigned long)SAMPLING_FREQ * 256, I2S_MCLK,
                      (unsigned long)SAMPLING_FREQ * 64, SAMPLING_FREQ);
        Serial.println("Targets/configuration are not a measurement at the ADC pins.");
    }
    Serial.printf("FFT: %u samples (16.384 ms), hop=%u samples (8.192 ms), 50%% overlap\n",
                  unsigned(SAMPLES), unsigned(AUDIO_HOP_SAMPLES));
    return true;
}

// Everything below runs on SignalAcquisition's core. No extra task or mutex,
// and no text is ever sent to Serial2. If USB TX is backed up, skip diagnostics.
static char audioDebugText[1024];
static size_t audioDebugLength = 0;
static size_t audioDebugSent = 0;
static uint32_t lastAudioSummary = 0;
static uint32_t lastSlotSummary = 0;

static const char *modeName(CubinaWire::Mode mode) {
    switch (mode) {
    case CubinaWire::Mode::Animation:
        return "Animation";
    case CubinaWire::Mode::AudioMic:
        return "Mic";
    case CubinaWire::Mode::AudioBluetooth:
        return "ADC";
    case CubinaWire::Mode::AudioSD:
        return "AudioSD";
    case CubinaWire::Mode::AudioDJ:
        return "AudioDJ";
    case CubinaWire::Mode::Game:
        return "Game";
    case CubinaWire::Mode::Depth:
        return "Depth";
    }
    return "Unknown";
}

static void serviceAudioDebug() {
#if COMMAND_AUDIO_DEBUG
    if (audioDebugSent >= audioDebugLength) {
        return;
    }
    const int space = Serial.availableForWrite();
    if (space <= 0) {
        return;
    }
    size_t chunk = min(size_t(space), audioDebugLength - audioDebugSent);
    chunk = min(chunk, size_t(128));
    audioDebugSent +=
        Serial.write(reinterpret_cast<uint8_t *>(audioDebugText + audioDebugSent), chunk);
#endif
}

static size_t formatAudioSlot(char *text, size_t capacity, const int32_t *words, size_t frames,
                              size_t stride, size_t slot) {
    int32_t lo = INT32_MAX;
    int32_t hi = INT32_MIN;
    double sum = 0.0;
    double squares = 0.0;
    size_t badPadding = 0;
    for (size_t i = 0; i < frames; ++i) {
        const int32_t word = words[i * stride + slot];
        const int32_t sample = word >> 8;
        lo = min(lo, sample);
        hi = max(hi, sample);
        sum += sample;
        squares += double(sample) * sample;
        if ((uint32_t(word) & 0xffu) != 0) {
            ++badPadding;
        }
    }
    const double mean = sum / frames;
    const double rms = sqrt(max(0.0, squares / frames - mean * mean));
    const int n =
        snprintf(text, capacity,
                 "slot%u signed24=[%ld %ld %ld %ld] min=%ld max=%ld AC_RMS=%.1f pad!=0:%u/%u\r\n",
                 unsigned(slot), long(words[slot] >> 8), long(words[stride + slot] >> 8),
                 long(words[2 * stride + slot] >> 8), long(words[3 * stride + slot] >> 8), long(lo),
                 long(hi), rms, unsigned(badPadding), unsigned(frames));
    return n > 0 && size_t(n) < capacity ? size_t(n) : 0;
}

static void queueAudioDebug(const int32_t *words, size_t frames, size_t channels,
                            const AudioPayload &audio, float inputLevel, float measuredRMS) {
#if COMMAND_AUDIO_DEBUG
    const uint32_t now = millis();
    if (!audioDebugEnabled || audioDebugSent < audioDebugLength ||
        uint32_t(now - lastAudioSummary) < 500) {
        return;
    }
    lastAudioSummary = now;
    audioDebugLength = 0;
    audioDebugSent = 0;
    const char *source = audio.mode == 1 ? "Mic" : "ADC";
    const float sourceGain = audio.mode == 1 ? uiInputGainMic : uiInputGainADC;
    int n =
        snprintf(audioDebugText, sizeof(audioDebugText),
                 "%s: InRMS=%.7f Gain=%.3f Sens=%.2f EQ=%d AGC=%+.1fdB VIn=%.1f VRaw=%u VSm=%u VAvg=%u Beats=%lu "
                 "BassRMS=%.5f Attacks=%lu BPM=%.1f Phase=%u Conf=%u Flags=%u Bins:[",
                 source, double(measuredRMS), double(sourceGain), double(uiMasterSensitivity),
                 int((audio.tempoFlags & CubinaWire::SPECTRUM_CORRECTED) != 0),
                 newProcessingActive && uiAutoGain ? double(autoGain.gainDb()) : 0.0,
                 double(inputLevel), unsigned(audio.volumeRaw), unsigned(audio.volumeSm),
                 unsigned(audio.sampleAvg), (unsigned long)beatCount, double(kickDetector.bassRms),
                 (unsigned long)audio.attackCount, double(audio.tempoBpm100) / 100.0,
                 unsigned(audio.tempoPhase), unsigned(audio.tempoConfidence), unsigned(audio.tempoFlags));
    if (n <= 0 || size_t(n) >= sizeof(audioDebugText)) {
        return;
    }
    audioDebugLength = size_t(n);
    for (size_t i = 0; i < 16; ++i) {
        n = snprintf(audioDebugText + audioDebugLength, sizeof(audioDebugText) - audioDebugLength,
                     i == 15 ? "%u]\r\n" : "%u ", unsigned(audio.fftResult[i]));
        if (n <= 0 || size_t(n) >= sizeof(audioDebugText) - audioDebugLength) {
            audioDebugLength = 0;
            return;
        }
        audioDebugLength += size_t(n);
    }
    const uint32_t healthMs = uint32_t(millis() - audioHealth.startedMs);
    const unsigned samplesPerSecond =
        healthMs ? unsigned(uint64_t(audioHealth.hops) * AUDIO_HOP_SAMPLES * 1000 / healthMs) : 0;
    n = snprintf(audioDebugText + audioDebugLength, sizeof(audioDebugText) - audioDebugLength,
                 "Health: samples/s=%u WorkMax=%luus readErrors=%lu\r\n", samplesPerSecond,
                 (unsigned long)audioHealth.maxDSPUs, (unsigned long)audioHealth.readErrors);
    if (n > 0 && size_t(n) < sizeof(audioDebugText) - audioDebugLength) {
        audioDebugLength += size_t(n);
    }

#if COMMAND_AUDIO_SLOT_DEBUG
    if (uint32_t(now - lastSlotSummary) >= 1000) {
        lastSlotSummary = now;
        for (size_t slot = 0; slot < channels; ++slot) {
            audioDebugLength += formatAudioSlot(audioDebugText + audioDebugLength,
                                                sizeof(audioDebugText) - audioDebugLength, words,
                                                frames, channels, slot);
        }
    }
#endif
#endif
}

// Finish any partly drained debug line so a calibration reply starts cleanly.
static void breakAudioDebugLine() {
    if (audioDebugSent > 0 && audioDebugSent < audioDebugLength) {
        Serial.println("");
    }
    audioDebugSent = 0;
    audioDebugLength = 0;
}

// One line per measurement, all levels in dBFS. "bands" are the analysis
// band RMS values; "bins" are the largest published display bytes.
static void reportCalibrationMeasurement() {
    breakAudioDebugLine();
    Serial.printf("CAL MEASURE path=%s hops=%u rms=%.2f rmsmax=%.2f gate=%.2f gatemax=%.2f vraw=%u bands=",
                  newProcessingActive ? "new" : "old", calibrationMeter.hops,
                  double(calibrationMeter.rmsDb()), double(calibrationMeter.rmsMaxDb()),
                  double(calibrationMeter.gateDb()), double(calibrationMeter.gateMaxDb()),
                  unsigned(calibrationMeter.volumeRawMax));
    for (size_t band = 0; band < AudioAnalysis::SpectrumBands; ++band) {
        Serial.printf(band ? ",%.2f" : "%.2f", double(calibrationMeter.bandDb(band)));
    }
    Serial.printf(" bins=");
    for (size_t band = 0; band < AudioAnalysis::SpectrumBands; ++band) {
        Serial.printf(band ? ",%u" : "%u", unsigned(calibrationMeter.binMax[band]));
    }
    Serial.printf("\r\n");
}

static void acquireAudio(bool sd) {

    if (!sd && !i2sReady) {
        delay(1); // Configuration failure was printed; buttons still work.
        return;
    }
    const size_t channels = (currentMode == MODE_ADC) ? 2 : 1;
    int32_t words[AUDIO_HOP_SAMPLES * 2] = {};
    size_t bytesIn = 0;
    const size_t requested = AUDIO_HOP_SAMPLES * channels * sizeof(int32_t);
    esp_err_t result = ESP_OK;

    if (sd) {
        bytesIn = SourceAdapters::readSD(words, AUDIO_HOP_SAMPLES) ? requested : 0;
    } else {
        result = i2s_read(I2S_PORT, words, requested, &bytesIn, pdMS_TO_TICKS(10));
    }
    if (result != ESP_OK || bytesIn != requested) {
        // A gap invalidates continuous filter history; re-warm the detector.
        // Retain any event already waiting for transmission.
        audioWindow.reset();
        rawWindow.reset();
        gateWindow.reset();
        gateHighpassLeft.configure(SAMPLING_FREQ, GATE_HIGHPASS_HZ, true);
        gateHighpassRight.configure(SAMPLING_FREQ, GATE_HIGHPASS_HZ, true);
        correctionLeft.configure(uiInputCorrectionHz);
        correctionRight.configure(uiInputCorrectionHz);
        tempoTracker.reset();
        tempoTrackerNew.reset();
        hitDetector.reset();
        kickDetector.begin(SAMPLING_FREQ);
        kickDetectorRight.begin(SAMPLING_FREQ);
        kickPhaseLeft.begin(SAMPLING_FREQ, KICK_PHASE_LOW_HZ, KICK_PHASE_HIGH_HZ);
        kickPhaseRight.begin(SAMPLING_FREQ, KICK_PHASE_LOW_HZ, KICK_PHASE_HIGH_HZ);
        ++audioHealth.readErrors;
        volumeSmFloat = 0.0f;
        sampleAvgFloat = 0.0f;
        for (float &level : binSm) {
            level = 0.0f;
        }
        if (millis() - lastAudioDiagnostic >= 1000) {
            Serial.printf("I2S read: %s, bytes=%u/%u\n", esp_err_to_name(result), (unsigned)bytesIn,
                          (unsigned)requested);
            lastAudioDiagnostic = millis();
        }
        return; // Never process unfilled samples as valid audio.
    }
    if ((int32_t)(millis() - audioReadyAt) < 0) {
        // Drain reset/fade-in data without mixing it into the first window.
        audioWindow.reset();
        rawWindow.reset();
        gateWindow.reset();
        return;
    }

    const uint32_t dspStart = micros();
    const bool adcMode = (currentMode == MODE_ADC);
    if (audioHealth.hops == 0) {
        // Begin throughput accounting after startup settling, not before it.
        audioHealth.startedMs = millis() - 8;
    }
    ++audioHealth.hops;
    for (size_t i = 0; i < AUDIO_HOP_SAMPLES; ++i) {
        // The driver supplies signed, upper-aligned 32-bit PCM containers.
        // Converting each channel independently avoids integer overflow/mixing.
        hopLeft[i] = float(words[i * channels]) / 2147483648.0f;
        hopRight[i] = channels == 2 ? float(words[i * channels + 1]) / 2147483648.0f : hopLeft[i];
        kickDetector.processSample(hopLeft[i]);
        kickPhaseLeft.processSample(hopLeft[i]);
        if (channels == 2) {
            kickDetectorRight.processSample(hopRight[i]);
            kickPhaseRight.processSample(hopRight[i]);
        }
        // Run the correction continuously so its one-sample history is valid
        // the moment the A/B switch turns it on.
        correctedLeft[i] = correctionLeft.process(hopLeft[i]);
        correctedRight[i] = channels == 2 ? correctionRight.process(hopRight[i]) : correctedLeft[i];
        gateLeft[i] = gateHighpassLeft.process(hopLeft[i]);
        gateRight[i] = channels == 2 ? gateHighpassRight.process(hopRight[i]) : gateLeft[i];
    }
    const bool kickLeft = kickDetector.finishBlock(uiKickSensitivity, uiKickMinRMS);
    const bool kickRight =
        channels == 2 && kickDetectorRight.finishBlock(uiKickSensitivity, uiKickMinRMS);
    const bool tempoKick = (kickLeft || kickRight) &&
        (tempoKickCount == 0 || uint32_t(millis() - lastTempoKickMs) >= KICK_MIN_INTERVAL_MS);
    if (tempoKick) {
        ++tempoKickCount;
        lastTempoKickMs = millis();
    }
    // OLD publishes the dedicated detector's kicks; NEW publishes the
    // unified detector's low-group hits instead (below). The tempo always
    // uses the dedicated kicks.
    if (tempoKick && !newProcessingActive) {
        // Two channel detections of one hit remain ONE transported event.
        recordKickEvent(millis());
    }
    const bool correcting = newProcessingActive && uiInputCorrectionHz > 0.0f;
    rawWindow.appendHop(hopLeft, hopRight, channels == 2);
    gateWindow.appendHop(gateLeft, gateRight, channels == 2);
    if (!audioWindow.appendHop(correcting ? correctedLeft : hopLeft,
                               correcting ? correctedRight : hopRight, channels == 2)) {
        return;
    }

    // RMS uses BOTH channel powers. The spectrum uses one intact dominant
    // channel per complete window, so we still perform only one float FFT.
    // The FFT always runs on the UNCORRECTED window; NEW applies the treble
    // correction to the display bands as a per-band gain (below). Rhythm,
    // attacks and calibration then see the same spectrum in both A/B
    // positions. (Running the FFT on the corrected window changed which
    // channel was dominant and jittered bands +/-20-30% per frame, which cut
    // NEW BPM accuracy on the replay: Lay Low 99% -> 82%, Levels 88% -> 76%.)
    // Gate and activity floor use the uncorrected input: the correction lifts
    // high-frequency hiss by up to ~30 dB, which must not open the gate.
    const float inputRMS = rawWindow.copyForFFT(vReal, vImag);
    // Volume and AGC keep the corrected (flat-response) loudness in NEW.
    const float measuredRMS = correcting ? audioWindow.rms() : inputRMS;
    const float gateLevelRMS = gateWindow.rms();
    spectrumWindow.apply(vReal);
    FFT.compute(FFTDirection::Forward);
    FFT.complexToMagnitude();
    ++audioHealth.analyses;

    // Same processing for every source; only input calibration/noise differ.
    const float inputGain = adcMode ? uiInputGainADC : uiInputGainMic;
    const float gateRMS = adcMode ? uiNoiseFloorRMSADC : uiNoiseFloorRMSMic;
    const float sensitivity = max(0.0f, uiMasterSensitivity) * max(0.0f, inputGain);
    const float scale = 255.0f * sensitivity / max(0.000001f, uiFullScaleRMS);
    const bool audible =
        (!isnan(measuredRMS) && !isinf(measuredRMS)) && gateLevelRMS > max(0.0f, gateRMS);
    // AGC scales DISPLAY levels only. Detectors below keep the fixed scale.
    // Always track, so the A/B switch can turn AGC on without a settling delay.
    const float trackedGain =
        autoGain.update(measuredRMS, audible, AudioAnalysis::HopSeconds, uiAutoGainSettings);
    const float autoGainFactor = newProcessingActive && uiAutoGain ? trackedGain : 1.0f;
    const float displayScale = scale * autoGainFactor;
    const float volumeInputFloat = audible ? measuredRMS * displayScale : 0.0f;
    const float volumeRawFloat = AudioAnalysis::boundedLevel(volumeInputFloat);

    AudioPayload payload = {}; // Complete summary for our shared binary contract.
    payload.mode = sd ? 3 : (adcMode ? 2 : 1);
    float tempoBands[16] = {};
    const float *correction = correcting ? correctionBandGain(uiInputCorrectionHz) : nullptr;
    for (unsigned band = 0; band < 16; ++band) {
        tempoBands[band] = spectrumWindow.bandRMS(vReal, band);
        const float bandRms = correction ? tempoBands[band] * correction[band] : tempoBands[band];
        float level = audible ? bandRms * displayScale * max(0.0f, binEQ[band]) : 0.0f;
        level = AudioAnalysis::boundedLevel(level);
        if (level > 0.0f) {
            level = 255.0f * powf(level / 255.0f, max(0.1f, uiSpectrumGamma));
        }
        // Bound BEFORE smoothing on every source: no invisible >255 state.
        binSm[band] = AudioAnalysis::smoothLevel(binSm[band], level, AudioAnalysis::HopSeconds,
                                                 uiBandAttackMs, uiBandReleaseMs);
        payload.fftResult[band] = uint8_t(binSm[band] < 1.0f ? 0 : lroundf(binSm[band]));
    }
    // Rhythm uses pre-EQ/pre-gamma bands. A modest calibrated activity floor
    // keeps a tiny electrical baseline from counting as music during silence.
    // The activity floor uses the FIXED calibration, as before AGC existed.
    const bool rhythmAudible = audible && inputRMS * scale >= 5.0f;
    const float kickRise = channels == 2 ?
        0.5f * (kickPhaseLeft.takeRise(uiKickMinRMS) + kickPhaseRight.takeRise(uiKickMinRMS)) :
        kickPhaseLeft.takeRise(uiKickMinRMS);
    debugKickRise = kickRise;
    for (unsigned band = 0; band < 16; ++band) debugTempoBands[band] = tempoBands[band];
    const TempoTracker::Result rhythmOld = tempoTracker.process(
        tempoBands, rhythmAudible, AudioAnalysis::HopSeconds, tempoKick);
    const TempoTracker::Result rhythmNew = tempoTrackerNew.process(
        tempoBands, rhythmAudible, AudioAnalysis::HopSeconds, tempoKick, kickRise);
    const TempoTracker::Result &rhythm = newProcessingActive ? rhythmNew : rhythmOld;
    // Run continuously in both positions so switching A/B needs no settling.
    const HitDetector::Result hit = hitDetector.process(tempoBands, rhythmAudible, AudioAnalysis::HopSeconds);
    if (newProcessingActive) {
        // NEW: the unified hit detector is the only source of published hits.
        if (hit.event) {
            ++attackCount;
            payload.attackStrength = uint8_t(lroundf(hit.strength * 255.0f));
            payload.attackBands = hit.bands();
            if (hit.groups & 1u) {
                recordKickEvent(millis());
            }
        }
    } else if (rhythm.attack.event) {
        ++attackCount;
        payload.attackStrength = uint8_t(lroundf(rhythm.attack.strength * 255.0f));
        payload.attackBands = rhythm.attack.bands;
    }
    payload.attackCount = attackCount;
    payload.tempoBpm100 = uint16_t(lroundf(rhythm.bpm * 100.0f));
    payload.tempoPhase = uint16_t(rhythm.phase * 65536.0f);
    payload.tempoConfidence = uint8_t(lroundf(rhythm.confidence * 255.0f));
    payload.tempoFlags = (correcting ? CubinaWire::SPECTRUM_CORRECTED : 0) |
                         (rhythm.active ? CubinaWire::AUDIO_ACTIVE : 0) |
                         (rhythm.locked ? CubinaWire::TEMPO_LOCKED : 0);

    volumeSmFloat =
        AudioAnalysis::smoothLevel(volumeSmFloat, volumeRawFloat, AudioAnalysis::HopSeconds,
                                   uiVolumeAttackMs, uiVolumeReleaseMs);
    sampleAvgFloat = AudioAnalysis::smoothLevel(
        sampleAvgFloat, volumeRawFloat, AudioAnalysis::HopSeconds, uiAverageMs, uiAverageMs);
    payload.volumeRaw = uint8_t(lroundf(volumeRawFloat));
    payload.volumeSm = uint8_t(lroundf(volumeSmFloat));
    payload.sampleAvg = uint8_t(lroundf(sampleAvgFloat));
    payload.beat = beatPending ? 1 : 0;
    const bool beatPulseActive =
        beatCount > 0 && uint32_t(millis() - lastBeatEventMs) < KICK_LED_HOLD_MS;
    const float masterSensitivity = max(0.0f, uiMasterSensitivity);

    FastLED.clear();

    // LEDs 1-14 show smoothed volume; LED 15 is reserved for kicks.
    constexpr int volumeLedCount = NUM_LEDS - 1;
    int litLeds = map(payload.volumeSm, 0, 255, 0, volumeLedCount);
    if (litLeds > volumeLedCount) {
        litLeds = volumeLedCount;
    }

    // Fixed color zones: 1-4 blue, 5-8 green, 9-11 yellow, 12-14 red.
    // A/B test: the whole bar is purple while AGC is switched off.
    for (int i = 0; i < litLeds; i++) {
        if (!newProcessingActive) {
            leds[i] = CRGB::Purple;
        } else if (i < 4) {
            leds[i] = CRGB::Blue;
        } else if (i < 8) {
            leds[i] = CRGB::Green;
        } else if (i < 11) {
            leds[i] = CRGB::Yellow;
        } else {
            leds[i] = CRGB::Red;
        }
    }

    // LED 15 stays off between short white kick flashes.
    if (beatPulseActive && masterSensitivity > 0.0f) {
        leds[NUM_LEDS - 1] = CRGB::White;
    }
    FastLED.show();
    // ==========================================

    // Acquisition publishes frequently; UARTSender independently samples
    // the latest complete result at a nominal 60 Hz. No UART work under this mutex.
    CommandState::publishAudio(payload, beatCount);
    beatPending = false;
    if (calibrationMeter.add(inputRMS, gateLevelRMS, tempoBands, payload.volumeRaw, payload.fftResult,
                             AudioAnalysis::HopSeconds)) {
        reportCalibrationMeasurement();
    }
    queueAudioDebug(words, AUDIO_HOP_SAMPLES, channels, payload, volumeInputFloat, inputRMS);
    // Includes FFT, tempo analysis, display, publication, and diagnostics, but not
    // the blocking I2S read or the task's outside polling/yield operations.
    audioHealth.maxDSPUs = max(audioHealth.maxDSPUs, uint32_t(micros() - dspStart));
}

namespace {
    using CommandButtons::ModeButton;
    using CubinaWire::Mode;
    ModeButton buttons[] = {
        ModeButton(COMMAND_BTN_ANIMATION), ModeButton(COMMAND_BTN_MIC),
        ModeButton(COMMAND_BTN_ADC),      ModeButton(COMMAND_BTN_GAME),
        ModeButton(COMMAND_BTN_DEPTH),
    };
    Mode selected = Mode::Animation;
    uint32_t lastDepthRead = 0;
    extern char activeProfile[];
    void activateSourceProfile();
    void demand(Mode m) {
        // Only the two physical audio inputs are exposed by this build.
        // AudioBluetooth is the existing wire value 2 for the PCM1808 path;
        // it does not imply an ESP32 Bluetooth audio connection or a relay.
        // Keep old SD/DJ requests on that SAME wired ADC input as well.
        if (m == Mode::AudioSD || m == Mode::AudioDJ) {
            m = Mode::AudioBluetooth;
        }

        // Avoid interleaving a new mode message into a partially drained debug line.
        if (audioDebugSent > 0 && audioDebugSent < audioDebugLength) {
            Serial.println("");
        }
        audioDebugSent = 0;
        audioDebugLength = 0;

        if (m == selected && CubinaWire::audioMode(m) && i2sReady) {
            // Next visual: keep the I2S driver, clocks, DMA, filter history,
            // and live measurements intact. Only a real source change needs
            // the teardown/reinstall below. A failed setup still gets retried.
            CommandState::repeatModeDemand();
            Serial.printf("Demand next audio routine: %s; I2S kept running\r\n", modeName(m));
            return;
        }

        selected = m;
        CommandState::selectMode(m);
        // Close/reset the old audio source before switching anything external.
        configureI2SForMode(MODE_ANIMATION);
        currentMode = MODE_ANIMATION;
        bool ready = SourceAdapters::select(m);
        activeProfile[0] = '\0';
        if (m == Mode::AudioMic || m == Mode::AudioBluetooth) {
            currentMode = m == Mode::AudioMic ? MODE_I2S_MIC : MODE_ADC;
            activateSourceProfile();  // This hardware configuration's calibration.
            ready = ready && configureI2SForMode(currentMode);
        }
        CommandState::setReady(ready && !CubinaWire::audioMode(m) && m != Mode::Depth);
        Serial.printf("Demand mode: %s (%u)\r\n", modeName(m), unsigned(m));
    }
    // ---- Calibration profiles and commands over USB serial ----
    //
    // A PROFILE describes one hardware configuration: an audio source feeding
    // a digitizer (e.g. "bt-pcm1808": Bluetooth receiver into this PCM1808
    // module; "mic-inmp441"; later "dj-pcm1808"). Each physical input (ADC,
    // Mic) is assigned one profile; selecting the input loads it. Profiles are
    // measured by tools/audio_replay/calibrate.py and stored in NVS under
    // "cal-<name>". Correct per-profile data lets ONE visual tuning serve
    // every configuration. Expedient phase: once a configuration is final,
    // copy its values into compiledDefaults() below.
    //
    // One command per line; every reply starts with "CAL ":
    //   ping | mode adc|mic|anim | ab new|old | debug on|off | get
    //   set gate|corr_hz|agc_ref|kick_min VALUE | measure SECONDS
    //   profile | profile use NAME | save | load | defaults | forget
    constexpr const char *AssignmentNamespace = "cubina-audio";
    constexpr size_t ProfileNameBytes = 12;  // NVS namespaces are <= 15 chars with "cal-".
    char activeProfile[ProfileNameBytes] = "";

    bool micSource() {
        return currentMode == MODE_I2S_MIC;
    }
    // Compiled fallback values per physical input, used when a profile has
    // never been saved. ADC: the 2026-11-19 PCM1808 module (low-pass measured
    // at 201 Hz by calibrate.py). Mic: INMP441, assumed flat.
    CalibrationValues compiledDefaults(bool mic) {
        return mic ? CalibrationValues{0.00010f, 0.0f, 0.0055f, 0.0003f}
                   : CalibrationValues{0.0002f, 235.0f, 0.0055f, 0.0003f};
    }
    const char *defaultProfileName(bool mic) {
        return mic ? "mic-inmp441" : "bt-pcm1808";
    }
    CalibrationValues currentCalibration() {
        return {micSource() ? uiNoiseFloorRMSMic : uiNoiseFloorRMSADC, uiInputCorrectionHz,
                uiAutoGainSettings.referenceRms, uiKickMinRMS};
    }
    void applyCalibration(const CalibrationValues &v) {
        (micSource() ? uiNoiseFloorRMSMic : uiNoiseFloorRMSADC) = v.gateRms;
        uiInputCorrectionHz = v.correctionHz;
        uiAutoGainSettings.referenceRms = v.agcReference;
        uiKickMinRMS = v.kickMinRms;
        correctionLeft.configure(uiInputCorrectionHz);
        correctionRight.configure(uiInputCorrectionHz);
    }
    void printCalibration(const char *what) {
        breakAudioDebugLine();
        const CalibrationValues v = currentCalibration();
        Serial.printf("CAL %s profile=%s source=%s gate=%.7f corr_hz=%.1f agc_ref=%.6f kick_min=%.7f\r\n",
                      what, activeProfile[0] ? activeProfile : "-", micSource() ? "mic" : "adc",
                      double(v.gateRms), double(v.correctionHz), double(v.agcReference),
                      double(v.kickMinRms));
    }
    void profileNamespace(const char *name, char (&out)[16]) {
        snprintf(out, sizeof(out), "cal-%s", name);
    }
    // Load NAME's stored values, or the compiled values for this input.
    bool loadProfile(const char *name) {
        char space[16];
        profileNamespace(name, space);
        CalibrationValues v = compiledDefaults(micSource());
        Preferences prefs;
        const bool stored = prefs.begin(space, true);
        if (stored) {
            v.gateRms = prefs.getFloat("gate", v.gateRms);
            v.correctionHz = prefs.getFloat("corr_hz", v.correctionHz);
            v.agcReference = prefs.getFloat("agc_ref", v.agcReference);
            v.kickMinRms = prefs.getFloat("kick_min", v.kickMinRms);
            prefs.end();
        }
        snprintf(activeProfile, sizeof(activeProfile), "%s", name);
        applyCalibration(v);
        return stored;
    }
    // Called by demand() before an audio input is configured.
    void activateSourceProfile() {
        const char *key = micSource() ? "mic" : "adc";
        char name[ProfileNameBytes] = "";
        Preferences prefs;
        if (prefs.begin(AssignmentNamespace, true)) {
            prefs.getString(key, name, sizeof(name));
            prefs.end();
        }
        if (!name[0]) {
            snprintf(name, sizeof(name), "%s", defaultProfileName(micSource()));
        }
        printCalibration(loadProfile(name) ? "LOADED" : "COMPILED");
    }
    void assignProfile(const char *name) {
        Preferences prefs;
        if (prefs.begin(AssignmentNamespace, false)) {
            prefs.putString(micSource() ? "mic" : "adc", name);
            prefs.end();
        }
    }
    void saveCalibration() {
        char space[16];
        profileNamespace(activeProfile, space);
        Preferences prefs;
        if (!activeProfile[0] || !prefs.begin(space, false)) {
            breakAudioDebugLine();
            Serial.printf("CAL ERR save failed (select an audio input first)\r\n");
            return;
        }
        const CalibrationValues v = currentCalibration();
        prefs.putFloat("gate", v.gateRms);
        prefs.putFloat("corr_hz", v.correctionHz);
        prefs.putFloat("agc_ref", v.agcReference);
        prefs.putFloat("kick_min", v.kickMinRms);
        prefs.end();
        assignProfile(activeProfile);
        printCalibration("SAVED");
    }
    void reply(const char *text, const char *arg = "") {
        breakAudioDebugLine();
        Serial.printf("CAL %s %s\r\n", text, arg);
    }
    bool validProfileName(const char *name) {
        const size_t length = strlen(name);
        if (length == 0 || length >= ProfileNameBytes) {
            return false;
        }
        for (size_t i = 0; i < length; ++i) {
            const char c = name[i];
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
                return false;
            }
        }
        return true;
    }
    void handleCalibrationCommand(const char *line) {
        char command[16] = {};
        char argument[24] = {};
        char third[24] = {};
        const int fields = sscanf(line, "%15s %23s %23s", command, argument, third);
        if (fields < 1) {
            return;
        }
        const float value = fields >= 3 ? float(atof(third)) : 0.0f;
        const auto is = [](const char *a, const char *b) { return strcmp(a, b) == 0; };
        const bool audioInput = CubinaWire::audioMode(selected) && activeProfile[0];
        if (is(command, "ping")) {
            reply("OK", "ping");
        } else if (is(command, "mode")) {
            if (is(argument, "adc")) {
                demand(Mode::AudioBluetooth);
            } else if (is(argument, "mic")) {
                demand(Mode::AudioMic);
            } else if (is(argument, "anim")) {
                demand(Mode::Animation);
            } else {
                reply("ERR mode", argument);
                return;
            }
            reply("OK mode", argument);
        } else if (is(command, "ab") && (is(argument, "new") || is(argument, "old"))) {
            newProcessingActive = is(argument, "new");
            reply("OK ab", argument);
        } else if (is(command, "debug") && (is(argument, "on") || is(argument, "off"))) {
            audioDebugEnabled = is(argument, "on");
            reply("OK debug", argument);
        } else if (is(command, "get") || (is(command, "profile") && fields == 1)) {
            printCalibration("VALUES");
        } else if (!audioInput && (is(command, "set") || is(command, "profile") || is(command, "save") ||
                                   is(command, "load") || is(command, "defaults") || is(command, "forget"))) {
            reply("ERR", "select an audio input first (mode adc|mic)");
        } else if (is(command, "profile") && is(argument, "use") && fields == 3) {
            if (!validProfileName(third)) {
                reply("ERR profile name (a-z, 0-9, '-', at most 11)", third);
                return;
            }
            assignProfile(third);
            printCalibration(loadProfile(third) ? "LOADED" : "COMPILED");
        } else if (is(command, "set") && fields == 3 && isfinite(value)) {
            CalibrationValues v = currentCalibration();
            if (is(argument, "gate")) {
                v.gateRms = constrain(value, 1.0e-6f, 0.01f);
            } else if (is(argument, "corr_hz")) {
                v.correctionHz = constrain(value, 0.0f, 15000.0f);
            } else if (is(argument, "agc_ref")) {
                v.agcReference = constrain(value, 1.0e-5f, 0.5f);
            } else if (is(argument, "kick_min")) {
                v.kickMinRms = constrain(value, 1.0e-6f, 0.05f);
            } else {
                reply("ERR set", argument);
                return;
            }
            applyCalibration(v);
            printCalibration("VALUES");
        } else if (is(command, "measure")) {
            const float seconds = float(atof(argument));
            if (!CubinaWire::audioMode(selected) || !i2sReady || !(seconds > 0.0f)) {
                reply("ERR measure", "needs an audio mode and seconds > 0");
                return;
            }
            calibrationMeter.start(constrain(seconds, 0.05f, 60.0f));
            reply("OK measure", argument);
        } else if (is(command, "save")) {
            saveCalibration();
        } else if (is(command, "load")) {
            printCalibration(loadProfile(activeProfile) ? "LOADED" : "COMPILED");
        } else if (is(command, "defaults")) {
            applyCalibration(compiledDefaults(micSource()));
            printCalibration("VALUES");
        } else if (is(command, "forget")) {
            char space[16];
            profileNamespace(activeProfile, space);
            Preferences prefs;
            if (prefs.begin(space, false)) {
                prefs.clear();
                prefs.end();
            }
            reply("OK forget", activeProfile);
        } else {
            reply("ERR unknown", line);
        }
    }
    void pollSerialCommands() {
        static char line[64];
        static size_t length = 0;
        while (Serial.available() > 0) {
            const int c = Serial.read();
            if (c < 0) {
                break;
            }
            if (c == '\n' || c == '\r') {
                if (length > 0) {
                    line[length] = '\0';
                    length = 0;
                    handleCalibrationCommand(line);
                }
            } else if (length < sizeof(line) - 1) {
                line[length++] = char(c);
            }
        }
    }
    void pollButtons() {
        for (unsigned i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
            // Every button is a plain debounced press. Holding ADC must not
            // enter an unimplemented SD mode or change the physical input.
            const CommandButtons::Event event = buttons[i].poll();
            if (event != CommandButtons::Event::None) {
                switch (i) {
                case 0:
                    demand(Mode::Animation);
                    break;
                case 1:
                    demand(Mode::AudioMic);
                    break;
                case 2:
                    demand(Mode::AudioBluetooth);
                    break;
                case 3:
                    // TEMPORARY: in an audio mode, Game steps back one routine so
                    // adjacent cards can be compared. Elsewhere it selects Game mode.
                    if (CubinaWire::audioMode(selected)) {
                        if (!i2sReady) break;  // Source not up yet: ignore.
                        if (audioDebugSent > 0 && audioDebugSent < audioDebugLength) {
                            Serial.println("");
                        }
                        audioDebugSent = 0;
                        audioDebugLength = 0;
                        CommandState::repeatModeDemand(true);
                        Serial.printf("Demand previous audio routine: %s\r\n", modeName(selected));
                    } else {
                        demand(Mode::Game);
                    }
                    break;
                case 4:
                    // Temporary debugging: Depth is now a Hold toggle.
                    // Keep acquisition and the current audio source running.
                    CommandState::togglePlaybackHold();
                    break;
                }
            }
        }
    }
    void task(void *) {
        // Arduino Bluepad32 synchronizes with its own Core-0 Bluetooth stack;
        // keep BP32.update and all ControllerPtr access in this Core-1 task.
        for (auto &b : buttons) {
            if (b.pin >= 0) {
                pinMode(b.pin, INPUT_PULLUP);
            }
        }
        const double mclk = ledcSetup(0, 8000000, 2);
        ledcAttachPin(I2S_MCLK, 0);
        ledcWrite(0, 2);
        Serial.printf("LEDC configured SCKI=%.0f Hz\n", mclk);
        FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
        FastLED.setBrightness(100);
        FastLED.clear();
        FastLED.show();
        demand(Mode::Animation);
        for (;;) {
            serviceAudioDebug();
            pollSerialCommands();
            BluetoothManager::update();
            pollButtons();
            if (CubinaWire::audioMode(selected)) {
                acquireAudio(false); // Mic or the sole physical PCM1808 input.
            } else if (selected == Mode::Game) {
                CommandState::publishGamepads(BluetoothManager::getState(ROLE_MASTER),
                                              BluetoothManager::getState(ROLE_PLAYER_2));
            } else if (selected == Mode::Depth && uint32_t(millis() - lastDepthRead) >= 33) {
                static uint8_t bits[CubinaWire::DEPTH_BYTES];
                lastDepthRead = millis();
                if (SourceAdapters::readDepth(bits)) {
                    CommandState::publishDepth(bits);
                }
            }
            vTaskDelay(1); // yielding, not an unbounded busy spin
        }
    }
} // namespace
void SignalAcquisition::begin() {
    // Register BP32 callbacks once in Arduino setup; update runs in the task.
    BluetoothManager::setup();
    BaseType_t ok = xTaskCreatePinnedToCore(task, "SignalAcquisition", 16384, nullptr, 1, nullptr,
                                            COMMAND_ACQUISITION_CORE);
    configASSERT(ok == pdPASS);
}
