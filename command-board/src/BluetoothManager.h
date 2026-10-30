#pragma once

#include "BoardConfig.h"
#include "GamepadState.h"

#if COMMAND_ENABLE_GAMEPAD_BLUETOOTH
#include <Bluepad32.h>
#endif

// This class exists ONLY on the command board, where Bluetooth hardware lives.
// It converts Bluepad32 objects into our shared, hardware-independent state.
// No Bluepad32 pointer is returned to a game or sent over UART.
//
// setup() runs once during Arduino setup. SignalAcquisition then owns update()
// and getState() on its task. That task publishes copied state through the
// existing CommandState mutex accessors; this class needs no second mutex.
class BluetoothManager {
  public:
    static void setup ();
    static void update ();
    static GamepadState getState (ControllerRole role);
    static void forceDisconnectAll ();

  private:
#if COMMAND_ENABLE_GAMEPAD_BLUETOOTH
    // These are REAL Bluepad32 hardware handles, never render-side proxies.
    static ControllerPtr gamepads[2];
    static void onConnectedController (ControllerPtr controller);
    static void onDisconnectedController (ControllerPtr controller);
#endif
};
