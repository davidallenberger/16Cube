#include "Audio.h"
#include "GeometryEngine.h"
#include "Shapes.h"

namespace {
    constexpr uint32_t minColorChangeMs = 700;
    constexpr uint8_t capChangesPerBodyChange = 4;
    constexpr uint32_t rainbowColors[] = {
        0xFF0000, 0xFF8000, 0xFFFF00, 0x00FF00, 0x0000FF, 0x4B0082, 0x9400D3
    };
    constexpr uint8_t rainbowColorCount = sizeof(rainbowColors) / sizeof(rainbowColors[0]);

    struct CylinderResponse {
        TextureState caps{MODE_SOLID};
        uint8_t bodyColor = random8(rainbowColorCount);
        uint8_t capColor = random8(rainbowColorCount);
        uint32_t hits = 0;
        uint32_t colorChanges = 0;
        uint32_t lastColorChangeMs = 0;
    };

    void changeCaps(CylinderResponse &response) {
        if (random8(3) == 0) {
            response.capColor = (response.capColor + random8(1, rainbowColorCount)) % rainbowColorCount;
            response.caps.mode = MODE_SOLID;
            response.caps.palette[0] = CRGB(rainbowColors[response.capColor]);
            return;
        }

        // Sample every non-solid shader. Skip the active one so successive
        // texture changes are visible even when they draw the same palette.
        constexpr uint8_t firstMode = MODE_UNIFORM_CYCLE;
        constexpr uint8_t modeCount = SHADER_MODE_COUNT - firstMode;
        const uint8_t modeIndex = response.caps.mode == MODE_SOLID ? random8(modeCount) :
            (response.caps.mode - firstMode + random8(1, modeCount)) % modeCount;
        response.caps.mode = ShaderMode(firstMode + modeIndex);
        response.caps.palette = response.caps.mode == MODE_SOAP || response.caps.mode == MODE_WAVING_CELL
            ? PaletteUtils::getSmartPalette(response.caps.mode) : PaletteUtils::getRandomPalette();
    }

    float updateCylinder(TextureState &texture, uint32_t deltaMs, void *context) {
        auto &response = *static_cast<CylinderResponse *>(context);
        if (!texture.cylinderCaps) {
            texture.cylinderCaps = &response.caps;
            texture.palette[0] = CRGB(rainbowColors[response.bodyColor]);
            response.caps.palette[0] = CRGB(rainbowColors[response.capColor]);
        }

        const float sensitivity = Audio::controls().peakAttackSensitivity;
        const auto audio = RemoteInputs::audio(sensitivity);
        const bool attack = audio.hasAttack();
        if (attack) {
            ++response.hits;
        }

        const uint32_t now = millis();
        // Keep the shared attack count for tuning. Only the cylinder's
        // material changes wait for the minimum hold.
        if (attack && (response.colorChanges == 0 ||
                       now - response.lastColorChangeMs >= minColorChangeMs)) {
            response.lastColorChangeMs = now;
            ++response.colorChanges;
            changeCaps(response);
            if (response.colorChanges % capChangesPerBodyChange == 0) {
                response.bodyColor = (response.bodyColor + random8(1, rainbowColorCount)) % rainbowColorCount;
                texture.palette[0] = CRGB(rainbowColors[response.bodyColor]);
            }
        }
        response.caps.advance(deltaMs);

        // The shared audio clock smooths BPM. The shape renderer integrates
        // this bounded rate, so an estimate change never snaps the orientation.
        // Ease up from 0.1x at the shared 12 BPM idle rate to 0.7x at
        // 80 BPM, then use the existing musical range (1.0x at 120 BPM).
        const float rate = audio.bpm < 80.0f
            ? 0.1f + 0.6f * constrain((audio.bpm - 12.0f) / 68.0f, 0.0f, 1.0f)
            : constrain(1.0f + (audio.bpm - 120.0f) * 0.0075f, 0.7f, 1.3f);
        char detail[144];
        const char *capPalette = response.caps.mode == MODE_SOLID ? "ROYGBIV" :
            response.caps.mode == MODE_HIPHOTIC ? "Hiphotic Native" : PaletteUtils::getPaletteNameBuffer();
        snprintf(detail, sizeof(detail),
                 "Cylinder rate=%.2fx hits=%lu body=#%06lX capMode=%u capPalette=%s attackTrace=1 changes=%lu",
                 double(rate), (unsigned long)response.hits, (unsigned long)rainbowColors[response.bodyColor],
                 unsigned(response.caps.mode), capPalette, (unsigned long)response.colorChanges);
        Audio::debug(audio, detail, attack ? response.hits : 0);
        return rate;
    }
}

void Audio::animateCylinder() {
    CylinderResponse response;
    const Shapes::FrameControl control{updateCylinder, TestDemo::audioRoutineRunning, &response};
    // The frame callback sets both materials before the first draw.
    Shapes::animateTumbling(SHAPE_CYLINDER, 0, RENDER_SOLID, CRGBPalette16(CRGB(CRGB::Black)),
                            MODE_SOLID, 1.0f, true, &control);
}
