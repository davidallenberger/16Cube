// CUBINA_I2S_PROFILE_BEGIN -- injected into pinned FastLED 3.6.0 only.
// Counter-only ISR instrumentation. No USB, locks, allocation or callbacks
// into application code. CubeCore reads it BETWEEN completed LED shows.
#define CUBINA_I2S_PROFILE_AVAILABLE 1

struct CubinaI2sProbeData {
    bool enabled;
    bool havePrevious;
    uint32_t cpuMHz;
    uint32_t previousEntry;
    uint32_t txStart;
    uint32_t refills;
    uint64_t fillCycles;
    uint32_t maximumFillCycles;
    uint32_t maximumGapCycles;
    uint32_t gapsOver40us;
    uint32_t maximumFirstCycles;
    uint32_t firstOver40us;
    uint32_t fillsOver30us;
};

static DRAM_ATTR volatile CubinaI2sProbeData gCubinaI2sProbe = {};

static inline __attribute__((always_inline)) uint32_t cubinaI2sReadCycles() {
    uint32_t cycles;
    // A volatile register read with a compiler barrier: do not allow common
    // subexpression elimination or motion across the measured refill work.
    __asm__ __volatile__("rsr.ccount %0" : "=a"(cycles) : : "memory");
    return cycles;
}

static inline void IRAM_ATTR cubinaI2sProbeNewShow() {
    // Exclude the deliberate pause between whole frames from IRQGapMax.
    gCubinaI2sProbe.havePrevious = false;
}

static inline void IRAM_ATTR cubinaI2sProbeTxStart(uint32_t now) {
    gCubinaI2sProbe.txStart = now;
}

static inline void IRAM_ATTR cubinaI2sProbeEnter(uint32_t now) {
    if (gCubinaI2sProbe.havePrevious) {
        const uint32_t gap = uint32_t(now - gCubinaI2sProbe.previousEntry);
        if (gap > gCubinaI2sProbe.maximumGapCycles) {
            gCubinaI2sProbe.maximumGapCycles = gap;
        }
        if (gap > gCubinaI2sProbe.cpuMHz * 40) {
            ++gCubinaI2sProbe.gapsOver40us;
        }
    } else {
        const uint32_t first = uint32_t(now - gCubinaI2sProbe.txStart);
        if (first > gCubinaI2sProbe.maximumFirstCycles) {
            gCubinaI2sProbe.maximumFirstCycles = first;
        }
        if (first > gCubinaI2sProbe.cpuMHz * 40) {
            ++gCubinaI2sProbe.firstOver40us;
        }
    }
    gCubinaI2sProbe.previousEntry = now;
    gCubinaI2sProbe.havePrevious = true;
}

static inline void IRAM_ATTR cubinaI2sProbeExit(uint32_t start, uint32_t end) {
    const uint32_t elapsed = uint32_t(end - start);
    ++gCubinaI2sProbe.refills;
    gCubinaI2sProbe.fillCycles += elapsed;
    if (elapsed > gCubinaI2sProbe.maximumFillCycles) {
        gCubinaI2sProbe.maximumFillCycles = elapsed;
    }
    if (elapsed > gCubinaI2sProbe.cpuMHz * 30) {
        ++gCubinaI2sProbe.fillsOver30us;
    }
}
// CUBINA_I2S_PROFILE_END
