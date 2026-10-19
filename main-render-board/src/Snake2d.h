#pragma once
#include <Arduino.h>
#include "CubeCore.h"
#include "BluetoothManager.h"
#include "TextureEngine.h"
#include "AppMemory.h"

class Snake2d {
public:
    static void run();
    static void renderIcon(TextureState& tex);

private:
    static void resetGame();
    static void handleInput(GamepadState pad);
    static void updatePhysics();
    static void drawFrame();
    static void drawMirrored(int x, int z, CRGB col);
    static void spawnApple();
    static void playGameOver();

    // 256 is exactly enough to fill a 16x16 grid.
    static const int MAX_LEN = 128; 
    
    // In 2D mode, we only use X and Z coordinates
    static int snakeLen;

    static int8_t dx, dz;
    static int8_t pDx, pDz; 

    static int8_t ax, az; 

    static unsigned long lastMoveTime;
    static unsigned long moveInterval;
    static CRGBPalette16 snakePalette;
};