#include "Tetris2d.h"

// Standard 2D bounding-box matrices (4x4)
static const int8_t SHAPES_2D[7][4][4] = {
    {{0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}}, // I
    {{0,0,0,0}, {1,0,0,0}, {1,1,1,0}, {0,0,0,0}}, // J
    {{0,0,0,0}, {0,0,1,0}, {1,1,1,0}, {0,0,0,0}}, // L
    {{0,0,0,0}, {0,1,1,0}, {0,1,1,0}, {0,0,0,0}}, // O
    {{0,0,0,0}, {0,1,1,0}, {1,1,0,0}, {0,0,0,0}}, // S
    {{0,0,0,0}, {0,1,0,0}, {1,1,1,0}, {0,0,0,0}}, // T
    {{0,0,0,0}, {1,1,0,0}, {0,1,1,0}, {0,0,0,0}}  // Z
};

static const CRGB COLORS_2D[7] = {
    CRGB::Cyan, CRGB::Blue, CRGB::Orange, CRGB::Yellow, 
    CRGB::Green, CRGB::Purple, CRGB::Red
};

void Tetris2d::run() {
    Tetris2d game; // Instantiates cleanly on the stack
    game.play(); // Blocks here until game over / exit
}
void Tetris2d::renderIcon(TextureState& tex) {
    CRGB baseColor = CRGB(0, 0, 40); 
    for(int x = 0; x < RNDR_X; x++) {
        if (x % 2 == 0) {
            setVoxel(x, 0, 0, baseColor); 
            setVoxel(x, 7, 0, baseColor); 
            setVoxel(0, x, 0, baseColor); 
            setVoxel(7, x, 0, baseColor); 
        }
    }
    int zDrop = 8 + (beatsin8(30, 0, 2)); 
    auto drawFlatT = [&](int fX, int fY, bool isXAxis) {
        if (isXAxis) {
            setVoxel(fX - 1, fY, zDrop, CRGB::Purple); setVoxel(fX, fY, zDrop, CRGB::Purple);
            setVoxel(fX + 1, fY, zDrop, CRGB::Purple); setVoxel(fX, fY, zDrop - 1, CRGB::Purple);
        } else {
            setVoxel(fX, fY - 1, zDrop, CRGB::Purple); setVoxel(fX, fY, zDrop, CRGB::Purple);
            setVoxel(fX, fY + 1, zDrop, CRGB::Purple); setVoxel(fX, fY, zDrop - 1, CRGB::Purple);
        }
    };
    drawFlatT(3, 0, true);  
    drawFlatT(4, 7, true);  
    drawFlatT(0, 4, false); 
    drawFlatT(7, 3, false); 
}

// =========================================================================
//                  THE MULTI-SURFACE PROJECTION ENGINE
// =========================================================================
void Tetris2d::drawMirrored(int lx, int ly, CRGB color) {
    if(lx < 0 || lx >= 8 || ly < 0 || ly >= 16) return;
    
    #ifdef HARDWARE_BURNCUBE
        int physicalZ = ly; 
        // Physical Dig-Octa rig has a front obstruction: Rotate 90 degrees
        setVoxel(0, lx, physicalZ, color);
        setVoxel(7, 7 - lx, physicalZ, color);
    #else
        int physicalZ = ly; 
        #ifndef TARGET_2026_CUBOID
        if (physicalZ > 15) return;
        #endif
        // Standard unobstructed hardware mapping (Front and Back Faces)
        setVoxel(lx, 0, physicalZ, color);
        setVoxel(7 - lx, 7, physicalZ, color);
    #endif
}

void Tetris2d::drawBoard() {
    clearAll();
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 16; y++) {
            if (board[x][y]) {
                drawMirrored(x, y, board[x][y]);
            }
        }
    }
}

void Tetris2d::drawPiece() {
    // 1. Calculate Ghost Y
    int8_t ghostY = pieceY;
    while (ghostY > 0 && !checkCollision(pieceX, ghostY - 1, activeMatrix)) {
        ghostY--;
    }

    // 2. Draw Ghost and Real Piece
    CRGB ghostColor = pieceColor;
    ghostColor.nscale8(40); // 15% brightness

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (activeMatrix[r][c]) {
                // Draw Ghost
                drawMirrored(pieceX + c, ghostY - r, ghostColor);
                // Draw Real
                drawMirrored(pieceX + c, pieceY - r, pieceColor);
            }
        }
    }
}

// =========================================================================
//                          GAME LOGIC
// =========================================================================
void Tetris2d::shuffleBag() {
    for (int i = 0; i < 7; i++) bag[i] = i;
    for (int i = 6; i > 0; i--) {
        int j = random8(i + 1);
        int temp = bag[i]; bag[i] = bag[j]; bag[j] = temp;
    }
    bagIndex = 0;
}

void Tetris2d::spawnPiece() {
    if (bagIndex >= 7) shuffleBag();
    currentShapeId = bag[bagIndex++];
    pieceColor = COLORS_2D[currentShapeId];

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            activeMatrix[r][c] = SHAPES_2D[currentShapeId][r][c];
        }
    }
    
    pieceX = 2; // Roughly centered in an 8-wide grid
    pieceY = 16; 
    
    if (checkCollision(pieceX, pieceY, activeMatrix)) {
        animateGameOver();
    }
}

bool Tetris2d::checkCollision(int8_t testX, int8_t testY, int8_t matrix[4][4]) {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (matrix[r][c]) {
                int px = testX + c;
                int py = testY - r;
                
                if (px < 0 || px >= 8 || py < 0) return true; // Walls & Floor
                if (py < 16 && board[px][py]) return true;    // Locked blocks
            }
        }
    }
    return false;
}

bool Tetris2d::tryMove(int8_t dx, int8_t dy) {
    if (!checkCollision(pieceX + dx, pieceY + dy, activeMatrix)) {
        pieceX += dx;
        pieceY += dy;
        return true;
    }
    return false;
}

void Tetris2d::rotateMatrix(bool clockwise) {
    if (currentShapeId == 3) return; // Don't rotate O-block

    int8_t temp[4][4];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (clockwise) temp[c][3 - r] = activeMatrix[r][c];
            else temp[3 - c][r] = activeMatrix[r][c];
        }
    }

    if (!checkCollision(pieceX, pieceY, temp)) {
        for (int r=0; r<4; r++) for (int c=0; c<4; c++) activeMatrix[r][c] = temp[r][c];
        return;
    }
    
    // Basic Wall Kicks
    if (!checkCollision(pieceX + 1, pieceY, temp)) { pieceX++; for (int r=0; r<4; r++) for (int c=0; c<4; c++) activeMatrix[r][c] = temp[r][c]; return; }
    if (!checkCollision(pieceX - 1, pieceY, temp)) { pieceX--; for (int r=0; r<4; r++) for (int c=0; c<4; c++) activeMatrix[r][c] = temp[r][c]; return; }
}

void Tetris2d::lockPiece() {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (activeMatrix[r][c]) {
                int py = pieceY - r;
                if (py >= 0 && py < 16) board[pieceX + c][py] = pieceColor;
            }
        }
    }
    checkLines();
    spawnPiece();
}

void Tetris2d::checkLines() {
    for (int y = 0; y < 16; y++) {
        bool full = true;
        for (int x = 0; x < 8; x++) {
            if (!board[x][y]) { full = false; break; }
        }
        
        if (full) {
            animateClearLine(y);
            // Shift everything down
            for (int shiftY = y; shiftY < 15; shiftY++) {
                for (int x = 0; x < 8; x++) {
                    board[x][shiftY] = board[x][shiftY + 1];
                }
            }
            // Clear top line
            for (int x = 0; x < 8; x++) board[x][15] = CRGB::Black;
            y--; // Re-check this height
        }
    }
}

void Tetris2d::animateClearLine(uint8_t line) {
    for(int flash = 0; flash < 3; flash++) {
        for(int x = 0; x < 8; x++) drawMirrored(x, line, CRGB::White);
        showCube(); delay(50);
        for(int x = 0; x < 8; x++) drawMirrored(x, line, CRGB::Black);
        showCube(); delay(50);
    }
}

void Tetris2d::animateGameOver() {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 8; x++) {
            if (board[x][y]) drawMirrored(x, y, CRGB::Red);
        }
        showCube(); delay(20);
    }
    memset(board, 0, sizeof(board)); // Erase board
    bagIndex = 7; // Force re-shuffle
}

void Tetris2d::handleInput(GamepadState pad) {
    unsigned long now = millis();

    // Movement (D-Pad or Stick)
    if (now - lastMoveTime > 150) {
        int8_t dx = 0;
        
        bool rightPressed = pad.dpad & DPAD_RIGHT || pad.axisX > 200;
        bool leftPressed  = pad.dpad & DPAD_LEFT || pad.axisX < -200;

#ifdef HARDWARE_BURNCUBE
        if (rightPressed) dx = 1;
        else if (leftPressed) dx = -1;
#else
        // Maintain the original mirrored math for the Desktop cube
        if (rightPressed) dx = -1;
        else if (leftPressed) dx = 1;
#endif

        if (dx != 0) {
            tryMove(dx, 0);
            lastMoveTime = now;
        }
    }

    // Rotation (A = CW, B/X = CCW)
    if (now - lastInputTime > 200) {
        if (pad.a || pad.y) { rotateMatrix(true); lastInputTime = now; }
        else if (pad.b || pad.x) { rotateMatrix(false); lastInputTime = now; }
    }
    
    // Hard Drop (Up on D-Pad)
    if ((pad.dpad & DPAD_UP || pad.axisY < -200) && (now - lastInputTime > 300)) {
        while (tryMove(0, -1)) {}
        lastDropTime = 0; // Force immediate lock
        lastInputTime = now;
    }
}

void Tetris2d::play() {
    Serial.println("Starting 2D Mirrored Tetris...");
    memset(board, 0, sizeof(board));
    bagIndex = 7;
    spawnPiece();
    
    lastMoveTime = millis();
    lastDropTime = millis();
    lastInputTime = millis();
    
    while (true) {
        GamepadState p1 = BluetoothManager::getState(ROLE_MASTER);
        GamepadState p2 = BluetoothManager::getState(ROLE_PLAYER_2);
        GamepadState activePad = p1.connected ? p1 : p2;

        if (activePad.connected) handleInput(activePad);

        // Soft Drop gravity override (D-Pad Down, Left Stick Down, or ANY Front Bumper/Trigger)
        unsigned long currentDropInterval = 600;
        if (activePad.connected && (activePad.dpad & DPAD_DOWN || activePad.axisY > 200 || 
                          activePad.l1 || activePad.l2 || activePad.r1 || activePad.r2)) {
            currentDropInterval = 50; 
        }

        // Gravity
        if (millis() - lastDropTime >= currentDropInterval) {
            if (!tryMove(0, -1)) {
                lockPiece();
            }
            lastDropTime = millis();
        }

        drawBoard();
        drawPiece();
        showCube(); // The hardware tripwire safely watches from in here
        delay(15);
    }
}