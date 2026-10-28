#pragma once
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <string>
using std::min; using std::max; using std::isnan; using std::isinf;
#define INPUT_PULLUP 2
#define LOW 0
#define SERIAL_8N1 0
#define ESP_INTR_FLAG_LEVEL1 1
#define pdMS_TO_TICKS(x) (x)
struct String : std::string { using std::string::string; String(unsigned long) {} String(std::string s):std::string(s){} };
struct SerialStub { size_t setRxBufferSize(size_t n){return n;} int available(){return 0;} int read(){return -1;} size_t write(uint8_t){return 1;} size_t setTxBufferSize(size_t n){return n;} int availableForWrite(){return 1024;} template<class... T> void begin(T...) {} template<class... T> void printf(const char*, T...) {} template<class T> size_t println(T) {return 0;} size_t write(uint8_t*,size_t n){return n;} };
extern SerialStub Serial, Serial2;
unsigned long millis(); void delay(int); int digitalRead(int); void pinMode(int,int);
long map(long,long,long,long,long);
double ledcSetup(int,double,int); void ledcAttachPin(int,int); void ledcWrite(int,int);

uint32_t esp_random();uint32_t micros();void delayMicroseconds(unsigned);long random(long);
inline void randomSeed(unsigned){}
