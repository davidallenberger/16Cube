# Data flow and the audio contract

[Documentation index](../README.md) · [Architecture](SYSTEM.md)

## From input to a display frame

1. `SignalAcquisition` polls debounced buttons and gamepads, configures the
   active audio source, reads PCM, and computes features. It publishes through
   `CommandState`, whose mutex protects short state copies.
2. `UARTSender` takes the latest snapshot, retains short-event information,
   explicitly encodes the packet, and sends it over Serial2.
3. Render `UARTReceiver` validates bytes and updates private caches, freshness
   times, and pending event latches under its own mutex.
4. `CubeSystem::frameCheckpoint()` calls `RemoteInputs::beginFrame()` on the
   render task. It freezes matching control/audio/gamepad state and releases
   the mutex before any app escape or drawing.
5. The renderer reads `RemoteInputs::audio()` and/or gamepad/depth accessors,
   updates its state, draws, and calls `showCube()` for output and the next
   checkpoint. Repeated audio reads within the drawing pass are stable.

The snapshot is the latest received state, not a lossless queue of every FFT
window. Multiple attacks between frames can coalesce into one visible event.

## Wire format and event semantics

[`SharedProtocol.h`](../../shared/SharedProtocol.h) is the only wire-format
source. Current version is **3**. Packet fields use explicit little-endian
serialization; sending `sizeof(AudioPayload)` would include layout-dependent
padding and is prohibited.

| Element | Current contract |
| --- | --- |
| Framing | COBS-encoded bytes followed by a zero delimiter |
| Integrity | CRC-16/CCITT-FALSE, polynomial 0x1021, initial 0xFFFF |
| Header | 22 bytes: version, type, mode, flags, payload length, session, mode revision, sequence, captured timestamp |
| Types | Status, Audio, Gamepads, Depth |
| Payload lengths | Status 0; Audio 38; Gamepads 38; Depth 512 bytes |
| Transport | 921,600 baud, UART 8N1 |
| Scheduling | At most one packet submission per 16,667 µs, including mode/status packets |
| Desired source cadence | Audio/game 16,667 µs; depth 33,334 µs; status heartbeat 200,000 µs |
| Parser recovery | Bounded buffers, overflow discard through delimiter, 50 ms inter-byte timeout |
| Fresh data | Receiver data expires after 250 ms; link expires after 1,000 ms |

Audio's 38 bytes comprise the original 21 feature bytes (mode, beat,
volumeRaw, volumeSm, average, 16 bands), a 32-bit kick counter, tempo BPM×100,
phase, confidence/flags, and general attack count/strength/band mask. The
session changes on command reboot. Revision changes for each mode demand,
including a repeated mode press. Sequence changes with transmitted packets.
These counters have different meanings and must not be substituted.

Receiver ordering checks reject duplicate/old sequences, old revisions, and
contradictory modes in a revision. A source/session transition clears caches
and pending source events. Capture timestamps belong to the command clock;
render freshness and phase extrapolation use local receipt times. Never
subtract the two boards' `millis()` values.

Pending dedicated kicks are retained through a cumulative counter. General
attacks retain count plus strongest pending strength and OR-ed band mask.
On render they map into bass (bands 0–3), mid (4–10), high (11–15). The same
frame can contain several groups. A dedicated kick and a general bass event
merge into one `bassKick` boolean; `dedicatedBassKick` remains available.
A count delta is not a request to render every missed historical event.

Gamepad snapshots include held state and latched presses between updates.
`gamepad()` includes one-frame taps; `heldGamepad()` excludes synthetic taps
and is used for Home duration. `shared/GamepadState.h` defines the portable
buttons/axes contract, avoiding Bluepad32 objects on the render board.

## Capture and spectrum

[`AudioAnalysis.h`](../../command-board/src/AudioAnalysis.h) defines 31,250 Hz,
a 512-sample FFT window, and 256-sample hops (50% overlap, 8.192 ms per hop).
A bin is about 61.035 Hz wide. Each channel has its DC offset removed. Stereo
volume combines channel powers so opposite-phase inputs do not cancel. One
intact channel—the higher-power channel—is selected for the single FFT; the
spectrum can omit content unique to the quieter channel. This is a deliberate
CPU tradeoff, not a full stereo-spectrum analysis.

The cached Hamming window and Parseval-style normalization produce approximate
band RMS from squared FFT magnitudes. DC slot 0 is excluded. The 16 half-open
slot ranges use edges:

```text
1, 2, 3, 4, 5, 7, 9, 12, 16, 22, 30, 42, 58, 82, 116, 164, 256
```

For example, band 0 is slot 1 near 61 Hz, band 1 slot 2 near 122 Hz. These are
FFT measurement ranges, not sharp acoustic filters or musical note detectors.
Bands run low-to-high in `fftResult[16]`.

Command defaults in `SignalAcquisition.cpp`:

| Control | Default | Meaning |
| --- | --- | --- |
| `uiMasterSensitivity` | 1.0 | Shared visual input scale |
| `uiInputGainADC` / `uiInputGainMic` | 1.0 / 4.0 | Source calibration |
| `uiFullScaleRMS` | 0.02 | RMS corresponding to full level at unity scale |
| ADC / mic raw RMS floor | 0.00002 / 0.00010 | Noise gate before artistic response |
| `uiSpectrumGamma` | 0.60 | Lifts weaker spectral display levels |
| Volume and band attack / release | 16 / 50 ms | Exponential level smoothing |
| Average time | 800 ms | Longer-term volume reference |

The dedicated kick detector runs on PCM in roughly 35–180 Hz, comparing fast
and reference power (20/350 ms). Default sensitivity 1.0 means about 1.7×
reference, minimum RMS 0.0003, minimum interval 180 ms. General attacks use
`AttackDetector.h` on spectral power, with their own fresh-rise logic and
80 ms minimum separation. Tempo is a third mechanism in `TempoTracker.h`;
its estimate is not itself an attack. Tune these independently.

## The renderer-facing API

[`AudioFrame.h`](../../render-board/src/AudioFrame.h) is the complete public
input for an audio effect:

- `volumeRaw`, `volumeSm`, `volumeAvg`: 0…255; smoothed volume is the usual
  motion/size control.
- `fftResult[16]`: the transmitted bands, before optional display compensation.
- `bpm`: a usable smoothed rate; quiet/disconnected input tends to 12 BPM,
  active sound without an estimate starts toward 60 BPM.
- `bassKick`, `dedicatedBassKick`, `midAttack`, `highAttack`, `hasAttack()`.
- `beatAngle(N)` and `beatPulse(N)`: continuous musical motion over N beats,
  with an arbitrary origin rather than a detected bar start.

`RemoteInputs::audio(sensitivity)` changes general-event admission
(`32 / sensitivity`), not FFT, volume, or time. Positive sensitivity admits
an already detected dedicated kick. Nonpositive sensitivity suppresses events.
The adapter's private clock integrates rate and gently corrects locked phase;
using `millis()*currentBPM` independently would teleport motion on tempo changes.
No effect should consume counters or reconstruct packet freshness itself.

## Render-side adjustments

[`Audio::controls()`](../../render-board/src/Audio.h) contains effect gains and
switches. Source calibration remains on command; artistic size/height response
belongs on render. For example, several effects reach full size near volumeSm
75, whereas Julia and Waverly have their own sensitivity curves.

`Audio::displaySpectrum()` optionally compensates display bands, default on.
Bands 0–3 remain unchanged; bands 4–15 subtract small floors then multiply by
1.25…5.5 with saturation. This is an empirical display curve for the tested
source, not a replacement FFT or a change to measured volume. Spectrum-driven
effects call it explicitly. Drift Rose's midrange fallback deliberately uses
its own input path. Raw and DisplayBins in diagnostics distinguish the two.

[`MidrangeBoost.h`](../../render-board/src/MidrangeBoost.h) is a separate
render-side rise detector for bands 2–9, suppressed when bass or volume is
already strong. Waverly, Julia Volume, and Drift Rose opt in with separate
history. The common enable defaults true; gain is `3.75/14.4` (about 0.2604).
It augments the effect drive, not the transmitted volume or global audio state.

Motion smoothing, spatial antialiasing, and framebuffer fades solve different
problems. A level envelope reduces geometry jitter; fractional voxel coverage
reduces spatial stepping; a short afterimage reduces abrupt visible replacement.
They may coexist, but excessive smoothing can hide the audio response.

## Implemented sources and placeholders

Mic and PCM1808 are implemented. The wire names AudioBluetooth/AudioDJ refer
to analog audio feeding that ADC. Current button selection maps unsupported
SD/DJ demands to the ADC path. `SourceAdapters` provides weak SD/depth hooks;
its default reads return false. The Depth-labelled physical button currently
toggles playback Hold. A depth packet renderer and protocol exist, but a real
camera/SD pipeline is not delivered by those stubs.

For effect inventory see [the catalog](../ANIMATIONS.md); for tuning history
see [the audio journal](../AUDIO_ANIMATIONS.md), whose older settings may be
superseded by this snapshot.

### Display compensation is not input calibration

`Audio::displaySpectrum()` copies bands and, when
`spectrumCompensationEnabled=true`, leaves bands 0–3 unchanged while subtracting
small calibrated floors and multiplying bands 4–15 (gains 1.25…5.5, clamped
to 255). This is a render-board display curve tuned to the reference source,
not a WLED-identical FFT implementation or an adjustment to transmitted RMS.
Spectrum effects opt into it. Drift Rose's midrange detector deliberately uses
its own raw-feature path. Debug output can show both `Bins` and `DisplayBins`.
Noise admission may still check raw bins so compensation cannot promote quiet
line noise into activity.

`MidrangeBoost.h` is another, separate mechanism: a rising midrange fallback
when bass/volume are not already doing the work. Waverly, Julia Volume, and
Drift Rose share the detector implementation with per-effect history; its
switch/gain live in Audio::Controls. It augments geometric drive, not speaker
volume or every effect's FFT spectrum. Gain is currently `3.75/14.4` (~0.2604).

Examples of artistic controls at this snapshot:

| Control | Value / use |
| --- | --- |
| `cornerBurstGain` | 255/75; smoothed volume 75 reaches attached-front cap |
| `peakAttackSensitivity` | 2.5; weaker general attacks can qualify for Puddle Peak |
| `surroundGeqGain` / `surroundGeqBassGain` | 3.0 / 2.5 |
| `particleGeqGain` / `particleGeqBrightness` | 3.0 / 2.5; launch response versus light before spatial blend |
| `frequencyShapesLevelForFullSize` / `frequencyPlanesLevelForFullArea` | 85 / 85 |
| `shapeVolumeForFullCube` / `bouncingBallsVolumeForFullHeight` | 75 / 75 |
| `juliaSensitivity`, attack/release | 1.5; 100/220 ms closure smoothing |
| `waverlySensitivity` | 1.5 |

Minimum visible rows/points are render-side idle presentations. They do not
prove the source is above its noise gate. Puddle Peak, kick-colored shapes,
post-crest Corner Burst, and clock-driven breathing respond to different
features; a single common event throttle would change their meaning.

## Implemented sources and placeholders

The wired buttons choose Animation, INMP441 Mic, PCM1808 ADC, and Game.
Repeating a working active audio source calls `repeatModeDemand()` without
tearing down I2S. The Depth-labelled button currently toggles playback Hold.
SD and DJ demands are mapped to the wired ADC path by the current acquisition
control path; there is no button cycling through hidden sources.
`SourceAdapters` supplies weak extension hooks for external selection, SD PCM,
and depth occupancy. Default SD/depth reads return false. Depth's renderer and
512-byte transport exist, but this is not a working camera pipeline without
an adapter implementation.
