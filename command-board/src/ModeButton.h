#pragma once

#include <Arduino.h>
#include "SharedProtocol.h"

// All button state belongs to SignalAcquisition's task. There is no ISR,
// second task, delay loop, or mutex involved in detecting a press or hold.
namespace CommandButtons {
    constexpr uint32_t DebounceMs = 30;

    enum class Event { None, Press };

    struct ModeButton {
        const int pin;

        explicit ModeButton(int buttonPin) : pin(buttonPin) {
        }

        // Emit once on the debounced DOWN edge. Holding a button never repeats
        // and has no alternate source-selection meaning. Disabled pins do not
        // access GPIO. Elapsed unsigned arithmetic handles millis() wrapping.
        Event poll() {
            if (pin < 0) {
                return Event::None;
            }

            const uint32_t now = millis();
            const bool down = digitalRead(pin) == LOW;

            if (down != rawDown) {
                rawDown = down;
                lastRawChange = now;
            }

            if (down != stableDown && uint32_t(now - lastRawChange) >= DebounceMs) {
                stableDown = down;
                if (stableDown) {
                    return Event::Press;
                }
            }

            return Event::None;
        }

      private:
        bool rawDown = false;
        bool stableDown = false;
        uint32_t lastRawChange = 0;
    };
} // namespace CommandButtons
