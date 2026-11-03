# Cubina audio animation handoff

Work from the current pushed main branch of davidallenberger/16Cube. Inspect
its actual source rather than rebuilding a parallel audio interface. The
playback refactor was prepared from 32b70f1c994be3b0543e229b878c1ff93d2113a6;
its later tested commit will be the new baseline once installed and pushed.

## Ownership

- command-board acquires PCM1808/I2S-microphone samples and produces volume,
  sixteen frequency bands, kick/general-attack events, and estimated tempo.
- main-render-board draws the selected program. Audio and normal animation are
  separate top-level decks. They NEVER intermix, run concurrently, or create
  another rendering task.
- TestDemo owns playback. TestDemo.cpp retains normal art/billboard scheduling;
  TestDemoAudio.cpp owns the approved audio list, selected entry, shuffle and
  timer. PlaybackDeck.h provides their common bounded shuffle/selection tools.
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
precomputation or existing helpers suffice. Inspect CubeCore, BpmRenderer,
TextureEngine, Streamers and Surfaces for the drawing conventions.

Run bash tests/check.sh after relevant changes. Host checks simulate input and
time; they do not prove ESP32 timing or physical appearance. Purpose-built
single-dimension visuals should make it easy to hear/see the causal connection
before combining volume, frequency, attacks and tempo into richer artwork.
