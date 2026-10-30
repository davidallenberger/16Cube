#pragma once
#include <stdint.h>

// Vendor constants in the host stub mirror the documented Bluepad32 API.
constexpr uint8_t DPAD_UP = 0x01;
constexpr uint8_t DPAD_DOWN = 0x02;
constexpr uint8_t DPAD_RIGHT = 0x04;
constexpr uint8_t DPAD_LEFT = 0x08;
constexpr uint8_t MISC_BUTTON_SYSTEM = 0x01;
constexpr uint8_t MISC_BUTTON_SELECT = 0x02;
constexpr uint8_t MISC_BUTTON_START = 0x04;
constexpr uint8_t MISC_BUTTON_CAPTURE = 0x08;

struct FakeController {
    uint8_t directions = 0;
    uint8_t extras = 0;
    bool connected = true;
    bool isGamepad () { return true; }
    bool isConnected () { return connected; }
    void disconnect () { connected = false; }
    uint8_t dpad () { return directions; }
    int16_t axisX () { return -123; }
    int16_t axisY () { return 234; }
    bool a () { return true; }
    bool b () { return false; }
    bool x () { return false; }
    bool y () { return true; }
    bool l1 () { return true; }
    bool l2 () { return false; }
    bool r1 () { return false; }
    bool r2 () { return true; }
    uint8_t miscButtons () { return extras; }
};
using ControllerPtr = FakeController*;

struct BP32Stub {
    void (*onConnect) (ControllerPtr) = nullptr;
    void (*onDisconnect) (ControllerPtr) = nullptr;
    void setup (void (*connect) (ControllerPtr), void (*disconnect) (ControllerPtr)) {
        onConnect = connect;
        onDisconnect = disconnect;
    }
    void update () {}
};
extern BP32Stub BP32;
