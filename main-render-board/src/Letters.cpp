#include "Letters.h"
#include "CubeCore.h"
#include "Icons.h"
#include "Hebrew.h" 
#include <vector>
#include <string>
#include <sstream>

const unsigned char readableFont[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x7e, 0x81, 0xa5, 0x81, 0x9d, 0xb9, 0x81, 0x7e, 0x7e, 0xff, 0xdb, 0xff, 0xe3, 0xc7, 0xff, 0x7e, 0x6c, 0xfe, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00, 0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x10, 0x00, 0x38, 0x7c, 0x38, 0xfe, 0xfe, 0x10, 0x10, 0x7c, 0x00, 0x18, 0x3c, 0x7e, 0xff, 0x7e, 0x18, 0x7e, 0x00, 0x00, 0x18, 0x3c, 0x3c, 0x18, 0x00, 0x00, 0xff, 0xff, 0xe7, 0xc3, 0xc3, 0xe7, 0xff, 0xff, 0x00, 0x3c, 0x66, 0x42, 0x42, 0x66, 0x3c, 0x00, 0xff, 0xc3, 0x99, 0xbd, 0xbd, 0x99, 0xc3, 0xff, 0x0f, 0x07, 0x0f, 0x7d, 0xcc, 0xcc, 0xcc, 0x78, 0x3c, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x7e, 0x18, 0x3f, 0x33, 0x3f, 0x30, 0x30, 0x70, 0xf0, 0xe0, 0x7f, 0x63, 0x7f, 0x63, 0x63, 0x67, 0xe6, 0xc0, 0x99, 0x5a, 0x3c, 0xe7, 0xe7, 0x3c, 0x5a, 0x99, 0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0x00, 0x02, 0x0e, 0x3e, 0xfe, 0x3e, 0x0e, 0x02, 0x00, 0x18, 0x3c, 0x7e, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x66, 0x00, 0x7f, 0xdb, 0xdb, 0x7b, 0x1b, 0x1b, 0x1b, 0x00, 0x3f, 0x60, 0x7c, 0x66, 0x66, 0x3e, 0x06, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x7e, 0x7e, 0x00, 0x18, 0x3c, 0x7e, 0x18, 0x7e, 0x3c, 0x18, 0xff, 0x18, 0x3c, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0x00, 0x00, 0x18, 0x0c, 0xfe, 0x0c, 0x18, 0x00, 0x00, 0x00, 0x30, 0x60, 0xfe, 0x60, 0x30, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xc0, 0xc0, 0xfe, 0x00, 0x00, 0x00, 0x24, 0x66, 0xff, 0x66, 0x24, 0x00, 0x00, 0x00, 0x18, 0x3c, 0x7e, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x7e, 0x3c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00, 0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6c, 0x6c, 0xfe, 0x6c, 0xfe, 0x6c, 0x6c, 0x00, 0x18, 0x7e, 0xc0, 0x7c, 0x06, 0xfc, 0x18, 0x00, 0x00, 0xc6, 0xcc, 0x18, 0x30, 0x66, 0xc6, 0x00, 0x38, 0x6c, 0x38, 0x76, 0xdc, 0xcc, 0x76, 0x00, 0x30, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0c, 0x00, 0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x18, 0x30, 0x00, 0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00, 0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x00, 0x7c, 0xce, 0xde, 0xf6, 0xe6, 0xc6, 0x7c, 0x00, 0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x7c, 0xc6, 0x06, 0x7c, 0xc0, 0xc0, 0xfe, 0x00, 0xfc, 0x06, 0x06, 0x3c, 0x06, 0x06, 0xfc, 0x00, 0x0c, 0xcc, 0xcc, 0xcc, 0xfe, 0x0c, 0x0c, 0x00, 0xfe, 0xc0, 0xfc, 0x06, 0x06, 0xc6, 0x7c, 0x00, 0x7c, 0xc0, 0xc0, 0xfc, 0xc6, 0xc6, 0x7c, 0x00, 0xfe, 0x06, 0x06, 0x0c, 0x18, 0x30, 0x30, 0x00, 0x7c, 0xc6, 0xc6, 0x7c, 0xc6, 0xc6, 0x7c, 0x00, 0x7c, 0xc6, 0xc6, 0x7e, 0x06, 0x06, 0x7c, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x30, 0x0c, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0c, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x30, 0x18, 0x0c, 0x06, 0x0c, 0x18, 0x30, 0x00, 0x3c, 0x66, 0x0c, 0x18, 0x18, 0x00, 0x18, 0x00, 0x7c, 0xc6, 0xde, 0xde, 0xde, 0xc0, 0x7e, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0x00, 0xfc, 0xc6, 0xc6, 0xfc, 0xc6, 0xc6, 0xfc, 0x00, 0x7c, 0xc6, 0xc0, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0xf8, 0xcc, 0xc6, 0xc6, 0xc6, 0xcc, 0xf8, 0x00, 0xfe, 0xc0, 0xc0, 0xf8, 0xc0, 0xc0, 0xfe, 0x00, 0xfe, 0xc0, 0xc0, 0xf8, 0xc0, 0xc0, 0xc0, 0x00, 0x7c, 0xc6, 0xc0, 0xc0, 0xce, 0xc6, 0x7c, 0x00, 0xc6, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0x00, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x06, 0x06, 0x06, 0x06, 0x06, 0xc6, 0x7c, 0x00, 0xc6, 0xcc, 0xd8, 0xf0, 0xd8, 0xcc, 0xc6, 0x00, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xfe, 0x00, 0xc6, 0xee, 0xfe, 0xfe, 0xd6, 0xc6, 0xc6, 0x00, 0xc6, 0xe6, 0xf6, 0xde, 0xce, 0xc6, 0xc6, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0xfc, 0xc6, 0xc6, 0xfc, 0xc0, 0xc0, 0xc0, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xd6, 0xde, 0x7c, 0x06, 0xfc, 0xc6, 0xc6, 0xfc, 0xd8, 0xcc, 0xc6, 0x00, 0x7c, 0xc6, 0xc0, 0x7c, 0x06, 0xc6, 0x7c, 0x00, 0xff, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xfe, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x38, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xd6, 0xfe, 0x6c, 0x00, 0xc6, 0xc6, 0x6c, 0x38, 0x6c, 0xc6, 0xc6, 0x00, 0xc6, 0xc6, 0xc6, 0x7c, 0x18, 0x30, 0xe0, 0x00, 0xfe, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xfe, 0x00, 0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, 0x00, 0xc0, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x02, 0x00, 0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, 0x00, 0x10, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x18, 0x18, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x06, 0x7e, 0xc6, 0x7e, 0x00, 0xc0, 0xc0, 0xc0, 0xfc, 0xc6, 0xc6, 0xfc, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc0, 0xc6, 0x7c, 0x00, 0x06, 0x06, 0x06, 0x7e, 0xc6, 0xc6, 0x7e, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0x7c, 0x00, 0x1c, 0x36, 0x30, 0x78, 0x30, 0x30, 0x78, 0x00, 0x00, 0x00, 0x7e, 0xc6, 0xc6, 0x7e, 0x06, 0xfc, 0xc0, 0xc0, 0xfc, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x06, 0x00, 0x06, 0x06, 0x06, 0x06, 0xc6, 0x7c, 0xc0, 0xc0, 0xcc, 0xd8, 0xf8, 0xcc, 0xc6, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0xcc, 0xfe, 0xfe, 0xd6, 0xd6, 0x00, 0x00, 0x00, 0xfc, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0xfc, 0xc6, 0xc6, 0xfc, 0xc0, 0xc0, 0x00, 0x00, 0x7e, 0xc6, 0xc6, 0x7e, 0x06, 0x06, 0x00, 0x00, 0xfc, 0xc6, 0xc0, 0xc0, 0xc0, 0x00, 0x00, 0x00, 0x7e, 0xc0, 0x7c, 0x06, 0xfc, 0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x18, 0x0e, 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0x7c, 0x38, 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xd6, 0xfe, 0x6c, 0x00, 0x00, 0x00, 0xc6, 0x6c, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0xfc, 0x00, 0x00, 0xfe, 0x0c, 0x38, 0x60, 0xfe, 0x00, 0x0e, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0e, 0x00, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00, 0x70, 0x18, 0x18, 0x0e, 0x18, 0x18, 0x70, 0x00, 0x76, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0x00};

struct RenderItem
{
    bool isIcon;
    bool isHebrew; // ADDED: Direct Hebrew pipeline hook
    int value;
    std::vector<int> iconFrames;
    bool rainbow;
    bool rotate;
    int width;
};

static CRGB getIconColor(uint8_t iconByte)
{
    switch (iconByte)
    {
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

static void rotate16x16(uint32_t phase, int &x, int &y)
{
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

namespace Letters
{
    void scrollTextInternal(const char *text, uint32_t durationMs, uint16_t iterations,
                            float xScale, float yScale, uint8_t scrollDelayMs,
                            ShaderMode fgMode, CRGBPalette16 fgPal,
                            ShaderMode bgMode, CRGBPalette16 bgPal,
                            bool overrideIconColor, uint8_t dimBackgroundPercent, bool rightToLeft)
    {
        // HARDCODED AXIS: Vertical orientation is a physical constant of the cube
        const uint8_t SCROLL_AXIS = 1;

        std::vector<RenderItem> items;
        int ptr = 0;
        int totalWidth = 0;

        int scaledTextWidth = round(8.0f * xScale);
        int scaledTextHeight = round(8.0f * yScale);
        int textYOffset = (16 - scaledTextHeight) / 2;

        // 1. ADVANCED UNIFIED PARSER
        while (text[ptr] != '\0')
        {
            if (text[ptr] == '`')
            {
                ptr++;
                std::string iconStr = "";
                while (text[ptr] != '`' && text[ptr] != '\0')
                {
                    iconStr += text[ptr];
                    ptr++;
                }
                if (text[ptr] == '`')
                    ptr++;

                // --- NEW: NATIVE HEBREW PARSER ---
                if (iconStr.length() > 0 && iconStr[0] == 'H') {
                    for (size_t i = 1; i < iconStr.length(); i++) {
                        char hChar = iconStr[i];
                        int hIndex = Hebrew::getHebrewIndex(hChar);
                        
                        if (hIndex != -1) {
                            int charWidth = Hebrew::hebrewWidths[hIndex];
                            // Pad the physical character width with 2 pixels of kerning
                            items.push_back({false, true, hChar, {}, false, false, charWidth + 2});
                            totalWidth += (charWidth + 2);
                        }
                    }
                    continue; 
                }

                bool rainbow = false;
                bool rotate = false;
                int s_ptr = 0;

                while (s_ptr < iconStr.length())
                {
                    if (iconStr[s_ptr] == 'C') rainbow = true;
                    else if (iconStr[s_ptr] == 'R') rotate = true;
                    else if (iconStr[s_ptr] == 'B') { rainbow = true; rotate = true; }
                    else break;
                    s_ptr++;
                }
                iconStr = iconStr.substr(s_ptr);

                std::vector<int> frames;
                if (iconStr.front() == '(' && iconStr.back() == ')')
                {
                    std::string inner = iconStr.substr(1, iconStr.size() - 2);
                    std::stringstream ss(inner);
                    std::string token;
                    while (std::getline(ss, token, '.'))
                        frames.push_back(atoi(token.c_str()));
                }
                else if (iconStr.front() == '{' && iconStr.back() == '}')
                {
                    std::string inner = iconStr.substr(1, iconStr.size() - 2);
                    std::stringstream ss(inner);
                    std::string token;
                    std::vector<int> pool;
                    while (std::getline(ss, token, ','))
                        pool.push_back(atoi(token.c_str()));
                    if (!pool.empty())
                        frames.push_back(pool[random8(pool.size())]);
                }
                else
                {
                    frames.push_back(atoi(iconStr.c_str()));
                }

                if (!frames.empty())
                {
                    items.push_back({true, false, 0, frames, rainbow, rotate, 16});
                    totalWidth += 16;
                }
            }
            else
            {
                items.push_back({false, false, text[ptr], {}, false, false, scaledTextWidth});
                totalWidth += scaledTextWidth;
                ptr++;
            }
        }

        // PADDING: Add exactly 1 scaled space character of padding
        int paddedWidth = totalWidth + (int)(8.0f * xScale);

        // Animation Timers
        uint32_t currentFrame = 0;
        uint32_t rotationPhase = 0;
        uint32_t globalRainbowPhase = 0;

        uint32_t startTime = millis();
        uint32_t lastAnimTime = startTime;
        uint32_t lastRainbowTime = startTime;
        uint32_t lastTexTime = startTime; 

        int scroll_pos = 0;
        uint16_t currentIteration = 0;

        TextureState fgTex(fgMode, fgPal, 1.0f);
        TextureState bgTex(bgMode, bgPal, 1.0f);

        // 2. THE CONTINUOUS LOOPING ENGINE
        while (true)
        {
            // Evaluate Exit Conditions
            if (durationMs > 0 && (millis() - startTime >= durationMs)) break;
            if (iterations > 0 && currentIteration >= iterations) break;

            clearAll();

            uint32_t now = millis();
            
            // Native ShaderMode Clock Sync
            uint32_t texDelta = now - lastTexTime;
            lastTexTime = now;
            fgTex.advance(texDelta);
            bgTex.advance(texDelta);

            if (now - lastAnimTime >= 120)
            {
                currentFrame++;
                rotationPhase++;
                lastAnimTime = now;
            }
            if (now - lastRainbowTime >= 30)
            {
                globalRainbowPhase += 8;
                lastRainbowTime = now;
            }

            // Fill Internal Background Volume
            for (uint8_t i = 1; i < RNDR_X; i++)
            {
                for (uint8_t j = 1; j < RNDR_Y; j++)
                {
                    uint8_t fx_top, fy_top, fz_top;
                    uint8_t fx_bot, fy_bot, fz_bot;

                    // Dynamically map Top/Bottom planes based on the physical scroll axis
                    if (SCROLL_AXIS == 1)
                    {
                        fx_top = i; fy_top = j; fz_top = RNDR_Z - 1;
                        fx_bot = i; fy_bot = j; fz_bot = 0;
                    }
                    else if (SCROLL_AXIS == 0)
                    {
                        fx_top = RNDR_X - 1; fy_top = i; fz_top = j;
                        fx_bot = 0;          fy_bot = i; fz_bot = j;
                    }
                    else
                    {
                        fx_top = i; fy_top = RNDR_Y - 1; fz_top = j;
                        fx_bot = i; fy_bot = 0;          fz_bot = j;
                    }

                    CRGB topColor = bgTex.getColor(fx_top, fy_top, fz_top);
                    CRGB botColor = bgTex.getColor(fx_bot, fy_bot, fz_bot);

                    if (dimBackgroundPercent > 0)
                    {
                        uint8_t dimFactor = 255 - (dimBackgroundPercent * 255 / 100);
                        topColor.nscale8(dimFactor);  
                        botColor.nscale8(dimFactor);
                    }

                    setVoxel(fx_top, fy_top, fz_top, topColor);
                    setVoxel(fx_bot, fy_bot, fz_bot, botColor);
                }
            }
            
            // Draw Faces
            for (int p = 0; p < RNDR_PERIMETER; p++)
            {
                // THE DIRECTIONAL ENGINE: Flips the modulo vector for true RTL entry 
                int msg_col = rightToLeft ? (scroll_pos + p) % paddedWidth : (scroll_pos - p) % paddedWidth;
                if (msg_col < 0) msg_col += paddedWidth;

                uint8_t u, w;
                getPerimeterCoords(p, u, w);

                int current_col = 0;
                RenderItem *current_item = nullptr;
                int local_x = 0;

                if (msg_col < totalWidth)
                {
                    for (auto &item : items)
                    {
                        if (msg_col >= current_col && msg_col < current_col + item.width)
                        {
                            current_item = &item;
                            local_x = msg_col - current_col;
                            break;
                        }
                        current_col += item.width;
                    }
                }

                for (int h = 0; h < 16; h++)
                {
                    int visual_h = 15 - h;
                    uint8_t fx, fy, fz;
                    if (SCROLL_AXIS == 1) { fx = u; fy = w; fz = h; }
                    else if (SCROLL_AXIS == 0) { fx = h; fy = u; fz = w; }
                    else { fx = u; fy = h; fz = w; }

                    // 1. UNIVERSAL BACKGROUND DIMMING
                    CRGB finalColor = bgTex.getColor(fx, fy, fz);
                    if (dimBackgroundPercent > 0)
                    {
                        uint8_t dimFactor = 255 - (dimBackgroundPercent * 255 / 100);
                        finalColor.nscale8(dimFactor);
                    }

                    // 2. FOREGROUND OVERLAY
                    if (current_item)
                    {
                        // RTL MATH UN-MIRRORING: Flips internal rendering to offset physical reverse travel
                        int flipped_local_x = rightToLeft ? (current_item->width - 1 - local_x) : local_x;

                        if (current_item->isHebrew)
                        {
                            int hIndex = Hebrew::getHebrewIndex(current_item->value);
                            int actualWidth = Hebrew::hebrewWidths[hIndex];
                            
                            int h_x = flipped_local_x;
                            int h_y = visual_h;

                            // Bounds constraint ensures the 2px kerning padding remains blank
                            if (h_x < actualWidth && h_y >= 0 && h_y < 16) {
                                uint8_t pixel = pgm_read_byte(&Hebrew::hebrewIcons[hIndex][h_y * 16 + h_x]);
                                if (pixel != 0) {
                                    finalColor = fgTex.getColor(fx, fy, fz);
                                }
                            }
                        }
                        else if (current_item->isIcon)
                        {
                            int iconId = current_item->iconFrames[currentFrame % current_item->iconFrames.size()];
                            int icon_x = flipped_local_x;
                            int icon_y = visual_h;

                            if (iconId >= 1 && iconId <= 6)
                                icon_x = 15 - icon_x; // Retain standard Pacman kludge
                            if (current_item->rotate)
                                rotate16x16(rotationPhase, icon_x, icon_y);

                            uint8_t iconByte = pgm_read_byte(&icons[iconId][icon_y * 16 + icon_x]);

                            // Overwrite with Icon/Foreground color if a pixel exists
                            if (iconByte != 0)
                            {
                                if (current_item->rainbow)
                                {
                                    uint8_t phase = (iconByte * 16 + globalRainbowPhase) % 256;
                                    finalColor = CHSV(phase, 255, 255);
                                }
                                else if (overrideIconColor)
                                {
                                    finalColor = fgTex.getColor(fx, fy, fz);
                                }
                                else
                                {
                                    finalColor = getIconColor(iconByte);
                                }
                            }
                        }
                        else
                        {
                            // Standard ASCII Characters
                            char c = current_item->value;
                            if (c < 0 || c > 127) c = ' ';

                            if (visual_h >= textYOffset && visual_h < textYOffset + scaledTextHeight)
                            {
                                int font_y = (visual_h - textYOffset) / yScale;
                                int font_x = flipped_local_x / xScale;

                                font_y = constrain(font_y, 0, 7);
                                font_x = constrain(font_x, 0, 7);

                                uint8_t fontByte = readableFont[(c * 8) + font_y];

                                if (fontByte & (1 << (7 - font_x)))
                                {
                                    finalColor = fgTex.getColor(fx, fy, fz);
                                }
                            }
                        }
                    }

                    // 3. RENDER THE PIXEL
                    setVoxel(fx, fy, fz, finalColor);
                }
            }

            showCube();
            delay(scrollDelayMs); // Mechanical Tape Scroll Speed
            scroll_pos++;

            if (scroll_pos > 0 && (scroll_pos % paddedWidth == 0))
            {
                currentIteration++;
            }
            yield();
        }
    }

} // namespace Letters