# Current checkpoint and open work

[Documentation index](README.md)

## Baseline

Source checkpoint: `f7840b540a18e69876e9b73ab6e67da266285dc8`, “Curate WLED
palettes, clarify color choices, and fix between-scene Next.” This documentation
was reviewed against that source on 2026-11-19. The source checkpoint was
already committed before this documentation pass. Documentation additions do
not change firmware behavior.

Current configuration is desktop 16³, brightness 32, UART receiver enabled,
expanded palettes enabled, color selection logs enabled, spacer scans disabled.
Ambient is descending and manually held. Audio is independently descending
with a 30-second duration. There are 74 ambient concepts: 67 enabled and seven
disabled, with additional weighting/subvariants. Audio has 16 enabled cards
and one disabled Attack Monitor. The catalog enumerates them.

## Accepted/current visual decisions

- The curated palette catalog has 49 entries; nine explicitly dark entries
  are reserved for permitted box/plane geometry. Selection is equal per
  eligible palette. Soap uses the expanded pool, Waving Cell keeps Rainbow
  or Party, and Snake's classic pool is preserved.
- The approved Waterfall is a replicated four-face presentation. The old
  diagonal standalone card is gone. Volcano is the wider 1.5× plume, a single
  central jet. Fire's tuned density/turbulence and warm coloring were accepted.
- Atomic Orbits uses shared ellipses/tumbling and full palette sampling
  brightness before spatial blending. Eye of Sauron is radial fire with a
  black pupil through the sphere; colored-pupil/material auditions were removed.
- Pinwheel Arms keeps progressive arm counts rather than a separate old Drift
  3D card. Sine Wave includes the rocking variant with one-third probability.
  Tartan and Passing Planes share a 50/50 card. PS Galaxy chooses two/three arms.
- Audio Shapes shares Cylinder/Octahedron, uses dedicated kicks for body-color
  changes and BPM for speed, plus eight smoothed volume cubes. Audio cone and
  hemisphere cards were removed.
- Freqwave shares max-brightness smooth wireframe/sphere emission; frequency
  cubes retain only the eight-band corner-growth treatment. GEQ chooses
  Surround/Inward 2:1; PS GEQ chooses its three layouts equally.

These are current choices, not a promise that every family-level overload has
identical defaults or uses the full palette catalog.

## Auditions and unfinished work

| Area | Current interpretation / next evaluation |
| --- | --- |
| Aurora | Enabled; must be viewed on the full-size cube before a keep/delete decision. Check suspended, wispy curtains, visible depth excursion, bottom clearance, and color/height motion. |
| PS GEQ | Enabled experimental layouts. Still tune flicker, lateral spew/gravity, overlapping colors, and source-dependent launch levels. The upstream 25×25 display and physical 16³ cube differ substantially in density/viewing geometry. |
| PS Attractor | Enabled tuned 3D nozzle/orbit presentation; further visual work remains. Deck turns AgeColor on; Move/Swallow remain off. Current exclusion radius default is 7 voxels and tangentAim is false; old radius/tangent auditions in chat are not current defaults. |
| PS Galaxy | Current persistent spiral/rocking presentation is approved enough to keep enabled. Earlier “doesn't work yet” checkpoint comments describe prior implementations. It remains an artistic adaptation rather than WLED-exact dynamics. |
| Midrange/vocal response | Shared midrange fallback exists, but general tuning across sources and future cue extraction remain open. Source-calibration logging/workflow should be improved before a broad retuning exercise. |
| Corner Burst release cue | Sustained-drop detection may be worth factoring out when a second consumer needs the post-crest event. It is not the general attack detector. |
| Full-size installation | Verify topology, UART pin conflicts, brightness/power configuration, and aesthetics on physical BurnCube. Host canvas checks do not commission the installation. |
| Next timing fix | Regression reproduces/fixes a dropped Next between app calls; source/build checks passed at checkpoint. Hardware confirmation after flashing remains outstanding in the recorded work. |
| Dependencies | Render dependencies are pinned; command framework uses a moving Bluepad32 main ZIP. Reproducible command-toolchain pinning remains future work. |
| Depth / SD / DJ integration | Wire modes/hooks exist; default SD/depth adapters return no data. Current Depth button is Hold; ordinary controls do not select SD/DJ. |
| PCM1808 treble roll-off (hardware; software-corrected) | 2026-11-19 tone test: a first-order ~235 Hz low-pass, about -6 dB/octave, -28 dB at 13 kHz relative to 400 Hz, identical on L/R. Unchanged with the Mac headphone jack in place of the Bluetooth receiver and with the Y splitter removed, so it is on the PCM1808 module side (likely a wrong-value input R/C part). Explains `Audio::displaySpectrum`'s large high-band gains and weak treble attacks. Now undone in software (`InputCorrection`, part of NEW processing; renderer skips its boost via the v4 `SPECTRUM_CORRECTED` flag). `calibrate.py` measured the corner at 203 Hz and saved profile `bt-pcm1808` on the board (flat within +/-0.3 dB to 6.4 kHz). Higher-quality modules are being purchased: when one is installed, rerun `calibrate.py --save` (it will fit a flat response and turn the correction off). |
| AGC: silence boost (fixed, confirmed on hardware) | New-system artifact seen on hardware 2026-11-19 (confirmed with the Game A/B switch): with music stopped, idle receiver hiss/hum sat just above the old -94 dBFS ADC gate, AGC climbed to +18 dB and effects animated the noise. Fix: AGC adapts only above `minAdaptRms` (~-68 dBFS) and the ADC gate is now 0.0002 (-74 dBFS). Replay confirms gain holds and display is 0 in silence; hardware showed zeros in the bins in silence after flashing. |
| AGC: breakdown lift | On hardware the 0.3 dB/s fall raised gain ~9 dB across Levels' 30 s breakdown, so the following build looked as bright as the first drop. Idea: fall fast only while kicks/bass continue (volume turned down), slowly when they stop (breakdown). |
| Audio cut-over plan | Data/protocol-level audio work (AGC, input correction, then attacks, tempo, new features) stays behind the Game A/B switch as one NEW system vs the OLD one. When NEW is accepted: remove the OLD path, the renderer's `displaySpectrum` treble boost and the A/B switch, then move to animation improvements. Also remove the renderer's `MidrangeBoost` (Julia, Waverly, Drift Rose): midrange/vocal response should come from a command-board data feature (step 4), not renderer-side lifting. Per-effect `...ForFull` / GEQ gains are then retuned by the automatic tuning loop against each routine's stated intent, not against the old look. |
| Tempo wakes in silence (likely fixed, verify) | Seen 2026-11-19 on GEQ with no music: render BPM jumps 12 → ~147 → back to 12 every few seconds and a beat-driven element flickers, while bins are all 0. Likely a single ~8 ms noise window crossing the tempo activity floor sets AUDIO_ACTIVE for ~1.2 s, so the render clock chases the last remembered tempo. Found the cause with calibrate.py: ~60 Hz rumble bursts up to -64 dBFS from the receiver chain. The gate now listens above 150 Hz, so silence keeps tempo inactive (tempo needs the gate open). Verify on GEQ in silence; step 3 may still require sustained sound for activity. |
| Calibration profiles | Per-hardware-configuration profiles on the command board (NVS), measured and saved by `tools/audio_replay/calibrate.py --profile NAME --save`. Saved: `bt-pcm1808` (2026-11-19). Not yet: `mic-inmp441` (uses compiled flat defaults), a DJ deck (not owned yet). When a configuration is final, copy its values into the compiled defaults. |
| Routine tuning (Part B) | `tools/render_replay` runs the real audio routines on the Mac against the reference songs (NEW vs OLD) and `run_all.sh` writes a per-routine behavior report in ~20 s, deterministically. Next: walk through each routine's intended look with the user, check the report against it, then tune. |
| Tempo pass 1 (NEW, flashed 2026-11-20) | 2026-11-20: FFT runs on the uncorrected input (correction applied per display band), restoring NEW BPM accuracy to OLD's (88/91/99% of active time on Levels/Sandstorm/Lay Low); a kick-phase arbiter keeps the beat on the kick (beat F 0.37/0.55/0.56 -> 0.59/0.59/0.57). Scored with `tools/audio_replay/reference.py` + `score_rhythm.py`. Verify DSP time on hardware (two trackers now run). See AUDIO_ROUTINE_REVIEW.md. |
| Unified hit detector (NEW, flashed 2026-11-20) | 2026-11-20: `HitDetector.h` replaces render-side event thresholds for NEW frames; level-independent log onset strength per group, fixed ~6 dB rise + 100 ms refractory; hit rate 84/132/104 per min on the three songs (OLD 77/191/104), treble hits now exist, breakdowns quiet. Puddle Peak darker between hits: audition. See AUDIO_ROUTINE_REVIEW.md. |
| NEW is the reference (2026-11-20) | The Game-button A/B switch is retired: NEW processing stays on. The USB serial command `ab old` / `ab new` still switches for debugging (purple volume bar = OLD). The OLD path is removed at cut-over. |
| Game button = Previous (temporary) | In an audio mode the Game button steps back one audio routine (Mic/BT pressed again steps forward), so adjacent cards can be toggled. Protocol v5 `PLAYBACK_BACK` flag marks the demand as "previous"; both boards must be flashed. Outside audio, Game still selects Game mode. Move to the rotary encoder later. |

## Disabled versus deleted

Preserved disabled ambient cards: Ovoid, Torus, Icons CCW, Text Israel, Noise
Clouds, Meteor 3D, Palette Audition. Attack Monitor is disabled in audio.
Enabling a card is a deliberate deck decision, separate from whether its
function exists.

Removed auditions include Octopus, Vortex, Rocktaves, PS Blobs/Ballpit cards,
extra Matrix glyph experiments, audio Attractor/Spray, audio Merkaba/Sine Wave,
and discarded GEQ/shell comparison versions. Consult history to recover a
removed implementation; do not casually re-enable an unrelated preserved
function because its name resembles an old audition.

## Verification record

The source checkpoint's recorded checks passed: render firmware build, host
suite, render regression checks, palette-enabled/disabled configurations, and
a targeted palette-policy harness. These are historical results of that source
change, not new tests run by writing this documentation. The development guide
lists repeatable checked-in commands and their scope. Physical visual
acceptance is recorded separately from compile/test success.
