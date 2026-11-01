#pragma once

#include <math.h>
#include <stddef.h>

// Acquisition-task-owned rhythm estimate. This is an experimental tempo and
// phase tracker, not a claim to recognize the musical beat of every recording.
// It reuses linear FFT band RMS; it does not run another FFT or change gain.
class TempoTracker {
  public:
    struct Result {
        float bpm = 0.0f;
        float phase = 0.0f; // 0 = predicted beat; approaches 1 before next.
        float confidence = 0.0f;
        bool active = false;
        bool locked = false;
    };

    void reset() {
        *this = TempoTracker();
    }

    Result process(const float *bands, bool audible, float seconds) {
        audioTime += seconds;
        if (audioTime > 60.0f) {
            // Retain sub-millisecond float resolution during long installations.
            audioTime -= 60.0f;
            lastOnset -= 60.0f;
        }
        phase = wrap(phase + seconds * bpm / 60.0f);
        quietSeconds = audible ? 0.0f : quietSeconds + seconds;

        // Positive changes in log band RMS form a gain-tolerant novelty curve.
        // Extra bass weight favors a musical pulse, but all bands contribute.
        // No display EQ, gamma, or smoothing is involved in this measurement.
        float flux = 0.0f;
        float weights = 0.0f;
        for (unsigned band = 0; band < 16; ++band) {
            const float level = log1pf(fmaxf(0.0f, bands[band]) * 100000.0f);
            const float weight = band < 4 ? 3.0f : 1.0f;
            if (havePrevious && audible) {
                flux += fmaxf(0.0f, level - previous[band]) * weight;
            }
            weights += weight;
            previous[band] = level;
        }
        havePrevious = true;
        flux /= weights;

        const float novelty = fmaxf(0.0f, flux - fluxMean * 1.1f);
        const bool onset = audible && onsetArmed && flux > fmaxf(0.035f, fluxMean * 1.6f) &&
                           audioTime - lastOnset > 0.09f;
        if (flux < fmaxf(0.02f, fluxMean * 0.8f)) {
            onsetArmed = true;
        }
        if (onset) {
            onsetArmed = false;
            lastOnset = audioTime;
            if (confidence >= LockConfidence) {
                // Only attacks near a predicted beat pull the clock. Half-beat
                // hi-hats should not immediately move it onto the off-beat.
                const float error = phase > 0.5f ? phase - 1.0f : phase;
                if (fabsf(error) <= 0.22f) {
                    phase = wrap(phase - error * 0.35f);
                }
            }
        }
        fluxMean += (1.0f - expf(-seconds)) * (flux - fluxMean);

        // Two 256-sample hops make one novelty sample (61.035 Hz at 31,250).
        // The fixed ring holds about 8.4 seconds. Correlation is spread over
        // hops: only two lag candidates are tested per novelty publication.
        // There is no periodic large all-lags workload on one acquisition hop.
        accumulatedNovelty += novelty;
        if (++hopPair == 2) {
            hopPair = 0;
            noveltyEnvelope += 0.55f * (accumulatedNovelty - noveltyEnvelope);
            history[writeIndex] = noveltyEnvelope;
            accumulatedNovelty = 0.0f;
            writeIndex = (writeIndex + 1) % Capacity;
            if (count < Capacity) {
                ++count;
            }

            if (count >= MinimumHistory && quietSeconds < 0.5f) {
                for (unsigned work = 0; work < 2; ++work) {
                    scores[scanLag] = correlate(scanLag);
                    if (++scanLag > MaximumLag + 1) {
                        chooseTempo();
                        scanLag = MinimumLag - 1;
                    }
                }
            }
        }

        if (!audible) {
            confidence *= expf(-seconds / 0.6f);
        }
        if (quietSeconds > 2.0f && !quietReset) {
            // Old music must not immediately re-lock a new recording after a
            // long silence. Preserve only the free-running last tempo/phase.
            count = 0;
            scanLag = MinimumLag - 1;
            for (float &score : scores) {
                score = 0.0f;
            }
            confidence = 0.0f;
            quietReset = true;
        }
        if (audible) {
            quietReset = false;
        }

        Result result;
        result.bpm = haveTempo ? bpm : 0.0f;
        result.phase = phase;
        result.confidence = confidence;
        result.active = quietSeconds < 0.35f;
        result.locked = result.active && confidence >= LockConfidence;
        return result;
    }

  private:
    static constexpr size_t Capacity = 512;
    static constexpr size_t MinimumHistory = 184; // About three seconds.
    static constexpr unsigned MinimumLag = 18;
    static constexpr unsigned MaximumLag = 62;
    static constexpr float NoveltyRate = 31250.0f / 512.0f;
    static constexpr float LockConfidence = 0.45f;

    float history[Capacity] = {};
    float scores[MaximumLag + 2] = {};
    float previous[16] = {};
    size_t writeIndex = 0;
    size_t count = 0;
    unsigned scanLag = MinimumLag - 1;
    unsigned hopPair = 0;
    float accumulatedNovelty = 0.0f;
    float noveltyEnvelope = 0.0f;
    float audioTime = 0.0f;
    float quietSeconds = 1.0f;
    float lastOnset = -1.0f;
    float fluxMean = 0.0f;
    float bpm = 120.0f;
    float phase = 0.0f;
    float confidence = 0.0f;
    float pendingBpm = 0.0f;
    unsigned pendingEstimates = 0;
    bool havePrevious = false;
    bool haveTempo = false;
    bool onsetArmed = true;
    bool quietReset = false;

    static float wrap(float value) {
        return value - floorf(value);
    }

    float atAge(size_t age) const {
        return history[(writeIndex + Capacity - 1 - age) % Capacity];
    }

    float correlate(unsigned lag) const {
        float sx = 0.0f;
        float sy = 0.0f;
        float xx = 0.0f;
        float yy = 0.0f;
        float xy = 0.0f;
        const size_t pairs = count - lag;
        for (size_t i = 0; i < pairs; ++i) {
            const float x = atAge(i);
            const float y = atAge(i + lag);
            sx += x;
            sy += y;
            xx += x * x;
            yy += y * y;
            xy += x * y;
        }
        const float n = float(pairs);
        const float numerator = xy - sx * sy / n;
        const float denominator = sqrtf(fmaxf(0.0f, (xx - sx * sx / n) * (yy - sy * sy / n)));
        return denominator > 0.000001f ? fmaxf(0.0f, numerator / denominator) : 0.0f;
    }

    void chooseTempo() {
        unsigned best = MinimumLag;
        float merit = -1.0f;
        for (unsigned lag = MinimumLag; lag <= MaximumLag; ++lag) {
            const float candidate = 60.0f * NoveltyRate / lag;
            // Small preference for the shorter period breaks near-equal
            // fundamental/subharmonic ties. Half/double-time ambiguity remains
            // possible on real music; this is not hidden as a certain answer.
            float value = scores[lag] * (0.97f + 0.03f * candidate / 200.0f);
            // A doubled/tripled period can correlate more strongly simply
            // because it happens to land nearer an integer sample lag. If a
            // shorter divisor already has substantial support, prefer that
            // pulse instead of mistaking every second/third beat for the beat.
            for (unsigned divisor = 2; divisor <= 3; ++divisor) {
                const unsigned shorter = lag / divisor;
                if (shorter >= MinimumLag) {
                    const float support = fmaxf(scores[shorter], scores[shorter + 1]);
                    if (support > 0.25f && support > scores[lag] * 0.45f) {
                        value *= fmaxf(0.3f, 1.0f - 0.7f * support / fmaxf(0.001f, scores[lag]));
                    }
                }
            }
            if (haveTempo && fabsf(candidate - bpm) < bpm * 0.08f) {
                value *= 1.025f;
            }
            if (value > merit) {
                merit = value;
                best = lag;
            }
        }

        const float curvature = scores[best - 1] - 2.0f * scores[best] + scores[best + 1];
        const float offset =
            curvature < -0.000001f
                ? fmaxf(-0.5f,
                        fminf(0.5f, 0.5f * (scores[best - 1] - scores[best + 1]) / curvature))
                : 0.0f;
        const float candidate = fmaxf(60.0f, fminf(200.0f, 60.0f * NoveltyRate / (best + offset)));
        const float certainty = fmaxf(0.0f, fminf(1.0f, (scores[best] - 0.20f) / 0.65f));
        const bool acquiring = confidence < LockConfidence;
        confidence += 0.35f * (certainty - confidence);
        if (certainty < LockConfidence) {
            return;
        }

        if (!haveTempo || acquiring) {
            bpm = candidate;
            phase = wrap((audioTime - lastOnset) * bpm / 60.0f);
            haveTempo = true;
            pendingEstimates = 0;
        } else if (fabsf(candidate - bpm) < bpm * 0.15f) {
            bpm += 0.2f * (candidate - bpm);
            pendingEstimates = 0;
        } else {
            // Require repeated agreement before jumping to a new tempo family.
            pendingEstimates =
                fabsf(candidate - pendingBpm) < candidate * 0.06f ? pendingEstimates + 1 : 1;
            pendingBpm = candidate;
            if (pendingEstimates >= 3) {
                bpm = candidate;
                phase = wrap((audioTime - lastOnset) * bpm / 60.0f);
                pendingEstimates = 0;
            }
        }
    }
};
