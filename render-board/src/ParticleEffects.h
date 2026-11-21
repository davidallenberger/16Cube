#pragma once
#include "Audio.h"
#include "ParticleEffectSupport.h"
#include "UARTReceiver.h"

namespace ParticleEffects {
    enum class GeqLayout { Clusters, Tiles, PairedBands };

    struct GeqSettings {
        uint8_t speed = 165, intensity = 200, diverge = 28, bounce = 128, gravity = 16;
        bool cylinder = false, walls = false, floor = false;
        GeqLayout layout = GeqLayout::Clusters;
        float inputGain = 1.0f;
        float brightnessGain = 1.0f;
        // Audition B (owner-approved 2026-11-20): a more liquid stream with
        // longer trails joining successive launches, more speed jitter, a
        // small sway instead of straight drift, and slightly wider streams.
        // Lateral motion stays small so the 16 columns do not interfere.
        bool liquid = false;
    };

    inline void animateGeq(uint32_t durationMs, const GeqSettings& settings = {},
                           const CRGBPalette16& palette = RainbowColors_p, bool (*keepDrawing)() = nullptr) {
        using namespace ParticleEffectSupport;
        const bool tiled = settings.layout == GeqLayout::Tiles;
        const bool pairedBands = settings.layout == GeqLayout::PairedBands;
        const int binsPerBand = pairedBands ? 2 : 1;
        const int bandCount = 16 / binsPerBand;
        const int nozzlesPerBand = 256 / bandCount;
        const Side xSide = settings.walls ? Side::Bounce : (settings.cylinder ? Side::Wrap : Side::Open);
        const Side ySide = settings.walls ? Side::Bounce : Side::Open;
        const float bounce = settings.bounce / 255.0f;
        const int gravity = min(31, int(settings.gravity)) << 2; // WLED 3.4 fixed-point force.
        const int divergence = settings.diverge >> 2;
        const auto lateralSpeed = [divergence](int first, int second) {
            return ((first % (divergence + 1)) - (second % (divergence + 1))) * wledVelocity;
        };
        const float inputGain = std::isfinite(settings.inputGain) ? constrain(settings.inputGain, 0.0f, 8.0f) : 1.0f;
        const float brightnessGain = std::isfinite(settings.brightnessGain) ? constrain(settings.brightnessGain, 0.0f, 8.0f) : 1.0f;
        constexpr float geqStep = 1.0f / 42.0f;
        constexpr unsigned batchInterval = 6; // Simultaneous launches every ~143 ms.
        // Under 4% after two display frames; liquid keeps ~59% per frame so
        // launches ~143 ms apart join into one stream.
        const uint8_t afterimageRetention = settings.liquid ? 150 : 48;
        const bool liquid = settings.liquid;
        static uint8_t frameCoverage[(RNDR_X * RNDR_Y * RNDR_Z + 7) / 8];
        unsigned geqFrame = 0;
        int gravityRemainder = 0;
        const auto speedSpread = [liquid](int speed) { return speed * (liquid ? 25 : 15) / 100; };
        const auto tileNozzle = [](int bin, int nozzle) -> Point3 {
            // Swap bands 7/16 and 9/15 to give the center two active mids.
            constexpr uint8_t placement[] = {0, 1, 2, 3, 4, 5, 15, 7, 14, 9, 10, 11, 12, 13, 8, 6};
            const int tile = AudioLayouts::orbitInwardTiles[placement[bin]];
            return {AudioLayouts::inwardCoordinate(tile % 4, nozzle % 4, RNDR_X),
                    AudioLayouts::inwardCoordinate(tile / 4, nozzle / 4, RNDR_Y), 0.0f};
        };
        const auto clusterNozzle = [](int bin, int block) -> Point3 {
            const int tile = AudioLayouts::orbitInwardTiles[bin];
            const int quadrant = block / 4;
            const int column = 2 * (tile % 4) + block % 2;
            const int row = 2 * (tile / 4) + (block % 4) / 2;
            return {AudioLayouts::inwardCoordinate(quadrant % 2, column, RNDR_X, 2),
                    AudioLayouts::inwardCoordinate(quadrant / 2, row, RNDR_Y, 2), 0.0f};
        };
        const auto pairedBandNozzle = [](int band, int nozzle) -> Point3 {
            // Sweep clockwise across two rows of four 4×8 patches.
            const int row = band / 4;
            const int column = row == 0 ? band : 7 - band;
            return {AudioLayouts::inwardCoordinate(column, nozzle % 4, RNDR_X),
                    AudioLayouts::inwardCoordinate(row, nozzle / 4, RNDR_Y, 2), 0.0f};
        };
        const auto nozzlePosition = [&](int bin, int nozzle) {
            if (pairedBands) return pairedBandNozzle(bin, nozzle);
            return tiled ? tileNozzle(bin, nozzle) : clusterNozzle(bin, nozzle);
        };
        // Keep the nozzle row at full band color, independent of the spray
        // and its brightness and height diagnostics.
        CRGB floorColors[RNDR_Y][RNDR_X]{};
        for (int bin = 0; bin < bandCount; ++bin) {
            const CRGB color = ColorFromPalette(palette, bin * (256 / bandCount), 255);
            for (int nozzle = 0; nozzle < nozzlesPerBand; ++nozzle) {
                const auto point = nozzlePosition(bin, nozzle);
                const int x = constrain(int(lroundf(point.x)), 0, RNDR_X - 1);
                const int y = constrain(int(lroundf(point.y)), 0, RNDR_Y - 1);
                floorColors[y][x] = color;
            }
        }
        const Point3 fixedOrigin{RNDR_CX, RNDR_CY, 0.0f};
        // One record per band launch. A seed varies the nozzle copies
        // independently without allocating a particle for every nozzle.
        struct GeqParticle {
            Motion motion;
            uint16_t age;
            uint8_t band, speed, variation;
            bool active;
        };
        static_assert(sizeof(GeqParticle) <= 32, "PS GEQ needs a compact particle record");
        using GeqPool = ParticlePool<GeqParticle>;
        GeqPool pool(population(GeqPool::capacity, GeqPool::capacity));
        const auto lifetime = [&](int speed, int variation) {
            return 20 + (speed >> 1) + (speed - (speed >> 1) + variation) * settings.intensity / 255;
        };
        RemoteInputs::AudioFrame audio{};
        uint8_t displayBins[16] = {};
        bool compensated = false;
        struct GeqPeaks {
            int raw, driven, launch, voxel;
            unsigned clipped, spawned, blocked;
            float height, flightHeight;
            bool fallingVisible;
        } peaks{};
        char detail[192] = "PS GEQ measuring first 500 ms";
        uint32_t lastReport = millis();
        run(durationMs, afterimageRetention, [&](uint32_t tick) {
            if (!wledFrameDue(tick)) return;
            const bool launchFrame = geqFrame % batchInterval == 0;
            audio = RemoteInputs::audio();
            compensated = Audio::displaySpectrum(audio, displayBins);
            int nextSlot = 0;
            bool poolBlocked = false;
            // Rotate priority between batches if the particle pool fills.
            const int firstBin = (geqFrame / batchInterval) % bandCount;
            ++geqFrame;
            for (int i = 0; i < bandCount; ++i) {
                const int band = (firstBin + i) % bandCount;
                const int bin = band * binsPerBand;
                const int lastBin = bin + binsPerBand - 1;
                const int raw = max(audio.fftResult[bin], audio.fftResult[lastBin]);
                const int amplified = lroundf(max(displayBins[bin], displayBins[lastBin]) * inputGain);
                const int level = min(255, amplified);
                peaks.raw = max(peaks.raw, raw);
                peaks.driven = max(peaks.driven, level);
                peaks.clipped += amplified > 255;
                if (!launchFrame || poolBlocked || level == 0) continue;
                const int speed = (level * settings.speed) >> 9;
                if (speed <= (gravity >> 4)) continue;
                nextSlot = pool.firstFree(nextSlot);
                if (nextSlot == pool.size()) { poolBlocked = true; continue; }
                // Slower copies share gravity and derive their displacement at draw time.
                const int referenceSpeed = speed + speedSpread(speed);
                const Point3 velocity{0.0f, 0.0f, referenceSpeed * wledVelocity * verticalScale};
                pool[nextSlot++] = {{fixedOrigin, velocity}, 0, uint8_t(band), uint8_t(speed), random8(), true};
                peaks.launch = max(peaks.launch, referenceSpeed);
                peaks.spawned += nozzlesPerBand;
            }
            peaks.blocked += poolBlocked;
            // WLED applies gravity before moving. At low forces, save fractional kicks.
            const int force = gravity >= 16 ? gravity : gravityRemainder + gravity;
            gravityRemainder = gravity < 16 ? force % 16 : 0;
            const float kick = (force / 16) * wledVelocity * verticalScale;
            const float terminal = 127 * wledVelocity * verticalScale;
            for (int i = 0; i < pool.size(); ++i) {
                auto& particle = pool[i];
                if (!particle.active) continue;
                const bool expired = ++particle.age >= lifetime(particle.speed, particle.speed - 1);
                if (expired) { particle.active = false; continue; }
                auto& motion = particle.motion;
                auto& p = motion.position;
                auto& v = motion.velocity;
                v.z = max(-terminal, v.z - kick);
                advance(motion, geqStep, {}, 1.0f);
                particle.active = resolveSide(p.x, v.x, RNDR_X, xSide, bounce) &&
                                  resolveSide(p.y, v.y, RNDR_Y, ySide, bounce) &&
                                  (settings.floor || p.z > -1.0f);
                if (particle.active && settings.floor) bounceFloor(motion, bounce, 0.0f);
                if (particle.active && !gravity && p.z > RNDR_Z - 1.0f) {
                    particle.active = resolveSide(p.z, v.z, RNDR_Z, settings.floor ? Side::Bounce : Side::Open, bounce);
                }
            }
        }, [&] {
            const auto measure = [&](Point3 p, bool falling) {
                const int x = constrain(int(lroundf(p.x)), 0, RNDR_X - 1);
                const int y = constrain(int(lroundf(p.y)), 0, RNDR_Y - 1);
                const int z = constrain(int(lroundf(p.z)), 0, RNDR_Z - 1);
                const CRGB lit = getVoxel(x, y, z);
                const int voxel = max(lit.r, max(lit.g, lit.b));
                peaks.voxel = max(peaks.voxel, voxel);
                if (voxel > 0) {
                    peaks.height = max(peaks.height, min(p.z, float(RNDR_Z - 1)));
                    peaks.fallingVisible |= falling;
                }
            };
            const auto draw = [&](Point3 p, uint8_t hue, int remainingFrames, float fade, bool falling) {
                if (remainingFrames <= 0 || p.x <= -1.0f || p.x >= RNDR_X ||
                    p.y <= -1.0f || p.y >= RNDR_Y) return;
                // WLED fades by remaining lifetime in frames.
                float light = remainingFrames * 2.0f;
                // Keep descending spray visible through the spatial blend,
                // then fade it over the final eight updates.
                if (falling) light = max(light, 36.0f * min(1.0f, remainingFrames / 8.0f));
                const uint8_t brightness = lroundf(min(255.0f, light * brightnessGain) * fade);
                if (brightness == 0) return;
                const CRGB color = ColorFromPalette(palette, hue, brightness);
                drawBall(p, liquid && !falling ? 0.75f : 0.55f, color, xSide == Side::Wrap, true, 1.0f,
                         falling ? ParticleBlend::EmptyOnly : ParticleBlend::Brightest, frameCoverage);
                measure(p, falling);
            };
            const auto drawParticles = [&](bool fallingPass) {
                for (int i = 0; i < pool.size(); ++i) {
                    const auto& particle = pool[i];
                    if (!particle.active) continue;
                    const auto& p = particle.motion.position;
                    const uint8_t hue = particle.band * (256 / bandCount);
                    const float ageSeconds = particle.age * geqStep;
                    const int spread = speedSpread(particle.speed);
                    for (int block = 0; block < nozzlesPerBand; ++block) {
                        Point3 copy = nozzlePosition(particle.band, block);
                        copy.z = p.z;
                        const uint16_t variation = uint16_t((particle.variation * 257u ^ (block + 1) * 0x9E37u) * 40503u);
                        const uint16_t speedSeed = uint16_t(variation * 25173u + 13849u);
                        const int speedBelowReference = (uint32_t(speedSeed) * (2 * spread + 1)) >> 16;
                        const float velocityOffset = speedBelowReference * wledVelocity * verticalScale;
                        copy.z -= velocityOffset * ageSeconds;
                        const bool falling = particle.motion.velocity.z < velocityOffset;
                        if (falling != fallingPass) continue;
                        const int life = lifetime(particle.speed, (uint32_t(variation) * particle.speed) >> 16);
                        peaks.flightHeight = max(peaks.flightHeight, copy.z);
                        if (copy.z <= -1.0f || copy.z >= RNDR_Z) continue;
                        if (divergence) {
                            // Constant horizontal velocity with open sides. Derive
                            // it from the launch seed to keep the current pool size.
                            const uint32_t spray = uint32_t(variation) * 0x9E3779B1u;
                            const float drift = liquid ? 0.6f : 1.0f;
                            copy.x += drift * lateralSpeed(spray & 255u, (spray >> 8) & 255u) * ageSeconds;
                            copy.y += drift * lateralSpeed((spray >> 16) & 255u, spray >> 24) * ageSeconds;
                        }
                        if (liquid) {
                            // Gentle churn: half a voxel of sway at ~1.5 Hz,
                            // phased per copy so the stream edge is ragged.
                            const float phase = (variation & 1023u) * (6.2831853f / 1024.0f);
                            copy.x += 0.5f * sinf(ageSeconds * 9.4f + phase);
                            copy.y += 0.5f * cosf(ageSeconds * 7.1f + phase * 1.7f);
                        }
                        // Ease in over two updates and taper across the last voxel
                        // at open boundaries where flight can end before lifetime.
                        const float edge = min(min(copy.y + 1.0f, RNDR_Y - copy.y),
                                               min(copy.z + 1.0f, RNDR_Z - copy.z));
                        const float xEdge = xSide == Side::Wrap ? 1.0f : min(copy.x + 1.0f, RNDR_X - copy.x);
                        const float fade = min(particle.age * 0.5f, constrain(min(edge, xEdge), 0.0f, 1.0f));
                        draw(copy, hue, life - particle.age, fade, falling);
                    }
                }
            };
            // Falling spray can only fill voxels the rising pass left empty.
            for (auto& bits : frameCoverage) bits = 0;
            drawParticles(false);
            drawParticles(true);
            for (int y = 0; y < RNDR_Y; ++y) {
                for (int x = 0; x < RNDR_X; ++x) {
                    const CRGB color = floorColors[y][x];
                    if (color != CRGB::Black) setVoxel(x, y, 0, color);
                }
            }
            const uint32_t now = millis();
            if (uint32_t(now - lastReport) >= 500) {
                snprintf(detail, sizeof(detail), liquid ? "PS GEQ liquid curve=%s gain=%.1f light=%.1f rawPeak=%d drivePeak=%d clips=%u "
                         "launchPeak=%.1f zPeak=%.1f flightPeak=%.1f falling=%u lightPeak=%d spawned=%u poolFullSteps=%u cap=%d" : "PS GEQ curve=%s gain=%.1f light=%.1f rawPeak=%d drivePeak=%d clips=%u "
                         "launchPeak=%.1f zPeak=%.1f flightPeak=%.1f falling=%u lightPeak=%d spawned=%u poolFullSteps=%u cap=%d",
                         compensated ? "comp" : "raw", double(inputGain), double(brightnessGain), peaks.raw, peaks.driven, peaks.clipped,
                         double(peaks.launch * wledVelocity * verticalScale), double(peaks.height),
                         double(peaks.flightHeight), unsigned(peaks.fallingVisible), peaks.voxel,
                         peaks.spawned, peaks.blocked, pool.size());
                peaks = {};
                lastReport = now;
            }
            Audio::debug(audio, detail, 0, displayBins);
        }, keepDrawing);
    }

    // PS GEQ Nova (owner idea, 2026-11-20): eight particle hoses at the
    // cube's center, 45 degrees apart, one per band pair (bass first). Each
    // sprays outward a little above horizontal; louder bands spray farther,
    // denser and brighter. Mild gravity bends the streams so the lower half
    // stays readable. Silence leaves a dim colored nozzle ring at the center.
    inline void animateNova(const CRGBPalette16& palette, float inputGain, float brightnessGain,
                            bool (*keepDrawing)() = nullptr) {
        using namespace ParticleEffectSupport;
        constexpr int hoses = 8;
        constexpr float lifeSeconds = 0.9f;
        constexpr float gravityPerSecond = 5.0f * verticalScale;
        constexpr float tilt = 0.25f; // Rise per unit of outward speed.
        struct NovaParticle {
            Motion motion;
            float age;
            uint8_t hose;
            bool active;
        };
        using NovaPool = ParticlePool<NovaParticle>;
        NovaPool pool(NovaPool::capacity);
        inputGain = std::isfinite(inputGain) ? constrain(inputGain, 0.0f, 8.0f) : 1.0f;
        brightnessGain = std::isfinite(brightnessGain) ? constrain(brightnessGain, 0.0f, 8.0f) : 1.0f;
        const float reach = min(RNDR_CX, RNDR_CY) + 0.5f;
        RemoteInputs::AudioFrame audio{};
        uint8_t bins[16] = {};
        float levels[hoses] = {};
        float emitDebt[hoses] = {};
        unsigned spawned = 0, blocked = 0;
        char detail[160] = "PS GEQ Nova measuring";
        uint32_t lastReport = millis();
        unsigned firstHose = 0;
        // Long trails (~67% retained per frame) join particles into streams.
        run(0, 170, [&](uint32_t) {
            audio = RemoteInputs::audio();
            Audio::displaySpectrum(audio, bins);
            int nextSlot = 0;
            // Rotate priority so a full pool never starves the same hoses.
            firstHose = (firstHose + 1) % hoses;
            for (int step = 0; step < hoses; ++step) {
                const int hose = int((firstHose + step) % hoses);
                const int level = min(255, int(lroundf(max(bins[2 * hose], bins[2 * hose + 1]) * inputGain)));
                levels[hose] = level / 255.0f;
                if (level < 12) { emitDebt[hose] = 0.0f; continue; }
                // Sized to the pool: about 256 live particles over 0.9 s.
                emitDebt[hose] += 0.2f + 0.6f * levels[hose];
                const float angle = hose * (6.2831853f / hoses);
                const float dx = cosf(angle), dy = sinf(angle);
                // Outward speed so the stream's reach follows the level.
                const float speed = (0.25f + 0.75f * levels[hose]) * reach / (lifeSeconds * 0.8f);
                while (emitDebt[hose] >= 1.0f) {
                    emitDebt[hose] -= 1.0f;
                    nextSlot = pool.firstFree(nextSlot);
                    if (nextSlot == pool.size()) { ++blocked; emitDebt[hose] = 0.0f; break; }
                    const float jitter = sample(0.85f, 1.15f);
                    const float spreadX = sample(-0.2f, 0.2f), spreadY = sample(-0.2f, 0.2f);
                    const Point3 velocity{(dx + spreadX) * speed * jitter, (dy + spreadY) * speed * jitter,
                                          (tilt + sample(-0.08f, 0.08f)) * speed};
                    const Point3 origin{RNDR_CX + dx * 0.6f, RNDR_CY + dy * 0.6f, RNDR_CZ};
                    pool[nextSlot++] = {{origin, velocity}, 0.0f, uint8_t(hose), true};
                    ++spawned;
                }
            }
            for (int i = 0; i < pool.size(); ++i) {
                auto& particle = pool[i];
                if (!particle.active) continue;
                particle.age += dt;
                auto& p = particle.motion.position;
                auto& v = particle.motion.velocity;
                v.z -= gravityPerSecond * dt;
                p.x += v.x * dt; p.y += v.y * dt; p.z += v.z * dt;
                particle.active = particle.age < lifeSeconds &&
                    p.x > -1.0f && p.x < RNDR_X && p.y > -1.0f && p.y < RNDR_Y && p.z > -1.0f && p.z < RNDR_Z;
            }
        }, [&] {
            for (int i = 0; i < pool.size(); ++i) {
                const auto& particle = pool[i];
                if (!particle.active) continue;
                const float remaining = 1.0f - particle.age / lifeSeconds;
                const float light = (0.35f + 0.65f * levels[particle.hose]) * min(1.0f, remaining * 2.0f);
                const uint8_t brightness = uint8_t(min(255.0f, 255.0f * light * min(1.0f, brightnessGain * 0.5f)));
                if (brightness == 0) continue;
                const CRGB color = ColorFromPalette(palette, uint8_t(particle.hose * (256 / hoses)), brightness);
                drawBall(particle.motion.position, 1.0f, color, false, true, 1.0f, ParticleBlend::Brightest);
            }
            // Nozzle ring: always visible, brighter with each band's level.
            for (int hose = 0; hose < hoses; ++hose) {
                const float angle = hose * (6.2831853f / hoses);
                const int x = constrain(int(lroundf(RNDR_CX + cosf(angle))), 0, RNDR_X - 1);
                const int y = constrain(int(lroundf(RNDR_CY + sinf(angle))), 0, RNDR_Y - 1);
                const uint8_t z = uint8_t(lroundf(RNDR_CZ));
                const CRGB color = ColorFromPalette(palette, uint8_t(hose * (256 / hoses)),
                                                    uint8_t(48 + 207 * levels[hose]));
                setVoxel(x, y, min(int(z), RNDR_Z - 1), color);
            }
            const uint32_t now = millis();
            if (uint32_t(now - lastReport) >= 500) {
                snprintf(detail, sizeof(detail),
                         "PS GEQ Nova levels=%.2f/%.2f/%.2f/%.2f/%.2f/%.2f/%.2f/%.2f spawned=%u poolFull=%u cap=%d",
                         double(levels[0]), double(levels[1]), double(levels[2]), double(levels[3]),
                         double(levels[4]), double(levels[5]), double(levels[6]), double(levels[7]),
                         spawned, blocked, pool.size());
                spawned = blocked = 0;
                lastReport = now;
            }
            Audio::debug(audio, detail, 0, bins);
        }, keepDrawing);
    }
}
