#pragma once

#include "AppMemory.h"
#include "GeometryEngine.h"

#include <new>

namespace ParticleEngine {
    using GeometryEngine::Point3;

    struct Motion {
        Point3 position;
        Point3 velocity;
    };

    struct Particle {
        bool active;
        uint8_t type; // Optional scene-specific particle tag.
        Motion motion;
        float drag;
        float gravity;
        float life;  // Normalized fade, from 1 (full color) to 0.
        float decay; // Fade per second.
        CRGB color;
        bool hasTail;
    };

    constexpr int maxParticles = SHARED_APP_MEMORY_BYTES / sizeof(Particle);
    static_assert(maxParticles > 0, "No particle fits in the scratch arena");

    // A scene owns this view of the scratch arena until it returns.
    class Pool {
    public:
        explicit Pool(int requested)
            : particles_(reinterpret_cast<Particle*>(SharedAppMemory)),
              size_(min(requested, maxParticles)) {
            for (int i = 0; i < size_; ++i) new (&particles_[i]) Particle{};
        }

        int size() const { return size_; }
        Particle& operator[](int index) { return particles_[index]; }

        int firstFree(int start = 0) const {
            for (int i = start; i < size_; ++i) {
                if (!particles_[i].active) return i;
            }
            return size_;
        }

        bool anyAlive() const {
            for (int i = 0; i < size_; ++i) {
                if (particles_[i].active) return true;
            }
            return false;
        }

    private:
        Particle* particles_;
        int size_;
    };

    // Drag is the fraction of velocity kept per simulation step. Callers
    // that tune drag for a specific step use that same step on every update.
    inline void advance(Motion& motion, float dt, Point3 acceleration, float drag) {
        auto& p = motion.position;
        auto& v = motion.velocity;
        p.x += v.x * dt;
        p.y += v.y * dt;
        p.z += v.z * dt;
        v.x = v.x * drag + acceleration.x * dt;
        v.y = v.y * drag + acceleration.y * dt;
        v.z = v.z * drag + acceleration.z * dt;
    }

    inline void advance(Particle& particle, float dt) {
        advance(particle.motion, dt, {0, 0, -particle.gravity}, particle.drag);
        particle.life -= particle.decay * dt;
        if (particle.life <= 0.0f) particle.active = false;
    }

    enum class Side { Open, Bounce, Wrap };

    inline bool resolveSide(float& position, float& speed, int extent,
                            Side side, float restitution) {
        const float last = extent - 1.0f;
        if (side == Side::Bounce) {
            if (position < 0.0f || position > last) {
                position = constrain(position, 0.0f, last);
                speed = -speed * restitution;
            }
            return true;
        }
        if (side == Side::Wrap) {
            if (position < 0.0f) position += extent;
            if (position >= extent) position -= extent;
            return true;
        }
        // Keep fractional edge coverage until the spark clears the wall.
        return position > -1.0f && position < extent;
    }

    inline void bounceFloor(Motion& motion, float restitution, float roughness) {
        auto& p = motion.position;
        auto& v = motion.velocity;
        if (p.z > 0.0f) return;
        p.z = 0.0f;
        if (v.z >= 0.0f) return;
        v.z *= -restitution;
        if (v.z < 1.0f) {
            v.z = 0.0f;
            return;
        }
        // Convert a little rebound speed into a sideways kick.
        const uint16_t angle = random16();
        const float kick = v.z * roughness;
        v.x += kick * cos16(angle) / 32768.0f;
        v.y += kick * sin16(angle) / 32768.0f;
        v.z *= 1.0f - roughness;
    }

    inline void drawSmooth(Point3 position, CRGB color, bool wrapX) {
        const auto x = lroundf(position.x * 256.0f);
        const auto y = lroundf(position.y * 256.0f);
        const auto z = lroundf(position.z * 256.0f);
        GeometryEngine::drawWuVoxel(x, y, z, color);
        // The renderer clips at the cube edge. Draw the other half across
        // the seam when the final column shares coverage with the first.
        if (wrapX && x > (RNDR_X - 1) * 256) {
            GeometryEngine::drawWuVoxel(x - RNDR_X * 256, y, z, color);
        }
    }

    // Sample a soft sphere in its local bounds so larger heads stay bright
    // even when their centers fall between voxels.
    inline void drawBall(Point3 center, float radius, CRGB color, bool wrapX) {
        const float outer = radius + 0.5f;
        const float inner = max(0.0f, radius - 0.5f);
        const float outerSq = outer * outer;
        const float fadeScale = 255.0f / (outerSq - inner * inner);
        const int firstX = int(ceilf(center.x - outer));
        const int lastX = int(floorf(center.x + outer));
        const int firstY = max(0, int(ceilf(center.y - outer)));
        const int lastY = min(RNDR_Y - 1, int(floorf(center.y + outer)));
        const int firstZ = max(0, int(ceilf(center.z - outer)));
        const int lastZ = min(RNDR_Z - 1, int(floorf(center.z + outer)));
        for (int x = firstX; x <= lastX; ++x) {
            if (!wrapX && (x < 0 || x >= RNDR_X)) continue;
            const int voxelX = wrapX ? (x % RNDR_X + RNDR_X) % RNDR_X : x;
            const float dx = x - center.x;
            for (int y = firstY; y <= lastY; ++y) {
                const float dy = y - center.y;
                const float xySq = dx * dx + dy * dy;
                for (int z = firstZ; z <= lastZ; ++z) {
                    const float dz = z - center.z;
                    const float coverage = (outerSq - xySq - dz * dz) * fadeScale;
                    if (coverage <= 0.0f) continue;
                    CRGB lit = color;
                    lit.nscale8(uint8_t(min(255.0f, coverage)));
                    setVoxel(voxelX, y, z, getVoxel(voxelX, y, z) + lit);
                }
            }
        }
    }
}
