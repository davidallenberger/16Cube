#pragma once

#include <stdint.h>
#include <math.h>

namespace RemoteInputs {
    namespace Detail {
        class AudioFrameClock;
    }

    // This is the COMPLETE audio interface for an animation. Obtain one copy
    // with RemoteInputs::audio() and use it throughout the frame. It contains
    // no wire payload, packet timestamps, event counters, or detector flags.
    // The separate Audio class is the collection of animation routines.
    class AudioFrame {
      public:
        uint8_t volumeRaw = 0; // Immediate level, 0..255.
        uint8_t volumeSm = 0;  // Smoothed level: the usual artistic control.
        uint8_t volumeAvg = 0; // Longer-term average level, 0..255.
        uint8_t fftResult[16] = {}; // Low to high frequency, each 0..255.

        // Usable, smoothly changing motion speed. Silence/disconnection uses
        // a slow 12 BPM; audible music without an estimate starts at 60 BPM.
        // This is not a promise that a provisional musical estimate is right.
        float bpm = 12.0f;

        // Notices of actual sound changes, NOT predicted tempo ticks. These
        // remain unchanged for the entire rendered frame; reading twice does
        // not consume them. A broad sound may set several at the same time.
        bool bassKick = false;
        bool midAttack = false;
        bool highAttack = false;

        // One revolution every N beats. Example: beatAngle(10) at 120 BPM
        // takes five seconds to travel from 0 to TWO_PI and start again.
        // An invalid zero period returns zero. The internal beat count keeps
        // multi-beat cycles continuous across individual beat boundaries.
        // It has an arbitrary musical origin: this does not detect bar starts.
        float beatAngle(uint16_t beatsPerCycle = 1) const {
            if (beatsPerCycle == 0) {
                return 0.0f;
            }
            const float position =
                (float(wholeBeats % beatsPerCycle) + beatFraction) / float(beatsPerCycle);
            return position * 6.283185307f;
        }

        // Smooth 0 -> 1 -> 0, once every N beats. No drawing, volume scaling,
        // event detection, or clock advancement happens in this helper.
        // Call once outside voxel loops and reuse its result when possible.
        float beatPulse(uint16_t beatsPerCycle = 1) const {
            return 0.5f - 0.5f * cosf(beatAngle(beatsPerCycle));
        }

      private:
        // Only the render-side adapter can set the timing of a snapshot.
        // Keeping integer beats separate avoids float precision loss during
        // days of unattended playback. Animations never manage this state.
        friend class Detail::AudioFrameClock;
        uint32_t wholeBeats = 0;
        float beatFraction = 0.0f;
    };
}
