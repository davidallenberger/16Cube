#include <Arduino.h>
#include <vector>
#include <cassert>
#include <cstdio>

struct CaptureSerial {
    std::string output;
    int space = 64;
    int availableForWrite() { return space; }
    template <class... T> void printf(const char *, T...) {}
    size_t write(uint8_t *bytes, size_t count) {
        assert(space > 0 && count <= size_t(space) && count <= 64);
        output.append(reinterpret_cast<char *>(bytes), count);
        return count;
    }
};
static CaptureSerial probeSerial;
#define Serial probeSerial
#include "BpmDiagnostic.h"
#undef Serial

SerialStub Serial;
SerialStub Serial2;
FastLEDStub FastLED;
CRGB CRGB::Black;
CRGB CRGB::Green;
CRGB CRGB::Yellow;
CRGB CRGB::Red;
CRGB CRGB::White;
CRGB CRGB::Blue;
CRGB CRGB::Purple;
static uint8_t scratch[RNDR_X * RNDR_Y * RNDR_Z];
uint8_t *SharedAppMemory = scratch;

static uint32_t clockUs = 0;
static unsigned frames = 0;
static bool busyInput = false;
static bool blockUsb = false;
static uint32_t fingerprint = 2166136261u;
static std::vector<uint32_t> colors;

struct Finished {
};

unsigned long millis() { return clockUs / 1000; }
uint32_t micros() { return clockUs; }

RemoteInputs::Control RemoteInputs::control() {
    Control state;
    state.linked = true;
    state.ready = true;
    state.mode = busyInput ? CubinaWire::Mode::AudioBluetooth : CubinaWire::Mode::Animation;
    return state;
}

RemoteInputs::Stats RemoteInputs::stats() {
    Stats result;
    result.accepted = uint64_t(clockUs) * (busyInput ? 60 : 5) / 1000000;
    return result;
}

void setVoxel(uint8_t x, uint8_t y, uint8_t z, const CRGB &color) {
    assert(x < RNDR_X && y < RNDR_Y && z < RNDR_Z);
    fingerprint = (fingerprint ^ color.r) * 16777619u;
    fingerprint = (fingerprint ^ color.g) * 16777619u;
    fingerprint = (fingerprint ^ color.b) * 16777619u;
}

void showCube() {
    colors.push_back(fingerprint);
    fingerprint = 2166136261u;
    clockUs += 17000;
    probeSerial.space = blockUsb && frames < 100 ? 0 : 64;
    if (++frames == 180) {
        throw Finished();
    }
}

static std::vector<uint32_t> render(bool busy, bool block) {
    clockUs = 1000000;
    frames = 0;
    busyInput = busy;
    blockUsb = block;
    probeSerial.output.clear();
    probeSerial.space = block ? 0 : 64;
    colors.clear();
    fingerprint = 2166136261u;
    try {
        BpmDiagnostic::run();
        assert(false && "Diagnostic unexpectedly returned");
    } catch (const Finished &) {
    }
    assert(probeSerial.output.find("BPM_PROBE:") != std::string::npos);
    assert(probeSerial.output.find("RX/s=") != std::string::npos);
    return colors;
}

int main() {
    const auto quiet = render(false, false);
    const auto busy = render(true, false);
    assert(quiet == busy);
    const auto blocked = render(true, true);
    assert(busy == blocked);
    puts("Fixed BPM probe passed: identical colors under 5/60 packet rates, bounded USB "
         "writes and continued frames through USB backpressure.");
}
