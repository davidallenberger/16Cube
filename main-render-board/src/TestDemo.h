#pragma once

#include <stdint.h>

// Playback belongs here. Audio/Streamers/Surfaces contain effects; they do
// not own approved lists, selected indices, shuffles or mode-button policy.
// Everything below is owned by the render task. No UART task calls it.
namespace TestDemo {
    enum class Deck { Animation, Audio };
    enum class PlaybackOrder { Sequential, Shuffled };

    struct AudioPlaybackSettings {
        PlaybackOrder order = PlaybackOrder::Sequential;
        uint32_t durationMs = 30000; // 0 holds until the next button press.
    };

    // Audio has its OWN approved list, order, timer and remembered selection.
    // It never enters the art/billboard decks. Changing order keeps the current
    // routine and applies the new ordering to subsequent selection.
    AudioPlaybackSettings &audioPlayback();
    PlaybackOrder &animationOrder(); // Defaults to existing shuffled playback.
    void runAudio();
    void runAnimation();
    const char *currentAudioRoutineName();

    // CubeSystem calls this for another press of the active mode button.
    // Audio advances once, then its effect returns at the next loop boundary.
    // Normal animation keeps its existing same-task OS escape mechanism.
    void requestNext(Deck deck);

    // Used by each audio effect's drawing loop. TestDemo alone interprets the
    // deadline/button flag; the effect just returns when it becomes false.
    // Outside runAudio(), a direct effect call is untimed, useful in development.
    bool audioRoutineRunning();

    // OS calls this before leaving/restarting an app. Clear the active timer,
    // not the remembered selection. Mode/source changes are never "next".
    void stopPlayback();

    void runHardwareWiringTest();
    // Existing entry point retained for deliberate ordered regression runs.
    // false uses the normal weighted decks; true uses ordered ten-second tests.
    void runEngine(bool isTestMode);
}
