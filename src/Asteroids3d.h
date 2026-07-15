#pragma once
#include <Arduino.h>
#include "CubeCore.h"
#include "BluetoothManager.h"
#include "TextureEngine.h"
#include "GeometryEngine.h"
#include "AppMemory.h"

class Asteroids3d {
public:
    static void run();
    static void renderIcon(TextureState& tex);
    struct Bullet   { bool active; float px, py, pz, vx, vy, vz, distanceTraveled; };
    struct Asteroid { bool active; bool isLarge; float px, py, pz, vx, vy, vz, rotX, rotY, rotZ; };
    struct Particle { bool active; float px, py, pz, vx, vy, vz, life; CRGB color; };

private:
    static void resetGame();
    static void handleInput(GamepadState pad);
    static void updatePhysics(float dt);
    static void checkCollisions();
    static void drawFrame();
    static void playGameOver();
    static void spawnAsteroid(bool isLarge, float px, float py, float pz);

    // --- MATH & PHYSICS CONSTANTS ---
    static constexpr float MAX_SPEED = 12.0f;
    static constexpr float THRUST_POWER = 18.0f;
    static constexpr float DRAG_COEFFICIENT = 0.99f; 
    static constexpr float BULLET_SPEED = 20.0f;

    // --- THE SHIP ---
    static float sPx, sPy, sPz;
    static float sVx, sVy, sVz;
    static float yaw, pitch; 
    static bool thrusting;
    
    // Shield Mechanics
    static bool shieldActive;
    static uint32_t shieldStartTime;
    
    // Weapon Cooldown
    static uint32_t lastFireTime;

    // --- ENTITY LIMITS ---
    static const int MAX_BULLETS = 30;
    static const int MAX_ASTEROIDS = 15;
    static const int MAX_PARTICLES = 80;

    
    static int score;
    static uint32_t lastFrameTime;
};