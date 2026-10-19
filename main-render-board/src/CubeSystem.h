#pragma once
#include <Arduino.h>
#include <setjmp.h>
#include "CubeCore.h"
#include "BluetoothManager.h"
#include "TextureEngine.h"

extern jmp_buf engineJumpBuffer; 
extern bool teleportArmed;

// 1. THE OS STATES (Drastically reduced)
enum SystemState {
    SYS_STATE_BOOT_PAIRING, // Used for the 30-second boot sandbox
    SYS_STATE_MENU,         // The Launcher
    SYS_STATE_RUN_APP       // Runs whatever is in the registry
};

// 2. THE APP CONTRACT
typedef void (*AppIconFunc)(TextureState& tex);
typedef void (*AppRunFunc)();

struct CubeApp {
    const char* name;
    AppIconFunc renderIcon;
    AppRunFunc run;
};

class CubeSystem {
private:
    static SystemState currentState;
    static int16_t selectedMenuIndex;
    static uint32_t stateTimer;

    // Core OS Handlers
    
    static void handleMenu();
    static void handleApp();
    static void drawNumberOnFaces(uint8_t num, TextureState& tex);

public:
    static void handlePairingSandbox();
    static void setup();
    static void loop();
    static void changeState(SystemState newState);
};