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
    void drawRandomCornerBurst() {
        void (*const shapes[])() = {
            &Audio::animateCornerBurst, &Audio::animateCornerBurstSphere,
            &Audio::animateCornerBurstBox
        };
        shapes[random(3)]();
    }

    void drawRandomBouncingBalls() {
        using Pattern = Audio::BouncingBallsPattern;
        Audio::bouncingBalls(random(2) == 0 ? Pattern::Clockwise : Pattern::OrbitInward);
    }

    void drawRandomGeq() {
        if (random(3) < 2) Audio::animateSurroundGEQ();
        else Audio::animateInwardGEQ();
    }

    void drawRandomParticleGeq() {
        void (*const layouts[])() = {
            &Audio::animateParticleGeqClusters,
            &Audio::animateParticleGeqTiles,
            &Audio::animateParticleGeqPairedBands
        };
        layouts[random(sizeof(layouts) / sizeof(layouts[0]))]();
    }

    int frequencyPlaneLook = -1;

    bool useFrequencyDiscs() {
        if (frequencyPlaneLook < 0) frequencyPlaneLook = int(random(2));
        return frequencyPlaneLook == 1;
    }

    void drawFrequencyPlanes() {
        Audio::frequencyPlanes(useFrequencyDiscs());
    }

    void drawRandomFreqwave() {
        using Shape = Audio::FreqwaveShape;
        Audio::freqwave(random(2) == 0 ? Shape::WireframeBox : Shape::Sphere);
    }

    void drawRandomAudioShape() {
        if (random(2) == 0) Audio::animateCylinder();
        else Audio::animateOctahedron();
    }

    // AUDITION PAIRS (2026-11-20). A = the accepted look, B = the trial.
    // Both cards of a pair use the same shape/pattern: A picks it on entry
    // and B reuses it, so pressing Next from A to B changes only the trial.
    // Listed last so descending playback starts with them, A before B.
    int auditionVariant[4] = {-1, -1, -1, -1};

    uint8_t auditionPick(int pair, long choices, bool fresh) {
        if (fresh || auditionVariant[pair] < 0) auditionVariant[pair] = int(random(choices));
        return uint8_t(auditionVariant[pair]);
    }

    void drawCornerBurstA() { Audio::cornerBurst(auditionPick(0, 3, true), false); }
    void drawCornerBurstB() { Audio::cornerBurst(auditionPick(0, 3, false), true); }

    const BpmSpatialMode breatheModes[] = {BPM_SPHERE, BPM_CHECKERBOARD, BPM_DIAGONAL, BPM_HELIX};
    void drawBreatheA() {
        Audio::bpm(breatheModes[auditionPick(1, 4, true)], Audio::controls().bpmSensitivity, false);
    }
    void drawBreatheB() {
        Audio::bpm(breatheModes[auditionPick(1, 4, false)], Audio::controls().bpmSensitivity, true);
    }

    void drawFrequencyColorA() { Audio::frequencyPlanes(auditionPick(2, 2, true) == 1, false); }
    void drawFrequencyColorB() { Audio::frequencyPlanes(auditionPick(2, 2, false) == 1, true); }

    void drawParticleGeqA() { Audio::particleGeq(auditionPick(3, 3, true), false); }
    void drawParticleGeqB() { Audio::particleGeq(auditionPick(3, 3, false), true); }

    const AudioRoutine audioRoutines[] = {
        {true, "Julia Volume", &Audio::animateJulia},
        {true, "Waverly", &drawWaverly},

        {true, "Puddles", &Audio::animatePuddles},
        {true, "Puddle Peak", &Audio::animatePuddlePeak},
        {true, "Breathe", &drawBreathe},
        {false, "Attack Monitor", &Audio::animateAttackMonitor},
        {true, "Corner Burst", &drawRandomCornerBurst},
        {true, "Drift Rose Volume", &Audio::animateDriftRose},
        {true, "Plasmoid Depth Mirror", &Audio::animatePlasmoidMirrored},
        {true, "Bouncing Balls Audio", &drawRandomBouncingBalls},
        {true, "Funky Plank", &Audio::animateFunkyPlank},
        {true, "GEQ", &drawRandomGeq},
        {true, "Frequency Planes / Discs", &drawFrequencyPlanes},
        {true, "Freqwave", &drawRandomFreqwave},
        {true, "PS GEQ", &drawRandomParticleGeq},
        {true, "Frequency Cubes 8 Bands Corner", &Audio::frequencyCubes},
        {true, "Audio Shapes", &drawRandomAudioShape},

        // Audition pairs, in reverse because descending playback starts at
        // the end: Corner Burst A, B, Breathe A, B, ... then Nova.
        {true, "Trial PS GEQ Nova (new)", &Audio::animateParticleGeqNova},
        {true, "Trial PS GEQ B (liquid)", &drawParticleGeqB},
        {true, "Trial PS GEQ A (now)", &drawParticleGeqA},
        {true, "Trial Frequency Planes B (rainbow + lightness)", &drawFrequencyColorB},
        {true, "Trial Frequency Planes A (now)", &drawFrequencyColorA},
        {true, "Trial Breathe B (sound color)", &drawBreatheB},
        {true, "Trial Breathe A (now)", &drawBreatheA},
        {true, "Trial Corner Burst B (peak-sized)", &drawCornerBurstB},
        {true, "Trial Corner Burst A (now)", &drawCornerBurstA},
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
        if (audioDeck.empty()) return;
        const auto previous = audioRoutines[audioDeck.current()].draw;
        if (previous == &drawWaverly) waverlyLookSelected = false;
        if (previous == &drawBreathe) breathePatternSelected = false;
        if (previous == &drawFrequencyPlanes) frequencyPlaneLook = -1;
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
    if (audioRoutines[audioDeck.current()].draw == &drawFrequencyPlanes) {
        return useFrequencyDiscs() ? "Frequency Discs" : "Frequency Planes";
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
