// Compile the production scanner against the REAL CubeCore strip arrays and
// mapper. Only Arduino/FastLED hardware services are stubbed on the host.
#include <Arduino.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

struct RecordingSerial {
    std::vector<std::string> lines;

    void println(const char *text) {
        lines.push_back(std::string(text) + "\n");
    }

    template<class... Args>
    void printf(const char *format, Args... args) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), format, args...);
        lines.push_back(buffer);
    }
};

RecordingSerial diagnosticSerial;
#define Serial diagnosticSerial
#include "CubeCore.cpp"
#include "SpacerCheck.cpp"
#undef Serial

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

uint32_t clockUs = 100000;
unsigned long millis() { return clockUs / 1000; }
uint32_t micros() { return clockUs++; }
void delay(int ms) { clockUs += ms * 1000; }
void delayMicroseconds(unsigned us) { clockUs += us; }
uint32_t esp_random() { return 1; }
void CubeSystem::frameCheckpoint() {}

static SpacerCheck::SceneInfo scene(const char *name, const char *palette) {
    SpacerCheck::SceneInfo info;
    info.name = name;
    info.render = "Solid";
    info.motion = "Static";
    info.shader = "Noise Field";
    info.palette = palette;
    info.step = 3;
    info.totalSteps = 56;
    info.ordered = false;
    return info;
}

static void expect(const std::string &text, const char *part) {
    assert(text.find(part) != std::string::npos);
}

int main() {
    // Startup/menu shows outside a dispatcher invocation must not be charged
    // to the first animation and must never produce serial diagnostic output.
    showCube();
    assert(SpacerCheck::record.frames == 0);
    assert(diagnosticSerial.lines.empty());

    clearAll();
    SpacerCheck::beginScene(scene("All voxels", "Original palette"));
    for (unsigned x = 0; x < RNDR_X; ++x) {
        for (unsigned y = 0; y < RNDR_Y; ++y) {
            for (unsigned z = 0; z < RNDR_Z; ++z) {
                setVoxel(x, y, z, CRGB(255, 127, 63));
            }
        }
    }
    showCube();
    assert(SpacerCheck::record.frames == 1);
    assert(SpacerCheck::record.spacers == 265);
    assert(SpacerCheck::record.badFrames == 0);
    assert(diagnosticSerial.lines.empty());
    SpacerCheck::endScene();
    assert(diagnosticSerial.lines.size() == 1);
    expect(diagnosticSerial.lines.back(), "previous=\"<boot>\"");
    expect(diagnosticSerial.lines.back(), "frames=1 spacers=265 badFrames=0 badSamples=0 first=[none]");
    expect(diagnosticSerial.lines.back(), "UART=0 DMA=2 diagBytes=");
    expect(diagnosticSerial.lines.back(), "status=complete");
    SpacerCheck::endScene();
    assert(diagnosticSerial.lines.size() == 1);

    CRGB *strips[] = {strip0, strip1, strip2, strip3, strip4, strip5, strip6, strip7};
    const unsigned lengths[] = {544, 544, 544, 544, 544, 544, 550, 547};
    bool mapped[8][550] = {};
    for (unsigned x = 0; x < RNDR_X; ++x) {
        for (unsigned y = 0; y < RNDR_Y; ++y) {
            for (unsigned z = 0; z < RNDR_Z; ++z) {
                const MappedPixel pixel = mapXYZ(x, y, z);
                assert(pixel.ok);
                mapped[pixel.strip][pixel.index] = true;
            }
        }
    }

    char mutablePalette[] = "Captured palette";
    SpacerCheck::beginScene(scene("Every spacer", mutablePalette));
    strcpy(mutablePalette, "Changed label");
    clearAll();
    unsigned holes = 0;
    for (unsigned output = 0; output < 8; ++output) {
        for (unsigned index = 0; index < lengths[output]; ++index) {
            if (mapped[output][index]) {
                continue;
            }
            strips[output][index] = CRGB(1, 255, 3);
            showCube();
            ++holes;
            assert(SpacerCheck::record.frames == holes);
            assert(SpacerCheck::record.badFrames == holes);
            assert(SpacerCheck::record.badSamples == holes);
            // Inspection must not repair, dim or otherwise change this pixel.
            assert(strips[output][index].r == 1);
            assert(strips[output][index].g == 255);
            assert(strips[output][index].b == 3);
            strips[output][index] = CRGB::Black;
        }
    }
    assert(holes == 265);
    assert(SpacerCheck::record.firstOutput == 0);
    assert(SpacerCheck::record.firstIndex == 16);
    assert(SpacerCheck::record.firstBadFrame == 1);
    assert(diagnosticSerial.lines.size() == 1);
    SpacerCheck::endScene();
    assert(diagnosticSerial.lines.size() == 2);
    expect(diagnosticSerial.lines.back(), "previous=\"All voxels\"");
    expect(diagnosticSerial.lines.back(), "palette=\"Captured palette\"");
    expect(diagnosticSerial.lines.back(), "badFrames=265 badSamples=265");
    expect(diagnosticSerial.lines.back(), "out=0,index=16,frame=1,rgb=(1,255,3)");

    // Multiple bad spacers are one bad FRAME, but several bad observations.
    SpacerCheck::beginScene(scene("Multiple", "Ocean"));
    strip0[16] = CRGB(0, 128, 0);
    strip7[274] = CRGB(255, 0, 0);
    showCube();
    assert(SpacerCheck::record.badFrames == 1);
    assert(SpacerCheck::record.badSamples == 2);
    SpacerCheck::endScene();
    assert(diagnosticSerial.lines.size() == 3);
    clearAll();

    // A replaced invocation receives one interrupted summary on the NEXT
    // begin call, outside showCube(). No destructor or stack unwinding needed.
    SpacerCheck::beginScene(scene("Abandoned", "Lava"));
    showCube();
    SpacerCheck::beginScene(scene("Replacement", "Forest"));
    assert(diagnosticSerial.lines.size() == 4);
    expect(diagnosticSerial.lines.back(), "status=interrupted");
    SpacerCheck::endScene();
    expect(diagnosticSerial.lines.back(), "previous=\"Abandoned\"");
    expect(diagnosticSerial.lines.back(), "frames=0 spacers=0");

    printf("Spacer check passed: real 4096-voxel mapper, all 265 holes individually detected, counters, palette capture, predecessor, no repairs or in-frame logging.\n");
    printf("Diagnostic state: %zu bytes of host SceneRecord; pointer sizes differ on ESP32.\n", sizeof(SpacerCheck::record));
}
