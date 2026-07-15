#pragma once
#include "CubeCore.h"
#include "TextureEngine.h"
#include "GeometryEngine.h"
#include "Shapes.h"

class Surfaces {
private:
    // ==========================================
    // INTERNAL DIRECTORS
    // ==========================================

    static void animateSineWaveInternal(uint32_t durationMs, Orientation axis, WaveShape shape, int period, ShaderMode sMode, CRGBPalette16 pal, float waveSpeed) {
        uint32_t startTime = millis(), lastFrame = millis();
        float geomPhase = 0.0f;

        // 1. The True Amplitude & Origin Fix
        float amp, vShiftCenter, uCenter, vCenter;
        
        if (axis == AXIS_Z) {
            amp = (RNDR_Z / 2.0f) - 1.0f; vShiftCenter = RNDR_Z / 2.0f;
            uCenter = RNDR_CX; vCenter = RNDR_CY;
        } else if (axis == AXIS_Y) {
            amp = (RNDR_Y / 2.0f) - 1.0f; vShiftCenter = RNDR_Y / 2.0f;
            uCenter = RNDR_CX; vCenter = RNDR_CZ;
        } else { // AXIS_X
            amp = (RNDR_X / 2.0f) - 1.0f; vShiftCenter = RNDR_X / 2.0f;
            uCenter = RNDR_CY; vCenter = RNDR_CZ;
        }

        TextureState tex(sMode, pal, 4.0f); // Fast 4.0f Multiplier for Waves

        while (millis() - startTime < durationMs) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now;
            
            // Animate the ripple using the passed speed
            geomPhase += waveSpeed * deltaMs;
            int phaseShift = (int)geomPhase % 360;
            
            clearAll();
            
            // Lock the origin (-uCenter, -vCenter) precisely to the middle of the room
            GeometryEngine::drawSineWave(amp, period, phaseShift, vShiftCenter - 0.5f, -uCenter, -vCenter, axis, shape, tex);
            
            showCube(); 
            tex.advance(deltaMs); 
            yield();
        }
    }

   static void animateHyperbolicParaboloidInternal(uint32_t durationMs, float rotXSpeed, float rotYSpeed, float rotZSpeed, ShaderMode sMode, CRGBPalette16 pal, uint32_t speedMs = 0) {
        uint32_t startTime = millis(), lastFrame = millis();
        int offSet = 0, offsetSign = 1; float rx = 0, ry = 0, rz = 0;
        
        TextureState tex(sMode, pal, 1.66f); // Ideal speed for the Saddle math

        while (millis() - startTime < durationMs) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now;
            clearAll();
            rx += rotXSpeed; ry += rotYSpeed; rz += rotZSpeed;
            if (rx >= 360.0f) rx -= 360.0f; if (rx < 0.0f) rx += 360.0f;
            if (ry >= 360.0f) ry -= 360.0f; if (ry < 0.0f) ry += 360.0f;
            if (rz >= 360.0f) rz -= 360.0f; if (rz < 0.0f) rz += 360.0f;

            GeometryEngine::drawHyperbolicParaboloid(offSet, offsetSign, rx, ry, rz, tex);
            offSet += 2; if (offSet > 20) { offSet = 0; offsetSign = -offsetSign; }
            showCube(); tex.advance(deltaMs); if (speedMs > 0) delay(speedMs); else yield();
        }
    }

#ifdef TARGET_2026_CUBOID
    static void animateSineWaveSplit2026Internal(uint32_t durationMs, int period, SplitWaveMode2026 mode, ShaderMode sMode, CRGBPalette16 pal, float waveSpeed = 0.8f) {
        uint32_t startTime = millis(), lastFrame = millis();
        float geomPhase = 0.0f, amplitude = 3.5f; 
        uint8_t aBot = 2, aTop = 2; bool invBot = false, invTop = false, outOfPhase = false;

        if (mode == SPLIT_2026_MIRRORED_Z) { aBot = 2; aTop = 2; invTop = true; } 
        else if (mode == SPLIT_2026_OPPOSING_X) { aBot = 0; aTop = 0; outOfPhase = true; } 
        else if (mode == SPLIT_2026_OPPOSING_Y) { aBot = 1; aTop = 1; outOfPhase = true; } 
        else if (mode == SPLIT_2026_RANDOM) { aBot = random8(3); aTop = random8(3); invBot = (random8(2) == 1); invTop = (random8(2) == 1); outOfPhase = (random8(2) == 1); }

        TextureState tex(sMode, pal, 4.0f);

        while (millis() - startTime < durationMs) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now;
            geomPhase += waveSpeed * deltaMs; clearAll();
            int phaseShiftDegrees = (int)geomPhase % 360;
            uint16_t phaseBot = (uint16_t)(((uint32_t)phaseShiftDegrees * 65536) / 360);
            uint16_t phaseTop = outOfPhase ? (phaseBot + 32768) : phaseBot;
            GeometryEngine::drawSineWaveSplit2026(amplitude, period, phaseBot, phaseTop, aBot, aTop, invBot, invTop, tex);
            showCube(); tex.advance(deltaMs); yield();
        }
    }
#endif
// ==========================================
    // THE PHANTOM ZONE RINGS (Kissing Wobble)
    // ==========================================
    static void animateWobblingDishInternal(uint32_t durationMs, float rpm, bool intersectCenter) {
        uint32_t startTime = millis(), lastFrame = millis();

        // SINGLE UNIFIED TEXTURE: Enforces that both rings share the exact same 3D color space.
        // This makes the "kissing" point perfectly seamless, hiding the geometry intersection.
        TexturePlan plan = PaletteUtils::getRandomTexturePlan();
        TextureState tex(plan.sMode, plan.palette, 1.5f);

        float wobbleSpeed = TWO_PI * (rpm / 60.0f); 

        // TRUE 3D RADIUS: explicitly pad the native radius to 4.0 
        float ringRadius = fminf((float)RNDR_CX, (float)RNDR_CY) + 0.5f; 

        // THE TILT FIX: Lock the tilt to a maximum of 45 degrees. 
        float maxTanCap = tanf(45.0f * PI / 180.0f); 
        float maxAllowedTan = intersectCenter ? (RNDR_CZ / ringRadius) : (RNDR_CZ / (2.0f * ringRadius));
        float actualTan = fminf(maxTanCap, maxAllowedTan);
        float tiltAngle = atanf(actualTan);

        // Z-separation so rims perfectly kiss
        float kissDistance = intersectCenter ? 0.0f : (2.0f * ringRadius * sinf(tiltAngle));

        while (millis() - startTime < durationMs) {
            uint32_t now = millis(); 
            uint32_t deltaMs = now - lastFrame;
            if (deltaMs == 0) { yield(); continue; }
            lastFrame = now; 

            float timeSec = (now - startTime) / 1000.0f;
            float wobblePhase = timeSec * wobbleSpeed;
            
            clearAll();

            float sinTilt = sinf(-tiltAngle), cosTilt = cosf(-tiltAngle);
            float tanTilt = sinTilt / cosTilt;

            for (uint8_t x = 0; x < RNDR_X; x++) {
                float vx = (float)x - RNDR_CX;
                for (uint8_t y = 0; y < RNDR_Y; y++) {
                    float vy = (float)y - RNDR_CY;

                    for (int i = 0; i < 2; i++) {
                        float offset_i = (i == 0) ? (kissDistance / 2.0f) : (-kissDistance / 2.0f);
                        float phase_i = wobblePhase + ((float)i * PI);

                        // True 3D Matrix Rotation Math
                        float cosYaw = cosf(-phase_i), sinYaw = sinf(-phase_i);
                        float vx1 = vx * cosYaw - vy * sinYaw;
                        float vy1 = vx * sinYaw + vy * cosYaw;

                        // 1. FAST CULLING: Calculate 3D footprint on the mathematical plane
                        float lx_plane = vx1;
                        float ly_plane = vy1 / cosTilt; 
                        float r_3d = sqrtf(lx_plane * lx_plane + ly_plane * ly_plane);

                        // Strict 3D radius cutoff (Cull BOTH the outside and the inside!)
                        // This leaves only a hollow ring of ~1.5 thickness for the raycaster
                        if (fabsf(r_3d - ringRadius) > 1.5f) continue;

                        // 2. RAYCAST Z: Algebraically find exact height of mathematical plane
                        float exact_z = RNDR_CZ + offset_i - vy1 * tanTilt;

                        // 3. RENDER: Only check the 2 or 3 integer Z voxels intersecting the plane
                        int z_min = max(0, (int)floorf(exact_z - 1.2f));
                        int z_max = min((int)RNDR_Z - 1, (int)ceilf(exact_z + 1.2f));

                        for (int z = z_min; z <= z_max; z++) {
                            float vz = (float)z - RNDR_CZ - offset_i;
                            
                            float lx = vx1;
                            float ly = vy1 * cosTilt - vz * sinTilt;
                            float lz = vy1 * sinTilt + vz * cosTilt;

                            // True 3D HOLLOW TORUS SDF 
                            // Replaced fmaxf (solid disk) with fabsf (hollow hoop)
                            float dist_radial = fabsf(sqrtf(lx * lx + ly * ly) - ringRadius);
                            float dist_to_surface = sqrtf(lz * lz + dist_radial * dist_radial);

                            // BRUTAL BINARY SNAP (0.8f guarantees unbroken 1-2 pixel thickness across slopes)
                            if (dist_to_surface <= 0.8f) {
                                CRGB c = tex.getColor(x, y, z);
                                CRGB existing = getVoxel(x, y, z);
                                setVoxel(x, y, z, existing + c);
                            }
                        }
                    }
                }
            }
            
            tex.advance(deltaMs);
            showCube();
            yield();
        }
    }
public:
    // ==========================================
    // PURE OVERLOADS (Intent Inference)
    // ==========================================
    
    // --- Hyperbolic Paraboloid ---
    static void animateHyperbolicParaboloid(uint32_t durationMs, float rotX, float rotY, float rotZ, CRGBPalette16 pal, ShaderMode sMode, uint32_t speedMs = 0) { animateHyperbolicParaboloidInternal(durationMs, rotX, rotY, rotZ, sMode, pal, speedMs); }
    static void animateHyperbolicParaboloid(uint32_t durationMs, float rotX, float rotY, float rotZ, CRGB::HTMLColorCode colorCode, uint32_t speedMs = 0) { animateHyperbolicParaboloidInternal(durationMs, rotX, rotY, rotZ, MODE_SOLID, CRGBPalette16(CRGB(colorCode)), speedMs); }
    static void animateHyperbolicParaboloid(uint32_t durationMs, float rotX, float rotY, float rotZ, CRGB color, uint32_t speedMs = 0) { animateHyperbolicParaboloidInternal(durationMs, rotX, rotY, rotZ, MODE_SOLID, CRGBPalette16(color), speedMs); }
    static void animateHyperbolicParaboloid(uint32_t durationMs, float rotX, float rotY, float rotZ, ShaderMode sMode, uint32_t speedMs = 0) { animateHyperbolicParaboloidInternal(durationMs, rotX, rotY, rotZ, sMode, PaletteUtils::getSmartPalette(sMode), speedMs); }
    
    static void animateHyperbolicParaboloid(uint32_t durationMs, float step, Orientation axis, CRGBPalette16 pal, ShaderMode sMode, uint32_t speedMs = 0) { float sx = (axis == AXIS_X) ? step : 0.0f; float sy = (axis == AXIS_Y) ? step : 0.0f; float sz = (axis == AXIS_Z) ? step : 0.0f; animateHyperbolicParaboloidInternal(durationMs, sx, sy, sz, sMode, pal, speedMs); }
    static void animateHyperbolicParaboloid(uint32_t durationMs, float step, Orientation axis, CRGB::HTMLColorCode colorCode, uint32_t speedMs = 0) { float sx = (axis == AXIS_X) ? step : 0.0f; float sy = (axis == AXIS_Y) ? step : 0.0f; float sz = (axis == AXIS_Z) ? step : 0.0f; animateHyperbolicParaboloidInternal(durationMs, sx, sy, sz, MODE_SOLID, CRGBPalette16(CRGB(colorCode)), speedMs); }
    static void animateHyperbolicParaboloid(uint32_t durationMs, float step, Orientation axis, CRGB color, uint32_t speedMs = 0) { float sx = (axis == AXIS_X) ? step : 0.0f; float sy = (axis == AXIS_Y) ? step : 0.0f; float sz = (axis == AXIS_Z) ? step : 0.0f; animateHyperbolicParaboloidInternal(durationMs, sx, sy, sz, MODE_SOLID, CRGBPalette16(color), speedMs); }
    static void animateHyperbolicParaboloid(uint32_t durationMs, float step, Orientation axis, ShaderMode sMode, uint32_t speedMs = 0) { float sx = (axis == AXIS_X) ? step : 0.0f; float sy = (axis == AXIS_Y) ? step : 0.0f; float sz = (axis == AXIS_Z) ? step : 0.0f; animateHyperbolicParaboloidInternal(durationMs, sx, sy, sz, sMode, PaletteUtils::getSmartPalette(sMode), speedMs); }

    // --- Sine Waves ---
    static void animateSineWave(uint32_t durationMs, Orientation axis, WaveShape shape, int period, CRGBPalette16 pal, ShaderMode sMode, float waveSpeed = 0.8f) { animateSineWaveInternal(durationMs, axis, shape, period, sMode, pal, waveSpeed); }
    static void animateSineWave(uint32_t durationMs, Orientation axis, WaveShape shape, int period, CRGB::HTMLColorCode colorCode, float waveSpeed = 0.8f) { animateSineWaveInternal(durationMs, axis, shape, period, MODE_SOLID, CRGBPalette16(CRGB(colorCode)), waveSpeed); }
    static void animateSineWave(uint32_t durationMs, Orientation axis, WaveShape shape, int period, CRGB color, float waveSpeed = 0.8f) { animateSineWaveInternal(durationMs, axis, shape, period, MODE_SOLID, CRGBPalette16(color), waveSpeed); }
    static void animateSineWave(uint32_t durationMs, Orientation axis, WaveShape shape, int period, ShaderMode sMode, float waveSpeed = 0.8f) { animateSineWaveInternal(durationMs, axis, shape, period, sMode, PaletteUtils::getSmartPalette(sMode), waveSpeed); }

#ifdef TARGET_2026_CUBOID
    // --- Sine Wave Split (2026 Engine) ---
    static void animateSineWaveSplit2026(uint32_t durationMs, int period, SplitWaveMode2026 mode, CRGBPalette16 pal, ShaderMode sMode, float waveSpeed = 0.8f) { animateSineWaveSplit2026Internal(durationMs, period, mode, sMode, pal, waveSpeed); }
    static void animateSineWaveSplit2026(uint32_t durationMs, int period, SplitWaveMode2026 mode, CRGB::HTMLColorCode colorCode, float waveSpeed = 0.8f) { animateSineWaveSplit2026Internal(durationMs, period, mode, MODE_SOLID, CRGBPalette16(CRGB(colorCode)), waveSpeed); }
    static void animateSineWaveSplit2026(uint32_t durationMs, int period, SplitWaveMode2026 mode, CRGB color, float waveSpeed = 0.8f) { animateSineWaveSplit2026Internal(durationMs, period, mode, MODE_SOLID, CRGBPalette16(color), waveSpeed); }
    static void animateSineWaveSplit2026(uint32_t durationMs, int period, SplitWaveMode2026 mode, ShaderMode sMode, float waveSpeed = 0.8f) { animateSineWaveSplit2026Internal(durationMs, period, mode, sMode, PaletteUtils::getSmartPalette(sMode), waveSpeed); }
#endif

 // Callers for Wobbling Dish
    static void animateWobblingDish(uint32_t durationMs, bool intersectCenter = false, float rpm = 45.0f) {  animateWobblingDishInternal(durationMs, rpm, intersectCenter);}
    
};