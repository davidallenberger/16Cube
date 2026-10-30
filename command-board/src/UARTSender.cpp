#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "UARTSender.h"
#include "CommandState.h"
#include "BoardConfig.h"
#include "SendTiming.h"

using namespace CubinaWire;

namespace {
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
        packet.flags = state.ready ? SOURCE_READY : 0;

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
        SendTiming::Deadline dataDeadline;
        SendTiming::Deadline statusDeadline;
        static uint8_t bytes[MAX_FRAME];

        for (;;) {
            // This first copy only decides whether work is due. It does not
            // clear button latches or hold a mutex across UART operations.
            const CommandSnapshot view = CommandState::snapshot ();
            const uint32_t now = micros ();
            const bool changed = view.revision != seenRevision;
            const bool carriesData =
                audioMode (view.mode) || view.mode == Mode::Game || view.mode == Mode::Depth;
            const bool dataDue = carriesData && freshSource (view) && dataDeadline.due (now);
            const bool statusDue = changed || statusDeadline.due (now);

            if (!dataDue && !statusDue) {
                vTaskDelay (1);
                continue;
            }

            // Take the LATEST complete state at transmission time. Acquisition
            // can keep publishing while Serial2.write() runs on this task.
            // Only this copy operation consumes pending game-button latches.
            CommandSnapshot state = CommandState::takeForSend ();
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

                seenRevision = state.revision;
                statusDeadline.restart (now, SendTiming::StatusPeriodUs);

                if (packet.type != Type::Status) {
                    if (newDemand) {
                        dataDeadline.restart (now, dataPeriod (state.mode));
                    } else {
                        dataDeadline.advance (now, dataPeriod (state.mode));
                    }
                } else if (newDemand) {
                    // Start the new mode with a fresh schedule; the previous
                    // source's timing must not delay its first data update.
                    dataDeadline.restart (now, 0);
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
    Serial.printf ("UART audio target: 60 Hz, period=%lu us, baud=%lu\n",
                   (unsigned long)SendTiming::AudioPeriodUs, (unsigned long)BAUD);

    BaseType_t ok = xTaskCreatePinnedToCore (task, "UARTSender", 6144, nullptr, 1, nullptr,
                                             COMMAND_SENDER_CORE);
    configASSERT (ok == pdPASS);
}
