#include <nvs_flash.h>
#include "CubeSystem.h"
#include "AppMemory.h"
#include "TestDemo.h"
#include "Shapes.h"

// --- GAME INCLUDES ---
#include "Tetris.h"
#include "Tetris2d.h"
#include "Rubiks.h"
#include "Breakout.h"
#include "Breakout2d.h"
#include "Snake.h"
#include "Snake2d.h"
#include "Asteroids3d.h"

SystemState CubeSystem::currentState = SYS_STATE_BOOT_PAIRING;
int16_t CubeSystem::selectedMenuIndex = 0;
uint32_t CubeSystem::stateTimer = 0;
jmp_buf engineJumpBuffer;
bool teleportArmed = false;
static bool menuNeedsRedraw = true;

// =========================================================================
//                          THE APP REGISTRY
// =========================================================================

// Local Wrappers for system-level apps
static void runAmbientArt() { 
    while (true) {
        TestDemo::runEngine(false); 
    }
}
static void renderAmbientIcon(TextureState& tex) { Shapes::animateStatic(SHAPE_MERKABA, 10, RENDER_SOLID, MODE_SPATIAL_GRADIENT); }

static void runDiagnostics() {
    // A self-contained blocking loop for the controller sandbox
    while (true) {
        BluetoothManager::update();
        ControllerPtr p1 = BluetoothManager::getGamepad(ROLE_MASTER);
        ControllerPtr p2 = BluetoothManager::getGamepad(ROLE_PLAYER_2);
        
        // Escape back to the OS menu
        if ((p1 && p1->miscButtons()) || (p2 && p2->miscButtons())) {
            break; 
        }

        // Just piggyback on the existing drawing logic
        CubeSystem::handlePairingSandbox(); 
        delay(15);
    }
}
static void renderDiagIcon(TextureState& tex) {
    uint8_t pulse = beatsin8(50, 20, 255);
    CRGB col = CRGB(pulse, 0, 0); 
    for(int z = 5; z <= 10; z++) { setVoxel(3, 3, z, col); setVoxel(4, 3, z, col); setVoxel(3, 4, z, col); setVoxel(4, 4, z, col); }
    for(int x = 1; x <= 6; x++) { if(x == 3 || x == 4) continue; setVoxel(x, 3, 7, col); setVoxel(x, 4, 7, col); setVoxel(x, 3, 8, col); setVoxel(x, 4, 8, col); }
    for(int y = 1; y <= 6; y++) { if(y == 3 || y == 4) continue; setVoxel(3, y, 7, col); setVoxel(4, y, 7, col); setVoxel(3, y, 8, col); setVoxel(4, y, 8, col); }
}

// -------------------------------------------------------------------------
// ADD NEW GAMES HERE! The system will automatically absorb them.
static const CubeApp appRegistry[] = {
    {"Ambient Art", renderAmbientIcon,    runAmbientArt},
    {"Tetris 3D",   Tetris::renderIcon,   Tetris::run},
    {"Tetris 2D",   Tetris2d::renderIcon, Tetris2d::run},
    {"Asteroids 3D", Asteroids3d::renderIcon, Asteroids3d::run},
    {"Breakout 3D", Breakout::renderIcon, Breakout::run},   
    {"Breakout 2D", Breakout2d::renderIcon, Breakout2d::run}, 
    {"Snake 3D",    Snake::renderIcon,    Snake::run},
    {"Snake 2D",    Snake2d::renderIcon,  Snake2d::run},
    {"Rubiks",      Rubiks::renderIcon,   Rubiks::run},
    {"Diagnostics", renderDiagIcon,       runDiagnostics}
};
static const int TOTAL_APPS = sizeof(appRegistry) / sizeof(appRegistry[0]);
// -------------------------------------------------------------------------

void CubeSystem::setup() {
    Serial.println("Booting Cube OS...");

    TestDemo::runHardwareWiringTest();
    //delay(500); 
    initAppMemory();

#ifdef DISABLE_BLUETOOTH
    // THE FULL HATCHET: No NVS, No Bluetooth Stack, No Radio.
    Serial.println("BLUETOOTH KILLED BY COMPILER. Skipping NVS. Booting autonomous art engine...");
    selectedMenuIndex = 0; // Force Ambient Art
    changeState(SYS_STATE_RUN_APP);
#else
    // NORMAL BOOT: Mount memory, spin up radio, wait for controllers.
    Serial.println("Mounting NVS Flash Memory...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    BluetoothManager::setup();
    changeState(SYS_STATE_BOOT_PAIRING);
#endif
}

void CubeSystem::changeState(SystemState newState) {
    currentState = newState;
    stateTimer = millis();
    menuNeedsRedraw = true;
    clearAll();
    FastLED.show();
    Serial.printf("System changed to State: %d\n", newState);
}

void CubeSystem::loop() {
#ifndef DISABLE_BLUETOOTH
    BluetoothManager::update();
#endif

    switch (currentState) {
        case SYS_STATE_BOOT_PAIRING:   handlePairingSandbox(); break;
        case SYS_STATE_MENU:           handleMenu();           break;
        case SYS_STATE_RUN_APP:        handleApp();            break;
    }
}

// =========================================================================
//                          MAIN MENU LAUNCHER
// =========================================================================
void CubeSystem::handleMenu() {
    ControllerPtr p1 = BluetoothManager::getGamepad(ROLE_MASTER);
    ControllerPtr p2 = BluetoothManager::getGamepad(ROLE_PLAYER_2);
    
    static uint8_t lastActiveCount = 0;
    static uint32_t lastConnectionTime = 0;
    uint8_t activeCount = (p1 ? 1 : 0) + (p2 ? 1 : 0);

    if (activeCount > lastActiveCount) lastConnectionTime = millis();
    lastActiveCount = activeCount;

    static uint32_t lastInputTime = 0;
    
    if ((millis() - lastConnectionTime > 1000) && (millis() - lastInputTime > 200)) { 
        bool downPressed = (p1 && (p1->dpad() & (DPAD_DOWN | DPAD_RIGHT) || p1->axisY() > 200 || p1->axisX() > 200)) || 
                           (p2 && (p2->dpad() & (DPAD_DOWN | DPAD_RIGHT) || p2->axisY() > 200 || p2->axisX() > 200));
        
        bool upPressed   = (p1 && (p1->dpad() & (DPAD_UP | DPAD_LEFT) || p1->axisY() < -200 || p1->axisX() < -200)) || 
                           (p2 && (p2->dpad() & (DPAD_UP | DPAD_LEFT) || p2->axisY() < -200 || p2->axisX() < -200));
                           
        bool selectPressed = (p1 && (p1->a() || p1->b() || p1->x() || p1->y())) || 
                             (p2 && (p2->a() || p2->b() || p2->x() || p2->y()));

        if (downPressed) {
            selectedMenuIndex = (selectedMenuIndex + 1) % TOTAL_APPS;
            lastInputTime = millis();
            stateTimer = millis(); 
            menuNeedsRedraw = true; // Tag for redraw
        } else if (upPressed) {
            selectedMenuIndex = (selectedMenuIndex - 1 + TOTAL_APPS) % TOTAL_APPS;
            lastInputTime = millis();
            stateTimer = millis(); 
            menuNeedsRedraw = true; // Tag for redraw
        }
    
        if (selectPressed) {
            changeState(SYS_STATE_RUN_APP);
            return;
        }
    }

    if (millis() - stateTimer > 60000) {
        Serial.println("Menu idle. Auto-starting ambient mode.");
        selectedMenuIndex = 0; 
        changeState(SYS_STATE_RUN_APP);
        return;
    }

    // THE FIX: Event-Driven Rendering. 
    // Only push data to the LEDs if the menu actually changed.
    if (menuNeedsRedraw) {
        Serial.printf("DEBUG: Drawing Menu Icon for %s\n", appRegistry[selectedMenuIndex].name);
        
        clearAll();
        static TextureState iconTex(MODE_UNIFORM_CYCLE, RainbowColors_p);
        
        appRegistry[selectedMenuIndex].renderIcon(iconTex);
        FastLED.show();
        
        menuNeedsRedraw = false; // Lock the canvas to protect against interrupts
    }
    
    delay(15);
}

void CubeSystem::handleApp() {
    clearAll();
    
    if (setjmp(engineJumpBuffer) != 0) {
        teleportArmed = false; 
        Serial.println("App aborted via Home button trap. Returning to menu.");
        changeState(SYS_STATE_MENU);
        return; 
    }

    teleportArmed = true; 
    
    Serial.printf("Launching App: %s\n", appRegistry[selectedMenuIndex].name);
    
    // Fire the selected app's engine. Code blocks here until the app returns naturally or traps out.
    appRegistry[selectedMenuIndex].run();

    teleportArmed = false; 
    changeState(SYS_STATE_MENU); 
}

// =========================================================================
//                   THE BOOT PAIRING SANDBOX
// =========================================================================
void CubeSystem::drawNumberOnFaces(uint8_t num, TextureState& tex) {
    uint8_t font[3][5] = {
        {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
        {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
        {0b111, 0b001, 0b111, 0b100, 0b111}  // 2
    };
    uint8_t zOffset = 5; 
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 3; x++) {
            if (font[num][4 - y] & (1 << (2 - x))) {
                CRGB color = tex.getColor(x, y, zOffset);
                int fx = 5 - x; int inv_fx = x + 2; 
                setVoxel(fx, 0, y + zOffset, color);
                setVoxel(inv_fx, 7, y + zOffset, color);
                setVoxel(0, inv_fx, y + zOffset, color);
                setVoxel(7, fx, y + zOffset, color);
            }
        }
    }
}

void CubeSystem::handlePairingSandbox() {
    uint32_t elapsed = millis() - stateTimer;
    ControllerPtr p1 = BluetoothManager::getGamepad(ROLE_MASTER);
    ControllerPtr p2 = BluetoothManager::getGamepad(ROLE_PLAYER_2);
    
    uint8_t activeCount = (p1 ? 1 : 0) + (p2 ? 1 : 0);
    static uint8_t lastActiveCount = 0;
    static uint32_t lastConnectionTime = 0;
    
    if (activeCount > lastActiveCount) lastConnectionTime = millis();
    lastActiveCount = activeCount;

    if (millis() - lastConnectionTime > 1000) {
        if ((p1 && p1->miscButtons()) || (p2 && p2->miscButtons())) {
            changeState(SYS_STATE_MENU);
            return;
        }
    }
    
    if (elapsed > 30000) {
        if (activeCount == 0) {
            selectedMenuIndex = 0; // Ambient
            changeState(SYS_STATE_RUN_APP);
        } else {
            changeState(SYS_STATE_MENU);
        }
        return;
    }

    static uint32_t wipeTimer = 0;
    if (p1 && p1->a()) {
        if (wipeTimer == 0 || wipeTimer < stateTimer) wipeTimer = millis();
        if (millis() - wipeTimer > 5000) { 
            Serial.println("EXECUTING BLUETOOTH NVM WIPE...");
            BP32.forgetBluetoothKeys();
            wipeTimer = 0;
            for (int i = 0; i < 3; i++) {
                clearAll();
                for(int x=0; x<RNDR_X; x++) for(int y=0; y<RNDR_Y; y++) for(int z=0; z<RNDR_Z; z++) setVoxel(x,y,z, CRGB::Red);
                FastLED.show(); delay(150); clearAll(); FastLED.show(); delay(150);
            }
            ESP.restart(); 
        }
    } else {
        wipeTimer = 0;
    }

    static uint32_t lastFrameTime = 0;
    if (millis() - lastFrameTime < 100) return; 
    lastFrameTime = millis();

    clearAll();
    static TextureState bootTex(MODE_UNIFORM_CYCLE, RainbowColors_p);
    bootTex.advance(15);
    drawNumberOnFaces(activeCount, bootTex);

    if (p1) {
        int bZ = 0;
        setVoxel(0, 0, bZ, CRGB::Red); setVoxel(0, 7, bZ, CRGB::Blue);
        setVoxel(7, 0, bZ, CRGB::Green); setVoxel(7, 7, bZ, CRGB::White);

        auto draw2x2x4 = [&](int startX, int startY, CRGB col) {
            for(int x = startX; x <= startX + 1; x++) for(int y = startY; y <= startY + 1; y++) for(int z = 0; z <= 3; z++) setVoxel(x, y, bZ + z, col);
        };

        if (p1->a()) draw2x2x4(0, 0, CRGB::Blue);
        if (p1->b()) draw2x2x4(0, 2, CRGB::Red);
        if (p1->y()) draw2x2x4(0, 4, CRGB::Yellow);
        if (p1->x()) draw2x2x4(2, 2, CRGB::Purple);

        if (p1->miscButtons() & 0x08) draw2x2x4(4, 0, CRGB::White);
        if (p1->miscButtons() & 0x01) draw2x2x4(2, 0, CRGB::White);
        if (p1->miscButtons() & 0x02) draw2x2x4(4, 4, CRGB::White);
        if (p1->miscButtons() & 0x04) draw2x2x4(2, 4, CRGB::White);

        if ((p1->dpad() & DPAD_DOWN)  || p1->axisY() > 200)   draw2x2x4(6, 0, CRGB::Green);
        if ((p1->dpad() & DPAD_LEFT)  || p1->axisX() < -200)  draw2x2x4(6, 2, CRGB::Green); 
        if ((p1->dpad() & DPAD_UP)    || p1->axisY() < -200)  draw2x2x4(6, 4, CRGB::Green);
        if ((p1->dpad() & DPAD_RIGHT) || p1->axisX() > 200)   draw2x2x4(4, 2, CRGB::Green);
    }
    FastLED.show();
}