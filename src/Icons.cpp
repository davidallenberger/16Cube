#include "Icons.h"
#include "CubeCore.h"
#include "icons.h"
#include <FastLED.h>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <deque> // NEW: Required for the infinite sliding window

// Upgraded to hold the array of frames for live evaluation!
struct IconDrawCommand {
  std::vector<int> frames;
  bool rotate;
  bool rainbow;
};

// --- WLED Native Randomizer ---
static std::string expandRandomString(const std::string& input) {
  std::string result = input;
  size_t start = result.find('{');
  size_t end = result.find('}');
  
  if (start != std::string::npos && end != std::string::npos && end > start) {
    std::string inner = result.substr(start + 1, end - start - 1);
    
    std::vector<std::string> pool;
    std::stringstream ss(inner);
    std::string token;
    
    while (std::getline(ss, token, ',')) {
      pool.push_back(token);
    }
    
    if (pool.empty()) return result; 
    
    std::string generated = "";
    int numIcons = 4; // Generates chunks of 4. This will now fire continuously!
    
    for (int i = 0; i < numIcons; i++) {
      int randomIndex = random(pool.size());
      if (random(2) == 1) generated += "C"; 
      generated += pool[randomIndex];
      if (i < numIcons - 1) generated += ",";
    }
    
    result.replace(start, end - start + 1, generated);
  }
  return result; 
}

// --- WLED String Parser ---
static std::string stripToAllowed(const std::string& input) {
  std::string result;
  for (char ch : input) {
    if ((ch >= '0' && ch <= '9') || ch == ',' || ch == '.' || ch == '(' || 
        ch == ')' || ch == 'R' || ch == 'C' || ch == 'B') {
      result += ch;
    }
  }
  return result;
}

// Upgraded to return raw vectors for live evaluation
static std::vector<IconDrawCommand> parseInstructions(const std::string& icon_instructions) {
  std::vector<IconDrawCommand> result;
  std::stringstream ss(icon_instructions);
  std::string token;
  
  while (std::getline(ss, token, ',')) {
    bool rotate = false, rainbow = false;
    
    int ptr = 0;
    while (ptr < token.length()) {
      if (token[ptr] == 'R') rotate = true;
      else if (token[ptr] == 'C') rainbow = true;
      else if (token[ptr] == 'B') { rotate = true; rainbow = true; }
      else break;
      ptr++;
    }
    token = token.substr(ptr);
    if (token.empty()) continue;
    
    if (token.front() == '(' && token.back() == ')') {
      std::string inner = token.substr(1, token.size() - 2);
      std::vector<int> frames;
      std::stringstream ssInner(inner);
      std::string f;
      while (std::getline(ssInner, f, '.')) {
        frames.push_back(atoi(f.c_str()));
      }
      if (!frames.empty()) {
        result.push_back({frames, rotate, rainbow}); // Store the whole array
      }
    } else {
      result.push_back({{atoi(token.c_str())}, rotate, rainbow}); // Store as 1-item array
    }
  }
  return result;
}

// --- Coordinate Mappers ---
static void rotate16x16(uint32_t phase, int& x, int& y) {
  const float cx = 7.5, cy = 7.5;
  float dx = x - cx, dy = y - cy;
  float angle = (phase % 16) * (PI / 8.0);
  float cosA = cos(angle);
  float sinA = sin(angle);
  int rx = round(dx * cosA - dy * sinA);
  int ry = round(dx * sinA + dy * cosA);
  x = constrain((int)(cx + rx), 0, 15);
  y = constrain((int)(cy + ry), 0, 15);
}

// --- WLED Palette ---
static CRGB getPerceptualRainbowColor(uint32_t phase) {
  return CHSV(phase, 255, 255); 
}

static CRGB getColorFromPaletteCode(uint8_t iconByte) {
  switch (iconByte) {
    case 1:  return CRGB::Red;          case 2:  return CRGB::Orange;
    case 3:  return CRGB::Yellow;       case 4:  return CRGB::Green;
    case 5:  return CRGB::Blue;         case 6:  return CRGB::Purple;
    case 7:  return CRGB::White;        case 8:  return CRGB::DarkGreen;
    case 9:  return CRGB::LimeGreen;    case 10: return CRGB::LightBlue;
    case 11: return CRGB::RoyalBlue;    case 12: return CRGB::White;
    case 13: return CRGB::OrangeRed;    case 14: return CRGB::DarkRed;
    case 15: return CRGB::Pink;         case 16: return CRGB::LightPink;
    case 17: return CRGB::LightSlateGrey;case 18: return CRGB::MediumTurquoise;
    case 19: return CRGB::Tan;          case 20: return 0x966919; 
    case 21: return 0x800020;           case 22: return CRGB::Gold;
    default: return CRGB::Black;
  }
}

namespace Icons {

void scrollIcons(const char* iconString, uint32_t durationMs, uint8_t scrollDelayMs, uint16_t animDelayMs, bool counterClockwise) {
  
  const uint8_t SCROLL_AXIS = 1; 

  uint32_t currentFrame = 0;
  uint32_t rotationPhase = 0;
  uint32_t globalRainbowPhase = 0;
  
  uint32_t startTime = millis();
  uint32_t lastAnimTime = startTime;
  uint32_t lastRainbowTime = startTime;
  
  int scroll_pos = 0;

  // THE INFINITE TAPE BUFFER (Sliding Window)
  std::deque<IconDrawCommand> infiniteTape;
  int tapeStartCol = 0; 

  // CONTINUOUS LOOPING ENGINE
  while (true) {
    if (durationMs > 0 && (millis() - startTime >= durationMs)) break;

    clearAll();
    
    uint32_t now = millis();
    if (now - lastAnimTime >= animDelayMs) {
      currentFrame++;     
      rotationPhase++;    
      lastAnimTime = now;
    }
    if (now - lastRainbowTime >= 30) {
      globalRainbowPhase += 8; 
      lastRainbowTime = now;
    }

    // 1. PRUNE OFF-SCREEN ICONS
    // If the right edge of an icon (tapeStartCol + 16) falls behind the visible tail of the screen (scroll_pos - 59)
    while (infiniteTape.size() > 0 && (tapeStartCol + 16 < scroll_pos - (RNDR_PERIMETER - 1))) {
        infiniteTape.pop_front();
        tapeStartCol += 16;
    }

    // 2. GENERATE NEW RANDOM ICONS
    // If the front edge of the tape drops below the leading edge of the screen (scroll_pos)
    while (tapeStartCol + (infiniteTape.size() * 16) <= scroll_pos) {
        std::string expanded = expandRandomString(iconString);
        std::string parsedString = stripToAllowed(expanded);
        
        std::vector<IconDrawCommand> newCmds = parseInstructions(parsedString);
        std::reverse(newCmds.begin(), newCmds.end()); // Last icon enters first
        
        // Append smoothly with zero spacing
        for (auto& cmd : newCmds) {
            infiniteTape.push_back(cmd);
        }
    }
    
    // 3. RENDER THE SLIDING WINDOW
    for (int p = 0; p < RNDR_PERIMETER; p++) {
      
      int virtual_p = counterClockwise ? p : ((RNDR_PERIMETER - 1) - p);
      int msg_col = scroll_pos - virtual_p;
      
      // If the pixel hits our active tape block
      if (msg_col >= tapeStartCol && msg_col < tapeStartCol + (infiniteTape.size() * 16)) {
        
        int iconIndex = (msg_col - tapeStartCol) / 16;
        int localX = (msg_col - tapeStartCol) % 16; 
        
        IconDrawCommand cmd = infiniteTape[iconIndex];
        
        // LIVE EVALUATION: Get the specific frame for this exact millisecond
        int currentIconId = cmd.frames[currentFrame % cmd.frames.size()];
        
        uint8_t u, w;
        getPerimeterCoords(p, u, w);
        
       for (int h = 0; h < 16; h++) {
          int x = 15 - localX; 
          int y = 15 - h;
          
          // --- THE PERFECTED DIRECTIONAL KLUDGE ---
          if (counterClockwise) {
              // CCW Math (-p) mirrors the entire tape. Flip normal icons back to upright.
              // Exclude Pacman (IDs 1-6) because the math-mirror perfectly makes him face left!
              if (!(currentIconId >= 1 && currentIconId <= 6)) {
                  x = 15 - x; 
              }
          } 
          
          if (cmd.rotate) rotate16x16(rotationPhase, x, y);
          if (currentIconId >= NUMBER_OF_ICONS) continue;
          
          uint8_t iconByte = pgm_read_byte(&icons[currentIconId][y * 16 + x]);
          
          if (iconByte != 0) {
            CRGB color;
            if (cmd.rainbow) {
              uint8_t phase = (iconByte * 16 + globalRainbowPhase) % 256;
              color = getPerceptualRainbowColor(phase);
            } else {
              color = getColorFromPaletteCode(iconByte);
            }
            
            if (SCROLL_AXIS == 1)      setVoxel(u, w, h, color);      
            else if (SCROLL_AXIS == 0) setVoxel(h, u, w, color); 
            else                       setVoxel(u, h, w, color);                
          }
        }
      }
    }
    
    showCube();
    delay(scrollDelayMs); 
    scroll_pos++;
    yield();
  }
}

} // namespace Icons