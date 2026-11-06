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


## Waverly color audition

Waverly is registered once, first in the audio deck. Each visit randomly
selects one of six approved color mechanisms with equal probability; the five
palette-driven mechanisms independently choose Rainbow, Lava, Forest, Ocean,
or Party. Hiphotic uses its native colors. The 26 approved combinations remain
available, but there is no longer an exhaustive audition sequence.

Next and normal expiry advance to the next audio routine. The normal managed
duration is 30 seconds; duration=0 and Depth Hold retain their existing behavior.
Source/mode restarts retain the selected look. The selected shader/palette is
reported in the routine name. Moving textures run at 1.0x; the column-gradient
cycle retains one palette step per 80 ms. Uniform Cycle, Spatial Gradient,
Scatter, solid colors, noncycling gradients and frozen variants are excluded.

All looks use the same two-spatial-axis noise field and initial noise phase.
Smoothed volume controls vertical reach; Z is up. Spatial noise scale spans
three cells across either horizontal axis, and reach scales with RNDR_Z.
Each lower voxel has a full-inversion partner with the same color. Gradient
looks map palette index along each column; other looks use existing textures
without modifying their math. Live music can still differ between auditions.

Audio::controls() exposes waverlySensitivity (height gain, default 1.5),
waverlyStepMs (32), waverlyNoiseRate (0.12) and waverlyFade (55 per 32 ms).
These fixed artistic settings are unchanged from the initial audition; source
calibration and smoothing are unchanged. Geometry updates are paced separately
from display/checkpoints; trails decay by elapsed time. Silence adds no light.
Spacer scanning remains disabled in render-board/platformio.ini; the tested
four-DMA-buffer/UART baseline is retained.

Build and host checks do not establish the appearance or update rate on real
hardware. Host texture/noise substitutes verify geometry/control contracts;
the firmware build compiles the real FastLED noise and TextureState modes.

With the FFT fallback switched off, Waverly refreshes a bounded reach pulse
for shared mid/high attacks. Bass
events alone do not trigger it. waverlyAttackSensitivity defaults to 1.0,
waverlyAttackReach to 0.15 of Z (clamped to 0..0.25), and
waverlyAttackDecayMs to 200. The extra reach follows the same noise contour,
decays linearly and never stacks; simultaneous/repeated events refresh one
pulse. In this mode, setting attack reach to zero leaves only volume and the
idle planes. Debug reports
mid/high flags and the pre-noise push in voxels. Volume gain, source smoothing,
noise rate and gradient cycling are unchanged.

## Deferred tuning observations: desktop ADC, November 3, 2026

The user reports that Waverly looks excellent on rhythmic passages and wants
more routine variety before further tuning. Keep Gradient Cycle speed and the
current volume/attack settings as the starting point. The recorded desktop
Mac/Spotify levels are part of the baseline; speaker loudness is not the ADC
measurement. Change one level/control at a time when returning to calibration.

A supplied capture contains 497 Audio reports: VRaw spans 1..158, VSm 1..144,
Bass=1 appears in 34 reports, and Mid/High are zero in all sampled reports.
One Waverly report shows push=1.60 despite current mid=high=0, establishing a
recent admitted mid/high event followed by pulse decay. RXbad stays zero.
These are sampled diagnostics, not counts of detected attacks or proof of
absence of clipping in the upstream audio path.

Audio::debug samples a single frame every 500 ms. The command-board kick LED
holds each dedicated kick for 80 ms, while render event notices are latched
until a drawing frame and then drained. Most brief notices can therefore be
missed by the serial snapshot. Bass=0 on a printed line is compatible with a
visible LED flash and a successful volume-driven wave. Later diagnostics should
count events over the reporting interval, distinguish generated/admitted events,
and report peak strength/push rather than merely sampling booleans.

The shared attack detector measures sudden positive spectral-power increases,
not vocals, pitch, vibrato, or all audible modulation. It weights the lowest
four bands by 3, gates novelty against the whole signal, and labels contributing
bands only when each supplies at least 10% of the total rise. Rearming and an
80-ms minimum event interval are shared. Mid/high changes can be masked by
stronger low-frequency changes; local animation sensitivity cannot recover a
candidate the command board never emitted. Spectrum display EQ/gamma does not
change these pre-EQ measurements.

For later work: use fixed track excerpts (vocal-dominant, percussion-dominant,
and mixed) at documented source levels; improve interval/event diagnostics
before changing detection; then consider region-relative novelty or regional
energy envelopes for sustained vocal expression. Keep source calibration,
shared detection, and artistic pulse amount/decay separate. Any new audio
measurement belongs in the shared acquisition/interface foundation, not a
private detector inside Waverly.

## Checkerboard volume-controlled pulse speed

Audio BPM Checkerboard retains the original density-wave renderer and repeating
pulse. Smoothed volume controls oscillator speed, rather than column reach.
checkerMinHz defaults to 0.2 cycles/second (quiet), checkerMaxHz to 2.0 (full
volume). bpmSensitivity scales volume before this mapping. Phase integrates
elapsed time continuously, so volume changes alter speed without resetting or
jumping the pattern. Sustained volume keeps pulsing at a steady rate. Silence
keeps the slow pulse visible. Overall level is fixed for this experiment;
palette movement retains its previous rate. Debug reports pulse frequency and
volume. Other BPM styles and Waverly are unchanged.

## GravityMeter

GravityMeter is the first audio deck entry. Each invocation randomly chooses
Sphere, Box or Merkaba and a moving texture/palette once. The moving texture
helper selects Linear Flow, Noise Field, Distance Field, Hiphotic or Distortion
Waves; palette-driven modes can use Rainbow, Lava, Forest, Ocean or Party.
Hiphotic uses native hues. Texture phase advances at 1.0x.

volumeSm is 0..255. gravitySensitivity (default 1) scales it and the result maps
linearly to a floating-point radius. There is no additional size smoothing,
attack impulse, peak-memory outline or automatic rotation. Zero volume clears
the display. Sphere and Box use the existing solid renderers; Merkaba retains
its existing interlocking surface renderer. The geometry engine's coverage
softens discrete voxel steps; it does not create more physical resolution.

gravityFromUpperCorner defaults to false. When true, one of four upper corners
is randomly chosen per invocation as the expansion origin; the out-of-cube
part is clipped by drawing only logical voxels. The shortest logical span sets
the maximum radius: half-span centered, full-span from a corner. On 16 cubed
these are 7.5 and 15, respectively. Shapes stay isotropic on the 10x10x16
canvas, so shorter axes constrain the reach. A sphere need not fill all corner
voxels at full scale. Octahedron is a further symmetric candidate, not enabled
in this initial three-shape set.

gravityStepMs defaults to 16; showCube checkpoints continue between updates.
Direct calls accept a shape, origin, shader, palette, duration and corner index.
Managed playback retains normal duration/Next/Hold behavior. Bounded debug
reports include shape, origin/corner, radius/max radius, gain, shader and palette.
Host checks verify dispatch and radius/origin mapping for all three shapes
and all corners on both canvases; the firmware build uses the actual geometry
engine. Hardware appearance remains to be evaluated.

Ovoid, Tetrahedron, Octahedron, Torus and Hemispheres have been re-enabled in
TestDemo for renewed visual evaluation. Their existing motion/render/color
rules are retained. GravityMeter's Merkaba overrides arbitrary shaders with
MODE_SOLID and the same 50/50 four-color symmetry or curated component-split
choice as TestDemo. PaletteUtils::getMerkabaPalette is the common builder used
by Shapes' TestDemo path and GravityMeter. The existing geometry sentinel
interpretation is unchanged.

## Shape review and four-streamer update

Torus and Ovoid are disabled again after hardware review. Octahedron is restricted to Spinning (Z-axis) and Solid, with its existing
curated contrast colors. Static and Wireframe variants are disabled.

Vertical Streamer now maintains four independent paths, histories and color
plans. Each brush uses one quarter of the original footprint allocation:
16x16 gives each the old 8x8 brush radius; other footprints scale proportionally.
The first stream retains the supplied palette/shader (randomized by TestDemo),
and three others choose independent moving texture plans once on entry. Paths can traverse the full footprint and blend additively when overlapping. Motion/history cadence and
the duration/Hold/checkpoint mechanism retain the existing behavior.

Run python3 tests/vertical_streamer_test.py for the production-loop host checks
on 16x16x16 and 10x10x16. These verify independent plan setup and bounded drawing,
not the hardware appearance of the chosen palettes.

Tetrahedron and Hemispheres no longer allow Static playback. Both retain
Expanding/Collapsing and Tumbling. Tetrahedron allows Solid or Wireframe during
expansion, but tumbling forces Solid. Hemispheres remains Solid-only.

Vertical Streamer restores full-footprint wandering for all four streams.
Quadrant limits and repulsive forces are removed. Contact between heads
(within two brush radii) redirects each touching head toward the best of twelve
random destinations, scored by clearance from other heads and destinations.
A 300-ms per-head cooldown gives the chosen direction time to take effect;
persistent contact retries afterward. Normal destination arrival chooses a
single random destination. Existing brush sizes, colors, histories and the
travel-speed cap are retained. Trail crossings can still occur; this redirects
new growth rather than rewriting previous layers. Tests cover continued
motion, bounds and collision recovery from coincident starting positions on
both canvases. Hardware appearance still needs evaluation.

## Consolidated Puddles coloring

The audio deck retains one Puddles entry and one Puddle Peak entry, with a
random horizontal/vertical/cube geometry selected once per invocation.
Coloring is restored to the original solid-per-spawn regime: CHSV hue advances
once per spawn, and all shapes in a Peak burst share that solid color. Existing
pixels retain their spawned color and fade with the original 250-ms time
constant. Spatial and moving texture sampling are removed. No live recoloring
of fading shapes is enabled. Triggering, sizing, density and cadence are
unchanged. Bounded debug reports geometry and solid-per-spawn coloring.

## Deferred breathing/tempo observations, November 4, 2026

A supplied capture has 57 reports, all BPM Sphere with Hold=1. The displayed
rate stays near 64.2 initially, then rises through 104.1, 118.4 and 123.5 before
settling near 126.3. This is consistent with a half-time estimate transitioning
to the faster interpretation, not proof the song changed tempo. RXbad remains
zero. These are the Sphere's tempo-clock reports, not the volume-speed
Checkerboard experiment. The log lacks raw tempo confidence/lock and phase
error, so it cannot establish musical correctness or synchronization by itself.
The user judges approximately 126 more accurate. Future exploration should
distinguish stable pulse rate from bass-kick phase alignment, protecting the
existing dedicated kick detector. No tempo/detector changes made here.


### Shared kick-guided tempo (November 2026)

TempoTracker now receives the existing dedicated bass-kick event directly from
acquisition. The kick detector, general attack detector, volume calibration and
wire format are unchanged. All BPM consumers receive the resulting shared clock.

The last eight inter-kick intervals are checked for agreement: at least four
must directly support a period in the 60–200 BPM range, and at least 75% must
support that period or a two/three-beat gap. A supported train takes precedence
over spectral autocorrelation, reducing half-time estimates caused by alternating
accents and phase alignment to treble offbeats. Establishing the train anchors
phase to its kick; subsequent nearby kicks gently correct drift. Off-grid hits
do not reset phase. Missing kicks free-run; absent or irregular kicks release
control to the existing spectral estimate. Storage and work are bounded, without
allocation or another FFT. Checkerboard again uses the shared clock (one breath
per beat); volume affects brightness, not pulse speed. The experimental
checkerMinHz/checkerMaxHz controls are removed.

This does not resolve musical ambiguity when the kick pattern itself is half-time,
syncopated, or consistently subdivided. Detection latency remains, and synthetic
validation is not proof of correct interpretation of a particular recording.
Flash BOTH command-board and render-board to audition this change. No protocol
migration is needed. Hardware acquisition WorkMax/headroom still needs checking.


Checkerboard supports a configurable number of beats per full breath, including spatial travel.
After auditioning half tempo, the selected default is back to one beat.
`Audio::controls().checkerBeatsPerCycle` selects 1 (every beat), 2 (half tempo),
or 4 (quarter tempo); zero is treated as 1. At 126 BPM, setting 2 gives 63 full
breaths per minute. Other BPM patterns retain one beat per cycle. This setting
changes animation timing only, leaving the shared tempo and beat alignment intact.


The audio deck now registers one Breathe card. Each visit selects Sphere,
Checkerboard, Diagonal or Helix with equal probability. Hold and source/mode
restarts preserve that selection; Next or expiry leaves the card and permits a
fresh selection next visit. Debug output names the selected pattern. All use
the shared musical clock, with Checkerboard retaining the selected one-beat setting.

### Julia 3D first audition

The last normal animation concept (52) is Julia 3D, implemented as
`Surfaces::animateJulia(durationMs, speedMs=40)`. Normal animation playback now
defaults to `PlaybackOrder::Descending` in `TestDemo::animationOrder()`: Julia,
Tetris, Rubiks, then backward through enabled concepts, wrapping at the front.
Ordered scenes retain their ten-second duration and shape motion exhaustion.
Next advances backward once; Hold keeps Julia calculating and moving. Audio deck
ordering is unchanged. Sequential (forward) and Shuffled remain available.

Each normalized physical voxel starts a direct three-coordinate recurrence:
`x'=x*x-y*y-z*z+a`, `y'=2*x*y+b`, `z'=2*x*z+d`, using old coordinates throughout.
The shortest canvas span defines isotropic scale. A slowly rotating sampling
volume and smooth c=(a,b,d) variation animate the geometry. The starting c
neighborhood follows WLED's complex Julia, with an additional evolving third
component. Squared escape radius is 16; iterations are capped at 24 per voxel.
Fast exterior escape (fewer than four iterations) and bounded points are dark,
revealing a band of late-escaping structure. Fractional escape time supplies
palette position and softened edge brightness. Each visit chooses one of
Rainbow/Lava/Forest/Ocean/Party, with no independent palette cycling yet.

Once per second, debug reports c, palette, occupied-voxel count and frame time
(including showCube). The target frame interval is 40ms, longer if calculation
and output require it. No extra allocation or arena storage is used. Host tests
exercise the actual animation body on 16x16x16 and 10x10x16, including bounds,
sparse coverage, evolving geometry, palette variation, duration, Hold and clock
wrap, and descending selector skip/wrap behavior. ESP32 speed and aesthetics
still require a hardware audition. Only render-board changes are required.

Future audio experiments: use smoothed volume to travel along a curated c path,
or use musical timing to advance a palette offset. Changing c can increase or
decrease occupancy and alter connectivity; arbitrary c motion is not a monotonic
volume-to-size control. First inspect this non-audio geometry before choosing
that path. The current implementation intentionally has neither audio mapping.

Julia cavern revision: use WLED slider-equivalent iterationSetting=18 (nine
iterations), remove rotation, and color ALL escaping points at full palette
brightness. Bounded points remain black. The fixed view now samples real X
[-1.2,1.2], imaginary vertical Z [-0.8,1.0], and third-component depth Y
[-0.35,0.35]. These intentional per-axis viewing ranges replace the initial
isotropic object presentation: the colored exterior reaches the top/bottom,
while the narrow depth window exposes the black opening through the cube.
Coordinates vary continuously in all three dimensions; this is not extrusion.
The parameters c still evolve slowly. No audio response or palette cycling has
been added. Debug reports the actual iteration limit. The test checks lit top
and bottom planes, a darker middle opening, changing occupancy and bounds on
both canvases. Physical appearance still needs auditioning.

Julia rainbow/cavern revision: color is now fixed to the 16 RGB entries of
WLED v16.0.1 RainbowColors_gc22 (palette 11), from wled00/palettes.cpp. Those
entries compensate for WLED's gamma pipeline and differ from FastLED's stock
RainbowColors_p. This uses the WLED palette values without changing the cube's
output pipeline; identical apparent colors on different hardware/gamma settings
are not guaranteed. Palette selection has no randomness, shader or cycle offset.
Index is now WLED Julia's integer `iteration*255/maxIterations` (with linear
palette interpolation), replacing fractional escape-time coloring. The actual
iteration limit stays nine (slider 18). Bounded points remain black.

The fixed window is now real X [-0.9,0.9], vertical imaginary Z [-0.7,0.9],
and third-coordinate depth Y [-0.24,0.24]. This zooms closer into the opening:
at the starting parameter, 16-cube black occupancy increases from 860/4096
(21%) to 1422/4096 (35%). It preserves colored ceiling/floor regions while
exposing more cavern. Tests verify the fixed WLED palette, integer index mapping,
no random color selection, at least 30% initial black occupancy and a majority
black central plane on both canvases. These are numerical appearance checks;
actual viewing and gamma balance require hardware audition.

Julia warm-cycle audition: the default now combines evolving c and continuous
palette cycling. The fixed palette follows red/orange/yellow/red/pink/magenta,
with a smooth return to red. Escape counts 2..8 span this sequence; every lit
voxel receives the same time-based palette offset. A full cycle takes 8.192s
(colorStepMs=32). No random palette/shader selection or spatial rotation occurs.
`cRate=2.0` doubles the prior speed along the same parameter path, preserving
its amplitudes, viewing window and nine-iteration geometry definition.

`Surfaces::animateJulia(durationMs, speedMs, cycleColors, varyC)` defaults both
booleans to true. Set cycleColors=false to audition shape motion alone; set
varyC=false to freeze c at its initial value and audition cycling alone. Both
false gives a stationary image. These arguments can be set at the Julia call
in TestDemo.cpp; duration/Hold/Next remain managed as before. Tests compare
cycling-only vs stationary renders and verify identical lit masks with changing
colors, as well as varying geometry in the default mode. Audio mapping and a
curated open/closed c path remain later experiments.

Julia fuller-spectrum/closure revision: replace the warm-only palette with a
fixed ordered red/orange/yellow/green/cyan/blue/indigo/violet/magenta cycle.
The 8.192s cycle and independent cycling/geometry switches are retained.
A smooth closure excursion now adds `0.24*(0.5-0.5*cos(cTime*0.50))` to c's
imaginary component. This returns to the familiar open starting neighborhood
and reaches substantially more closed states, without scaling or masking the
fractal. At cRate=2 the added excursion repeats every approximately 6.28s;
the other parameter oscillations still vary the contours across cycles.
The iteration limit remains nine. Over a ten-second test on both canvases,
black occupancy falls below 15% at closure, clear front-to-back paths below
5% of the front view, and open-state clear paths exceed 20%. Tests also verify
that every palette bin is visited during cycling. This is a more closed path,
not a claim to find the globally most closed Julia parameter.

Animation development hold: `TestDemo::animationHoldUntilNext()` defaults true
in TestDemo.h. The selected animation (currently Julia, last slot/descending)
runs until manual Next or a mode switch. Next selects and holds the next card.
Set the setting false for timed suite playback. The runtime duration override
is scoped to an active normal animation scene and cleared by stopAnimation();
it does not set the command-board playbackHeld latch or affect audio/menu
routines. Finite scenes repeat their selected parameters while held.

Julia wider opening: the fixed imaginary vertical window is now symmetric
[-0.8,0.8]. The c path interpolates from open (-0.66,0,0) to closed near
(-0.94299,0.5562,0); small contour variations fade out at endpoints. The existing
cRate=2 and closure timing remain. At the exact open endpoint, the two central
X strands on both front/back Y edges have only Z=0 and Z=15 illuminated: one
pixel at each end, with fourteen dark pixels between. This is verified in the
production-body test on both 16x16x16 and 10x10x16. It does not imply every
strand has the same cap thickness. Closure tests and full-spectrum cycling
remain in place. Tests also exercise held Next stepping and stop cleanup.

Julia endpoint correction: the user observed completely dark 16-pixel center
strands between the previously tested front/back edges. Extend the vertical
sampling window from [-0.8,0.8] to [-0.85,0.85], preserving the open c endpoint.
At that endpoint, all central strands throughout Y now have exactly the top
and bottom pixel lit on both supported canvases. During motion, tests require
at least one lit voxel on each central strand, preventing the observed 16-dark
case. Previously only the depth-edge strands were checked.

Interpret the requested 10% closure limit as at least 10% dark volume (no more
than 90% lit). Reduce the imaginary closed endpoint from 0.5562 to 0.54. The
production-body test now runs 60 seconds/1500 frames per canvas and verifies
this occupancy bound on every frame, while still reaching over 85% lit at the
closed end and preserving the broad opening. This is a calibrated parameter
path, not a per-frame clipping mask or a universal bound for arbitrary edits
to c/window/iteration settings. Color cycling and animation hold are unchanged.

Julia closure target is now at least 20% dark (at most 80% lit), replacing the
10% target. Set the closed imaginary c endpoint to 0.42. The same 60-second
production-body tests verify the 80% lit ceiling on every frame on both canvases
and confirm the closed state still exceeds 75% lit. Open-state center caps,
cycle speed, color sequence and development hold are retained.

Julia canonical-color audition: replace the interpolated spectrum palette with
seven directly selected RGB swatches: red FF0000, orange FF8000, yellow FFFF00,
green 00FF00, blue 0000FF, indigo 4B0082, violet 9400D3. Escape count minus two
selects a band; a shared whole-color offset advances the bands in that order.
There are no cyan/magenta inserts, interpolation or intermediate transition
shades. Each step lasts 1160ms, giving a full cycle of 8.12s, approximately the
previous cycle duration. All bands step together while remaining different
colors. Shape changes can still move a voxel between escape bands independently
of the color timer; equal dwell applies to a fixed escape band, not necessarily
to any particular voxel while c is moving. The geometry/cRate, hold setting and
20% dark closure target are retained.

Tests now validate that only the seven swatches are emitted. With c frozen,
a full cycle gives each color exactly 29 rendered frames at the 40ms target,
and cycling changes colors while preserving identical geometry masks. The
60-second bounds, center-strand and occupancy checks still pass on both canvases.

Julia short hue transitions: each canonical color is held for 960ms, followed
by a 200ms saturated hue-space transition to the next color. A smoothstep
progress curve eases the transition endpoints; hue travels in spectrum order,
including violet->magenta->red at wrap. Brightness/value interpolates between
the canonical swatches, preserving the darker indigo endpoint. This is not
an RGB channel crossfade. Canonical holds retain the exact prior RGB values.
Total cycle duration remains 8.12s. Color conversion is done for seven band
colors once per frame, outside voxel iteration. cycleColors=false retains
static canonical bands. Geometry/c motion/20% dark target are unchanged.

The production-body test permits intermediate colors only in the transition
portion, checks full saturation, and verifies equal canonical dwell for all
seven colors with c frozen. It also retains the 60-second geometry bounds and
occupancy checks on both canvases, plus the cycling-vs-static mask comparison.


Julia transition timing is now 400ms, retaining the 960ms canonical hold.
Each color step is 1360ms and the full seven-color cycle is 9.52s. Equal-dwell
and transition-window tests use the updated timing; geometry is unchanged.


Julia transition timing is now 650ms with the 960ms hold retained: 1610ms
per color, 11.27s per complete cycle. Tests allow 40ms frame quantization
when checking canonical hold durations.

### Julia Volume audio card

The first audio deck card is now Julia Volume (`Audio::animateJulia`). Smoothed
volume replaces the timed cosine controlling the position on the approved c
path: louder input fills more of the cube/narrows the cavern, silence selects
the open endpoint, and saturated input selects the closed endpoint. Parameter
closure stays in [0,1]; the existing window and c endpoints retain the target
20% dark bound. Small contour variation continues independently between the
endpoints, like Waverly's continuing noise motion at constant volume. This is
volume-driven movement, not a new BPM or onset detector. No extra envelope is
added after the shared volumeSm measurement.

Controls in Audio.h: juliaSensitivity=1.5 (Waverly's initial artistic gain),
juliaVolumeGamma=1.0 (initially linear), juliaStepMs=40. Mapping is
`pow(clamp(volumeSm*juliaSensitivity/255,0,1),juliaVolumeGamma)`.
Gamma below 1 emphasizes quiet input, above 1 reserves closure for louder
input. Gain clips to [0,8], gamma to [0.25,4], and nonfinite controls fall back
to defaults. This curve shapes c-path position, not actual lit-voxel percentage;
fractal occupancy itself is nonlinear. Hardware/source calibration is unchanged.
Debug includes measured volume, mapped closure, gain and gamma for later tuning.

The approved 4-second color holds and 2-second saturated hue transitions remain
independent of volume. The non-audio Julia card remains last in the animation
deck with timed c motion and development Hold. Both call the same JuliaRenderer
in Streamers.h, next to shared BpmRenderer; Surfaces::animateJulia remains the
normal entry point. The audio wrapper supplies plain callbacks for closure and
managed duration/Next, without allocation, private timers or detector state.

Validation includes real receiver->audio snapshot->Julia integration for quiet,
full and intermediate volume; static endpoint occupancy; gain/gamma behavior and
nonfinite controls; deck membership/timers/Next/source preservation; and the
shared renderer at 17 independently supplied closure levels on both canvases.
Bounds/center-strand checks cover arbitrary closure levels and contour phases,
not just the old timed cosine path. Only render-board needs flashing.


Waverly palette curation: exclude Lava, Forest and Ocean as too dull. The card
now chooses Rainbow or Party for its five palette-driven mechanisms, plus
Hiphotic's native coloring (11 approved looks). Equal mechanism weighting is
preserved. Other routines and shared palette-selection helpers are unchanged.

### Deferred listening note: vocal/midrange response

User observation: Waverly and Julia Volume still do not respond as desired to
midrange/vocal activity; the vocal-range behavior is not yet right. Revisit
this after broader animation auditions rather than tuning it now. Evaluate
vocal modulation/phrasing and other interesting non-bass activity, not just
overall loudness or kick response. Waverly currently has its optional shared
mid/high onset push; Julia Volume currently follows volumeSm only, with no
separate midrange/vocal input. These are different mechanisms and neither
should be treated as a finished vocal-response solution. No behavior changed
in response to this note.

Julia Audio jitter compensation: add exponential smoothing to the geometry's
closure coordinate, after volume gain/gamma mapping and before deriving c.
Audio controls juliaAttackMs=100 and juliaReleaseMs=220 are time constants
(about 63% of a step after one time constant, not fixed delays). Frame elapsed
time determines the interpolation amount, so speed does not depend on render
frame rate; unsigned elapsed arithmetic handles millis wrap. The response
cannot overshoot the target or leave [0,1]. Each invocation initializes from
its first input rather than retaining geometry from a previous audio session.
Set both timings to zero to compare the old direct response.

This is effect-local motion smoothing: shared volumeSm, Waverly, BPM/onset
detectors and the normal timed Julia remain unchanged. Color timing and c
endpoints are retained. The bounded audio debug reports target closure; the
once-per-second Julia renderer log reports actual smoothed closure with c.
Tests cover volume-step rise/fall, zero-time bypass, endpoint initialization,
and identical geometry through millis wrap on both canvases, in addition to
the existing color, occupancy, duration and deck checks. This addresses abrupt
parameter changes; the binary fractal boundary still requires hardware audition.


Julia smoothing audition: increase juliaAttackMs to 300 and juliaReleaseMs to
600 after the first 100/220ms setting remained jittery. Larger time constants
mean smoother/slower parameter movement; attack controls increasing closure,
release controls reopening. If stronger parameter smoothing adds lag without
removing flicker, investigate fading the binary voxel boundary instead of
continuing to increase these constants. This timing-only adjustment was not
built or tested.


Julia smoothing decision: retain the previous 100ms attack / 220ms release
values. The stronger 300/600ms audition is reverted. The animation is accepted
for now; further boundary smoothing is deferred. No tests/build run for this
configuration-only restoration.

### Corner Burst audition

Corner Burst is the last audio card and opens first in descending order. A
dim fill follows smoothed volume from a fixed corner. The diagonal front
holds its crest while volume stays at least 12% or four points below it,
whichever is larger. After 40 ms below threshold the front releases at that
position, keeps its release hue, and travels independently toward the far
corner while fading. One front travels at a time. A fresh rise rearms it
after travel completes. The attached front and fill cycle rainbow hues.

The single card randomly selects Plane, Sphere, or Box on entry. All launch
one bottom corner clockwise from the original and use 15% fill brightness.
The centered box outline with moving or cycling texture and overlapping
fronts remain separate experiments.

If another effect needs the moment after a volume crest, factor Corner
Burst's release detection into a shared cue. Preserve the sustained-drop
confirmation and fresh-rise rearm. Puddle Peak already consumes shared
attack events, which mark the start of a hit.

### Midrange FFT fallback in Waverly and Julia Volume

Waverly and Julia Volume use the render-board detector in MidrangeBoost.h.
It measures rises above recent levels in FFT bands 2–9 and suppresses the
lift when bass or smoothed volume is strong. Each playback keeps its own
detector history.

Audio::controls() provides one switch (midrangeBoostEnabled, default true)
and one strength (midrangeBoostGain, default approximately 0.2604) for both effects. Waverly
maps the lift to 3.75 voxels of maximum reach on a 16-high cube before noise.
Julia adds the same normalized lift before closure gamma and motion smoothing.
With the switch off, Waverly uses its prior attack push and Julia uses volume.

Artistic volume gains belong to each effect. Input calibration on the command
board affects the source signal shared by the audio deck. Other render
effects can opt into the same detector and map its lift to their own motion.


### Idle rendering and Puddle Peak tuning

Waverly keeps both outer planes lit. Surround GEQ keeps one row per bar
and a small center sphere, with the peak cap fading in above the idle row.
Corner Burst draws one rainbow-cycling voxel at its origin for all three
front shapes, including between waves. These are render-side visual floors.

Puddle Peak defaults to 2.5 event sensitivity and applies a 1.25 size gain
to smoothed volume before rounding line lengths and cube edges to voxels.
Sensitivity admits weaker shared attacks, while size gain affects the
geometry of bursts that already fired.
