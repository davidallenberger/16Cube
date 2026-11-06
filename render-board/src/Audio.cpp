#include "Audio.h"
#include "TestDemo.h"
#include "AppMemory.h"
#include <math.h>
#include <cmath>

#ifndef AUDIO_SERIAL_DEBUG
#define AUDIO_SERIAL_DEBUG 1
#endif

Audio::Controls &Audio::controls() {
    // Future menus may update these on the animation task. They are not UART
    // globals and must not be mutated concurrently by another task.
    static Controls settings;
    return settings;
}

namespace {
    float juliaAudioClosure() {
        const auto audio = RemoteInputs::audio();
        const auto &settings = Audio::controls();
        const float gain = std::isfinite(settings.juliaSensitivity) ?
            min(8.0f, max(0.0f, settings.juliaSensitivity)) : 1.5f;
        const float gamma = std::isfinite(settings.juliaVolumeGamma) ?
            min(4.0f, max(0.25f, settings.juliaVolumeGamma)) : 1.0f;
        const float volume = min(1.0f, float(audio.volumeSm) * gain / 255.0f);
        const float closure = gamma == 1.0f ? volume : powf(volume, gamma);
        char detail[96];
        snprintf(detail, sizeof(detail), "Julia volume=%u target=%.3f gain=%.2f gamma=%.2f",
            unsigned(audio.volumeSm), double(closure), double(gain), double(gamma));
        Audio::debug(audio, detail);
        return closure;
    }
}

void Audio::animateJulia() {
    // Volume replaces the timed open/close oscillator. Small contour motion
    // and the approved 4s hold / 2s hue transitions continue independently.
    JuliaRenderer::animateJulia(0, controls().juliaStepMs, true, true,
        juliaAudioClosure, TestDemo::audioRoutineRunning,
        controls().juliaAttackMs, controls().juliaReleaseMs);
}

void Audio::renderIcon(TextureState &) {
    for (uint8_t y = 0; y < RNDR_Y; ++y) {
        for (uint8_t x = 0; x < RNDR_X; ++x) {
            setVoxel(x, y, RNDR_Z / 2, CRGB::Blue);
        }
    }
}

// Convenience entry point for a routine that has not already read audio.
// Routines with a snapshot should use debug(audio) to describe the exact data
// they are drawing, without taking a second audio snapshot for the report.
void Audio::debug() {
    debug(RemoteInputs::audio());
}

void Audio::debug(const RemoteInputs::AudioFrame &audio, const char *detail) {
#if AUDIO_SERIAL_DEBUG
    // Render task only. No printing in UARTReceiver and no lock held during TX.
    // Static storage survives a nonlocal app exit; no resource-owning locals.
    static char text[640];
    static size_t queued = 0;
    static size_t sent = 0;
    static uint32_t lastReport = 0;
    const uint32_t now = millis();

    if (sent >= queued && uint32_t(now - lastReport) >= 500) {
        lastReport = now;
        sent = 0;
        queued = 0;
        const RemoteInputs::Control control = RemoteInputs::control();
        const RemoteInputs::Stats stats = RemoteInputs::stats();
        const char *source = "Unknown";
        switch (control.mode) {
        case CubinaWire::Mode::AudioMic:
            source = "Mic";
            break;
        case CubinaWire::Mode::AudioBluetooth:
            source = "ADC";
            break;
        case CubinaWire::Mode::AudioSD:
            source = "SD";
            break;
        case CubinaWire::Mode::AudioDJ:
            source = "DJ";
            break;
        case CubinaWire::Mode::Animation:
            source = "Animation";
            break;
        case CubinaWire::Mode::Game:
            source = "Game";
            break;
        case CubinaWire::Mode::Depth:
            source = "Depth";
            break;
        }
        int n = snprintf(
            text, sizeof(text),
            "Audio: source=%s routine=%s linked=%u VRaw=%u VSm=%u VAvg=%u "
            "BPM=%.1f Bass=%u Mid=%u High=%u RXok=%lu RXbad=%lu Hold=%u %s\r\nBins:[",
            source, TestDemo::currentAudioRoutineName(), unsigned(control.linked),
            unsigned(audio.volumeRaw), unsigned(audio.volumeSm), unsigned(audio.volumeAvg),
            double(audio.bpm), unsigned(audio.bassKick), unsigned(audio.midAttack),
            unsigned(audio.highAttack), (unsigned long)stats.accepted,
            (unsigned long)stats.rejected, unsigned(TestDemo::playbackHeld()), detail);
        if (n <= 0 || size_t(n) >= sizeof(text)) {
            return;
        }
        queued = size_t(n);

        for (unsigned i = 0; i < 16; ++i) {
            n = snprintf(text + queued, sizeof(text) - queued, i == 15 ? "%u]\r\n" : "%u ",
                         unsigned(audio.fftResult[i]));
            if (n <= 0 || size_t(n) >= sizeof(text) - queued) {
                queued = 0;
                return;
            }
            queued += size_t(n);
        }
    }

    if (sent < queued) {
        const int space = Serial.availableForWrite();
        if (space > 0) {
            size_t chunk = min(size_t(space), queued - sent);
            chunk = min(chunk, size_t(128));
            sent += Serial.write(reinterpret_cast<uint8_t *>(text + sent), chunk);
        }
    }
#endif
}

void Audio::animatePuddles() {
    puddles(false, PuddleShape(random(3)), controls().puddlesSensitivity,
            controls().maxCubeEdge);
}

void Audio::animatePuddlePeak() {
    puddles(true, PuddleShape(random(3)), controls().puddlePeakSensitivity,
            controls().maxCubeEdge, controls().puddlePeakDensity);
}

// Inspired by Puddles/Puddlepeak by Andrew Tuline, merged by @dedehai.
// Both effects share geometry and fading. Ordinary Puddles responds to volume;
// Puddle Peak responds to the same bass/mid/high events every other animation
// sees. There is no second detector, baseline, timestamp or rearm algorithm
// hidden inside this visual. Its sensitivity only admits weaker shared events.
void Audio::puddles(bool peakDetect, PuddleShape shape, float sensitivity, uint8_t maxCubeEdge,
                    float peakDensity) {
    constexpr float PUDDLE_FADE_TIME_MS = 250.0f;
    constexpr uint32_t PUDDLE_SPAWN_INTERVAL_MS = 34;

    // Ordinary Puddles' 250-ms exponential fade and frame-rounded 34-ms
    // cadence leave roughly six full-brightness shapes' worth on screen.
    // Use that as the reference footprint of ONE accepted peak. This is a
    // starting artistic match, not a promise of identical average occupancy:
    // peak bursts are synchronized to sounds, and can be far apart.
    constexpr float REFERENCE_BURST_SHAPES = 6.0f;
    constexpr float REFERENCE_CANVAS_VOXELS = 16.0f * 16.0f * 16.0f;
    constexpr unsigned MAX_BURST_SHAPES = 32;
    constexpr unsigned MAX_BURST_VOXEL_WRITES = 2048;
    peakDensity = std::isfinite(peakDensity) ? max(0.0f, peakDensity) : 1.0f;
    sensitivity = std::isfinite(sensitivity) ? max(0.0f, sensitivity) : 1.0f;

    uint32_t lastFade = millis();
    uint32_t lastSpawn = millis();
    uint8_t hue = 0;
    const char *shapeName = shape == PuddleShape::Horizontal ? "Horizontal" :
        shape == PuddleShape::Vertical ? "Vertical" : "Cube";

    while (TestDemo::audioRoutineRunning()) {
        const uint32_t now = millis();
        const uint32_t elapsed = uint32_t(now - lastFade);

        if (elapsed > 0) {
            // Match ordinary Puddles' fade, so the two effects differ mainly
            // in WHEN a line appears rather than in how long it remains.
            const float remaining = expf(-float(elapsed) / PUDDLE_FADE_TIME_MS);
            const uint8_t fade = uint8_t(min(255.0f, max(1.0f, 255.0f * (1.0f - remaining))));
            fadeAll(fade);
            lastFade = now;
        }

        const auto audio = RemoteInputs::audio(sensitivity);
        char detail[80];
        snprintf(detail, sizeof(detail), "Puddles shape=%s color=solid-per-spawn", shapeName);
        Audio::debug(audio, detail);

        const bool hit = audio.bassKick || audio.midAttack || audio.highAttack;
        const bool volumeReady = audio.volumeSm * sensitivity >= 2.0f;
        const bool intervalReady = uint32_t(now - lastSpawn) >= PUDDLE_SPAWN_INTERVAL_MS;
        const bool spawn = peakDetect ? hit && peakDensity > 0.0f : volumeReady && intervalReady;

        if (spawn && sensitivity > 0.0f) {
            lastSpawn = now;

            // Peak sensitivity changes the trigger; ordinary sensitivity
            // changes size. Both use the SAME volume for all geometry choices.
            const float response = peakDetect ? 1.0f : sensitivity;
            const uint8_t volume = uint8_t(min(255.0f, audio.volumeSm * response));
            // Original regime: one solid hue per spawn/burst, retained while
            // fading. Hue advances on the next spawn; no spatial shader.
            const CRGB color = CHSV(hue++, 220, 255);
            const bool vertical = shape == PuddleShape::Vertical;
            const uint8_t dimension = vertical ? RNDR_Z : RNDR_X;
            const uint8_t length = 1 + uint16_t(volume) * (dimension - 1) / 255;
            const uint8_t fit = min(RNDR_X, min(RNDR_Y, RNDR_Z));
            const uint8_t referenceMaximum = max(uint8_t(1), min(uint8_t(5), maxCubeEdge));
            const uint8_t maximum = min(referenceMaximum, fit);
            const uint8_t edge = 1 + uint16_t(volume) * (maximum - 1) / 255;
            unsigned shapeCount = 1; // Ordinary Puddles retains its existing cadence.

            if (peakDetect) {
                // Preserve the same relative footprint on a smaller canvas.
                // Compare this shape with its equivalent on the reference
                // 16^3 cube at the SAME volume. Otherwise a shorter line on a
                // small canvas would incorrectly be counted as a full line.
                // Size/volume information is retained: quiet sounds still
                // draw smaller shapes; density does not compensate it away.
                const unsigned actualVoxels = shape == PuddleShape::Cube
                    ? unsigned(edge) * edge * edge
                    : length;
                const unsigned referenceEdge = 1 + uint16_t(volume) * (referenceMaximum - 1) / 255;
                const unsigned referenceVoxels = shape == PuddleShape::Cube
                    ? referenceEdge * referenceEdge * referenceEdge
                    : 1 + uint16_t(volume) * 15 / 255;
                const float canvasScale = float(unsigned(RNDR_X) * RNDR_Y * RNDR_Z) /
                    REFERENCE_CANVAS_VOXELS;
                const float requested = REFERENCE_BURST_SHAPES * peakDensity * canvasScale *
                    float(referenceVoxels) / float(actualVoxels);

                // Bound both random-placement work and voxel writes, even
                // for an extreme density setting. This needs no pool, heap,
                // task or extra arena. A fractional request rounds upward so
                // a positive density still gives a small canvas a response.
                const unsigned shapeLimit = min(MAX_BURST_SHAPES,
                    max(1u, MAX_BURST_VOXEL_WRITES / actualVoxels));
                shapeCount = unsigned(ceilf(fminf(requested, float(shapeLimit))));
            }

            // All shapes in this burst appear in this SAME frame. Random
            // positions add coverage, while their shared spawn time ties them
            // to one audible event. Nothing spawns between accepted events.
            for (unsigned item = 0; item < shapeCount; ++item) {
                if (shape == PuddleShape::Cube) {
                    const uint8_t x0 = random(RNDR_X - edge + 1);
                    const uint8_t y0 = random(RNDR_Y - edge + 1);
                    const uint8_t z0 = random(RNDR_Z - edge + 1);
                    for (uint8_t z = z0; z < z0 + edge; ++z) {
                        for (uint8_t y = y0; y < y0 + edge; ++y) {
                            for (uint8_t x = x0; x < x0 + edge; ++x) {
                                setVoxel(x, y, z, color);
                            }
                        }
                    }
                } else {
                    // Z is vertical; X is the existing horizontal line.
                    uint8_t x = random(vertical ? RNDR_X : RNDR_X - length + 1);
                    const uint8_t y = random(RNDR_Y);
                    uint8_t z = random(vertical ? RNDR_Z - length + 1 : RNDR_Z);
                    for (uint8_t i = 0; i < length; ++i) {
                        setVoxel(x, y, z, color);
                        if (vertical) {
                            ++z;
                        } else {
                            ++x;
                        }
                    }
                }
            }
        }

        // Silence or stale input stops new lines; existing lines fade away.
        // Every frame also checks for the next-routine or mode-change escape.
        showCube();
    }
}

void Audio::animateBpmSphere() {
    bpm(BPM_SPHERE, controls().bpmSensitivity);
}

void Audio::animateAttackMonitor() {
    uint32_t flashStarted = 0;
    bool flashing = false;
    while (TestDemo::audioRoutineRunning()) {
        const auto audio = RemoteInputs::audio(controls().attackSensitivity);
        const uint32_t now = millis();
        if (audio.bassKick || audio.midAttack || audio.highAttack) {
            flashStarted = now;
            flashing = true;
        }
        if (uint32_t(now - flashStarted) >= 80) {
            flashing = false;
        }
        clearAll();
        if (flashing) {
            // Fixed location/size/brightness isolates WHEN an attack arrives.
            // The short visible hold is presentation, not another detector.
            for (uint8_t z = RNDR_Z / 2 - 2; z < RNDR_Z / 2 + 2; ++z) {
                for (uint8_t y = RNDR_Y / 2 - 2; y < RNDR_Y / 2 + 2; ++y) {
                    for (uint8_t x = RNDR_X / 2 - 2; x < RNDR_X / 2 + 2; ++x) {
                        setVoxel(x, y, z, CRGB::White);
                    }
                }
            }
        }
        Audio::debug(audio);
        showCube();
    }
}

void Audio::animateBpmCheckerboard() {
    bpm(BPM_CHECKERBOARD, controls().bpmSensitivity);
}

void Audio::animateBpmDiagonal() {
    bpm(BPM_DIAGONAL, controls().bpmSensitivity);
}

void Audio::animateBpmHelix() {
    bpm(BPM_HELIX, controls().bpmSensitivity);
}

void Audio::bpm(BpmSpatialMode mode, float sensitivity) {
    sensitivity = std::isfinite(sensitivity) ? max(0.0f, sensitivity) : 1.0f;

    // Every spatial mode uses the shared musical clock. Volume changes
    // brightness only; it cannot accelerate or reset the breathing cycle.
    uint8_t *phaseMap = reinterpret_cast<uint8_t *>(SharedAppMemory);
    BpmRenderer::prepare(phaseMap, mode);

    while (TestDemo::audioRoutineRunning()) {
        const auto audio = RemoteInputs::audio();
        Audio::debug(audio);

        // Checkerboard has an adjustable beats-per-breath setting. Use the same
        // period for brightness and spatial travel so both stay in step.
        // This divides the shared clock without changing the BPM estimate.
        const uint16_t beatsPerCycle = mode == BPM_CHECKERBOARD ?
            max(uint16_t(1), controls().checkerBeatsPerCycle) : 1;
        const float breath = 1.0f - audio.beatPulse(beatsPerCycle);
        const float cycle = audio.beatAngle(beatsPerCycle) / 6.283185307f;
        const uint8_t beat = uint8_t(64.0f + breath * 191.0f);
        const uint8_t travel = uint8_t(cycle * 256.0f);

        // Volume remains an artistic brightness control. Zero/stale volume
        // keeps the familiar dim idle breath. Timing does not rescale volume.
        const float volume = min(255.0f, audio.volumeSm * sensitivity);
        const float amplitude = volume > 1.0f ? 64.0f + volume * 0.6f : 48.0f;
        const uint8_t level = uint8_t(min(255.0f, 32.0f + amplitude * breath));
        BpmRenderer::draw(phaseMap, RainbowColors_p, uint8_t(beat - travel),
                          uint8_t(millis() / 40), level);

        showCube();
    }
}

// Waverly-inspired geometry, based on Stepko / Andrew Tuline's WLED
// v16.0.1 mode_2DWaverly. This is a dimension-aware volumetric adaptation:
// noise has TWO spatial coordinates and one time coordinate; Z is height.
// Coloring is deliberately independent so audition entries share the exact
// same geometry, gain and noise trajectory. No FFT/beat detector is involved.
void Audio::waverly(ShaderMode mode, CRGBPalette16 palette,
                    bool columnGradient, uint32_t durationMs) {
    // Keep excluded shaders out of Waverly even for direct experimental calls.
    if (!columnGradient && mode != MODE_LINEAR_FLOW && mode != MODE_NOISE_FIELD &&
        mode != MODE_DISTANCE_FIELD && mode != MODE_HIPHOTIC && mode != MODE_DISTORTION_WAVES) {
        return;
    }

    // All locals are plain data. No allocation, locks or destructor-owned
    // resources span showCube(), which can escape through the existing OS.
    // The 8192-byte arena remains free; only the existing LED buffer is used.
    TextureState texture(mode, palette, 1.0f);
    const uint32_t startedMs = millis();
    uint32_t lastUpdateMs = startedMs;
    bool firstFrame = true;
    float tallest = 0.0f;
    bool attackActive = false;
    uint32_t lastAttackMs = startedMs;
    clearAll();

    while (TestDemo::audioRoutineRunning() &&
           (durationMs == 0 || TestDemo::routineTimeRemaining(uint32_t(millis() - startedMs), durationMs))) {
        const auto audio = RemoteInputs::audio(controls().waverlyAttackSensitivity);
        const uint32_t now = millis();
        const uint32_t elapsedMs = uint32_t(now - startedMs);
        const uint32_t deltaMs = uint32_t(now - lastUpdateMs);
        const uint32_t stepMs = min(uint32_t(250), max(uint32_t(1), controls().waverlyStepMs));
        const float gain = std::isfinite(controls().waverlySensitivity) ?
            min(8.0f, max(0.0f, controls().waverlySensitivity)) : 1.5f;

        // Capture shared events on EVERY display pass, including passes
        // between slower geometry updates. No private detector or beat ticks.
        // Broad sounds may carry both flags; they refresh one pulse, not two.
        if (audio.midAttack || audio.highAttack) {
            lastAttackMs = now;
            attackActive = true;
        }
        const uint32_t decayMs = min(uint32_t(2000), max(uint32_t(1), controls().waverlyAttackDecayMs));
        const uint32_t attackAgeMs = uint32_t(now - lastAttackMs);
        if (attackAgeMs >= decayMs) {
            attackActive = false;
        }
        const float attackFraction = std::isfinite(controls().waverlyAttackReach) ?
            min(0.25f, max(0.0f, controls().waverlyAttackReach)) : 0.15f;
        const float attackReach = attackActive ?
            float(RNDR_Z) * attackFraction * (1.0f - float(attackAgeMs) / float(decayMs)) : 0.0f;

        if (firstFrame || deltaMs >= stepMs) {
            // Decay follows real elapsed time rather than frame count. Faster
            // rendering therefore does not shorten trails or brighten buildup.
            const float retention = 1.0f - float(controls().waverlyFade) / 255.0f;
            const uint8_t fade = uint8_t(255.0f *
                (1.0f - powf(retention, float(deltaMs) / 32.0f)));
            fadeAll(fade);
            texture.advance(deltaMs);
            const float noiseRate = std::isfinite(controls().waverlyNoiseRate) ?
                min(1.0f, max(0.0f, controls().waverlyNoiseRate)) : 0.12f;
            // Wrap before conversion: a long Hold must not overflow a float
            // conversion into an integer. inoise8 has periodic coordinates.
            const uint16_t noiseTime = uint16_t(fmodf(float(elapsedMs) * noiseRate, 65536.0f));
            const uint8_t palettePhase = columnGradient ? uint8_t((elapsedMs / 80) & 255) : 0;
            const float volume = float(audio.volumeSm) / 255.0f;
            const float maximumReach = float(RNDR_Z) * 0.9f;
            tallest = 0.0f;

            for (uint8_t y = 0; y < RNDR_Y; ++y) {
                for (uint8_t x = 0; x < RNDR_X; ++x) {
                    // Three broad noise cells across either footprint, even
                    // on a 10x10x16 canvas. Depth is never an extrusion of X.
                    const uint16_t nx = 1379 + uint32_t(x) * 768 / max(1, int(RNDR_X) - 1);
                    const uint16_t ny = 7919 + uint32_t(y) * 768 / max(1, int(RNDR_Y) - 1);
                    const float noise = float(inoise8(nx, ny, noiseTime)) / 255.0f;
                    const float height = min(float(RNDR_Z), noise * (volume * gain * maximumReach + attackReach));
                    tallest = max(tallest, height);
                    for (uint8_t z = 0; z < RNDR_Z; ++z) {
                        // Fractional tips avoid whole layers popping on/off.
                        // At zero volume no new light is added; trails decay.
                        const float coverage = min(1.0f, max(0.0f, height - float(z)));
                        if (coverage <= 0.0f) {
                            break;
                        }
                        CRGB color;
                        if (columnGradient) {
                            const float alongColumn = min(1.0f, float(z) / max(1.0f, height - 1.0f));
                            const uint8_t index = uint8_t(250.0f * (1.0f - alongColumn)) + palettePhase;
                            color = ColorFromPalette(palette, index, 255, LINEARBLEND);
                        } else {
                            color = texture.getColor(x, y, z);
                        }
                        color.nscale8(uint8_t(coverage * 255.0f));
                        const uint8_t mx = RNDR_X - 1 - x;
                        const uint8_t my = RNDR_Y - 1 - y;
                        const uint8_t mz = RNDR_Z - 1 - z;
                        // Copy the COLOR with the geometry, preserving exact
                        // full-inversion symmetry for every texture choice.
                        CRGB lower = getVoxel(x, y, z);
                        lower += color;
                        setVoxel(x, y, z, lower);
                        CRGB upper = getVoxel(mx, my, mz);
                        upper += color;
                        setVoxel(mx, my, mz, upper);
                    }
                }
            }
            lastUpdateMs = now;
            firstFrame = false;
        }

        char detail[192];
        snprintf(detail, sizeof(detail),
                 "Waverly height=%.1f/%u gain=%.2f step=%lums color=%s mid=%u high=%u push=%.2f",
                 double(tallest), unsigned(RNDR_Z), double(gain),
                 (unsigned long)stepMs, columnGradient ? "gradient-cycle" : "moving@1x",
                 unsigned(audio.midAttack), unsigned(audio.highAttack), double(attackReach));
        Audio::debug(audio, detail);
        // Keep servicing buttons/UART even between slower geometry updates.
        // showCube() supplies the existing 60 Hz limiter; no blocking sleep.
        showCube();
    }
}

void Audio::animateSurroundGEQ() {
    constexpr uint8_t groupCount = 8;
    // Keep the four lowest bands for the sphere. Preserve the first four
    // non-bass bands individually and combine pairs higher up the spectrum.
    constexpr uint8_t edges[] = {4, 5, 6, 7, 8, 10, 12, 14, 16};
    const CRGB colors[groupCount] = {
        CRGB(0xFF0000), CRGB(0xFF8000), CRGB(0xFFFF00), CRGB(0x00FF00),
        CRGB(0x0000FF), CRGB(0x4B0082), CRGB(0x9400D3), CRGB(0xFFFFFF)
    };
    // Two-deep blocks on the full cube. One-deep on the narrower cuboid
    // keeps the corner bands unambiguous and leaves room for the sphere.
    const int thickness = min(RNDR_X, RNDR_Y) / groupCount;
    const float maxRadius = min(RNDR_CX, RNDR_CY) - thickness + 0.5f;
    const int maxBarHeight = RNDR_Z - 1; // Top row is available for the peak.
    const TexturePlan plan = PaletteUtils::getRandomMovingTexturePlan();
    TextureState texture(plan.sMode, plan.palette, 1.0f);
    float peaks[groupCount] = {};
    uint32_t peakMs[groupCount] = {};
    // All voxels at the same distance share coverage, so store one value
    // per half-voxel shell instead of keeping another cube buffer.
    constexpr int shellCount = RNDR_X + RNDR_Y;
    constexpr float recessionMs = 180.0f;
    float sphereCoverage[shellCount] = {};
    uint32_t lastMs = millis();

    while (TestDemo::audioRoutineRunning()) {
        const auto audio = RemoteInputs::audio();
        const uint32_t now = millis();
        const uint32_t elapsedMs = uint32_t(now - lastMs);
        texture.advance(elapsedMs);
        lastMs = now;

        uint8_t bass = 0;
        for (uint8_t band = 0; band < edges[0]; ++band) {
            bass = max(bass, audio.fftResult[band]);
        }
        const float setting = controls().surroundGeqGain;
        const float gain = std::isfinite(setting) ? min(8.0f, max(0.0f, setting)) : 3.0f;
        const float bassSetting = controls().surroundGeqBassGain;
        const float bassGain = std::isfinite(bassSetting) ?
            min(8.0f, max(0.0f, bassSetting)) : 2.5f;
        const float radius = maxRadius * min(1.0f, bass * bassGain / 255.0f);
        // Loudness lifts all bars together. Spectrum strength determines
        // how much of the remaining height each bar fills.
        const float baseline = RNDR_Z * (3.0f / 16.0f) * min(1.0f, audio.volumeSm * gain / 255.0f);
        float heights[groupCount];
        int peakRows[groupCount];
        for (uint8_t group = 0; group < groupCount; ++group) {
            uint8_t level = 0;
            for (uint8_t band = edges[group]; band < edges[group + 1]; ++band) {
                level = max(level, audio.fftResult[band]);
            }
            const float height = baseline + (maxBarHeight - baseline) * min(1.0f, level * gain / 255.0f);
            heights[group] = height;
            if (height >= peaks[group]) {
                peaks[group] = height;
                peakMs[group] = now;
            } else if (uint32_t(now - peakMs[group]) >= 120) {
                // Drop at eight voxels per second after a brief hold.
                peaks[group] = max(height, peaks[group] - 0.008f * elapsedMs);
            }
            peakRows[group] = min(maxBarHeight, int(ceilf(peaks[group])));
        }

        clearAll();
        const int radiusSteps = int(radius * 2.0f);
        const float fade = 255.0f * elapsedMs / recessionMs;
        int outerShell = -1;
        for (int shell = 0; shell < shellCount; ++shell) {
            // Match the solid sphere's half-voxel radius and edge coverage.
            const int edge = shell - radiusSteps + 2;
            const int target = radius > 0.0f ? 255 - min(4, max(0, edge)) * 255 / 4 : 0;
            sphereCoverage[shell] = max(float(target), sphereCoverage[shell] - fade);
            if (sphereCoverage[shell] >= 1.0f) {
                outerShell = shell;
            }
        }
        if (outerShell >= 0) {
            const int distanceLimit = (outerShell + 1) * (outerShell + 1);
            for (int x = 0; x < RNDR_X; ++x) {
                const int dx = 2 * x - int(2 * RNDR_CX);
                for (int y = 0; y < RNDR_Y; ++y) {
                    const int dy = 2 * y - int(2 * RNDR_CY);
                    const int xySquared = dx * dx + dy * dy;
                    if (xySquared >= distanceLimit) continue;
                    for (int z = 0; z < RNDR_Z; ++z) {
                        const int dz = 2 * z - int(2 * RNDR_CZ);
                        const int distanceSquared = xySquared + dz * dz;
                        if (distanceSquared >= distanceLimit) continue;
                        const int shell = int(sqrtf(float(distanceSquared)));
                        const uint8_t coverage = uint8_t(sphereCoverage[shell]);
                        if (coverage > 0) {
                            CRGB color = texture.getColor(x, y, z);
                            color.nscale8(coverage);
                            setVoxel(x, y, z, color);
                        }
                    }
                }
            }
        }
        // Traverse each footprint voxel once. Reversing successive faces
        // gives their shared corners the same band at either side of a seam.
        for (int y = 0; y < RNDR_Y; ++y) {
            for (int x = 0; x < RNDR_X; ++x) {
                int group;
                if (y < thickness) {
                    group = x * groupCount / RNDR_X;
                } else if (x >= RNDR_X - thickness) {
                    group = (RNDR_Y - 1 - y) * groupCount / RNDR_Y;
                } else if (y >= RNDR_Y - thickness) {
                    group = (RNDR_X - 1 - x) * groupCount / RNDR_X;
                } else if (x < thickness) {
                    group = y * groupCount / RNDR_Y;
                } else {
                    continue;
                }
                const float height = heights[group];
                for (int z = 0; z < RNDR_Z && z < height; ++z) {
                    CRGB color = colors[group];
                    color.nscale8(uint8_t(192.0f * min(1.0f, height - z)));
                    setVoxel(x, y, z, color);
                }
                // The full-brightness cap sits one row above the peak body.
                const int peakRow = peakRows[group];
                if (peaks[group] >= 1.0f) {
                    setVoxel(x, y, peakRow, colors[group]);
                }
            }
        }
        char detail[176];
        snprintf(detail, sizeof(detail), "GEQ gain=%.2f bassGain=%.2f baseline=%.2f bass=%u radius=%.2f bars=%.1f/%.1f/%.1f/%.1f/%.1f/%.1f/%.1f/%.1f",
                 double(gain), double(bassGain), double(baseline), unsigned(bass), double(radius),
                 double(heights[0]), double(heights[1]), double(heights[2]), double(heights[3]),
                 double(heights[4]), double(heights[5]), double(heights[6]), double(heights[7]));
        Audio::debug(audio, detail);
        showCube();
    }
}

namespace {
enum class CornerFront { Plane, Sphere, Box };

void drawCornerBurst(CornerFront shape) {
    enum class Phase { Waiting, Crest, Traveling };
    constexpr int farX = RNDR_X - 1, farY = RNDR_Y - 1, farZ = RNDR_Z - 1;
    const auto depthAt = [shape](int dx, int dy, int dz) {
        if (shape == CornerFront::Sphere) {
            return sqrtf(float(dx * dx + dy * dy + dz * dz));
        }
        if (shape == CornerFront::Box) {
            return float(max(dx, max(dy, dz)));
        }
        return float(dx + dy + dz);
    };
    const float maxDepth = depthAt(farX, farY, farZ);
    // Use a shared physical point on the bottom diagonal for attachment.
    const int capOffset = min(11, min(farX, farY));
    const float attachedCap = depthAt(capOffset, capOffset, 0);
    const float gainSetting = Audio::controls().cornerBurstGain;
    const float volumeGain = std::isfinite(gainSetting) ?
        max(0.0f, gainSetting) : 255.0f / 75.0f;
    const auto attachedDepth = [attachedCap, volumeGain](int volume) {
        return attachedCap * min(1.0f, volume * volumeGain / 255.0f);
    };
    constexpr uint32_t releaseMs = 40;
    constexpr float travelMs = 750.0f;
    constexpr int minDrop = 4;
    Phase phase = Phase::Waiting;
    int trough = 0;
    int crest = 0;
    bool confirming = false;
    uint32_t dropStarted = 0;
    uint32_t released = 0;
    float releaseDepth = 0.0f;
    CRGB releaseColor;
    const uint32_t started = millis();

    while (TestDemo::audioRoutineRunning()) {
        const auto audio = RemoteInputs::audio();
        const uint32_t now = millis();
        const int volume = audio.volumeSm;
        const float fill = attachedDepth(volume);
        const CRGB rainbow = CHSV(uint8_t(uint32_t(now - started) / 25), 255, 255);
        if (phase == Phase::Waiting) {
            trough = min(trough, volume);
            if (volume - trough >= minDrop) {
                crest = volume;
                confirming = false;
                phase = Phase::Crest;
            }
        }

        float front = 0.0f;
        float frontStrength = 0.0f;
        CRGB frontColor = rainbow;
        if (phase == Phase::Crest) {
            crest = max(crest, volume);
            // Keep the bright edge at the crest while the dim fill recedes.
            front = attachedDepth(crest);
            frontStrength = 1.0f;
            const int threshold = max(minDrop, (crest * 12 + 99) / 100);
            if (crest - volume < threshold) {
                confirming = false;
            } else if (!confirming) {
                dropStarted = now;
                confirming = true;
            // A rebound cancels confirmation. A sustained drop hands the
            // held crest to the traveling front at the same position and hue.
            } else if (uint32_t(now - dropStarted) >= releaseMs) {
                releaseDepth = front;
                releaseColor = rainbow;
                released = now;
                confirming = false;
                phase = Phase::Traveling;
            }
        } else if (phase == Phase::Traveling) {
            front = releaseDepth + maxDepth * uint32_t(now - released) / travelMs;
            if (front < maxDepth) {
                frontStrength = (maxDepth - front) / (maxDepth - releaseDepth);
                frontColor = releaseColor;
            } else {
                trough = volume;
                phase = Phase::Waiting;
            }
        }

        clearAll();
        // Measure from the bottom corner 90 degrees clockwise from (0,0,0). Sum gives
        // a diagonal plane, Euclidean distance a sphere, and max a box.
        for (int z = 0; z < RNDR_Z; ++z) {
            for (int y = 0; y < RNDR_Y; ++y) {
                for (int x = 0; x < RNDR_X; ++x) {
                    const int dx = x, dy = farY - y, dz = z;
                    const float depth = depthAt(dx, dy, dz);
                    const float body = 0.15f * min(1.0f, max(0.0f, fill - depth));
                    const float edge = frontStrength * max(0.0f, 1.0f - fabsf(depth - front) / 1.5f);
                    const uint8_t brightness = uint8_t(255.0f * max(body, edge));
                    if (brightness > 0) {
                        CRGB color = edge > body ? frontColor : rainbow;
                        color.nscale8(brightness);
                        setVoxel(x, y, z, color);
                    }
                }
            }
        }
        char detail[120];
        snprintf(detail, sizeof(detail), "Burst phase=%s crest=%d dropMs=%lu fill=%.1f front=%.1f gain=%.2f",
                 phase == Phase::Traveling ? "travel" : phase == Phase::Crest ? "crest" : "waiting",
                 crest, (unsigned long)(confirming ? uint32_t(now - dropStarted) : 0),
                 double(fill), double(front), double(volumeGain));
        Audio::debug(audio, detail);
        showCube();
    }
}
} // namespace

void Audio::animateCornerBurst() {
    drawCornerBurst(CornerFront::Plane);
}

void Audio::animateCornerBurstSphere() {
    drawCornerBurst(CornerFront::Sphere);
}

void Audio::animateCornerBurstBox() {
    drawCornerBurst(CornerFront::Box);
}
