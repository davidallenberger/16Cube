#pragma once
#include "Shapes.h"
#include "AppMemory.h"
#include "ParticleEffectSupport.h"
#include "TextureEngine.h"

#include <algorithm>
#include <type_traits>

class Balls {
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

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
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

    // Box balls have no lifetime or per-particle drag, so 200 fit in the shared arena.
    struct BoxBall { bool active; uint8_t type; ParticleEngine::Motion motion; CRGB color; };
    using BoxPool = ParticleEngine::ParticlePool<BoxBall>;
    static_assert(BoxPool::capacity >= 200, "Box must fit 200 balls in the shared arena");

public:
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

    // --- animateRollingBall ---
    static void animateRollingBall(uint32_t durationMs, RenderMode rMode, CRGBPalette16 pal, ShaderMode sMode, uint32_t speedMs = 15) {
        pal = Shapes::applyShapeColorHacks(SHAPE_SPHERE, pal, sMode);
        rMode = Shapes::applyShapeRenderHacks(SHAPE_SPHERE, rMode);
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

    // ==========================================
    // 3D BOUNCING BALLS (Juggler's Matrix)
    // ==========================================
    struct BouncingBall {
        uint32_t lastBounceTime;
        float impactVelocity;
        float height;
    };

    static void animateBouncingBalls(uint32_t durationMs, CRGBPalette16 pal = RainbowColors_p, uint8_t gravityParam = 128, uint8_t numBallsParam = 128) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();

        // 1. FIT ALL COLUMNS INSIDE THE FIXED SHARED ARENA
        // Each column owns MAX_BALLS_PER_COL slots. This is also the stride
        // in both indexing loops below: c * MAX_BALLS_PER_COL + i.
        // Reducing only numBalls would leave the old large stride in place
        // and would STILL write beyond the arena on the later columns.
        constexpr int TOTAL_COLS = RNDR_X * RNDR_Y;
        constexpr int BALLS_PER_COL_CAPACITY =
            SHARED_APP_MEMORY_BYTES / (TOTAL_COLS * sizeof(BouncingBall));
        static_assert(BALLS_PER_COL_CAPACITY > 0, "One ball per column does not fit the arena");

        const int requestedMaxBallsPerCol = (RNDR_X == 10) ? 6 : ((RNDR_X > 10) ? 9 : 10);
        const int MAX_BALLS_PER_COL = min(requestedMaxBallsPerCol, BALLS_PER_COL_CAPACITY);

        // With the current 12-byte structure: 16x16 permits 2 balls/column;
        // 10x10 retains 6; 8x8 retains 10. No larger allocation is requested.
        BouncingBall* balls = reinterpret_cast<BouncingBall*>(SharedAppMemory);

        // The input knob still selects between one ball and the safe maximum.
        unsigned numBalls = (numBallsParam * (MAX_BALLS_PER_COL - 1)) / 255 + 1;

        // Pick each ball's color ONCE, before drawing any frames. On a 16^3
        // cube the default density selects one ball per column, so every
        // column uses ball ID zero. Lava's palette entry zero is black;
        // previously that made the entire scene invisible for that palette.
        //
        // Preserve the normal ID-to-color mapping when its color is visible.
        // If it is black, search the remaining palette entries for a visible
        // color. An intentionally all-black palette remains all black.
        // This small local array uses 6 bytes on 16^3; the shared arena,
        // physics, number of balls, and frame timing do not change.
        CRGB ballColors[BALLS_PER_COL_CAPACITY];

        for (unsigned i = 0; i < numBalls; ++i) {
            const uint8_t colorIndex = i * (256 / max((int)numBalls, 8));
            CRGB color = ColorFromPalette(pal, colorIndex, 255, LINEARBLEND);

            for (uint8_t entry = 0; entry < 16; ++entry) {
                if (color.r != 0 || color.g != 0 || color.b != 0) {
                    break;
                }

                const uint8_t fallbackIndex = entry * 16;
                color = ColorFromPalette(pal, fallbackIndex, 255, LINEARBLEND);
            }

            ballColors[i] = color;
        }

        // 2. WLED PHYSICS TRANSLATION
        // WLED alters gravity by dividing the time delta. 128 = half speed.
        float timeDivisor = ((255 - gravityParam) / 64.0f) + 1.0f;
        const float gravity = -9.81f;

        // INITIALIZATION
        for (int c = 0; c < TOTAL_COLS; c++) {
            for (unsigned i = 0; i < numBalls; i++) {
                int idx = c * MAX_BALLS_PER_COL + i;
                balls[idx].lastBounceTime = startTime;

                // Randomize initial kick velocity so they immediately decouple
                float impactVelocityStart = sqrtf(-2.0f * gravity) * (random8(5, 11) / 10.0f);
                balls[idx].impactVelocity = impactVelocityStart;
                balls[idx].height = 0.0f;
            }
        }

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrame;
            if (deltaMs == 0) { yield(); continue; }
            lastFrame = now;

            clearAll();

            for (int x = 0; x < RNDR_X; x++) {
                for (int y = 0; y < RNDR_Y; y++) {
                    int c = x + y * RNDR_X;

                    for (unsigned i = 0; i < numBalls; i++) {
                        int idx = c * MAX_BALLS_PER_COL + i;

                        float timeSinceLastBounce = (now - balls[idx].lastBounceTime) / timeDivisor;
                        float timeSec = timeSinceLastBounce / 1000.0f;

                        // Physics: Height = 1/2 * a * t^2 + v * t
                        balls[idx].height = (0.5f * gravity * timeSec + balls[idx].impactVelocity) * timeSec;

                        // Floor Collision
                        if (balls[idx].height <= 0.0f) {
                            balls[idx].height = 0.0f;

                            // WLED Dampening: Balls lose energy at different rates based on their ID
                            float dampening = 0.9f - (float)i / (float)(numBalls * numBalls);
                            balls[idx].impactVelocity *= dampening;
                            balls[idx].lastBounceTime = now;

                            // If it rests on the floor, give it a new random kick to keep it alive
                            if (balls[idx].impactVelocity < 0.015f) {
                                balls[idx].impactVelocity = sqrtf(-2.0f * gravity) * (random8(5, 11) / 10.0f);
                            }
                        } else if (balls[idx].height > 1.0f) {
                            continue; // Prevent clipping outside the top glass
                        }

                        // 3. PHYSICAL Z-MAPPING
                        int z = constrain((int)roundf(balls[idx].height * (RNDR_Z - 1)), 0, RNDR_Z - 1);

                        // Each ball keeps the visible palette color chosen
                        // for its ID when this invocation started.
                        const CRGB color = ballColors[i];

                        // Additive blending allows colors to mix beautifully when passing through each other
                        CRGB existing = getVoxel(x, y, z);
                        setVoxel(x, y, z, existing + color);
                    }
                }
            }
            showCube();
            delay(15);
        }
    }

    static void animatePSBox(uint32_t durationMs) {
        animatePSBox(durationMs, BoxSettings{});
    }

    static void animatePSBox(uint32_t durationMs, const BoxSettings& settings) {
        using namespace ParticleEffectSupport;
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

    static void animateBouncingBall(uint32_t durationMs, uint32_t speedMs = 15) {
        using namespace ParticleEngine;
        constexpr float initialSpeedScale = 1.35f;
        constexpr float gravity = 38.0f * initialSpeedScale;
        constexpr float verticalRestitution = 0.84f;
        constexpr float wallRestitution = 0.9f;
        constexpr float floorFriction = 0.08f;
        // Measure total mechanical energy as an equivalent drop height.
        constexpr float minKickThresholdHeight = 0.10f * (RNDR_Z - 1);
        constexpr float maxKickThresholdHeight = 0.15f * (RNDR_Z - 1);
        constexpr float sidewaysShare = 0.08f;
        // The strongest request can reach the ceiling from the floor
        // even with a sideways kick, if the shared energy budget allows it.
        constexpr float maxKickTravel = 1.15f;
        constexpr float maxRadius = 2.5f;
        constexpr float minRadius = maxRadius * 0.5f;
        constexpr float maxBallVolume = maxRadius * maxRadius * maxRadius;
        constexpr float minBallVolume = minRadius * minRadius * minRadius;
        constexpr int originalCount = RNDR_X > 8 ? 3 : 1;
        constexpr int maxBalls = originalCount + 2;
        const int count = random8(max(2, originalCount), maxBalls + 1);
        // The common 4π/3 factor cancels, so budget and mass use radius cubed.
        float remainingVolume = originalCount * maxBallVolume * (random16(820, 1001) / 1000.0f);
        struct Ball {
            Motion motion;
            float radius;
            float inverseMass;
            float rotX, rotY;
            TextureState texture;
            RenderMode mode;
        };
        static_assert(ParticlePool<Ball>::capacity >= maxBalls, "Bouncing balls must fit the shared arena");
        static_assert(std::is_trivially_destructible<Ball>::value, "Arena ball state must not require teardown");
        // Each texture contains voxel caches. Keep them out of the loop stack so
        // showCube() has room to handle a button press and unwind the scene.
        ParticlePool<Ball> balls(count);
        const int solidCount = random8(1, count);
        const uint8_t solidHue = random8();
        const auto sample = [](float low, float high) {
            return low + (high - low) * (random16() / 65536.0f);
        };
        const auto mechanicalEnergy = [gravity](const Ball& ball) {
            const auto& v = ball.motion.velocity;
            const float height = ball.motion.position.z - ball.radius;
            return (0.5f * (v.x * v.x + v.y * v.y + v.z * v.z) + gravity * height)
                / ball.inverseMass;
        };
        float totalMass = 0.0f;
        float initialEnergy = 0.0f;

        for (int i = 0; i < count; ++i) {
            const int remainingBalls = count - i - 1;
            const float minVolume = max(minBallVolume, remainingVolume - remainingBalls * maxBallVolume);
            const float maxVolume = min(maxBallVolume, remainingVolume - remainingBalls * minBallVolume);
            const float volume = sample(minVolume, maxVolume);
            remainingVolume -= volume;

            auto& ball = balls[i];
            const float r = ball.radius = constrain(cbrtf(volume), minRadius, maxRadius);
            const float mass = r * r * r;
            ball.inverseMass = 1.0f / mass;
            totalMass += mass;
            // Start near the ceiling. If the top is crowded, try lower positions.
            for (int attempt = 0; attempt < 32; ++attempt) {
                ball.motion.position = {sample(r, RNDR_X - 1.0f - r), sample(r, RNDR_Y - 1.0f - r),
                    attempt < 12 ? RNDR_Z - 1.0f - r : sample(r, RNDR_Z - 1.0f - r)};
                const auto& p = ball.motion.position;
                bool overlaps = false;
                for (int j = 0; j < i; ++j) {
                    const auto& other = balls[j].motion.position;
                    const float dx = p.x - other.x, dy = p.y - other.y, dz = p.z - other.z;
                    const float contact = r + balls[j].radius;
                    if (dx*dx + dy*dy + dz*dz < contact * contact) {
                        overlaps = true;
                        break;
                    }
                }
                if (!overlaps) break;
            }
            ball.motion.velocity = {
                (random16(20, 50) / 10.0f) * (random8(2) == 0 ? 1.0f : -1.0f) * initialSpeedScale,
                (random16(20, 50) / 10.0f) * (random8(2) == 0 ? 1.0f : -1.0f) * initialSpeedScale,
                0.0f
            };
            initialEnergy += mechanicalEnergy(ball);

            if (i < solidCount) {
                const uint8_t hue = solidHue + (256 * i / solidCount);
                ball.texture = TextureState(MODE_SOLID, CRGBPalette16(CHSV(hue, 255, 255)), 1.66f);
            } else {
                const TexturePlan plan = PaletteUtils::getRandomNonSolidTexturePlan();
                ball.texture = TextureState(plan.sMode, plan.palette, 1.66f);
                ball.texture.internalPhase = i * 128.0f;
                ball.texture.phase = uint32_t(ball.texture.internalPhase);
            }
            ball.mode = random8(100) < 30 ? RENDER_SHELL : RENDER_SOLID;
        }

        const auto nextKickThreshold = [&] {
            return totalMass * gravity * sample(minKickThresholdHeight, maxKickThresholdHeight);
        };
        float kickThreshold = nextKickThreshold();
        const uint32_t start = millis();
        uint32_t lastFrame = start;
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - start), durationMs)) {
            const uint32_t now = millis();
            const uint32_t deltaMs = now - lastFrame;
            if (deltaMs == 0) { yield(); continue; }
            lastFrame = now;

            const float dt = min(0.05f, deltaMs / 1000.0f);
            const int substeps = int(ceilf(dt / 0.0125f));
            const float step = dt / substeps;
            for (int tick = 0; tick < substeps; ++tick) {
                for (int i = 0; i < count; ++i) {
                    auto& ball = balls[i];
                    auto& motion = ball.motion;
                    auto& v = motion.velocity;
                    if (motion.position.z <= ball.radius && v.z == 0.0f) {
                        // A supported ball loses horizontal speed to the floor.
                        const float speed = sqrtf(v.x * v.x + v.y * v.y);
                        const float retained = speed > 0.0f
                            ? max(0.0f, 1.0f - floorFriction * gravity * step / speed) : 0.0f;
                        advance(motion, step, {}, retained);
                    } else {
                        // Use midpoint vertical speed to integrate constant gravity.
                        const float halfKick = 0.5f * gravity * step;
                        v.z -= halfKick;
                        advance(motion, step, {}, 1.0f);
                        v.z -= halfKick;
                    }
                }
                for (int i = 0; i < count; ++i) {
                    for (int j = i + 1; j < count; ++j) {
                        auto& a = balls[i];
                        auto& b = balls[j];
                        resolveSphereContact(a.motion, b.motion, a.radius + b.radius, 0.97f,
                                             false, a.inverseMass, b.inverseMass);
                    }
                }
                for (int i = 0; i < count; ++i) {
                    auto& ball = balls[i];
                    auto& p = ball.motion.position;
                    auto& v = ball.motion.velocity;
                    const float r = ball.radius;
                    resolveSide(p.x, v.x, RNDR_X, Side::Bounce, wallRestitution, r);
                    resolveSide(p.y, v.y, RNDR_Y, Side::Bounce, wallRestitution, r);
                    const float ceiling = RNDR_Z - 1.0f - r;
                    // Recover the speed at the contact plane before applying
                    // restitution. The portion below the floor adds no energy.
                    if (p.z < r && v.z < 0.0f) {
                        v.z = -sqrtf(max(0.0f, v.z * v.z - 2.0f * gravity * (r - p.z)));
                    } else if (p.z > ceiling && v.z > 0.0f) {
                        v.z = sqrtf(v.z * v.z + 2.0f * gravity * (p.z - ceiling));
                    }
                    resolveSide(p.z, v.z, RNDR_Z, Side::Bounce, verticalRestitution, r);
                    if (p.z == r && v.z < 1.0f) v.z = 0.0f;
                }
            }

            float energy = 0.0f;
            for (int i = 0; i < count; ++i) {
                energy += mechanicalEnergy(balls[i]);
            }
            const float availableEnergy = initialEnergy - energy;
            if (energy < kickThreshold && availableEnergy > 0.0f) {
                // Every ball gets a kick. Scale the whole batch if the
                // combined requests exceed the remaining energy budget.
                float kickEnergy[maxBalls];
                float requestedEnergy = 0.0f;
                for (int i = 0; i < count; ++i) {
                    const auto& ball = balls[i];
                    const float travel = RNDR_Z - 1.0f - 2.0f * ball.radius;
                    kickEnergy[i] = gravity * travel * sample(0.35f, maxKickTravel) / ball.inverseMass;
                    requestedEnergy += kickEnergy[i];
                }
                const float scale = min(1.0f, availableEnergy / requestedEnergy);
                for (int i = 0; i < count; ++i) {
                    auto& ball = balls[i];
                    auto& v = ball.motion.velocity;
                    const float addedSpeedSq = 2.0f * kickEnergy[i] * scale * ball.inverseMass;
                    const float addedSideSpeedSq = sidewaysShare * addedSpeedSq;
                    const uint16_t angle = random16();
                    const float dx = cos16(angle), dy = sin16(angle);
                    const float inverseLength = 1.0f / sqrtf(dx * dx + dy * dy);
                    const float along = (v.x * dx + v.y * dy) * inverseLength;
                    // Account for current motion so the impulse adds only
                    // its assigned kinetic energy, even to a moving ball.
                    const float impulse = addedSideSpeedSq /
                        (sqrtf(along * along + addedSideSpeedSq) + fabsf(along));
                    const float signedScale = along < 0.0f ? -impulse * inverseLength : impulse * inverseLength;
                    v.x += dx * signedScale;
                    v.y += dy * signedScale;
                    v.z = sqrtf(v.z * v.z + (1.0f - sidewaysShare) * addedSpeedSq);
                }
                kickThreshold = nextKickThreshold();
            }

            clearAll();
            for (int i = 0; i < count; ++i) {
                auto& ball = balls[i];
                const auto& p = ball.motion.position;
                const auto& v = ball.motion.velocity;
                ball.rotX += v.y * 45.0f * dt;
                ball.rotY += v.x * 45.0f * dt;
                GeometryEngine::drawSphere(p.x, p.y, p.z, ball.radius, ball.mode, ball.texture,
                                           ball.rotX, ball.rotY, 0.0f, false);
            }
            showCube();
            for (int i = 0; i < count; ++i) balls[i].texture.advance(deltaMs);
            if (speedMs > 0) delay(speedMs); else yield();
        }
    }

    // ==========================================
    // LAVA LAMP (True Metaballs + Soft Body Physics)
    // ==========================================
    struct LavaBlob {
        float x, y, z;
        float vx, vy, vz;
        float targetX, targetY;
        bool isRising;
        float baseRadius;
        float currentRadius; // Active pulsation
        float pulsePhase;    // Active pulsation
        float pulseSpeed;    // Active pulsation
        float speedFactor;
        float hue;           // Independent thermal color
    };

    static void animateLavaLamp(uint32_t durationMs, float speedMultiplier = 1.0f, float volumePercent = 0.15f, CRGBPalette16 pal = LavaColors_p) {
        uint32_t startTime = millis();
        uint32_t lastFrameTime = millis();

        float totalCubeVolume = (float)(RNDR_X * RNDR_Y * RNDR_Z);
        float targetFluidVolume = totalCubeVolume * volumePercent;

        const int NUM_BLOBS = (RNDR_X * RNDR_Y * RNDR_Z >= 4096) ? 6 : 3;
        LavaBlob blobs[NUM_BLOBS];

        float weights[NUM_BLOBS];
        float sumWeights = 0.0f;
        for (int i = 0; i < NUM_BLOBS; i++) {
            weights[i] = 50.0f + (float)random8();
            sumWeights += weights[i];
        }

        float glassPadding = max(1.0f, RNDR_X * 0.125f);

        // INITIALIZATION
        for (int i = 0; i < NUM_BLOBS; i++) {
            float blobVolume = targetFluidVolume * (weights[i] / sumWeights);
            blobs[i].baseRadius = powf(blobVolume * 0.238732f, 0.333333f) * 0.65f;
            blobs[i].currentRadius = blobs[i].baseRadius;

            blobs[i].pulsePhase = random16(0, 6283) / 1000.0f; // 0 to 2PI
            blobs[i].pulseSpeed = 1.5f + (random8() / 255.0f) * 2.0f;

            blobs[i].x = glassPadding + ((float)random8() / 255.0f) * (RNDR_X - glassPadding * 2.0f);
            blobs[i].y = glassPadding + ((float)random8() / 255.0f) * (RNDR_Y - glassPadding * 2.0f);
            blobs[i].z = ((float)random8() / 255.0f) * (RNDR_Z - 2.0f) + 1.0f;

            blobs[i].targetX = glassPadding + ((float)random8() / 255.0f) * (RNDR_X - glassPadding * 2.0f);
            blobs[i].targetY = glassPadding + ((float)random8() / 255.0f) * (RNDR_Y - glassPadding * 2.0f);

            blobs[i].vx = 0; blobs[i].vy = 0; blobs[i].vz = 0;
            blobs[i].isRising = (random8(2) == 0);
            blobs[i].speedFactor = 0.5f + ((float)random8() / 255.0f);

            blobs[i].hue = random8(); // Stagger initial thermal colors
        }

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrameTime;
            if (deltaMs == 0) { yield(); continue; }
            lastFrameTime = now;

            clearAll();

            float dt = ((float)deltaMs / 16.0f) * speedMultiplier;

            // ==========================================
            // SOFT-BODY PHYSICS LOOP
            // ==========================================
            for (int i = 0; i < NUM_BLOBS; i++) {

                // 1. Independent Breathing (Radius Pulsation)
                blobs[i].pulsePhase += blobs[i].pulseSpeed * (deltaMs / 1000.0f);
                // Pulse size between 80% and 120% of base mass
                blobs[i].currentRadius = blobs[i].baseRadius * (1.0f + 0.2f * sinf(blobs[i].pulsePhase));

                // 2. Thermal Color Drift
                blobs[i].hue += 5.0f * (deltaMs / 1000.0f) * speedMultiplier;
                if (blobs[i].hue >= 255.0f) blobs[i].hue -= 255.0f;

                // 3. X/Y Target Seeking
                float dx = blobs[i].targetX - blobs[i].x;
                float dy = blobs[i].targetY - blobs[i].y;
                float dist = sqrtf(dx*dx + dy*dy);

                if (dist < 1.0f) {
                    blobs[i].targetX = glassPadding + ((float)random8() / 255.0f) * (RNDR_X - glassPadding * 2.0f);
                    blobs[i].targetY = glassPadding + ((float)random8() / 255.0f) * (RNDR_Y - glassPadding * 2.0f);
                } else {
                    blobs[i].vx += (dx / dist) * 0.006f * dt;
                    blobs[i].vy += (dy / dist) * 0.006f * dt;
                }

                // 4. Z-Axis Buoyancy
                float buoyancyAccel = 0.025f * blobs[i].speedFactor * dt;
                if (blobs[i].isRising) {
                    blobs[i].vz += buoyancyAccel;
                    if (blobs[i].z >= RNDR_Z - 1.5f) blobs[i].isRising = false;
                } else {
                    blobs[i].vz -= buoyancyAccel;
                    if (blobs[i].z <= 1.0f) blobs[i].isRising = true;
                }

                // 5. The "Wall Squish" (Soft Deceleration)
                float squishBoundaryX = glassPadding + blobs[i].currentRadius * 0.5f;
                float squishBoundaryY = glassPadding + blobs[i].currentRadius * 0.5f;

                if (blobs[i].x < squishBoundaryX) {
                    blobs[i].vx += (squishBoundaryX - blobs[i].x) * 0.05f * dt;
                    blobs[i].vx *= 0.85f; // Viscous friction against the glass
                } else if (blobs[i].x > RNDR_X - squishBoundaryX) {
                    blobs[i].vx -= (blobs[i].x - (RNDR_X - squishBoundaryX)) * 0.05f * dt;
                    blobs[i].vx *= 0.85f;
                }

                if (blobs[i].y < squishBoundaryY) {
                    blobs[i].vy += (squishBoundaryY - blobs[i].y) * 0.05f * dt;
                    blobs[i].vy *= 0.85f;
                } else if (blobs[i].y > RNDR_Y - squishBoundaryY) {
                    blobs[i].vy -= (blobs[i].y - (RNDR_Y - squishBoundaryY)) * 0.05f * dt;
                    blobs[i].vy *= 0.85f;
                }

                // Apply general drag and commit velocity
                float drag = powf(0.88f, dt);
                blobs[i].vx *= drag;
                blobs[i].vy *= drag;
                blobs[i].vz *= drag;

                blobs[i].x += blobs[i].vx * dt;
                blobs[i].y += blobs[i].vy * dt;
                blobs[i].z += blobs[i].vz * dt;

                // Floor/Ceiling physical locks
                if (blobs[i].z < 0.0f) { blobs[i].z = 0.0f; blobs[i].vz *= 0.5f; }
                if (blobs[i].z > RNDR_Z - 1.0f) { blobs[i].z = RNDR_Z - 1.0f; blobs[i].vz *= 0.5f; }
            }

            // ==========================================
            // ENERGY-WEIGHTED RENDER LOOP (Deferred Rendering)
            // ==========================================
            float skinThreshold = (RNDR_X <= 8) ? 0.85f : 0.75f;
            float coreThreshold = skinThreshold + 0.30f;

            float r2[NUM_BLOBS];
            float stretchZ[NUM_BLOBS];
            CRGB blobColors[NUM_BLOBS];

            for (int i = 0; i < NUM_BLOBS; i++) {
                r2[i] = blobs[i].currentRadius * blobs[i].currentRadius;
                stretchZ[i] = 1.0f - constrain(fabsf(blobs[i].vz) * 1.5f, 0.0f, 0.25f);
                blobColors[i] = ColorFromPalette(pal, (uint8_t)blobs[i].hue, 255, LINEARBLEND);
            }

            for (uint8_t z = 0; z < RNDR_Z; z++) {
                // LOOP HOISTING: Calculate Z-axis math once per layer, not 256 times
                float dzSq[NUM_BLOBS];
                for (int i = 0; i < NUM_BLOBS; i++) {
                    float dz = ((float)z - blobs[i].z) * stretchZ[i];
                    dzSq[i] = dz * dz;
                }

                for (uint8_t y = 0; y < RNDR_Y; y++) {
                    // LOOP HOISTING: Calculate Y-axis math once per row, not 16 times
                    float dySq[NUM_BLOBS];
                    for (int i = 0; i < NUM_BLOBS; i++) {
                        float dy = (float)y - blobs[i].y;
                        dySq[i] = dy * dy;
                    }

                    for (uint8_t x = 0; x < RNDR_X; x++) {
                        float totalEnergy = 0.0f;
                        float energies[NUM_BLOBS];

                        for (int i = 0; i < NUM_BLOBS; i++) {
                            float dx = (float)x - blobs[i].x;
                            float d2 = (dx*dx) + dySq[i] + dzSq[i];

                            // Calculate raw energy (Fastest operation possible)
                            energies[i] = r2[i] / (d2 + 0.1f);
                            totalEnergy += energies[i];
                        }

                        // DEFERRED RENDERING:
                        // Only mix RGB floats if this voxel actually contains lava!
                        if (totalEnergy >= skinThreshold) {
                            float rSum = 0.0f, gSum = 0.0f, bSum = 0.0f;
                            for (int i = 0; i < NUM_BLOBS; i++) {
                                rSum += blobColors[i].r * energies[i];
                                gSum += blobColors[i].g * energies[i];
                                bSum += blobColors[i].b * energies[i];
                            }

                            CRGB finalColor;
                            finalColor.r = constrain((int)(rSum / totalEnergy), 0, 255);
                            finalColor.g = constrain((int)(gSum / totalEnergy), 0, 255);
                            finalColor.b = constrain((int)(bSum / totalEnergy), 0, 255);

                            if (totalEnergy >= coreThreshold) {
                                setVoxel(x, y, z, finalColor);
                            } else {
                                float depth = (totalEnergy - skinThreshold) / (coreThreshold - skinThreshold);
                                uint8_t dimFactor = 40 + (uint8_t)(depth * 160.0f);
                                finalColor.nscale8(dimFactor);
                                setVoxel(x, y, z, finalColor);
                            }
                        }
                    }
                }
            }

            showCube();
            yield();
        }
    }
};
