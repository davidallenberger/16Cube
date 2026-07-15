#pragma once
#include "CubeCore.h"

// ==========================================
// V2 ARCHITECTURE (THE BRUSHES)
// ==========================================
enum ShaderMode {
    MODE_SOLID,           // Explicitly ignores time and space
    MODE_UNIFORM_CYCLE,   // Solid color, shifts over time - moving texture
    MODE_SPATIAL_GRADIENT,// Frozen 3D heatmap 
    MODE_LINEAR_FLOW,     // 3D waves of color - moving texture it cycles whatever is spread across the field
    MODE_NOISE_FIELD,     // 3D Perlin Clouds (Blends)
    MODE_SCATTER,         // Digital Hash Glitter (Random)
    MODE_DISTANCE_FIELD,  // 3D Lava Lamp Cores (Metaballs) - moving texture
    MODE_HIPHOTIC,         // Self-contained generative math - moving texture: Use Hiphotic when you want a smooth, highly structured, full-color mathematical gradient washing across a shape. 
    MODE_DISTORTION_WAVES // 3D Euclidean Interference Patterns: Use Distortion Waves when you want an organic, liquid-like, palette-specific texture where expanding rings collide and warp against each other.
};

// ==========================================
// UNIFIED PALETTE UTILITIES
// ==========================================
// Add this simple struct right above or inside PaletteUtils
struct TexturePlan { 
    ShaderMode sMode; 
    CRGBPalette16 palette; 
};
class PaletteUtils {
public:
    // Header-safe static buffer for debugging
    static char* getPaletteNameBuffer() {
        static char buffer[64] = "Unknown";
        return buffer;
    }

    // Translates 8-bit FastLED math into plain English
    static const char* getHueName(uint8_t hue) {
        if (hue < 11) return "Red";
        if (hue < 32) return "Orange";
        if (hue < 54) return "Yellow";
        if (hue < 85) return "Green";
        if (hue < 128) return "Aqua/Cyan";
        if (hue < 170) return "Blue";
        if (hue < 213) return "Purple";
        if (hue < 245) return "Pink/Magenta";
        return "Red";
    }

    static CRGBPalette16 getRandomOrganicPalette() {
        uint8_t p = random8(4);
        if      (p == 0) { strcpy(getPaletteNameBuffer(), "Ocean (Blues/Greens)"); return OceanColors_p; }
        else if (p == 1) { strcpy(getPaletteNameBuffer(), "Lava (Reds/Oranges)"); return LavaColors_p; }
        else if (p == 2) { strcpy(getPaletteNameBuffer(), "Forest (Greens/Yellows)"); return ForestColors_p; }
        else             { strcpy(getPaletteNameBuffer(), "Party (Neon Mix)"); return PartyColors_p; }
    }

    static CRGB getRandomVibrantColor() {
        // Force max saturation and value to guarantee a bright, pretty neon color
        uint8_t hue = random8();
        sprintf(getPaletteNameBuffer(), "Solid %s (Hue: %d)", getHueName(hue), hue);
        return CHSV(hue, 255, 255); 
    }

    static CRGBPalette16 getSmartPalette(ShaderMode sMode) {
        if (sMode == MODE_SOLID || sMode == MODE_SPATIAL_GRADIENT) {
            return CRGBPalette16(getRandomVibrantColor());
        }
        if (sMode == MODE_NOISE_FIELD || sMode == MODE_LINEAR_FLOW || sMode == MODE_DISTANCE_FIELD) {
            return getRandomOrganicPalette();
        }
        strcpy(getPaletteNameBuffer(), "Party (Neon Mix)");
        return PartyColors_p; 
    }
   
    static TexturePlan getRandomNonSolidTexturePlan() {
        TexturePlan plan;
        uint8_t r = random8(100);
        
        if (r < 50) { 
            // 50% Chance: Mathematical / Flowing Gradients
            uint8_t m = random8(3); 
            plan.sMode = (m == 0) ? MODE_SPATIAL_GRADIENT : ((m == 1) ? MODE_LINEAR_FLOW : MODE_HIPHOTIC); 
            plan.palette = PartyColors_p; 
        } 
        else { 
            // 50% Chance: High-Texture Noise & Scatter
            if (random8(2) == 0) { 
                plan.sMode = MODE_NOISE_FIELD; 
                plan.palette = PaletteUtils::getRandomOrganicPalette(); 
            } else { 
                plan.sMode = MODE_SCATTER; 
                plan.palette = PartyColors_p;
            }
        }
        return plan;
    }

    static CRGBPalette16 getDynamicContrastPalette(CRGB sentinel) {
        CRGBPalette16 pal;
        pal[0] = sentinel; // Keep the sentinel at Index 0 so GeometryEngine knows it's a hack

        if (sentinel == CRGB(CRGB::White)) {
            // The Merkaba 4-Color Symmetry
            pal[1] = CRGB::Red;
            pal[2] = CRGB::Green;
            pal[3] = CRGB::Blue;
            pal[4] = CRGB::DarkViolet;
            strcpy(getPaletteNameBuffer(), "Solid White (Merkaba Hack)");
            return pal;
        } 
        
        if (sentinel == CRGB(CRGB::Gray)) {
            // The Dynamic 2-Color (or 3-Color) Contrast Engine
            uint8_t r = random8(5);
            CRGB c1, c2, c3;
            
            switch(r) {
                case 0: c1 = CRGB::Cyan; c2 = CRGB::DeepPink; c3 = CRGB::Purple; strcpy(getPaletteNameBuffer(), "Cyberpunk Split"); break;
                case 1: c1 = CRGB::Gold; c2 = CRGB::Blue; c3 = CRGB::DarkRed; strcpy(getPaletteNameBuffer(), "Pharaoh Split"); break;
                case 2: c1 = CRGB::Red; c2 = CRGB::Blue; c3 = CRGB::Orange; strcpy(getPaletteNameBuffer(), "Toxic Split"); break;
                case 3: c1 = CRGB::Red; c2 = CRGB::Green; c3 = CRGB::Gold; strcpy(getPaletteNameBuffer(), "Fire & Ice Split"); break;
                case 4: c1 = CRGB::Green; c2 = CRGB::Purple; c3 = CRGB::OrangeRed; strcpy(getPaletteNameBuffer(), "Villain Split"); break;
            }
            
            pal[1] = c1; // Primary Component
            pal[2] = c2; // Secondary Component
            pal[3] = c3; // Accent Component (for Hemispheres)
            return pal;
        }
        
        return CRGBPalette16(sentinel);
    }
    
    static TexturePlan getRandomTexturePlan() {
        TexturePlan plan;
        uint8_t r = random8(99);
        if (r < 33) { 
            plan.sMode = MODE_SOLID; 
            plan.palette = CRGBPalette16(CHSV(random8(), 255, 255)); 
        } 
        else if (r < 66) { 
            uint8_t m = random8(3); 
            plan.sMode = (m == 0) ? MODE_SPATIAL_GRADIENT : ((m == 1) ? MODE_LINEAR_FLOW : MODE_HIPHOTIC); 
            plan.palette = PartyColors_p; 
        } 
        else { 
            if (random8(2) == 0) { 
                plan.sMode = MODE_NOISE_FIELD; 
                plan.palette = PaletteUtils::getRandomOrganicPalette(); 
            } else { 
                plan.sMode = MODE_SCATTER; 
                plan.palette = PartyColors_p;
            }
        }
        return plan;
    }
};

struct TextureState {
    ShaderMode mode;      // The Unified Engine Mode
    
    uint32_t phase;
    float internalPhase;  // Precise Time Tracking
    float speedMultiplier;// Speed Scaling
    uint8_t scale; 
    CRGBPalette16 palette; 
    bool drawBalls; 
    
    uint8_t hX[16], hY[16], hZ[16];
    float mbX[3], mbY[3], mbZ[3];

    // --- ADD THE DISTORTION WAVE CACHE ---
    uint32_t dwA, dwA2, dwA3;
    int32_t dwX[3], dwY[3], dwZ[3];

    // ------------------------------------------
    // THE PAINT + BRUSH CONSTRUCTOR
    // ------------------------------------------
    /*TextureState(ShaderMode sm, CRGBPalette16 pal = PartyColors_p, float speedMult = 1.0f, uint8_t s = 128, bool db = true) 
        : mode(sm), phase(0), internalPhase(0.0f), speedMultiplier(speedMult), scale(s), palette(pal), drawBalls(db) {    
        initMath();
    }*/
   // ------------------------------------------
    // THE PAINT + BRUSH CONSTRUCTOR
    // ------------------------------------------
    TextureState(ShaderMode sm = MODE_SOLID, CRGBPalette16 pal = PartyColors_p, float speedMult = 1.0f, uint8_t s = (RNDR_X > 8 ? 64 : 128), bool db = true)
    //TextureState(ShaderMode sm, CRGBPalette16 pal = PartyColors_p, float speedMult = 1.0f, uint8_t s = (RNDR_X > 8 ? 64 : 128), bool db = true) 
        : mode(sm), phase(0), internalPhase(0.0f), speedMultiplier(speedMult), scale(s), palette(pal), drawBalls(db) {    
        initMath();
    }

    // ------------------------------------------
    // CLOCK ENGINE
    // ------------------------------------------
    void advance(uint32_t deltaMs) {
        // Base speed: 0.2 phase units per millisecond (~3 phase per 15ms)
        float basePhasePerMs = 0.2f; 
        
        // Natural Gearing: Metaballs look best when they run slightly faster
        if (mode == MODE_DISTANCE_FIELD) {
            basePhasePerMs = 0.4f; 
        }

        internalPhase += basePhasePerMs * speedMultiplier * (float)deltaMs;
        phase = (uint32_t)internalPhase;
        
        initMath(); // Re-calculate dynamic fields (Metaballs, Hiphotic)
    }

    void initMath() {
        if (mode == MODE_HIPHOTIC) {
            uint32_t slowPhase = phase / 2; 
            uint8_t p3 = slowPhase / 3;
            uint8_t p4 = slowPhase / 4;
            uint8_t p5 = slowPhase / 5;
            for (int i = 0; i < 16; i++) {
                hX[i] = cos8((i * scale >> 4) + p3);
                hY[i] = sin8((i * scale >> 4) + p4);
                hZ[i] = cos8((i * scale >> 4) + p5);
            }
        }
        else if (mode == MODE_DISTORTION_WAVES) {
            // 1. Tame the time variable (WLED used millis/32, phase moves much faster)
            dwA = phase / 8;
            dwA2 = dwA / 2;
            dwA3 = dwA / 3;

            // 2. Tame the scale (TextureState defaults to 128, WLED expects 1-8)
            uint8_t dwScale = max((int)1, (int)(scale / 16));
            int32_t xMax = RNDR_X * dwScale;
            int32_t yMax = RNDR_Y * dwScale;
            int32_t zMax = RNDR_Z * dwScale;

            // 3. Orbiting centers (Multiplying phase gives graceful, independent sweep speeds)
            dwX[0] = ((uint32_t)(sin16((uint16_t)(phase * 15)) + 32768) * xMax) >> 16;
            dwY[0] = ((uint32_t)(sin16((uint16_t)(phase * 18)) + 32768) * yMax) >> 16;
            dwZ[0] = ((uint32_t)(sin16((uint16_t)(phase * 16)) + 32768) * zMax) >> 16;

            dwX[1] = ((uint32_t)(sin16((uint16_t)(phase * 19)) + 32768) * xMax) >> 16;
            dwY[1] = ((uint32_t)(sin16((uint16_t)(phase * 22)) + 32768) * yMax) >> 16;
            dwZ[1] = ((uint32_t)(sin16((uint16_t)(phase * 21)) + 32768) * zMax) >> 16;

            dwX[2] = ((uint32_t)(sin16((uint16_t)(phase * 25)) + 32768) * xMax) >> 16;
            dwY[2] = ((uint32_t)(sin16((uint16_t)(phase * 21)) + 32768) * yMax) >> 16;
            dwZ[2] = ((uint32_t)(sin16((uint16_t)(phase * 24)) + 32768) * zMax) >> 16;
        }
        else if (mode == MODE_DISTANCE_FIELD) {
            float cx = RNDR_CX, cy = RNDR_CY, cz = RNDR_CZ;
            float rx = (RNDR_X > 4) ? (RNDR_X / 2.0f) - 1.0f : 1.0f;
            float ry = (RNDR_Y > 4) ? (RNDR_Y / 2.0f) - 1.0f : 1.0f;
            float rz = (RNDR_Z > 4) ? (RNDR_Z / 2.0f) - 1.0f : 1.0f;

            mbX[0] = cx + (sin16((uint16_t)(phase * 11)) / 32768.0f) * rx;
            mbY[0] = cy + (cos16((uint16_t)(phase * 13)) / 32768.0f) * ry;
            mbZ[0] = cz + (sin16((uint16_t)(phase * 17)) / 32768.0f) * rz;

            mbX[1] = cx + (cos16((uint16_t)(phase * 19)) / 32768.0f) * rx;
            mbY[1] = cy + (sin16((uint16_t)(phase * 23)) / 32768.0f) * ry;
            mbZ[1] = cz + (cos16((uint16_t)(phase * 29)) / 32768.0f) * rz;

            mbX[2] = cx + (sin16((uint16_t)(phase * 31)) / 32768.0f) * rx;
            mbY[2] = cy + (cos16((uint16_t)(phase * 37)) / 32768.0f) * ry;
            mbZ[2] = cz + (sin16((uint16_t)(phase * 41)) / 32768.0f) * rz;
        }
    }

    inline CRGB getColor(uint8_t x, uint8_t y, uint8_t z) const {
        if (mode == MODE_SOLID)  return palette[0];
        
        if (mode == MODE_HIPHOTIC) { 
            uint8_t math_val = sin8(hX[x] + hY[y] + hZ[z] + ((phase * 3) / 4));
            return CHSV(math_val, 255, 255);
        }
        if (mode == MODE_UNIFORM_CYCLE) return ColorFromPalette(palette, phase / 2, 255, LINEARBLEND);
        if (mode == MODE_SPATIAL_GRADIENT) return ColorFromPalette(palette, (x * 11 + y * 7 + z * 13), 255, LINEARBLEND);
        if (mode == MODE_LINEAR_FLOW) return ColorFromPalette(palette, (x * 11 + y * 7 + z * 13) + phase, 255, LINEARBLEND);
        
        if (mode == MODE_NOISE_FIELD) {
            uint8_t rawNoise = inoise8(x * scale >> 2, y * scale >> 2, (z * scale >> 2) + phase);
            uint8_t colorIndex = (rawNoise * 2) - phase; 
            return ColorFromPalette(palette, colorIndex, 255, LINEARBLEND);
        }
        if (mode == MODE_SCATTER) return ColorFromPalette(palette, (x * 71 + y * 31 + z * 113 + phase), 255, LINEARBLEND);
        
        if (mode == MODE_DISTANCE_FIELD) {
            float sum = 0.0f;
            bool isCore = false;
            for(int i = 0; i < 3; i++) {
                float distSq = ((float)x - mbX[i]) * ((float)x - mbX[i]) + ((float)y - mbY[i]) * ((float)y - mbY[i]) + ((float)z - mbZ[i]) * ((float)z - mbZ[i]);
                if (distSq < 0.1f) distSq = 0.1f; 
                //sum += 12.0f / distSq; 
                // 8x8 gets a mass of 12. 16x16 gets a massive aura of 48.
                float auraMass = (RNDR_X > 8) ? 48.0f : 12.0f;
                sum += auraMass / distSq;
                if (drawBalls && distSq <= 1.0f) isCore = true;
            }
            if (isCore) return CRGB::White;
            return ColorFromPalette(palette, min(255, (int)(sum * 64)) + (phase / 2), 255, LINEARBLEND);
        }
        if (mode == MODE_DISTORTION_WAVES) {
            // Apply the same taming scale to the voxel coordinates
            uint8_t dwScale = max((int)1, (int)(scale / 16));
            int32_t sx = (x + 1) * dwScale;
            int32_t sy = (y + 1) * dwScale;
            int32_t sz = (z + 1) * dwScale;

            // 1. Warped Cosine Grid Math (3D)
            uint8_t rdistort = cos8((cos8(((x<<3)+dwA)&255) + cos8(((y<<3)-dwA2)&255) + cos8(((z<<3)+dwA3)&255)) & 255) >> 1;
            uint8_t gdistort = cos8((cos8(((x<<3)-dwA2)&255) + cos8(((y<<3)+dwA3)&255) + cos8(((z<<3)+dwA+32)&255)) & 255) >> 1;
            uint8_t bdistort = cos8((cos8(((x<<3)+dwA3)&255) + cos8(((y<<3)-dwA)&255)  + cos8(((z<<3)+dwA2+64)&255)) & 255) >> 1;

            // 2. Interference Distance Math
            int32_t dx = sx - dwX[0], dy = sy - dwY[0], dz = sz - dwZ[0];
            uint8_t valueR = cos8(rdistort + ((dwA - ((dx*dx + dy*dy + dz*dz) >> 7)) << 1));

            int32_t dx1 = sx - dwX[1], dy1 = sy - dwY[1], dz1 = sz - dwZ[1];
            uint8_t valueG = cos8(gdistort + ((dwA2 - ((dx1*dx1 + dy1*dy1 + dz1*dz1) >> 7)) << 1));

            int32_t dx2 = sx - dwX[2], dy2 = sy - dwY[2], dz2 = sz - dwZ[2];
            uint8_t valueB = cos8(bdistort + ((dwA3 - ((dx2*dx2 + dy2*dy2 + dz2*dz2) >> 7)) << 1));

            // Map combined intensity to the currently active palette
            uint8_t brightness = (valueR + valueG + valueB) / 3;
            return ColorFromPalette(palette, brightness, 255, LINEARBLEND);
        }

        return CRGB::Black;
    }
};