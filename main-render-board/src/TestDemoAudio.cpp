#include "TestDemo.h"
#include "PlaybackDeck.h"
#include "Audio.h"

namespace {
    struct AudioRoutine {
        bool enabled;
        const char *name;
        void (*draw)();
    };

    // THE APPROVED AUDIO DECK. Set enabled=false to leave a work-in-progress
    // routine out of unattended playback. Its Audio:: method can still exist
    // and be called directly. These entries NEVER mix with the normal deck.
    // Existing entries start enabled to preserve the current available set.
    const AudioRoutine audioRoutines[] = {
        {true, "Puddles Horizontal", &Audio::animatePuddles},
        {true, "Puddles Vertical", &Audio::animatePuddlesVertical},
        {true, "Puddles Cubes", &Audio::animatePuddlesCubes},
        {true, "Puddle Peak Horizontal", &Audio::animatePuddlePeak},
        {true, "Puddle Peak Vertical", &Audio::animatePuddlePeakVertical},
        {true, "Puddle Peak Cubes", &Audio::animatePuddlePeakCubes},
        {true, "BPM Sphere", &Audio::animateBpmSphere},
        {true, "BPM Checkerboard", &Audio::animateBpmCheckerboard},
        {true, "BPM Diagonal", &Audio::animateBpmDiagonal},
        {true, "BPM Helix", &Audio::animateBpmHelix},
        {true, "Attack Monitor", &Audio::animateAttackMonitor},
    };
    constexpr size_t AUDIO_ROUTINE_COUNT = sizeof(audioRoutines) / sizeof(audioRoutines[0]);
    TestDemo::Detail::IndexDeck<AUDIO_ROUTINE_COUNT> audioDeck;
    bool audioDeckBuilt = false;
    TestDemo::PlaybackOrder appliedOrder = TestDemo::PlaybackOrder::Sequential;

    // These are playback state, not effect state. A source/mode escape can
    // skip normal stack cleanup, so stopPlayback() explicitly cancels them.
    bool audioRunning = false;
    bool audioAdvanceRequested = false;
    uint32_t audioStartedMs = 0;

    uint16_t pickAudioIndex(uint16_t limit) {
        return uint16_t(random(long(limit)));
    }

    void ensureAudioDeck() {
        const auto order = TestDemo::audioPlayback().order;
        if (audioDeckBuilt && order == appliedOrder) {
            return;
        }
        const int preserve = audioDeckBuilt && !audioDeck.empty() ? int(audioDeck.current()) : -1;
        audioDeck.clear();
        for (size_t i = 0; i < AUDIO_ROUTINE_COUNT; ++i) {
            if (audioRoutines[i].enabled) {
                audioDeck.add(uint8_t(i));
            }
        }
        audioDeck.start(order == TestDemo::PlaybackOrder::Shuffled, pickAudioIndex, preserve);
        appliedOrder = order;
        audioDeckBuilt = true;
    }

    void advanceAudio() {
        ensureAudioDeck();
        audioDeck.advance(appliedOrder == TestDemo::PlaybackOrder::Shuffled, pickAudioIndex);
    }
}

TestDemo::AudioPlaybackSettings &TestDemo::audioPlayback() {
    static AudioPlaybackSettings settings;
    return settings;
}

const char *TestDemo::currentAudioRoutineName() {
    ensureAudioDeck();
    return audioDeck.empty() ? "No enabled audio routines" : audioRoutines[audioDeck.current()].name;
}

void TestDemo::requestNext(Deck deck) {
    if (deck == Deck::Animation) {
        Detail::advanceAnimation();
        return;
    }

    // Select immediately so even a source switch immediately after the press
    // preserves the newly selected routine. The returning old effect must NOT
    // cause another automatic advance at the same time as this manual step.
    advanceAudio();
    audioAdvanceRequested = true;
}

bool TestDemo::audioRoutineRunning() {
    if (!audioRunning) {
        return true; // Direct experimental Audio:: calls have no managed timer.
    }
    if (audioAdvanceRequested) {
        return false;
    }
    const uint32_t duration = audioPlayback().durationMs;
    return duration == 0 || uint32_t(millis() - audioStartedMs) < duration;
}

void TestDemo::stopPlayback() {
    audioRunning = false;
    audioAdvanceRequested = false;
    Detail::stopAnimation();
}

void TestDemo::runAudio() {
    Detail::stopAnimation();
    for (;;) {
        ensureAudioDeck();
        if (audioDeck.empty()) {
            // An empty approved list is a dark, responsive audio mode, not
            // an out-of-bounds call or a busy loop. showCube services the OS.
            clearAll();
            showCube();
            continue;
        }

        const AudioRoutine &routine = audioRoutines[audioDeck.current()];
        clearAll();
        audioStartedMs = millis();
        audioAdvanceRequested = false;
        audioRunning = true;
        Serial.printf("Audio routine: %s\r\n", routine.name);
        routine.draw();

        const bool manuallyAdvanced = audioAdvanceRequested;
        audioRunning = false;
        audioAdvanceRequested = false;
        if (!manuallyAdvanced) {
            // Expiry or a normal return advances once. No autonomous timing
            // thread exists: everything happens between drawing routines.
            advanceAudio();
        }
    }
}
