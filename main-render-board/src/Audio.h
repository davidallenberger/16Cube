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
        float puddlePeakSensitivity = 1.0f; // Onset trigger sensitivity.
        float bpmSensitivity = 1.0f;        // Volume-to-brightness response.
        uint8_t maxCubeEdge = 4;            // Clamped to 1..5 and cube bounds.
    };
    static Controls &controls();

    // Run the selected entry in the audio routine list. It remains selected
    // when acquisition switches between microphone and the wired ADC input.
    static void run();
    static void renderIcon(TextureState &texture);

    // Called by CubeSystem on the animation task BEFORE escaping the old
    // routine. Advance once and wrap at the end of the list. This method
    // changes the selection only; run() launches it after the old app exits.
    static void nextRoutine();

    // Readable selection for diagnostics; render task only, like run/next.
    static const char *currentRoutineName();

    // Current default effect. Each invocation starts its animation state anew.
    // It runs until the existing CubeSystem checkpoint exits the active app.
    static void animatePuddles();

    // Sound-onset variant: a strong increase in ANY of the 16 bands triggers
    // one line. Sustained levels do not repeatedly trigger just for being loud.
    // Smoothed volume sets length; the bass-kick detector is independent.
    static void animatePuddlePeak();

    static void animatePuddlesVertical();
    static void animatePuddlesCubes();
    static void animatePuddlePeakVertical();
    static void animatePuddlePeakCubes();

    // Main reusable puddle loop. Call directly to supply sensitivity/geometry
    // externally, or use the playlist wrappers and controls() above.
    static void puddles(bool peakDetect, PuddleShape shape, float sensitivity = 1.0f,
                        uint8_t maxCubeEdge = 4);

    static void animateBpmSphere();
    static void animateBpmCheckerboard();
    static void animateBpmDiagonal();
    static void animateBpmHelix();
    static void bpm(BpmSpatialMode mode, float sensitivity = 1.0f);

    // General audio diagnostics; never consumes a kick or a button event.
    // Call regularly to drain its bounded USB queue. Internal static buffers
    // belong only to this method and survive an app's nonlocal exit safely.
    static void debug();
    // A routine may append a short diagnostic description. For BPM this
    // reports measured work time, showCube time, and the actual frame rate.
    // The description is copied into the report immediately, never retained
    // as a pointer after this call. Existing debug(audio) calls still work.
    static void debug(const RemoteInputs::Audio &audio, const char *details = nullptr);

  private:
    struct Routine {
        const char *name;
        void (*run)();
    };

    // The list and selection live in method-local static storage. They belong
    // only to the animation task, so the UART task needs no access or mutex.
    static const Routine *routineList(size_t &count);
    static size_t &selectedRoutineIndex();
};
