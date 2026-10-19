#pragma once

#include <Arduino.h>
#include "CubeCore.h"
#include "BluetoothManager.h"
#include "TextureEngine.h"

class Tetris {
public:
    enum SimSpeed {
        SPEED_NORMAL,
        SPEED_FAST,
        SPEED_SUPERHUMAN
    };

    // The autonomous AI Show
    void animate(uint32_t durationSeconds, SimSpeed speed = SPEED_NORMAL);

    static void run();
    static void renderIcon(TextureState& tex);

    // The Interactive Player Mode
    void play();

private:
    struct Point {
        int8_t x, y, z;
    };

    Point currentPiece;
    Point targetPos;         
    int8_t targetZ;          
    CRGB pieceColor;         
    bool isActive;           
    
    int8_t activeBlocks[4][3]; 
    int8_t targetBlocks[4][3]; 
    uint8_t currentShapeId;

    bool isInteractiveMode;
    int8_t lastGhostZ;
    int8_t extrudedBlocks[4][3];

    int8_t bag[7];
    uint8_t bagIndex;
    void shuffleBag();
    uint8_t getNextShape();

    unsigned long lastMoveTime;
    unsigned long lastDropTime;
    unsigned long lastFidgetTime; 
    unsigned long lastInputTime; 
    
    unsigned long moveInterval;
    unsigned long dropInterval;
    unsigned long fidgetInterval;
    SimSpeed currentSpeed; 

    // Core Mechanics
    void spawnPiece(bool useAI = true);
    void findBestTarget(); 
    long calculateScore(int8_t testX, int8_t testY, int8_t testZ, int8_t blocks[4][3]);
    
    // Rotations (Now supports reverse for the 2-axis layout)
    void rotateBlocksX(int8_t blocks[4][3]); 
    void rotateBlocksY(int8_t blocks[4][3]); 
    void rotateBlocksZ(int8_t blocks[4][3]); 
    bool tryRotate(uint8_t axis, bool reverse = false);

    // Collisions & Movement
    bool checkCollision(int8_t testX, int8_t testY, int8_t testZ, int8_t blocks[4][3]);
    bool tryMove(int8_t dx, int8_t dy, int8_t dz);
    //void handleInput(ControllerPtr pad);
    void handleInput(GamepadState pad);
    
    // Rendering & State
    int8_t getGhostZ(); 
    void drawPiece(); 
    void erasePiece();
    void lockPiece();
    bool lockAndCheck(); 

    // Board Management
    void checkPlanes();
    void animateClearPlane(uint8_t z1, uint8_t z2);
    void shiftPlanesDown(uint8_t startZ, uint8_t dropAmount);
    void animateGameOver();
};