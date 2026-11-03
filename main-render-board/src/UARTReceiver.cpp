#include "UARTReceiver.h"

#if CUBINA_ENABLE_UART_RECEIVER
#include "CubeCore.h"
#include "AudioFrameClock.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
/*
 * UART RECEIVER AND THE ANIMATION AUDIO INTERFACE
 *
 * Core 0 owns the UART driver and nonblocking byte parser. It validates COBS
 * framing, version, lengths and CRC-16 before accepting a packet. Ordering
 * checks reject old/duplicate sequences, old revisions, and contradictory
 * modes within one revision. A new session/revision clears source caches.
 * The binary protocol is version 3; its 38-byte audio payload is explicitly
 * encoded in shared/SharedProtocol.h. Never transmit sizeof(AudioPayload).
 *
 * The receive task only caches input. It never draws, advances the musical
 * clock, selects an app, or longjmps. CubeSystem::frameCheckpoint(), on the
 * animation task, freezes the next frame BEFORE taking a same-task escape.
 * All mutexes have been released before drawing or jumping begins.
 *
 * ANIMATION CONTRACT
 * RemoteInputs::audio() returns one AudioFrame: flat volumeRaw/volumeSm/
 * volumeAvg, fftResult[16], usable bpm, bassKick/midAttack/highAttack, and
 * beatPulse(N)/beatAngle(N). It exposes no payload, counts, timestamps,
 * confidence, raw phase, separate Attack object, or consume functions.
 * Calls within one frame never advance time or consume events. The optional
 * sensitivity argument changes only event admission. Stale audio is neutral;
 * the musical clock eases toward slow idle motion. Clock interpretation lives
 * in AudioFrameClock.h, not in each BPM animation.
 *
 * PRIVATE STATE AND OWNERSHIP
 * mutex: protects command authority, raw source caches, pending events,
 *        timestamps and packet statistics shared with the receive task.
 * ctl: last accepted mode, source readiness, boot session and demand revision.
 * audioValue: private AudioCache containing AudioPayload, kick counter,
 *             sender timestamp, local receipt time and a have-data flag.
 * pendingEvents: OR-latched kick and strongest general attack per group,
 *                with LOCAL receipt times to prevent delayed stale flashes.
 * audioGeneration: increments on a source reset; resets the musical clock
 *                  when the animation task next freezes a frame.
 * frameAudio/frameKick/frameStrength: render-task-owned frozen input. The
 *     receiver never changes it. Frame strengths are private; the public
 *     accessor maps them to simple booleans at the requested sensitivity.
 * frameGeneration/audioClock: render-task-owned source identity and clock.
 * held/framePads/presses: actual gamepad state, frozen gamepad state, and
 *     pending short press masks. A read never consumes a gamepad press.
 * depthBits/haveDepth/lastDepth: latest fixed 512-byte occupancy grid and age.
 * lastPacket: receipt time used for the one-second link timeout.
 * lastGame/haveGame: receipt time and availability of gamepad input.
 * lastSequence/haveSequence: ordering history, with signed wrap comparisons.
 * counters: accepted packets, authority/parser rejections and duplicates.
 *
 * beginFrame() copies the raw audio and drains pending events under the
 * mutex, then updates audioClock AFTER unlocking. The timer/trigonometric
 * work never holds the UART mutex. No heap allocation is introduced here.
 * Gamepad/depth behavior and buffer/task sizes remain unchanged.
 *
 * EVENTS
 * A dedicated kick-counter change maps to bassKick. General attack changes
 * map into three private groups using the same sixteen spectrum bands:
 *   bass: 0..3 (centers about 61..244 Hz)
 *   mid:  4..10 (about 305..2502 Hz)
 *   high: 11..15 (about 2563..15564 Hz)
 * These are musical groupings, not sharp filters or instrument classifiers.
 * A kick and general bass attack merge into ONE bassKick boolean. Multiple
 * events between frames coalesce. General attack strength/masks may already
 * be aggregated by the sender; they are not exact per-instrument energies.
 * Lost metadata cannot be recovered from a count alone. The dedicated kick
 * count still recovers a pending bass event after a missed packet.
 *
 * SAFETY/LIMITS
 * Source freshness is 250 ms. Status packets do not refresh audio/gamepad/
 * depth ages. Source resets clear pending events; expired events are dropped.
 * unsigned local elapsed times handle millis() wrap. Sender timestamps are
 * never subtracted from local timestamps. CRC is error detection, not
 * authentication; there are no ACKs/retransmits or delivery guarantees.
 * Parser buffers are bounded and incomplete packets reset after a 50-ms
 * inter-byte gap on the next byte. Oversized frames discard through delimiter.
 * The task drains available UART bytes before yielding one FreeRTOS tick.
 * Initialization failures assert. The mutex uses portMAX_DELAY, with short
 * copy-only critical sections. No resource-owning object crosses longjmp.
 */

using namespace CubinaWire;

namespace {

SemaphoreHandle_t mutex = nullptr;
RemoteInputs::Control ctl;
struct AudioCache {
    bool valid = false;
    AudioPayload values = {};
    uint32_t beatCount = 0;
    uint32_t capturedMs = 0;
    uint32_t receivedMs = 0;
};

struct PendingAudioEvents {
    bool kick = false;
    uint32_t kickReceivedMs = 0;
    uint8_t strength[3] = {};
    uint32_t receivedMs[3] = {};
};

AudioCache audioValue;
PendingAudioEvents pendingEvents;
uint32_t audioGeneration = 0;

// Only the animation task reads/writes this frozen frame and its clock.
RemoteInputs::AudioFrame frameAudio;
bool frameKick = false;
uint8_t frameStrength[3] = {};
uint32_t frameGeneration = 0;
RemoteInputs::Detail::AudioFrameClock audioClock;
GamepadState held[2];
GamepadState framePads[2];
uint32_t presses[2] = {};
uint8_t depthBits[DEPTH_BYTES] = {};
uint32_t lastPacket = 0;
uint32_t lastGame = 0;
uint32_t lastDepth = 0;
uint32_t lastSequence = 0;
bool haveSequence = false;
bool haveGame = false;
bool haveDepth = false;
RemoteInputs::Stats counters;
constexpr uint32_t DATA_TIMEOUT_MS = 250;
constexpr uint32_t LINK_TIMEOUT_MS = 1000;

bool gameFresh() {
    return ctl.mode == Mode::Game && haveGame &&
           uint32_t(millis() - lastGame) <= DATA_TIMEOUT_MS;
}

void clearSources() {
    audioValue = {};
    pendingEvents = {};
    ++audioGeneration;
    haveGame = false;
    haveDepth = false;
    memset(depthBits, 0, sizeof(depthBits));

    for (int i = 0; i < 2; ++i) {
        held[i] = GamepadState();
        framePads[i] = GamepadState();
        presses[i] = 0;
    }
}

void accept(const Packet &p) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool newSession = !ctl.linked || p.session != ctl.session;

    // A timed-out link can reconnect with the same session/sequence stream.
    if (!newSession && haveSequence && int32_t(p.sequence - lastSequence) <= 0) {
        ++counters.duplicates;
        xSemaphoreGive(mutex);
        return;
    }

    if (!newSession && int32_t(p.modeRevision - ctl.revision) < 0) {
        ++counters.rejected;
        xSemaphoreGive(mutex);
        return;
    }

    if (!newSession && p.modeRevision == ctl.revision && p.mode != ctl.mode) {
        ++counters.rejected;
        xSemaphoreGive(mutex);
        return;
    }

    if (newSession || p.modeRevision != ctl.revision) {
        clearSources();
    }

    ctl.linked = true;
    ctl.mode = p.mode;
    ctl.session = p.session;
    ctl.revision = p.modeRevision;
    ctl.ready = p.flags & SOURCE_READY;
    lastPacket = millis();
    lastSequence = p.sequence;
    haveSequence = true;
    ++counters.accepted;

    if (!ctl.ready) {
        // Repeated not-ready heartbeats must not restart the idle clock.
        // The transition clears an existing source once; later packets are
        // already neutral. Session/revision changes were cleared above.
        if (audioValue.valid || haveGame || haveDepth) {
            clearSources();
        }
    } else if (p.type == Type::Audio) {
        const uint32_t now = millis();
        const uint32_t nextBeats = get32(p.payload + 21);
        const AudioPayload nextAudio = readAudio(p);
        const bool previousFresh = audioValue.valid &&
            uint32_t(now - audioValue.receivedMs) <= DATA_TIMEOUT_MS;
        const bool kick = previousFresh
            ? nextBeats != audioValue.beatCount
            : nextAudio.beat != 0;

        if (kick) {
            pendingEvents.kick = true;
            pendingEvents.kickReceivedMs = now;
        }

        const bool newAttack = previousFresh
            ? nextAudio.attackCount != audioValue.values.attackCount
            : nextAudio.attackStrength != 0;
        if (newAttack) {
            constexpr uint16_t GROUP_MASKS[3] = {0x000F, 0x07F0, 0xF800};
            for (unsigned group = 0; group < 3; ++group) {
                if (nextAudio.attackBands & GROUP_MASKS[group]) {
                    if (uint32_t(now - pendingEvents.receivedMs[group]) > DATA_TIMEOUT_MS) {
                        pendingEvents.strength[group] = 0;
                    }
                    pendingEvents.strength[group] = max(
                        pendingEvents.strength[group], nextAudio.attackStrength);
                    pendingEvents.receivedMs[group] = now;
                }
            }
        }

        audioValue.valid = true;
        audioValue.values = nextAudio;
        audioValue.beatCount = nextBeats;
        audioValue.capturedMs = p.capturedMs;
        audioValue.receivedMs = now;
    } else if (p.type == Type::Gamepads) {
        for (int i = 0; i < 2; ++i) {
            held[i] = readPad(p.payload + 15 * i);
            if (held[i].connected) {
                presses[i] |= get32(p.payload + 30 + 4 * i);
            } else {
                presses[i] = 0;
            }
        }
        lastGame = millis();
        haveGame = true;
    } else if (p.type == Type::Depth) {
        memcpy(depthBits, p.payload, DEPTH_BYTES);
        lastDepth = millis();
        haveDepth = true;
    }

    xSemaphoreGive(mutex);
}

void task(void *) {
    // This task is pinned to Core 0. Install the UART driver HERE, rather
    // than in begin(), which CubeSystem calls from the animation task.
    //
    // On ESP32, installing a peripheral driver also assigns its interrupt
    // handler to the core doing the installation. Pinning only this parser
    // task does not move an interrupt that was already installed on Core 1.
    // Keeping both UART reception and its interrupt on Core 0 avoids that
    // unnecessary competition with FastLED's I2S output interrupt on Core 1.
    configASSERT(xPortGetCoreID() == 0);
    Serial2.setRxBufferSize(8192);
    Serial2.begin(BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

    Serial.printf("UARTReceiver: UART2 initialized on Core %u; receiver task on Core %u\r\n",
                  unsigned(xPortGetCoreID()), unsigned(xPortGetCoreID()));

    static Parser parser;
    static Packet packet;
    uint32_t lastRejected = 0;

    for (;;) {
        // Drain ALL currently buffered bytes. Never readBytes/readStringUntil.
        while (Serial2.available() > 0) {
            int b = Serial2.read();
            if (b < 0) {
                break;
            }

            if (parser.feed(uint8_t(b), millis(), packet)) {
                accept(packet);
            }
        }

        xSemaphoreTake(mutex, portMAX_DELAY);
        counters.rejected += parser.rejected - lastRejected;
        lastRejected = parser.rejected;
        xSemaphoreGive(mutex);

        // Give the scheduler/watchdog a turn, even on an idle wire.
        vTaskDelay(1);
    }
}

} // namespace

void RemoteInputs::begin() {
    if (mutex) {
        return;
    }

    mutex = xSemaphoreCreateMutex();
    configASSERT(mutex);

    // Create shared storage before starting the task. It is safe for an
    // accessor to run before the task has initialized UART2: the initial
    // snapshots are simply unlinked/invalid until the first valid packet.
    // Do not put Serial2.begin() here; this caller is on the render core.
    BaseType_t ok = xTaskCreatePinnedToCore(
        task, "UARTReceiver", 6144, nullptr, 2, nullptr, 0);
    configASSERT(ok == pdPASS);
}

RemoteInputs::Control RemoteInputs::control() {
    Control c;
    if (!mutex) {
        return c;
    }

    xSemaphoreTake(mutex, portMAX_DELAY);
    c = ctl;
    c.linked = ctl.linked && uint32_t(millis() - lastPacket) <= LINK_TIMEOUT_MS;
    if (!c.linked) {
        c.ready = false;
    }
    xSemaphoreGive(mutex);
    return c;
}

RemoteInputs::AudioFrame RemoteInputs::audio(float sensitivity) {
    // The frame and its private event metadata belong to this animation task.
    // This accessor never touches the live receiver cache or changes a latch.
    return Detail::AudioFrameClock::withEvents(frameAudio, frameKick, frameStrength, sensitivity);
}

RemoteInputs::Control RemoteInputs::beginFrame() {
    if (!mutex) {
        return {};
    }

    Control controlSnapshot;
    AudioCache captured;
    PendingAudioEvents events;
    uint32_t generation = 0;
    bool fresh = false;
    bool audioModeSelected = false;
    uint32_t now = 0;

    xSemaphoreTake(mutex, portMAX_DELAY);
    // Read time AFTER taking the mutex: a receiver publication while waiting
    // must not appear to come from the future and fail unsigned age checks.
    now = millis();
    controlSnapshot = ctl;
    controlSnapshot.linked = ctl.linked && uint32_t(now - lastPacket) <= LINK_TIMEOUT_MS;
    if (!controlSnapshot.linked) {
        controlSnapshot.ready = false;
    }
    captured = audioValue;
    events = pendingEvents;
    pendingEvents = {};
    generation = audioGeneration;
    audioModeSelected = audioMode(ctl.mode);
    fresh = audioModeSelected && ctl.ready && captured.valid &&
        uint32_t(now - captured.receivedMs) <= DATA_TIMEOUT_MS;
    for (int i = 0; i < 2; ++i) {
        framePads[i] = gameFresh() ? held[i] : GamepadState();
        applyPresses(framePads[i], presses[i]);
        presses[i] = 0;
    }
    xSemaphoreGive(mutex);

    // All math is outside the mutex. No acquisition, UART I/O, LED output,
    // heap allocation, or app escape happens in this update.
    if (generation != frameGeneration) {
        audioClock.reset();
        frameGeneration = generation;
    }
    if (!audioModeSelected) {
        // Ordinary animation/game/depth playback does not need tempo math.
        // Keep that path free of the extra exponential/phase calculations.
        frameAudio = {};
        frameKick = false;
        memset(frameStrength, 0, sizeof(frameStrength));
        return controlSnapshot;
    }
    frameAudio = audioClock.update(captured.values, fresh, captured.receivedMs, now);
    frameKick = fresh && events.kick &&
        uint32_t(now - events.kickReceivedMs) <= DATA_TIMEOUT_MS;
    for (unsigned group = 0; group < 3; ++group) {
        frameStrength[group] = fresh &&
            uint32_t(now - events.receivedMs[group]) <= DATA_TIMEOUT_MS
            ? events.strength[group]
            : 0;
    }
    return controlSnapshot;
}

GamepadState RemoteInputs::gamepad(ControllerRole r) {
    GamepadState s;
    if (!mutex || (r != ROLE_MASTER && r != ROLE_PLAYER_2)) {
        return s;
    }

    xSemaphoreTake(mutex, portMAX_DELAY);
    if (gameFresh()) {
        s = framePads[int(r)];
    }
    xSemaphoreGive(mutex);
    return s;
}

GamepadState RemoteInputs::heldGamepad(ControllerRole r) {
    GamepadState s;
    if (!mutex || (r != ROLE_MASTER && r != ROLE_PLAYER_2)) {
        return s;
    }

    xSemaphoreTake(mutex, portMAX_DELAY);
    if (gameFresh()) {
        s = held[int(r)];
    }
    xSemaphoreGive(mutex);
    return s;
}

bool RemoteInputs::depth(uint8_t *bits) {
    if (!mutex || !bits) {
        return false;
    }

    xSemaphoreTake(mutex, portMAX_DELAY);
    bool fresh = ctl.mode == Mode::Depth && haveDepth &&
                 uint32_t(millis() - lastDepth) <= DATA_TIMEOUT_MS;
    if (fresh) {
        memcpy(bits, depthBits, DEPTH_BYTES);
    } else {
        memset(bits, 0, DEPTH_BYTES);
    }
    xSemaphoreGive(mutex);
    return fresh;
}

RemoteInputs::Stats RemoteInputs::stats() {
    Stats s;
    if (!mutex) {
        return s;
    }

    xSemaphoreTake(mutex, portMAX_DELAY);
    s = counters;
    xSemaphoreGive(mutex);
    return s;
}

#else

#pragma message("CUBINA: inter-board UART2 receiver COMPILED OUT for standalone render test")

// The disabled build does not include CubeCore, AudioFrameClock or FreeRTOS
// here, and does not instantiate any receiver state. Keep the public input
// interface so CubeSystem and the effects compile without special cases.
// These accessors do no serial I/O, locking, clock updates or task work.
namespace RemoteInputs {
    void begin() {
    }

    Control control() {
        return {};
    }

    Control beginFrame() {
        return {};
    }

    AudioFrame audio(float) {
        return {}; // Zero levels/events; no simulated audio or moving clock.
    }

    Stats stats() {
        return {};
    }

    GamepadState gamepad(ControllerRole) {
        return {};
    }

    GamepadState heldGamepad(ControllerRole) {
        return {};
    }

    bool depth(uint8_t *bits) {
        if (bits) {
            memset(bits, 0, CubinaWire::DEPTH_BYTES);
        }
        return false;
    }
}

#endif // CUBINA_ENABLE_UART_RECEIVER
