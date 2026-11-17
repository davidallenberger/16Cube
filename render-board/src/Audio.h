#pragma once

#include "CubeCore.h"
#include "TextureEngine.h"
#include "UARTReceiver.h"
#include "BpmRenderer.h"

namespace AudioLayouts {
    // Top view, row-major tile indices. Each successive pair moves clockwise
    // on opposing paths, then turns inward to the central four tiles.
    constexpr uint8_t orbitInwardTiles[16] = {0, 15, 1, 14, 2, 13, 3, 12,
                                             7, 8, 11, 4, 10, 5, 9, 6};

    inline float inwardCoordinate(int block, int column, int extent, int blocksPerAxis = 4) {
        const int first = block * extent / blocksPerAxis;
        const int width = (block + 1) * extent / blocksPerAxis - first;
        const int columnsPerBlock = 16 / blocksPerAxis;
        return first + float(column * (width - 1)) / (columnsPerBlock - 1);
    }
}

// Static animation collection, like Streamers and Surfaces. Callers do not
// need to construct an Audio object.
// Future volume/frequency/kick routines can be added as independent methods.
class Audio {
  public:
    enum class PuddleShape { Horizontal, Vertical, Cube };

    // Render-task-owned controls. Calibrate source gains on the command board;
    // tune these for artistic response without changing that calibration.
    struct Controls {
        float puddlesSensitivity = 1.0f;    // Volume-to-size response.
        float peakAttackSensitivity = 2.5f; // General event admission for Puddle Peak.
        uint16_t checkerBeatsPerCycle = 1;  // 1 = every beat, 2 = half tempo, 4 = quarter tempo.
        float bpmSensitivity = 1.0f;        // Brightness response; timing uses the shared beat clock.
        float cornerBurstGain = 255.0f / 75.0f; // VolumeSm 75 reaches the attached cap.
        float surroundGeqGain = 3.0f;     // Shared lift and frequency bar gain.
        float surroundGeqBassGain = 2.5f; // Center sphere radius gain.
        float particleGeqGain = 3.0f;      // FFT gain for PS GEQ's launch speed and rate.
        float particleGeqBrightness = 2.5f; // Boost particle light before the spatial blend.
        bool spectrumCompensationEnabled = true; // Display compensation for spectrum-driven effects.
        uint8_t frequencyPlanesLevelForFullArea = 85;
        uint8_t frequencyShapesLevelForFullSize = 85;
        uint8_t plasmoidIntensity = 128;    // WLED's # of pixels setting. 128 gives a cutoff of 2 * VolumeSm.
        uint8_t driftRoseVolumeForFullReach = 75; // VolumeSm where the rose reaches its full radius.
        uint8_t bouncingBallsVolumeForFullHeight = 75; // VolumeSm where a hop reaches the ceiling.
        uint8_t shapeVolumeForFullCube = 75; // VolumeSm where the corner cubes reach full size.
        bool midrangeBoostEnabled = true; // Shared by audio Julia, Waverly and Drift Rose.
        float midrangeBoostGain = 3.75f / 14.4f;   // Maximum lift in normalized effect drive.
        float juliaSensitivity = 1.5f;     // Same initial volume gain as Waverly.
        float juliaVolumeGamma = 1.0f;     // <1 boosts quiet levels; >1 favors louder levels.
        uint32_t juliaAttackMs = 100;      // Geometry smoothing time constant on volume rises; 0 bypasses.
        uint32_t juliaReleaseMs = 220;     // Geometry smoothing time constant on falls; 0 bypasses.
        uint32_t juliaStepMs = 40;         // Drawing interval; color timing stays in milliseconds.
        float waverlySensitivity = 1.5f;    // Artistic height gain; no source calibration.
        uint32_t waverlyStepMs = 32;        // Geometry/color update interval.
        float waverlyNoiseRate = 0.12f;     // Noise-coordinate units per millisecond.
        float waverlyAttackSensitivity = 1.0f; // Shared mid/high event admission.
        float waverlyAttackReach = 0.15f;      // Maximum extra reach as a fraction of Z.
        uint32_t waverlyAttackDecayMs = 200;   // Refreshable pulse; never accumulates.
        uint8_t waverlyFade = 55;           // Fade amount per 32 ms, time-normalized.
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


    // Main reusable puddle loop. Call directly to supply sensitivity/geometry
    // externally, or use the playlist wrappers and controls() above.
    static void puddles(bool peakDetect, PuddleShape shape, float sensitivity = 1.0f,
                        uint8_t maxCubeEdge = 4, float peakDensity = 1.0f);

    // One shared Waverly drawing loop. TestDemo selects audition colors;
    // effects do not own the playback cursor or the sequence of combinations.
    // Cycling column gradients use the palette along each column. Otherwise
    // shader selects a normal moving TextureState at its usual 1.0x rate.
    static void waverly(ShaderMode shader, CRGBPalette16 palette,
                        bool cyclingGradient = false, uint32_t durationMs = 0);

    static void animateJulia();
    static void animateDriftRose();
    static void animateCylinder();
    static void animateOctahedron();
    static void animatePlasmoidMirrored();
    static void animateSurroundGEQ();
    static void animateFunkyPlank();
    static void animateFrequencyPlanes();
    static void frequencyPlanes(bool discs);
    static void frequencyCubes();
    enum class FreqwaveShape { WireframeBox, Sphere };
    static void freqwave(FreqwaveShape shape);
    static void animateInwardGEQ();
    enum class BouncingBallsPattern { Clockwise, OrbitInward };
    static void bouncingBalls(BouncingBallsPattern pattern);
    static void animateParticleGeqClusters();
    static void animateParticleGeqTiles();
    static void animateParticleGeqPairedBands();
    static bool displaySpectrum(const RemoteInputs::AudioFrame& audio, uint8_t (&bins)[16]);
    static void animateCornerBurst();
    static void animateCornerBurstSphere();
    static void animateCornerBurstBox();

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
    // A nonzero attack number adds a timestamped record to the serial stream.
    static void debug(const RemoteInputs::AudioFrame &audio, const char *detail = "",
                      uint32_t attackNumber = 0, const uint8_t* displayBins = nullptr);

private:
    struct VolumeCubes;
    struct CylinderResponse;
    struct OctahedronResponse;
    static constexpr float cubeSizeReleaseMs = 45.0f;
    static float trackShapeSize(float previous, float target, float retained);
    static void drawGeqBars(bool inward);
};
