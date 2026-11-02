#include "UARTReceiver.cpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

SerialStub Serial;
SerialStub Serial2;
static uint32_t clockMs = 0;
unsigned long millis() {
    return clockMs;
}

static float angleDistance(float a, float b) {
    float difference = a - b;
    const float turn = 6.283185307f;
    difference -= floorf(difference / turn + 0.5f) * turn;
    return fabsf(difference);
}

int main() {
    using namespace CubinaWire;
    using RemoteInputs::Detail::AudioFrameClock;
    RemoteInputs::begin();
    Packet packet;
    packet.mode = Mode::AudioBluetooth;
    packet.flags = SOURCE_READY;
    packet.session = 1;
    packet.modeRevision = 1;
    AudioPayload payload = {};
    payload.mode = 2;
    payload.volumeRaw = 100;
    payload.volumeSm = 80;
    payload.sampleAvg = 60;
    payload.fftResult[7] = 55;
    uint32_t kicks = 0;

    auto send = [&] {
        ++packet.sequence;
        writeAudio(packet, payload, kicks);
        accept(packet);
    };
    send();
    RemoteInputs::beginFrame();
    const auto first = RemoteInputs::audio();
    assert(first.volumeRaw == 100 && first.volumeSm == 80 && first.volumeAvg == 60);
    assert(first.fftResult[7] == 55);
    assert(!first.bassKick && !first.midAttack && !first.highAttack);

    // Every band belongs to exactly one private musical group.
    for (unsigned band = 0; band < 16; ++band) {
        ++payload.attackCount;
        payload.attackStrength = 128;
        payload.attackBands = uint16_t(1u << band);
        send();
        RemoteInputs::beginFrame();
        const auto frame = RemoteInputs::audio();
        assert(frame.bassKick == (band < 4));
        assert(frame.midAttack == (band >= 4 && band <= 10));
        assert(frame.highAttack == (band >= 11));
        const auto again = RemoteInputs::audio();
        assert(frame.bassKick == again.bassKick && frame.midAttack == again.midAttack);
        RemoteInputs::beginFrame();
        assert(!RemoteInputs::audio().bassKick && !RemoteInputs::audio().midAttack);
        assert(!RemoteInputs::audio().highAttack);
    }

    // Freeze at the checkpoint; an arrival cannot change a frame being drawn.
    const auto before = RemoteInputs::audio();
    ++payload.attackCount;
    payload.attackBands = 0xFFFF;
    send();
    assert(!RemoteInputs::audio().bassKick && !RemoteInputs::audio().highAttack);
    assert(RemoteInputs::audio().beatAngle(10) == before.beatAngle(10));
    payload.attackStrength = 0;
    payload.attackBands = 0;
    send(); // Later quiet packet must not erase the pending event.
    RemoteInputs::beginFrame();
    assert(RemoteInputs::audio().bassKick && RemoteInputs::audio().midAttack);
    assert(RemoteInputs::audio().highAttack);
    RemoteInputs::beginFrame();

    // Sensitivity is a read-only artistic threshold, independent of levels.
    ++payload.attackCount;
    payload.attackBands = 0x0080;
    payload.attackStrength = 20;
    send();
    RemoteInputs::beginFrame();
    assert(!RemoteInputs::audio().midAttack);
    assert(RemoteInputs::audio(2).midAttack);
    assert(!RemoteInputs::audio(0).midAttack);
    assert(!RemoteInputs::audio(-1).midAttack);
    assert(!RemoteInputs::audio(std::numeric_limits<float>::quiet_NaN()).midAttack);
    assert(RemoteInputs::audio(2).volumeSm == 80);
    assert(RemoteInputs::audio(2).fftResult[7] == 55);

    ++kicks;
    payload.beat = 1;
    payload.attackStrength = 0;
    payload.attackBands = 0;
    send();
    RemoteInputs::beginFrame();
    assert(RemoteInputs::audio().bassKick);
    assert(RemoteInputs::audio().bassKick);
    assert(!RemoteInputs::audio(0).bassKick);
    RemoteInputs::beginFrame();
    assert(!RemoteInputs::audio().bassKick);

    // No read advances the clock, even if wall time changes during the frame.
    const float frozenAngle = RemoteInputs::audio().beatAngle(10);
    clockMs += 50;
    assert(RemoteInputs::audio().beatAngle(10) == frozenAngle);
    RemoteInputs::beginFrame();
    assert(RemoteInputs::audio().beatAngle(10) != frozenAngle);

    // Quiet packets keep audio fresh without making an OLD event fresh.
    ++payload.attackCount;
    payload.attackStrength = 200;
    payload.attackBands = 0x8000;
    payload.beat = 0;
    send();
    clockMs += 251;
    payload.attackStrength = 0;
    payload.attackBands = 0;
    send();
    RemoteInputs::beginFrame();
    assert(RemoteInputs::audio().volumeSm == 80);
    assert(!RemoteInputs::audio().highAttack);

    // Revision/source changes clear events and previous-source levels.
    ++payload.attackCount;
    payload.attackStrength = 200;
    payload.attackBands = 0xFFFF;
    send();
    ++packet.modeRevision;
    packet.type = Type::Status;
    packet.mode = Mode::AudioMic;
    packet.length = 0;
    ++packet.sequence;
    accept(packet);
    RemoteInputs::beginFrame();
    assert(RemoteInputs::audio().volumeSm == 0);
    assert(!RemoteInputs::audio().bassKick && !RemoteInputs::audio().midAttack);

    // Source data expires even while heartbeats keep the link alive.
    packet.mode = Mode::AudioBluetooth;
    ++packet.modeRevision;
    payload.attackStrength = 0;
    payload.attackBands = 0;
    send();
    RemoteInputs::beginFrame();
    clockMs += 251;
    packet.type = Type::Status;
    packet.length = 0;
    ++packet.sequence;
    accept(packet);
    RemoteInputs::beginFrame();
    assert(RemoteInputs::control().linked);
    assert(RemoteInputs::audio().volumeSm == 0);
    assert(RemoteInputs::audio().fftResult[7] == 0);

    // Production clock: ten beats take five seconds at 120 BPM. Once locked,
    // check the FULL multi-beat cycle, not ten repetitions of one beat phase.
    AudioFrameClock tempo;
    payload.tempoBpm100 = 12000;
    payload.tempoFlags = AUDIO_ACTIVE | TEMPO_LOCKED;
    float tenAtStart = 0;
    float tenAtHalf = 0;
    float tenAtEnd = 0;
    float oneAtStart = 0;
    float oneAtEnd = 0;
    for (uint32_t time = 0; time <= 25000; time += 10) {
        payload.tempoPhase = uint16_t((time % 500) * 65536u / 500);
        const auto frame = tempo.update(payload, true, time, time);
        assert(frame.beatPulse(10) >= 0 && frame.beatPulse(10) <= 1);
        assert(frame.beatAngle(0) == 0 && frame.beatPulse(0) == 0);
        if (time == 20000) {
            tenAtStart = frame.beatAngle(10);
            oneAtStart = frame.beatAngle();
            assert(fabsf(frame.bpm - 120.0f) < 0.01f);
        }
        if (time == 20500) {
            oneAtEnd = frame.beatAngle();
        }
        if (time == 22500) {
            tenAtHalf = frame.beatAngle(10);
        }
        if (time == 25000) {
            tenAtEnd = frame.beatAngle(10);
        }
    }
    assert(angleDistance(oneAtStart, oneAtEnd) < 0.01f);
    assert(angleDistance(tenAtStart, tenAtEnd) < 0.01f);
    assert(fabsf(angleDistance(tenAtHalf, tenAtStart) - 3.141592654f) < 0.01f);

    // The public pulse really rises 0 -> 1 -> 0 over the requested cycle.
    AudioFrameClock slow;
    for (uint32_t time = 0; time <= 5000; time += 50) {
        const auto frame = slow.update(payload, false, 0, time);
        if (time == 0 || time == 5000) {
            assert(frame.beatPulse() < 0.0001f);
        }
        if (time == 2500) {
            assert(frame.beatPulse() > 0.9999f);
            assert(fabsf(frame.beatAngle() - 3.141592654f) < 0.001f);
        }
    }

    // Not-ready heartbeats cannot keep restarting the idle breathing cycle.
    packet.flags = 0;
    packet.type = Type::Status;
    packet.length = 0;
    ++packet.sequence;
    accept(packet);
    RemoteInputs::beginFrame();
    const float initialIdleAngle = RemoteInputs::audio().beatAngle();
    for (unsigned i = 0; i < 10; ++i) {
        clockMs += 50;
        ++packet.sequence;
        accept(packet);
        RemoteInputs::beginFrame();
    }
    assert(angleDistance(RemoteInputs::audio().beatAngle(), initialIdleAngle) > 0.5f);

    AudioFrameClock idle;
    const auto start = idle.update(payload, false, 0, 0xFFFFFFF0u);
    const auto later = idle.update(payload, false, 0, 0x00000054u);
    assert(fabsf(later.bpm - 12.0f) < 0.001f);
    assert(later.beatAngle() > start.beatAngle());
    assert(later.volumeSm == 0);
    puts("Audio frame passed: flat API, stable reads, groups, retention/expiry, sensitivity, source reset, 120 BPM multi-beat helpers, idle and millis wrap.");
}
