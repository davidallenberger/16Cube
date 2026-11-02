#include <Arduino.h>
#include <vector>
#include <cassert>
#include <cstdio>

// Capture the actual production task's writes, without replacing its packet
// construction, scheduling decisions, or treatment of mode demands.
struct CaptureUart {
    size_t setTxBufferSize(size_t count) { return count; }
    size_t setRxBufferSize(size_t count) { return count; }
    template <class... T> void begin(T...) {}
    size_t write(uint8_t) { return 1; }
    size_t write(uint8_t *bytes, size_t length);
};
static CaptureUart capturedUart;
#define Serial2 capturedUart
#define CUBINA_TEST_TASK_LIFECYCLE
#include "UARTSender.cpp"
#undef Serial2

SerialStub Serial;
SerialStub Serial2;

enum class Scenario { Audio, Game, Depth, Demands, BlockedWrite, Wrap };
static Scenario scenario = Scenario::Audio;
static uint64_t clockUs = 0;
static uint64_t startedUs = 0;
static uint64_t durationUs = 0;
static unsigned ticks = 0;
static bool injectedBlock = false;
static std::vector<uint64_t> writtenAt;
static std::vector<CubinaWire::Packet> packets;

struct Finished {
};

unsigned long millis() {
    return uint32_t(clockUs / 1000);
}

uint32_t micros() {
    return uint32_t(clockUs);
}

int xPortGetCoreID() {
    return 0;
}

BaseType_t testCreatePinnedTask(void (*)(void *), int) {
    return pdPASS;
}

void testTaskDelay(unsigned) {
    clockUs += 1000;
    if (++ticks % 73 == 0) {
        clockUs += 4300; // Scheduler jitter must not be repaid with a burst.
    }
    if (clockUs - startedUs >= durationUs) {
        throw Finished();
    }
}

CommandSnapshot CommandState::snapshot() {
    CommandSnapshot state;
    state.session = 123;
    state.revision = 1;
    state.ready = true;
    state.capturedMs = millis();
    state.mode = CubinaWire::Mode::AudioBluetooth;
    state.audio.mode = 2;

    if (scenario == Scenario::Game) {
        state.mode = CubinaWire::Mode::Game;
    } else if (scenario == Scenario::Depth) {
        state.mode = CubinaWire::Mode::Depth;
    } else if (scenario == Scenario::Demands) {
        state.revision = 1 + uint32_t((clockUs - startedUs) / 2000);
        state.mode = state.revision % 2 ? CubinaWire::Mode::Game : CubinaWire::Mode::Animation;
    }
    state.pads[0].connected = true;
    state.presses[0] = 1;
    return state;
}

CommandSnapshot CommandState::takeForSend(bool) {
    return snapshot();
}

size_t CaptureUart::write(uint8_t *bytes, size_t length) {
    if (scenario == Scenario::BlockedWrite && !injectedBlock && packets.size() >= 6) {
        // Simulate a write that waits for 250 ms and initially accepts nothing.
        // Scheduling from a timestamp taken before this wait is unsafe.
        injectedBlock = true;
        clockUs += 250000;
        return 0;
    }

    CubinaWire::Packet decoded;
    assert(length > 1 && bytes[length - 1] == 0);
    assert(CubinaWire::decode(bytes, length - 1, decoded));
    writtenAt.push_back(clockUs);
    packets.push_back(decoded);
    clockUs += 90; // Time used while submitting bytes to the fake driver.
    return length;
}

static void exercise(Scenario next, uint64_t start = 0) {
    scenario = next;
    clockUs = start;
    startedUs = start;
    durationUs = 2000000;
    ticks = 0;
    injectedBlock = false;
    writtenAt.clear();
    packets.clear();

    try {
        task(nullptr);
        assert(false && "Sender task unexpectedly returned");
    } catch (const Finished &) {
    }

    assert(packets.size() > 10);
    uint64_t lastDepth = 0;
    for (size_t index = 0; index < writtenAt.size(); ++index) {
        if (index) {
            assert(writtenAt[index] - writtenAt[index - 1] >= 16667);
        }
        if (packets[index].type == CubinaWire::Type::Depth) {
            if (lastDepth) {
                assert(writtenAt[index] - lastDepth >= 33334);
            }
            lastDepth = writtenAt[index];
        }
        if (packets[index].type == CubinaWire::Type::Gamepads) {
            assert(CubinaWire::get32(packets[index].payload + 30) == 1);
        }
    }
    if (next == Scenario::BlockedWrite) {
        assert(injectedBlock);
    }
    assert(packets.size() <= 120);
}

int main() {
    exercise(Scenario::Audio);
    exercise(Scenario::Game);
    exercise(Scenario::Depth);
    exercise(Scenario::Demands);
    exercise(Scenario::BlockedWrite);
    exercise(Scenario::Wrap, 0xffff0000ull);
    puts("Production sender task passed: all packets >=16,667 us apart, mode storms, "
         "game press payloads, depth cadence, blocked writes, and micros wrap.");
}
