#pragma once
#include "CubeCore.h"
#include "TextureEngine.h"
#include "AppMemory.h"

enum BlackHoleOrbit {
    ORBIT_SYNC = 0,    // One majestic Saturn ring (All stars share the exact same 3D plane)
    ORBIT_SPLIT = 1,   // Dual-axis gyroscope (Inner ring and Outer ring tumble independently)
    ORBIT_CHAOTIC = 2  // Quantum swarm (Every single star gets a unique, randomized trajectory)
};
    // ==========================================
    // 3D BPM ENGINE (Volumetric Density Waves)
    // ==========================================
    enum BpmSpatialMode {
        BPM_CHECKERBOARD = 0, // 2D serpentine scatter extruded to columns
        BPM_SPHERE = 1,       // 3D radial expansion from the core
        BPM_DIAGONAL = 2,     // 3D angled slicing planes
        BPM_HELIX = 3         // 3D twisting spiral staircase
    };

    // ==========================================
    // 3D CHUNCHUN (Volumetric Swarms)
    // ==========================================
    enum ChunchunMode {
        CHUNCHUN_SERPENTINE = 0, // Literal 1D raster sweep extruded to 3D columns
        CHUNCHUN_LISSAJOUS = 1   // Continuous multi-axis figure-8 weave
    };

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
            
            fadeAll(75);

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
        
        // Pull dynamically from the full non-solid pool
        TexturePlan plan = PaletteUtils::getRandomNonSolidTexturePlan();
        TextureState tex(plan.sMode, plan.palette, 1.0f);

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
    // LAVA LAMP (True Metaballs + Soft Body Physics)
    // ==========================================
    struct LavaBlob {
        float x, y, z;
        float vx, vy, vz;
        float targetX, targetY; 
        bool isRising;          
        float baseRadius; 
        float currentRadius; // Active pulsation
        float pulsePhase;    // Active pulsation
        float pulseSpeed;    // Active pulsation
        float speedFactor;      
        float hue;           // Independent thermal color
    };

    static void animateLavaLamp(uint32_t durationMs, float speedMultiplier = 1.0f, float volumePercent = 0.15f, CRGBPalette16 pal = LavaColors_p) {
        uint32_t startTime = millis();
        uint32_t lastFrameTime = millis();

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

        // INITIALIZATION
        for (int i = 0; i < NUM_BLOBS; i++) {
            float blobVolume = targetFluidVolume * (weights[i] / sumWeights);
            blobs[i].baseRadius = powf(blobVolume * 0.238732f, 0.333333f) * 0.65f;
            blobs[i].currentRadius = blobs[i].baseRadius;
            
            blobs[i].pulsePhase = random16(0, 6283) / 1000.0f; // 0 to 2PI
            blobs[i].pulseSpeed = 1.5f + (random8() / 255.0f) * 2.0f; 

            blobs[i].x = glassPadding + ((float)random8() / 255.0f) * (RNDR_X - glassPadding * 2.0f);
            blobs[i].y = glassPadding + ((float)random8() / 255.0f) * (RNDR_Y - glassPadding * 2.0f);
            blobs[i].z = ((float)random8() / 255.0f) * (RNDR_Z - 2.0f) + 1.0f; 
            
            blobs[i].targetX = glassPadding + ((float)random8() / 255.0f) * (RNDR_X - glassPadding * 2.0f);
            blobs[i].targetY = glassPadding + ((float)random8() / 255.0f) * (RNDR_Y - glassPadding * 2.0f);
            
            blobs[i].vx = 0; blobs[i].vy = 0; blobs[i].vz = 0;
            blobs[i].isRising = (random8(2) == 0); 
            blobs[i].speedFactor = 0.5f + ((float)random8() / 255.0f); 
            
            blobs[i].hue = random8(); // Stagger initial thermal colors
        }

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrameTime;
            if (deltaMs == 0) { yield(); continue; }
            lastFrameTime = now;

            clearAll(); 

            float dt = ((float)deltaMs / 16.0f) * speedMultiplier; 

            // ==========================================
            // SOFT-BODY PHYSICS LOOP
            // ==========================================
            for (int i = 0; i < NUM_BLOBS; i++) {
                
                // 1. Independent Breathing (Radius Pulsation)
                blobs[i].pulsePhase += blobs[i].pulseSpeed * (deltaMs / 1000.0f);
                // Pulse size between 80% and 120% of base mass
                blobs[i].currentRadius = blobs[i].baseRadius * (1.0f + 0.2f * sinf(blobs[i].pulsePhase));

                // 2. Thermal Color Drift
                blobs[i].hue += 5.0f * (deltaMs / 1000.0f) * speedMultiplier;
                if (blobs[i].hue >= 255.0f) blobs[i].hue -= 255.0f;

                // 3. X/Y Target Seeking
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

                // 4. Z-Axis Buoyancy
                float buoyancyAccel = 0.025f * blobs[i].speedFactor * dt;
                if (blobs[i].isRising) {
                    blobs[i].vz += buoyancyAccel;
                    if (blobs[i].z >= RNDR_Z - 1.5f) blobs[i].isRising = false; 
                } else {
                    blobs[i].vz -= buoyancyAccel;
                    if (blobs[i].z <= 1.0f) blobs[i].isRising = true; 
                }

                // 5. The "Wall Squish" (Soft Deceleration)
                float squishBoundaryX = glassPadding + blobs[i].currentRadius * 0.5f;
                float squishBoundaryY = glassPadding + blobs[i].currentRadius * 0.5f;

                if (blobs[i].x < squishBoundaryX) {
                    blobs[i].vx += (squishBoundaryX - blobs[i].x) * 0.05f * dt; 
                    blobs[i].vx *= 0.85f; // Viscous friction against the glass
                } else if (blobs[i].x > RNDR_X - squishBoundaryX) {
                    blobs[i].vx -= (blobs[i].x - (RNDR_X - squishBoundaryX)) * 0.05f * dt;
                    blobs[i].vx *= 0.85f;
                }

                if (blobs[i].y < squishBoundaryY) {
                    blobs[i].vy += (squishBoundaryY - blobs[i].y) * 0.05f * dt;
                    blobs[i].vy *= 0.85f;
                } else if (blobs[i].y > RNDR_Y - squishBoundaryY) {
                    blobs[i].vy -= (blobs[i].y - (RNDR_Y - squishBoundaryY)) * 0.05f * dt;
                    blobs[i].vy *= 0.85f;
                }

                // Apply general drag and commit velocity
                float drag = powf(0.88f, dt); 
                blobs[i].vx *= drag;
                blobs[i].vy *= drag;
                blobs[i].vz *= drag;

                blobs[i].x += blobs[i].vx * dt;
                blobs[i].y += blobs[i].vy * dt;
                blobs[i].z += blobs[i].vz * dt;
                
                // Floor/Ceiling physical locks
                if (blobs[i].z < 0.0f) { blobs[i].z = 0.0f; blobs[i].vz *= 0.5f; }
                if (blobs[i].z > RNDR_Z - 1.0f) { blobs[i].z = RNDR_Z - 1.0f; blobs[i].vz *= 0.5f; }
            }

            // ==========================================
            // ENERGY-WEIGHTED RENDER LOOP (Deferred Rendering)
            // ==========================================
            float skinThreshold = (RNDR_X <= 8) ? 0.85f : 0.75f;
            float coreThreshold = skinThreshold + 0.30f; 
            
            float r2[NUM_BLOBS];
            float stretchZ[NUM_BLOBS];
            CRGB blobColors[NUM_BLOBS];
            
            for (int i = 0; i < NUM_BLOBS; i++) {
                r2[i] = blobs[i].currentRadius * blobs[i].currentRadius;
                stretchZ[i] = 1.0f - constrain(fabsf(blobs[i].vz) * 1.5f, 0.0f, 0.25f);
                blobColors[i] = ColorFromPalette(pal, (uint8_t)blobs[i].hue, 255, LINEARBLEND);
            }

            for (uint8_t z = 0; z < RNDR_Z; z++) {
                // LOOP HOISTING: Calculate Z-axis math once per layer, not 256 times
                float dzSq[NUM_BLOBS];
                for (int i = 0; i < NUM_BLOBS; i++) {
                    float dz = ((float)z - blobs[i].z) * stretchZ[i]; 
                    dzSq[i] = dz * dz;
                }

                for (uint8_t y = 0; y < RNDR_Y; y++) {
                    // LOOP HOISTING: Calculate Y-axis math once per row, not 16 times
                    float dySq[NUM_BLOBS];
                    for (int i = 0; i < NUM_BLOBS; i++) {
                        float dy = (float)y - blobs[i].y;
                        dySq[i] = dy * dy;
                    }

                    for (uint8_t x = 0; x < RNDR_X; x++) {
                        float totalEnergy = 0.0f;
                        float energies[NUM_BLOBS];
                        
                        for (int i = 0; i < NUM_BLOBS; i++) {
                            float dx = (float)x - blobs[i].x;
                            float d2 = (dx*dx) + dySq[i] + dzSq[i];
                            
                            // Calculate raw energy (Fastest operation possible)
                            energies[i] = r2[i] / (d2 + 0.1f);
                            totalEnergy += energies[i];
                        }

                        // DEFERRED RENDERING: 
                        // Only mix RGB floats if this voxel actually contains lava!
                        if (totalEnergy >= skinThreshold) {
                            float rSum = 0.0f, gSum = 0.0f, bSum = 0.0f;
                            for (int i = 0; i < NUM_BLOBS; i++) {
                                rSum += blobColors[i].r * energies[i];
                                gSum += blobColors[i].g * energies[i];
                                bSum += blobColors[i].b * energies[i];
                            }
                            
                            CRGB finalColor;
                            finalColor.r = constrain((int)(rSum / totalEnergy), 0, 255);
                            finalColor.g = constrain((int)(gSum / totalEnergy), 0, 255);
                            finalColor.b = constrain((int)(bSum / totalEnergy), 0, 255);

                            if (totalEnergy >= coreThreshold) {
                                setVoxel(x, y, z, finalColor);
                            } else {
                                float depth = (totalEnergy - skinThreshold) / (coreThreshold - skinThreshold);
                                uint8_t dimFactor = 40 + (uint8_t)(depth * 160.0f);
                                finalColor.nscale8(dimFactor); 
                                setVoxel(x, y, z, finalColor);
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

    // ==========================================
    // BLACK HOLE (Lissajous Orbits)
    // ==========================================
    // Master Director
    static void animateBlackHole(uint32_t durationMs, uint32_t speedMs, CRGBPalette16 pal, ShaderMode sMode, BlackHoleOrbit orbitStyle = ORBIT_SYNC) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        
        // Dynamically scale the star count so it looks dense on both 8x8x16 and 16^3
        const int numOuter = (RNDR_X > 8) ? 12 : 6;
        const int numInner = (RNDR_X > 8) ? 6 : 3;
        
        // Construct the 3D paint brush based on user arguments
        TextureState tex(sMode, pal, 1.0f);
        
        float tBase = 0.0f;

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrame;
            if (deltaMs == 0) { yield(); continue; }
            lastFrame = now;
            
            tBase += (deltaMs / 1000.0f) * 1.5f; // Global orbit speed scalar

            // 1. VOLUMETRIC FADE (The Comet Trails)
            fadeAll(55);
        
            // 2. THE SINGULARITY
            // A persistent, crisp white dot at the dead center of the array
            int cx = (int)RNDR_CX;
            int cy = (int)RNDR_CY;
            int cz = (int)RNDR_CZ;
            setVoxel(cx, cy, cz, CRGB::White);

            // 3. DRAW ORBITS
            auto drawOrbit = [&](int count, float radiusMultiplier, int tier) {
                for (int i = 0; i < count; i++) {
                    uint16_t phaseX, phaseY, phaseZ;
                    
                    if (orbitStyle == ORBIT_SYNC) {
                        // All stars share the same frequency multipliers. 
                        // The (i * 8192) evenly spaces them around the single ring.
                        phaseX = (uint16_t)(tBase * 11000) + (i * 8192);
                        phaseY = (uint16_t)(tBase * 13000) + (i * 8192);
                        phaseZ = (uint16_t)(tBase * 9000)  + (i * 8192);
                    } 
                    else if (orbitStyle == ORBIT_SPLIT) {
                        // Frequencies are aggressively shifted based on the 'tier' (0 or 1).
                        phaseX = (uint16_t)(tBase * (11000 + tier * 4500)) + (i * 8192);
                        phaseY = (uint16_t)(tBase * (13000 + tier * 3500)) + (i * 8192);
                        phaseZ = (uint16_t)(tBase * (9000  + tier * 5500)) + (i * 8192);
                    } 
                    else { // ORBIT_CHAOTIC
                        // Generate a globally unique ID for each star to decouple every plane
                        int uniqueId = (tier * 10) + i; 
                        phaseX = (uint16_t)(tBase * (11000 + uniqueId * 1500)) + (i * 8192);
                        phaseY = (uint16_t)(tBase * (13000 - uniqueId * 1100)) + (i * 8192);
                        phaseZ = (uint16_t)(tBase * (9000  + uniqueId * 1800)) + (i * 8192);
                    }

                    // Map the sine waves (-32767 to 32767) to the physical rendering limits
                    float xPos = RNDR_CX + (sin16(phaseX) / 32768.0f) * (RNDR_CX * radiusMultiplier);
                    float yPos = RNDR_CY + (sin16(phaseY) / 32768.0f) * (RNDR_CY * radiusMultiplier);
                    float zPos = RNDR_CZ + (sin16(phaseZ) / 32768.0f) * (RNDR_CZ * radiusMultiplier);

                    int px = constrain((int)roundf(xPos), 0, RNDR_X - 1);
                    int py = constrain((int)roundf(yPos), 0, RNDR_Y - 1);
                    int pz = constrain((int)roundf(zPos), 0, RNDR_Z - 1);

                    CRGB starColor = tex.getColor(px, py, pz);
                    CRGB existing = getVoxel(px, py, pz);
                    setVoxel(px, py, pz, existing + starColor);
                }
            };

            // Outer stars: Tier 0
            drawOrbit(numOuter, 1.0f, 0);

            // Inner stars: Tier 1
            drawOrbit(numInner, 0.45f, 1);

            showCube();
            tex.advance(deltaMs); // Push the internal texture clock forward
            if (speedMs > 0) delay(speedMs); else yield();
        }
    }
    // Overload: Shader Mode + Custom Orbit (Defaults to Rainbow Palette)
    static void animateBlackHole(uint32_t durationMs, uint32_t speedMs, ShaderMode sMode, BlackHoleOrbit orbitStyle = ORBIT_SYNC) { 
        animateBlackHole(durationMs, speedMs, RainbowColors_p, sMode, orbitStyle); 
    }
    
    // Overload: The Ultimate Default (Rainbow + Linear Flow + Synchronized)
    static void animateBlackHole(uint32_t durationMs, uint32_t speedMs = 15) { 
        animateBlackHole(durationMs, speedMs, RainbowColors_p, MODE_LINEAR_FLOW, ORBIT_SYNC); 
    }

    // ==========================================
    // 3D BOUNCING BALLS (Juggler's Matrix)
    // ==========================================
    struct BouncingBall {
        uint32_t lastBounceTime;
        float impactVelocity;
        float height;
    };

    static void animateBouncingBalls(uint32_t durationMs, CRGBPalette16 pal = RainbowColors_p, uint8_t gravityParam = 128, uint8_t numBallsParam = 128) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();

        // 1. DYNAMIC MEMORY SCALING
        // SharedAppMemory allows 8192 bytes (8x8x16) or 29000 bytes (16^3).
        // Each ball struct is 12 bytes. 
        // 8x8 requires 64 columns. Max balls = 8192 / (64 * 12) = 10 balls per col.
        // 16x16 requires 256 columns. Max balls = 29000 / (256 * 12) = 9 balls per col.
        const int MAX_BALLS_PER_COL = (RNDR_X > 8) ? 9 : 10; 
        const int TOTAL_COLS = RNDR_X * RNDR_Y;
        
        // Cast the global arena
        BouncingBall* balls = (BouncingBall*)SharedAppMemory;

        // Map the 0-255 parameter to actual ball counts based on memory limits
        unsigned numBalls = (numBallsParam * (MAX_BALLS_PER_COL - 1)) / 255 + 1;

        // 2. WLED PHYSICS TRANSLATION
        // WLED alters gravity by dividing the time delta. 128 = half speed.
        float timeDivisor = ((255 - gravityParam) / 64.0f) + 1.0f;
        const float gravity = -9.81f; 

        // INITIALIZATION
        for (int c = 0; c < TOTAL_COLS; c++) {
            for (unsigned i = 0; i < numBalls; i++) {
                int idx = c * MAX_BALLS_PER_COL + i;
                balls[idx].lastBounceTime = startTime;
                
                // Randomize initial kick velocity so they immediately decouple
                float impactVelocityStart = sqrtf(-2.0f * gravity) * (random8(5, 11) / 10.0f);
                balls[idx].impactVelocity = impactVelocityStart;
                balls[idx].height = 0.0f;
            }
        }

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrame;
            if (deltaMs == 0) { yield(); continue; }
            lastFrame = now;

            clearAll();

            for (int x = 0; x < RNDR_X; x++) {
                for (int y = 0; y < RNDR_Y; y++) {
                    int c = x + y * RNDR_X;

                    for (unsigned i = 0; i < numBalls; i++) {
                        int idx = c * MAX_BALLS_PER_COL + i;
                        
                        float timeSinceLastBounce = (now - balls[idx].lastBounceTime) / timeDivisor;
                        float timeSec = timeSinceLastBounce / 1000.0f;
                        
                        // Physics: Height = 1/2 * a * t^2 + v * t
                        balls[idx].height = (0.5f * gravity * timeSec + balls[idx].impactVelocity) * timeSec;

                        // Floor Collision
                        if (balls[idx].height <= 0.0f) {
                            balls[idx].height = 0.0f;
                            
                            // WLED Dampening: Balls lose energy at different rates based on their ID
                            float dampening = 0.9f - (float)i / (float)(numBalls * numBalls);
                            balls[idx].impactVelocity *= dampening;
                            balls[idx].lastBounceTime = now;

                            // If it rests on the floor, give it a new random kick to keep it alive
                            if (balls[idx].impactVelocity < 0.015f) {
                                balls[idx].impactVelocity = sqrtf(-2.0f * gravity) * (random8(5, 11) / 10.0f);
                            }
                        } else if (balls[idx].height > 1.0f) {
                            continue; // Prevent clipping outside the top glass
                        }

                        // 3. PHYSICAL Z-MAPPING
                        int z = constrain((int)roundf(balls[idx].height * (RNDR_Z - 1)), 0, RNDR_Z - 1);

                        // 4. COLOR ASSIGNMENT (WLED Independence Logic)
                        // Binds a specific palette color strictly to the ball's ID
                        uint8_t colorIndex = i * (256 / max((int)numBalls, 8));
                        CRGB color = ColorFromPalette(pal, colorIndex, 255, LINEARBLEND);

                        // Additive blending allows colors to mix beautifully when passing through each other
                        CRGB existing = getVoxel(x, y, z);
                        setVoxel(x, y, z, existing + color);
                    }
                }
            }
            showCube();
            delay(15); 
        }
    }
    
    static void animateBpmVolumetric(uint32_t durationMs, CRGBPalette16 pal = RainbowColors_p, uint8_t effectSpeed = 64, BpmSpatialMode mode = BPM_CHECKERBOARD) {
        // Map the 0-255 speed parameter to a usable FastLED BPM range (10 to 120 BPM)
        uint8_t bpm = map(effectSpeed, 0, 255, 10, 120);
        
        // -------------------------------------------------------------------------
        // 1. THE PRE-COMPUTATION ENGINE (HEAP-FREE)
        // Map the phase array directly to the global scratchpad. Zero fragmentation.
        // -------------------------------------------------------------------------
        uint8_t* phaseMap = (uint8_t*)SharedAppMemory;
        
        float cx = RNDR_CX;
        float cy = RNDR_CY;
        float cz = RNDR_CZ;
        float maxDist = sqrtf(cx*cx + cy*cy + cz*cz);
        if (maxDist < 1.0f) maxDist = 1.0f; 
        
        uint8_t zStep = 256 / RNDR_Z;
        uint8_t diagStep = 256 / (RNDR_X + RNDR_Y + RNDR_Z);

        uint32_t idx = 0;
        for (uint8_t z = 0; z < RNDR_Z; z++) {
            for (uint8_t y = 0; y < RNDR_Y; y++) {
                for (uint8_t x = 0; x < RNDR_X; x++) {
                    uint8_t spatialPhase = 0;
                    
                    if (mode == BPM_CHECKERBOARD) {
                        bool isEven = ((x + y) % 2 == 0);
                        spatialPhase = isEven ? (z * zStep) : (256 - (z * zStep));
                    } 
                    else if (mode == BPM_SPHERE) {
                        float dx = (float)x - cx; 
                        float dy = (float)y - cy; 
                        float dz = (float)z - cz;
                        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
                        spatialPhase = (uint8_t)((dist / maxDist) * 255.0f);
                    }
                    else if (mode == BPM_DIAGONAL) {
                        spatialPhase = (uint8_t)((x + y + z) * diagStep);
                    }
                    else if (mode == BPM_HELIX) {
                        float angle = atan2f((float)y - cy, (float)x - cx);
                        uint8_t anglePhase = (uint8_t)((angle + 3.14159f) * 40.74f); 
                        spatialPhase = anglePhase + (z * zStep);
                    }
                    
                    phaseMap[idx++] = spatialPhase;
                }
            }
        }


       // -------------------------------------------------------------------------
        // 2. THE RENDER ENGINE
        // -------------------------------------------------------------------------
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            if (now - lastFrame == 0) { yield(); continue; }
            lastFrame = now;

            uint8_t beat = beatsin8(bpm, 64, 255);
            uint8_t timeBase = now / 15; 

            idx = 0; // Reset lookup index for the new frame
            
            for (uint8_t z = 0; z < RNDR_Z; z++) {
                // yield(); <-- REMOVED: Do not context-switch 16 times per frame!
                for (uint8_t y = 0; y < RNDR_Y; y++) {
                    for (uint8_t x = 0; x < RNDR_X; x++) {
                        
                        // O(1) Memory Lookup replaces all branching and float math
                        uint8_t spatialPhase = phaseMap[idx++];
                        
                        uint8_t brightness = beat - timeBase + spatialPhase;
                        brightness = qsub8(brightness, 32);     
                        brightness = qadd8(brightness, brightness); 
                        
                        // BIT SHIFT: Divides by 2 directly in the ALU
                        uint8_t colorIndex = timeBase + (spatialPhase >> 1);
                        
                        CRGB finalColor = ColorFromPalette(pal, colorIndex, brightness, LINEARBLEND);
                        setVoxel(x, y, z, finalColor);
                    }
                }
            }
            showCube();
            delay(15);
        }
    }
    
    
    static void animateChunchunVolumetric(uint32_t durationMs, CRGBPalette16 pal = RainbowColors_p, uint8_t effectSpeed = 128, uint8_t gapSizeParam = 128, ChunchunMode mode = CHUNCHUN_SERPENTINE) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        
        const unsigned numBirds = (RNDR_X > 8) ? 30 : 12;
        const uint32_t totalVoxels = RNDR_X * RNDR_Y * RNDR_Z;

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            if (now - lastFrame == 0) { yield(); continue; }
            lastFrame = now;

           // 1. WLED TRAIL (Fade to create comets)
            uint8_t fadeAmt = (mode == CHUNCHUN_SERPENTINE) ? 75 : 35;
            fadeAll(fadeAmt);

            // 2. THE WLED TIMELINE MATH
            uint32_t baseCounter = now * (6 + (effectSpeed >> 4));
            unsigned span = (gapSizeParam << 8) / numBirds;

            for (unsigned i = 0; i < numBirds; i++) {
                
                uint32_t birdCounter = baseCounter - (i * span);
                uint16_t megumin = sin16((uint16_t)birdCounter) + 0x8000;
                
                int px = 0, py = 0, pz = 0;

                // 3. THE 3D ARCHITECTURES
                if (mode == CHUNCHUN_SERPENTINE) {
                    uint32_t voxelIndex = (uint32_t(megumin) * totalVoxels) >> 16;
                    voxelIndex = constrain(voxelIndex, 0, totalVoxels - 1);

                    pz = voxelIndex / (RNDR_X * RNDR_Y);
                    int floorIndex = voxelIndex % (RNDR_X * RNDR_Y);
                    py = floorIndex / RNDR_X;
                    px = floorIndex % RNDR_X;

                    if (py % 2 != 0) px = (RNDR_X - 1) - px;
                    if (pz % 2 != 0) py = (RNDR_Y - 1) - py;

                } 
                else if (mode == CHUNCHUN_LISSAJOUS) {
                    uint16_t xPhase = (uint16_t)(birdCounter * 3);
                    uint16_t yPhase = (uint16_t)(birdCounter * 4);
                    uint16_t zPhase = (uint16_t)(birdCounter * 5);
                    
                    px = constrain((int)roundf(RNDR_CX + (sin16(xPhase) / 32768.0f) * RNDR_CX), 0, RNDR_X - 1);
                    py = constrain((int)roundf(RNDR_CY + (sin16(yPhase) / 32768.0f) * RNDR_CY), 0, RNDR_Y - 1);
                    pz = constrain((int)roundf(RNDR_CZ + (sin16(zPhase) / 32768.0f) * RNDR_CZ), 0, RNDR_Z - 1);
                }

                // 4. WLED COLOR MAPPING
                uint8_t colorIndex = (i * 255) / numBirds;
                CRGB birdColor = ColorFromPalette(pal, colorIndex, 255, LINEARBLEND);

                CRGB existing = getVoxel(px, py, pz);
                setVoxel(px, py, pz, existing + birdColor);
            }

            showCube();
            delay(15); 
        }
    }
    // ==========================================
    // COLORED BURSTS (Auto-Scaling 3D Rays)
    // ==========================================
  /*  static void animateColoredBursts(uint32_t durationMs, uint8_t effectSpeed = 128) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        
        // 1. AUTOMATIC VOLUME SCALING
        // Square root curve: 4x the volume yields 2x the rays.
        uint32_t volume = RNDR_X * RNDR_Y * RNDR_Z;
        uint8_t numLines = max(1, (int)(sqrt(volume) / 8.0f));
        
        // Scale WLED speed to a usable FastLED BPM range
        uint8_t speed = (effectSpeed / 16); 
        uint8_t hueBase = 0;

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            if (now - lastFrame == 0) { yield(); continue; }
            lastFrame = now;

            hueBase++; 

            // 2. WLED FADE (The smeared trails)
            fadeAll(55);

            // 3. ORIGIN CALCULATION
            // X and Y drift around the core; Z sweeps the height.
            uint8_t ox = beatsin8(1 + speed, RNDR_CX - 2, RNDR_CX + 2);
            uint8_t oy = beatsin8(2 + speed, RNDR_CY - 2, RNDR_CY + 2);
            uint8_t oz = beatsin8(2 + speed, 0, RNDR_Z - 1); 

            // 4. RAY TRACING LOOP
            for (size_t i = 0; i < numLines; i++) {
                
                uint8_t dx = beatsin8(5 + speed, 0, RNDR_X - 1, 0, i * 24);
                uint8_t dy = beatsin8(4 + speed, 0, RNDR_Y - 1, 0, i * 36);
                uint8_t dz = beatsin8(3 + speed, 0, RNDR_Z - 1, 0, (i * 48) + 64);

                uint8_t xsteps = abs8(ox - dx) + 1;
                uint8_t ysteps = abs8(oy - dy) + 1;
                uint8_t zsteps = abs8(oz - dz) + 1;
                uint8_t steps = max(xsteps, max(ysteps, zsteps));

                // 5. COLOR LOCK (Hardcoded Rainbow)
                uint8_t colorIndex = (i * 255 / numLines) + hueBase;
                CRGB rayColor = ColorFromPalette(RainbowColors_p, colorIndex, 255, LINEARBLEND);

                // 6. DRAW SOLID 3D LINE
                for (size_t j = 1; j <= steps; j++) {
                    uint8_t rate = j * 255 / steps;
                    
                    uint8_t px = lerp8by8(ox, dx, rate);
                    uint8_t py = lerp8by8(oy, dy, rate);
                    uint8_t pz = lerp8by8(oz, dz, rate);

                    setVoxel(px, py, pz, rayColor); 
                }
            }

            showCube();
            delay(15);
        }
    }*/

    // ==========================================
    // COLORED BURSTS (Auto-Scaling Anti-Aliased Rays)
    // ==========================================
      static void animateColoredBursts(uint32_t durationMs, uint8_t effectSpeed = 128) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        
        uint32_t volume = RNDR_X * RNDR_Y * RNDR_Z;
        uint8_t numLines = max(1, (int)(sqrt(volume) / 8.0f));
        
        uint8_t speed = (effectSpeed / 16); 
        uint8_t hueBase = 0;

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            if (now - lastFrame == 0) { yield(); continue; }
            lastFrame = now;

            hueBase++; 
            fadeAll(55);

            // 1. ORIGIN CALCULATION (Upgraded to 16-bit sub-voxel precision)
            int32_t ox_fp = beatsin16(1 + speed, (int32_t)((RNDR_CX - 2.0f) * 256.0f), (int32_t)((RNDR_CX + 2.0f) * 256.0f));
            int32_t oy_fp = beatsin16(2 + speed, (int32_t)((RNDR_CY - 2.0f) * 256.0f), (int32_t)((RNDR_CY + 2.0f) * 256.0f));
            int32_t oz_fp = beatsin16(2 + speed, 0, (RNDR_Z - 1) * 256); 

            // 2. RAY TRACING LOOP
            for (size_t i = 0; i < numLines; i++) {
                
                // Scale the old 8-bit phases directly into the new 16-bit range
                uint16_t phaseX = i * 6144;         // i * 24 * 256
                uint16_t phaseY = i * 9216;         // i * 36 * 256
                uint16_t phaseZ = (i * 12288) + 16384; // (i * 48 + 64) * 256

                int32_t dx_fp = beatsin16(5 + speed, 0, (RNDR_X - 1) * 256, 0, phaseX);
                int32_t dy_fp = beatsin16(4 + speed, 0, (RNDR_Y - 1) * 256, 0, phaseY);
                int32_t dz_fp = beatsin16(3 + speed, 0, (RNDR_Z - 1) * 256, 0, phaseZ);

                uint8_t colorIndex = (i * 255 / numLines) + hueBase;
                CRGB rayColor = ColorFromPalette(RainbowColors_p, colorIndex, 255, LINEARBLEND);

                // 3. DRAW ANTI-ALIASED 3D LINE
                GeometryEngine::drawWuLine3D(ox_fp, oy_fp, oz_fp, dx_fp, dy_fp, dz_fp, rayColor);
            }
            showCube();
            delay(15);
        }
    }
    // ==========================================
    // COLOR TWINKLES (Volumetric Sparkle)
    // ==========================================
    static void animateColorTwinkles(uint32_t durationMs, uint8_t effectSpeed = 128, uint8_t spawnSpeed = 128) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        
        const uint32_t totalVoxels = RNDR_X * RNDR_Y * RNDR_Z;
        
        // 1. MEMORY OPTIMIZATION (No Heap Allocation)
        // 1 bit per voxel. 512 bytes for a 16x16x16 cube.
        const uint32_t stateBytes = (totalVoxels + 7) / 8;
        
        // Zero out the shared app memory block so we don't inherit garbage 
        // data from whatever effect was running before this one.
        memset(SharedAppMemory, 0, stateBytes);

        uint8_t fadeUpAmount = 8 + (effectSpeed >> 2);
        uint8_t fadeDownAmount = 8 + (effectSpeed >> 3);

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            if (now - lastFrame < 15) { yield(); continue; } 
            lastFrame = now;

            // 2. FADE LOOP 
            for (uint32_t i = 0; i < totalVoxels; i++) {
                
                uint8_t x = (i % (RNDR_X * RNDR_Y)) % RNDR_X;
                uint8_t y = (i % (RNDR_X * RNDR_Y)) / RNDR_X;
                uint8_t z = i / (RNDR_X * RNDR_Y);

                CRGB c = getVoxel(x, y, z);
                if (!c) continue; 

                uint32_t byteIndex = i >> 3;
                uint8_t bitNum = i & 0x07;
                
                // Read directly from the SharedAppMemory buffer
                bool fadingUp = bitRead(SharedAppMemory[byteIndex], bitNum);

                if (fadingUp) {
                    CRGB inc = c;
                    inc.nscale8(fadeUpAmount);
                    if (!inc) inc = c; 

                    c.r = qadd8(c.r, inc.r);
                    c.g = qadd8(c.g, inc.g);
                    c.b = qadd8(c.b, inc.b);

                    if (c.r == 255 || c.g == 255 || c.b == 255) {
                        bitWrite(SharedAppMemory[byteIndex], bitNum, 0); 
                    }
                    setVoxel(x, y, z, c);
                    
                } else {
                    c.fadeToBlackBy(fadeDownAmount);
                    setVoxel(x, y, z, c);
                }
            }

            // 3. SPAWN LOOP
            uint32_t maxSpawns = (totalVoxels / 50) + 1; 
            
            for (uint32_t j = 0; j <= maxSpawns; j++) {
                
                if (random8() <= spawnSpeed) { 
                    for (uint8_t attempts = 0; attempts < 5; attempts++) {
                        uint32_t i = random16(totalVoxels);
                        uint8_t x = (i % (RNDR_X * RNDR_Y)) % RNDR_X;
                        uint8_t y = (i % (RNDR_X * RNDR_Y)) / RNDR_X;
                        uint8_t z = i / (RNDR_X * RNDR_Y);

                        if (!getVoxel(x, y, z)) { 
                            uint32_t byteIndex = i >> 3;
                            uint8_t bitNum = i & 0x07;
                            
                            // Flag the bit in SharedAppMemory to fade up
                            bitWrite(SharedAppMemory[byteIndex], bitNum, 1); 
                            
                            CRGB startColor = ColorFromPalette(RainbowColors_p, random8(), 64, NOBLEND);
                            setVoxel(x, y, z, startColor);
                            
                            break; 
                        }
                    }
                }
            }

            showCube();
        }
    }
// ==========================================
    // VOLUMETRIC DNA SPIRAL (SDF Anti-Aliased & Optimized)
    // ==========================================
    static void animateDNASpiral(uint32_t durationMs, CRGBPalette16 pal = RainbowColors_p, float speedMultiplier = 1.0f) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        
        float radius = (RNDR_X / 2.0f) - 0.5f; 
        if (radius < 1.0f) radius = 1.0f;
        
        float backboneThickness = (RNDR_X > 8) ? 1.5f : 0.9f; 
        float rungThickness     = (RNDR_X > 8) ? 1.0f : 0.6f;

        // Pre-calculate squared thresholds for the bounding boxes (Saves 12,000 sqrtf calls per frame)
        float aaPaddingSq = (backboneThickness + 1.2f) * (backboneThickness + 1.2f);
        float rungPaddingSq = (rungThickness + 1.2f) * (rungThickness + 1.2f);

        float zFreq = TWO_PI / (RNDR_Z * 0.8f);

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrame;
            if (deltaMs == 0) { yield(); continue; } 
            lastFrame = now;

            clearAll(); 

            float timePhase = (now * 0.002f * speedMultiplier);

            for (int z = 0; z < RNDR_Z; z++) {
                float phase = (z * zFreq) + timePhase;
                
                float bx1 = RNDR_CX + (sinf(phase) * radius);
                float by1 = RNDR_CY + (cosf(phase) * radius);
                float bx2 = RNDR_CX + (sinf(phase + PI) * radius);
                float by2 = RNDR_CY + (cosf(phase + PI) * radius);

                bool hasRung = (((z + (now / 200)) % 4) != 0);
                uint8_t hue = (z * 255 / RNDR_Z) + (now / 20);
                CRGB baseColor = ColorFromPalette(pal, hue, 255, LINEARBLEND);

                // Math optimization: Pre-calculate the segment length squared once per Z-layer
                float l2 = (bx2 - bx1)*(bx2 - bx1) + (by2 - by1)*(by2 - by1);

                for (int y = 0; y < RNDR_Y; y++) {
                    float fy = (float)y;
                    // Loop Hoisting: Calculate Y-distances outside the X-loop
                    float dy1 = fy - by1; float dy1Sq = dy1*dy1;
                    float dy2 = fy - by2; float dy2Sq = dy2*dy2;

                    for (int x = 0; x < RNDR_X; x++) {
                        float fx = (float)x;

                        float dx1 = fx - bx1; float dist1Sq = dx1*dx1 + dy1Sq;
                        float dx2 = fx - bx2; float dist2Sq = dx2*dx2 + dy2Sq;

                        float intensity = 0.0f;

                        // ONLY run sqrtf if the pixel is physically inside the strand's glow radius
                        if (dist1Sq < aaPaddingSq) {
                            intensity = max(intensity, 1.0f - (sqrtf(dist1Sq) / backboneThickness));
                        }
                        if (dist2Sq < aaPaddingSq) {
                            intensity = max(intensity, 1.0f - (sqrtf(dist2Sq) / backboneThickness));
                        }

                        // Rung bounding box & calculation
                        if (hasRung && intensity < 1.0f && l2 > 0.001f) {
                            float minX = min(bx1, bx2) - rungThickness - 1.2f;
                            float maxX = max(bx1, bx2) + rungThickness + 1.2f;
                            float minY = min(by1, by2) - rungThickness - 1.2f;
                            float maxY = max(by1, by2) + rungThickness + 1.2f;

                            // Bounding box check before doing heavy projection math
                            if (fx >= minX && fx <= maxX && fy >= minY && fy <= maxY) {
                                float t = max(0.0f, min(1.0f, ((fx - bx1)*(bx2 - bx1) + (fy - by1)*(by2 - by1)) / l2));
                                float projX = bx1 + t * (bx2 - bx1);
                                float projY = by1 + t * (by2 - by1);
                                float distRungSq = (fx - projX)*(fx - projX) + (fy - projY)*(fy - projY);

                                if (distRungSq < rungPaddingSq) {
                                    float rungIntensity = 1.0f - (sqrtf(distRungSq) / rungThickness);
                                    if (rungIntensity > intensity) intensity = rungIntensity;
                                }
                            }
                        }

                        if (intensity > 0.01f) {
                            uint8_t brightness = (uint8_t)(intensity * 255.0f);
                            CRGB color = baseColor;
                            color.nscale8(brightness); 
                            CRGB existing = getVoxel(x, y, z);
                            setVoxel(x, y, z, existing + color);
                        }
                    }
                }
            }
            showCube();
        }
    }
    
    
    // ==========================================
    // 3D DRIFT (Aerodynamic Drag & Golden Tumble)
    // ==========================================
    static void animateDrift3D(uint32_t durationMs, CRGBPalette16 pal = RainbowColors_p, uint8_t effectSpeed = 128) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();

        // 1. OCTANT VECTORS (The 8 baseline arms)
        const int16_t INV_SQRT3 = 147;
        const int16_t armX[8] = { INV_SQRT3,  INV_SQRT3,  INV_SQRT3,  INV_SQRT3, -INV_SQRT3, -INV_SQRT3, -INV_SQRT3, -INV_SQRT3};
        const int16_t armY[8] = { INV_SQRT3,  INV_SQRT3, -INV_SQRT3, -INV_SQRT3,  INV_SQRT3,  INV_SQRT3, -INV_SQRT3, -INV_SQRT3};
        const int16_t armZ[8] = { INV_SQRT3, -INV_SQRT3,  INV_SQRT3, -INV_SQRT3,  INV_SQRT3, -INV_SQRT3,  INV_SQRT3, -INV_SQRT3};

        float rx = RNDR_X / 2.0f;
        float ry = RNDR_Y / 2.0f;
        float rz = RNDR_Z / 2.0f;
        float cornerDist = sqrtf(rx*rx + ry*ry + rz*rz);
        
        int16_t maxR_fp = (int16_t)(cornerDist * 256.0f); 
        int16_t step_fp = 32; 

        // State variables for continuous, non-reversing motion
        static uint16_t angleX = 0, angleY = 0, angleZ = 0;
        static uint32_t masterSpin = 0;

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            if (now - lastFrame < 15) { yield(); continue; }
            lastFrame = now;

            fadeAll(155);

    
            // --- 1. OSCILLATING SPEED ENGINE (Widened Dynamic Range) ---
            static uint32_t physLastFrame = millis();
            uint32_t deltaMs = now - physLastFrame;
            physLastFrame = now;

            uint16_t breathClock = (now * effectSpeed) / 24;
            uint16_t normalizedSin = sin16(breathClock) + 32768; // 0 to 65535
            
            uint32_t baseSpeed = 20 + (effectSpeed / 3); 
            uint32_t minSpeed = baseSpeed;
            // NEW: Reduced max speed ceiling (160% instead of 240%). 
            // Adjust this number (e.g., 140 to 180) to dial in the exact peak rotation speed you want.
            uint32_t maxSpeed = (baseSpeed * 110) / 100;
            uint32_t speedRange = maxSpeed - minSpeed;

            uint32_t currentSpeed = minSpeed + ((normalizedSin * speedRange) >> 16); 
            uint32_t timeScaledSpeed = (currentSpeed * deltaMs) / 15;

            // --- 2. GOLDEN RATIO TUMBLE ---
            angleX += timeScaledSpeed * 1; 
            angleY += timeScaledSpeed * 2;  
            angleZ += timeScaledSpeed * 3; 

            float fsx = sin16(angleX) / 32768.0f, fcx = cos16(angleX) / 32768.0f;
            float fsy = sin16(angleY) / 32768.0f, fcy = cos16(angleY) / 32768.0f;
            float fsz = sin16(angleZ) / 32768.0f, fcz = cos16(angleZ) / 32768.0f;

            int16_t UX_x = (fcy * fcz) * 32767;
            int16_t UX_y = (fcy * fsz) * 32767;
            int16_t UX_z = (-fsy) * 32767;

            int16_t UY_x = (fsx * fsy * fcz - fcx * fsz) * 32767;
            int16_t UY_y = (fsx * fsy * fsz + fcx * fcz) * 32767;
            int16_t UY_z = (fsx * fcy) * 32767;

            int16_t UZ_x = (fcx * fsy * fcz + fsx * fsz) * 32767;
            int16_t UZ_y = (fcx * fsy * fsz - fsx * fcz) * 32767;
            int16_t UZ_z = (fcx * fcy) * 32767;

            // --- 3. THE AIR RESISTANCE BEND ---
            masterSpin += timeScaledSpeed * 20; 
            
            // EXTREME BEND (TRUE AERODYNAMIC DRAG)
            // In physics, drag scales with velocity squared (v^2).
            // We square the normalized speed factor (0-65535) to create a true parabolic drag curve 
            // entirely in fast 32-bit integer math.
            uint32_t dragFactor = ((uint32_t)normalizedSin * (uint32_t)normalizedSin) >> 16; 
            
            int32_t minBend = 1000; 
            int32_t maxBend = 30000;
            
            // Apply the v^2 drag factor to the bend limits
            int32_t currentBend = minBend + ((dragFactor * (maxBend - minBend)) >> 16);

            for (int16_t r_fp = 0; r_fp <= maxR_fp; r_fp += step_fp) {
                
                // Tips of the arms drag backward based on the current air resistance
                int32_t drag = (currentBend * (int32_t)r_fp) / maxR_fp;
                
                // Subtract drag from master spin (arms trail behind the core)
                uint16_t localAngle = (uint16_t)(masterSpin - drag);
                
                int16_t sTwist = sin16(localAngle);
                int16_t cTwist = cos16(localAngle);

                for (int i = 0; i < 8; i++) {
                    // Step A: Scale the base vectors outward
                    int32_t bx = (armX[i] * r_fp) >> 8;
                    int32_t by = (armY[i] * r_fp) >> 8;
                    int32_t bz = (armZ[i] * r_fp) >> 8;

                    // Step B: Apply the Aerodynamic Drag
                    int32_t tx = (bx * cTwist - by * sTwist) >> 15;
                    int32_t ty = (bx * sTwist + by * cTwist) >> 15;
                    int32_t tz = bz;

                    // Step C: Apply the 3D Tumble (Project onto the tumbling axes)
                    int32_t finalX = (tx * UX_x + ty * UY_x + tz * UZ_x) >> 15;
                    int32_t finalY = (tx * UX_y + ty * UY_y + tz * UZ_y) >> 15;
                    int32_t finalZ = (tx * UX_z + ty * UY_z + tz * UZ_z) >> 15;

                    if (RNDR_Z > RNDR_X) {
                       finalZ = (finalZ * (int32_t)RNDR_Z) / (int32_t)RNDR_X;
                    }

                    int32_t px_fp = (int32_t)(RNDR_CX * 256.0f) + finalX;
                    int32_t py_fp = (int32_t)(RNDR_CY * 256.0f) + finalY;
                    int32_t pz_fp = (int32_t)(RNDR_CZ * 256.0f) + finalZ;

                    uint8_t armBrightness = min(255, 64 + ((r_fp >> 8) * 30));
                    uint8_t colorIndex = (uint8_t)((r_fp >> 8) * 20 + (now / 20) + (i * 15));
                    CRGB col = ColorFromPalette(pal, colorIndex, armBrightness, LINEARBLEND);
                    
                    GeometryEngine::drawWuVoxel(px_fp, py_fp, pz_fp, col);
                }
            }
            showCube();
        }
    }
    
    // ==========================================
    // 3D DRIFT ROSE (Harmonic Fibonacci Mandala)
    // ==========================================
    static void animateDriftRose3D(uint32_t durationMs, CRGBPalette16 pal = RainbowColors_p, uint8_t effectSpeed = 128) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();

        const int NUM_SPOKES = (RNDR_X > 8) ? 80 : 36;
        
        // 1. THE PRE-COMPUTATION ENGINE
        // Safely map 1,080 bytes of integer arrays to AppMemory.
        // Because AppMemory is volatile, we recompute this once per scene load.
        int32_t* spokeX_fp = (int32_t*)SharedAppMemory;
        int32_t* spokeY_fp = (int32_t*)(SharedAppMemory + 360);  // 90 * sizeof(int32_t)
        int32_t* spokeZ_fp = (int32_t*)(SharedAppMemory + 720);  // 180 * sizeof(int32_t)
        
        float goldenRatio = (1.0f + sqrtf(5.0f)) / 2.0f;
        float angleInc = TWO_PI * goldenRatio;
        
        float maxR_XY = min(RNDR_X, RNDR_Y) / 2.0f;
        float maxR_Z  = RNDR_Z / 2.0f; // ALWAYS STRETCHED TO FULL VERTICAL VOLUME
        
        if (maxR_XY < 1.0f) maxR_XY = 1.0f;
        if (maxR_Z < 1.0f) maxR_Z = 1.0f;

        for (int i = 0; i < NUM_SPOKES; i++) {
            float t = (float)i / (float)NUM_SPOKES;
            float phi = acosf(1.0f - 2.0f * t);
            float theta = angleInc * i;
            
            // Bake the radius and the 256x sub-pixel multiplier directly into AppMemory
            spokeX_fp[i] = (int32_t)(sinf(phi) * cosf(theta) * maxR_XY * 256.0f);
            spokeY_fp[i] = (int32_t)(sinf(phi) * sinf(theta) * maxR_XY * 256.0f);
            spokeZ_fp[i] = (int32_t)(cosf(phi) * maxR_Z * 256.0f);
        }

        uint8_t fadeAmt = 32 + (effectSpeed >> 3);
        
        // Pre-compute center positions in fixed-point
        int32_t cx_fp = (int32_t)(RNDR_CX * 256.0f);
        int32_t cy_fp = (int32_t)(RNDR_CY * 256.0f);
        int32_t cz_fp = (int32_t)(RNDR_CZ * 256.0f);

        // 2. THE RENDER ENGINE
        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            if (now - lastFrame < 15) { yield(); continue; }
            lastFrame = now;

            // Lightning-fast hardware fade completely replaces the 4096-iteration x/y/z loop
            fadeAll(fadeAmt);

            for (int i = 0; i < NUM_SPOKES; i++) {
                uint8_t spokeBpm = min(i + 1, 255); 
                
                // ZERO-FLOAT KINEMATICS
                int32_t r_scalar = (int32_t)beatsin16(spokeBpm, 0, 65535) - 32768;

                int32_t px_fp = cx_fp + ((spokeX_fp[i] * r_scalar) >> 15);
                int32_t py_fp = cy_fp + ((spokeY_fp[i] * r_scalar) >> 15);
                int32_t pz_fp = cz_fp + ((spokeZ_fp[i] * r_scalar) >> 15);

                uint8_t colorIndex = i * (256 / NUM_SPOKES);
                CRGB col = ColorFromPalette(pal, colorIndex, 255, LINEARBLEND);
                
                GeometryEngine::drawWuVoxel(px_fp, py_fp, pz_fp, col);
            }
            showCube();
            delay(15); // Explicit DMA frame limiter
        }
    }

    /*
 // The literal ratios from the left axis of your chart
    enum LissajousRatio {
        RATIO_1_1, RATIO_2_1, RATIO_3_1, RATIO_3_2, 
        RATIO_4_3, RATIO_5_3, RATIO_5_4
    };

    // Passes strictly the Ratio and K, draws a solid line, extrudes natively on Z, 4-way Radial Symmetry
    static void animateChartExtruded(uint32_t durationMs, LissajousRatio ratio, uint8_t k, CRGBPalette16 pal = RainbowColors_p) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();

        // 1. Look up the exact frequencies from the requested chart ratio
        float nx = 1.0f, ny = 1.0f;
        switch(ratio) {
            case RATIO_1_1: nx = 1.0f; ny = 1.0f; break;
            case RATIO_2_1: nx = 2.0f; ny = 1.0f; break;
            case RATIO_3_1: nx = 3.0f; ny = 1.0f; break;
            case RATIO_3_2: nx = 3.0f; ny = 2.0f; break;
            case RATIO_4_3: nx = 4.0f; ny = 3.0f; break;
            case RATIO_5_3: nx = 5.0f; ny = 3.0f; break;
            case RATIO_5_4: nx = 5.0f; ny = 4.0f; break;
        }

        // 2. Convert 'k' (0-7) directly to a phase shift in radians
        float phaseX = k * (PI / 4.0f);

        // 3. Chunchun's Z Extrusion
        float nz = 1.0f; 

        // Center points and stretching radii
        float cx = (RNDR_X - 1) / 2.0f;
        float cy = (RNDR_Y - 1) / 2.0f;
        float cz = (RNDR_Z - 1) / 2.0f;

        // 800 points guarantees a solid, unbroken trace even on 4 strands
        const int numPoints = 800; 

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            if (now - lastFrame < 15) { yield(); continue; } // Cap at ~60fps
            lastFrame = now;

            clearAll();
            
            // Shift this over time to make the texture "flow" along the solid wires
            uint8_t colorShift = (now / 15) & 0xFF; 

            for (int i = 0; i < numPoints; i++) {
                // Map 'i' to a full 0 to 2*PI mathematical circle
                float t = (i / (float)numPoints) * TWO_PI;

                // Calculate the base string ONCE (saves massive CPU)
                float bx = sinf(nx * t + phaseX);
                float by = sinf(ny * t);
                float bz = sinf(nz * t);

                // Color mapping
                uint8_t knotPosition = (i * 255) / numPoints;
                CRGB newColor = ColorFromPalette(pal, knotPosition + colorShift, 255, LINEARBLEND);

                // 4-WAY RADIAL SYMMETRY MULTIPLEXER
                for (int rot = 0; rot < 4; rot++) {
                    float rotX, rotY;
                    
                    // Instant 90-degree rotations using coordinate swapping
                    if (rot == 0)      { rotX = bx;   rotY = by; }   // 0 degrees
                    else if (rot == 1) { rotX = -by;  rotY = bx; }   // 90 degrees
                    else if (rot == 2) { rotX = -bx;  rotY = -by; }  // 180 degrees
                    else               { rotX = by;   rotY = -bx; }  // 270 degrees

                    // STRETCH TO HARDWARE BOUNDS
                    int px = constrain((int)roundf(cx + rotX * cx), 0, RNDR_X - 1);
                    int py = constrain((int)roundf(cy + rotY * cy), 0, RNDR_Y - 1);
                    int pz = constrain((int)roundf(cz + bz * cz), 0, RNDR_Z - 1);
                    
                    // Additive blending: The core and the face intersections will glow brightly!
                    CRGB existing = getVoxel(px, py, pz);
                    setVoxel(px, py, pz, existing | newColor);
                }
            }
            showCube();
        }
    }*/
    /*
    static void animateTartan3D(uint32_t durationMs, CRGBPalette16 pal = PartyColors_p, uint8_t effectSpeed = 128) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        
        // Translate WLED speed into a discrete stepping interval
        // A speed of 128 gives ~330ms per step, creating a deliberate, gentle geometric shift
        uint32_t stepInterval = map(effectSpeed, 0, 255, 600, 60); 

        // Pre-allocate our Boolean gap arrays to keep the inner loop lightning fast
        bool gapX[RNDR_X], gapY[RNDR_Y], gapZ[RNDR_Z];

        while (millis() - startTime < durationMs) {
            uint32_t now = millis(); 
            if (now - lastFrame < 15) { yield(); continue; } 
            lastFrame = now;

            // DISCRETE STEPPING: The lattice sits perfectly still until this ticks over
            uint32_t ticks = now / stepInterval;
            
            // Offset the gaps to make the empty tunnels "step" through the room.
            // Dividing ticks makes the axes weave at different speeds.
            int offsetX = ticks % 4;
            int offsetY = (ticks / 2) % 4;
            int offsetZ = (ticks / 3) % 4; 

            // HOISTED: A voxel is in a "gap band" if it falls in the second half of a 4-voxel cycle (2 ON, 2 OFF)
            for (int x = 0; x < RNDR_X; x++) gapX[x] = ((x + offsetX) % 4) >= 2;
            for (int y = 0; y < RNDR_Y; y++) gapY[y] = ((y + offsetY) % 4) >= 2;
            for (int z = 0; z < RNDR_Z; z++) gapZ[z] = ((z + offsetZ) % 4) >= 2;

            uint8_t colorShift = (ticks * 4) & 0xFF;

            // INNER LOOP: Boolean scaffolding logic
            for (int z = 0; z < RNDR_Z; z++) {
                for (int y = 0; y < RNDR_Y; y++) {
                    for (int x = 0; x < RNDR_X; x++) {
                        
                        // Count how many "gap bands" intersect at this exact voxel
                        uint8_t numGaps = gapX[x] + gapY[y] + gapZ[z];
                        
                        // IF 2 OR 3 GAPS INTERSECT: It creates a hollow 2x2 tunnel passing entirely through the structure
                        if (numGaps >= 2) {
                            setVoxel(x, y, z, CRGB::Black); 
                        } 
                        // IF 0 OR 1 GAPS INTERSECT: It forms the solid physical lattice (beams and pillars)
                        else {
                            uint8_t hue;
                            // Color the lattice based on its structural role to mimic woven threads
                            if (numGaps == 0) hue = (x * 8 + y * 8 + z * 8); // Corner Pillar (Intersection of walls)
                            else if (gapX[x]) hue = (y * 16 + z * 16) + 40;  // X-Beam (Running through X-gap)
                            else if (gapY[y]) hue = (x * 16 + z * 16) + 80;  // Y-Beam (Running through Y-gap)
                            else              hue = (x * 16 + y * 16) + 120; // Z-Beam (Running through Z-gap)

                            // NOBLEND forces razor-sharp edges with no fuzzy math
                            setVoxel(x, y, z, ColorFromPalette(pal, hue + colorShift, 255, NOBLEND));
                        }
                    }
                }
            }
            showCube();
        }
    }
    static void animateWavingCell3D(uint32_t durationMs, CRGBPalette16 pal = PartyColors_p, uint8_t effectSpeed = 128) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        uint32_t t = 0;

        uint8_t aX = 12, aY = 15, aZ = 18; 

        while (millis() - startTime < durationMs) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame;
            if (deltaMs == 0) { yield(); continue; } lastFrame = now;

            t += (deltaMs * (effectSpeed + 1)) >> 6;
            uint8_t t8 = t & 0xFF;

            for (int z = 0; z < RNDR_Z; z++) {
                // HOISTED: Z math only runs up to 16 times per frame
                uint8_t waveZ = sin8((z * aZ) + t8);
                uint8_t cosZ = cos8(z * aZ); 
                
                for (int y = 0; y < RNDR_Y; y++) {
                    // HOISTED: Y math only runs up to 256 times per frame
                    uint8_t waveY = sin8((y * aY) + waveZ);
                    uint8_t cosYZ = cosZ + cos8(y * aY); 
                    
                    for (int x = 0; x < RNDR_X; x++) {
                        // INNER LOOP: Only the X math runs 4096 times
                        uint8_t wave = sin8((x * aX) + waveY) + cosYZ + cos8(x * aX);
                        uint8_t colorIndex = wave + (t >> 2);
                        uint8_t brightness = qadd8(wave, 64);
                        
                        setVoxel(x, y, z, ColorFromPalette(pal, colorIndex, brightness, LINEARBLEND));
                    }
                }
            }
            showCube();
        }
    }

static void animateSoapFilm3D(uint32_t durationMs, CRGBPalette16 pal = RainbowColors_p, uint8_t effectSpeed = 128) {
        uint32_t startTime = millis(), lastFrame = millis(), noiseTime = 0;
        uint16_t scale = 1200; 

        while (millis() - startTime < durationMs) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame;
            if (deltaMs == 0) { yield(); continue; } lastFrame = now;

            noiseTime += (deltaMs * effectSpeed) / 4;

            for (int z = 0; z < RNDR_Z; z++) {
                uint32_t zScale = z * scale;
                for (int y = 0; y < RNDR_Y; y++) {
                    uint32_t yScale = y * scale;
                    for (int x = 0; x < RNDR_X; x++) {
                        uint32_t xScale = x * scale;
                        
                        // Domain warping
                        uint16_t warpX = inoise16(xScale, yScale, zScale + noiseTime);
                        uint16_t warpY = inoise16(xScale + 4000, yScale + 4000, zScale + noiseTime);
                        uint8_t data = inoise8(xScale + (warpX >> 4), yScale + (warpY >> 4), zScale + (noiseTime >> 2));
                        
                        setVoxel(x, y, z, ColorFromPalette(pal, data * 3, 255, LINEARBLEND));
                    }
                }
            }
            showCube();
        }
    }

   

    static void animatePlasmaBall3D(uint32_t durationMs, CRGBPalette16 pal = RainbowColors_p, uint8_t effectSpeed = 128) {
        uint32_t startTime = millis(), lastFrame = millis(), noiseTime = 0, colorTime = 0;
        
        float baseRadius = (RNDR_X / 2.0f) - 1.5f;
        float maxDisplacement = 1.5f;
        float maxAllowedRadSq = (baseRadius + maxDisplacement) * (baseRadius + maxDisplacement);
        uint16_t scale = 1500; 

        while (millis() - startTime < durationMs) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame;
            if (deltaMs == 0) { yield(); continue; } lastFrame = now;

            noiseTime += (deltaMs * effectSpeed) / 4;
            colorTime += deltaMs / 4; 
            clearAll();

            for (int z = 0; z < RNDR_Z; z++) {
                float dz = (float)z - RNDR_CZ; float dzSq = dz * dz;
                for (int y = 0; y < RNDR_Y; y++) {
                    float dy = (float)y - RNDR_CY; float dySq = dy * dy;
                    for (int x = 0; x < RNDR_X; x++) {
                        float dx = (float)x - RNDR_CX; 
                        float distSq = dx * dx + dySq + dzSq;

                        // SDF CULLING: Skip the expensive math if we are outside the bounds of the bubble!
                        if (distSq > maxAllowedRadSq) continue; 

                        // ONLY run the expensive sqrt and noise if the voxel survived the cull
                        float dist = sqrtf(distSq);
                        uint8_t noiseVal = inoise8(x * scale, y * scale, z * scale + noiseTime);
                        float displacement = ((noiseVal / 255.0f) * (maxDisplacement * 2.0f)) - maxDisplacement;
                        float localRadius = baseRadius + displacement;

                        if (dist < localRadius) {
                            float depth = dist / localRadius;
                            uint8_t hue = (colorTime & 0xFF) + (uint8_t)(depth * 64.0f);
                            CRGB finalColor = ColorFromPalette(pal, hue, 255, LINEARBLEND);
                            if (depth < 0.4f) finalColor |= CRGB(150, 150, 150); // White hot core
                            setVoxel(x, y, z, finalColor);
                        }
                    }
                }
            }
            showCube();
        }
    }
*/
};