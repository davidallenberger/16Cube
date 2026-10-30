#include <Arduino.h>
#include "CommandState.cpp"
#include <assert.h>
#include <thread>
#include <stdio.h>
SerialStub Serial,Serial2;
unsigned long millis(){return 100;}uint32_t esp_random(){return 123;}
int main(){
    using CubinaWire::Mode;CommandState::begin();CommandState::selectMode(Mode::Game);
    GamepadState a,b;a.connected=true;a.a=true;
    CommandState::publishGamepads(a,b);a.a=false;CommandState::publishGamepads(a,b);
    auto s=CommandState::takeForSend();assert(!s.pads[0].a && (s.presses[0]&1));
    assert(CommandState::takeForSend().presses[0]==0);
    // New tap during a sender write survives into the NEXT snapshot.
    a.a=true;CommandState::publishGamepads(a,b);assert(CommandState::takeForSend().presses[0]&1);
    CommandState::selectMode(Mode::AudioBluetooth);assert(!CommandState::snapshot().ready);
    // Exercise the real CommandState mutex with concurrent publication/read.
    std::thread writer([]{for(int n=0;n<5000;++n){AudioPayload v={};v.mode=2;v.volumeRaw=n%256;
        for(auto& x:v.fftResult)x=v.volumeRaw;CommandState::publishAudio(v,n);}});
    for(int n=0;n<5000;++n){auto r=CommandState::snapshot();if(r.ready)for(auto x:r.audio.fftResult)assert(x==r.audio.volumeRaw);}
    writer.join();puts("Command state tests passed: coherent snapshots, short taps, mode clear, concurrent publication.");
}
