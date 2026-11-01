#include <Arduino.h>
#include <cassert>
#include <cstdio>
#include <string>

struct CaptureSerial {
    std::string text;
    unsigned calls = 0;
    void flush() {}
    void printf(const char *textOnly) {
        text += textOnly;
        ++calls;
    }
    template <class... T> void printf(const char *format, T... values) {
        char line[1024];
        snprintf(line, sizeof(line), format, values...);
        text += line;
        ++calls;
    }
};
static CaptureSerial capture;
#define Serial capture
#define private public
#include "BpmProfile.h"
#undef private
#undef Serial

SerialStub Serial;
SerialStub Serial2;
static uint32_t clockUs = 0;
static unsigned starts = 0;
static unsigned stops = 0;

uint32_t micros() { return clockUs; }
unsigned long millis() { return clockUs / 1000; }
RemoteInputs::Control RemoteInputs::control() { return Control(); }
RemoteInputs::Stats RemoteInputs::stats() { return Stats(); }
void bpmProfileStartDriver() { ++starts; }
BpmDriverStats bpmProfileStopDriver() { ++stops; return BpmDriverStats(); }

int main() {
    clockUs = 0xfffffff0u;
    BpmProfile::begin(BpmProfile::Test::RealQuiet, true);
    const unsigned entryCalls = capture.calls;
    BpmProfile::beginFrame();
    BpmProfile::record(BpmProfile::Stage::Snapshot, 4);
    BpmProfile::record(BpmProfile::Stage::SnapshotWait, 1);
    BpmProfile::record(BpmProfile::Stage::SnapshotHold, 2);
    BpmProfile::record(BpmProfile::Stage::Draw, 500);
    BpmProfile::record(BpmProfile::Stage::Led, 16500);
    BpmProfile::audioContext(true, 91);
    BpmProfile::spacers(2);
    clockUs += 18000; // Unsigned clock wrap during a completed frame.
    BpmProfile::completeFrame();
    assert(capture.calls == entryCalls);
    assert(BpmProfile::state().frame.count == 1);
    assert(BpmProfile::state().elapsedUs == 18000);
    assert(BpmProfile::state().badSpacerFrames == 1);
    // Snapshot wait/hold are subsets, not double-counted as extra work.
    assert(BpmProfile::state().other.total == 996);

    BpmProfile::beginFrame();
    BpmProfile::record(BpmProfile::Stage::Draw, 999999);
    clockUs += 1000;
    // Model a checkpoint longjmp: finish without completing this frame.
    BpmProfile::finish();
    assert(starts == 1 && stops == 1);
    assert(BpmProfile::state().frame.count == 1);
    assert(BpmProfile::state().stages[size_t(BpmProfile::Stage::Draw)].maximum == 500);
    assert(capture.text.find("spacerBadFrames=1") != std::string::npos);
    assert(capture.text.find("I2S probe=0") != std::string::npos);
    assert(capture.text.find("BPM_PROFILE END") != std::string::npos);
    const unsigned finishedCalls = capture.calls;
    BpmProfile::finish();
    assert(capture.calls == finishedCalls);
    puts("BPM profile passed: quiet collection, clock wrap, subset accounting, partial-frame discard and one-shot report.");
}
