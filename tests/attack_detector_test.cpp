#include "AttackDetector.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

int main() {
    constexpr float dt = 256.0f / 31250.0f;
    // Bass, midrange and treble share the same detector and event contract.
    for (unsigned selected : {0u, 7u, 12u, 15u}) {
        AttackDetector detector;
        float bands[16] = {};
        for (unsigned i = 0; i < 100; ++i) {
            assert(!detector.process(bands, false, dt).event);
        }
        bands[selected] = .02f;
        auto attack = detector.process(bands, true, dt);
        assert(attack.event);
        assert(attack.strength > 0 && attack.strength <= 1);
        assert(attack.bands & (1u << selected));
        // Loudness by itself must not continuously produce attacks.
        for (unsigned i = 0; i < 200; ++i) {
            assert(!detector.process(bands, true, dt).event);
        }
        bands[selected] = 0;
        for (unsigned i = 0; i < 20; ++i) {
            assert(!detector.process(bands, false, dt).event);
        }
        bands[selected] = .03f;
        assert(detector.process(bands, true, dt).event);
        detector.reset();
        assert(!detector.process(bands, true, dt).event);
    }
    puts("Attack tests passed: bass/mid/treble attacks, sustain rejection, rearming, reset and band masks.");
}
