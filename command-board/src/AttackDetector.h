#pragma once

#include <math.h>
#include <stdint.h>

// A shared measurement of sudden increases in linear spectral power.
// It does not classify instruments and does not apply an animation threshold.
// Call once per NEW analysis window, before display EQ/gamma/smoothing.
class AttackDetector {
  public:
    struct Result {
        bool event = false;
        float strength = 0.0f; // 0..1, relative to recent signal power.
        float novelty = 0.0f;  // Power rise normalized by recent signal power.
        uint16_t bands = 0;    // Bands contributing a substantial increase.
    };

    void reset() {
        *this = AttackDetector();
    }

    Result process(const float *bands, bool audible, float seconds) {
        Result result;
        float total = 0.0f;
        float rising = 0.0f;
        float changes[16];
        for (unsigned band = 0; band < 16; ++band) {
            const float value = fmaxf(0.0f, bands[band]);
            // Modest bass weighting helps the musical clock without excluding
            // treble or midrange attacks. Absolute power avoids giving tiny
            // leakage bins the same vote as a strong musical component.
            const float power = value * value * (band < 4 ? 3.0f : 1.0f);
            changes[band] = fmaxf(0.0f, power - previous[band]);
            total += power;
            rising += changes[band];
            previous[band] = power;
        }
        elapsed += seconds;
        sinceEvent += seconds;
        recentPower += (1.0f - expf(-seconds / 0.5f)) * (total - recentPower);
        const float fraction = rising / fmaxf(total, 1.0e-12f);
        const float threshold = fmaxf(0.12f, fluxMean * 3.0f);
        fluxMean += (1.0f - expf(-seconds)) * (fraction - fluxMean);
        if (fraction < 0.06f || !audible) {
            armed = true;
        }
        if (!audible || elapsed < 0.5f || fraction < threshold) {
            return result;
        }
        result.novelty = rising / fmaxf(recentPower, 1.0e-12f);
        result.strength = fminf(1.0f, rising / fmaxf(recentPower * 4.0f, 1.0e-12f));
        for (unsigned band = 0; band < 16; ++band) {
            if (changes[band] >= rising * 0.10f) {
                result.bands |= uint16_t(1u << band);
            }
        }
        if (armed && sinceEvent >= 0.08f) {
            result.event = true;
            sinceEvent = 0.0f;
            armed = false;
        }
        return result;
    }

  private:
    float previous[16] = {};
    float recentPower = 0.0f;
    float fluxMean = 0.0f;
    float elapsed = 0.0f;
    float sinceEvent = 1.0f;
    bool armed = true;
};
