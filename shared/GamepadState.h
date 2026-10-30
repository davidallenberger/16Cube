#pragma once

#include <stdint.h>

// This header is the game-input contract shared by BOTH boards.
// It deliberately includes no Bluetooth, UART, Arduino, or FreeRTOS headers.
// Games understand these values; only acquisition knows which hardware
// produced them, and only the UART modules know how they were transported.
namespace CubinaInput {
// Directions are independent bits, not mutually exclusive numbers.
// For example, Up + Right is DpadUp | DpadRight (a diagonal press).
// Names avoid DPAD_* macros because Bluepad32 owns those names.
constexpr uint8_t DpadUp = 0x01;
constexpr uint8_t DpadDown = 0x02;
constexpr uint8_t DpadRight = 0x04;
constexpr uint8_t DpadLeft = 0x08;

// These bits describe the extra buttons, separately from A/B/X/Y.
// System usually means the controller's Home/PS/Xbox-logo button.
// Select, Start, and Capture must NOT accidentally act as Home.
constexpr uint8_t System = 0x01;
constexpr uint8_t Select = 0x02;
constexpr uint8_t Start = 0x04;
constexpr uint8_t Capture = 0x08;
} // namespace CubinaInput

// The role chooses which player's snapshot to request. A disconnected player
// still has a snapshot: connected is false and the other fields are neutral.
enum ControllerRole { ROLE_NONE = -1, ROLE_MASTER = 0, ROLE_PLAYER_2 = 1 };

// This is the original gamepad model, moved out of BluetoothManager.
// Keep its fields stable: the UART encoder explicitly serializes them.
// Do not transmit sizeof(GamepadState); C++ padding is not a wire format.
struct GamepadState {
    bool connected;

    // Bitmask using CubinaInput::Dpad* above; zero means no direction held.
    uint8_t dpad;

    // Left-stick axes: approximately -512 to +512, with zero at the center.
    // Negative X = left; positive X = right. Negative Y = up; positive Y = down.
    int16_t axisX;
    int16_t axisY;

    // These named booleans need no bit decoding in game code.
    bool a;
    bool b;
    bool x;
    bool y;
    bool l1;
    bool l2;
    bool r1;
    bool r2;

    // Bitmask using System, Select, Start, and Capture above.
    uint8_t miscButtons;

    // A default snapshot is safe to use when a player or UART disappears.
    GamepadState ()
        : connected (false), dpad (0), axisX (0), axisY (0), a (false), b (false),
          x (false), y (false), l1 (false), l2 (false), r1 (false), r2 (false),
          miscButtons (0) {
    }
};
