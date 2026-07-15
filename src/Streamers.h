#pragma once
#include "CubeCore.h"
#include "TextureEngine.h"
#include "AppMemory.h"

class Streamers {
  private:
  static float getThicknessFromVolume(float volPercent) {
        float planeArea = (float)(RNDR_X * RNDR_Y);
        float targetArea = planeArea * volPercent;
        return sqrtf(targetArea) / 2.0f; 
    }   
    // ==========================================
    // THE BRUSH-ENABLED ANIMATORS
    // ==========================================

    // ==========================================
    // THE KINETIC PARTICLE ENGINE
    // ==========================================
    struct KineticParticle {
        bool active;
        uint8_t type;     // 0 = standard spark, 1 = firework climbing shell
        float px, py, pz;
        float vx, vy, vz;
        float drag;
        float gravity;
        float life;       // 1.0f (full brightness) down to 0.0f (dead)
        float decay;      // How much life is lost per second
        CRGB color;
        bool hasTail;
    };

    static void processKineticPhysics(KineticParticle* pool, int count, float dt) {
        for (int i = 0; i < count; i++) {
            if (!pool[i].active) continue;

            // 1. Apply Velocity
            pool[i].px += pool[i].vx * dt;
            pool[i].py += pool[i].vy * dt;
            pool[i].pz += pool[i].vz * dt;

            // 2. Apply Newtonian Drag & Gravity
            pool[i].vx *= pool[i].drag;
            pool[i].vy *= pool[i].drag;
            pool[i].vz *= pool[i].drag;
            pool[i].vz -= pool[i].gravity * dt;

            // 3. Apply Aging
            pool[i].life -= pool[i].decay * dt;

            int ix = (int)lroundf(pool[i].px);
            int iy = (int)lroundf(pool[i].py);
            int iz = (int)lroundf(pool[i].pz);

            // 4. Kill Logic (Faded out, fell through floor, or hit the glass)
            if (pool[i].life <= 0.0f || iz < 0 || ix < 0 || ix >= RNDR_X || iy < 0 || iy >= RNDR_Y) {
                pool[i].active = false;
                continue;
            }

           // 5. Draw the Particle
            if (iz < RNDR_Z) {
                CRGB drawColor = pool[i].color;
                
                // RESTORED: High-velocity particles burn white-hot
                float currentSpeed = abs(pool[i].vx) + abs(pool[i].vy) + abs(pool[i].vz);
                if (currentSpeed > 15.0f) {
                    drawColor |= CRGB(180, 180, 180); // Inject pure white light into the base color
                }

                drawColor.nscale8((uint8_t)(pool[i].life * 255.0f)); 
                
                // Additive blending for glowing cores
                CRGB existing = getVoxel(ix, iy, iz);
                setVoxel(ix, iy, iz, existing + drawColor);

                // 6. Optional Kinetic Tails
                if (pool[i].hasTail) {
                    if (currentSpeed > 5.0f) {
                        int tailX = (int)lroundf(pool[i].px - (pool[i].vx * dt * 2.0f));
                        int tailY = (int)lroundf(pool[i].py - (pool[i].vy * dt * 2.0f));
                        int tailZ = (int)lroundf(pool[i].pz - (pool[i].vz * dt * 2.0f));
                        
                        if (tailX >= 0 && tailX < RNDR_X && tailY >= 0 && tailY < RNDR_Y && tailZ >= 0 && tailZ < RNDR_Z) {
                            CRGB tailColor = drawColor;
                            tailColor.nscale8(80); 
                            setVoxel(tailX, tailY, tailZ, getVoxel(tailX, tailY, tailZ) + tailColor);
                        }
                    }
                }
            }
        }
    }

    public:
    // ==========================================
    // GEOMETRY LOOPS (Untouched)
    // ==========================================
    static void animateRandomFall(uint32_t durationMs, float fallSpeed = 0.4f, float returnSpeed = 0.8f) {
        uint32_t startTime = millis();
        float zPos[16][16]; uint8_t state[16][16]; 
        for(int x = 0; x < RNDR_X; x++) { for(int y = 0; y < RNDR_Y; y++) { zPos[x][y] = RNDR_Z - 1.0f; state[x][y] = 0; } }
        bool returnPhase = false; int totalVoxels = RNDR_X * RNDR_Y;

        while (millis() - startTime < durationMs) {
            clearAll(); int atBottom = 0, atTop = 0, triggersPerFrame = max(1, totalVoxels / 30); 
            for(int i = 0; i < triggersPerFrame; i++) {
                int rx = random8(RNDR_X), ry = random8(RNDR_Y);
                if (!returnPhase && state[rx][ry] == 0) state[rx][ry] = 1; 
                if (returnPhase && state[rx][ry] == 2) state[rx][ry] = 3;  
            }

            for(int x = 0; x < RNDR_X; x++) {
                for(int y = 0; y < RNDR_Y; y++) {
                    if (state[x][y] == 1) { zPos[x][y] -= fallSpeed; if (zPos[x][y] <= 0.0f) { zPos[x][y] = 0.0f; state[x][y] = 2; } } 
                    else if (state[x][y] == 3) { zPos[x][y] += returnSpeed; if (zPos[x][y] >= RNDR_Z - 1.0f) { zPos[x][y] = RNDR_Z - 1.0f; state[x][y] = 0; } }
                    if (state[x][y] == 0) atTop++; if (state[x][y] == 2) atBottom++;
                    float zRatio = zPos[x][y] / (RNDR_Z - 1.0f);
                    CRGB color = blend(CRGB::Blue, CRGB::Red, (uint8_t)(zRatio * 255.0f));
                    int zInt = (int)roundf(zPos[x][y]); setVoxel(x, y, zInt, color);
                    if (state[x][y] == 1 && zInt + 1 < RNDR_Z) { CRGB tail = color; tail.nscale8(80); setVoxel(x, y, zInt + 1, tail); } 
                    else if (state[x][y] == 3 && zInt - 1 >= 0) { CRGB tail = color; tail.nscale8(80); setVoxel(x, y, zInt - 1, tail); }
                }
            }
            if (!returnPhase && atBottom == totalVoxels) { showCube(); delay(600); returnPhase = true; } 
            else if (returnPhase && atTop == totalVoxels) { showCube(); delay(600); returnPhase = false; }
            showCube(); yield();
        }
    }

   // ==========================================
    // THE FIREWORKS (Powered by Kinetic Engine)
    // ==========================================
    static void animateFireworks(uint32_t durationMs) {
        uint32_t startTime = millis(), lastFrame = millis();
        
        // Define the Safe Launch Zone (prevents clipping glass walls)
        float minX = RNDR_X * 0.25f, maxX = RNDR_X * 0.75f;
        float minY = RNDR_Y * 0.25f, maxY = RNDR_Y * 0.75f;

        const int MAX_PARTICLES = (RNDR_X * RNDR_Y * RNDR_Z) / 7;
        
        //static KineticParticle pool[MAX_PARTICLES];
        KineticParticle* pool = (KineticParticle*)SharedAppMemory;
        for(int i = 0; i < MAX_PARTICLES; i++) pool[i].active = false;

        bool keepRunning = true;

        while (keepRunning) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; 
            if (deltaMs == 0) { yield(); continue; }
            lastFrame = now; float dt = deltaMs / 1000.0f;
            
            // Check the clock once per frame
            bool timeIsUp = (now - startTime >= durationMs);
            
            // Fast exponential fade clears the smoke
            for (int x = 0; x < RNDR_X; x++) {
                for (int y = 0; y < RNDR_Y; y++) {
                    for (int z = 0; z < RNDR_Z; z++) {
                        CRGB c = getVoxel(x, y, z);
                        if (c) { c.nscale8(180); setVoxel(x, y, z, c); }
                    }
                }
            }

            // 1. RANDOM LAUNCHER (Only allow launches if time is NOT up)
            if (!timeIsUp && random8(100) < 2) { 
                for (int i = 0; i < MAX_PARTICLES; i++) {
                    if (!pool[i].active) {
                        
                        // MATHEMATICALLY TARGETED APEX
                        // Height = v^2 / (2 * Gravity). 
                        // With 15.0f gravity, a speed of 18.0 hits Z=10.8, and 22.0 hits Z=16.1.
                        float launchSpeed = random16(180, 220) / 10.0f;
                        
                        pool[i] = {
                            true, 1, // Type 1 = Shell
                            (float)random16((int)minX * 10, (int)maxX * 10) / 10.0f, 
                            (float)random16((int)minY * 10, (int)maxY * 10) / 10.0f, 
                            0.0f,
                            0.0f, 0.0f, launchSpeed,
                            1.0f, 15.0f,  // 1.0f = NO DRAG. Ascent is dictated purely by gravity!
                            1.0f, 0.0f,   // Doesn't decay while climbing
                            CRGB::White, true
                        };
                        break;
                    }
                }
            }

            // 2. DETONATION CHECK
            for (int i = 0; i < MAX_PARTICLES; i++) {
                if (pool[i].active && pool[i].type == 1 && pool[i].vz <= 0.0f) {
                    pool[i].active = false; // Kill the shell
                    
                    CRGBPalette16 explosionPal = PaletteUtils::getRandomOrganicPalette();
                    bool isConfetti = random8(2) == 0;
                    CRGB solidColor = PaletteUtils::getRandomVibrantColor();
                    
                    int sparksToSpawn = (RNDR_X * RNDR_Y * RNDR_Z) / 29;
                    
                    for (int j = 0; j < MAX_PARTICLES && sparksToSpawn > 0; j++) {
                        if (!pool[j].active) {
                            float theta = (random16(0, 6283) / 1000.0f); 
                            float v = (random16(0, 2000) / 1000.0f) - 1.0f; 
                            float phi = acos(v);
                            
                            // High initial burst
                            float burstSpeed = random16(300, 500) / 10.0f; 
                            CRGB sparkColor = isConfetti ? ColorFromPalette(explosionPal, random8(), 255, NOBLEND) : solidColor;

                            pool[j] = {
                                true, 0, // Type 0 = Spark
                                pool[i].px, pool[i].py, pool[i].pz,
                                burstSpeed * sin(phi) * cos(theta), burstSpeed * sin(phi) * sin(theta), burstSpeed * cos(phi),
                                0.75f, 12.0f, // MASSIVE drag (stops midair), low gravity (weeping willow fall)
                                1.0f, (float)random16(8, 15) / 10.0f, // 0.8 to 1.5 decay rate (dies in < 1 second)
                                sparkColor, false // No tails to keep the sphere crisp
                            };
                            sparksToSpawn--;
                        }
                    }
                }
            }

            // 3. RUN THE PHYSICS ENGINE
            processKineticPhysics(pool, MAX_PARTICLES, dt);
            
            // 4. GRACEFUL EXIT CHECK
            bool anyAlive = false;
            for(int i = 0; i < MAX_PARTICLES; i++) {
                if (pool[i].active) { anyAlive = true; break; }
            }

            // If time is up AND the sky is completely clear of sparks, we exit
            if (timeIsUp && !anyAlive) keepRunning = false;
            
            showCube(); yield();
        }
    }

    

    // ==========================================
    // THE ATOM SMASHER (Powered by Kinetic Engine)
    // ==========================================
    static void animateAtomSmasher(uint32_t durationMs = 15000, int targetCycles = 0) {
        uint32_t startTime = millis(), lastFrame = millis();
        float angleA = 0.0f, angleB = PI, zA = RNDR_Z - 1.0f, zB = 0.0f;
        
        // FIX 1: Tucked the orbit radius in slightly to prevent Index 16 Out-Of-Bounds
        float orbitRadius = min(RNDR_CX, RNDR_CY) - 1.5f; 
        
        // ELEVATED DETONATION: We shift the convergence point to the upper 3rd of the cube (65% height).
        // This gives the explosion plenty of room to occupy the top before the 25.0f gravity pulls it down.
        float targetZ = RNDR_Z * 0.60f; 
        
        float speed = TWO_PI * 0.5f;
        
        // Asymmetric vertical speeds guarantee both points arrive at the elevated target simultaneously
        float speedZA = ((RNDR_Z - 1.0f) - targetZ) / 3.5f;
        float speedZB = targetZ / 3.5f;

        enum State { SPIRAL_IN, IMPLOSION_PAUSE, KINETIC_BLAST };
        State state = SPIRAL_IN;
        uint32_t stateTimer = millis();

        const int MAX_EMBERS = (RNDR_X * RNDR_Y * RNDR_Z) / 12;
        //static KineticParticle embers[MAX_EMBERS];
        KineticParticle* embers = (KineticParticle*)SharedAppMemory;
        for(int i = 0; i < MAX_EMBERS; i++) embers[i].active = false;

        float masterPhase = 0.0f;
        int currentCycle = 0;
        bool keepRunning = true;

        CRGB colA = CHSV(random8(), 255, 255), colB = CHSV(random8(), 255, 255);

        while (keepRunning) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; 
            if (deltaMs == 0) { yield(); continue; }
            lastFrame = now; float dt = deltaMs / 1000.0f; masterPhase += 0.33f * deltaMs;
            clearAll();

            if (state == SPIRAL_IN) {
                speed += (TWO_PI * 1.5f) * dt; 
                angleA += speed * dt; angleB -= speed * dt;
                
                zA -= speedZA * dt; 
                zB += speedZB * dt;

                int ax = (int)lroundf(RNDR_CX + cosf(angleA) * orbitRadius), ay = (int)lroundf(RNDR_CY + sinf(angleA) * orbitRadius), az = (int)lroundf(zA);
                int bx = (int)lroundf(RNDR_CX + cosf(angleB) * orbitRadius), by = (int)lroundf(RNDR_CY + sinf(angleB) * orbitRadius), bz = (int)lroundf(zB);
                setVoxel(ax, ay, az, colA); setVoxel(bx, by, bz, colB);

                if (zA <= targetZ && zB >= targetZ) { state = IMPLOSION_PAUSE; stateTimer = now; }
            }
            else if (state == IMPLOSION_PAUSE) {
                if (now - stateTimer > 120) {
                    state = KINETIC_BLAST; stateTimer = now;
                    for(int i = 0; i < MAX_EMBERS; i++) {
                        float theta = (random16(0, 6283) / 1000.0f); 
                        float v = (random16(0, 2000) / 1000.0f) - 1.0f; 
                        float phi = acos(v);
                        
                        // Lower initial speed, higher drag. It flashes white, then blooms rainbow.
                        float burstSpeed = random16(250, 450) / 10.0f; 
                        
                        CRGB sparkColor = ColorFromPalette(PartyColors_p, random8(), 255, NOBLEND);

                        embers[i] = {
                            true, 0,
                            RNDR_CX, RNDR_CY, targetZ, // Detonate at the elevated Z
                            burstSpeed * sin(phi) * cos(theta), burstSpeed * sin(phi) * sin(theta), burstSpeed * cos(phi),
                            0.82f, 25.0f, // Restored: Hard atmospheric braking and heavy gravity
                            1.0f, 0.2f,   // Restored: Authentic slow decay    
                            sparkColor, 
                            true              
                        };
                    }
                }
            }
            else if (state == KINETIC_BLAST) {
                processKineticPhysics(embers, MAX_EMBERS, dt);

                bool anyAlive = false;
                for(int i = 0; i < MAX_EMBERS; i++) if (embers[i].active) anyAlive = true;

                if (!anyAlive || now - stateTimer > 4000) {
                    currentCycle++;
                    if ((targetCycles > 0 && currentCycle >= targetCycles) || (targetCycles <= 0 && (millis() - startTime >= durationMs))) keepRunning = false;
                    if (keepRunning) {
                        state = SPIRAL_IN; stateTimer = now;
                        angleA = 0.0f; angleB = PI; zA = RNDR_Z - 1.0f; zB = 0.0f; speed = TWO_PI * 0.5f;
                        colA = CHSV(random8(), 255, 255); colB = CHSV(random8(), 255, 255);
                    }
                }
            }
            showCube(); 
            
            // FIX 2: Explicit frame limiter to prevent ESP32 DMA starvation
            // Replaces the naked 'yield()' call
            delay(15); 
        }
    }
    static void animateVerticalPipes(uint32_t durationMs, uint32_t speedMs) {
        uint32_t startTime = millis(), lastFrame = millis(); float masterHue = 0.0f;
        uint8_t px[50], py[50], pz[50], pDir[50]; int activeCount = 0;
        for (uint8_t x = 0; x < RNDR_X; x++) { for (uint8_t y = 0; y < RNDR_Y; y++) { if (random8(7) == 0 && activeCount < 50) { px[activeCount] = x; py[activeCount] = y; pz[activeCount] = 0; pDir[activeCount] = random8(3); activeCount++; } } }
        clearAll(); 
        while (millis() - startTime < durationMs) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now; masterHue -= 0.133f * deltaMs; 
            CRGB currentFrameColor = CHSV((uint8_t)masterHue, 255, 255);
            for (int i = 0; i < activeCount; i++) {
                if (random8(3) == 0) { pDir[i] = random8(3); for (int z = 0; z < RNDR_Z; z++) setVoxel(px[i], py[i], z, CRGB::Black); }
                if (pDir[i] == 1) { if (pz[i] < RNDR_Z - 1) pz[i]++; else { pz[i]--; pDir[i] = 2; } } else if (pDir[i] == 2) { if (pz[i] > 0) pz[i]--; else { pz[i]++; pDir[i] = 1; } }
                setVoxel(px[i], py[i], pz[i], currentFrameColor);
            }
            showCube(); if (speedMs > 0) delay(speedMs); else yield();
        }
    }

  static void animateHula(uint32_t durationMs, uint32_t speedMs, CRGBPalette16 pal = RainbowColors_p, float revsPerSec = 1.5f) {
        uint32_t startTime = millis(), lastFrameTime = millis(); float globalAngle = 0.0f, colorPhase = 0.0f, midZ = (RNDR_Z - 1) / 2.0f;
        while (millis() - startTime < durationMs) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrameTime; lastFrameTime = now; clearAll();
            float s = sin(globalAngle), c = cos(globalAngle);
            for (int z = 0; z < RNDR_Z; z++) {
                float dz = z - midZ, dist = (dz < 0) ? -dz : dz, size = (dist / midZ) * (RNDR_X / 2.0f) + (RNDR_X / 2.0f);
                uint8_t layerColorIndex = (uint8_t)colorPhase + (z * 10); CRGB layerColor = ColorFromPalette(pal, layerColorIndex, 255, LINEARBLEND);
                for (int x = 0; x < RNDR_X; x++) { for (int y = 0; y < RNDR_Y; y++) { float ux = ((x - RNDR_CX) * c) + ((y - RNDR_CY) * s), uy = -((x - RNDR_CX) * s) + ((y - RNDR_CY) * c), origX = ux + RNDR_CX, origY = uy + RNDR_CY; if (origX >= 0.0f && origX < size && origY >= 0.0f && origY < size) { setVoxel(x, y, z, layerColor); } } }
            }
            showCube(); globalAngle += (revsPerSec * TWO_PI) * (deltaMs / 1000.0f); colorPhase += 100.0f * (deltaMs / 1000.0f); if (speedMs > 0) delay(speedMs); else yield();
        }
    }


    static void animateTornado(uint32_t durationMs, uint32_t speedMs, CRGBPalette16 pal = CloudColors_p, float startingRevs = 1.0f) {
        uint32_t startTime = millis(), lastFrameTime = millis(); float globalAngle = 0.0f, colorPhase = 0.0f;
        const int MAX_DEBRIS = 64; int numDebris = min((int)(RNDR_Z * 3), MAX_DEBRIS); float dZ[MAX_DEBRIS], dAng[MAX_DEBRIS], dRadOffset[MAX_DEBRIS], dSpeed[MAX_DEBRIS];
        for(int i = 0; i < numDebris; i++) { dZ[i] = random16(0, RNDR_Z * 10) / 10.0f; dAng[i] = random16(0, TWO_PI * 100) / 100.0f; dRadOffset[i] = random16(10, 20) / 10.0f; dSpeed[i] = random16(10, 30) / 10.0f; }
        while (millis() - startTime < durationMs) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrameTime; lastFrameTime = now; clearAll();
            float progress = (float)(now - startTime) / (float)durationMs, currentRevs = startingRevs * (1.0f + (progress * 2.5f)); 
            globalAngle += (currentRevs * TWO_PI) * (deltaMs / 1000.0f); colorPhase += 150.0f * (deltaMs / 1000.0f);
            int numArms = 4; 
            for (int z = 0; z < RNDR_Z; z++) {
                float zPercent = (float)z / (float)(RNDR_Z > 1 ? RNDR_Z - 1 : 1), maxRadius = (RNDR_X - 1) / 2.0f, idealRadius = 0.5f + pow(zPercent, 1.5f) * (maxRadius - 0.5f);
                float zTwist = z * 0.5f; uint8_t layerColorIndex = (uint8_t)colorPhase + (z * 8); CRGB layerColor = ColorFromPalette(pal, layerColorIndex, 255, LINEARBLEND);
                for (int x = 0; x < RNDR_X; x++) {
                    for (int y = 0; y < RNDR_Y; y++) {
                        float dx = x - RNDR_CX, dy = y - RNDR_CY, pixelDist = sqrt((dx * dx) + (dy * dy)), distFromWall = abs(pixelDist - idealRadius), wallThickness = 1.5f;
                        if (distFromWall < wallThickness) { float wallBrightness = 1.0f - (distFromWall / wallThickness), pixelAngle = atan2(dy, dx), spiralPhase = (numArms * (pixelAngle - globalAngle)) + zTwist, spiralMod = (sin(spiralPhase) + 1.0f) * 0.5f, intensity = wallBrightness * (0.2f + 0.8f * spiralMod); CRGB finalColor = layerColor; finalColor.nscale8((uint8_t)(intensity * 255.0f)); setVoxel(x, y, z, finalColor); }
                    }
                }
            }
            for(int i = 0; i < numDebris; i++) {
                dZ[i] += dSpeed[i] * (deltaMs / 1000.0f); if (dZ[i] >= RNDR_Z) { dZ[i] = 0; dAng[i] = random16(0, TWO_PI * 100) / 100.0f; }
                int z = (int)dZ[i]; float zPercent = (float)z / (float)(RNDR_Z > 1 ? RNDR_Z - 1 : 1), maxRadius = (RNDR_X - 1) / 2.0f, baseRadius = 0.5f + pow(zPercent, 1.5f) * (maxRadius - 0.5f), radius = baseRadius * dRadOffset[i];
                dAng[i] += (currentRevs * TWO_PI * 1.3f) * (deltaMs / 1000.0f);
                float px = RNDR_CX + (cos(dAng[i]) * radius), py = RNDR_CY + (sin(dAng[i]) * radius);
                if (px >= 0 && px < RNDR_X && py >= 0 && py < RNDR_Y) { CRGB debrisColor = ColorFromPalette(pal, (uint8_t)(colorPhase + (i * 15)), 255, LINEARBLEND); debrisColor |= CRGB(100, 100, 100); CRGB existing = getVoxel((int)px, (int)py, z); setVoxel((int)px, (int)py, z, existing + debrisColor); }
            }
            showCube(); if (speedMs > 0) delay(speedMs); else yield();
        }
    }
    // ==========================================
    // WHAT THE HELIX (Continuous Capsule Sweeping)
    // ==========================================
    static void animateWhatTheHelix(uint32_t durationMs, uint32_t speedMs) {
        uint32_t startTime = millis();
        uint32_t lastFrameTime = millis();
        
        float radius = (RNDR_X > 8) ? (RNDR_X / 2.0f) - 1.5f : 3.0f; 
        float thickness = (RNDR_X > 8) ? 1.5f : 1.25f; 

        // TRANSLATE THE DELAY INTO A SMOOTH PHYSICS MULTIPLIER
        // Baseline is 50. If speedMs is 100 (Burn Cube), physics run at 0.5x speed.
        float physicsMultiplier = 50.0f / (float)(speedMs > 0 ? speedMs : 50);

        // 1. THE ELEGANT MASTER CLOCK 
        float masterPhase = 0.0f;
        uint32_t pauseEndTime = 0; 

        static uint8_t helixStyle = 255;
        helixStyle = (helixStyle + 1) % 4; 
        
        ShaderMode sMode = MODE_UNIFORM_CYCLE;
        CRGBPalette16 pal = PartyColors_p;
        
        switch(helixStyle) {
            case 0: sMode = MODE_UNIFORM_CYCLE; pal = RainbowColors_p; break;
            case 1: sMode = MODE_NOISE_FIELD;   pal = OceanColors_p;   break;
            case 2: sMode = MODE_LINEAR_FLOW;   pal = LavaColors_p;    break;
            case 3: sMode = MODE_SCATTER;       pal = PartyColors_p;   break;
        }
        
        TextureState tex(sMode, pal, 1.0f);

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrameTime;
            if (deltaMs == 0) { yield(); continue; } 
            lastFrameTime = now;

            clearAll();

            // 2. TORSION PENDULUM PHYSICS WITH TIME-STOP
            if (now < pauseEndTime) {
                // Time is frozen. The frame renders perfectly still.
            } else {
                float oldPhase = masterPhase;
                
                // APPLY THE PHYSICS MULTIPLIER TO THE TIME DELTA
                masterPhase += ((TWO_PI / 5500.0f) * deltaMs) * physicsMultiplier; 
                
                // If we just crossed the apex of the swing, snap to it and freeze time
                if (oldPhase < (PI / 2.0f) && masterPhase >= (PI / 2.0f)) {
                    masterPhase = (PI / 2.0f);
                    pauseEndTime = now + (uint32_t)(250 / physicsMultiplier); // Hang-time scales too!
                } else if (oldPhase < (3.0f * PI / 2.0f) && masterPhase >= (3.0f * PI / 2.0f)) {
                    masterPhase = (3.0f * PI / 2.0f);
                    pauseEndTime = now + (uint32_t)(250 / physicsMultiplier); 
                }
                
                if (masterPhase >= TWO_PI) masterPhase -= TWO_PI;
            }

            // Twist Phase: Over-amplify and clamp 
            float rawTwist = sin(masterPhase) * 1.5f;
            
            // INCREASED COILING: (PI * 3.0f) / 8.0f (Creates 3 full coils)
            float twistPhase = constrain(rawTwist, -1.0f, 1.0f) * ((PI * 3.0f) / 8.0f);

            // Rotation Phase: 2.5 full rotations before reversing
            float rotPhase = 5.0f * PI * sin(masterPhase);
            
            // 3. FLAWLESS VOLUMETRIC RENDERING (Continuous Capsule Sweeping)
            for (int z = 0; z < RNDR_Z; z++) {
                
                // Calculate exactly where the helix enters and exits this Z-layer
                float normZ_top = (float)z - (RNDR_Z / 2.0f) - 0.5f;
                float normZ_bot = (float)z - (RNDR_Z / 2.0f) + 0.5f;
                
                float angleTop = rotPhase + (normZ_top * twistPhase);
                float angleBot = rotPhase + (normZ_bot * twistPhase);
                
                float cx_top = RNDR_CX + (cos(angleTop) * radius);
                float cy_top = RNDR_CY + (sin(angleTop) * radius);
                
                float cx_bot = RNDR_CX + (cos(angleBot) * radius);
                float cy_bot = RNDR_CY + (sin(angleBot) * radius);
                
                // Calculate the 2D trajectory segment through this layer
                float dx_seg = cx_bot - cx_top;
                float dy_seg = cy_bot - cy_top;
                float segLengthSq = (dx_seg * dx_seg) + (dy_seg * dy_seg);

                for (int x = 0; x < RNDR_X; x++) {
                    for (int y = 0; y < RNDR_Y; y++) {
                        float dist;
                        
                        // Find the shortest distance from the voxel to the trajectory line segment
                        if (segLengthSq < 0.001f) {
                            float dx = (float)x - cx_top;
                            float dy = (float)y - cy_top;
                            dist = sqrtf(dx*dx + dy*dy);
                        } else {
                            float t = (((float)x - cx_top) * dx_seg + ((float)y - cy_top) * dy_seg) / segLengthSq;
                            t = fmaxf(0.0f, fminf(1.0f, t)); // Constrain to the physical segment bounds
                            
                            float projX = cx_top + (t * dx_seg);
                            float projY = cy_top + (t * dy_seg);
                            
                            float dx = (float)x - projX;
                            float dy = (float)y - projY;
                            dist = sqrtf(dx*dx + dy*dy);
                        }
                        
                        float intensity = constrain(thickness - dist, 0.0f, 1.0f);
                        
                        if (intensity > 0.0f) {
                            CRGB color = tex.getColor(x, y, z);
                            
                            // Anti-aliasing scaling
                            color.nscale8((uint8_t)(intensity * 255.0f)); 
                            
                            CRGB existing = getVoxel(x, y, z);
                            setVoxel(x, y, z, existing + color);
                        }
                    }
                }
            }

            showCube();
            // The Texture clock NEVER stops, keeping the paint boiling!
            tex.advance(deltaMs);
            yield(); // Smooth hardware yield, NO artificial delays
        }
    }
// ==========================================
    // LAVA LAMP (Fixed Volume Partitioning)
    // ==========================================
    struct LavaBlob {
        float x, y, z;
        float vx, vy, vz;
        float targetX, targetY; 
        bool isRising;          
        float baseRadius; 
        float speedFactor;      
    };

    static void animateLavaLamp(uint32_t durationMs, float speedMultiplier = 1.0f, float volumePercent = 0.15f) {
        
        uint32_t startTime = millis();
        uint32_t lastFrameTime = millis();

        // 1. EXACT VOLUME PARTITIONING
        float totalCubeVolume = (float)(RNDR_X * RNDR_Y * RNDR_Z);
        float targetFluidVolume = totalCubeVolume * volumePercent; 
        
        const int NUM_BLOBS = (RNDR_X * RNDR_Y * RNDR_Z >= 4096) ? 6 : 3;
        LavaBlob blobs[NUM_BLOBS];

        float weights[NUM_BLOBS];
        float sumWeights = 0.0f;
        for (int i = 0; i < NUM_BLOBS; i++) {
            weights[i] = 50.0f + (float)random8(); 
            sumWeights += weights[i];
        }

        float glassPadding = max(1.0f, RNDR_X * 0.125f); 

        // 2. INITIALIZATION
        for (int i = 0; i < NUM_BLOBS; i++) {
            float blobVolume = targetFluidVolume * (weights[i] / sumWeights);
            
            // THE FIX 1: Apply a 0.65f empirical dampener. 
            // This counteracts the volume bloat caused by overlapping metaball fields inside a narrow bounding box.
            blobs[i].baseRadius = powf(blobVolume * 0.238732f, 0.333333f) * 0.65f;

            blobs[i].x = glassPadding + ((float)random8() / 255.0f) * (RNDR_X - glassPadding * 2.0f);
            blobs[i].y = glassPadding + ((float)random8() / 255.0f) * (RNDR_Y - glassPadding * 2.0f);
            blobs[i].z = ((float)random8() / 255.0f) * (RNDR_Z - 2.0f) + 1.0f; 
            
            blobs[i].targetX = glassPadding + ((float)random8() / 255.0f) * (RNDR_X - glassPadding * 2.0f);
            blobs[i].targetY = glassPadding + ((float)random8() / 255.0f) * (RNDR_Y - glassPadding * 2.0f);
            
            blobs[i].vx = 0; blobs[i].vy = 0; blobs[i].vz = 0;
            
            blobs[i].isRising = (random8(2) == 0); 
            blobs[i].speedFactor = 0.5f + ((float)random8() / 255.0f); 
        }

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrameTime;
            if (deltaMs == 0) { yield(); continue; }
            lastFrameTime = now;

            clearAll(); 

            float dt = ((float)deltaMs / 16.0f) * speedMultiplier; 

            // ==========================================
            // PHYSICS LOOP
            // ==========================================
            for (int i = 0; i < NUM_BLOBS; i++) {
                
                float dx = blobs[i].targetX - blobs[i].x;
                float dy = blobs[i].targetY - blobs[i].y;
                float dist = sqrtf(dx*dx + dy*dy);

                if (dist < 1.0f) {
                    blobs[i].targetX = glassPadding + ((float)random8() / 255.0f) * (RNDR_X - glassPadding * 2.0f);
                    blobs[i].targetY = glassPadding + ((float)random8() / 255.0f) * (RNDR_Y - glassPadding * 2.0f);
                } else {
                    blobs[i].vx += (dx / dist) * 0.006f * dt;
                    blobs[i].vy += (dy / dist) * 0.006f * dt;
                }

                float buoyancyAccel = 0.025f * blobs[i].speedFactor * dt;
                
                if (blobs[i].isRising) {
                    blobs[i].vz += buoyancyAccel;
                    if (blobs[i].z >= RNDR_Z - 1.5f) blobs[i].isRising = false; 
                } else {
                    blobs[i].vz -= buoyancyAccel;
                    if (blobs[i].z <= 1.0f) blobs[i].isRising = true; 
                }

                float drag = powf(0.88f, dt); 
                blobs[i].vx *= drag;
                blobs[i].vy *= drag;
                blobs[i].vz *= drag;

                blobs[i].x += blobs[i].vx * dt;
                blobs[i].y += blobs[i].vy * dt;
                blobs[i].z += blobs[i].vz * dt;

                if (blobs[i].x < glassPadding) { blobs[i].x = glassPadding; blobs[i].vx *= -0.5f; }
                if (blobs[i].x > RNDR_X - glassPadding) { blobs[i].x = RNDR_X - glassPadding; blobs[i].vx *= -0.5f; }
                if (blobs[i].y < glassPadding) { blobs[i].y = glassPadding; blobs[i].vy *= -0.5f; }
                if (blobs[i].y > RNDR_Y - glassPadding) { blobs[i].y = RNDR_Y - glassPadding; blobs[i].vy *= -0.5f; }
                
                if (blobs[i].z < 0.0f) { blobs[i].z = 0.0f; blobs[i].vz = 0.0f; }
                if (blobs[i].z > RNDR_Z - 1.0f) { blobs[i].z = RNDR_Z - 1.0f; blobs[i].vz = 0.0f; }
            }

            // ==========================================
            // RENDER LOOP (The Dimming Trick)
            // ==========================================
            
            // THE FIX 2: Raise the skin threshold drastically from 0.70 to 0.85 
            // This cuts away the ambient overlapping field energy so the tube doesn't fill with "fuzz"
            float skinThreshold = (RNDR_X <= 8) ? 0.85f : 0.75f;
            float coreThreshold = skinThreshold + 0.30f; 
            
            float r2[NUM_BLOBS];
            float stretchZ[NUM_BLOBS];
            
            for (int i = 0; i < NUM_BLOBS; i++) {
                r2[i] = blobs[i].baseRadius * blobs[i].baseRadius;
                
                // THE FIX 3: Severely restrict the Z-stretch modifier (cut from 0.45 max down to 0.25 max). 
                // Previously, fast moving blobs were accidentally doubling their rendered volume.
                stretchZ[i] = 1.0f - constrain(fabsf(blobs[i].vz) * 1.5f, 0.0f, 0.25f);
            }

            for (uint8_t z = 0; z < RNDR_Z; z++) {
                //uint8_t colorIndex = 64 + (uint8_t)(((float)z / (float)(RNDR_Z - 1)) * 191.0f);
                //CRGB layerColor = ColorFromPalette(pal, colorIndex);
                uint8_t blendFactor = (uint8_t)(((float)z / (float)(RNDR_Z > 1 ? RNDR_Z - 1 : 1)) * 255.0f);
                CRGB layerColor = blend(CRGB::OrangeRed, CRGB::Orange, blendFactor);

                for (uint8_t y = 0; y < RNDR_Y; y++) {
                    for (uint8_t x = 0; x < RNDR_X; x++) {
                        
                        float energy = 0.0f;
                        
                        for (int i = 0; i < NUM_BLOBS; i++) {
                            float dx = (float)x - blobs[i].x;
                            float dy = (float)y - blobs[i].y;
                            float dz = ((float)z - blobs[i].z) * stretchZ[i]; 
                            
                            float d2 = (dx*dx) + (dy*dy) + (dz*dz);
                            energy += r2[i] / (d2 + 0.1f);
                        }

                        if (energy >= skinThreshold) {
                            if (energy >= coreThreshold) {
                                setVoxel(x, y, z, layerColor);
                            } else {
                                float depth = (energy - skinThreshold) / (coreThreshold - skinThreshold);
                                uint8_t dimFactor = 40 + (uint8_t)(depth * 160.0f);
                                
                                CRGB skinColor = layerColor;
                                skinColor.nscale8(dimFactor); 
                                
                                setVoxel(x, y, z, skinColor);
                            }
                        }
                    }
                }
            }
            
            showCube();
            yield();
        }
    }
    // ==========================================
    // PURE OVERLOADS (Intent Inference)
    // ==========================================
    // ==========================================
    // VERTICAL STREAMER (Internal Engine)
    // ==========================================
    static void animateVerticalStreamer(uint32_t durationMs, uint32_t speedMs, float volumePercent, CRGBPalette16 pal,ShaderMode sMode) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        float masterPhase = 0.0f;
        
        float thickness = getThicknessFromVolume(volumePercent);
        float histX[32], histY[32]; 
        float currentX = RNDR_CX, currentY = RNDR_CY;
        float targetX = RNDR_CX, targetY = RNDR_CY;
        
        for(int i = 0; i < 32; i++) { histX[i] = RNDR_CX; histY[i] = RNDR_CY; }
        
        while (millis() - startTime < durationMs) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now;
            clearAll();
            masterPhase += 0.33f * deltaMs; 
            
            if (abs(currentX - targetX) < 0.5f && abs(currentY - targetY) < 0.5f) {
                targetX = random((int)ceil(thickness), (int)floor(RNDR_X - thickness));
                targetY = random((int)ceil(thickness), (int)floor(RNDR_Y - thickness));
            }
            currentX += (targetX > currentX) ? 0.2f : -0.2f; currentY += (targetY > currentY) ? 0.2f : -0.2f;
            
            for (int z = 0; z < RNDR_Z - 1; z++) { histX[z] = histX[z + 1]; histY[z] = histY[z + 1]; }
            histX[RNDR_Z - 1] = currentX; histY[RNDR_Z - 1] = currentY;
            
            for (int z = 0; z < RNDR_Z; z++) {
                TextureState tex(sMode, pal, 1.0f);
                tex.internalPhase = masterPhase + (z * 10);
                tex.phase = (uint32_t)tex.internalPhase;
                tex.initMath(); // Re-sync the dynamic fields

                int ix0 = max(0, (int)floor(histX[z] - thickness - 1.0f)), ix1 = min((int)RNDR_X - 1, (int)ceil(histX[z] + thickness + 1.0f));
                int iy0 = max(0, (int)floor(histY[z] - thickness - 1.0f)), iy1 = min((int)RNDR_Y - 1, (int)ceil(histY[z] + thickness + 1.0f));
                
                for(int x = ix0; x <= ix1; x++) {
                    for(int y = iy0; y <= iy1; y++) {
                        float dx = x - histX[z], dy = y - histY[z], dist = sqrtf(dx*dx + dy*dy);
                        float intensity = constrain(thickness - dist, 0.0f, 1.0f);
                        if (intensity > 0.0f) {
                            CRGB color = tex.getColor(x, y, z);
                            color.nscale8((uint8_t)(intensity * 255.0f));
                            setVoxel(x, y, z, color);
                        }
                    }
                }
            }
            showCube(); 
            
            // The Hardware Normalizer
            if (speedMs > 0) delay(speedMs); else yield();
        }
    }
    static void animateVerticalStreamer(uint32_t durationMs, uint32_t speedMs, float volumePercent, CRGB::HTMLColorCode colorCode) { animateVerticalStreamer(durationMs, speedMs, volumePercent, CRGBPalette16(CRGB(colorCode)), MODE_SOLID); }
    static void animateVerticalStreamer(uint32_t durationMs, uint32_t speedMs, float volumePercent, ShaderMode sMode) { animateVerticalStreamer(durationMs, speedMs, volumePercent, RainbowColors_p, sMode); }

     static void animateCyclone(uint32_t durationMs, int numStreamers, float volumePercent, CRGBPalette16 pal, ShaderMode sMode, float rps= 1.0f) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        float masterPhase = 0.0f, anglePhase = 0.0f;
        float thickness = getThicknessFromVolume(volumePercent);
        float radius = (RNDR_X / 2.0f) - thickness;
        
        // If there are more than 3 arms, proportionally slow down the RPMs 
        // so the visual frequency (arms per second) remains identical.
        float adjustedRps = rps;
        if (numStreamers > 3) {
            adjustedRps = rps * (3.0f / (float)numStreamers);
        }
        
        float totalTwist = 140.0f;
        float lagPerLayer = totalTwist / (RNDR_Z - 1);
        float spinSpeed = adjustedRps * 360.0f;

        while (millis() - startTime < durationMs) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now;
            clearAll();
            masterPhase += 0.33f * deltaMs; 

            for (uint8_t z = 0; z < RNDR_Z; z++) {
                float zDistance = (RNDR_Z - 1) - z, lagAngle = zDistance * lagPerLayer;
                for (int i = 0; i < numStreamers; i++) {
                    float offsetAngle = (360.0f / numStreamers) * i;
                    uint32_t colorOffset = (512 / numStreamers) * i;
                    
                    TextureState tex(sMode, pal, 1.0f);
                    tex.internalPhase = masterPhase + colorOffset + (z * 5);
                    tex.phase = (uint32_t)tex.internalPhase;
                    tex.initMath();

                    float rad = (anglePhase + offsetAngle - lagAngle) * 0.0174533f;
                    float cx = RNDR_CX + cos(rad) * radius, cy = RNDR_CY + sin(rad) * radius;

                    uint8_t ix0 = max(0, (int)round(cx - thickness - 1)), ix1 = min((int)RNDR_X - 1, (int)round(cx + thickness + 1));
                    uint8_t iy0 = max(0, (int)round(cy - thickness - 1)), iy1 = min((int)RNDR_Y - 1, (int)round(cy + thickness + 1));

                    for(uint8_t x = ix0; x <= ix1; x++) {
                        for(uint8_t y = iy0; y <= iy1; y++) {
                            float dx = cx - (float)x, dy = cy - (float)y, dist = sqrtf(dx*dx + dy*dy);
                            if (dist <= thickness) setVoxel(x, y, z, tex.getColor(x, y, z));
                        }
                    }
                }
            }
            showCube();
            anglePhase += spinSpeed * (deltaMs / 1000.0f);
            if (anglePhase >= 360.0f) anglePhase -= 360.0f;
            yield();
        }
    } 
    static void animateCyclone(uint32_t durationMs, int numStreamers, float volumePercent, CRGB::HTMLColorCode colorCode, float rps = 1.0f) { animateCyclone(durationMs, numStreamers, volumePercent, CRGBPalette16(CRGB(colorCode)), MODE_SOLID, rps); }
    static void animateCyclone(uint32_t durationMs, int numStreamers, float volumePercent, ShaderMode sMode, float rps = 1.0f) { animateCyclone(durationMs, numStreamers, volumePercent, RainbowColors_p, sMode, rps); }

    static void animateRain(uint32_t durationMs, int dropsPerFrame, float fallSpeed, CRGBPalette16 pal, ShaderMode sMode) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        float masterPhase = 0.0f;
        const int MAX_DROPS = 150;
        float dX[MAX_DROPS], dY[MAX_DROPS], dZ[MAX_DROPS];
        bool active[MAX_DROPS] = {false};

        while (millis() - startTime < durationMs) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now;
            clearAll();
            masterPhase += 0.33f * deltaMs;
            
            TextureState tex(sMode, pal, 1.0f);
            tex.internalPhase = masterPhase;
            tex.phase = (uint32_t)masterPhase;
            tex.initMath();
            
            for (int i = 0; i < dropsPerFrame; i++) {
                for (int j = 0; j < MAX_DROPS; j++) {
                    if (!active[j]) { active[j] = true; dX[j] = random8(RNDR_X); dY[j] = random8(RNDR_Y); dZ[j] = RNDR_Z - 1.0f; break; }
                }
            }

            for (int j = 0; j < MAX_DROPS; j++) {
                if (active[j]) {
                    int xInt = (int)dX[j], yInt = (int)dY[j], zInt = (int)roundf(dZ[j]);
                    
                    // Only check X/Y bounds here so the drop can fall seamlessly through the floor
                    if (xInt >= 0 && xInt < RNDR_X && yInt >= 0 && yInt < RNDR_Y) {
                        
                        // Protect TextureEngine from out-of-bounds Z queries
                        int safeZ = max(0, min(zInt, RNDR_Z - 1));
                        CRGB color = (sMode == MODE_SCATTER) ? 
                                     ColorFromPalette(pal, (j * 37) % 255, 255, NOBLEND) : 
                                     tex.getColor(xInt, yInt, safeZ);

                        // 1. Draw the Head (if it is currently above the floor)
                        if (zInt >= 0 && zInt < RNDR_Z) {
                            setVoxel(xInt, yInt, zInt, color);
                        }
                        
                        // 2. Draw the Tail (1 voxel above the head)
                        if (zInt + 1 >= 0 && zInt + 1 < RNDR_Z) { 
                            CRGB tail = color; 
                            tail.nscale8(80); 
                            setVoxel(xInt, yInt, zInt + 1, tail); 
                        }
                    }
                    
                    dZ[j] -= fallSpeed;
                    
                    // Let it fall to -1.5f so the tail has time to hit the Z=0 floor before deleting
                    if (dZ[j] < -1.5f) active[j] = false;
                }
            }
            showCube(); delay(5);
        }
    }
    static void animateRain(uint32_t durationMs, int dropsPerFrame, float fallSpeed, CRGB::HTMLColorCode colorCode) { animateRain(durationMs, dropsPerFrame, fallSpeed, CRGBPalette16(CRGB(colorCode)),MODE_SOLID); }
    static void animateRain(uint32_t durationMs) { animateRain(durationMs, 2, 0.8f, OceanColors_p, MODE_LINEAR_FLOW);}
    
    // ==========================================
    // THE WILD MOUSE (Internal Engine)
    // ==========================================
    static void animateWildMouse(uint32_t durationMs, uint32_t speedMs) {
        uint32_t startTime = millis(), lastFrame = millis(); 
        float masterPhase = 0.0f;
        
        int numMice = min(max(1, (RNDR_X * RNDR_Y * RNDR_Z) / 512), 16); 
        const uint8_t MAX_TAIL = 64; 
        uint8_t tailLength = min((int)(RNDR_X * 3), (int)MAX_TAIL);
        
        uint8_t hx[16][MAX_TAIL], hy[16][MAX_TAIL], hz[16][MAX_TAIL]; 
        int headPtr[16], x[16], y[16], z[16], dir[16], axis[16];
        
        // Array to hold unique palettes for each mouse
        CRGBPalette16 mousePals[16];

        for (int m = 0; m < numMice; m++) { 
            x[m] = random8(RNDR_X); y[m] = random8(RNDR_Y); z[m] = random8(RNDR_Z); 
            dir[m] = (random8(2) == 0) ? 1 : -1; 
            axis[m] = random8(3); 
            headPtr[m] = 0; 
            
            // 50/50 mix: Solid vibrant colors OR flowing organic gradients
            if (random8(2) == 0) {
                mousePals[m] = CRGBPalette16(PaletteUtils::getRandomVibrantColor());
            } else {
                mousePals[m] = PaletteUtils::getRandomOrganicPalette();
            }

            for (int i = 0; i < tailLength; i++) { 
                hx[m][i] = x[m]; hy[m][i] = y[m]; hz[m][i] = z[m]; 
            } 
        }

        while (millis() - startTime < durationMs) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now; masterPhase += 0.133f * deltaMs; 
            
            for (int m = 0; m < numMice; m++) {
                switch(axis[m]) {
                    case 0: if (dir[m] == 1) { x[m]++; if (x[m] >= RNDR_X - 1) { x[m] = RNDR_X - 1; axis[m] = 1; dir[m] = (y[m] < RNDR_CY) ? 1 : -1; } } else { x[m]--; if (x[m] <= 0) { x[m] = 0; axis[m] = 2; dir[m] = (z[m] < RNDR_CZ) ? 1 : -1; } } if (random8(5) == 0) axis[m] = (random8(2) == 0) ? 1 : 2; break;
                    case 1: if (dir[m] == 1) { y[m]++; if (y[m] >= RNDR_Y - 1) { y[m] = RNDR_Y - 1; axis[m] = 2; dir[m] = (z[m] < RNDR_CZ) ? 1 : -1; } } else { y[m]--; if (y[m] <= 0) { y[m] = 0; axis[m] = 0; dir[m] = (x[m] < RNDR_CX) ? 1 : -1; } } if (random8(5) == 0) axis[m] = (random8(2) == 0) ? 2 : 0; break;
                    case 2: if (dir[m] == 1) { z[m]++; if (z[m] >= RNDR_Z - 1) { z[m] = RNDR_Z - 1; axis[m] = 0; dir[m] = (x[m] < RNDR_CX) ? 1 : -1; } } else { z[m]--; if (z[m] <= 0) { z[m] = 0; axis[m] = 1; dir[m] = (y[m] < RNDR_CY) ? 1 : -1; } } if (random8(5) == 0) axis[m] = (random8(2) == 0) ? 0 : 1; break;
                }
                headPtr[m] = (headPtr[m] + 1) % tailLength; hx[m][headPtr[m]] = x[m]; hy[m][headPtr[m]] = y[m]; hz[m][headPtr[m]] = z[m];
            }
            
            clearAll();
            
            for (int m = 0; m < numMice; m++) { 
                for (int i = 0; i < tailLength; i++) { 
                    int age = (headPtr[m] - i + tailLength) % tailLength; 
                    uint8_t px = hx[m][i], py = hy[m][i], pz = hz[m][i]; 
                    uint8_t colorIndex = (px * 16) + (py * 16) + (pz * 16) + (uint32_t)masterPhase; 
                    
                    // Pull color from this specific mouse's assigned palette array
                    CRGB color = ColorFromPalette(mousePals[m], colorIndex, 255, LINEARBLEND); 
                    
                    uint8_t brightness = 255 - (age * (255 / tailLength)); 
                    if (brightness > 10) { 
                        color.nscale8(brightness); 
                        
                        // Additive blending allows beams to mix white-hot when crossing paths
                        setVoxel(px, py, pz, getVoxel(px, py, pz) + color); 
                    } 
                } 
            }
            
            showCube(); 
            if (speedMs > 0) delay(speedMs); else yield();
        }
    }
};