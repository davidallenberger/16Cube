#pragma once

// The render-board PlatformIO environment opts in to the imported palettes.
#ifndef INCLUDE_WLED_PALETTES
#define INCLUDE_WLED_PALETTES 0
#endif

#if INCLUDE_WLED_PALETTES
#include "CubeCore.h"

namespace WledPalettes {
    struct Entry {
        uint8_t id;
        const char *name;
        const uint8_t *gradient;
        bool isDarkPalette;
    };
    constexpr size_t count = 49;
    extern const Entry entries[count];
    CRGBPalette16 load(const Entry &entry);

    // Equal chances across the five classic palettes and 44 WLED keepers.
    CRGBPalette16 selectRandom(char *name, size_t nameCapacity, bool allowDark = false,
                               const Entry **selected = nullptr);
}
#endif
