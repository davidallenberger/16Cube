#pragma once
// Deterministic host stand-in for the Arduino core, used only by the render
// replay (tools/render_replay). FastLED's own stub platform supplies LEDs,
// palettes, noise and random8/16; this file supplies the rest. The replay
// owns the clock (millis/micros) so every run is reproducible.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <string>

using std::max;
using std::min;

extern "C" {
uint32_t millis(void);
uint32_t micros(void);
void delay(int ms);
void delayMicroseconds(int us);
void yield(void);
}

template <typename T, typename A, typename B> inline T constrain(T x, A a, B b) {
    return x < T(a) ? T(a) : (x > T(b) ? T(b) : x);
}
inline long map(long x, long inMin, long inMax, long outMin, long outMax) {
    return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}
long random(long upper);
long random(long lower, long upper);
inline void randomSeed(unsigned long) {}
uint32_t esp_random();

#ifndef PI
#define PI 3.14159265358979323846
#endif
#ifndef TWO_PI
#define TWO_PI 6.28318530717958647693
#endif
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
inline void pinMode(int, int) {}
inline int digitalRead(int) { return HIGH; }
inline void digitalWrite(int, int) {}

// Serial output is captured line by line for the replay's logs.
struct ReplaySerial {
    void begin(unsigned long) {}
    template <class... T> void begin(unsigned long, T...) {}
    int availableForWrite() { return 4096; }
    int available() { return 0; }
    int read() { return -1; }
    void setTxBufferSize(size_t) {}
    void setRxBufferSize(size_t) {}
    size_t write(const uint8_t *data, size_t length);
    size_t write(uint8_t c) { return write(&c, 1); }
    int printf(const char *format, ...) __attribute__((format(printf, 2, 3)));
    void print(const char *text) { write(reinterpret_cast<const uint8_t *>(text), strlen(text)); }
    void println(const char *text = "") { print(text); print("\n"); }
};
extern ReplaySerial Serial;
extern ReplaySerial Serial2;
#ifndef SERIAL_8N1
#define SERIAL_8N1 0
#endif
