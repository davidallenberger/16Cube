#include <cstdint>
#define IRAM_ATTR
#define DRAM_ATTR
#include "i2s_probe_fragment.h"
uint32_t getCpuFrequencyMhz();
#include "CubeCore.cpp"
#include <cassert>
#include <cstdio>

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
unsigned long millis() { return 0; }
uint32_t micros() { return 0; }
uint32_t getCpuFrequencyMhz() { return 240; }
void delay(int) {}
void delayMicroseconds(unsigned) {}
uint32_t esp_random() { return 1; }
void CubeSystem::frameCheckpoint() {}

int main() {
    bpmProfileStartDriver();
    assert(gCubinaI2sProbe.enabled);
    assert(gCubinaI2sProbe.cpuMHz == 240);
    cubinaI2sProbeNewShow();
    cubinaI2sProbeEnter(10);
    cubinaI2sProbeExit(10, 2410);
    cubinaI2sProbeEnter(7210);
    const BpmDriverStats result = bpmProfileStopDriver();
    assert(!gCubinaI2sProbe.enabled);
    assert(result.available && result.cpuMHz == 240);
    assert(result.refills == 1);
    assert(result.fillCycles == 2400);
    assert(result.maximumFillCycles == 2400);
    assert(result.maximumGapCycles == 7200);
    assert(result.gapsOver40us == 0);
    puts("CubeCore driver bridge passed: same-translation-unit counters reset, arm, disarm and copy correctly.");
}
