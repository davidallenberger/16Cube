# Animation catalog and playback

[Documentation index](README.md) · [Palette policies](architecture/PALETTES.md)

This catalog reflects `TestDemo.cpp` and `TestDemoAudio.cpp` at the documented
checkpoint. A concept ID is an internal stable table index; it is not the same
as its current position in the ordered deck. Serial `[position/total]` can
include disabled positions. Combined cards choose a variation at entry.

## Ambient play order

The table lists **actual descending order**, starting with Box in the original visual sequence. Fractal Cavern plays immediately after Julia 3D. Disabled entries are shown in their positions
with no play number. Enabling a disabled card changes the first visible card
if it precedes the enabled auditions. The nine Box brushes can require several Next presses
before Sphere; repeated Box entries are intentional. The disabled palette
audition has its own per-palette Next stepping.

| Play | Scene | Family / source under `render-board/src/` | Notes |
| --- | --- | --- | --- |
| Disabled | Fractal Sculpture | Surfaces.h / GeometryEngine.h | Disabled after rough-sphere feedback; retained callable for comparison. |
| Disabled | Palette Audition | TestDemo.cpp / Shapes.h | 49 Linear Flow full-box samples when enabled. |
| 1 | Box | Shapes.h | Nine-brush full-volume texture rotation. |
| 2 | Sphere | Shapes.h | Static or expanding sphere with textured coverage. |
| 3 | Tetrahedron | Shapes.h | Expanding or tumbling; solid and wireframe options. |
| 4 | Octahedron | Shapes.h | Spinning solid; two contrasting halves are available through shape color hooks. |
| 5 | Merkaba | Shapes.h | Tumbling interlocking tetrahedra; component contrast schemes. |
| 6 | Cylinder | Shapes.h | Tumbling body and end caps. |
| 7 | Hemispheres | Shapes.h | Opposed hemispheres; expansion or tumble. |
| 8 | Hourglass | Shapes.h | Tumbling pinched waist profile. |
| 9 | Hinge Dance | Planes.h | Planes hinge around shared edges. |
| 10 | Splat | Planes.h | Planes land/collapse in successive directions with independently selected materials. |
| 11 | Bounce | Planes.h | Axis-oriented planes bounce through the volume. |
| 12 | Smashing Planes | Planes.h | Travel, split into counter-moving planes, then destroy into particles. |
| 13 | Plane Trails | Planes.h | Sparse matrix of heads/trails moving along a randomly selected dimension. |
| 14 | Tartan / Passing Planes | Patterns.h / Planes.h | 50/50 on entry; Tartan is in Patterns.h, Passing Planes in Planes.h. |
| 15 | Saddle | Surfaces.h | Textured hyperbolic-paraboloid surface with axis selection. |
| 16 | Sine Wave | Surfaces.h | One-third rocking variant; otherwise ordinary wave / cuboid split wave. |
| 17 | Julia 3D | Surfaces.h | Shared evolving fractal cavern with independently cycling colors. |
| 18 | Fractal Cavern | Surfaces.h / GeometryEngine.h | Moving Mandelbox window; 3× motion/color speed after first audition. |
| 19 | Wobbling Dish | Surfaces.h | Rotating/wobbling curved dish with selected material. |
| 20 | Aurora | Surfaces.h | Two wispy curtains; validate on large cube. |
| 21 | Vert Streamer | Streamers.h | Four independently painted wandering stream heads. |
| 22 | What The Helix | Streamers.h | Helical streamer paths with non-solid material. |
| 23 | DNA Spiral | Streamers.h | Two intertwined strands with fixed rainbow treatment. |
| 24 | Fibonacci Scaffold / Spiral | Surfaces.h / GeometryEngine.h | 50/50 at entry: fourfold scaffold with 4-plane trail or Fibonacci spiral with 8-plane trail. |
| 25 | Chunchun (Liss) | Streamers.h | Flock points following the Lissajous-style path. |
| 26 | Chunchun (Serp) | Streamers.h | Original serpentine volumetric flock path. |
| 27 | PS Ghost Rider | Streamers.h | Travelling head with emitted spiral/diverging particle tail. |
| 28 | Hula | Streamers.h | Rotating swept hoop/streamer presentation; non-solid palette treatment. |
| 29 | Pipes | Streamers.h | Independent vertical pipe motion with its own coloring. |
| 30 | Recursive Cube | Surfaces.h / GeometryEngine.h | Eight synchronized crisp rainbow mice; immediate start and deeper recursive rectangles. |
| 31 | Wild Mouse | Streamers.h | Wandering heads with solid or independently selected palette materials. |
| 32 | PS Spray | Streamers.h | Position and aim randomized on entry. |
| 33 | Comets | Streamers.h | Volume-scaled 3D heads with fading rainbow trails. |
| 34 | Drift Rose 3D | Streamers.h | Non-audio shared irregular petal/rose geometry. |
| 35 | Vector Arms | Streamers.h | Vector-driven arms with its preserved color treatment. |
| 36 | Plasma Arms | Streamers.h | Rainbow plasma-field arm presentation. |
| 37 | PS Galaxy | Swirls.h | Two or three spiral arms chosen equally; rocking full disc. |
| 38 | Black Hole | Swirls.h | Orbital stars/rings around the center; distinct from PS Galaxy. |
| 39 | PS Attractor | Swirls.h | Deck enables AgeColor; Move/Swallow remain off. |
| 40 | Cyclone | Swirls.h | Organized winding streamers; volume-based footprint. |
| 41 | Tornado | Swirls.h | Spinning funnel/helix geometry with selected palette. |
| 42 | Atomic Orbits | Swirls.h | Four rainbow-traced orbits, central cycling sphere, shared tumble/ellipse renderer. |
| 43 | Coprime Weave | Surfaces.h / GeometryEngine.h | Three broad ribbons with 2.5/3.5/5.5-second periods and shared tumble. |
| 44 | Prime Symmetry | Surfaces.h / GeometryEngine.h | Interacting 3/5/7-arm figures in orthogonal planes. |
| 45 | Pinwheel Arms | Swirls.h | Progressive 4 → 6 → 8 → 10 arms with transitions. |
| 46 | Atom Smasher | Explosions.h | Incoming particles meet and launch a destruction burst. |
| 47 | Fireworks | Explosions.h | Random selection across the implemented burst families. |
| 48 | PS Impact | Explosions.h | Falling colored meteor, kinetic tail, and floor splash. |
| 49 | Flying Box | Boxes.h | Moving box with newly selected materials during its lifecycle. |
| 50 | Moving Boxes | Boxes.h | Multiple independently painted moving boxes. |
| 51 | Rolling Ball | Balls.h | Textured moving sphere, solid or shell. |
| 52 | Bouncing Balls | Balls.h | Separate older strand-oriented bouncing scene; not the same as Bouncing Ball. |
| 53 | PS Box | Balls.h | 200 reference balls; 250 ms displayed-color transition. |
| 54 | Bouncing Ball | Balls.h | Multiple material-bearing bodies; gravity, contacts, bounded energy reinjection. |
| 55 | Lava Lamp | Balls.h | Palette-colored evolving volumetric blobs/metaballs. |
| 56 | Color Twinkles | Patterns.h | Independent colored sparkle/twinkle population. |
| 57 | Rain | Patterns.h | Falling textured/colored rain presentation. |
| 58 | Random Fall | Patterns.h | Falling points with a faster return journey. |
| 59 | BPM Volumetric | Patterns.h | Ambient musical-rate mathematical pulse pattern; not live audio input. |
| 60 | The Matrix | Patterns.h | Original narrow green rain trails with pale tips. |
| 61 | PS Fire | Flows.h | Compact hot embers, floor-area emitters, shared wind and local turbulence. |
| 62 | PS Waterfall | Flows.h | One simulated source rendered at four faces. |
| 63 | PS Volcano | Flows.h | Deck sets plumeWidthScale=1.5; single wider jet. |
| 64 | Eye of Sauron | EyeOfSauron.h | Radial fire with black through-volume pupil. |
| 65 | Tetris | Games/ | Autonomous Tetris animation; distinct from interactive registry app. |
| 66 | Rubiks Cube | Games/ | Autonomous scramble-and-solve show. |
| 67 | Text: F Yeah | Letters.cpp / Icons.cpp | Fixed text/icon marquee with dimmed textured background. |
| 68 | Text: Just Yes | Letters.cpp / Icons.cpp | Fixed text/icon marquee with dimmed textured background. |
| 69 | Text: Cubina | Letters.cpp / Icons.cpp | Fixed text/icon marquee with dimmed textured background. |
| 70 | Text: Pizza | Letters.cpp / Icons.cpp | Fixed text/icon marquee with dimmed textured background. |
| 71 | Icons: CW | Letters.cpp / Icons.cpp | Clockwise wrapped icon sequence. |
| 72 | Text: FYB | Letters.cpp / Icons.cpp | Fixed text/icon marquee with dimmed textured background. |
| Disabled | Ovoid | Shapes.h (disabled) |  |
| Disabled | Torus | Shapes.h (disabled) |  |
| Disabled | Icons: CCW | TestDemo.cpp / retained experiment |  |
| Disabled | Text: Israel | TestDemo.cpp / retained experiment |  |
| Disabled | Noise Clouds | TestDemo.cpp / retained experiment |  |
| Disabled | Meteor 3D | TestDemo.cpp / retained experiment |  |

There are 72 enabled ambient concepts out of 80. Shuffled playback uses
weighted art and billboard bags rather than this order. `demoBoost` controls
bag multiplicity, and the Box brush rotation creates a slot for each of nine
materials. Enabled rules, permitted shape motions/renders, and dispatch remain
the source of truth; a method existing in a family file does not enable it.

## Audio play order and visual mapping

Audio has a separate deck; default descending playback gives the order below.
The active source is independent of the selected visual. Each call starts its
local effect state again; managed playback owns timers and Next. Audio method
calls outside the managed deck are intentionally untimed development calls.

| Play | Card | Input and spatial idea |
| --- | --- | --- |
| 1 | Audio Shapes | 50/50 Cylinder or Octahedron; dedicated kicks recolor the body, BPM changes rotation, volume sizes eight corner cubes |
| 2 | Frequency Cubes 8 Bands Corner | Merge adjacent FFT pairs into eight regions; grow solid cubes outward from the corner nearest center; size response with release smoothing, full color and minimum one point |
| 3 | PS GEQ | Equal choice of 4-, 16-, or 32-nozzle same-band clusters; frequency-driven launches, spread, gravity, and short afterimage |
| 4 | Freqwave | Equal wireframe-box or sphere shell; beat-clock emission, dominant band chooses hue, continuous outward travel at max sample brightness |
| 5 | Frequency Planes / Discs | Equal shape choice; eight paired bands control area of mirrored horizontal planes or filled discs |
| 6 | GEQ | Surround bars + center bass sphere (2/3 probability) or inward bars (1/3) |
| 7 | Funky Plank | Eight merged band lanes, two columns wide around perimeter; discrete ROYGBIV level buckets flow through history |
| 8 | Bouncing Balls Audio | Rainbow tile groups jump with audio cues; equal Clockwise or OrbitInward pattern selection, retained bounce energy |
| 9 | Plasmoid Depth Mirror | WLED-inspired strip waveform mapped through mirrored cube depth; volume controls visible fill |
| 10 | Drift Rose Volume | Shared rose renderer, 80 spokes, volume/midrange fallback changes reach |
| 11 | Corner Burst | Equal Plane/Sphere/Box front; volume holds crest, sustained fall releases it to travel independently |
| 12 | Breathe | Equal Sphere, Checkerboard, Diagonal, Helix; shared musical clock controls breathing |
| 13 | Puddle Peak | Shared qualifying attacks spawn bursts; smoothed volume sizes them |
| 14 | Puddles | Volume-sized transient shapes without the peak-only policy |
| 15 | Waverly | Volume-driven opening/closing surfaces; optional midrange fallback; curated moving looks |
| 16 | Julia Volume | Volume selects closure on the shared fractal parameter path, with effect-local smoothing |

Attack Monitor remains disabled between Corner Burst and Breathe in descending
order. Waverly chooses equally among six mechanisms, then equally between
Rainbow/Party for the five palette-based mechanisms; Hiphotic uses native hues.
This is not an equal choice among its eleven named looks. Waverly, Breathe,
and Planes/Discs remember their selected look through a source restart and
choose again after manual/automatic advance.

### PS GEQ layout names

The 4/16/32 labels describe repeated launch positions per band, not FFT size:

- **4** (`GeqLayout::Clusters`, `animateParticleGeqClusters`): 16 bands with
  16 nozzle copies each, arranged as 2×2 same-band clusters repeated in the
  four floor quadrants. Every quadrant contains all 16 bands.
- **16** (`Tiles`, `animateParticleGeqTiles`): 16 bands, one 4×4 same-band
  tile each. The placement intentionally swaps selected bands to put some
  mids toward the center.
- **32** (`PairedBands`, `animateParticleGeqPairedBands`): eight adjacent
  merged band pairs, each occupying a 4×8 patch (32 nozzle positions).

Each layout covers 256 reference floor positions on 16³. One compact record
can be replicated across nozzle copies; 256 nozzles do not require a distinct
simulated record per nozzle per launch. All use the approved simultaneous
batch approach, speed 165, random lifetime/speed variation, divergence,
gravity, a short retained image, and a full-brightness nozzle floor. This is
still an audition: brightness, visual flicker, overlapping RGB, and sparse
frequency energy make matching the upstream 2D GIF difficult. The removed
WLED-comparison and old V1/V2/V3 cards are not additional active deck entries.

## Shape versus scene semantics

Ordinary shape cards randomly choose among allowed motions and render modes
from `SHAPE_RULES`; enabled motions are not uniform across shapes. Box is
stationary/filled with animated material. Sphere is static or expanding.
Octahedron spins at its halved deck rate. Merkaba tumbles. Cylinder tumbles.
Some methods support additional combinations not enabled in this deck.

Several visually related names represent distinct implementations:

| Names | Distinction |
| --- | --- |
| Black Hole / PS Galaxy | Tumbling orbital stars versus a persistent spiral disc with two/three arms |
| Bouncing Balls / Bouncing Ball / PS Box | Older strand bounces versus material-bearing colliding bodies with energy replenishment versus WLED-inspired force-driven particle box |
| Pinwheel / Plasma / Vector Arms | Progressive rotating arm geometry versus plasma field arms versus vector burst motion |
| Comets / Plane Trails | Free 3D heads versus a sparse matrix moving along a selected dimension |
| Fire / Volcano / Waterfall | Floor flame/turbulence versus an upward bouncing hot jet versus a falling four-face curtain |
| Corner Burst / Freqwave | Post-crest release from a corner versus beat-emitted shells from the center |

## Marquees, games, and preserved experiments

The final ambient group contains the enabled text/icon cards and autonomous
Rubiks/Tetris shows. Interactive games are separate CubeSystem registry apps;
selecting a game menu entry does not make it an ambient card.

Disabled ambient concepts are Ovoid, Torus, Icons CCW, Text Israel, Noise
Clouds, Meteor 3D, Palette Audition, and Fractal Sculpture. Disabled does not mean removed from
source. Previously deleted experiments (Octopus, Vortex, Rocktaves, audio
Merkaba/Sine Wave, PS Blobs/Ballpit, audio Spray/Attractor, and extra shell or
Matrix auditions) should not be described as merely waiting in the active deck.
The current source catalog takes precedence over historical chat names.

The eight-band Frequency Cubes card uses seven canonical rainbow swatches
plus white for its eighth region. Audio Shapes' corner cubes are a different
volume-driven overlay: their shared cycling color is ROYGBIV-only, with white
excluded. Cylinder's maximum corner cube is 3³; Octahedron's is 4³ on the
reference canvas. Their hue changes every six seconds with a 600 ms transition,
independently of the kick-driven body colors. Rotation maps quiet input toward
0.1× and faster BPM up to 3× through the shared shape frame-control callback.

## Fractal auditions

The regular grouped order is restored. Coprime Weave sits beside Prime Symmetry
and Pinwheel Arms in Swirls; Recursive Cube and Wild Mouse are adjacent in
Streamers. One combined Fibonacci card follows DNA Spiral. **Fractal Cavern** is accepted and plays immediately
after **Julia 3D**. **Fractal Sculpture**
is disabled after the first physical audition. Both use Rainbow at full material brightness with fractional boundary
coverage. Cavern now cycles the palette in eight seconds; Sculpture retains
its original 24-second cycle. They use GeometryEngine's shared fractal fields
and volumetric rasterizer, TextureState's cycling palette sampler, and the
existing cached tumble transform. Julia's renderer and existing look are
unchanged. There is no separate per-face rendering or camera projection.

Cavern samples a gently changing four-fold Mandelbox while translating through
it on three axes. On 16³ the translation component peaks at approximately
1.25 / 1.04 / 0.89 voxels per second in X/Y/Z, with an additional
1.5 / 2.4 / 0.9 degrees-per-second field rotation. A shared 3× cavern
speed multiplier also accelerates the small fold-scale evolution and color
cycle, leaving the shape, path, and palette intact. The path traverses the
fractal rather than repeatedly zooming unresolved details. The sampled region
was selected to keep visible surfaces and openings on all six faces.

Sculpture is a centered, five-iteration power-eight Mandelbulb with a shared
2.0 / 3.2 / 1.2 degrees-per-second tumble. Its low-order lobes remain a stable
object as its orientation and contour colors change. The power calculation
uses algebraic angle doubling instead of repeated trigonometric calls.

`Surfaces::FractalSettings` exposes frame interval (40 ms target), color period,
cavern speed multiplier/travel rate, and sculpture tumble rate. Serial reports the selected
look, cycle, motion rate, lit count, draw time, and frame work time. Slow
frames skip additional waiting; these are target rates, not hardware timing
measurements. No additional framebuffer or scratch arena is allocated.

Host checks cover 90 sampled seconds on both canvases, changing geometry and
colors, all-face cavern participation/openings, finite fields including poles,
Mandelbulb equivalence to a trigonometric reference at selected points, Hold,
clock wrap, and front-of-deck registration. On 16³ the sampled cavern occupies
roughly 37–49% of voxels and the sculpture roughly 40–41%. Physical speed,
smoothness, color, and visibility still require auditioning.

First physical feedback: Cavern is promising and received the 3× speed
increase. Sculpture reads as a rough sphere with artifacts, so additional
fractal detail is not the desired next step. A clearly deforming or recursive
open structure is the replacement direction. Both locally supplied video
references were subsequently inspected: organic multiscale branching in the
first and porous repeated architecture in the second informed the three
scaffold auditions below.

## Recursive scaffold auditions

**Recursive Cube** and **Wild Mouse** are adjacent at Wild Mouse's original
Streamers position: **Pipes → Recursive Cube → Wild Mouse → PS Spray**.
**Fibonacci Scaffold / Spiral** follows **DNA Spiral** and precedes Chunchun
(Liss). Its coin flip happens once at routine entry: 50% Scaffold, 50% Spiral;
the selected pattern remains unchanged through subsequent sweeps until Next.
Serial identifies the actual selected variant on each sweep. Shared geometry
owns all coverage; no private voxel renderer, framebuffer, particle pool, or
extra task is added.

Recursive Cube starts its eight synchronized mouse trails immediately, with no
outline hold. They share one spatially accelerated arclength clock, starting at
mirrored corners and tracing the parent frame, one-third-size corner cubes,
and center copies. This same connected walk now recurses again within every
copy while the smallest rectangle can still span at least one voxel, capped at
five levels. On 16³ there are three levels with sides 15, 5, and about 1.67
voxels; desktop also uses three levels. Each walk covers all edges, retracing
four edges per cube to close without jumping between generations. Tails grow
from entry and span three times Wild Mouse's length: three times (RNDR_X × 3
positions, capped at 64, minus one). On 16³ this is 141 voxels, 1.5x the prior
94-voxel trail. Speed is now 36 voxels/second on the outer faces and increases
linearly with cubical depth toward a 108-voxel/second center limit. The one-third
inner cube runs at about 84 voxels/second. The center-depth metric uses the
largest normalized axis distance, preserving mirrored synchronization. A
5ms-step midpoint integrator accumulates travel continuously, independently of
render cadence. Trail length/fading remain spatial rather than time based. Paths stay fixed, with crisp one-voxel plotting and
whole-hue overlap selection; only tail age reduces material brightness.

The original scale/tumble cube director remains callable through FractalLook
for comparison. The Sierpinski Octahedron audition has been deleted.

Fibonacci Scaffold keeps the previous pattern's outer-face design: four
quarter-turn copies of a right-angle path whose segments shorten by 1/phi.
A sweep starts from a randomly chosen side face (±X or ±Y, never ±Z),
moves through the full depth, and rotates smoothly by 45 degrees
per voxel of travel. One sweep takes eight seconds, including 400ms pauses at
each edge. Starting faces come from a shuffled four-face cycle: each of
±X/±Y occurs once before reshuffling, with no immediate repeat at a cycle
boundary. Pattern size remains fixed
while rotating, so corners can clip at the cube boundary instead of shrinking.
The old six-face, inward/outward layer sequence has been removed.

The Spiral variation of the combined card uses the identical sweep, rotation, and
18-second Rainbow cycle, but draws tangent quarter-circles with Fibonacci radii
1,1,2,3,5,8,13. Its 21x13 bounding rectangle is centered and uniformly scaled to
fit the starting face. This preserves the arc proportions and gives a clear
comparison against the angular fourfold pattern. Both use shared plane frames
and the core SDF line renderer; zero core radius keeps lines thin, with coverage
blending the moving leader between voxel layers. Scaffold has a four-plane
trail footprint and Spiral an eight-plane footprint. The leader leaves an
impression on each integer plane it passes. That impression stays at its
original position and 45-degree-per-plane orientation, fading linearly as the
leader advances. The entry-face image therefore fades once to zero; no delayed
copies subsequently enter from outside the cube. Palette cycling continues
across the material, and strongest whole-hue blending avoids whitening.
At integral head positions the 4/8 planes include the leader; fractional motion
also blends the head onto its adjacent voxel layer. All impressions reset when
a new sweep begins. Tests check that the entrance geometry stays fixed and its
brightness falls monotonically to zero for both variants on both canvas sizes.
The previous moving-copy model fails this regression, even after its abrupt
entry gate was softened.

The field director prints the look, palette period, tumble rate and frame work
time. The sweep director prints the pattern, chosen face, sweep duration, and
rotation per plane. Target frame interval remains 40 ms.
Actual hardware timing and whether the recursive hierarchy reads clearly from
several sides still need auditioning. Tests cover both canvas sizes, finite
bounded drawing, changing geometry/colors, scale-loop continuity, and the
thin primitive support, 4/8-plane trail lengths/fading and side-face-only
selection in both sweep variants, alongside the earlier
fractal and playback checks.

## Prime motion studies

These two auditions use full-brightness Rainbow material with an 18-second
color lap; fractional geometry coverage supplies edge smoothing. They share a
40ms target frame loop, duration/Next handling, and existing geometry coverage.
They add no framebuffer or particle pool. Serial prints the actual motion
periods, arm speed, and target timing for each selection.

- **Coprime Weave:** three broad curved strips span X, Y, and Z. Their bend/drift
  cycles now take 2.5, 3.5, and 5.5 seconds (2x original speed). Their local
  deformation repeats after 192.5 seconds; shared tumble continues independently.
  The whole weave rotates about the fixed cube center with the common tumble
  helper at 10/16/6 degrees per second (twice its prior tumble speed). Centerlines and ribbon width vectors rotate
  together. Size stays fixed, allowing tips to clip at the cube faces rather
  than making the object breathe as its orientation changes. Ribbon half-width
  is 22% of the inscribed half-span. Cross-strokes use the shared SDF line
  renderer with strongest whole-color overlap. The 18-second color lap is unchanged.
- **Prime Symmetry:** three-, five-, and seven-arm figures occupy three
  orthogonal planes, rotating over 2.5, 3.5, and 5.5 seconds (4× original arm speed). Curvature varies
  smoothly. Its shared tumble now matches the ribbons at 10/16/6 degrees
  per second. Each figure has a distinct cycling hue region and uses Pinwheel's circumscribed outer radius, half the canvas diagonal, so arms reach and
  clip at the cube faces throughout the tumble. The inner radius is unchanged. The prime counts govern recognizable rotational order.

Visual success and hardware frame rate still require auditioning. Host checks
cover both canvas sizes, drawing bounds, live geometry/color changes, scaffold
one-plane support, and coprime realignment.

Coprime Weave, Prime Symmetry and Pinwheel Arms are adjacent in Swirls,
at Pinwheel's original deck location. Descending playback is **Atomic Orbits →
Coprime Weave → Prime Symmetry → Pinwheel Arms → Atom Smasher**. The comparison card uses `comparison=true`, leaving
Pinwheel's original behavior available by default. Comparison mode uses a
3.5-second main spin, the same 10/16/6-degree tumble, an 18-second hue cycle,
and a 40ms target frame interval. Its material is full-brightness at all radii,
with the same SDF stroke coverage and strongest whole-hue overlap as the other
auditions. Radial dimming, additive Wu accumulation and persistent frame fading
remain in normal mode. Arm-layout transition fades remain in both modes.
`TestDemo::ArmComparison` owns the common comparison timing defaults.

Pendulum Wave was deleted after audition because its sinusoidal motion overlapped
too much with existing sine-wave routines.

Recursive Cube and Wild Mouse now share Wild Mouse's restored Streamers
location; their current motion/render settings are preserved.
