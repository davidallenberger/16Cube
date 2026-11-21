#pragma once
#include <stdint.h>
#include <cmath>
struct CHSV{uint8_t v;CHSV(uint8_t,uint8_t,uint8_t value):v(value){}};
struct CRGB{uint8_t r=0,g=0,b=0;CRGB(){} CRGB(uint32_t rgb):r(rgb>>16),g(rgb>>8),b(rgb){} CRGB(int red,int green,int blue):r(red),g(green),b(blue){} CRGB(CHSV hsv):r(hsv.v),g(hsv.v),b(hsv.v){} void nscale8(uint8_t v){r=uint16_t(r)*v/255;g=uint16_t(g)*v/255;b=uint16_t(b)*v/255;} CRGB &operator+=(const CRGB &c){r=std::min(255,int(r)+c.r);g=std::min(255,int(g)+c.g);b=std::min(255,int(b)+c.b);return *this;} static CRGB Black,Green,Yellow,Red,White,Blue,Purple; enum { Gray = 0x808080 };};
inline CRGB operator+(CRGB a, const CRGB &b) { return a += b; }
inline bool operator==(const CRGB &a, const CRGB &b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
struct CRGBPalette16 {
    int id;
    CRGBPalette16(int value) : id(value) {}
    CRGBPalette16(const CRGB&, const CRGB&, const CRGB&, const CRGB&) : id(0) {}
    operator int() const { return id; }
    CRGB operator[](int) const { return CRGB(255, 255, 255); }
};
inline CRGB blend(const CRGB &a, const CRGB &b, uint8_t amount) {
    return CRGB((a.r * (255 - amount) + b.r * amount) / 255,
                (a.g * (255 - amount) + b.g * amount) / 255,
                (a.b * (255 - amount) + b.b * amount) / 255);
}
inline int16_t sin16(uint16_t phase) {
    return int16_t(32767.0 * std::sin(phase * (6.283185307179586 / 65536.0)));
}
inline int16_t cos16(uint16_t phase) { return sin16(uint16_t(phase + 16384)); }
inline uint16_t sqrt16(uint16_t value) { return uint16_t(std::sqrt(value)); }
constexpr int LINEARBLEND = 0;
inline uint8_t qsub8(uint8_t a,uint8_t b){return a>b?a-b:0;}
inline uint8_t qadd8(uint8_t a,uint8_t b){return unsigned(a)+b>255?255:a+b;}
inline CRGB ColorFromPalette(const CRGBPalette16&,uint8_t,uint8_t value,int){return CRGB(value,value,value);}
struct WS2812B{};struct WS2812{};
#define GRB 0
#define RGB 1
struct FastLEDStub{template<class T,int P,int O>void addLeds(CRGB*,int){} void clear(){}void show(){}void setBrightness(int){}};
extern FastLEDStub FastLED;
inline void fill_solid(CRGB* pixels,int count,CRGB color){for(int i=0;i<count;++i)pixels[i]=color;} inline void fadeToBlackBy(CRGB*,int,uint8_t){}
inline void random16_set_seed(uint32_t){}inline void random16_add_entropy(uint32_t){}
inline uint8_t beatsin8(int,int,int){return 100;}
inline uint8_t sin8(uint8_t phase) {
    return uint8_t(128.0 + 127.0 * std::sin(phase * (6.283185307179586 / 256.0)));
}
inline uint8_t cubicwave8(uint8_t phase) {
    const double triangle = (phase < 128 ? phase : 255 - phase) / 127.0;
    return uint8_t(255.0 * triangle * triangle * (3.0 - 2.0 * triangle));
}
static int RainbowColors_p=0;

static int OceanColors_p=1, LavaColors_p=2, ForestColors_p=3, PartyColors_p=4;
// Deterministic spatial noise for geometry contracts; not a visual emulation.
inline uint8_t inoise8(uint16_t x,uint16_t y,uint16_t t){return 80+((x/64+2*(y/64)+t/64)%130);}
