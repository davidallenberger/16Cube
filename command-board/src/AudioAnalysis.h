#pragma once

#include <stddef.h>
#include <math.h>

// Sample rate, FFT window length, and hop length are independent of UART and
// rendering. One slot in this FFT is SampleRate / WindowSamples = 61.035 Hz.
namespace AudioAnalysis {
    constexpr size_t SampleRate = 31250;
    constexpr size_t WindowSamples = 512;
    constexpr size_t HopSamples = 256;
    constexpr size_t SpectrumBands = 16;
    constexpr float HopSeconds = float (HopSamples) / float (SampleRate);

    // Half-open FFT slot ranges. For example, band 0 uses slot [1,2), i.e.
    // slot 1 only, centered near 61 Hz. Band 1 uses slot 2, near 122 Hz.
    // These are measurement centers, not sharp acoustic filter boundaries.
    // Slot 0 is DC (the average voltage), and is deliberately excluded.
    constexpr unsigned BandEdges[17] = {1,  2,  3,  4,  5,  7,   9,   12, 16,
                                        22, 30, 42, 58, 82, 116, 164, 256};

    inline float smoothLevel (float previous, float target, float seconds, float attackMs,
                              float releaseMs) {
        const float timeMs = target > previous ? attackMs : releaseMs;
        if (timeMs <= 0.0f) {
            return target;
        }
        const float weight = 1.0f - expf (-seconds * 1000.0f / timeMs);
        return previous + weight * (target - previous);
    }

    inline float boundedLevel (float value) {
        return fminf (255.0f, fmaxf (0.0f, value));
    }

    class OverlapWindow {
      public:
        void reset () {
            storedSamples = 0;
        }

        // Both histories retain ORIGINAL normalized waveform values. FFT work
        // arrays are separate because the transform overwrites its inputs.
        bool appendHop (const float *left, const float *right, bool twoChannels) {
            stereo = twoChannels;
            if (storedSamples == WindowSamples) {
                for (size_t i = 0; i < HopSamples; ++i) {
                    historyLeft[i] = historyLeft[i + HopSamples];
                    historyRight[i] = historyRight[i + HopSamples];
                }
                storedSamples = HopSamples;
            }
            for (size_t i = 0; i < HopSamples; ++i) {
                historyLeft[storedSamples + i] = left[i];
                historyRight[storedSamples + i] = twoChannels ? right[i] : left[i];
            }
            storedSamples += HopSamples;
            return storedSamples == WindowSamples;
        }

        // Measure each channel after removing its own DC offset. Stereo RMS
        // combines POWERS, so opposite-polarity channels cannot cancel volume.
        // Select one intact channel for the single FFT. This costs one FFT,
        // but can omit content exclusive to the quieter channel; it is a
        // documented CPU tradeoff, not a complete stereo-spectrum measurement.
        float copyForFFT (float *real, float *imaginary) {
            float meanLeft = 0.0f;
            float meanRight = 0.0f;
            for (size_t i = 0; i < WindowSamples; ++i) {
                meanLeft += historyLeft[i];
                meanRight += historyRight[i];
            }
            meanLeft /= WindowSamples;
            meanRight /= WindowSamples;
            float powerLeft = 0.0f;
            float powerRight = 0.0f;
            for (size_t i = 0; i < WindowSamples; ++i) {
                const float l = historyLeft[i] - meanLeft;
                const float r = historyRight[i] - meanRight;
                powerLeft += l * l;
                powerRight += r * r;
            }
            spectrumRight = stereo && powerRight > powerLeft;
            for (size_t i = 0; i < WindowSamples; ++i) {
                real[i] = spectrumRight ? historyRight[i] - meanRight : historyLeft[i] - meanLeft;
                imaginary[i] = 0.0f;
            }
            return sqrtf ((powerLeft + powerRight) / (2.0f * WindowSamples));
        }

        bool selectedRight () const {
            return spectrumRight;
        }

        // The same stereo RMS as copyForFFT, without touching FFT storage.
        // Used to gate on the UNCORRECTED input (see InputCorrection).
        float rms () const {
            float meanLeft = 0.0f;
            float meanRight = 0.0f;
            for (size_t i = 0; i < WindowSamples; ++i) {
                meanLeft += historyLeft[i];
                meanRight += historyRight[i];
            }
            meanLeft /= WindowSamples;
            meanRight /= WindowSamples;
            float powerLeft = 0.0f;
            float powerRight = 0.0f;
            for (size_t i = 0; i < WindowSamples; ++i) {
                const float l = historyLeft[i] - meanLeft;
                const float r = historyRight[i] - meanRight;
                powerLeft += l * l;
                powerRight += r * r;
            }
            return sqrtf ((powerLeft + powerRight) / (2.0f * WindowSamples));
        }

      private:
        float historyLeft[WindowSamples] = {};
        float historyRight[WindowSamples] = {};
        size_t storedSamples = 0;
        bool stereo = false;
        bool spectrumRight = false;
    };

    // Undo a measured first-order low-pass in front of the ADC.
    //
    // The 2026-11-19 PCM1808 module rolls off at about 235 Hz, -6 dB/octave
    // (tools/audio_replay/tone_steps.py; identical on L/R, independent of the
    // source). Modeling it as y[n] = (1-p)x[n] + p*y[n-1], with the matched
    // pole p = exp(-2*pi*fc/fs), the exact inverse is
    //     x[n] = (y[n] - p*y[n-1]) / (1-p).
    // Gain is 0 dB at DC, +6 dB/octave above fc, and at most (1+p)/(1-p)
    // (~32.5 dB) at Nyquist, about 2-3 dB short of the analog curve near
    // 13 kHz. It also lifts high-frequency noise, so the silence gate keeps
    // measuring the uncorrected input. cornerHz <= 0 is an exact pass-through:
    // set it to 0 when a flat ADC module is installed.
    class InputCorrection {
      public:
        void configure(float cornerHz) {
            pole = cornerHz > 0.0f ? expf(-6.283185307f * cornerHz / float(SampleRate)) : 0.0f;
            scale = 1.0f / (1.0f - pole);
            previous = 0.0f;
        }

        float process(float sample) {
            const float restored = (sample - pole * previous) * scale;
            previous = sample;
            return restored;
        }

      private:
        float pole = 0.0f;
        float scale = 1.0f;
        float previous = 0.0f;
    };

    // Slow automatic gain control for DISPLAY levels only.
    //
    // It follows the loud end of the recent music (a ~0.4 s loudness that a
    // ceiling tracks: up within about a second, down slowly at a fixed dB
    // rate) and returns the factor that moves that ceiling to referenceRms.
    // At the calibrated listening level the factor is about 1, so visuals
    // look as they did with fixed gain. Turning the source up is undone in
    // a few seconds; turning it down, over tens of seconds. That asymmetry
    // is the price of keeping a song's quiet-verse/loud-drop contrast: the
    // ceiling cannot tell a lowered volume knob from a breakdown, so it
    // falls slowly. Measured with tools/audio_replay (Levels, Sandstorm):
    // 0.3 dB/s keeps about 2/3 of the fixed-gain section contrast and
    // recovers from a 10 dB cut in 20-35 s; 0.15 keeps more but takes ~70 s.
    // Silence (gate closed), near-silence relative to the ceiling, and any
    // level below minAdaptRms freeze it, so it never turns background hiss up. Kick, attack and tempo detection never use it.
    class AutoGain {
      public:
        struct Settings {
            // Calibrated 2026-11-19 from a board capture of Levels at the
            // usual listening volume: AGC is about 0 dB in its loud sections.
            float referenceRms = 0.0055f;   // Loud-section RMS that should look "normal".
            float maxBoostDb = 18.0f;       // Most a quiet source is lifted.
            float maxCutDb = 30.0f;         // Most a loud source is reduced.
            float levelMs = 400.0f;         // Loudness integration, like momentary loudness.
            float riseMs = 300.0f;          // Ceiling rise time constant when music gets louder.
            float fallDbPerSecond = 0.3f;   // Ceiling fall rate: lower keeps more song contrast,
                                            // higher recovers sooner after volume is turned down.
            float followRangeDb = 40.0f;    // Below ceiling - this, treat as near-silence: hold.
            // Absolute floor: below this 0.4 s loudness it never adapts, so
            // background hiss/hum between songs can't walk the gain up to
            // maxBoostDb. Matches the tempo tracker's activity floor (~-68 dBFS).
            float minAdaptRms = 0.0004f;
        };

        void reset(const Settings &settings) {
            ceilingDb = toDb(settings.referenceRms);
            levelPower = 0.0f;
            audibleSeconds = 0.0f;
            factor = 1.0f;
        }

        float update(float rms, bool audible, float seconds, const Settings &settings) {
            if (!audible || !(rms >= 0.0f) || isinf(rms)) {
                return factor; // Freeze: keep the last gain through gaps and silence.
            }
            const float weight = 1.0f - expf(-seconds * 1000.0f / fmaxf(1.0f, settings.levelMs));
            levelPower += weight * (rms * rms - levelPower);
            audibleSeconds += seconds;
            if (audibleSeconds >= settings.levelMs / 1000.0f &&
                levelPower >= settings.minAdaptRms * settings.minAdaptRms) {
                const float levelDb = 10.0f * log10f(fmaxf(levelPower, 1.0e-20f));
                if (levelDb > ceilingDb) {
                    ceilingDb += (levelDb - ceilingDb) *
                                 (1.0f - expf(-seconds * 1000.0f / fmaxf(1.0f, settings.riseMs)));
                } else if (levelDb > ceilingDb - settings.followRangeDb) {
                    ceilingDb -= settings.fallDbPerSecond * seconds;
                }
                const float referenceDb = toDb(settings.referenceRms);
                ceilingDb = fminf(referenceDb + settings.maxCutDb,
                                  fmaxf(referenceDb - settings.maxBoostDb, ceilingDb));
                factor = powf(10.0f, (referenceDb - ceilingDb) / 20.0f);
            }
            return factor;
        }

        float gainDb() const {
            return 20.0f * log10f(factor);
        }

      private:
        static float toDb(float rms) {
            return 20.0f * log10f(fmaxf(rms, 1.0e-10f));
        }

        float ceilingDb = -45.2f;
        float levelPower = 0.0f;
        float audibleSeconds = 0.0f;
        float factor = 1.0f;
    };

    // Averages what the acquisition path measures over a requested time, for
    // the Mac-driven calibration loop (tools/audio_replay/calibrate.py).
    // inputRms is the UNCORRECTED window RMS; gateRms is what the silence gate
    // compares (uncorrected, above ~150 Hz); bandRms are the linear band RMS
    // values the analysis used (corrected while new processing is active);
    // volumeRaw/bins are the published display bytes. No allocation.
    class CalibrationMeter {
      public:
        void start(float seconds) {
            *this = CalibrationMeter();
            remaining = seconds;
            running = true;
        }

        bool busy() const {
            return running;
        }

        // Returns true exactly once, on the hop that completes the measurement.
        bool add(float inputRms, float gateRms, const float *bandRms, uint8_t volumeRaw,
                 const uint8_t *bins, float seconds) {
            if (!running) {
                return false;
            }
            ++hops;
            rmsPower += inputRms * inputRms;
            rmsMax = fmaxf(rmsMax, inputRms);
            gatePower += gateRms * gateRms;
            gateMax = fmaxf(gateMax, gateRms);
            for (size_t band = 0; band < SpectrumBands; ++band) {
                bandPower[band] += bandRms[band] * bandRms[band];
                if (bins[band] > binMax[band]) {
                    binMax[band] = bins[band];
                }
            }
            if (volumeRaw > volumeRawMax) {
                volumeRawMax = volumeRaw;
            }
            remaining -= seconds;
            if (remaining > 0.0f) {
                return false;
            }
            running = false;
            return true;
        }

        static float toDb(float power) {
            return 10.0f * log10f(fmaxf(power, 1.0e-20f));
        }
        float rmsDb() const {
            return toDb(hops ? rmsPower / hops : 0.0f);
        }
        float rmsMaxDb() const {
            return toDb(rmsMax * rmsMax);
        }
        float gateDb() const {
            return toDb(hops ? gatePower / hops : 0.0f);
        }
        float gateMaxDb() const {
            return toDb(gateMax * gateMax);
        }
        float bandDb(size_t band) const {
            return toDb(hops ? bandPower[band] / hops : 0.0f);
        }

        unsigned hops = 0;
        uint8_t volumeRawMax = 0;
        uint8_t binMax[SpectrumBands] = {};

      private:
        float remaining = 0.0f;
        bool running = false;
        float rmsPower = 0.0f;
        float rmsMax = 0.0f;
        float gatePower = 0.0f;
        float gateMax = 0.0f;
        float bandPower[SpectrumBands] = {};
    };

    // Cache the window once, instead of repeatedly evaluating 512 cosines.
    // Parseval normalization converts a band's sum of squared FFT magnitudes
    // into an approximate waveform RMS. It is not a perceived-loudness model.
    class SpectrumWindow {
      public:
        void apply (float *real) {
            if (!initialized) {
                windowPower = 0.0f;
                for (size_t i = 0; i < WindowSamples; ++i) {
                    weights[i] = 0.54f - 0.46f * cosf (6.28318530718f * float (i) /
                                                       float (WindowSamples - 1));
                    windowPower += weights[i] * weights[i];
                }
                initialized = true;
            }
            for (size_t i = 0; i < WindowSamples; ++i) {
                real[i] *= weights[i];
            }
        }

        float bandRMS (const float *magnitudes, unsigned band) const {
            float power = 0.0f;
            for (unsigned slot = BandEdges[band]; slot < BandEdges[band + 1]; ++slot) {
                power += magnitudes[slot] * magnitudes[slot];
            }
            // Positive and negative FFT halves have equal power for real input.
            return sqrtf (2.0f * power / (float (WindowSamples) * windowPower));
        }

      private:
        float weights[WindowSamples] = {};
        float windowPower = 1.0f;
        bool initialized = false;
    };
} // namespace AudioAnalysis
