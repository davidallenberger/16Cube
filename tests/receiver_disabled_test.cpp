#include "UARTReceiver.cpp"
#include <cassert>
#include <cstdio>

#if CUBINA_ENABLE_UART_RECEIVER
#error "This test requires the receiver to be compiled out"
#endif

// Deliberately NO definitions of Serial, Serial2, millis, mutex functions or
// task functions. Linking proves the OFF implementation needs none of them.
int main() {
    for (unsigned i = 0; i < 100; ++i) {
        RemoteInputs::begin();
        const auto control = RemoteInputs::beginFrame();
        assert(!control.linked && !control.ready);
        assert(control.mode == CubinaWire::Mode::Animation);
        assert(!RemoteInputs::control().linked);
        const auto audio = RemoteInputs::audio();
        assert(audio.volumeRaw == 0 && audio.volumeSm == 0 && audio.volumeAvg == 0);
        assert(!audio.bassKick && !audio.midAttack && !audio.highAttack);
        for (auto value : audio.fftResult) {
            assert(value == 0);
        }
        assert(audio.beatAngle() == 0 && audio.beatPulse() == 0);
        const auto stats = RemoteInputs::stats();
        assert(stats.accepted == 0 && stats.rejected == 0 && stats.duplicates == 0);
        assert(!RemoteInputs::gamepad(ROLE_MASTER).connected);
        assert(!RemoteInputs::heldGamepad(ROLE_PLAYER_2).connected);
    }
    uint8_t depth[CubinaWire::DEPTH_BYTES + 1];
    memset(depth, 0xA5, sizeof(depth));
    assert(!RemoteInputs::depth(depth));
    for (size_t i = 0; i < CubinaWire::DEPTH_BYTES; ++i) {
        assert(depth[i] == 0);
    }
    assert(depth[CubinaWire::DEPTH_BYTES] == 0xA5);
    assert(!RemoteInputs::depth(nullptr));
    puts("UART OFF passed: neutral standalone inputs; links without Serial2, serial I/O, clock or FreeRTOS services.");
}
