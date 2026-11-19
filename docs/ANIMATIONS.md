# Animation catalog and playback

[Documentation index](README.md) · [Palette policies](architecture/PALETTES.md)

This catalog reflects `TestDemo.cpp` and `TestDemoAudio.cpp` at the documented
checkpoint. A concept ID is an internal stable table index; it is not the same
as its current position in the ordered deck. Serial `[position/total]` can
include disabled positions. Combined cards choose a variation at entry.

## Ambient play order

The table lists **actual descending order**, starting at the original front
of the visual sequence (Box). Disabled entries are shown in their positions
with no play number. Enabling a disabled card changes the first visible card
if it precedes Box. The nine Box brushes can require several Next presses
before Sphere; repeated Box entries are intentional. The disabled palette
audition has its own per-palette Next stepping.

| Play | Scene | Family / source under `render-board/src/` | Notes |
| --- | --- | --- | --- |
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
| 18 | Wobbling Dish | Surfaces.h | Rotating/wobbling curved dish with selected material. |
| 19 | Aurora | Surfaces.h | Two wispy curtains; validate on large cube. |
| 20 | Vert Streamer | Streamers.h | Four independently painted wandering stream heads. |
| 21 | What The Helix | Streamers.h | Helical streamer paths with non-solid material. |
| 22 | DNA Spiral | Streamers.h | Two intertwined strands with fixed rainbow treatment. |
| 23 | Chunchun (Liss) | Streamers.h | Flock points following the Lissajous-style path. |
| 24 | Chunchun (Serp) | Streamers.h | Original serpentine volumetric flock path. |
| 25 | PS Ghost Rider | Streamers.h | Travelling head with emitted spiral/diverging particle tail. |
| 26 | Hula | Streamers.h | Rotating swept hoop/streamer presentation; non-solid palette treatment. |
| 27 | Pipes | Streamers.h | Independent vertical pipe motion with its own coloring. |
| 28 | Wild Mouse | Streamers.h | Wandering heads with solid or independently selected palette materials. |
| 29 | PS Spray | Streamers.h | Position and aim randomized on entry. |
| 30 | Comets | Streamers.h | Volume-scaled 3D heads with fading rainbow trails. |
| 31 | Drift Rose 3D | Streamers.h | Non-audio shared irregular petal/rose geometry. |
| 32 | Vector Arms | Streamers.h | Vector-driven arms with its preserved color treatment. |
| 33 | Plasma Arms | Streamers.h | Rainbow plasma-field arm presentation. |
| 34 | PS Galaxy | Swirls.h | Two or three spiral arms chosen equally; rocking full disc. |
| 35 | Black Hole | Swirls.h | Orbital stars/rings around the center; distinct from PS Galaxy. |
| 36 | PS Attractor | Swirls.h | Deck enables AgeColor; Move/Swallow remain off. |
| 37 | Cyclone | Swirls.h | Organized winding streamers; volume-based footprint. |
| 38 | Tornado | Swirls.h | Spinning funnel/helix geometry with selected palette. |
| 39 | Pinwheel Arms | Swirls.h | Progressive 4 → 6 → 8 → 10 arms with transitions. |
| 40 | Atomic Orbits | Swirls.h | Four rainbow-traced orbits, central cycling sphere, shared tumble/ellipse renderer. |
| 41 | Atom Smasher | Explosions.h | Incoming particles meet and launch a destruction burst. |
| 42 | Fireworks | Explosions.h | Random selection across the implemented burst families. |
| 43 | PS Impact | Explosions.h | Falling colored meteor, kinetic tail, and floor splash. |
| 44 | Flying Box | Boxes.h | Moving box with newly selected materials during its lifecycle. |
| 45 | Moving Boxes | Boxes.h | Multiple independently painted moving boxes. |
| 46 | Rolling Ball | Balls.h | Textured moving sphere, solid or shell. |
| 47 | Bouncing Balls | Balls.h | Separate older strand-oriented bouncing scene; not the same as Bouncing Ball. |
| 48 | PS Box | Balls.h | 200 reference balls; 250 ms displayed-color transition. |
| 49 | Bouncing Ball | Balls.h | Multiple material-bearing bodies; gravity, contacts, bounded energy reinjection. |
| 50 | Lava Lamp | Balls.h | Palette-colored evolving volumetric blobs/metaballs. |
| 51 | Color Twinkles | Patterns.h | Independent colored sparkle/twinkle population. |
| 52 | Rain | Patterns.h | Falling textured/colored rain presentation. |
| 53 | Random Fall | Patterns.h | Falling points with a faster return journey. |
| 54 | BPM Volumetric | Patterns.h | Ambient musical-rate mathematical pulse pattern; not live audio input. |
| 55 | The Matrix | Patterns.h | Original narrow green rain trails with pale tips. |
| 56 | PS Fire | Flows.h | Compact hot embers, floor-area emitters, shared wind and local turbulence. |
| 57 | PS Waterfall | Flows.h | One simulated source rendered at four faces. |
| 58 | PS Volcano | Flows.h | Deck sets plumeWidthScale=1.5; single wider jet. |
| 59 | Eye of Sauron | EyeOfSauron.h | Radial fire with black through-volume pupil. |
| 60 | Tetris | Games/ | Autonomous Tetris animation; distinct from interactive registry app. |
| 61 | Rubiks Cube | Games/ | Autonomous scramble-and-solve show. |
| 62 | Text: F Yeah | Letters.cpp / Icons.cpp | Fixed text/icon marquee with dimmed textured background. |
| 63 | Text: Just Yes | Letters.cpp / Icons.cpp | Fixed text/icon marquee with dimmed textured background. |
| 64 | Text: Cubina | Letters.cpp / Icons.cpp | Fixed text/icon marquee with dimmed textured background. |
| 65 | Text: Pizza | Letters.cpp / Icons.cpp | Fixed text/icon marquee with dimmed textured background. |
| 66 | Icons: CW | Letters.cpp / Icons.cpp | Clockwise wrapped icon sequence. |
| 67 | Text: FYB | Letters.cpp / Icons.cpp | Fixed text/icon marquee with dimmed textured background. |
| Disabled | Ovoid | Shapes.h (disabled) |  |
| Disabled | Torus | Shapes.h (disabled) |  |
| Disabled | Icons: CCW | TestDemo.cpp / retained experiment |  |
| Disabled | Text: Israel | TestDemo.cpp / retained experiment |  |
| Disabled | Noise Clouds | TestDemo.cpp / retained experiment |  |
| Disabled | Meteor 3D | TestDemo.cpp / retained experiment |  |

There are 67 enabled ambient concepts out of 74. Shuffled playback uses
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
Clouds, Meteor 3D, and Palette Audition. Disabled does not mean removed from
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
