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

      private:
        float historyLeft[WindowSamples] = {};
        float historyRight[WindowSamples] = {};
        size_t storedSamples = 0;
        bool stereo = false;
        bool spectrumRight = false;
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
