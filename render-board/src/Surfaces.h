#pragma once
#include "TestDemo.h"
#include "CubeCore.h"
#include "TextureEngine.h"
#include "GeometryEngine.h"
#include "Shapes.h"
#include "Streamers.h"

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

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
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

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
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

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
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
   

  static void animateWobblingDishInternal(uint32_t durationMs, float rpm, bool intersectCenter) {
    uint32_t startTime = millis(), lastFrame = millis();

    TexturePlan plan = PaletteUtils::getRandomTexturePlan();
    TextureState tex(plan.sMode, plan.palette, 1.5f);

    float wobbleSpeed = TWO_PI * (rpm / 60.0f); 
    float ringRadius = fminf((float)RNDR_CX, (float)RNDR_CY) + 0.5f; 

    // THE TILT FIX: Strictly 30 degrees
    float maxTanCap = tanf(30.0f * PI / 180.0f); 
    float maxAllowedTan = intersectCenter ? (RNDR_CZ / ringRadius) : (RNDR_CZ / (2.0f * ringRadius));
    float actualTan = fminf(maxTanCap, maxAllowedTan);
    float tiltAngle = atanf(actualTan);

    float kissDistance = intersectCenter ? 0.0f : (2.0f * ringRadius * sinf(tiltAngle));
    
    // STATIC CONSTANTS
    float sinTilt = sinf(-tiltAngle), cosTilt = cosf(-tiltAngle);
    float tanTilt = sinTilt / cosTilt;
    
    // THE Z=1 FLOOR KISS CALCULATION (FIXED)
    // Using absolute sinf() correctly finds the true geometric lowest point of the tilted torus
    float lowestTheoreticalZ = RNDR_CZ - (kissDistance / 2.0f) - (ringRadius * sinf(tiltAngle));
    float zElevation = 1.0f - lowestTheoreticalZ; 

    // PRE-CALCULATED SQUARED BOUNDARIES
    float innerBoundSq = (ringRadius - 1.5f) * (ringRadius - 1.5f);
    float outerBoundSq = (ringRadius + 1.5f) * (ringRadius + 1.5f);

    // Master dial to reduce floor brightness (0-255). 
    // 255 = 100% of the original brightness. 127 = 50%, etc.
    uint8_t floorBrightness = 127; 

    // A cool slate-grey blue that peaks at your original brightness level of 40
    CRGB floorColor = CRGB(25, 32, 40); 
    floorColor.nscale8(floorBrightness);

    while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
        uint32_t now = millis(); 
        uint32_t deltaMs = now - lastFrame;
        if (deltaMs == 0) { yield(); continue; }
        lastFrame = now; 

        float timeSec = (now - startTime) / 1000.0f;
        float wobblePhase = timeSec * wobbleSpeed;
        
        clearAll();

        // 1. DRAW THE SLATE GREY BASE FLOOR
        for(uint8_t x = 0; x < RNDR_X; x++){
            for(uint8_t y = 0; y < RNDR_Y; y++){
                setVoxel(x, y, 0, floorColor); 
            }
        }

        float phase[2] = { wobblePhase, wobblePhase + (float)PI };
        float cosYaw[2] = { cosf(-phase[0]), cosf(-phase[1]) };
        float sinYaw[2] = { sinf(-phase[0]), sinf(-phase[1]) };
        float offset[2] = { kissDistance / 2.0f, -kissDistance / 2.0f };

        for (int i = 0; i < 2; i++) {
            float cy = cosYaw[i];
            float sy = sinYaw[i];
            float off = offset[i];

            for (uint8_t x = 0; x < RNDR_X; x++) {
                float vx = (float)x - RNDR_CX;
                for (uint8_t y = 0; y < RNDR_Y; y++) {
                    float vy = (float)y - RNDR_CY;

                    float vx1 = vx * cy - vy * sy;
                    float vy1 = vx * sy + vy * cy;

                    float lx_plane = vx1;
                    float ly_plane = vy1 / cosTilt; 
                    
                    float r2 = (lx_plane * lx_plane) + (ly_plane * ly_plane);
                    if (r2 < innerBoundSq || r2 > outerBoundSq) continue;

                    float r_3d = sqrtf(r2);
                    if (fabsf(r_3d - ringRadius) > 1.5f) continue;

                    float exact_z = RNDR_CZ + zElevation + off - vy1 * tanTilt;

                    
                    // 3. THE RING RAYCASTER
                    int z_min = max(1, (int)floorf(exact_z - 1.2f)); 
                    int z_max = min((int)RNDR_Z - 1, (int)ceilf(exact_z + 1.2f));

                    for (int z = z_min; z <= z_max; z++) {
                        float vz = (float)z - (RNDR_CZ + zElevation) - off;
                        
                        float lx = vx1;
                        float ly = vy1 * cosTilt - vz * sinTilt;
                        float lz = vy1 * sinTilt + vz * cosTilt;

                        float dist_radial = fabsf(sqrtf(lx * lx + ly * ly) - ringRadius);
                        float dist_to_surface = sqrtf(lz * lz + dist_radial * dist_radial);

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
        delay(15); 
    }
}
public:
    // WLED Polar Lights uses moving noise with a vertical falloff. On the
    // cube the light is confined to a curtain that also bends through depth.
    static void animatePolarLights(uint32_t durationMs, uint32_t stepMs = 24) {
        const CRGBPalette16 aurora(CRGB(0x291068), CRGB(0x5034C8),
                                  CRGB(0x00C4DB), CRGB(0x39FF77));
        constexpr float curtainWidth = RNDR_CY * 0.24f;
        const uint32_t started = millis();
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            const uint32_t elapsed = millis() - started;
            const float drift = TWO_PI * (elapsed % 7200) / 7200.0f;
            const float ripple = TWO_PI * (elapsed % 11300) / 11300.0f;
            const uint16_t noiseTime = uint16_t(elapsed / 8);
            clearAll();
            for (int x = 0; x < RNDR_X; ++x) {
                const float along = float(x) / (RNDR_X - 1);
                const float fold = sinf(TWO_PI * along - drift);
                const float smallFold = sinf(2.0f * TWO_PI * along + ripple);
                const float center = RNDR_CY * (1.0f + 0.52f * fold + 0.16f * smallFold);
                const float lean = 0.16f * RNDR_CY * cosf(TWO_PI * along - drift);
                for (int z = 0; z < RNDR_Z; ++z) {
                    const float height = (z - RNDR_CZ) / RNDR_CZ;
                    const float glow = max(0.0f, 1.0f - fabsf(height));
                    if (glow <= 0.0f) continue;
                    const uint8_t noise = inoise8(x * 920 / RNDR_X, z * 510 / RNDR_Z + noiseTime, noiseTime / 2);
                    const uint8_t hue = uint8_t(constrain((int(noise) - 64) * 2, 0, 255));
                    const float intensity = min(255.0f, max(0.0f, (noise - 40.0f) * 2.3f)) * glow;
                    const float ridge = center + height * lean;
                    const int first = max(0, int(ceilf(ridge - curtainWidth)));
                    const int last = min(int(RNDR_Y) - 1, int(floorf(ridge + curtainWidth)));
                    const CRGB color = ColorFromPalette(aurora, hue, 255, LINEARBLEND_NOWRAP);
                    for (int y = first; y <= last; ++y) {
                        const float coverage = max(0.0f, 1.0f - fabsf(y - ridge) / curtainWidth);
                        CRGB light = color;
                        light.nscale8(uint8_t(intensity * coverage));
                        setVoxel(x, y, z, light);
                    }
                }
            }
            showCube();
            delay(stepMs);
        }
    }

    static void animateOctopus(uint32_t durationMs, uint32_t stepMs = 16) {
        constexpr unsigned volume = unsigned(RNDR_X) * RNDR_Y * RNDR_Z;
        constexpr unsigned scaledArms = (5u * volume + 2048u) / 4096u;
        constexpr unsigned arms = scaledArms > 0 ? scaledArms : 1;
        const CRGBPalette16 palette = RainbowColors_p;
        const float span = min(RNDR_X, min(RNDR_Y, RNDR_Z)) - 1.0f;
        // Nearly face-to-face travel; signed time also gives a continuous
        // pre-start history for the trailing arms on the first frame.
        const auto hubAt = [](float t, float &x, float &y, float &z) {
            x = RNDR_CX * (1.0f + 0.98f * sinf(TWO_PI * t / 2.4f));
            y = RNDR_CY * (1.0f + 0.98f * sinf(TWO_PI * t / 3.1f + 1.1f));
            z = RNDR_CZ * (1.0f + 0.98f * sinf(TWO_PI * t / 2.7f + 2.2f));
        };
        const uint32_t started = millis();
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            const uint32_t elapsed = uint32_t(millis() - started);
            const float t = elapsed * 0.001f;
            const float spin = TWO_PI * (elapsed % 2000) / 2000.0f;
            // Smoothly alternate a coordinated sweep with independent flailing.
            const float activity = 0.5f + 0.5f * sinf(TWO_PI * t / 6.7f);
            const float chaos = activity * activity;
            float cx, cy, cz;
            hubAt(t, cx, cy, cz);
            clearAll();
            for (unsigned arm = 0; arm < arms; ++arm) {
                const float phase = TWO_PI * arm / arms;
                const float heading = spin + phase + chaos * 0.7f * sinf(3.7f*t + phase*2.0f);
                const float elevation = 0.65f * sinf(phase + t*1.3f);
                const float ux = cosf(heading) * cosf(elevation);
                const float uy = sinf(heading) * cosf(elevation);
                const float uz = sinf(elevation);
                // Near-wall arms really contract, rather than being stretched
                // across the face. Unobstructed arms can span most of the cube.
                float length = span * 0.95f;
                const auto limitAtWall = [&length](float position, float direction, float extent) {
                    if (fabsf(direction) > 0.001f) {
                        const float distance = (direction > 0 ? extent-position : -position) / direction;
                        length = min(length, max(0.0f, distance));
                    }
                };
                limitAtWall(cx, ux, RNDR_X-1.0f);
                limitAtWall(cy, uy, RNDR_Y-1.0f);
                limitAtWall(cz, uz, RNDR_Z-1.0f);
                float px = cx, py = cy, pz = cz;
                constexpr int segments = 24;
                for (int sample = 1; sample <= segments; ++sample) {
                    const float u = float(sample) / segments;
                    const float bend = u*u;
                    float hx, hy, hz;
                    hubAt(t - (0.18f + 0.30f*chaos)*u, hx, hy, hz);
                    // Different delays and traveling bends let tips flop while
                    // roots remain attached. Short arms also lose their bend.
                    const float curl = length * bend * (0.12f + 0.38f*chaos)
                        * sinf(6.0f*u - (4.0f + arm*0.37f)*t + phase);
                    const float lag = bend * min(1.0f, length / (span*0.4f));
                    const float x = cx + ux*length*u - sinf(heading)*curl + (hx-cx)*lag;
                    const float y = cy + uy*length*u + cosf(heading)*curl + (hy-cy)*lag;
                    const float z = cz + uz*length*u + curl*sinf(phase+t*2.1f) + (hz-cz)*lag;
                    const CRGB color = ColorFromPalette(palette,
                        uint8_t(elapsed/10 - int(255.0f*u)), 255, LINEARBLEND);
                    // Shared distance-based antialiasing; max blending keeps
                    // adjoining segments from accumulating into white hotspots.
                    const float radius = span * (0.018f + 0.035f*sinf(PI*u));
                    GeometryEngine::drawSdfLine3D(px, py, pz, x, y, z, radius, color, false);
                    px=x; py=y; pz=z;
                }
            }
            showCube();
            delay(stepMs);
        }
    }


    // Shared with Audio::animateJulia; normal animation retains timed c motion.
    static void animateJulia(uint32_t durationMs, uint32_t speedMs = 40,
                             bool cycleColors = true, bool varyC = true) {
        JuliaRenderer::animateJulia(durationMs, speedMs, cycleColors, varyC);
    }

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
    static void animateRockingSineWave(uint32_t durationMs, float waveSpeed) {
        constexpr float rockPeriodMs = 8000.0f;
        constexpr float maxTilt = 25.0f * PI / 180.0f;
        const float extent = min(RNDR_X, RNDR_Y);
        const float angleScale = 65536.0f / extent;
        // Leave vertical room for the lifted edge of the rocking sheet.
        const float amplitude = min(RNDR_CZ - 0.5f, extent * 0.28f);
        TextureState tex(MODE_LINEAR_FLOW, RainbowColors_p, 4.0f);
        const uint32_t start = millis();
        uint32_t lastFrame = start;
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - start), durationMs)) {
            const uint32_t now = millis();
            const uint32_t elapsed = now - start;
            tex.advance(now - lastFrame);
            lastFrame = now;
            const uint16_t phase = uint16_t(fmodf(waveSpeed * elapsed, 360.0f) * (65536.0f / 360.0f));
            const float angle = maxTilt * sinf(TWO_PI * elapsed / rockPeriodMs);
            const float sine = sinf(angle), cosine = cosf(angle);
            clearAll();
            for (int x = 0; x < RNDR_X; ++x) {
                const float u = x - RNDR_CX;
                for (int y = 0; y < RNDR_Y; ++y) {
                    const float v = y - RNDR_CY;
                    const float distance = sqrtf(u * u + v * v);
                    const float height = GeometryEngine::sineWaveHeight(distance, angleScale, phase, amplitude);
                    const float wy = RNDR_CY + v * cosine - height * sine;
                    const float wz = RNDR_CZ + v * sine + height * cosine;
                    const CRGB color = tex.getColor(x, constrain(int(lroundf(wy)), 0, RNDR_Y - 1),
                                                    constrain(int(lroundf(wz)), 0, RNDR_Z - 1));
                    GeometryEngine::drawWuVoxel(x * 256, lroundf(wy * 256), lroundf(wz * 256), color);
                }
            }
            showCube();
            yield();
        }
    }

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
