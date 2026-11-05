#pragma once
#include "CubeCore.h"
#include "TextureEngine.h"
#include <vector>
constexpr int RENDER_SHELL = 1;
namespace GeometryEngine {
    // Host dispatch spy. Rasterization remains the existing production engine;
    // these records verify the new routine's origin, radius and shape dispatch.
    struct GravityCall { unsigned shape; float x, y, z, radius; int mode; ShaderMode shader; CRGBPalette16 palette; };
    static std::vector<GravityCall> gravityCalls;
    inline void record(unsigned shape, float x, float y, float z, float radius,
                       int mode, const TextureState &tex) {
        gravityCalls.push_back({shape, x, y, z, radius, mode, tex.mode, tex.palette});
        setVoxel(uint8_t(x), uint8_t(y), uint8_t(z), tex.getColor(0, 0, 0));
    }
    inline void drawSphere(float x, float y, float z, float radius, int mode,
                           const TextureState &tex, float, float, float) {
        record(0, x, y, z, radius, mode, tex);
    }
    inline void drawBox(float x, float y, float z, float radius, int mode,
                        const TextureState &tex, float, float, float) {
        record(1, x, y, z, radius, mode, tex);
    }
    inline void drawMerkaba(float x, float y, float z, float radius, int mode,
                            const TextureState &tex, float, float, float) {
        record(2, x, y, z, radius, mode, tex);
    }
}
