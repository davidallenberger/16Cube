#pragma once

#include <Arduino.h>
#include "CubeCore.h"
#include "BluetoothManager.h"
#include "TextureEngine.h"

class Tetris2d {
public:
    static void run(); // The launcher
    static void renderIcon(TextureState& tex); // It brings its own icon

private:
    void play();
    // Pure 2D logical representation
    CRGB board[8][16];
    
    int8_t pieceX;
    int8_t pieceY; // Y acts as Z-height in the 2D logic
    uint8_t currentShapeId;
    CRGB pieceColor;
    
    // 4x4 matrix for easy 2D rotation without pivot bugs
    int8_t activeMatrix[4][4]; 

    int8_t bag[7];
    uint8_t bagIndex;
    
    unsigned long lastMoveTime;
    unsigned long lastDropTime;
    unsigned long lastInputTime; 

    void shuffleBag();
    void spawnPiece();
    
    void rotateMatrix(bool clockwise);
    bool checkCollision(int8_t testX, int8_t testY, int8_t matrix[4][4]);
    bool tryMove(int8_t dx, int8_t dy);
    
    //static void handleInput(ControllerPtr pad);
    void handleInput(GamepadState pad);
    
    void drawMirrored(int lx, int ly, CRGB color);
    void drawBoard();
    void drawPiece();
    
    void lockPiece();
    void checkLines();
    void animateClearLine(uint8_t line);
    void animateGameOver();
};