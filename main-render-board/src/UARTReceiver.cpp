#include "UARTReceiver.h"
#include "CubeCore.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
/*
 * UART RECEIVER: PURPOSE, PROTOCOL, STATE, AND SAFETY
 *
 * This describes the current UARTReceiver.cpp implementation. Packet encoding
 * and validation live in shared/SharedProtocol.h; gamepad meanings live in
 * shared/GamepadState.h. Keep those headers identical for both boards.
 *
 * WHAT THIS FILE DOES
 * -------------------
 * The command board acquires inputs and chooses the demanded operating mode.
 * This render-board file receives that information, validates it, and stores
 * copies for the animation task. It does not acquire audio or Bluetooth data,
 * draw animations, choose an app, or execute longjmp().
 *
 * The animation task calls RemoteInputs accessors to obtain snapshots. Its
 * CubeSystem::frameCheckpoint() decides whether a newly demanded mode requires
 * leaving the current app. Only that animation task may longjmp to its own
 * setjmp return point. A UART task cannot jump into another task's call stack.
 *
 * SERIAL CONNECTION AND TASK
 * --------------------------
 * Serial2 carries binary packets only: BAUD = 921600, SERIAL_8N1 (8 data bits,
 * no parity, one stop bit). UART_RX_PIN and UART_TX_PIN come from the board
 * configuration included through CubeCore.h. USB Serial is a separate port.
 *
 * begin() creates the mutex, requests an 8192-byte RX buffer, starts Serial2,
 * and starts task() on Core 0 with priority 2 and a 6144-byte task stack.
 * Repeated begin() calls return immediately if mutex already exists. Call
 * begin() during setup, before starting the animation engine; this is not a
 * general-purpose, concurrent initialization mechanism.
 *
 * task() drains all currently available bytes before yielding for one FreeRTOS
 * tick. One tick is not necessarily one millisecond. Serial2.read() supplies
 * one byte at a time; there is no readBytes() or readStringUntil() waiting for
 * the rest of a damaged packet. The task still briefly waits on our mutex
 * when publishing state. A continuously saturated input could delay its yield.
 *
 * PACKET LABELS: WHAT THE BYTES MEAN
 * ---------------------------------
 * Protocol VERSION is 1. A decoded packet contains a 22-byte header, the
 * payload, and a two-byte CRC. Multibyte integers are explicitly little endian
 * (least-significant byte first), not raw C++ structure copies.
 *
 * Header byte offsets:
 *   0       version       Protocol layout understood by both boards.
 *   1       type          Kind of payload carried by THIS packet.
 *   2       mode          Operating mode demanded by the command board.
 *   3       flags         SOURCE_READY is bit 0; other bits are rejected.
 *   4..5    length        Payload length, excluding header and CRC.
 *   6..9    session       Identifier selected when the command board boots.
 *   10..13  modeRevision  Changes for each new mode demand, including a demand
 *                         for the same mode again.
 *   14..17  sequence      Packet-order counter within the session.
 *   18..21  capturedMs    Command-board timestamp for the captured information.
 *   22..    payload       Type-specific data; CRC follows immediately after it.
 *
 * type and mode serve DIFFERENT purposes. A Status packet can demand AudioMic
 * without carrying audio samples. Every accepted packet carries mode authority;
 * a separate mode-change packet is not required.
 *
 * Mode numbers:
 *   0 Animation; 1 AudioMic; 2 AudioBluetooth; 3 AudioSD;
 *   4 AudioDJ; 5 Game; 6 Depth.
 *
 * Type numbers and payloads:
 *   0 Status:   zero payload bytes; carries mode/readiness/heartbeat information.
 *   1 Audio:    25 bytes = the 21-byte AudioPayload plus a uint32 beat count.
 *               AudioPayload contains mode, beat, volumeRaw, volumeSm,
 *               sampleAvg, and fftResult[16]. These are visualizer summaries,
 *               not the original PCM audio samples.
 *               Its legacy source label is 1 for microphone, 3 for SD, and
 *               2 for analog Bluetooth/DJ. That label is NOT the header Mode
 *               enumeration, although both fields happen to be called mode.
 *   2 Gamepads: 38 bytes = two 15-byte player states, then two four-byte masks
 *               recording pending button presses. Player indices 0 and 1 are
 *               ROLE_MASTER and ROLE_PLAYER_2. ROLE_NONE is not an array index.
 *   3 Depth:    512 bytes = one occupancy bit for each voxel in a 16x16x16 grid.
 *               Bit index is x + 16 * (y + 16 * z), least-significant bit first
 *               within each byte. This is not a list of floating-point points.
 *
 * Shared gamepad states carry connected, dpad, axisX, axisY, a/b/x/y,
 * l1/l2/r1/r2, and miscButtons. Directions use Up=1, Down=2, Right=4, Left=8;
 * extra buttons use System=1, Select=2, Start=4, Capture=8. Constants are named
 * in CubinaInput. The receiver transports these meanings; it does not decide
 * which game action a button should perform.
 *
 * FRAMING, ERROR CHECKS, AND RECOVERY
 * ---------------------------------
 * This protocol uses COBS framing followed by a zero-byte delimiter. COBS makes
 * the encoded packet body contain no zero bytes, so zero marks a boundary.
 * There is NO 0xAA55 sync word and NO XOR checksum in this implementation.
 *
 * Parser.feed() accumulates bytes in a bounded buffer. At a delimiter it
 * decodes COBS, then checks the version, lengths, CRC-16/CCITT-FALSE, and packet
 * fields. CRC covers the decoded header and payload. CRC parameters are
 * polynomial 0x1021, initial value 0xFFFF, no reflection, final XOR zero.
 *
 * Validation rejects unknown modes/types/flags, inconsistent payload lengths,
 * incompatible type/mode combinations, invalid audio source/beat labels,
 * nonboolean gamepad fields, direction bits outside the low four bits, axes
 * outside -512..512, and press-mask bits outside their defined 20-bit region.
 * miscButtons itself is still an eight-bit field; validation does not restrict
 * it to just the four currently named extra-button bits.
 *
 * If an encoded frame grows too large, the parser discards it and ignores
 * further bytes until a delimiter (unless its inter-byte timeout resets it).
 * A gap greater than 50 ms resets an unfinished frame when the NEXT byte
 * arrives. This is an inter-byte timeout, not a maximum total packet duration.
 * It does not block waiting for the missing bytes.
 *
 * Damaged or truncated input can lose packets, but cannot grow the parser's
 * buffer without bounds. After a clean delimiter, subsequent intact packets
 * can be recovered. UART hardware/buffer overflow is not separately counted
 * here; any resulting corruption is handled by framing/validation when seen.
 *
 * Only a fully validated packet reaches accept(). accept() adds ordering and
 * mode-authority checks under the mutex:
 *   - Within the same session, duplicate/older sequence numbers are discarded.
 *   - An older modeRevision is rejected.
 *   - The same revision cannot claim a different mode.
 *   - A new session or changed revision clears old source data before publishing
 *     the new demand. ready=false also clears source data.
 *
 * Signed-difference comparisons permit uint32 counter wrap, provided compared
 * values are less than half the counter range apart. A new session starts a
 * new ordering history. Sessions are boot identifiers, not authentication.
 * No session-history blacklist prevents an old session from being accepted
 * again if a packet carrying it arrives later.
 *
 * SHARED STATE VARIABLES (ANONYMOUS NAMESPACE)
 * -------------------------------------------
 * mutex:
 *   Protects ctl, source caches, timestamps, pending events, and counters.
 *   No drawing, serial transmission, or longjmp happens while it is held.
 * ctl:
 *   Last accepted command authority. Its fields are linked, ready, mode,
 *   session, and revision. ctl.revision corresponds to packet.modeRevision.
 *   ready says the sender reports SOURCE_READY; it does not guarantee that a
 *   fresh payload of the required type has already arrived.
 * audioValue:
 *   Cached audio snapshot: valid, values (AudioPayload), beatCount,
 *   capturedMs (sender clock), and receivedMs (local render clock).
 * held[2]:
 *   Latest received physical gamepad states, without synthesized short taps.
 * framePads[2]:
 *   Animation-frame gamepad snapshots: held state plus pending short presses.
 * presses[2]:
 *   OR-accumulated pending press masks. Bits 0..7 are A/B/X/Y/L1/L2/R1/R2,
 *   bits 8..11 are directions, and bits 12..19 are miscButtons.
 *   beginFrame() merges these into framePads and clears the pending masks.
 * depthBits[DEPTH_BYTES]:
 *   Cached 512-byte voxel occupancy grid.
 * lastPacket:
 *   Local millis() when the last packet passed both parser and accept checks.
 * lastGame / lastDepth:
 *   Local receipt times for the latest gamepad/depth payloads.
 * lastSequence / haveSequence:
 *   Last accepted packet number, and whether such a number has been recorded.
 * haveGame / haveDepth:
 *   Whether the corresponding source cache has received a payload since reset.
 * beatPending:
 *   One pending kick indication retained until consumeBeat() consumes it.
 * counters:
 *   accepted = packets published by accept(); duplicates = duplicate/older
 *   sequence packets; rejected = parser rejections plus authority rejections.
 *   These counters do not count every lost byte or every expired partial frame.
 * DATA_TIMEOUT_MS = 250:
 *   Maximum local receipt age for an audio/gamepad/depth snapshot to be usable.
 * LINK_TIMEOUT_MS = 1000:
 *   Maximum age of an accepted packet for control() to report linked=true.
 *
 * HELPERS AND TASK-LOCAL VARIABLES
 * --------------------------------
 * gameFresh(): tests Game mode, haveGame, and lastGame age.
 * clearSources(): clears audio, pending kick, gamepad state/presses, and depth.
 *   It does not reset command authority, lastSequence, counters, or timestamps.
 * accept(p): p is the parser-validated Packet being considered for publication.
 *   newSession is true on the first accepted session or a changed session ID.
 *   nextBeats is the incoming beat counter at audio payload offset 21.
 *   i is a player-array index, always 0 or 1.
 * task():
 *   parser is the task-owned, persistent framing/validation state machine.
 *   packet is persistent scratch storage for its latest decoded Packet.
 *   b is Serial2.read()'s signed result; negative means no byte was returned.
 *   lastRejected remembers how many parser rejections were already added to
 *   counters, so each rejection is added only once.
 * begin(): ok is the FreeRTOS task-creation result checked against pdPASS.
 *
 * The Parser object itself owns:
 *   bytes[MAX_ENCODED]: encoded frame storage, excluding the delimiter.
 *   used: number of buffered bytes; lastByte: local arrival time of last byte.
 *   dropping: overflow occurred; discard until boundary/reset.
 *   accepted/rejected: parser-level frame counters, separate from counters.
 * Its feed(b, now, p) arguments are incoming byte, local time, and decoded
 * output packet. Local ok means the completed frame passed decoding/validation.
 * SharedProtocol defines HEADER_BYTES=22, MAX_PAYLOAD=512, MAX_RAW=536,
 * MAX_ENCODED=539, MAX_FRAME=540 (including delimiter), AUDIO_BYTES=25,
 * GAMEPAD_BYTES=38, and DEPTH_BYTES=512.
 *
 * ACCESSORS AND THEIR LOCAL VARIABLES
 * -----------------------------------
 * control(): c is a copy of ctl, with linked/ready adjusted for link age.
 * audio(): a is a copy of audioValue; returns an empty snapshot when stale
 *   or outside an audio mode. Check a.valid before using its values.
 * consumeBeat(): b is the fresh pending-kick result; beatPending is cleared
 *   whether the result is true or false.
 * beginFrame(): i selects a player; publish held state if fresh, otherwise a
 *   neutral GamepadState, merge pending presses, then clear those presses.
 * gamepad(r): r is the requested role; s is the returned frame snapshot.
 * heldGamepad(r): same role validation, but s copies actual held state.
 *   Invalid roles, stale data, or calls before begin() return neutral state.
 * depth(bits): bits must point to at least DEPTH_BYTES writable bytes.
 *   fresh is the mode/cache/age test. Copies the grid if fresh; otherwise
 *   zeros the caller's buffer. Null pointers return false without copying.
 * stats(): s is a consistent copy of counters.
 *
 * All value-returning accessors release mutex before returning. A caller owns
 * its returned copy; it is not a pointer into state being changed by Core 0.
 * The receiver task owns parser/packet; animation code never touches them.
 * beginFrame() is animation-task work, called by the frame checkpoint, not
 * separately by every game. It does not poll Bluetooth or read UART bytes.
 *
 * EVENT RETENTION, TIMEOUTS, AND LIMITS
 * ------------------------------------
 * Audio beatCount changes set beatPending. On the first audio payload, the
 * beat flag can set it. Multiple kicks before consumeBeat() coalesce into ONE
 * pending indication; this is not an event queue preserving every kick/time.
 * The current packet's beat byte alone is not a reliable retained event.
 *
 * Gamepad press masks are OR-latched until beginFrame(), allowing a short tap
 * to be observed even if released before the next frame. Repeated taps of the
 * same button before consumption combine into one. Stick movements are not
 * queued. Disconnection clears that player's pending presses.
 *
 * Timeout checks use unsigned elapsed local time, handling ordinary millis()
 * wrap. capturedMs is not used to calculate link age: the two board clocks
 * are not synchronized. Sender buffering/delay is therefore not measured here.
 * Expired source accessors return neutral data; they do not force a mode change.
 * Status packets keep the link alive but do not refresh source timestamps.
 *
 * IMPORTANT CURRENT IMPLEMENTATION DETAIL:
 * control() marks only its RETURNED COPY disconnected after link timeout.
 * It does not clear stored ctl.linked or haveSequence. Consequently accept()'s
 * existing "timed-out link can reconnect" comment overstates its behavior:
 * the same session must still provide a newer sequence and a non-older mode
 * revision after silence.
 * A different session ID is accepted as a new session. This documentation
 * records that behavior; it does not change it.
 *
 * CRC detects many transmission errors; it is not cryptographic authentication
 * and cannot reject every possible corruption. There are no ACKs, retransmits,
 * or delivery guarantees. A validated packet with new authority can demand a
 * mode change. Safety here means bounded parsing, checked fields, coherent
 * snapshots, and neutral stale inputs, not an electrical/brightness interlock.
 *
 * Mutex/task allocation failures use configASSERT(), not graceful recovery.
 * The RX buffer-size request and Serial2 startup have no explicit failure
 * recovery here. Critical sections are intentionally short, but use
 * portMAX_DELAY rather than a bounded mutex wait.
 */


using namespace CubinaWire;

namespace {

SemaphoreHandle_t mutex = nullptr;
RemoteInputs::Control ctl;
RemoteInputs::Audio audioValue;
GamepadState held[2], framePads[2];
uint32_t presses[2] = {};
uint8_t depthBits[DEPTH_BYTES] = {};
uint32_t lastPacket = 0, lastGame = 0, lastDepth = 0, lastSequence = 0;
bool haveSequence = false, haveGame = false, haveDepth = false, beatPending = false;
RemoteInputs::Stats counters;
constexpr uint32_t DATA_TIMEOUT_MS = 250, LINK_TIMEOUT_MS = 1000;

bool gameFresh() {
    return ctl.mode == Mode::Game && haveGame &&
           uint32_t(millis() - lastGame) <= DATA_TIMEOUT_MS;
}

void clearSources() {
    audioValue = {};
    beatPending = false;
    haveGame = haveDepth = false;
    memset(depthBits, 0, sizeof(depthBits));

    for (int i = 0; i < 2; ++i) {
        held[i] = framePads[i] = GamepadState();
        presses[i] = 0;
    }
}

void accept(const Packet& p) {
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

    if (newSession || p.modeRevision != ctl.revision) clearSources();

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
        clearSources();
    } else if (p.type == Type::Audio) {
        const uint32_t nextBeats = get32(p.payload + 21);

        if (audioValue.valid && nextBeats != audioValue.beatCount) {
            beatPending = true;
        } else if (!audioValue.valid && p.payload[1]) {
            beatPending = true;
        }

        audioValue.valid = true;
        audioValue.values = readAudio(p);
        audioValue.beatCount = get32(p.payload + 21);
        audioValue.capturedMs = p.capturedMs;
        audioValue.receivedMs = millis();
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

void task(void*) {
    static Parser parser;
    static Packet packet;
    uint32_t lastRejected = 0;

    for (;;) {
        // Drain ALL currently buffered bytes. Never readBytes/readStringUntil.
        while (Serial2.available() > 0) {
            int b = Serial2.read();
            if (b < 0) break;

            if (parser.feed(uint8_t(b), millis(), packet)) accept(packet);
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
    if (mutex) return;

    mutex = xSemaphoreCreateMutex();
    configASSERT(mutex);
    Serial2.setRxBufferSize(8192);
    Serial2.begin(BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

    BaseType_t ok = xTaskCreatePinnedToCore(
        task, "UARTReceiver", 6144, nullptr, 2, nullptr, 0);
    configASSERT(ok == pdPASS);
}

RemoteInputs::Control RemoteInputs::control() {
    Control c;
    if (!mutex) return c;

    xSemaphoreTake(mutex, portMAX_DELAY);
    c = ctl;
    c.linked = ctl.linked && uint32_t(millis() - lastPacket) <= LINK_TIMEOUT_MS;
    if (!c.linked) c.ready = false;
    xSemaphoreGive(mutex);
    return c;
}

RemoteInputs::Audio RemoteInputs::audio() {
    Audio a;
    if (!mutex) return a;

    xSemaphoreTake(mutex, portMAX_DELAY);
    a = audioValue;
    if (!audioMode(ctl.mode) ||
        uint32_t(millis() - a.receivedMs) > DATA_TIMEOUT_MS) {
        a = {};
    }
    xSemaphoreGive(mutex);
    return a;
}

bool RemoteInputs::consumeBeat() {
    if (!mutex) return false;

    xSemaphoreTake(mutex, portMAX_DELAY);
    bool b = beatPending && audioValue.valid && audioMode(ctl.mode) &&
             uint32_t(millis() - audioValue.receivedMs) <= DATA_TIMEOUT_MS;
    beatPending = false;
    xSemaphoreGive(mutex);
    return b;
}

void RemoteInputs::beginFrame() {
    if (!mutex) return;

    xSemaphoreTake(mutex, portMAX_DELAY);
    for (int i = 0; i < 2; ++i) {
        framePads[i] = gameFresh() ? held[i] : GamepadState();
        applyPresses(framePads[i], presses[i]);
        presses[i] = 0;
    }
    xSemaphoreGive(mutex);
}

GamepadState RemoteInputs::gamepad(ControllerRole r) {
    GamepadState s;
    if (!mutex || (r != ROLE_MASTER && r != ROLE_PLAYER_2)) return s;

    xSemaphoreTake(mutex, portMAX_DELAY);
    if (gameFresh()) s = framePads[int(r)];
    xSemaphoreGive(mutex);
    return s;
}

GamepadState RemoteInputs::heldGamepad(ControllerRole r) {
    GamepadState s;
    if (!mutex || (r != ROLE_MASTER && r != ROLE_PLAYER_2)) return s;

    xSemaphoreTake(mutex, portMAX_DELAY);
    if (gameFresh()) s = held[int(r)];
    xSemaphoreGive(mutex);
    return s;
}

bool RemoteInputs::depth(uint8_t* bits) {
    if (!mutex || !bits) return false;

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
    if (!mutex) return s;

    xSemaphoreTake(mutex, portMAX_DELAY);
    s = counters;
    xSemaphoreGive(mutex);
    return s;
}
