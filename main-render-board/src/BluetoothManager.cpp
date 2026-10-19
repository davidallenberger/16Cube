#include "BluetoothManager.h"
#include "CubeCore.h"

#ifdef ENABLE_DUAL_CORE
SemaphoreHandle_t BluetoothManager::gamepadMutex = NULL;
GamepadState BluetoothManager::cachedStateP1;
GamepadState BluetoothManager::cachedStateP2;
#endif

ControllerPtr BluetoothManager::gamepads[MAX_PADS] = {nullptr, nullptr};

void BluetoothManager::onConnectedController(ControllerPtr ctl) {
    if (getSlotForPointer(ctl) != -1) return;

    int slot = getEmptySlot();
    if (slot != -1) {
        gamepads[slot] = ctl;
        Serial.printf("--> Controller connected into Slot %d as %s!\n", 
                      slot, (slot == 0) ? "MASTER" : "PLAYER_2");
    } else {
        Serial.println("--> Connection refused: All controller slots full.");
        ctl->disconnect();
    }
}

void BluetoothManager::onDisconnectedController(ControllerPtr ctl) {
    int slot = getSlotForPointer(ctl);
    if (slot == -1) return;

    Serial.printf("<-- Controller in Slot %d disconnected!\n", slot);
    gamepads[slot] = nullptr;

    // REPLACEMENT SEQUENCE: If Master disconnected but Player 2 is still alive, Promote P2
    if (slot == 0 && gamepads[1] != nullptr) {
        Serial.println("--> Promoting Player 2 to MASTER slot.");
        gamepads[0] = gamepads[1];
        gamepads[1] = nullptr;
    }
}

int BluetoothManager::getSlotForPointer(ControllerPtr ctl) {
    for (int i = 0; i < MAX_PADS; i++) {
        if (gamepads[i] == ctl) return i;
    }
    return -1;
}

int BluetoothManager::getEmptySlot() {
    for (int i = 0; i < MAX_PADS; i++) {
        if (gamepads[i] == nullptr) return i;
    }
    return -1;
}

void BluetoothManager::setup() {
#ifdef ENABLE_DUAL_CORE
    Serial.println("INITIALIZING DUAL-CORE ENGINE: Pinning Bluetooth strictly to Core 0...");
    gamepadMutex = xSemaphoreCreateMutex();
    
    xTaskCreatePinnedToCore(
        btRadioTask,      // Function to run
        "BT_Radio_Task",  // Task name for debugging
        4096,             // Stack size (4KB)
        NULL,             // Parameters 
        2,                // Priority (Higher than default OS tasks)
        NULL,             // Task handle
        0                 // PIN TO CORE 0
    );
#else
    // Standard Single-Core Initialization
    BP32.setup(&onConnectedController, &onDisconnectedController);
#endif
}

void BluetoothManager::update() {
#ifndef ENABLE_DUAL_CORE
    // If we are NOT dual-core, we must manually poll the radio here on Core 1
    BP32.update(); 
#endif
}

bool BluetoothManager::isConnected(ControllerRole role) {
    int idx = (int)role;
    return (gamepads[idx] != nullptr && gamepads[idx]->isConnected());
}

ControllerPtr BluetoothManager::getGamepad(ControllerRole role) {
    ControllerPtr pad = gamepads[(int)role];
    
    // CRITICAL FIX: Only return the pointer if the gamepad has finished authenticating
    if (pad != nullptr && pad->isConnected()) {
        return pad;
    }
    return nullptr;
}

bool BluetoothManager::hasAnyMaster() {
    return (getGamepad(ROLE_MASTER) != nullptr);
}

// =========================================================================
// THE HAL TRANSLATOR (Hardware Abstraction Layer)
// =========================================================================
GamepadState BluetoothManager::getState(ControllerRole role) {
#ifdef ENABLE_DUAL_CORE
    GamepadState state; 
    
    // Grab the data from the Core 0 cache
    if (gamepadMutex != NULL) {
        if (xSemaphoreTake(gamepadMutex, portMAX_DELAY)) {
            state = (role == ROLE_MASTER) ? cachedStateP1 : cachedStateP2;
            xSemaphoreGive(gamepadMutex);
        }
    }
    return state;
    
#else
    // Standard Single-Core behavior: Live polling
    GamepadState state;
    ControllerPtr pad = getGamepad(role);

    if (pad && pad->isConnected()) {
        state.connected = true;
        state.dpad = pad->dpad();
        state.axisX = pad->axisX();
        state.axisY = pad->axisY();
        state.a = pad->a(); state.b = pad->b();
        state.x = pad->x(); state.y = pad->y();
        state.l1 = pad->l1(); state.l2 = pad->l2();
        state.r1 = pad->r1(); state.r2 = pad->r2();
        state.miscButtons = pad->miscButtons();
    }
    return state;
#endif
}


// =========================================================================
// THE FREERTOS DUAL-CORE ENGINE (Runs exclusively on Core 0)
// =========================================================================

#ifdef ENABLE_DUAL_CORE
void BluetoothManager::btRadioTask(void *pvParameters) {
    // 1. THE FIX: Initialize the radio natively inside Core 0's thread context
    BP32.setup(&onConnectedController, &onDisconnectedController);
    Serial.println("Bluepad32 stack successfully initialized on Core 0.");

    while (true) {
        // 2. Process physical radio packets natively on Core 0
        BP32.update(); 

        // 3. Lock the memory bus
        if (xSemaphoreTake(gamepadMutex, portMAX_DELAY)) {
            
            // Safely overwrite the cache for P1
            ControllerPtr p1 = getGamepad(ROLE_MASTER);
            cachedStateP1.connected = (p1 && p1->isConnected());
            if (cachedStateP1.connected) {
                cachedStateP1.dpad = p1->dpad();
                cachedStateP1.axisX = p1->axisX();
                cachedStateP1.axisY = p1->axisY();
                cachedStateP1.a = p1->a(); cachedStateP1.b = p1->b();
                cachedStateP1.x = p1->x(); cachedStateP1.y = p1->y();
                cachedStateP1.l1 = p1->l1(); cachedStateP1.l2 = p1->l2();
                cachedStateP1.r1 = p1->r1(); cachedStateP1.r2 = p1->r2();
                cachedStateP1.miscButtons = p1->miscButtons();
            }

            // Safely overwrite the cache for P2
            ControllerPtr p2 = getGamepad(ROLE_PLAYER_2);
            cachedStateP2.connected = (p2 && p2->isConnected());
            if (cachedStateP2.connected) {
                cachedStateP2.dpad = p2->dpad();
                cachedStateP2.axisX = p2->axisX();
                cachedStateP2.axisY = p2->axisY();
                cachedStateP2.a = p2->a(); cachedStateP2.b = p2->b();
                cachedStateP2.x = p2->x(); cachedStateP2.y = p2->y();
                cachedStateP2.l1 = p2->l1(); cachedStateP2.l2 = p2->l2();
                cachedStateP2.r1 = p2->r1(); cachedStateP2.r2 = p2->r2();
                cachedStateP2.miscButtons = p2->miscButtons();
            }
            
            // 4. Release the memory bus
            xSemaphoreGive(gamepadMutex); 
        }

        // 5. THE FIX: 1-Tick Yield. Fast enough to catch all packets,
        // but still gives the FreeRTOS scheduler room to breathe.
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}
#endif