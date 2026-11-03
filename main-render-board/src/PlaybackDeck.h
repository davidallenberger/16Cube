#pragma once

#include <stddef.h>
#include <stdint.h>

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

          private:
            uint8_t items[Capacity] = {};
            size_t count = 0;
            size_t position = 0;
        };

        // Implemented beside the existing normal animation engine. Its
        // shuffled cursor already moves before drawing; ordered playback
        // needs an explicit step when the OS abandons a scene on a button.
        void advanceAnimation();
        void stopAnimation();
    }
}
