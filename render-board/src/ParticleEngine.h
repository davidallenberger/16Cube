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

    enum ParticleFlag : uint8_t { KineticTail = 1, HitFloor = 2, WaterfallBackRow = 4 };

    struct Particle {
        bool active;
        uint8_t type; // Optional scene-specific particle tag.
        Motion motion;
        float drag;
        float gravity;
        float life;  // Normalized fade, from 1 (full color) to 0.
        float decay; // Fade per second.
        CRGB color;
        uint8_t flags;
    };

    // A scene owns this view of the scratch arena until it returns.
    template <typename Item>
    class ParticlePool {
    public:
        static constexpr int capacity = SHARED_APP_MEMORY_BYTES / sizeof(Item);
        static_assert(capacity > 0, "No particle fits in the scratch arena");

        explicit ParticlePool(int requested)
            : particles_(reinterpret_cast<Item*>(SharedAppMemory)),
              size_(min(requested, capacity)) {
            for (int i = 0; i < size_; ++i) new (&particles_[i]) Item{};
        }

        int size() const { return size_; }
        Item& operator[](int index) { return particles_[index]; }

        int firstFree(int start = 0) const {
            for (int i = start; i < size_; ++i) {
                if (!particles_[i].active) return i;
            }
            return size_;
        }

        void spawn(const Item& particle) {
            const int slot = firstFree();
            if (slot < size_) particles_[slot] = particle;
        }

        bool anyAlive() const {
            for (int i = 0; i < size_; ++i) {
                if (particles_[i].active) return true;
            }
            return false;
        }

    private:
        Item* particles_;
        int size_;
    };

    using Pool = ParticlePool<Particle>;
    constexpr int maxParticles = Pool::capacity;

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
                            Side side, float restitution, float inset = 0.0f) {
        const float last = extent - 1.0f;
        if (side == Side::Bounce) {
            const float low = min(inset, last * 0.5f);
            const float high = last - low;
            if (position < low || position > high) {
                // Contact separation can put a particle outside a wall
                // even when its velocity already points back into the box.
                if (position < low ? speed < 0.0f : speed > 0.0f) speed = -speed * restitution;
                position = constrain(position, low, high);
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

    inline void bounceFloor(Motion& motion, float restitution, float roughness, float height = 0.0f) {
        auto& p = motion.position;
        auto& v = motion.velocity;
        if (p.z > height) return;
        p.z = height;
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

    inline void resolveSphereContact(Motion& a, Motion& b, float contact, float restitution,
                                     bool wrapX = false, float inverseMassA = 1.0f, float inverseMassB = 1.0f) {
        Point3 d{b.position.x - a.position.x, b.position.y - a.position.y, b.position.z - a.position.z};
        if (wrapX && fabsf(d.x) > RNDR_X * 0.5f) d.x += d.x > 0 ? -RNDR_X : RNDR_X;
        const float distanceSq = d.x*d.x + d.y*d.y + d.z*d.z;
        if (distanceSq >= contact * contact) return;

        const float distance = sqrtf(distanceSq);
        const Point3 n = distance > 0.0001f ? Point3{d.x / distance, d.y / distance, d.z / distance}
                                            : Point3{1.0f, 0.0f, 0.0f};
        const float shareA = inverseMassA / (inverseMassA + inverseMassB);
        const float shareB = 1.0f - shareA;
        const float correction = 0.8f * (contact - distance);
        const float moveA = correction * shareA, moveB = correction * shareB;
        a.position.x -= n.x * moveA;
        a.position.y -= n.y * moveA;
        a.position.z -= n.z * moveA;
        b.position.x += n.x * moveB;
        b.position.y += n.y * moveB;
        b.position.z += n.z * moveB;

        const float closing = (a.velocity.x - b.velocity.x) * n.x +
                              (a.velocity.y - b.velocity.y) * n.y +
                              (a.velocity.z - b.velocity.z) * n.z;
        if (closing <= 0.0f) return;
        const float impulse = (1.0f + restitution) * closing;
        const float kickA = impulse * shareA, kickB = impulse * shareB;
        a.velocity.x -= kickA * n.x;
        a.velocity.y -= kickA * n.y;
        a.velocity.z -= kickA * n.z;
        b.velocity.x += kickB * n.x;
        b.velocity.y += kickB * n.y;
        b.velocity.z += kickB * n.z;
    }

    // Choose cells at least as wide as the largest contact diameter.
    // Grid callers use equal mass and one impulse per contact.
    template <int cellSize = 4, typename Item, typename Radius>
    inline void collide(ParticlePool<Item>& pool, float restitution, bool wrapX, Radius radius) {
        constexpr int nx = (RNDR_X + cellSize - 1) / cellSize;
        constexpr int ny = (RNDR_Y + cellSize - 1) / cellSize;
        constexpr int nz = (RNDR_Z + cellSize - 1) / cellSize;
        int16_t heads[nx * ny * nz];
        int16_t next[ParticlePool<Item>::capacity];
        auto cellIndex = [](int x, int y, int z) { return (z * ny + y) * nx + x; };
        for (auto& head : heads) head = -1;
        for (int i = 0; i < pool.size(); ++i) {
            next[i] = -2; // Not in the grid.
            const auto& p = pool[i].motion.position;
            if (!pool[i].active || p.x < 0 || p.x >= RNDR_X ||
                p.y < 0 || p.y >= RNDR_Y || p.z < 0 || p.z >= RNDR_Z) continue;
            auto& head = heads[cellIndex(int(p.x) / cellSize, int(p.y) / cellSize,
                                         int(p.z) / cellSize)];
            next[i] = head;
            head = i;
        }
        for (int i = 0; i < pool.size(); ++i) {
            if (next[i] == -2) continue;
            auto& a = pool[i].motion;
            const int cx = constrain(int(a.position.x) / cellSize, 0, nx - 1);
            const int cy = constrain(int(a.position.y) / cellSize, 0, ny - 1);
            const int cz = constrain(int(a.position.z) / cellSize, 0, nz - 1);
            const int firstX = wrapX && nx > 2 ? cx - 1 : max(0, cx - 1);
            const int lastX = wrapX && nx > 2 ? cx + 1 : min(nx - 1, cx + 1);
            for (int z = max(0, cz - 1); z <= min(nz - 1, cz + 1); ++z) {
                for (int y = max(0, cy - 1); y <= min(ny - 1, cy + 1); ++y) {
                    for (int x = firstX; x <= lastX; ++x) {
                        for (int j = heads[cellIndex((x + nx) % nx, y, z)]; j >= 0; j = next[j]) {
                            if (j <= i) continue;
                            resolveSphereContact(a, pool[j].motion, radius(pool[i]) + radius(pool[j]),
                                                 restitution, wrapX);
                        }
                    }
                }
            }
        }
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
    inline void drawBall(Point3 center, float radius, CRGB color, bool wrapX,
                         bool fullBrightnessCore = false) {
        const float outer = fullBrightnessCore ? max(1.0f, radius + 0.5f) : radius + 0.5f;
        const float inner = max(0.0f, radius - 0.5f);
        const float outerSq = outer * outer;
        float innerSq = inner * inner;
        if (fullBrightnessCore) {
            // Expand the core just enough to reach the closest voxel center.
            const float dx = center.x - roundf(center.x);
            const float dy = center.y - roundf(center.y);
            const float dz = center.z - roundf(center.z);
            innerSq = max(innerSq, dx * dx + dy * dy + dz * dz);
        }
        const float fadeScale = 255.0f / (outerSq - innerSq);
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
                    if (!fullBrightnessCore || xySq + dz * dz > innerSq) {
                        lit.nscale8(uint8_t(min(255.0f, coverage)));
                    }
                    setVoxel(voxelX, y, z, getVoxel(voxelX, y, z) + lit);
                }
            }
        }
    }
}
