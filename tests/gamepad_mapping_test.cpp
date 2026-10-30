#include "BluetoothManager.cpp"
#include <assert.h>
#include <stdio.h>

BP32Stub BP32;

int main () {
    BluetoothManager::setup ();
    FakeController controller;
    BP32.onConnect (&controller);

    // Exercise every direction combination, including diagonal presses.
    for (unsigned mask = 0; mask < 16; ++mask) {
        controller.directions = mask;
        const GamepadState state = BluetoothManager::getState (ROLE_MASTER);
        uint8_t expected = 0;
        if (mask & DPAD_UP) {
            expected |= CubinaInput::DpadUp;
        }
        if (mask & DPAD_DOWN) {
            expected |= CubinaInput::DpadDown;
        }
        if (mask & DPAD_LEFT) {
            expected |= CubinaInput::DpadLeft;
        }
        if (mask & DPAD_RIGHT) {
            expected |= CubinaInput::DpadRight;
        }
        assert (state.connected && state.dpad == expected);
    }

    for (unsigned mask = 0; mask < 16; ++mask) {
        controller.extras = mask;
        const GamepadState state = BluetoothManager::getState (ROLE_MASTER);
        uint8_t expected = 0;
        if (mask & MISC_BUTTON_SYSTEM) {
            expected |= CubinaInput::System;
        }
        if (mask & MISC_BUTTON_SELECT) {
            expected |= CubinaInput::Select;
        }
        if (mask & MISC_BUTTON_START) {
            expected |= CubinaInput::Start;
        }
        if (mask & MISC_BUTTON_CAPTURE) {
            expected |= CubinaInput::Capture;
        }
        assert (state.miscButtons == expected);
        assert (state.axisX == -123 && state.axisY == 234);
        assert (state.a && !state.b && !state.x && state.y);
        assert (state.l1 && !state.l2 && !state.r1 && state.r2);
    }

    assert (!BluetoothManager::getState (ROLE_PLAYER_2).connected);
    assert (!BluetoothManager::getState (ROLE_NONE).connected);
    BP32.onDisconnect (&controller);
    assert (!BluetoothManager::getState (ROLE_MASTER).connected);
    puts ("Gamepad mapping tests passed: directions, extra buttons, axes, named buttons, "
          "disconnect.");
}
