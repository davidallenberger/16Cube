#pragma once
#include <Arduino.h>
#include "CubeCore.h"
#include "BluetoothManager.h"
#include "TextureEngine.h"
#include "AppMemory.h"

enum Breakout2dMode {
    MODE2D_CLASSIC,
    MODE2D_OVERDRIVE  
};

class Breakout2d {
public:
    static void run();
    static void renderIcon(TextureState& tex);

    struct Explosion2D {
        int8_t x, z;
        uint32_t startTime;
    };
private:
    static bool runSubMenu();
    static void resetLevel();
    static void resetBall();
    //static void handleInput(ControllerPtr pad);
    static void handleInput(GamepadState pad);
    static void updatePhysics();
    static void drawFrame();
    static void drawMirrored(int x, int z, CRGB col);
    
    static void playWinAnimation();

    static Breakout2dMode currentMode;

    // 2D Brick grid (X and Z only)
    static int bricksRemaining;
    static bool ballInPlay;
    
    // Paddle State (3x1 pad)
    static float paddleX;

    // Ball State (1x1 ball)
    static float ballX;
    static float ballZ;
    static float velX;
    static float velZ;
    
    static uint8_t explosionCount;
};