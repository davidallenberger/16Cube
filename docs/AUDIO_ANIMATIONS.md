# Cubina audio animation handoff

Work from the actual local checkout of davidallenberger/16Cube, checking HEAD
and local modifications before editing. Preserve unpushed work. Inspect its
source rather than rebuilding a parallel audio interface.

## Ownership

- command-board acquires PCM1808/I2S-microphone samples and produces volume,
  sixteen frequency bands, kick/general-attack events, and estimated tempo.
- render-board draws the selected program. Audio and normal animation are
  separate top-level decks. They NEVER intermix, run concurrently, or create
  another rendering task.
- TestDemo owns playback. TestDemo.cpp retains normal art/billboard scheduling;
  TestDemoAudio.cpp owns the approved audio list, selected entry, shuffle and
  timer. TestDemo.h contains their common bounded shuffle/selection tools.
- Audio.cpp/Audio.h contain static visualization methods, artistic controls,
  icon drawing and readable debug. Effects have no playlist or selected index.
- UARTReceiver/AudioFrame/AudioFrameClock own packet interpretation, consistent
  per-frame inputs and the musical clock. Transport details are private.

## Input contract

Read once per drawing pass:

```cpp
const auto audio = RemoteInputs::audio();
```

Public fields: volumeRaw, volumeSm, volumeAvg, fftResult[16], bpm,
bassKick, midAttack, highAttack. Most visuals should use volumeSm. Optional
RemoteInputs::audio(sensitivity) admits weaker general events without changing
volume/spectrum calibration. Dedicated bass kicks already have a binary decision.

PEAK/ATTACK EVENTS ARE A FUNDAMENTAL FEATURE. The command board's shared
AttackDetector measures sudden rises in linear spectral energy before display
shaping. A separate specialized bass detector feeds the SAME public bassKick
indicator. Events are retained across UART publications and frozen for a frame;
reading twice does not consume them. There is no public Attack object or
consumeBeat/consumeAttack interface. Do not silently implement another onset
or peak detector inside each visualization.

Puddle Peak uses these events to spawn synchronized, randomly placed bursts.
Sensitivity controls which events qualify; density controls burst coverage;
smoothed volume controls individual shape size. Position is decorative. This
makes event timing and size meaningful even though random position encodes no
frequency/location. Horizontal/vertical/cube wrappers share one drawing loop.
Ordinary Puddles is a separate volume-driven effect with continuous spawns.

Tempo helpers:

```cpp
const float pulse = audio.beatPulse(10); // Smooth 0 -> 1 -> 0 over ten beats.
const float angle = audio.beatAngle(10); // 0 to TWO_PI over those ten beats.
```

At 120 BPM this cycle takes five seconds. The shared clock handles estimate
smoothing, phase correction and slow idle fallback. Helpers do not draw or
apply volume. Multi-beat origins are arbitrary, not bar/downbeat detection.
Tempo remains heuristic, with half/double-time and tempo-ramp limitations.
No AGC or semantic instrument/speech recognition is implemented.

## Adding and playing effects

Each effect is a static Audio method with its working state local to that
method. Add an enabled entry to audioRoutines in TestDemoAudio.cpp only when it
belongs in the approved deck. Set enabled=false to retain experimental code
without unattended playback. All previously available entries initially remain
enabled, including the Attack Monitor diagnostic.

The effect must cooperate with TestDemo's lifetime check:

```cpp
void Audio::animateExample() {
    // Any persistent state for THIS invocation belongs here.
    while (TestDemo::audioRoutineRunning()) {
        const auto audio = RemoteInputs::audio();
        // Draw from volume, frequency, event flags and/or timing helpers.
        Audio::debug(audio);
        showCube();
    }
}
```

Timers and button bookkeeping do not belong in the effect. Outside runAudio,
a direct method call is untimed for isolated testing. Mode/source changes still
use the OS's legacy same-task escape: do not hold mutexes or resource-owning
automatic objects across showCube. No extra task is needed for animation work.

Defaults: audio sequential, 30 seconds per entry. Change the defaults in
TestDemo.h or set these on the render task:

```cpp
TestDemo::audioPlayback().order = TestDemo::PlaybackOrder::Shuffled;
TestDemo::audioPlayback().durationMs = 30000;
// Use durationMs = 0 to hold until a repeated source-button press.
```

A repeated button press for the active audio source advances once immediately,
without an OS restart. A source switch preserves the selected visual and starts
its timer anew. Shuffled audio visits every enabled entry once per cycle and
avoids immediate repetition across cycles when more than one entry is enabled.
Normal animation retains its existing art/billboard timing. Its order can be
set through TestDemo::animationOrder(); ordered mode retains the existing
10-second-per-concept regression behavior and shape variants.

## Working constraints

Desktop target: 16x16x16, 4096 voxels, eight parallel outputs with spacer pixels.
Preserve the stable FastLED pinning, physical mapping, frame limiter and existing
arena/task/buffer sizes unless explicitly investigating them. Calibrate source
gain on command-board; tune artistic response separately in each effect.
Prefer clear comments, expanded formatting, one statement per line, no space
between a function name and '('. Avoid per-voxel expensive trig/sqrt where
precomputation or existing helpers suffice. Inspect CubeCore, TextureEngine, Streamers (including its shared BPM renderer)
and Surfaces for the drawing conventions.

Run bash tests/check.sh after relevant changes. Host checks simulate input and
time; they do not prove ESP32 timing or physical appearance. Purpose-built
single-dimension visuals should make it easy to hear/see the causal connection
before combining volume, frequency, attacks and tempo into richer artwork.


## Temporary debug Hold

The command-board Depth / motion-capture button toggles a boolean Hold latch.
It no longer selects depth acquisition. Both boards must run the matching
firmware: the UART header now admits the PLAYBACK_HOLD flag. Source selection,
source readiness, calibration and mode revision are unchanged by this toggle.

Hold keeps the current audio or normal animation routine moving, suppressing
its automatic duration/cycle exit. Finite normal shows repeat the same scene.
Manual Next and source/mode buttons still work, and Hold applies to the next
selected playlist routine until toggled off. On release, an expired routine
finishes at its next normal exit check. Link loss releases the render latch;
reconnection restores the command board's boolean. Games/menu are unaffected.
The command board starts with Hold off on boot.

Add new audio routines at the top of audioRoutines in TestDemoAudio.cpp. The
first enabled entry starts first after render boot in sequential order; Hold
makes it available for extended inspection. Preserve disabled entries unless
explicitly enabling them for playback.

## Fixed-size helper storage

These helpers predate the parameterized drawing engine. They use logical
coordinates and do not bypass spacer mapping. Hardware will never exceed 16
voxels on any axis, so the existing 16-element caches are intentional capacity
limits and do not need expansion:

- TextureState in TextureEngine.h caches hue, RGB distortion waves and squared
  distances in 16-element arrays per axis. Caching avoids repeating shader
  calculations at every voxel. Both 16x16x16 and 10x10x16 canvases fit.
- Streamers::animateRandomFall uses 16x16 position/state arrays for XY columns.
  Planes::animateSplat uses a 16x16 travel-distance map for the two face axes.
  Their active loops use logical dimensions, with unused cache capacity on
  smaller canvases.
- Games/Tetris.cpp has 16-entry layer-fill scratch arrays and a 16x16x16
  game-over occupancy snapshot. Tetris also has deliberate 8x8 logical-board
  rules. Games/Tetris2d.h contains an 8x16 board; these are game-specific
  dimensions, not requests to support larger hardware.

Not every literal 16 is a canvas assumption: FFT bands, palette entries,
font/icon bitmap widths and bounded deck capacities describe other data.
This cleanup documents these limits without changing shader math, game rules
or memory allocations.

## Cleanup validation

Both board firmware builds, the full host test suite and rendering regressions
passed. On November 3, 2026, the user confirmed that hardware tests pass for
this cleanup and the temporary Hold behavior.
