#pragma once
#include "Shapes.h"
#include "TestDemo.h"
#include "CubeCore.h"
#include "TextureEngine.h"
#include "GeometryEngine.h"

class Boxes {
    static void animateFastCornerPulse(RenderMode rMode, TextureState &tex, bool travel, uint32_t speedMs) {
        auto getCorner = [](uint8_t &cx, uint8_t &cy, uint8_t &cz) {
            cx = (random8(2) == 0) ? 0 : RNDR_X - 1;
            cy = (random8(2) == 0) ? 0 : RNDR_Y - 1;
            cz = (random8(2) == 0) ? 0 : RNDR_Z - 1;
        };

        uint8_t cx1, cy1, cz1; getCorner(cx1, cy1, cz1);
        uint8_t cx2 = cx1, cy2 = cy1, cz2 = cz1;

        if (travel) {
            do { getCorner(cx2, cy2, cz2); } while (cx1 == cx2 && cy1 == cy2 && cz1 == cz2);
        }

        uint8_t maxSize = max(RNDR_X, max(RNDR_Y, RNDR_Z));

        auto applyBounds = [&](uint8_t size, uint8_t cx, uint8_t cy, uint8_t cz, uint8_t &x0, uint8_t &y0, uint8_t &z0, uint8_t &x1, uint8_t &y1, uint8_t &z1) {
            if (cx == 0) { x0 = 0; x1 = min((int)RNDR_X - 1, size - 1); } else { x1 = RNDR_X - 1; x0 = max(0, (int)RNDR_X - size); }
            if (cy == 0) { y0 = 0; y1 = min((int)RNDR_Y - 1, size - 1); } else { y1 = RNDR_Y - 1; y0 = max(0, (int)RNDR_Y - size); }
            if (cz == 0) { z0 = 0; z1 = min((int)RNDR_Z - 1, size - 1); } else { z1 = RNDR_Z - 1; z0 = max(0, (int)RNDR_Z - size); }
        };

        uint32_t lastFrameTime = millis();

        for (uint8_t size = 1; size <= maxSize; size++) {
            uint8_t x0, y0, z0, x1, y1, z1; applyBounds(size, cx1, cy1, cz1, x0, y0, z0, x1, y1, z1);
            clearAll(); GeometryEngine::drawFastBox(x0, y0, z0, x1, y1, z1, rMode, tex); showCube();
            uint32_t now = millis(); tex.advance(now - lastFrameTime); lastFrameTime = now;
            if (speedMs > 0) delay(speedMs); else yield();
        }
        for (int size = maxSize - 1; size >= 1; size--) {
            uint8_t x0, y0, z0, x1, y1, z1; applyBounds((uint8_t)size, cx2, cy2, cz2, x0, y0, z0, x1, y1, z1);
            clearAll(); GeometryEngine::drawFastBox(x0, y0, z0, x1, y1, z1, rMode, tex); showCube();
            uint32_t now = millis(); tex.advance(now - lastFrameTime); lastFrameTime = now;
            if (speedMs > 0) delay(speedMs); else yield();
        }
    }

    static void animateFastCenterPulse(RenderMode rMode, TextureState &tex, uint32_t speedMs) {
        int cx1 = (RNDR_X - 1) / 2, cx2 = RNDR_X / 2;
        int cy1 = (RNDR_Y - 1) / 2, cy2 = RNDR_Y / 2;
        int cz1 = (RNDR_Z - 1) / 2, cz2 = RNDR_Z / 2;
        uint8_t max_r = max(RNDR_X, max(RNDR_Y, RNDR_Z)) / 2;

        auto applyBounds = [&](uint8_t r, uint8_t &x0, uint8_t &y0, uint8_t &z0, uint8_t &x1, uint8_t &y1, uint8_t &z1) {
            x0 = max(0, cx1 - r); x1 = min((int)RNDR_X - 1, cx2 + r);
            y0 = max(0, cy1 - r); y1 = min((int)RNDR_Y - 1, cy2 + r);
            z0 = max(0, cz1 - r); z1 = min((int)RNDR_Z - 1, cz2 + r);
        };

        uint32_t lastFrameTime = millis();

        for (uint8_t r = 0; r <= max_r; r++) {
            uint8_t x0, y0, z0, x1, y1, z1; applyBounds(r, x0, y0, z0, x1, y1, z1);
            clearAll(); GeometryEngine::drawFastBox(x0, y0, z0, x1, y1, z1, rMode, tex); showCube();
            uint32_t now = millis(); tex.advance(now - lastFrameTime); lastFrameTime = now;
            if (speedMs > 0) delay(speedMs); else yield();
        }
        for (int r = max_r - 1; r >= 0; r--) {
            uint8_t x0, y0, z0, x1, y1, z1; applyBounds((uint8_t)r, x0, y0, z0, x1, y1, z1);
            clearAll(); GeometryEngine::drawFastBox(x0, y0, z0, x1, y1, z1, rMode, tex); showCube();
            uint32_t now = millis(); tex.advance(now - lastFrameTime); lastFrameTime = now;
            if (speedMs > 0) delay(speedMs); else yield();
        }
    }

    static void animatePulseBoxSequenceInternal(uint8_t iterations, bool travel, RenderMode rMode, ShaderMode sMode, CRGBPalette16 pal, uint32_t speedMs) {
        if (iterations == 0) return;
        TextureState tex(sMode, pal, 1.0f);
        for (int i = 0; i < iterations - 1; i++) animateFastCornerPulse(rMode, tex, travel, speedMs);
        for (int i = 0; i < 2; i++) animateFastCenterPulse(rMode, tex, speedMs);
    }

public:
    // --- animateFlyingBox ---
    static void animateFlyingBox(uint32_t durationMs, uint32_t speedMs) {
        uint32_t startTime = millis();

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            bool travel = (random8(2) == 0);
            RenderMode rMode = (RenderMode)random8(3);

            // Ask the engine for a guaranteed good aesthetic pairing
            TexturePlan plan = PaletteUtils::getRandomTexturePlan();
            TextureState tex(plan.sMode, plan.palette, 1.0f);

            animateFastCornerPulse(rMode, tex, travel, speedMs);
        }
    }

    // --- animatePulseBoxSequence ---
    static void animatePulseBoxSequence(uint8_t iterations, bool travel, RenderMode rMode, CRGBPalette16 pal, ShaderMode sMode, uint32_t speedMs = 0) {
        pal = Shapes::applyShapeColorHacks(SHAPE_BOX, pal, sMode);
        animatePulseBoxSequenceInternal(iterations, travel, rMode, sMode, pal, speedMs);
    }
    //If you just pass an HTMLColorCode, it implies SOLID
    static void animatePulseBoxSequence(uint8_t iterations, bool travel, RenderMode rMode, CRGB::HTMLColorCode colorCode, uint32_t speedMs = 0) {
        animatePulseBoxSequence(iterations, travel, rMode, CRGBPalette16(CRGB(colorCode)), MODE_SOLID, speedMs);
    }
    //If you just pass a ShaderMode for something like hiphotic, it implies an ignored palette
    static void animatePulseBoxSequence(uint8_t iterations, bool travel, RenderMode rMode, ShaderMode sMode, uint32_t speedMs = 0) {
        animatePulseBoxSequence(iterations, travel, rMode, RainbowColors_p, sMode, speedMs);
    }

    static void animateMovingBoxes(uint32_t durationMs, uint32_t speedMs) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        //float r = RNDR_X / 8.0f;
        float r = (RNDR_X > 8) ? (RNDR_X / 8.0f) : 1.5f;

        float x[3], y[3], z[3];
        float *u[3], *v[3];
        float maxU[3], maxV[3];
        uint8_t edge[3] = {0, 0, 0};

        // 1. Store the randomized plans
        TexturePlan plan[3];

        for (int i = 0; i < 3; i++) {
            x[i] = random16(r * 10, (RNDR_X - 1 - r) * 10) / 10.0f;
            y[i] = random16(r * 10, (RNDR_Y - 1 - r) * 10) / 10.0f;
            z[i] = random16(r * 10, (RNDR_Z - 1 - r) * 10) / 10.0f;

            // Generate a guaranteed good shader/palette pairing for each box
            plan[i] = PaletteUtils::getRandomTexturePlan();
            bool reverse = (random8(2) == 0);

            if (i == 0) {
                u[i] = reverse ? &y[i] : &x[i]; v[i] = reverse ? &x[i] : &y[i];
                maxU[i] = reverse ? (RNDR_Y - 1 - r) : (RNDR_X - 1 - r); maxV[i] = reverse ? (RNDR_X - 1 - r) : (RNDR_Y - 1 - r);
            } else if (i == 1) {
                u[i] = reverse ? &z[i] : &x[i]; v[i] = reverse ? &x[i] : &z[i];
                maxU[i] = reverse ? (RNDR_Z - 1 - r) : (RNDR_X - 1 - r); maxV[i] = reverse ? (RNDR_X - 1 - r) : (RNDR_Z - 1 - r);
            } else {
                u[i] = reverse ? &z[i] : &y[i]; v[i] = reverse ? &y[i] : &z[i];
                maxU[i] = reverse ? (RNDR_Z - 1 - r) : (RNDR_Y - 1 - r); maxV[i] = reverse ? (RNDR_Y - 1 - r) : (RNDR_Z - 1 - r);
            }
        }

        // 2. Initialize the persistent states BEFORE the loop so they can animate their phases
        TextureState tex[3] = {
            TextureState(plan[0].sMode, plan[0].palette, 1.0f),
            TextureState(plan[1].sMode, plan[1].palette, 1.0f),
            TextureState(plan[2].sMode, plan[2].palette, 1.0f)
        };

        float step = 0.5f;

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrame;
            lastFrame = now;

            clearAll();

            for (int i = 0; i < 3; i++) {
                if (edge[i] == 0) {
                    *u[i] += step; if (*u[i] >= maxU[i]) { *u[i] = maxU[i]; edge[i] = 1; }
                } else if (edge[i] == 1) {
                    *v[i] += step; if (*v[i] >= maxV[i]) { *v[i] = maxV[i]; edge[i] = 2; }
                } else if (edge[i] == 2) {
                    *u[i] -= step; if (*u[i] <= r) { *u[i] = r; edge[i] = 3; }
                } else if (edge[i] == 3) {
                    *v[i] -= step; if (*v[i] <= r) { *v[i] = r; edge[i] = 0; }
                }

                // 3. Render and advance phase
                uint8_t x0 = max(0, (int)round(x[i] - r));
                uint8_t x1 = min((int)RNDR_X - 1, (int)round(x[i] + r));
                uint8_t y0 = max(0, (int)round(y[i] - r));
                uint8_t y1 = min((int)RNDR_Y - 1, (int)round(y[i] + r));
                uint8_t z0 = max(0, (int)round(z[i] - r));
                uint8_t z1 = min((int)RNDR_Z - 1, (int)round(z[i] + r));

                GeometryEngine::drawFastBox(x0, y0, z0, x1, y1, z1, RENDER_SOLID, tex[i]);
                tex[i].advance(deltaMs);
            }

            showCube();
            if (speedMs > 0) delay(speedMs); else yield();
        }
    }
};
