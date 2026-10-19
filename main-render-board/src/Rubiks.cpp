#include "Rubiks.h"
#include "BluetoothManager.h"
#include "AppMemory.h"

#ifdef TARGET_2026_CUBOID
    #define logicalCube ((Rubiks::Cubie (*)[3][5])SharedAppMemory)
#else
    #define logicalCube ((Rubiks::Cubie (*)[3][3])SharedAppMemory)
#endif

// --- THE 2.0x OPTICAL SCALING LAW ---
#ifdef HARDWARE_BURNCUBE
const float RUBIKS_SCALE_MS = 2.0f;
#else
const float RUBIKS_SCALE_MS = 1.0f;
#endif

// --- LOCAL CONSTANTS ---
static const uint16_t SPEED_SCRAMBLE_90  = (uint16_t)(300 * RUBIKS_SCALE_MS);
static const uint16_t SPEED_SCRAMBLE_180 = (uint16_t)(450 * RUBIKS_SCALE_MS);
static const uint16_t SPEED_SOLVE_90     = (uint16_t)(200 * RUBIKS_SCALE_MS);
static const uint16_t SPEED_SOLVE_180    = (uint16_t)(300 * RUBIKS_SCALE_MS);
static const uint16_t PAUSE_SCRAMBLE     = (uint16_t)(50  * RUBIKS_SCALE_MS);
static const uint16_t PAUSE_SOLVE        = (uint16_t)(30  * RUBIKS_SCALE_MS);
static const uint16_t SPEED_BLINK        = (uint16_t)(150 * RUBIKS_SCALE_MS);

static const CRGB COLOR_U = CRGB::White;
static const CRGB COLOR_D = CRGB::Yellow;
static const CRGB COLOR_F = CRGB::Green;
static const CRGB COLOR_B = CRGB::Blue;
static const CRGB COLOR_L = CRGB(255, 64, 0); 
static const CRGB COLOR_R = CRGB::Red;
static const CRGB COLOR_BLACK = CRGB::Black;

// =========================================================================
//                  THE APP CONTRACT (ICON & LAUNCHER)
// =========================================================================
void Rubiks::renderIcon(TextureState& tex) {
    int startZ = RNDR_CZ - 4; 
    int endZ = startZ + 7;

    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            for (int z = startZ; z <= endZ; z++) {
                CRGB voxelColor = CRGB::Black;
                if (z == endZ) voxelColor = CRGB::White;
                else if (z == startZ) voxelColor = CRGB::Yellow;
                else if (y == 7) voxelColor = CRGB::Green;
                else if (y == 0) voxelColor = CRGB::Blue;
                else if (x == 0) voxelColor = CRGB(255, 64, 0); // Orange
                else if (x == 7) voxelColor = CRGB::Red;

                if (voxelColor != CRGB::Black) setVoxel(x, y, z, voxelColor);
            }
        }
    }
}

#ifdef TARGET_2026_CUBOID
    // =========================================================================
    // 3x3x5 RUBIK'S TOWER ENGINE (Burn Cube 2026)
    // =========================================================================
  //  Rubiks::Cubie Rubiks::logicalCube[3][3][5];

    void Rubiks::resetCube() {
        for (int x = 0; x < 3; x++) {
            for (int y = 0; y < 3; y++) {
                for (int z = 0; z < 5; z++) {
                    for (int f = 0; f < 6; f++) logicalCube[x][y][z].colors[f] = COLOR_BLACK;
                    
                    if (z == 4) logicalCube[x][y][z].colors[UP] = COLOR_U;
                    if (z == 0) logicalCube[x][y][z].colors[DOWN] = COLOR_D;
                    if (y == 2) logicalCube[x][y][z].colors[FRONT] = COLOR_F;
                    if (y == 0) logicalCube[x][y][z].colors[BACK] = COLOR_B;
                    if (x == 0) logicalCube[x][y][z].colors[LEFT] = COLOR_L;
                    if (x == 2) logicalCube[x][y][z].colors[RIGHT] = COLOR_R;
                }
            }
        }
    }

    void Rubiks::rotateCubieColors(Cubie& c, Move m) {
        CRGB temp;
        if (m >= Z0_CW && m <= Z4_CCW) {
            bool cw = (m % 2 == 0); 
            if (!cw) { 
                temp = c.colors[FRONT]; c.colors[FRONT] = c.colors[RIGHT]; 
                c.colors[RIGHT] = c.colors[BACK]; c.colors[BACK] = c.colors[LEFT]; c.colors[LEFT] = temp;
            } else {
                temp = c.colors[FRONT]; c.colors[FRONT] = c.colors[LEFT]; 
                c.colors[LEFT] = c.colors[BACK]; c.colors[BACK] = c.colors[RIGHT]; c.colors[RIGHT] = temp;
            }
        } else if (m >= X0_180 && m <= X2_180) {
            temp = c.colors[UP]; c.colors[UP] = c.colors[DOWN]; c.colors[DOWN] = temp;
            temp = c.colors[FRONT]; c.colors[FRONT] = c.colors[BACK]; c.colors[BACK] = temp;
        } else if (m >= Y0_180 && m <= Y2_180) {
            temp = c.colors[UP]; c.colors[UP] = c.colors[DOWN]; c.colors[DOWN] = temp;
            temp = c.colors[LEFT]; c.colors[LEFT] = c.colors[RIGHT]; c.colors[RIGHT] = temp;
        }
    }

    void Rubiks::applyLogicalMove(Move m) {
        Cubie tempCube[3][3][5];
        for(int x=0; x<3; x++) for(int y=0; y<3; y++) for(int z=0; z<5; z++) tempCube[x][y][z] = logicalCube[x][y][z];

        for(int x=0; x<3; x++) {
            for(int y=0; y<3; y++) {
                for(int z=0; z<5; z++) {
                    int nx = x, ny = y, nz = z;
                    bool inSlice = false;

                    if (m >= Z0_CW && m <= Z4_CCW) {
                        int targetZ = (m - Z0_CW) / 2;
                        if (z == targetZ) {
                            inSlice = true;
                            bool cw = (m % 2 == 0);
                            nx = cw ? y : 2 - y;
                            ny = cw ? 2 - x : x;
                        }
                    } else if (m >= X0_180 && m <= X2_180) {
                        int targetX = m - X0_180;
                        if (x == targetX) { inSlice = true; ny = 2 - y; nz = 4 - z; }
                    } else if (m >= Y0_180 && m <= Y2_180) {
                        int targetY = m - Y0_180;
                        if (y == targetY) { inSlice = true; nx = 2 - x; nz = 4 - z; }
                    }

                    if (inSlice) {
                        logicalCube[nx][ny][nz] = tempCube[x][y][z];
                        rotateCubieColors(logicalCube[nx][ny][nz], m);
                    }
                }
            }
        }
    }

    bool Rubiks::mapToLocalXY(int phys, int& cubieIdx, int& localIdx) {
        if (phys >= 0 && phys <= 1) { cubieIdx = 0; localIdx = phys; return true; }
        if (phys >= 3 && phys <= 4) { cubieIdx = 1; localIdx = phys - 3; return true; }
        if (phys >= 6 && phys <= 7) { cubieIdx = 2; localIdx = phys - 6; return true; }
        return false; 
    }

    bool Rubiks::mapToLocalZ(int phys, int& cubieIdx, int& localIdx) {
        if (phys >= 0 && phys <= 2) { cubieIdx = 0; localIdx = phys; return true; }         
        if (phys >= 4 && phys <= 5) { cubieIdx = 1; localIdx = phys - 4; return true; }     
        if (phys >= 7 && phys <= 8) { cubieIdx = 2; localIdx = phys - 7; return true; }     
        if (phys >= 10 && phys <= 11) { cubieIdx = 3; localIdx = phys - 10; return true; }  
        if (phys >= 13 && phys <= 15) { cubieIdx = 4; localIdx = phys - 13; return true; }  
        return false;
    }

#else
    // =========================================================================
    // STANDARD 3x3x3 ENGINE (Desktop Prototype 16^3)
    // =========================================================================
    //Rubiks::Cubie Rubiks::logicalCube[3][3][3];

    void Rubiks::resetCube() {
        for (int x = 0; x < 3; x++) {
            for (int y = 0; y < 3; y++) {
                for (int z = 0; z < 3; z++) {
                    for (int f = 0; f < 6; f++) logicalCube[x][y][z].colors[f] = COLOR_BLACK;
                    
                    if (y == 2) logicalCube[x][y][z].colors[UP] = COLOR_U;
                    if (y == 0) logicalCube[x][y][z].colors[DOWN] = COLOR_D;
                    if (z == 2) logicalCube[x][y][z].colors[FRONT] = COLOR_F;
                    if (z == 0) logicalCube[x][y][z].colors[BACK] = COLOR_B;
                    if (x == 0) logicalCube[x][y][z].colors[LEFT] = COLOR_L;
                    if (x == 2) logicalCube[x][y][z].colors[RIGHT] = COLOR_R;
                }
            }
        }
    }

    void Rubiks::rotateCubieColors(Cubie& c, Move m) {
        CRGB temp;
        switch(m) {
            case U: temp = c.colors[FRONT]; c.colors[FRONT] = c.colors[RIGHT]; c.colors[RIGHT] = c.colors[BACK]; c.colors[BACK] = c.colors[LEFT]; c.colors[LEFT] = temp; break;
            case U_PRIME: temp = c.colors[FRONT]; c.colors[FRONT] = c.colors[LEFT]; c.colors[LEFT] = c.colors[BACK]; c.colors[BACK] = c.colors[RIGHT]; c.colors[RIGHT] = temp; break;
            case D: temp = c.colors[FRONT]; c.colors[FRONT] = c.colors[LEFT]; c.colors[LEFT] = c.colors[BACK]; c.colors[BACK] = c.colors[RIGHT]; c.colors[RIGHT] = temp; break;
            case D_PRIME: temp = c.colors[FRONT]; c.colors[FRONT] = c.colors[RIGHT]; c.colors[RIGHT] = c.colors[BACK]; c.colors[BACK] = c.colors[LEFT]; c.colors[LEFT] = temp; break;
            case F: temp = c.colors[UP]; c.colors[UP] = c.colors[LEFT]; c.colors[LEFT] = c.colors[DOWN]; c.colors[DOWN] = c.colors[RIGHT]; c.colors[RIGHT] = temp; break;
            case F_PRIME: temp = c.colors[UP]; c.colors[UP] = c.colors[RIGHT]; c.colors[RIGHT] = c.colors[DOWN]; c.colors[DOWN] = c.colors[LEFT]; c.colors[LEFT] = temp; break;
            case B: temp = c.colors[UP]; c.colors[UP] = c.colors[RIGHT]; c.colors[RIGHT] = c.colors[DOWN]; c.colors[DOWN] = c.colors[LEFT]; c.colors[LEFT] = temp; break;
            case B_PRIME: temp = c.colors[UP]; c.colors[UP] = c.colors[LEFT]; c.colors[LEFT] = c.colors[DOWN]; c.colors[DOWN] = c.colors[RIGHT]; c.colors[RIGHT] = temp; break;
            case L: temp = c.colors[UP]; c.colors[UP] = c.colors[BACK]; c.colors[BACK] = c.colors[DOWN]; c.colors[DOWN] = c.colors[FRONT]; c.colors[FRONT] = temp; break;
            case L_PRIME: temp = c.colors[UP]; c.colors[UP] = c.colors[FRONT]; c.colors[FRONT] = c.colors[DOWN]; c.colors[DOWN] = c.colors[BACK]; c.colors[BACK] = temp; break;
            case R: temp = c.colors[UP]; c.colors[UP] = c.colors[FRONT]; c.colors[FRONT] = c.colors[DOWN]; c.colors[DOWN] = c.colors[BACK]; c.colors[BACK] = temp; break;
            case R_PRIME: temp = c.colors[UP]; c.colors[UP] = c.colors[BACK]; c.colors[BACK] = c.colors[DOWN]; c.colors[DOWN] = c.colors[FRONT]; c.colors[FRONT] = temp; break;
        }
    }

    void Rubiks::applyLogicalMove(Move m) {
        Cubie tempCube[3][3][3];
        for(int x=0; x<3; x++) for(int y=0; y<3; y++) for(int z=0; z<3; z++) tempCube[x][y][z] = logicalCube[x][y][z];

        for(int x=0; x<3; x++) {
            for(int y=0; y<3; y++) {
                for(int z=0; z<3; z++) {
                    int nx = x, ny = y, nz = z;
                    bool inSlice = false;

                    if ((m == U || m == U_PRIME) && y == 2) { inSlice = true; nx = (m == U) ? 2 - z : z; nz = (m == U) ? x : 2 - x; }
                    else if ((m == D || m == D_PRIME) && y == 0) { inSlice = true; nx = (m == D) ? z : 2 - z; nz = (m == D) ? 2 - x : x; }
                    else if ((m == F || m == F_PRIME) && z == 2) { inSlice = true; nx = (m == F) ? y : 2 - y; ny = (m == F) ? 2 - x : x; }
                    else if ((m == B || m == B_PRIME) && z == 0) { inSlice = true; nx = (m == B) ? 2 - y : y; ny = (m == B) ? x : 2 - x; }
                    else if ((m == L || m == L_PRIME) && x == 0) { inSlice = true; nz = (m == L) ? y : 2 - y; ny = (m == L) ? 2 - z : z; }
                    else if ((m == R || m == R_PRIME) && x == 2) { inSlice = true; nz = (m == R) ? 2 - y : y; ny = (m == R) ? z : 2 - z; }

                    if (inSlice) {
                        logicalCube[nx][ny][nz] = tempCube[x][y][z];
                        rotateCubieColors(logicalCube[nx][ny][nz], m);
                    }
                }
            }
        }
    }

    bool Rubiks::mapToLocal(int phys, int& cubieIdx, int& localIdx) {
        if (phys >= 0 && phys <= 3) { cubieIdx = 0; localIdx = phys; return true; }
        if (phys >= 6 && phys <= 9) { cubieIdx = 1; localIdx = phys - 6; return true; }
        if (phys >= 12 && phys <= 15) { cubieIdx = 2; localIdx = phys - 12; return true; }
        return false; 
    }
#endif

// =========================================================================
// SHARED UTILITIES & RENDERERS
// =========================================================================
void Rubiks::init() {
    resetCube();
}

CRGB Rubiks::getStaticVoxelColor(int x, int y, int z) {
    int cx, cy, cz, lx, ly, lz; 

    #ifdef TARGET_2026_CUBOID
        if (!mapToLocalXY(x, cx, lx) || !mapToLocalXY(y, cy, ly) || !mapToLocalZ(z, cz, lz)) {
            return COLOR_BLACK; 
        }
    #else
        if (!mapToLocal(x, cx, lx) || !mapToLocal(y, cy, ly) || !mapToLocal(z, cz, lz)) {
            return COLOR_BLACK; 
        }
    #endif

    Cubie& c = logicalCube[cx][cy][cz];
    CRGB activeColors[3];
    uint8_t colorCount = 0;

    #ifdef TARGET_2026_CUBOID
        if (cz == 4 && lz == 2) activeColors[colorCount++] = c.colors[UP];
        if (cz == 0 && lz == 0) activeColors[colorCount++] = c.colors[DOWN];
        if (cy == 2 && ly == 1) activeColors[colorCount++] = c.colors[FRONT];
        if (cy == 0 && ly == 0) activeColors[colorCount++] = c.colors[BACK];
        if (cx == 0 && lx == 0) activeColors[colorCount++] = c.colors[LEFT];
        if (cx == 2 && lx == 1) activeColors[colorCount++] = c.colors[RIGHT];
    #else
        if (cy == 2 && ly == 3) activeColors[colorCount++] = c.colors[UP];
        if (cy == 0 && ly == 0) activeColors[colorCount++] = c.colors[DOWN];
        if (cx == 2 && lx == 3) activeColors[colorCount++] = c.colors[RIGHT];
        if (cx == 0 && lx == 0) activeColors[colorCount++] = c.colors[LEFT];
        if (cz == 2 && lz == 3) activeColors[colorCount++] = c.colors[FRONT];
        if (cz == 0 && lz == 0) activeColors[colorCount++] = c.colors[BACK];
    #endif

    if (colorCount == 0) return COLOR_BLACK; 
    if (colorCount == 1) return activeColors[0]; 
    
    uint8_t beat = beat8(45); 
    if (colorCount == 2) return blend(activeColors[0], activeColors[1], cubicwave8(beat));
    uint8_t phase = beat / 85;               
    uint8_t mix = (beat % 85) * 3;           
    if (phase == 0) return blend(activeColors[0], activeColors[1], mix);
    if (phase == 1) return blend(activeColors[1], activeColors[2], mix);
    return blend(activeColors[2], activeColors[0], mix);
}

void Rubiks::drawStaticCube() {
    for (uint8_t x = 0; x < RNDR_X; x++) {
        for (uint8_t y = 0; y < RNDR_Y; y++) {
            for (uint8_t z = 0; z < RNDR_Z; z++) {
                setVoxel(x, y, z, getStaticVoxelColor(x, y, z)); 
            }
        }
    }
}

void Rubiks::drawIntermediateFrame(Move m, float angle) {
    float rad = -angle * (PI / 180.0f);
    float s = sin(rad);
    float c = cos(rad);
    
    #ifdef TARGET_2026_CUBOID
        float centerX = 3.5f; float centerY = 3.5f; float centerZ = 7.5f;
    #else
        float centerX = 7.5f; float centerY = 7.5f; float centerZ = 7.5f;
    #endif

    for (uint8_t x = 0; x < RNDR_X; x++) {
        for (uint8_t y = 0; y < RNDR_Y; y++) {
            for (uint8_t z = 0; z < RNDR_Z; z++) {
                
                bool inSlice = false;
                
                #ifdef TARGET_2026_CUBOID
                    int cIdx, lIdx;
                    if (m >= Z0_CW && m <= Z4_CCW) {
                        int targetZ = (m - Z0_CW) / 2;
                        if (mapToLocalZ(z, cIdx, lIdx) && cIdx == targetZ) inSlice = true;
                    } else if (m >= X0_180 && m <= X2_180) {
                        int targetX = m - X0_180;
                        if (mapToLocalXY(x, cIdx, lIdx) && cIdx == targetX) inSlice = true;
                    } else if (m >= Y0_180 && m <= Y2_180) {
                        int targetY = m - Y0_180;
                        if (mapToLocalXY(y, cIdx, lIdx) && cIdx == targetY) inSlice = true;
                    }
                #else
                    if ((m == U || m == U_PRIME) && y >= 12) inSlice = true;
                    else if ((m == D || m == D_PRIME) && y <= 3) inSlice = true;
                    else if ((m == F || m == F_PRIME) && z >= 12) inSlice = true;
                    else if ((m == B || m == B_PRIME) && z <= 3) inSlice = true;
                    else if ((m == L || m == L_PRIME) && x <= 3) inSlice = true;
                    else if ((m == R || m == R_PRIME) && x >= 12) inSlice = true;
                #endif

                if (!inSlice) {
                    setVoxel(x, y, z, getStaticVoxelColor(x, y, z));
                } else {
                    float dx = (float)x - centerX;
                    float dy = (float)y - centerY;
                    float dz = (float)z - centerZ;
                    int srcX = x, srcY = y, srcZ = z;

                    #ifdef TARGET_2026_CUBOID
                        if (m >= Z0_CW && m <= Z4_CCW) {
                            srcX = (int)round(dx * c - dy * s + centerX);
                            srcY = (int)round(dx * s + dy * c + centerY);
                        } else if (m >= X0_180 && m <= X2_180) {
                            srcY = (int)round(dy * c - dz * s + centerY);
                            srcZ = (int)round(dy * s + dz * c + centerZ);
                        } else if (m >= Y0_180 && m <= Y2_180) {
                            srcX = (int)round(dx * c - dz * s + centerX);
                            srcZ = (int)round(dx * s + dz * c + centerZ);
                        }
                    #else
                        if (m == U || m == U_PRIME || m == D || m == D_PRIME) {
                            srcX = (int)round(dx * c - dz * s + centerX);
                            srcZ = (int)round(dx * s + dz * c + centerZ);
                        } else if (m == F || m == F_PRIME || m == B || m == B_PRIME) {
                            srcX = (int)round(dx * c - dy * s + centerX);
                            srcY = (int)round(dx * s + dy * c + centerY);
                        } else if (m == L || m == L_PRIME || m == R || m == R_PRIME) {
                            srcZ = (int)round(dz * c - dy * s + centerX);
                            srcY = (int)round(dz * s + dy * c + centerY);
                        }
                    #endif

                    if (srcX >= 0 && srcX < RNDR_X && srcY >= 0 && srcY < RNDR_Y && srcZ >= 0 && srcZ < RNDR_Z) {
                        setVoxel(x, y, z, getStaticVoxelColor(srcX, srcY, srcZ)); 
                    } else {
                        setVoxel(x, y, z, COLOR_BLACK);
                    }
                }
            }
        }
    }
}

void Rubiks::animateMove(Move m, uint16_t duration_ms) {
    uint32_t start_time = millis();
    float target_angle;
    
    #ifdef TARGET_2026_CUBOID
        if (m >= Z0_CW && m <= Z4_CCW) {
            target_angle = 90.0f;
            if (m % 2 != 0) target_angle = -90.0f; // CCW
        } else {
            target_angle = 180.0f; 
        }
    #else
        target_angle = 90.0f;
        if (m == U_PRIME || m == D_PRIME || m == F_PRIME || m == B_PRIME || m == L_PRIME || m == R_PRIME) target_angle = -90.0f; 
        if (m == D || m == D_PRIME || m == B || m == B_PRIME || m == L || m == L_PRIME) target_angle *= -1.0f; 
    #endif

    while (millis() - start_time < duration_ms) {
        float progress = (float)(millis() - start_time) / duration_ms;
        float ease = (1.0f - cos(progress * PI)) / 2.0f; 
        float current_angle = target_angle * ease;

        clearAll(); 
        drawIntermediateFrame(m, current_angle);
        showCube();
        delay(5); 
    }

    applyLogicalMove(m);
    drawStaticCube();
    showCube();
}

void Rubiks::performScramble(int moves, Move* historyOut, bool superhuman) {
    for (int i = 0; i < moves; i++) {
        #ifdef TARGET_2026_CUBOID
            Move randomMove = static_cast<Move>(random8() % 16); 
            uint16_t dur = (randomMove >= X0_180) ? SPEED_SCRAMBLE_180 : SPEED_SCRAMBLE_90;
        #else
            Move randomMove = static_cast<Move>(random8() % 12); 
            uint16_t dur = SPEED_SCRAMBLE_90;
        #endif
        
        if (historyOut != nullptr) historyOut[i] = randomMove;
        
        if (superhuman) {
            applyLogicalMove(randomMove);
            drawStaticCube();
            showCube();
            delay(15); 
        } else {
            animateMove(randomMove, dur); 
            delay(PAUSE_SCRAMBLE);
        }
    }
}

void Rubiks::runScrambleAndSolveShow() {
    init();
    
    drawStaticCube();
    showCube();
    delay(1000); 

    #ifdef TARGET_2026_CUBOID
        const int SCRAMBLE_MOVES = 35;
        Move history[SCRAMBLE_MOVES];

        performScramble(SCRAMBLE_MOVES, history);
        delay(2000); 

        for (int i = SCRAMBLE_MOVES - 1; i >= 0; i--) {
            Move reverseMove;
            Move prev = history[i];
            if (prev >= Z0_CW && prev <= Z4_CCW) reverseMove = static_cast<Move>(prev ^ 1); 
            else reverseMove = prev; 
            
            uint16_t dur = (reverseMove >= X0_180) ? SPEED_SOLVE_180 : SPEED_SOLVE_90;
            animateMove(reverseMove, dur); 
            delay(PAUSE_SOLVE);
        }
    #else
        const int SCRAMBLE_MOVES = 30;
        Move history[SCRAMBLE_MOVES];

        for (int i = 0; i < SCRAMBLE_MOVES; i++) {
            Move randomMove = static_cast<Move>(random8() % 12); 
            history[i] = randomMove;
            animateMove(randomMove, SPEED_SCRAMBLE_90); 
            delay(PAUSE_SCRAMBLE);
        }

        delay(2000); 

        for (int i = SCRAMBLE_MOVES - 1; i >= 0; i--) {
            Move reverseMove;
            switch(history[i]) {
                case U: reverseMove = U_PRIME; break; case U_PRIME: reverseMove = U; break;
                case D: reverseMove = D_PRIME; break; case D_PRIME: reverseMove = D; break;
                case F: reverseMove = F_PRIME; break; case F_PRIME: reverseMove = F; break;
                case B: reverseMove = B_PRIME; break; case B_PRIME: reverseMove = B; break;
                case L: reverseMove = L_PRIME; break; case L_PRIME: reverseMove = L; break;
                case R: reverseMove = R_PRIME; break; case R_PRIME: reverseMove = R; break;
            }
            animateMove(reverseMove, SPEED_SOLVE_90); 
            delay(PAUSE_SOLVE);
        }
    #endif

    playStartBlink(SPEED_BLINK); 
    delay(3000); 
}

void Rubiks::playStartBlink(uint16_t interval_ms) {
    for (int i = 0; i < 3; i++) {
        clearAll();
        showCube();
        delay(interval_ms);
        
        drawStaticCube();
        showCube();
        delay(interval_ms);
    }
}

void Rubiks::testInitialState() {
    init(); 
    
    uint32_t start_time = millis();
    while (millis() - start_time < 30000) {
        drawStaticCube();
        showCube();
        delay(15); 
    }
    
    clearAll();
    showCube();
}

void Rubiks::run() {
    init();
    
    // 1. THE OPENING SCRAMBLE (Superhuman speed enabled)
    performScramble(30, nullptr, true);

    // 2. INTERACTIVE GAME STATE
    enum SelAxis { AXIS_Z, AXIS_X, AXIS_Y };
    SelAxis currentAxis = AXIS_Z;
    int currentSlice = 0;
    
    uint32_t lastInputTime = millis();
    
    while(true) { 
        
        GamepadState p1 = BluetoothManager::getState(ROLE_MASTER);
        GamepadState p2 = BluetoothManager::getState(ROLE_PLAYER_2);
        GamepadState activePad = p1.connected ? p1 : p2;

       if (activePad.connected && millis() - lastInputTime > 200) {
            bool moved = false;
            
            bool upPressed    = activePad.dpad & DPAD_UP    || activePad.axisY < -200;
            bool downPressed  = activePad.dpad & DPAD_DOWN  || activePad.axisY > 200;
            bool leftPressed  = activePad.dpad & DPAD_LEFT  || activePad.axisX < -200;
            bool rightPressed = activePad.dpad & DPAD_RIGHT || activePad.axisX > 200;

        #ifdef HARDWARE_BURNCUBE
            // 90-degree CW hardware rotation map
            bool tempUp = upPressed;
            upPressed = leftPressed;
            leftPressed = downPressed;
            downPressed = rightPressed;
            rightPressed = tempUp;
        #endif

            if (rightPressed) {
                currentAxis = static_cast<SelAxis>((currentAxis + 1) % 3);
                currentSlice = 0; 
                moved = true;
            } else if (leftPressed) {
                currentAxis = static_cast<SelAxis>((currentAxis + 2) % 3);
                currentSlice = 0;
                moved = true;
            }
            
            int maxSlices = 3;
            #ifdef TARGET_2026_CUBOID
                if (currentAxis == AXIS_Z) maxSlices = 5;
            #endif

            if (upPressed) {
                currentSlice = (currentSlice + 1) % maxSlices;
                moved = true;
            } else if (downPressed) {
                currentSlice = (currentSlice + maxSlices - 1) % maxSlices;
                moved = true;
            }

            bool actionCW = activePad.a || activePad.x;
            bool actionCCW = activePad.b || activePad.y; 
            
            if (actionCW || actionCCW) {
                bool isValidMove = false;
                Move moveToPlay;
                uint16_t speedToPlay = SPEED_SOLVE_90;

                #ifdef TARGET_2026_CUBOID
                    if (currentAxis == AXIS_Z) {
                        moveToPlay = static_cast<Move>(Z0_CW + (currentSlice * 2) + (actionCCW ? 1 : 0));
                        isValidMove = true;
                    } else if (currentAxis == AXIS_X) {
                        moveToPlay = static_cast<Move>(X0_180 + currentSlice);
                        speedToPlay = SPEED_SOLVE_180;
                        isValidMove = true;
                    } else if (currentAxis == AXIS_Y) {
                        moveToPlay = static_cast<Move>(Y0_180 + currentSlice);
                        speedToPlay = SPEED_SOLVE_180;
                        isValidMove = true;
                    }
                #else
                    if (currentSlice != 1) {
                        isValidMove = true;
                        if (currentAxis == AXIS_Z) moveToPlay = (currentSlice == 0) ? (actionCW ? B : B_PRIME) : (actionCW ? F : F_PRIME);
                        if (currentAxis == AXIS_X) moveToPlay = (currentSlice == 0) ? (actionCW ? L : L_PRIME) : (actionCW ? R : R_PRIME);
                        if (currentAxis == AXIS_Y) moveToPlay = (currentSlice == 0) ? (actionCW ? D : D_PRIME) : (actionCW ? U : U_PRIME);
                    }
                #endif

                if (isValidMove) {
                    animateMove(moveToPlay, speedToPlay);
                }
                moved = true; 
            }
            if (moved) lastInputTime = millis();
        }

        clearAll();
        uint8_t pulse = beatsin8(60, 32, 255); 

        for(int x=0; x<RNDR_X; x++) {
            for(int y=0; y<RNDR_Y; y++) {
                for(int z=0; z<RNDR_Z; z++) {
                    CRGB existing = getStaticVoxelColor(x, y, z);
                    if (existing != COLOR_BLACK) {
                        int cIdx, lIdx;
                        bool highlight = false;

                        #ifdef TARGET_2026_CUBOID
                            if (currentAxis == AXIS_Z && mapToLocalZ(z, cIdx, lIdx) && cIdx == currentSlice) highlight = true;
                            if (currentAxis == AXIS_X && mapToLocalXY(x, cIdx, lIdx) && cIdx == currentSlice) highlight = true;
                            if (currentAxis == AXIS_Y && mapToLocalXY(y, cIdx, lIdx) && cIdx == currentSlice) highlight = true;
                        #else
                            if (currentAxis == AXIS_Z && mapToLocal(z, cIdx, lIdx) && cIdx == currentSlice) highlight = true;
                            if (currentAxis == AXIS_X && mapToLocal(x, cIdx, lIdx) && cIdx == currentSlice) highlight = true;
                            if (currentAxis == AXIS_Y && mapToLocal(y, cIdx, lIdx) && cIdx == currentSlice) highlight = true;
                        #endif

                        if (highlight) existing.nscale8(pulse); 
                        
                        setVoxel(x, y, z, existing);
                    }
                }
            }
        }
        showCube(); 
        delay(15);
    }
}