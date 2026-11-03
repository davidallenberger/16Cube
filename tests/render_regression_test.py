#!/usr/bin/env python3
"""Host checks for the production bouncing-balls body and marquee test route.

Only these production functions are extracted into small host harnesses.
This exercises the actual function bodies without stubbing every unrelated
animation in Streamers.h/TestDemo.cpp. It does not emulate ESP32 LED timing.
"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'main-render-board/src'
COMPILER = os.environ.get('CXX', 'g++')


def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 0

    for position in range(opening, len(text)):
        if text[position] == '{':
            depth += 1
        elif text[position] == '}':
            depth -= 1
            if depth == 0:
                return text[start:position + 1]

    raise RuntimeError('Unclosed production function: ' + signature)


BALL_HARNESS = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
using std::min;
using std::max;
constexpr int RNDR_X = CANVAS;
constexpr int RNDR_Y = CANVAS;
constexpr int RNDR_Z = 16;
constexpr int SHARED_APP_MEMORY_BYTES = 8192;
alignas(16) uint8_t arena[SHARED_APP_MEMORY_BYTES + 32];
uint8_t *SharedAppMemory = arena;
struct CRGB {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    CRGB() {}
    CRGB(int red, int green, int blue): r(red), g(green), b(blue) {}
    CRGB operator+(CRGB other) const {
        return CRGB(min(255, int(r) + other.r), min(255, int(g) + other.g),
                    min(255, int(b) + other.b));
    }
};
struct CRGBPalette16 { CRGB entries[16]; };
CRGBPalette16 RainbowColors_p;
constexpr int LINEARBLEND = 1;
unsigned colorCalls = 0;
CRGB ColorFromPalette(const CRGBPalette16 &pal, uint8_t index, int, int) {
    ++colorCalls;
    // Respect the palette's interpolation contract. On the 8x8 canvas,
    // the ten-ball setting can select positions between palette entries.
    const CRGB first = pal.entries[index / 16];
    const CRGB second = pal.entries[(index / 16 + 1) % 16];
    const unsigned fraction = index % 16;
    return CRGB((first.r * (16 - fraction) + second.r * fraction) / 16,
                (first.g * (16 - fraction) + second.g * fraction) / 16,
                (first.b * (16 - fraction) + second.b * fraction) / 16);
}
uint32_t nowMs = 0;
unsigned frames = 0;
unsigned lit = 0;
unsigned writes = 0;
uint32_t millis() { return nowMs++; }
void delay(int ms) { nowMs += ms; }
void yield() {}
uint8_t random8(int low, int) { return low; }
int constrain(int value, int low, int high) { return max(low, min(value, high)); }
void clearAll() {}
void showCube() { ++frames; }
CRGB getVoxel(int, int, int) { return CRGB(); }
void setVoxel(int x, int y, int z, CRGB color) {
    assert(x >= 0 && x < RNDR_X);
    assert(y >= 0 && y < RNDR_Y);
    assert(z >= 0 && z < RNDR_Z);
    ++writes;
    if (color.r || color.g || color.b) {
        ++lit;
    }
}
class Streamers {
public:
__BALL_CODE__
};
int main() {
    static_assert(sizeof(Streamers::BouncingBall) == 12, "Test matches ESP32 ball size");
    const CRGB starts[] = {
        CRGB(0, 0, 0),      // Lava: entry zero black, entry one maroon.
        CRGB(25, 25, 112),  // Ocean.
        CRGB(0, 100, 0),    // Forest.
        CRGB(85, 0, 171),   // Party.
        CRGB(0, 0, 0)       // Intentionally all-black palette.
    };
    unsigned cases = 0;
    for (int palette = 0; palette < 5; ++palette) {
        CRGBPalette16 pal;
        for (int entry = 0; entry < 16; ++entry) {
            pal.entries[entry] = palette == 4 ? CRGB() : CRGB(128, 16, 8);
        }
        pal.entries[0] = starts[palette];
        const uint8_t densities[] = {0, 128, 255};
        for (uint8_t density : densities) {
            memset(arena, 0xA5, sizeof(arena));
            nowMs = 0;
            frames = 0;
            writes = 0;
            lit = 0;
            colorCalls = 0;
            Streamers::animateBouncingBalls(200, pal, 128, density);
            assert(frames > 0);
            assert(writes > 0);
            assert(palette == 4 ? lit == 0 : lit == writes);
            const unsigned capacity = 8192 / (RNDR_X * RNDR_Y * 12);
            const unsigned requested = CANVAS == 10 ? 6 : (CANVAS > 10 ? 9 : 10);
            const unsigned maximum = min(capacity, requested);
            const unsigned selected = density * (maximum - 1) / 255 + 1;
            assert(writes == frames * RNDR_X * RNDR_Y * selected);
            for (unsigned byte = 8192; byte < sizeof(arena); ++byte) {
                assert(arena[byte] == 0xA5);
            }
            ++cases;
        }
    }
    printf("Bouncing balls: %u cases on %dx%d; visibility and arena bounds pass.\n",
           cases, RNDR_X, RNDR_Y);
}
'''


MARQUEE_HARNESS = r'''
#include <cassert>
#include <cstring>
#include "TestDemo.h"
constexpr int SPEED_TEXT_ICONS = 95;
constexpr int OceanColors_p = 7;
constexpr int MODE_NOISE_FIELD = 5;
struct CRGB { enum { Gold = 42 }; };
struct SerialStub {
    unsigned logs = 0;
    void println(const char *) { ++logs; }
} Serial;
struct EndTest {};
unsigned marqueeCalls = 0;
unsigned engineCalls = 0;
unsigned stopCalls = 0;
namespace Letters {
    void scrollTextCycles(const char *text, int cycles, float sx, float sy,
                          int speed, int fg, int bg, int shader, bool overrideColor,
                          int dimPercent) {
        assert(strcmp(text, "`R30` KINKY PIZZA `R30`") == 0);
        assert(cycles == 2 && sx == 1.0f && sy == 1.0f);
        assert(speed == 95 && fg == CRGB::Gold && bg == OceanColors_p);
        assert(shader == MODE_NOISE_FIELD && !overrideColor && dimPercent == 75);
        if (++marqueeCalls == 3) {
            throw EndTest();
        }
    }
}
namespace TestDemo {
    void stopPlayback() { ++stopCalls; }
    PlaybackOrder &animationOrder() {
        static PlaybackOrder order = PlaybackOrder::Shuffled;
        return order;
    }
    void runEngine(bool) {
        ++engineCalls;
        throw EndTest();
    }
__MARQUEE_CODE__
}
int main() {
    try { TestDemo::runAnimation(); } catch (EndTest) {}
    assert(stopCalls == 1);
#if CUBINA_TEST_KINKY_PIZZA_ONLY
    assert(marqueeCalls == 3 && engineCalls == 0 && Serial.logs == 1);
#else
    assert(marqueeCalls == 0 && engineCalls == 1 && Serial.logs == 0);
#endif
    marqueeCalls = 0;
    try { TestDemo::runKinkyPizzaTest(); } catch (EndTest) {}
    assert(marqueeCalls == 3);
}
'''


def compile_and_run(folder, name, content, *flags):
    source = folder / (name + '.cpp')
    binary = folder / name
    source.write_text(content)
    subprocess.run([COMPILER, '-std=c++11', '-fsanitize=address,undefined', '-g',
                    '-I', str(SOURCE), *flags, str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))


def main():
    if not shutil.which(COMPILER):
        raise SystemExit('Install a C++ compiler or set CXX to its executable.')

    streamers = (SOURCE / 'Streamers.h').read_text()
    beginning = streamers.index('    struct BouncingBall {')
    ending = streamers.index('    static void animateBpmVolumetric(', beginning)
    ball_code = streamers[beginning:ending]
    demo = (SOURCE / 'TestDemo.cpp').read_text()
    marquee_code = function(demo, 'void runKinkyPizzaTest()') + '\n' + function(demo, 'void runAnimation()')

    with tempfile.TemporaryDirectory(prefix='cubina-render-regression-') as temp:
        folder = Path(temp)
        for canvas in (16, 10, 8):
            compile_and_run(folder, 'balls_' + str(canvas),
                            BALL_HARNESS.replace('__BALL_CODE__', ball_code),
                            '-DCANVAS=' + str(canvas))
        for enabled in (0, 1):
            compile_and_run(folder, 'marquee_' + str(enabled),
                            MARQUEE_HARNESS.replace('__MARQUEE_CODE__', marquee_code),
                            '-DCUBINA_TEST_KINKY_PIZZA_ONLY=' + str(enabled))
        print('Marquee route: direct calls and both boot-switch settings pass.')


if __name__ == '__main__':
    main()
