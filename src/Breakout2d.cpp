#include "Breakout2d.h"

Breakout2dMode Breakout2d::currentMode = MODE2D_CLASSIC;

#define bricks ((bool (*)[RNDR_Z])SharedAppMemory)
#define explosions ((Breakout2d::Explosion2D*)(SharedAppMemory + 1024))

int Breakout2d::bricksRemaining = 0;
bool Breakout2d::ballInPlay = false;

float Breakout2d::paddleX = 2.5f;
float Breakout2d::ballX = 0.0f;
float Breakout2d::ballZ = 0.0f;
float Breakout2d::velX = 0.0f;
float Breakout2d::velZ = 0.0f;

uint8_t Breakout2d::explosionCount = 0;

const CRGB BRICK_COLORS_2D[4] = {
    CRGB::Blue,        // Z = 12 
    CRGB::Green,       // Z = 13
    CRGB::Yellow,      // Z = 14
    CRGB::Red          // Z = 15 
};

// =========================================================================
//                          MIRRORED RENDERING
// =========================================================================
void Breakout2d::drawMirrored(int x, int z, CRGB col) {
    if (x >= 0 && x < RNDR_X && z >= 0 && z < RNDR_Z) {
#ifdef HARDWARE_BURNCUBE
        // Physical Dig-Octa rig has a front obstruction: Rotate 90 degrees
        setVoxel(0, x, z, col);          
        setVoxel(RNDR_X - 1, (RNDR_X - 1) - x, z, col); 
#else
        // Standard unobstructed hardware mapping (Front and Back Faces)
        setVoxel(x, 0, z, col);          
        setVoxel((RNDR_X - 1) - x, RNDR_Y - 1, z, col); 
#endif
    }
}

// =========================================================================
//                          THE SUBMENU
// =========================================================================
bool Breakout2d::runSubMenu() {
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
                currentMode = static_cast<Breakout2dMode>(menuIndex);
                return true; 
            }
            if (activePad.miscButtons) return false; 
        }

        clearAll();
        
        // Preview: 3-Wide Green Paddle
        for(int px = 0; px < 3; px++) drawMirrored(2 + px, 0, CRGB::Green);

       if (menuIndex == 0) { 
            // --- CLASSIC MODE DIORAMA (2D) ---
            for(int z = 12; z <= 15; z++) {
                CRGB col = BRICK_COLORS_2D[z - 12];
                for(int x = 1; x <= 6; x++) drawMirrored(x, z, col); 
            }
            drawMirrored(5, 5, CRGB::White); 
        } 
        else if (menuIndex == 1) { 
            // --- BREAKTHRU (OVERDRIVE) DIORAMA (2D) ---
            for(int z = 12; z <= 15; z++) {
                CRGB col = BRICK_COLORS_2D[z - 12];
                for(int x = 1; x <= 6; x++) {
                    bool inHole = (x == 3 || x == 4);
                    if (z <= 13 && (x >= 2 && x <= 5)) inHole = true; 
                    if (!inHole) drawMirrored(x, z, col); 
                }
            }
            drawMirrored(2, 0, CRGB::Green); 
            drawMirrored(4, 0, CRGB::Green);
            drawMirrored(3, 9, CRGB::White);
            drawMirrored(4, 3, CRGB(100, 100, 100)); // Fading Trail
            drawMirrored(3, 1, CRGB(30, 30, 30));    // Fading Trail
        }
        showCube(); 
        delay(15);
    }
}
// =========================================================================
//                          GAME ENGINE
// =========================================================================
void Breakout2d::resetLevel() {
    bricksRemaining = 0;
    explosionCount = 0;

    for(int x=0; x<RNDR_X; x++) {
        for(int z=0; z<RNDR_Z; z++) {
            bricks[x][z] = false;
        }
    }
    
    for(int z = 12; z <= 15; z++) {
        for(int x = 0; x < RNDR_X; x++) {
            bricks[x][z] = true;
            bricksRemaining++;
        }
    }
    resetBall();
}

void Breakout2d::resetBall() {
    ballInPlay = false;
    ballX = paddleX + 1.0f; // Center of the 3-wide pad
    ballZ = 1.0f;           // Rests on top of the pad
    velX = 0.0f;
    velZ = 0.0f;
}

void Breakout2d::handleInput(GamepadState pad) {
    if (!pad.connected) return;

    float moveSpeed = 0.4f;
    float ax = -(pad.axisX / 512.0f);
    
    bool rightPressed = pad.dpad & DPAD_RIGHT;
    bool leftPressed  = pad.dpad & DPAD_LEFT;

#ifdef HARDWARE_BURNCUBE
    // Flip the X-axis polarity for the Burn Cube's mirrored face
    ax = -ax; 
    bool temp = rightPressed;
    rightPressed = leftPressed;
    leftPressed = temp;
#endif

    if (abs(ax) > 0.2f) paddleX += ax * moveSpeed;
    if (rightPressed) paddleX -= moveSpeed; 
    if (leftPressed)  paddleX += moveSpeed; 

    if (paddleX < 0.0f) paddleX = 0.0f;
    if (paddleX > RNDR_X - 3.0f) paddleX = RNDR_X - 3.0f;

    if (ballInPlay && currentMode == MODE2D_OVERDRIVE) {
        if (velZ > 0.0f) {
            if (abs(ax) > 0.2f) velX += ax * 0.08f;
            if (rightPressed) velX -= 0.08f; 
            if (leftPressed) velX += 0.08f;  
            
            if (velX > 0.4f) velX = 0.4f; 
            if (velX < -0.4f) velX = -0.4f;
        } 
    }

    bool launchPressed = pad.a || pad.b || pad.x || pad.y;
    if (!ballInPlay && launchPressed) {
        ballInPlay = true;
        if (currentMode == MODE2D_OVERDRIVE) {
            velZ = 0.26f; 
            velX = 0.0f; 
        } else {
            velZ = 0.20f; 
            velX = (random(0, 100) / 1000.0f) - 0.05f; 
        }
    }
}

void Breakout2d::updatePhysics() {
    if (!ballInPlay) {
        ballX = paddleX + 1.0f;
        return;
    }

    int prevX = (int)ballX;
    int prevZ = (int)ballZ;

    ballX += velX;
    ballZ += velZ;

    // --- WALL COLLISIONS ---
    bool hitX = false, hitZ = false;

    // Re-enable bouncing to allow horizontal speed to carry over into the descent
    if (ballX < 0.0f) { ballX = 0.0f; velX = -velX; hitX = true; }
    if (ballX >= RNDR_X - 1.0f) { ballX = RNDR_X - 1.0f; velX = -velX; hitX = true; }
    if (ballZ >= RNDR_Z - 1.0f) { ballZ = RNDR_Z - 1.0f; velZ = -velZ; hitZ = true; }

    // THE FIX: Corner Trap Ejector
    if (hitZ && hitX) {
        float randX = random(15, 35) / 100.0f;
        if (ballX <= 0.0f) velX = randX;
        else velX = -randX;
    }

// --- PADDLE COLLISION ---
    if (ballZ < 1.0f && velZ < 0.0f) {
        // Tightened hit box: require visible overlap
        if (ballX + 0.5f >= paddleX && ballX + 0.5f <= paddleX + 3.0f) {
            
            ballZ = 1.0f;
            velZ = -velZ; 

            // Apply hit-offset momentum AND randomness to BOTH modes
            float hitOffsetX = (ballX + 0.5f) - (paddleX + 1.5f); 
            velX += hitOffsetX * 0.12f + ((random(0, 100) / 1000.0f) - 0.05f);

            if (velX > 0.3f) velX = 0.3f; if (velX < -0.3f) velX = -0.3f;
        } 
    }
    
    // Instant Death threshold
    if (ballZ < 0.2f) {
        resetBall(); 
        return;
    }

    // --- BRICK COLLISIONS ---
    int currX = (int)ballX;
    int currZ = (int)ballZ;

    if (currX >= 0 && currX < RNDR_X && currZ >= 12 && currZ < RNDR_Z) {
        if (bricks[currX][currZ]) {
            bricks[currX][currZ] = false; 
            bricksRemaining--;

            if (explosionCount < 64) {
                explosions[explosionCount++] = {(int8_t)currX, (int8_t)currZ, (uint32_t)millis()};
            }

            if (currentMode == MODE2D_CLASSIC) {
                if (prevZ != currZ) velZ = -velZ; 
                else if (prevX != currX) velX = -velX; 
                else velZ = -velZ;
            } else {
                // Breakthru Turbulence
                velX += ((random(0, 100) / 1000.0f) - 0.05f);
            }
            
            if (bricksRemaining <= 0) {
                playWinAnimation();
                resetLevel();
                return; 
            }
        }
    }
}

// =========================================================================
//                          THE WIN REWARD
// =========================================================================
void Breakout2d::playWinAnimation() {
    struct Particle2D { 
        float x, z; 
        float vx, vz; 
        CRGB color; 
    };
    
    const int NUM_PARTICLES = 60; // Reduced for 2D plane
    Particle2D particles[NUM_PARTICLES];

    for (int i = 0; i < NUM_PARTICLES; i++) {
        particles[i].x = (random8() / 255.0f) * RNDR_X;
        particles[i].z = 12.0f + (random8() / 255.0f) * 4.0f; 

        particles[i].vx = (random(0, 300) - 150) / 100.0f;
        particles[i].vz = (random(0, 300) - 150) / 100.0f + 0.8f; 

        particles[i].color = ColorFromPalette(PartyColors_p, random8(), 255, NOBLEND);
    }

    clearAll();
    for(int x = 0; x < RNDR_X; x++) {
        for(int z = 12; z < 16; z++) {
            drawMirrored(x, z, CRGB::White);
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
                particles[i].vz *= 0.92f;
                particles[i].vz -= 0.06f; 

                particles[i].x += particles[i].vx;
                particles[i].z += particles[i].vz;

                int ix = (int)roundf(particles[i].x);
                int iz = (int)roundf(particles[i].z);

                drawMirrored(ix, iz, particles[i].color);
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
void Breakout2d::drawFrame() {
    clearAll();

    // 1. Draw Bricks
    for(int z = 12; z <= 15; z++) {
        CRGB baseColor = BRICK_COLORS_2D[z - 12];
        for(int x = 0; x < RNDR_X; x++) {
            if (bricks[x][z]) {
                drawMirrored(x, z, baseColor);
            }
        }
    }

    // 2. Draw Explosions 
    for(int i = 0; i < explosionCount; ) {
        uint32_t age = millis() - explosions[i].startTime;
        
        if (age < 150) {
            drawMirrored(explosions[i].x, explosions[i].z, CRGB::White);
            i++;
        } else if (age < 2000) { 
            uint8_t hue = (age / 4) % 256; 
            drawMirrored(explosions[i].x, explosions[i].z, CHSV(hue, 255, 255));
            i++;
        } else {
            explosions[i] = explosions[explosionCount - 1];
            explosionCount--;
        }
    }

    // 3. Draw 3-Wide Green Paddle 
    int pX = (int)paddleX;
    for(int px = 0; px < 3; px++) {
        drawMirrored(pX + px, 0, CRGB::Green);
    }

    // 4. Draw 1x1 White Ball
    drawMirrored((int)ballX, (int)ballZ, CRGB::White);
}

void Breakout2d::run() {
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

void Breakout2d::renderIcon(TextureState& tex) {
    // 3-wide Green Paddle
    drawMirrored(2, 0, CRGB::Green);
    drawMirrored(3, 0, CRGB::Green);
    drawMirrored(4, 0, CRGB::Green);
    
    // White Ball
    drawMirrored(3, 4, CRGB::White);

    // Brick Stack
    for (int z = 12; z <= 15; z++) {
        CRGB layerColor = BRICK_COLORS_2D[z - 12];
        for (int x = 0; x < RNDR_X; x++) {
            drawMirrored(x, z, layerColor);
        }
    }
}