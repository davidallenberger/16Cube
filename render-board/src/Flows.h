#pragma once
#include "ParticleEffectSupport.h"
#include "TextureEngine.h"

class Flows {
    // Render a face-local point once per side, rotating around the Z axis.
    template <typename Draw>
    static void drawFourFaceCopies(float along, float depth, float z, const Draw& draw) {
        draw({RNDR_CX + along, depth, z});
        draw({RNDR_X - 1 - depth, RNDR_CY + along, z});
        draw({RNDR_CX - along, RNDR_Y - 1 - depth, z});
        draw({depth, RNDR_CY - along, z});
    }

    static void drawFallingDroplet(ParticleEngine::Point3 p, CRGB color, bool wrapX) {
        int x = lroundf(p.x);
        if (wrapX && x == RNDR_X) x = 0;
        GeometryEngine::drawStrandVoxel(x, lroundf(p.y), p.z, color);
    }

public:
    struct FireSettings {
        uint8_t speed = 110;
        uint8_t intensity = 128;
        uint8_t flameHeight = 110;
        uint8_t wind = 50;
        uint8_t spread = 31; // 0–31, across the floor.
        float visibleHeightScale = 1.00f; // Scale the flame above the floor.
        uint8_t emissionPercent = 100; // Percent of the reference emission rate.
        float hotParticleRadius = 1.55f; // Hot particles overlap, then narrow as they cool.
        bool logTiming = true;
        uint8_t turbulence = 120; // Local rolling motion through the body of the flame.
    };

    struct PSVolcanoSettings {
        uint8_t speed = 100;
        uint8_t intensity = 190;
        uint8_t move = 0;
        uint8_t bounce = 160;
        uint8_t spread = 6; // 0–31.
        float particleRadius = 0.75f; // Radius on the 16³ cube.
        float radiusWobble = 0.04f;
        float emitterWobble = 0.325f; // Nozzle travel around its resting position.
        float plumeWidthScale = 1.0f; // Scale the rendered plume around its nozzle in X and Y.
        bool ageColor = true;
        bool walls = false;
        bool collide = false;
    };

    struct PSWaterfallSettings {
        uint8_t speed = 15;
        uint8_t intensity = 200;
        uint8_t variation = 40;
        uint8_t collide = 160; // 0 disables collisions. Higher values harden contacts.
        uint8_t position = 16; // Left to right, 0–31.
        float spanFraction = 1.0f / 3.0f; // Fraction of the ceiling diagonal.
        float bulgeVoxels = 1.5f; // Peak outward offset per row on the 16³ cube.
        float faceRecessFraction = 0.20f; // Three voxels behind a face on 16³.
        float faceWidthScale = 0.85f;
        float faceDepthScale = 0.65f;
        bool cylinder = false;
        bool walls = false;
        bool ground = true;
    };

    static void animateFire(uint32_t durationMs) {
        animateFire(durationMs, FireSettings{});
    }

    static void animateFire(uint32_t durationMs, const FireSettings& settings) {
        using namespace ParticleEffectSupport;
        const CRGBPalette16 palette = PaletteUtils::firePalette();
        // WLED's units are 1/64 voxel and velocity is per simulation update.
        // Compact records leave room for independent trajectories in the shared arena.
        constexpr int subpixels = 64;
        struct Ember {
            int16_t x, y, z;
            int8_t vx, vy, vz;
            uint8_t heat;
            bool active;
        };
        using FirePool = ParticlePool<Ember>;
        static_assert(sizeof(Ember) <= 12, "PS Fire needs a compact particle record");
        FirePool pool(population(FirePool::capacity, FirePool::capacity));

        struct Emitter { int16_t x, y; uint32_t nextFrame; };
        constexpr int emittersX = (RNDR_X + 1) / 2;
        constexpr int emittersY = (RNDR_Y + 1) / 2;
        constexpr int emitterCount = emittersX * emittersY;
        Emitter emitters[emitterCount];
        const float spreadFraction = (min(31, int(settings.spread)) + 1) / 32.0f;
        for (int y = 0; y < emittersY; ++y) {
            for (int x = 0; x < emittersX; ++x) {
                const float u = (x + 0.5f) / emittersX - 0.5f;
                const float v = (y + 0.5f) / emittersY - 0.5f;
                emitters[x + y * emittersX] = {
                    int16_t(lroundf((RNDR_CX + u * RNDR_X * spreadFraction) * subpixels)),
                    int16_t(lroundf((RNDR_CY + v * RNDR_Y * spreadFraction) * subpixels)), random8(5)};
            }
        }

        const int fireSpeed = max(100, int(settings.speed));
        const int height = settings.flameHeight;
        const int heatRange = (height * height) >> 8;
        const int rise = (RNDR_Z >> 1) + (fireSpeed >> 4);
        const int launchZ = lroundf(-4.0f * verticalScale * subpixels);
        const int jitterX = int(0.5f * RNDR_X * spreadFraction * subpixels / emittersX);
        const int jitterY = int(0.5f * RNDR_Y * spreadFraction * subpixels / emittersY);
        const uint32_t stepUs = settings.speed < 100 ?
            1000u * max(24L, map(settings.speed, 0, 99, 50, 10)) : 1000000u / 42u;
        constexpr uint32_t maxCatchUpSteps = 3;
        constexpr int warmPaletteStart = 180;
        constexpr int hottestPaletteIndex = 210;
        constexpr float coolRadius = 0.65f;
        // Cache the brush for each possible lifetime. Speed is at least 100,
        // so the flame's starting heat divides by at least four.
        constexpr int maxFlameHeat = 20 + ((255 * 255) >> 8) / 4 + 16 + (RNDR_Z >> 1);
        constexpr int maxSparkHeat = 30 + RNDR_Z;
        constexpr int maxHeat = maxFlameHeat > maxSparkHeat ? maxFlameHeat : maxSparkHeat;
        struct Brush { float radius; CRGB color; };
        Brush brushes[maxHeat + 1];
        for (int lifetime = 0; lifetime <= maxHeat; ++lifetime) {
            const int heat = min(255, lifetime * (3 + (settings.intensity >> 5)) + 5);
            const float warmth = heat / 255.0f;
            const float radius = coolRadius + (settings.hotParticleRadius - coolRadius) * warmth * warmth;
            const int paletteIndex = heat <= warmPaletteStart ? heat :
                warmPaletteStart + (heat - warmPaletteStart) *
                (hottestPaletteIndex - warmPaletteStart) / (255 - warmPaletteStart);
            const float widthRatio = (radius + 0.5f) / (coolRadius + 0.5f);
            const uint8_t brightness = uint8_t(heat / max(1.0f, widthRatio * widthRatio * widthRatio));
            CRGB color = ColorFromPalette(palette, paletteIndex, brightness, LINEARBLEND_NOWRAP);
            color.b = 0;
            brushes[lifetime] = {radius, color};
        }
        uint16_t windPhase = random16();
        const uint16_t gustSeed = random16();
        uint16_t fuelPhase = random16();
        int windRemainderX = 0, windRemainderY = 0;
        uint32_t frame = 0;

        struct Timing {
            uint32_t steps, renders, skipped, catchUp;
            uint32_t simulationUs, drawUs, outputUs, worstFrameUs;
            uint32_t emitted, rejected, active, peakActive;
            uint32_t riseUnits, riseSamples;
            int peakWindX, peakWindY, peakTurbulence;
        } timing{};
        const uint32_t start = millis();
        uint32_t lastStepUs = micros() - stepUs;
        uint32_t lastLogUs = lastStepUs + stepUs;

        constexpr int flowSpacing = 3;
        constexpr int flowStep = flowSpacing * subpixels;
        constexpr int flowWidth = (RNDR_X + flowSpacing - 1) / flowSpacing + 1;
        constexpr int flowDepth = (RNDR_Y + flowSpacing - 1) / flowSpacing + 1;
        constexpr int turbulenceTop = RNDR_Z * subpixels * 3 / 4;
        constexpr int flowHeight = (turbulenceTop + flowStep - 1) / flowStep + 1;
        struct Current { int16_t x, y; };
        Current currents[flowHeight][flowDepth][flowWidth];

        const auto updateCurrents = [&] {
            // Reuse noise samples for the neighboring gradients. A quarter
            // turn of each gradient gives a sideways current around it.
            uint8_t noise[flowDepth + 2][flowWidth + 2];
            for (int z = 0; z < flowHeight; ++z) {
                const uint16_t phase = uint16_t(frame * 14u + z * flowStep);
                for (int y = 0; y < flowDepth + 2; ++y) {
                    for (int x = 0; x < flowWidth + 2; ++x) {
                        noise[y][x] = inoise8(uint16_t(gustSeed + (x - 1) * flowStep),
                                              uint16_t((y - 1) * flowStep), phase);
                    }
                }
                for (int y = 0; y < flowDepth; ++y) {
                    for (int x = 0; x < flowWidth; ++x) {
                        currents[z][y][x] = {int16_t(noise[y + 2][x + 1] - noise[y][x + 1]),
                                             int16_t(noise[y + 1][x] - noise[y + 1][x + 2])};
                    }
                }
            }
        };
        const auto currentAt = [&](const Ember& p) {
            const int x = constrain(int(p.x), 0, (flowWidth - 1) * flowStep);
            const int y = constrain(int(p.y), 0, (flowDepth - 1) * flowStep);
            const int z = max(0, int(p.z));
            const int ix = min(x / flowStep, flowWidth - 2);
            const int iy = min(y / flowStep, flowDepth - 2);
            const int iz = min(z / flowStep, flowHeight - 2);
            const int fx = x - ix * flowStep, fy = y - iy * flowStep, fz = z - iz * flowStep;
            const auto lerp = [flowStep](Current a, Current b, int fraction) -> Current {
                return {int16_t(a.x + (b.x - a.x) * fraction / flowStep),
                        int16_t(a.y + (b.y - a.y) * fraction / flowStep)};
            };
            const auto inLayer = [&](int layer) {
                const auto& plane = currents[layer];
                return lerp(lerp(plane[iy][ix], plane[iy][ix + 1], fx),
                            lerp(plane[iy + 1][ix], plane[iy + 1][ix + 1], fx), fy);
            };
            return lerp(inLayer(iz), inLayer(iz + 1), fz);
        };

        const auto stepFire = [&] {
            const bool windUpdate = frame % 3 == 0;
            int windX = 0, windY = 0;
            if (windUpdate) {
                ++windPhase;
                // As in WLED, keep fractional changes to velocity across gust updates.
                windRemainderX += (int(inoise8(windPhase, gustSeed)) - 127) * settings.wind / 128;
                windRemainderY += (int(inoise8(windPhase, gustSeed + 0x6100)) - 127) * settings.wind / 128;
                windX = windRemainderX / 16;
                windY = windRemainderY / 16;
                windRemainderX %= 16;
                windRemainderY %= 16;
                timing.peakWindX = max(timing.peakWindX, abs(windX));
                timing.peakWindY = max(timing.peakWindY, abs(windY));
                if (settings.turbulence) updateCurrents();
            }

            timing.active = 0;
            for (int i = 0; i < pool.size(); ++i) {
                auto& p = pool[i];
                if (!p.active) continue;
                if (--p.heat == 0) { p.active = false; continue; }
                if (windUpdate) {
                    int localX = 0, localY = 0;
                    if (settings.turbulence && p.z > -subpixels && p.z < turbulenceTop) {
                        const Current current = currentAt(p);
                        const int envelope = min(256, min((p.z + subpixels) * 2, turbulenceTop - p.z));
                        const int strength = settings.turbulence * envelope;
                        localX = constrain(current.x * strength / (1536 * 256), -6, 6);
                        localY = constrain(current.y * strength / (1536 * 256), -6, 6);
                        timing.peakTurbulence = max(timing.peakTurbulence, max(abs(localX), abs(localY)));
                    }
                    p.vx = constrain(p.vx + windX + localX, -127, 127);
                    p.vy = constrain(p.vy + windY + localY, -127, 127);
                }
                p.x += p.vx;
                p.y += p.vy;
                const int upward = p.vz + (p.heat >> 2);
                p.z += upward;
                constexpr int edge = 2 * subpixels;
                p.active = p.x > -edge && p.x < RNDR_X * subpixels + edge &&
                           p.y > -edge && p.y < RNDR_Y * subpixels + edge &&
                           p.z < RNDR_Z * subpixels + edge;
                timing.active += p.active;
                if (p.active && p.z >= 0 && p.z < RNDR_Z * subpixels) {
                    timing.riseUnits += upward;
                    ++timing.riseSamples;
                }
            }

            fuelPhase += 3;
            int nextSlot = 0;
            if (settings.emissionPercent > 0) {
                // Rotate priority so pool pressure doesn't starve one side.
                for (int i = 0; i < emitterCount; ++i) {
                    auto& emitter = emitters[(frame + i) % emitterCount];
                    if (int32_t(frame - emitter.nextFrame) < 0) continue;
                    // Nearby sources respond to the same fuel field, but
                    // have independent launch times and never switch off as a block.
                    const int fuel = inoise8(uint16_t(emitter.x * 2), uint16_t(emitter.y * 2), fuelPhase);
                    const int gap = 1 + random8(3) + (255 - fuel) / 80 + max(0, 96 - fuel) / 8;
                    emitter.nextFrame = frame + max(1, gap * 100 / int(settings.emissionPercent));
                    nextSlot = pool.firstFree(nextSlot);
                    if (nextSlot == pool.size()) { ++timing.rejected; continue; }

                    const int maxLife = 16 + random8(max(1, RNDR_Z >> 1));
                    const int spread = 2 + random8(2 + (fireSpeed >> 4));
                    Ember p{int16_t(emitter.x + int(random16(2 * jitterX + 1)) - jitterX),
                            int16_t(emitter.y + int(random16(2 * jitterY + 1)) - jitterY),
                            int16_t(launchZ),
                            int8_t(int(random8(5)) - 2 + int(random8(2 * spread + 1)) - spread),
                            int8_t(int(random8(5)) - 2 + int(random8(2 * spread + 1)) - spread),
                            int8_t(rise + (height >> 4) + int(random8(2 * spread + 1)) - spread),
                            uint8_t(20 + random16(max(1, heatRange)) / (1 + (fireSpeed >> 5)) +
                                    random8(maxLife >> 1, maxLife)),
                            true};
                    if (random16(4096) < 10 + (settings.intensity >> 2)) {
                        p.vz = rise + ((30 + (settings.intensity >> 1) + height) >> 4);
                        p.heat = 30 + random8(RNDR_Z);
                    }
                    // Age through the invisible part of the rise at launch,
                    // leaving the pool for particles that can contribute light.
                    while (p.z < -subpixels * 2 && p.heat > 1) {
                        --p.heat;
                        p.x += p.vx;
                        p.y += p.vy;
                        p.z += p.vz + (p.heat >> 2);
                    }
                    pool[nextSlot++] = p;
                    ++timing.emitted;
                    ++timing.active;
                }
            }
            timing.peakActive = max(timing.peakActive, timing.active);
            ++frame;
        };

        clearAll();
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - start), durationMs)) {
            const uint32_t frameStartUs = micros();
            const uint32_t due = (frameStartUs - lastStepUs) / stepUs;
            if (!due) { delay(1); continue; }
            lastStepUs += due * stepUs;
            const uint32_t steps = min(due, maxCatchUpSteps);
            timing.steps += steps;
            timing.skipped += due - steps;
            timing.catchUp += steps - 1;
            for (uint32_t i = 0; i < steps; ++i) stepFire();
            const uint32_t drawStartUs = micros();

            fadeAll(255 - 170);
            for (int i = 0; i < pool.size(); ++i) {
                const auto& p = pool[i];
                if (!p.active) continue;
                float z = p.z / float(subpixels);
                z += max(0.0f, z) * (settings.visibleHeightScale - 1.0f);
                const auto& brush = brushes[p.heat];
                if (z <= -brush.radius - 0.5f || z >= RNDR_Z + brush.radius - 0.5f) continue;
                drawBall({p.x / float(subpixels), p.y / float(subpixels), z}, brush.radius, brush.color, false);
            }
            const uint32_t outputStartUs = micros();
            showCube();
            const uint32_t frameEndUs = micros();

            ++timing.renders;
            timing.simulationUs += drawStartUs - frameStartUs;
            timing.drawUs += outputStartUs - drawStartUs;
            timing.outputUs += frameEndUs - outputStartUs;
            timing.worstFrameUs = max(timing.worstFrameUs, frameEndUs - frameStartUs);
            const uint32_t logElapsedUs = frameEndUs - lastLogUs;
            if (settings.logTiming && logElapsedUs >= 2000000) {
                const double perSecond = 1000000.0 / logElapsedUs;
                const double msPerRender = 0.001 / timing.renders;
                Serial.printf("PS Fire target=%.1fHz sim=%.1fHz render=%.1ffps catchup=%u missed=%u "
                              "ms sim/draw/show=%.2f/%.2f/%.2f worst=%.2f "
                              "sources=%d embers=%u/%d peak=%u emit/s=%.0f rejected=%u rise=%.1fvox/s windPeak=%d/%d rollPeak=%d\n",
                              1000000.0 / stepUs, timing.steps * perSecond, timing.renders * perSecond,
                              unsigned(timing.catchUp), unsigned(timing.skipped),
                              timing.simulationUs * msPerRender, timing.drawUs * msPerRender,
                              timing.outputUs * msPerRender, timing.worstFrameUs * 0.001,
                              emitterCount, unsigned(timing.active), pool.size(), unsigned(timing.peakActive),
                              timing.emitted * perSecond, unsigned(timing.rejected),
                              timing.riseUnits * (timing.steps * perSecond) / max(1u, timing.riseSamples) / subpixels,
                              timing.peakWindX, timing.peakWindY, timing.peakTurbulence);
                lastLogUs = frameEndUs;
                timing = {};
            }
        }
    }

    static void animatePSVolcano(uint32_t durationMs) {
        animatePSVolcano(durationMs, PSVolcanoSettings{});
    }

    static void animatePSVolcano(uint32_t durationMs, const PSVolcanoSettings& settings) {
        using namespace ParticleEffectSupport;
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
                // Wobble the source itself. Spread changes individual spark
                // velocities around that moving point.
                const Point3 origin{
                    constrain(nozzle.position.x + emitterWobble * sinf(tick * 0.025f),
                              0.0f, RNDR_X - 1.0f),
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
                const CRGB color = ColorFromPalette(palette, index, brightness, blendMode);
                Point3 position = p.motion.position;
                if (settings.plumeWidthScale != 1.0f) {
                    // Apply the same horizontal scale to spark centers and their
                    // footprints. Wobble, rise, and cooling run in the original simulation.
                    position.x = nozzle.position.x + (position.x - nozzle.position.x) * settings.plumeWidthScale;
                    position.y = nozzle.position.y + (position.y - nozzle.position.y) * settings.plumeWidthScale;
                }
                drawBall(position, radius * lengthScale, color, false, false, settings.plumeWidthScale);
            }
        });
    }

    static void animatePSWaterfall(uint32_t durationMs) {
        animatePSWaterfall(durationMs, PSWaterfallSettings{});
    }

    static void animatePSWaterfall(uint32_t durationMs, const PSWaterfallSettings& settings,
                                 const CRGBPalette16& palette = OceanColors_p) {
        using namespace ParticleEffectSupport;
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
        const auto drawWater = [&](Point3 position, CRGB color, bool falling) {
            const auto draw = [&](Point3 p) {
                if (falling) drawFallingDroplet(p, color, wrapX);
                else drawSmooth(p, color, wrapX);
            };
            // Unfold the simulation's diagonal coordinates for the face copies.
            // Copy only its rendering: all four falls share the same particle pool,
            // collisions, splash physics, nozzle variation, and ceiling profile.
            const float dx = position.x - centerX, dy = position.y - RNDR_CY;
            const float along = (dx - dy) * diagonalComponent * settings.faceWidthScale;
            const float depth = (dx + dy) * diagonalComponent * settings.faceDepthScale;
            const float recess = min(RNDR_X - 1, RNDR_Y - 1) * settings.faceRecessFraction;
            drawFourFaceCopies(along, recess + depth, position.z, draw);
        };
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
                    drawWater(p.motion.position, ColorFromPalette(palette, p.type, brightness), false);
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
                        drawWater(position, ColorFromPalette(palette, p.type), true);
                    }
                }
            }
        });
    }
};
