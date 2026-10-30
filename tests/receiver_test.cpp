#include <Arduino.h>
#include <FastLED.h>
#include "UARTReceiver.cpp"
#define private public
#include "CubeSystem.h"
#undef private
#include "CubeSystem.cpp"
#include <assert.h>
#include <stdio.h>
SerialStub Serial,Serial2;FastLEDStub FastLED;
CRGB CRGB::Black,CRGB::Green,CRGB::Yellow,CRGB::Red,CRGB::White,CRGB::Blue,CRGB::Purple;
static uint32_t clockMs=100;
unsigned long millis(){return clockMs;}uint32_t micros(){return clockMs*1000;}
void clearAll(){}void showCube(){}void initAppMemory(){}void setVoxel(uint8_t,uint8_t,uint8_t,const CRGB&){}
namespace TestDemo{void runEngine(bool){}void runHardwareWiringTest(){}}
void Audio::run(){}
void Audio::renderIcon(TextureState&){}
void Depth::run(){}
void Depth::renderIcon(TextureState&){}
int main(){
    // Registry entry points remain wired to the correct input families.
    assert (appRegistry[AUDIO_APP].run == &Audio::run);
    assert (appRegistry[DEPTH_APP].run == &Depth::run);

    using namespace CubinaWire;RemoteInputs::begin();Packet p;
    p.session=1;p.modeRevision=1;p.sequence=1;p.mode=Mode::AudioBluetooth;p.flags=SOURCE_READY;
    AudioPayload a={};a.mode=2;a.volumeRaw=100;writeAudio(p,a,10);accept(p);
    assert(RemoteInputs::audio().values.volumeRaw==100);
    p.sequence=2;writeAudio(p,a,11);accept(p);assert(RemoteInputs::consumeBeat());assert(!RemoteInputs::consumeBeat());
    // Do not accept duplicate/stale data or contradictory same-revision modes.
    accept(p);assert(RemoteInputs::stats().duplicates==1);
    Packet bad=p;bad.sequence=3;bad.mode=Mode::AudioDJ;accept(bad);assert(RemoteInputs::control().mode==Mode::AudioBluetooth);
    // Actual nonlocal dispatch: revision committed BEFORE jump, then no repeat.
    if(setjmp(engineJumpBuffer)==0){teleportArmed=true;CubeSystem::frameCheckpoint();assert(false);}
    assert(!teleportArmed);assert(CubeSystem::selectedMenuIndex==10);
    CubeSystem::frameCheckpoint();assert(CubeSystem::currentState==SYS_STATE_RUN_APP);
    p={};p.session=1;p.modeRevision=2;p.sequence=4;p.mode=Mode::Game;p.flags=SOURCE_READY;
    p.type=Type::Gamepads;p.length=GAMEPAD_BYTES;GamepadState g;g.connected=true;
    writePad(p.payload,g);writePad(p.payload+15,GamepadState());put32(p.payload+30,1);
    accept(p);CubeSystem::frameCheckpoint();assert(CubeSystem::currentState==SYS_STATE_MENU);
    assert(RemoteInputs::gamepad(ROLE_MASTER).a);assert(!RemoteInputs::heldGamepad(ROLE_MASTER).a);
    RemoteInputs::beginFrame();assert(!RemoteInputs::gamepad(ROLE_MASTER).a);
    // New authoritative depth demand bypasses menu; occupancy clears on switch.
    p={};p.mode=Mode::Depth;p.session=1;p.modeRevision=3;p.sequence=5;accept(p);
    CubeSystem::frameCheckpoint();assert(CubeSystem::selectedMenuIndex==11);
    assert(!RemoteInputs::audio().valid);
    p.flags=SOURCE_READY;p.type=Type::Depth;p.length=DEPTH_BYTES;p.sequence=6;
    setVoxelBit(p.payload,15,15,15);accept(p);uint8_t bits[512];assert(RemoteInputs::depth(bits)&&voxel(bits,15,15,15));
    clockMs+=251;assert(!RemoteInputs::depth(bits));
    // Controller reboot with lower counters is a new session, not stale data.
    p={};p.session=2;p.sequence=1;p.modeRevision=1;p.mode=Mode::Animation;accept(p);
    CubeSystem::frameCheckpoint();assert(CubeSystem::selectedMenuIndex==0);
    clockMs+=1001;assert(!RemoteInputs::control().linked);
    // Home hold is a one-shot jump, and needs real held input, not tap latches.
    p={};p.session=2;p.sequence=2;p.modeRevision=2;p.mode=Mode::Game;
    p.flags=SOURCE_READY;p.type=Type::Gamepads;p.length=GAMEPAD_BYTES;
    // Holding non-System extra buttons must never escape the game.
    const uint8_t ordinaryExtras[] = {
        CubinaInput::Select,
        CubinaInput::Start,
        CubinaInput::Capture
    };
    for (uint8_t extra : ordinaryExtras) {
        g.miscButtons = extra;
        writePad (p.payload, g);
        writePad (p.payload + 15, GamepadState ());
        ++p.sequence;
        accept (p);
        CubeSystem::frameCheckpoint ();
        clockMs += 501;
        ++p.sequence;
        accept (p);
        if (setjmp (engineJumpBuffer) == 0) {
            teleportArmed = true;
            CubeSystem::frameCheckpoint ();
            teleportArmed = false;
        } else {
            assert (false && "Non-System button triggered menu exit");
        }
    }
    g.miscButtons = 0;
    writePad (p.payload, g);
    ++p.sequence;
    accept (p);
    CubeSystem::frameCheckpoint ();
    g.miscButtons = CubinaInput::System;
    ++p.sequence;
    writePad(p.payload,g);writePad(p.payload+15,GamepadState());
    accept(p);CubeSystem::frameCheckpoint();clockMs+=501;++p.sequence;accept(p);
    if(setjmp(engineJumpBuffer)==0){teleportArmed=true;CubeSystem::frameCheckpoint();assert(false);}
    assert(!teleportArmed);
    teleportArmed=true;CubeSystem::frameCheckpoint(); // held Home must NOT jump twice
    teleportArmed=false;
    // Valid rollover and signed revision/sequence comparisons within half-range.
    p.session=3;p.sequence=0xfffffffeu;p.modeRevision=0xfffffffeu;accept(p);
    p.sequence=0xffffffffu;accept(p);p.sequence=0;p.modeRevision=0;accept(p);
    assert(RemoteInputs::control().revision==0);
    puts("Receiver/OS tests passed: stale input, session reboot, edge latches, beat latches, direct dispatch, one-shot mode tripwire.");
}
