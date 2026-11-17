#pragma once
#include "ParticleEffectSupport.h"
#include "StreamerGeometry.h"
#include "TextureEngine.h"

enum BlackHoleOrbit {
    ORBIT_SYNC = 0,    // One majestic Saturn ring (All stars share the exact same 3D plane)
    ORBIT_SPLIT = 1,   // Dual-axis gyroscope (Inner ring and Outer ring tumble independently)
    ORBIT_CHAOTIC = 2  // Quantum swarm (Every single star gets a unique, randomized trajectory)
};

class Swirls {
public:
    struct AttractorSettings {
        uint8_t mass = 100, particles = 195, size = 2, collide = 0, friction = 16;
        bool ageColor = false, move = false, swallow = false;
        float nozzleExclusionRadius = 7.0f; // Voxels from the cube center.
        bool tangentAim = false; // Keep initial shots from entering the exclusion sphere.
    };

    struct GalaxySettings {
        uint8_t speed = 80, intensity = 128, size = 1, color = 4;
        uint8_t arms = 3; // Share the existing particle budget across evenly spaced arms.
        bool starfield = false, trace = false;
        float rockSweepDegrees = 90.0f; // Total excursion about horizontal: +45 to -45 degrees.
        float rockPeriodSeconds = 24.0f; // Full out-and-back cycle with eased reversals.
        float discParticleFraction = 0.5f; // Leave dark gaps between the visible spiral arms.
    };

    static void animatePSGalaxy(uint32_t durationMs) {
        animatePSGalaxy(durationMs, GalaxySettings{});
    }

    static void animatePSGalaxy(uint32_t durationMs, const GalaxySettings& settings,
                              const CRGBPalette16* selectedPalette = nullptr) {
        using namespace ParticleEffectSupport;
        const CRGBPalette16 palette = selectedPalette ? *selectedPalette : CRGBPalette16(RainbowColors_p);
        const Point3 center{RNDR_CX, RNDR_CY, RNDR_CZ};
        const int colorControl = min(31, int(settings.color));
        const int huePeriod = (33 - colorControl) >> 1;
        const uint8_t size = settings.trace ? uint8_t(settings.size != 0) : settings.size;
        Pool pool(population(settings.starfield ? maxParticles :
                             max(1, int(maxParticles * constrain(settings.discParticleFraction, 0.0f, 1.0f)))));
        if (!settings.starfield) {
            // Persistent normalized spiral seeds. Their coordinates describe the disc,
            // not world positions. No inward force, aging, or center-born replacements:
            // those progressively collapsed the previously full-sized galaxy.
            const int arms = constrain(int(settings.arms), 1, pool.size());
            for (int i = 0; i < pool.size(); ++i) {
                const int arm = i % arms;
                const int armParticles = (pool.size() - 1 - arm) / arms + 1;
                const float fraction = (i / arms + 0.5f) / armParticles;
                const float r = 0.15f + 0.85f * fraction;
                const float angle = arm * (TWO_PI / arms) + 4.5f * fraction + sample(-0.10f, 0.10f);
                pool[i] = {true, uint8_t(i * 256 / pool.size()),
                           {{r * cosf(angle), r * sinf(angle), sample(-0.02f, 0.02f)}, {0, 0, 0}},
                           1.0f, 0.0f, 1.0f, 0.0f, CRGB::Black, 0};
            }
        }
        uint8_t hue = random8();
        uint32_t frame = 0;
        float elapsedSeconds = 0.0f;
        run(durationMs, settings.trace ? 250 : 0, [&](uint32_t tick) {
            elapsedSeconds += dt;
            if (wledFrameDue(tick)) {
                if (frame % huePeriod == 0) hue += 2;
                ++frame;
                if (settings.starfield && random8() < (10 + (settings.intensity >> 1)) * volumeScale) {
                    const Point3 v{sample(-7, 7) * wledVelocity, sample(-7, 7) * wledVelocity,
                                   sample(-7, 7) * wledVelocity};
                    pool.spawn({true, hue, {center, v}, 1.0f, 0.0f, 1.0f,
                                42.0f / sample(800, 4000), CRGB::Black, 0});
                }
            }
            if (!settings.starfield) return;
            for (int i = 0; i < pool.size(); ++i) {
                auto& p = pool[i];
                if (!p.active) continue;
                advance(p, dt);
                auto& pos = p.motion.position;
                const float x = pos.x - center.x, y = pos.y - center.y, z = pos.z - center.z;
                const float distance = sqrtf(x*x + y*y + z*z);
                const float expand = (1 + (1 + (settings.speed >> 1)) * distance * 64.0f) * (42.0f / 400000.0f);
                pos.x += dt * (expand * x + wledVelocity * y);
                pos.y += dt * (expand * y - wledVelocity * x);
                pos.z += dt * expand * z;
                p.active = p.active && pos.x > -1 && pos.x < RNDR_X && pos.y > -1 && pos.y < RNDR_Y &&
                           pos.z > -1 && pos.z < RNDR_Z;
            }
        }, [&] {
            // Coherent rotation with bounded shear keeps the arms readable. Unbounded
            // differential rotation would eventually wind them into a filled ellipse.
            const float orbit = elapsedSeconds * (0.12f + settings.speed * 0.0025f);
            const auto discPoint = [&](int i) -> Point3 {
                const auto& seed = pool[i].motion.position;
                const float r = sqrtf(seed.x * seed.x + seed.y * seed.y);
                const float angle = atan2f(seed.y, seed.x) - orbit +
                                    0.18f * sinf(elapsedSeconds * 0.35f + r * 3.0f);
                return {r * cosf(angle), r * sinf(angle), seed.z};
            };
            const float tilt = settings.rockSweepDegrees * (TWO_PI / 720.0f) *
                               cosf(TWO_PI * elapsedSeconds / max(1.0f, settings.rockPeriodSeconds));
            const float tiltCos = cosf(tilt), tiltSin = sinf(tilt);
            float extentX = 0.001f, extentY = 0.001f;
            if (!settings.starfield) {
                for (int i = 0; i < pool.size(); ++i) {
                    const Point3 p = discPoint(i);
                    extentX = max(extentX, fabsf(p.x));
                    extentY = max(extentY, fabsf(p.y));
                }
            }
            // Fit the actual arms to the cube, extending the tilted disc into an
            // ellipse as space permits. Reserve only its small thickness, not whole
            // dark planes. At horizontal, a thin disc cannot reach top and bottom.
            const float thickness = 0.02f * min(RNDR_CX, min(RNDR_CY, RNDR_CZ));
            const float across = min((RNDR_CY - thickness * fabsf(tiltSin)) / max(0.0001f, fabsf(tiltCos)),
                                     (RNDR_CZ - thickness * fabsf(tiltCos)) / max(0.0001f, fabsf(tiltSin)));
            for (int i = 0; i < pool.size(); ++i) {
                const auto& p = pool[i];
                if (!p.active) continue;
                Point3 display = p.motion.position;
                uint8_t index = p.type;
                if (!settings.starfield) {
                    const Point3 local = discPoint(i);
                    const float y = local.y * across / extentY;
                    const float normal = local.z * thickness / 0.02f;
                    display = {center.x + local.x * RNDR_CX / extentX,
                               center.y + y * tiltCos - normal * tiltSin,
                               center.z + y * tiltSin + normal * tiltCos};
                    index = uint8_t(int((atan2f(local.y, local.x) + PI) * (256.0f / TWO_PI)) + int(hue));
                } else {
                    const float x = display.x-center.x, y = display.y-center.y, z = display.z-center.z;
                    const float distance = sqrtf(x*x + y*y + z*z);
                    if (colorControl == 0) index = uint8_t(min(180.0f, 180.0f * distance / ((RNDR_X+RNDR_Y+RNDR_Z)/6.0f)));
                    if (colorControl == 31) index = uint8_t(int(p.life * 42.0f / p.decay) >> 2);
                }
                auto color = ColorFromPalette(palette, index);
                if (settings.starfield) color.nscale8(uint8_t(min(255.0f, 84.0f * p.life / p.decay)));
                if (size == 0) drawParticlePoint(display, color);
                else if (!settings.starfield || settings.trace) drawSmooth(display, color, false);
                else drawBall(display, 0.65f + size / 64.0f, color, false);
            }
        });
    }

    // ==========================================
    // BLACK HOLE (Lissajous Orbits)
    // ==========================================
    // Master Director
    static void animateBlackHole(uint32_t durationMs, uint32_t speedMs, CRGBPalette16 pal, ShaderMode sMode, BlackHoleOrbit orbitStyle = ORBIT_SYNC) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();

        // Dynamically scale the star count so it looks dense on both 8x8x16 and 16^3
        const int numOuter = (RNDR_X > 8) ? 12 : 6;
        const int numInner = (RNDR_X > 8) ? 6 : 3;

        // Construct the 3D paint brush based on user arguments
        TextureState tex(sMode, pal, 1.0f);

        float tBase = 0.0f;

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis();
            uint32_t deltaMs = now - lastFrame;
            if (deltaMs == 0) { yield(); continue; }
            lastFrame = now;

            tBase += (deltaMs / 1000.0f) * 1.5f; // Global orbit speed scalar

            // 1. VOLUMETRIC FADE (The Comet Trails)
            fadeAll(55);

            // 2. THE SINGULARITY
            // A persistent, crisp white dot at the dead center of the array
            int cx = (int)RNDR_CX;
            int cy = (int)RNDR_CY;
            int cz = (int)RNDR_CZ;
            setVoxel(cx, cy, cz, CRGB::White);

            // 3. DRAW ORBITS
            auto drawOrbit = [&](int count, float radiusMultiplier, int tier) {
                for (int i = 0; i < count; i++) {
                    uint16_t phaseX, phaseY, phaseZ;

                    if (orbitStyle == ORBIT_SYNC) {
                        // All stars share the same frequency multipliers.
                        // The (i * 8192) evenly spaces them around the single ring.
                        phaseX = (uint16_t)(tBase * 11000) + (i * 8192);
                        phaseY = (uint16_t)(tBase * 13000) + (i * 8192);
                        phaseZ = (uint16_t)(tBase * 9000)  + (i * 8192);
                    }
                    else if (orbitStyle == ORBIT_SPLIT) {
                        // Frequencies are aggressively shifted based on the 'tier' (0 or 1).
                        phaseX = (uint16_t)(tBase * (11000 + tier * 4500)) + (i * 8192);
                        phaseY = (uint16_t)(tBase * (13000 + tier * 3500)) + (i * 8192);
                        phaseZ = (uint16_t)(tBase * (9000  + tier * 5500)) + (i * 8192);
                    }
                    else { // ORBIT_CHAOTIC
                        // Generate a globally unique ID for each star to decouple every plane
                        int uniqueId = (tier * 10) + i;
                        phaseX = (uint16_t)(tBase * (11000 + uniqueId * 1500)) + (i * 8192);
                        phaseY = (uint16_t)(tBase * (13000 - uniqueId * 1100)) + (i * 8192);
                        phaseZ = (uint16_t)(tBase * (9000  + uniqueId * 1800)) + (i * 8192);
                    }

                    // Map the sine waves (-32767 to 32767) to the physical rendering limits
                    float xPos = RNDR_CX + (sin16(phaseX) / 32768.0f) * (RNDR_CX * radiusMultiplier);
                    float yPos = RNDR_CY + (sin16(phaseY) / 32768.0f) * (RNDR_CY * radiusMultiplier);
                    float zPos = RNDR_CZ + (sin16(phaseZ) / 32768.0f) * (RNDR_CZ * radiusMultiplier);

                    int px = constrain((int)roundf(xPos), 0, RNDR_X - 1);
                    int py = constrain((int)roundf(yPos), 0, RNDR_Y - 1);
                    int pz = constrain((int)roundf(zPos), 0, RNDR_Z - 1);

                    CRGB starColor = tex.getColor(px, py, pz);
                    CRGB existing = getVoxel(px, py, pz);
                    setVoxel(px, py, pz, existing + starColor);
                }
            };

            // Outer stars: Tier 0
            drawOrbit(numOuter, 1.0f, 0);

            // Inner stars: Tier 1
            drawOrbit(numInner, 0.45f, 1);

            showCube();
            tex.advance(deltaMs); // Push the internal texture clock forward
            if (speedMs > 0) delay(speedMs); else yield();
        }
    }
    // Overload: Shader Mode + Custom Orbit (Defaults to Rainbow Palette)
    static void animateBlackHole(uint32_t durationMs, uint32_t speedMs, ShaderMode sMode, BlackHoleOrbit orbitStyle = ORBIT_SYNC) {
        animateBlackHole(durationMs, speedMs, RainbowColors_p, sMode, orbitStyle);
    }

    // Overload: The Ultimate Default (Rainbow + Linear Flow + Synchronized)
    static void animateBlackHole(uint32_t durationMs, uint32_t speedMs = 15) {
        animateBlackHole(durationMs, speedMs, RainbowColors_p, MODE_LINEAR_FLOW, ORBIT_SYNC);
    }

    static void animateAttractor(uint32_t durationMs) {
        animateAttractor(durationMs, AttractorSettings{});
    }

    static void animateAttractor(uint32_t durationMs, const AttractorSettings& settings,
                                 const CRGBPalette16& palette = OceanColors_p) {
        using namespace ParticleEffectSupport;
        const Point3 center{RNDR_CX, RNDR_CY, RNDR_CZ};
        const float faceDistance = min(RNDR_CX, min(RNDR_CY, RNDR_CZ));
        const float colorScale = 36.0f / max(1.0f, 0.94f * faceDistance);
        const float exclusionRadius = constrain(settings.nozzleExclusionRadius, 0.0f, max(0.0f, faceDistance - 1.0f));
        const float exclusionRadiusSq = exclusionRadius * exclusionRadius;
        Motion nozzle{{1.0f, center.y, 1.0f}, {-7 * wledVelocity, 0, 0}};
        Motion attractor{center, {0, 0, -7 * wledVelocity}};
        Pool pool(population((25 + 165 * settings.particles / 255) * maxParticles / 256));
        const int frictionPeriod = 33 - min(31, int(settings.friction));
        const float bounce = (120 + 135 * settings.collide / 255.0f) / 255.0f;
        const uint8_t size = settings.size >> 1;
        uint32_t frame = 0;
        uint8_t hue = random8();
        float emission = 0.0f;
        const auto bounceAxis = [](float& position, float& normal, float& tangentA, float& tangentB, int extent) {
            const float incoming = normal;
            resolveSide(position, normal, extent, Side::Bounce, 1.0f, 1.0f);
            if (normal == incoming) return;

            // WLED transfers speed along the wall at a rough bounce. On a cube,
            // split that transfer between the wall's two tangent directions.
            const float total = fabsf(normal) + fabsf(tangentA) + fabsf(tangentB);
            const float transfer = fabsf(normal) * (200.0f / 255.0f);
            const float split = sample(0.0f, 1.0f);
            tangentA += sample(-transfer, transfer) * split;
            tangentB += sample(-transfer, transfer) * (1.0f - split);
            normal = copysignf(max(0.0f, total - fabsf(tangentA) - fabsf(tangentB)), normal);
        };
        const auto moveEmitter = [&](Motion& emitter) {
            advance(emitter, 1.0f / 42, {}, 1.0f);
            auto& p = emitter.position;
            auto& v = emitter.velocity;
            bounceAxis(p.z, v.z, v.x, v.y, RNDR_Z);
            bounceAxis(p.x, v.x, v.y, v.z, RNDR_X);
            bounceAxis(p.y, v.y, v.x, v.z, RNDR_Y);
        };
        run(durationMs, 0, [&](uint32_t tick) {
            const bool emitFrame = wledFrameDue(tick);
            bool applyFriction = false;
            if (emitFrame) {
                if (settings.move && frame % 3 == 0) moveEmitter(attractor);
                if (frame % 5 == 0) ++hue;
                applyFriction = frame % frictionPeriod == 0;
                // At 16³ try one launch per WLED update while a particle slot is free.
                emission += volumeScale;
                if (emission >= 1.0f) {
                    emission -= 1.0f;
                    // Rotate the aim in XZ and alternate opposing shots from the same nozzle.
                    // A smaller spread along Y gives the WLED spray some depth.
                    const uint16_t angle = uint16_t((frame + 1) * 256u + ((frame & 1) ? 0x7FFFu : 0u));
                    Point3 v{
                        (int32_t(cos16(angle)) * 12 / 32600 + int(random8(8)) - 4) * wledVelocity,
                        sample(-1.0f, 1.0f) * wledVelocity,
                        (int32_t(sin16(angle)) * 12 / 32600 + int(random8(8)) - 4) * wledVelocity
                    };
                    if (settings.tangentAim && exclusionRadius > 0.0f) {
                        // Restrict the final spread-adjusted launch ray, preserving speed.
                        // Outside the sphere, an inward shot may still clear its edge.
                        // Clamp only rays that would enter it to the nearest tangent ray.
                        const Point3 offset{nozzle.position.x - center.x, nozzle.position.y - center.y,
                                            nozzle.position.z - center.z};
                        const float distanceSq = offset.x * offset.x + offset.y * offset.y + offset.z * offset.z;
                        const float speed = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
                        if (distanceSq > 0.0f && speed > 0.0f) {
                            const float distance = sqrtf(distanceSq);
                            const Point3 normal{offset.x / distance, offset.y / distance, offset.z / distance};
                            const float radial = v.x * normal.x + v.y * normal.y + v.z * normal.z;
                            const float tangentRatio = min(1.0f, exclusionRadius / distance);
                            const float minimumRadial = -speed * sqrtf(max(0.0f, 1.0f - tangentRatio * tangentRatio));
                            if (radial < minimumRadial) {
                                Point3 tangent{v.x - radial * normal.x, v.y - radial * normal.y,
                                               v.z - radial * normal.z};
                                float length = sqrtf(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
                                if (length < 0.0001f) {
                                    // A directly inward shot has no preferred tangent direction.
                                    tangent = fabsf(normal.x) < 0.9f ? Point3{0, normal.z, -normal.y}
                                                                            : Point3{-normal.z, 0, normal.x};
                                    length = sqrtf(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
                                }
                                const float scale = speed * tangentRatio / length;
                                v = {normal.x * minimumRadial + tangent.x * scale,
                                     normal.y * minimumRadial + tangent.y * scale,
                                     normal.z * minimumRadial + tangent.z * scale};
                            }
                        }
                    }
                    pool.spawn({true, hue, {nozzle.position, v}, 1.0f, 0.0f, 1.0f,
                                42.0f / random16(50, 350), CRGB::Black, 0});
                }
                moveEmitter(nozzle);
                // The invisible sphere is a wall for the nozzle only.
                auto& p = nozzle.position;
                auto& v = nozzle.velocity;
                const Point3 offset{p.x - center.x, p.y - center.y, p.z - center.z};
                const float distanceSq = offset.x * offset.x + offset.y * offset.y + offset.z * offset.z;
                if (distanceSq < exclusionRadiusSq) {
                    const float distance = sqrtf(distanceSq);
                    const Point3 normal = distance > 0.0f
                        ? Point3{offset.x / distance, offset.y / distance, offset.z / distance}
                        : Point3{1.0f, 0.0f, 0.0f};
                    p = {center.x + exclusionRadius * normal.x, center.y + exclusionRadius * normal.y,
                         center.z + exclusionRadius * normal.z};
                    const float inward = v.x * normal.x + v.y * normal.y + v.z * normal.z;
                    if (inward < 0.0f) {
                        v.x -= 2.0f * inward * normal.x;
                        v.y -= 2.0f * inward * normal.y;
                        v.z -= 2.0f * inward * normal.z;
                    }
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
                const float force = settings.mass * wledGravity / max(2.0f, r2);
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
                const float gradient = (pos.x - center.x + pos.z - center.z) * colorScale;
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

    static void animateCyclone(uint32_t durationMs, int numStreamers, float volumePercent, CRGBPalette16 pal, ShaderMode sMode, float rps= 1.0f) {
        uint32_t startTime = millis();
        uint32_t lastFrame = millis();
        float masterPhase = 0.0f, anglePhase = 0.0f;
        float thickness = StreamerGeometry::getThicknessFromVolume(volumePercent);
        float radius = (RNDR_X / 2.0f) - thickness;

        // If there are more than 3 arms, proportionally slow down the RPMs
        // so the visual frequency (arms per second) remains identical.
        float adjustedRps = rps;
        if (numStreamers > 3) {
            adjustedRps = rps * (3.0f / (float)numStreamers);
        }

        float totalTwist = 140.0f;
        float lagPerLayer = totalTwist / (RNDR_Z - 1);
        float spinSpeed = adjustedRps * 360.0f;

        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now;
            clearAll();
            masterPhase += 0.33f * deltaMs;

            for (uint8_t z = 0; z < RNDR_Z; z++) {
                float zDistance = (RNDR_Z - 1) - z, lagAngle = zDistance * lagPerLayer;
                for (int i = 0; i < numStreamers; i++) {
                    float offsetAngle = (360.0f / numStreamers) * i;
                    uint32_t colorOffset = (512 / numStreamers) * i;

                    TextureState tex(sMode, pal, 1.0f);
                    tex.internalPhase = masterPhase + colorOffset + (z * 5);
                    tex.phase = (uint32_t)tex.internalPhase;
                    tex.initMath();

                    float rad = (anglePhase + offsetAngle - lagAngle) * 0.0174533f;
                    float cx = RNDR_CX + cos(rad) * radius, cy = RNDR_CY + sin(rad) * radius;

                    uint8_t ix0 = max(0, (int)round(cx - thickness - 1)), ix1 = min((int)RNDR_X - 1, (int)round(cx + thickness + 1));
                    uint8_t iy0 = max(0, (int)round(cy - thickness - 1)), iy1 = min((int)RNDR_Y - 1, (int)round(cy + thickness + 1));

                    for(uint8_t x = ix0; x <= ix1; x++) {
                        for(uint8_t y = iy0; y <= iy1; y++) {
                            float dx = cx - (float)x, dy = cy - (float)y, dist = sqrtf(dx*dx + dy*dy);
                            if (dist <= thickness) setVoxel(x, y, z, tex.getColor(x, y, z));
                        }
                    }
                }
            }
            showCube();
            anglePhase += spinSpeed * (deltaMs / 1000.0f);
            if (anglePhase >= 360.0f) anglePhase -= 360.0f;
            yield();
        }
    }
    static void animateCyclone(uint32_t durationMs, int numStreamers, float volumePercent, CRGB::HTMLColorCode colorCode, float rps = 1.0f) { animateCyclone(durationMs, numStreamers, volumePercent, CRGBPalette16(CRGB(colorCode)), MODE_SOLID, rps); }
    static void animateCyclone(uint32_t durationMs, int numStreamers, float volumePercent, ShaderMode sMode, float rps = 1.0f) { animateCyclone(durationMs, numStreamers, volumePercent, RainbowColors_p, sMode, rps); }

    static void animateTornado(uint32_t durationMs, uint32_t speedMs, CRGBPalette16 pal = CloudColors_p, float startingRevs = 1.0f) {
        uint32_t startTime = millis(), lastFrameTime = millis(); float globalAngle = 0.0f, colorPhase = 0.0f;
        const int MAX_DEBRIS = 64; int numDebris = min((int)(RNDR_Z * 3), MAX_DEBRIS); float dZ[MAX_DEBRIS], dAng[MAX_DEBRIS], dRadOffset[MAX_DEBRIS], dSpeed[MAX_DEBRIS];
        for(int i = 0; i < numDebris; i++) { dZ[i] = random16(0, RNDR_Z * 10) / 10.0f; dAng[i] = random16(0, TWO_PI * 100) / 100.0f; dRadOffset[i] = random16(10, 20) / 10.0f; dSpeed[i] = random16(10, 30) / 10.0f; }
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrameTime; lastFrameTime = now; clearAll();
            float progress = (float)(now - startTime) / (float)durationMs, currentRevs = startingRevs * (1.0f + (progress * 2.5f));
            globalAngle += (currentRevs * TWO_PI) * (deltaMs / 1000.0f); colorPhase += 150.0f * (deltaMs / 1000.0f);
            int numArms = 4;
            for (int z = 0; z < RNDR_Z; z++) {
                float zPercent = (float)z / (float)(RNDR_Z > 1 ? RNDR_Z - 1 : 1), maxRadius = (RNDR_X - 1) / 2.0f, idealRadius = 0.5f + pow(zPercent, 1.5f) * (maxRadius - 0.5f);
                float zTwist = z * 0.5f; uint8_t layerColorIndex = (uint8_t)colorPhase + (z * 8); CRGB layerColor = ColorFromPalette(pal, layerColorIndex, 255, LINEARBLEND);
                for (int x = 0; x < RNDR_X; x++) {
                    for (int y = 0; y < RNDR_Y; y++) {
                        float dx = x - RNDR_CX, dy = y - RNDR_CY, pixelDist = sqrt((dx * dx) + (dy * dy)), distFromWall = abs(pixelDist - idealRadius), wallThickness = 1.5f;
                        if (distFromWall < wallThickness) { float wallBrightness = 1.0f - (distFromWall / wallThickness), pixelAngle = atan2(dy, dx), spiralPhase = (numArms * (pixelAngle - globalAngle)) + zTwist, spiralMod = (sin(spiralPhase) + 1.0f) * 0.5f, intensity = wallBrightness * (0.2f + 0.8f * spiralMod); CRGB finalColor = layerColor; finalColor.nscale8((uint8_t)(intensity * 255.0f)); setVoxel(x, y, z, finalColor); }
                    }
                }
            }
            for(int i = 0; i < numDebris; i++) {
                dZ[i] += dSpeed[i] * (deltaMs / 1000.0f); if (dZ[i] >= RNDR_Z) { dZ[i] = 0; dAng[i] = random16(0, TWO_PI * 100) / 100.0f; }
                int z = (int)dZ[i]; float zPercent = (float)z / (float)(RNDR_Z > 1 ? RNDR_Z - 1 : 1), maxRadius = (RNDR_X - 1) / 2.0f, baseRadius = 0.5f + pow(zPercent, 1.5f) * (maxRadius - 0.5f), radius = baseRadius * dRadOffset[i];
                dAng[i] += (currentRevs * TWO_PI * 1.3f) * (deltaMs / 1000.0f);
                float px = RNDR_CX + (cos(dAng[i]) * radius), py = RNDR_CY + (sin(dAng[i]) * radius);
                if (px >= 0 && px < RNDR_X && py >= 0 && py < RNDR_Y) { CRGB debrisColor = ColorFromPalette(pal, (uint8_t)(colorPhase + (i * 15)), 255, LINEARBLEND); debrisColor |= CRGB(100, 100, 100); CRGB existing = getVoxel((int)px, (int)py, z); setVoxel((int)px, (int)py, z, existing + debrisColor); }
            }
            showCube(); if (speedMs > 0) delay(speedMs); else yield();
        }
    }

    static constexpr uint32_t pinwheelArmHoldMs = 7000;
    static constexpr uint32_t pinwheelArmsCycleMs = 4 * pinwheelArmHoldMs;

    static void animatePinwheelArms(uint32_t durationMs, uint8_t effectSpeed) {
        using GeometryEngine::Point3;
        constexpr float diagonal = 0.57735027f;
        constexpr float ringRadius = 0.89442719f;
        constexpr float ringHeight = 0.44721360f;
        // Tetrahedron, octahedron, cube corners, then a pentagonal
        // antiprism. Adjacent entries share a hue, pairing opposite arms
        // whenever the layout has them.
        const Point3 directions[4][10] = {
            {{diagonal, diagonal, diagonal}, {-diagonal, -diagonal, diagonal},
             {diagonal, -diagonal, -diagonal}, {-diagonal, diagonal, -diagonal}},
            {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}},
            {{diagonal, diagonal, diagonal}, {-diagonal, -diagonal, -diagonal},
             {diagonal, diagonal, -diagonal}, {-diagonal, -diagonal, diagonal},
             {diagonal, -diagonal, diagonal}, {-diagonal, diagonal, -diagonal},
             {diagonal, -diagonal, -diagonal}, {-diagonal, diagonal, diagonal}},
            {{ringRadius, 0, ringHeight}, {-ringRadius, 0, -ringHeight},
             {0.27639320f, 0.85065081f, ringHeight}, {-0.27639320f, -0.85065081f, -ringHeight},
             {-0.72360680f, 0.52573111f, ringHeight}, {0.72360680f, -0.52573111f, -ringHeight},
             {-0.72360680f, -0.52573111f, ringHeight}, {0.72360680f, 0.52573111f, -ringHeight},
             {0.27639320f, -0.85065081f, ringHeight}, {-0.27639320f, 0.85065081f, -ringHeight}}
        };
        constexpr uint32_t transitionMs = 1200;
        // For each target layout, map its arms back to nearby directions in
        // the preceding layout. -1 marks a new arm.
        constexpr int8_t previousArm[4][10] = {
            {2, 6, 5, 9},
            {0, 1, 3, 2, -1, -1},
            {0, 1, 2, 3, 4, 5, -1, -1},
            {-1, -1, 0, 1, 7, 6, 3, 2, 4, 5},
        };
        const CRGBPalette16 palette(RainbowColors_p);
        const float maxRadius = 0.5f * sqrtf(RNDR_X * RNDR_X + RNDR_Y * RNDR_Y + RNDR_Z * RNDR_Z);
        const uint32_t start = millis();
        uint32_t lastFrame = start;
        uint16_t rotationPhase = 0, spin = 0;
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - start), durationMs)) {
            const uint32_t now = millis();
            const uint32_t deltaMs = now - lastFrame;
            if (deltaMs < 15) { yield(); continue; }
            lastFrame = now;
            const uint32_t elapsed = now - start;
            const int layout = (elapsed % pinwheelArmsCycleMs) / pinwheelArmHoldMs;
            const int arms = 4 + 2 * layout;
            const uint8_t hue = uint8_t(elapsed * uint64_t(21) / 1000); // 21 hue steps per second.
            const auto armColor = [&](int arm, int count) {
                return ColorFromPalette(palette, uint8_t(hue + (arm / 2) * 256 / (count / 2)));
            };
            Point3 frameDirections[10];
            CRGB frameColors[10];
            int frameArms = arms;
            const uint32_t layoutAge = elapsed % pinwheelArmHoldMs;
            if (elapsed >= pinwheelArmHoldMs && layoutAge < transitionMs) {
                const int oldLayout = (layout + 3) % 4;
                const int oldArms = 4 + 2 * oldLayout;
                const float t = float(layoutAge) / transitionMs;
                const float mix = t * t * (3.0f - 2.0f * t);
                const uint8_t weight = uint8_t(lroundf(255.0f * mix));
                uint16_t carried = 0;
                for (int arm = 0; arm < arms; ++arm) {
                    const Point3& to = directions[layout][arm];
                    const int old = previousArm[layout][arm];
                    frameDirections[arm] = to;
                    frameColors[arm] = armColor(arm, arms);
                    if (old < 0) {
                        frameColors[arm].nscale8(weight);
                        continue;
                    }
                    carried |= 1u << old;
                    const Point3& from = directions[oldLayout][old];
                    const Point3 direction{from.x + mix * (to.x - from.x),
                                           from.y + mix * (to.y - from.y),
                                           from.z + mix * (to.z - from.z)};
                    // Keep the arm's reach while its direction changes.
                    const float inverseLength = 1.0f / sqrtf(direction.x * direction.x +
                                                              direction.y * direction.y +
                                                              direction.z * direction.z);
                    frameDirections[arm] = {direction.x * inverseLength, direction.y * inverseLength,
                                            direction.z * inverseLength};
                    frameColors[arm] = blend(armColor(old, oldArms), frameColors[arm], weight);
                }
                for (int old = 0; old < oldArms; ++old) {
                    if (carried & (1u << old)) continue;
                    frameDirections[frameArms] = directions[oldLayout][old];
                    frameColors[frameArms] = armColor(old, oldArms);
                    frameColors[frameArms++].nscale8(255 - weight);
                }
            } else {
                for (int arm = 0; arm < arms; ++arm) {
                    frameDirections[arm] = directions[layout][arm];
                    frameColors[arm] = armColor(arm, arms);
                }
            }
            const float pulse = (sin16(uint16_t(elapsed * effectSpeed / 24)) + 32768) / 65536.0f;
            const uint32_t speed = uint32_t((20 + effectSpeed / 3) * (1.0f + 0.10f * pulse) * deltaMs / 15.0f);
            rotationPhase += speed;
            spin += 20 * speed;
            constexpr float degreesPerStep = 360.0f / 65536.0f;
            const float angle = rotationPhase * degreesPerStep;
            const GeometryEngine::TumbleRotation tumble({angle, 2 * angle, 3 * angle});
            const float bend = 1000.0f + 29000.0f * pulse * pulse;

            fadeAll(155);
            for (float radius = 0; radius <= maxRadius; radius += 0.125f) {
                const uint16_t twist = spin - uint16_t(bend * radius / maxRadius);
                const float sine = sin16(twist) / 32768.0f, cosine = cos16(twist) / 32768.0f;
                const uint8_t brightness = min(255, 64 + int(radius * 30));
                for (int arm = 0; arm < frameArms; ++arm) {
                    const auto& dir = frameDirections[arm];
                    const auto p = tumble.apply({radius * (dir.x * cosine - dir.y * sine),
                                                 radius * (dir.x * sine + dir.y * cosine), radius * dir.z});
                    CRGB color = frameColors[arm];
                    color.nscale8(brightness);
                    GeometryEngine::drawWuVoxel(lroundf(256 * (RNDR_CX + p.x)),
                                                lroundf(256 * (RNDR_CY + p.y)),
                                                lroundf(256 * (RNDR_CZ + p.z)), color);
                }
            }
            showCube();
        }
    }

    static void animateAtomicOrbits(uint32_t durationMs, uint32_t stepMs = 20) {
        using GeometryEngine::Point3;
        constexpr uint32_t ringTraceMs = 340, holdMs = 600;
        constexpr unsigned rings = 4;
        const Point3 center{RNDR_CX, RNDR_CY, RNDR_CZ};
        const float radius = min(RNDR_CX, min(RNDR_CY, RNDR_CZ)) * 0.9f;
        // Normals along tetrahedral directions avoid a common line of intersection.
        const Point3 normals[rings] = {{1,1,1}, {1,-1,-1}, {-1,1,-1}, {-1,-1,1}};
        const CRGBPalette16 rainbow = RainbowColors_p;
        TextureState nucleus(MODE_SOLID);
        const uint32_t started = millis();
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            const uint32_t elapsed = uint32_t(millis() - started);
            const uint32_t tumbleStart = rings * ringTraceMs + holdMs;
            const uint32_t movingMs = elapsed > tumbleStart ? elapsed - tumbleStart : 0;
            const GeometryEngine::TumbleRotation rotation(GeometryEngine::tumbleAngles(movingMs * 0.2f, 0.4f));

            clearAll();
            for (unsigned i = 0; i < rings; ++i) {
                const auto &n = normals[i];
                const float reveal = (float(elapsed) - i * ringTraceMs) / ringTraceMs;
                const float a = radius * 0.70710678f, b = radius * 0.40824829f;
                const Point3 u{a*n.y, -a*n.x, 0};
                const Point3 v{b*n.z*n.x, b*n.z*n.y, -2.0f*b};
                GeometryEngine::drawEllipse3D(center, rotation.apply(u), rotation.apply(v), reveal,
                                               rainbow, uint8_t(i * 64 + movingMs / 40));
            }
            nucleus.palette[0] = CHSV(uint8_t(elapsed / 48), 255, 255);
            GeometryEngine::drawSphere(center.x, center.y, center.z, radius * 0.29f,
                                       RENDER_SOLID, nucleus, 0, 0, 0, false);
            showCube();
            delay(stepMs);
        }
    }
};
