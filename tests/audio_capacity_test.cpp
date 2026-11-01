#include <Arduino.h>
#include <string>
#include <vector>
#include <cassert>
#include <cstdio>

static bool capturing = false;
struct CaptureSerial {
    unsigned writes = 0;
    unsigned prints = 0;
    void flush() {}
    int availableForWrite() { return 1024; }
    size_t write(uint8_t *, size_t size) {
        if (capturing) { ++writes; }
        return size;
    }
    template <class... T> void printf(const char *, T...) {
        if (capturing) { ++prints; }
    }
};
static CaptureSerial capture;
#define Serial capture
#include "Audio.cpp"
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
static unsigned snapshots = 0;
static uint32_t fingerprint = 2166136261u;
static std::vector<uint32_t> colors;

struct Finished {};
uint32_t micros() { return clockUs; }
unsigned long millis() { return clockUs / 1000; }
void delay(int) {}
long random(long) { return 0; }
void fadeAll(uint8_t) {}
void CubeSystem::frameCheckpoint() {}
void bpmProfileStartDriver() { capturing = true; }
BpmDriverStats bpmProfileStopDriver() { capturing = false; return BpmDriverStats(); }
RemoteInputs::Control RemoteInputs::control() {
    Control result;
    result.mode = CubinaWire::Mode::AudioBluetooth;
    result.linked = true;
    result.ready = true;
    return result;
}
RemoteInputs::Stats RemoteInputs::stats() { return Stats(); }
RemoteInputs::Audio RemoteInputs::audio() {
    ++snapshots;
    Audio result;
    result.valid = true;
    result.receivedMs = millis();
    result.values.volumeSm = 100;
    result.values.tempoBpm100 = 12800;
    result.values.tempoPhase = uint16_t((uint64_t(clockUs) * 128 * 65536 / 60000000) % 65536);
    result.values.tempoFlags = CubinaWire::AUDIO_ACTIVE | CubinaWire::TEMPO_LOCKED;
    return result;
}
void setVoxel(uint8_t x, uint8_t y, uint8_t z, const CRGB &pixel) {
    assert(x < RNDR_X && y < RNDR_Y && z < RNDR_Z);
    fingerprint = (fingerprint ^ pixel.r) * 16777619u;
    fingerprint = (fingerprint ^ pixel.g) * 16777619u;
    fingerprint = (fingerprint ^ pixel.b) * 16777619u;
}
void showCube() {
    colors.push_back(fingerprint);
    fingerprint = 2166136261u;
    clockUs += 21000;
    if (++frames == 160) { throw Finished(); }
}
static std::vector<uint32_t> run(void (*method)(), bool expectUsb) {
    capturing = false;
    capture.writes = 0;
    capture.prints = 0;
    clockUs = 1000000;
    frames = 0;
    snapshots = 0;
    fingerprint = 2166136261u;
    colors.clear();
    try {
        method();
        assert(false && "Loop unexpectedly returned");
    } catch (const Finished &) {}
    BpmProfile::finish();
    assert(snapshots == 160);
    assert((capture.writes != 0) == expectUsb);
    // Entry/report printf calls occur outside the capture. No hidden periodic
    // formatting/output leaks into the three quiet cases.
    assert(capture.prints == 0);
    return colors;
}
int main() {
    const auto quiet = run(&Audio::profileRealQuiet, false);
    const auto debug = run(&Audio::profileRealDebug, true);
    assert(quiet == debug);
    const auto copy = run(&Audio::profileSnapshotFixed, false);
    const auto math = run(&Audio::profileMathFixed, false);
    assert(copy == math);
    assert(quiet != copy);
    const char *expected[] = {"A Real BPM quiet", "B Real BPM USB debug", "C Snapshot fixed BPM", "D Audio math fixed BPM"};
    for (const char *name : expected) {
        assert(std::string(Audio::currentRoutineName()) == name);
        Audio::nextRoutine();
    }
    assert(std::string(Audio::currentRoutineName()) == expected[0]);
    puts("Audio cases passed: real quiet/debug colors match, fixed snapshot/math colors match, one snapshot/frame and four-case dispatch.");
}
