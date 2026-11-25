#pragma once

#include <stddef.h>
#include <stdint.h>

// Playback belongs here. Audio/Streamers/Surfaces contain effects; they do
// not own approved lists, selected indices, shuffles or mode-button policy.
// Everything below is owned by the render task. No UART task calls it.
namespace TestDemo {
    // Shared temporary controls for the adjacent arm-shape auditions.
    struct ArmComparison {
        static constexpr float tumblePhaseRate=20.0f;
        static constexpr uint32_t spinMs=3500;
        static constexpr uint32_t colorCycleMs=18000;
        static constexpr uint32_t frameMs=40;
    };
    // Temporary debug latch, owned by the render task. Holding keeps motion
    // and live audio running; only a routine's automatic exit is suppressed.
    // Manual next and mode changes remain available. Releasing an expired
    // routine lets it finish at its next ordinary exit check.
    inline bool &playbackHeld() {
        static bool held = false;
        return held;
    }

    // Development default: keep each selected animation running until Next.
    // Separate from the command-board Hold button and scoped to animation only.
    inline bool &animationHoldUntilNext() {
        static bool enabled = true;
        return enabled;
    }

    namespace Detail {
        inline bool &animationDurationHeld() {
            static bool active = false;
            return active;
        }
    }

    inline bool routineTimeRemaining(uint32_t elapsedMs, uint32_t durationMs) {
        return playbackHeld() || Detail::animationDurationHeld() || elapsedMs < durationMs;
    }

    enum class Deck { Animation, Audio };
    enum class PlaybackOrder { Sequential, Descending, Shuffled };

    struct AudioPlaybackSettings {
        PlaybackOrder order = PlaybackOrder::Descending;
        uint32_t durationMs = 30000; // 0 holds until the next button press.
    };

    // Audio has its OWN approved list, order, timer and remembered selection.
    // It never enters the art/billboard decks. Changing order keeps the current
    // routine and applies the new ordering to subsequent selection.
    AudioPlaybackSettings &audioPlayback();
    PlaybackOrder &animationOrder(); // Descending ordered tests start at the last position in the ordered deck.
    void runAudio();
    void runAnimation();
    const char *currentAudioRoutineName();

    // CubeSystem calls this for another press of the active mode button.
    // Audio advances once, then its effect returns at the next loop boundary.
    // Normal animation keeps its existing same-task OS escape mechanism.
    void requestNext(Deck deck);
    // Temporary (Game button in audio modes): step back one audio routine.
    void requestPrevious();

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

// PRIVATE TestDemo building blocks. No task, allocation, timer or audio
// detection lives here. Both the normal weighted decks and the audio deck
// use this same Fisher-Yates shuffle. The caller supplies its random picker.
namespace TestDemo {
    namespace Detail {
        template <typename PickIndex>
        void shuffleDeck(uint8_t *items, size_t count, PickIndex pickIndex) {
            for (size_t remaining = count; remaining > 1; --remaining) {
                const size_t other = pickIndex(uint16_t(remaining));
                const uint8_t saved = items[remaining - 1];
                items[remaining - 1] = items[other];
                items[other] = saved;
            }
        }

        // A bounded bag of approved routine IDs, not animation objects. Add
        // only enabled IDs. Sequential order visits those IDs in list order;
        // shuffled order visits every ID once before reshuffling the bag.
        template <size_t Capacity>
        class IndexDeck {
          public:
            static_assert(Capacity > 0 && Capacity <= 256, "Deck capacity must fit byte IDs");

            void clear() {
                count = 0;
                position = 0;
            }

            bool add(uint8_t id) {
                if (count >= Capacity) {
                    return false;
                }
                items[count++] = id;
                return true;
            }

            bool empty() const {
                return count == 0;
            }

            uint8_t current() const {
                return empty() ? 0 : items[position];
            }

            template <typename PickIndex>
            void start(bool shuffled, PickIndex pickIndex, int preserveId = -1) {
                position = 0;
                if (shuffled) {
                    shuffleDeck(items, count, pickIndex);
                }
                for (size_t i = 0; i < count; ++i) {
                    if (int(items[i]) == preserveId) {
                        if (shuffled) {
                            // Start the new shuffle cycle with the current
                            // routine, then visit every other enabled entry.
                            const uint8_t saved = items[0];
                            items[0] = items[i];
                            items[i] = saved;
                        } else {
                            position = i;
                        }
                        break;
                    }
                }
            }

            template <typename PickIndex>
            void advance(bool shuffled, PickIndex pickIndex) {
                if (empty()) {
                    return;
                }
                const uint8_t previous = current();
                ++position;
                if (position >= count) {
                    position = 0;
                    if (shuffled) {
                        shuffleDeck(items, count, pickIndex);
                        if (count > 1 && items[0] == previous) {
                            const uint8_t saved = items[0];
                            items[0] = items[1];
                            items[1] = saved;
                        }
                    }
                }
            }

            // One step back in the current visiting order (no reshuffle).
            void retreat() {
                if (empty()) {
                    return;
                }
                position = position == 0 ? count - 1 : position - 1;
            }

          private:
            uint8_t items[Capacity] = {};
            size_t count = 0;
            size_t position = 0;
        };

        // Implemented beside the existing normal animation engine. Its
        // shuffled cursor already moves before drawing; ordered playback
        // accepts an explicit step both during drawing and between app calls.
        void advanceAnimation();
        void stopAnimation();
    }
}
