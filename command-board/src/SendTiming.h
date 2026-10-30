#pragma once

#include <stdint.h>

// Transmission cadence is independent of audio capture and LED rendering.
// Audio and games target 60 updates/second. Depth stays at 30 updates/second.
// Periods are rounded upward to whole microseconds; 16,667 us is almost 60 Hz.
namespace SendTiming {
    constexpr uint32_t AudioPeriodUs = 16667;
    constexpr uint32_t GamePeriodUs = 16667;
    constexpr uint32_t DepthPeriodUs = 33334;
    constexpr uint32_t StatusPeriodUs = 200000;

    // A task may wake a little later than requested. Scheduling each update
    // from its ACTUAL wake time would add that lateness repeatedly and reduce
    // the average rate. Instead, preserve the intended sequence of deadlines.
    //
    // If an entire additional slot has already been missed, start a new
    // deadline from now. Do not replay a backlog or send catch-up bursts.
    // Unsigned subtraction handles ordinary micros() counter wrap; callers
    // must service/check the clock within half the uint32_t range (~36 min).
    class Deadline {
      public:
        bool due (uint32_t now) const {
            return !armed || int32_t (now - nextDue) >= 0;
        }

        void advance (uint32_t now, uint32_t period) {
            if (!armed) {
                restart (now, period);
                return;
            }

            nextDue += period;

            if (int32_t (now - nextDue) >= 0) {
                restart (now, period);
            }
        }

        void restart (uint32_t now, uint32_t period) {
            armed = true;
            nextDue = now + period;
        }

      private:
        bool armed = false;
        uint32_t nextDue = 0;
    };
} // namespace SendTiming
