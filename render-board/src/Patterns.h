#pragma once
#include "AppMemory.h"
#include "BpmRenderer.h"
#include "CubeCore.h"
#include "TestDemo.h"
#include "TextureEngine.h"

#include <math.h>

class Patterns {
public:
    // ==========================================
    // COLOR TWINKLES (Volumetric Sparkle)
    // ==========================================
    static void animateColorTwinkles(uint32_t durationMs, uint8_t effectSpeed = 128, uint8_t spawnSpeed = 128) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();

        const uint32_t totalVoxels = RNDR_X * RNDR_Y * RNDR_Z;

        // 1. MEMORY OPTIMIZATION (No Heap Allocation)
        // 1 bit per voxel. 512 bytes for a 16x16x16 cube.
        const uint32_t stateBytes = (totalVoxels + 7) / 8;

        // Zero out the shared app memory block so we don't inherit garbage
        // data from whatever effect was running before this one.
        memset(SharedAppMemory, 0, stateBytes);

        uint8_t fadeUpAmount = 8 + (effectSpeed >> 2);
        uint8_t fadeDownAmount = 8 + (effectSpeed >> 3);

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis();
            if (now - lastFrame < 15) { yield(); continue; }
            lastFrame = now;

            // 2. FADE LOOP
            for (uint32_t i = 0; i < totalVoxels; i++) {

                uint8_t x = (i % (RNDR_X * RNDR_Y)) % RNDR_X;
                uint8_t y = (i % (RNDR_X * RNDR_Y)) / RNDR_X;
                uint8_t z = i / (RNDR_X * RNDR_Y);

                CRGB c = getVoxel(x, y, z);
                if (!c) continue;

                uint32_t byteIndex = i >> 3;
                uint8_t bitNum = i & 0x07;

                // Read directly from the SharedAppMemory buffer
                bool fadingUp = bitRead(SharedAppMemory[byteIndex], bitNum);

                if (fadingUp) {
                    CRGB inc = c;
                    inc.nscale8(fadeUpAmount);
                    if (!inc) inc = c;

                    c.r = qadd8(c.r, inc.r);
                    c.g = qadd8(c.g, inc.g);
                    c.b = qadd8(c.b, inc.b);

                    if (c.r == 255 || c.g == 255 || c.b == 255) {
                        bitWrite(SharedAppMemory[byteIndex], bitNum, 0);
                    }
                    setVoxel(x, y, z, c);

                } else {
                    c.fadeToBlackBy(fadeDownAmount);
                    setVoxel(x, y, z, c);
                }
            }

            // 3. SPAWN LOOP
            uint32_t maxSpawns = (totalVoxels / 50) + 1;

            for (uint32_t j = 0; j <= maxSpawns; j++) {

                if (random8() <= spawnSpeed) {
                    for (uint8_t attempts = 0; attempts < 5; attempts++) {
                        uint32_t i = random16(totalVoxels);
                        uint8_t x = (i % (RNDR_X * RNDR_Y)) % RNDR_X;
                        uint8_t y = (i % (RNDR_X * RNDR_Y)) / RNDR_X;
                        uint8_t z = i / (RNDR_X * RNDR_Y);

                        if (!getVoxel(x, y, z)) {
                            uint32_t byteIndex = i >> 3;
                            uint8_t bitNum = i & 0x07;

                            // Flag the bit in SharedAppMemory to fade up
                            bitWrite(SharedAppMemory[byteIndex], bitNum, 1);

                            CRGB startColor = ColorFromPalette(RainbowColors_p, random8(), 64, NOBLEND);
                            setVoxel(x, y, z, startColor);

                            break;
                        }
                    }
                }
            }

            showCube();
        }
    }
// ==========================================

    static void animateTheMatrix(uint32_t durationMs, uint32_t stepMs = 25) {
        struct Column {
            float headZ;
            uint8_t fallTimeTenths;
            uint8_t trailLength;
        };
        Column columns[RNDR_Y][RNDR_X];
        const auto startDrop = [](Column &column, bool stagger) {
            column.fallTimeTenths = random8(9, 21);
            column.trailLength = max(1, (RNDR_Z * random8(4, 8) + 8) / 16);
            column.headZ = stagger
                ? random16(7 * RNDR_Z * 16) / 16.0f - column.trailLength
                : RNDR_Z - 1 + random8(3, 8) * RNDR_Z;
        };
        for (auto &row : columns) {
            for (auto &column : row) startDrop(column, true);
        }

        const uint32_t started = millis();
        uint32_t lastFrame = started;
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            const uint32_t now = millis();
            const float travel = (now - lastFrame) * (RNDR_Z / 100.0f);
            lastFrame = now;
            clearAll();

            for (int y = 0; y < RNDR_Y; ++y) {
                for (int x = 0; x < RNDR_X; ++x) {
                    Column &column = columns[y][x];
                    column.headZ -= travel / column.fallTimeTenths;
                    if (column.headZ < -column.trailLength) startDrop(column, false);
                    if (column.headZ >= RNDR_Z) continue;

                    const int first = max(0, int(floorf(column.headZ)));
                    const int last = min(RNDR_Z - 1, int(floorf(column.headZ + column.trailLength)));
                    for (int z = first; z <= last; ++z) {
                        const float distance = z - column.headZ;
                        // Feather the bright tip between rows as it falls.
                        const float tip = max(0.0f, 1.0f - fabsf(distance));
                        const float green = distance < 0.0f
                            ? 255.0f * tip
                            : 210.0f * (1.0f - distance / column.trailLength) + 45.0f * tip;
                        setVoxel(x, y, z, CRGB(uint8_t(155.0f * tip), uint8_t(green), uint8_t(95.0f * tip)));
                    }
                }
            }
            showCube();
            delay(stepMs);
        }
    }

    static void animateRain(uint32_t durationMs, int dropsPerFrame, float fallSpeed, CRGBPalette16 pal, ShaderMode sMode) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        float masterPhase = 0.0f;
        const int MAX_DROPS = 150;
        float dX[MAX_DROPS], dY[MAX_DROPS], dZ[MAX_DROPS];
        bool active[MAX_DROPS] = {false};

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now;
            clearAll();
            masterPhase += 0.33f * deltaMs;

            TextureState tex(sMode, pal, 1.0f);
            tex.internalPhase = masterPhase;
            tex.phase = (uint32_t)masterPhase;
            tex.initMath();

            for (int i = 0; i < dropsPerFrame; i++) {
                for (int j = 0; j < MAX_DROPS; j++) {
                    if (!active[j]) { active[j] = true; dX[j] = random8(RNDR_X); dY[j] = random8(RNDR_Y); dZ[j] = RNDR_Z - 1.0f; break; }
                }
            }

            for (int j = 0; j < MAX_DROPS; j++) {
                if (active[j]) {
                    int xInt = (int)dX[j], yInt = (int)dY[j], zInt = (int)roundf(dZ[j]);

                    // Only check X/Y bounds here so the drop can fall seamlessly through the floor
                    if (xInt >= 0 && xInt < RNDR_X && yInt >= 0 && yInt < RNDR_Y) {

                        // Protect TextureEngine from out-of-bounds Z queries
                        int safeZ = max(0, min(zInt, RNDR_Z - 1));
                        CRGB color = (sMode == MODE_SCATTER) ?
                                     ColorFromPalette(pal, (j * 37) % 255, 255, NOBLEND) :
                                     tex.getColor(xInt, yInt, safeZ);

                        // 1. Draw the Head (if it is currently above the floor)
                        if (zInt >= 0 && zInt < RNDR_Z) {
                            setVoxel(xInt, yInt, zInt, color);
                        }

                        // 2. Draw the Tail (1 voxel above the head)
                        if (zInt + 1 >= 0 && zInt + 1 < RNDR_Z) {
                            CRGB tail = color;
                            tail.nscale8(80);
                            setVoxel(xInt, yInt, zInt + 1, tail);
                        }
                    }

                    dZ[j] -= fallSpeed;

                    // Let it fall to -1.5f so the tail has time to hit the Z=0 floor before deleting
                    if (dZ[j] < -1.5f) active[j] = false;
                }
            }
            showCube(); delay(5);
        }
    }
    static void animateRain(uint32_t durationMs, int dropsPerFrame, float fallSpeed, CRGB::HTMLColorCode colorCode) { animateRain(durationMs, dropsPerFrame, fallSpeed, CRGBPalette16(CRGB(colorCode)),MODE_SOLID); }
    static void animateRain(uint32_t durationMs) { animateRain(durationMs, 2, 0.8f, OceanColors_p, MODE_LINEAR_FLOW);}

    static void animateRandomFall(uint32_t durationMs, float fallSpeed = 0.4f, float returnSpeed = 0.8f) {
        uint32_t startTime = millis();
        float zPos[16][16]; uint8_t state[16][16];
        for(int x = 0; x < RNDR_X; x++) { for(int y = 0; y < RNDR_Y; y++) { zPos[x][y] = RNDR_Z - 1.0f; state[x][y] = 0; } }
        bool returnPhase = false; int totalVoxels = RNDR_X * RNDR_Y;

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            clearAll(); int atBottom = 0, atTop = 0, triggersPerFrame = max(1, totalVoxels / 30);
            for(int i = 0; i < triggersPerFrame; i++) {
                int rx = random8(RNDR_X), ry = random8(RNDR_Y);
                if (!returnPhase && state[rx][ry] == 0) state[rx][ry] = 1;
                if (returnPhase && state[rx][ry] == 2) state[rx][ry] = 3;
            }

            for(int x = 0; x < RNDR_X; x++) {
                for(int y = 0; y < RNDR_Y; y++) {
                    if (state[x][y] == 1) { zPos[x][y] -= fallSpeed; if (zPos[x][y] <= 0.0f) { zPos[x][y] = 0.0f; state[x][y] = 2; } }
                    else if (state[x][y] == 3) { zPos[x][y] += returnSpeed; if (zPos[x][y] >= RNDR_Z - 1.0f) { zPos[x][y] = RNDR_Z - 1.0f; state[x][y] = 0; } }
                    if (state[x][y] == 0) atTop++; if (state[x][y] == 2) atBottom++;
                    float zRatio = zPos[x][y] / (RNDR_Z - 1.0f);
                    CRGB color = blend(CRGB::Blue, CRGB::Red, (uint8_t)(zRatio * 255.0f));
                    int zInt = (int)roundf(zPos[x][y]); setVoxel(x, y, zInt, color);
                    if (state[x][y] == 1 && zInt + 1 < RNDR_Z) { CRGB tail = color; tail.nscale8(80); setVoxel(x, y, zInt + 1, tail); }
                    else if (state[x][y] == 3 && zInt - 1 >= 0) { CRGB tail = color; tail.nscale8(80); setVoxel(x, y, zInt - 1, tail); }
                }
            }
            if (!returnPhase && atBottom == totalVoxels) { showCube(); delay(600); returnPhase = true; }
            else if (returnPhase && atTop == totalVoxels) { showCube(); delay(600); returnPhase = false; }
            showCube(); yield();
        }
    }

    static void animateBpmVolumetric(uint32_t durationMs, CRGBPalette16 pal = RainbowColors_p,
                                     uint8_t effectSpeed = 64, BpmSpatialMode mode = BPM_CHECKERBOARD) {
        // Preserve the original timed animation. The audio wrapper shares its
        // geometry renderer, but supplies an independently tracked music phase.
        const uint8_t bpm = map(effectSpeed, 0, 255, 10, 120);
        uint8_t *phaseMap = reinterpret_cast<uint8_t *>(SharedAppMemory);
        BpmRenderer::prepare(phaseMap, mode);
        const uint32_t startTime = millis();

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            const uint32_t now = millis();
            const uint8_t beat = beatsin8(bpm, 64, 255);
            const uint8_t timeBase = now / 15;
            BpmRenderer::draw(phaseMap, pal, uint8_t(beat - timeBase), timeBase);
            // showCube() owns frame pacing and the mode-change checkpoint.
            // A second delay here needlessly adds to LED transfer time.
            showCube();
        }
    }

    // Based on WLED's Tartan (Elliott Kember, modified by Andrew Tuline).
    // The three stripe profiles are evaluated separately, then added in RGB.
    static void animateTartan(uint32_t durationMs, uint32_t stepMs = 24) {
        constexpr float bandCutoff = 0.35f;
        const CRGBPalette16 palette = RainbowColors_p;
        const uint32_t started = millis();
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            const uint32_t elapsed = millis() - started;
            const auto oscillate = [elapsed](uint32_t periodMs, float phase) {
                return sinf(TWO_PI * float(elapsed % periodMs) / periodMs + phase);
            };
            const float driftX = 360.0f * oscillate(20000, 0.0f);
            const float driftY = 360.0f * oscillate(30000, PI / 2.0f);
            const float driftZ = 360.0f * oscillate(15000, PI);
            const float hueSpread = 5.5f + 2.5f * oscillate(6000, 0.0f);
            const auto stripe = [&palette](int position, int span, float phaseSpan,
                                           float drift, float hueOffset, float hueStep) {
                // Keep fractional stripe positions through the turn. Use
                // only the part above the cutoff, with soft shoulders.
                const float phase = position * phaseSpan / span + drift;
                const float wave = sinf(phase * (TWO_PI / 256.0f));
                const float crest = max(0.0f, (wave - bandCutoff) / (1.0f - bandCutoff));
                const uint8_t brightness = uint8_t(144.0f * crest * crest * (2.0f - crest));
                const uint8_t hue = uint8_t(lroundf(hueOffset + position * (16.0f * hueStep) / span));
                return ColorFromPalette(palette, hue, brightness, LINEARBLEND);
            };

            CRGB across[RNDR_X], depth[RNDR_Y], height[RNDR_Z];
            for (int x = 0; x < RNDR_X; ++x) {
                across[x] = stripe(x, RNDR_X, 512, driftX, driftY / 3.0f, hueSpread);
            }
            for (int y = 0; y < RNDR_Y; ++y) {
                depth[y] = stripe(y, RNDR_Y, 640, driftY, driftZ / 3.0f + 85, 3);
            }
            for (int z = 0; z < RNDR_Z; ++z) {
                height[z] = stripe(z, RNDR_Z, 384, driftZ, driftX / 3.0f + 171, 7);
            }

            for (int z = 0; z < RNDR_Z; ++z) {
                for (int y = 0; y < RNDR_Y; ++y) {
                    const CRGB yz = depth[y] + height[z];
                    for (int x = 0; x < RNDR_X; ++x) setVoxel(x, y, z, across[x] + yz);
                }
            }
            showCube();
            delay(stepMs);
        }
    }
};
