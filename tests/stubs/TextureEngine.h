#pragma once
#include <FastLED.h>
constexpr int MODE_UNIFORM_CYCLE=0,MODE_SPATIAL_GRADIENT=1,RENDER_SOLID=0,SHAPE_MERKABA=0;
struct TextureState{TextureState(int,int){}CRGB getColor(int,int,int){return CRGB();}void advance(int){}};
