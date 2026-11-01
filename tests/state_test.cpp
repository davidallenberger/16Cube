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
    AudioPayload live = {};
    live.mode = 2;
    live.volumeRaw = 80;
    for (auto &band : live.fftResult) {
        band = live.volumeRaw;
    }
    CommandState::publishAudio(live, 7);
    const CommandSnapshot beforeRepeat = CommandState::snapshot();
    CommandState::repeatModeDemand();
    const CommandSnapshot afterRepeat = CommandState::snapshot();
    assert(afterRepeat.revision == beforeRepeat.revision + 1);
    assert(afterRepeat.mode == beforeRepeat.mode);
    assert(afterRepeat.ready == beforeRepeat.ready);
    assert(afterRepeat.capturedMs == beforeRepeat.capturedMs);
    assert(afterRepeat.beats == beforeRepeat.beats);
    assert(afterRepeat.audio.volumeRaw == 80);
    // Exercise the real CommandState mutex with concurrent publication/read.
    std::thread writer([]{for(int n=0;n<5000;++n){AudioPayload v={};v.mode=2;v.volumeRaw=n%256;
        for(auto& x:v.fftResult)x=v.volumeRaw;CommandState::publishAudio(v,n);}});
    for(int n=0;n<5000;++n){auto r=CommandState::snapshot();if(r.ready)for(auto x:r.audio.fftResult)assert(x==r.audio.volumeRaw);}
    writer.join();puts("Command state tests passed: coherent snapshots, short taps, mode clear, concurrent publication.");
}
