#pragma once
#include <Arduino.h>

// MUST be defined before FastLED is included anywhere
#ifndef FASTLED_ESP32_I2S
#define FASTLED_ESP32_I2S 1   
#endif
#include <FastLED.h>

// Define global cube dimensions
constexpr uint8_t CUBE_SIZE = 16;
constexpr uint8_t CUBE_MAX = CUBE_SIZE - 1; 

// ================== ENVIRONMENT SWITCHES ==================

// 1. SELECT YOUR HARDWARE WIRING TOPOLOGY
//#define HARDWARE_BURNCUBE      // The 4-pin Dig-Octa T-Connector layout
#define HARDWARE_DESKTOP     // The original 8-pin serpentine test layout

// 2. SELECT YOUR RENDER CANVAS - For 8*8*16 uncomment next two lines
#define TARGET_2026_CUBOID     // 8x8x16 spatial rendering

// 3. COMPILE-TIME BLUETOOTH KILL SWITCH
// Uncomment the line below to completely disable the Bluetooth radio, 
// pairing menus, and background interrupts. The cube will boot directly 
// into Ambient Art autonomously.
//#define DISABLE_BLUETOOTH

// 4. MULTI-THREADING OPTIMIZATION
// Uncomment to pin the Bluetooth radio to Core 0, leaving Core 1 entirely to FastLED
#define ENABLE_DUAL_CORE
// =========================================================

#ifdef TARGET_2026_CUBOID
    constexpr uint8_t RNDR_X = 8;
    constexpr uint8_t RNDR_Y = 8;
    constexpr uint8_t RNDR_Z = 16;
    constexpr uint8_t RNDR_PERIMETER = 28; // 8x8 Non-overlapping corners (7+7+7+7)
    constexpr float RNDR_CX = 3.5f;
    constexpr float RNDR_CY = 3.5f;
    constexpr float RNDR_CZ = 7.5f;
#else
    constexpr uint8_t RNDR_X = CUBE_SIZE;
    constexpr uint8_t RNDR_Y = CUBE_SIZE;
    constexpr uint8_t RNDR_Z = CUBE_SIZE;
    constexpr uint8_t RNDR_PERIMETER = 60; // 16x16 Non-overlapping corners (15+15+15+15)
    constexpr float RNDR_CX = (CUBE_SIZE - 1) / 2.0f;
    constexpr float RNDR_CY = (CUBE_SIZE - 1) / 2.0f;
    constexpr float RNDR_CZ = (CUBE_SIZE - 1) / 2.0f;
#endif

// Core Hardware Lifecycle
void initCube();
void showCube();
void clearAll();
void seedAllRng();
void fadeAll(uint8_t fadeAmt);

// Voxel Read/Write (3D Space -> Physical Space)
void setVoxel(uint8_t x, uint8_t y, uint8_t z, const CRGB& c);
CRGB getVoxel(uint8_t x, uint8_t y, uint8_t z);

// Map perimeter index (0..59) to 2D coordinates (u, w) around the cube faces.
void getPerimeterCoords(uint8_t p, uint8_t& u, uint8_t& w);

// Globally available perimeter-to-3D axis mapping
void setVoxelByAxis(uint8_t u, uint8_t w, uint8_t h, uint8_t axis, CRGB color);

// Debugging
void debugVoxel(uint8_t x, uint8_t y, uint8_t z);