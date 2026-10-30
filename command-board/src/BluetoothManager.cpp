#include "BluetoothManager.h"

#if COMMAND_ENABLE_GAMEPAD_BLUETOOTH

ControllerPtr BluetoothManager::gamepads[2] = {nullptr, nullptr};

void BluetoothManager::onConnectedController (ControllerPtr controller) {
    // Ignore duplicate callbacks for a controller we already assigned.
    for (auto existing : gamepads) {
        if (existing == controller) {
            return;
        }
    }

    // Bluepad32 also supports mice/keyboards. This acquisition path is for
    // gamepads only, with at most two players.
    if (!controller->isGamepad ()) {
        controller->disconnect ();
        return;
    }

    for (auto &slot : gamepads) {
        if (!slot) {
            slot = controller;
            return;
        }
    }

    controller->disconnect ();
}

void BluetoothManager::onDisconnectedController (ControllerPtr controller) {
    for (auto &slot : gamepads) {
        if (slot == controller) {
            slot = nullptr;
        }
    }

    // Keep the remaining controller in the master slot, matching the
    // previous two-player behavior.
    if (!gamepads[0] && gamepads[1]) {
        gamepads[0] = gamepads[1];
        gamepads[1] = nullptr;
    }
}

void BluetoothManager::setup () {
    BP32.setup (&onConnectedController, &onDisconnectedController);
}

void BluetoothManager::update () {
    // Bluepad32 delivers updated readings and connection callbacks here.
    BP32.update ();
}

GamepadState BluetoothManager::getState (ControllerRole role) {
    GamepadState state;

    if (role != ROLE_MASTER && role != ROLE_PLAYER_2) {
        return state;
    }

    ControllerPtr controller = gamepads[int (role)];

    if (!controller || !controller->isConnected ()) {
        return state;
    }

    state.connected = true;
    state.axisX = controller->axisX ();
    state.axisY = controller->axisY ();

    // Explicit boundary between Bluepad32's definitions and OUR definitions.
    // Even though the bit values currently match, each direction is mapped
    // by name. The receiver and games depend only on CubinaInput constants.
    const uint8_t directions = controller->dpad ();

    if (directions & DPAD_UP) {
        state.dpad |= CubinaInput::DpadUp;
    }
    if (directions & DPAD_DOWN) {
        state.dpad |= CubinaInput::DpadDown;
    }
    if (directions & DPAD_RIGHT) {
        state.dpad |= CubinaInput::DpadRight;
    }
    if (directions & DPAD_LEFT) {
        state.dpad |= CubinaInput::DpadLeft;
    }

    state.a = controller->a ();
    state.b = controller->b ();
    state.x = controller->x ();
    state.y = controller->y ();
    state.l1 = controller->l1 ();
    state.l2 = controller->l2 ();
    state.r1 = controller->r1 ();
    state.r2 = controller->r2 ();

    // Keep each extra button distinct. In particular, only System becomes
    // the Home/menu escape bit; Start and Select must remain game inputs.
    const uint8_t extras = controller->miscButtons ();

    if (extras & MISC_BUTTON_SYSTEM) {
        state.miscButtons |= CubinaInput::System;
    }
    if (extras & MISC_BUTTON_SELECT) {
        state.miscButtons |= CubinaInput::Select;
    }
    if (extras & MISC_BUTTON_START) {
        state.miscButtons |= CubinaInput::Start;
    }
    if (extras & MISC_BUTTON_CAPTURE) {
        state.miscButtons |= CubinaInput::Capture;
    }

    return state;
}

void BluetoothManager::forceDisconnectAll () {
    // This is a real hardware action, so it belongs on the command board.
    for (auto controller : gamepads) {
        if (controller) {
            controller->disconnect ();
        }
    }
}

#else

// Disabling Bluetooth removes Bluepad32 entirely from compilation. Acquisition
// can still ask for snapshots; it gets disconnected, neutral gamepad state.
void BluetoothManager::setup () {
}

void BluetoothManager::update () {
}

GamepadState BluetoothManager::getState (ControllerRole role) {
    return GamepadState ();
}

void BluetoothManager::forceDisconnectAll () {
}

#endif
