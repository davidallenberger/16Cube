#pragma once
#include "CubeCore.h"
#include "GeometryEngine.h"
#include "TextureEngine.h"
#include "TestDemo.h"

class EyeOfSauron {
public:
    static void animateEyeOfSauron(uint32_t durationMs) {
        const CRGBPalette16 palette = PaletteUtils::firePalette();
        const float sphereRadius = min(RNDR_X, min(RNDR_Y, RNDR_Z)) * 0.5f;
        const float pupilHalfHeight = sphereRadius * 0.64f;
        const float pupilHalfWidth = max(1.05f, sphereRadius * 0.18f);
        constexpr uint32_t gazeCycleMs = 6000;
        struct Gaze { uint16_t ms; float degrees; float height; };
        constexpr Gaze gaze[] = {
            {0, 0, 0}, {700, 3, 0.1f}, {1050, -65, 0.55f}, {1600, -72, 0.4f},
            {2100, -90, 0.2f}, {2600, -87, 0.15f}, {3150, 27, -0.5f},
            {3650, 34, -0.4f}, {4090, 90, 0.35f}, {4820, 86, 0.2f},
            {5440, 0, 0}, {gazeCycleMs, 0, 0}
        };
        const uint32_t started = millis();
        uint32_t lastFrame = started - 24;
        clearAll();
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            const uint32_t now = millis();
            const uint32_t deltaMs = now - lastFrame;
            if (deltaMs < 24) { yield(); continue; }
            lastFrame = now;
            const uint32_t elapsed = now - started;
            const uint16_t time = uint16_t(elapsed * 4 / 5);

            const uint32_t gazeTime = elapsed % gazeCycleMs;
            unsigned next = 1;
            while (gazeTime >= gaze[next].ms) ++next;
            const Gaze& from = gaze[next - 1];
            const Gaze& to = gaze[next];
            const float t = float(gazeTime - from.ms) / (to.ms - from.ms);
            const float eased = t * t * (3.0f - 2.0f * t);
            const float angle = (from.degrees + (to.degrees - from.degrees) * eased) * (PI / 180.0f);
            const float gazeX = sinf(angle), gazeY = cosf(angle);
            const float pupilZ = (from.height + (to.height - from.height) * eased) * (sphereRadius / 8.0f);

            clearAll();
            GeometryEngine::rasterDepthFeatheredSphere(sphereRadius, [&](int x, int z, float span) {
                const float width = x - RNDR_CX;
                const float height = z - RNDR_CZ;
                const TextureDetail::RadialFireColumn fire(x, z, width / sphereRadius, height / sphereRadius, time);

                for (int y = 0; y < RNDR_Y; ++y) {
                    const float depth = y - RNDR_CY;
                    const float coverage = GeometryEngine::depthFeatheredCoverage(span, depth);
                    if (coverage <= 0.0f) continue;

                    const float acrossGaze = width * gazeY - depth * gazeX;
                    const float slitHeight = height - pupilZ;
                    const float slitZ = slitHeight / pupilHalfHeight;
                    const float slitWidth = pupilHalfWidth * sqrtf(max(0.0f, 1.0f - slitZ * slitZ));
                    const float slitEdge = max(fabsf(acrossGaze) - slitWidth, fabsf(slitHeight) - pupilHalfHeight);
                    const uint8_t pupilBlend = uint8_t(255.0f * constrain(0.5f - slitEdge, 0.0f, 1.0f));
                    if (pupilBlend == 255) continue;

                    CRGB color = blend(fire.colorAt(y, palette), CRGB::Black, pupilBlend);
                    color.nscale8(uint8_t(255.0f * coverage));
                    setVoxel(x, y, z, color);
                }
            });
            showCube();
        }
    }
};
