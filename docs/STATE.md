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
