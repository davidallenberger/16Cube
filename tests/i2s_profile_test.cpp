#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
#define DRAM_ATTR
#define IRAM_ATTR
#include "i2s_probe_fragment.h"

int main() {
    memset((void *)&gCubinaI2sProbe, 0, sizeof(gCubinaI2sProbe));
    gCubinaI2sProbe.enabled = true;
    gCubinaI2sProbe.cpuMHz = 10;
    cubinaI2sProbeNewShow();
    cubinaI2sProbeTxStart(0xffffff00u);
    cubinaI2sProbeEnter(0xfffffff0u);
    cubinaI2sProbeExit(0xfffffff0u, 34);
    cubinaI2sProbeEnter(uint32_t(0xfffffff0u + 300));
    assert(gCubinaI2sProbe.maximumGapCycles == 300);
    assert(gCubinaI2sProbe.maximumFillCycles == 50);
    assert(gCubinaI2sProbe.gapsOver40us == 0);
    assert(gCubinaI2sProbe.maximumFirstCycles == 240);
    cubinaI2sProbeEnter(uint32_t(0xfffffff0u + 800));
    assert(gCubinaI2sProbe.maximumGapCycles == 500);
    assert(gCubinaI2sProbe.gapsOver40us == 1);
    cubinaI2sProbeNewShow();
    cubinaI2sProbeTxStart(9999500);
    cubinaI2sProbeEnter(10000000);
    assert(gCubinaI2sProbe.maximumGapCycles == 500);
    assert(gCubinaI2sProbe.gapsOver40us == 1);
    assert(gCubinaI2sProbe.firstOver40us == 1);
    cubinaI2sProbeExit(0, 301);
    assert(gCubinaI2sProbe.fillsOver30us == 1);
    gCubinaI2sProbe.fillCycles = 0xfffffff0ull;
    cubinaI2sProbeExit(100, 200);
    assert(gCubinaI2sProbe.fillCycles == 0x100000054ull);
    puts("I2S counters passed: cycle wrap, 64-bit totals, gap threshold and exclusion of inter-frame pauses.");
}
