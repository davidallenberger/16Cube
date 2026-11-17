#pragma once
#include "TestDemo.h"
#include "CubeCore.h"

#include <math.h>

// One Julia evaluator/color cycle for normal and volume-driven playback.
// Optional plain callbacks keep audio acquisition and playback out of geometry.
class JuliaRenderer {
  public:
    // Direct three-coordinate Julia recurrence. Each physical voxel supplies
    // its initial (x,y,z); c is shared by the whole frame. This is not a stack
    // of independent 2D images. No heap or scratch arena is needed.
    static void animateJulia(uint32_t durationMs, uint32_t speedMs = 40,
                             bool cycleColors = true, bool varyC = true,
                             float (*closureInput)() = nullptr,
                             bool (*keepRunning)() = nullptr,
                             uint32_t attackMs = 0, uint32_t releaseMs = 0) {
        const uint8_t iterationSetting = 18; // WLED slider units; actual limit is half.
        const uint8_t maxIterations = iterationSetting / 2;
        // Canonical ROYGBIV holds, with short saturated hue transitions.
        // Each escape band advances one swatch per tick, preserving contours.
        const CRGB colors[] = {
            CRGB(0xFF0000), CRGB(0xFF8000), CRGB(0xFFFF00), CRGB(0x00FF00),
            CRGB(0x0000FF), CRGB(0x4B0082), CRGB(0x9400D3)
        };
        // Spectrum hue angles match the canonical RGB swatches above.
        const float hues[] = {0.0f, 30.117647f, 60.0f, 120.0f, 240.0f, 274.615385f, 282.085308f};
        const float values[] = {255.0f, 255.0f, 255.0f, 255.0f, 255.0f, 130.0f, 211.0f};
        const uint8_t colorCount = sizeof(colors) / sizeof(colors[0]);
        const float cRate = 2.0f; // Twice the previous parameter-motion rate.
        const uint32_t colorStepMs = 6000; // Total per color: 4s hold + 2s transition; full cycle 42s.
        const uint32_t colorHoldMs = 4000; // First 4s hold; remaining 2000ms travels through hues.
        // Fixed viewing window: physical Z is the familiar imaginary/vertical
        // axis. Physical Y explores a narrow, genuinely varying depth range,
        // keeping the black opening visible through the front and back.
        const float xStep = 1.8f / max(1.0f, float(RNDR_X - 1));
        const float yStep = 0.48f / max(1.0f, float(RNDR_Y - 1));
        const float zStep = 1.7f / max(1.0f, float(RNDR_Z - 1));
        const uint32_t started = millis();
        // uint32_t lastDebug = started - 1000;
        uint32_t lastClosureMs = started;
        float smoothedClosure = 0.0f;
        bool closureStarted = false;

        while (keepRunning ? keepRunning() :
               TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            const uint32_t frameStart = millis();
            const float seconds = float(uint32_t(frameStart - started)) * 0.001f;
            const float cTime = varyC ? seconds * cRate : 0.0f;
            const uint8_t colorOffset = cycleColors ?
                uint8_t((uint32_t(frameStart - started) / colorStepMs) % colorCount) : 0;
            CRGB frameColors[7];
            const uint32_t colorAge = uint32_t(frameStart - started) % colorStepMs;
            float transition = cycleColors && colorAge > colorHoldMs ?
                float(colorAge - colorHoldMs) / float(colorStepMs - colorHoldMs) : 0.0f;
            transition = transition * transition * (3.0f - 2.0f * transition);
            // Do color math seven times per frame, outside the voxel loop.
            for (uint8_t i = 0; i < colorCount; ++i) {
                const uint8_t current = (i + colorOffset) % colorCount;
                const uint8_t next = (current + 1) % colorCount;
                frameColors[i] = colors[current];
                if (transition > 0.0f) {
                    const float nextHue = next == 0 ? 360.0f : hues[next];
                    const float hue = hues[current] + (nextHue - hues[current]) * transition;
                    const float value = values[current] + (values[next] - values[current]) * transition;
                    const float sector = hue / 60.0f;
                    const uint8_t part = uint8_t(sector);
                    const float fraction = sector - part;
                    const uint8_t high = uint8_t(value + 0.5f);
                    const uint8_t rising = uint8_t(value * fraction + 0.5f);
                    const uint8_t falling = uint8_t(value * (1.0f - fraction) + 0.5f);
                    uint8_t red = 0, green = 0, blue = 0;
                    switch (part % 6) {
                        case 0: red = high; green = rising; break;
                        case 1: red = falling; green = high; break;
                        case 2: green = high; blue = rising; break;
                        case 3: green = falling; blue = high; break;
                        case 4: red = rising; blue = high; break;
                        case 5: red = high; blue = falling; break;
                    }
                    frameColors[i] = CRGB((uint32_t(red) << 16) | (uint32_t(green) << 8) | blue);
                }
            }
            // Interpolate between a broad opening and near closure. Endpoints
            // are fixed so the open state can reach a one-pixel ceiling/floor
            // on central strands throughout the depth. The closed endpoint
            // retains at least about 20% dark volume on supported canvases.
            float closure = 0.5f - 0.5f * cosf(cTime * 0.50f);
            if (closureInput) {
                const float target = fminf(1.0f, fmaxf(0.0f, closureInput()));
                const uint32_t deltaMs = uint32_t(frameStart - lastClosureMs);
                const uint32_t responseMs = target > smoothedClosure ? attackMs : releaseMs;
                // Smooth geometry position, not global audio. Exponential
                // response is frame-rate independent and never overshoots the
                // calibrated path. Initialize from input on each invocation.
                if (!closureStarted || responseMs == 0) {
                    smoothedClosure = target;
                } else {
                    const float amount = 1.0f - expf(-float(deltaMs) / float(responseMs));
                    smoothedClosure += amount * (target - smoothedClosure);
                }
                lastClosureMs = frameStart;
                closureStarted = true;
                closure = smoothedClosure;
            }
            const float detail = closure * (1.0f - closure);
            const float a = -0.66f + (-0.94299f + 0.66f) * closure +
                0.05f * sinf(cTime * 0.65f) * detail;
            const float b = 0.42f * closure + 0.05f * sinf(cTime * 0.50f) * detail;
            const float d = 0.12f * sinf(cTime * 0.37f) * detail;
            // unsigned lit = 0;
            clearAll();
            for (uint8_t iz = 0; iz < RNDR_Z; ++iz) {
                for (uint8_t iy = 0; iy < RNDR_Y; ++iy) {
                    for (uint8_t ix = 0; ix < RNDR_X; ++ix) {
                        float x = (float(ix) - RNDR_CX) * xStep;
                        float y = -0.85f + float(iz) * zStep;
                        float z = (float(iy) - RNDR_CY) * yStep;
                        float radiusSquared = x * x + y * y + z * z;
                        uint8_t iteration = 0;
                        while (iteration < maxIterations && radiusSquared <= 16.0f) {
                            // All right-hand sides use the OLD coordinates.
                            const float nextX = x * x - y * y - z * z + a;
                            const float nextY = 2.0f * x * y + b;
                            z = 2.0f * x * z + d;
                            x = nextX;
                            y = nextY;
                            radiusSquared = x * x + y * y + z * z;
                            ++iteration;
                        }
                        if (iteration == maxIterations) {
                            continue;
                        }
                        // Useful escape counts (2..8) span seven fixed colors.
                        // All bands share the hold/transition timing while
                        // retaining their separation. The cavern is unaffected.
                        const int band = max(0, int(iteration) - 2);
                        setVoxel(ix, iy, iz, frameColors[band % colorCount]);
                        // ++lit;
                    }
                }
            }
            showCube();
            /*
            if (uint32_t(frameStart - lastDebug) >= 1000) {
                lastDebug = frameStart;
                Serial.printf("Julia: c=(%.4f,%.4f,%.4f) closure=%.3f iterations=%u palette=Canonical-ROYGBIV cycle=%u cRate=%.1f lit=%u/%u frame=%lums\r\n",
                    double(a), double(b), double(d), double(closure), unsigned(maxIterations), unsigned(cycleColors), double(varyC ? cRate : 0.0f), lit,
                    unsigned(RNDR_X * RNDR_Y * RNDR_Z),
                    (unsigned long)uint32_t(millis() - frameStart));
            }
            */
            const uint32_t spent = uint32_t(millis() - frameStart);
            if (spent < speedMs) {
                delay(speedMs - spent);
            } else {
                yield();
            }
        }
    }

};
