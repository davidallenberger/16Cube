#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include "CubeCore.h"
#include "TextureEngine.h"

class Rubiks {
public:
    enum Face { UP, DOWN, FRONT, BACK, LEFT, RIGHT, NONE };

#ifdef TARGET_2026_CUBOID
    // 3x3x5 Rubik's Tower Moves (Verticals restricted to 180 degrees)
    enum Move { 
        Z0_CW, Z0_CCW, Z1_CW, Z1_CCW, Z2_CW, Z2_CCW, Z3_CW, Z3_CCW, Z4_CW, Z4_CCW,
        X0_180, X1_180, X2_180, Y0_180, Y1_180, Y2_180 
    };

    struct Cubie {
        CRGB colors[6]; 
    };
#else
    // Standard 3x3x3 Moves
    enum Move { U, U_PRIME, D, D_PRIME, F, F_PRIME, B, B_PRIME, L, L_PRIME, R, R_PRIME };

    struct Cubie {
        CRGB colors[6]; 
    };
#endif

    // --- THE APP CONTRACT ---
    static void renderIcon(TextureState& tex);
    static void run();

    // Autonomous Shows
    static void runScrambleAndSolveShow();
    static void testInitialState();

private:
    static void init();
    static void resetCube();
    static void applyLogicalMove(Move m);
    static void rotateCubieColors(Cubie& c, Move m);
    
    static void drawStaticCube();
    static void drawIntermediateFrame(Move m, float angle);
    static void animateMove(Move m, uint16_t duration_ms);
    static void performScramble(int moves, Move* historyOut = nullptr, bool superhuman = false);
    static void playStartBlink(uint16_t interval_ms = 300);

    static CRGB getStaticVoxelColor(int x, int y, int z);
    
#ifdef TARGET_2026_CUBOID
    //static Cubie logicalCube[3][3][5];
    static bool mapToLocalXY(int phys, int& cubieIdx, int& localIdx);
    static bool mapToLocalZ(int phys, int& cubieIdx, int& localIdx);
#else
    //static Cubie logicalCube[3][3][3];
    static bool mapToLocal(int phys, int& cubieIdx, int& localIdx);
#endif
};