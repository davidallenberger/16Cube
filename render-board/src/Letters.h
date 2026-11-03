#pragma once
#include <FastLED.h>
#include "TextureEngine.h"

namespace Letters {
    // THE MASTER INTERNAL DIRECTOR
    void scrollTextInternal(const char* text, uint32_t durationMs, uint16_t iterations, 
                            float xScale, float yScale, uint8_t scrollDelayMs,
                            ShaderMode fgMode, CRGBPalette16 fgPal,
                            ShaderMode bgMode, CRGBPalette16 bgPal,
                            bool overrideIconColor, uint8_t dimBackgroundPercent, bool rightToLeft);

    // ==========================================
    // MODE 1: CYCLES (Loops the message N times)
    // ==========================================
    
    // 1. FG Palette, BG Palette
    inline void scrollTextCycles(const char* text, uint16_t iterations, float xScale, float yScale, uint8_t scrollDelayMs,
                           CRGBPalette16 fgPal, ShaderMode fgMode, CRGBPalette16 bgPal, ShaderMode bgMode,
                           bool overrideIconColor = false, uint8_t dimBackgroundPercent = 75, bool rightToLeft = false) {
        scrollTextInternal(text, 0, iterations, xScale, yScale, scrollDelayMs, fgMode, fgPal, bgMode, bgPal, overrideIconColor, dimBackgroundPercent, rightToLeft);
    }
    // 2. FG Solid, BG Palette
    inline void scrollTextCycles(const char* text, uint16_t iterations, float xScale, float yScale, uint8_t scrollDelayMs,
                           CRGB fgColor, CRGBPalette16 bgPal, ShaderMode bgMode,
                           bool overrideIconColor = false, uint8_t dimBackgroundPercent = 75, bool rightToLeft = false) {
        scrollTextInternal(text, 0, iterations, xScale, yScale, scrollDelayMs, MODE_SOLID, CRGBPalette16(fgColor), bgMode, bgPal, overrideIconColor, dimBackgroundPercent, rightToLeft);
    }
    // 3. FG Palette, BG Solid
    inline void scrollTextCycles(const char* text, uint16_t iterations, float xScale, float yScale, uint8_t scrollDelayMs,
                           CRGBPalette16 fgPal, ShaderMode fgMode, CRGB bgColor,
                           bool overrideIconColor = false, uint8_t dimBackgroundPercent = 75, bool rightToLeft = false) {
        scrollTextInternal(text, 0, iterations, xScale, yScale, scrollDelayMs, fgMode, fgPal, MODE_SOLID, CRGBPalette16(bgColor), overrideIconColor, dimBackgroundPercent, rightToLeft);
    }
    // 4. FG Solid, BG Solid
    inline void scrollTextCycles(const char* text, uint16_t iterations, float xScale, float yScale, uint8_t scrollDelayMs,
                           CRGB fgColor, CRGB bgColor,
                           bool overrideIconColor = false, uint8_t dimBackgroundPercent = 75, bool rightToLeft = false) {
        scrollTextInternal(text, 0, iterations, xScale, yScale, scrollDelayMs, MODE_SOLID, CRGBPalette16(fgColor), MODE_SOLID, CRGBPalette16(bgColor), overrideIconColor, dimBackgroundPercent, rightToLeft);
    }
    // 5. FG Solid, BG Smart (No Palette)
    inline void scrollTextCycles(const char* text, uint16_t iterations, float xScale, float yScale, uint8_t scrollDelayMs,
                           CRGB fgColor, ShaderMode bgMode,
                           bool overrideIconColor = false, uint8_t dimBackgroundPercent = 75, bool rightToLeft = false) {
        scrollTextInternal(text, 0, iterations, xScale, yScale, scrollDelayMs, MODE_SOLID, CRGBPalette16(fgColor), bgMode, PaletteUtils::getSmartPalette(bgMode), overrideIconColor, dimBackgroundPercent, rightToLeft);
    }
    // 6. FG Smart, BG Smart (No Palettes)
    inline void scrollTextCycles(const char* text, uint16_t iterations, float xScale, float yScale, uint8_t scrollDelayMs,
                           ShaderMode fgMode, ShaderMode bgMode,
                           bool overrideIconColor = false, uint8_t dimBackgroundPercent = 75, bool rightToLeft = false) {
        scrollTextInternal(text, 0, iterations, xScale, yScale, scrollDelayMs, fgMode, PaletteUtils::getSmartPalette(fgMode), bgMode, PaletteUtils::getSmartPalette(bgMode), overrideIconColor, dimBackgroundPercent, rightToLeft);
    }

    // ==========================================
    // MODE 2: TIMED (Runs for N milliseconds)
    // ==========================================
    
    // 1. FG Palette, BG Palette
    inline void scrollTextTimed(const char* text, uint32_t durationMs, float xScale, float yScale, uint8_t scrollDelayMs,
                           CRGBPalette16 fgPal, ShaderMode fgMode, CRGBPalette16 bgPal, ShaderMode bgMode,
                           bool overrideIconColor = false, uint8_t dimBackgroundPercent = 75, bool rightToLeft = false) {
        scrollTextInternal(text, durationMs, 0, xScale, yScale, scrollDelayMs, fgMode, fgPal, bgMode, bgPal, overrideIconColor, dimBackgroundPercent, rightToLeft);
    }
    // 2. FG Solid, BG Palette
    inline void scrollTextTimed(const char* text, uint32_t durationMs, float xScale, float yScale, uint8_t scrollDelayMs,
                           CRGB fgColor, CRGBPalette16 bgPal, ShaderMode bgMode,
                           bool overrideIconColor = false, uint8_t dimBackgroundPercent = 75, bool rightToLeft = false) {
        scrollTextInternal(text, durationMs, 0, xScale, yScale, scrollDelayMs, MODE_SOLID, CRGBPalette16(fgColor), bgMode, bgPal, overrideIconColor, dimBackgroundPercent, rightToLeft);
    }
    // 3. FG Palette, BG Solid
    inline void scrollTextTimed(const char* text, uint32_t durationMs, float xScale, float yScale, uint8_t scrollDelayMs,
                           CRGBPalette16 fgPal, ShaderMode fgMode, CRGB bgColor,
                           bool overrideIconColor = false, uint8_t dimBackgroundPercent = 75, bool rightToLeft = false) {
        scrollTextInternal(text, durationMs, 0, xScale, yScale, scrollDelayMs, fgMode, fgPal, MODE_SOLID, CRGBPalette16(bgColor), overrideIconColor, dimBackgroundPercent, rightToLeft);
    }
    // 4. FG Solid, BG Solid
    inline void scrollTextTimed(const char* text, uint32_t durationMs, float xScale, float yScale, uint8_t scrollDelayMs,
                           CRGB fgColor, CRGB bgColor,
                           bool overrideIconColor = false, uint8_t dimBackgroundPercent = 75, bool rightToLeft = false) {
        scrollTextInternal(text, durationMs, 0, xScale, yScale, scrollDelayMs, MODE_SOLID, CRGBPalette16(fgColor), MODE_SOLID, CRGBPalette16(bgColor), overrideIconColor, dimBackgroundPercent, rightToLeft);
    }
    // 5. FG Solid, BG Smart (No Palette)
    inline void scrollTextTimed(const char* text, uint32_t durationMs, float xScale, float yScale, uint8_t scrollDelayMs,
                           CRGB fgColor, ShaderMode bgMode,
                           bool overrideIconColor = false, uint8_t dimBackgroundPercent = 75, bool rightToLeft = false) {
        scrollTextInternal(text, durationMs, 0, xScale, yScale, scrollDelayMs, MODE_SOLID, CRGBPalette16(fgColor), bgMode, PaletteUtils::getSmartPalette(bgMode), overrideIconColor, dimBackgroundPercent, rightToLeft);
    }
    // 6. FG Smart, BG Smart (No Palettes)
    inline void scrollTextTimed(const char* text, uint32_t durationMs, float xScale, float yScale, uint8_t scrollDelayMs,
                           ShaderMode fgMode, ShaderMode bgMode,
                           bool overrideIconColor = false, uint8_t dimBackgroundPercent = 75, bool rightToLeft = false) {
        scrollTextInternal(text, durationMs, 0, xScale, yScale, scrollDelayMs, fgMode, PaletteUtils::getSmartPalette(fgMode), bgMode, PaletteUtils::getSmartPalette(bgMode), overrideIconColor, dimBackgroundPercent, rightToLeft);
    }
}