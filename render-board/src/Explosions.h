#pragma once
#include "TestDemo.h"
#include "CubeCore.h"
#include "GeometryEngine.h"
#include "ParticleEngine.h"
#include "TextureEngine.h"

#include <math.h>

// Particle counts and radius use the 16³ cube as their reference. The
// tail length follows display height, and drift follows horizontal size.
struct PSImpactSettings {
    uint16_t minLaunchMs = 400;
    uint16_t maxLaunchMs = 700;
    float meteorRadiusAtReferenceVolume = .25f; // Used by every spark in the meteor.
    float tailHeightFraction = 0.90f;
    // At 1.0 sparks fade from full brightness over their entire life.
    // Values below 1.0 add a bright hold. Above 1.0 they also dim at birth.
    float tailFade = 2.0f;
    float tailSidewaysDrift = 1.5f; // Random speed in either direction per axis on the 16³ cube.
    uint8_t minSplashAtReferenceVolume = 24; // Reference is a 16³ cube.
    uint8_t maxSplashAtReferenceVolume = 36;
    float splashForce = 1.0f;
    float splashSpread = 1.5f; // Horizontal speed multiplier.
    float maxSplashRiseSpeed = 31.0f; // Voxels per second on the reference cube.
    float hardness = 0.55f;
    uint8_t blurRetention = 0; // The moving sparks make the trail.
    bool cylinder = false;
    bool walls = false;
};

class Explosions {
    // Keep the established sparkle, brightness and tails for the original bursts.
    static void drawKineticParticles(ParticleEngine::Pool& pool, float dt) {
        for (int i = 0; i < pool.size(); ++i) {
            auto& particle = pool[i];
            if (!particle.active) continue;

            ParticleEngine::advance(particle, dt);
            const auto& p = particle.motion.position;
            const auto& v = particle.motion.velocity;
            int ix = (int)lroundf(p.x);
            int iy = (int)lroundf(p.y);
            int iz = (int)lroundf(p.z);

            // 4. Kill Logic (Faded out, fell through floor, or hit the glass)
            if (!particle.active || iz < 0 || ix < 0 || ix >= RNDR_X || iy < 0 || iy >= RNDR_Y) {
                particle.active = false;
                continue;
            }

           // 5. Draw the Particle
            if (iz < RNDR_Z) {
                CRGB drawColor = particle.color;

                // RESTORED: High-velocity particles burn white-hot
                float currentSpeed = abs(v.x) + abs(v.y) + abs(v.z);
                if (currentSpeed > 15.0f) {
                    drawColor |= CRGB(180, 180, 180); // Inject pure white light into the base color
                }

                drawColor.nscale8((uint8_t)(particle.life * 255.0f));

                ParticleEngine::drawParticlePoint(p, drawColor);

                if ((particle.flags & ParticleEngine::KineticTail) && currentSpeed > 5.0f) {
                    CRGB tailColor = drawColor;
                    tailColor.nscale8(80);
                    ParticleEngine::drawParticlePoint({p.x - (v.x * dt * 2.0f),
                        p.y - (v.y * dt * 2.0f), p.z - (v.z * dt * 2.0f)}, tailColor);
                }
            }
        }
    }

public:
    // WLED PS Impact, adapted to a volumetric floor splash. The sources
    // are invisible, and their newest sparks form the meteor heads.
    static void animatePSImpact(uint32_t durationMs, const PSImpactSettings& settings = {}) {
        using namespace ParticleEngine;
        constexpr uint32_t stepUs = 8333;
        constexpr float dt = stepUs * 0.000001f;
        constexpr float floorRoughness = 0.18f;
        const Side xSide = settings.walls ? Side::Bounce : (settings.cylinder ? Side::Wrap : Side::Open);
        const Side ySide = settings.walls ? Side::Bounce : Side::Open;
        const bool wrapX = xSide == Side::Wrap;
        const float horizontalScale = min(RNDR_X, RNDR_Y) / 16.0f;
        constexpr float verticalScale = RNDR_Z / 16.0f;
        constexpr float gravity = 30.0f * verticalScale;
        constexpr int volume = RNDR_X * RNDR_Y * RNDR_Z;
        constexpr float volumeScale = volume / 4096.0f;
        const float meteorRadius = settings.meteorRadiusAtReferenceVolume * cbrtf(volumeScale);
        const float tailHeight = settings.tailHeightFraction * RNDR_Z;
        const float launchHeight = RNDR_Z - 0.5f + meteorRadius;
        constexpr float trailGravity = gravity * 0.25f;
        constexpr int splashOverlap = 4; // Budget for overlapping floor bursts.
        const int maxSplash = constrain(int(lroundf(settings.maxSplashAtReferenceVolume * volumeScale)),
                                        1, maxParticles / splashOverlap);
        const int minSplash = constrain(int(lroundf(settings.minSplashAtReferenceVolume * volumeScale)),
                                        1, maxSplash);

        struct Meteor {
            Motion motion;
            CRGB color;
            bool active;
        };
        enum SparkType : uint8_t { Splash, Trail };
        Meteor meteors[2]{};
        Pool sparks(splashOverlap * maxSplash + 4 * RNDR_Z);
        uint16_t launchTicks = 0;
        uint8_t lastHue = random8();
        bool emitTrail = false;

        auto sample = [](float low, float high) {
            return low + (high - low) * (random16() / 65536.0f);
        };
        auto emit = [&](const Particle& spark) {
            int slot = sparks.firstFree();
            if (slot == sparks.size()) {
                // A crowded pool can give up its faintest trail spark.
                float leastLife = 2.0f;
                for (int i = 0; i < sparks.size(); ++i) {
                    if (sparks[i].type == Trail && sparks[i].life < leastLife) {
                        leastLife = sparks[i].life;
                        slot = i;
                    }
                }
            }
            if (slot < sparks.size()) sparks[slot] = spark;
        };
        auto splash = [&](Point3 origin, CRGB color) {
            const int count = random16(minSplash, maxSplash + 1);
            const uint16_t phase = random16();
            const float maxRise = max(0.0f, settings.maxSplashRiseSpeed);
            const float baseRise = min(21.0f, maxRise);
            for (int i = 0; i < count; ++i) {
                // Spread directions around the floor, with different speeds
                // and heights so the splash breaks up as it expands.
                const uint16_t angle = phase + (uint32_t(i) * 65536u / count);
                const bool tall = i % 6 == 0;
                const float rise = tall ? sample(0.93f * maxRise, maxRise)
                                        : sample(min(8.0f, baseRise), baseRise);
                // Steeper sparks have more time to reach the apex before
                // drifting out of an open side of the cube.
                const float spread = tall ? 0.3f : 1.0f;
                const float radial = sample(6.0f, 15.0f) * settings.splashForce
                                     * settings.splashSpread * horizontalScale * spread;
                const Point3 velocity{radial * cos16(angle) / 32768.0f,
                                      radial * sin16(angle) / 32768.0f,
                                      rise * settings.splashForce * verticalScale};
                CRGB sparkColor = color;
                sparkColor.nscale8(random8(170, 255));
                const float lifetime = tall ? sample(1.16f, 1.25f) : sample(0.72f, 1.25f);
                emit({true, Splash, {origin, velocity}, 0.997f, gravity,
                      1.0f, 1.0f / lifetime, sparkColor, false});
            }
        };

        const uint32_t started = millis();
        uint32_t lastFrame = micros();
        uint32_t accumulated = stepUs;
        clearAll();
        while (true) {
            const bool launching = TestDemo::routineTimeRemaining(millis() - started, durationMs);
            if (!launching && !meteors[0].active && !meteors[1].active && !sparks.anyAlive()) break;

            const uint32_t now = micros();
            // Limit catch-up after a stalled frame so a splash remains visible.
            accumulated += min(uint32_t(now - lastFrame), 6 * stepUs);
            lastFrame = now;
            while (accumulated >= stepUs) {
                accumulated -= stepUs;
                emitTrail = !emitTrail; // One emission opportunity per 60 Hz update.
                for (int i = 0; i < sparks.size(); ++i) {
                    auto& spark = sparks[i];
                    if (!spark.active) continue;
                    advance(spark, dt);
                    if (!spark.active) continue;
                    auto& p = spark.motion.position;
                    auto& v = spark.motion.velocity;
                    if (!resolveSide(p.x, v.x, RNDR_X, xSide, settings.hardness) ||
                        !resolveSide(p.y, v.y, RNDR_Y, ySide, settings.hardness)) {
                        spark.active = false;
                    } else if (spark.type == Splash) {
                        bounceFloor(spark.motion, settings.hardness, floorRoughness);
                    } else if (p.z < -meteorRadius - 0.5f) {
                        spark.active = false;
                    }
                }

                for (auto& meteor : meteors) {
                    if (!meteor.active) continue;
                    auto& p = meteor.motion.position;
                    const Point3 previous = p;
                    advance(meteor.motion, dt, {0.0f, 0.0f, -gravity}, 1.0f);
                    if (p.z <= 0.0f) {
                        // Burst at the floor crossing between simulation steps.
                        const float fraction = previous.z / (previous.z - p.z);
                        splash({previous.x + (p.x - previous.x) * fraction,
                                previous.y + (p.y - previous.y) * fraction, 0.0f}, meteor.color);
                        meteor.active = false;
                    } else if (emitTrail) {
                        const auto& v = meteor.motion.velocity;
                        const float spread = settings.tailSidewaysDrift;
                        const Point3 drift{0.15f * v.x + sample(-spread, spread) * horizontalScale,
                                           0.15f * v.y + sample(-spread, spread) * horizontalScale,
                                           0.15f * v.z};
                        // Set lifetime from the distance the falling source
                        // will gain on the spark. Fade still follows spark age.
                        const float relativeSpeed = drift.z - v.z;
                        const float relativeGravity = gravity - trailGravity;
                        const float lifetime = 2.0f * tailHeight /
                            (relativeSpeed + sqrtf(relativeSpeed * relativeSpeed +
                                                   2.0f * relativeGravity * tailHeight));
                        if (lifetime > 0.0f) {
                            emit({true, Trail, {p, drift}, 1.0f, trailGravity,
                                  1.0f, 1.0f / (lifetime * sample(0.90f, 1.0f)), meteor.color, false});
                        }
                    }
                }

                if (launchTicks > 0) --launchTicks;
                if (launching && launchTicks == 0) {
                    for (auto& meteor : meteors) {
                        if (meteor.active) continue;
                        const float edgeX = RNDR_X - 1.0f;
                        const float edgeY = RNDR_Y - 1.0f;
                        const float targetX = sample(0.08f, 0.92f) * edgeX;
                        const float targetY = sample(0.08f, 0.92f) * edgeY;
                        const float x = constrain(targetX + sample(-0.5f, 0.5f) * edgeX, 0.0f, edgeX);
                        const float y = constrain(targetY + sample(-0.5f, 0.5f) * edgeY, 0.0f, edgeY);
                        const float fallTime = sample(0.63f, 0.85f);
                        meteor.motion = {{x, y, launchHeight},
                                         {(targetX - x) / fallTime, (targetY - y) / fallTime,
                                          -launchHeight / fallTime + gravity * fallTime * 0.5f}};
                        lastHue += random8(96, 161);
                        meteor.color = CHSV(lastHue, 255, 255);
                        PaletteUtils::describeSolid(meteor.color);
                        PaletteUtils::logTexture("PS Impact meteor and splash", MODE_SOLID, PaletteUtils::lastPaletteDescription());
                        meteor.active = true;
                        launchTicks = random16(settings.minLaunchMs, settings.maxLaunchMs + 1) * 1000UL / stepUs;
                        break;
                    }
                }
            }

            if (settings.blurRetention) fadeAll(255 - settings.blurRetention);
            else clearAll();
            for (int i = 0; i < sparks.size(); ++i) {
                const auto& spark = sparks[i];
                if (!spark.active) continue;
                CRGB color = spark.color;
                const bool trail = spark.type == Trail;
                const float fadePart = trail ? max(0.01f, settings.tailFade) : 0.5f;
                color.nscale8(uint8_t(255.0f * min(1.0f, spark.life / fadePart)));
                if (trail) drawBall(spark.motion.position, meteorRadius, color, wrapX);
                else drawSmooth(spark.motion.position, color, wrapX);
            }
            showCube();
        }
        clearAll();
    }

   // ==========================================
    // THE FIREWORKS (Powered by Kinetic Engine)
    // ==========================================
    static void animateFireworks(uint32_t durationMs) {
        uint32_t startTime = millis(), lastFrame = millis();
        enum ParticleType : uint8_t { Spark, ClassicShell, LooseShell, LayeredShell };
        constexpr int volume = RNDR_X * RNDR_Y * RNDR_Z;
        constexpr float volumeScale = volume / 4096.0f;
        const float burstScale = (42.0f / 64.0f) * cbrtf(volumeScale);
        constexpr float launchScale = RNDR_Z / 16.0f;

        // Define the Safe Launch Zone (prevents clipping glass walls)
        float minX = RNDR_X * 0.25f, maxX = RNDR_X * 0.75f;
        float minY = RNDR_Y * 0.25f, maxY = RNDR_Y * 0.75f;

        ParticleEngine::Pool pool(min(volume / 7, 170));

        bool keepRunning = true;

        while (keepRunning) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame;
            if (deltaMs == 0) { yield(); continue; }
            lastFrame = now; float dt = deltaMs / 1000.0f;

            // Check the clock once per frame
            bool timeIsUp = !TestDemo::routineTimeRemaining(uint32_t(now - startTime), durationMs);

            fadeAll(75);

            // 1. RANDOM LAUNCHER (Only allow launches if time is NOT up)
            if (!timeIsUp && random8(100) < 2) {
                const int index = pool.firstFree();
                if (index < pool.size()) {
                    const uint8_t shell = random8(ClassicShell, LayeredShell + 1);
                    // The slower shells burst nearer the middle, leaving room for tilted layers.
                    const float launchSpeed = (shell == ClassicShell ? random16(180, 220) : random16(140, 174))
                                              * (launchScale / 10.0f);
                    pool[index] = {
                        true, shell,
                        {{random16(int(minX * 10), int(maxX * 10)) / 10.0f,
                          random16(int(minY * 10), int(maxY * 10)) / 10.0f, 0.0f},
                         {0.0f, 0.0f, launchSpeed}},
                        1.0f, 15.0f * launchScale,
                        1.0f, 0.0f, // Shells keep their brightness until the apex.
                        CRGB::White, true
                    };
                }
            }

            // 2. DETONATION CHECK
            for (int i = 0; i < pool.size(); i++) {
                if (pool[i].active && pool[i].type != Spark && pool[i].motion.velocity.z <= 0.0f) {
                    const uint8_t shell = pool[i].type;
                    const auto origin = pool[i].motion.position;
                    pool[i].active = false; // Kill the shell

                    if (shell == LayeredShell) {
                        const int rings = random8(2, 4);
                        int freeSlots = 0;
                        for (int j = 0; j < pool.size(); ++j) freeSlots += !pool[j].active;
                        const int perRing = min(max(4, int(lroundf(26 * volumeScale))), freeSlots / rings);
                        const float yaw = random16() * (TWO_PI / 65536.0f);
                        const float tilt = random16(350, 1200) / 1000.0f;
                        const float cy = cosf(yaw), sy = sinf(yaw), ct = cosf(tilt), st = sinf(tilt);
                        const float phase = random16() * (TWO_PI / 65536.0f);
                        const int lobes = random8(2, 7);
                        const uint8_t hue = random8(), hueStep = random8(24, 128);
                        PaletteUtils::logColor("Fireworks layered", "Rainbow / tilted rings");
                        int slot = 0;
                        for (int ring = 0; ring < rings; ++ring) {
                            const auto color = ColorFromPalette(RainbowColors_p, uint8_t(hue + ring * hueStep));
                            for (int spark = 0; spark < perRing; ++spark) {
                                const float angle = phase + spark * (TWO_PI / perRing);
                                const float speed = (5.4f + 3.0f * ring) * burstScale *
                                                    (1.0f + 0.16f * sinf(angle * lobes));
                                const float u = cosf(angle) * speed, v = sinf(angle) * speed;
                                slot = pool.firstFree(slot);
                                pool[slot++] = {true, Spark, {origin, {cy * u - sy * ct * v, sy * u + cy * ct * v, st * v}},
                                                1.0f, 4.0f * launchScale, 1.0f, 0.68f, color, 0};
                            }
                        }
                        continue;
                    }

                    const bool classic = shell == ClassicShell;
                    const CRGBPalette16 explosionPal = classic ? PaletteUtils::getRandomOrganicPalette() : CRGBPalette16(RainbowColors_p);
                    const PaletteDescription explosionColor = PaletteUtils::lastPaletteDescription();
                    const bool isConfetti = classic && random8(2) == 0;
                    const CRGB solidColor = classic ? PaletteUtils::getRandomVibrantColor() : CRGB::Black;
                    if (classic) {
                        PaletteUtils::logTexture("Fireworks classic", isConfetti ? MODE_LINEAR_FLOW : MODE_SOLID,
                                                 isConfetti ? explosionColor : PaletteUtils::lastPaletteDescription());
                    } else PaletteUtils::logColor("Fireworks loose", "Rainbow");
                    const uint8_t hue = classic ? 0 : random8();
                    int sparksToSpawn = classic ? volume / 29 : max(1, int(lroundf(random8(17, 29) * volumeScale)));
                    for (int j = pool.firstFree(); j < pool.size() && sparksToSpawn > 0;
                         j = pool.firstFree(j + 1)) {
                        float theta = (random16(0, 6283) / 1000.0f);
                        float v = (random16(0, 2000) / 1000.0f) - 1.0f;
                        float phi = acos(v);

                        const float burstSpeed = classic ? random16(300, 500) / 10.0f
                                                         : random16(360, 1000) * (burstScale / 100.0f);
                        const CRGB sparkColor = classic
                            ? (isConfetti ? ColorFromPalette(explosionPal, random8(), 255, NOBLEND) : solidColor)
                            : ColorFromPalette(explosionPal, uint8_t(hue + int(random8(65)) - 32));

                        pool[j] = {
                            true, Spark,
                            {origin, {burstSpeed * sin(phi) * cos(theta),
                                      burstSpeed * sin(phi) * sin(theta), burstSpeed * cos(phi)}},
                            classic ? 0.75f : 1.0f, (classic ? 12.0f : 4.0f) * launchScale,
                            1.0f, classic ? random16(8, 15) / 10.0f : random16(55, 90) / 100.0f,
                            sparkColor, false // No tails to keep the sphere crisp
                        };
                        sparksToSpawn--;
                    }
                }
            }

            drawKineticParticles(pool, dt);
            if (timeIsUp && !pool.anyAlive()) keepRunning = false;

            showCube(); yield();
        }
    }



    // ==========================================
    // THE ATOM SMASHER (Powered by Kinetic Engine)
    // ==========================================
    static void animateAtomSmasher(uint32_t durationMs = 15000, int targetCycles = 0) {
        uint32_t startTime = millis(), lastFrame = millis();
        float angleA = 0.0f, angleB = PI, zA = RNDR_Z - 1.0f, zB = 0.0f;

        // FIX 1: Tucked the orbit radius in slightly to prevent Index 16 Out-Of-Bounds
        float orbitRadius = min(RNDR_CX, RNDR_CY) - 1.5f;

        // ELEVATED DETONATION: We shift the convergence point to the upper 3rd of the cube (65% height).
        // This gives the explosion plenty of room to occupy the top before the 25.0f gravity pulls it down.
        float targetZ = RNDR_Z * 0.60f;

        float speed = TWO_PI * 0.5f;

        // Asymmetric vertical speeds guarantee both points arrive at the elevated target simultaneously
        float speedZA = ((RNDR_Z - 1.0f) - targetZ) / 3.5f;
        float speedZB = targetZ / 3.5f;

        enum State { SPIRAL_IN, IMPLOSION_PAUSE, KINETIC_BLAST };
        State state = SPIRAL_IN;
        uint32_t stateTimer = millis();

        ParticleEngine::Pool embers((RNDR_X * RNDR_Y * RNDR_Z) / 12);

        float masterPhase = 0.0f;
        int currentCycle = 0;
        bool keepRunning = true;

        CRGB colA = CHSV(random8(), 255, 255), colB = CHSV(random8(), 255, 255);
        PaletteUtils::describeSolid(colA);
        PaletteUtils::logTexture("Atom Smasher incoming", MODE_SOLID, PaletteUtils::lastPaletteDescription(), 1);
        PaletteUtils::describeSolid(colB);
        PaletteUtils::logTexture("Atom Smasher incoming", MODE_SOLID, PaletteUtils::lastPaletteDescription(), 2);

        while (keepRunning) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame;
            if (deltaMs == 0) { yield(); continue; }
            lastFrame = now; float dt = deltaMs / 1000.0f; masterPhase += 0.33f * deltaMs;
            clearAll();

            if (state == SPIRAL_IN) {
                speed += (TWO_PI * 1.5f) * dt;
                angleA += speed * dt; angleB -= speed * dt;

                zA -= speedZA * dt;
                zB += speedZB * dt;

                int ax = (int)lroundf(RNDR_CX + cosf(angleA) * orbitRadius), ay = (int)lroundf(RNDR_CY + sinf(angleA) * orbitRadius), az = (int)lroundf(zA);
                int bx = (int)lroundf(RNDR_CX + cosf(angleB) * orbitRadius), by = (int)lroundf(RNDR_CY + sinf(angleB) * orbitRadius), bz = (int)lroundf(zB);
                setVoxel(ax, ay, az, colA); setVoxel(bx, by, bz, colB);

                if (zA <= targetZ && zB >= targetZ) { state = IMPLOSION_PAUSE; stateTimer = now; }
            }
            else if (state == IMPLOSION_PAUSE) {
                if (now - stateTimer > 120) {
                    state = KINETIC_BLAST; stateTimer = now;
                    for(int i = 0; i < embers.size(); i++) {
                        float theta = (random16(0, 6283) / 1000.0f);
                        float v = (random16(0, 2000) / 1000.0f) - 1.0f;
                        float phi = acos(v);

                        // Lower initial speed, higher drag. It flashes white, then blooms rainbow.
                        float burstSpeed = random16(250, 450) / 10.0f;

                        CRGB sparkColor = ColorFromPalette(PartyColors_p, random8(), 255, NOBLEND);

                        embers[i] = {
                            true, 0,
                            {{RNDR_CX, RNDR_CY, targetZ},
                             {burstSpeed * sin(phi) * cos(theta),
                              burstSpeed * sin(phi) * sin(theta), burstSpeed * cos(phi)}},
                            0.82f, 25.0f, // Restored: Hard atmospheric braking and heavy gravity
                            1.0f, 0.2f,   // Restored: Authentic slow decay
                            sparkColor,
                            true
                        };
                    }
                }
            }
            else if (state == KINETIC_BLAST) {
                drawKineticParticles(embers, dt);
                if (!embers.anyAlive() || now - stateTimer > 4000) {
                    currentCycle++;
                    if ((!TestDemo::playbackHeld() && targetCycles > 0 && currentCycle >= targetCycles) || (targetCycles <= 0 && (!TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)))) keepRunning = false;
                    if (keepRunning) {
                        state = SPIRAL_IN; stateTimer = now;
                        angleA = 0.0f; angleB = PI; zA = RNDR_Z - 1.0f; zB = 0.0f; speed = TWO_PI * 0.5f;
                        colA = CHSV(random8(), 255, 255); colB = CHSV(random8(), 255, 255);
                        PaletteUtils::describeSolid(colA);
                        PaletteUtils::logTexture("Atom Smasher incoming", MODE_SOLID, PaletteUtils::lastPaletteDescription(), 1);
                        PaletteUtils::describeSolid(colB);
                        PaletteUtils::logTexture("Atom Smasher incoming", MODE_SOLID, PaletteUtils::lastPaletteDescription(), 2);
                    }
                }
            }
            showCube();

            // FIX 2: Explicit frame limiter to prevent ESP32 DMA starvation
            // Replaces the naked 'yield()' call
            delay(15);
        }
    }
    // A collision sheet breaks into one spark per 2x2 patch, using the
    // same drag, gravity, fade and tails as the Atom Smasher burst.
    static void animateSmashingPlanesBurst(uint8_t axis, uint32_t stepMs) {
        const int dims[] = {RNDR_X, RNDR_Y, RNDR_Z};
        const uint8_t uAxis = (axis + 1) % 3;
        const uint8_t vAxis = (axis + 2) % 3;
        ParticleEngine::Pool sparks(((dims[uAxis] + 1) / 2) * ((dims[vAxis] + 1) / 2));
        int count = 0;
        for (int v = 0; v < dims[vAxis] && count < sparks.size(); v += 2) {
            for (int u = 0; u < dims[uAxis] && count < sparks.size(); u += 2) {
                float position[3], velocity[3];
                position[axis] = (dims[axis] - 1) * 0.5f;
                position[uAxis] = u + random8(min(2, dims[uAxis] - u));
                position[vAxis] = v + random8(min(2, dims[vAxis] - v));
                const float sign = ((u / 2 + v / 2) % 2) ? 1.0f : -1.0f;
                velocity[axis] = sign * random8(18, 30) * dims[axis] / 16.0f;
                velocity[uAxis] = (int(random8(161)) - 80) * 0.1f;
                velocity[vAxis] = (int(random8(161)) - 80) * 0.1f;
                sparks[count++] = {
                    true, 0,
                    {{position[0], position[1], position[2]}, {velocity[0], velocity[1], velocity[2]}},
                    0.94f, 8.0f,
                    1.0f, random8(85, 141) * 0.01f,
                    ColorFromPalette(PartyColors_p, random8(), 255, NOBLEND), true
                };
            }
        }

        uint32_t lastFrame = millis();
        delay(stepMs);
        while (true) {
            const uint32_t now = millis();
            const float dt = (now - lastFrame) * 0.001f;
            lastFrame = now;
            clearAll();
            drawKineticParticles(sparks, dt);
            showCube();
            delay(stepMs);
            if (!sparks.anyAlive()) break;
        }
    }
};
