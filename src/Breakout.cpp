#include "Breakout.h"

// --- STATIC ALLOCATIONS ---
BreakoutMode Breakout::currentMode = MODE_CLASSIC;


// A pointer to a 2D array allows you to keep using bricks[x][y][z] normally!
#define bricks ((bool (*)[RNDR_Y][RNDR_Z])SharedAppMemory)
#define explosions ((Breakout::Explosion*)(SharedAppMemory + 4096))

int Breakout::bricksRemaining = 0;
bool Breakout::ballInPlay = false;

float Breakout::paddleX = 2.5f;
float Breakout::paddleY = 2.5f;

float Breakout::ballX = 0.0f;
float Breakout::ballY = 0.0f;
float Breakout::ballZ = 0.0f;
float Breakout::velX = 0.0f;
float Breakout::velY = 0.0f;
float Breakout::velZ = 0.0f;

uint8_t Breakout::explosionCount = 0;

const CRGB BRICK_COLORS[4] = {
    CRGB::Blue,        // Z = 12 (Bottom layer)
    CRGB::Green,       // Z = 13
    CRGB::Yellow,      // Z = 14
    CRGB::Red          // Z = 15 (Top layer)
};

// =========================================================================
//                          THE SUBMENU
// =========================================================================
bool Breakout::runSubMenu() {
    int menuIndex = 0;
    const int TOTAL_MODES = 2;
    uint32_t lastInputTime = millis();

    while (true) {
        GamepadState p1 = BluetoothManager::getState(ROLE_MASTER);
        GamepadState p2 = BluetoothManager::getState(ROLE_PLAYER_2);
        GamepadState activePad = p1.connected ? p1 : p2;

        if (activePad.connected && millis() - lastInputTime > 200) {
            
            if (activePad.dpad & (DPAD_RIGHT | DPAD_DOWN) || activePad.axisX > 200 || activePad.axisY > 200) {
                menuIndex = (menuIndex + 1) % TOTAL_MODES;
                lastInputTime = millis();
            } else if (activePad.dpad & (DPAD_LEFT | DPAD_UP) || activePad.axisX < -200 || activePad.axisY < -200) {
                menuIndex = (menuIndex - 1 + TOTAL_MODES) % TOTAL_MODES;
                lastInputTime = millis();
            }

            if (activePad.a || activePad.b || activePad.x || activePad.y) {
                currentMode = static_cast<BreakoutMode>(menuIndex);
                return true; 
            }
            if (activePad.miscButtons) return false; 
        }

        clearAll();
        
        // Preview: 3x3x1 Green Paddle
        for(int px = 0; px < 3; px++) {
            for(int py = 0; py < 3; py++) {
                setVoxel(2 + px, 2 + py, 0, CRGB::Green);
            }
        }

       auto drawSubBall = [&](int bx, int by, int bz, CRGB col) {
            for(int dx=0; dx<2; dx++) for(int dy=0; dy<2; dy++) for(int dz=0; dz<2; dz++) setVoxel(bx+dx, by+dy, bz+dz, col);
        };

        if (menuIndex == 0) { 
            // --- CLASSIC MODE DIORAMA ---
            for (int z = 12; z <= 15; z++) {
                CRGB col = BRICK_COLORS[z - 12];
                for (int x = 0; x < RNDR_X; x++) {
                    for (int y = 0; y < RNDR_Y; y++) {
                        setVoxel(x, y, z, col);
                    }
                }
            }
            drawSubBall(4, 4, 5, CRGB::White); 
        } 
        else if (menuIndex == 1) { 
            // --- BREAKTHRU (OVERDRIVE) DIORAMA ---
            for (int z = 12; z <= 15; z++) {
                CRGB col = BRICK_COLORS[z - 12];
                for (int x = 0; x < RNDR_X; x++) {
                    for (int y = 0; y < RNDR_Y; y++) {
                        float cx = 3.5f, cy = 3.5f;
                        float dSq = (x - cx) * (x - cx) + (y - cy) * (y - cy);
                        float threshold = 0.0f;
                        if (z == 12) threshold = 8.0f; 
                        else if (z == 13) threshold = 5.0f; 
                        else if (z == 14) threshold = 2.0f;
                        else if (z == 15) threshold = 0.5f;
                        float noise = ((x * 23 ^ y * 17 ^ z * 7) % 10) / 10.0f; 
                        if (dSq > threshold + (noise * 4.0f)) {
                            setVoxel(x, y, z, col);
                        }
                    }
                }
            }
            setVoxel(2, 2, 0, CRGB::Green); setVoxel(4, 2, 0, CRGB::Green);
            setVoxel(2, 4, 0, CRGB::Green); setVoxel(4, 4, 0, CRGB::Green);
            drawSubBall(3, 3, 9, CRGB::White);        
            setVoxel(3, 3, 3, CRGB(100, 100, 100)); setVoxel(3, 4, 3, CRGB(100, 100, 100));
            setVoxel(4, 3, 3, CRGB(100, 100, 100)); setVoxel(4, 4, 3, CRGB(100, 100, 100));
            setVoxel(2, 2, 1, CRGB(30, 30, 30)); setVoxel(2, 3, 1, CRGB(30, 30, 30));
            setVoxel(3, 2, 1, CRGB(30, 30, 30)); setVoxel(3, 3, 1, CRGB(30, 30, 30));
        }
        showCube(); 
        delay(15);
    }
}

// =========================================================================
//                          GAME ENGINE
// =========================================================================
void Breakout::resetLevel() {
    bricksRemaining = 0;
    explosionCount = 0;

    for(int x=0; x<RNDR_X; x++) {
        for(int y=0; y<RNDR_Y; y++) {
            for(int z=0; z<RNDR_Z; z++) {
                bricks[x][y][z] = false;
            }
        }
    }
    
    for(int z = 12; z <= 15; z++) {
        for(int x = 0; x < RNDR_X; x++) {
            for(int y = 0; y < RNDR_Y; y++) {
                bricks[x][y][z] = true;
                bricksRemaining++;
            }
        }
    }
    resetBall();
}

void Breakout::resetBall() {
    ballInPlay = false;
    ballX = paddleX + 0.5f; // Centers the 2x2 ball directly over the 3x3 paddle
    ballY = paddleY + 0.5f;
    ballZ = 1.0f; // Rests exactly on top of the 1-high paddle (Z=0)
    velX = 0.0f;
    velY = 0.0f;
    velZ = 0.0f;
}

void Breakout::handleInput(GamepadState pad) {
    if (!pad.connected) return;

    float moveSpeed = 0.4f;
    float ax = -(pad.axisX / 512.0f);
    float ay = -(pad.axisY / 512.0f);

    if (abs(ax) > 0.2f) paddleX += ax * moveSpeed;
    if (abs(ay) > 0.2f) paddleY += ay * moveSpeed;

    if (paddleX < 0.0f) paddleX = 0.0f;
    if (paddleX > RNDR_X - 3.0f) paddleX = RNDR_X - 3.0f;
    if (paddleY < 0.0f) paddleY = 0.0f;
    if (paddleY > RNDR_Y - 3.0f) paddleY = RNDR_Y - 3.0f;

    if (ballInPlay && currentMode == MODE_OVERDRIVE) {
        if (velZ > 0.0f) {
            if (abs(ax) > 0.2f) velX += ax * 0.08f;
            if (abs(ay) > 0.2f) velY += ay * 0.08f;
            
            if (velX > 0.4f) velX = 0.4f; if (velX < -0.4f) velX = -0.4f;
            if (velY > 0.4f) velY = 0.4f; if (velY < -0.4f) velY = -0.4f;
        } 
    }

    bool launchPressed = pad.a || pad.b || pad.x || pad.y;
    if (!ballInPlay && launchPressed) {
        ballInPlay = true;
        if (currentMode == MODE_OVERDRIVE) {
            velZ = 0.26f; 
            velX = 0.0f; 
            velY = 0.0f;
        } else {
            velZ = 0.20f; 
            velX = (random(0, 100) / 1000.0f) - 0.05f; 
            velY = (random(0, 100) / 1000.0f) - 0.05f;
        }
    }
}

void Breakout::updatePhysics() {
    if (!ballInPlay) {
        ballX = paddleX + 0.5f;
        ballY = paddleY + 0.5f;
        return;
    }

    int prevX = (int)ballX;
    int prevY = (int)ballY;
    int prevZ = (int)ballZ;

    ballX += velX;
    ballY += velY;
    ballZ += velZ;

    // --- WALL COLLISIONS ---
    bool hitX = false, hitY = false, hitZ = false;

    if (ballX < 0.0f) { ballX = 0.0f; velX = -velX; hitX = true; }
    if (ballX >= RNDR_X - 2.0f) { ballX = RNDR_X - 2.0f; velX = -velX; hitX = true; }
    
    if (ballY < 0.0f) { ballY = 0.0f; velY = -velY; hitY = true; }
    if (ballY >= RNDR_Y - 2.0f) { ballY = RNDR_Y - 2.0f; velY = -velY; hitY = true; }
    
    if (ballZ >= RNDR_Z - 2.0f) { ballZ = RNDR_Z - 2.0f; velZ = -velZ; hitZ = true; }

    // THE FIX: Corner Trap Ejector
    // If steered into a top corner, blast it out with randomized horizontal momentum
    if (hitZ && (hitX || hitY)) {
        float randX = random(15, 35) / 100.0f;
        float randY = random(15, 35) / 100.0f;

        if (ballX <= 0.0f) velX = randX;
        else if (ballX >= RNDR_X - 2.0f) velX = -randX;
        else velX = (random(0, 2) == 0 ? randX : -randX);

        if (ballY <= 0.0f) velY = randY;
        else if (ballY >= RNDR_Y - 2.0f) velY = -randY;
        else velY = (random(0, 2) == 0 ? randY : -randY);
    }

   // --- PADDLE COLLISION (Paddle is at Z = 0) ---
    if (ballZ < 1.0f && velZ < 0.0f) {
        // THE FIX: Tightened the hitbox so the ball must visibly overlap the paddle.
        // No more magical edge-case bounces when the paddle "really isn't there".
        if (ballX + 1.5f >= paddleX && ballX + 0.5f <= paddleX + 3.0f &&
            ballY + 1.5f >= paddleY && ballY + 0.5f <= paddleY + 3.0f) {
            
            ballZ = 1.0f;
            velZ = -velZ; 

            // THE FIX: Apply hit-offset momentum AND randomness to BOTH modes!
            float hitOffsetX = (ballX + 1.0f) - (paddleX + 1.5f); 
            float hitOffsetY = (ballY + 1.0f) - (paddleY + 1.5f); 
            
            velX += hitOffsetX * 0.08f + ((random(0, 100) / 1000.0f) - 0.05f);
            velY += hitOffsetY * 0.08f + ((random(0, 100) / 1000.0f) - 0.05f);

            if (velX > 0.3f) velX = 0.3f; if (velX < -0.3f) velX = -0.3f;
            if (velY > 0.3f) velY = 0.3f; if (velY < -0.3f) velY = -0.3f;
        } 
    }
    
    // THE FIX: Instant Death. If it slips past the paddle, 
    // immediately kill the ball so it doesn't linger or magically bounce.
    if (ballZ < 0.2f) {
        resetBall();
        return;
    }

    // --- BRICK COLLISIONS ---
    bool hitBrick = false;
    for(int dx=0; dx<2; dx++) {
        for(int dy=0; dy<2; dy++) {
            for(int dz=0; dz<2; dz++) {
                int currX = (int)ballX + dx;
                int currY = (int)ballY + dy;
                int currZ = (int)ballZ + dz;

                if (currX >= 0 && currX < RNDR_X && currY >= 0 && currY < RNDR_Y && currZ >= 12 && currZ < RNDR_Z) {
                    if (bricks[currX][currY][currZ]) {
                        bricks[currX][currY][currZ] = false; 
                        bricksRemaining--;
                        hitBrick = true;

                        if (explosionCount < 64) {
                            explosions[explosionCount++] = {(int8_t)currX, (int8_t)currY, (int8_t)currZ, (uint32_t)millis()};
                        }
                    }
                }
            }
        }
    }

    if (hitBrick) {
        if (currentMode == MODE_CLASSIC) {
            if ((int)ballZ != prevZ) velZ = -velZ; 
            else if ((int)ballX != prevX) velX = -velX; 
            else if ((int)ballY != prevY) velY = -velY; 
            else velZ = -velZ;
        } else {
            // THE FIX: Breakthru Turbulence!
            // Smashing through a brick imparts a chaotic wobble to the flight path
            velX += ((random(0, 100) / 1000.0f) - 0.05f);
            velY += ((random(0, 100) / 1000.0f) - 0.05f);
        }
        
        if (bricksRemaining <= 0) {
            playWinAnimation();
            resetLevel();
            return; 
        }
    }
       
}

// =========================================================================
//                          THE WIN REWARD
// =========================================================================
void Breakout::playWinAnimation() {
    struct Particle { 
        float x, y, z; 
        float vx, vy, vz; 
        CRGB color; 
    };
    
    const int NUM_PARTICLES = 150;
    Particle particles[NUM_PARTICLES];

    for (int i = 0; i < NUM_PARTICLES; i++) {
        particles[i].x = (random8() / 255.0f) * RNDR_X;
        particles[i].y = (random8() / 255.0f) * RNDR_Y;
        particles[i].z = 12.0f + (random8() / 255.0f) * 4.0f; 

        particles[i].vx = (random(0, 300) - 150) / 100.0f;
        particles[i].vy = (random(0, 300) - 150) / 100.0f;
        particles[i].vz = (random(0, 300) - 150) / 100.0f + 0.8f; 

        particles[i].color = ColorFromPalette(PartyColors_p, random8(), 255, NOBLEND);
    }

    clearAll();
    for(int x = 0; x < RNDR_X; x++) {
        for(int y = 0; y < RNDR_Y; y++) {
            for(int z = 12; z < 16; z++) {
                setVoxel(x, y, z, CRGB::White);
            }
        }
    }
    showCube();
    delay(300); 

    uint32_t start = millis();
    while (millis() - start < 4500) { 
        clearAll();
        bool anyAlive = false;

        for (int i = 0; i < NUM_PARTICLES; i++) {
            if (particles[i].z >= -1.0f) { 
                anyAlive = true;

                particles[i].vx *= 0.92f;
                particles[i].vy *= 0.92f;
                particles[i].vz *= 0.92f;
                particles[i].vz -= 0.06f; 

                particles[i].x += particles[i].vx;
                particles[i].y += particles[i].vy;
                particles[i].z += particles[i].vz;

                int ix = (int)roundf(particles[i].x);
                int iy = (int)roundf(particles[i].y);
                int iz = (int)roundf(particles[i].z);

                if (ix >= 0 && ix < RNDR_X && iy >= 0 && iy < RNDR_Y && iz >= 0 && iz < RNDR_Z) {
                    setVoxel(ix, iy, iz, particles[i].color);
                }
            }
        }
        showCube();
        delay(15);
        
        if (!anyAlive) break; 
    }
}

// =========================================================================
//                          RENDER LOOPS
// =========================================================================
void Breakout::drawFrame() {
    clearAll();

    // 1. Draw 4 Layers of Bricks (Z = 12 to 15)
    for(int z = 12; z <= 15; z++) {
        CRGB baseColor = BRICK_COLORS[z - 12];
        for(int x = 0; x < RNDR_X; x++) {
            for(int y = 0; y < RNDR_Y; y++) {
                if (bricks[x][y][z]) {
                    setVoxel(x, y, z, baseColor);
                }
            }
        }
    }

    // 2. Draw Block Explosions 
    for(int i = 0; i < explosionCount; ) {
        uint32_t age = millis() - explosions[i].startTime;
        
        if (age < 150) {
            setVoxel(explosions[i].x, explosions[i].y, explosions[i].z, CRGB::White);
            i++;
        } else if (age < 2000) { 
            uint8_t hue = (age / 4) % 256; 
            setVoxel(explosions[i].x, explosions[i].y, explosions[i].z, CHSV(hue, 255, 255));
            i++;
        } else {
            explosions[i] = explosions[explosionCount - 1];
            explosionCount--;
        }
    }

    // 3. Draw 3x3x1 Green Paddle 
    int pX = (int)paddleX;
    int pY = (int)paddleY;
    for(int px = 0; px < 3; px++) {
        for(int py = 0; py < 3; py++) {
            setVoxel(pX + px, pY + py, 0, CRGB::Green);
        }
    }

    // 4. Draw 2x2x2 White Ball
    for(int dx=0; dx<2; dx++) {
        for(int dy=0; dy<2; dy++) {
            for(int dz=0; dz<2; dz++) {
                setVoxel((int)ballX + dx, (int)ballY + dy, (int)ballZ + dz, CRGB::White);
            }
        }
    }
}

void Breakout::run() {
    if (!runSubMenu()) return; 

    resetLevel();
    uint32_t lastFrame = 0;
    const uint32_t FRAME_DELAY = 16; 

    while (true) {
        if (millis() - lastFrame >= FRAME_DELAY) {
            
            GamepadState p1 = BluetoothManager::getState(ROLE_MASTER);
            GamepadState p2 = BluetoothManager::getState(ROLE_PLAYER_2);
            GamepadState activePad = p1.connected ? p1 : p2; 
            
            if (activePad.connected && activePad.miscButtons) {
                break;
            }

            handleInput(activePad);
            updatePhysics();
            drawFrame();
            
            showCube(); 
            lastFrame = millis();
        }
    }
}

void Breakout::renderIcon(TextureState& tex) {
    for(int px = 0; px < 3; px++) {
        for(int py = 0; py < 3; py++) {
            setVoxel(2 + px, 2 + py, 0, CRGB::Green);
        }
    }
    
    for(int dx=0; dx<2; dx++) {
        for(int dy=0; dy<2; dy++) {
            for(int dz=0; dz<2; dz++) {
                setVoxel(3 + dx, 3 + dy, 4 + dz, CRGB::White);
            }
        }
    }

    for (int z = 12; z <= 15; z++) {
        CRGB layerColor = BRICK_COLORS[z - 12];
        for (int x = 0; x < RNDR_X; x++) {
            for (int y = 0; y < RNDR_Y; y++) {
                setVoxel(x, y, z, layerColor);
            }
        }
    }
}