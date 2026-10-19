#include "Tetris.h"

static const int8_t SHAPES[7][4][3] = {
    {{0, 1, 0}, {1, 1, 0}, {2, 1, 0}, {3, 1, 0}}, // I (ID: 0)
    {{0, 2, 0}, {0, 1, 0}, {1, 1, 0}, {2, 1, 0}}, // J (ID: 1)
    {{2, 2, 0}, {0, 1, 0}, {1, 1, 0}, {2, 1, 0}}, // L (ID: 2)
    {{1, 2, 0}, {2, 2, 0}, {1, 1, 0}, {2, 1, 0}}, // O (ID: 3)
    {{1, 2, 0}, {2, 2, 0}, {0, 1, 0}, {1, 1, 0}}, // S (ID: 4)
    {{1, 2, 0}, {0, 1, 0}, {1, 1, 0}, {2, 1, 0}}, // T (ID: 5)
    {{0, 2, 0}, {1, 2, 0}, {1, 1, 0}, {2, 1, 0}}  // Z (ID: 6)
};

static const CRGB SHAPE_COLORS[7] = {
    CRGB::Cyan, CRGB::Blue, CRGB::Orange, CRGB::Yellow, 
    CRGB::Green, CRGB::Purple, CRGB::Red
};

void Tetris::rotateBlocksX(int8_t blocks[4][3]) {
    for (int b = 0; b < 4; b++) {
        int8_t oldY = blocks[b][1];
        blocks[b][1] = 3 - blocks[b][2];
        blocks[b][2] = oldY;
    }
}

void Tetris::rotateBlocksY(int8_t blocks[4][3]) {
    for (int b = 0; b < 4; b++) {
        int8_t oldX = blocks[b][0];
        blocks[b][0] = 3 - blocks[b][2];
        blocks[b][2] = oldX;
    }
}

void Tetris::rotateBlocksZ(int8_t blocks[4][3]) {
    for (int b = 0; b < 4; b++) {
        int8_t oldX = blocks[b][0];
        blocks[b][0] = 3 - blocks[b][1];
        blocks[b][1] = oldX;
    }
}

// ==============================================================================
// 3D SUPER ROTATION SYSTEM (PIVOTS & WALL KICKS)
// ==============================================================================
bool Tetris::tryRotate(uint8_t axis, bool reverse) {
    // 1. Lock the O-Block 
    if (currentShapeId == 3) return true; 

    int8_t temp[4][3];
    int8_t extBackup[4][3];
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            temp[i][j] = activeBlocks[i][j];
            extBackup[i][j] = extrudedBlocks[i][j];
        }
    }
    
    int rotations = reverse ? 3 : 1;
    for (int r = 0; r < rotations; r++) {
        if (axis == 0) { rotateBlocksX(temp); rotateBlocksX(extrudedBlocks); }
        else if (axis == 1) { rotateBlocksY(temp); rotateBlocksY(extrudedBlocks); }
        else if (axis == 2) { rotateBlocksZ(temp); rotateBlocksZ(extrudedBlocks); }
    }

    // 2. Pivot Shift Compensation (Rotate in place)
    // We map the physical block index that acts as the "center" voxel for I, J, L, O, S, T, Z
    uint8_t pivotIndex[] = {2, 2, 2, 0, 3, 2, 2}; 
    uint8_t p = pivotIndex[currentShapeId];
    
    // Calculate how far the center block drifted during the matrix rotation
    int8_t shiftX = activeBlocks[p][0] - temp[p][0];
    int8_t shiftY = activeBlocks[p][1] - temp[p][1];
    int8_t shiftZ = activeBlocks[p][2] - temp[p][2];

    // Counteract the drift
    int8_t baseTestX = currentPiece.x + shiftX;
    int8_t baseTestY = currentPiece.y + shiftY;
    int8_t baseTestZ = currentPiece.z + shiftZ;

    // 3. The 3D Wall Kicks
    static const int8_t KICKS[11][3] = {
        {0, 0, 0},   // Attempt 1: Perfect rotation (No kick)
        {1, 0, 0}, {-1, 0, 0}, // Attempt 2-3: X-axis horizontal nudges
        {0, 1, 0}, {0, -1, 0}, // Attempt 4-5: Y-axis horizontal nudges
        {0, 0, 1},   // Attempt 6: Z-axis up (Floor kick)
        {1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0}, // Attempt 7-10: Diagonal nudges
        {0, 0, 2}    // Attempt 11: Hard double floor kick (Saves the I-block)
    };

    for (int k = 0; k < 11; k++) {
        int8_t kx = baseTestX + KICKS[k][0];
        int8_t ky = baseTestY + KICKS[k][1];
        int8_t kz = baseTestZ + KICKS[k][2];

        if (!checkCollision(kx, ky, kz, temp)) {
            // Success! Apply new position and lock in the rotation
            currentPiece.x = kx;
            currentPiece.y = ky;
            currentPiece.z = kz;
            
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 3; j++) {
                    activeBlocks[i][j] = temp[i][j];
                }
            }
            return true; // Exit early once a valid space is found
        }
    }
    
    // 4. Complete Failure
    // If we exhausted all 11 kick positions and they all collided, revert.
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            extrudedBlocks[i][j] = extBackup[i][j];
        }
    }
    return false;
}

void Tetris::shuffleBag() {
    for (int i = 0; i < 7; i++) bag[i] = i;
    for (int i = 6; i > 0; i--) {
        int j = random8(i + 1);
        int temp = bag[i];
        bag[i] = bag[j];
        bag[j] = temp;
    }
    bagIndex = 0;
}

uint8_t Tetris::getNextShape() {
    if (bagIndex >= 7) shuffleBag(); 
    return bag[bagIndex++];
}

// ==============================================================================
// 1. THE INTERACTIVE PLAYER LOOP
// ==============================================================================
void Tetris::play() {
    Serial.println("Starting Interactive 3D Tetris...");
    isInteractiveMode = true;
    clearAll();
    isActive = false;
    bagIndex = 7;
    lastInputTime = millis();
    
    while (true) {
        unsigned long now = millis();

        if (!isActive) {
            spawnPiece(false); 
            drawPiece();
            showCube();
        }

        GamepadState p1 = BluetoothManager::getState(ROLE_MASTER);
        GamepadState p2 = BluetoothManager::getState(ROLE_PLAYER_2);
        GamepadState activePad = p1.connected ? p1 : p2;

        erasePiece();

        if (activePad.connected) handleInput(activePad);

        unsigned long currentDropInterval = 1000;
        if (activePad.connected && (activePad.l1 || activePad.l2 || activePad.r1 || activePad.r2)) {
            currentDropInterval = 50; 
        }

        if (now - lastDropTime >= currentDropInterval) {
            lastDropTime = now;
            if (!tryMove(0, 0, -1)) {
                if (lockAndCheck()) {
                    clearAll();
                    bagIndex = 7;
                    continue; 
                }
            }
        }

        drawPiece();
        showCube(); 
        delay(15);
    }
}

void Tetris::handleInput(GamepadState pad) {
    unsigned long now = millis();

    // 1. Translation (D-Pad or Left Stick)
    if (now - lastMoveTime > 150) {
        int8_t dx = 0, dy = 0;
        
        bool upPressed    = pad.dpad & DPAD_UP    || pad.axisY < -200;
        bool downPressed  = pad.dpad & DPAD_DOWN  || pad.axisY > 200;
        bool leftPressed  = pad.dpad & DPAD_LEFT  || pad.axisX < -200;
        bool rightPressed = pad.dpad & DPAD_RIGHT || pad.axisX > 200;

#ifdef HARDWARE_BURNCUBE
        // 90-degree CW hardware rotation map
        bool tempUp = upPressed;
        upPressed = leftPressed;
        leftPressed = downPressed;
        downPressed = rightPressed;
        rightPressed = tempUp;
#endif

        if (rightPressed) dx = -1;
        else if (leftPressed) dx = 1;
        
        if (upPressed) dy = 1;
        else if (downPressed) dy = -1;

        if (dx != 0 || dy != 0) {
            tryMove(dx, dy, 0);
            lastMoveTime = now;
        }
    }

    // 2. The 2-Axis Rotation Lock 
    if (now - lastInputTime > 200) {
        bool rotated = false;
        
        // Buttons stay fixed relative to the controller face
        if (pad.x)      rotated = tryRotate(0, true);  
        else if (pad.b) rotated = tryRotate(0, false); 
        else if (pad.y) rotated = tryRotate(2, true);  
        else if (pad.a) rotated = tryRotate(2, false); 

        if (rotated) lastInputTime = now;
    }
}
// ==============================================================================
// 2. THE AUTONOMOUS AI LOOP
// ==============================================================================
void Tetris::animate(uint32_t durationSeconds, SimSpeed speed) {
    isInteractiveMode = false;
    unsigned long startTime = millis();
    unsigned long durationMs = durationSeconds * 1000UL;

    currentSpeed = speed;
    switch (currentSpeed) {
        case SPEED_NORMAL: moveInterval = 60; dropInterval = 200; fidgetInterval = 200; break;
        case SPEED_FAST: moveInterval = 30;  dropInterval = 60;  fidgetInterval = 100; break;
        case SPEED_SUPERHUMAN: moveInterval = 15;  dropInterval = 15;  fidgetInterval = 30;  break;
    }

    clearAll();
    isActive = false;
    bagIndex = 7; 
    uint8_t snagCount = 0; 

    while (millis() - startTime < durationMs) {
        unsigned long now = millis();

        if (!isActive) {
            spawnPiece(true); 
            snagCount = 0;
            drawPiece();
            showCube();
        }

        erasePiece();

        bool lockedThisFrame = false;
        bool aligned = (currentPiece.x == targetPos.x && currentPiece.y == targetPos.y);

        if (currentPiece.z > targetZ + 5) {
            if (now - lastFidgetTime >= fidgetInterval) {
                lastFidgetTime = now;
                tryRotate(random8(3));
            }
        } else {
            for(int b = 0; b < 4; b++) {
                activeBlocks[b][0] = targetBlocks[b][0];
                activeBlocks[b][1] = targetBlocks[b][1];
                activeBlocks[b][2] = targetBlocks[b][2];
            }
        }

        if (!aligned && now - lastMoveTime >= moveInterval) {
            lastMoveTime = now;
            int8_t dx = 0; int8_t dy = 0;

            if (currentPiece.x < targetPos.x) dx = 1;
            else if (currentPiece.x > targetPos.x) dx = -1;

            if (currentPiece.y < targetPos.y) dy = 1;
            else if (currentPiece.y > targetPos.y) dy = -1;

            if (dx != 0 || dy != 0) {
                if (!tryMove(dx, dy, 0)) {
                    if (dx != 0) tryMove(dx, 0, 0);
                    if (dy != 0) tryMove(0, dy, 0);
                }
            }
        }

        if (aligned) {
            unsigned long activeDropInterval = dropInterval;
            if (currentPiece.z <= targetZ + 5) {
                activeDropInterval = (currentSpeed == SPEED_SUPERHUMAN) ? 10 : 25; 
            }

            if (now - lastDropTime >= activeDropInterval) {
                lastDropTime = now;
                if (!tryMove(0, 0, -1)) lockedThisFrame = true;
            }
        } else {
            if (now - lastDropTime >= dropInterval) {
                lastDropTime = now;
                
                if (!tryMove(0, 0, -1)) {
                    int8_t tx = 0, ty = 0;
                    if (currentPiece.x < targetPos.x) tx = 1;
                    else if (currentPiece.x > targetPos.x) tx = -1;
                    if (currentPiece.y < targetPos.y) ty = 1;
                    else if (currentPiece.y > targetPos.y) ty = -1;
                    
                    bool blockedX = (tx != 0) && checkCollision(currentPiece.x + tx, currentPiece.y, currentPiece.z, activeBlocks);
                    bool blockedY = (ty != 0) && checkCollision(currentPiece.x, currentPiece.y + ty, currentPiece.z, activeBlocks);
                    
                    bool totallyBlocked = true;
                    if (tx != 0 && !blockedX) totallyBlocked = false;
                    if (ty != 0 && !blockedY) totallyBlocked = false;
                    
                    if (totallyBlocked) {
                        snagCount++;
                        if (snagCount > 5) lockedThisFrame = true; 
                    } else {
                        snagCount = 0; 
                    }
                } else {
                    snagCount = 0;
                }
            }
        }

        drawPiece(); 
        showCube(); 

        if (lockedThisFrame) lockAndCheck();
        
        delay(2); 
    }
}

// ==============================================================================
// PIECE SPAWNING & DELLACHERIE AI 
// ==============================================================================
void Tetris::spawnPiece(bool useAI) {
    currentShapeId = getNextShape();
    pieceColor = SHAPE_COLORS[currentShapeId];

    for (int b = 0; b < 4; b++) {
        activeBlocks[b][0] = SHAPES[currentShapeId][b][0];
        activeBlocks[b][1] = SHAPES[currentShapeId][b][1];
        activeBlocks[b][2] = SHAPES[currentShapeId][b][2];
        
        extrudedBlocks[b][0] = SHAPES[currentShapeId][b][0];
        extrudedBlocks[b][1] = SHAPES[currentShapeId][b][1];
        extrudedBlocks[b][2] = SHAPES[currentShapeId][b][2] + 1;
    }
    
    if (useAI) {
        uint8_t startFace = random8(6);
        if (startFace == 1) { rotateBlocksX(activeBlocks); rotateBlocksX(extrudedBlocks); }
        else if (startFace == 2) { rotateBlocksX(activeBlocks); rotateBlocksX(activeBlocks); rotateBlocksX(extrudedBlocks); rotateBlocksX(extrudedBlocks); }
        else if (startFace == 3) { rotateBlocksX(activeBlocks); rotateBlocksX(activeBlocks); rotateBlocksX(activeBlocks); rotateBlocksX(extrudedBlocks); rotateBlocksX(extrudedBlocks); rotateBlocksX(extrudedBlocks); }
        else if (startFace == 4) { rotateBlocksY(activeBlocks); rotateBlocksY(extrudedBlocks); }
        else if (startFace == 5) { rotateBlocksY(activeBlocks); rotateBlocksY(activeBlocks); rotateBlocksY(activeBlocks); rotateBlocksY(extrudedBlocks); rotateBlocksY(extrudedBlocks); rotateBlocksY(extrudedBlocks); }

        uint8_t startRot = random8(4);
        for(int i = 0; i < startRot; i++) { 
            rotateBlocksZ(activeBlocks); 
            rotateBlocksZ(extrudedBlocks); 
        }
    }

    int min_x = 99, max_x = -99;
    int min_y = 99, max_y = -99;
    for (int b = 0; b < 4; b++) {
        if (activeBlocks[b][0] < min_x) min_x = activeBlocks[b][0];
        if (activeBlocks[b][0] > max_x) max_x = activeBlocks[b][0];
        if (activeBlocks[b][1] < min_y) min_y = activeBlocks[b][1];
        if (activeBlocks[b][1] > max_y) max_y = activeBlocks[b][1];
        
        if (isInteractiveMode) {
            if (extrudedBlocks[b][0] < min_x) min_x = extrudedBlocks[b][0];
            if (extrudedBlocks[b][0] > max_x) max_x = extrudedBlocks[b][0];
            if (extrudedBlocks[b][1] < min_y) min_y = extrudedBlocks[b][1];
            if (extrudedBlocks[b][1] > max_y) max_y = extrudedBlocks[b][1];
        }
    }
    
    #ifdef TARGET_2026_CUBOID
        int safe_start_x = -min_x;
        int safe_end_x = (RNDR_X - 1) - max_x;
        int safe_start_y = -min_y;
        int safe_end_y = (RNDR_Y - 1) - max_y;
        
        if (useAI) {
            currentPiece.x = random(safe_start_x, safe_end_x + 1);
            currentPiece.y = random(safe_start_y, safe_end_y + 1);
            currentPiece.z = RNDR_Z; 
        } else {
            currentPiece.x = safe_start_x + ((safe_end_x - safe_start_x) / 2);
            currentPiece.y = safe_start_y + ((safe_end_y - safe_start_y) / 2);
            currentPiece.z = RNDR_Z - 1; 
        }
    #else
        int phys_min_x = min_x * 2;
        int phys_max_x = (max_x * 2) + 1;
        int phys_min_y = min_y * 2;
        int phys_max_y = (max_y * 2) + 1;
        
        int safe_start_x = -phys_min_x;
        int safe_end_x = 15 - phys_max_x;
        int safe_start_y = -phys_min_y;
        int safe_end_y = 15 - phys_max_y;
        
        if (useAI) {
            currentPiece.x = random(safe_start_x / 2, (safe_end_x / 2) + 1) * 2;
            currentPiece.y = random(safe_start_y / 2, (safe_end_y / 2) + 1) * 2;
            currentPiece.z = 16; 
        } else {
            currentPiece.x = (safe_start_x / 2 + ((safe_end_x / 2) - (safe_start_x / 2)) / 2) * 2;
            currentPiece.y = (safe_start_y / 2 + ((safe_end_y / 2) - (safe_start_y / 2)) / 2) * 2;
            currentPiece.z = 14; 
        }
    #endif
    
    isActive = true;
    lastMoveTime = millis();
    lastDropTime = millis();
    lastFidgetTime = millis();
    lastGhostZ = -1;

    if (useAI) {
        findBestTarget();
    }
}

void Tetris::findBestTarget() {
    #ifdef TARGET_2026_CUBOID
        const int MAX_LOGICAL_Z = RNDR_Z; 
    #else
        const int MAX_LOGICAL_Z = 8;
    #endif

    int8_t logicalHeights[8][8] = {0};
    for (int lx = 0; lx < 8; lx++) {
        for (int ly = 0; ly < 8; ly++) {
            for (int lz = MAX_LOGICAL_Z - 1; lz >= 0; lz--) {
                bool occupied = false;
                
                #ifdef TARGET_2026_CUBOID
                    if (getVoxel(lx, ly, lz)) occupied = true;
                #else
                    for(int dx = 0; dx < 2; dx++) {
                        for(int dy = 0; dy < 2; dy++) {
                            for(int dz = 0; dz < 2; dz++) {
                                if (getVoxel(lx * 2 + dx, ly * 2 + dy, lz * 2 + dz)) {
                                    occupied = true; break;
                                }
                            }
                            if(occupied) break;
                        }
                        if(occupied) break;
                    }
                #endif

                if (occupied) {
                    logicalHeights[lx][ly] = lz + 1;
                    break;
                }
            }
        }
    }

    int8_t baseFill[16] = {0}; 
    for (int lx = 0; lx < 8; lx++) {
        for (int ly = 0; ly < 8; ly++) {
            for (int lz = 0; lz < logicalHeights[lx][ly]; lz++) {
                baseFill[lz]++;
            }
        }
    }

    long bestScore = 2147483647; 
    int8_t bestX = 3;
    int8_t bestY = 3;
    int8_t bestZ = MAX_LOGICAL_Z - 1;
    int8_t testBlocks[4][3];
    
    for (int face = 0; face < 6; face++) {
        for (int b = 0; b < 4; b++) {
            testBlocks[b][0] = SHAPES[currentShapeId][b][0];
            testBlocks[b][1] = SHAPES[currentShapeId][b][1];
            testBlocks[b][2] = SHAPES[currentShapeId][b][2];
        }

        if (face == 1) { rotateBlocksX(testBlocks); }
        else if (face == 2) { rotateBlocksX(testBlocks); rotateBlocksX(testBlocks); }
        else if (face == 3) { rotateBlocksX(testBlocks); rotateBlocksX(testBlocks); rotateBlocksX(testBlocks); }
        else if (face == 4) { rotateBlocksY(testBlocks); }
        else if (face == 5) { rotateBlocksY(testBlocks); rotateBlocksY(testBlocks); rotateBlocksY(testBlocks); }

        for (int rot = 0; rot < 4; rot++) {
            if (rot > 0) rotateBlocksZ(testBlocks);

            for (int8_t lx = -3; lx < 8; lx++) {
                for (int8_t ly = -3; ly < 8; ly++) {
                    
                    bool outOfBounds = false;
                    for (int b = 0; b < 4; b++) {
                        int px = lx + testBlocks[b][0];
                        int py = ly + testBlocks[b][1];
                        if (px < 0 || px > 7 || py < 0 || py > 7) {
                            outOfBounds = true; break;
                        }
                    }
                    if (outOfBounds) continue;

                    int8_t dropZ = 0;
                    for (int b = 0; b < 4; b++) {
                        int px = lx + testBlocks[b][0];
                        int py = ly + testBlocks[b][1];
                        int bz = testBlocks[b][2];
                        int8_t requiredZ = logicalHeights[px][py] - bz;
                        if (requiredZ > dropZ) dropZ = requiredZ;
                    }

                    if (dropZ > (MAX_LOGICAL_Z - 1)) continue; 

                    int holesCreated = 0;
                    int bumpiness = 0;
                    
                    int8_t testHeights[8][8];
                    for(int i = 0; i < 8; i++) for(int j = 0; j < 8; j++) testHeights[i][j] = logicalHeights[i][j];

                    for (int b = 0; b < 4; b++) {
                        int px = lx + testBlocks[b][0];
                        int py = ly + testBlocks[b][1];
                        int pz = dropZ + testBlocks[b][2];
                        
                        if (pz > testHeights[px][py]) {
                            holesCreated += (pz - testHeights[px][py]);
                        }
                        testHeights[px][py] = max((int)testHeights[px][py], pz + 1);
                    }

                    for (int i = 0; i < 8; i++) {
                        for (int j = 0; j < 8; j++) {
                            if (i < 7) bumpiness += abs(testHeights[i][j] - testHeights[i+1][j]);
                            if (j < 7) bumpiness += abs(testHeights[i][j] - testHeights[i][j+1]);
                        }
                    }

                    int8_t tempFill[16];
                    for (int z = 0; z < MAX_LOGICAL_Z; z++) tempFill[z] = baseFill[z];
                    for (int b = 0; b < 4; b++) {
                        int pz = dropZ + testBlocks[b][2];
                        if (pz < MAX_LOGICAL_Z) tempFill[pz]++;
                    }

                    int planesCompleted = 0;
                    for (int z = 0; z < MAX_LOGICAL_Z; z++) {
                        if (tempFill[z] >= 64) planesCompleted++;
                    }

                    long score = (dropZ * 10) + (holesCreated * 1000) + (bumpiness * 5) - (planesCompleted * 100000);
                    score += random8(4);

                    if (score < bestScore) {
                        bestScore = score;
                        bestX = lx;
                        bestY = ly;
                        bestZ = dropZ;
                        for (int b = 0; b < 4; b++) {
                            targetBlocks[b][0] = testBlocks[b][0];
                            targetBlocks[b][1] = testBlocks[b][1];
                            targetBlocks[b][2] = testBlocks[b][2];
                        }
                    }
                } 
            }
            delay(1); 
        } 
    } 
    
    #ifdef TARGET_2026_CUBOID
        targetPos.x = bestX;
        targetPos.y = bestY;
        targetZ = bestZ;
    #else
        targetPos.x = bestX * 2;
        targetPos.y = bestY * 2;
        targetZ = bestZ * 2;
    #endif
}

long Tetris::calculateScore(int8_t testX, int8_t testY, int8_t testZ, int8_t blocks[4][3]) { return 0; }

// ==============================================================================
// COLLISION, MOVEMENT & RENDERING 
// ==============================================================================
bool Tetris::checkCollision(int8_t testX, int8_t testY, int8_t testZ, int8_t blocks[4][3]) {
    for (int layer = 0; layer < (isInteractiveMode ? 2 : 1); layer++) {
        for (int b = 0; b < 4; b++) {
            int bx = (layer == 0) ? blocks[b][0] : extrudedBlocks[b][0];
            int by = (layer == 0) ? blocks[b][1] : extrudedBlocks[b][1];
            int bz = (layer == 0) ? blocks[b][2] : extrudedBlocks[b][2];

            #ifdef TARGET_2026_CUBOID
                int cx = testX + bx;
                int cy = testY + by;
                int cz = testZ + bz;
                
                if (cx < 0 || cx >= RNDR_X || cy < 0 || cy >= RNDR_Y || cz < 0) return true;
                if (cz < RNDR_Z && getVoxel(cx, cy, cz)) return true;
            #else
                int px = testX + (bx * 2);
                int py = testY + (by * 2);
                int pz = testZ + (bz * 2);
                
                for (int vx = 0; vx < 2; vx++) {
                    for (int vy = 0; vy < 2; vy++) {
                        for (int vz = 0; vz < 2; vz++) {
                            int cx = px + vx;
                            int cy = py + vy;
                            int cz = pz + vz;
                            if (cx < 0 || cx > 15 || cy < 0 || cy > 15 || cz < 0) return true;
                            if (cz <= 15 && getVoxel(cx, cy, cz)) return true;
                        }
                    }
                }
            #endif
        }
    }
    return false;
}

bool Tetris::tryMove(int8_t dx, int8_t dy, int8_t dz) {
    int8_t nx = currentPiece.x + dx;
    int8_t ny = currentPiece.y + dy;
    int8_t nz = currentPiece.z + dz;

    if (checkCollision(nx, ny, nz, activeBlocks)) return false;

    currentPiece.x = nx;
    currentPiece.y = ny;
    currentPiece.z = nz;
    return true;
}

int8_t Tetris::getGhostZ() {
    int8_t ghostZ = currentPiece.z;
    while (ghostZ > 0 && !checkCollision(currentPiece.x, currentPiece.y, ghostZ - 1, activeBlocks)) {
        ghostZ--;
    }
    return ghostZ;
}

void Tetris::drawPiece() {
    int8_t ghostZ = -1;
    CRGB ghostColor = CRGB::Black;
    
    if (isInteractiveMode) {
        ghostZ = getGhostZ();
        lastGhostZ = ghostZ;
        ghostColor = pieceColor;
        ghostColor.nscale8(40); 
    }

    for (int layer = 0; layer < (isInteractiveMode ? 2 : 1); layer++) {
        for (int b = 0; b < 4; b++) {
            int bx = (layer == 0) ? activeBlocks[b][0] : extrudedBlocks[b][0];
            int by = (layer == 0) ? activeBlocks[b][1] : extrudedBlocks[b][1];
            int bz = (layer == 0) ? activeBlocks[b][2] : extrudedBlocks[b][2];

            #ifdef TARGET_2026_CUBOID
                int finalX = currentPiece.x + bx;
                int finalY = currentPiece.y + by;
                int finalZReal = currentPiece.z + bz;
                
                if (finalX >= 0 && finalX < RNDR_X && finalY >= 0 && finalY < RNDR_Y) {
                    if (isInteractiveMode) {
                        int finalZGhost = ghostZ + bz;
                        if (finalZGhost >= 0 && finalZGhost < RNDR_Z) {
                            setVoxel(finalX, finalY, finalZGhost, ghostColor);
                        }
                    }
                    if (finalZReal >= 0 && finalZReal < RNDR_Z) {
                        setVoxel(finalX, finalY, finalZReal, pieceColor);
                    }
                }
            #else
                int px = currentPiece.x + (bx * 2);
                int py = currentPiece.y + (by * 2);
                int pzReal = currentPiece.z + (bz * 2);
                int pzGhost = isInteractiveMode ? ghostZ + (bz * 2) : -1;

                for (int dx = 0; dx < 2; dx++) {
                    for (int dy = 0; dy < 2; dy++) {
                        for (int dz = 0; dz < 2; dz++) {
                            int finalX = px + dx;
                            int finalY = py + dy;
                            
                            if (finalX >= 0 && finalX <= 15 && finalY >= 0 && finalY <= 15) {
                                if (isInteractiveMode && pzGhost != -1) {
                                    int finalZGhostScale = pzGhost + dz;
                                    if (finalZGhostScale >= 0 && finalZGhostScale <= 15) {
                                        setVoxel(finalX, finalY, finalZGhostScale, ghostColor);
                                    }
                                }
                                int finalZRealScale = pzReal + dz;
                                if (finalZRealScale >= 0 && finalZRealScale <= 15) {
                                    setVoxel(finalX, finalY, finalZRealScale, pieceColor);
                                }
                            }
                        }
                    }
                }
            #endif
        }
    }
}

void Tetris::erasePiece() {
    int8_t ghostZ = isInteractiveMode ? lastGhostZ : -1; 

    for (int layer = 0; layer < (isInteractiveMode ? 2 : 1); layer++) {
        for (int b = 0; b < 4; b++) {
            int bx = (layer == 0) ? activeBlocks[b][0] : extrudedBlocks[b][0];
            int by = (layer == 0) ? activeBlocks[b][1] : extrudedBlocks[b][1];
            int bz = (layer == 0) ? activeBlocks[b][2] : extrudedBlocks[b][2];

            #ifdef TARGET_2026_CUBOID
                int finalX = currentPiece.x + bx;
                int finalY = currentPiece.y + by;
                int finalZReal = currentPiece.z + bz;
                
                if (finalX >= 0 && finalX < RNDR_X && finalY >= 0 && finalY < RNDR_Y) {
                    if (finalZReal >= 0 && finalZReal < RNDR_Z) {
                        setVoxel(finalX, finalY, finalZReal, CRGB::Black);
                    }
                    if (isInteractiveMode && ghostZ != -1) {
                        int finalZGhost = ghostZ + bz;
                        if (finalZGhost >= 0 && finalZGhost < RNDR_Z) {
                            setVoxel(finalX, finalY, finalZGhost, CRGB::Black);
                        }
                    }
                }
            #else
                int px = currentPiece.x + (bx * 2);
                int py = currentPiece.y + (by * 2);
                int pzReal = currentPiece.z + (bz * 2);
                int pzGhost = isInteractiveMode ? ghostZ + (bz * 2) : -1;

                for (int dx = 0; dx < 2; dx++) {
                    for (int dy = 0; dy < 2; dy++) {
                        for (int dz = 0; dz < 2; dz++) {
                            int finalX = px + dx;
                            int finalY = py + dy;
                            
                            if (finalX >= 0 && finalX <= 15 && finalY >= 0 && finalY <= 15) {
                                int finalZRealScale = pzReal + dz;
                                if (finalZRealScale >= 0 && finalZRealScale <= 15) {
                                    setVoxel(finalX, finalY, finalZRealScale, CRGB::Black);
                                }
                                if (isInteractiveMode && pzGhost != -1) {
                                    int finalZGhostScale = pzGhost + dz;
                                    if (finalZGhostScale >= 0 && finalZGhostScale <= 15) {
                                        setVoxel(finalX, finalY, finalZGhostScale, CRGB::Black);
                                    }
                                }
                            }
                        }
                    }
                }
            #endif
        }
    }
}

void Tetris::lockPiece() { 
    // The voxels are intentionally left on the LED glass buffer!
}

bool Tetris::lockAndCheck() {
    int lowestZ = 99;
    for (int layer = 0; layer < (isInteractiveMode ? 2 : 1); layer++) {
        for (int b = 0; b < 4; b++) {
            int bz = (layer == 0) ? activeBlocks[b][2] : extrudedBlocks[b][2];
            #ifdef TARGET_2026_CUBOID
                int pz = currentPiece.z + bz;
            #else
                int pz = currentPiece.z + (bz * 2);
            #endif
            if (pz < lowestZ) lowestZ = pz;
        }
    }
    
    #ifdef TARGET_2026_CUBOID
    if (lowestZ >= RNDR_Z - 2) {
    #else
    if (lowestZ >= 13) {
    #endif
        animateGameOver();
        clearAll();
        isActive = false;
        return true; 
    } else {
        lockPiece(); 
        checkPlanes(); 
        isActive = false; 
        return false;
    }
}

// ==============================================================================
// PLANE CLEARING & GAME OVER
// ==============================================================================
void Tetris::checkPlanes() {
    #ifdef TARGET_2026_CUBOID
        for (int z = 0; z < RNDR_Z; z++) {
            bool full = true;
            for (int x = 0; x < RNDR_X; x++) {
                for (int y = 0; y < RNDR_Y; y++) {
                    if (!getVoxel(x, y, z)) { full = false; break; }
                }
                if (!full) break;
            }

            if (full) {
                animateClearPlane(z, z); 
                shiftPlanesDown(z, 1);
                z--; 
            }
        }
    #else
        for (int logicalZ = 0; logicalZ < 8; logicalZ++) {
            int physicalZ = logicalZ * 2;
            bool full = true;
            
            for (int x = 0; x < 16; x++) {
                for (int y = 0; y < 16; y++) {
                    if (!getVoxel(x, y, physicalZ)) { full = false; break; }
                }
                if (!full) break;
            }

            if (full) {
                animateClearPlane(physicalZ, physicalZ + 1);
                shiftPlanesDown(physicalZ, 2);
                logicalZ--; 
            }
        }
    #endif
}

void Tetris::animateClearPlane(uint8_t z1, uint8_t z2) {
    unsigned long start = millis();
    float cx = (RNDR_X - 1) / 2.0f;
    float cy = (RNDR_Y - 1) / 2.0f;
    
    unsigned long rippleDuration = 1000;
    unsigned long blinkDelay = 50;
    
    while (millis() - start < rippleDuration) {
        uint8_t baseHue = (millis() / 10); 
        for (int x = 0; x < RNDR_X; x++) {
            for (int y = 0; y < RNDR_Y; y++) {
                float dist = sqrt(pow(x - cx, 2) + pow(y - cy, 2));
                uint8_t hue = baseHue + (uint8_t)(dist * 15);
                setVoxel(x, y, z1, CHSV(hue, 255, 255));
                if (z1 != z2) setVoxel(x, y, z2, CHSV(hue, 255, 255));
            }
        }
        showCube();
        delay(20); 
    }

    for (int i = 0; i < 2; i++) {
        for (int x = 0; x < RNDR_X; x++) {
            for (int y = 0; y < RNDR_Y; y++) {
                setVoxel(x, y, z1, CRGB::Black);
                if (z1 != z2) setVoxel(x, y, z2, CRGB::Black);
            }
        }
        showCube();
        delay(blinkDelay);

        for (int x = 0; x < RNDR_X; x++) {
            for (int y = 0; y < RNDR_Y; y++) {
                setVoxel(x, y, z1, CRGB::White);
                if (z1 != z2) setVoxel(x, y, z2, CRGB::White);
            }
        }
        showCube();
        delay(blinkDelay);
    }
}

void Tetris::shiftPlanesDown(uint8_t startZ, uint8_t dropAmount) {
    for (int z = startZ + dropAmount; z < RNDR_Z; z++) {
        for (int x = 0; x < RNDR_X; x++) {
            for (int y = 0; y < RNDR_Y; y++) {
                CRGB valAbove = getVoxel(x, y, z);
                setVoxel(x, y, z - dropAmount, valAbove);
            }
        }
    }
    for (int z = RNDR_Z - dropAmount; z < RNDR_Z; z++) {
        for (int x = 0; x < RNDR_X; x++) {
            for (int y = 0; y < RNDR_Y; y++) {
                setVoxel(x, y, z, CRGB::Black);
            }
        }
    }
    showCube();
}

void Tetris::animateGameOver() {
    bool state[16][16][16] = {false};
    
    for(int z = 0; z < RNDR_Z; z++) {
        for(int y = 0; y < RNDR_Y; y++) {
            for(int x = 0; x < RNDR_X; x++) {
                if(getVoxel(x, y, z)) state[x][y][z] = true;
            }
        }
    }

    for (int i = 0; i < 3; i++) {
        for(int z = 0; z < RNDR_Z; z++)
            for(int y = 0; y < RNDR_Y; y++)
                for(int x = 0; x < RNDR_X; x++)
                    if(state[x][y][z]) setVoxel(x, y, z, CRGB::Red);
        showCube();
        delay(500);

        for(int z = 0; z < RNDR_Z; z++)
            for(int y = 0; y < RNDR_Y; y++)
                for(int x = 0; x < RNDR_X; x++)
                    if(state[x][y][z]) setVoxel(x, y, z, CRGB::Black);
        showCube();
        delay(500);
    }
}
void Tetris::run() {
    Tetris game; 
    game.play(); // System blocks here while the stack instance runs
}

void Tetris::renderIcon(TextureState& tex) {
    // 1. THE INTERIOR SHAPES (Suspended deep inside the volume)
    // Yellow O-Block floating in the lower-middle
    setVoxel(3, 3, 6, CRGB::Yellow); setVoxel(4, 3, 6, CRGB::Yellow);
    setVoxel(3, 4, 6, CRGB::Yellow); setVoxel(4, 4, 6, CRGB::Yellow);

    // Cyan I-Block falling vertically through the upper-middle
    setVoxel(3, 3, 8, CRGB::Cyan); setVoxel(3, 3, 9, CRGB::Cyan);
    setVoxel(3, 3, 10, CRGB::Cyan); setVoxel(3, 3, 11, CRGB::Cyan);

    // 2. THE SIDE FACES (Glued to the outer glass at Z = 2)
    // Front Face (Y = 0): Blue J-Block
    setVoxel(2, 0, 3, CRGB::Blue); setVoxel(2, 0, 2, CRGB::Blue); 
    setVoxel(3, 0, 2, CRGB::Blue); setVoxel(4, 0, 2, CRGB::Blue);
    
    // Back Face (Y = 7): Red Z-Block
    setVoxel(2, 7, 3, CRGB::Red); setVoxel(3, 7, 3, CRGB::Red); 
    setVoxel(3, 7, 2, CRGB::Red); setVoxel(4, 7, 2, CRGB::Red);

    // Left Face (X = 0): Orange L-Block
    setVoxel(0, 4, 3, CRGB::Orange); setVoxel(0, 4, 2, CRGB::Orange); 
    setVoxel(0, 3, 2, CRGB::Orange); setVoxel(0, 2, 2, CRGB::Orange);

    // Right Face (X = 7): Green S-Block
    setVoxel(7, 2, 2, CRGB::Green); setVoxel(7, 3, 2, CRGB::Green); 
    setVoxel(7, 3, 3, CRGB::Green); setVoxel(7, 4, 3, CRGB::Green);

    // 3. THE TOP FACE (Ceiling at Z = 15)
    // Purple T-Block pressed against the top glass
    setVoxel(2, 4, 15, CRGB::Purple); setVoxel(3, 4, 15, CRGB::Purple);
    setVoxel(4, 4, 15, CRGB::Purple); setVoxel(3, 3, 15, CRGB::Purple);

    // 4. Ambient floor glow (Z = 0) for depth context
    uint8_t beat = beatsin8(20, 10, 40);
    CRGB glow = CHSV(160, 255, beat); 
    for (int x = 1; x < 7; x++) {
        for (int y = 1; y < 7; y++) {
            setVoxel(x, y, 0, glow);
        }
    }
}