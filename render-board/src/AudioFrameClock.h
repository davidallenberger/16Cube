#pragma once

// INTERNAL render adapter. Animation files include AudioFrame.h through
// UARTReceiver.h instead of this file. All interpretation of wire timing and
// retained events belongs here; no animation should repeat these decisions.
#include "AudioFrame.h"
#include "SharedProtocol.h"
#include <math.h>

namespace RemoteInputs {
    namespace Detail {
        class AudioFrameClock {
          public:
            void reset() {
                *this = AudioFrameClock();
            }

            AudioFrame update(const AudioPayload &payload, bool fresh, uint32_t receivedMs,
                              uint32_t now) {
                AudioFrame frame;
                const float seconds = started
                    ? fminf(0.25f, float(uint32_t(now - lastFrameMs)) / 1000.0f)
                    : 0.0f;
                lastFrameMs = now;
                started = true;

                const bool active = fresh && (payload.tempoFlags & CubinaWire::AUDIO_ACTIVE);
                const bool locked = active && (payload.tempoFlags & CubinaWire::TEMPO_LOCKED);
                const float estimate = float(payload.tempoBpm100) / 100.0f;
                const float targetRate = !active ? 12.0f : estimate >= 60.0f ? estimate : 60.0f;

                // Integrate speed, rather than multiplying millis() by a new
                // BPM each frame. A changed estimate must not teleport motion.
                rate += (1.0f - expf(-seconds / 0.5f)) * (targetRate - rate);
                advance(seconds * rate / 60.0f);

                if (locked) {
                    // Transmitted phase describes the latest audio packet.
                    // Extrapolate using its LOCAL receipt age. Never subtract
                    // the command clock from the render clock.
                    const float age = float(uint32_t(now - receivedMs)) / 1000.0f;
                    float target = float(payload.tempoPhase) / 65536.0f + age * estimate / 60.0f;
                    target -= floorf(target);
                    float error = target - fraction;
                    error -= floorf(error + 0.5f);
                    if (previouslyLocked) {
                        advance(error * (1.0f - expf(-seconds / 0.15f)));
                    } else {
                        // Establish the first beat position without inventing
                        // a negative beat number near the zero boundary.
                        fraction = target;
                    }
                }
                previouslyLocked = locked;

                // Stale input is neutral, with only the idle motion clock
                // left running. No animation needs to inspect a valid flag.
                if (fresh) {
                    frame.volumeRaw = payload.volumeRaw;
                    frame.volumeSm = payload.volumeSm;
                    frame.volumeAvg = payload.sampleAvg;
                    for (unsigned band = 0; band < 16; ++band) {
                        frame.fftResult[band] = payload.fftResult[band];
                    }
                }
                frame.bpm = rate;
                frame.wholeBeats = beats;
                frame.beatFraction = fraction;
                return frame;
            }

            // Called on a COPY of the frozen frame. Event sensitivity changes
            // event admission only, never volume, FFT levels, or the clock.
            // The dedicated bass detector has already made its own decision;
            // it remains a bass hit whenever sensitivity is positive. General
            // bass/mid/treble events use the same relative-strength threshold.
            static AudioFrame withEvents(AudioFrame frame, bool kick, const uint8_t *strength,
                                         float sensitivity) {
                if (!isfinite(sensitivity)) {
                    sensitivity = 1.0f;
                }
                if (sensitivity <= 0.0f) {
                    return frame;
                }
                const float threshold = 32.0f / sensitivity;
                frame.bassKick = kick || (strength[0] > 0 && strength[0] >= threshold);
                frame.midAttack = strength[1] > 0 && strength[1] >= threshold;
                frame.highAttack = strength[2] > 0 && strength[2] >= threshold;
                return frame;
            }

          private:
            void advance(float amount) {
                fraction += amount;
                const int32_t completed = int32_t(floorf(fraction));
                beats += uint32_t(completed);
                fraction -= float(completed);
            }

            bool started = false;
            bool previouslyLocked = false;
            uint32_t lastFrameMs = 0;
            uint32_t beats = 0;
            float fraction = 0.0f;
            float rate = 12.0f;
        };
    }
}
