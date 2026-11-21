# Audio routine review (in progress)

Per-routine intent in the owner's words, restated as measurable targets, then
checked against the render-replay report (`tools/render_replay/reports/`).
`docs/AUDIO_DESIGN_INTENT.md` is background only; this file records what was
agreed routine by routine.

## Principles (all routines)

- Pegged elements are unattractive. A column, cube or shape stuck at its
  maximum should be rare (target: any single element at max under ~10% of the
  music time), with headroom left for the loudest moments.
- The cube must never look dead. In silence every routine keeps a visible
  sign of life (a slow shape, single voxels, a dim glow), even if small.
  Elements can vanish entirely only when something else already shows life
  (e.g. Audio Shapes corner cubes can disappear because the center shape
  keeps moving).

## Tuning method (agreed)

Log every visual element of every routine (column heights, shape sizes, sphere
radius, color changes, speeds) while replaying reference songs, then check
each element against the stated intent: range used, time pegged, silence,
reaction to hits. The two current songs (Levels, Sandstorm) are both
four-on-the-floor dance tracks, so add audition songs with different
character: vocal-led with a strong midrange, a sparse or acoustic track, a
sub-bass-heavy hip-hop track, and something with wide dynamics (quiet to
loud). Owner to choose the songs.

## Tuning rule for volume-driven routines (owner)

1. Strip the render-side compensations (midrange boost, treble boost) and
   judge each routine on the NEW data as it is ("new native"), with the
   reference songs.
2. If a routine is too calm or too busy, adjust its own per-animation gain:
   one plain multiplier for how active it is relative to the incoming data.
3. No routine-specific compensation for bad data. Data problems get fixed on
   the command board (calibration, processing), never patched in an
   animation.

## Unified attack detection (owner direction, step 2)

One attack/event detector for everything, on the command board. Toss the
separate ad-hoc ones: the render-side midrange boost, per-routine detectors
(Corner Burst's volume-crest rule, Waverly's own push, render-side event
re-thresholding) and any separate midrange detection. Every routine reads
the same events with the same refractory behavior; routines may only choose
which events they use and how big they draw them.

Beat vs peaks (to think through): share the front end, not the decision.
- One onset-strength signal per band group feeds both the tempo tracker and
  the event detector, so beats and events always agree on what a hit is.
- The tempo tracker wants every regular onset; "interesting" events want the
  salient ones. So: an event is an onset that is strong for this song, or
  one the beat grid didn't predict (off-beat, a fill, an entry, a drop).
  Each event carries its band(s), strength, and whether it fell on the beat.
- Routines then pick: beat-locked motion uses the beat; "something happened"
  visuals use events; a routine can ask for on-beat accents only.

## Rhythm baseline (2026-11-20, before the unified detector)

Tools: `tools/audio_replay/reference.py` (Mac-only, librosa; never in
firmware) makes reference beats and onsets per song; `score_rhythm.py`
scores an audio_replay CSV against it (70 ms window). librosa locked to the
off-beat open hi-hats on Levels and Sandstorm (the 40-100 Hz kick rises half
a beat after its beats), so `reference.py` folds kick energy over the grid
and moves the beats by half when needed. Lay Low needed no shift.

| | Levels | Sandstorm | Lay Low |
|---|---|---|---|
| Tempo locked (during music) | 48% | 33% | 49% |
| BPM right while locked | 97% | 99% | 98% |
| Predicted beats on a real beat (locked) | 52% | 92% | 57% |
| Kick detector: per min / on a beat | 96 / 47% | 135 / 68% | 103 / 49% |
| Attacks: per min / on a reference onset | 54 / 42% | 140 / 91% | 28 / 63% |
| Attacks catching strong onsets | 32% | 43% | 11% |

What it says:
1. The BPM value is right; the problem is how rarely it locks.
2. Phase: in Levels and Lay Low the bass-kick detector (35-180 Hz) also
   fires on off-beat bass notes (about a third of kicks land on the
   off-beat), and the predicted beat follows them onto the off-beat about a
   third of the time. Sandstorm, with no off-beat bass, is 92% on the beat.
3. Attacks are not reproducible across songs: 28 to 140 per minute, and
   Lay Low misses 89% of its strong onsets. This is the "per-song strength"
   problem the unified detector is meant to fix.

### Tempo pass 1 (2026-11-20, NEW only; OLD unchanged)

Scored over all active time (what the renderer uses), not just "locked":

| | Levels | Sandstorm | Lay Low |
|---|---|---|---|
| BPM right, NEW before -> after | 76% -> 88% | 91% -> 91% | 82% -> 99% |
| Predicted beats on a real beat (F), OLD -> NEW | 0.37 -> 0.59 | 0.55 -> 0.59 | 0.56 -> 0.57 |

1. The FFT now always runs on the uncorrected input; NEW applies the treble
   correction to the display bands as a per-band gain. Running the FFT on
   the corrected signal changed the dominant channel and jittered bands
   +/-20-30% per frame, which cost NEW its BPM accuracy. Display effect:
   every routine's fill and pegging within ~1% (render report rerun).
2. Kick-phase arbiter (`TempoTracker::Settings::kickPhaseArbiter`, NEW
   tracker only): a 6 s phasor of 40-100 Hz power rises at the tracker's
   phase. It snaps the grid onto the kicks when they land >0.15 beat away
   (at most every 3 s) and pulls gently (0.5 s) otherwise; single kicks and
   the all-band onset fit may no longer jump the phase by more than 1/4 beat.
3. OLD and NEW trackers both run every hop; the Game A/B switch selects
   which one is published.

Still open: sections with off-beat sub-bass (Lay Low's drop) fool the kick
band; narrowing the kick band did not help (kick precision ~50% at every
band tried). Next ideas: require the kick's broadband click with the low
rise, and a real multi-hypothesis beat tracker. Bars/downbeats not started.

## Unified hit detector (built and flashed 2026-11-20, NEW only)

`command-board/src/HitDetector.h`. One decision for every routine; the
renderer no longer re-thresholds NEW hits (routine sensitivity only turns
hits on/off). Three groups (low bands 0-3, mid 4-10, high 11-15). Onset
strength = mean rise of log band power, so it is independent of level, AGC
and the treble roll-off: the same song gives the same hits from any
calibrated source. A hit = a local peak above the group's recent mean + 2x
deviation AND an average rise of at least 1.4 (natural log, ~6 dB), with a
100 ms refractory per group. Low-group hits are published as the kick in
NEW (Audio Shapes' color changes, bass hits); the tempo keeps the dedicated
kick detector.

Replay (3 songs, vs kick-aligned librosa reference):

| | Levels | Sandstorm | Lay Low |
|---|---|---|---|
| Hits per minute, OLD -> NEW | 77 -> 84 | 191 -> 132 | 104 -> 104 |
| Hits on a real onset, OLD -> NEW | 79% -> 78% | 96% -> 94% | 84% -> 78% |
| Strong onsets caught, OLD -> NEW | 28% -> 44% | 51% -> 37% | 34% -> 44% |
| Groups low/mid/high per min (NEW) | 60/13/19 | 87/37/29 | 42/10/56 |

- OLD had no treble hits at all; NEW has all three groups.
- Hits follow the music: Levels' breakdown 3-4 hits per 15 s vs 35-38 in
  the drops (OLD: 0-8 vs 20-52, but with a 2.5x spread in rate by song).
- Tried and rejected: a per-song adaptive percentile. It made rates equal
  across songs but re-adapted within seconds, so quiet intros and
  breakdowns got as many hits as drops.
- Render report: Puddle Peak reacts 3.4-4.1x per hit (was 1.9-2.8x) but is
  dark 34-41% of the music (was 12-23%); judge on the cube. Waverly reacts
  more to mid/treble hits. Audio Shapes changes color 16-21/min (was 21-26).
- Not yet: the "on the beat / off the beat" flag per hit (needs a protocol
  bit), and feeding the same onset strength to the tempo tracker.

## Fix list

### Fix now (small, agreed)
0. DONE: third audition song is Tiësto "Lay Low" (2026-11-20). A louder,
   steadier master: AGC about -4 dB, quietest 10% still volumeSm 49, 122 BPM,
   far fewer hits (70 vs 172 Levels, 504 Sandstorm). run_all.sh and
   report.py now use all three songs (WAV: laylow.wav).
1. DONE (2026-11-20, flashed): turn off the render-side midrange boost
   when NEW processing is on (Julia Volume, Waverly, Drift Rose Volume). OLD
   keeps it for the A/B. `midrangeBoostActive()` in Audio.cpp; test added.
2. DONE (flashed 2026-11-20): Audio Shapes corner cubes: retune the per-animation
   gain so they stop pegging at max; the range scaler (4b) only if the gain
   alone can't. See item 5 for the values.
3. DONE (flashed 2026-11-20): Audio Shapes center color changes at most once per
   four beats (first bass hit after the window), as a complementary pair (a
   hue and its opposite) stepping 80/256 around the wheel each change.
   Cylinder: body and caps are the pair; Octahedron: palette colors 1-2 are
   the pair, 3 an accent. Detail logs changes=. Taking the strongest hit in
   the window waits for the step-2 events.
4. DONE (flashed 2026-11-20): silence signs of life. Frequency Planes / Discs:
   a dim center dot on every band layer. Plasmoid: the same moving pattern,
   sparse (cutoff 40) and faint (~27% brightness). Corner Burst needed
   nothing: its corner voxel was already always lit (the report rounded
   1 voxel to 0%).
4b. Shared dynamic-range scaler (owner approved): one reusable render-side
   component that tracks a signal's recent soft and loud levels (fast to
   widen, slow to narrow, with a minimum range so quiet noise isn't blown
   up) and maps a value onto 0-1 within that range. Used by every routine
   with a size, height or color scale: Audio Shapes corner cubes, Bouncing
   Balls hops, Funky Plank colors, GEQ columns and sphere, Frequency Cubes,
   Frequency Planes / Discs, PS GEQ. Replaces the fixed "full scale"
   constants. Silence behavior stays as agreed per routine.
   CAUTION (owner): too much adaptation fights the automatic tuning of
   graphics. The command-board AGC already normalizes level, so a second
   fast-adapting stage would double-adapt and give the tuning loop a moving
   target. Design constraints: first try fixing pegging with fixed per-routine
   full-scale values tuned by the loop on the AGC'd signal; add the scaler
   only where that isn't enough, and then make it slow (tens of seconds),
   bounded (a few dB of adjustment at most), top-only (it only adds headroom
   when an element pegs persistently; it never boosts quiet passages), and
   logged so the replay and tuning loop see exactly what it did.
   Must be a flag (owner): a render control to switch it on or off (globally,
   and per routine if useful), off while the tuning loop measures fixed
   gains, so its effect can be A/B'd on its own.
5. Pegging audit across all GEQ-style routines (GEQ, Frequency Cubes, Funky
   Plank color buckets, Bouncing Balls hop height, Plasmoid fill, Drift Rose reach, Corner Burst fill,
   Frequency Planes / Discs, PS GEQ, Freqwave, Audio Shapes corner cubes):
   log each element's size in the replay, report time at max, and retune
   full-scale levels (or make them adaptive) where it exceeds the target.
   Includes the GEQ center sphere (visible, not buried by the bars).
   Prerequisite: per-element detail logging in every routine, and more
   audition songs. PARTLY DONE: report.py now has a "Pegged elements" row
   (Audio Shapes, Bouncing Balls, Corner Burst, Drift Rose, Frequency Cubes
   and Planes per band, GEQ per column, Funky Plank top color, Waverly
   closing) and "Color changes per minute"; Frequency Cubes logs all 8 band
   levels. Still missing: Julia (detail line is commented out), PS GEQ,
   GEQ sphere vs bars, Freqwave.
   GAINS TUNED (flashed 2026-11-20), by sweeping each routine's own control over all
   three songs on NEW (render_replay --set) and taking the value where the
   element pegs under ~10% on every song while still using its range:
   - shapeVolumeForFullCube 75 -> 100 (corner cubes at max 25-34% -> ~3-5%)
   - bouncingBallsVolumeForFullHeight 75 -> 100 (ceiling hops 33-61% -> ~5-10%)
   - cornerBurstGain 3.4 -> 2.7 (fill at cap 24-33% -> 4-6%)
   - driftRoseVolumeForFullReach 75 -> 100 (full reach 24-33% -> ~2-4%)
   - plasmoidIntensity 128 -> 85 (nearly full 36-44% -> ~1%; typical ~half)
   - frequencyPlanesLevelForFullArea and frequencyShapesLevelForFullSize
     85 -> 110 (worst band full 18-34% -> ~5-10%)
   - juliaSensitivity 1.5 -> 2.5 (typical fill 55% -> 65%, light vs volume
     +0.57 -> +0.76 in Levels); then 2.25 after the owner saw it on the
     cube (-10%), then 2.025 (another -10%, 2026-11-20)
   GEQ left at 3.0 (columns full 1-3% avg). These also apply to OLD, which
   now looks smaller; OLD is reference only. Sweep table:
   scratchpad sweep_out.md (summarized here).
   VERIFIED in the full 3-song report: on NEW every tuned element pegs
   0-6% (worst single band 5-9%, worst GEQ column 12-14%); Plasmoid typical
   fill 48-51%, nearly full 0-1%; Julia typical 65%. Lay Low on OLD (no AGC)
   pegs heavily (bouncing hops 44%, plank top color 39%, Plasmoid full 23%),
   which is the loud-master case the AGC exists for.
6. DONE: palette now runs dark to bright (blank, indigo, blue, violet,
   red, orange, amber, warm white; magenta replaced by classic red after the
   owner saw too much pink on the cube, 2026-11-20) so louder = brighter; NEW
   thresholds {22,31,38,44,51,61,78} from the reference songs' level spread
   (provisional until the third song); OLD keeps {7,12,...,52}.
   Original item (owner: decorative, "hard to take too seriously"; fix as I
   see fit to maximize intuitiveness): rebalance the color thresholds so all colors appear and the top
   color is rare (adaptive to the song), and reorder or swap the palette so
   stronger blocks are brighter, not dimmer. Goal is pretty and balanced,
   not a precise audio readout. My view: mostly agree, but with brighter =
   stronger, kicks show as bright stripes scrolling down (about two beats
   visible in the 16 rows), so rhythm becomes readable at a glance.
7. Corner Burst intuitiveness trial: launch size follows peak size, fill
   discharges on launch, tempo-timed crossing, short pause between launches.
8. Breathe: one slow breath per bar with a small pulse that lands on each
   kick; breath depth from loudness via its own gain; color from the
   three-band blend or sound brightness instead of a free-running rainbow.
   PARTLY DONE (flashed 2026-11-20): with music it breathes once per four beats
   (70%) plus a sharp accent on each beat (30%); idle keeps the old slow
   breath. Color change not done yet. CORRECTION: the breath was never
   inverted; `beatPulse` is 0 on the beat, so the old `1 - beatPulse` was
   already brightest on the beat. The report metric was the inverted one
   and is fixed ("Brightest on the beat").
9. Julia Volume responsiveness: re-map closure to where the visible volume
   changes most, remove or shorten the extra geometry smoothing, then set its
   gain against the reference songs (target: Waverly-like response).
   PARTLY DONE (flashed 2026-11-20): measured lit volume vs loudness with stepped
   synthetic input: 41% (silent) to 79% (closure 1), curving upward, so the
   low half barely changes. Default `juliaVolumeGamma` 1.0 -> 0.6 straightens
   it. Replay: Levels NEW fill 46-52% -> 51-58%. Still narrow because typical
   NEW volume (volumeSm 33-87 in Levels) only reaches closure ~0.5 at gain
   1.5. Next: gain ~2.5-3 (closure 1 near volumeSm 90), set with the third
   song. Extra smoothing left as is (unseen flicker risk).
10. Measure the silence slow-down (replay with silence after music; then on
   hardware) and fix whatever keeps the speed up.
   REPLAY DONE: Levels + 20 s silence through Audio Shapes: BPM 102 -> 45
   (0.5 s) -> 16 (1.5 s) -> 12 idle by ~3 s; shape speed 0.89x -> 0.10x and
   stays there. The replay noise model has no rumble bursts, so still check
   on the cube with the high-pass gate.

### Work afterward (needs the audio feature steps)
- Breathe: a strong "interesting event" triggers a deeper breath.
- Attacks (step 2): keep a refractory period (owner: it's what keeps
  Puddle Peak from getting busy); use Puddle Peak as the first test bed.
- Attacks (step 2): "interesting event" detection across bass, mid and
  treble, with fewer false positives. Feeds Audio Shapes color and others.
- Bar position (step 3): today a "bar" is just four beats from an arbitrary
  starting beat; the clock has no downbeat. Audio Shapes' once-a-bar color
  and Breathe's bar breath are therefore four-beat windows that may start on
  beat 2, 3 or 4. Needs downbeat detection.
- Tempo (step 3): steadier BPM, beat and bar position, so color changes land
  on the bar and speed doesn't jump on bad readings. Revisit the 60 BPM floor
  so slow music can slow shapes down.
- New features (step 4): vocal/midrange presence to replace the midrange
  boost; vocal entries and modulation.
- AGC breakdown lift: breakdowns should still read as smaller (Frequency
  Cubes loud/soft contrast dropped from 3.0x to 2.3x in Levels).
- Replay the Audio Shapes Cylinder variant.
- Cut-over: remove the OLD path, the displaySpectrum boost, the A/B switch and
  the midrange boost code.
- Color experiments on the GEQ-style routines: three-band blend
  (blue/amber/white), ember, rainbow plus lightness.
- Future idea: a better frequency geometry than the eight cubes clustered at
  the center.
- PS GEQ: liquid look (continuous emission, gentle turbulence, slightly
  wider spread; small lateral motion only, no floor splash); tune in the
  replay first.
- PS GEQ Nova: rebuild as eight 45-degree hoses from the center.

## Audition pairs (built and flashed 2026-11-20)

Nine temporary cards play first after a flash (descending order), A then B,
so one Next press compares them. A is the accepted look; B is the trial. Both
cards of a pair share the shape/pattern (A picks, B reuses). Remove the cards
when a pair is decided. Code: `TestDemoAudio.cpp` (cards), `Audio.cpp`,
`ParticleEffects.h`.

1. Corner Burst A / B (fix list item 7). B: launch strength = swell vs the
   recent volume range (`cornerBurstTrialSwell` 0.5 to launch,
   `cornerBurstTrialFullSwell` 1.0 for full strength, range memory
   `cornerBurstTrialRangeRelax` 4 levels/s); bright/thick/fast for big
   swells; fill discharges on launch and regrows over a beat; the front
   crosses in ~1 beat; half-beat pause. Replay sweep (3 songs): launches
   46-51/min (A 65-73), strength p10/50/90 ~0.55/0.8/1.0 (A always 1.0),
   fill follows volume +0.64-0.70 (A +0.9; lower because of the discharge).
   Still uses the volume-crest peak; the unified detector replaces it later.
2. Breathe A / B (item 8, color). B: color from a three-band blend (deep blue
   lows, amber mids, white highs, by squared share of display level, 300 ms
   smoothing) instead of the rainbow. Breath timing and depth unchanged.
3. Frequency Planes A / B (color experiments). B: rainbow plus lightness
   (hue by band, saturation falls with pitch, full brightness). Geometry and
   metrics identical to A, as intended.
4. PS GEQ A / B (liquid). B: afterimage 150 (streams join between launches),
   speed jitter 25% (was 15%), lateral drift x0.6 plus a half-voxel sway,
   rising particles 0.75 radius. No splash or deflection. Replay: fill
   37-40% -> 46-51%, same height/volume tracking.
5. PS GEQ Nova (new). Eight hoses at the center, 45 degrees apart, band
   pairs, spraying outward and slightly up with mild gravity; long trails;
   dim nozzle ring in silence. Replay: ~11% lit, follows volume +0.24-0.74,
   particle pool never starved (priority rotates).

## 1. Audio Shapes

**Owner's words (summary):** a shape in the center tumbles faster or slower
with the BPM. Its color should change on an "interesting event": a noticeable
attack in bass, mid or treble. Only bass-driven changes ever worked well, with
many false positives. The eight corner cubes grow from the center with
smoothed volume, to show loudness and fill the space. They peg at maximum too
often; on songs that are loud throughout they should adapt.

**Targets:**
- Tumble speed tracks tempo, not loudness. Steady while the tempo is locked;
  a sensible default speed while it isn't, with no jumps from bad BPM readings.
- Color changes about once per bar (every 4 beats: ~30/min at 120 BPM), on a
  notable hit from any band, fewer in breakdowns, none in silence. Every beat
  is too much.
- New colors are deliberate complementary contrasts (pleasing, intuitive),
  not random picks. Today the Cylinder picks random rainbow colors and the
  Octahedron a random contrast palette.
- Corner cubes span their full range within each song: near minimum in
  breakdowns, near maximum only at the loudest moments (at maximum less than
  about 10% of the music time), with visible movement in sustained loud passages.
- Silence (owner): small corner cubes AND a strongly slowed center shape.
  The exaggerated slow-down multipliers for silent or slow music are
  intentional. The lit center shape in silence is fine. The slow-down relies
  on BPM falling to its idle value (12 BPM gives 0.1x), so BPM wandering in
  silence breaks it; check this once the tempo wake fix is verified.
  Corner cubes may shrink to nothing in silence (current code does this);
  the slowed center shape is the sign of life.

**Replay check (Octahedron, before changes):**
- Color changes once per kick: 93/min (Levels), 129/min (Sandstorm). Too many,
  and only bass drives them.
- Tumble follows BPM (correlation 0.95-0.99, 0.7-1.9x speed) and not loudness.
  Correct, but tempo errors show directly.
- Corner cubes at maximum 33-38% of the time on NEW (10-18% on OLD). The
  full-volume constant was tuned on the old, quieter data.
- Silence: 29% of the cube lit (the center shape; accepted).
- Cylinder variant not replayed yet.

**On the cube (2026-11-20, owner):** color changes every four beats and the
corner cubes look fine with the fixed gain (no adaptive range needed). Still
changes when nothing interesting is happening: the window admits the first
bass kick after four beats, and steady kicks always qualify. Fix with the
step-2 "interesting event" detector, not a routine-side threshold.

**Depends on:** attack work (step 2) for multi-band "interesting events";
tempo work (step 3) for steadier BPM.

## 2. Frequency Cubes 8 Bands Corner

**Owner's words (summary):** the 16 bands are folded into 8, and each corner
cube's size shows how strong its frequency range is. The new processing
already looks visibly more accurate. If a more intuitive geometry for showing
frequencies comes up later, show it.

**Targets:**
- Each cube's size follows its own band's strength. A tone or instrument in
  one range moves mainly its own cube.
- Treble cubes are as alive as bass cubes for equally present content (no
  built-in tilt toward the low end).
- Each cube uses its full range across a song, rarely pinned at max or min.
- Silence: all cubes at minimum size, still. In code the minimum is one
  voxel per cube, so silence shows 8 colored voxels clustered at the center
  (the replay's "0% fill" is 8/4096 rounded down). Owner expected this.

**Replay check:**
- NEW fills more (35-38% typical vs 22-26%) and moves more. Mid and treble
  now drive it as much as bass (Levels: bass/mid/treble 0.49/0.80/0.75 vs
  0.72 for each on OLD, where everything moved together).
- Loud-vs-soft contrast is lower on NEW (2.3x vs 3.0x Levels; 3.6x vs 5.5x
  Sandstorm) because the automatic gain evens out levels. Watch that
  breakdowns still read as smaller.
- Silence: dark and still. Good.
- Not yet measured: per-cube sizes (how often each cube is pinned). The
  replay log needs a per-cube detail field to check this.

**Future idea:** a better frequency geometry than the eight cubes clustered
at the center (owner clarified: the center arrangement, not the corners).

## 3. PS GEQ

**Owner's words (summary):** an arrangement of nozzles shooting particles up
like fire hoses: they blast up, "spew" with some lateral motion, and fall back
down like water. Modeled on WLED's PS GEQ 2D (reference: a 25x25 animated
GIF, screen recording 2026-11-20). It never quite got the liquid, spewing feel
of the GIF, though it came out well. Improving the resemblance is worth doing.

**What the reference shows (frames pulled from the recording):**
- Each band is a dense, solid-looking column from the floor to a height set by
  its level, not a set of separate dots.
- Column edges are ragged and churn: particles jostle sideways, the column
  widens where falling spray meets rising spray, and it pools and spreads at
  the floor before draining.
- Heights jump with the music; a column can collapse into a heap and refill.
- Color is per band with brightness flicker inside the column.

**How ours differs (code reading):** one launched particle per band per batch,
copied deterministically to each nozzle; falling spray drifts with a constant
sideways speed; no particle interaction or floor pooling. That gives clean
arcs rather than a churning liquid.

**Targets:**
- Columns read as solid liquid streams whose height follows each band's level.
- Falling water visibly spills around the column and splashes/pools at the
  floor before fading.
- Loud vs soft parts clearly different in height (replay contrast is only
  1.3x in Levels NEW; 1.8x in Sandstorm).
- Silence: the lit floor (one voxel layer) is the intended sign of life.
  Accepted as is.

**Possible improvements to try (replay first, then cube):**
1. Emit every frame with speed jitter so each column is continuous.
2. Turbulent sideways motion (noise-based) instead of constant drift.
3. Deflect falling particles outward where they meet the rising stream, so the
   water cascades around the column. OWNER OBJECTS: something like this was
   tried and looked like a mess. With only 16 columns, too much sideways
   motion makes the streams interfere. Keep any lateral motion small.
4. ~~Floor splash / puddle.~~ Rejected by owner.
5. Idea to keep: a somewhat wider spread so each column reads as a stream
   rather than a thin line (within the lateral limit above).

**Replay check:**
- Fill 37-40% NEW (27-32% OLD); never dark during music.
- Height follows volume (+0.60 Levels, +0.76 Sandstorm).
- Weak loud/soft contrast (1.3x Levels NEW).
- No visible reaction to hits (about 1.0-1.1x), which is expected for a
  level-driven fountain.
- Silence: the colored floor stays lit (6% = the 256 floor voxels), still.
  Matches intent.

## 4. Freqwave

**Owner's words (summary):** a shape (hollow sphere or wireframe box) is
emitted from the center on every beat, colored by the dominant frequency, and
disappears as it touches the cube's faces. Every beat seems necessary; unsure
whether per bar would be better. Asked whether there's a more intuitive
frequency-to-color mapping, and whether palettes could supply the mapping.

**Code today:** one shell per beat of the shared beat clock; travel speed is
fixed (about 1 s to reach the faces), so the number of shells in flight
depends on tempo (about 2 at 128 BPM). Color = hue of the loudest of the 16
bands, rainbow from red (band 0) to violet (band 15). In quiet (volume <= 2)
shells are black, so true silence goes dark.

**Replay check:**
- About 85% of shells come from three bands: band 0 (red, bass), bands 6-7
  (green, 550-1000 Hz). Sandstorm: bands 0-1 (red/orange) 75%. So the color
  mostly flips between red and green and says little about the music.
- Fill 23%, dark 1-5% of the music time, light barely follows volume in
  Levels (+0.05). Fine for a beat-paced routine.
- Silence: 1-4% fill, almost no motion, fading to dark. Breaks "never dead".

**Proposals (to agree):**
- Keep one shell per beat. Optionally accent the bar's first beat (brighter or
  thicker shell) once the tempo work gives bar position.
- Tie travel time to the beat (e.g. reach the faces in two beats) so the same
  number of nested shells shows at any tempo.
- Color from the spectral centroid (the sound's "brightness": where the
  energy's balance point sits) instead of the loudest band, stretched to the
  song's recent range so the whole palette gets used.
- Color mapping options: rainbow (low = red, high = violet; the common
  convention), or a lightness-based palette (low = deep and dark, high =
  pale/bright), which matches the most consistent finding on how people pair
  pitch with color. Any curated palette can be the mapping; make it
  selectable.
- Silence: a faint, slow shell at the idle tempo so the cube stays alive.

**Color convention options (discussed, not chosen):**
- On LEDs, "dark" means dim, which already means quiet in other routines. So
  carry lightness through whiteness instead: low sounds deep and saturated,
  high sounds pale and pastel, at full brightness.
- Lightness palettes: ember (deep red, orange, yellow, white-hot) and ice
  (indigo, blue, cyan, white).
- Rainbow plus lightness: keep the rainbow hue and add white as pitch rises.
- DJ-software three-band blend: mix three anchor colors by the share of energy
  in lows, mids and highs, so the color is a meaningful blend (Serato style:
  red, green, blue; Rekordbox three-band style: deep blue, amber, white, which
  is also a lightness scale). Familiar to DJs, and relevant once the DJ deck
  arrives.
- Owner: try these on every GEQ-style routine where they look good
  (Frequency Cubes, Frequency Planes / Discs, PS GEQ, GEQ, Freqwave), not
  just Freqwave.
- Not feasible now: note- or key-based colors (Scriabin-style note colors,
  key color wheels) need finer frequency detail or key detection.

## 5. Frequency Planes / Discs

**Owner's words (summary):** two shapes, filled squares or circles (chosen at
random), stacked as 8 frequency bands mirrored about the middle, low to high,
in the rainbow palette. Each one's size follows its band's strength. Worked
well even with the weak treble, and better now. Status: happy with it.

**Replay check:**
- NEW fills more (50-53% typical vs 35-39%), never dark during music, never
  full. Light follows volume (+0.73 Levels, +0.89 Sandstorm).
- Mid and treble now drive it as much as bass (Levels 0.55/0.79/0.79).
- Loud/soft contrast lower on NEW (1.4x vs 1.8x Levels), the same AGC
  breakdown effect as Frequency Cubes.

- Silence: completely dark (0 voxels lit). Breaks "never dead"; needs a
  minimum sign of life (e.g. one voxel or a faint center dot per band, as in
  Frequency Cubes).

**Changes:** silence sign of life only. Candidate for the color/lightness
experiments.

**On the cube (2026-11-20, owner):** still looks good after the gain change
(110); non-bass planes are easily identifiable. (It never used the midrange
boost; the separation comes from the command-board treble correction
replacing the render-side treble boost.)

## 6. GEQ

**Owner's words (summary):** columns mirrored around all four faces, each
column's height following its band's strength. Above each column, after a
blank spacer, a peak marker that falls more slowly, as if the column pushes it
up (classic graphic-equalizer peak hold). Status: works.

**Replay check:**
- Fill 38-47% NEW (27-41% OLD); never dark or full during music.
- Height follows volume well (+0.89 Levels, +0.91 Sandstorm).
- In Levels it is driven mostly by bass (bass/mid/treble 0.83/0.24/0.40).
- Loud/soft contrast lower on NEW (1.8x vs 2.8x Levels), the AGC breakdown
  effect again.
- Silence: 3% lit and still (the base/markers). Meets "never dead".
- Not yet measured: how often individual columns sit at full height.

**Code detail (owner forgot the sphere):** GEQ picks one of two variants.
Surround GEQ (2 of 3 picks): 8 column groups mirrored on the four faces from
bands 4-15, plus a center sphere whose radius follows the lowest four bands
(bass, up to ~300 Hz). Inward GEQ (1 of 3): 16 groups, bass stays in the
columns, no sphere. In both, loudness lifts all bars by up to 3/16 of the
height before the spectrum fills the rest.
- Owner concern: the sphere may be hidden behind tall bars now that NEW makes
  the bars taller. Check in the per-element log (sphere radius vs bar
  heights) and on the cube.
- The replay doesn't log which variant ran and has no GEQ detail line; add
  one (variant, sphere radius, per-group heights).

**Changes:** none specific; covered by the pegging audit and the sphere check.

**On the cube (2026-11-20, owner):** bars too strong; center sphere too
occluded. Done (flashed 2026-11-20): `surroundGeqGain` 3.0 -> 2.1 (owner's
0.7x; replay: columns at full height 1-3% -> 0%, typical fill 38-47% ->
31-40%), and surround walls one voxel deep instead of two so the sphere is
visible from every side (sphere size limit unchanged).

## 7. Funky Plank

**Owner's words (summary):** 16 bands folded into 8, mirrored on all four
faces. Each moment's band strength becomes a discrete color block, and the
rows scroll from top to bottom (a well-known GEQ effect). Status: works.

**Code:** a new row every 64 ms; strength picks one of 8 buckets (blank, red,
orange, yellow, green, blue, indigo, violet) using fixed thresholds
{7, 12, 20, 28, 36, 44, 52}, tuned on the old ADC log.

**Replay check (share of blocks per color, Levels NEW vs OLD):**
- NEW: blank 2%, R 1%, O 5%, Y 10%, G 21%, B 19%, I 14%, V 29%.
- OLD: blank 15%, R 6%, O 11%, Y 14%, G 17%, B 12%, I 9%, V 16%.
- Sandstorm is the same story (NEW violet 29%, blank 3%).
- So on NEW the top color (violet) is pegged 29% of the time and the low
  colors almost vanish: the fixed thresholds no longer fit the hotter,
  treble-corrected signal.
- Loud passages get darker, not brighter (light vs volume -0.31 on NEW):
  indigo and violet are the dimmest colors on LEDs, and they mean "strongest".
- Silence: 1-2% lit (a few blocks). Meets "never dead".

**Owner:** pretty to look at; nobody really reads audio from it, so don't
take it too seriously. Proposals below are approved for me to apply as I see
fit (fix list item 6).

**Proposals:**
- Retune the thresholds so the colors are used evenly and the top color is
  rare (part of the pegging audit), or make them adapt to the song.
- Consider a palette where stronger means brighter (the lightness idea:
  e.g. deep blue to white), or at least avoid ending on the dimmest colors.

## 8. Bouncing Balls Audio

**Owner's words (summary):** 16 dimly lit "ball piles" on the floor. On the
beat, pairs of piles launch their balls into the air as if hit by a hammer
from below, harder for louder music. Deliberate irregularity (each ball's
height varies a little) to feel natural.

**Code:** two patterns (Clockwise, Orbit Inward) chosen at random; each beat
launches the next pair; hop height follows volume; each ball gets 85-100% of
the hop; landings rebound and settle; a brief white flash on launch.

**Replay check:**
- Hops hitting the ceiling (height 15 = full cube): Levels 33% NEW vs 13%
  OLD; Sandstorm 47% NEW vs 26% OLD. Typical hop: Levels 13.1 NEW vs 6.2
  OLD. On NEW most hops go nearly to the top, so loudness hardly shows.
- Silence: floor piles lit (6%), still. Meets "never dead".

**Proposal:** scale hop height against the song's recent loudness range so
hops spread from low to near-top and only the loudest hits reach the ceiling
(target: under ~10% of hops). Keep the 85-100% per-ball irregularity. Part of
the pegging audit.

## 9. Plasmoid Depth Mirror

**Owner's words (summary):** lights the whole volume of the cube from dim to
bright with a moving, fairy-like pattern (similar to the blends routine but
more regular). Volume sets what share of the cube lights and how brightly.
No volume-to-color link. Very effective; status: works.

**Code:** a WLED Plasmoid port. Two moving waves give every voxel a pattern
score; smoothed volume sets a cutoff, and voxels whose score is below it
light up, so louder music lights more voxels. Brightness comes from the
score with a floor; color is a rainbow by position (not volume). Mirrored
front-to-back in depth. Below volume 2 nothing lights.

**Replay check:**
- NEW fills far more: typical 82% vs 22% (Levels), and is nearly full (>85%)
  36-41% of the music time vs 15-25% on OLD. Loud/soft contrast collapsed
  from 23.6x to 3.2x in Levels. On NEW it sits near "everything lit" most
  of the time: pegged.
- Still follows volume tightly (light +0.93).
- Silence: completely dark. Breaks "never dead".

**Proposals:**
- Retune the volume-to-cutoff scale (the `plasmoidIntensity` control) so
  typical music lights around half the cube and full is reserved for peaks
  (pegging audit).
- Silence: keep a faint sparse pattern drifting so the cube stays alive.

**Owner:** approved. Pegging fix definitely. For silence, show the same
moving pattern, just fainter; owner expects that won't be irritating.
Confirm on the cube.

## 10. Drift Rose Volume

**Owner's words (summary):** 3D rose petals slowly drifting in and out from
the center; pretty on its own. Smoothed volume throws the petals out further.
Owner believes there's no frequency aspect. Very convincing and very pretty;
status: works.

**Code:** reach = 40% (quiet) up to 100%, driven by smoothed volume over a
fixed full-scale (`driftRoseVolumeForFullReach` = 75), PLUS the render-side
midrange boost at double strength. So there is a hidden frequency aspect:
midrange content pushes the petals out too. It goes away with fix item 1.

**Replay check (petal reach):**
- Levels NEW: typical 91%, at full reach 24% of the time (OLD: 66%, 9%).
- Sandstorm NEW: typical 90%, full 28% (OLD: 75%, 14%).
- On NEW the rose sits near full spread most of the time: pegged, so the
  in-and-out breathing with loudness shrinks.
- Midrange boost active 14% of frames on NEW (48% on OLD).
- Silence: 5% lit with slow drift, meets "never dead".

**Proposal:** retune the full-scale so typical music sits mid-reach and full
spread is for peaks (pegging audit). Remove the midrange boost (fix item 1)
and re-check, since that changes the drive.

## 11. Corner Burst

**Owner's words (summary):** pick a lower corner. Overall volume fills the
cube diagonally from it, dimly, behind a wavefront. On a peak, the wave
launches and travels across the rest of the cube, bright as it leaves.
Several wavefront shapes. Peak detection is decent for some ranges; the
volume-to-fill is very intuitive. Status: works.

**Code:** three shapes (diagonal plane, sphere, box) chosen at random. Fill
depth = smoothed volume x `cornerBurstGain` (default 255/75, so volume 75
fills to the cap). Peak = volume rises at least 4 above its trough, then
falls 12% below the crest for 40 ms; the front then travels for 750 ms.

**Replay check:**
- Fill at its cap: Levels 24% of the time NEW vs 9% OLD; Sandstorm 28% vs
  14%. Typical fill 18.8 of 22 NEW vs 6.5 OLD (Levels). Pegged on NEW, so
  the volume-to-fill link (the intuitive part) loses range.
- A wave is traveling in about half the samples, roughly 40 launches a
  minute (estimate from 0.5 s detail sampling); about the same NEW and OLD.
- Silence: completely dark. Breaks "never dead".

**Proposals:**
- Retune `cornerBurstGain` (its per-animation gain) so typical music fills
  about half way and the cap is for peaks (pegging audit).
- Silence: a faint fill sliver at the corner.
- Peak launches: later, consider the attack/"interesting event" detector
  from step 2 instead of the volume-crest rule; measure launch rate per
  section once the replay logs every launch.

**Owner:** approved to try my ideas for making it more intuitive:
- Scale each launch by the size of the peak: a big swell sends a bright,
  thick, fast front; a small one a faint, thin one. Peak size is read
  against the song's recent range, so only real swells launch (no wobble).
- Discharge: when the front launches, the dim fill drops back toward the
  corner and regrows, like a spring releasing, so cause and effect read
  clearly.
- Time the crossing to the tempo (cross the cube in about one beat) so
  waves feel in rhythm.
- A short pause after each launch so fronts don't stack into noise.
Judge in the replay (launch rate, launch size vs peak size), then on the cube.

## 12. Breathe

**Owner's words (summary):** a breathing animation with a sine driver that
cycles colors and moves with the BPM. "Not great." Beat alone may be too
little information for an audio animation, so the intuition is weak (the
same reason frequency cubes were added to Audio Shapes).

**Code:** four patterns (Sphere, Checkerboard, Diagonal, Helix) chosen at
random. Brightness breath = 1 - beat pulse, one breath per beat (the
Checkerboard has a beats-per-breath setting). A rainbow travels through the
pattern with the beat. Volume only scales how deep the breath is.

**Replay check:**
- (CORRECTED) The -0.97 against "beat pulse" meant brightest ON the beat:
  the report's beat-pulse signal is 0 on the beat. Not inverted. Ignore the
  next line. That alone
  can make it feel disconnected.
- One breath per beat at 120-140 BPM is fast for "breathing".
- Barely follows volume (+0.15 Levels) or any band.
- Silence: 78% lit with slow motion. Alive.

**Proposals (owner approved; fix list item 8):**
- Breathe on the bar: one slow breath per 4 beats, with a small pulse on
  each beat that lands on the kick (bright on the beat, not dim).
- Add information: breath depth from loudness (tuned so quiet passages
  breathe shallowly and drops breathe deep); color from the three-band
  blend or brightness of the sound instead of a free-running rainbow.
- Optionally let a strong "interesting event" (step 2) trigger a deeper
  breath.

## 13-14. Puddle Peak and Puddles

**Owner's words (summary):** the same idea in two flavors. A line or small
cube appears at a random spot in a fading, cycling rainbow color. Puddles
draws them at a steady interval; Puddle Peak draws them only when a
thresholded event is detected. Volume or peak strength sets the length or
size. "Cobbled together but it actually works." Puddle Peak's refractory
period (a pause after each event) is important and effective: it keeps the
routine from getting too busy. If peak detection improves and generalizes to
"anything interesting", Puddle Peak should get better.

**Code:** three shapes (horizontal line, vertical line, cube) chosen at
random. 250 ms fade, one hue step per spawn. Puddles spawns every 34 ms
while volume is above a small floor. Puddle Peak spawns a burst (about six
shapes' worth) on any shared bass, mid or treble attack; the refractory
comes from the command-board detectors themselves.

**Replay check:**
- Puddle Peak: dark 25% of the music time on NEW (44% OLD) in Levels; 14%
  NEW in Sandstorm. Clear visible reaction to hits (motion 2-3x after bass,
  mid and treble events). Events on NEW Levels: ~103 bass, 24 mid, 4 treble
  per minute.
- Puddles: almost never dark during music, small footprint (1%).
- Silence: both completely dark.

**Proposals:**
- Puddle Peak is the cleanest test bed for the step-2 "interesting event"
  detector: switch it to the new events when they exist and judge by eye.
  Keep the refractory behavior as a requirement of the new detector.
- Silence: for Puddles, an occasional tiny, dim puddle so the cube isn't
  dead. Puddle Peak going dark in silence is fine (owner).

## 15. Waverly

**Owner's words (summary):** a cavern: an irregular noise surface mirrored
top and bottom, with a moving, color-cycling texture through it. Responds
to smoothed volume. Slamming closed should be very rare. Status: works,
"wonderful".

**Code:** surface height = noise x (smoothed volume x gain x 90% of the
height + extra reach). The extra reach is the render-side midrange boost when
it is on; otherwise a brief push from shared mid/treble attacks. The top
surface is the bottom one mirrored, so the cavern closes only where both
reach the middle.

**Replay check (tallest point, of 16):**
- Levels NEW typical 4.3, 90th percentile 5.9, max 7.9; at or past the
  middle (8) 0% of the time. Sandstorm NEW 1% (max 10.8). OLD similar.
  Closing is very rare, as intended.
- Light follows volume well (+0.88/+0.90); bass, mid and treble all drive it
  on NEW.
- Silence: 12% lit with slow motion. Alive.

**Changes:** none, except that removing the midrange boost (fix item 1)
switches its extra reach to the shared attack push, and later to the unified
events. Re-check that closing stays rare after that. Owner: after removing the
boost, dial its own gain (`waverlySensitivity`) to restore the liveliness,
keeping closing rare.
CHECKED 2026-11-20 (replay, boost off on NEW): unchanged. Levels fill
50% (31-63%) vs 49% (33-59%) with the boost; light vs volume +0.87 vs +0.88;
closing 0-1% on all three songs. No gain change needed.

## 16. Julia Volume

**Owner's words (summary):** the same idea as Waverly, but with a 3D Julia
fractal. Less responsive than Waverly, maybe the nature of the math; make it
as responsive as Waverly if possible.

**Code:** smoothed volume x gain (`juliaSensitivity` 1.5) plus the midrange
boost sets a "closure" value that walks the fractal's parameters from a
broad opening toward near-closed. On top of the command board's smoothing,
the geometry is smoothed again (100 ms rise, 220 ms fall), and it draws
every 40 ms (25 fps, vs 60 for Waverly).

**Replay check (vs Waverly):**
- Fill barely moves: 46-52% (Waverly 33-59%). Loud/soft light ratio 1.1x
  (Waverly 1.8x). Light vs volume +0.52 (Waverly +0.88).
- Reaction after hits 1.3-1.5x, similar to Waverly.
- Silence: 41% lit, slow motion. Alive.

**Why it's less responsive:** mostly the mapping, not the math. The
closure path changes the fractal's shape a lot but its lit volume little,
and double smoothing at 25 fps blurs the rest.

**Proposals:**
- Re-map closure so loudness moves the part of the parameter path where the
  visible volume changes most (measure lit volume vs closure in the replay,
  then pick the path or a curve that makes it close to linear).
- Drop the extra geometry smoothing (or shorten it) since volume is already
  smoothed; check whether the frame rate can rise.
- After removing the midrange boost, set its gain against the reference
  songs; target Waverly-like numbers (light vs volume ~+0.85, loud/soft ~2x).

## Idea: PS GEQ Nova (new routine, replaces the failed attempt)

Use the PS Spray "particle hose": eight hoses from the center at 45 degree
steps, each its own color (band), spraying outward. Gravity bends the streams
down, so watch that the lower half doesn't turn into a mess.

## Cross-cutting: tempo speed in silence and slow music

Owner question: does BPM ramp up quickly but down slowly? Code today:
- Command board: tempo "active" drops after 1.2 s below the gate.
- Render clock: speed eases toward the target with a 0.5 s time constant,
  the same up and down. The target is the BPM estimate (never below 60) while
  active, and 12 BPM (idle) once inactive. Expected settle into the idle
  slow-down: about 1.2 s + 1.5 s, roughly 3 s.
- Likely real causes of a slow or missing slow-down: any gate flicker (rumble)
  restarts the 1.2 s timer and snaps the target back to the last tempo; quiet
  or slow music never goes below the 60 BPM floor (0.52x shape speed), so
  "slow music" only gets the full slow-down in true silence.
- To do: measure the slow-down in a replay with silence after music, and on
  hardware with the high-pass gate.
