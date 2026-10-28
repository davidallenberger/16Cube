#pragma once
#include <stdint.h>
struct CHSV{CHSV(uint8_t,uint8_t,uint8_t){}};
struct CRGB{CRGB(){} CRGB(int,int,int){} CRGB(CHSV){} static CRGB Black,Green,Yellow,Red,White,Blue,Purple;};
struct WS2812B{};struct WS2812{};
#define GRB 0
#define RGB 1
struct FastLEDStub{template<class T,int P,int O>void addLeds(CRGB*,int){} void clear(){}void show(){}void setBrightness(int){}};
extern FastLEDStub FastLED;
inline void fill_solid(CRGB*,int,CRGB){} inline void fadeToBlackBy(CRGB*,int,uint8_t){}
inline void random16_set_seed(uint32_t){}inline void random16_add_entropy(uint32_t){}
inline uint8_t beatsin8(int,int,int){return 100;}
static int RainbowColors_p=0;
