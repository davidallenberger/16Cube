#include "Audio.h"
#include "CubeSystem.h"
#include "AppMemory.h"
#include <math.h>
#include <cmath>

#ifndef AUDIO_SERIAL_DEBUG
#define AUDIO_SERIAL_DEBUG 1
#endif

// This is the temporary audio playlist. Add each future routine here in the
// desired button order, with its readable name and static method address.
// Calling a named method directly still works; it does not change this list's
// selection. Each animation continues to own its own local working state.
const Audio::Routine *Audio::routineList(size_t &count) {
    static const Routine routines[] = {
        {"Puddles Horizontal", &Audio::animatePuddles},
        {"Puddles Vertical", &Audio::animatePuddlesVertical},
        {"Puddles Cubes", &Audio::animatePuddlesCubes},
        {"Puddle Peak Horizontal", &Audio::animatePuddlePeak},
        {"Puddle Peak Vertical", &Audio::animatePuddlePeakVertical},
        {"Puddle Peak Cubes", &Audio::animatePuddlePeakCubes},
        {"BPM Sphere", &Audio::animateBpmSphere},
        {"BPM Checkerboard", &Audio::animateBpmCheckerboard},
        {"BPM Diagonal", &Audio::animateBpmDiagonal},
        {"BPM Helix", &Audio::animateBpmHelix},
    };

    count = sizeof(routines) / sizeof(routines[0]);
    return routines;
}

Audio::Controls &Audio::controls() {
    // Future menus may update these on the animation task. They are not UART
    // globals and must not be mutated concurrently by another task.
    static Controls settings;
    return settings;
}

size_t &Audio::selectedRoutineIndex() {
    // Playback selection, not animation state. It survives app exits so a
    // source change can restart the SAME visualization with different audio.
    // This is render-task-owned state; the receiver never changes it.
    static size_t selectedIndex = 0;
    return selectedIndex;
}

void Audio::nextRoutine() {
    size_t count = 0;
    routineList(count);

    size_t &selectedIndex = selectedRoutineIndex();
    selectedIndex = (selectedIndex + 1) % count;
}

const char *Audio::currentRoutineName() {
    size_t count = 0;
    const Routine *routines = routineList(count);
    return routines[selectedRoutineIndex() % count].name;
}

// CubeSystem calls this entry point after choosing audio mode. No timer or
// automatic selection change is introduced: unattended playback stays on the
// selected effect. A repeated demand for the same source selects the next
// effect in frameCheckpoint(), exits the old app, and then calls run() again.
void Audio::run() {
    size_t count = 0;
    const Routine *routines = routineList(count);
    const Routine &routine = routines[selectedRoutineIndex() % count];

    Serial.printf("Audio routine: %s\r\n", routine.name);

    // The current effects run indefinitely and call showCube() regularly.
    // If a future effect returns, restart it here instead of unexpectedly
    // dropping unattended playback into CubeSystem's game menu.
    for (;;) {
        routine.run();
        CubeSystem::frameCheckpoint();
        delay(1);
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

void Audio::debug(const RemoteInputs::Audio &audio, const char *details) {
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
        const long age = audio.valid ? long(uint32_t(now - audio.receivedMs)) : -1;
        const char *rhythmState = "stale";
        if (audio.valid) {
            if (!(audio.values.tempoFlags & CubinaWire::AUDIO_ACTIVE)) {
                rhythmState = "idle";
            } else if (audio.values.tempoFlags & CubinaWire::TEMPO_LOCKED) {
                rhythmState = "locked";
            } else {
                rhythmState = "search";
            }
        }
        int n =
            snprintf(text, sizeof(text),
                     "Audio: source=%s routine=%s linked=%u valid=%u session=%lu rev=%lu age=%ldms "
                     "VRaw=%u VSm=%u VAvg=%u Kicks=%lu BPM=%.1f Phase=%u%% Conf=%u%% Rhythm=%s RXok=%lu "
                     "RXbad=%lu%s%s\r\nBins:[",
                     source, currentRoutineName(), unsigned(control.linked), unsigned(audio.valid),
                     (unsigned long)control.session, (unsigned long)control.revision, age,
                     unsigned(audio.values.volumeRaw), unsigned(audio.values.volumeSm),
                     unsigned(audio.values.sampleAvg), (unsigned long)audio.beatCount,
                     double(audio.values.tempoBpm100) / 100.0,
                     unsigned(audio.values.tempoPhase) * 100 / 65536,
                     unsigned(audio.values.tempoConfidence) * 100 / 255,
                     rhythmState,
                     (unsigned long)stats.accepted, (unsigned long)stats.rejected,
                     details && details[0] ? " " : "", details ? details : "");
        if (n <= 0 || size_t(n) >= sizeof(text)) {
            return;
        }
        queued = size_t(n);

        for (unsigned i = 0; i < 16; ++i) {
            n = snprintf(text + queued, sizeof(text) - queued, i == 15 ? "%u]\r\n" : "%u ",
                         unsigned(audio.values.fftResult[i]));
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
            controls().maxCubeEdge);
}

void Audio::animatePuddlePeakVertical() {
    puddles(true, PuddleShape::Vertical, controls().puddlePeakSensitivity, controls().maxCubeEdge);
}

void Audio::animatePuddlePeakCubes() {
    puddles(true, PuddleShape::Cube, controls().puddlePeakSensitivity, controls().maxCubeEdge);
}

// Inspired by Puddles/Puddlepeak by Andrew Tuline, merged by @dedehai.
// This adaptation draws a line for a noticeable NEW burst in any frequency
// band, rather than selecting one frequency or reusing the bass-kick flag.
// Each band is compared with its own recent level, so a quieter treble accent
// can qualify even while another band carries a steady, louder bass note.
//
// Detection uses the 16 compressed/smoothed band summaries already received.
// This is a visual onset detector, not raw-waveform peak measurement or musical
// beat tracking. It changes neither acquisition gain nor the UART protocol.
void Audio::puddles(bool peakDetect, PuddleShape shape, float sensitivity, uint8_t maxCubeEdge) {
    constexpr float PUDDLE_FADE_TIME_MS = 250.0f;

    // The main visual sensitivity knob: higher detects smaller increases.
    // This affects WHEN a line appears, not audio gain or line length.
    sensitivity = std::isfinite(sensitivity) ? max(0.0f, sensitivity) : 1.0f;
    constexpr uint32_t PUDDLE_SPAWN_INTERVAL_MS = 34;

    // Defaults for the detector, all in the existing 0..255 band units.
    // Require BOTH a meaningful absolute rise and a rise relative to that
    // band's recent level. A small activity floor excludes near-zero chatter.
    constexpr float REFERENCE_TIME_MS = 350.0f;
    constexpr float MINIMUM_RISE = 8.0f;
    constexpr float RELATIVE_RISE = 0.35f;
    constexpr uint8_t MINIMUM_BAND_LEVEL = 12;
    constexpr uint32_t PEAK_MIN_INTERVAL_MS = 100;
    constexpr uint32_t REFERENCE_GAP_MS = 250;

    // Each routine invocation owns its detector history. baseline[] follows
    // recent levels; armed[] remembers whether a band may report a new burst.
    // Nothing here is shared with the receiver task or another animation.
    float baseline[16] = {};
    bool armed[16] = {};

    uint32_t lastFade = millis();
    uint32_t lastPeak = 0;
    uint32_t lastSpawn = millis();
    uint32_t lastExaminedCapture = 0;
    bool havePeak = false;
    bool haveReference = false;
    uint8_t hue = 0;

    for (;;) {
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

        const RemoteInputs::Audio audio = RemoteInputs::audio();
        Audio::debug(audio);

        bool peak = false;

        if (!peakDetect) {
            const float volume = audio.valid ? audio.values.volumeSm * sensitivity : 0.0f;
            peak = volume >= 2.0f && uint32_t(now - lastSpawn) >= PUDDLE_SPAWN_INTERVAL_MS;
            if (peak) {
                lastSpawn = now;
            }
        } else if (!audio.valid) {
            // Do not compare resumed audio with history from before a dropout.
            // Existing lines still fade while no usable input is available.
            haveReference = false;
        } else if (!haveReference || audio.capturedMs != lastExaminedCapture) {
            // Process each publication once, even if render frames reread it.
            // Subtract only COMMAND-board timestamps from one another; never
            // compare them with this board's clock. Unsigned subtraction also
            // handles the sender's millisecond counter wrapping.
            const uint32_t audioElapsed = uint32_t(audio.capturedMs - lastExaminedCapture);
            const bool initialize = !haveReference || audioElapsed > REFERENCE_GAP_MS;
            lastExaminedCapture = audio.capturedMs;

            if (initialize) {
                // Establish the current background without inventing an
                // onset merely because we entered the routine or reconnected.
                for (unsigned band = 0; band < 16; ++band) {
                    baseline[band] = audio.values.fftResult[band];
                    armed[band] = true;
                }
                haveReference = true;
            } else {
                const float follow = 1.0f - expf(-float(audioElapsed) / REFERENCE_TIME_MS);
                const bool audible = audio.values.volumeSm > 1;
                bool anyOnset = false;

                for (unsigned band = 0; band < 16; ++band) {
                    const float level = audio.values.fftResult[band];
                    const float rise = level - baseline[band];
                    const float requiredRise =
                        max(MINIMUM_RISE, baseline[band] * RELATIVE_RISE) / max(0.05f, sensitivity);

                    if (audible && level >= MINIMUM_BAND_LEVEL && rise >= requiredRise) {
                        if (armed[band]) {
                            anyOnset = true;
                        }
                        armed[band] = false;
                    } else if (!audible || rise <= requiredRise * 0.4f) {
                        // Hysteresis: settle substantially below the trigger
                        // difference before allowing this band to fire again.
                        // A constant note eventually becomes its own baseline
                        // and cannot retrigger simply for remaining loud.
                        armed[band] = true;
                    }

                    // Update AFTER testing, so the new burst is compared with
                    // the previous background rather than absorbed into it.
                    // This adapts a reference only; it never rescales audio.
                    baseline[band] += follow * (level - baseline[band]);
                }

                // Merge all qualifying bands into one visual event. Marking
                // bands above also consumes rises during this cooldown, so
                // one staggered attack is not queued up as several late lines.
                const bool intervalElapsed =
                    !havePeak || uint32_t(now - lastPeak) >= PEAK_MIN_INTERVAL_MS;
                peak = anyOnset && intervalElapsed;
            }
        }

        if (peak && sensitivity > 0.0f) {
            havePeak = true;
            lastPeak = now;

            // Peak sensitivity changes the trigger; ordinary sensitivity
            // changes size. Both use the SAME volume for all geometry choices.
            const float response = peakDetect ? 1.0f : sensitivity;
            const uint8_t volume = uint8_t(min(255.0f, audio.values.volumeSm * response));
            const CRGB color = CHSV(hue++, 220, 255);

            if (shape == PuddleShape::Cube) {
                const uint8_t fit = min(RNDR_X, min(RNDR_Y, RNDR_Z));
                const uint8_t maximum = max(uint8_t(1), min(uint8_t(5), min(maxCubeEdge, fit)));
                const uint8_t edge = 1 + uint16_t(volume) * (maximum - 1) / 255;
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
                // Z is the vertical axis; X is the existing horizontal line.
                const bool vertical = shape == PuddleShape::Vertical;
                const uint8_t dimension = vertical ? RNDR_Z : RNDR_X;
                const uint8_t length = 1 + uint16_t(volume) * (dimension - 1) / 255;
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

        // Silence or stale input stops new lines; existing lines fade away.
        // Every frame also checks for the next-routine or mode-change escape.
        showCube();
    }
}

void Audio::animateBpmSphere() {
    bpm(BPM_SPHERE, controls().bpmSensitivity);
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
    // A slow five-second cycle remains visible during silence. Unknown rhythm
    // while music is audible uses a provisional 60 BPM or the last estimate;
    // it is labelled "search" in debug, never presented as a confident beat.
    constexpr float IDLE_BPM = 12.0f;
    constexpr float SEARCH_BPM = 60.0f;
    sensitivity = std::isfinite(sensitivity) ? max(0.0f, sensitivity) : 1.0f;

    uint8_t *phaseMap = reinterpret_cast<uint8_t *>(SharedAppMemory);
    BpmRenderer::prepare(phaseMap, mode);
    uint32_t lastFrame = millis();
    float phase = 0.0f;
    float rate = IDLE_BPM;
    bool previouslyLocked = false;

    // These measurements belong only to this invocation on the render task.
    // They do not touch the UART payload or share state with the receiver.
    //
    // WorkMax: maximum time spent reading inputs, preparing diagnostics,
    // calculating the breath, and writing voxel colors in this window.
    // ShowMax: maximum time inside showCube(), INCLUDING its frame-cap wait,
    // checkpoint, and FastLED.show(). It is not purely LED transfer time.
    // FPS: completed frames divided by actual elapsed time in this window.
    //
    // These are elapsed wall times, not CPU-utilization measurements or DMA
    // underrun counters. A fast frame can still suffer a late LED interrupt.
    uint32_t timingStart = micros();
    uint32_t timingFrames = 0;
    uint32_t maximumWorkUs = 0;
    uint32_t maximumShowUs = 0;
    char timingText[96] = {};

    for (;;) {
        const uint32_t workStart = micros();
        const uint32_t now = millis();
        const float seconds = min(0.25f, float(uint32_t(now - lastFrame)) / 1000.0f);
        lastFrame = now;
        const RemoteInputs::Audio audio = RemoteInputs::audio();
        Audio::debug(audio, timingText);
        const bool active = audio.valid && (audio.values.tempoFlags & CubinaWire::AUDIO_ACTIVE);
        const bool locked = active && (audio.values.tempoFlags & CubinaWire::TEMPO_LOCKED);
        const float estimate = float(audio.values.tempoBpm100) / 100.0f;
        const float targetRate = !active ? IDLE_BPM : estimate >= 60.0f ? estimate : SEARCH_BPM;

        // Changing speed does not recompute phase from millis()*BPM, which
        // would jump the pattern. Integrate a smoothly changing local clock.
        rate += (1.0f - expf(-seconds / 0.5f)) * (targetRate - rate);
        phase += seconds * rate / 60.0f;
        phase -= floorf(phase);

        if (locked) {
            // The transmitted phase belongs to its latest audio publication.
            // Extrapolate only using LOCAL time since that packet arrived.
            // No cross-board subtraction or synchronized millis() is needed.
            const float age = float(uint32_t(now - audio.receivedMs)) / 1000.0f;
            float targetPhase = float(audio.values.tempoPhase) / 65536.0f + age * estimate / 60.0f;
            targetPhase -= floorf(targetPhase);
            float error = targetPhase - phase;
            error -= floorf(error + 0.5f);
            phase += previouslyLocked ? error * (1.0f - expf(-seconds / 0.15f)) : error;
            phase -= floorf(phase);
        }
        previouslyLocked = locked;

        // One smooth visual breath per beat. The musical phase moves BOTH
        // the density waves and the brightness envelope. Palette cycling is
        // decorative and deliberately independent of the tempo.
        const float breath = 0.5f + 0.5f * cosf(phase * 6.283185307f);
        const uint8_t beat = uint8_t(64.0f + breath * 191.0f);
        const uint8_t travel = uint8_t(phase * 256.0f);
        const float volume = active ? min(255.0f, audio.values.volumeSm * sensitivity) : 0.0f;
        const uint8_t level =
            uint8_t(min(255.0f, 32.0f + (active ? 64.0f + volume * 0.6f : 48.0f) * breath));
        BpmRenderer::draw(phaseMap, RainbowColors_p, uint8_t(beat - travel), uint8_t(now / 40),
                          level);

        const uint32_t showStart = micros();
        maximumWorkUs = max(maximumWorkUs, uint32_t(showStart - workStart));
        showCube();
        const uint32_t showEnd = micros();
        maximumShowUs = max(maximumShowUs, uint32_t(showEnd - showStart));
        ++timingFrames;

        const uint32_t timingElapsed = uint32_t(showEnd - timingStart);
        if (timingElapsed >= 1000000) {
            // Summarize once per second; debug() adds this to its ordinary
            // bounded USB report. No extra stream is sent over Serial2.
            const double fps = double(timingFrames) * 1000000.0 / timingElapsed;
            snprintf(timingText, sizeof(timingText),
                     "Perf[WorkMax=%luus ShowMax=%luus FPS=%.1f]",
                     (unsigned long)maximumWorkUs, (unsigned long)maximumShowUs, fps);
            timingStart = showEnd;
            timingFrames = 0;
            maximumWorkUs = 0;
            maximumShowUs = 0;
        }
    }
}
