#include "CubeCore.h"
#include "CubeSystem.h"

// ========================================================
//                 WIRING & PINS CONFIGURATION
// ========================================================
#define LED_TYPE      WS2812


#ifdef HARDWARE_BURNCUBE
    // --- BURN CUBE 2026 (Dig-Octa Brainboard) ---
    #define BRIGHTNESS 64  
    #define COLOR_ORDER   RGB

    // Dig-Octa Left-to-Right Output Pins (LED 6,5,4,3)
    #define PIN_0 5  // Brainboard LED 6
    #define PIN_1 4  // Brainboard LED 5
    #define PIN_2 0  // Brainboard LED 1
    #define PIN_3 2  // Brainboard LED 3

    // --- THE SAFETY PORTS (Unused LEDs) ---
    #define PIN_SAFE_1 1  
    #define PIN_SAFE_2 3  
    #define PIN_SAFE_3 12 
    #define PIN_SAFE_4 13 

    #define STRIP_LEN 256
    CRGB strip0[STRIP_LEN];
    CRGB strip1[STRIP_LEN];
    CRGB strip2[STRIP_LEN];
    CRGB strip3[STRIP_LEN];

    CRGB safetyPixel[1];

#else
    // --- DESKTOP PROTOTYPE (Sacrificial Pixels) ---
    #define BRIGHTNESS 32
    #define COLOR_ORDER   GRB
   
    #define PIN_0 23   
    #define PIN_1 19   
    #define PIN_2 22   
    #define PIN_3 21   
    #define PIN_4 4    
    #define PIN_5 5    
    #define PIN_6 2    
    #define PIN_7 18   

    #define STRIP0_LEN 544   
    #define STRIP1_LEN 544   
    #define STRIP2_LEN 544   
    #define STRIP3_LEN 544   
    #define STRIP4_LEN 544   
    #define STRIP5_LEN 544   
    #define STRIP6_LEN 550   
    #define STRIP7_LEN 547   

    CRGB strip0[STRIP0_LEN]; CRGB strip1[STRIP1_LEN];
    CRGB strip2[STRIP2_LEN]; CRGB strip3[STRIP3_LEN];
    CRGB strip4[STRIP4_LEN]; CRGB strip5[STRIP5_LEN];
    CRGB strip6[STRIP6_LEN]; CRGB strip7[STRIP7_LEN];

    static constexpr uint16_t COL_STRIDE   = 17;               
    static constexpr uint16_t PLANE_STRIDE = CUBE_SIZE * COL_STRIDE;   
#endif


// ========================================================
//                 3D -> PHYSICAL MAPPING
// ========================================================
struct MappedPixel {
  uint8_t  strip;   
  uint16_t index;
  bool     ok;
};

#ifdef HARDWARE_BURNCUBE
// --- BURN CUBE MAPPING ---
static inline MappedPixel mapXYZ(uint8_t x, uint8_t y, uint8_t z) {
  MappedPixel m{0, 0, false};
  if (x >= RNDR_X || y >= RNDR_Y || z >= RNDR_Z) return m;

  uint8_t y_inv = (RNDR_Y - 1) - y;
  uint8_t x_local = 0;

  switch(x) {
      case 0: m.strip = 3; x_local = 1; break; 
      case 1: m.strip = 3; x_local = 0; break; 
      case 2: m.strip = 2; x_local = 1; break; 
      case 3: m.strip = 2; x_local = 0; break; 
      case 4: m.strip = 0; x_local = 0; break; 
      case 5: m.strip = 0; x_local = 1; break; 
      case 6: m.strip = 1; x_local = 0; break; 
      case 7: m.strip = 1; x_local = 1; break; 
  }

  uint8_t drop_index = (x_local == 0) ? y_inv : (7 - y_inv);
  uint8_t pixel_index = 15 - z;
  
  // Fast bitwise equivalent of: (x_local * 128) + (drop_index * 16) + pixel_index
  m.index = (x_local << 7) + (drop_index << 4) + pixel_index;
  m.ok = true;
  return m;
}

#else
// --- DESKTOP MAPPING (ALGEBRAICALLY OPTIMIZED) ---
static inline MappedPixel mapXYZ(uint8_t x, uint8_t y, uint8_t z) {
  MappedPixel m{0, 0, false};
  if (x >= CUBE_SIZE || y >= CUBE_SIZE || z >= CUBE_SIZE) return m;

  // Bitwise division: logicalStrip = y / 4; yLocal = y % 4;
  uint8_t logicalStrip = y >> 2; 
  uint8_t yLocal = y & 3;        

  uint8_t c;
  if (logicalStrip != 3) {
      c = (y & 1) ? (uint8_t)(15 - x) : x; // Serpentine across planes
  } else {
      c = x; // Straight across planes for strip 3
  }
  
  uint8_t zc = (c & 1) ? (uint8_t)(15 - z) : z;

  // Modulo 1088 completely eliminated through algebraic substitution
  uint16_t inStripLogical = (uint16_t)yLocal * PLANE_STRIDE + (uint16_t)c * COL_STRIDE + zc;

  const uint16_t HALF_LOGICAL = 544;

  if (logicalStrip == 0) {
    if (inStripLogical < HALF_LOGICAL) { m.strip = 0; m.index = inStripLogical; }
    else                               { m.strip = 1; m.index = inStripLogical - HALF_LOGICAL; }
  } else if (logicalStrip == 1) {
    if (inStripLogical < HALF_LOGICAL) { m.strip = 2; m.index = inStripLogical; }
    else                               { m.strip = 3; m.index = inStripLogical - HALF_LOGICAL; }
  } else if (logicalStrip == 2) {
    if (inStripLogical < HALF_LOGICAL) { m.strip = 4; m.index = inStripLogical; }
    else                               { m.strip = 5; m.index = inStripLogical - HALF_LOGICAL; }
  } else { 
    // Division by 272 completely eliminated through algebraic substitution
    uint16_t phys = inStripLogical + (uint16_t)(3 * yLocal); 
    if (phys < 550) { m.strip = 6; m.index = phys; }
    else            { m.strip = 7; m.index = phys - 550; }
  }

  uint16_t maxLen =
    (m.strip == 0) ? STRIP0_LEN : (m.strip == 1) ? STRIP1_LEN :
    (m.strip == 2) ? STRIP2_LEN : (m.strip == 3) ? STRIP3_LEN :
    (m.strip == 4) ? STRIP4_LEN : (m.strip == 5) ? STRIP5_LEN :
    (m.strip == 6) ? STRIP6_LEN : STRIP7_LEN;

  m.ok = (m.index < maxLen);
  return m;
}
#endif

// ========================================================
//                       CORE API
// ========================================================
void initCube() {
#ifdef HARDWARE_BURNCUBE
  FastLED.addLeds<LED_TYPE, PIN_0, COLOR_ORDER>(strip0, STRIP_LEN);
  FastLED.addLeds<LED_TYPE, PIN_1, COLOR_ORDER>(strip1, STRIP_LEN);
  FastLED.addLeds<LED_TYPE, PIN_2, COLOR_ORDER>(strip2, STRIP_LEN);
  FastLED.addLeds<LED_TYPE, PIN_3, COLOR_ORDER>(strip3, STRIP_LEN);

  FastLED.addLeds<LED_TYPE, PIN_SAFE_1, COLOR_ORDER>(safetyPixel, 1);
  FastLED.addLeds<LED_TYPE, PIN_SAFE_2, COLOR_ORDER>(safetyPixel, 1);
  FastLED.addLeds<LED_TYPE, PIN_SAFE_3, COLOR_ORDER>(safetyPixel, 1);
  FastLED.addLeds<LED_TYPE, PIN_SAFE_4, COLOR_ORDER>(safetyPixel, 1);
  safetyPixel[0] = CRGB::Red;
#else
  FastLED.addLeds<LED_TYPE, PIN_0, COLOR_ORDER>(strip0, STRIP0_LEN);
  FastLED.addLeds<LED_TYPE, PIN_1, COLOR_ORDER>(strip1, STRIP1_LEN);
  FastLED.addLeds<LED_TYPE, PIN_2, COLOR_ORDER>(strip2, STRIP2_LEN);
  FastLED.addLeds<LED_TYPE, PIN_3, COLOR_ORDER>(strip3, STRIP3_LEN);
  FastLED.addLeds<LED_TYPE, PIN_4, COLOR_ORDER>(strip4, STRIP4_LEN);
  FastLED.addLeds<LED_TYPE, PIN_5, COLOR_ORDER>(strip5, STRIP5_LEN);
  FastLED.addLeds<LED_TYPE, PIN_6, COLOR_ORDER>(strip6, STRIP6_LEN);
  FastLED.addLeds<LED_TYPE, PIN_7, COLOR_ORDER>(strip7, STRIP7_LEN);
#endif

  FastLED.setBrightness(BRIGHTNESS);
  clearAll();
  showCube();
  seedAllRng();
}

void showCube() {
    // 1. Push the pixels
    FastLED.show(); 
    
#ifndef DISABLE_BLUETOOTH
    // 2. Keep the Bluetooth radio breathing
    BluetoothManager::update(); 
    
    ControllerPtr p1 = BluetoothManager::getGamepad(ROLE_MASTER);
    ControllerPtr p2 = BluetoothManager::getGamepad(ROLE_PLAYER_2);
    
    // 3. THE BULLETPROOF TRIPWIRE (Hold for 500ms)
    static uint32_t exitHoldStart = 0;
    
    // Check if either controller is holding a menu/system button
    bool isExitPressed = (p1 && p1->miscButtons()) || (p2 && p2->miscButtons());

    if (isExitPressed) {
        if (exitHoldStart == 0) exitHoldStart = millis();
        
        // ONLY teleport if it has been held continuously for 500ms
        if (millis() - exitHoldStart > 500) {
            if (teleportArmed) {
                exitHoldStart = 0; // Reset before jumping into the void
                longjmp(engineJumpBuffer, 1); 
            }
        }
    } else {
        exitHoldStart = 0; 
    }
#endif
}

void clearAll() {
#ifdef HARDWARE_BURNCUBE
  fill_solid(strip0, STRIP_LEN, CRGB::Black);
  fill_solid(strip1, STRIP_LEN, CRGB::Black);
  fill_solid(strip2, STRIP_LEN, CRGB::Black);
  fill_solid(strip3, STRIP_LEN, CRGB::Black);
#else
  fill_solid(strip0, STRIP0_LEN, CRGB::Black);
  fill_solid(strip1, STRIP1_LEN, CRGB::Black);
  fill_solid(strip2, STRIP2_LEN, CRGB::Black);
  fill_solid(strip3, STRIP3_LEN, CRGB::Black);
  fill_solid(strip4, STRIP4_LEN, CRGB::Black);
  fill_solid(strip5, STRIP5_LEN, CRGB::Black);
  fill_solid(strip6, STRIP6_LEN, CRGB::Black);
  fill_solid(strip7, STRIP7_LEN, CRGB::Black);
#endif
}

void seedAllRng() {
  uint32_t seed = esp_random();
  random16_set_seed(seed);
  random16_add_entropy(esp_random()); 
  srand(seed);
  randomSeed(seed); 
}

void setVoxel(uint8_t x, uint8_t y, uint8_t z, const CRGB& c) {
  if (x >= RNDR_X || y >= RNDR_Y || z >= RNDR_Z) return;

  MappedPixel m = mapXYZ(x, y, z);
  if (!m.ok) return;

#ifdef HARDWARE_BURNCUBE
  switch (m.strip) {
    case 0: strip0[m.index] = c; break; 
    case 1: strip1[m.index] = c; break;
    case 2: strip2[m.index] = c; break; 
    case 3: strip3[m.index] = c; break;
  }
#else
  switch (m.strip) {
    case 0: strip0[m.index] = c; break; case 1: strip1[m.index] = c; break;
    case 2: strip2[m.index] = c; break; case 3: strip3[m.index] = c; break;
    case 4: strip4[m.index] = c; break; case 5: strip5[m.index] = c; break;
    case 6: strip6[m.index] = c; break; case 7: strip7[m.index] = c; break;
  }
#endif
}

CRGB getVoxel(uint8_t x, uint8_t y, uint8_t z) {
  MappedPixel m = mapXYZ(x, y, z);
  if (!m.ok) return CRGB::Black;

#ifdef HARDWARE_BURNCUBE
  switch (m.strip) {
    case 0: return strip0[m.index]; 
    case 1: return strip1[m.index];
    case 2: return strip2[m.index]; 
    case 3: return strip3[m.index];
  }
#else
  switch (m.strip) {
    case 0: return strip0[m.index]; case 1: return strip1[m.index];
    case 2: return strip2[m.index]; case 3: return strip3[m.index];
    case 4: return strip4[m.index]; case 5: return strip5[m.index];
    case 6: return strip6[m.index]; case 7: return strip7[m.index];
  }
#endif
  return CRGB::Black;
}

void debugVoxel(uint8_t x, uint8_t y, uint8_t z) {
  MappedPixel m = mapXYZ(x, y, z);
  Serial.printf("xyz(%d,%d,%d) -> ", x, y, z);
  if (!m.ok) Serial.println("INVALID");
  else Serial.printf("strip %d phys %d\n", m.strip, m.index);
}

void getPerimeterCoords(uint8_t p, uint8_t& u, uint8_t& w) {
#ifdef TARGET_2026_CUBOID
    // 8x8 perimeter mapping (28 LEDs around the ring)
    if (p < 7)       { u = p; w = 0; }               // Face 1
    else if (p < 14) { u = 7; w = p - 7; }           // Face 2 (Bend 1)
    else if (p < 21) { u = 7 - (p - 14); w = 7; }    // Face 3 (Bend 2)
    else             { u = 0; w = 7 - (p - 21); }    // Face 4 (Bend 3)
#else
    // 16x16 perimeter mapping (60 LEDs around the ring)
    if (p < 15)      { u = p; w = 0; } 
    else if (p < 30) { u = 15; w = p - 15; } 
    else if (p < 45) { u = 15 - (p - 30); w = 15; } 
    else             { u = 0; w = 15 - (p - 45); }
#endif
}

void setVoxelByAxis(uint8_t u, uint8_t w, uint8_t h, uint8_t axis, CRGB color) {
    if (axis == 1) setVoxel(u, h, w, color);
    else if (axis == 0) setVoxel(h, u, w, color);
    else setVoxel(u, w, h, color);
}

void fadeAll(uint8_t fadeAmt) {
#ifdef HARDWARE_BURNCUBE
  fadeToBlackBy(strip0, STRIP_LEN, fadeAmt);
  fadeToBlackBy(strip1, STRIP_LEN, fadeAmt);
  fadeToBlackBy(strip2, STRIP_LEN, fadeAmt);
  fadeToBlackBy(strip3, STRIP_LEN, fadeAmt);
#else
  fadeToBlackBy(strip0, STRIP0_LEN, fadeAmt);
  fadeToBlackBy(strip1, STRIP1_LEN, fadeAmt);
  fadeToBlackBy(strip2, STRIP2_LEN, fadeAmt);
  fadeToBlackBy(strip3, STRIP3_LEN, fadeAmt);
  fadeToBlackBy(strip4, STRIP4_LEN, fadeAmt);
  fadeToBlackBy(strip5, STRIP5_LEN, fadeAmt);
  fadeToBlackBy(strip6, STRIP6_LEN, fadeAmt);
  fadeToBlackBy(strip7, STRIP7_LEN, fadeAmt);
#endif
}