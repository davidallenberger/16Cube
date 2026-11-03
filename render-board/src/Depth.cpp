#include "Depth.h"
#include "UARTReceiver.h"

// Depth is a separate input/animation family. Its occupancy grid is unrelated
// to the audio spectrum and is deliberately kept out of the Audio class.
void Depth::run () {
    animateOccupancy ();
}

void Depth::renderIcon (TextureState &) {
    setVoxel (RNDR_X / 2, RNDR_Y / 2, RNDR_Z / 2, CRGB::Green);
}
void Depth::animateOccupancy () {
    static uint8_t bits[CubinaWire::DEPTH_BYTES];
    for (;;) {
        clearAll ();
        if (RemoteInputs::depth (bits)) {
            for (unsigned z = 0; z < 16; ++z) {
                for (unsigned y = 0; y < 16; ++y) {
                    for (unsigned x = 0; x < 16; ++x) {
                        if (CubinaWire::voxel (bits, x, y, z)) {
                            // Scale the 16^3 logical occupancy onto the current 10x10x16 or
                            // 16^3 canvas. Multiple occupied voxels may merge on a cuboid.
                            setVoxel (x * RNDR_X / 16, y * RNDR_Y / 16, z * RNDR_Z / 16,
                                      CRGB::Green);
                        }
                    }
                }
            }
        }
        showCube ();
    }
}
