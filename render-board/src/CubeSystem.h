#pragma once
#include <Arduino.h>
#include <setjmp.h>
#include "CubeCore.h"
#include "GamepadState.h"
#include "TextureEngine.h"

extern jmp_buf engineJumpBuffer;
extern bool teleportArmed;

// The render engine either displays diagnostics, the menu, or an app.
enum SystemState {
    SYS_STATE_BOOT_PAIRING, // Legacy state name; now remote-input diagnostics.
    SYS_STATE_MENU,         // The Launcher
    SYS_STATE_RUN_APP       // Runs whatever is in the registry
};

// Each registered app supplies a menu icon and a run function.
typedef void (*AppIconFunc) (TextureState &tex);
typedef void (*AppRunFunc) ();

struct CubeApp {
    const char *name;
    AppIconFunc renderIcon;
    AppRunFunc run;
};

class CubeSystem {
  private:
    static SystemState currentState;
    static int16_t selectedMenuIndex;
    static uint32_t stateTimer;

    // These handlers all run on the animation task.

    static void handleMenu ();
    static void handleApp ();
    static void drawNumberOnFaces (uint8_t num, TextureState &tex);

  public:
    static void handlePairingSandbox ();
    // Called on the animation task before showing a frame. Reads received
    // demands, updates per-frame gamepad input, and may escape the current app.
    // The UART receiver task must never call this or use engineJumpBuffer.
    static void frameCheckpoint ();
    static void setup ();
    static void loop ();
    static void changeState (SystemState newState);
};