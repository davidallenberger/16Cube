#pragma once
#include <Bluepad32.h>
#include "CubeCore.h"

#ifdef ENABLE_DUAL_CORE
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#endif

enum ControllerRole {
    ROLE_NONE = -1,
    ROLE_MASTER = 0,
    ROLE_PLAYER_2 = 1
};
#ifndef DPAD_UP
#define DPAD_UP    0x01
#define DPAD_DOWN  0x02
#define DPAD_LEFT  0x04
#define DPAD_RIGHT 0x08
#endif

// The generic HAL struct. The games will only ever interact with this.
struct GamepadState {
    bool connected;
    uint8_t dpad; 
    int16_t axisX;
    int16_t axisY;
    bool a, b, x, y;
    bool l1, l2, r1, r2;
    uint8_t miscButtons;

    GamepadState() : connected(false), dpad(0), axisX(0), axisY(0), 
                     a(false), b(false), x(false), y(false), 
                     l1(false), l2(false), r1(false), r2(false), 
                     miscButtons(0) {}
};
class BluetoothManager {
private:
    static constexpr uint8_t MAX_PADS = 2;
    static ControllerPtr gamepads[MAX_PADS];
    
#ifdef ENABLE_DUAL_CORE
    static SemaphoreHandle_t gamepadMutex;
    static GamepadState cachedStateP1;
    static GamepadState cachedStateP2;
    static void btRadioTask(void *pvParameters);
#endif    

    static void onConnectedController(ControllerPtr ctl);
    static void onDisconnectedController(ControllerPtr ctl);
    static int getSlotForPointer(ControllerPtr ctl);
    static int getEmptySlot();

public:
    static void setup();
    static void update();
    
    static bool isConnected(ControllerRole role);
    static ControllerPtr getGamepad(ControllerRole role);
    static GamepadState getState(ControllerRole role);
    
    // Recovery & Replacement API
    static void forceDisconnectAll();
    static bool hasAnyMaster();
};