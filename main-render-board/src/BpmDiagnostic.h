#pragma once

#include "AppMemory.h"
#include "BpmRenderer.h"
#include "UARTReceiver.h"
#include <freertos/task.h>

#ifndef CUBINA_BPM_PROBE_LOG
#define CUBINA_BPM_PROBE_LOG 1
#endif

// Temporary fixed-pattern comparison, independent of the incoming audio.
// This uses the same map/draw engine and clock equations as timed Streamers
// BPM. It intentionally does not track music, change gain, or pace frames.
// Removing the diagnostic switch in CubeSystem restores normal dispatch.
class BpmDiagnostic {
  public:
    static void run() {
        // TestDemo's desktop BPM uses speed 64, which maps to 37 BPM. Fix the
        // geometry, palette, brightness and speed for BOTH test modes so the
        // incoming packet rate is the variable we are comparing.
        constexpr uint8_t FIXED_BPM = 37;
        constexpr uint8_t FIXED_LEVEL = 255;
        uint8_t *phaseMap = reinterpret_cast<uint8_t *>(SharedAppMemory);
        BpmRenderer::prepare(phaseMap, BPM_CHECKERBOARD);

        // These counters and this USB text buffer belong only to this render
        // task invocation. No lock or resource-owning local crosses showCube,
        // because its checkpoint may use the existing longjmp escape.
        char report[320] = {};
        size_t queued = 0;
        size_t sent = 0;
        uint32_t windowStart = micros();
        uint32_t frames = 0;
        uint32_t maximumDrawUs = 0;
        uint32_t maximumShowUs = 0;
        uint32_t previousAccepted = RemoteInputs::stats().accepted;

        // This records the task running the drawing code, and the FastLED
        // version compiled into THIS firmware. It does not measure ISR core
        // affinity or count DMA underruns inside the LED driver.
#ifdef FASTLED_VERSION
        const unsigned long fastLedVersion = FASTLED_VERSION;
#else
        const unsigned long fastLedVersion = 0;
#endif
        Serial.printf("BPM_PROBE: fixed Checkerboard, 37 BPM, level=255, renderCore=%u, "
                      "FASTLED_VERSION=%lu, periodicLog=%u\r\n",
                      unsigned(xPortGetCoreID()), fastLedVersion,
                      unsigned(CUBINA_BPM_PROBE_LOG != 0));

        for (;;) {
            // Bounded USB output: write only what fits now, with at most 64
            // bytes per frame. Nothing here is sent over binary Serial2.
            if (CUBINA_BPM_PROBE_LOG && sent < queued) {
                const int space = Serial.availableForWrite();
                if (space > 0) {
                    size_t count = min(size_t(space), queued - sent);
                    count = min(count, size_t(64));
                    sent += Serial.write(reinterpret_cast<uint8_t *>(report + sent), count);
                }
            }

            const uint32_t now = millis();
            const uint8_t beat = beatsin8(FIXED_BPM, 64, 255);
            const uint8_t timeBase = uint8_t(now / 15);
            const uint32_t drawStart = micros();
            BpmRenderer::draw(phaseMap, RainbowColors_p, uint8_t(beat - timeBase),
                              timeBase, FIXED_LEVEL);
            const uint32_t showStart = micros();
            maximumDrawUs = max(maximumDrawUs, uint32_t(showStart - drawStart));

            // Keep the production showCube unchanged: same mode checkpoint,
            // same 60 Hz cap, same LED driver, and the same physical buffers.
            showCube();
            const uint32_t showEnd = micros();
            maximumShowUs = max(maximumShowUs, uint32_t(showEnd - showStart));
            ++frames;

            const uint32_t elapsed = uint32_t(showEnd - windowStart);
            if (elapsed >= 1000000) {
                if (CUBINA_BPM_PROBE_LOG && sent >= queued) {
                    // The receiver accessors briefly copy under their mutex
                    // and release it before any USB formatting/transmission.
                    const RemoteInputs::Control control = RemoteInputs::control();
                    const RemoteInputs::Stats stats = RemoteInputs::stats();
                    const uint32_t newPackets = uint32_t(stats.accepted - previousAccepted);
                    const double fps = double(frames) * 1000000.0 / elapsed;
                    const double packetsPerSecond = double(newPackets) * 1000000.0 / elapsed;
                    const char *mode = "Other";
                    switch (control.mode) {
                    case CubinaWire::Mode::Animation:
                        mode = "Animation";
                        break;
                    case CubinaWire::Mode::AudioMic:
                        mode = "Mic";
                        break;
                    case CubinaWire::Mode::AudioBluetooth:
                        mode = "ADC";
                        break;
                    default:
                        break;
                    }

                    // ShowMax includes the checkpoint, frame-cap wait and
                    // LED transfer. DrawMax covers only map-to-voxel drawing.
                    // They are elapsed durations, not CPU utilization or
                    // measurements of the driver's short interrupt deadlines.
                    const int length = snprintf(
                        report, sizeof(report),
                        "BPM_PROBE: mode=%s linked=%u ready=%u rev=%lu "
                        "FPS=%.1f DrawMax=%luus ShowMax=%luus "
                        "RX/s=%.1f RXok=%lu RXbad=%lu RXdup=%lu\r\n",
                        mode, unsigned(control.linked), unsigned(control.ready),
                        (unsigned long)control.revision, fps,
                        (unsigned long)maximumDrawUs, (unsigned long)maximumShowUs,
                        packetsPerSecond, (unsigned long)stats.accepted,
                        (unsigned long)stats.rejected, (unsigned long)stats.duplicates);
                    queued = length > 0 && size_t(length) < sizeof(report) ? size_t(length) : 0;
                    sent = 0;
                    previousAccepted = stats.accepted;
                    windowStart = showEnd;
                    frames = 0;
                    maximumDrawUs = 0;
                    maximumShowUs = 0;
                } else if (!CUBINA_BPM_PROBE_LOG) {
                    // No periodic USB text in this comparison. Leave the
                    // geometry, LED timing and incoming UART processing alone.
                    windowStart = showEnd;
                    frames = 0;
                    maximumDrawUs = 0;
                    maximumShowUs = 0;
                }
                // If USB has not drained the previous report, retain this
                // longer measurement window instead of blocking or flooding.
            }
        }
    }
};
