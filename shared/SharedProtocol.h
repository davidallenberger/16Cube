#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Input meaning lives in GamepadState.h. This file defines packet transport.
// Both command and render compile against the same input contract.
#include "GamepadState.h"

struct AudioPayload {
    uint8_t mode;
    uint8_t beat;
    uint8_t volumeRaw;
    uint8_t volumeSm;
    uint8_t sampleAvg;
    uint8_t fftResult[16];
};
static_assert (sizeof (AudioPayload) == 21, "Audio contract changed");

namespace CubinaWire {
constexpr uint32_t BAUD = 921600;
constexpr uint8_t VERSION = 1;
enum class Mode : uint8_t {
    Animation = 0,
    AudioMic = 1,
    AudioBluetooth = 2,
    AudioSD = 3,
    AudioDJ = 4,
    Game = 5,
    Depth = 6
};
enum class Type : uint8_t { Status = 0, Audio = 1, Gamepads = 2, Depth = 3 };
constexpr uint8_t SOURCE_READY = 1;
constexpr size_t DEPTH_BYTES = 16 * 16 * 16 / 8; // one occupancy BIT per voxel
constexpr size_t HEADER_BYTES = 22;
constexpr size_t MAX_PAYLOAD = DEPTH_BYTES;
constexpr size_t MAX_RAW = HEADER_BYTES + MAX_PAYLOAD + 2;
constexpr size_t MAX_ENCODED = MAX_RAW + MAX_RAW / 254 + 1;
constexpr size_t MAX_FRAME = MAX_ENCODED + 1; // COBS delimiter included
constexpr size_t AUDIO_BYTES = 25;
constexpr size_t GAMEPAD_BYTES = 38;
// Header fields use explicit little-endian encoding. Never transmit sizeof a
// C++ struct: bool, padding and compiler layout are not wire specifications.
struct Packet {
    Type type = Type::Status;
    Mode mode = Mode::Animation;
    uint8_t flags = 0;
    uint16_t length = 0;
    // session changes when the command board reboots. modeRevision changes
    // for each mode demand. sequence advances with packets, not button presses.
    // capturedMs is the command board's clock, not the render board's clock.
    uint32_t session = 0;
    uint32_t modeRevision = 0;
    uint32_t sequence = 0;
    uint32_t capturedMs = 0;
    uint8_t payload[MAX_PAYLOAD] = {};
};
inline bool audioMode (Mode m) {
    return m >= Mode::AudioMic && m <= Mode::AudioDJ;
}
inline bool validMode (uint8_t m) {
    return m <= uint8_t (Mode::Depth);
}
// Integer helpers write the least-significant byte first (little endian).
// Both boards use these helpers instead of relying on compiler struct layout.
inline void put16 (uint8_t *p, uint16_t v) {
    p[0] = v;
    p[1] = v >> 8;
}
inline uint16_t get16 (const uint8_t *p) {
    return uint16_t (p[0]) | uint16_t (p[1]) << 8;
}
inline void put32 (uint8_t *p, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        p[i] = v >> (8 * i);
    }
}
inline uint32_t get32 (const uint8_t *p) {
    return uint32_t (p[0]) | uint32_t (p[1]) << 8 | uint32_t (p[2]) << 16 |
           uint32_t (p[3]) << 24;
}
// CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, xorout 0.
inline uint16_t crc16 (const uint8_t *p, size_t n) {
    uint16_t c = 0xffff;
    while (n--) {
        c ^= uint16_t (*p++) << 8;
        for (int i = 0; i < 8; ++i) {
            c = (c & 0x8000) ? uint16_t ((c << 1) ^ 0x1021) : uint16_t (c << 1);
        }
    }
    return c;
}
// Reject inconsistent types, lengths, or field values before publishing input.
inline bool valid (const Packet &p) {
    if (!validMode (uint8_t (p.mode)) || (p.flags & ~SOURCE_READY)) {
        return false;
    }
    switch (p.type) {
    case Type::Status:
        return p.length == 0;
    case Type::Audio:
        return audioMode (p.mode) && p.length == AUDIO_BYTES && p.payload[1] <= 1 &&
               p.payload[0] == (p.mode == Mode::AudioMic  ? 1
                                : p.mode == Mode::AudioSD ? 3
                                                          : 2);
    case Type::Gamepads:
        if (p.mode != Mode::Game || p.length != GAMEPAD_BYTES) {
            return false;
        }
        for (int k = 0; k < 2; ++k) {
            const uint8_t *q = p.payload + k * 15;
            if (q[0] > 1 || (q[1] & 0xf0)) {
                return false;
            }
            for (int i = 6; i < 14; ++i) {
                if (q[i] > 1) {
                    return false;
                }
            }
            const int32_t x = int16_t (get16 (q + 2));
            const int32_t y = int16_t (get16 (q + 4));
            if (x < -512 || x > 512 || y < -512 || y > 512) {
                return false;
            }
            if (get32 (p.payload + 30 + 4 * k) & 0xfff00000u) {
                return false;
            }
        }
        return true;
    case Type::Depth:
        return p.mode == Mode::Depth && p.length == DEPTH_BYTES;
    }
    return false;
}
// Build a header + payload + CRC, then COBS-encode it. COBS removes interior
// zero bytes, so a final zero byte unambiguously marks the end of this packet.
inline size_t encode (const Packet &p, uint8_t *out, size_t capacity) {
    if (capacity < MAX_FRAME || !valid (p)) {
        return 0;
    }
    uint8_t raw[MAX_RAW];
    raw[0] = VERSION;
    raw[1] = uint8_t (p.type);
    raw[2] = uint8_t (p.mode);
    raw[3] = p.flags;
    put16 (raw + 4, p.length);
    put32 (raw + 6, p.session);
    put32 (raw + 10, p.modeRevision);
    put32 (raw + 14, p.sequence);
    put32 (raw + 18, p.capturedMs);
    memcpy (raw + HEADER_BYTES, p.payload, p.length);
    const size_t n = HEADER_BYTES + p.length;
    put16 (raw + n, crc16 (raw, n));
    size_t codeAt = 0;
    size_t w = 1;
    uint8_t code = 1;
    for (size_t i = 0; i < n + 2; ++i) {
        if (raw[i] == 0) {
            out[codeAt] = code;
            codeAt = w++;
            code = 1;
        } else {
            out[w++] = raw[i];
            if (++code == 0xff) {
                out[codeAt] = code;
                codeAt = w++;
                code = 1;
            }
        }
    }
    out[codeAt] = code;
    out[w++] = 0;
    return w;
}
// Undo COBS, check length/version/CRC, and reconstruct one validated packet.
// n excludes the zero delimiter. A failed packet must never update app state.
inline bool decode (const uint8_t *data, size_t n, Packet &p) {
    uint8_t raw[MAX_RAW];
    size_t r = 0;
    size_t w = 0;
    while (r < n) {
        uint8_t code = data[r++];
        if (!code || r + code - 1 > n) {
            return false;
        }
        for (int i = 1; i < code; ++i) {
            if (w >= MAX_RAW || !data[r]) {
                return false;
            }
            raw[w++] = data[r++];
        }
        if (code != 0xff && r < n) {
            if (w >= MAX_RAW) {
                return false;
            }
            raw[w++] = 0;
        }
    }
    if (w < HEADER_BYTES + 2 || raw[0] != VERSION) {
        return false;
    }
    const size_t len = get16 (raw + 4);
    if (len > MAX_PAYLOAD || w != HEADER_BYTES + len + 2 ||
        crc16 (raw, w - 2) != get16 (raw + w - 2)) {
        return false;
    }
    p.type = Type (raw[1]);
    p.mode = Mode (raw[2]);
    p.flags = raw[3];
    p.length = len;
    p.session = get32 (raw + 6);
    p.modeRevision = get32 (raw + 10);
    p.sequence = get32 (raw + 14);
    p.capturedMs = get32 (raw + 18);
    memcpy (p.payload, raw + HEADER_BYTES, len);
    return valid (p);
}
// One bounded byte at a time. Overflow discards until delimiter. Timeout is
// inter-byte, not total frame duration, and uses unsigned wrap-safe arithmetic.
class Parser {
    uint8_t bytes[MAX_ENCODED];
    size_t used = 0;
    uint32_t lastByte = 0;
    bool dropping = false;

  public:
    uint32_t rejected = 0;
    uint32_t accepted = 0;
    bool feed (uint8_t b, uint32_t now, Packet &p) {
        if ((used || dropping) && uint32_t (now - lastByte) > 50) {
            used = 0;
            dropping = false;
        }
        lastByte = now;
        if (!b) {
            bool ok = !dropping && used && decode (bytes, used, p);
            if (used || dropping) {
                if (ok) {
                    ++accepted;
                } else {
                    ++rejected;
                }
            }
            used = 0;
            dropping = false;
            return ok;
        }
        if (dropping) {
            return false;
        }
        if (used == sizeof (bytes)) {
            used = 0;
            dropping = true;
            return false;
        }
        bytes[used++] = b;
        return false;
    }
};
// Compact mask used ONLY to remember short button presses between updates.
// Bits 0..7 are A/B/X/Y/L1/L2/R1/R2; bits 8..11 are directions; bits
// 12..19 hold miscButtons. Stick positions are continuous, not latched presses.
inline uint32_t buttons (const GamepadState &s) {
    if (!s.connected) {
        return 0;
    }
    return uint32_t (s.a) | uint32_t (s.b) << 1 | uint32_t (s.x) << 2 |
           uint32_t (s.y) << 3 | uint32_t (s.l1) << 4 | uint32_t (s.l2) << 5 |
           uint32_t (s.r1) << 6 | uint32_t (s.r2) << 7 | uint32_t (s.dpad & 15) << 8 |
           uint32_t (s.miscButtons) << 12;
}
// Merge pending presses into one frame snapshot so a quick press-and-release
// between frames is still visible. heldGamepad() never receives this merge.
inline void applyPresses (GamepadState &s, uint32_t m) {
    if (!s.connected) {
        return;
    }
    s.a |= bool (m & 1);
    s.b |= bool (m & 2);
    s.x |= bool (m & 4);
    s.y |= bool (m & 8);
    s.l1 |= bool (m & 16);
    s.l2 |= bool (m & 32);
    s.r1 |= bool (m & 64);
    s.r2 |= bool (m & 128);
    s.dpad |= (m >> 8) & 15;
    s.miscButtons |= (m >> 12) & 255;
}
// One player occupies exactly 15 bytes on the wire:
// 0 connected; 1 directions; 2..3 X; 4..5 Y; 6..13 named buttons; 14 extras.
// Direction and extra-button bits retain their GamepadState.h meanings.
inline void writePad (uint8_t *q, const GamepadState &s) {
    q[0] = s.connected;
    q[1] = s.dpad;
    put16 (q + 2, uint16_t (s.axisX));
    put16 (q + 4, uint16_t (s.axisY));
    q[6] = s.a;
    q[7] = s.b;
    q[8] = s.x;
    q[9] = s.y;
    q[10] = s.l1;
    q[11] = s.l2;
    q[12] = s.r1;
    q[13] = s.r2;
    q[14] = s.miscButtons;
}
// Inverse of writePad(): byte positions are the sender/receiver binding.
// Call only for a packet that has already passed validation.
inline GamepadState readPad (const uint8_t *q) {
    GamepadState s;
    s.connected = q[0];
    s.dpad = q[1];
    s.axisX = int16_t (get16 (q + 2));
    s.axisY = int16_t (get16 (q + 4));
    s.a = q[6];
    s.b = q[7];
    s.x = q[8];
    s.y = q[9];
    s.l1 = q[10];
    s.l2 = q[11];
    s.r1 = q[12];
    s.r2 = q[13];
    s.miscButtons = q[14];
    return s;
}
// Audio keeps the existing 21-byte visualizer payload followed by a 32-bit
// beat counter. The counter preserves beats between UART/render updates.
inline void writeAudio (Packet &p, const AudioPayload &a, uint32_t beats) {
    p.type = Type::Audio;
    p.length = AUDIO_BYTES;
    memcpy (p.payload, &a, 21);
    put32 (p.payload + 21, beats);
}
inline AudioPayload readAudio (const Packet &p) {
    AudioPayload a;
    memcpy (&a, p.payload, 21);
    return a;
}
// Depth is one occupancy bit per voxel, X changing fastest, then Y, then Z.
// This is a 16-cubed occupancy grid, not floating-point XYZ coordinates.
inline size_t voxelIndex (unsigned x, unsigned y, unsigned z) {
    return x + 16 * (y + 16 * z);
}
inline bool voxel (const uint8_t *bits, unsigned x, unsigned y, unsigned z) {
    if (x >= 16 || y >= 16 || z >= 16) {
        return false;
    }
    size_t n = voxelIndex (x, y, z);
    return bits[n >> 3] & (1u << (n & 7));
}
inline void setVoxelBit (uint8_t *bits, unsigned x, unsigned y, unsigned z,
                         bool on = true) {
    if (x >= 16 || y >= 16 || z >= 16) {
        return;
    }
    size_t n = voxelIndex (x, y, z);
    uint8_t mask = 1u << (n & 7);
    if (on) {
        bits[n >> 3] |= mask;
    } else {
        bits[n >> 3] &= uint8_t (~mask);
    }
}
} // namespace CubinaWire
