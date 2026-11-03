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

// 2. SELECT YOUR RENDER CANVAS - For 8*8*16 (or 10*10*16) uncomment next two lines
//#define TARGET_2026_CUBOID     // 10x10x16 spatial rendering

// The render board has NO Bluetooth radio/task. UARTReceiver owns Core 0;
// animation and all FastLED calls stay on the Arduino Core 1 task.
// =========================================================

#ifdef TARGET_2026_CUBOID
    constexpr uint8_t RNDR_X = 10;
    constexpr uint8_t RNDR_Y = 10;
    constexpr uint8_t RNDR_Z = 16;
    constexpr uint8_t RNDR_PERIMETER = 36; // 10x10 Non-overlapping corners (9+9+9+9)
    constexpr float RNDR_CX = 4.5f;
    constexpr float RNDR_CY = 4.5f;
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

// Verify these two pins against the actual render PCB before flashing.
// Desktop retains the supplied pin assignment; BurnCube must be assigned
// explicitly rather than silently starting UART with LED output pins.
#ifdef HARDWARE_BURNCUBE
#ifndef UART_RX_PIN
#error "Define UART_RX_PIN for the BurnCube UART input (check LED conflicts)"
#endif
#ifndef UART_TX_PIN
#error "Define UART_TX_PIN for BurnCube, or -1 for receive-only"
#endif
#else
#ifndef UART_RX_PIN
#define UART_RX_PIN 27
#endif
#ifndef UART_TX_PIN
#define UART_TX_PIN 14
#endif
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