#pragma once

#include "ParticleEngine.h"
#include "TestDemo.h"

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

    namespace Detail {
        using namespace ParticleEngine;

        constexpr float verticalScale = RNDR_Z / 16.0f;
        constexpr float volumeScale = RNDR_X * RNDR_Y * RNDR_Z / 4096.0f;
        constexpr float dt = 0.016667f;

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
        inline void run(uint32_t durationMs, uint8_t blur, Step step, Draw draw) {
            constexpr uint32_t stepUs = 16667;
            const uint32_t start = millis();
            uint32_t lastFrame = micros();
            uint32_t accrued = stepUs;
            uint32_t tick = 0;
            clearAll();
            while (TestDemo::routineTimeRemaining(millis() - start, durationMs)) {
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

        inline void stepSpray(Pool& pool, Side xSide, Side ySide, bool ground,
                              float bounce, float roughness, bool collisions) {
            for (int i = 0; i < pool.size(); ++i) {
                auto& p = pool[i];
                if (p.active) advance(p, dt);
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
                const float y = RNDR_CY - (x - centerX) + (nozzle % rows == 0 ? -rowOffset : rowOffset);
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
                                  1.0f, 1.0f / sample(1.6f, 2.7f), CRGB::Black, false};
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
                    drawFallingDroplet(p.motion.position, ColorFromPalette(palette, p.type), wrapX);
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
}
