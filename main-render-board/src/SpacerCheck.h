#pragma once

#include <stdint.h>

// Temporary, render-task-only investigation of the DESKTOP spacer pixels.
// Enable this through platformio.ini so every translation unit sees the same
// setting. With the switch off, neither scanning nor diagnostic state exists.
#ifndef CUBINA_CHECK_SPACERS
#define CUBINA_CHECK_SPACERS 0
#endif

#if CUBINA_CHECK_SPACERS
#if !defined(CUBINA_ENABLE_UART_RECEIVER) || CUBINA_ENABLE_UART_RECEIVER != 0
#error "Spacer baseline requires CUBINA_ENABLE_UART_RECEIVER=0 in build_flags"
#endif
#if !defined(FASTLED_ESP32_I2S_NUM_DMA_BUFFERS)
#error "Set FASTLED_ESP32_I2S_NUM_DMA_BUFFERS explicitly in build_flags"
#endif
#endif

namespace SpacerCheck {
    // These labels describe one invocation of TestDemo's scene dispatcher.
    // Names/render/motion/shader point to persistent literal strings. Palette
    // can point at a reusable name buffer: beginScene copies that one label.
    struct SceneInfo {
        const char *name;
        const char *render;
        const char *motion;
        const char *shader;
        const char *palette;
        uint16_t step;
        uint16_t totalSteps;
        bool ordered;
    };

    // A single pre-show inspection, assembled in CubeCore.cpp where the
    // real physical strip arrays are defined. No CRGB objects are copied.
    struct FrameScan {
        uint16_t checked = 0;
        uint16_t nonblack = 0;
        uint16_t firstIndex = 0;
        uint8_t firstOutput = 0;
        uint8_t red = 0;
        uint8_t green = 0;
        uint8_t blue = 0;
    };

#if CUBINA_CHECK_SPACERS
    void beginScene(const SceneInfo &info);
    void endScene();

    // PRIVATE CubeCore integration: the animation task calls these before
    // FastLED.show(). They never print, allocate, lock, or correct a pixel.
    bool sceneActive();
    void recordFrame(const FrameScan &scan, uint32_t scanUs);
#endif
}
