#pragma once

#include <math.h>
#include <stddef.h>
#include "AttackDetector.h"

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
        AttackDetector::Result attack;
    };

    void reset() {
        *this = TempoTracker();
    }

    Result process(const float *bands, bool audible, float seconds, bool bassKick = false) {
        phase = wrap(phase + seconds * bpm / 60.0f);
        updateKickClock(bassKick, seconds);
        quietSeconds = audible ? 0.0f : quietSeconds + seconds;

        // One shared attack measurement supplies both the public attack event
        // and the tempo novelty history. It uses linear power, not display bins.
        const auto attack = attacks.process(bands, audible, seconds);
        const float novelty = attack.novelty;

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

        if (quietSeconds > 0.8f) {
            confidence *= expf(-seconds / 0.4f);
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
        result.active = quietSeconds < 1.2f;
        if (!result.active || confidence < 0.30f) {
            lockHeld = false;
        } else if (confidence >= 0.45f) {
            lockHeld = true;
        }
        result.locked = result.active && lockHeld;
        result.attack = attack;
        return result;
    }

  private:
    static constexpr size_t Capacity = 512;
    static constexpr size_t MinimumHistory = 184; // About three seconds.
    static constexpr unsigned MinimumLag = 18;
    static constexpr unsigned MaximumLag = 62;
    static constexpr float NoveltyRate = 31250.0f / 512.0f;
    static constexpr float LockConfidence = 0.45f;

    // Eight recent inter-kick intervals are independent of spectral loudness.
    // Only a repeated direct interval can nominate a tempo; multiples tolerate
    // missing kicks, but cannot manufacture double time from sparse kicks.
    float kickIntervals[8] = {};
    unsigned kickCount = 0;
    unsigned kickWrite = 0;
    float kickAge = 4.0f;
    float kickPeriod = 0.0f;
    bool kickSupported = false;

    void updateKickClock(bool event, float seconds) {
        kickAge += seconds;
        if (kickAge > 2.1f) {
            kickSupported = false;
            kickCount = 0;
            kickWrite = 0;
        }
        if (!event) {
            return;
        }
        const float interval = kickAge;
        kickAge = 0.0f;
        if (interval < 0.18f || interval > 2.1f) {
            return;
        }
        kickIntervals[kickWrite] = interval;
        kickWrite = (kickWrite + 1) % 8;
        if (kickCount < 8) {
            ++kickCount;
        }
        float bestPeriod = 0.0f;
        unsigned bestDirect = 0;
        unsigned bestSupported = 0;
        for (unsigned i = 0; i < kickCount; ++i) {
            const float candidate = kickIntervals[i];
            if (candidate < 0.295f || candidate > 1.01f) {
                continue;
            }
            unsigned direct = 0;
            unsigned supported = 0;
            float sum = 0.0f;
            for (unsigned j = 0; j < kickCount; ++j) {
                const float ratio = kickIntervals[j] / candidate;
                const float multiple = roundf(ratio);
                if (multiple >= 1.0f && multiple <= 3.0f &&
                    fabsf(ratio - multiple) < 0.08f) {
                    ++supported;
                    if (multiple == 1.0f) {
                        ++direct;
                        sum += kickIntervals[j];
                    }
                }
            }
            if (direct >= 4 && supported * 4 >= kickCount * 3 &&
                (direct > bestDirect || (direct == bestDirect && supported > bestSupported))) {
                bestDirect = direct;
                bestSupported = supported;
                bestPeriod = sum / direct;
            }
        }
        const bool establishingKickGrid = !kickSupported;
        kickSupported = bestPeriod > 0.0f;
        if (!kickSupported) {
            return;
        }
        kickPeriod = bestPeriod;
        const float targetBpm = fmaxf(60.0f, fminf(200.0f, 60.0f / kickPeriod));
        const bool acquiring = establishingKickGrid || !haveTempo || !lockHeld || fabsf(targetBpm - bpm) > bpm * 0.15f;
        bpm = acquiring ? targetBpm : bpm + 0.25f * (targetBpm - bpm);
        haveTempo = true;
        confidence = fmaxf(confidence, 0.75f);
        pendingEstimates = 0;
        // Correct only kicks near the predicted beat after acquisition. A
        // syncopated kick must not drag every subsequent beat off the grid.
        float error = -phase;
        error -= floorf(error + 0.5f);
        if (acquiring || fabsf(error) < 0.20f) {
            phase = wrap(phase + error * (acquiring ? 1.0f : 0.5f));
        }
    }

    float history[Capacity] = {};
    float scores[MaximumLag + 2] = {};
    AttackDetector attacks;
    bool lockHeld = false;
    size_t writeIndex = 0;
    size_t count = 0;
    unsigned scanLag = MinimumLag - 1;
    unsigned hopPair = 0;
    float accumulatedNovelty = 0.0f;
    float noveltyEnvelope = 0.0f;
    float quietSeconds = 1.0f;
    float bpm = 120.0f;
    float phase = 0.0f;
    float confidence = 0.0f;
    float pendingBpm = 0.0f;
    unsigned pendingEstimates = 0;
    bool haveTempo = false;
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

    // Find the phase supported by the complete novelty history. Anchoring to
    // just the latest onset could choose a hi-hat halfway between bass beats.
    void alignPhase(bool acquiring) {
        float real = 0.0f;
        float imaginary = 0.0f;
        float weight = 0.0f;
        const float step = 6.28318530718f / NoveltyRate * bpm / 60.0f;
        const float rotateReal = cosf(step);
        const float rotateImaginary = sinf(step);
        float cosine = 1.0f;
        float sine = 0.0f;
        for (size_t age = 0; age < count; ++age) {
            const float value = atAge(age);
            real += value * cosine;
            imaginary += value * sine;
            weight += value;
            const float nextCosine = cosine * rotateReal - sine * rotateImaginary;
            sine = sine * rotateReal + cosine * rotateImaginary;
            cosine = nextCosine;
        }
        // Weak/canceling phase evidence must not snap a running clock.
        if (weight <= 0.0f || sqrtf(real * real + imaginary * imaginary) < weight * 0.15f) {
            return;
        }
        const float target = wrap(atan2f(imaginary, real) / 6.28318530718f +
                                  (256.0f / 31250.0f) * bpm / 60.0f);
        float error = target - phase;
        error -= floorf(error + 0.5f);
        phase = wrap(phase + error * (acquiring ? 1.0f : 0.5f));
    }

    void chooseTempo() {
        // A recent, regular dedicated kick train takes precedence over
        // alternating spectral accents and offbeat hats. Release it promptly
        // when kicks disappear; the spectral history continues throughout.
        if (kickSupported && kickAge < fminf(2.1f, kickPeriod * 2.5f)) {
            return;
        }
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
        const bool acquiring = !lockHeld;
        confidence += 0.35f * (certainty - confidence);
        if (certainty < LockConfidence) {
            return;
        }

        if (!haveTempo || acquiring) {
            bpm = candidate;
            alignPhase(true);
            haveTempo = true;
            pendingEstimates = 0;
        } else if (fabsf(candidate - bpm) < bpm * 0.15f) {
            bpm += 0.2f * (candidate - bpm);
            alignPhase(false);
            pendingEstimates = 0;
        } else {
            // Require repeated agreement before jumping to a new tempo family.
            pendingEstimates =
                fabsf(candidate - pendingBpm) < candidate * 0.06f ? pendingEstimates + 1 : 1;
            pendingBpm = candidate;
            if (pendingEstimates >= 3) {
                bpm = candidate;
                alignPhase(true);
                pendingEstimates = 0;
            }
        }
    }
};
