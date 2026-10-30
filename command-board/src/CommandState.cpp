#include "CommandState.h"
namespace { SemaphoreHandle_t mutex=nullptr; CommandSnapshot state; uint32_t previous[2]={}; }
void CommandState::begin(){mutex=xSemaphoreCreateMutex();configASSERT(mutex);state.session=esp_random();}
CommandSnapshot CommandState::snapshot(){xSemaphoreTake(mutex,portMAX_DELAY);CommandSnapshot s=state;xSemaphoreGive(mutex);return s;}
void CommandState::selectMode(CubinaWire::Mode m){
    xSemaphoreTake(mutex,portMAX_DELAY);state.mode=m;++state.revision;state.ready=false;
    state.audio={};state.pads[0]=state.pads[1]=GamepadState();
    state.presses[0]=state.presses[1]=previous[0]=previous[1]=0;
    memset(state.depth,0,sizeof(state.depth));state.capturedMs=millis();xSemaphoreGive(mutex);
}
void CommandState::setReady(bool r){xSemaphoreTake(mutex,portMAX_DELAY);state.ready=r;xSemaphoreGive(mutex);}
void CommandState::publishAudio(const AudioPayload& a,uint32_t b){
    xSemaphoreTake(mutex,portMAX_DELAY);state.audio=a;state.beats=b;state.capturedMs=millis();state.ready=true;xSemaphoreGive(mutex);
}
void CommandState::publishGamepads(const GamepadState& a,const GamepadState& b){
    xSemaphoreTake(mutex,portMAX_DELAY);state.pads[0]=a;state.pads[1]=b;
    for(int i=0;i<2;++i){uint32_t now=CubinaWire::buttons(state.pads[i]);state.presses[i]|=now&~previous[i];previous[i]=now;}
    state.capturedMs=millis();state.ready=true;xSemaphoreGive(mutex);
}
void CommandState::publishDepth(const uint8_t* bits){xSemaphoreTake(mutex,portMAX_DELAY);memcpy(state.depth,bits,sizeof(state.depth));state.capturedMs=millis();state.ready=true;xSemaphoreGive(mutex);}
CommandSnapshot CommandState::takeForSend(){
    xSemaphoreTake(mutex,portMAX_DELAY);CommandSnapshot s=state;
    state.presses[0]=state.presses[1]=0;xSemaphoreGive(mutex);return s;
}
