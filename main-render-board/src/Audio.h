#pragma once

#include "CubeCore.h"
#include "TextureEngine.h"
#include "UARTReceiver.h"
#include "BpmRenderer.h"

// Static animation collection, like Streamers and Surfaces. Implementations
// are grouped in Audio.cpp; callers do not need to construct an Audio object.
// Future volume/frequency/kick routines can be added as independent methods.
class Audio {
  public:
    enum class PuddleShape { Horizontal, Vertical, Cube };

    // Render-task-owned controls. Calibrate source gains on the command board;
    // tune these for artistic response without changing that calibration.
    struct Controls {
        float puddlesSensitivity = 1.0f;    // Volume-to-size response.
        float puddlePeakSensitivity = 2.0f; // Shared-event admission.
        float bpmSensitivity = 1.0f;        // Volume-to-brightness response.
        float attackSensitivity = 1.0f;     // Shared-event admission.
        uint8_t maxCubeEdge = 4;            // Clamped to 1..5 and cube bounds.

        // Peak BURST coverage, independent of which attacks qualify. At 1.0,
        // one hit draws roughly ordinary Puddles' current visible footprint.
        // 2.0 doubles the requested shapes; 0 disables peak spawns. Canvas and
        // shape dimensions are accounted for; work per burst remains bounded.
        float puddlePeakDensity = 1.0f;
    };
    static Controls &controls();

    // TestDemo owns playback. These methods draw effects and can also be
    // called directly for isolated development outside a managed deck.
    static void renderIcon(TextureState &texture);

    // Each invocation starts its effect state anew. In the managed audio
    // deck, TestDemo ends it on a timer/button. Direct calls are untimed;
    // CubeSystem's existing checkpoint still handles mode/source escapes.
    static void animatePuddles();

    // Event-driven variant: a bass, midrange or treble attack starts a burst
    // of shapes. Smoothed volume determines each size; density determines
    // how much of the canvas the burst populates. Detection and event
    // retention belong to the audio adapter, not to the animation.
    static void animatePuddlePeak();

    static void animatePuddlesVertical();
    static void animatePuddlesCubes();
    static void animatePuddlePeakVertical();
    static void animatePuddlePeakCubes();

    // Main reusable puddle loop. Call directly to supply sensitivity/geometry
    // externally, or use the playlist wrappers and controls() above.
    static void puddles(bool peakDetect, PuddleShape shape, float sensitivity = 1.0f,
                        uint8_t maxCubeEdge = 4, float peakDensity = 1.0f);

    static void animateBpmSphere();
    static void animateBpmCheckerboard();
    static void animateBpmDiagonal();
    static void animateBpmHelix();
    // Single-dimension diagnostic: a fixed white cube flashes for accepted
    // bass/mid/high events. No tempo, random placement, or volume-driven size.
    static void animateAttackMonitor();
    static void bpm(BpmSpatialMode mode, float sensitivity = 1.0f);

    // General audio diagnostics; reads the same frozen frame as animations.
    // Call regularly to drain its bounded USB queue. Internal static buffers
    // belong only to this method and survive an app's nonlocal exit safely.
    static void debug();
    // Reuse the routine's existing snapshot so the report describes the
    // same audio values that produced this frame.
    static void debug(const RemoteInputs::AudioFrame &audio);

};
