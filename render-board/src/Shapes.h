#pragma once
#include "TestDemo.h"
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
    friend class Boxes;
    friend class Balls;

public:
    struct FrameControl {
        // May update the texture. Return a multiplier for this frame's
        // rotation advance, with 1.0 preserving the normal rate.
        float (*update)(TextureState &, uint32_t deltaMs, void *state);
        bool (*keepRunning)();
        void *state;
        void (*drawOverlay)(void *state);
    };

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
                if (shape == SHAPE_MERKABA) return PaletteUtils::getMerkabaPalette(true);
                return CRGBPalette16(PaletteUtils::getRandomVibrantColor());
            }
            
            if (isGray) {
                if (shape == SHAPE_MERKABA) return PaletteUtils::getMerkabaPalette(false);
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
    static void animateStaticShape(DrawShapeFunc drawFunc, float fixedRadius, uint32_t durationMs, RenderMode rMode, ShaderMode sMode, CRGBPalette16 pal, bool rotate = false, float tumbleSpeed = 1.0f, float customX = 0, float customY = 0, float customZ = 0, bool stretch = false, const FrameControl *frameControl = nullptr) {
        uint32_t startTime = millis();
        uint32_t lastFrameTime = millis();
        
        float geomPhase = 0.0f; 
        float aspectZ = stretch ? ((RNDR_Z > RNDR_X) ? (RNDR_CZ / RNDR_CX) : 1.0f) : 1.0f;
        
        TextureState tex(sMode, pal, 1.0f);

        while (frameControl ? frameControl->keepRunning() :
               TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrameTime;
            lastFrameTime = now;
            
            const float rate = frameControl ? frameControl->update(tex, deltaMs, frameControl->state) : 1.0f;
            geomPhase += 0.2f * deltaMs * rate;

            clearAll();
            const auto angles = rotate ? GeometryEngine::tumbleAngles(geomPhase, tumbleSpeed) :
                GeometryEngine::Point3{tumbleSpeed == 0.0f ? customX : geomPhase * customX,
                                       tumbleSpeed == 0.0f ? customY : geomPhase * customY,
                                       tumbleSpeed == 0.0f ? customZ : geomPhase * customZ};
            drawFunc(RNDR_CX, RNDR_CY, RNDR_CZ, fixedRadius, rMode, tex,
                     angles.x, angles.y, angles.z, true, aspectZ);
            if (frameControl && frameControl->drawOverlay) frameControl->drawOverlay(frameControl->state);
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

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
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

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
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

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
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
            case SHAPE_CYLINDER:    return {GeometryEngine::drawCylinder, 3.5f, 3.5f, 3.5f};
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
            case SHAPE_CYLINDER:    return {GeometryEngine::drawCylinder, 7.0f, 7.0f, 7.0f};
            case SHAPE_HEMISPHERES: return {(DrawShapeFunc)GeometryEngine::drawHemisphereHourglass, 7.5f, 7.5f, 7.5f};
            case SHAPE_BOX:         return {(DrawShapeFunc)GeometryEngine::drawBox, 7.5f, 7.5f, 7.5f};
            case SHAPE_HOURGLASS: return {(DrawShapeFunc)GeometryEngine::drawHourglass, 7.5f, 7.5f, 7.5f};
            default:                return {(DrawShapeFunc)GeometryEngine::drawSphere, 8.0f, 10.0f, 8.0f};
            }
        #endif
    }

public:
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

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
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
    static void animateTumbling(ShapeType shape, uint32_t durationMs, RenderMode rMode, CRGBPalette16 pal, ShaderMode sMode, float tumbleSpeed = 1.0f, bool stretch = true, const FrameControl *frameControl = nullptr) {
        pal = applyShapeColorHacks(shape, pal, sMode);
        rMode = applyShapeRenderHacks(shape, rMode);
        ShapeConfig cfg = getShapeConfig(shape);
        animateStaticShape(cfg.drawFunc, cfg.tumbleRadius, durationMs, rMode, sMode, pal, true, tumbleSpeed, 0.0f, 0.0f, 0.0f, stretch, frameControl);
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
    static void animateSpinning(ShapeType shape, uint32_t durationMs, float speed, RenderMode rMode, CRGBPalette16 pal, ShaderMode sMode, bool stretch = true, const FrameControl *frameControl = nullptr) {
        pal = applyShapeColorHacks(shape, pal, sMode);
        rMode = applyShapeRenderHacks(shape, rMode);
        
        // Added SHAPE_MERKABA to the allow-list
        if (shape != SHAPE_OCTAHEDRON && shape != SHAPE_TORUS && shape != SHAPE_MERKABA) return;
        
        ShapeConfig cfg = getShapeConfig(shape);
        // Disables the 'rotate' flag and passes the explicit sz (Z-axis) speed
        animateStaticShape(cfg.drawFunc, cfg.tumbleRadius, durationMs, rMode, sMode, pal, false, 1.0f, 0, 0, speed, stretch, frameControl);
    }
    //If you just pass an HTMLColorCode, it implies SOLID
    static void animateSpinning(ShapeType shape, uint32_t durationMs, float speed, RenderMode rMode, CRGB::HTMLColorCode colorCode, bool stretch = true) {
        animateSpinning(shape, durationMs, speed, rMode, CRGBPalette16(CRGB(colorCode)), MODE_SOLID, stretch);
    }
    //If you just pass a ShaderMode for something like hiphotic, it implies an ignored palette
    static void animateSpinning(ShapeType shape, uint32_t durationMs, float speed, RenderMode rMode, ShaderMode sMode, bool stretch = true) {
        animateSpinning(shape, durationMs, speed, rMode, RainbowColors_p, sMode, stretch);
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
};
