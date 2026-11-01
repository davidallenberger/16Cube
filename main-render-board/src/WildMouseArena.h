#pragma once

#include "CubeCore.h"
#include "AppMemory.h"
#include "TextureEngine.h"
#include <new>
#include <type_traits>

// Test-only equivalent of Streamers::animateWildMouse. The movement, RNG call
// order, colors, tail fading, draw order and delays are retained. Only storage
// moves from large local arrays to the already allocated shared arena.
class WildMouseArena {
private:
    struct State {
        uint8_t hx[16][64];
        uint8_t hy[16][64];
        uint8_t hz[16][64];
        int headPtr[16];
        int x[16];
        int y[16];
        int z[16];
        int dir[16];
        int axis[16];
        CRGBPalette16 mousePals[16];
    };

    static_assert(sizeof(State) <= SHARED_APP_MEMORY_BYTES, "Wild Mouse state does not fit the existing arena");
    static_assert(std::is_trivially_destructible<State>::value, "Arena state must not require destructor cleanup on a mode jump");

public:
    static constexpr size_t stateBytes() {
        return sizeof(State);
    }

    static void animate(uint32_t durationMs, uint32_t speedMs) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        float masterPhase = 0.0f;
        int numMice = min(max(1, (RNDR_X * RNDR_Y * RNDR_Z) / 512), 16);
        const uint8_t MAX_TAIL = 64;
        uint8_t tailLength = min((int)(RNDR_X * 3), (int)MAX_TAIL);
        // This placement construction reuses the existing shared arena. It
        // does not allocate from the heap or enlarge the arena. Primitive
        // fields used below are initialized by the same loops as the original.
        State& state = *new (SharedAppMemory) State;
        auto& hx = state.hx;
        auto& hy = state.hy;
        auto& hz = state.hz;
        auto& headPtr = state.headPtr;
        auto& x = state.x;
        auto& y = state.y;
        auto& z = state.z;
        auto& dir = state.dir;
        auto& axis = state.axis;
        auto& mousePals = state.mousePals;
        for (int m = 0; m < numMice; m++) {
            x[m] = random8(RNDR_X);
            y[m] = random8(RNDR_Y);
            z[m] = random8(RNDR_Z);
            dir[m] = (random8(2) == 0) ? 1 : -1;
            axis[m] = random8(3);
            headPtr[m] = 0;
            // 50/50 mix: Solid vibrant colors OR flowing organic gradients
            if (random8(2) == 0) {
                mousePals[m] = CRGBPalette16(PaletteUtils::getRandomVibrantColor());
            }
            else {
                mousePals[m] = PaletteUtils::getRandomOrganicPalette();
            }
            for (int i = 0; i < tailLength; i++) {
                hx[m][i] = x[m];
                hy[m][i] = y[m];
                hz[m][i] = z[m];
            }
        }
        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrame;
            lastFrame = now;
            masterPhase += 0.133f * deltaMs;
            for (int m = 0; m < numMice; m++) {
                switch (axis[m]) {
                    case 0:
                        if (dir[m] == 1) {
                            x[m]++;
                            if (x[m] >= RNDR_X - 1) {
                                x[m] = RNDR_X - 1;
                                axis[m] = 1;
                                dir[m] = (y[m] < RNDR_CY) ? 1 : -1;
                            }
                        }
                        else {
                            x[m]--;
                            if (x[m] <= 0) {
                                x[m] = 0;
                                axis[m] = 2;
                                dir[m] = (z[m] < RNDR_CZ) ? 1 : -1;
                            }
                        }
                        if (random8(5) == 0) {
                            axis[m] = (random8(2) == 0) ? 1 : 2;
                        }
                        break;
                    case 1:
                        if (dir[m] == 1) {
                            y[m]++;
                            if (y[m] >= RNDR_Y - 1) {
                                y[m] = RNDR_Y - 1;
                                axis[m] = 2;
                                dir[m] = (z[m] < RNDR_CZ) ? 1 : -1;
                            }
                        }
                        else {
                            y[m]--;
                            if (y[m] <= 0) {
                                y[m] = 0;
                                axis[m] = 0;
                                dir[m] = (x[m] < RNDR_CX) ? 1 : -1;
                            }
                        }
                        if (random8(5) == 0) {
                            axis[m] = (random8(2) == 0) ? 2 : 0;
                        }
                        break;
                    case 2:
                        if (dir[m] == 1) {
                            z[m]++;
                            if (z[m] >= RNDR_Z - 1) {
                                z[m] = RNDR_Z - 1;
                                axis[m] = 0;
                                dir[m] = (x[m] < RNDR_CX) ? 1 : -1;
                            }
                        }
                        else {
                            z[m]--;
                            if (z[m] <= 0) {
                                z[m] = 0;
                                axis[m] = 1;
                                dir[m] = (y[m] < RNDR_CY) ? 1 : -1;
                            }
                        }
                        if (random8(5) == 0) {
                            axis[m] = (random8(2) == 0) ? 0 : 1;
                        }
                        break;
                }
                headPtr[m] = (headPtr[m] + 1) % tailLength;
                hx[m][headPtr[m]] = x[m];
                hy[m][headPtr[m]] = y[m];
                hz[m][headPtr[m]] = z[m];
            }
            clearAll();
            for (int m = 0; m < numMice; m++) {
                for (int i = 0; i < tailLength; i++) {
                    int age = (headPtr[m] - i + tailLength) % tailLength;
                    uint8_t px = hx[m][i];
                    uint8_t py = hy[m][i];
                    uint8_t pz = hz[m][i];
                    uint8_t colorIndex = (px * 16) + (py * 16) + (pz * 16) + (uint32_t)masterPhase;
                    // Pull color from this specific mouse's assigned palette array
                    CRGB color = ColorFromPalette(mousePals[m], colorIndex, 255, LINEARBLEND);
                    uint8_t brightness = 255 - (age * (255 / tailLength));
                    if (brightness > 10) {
                        color.nscale8(brightness);
                        // Additive blending allows beams to mix white-hot when crossing paths
                        setVoxel(px, py, pz, getVoxel(px, py, pz) + color);
                    }
                }
            }
            showCube();
            if (speedMs > 0) {
                delay(speedMs);
            } else {
                yield();
            }
        }
    }
};
