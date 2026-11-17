#pragma once
#include "ParticleEngine.h"
#include "TestDemo.h"

namespace ParticleEffectSupport {
    using namespace ParticleEngine;

    constexpr float verticalScale = RNDR_Z / 16.0f;
    constexpr float volumeScale = RNDR_X * RNDR_Y * RNDR_Z / 4096.0f;
    constexpr float dt = 0.016667f;
    constexpr float wledVelocity = 42.0f / 64.0f;
    constexpr float wledGravity = wledVelocity * 42.0f / 16.0f;

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


}
