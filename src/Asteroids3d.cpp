#include "Asteroids3d.h"

// --- STATIC ALLOCATIONS ---
float Asteroids3d::sPx = 0, Asteroids3d::sPy = 0, Asteroids3d::sPz = 0;
float Asteroids3d::sVx = 0, Asteroids3d::sVy = 0, Asteroids3d::sVz = 0;
float Asteroids3d::yaw = 0, Asteroids3d::pitch = 0;
bool Asteroids3d::thrusting = false;
bool Asteroids3d::shieldActive = false;
uint32_t Asteroids3d::shieldStartTime = 0;
uint32_t Asteroids3d::lastFireTime = 0;

// --- MEMORY ARENA POINTERS ---
#define bullets ((Asteroids3d::Bullet*)(SharedAppMemory))
#define asteroids ((Asteroids3d::Asteroid*)(SharedAppMemory + 1000))
#define particles ((Asteroids3d::Particle*)(SharedAppMemory + 2000))

int Asteroids3d::score = 0;
uint32_t Asteroids3d::lastFrameTime = 0;

// --- FILE-SCOPED WAVE MECHANICS ---
static int currentWave = 1;
static uint32_t waveTimer = 0;

// =========================================================================
// SCALABLE HARDWARE MATH
// =========================================================================
constexpr float AST_VOL = (RNDR_X * RNDR_Y * RNDR_Z);
constexpr float AST_LARGE_RAD = (RNDR_X * 0.22f); 
constexpr float AST_SMALL_RAD = (AST_LARGE_RAD * 0.55f);
constexpr int   AST_SPAWN_COUNT = (AST_VOL >= 2000) ? 4 : 2; 
//constexpr int   AST_SPAWN_COUNT = 0; 
  


// Force a massive 7-voxel safe zone on the 8x8 tube so rocks spawn at the floor/ceiling
constexpr float AST_SAFE_ZONE = (RNDR_X <= 8) ? 7.0f : (AST_LARGE_RAD * 1.8f);
constexpr float AST_BULLET_RANGE = ((RNDR_Z > RNDR_X ? RNDR_Z : RNDR_X) * 0.6f);

// --- MATH HELPERS ---
static void wrapBoundary(float& val, float maxVal) {
    while (val < 0.0f) val += maxVal;
    while (val >= maxVal) val -= maxVal;
}

static void rotate3D(float& x, float& y, float& z, float pPitch, float pYaw) {
    // 1. Rotate around Y axis (Pitch)
    float x1 = x * cos(pPitch) - z * sin(pPitch);
    float z1 = x * sin(pPitch) + z * cos(pPitch);
    float y1 = y;
    
    // 2. Rotate around Z axis (Yaw)
    float x2 = x1 * cos(pYaw) - y1 * sin(pYaw);
    float y2 = x1 * sin(pYaw) + y1 * cos(pYaw);
    
    x = x2; y = y2; z = z1;
}

// =========================================================================
//                          GAME ENGINE LOGIC
// =========================================================================

void Asteroids3d::spawnAsteroid(bool isLarge, float px, float py, float pz) {
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!asteroids[i].active) {
            asteroids[i].active = true;
            asteroids[i].isLarge = isLarge;
            asteroids[i].px = px; asteroids[i].py = py; asteroids[i].pz = pz;
            
            // Gentler Wave Multiplier (+15% speed per wave instead of 25%)
            float waveMult = 1.0f + ((currentWave - 1) * 0.15f);
            
           // Training Speeds:
            // Large rocks: 0.4 to 0.8 units per second
            // Small rocks: 0.8 to 1.5 units per second
            //float speed = isLarge ? random(4, 8) / 10.0f : random(8, 15) / 10.0f;
            float speed = isLarge ? random(8, 16) / 10.0f : random(15, 29) / 10.0f;
            speed *= waveMult;

            float aYaw = random(0, 628) / 100.0f;
            float aPitch = random(-157, 157) / 100.0f;
            
            asteroids[i].vx = cos(aYaw) * cos(aPitch) * speed;
            asteroids[i].vy = sin(aYaw) * cos(aPitch) * speed;
            asteroids[i].vz = sin(aPitch) * speed;
            
            asteroids[i].rotX = random(0, 360);
            asteroids[i].rotY = random(0, 360);
            asteroids[i].rotZ = random(0, 360);
            break;
        }
    }
}

void Asteroids3d::resetGame() {
    sPx = RNDR_CX; sPy = RNDR_CY; sPz = RNDR_CZ;
    sVx = 0; sVy = 0; sVz = 0;
    yaw = 0; pitch = 0;
    score = 0;
    currentWave = 1;
    waveTimer = 0;
    shieldActive = false;
    
    for(int i=0; i<MAX_BULLETS; i++) bullets[i].active = false;
    for(int i=0; i<MAX_ASTEROIDS; i++) asteroids[i].active = false;
    for(int i=0; i<MAX_PARTICLES; i++) particles[i].active = false;

    // Dynamically scaled spawn count and safe zone
    for(int i=0; i<AST_SPAWN_COUNT; i++) {
        float nx, ny, nz;
        do {
            nx = random(0, RNDR_X);
            ny = random(0, RNDR_Y);
            nz = random(0, RNDR_Z);
        } while(sqrt(pow(nx-sPx,2) + pow(ny-sPy,2) + pow(nz-sPz,2)) < AST_SAFE_ZONE);
        spawnAsteroid(true, nx, ny, nz);
    }
}

void Asteroids3d::playGameOver() {
    // 1. Shatter the ship into kinetic confetti
    for(int i=0; i<MAX_PARTICLES; i++) {
        particles[i].active = true;
        particles[i].px = sPx; particles[i].py = sPy; particles[i].pz = sPz;
        
        float theta = (random16(0, 6283) / 1000.0f); 
        float v = (random16(0, 2000) / 1000.0f) - 1.0f; 
        float phi = acos(v);
        float burstSpeed = random16(300, 600) / 10.0f; 
        
        particles[i].vx = burstSpeed * sin(phi) * cos(theta);
        particles[i].vy = burstSpeed * sin(phi) * sin(theta);
        particles[i].vz = burstSpeed * cos(phi);
        
        particles[i].life = 1.0f;
        particles[i].color = (random8(2) == 0) ? CRGB::Green : CRGB::Red; 
    }

    // 2. Let the explosion play out
    uint32_t deathTimer = millis();
    while(millis() - deathTimer < 3000) {
        uint32_t now = millis();
        float dt = (now - lastFrameTime) / 1000.0f;
        lastFrameTime = now;
        if (dt > 0.05f) dt = 0.05f;

        clearAll();
        
        // Render Particles
        for(int i=0; i<MAX_PARTICLES; i++) {
            if (particles[i].active) {
                particles[i].px += particles[i].vx * dt;
                particles[i].py += particles[i].vy * dt;
                particles[i].pz += particles[i].vz * dt;
                particles[i].vx *= 0.90f; // High Drag
                particles[i].vy *= 0.90f;
                particles[i].vz *= 0.90f;
                particles[i].life -= 0.6f * dt;
                
                if (particles[i].life <= 0) {
                    particles[i].active = false;
                    continue;
                }
                
                int ix = (int)round(particles[i].px);
                int iy = (int)round(particles[i].py);
                int iz = (int)round(particles[i].pz);
                
                if (ix>=0 && ix<RNDR_X && iy>=0 && iy<RNDR_Y && iz>=0 && iz<RNDR_Z) {
                    CRGB c = particles[i].color;
                    c.nscale8((uint8_t)(particles[i].life * 255.0f));
                    setVoxel(ix, iy, iz, getVoxel(ix, iy, iz) + c);
                }
            }
        }
        
        showCube();
        delay(15);
    }
    
    resetGame();
}

void Asteroids3d::handleInput(GamepadState pad) {
    if (!pad.connected) return;
    uint32_t now = millis();
    float dt = 0.016f; // Assumption for input scaling

    // 1. Rotation (Pitch and Yaw via D-Pad / Left Stick)
    float turnSpeed = 3.5f;
    if (pad.dpad & DPAD_UP || pad.axisY < -200)    pitch -= turnSpeed * dt;
    if (pad.dpad & DPAD_DOWN || pad.axisY > 200)   pitch += turnSpeed * dt;
    if (pad.dpad & DPAD_LEFT || pad.axisX < -200)  yaw -= turnSpeed * dt;
    if (pad.dpad & DPAD_RIGHT || pad.axisX > 200)  yaw += turnSpeed * dt;

    // 2. Shield (Y Button)
    bool wasShieldActive = shieldActive;
    shieldActive = pad.y;
    
    if (shieldActive) {
        thrusting = false; // Engines cut while shielded
        if (!wasShieldActive) shieldStartTime = now;
        return; // No weapons or teleports while shielding
    }

    // 3. Forward Vector Math
    float fVx = cos(yaw) * cos(pitch);
    float fVy = sin(yaw) * cos(pitch);
    float fVz = sin(pitch);

    // 4. Thrust (B Button)
    thrusting = pad.b;
    if (thrusting) {
        sVx += fVx * THRUST_POWER * dt;
        sVy += fVy * THRUST_POWER * dt;
        sVz += fVz * THRUST_POWER * dt;
    }

    // 5. Fire (X Button or Bumpers)
    bool firePressed = pad.x || pad.l1 || pad.r1 || pad.l2 || pad.r2;
    if (firePressed && (now - lastFireTime > 150)) {
        for(int i=0; i<MAX_BULLETS; i++) {
            if (!bullets[i].active) {
                bullets[i].active = true;
                
                // Normalize by the largest axis to snap perfectly to a -1, 0, or 1 grid offset
                float max_abs = fmaxf(fabsf(fVx), fmaxf(fabsf(fVy), fabsf(fVz)));
                float gunX = round(fVx / max_abs);
                float gunY = round(fVy / max_abs);
                float gunZ = round(fVz / max_abs);

                // Spawn exactly at the red gun, pushed slightly forward
                bullets[i].px = sPx + gunX + (fVx * 0.5f); 
                bullets[i].py = sPy + gunY + (fVy * 0.5f);
                bullets[i].pz = sPz + gunZ + (fVz * 0.5f);
                
                bullets[i].vx = fVx * BULLET_SPEED;
                bullets[i].vy = fVy * BULLET_SPEED;
                bullets[i].vz = fVz * BULLET_SPEED;
                bullets[i].distanceTraveled = 0.0f;
                lastFireTime = now;
                break;
            }
        }
    }

    // 6. Teleport (A Button)
    static bool prevTeleport = false;
    bool telePressed = pad.a;
    if (telePressed && !prevTeleport) {
        float nx, ny, nz;
        do {
            nx = random(0, RNDR_X);
            ny = random(0, RNDR_Y);
            nz = random(0, RNDR_Z);
        } while(sqrt(pow(nx-sPx,2) + pow(ny-sPy,2) + pow(nz-sPz,2)) < (AST_SAFE_ZONE * 0.8f));
        sPx = nx; sPy = ny; sPz = nz;
    }
    prevTeleport = telePressed;
}

void Asteroids3d::updatePhysics(float dt) {
    // 1. Ship Newtonian Physics
    sPx += sVx * dt;
    sPy += sVy * dt;
    sPz += sVz * dt;
    
    // Apply continuous drag friction
    float dragFactor = pow(DRAG_COEFFICIENT, dt * 60.0f);
    sVx *= dragFactor; sVy *= dragFactor; sVz *= dragFactor;

    // Hard Pythagorean Velocity Cap
    float currentSpeed = sqrt(sVx*sVx + sVy*sVy + sVz*sVz);
    if (currentSpeed > MAX_SPEED) {
        float ratio = MAX_SPEED / currentSpeed;
        sVx *= ratio; sVy *= ratio; sVz *= ratio;
    }

    wrapBoundary(sPx, RNDR_X); wrapBoundary(sPy, RNDR_Y); wrapBoundary(sPz, RNDR_Z);

    // 2. Shield Overload Check
    if (shieldActive && (millis() - shieldStartTime > 2000)) {
        playGameOver();
        return;
    }

    // 3. Bullet Physics
    for(int i=0; i<MAX_BULLETS; i++) {
        if (bullets[i].active) {
            bullets[i].px += bullets[i].vx * dt;
            bullets[i].py += bullets[i].vy * dt;
            bullets[i].pz += bullets[i].vz * dt;
            bullets[i].distanceTraveled += BULLET_SPEED * dt;
            
            // Scaled burnout range
            if (bullets[i].distanceTraveled > AST_BULLET_RANGE) bullets[i].active = false;
            
            wrapBoundary(bullets[i].px, RNDR_X); 
            wrapBoundary(bullets[i].py, RNDR_Y); 
            wrapBoundary(bullets[i].pz, RNDR_Z);
        }
    }

    // 4. Asteroid Physics
    for(int i=0; i<MAX_ASTEROIDS; i++) {
        if (asteroids[i].active) {
            asteroids[i].px += asteroids[i].vx * dt;
            asteroids[i].py += asteroids[i].vy * dt;
            asteroids[i].pz += asteroids[i].vz * dt;
            
            // Visual Tumbling
            asteroids[i].rotX += 45.0f * dt;
            asteroids[i].rotY += 30.0f * dt;
            
            wrapBoundary(asteroids[i].px, RNDR_X); 
            wrapBoundary(asteroids[i].py, RNDR_Y); 
            wrapBoundary(asteroids[i].pz, RNDR_Z);
        }
    }

    // 5. Particle Physics (Asteroid Shattering)
    for(int i=0; i<MAX_PARTICLES; i++) {
        if (particles[i].active) {
            particles[i].px += particles[i].vx * dt;
            particles[i].py += particles[i].vy * dt;
            particles[i].pz += particles[i].vz * dt;
            particles[i].vx *= 0.95f; 
            particles[i].vy *= 0.95f;
            particles[i].vz *= 0.95f;
            particles[i].life -= 1.2f * dt;
            if (particles[i].life <= 0) particles[i].active = false;
        }
    }

    // 6. Wave Progression Check
    bool anyRocks = false;
    for(int i=0; i<MAX_ASTEROIDS; i++) {
        if (asteroids[i].active) { anyRocks = true; break; }
    }
    
    if (!anyRocks && waveTimer == 0) {
        waveTimer = millis(); // Start the 1.5s delay timer
    }
    
    if (waveTimer > 0 && (millis() - waveTimer > 1500)) {
        currentWave++;
        waveTimer = 0;
        
        // Spawn the next wave
        for(int i=0; i<AST_SPAWN_COUNT; i++) {
            float nx, ny, nz;
            do {
                nx = random(0, RNDR_X);
                ny = random(0, RNDR_Y);
                nz = random(0, RNDR_Z);
            } while(sqrt(pow(nx-sPx,2) + pow(ny-sPy,2) + pow(nz-sPz,2)) < AST_SAFE_ZONE);
            spawnAsteroid(true, nx, ny, nz);
        }
    }
}

void Asteroids3d::checkCollisions() {
    // 1. Bullets vs Asteroids
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!bullets[b].active) continue;
        
        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!asteroids[a].active) continue;
            
            // Dynamically scaled hitboxes
            float hitRadius = asteroids[a].isLarge ? AST_LARGE_RAD : AST_SMALL_RAD;
            float distSq = pow(bullets[b].px - asteroids[a].px, 2) + 
                           pow(bullets[b].py - asteroids[a].py, 2) + 
                           pow(bullets[b].pz - asteroids[a].pz, 2);
                           
            if (distSq < hitRadius * hitRadius) {
                bullets[b].active = false;
                asteroids[a].active = false;
                score += asteroids[a].isLarge ? 10 : 25;
                
                // If it was large, break into two smaller ones
                // If it was large, break into two smaller ones AND play split animation
                if (asteroids[a].isLarge) {
                    spawnAsteroid(false, asteroids[a].px, asteroids[a].py, asteroids[a].pz);
                    spawnAsteroid(false, asteroids[a].px, asteroids[a].py, asteroids[a].pz);
                    
                    // The Split Animation: A burst of magma dust
                    for(int p=0; p<12; p++) {
                        for(int pi=0; pi<MAX_PARTICLES; pi++) {
                            if(!particles[pi].active) {
                                particles[pi].active = true;
                                particles[pi].px = asteroids[a].px; 
                                particles[pi].py = asteroids[a].py; 
                                particles[pi].pz = asteroids[a].pz;
                                
                                // True 3D spherical burst math
                                float theta = random16(0, 6283) / 1000.0f;
                                float phi = acos((random16(0, 2000) / 1000.0f) - 1.0f);
                                float burstSpeed = random16(10, 30) / 10.0f;
                                
                                particles[pi].vx = burstSpeed * sin(phi) * cos(theta);
                                particles[pi].vy = burstSpeed * sin(phi) * sin(theta);
                                particles[pi].vz = burstSpeed * cos(phi);
                                
                                particles[pi].life = 1.0f; 
                                particles[pi].color = (random8(2) == 0) ? CRGB::Yellow : CRGB::DarkOrange;
                                break;
                            }
                        }
                    }
                } else {
                    // Detonate small asteroid into Confetti
                    for(int p=0; p<15; p++) {
                        for(int pi=0; pi<MAX_PARTICLES; pi++) {
                            if(!particles[pi].active) {
                                particles[pi].active = true;
                                particles[pi].px = asteroids[a].px; 
                                particles[pi].py = asteroids[a].py; 
                                particles[pi].pz = asteroids[a].pz;
                                particles[pi].vx = (random(0, 200)/10.0f - 10.0f);
                                particles[pi].vy = (random(0, 200)/10.0f - 10.0f);
                                particles[pi].vz = (random(0, 200)/10.0f - 10.0f);
                                particles[pi].life = 1.0f;
                                particles[pi].color = CRGB::Orange;
                                break;
                            }
                        }
                    }
                }
                break; // Bullet destroyed, stop checking this bullet
            }
        }
    }

    // 2. Ship vs Asteroids
    if (!shieldActive) {
        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!asteroids[a].active) continue;
            
            // Dynamically scaled hitboxes
            float hitRadius = asteroids[a].isLarge ? AST_LARGE_RAD : AST_SMALL_RAD;
            float shipRadius = 1.2f; // Approximation of the 2x2x1 ship
            
            float distSq = pow(sPx - asteroids[a].px, 2) + 
                           pow(sPy - asteroids[a].py, 2) + 
                           pow(sPz - asteroids[a].pz, 2);
                           
            if (distSq < pow(hitRadius + shipRadius, 2)) {
                playGameOver();
                return;
            }
        }
    }
}

void Asteroids3d::drawFrame() {
    clearAll();
    
    // 1. Draw Asteroids (Textured Fiery Magma Rocks)
    static TextureState astTex(MODE_SOLID, CRGBPalette16(CRGB::Red), 1.0f);
    for(int i=0; i<MAX_ASTEROIDS; i++) {
        if (asteroids[i].active) {
            float rad = asteroids[i].isLarge ? AST_LARGE_RAD : AST_SMALL_RAD;
            GeometryEngine::drawSphere(asteroids[i].px, asteroids[i].py, asteroids[i].pz, 
                                       rad, RENDER_SOLID, astTex, 
                                       asteroids[i].rotX, asteroids[i].rotY, asteroids[i].rotZ, false);
        }
    }

    // 2. Draw Bullets
    for(int i=0; i<MAX_BULLETS; i++) {
        if (bullets[i].active) {
            int ix = (int)round(bullets[i].px);
            int iy = (int)round(bullets[i].py);
            int iz = (int)round(bullets[i].pz);
            if (ix>=0 && ix<RNDR_X && iy>=0 && iy<RNDR_Y && iz>=0 && iz<RNDR_Z) {
                setVoxel(ix, iy, iz, CRGB::White);
            }
        }
    }

    // 3. Draw Particles
    for(int i=0; i<MAX_PARTICLES; i++) {
        if (particles[i].active) {
            int ix = (int)round(particles[i].px);
            int iy = (int)round(particles[i].py);
            int iz = (int)round(particles[i].pz);
            if (ix>=0 && ix<RNDR_X && iy>=0 && iy<RNDR_Y && iz>=0 && iz<RNDR_Z) {
                CRGB c = particles[i].color;
                c.nscale8((uint8_t)(particles[i].life * 255.0f));
                setVoxel(ix, iy, iz, getVoxel(ix, iy, iz) + c);
            }
        }
    }

// 4. Draw the Ship (3-Pixel Inline Design with Grid Snapping)
    CRGB hullColor = shieldActive ? CRGB::Blue : CRGB::Green;
    
    // Calculate the continuous forward vector
    float fVx = cos(yaw) * cos(pitch);
    float fVy = sin(yaw) * cos(pitch);
    float fVz = sin(pitch);

    // Normalize by the largest axis to snap perfectly to a -1, 0, or 1 grid offset
    float max_abs = fmaxf(fabsf(fVx), fmaxf(fabsf(fVy), fabsf(fVz)));
    float dx = round(fVx / max_abs);
    float dy = round(fVy / max_abs);
    float dz = round(fVz / max_abs);

    // Define the 3 absolute voxel positions
    float cx = round(sPx),      cy = round(sPy),      cz = round(sPz);      // Core
    float nx = cx + dx,         ny = cy + dy,         nz = cz + dz;         // Nose
    float tx = cx - dx,         ty = cy - dy,         tz = cz - dz;         // Tail

    wrapBoundary(cx, RNDR_X); wrapBoundary(cy, RNDR_Y); wrapBoundary(cz, RNDR_Z);
    wrapBoundary(nx, RNDR_X); wrapBoundary(ny, RNDR_Y); wrapBoundary(nz, RNDR_Z);
    wrapBoundary(tx, RNDR_X); wrapBoundary(ty, RNDR_Y); wrapBoundary(tz, RNDR_Z);

    setVoxel((int)cx, (int)cy, (int)cz, hullColor); // Core
    setVoxel((int)tx, (int)ty, (int)tz, hullColor); // Tail
    setVoxel((int)nx, (int)ny, (int)nz, CRGB::Red); // Gun

    // 5. Draw the Exhaust Trail (Original Stutter, NO Particles)
    if (thrusting && random8(10) > 3) { 
        // Just step one voxel further backward from the tail
        float ex = cx - (dx * 2.0f);
        float ey = cy - (dy * 2.0f);
        float ez = cz - (dz * 2.0f);
        
        wrapBoundary(ex, RNDR_X); 
        wrapBoundary(ey, RNDR_Y); 
        wrapBoundary(ez, RNDR_Z);
        
        setVoxel((int)ex, (int)ey, (int)ez, CRGB::OrangeRed);
    }
}

// =========================================================================
//                          OS INTEGRATION
// =========================================================================

void Asteroids3d::run() {
    Serial.println("Starting Asteroids 3D...");
    resetGame();
    lastFrameTime = millis();
    
    while (true) {
        uint32_t now = millis();
        float dt = (now - lastFrameTime) / 1000.0f;
        
        if (dt >= 0.016f) { // ~60 FPS lock
            lastFrameTime = now;
            if (dt > 0.05f) dt = 0.05f; // Prevent tunneling

            GamepadState p1 = BluetoothManager::getState(ROLE_MASTER);
            GamepadState p2 = BluetoothManager::getState(ROLE_PLAYER_2);
            GamepadState activePad = p1.connected ? p1 : p2; 
            
            if (activePad.connected && activePad.miscButtons) {
                break; // Escape to OS Menu
            }

            handleInput(activePad);
            updatePhysics(dt);
            checkCollisions();
            drawFrame();
            
            showCube(); 
        } else {
            yield();
        }
    }
}

void Asteroids3d::renderIcon(TextureState& tex) {
    // Miniaturized Diorama for universal scaling (Fits neatly in 8x8 space)
    
    // The Ship (3-Pixel Inline Design)
    setVoxel(1, 3, 2, CRGB::Green); // Tail
    setVoxel(2, 3, 2, CRGB::Green); // Core
    setVoxel(3, 3, 2, CRGB::Red);   // Gun
    
    // The Orange Exhaust (Original stutter style)
    if (millis() % 200 > 50) {
        setVoxel(0, 3, 2, CRGB::OrangeRed);
    }
    
    // The Bullet
    setVoxel(4, 3, 2, CRGB::White);
    
    // Two dynamically scaled Small Asteroids (Hovering top-right)
    GeometryEngine::drawSphere(5, 6, 4, AST_SMALL_RAD, RENDER_SOLID, tex, millis()/10.0f, millis()/15.0f, 0, false);
    GeometryEngine::drawSphere(6, 4, 6, AST_SMALL_RAD, RENDER_SOLID, tex, millis()/12.0f, millis()/10.0f, 0, false);
}