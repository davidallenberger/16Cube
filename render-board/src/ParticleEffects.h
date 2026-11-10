#pragma once

#include "ParticleEngine.h"
#include "TestDemo.h"
#include "UARTReceiver.h"

#include <algorithm>

namespace ParticleEffects {
    struct VolcanoSettings {
        uint8_t speed = 100;
        uint8_t intensity = 190;
        uint8_t move = 0;
        uint8_t bounce = 160;
        uint8_t spread = 6; // 0–31.
        float particleRadius = 0.75f; // Radius on the 16³ cube.
        float radiusWobble = 0.04f;
        float emitterWobble = 0.325f; // Nozzle travel around its resting position.
        bool ageColor = true;
        bool walls = false;
        bool collide = false;
    };

    struct BoxSettings {
        uint8_t speed = 128;
        uint8_t particles = 200; // Population on the 16³ reference cube.
        uint8_t tilt = 128;
        uint8_t hardness = 128;
        uint8_t size = 1; // 1 is one voxel. 31 chooses a size per ball.
        bool random = true;
        bool washingMachine = false;
        bool sloshing = false;
    };

    struct WaterfallSettings {
        uint8_t speed = 15;
        uint8_t intensity = 200;
        uint8_t variation = 40;
        uint8_t collide = 160; // 0 disables collisions. Higher values harden contacts.
        uint8_t position = 16; // Left to right, 0–31.
        float spanFraction = 1.0f / 3.0f; // Fraction of the ceiling diagonal.
        float bulgeVoxels = 1.5f; // Peak outward offset per row on the 16³ cube.
        bool cylinder = false;
        bool walls = false;
        bool ground = true;
    };

    struct GhostRiderSettings {
        uint8_t speed = 70;
        uint8_t spiral = 130;
        uint8_t blur = 220;
        uint8_t colorCycle = 30;
        uint8_t spread = 35; // The cube can exceed WLED's 0–31 range.
        bool ageColor = true;
        bool walls = false;
    };

    struct AttractorSettings {
        uint8_t mass = 100, particles = 82, size = 2, collide = 0, friction = 16;
        bool ageColor = false, move = false, swallow = false;
        float orbitRadius = 0.94f; // Fraction of the shortest distance from center to a face.
    };

    struct SpraySettings {
        uint8_t speed = 150, intensity = 150, leftRight = 220, upDown = 30, angle = 21;
        bool gravity = false, cylinder = false, collide = false;
    };

    struct GeqSettings {
        uint8_t speed = 155, intensity = 200, diverge = 0, bounce = 128, gravity = 16;
        bool cylinder = false, walls = false, floor = false;
    };

    struct GeqNovaSettings {
        uint8_t speed = 128, intensity = 180, rotationSpeed = 0, colorChange = 0, nozzle = 8;
        bool direction = false; // Reverse rotation.
    };

    struct GalaxySettings {
        uint8_t speed = 80, intensity = 128, size = 1, color = 4;
        bool starfield = false, trace = false;
    };

    namespace Detail {
        using namespace ParticleEngine;

        constexpr float verticalScale = RNDR_Z / 16.0f;
        constexpr float volumeScale = RNDR_X * RNDR_Y * RNDR_Z / 4096.0f;
        constexpr float dt = 0.016667f;
        constexpr float wledVelocity = 42.0f / 64.0f;
        constexpr float wledGravity = wledVelocity * 42.0f / 16.0f;

        // Box balls have no lifetime or per-particle drag, so 200 fit in the shared arena.
        struct BoxBall { bool active; uint8_t type; Motion motion; CRGB color; };
        using BoxPool = ParticlePool<BoxBall>;
        static_assert(BoxPool::capacity >= 200, "Box must fit 200 balls in the shared arena");

        inline float sample(float low, float high) {
            return low + (high - low) * (random16() / 65536.0f);
        }

        // Scale emit rates and populations from the 16³ reference cube.
        inline int population(int reference, int capacity = maxParticles) {
            return constrain(int(lroundf(reference * volumeScale)), 1, capacity);
        }

        template <typename Step, typename Draw>
        inline void run(uint32_t durationMs, uint8_t blur, Step step, Draw draw,
                        bool (*continueDrawing)() = nullptr) {
            constexpr uint32_t stepUs = 16667;
            const uint32_t start = millis();
            uint32_t lastFrame = micros();
            uint32_t accrued = stepUs;
            uint32_t tick = 0;
            clearAll();
            while (continueDrawing ? continueDrawing() :
                   TestDemo::routineTimeRemaining(millis() - start, durationMs)) {
                const uint32_t now = micros();
                accrued += min(uint32_t(now - lastFrame), 3 * stepUs);
                lastFrame = now;
                while (accrued >= stepUs) {
                    accrued -= stepUs;
                    step(tick++);
                }
                if (blur) fadeAll(255 - blur);
                else clearAll();
                draw();
                showCube();
            }
        }

        // Seven evenly spaced WLED emission opportunities per ten 60 Hz steps.
        inline bool wledFrameDue(uint32_t tick) { return ((tick % 10) * 7) % 10 < 7; }

        inline bool emitBand(uint8_t level, int threshold) {
            return level > threshold || (level > 0 && random16(0, ((threshold - level) >> 2) + 2) == 0);
        }

        inline bool liveAudioSource() {
            const auto control = RemoteInputs::control();
            return control.linked && control.ready && control.mode >= CubinaWire::Mode::AudioMic &&
                   control.mode <= CubinaWire::Mode::AudioDJ;
        }

        inline void drawSpray(Pool& pool, const CRGBPalette16& palette, bool wrapX) {
            for (int i = 0; i < pool.size(); ++i) {
                const auto& p = pool[i];
                if (!p.active) continue;
                const uint8_t brightness = uint8_t(255.0f * min(1.0f, 2.0f * p.life));
                drawSmooth(p.motion.position, ColorFromPalette(palette, p.type, brightness), wrapX);
            }
        }

        inline void drawParticlePoint(Point3 p, CRGB color, bool wrapX = false) {
            int x = lroundf(p.x);
            if (wrapX && x == RNDR_X) x = 0;
            const int y = lroundf(p.y), z = lroundf(p.z);
            if (x >= 0 && x < RNDR_X && y >= 0 && y < RNDR_Y && z >= 0 && z < RNDR_Z) {
                setVoxel(x, y, z, getVoxel(x, y, z) + color);
            }
        }

        inline void resolveSprayTop(Pool& pool, Side side, float bounce) {
            for (int i = 0; i < pool.size(); ++i) {
                auto& p = pool[i];
                if (!p.active || p.motion.position.z <= RNDR_Z - 1.0f) continue;
                p.active = resolveSide(p.motion.position.z, p.motion.velocity.z, RNDR_Z, side, bounce);
            }
        }

        inline void bounceEmitter(Motion& emitter, float step, float speed) {
            advance(emitter, step, {}, 1.0f);
            auto& p = emitter.position;
            auto& v = emitter.velocity;
            if (p.x >= 0 && p.x <= RNDR_X - 1 && p.y >= 0 && p.y <= RNDR_Y - 1 &&
                p.z >= 0 && p.z <= RNDR_Z - 1) return;
            // Redistribute the nozzle's heading at a wall, reflecting each outward component.
            const float elevation = sample(-1.0f, 1.0f);
            v = GeometryEngine::helixPoint({}, speed * sqrtf(1.0f - elevation * elevation),
                                           sample(0, TWO_PI), speed * elevation);
            if (p.x < 0 || p.x > RNDR_X - 1) v.x = p.x < 0 ? fabsf(v.x) : -fabsf(v.x);
            if (p.y < 0 || p.y > RNDR_Y - 1) v.y = p.y < 0 ? fabsf(v.y) : -fabsf(v.y);
            if (p.z < 0 || p.z > RNDR_Z - 1) v.z = p.z < 0 ? fabsf(v.z) : -fabsf(v.z);
            p = {constrain(p.x, 0.0f, RNDR_X - 1.0f), constrain(p.y, 0.0f, RNDR_Y - 1.0f),
                 constrain(p.z, 0.0f, RNDR_Z - 1.0f)};
        }

        inline void stepSpray(Pool& pool, Side xSide, Side ySide, bool ground,
                              float bounce, float roughness, bool collisions, float step = dt) {
            for (int i = 0; i < pool.size(); ++i) {
                auto& p = pool[i];
                if (p.active) advance(p, step);
            }
            if (collisions) collide(pool, bounce, xSide == Side::Wrap,
                                   [](const Particle&) { return 0.40f; });
            for (int i = 0; i < pool.size(); ++i) {
                auto& particle = pool[i];
                if (!particle.active) continue;
                auto& motion = particle.motion;
                auto& p = motion.position;
                auto& v = motion.velocity;
                if (!resolveSide(p.x, v.x, RNDR_X, xSide, bounce) ||
                    !resolveSide(p.y, v.y, RNDR_Y, ySide, bounce) ||
                    (!ground && p.z <= -1.0f)) {
                    particle.active = false;
                } else if (ground) {
                    if (p.z <= 0.0f) particle.flags |= HitFloor;
                    bounceFloor(motion, bounce, roughness);
                }
            }
        }

        inline void drawFallingDroplet(Point3 p, CRGB color, bool wrapX) {
            int x = lroundf(p.x);
            if (wrapX && x == RNDR_X) x = 0;
            const int y = lroundf(p.y);
            if (x < 0 || x >= RNDR_X || y < 0 || y >= RNDR_Y ||
                p.z <= -1.0f || p.z >= RNDR_Z) return;

            // Keep the light in one column. Share it vertically for smooth
            // descent, then use full 3D blending once the droplet hits.
            const int z = int(floorf(p.z));
            const uint8_t upper = uint8_t((p.z - z) * 255.0f);
            if (z >= 0) {
                CRGB lowerColor = color;
                lowerColor.nscale8(255 - upper);
                setVoxel(x, y, z, getVoxel(x, y, z) + lowerColor);
            }
            if (z + 1 < RNDR_Z) {
                color.nscale8(upper);
                setVoxel(x, y, z + 1, getVoxel(x, y, z + 1) + color);
            }
        }
    }

    inline void animateVolcano(uint32_t durationMs, const VolcanoSettings& settings = {}) {
        using namespace Detail;
        // WLED uses its Fire palette. Keep the upper end yellow so the
        // fountain visibly cools through orange and red as the sparks age.
        const CRGBPalette16 palette(CRGB::Red, CRGB::OrangeRed, CRGB::Orange, CRGB::Yellow);
        const Side side = settings.walls ? Side::Bounce : Side::Open;
        const float bounce = settings.bounce / 255.0f;
        const float spread = (0.7f + settings.spread * 0.48f) * min(RNDR_X, RNDR_Y) / 16.0f;
        const float lift = settings.speed * 0.29f * verticalScale;
        const float gravity = 29.0f * verticalScale;
        const float emissionRate = 150.0f * volumeScale / (11 - settings.intensity / 25);
        const float lengthScale = cbrtf(volumeScale);
        const float emitterWobble = settings.emitterWobble * lengthScale;
        Pool pool(population(138));
        Motion nozzle{{RNDR_CX, RNDR_CY, 0.65f * verticalScale},
                      {settings.move * RNDR_X / 640.0f, settings.move * RNDR_Y / 960.0f, 0.0f}};
        float emission = 1.0f;
        uint8_t hue = random8();
        run(durationMs, 175, [&](uint32_t tick) {
            advance(nozzle, dt, {0, 0, 0}, 1.0f);
            resolveSide(nozzle.position.x, nozzle.velocity.x, RNDR_X, Side::Bounce, 1.0f);
            resolveSide(nozzle.position.y, nozzle.velocity.y, RNDR_Y, Side::Bounce, 1.0f);
            emission += emissionRate * dt;
            while (emission >= 1.0f) {
                emission -= 1.0f;
                // Wobble the source itself. Spread still changes individual
                // spark velocities around that moving point.
                const Point3 origin{
                    constrain(nozzle.position.x + emitterWobble * sinf(tick * 0.025f), 0.0f, RNDR_X - 1.0f),
                    constrain(nozzle.position.y + emitterWobble * 0.7f * sinf(tick * 0.018f),
                              0.0f, RNDR_Y - 1.0f),
                    nozzle.position.z};
                const auto velocity = GeometryEngine::helixPoint(
                    {0, 0, 0}, sample(0.1f, spread), sample(0, TWO_PI),
                    max(0.0f, lift + sample(-spread, spread)));
                pool.spawn({true, hue++, {origin, velocity}, 0.999f, gravity,
                            1.0f, 1.0f / sample(2.3f, 3.1f), CRGB::Black, false});
            }
            stepSpray(pool, side, side, true, bounce, 0.32f, settings.collide);
        }, [&] {
            for (int i = 0; i < pool.size(); ++i) {
                const auto& p = pool[i];
                if (!p.active) continue;
                const uint8_t index = settings.ageColor ? uint8_t(255.0f * p.life) : p.type;
                const auto blendMode = settings.ageColor ? LINEARBLEND_NOWRAP : LINEARBLEND;
                const uint8_t brightness = uint8_t(255.0f * min(1.0f, 2.0f * p.life));
                const uint8_t phase = p.type * 41 + int((1.0f - p.life) * 512.0f);
                const float wobble = (sin8(phase) - 128) / 128.0f;
                const float radius = max(0.0f, settings.particleRadius + settings.radiusWobble * wobble);
                drawBall(p.motion.position, radius * lengthScale,
                         ColorFromPalette(palette, index, brightness, blendMode), false);
            }
        });
    }

    inline void animateWaterfall(uint32_t durationMs, const WaterfallSettings& settings = {},
                                 const CRGBPalette16& palette = OceanColors_p) {
        using namespace Detail;
        const Side xSide = settings.walls ? Side::Bounce : (settings.cylinder ? Side::Wrap : Side::Open);
        const Side ySide = settings.walls ? Side::Bounce : Side::Open;
        const bool wrapX = xSide == Side::Wrap;
        const float bounce = (settings.collide ? settings.collide : 120) / 255.0f;
        // A clockwise quarter turn from the previous diagonal gives
        // opposite X and Y offsets. Position still shifts the line in X.
        const float centerX = RNDR_CX + (min(31, int(settings.position)) - 16) * (RNDR_X - 1) / 32.0f;
        constexpr int rows = 2;
        constexpr float rowOffset = 1.0f;
        const float requestedHalfSpan = 0.5f * (min(RNDR_X, RNDR_Y) - 1) *
                                        constrain(settings.spanFraction, 0.0f, 1.0f);
        const float halfSpan = min(requestedHalfSpan, RNDR_CY - rowOffset);
        const int firstColumn = int(ceilf(max(0.0f, centerX - halfSpan)));
        const int lastColumn = int(floorf(min(RNDR_X - 1.0f, centerX + halfSpan)));
        const int stations = max(1, lastColumn - firstColumn + 1);
        const int nozzles = rows * stations;
        // Spread the flow over time across both rows instead of launching a whole sheet at once.
        const float dropRate = settings.intensity > 0 ?
            720.0f * volumeScale / (12 - (settings.intensity >> 5)) : 0.0f;
        const int variation = settings.variation >> 3;
        const int downSpeed = -(int(settings.speed) + 7) / 8;
        const float speedScale = verticalScale / (64.0f * dt);
        const float driftSpeed = variation * min(RNDR_X, RNDR_Y) / (16.0f * 64.0f * dt);
        constexpr float diagonalComponent = 0.70710678f;
        const float bulgeComponent = settings.bulgeVoxels * min(RNDR_X, RNDR_Y) * (diagonalComponent / 16.0f);
        auto sampleDrift = [] {
            return (sample(-1.0f, 1.0f) + sample(-1.0f, 1.0f) + sample(-1.0f, 1.0f)) / 3.0f;
        };
        float emission = 0.0f;
        int nozzle = 0;
        uint8_t hues[rows * RNDR_X];
        for (int i = 0; i < nozzles; ++i) hues[i] = 90 * i;
        Pool pool(population(maxParticles));
        run(durationMs, 190, [&](uint32_t) {
            for (int i = 0; i < nozzles; ++i) {
                hues[i] += 1 + random8(settings.variation >> 1);
            }
            emission += dropRate * dt;
            while (emission >= 1.0f) {
                emission -= 1.0f;
                const int column = nozzle / rows;
                const float station = stations > 1 ? float(column) / (stations - 1) : 0.5f;
                const float x = firstColumn <= lastColumn ? float(firstColumn + column) : centerX;
                const bool backRow = nozzle % rows != 0;
                const float y = RNDR_CY - (x - centerX) + (backRow ? rowOffset : -rowOffset);
                const Point3 origin{x, y, RNDR_Z + (1.0f + 3.0f * station) * verticalScale};
                // Most launches stay near vertical. Fold cross-curtain drift forward
                // and let the center of the distribution fall exactly straight down.
                float along = sampleDrift();
                float forward = fabsf(sampleDrift());
                if (along * along + forward * forward < 0.30f * 0.30f) along = forward = 0.0f;
                const Point3 velocity{(along - forward) * diagonalComponent * driftSpeed,
                                      (-along - forward) * diagonalComponent * driftSpeed,
                                      downSpeed * speedScale};
                int slot = pool.firstFree();
                if (slot == pool.size()) {
                    // Preserve the feed under load by recycling the splash closest to fading out.
                    for (int i = 0; i < pool.size(); ++i) {
                        if ((pool[i].flags & HitFloor) && (slot == pool.size() || pool[i].life < pool[slot].life)) {
                            slot = i;
                        }
                    }
                }
                if (slot < pool.size()) {
                    pool[slot] = {true, hues[nozzle], {origin, velocity}, 0.999f, 32.0f * verticalScale,
                                  1.0f, 1.0f / sample(1.6f, 2.7f), CRGB::Black,
                                  uint8_t(backRow ? WaterfallBackRow : 0)};
                }
                nozzle = (nozzle + 1) % nozzles;
            }
            stepSpray(pool, xSide, ySide, settings.ground, bounce, 0.46f, settings.collide > 0);
        }, [&] {
            for (int i = 0; i < pool.size(); ++i) {
                const auto& p = pool[i];
                if (!p.active) continue;
                if (p.flags & HitFloor) {
                    const uint8_t brightness = uint8_t(255.0f * min(1.0f, 2.0f * p.life));
                    drawSmooth(p.motion.position, ColorFromPalette(palette, p.type, brightness), wrapX);
                } else {
                    auto position = p.motion.position;
                    const float drop = constrain(1.0f - position.z / (RNDR_Z - 1), 0.0f, 1.0f);
                    const float envelope = 4.0f * drop * (1.0f - drop);
                    // Push the rows apart across the diagonal as they fall.
                    // Return to zero offset at the floor so the splash joins cleanly.
                    const float offset = (p.flags & WaterfallBackRow ? bulgeComponent : -bulgeComponent) *
                                         envelope * envelope;
                    position.x += offset;
                    position.y += offset;
                    float unused = 0.0f;
                    if (resolveSide(position.x, unused, RNDR_X, xSide, bounce) &&
                        resolveSide(position.y, unused, RNDR_Y, ySide, bounce)) {
                        drawFallingDroplet(position, ColorFromPalette(palette, p.type), wrapX);
                    }
                }
            }
        });
    }

    inline void animateBox(uint32_t durationMs, const BoxSettings& settings = {}) {
        using namespace Detail;
        constexpr int playbackPercent = 100; // Lower for debug slow motion.
        // WLED 16.0 PS Box advances its tilt on the effect clock. Convert its
        // 1/64-pixel velocity units to voxels per second for our 60 Hz solver.
        constexpr int wledHz = 42;
        constexpr float velocityUnit = wledHz / 64.0f;
        constexpr float maxSpeed = 120.0f * velocityUnit;
        const int forcePeriod = ((255 - settings.speed) >> 6) + 1;
        const int phaseStep = (settings.speed >> 6) + 1;
        const float hardness = max(2, int(settings.hardness)) / 255.0f;
        const float wallBounce = min(200, int(settings.hardness)) / 255.0f;
        const int maxSize = min((RNDR_X * RNDR_Z) >> 2, 255);
        const int size = min(int(settings.size), 31);
        const int count = population(max(2, int(settings.particles)), BoxPool::capacity);
        BoxPool pool(count);
        // C9 New, WLED PS Box's default palette (53).
        static const uint8_t c9[] PROGMEM = {0, 255, 5, 0, 60, 255, 5, 0, 61, 196, 57, 2, 120, 196, 57, 2,
                                             121, 6, 126, 2, 180, 6, 126, 2, 181, 4, 30, 114, 255, 4, 30, 114};
        const CRGBPalette16 palette(c9);
        uint16_t phase = random16();
        uint32_t wledFrame = 0;
        uint32_t physicsTick = 0;
        int forceClock = 60 - wledHz;
        int spawnClock = 60 - wledHz;
        int physicsCredit = 100 - playbackPercent;
        int spawned = 0;
        constexpr uint8_t singleVoxelMaxSize = 2;
        constexpr uint32_t colorFadeUs = 250000;
        struct LitVoxel { uint16_t index; CRGB color; };
        // Store only occupied voxels. The animation task runs one scene at a time.
        static LitVoxel voxelFrames[2][UINT8_MAX];
        auto* previousVoxels = voxelFrames[0];
        auto* currentVoxels = voxelFrames[1];
        int previousCount = 0;
        uint32_t lastDraw = micros();
        auto radius = [](const BoxBall& p) {
            return p.type <= singleVoxelMaxSize ? 0.5f : 1.0f + p.type * (52.0f / 4096.0f);
        };
        auto bounce = [&](float& position, float& velocity, int extent, float ballRadius) {
            const float last = extent - 1.0f;
            const float inset = min(last * 0.5f, ballRadius - 0.5f);
            if (position < inset || position > last - inset) {
                if (position < inset ? velocity < 0.0f : velocity > 0.0f) velocity *= -wallBounce;
                position = constrain(position, inset, last - inset);
            }
        };
        run(durationMs, 0, [&](uint32_t) {
            // Keep the population ramp at normal speed for a short audition card.
            spawnClock += wledHz;
            if (spawnClock >= 60) {
                spawnClock -= 60;
                if (spawned < count) {
                    const uint8_t ballSize = size == 31 ? random8(maxSize) + 1 : size * maxSize / 31;
                    CRGB color = ColorFromPalette(palette, random8());
                    // Keep the C9 hue and saturation at full value for every ball.
                    const int peak = max(max(color.r, color.g), color.b);
                    if (peak > 0) {
                        color.r = color.r * 255 / peak;
                        color.g = color.g * 255 / peak;
                        color.b = color.b * 255 / peak;
                    }
                    pool[spawned++] = {true, ballSize,
                        {{sample(0, RNDR_X - 1), sample(0, RNDR_Y - 1), sample(0, RNDR_Z - 1)}, {0, 0, 0}},
                        color};
                }
            }
            // Slow the complete physics step so force, friction, and bounces stay in sync.
            physicsCredit += playbackPercent;
            if (physicsCredit < 100) return;
            physicsCredit -= 100;
            const uint32_t tick = physicsTick++;
            forceClock += wledHz;
            if (forceClock >= 60) {
                forceClock -= 60;
                Point3 impulse{};
                if (settings.speed > 0 && wledFrame % forcePeriod == 0) {
                    if (settings.washingMachine) {
                        const uint8_t wave = uint8_t((tick * 1000ULL / 60) >> 7);
                        const int pulse = wave > 127 ? wave - 127 : wave;
                        const int ramp = constrain(min(pulse, 90 - pulse), 0, 15);
                        const int step = (wave > 127 ? -127 : 127) * ramp / 15 / ((400 - settings.speed) >> 3);
                        phase = step == 0 ? 190 : uint16_t(phase + step);
                    } else {
                        phase -= phaseStep;
                    }
                    // The third phase offset extends the same shared noise field into depth.
                    auto noiseForce = [&](uint16_t offset) {
                        return (int(inoise8(uint16_t(phase + offset))) - 127) * settings.tilt / 128;
                    };
                    const uint16_t angle = uint16_t(phase << 8);
                    const int x = settings.random ? noiseForce(0) : settings.tilt * cos16(angle) / 65535;
                    const int y = settings.random ? noiseForce(20000) : 0;
                    const int z = settings.random ? noiseForce(10000) : settings.tilt * sin16(angle) / 65535;
                    constexpr float forceUnit = velocityUnit / 16.0f;
                    impulse = {constrain(x, -127, 127) * forceUnit, constrain(y, -127, 127) * forceUnit,
                               constrain(settings.sloshing ? -abs(z) : z, -127, 127) * forceUnit};
                }
                const bool friction = wledFrame++ % 16 == 0;
                auto kick = [&](float velocity, float delta) {
                    velocity = constrain(velocity + delta, -maxSpeed, maxSpeed);
                    return friction ? copysignf(max(0.0f, fabsf(velocity) - velocityUnit), velocity) : velocity;
                };
                for (int i = 0; i < spawned; ++i) {
                    auto& v = pool[i].motion.velocity;
                    v = {kick(v.x, impulse.x), kick(v.y, impulse.y), kick(v.z, impulse.z)};
                }
            }
            for (int i = 0; i < spawned; ++i) advance(pool[i].motion, dt, {}, 1.0f);
            collide(pool, hardness, false, radius);
            for (int i = 0; i < spawned; ++i) {
                auto& ball = pool[i];
                auto& m = ball.motion;
                const float r = radius(ball);
                bounce(m.position.x, m.velocity.x, RNDR_X, r);
                bounce(m.position.y, m.velocity.y, RNDR_Y, r);
                bounce(m.position.z, m.velocity.z, RNDR_Z, r);
            }
        }, [&] {
            int currentCount = 0;
            for (int i = 0; i < spawned; ++i) {
                const auto& ball = pool[i];
                const auto& p = ball.motion.position;
                if (ball.type <= singleVoxelMaxSize) {
                    const int x = lroundf(p.x), y = lroundf(p.y), z = lroundf(p.z);
                    const uint16_t index = (z * RNDR_Y + y) * RNDR_X + x;
                    currentVoxels[currentCount++] = {index, ball.color};
                } else {
                    drawBall(p, 0.5f + ball.type / 64.0f, ball.color, false, true);
                }
            }
            std::sort(currentVoxels, currentVoxels + currentCount,
                      [](const LitVoxel& a, const LitVoxel& b) { return a.index < b.index; });
            const uint32_t now = micros();
            const int maxChange = (255 * min(now - lastDraw, colorFadeUs) + colorFadeUs - 1) / colorFadeUs;
            lastDraw = now;
            auto approach = [maxChange](uint8_t from, uint8_t to) -> uint8_t {
                return from + constrain(int(to) - from, -maxChange, maxChange);
            };
            int old = 0, written = 0;
            for (int i = 0; i < currentCount;) {
                const uint16_t index = currentVoxels[i].index;
                CRGB color = currentVoxels[i++].color;
                while (i < currentCount && currentVoxels[i].index == index) color += currentVoxels[i++].color;
                while (old < previousCount && previousVoxels[old].index < index) ++old;
                if (old < previousCount && previousVoxels[old].index == index) {
                    const auto& before = previousVoxels[old].color;
                    color = CRGB(approach(before.r, color.r), approach(before.g, color.g), approach(before.b, color.b));
                }
                currentVoxels[written++] = {index, color};
                // Keep the visible peak at full value, but retain the raw fade state.
                const int peak = max(max(color.r, color.g), color.b);
                if (peak > 0 && peak < 255) {
                    color = CRGB(color.r * 255 / peak, color.g * 255 / peak, color.b * 255 / peak);
                }
                const int x = index % RNDR_X, y = index / RNDR_X % RNDR_Y, z = index / (RNDR_X * RNDR_Y);
                setVoxel(x, y, z, getVoxel(x, y, z) + color);
            }
            previousCount = written;
            std::swap(previousVoxels, currentVoxels);
        });
    }

    inline void animateGhostRider(uint32_t durationMs, const GhostRiderSettings& settings = {},
                                   const CRGBPalette16& palette = RainbowColors_p) {
        using namespace Detail;
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

    inline void animateAttractor(uint32_t durationMs, const AttractorSettings& settings = {},
                                 const CRGBPalette16& palette = OceanColors_p) {
        using namespace Detail;
        const Point3 center{RNDR_CX, RNDR_CY, RNDR_CZ};
        const float faceDistance = min(RNDR_CX, min(RNDR_CY, RNDR_CZ));
        const float orbitRadius = max(1.0f, faceDistance * constrain(settings.orbitRadius, 0.0f, 1.0f));
        const float orbitSpeed = sqrtf(settings.mass * wledGravity);
        constexpr float minLifeSeconds = 3.4f;
        constexpr float maxLifeSeconds = 5.6f;
        constexpr uint8_t nozzleCount = 3;
        constexpr uint32_t nozzlePeriod = 336; // Eight seconds at 42 effect updates per second.
        Motion attractor{center, {0, 0, -7 * wledVelocity}};
        Pool pool(population((25 + 165 * settings.particles / 255) * maxParticles / 256));
        const int frictionPeriod = 33 - min(31, int(settings.friction));
        const float bounce = (120 + 135 * settings.collide / 255.0f) / 255.0f;
        const uint8_t size = settings.size >> 1;
        uint32_t frame = 0;
        uint8_t hue = random8();
        uint8_t nozzle = 0;
        float emission = 0.0f;
        // Each tilted plane lines up with a different axis at its outermost point.
        const auto orientOrbit = [](float u, float v, float depth, uint8_t path) -> Point3 {
            const float across = 0.819152f * v - 0.573576f * depth;
            const float up = 0.573576f * v + 0.819152f * depth;
            if (path == 0) return {u, across, up};
            if (path == 1) return {up, u, across};
            return {across, up, u};
        };
        run(durationMs, 0, [&](uint32_t tick) {
            const bool emitFrame = wledFrameDue(tick);
            bool applyFriction = false;
            float mass = settings.mass;
            if (liveAudioSource()) mass *= RemoteInputs::audio().volumeSm / 255.0f;
            if (emitFrame) {
                if (settings.move && frame % 3 == 0) bounceEmitter(attractor, 1.0f / 42, 7 * wledVelocity);
                if (frame % 5 == 0) ++hue;
                applyFriction = frame % frictionPeriod == 0;
                // At 16³ this emits about 14 particles per second across all three clouds.
                emission += volumeScale / 3.0f;
                if (emission >= 1.0f) {
                    emission -= 1.0f;
                    const float phase = (frame % nozzlePeriod) * (TWO_PI / nozzlePeriod) +
                                        nozzle * (TWO_PI / nozzleCount);
                    const float radius = orbitRadius * sample(0.97f, 1.03f);
                    const float c = cosf(phase), s = sinf(phase);
                    const float depth = orbitRadius * sample(-0.025f, 0.025f);
                    const Point3 offset = orientOrbit(radius * c, radius * s, depth, nozzle);
                    const auto& anchor = attractor.position;
                    const Point3 origin{anchor.x + offset.x, anchor.y + offset.y, anchor.z + offset.z};
                    // Start close together. Differences in speed and tilt separate the older sparks.
                    const float speed = orbitSpeed * sample(0.89f, 1.01f);
                    const float normalSpeed = orbitSpeed * sample(-0.12f, 0.12f);
                    const Point3 v = orientOrbit(-speed * s, speed * c, normalSpeed, nozzle);
                    pool.spawn({true, hue, {origin, v}, 1.0f, 0.0f, 1.0f,
                                1.0f / sample(minLifeSeconds, maxLifeSeconds), CRGB::Black, 0});
                    nozzle = (nozzle + 1) % nozzleCount;
                }
                ++frame;
            }
            for (int i = 0; i < pool.size(); ++i) {
                auto& p = pool[i];
                if (!p.active) continue;
                auto& v = p.motion.velocity;
                const auto& pos = p.motion.position;
                const Point3 delta{attractor.position.x - pos.x, attractor.position.y - pos.y,
                                   attractor.position.z - pos.z};
                const float r2 = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
                // WLED softens the central pull and ages particles quickly in Swallow mode.
                const float force = mass * wledGravity / max(2.0f, r2);
                constexpr float limit = 120.0f * wledVelocity;
                const auto updateVelocity = [&](float speed, float offset) {
                    speed = constrain(speed + force * offset * dt, -limit, limit);
                    // Retain fractional velocity so the drag doesn't drain the orbit into the center.
                    return applyFriction ? speed * (253.0f / 255.0f) : speed;
                };
                v = {updateVelocity(v.x, delta.x), updateVelocity(v.y, delta.y), updateVelocity(v.z, delta.z)};
                advance(p.motion, dt, {}, 1.0f);
                p.life -= p.decay * dt * (settings.swallow && r2 < 2.0f ? 9.0f : 1.0f);
                if (p.life <= 0.0f) p.active = false;
            }
            if (settings.collide) collide(pool, bounce, false, [size](const Particle&) {
                return size == 0 ? 0.5f : min(2.0f, 1.0f + size * (52.0f / 4096.0f));
            });
        }, [&] {
            for (int i = 0; i < pool.size(); ++i) {
                const auto& p = pool[i];
                if (!p.active) continue;
                const auto& pos = p.motion.position;
                // A shared rainbow wave gives neighbors similar colors. Age nudges the hue
                // within that wave and fades particles in and out of the cloud.
                const float gradient = (pos.x - center.x + pos.z - center.z) * (36.0f / orbitRadius);
                const uint8_t index = settings.ageColor ? uint8_t(int(frame / 2) + int(gradient + 18.0f * p.life)) : p.type;
                const float elapsed = (1.0f - p.life) / p.decay;
                const uint8_t brightness = uint8_t(255.0f * min(1.0f, min(elapsed * 3.0f, p.life * 3.0f)));
                const auto color = ColorFromPalette(palette, index, brightness);
                if (size == 0) drawParticlePoint(p.motion.position, color);
                else if (size == 1) drawSmooth(p.motion.position, color, false);
                else drawBall(p.motion.position, 0.5f + size / 64.0f, color, false);
            }
        });
    }

    inline void animateSpray(uint32_t durationMs, const SpraySettings& settings = {},
                             const CRGBPalette16& palette = PartyColors_p) {
        using namespace Detail;
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
                const bool reactive = liveAudioSource();
                const auto audio = reactive ? RemoteInputs::audio() : RemoteInputs::AudioFrame{};
                const int period = reactive ? 11 - audio.volumeSm / 25 : 11 - settings.intensity / 25;
                if (frame % period == 0 || (reactive && frame % 20 == 0)) {
                    emission += volumeScale;
                    if (emission >= 1.0f) {
                        emission -= 1.0f;
                        const float speed = ((settings.speed >> 2) + (audio.volumeRaw >> 3)) * wledVelocity;
                        const int spread = reactive ? 1 + ((audio.volumeRaw * settings.speed) >> 12) : 3;
                        const float horizontal = speed * cosf(angle) * 0.70710678f;
                        const Point3 v{horizontal + sample(-spread, spread) * wledVelocity,
                                       horizontal + sample(-spread, spread) * wledVelocity,
                                       speed * sinf(angle) + sample(-spread, spread) * wledVelocity};
                        const float life = reactive ? sample(30, max(31, (audio.volumeSm + settings.intensity) / 2)) :
                                                     sample(150 + settings.intensity, 300 + settings.intensity);
                        hue += reactive ? audio.volumeSm / 30 : 1;
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

    inline void animateGeq(uint32_t durationMs, const GeqSettings& settings = {},
                           const CRGBPalette16& palette = PartyColors_p, bool (*keepDrawing)() = nullptr) {
        using namespace Detail;
        const Side xSide = settings.walls ? Side::Bounce : (settings.cylinder ? Side::Wrap : Side::Open);
        const Side ySide = settings.walls ? Side::Bounce : Side::Open;
        const float bounce = settings.bounce / 255.0f;
        const float gravity = min(31, int(settings.gravity)) * 4 * wledGravity * verticalScale;
        const int threshold = 300 - settings.intensity;
        const float divergence = (settings.diverge >> 2) * wledVelocity;
        Pool pool(population(maxParticles * 2 / 3));
        run(durationMs, 0, [&](uint32_t tick) {
            if (wledFrameDue(tick)) {
                const auto audio = RemoteInputs::audio();
                int nextSlot = 0;
                for (int bin = 0; bin < 16; ++bin) {
                    const uint8_t level = audio.fftResult[bin];
                    if (!emitBand(level, threshold) || (volumeScale < 1.0f && sample(0, 1) >= volumeScale)) continue;
                    nextSlot = pool.firstFree(nextSlot);
                    if (nextSlot == pool.size()) break;
                    const int speed = (level * settings.speed) >> 9;
                    const int lifetimeVariation = speed ? random16(speed) : 0;
                    const float life = 20 + (speed >> 1) + (speed - (speed >> 1) + lifetimeVariation) *
                                       (settings.intensity / 255.0f);
                    // Sixteen bands in four rows, with bass-to-treble groups along X.
                    const Point3 origin{(bin / 4 + sample(0, 1)) * (RNDR_X - 1) / 4.0f,
                                        (bin % 4 + sample(0, 1)) * (RNDR_Y - 1) / 4.0f, 0.0f};
                    const Point3 velocity{sample(-divergence, divergence), sample(-divergence, divergence),
                                          speed * wledVelocity * verticalScale};
                    pool[nextSlot++] = {true, uint8_t(bin * 16 + int(random8(17)) - 8), {origin, velocity},
                                        1.0f, gravity, 1.0f, 42.0f / life, CRGB::Black, 0};
                }
            }
            stepSpray(pool, xSide, ySide, settings.floor, bounce, 0.0f, false);
            if (!settings.gravity) resolveSprayTop(pool, settings.floor ? Side::Bounce : Side::Open, bounce);
        }, [&] { drawSpray(pool, palette, xSide == Side::Wrap); }, keepDrawing);
    }

    inline void animateGeqNova(uint32_t durationMs, const GeqNovaSettings& settings = {},
                               bool (*keepDrawing)() = nullptr, const CRGBPalette16* selectedPalette = nullptr) {
        using namespace Detail;
        // WLED 16.0's palette 13 is Sunset Real.
        static const uint8_t sunset[] PROGMEM = {0,181,0,0, 22,218,85,0, 51,255,170,0, 85,211,85,77,
                                                 135,167,0,169, 198,73,0,188, 255,0,0,207};
        const CRGBPalette16 palette = selectedPalette ? *selectedPalette : CRGBPalette16(sunset);
        const Point3 center{RNDR_CX, RNDR_CY, RNDR_CZ};
        const int threshold = 300 - settings.intensity;
        const float spread = (min(31, int(settings.nozzle)) >> 2) * wledVelocity;
        const float turn = (settings.direction ? 1.0f : -1.0f) * settings.rotationSpeed * (TWO_PI / 16384.0f);
        constexpr float goldenAngle = 2.39996323f;
        Pool pool(population(maxParticles));
        float rotation = 0.0f;
        uint32_t frame = 0;
        uint8_t hueOffset = 0;
        run(durationMs, 0, [&](uint32_t tick) {
            if (wledFrameDue(tick)) {
                const auto audio = RemoteInputs::audio();
                rotation += turn;
                if (rotation >= TWO_PI) rotation -= TWO_PI;
                if (rotation <= -TWO_PI) rotation += TWO_PI;
                if (settings.colorChange && frame % (32 - (settings.colorChange >> 3)) == 0) {
                    hueOffset += 1 + (settings.colorChange >> 4);
                }
                ++frame;
                const uint8_t first = random8(16);
                int nextSlot = 0;
                for (int i = 0; i < 16; ++i) {
                    const int bin = (i + first) & 15;
                    const uint8_t level = audio.fftResult[bin];
                    if (!emitBand(level, threshold) || (volumeScale < 1.0f && sample(0, 1) >= volumeScale)) continue;
                    nextSlot = pool.firstFree(nextSlot);
                    if (nextSlot == pool.size()) break;
                    // Equal-area Fibonacci directions extend the original radial spray to a sphere.
                    const float z = 1.0f - (bin + 0.5f) / 8.0f;
                    const float speed = (5 + ((level * (settings.speed + 20)) >> 10)) * wledVelocity;
                    auto v = GeometryEngine::helixPoint({}, speed * sqrtf(1.0f - z * z),
                                                       bin * goldenAngle + rotation, speed * z);
                    v.x += sample(-spread, spread);
                    v.y += sample(-spread, spread);
                    v.z += sample(-spread, spread);
                    pool[nextSlot++] = {true, uint8_t(bin * 16 + hueOffset), {center, v}, 1.0f, 0.0f,
                                        1.0f, 42.0f / sample(200, 400), CRGB::Black, 0};
                }
            }
            stepSpray(pool, Side::Open, Side::Open, false, 0.0f, 0.0f, false);
            resolveSprayTop(pool, Side::Open, 0.0f);
        }, [&] { drawSpray(pool, palette, false); }, keepDrawing);
    }

    inline void animateGalaxy(uint32_t durationMs, const GalaxySettings& settings = {},
                              const CRGBPalette16* selectedPalette = nullptr) {
        using namespace Detail;
        // WLED 16.0 palette 59, Fairy Reaf.
        static const uint8_t fairyReaf[] PROGMEM = {0,220,19,187, 160,12,225,219, 219,203,242,223, 255,255,255,255};
        const CRGBPalette16 palette = selectedPalette ? *selectedPalette : CRGBPalette16(fairyReaf);
        const Point3 center{RNDR_CX, RNDR_CY, RNDR_CZ};
        constexpr float diskCos = 0.819152f, diskSin = 0.573576f; // A 35 degree tilt.
        const float widthScale = cbrtf(volumeScale);
        const float maxDistance = (RNDR_X + RNDR_Y + RNDR_Z) / 6.0f;
        const int colorControl = min(31, int(settings.color));
        const int huePeriod = (33 - colorControl) >> 1;
        const uint8_t size = settings.trace ? uint8_t(settings.size != 0) : settings.size;
        Pool pool(population(maxParticles));
        Motion nozzle{center, {-4 * wledVelocity, 0, 0}};
        uint8_t hue = random8();
        uint32_t frame = 0;
        run(durationMs, settings.trace ? 250 : 0, [&](uint32_t tick) {
            if (wledFrameDue(tick)) {
                if (frame % huePeriod == 0) hue += 2;
                if (random8() < (10 + (settings.intensity >> 1)) * volumeScale) {
                    const int variance = settings.starfield ? 7 : 1;
                    Point3 v{sample(-variance, variance) * wledVelocity, sample(-variance, variance) * wledVelocity,
                             sample(-variance, variance) * wledVelocity};
                    Point3 origin = nozzle.position;
                    if (settings.starfield) {
                        origin = center;
                    } else {
                        // Keep the wandering source and its seeds within a thin, tilted disk.
                        const float normal = -(origin.y - center.y) * diskSin + (origin.z - center.z) * diskCos;
                        origin.y += normal * diskSin * 0.82f;
                        origin.z -= normal * diskCos * 0.82f;
                        const float normalVelocity = -v.y * diskSin + v.z * diskCos;
                        v.y += normalVelocity * diskSin * 0.80f;
                        v.z -= normalVelocity * diskCos * 0.80f;
                    }
                    pool.spawn({true, hue, {origin, v}, 1.0f, 0.0f, 1.0f, 42.0f / sample(800, 4000), CRGB::Black, 0});
                }
                if (!settings.starfield && frame % 4 == 0) bounceEmitter(nozzle, 1.0f / 42, 4 * wledVelocity);
                ++frame;
            }
            for (int i = 0; i < pool.size(); ++i) {
                auto& p = pool[i];
                if (!p.active) continue;
                advance(p, dt);
                auto& pos = p.motion.position;
                const float x = pos.x - center.x, y = pos.y - center.y, z = pos.z - center.z;
                if (settings.starfield) {
                    const float distance = sqrtf(x*x + y*y + z*z);
                    const float expand = (1 + (1 + (settings.speed >> 1)) * distance * 64.0f) * (42.0f / 400000.0f);
                    pos.x += dt * (expand * x + wledVelocity * y);
                    pos.y += dt * (expand * y - wledVelocity * x);
                    pos.z += dt * expand * z;
                    p.active = p.active && pos.x > -1 && pos.x < RNDR_X && pos.y > -1 && pos.y < RNDR_Y &&
                               pos.z > -1 && pos.z < RNDR_Z;
                } else {
                    const float diskY = y * diskCos + z * diskSin;
                    const float normal = -y * diskSin + z * diskCos;
                    const float r = max(20.0f / 64.0f, sqrtf(x*x + diskY*diskY));
                    const float angular = (2 + (50 + settings.speed) * widthScale / r) * (42.0f / 1024.0f);
                    const float pull = (21.0f / 64.0f) / max(0.18f, r - 19.0f / 64.0f);
                    const float flowY = -angular * x - pull * diskY;
                    pos.x += dt * (angular * diskY - pull * x);
                    pos.y += dt * (diskCos * flowY + 0.8f * diskSin * normal);
                    pos.z += dt * (diskSin * flowY - 0.8f * diskCos * normal);
                    if (r < 2.0f * widthScale) p.life -= 4 * p.decay * dt;
                    if (p.life <= 0.0f) p.active = false;
                }
            }
        }, [&] {
            for (int i = 0; i < pool.size(); ++i) {
                const auto& p = pool[i];
                if (!p.active) continue;
                const auto& pos = p.motion.position;
                const float x = pos.x - center.x, y = pos.y - center.y, z = pos.z - center.z;
                const float distance = sqrtf(x*x + y*y + z*z);
                uint8_t index = p.type;
                if (colorControl == 0) index = uint8_t(min(180.0f, 180.0f * distance / maxDistance));
                if (colorControl == 31) index = uint8_t(int(p.life * 42.0f / p.decay) >> 2);
                auto color = ColorFromPalette(palette, index);
                if (!settings.starfield) {
                    color = blend(CRGB(CRGB::White), color, uint8_t(255.0f * min(1.0f, distance / (2 * widthScale))));
                }
                color.nscale8(uint8_t(min(255.0f, 84.0f * p.life / p.decay)));
                if (size == 0) drawParticlePoint(pos, color);
                else if (settings.trace) drawSmooth(pos, color, false);
                else drawBall(pos, 0.65f + size / 64.0f, color, false);
            }
        });
    }
}
