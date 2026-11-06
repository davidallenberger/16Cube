#pragma once

#include "AudioFrame.h"
#include <algorithm>
#include <cmath>

// Per-playback detector state. Lift is normalized to [0, 1].
struct MidrangeBoost {
    float bandBaseline[8] = {};
    float lift = 0.0f;
    uint32_t lastFftMs = 0;
    bool fftInitialized = false;
    float gate = 0.0f;
    float strongestRise = 0.0f;
    unsigned strongestBand = 0;

    void update(const RemoteInputs::AudioFrame &audio, uint32_t now,
                bool fftEnabled) {
        // One-based bands
        // 2..9 include lower instruments; downweight the two bass-adjacent bands.
        // Track each band's recent level so steady spectral energy settles out.
        const uint32_t fftDeltaMs = fftInitialized ? uint32_t(now - lastFftMs) : 0;
        lastFftMs = now;
        const float baselineBlend = 1.0f - expf(-float(fftDeltaMs) / 1000.0f);
        strongestRise = 0.0f;
        strongestBand = 0;
        for (unsigned i = 0; i < 8; ++i) {
            const float level = float(audio.fftResult[i + 1]);
            if (!fftInitialized || !fftEnabled) bandBaseline[i] = level;
            const float weight = i == 0 ? 0.35f : (i == 1 ? 0.65f : 1.0f);
            const float rise = std::max(0.0f, std::min(level - 12.0f, level - bandBaseline[i] - 5.0f)) * weight;
            if (rise > strongestRise) {
                strongestRise = rise;
                strongestBand = i + 2; // Human-readable, one-based bin.
            }
            bandBaseline[i] += baselineBlend * (level - bandBaseline[i]);
        }
        fftInitialized = true;
        // Either volume/bass does the work, or spectral change supplies a small
        // fallback. Gates are continuous; a kick flag is not required to suppress.
        const float bass = float(std::max(audio.fftResult[0], audio.fftResult[1]));
        gate = std::min(std::min(1.0f, std::max(0.0f, (80.0f - bass) / 35.0f)),
                               std::min(1.0f, std::max(0.0f, (50.0f - float(audio.volumeSm)) / 25.0f)));
        const float candidate = std::min(1.0f, strongestRise / 25.0f) * gate;
        // A refreshable 250 ms release, never a sum of bands/events. Clamp the
        // stored tail too, preventing a suppressed boost returning after bass.
        lift = fftEnabled ? std::min(gate,
            std::max(candidate, lift - float(fftDeltaMs) / 250.0f)) : 0.0f;
    }
};
