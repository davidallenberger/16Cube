#pragma once

// STANDALONE RENDER TEST: change ONLY this switch, then rebuild/flash render.
// 0: compile out the board-to-board UART2 receiver and return neutral inputs.
//    No receiver task, receive mutex, RX buffer, parser, or UART2 begin/read/write.
// 1: compile the original receiver, unchanged.
// USB Serial logging is separate and remains available in either build.
// The default is deliberately OFF for this baseline experiment.
#ifndef CUBINA_ENABLE_UART_RECEIVER
#define CUBINA_ENABLE_UART_RECEIVER 0
#endif

#if CUBINA_ENABLE_UART_RECEIVER != 0 && CUBINA_ENABLE_UART_RECEIVER != 1
#error "CUBINA_ENABLE_UART_RECEIVER must be 0 or 1"
#endif

#include <Arduino.h>
#include "SharedProtocol.h"
#include "AudioFrame.h"

// Render-side access to the latest validated command-board data.
// The receiver owns synchronization. Audio reads use the frozen render-frame
// copy; other accessors take their own mutex. Callers never manage a mutex.
// Games can include this header without including Bluepad32 or BluetoothManager.
namespace RemoteInputs {
    struct Control {
        bool linked = false;
        bool ready = false;
        bool playbackHeld = false;
        CubinaWire::Mode mode = CubinaWire::Mode::Animation;
        uint32_t session = 0;
        uint32_t revision = 0;
    };

    // The only audio entry point for animations. The returned frame has flat
    // volume/band fields, three event booleans, and musical timing helpers.
    // Read as often as needed: this never consumes an event or advances time.
    // Optional sensitivity admits weaker events; it does not rescale volume.
    AudioFrame audio(float sensitivity = 1.0f);

    struct Stats {
        uint32_t accepted = 0;
        uint32_t rejected = 0;
        uint32_t duplicates = 0;
    };

    // Initialize storage/UART and start the Core 0 receiver task once.
    void begin();

    Control control();
    Stats stats();

    // Use this for ordinary game input. It includes held buttons plus short
    // presses latched for this frame. A default neutral state is returned
    // when the player is disconnected or its received data has expired.
    GamepadState gamepad(ControllerRole role);

    // Use this when duration matters(for example, holding Home for 500 ms).
    // It contains actual held state, without synthesized one-frame taps.
    GamepadState heldGamepad(ControllerRole role);

    // Copy a fresh 512-byte occupancy grid into the caller's buffer.
    // Returns false if there is no fresh grid for the current mode.
    bool depth(uint8_t* occupancy512);

    // Called by the animation task's checkpoint, not by individual games.
    // Freezes audio (including its musical clock and event booleans) and
    // gamepad presses for the next drawing pass. Clears only pending events,
    // so every read within that frame sees the same results. Returns the
    // matching control snapshot for OS dispatch. No animation calls it.
    Control beginFrame();
}
