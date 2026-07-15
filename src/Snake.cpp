#include "Snake.h"

CRGBPalette16 Snake::snakePalette;

// Static Memory Allocations
#define sx ((int8_t*)SharedAppMemory)
#define sy ((int8_t*)(SharedAppMemory + 256))
#define sz ((int8_t*)(SharedAppMemory + 512))
int Snake::snakeLen = 0;

int8_t Snake::dx = 0; int8_t Snake::dy = 0; int8_t Snake::dz = 0;
int8_t Snake::pDx = 0; int8_t Snake::pDy = 0; int8_t Snake::pDz = 0;
int8_t Snake::ax = 0; int8_t Snake::ay = 0; int8_t Snake::az = 0;
unsigned long Snake::lastMoveTime = 0;
unsigned long Snake::moveInterval = 250;

void Snake::spawnApple() {
    bool valid = false;
    while (!valid) {
        ax = random(0, RNDR_X);
        ay = random(0, RNDR_Y);
        az = random(0, RNDR_Z);
        valid = true;
        // Ensure the apple doesn't spawn inside the snake's body
        for (int i = 0; i < snakeLen; i++) {
            if (sx[i] == ax && sy[i] == ay && sz[i] == az) {
                valid = false;
                break;
            }
        }
    }
}

void Snake::resetGame() {
    snakeLen = 3;
    sx[0] = RNDR_X / 2; sy[0] = RNDR_Y / 2; sz[0] = RNDR_Z / 2;
    sx[1] = sx[0] - 1;  sy[1] = sy[0];      sz[1] = sz[0];
    sx[2] = sx[0] - 2;  sy[2] = sy[0];      sz[2] = sz[0];
    
    dx = 1; dy = 0; dz = 0;
    pDx = 1; pDy = 0; pDz = 0;
    
    moveInterval = 300; // Start at a comfortable speed
    spawnApple();

    // WILD MOUSE RANDOM PALETTE INJECTION
    // 50/50 chance of a solid vibrant color vs a flowing organic gradient
    if (random8(2) == 0) {
        snakePalette = CRGBPalette16(PaletteUtils::getRandomVibrantColor());
    } else {
        snakePalette = PaletteUtils::getRandomOrganicPalette();
    }
}

void Snake::playGameOver() {
    // Flash Red
    for (int i = 0; i < 3; i++) {
        clearAll();
        for (int j = 0; j < snakeLen; j++) {
            setVoxel(sx[j], sy[j], sz[j], CRGB::Red);
        }
        showCube();
        delay(200);
        clearAll();
        showCube();
        delay(200);
    }
    resetGame();
}

void Snake::handleInput(GamepadState pad) {
    if (!pad.connected) return;

    // X / Y Axis Navigation (D-Pad or Left Stick)
    if (pad.dpad & DPAD_UP || pad.axisY < -200) {
        if (dy == 0) { pDx = 0; pDy = 1; pDz = 0; }
    } else if (pad.dpad & DPAD_DOWN || pad.axisY > 200) {
        if (dy == 0) { pDx = 0; pDy = -1; pDz = 0; }
    } else if (pad.dpad & DPAD_LEFT || pad.axisX < -200) {
        if (dx == 0) { pDx = 1; pDy = 0; pDz = 0; } // FLIPPED X POLARITY
    } else if (pad.dpad & DPAD_RIGHT || pad.axisX > 200) {
        if (dx == 0) { pDx = -1; pDy = 0; pDz = 0; } // FLIPPED X POLARITY
    }
    
    // Z Axis Navigation (Y/X to raise, B/A to lower)
    if (pad.x || pad.y) {
        if (dz == 0) { pDx = 0; pDy = 0; pDz = 1; }
    } else if (pad.a || pad.b) {
        if (dz == 0) { pDx = 0; pDy = 0; pDz = -1; }
    }
}

void Snake::updatePhysics() {
    // Lock in the buffered direction
    dx = pDx; dy = pDy; dz = pDz;

    int8_t nx = sx[0] + dx;
    int8_t ny = sy[0] + dy;
    int8_t nz = sz[0] + dz;

    // --- SEAMLESS WRAPPING (Toroidal Space) ---
    if (nx < 0) nx = RNDR_X - 1;
    else if (nx >= RNDR_X) nx = 0;
    
    if (ny < 0) ny = RNDR_Y - 1;
    else if (ny >= RNDR_Y) ny = 0;
    
    if (nz < 0) nz = RNDR_Z - 1;
    else if (nz >= RNDR_Z) nz = 0;

    // Self Collision (The only way to die)
    for (int i = 0; i < snakeLen; i++) {
        if (sx[i] == nx && sy[i] == ny && sz[i] == nz) {
            playGameOver();
            return;
        }
    }

    // Shift body
    for (int i = snakeLen - 1; i > 0; i--) {
        sx[i] = sx[i - 1];
        sy[i] = sy[i - 1];
        sz[i] = sz[i - 1];
    }
    
    // Update head
    sx[0] = nx;
    sy[0] = ny;
    sz[0] = nz;

    // Check Apple
    if (nx == ax && ny == ay && nz == az) {
        if (snakeLen < MAX_LEN) snakeLen++;
        spawnApple();
        // Speed up slightly as you grow, cap at very fast
        if (moveInterval > 100) moveInterval -= 5; 
    }
}

void Snake::drawFrame() {
    clearAll();

    // Draw the Apple (Slow glowing rainbow single pixel)
    uint8_t hue = (millis() / 20) % 256; 
    setVoxel(ax, ay, az, CHSV(hue, 255, 255));

    // Draw the Snake
    for (int i = 0; i < snakeLen; i++) {
        if (i == 0) {
            setVoxel(sx[i], sy[i], sz[i], CRGB::White); // Head
        } else {
            // Map the body segment along the chosen palette
            uint8_t colorIndex = (i * 255) / snakeLen;
            CRGB bodyColor = ColorFromPalette(snakePalette, colorIndex, 255, LINEARBLEND);
            
            // Subtly fade brightness towards the tail
            uint8_t brightness = map(i, 0, snakeLen, 255, 50);
            bodyColor.nscale8(brightness);
            
            setVoxel(sx[i], sy[i], sz[i], bodyColor);
        }
    }
}

void Snake::run() {
    Serial.println("Starting 3D Volumetric Snake...");
    resetGame();
    lastMoveTime = millis();
    uint32_t lastFrame = 0;
    
    while (true) {
        if (millis() - lastFrame >= 16) {
            GamepadState p1 = BluetoothManager::getState(ROLE_MASTER);
            GamepadState p2 = BluetoothManager::getState(ROLE_PLAYER_2);
            GamepadState activePad = p1.connected ? p1 : p2; 
            
            if (activePad.connected && activePad.miscButtons) {
                break; // Exit to menu
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

void Snake::renderIcon(TextureState& tex) {
    // 22-Segment 3D Snake: Starts on the glass face, climbs, and dives into the volume
    const int NUM_PTS = 22;
    int8_t pts[NUM_PTS][3] = {
        {0,1,5}, {0,2,5}, {0,3,5}, {0,4,5}, {0,5,5}, {0,6,5}, // Flat against the outer glass (X=0)
        {0,6,6}, {0,6,7}, {0,6,8}, {0,6,9}, {0,6,10},         // Turns 90 deg and climbs UP the glass
        {1,6,10}, {2,6,10}, {3,6,10}, {4,6,10}, {5,6,10},     // Turns 90 deg and dives INTO the volume
        {5,5,10}, {5,4,10}, {5,3,10}, {5,2,10},               // Turns 90 deg across the center
        {5,2,9}, {5,2,8}                                      // Turns 90 deg down towards the apple (Head)
    };
    
    for (int i = 0; i < NUM_PTS; i++) {
        if (i == NUM_PTS - 1) {
            setVoxel(pts[i][0], pts[i][1], pts[i][2], CRGB::White); // Head
        } else {
            uint8_t hue = (i * 255) / NUM_PTS;
            CRGB col = CHSV(hue, 255, 255);
            uint8_t brightness = map(i, 0, NUM_PTS, 30, 255);
            col.nscale8(brightness);
            setVoxel(pts[i][0], pts[i][1], pts[i][2], col);
        }
    }
    
    // The Apple (Directly in the path of the head)
    uint8_t appleHue = (millis() / 20) % 256; 
    setVoxel(5, 2, 5, CHSV(appleHue, 255, 255));
}