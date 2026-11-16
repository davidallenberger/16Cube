#pragma once
#include <FastLED.h>
constexpr int SHAPE_MERKABA=0;
enum ShaderMode { MODE_SOLID, MODE_UNIFORM_CYCLE, MODE_SPATIAL_GRADIENT,
 MODE_LINEAR_FLOW, MODE_NOISE_FIELD, MODE_SCATTER, MODE_DISTANCE_FIELD,
 MODE_HIPHOTIC, MODE_DISTORTION_WAVES, MODE_SOAP, MODE_WAVING_CELL, SHADER_MODE_COUNT };
struct TextureState {
 uint32_t phase=0;
 ShaderMode mode;
 CRGBPalette16 palette;
 const TextureState *cylinderCaps = nullptr;
 TextureState(ShaderMode sm,int pal,float=1.0f,uint8_t=64,bool=true):mode(sm),palette(pal){}
 CRGB getColor(int,int,int) const {return CRGB(255,255,255);}
 void advance(uint32_t delta){phase+=delta;}
};

struct TexturePlan { ShaderMode sMode; CRGBPalette16 palette; };
struct PaletteUtils {
 static CRGBPalette16 getMerkabaPalette(bool fourColor){return fourColor ? 100 : 101;}
 static TexturePlan getRandomMovingTexturePlan(){return {MODE_LINEAR_FLOW,RainbowColors_p};}
 static const char *getPaletteNameBuffer(){return "Rainbow";}
};
