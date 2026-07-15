#pragma once
#include "CubeCore.h"
#include "TextureEngine.h"
#include "GeometryEngine.h"

typedef void (*DrawShapeFunc)(float cx, float cy, float cz, float radius, RenderMode mode, const TextureState &tex, float rotX, float rotY, float rotZ, bool eraseBackground, float aspectZ);

enum Orientation { AXIS_X = 0, AXIS_Y = 1, AXIS_Z = 2 };

enum ShapeType {
    SHAPE_BOX, SHAPE_SPHERE, SHAPE_OVOID, SHAPE_TETRAHEDRON, SHAPE_OCTAHEDRON, 
    SHAPE_MERKABA, SHAPE_TORUS, SHAPE_CYLINDER, SHAPE_HEMISPHERES, SHAPE_HOURGLASS
};

class Shapes {
private:
static RenderMode applyShapeRenderHacks(ShapeType shape, RenderMode rMode) {
        // If it's not a wireframe, let it pass untouched
        if (rMode != RENDER_WIREFRAME) return rMode;

        // If it is a curved shape, forcibly override the wireframe request to Solid
        if (shape == SHAPE_SPHERE || shape == SHAPE_OVOID || 
            shape == SHAPE_TORUS || shape == SHAPE_CYLINDER || 
            shape == SHAPE_HEMISPHERES || shape == SHAPE_HOURGLASS) { // <-- Added Hourglass
            return RENDER_SOLID; 
        }

        return rMode; // Boxes, Tetrahedrons, and Octahedrons pass through safely
}
    static CRGBPalette16 applyShapeColorHacks(ShapeType shape, CRGBPalette16 pal, ShaderMode sMode) {
        if (sMode == MODE_SOLID || sMode == MODE_SPATIAL_GRADIENT) {
            bool isWhite = (pal[0] == CRGB(CRGB::White));
            bool isGray  = (pal[0] == CRGB(CRGB::Gray));
            
            if (isWhite) {
                // Only Merkaba is allowed to use White
                if (shape == SHAPE_MERKABA) return PaletteUtils::getDynamicContrastPalette(CRGB::White);
                return CRGBPalette16(PaletteUtils::getRandomVibrantColor());
            }
            
            if (isGray) {
                // The universal dynamic contrast hook
                if (shape == SHAPE_OCTAHEDRON || shape == SHAPE_MERKABA || shape == SHAPE_CYLINDER || shape == SHAPE_HEMISPHERES || shape == SHAPE_HOURGLASS) {
                    return PaletteUtils::getDynamicContrastPalette(CRGB::Gray);
                }
                return CRGBPalette16(PaletteUtils::getRandomVibrantColor());
            }
        }
        return pal; 
    }
    static CRGBPalette16 getSmartPalette(ShaderMode sMode) {
        if (sMode == MODE_SOLID) return CRGBPalette16(CHSV(random8(), 255, 255));
        if (sMode == MODE_NOISE_FIELD || sMode == MODE_LINEAR_FLOW) return PaletteUtils::getRandomOrganicPalette();
        return PartyColors_p;
    }
    
    // ==========================================
    // MASTER DIRECTORS (Transform Path)
    // ==========================================
    static void animateStaticShape(DrawShapeFunc drawFunc, float fixedRadius, uint32_t durationMs, RenderMode rMode, ShaderMode sMode, CRGBPalette16 pal, bool rotate = false, float tumbleSpeed = 1.0f, float customX = 0, float customY = 0, float customZ = 0, bool stretch = false) {
        uint32_t startTime = millis();
        uint32_t lastFrameTime = millis();
        
        float geomPhase = 0.0f; 
        float aspectZ = stretch ? ((RNDR_Z > RNDR_X) ? (RNDR_CZ / RNDR_CX) : 1.0f) : 1.0f;
        
        TextureState tex(sMode, pal, 1.0f);

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrameTime;
            lastFrameTime = now;
            
            geomPhase += 0.2f * (float)deltaMs;

            clearAll();
            float rx = rotate ? geomPhase * (0.5f * tumbleSpeed) : (tumbleSpeed == 0.0f ? customX : geomPhase * customX);
            float ry = rotate ? geomPhase * (0.8f * tumbleSpeed) : (tumbleSpeed == 0.0f ? customY : geomPhase * customY);
            float rz = rotate ? geomPhase * (0.3f * tumbleSpeed) : (tumbleSpeed == 0.0f ? customZ : geomPhase * customZ);
            
            drawFunc(RNDR_CX, RNDR_CY, RNDR_CZ, fixedRadius, rMode, tex, rx, ry, rz, true, aspectZ);
            showCube();
            
            tex.advance(deltaMs);
            yield();
        }
    }

    static void animateExpandingCollapsingBase(DrawShapeFunc drawFunc, float maxRadius, uint32_t durationMs, RenderMode rMode, ShaderMode sMode, uint32_t speedMs, CRGBPalette16 pal, bool rotate = false, float tumbleSpeed = 1.0f, float customX = 0, float customY = 0, float customZ = 0, bool stretch = false) {
        uint32_t startTime = millis();
        uint32_t lastFrameTime = millis();
        uint32_t holdStartTime = 0;
        
        float currentRadius = 0.0f, radiusDir = 1.0f;
        bool holding = false;
        float geomPhase = 0.0f;
        float aspectZ = stretch ? ((RNDR_Z > RNDR_X) ? (RNDR_CZ / RNDR_CX) : 1.0f) : 1.0f;

        TextureState tex(sMode, pal, 0.8f);

        while (millis() - startTime < durationMs) {
            clearAll();
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrameTime;
            lastFrameTime = now;

            if (holding) {
                if (now - holdStartTime > speedMs * 3) { holding = false; radiusDir = -1.0f; }
            } else {
                currentRadius += radiusDir * (0.5f / (float)speedMs) * (float)deltaMs;
                if (currentRadius >= maxRadius) { currentRadius = maxRadius; holding = true; holdStartTime = now; }
                else if (currentRadius <= 0.0f) { currentRadius = 0.0f; radiusDir = 1.0f; }
            }

            geomPhase += 0.2f * (float)deltaMs;

            float rx = rotate ? geomPhase * (0.5f * tumbleSpeed) : (tumbleSpeed == 0.0f ? customX : geomPhase * customX);
            float ry = rotate ? geomPhase * (0.8f * tumbleSpeed) : (tumbleSpeed == 0.0f ? customY : geomPhase * customY);
            float rz = rotate ? geomPhase * (0.3f * tumbleSpeed) : (tumbleSpeed == 0.0f ? customZ : geomPhase * customZ);

            drawFunc(RNDR_CX, RNDR_CY, RNDR_CZ, currentRadius, rMode, tex, rx, ry, rz, true, aspectZ);
            
            showCube();
            tex.advance(deltaMs);
            yield(); 
        }
    }

   static void animateExpandingCollapsingBoxInternal(uint32_t durationMs, RenderMode rMode, ShaderMode sMode, uint32_t speedMs, CRGBPalette16 pal, bool stretch) {
        uint32_t startTime = millis();
        uint32_t lastStepTime = millis();
        uint32_t lastFrameTime = millis();
        uint32_t holdStartTime = 0;
        
        int currentStepZ = 0;
        int stepDir = 1;
        bool holding = false;

        int cx1 = (RNDR_X - 1) / 2, cx2 = RNDR_X / 2;
        int cy1 = (RNDR_Y - 1) / 2, cy2 = RNDR_Y / 2;
        int cz1 = (RNDR_Z - 1) / 2, cz2 = RNDR_Z / 2;
        
        int maxStepZ;
        #ifdef TARGET_2026_CUBOID
            maxStepZ = stretch ? (RNDR_Z / 2) : (RNDR_X / 2);
        #else
            maxStepZ = RNDR_X / 2;
        #endif

        TextureState tex(sMode, pal, 0.8f);

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrameTime;
            lastFrameTime = now;

            clearAll();

            // 1. THE SNAPPY INTEGER TIMER
            if (holding) {
                if (now - holdStartTime > (speedMs * 3)) { // Hold briefly at max size
                    holding = false; 
                    stepDir = -1; 
                    lastStepTime = now;
                }
            } else {
                if (now - lastStepTime >= speedMs) {
                    currentStepZ += stepDir;
                    lastStepTime = now;

                    if (currentStepZ >= maxStepZ) { 
                        currentStepZ = maxStepZ; 
                        holding = true; 
                        holdStartTime = now; 
                    }
                    else if (currentStepZ <= 0) { 
                        currentStepZ = 0; 
                        stepDir = 1; 
                    }
                }
            }

            // 2. GEOMETRY SCALING
            int stepX = currentStepZ;
            int stepY = currentStepZ;

            #ifdef TARGET_2026_CUBOID
                if (stretch) { 
                    // Mathematically map the vertical step to the horizontal bounds perfectly
                    stepX = (currentStepZ * (RNDR_X / 2)) / (RNDR_Z / 2);
                    stepY = stepX; 
                }
            #endif

            // 3. APPLY GRID BOUNDS
            uint8_t x0 = max(0, cx1 - stepX), x1 = min((int)RNDR_X - 1, cx2 + stepX);
            uint8_t y0 = max(0, cy1 - stepY), y1 = min((int)RNDR_Y - 1, cy2 + stepY);
            uint8_t z0 = max(0, cz1 - currentStepZ), z1 = min((int)RNDR_Z - 1, cz2 + currentStepZ);

            // 4. RENDER
            GeometryEngine::drawFastBox(x0, y0, z0, x1, y1, z1, rMode, tex);
            
            showCube();
            tex.advance(deltaMs); 
            yield(); 
        }
    }

    static void animateSpringBase(DrawShapeFunc drawFunc, float radius, uint32_t durationMs, RenderMode rMode, ShaderMode sMode, uint32_t speedMs, CRGBPalette16 pal, bool rotate = false, float tumbleSpeed = 1.0f, float customX = 0, float customY = 0, float customZ = 0, bool stretch = false) {
        uint32_t startTime = millis();
        uint32_t lastFrameTime = millis();
        
        float aspectZ = stretch ? ((RNDR_Z > RNDR_X) ? (RNDR_CZ / RNDR_CX) : 1.0f) : 1.0f;
        float currentZ = RNDR_CZ; 
        float zDir = 1.0f; 
        float geomPhase = 0.0f;

        TextureState tex(sMode, pal, 0.8f);

        auto isBlank = []() -> bool {
            for (uint8_t x = 0; x < RNDR_X; x++) {
                for (uint8_t y = 0; y < RNDR_Y; y++) {
                    for (uint8_t z = 0; z < RNDR_Z; z++) {
                        CRGB c = getVoxel(x, y, z);
                        if (c.r > 0 || c.g > 0 || c.b > 0) return false; 
                    }
                }
            }
            return true; 
        };

        while(true) {
            clearAll();
            drawFunc(RNDR_CX, RNDR_CY, currentZ, radius, rMode, tex, customX, customY, customZ, true, aspectZ);
            if (isBlank()) {
                currentZ += 0.5f; 
                break;
            }
            currentZ -= 0.5f;
            if (currentZ < -50.0f) { currentZ = 0.0f; break; } 
        }

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrameTime;
            lastFrameTime = now;

            float moveSpeed = (speedMs > 0) ? (1.0f / (float)speedMs) : 0.1f;
            float nextZ = currentZ + (zDir * moveSpeed * (float)deltaMs);

            geomPhase += 0.2f * (float)deltaMs;

            float rx = rotate ? geomPhase * (0.5f * tumbleSpeed) : (tumbleSpeed == 0.0f ? customX : geomPhase * customX);
            float ry = rotate ? geomPhase * (0.8f * tumbleSpeed) : (tumbleSpeed == 0.0f ? customY : geomPhase * customY);
            float rz = rotate ? geomPhase * (0.3f * tumbleSpeed) : (tumbleSpeed == 0.0f ? customZ : geomPhase * customZ);
            
            clearAll();
            drawFunc(RNDR_CX, RNDR_CY, nextZ, radius, rMode, tex, rx, ry, rz, true, aspectZ);
            
            if (isBlank()) {
                zDir = (nextZ > RNDR_CZ) ? -1.0f : 1.0f;
                clearAll();
                drawFunc(RNDR_CX, RNDR_CY, currentZ, radius, rMode, tex, rx, ry, rz, true, aspectZ);
            } else {
                currentZ = nextZ;
            }
            
            showCube();
            tex.advance(deltaMs);
            yield(); 
        }
    }

    static void animateFastCornerPulse(RenderMode rMode, TextureState &tex, bool travel, uint32_t speedMs) {
        auto getCorner = [](uint8_t &cx, uint8_t &cy, uint8_t &cz) {
            cx = (random8(2) == 0) ? 0 : RNDR_X - 1;
            cy = (random8(2) == 0) ? 0 : RNDR_Y - 1;
            cz = (random8(2) == 0) ? 0 : RNDR_Z - 1;
        };

        uint8_t cx1, cy1, cz1; getCorner(cx1, cy1, cz1);
        uint8_t cx2 = cx1, cy2 = cy1, cz2 = cz1;
        
        if (travel) {
            do { getCorner(cx2, cy2, cz2); } while (cx1 == cx2 && cy1 == cy2 && cz1 == cz2);
        }

        uint8_t maxSize = max(RNDR_X, max(RNDR_Y, RNDR_Z));

        auto applyBounds = [&](uint8_t size, uint8_t cx, uint8_t cy, uint8_t cz, uint8_t &x0, uint8_t &y0, uint8_t &z0, uint8_t &x1, uint8_t &y1, uint8_t &z1) {
            if (cx == 0) { x0 = 0; x1 = min((int)RNDR_X - 1, size - 1); } else { x1 = RNDR_X - 1; x0 = max(0, (int)RNDR_X - size); }
            if (cy == 0) { y0 = 0; y1 = min((int)RNDR_Y - 1, size - 1); } else { y1 = RNDR_Y - 1; y0 = max(0, (int)RNDR_Y - size); }
            if (cz == 0) { z0 = 0; z1 = min((int)RNDR_Z - 1, size - 1); } else { z1 = RNDR_Z - 1; z0 = max(0, (int)RNDR_Z - size); }
        };

        uint32_t lastFrameTime = millis();

        for (uint8_t size = 1; size <= maxSize; size++) {
            uint8_t x0, y0, z0, x1, y1, z1; applyBounds(size, cx1, cy1, cz1, x0, y0, z0, x1, y1, z1);
            clearAll(); GeometryEngine::drawFastBox(x0, y0, z0, x1, y1, z1, rMode, tex); showCube();
            uint32_t now = millis(); tex.advance(now - lastFrameTime); lastFrameTime = now;
            if (speedMs > 0) delay(speedMs); else yield(); 
        }
        for (int size = maxSize - 1; size >= 1; size--) {
            uint8_t x0, y0, z0, x1, y1, z1; applyBounds((uint8_t)size, cx2, cy2, cz2, x0, y0, z0, x1, y1, z1);
            clearAll(); GeometryEngine::drawFastBox(x0, y0, z0, x1, y1, z1, rMode, tex); showCube();
            uint32_t now = millis(); tex.advance(now - lastFrameTime); lastFrameTime = now;
            if (speedMs > 0) delay(speedMs); else yield();
        }
    }

    static void animateFastCenterPulse(RenderMode rMode, TextureState &tex, uint32_t speedMs) {
        int cx1 = (RNDR_X - 1) / 2, cx2 = RNDR_X / 2;
        int cy1 = (RNDR_Y - 1) / 2, cy2 = RNDR_Y / 2;
        int cz1 = (RNDR_Z - 1) / 2, cz2 = RNDR_Z / 2;
        uint8_t max_r = max(RNDR_X, max(RNDR_Y, RNDR_Z)) / 2;

        auto applyBounds = [&](uint8_t r, uint8_t &x0, uint8_t &y0, uint8_t &z0, uint8_t &x1, uint8_t &y1, uint8_t &z1) {
            x0 = max(0, cx1 - r); x1 = min((int)RNDR_X - 1, cx2 + r);
            y0 = max(0, cy1 - r); y1 = min((int)RNDR_Y - 1, cy2 + r);
            z0 = max(0, cz1 - r); z1 = min((int)RNDR_Z - 1, cz2 + r);
        };

        uint32_t lastFrameTime = millis();

        for (uint8_t r = 0; r <= max_r; r++) {
            uint8_t x0, y0, z0, x1, y1, z1; applyBounds(r, x0, y0, z0, x1, y1, z1);
            clearAll(); GeometryEngine::drawFastBox(x0, y0, z0, x1, y1, z1, rMode, tex); showCube(); 
            uint32_t now = millis(); tex.advance(now - lastFrameTime); lastFrameTime = now;
            if (speedMs > 0) delay(speedMs); else yield();
        }
        for (int r = max_r - 1; r >= 0; r--) {
            uint8_t x0, y0, z0, x1, y1, z1; applyBounds((uint8_t)r, x0, y0, z0, x1, y1, z1);
            clearAll(); GeometryEngine::drawFastBox(x0, y0, z0, x1, y1, z1, rMode, tex); showCube(); 
            uint32_t now = millis(); tex.advance(now - lastFrameTime); lastFrameTime = now;
            if (speedMs > 0) delay(speedMs); else yield();
        }
    }

    static void animatePulseBoxSequenceInternal(uint8_t iterations, bool travel, RenderMode rMode, ShaderMode sMode, CRGBPalette16 pal, uint32_t speedMs) {
        if (iterations == 0) return;
        TextureState tex(sMode, pal, 1.0f);
        for (int i = 0; i < iterations - 1; i++) animateFastCornerPulse(rMode, tex, travel, speedMs);
        for (int i = 0; i < 2; i++) animateFastCenterPulse(rMode, tex, speedMs);
    }

    

    struct ShapeConfig { DrawShapeFunc drawFunc; float staticRadius; float expandMaxRadius; float tumbleRadius; };

    static ShapeConfig getShapeConfig(ShapeType shape) {
        #ifdef TARGET_2026_CUBOID
            switch (shape) {
            case SHAPE_SPHERE:      return {(DrawShapeFunc)GeometryEngine::drawSphere, 3.5f, 4.0f, 3.5f};
            case SHAPE_OVOID:       return {(DrawShapeFunc)GeometryEngine::drawOvoid, 3.0f, 3.5f, 3.0f}; 
            case SHAPE_TETRAHEDRON: return {(DrawShapeFunc)GeometryEngine::drawTetrahedron, 4.0f, 4.0f, 3.5f};
            case SHAPE_OCTAHEDRON:  return {(DrawShapeFunc)GeometryEngine::drawOctahedron, 4.5f, 4.5f, 4.0f};
            case SHAPE_MERKABA:     return {(DrawShapeFunc)GeometryEngine::drawMerkaba, 3.5f, 3.5f, 3.5f};
            case SHAPE_TORUS:       return {(DrawShapeFunc)GeometryEngine::drawTorus, 3.0f, 3.0f, 3.0f};
            case SHAPE_CYLINDER:    return {(DrawShapeFunc)GeometryEngine::drawCylinder, 3.5f, 3.5f, 3.5f};
            case SHAPE_HEMISPHERES: return {(DrawShapeFunc)GeometryEngine::drawHemisphereHourglass, 3.5f, 3.5f, 3.5f};
            case SHAPE_BOX:         return {(DrawShapeFunc)GeometryEngine::drawBox, 3.5f, 4.0f, 3.5f};
            case SHAPE_HOURGLASS: return {(DrawShapeFunc)GeometryEngine::drawHourglass, 3.5f, 3.5f, 3.5f};
            default:                return {(DrawShapeFunc)GeometryEngine::drawSphere, 3.5f, 4.0f, 3.5f};
            }
        #else
            switch (shape) {
            case SHAPE_SPHERE:      return {(DrawShapeFunc)GeometryEngine::drawSphere, 8.0f, 10.0f, 8.0f};
            case SHAPE_OVOID:       return {(DrawShapeFunc)GeometryEngine::drawOvoid, 6.0f, 8.0f, 6.0f};
            case SHAPE_TETRAHEDRON: return {(DrawShapeFunc)GeometryEngine::drawTetrahedron, 7.5f, 7.5f, 7.5f};
            case SHAPE_OCTAHEDRON:  return {(DrawShapeFunc)GeometryEngine::drawOctahedron, 10.5f, 10.5f, 8.5f};
            case SHAPE_MERKABA:     return {(DrawShapeFunc)GeometryEngine::drawMerkaba, 6.5f, 6.5f, 6.5f};
            case SHAPE_TORUS:       return {(DrawShapeFunc)GeometryEngine::drawTorus, 5.5f, 5.5f, 5.5f};
            case SHAPE_CYLINDER:    return {(DrawShapeFunc)GeometryEngine::drawCylinder, 7.0f, 7.0f, 7.0f};
            case SHAPE_HEMISPHERES: return {(DrawShapeFunc)GeometryEngine::drawHemisphereHourglass, 7.5f, 7.5f, 7.5f};
            case SHAPE_BOX:         return {(DrawShapeFunc)GeometryEngine::drawBox, 7.5f, 7.5f, 7.5f};
            case SHAPE_HOURGLASS: return {(DrawShapeFunc)GeometryEngine::drawHourglass, 7.5f, 7.5f, 7.5f};
            default:                return {(DrawShapeFunc)GeometryEngine::drawSphere, 8.0f, 10.0f, 8.0f};
            }
        #endif
    }

    static void animateRollingBallInternal(uint32_t durationMs, RenderMode rMode, ShaderMode sMode, uint32_t speedMs, CRGBPalette16 pal) {
        uint32_t startTime = millis();
        uint32_t lastFrameTime = millis();
        
        float radius = (RNDR_X > 8) ? (RNDR_X / 4.0f) : 2.5f;
        float orbitRadius = (RNDR_Y / 2.0f) - radius;
        if (orbitRadius < 0.5f) orbitRadius = 0.5f; 

        float phaseOrbital = 0.0f;
        float phaseX = 0.0f;
        float rollAngle = 0.0f;

        TextureState tex(sMode, pal, 1.66f);

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrameTime;
            lastFrameTime = now;

            clearAll();
            
            float orbitSpeed = TWO_PI * 1.5f; 
            float xSweepSpeed = TWO_PI * 0.5f; 

            phaseOrbital += orbitSpeed * (deltaMs / 1000.0f);
            phaseX += xSweepSpeed * (deltaMs / 1000.0f);

            float currentY = RNDR_CY + (sin(phaseOrbital) * orbitRadius);
            float currentZ = RNDR_CZ + (cos(phaseOrbital) * orbitRadius);
            float currentX = RNDR_CX + (sin(phaseX) * orbitRadius);

            float spinSpeed = 300.0f; 
            rollAngle += spinSpeed * (deltaMs / 1000.0f);
            if (rollAngle >= 360.0f) rollAngle -= 360.0f;

            // Pass 1.0f explicitly for aspectZ at the end, bypassing any stretch logic
            GeometryEngine::drawSphere(currentX, currentY, currentZ, radius, rMode, tex, rollAngle, rollAngle, 0.0f, true);
            
            showCube();
            tex.advance(deltaMs);
            
            if (speedMs > 0) delay(speedMs); else yield();
        }
    }
    
public:
    struct BouncingBall {
        float px, py, pz;
        float vx, vy, vz;
        float rotX, rotY, rotZ;
    };

static void animateBouncingBall(uint32_t durationMs, uint32_t speedMs = 15) {
        uint32_t startTime = millis();
        uint32_t lastFrameTime = millis();
        
        const float SYSTEM_ENERGY = 1.35f; 
        
        // Hardcode a 5-voxel diameter (2.5 radius) for both architectures
        float radius = 2.5f; 
        
        const int MAX_BALLS = 3;
        // 3 Balls for the 16x16x16 Desktop, 1 Ball for the 8x8x16 Burn Cube
        int numBalls = (RNDR_X > 8) ? 3 : 1;

        BouncingBall balls[MAX_BALLS];
        TextureState tex[MAX_BALLS]; 
        RenderMode rMode[MAX_BALLS];

        // Safe bounds calculation (On an 8x8 matrix with a 5-wide ball, this leaves exactly 2 voxels of scatter room)
        float safeX = (RNDR_X - 1.0f) - (radius * 2.0f);
        float safeY = (RNDR_Y - 1.0f) - (radius * 2.0f);

        for (int i = 0; i < numBalls; i++) {
            // Spawn Physics: Safely scattered X/Y
            balls[i].px = radius + ((random8() / 255.0f) * safeX);
            balls[i].py = radius + ((random8() / 255.0f) * safeY);
            
            // All drop simultaneously from the ceiling
            balls[i].pz = (RNDR_Z - 1.0f) - radius; 
            
            balls[i].vx = (random16(20, 50) / 10.0f) * ((random8(2) == 0) ? 1.0f : -1.0f) * SYSTEM_ENERGY;
            balls[i].vy = (random16(20, 50) / 10.0f) * ((random8(2) == 0) ? 1.0f : -1.0f) * SYSTEM_ENERGY;
            balls[i].vz = 0.0f;
            balls[i].rotX = 0.0f; balls[i].rotY = 0.0f; balls[i].rotZ = 0.0f;

            // Autonomous Texture Assignment
            TexturePlan plan = PaletteUtils::getRandomTexturePlan();
            tex[i] = TextureState(plan.sMode, plan.palette, 1.66f);
            
            if (plan.sMode != MODE_SOLID) {
                tex[i].internalPhase = i * 128.0f;
                tex[i].phase = (uint32_t)tex[i].internalPhase;
            }

            // 70% Solid, 30% Shell for visual variety
            rMode[i] = (random8(100) < 30) ? RENDER_SHELL : RENDER_SOLID;
        }
        
        const float gravity = 38.0f * SYSTEM_ENERGY;     
        const float maxLateralSpeed = 8.0f * SYSTEM_ENERGY;

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrameTime;
            if (deltaMs == 0) { yield(); continue; } 
            lastFrameTime = now;
            
            float dt = deltaMs / 1000.0f;
            if (dt > 0.05f) dt = 0.05f; // Cap dt to prevent tunneling

            clearAll();

            for (int i = 0; i < numBalls; i++) {
                // Kinematics
                balls[i].vz -= gravity * dt; 
                balls[i].px += balls[i].vx * dt;
                balls[i].py += balls[i].vy * dt;
                balls[i].pz += balls[i].vz * dt;

                // Wall & Floor Collisions
                float maxZ = (RNDR_Z - 1) - radius;
                if (balls[i].px < radius) { balls[i].px = radius; balls[i].vx = -balls[i].vx; }
                else if (balls[i].px > RNDR_X - 1 - radius) { balls[i].px = RNDR_X - 1 - radius; balls[i].vx = -balls[i].vx; }

                if (balls[i].py < radius) { balls[i].py = radius; balls[i].vy = -balls[i].vy; }
                else if (balls[i].py > RNDR_Y - 1 - radius) { balls[i].py = RNDR_Y - 1 - radius; balls[i].vy = -balls[i].vy; }

                if (balls[i].pz < radius) {
                    balls[i].pz = radius; balls[i].vz = -balls[i].vz; 
                    balls[i].vx += ((random8(15) / 10.0f) - 0.7f) * SYSTEM_ENERGY; 
                    balls[i].vy += ((random8(15) / 10.0f) - 0.7f) * SYSTEM_ENERGY;
                    
                    if (balls[i].vx > maxLateralSpeed) balls[i].vx = maxLateralSpeed;
                    if (balls[i].vx < -maxLateralSpeed) balls[i].vx = -maxLateralSpeed;
                    if (balls[i].vy > maxLateralSpeed) balls[i].vy = maxLateralSpeed;
                    if (balls[i].vy < -maxLateralSpeed) balls[i].vy = -maxLateralSpeed;
                } else if (balls[i].pz > maxZ) {
                    balls[i].pz = maxZ; balls[i].vz = -balls[i].vz; 
                }

                // Visual Rotation
                balls[i].rotX += balls[i].vy * 45.0f * dt;
                balls[i].rotY += balls[i].vx * 45.0f * dt;

                // Render: EraseBackground = false, AspectZ = 1.0f
                GeometryEngine::drawSphere(balls[i].px, balls[i].py, balls[i].pz, radius, rMode[i], tex[i], balls[i].rotX, balls[i].rotY, balls[i].rotZ, false);
            }
            
            showCube();
            for (int i = 0; i < numBalls; i++) tex[i].advance(deltaMs);
            if (speedMs > 0) delay(speedMs); else yield();
        }
    }

    // ==========================================
    // THE 9 PRIMARY V2 APIS
    // ==========================================

    // --- animateStatic ---
    static void animateStatic(ShapeType shape, uint32_t durationMs, RenderMode rMode, CRGBPalette16 pal, ShaderMode sMode, uint32_t speedMs = 15, bool stretch = true) {
        pal = applyShapeColorHacks(shape, pal, sMode);
        rMode = applyShapeRenderHacks(shape, rMode);
        
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        
        float aspectZ = stretch ? ((RNDR_Z > RNDR_X) ? (RNDR_CZ / RNDR_CX) : 1.0f) : 1.0f;
        
        ShapeConfig cfg = getShapeConfig(shape);
        float radius = cfg.staticRadius;
        float zOff = (shape == SHAPE_OCTAHEDRON) ? 45.0f : 0.0f;

        int cx1 = (RNDR_X - 1) / 2, cx2 = RNDR_X / 2;
        int cy1 = (RNDR_Y - 1) / 2, cy2 = RNDR_Y / 2;
        int cz1 = (RNDR_Z - 1) / 2, cz2 = RNDR_Z / 2;
        float boxRad = (RNDR_X > 4) ? (RNDR_X / 2.0f) : 3.5f;
        int stepX = (int)boxRad, stepY = stepX, stepZ = stepX;
        #ifdef TARGET_2026_CUBOID
            if (stretch) { stepZ = stepX * 2; }
        #endif
        uint8_t x0 = max(0, cx1 - stepX), x1 = min((int)RNDR_X - 1, cx2 + stepX);
        uint8_t y0 = max(0, cy1 - stepY), y1 = min((int)RNDR_Y - 1, cy2 + stepY);
        uint8_t z0 = max(0, cz1 - stepZ), z1 = min((int)RNDR_Z - 1, cz2 + stepZ);

        TextureState tex(sMode, pal, 1.0f);

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrame;
            lastFrame = now;

            clearAll();
            
            if (shape == SHAPE_BOX) {
                GeometryEngine::drawFastBox(x0, y0, z0, x1, y1, z1, rMode, tex);
            } else {
                cfg.drawFunc(RNDR_CX, RNDR_CY, RNDR_CZ, radius, rMode, tex, 0.0f, 0.0f, zOff, true, aspectZ);
            }

            showCube();
            tex.advance(deltaMs);
            if (speedMs > 0) delay(speedMs); else yield();
        }
    }
    //If you just pass an HTMLColorCode, it implies SOLID
    static void animateStatic(ShapeType shape, uint32_t durationMs, RenderMode rMode, CRGB::HTMLColorCode colorCode, uint32_t speedMs = 15, bool stretch = true) {
        animateStatic(shape, durationMs, rMode, CRGBPalette16(CRGB(colorCode)), MODE_SOLID, speedMs, stretch);
    }
    //If you just pass a ShaderMode for something like hiphotic, it implies an ignored palette
    static void animateStatic(ShapeType shape, uint32_t durationMs, RenderMode rMode, ShaderMode sMode, uint32_t speedMs = 15, bool stretch = true) {
        animateStatic(shape, durationMs, rMode, RainbowColors_p, sMode, speedMs, stretch);
    }


    // --- animateExpandingCollapsing ---
    static void animateExpandingCollapsing(ShapeType shape, uint32_t durationMs, RenderMode rMode, CRGBPalette16 pal, ShaderMode sMode, uint32_t speedMs = 15, bool stretch = true) {
        pal = applyShapeColorHacks(shape, pal, sMode);
        rMode = applyShapeRenderHacks(shape, rMode);
        if (shape == SHAPE_BOX) { 
            animateExpandingCollapsingBoxInternal(durationMs, rMode, sMode, speedMs, pal, stretch); 
            return; 
        }
        ShapeConfig cfg = getShapeConfig(shape);
        float zOff = (shape == SHAPE_OCTAHEDRON) ? 45.0f : 0.0f;
        animateExpandingCollapsingBase(cfg.drawFunc, cfg.expandMaxRadius, durationMs, rMode, sMode, speedMs, pal, false, 0.0f, 0.0f, 0.0f, zOff, stretch);
    }
    //If you just pass an HTMLColorCode, it implies SOLID
    static void animateExpandingCollapsing(ShapeType shape, uint32_t durationMs, RenderMode rMode, CRGB::HTMLColorCode colorCode, uint32_t speedMs = 15, bool stretch = true) {
        animateExpandingCollapsing(shape, durationMs, rMode, CRGBPalette16(CRGB(colorCode)), MODE_SOLID, speedMs, stretch);
    }
    //If you just pass a ShaderMode for something like hiphotic, it implies an ignored palette
    static void animateExpandingCollapsing(ShapeType shape, uint32_t durationMs, RenderMode rMode, ShaderMode sMode, uint32_t speedMs = 15, bool stretch = true) {
        animateExpandingCollapsing(shape, durationMs, rMode, RainbowColors_p, sMode, speedMs, stretch);
    }

    // --- animateTumbling ---
    static void animateTumbling(ShapeType shape, uint32_t durationMs, RenderMode rMode, CRGBPalette16 pal, ShaderMode sMode, float tumbleSpeed = 1.0f, bool stretch = true) {
        pal = applyShapeColorHacks(shape, pal, sMode);
        rMode = applyShapeRenderHacks(shape, rMode);
        ShapeConfig cfg = getShapeConfig(shape);
        animateStaticShape(cfg.drawFunc, cfg.tumbleRadius, durationMs, rMode, sMode, pal, true, tumbleSpeed, 0.0f, 0.0f, 0.0f, stretch);
    }
    //If you just pass an HTMLColorCode, it implies SOLID
    static void animateTumbling(ShapeType shape, uint32_t durationMs, RenderMode rMode, CRGB::HTMLColorCode colorCode, float tumbleSpeed = 1.0f, bool stretch = true) {
        animateTumbling(shape, durationMs, rMode, CRGBPalette16(CRGB(colorCode)), MODE_SOLID, tumbleSpeed, stretch);
    }
    //If you just pass a ShaderMode for something like hiphotic, it implies an ignored palette
    static void animateTumbling(ShapeType shape, uint32_t durationMs, RenderMode rMode, ShaderMode sMode, float tumbleSpeed = 1.0f, bool stretch = true) {
        animateTumbling(shape, durationMs, rMode, RainbowColors_p, sMode, tumbleSpeed, stretch);
    }

    // --- animateSpinning ---
    static void animateSpinning(ShapeType shape, uint32_t durationMs, float speed, RenderMode rMode, CRGBPalette16 pal, ShaderMode sMode, bool stretch = true) {
        pal = applyShapeColorHacks(shape, pal, sMode);
        rMode = applyShapeRenderHacks(shape, rMode);
        
        // Added SHAPE_MERKABA to the allow-list
        if (shape != SHAPE_OCTAHEDRON && shape != SHAPE_TORUS && shape != SHAPE_MERKABA) return;
        
        ShapeConfig cfg = getShapeConfig(shape);
        float sx = speed;
        float sy = speed;
        float sz = speed;
        
        // Disables the 'rotate' flag and passes the explicit sz (Z-axis) speed
        animateStaticShape(cfg.drawFunc, cfg.tumbleRadius, durationMs, rMode, sMode, pal, false, 1.0f, 0, 0, sz, stretch);
    }
    //If you just pass an HTMLColorCode, it implies SOLID
    static void animateSpinning(ShapeType shape, uint32_t durationMs, float speed, RenderMode rMode, CRGB::HTMLColorCode colorCode, bool stretch = true) {
        animateSpinning(shape, durationMs, speed, rMode, CRGBPalette16(CRGB(colorCode)), MODE_SOLID, stretch);
    }
    //If you just pass a ShaderMode for something like hiphotic, it implies an ignored palette
    static void animateSpinning(ShapeType shape, uint32_t durationMs, float speed, RenderMode rMode, ShaderMode sMode, bool stretch = true) {
        animateSpinning(shape, durationMs, speed, rMode, RainbowColors_p, sMode, stretch);
    }

    // --- animateRollingBall ---
    static void animateRollingBall(uint32_t durationMs, RenderMode rMode, CRGBPalette16 pal, ShaderMode sMode, uint32_t speedMs = 15) {
        pal = applyShapeColorHacks(SHAPE_SPHERE, pal, sMode);
        rMode = applyShapeRenderHacks(SHAPE_SPHERE, rMode);
        animateRollingBallInternal(durationMs, rMode, sMode, speedMs, pal);
    }    
    // If you just pass an HTMLColorCode, it implies SOLID
    static void animateRollingBall(uint32_t durationMs, RenderMode rMode, CRGB::HTMLColorCode colorCode, uint32_t speedMs = 15) {
        animateRollingBall(durationMs, rMode, CRGBPalette16(CRGB(colorCode)), MODE_SOLID, speedMs);
    }
    // If you just pass a ShaderMode for something like hiphotic, it implies an ignored palette
    static void animateRollingBall(uint32_t durationMs, RenderMode rMode, ShaderMode sMode, uint32_t speedMs = 15) {
        animateRollingBall(durationMs, rMode, RainbowColors_p, sMode, speedMs);
    }
       // --- animateSpring ---
    static void animateSpring(ShapeType shape, uint32_t durationMs, RenderMode rMode, CRGBPalette16 pal, ShaderMode sMode, uint32_t speedMs = 15, bool stretch = false) {
        pal = applyShapeColorHacks(shape, pal, sMode);
        rMode = applyShapeRenderHacks(shape, rMode);
        ShapeConfig cfg = getShapeConfig(shape);
        DrawShapeFunc draw = cfg.drawFunc;
        if (shape == SHAPE_BOX) draw = (DrawShapeFunc)GeometryEngine::drawBox;
        animateSpringBase(draw, cfg.expandMaxRadius, durationMs, rMode, sMode, speedMs, pal, false, 0.0f, 0.0f, 0.0f, stretch);
    }
    //If you just pass an HTMLColorCode, it implies SOLID
    static void animateSpring(ShapeType shape, uint32_t durationMs, RenderMode rMode, CRGB::HTMLColorCode colorCode, uint32_t speedMs = 15, bool stretch = false) {
        animateSpring(shape, durationMs, rMode, CRGBPalette16(CRGB(colorCode)), MODE_SOLID, speedMs, stretch);
    }
    //If you just pass a ShaderMode for something like hiphotic, it implies an ignored palette
    static void animateSpring(ShapeType shape, uint32_t durationMs, RenderMode rMode, ShaderMode sMode, uint32_t speedMs = 15, bool stretch = false) {
        animateSpring(shape, durationMs, rMode, RainbowColors_p, sMode, speedMs, stretch);
    }

    // --- animateFlyingBox ---
    static void animateFlyingBox(uint32_t durationMs, uint32_t speedMs) {
        uint32_t startTime = millis();
        
        while (millis() - startTime < durationMs) {
            bool travel = (random8(2) == 0);
            RenderMode rMode = (RenderMode)random8(3);
            
            // Ask the engine for a guaranteed good aesthetic pairing
            TexturePlan plan = PaletteUtils::getRandomTexturePlan();
            TextureState tex(plan.sMode, plan.palette, 1.0f);
            
            animateFastCornerPulse(rMode, tex, travel, speedMs);
        }
    }

    // --- animatePulseBoxSequence ---
    static void animatePulseBoxSequence(uint8_t iterations, bool travel, RenderMode rMode, CRGBPalette16 pal, ShaderMode sMode, uint32_t speedMs = 0) {
        pal = applyShapeColorHacks(SHAPE_BOX, pal, sMode);
        animatePulseBoxSequenceInternal(iterations, travel, rMode, sMode, pal, speedMs);
    }
    //If you just pass an HTMLColorCode, it implies SOLID
    static void animatePulseBoxSequence(uint8_t iterations, bool travel, RenderMode rMode, CRGB::HTMLColorCode colorCode, uint32_t speedMs = 0) {
        animatePulseBoxSequence(iterations, travel, rMode, CRGBPalette16(CRGB(colorCode)), MODE_SOLID, speedMs);
    }
    //If you just pass a ShaderMode for something like hiphotic, it implies an ignored palette
    static void animatePulseBoxSequence(uint8_t iterations, bool travel, RenderMode rMode, ShaderMode sMode, uint32_t speedMs = 0) {
        animatePulseBoxSequence(iterations, travel, rMode, RainbowColors_p, sMode, speedMs);
    }

    static void animateMovingBoxes(uint32_t durationMs, uint32_t speedMs) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        //float r = RNDR_X / 8.0f; 
        float r = (RNDR_X > 8) ? (RNDR_X / 8.0f) : 1.5f;

        float x[3], y[3], z[3];
        float *u[3], *v[3];
        float maxU[3], maxV[3];
        uint8_t edge[3] = {0, 0, 0};
        
        // 1. Store the randomized plans
        TexturePlan plan[3];

        for (int i = 0; i < 3; i++) {
            x[i] = random16(r * 10, (RNDR_X - 1 - r) * 10) / 10.0f;
            y[i] = random16(r * 10, (RNDR_Y - 1 - r) * 10) / 10.0f;
            z[i] = random16(r * 10, (RNDR_Z - 1 - r) * 10) / 10.0f;
            
            // Generate a guaranteed good shader/palette pairing for each box
            plan[i] = PaletteUtils::getRandomTexturePlan();
            bool reverse = (random8(2) == 0);

            if (i == 0) { 
                u[i] = reverse ? &y[i] : &x[i]; v[i] = reverse ? &x[i] : &y[i];
                maxU[i] = reverse ? (RNDR_Y - 1 - r) : (RNDR_X - 1 - r); maxV[i] = reverse ? (RNDR_X - 1 - r) : (RNDR_Y - 1 - r);
            } else if (i == 1) { 
                u[i] = reverse ? &z[i] : &x[i]; v[i] = reverse ? &x[i] : &z[i];
                maxU[i] = reverse ? (RNDR_Z - 1 - r) : (RNDR_X - 1 - r); maxV[i] = reverse ? (RNDR_X - 1 - r) : (RNDR_Z - 1 - r);
            } else { 
                u[i] = reverse ? &z[i] : &y[i]; v[i] = reverse ? &y[i] : &z[i];
                maxU[i] = reverse ? (RNDR_Z - 1 - r) : (RNDR_Y - 1 - r); maxV[i] = reverse ? (RNDR_Y - 1 - r) : (RNDR_Z - 1 - r);
            }
        }

        // 2. Initialize the persistent states BEFORE the loop so they can animate their phases
        TextureState tex[3] = {
            TextureState(plan[0].sMode, plan[0].palette, 1.0f),
            TextureState(plan[1].sMode, plan[1].palette, 1.0f),
            TextureState(plan[2].sMode, plan[2].palette, 1.0f)
        };

        float step = 0.5f;

        while (millis() - startTime < durationMs) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrame;
            lastFrame = now;

            clearAll();

            for (int i = 0; i < 3; i++) {
                if (edge[i] == 0) {
                    *u[i] += step; if (*u[i] >= maxU[i]) { *u[i] = maxU[i]; edge[i] = 1; }
                } else if (edge[i] == 1) {
                    *v[i] += step; if (*v[i] >= maxV[i]) { *v[i] = maxV[i]; edge[i] = 2; }
                } else if (edge[i] == 2) {
                    *u[i] -= step; if (*u[i] <= r) { *u[i] = r; edge[i] = 3; }
                } else if (edge[i] == 3) {
                    *v[i] -= step; if (*v[i] <= r) { *v[i] = r; edge[i] = 0; }
                }

                // 3. Render and advance phase
                uint8_t x0 = max(0, (int)round(x[i] - r));
                uint8_t x1 = min((int)RNDR_X - 1, (int)round(x[i] + r));
                uint8_t y0 = max(0, (int)round(y[i] - r));
                uint8_t y1 = min((int)RNDR_Y - 1, (int)round(y[i] + r));
                uint8_t z0 = max(0, (int)round(z[i] - r));
                uint8_t z1 = min((int)RNDR_Z - 1, (int)round(z[i] + r));

                GeometryEngine::drawFastBox(x0, y0, z0, x1, y1, z1, RENDER_SOLID, tex[i]);
                tex[i].advance(deltaMs);
            }

            showCube();
            if (speedMs > 0) delay(speedMs); else yield();
        }
    }
};