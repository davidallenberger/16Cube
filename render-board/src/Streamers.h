#pragma once
#include "AppMemory.h"
#include "CubeCore.h"
#include "GeometryEngine.h"
#include "ParticleEffectSupport.h"
#include "StreamerGeometry.h"
#include "TestDemo.h"
#include "TextureEngine.h"

#include <math.h>

enum ChunchunMode {
    CHUNCHUN_SERPENTINE = 0, // Literal 1D raster sweep extruded to 3D columns
    CHUNCHUN_LISSAJOUS = 1   // Continuous multi-axis figure-8 weave
};

class Streamers {
    static void drawSpray(ParticleEngine::Pool& pool, const CRGBPalette16& palette, bool wrapX) {
        for (int i = 0; i < pool.size(); ++i) {
            const auto& p = pool[i];
            if (!p.active) continue;
            const uint8_t brightness = uint8_t(255.0f * min(1.0f, 2.0f * p.life));
            ParticleEngine::drawSmooth(p.motion.position, ColorFromPalette(palette, p.type, brightness), wrapX);
        }
    }

    static void resolveSprayTop(ParticleEngine::Pool& pool, ParticleEngine::Side side, float bounce) {
        for (int i = 0; i < pool.size(); ++i) {
            auto& p = pool[i];
            if (!p.active || p.motion.position.z <= RNDR_Z - 1.0f) continue;
            p.active = ParticleEngine::resolveSide(p.motion.position.z, p.motion.velocity.z, RNDR_Z, side, bounce);
        }
    }

public:
    struct GhostRiderSettings {
        uint8_t speed = 70;
        uint8_t spiral = 130;
        uint8_t blur = 220;
        uint8_t colorCycle = 30;
        uint8_t spread = 35; // The cube can exceed WLED's 0–31 range.
        bool ageColor = true;
        bool walls = false;
    };

    struct SpraySettings {
        uint8_t speed = 150, intensity = 150, leftRight = 220, upDown = 30, angle = 21;
        bool gravity = false, cylinder = false, collide = false;
    };

    static void animateGhostRider(uint32_t durationMs) {
        animateGhostRider(durationMs, GhostRiderSettings{});
    }

    static void animateGhostRider(uint32_t durationMs, const GhostRiderSettings& settings,
                                   const CRGBPalette16& palette = RainbowColors_p) {
        using namespace ParticleEffectSupport;
        const Side side = settings.walls ? Side::Bounce : Side::Wrap;
        const float speed = (5.0f + settings.speed * 0.09f) * cbrtf(volumeScale);
        const float spread = settings.spread * 0.095f * cbrtf(volumeScale);
        const float maxTurn = settings.spiral * 0.0024f;
        float yaw = sample(0, TWO_PI);
        float turn = 0.0f;
        float curlDirection = 1.0f;
        float elevationPhase = sample(0, TWO_PI);
        float verticalDirection = 1.0f;
        uint8_t hue = random8();
        float emission = 0.0f;
        Pool pool(population(150));
        Motion head{{sample(0, RNDR_X - 1), sample(0, RNDR_Y - 1), sample(0, RNDR_Z - 1)}, {}};
        run(durationMs, settings.blur, [&](uint32_t tick) {
            // As in WLED, the head steers by varying the turn rate. A second
            // phase tilts the heading through depth instead of drawing a
            // fixed helix. The already emitted trail keeps its own velocity.
            turn += curlDirection * settings.spiral * 0.000015f;
            if (fabsf(turn) >= maxTurn) curlDirection = -curlDirection;
            yaw += turn;
            // Let pitch evolve more slowly than the horizontal curl so
            // the head travels through the height as it spirals.
            elevationPhase += 0.012f + 0.025f * fabsf(turn);
            if (elevationPhase >= TWO_PI) elevationPhase -= TWO_PI;
            const float elevation = sinf(elevationPhase) * 0.68f * verticalDirection;
            head.velocity = GeometryEngine::helixPoint({0, 0, 0}, cosf(elevation) * speed,
                                                       yaw, sinf(elevation) * speed);
            advance(head, dt, {0, 0, 0}, 1.0f);
            const Point3 incoming = head.velocity;
            resolveSide(head.position.x, head.velocity.x, RNDR_X, side, 1.0f);
            resolveSide(head.position.y, head.velocity.y, RNDR_Y, side, 1.0f);
            resolveSide(head.position.z, head.velocity.z, RNDR_Z, side, 1.0f);
            if (incoming.x != head.velocity.x || incoming.y != head.velocity.y) {
                yaw = atan2f(head.velocity.y, head.velocity.x);
            }
            if (incoming.z != head.velocity.z) verticalDirection = -verticalDirection;
            if (tick % (11 - settings.colorCycle / 25) == 0) ++hue;
            if (settings.colorCycle > 190) hue += (settings.colorCycle - 190) >> 2;

            stepSpray(pool, Side::Open, Side::Open, false, 0, 0, false);
            for (int i = 0; i < pool.size(); ++i) {
                auto& p = pool[i];
                if (p.active && p.motion.position.z >= RNDR_Z) p.active = false;
            }
            emission += 1.5f * volumeScale;
            while (emission >= 1.0f) {
                emission -= 1.0f;
                const auto& v = head.velocity;
                const Point3 velocity{-v.x * 0.72f + sample(-spread, spread),
                                      -v.y * 0.72f + sample(-spread, spread),
                                      -v.z * 0.72f + sample(-spread, spread)};
                pool.spawn({true, hue, {head.position, velocity}, 0.995f, 0.0f,
                            1.0f, 1.0f / sample(0.75f, 1.12f), CRGB::Black, false});
            }
        }, [&] {
            for (int i = 0; i < pool.size(); ++i) {
                const auto& p = pool[i];
                if (!p.active) continue;
                const uint8_t index = settings.ageColor ? hue + int(p.life * 192.0f) : p.type;
                drawSmooth(p.motion.position,
                           ColorFromPalette(palette, index, uint8_t(p.life * 240.0f)), false);
            }
            drawSmooth(head.position, CRGB::White, !settings.walls);
        });
    }

    static void animateSpray(uint32_t durationMs) {
        animateSpray(durationMs, SpraySettings{});
    }

    static void animateSpray(uint32_t durationMs, const SpraySettings& settings,
                             const CRGBPalette16& palette = PartyColors_p) {
        using namespace ParticleEffectSupport;
        const Point3 nozzle{(RNDR_X - 1) * (settings.leftRight / 255.0f), RNDR_CY,
                            (RNDR_Z - 1) * (settings.upDown / 255.0f)};
        const float angle = TWO_PI * (1.0f - (min(31, int(settings.angle)) + 1) / 32.0f);
        const Side xSide = settings.cylinder ? Side::Wrap : Side::Bounce;
        constexpr float hardness = 200.0f / 255.0f;
        Pool pool(population(maxParticles));
        uint32_t frame = 0;
        uint8_t hue = random8();
        float emission = 1.0f - volumeScale;
        run(durationMs, 200, [&](uint32_t tick) {
            if (wledFrameDue(tick)) {
                const int period = 11 - settings.intensity / 25;
                if (frame % period == 0) {
                    emission += volumeScale;
                    if (emission >= 1.0f) {
                        emission -= 1.0f;
                        const float speed = (settings.speed >> 2) * wledVelocity;
                        constexpr int spread = 3;
                        const float horizontal = speed * cosf(angle) * 0.70710678f;
                        const Point3 v{horizontal + sample(-spread, spread) * wledVelocity,
                                       horizontal + sample(-spread, spread) * wledVelocity,
                                       speed * sinf(angle) + sample(-spread, spread) * wledVelocity};
                        const float life = sample(150 + settings.intensity, 300 + settings.intensity);
                        ++hue;
                        pool.spawn({true, hue, {nozzle, v}, 1.0f, settings.gravity ? 8 * wledGravity : 0.0f,
                                    1.0f, 42.0f / life, CRGB::Black, 0});
                    }
                }
                ++frame;
            }
            stepSpray(pool, xSide, Side::Bounce, true, hardness, 0.0f, settings.collide);
            if (!settings.gravity) resolveSprayTop(pool, Side::Bounce, hardness);
        }, [&] { drawSpray(pool, palette, settings.cylinder); });
    }

    static void animateVerticalPipes(uint32_t durationMs, uint32_t speedMs) {
        uint32_t startTime = millis(), lastFrame = millis(); float masterHue = 0.0f;
        uint8_t px[50], py[50], pz[50], pDir[50]; int activeCount = 0;
        for (uint8_t x = 0; x < RNDR_X; x++) { for (uint8_t y = 0; y < RNDR_Y; y++) { if (random8(7) == 0 && activeCount < 50) { px[activeCount] = x; py[activeCount] = y; pz[activeCount] = 0; pDir[activeCount] = random8(3); activeCount++; } } }
        clearAll(); 
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now; masterHue -= 0.133f * deltaMs; 
            CRGB currentFrameColor = CHSV((uint8_t)masterHue, 255, 255);
            for (int i = 0; i < activeCount; i++) {
                if (random8(3) == 0) { pDir[i] = random8(3); for (int z = 0; z < RNDR_Z; z++) setVoxel(px[i], py[i], z, CRGB::Black); }
                if (pDir[i] == 1) { if (pz[i] < RNDR_Z - 1) pz[i]++; else { pz[i]--; pDir[i] = 2; } } else if (pDir[i] == 2) { if (pz[i] > 0) pz[i]--; else { pz[i]++; pDir[i] = 1; } }
                setVoxel(px[i], py[i], pz[i], currentFrameColor);
            }
            showCube(); if (speedMs > 0) delay(speedMs); else yield();
        }
    }

  static void animateHula(uint32_t durationMs, uint32_t speedMs, CRGBPalette16 pal = RainbowColors_p, float revsPerSec = 1.5f) {
        uint32_t startTime = millis(), lastFrameTime = millis(); float globalAngle = 0.0f, colorPhase = 0.0f, midZ = (RNDR_Z - 1) / 2.0f;
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrameTime; lastFrameTime = now; clearAll();
            float s = sin(globalAngle), c = cos(globalAngle);
            for (int z = 0; z < RNDR_Z; z++) {
                float dz = z - midZ, dist = (dz < 0) ? -dz : dz, size = (dist / midZ) * (RNDR_X / 2.0f) + (RNDR_X / 2.0f);
                uint8_t layerColorIndex = (uint8_t)colorPhase + (z * 10); CRGB layerColor = ColorFromPalette(pal, layerColorIndex, 255, LINEARBLEND);
                for (int x = 0; x < RNDR_X; x++) { for (int y = 0; y < RNDR_Y; y++) { float ux = ((x - RNDR_CX) * c) + ((y - RNDR_CY) * s), uy = -((x - RNDR_CX) * s) + ((y - RNDR_CY) * c), origX = ux + RNDR_CX, origY = uy + RNDR_CY; if (origX >= 0.0f && origX < size && origY >= 0.0f && origY < size) { setVoxel(x, y, z, layerColor); } } }
            }
            showCube(); globalAngle += (revsPerSec * TWO_PI) * (deltaMs / 1000.0f); colorPhase += 100.0f * (deltaMs / 1000.0f); if (speedMs > 0) delay(speedMs); else yield();
        }
    }


    // ==========================================
    // WHAT THE HELIX (Continuous Capsule Sweeping)
    // ==========================================
    static void animateWhatTheHelix(uint32_t durationMs, uint32_t speedMs) {
        uint32_t startTime = millis();
        uint32_t lastFrameTime = millis();
        
        float radius = (RNDR_X > 8) ? (RNDR_X / 2.0f) - 1.5f : 3.0f; 
        float thickness = (RNDR_X > 8) ? 1.5f : 1.25f; 

        // TRANSLATE THE DELAY INTO A SMOOTH PHYSICS MULTIPLIER
        // Baseline is 50. If speedMs is 100 (Burn Cube), physics run at 0.5x speed.
        float physicsMultiplier = 50.0f / (float)(speedMs > 0 ? speedMs : 50);

        // 1. THE ELEGANT MASTER CLOCK 
        float masterPhase = 0.0f;
        uint32_t pauseEndTime = 0; 

        static uint8_t helixStyle = 255;
        helixStyle = (helixStyle + 1) % 4; 
        
        TexturePlan plan = PaletteUtils::getRandomNonSolidTexturePlan(true);
        PaletteUtils::logTexture("What The Helix", plan);
        TextureState tex(plan.sMode, plan.palette, 1.0f);

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrameTime;
            if (deltaMs == 0) { yield(); continue; } 
            lastFrameTime = now;

            clearAll();

            // 2. TORSION PENDULUM PHYSICS WITH TIME-STOP
            if (now < pauseEndTime) {
                // Time is frozen. The frame renders perfectly still.
            } else {
                float oldPhase = masterPhase;
                
                // APPLY THE PHYSICS MULTIPLIER TO THE TIME DELTA
                masterPhase += ((TWO_PI / 5500.0f) * deltaMs) * physicsMultiplier; 
                
                // If we just crossed the apex of the swing, snap to it and freeze time
                if (oldPhase < (PI / 2.0f) && masterPhase >= (PI / 2.0f)) {
                    masterPhase = (PI / 2.0f);
                    pauseEndTime = now + (uint32_t)(250 / physicsMultiplier); // Hang-time scales too!
                } else if (oldPhase < (3.0f * PI / 2.0f) && masterPhase >= (3.0f * PI / 2.0f)) {
                    masterPhase = (3.0f * PI / 2.0f);
                    pauseEndTime = now + (uint32_t)(250 / physicsMultiplier); 
                }
                
                if (masterPhase >= TWO_PI) masterPhase -= TWO_PI;
            }

            // Twist Phase: Over-amplify and clamp 
            float rawTwist = sin(masterPhase) * 1.5f;
            
            // INCREASED COILING: (PI * 3.0f) / 8.0f (Creates 3 full coils)
            float twistPhase = constrain(rawTwist, -1.0f, 1.0f) * ((PI * 3.0f) / 8.0f);

            // Rotation Phase: 2.5 full rotations before reversing
            float rotPhase = 5.0f * PI * sin(masterPhase);
            
            // 3. FLAWLESS VOLUMETRIC RENDERING (Continuous Capsule Sweeping)
            for (int z = 0; z < RNDR_Z; z++) {
                
                // Calculate exactly where the helix enters and exits this Z-layer
                float normZ_top = (float)z - (RNDR_Z / 2.0f) - 0.5f;
                float normZ_bot = (float)z - (RNDR_Z / 2.0f) + 0.5f;
                
                float angleTop = rotPhase + (normZ_top * twistPhase);
                float angleBot = rotPhase + (normZ_bot * twistPhase);
                
                const GeometryEngine::Point3 center{RNDR_CX, RNDR_CY, float(z)};
                const auto top = GeometryEngine::helixPoint(center, radius, angleTop, -0.5f);
                const auto bottom = GeometryEngine::helixPoint(center, radius, angleBot, 0.5f);
                const float cx_top = top.x, cy_top = top.y;
                const float cx_bot = bottom.x, cy_bot = bottom.y;
                
                // Calculate the 2D trajectory segment through this layer
                float dx_seg = cx_bot - cx_top;
                float dy_seg = cy_bot - cy_top;
                float segLengthSq = (dx_seg * dx_seg) + (dy_seg * dy_seg);

                for (int x = 0; x < RNDR_X; x++) {
                    for (int y = 0; y < RNDR_Y; y++) {
                        float dist;
                        
                        // Find the shortest distance from the voxel to the trajectory line segment
                        if (segLengthSq < 0.001f) {
                            float dx = (float)x - cx_top;
                            float dy = (float)y - cy_top;
                            dist = sqrtf(dx*dx + dy*dy);
                        } else {
                            float t = (((float)x - cx_top) * dx_seg + ((float)y - cy_top) * dy_seg) / segLengthSq;
                            t = fmaxf(0.0f, fminf(1.0f, t)); // Constrain to the physical segment bounds
                            
                            float projX = cx_top + (t * dx_seg);
                            float projY = cy_top + (t * dy_seg);
                            
                            float dx = (float)x - projX;
                            float dy = (float)y - projY;
                            dist = sqrtf(dx*dx + dy*dy);
                        }
                        
                        float intensity = constrain(thickness - dist, 0.0f, 1.0f);
                        
                        if (intensity > 0.0f) {
                            CRGB color = tex.getColor(x, y, z);
                            
                            // Anti-aliasing scaling
                            color.nscale8((uint8_t)(intensity * 255.0f)); 
                            
                            CRGB existing = getVoxel(x, y, z);
                            setVoxel(x, y, z, existing + color);
                        }
                    }
                }
            }

            showCube();
            // The Texture clock NEVER stops, keeping the paint boiling!
            tex.advance(deltaMs);
            yield(); // Smooth hardware yield, NO artificial delays
        }
    }
    // ==========================================
    // VERTICAL STREAMER (Internal Engine)
    // ==========================================
    static void animateVerticalStreamer(uint32_t durationMs, uint32_t speedMs, float volumePercent, CRGBPalette16 pal,ShaderMode sMode) {
        const uint8_t count = 4;
        const uint32_t startTime = millis();
        float masterPhase = 0.0f;
        // Divide the footprint among four streamers. On 16x16 this gives
        // each the same brush radius as the former single 8x8 streamer.
        // Total requested coverage remains proportional to logical volume.
        const float thickness = StreamerGeometry::getThicknessFromVolume(volumePercent / count);
        float histX[count][RNDR_Z];
        float histY[count][RNDR_Z];
        float currentX[count], currentY[count], targetX[count], targetY[count];
        const PaletteDescription firstColor = PaletteUtils::lastPaletteDescription();
        TexturePlan plans[count];
        float lowX[count], highX[count], lowY[count], highY[count];
        uint32_t redirectedMs[count];
        for (uint8_t i = 0; i < count; ++i) {
            plans[i] = PaletteUtils::getRandomMovingTexturePlan();
            // Preserve explicit single-color/shader overloads for the first
            // stream; the normal deck supplies its randomized plan here.
            if (i == 0) {
                plans[i].sMode = sMode;
                plans[i].palette = pal;
                plans[i].color = firstColor;
            }
            PaletteUtils::logTexture("Vert Streamer", plans[i], i + 1);
            // Restore the original full-footprint freedom for every head.
            lowX[i] = min(thickness, (RNDR_X - 1.0f) * 0.5f);
            highX[i] = RNDR_X - 1.0f - lowX[i];
            lowY[i] = min(thickness, (RNDR_Y - 1.0f) * 0.5f);
            highY[i] = RNDR_Y - 1.0f - lowY[i];
            currentX[i] = (i & 1) ? RNDR_X * 0.75f : RNDR_X * 0.25f;
            currentY[i] = (i & 2) ? RNDR_Y * 0.75f : RNDR_Y * 0.25f;
            currentX[i] = constrain(currentX[i], lowX[i], highX[i]);
            currentY[i] = constrain(currentY[i], lowY[i], highY[i]);
            redirectedMs[i] = startTime - 300;
            targetX[i] = currentX[i];
            targetY[i] = currentY[i];
            for (uint8_t z = 0; z < RNDR_Z; ++z) {
                histX[i][z] = currentX[i];
                histY[i][z] = currentY[i];
            }
        }
        uint32_t lastFrame = millis();
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            const uint32_t now = millis();
            const uint32_t deltaMs = uint32_t(now - lastFrame);
            lastFrame = now;
            masterPhase += 0.33f * deltaMs;
            clearAll();
            float nextX[count], nextY[count];
            bool touching[count] = {};
            const float contactDistance = thickness * 2.0f;
            // Evaluate all six pairs before moving any head. This only
            // changes destinations; there is no force to cancel attraction.
            for (uint8_t i = 0; i < count; ++i) {
                for (uint8_t j = i + 1; j < count; ++j) {
                    const float dx = currentX[i] - currentX[j];
                    const float dy = currentY[i] - currentY[j];
                    if (dx * dx + dy * dy < contactDistance * contactDistance) {
                        touching[i] = true;
                        touching[j] = true;
                    }
                }
            }
            for (uint8_t i = 0; i < count; ++i) {
                const bool redirect = touching[i] && uint32_t(now - redirectedMs[i]) >= 300;
                const bool arrived = fabsf(currentX[i] - targetX[i]) < 0.05f &&
                                     fabsf(currentY[i] - targetY[i]) < 0.05f;
                if (redirect || arrived) {
                    // On contact, score twelve random destinations by their
                    // clearance from other heads AND their destinations.
                    // Ordinary wandering keeps a single random destination.
                    // Cooldown gives a new direction time to separate heads;
                    // persistent contact can retry, without per-frame jitter.
                    float bestScore = -1.0f;
                    const uint8_t candidates = redirect ? 12 : 1;
                    for (uint8_t attempt = 0; attempt < candidates; ++attempt) {
                        const float x = lowX[i] + float(random(0, 1001)) / 1000.0f * (highX[i] - lowX[i]);
                        const float y = lowY[i] + float(random(0, 1001)) / 1000.0f * (highY[i] - lowY[i]);
                        float score = 1.0e9f;
                        for (uint8_t j = 0; j < count; ++j) {
                            if (i == j) {
                                continue;
                            }
                            const float dx = x - currentX[j];
                            const float dy = y - currentY[j];
                            const float tx = x - targetX[j];
                            const float ty = y - targetY[j];
                            score = min(score, min(dx * dx + dy * dy, tx * tx + ty * ty));
                        }
                        if (score > bestScore) {
                            bestScore = score;
                            targetX[i] = x;
                            targetY[i] = y;
                        }
                    }
                    if (redirect) {
                        redirectedMs[i] = now;
                    }
                }
                const float dx = targetX[i] - currentX[i];
                const float dy = targetY[i] - currentY[i];
                nextX[i] = currentX[i] + constrain(dx, -0.2f, 0.2f);
                nextY[i] = currentY[i] + constrain(dy, -0.2f, 0.2f);
            }
            for (uint8_t i = 0; i < count; ++i) {
                const float dx = nextX[i] - currentX[i];
                const float dy = nextY[i] - currentY[i];
                const float distance = sqrtf(dx * dx + dy * dy);
                // Preserve the established travel-speed bound.
                const float scale = distance > 0.28f ? 0.28f / distance : 1.0f;
                currentX[i] = constrain(currentX[i] + dx * scale,
                    lowX[i], highX[i]);
                currentY[i] = constrain(currentY[i] + dy * scale,
                    lowY[i], highY[i]);
                for (uint8_t z = 0; z + 1 < RNDR_Z; ++z) {
                    histX[i][z] = histX[i][z + 1];
                    histY[i][z] = histY[i][z + 1];
                }
                histX[i][RNDR_Z - 1] = currentX[i];
                histY[i][RNDR_Z - 1] = currentY[i];
                for (uint8_t z = 0; z < RNDR_Z; ++z) {
                    TextureState tex(plans[i].sMode, plans[i].palette, 1.0f);
                    tex.internalPhase = masterPhase + i * 64 + z * 10;
                    tex.phase = uint32_t(tex.internalPhase);
                    tex.initMath();
                    const int x0 = max(0, int(floorf(histX[i][z] - thickness - 1.0f)));
                    const int x1 = min(int(RNDR_X) - 1, int(ceilf(histX[i][z] + thickness + 1.0f)));
                    const int y0 = max(0, int(floorf(histY[i][z] - thickness - 1.0f)));
                    const int y1 = min(int(RNDR_Y) - 1, int(ceilf(histY[i][z] + thickness + 1.0f)));
                    for (int x = x0; x <= x1; ++x) {
                        for (int y = y0; y <= y1; ++y) {
                            const float dx = x - histX[i][z];
                            const float dy = y - histY[i][z];
                            const float intensity = constrain(thickness - sqrtf(dx * dx + dy * dy), 0.0f, 1.0f);
                            if (intensity > 0.0f) {
                                CRGB color = tex.getColor(x, y, z);
                                color.nscale8(uint8_t(intensity * 255.0f));
                                CRGB combined = getVoxel(x, y, z);
                                combined += color;
                                setVoxel(x, y, z, combined);
                            }
                        }
                    }
                }
            }
            showCube();
            if (speedMs > 0) {
                delay(speedMs);
            } else {
                yield();
            }
        }
    }

    static void animateVerticalStreamer(uint32_t durationMs, uint32_t speedMs, float volumePercent, CRGB::HTMLColorCode colorCode) { animateVerticalStreamer(durationMs, speedMs, volumePercent, CRGBPalette16(CRGB(colorCode)), MODE_SOLID); }
    static void animateVerticalStreamer(uint32_t durationMs, uint32_t speedMs, float volumePercent, ShaderMode sMode) { animateVerticalStreamer(durationMs, speedMs, volumePercent, RainbowColors_p, sMode); }

    static void animateMeteor(uint32_t durationMs, uint32_t stepMs = 20) {
        constexpr uint8_t spans[] = {RNDR_X - 1, RNDR_Y - 1, RNDR_Z - 1};
        constexpr uint16_t crossingMs = 1250;
        constexpr uint16_t launchIntervalMs = 1400;
        constexpr uint8_t maxSegments = 2 * CUBE_SIZE;
        const CRGBPalette16 palette = RainbowColors_p;

        struct Crossing {
            float entry[3];
            float delta[3];
            uint32_t launchedAt;
            uint16_t headExitMs;
            uint8_t segments;
            uint16_t wakeLifeMs[maxSegments + 1];
        };
        // The current crossing and the fading wake from the previous one.
        Crossing paths[2] = {};
        uint8_t current = 0;
        uint8_t faceAxis = random8(3);
        const auto launch = [&paths, &current, &faceAxis, &spans](uint32_t now) {
            Crossing &path = paths[current];
            path.launchedAt = now;
            // Let the head travel one voxel beyond the exit face so the
            // anti-aliasing dims it as it leaves.
            path.headExitMs = crossingMs + (crossingMs + spans[faceAxis] - 1) / spans[faceAxis];
            float lengthSquared = 0.0f;
            for (uint8_t axis = 0; axis < 3; ++axis) {
                // On both transverse axes, use opposite sides of the center.
                // Vary the endpoints within those regions for different angles.
                float entry = axis == faceAxis ? 0.0f : random8(8, 36) / 100.0f;
                float exit = axis == faceAxis ? 1.0f : random8(65, 93) / 100.0f;
                if (random8(2)) {
                    entry = 1.0f - entry;
                    exit = 1.0f - exit;
                }
                path.entry[axis] = entry * spans[axis];
                path.delta[axis] = (exit - entry) * spans[axis];
                lengthSquared += path.delta[axis] * path.delta[axis];
            }
            path.segments = min(int(maxSegments), int(ceilf(sqrtf(lengthSquared))));
            for (uint8_t i = 0; i <= path.segments; ++i) {
                path.wakeLifeMs[i] = random16(600, 1001);
            }
        };
        const auto drawPoint = [](const Crossing &path, float progress, CRGB color) {
            GeometryEngine::drawWuVoxel(
                lroundf((path.entry[0] + progress * path.delta[0]) * 256.0f),
                lroundf((path.entry[1] + progress * path.delta[1]) * 256.0f),
                lroundf((path.entry[2] + progress * path.delta[2]) * 256.0f), color);
        };

        const uint32_t started = millis();
        launch(started);
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            const uint32_t now = millis();
            if (now - paths[current].launchedAt >= launchIntervalMs) {
                current ^= 1;
                faceAxis = (faceAxis + random8(1, 3)) % 3;
                launch(now);
            }
            clearAll();
            for (const Crossing &path : paths) {
                if (path.segments == 0) continue;
                const uint32_t elapsed = now - path.launchedAt;
                for (uint8_t i = 0; i <= path.segments; ++i) {
                    const uint32_t arrivalMs = uint32_t(i) * crossingMs / path.segments;
                    if (elapsed < arrivalMs) break;
                    const uint32_t ageMs = elapsed - arrivalMs;
                    if (ageMs >= path.wakeLifeMs[i]) continue;
                    // A wake sample stays where it was lit and keeps its hue.
                    // Each sample dims at its own rate without frame-to-frame flicker.
                    const float remaining = 1.0f - float(ageMs) / path.wakeLifeMs[i];
                    const uint8_t brightness = uint8_t(155.0f * remaining * remaining);
                    const CRGB color = ColorFromPalette(palette, i * 240 / path.segments, brightness, LINEARBLEND);
                    drawPoint(path, float(i) / path.segments, color);
                }
            }
            const Crossing &path = paths[current];
            const uint32_t elapsed = now - path.launchedAt;
            if (elapsed < path.headExitMs) {
                const float progress = float(elapsed) / crossingMs;
                const uint8_t hue = min(elapsed, uint32_t(crossingMs)) * 240 / crossingMs;
                const CRGB color = ColorFromPalette(palette, hue, 255, LINEARBLEND);
                drawPoint(path, progress, blend(color, CRGB(CRGB::White), 75));
            }
            showCube();
            delay(stepMs);
        }
    }

    // Digital rain across the whole XY footprint. A column's dark interval
    // sets the density, and its trail length scales with the render height.
    // ==========================================
    // THE WILD MOUSE (Internal Engine)
    // ==========================================
    static void animateWildMouse(uint32_t durationMs, uint32_t speedMs) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        float masterPhase = 0.0f;

        // Preserve the original density and tail-length rules. The cube
        // dimensions are compile-time constants, so the arrays can reserve
        // exactly the slots this configuration uses, without heap allocation.
        // On 16x16x16 this is eight mice with 48 positions per tail, rather
        // than reserving sixteen mice with 64 positions each.
        constexpr int REQUESTED_MICE = (RNDR_X * RNDR_Y * RNDR_Z) / 512;
        constexpr int NUM_MICE = REQUESTED_MICE < 1 ? 1 :
                                 (REQUESTED_MICE > 16 ? 16 : REQUESTED_MICE);
        constexpr int MAX_TAIL = 64;
        constexpr int TAIL_LENGTH = RNDR_X * 3 < MAX_TAIL ? RNDR_X * 3 : MAX_TAIL;

        // All state remains local to this invocation on the animation task.
        // No static storage, shared-arena space, or receiver mutex is added.
        uint8_t hx[NUM_MICE][TAIL_LENGTH];
        uint8_t hy[NUM_MICE][TAIL_LENGTH];
        uint8_t hz[NUM_MICE][TAIL_LENGTH];
        int headPtr[NUM_MICE];
        int x[NUM_MICE];
        int y[NUM_MICE];
        int z[NUM_MICE];
        int dir[NUM_MICE];
        int axis[NUM_MICE];
        CRGBPalette16 mousePals[NUM_MICE];

        for (int m = 0; m < NUM_MICE; m++) {
            x[m] = random8(RNDR_X);
            y[m] = random8(RNDR_Y);
            z[m] = random8(RNDR_Z);
            dir[m] = (random8(2) == 0) ? 1 : -1;
            axis[m] = random8(3);
            headPtr[m] = 0;

            // Each mouse keeps its own solid color or flowing palette.
            if (random8(2) == 0) {
                mousePals[m] = CRGBPalette16(PaletteUtils::getRandomVibrantColor());
            } else {
                mousePals[m] = PaletteUtils::getRandomPalette();
            }

            PaletteUtils::logTexture("Wild Mouse", MODE_LINEAR_FLOW, PaletteUtils::lastPaletteDescription(), m + 1);

            for (int i = 0; i < TAIL_LENGTH; i++) {
                hx[m][i] = x[m];
                hy[m][i] = y[m];
                hz[m][i] = z[m];
            }
        }

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrame;
            lastFrame = now;
            masterPhase += 0.133f * deltaMs;

            for (int m = 0; m < NUM_MICE; m++) {
                // Move one voxel along the current axis. At a wall, turn
                // toward the cube interior. Occasional random turns retain
                // the original wandering paths and random-number call order.
                switch (axis[m]) {
                case 0:
                    if (dir[m] == 1) {
                        x[m]++;
                        if (x[m] >= RNDR_X - 1) {
                            x[m] = RNDR_X - 1;
                            axis[m] = 1;
                            dir[m] = (y[m] < RNDR_CY) ? 1 : -1;
                        }
                    } else {
                        x[m]--;
                        if (x[m] <= 0) {
                            x[m] = 0;
                            axis[m] = 2;
                            dir[m] = (z[m] < RNDR_CZ) ? 1 : -1;
                        }
                    }
                    if (random8(5) == 0) {
                        axis[m] = (random8(2) == 0) ? 1 : 2;
                    }
                    break;

                case 1:
                    if (dir[m] == 1) {
                        y[m]++;
                        if (y[m] >= RNDR_Y - 1) {
                            y[m] = RNDR_Y - 1;
                            axis[m] = 2;
                            dir[m] = (z[m] < RNDR_CZ) ? 1 : -1;
                        }
                    } else {
                        y[m]--;
                        if (y[m] <= 0) {
                            y[m] = 0;
                            axis[m] = 0;
                            dir[m] = (x[m] < RNDR_CX) ? 1 : -1;
                        }
                    }
                    if (random8(5) == 0) {
                        axis[m] = (random8(2) == 0) ? 2 : 0;
                    }
                    break;

                case 2:
                    if (dir[m] == 1) {
                        z[m]++;
                        if (z[m] >= RNDR_Z - 1) {
                            z[m] = RNDR_Z - 1;
                            axis[m] = 0;
                            dir[m] = (x[m] < RNDR_CX) ? 1 : -1;
                        }
                    } else {
                        z[m]--;
                        if (z[m] <= 0) {
                            z[m] = 0;
                            axis[m] = 1;
                            dir[m] = (y[m] < RNDR_CY) ? 1 : -1;
                        }
                    }
                    if (random8(5) == 0) {
                        axis[m] = (random8(2) == 0) ? 0 : 1;
                    }
                    break;
                }

                // Circular history: overwrite the oldest tail position with
                // this frame's new head position, without shifting the array.
                headPtr[m] = (headPtr[m] + 1) % TAIL_LENGTH;
                hx[m][headPtr[m]] = x[m];
                hy[m][headPtr[m]] = y[m];
                hz[m][headPtr[m]] = z[m];
            }

            clearAll();

            for (int m = 0; m < NUM_MICE; m++) {
                for (int i = 0; i < TAIL_LENGTH; i++) {
                    int age = (headPtr[m] - i + TAIL_LENGTH) % TAIL_LENGTH;
                    uint8_t px = hx[m][i];
                    uint8_t py = hy[m][i];
                    uint8_t pz = hz[m][i];
                    uint8_t colorIndex = (px * 16) + (py * 16) + (pz * 16) +
                                         static_cast<uint32_t>(masterPhase);
                    CRGB color = ColorFromPalette(mousePals[m], colorIndex, 255, LINEARBLEND);

                    // Retain the existing brightness falloff so reducing
                    // unused storage does not change the visible tail.
                    uint8_t brightness = 255 - (age * (255 / TAIL_LENGTH));
                    if (brightness > 10) {
                        color.nscale8(brightness);

                        // Additive blending mixes colors where trails cross.
                        setVoxel(px, py, pz, getVoxel(px, py, pz) + color);
                    }
                }
            }

            showCube();
            if (speedMs > 0) {
                delay(speedMs);
            } else {
                yield();
            }
        }
    }

    static void animateChunchunVolumetric(uint32_t durationMs, CRGBPalette16 pal = RainbowColors_p, uint8_t effectSpeed = 128, uint8_t gapSizeParam = 128, ChunchunMode mode = CHUNCHUN_SERPENTINE) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        
        const unsigned numBirds = (RNDR_X > 8) ? 30 : 12;
        const uint32_t totalVoxels = RNDR_X * RNDR_Y * RNDR_Z;

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis();
            if (now - lastFrame == 0) { yield(); continue; }
            lastFrame = now;

           // 1. WLED TRAIL (Fade to create comets)
            uint8_t fadeAmt = (mode == CHUNCHUN_SERPENTINE) ? 75 : 35;
            fadeAll(fadeAmt);

            // 2. THE WLED TIMELINE MATH
            uint32_t baseCounter = now * (6 + (effectSpeed >> 4));
            unsigned span = (gapSizeParam << 8) / numBirds;

            for (unsigned i = 0; i < numBirds; i++) {
                
                uint32_t birdCounter = baseCounter - (i * span);
                uint16_t megumin = sin16((uint16_t)birdCounter) + 0x8000;
                
                int px = 0, py = 0, pz = 0;

                // 3. THE 3D ARCHITECTURES
                if (mode == CHUNCHUN_SERPENTINE) {
                    uint32_t voxelIndex = (uint32_t(megumin) * totalVoxels) >> 16;
                    voxelIndex = constrain(voxelIndex, 0, totalVoxels - 1);

                    pz = voxelIndex / (RNDR_X * RNDR_Y);
                    int floorIndex = voxelIndex % (RNDR_X * RNDR_Y);
                    py = floorIndex / RNDR_X;
                    px = floorIndex % RNDR_X;

                    if (py % 2 != 0) px = (RNDR_X - 1) - px;
                    if (pz % 2 != 0) py = (RNDR_Y - 1) - py;

                } 
                else if (mode == CHUNCHUN_LISSAJOUS) {
                    uint16_t xPhase = (uint16_t)(birdCounter * 3);
                    uint16_t yPhase = (uint16_t)(birdCounter * 4);
                    uint16_t zPhase = (uint16_t)(birdCounter * 5);
                    
                    px = constrain((int)roundf(RNDR_CX + (sin16(xPhase) / 32768.0f) * RNDR_CX), 0, RNDR_X - 1);
                    py = constrain((int)roundf(RNDR_CY + (sin16(yPhase) / 32768.0f) * RNDR_CY), 0, RNDR_Y - 1);
                    pz = constrain((int)roundf(RNDR_CZ + (sin16(zPhase) / 32768.0f) * RNDR_CZ), 0, RNDR_Z - 1);
                }

                // 4. WLED COLOR MAPPING
                uint8_t colorIndex = (i * 255) / numBirds;
                CRGB birdColor = ColorFromPalette(pal, colorIndex, 255, LINEARBLEND);

                CRGB existing = getVoxel(px, py, pz);
                setVoxel(px, py, pz, existing + birdColor);
            }

            showCube();
            delay(15); 
        }
    }
    // ==========================================
    // VECTOR ARMS (Auto-Scaling 3D Rays)
    // ==========================================
  /*  static void animateVectorArms(uint32_t durationMs, uint8_t effectSpeed = 128) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        
        // 1. AUTOMATIC VOLUME SCALING
        // Square root curve: 4x the volume yields 2x the rays.
        uint32_t volume = RNDR_X * RNDR_Y * RNDR_Z;
        uint8_t numLines = max(1, (int)(sqrt(volume) / 8.0f));
        
        // Scale WLED speed to a usable FastLED BPM range
        uint8_t speed = (effectSpeed / 16); 
        uint8_t hueBase = 0;

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis();
            if (now - lastFrame == 0) { yield(); continue; }
            lastFrame = now;

            hueBase++; 

            // 2. WLED FADE (The smeared trails)
            fadeAll(55);

            // 3. ORIGIN CALCULATION
            // X and Y drift around the core; Z sweeps the height.
            uint8_t ox = beatsin8(1 + speed, RNDR_CX - 2, RNDR_CX + 2);
            uint8_t oy = beatsin8(2 + speed, RNDR_CY - 2, RNDR_CY + 2);
            uint8_t oz = beatsin8(2 + speed, 0, RNDR_Z - 1); 

            // 4. RAY TRACING LOOP
            for (size_t i = 0; i < numLines; i++) {
                
                uint8_t dx = beatsin8(5 + speed, 0, RNDR_X - 1, 0, i * 24);
                uint8_t dy = beatsin8(4 + speed, 0, RNDR_Y - 1, 0, i * 36);
                uint8_t dz = beatsin8(3 + speed, 0, RNDR_Z - 1, 0, (i * 48) + 64);

                uint8_t xsteps = abs8(ox - dx) + 1;
                uint8_t ysteps = abs8(oy - dy) + 1;
                uint8_t zsteps = abs8(oz - dz) + 1;
                uint8_t steps = max(xsteps, max(ysteps, zsteps));

                // 5. COLOR LOCK (Hardcoded Rainbow)
                uint8_t colorIndex = (i * 255 / numLines) + hueBase;
                CRGB rayColor = ColorFromPalette(RainbowColors_p, colorIndex, 255, LINEARBLEND);

                // 6. DRAW SOLID 3D LINE
                for (size_t j = 1; j <= steps; j++) {
                    uint8_t rate = j * 255 / steps;
                    
                    uint8_t px = lerp8by8(ox, dx, rate);
                    uint8_t py = lerp8by8(oy, dy, rate);
                    uint8_t pz = lerp8by8(oz, dz, rate);

                    setVoxel(px, py, pz, rayColor); 
                }
            }

            showCube();
            delay(15);
        }
    }*/

    // ==========================================
    // VECTOR ARMS (Auto-Scaling Anti-Aliased Rays)
    // ==========================================
      static void animateVectorArms(uint32_t durationMs, uint8_t effectSpeed = 128) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        
        uint32_t volume = RNDR_X * RNDR_Y * RNDR_Z;
        uint8_t numLines = max(1, (int)(sqrt(volume) / 8.0f));
        
        uint8_t speed = (effectSpeed / 16); 
        uint8_t hueBase = 0;

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis();
            if (now - lastFrame == 0) { yield(); continue; }
            lastFrame = now;

            hueBase++; 
            fadeAll(55);

            // 1. ORIGIN CALCULATION (Upgraded to 16-bit sub-voxel precision)
            int32_t ox_fp = beatsin16(1 + speed, (int32_t)((RNDR_CX - 2.0f) * 256.0f), (int32_t)((RNDR_CX + 2.0f) * 256.0f));
            int32_t oy_fp = beatsin16(2 + speed, (int32_t)((RNDR_CY - 2.0f) * 256.0f), (int32_t)((RNDR_CY + 2.0f) * 256.0f));
            int32_t oz_fp = beatsin16(2 + speed, 0, (RNDR_Z - 1) * 256); 

            // 2. RAY TRACING LOOP
            for (size_t i = 0; i < numLines; i++) {
                
                // Scale the old 8-bit phases directly into the new 16-bit range
                uint16_t phaseX = i * 6144;         // i * 24 * 256
                uint16_t phaseY = i * 9216;         // i * 36 * 256
                uint16_t phaseZ = (i * 12288) + 16384; // (i * 48 + 64) * 256

                int32_t dx_fp = beatsin16(5 + speed, 0, (RNDR_X - 1) * 256, 0, phaseX);
                int32_t dy_fp = beatsin16(4 + speed, 0, (RNDR_Y - 1) * 256, 0, phaseY);
                int32_t dz_fp = beatsin16(3 + speed, 0, (RNDR_Z - 1) * 256, 0, phaseZ);

                uint8_t colorIndex = (i * 255 / numLines) + hueBase;
                CRGB rayColor = ColorFromPalette(RainbowColors_p, colorIndex, 255, LINEARBLEND);

                // 3. DRAW ANTI-ALIASED 3D LINE
                GeometryEngine::drawWuLine3D(ox_fp, oy_fp, oz_fp, dx_fp, dy_fp, dz_fp, rayColor);
            }
            showCube();
            delay(15);
        }
    }
    // Based on WLED Frizzles by Stepko and Andrew Tuline.
    // Fill rate counts moving heads per voxel. Trails add light behind them.
    static void animateComets(uint32_t durationMs, uint32_t stepMs = 20,
                                float headFillRate = 1.0f / 512.0f) {
        constexpr uint32_t voxelCount = uint32_t(RNDR_X) * RNDR_Y * RNDR_Z;
        const float fill = headFillRate > 0.0f ? min(headFillRate, 1.0f) : 0.0f;
        const uint32_t headCount = max(1u, uint32_t(lroundf(voxelCount * fill)));
        constexpr uint16_t bpmUnit = 256; // FastLED beatsin88 uses fractional BPM.
        constexpr uint16_t baseBpm = 16 * bpmUnit;
        constexpr uint8_t trailFade = 16;
        const CRGBPalette16 palette = RainbowColors_p;
        const uint32_t started = millis();
        clearAll();
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            fadeAll(trailFade);
            const CRGB color = ColorFromPalette(palette, beatsin8(12, 0, 255, started));
            for (uint32_t head = 0; head < headCount; ++head) {
                // Keep the original speed range as density changes.
                const uint16_t rateOffset = headCount > 1
                    ? bpmUnit + (7u * bpmUnit * head) / (headCount - 1)
                    : 9 * bpmUnit / 2;
                const uint16_t depthPhase = (65536u * head) / headCount;
                const uint16_t x = beatsin88(baseBpm + rateOffset, 0, (RNDR_X - 1) * 256, started);
                const uint16_t y = beatsin88(baseBpm - rateOffset, 0, (RNDR_Y - 1) * 256, started);
                const uint16_t z = beatsin88(9 * bpmUnit + 2 * rateOffset, 0, (RNDR_Z - 1) * 256,
                                            started, depthPhase);
                GeometryEngine::drawWuVoxel(x, y, z, color);
            }
            showCube();
            delay(stepMs);
        }
    }

    // VOLUMETRIC DNA SPIRAL (SDF Anti-Aliased & Optimized)
    // ==========================================
    static void animateDNASpiral(uint32_t durationMs, CRGBPalette16 pal = RainbowColors_p, float speedMultiplier = 1.0f) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        
        float radius = (RNDR_X / 2.0f) - 0.5f; 
        if (radius < 1.0f) radius = 1.0f;
        
        float backboneThickness = (RNDR_X > 8) ? 1.5f : 0.9f; 
        float rungThickness     = (RNDR_X > 8) ? 1.0f : 0.6f;

        // Pre-calculate squared thresholds for the bounding boxes (Saves 12,000 sqrtf calls per frame)
        float aaPaddingSq = (backboneThickness + 1.2f) * (backboneThickness + 1.2f);
        float rungPaddingSq = (rungThickness + 1.2f) * (rungThickness + 1.2f);

        float zFreq = TWO_PI / (RNDR_Z * 0.8f);

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrame;
            if (deltaMs == 0) { yield(); continue; } 
            lastFrame = now;

            clearAll(); 

            float timePhase = (now * 0.002f * speedMultiplier);

            for (int z = 0; z < RNDR_Z; z++) {
                float phase = (z * zFreq) + timePhase;
                
                float bx1 = RNDR_CX + (sinf(phase) * radius);
                float by1 = RNDR_CY + (cosf(phase) * radius);
                float bx2 = RNDR_CX + (sinf(phase + PI) * radius);
                float by2 = RNDR_CY + (cosf(phase + PI) * radius);

                bool hasRung = (((z + (now / 200)) % 4) != 0);
                uint8_t hue = (z * 255 / RNDR_Z) + (now / 20);
                CRGB baseColor = ColorFromPalette(pal, hue, 255, LINEARBLEND);

                // Math optimization: Pre-calculate the segment length squared once per Z-layer
                float l2 = (bx2 - bx1)*(bx2 - bx1) + (by2 - by1)*(by2 - by1);

                for (int y = 0; y < RNDR_Y; y++) {
                    float fy = (float)y;
                    // Loop Hoisting: Calculate Y-distances outside the X-loop
                    float dy1 = fy - by1; float dy1Sq = dy1*dy1;
                    float dy2 = fy - by2; float dy2Sq = dy2*dy2;

                    for (int x = 0; x < RNDR_X; x++) {
                        float fx = (float)x;

                        float dx1 = fx - bx1; float dist1Sq = dx1*dx1 + dy1Sq;
                        float dx2 = fx - bx2; float dist2Sq = dx2*dx2 + dy2Sq;

                        float intensity = 0.0f;

                        // ONLY run sqrtf if the pixel is physically inside the strand's glow radius
                        if (dist1Sq < aaPaddingSq) {
                            intensity = max(intensity, 1.0f - (sqrtf(dist1Sq) / backboneThickness));
                        }
                        if (dist2Sq < aaPaddingSq) {
                            intensity = max(intensity, 1.0f - (sqrtf(dist2Sq) / backboneThickness));
                        }

                        // Rung bounding box & calculation
                        if (hasRung && intensity < 1.0f && l2 > 0.001f) {
                            float minX = min(bx1, bx2) - rungThickness - 1.2f;
                            float maxX = max(bx1, bx2) + rungThickness + 1.2f;
                            float minY = min(by1, by2) - rungThickness - 1.2f;
                            float maxY = max(by1, by2) + rungThickness + 1.2f;

                            // Bounding box check before doing heavy projection math
                            if (fx >= minX && fx <= maxX && fy >= minY && fy <= maxY) {
                                float t = max(0.0f, min(1.0f, ((fx - bx1)*(bx2 - bx1) + (fy - by1)*(by2 - by1)) / l2));
                                float projX = bx1 + t * (bx2 - bx1);
                                float projY = by1 + t * (by2 - by1);
                                float distRungSq = (fx - projX)*(fx - projX) + (fy - projY)*(fy - projY);

                                if (distRungSq < rungPaddingSq) {
                                    float rungIntensity = 1.0f - (sqrtf(distRungSq) / rungThickness);
                                    if (rungIntensity > intensity) intensity = rungIntensity;
                                }
                            }
                        }

                        if (intensity > 0.01f) {
                            uint8_t brightness = (uint8_t)(intensity * 255.0f);
                            CRGB color = baseColor;
                            color.nscale8(brightness); 
                            CRGB existing = getVoxel(x, y, z);
                            setVoxel(x, y, z, existing + color);
                        }
                    }
                }
            }
            showCube();
        }
    }
    
    
    // ==========================================
    // 3D DRIFT ROSE (Harmonic Fibonacci Mandala)
    // ==========================================
    static void animateDriftRose3D(uint32_t durationMs, CRGBPalette16 pal = RainbowColors_p,
                                   uint8_t effectSpeed = 128,
                                   float (*readReach)() = nullptr,
                                   bool (*keepRunning)() = nullptr,
                                   uint8_t requestedSpokes = 0) {
        uint32_t startTime = millis();
        uint32_t lastFrame = startTime;

        constexpr int fullSpokeCount = RNDR_X > 8 ? 80 : 36;
        const int spokeCount = requestedSpokes ? min(int(requestedSpokes), fullSpokeCount) : fullSpokeCount;

        // Rebuild the evenly spaced directions for the chosen spoke count.
        static_assert(3 * fullSpokeCount * sizeof(int32_t) <= SHARED_APP_MEMORY_BYTES,
                      "Drift Rose spokes must fit the shared animation arena");
        auto *spokeX_fp = reinterpret_cast<int32_t *>(SharedAppMemory);
        auto *spokeY_fp = spokeX_fp + spokeCount;
        auto *spokeZ_fp = spokeY_fp + spokeCount;
        
        float goldenRatio = (1.0f + sqrtf(5.0f)) / 2.0f;
        float angleInc = TWO_PI * goldenRatio;
        
        float maxR_XY = min(RNDR_X, RNDR_Y) / 2.0f;
        float maxR_Z  = RNDR_Z / 2.0f; // ALWAYS STRETCHED TO FULL VERTICAL VOLUME
        
        if (maxR_XY < 1.0f) maxR_XY = 1.0f;
        if (maxR_Z < 1.0f) maxR_Z = 1.0f;

        for (int i = 0; i < spokeCount; i++) {
            float t = float(i) / spokeCount;
            float phi = acosf(1.0f - 2.0f * t);
            float theta = angleInc * i;
            
            // Bake the radius and the 256x sub-pixel multiplier directly into AppMemory
            spokeX_fp[i] = (int32_t)(sinf(phi) * cosf(theta) * maxR_XY * 256.0f);
            spokeY_fp[i] = (int32_t)(sinf(phi) * sinf(theta) * maxR_XY * 256.0f);
            spokeZ_fp[i] = (int32_t)(cosf(phi) * maxR_Z * 256.0f);
        }

        uint8_t fadeAmt = 32 + (effectSpeed >> 3);
        
        // Pre-compute center positions in fixed-point
        int32_t cx_fp = (int32_t)(RNDR_CX * 256.0f);
        int32_t cy_fp = (int32_t)(RNDR_CY * 256.0f);
        int32_t cz_fp = (int32_t)(RNDR_CZ * 256.0f);

        // 2. THE RENDER ENGINE
        while (keepRunning ? keepRunning() :
               TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis();
            const uint32_t frameMs = now - lastFrame;
            if (frameMs < 15) { yield(); continue; }
            lastFrame = now;

            // FastLED beat16 uses 280/256 phase units per millisecond at
            // one cycle per minute. Every spoke samples the same clock.
            const uint32_t scaledClock = now * 280u;
            const int32_t reach = readReach ? lroundf(readReach() * 32768.0f) : 32768;

            // Lightning-fast hardware fade completely replaces the 4096-iteration x/y/z loop
            fadeAll(fadeAmt);

            for (int i = 0; i < spokeCount; i++) {
                // Sample the original range of rates even with fewer spokes.
                const int spokeRate = 1 + i * (fullSpokeCount - 1) / max(1, spokeCount - 1);
                const uint16_t phase = uint16_t((scaledClock * spokeRate) >> 8);
                const int32_t oscillation = int32_t(scale16(uint16_t(sin16(phase) + 32768), 65535)) - 32768;
                const int32_t r_scalar = (oscillation * reach) >> 15;

                int32_t px_fp = cx_fp + ((spokeX_fp[i] * r_scalar) >> 15);
                int32_t py_fp = cy_fp + ((spokeY_fp[i] * r_scalar) >> 15);
                int32_t pz_fp = cz_fp + ((spokeZ_fp[i] * r_scalar) >> 15);

                uint8_t colorIndex = i * (256 / spokeCount);
                CRGB col = ColorFromPalette(pal, colorIndex, 255, LINEARBLEND);
                
                GeometryEngine::drawWuVoxel(px_fp, py_fp, pz_fp, col);
            }
            showCube();
            delay(15); // Explicit DMA frame limiter
        }
    }

    // A drifting 3D Perlin field. Low samples leave holes, and the visible
    // range drives both brightness and color within one fixed rainbow.
    static void animateNoiseClouds(uint32_t durationMs, uint32_t stepMs = 24) {
        constexpr uint16_t noiseSpan = 640; // 2.5 noise cells per render axis.
        constexpr int darkThreshold = 132;
        constexpr int fullBrightness = 192;
        constexpr int visibleRange = fullBrightness - darkThreshold;
        const CRGBPalette16 palette = RainbowColors_p;
        CRGB colorByNoise[256];
        for (int sample = 0; sample < 256; ++sample) {
            const int level = constrain(sample - darkThreshold, 0, visibleRange);
            colorByNoise[sample] = ColorFromPalette(palette, level * 240 / visibleRange,
                                                    level * 255 / visibleRange, LINEARBLEND);
        }

        const uint32_t started = millis();
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            const uint32_t elapsed = millis() - started;
            const uint16_t offsetX = 12345 + elapsed / 26;
            const uint16_t offsetY = 23456 + elapsed / 41;
            const uint16_t offsetZ = 34567 + elapsed / 17;
            for (int z = 0; z < RNDR_Z; ++z) {
                const uint16_t nz = offsetZ + z * noiseSpan / RNDR_Z;
                for (int y = 0; y < RNDR_Y; ++y) {
                    const uint16_t ny = offsetY + y * noiseSpan / RNDR_Y;
                    for (int x = 0; x < RNDR_X; ++x) {
                        const uint16_t nx = offsetX + x * noiseSpan / RNDR_X;
                        setVoxel(x, y, z, colorByNoise[inoise8(nx, ny, nz)]);
                    }
                }
            }
            showCube();
            delay(stepMs);
        }
    }

    // Inspired by WLED 16.0.1's 2D Plasma Ball (Stepko, modified by Andrew
    // Tuline). Warp filaments through the volume and cycle one shared hue.
    static void animatePlasmaArms(uint32_t durationMs, uint8_t effectSpeed = 128) {
        constexpr float referenceHalfSpan = 7.5f;
        constexpr uint16_t noiseSpan = 480;
        const CRGBPalette16 palette = RainbowColors_p;
        uint8_t warpX[RNDR_Z][RNDR_Y];
        uint8_t warpY[RNDR_Z][RNDR_X];
        uint8_t warpZ[RNDR_Y][RNDR_X];
        const uint32_t started = millis();
        clearAll();
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            const uint32_t elapsed = millis() - started;
            const uint16_t time = uint16_t(elapsed * (32u + effectSpeed) / 512u);
            const uint8_t hue = beat8(5);
            fadeAll(48);

            // Each coordinate bends with the other two and with time.
            // Cache the noise samples shared by voxels along each axis.
            for (int z = 0; z < RNDR_Z; ++z) {
                for (int y = 0; y < RNDR_Y; ++y) {
                    warpX[z][y] = inoise8(y * noiseSpan / RNDR_Y, z * noiseSpan / RNDR_Z, time);
                }
                for (int x = 0; x < RNDR_X; ++x) {
                    warpY[z][x] = inoise8(z * noiseSpan / RNDR_Z, x * noiseSpan / RNDR_X, time + 1709);
                }
            }
            for (int y = 0; y < RNDR_Y; ++y) {
                for (int x = 0; x < RNDR_X; ++x) {
                    warpZ[y][x] = inoise8(x * noiseSpan / RNDR_X, y * noiseSpan / RNDR_Y, time + 3907);
                }
            }

            for (int z = 0; z < RNDR_Z; ++z) {
                const float zBase = (z - RNDR_CZ) * (referenceHalfSpan / RNDR_CZ);
                for (int y = 0; y < RNDR_Y; ++y) {
                    const float yBase = (y - RNDR_CY) * (referenceHalfSpan / RNDR_CY);
                    const float xOffset = (int(warpX[z][y]) - 128) * 0.05f;
                    for (int x = 0; x < RNDR_X; ++x) {
                        const float xBase = (x - RNDR_CX) * (referenceHalfSpan / RNDR_CX);
                        const float ax = fabsf(xBase + xOffset);
                        const float ay = fabsf(yBase + (int(warpY[z][x]) - 128) * 0.05f);
                        const float az = fabsf(zBase + (int(warpZ[y][x]) - 128) * 0.05f);

                        // Equal coordinate magnitudes trace the body diagonals.
                        // Two near-zero coordinates trace the principal axes.
                        const float low = min(ax, min(ay, az));
                        const float high = max(ax, max(ay, az));
                        const float diagonal = high - low;
                        const float axial = max(min(ax, ay), min(max(ax, ay), az));
                        const float coverage = min(1.0f, 1.45f - min(diagonal, axial));
                        if (coverage <= 0.0f) continue;

                        const uint8_t brightness = uint8_t(coverage * warpX[z][y]);
                        const CRGB light = ColorFromPalette(palette, hue, brightness, LINEARBLEND);
                        setVoxel(x, y, z, getVoxel(x, y, z) + light);
                    }
                }
            }
            showCube();
            delay(16);
        }
    }

    /*
 // The literal ratios from the left axis of your chart
    enum LissajousRatio {
        RATIO_1_1, RATIO_2_1, RATIO_3_1, RATIO_3_2, 
        RATIO_4_3, RATIO_5_3, RATIO_5_4
    };

    // Passes strictly the Ratio and K, draws a solid line, extrudes natively on Z, 4-way Radial Symmetry
    static void animateChartExtruded(uint32_t durationMs, LissajousRatio ratio, uint8_t k, CRGBPalette16 pal = RainbowColors_p) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();

        // 1. Look up the exact frequencies from the requested chart ratio
        float nx = 1.0f, ny = 1.0f;
        switch(ratio) {
            case RATIO_1_1: nx = 1.0f; ny = 1.0f; break;
            case RATIO_2_1: nx = 2.0f; ny = 1.0f; break;
            case RATIO_3_1: nx = 3.0f; ny = 1.0f; break;
            case RATIO_3_2: nx = 3.0f; ny = 2.0f; break;
            case RATIO_4_3: nx = 4.0f; ny = 3.0f; break;
            case RATIO_5_3: nx = 5.0f; ny = 3.0f; break;
            case RATIO_5_4: nx = 5.0f; ny = 4.0f; break;
        }

        // 2. Convert 'k' (0-7) directly to a phase shift in radians
        float phaseX = k * (PI / 4.0f);

        // 3. Chunchun's Z Extrusion
        float nz = 1.0f; 

        // Center points and stretching radii
        float cx = (RNDR_X - 1) / 2.0f;
        float cy = (RNDR_Y - 1) / 2.0f;
        float cz = (RNDR_Z - 1) / 2.0f;

        // 800 points guarantees a solid, unbroken trace even on 4 strands
        const int numPoints = 800; 

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis();
            if (now - lastFrame < 15) { yield(); continue; } // Cap at ~60fps
            lastFrame = now;

            clearAll();
            
            // Shift this over time to make the texture "flow" along the solid wires
            uint8_t colorShift = (now / 15) & 0xFF; 

            for (int i = 0; i < numPoints; i++) {
                // Map 'i' to a full 0 to 2*PI mathematical circle
                float t = (i / (float)numPoints) * TWO_PI;

                // Calculate the base string ONCE (saves massive CPU)
                float bx = sinf(nx * t + phaseX);
                float by = sinf(ny * t);
                float bz = sinf(nz * t);

                // Color mapping
                uint8_t knotPosition = (i * 255) / numPoints;
                CRGB newColor = ColorFromPalette(pal, knotPosition + colorShift, 255, LINEARBLEND);

                // 4-WAY RADIAL SYMMETRY MULTIPLEXER
                for (int rot = 0; rot < 4; rot++) {
                    float rotX, rotY;
                    
                    // Instant 90-degree rotations using coordinate swapping
                    if (rot == 0)      { rotX = bx;   rotY = by; }   // 0 degrees
                    else if (rot == 1) { rotX = -by;  rotY = bx; }   // 90 degrees
                    else if (rot == 2) { rotX = -bx;  rotY = -by; }  // 180 degrees
                    else               { rotX = by;   rotY = -bx; }  // 270 degrees

                    // STRETCH TO HARDWARE BOUNDS
                    int px = constrain((int)roundf(cx + rotX * cx), 0, RNDR_X - 1);
                    int py = constrain((int)roundf(cy + rotY * cy), 0, RNDR_Y - 1);
                    int pz = constrain((int)roundf(cz + bz * cz), 0, RNDR_Z - 1);
                    
                    // Additive blending: The core and the face intersections will glow brightly!
                    CRGB existing = getVoxel(px, py, pz);
                    setVoxel(px, py, pz, existing | newColor);
                }
            }
            showCube();
        }
    }*/
    /*
    static void animateTartan3D(uint32_t durationMs, CRGBPalette16 pal = PartyColors_p, uint8_t effectSpeed = 128) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        
        // Translate WLED speed into a discrete stepping interval
        // A speed of 128 gives ~330ms per step, creating a deliberate, gentle geometric shift
        uint32_t stepInterval = map(effectSpeed, 0, 255, 600, 60); 

        // Pre-allocate our Boolean gap arrays to keep the inner loop lightning fast
        bool gapX[RNDR_X], gapY[RNDR_Y], gapZ[RNDR_Z];

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis(); 
            if (now - lastFrame < 15) { yield(); continue; } 
            lastFrame = now;

            // DISCRETE STEPPING: The lattice sits perfectly still until this ticks over
            uint32_t ticks = now / stepInterval;
            
            // Offset the gaps to make the empty tunnels "step" through the room.
            // Dividing ticks makes the axes weave at different speeds.
            int offsetX = ticks % 4;
            int offsetY = (ticks / 2) % 4;
            int offsetZ = (ticks / 3) % 4; 

            // HOISTED: A voxel is in a "gap band" if it falls in the second half of a 4-voxel cycle (2 ON, 2 OFF)
            for (int x = 0; x < RNDR_X; x++) gapX[x] = ((x + offsetX) % 4) >= 2;
            for (int y = 0; y < RNDR_Y; y++) gapY[y] = ((y + offsetY) % 4) >= 2;
            for (int z = 0; z < RNDR_Z; z++) gapZ[z] = ((z + offsetZ) % 4) >= 2;

            uint8_t colorShift = (ticks * 4) & 0xFF;

            // INNER LOOP: Boolean scaffolding logic
            for (int z = 0; z < RNDR_Z; z++) {
                for (int y = 0; y < RNDR_Y; y++) {
                    for (int x = 0; x < RNDR_X; x++) {
                        
                        // Count how many "gap bands" intersect at this exact voxel
                        uint8_t numGaps = gapX[x] + gapY[y] + gapZ[z];
                        
                        // IF 2 OR 3 GAPS INTERSECT: It creates a hollow 2x2 tunnel passing entirely through the structure
                        if (numGaps >= 2) {
                            setVoxel(x, y, z, CRGB::Black); 
                        } 
                        // IF 0 OR 1 GAPS INTERSECT: It forms the solid physical lattice (beams and pillars)
                        else {
                            uint8_t hue;
                            // Color the lattice based on its structural role to mimic woven threads
                            if (numGaps == 0) hue = (x * 8 + y * 8 + z * 8); // Corner Pillar (Intersection of walls)
                            else if (gapX[x]) hue = (y * 16 + z * 16) + 40;  // X-Beam (Running through X-gap)
                            else if (gapY[y]) hue = (x * 16 + z * 16) + 80;  // Y-Beam (Running through Y-gap)
                            else              hue = (x * 16 + y * 16) + 120; // Z-Beam (Running through Z-gap)

                            // NOBLEND forces razor-sharp edges with no fuzzy math
                            setVoxel(x, y, z, ColorFromPalette(pal, hue + colorShift, 255, NOBLEND));
                        }
                    }
                }
            }
            showCube();
        }
    }
static void animateSoapFilm3D(uint32_t durationMs, CRGBPalette16 pal = RainbowColors_p, uint8_t effectSpeed = 128) {
        uint32_t startTime = millis(), lastFrame = millis(), noiseTime = 0;
        uint16_t scale = 1200; 

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame;
            if (deltaMs == 0) { yield(); continue; } lastFrame = now;

            noiseTime += (deltaMs * effectSpeed) / 4;

            for (int z = 0; z < RNDR_Z; z++) {
                uint32_t zScale = z * scale;
                for (int y = 0; y < RNDR_Y; y++) {
                    uint32_t yScale = y * scale;
                    for (int x = 0; x < RNDR_X; x++) {
                        uint32_t xScale = x * scale;
                        
                        // Domain warping
                        uint16_t warpX = inoise16(xScale, yScale, zScale + noiseTime);
                        uint16_t warpY = inoise16(xScale + 4000, yScale + 4000, zScale + noiseTime);
                        uint8_t data = inoise8(xScale + (warpX >> 4), yScale + (warpY >> 4), zScale + (noiseTime >> 2));
                        
                        setVoxel(x, y, z, ColorFromPalette(pal, data * 3, 255, LINEARBLEND));
                    }
                }
            }
            showCube();
        }
    }

   

    static void animatePlasmaBall3D(uint32_t durationMs, CRGBPalette16 pal = RainbowColors_p, uint8_t effectSpeed = 128) {
        uint32_t startTime = millis(), lastFrame = millis(), noiseTime = 0, colorTime = 0;
        
        float baseRadius = (RNDR_X / 2.0f) - 1.5f;
        float maxDisplacement = 1.5f;
        float maxAllowedRadSq = (baseRadius + maxDisplacement) * (baseRadius + maxDisplacement);
        uint16_t scale = 1500; 

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame;
            if (deltaMs == 0) { yield(); continue; } lastFrame = now;

            noiseTime += (deltaMs * effectSpeed) / 4;
            colorTime += deltaMs / 4; 
            clearAll();

            for (int z = 0; z < RNDR_Z; z++) {
                float dz = (float)z - RNDR_CZ; float dzSq = dz * dz;
                for (int y = 0; y < RNDR_Y; y++) {
                    float dy = (float)y - RNDR_CY; float dySq = dy * dy;
                    for (int x = 0; x < RNDR_X; x++) {
                        float dx = (float)x - RNDR_CX; 
                        float distSq = dx * dx + dySq + dzSq;

                        // SDF CULLING: Skip the expensive math if we are outside the bounds of the bubble!
                        if (distSq > maxAllowedRadSq) continue; 

                        // ONLY run the expensive sqrt and noise if the voxel survived the cull
                        float dist = sqrtf(distSq);
                        uint8_t noiseVal = inoise8(x * scale, y * scale, z * scale + noiseTime);
                        float displacement = ((noiseVal / 255.0f) * (maxDisplacement * 2.0f)) - maxDisplacement;
                        float localRadius = baseRadius + displacement;

                        if (dist < localRadius) {
                            float depth = dist / localRadius;
                            uint8_t hue = (colorTime & 0xFF) + (uint8_t)(depth * 64.0f);
                            CRGB finalColor = ColorFromPalette(pal, hue, 255, LINEARBLEND);
                            if (depth < 0.4f) finalColor |= CRGB(150, 150, 150); // White hot core
                            setVoxel(x, y, z, finalColor);
                        }
                    }
                }
            }
            showCube();
        }
    }
*/
};
