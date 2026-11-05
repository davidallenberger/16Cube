#pragma once
#include <FastLED.h>
constexpr int RENDER_SOLID=0,SHAPE_MERKABA=0;
enum ShaderMode { MODE_SOLID, MODE_UNIFORM_CYCLE, MODE_SPATIAL_GRADIENT,
 MODE_LINEAR_FLOW, MODE_NOISE_FIELD, MODE_SCATTER, MODE_DISTANCE_FIELD,
 MODE_HIPHOTIC, MODE_DISTORTION_WAVES };
struct TextureState {
 uint32_t phase=0;
 ShaderMode mode;
 CRGBPalette16 palette;
 TextureState(ShaderMode sm,int pal,float=1.0f):mode(sm),palette(pal){}
 CRGB getColor(int,int,int) const {return CRGB(255,255,255);}
 void advance(uint32_t delta){phase+=delta;}
};

struct TexturePlan { ShaderMode sMode; CRGBPalette16 palette; };
struct PaletteUtils {
 static CRGBPalette16 getMerkabaPalette(bool fourColor){return fourColor ? 100 : 101;}
 static TexturePlan getRandomMovingTexturePlan(){return {MODE_LINEAR_FLOW,RainbowColors_p};}
 static const char *getPaletteNameBuffer(){return "Rainbow";}
};
