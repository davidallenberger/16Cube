#include "Snake2d.h"

CRGBPalette16 Snake2d::snakePalette;

// Static Memory Allocations
#define sx ((int8_t*)SharedAppMemory)
#define sz ((int8_t*)(SharedAppMemory + 128))
int Snake2d::snakeLen = 0;

int8_t Snake2d::dx = 0; int8_t Snake2d::dz = 0;
int8_t Snake2d::pDx = 0; int8_t Snake2d::pDz = 0;
int8_t Snake2d::ax = 0; int8_t Snake2d::az = 0;
unsigned long Snake2d::lastMoveTime = 0;
unsigned long Snake2d::moveInterval = 250;

// Mirrored drawing helper, projects X/Z plane onto faces
void Snake2d::drawMirrored(int x, int z, CRGB col) {
    if (x >= 0 && x < RNDR_X && z >= 0 && z < RNDR_Z) {
#ifdef HARDWARE_BURNCUBE
        // Physical Dig-Octa rig has a front obstruction: Rotate 90 degrees
        setVoxel(0, x, z, col);                             
        setVoxel(RNDR_X - 1, (RNDR_Y - 1) - x, z, col);     
#else
        // Standard unobstructed hardware mapping (Front and Back Faces)
        setVoxel(x, 0, z, col);                             
        setVoxel((RNDR_X - 1) - x, RNDR_Y - 1, z, col);     
#endif
    }
}

void Snake2d::spawnApple() {
    bool valid = false;
    while (!valid) {
        ax = random(0, RNDR_X);
        az = random(0, RNDR_Z);
        valid = true;
        for (int i = 0; i < snakeLen; i++) {
            if (sx[i] == ax && sz[i] == az) {
                valid = false;
                break;
            }
        }
    }
}

void Snake2d::resetGame() {
    snakeLen = 3;
    sx[0] = RNDR_X / 2; sz[0] = RNDR_Z / 2;
    sx[1] = sx[0] - 1;  sz[1] = sz[0];
    sx[2] = sx[0] - 2;  sz[2] = sz[0];
    
    dx = 1; dz = 0;
    pDx = 1; pDz = 0;
    
    moveInterval = 250; 
    spawnApple();

    // WILD MOUSE RANDOM PALETTE INJECTION
    if (random8(2) == 0) {
        snakePalette = CRGBPalette16(PaletteUtils::getRandomVibrantColor());
    } else {
        snakePalette = PaletteUtils::getRandomOrganicPalette();
    }
}

void Snake2d::playGameOver() {
    for (int i = 0; i < 3; i++) {
        clearAll();
        for (int j = 0; j < snakeLen; j++) {
            drawMirrored(sx[j], sz[j], CRGB::Red);
        }
        showCube();
        delay(200);
        clearAll();
        showCube();
        delay(200);
    }
    resetGame();
}

void Snake2d::handleInput(GamepadState pad) {
    if (!pad.connected) return;

    bool upPressed    = pad.dpad & DPAD_UP    || pad.axisY < -200;
    bool downPressed  = pad.dpad & DPAD_DOWN  || pad.axisY > 200;
    bool leftPressed  = pad.dpad & DPAD_LEFT  || pad.axisX < -200;
    bool rightPressed = pad.dpad & DPAD_RIGHT || pad.axisX > 200;

    // Up/Down manipulates the Z axis in our 2D grid
    if (upPressed) {
        if (dz == 0) { pDx = 0; pDz = 1; }
    } else if (downPressed) {
        if (dz == 0) { pDx = 0; pDz = -1; }
    } else if (leftPressed) {
        if (dx == 0) { pDx = -1; pDz = 0; } // True Left
    } else if (rightPressed) {
        if (dx == 0) { pDx = 1; pDz = 0; }  // True Right
    }
}

void Snake2d::updatePhysics() {
    dx = pDx; dz = pDz;

    int8_t nx = sx[0] + dx;
    int8_t nz = sz[0] + dz;

    // --- SEAMLESS WRAPPING (Toroidal Space) ---
    if (nx < 0) nx = RNDR_X - 1;
    else if (nx >= RNDR_X) nx = 0;
    
    if (nz < 0) nz = RNDR_Z - 1;
    else if (nz >= RNDR_Z) nz = 0;

    // Body check (The only way to die)
    for (int i = 0; i < snakeLen; i++) {
        if (sx[i] == nx && sz[i] == nz) {
            playGameOver();
            return;
        }
    }

    // Shift body
    for (int i = snakeLen - 1; i > 0; i--) {
        sx[i] = sx[i - 1];
        sz[i] = sz[i - 1];
    }
    sx[0] = nx;
    sz[0] = nz;

    // Check Apple
    if (nx == ax && nz == az) {
        if (snakeLen < MAX_LEN) snakeLen++;
        spawnApple();
        if (moveInterval > 70) moveInterval -= 5;
    }
}

void Snake2d::drawFrame() {
    clearAll();

    // Apple
    uint8_t hue = (millis() / 20) % 256; 
    drawMirrored(ax, az, CHSV(hue, 255, 255));

    // Snake
    for (int i = 0; i < snakeLen; i++) {
        if (i == 0) {
            drawMirrored(sx[i], sz[i], CRGB::White);
        } else {
            // Map the body segment along the chosen palette
            uint8_t colorIndex = (i * 255) / snakeLen;
            CRGB bodyColor = ColorFromPalette(snakePalette, colorIndex, 255, LINEARBLEND);
            
            // Subtly fade brightness towards the tail
            uint8_t brightness = map(i, 0, snakeLen, 255, 50);
            bodyColor.nscale8(brightness);
            
            drawMirrored(sx[i], sz[i], bodyColor);
        }
    }
}

void Snake2d::run() {
    Serial.println("Starting 2D Mirrored Snake...");
    resetGame();
    lastMoveTime = millis();
    uint32_t lastFrame = 0;
    
    while (true) {
        if (millis() - lastFrame >= 16) {
            GamepadState p1 = BluetoothManager::getState(ROLE_MASTER);
            GamepadState p2 = BluetoothManager::getState(ROLE_PLAYER_2);
            GamepadState activePad = p1.connected ? p1 : p2; 
            
            if (activePad.connected && activePad.miscButtons) {
                break;
            }

            handleInput(activePad);

            if (millis() - lastMoveTime >= moveInterval) {
                updatePhysics();
                lastMoveTime = millis();
            }

            drawFrame();
            showCube(); 
            lastFrame = millis();
        }
    }
}

void Snake2d::renderIcon(TextureState& tex) {
    // 21-Segment 2D Mirrored Snake: Long winding path with right angles
    const int NUM_PTS = 21;
    int8_t pts[NUM_PTS][2] = {
        {7,1}, {6,1}, {5,1}, {4,1}, {3,1}, {2,1},                       // Slithers Left
        {2,2}, {2,3}, {2,4}, {2,5}, {2,6}, {2,7}, {2,8}, {2,9}, {2,10}, // Climbs straight Up
        {3,10}, {4,10}, {5,10},                                         // Turns Right
        {5,9}, {5,8}, {5,7}                                             // Dives Down towards apple (Head)
    };
    
    for (int i = 0; i < NUM_PTS; i++) {
        if (i == NUM_PTS - 1) {
            drawMirrored(pts[i][0], pts[i][1], CRGB::White); // Head
        } else {
            uint8_t hue = (i * 255) / NUM_PTS;
            CRGB col = CHSV(hue, 255, 255);
            uint8_t brightness = map(i, 0, NUM_PTS, 30, 255);
            col.nscale8(brightness);
            drawMirrored(pts[i][0], pts[i][1], col);
        }
    }
    
    // The Apple
    uint8_t appleHue = (millis() / 20) % 256; 
    drawMirrored(5, 4, CHSV(appleHue, 255, 255));
}