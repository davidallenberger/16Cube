#include "Audio.h"
#include "GeometryEngine.h"
#include "Shapes.h"

namespace {
    constexpr uint32_t cubeColorPeriodMs = 6000;
    constexpr uint32_t cubeColorFadeMs = 600;
    constexpr uint32_t rainbowColors[] = {
        0xFF0000, 0xFF8000, 0xFFFF00, 0x00FF00, 0x0000FF, 0x4B0082, 0x9400D3
    };
    constexpr uint8_t rainbowColorCount = sizeof(rainbowColors) / sizeof(rainbowColors[0]);

    uint8_t nextRainbowColor(uint8_t previous) {
        return (previous + random8(1, rainbowColorCount)) % rainbowColorCount;
    }

    // The shape renderer integrates the rate and preserves orientation as
    // BPM moves. Keep the idle and 120 BPM anchors and open up the fast end.
    float shapeRateForBpm(float bpm) {
        if (bpm < 80.0f) return 0.1f + 0.6f * constrain((bpm - 12.0f) / 68.0f, 0.0f, 1.0f);
        if (bpm < 120.0f) return 1.0f + (bpm - 120.0f) * 0.0075f;
        return min(3.0f, 1.0f + (bpm - 120.0f) * 0.05f);
    }

    void changeOctahedronColors(TextureState &texture) {
        const CRGB first = texture.palette[1];
        const CRGB second = texture.palette[2];
        do {
            texture.palette = PaletteUtils::getDynamicContrastPalette(CRGB::Gray);
        } while (texture.palette[1] == first || texture.palette[2] == second);
    }
}

struct Audio::VolumeCubes {
    const int maxEdge;
    uint8_t targetColor = random8(rainbowColorCount);
    uint8_t previousColor = targetColor;
    uint32_t colorStartedMs = millis();
    CRGB color{rainbowColors[targetColor]};
    float visibleVolume = 0.0f;
    float edge = 0.0f;

    explicit VolumeCubes(int maxEdge) : maxEdge(maxEdge) {}

    void update(uint8_t volume, uint32_t deltaMs) {
        const int fullVolume = max(1, int(controls().shapeVolumeForFullCube));
        const float retained = expf(-float(deltaMs) / cubeSizeReleaseMs);
        visibleVolume = trackShapeSize(visibleVolume, min(int(volume), fullVolume), retained);
        edge = maxEdge * cbrtf(visibleVolume / fullVolume);

        const uint32_t now = millis();
        const uint32_t age = now - colorStartedMs;
        if (age >= cubeColorPeriodMs) {
            previousColor = targetColor;
            targetColor = nextRainbowColor(targetColor);
            colorStartedMs += age - age % cubeColorPeriodMs;
        }
        const float progress = min(1.0f, float(now - colorStartedMs) / cubeColorFadeMs);
        const float eased = progress * progress * (3.0f - 2.0f * progress);
        color = blend(CRGB(rainbowColors[previousColor]), CRGB(rainbowColors[targetColor]),
                      uint8_t(255.0f * eased));
    }

    void draw() const {
        if (edge <= 0.0f) return;
        // Grow from the innermost voxel of each corner block toward its three outer faces.
        const int inner = maxEdge - 1;
        for (unsigned corner = 0; corner < 8; ++corner) {
            const bool lowX = !(corner & 1), lowY = !(corner & 2), lowZ = !(corner & 4);
            GeometryEngine::drawCornerCube(lowX ? inner : RNDR_X - maxEdge,
                                           lowY ? inner : RNDR_Y - maxEdge,
                                           lowZ ? inner : RNDR_Z - maxEdge,
                                           lowX, lowY, lowZ, edge, color);
        }
    }
};

struct Audio::CylinderResponse {
    VolumeCubes cubes{3};
    TextureState caps{MODE_SOLID};
    uint8_t bodyColor = random8(rainbowColorCount);
    uint8_t capColor = nextRainbowColor(bodyColor);
    uint32_t kicks = 0;

    static float update(TextureState &texture, uint32_t deltaMs, void *context) {
        auto &response = *static_cast<CylinderResponse *>(context);
        if (!texture.cylinderCaps) {
            texture.cylinderCaps = &response.caps;
            texture.palette[0] = CRGB(rainbowColors[response.bodyColor]);
            response.caps.palette[0] = CRGB(rainbowColors[response.capColor]);
        }

        const auto audio = RemoteInputs::audio();
        if (audio.dedicatedBassKick) {
            ++response.kicks;
            response.bodyColor = nextRainbowColor(response.bodyColor);
            const uint8_t previousCap = response.capColor;
            do {
                response.capColor = nextRainbowColor(previousCap);
            } while (response.capColor == response.bodyColor);
            texture.palette[0] = CRGB(rainbowColors[response.bodyColor]);
            response.caps.palette[0] = CRGB(rainbowColors[response.capColor]);
        }
        response.cubes.update(audio.volumeSm, deltaMs);

        const float rate = shapeRateForBpm(audio.bpm);
        char detail[128];
        snprintf(detail, sizeof(detail),
                 "Cylinder rate=%.2fx kicks=%lu body=#%06lX caps=#%06lX",
                 double(rate), (unsigned long)response.kicks,
                 (unsigned long)rainbowColors[response.bodyColor],
                 (unsigned long)rainbowColors[response.capColor]);
        debug(audio, detail, audio.dedicatedBassKick ? response.kicks : 0);
        return rate;
    }

    static void drawCubes(void *context) {
        static_cast<const CylinderResponse *>(context)->cubes.draw();
    }
};

struct Audio::OctahedronResponse {
    VolumeCubes cubes{4};
    uint32_t kicks = 0;

    static float update(TextureState &texture, uint32_t deltaMs, void *context) {
        auto &response = *static_cast<OctahedronResponse *>(context);
        const auto audio = RemoteInputs::audio();
        if (audio.dedicatedBassKick) {
            ++response.kicks;
            changeOctahedronColors(texture);
        }
        response.cubes.update(audio.volumeSm, deltaMs);

        const float rate = shapeRateForBpm(audio.bpm);
        char detail[128];
        snprintf(detail, sizeof(detail), "Octahedron rate=%.2fx cubeEdge=%.2f fullVol=%d kicks=%lu",
                 double(rate), double(response.cubes.edge), max(1, int(controls().shapeVolumeForFullCube)),
                 (unsigned long)response.kicks);
        debug(audio, detail, audio.dedicatedBassKick ? response.kicks : 0);
        return rate;
    }

    static void drawCubes(void *context) {
        static_cast<const OctahedronResponse *>(context)->cubes.draw();
    }
};

void Audio::animateCylinder() {
    CylinderResponse response;
    const Shapes::FrameControl control{CylinderResponse::update, TestDemo::audioRoutineRunning, &response,
                                       CylinderResponse::drawCubes};
    // The frame callback sets both materials before the first draw.
    Shapes::animateTumbling(SHAPE_CYLINDER, 0, RENDER_SOLID, CRGBPalette16(CRGB(CRGB::Black)),
                            MODE_SOLID, 1.0f, true, &control);
}

void Audio::animateOctahedron() {
    OctahedronResponse response;
    const Shapes::FrameControl control{OctahedronResponse::update, TestDemo::audioRoutineRunning, &response,
                                       OctahedronResponse::drawCubes};
    // Gray selects the geometry engine's existing contrasting material split.
    Shapes::animateSpinning(SHAPE_OCTAHEDRON, 0, 1.1f, RENDER_SOLID, CRGBPalette16(CRGB(CRGB::Gray)),
                            MODE_SOLID, true, &control);
}
