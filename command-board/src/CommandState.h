#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "SharedProtocol.h"
struct CommandSnapshot {
    CubinaWire::Mode mode=CubinaWire::Mode::Animation;
    uint32_t session=0,revision=1,capturedMs=0,beats=0;
    bool ready=false;
    AudioPayload audio={};
    GamepadState pads[2];
    uint32_t presses[2]={};
    uint8_t depth[CubinaWire::DEPTH_BYTES]={};
};
namespace CommandState {
void begin();
void selectMode(CubinaWire::Mode mode); // increments demand revision, clears stale data
void setReady(bool ready);
void publishAudio(const AudioPayload&,uint32_t beatCount);
void publishGamepads(const GamepadState&,const GamepadState&);
void publishDepth(const uint8_t* bits);
CommandSnapshot snapshot();
CommandSnapshot takeForSend(); // atomic copy + clear edge latches
}
