// Unlike the animation playback tests, this test uses the actual CubeCore
// mapping and physical strip arrays. It checks the values handed to FastLED,
// including the sacrificial/spacer LEDs that must remain completely black.
#include "CubeCore.cpp"
#include "Streamers.h"
#include <array>
#include <cassert>
#include <cstdio>

SerialStub Serial;
SerialStub Serial2;
FastLEDStub FastLED;
CRGB CRGB::Black(0, 0, 0);
CRGB CRGB::Green(0, 255, 0);
CRGB CRGB::Yellow(255, 255, 0);
CRGB CRGB::Red(255, 0, 0);
CRGB CRGB::White(255, 255, 255);
CRGB CRGB::Blue(0, 0, 255);
CRGB CRGB::Purple(128, 0, 128);

unsigned long millis() { return 100; }
uint32_t micros() { return 100000; }
void delay(int) {}
void delayMicroseconds(unsigned) {}
uint32_t esp_random() { return 1; }
void CubeSystem::frameCheckpoint() {}

static bool black(const CRGB &pixel) {
    return pixel.r == 0 && pixel.g == 0 && pixel.b == 0;
}

int main() {
    const BpmSpatialMode modes[] = {
        BPM_CHECKERBOARD, BPM_SPHERE, BPM_DIAGONAL, BPM_HELIX
    };
    const uint8_t levels[] = {0, 1, 32, 80, 128, 255};
    const uint8_t offsets[] = {0, 1, 31, 127, 255};
    constexpr size_t VOXELS = RNDR_X * RNDR_Y * RNDR_Z;
    std::array<uint8_t, VOXELS + 32> scratch;

    CRGB *strips[] = {strip0, strip1, strip2, strip3, strip4, strip5, strip6, strip7};
    const size_t lengths[] = {
        STRIP0_LEN, STRIP1_LEN, STRIP2_LEN, STRIP3_LEN,
        STRIP4_LEN, STRIP5_LEN, STRIP6_LEN, STRIP7_LEN
    };
    unsigned frames = 0;

    for (BpmSpatialMode mode : modes) {
        scratch.fill(0xA5);
        BpmRenderer::prepare(scratch.data(), mode);

        for (size_t index = VOXELS; index < scratch.size(); ++index) {
            assert(scratch[index] == 0xA5);
        }

        for (uint8_t level : levels) {
            for (uint8_t offset : offsets) {
                clearAll();
                BpmRenderer::draw(scratch.data(), RainbowColors_p, offset, 37, level);

                for (unsigned strip = 0; strip < 8; ++strip) {
                    for (size_t index = 0; index < lengths[strip]; ++index) {
                        // Six lanes have one spacer per 16 vertical pixels.
                        // The final pair also has three extra wiring pixels
                        // after each 272-pixel plane, hence a 275 stride.
                        const size_t local = strip < 6 ? index : index % 275;
                        const bool spacer = local % 17 == 16 ||
                                            (strip >= 6 && local >= 272);
                        if (spacer || level == 0) {
                            assert(black(strips[strip][index]));
                        }
                    }
                }
                ++frames;
            }
        }
    }

    printf("BPM physical-buffer test passed: %u frames, %ux%ux%u canvas; spacers stay black.\n",
           frames, RNDR_X, RNDR_Y, RNDR_Z);
}
