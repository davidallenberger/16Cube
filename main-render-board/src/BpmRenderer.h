#pragma once

#include "CubeCore.h"
#include <math.h>

// Shared geometry/drawing for timed Streamers and the audio-controlled wrapper.
// These modes describe spatial arrangement, independently of musical timing.
enum BpmSpatialMode { BPM_CHECKERBOARD = 0, BPM_SPHERE = 1, BPM_DIAGONAL = 2, BPM_HELIX = 3 };

class BpmRenderer {
  public:
    // Caller owns a scratch byte per rendered voxel. Prepare once on entry,
    // then retain the map for every frame. No allocation or task is created.
    static void prepare(uint8_t *phaseMap, BpmSpatialMode mode) {
        const float maxDist =
            fmaxf(1.0f, sqrtf(RNDR_CX * RNDR_CX + RNDR_CY * RNDR_CY + RNDR_CZ * RNDR_CZ));
        const uint8_t zStep = 256 / RNDR_Z;
        const uint8_t diagStep = 256 / (RNDR_X + RNDR_Y + RNDR_Z);
        size_t index = 0;

        for (uint8_t z = 0; z < RNDR_Z; ++z) {
            for (uint8_t y = 0; y < RNDR_Y; ++y) {
                for (uint8_t x = 0; x < RNDR_X; ++x) {
                    uint8_t spatial = 0;
                    switch (mode) {
                    case BPM_CHECKERBOARD:
                        spatial = (x + y) % 2 == 0 ? z * zStep : 256 - z * zStep;
                        break;
                    case BPM_SPHERE: {
                        const float dx = x - RNDR_CX;
                        const float dy = y - RNDR_CY;
                        const float dz = z - RNDR_CZ;
                        spatial = uint8_t(sqrtf(dx * dx + dy * dy + dz * dz) / maxDist * 255.0f);
                        break;
                    }
                    case BPM_DIAGONAL:
                        spatial = (x + y + z) * diagStep;
                        break;
                    case BPM_HELIX: {
                        const float angle = atan2f(y - RNDR_CY, x - RNDR_CX);
                        spatial = uint8_t((angle + 3.14159f) * 40.74f) + z * zStep;
                        break;
                    }
                    }
                    phaseMap[index++] = spatial;
                }
            }
        }
    }

    // brightnessOffset moves the density pattern; colorPhase moves the palette.
    // Timed Streamers passes its legacy offsets. Audio supplies a phase driven
    // by the musical clock, plus a slowly changing decorative palette phase.
    static void draw(const uint8_t *phaseMap, const CRGBPalette16 &palette,
                     uint8_t brightnessOffset, uint8_t colorPhase, uint8_t level = 255) {
        size_t index = 0;
        for (uint8_t z = 0; z < RNDR_Z; ++z) {
            for (uint8_t y = 0; y < RNDR_Y; ++y) {
                for (uint8_t x = 0; x < RNDR_X; ++x) {
                    const uint8_t spatial = phaseMap[index++];
                    uint8_t brightness = brightnessOffset + spatial;
                    brightness = qsub8(brightness, 32);
                    brightness = qadd8(brightness, brightness);
                    brightness = uint8_t(uint16_t(brightness) * level / 255);
                    const uint8_t colorIndex = colorPhase + (spatial >> 1);
                    setVoxel(x, y, z,
                             ColorFromPalette(palette, colorIndex, brightness, LINEARBLEND));
                }
            }
        }
    }
};
