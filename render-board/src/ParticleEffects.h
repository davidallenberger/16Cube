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
        constexpr uint8_t afterimageRetention = 48; // Under 4% after two display frames.
        static uint8_t frameCoverage[(RNDR_X * RNDR_Y * RNDR_Z + 7) / 8];
        unsigned geqFrame = 0;
        int gravityRemainder = 0;
        const auto speedSpread = [](int speed) { return speed * 15 / 100; };
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
                drawBall(p, 0.55f, color, xSide == Side::Wrap, true, 1.0f,
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
                            copy.x += lateralSpeed(spray & 255u, (spray >> 8) & 255u) * ageSeconds;
                            copy.y += lateralSpeed((spray >> 16) & 255u, spray >> 24) * ageSeconds;
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
                snprintf(detail, sizeof(detail), "PS GEQ curve=%s gain=%.1f light=%.1f rawPeak=%d drivePeak=%d clips=%u "
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
}
