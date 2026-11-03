#include "TestDemoAudio.cpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

SerialStub Serial;
SerialStub Serial2;
static uint32_t clockMs = 0;
static uint32_t randomState = 1;
static std::vector<unsigned> launches;
static std::vector<uint32_t> durations;
static unsigned animationRequests = 0;
static unsigned normalStops = 0;
static unsigned launchLimit = 0;
static bool manualFirst = false;
static bool toggleHold = false;
static bool buttonAtDeadline = false;

struct StopTest {
};

unsigned long millis() {
    return clockMs;
}

long random(long upper) {
    assert(upper > 0);
    randomState = randomState * 1664525u + 1013904223u;
    return long((randomState >> 8) % uint32_t(upper));
}

void clearAll() {
}

void showCube() {
    assert(false && "The approved deck should not be empty in this fixture");
}

void TestDemo::Detail::advanceAnimation() {
    ++animationRequests;
}

void TestDemo::Detail::stopAnimation() {
    ++normalStops;
}

static void draw(unsigned id) {
    if (launches.size() >= launchLimit) {
        throw StopTest();
    }
    launches.push_back(id);
    const uint32_t start = clockMs;
    bool pressed = false;
    while (TestDemo::audioRoutineRunning()) {
        clockMs += 10;
        if (toggleHold && launches.size() == 1) {
            TestDemo::playbackHeld() = uint32_t(clockMs - start) < 120;
        }
        if (!pressed && launches.size() == 1 &&
            ((manualFirst && uint32_t(clockMs - start) >= 20) ||
             (buttonAtDeadline && uint32_t(clockMs - start) >= TestDemo::audioPlayback().durationMs))) {
            TestDemo::requestNext(TestDemo::Deck::Audio);
            pressed = true;
        }
        assert(uint32_t(clockMs - start) < 1000 && "Player failed to end effect");
    }
    durations.push_back(uint32_t(clockMs - start));
}

void Audio::animatePuddles() {
    draw(0);
}

void Audio::animatePuddlesVertical() {
    draw(1);
}

void Audio::animatePuddlesCubes() {
    draw(2);
}

void Audio::animatePuddlePeak() {
    draw(3);
}

void Audio::animatePuddlePeakVertical() {
    draw(4);
}

void Audio::animatePuddlePeakCubes() {
    draw(5);
}

void Audio::animateBpmSphere() {
    draw(6);
}

void Audio::animateBpmCheckerboard() {
    draw(7);
}

void Audio::animateBpmDiagonal() {
    draw(8);
}

void Audio::animateBpmHelix() {
    draw(9);
}

void Audio::animateAttackMonitor() {
    draw(10);
}

static void reset(TestDemo::PlaybackOrder order = TestDemo::PlaybackOrder::Sequential) {
    TestDemo::stopPlayback();
    audioDeckBuilt = false;
    TestDemo::audioPlayback().order = order;
    TestDemo::audioPlayback().durationMs = 50;
    launches.clear();
    durations.clear();
    clockMs = 0;
    TestDemo::playbackHeld() = false;
    toggleHold = false;
    manualFirst = false;
    buttonAtDeadline = false;
}

static void play(unsigned count) {
    launchLimit = count;
    try {
        TestDemo::runAudio();
        assert(false && "Player returned unexpectedly");
    } catch (const StopTest &) {
    }
}

int main() {
    using namespace TestDemo;
    reset();
    assert(std::strcmp(currentAudioRoutineName(), "Puddles Horizontal") == 0);
    std::vector<unsigned> enabled;
    for (unsigned i = 0; i < AUDIO_ROUTINE_COUNT; ++i) {
        if (audioRoutines[i].enabled) {
            enabled.push_back(i);
        }
    }
    const unsigned count = enabled.size();
    play(count + 2);
    for (unsigned i = 0; i < launches.size(); ++i) {
        assert(launches[i] == enabled[i % count]);
        assert(durations[i] == 50);
    }

    // A press is one step, not a step plus another timed advance on return.
    reset();
    manualFirst = true;
    play(3);
    assert(launches == std::vector<unsigned>({0, 1, 2}));
    assert(durations[0] == 20 && durations[1] == 50);
    reset();
    buttonAtDeadline = true;
    play(3);
    assert(launches == std::vector<unsigned>({0, 1, 2}));

    // Hold mode has no deadline; the same button still ends the routine.
    reset();
    audioPlayback().durationMs = 0;
    manualFirst = true;
    play(1);
    assert(durations[0] == 20);
    assert(launches[0] == 0);

    // Boolean Hold outlives a deadline while motion continues. Release lets
    // the expired routine finish; manual Next still overrides Hold.
    reset();
    toggleHold = true;
    play(2);
    assert(durations[0] == 120 && durations[1] == 50);
    reset();
    playbackHeld() = true;
    manualFirst = true;
    play(1);
    assert(durations[0] == 20);
    assert(routineTimeRemaining(500, 50));
    playbackHeld() = false;
    assert(!routineTimeRemaining(500, 50));
    assert(routineTimeRemaining(20, 50));

    // A source/mode escape clears timers without rewinding the selection.
    // A just-applied manual step is retained even if a switch follows it.
    reset();
    requestNext(Deck::Audio);
    stopPlayback();
    assert(std::strcmp(currentAudioRoutineName(), "Puddles Vertical") == 0);
    assert(audioRoutineRunning()); // A direct experimental call is untimed.
    play(1);
    assert(launches[0] == 1);

    reset(PlaybackOrder::Shuffled);
    play(count * 3);
    for (unsigned cycle = 0; cycle < 3; ++cycle) {
        std::vector<unsigned> bag(launches.begin() + cycle * count, launches.begin() + (cycle + 1) * count);
        std::sort(bag.begin(), bag.end());
        for (unsigned i = 0; i < count; ++i) {
            assert(bag[i] == enabled[i]);
        }
        if (cycle > 0) {
            assert(launches[cycle * count - 1] != launches[cycle * count]);
        }
    }

    // Changing order preserves the selected entry rather than jumping to 0.
    reset();
    requestNext(Deck::Audio);
    requestNext(Deck::Audio);
    audioPlayback().order = PlaybackOrder::Shuffled;
    assert(std::strcmp(currentAudioRoutineName(), "Puddles Cubes") == 0);
    audioPlayback().order = PlaybackOrder::Sequential;
    assert(std::strcmp(currentAudioRoutineName(), "Puddles Cubes") == 0);

    // Selector tests use approved IDs 0 and 2 only: disabled/unlisted ID 1
    // cannot leak in. Empty/single-entry/capacity limits stay well defined.
    Detail::IndexDeck<2> approved;
    assert(approved.empty());
    assert(approved.add(0));
    assert(approved.add(2));
    assert(!approved.add(1));
    auto pick = [](uint16_t limit) -> uint16_t { return limit - 1; };
    approved.start(false, pick);
    assert(approved.current() == 0);
    approved.advance(false, pick);
    assert(approved.current() == 2);
    approved.advance(false, pick);
    assert(approved.current() == 0);
    approved.clear();
    approved.advance(true, pick);
    assert(approved.empty());
    approved.add(7);
    approved.start(true, pick);
    approved.advance(true, pick);
    assert(approved.current() == 7);

    // The shared shuffle preserves multiplicities in a weighted normal deck.
    uint8_t weighted[] = {0, 0, 0, 1, 2, 2};
    Detail::shuffleDeck(weighted, 6, [](uint16_t) -> uint16_t { return 0; });
    std::sort(weighted, weighted + 6);
    const uint8_t expected[] = {0, 0, 0, 1, 2, 2};
    assert(std::equal(weighted, weighted + 6, expected));

    // Manual audio selection never requests an animation-deck advance.
    animationRequests = 0;
    requestNext(Deck::Audio);
    assert(animationRequests == 0);
    requestNext(Deck::Animation);
    assert(animationRequests == 1);

    reset();
    clockMs = 0xFFFFFFF0u;
    play(2);
    assert(durations[0] == 50 && durations[1] == 50);
    puts("TestDemo audio deck passed: ordered/shuffled cycles, approved IDs, timers/hold, immediate single-step, deadline collision, source preservation, deck separation, clock wrap.");
}
