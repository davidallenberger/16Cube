#pragma once
#include <Arduino.h>
#include "CubeCore.h"
#include "BluetoothManager.h"
#include "TextureEngine.h"
#include "AppMemory.h"

class Snake {
public:
    static void run();
    static void renderIcon(TextureState& tex);

private:
    static void resetGame();
    static void handleInput(GamepadState pad);
    static void updatePhysics();
    static void drawFrame();
    static void spawnApple();
    static void playGameOver();

    // 1024 is plenty large. 16x16x16 max theoretical is 4096, 
    // but humans rarely survive past 100 in full 3D space.
    static const int MAX_LEN = 256; 
    
    static int snakeLen;

    // Current movement vector
    static int8_t dx, dy, dz;
    // Buffered movement vector (prevents reverse-suicide glitch)
    static int8_t pDx, pDy, pDz; 

    // Apple coordinates
    static int8_t ax, ay, az; 

    static unsigned long lastMoveTime;
    static unsigned long moveInterval;
    static CRGBPalette16 snakePalette;
};