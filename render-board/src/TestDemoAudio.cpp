#include "TestDemo.h"
#include "Audio.h"

namespace {
    struct AudioRoutine {
        bool enabled;
        const char *name;
        void (*draw)();
    };

    // Approved Waverly looks: Rainbow and Party for each palette-driven
    // mechanism, plus Hiphotic's native colors. One look is chosen per visit.
    struct WaverlyLook {
        const char *name;
        ShaderMode shader;
        uint8_t palette;
        bool columnGradient;
    };

    const WaverlyLook waverlyLooks[] = {
        {"Waverly Gradient Cycle Rainbow", MODE_LINEAR_FLOW, 0, true},
        {"Waverly Gradient Cycle Party", MODE_LINEAR_FLOW, 4, true},
        {"Waverly Linear Flow Rainbow", MODE_LINEAR_FLOW, 0, false},
        {"Waverly Linear Flow Party", MODE_LINEAR_FLOW, 4, false},
        {"Waverly Noise Field Rainbow", MODE_NOISE_FIELD, 0, false},
        {"Waverly Noise Field Party", MODE_NOISE_FIELD, 4, false},
        {"Waverly Distance Field Rainbow", MODE_DISTANCE_FIELD, 0, false},
        {"Waverly Distance Field Party", MODE_DISTANCE_FIELD, 4, false},
        {"Waverly Distortion Waves Rainbow", MODE_DISTORTION_WAVES, 0, false},
        {"Waverly Distortion Waves Party", MODE_DISTORTION_WAVES, 4, false},
        {"Waverly Hiphotic Native", MODE_HIPHOTIC, 0, false},
    };
    constexpr size_t WAVERLY_LOOK_COUNT = sizeof(waverlyLooks) / sizeof(waverlyLooks[0]);
    uint8_t waverlyLook = 0;
    bool waverlyLookSelected = false;

    void ensureWaverlyLook() {
        if (!waverlyLookSelected) {
            // Equal chances for six mechanisms; Hiphotic ignores palettes.
            // Keep all approved palettes available to the other mechanisms.
            const uint8_t mechanism = uint8_t(random(6));
            waverlyLook = mechanism == 5 ? 10 : mechanism * 2 + uint8_t(random(2));
            waverlyLookSelected = true;
        }
    }

    // One registered wrapper supplies the chosen look to the single drawing
    // function. Palette objects stay local; the table stores only small IDs.
    void drawWaverly() {
        ensureWaverlyLook();
        const WaverlyLook &look = waverlyLooks[waverlyLook];
        CRGBPalette16 palette = RainbowColors_p;
        if (look.palette == 4) {
            palette = PartyColors_p;
        }
        // Use the normal managed duration, Next and Hold policy.
        Audio::waverly(look.shader, palette, look.columnGradient);
    }

    // One breathing card, with equal chances for each spatial pattern.
    // Remember the choice across source/mode restarts, just like Waverly.
    const AudioRoutine breathePatterns[] = {
        {true, "Breathe Sphere", &Audio::animateBpmSphere},
        {true, "Breathe Checkerboard", &Audio::animateBpmCheckerboard},
        {true, "Breathe Diagonal", &Audio::animateBpmDiagonal},
        {true, "Breathe Helix", &Audio::animateBpmHelix},
    };
    constexpr size_t BREATHE_PATTERN_COUNT = sizeof(breathePatterns) / sizeof(breathePatterns[0]);
    uint8_t breathePattern = 0;
    bool breathePatternSelected = false;

    void ensureBreathePattern() {
        if (!breathePatternSelected) {
            breathePattern = uint8_t(random(long(BREATHE_PATTERN_COUNT)));
            breathePatternSelected = true;
        }
    }

    void drawBreathe() {
        ensureBreathePattern();
        breathePatterns[breathePattern].draw();
    }

    // THE APPROVED AUDIO DECK. Set enabled=false to leave a work-in-progress
    // routine out of unattended playback. Its Audio:: method can still exist
    // and be called directly. These entries NEVER mix with the normal deck.
    // Descending playback starts at the last enabled card after a flash.
    const AudioRoutine audioRoutines[] = {
        {true, "Julia Volume", &Audio::animateJulia},
        {true, "Waverly", &drawWaverly},

        {true, "Puddles", &Audio::animatePuddles},
        {true, "Puddle Peak", &Audio::animatePuddlePeak},
        {true, "Breathe", &drawBreathe},
        {false, "Attack Monitor", &Audio::animateAttackMonitor},
        {true, "Surround GEQ", &Audio::animateSurroundGEQ},
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
        for (size_t offset = 0; offset < AUDIO_ROUTINE_COUNT; ++offset) {
            const size_t i = order == TestDemo::PlaybackOrder::Descending ?
                AUDIO_ROUTINE_COUNT - 1 - offset : offset;
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
        if (!audioDeck.empty() && audioRoutines[audioDeck.current()].draw == &drawWaverly) {
            // Next/expiry leaves Waverly after one look. Select afresh on its
            // next visit; source/mode restarts keep the remembered choice.
            waverlyLookSelected = false;
        }
        if (!audioDeck.empty() && audioRoutines[audioDeck.current()].draw == &drawBreathe) {
            breathePatternSelected = false;
        }
        audioDeck.advance(appliedOrder == TestDemo::PlaybackOrder::Shuffled, pickAudioIndex);
    }
}

TestDemo::AudioPlaybackSettings &TestDemo::audioPlayback() {
    static AudioPlaybackSettings settings;
    return settings;
}

const char *TestDemo::currentAudioRoutineName() {
    ensureAudioDeck();
    if (audioDeck.empty()) {
        return "No enabled audio routines";
    }
    if (audioRoutines[audioDeck.current()].draw == &drawWaverly) {
        ensureWaverlyLook();
    }
    if (audioRoutines[audioDeck.current()].draw == &drawBreathe) {
        ensureBreathePattern();
        return breathePatterns[breathePattern].name;
    }
    return audioRoutines[audioDeck.current()].draw == &drawWaverly ?
        waverlyLooks[waverlyLook].name : audioRoutines[audioDeck.current()].name;
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
    return duration == 0 || routineTimeRemaining(uint32_t(millis() - audioStartedMs), duration);
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
        Serial.printf("Audio routine: %s\r\n", currentAudioRoutineName());
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
