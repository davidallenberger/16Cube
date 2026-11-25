#pragma once
#include "TestDemo.h"
#include "CubeCore.h"
#include "TextureEngine.h"
#include "GeometryEngine.h"
#include "Shapes.h"
#include "JuliaRenderer.h"

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

    TexturePlan plan = PaletteUtils::getRandomTexturePlan(true);
    PaletteUtils::logTexture("Wobbling Dish", plan);
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
    static void animateAurora(uint32_t durationMs, uint32_t stepMs = 24) {
        struct Curtain {
            float depth;
            float depthSweep;
            float slope;
            float folds;
            float foldWidth;
            float drift;
            float foldPhase;
            uint16_t seed;
        };
        // Two folds at separate depths and a diagonal curtain that crosses both.
        constexpr Curtain curtains[] = {
            {0.27f, -0.15f, 0.00f, 1.00f, 0.14f, -0.62f, 0.3f, 947},
            {0.73f,  0.15f, 0.00f, 1.65f, 0.11f,  0.48f, 2.6f, 6197},
            {0.50f,  0.00f, 0.76f, 1.20f, 0.06f, -0.39f, 4.5f, 10321}
        };
        constexpr float rippleWeight = 0.30f;
        constexpr float foldLean = 0.022f;
        constexpr float rippleLean = 0.018f;
        const CRGB green(18, 255, 53), teal(0, 222, 159);
        const CRGB pink(255, 32, 170), violet(94, 90, 238);
        constexpr int baseZ = 2;
        constexpr int verticalSpan = RNDR_Z - 1 - baseZ;
        const uint32_t started = millis();
        while (true) {
            const uint32_t now = millis();
            const uint32_t elapsed = now - started;
            if (!TestDemo::routineTimeRemaining(elapsed, durationMs)) break;
            const float seconds = elapsed * 0.001f;
            clearAll();

            for (const auto &curtain : curtains) {
                const float depthSpan = RNDR_Y - 1.0f;
                const float depthPhase = seconds * curtain.drift * 0.75f + curtain.foldPhase;
                const float meanDepth = curtain.depth + curtain.depthSweep * (0.5f + 0.5f * sinf(depthPhase));
                // Narrow the folds near a face so the curtain keeps moving smoothly within the cube.
                const float depthRoom = min(meanDepth, 1.0f - meanDepth) - 0.5f * fabsf(curtain.slope);
                const float foldWidth = min(curtain.foldWidth,
                                            (depthRoom - foldLean - rippleLean) / (1.0f + rippleWeight));
                const uint16_t wind = uint16_t(elapsed / 3);
                const uint16_t evolution = uint16_t(elapsed / 13);
                const uint16_t heightWind = uint16_t(elapsed / 12);
                const uint16_t heightEvolution = uint16_t(elapsed / 52);
                for (int x = 0; x < RNDR_X; ++x) {
                    const float along = float(x) / (RNDR_X - 1);
                    const float phase = TWO_PI * along * curtain.folds + seconds * curtain.drift + curtain.foldPhase;
                    const float fold = sinf(phase);
                    const float ripple = sinf(phase * 2.0f + seconds * 1.1f);
                    const float ridge = depthSpan * (meanDepth + curtain.slope * (along - 0.5f) +
                                                     foldWidth * (fold + rippleWeight * ripple));
                    const float lean = depthSpan * (foldLean * cosf(phase) + rippleLean * ripple);

                    // Advect brightness along the sheet. Each sample drives a
                    // whole ray so bright patches stretch vertically as they pass.
                    const float ray = inoise8(uint16_t(curtain.seed + along * 1340.0f) - wind, evolution);
                    const float swell = inoise8(uint16_t(curtain.seed + along * 440.0f) + wind / 2,
                                                uint16_t(1234 + evolution));
                    const float strength = constrain((ray - 65.0f) / 92.0f, 0.0f, 1.0f);
                    const float surge = constrain((swell - 90.0f) / 65.0f, 0.0f, 1.0f);
                    const float rayPower = 0.28f + 0.72f * strength * (0.45f + 0.55f * surge);
                    // Run tip height at quarter speed while light keeps traveling along the curtain.
                    const float heightRay = inoise8(uint16_t(curtain.seed + along * 1340.0f) - heightWind,
                                                    heightEvolution);
                    const float heightSwell = inoise8(uint16_t(curtain.seed + along * 440.0f) + heightWind / 2,
                                                      uint16_t(1234 + heightEvolution));
                    const float heightStrength = constrain((heightRay - 65.0f) / 92.0f, 0.0f, 1.0f);
                    const float heightSurge = constrain((heightSwell - 90.0f) / 65.0f, 0.0f, 1.0f);
                    const float tipPower = 0.28f + 0.72f * heightStrength * (0.45f + 0.55f * heightSurge);
                    const float rayHeight = verticalSpan * (0.66f + 0.26f * heightStrength + 0.28f * heightSurge);
                    const float pinkAmount = 0.08f + 0.76f * constrain((swell - 104.0f) / 63.0f, 0.0f, 1.0f);

                    const int last = min(verticalSpan, int(ceilf(rayHeight)));
                    for (int height = 0; height <= last; ++height) {
                        const float h = min(1.0f, height / rayHeight);
                        const float fade = 1.0f - h;
                        // The visible tips follow the same slow field as their height.
                        const float tipMix = constrain((h - 0.40f) / 0.45f, 0.0f, 1.0f);
                        const float rayLight = rayPower + tipMix * (tipPower - rayPower);
                        // Noise fades in above the full-bright curtain base.
                        const float noiseMix = min(1.0f, 4.0f * height / verticalSpan);
                        const float power = 1.0f - noiseMix * (1.0f - rayLight);
                        const uint8_t brightness = uint8_t(255.0f * power * fade * (0.35f + 0.65f * fade));
                        if (brightness == 0) continue;

                        CRGB color = blend(green, teal, uint8_t(255.0f * h));
                        const float violetMix = surge + tipMix * (heightSurge - surge);
                        color = blend(color, violet, uint8_t(255.0f * violetMix * h * h));
                        const float fringe = max(0.0f, 1.0f - height / (0.24f * verticalSpan));
                        color = blend(color, pink, uint8_t(255.0f * pinkAmount * fringe));
                        color.nscale8(brightness);

                        // One strand with fractional coverage across depth.
                        // The upper tip leans farther than the bright foot.
                        const int32_t depth = lroundf(256.0f * (ridge + h * lean));
                        GeometryEngine::drawWuVoxel(x * 256, depth, (baseZ + height) * 256, color);
                    }
                }
            }
            showCube();
            const uint32_t spent = millis() - now;
            if (spent < stepMs) delay(stepMs - spent);
        }
    }

    // Shared with Audio::animateJulia; normal animation retains timed c motion.
    static void animateJulia(uint32_t durationMs, uint32_t speedMs = 40,
                             bool cycleColors = true, bool varyC = true) {
        JuliaRenderer::animateJulia(durationMs, speedMs, cycleColors, varyC);
    }

    enum class FractalLook { Cavern, Sculpture, RecursiveCube };

    struct FractalSettings {
        uint32_t frameMs = 40;        // Target 25fps; never add a full delay after a slow draw.
        uint32_t colorCycleMs = 24000; // One continuous rainbow lap, independent of geometry.
        float cavernSpeed = 3.0f;     // Speed up all cavern motion and its palette cycle together.
        float cavernTravelRate = 0.10f; // Radians/s on a closed three-axis sampling path.
        float sculptureTumbleRate = 4.0f; // Common tumble phase in degrees/s.
        uint32_t scaffoldScaleMs = 12000; // One scale-to-scale handoff.
        uint32_t scaffoldInitialHoldMs = 5000; // Legacy scale/tumble comparison only; mice start immediately.
        uint32_t scaffoldColorMs = 18000; // Independent rainbow lap for scaffolds.
    };

    // One director for sampled fractal fields. Reuse the core
    // coverage/transforms and TextureState color cycling, with no privileged
    // viewing face. Julia is independent; the original bulb is off the deck.
    static void animateFractalVolume(uint32_t durationMs, FractalLook look,
                                     const FractalSettings &settings) {
        const bool cavern = look == FractalLook::Cavern;
        const bool scaffold = look == FractalLook::RecursiveCube;
        const bool holdInitialPose = scaffold;
        const float sceneRate = cavern ? constrain(settings.cavernSpeed, 0.05f, 8.0f) : 1.0f;
        const char *name = cavern ? "Fractal Cavern" :
                           look == FractalLook::RecursiveCube ? "Recursive Cube" : "Fractal Sculpture";
        const float halfSpan = max(1.0f, min(RNDR_CX, min(RNDR_CY, RNDR_CZ)));
        const float unitsPerVoxel = (cavern ? 0.45f : (scaffold ? 1.0f : 1.20f)) / halfSpan;
        // Linear Flow supplies the same cycling color sampler to both fields.
        // Orbit/position contours choose the material index, not brightness.
        const uint32_t colorPeriod = scaffold ? settings.scaffoldColorMs : settings.colorCycleMs;
        const float colorRate = 1280.0f * sceneRate / max(uint32_t(1), colorPeriod);
        TextureState texture(MODE_LINEAR_FLOW, RainbowColors_p, colorRate);
        const uint32_t started = millis();
        uint32_t lastFrame = started, lastReport = started;
        Serial.printf("%s | Rainbow | cycle=%lums | target=%lums | %s=%.2f\n", name,
                      (unsigned long)lroundf(colorPeriod / sceneRate), (unsigned long)settings.frameMs,
                      cavern ? "translation X peak vox/s" : "tumble phase degrees/s",
                      double(cavern ? 0.25f * settings.cavernTravelRate * sceneRate / unitsPerVoxel
                                    : settings.sculptureTumbleRate));
        if (scaffold) Serial.printf("Recursive Cube | scale cycle=%lums | continuous scale handoff\n",
                                    (unsigned long)settings.scaffoldScaleMs);
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            const uint32_t now = millis();
            const uint32_t elapsed = uint32_t(now - started);
            // Pause the geometry clock, not the frame loop or rainbow. Subtract
            // the hold afterward so motion starts continuously from this pose.
            const uint32_t geometryElapsed = holdInitialPose
                ? (elapsed > settings.scaffoldInitialHoldMs ? elapsed - settings.scaffoldInitialHoldMs : 0)
                : elapsed;
            const float seconds = float(geometryElapsed) * (0.001f * sceneRate);
            texture.advance(uint32_t(now - lastFrame));
            lastFrame = now;
            const float path = seconds * settings.cavernTravelRate;
            const GeometryEngine::Point3 origin = cavern ? GeometryEngine::Point3{
                0.25f * sinf(path + 0.4f), 0.25f * sinf(path * 0.83f + 1.2f),
                0.25f * sinf(path * 0.71f + 2.4f)} : GeometryEngine::Point3{0, 0, 0};
            const float tumblePhase = seconds * (cavern ? 1.0f : settings.sculptureTumbleRate);
            const GeometryEngine::TumbleRotation rotation(GeometryEngine::tumbleAngles(tumblePhase, 1.0f));
            const float foldScale = 2.8f + (cavern ? 0.025f * sinf(seconds * 0.07f) : 0.0f);
            const float scaleCycle = float(geometryElapsed % max(uint32_t(1), settings.scaffoldScaleMs)) /
                                     max(uint32_t(1), settings.scaffoldScaleMs);
            const GeometryEngine::RecursiveScaffoldField scaffoldField(
                scaleCycle, unitsPerVoxel);
            clearAll();
            unsigned elements;
                elements = GeometryEngine::rasterFractalVolume(unitsPerVoxel, cavern ? 0.40f : (scaffold ? 0.50f : 0.65f),
                    [&](GeometryEngine::Point3 point) -> GeometryEngine::FractalSample {
                        auto p = rotation.apply(point);
                        p.x += origin.x;
                        p.y += origin.y;
                        p.z += origin.z;
                        if (scaffold) return scaffoldField(p);
                        return cavern ? GeometryEngine::mandelboxField(p, foldScale)
                                      : GeometryEngine::mandelbulbField(p);
                    },
                    [&](const GeometryEngine::FractalSample &sample, GeometryEngine::Point3 point) {
                        const int contour = int(lroundf(sample.orbitTrap * 144.0f +
                                                      point.x * 48.0f + point.y * 37.0f + point.z * 29.0f));
                        return texture.getFlowColor(uint8_t(contour));
                    });
            const uint32_t drawMs = uint32_t(millis() - now);
            showCube();
            if (uint32_t(now - lastReport) >= 1000) {
                lastReport = now;
                Serial.printf("%s | %s=%u/%u draw=%lums frame=%lums\n", name,
                              "lit", elements,
                              unsigned(RNDR_X * RNDR_Y * RNDR_Z), (unsigned long)drawMs,
                              (unsigned long)uint32_t(millis() - now));
            }
            const uint32_t spent = uint32_t(millis() - now);
            if (spent < settings.frameMs) delay(settings.frameMs - spent);
            else yield();
        }
    }

    static void animateFractalCavern(uint32_t durationMs) {
        animateFractalVolume(durationMs, FractalLook::Cavern, FractalSettings{});
    }

    static void animateFractalSculpture(uint32_t durationMs) {
        animateFractalVolume(durationMs, FractalLook::Sculpture, FractalSettings{});
    }

    struct RecursiveMouseSettings {
        uint32_t frameMs=40;
        uint32_t colorCycleMs=18000;
        float outerSpeed=36.0f;
        float centerSpeedMultiplier=3.0f;
        float tailLengthMultiplier=3.0f; // 1.5x the preceding 2x-Wild-Mouse trail.

        float speedAt(float travel,GeometryEngine::Point3 span) const {
            const auto p=GeometryEngine::RecursiveCubePath::point(travel,span);
            // Cubical depth: zero on every outer face, one at the center.
            // Reflection leaves this value unchanged, so all eight mice can
            // share a clock while accelerating through the hidden interior.
            const float r=max(fabsf(2*p.x/max(.001f,span.x)-1),
                max(fabsf(2*p.y/max(.001f,span.y)-1),fabsf(2*p.z/max(.001f,span.z)-1)));
            const float depth=constrain(1-r,0.0f,1.0f);
            return outerSpeed*(1+(max(1.0f,centerSpeedMultiplier)-1)*depth);
        }
        float advance(float travel,GeometryEngine::Point3 span,uint32_t deltaMs) const {
            // Small midpoint steps make spatial acceleration independent of
            // render cadence. Never recompute elapsed*speed, which would jump
            // the head when its local speed changes.
            while(deltaMs) {
                const uint32_t step=min(uint32_t(5),deltaMs);
                const float seconds=step*.001f;
                const float midpoint=travel+.5f*seconds*speedAt(travel,span);
                travel+=seconds*speedAt(midpoint,span);
                deltaMs-=step;
            }
            return travel;
        }
    };

    static void animateRecursiveCube(uint32_t durationMs) {
        const RecursiveMouseSettings settings;
        const uint32_t started = millis();
        uint32_t lastFrame = started;
        const float halfSpan = max(1.0f, min(RNDR_CX, min(RNDR_CY, RNDR_CZ)));
        const GeometryEngine::Point3 span{2*halfSpan, 2*halfSpan, 2*halfSpan};
        const GeometryEngine::Point3 offset{RNDR_CX-halfSpan, RNDR_CY-halfSpan, RNDR_CZ-halfSpan};
        // Wild Mouse's base is RNDR_X*3 positions, capped at 64. Use three
        // times its spatial span, independent of the local head speed.
        constexpr unsigned tailPositions = RNDR_X*3 < 64 ? RNDR_X*3 : 64;
        const float tailLength = settings.tailLengthMultiplier*float(tailPositions-1);
        const unsigned tailSamples = unsigned(ceilf(tailLength*2));
        TextureState texture(MODE_LINEAR_FLOW, RainbowColors_p, 1280.0f/settings.colorCycleMs);
        float travel=0;
        Serial.printf("Recursive Cube | 8 synchronized mice | outer=%.1f center-limit=%.1f vox/s | tail=%.1f vox | levels=%u | no hold\n",
                      double(settings.outerSpeed),double(settings.outerSpeed*settings.centerSpeedMultiplier),
                      double(tailLength),GeometryEngine::RecursiveCubePath::levelsForSpan(span));
        while (TestDemo::routineTimeRemaining(uint32_t(millis()-started), durationMs)) {
            const uint32_t now=millis();
            const uint32_t deltaMs=uint32_t(now-lastFrame);
            texture.advance(deltaMs);lastFrame=now;
            clearAll();
            travel=settings.advance(travel,span,deltaMs);
            for(unsigned mouse=0;mouse<8;++mouse) for(unsigned age=0;age<=tailSamples;++age) {
                const float behind=tailLength*age/tailSamples;
                if(behind>travel) break;
                auto p=GeometryEngine::RecursiveCubePath::point(travel-behind,span);
                if(mouse&1) p.x=span.x-p.x;
                if(mouse&2) p.y=span.y-p.y;
                if(mouse&4) p.z=span.z-p.z;
                p.x+=offset.x;p.y+=offset.y;p.z+=offset.z;
                CRGB color=texture.getFlowColor(uint8_t(mouse*32));
                color.nscale8(uint8_t(255-240*age/tailSamples));
                GeometryEngine::drawCrispVoxel(p,color);
            }
            showCube();
            const uint32_t spent = uint32_t(millis()-now);
            if (spent < settings.frameMs) delay(settings.frameMs-spent);
            else yield();
        }
    }


    struct FibonacciSweepSettings {
        uint32_t frameMs=40;
        uint32_t sweepMs=8000;
        uint32_t edgeHoldMs=400;
        uint32_t colorCycleMs=18000;
        float degreesPerPlane=45;
        unsigned scaffoldTrailPlanes=4;
        unsigned spiralTrailPlanes=8;
    };

    struct FibonacciDirectionCycle {
        uint8_t faces[4]={0,1,2,3};
        unsigned cursor=4;
        uint8_t last=255;
        uint8_t nextFace() {
            if(cursor==4) {
                for(unsigned i=3;i>0;--i) {
                    const unsigned j=random8(i+1);
                    const uint8_t saved=faces[i];faces[i]=faces[j];faces[j]=saved;
                }
                if(faces[0]==last) {
                    const unsigned j=1+random8(3);
                    const uint8_t saved=faces[0];faces[0]=faces[j];faces[j]=saved;
                }
                cursor=0;
            }
            last=faces[cursor++];return last;
        }
    };

    // One leader crosses the cube, leaving stationary fading impressions on
    // planes it has actually passed. Moving delayed copies would re-enter the
    // starting face repeatedly and look like the pattern was restarting.
    static void animateFibonacciSweep(uint32_t durationMs,bool spiral,
                                       const FibonacciSweepSettings &settings) {
        const uint32_t started=millis();
        uint32_t lastFrame=started,lastSweep=0;
        FibonacciDirectionCycle directions;
        unsigned face=directions.nextFace();
        TextureState texture(MODE_LINEAR_FLOW,RainbowColors_p,1280.0f/max(uint32_t(1),settings.colorCycleMs));
        const uint32_t sweepMs=max(uint32_t(1),settings.sweepMs);
        const uint32_t hold=min(settings.edgeHoldMs,(sweepMs-1)/2);
        while(TestDemo::routineTimeRemaining(uint32_t(millis()-started),durationMs)) {
            const uint32_t now=millis(),elapsed=uint32_t(now-started),sweep=elapsed/sweepMs;
            const bool newSweep=sweep!=lastSweep;
            if(newSweep) { face=directions.nextFace();lastSweep=sweep; }
            if(elapsed==0 || newSweep) {
                const char *faceNames[]={"-X","+X","-Y","+Y"};
                Serial.printf("%s | from=%s | sweep=%lums | turn=%.1f degrees/plane | trail=%u planes including head | cycling Rainbow\n",
                    spiral ? "Fibonacci Spiral" : "Fibonacci Scaffold",faceNames[face],
                    (unsigned long)sweepMs,double(settings.degreesPerPlane),
                    spiral ? settings.spiralTrailPlanes : settings.scaffoldTrailPlanes);
            }
            texture.advance(uint32_t(now-lastFrame));lastFrame=now;
            const unsigned axis=face/2;
            const float normalHalf=axis==0 ? RNDR_CX : axis==1 ? RNDR_CY : RNDR_CZ;
            const float halfU=axis==0 ? RNDR_CY : RNDR_CX;
            const float halfV=axis==2 ? RNDR_CY : RNDR_CZ;
            const float progress=constrain((float(elapsed%sweepMs)-hold)/(sweepMs-2*hold),0.0f,1.0f);
            const float distance=2*normalHalf*progress;
            const unsigned trailPlanes=max(1u,spiral ? settings.spiralTrailPlanes : settings.scaffoldTrailPlanes);
            auto drawAt=[&](float position,uint8_t brightness) {
                const float depth=(face&1 ? 1.0f : -1.0f)*(normalHalf-position);
                const float angle=position*settings.degreesPerPlane*(PI/180);
                const float c=cosf(angle),s=sinf(angle);
                GeometryEngine::PlaneFrame3D plane;
                plane.center={RNDR_CX,RNDR_CY,RNDR_CZ};
                if(axis==0) { plane.center.x+=depth;plane.u={0,c,s};plane.v={0,-s,c}; }
                else { plane.center.y+=depth;plane.u={c,0,s};plane.v={-s,0,c}; }
                if(spiral) GeometryEngine::drawFibonacciSpiral3D(plane,halfU,halfV,texture,brightness);
                else GeometryEngine::drawGoldenScaffoldPlane(plane,halfU,halfV,texture,brightness);
            };
            clearAll();
            const int passedPlane=int(floorf(distance));
            const int firstPlane=max(0,passedPlane-int(trailPlanes)+1);
            for(int plane=firstPlane;plane<=passedPlane;++plane) {
                const float age=distance-plane;
                const uint8_t brightness=uint8_t(255*constrain(1-age/trailPlanes,0.0f,1.0f));
                if(brightness) drawAt(float(plane),brightness);
            }
            // The only moving pattern is the leader. At an integer plane it
            // coincides with its impression; strongest-hue blending prevents
            // duplicate brightness. Material color cycling remains continuous.
            drawAt(distance,255);
            showCube();
            const uint32_t spent=uint32_t(millis()-now);
            if(spent<settings.frameMs) delay(settings.frameMs-spent);else yield();
        }
    }

    static void animateFibonacci(uint32_t durationMs) {
        // Choose once at entry; retain the selected pattern for every sweep.
        const bool spiral=random8(2)!=0;
        animateFibonacciSweep(durationMs,spiral,FibonacciSweepSettings{});
    }

    enum class PrimeStudy { CoprimeWeave, PrimeSymmetry };
    struct PrimeStudySettings {
        uint32_t frameMs=TestDemo::ArmComparison::frameMs;
        uint32_t colorCycleMs=TestDemo::ArmComparison::colorCycleMs;
        float ribbonHalfWidthFraction=.22f;
        float ribbonSpeedMultiplier=2.0f;
        float ribbonTumblePhaseRate=TestDemo::ArmComparison::tumblePhaseRate;
        float primeTumblePhaseRate=TestDemo::ArmComparison::tumblePhaseRate;
        float primeArmSpeedMultiplier=4.0f; // +300% arm motion; independent of shared tumble.
    };

    // Geometric auditions, not particle simulations. One bounded frame loop
    // handles timing/colors; existing geometry helpers own all voxel coverage.
    static void animatePrimeStudy(uint32_t durationMs, PrimeStudy study,
                                   const PrimeStudySettings &settings) {
        const uint32_t started=millis();
        uint32_t lastFrame=started;
        const float reach=max(1.0f,min(RNDR_CX,min(RNDR_CY,RNDR_CZ)));
        const GeometryEngine::Point3 center{RNDR_CX,RNDR_CY,RNDR_CZ};
        TextureState texture(MODE_LINEAR_FLOW,RainbowColors_p,1280.0f/max(uint32_t(1),settings.colorCycleMs));
        const char *name=study==PrimeStudy::CoprimeWeave ? "Coprime Weave" : "Prime Symmetry";
        Serial.printf("%s | cycling Rainbow | target=%lums | color cycle=%lums\n",name,
                      (unsigned long)settings.frameMs,(unsigned long)settings.colorCycleMs);
        if (study==PrimeStudy::CoprimeWeave)
            Serial.printf("Coprime Weave | motion=%.1fx | pivot=cube center | tumble phase=%.1f deg/s\n",
                          double(settings.ribbonSpeedMultiplier),double(settings.ribbonTumblePhaseRate));
        else Serial.printf("Prime Symmetry | arms=3/5/7 | arm speed=%.1fx | shared slow tumble\n",
                           double(settings.primeArmSpeedMultiplier));
        while (TestDemo::routineTimeRemaining(uint32_t(millis()-started),durationMs)) {
            const uint32_t now=millis(),elapsed=uint32_t(now-started);
            texture.advance(uint32_t(now-lastFrame));lastFrame=now;
            clearAll();
            const float periods[]={5,7,11};
            if (study==PrimeStudy::CoprimeWeave) {
                // Rotate the whole local weave around the cube center. The
                // width vectors rotate with the centerlines, preserving each
                // ribbon's cross section and their relative arrangement.
                const GeometryEngine::TumbleRotation rotation(GeometryEngine::tumbleAngles(
                    float(elapsed)*.001f*settings.ribbonTumblePhaseRate,1.0f));
                for (unsigned ribbon=0;ribbon<3;++ribbon) {
                    const uint32_t period=max(uint32_t(1),uint32_t(periods[ribbon]*1000 /
                        max(.01f,settings.ribbonSpeedMultiplier)));
                    const float phase=TWO_PI*float(elapsed % period)/period;
                    GeometryEngine::Point3 width{0,0,0};
                    const float halfWidth=reach*settings.ribbonHalfWidthFraction;
                    if (ribbon==0) width.z=halfWidth;
                    else if (ribbon==1) width.x=halfWidth;
                    else width.y=halfWidth;
                    width=rotation.apply(width);
                    GeometryEngine::drawRibbon3D([&](float t) -> GeometryEngine::Point3 {
                        const float along=(2*t-1)*reach;
                        const float bend=reach*.68f*sinf(TWO_PI*t+phase);
                        const float drift=reach*.38f*sinf(PI*t+phase);
                        const auto p=rotation.apply(ribbon==0 ? GeometryEngine::Point3{along,bend,drift} :
                            ribbon==1 ? GeometryEngine::Point3{drift,along,bend} : GeometryEngine::Point3{bend,drift,along});
                        return {center.x+p.x,center.y+p.y,center.z+p.z};
                    },width,unsigned(ceilf(reach*8)),texture,uint8_t(ribbon*85));
                }
            } else {
                // Distinct radial orders occupy three orthogonal planes. Slow
                // shared tumble changes the view without replacing arm motion.
                const GeometryEngine::TumbleRotation rotation(
                    GeometryEngine::tumbleAngles(float(elapsed)*.001f*settings.primeTumblePhaseRate,1.0f));
                // Match Pinwheel's circumscribed radius. An inscribed radius
                // leaves diagonal arms buried inside the cube as it tumbles.
                const float outerReach=.5f*sqrtf(float(RNDR_X)*RNDR_X+float(RNDR_Y)*RNDR_Y+float(RNDR_Z)*RNDR_Z);
                const float innerReach=.13f*reach;
                const unsigned arms[]={3,5,7};
                for (unsigned figure=0;figure<3;++figure) {
                    const uint32_t spinPeriod=max(uint32_t(1),uint32_t(periods[figure]*2000 /
                        max(.01f,settings.primeArmSpeedMultiplier)));
                    const float phase=TWO_PI*float(elapsed % spinPeriod)/spinPeriod;
                    for (unsigned arm=0;arm<arms[figure];++arm) {
                        const unsigned samples=unsigned(ceilf(outerReach*3));
                        GeometryEngine::Point3 previous{0,0,0};
                        for (unsigned sample=0;sample<=samples;++sample) {
                            const float t=float(sample)/samples;
                            const float angle=phase+TWO_PI*arm/arms[figure]+.45f*t*sinf(phase);
                            const float r=innerReach+(outerReach-innerReach)*t;
                            const float u=r*cosf(angle),v=r*sinf(angle);
                            auto point=rotation.apply(figure==0 ? GeometryEngine::Point3{u,v,0} :
                                figure==1 ? GeometryEngine::Point3{0,u,v} : GeometryEngine::Point3{v,0,u});
                            if (sample) {
                                const CRGB color=texture.getFlowColor(uint8_t(figure*85+t*42));
                                GeometryEngine::drawSdfLine3D(center.x+previous.x,center.y+previous.y,center.z+previous.z,
                                    center.x+point.x,center.y+point.y,center.z+point.z,.24f,color,false,true);
                            }
                            previous=point;
                        }
                    }
                }
            }
            showCube();
            const uint32_t spent=uint32_t(millis()-now);
            if (spent<settings.frameMs) delay(settings.frameMs-spent); else yield();
        }
    }

    static void animateCoprimeWeave(uint32_t durationMs) {
        animatePrimeStudy(durationMs,PrimeStudy::CoprimeWeave,PrimeStudySettings{});
    }
    static void animatePrimeSymmetry(uint32_t durationMs) {
        animatePrimeStudy(durationMs,PrimeStudy::PrimeSymmetry,PrimeStudySettings{});
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
    static void animateRockingSineWave(uint32_t durationMs, float waveSpeed, const CRGBPalette16 &palette,
                                       ShaderMode shader) {
        constexpr float rockPeriodMs = 8000.0f;
        constexpr float maxTilt = 25.0f * PI / 180.0f;
        const float extent = min(RNDR_X, RNDR_Y);
        const float angleScale = 65536.0f / extent;
        // Leave vertical room for the lifted edge of the rocking sheet.
        const float amplitude = min(RNDR_CZ - 0.5f, extent * 0.28f);
        TextureState tex(shader, palette, 4.0f);
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
