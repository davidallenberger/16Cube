#pragma once
#include "CubeCore.h"
#include "TextureEngine.h"

enum SplitWaveMode2026 {
    SPLIT_2026_MIRRORED_Z,
    SPLIT_2026_OPPOSING_X,
    SPLIT_2026_OPPOSING_Y,
    SPLIT_2026_RANDOM
};
enum RenderMode {
    RENDER_SOLID,
    RENDER_SHELL,
    RENDER_WIREFRAME
};

enum WaveShape {
    WAVE_RADIAL,     // Drops like a pebble
    WAVE_DIRECTIONAL // Rolls like an ocean wave
};

namespace GeometryEngine {

    static inline int16_t max4(int16_t a, int16_t b, int16_t c, int16_t d) {
        int16_t m = a;
        if (b > m) m = b;
        if (c > m) m = c;
        if (d > m) m = d;
        return m;
    }

   inline uint8_t getCoverage(int d_surface, int d_edge, RenderMode mode, int blur = 2) {
        if (mode == RENDER_SOLID) {
            if (d_surface <= -blur) return 255; 
            if (d_surface >= blur) return 0; 
            return 255 - ((d_surface + blur) * 255 / (blur * 2));
        }
        else if (mode == RENDER_SHELL) {
            int abs_d = abs(d_surface);
            if (abs_d >= blur) return 0;
            return 255 - (abs_d * 255 / blur);
        }
        else if (mode == RENDER_WIREFRAME) {
            // FIX: Ensure we are actually on the outer shell, not deep inside
            int abs_surf = abs(d_surface);
            if (abs_surf >= blur) return 0; 
            
            int abs_edge = abs(d_edge);
            if (abs_edge >= blur) return 0; 

            uint8_t edge_cov = 255 - (abs_edge * 255 / blur);
            uint8_t surf_cov = 255 - (abs_surf * 255 / blur);
            
            // Fade the wireframe line gracefully in all directions
            return (edge_cov < surf_cov) ? edge_cov : surf_cov;
        }
        return 0;
    }

    inline void drawFastLine3D(int x0, int y0, int z0, int x1, int y1, int z1, const TextureState &tex) {
        int dx = x1 - x0, dy = y1 - y0, dz = z1 - z0;
        int steps = max(abs(dx), max(abs(dy), abs(dz)));

        if (steps == 0) {
            if (x0 >= 0 && x0 < RNDR_X && y0 >= 0 && y0 < RNDR_Y && z0 >= 0 && z0 < RNDR_Z) {
                setVoxel(x0, y0, z0, tex.getColor(x0, y0, z0));
            }
            return;
        }

        float fx = (float)x0, fy = (float)y0, fz = (float)z0;
        float sx = (float)dx / steps, sy = (float)dy / steps, sz = (float)dz / steps;

        for (int i = 0; i <= steps; i++) {
            int xi = (int)lroundf(fx), yi = (int)lroundf(fy), zi = (int)lroundf(fz);
            if (xi >= 0 && xi < RNDR_X && yi >= 0 && yi < RNDR_Y && zi >= 0 && zi < RNDR_Z) {
                setVoxel(xi, yi, zi, tex.getColor(xi, yi, zi));
            }
            fx += sx; fy += sy; fz += sz;
        }
    }

    // Blend along a strand while keeping its horizontal address fixed.
    inline void drawStrandVoxel(int x, int y, float z, CRGB color) {
        if (x < 0 || x >= RNDR_X || y < 0 || y >= RNDR_Y || z <= -1.0f || z >= RNDR_Z) return;
        const int lower = int(floorf(z));
        const uint8_t upperWeight = uint8_t((z - lower) * 255.0f);
        if (lower >= 0) {
            CRGB bottom = color;
            bottom.nscale8(255 - upperWeight);
            setVoxel(x, y, lower, getVoxel(x, y, lower) + bottom);
        }
        if (upperWeight > 0 && lower + 1 < RNDR_Z) {
            color.nscale8(upperWeight);
            setVoxel(x, y, lower + 1, getVoxel(x, y, lower + 1) + color);
        }
    }

    // ==========================================
    // 3D WU VOXEL (Sub-Voxel Anti-Aliasing)
    // ==========================================
    inline void drawWuVoxel(int32_t x_fp, int32_t y_fp, int32_t z_fp, CRGB col) {
        int ix = x_fp >> 8;
        int iy = y_fp >> 8;
        int iz = z_fp >> 8;
        
        uint8_t fx = x_fp & 0xFF;
        uint8_t fy = y_fp & 0xFF;
        uint8_t fz = z_fp & 0xFF;
        
        uint8_t ifx = 255 - fx;
        uint8_t ify = 255 - fy;
        uint8_t ifz = 255 - fz;

        uint8_t w000 = ((uint32_t)ifx * ify * ifz) >> 16;
        uint8_t w100 = ((uint32_t)fx  * ify * ifz) >> 16;
        uint8_t w010 = ((uint32_t)ifx * fy  * ifz) >> 16;
        uint8_t w110 = ((uint32_t)fx  * fy  * ifz) >> 16;
        uint8_t w001 = ((uint32_t)ifx * ify * fz ) >> 16;
        uint8_t w101 = ((uint32_t)fx  * ify * fz ) >> 16;
        uint8_t w011 = ((uint32_t)ifx * fy  * fz ) >> 16;
        uint8_t w111 = ((uint32_t)fx  * fy  * fz ) >> 16;

        auto plot = [](int x, int y, int z, CRGB c, uint8_t w) {
            if (w > 0 && x >= 0 && x < RNDR_X && y >= 0 && y < RNDR_Y && z >= 0 && z < RNDR_Z) {
                CRGB drawn = c;
                drawn.nscale8(w);
                CRGB existing = getVoxel(x, y, z);
                setVoxel(x, y, z, existing + drawn);
            }
        };

        plot(ix,   iy,   iz,   col, w000);
        plot(ix+1, iy,   iz,   col, w100);
        plot(ix,   iy+1, iz,   col, w010);
        plot(ix+1, iy+1, iz,   col, w110);
        plot(ix,   iy,   iz+1, col, w001);
        plot(ix+1, iy,   iz+1, col, w101);
        plot(ix,   iy+1, iz+1, col, w011);
        plot(ix+1, iy+1, iz+1, col, w111);
    }

    struct Point3 { float x, y, z; };

    // A cross section of a helix at the caller's phase and axial offset.
    inline Point3 helixPoint(Point3 center, float radius, float phase, float axialOffset) {
        return {center.x + cosf(phase) * radius, center.y + sinf(phase) * radius,
                center.z + axialOffset};
    }

    // Shared shape tumble in degrees. The caller supplies its own phase and speed.
    inline Point3 tumbleAngles(float phase, float speed) {
        return {phase * (0.5f * speed), phase * (0.8f * speed), phase * (0.3f * speed)};
    }

    struct TumbleRotation {
        float cx, sx, cy, sy, cz, sz;

        explicit TumbleRotation(Point3 angles) {
            const float x = fmodf(angles.x, 360.0f) * (PI / 180.0f);
            const float y = fmodf(angles.y, 360.0f) * (PI / 180.0f);
            const float z = fmodf(angles.z, 360.0f) * (PI / 180.0f);
            cx = cosf(x); sx = sinf(x);
            cy = cosf(y); sy = sinf(y);
            cz = cosf(z); sz = sinf(z);
        }

        Point3 apply(Point3 p) const {
            const float yy = cx * p.y - sx * p.z;
            const float zz = sx * p.y + cx * p.z;
            const float xx = cy * p.x + sy * zz;
            return {cz * xx - sz * yy, sz * xx + cz * yy, -sy * p.x + cy * zz};
        }
    };

    // Draw an ellipse centered at center. Orthogonal semi-axis vectors set
    // its radii and plane. The reveal fraction advances in parameter angle.
    inline void drawEllipse3D(Point3 center, Point3 u, Point3 v, float reveal,
                              const CRGBPalette16 &palette, uint8_t hueOffset) {
        const float a = sqrtf(u.x*u.x + u.y*u.y + u.z*u.z);
        const float b = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
        if (a <= 0.0f || b <= 0.0f || reveal <= 0.0f) return;
        // Sample at most half a voxel apart, even on the faster part of an
        // eccentric ellipse. Omit the duplicate endpoint on a complete ring.
        const unsigned steps = max(8u, unsigned(ceilf(2.0f * TWO_PI * max(a, b))));
        const float visible = min(1.0f, reveal) * steps;
        for (unsigned i = 0; i < steps && i < visible; ++i) {
            const uint16_t phase = uint16_t((uint32_t(i) * 65536u) / steps);
            const float c = cos16(phase) / 32768.0f;
            const float s = sin16(phase) / 32768.0f;
            const uint8_t brightness = uint8_t(255.0f * min(1.0f, visible - i));
            const CRGB color = ColorFromPalette(palette, uint8_t((phase >> 8) + hueOffset),
                                               brightness, LINEARBLEND);
            drawWuVoxel(lroundf(256.0f * (center.x + u.x*c + v.x*s)),
                        lroundf(256.0f * (center.y + u.y*c + v.y*s)),
                        lroundf(256.0f * (center.z + u.z*c + v.z*s)), color);
        }
    }

    // ==========================================
    // 3D WU LINE (Fixed-Point Interpolation)
    // ==========================================
    inline void drawWuLine3D(int32_t x0_fp, int32_t y0_fp, int32_t z0_fp, int32_t x1_fp, int32_t y1_fp, int32_t z1_fp, CRGB col) {
        int32_t dx = x1_fp - x0_fp;
        int32_t dy = y1_fp - y0_fp;
        int32_t dz = z1_fp - z0_fp;
        
        // Calculate step count based on the largest integer-level displacement
        int32_t steps = max(abs(dx), max(abs(dy), abs(dz))) >> 8;

        if (steps == 0) {
            drawWuVoxel(x0_fp, y0_fp, z0_fp, col);
            return;
        }

        int32_t xInc = dx / steps;
        int32_t yInc = dy / steps;
        int32_t zInc = dz / steps;

        int32_t curX = x0_fp;
        int32_t curY = y0_fp;
        int32_t curZ = z0_fp;

        for (int32_t i = 0; i <= steps; i++) {
            drawWuVoxel(curX, curY, curZ, col);
            curX += xInc;
            curY += yInc;
            curZ += zInc;
        }
    }

    // ==========================================
    // 3D SDF LINE (Mathematically Perfect Capsule)
    // ==========================================
    inline void drawSdfLine3D(float x0, float y0, float z0, float x1, float y1, float z1, float radius, CRGB col, bool additive = true) {
        // 1. Line segment vector and squared length
        float dx = x1 - x0;
        float dy = y1 - y0;
        float dz = z1 - z0;
        float l2 = (dx*dx + dy*dy + dz*dz);

        // Precompute the inverse length for fast multiplication later
        float invL2 = (l2 > 0.0001f) ? (1.0f / l2) : 0.0f;

        // 2. AABB CULLING (The Ultimate Optimization)
        // We only check voxels that physically fall within the bounding box of the line.
        // We add 1.2f to the radius to account for the anti-aliasing blur falloff.
        float padding = radius + 1.2f; 
        
        int minX = max(0, (int)floorf(min(x0, x1) - padding));
        int maxX = min((int)RNDR_X - 1, (int)ceilf(max(x0, x1) + padding));
        int minY = max(0, (int)floorf(min(y0, y1) - padding));
        int maxY = min((int)RNDR_Y - 1, (int)ceilf(max(y0, y1) + padding));
        int minZ = max(0, (int)floorf(min(z0, z1) - padding));
        int maxZ = min((int)RNDR_Z - 1, (int)ceilf(max(z0, z1) + padding));

        float radiusSq = radius * radius;
        float paddedRadiusSq = padding * padding; 

        // 3. TARGETED RENDER LOOP
        for (int z = minZ; z <= maxZ; z++) {
            float fz = (float)z;
            float pz_A = fz - z0; // Loop Hoisting: Calculate Z once per layer
            
            for (int y = minY; y <= maxY; y++) {
                float fy = (float)y;
                float py_A = fy - y0; // Loop Hoisting: Calculate Y once per row
                
                for (int x = minX; x <= maxX; x++) {
                    float fx = (float)x;
                    float px_A = fx - x0;

                    // Vector Projection: Find closest point on the line segment
                    float t = 0.0f;
                    if (l2 > 0.0001f) {
                        float dotAP_AB = (px_A * dx) + (py_A * dy) + (pz_A * dz);
                        t = dotAP_AB * invL2;
                        t = max(0.0f, min(1.0f, t)); // Clamp to the ends of the segment
                    }

                    // Coordinates of the closest point on the line
                    float projX = x0 + t * dx;
                    float projY = y0 + t * dy;
                    float projZ = z0 + t * dz;

                    // Calculate squared distance from current voxel to the projection point
                    float dX = fx - projX;
                    float dY = fy - projY;
                    float dZ = fz - projZ;
                    float distSq = dX*dX + dY*dY + dZ*dZ;

                    // 4. DEFERRED SQUARE ROOT
                    if (distSq <= paddedRadiusSq) {
                        float dist = sqrtf(distSq);
                        
                        // SDF Coverage Math:
                        // If dist < radius, intensity is 1.0 (Solid core)
                        // If dist > radius, intensity fades to 0.0 over 1 voxel of distance
                        float intensity = 1.0f - (dist - radius);
                        intensity = max(0.0f, min(1.0f, intensity));
                        
                        if (intensity > 0.01f) {
                            CRGB drawn = col;
                            drawn.nscale8((uint8_t)(intensity * 255.0f));
                            
                            // Curved polylines can use max blending to avoid
                            // repeatedly brightening shared segment endpoints.
                            CRGB existing = getVoxel(x, y, z);
                            setVoxel(x, y, z, additive ? existing + drawn : CRGB(
                                max(existing.r, drawn.r), max(existing.g, drawn.g),
                                max(existing.b, drawn.b)));
                        }
                    }
                }
            }
        }
    }

   inline void drawHyperbolicParaboloid(int offSet, int offsetSign, float rotX, float rotY, float rotZ, const TextureState &tex) {
        float hpOffsetSteps[RNDR_Z];
        float z_max = (float)(RNDR_Z - 1);
        
        for (int z = 0; z < RNDR_Z; z++) {
            float dist_from_center = fabsf((float)z - RNDR_CZ); 
            hpOffsetSteps[z] = (dist_from_center * 2.0f) / z_max; 
        }

        float radX = rotX * 0.0174532925f, radY = rotY * 0.0174532925f, radZ = rotZ * 0.0174532925f;
        float cx = cos(radX), sx = sin(radX), cy = cos(radY), sy = sin(radY), cz = cos(radZ), sz = sin(radZ);

        float y_max = (float)(RNDR_Y - 1);
        float x_max = (float)(RNDR_X - 1);

        for (int z = 0; z < RNDR_Z; z++) {
            float os = (float)(offsetSign * offSet) * hpOffsetSteps[z];
            int yA, yB;

            float z_ratio = (float)z / z_max;
            int y_inv = lroundf((1.0f - z_ratio) * y_max);
            int y_dir = lroundf(z_ratio * y_max);

            if (offsetSign == -1) {
                if (z < RNDR_Z / 2) { yA = y_inv + lroundf(os); yB = y_dir + lroundf(-os); }
                else                { yA = y_inv + lroundf(-os); yB = y_dir + lroundf(os); }
            } else {
                if (z < RNDR_Z / 2) { yA = y_dir + lroundf(os); yB = y_inv + lroundf(-os); }
                else                { yA = y_dir + lroundf(-os); yB = y_inv + lroundf(os); }
            }

            yA = constrain(yA, 0, (int)y_max); 
            yB = constrain(yB, 0, (int)y_max);
            
            float x0 = -RNDR_CX, y0 = (float)yA - RNDR_CY, z0 = (float)z - RNDR_CZ;
            float x1 = x_max - RNDR_CX, y1 = (float)yB - RNDR_CY, z1 = (float)z - RNDR_CZ;

            auto rotatePt = [&](float &px, float &py, float &pz) {
                float ry1 = py * cx - pz * sx, rz1 = py * sx + pz * cx, rx1 = px;
                float rx2 = rx1 * cy + rz1 * sy, rz2 = -rx1 * sy + rz1 * cy, ry2 = ry1;
                px = rx2 * cz - ry2 * sz; py = rx2 * sz + ry2 * cz; pz = rz2;
            };

            rotatePt(x0, y0, z0); rotatePt(x1, y1, z1);
            
            drawFastLine3D((int)lroundf(x0 + RNDR_CX), (int)lroundf(y0 + RNDR_CY), (int)lroundf(z0 + RNDR_CZ),
                           (int)lroundf(x1 + RNDR_CX), (int)lroundf(y1 + RNDR_CY), (int)lroundf(z1 + RNDR_CZ), tex);
        }
    }

    // ==========================================
    // SPHERE
    // ==========================================
    // Visit the bounded volume of a sphere or a horizontally scaled sphere.
    // A coverage sampler can turn the squared distance into a shell, solid,
    // or soft particle without owning another voxel traversal.
    template <typename Plot>
    inline void rasterSphere(Point3 center, float outer, Plot plot,
                             float horizontalScale = 1.0f, bool wrapX = false) {
        const float horizontalOuter = outer * horizontalScale;
        const float inverseScale = 1.0f / horizontalScale;
        const float outerSq = outer * outer;
        const int firstX = int(ceilf(center.x - horizontalOuter));
        const int lastX = int(floorf(center.x + horizontalOuter));
        const int firstY = max(0, int(ceilf(center.y - horizontalOuter)));
        const int lastY = min(RNDR_Y - 1, int(floorf(center.y + horizontalOuter)));
        const int firstZ = max(0, int(ceilf(center.z - outer)));
        const int lastZ = min(RNDR_Z - 1, int(floorf(center.z + outer)));
        for (int x = firstX; x <= lastX; ++x) {
            if (!wrapX && (x < 0 || x >= RNDR_X)) continue;
            const int voxelX = wrapX ? (x % RNDR_X + RNDR_X) % RNDR_X : x;
            const float dx = (x - center.x) * inverseScale;
            for (int y = firstY; y <= lastY; ++y) {
                const float dy = (y - center.y) * inverseScale;
                const float xySq = dx * dx + dy * dy;
                for (int z = firstZ; z <= lastZ; ++z) {
                    const float dz = z - center.z;
                    const float distanceSq = xySq + dz * dz;
                    if (distanceSq < outerSq) plot(voxelX, y, z, distanceSq);
                }
            }
        }
    }

    // Project along Y, feathering each strand at the two surface crossings.
    // This preserves the lateral silhouette of a front-facing sphere mask.
    template <typename VisitColumn>
    inline void rasterDepthFeatheredSphere(float radius, VisitColumn visitColumn) {
        for (int z = 0; z < RNDR_Z; ++z) {
            const float dz = z - RNDR_CZ;
            for (int x = 0; x < RNDR_X; ++x) {
                const float dx = x - RNDR_CX;
                const float spanSq = radius * radius - dx * dx - dz * dz;
                if (spanSq > 0.0f) visitColumn(x, z, sqrtf(spanSq));
            }
        }
    }

    inline float depthFeatheredCoverage(float span, float depth) {
        return constrain(span - fabsf(depth) + 0.5f, 0.0f, 1.0f);
    }

    inline int sphereShell(float distanceSq) {
        return sqrt16(uint16_t(4.0f * distanceSq));
    }

    inline void drawSphere(float cx, float cy, float cz, float radius, RenderMode mode, const TextureState &tex, float rotX, float rotY, float rotZ, bool eraseBackground = true) {
        if (radius < 0) return;
        const Point3 center{int(cx * 2.0f) * 0.5f, int(cy * 2.0f) * 0.5f, int(cz * 2.0f) * 0.5f};
        const int ir = int(radius * 2.0f);
        if (eraseBackground) clearAll();
        rasterSphere(center, ir * 0.5f + 1.0f, [&](int x, int y, int z, float distanceSq) {
            const auto coverage = getCoverage(sphereShell(distanceSq) - ir, 0,
                                              mode == RENDER_WIREFRAME ? RENDER_SHELL : mode);
            if (coverage == 0) return;
            CRGB color = tex.getColor(x, y, z);
            if (coverage < 255) color.nscale8(coverage);
            setVoxel(x, y, z, color);
        });
    }

    // ==========================================
    // OVOID (The Pill / Capsule)
    // ==========================================
    inline void drawOvoid(float cx, float cy, float cz, float radius, RenderMode mode, const TextureState &tex, float rotX, float rotY, float rotZ, bool eraseBackground = true) {
        if (radius < 0) return;
        
        float h = radius * 1.25f; 

        uint16_t angX = (uint32_t)(rotX * 182.0444f), angY = (uint32_t)(rotY * 182.0444f), angZ = (uint32_t)(rotZ * 182.0444f);
        int16_t cxf = cos16(angX), sxf = sin16(angX), cyf = cos16(angY), syf = sin16(angY), czf = cos16(angZ), szf = sin16(angZ);
        
        int32_t cx_fp = (int32_t)(cx * 256.0f), cy_fp = (int32_t)(cy * 256.0f), cz_fp = (int32_t)(cz * 256.0f);

        for (uint8_t x = 0; x < RNDR_X; x++) {
            int16_t x0 = (x * 256) - cx_fp;
            int32_t x0_cyf = (int32_t)x0 * cyf, x0_syf = (int32_t)x0 * syf;
            for (uint8_t y = 0; y < RNDR_Y; y++) {
                int16_t y0 = (y * 256) - cy_fp;
                int32_t y0_cxf = (int32_t)y0 * cxf, y0_sxf = (int32_t)y0 * sxf;
                for (uint8_t z = 0; z < RNDR_Z; z++) {
                    int16_t z0 = (z * 256) - cz_fp;

                    int16_t ry1 = (y0_cxf - (int32_t)z0 * sxf) >> 15;
                    int16_t rz1 = (y0_sxf + (int32_t)z0 * cxf) >> 15;
                    int16_t rx2 = (x0_cyf + (int32_t)rz1 * syf) >> 15;
                    int16_t rz2 = (-x0_syf + (int32_t)rz1 * cyf) >> 15;

                    int16_t rx = (((int32_t)rx2 * czf) - ((int32_t)ry1 * szf)) >> 15;
                    int16_t ry = (((int32_t)rx2 * szf) + ((int32_t)ry1 * czf)) >> 15;
                    int16_t rz = rz2;

                    float fx = (float)rx / 256.0f;
                    float fy = (float)ry / 256.0f;
                    float fz = (float)rz / 256.0f;

                    float pz_clamped = constrain(fz, -h, h);
                    float dz = fz - pz_clamped;
                    float dist3D = sqrtf(fx*fx + fy*fy + dz*dz);
                    
                    int d_surface = (int)roundf((dist3D - radius) * 2.0f);
                    uint8_t coverage = getCoverage(d_surface, 0, mode == RENDER_WIREFRAME ? RENDER_SHELL : mode, 2);

                    if (coverage > 0) {
                        CRGB color = tex.getColor(x, y, z);
                        if (coverage < 255) color.nscale8(coverage);
                        setVoxel(x, y, z, color);
                    } else if (eraseBackground) {
                        setVoxel(x, y, z, CRGB::Black);
                    }
                }
            }
        }
    }

   // ==========================================
    // BOX
    // ==========================================
    inline void drawBox(float cx, float cy, float cz, float radius, RenderMode mode, const TextureState &tex, float rotX, float rotY, float rotZ, bool eraseBackground = true, float aspectZ = 1.0f) {
        if (radius <= 0) return;
        
        float rx = radius;
        float ry = radius;
        float rz = radius * aspectZ; // Native Z-Stretch 

        uint16_t angX = (uint32_t)(rotX * 182.0444f), angY = (uint32_t)(rotY * 182.0444f), angZ = (uint32_t)(rotZ * 182.0444f);
        int16_t cxf = cos16(angX), sxf = sin16(angX), cyf = cos16(angY), syf = sin16(angY), czf = cos16(angZ), szf = sin16(angZ);
        
        int32_t cx_fp = (int32_t)(cx * 256.0f), cy_fp = (int32_t)(cy * 256.0f), cz_fp = (int32_t)(cz * 256.0f);

        for (uint8_t x = 0; x < RNDR_X; x++) {
            int16_t x0 = (x * 256) - cx_fp;
            int32_t x0_cyf = (int32_t)x0 * cyf, x0_syf = (int32_t)x0 * syf;
            for (uint8_t y = 0; y < RNDR_Y; y++) {
                int16_t y0 = (y * 256) - cy_fp;
                int32_t y0_cxf = (int32_t)y0 * cxf, y0_sxf = (int32_t)y0 * sxf;
                for (uint8_t z = 0; z < RNDR_Z; z++) {
                    int16_t z0 = (z * 256) - cz_fp;

                    int16_t ry1 = (y0_cxf - (int32_t)z0 * sxf) >> 15;
                    int16_t rz1 = (y0_sxf + (int32_t)z0 * cxf) >> 15;
                    int16_t rx2 = (x0_cyf + (int32_t)rz1 * syf) >> 15;
                    int16_t rz2 = (-x0_syf + (int32_t)rz1 * cyf) >> 15;

                    int16_t rx_rot = (((int32_t)rx2 * czf) - ((int32_t)ry1 * szf)) >> 15;
                    int16_t ry_rot = (((int32_t)rx2 * szf) + ((int32_t)ry1 * czf)) >> 15;
                    int16_t rz_rot = rz2;

                    // Evaluate physical bounds using the rotated coordinate space
                    float dx = fabsf((float)rx_rot / 256.0f);
                    float dy = fabsf((float)ry_rot / 256.0f);
                    float dz = fabsf((float)rz_rot / 256.0f);

                    float sd_x = dx - rx;
                    float sd_y = dy - ry;
                    float sd_z = dz - rz;

                    float d1 = sd_x, d2 = sd_y, d3 = sd_z, t;
                    // Sort descending to find the primary face and secondary edge distances
                    if (d1 < d2) { t = d1; d1 = d2; d2 = t; }
                    if (d1 < d3) { t = d1; d1 = d3; d3 = t; }
                    if (d2 < d3) { t = d2; d2 = d3; d3 = t; }

                    int d_surface = (int)roundf(d1 * 2.0f);
                    int d_edge = (int)roundf(d2 * 2.0f);

                    uint8_t coverage = getCoverage(d_surface, d_edge, mode, 2);

                    if (coverage > 0) {
                        CRGB color = tex.getColor(x, y, z);
                        if (coverage < 255) color.nscale8(coverage); 
                        setVoxel(x, y, z, color);
                    } else if (eraseBackground) {
                        setVoxel(x, y, z, CRGB::Black);
                    }
                }
            }
        }
    }


    // Rasterize a voxel-aligned box. Callers choose how to shade and
    // composite the covered voxels.
    template <typename Plot>
    inline void rasterFastBox(uint8_t x0, uint8_t y0, uint8_t z0,
                              uint8_t x1, uint8_t y1, uint8_t z1,
                              RenderMode mode, Plot plot) {
        for (uint8_t x = x0; x <= x1; ++x) {
            for (uint8_t y = y0; y <= y1; ++y) {
                for (uint8_t z = z0; z <= z1; ++z) {
                    const uint8_t boundaryHits =
                        (x == x0 || x == x1) + (y == y0 || y == y1) + (z == z0 || z == z1);
                    if (mode == RENDER_SOLID ||
                        (mode == RENDER_SHELL && boundaryHits >= 1) ||
                        (mode == RENDER_WIREFRAME && boundaryHits >= 2)) {
                        plot(x, y, z);
                    }
                }
            }
        }
    }

    inline void drawFastBox(uint8_t x0, uint8_t y0, uint8_t z0, uint8_t x1, uint8_t y1, uint8_t z1, RenderMode mode, const TextureState &tex) {
        rasterFastBox(x0, y0, z0, x1, y1, z1, mode, [&](uint8_t x, uint8_t y, uint8_t z) {
            setVoxel(x, y, z, tex.getColor(x, y, z));
        });
    }

    // A solid cube grows away from its anchor. Coverage multiplies where
    // two or three partially filled faces meet.
    inline void drawCornerCube(int x, int y, int z, bool towardLowX, bool towardLowY,
                               bool towardLowZ, float edge, CRGB color) {
        const int span = int(ceilf(edge)) - 1;
        const int outerX = x + (towardLowX ? -span : span);
        const int outerY = y + (towardLowY ? -span : span);
        const int outerZ = z + (towardLowZ ? -span : span);
        const float partial = edge - span;
        const uint8_t coverage[] = {255, uint8_t(255.0f * partial),
            uint8_t(255.0f * partial * partial), uint8_t(255.0f * partial * partial * partial)};
        rasterFastBox(min(x, outerX), min(y, outerY), min(z, outerZ),
                      max(x, outerX), max(y, outerY), max(z, outerZ), RENDER_SOLID,
                      [&](uint8_t vx, uint8_t vy, uint8_t vz) {
            const uint8_t light = coverage[(vx == outerX) + (vy == outerY) + (vz == outerZ)];
            if (light == 0) return;
            CRGB voxel = color;
            if (light < 255) voxel.nscale8(light);
            setVoxel(vx, vy, vz, voxel);
        });
    }

    template <typename ColorAt>
    inline void paintPlane(uint8_t axis, uint8_t layer, ColorAt colorAt) {
        const uint8_t dimensions[] = {RNDR_X, RNDR_Y, RNDR_Z};
        if (axis >= 3 || layer >= dimensions[axis]) return;
        const uint8_t uAxis = axis == 0 ? 1 : 0;
        const uint8_t vAxis = axis == 2 ? 1 : 2;
        uint8_t position[3];
        position[axis] = layer;
        for (uint8_t u = 0; u < dimensions[uAxis]; ++u) {
            position[uAxis] = u;
            for (uint8_t v = 0; v < dimensions[vAxis]; ++v) {
                position[vAxis] = v;
                setVoxel(position[0], position[1], position[2], colorAt(position[0], position[1], position[2]));
            }
        }
    }

    // Fill a centered horizontal patch with a one-voxel soft boundary.
    // The same coverage can be copied to multiple heights by the caller.
    template <typename Plot>
    inline void rasterHorizontalPatch(float halfWidth, float halfDepth, bool disc, Plot plot) {
        const float radius = min(halfWidth, halfDepth);
        for (int y = 0; y < RNDR_Y; ++y) {
            const float dy = fabsf(y - RNDR_CY);
            const float row = min(1.0f, halfDepth - dy + 0.5f);
            if (row <= 0.0f) continue;
            for (int x = 0; x < RNDR_X; ++x) {
                const float dx = fabsf(x - RNDR_CX);
                const float coverage = disc ? min(1.0f, radius - sqrtf(dx * dx + dy * dy) + 0.5f)
                    : row * min(1.0f, halfWidth - dx + 0.5f);
                if (coverage > 0.0f) plot(x, y, uint8_t(255.0f * coverage));
            }
        }
    }

    // ==========================================
    // TETRAHEDRON (Fast Fixed-Point Z-Scale)
    // ==========================================
    inline void drawTetrahedron(float cx, float cy, float cz, float radius, RenderMode mode, const TextureState &tex, float rotX, float rotY, float rotZ, bool eraseBackground = true, float aspectZ = 1.0f) {
        if (radius < 0) return;
        uint16_t angX = (uint32_t)(rotX * 182.0444f), angY = (uint32_t)(rotY * 182.0444f), angZ = (uint32_t)(rotZ * 182.0444f);
        int16_t cxf = cos16(angX), sxf = sin16(angX), cyf = cos16(angY), syf = sin16(angY), czf = cos16(angZ), szf = sin16(angZ);
        int16_t ir = radius * 256.0f;
        
        int32_t cx_fp = (int32_t)(cx * 256.0f), cy_fp = (int32_t)(cy * 256.0f), cz_fp = (int32_t)(cz * 256.0f);
        
        // Optimize slow division into a fast fixed-point multiplier
        int32_t invAspect_fp = (int32_t)((1.0f / aspectZ) * 256.0f);

        for (uint8_t x = 0; x < RNDR_X; x++) {
            int16_t x0 = (x * 256) - cx_fp;
            int32_t x0_cyf = (int32_t)x0 * cyf, x0_syf = (int32_t)x0 * syf;
            for (uint8_t y = 0; y < RNDR_Y; y++) {
                int16_t y0 = (y * 256) - cy_fp;
                int32_t y0_cxf = (int32_t)y0 * cxf, y0_sxf = (int32_t)y0 * sxf;
                for (uint8_t z = 0; z < RNDR_Z; z++) {
                    int16_t z0 = (z * 256) - cz_fp;
                    
                    // Fast optional stretch
                    if (aspectZ != 1.0f) z0 = (int16_t)((z0 * invAspect_fp) >> 8); 

                    int16_t ry1 = (y0_cxf - (int32_t)z0 * sxf) >> 15;
                    int16_t rz1 = (y0_sxf + (int32_t)z0 * cxf) >> 15;
                    int16_t rx2 = (x0_cyf + (int32_t)rz1 * syf) >> 15;
                    int16_t rz2 = (-x0_syf + (int32_t)rz1 * cyf) >> 15;

                    int16_t rx = (((int32_t)rx2 * czf) - ((int32_t)ry1 * szf)) >> 15;
                    int16_t ry = (((int32_t)rx2 * szf) + ((int32_t)ry1 * czf)) >> 15;
                    int16_t rz = rz2;

                    int16_t v[4] = {(int16_t)(-rx - ry - rz), (int16_t)(-rx + ry + rz), (int16_t)(rx - ry + rz), (int16_t)(rx + ry - rz)};
                    int16_t max1 = -32767, max2 = -32767;
                    for (int i = 0; i < 4; i++) {
                        if (v[i] > max1) { max2 = max1; max1 = v[i]; }
                        else if (v[i] > max2) { max2 = v[i]; }
                    }

                    int d_surface = (max1 - ir) / 128;
                    int d_edge = (max2 - ir) / 128;
                   
                    //uint8_t coverage = getCoverage(d_surface, d_edge, mode, 2);
                    // Safely soften the wireframe aliasing by widening the blur kernel when stretched
                    int blurRadius = (aspectZ > 1.2f && mode == RENDER_WIREFRAME) ? 3 : 2;
                    uint8_t coverage = getCoverage(d_surface, d_edge, mode, blurRadius);

                    if (coverage > 0) {
                        CRGB color = tex.getColor(x, y, z);
                        if (coverage < 255) color.nscale8(coverage);
                        setVoxel(x, y, z, color);
                    } else if (eraseBackground) {
                        setVoxel(x, y, z, CRGB::Black);
                    }
                }
            }
        }
    }

    // ==========================================
    // OCTAHEDRON (Fast Fixed-Point Z-Scale)
    // ==========================================
    inline void drawOctahedron(float cx, float cy, float cz, float radius, RenderMode mode, const TextureState &tex, float rotX, float rotY, float rotZ, bool eraseBackground = true, float aspectZ = 1.0f) {
        if (radius < 0) return;
        uint16_t angX = (uint32_t)(rotX * 182.0444f), angY = (uint32_t)(rotY * 182.0444f), angZ = (uint32_t)(rotZ * 182.0444f);
        int16_t cxf = cos16(angX), sxf = sin16(angX), cyf = cos16(angY), syf = sin16(angY), czf = cos16(angZ), szf = sin16(angZ);
        int16_t ir = radius * 256.0f;
        
        int32_t cx_fp = (int32_t)(cx * 256.0f), cy_fp = (int32_t)(cy * 256.0f), cz_fp = (int32_t)(cz * 256.0f);
        
        // Optimize slow division into a fast fixed-point multiplier
        int32_t invAspect_fp = (int32_t)((1.0f / aspectZ) * 256.0f);

        for (uint8_t x = 0; x < RNDR_X; x++) {
            int16_t x0 = (x * 256) - cx_fp;
            int32_t x0_cyf = (int32_t)x0 * cyf, x0_syf = (int32_t)x0 * syf;
            for (uint8_t y = 0; y < RNDR_Y; y++) {
                int16_t y0 = (y * 256) - cy_fp;
                int32_t y0_cxf = (int32_t)y0 * cxf, y0_sxf = (int32_t)y0 * sxf;
                for (uint8_t z = 0; z < RNDR_Z; z++) {
                    int16_t z0 = (z * 256) - cz_fp;
                    
                    // Fast optional stretch
                    if (aspectZ != 1.0f) z0 = (int16_t)((z0 * invAspect_fp) >> 8);

                    int16_t ry1 = (y0_cxf - (int32_t)z0 * sxf) >> 15;
                    int16_t rz1 = (y0_sxf + (int32_t)z0 * cxf) >> 15;
                    int16_t rx2 = (x0_cyf + (int32_t)rz1 * syf) >> 15;
                    int16_t rz2 = (-x0_syf + (int32_t)rz1 * cyf) >> 15;

                    int16_t rx = (((int32_t)rx2 * czf) - ((int32_t)ry1 * szf)) >> 15;
                    int16_t ry = (((int32_t)rx2 * szf) + ((int32_t)ry1 * czf)) >> 15;
                    int16_t rz = rz2;

                    int16_t abs_rx = abs(rx), abs_ry = abs(ry), abs_rz = abs(rz);
                    int d_surface = ((abs_rx + abs_ry + abs_rz) - ir) / 128;
                    int d_edge = min(abs_rx, min(abs_ry, abs_rz)) / 128;

                    //uint8_t coverage = getCoverage(d_surface, d_edge, mode, 2);
                    // Safely soften the wireframe aliasing by widening the blur kernel when stretched
                    int blurRadius = (aspectZ > 1.2f && mode == RENDER_WIREFRAME) ? 3 : 2;
                    uint8_t coverage = getCoverage(d_surface, d_edge, mode, blurRadius);

                    if (coverage > 0) {
                        CRGB color;
                        if (tex.mode == MODE_SOLID && tex.palette[0] == CRGB(CRGB::Gray)) {
                            color = (rx > 0) ? tex.palette[1] : tex.palette[2]; 
                        } else {
                            color = tex.getColor(x, y, z);
                        }
                        if (coverage < 255) color.nscale8(coverage); 
                        setVoxel(x, y, z, color);
                    } else if (eraseBackground) {
                        setVoxel(x, y, z, CRGB::Black);
                    }
                } 
            } 
        } 
    }

    // ==========================================
    // MERKABA (Dynamically Centered)
    // ==========================================
    inline void drawMerkaba(float cx, float cy, float cz, float radius, RenderMode mode, const TextureState& tex, float rotX, float rotY, float rotZ, bool eraseBackground = true) {
        if (radius <= 0) return; 
        uint16_t angX = (uint32_t)(rotX * 182.0444f), angY = (uint32_t)(rotY * 182.0444f), angZ = (uint32_t)(rotZ * 182.0444f);
        int16_t cxf = cos16(angX), sxf = sin16(angX), cyf = cos16(angY), syf = sin16(angY), czf = cos16(angZ), szf = sin16(angZ);
        int16_t ir = (int16_t)(radius * 256.0f); 
        const int16_t LINE_THICKNESS = 400;  
        
        int32_t cx_fp = (int32_t)(cx * 256.0f), cy_fp = (int32_t)(cy * 256.0f), cz_fp = (int32_t)(cz * 256.0f);

        for (uint8_t x = 0; x < RNDR_X; x++) {
            int16_t x0 = (x * 256) - cx_fp;
            int32_t x0_cyf = (int32_t)x0 * cyf, x0_syf = (int32_t)x0 * syf;
            for (uint8_t y = 0; y < RNDR_Y; y++) {
                int16_t y0 = (y * 256) - cy_fp;
                int32_t y0_cxf = (int32_t)y0 * cxf, y0_sxf = (int32_t)y0 * sxf;
                for (uint8_t z = 0; z < RNDR_Z; z++) {
                    int16_t z0 = (z * 256) - cz_fp;

                    int16_t ry1 = (y0_cxf - (int32_t)z0 * sxf) >> 15;
                    int16_t rz1 = (y0_sxf + (int32_t)z0 * cxf) >> 15;
                    int16_t rx2 = (x0_cyf + (int32_t)rz1 * syf) >> 15;
                    int16_t rz2 = (-x0_syf + (int32_t)rz1 * cyf) >> 15;

                    int16_t rx = (((int32_t)rx2 * czf) - ((int32_t)ry1 * szf)) >> 15;
                    int16_t ry = (((int32_t)rx2 * szf) + ((int32_t)ry1 * czf)) >> 15;
                    int16_t rz = rz2;

                    int16_t d1 = max4(-rx-ry-rz, -rx+ry+rz, rx-ry+rz, rx+ry-rz) - ir;
                    int16_t d2 = max4(rx+ry+rz, rx-ry-rz, -rx+ry-rz, -rx-ry+rz) - ir;

                    int16_t d_union = min(d1, d2);
                    int16_t abs_d = abs(d_union);

                    if (abs_d < LINE_THICKNESS) {
                        uint8_t brightness = map(abs_d, 0, LINE_THICKNESS, 255, 0); 
                        CRGB color;
                        if (tex.mode == MODE_SOLID && tex.palette[0] == CRGB(CRGB::White)) {
                            uint8_t octant = ((rx > 0) << 2) | ((ry > 0) << 1) | (rz > 0);
                            uint8_t pair = (octant < 4) ? octant : (~octant & 0x07);
                            CRGB pairColors[4] = { tex.palette[1], tex.palette[2], tex.palette[3], tex.palette[4] };
                            color = pairColors[pair];
                        } 
                        else if (tex.mode == MODE_SOLID && tex.palette[0] == CRGB(CRGB::Gray)) {
                            color = (d1 < d2) ? tex.palette[1] : tex.palette[2];
                        } else {
                            color = tex.getColor(x, y, z);
                        }

                        color.nscale8(brightness); 
                        setVoxel(x, y, z, color);
                    } else if (eraseBackground) {
                        setVoxel(x, y, z, CRGB::Black);
                    }
                }
            }
        }
    }

    // ==========================================
    // TORUS (Dynamically Centered)
    // ==========================================
  inline void drawTorus(float cx, float cy, float cz, float radius, RenderMode mode, const TextureState &tex, float rotX, float rotY, float rotZ, bool eraseBackground = true) {
    if (radius <= 0) return;
    uint16_t angX = (uint32_t)(rotX * 182.0444f), angY = (uint32_t)(rotY * 182.0444f), angZ = (uint32_t)(rotZ * 182.0444f);
    int16_t cxf = cos16(angX), sxf = sin16(angX), cyf = cos16(angY), syf = sin16(angY), czf = cos16(angZ), szf = sin16(angZ);
    
    int32_t cx_fp = (int32_t)(cx * 256.0f), cy_fp = (int32_t)(cy * 256.0f), cz_fp = (int32_t)(cz * 256.0f);
    float tube_radius = 1.5f;

    for (uint8_t x = 0; x < RNDR_X; x++) {
        int16_t x0 = (x * 256) - cx_fp;
        int32_t x0_cyf = (int32_t)x0 * cyf, x0_syf = (int32_t)x0 * syf;
        for (uint8_t y = 0; y < RNDR_Y; y++) {
            int16_t y0 = (y * 256) - cy_fp;
            int32_t y0_cxf = (int32_t)y0 * cxf, y0_sxf = (int32_t)y0 * sxf;
            for (uint8_t z = 0; z < RNDR_Z; z++) {
                int16_t z0 = (z * 256) - cz_fp;

                int16_t ry1 = (y0_cxf - (int32_t)z0 * sxf) >> 15;
                int16_t rz1 = (y0_sxf + (int32_t)z0 * cxf) >> 15;
                int16_t rx2 = (x0_cyf + (int32_t)rz1 * syf) >> 15;
                int16_t rz2 = (-x0_syf + (int32_t)rz1 * cyf) >> 15;

                int16_t rx = (((int32_t)rx2 * czf) - ((int32_t)ry1 * szf)) >> 15;
                int16_t ry = (((int32_t)rx2 * szf) + ((int32_t)ry1 * czf)) >> 15;
                int16_t rz = rz2;

                float fx = (float)rx / 256.0f;
                float fy = (float)ry / 256.0f;
                float fz = (float)rz / 256.0f;

                float dist2D = sqrtf(fx*fx + fz*fz); 
                float q = dist2D - radius;
                float dist3D = sqrtf(q*q + fy*fy); 
                
                int d_surface = (int)roundf((dist3D - tube_radius) * 2.0f);
                uint8_t coverage = getCoverage(d_surface, d_surface, mode == RENDER_WIREFRAME ? RENDER_SHELL : mode, 2);

                if (coverage > 0) {
                    CRGB color = tex.getColor(x, y, z);
                    if (coverage < 255) color.nscale8(coverage);
                    setVoxel(x, y, z, color);
                } else if (eraseBackground) {
                    setVoxel(x, y, z, CRGB::Black);
                }
            }
        }
    }
}

   // ==========================================
    // CYLINDER (Dynamically Centered - Solid Color Hack)
    // ==========================================
    inline void drawCylinder(float cx, float cy, float cz, float radius, RenderMode mode, const TextureState &tex, float rotX, float rotY, float rotZ, bool eraseBackground = true, float = 1.0f) {
        if (radius <= 0) return;
        uint16_t angX = (uint32_t)(rotX * 182.0444f), angY = (uint32_t)(rotY * 182.0444f), angZ = (uint32_t)(rotZ * 182.0444f);
        int16_t cxf = cos16(angX), sxf = sin16(angX), cyf = cos16(angY), syf = sin16(angY), czf = cos16(angZ), szf = sin16(angZ);
        
        float half_height = (RNDR_Z / 2.0f) - 0.5f; 
        float thickness_radius = radius * 0.707106f; 
        
        int32_t cx_fp = (int32_t)(cx * 256.0f), cy_fp = (int32_t)(cy * 256.0f), cz_fp = (int32_t)(cz * 256.0f);

        for (uint8_t x = 0; x < RNDR_X; x++) {
            int16_t x0 = (x * 256) - cx_fp;
            int32_t x0_cyf = (int32_t)x0 * cyf, x0_syf = (int32_t)x0 * syf;
            for (uint8_t y = 0; y < RNDR_Y; y++) {
                int16_t y0 = (y * 256) - cy_fp;
                int32_t y0_cxf = (int32_t)y0 * cxf, y0_sxf = (int32_t)y0 * sxf;
                for (uint8_t z = 0; z < RNDR_Z; z++) {
                    int16_t z0 = (z * 256) - cz_fp;

                    int16_t ry1 = (y0_cxf - (int32_t)z0 * sxf) >> 15;
                    int16_t rz1 = (y0_sxf + (int32_t)z0 * cxf) >> 15;
                    int16_t rx2 = (x0_cyf + (int32_t)rz1 * syf) >> 15;
                    int16_t rz2 = (-x0_syf + (int32_t)rz1 * cyf) >> 15;

                    int16_t rx = (((int32_t)rx2 * czf) - ((int32_t)ry1 * szf)) >> 15;
                    int16_t ry = (((int32_t)rx2 * szf) + ((int32_t)ry1 * czf)) >> 15;
                    int16_t rz = rz2;

                    float fx = (float)rx / 256.0f;
                    float fy = (float)ry / 256.0f;
                    float fz = (float)rz / 256.0f;

                    float dist2D = sqrtf(fx*fx + fy*fy);
                    float d_body = dist2D - thickness_radius;
                    float d_cap = fabsf(fz) - half_height;
                    
                    int d_surface = (int)roundf(max(d_body, d_cap) * 2.0f);
                    int d_edge = (int)roundf(min(d_body, d_cap) * 2.0f);
                    
                    uint8_t coverage = getCoverage(d_surface, d_edge, mode, 2);

                    if (coverage > 0) {
                        CRGB color;
                        if (tex.cylinderCaps) {
                            const TextureState &material = d_cap > d_body ? *tex.cylinderCaps : tex;
                            color = material.getColor(x, y, z);
                        } else if (tex.mode == MODE_SOLID && tex.palette[0] == CRGB(CRGB::Gray)) {
                            bool isEndCap = (d_cap > d_body);
                            bool isMatrixBoundary = (x == 0 || x == RNDR_X - 1 || y == 0 || y == RNDR_Y - 1 || z == 0 || z == RNDR_Z - 1);
                            color = (isEndCap || isMatrixBoundary) ? tex.palette[1] : tex.palette[2];
                        } else {
                            color = tex.getColor(x, y, z);
                        }
                        if (coverage < 255) color.nscale8(coverage);
                        setVoxel(x, y, z, color);
                    } else if (eraseBackground) {
                        setVoxel(x, y, z, CRGB::Black);
                    }
                }
            }
        }
    }

    // ==========================================
    // HEMISPHERE HOURGLASS (Dynamically Centered)
    // ==========================================
   inline void drawHemisphereHourglass(float cx, float cy, float cz, float radius, RenderMode mode, const TextureState &tex, float rotX, float rotY, float rotZ, bool eraseBackground = true) {
    if (radius <= 0) return;
    uint16_t angX = (uint32_t)(rotX * 182.0444f), angY = (uint32_t)(rotY * 182.0444f), angZ = (uint32_t)(rotZ * 182.0444f);
    int16_t cxf = cos16(angX), sxf = sin16(angX), cyf = cos16(angY), syf = sin16(angY), czf = cos16(angZ), szf = sin16(angZ);
    
    float max_radius = radius; 
    int32_t cx_fp = (int32_t)(cx * 256.0f), cy_fp = (int32_t)(cy * 256.0f), cz_fp = (int32_t)(cz * 256.0f);

    for (uint8_t x = 0; x < RNDR_X; x++) {
        int16_t x0 = (x * 256) - cx_fp;
        int32_t x0_cyf = (int32_t)x0 * cyf, x0_syf = (int32_t)x0 * syf;
        for (uint8_t y = 0; y < RNDR_Y; y++) {
            int16_t y0 = (y * 256) - cy_fp;
            int32_t y0_cxf = (int32_t)y0 * cxf, y0_sxf = (int32_t)y0 * sxf;
            for (uint8_t z = 0; z < RNDR_Z; z++) {
                int16_t z0 = (z * 256) - cz_fp;

                int16_t ry1 = (y0_cxf - (int32_t)z0 * sxf) >> 15;
                int16_t rz1 = (y0_sxf + (int32_t)z0 * cxf) >> 15;
                int16_t rx2 = (x0_cyf + (int32_t)rz1 * syf) >> 15;
                int16_t rz2 = (-x0_syf + (int32_t)rz1 * cyf) >> 15;

                int16_t rx = (((int32_t)rx2 * czf) - ((int32_t)ry1 * szf)) >> 15;
                int16_t ry = (((int32_t)rx2 * szf) + ((int32_t)ry1 * czf)) >> 15;
                int16_t rz = rz2;

                float fx = (float)rx / 256.0f;
                float fy = (float)ry / 256.0f;
                float fz = (float)rz / 256.0f;

                float sphere_center_z = (fz > 0) ? max_radius : -max_radius;
                float dz = fz - sphere_center_z;
                
                float dist3D = sqrtf(fx*fx + fy*fy + dz*dz);
                
                float d_sphere = dist3D - max_radius; 
                float d_cap = fabsf(fz) - max_radius;   
                
                //int d_surface = (int)roundf(max(d_sphere, d_cap) * 2.0f);
                //int d_edge = (int)roundf(min(fabsf(d_sphere), fabsf(d_cap)) * 2.0f); 
                int d_surface = (int)roundf(max(d_sphere, d_cap) * 2.0f);
                int d_edge = (int)roundf(min(d_sphere, d_cap) * 2.0f);
                
                uint8_t coverage = getCoverage(d_surface, d_edge, mode, 2);

               if (coverage > 0) {
                    CRGB color;
                   if (tex.mode == MODE_SOLID && tex.palette[0] == CRGB(CRGB::Gray)) {
                        if (d_cap > d_sphere) {
                            color = (fz > 0) ? tex.palette[1] : tex.palette[2]; // The Caps
                        } else {
                            color = tex.palette[3]; // The Body uses the 3rd Accent Color!
                        }
                    } else {
                        color = tex.getColor(x, y, z);
                    }
                    
                    if (coverage < 255) color.nscale8(coverage);
                    setVoxel(x, y, z, color);
                } else if (eraseBackground) {
                    setVoxel(x, y, z, CRGB::Black);
                }
            }
        }
    }
}
// ==========================================
    // HOURGLASS (Opposing Cones - Solid/Shell Only)
    // ==========================================
    inline void drawHourglass(float cx, float cy, float cz, float maxRadius, RenderMode mode, const TextureState &tex, float rotX, float rotY, float rotZ, bool eraseBackground = true, float aspectZ = 1.0f) {
        if (maxRadius <= 0) return;
        
        uint16_t angX = (uint32_t)(rotX * 182.0444f), angY = (uint32_t)(rotY * 182.0444f), angZ = (uint32_t)(rotZ * 182.0444f);
        int16_t cxf = cos16(angX), sxf = sin16(angX), cyf = cos16(angY), syf = sin16(angY), czf = cos16(angZ), szf = sin16(angZ);
        
        float half_height = (RNDR_Z / 2.0f) - 0.5f; 
        
        int32_t cx_fp = (int32_t)(cx * 256.0f), cy_fp = (int32_t)(cy * 256.0f), cz_fp = (int32_t)(cz * 256.0f);

        for (uint8_t x = 0; x < RNDR_X; x++) {
            int16_t x0 = (x * 256) - cx_fp;
            int32_t x0_cyf = (int32_t)x0 * cyf, x0_syf = (int32_t)x0 * syf;
            for (uint8_t y = 0; y < RNDR_Y; y++) {
                int16_t y0 = (y * 256) - cy_fp;
                int32_t y0_cxf = (int32_t)y0 * cxf, y0_sxf = (int32_t)y0 * sxf;
                for (uint8_t z = 0; z < RNDR_Z; z++) {
                    int16_t z0 = (z * 256) - cz_fp;

                    int16_t ry1 = (y0_cxf - (int32_t)z0 * sxf) >> 15;
                    int16_t rz1 = (y0_sxf + (int32_t)z0 * cxf) >> 15;
                    int16_t rx2 = (x0_cyf + (int32_t)rz1 * syf) >> 15;
                    int16_t rz2 = (-x0_syf + (int32_t)rz1 * cyf) >> 15;

                    int16_t rx = (((int32_t)rx2 * czf) - ((int32_t)ry1 * szf)) >> 15;
                    int16_t ry = (((int32_t)rx2 * szf) + ((int32_t)ry1 * czf)) >> 15;
                    int16_t rz = rz2;

                    float fx = (float)rx / 256.0f;
                    float fy = (float)ry / 256.0f;
                    float fz = (float)rz / 256.0f;

                    float absZ = fabsf(fz);
                    
                    // 1. THE DRAMATIC POINT TAPER MATH
                    // A pure linear cone creates a fat "4-high" cylinder in the center due to matrix rasterization.
                    // We blend 70% linear with 30% square root. This physically forces the radius to 
                    // jump out immediately at layer 6 and 9, creating a razor-sharp 2-layer high point.
                    float normZ = absZ / half_height;
                    float targetRadius = maxRadius * ((normZ * 0.7f) + (sqrtf(normZ) * 0.3f));
                    
                    float dist2D = sqrtf(fx*fx + fy*fy);
                    float d_body = dist2D - targetRadius;
                    float d_cap = absZ - half_height; 
                    
                    int d_surface = (int)roundf(max(d_body, d_cap) * 2.0f);
                    
                    // Wireframe math safely bypassed, pass 0 to edge
                    uint8_t coverage = getCoverage(d_surface, 0, mode, 2);

                    if (coverage > 0) {
                        CRGB color;
                        if (tex.mode == MODE_SOLID && tex.palette[0] == CRGB(CRGB::Gray)) {
                            color = (fz > 0) ? tex.palette[1] : tex.palette[2];
                        } else {
                            color = tex.getColor(x, y, z);
                        }
    
                        if (coverage < 255) color.nscale8(coverage);
                        setVoxel(x, y, z, color);
                    } else if (eraseBackground) {
                        setVoxel(x, y, z, CRGB::Black);
                    }
                }
            }
        }
    }
    // ==========================================
    // SINE WAVES
    // ==========================================
    inline float sineWaveHeight(float distance, float angleScale, uint16_t phase, float amplitude) {
        const uint16_t angle = uint16_t(int32_t(distance * angleScale));
        return amplitude * float(sin16(angle - phase)) / 32767.0f;
    }

    inline void drawSineWave(float amplitude, int periodDegrees, int phaseShiftDegrees, float verticalShift, float uShift, float vShift, int orientation, WaveShape shape, const TextureState &tex) {
        float angleScale = (360.0f / (float)periodDegrees) * 4096.0f;
        uint16_t phaseShift16 = (uint16_t)(((uint32_t)phaseShiftDegrees * 65536) / 360);

        uint8_t limitU, limitV, limitW;
        if (orientation == 2) { limitU = RNDR_X; limitV = RNDR_Y; limitW = RNDR_Z; }      // AXIS_Z
        else if (orientation == 1) { limitU = RNDR_X; limitV = RNDR_Z; limitW = RNDR_Y; } // AXIS_Y
        else { limitU = RNDR_Y; limitV = RNDR_Z; limitW = RNDR_X; }                       // AXIS_X

        for (uint8_t u = 0; u < limitU; u++) {
            for (uint8_t v = 0; v < limitV; v++) {
                float fx = (float)u + uShift; 
                float fy = (float)v + vShift; 
                float dist = (shape == WAVE_RADIAL) ? sqrtf((fx * fx) + (fy * fy)) : fx;

                int w = (int)roundf(sineWaveHeight(dist, angleScale, phaseShift16, amplitude) + verticalShift);

                if (w >= 0 && w < limitW) {
                    uint8_t finalX, finalY, finalZ;
                    
                    if (orientation == 2) { finalX = u; finalY = v; finalZ = w; }
                    else if (orientation == 1) { finalX = u; finalY = w; finalZ = v; }
                    else { finalX = w; finalY = u; finalZ = v; }

                    setVoxel(finalX, finalY, finalZ, tex.getColor(finalX, finalY, finalZ));
                }
            }
        }
    }

    // ==========================================
    // THE 2026 SPLIT-CUBE: UNIVERSAL PUDDLE ENGINE
    // ==========================================
#ifdef TARGET_2026_CUBOID
    inline void drawSineWaveSplit2026(float passedAmplitude, int periodDegrees, uint16_t phaseBot, uint16_t phaseTop, uint8_t axisBot, uint8_t axisTop, bool invertBot, bool invertTop, const TextureState &tex) {
        float angleScale = (360.0f / (float)periodDegrees) * 4096.0f;
        uint8_t halfZ = RNDR_Z / 2;

        // A reusable lambda to calculate exact bounds and amplitudes per-axis
        auto drawHalf = [&](uint16_t phase, uint8_t axis, bool invert, uint8_t zOffset) {
            uint8_t limU, limV, limW;
            float cU, cV, cW;
            
            if (axis == 0) { // Oscillates along X (U=Y, V=Local Z, W=X)
                limU = RNDR_Y; limV = halfZ; limW = RNDR_X;
                cU = RNDR_CY - 0.5f; cV = (halfZ / 2.0f) - 0.5f; cW = RNDR_CX - 0.5f;
            } else if (axis == 1) { // Oscillates along Y (U=X, V=Local Z, W=Y)
                limU = RNDR_X; limV = halfZ; limW = RNDR_Y;
                cU = RNDR_CX - 0.5f; cV = (halfZ / 2.0f) - 0.5f; cW = RNDR_CY - 0.5f;
            } else { // Oscillates along Local Z (U=X, V=Y, W=Local Z)
                limU = RNDR_X; limV = RNDR_Y; limW = halfZ;
                cU = RNDR_CX - 0.5f; cV = RNDR_CY - 0.5f; cW = (halfZ / 2.0f) - 0.5f;
            }

            // Completely ignore the passed-in amplitude and calculate perfect edge-to-edge math
            float amp = cW;

            for (uint8_t u = 0; u < limU; u++) {
                for (uint8_t v = 0; v < limV; v++) {
                    float fu = (float)u - cU;
                    float fv = (float)v - cV;
                    float dist = sqrtf(fu*fu + fv*fv); 
                    
                    int16_t s = sin16((uint16_t)(dist * angleScale) - phase);
                    int w = (int)roundf(((amp * (float)s) / 32767.0f) + cW);
                    
                    if (invert) w = (limW - 1) - w; 

                    uint8_t x, y, z;
                    if (axis == 0) { x = w; y = u; z = v; }      
                    else if (axis == 1) { x = u; y = w; z = v; } 
                    else { x = u; y = v; z = w; }                   
                    
                    // Shift the local Z up to the physical Top half if required
                    z += zOffset; 
                    
                    // Final safety clamp before rendering
                    if (x < RNDR_X && y < RNDR_Y && z >= zOffset && z < zOffset + halfZ) {
                        setVoxel(x, y, z, tex.getColor(x, y, z));
                    }
                }
            }
        };

        drawHalf(phaseBot, axisBot, invertBot, 0);       // Draw Bottom
        drawHalf(phaseTop, axisTop, invertTop, halfZ);   // Draw Top
    }
#endif

}
