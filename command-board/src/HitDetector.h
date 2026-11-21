#pragma once

#include <math.h>
#include <stdint.h>

// NEW unified hit detector (2026-11-20). One decision for every routine:
// the renderer no longer re-thresholds; routines only choose which groups
// they use and how big they draw a hit.
//
// Three band groups match the renderer's GROUP_MASKS: low = bands 0-3,
// mid = 4-10, high = 11-15. For each group:
//   1. Onset strength = mean positive change of log band power. Logs make
//      it independent of level, AGC and the fixed treble roll-off, so the
//      same music gives the same hits from any calibrated source.
//   2. A hit candidate is a local peak of that strength above the group's
//      recent mean + `sigma` x recent mean deviation (about 1.5 s memory).
//   3. "Strong for this song": each group tracks the `keepQuantile` of its
//      candidates' excess (log-domain running quantile, ~10 s to adapt).
//      Only candidates above it publish, so every song keeps roughly its own
//      top share of peaks, whatever its density or loudness.
//      Published strength = excess / (2 x that quantile), capped at 1
//      (0.5 = just admitted, 1 = twice as strong).
//   4. A refractory period per group (Puddle Peak relies on one).
// Input: linear band RMS (uncorrected is fine; see 1). Call once per hop.
// Namespace-scope constants: the ESP32 toolchain predates C++17 inline
// variables, so static constexpr member arrays would need out-of-line copies.
namespace HitGroups {
    constexpr unsigned Count = 3;
    constexpr unsigned First[Count] = {0, 4, 11};
    constexpr unsigned Last[Count] = {3, 10, 15};
    constexpr uint16_t Masks[Count] = {0x000F, 0x07F0, 0xF800};
}

class HitDetector {
  public:
    static constexpr unsigned Groups = HitGroups::Count;

    struct Settings {
        float sigma = 2.0f;            // Candidate: above mean + sigma x deviation.
        float keepQuantile = 0.8f;     // Publish the strongest (1 - this) of candidates.
        float quantileRate = 0.05f;    // Log-domain step per candidate.
        // When > 0, a fixed excess (natural-log units of mean band-power
        // rise; 0.23 = 1 dB) replaces the adaptive quantile.
        float fixedExcess = 1.4f;
        float refractorySeconds = 0.10f;
        float statsSeconds = 1.5f;     // Onset-strength mean/deviation memory.
    };

    struct Result {
        bool event = false;
        uint8_t groups = 0;       // Bit g set when group g hit (1 low, 2 mid, 4 high).
        float strength = 0.0f;    // Strongest published group, 0..1 relative to the song.
        float groupStrength[Groups] = {};
        uint16_t bands() const {
            uint16_t mask = 0;
            for (unsigned g = 0; g < Groups; ++g) {
                if (groups & (1u << g)) mask |= HitGroups::Masks[g];
            }
            return mask;
        }
    };

    Settings settings;

    void reset() {
        const Settings keep = settings;
        *this = HitDetector();
        settings = keep;
    }

    Result process(const float *bands, bool audible, float seconds) {
        Result result;
        elapsed += seconds;
        const float statsKeep = expf(-seconds / settings.statsSeconds);
        const float powerKeep = expf(-seconds / 1.0f);
        for (unsigned g = 0; g < Groups; ++g) {
            Group &group = groups[g];
            float flux = 0.0f;
            for (unsigned band = HitGroups::First[g]; band <= HitGroups::Last[g]; ++band) {
                const float power = bands[band] * bands[band];
                meanPower[band] = meanPower[band] * powerKeep + power * (1.0f - powerKeep);
                // A floor at 5% of the band's recent power keeps near-silent
                // bands from producing huge log ratios out of noise.
                const float floor = 0.05f * meanPower[band] + 1.0e-14f;
                const float level = logf(power + floor);
                flux += fmaxf(0.0f, level - previousLevel[band]);
                previousLevel[band] = level;
            }
            flux /= float(HitGroups::Last[g] - HitGroups::First[g] + 1);
            group.sinceHit += seconds;

            // Peak picking one hop late: the previous value is a peak when it
            // beat both neighbours. Statistics exclude nothing; hits are rare.
            const float candidate = group.previous;
            const bool peak = candidate > group.beforePrevious && candidate >= flux;
            const float threshold = group.mean + settings.sigma * group.deviation;
            if (audible && elapsed > 1.0f && peak && candidate > threshold) {
                const float excess = candidate - group.mean;
                if (group.logQuantile == 0.0f) {
                    group.logQuantile = logf(fmaxf(1.0e-9f, excess));
                }
                const float quantile = settings.fixedExcess > 0.0f ? settings.fixedExcess
                                                                   : expf(group.logQuantile);
                const bool strong = excess > quantile;
                group.logQuantile += settings.quantileRate * (strong ? settings.keepQuantile
                                                                      : settings.keepQuantile - 1.0f);
                const float strength = fminf(1.0f, 0.5f * excess / fmaxf(1.0e-9f, quantile));
                result.groupStrength[g] = strength;
                if (strong && group.sinceHit >= settings.refractorySeconds) {
                    group.sinceHit = 0.0f;
                    result.groups |= uint8_t(1u << g);
                    result.strength = fmaxf(result.strength, strength);
                }
            }
            if (audible) {
                group.mean = group.mean * statsKeep + flux * (1.0f - statsKeep);
                group.deviation = group.deviation * statsKeep + fabsf(flux - group.mean) * (1.0f - statsKeep);
            }
            group.beforePrevious = group.previous;
            group.previous = audible ? flux : 0.0f;
        }
        result.event = result.groups != 0;
        return result;
    }

  private:
    struct Group {
        float previous = 0.0f;
        float beforePrevious = 0.0f;
        float mean = 0.0f;
        float deviation = 0.0f;
        float logQuantile = 0.0f;
        float sinceHit = 1.0f;
    };
    Group groups[Groups];
    float previousLevel[16] = {};
    float meanPower[16] = {};
    float elapsed = 0.0f;
};
