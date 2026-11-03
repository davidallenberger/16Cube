#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "UARTSender.h"
#include "CommandState.h"
#include "BoardConfig.h"
#include "SendTiming.h"

using namespace CubinaWire;

namespace {
    // A global ceiling for EVERY encoded packet, including mode demands,
    // gamepad packets and heartbeats. At most one new packet is submitted per
    // 16,667 us. This is separate from a source's desired update cadence.
    constexpr uint32_t MINIMUM_PACKET_INTERVAL_US = 16667;

    uint32_t dataPeriod (Mode mode) {
        if (audioMode (mode)) {
            return SendTiming::AudioPeriodUs;
        }

        if (mode == Mode::Game) {
            return SendTiming::GamePeriodUs;
        }

        return SendTiming::DepthPeriodUs;
    }

    bool freshSource (const CommandSnapshot &state) {
        return state.ready && uint32_t (millis () - state.capturedMs) <= 250;
    }

    // Construct only the packet contents here; keep scheduling, mutex copies,
    // and UART writes in task(). This also makes the wire behavior testable.
    Packet makePacket (const CommandSnapshot &state, bool dataDue, bool newDemand,
                       uint32_t sequence) {
        Packet packet;
        packet.mode = state.mode;
        packet.session = state.session;
        packet.modeRevision = state.revision;
        packet.sequence = sequence;
        packet.capturedMs = state.capturedMs;
        packet.flags = (state.ready ? SOURCE_READY : 0) |
                       (state.playbackHeld ? PLAYBACK_HOLD : 0);

        // Every packet carries mode authority. The first packet for a new
        // demand can be a Status packet; the next data packet follows on
        // the normal cadence. The receiver therefore need not wait for
        // audio acquisition to finish before learning the selected mode.
        if (dataDue && !newDemand && state.ready) {
            if (audioMode (state.mode)) {
                writeAudio (packet, state.audio, state.beats);
            } else if (state.mode == Mode::Depth) {
                packet.type = Type::Depth;
                packet.length = DEPTH_BYTES;
                memcpy (packet.payload, state.depth, DEPTH_BYTES);
            }
        }

        // A game snapshot must carry the presses consumed by takeForSend(),
        // even on a mode demand or heartbeat. Otherwise a Status packet
        // could silently swallow a quick button tap.
        if (state.mode == Mode::Game && state.ready) {
            packet.type = Type::Gamepads;
            packet.length = GAMEPAD_BYTES;

            for (int i = 0; i < 2; ++i) {
                writePad (packet.payload + 15 * i, state.pads[i]);
                put32 (packet.payload + 30 + 4 * i, state.presses[i]);
            }
        }
        return packet;
    }

    void task (void *) {
        uint32_t sequence = 0;
        uint32_t seenRevision = 0;
        bool seenHold = false;
        SendTiming::Deadline dataDeadline;
        SendTiming::Deadline statusDeadline;
        SendTiming::Deadline packetDeadline;
        static uint8_t bytes[MAX_FRAME];

        for (;;) {
            // This first copy only decides whether work is due. It does not
            // clear button latches or hold a mutex across UART operations.
            const CommandSnapshot view = CommandState::snapshot ();
            const uint32_t now = micros ();

            // Check the global limit BEFORE taking/clearing event latches.
            // A new mode demand waits at most the remaining packet interval;
            // it cannot bypass this ceiling by generating extra Status frames.
            if (!packetDeadline.due(now)) {
                vTaskDelay(1);
                continue;
            }

            const bool changed = view.revision != seenRevision;
            const bool carriesData =
                audioMode (view.mode) || view.mode == Mode::Game || view.mode == Mode::Depth;
            const bool dataDue = carriesData && freshSource (view) && dataDeadline.due (now);
            const bool statusDue = changed || view.playbackHeld != seenHold || statusDeadline.due (now);

            if (!dataDue && !statusDue) {
                vTaskDelay (1);
                continue;
            }

            // Take the LATEST complete state at transmission time. Acquisition
            // can keep publishing while Serial2.write() runs on this task.
            // Only this copy operation consumes pending game-button latches.
            CommandSnapshot state = CommandState::takeForSend (dataDue);
            state.ready = state.ready && (state.mode == Mode::Animation ||
                                          uint32_t (millis () - state.capturedMs) <= 250);

            // Acquisition may have changed modes between the two snapshots.
            // Compare the state we will actually send, not the earlier view.
            const bool newDemand = state.revision != seenRevision;
            Packet packet = makePacket (state, dataDue, newDemand, ++sequence);

            const size_t length = encode (packet, bytes, sizeof (bytes));

            if (length) {
                // No state mutex is held here. The driver may briefly wait
                // for TX space, but that cannot block the acquisition task
                // from publishing a newer snapshot. There is one TX owner.
                size_t sent = 0;

                while (sent < length) {
                    const size_t count = Serial2.write (bytes + sent, length - sent);

                    if (!count) {
                        vTaskDelay (1);
                        continue;
                    }

                    sent += count;
                }

                // Use the ACTUAL completion time of the write calls. The
                // earlier 'now' can be stale if a UART write had to wait.
                // Scheduling from that old value could permit an immediate
                // extra packet after a stall. Never replay missed slots.
                //
                // Completion here means handed to the UART driver; it does
                // not claim that the final stop bit has left the GPIO pin.
                const uint32_t submittedAt = micros();
                packetDeadline.restart(submittedAt, MINIMUM_PACKET_INTERVAL_US);
                seenRevision = state.revision;
                seenHold = state.playbackHeld;
                statusDeadline.restart(submittedAt, SendTiming::StatusPeriodUs);

                if (packet.type != Type::Status) {
                    // Restart from the real submission time. Small scheduler
                    // delays may lower the achieved rate; they are not repaid
                    // by squeezing the following pair of packets closer.
                    dataDeadline.restart(submittedAt, dataPeriod(state.mode));
                } else if (newDemand) {
                    // Start the new mode with a fresh schedule; the previous
                    // source's timing must not delay its first data update.
                    // The global packet gate still applies to that first data
                    // packet, even though this source is ready immediately.
                    dataDeadline.restart(submittedAt, 0);
                }
            }

            // One scheduler tick is cooperative yielding, not a promise of
            // one millisecond. Hardware/task load can reduce achieved rates.
            vTaskDelay (1);
        }
    }
} // namespace

void UARTSender::begin () {
    Serial2.setTxBufferSize (2048);
    Serial2.setRxBufferSize (256);
    Serial2.begin (BAUD, SERIAL_8N1, COMMAND_UART_RX, COMMAND_UART_TX);

    // End any partial frame left in a receiver that was already running.
    Serial2.write (uint8_t (0));
    Serial.printf("UART ceiling: 60 packets/s including mode/status; minimum gap=%lu us, "
                  "audio period=%lu us, baud=%lu\r\n",
                  (unsigned long)MINIMUM_PACKET_INTERVAL_US,
                  (unsigned long)SendTiming::AudioPeriodUs, (unsigned long)BAUD);

    BaseType_t ok = xTaskCreatePinnedToCore (task, "UARTSender", 6144, nullptr, 1, nullptr,
                                             COMMAND_SENDER_CORE);
    configASSERT (ok == pdPASS);
}
