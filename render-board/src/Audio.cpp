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

void Audio::debug(const RemoteInputs::AudioFrame &audio) {
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
            "BPM=%.1f Bass=%u Mid=%u High=%u RXok=%lu RXbad=%lu\r\nBins:[",
            source, TestDemo::currentAudioRoutineName(), unsigned(control.linked),
            unsigned(audio.volumeRaw), unsigned(audio.volumeSm), unsigned(audio.volumeAvg),
            double(audio.bpm), unsigned(audio.bassKick), unsigned(audio.midAttack),
            unsigned(audio.highAttack), (unsigned long)stats.accepted,
            (unsigned long)stats.rejected);
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
    puddles(false, PuddleShape::Horizontal, controls().puddlesSensitivity, controls().maxCubeEdge);
}

void Audio::animatePuddlesVertical() {
    puddles(false, PuddleShape::Vertical, controls().puddlesSensitivity, controls().maxCubeEdge);
}

void Audio::animatePuddlesCubes() {
    puddles(false, PuddleShape::Cube, controls().puddlesSensitivity, controls().maxCubeEdge);
}

void Audio::animatePuddlePeak() {
    puddles(true, PuddleShape::Horizontal, controls().puddlePeakSensitivity,
            controls().maxCubeEdge, controls().puddlePeakDensity);
}

void Audio::animatePuddlePeakVertical() {
    puddles(true, PuddleShape::Vertical, controls().puddlePeakSensitivity,
            controls().maxCubeEdge, controls().puddlePeakDensity);
}

void Audio::animatePuddlePeakCubes() {
    puddles(true, PuddleShape::Cube, controls().puddlePeakSensitivity,
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
        Audio::debug(audio);

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
            // positions add coverage, while their shared time/color tie them
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

    // Geometry preparation is unchanged and runs once. The audio adapter now
    // owns the clock, packet-age interpretation, locking and idle fallback.
    // None of that belongs in this drawing routine.
    uint8_t *phaseMap = reinterpret_cast<uint8_t *>(SharedAppMemory);
    BpmRenderer::prepare(phaseMap, mode);

    while (TestDemo::audioRoutineRunning()) {
        const auto audio = RemoteInputs::audio();
        Audio::debug(audio);

        // One cycle per musical beat, matching the existing animation speed.
        // The legacy drawing peaks at the beat boundary, so invert the shared
        // 0 -> 1 -> 0 pulse here. Changing the period needs only a new argument
        // to BOTH helpers (for example 4 means four beats per visual cycle).
        const float breath = 1.0f - audio.beatPulse(1);
        const float cycle = audio.beatAngle(1) / 6.283185307f;
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
