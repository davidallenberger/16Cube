#include "Audio.h"
#include "JuliaRenderer.h"
#include "Streamers.h"
#include "MidrangeBoost.h"
#include "TestDemo.h"
#include "AppMemory.h"
#include "GeometryEngine.h"
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

bool Audio::displaySpectrum(const RemoteInputs::AudioFrame& audio, uint8_t (&bins)[16]) {
    // The boost below hand-compensated the ADC module's ~235 Hz roll-off.
    // When the command board has already undone it, applying both would
    // double the treble.
    const bool active = controls().spectrumCompensationEnabled && !audio.spectrumCorrected;
    // Start with the ADC reference song and its quiet lead-in. Leave the
    // bass bins alone and lift only the signal above each higher band's floor.
    constexpr uint8_t floor[] = {3, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4};
    constexpr float gain[] = {1.25f, 1.45f, 1.6f, 2.2f, 2.7f, 3.3f, 3.7f, 4.2f, 4.7f, 5.0f, 5.5f, 5.5f};
    for (int band = 0; band < 16; ++band) {
        const int raw = audio.fftResult[band];
        bins[band] = active && band >= 4
            ? min(255, int(lroundf(max(0, raw - floor[band - 4]) * gain[band - 4])))
            : raw;
    }
    return active;
}

namespace {
    float midrangeBoostGain() {
        const float gain = Audio::controls().midrangeBoostGain;
        return std::isfinite(gain) ? min(1.0f, max(0.0f, gain)) : Audio::Controls{}.midrangeBoostGain;
    }

    // The midrange boost hand-compensated the old processing path. The new
    // path's corrected spectrum is judged native: animations tune their own
    // gain instead of patching the data. OLD keeps the boost for the A/B.
    bool midrangeBoostActive(const RemoteInputs::AudioFrame &audio) {
        return Audio::controls().midrangeBoostEnabled && !audio.spectrumCorrected;
    }

    // Each renderer takes a plain callback. Reset its detector on entry.
    MidrangeBoost juliaBoost;
    MidrangeBoost driftRoseBoost;

    float juliaAudioClosure() {
        const auto audio = RemoteInputs::audio();
        const auto &settings = Audio::controls();
        const float gain = std::isfinite(settings.juliaSensitivity) ?
            min(8.0f, max(0.0f, settings.juliaSensitivity)) : 1.5f;
        const float gamma = std::isfinite(settings.juliaVolumeGamma) ?
            min(4.0f, max(0.25f, settings.juliaVolumeGamma)) : Audio::Controls{}.juliaVolumeGamma;
        uint8_t bins[16];
        Audio::displaySpectrum(audio, bins);
        juliaBoost.update(audio, millis(), midrangeBoostActive(audio), bins);
        const float lift = juliaBoost.lift * midrangeBoostGain();
        const float volume = min(1.0f, float(audio.volumeSm) * gain / 255.0f + lift);
        const float closure = gamma == 1.0f ? volume : powf(volume, gamma);
        /*
        char detail[96];
        snprintf(detail, sizeof(detail), "Julia curve=%s volume=%u target=%.3f gain=%.2f gamma=%.2f fft=%.3f",
            settings.spectrumCompensationEnabled ? "comp" : "raw", unsigned(audio.volumeSm),
            double(closure), double(gain), double(gamma), double(lift));
        Audio::debug(audio, detail, 0, bins);
        */
        return closure;
    }

    float driftRoseReach() {
        constexpr float quietReach = 0.40f;
        constexpr float midrangeMultiplier = 2.0f;
        const auto audio = RemoteInputs::audio();
        const auto &settings = Audio::controls();
        const int fullVolume = max(1, int(settings.driftRoseVolumeForFullReach));
        driftRoseBoost.update(audio, millis(), midrangeBoostActive(audio));
        const float lift = driftRoseBoost.lift * midrangeBoostGain() * midrangeMultiplier;
        const float drive = min(1.0f, float(audio.volumeSm) / fullVolume + lift);
        // VolumeSm already has attack and release smoothing. Apply it to
        // the geometry on every draw so the rose can follow individual swells.
        const float reach = quietReach + (1.0f - quietReach) * drive;
        char detail[80];
        snprintf(detail, sizeof(detail), "Drift Rose curve=raw reach=%.0f%% fullVol=%d fft=%.3f",
                 double(reach * 100.0f), fullVolume, double(lift));
        Audio::debug(audio, detail);
        return reach;
    }
}

void Audio::animateJulia() {
    juliaBoost = MidrangeBoost{};
    // Volume replaces the timed open/close oscillator. Small contour motion
    // and the approved 4s hold / 2s hue transitions continue independently.
    JuliaRenderer::animateJulia(0, controls().juliaStepMs, true, true,
        juliaAudioClosure, TestDemo::audioRoutineRunning,
        controls().juliaAttackMs, controls().juliaReleaseMs);
}

void Audio::animateDriftRose() {
    constexpr int volume = RNDR_X * RNDR_Y * RNDR_Z;
    // 80 spokes on the 16-cube. Keep enough to read as a rose on a smaller cube.
    constexpr int spokeCount = (80 * volume + 2048) / 4096;
    driftRoseBoost = MidrangeBoost{};
    Streamers::animateDriftRose3D(0, RainbowColors_p, 200,
        driftRoseReach, TestDemo::audioRoutineRunning, max(8, spokeCount));
}

// Plasmoid by Andrew Tuline, adapted from WLED v16.0.1. Two independently
// drifting strip waves set the fill and brightness. Their phases also color
// the strip, independently of the volume cutoff.
void Audio::animatePlasmoidMirrored() {
    constexpr uint32_t stepMs = 23; // WLED's default 42 Hz effect clock.
    constexpr unsigned sliceSize = unsigned(RNDR_X) * RNDR_Z;
    constexpr unsigned sliceShift = RNDR_X + 1;
    constexpr unsigned voxelCount = sliceSize * RNDR_Y;
    constexpr uint8_t idleNoise = 2;
    const CRGBPalette16 palette = RainbowColors_p;
    uint8_t cubicPhase = 0;
    uint8_t cosinePhase = 0;
    const uint32_t started = millis();
    uint32_t lastStep = started - stepMs;
    clearAll();

    while (TestDemo::audioRoutineRunning()) {
        const uint32_t now = millis();
        const uint32_t sinceStep = now - lastStep;
        if (sinceStep < stepMs) {
            showCube();
            continue;
        }
        lastStep = sinceStep >= 2 * stepMs ? now : lastStep + stepMs;
        const auto audio = RemoteInputs::audio();
        // Silence keeps the same moving pattern, sparse and faint, so the cube
        // never looks dead. Music takes over as soon as it clears the floor.
        constexpr unsigned idleCutoff = 40;
        constexpr uint8_t idleBrightness = 70;
        const bool idle = audio.volumeSm <= idleNoise;
        const unsigned cutoff = idle ? idleCutoff :
            max(idleCutoff, min(254u, unsigned(audio.volumeSm) * Audio::controls().plasmoidIntensity / 64));
        const auto phaseStep = [now, started](unsigned cyclesPerMinute) {
            const uint32_t elapsed = (now - started) % 60000u;
            const uint8_t angle = uint8_t(elapsed * cyclesPerMinute * 256u / 60000u);
            return int(unsigned(sin8(angle)) * 9 / 256) - 4;
        };
        cubicPhase = uint8_t(int(cubicPhase) + phaseStep(6));
        cosinePhase = uint8_t(int(cosinePhase) + phaseStep(7));
        fadeAll(32);

        unsigned refreshed = 0;
        if (cutoff > 0) {
            // Both waves wrap at 256, so evaluate one period for the whole cube.
            CRGB light[256];
            const unsigned brightnessFloor = min(48u, cutoff);
            for (unsigned i = 0; i < 256; ++i) {
                // Spatial frequencies at WLED's default speed setting of 128.
                const uint8_t cubicAngle = uint8_t(13 * i + cubicPhase);
                const uint8_t cosineAngle = uint8_t(117 * i + cosinePhase);
                const uint8_t score = cubicwave8(cubicAngle) / 2 +
                                      sin8(uint8_t(cosineAngle + 64)) / 2;
                if (score == 0 || score > cutoff) {
                    light[i] = CRGB::Black;
                    continue;
                }
                // The phase mix advances by an odd stride, visiting all 256
                // palette positions per strip period before the mask is applied.
                const uint8_t paletteIndex = uint8_t(cubicAngle + 2u * cosineAngle);
                light[i] = ColorFromPalette(palette, paletteIndex, 255, LINEARBLEND);
                light[i].nscale8(uint8_t(score + (255u - score) * brightnessFloor / 255u));
                if (idle) light[i].nscale8(idleBrightness);
            }

            for (uint8_t y = 0; y < RNDR_Y; ++y) {
                const unsigned depth = min(unsigned(y), unsigned(RNDR_Y - 1 - y));
                // Read each upright slice as a matrix, with width first. Rotate
                // its strip starting point so a 16x16 slice doesn't repeat at
                // every depth just because it is exactly one period long.
                unsigned position = depth * sliceShift % sliceSize;
                for (uint8_t z = 0; z < RNDR_Z; ++z) {
                    for (uint8_t x = 0; x < RNDR_X; ++x) {
                        const CRGB &incoming = light[uint8_t(depth * sliceSize + position)];
                        if (++position == sliceSize) {
                            position = 0;
                        }
                        if (incoming.r || incoming.g || incoming.b) {
                            // Fresh color stays saturated. Only pixels that
                            // leave the mask keep fading their previous color.
                            setVoxel(x, y, z, incoming);
                            ++refreshed;
                        }
                    }
                }
            }
        }

        char detail[104];
        snprintf(detail, sizeof(detail), "Plasmoid map=depth-mirror cutoff=%u fresh=%u/%u",
                 cutoff, refreshed, voxelCount);
        Audio::debug(audio, detail);
        showCube();
    }
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

void Audio::debug(const RemoteInputs::AudioFrame &audio, const char *detail, uint32_t attackNumber,
                  const uint8_t* displayBins) {
#if AUDIO_SERIAL_DEBUG
    // Render task only. No printing in UARTReceiver and no lock held during TX.
    // Static storage survives a nonlocal app exit; no resource-owning locals.
    static char text[640];
    static size_t queued = 0;
    static size_t sent = 0;
    static uint32_t lastReport = 0;
    const uint32_t now = millis();

    if (sent == queued) {
        sent = 0;
        queued = 0;
    }
    if (queued == 0 && uint32_t(now - lastReport) >= 500) {
        lastReport = now;
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
            "BPM=%.1f Bass=%u Mid=%u High=%u RXok=%lu RXbad=%lu Hold=%u %s\r\n",
            source, TestDemo::currentAudioRoutineName(), unsigned(control.linked),
            unsigned(audio.volumeRaw), unsigned(audio.volumeSm), unsigned(audio.volumeAvg),
            double(audio.bpm), unsigned(audio.bassKick), unsigned(audio.midAttack),
            unsigned(audio.highAttack), (unsigned long)stats.accepted,
            (unsigned long)stats.rejected, unsigned(TestDemo::playbackHeld()), detail);
        if (n <= 0 || size_t(n) >= sizeof(text)) {
            return;
        }
        queued = size_t(n);

        const auto appendBins = [&](const char* name, const uint8_t* bins) {
            for (unsigned i = 0; i < 16; ++i) {
                const int length = i == 0
                    ? snprintf(text + queued, sizeof(text) - queued, "%s:[%u ", name, unsigned(bins[i]))
                    : snprintf(text + queued, sizeof(text) - queued, i == 15 ? "%u]\r\n" : "%u ", unsigned(bins[i]));
                if (length <= 0 || size_t(length) >= sizeof(text) - queued) return false;
                queued += size_t(length);
            }
            return true;
        };
        if (!appendBins("Bins", audio.fftResult) || (displayBins && !appendBins("DisplayBins", displayBins))) {
            queued = 0;
            return;
        }
    }

    if (attackNumber != 0) {
        const auto event = RemoteInputs::audioAttackDiagnostics();
        char line[144];
        const int length = snprintf(line, sizeof(line),
            "Attack: ms=%lu hit=%lu VSm=%u kick=%u accepted=%u/%u/%u strength=%u/%u/%u\r\n",
            (unsigned long)now, (unsigned long)attackNumber, unsigned(audio.volumeSm),
            unsigned(event.dedicatedKick), unsigned(audio.bassKick), unsigned(audio.midAttack),
            unsigned(audio.highAttack), unsigned(event.bassStrength), unsigned(event.midStrength),
            unsigned(event.highStrength));
        if (length > 0 && size_t(length) < sizeof(line)) {
            // Keep pending bytes in order even when a hit arrives mid-report.
            // If the monitor cannot keep up, gaps in hit numbers show it.
            if (queued + size_t(length) > sizeof(text) && sent > 0) {
                memmove(text, text + sent, queued - sent);
                queued -= sent;
                sent = 0;
            }
            if (queued + size_t(length) <= sizeof(text)) {
                memcpy(text + queued, line, length);
                queued += size_t(length);
            }
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
    puddles(true, PuddleShape(random(3)), controls().peakAttackSensitivity,
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

        const bool hit = audio.hasAttack();
        const bool volumeReady = audio.volumeSm * sensitivity >= 2.0f;
        const bool intervalReady = uint32_t(now - lastSpawn) >= PUDDLE_SPAWN_INTERVAL_MS;
        const bool spawn = peakDetect ? hit && peakDensity > 0.0f : volumeReady && intervalReady;

        if (spawn && sensitivity > 0.0f) {
            lastSpawn = now;

            // Peak uses a separate size gain because its sensitivity controls
            // event admission. Apply the size gain before voxel quantization.
            const float response = peakDetect ? 1.25f : sensitivity;
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
                    GeometryEngine::rasterFastBox(x0, y0, z0, x0 + edge - 1, y0 + edge - 1, z0 + edge - 1,
                                                  RENDER_SOLID, [&](uint8_t x, uint8_t y, uint8_t z) {
                        setVoxel(x, y, z, color);
                    });
                } else {
                    // Z is vertical; X is the existing horizontal line.
                    const uint8_t x = random(vertical ? RNDR_X : RNDR_X - length + 1);
                    const uint8_t y = random(RNDR_Y);
                    const uint8_t z = random(vertical ? RNDR_Z - length + 1 : RNDR_Z);
                    GeometryEngine::rasterFastBox(x, y, z, x + (vertical ? 0 : length - 1), y,
                                                  z + (vertical ? length - 1 : 0),
                                                  RENDER_SOLID, [&](uint8_t ix, uint8_t iy, uint8_t iz) {
                        setVoxel(ix, iy, iz, color);
                    });
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
        if (audio.hasAttack()) {
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

void Audio::bouncingBalls(BouncingBallsPattern pattern) {
    constexpr int columnsOfSections = 4;
    constexpr int sectionCount = 2 * columnsOfSections;
    constexpr float radiansPerBeat = 6.283185307f / sectionCount;
    constexpr uint32_t whiteAccentMs = 90;
    constexpr float fullHeight = RNDR_Z - 1.0f;
    constexpr float fullFlightMs = 800.0f;
    constexpr float reboundSpeed = 0.65f;
    constexpr float reboundHeight = reboundSpeed * reboundSpeed;
    const CRGBPalette16 palette = RainbowColors_p;
    // The grids are top views. Tiles with the same number launch together.
    // Entry order matches BouncingBallsPattern; the original uses two rows.
    struct Pattern {
        const char* name;
        int rows;
        const uint8_t* launchBeat;
    };
    constexpr uint8_t clockwiseSteps[] = {1, 2, 3, 4, 8, 7, 6, 5};
    uint8_t inwardSteps[16];
    for (int i = 0; i < 16; ++i) inwardSteps[AudioLayouts::orbitInwardTiles[i]] = 1 + i / 2;
    const Pattern patterns[] = {
        {"Clockwise", 2, clockwiseSteps},
        {"Orbit Inward", 4, inwardSteps},
    };
    const auto& layout = patterns[static_cast<unsigned>(pattern)];
    const int tileCount = columnsOfSections * layout.rows;
    struct Hop {
        uint32_t startMs;
        float height;
        float durationMs;
    };
    Hop hops[sectionCount] = {};
    uint8_t heightPercent[RNDR_X * RNDR_Y];
    int lastBeat = -1;
    int nextSection = 0;
    int lastSection = -1;
    float lastHeight = 0.0f;

    while (TestDemo::audioRoutineRunning()) {
        const auto audio = RemoteInputs::audio();
        const uint32_t now = millis();
        const int beat = min(sectionCount - 1, int(audio.beatAngle(sectionCount) / radiansPerBeat));
        int launchedSection = -1;
        if (lastBeat < 0 || audio.volumeSm <= 2) {
            lastBeat = beat;
        } else {
            // Small backwards phase corrections near a beat must not fire it twice.
            const int forward = (beat - lastBeat + sectionCount) % sectionCount;
            if (forward > 0 && forward <= sectionCount / 2) {
                lastBeat = beat;
                const int fullVolume = max(1, int(controls().bouncingBallsVolumeForFullHeight));
                const float drive = min(1.0f, float(audio.volumeSm) / fullVolume);
                lastHeight = min(2.0f, fullHeight) + max(0.0f, fullHeight - 2.0f) * drive;
                lastSection = nextSection;
                launchedSection = nextSection;
                hops[nextSection] = {now, lastHeight, fullFlightMs * sqrtf(lastHeight / fullHeight)};
                nextSection = (nextSection + 1) % sectionCount;
            }
        }

        clearAll();
        bool bouncing[sectionCount] = {};
        for (int tile = 0; tile < tileCount; ++tile) {
            const int section = layout.launchBeat[tile] - 1;
            const auto& hop = hops[section];
            const uint32_t age = now - hop.startMs;
            // Give all 16 tiles fixed rainbow hues. Opposite tiles sit half
            // a palette cycle apart, so the Orbit pair has a clear color split.
            const int hueSlot = layout.rows == 2 ? section : (tile < 8 ? tile : 23 - tile);
            const CRGB sectionColor = ColorFromPalette(palette, uint8_t(hueSlot * 256 / tileCount), 255, LINEARBLEND);
            const int row = tile / columnsOfSections;
            const int column = tile % columnsOfSections;
            const int xStart = column * RNDR_X / columnsOfSections;
            const int xEnd = (column + 1) * RNDR_X / columnsOfSections;
            const int yStart = row * RNDR_Y / layout.rows;
            const int yEnd = (row + 1) * RNDR_Y / layout.rows;
            for (int y = yStart; y < yEnd; ++y) {
                for (int x = xStart; x < xEnd; ++x) {
                    auto& heightPercentForBall = heightPercent[x + RNDR_X * y];
                    if (section == launchedSection) heightPercentForBall = 85 + random(16);
                    int z = 0;
                    bool inFlight = false;
                    if (hop.height > 0.0f) {
                        const float scale = heightPercentForBall / 100.0f;
                        float height = hop.height * scale;
                        float flight = hop.durationMs * sqrtf(scale);
                        float elapsed = age;
                        // Follow successive arcs at constant gravity. On each
                        // landing, reduce speed and let sub-voxel hops settle.
                        while (height >= 1.0f && elapsed >= flight) {
                            elapsed -= flight;
                            height *= reboundHeight;
                            flight *= reboundSpeed;
                        }
                        if (height >= 1.0f) {
                            const float progress = elapsed / flight;
                            z = lroundf(4.0f * height * progress * (1.0f - progress));
                            inFlight = true;
                        }
                    }
                    bouncing[section] |= inFlight;
                    CRGB color = sectionColor;
                    if (!inFlight) {
                        color.nscale8(36);
                    } else if (age < whiteAccentMs) {
                        const uint8_t white = 255 * (whiteAccentMs - age) / whiteAccentMs;
                        color.nscale8(255 - white);
                        color += CRGB(white, white, white);
                    }
                    setVoxel(x, y, z, color);
                }
            }
        }
        // Both tiles in a pair share a launch. Keep the hop alive until
        // the last ball in either tile has settled.
        for (int section = 0; section < sectionCount; ++section) {
            if (!bouncing[section]) hops[section].height = 0.0f;
        }
        char detail[112];
        snprintf(detail, sizeof(detail), "Bouncing Balls pattern=%s step=%d next=%d hopHeight=%.1f",
                 layout.name, lastSection + 1, nextSection + 1, double(lastHeight));
        Audio::debug(audio, detail);
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

namespace {
// DJ-style three-band color (Rekordbox-like): deep blue for lows, amber for
// mids, white for highs, mixed by each group's share of the display level.
// Squaring the shares keeps the mix vivid instead of drifting to grey.
CRGB threeBandBlend(const uint8_t (&bins)[16], float (&share)[3], float retained) {
    const float sums[3] = {
        float(bins[0]) + bins[1] + bins[2] + bins[3],
        float(bins[4]) + bins[5] + bins[6] + bins[7] + bins[8] + bins[9],
        float(bins[10]) + bins[11] + bins[12] + bins[13] + bins[14] + bins[15],
    };
    const float means[3] = {sums[0] / 4.0f, sums[1] / 6.0f, sums[2] / 6.0f};
    const float squares[3] = {means[0] * means[0], means[1] * means[1], means[2] * means[2]};
    const float total = squares[0] + squares[1] + squares[2];
    if (total > 1.0f) {
        for (int i = 0; i < 3; ++i) {
            share[i] = squares[i] / total + (share[i] - squares[i] / total) * retained;
        }
    }
    const CRGB anchors[3] = {CRGB(0x0830FF), CRGB(0xFF9000), CRGB(0xFFFFFF)};
    float r = 0, g = 0, b = 0;
    for (int i = 0; i < 3; ++i) {
        r += anchors[i].r * share[i];
        g += anchors[i].g * share[i];
        b += anchors[i].b * share[i];
    }
    return CRGB(uint8_t(min(255.0f, r)), uint8_t(min(255.0f, g)), uint8_t(min(255.0f, b)));
}
}

void Audio::bpm(BpmSpatialMode mode, float sensitivity, bool soundColor) {
    constexpr uint16_t barBeats = 4;
    float share[3] = {1.0f, 0.0f, 0.0f}; // Silence starts deep blue.
    uint32_t lastFrame = millis();
    sensitivity = std::isfinite(sensitivity) ? max(0.0f, sensitivity) : 1.0f;

    // Every spatial mode uses the shared musical clock. Volume changes
    // brightness only; it cannot accelerate or reset the breathing cycle.
    uint8_t *phaseMap = reinterpret_cast<uint8_t *>(SharedAppMemory);
    BpmRenderer::prepare(phaseMap, mode);

    while (TestDemo::audioRoutineRunning()) {
        const auto audio = RemoteInputs::audio();
        if (!soundColor) Audio::debug(audio); // B logs its color below.

        // Checkerboard has an adjustable beats-per-breath setting. Use the same
        // period for brightness and spatial travel so both stay in step.
        // This divides the shared clock without changing the BPM estimate.
        const uint16_t beatsPerCycle = mode == BPM_CHECKERBOARD ?
            max(uint16_t(1), controls().checkerBeatsPerCycle) : 1;
        // beatPulse is zero on the beat, so 1 - pulse peaks ON each beat.
        const float onBeat = 1.0f - audio.beatPulse(beatsPerCycle);
        const float cycle = audio.beatAngle(beatsPerCycle) / 6.283185307f;
        const uint8_t beat = uint8_t(64.0f + onBeat * 191.0f);
        const uint8_t travel = uint8_t(cycle * 256.0f);

        // Volume remains an artistic brightness control. Zero/stale volume
        // keeps the familiar dim idle breath. Timing does not rescale volume.
        const float volume = min(255.0f, audio.volumeSm * sensitivity);
        const bool music = volume > 1.0f;
        // With music, breathe once per four beats (a bar-length breath; the
        // clock has no bar origin) and add a short accent on every beat, so
        // breathing reads as breathing and the kick still lands. Idle keeps
        // the slow single-beat breath of the 12 BPM idle clock.
        float breath = onBeat;
        if (music) {
            const float slow = 1.0f - audio.beatPulse(barBeats);
            const float accent = onBeat * onBeat * onBeat * onBeat;
            breath = 0.7f * slow + 0.3f * accent;
        }
        const float amplitude = music ? 64.0f + volume * 0.6f : 48.0f;
        const uint8_t level = uint8_t(min(255.0f, 32.0f + amplitude * breath));
        if (soundColor) {
            // Audition B: the color says what is sounding. A gentle hue
            // spread around the blend keeps the pattern's texture readable.
            uint8_t bins[16];
            displaySpectrum(audio, bins);
            const uint32_t now = millis();
            const float retained = expf(-float(uint32_t(now - lastFrame)) / 300.0f);
            lastFrame = now;
            const CRGB blend = threeBandBlend(bins, share, retained);
            CRGB edge = blend;
            edge.nscale8(170);
            const CRGBPalette16 palette(edge, blend, blend, edge);
            BpmRenderer::draw(phaseMap, palette, uint8_t(beat - travel), uint8_t(now / 40), level);
            char detail[96];
            snprintf(detail, sizeof(detail), "Breathe color share=%.2f/%.2f/%.2f rgb=%02X%02X%02X",
                     double(share[0]), double(share[1]), double(share[2]), blend.r, blend.g, blend.b);
            Audio::debug(audio, detail);
        } else {
            BpmRenderer::draw(phaseMap, RainbowColors_p, uint8_t(beat - travel),
                              uint8_t(millis() / 40), level);
        }

        showCube();
    }
}

// Waverly-inspired geometry, based on Stepko / Andrew Tuline's WLED
// v16.0.1 mode_2DWaverly. This is a dimension-aware volumetric adaptation:
// noise has TWO spatial coordinates and one time coordinate; Z is height.
// Coloring is deliberately independent so audition entries share the exact
// same geometry and noise trajectory. The FFT lift is independent of color.
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
    MidrangeBoost boost;
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

        const bool fftEnabled = midrangeBoostActive(audio);
        uint8_t bins[16];
        const bool compensated = displaySpectrum(audio, bins);
        boost.update(audio, now, fftEnabled, bins);
        const float extraReach = fftEnabled ? boost.lift * midrangeBoostGain() * float(RNDR_Z) * 0.9f : attackReach;

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
                    const float height = max(1.0f,
                        min(float(RNDR_Z), noise * (volume * gain * maximumReach + extraReach)));
                    tallest = max(tallest, height);
                    for (uint8_t z = 0; z < RNDR_Z; ++z) {
                        // Fractional tips avoid whole layers popping on/off.
                        // The two outer planes remain fully covered at idle.
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
                 "Waverly curve=%s height=%.1f/%u gain=%.2f step=%lums color=%s mid=%u high=%u push=%.2f fft=%u gate=%.2f bin=%u rise=%.1f",
                 compensated ? "comp" : "raw", double(tallest), unsigned(RNDR_Z), double(gain),
                 (unsigned long)stepMs, columnGradient ? "gradient-cycle" : "moving@1x",
                 unsigned(audio.midAttack), unsigned(audio.highAttack), double(extraReach), unsigned(fftEnabled),
                 double(boost.gate), boost.strongestBand, double(boost.strongestRise));
        Audio::debug(audio, detail, 0, bins);
        // Keep servicing buttons/UART even between slower geometry updates.
        // showCube() supplies the existing 60 Hz limiter; no blocking sleep.
        showCube();
    }
}

namespace {
    constexpr int surroundGroups = 8;
    // The lowest four bins belong to the Surround GEQ sphere.
    constexpr uint8_t surroundEdges[] = {4, 5, 6, 7, 8, 10, 12, 14, 16};

    int surroundGroupAt(int x, int y, int thickness) {
        // Reverse alternate faces so both faces agree at their shared corner.
        if (y < thickness) return x * surroundGroups / RNDR_X;
        if (x >= RNDR_X - thickness) return (RNDR_Y - 1 - y) * surroundGroups / RNDR_Y;
        if (y >= RNDR_Y - thickness) return (RNDR_X - 1 - x) * surroundGroups / RNDR_X;
        if (x < thickness) return y * surroundGroups / RNDR_Y;
        return -1;
    }
}

void Audio::animateFunkyPlank() {
    constexpr uint32_t rowPeriodMs = 64;
    // Lower bounds. OLD keeps the range seen in the original ADC log. NEW
    // (corrected spectrum) uses the reference-song spread so every color is
    // used and the top one is rare (about 10/15/15/15/15/12/10/8 percent).
    // Provisional until the third audition song is in.
    constexpr uint8_t oldBucketStarts[] = {7, 12, 20, 28, 36, 44, 52};
    constexpr uint8_t newBucketStarts[] = {22, 31, 38, 44, 51, 61, 78};
    // Stronger reads brighter: deep indigo up through warm white, so a loud
    // moment brightens the plank and kicks scroll down as bright stripes.
    // Classic red sits in the middle (owner: the earlier magenta read pink).
    const CRGB colors[] = {
        CRGB::Black, CRGB(0x100060), CRGB(0x0030FF), CRGB(0x8000FF),
        CRGB(0xFF0000), CRGB(0xFF4000), CRGB(0xFF9000), CRGB(0xFFFFA0)
    };
    constexpr char bucketNames[] = "-IBVROAW";
    CRGB history[RNDR_Z][surroundGroups] = {};
    int newest = 0;
    uint32_t lastRowMs = millis() - rowPeriodMs;
    clearAll();
    while (TestDemo::audioRoutineRunning()) {
        const uint32_t now = millis();
        if (uint32_t(now - lastRowMs) >= rowPeriodMs) {
            lastRowMs = now;
            const auto audio = RemoteInputs::audio();
            uint8_t bins[16];
            const bool compensated = displaySpectrum(audio, bins);
            newest = (newest + 1) % RNDR_Z;
            unsigned levels[surroundGroups];
            char buckets[surroundGroups + 1] = {};
            for (int group = 0; group < surroundGroups; ++group) {
                const uint8_t level = max(bins[2 * group], bins[2 * group + 1]);
                unsigned bucket = 0;
                const auto &bucketStarts = audio.spectrumCorrected ? newBucketStarts : oldBucketStarts;
                for (const auto start : bucketStarts) {
                    if (level < start) break;
                    ++bucket;
                }
                history[newest][group] = colors[bucket];
                levels[group] = level;
                buckets[group] = bucketNames[bucket];
            }
            for (int y = 0; y < RNDR_Y; ++y) {
                for (int x = 0; x < RNDR_X; ++x) {
                    const int group = surroundGroupAt(x, y, 1);
                    if (group < 0) continue;
                    for (int age = 0; age < RNDR_Z; ++age) {
                        setVoxel(x, y, RNDR_Z - 1 - age, history[(newest + RNDR_Z - age) % RNDR_Z][group]);
                    }
                }
            }
            char detail[128];
            snprintf(detail, sizeof(detail),
                     "Funky Plank curve=%s scroll=%lums levels=%u/%u/%u/%u/%u/%u/%u/%u colors=%s",
                     compensated ? "comp" : "raw", (unsigned long)rowPeriodMs,
                     levels[0], levels[1], levels[2], levels[3], levels[4], levels[5], levels[6], levels[7],
                     buckets);
            debug(audio, detail, 0, bins);
        }
        showCube();
    }
}

namespace {
float frequencyShapeRelease(uint32_t &lastFrame, float releaseMs = 25.0f) {
    const uint32_t now = millis();
    const uint32_t elapsed = now - lastFrame;
    lastFrame = now;
    return expf(-float(elapsed) / releaseMs);
}

}

float Audio::trackShapeSize(float previous, float target, float retained) {
    // Rise immediately. On a drop, smooth the size and redraw the boundary.
    if (target >= previous) return target;
    const float level = target + (previous - target) * retained;
    return level - target < 1.0f ? target : level;
}

void Audio::animateFrequencyPlanes() {
    frequencyPlanes(false);
}

void Audio::frequencyPlanes(bool discs, bool lightness) {
    constexpr int bands = 8;
    constexpr int noiseFloor = 6;
    CRGB colors[bands] = {
        CRGB(0xFF0000), CRGB(0xFF8000), CRGB(0xFFFF00), CRGB(0x00FF00),
        CRGB(0x0000FF), CRGB(0x4B0082), CRGB(0x9400D3), CRGB::White
    };
    if (lightness) {
        // Audition B, rainbow plus lightness: the rainbow hue stays, and
        // whiteness rises with pitch at full brightness. Lows are deep and
        // saturated, highs pale and pastel; dimness still means quiet.
        for (int band = 0; band < bands; ++band) {
            colors[band] = CHSV(uint8_t(band * 32), uint8_t(255 - band * 22), 255);
        }
    }
    const int fullAreaLevel = max(1, int(controls().frequencyPlanesLevelForFullArea));
    float visibleLevels[bands] = {};
    uint32_t lastFrame = millis();
    while (TestDemo::audioRoutineRunning()) {
        const auto audio = RemoteInputs::audio();
        uint8_t bins[16];
        const bool compensated = displaySpectrum(audio, bins);
        int levels[bands] = {};
        const float retained = frequencyShapeRelease(lastFrame);
        clearAll();
        for (int band = 0; band < bands; ++band) {
            for (int bin = 2 * band; bin < 2 * band + 2; ++bin) {
                if (audio.fftResult[bin] > noiseFloor) levels[band] = max(levels[band], int(bins[bin]));
            }
            visibleLevels[band] = trackShapeSize(visibleLevels[band], min(levels[band], fullAreaLevel), retained);
            const int upper = RNDR_Z / 2 + band;
            const int lower = RNDR_Z - 1 - upper;
            if (visibleLevels[band] == 0.0f) {
                // Silence keeps a dim center dot on every band's layer so the
                // cube never looks dead; music grows the shape from here.
                CRGB dot = colors[band];
                dot.nscale8(48);
                for (int y = (RNDR_Y - 1) / 2; y <= RNDR_Y / 2; ++y) {
                    for (int x = (RNDR_X - 1) / 2; x <= RNDR_X / 2; ++x) {
                        setVoxel(x, y, upper, dot);
                        setVoxel(x, y, lower, dot);
                    }
                }
                continue;
            }
            // Width and depth grow with the square root so lit area follows
            // the band level. Fractional edge coverage softens size changes.
            const float extent = sqrtf(visibleLevels[band] / fullAreaLevel);
            const float halfWidth = RNDR_X * 0.5f * extent;
            const float halfDepth = RNDR_Y * 0.5f * extent;
            GeometryEngine::rasterHorizontalPatch(halfWidth, halfDepth, discs,
                                                   [&](int x, int y, uint8_t coverage) {
                CRGB color = colors[band];
                color.nscale8(coverage);
                setVoxel(x, y, upper, color);
                setVoxel(x, y, lower, color);
            });
        }
        char detail[120];
        snprintf(detail, sizeof(detail), lightness ? "Frequency Planes lightness curve=%s levels=%d/%d/%d/%d/%d/%d/%d/%d full=%d" : "Frequency Planes curve=%s levels=%d/%d/%d/%d/%d/%d/%d/%d full=%d",
                 compensated ? "comp" : "raw", levels[0], levels[1], levels[2], levels[3],
                 levels[4], levels[5], levels[6], levels[7], fullAreaLevel);
        debug(audio, detail, 0, bins);
        showCube();
    }
}

void Audio::frequencyCubes() {
    constexpr int noiseFloor = 6;
    constexpr uint32_t rainbowEight[] = {
        0xFF0000, 0xFF8000, 0xFFFF00, 0x00FF00,
        0x0000FF, 0x4B0082, 0x9400D3, 0xFFFFFF
    };
    constexpr int grid = 2;
    constexpr int bandCount = 8;
    constexpr int binsPerBand = 2;
    const int fullSize = max(noiseFloor + 1, int(controls().frequencyShapesLevelForFullSize));
    // Viewed from above with +Y toward the back: front-left, back-left,
    // back-right, front-right.
    constexpr int quadrantOrder[grid][grid] = {{0, 3}, {1, 2}};
    float visibleLevels[bandCount] = {};
    uint32_t lastFrame = millis();
    while (TestDemo::audioRoutineRunning()) {
        const auto audio = RemoteInputs::audio();
        uint8_t bins[16];
        const bool compensated = displaySpectrum(audio, bins);
        const float retained = frequencyShapeRelease(lastFrame, cubeSizeReleaseMs);
        int levels[bandCount] = {};
        int active = 0;
        for (int band = 0; band < bandCount; ++band) {
            for (int offset = 0; offset < binsPerBand; ++offset) {
                const int bin = band * binsPerBand + offset;
                if (audio.fftResult[bin] > noiseFloor) levels[band] = max(levels[band], int(bins[bin]));
            }
            visibleLevels[band] = trackShapeSize(visibleLevels[band], min(levels[band], fullSize), retained);
            if (visibleLevels[band] > 0.0f) ++active;
        }
        clearAll();
        for (int gz = 0; gz < grid; ++gz) {
            const int z0 = gz * RNDR_Z / grid, z1 = (gz + 1) * RNDR_Z / grid;
            for (int gy = 0; gy < grid; ++gy) {
                const int y0 = gy * RNDR_Y / grid, y1 = (gy + 1) * RNDR_Y / grid;
                for (int gx = 0; gx < grid; ++gx) {
                    const int x0 = gx * RNDR_X / grid, x1 = (gx + 1) * RNDR_X / grid;
                    const int band = 4 * gz + quadrantOrder[gy][gx];
                    const int maxEdge = min(x1 - x0, min(y1 - y0, z1 - z0));
                    const CRGB color(rainbowEight[band]);
                    const float level = max(0.0f, visibleLevels[band] - 1.0f) / (fullSize - 1);
                    const float edge = 1.0f + (maxEdge - 1) * level;
                    // Anchor the cube at the corner closest to the display center.
                    GeometryEngine::drawCornerCube(gx == 0 ? x1 - 1 : x0, gy == 0 ? y1 - 1 : y0,
                                                   gz == 0 ? z1 - 1 : z0,
                                                   gx == 0, gy == 0, gz == 0, edge, color);
                }
            }
        }
        char detail[120];
        snprintf(detail, sizeof(detail), "Frequency Shapes curve=%s active=%d levels=%d/%d/%d/%d/%d/%d/%d/%d full=%d",
                 compensated ? "comp" : "raw", active, levels[0], levels[1], levels[2], levels[3],
                 levels[4], levels[5], levels[6], levels[7], fullSize);
        debug(audio, detail, 0, bins);
        showCube();
    }
}

void Audio::freqwave(FreqwaveShape shape) {
    // Launch on the beat. Fronts advance with elapsed
    // time so beat phase corrections cannot move a front already in flight.
    constexpr int quietVolume = 2;
    const float halfSpan = min(RNDR_CX, min(RNDR_CY, RNDR_CZ));
    const float smoothVoxelsPerMs = 0.008f * halfSpan / 7.5f;
    constexpr float edgeWidth = 0.95f;
    const float sphereExit = 8.0f * halfSpan / 7.5f;
    constexpr int historySize = 16;
    constexpr float radiansPerBeat = 6.283185307f / historySize;
    CRGB history[historySize] = {};
    uint32_t emittedAt[historySize] = {};
    const bool wireframe = shape == FreqwaveShape::WireframeBox;
    // On the narrower canvas both horizontal bounds clip together. Beyond
    // them there are no visible edges, even while the vertical bounds fit.
    const float exitRadius = wireframe ? halfSpan + 0.5f : sphereExit;
    int lastBeat = -1;
    unsigned newest = 0;
    clearAll();
    while (TestDemo::audioRoutineRunning()) {
        const uint32_t now = millis();
        const auto audio = RemoteInputs::audio();
        const float beatPosition = audio.beatAngle(historySize) / radiansPerBeat;
        const int beat = min(historySize - 1, int(beatPosition));
        if (lastBeat < 0) lastBeat = beat;
        const int steps = (beat - lastBeat + historySize) % historySize;
        // A phase correction can briefly cross a beat backwards. Hold the
        // last frame's beat boundary until the clock catches up.
        if (steps > 0 && steps < historySize / 2) {
            lastBeat = beat;
            newest = (newest + steps) % historySize;
            for (int age = 1; age < steps; ++age) {
                history[(newest + historySize - age) % historySize] = CRGB::Black;
            }
            uint8_t bins[16];
            const bool compensated = displaySpectrum(audio, bins);
            int band = -1;
            for (int i = 0; i < 16; ++i) {
                // Compensation should never promote the quiet ADC noise floor.
                if (audio.fftResult[i] > 6 && bins[i] > 0 && (band < 0 || bins[i] > bins[band])) band = i;
            }
            const uint8_t brightness = band >= 0 && audio.volumeSm > quietVolume ? 255 : 0;
            history[newest] = band < 0 ? CRGB::Black : CRGB(CHSV(uint8_t(band * 208 / 15), 255, brightness));
            emittedAt[newest] = now;
            char detail[88];
            snprintf(detail, sizeof(detail), "Freqwave curve=%s band=%d level=%u brightness=%u beat=%d",
                     compensated ? "comp" : "raw", band, band < 0 ? 0u : unsigned(bins[band]),
                     unsigned(brightness), beat);
            debug(audio, detail, 0, bins);
        }

        struct Front {
            float radius;
            CRGB color;
        };
        Front fronts[historySize] = {};
        int frontCount = 0;
        for (int age = 0; age < historySize; ++age) {
            const unsigned index = (newest + historySize - age) % historySize;
            CRGB color = history[index];
            if (!(color.r || color.g || color.b)) continue;
            const float radius = (now - emittedAt[index]) * smoothVoxelsPerMs;
            if (radius >= exitRadius) {
                history[index] = CRGB::Black;
                continue;
            }
            // On the even-sized canvases, centered voxel layers are half
            // integers. Pick one layer so each edge has a single strand.
            const float drawRadius = wireframe ? floorf(radius) + 0.5f : radius;
            if (!wireframe) color.nscale8(uint8_t(255.0f * min(1.0f, (sphereExit - radius) / edgeWidth)));
            fronts[frontCount++] = {drawRadius, color};
        }
        clearAll();
        if (wireframe) {
            for (int i = 0; i < frontCount; ++i) {
                const auto &front = fronts[i];
                const float r = front.radius;
                GeometryEngine::rasterFastBox(RNDR_CX - r, RNDR_CY - r, RNDR_CZ - r,
                                              RNDR_CX + r, RNDR_CY + r, RNDR_CZ + r,
                                              RENDER_WIREFRAME, [&](uint8_t x, uint8_t y, uint8_t z) {
                    CRGB color = getVoxel(x, y, z);
                    color += front.color;
                    setVoxel(x, y, z, color);
                });
            }
            showCube();
            continue;
        }
        if (frontCount > 0) {
            const float outer = fronts[frontCount - 1].radius + 1.0f;
            GeometryEngine::rasterSphere({RNDR_CX, RNDR_CY, RNDR_CZ}, outer,
                                         [&](int x, int y, int z, float distanceSq) {
                const float distance = sqrtf(distanceSq);
                CRGB result = CRGB::Black;
                for (int i = 0; i < frontCount; ++i) {
                    const float coverage = 1.0f - fabsf(distance - fronts[i].radius);
                    if (coverage <= 0.0f) continue;
                    CRGB color = fronts[i].color;
                    color.nscale8(uint8_t(255.0f * coverage));
                    result += color;
                }
                if (result.r || result.g || result.b) setVoxel(x, y, z, result);
            });
        }
        showCube();
    }
}

void Audio::animateSurroundGEQ() {
    drawGeqBars(false);
}

void Audio::animateInwardGEQ() {
    drawGeqBars(true);
}

void Audio::drawGeqBars(bool inward) {
    const int groupCount = inward ? 16 : surroundGroups;
    const auto& edges = surroundEdges;
    const CRGB surroundColors[surroundGroups] = {
        CRGB(0xFF0000), CRGB(0xFF8000), CRGB(0xFFFF00), CRGB(0x00FF00),
        CRGB(0x0000FF), CRGB(0x4B0082), CRGB(0x9400D3), CRGB(0xFFFFFF)
    };
    CRGB colors[16];
    for (int group = 0; group < groupCount; ++group) {
        colors[group] = inward ? ColorFromPalette(RainbowColors_p, group * 16, 255, LINEARBLEND)
                               : surroundColors[group];
    }
    // One-deep walls on every canvas: two-deep walls hid the center bass
    // sphere from every side. The sphere keeps its original size limit (one
    // voxel of air inside the walls on the full cube).
    const int thickness = 1;
    const int sphereClearance = max(1, min(RNDR_X, RNDR_Y) / surroundGroups);
    const float maxRadius = min(RNDR_CX, RNDR_CY) - sphereClearance + 0.5f;
    const int maxBarHeight = RNDR_Z - 1; // Top row is available for the peak.
    // Anchor the rainbow to the cube as the bass shell moves.
    const TextureState texture(MODE_SPATIAL_GRADIENT, RainbowColors_p);
    float peaks[16] = {};
    uint32_t peakMs[16] = {};
    // All voxels at the same distance share coverage, so store one value
    // per half-voxel shell instead of keeping another cube buffer.
    constexpr int shellCount = RNDR_X + RNDR_Y;
    constexpr float recessionMs = 180.0f;
    float sphereCoverage[shellCount] = {};
    uint32_t lastMs = millis();

    while (TestDemo::audioRoutineRunning()) {
        const auto audio = RemoteInputs::audio();
        uint8_t displayBins[16];
        const bool compensated = displaySpectrum(audio, displayBins);
        const uint32_t now = millis();
        const uint32_t elapsedMs = uint32_t(now - lastMs);
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
        // Half a voxel of radius lights the center cluster on an even grid.
        const float radius = max(0.5f, maxRadius * min(1.0f, bass * bassGain / 255.0f));
        // Loudness lifts all bars together. Spectrum strength determines
        // how much of the remaining height each bar fills.
        const float baseline = RNDR_Z * (3.0f / 16.0f) * min(1.0f, audio.volumeSm * gain / 255.0f);
        const auto barStrength = [gain](float level) { return level * gain / 255.0f; };
        float bassBarScale = 1.0f;
        if (inward) {
            uint8_t strongestSharedBand = 0;
            for (int band = edges[0]; band < 16; ++band) {
                strongestSharedBand = max(strongestSharedBand, displayBins[band]);
            }
            // Surround routes bass to the sphere. Keep those extra columns
            // within the shared bars' range, with half-height headroom for
            // bass-only passages. Scale the four together to preserve detail.
            const float bassStrength = barStrength(bass);
            const float bassCeiling = max(0.5f, min(1.0f, barStrength(strongestSharedBand)));
            if (bassStrength > bassCeiling) bassBarScale = bassCeiling / bassStrength;
        }
        float heights[16];
        int peakRows[16];
        for (uint8_t group = 0; group < groupCount; ++group) {
            uint8_t level = 0;
            const int start = inward ? group : edges[group];
            const int end = inward ? group + 1 : edges[group + 1];
            for (int band = start; band < end; ++band) {
                level = max(level, displayBins[band]);
            }
            const float strength = barStrength(level) * (inward && group < edges[0] ? bassBarScale : 1.0f);
            const float height = baseline + (maxBarHeight - baseline) * min(1.0f, strength);
            // Keep one row lit without feeding the visual floor into peak tracking.
            heights[group] = max(1.0f, height);
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
        if (!inward) {
            const int radiusSteps = int(radius * 2.0f);
            const float fade = 255.0f * elapsedMs / recessionMs;
            int outerShell = -1;
            for (int shell = 0; shell < shellCount; ++shell) {
                const uint8_t target = GeometryEngine::getCoverage(shell - radiusSteps, 0, RENDER_SOLID);
                sphereCoverage[shell] = max(float(target), sphereCoverage[shell] - fade);
                if (sphereCoverage[shell] >= 1.0f) {
                    outerShell = shell;
                }
            }
            if (outerShell >= 0) {
                GeometryEngine::rasterSphere({RNDR_CX, RNDR_CY, RNDR_CZ}, (outerShell + 1) * 0.5f,
                                             [&](int x, int y, int z, float distanceSq) {
                    const int shell = GeometryEngine::sphereShell(distanceSq);
                    const uint8_t coverage = uint8_t(sphereCoverage[shell]);
                    if (coverage == 0) return;
                    CRGB color = texture.getColor(x, y, z);
                    color.nscale8(coverage);
                    setVoxel(x, y, z, color);
                });
            }
        }
        const auto drawBar = [&](int x, int y, int group) {
            const auto plot = [&](int z, CRGB color) {
                if (inward && (RNDR_X < 16 || RNDR_Y < 16)) color += getVoxel(x, y, z);
                setVoxel(x, y, z, color);
            };
            const float height = heights[group];
            for (int z = 0; z < RNDR_Z && z < height; ++z) {
                CRGB color = colors[group];
                if (z > 0) color.nscale8(uint8_t(192.0f * min(1.0f, height - z)));
                plot(z, color);
            }
            // Draw the peak cap above the bar body.
            const int peakRow = peakRows[group];
            if (peaks[group] >= 1.0f) {
                CRGB cap = colors[group];
                // Fade the cap in above the idle row so tiny peaks cannot flash.
                cap.nscale8(uint8_t(255.0f * min(1.0f, peaks[group] - 1.0f)));
                plot(peakRow, cap);
            }
        };
        if (inward) {
            for (int block = 0; block < 16; ++block) {
                for (int band = 0; band < 16; ++band) {
                    const int tile = AudioLayouts::orbitInwardTiles[band];
                    const int x = lroundf(AudioLayouts::inwardCoordinate(block % 4, tile % 4, RNDR_X));
                    const int y = lroundf(AudioLayouts::inwardCoordinate(block / 4, tile / 4, RNDR_Y));
                    drawBar(x, y, band);
                }
            }
        } else {
            // Traverse each footprint voxel once. Reversing successive faces
            // gives their shared corners the same band at either side of a seam.
            for (int y = 0; y < RNDR_Y; ++y) {
                for (int x = 0; x < RNDR_X; ++x) {
                    const int group = surroundGroupAt(x, y, thickness);
                    if (group >= 0) drawBar(x, y, group);
                }
            }
        }
        char detail[192];
        if (inward) {
            snprintf(detail, sizeof(detail),
                     "GEQ Inward curve=%s gain=%.2f baseline=%.2f bassScale=%.2f bars=%.1f/%.1f/%.1f/%.1f/%.1f/%.1f/%.1f/%.1f/%.1f/%.1f/%.1f/%.1f/%.1f/%.1f/%.1f/%.1f",
                     compensated ? "comp" : "raw", double(gain), double(baseline), double(bassBarScale),
                     double(heights[0]), double(heights[1]), double(heights[2]), double(heights[3]),
                     double(heights[4]), double(heights[5]), double(heights[6]), double(heights[7]),
                     double(heights[8]), double(heights[9]), double(heights[10]), double(heights[11]),
                     double(heights[12]), double(heights[13]), double(heights[14]), double(heights[15]));
        } else {
            snprintf(detail, sizeof(detail), "GEQ curve=%s gain=%.2f bassGain=%.2f baseline=%.2f bass=%u radius=%.2f bars=%.1f/%.1f/%.1f/%.1f/%.1f/%.1f/%.1f/%.1f",
                     compensated ? "comp" : "raw", double(gain), double(bassGain), double(baseline), unsigned(bass), double(radius),
                     double(heights[0]), double(heights[1]), double(heights[2]), double(heights[3]),
                     double(heights[4]), double(heights[5]), double(heights[6]), double(heights[7]));
        }
        Audio::debug(audio, detail, 0, displayBins);
        showCube();
    }
}

namespace {
enum class CornerFront { Plane, Sphere, Box };

// trial (audition B, owner-approved 2026-11-20): launch size follows the
// swell against the song's recent volume range, the fill discharges on
// launch and regrows over a beat, the front crosses in about one beat, and
// a half-beat pause separates launches. trial=false is the accepted look.
void drawCornerBurst(CornerFront shape, bool trial = false) {
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
    // Trial state. The range tracker follows rises and falls instantly and
    // relaxes by 25 levels per second, so it reflects the last few seconds.
    float rangeHigh = 0.0f, rangeLow = 255.0f;
    float launchStrength = 1.0f;
    float launchTravelMs = travelMs;
    uint32_t dischargedAt = 0;
    bool discharged = false;
    uint32_t pauseUntil = started;
    uint32_t lastFrameMs = started;
    unsigned launches = 0;

    while (TestDemo::audioRoutineRunning()) {
        const auto audio = RemoteInputs::audio();
        const uint32_t now = millis();
        const int volume = audio.volumeSm;
        const float frameSeconds = min(0.1f, uint32_t(now - lastFrameMs) / 1000.0f);
        lastFrameMs = now;
        rangeHigh = max(float(volume), rangeHigh - Audio::controls().cornerBurstTrialRangeRelax * frameSeconds);
        rangeLow = min(float(volume), rangeLow + Audio::controls().cornerBurstTrialRangeRelax * frameSeconds);
        const float range = max(16.0f, rangeHigh - rangeLow);
        const float beatMs = 60000.0f / constrain(audio.bpm, 60.0f, 180.0f);
        float fill = attachedDepth(volume);
        if (trial && discharged) {
            // Spring release: the dim body regrows from the corner over a beat.
            fill *= min(1.0f, uint32_t(now - dischargedAt) / beatMs);
        }
        const CRGB rainbow = CHSV(uint8_t(uint32_t(now - started) / 25), 255, 255);
        if (phase == Phase::Waiting) {
            trough = min(trough, volume);
            const int swellNeeded = trial ? max(minDrop, int(lroundf(Audio::controls().cornerBurstTrialSwell * range))) : minDrop;
            const bool paused = trial && int32_t(now - pauseUntil) < 0;
            if (!paused && volume - trough >= swellNeeded) {
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
                ++launches;
                if (trial) {
                    // Peak size against the song's recent range: a big swell
                    // is bright, thick and fast; a small one faint and thin.
                    launchStrength = constrain((crest - trough) / (Audio::controls().cornerBurstTrialFullSwell * range), 0.25f, 1.0f);
                    launchTravelMs = beatMs * (1.3f - 0.5f * launchStrength);
                    dischargedAt = now;
                    discharged = true;
                }
                confirming = false;
                phase = Phase::Traveling;
            }
        } else if (phase == Phase::Traveling) {
            const float crossingMs = trial ? launchTravelMs : travelMs;
            front = releaseDepth + maxDepth * uint32_t(now - released) / crossingMs;
            if (front < maxDepth) {
                frontStrength = (maxDepth - front) / (maxDepth - releaseDepth);
                if (trial) frontStrength *= 0.35f + 0.65f * launchStrength;
                frontColor = releaseColor;
            } else {
                trough = volume;
                phase = Phase::Waiting;
                if (trial) pauseUntil = now + uint32_t(beatMs * 0.5f);
            }
        }
        const float edgeWidth = trial && phase == Phase::Traveling ? 0.8f + 1.7f * launchStrength : 1.5f;

        clearAll();
        // Measure from the bottom corner 90 degrees clockwise from (0,0,0). Sum gives
        // a diagonal plane, Euclidean distance a sphere, and max a box.
        for (int z = 0; z < RNDR_Z; ++z) {
            for (int y = 0; y < RNDR_Y; ++y) {
                for (int x = 0; x < RNDR_X; ++x) {
                    const int dx = x, dy = farY - y, dz = z;
                    const float depth = depthAt(dx, dy, dz);
                    const float body = 0.15f * min(1.0f, max(0.0f, fill - depth));
                    const float edge = frontStrength * max(0.0f, 1.0f - fabsf(depth - front) / edgeWidth);
                    const uint8_t brightness = uint8_t(255.0f * max(body, edge));
                    if (brightness > 0) {
                        CRGB color = edge > body ? frontColor : rainbow;
                        color.nscale8(brightness);
                        setVoxel(x, y, z, color);
                    }
                }
            }
        }
        // Keep the origin visible even after a released wave has left the cube.
        setVoxel(0, farY, 0, rainbow);
        char detail[192];
        snprintf(detail, sizeof(detail), "Burst%s phase=%s crest=%d dropMs=%lu fill=%.1f front=%.1f gain=%.2f launches=%u strength=%.2f range=%.0f",
                 trial ? " trial" : "",
                 phase == Phase::Traveling ? "travel" : phase == Phase::Crest ? "crest" : "waiting",
                 crest, (unsigned long)(confirming ? uint32_t(now - dropStarted) : 0),
                 double(fill), double(front), double(volumeGain), launches,
                 double(trial ? launchStrength : 1.0f), double(range));
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

void Audio::cornerBurst(uint8_t shape, bool trial) {
    const CornerFront fronts[] = {CornerFront::Plane, CornerFront::Sphere, CornerFront::Box};
    drawCornerBurst(fronts[shape % 3], trial);
}
