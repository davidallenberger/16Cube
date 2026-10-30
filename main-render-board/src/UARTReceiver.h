#pragma once

#include <Arduino.h>
#include "SharedProtocol.h"

// Render-side access to the latest validated command-board data.
// Each accessor copies data under the receiver's mutex, releases the mutex,
// and then returns. Callers never acquire/release that mutex themselves.
// Games can include this header without including Bluepad32 or BluetoothManager.
namespace RemoteInputs {
    struct Control {
        bool linked = false;
        bool ready = false;
        CubinaWire::Mode mode = CubinaWire::Mode::Animation;
        uint32_t session = 0;
        uint32_t revision = 0;
    };

    struct Audio {
        bool valid = false;
        AudioPayload values = {};
        uint32_t beatCount = 0;
        uint32_t capturedMs = 0;
        uint32_t receivedMs = 0;
    };

    struct Stats {
        uint32_t accepted = 0;
        uint32_t rejected = 0;
        uint32_t duplicates = 0;
    };

    // Initialize storage/UART and start the Core 0 receiver task once.
    void begin ();

    Control control ();
    Audio audio ();
    Stats stats ();

    // Return true once for a pending beat, consuming the pending indication.
    bool consumeBeat ();

    // Use this for ordinary game input. It includes held buttons plus short
    // presses latched for this frame. A default neutral state is returned
    // when the player is disconnected or its received data has expired.
    GamepadState gamepad (ControllerRole role);

    // Use this when duration matters (for example, holding Home for 500 ms).
    // It contains actual held state, without synthesized one-frame taps.
    GamepadState heldGamepad (ControllerRole role);

    // Copy a fresh 512-byte occupancy grid into the caller's buffer.
    // Returns false if there is no fresh grid for the current mode.
    bool depth (uint8_t* occupancy512);

    // Called by the animation task's checkpoint, not by individual games.
    // Transfers pending short presses into the frame snapshot and clears
    // those pending latches. Subsequent gamepad() calls read that snapshot.
    void beginFrame ();
}
