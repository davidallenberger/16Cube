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
static unsigned audioAdvanceRequests = 0;
static unsigned animationAdvanceRequests = 0;
namespace TestDemo {
    void runEngine(bool) {}
    void runHardwareWiringTest() {}
    void runAnimation() {}
    void runAudio() {}
    void stopPlayback() {}
    void requestNext(Deck deck) {
        if (deck == Deck::Audio) {
            ++audioAdvanceRequests;
        } else {
            ++animationAdvanceRequests;
        }
    }
}
void Audio::renderIcon(TextureState&){}
void Depth::run(){}
void Depth::renderIcon(TextureState&){}
int main(){
    // General attacks survive quiet packets and are frozen for one frame.
    using namespace CubinaWire;
    RemoteInputs::begin();
    Packet attackPacket;
    attackPacket.session = 100;
    attackPacket.sequence = 1;
    attackPacket.modeRevision = 1;
    attackPacket.mode = Mode::AudioBluetooth;
    attackPacket.flags = SOURCE_READY;
    AudioPayload attackPayload = {};
    attackPayload.mode = 2;
    attackPayload.attackCount = 1;
    attackPayload.attackStrength = 190;
    attackPayload.attackBands = 128;
    writeAudio(attackPacket, attackPayload, 0);
    accept(attackPacket);
    ++attackPacket.sequence;
    attackPayload.attackStrength = 0;
    attackPayload.attackBands = 0;
    writeAudio(attackPacket, attackPayload, 0);
    accept(attackPacket);
    RemoteInputs::beginFrame();
    assert(RemoteInputs::audio().midAttack);
    assert(RemoteInputs::audio().midAttack); // A second read is not consumption.
    RemoteInputs::beginFrame();
    assert(!RemoteInputs::audio().midAttack);
    // Registry entry points remain wired to the correct input families.
    assert (appRegistry[AUDIO_APP].run == &TestDemo::runAudio);
    assert (appRegistry[DEPTH_APP].run == &Depth::run);

    using namespace CubinaWire;RemoteInputs::begin();Packet p;
    p.session=1;p.modeRevision=1;p.sequence=1;p.mode=Mode::AudioBluetooth;p.flags=SOURCE_READY;
    AudioPayload a={};a.mode=2;a.volumeRaw=100;
    a.tempoBpm100 = 12800;
    a.tempoPhase = 16384;
    a.tempoConfidence = 220;
    a.tempoFlags = AUDIO_ACTIVE | TEMPO_LOCKED;
    writeAudio(p,a,10);accept(p);
    RemoteInputs::beginFrame();
    assert(RemoteInputs::audio().volumeRaw == 100);
    assert(RemoteInputs::audio().beatAngle() > 1.5f);
    p.sequence = 2;
    writeAudio(p, a, 11);
    accept(p);
    assert(!RemoteInputs::audio().bassKick); // Arrival cannot alter a frozen frame.
    RemoteInputs::beginFrame();
    assert(RemoteInputs::audio().bassKick);
    assert(RemoteInputs::audio().bassKick);
    RemoteInputs::beginFrame();
    assert(!RemoteInputs::audio().bassKick);
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
    assert(RemoteInputs::audio().volumeRaw == 0);
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

    // First entry into audio selects the current routine, never "next".
    p = {};
    p.session = 10;
    p.sequence = 1;
    p.modeRevision = 1;
    p.mode = Mode::AudioMic;
    accept(p);
    CubeSystem::frameCheckpoint();
    assert(audioAdvanceRequests == 0);
    assert(CubeSystem::selectedMenuIndex == AUDIO_APP);

    // Ordinary data/heartbeat packets must not act like another button press.
    ++p.sequence;
    accept(p);
    CubeSystem::frameCheckpoint();
    assert(audioAdvanceRequests == 0);

    // Hold is absolute packet state, independent of source demand revision.
    // Repeated heartbeats cannot restart/advance or accidentally toggle it.
    p.flags = PLAYBACK_HOLD;
    ++p.sequence;
    accept(p);
    CubeSystem::frameCheckpoint();
    assert(TestDemo::playbackHeld());
    assert(RemoteInputs::control().playbackHeld);
    assert(audioAdvanceRequests == 0);
    ++p.sequence;
    accept(p);
    CubeSystem::frameCheckpoint();
    assert(TestDemo::playbackHeld());
    assert(audioAdvanceRequests == 0);
    clockMs += 1001;
    CubeSystem::frameCheckpoint();
    assert(!TestDemo::playbackHeld());
    ++p.sequence;
    accept(p);
    CubeSystem::frameCheckpoint();
    assert(TestDemo::playbackHeld());
    p.flags = 0;
    ++p.sequence;
    accept(p);
    CubeSystem::frameCheckpoint();
    assert(!TestDemo::playbackHeld());
    assert(audioAdvanceRequests == 0);

    // A repeated source demand asks the managed effect to return, once.
    // Audio does NOT longjmp/restart its OS app for this ordinary advance.
    ++p.sequence;
    ++p.modeRevision;
    accept(p);
    if (setjmp(engineJumpBuffer) == 0) {
        teleportArmed = true;
        CubeSystem::frameCheckpoint();
    } else {
        assert(false && "Repeated audio demand unnecessarily escaped the app");
    }
    assert(audioAdvanceRequests == 1);
    assert(teleportArmed);
    teleportArmed = false;
    CubeSystem::frameCheckpoint();
    assert(audioAdvanceRequests == 1);
    ++p.sequence;
    accept(p);
    CubeSystem::frameCheckpoint();
    assert(audioAdvanceRequests == 1);

    // Changing sources preserves the visualization instead of advancing it.
    ++p.sequence;
    ++p.modeRevision;
    p.mode = Mode::AudioDJ;
    accept(p);
    CubeSystem::frameCheckpoint();
    assert(audioAdvanceRequests == 1);
    assert(demandedMode == Mode::AudioDJ);

    // A command-board reboot is a mode demand, not an intentional next press.
    p.session = 11;
    p.sequence = 1;
    p.modeRevision = 1;
    accept(p);
    CubeSystem::frameCheckpoint();
    assert(audioAdvanceRequests == 1);

    // Returning to audio from another app/menu also preserves its selection.
    CubeSystem::changeState(SYS_STATE_MENU);
    ++p.sequence;
    ++p.modeRevision;
    accept(p);
    CubeSystem::frameCheckpoint();
    assert(CubeSystem::currentState == SYS_STATE_RUN_APP);
    assert(audioAdvanceRequests == 1);

    // Same-session/same-revision reconnection must not advance the routine.
    clockMs += 1001;
    assert(!RemoteInputs::control().linked);
    ++p.sequence;
    accept(p);
    CubeSystem::frameCheckpoint();
    assert(audioAdvanceRequests == 1);

    // Normal Animation still uses its immediate OS escape. Its TestDemo
    // cursor adjustment is delivered exactly once, before abandoning a scene.
    ++p.sequence;
    ++p.modeRevision;
    p.mode = Mode::Animation;
    accept(p);
    CubeSystem::frameCheckpoint();
    assert(appRegistry[0].run == &TestDemo::runAnimation);
    assert(animationAdvanceRequests == 0);
    ++p.sequence;
    ++p.modeRevision;
    accept(p);
    if (setjmp(engineJumpBuffer) == 0) {
        teleportArmed = true;
        CubeSystem::frameCheckpoint();
        assert(false && "Animation button failed to skip current scene");
    }
    assert(animationAdvanceRequests == 1);
    assert(!teleportArmed);
    CubeSystem::frameCheckpoint();
    assert(animationAdvanceRequests == 1);

    puts("Receiver/OS tests passed: stale input, session reboot, edge latches, beat latches, direct dispatch, one-shot mode tripwire.");
    puts("Audio dispatch tests passed: repeated source advances once; heartbeat, source switch, reboot, menu return, and reconnect preserve selection.");
}
