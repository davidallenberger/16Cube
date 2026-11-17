#include "WledPalettes.h"

#if INCLUDE_WLED_PALETTES

#include <stdio.h>

#include <math.h>

// Curated palettes from WLED 16.0.1, wled00/palettes.cpp and the built-in palette picker.
// https://github.com/wled/WLED/blob/v16.0.1/wled00/palettes.cpp
// WLED palette data copyright (c) 2016-present Christian Schwinne and contributors.
// Distributed under EUPL-1.2-or-later. Original gradient sources are credited below.
// See docs/licenses/WLED-EUPL-1.2.txt for the upstream license.
namespace WledPalettes {
namespace {
// Gradient palette "Sunset_Real_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/atmospheric/Sunset_Real.c3g
const uint8_t Sunset_Real_gp[] PROGMEM = {
      0, 181,   0,   0,     22, 218,  85,   0,     51, 255, 170,   0,
     85, 211,  85,  77,    135, 167,   0, 169,    198,  73,   0, 188,
    255,   0,   0, 207,
};

// Gradient palette "rgi_15_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/ds/rgi/rgi_15.c3g
const uint8_t rgi_15_gp[] PROGMEM = {
      0,  41,  14,  99,     31, 128,  24,  74,     63, 227,  34,  50,
     95, 132,  31,  76,    127,  47,  29, 102,    159, 109,  47, 101,
    191, 176,  66, 100,    223, 129,  57, 104,    255,  84,  48, 108,
};

// Gradient palette "retro2_16_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/ma/retro2/retro2_16.c3g
const uint8_t retro2_16_gp[] PROGMEM = {
      0, 222, 191,   8,    255, 117,  52,   1,
};

// Gradient palette "Analogous_1_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/red/Analogous_1.c3g
const uint8_t Analogous_1_gp[] PROGMEM = {
      0,  38,   0, 255,     63,  86,   0, 255,    127, 139,   0, 255,
    191, 196,   0, 117,    255, 255,   0,   0,
};

// Gradient palette "es_pinksplash_08_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/pink_splash/es_pinksplash_08.c3g
const uint8_t es_pinksplash_08_gp[] PROGMEM = {
      0, 186,  63, 255,    127, 227,   9,  85,    175, 234, 205, 213,
    221, 205,  38, 176,    255, 205,  38, 176,
};

// Gradient palette "es_landscape_64_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/landscape/es_landscape_64.c3g
const uint8_t es_landscape_64_gp[] PROGMEM = {
      0,   0,   0,   0,     37,  31,  89,  19,     76,  72, 178,  43,
    127, 150, 235,   5,    128, 186, 234, 119,    130, 222, 233, 252,
    153, 197, 219, 231,    204, 132, 179, 253,    255,  28, 107, 225,
};

// Gradient palette "rainbowsherbet_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/ma/icecream/rainbowsherbet.c3g
const uint8_t rainbowsherbet_gp[] PROGMEM = {
      0, 255, 102,  41,     43, 255, 140,  90,     86, 255,  51,  90,
    127, 255, 153, 169,    170, 255, 255, 249,    209, 113, 255,  85,
    255, 157, 255, 137,
};

// Gradient palette "gr65_hult_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/hult/gr65_hult.c3g
const uint8_t gr65_hult_gp[] PROGMEM = {
      0, 251, 216, 252,     48, 255, 192, 255,     89, 239,  95, 241,
    160,  51, 153, 217,    216,  24, 184, 174,    255,  24, 184, 174,
};

// Gradient palette "gr64_hult_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/hult/gr64_hult.c3g
const uint8_t gr64_hult_gp[] PROGMEM = {
      0,  24, 184, 174,     66,   8, 162, 150,    104, 124, 137,   7,
    130, 178, 186,  22,    150, 124, 137,   7,    201,   6, 156, 144,
    239,   0, 128, 117,    255,   0, 128, 117,
};

// Gradient palette "GMT_drywet_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/gmt/GMT_drywet.c3g
const uint8_t GMT_drywet_gp[] PROGMEM = {
      0, 119,  97,  33,     42, 235, 199,  88,     84, 169, 238, 124,
    127,  37, 238, 232,    170,   7, 120, 236,    212,  27,   1, 175,
    255,   4,  51, 101,
};

// Gradient palette "ib_jul01_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/ing/xmas/ib_jul01.c3g
const uint8_t ib_jul01_gp[] PROGMEM = {
      0, 226,   6,  12,     94,  26,  96,  78,    132, 130, 189,  94,
    255, 177,   3,   9,
};

// Gradient palette "es_vintage_57_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/vintage/es_vintage_57.c3g
const uint8_t es_vintage_57_gp[] PROGMEM = {
      0,  29,   8,   3,     53,  76,   1,   0,    104, 142,  96,  28,
    153, 211, 191,  61,    255, 117, 129,  42,
};

// Gradient palette "Tertiary_01_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/vermillion/Tertiary_01.c3g
const uint8_t Tertiary_01_gp[] PROGMEM = {
      0,   0,  25, 255,     63,  38, 140, 117,    127,  86, 255,   0,
    191, 167, 140,  19,    255, 255,  25,  41,
};

// Gradient palette "Pink_Purple_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/atmospheric/Pink_Purple.c3g
const uint8_t Pink_Purple_gp[] PROGMEM = {
      0,  79,  32, 109,     25,  90,  40, 117,     51, 102,  48, 124,
     76, 141, 135, 185,    102, 180, 222, 248,    109, 208, 236, 252,
    114, 237, 250, 255,    122, 206, 200, 239,    149, 177, 149, 222,
    183, 187, 130, 203,    255, 198, 111, 184,
};

// Gradient palette "es_autumn_19_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/es/autumn/es_autumn_19.c3g
const uint8_t es_autumn_19_gp[] PROGMEM = {
      0,  90,  14,   5,     51, 139,  41,  13,     84, 180,  70,  17,
    104, 192, 202, 125,    112, 177, 137,   3,    122, 190, 200, 131,
    124, 192, 202, 124,    135, 177, 137,   3,    142, 194, 203, 118,
    163, 177,  68,  17,    204, 128,  35,  12,    249,  74,   5,   2,
    255,  74,   5,   2,
};

// Gradient palette "BlacK_Blue_Magenta_White_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/basic/BlacK_Blue_Magenta_White.c3g
const uint8_t BlacK_Blue_Magenta_White_gp[] PROGMEM = {
      0,   0,   0,   0,     42,   0,   0, 117,     84,   0,   0, 255,
    127, 113,   0, 255,    170, 255,   0, 255,    212, 255, 128, 255,
    255, 255, 255, 255,
};

// Gradient palette "BlacK_Magenta_Red_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/basic/BlacK_Magenta_Red.c3g
const uint8_t BlacK_Magenta_Red_gp[] PROGMEM = {
      0,   0,   0,   0,     63, 113,   0, 117,    127, 255,   0, 255,
    191, 255,   0, 117,    255, 255,   0,   0,
};

// Gradient palette "BlacK_Red_Magenta_Yellow_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/basic/BlacK_Red_Magenta_Yellow.c3g
const uint8_t BlacK_Red_Magenta_Yellow_gp[] PROGMEM = {
      0,   0,   0,   0,     42, 113,   0,   0,     84, 255,   0,   0,
    127, 255,   0, 117,    170, 255,   0, 255,    212, 255, 128, 117,
    255, 255, 255,   0,
};

// Gradient palette "Blue_Cyan_Yellow_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/nd/basic/Blue_Cyan_Yellow.c3g
const uint8_t Blue_Cyan_Yellow_gp[] PROGMEM = {
      0,   0,   0, 255,     63,   0, 128, 255,    127,   0, 255, 255,
    191, 113, 255, 117,    255, 255, 255,   0,
};

//Custom palette by Aircoookie
const uint8_t Orange_Teal_gp[] PROGMEM = {
      0,   0, 150,  92,     55,   0, 150,  92,    200, 255,  72,   0,
    255, 255,  72,   0,
};

//Custom palette by Aircoookie
const uint8_t Tiamat_gp[] PROGMEM = {
      0,   1,   2,  14,     33,   2,   5,  35,    100,  13, 135,  92,
    120,  43, 255, 193,    140, 247,   7, 249,    160, 193,  17, 208,
    180,  39, 255, 154,    200,   4, 213, 236,    220,  39, 252, 135,
    240, 193, 213, 253,    255, 255, 249, 255,
};

//Custom palette by Aircoookie
const uint8_t April_Night_gp[] PROGMEM = {
      0,   1,   5,  45,     10,   1,   5,  45,     25,   5, 169, 175,
     40,   1,   5,  45,     61,   1,   5,  45,     76,  45, 175,  31,
     91,   1,   5,  45,    112,   1,   5,  45,    127, 249, 150,   5,
    143,   1,   5,  45,    162,   1,   5,  45,    178, 255,  92,   0,
    193,   1,   5,  45,    214,   1,   5,  45,    229, 223,  45,  72,
    244,   1,   5,  45,    255,   1,   5,  45,
};

const uint8_t Orangery_gp[] PROGMEM = {
      0, 255,  95,  23,     30, 255,  82,   0,     60, 223,  13,   8,
     90, 144,  44,   2,    120, 255, 110,  17,    150, 255,  69,   0,
    180, 158,  13,  11,    210, 241,  82,  17,    255, 213,  37,   4,
};

//inspired by Mark Kriegsman https://gist.github.com/kriegsman/756ea6dcae8e30845b5a
const uint8_t C9_gp[] PROGMEM = {
      0, 184,   4,   0,     60, 184,   4,   0,     65, 144,  44,   2,
    125, 144,  44,   2,    130,   4,  96,   2,    190,   4,  96,   2,
    195,   7,   7,  88,    255,   7,   7,  88,
};

const uint8_t Sakura_gp[] PROGMEM = {
      0, 196,  19,  10,     65, 255,  69,  45,    130, 223,  45,  72,
    195, 255,  82, 103,    255, 223,  13,  17,
};

const uint8_t Aurora_gp[] PROGMEM = {
      0,   1,   5,  45,     64,   0, 200,  23,    128,   0, 255,   0,
    170,   0, 243,  45,    200,   0, 135,   7,    255,   1,   5,  45,
};

const uint8_t Atlantica_gp[] PROGMEM = {
      0,   0,  28, 112,     50,  32,  96, 255,    100,   0, 243,  45,
    150,  12,  95,  82,    200,  25, 190,  95,    255,  40, 170,  80,
};

const uint8_t C9_2_gp[] PROGMEM = {
      0,   6, 126,   2,     45,   6, 126,   2,     46,   4,  30, 114,
     90,   4,  30, 114,     91, 255,   5,   0,    135, 255,   5,   0,
    136, 196,  57,   2,    180, 196,  57,   2,    181, 137,  85,   2,
    255, 137,  85,   2,
};

// Gradient palette "temperature_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/arendal/temperature.c3g
const uint8_t temperature_gp[] PROGMEM = {
      0,  20,  92, 171,     14,  15, 111, 186,     28,   6, 142, 211,
     42,   2, 161, 227,     56,  16, 181, 239,     70,  38, 188, 201,
     84,  86, 204, 200,     99, 139, 219, 176,    113, 182, 229, 125,
    127, 196, 230,  63,    141, 241, 240,  22,    155, 254, 222,  30,
    170, 251, 199,   4,    184, 247, 157,   9,    198, 243, 114,  15,
    226, 213,  30,  29,    240, 151,  38,  35,    255, 151,  38,  35,
};

const uint8_t Aurora2_gp[] PROGMEM = {
      0,  17, 177,  13,     64, 121, 242,   5,    128,  25, 173, 121,
    192, 250,  77, 127,    255, 171, 101, 221,
};

// Gradient palette "bhw1_01_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_01.c3g
const uint8_t retro_clown_gp[] PROGMEM = {
      0, 242, 168,  38,    117, 226,  78,  80,    255, 161,  54, 225,
};

// Gradient palette "bhw1_04_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_04.c3g
const uint8_t candy_gp[] PROGMEM = {
      0, 243, 242,  23,     15, 242, 168,  38,    142, 111,  21, 151,
    198,  74,  22, 150,    255,   0,   0, 117,
};

// Gradient palette "bhw1_05_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_05.c3g
const uint8_t toxy_reaf_gp[] PROGMEM = {
      0,   2, 239, 126,    255, 145,  35, 217,
};

// Gradient palette "bhw1_06_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_06.c3g
const uint8_t fairy_reaf_gp[] PROGMEM = {
      0, 220,  19, 187,    160,  12, 225, 219,    219, 203, 242, 223,
    255, 255, 255, 255,
};

// Gradient palette "bhw1_three_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_three.c3g
const uint8_t pink_candy_gp[] PROGMEM = {
      0, 255, 255, 255,     45,  50,  64, 255,    112, 242,  16, 186,
    140, 255, 255, 255,    155, 242,  16, 186,    196, 116,  13, 166,
    255, 255, 255, 255,
};

// Gradient palette "bhw1_w00t_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw1/bhw1_w00t.c3g
const uint8_t red_reaf_gp[] PROGMEM = {
      0,  36,  68, 114,    104, 149, 195, 248,    188, 255,   0,   0,
    255,  94,  14,   9,
};

// Gradient palette "bhw2_23_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw2/bhw2_23.c3g
const uint8_t aqua_flash_gp[] PROGMEM = {
      0,   0,   0,   0,     66, 130, 242, 245,     96, 255, 255,  53,
    124, 255, 255, 255,    153, 255, 255,  53,    188, 130, 242, 245,
    255,   0,   0,   0,
};

// Gradient palette "bhw2_xc_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw2/bhw2_xc.c3g
const uint8_t yelblu_hot_gp[] PROGMEM = {
      0,  43,  30,  57,     58,  73,   0, 119,    122,  87,   0,  74,
    158, 197,  57,  22,    183, 218, 117,  27,    219, 239, 177,  32,
    255, 246, 247,  27,
};

// Gradient palette "bhw2_22_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw2/bhw2_22.c3g
const uint8_t red_flash_gp[] PROGMEM = {
      0,   0,   0,   0,     99, 242,  12,   8,    130, 253, 228, 163,
    155, 242,  12,   8,    255,   0,   0,   0,
};

// Gradient palette "bhw3_40_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw3/bhw3_40.c3g
const uint8_t blink_red_gp[] PROGMEM = {
      0,   4,   7,   4,     43,  40,  25,  62,     76,  61,  15,  36,
    109, 207,  39,  96,    127, 255, 156, 184,    165, 185,  73, 207,
    204, 105,  66, 240,    255,  77,  29,  78,
};

// Gradient palette "bhw3_52_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw3/bhw3_52.c3g
const uint8_t red_shift_gp[] PROGMEM = {
      0,  98,  22,  93,     45, 103,  22,  73,     99, 192,  45,  56,
    132, 235, 187,  59,    175, 228,  85,  26,    201, 228,  56,  48,
    255,   2,   0,   2,
};

// Gradient palette "bhw4_097_gp", originally from
// http://seaviewsensing.com/pub/cpt-city/bhw/bhw4/bhw4_097.c3g
const uint8_t red_tide_gp[] PROGMEM = {
      0, 251,  46,   0,     28, 255, 139,  25,     43, 246, 158,  63,
     58, 246, 216, 123,     84, 243,  94,  10,    114, 177,  65,  11,
    140, 255, 241, 115,    168, 177,  65,  11,    196, 250, 233, 158,
    216, 255,  94,   6,    255, 126,   8,   4,
};

const uint8_t trafficlight_gp[] PROGMEM = {
      0,   0,   0,   0,     85,   0, 255,   0,    170, 255, 255,   0,
    255, 255,   0,   0,
};

} // namespace

// Each classic palette and retained WLED palette occupies one equally likely slot.
// Fire and C9 New remain materials of their dedicated effects.
const Entry entries[] = {
    { 6, "Party", nullptr, false},
    { 8, "Lava", nullptr, false},
    { 9, "Ocean", nullptr, false},
    {10, "Forest", nullptr, false},
    {11, "Rainbow", nullptr, false},
    {12, "Rainbow Bands", nullptr, true},
    {13, "Sunset", Sunset_Real_gp, false},
    {16, "Red & Blue", rgi_15_gp, false},
    {17, "Yellowout", retro2_16_gp, false},
    {18, "Analogous", Analogous_1_gp, false},
    {19, "Splash", es_pinksplash_08_gp, false},
    {25, "Landscape", es_landscape_64_gp, false},
    {27, "Sherbet", rainbowsherbet_gp, false},
    {28, "Hult", gr65_hult_gp, false},
    {29, "Hult 64", gr64_hult_gp, false},
    {30, "Drywet", GMT_drywet_gp, false},
    {31, "Jul", ib_jul01_gp, false},
    {32, "Grintage", es_vintage_57_gp, true},
    {34, "Tertiary", Tertiary_01_gp, false},
    {38, "Light Pink", Pink_Purple_gp, false},
    {39, "Autumn", es_autumn_19_gp, true},
    {40, "Magenta", BlacK_Blue_Magenta_White_gp, false},
    {41, "Magred", BlacK_Magenta_Red_gp, false},
    {42, "Yelmag", BlacK_Red_Magenta_Yellow_gp, false},
    {43, "Yelblu", Blue_Cyan_Yellow_gp, false},
    {44, "Orange & Teal", Orange_Teal_gp, false},
    {45, "Tiamat", Tiamat_gp, true},
    {46, "April Night", April_Night_gp, true},
    {47, "Orangery", Orangery_gp, false},
    {48, "C9", C9_gp, false},
    {49, "Sakura", Sakura_gp, false},
    {50, "Aurora", Aurora_gp, true},
    {51, "Atlantica", Atlantica_gp, false},
    {52, "C9 2", C9_2_gp, false},
    {54, "Temperature", temperature_gp, false},
    {55, "Aurora 2", Aurora2_gp, false},
    {56, "Retro Clown", retro_clown_gp, false},
    {57, "Candy", candy_gp, false},
    {58, "Toxy Reaf", toxy_reaf_gp, false},
    {59, "Fairy Reaf", fairy_reaf_gp, false},
    {61, "Pink Candy", pink_candy_gp, false},
    {62, "Red Reaf", red_reaf_gp, false},
    {63, "Aqua Flash", aqua_flash_gp, true},
    {64, "Yelblu Hot", yelblu_hot_gp, false},
    {66, "Red Flash", red_flash_gp, true},
    {67, "Blink Red", blink_red_gp, true},
    {68, "Red Shift", red_shift_gp, false},
    {69, "Red Tide", red_tide_gp, false},
    {71, "Traffic Light", trafficlight_gp, false},
};
static_assert(sizeof(entries) / sizeof(entries[0]) == count,
              "The shared pool contains five classic and 44 curated WLED palettes");

static uint8_t applyWledGamma(uint8_t channel) {
    return uint8_t(255.0f * powf(channel / 255.0f, 2.2f) + 0.5f);
}

CRGBPalette16 load(const Entry &entry) {
    if (!entry.gradient) {
        switch (entry.id) {
            case 6: return CRGBPalette16(PartyColors_p);
            case 8: return CRGBPalette16(LavaColors_p);
            case 9: return CRGBPalette16(OceanColors_p);
            case 10: return CRGBPalette16(ForestColors_p);
            case 11: return CRGBPalette16(RainbowColors_p);
            default: return CRGBPalette16(RainbowStripeColors_p);
        }
    }

    CRGBPalette16 palette(entry.gradient);
    // WLED applies gamma after the palette lookup. Convert the 16 entries once
    // here to approximate that look while using the cube's normal Linear Flow.
    for (int i = 0; i < 16; ++i) {
        CRGB &color = palette[i];
        color = CRGB(applyWledGamma(color.r), applyWledGamma(color.g), applyWledGamma(color.b));
    }
    return palette;
}
CRGBPalette16 selectRandom(char *name, size_t nameCapacity, bool allowDark, const Entry **selected) {
    size_t eligible = 0;
    for (const auto &entry : entries) {
        if (allowDark || !entry.isDarkPalette) ++eligible;
    }
    size_t choice = random8(eligible);
    const Entry *picked = nullptr;
    for (const auto &entry : entries) {
        if (!allowDark && entry.isDarkPalette) continue;
        if (choice-- == 0) {
            picked = &entry;
            break;
        }
    }
    const auto &entry = *picked;
    if (selected) *selected = picked;
    if (entry.id < 12) snprintf(name, nameCapacity, "%s", entry.name);
    else snprintf(name, nameCapacity, "WLED %02u %s", unsigned(entry.id), entry.name);
    return load(entry);
}
} // namespace WledPalettes

#endif
