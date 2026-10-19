#pragma once
#include <Arduino.h>
#include "CubeCore.h"
#include "BluetoothManager.h"
#include "TextureEngine.h"
#include "AppMemory.h"

enum BreakoutMode {
    MODE_CLASSIC,
    MODE_OVERDRIVE  // The combined Steerable + Breakthru mode
};

class Breakout {
public:
    static void run();
    static void renderIcon(TextureState& tex);

    struct Explosion {
        int8_t x, y, z;
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
    
    // The Ultimate Visual Reward
    static void playWinAnimation();

    static BreakoutMode currentMode;

    static int bricksRemaining;
    static bool ballInPlay;
    
    // Paddle State (4x4 pad)
    static float paddleX;
    static float paddleY;

    // Ball State (1x1x1 ball)
    static float ballX;
    static float ballY;
    static float ballZ;
    static float velX;
    static float velY;
    static float velZ;
     // --- ON-HIT VISUAL REWARD SYSTEM ---
    
    static uint8_t explosionCount;
};