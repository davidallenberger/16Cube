# Graphics, motion, and particle engines

[Documentation index](../README.md) · [Palette policy](PALETTES.md)

## The rendering layers

A scene selects geometry, motion, and material. Geometry determines which
voxels are covered; a texture samples color in space/time; composition combines
coverage and color with the existing frame; `CubeCore` maps logical coordinates
to output arrays and transmits them. Effects can have bespoke motion and
composition while reusing the primitive renderer.

```text
TestDemo dispatch or app
  -> effect family / Shapes motion wrapper
     -> GeometryEngine rasterization + TextureState samples
        -> setVoxel / getVoxel
           -> physical CRGB arrays -> showCube -> FastLED I2S
```

## Geometry is shared infrastructure

[`GeometryEngine.h`](../../render-board/src/GeometryEngine.h) owns primitives,
coverage, and reusable transforms. Add a missing primitive here rather than
inventing a private voxel-box/sphere renderer inside an audio effect.

| Facility | Role |
| --- | --- |
| `getCoverage()` | Solid/shell/wireframe coverage from surface/edge distance |
| `drawFastLine3D()` | Integer/rounded line sampled with a texture |
| `drawWuVoxel()` / `drawWuLine3D()` | Fractional point/line splats in 8-bit fractional fixed-point coordinates |
| `drawStrandVoxel()` | Blend a point between two Z positions on one integer X/Y strand |
| `drawSdfLine3D()` | Rounded/thick segment with distance-based coverage |
| `drawEllipse3D()` | Arbitrarily oriented ellipse from center and two basis vectors, progressive reveal, palette around the perimeter |
| `drawSphere()` / `rasterSphere()` | Textured sphere or callback-based sphere traversal |
| `rasterDepthFeatheredSphere()` | Shared sphere column coverage used by volumetric presentations |
| `drawBox()` / `drawFastBox()` / `rasterFastBox()` | Rotated or axis-aligned box rendering/traversal |
| `drawCornerCube()` | Fractional-size cube growing from an anchored corner |
| `paintPlane()` / `rasterHorizontalPatch()` | Axis plane and horizontal rectangle/disc coverage |
| Polyhedral/curved draw methods | Tetrahedron, octahedron, merkaba, cylinder, hemispheres, hourglass, ovoid |
| `sineWaveHeight()` / wave draw methods | Shared ordinary and split-cuboid sine surfaces |
| `helixPoint()` | Shared parametric helix position |
| `tumbleAngles()` / `TumbleRotation` | Common multi-axis rotation and cached transform |

The engine has both smooth and discrete renderers. Calling the core library
does not automatically enable antialiasing: choose the primitive and blending
method appropriate to the scene. Likewise, shell and wireframe are different
objects. A shell covers faces; a wireframe covers edges.

[`Shapes.h`](../../render-board/src/Shapes.h) provides static,
expanding/collapsing, spring, tumbling, and spinning wrappers over geometry.
It dispatches `ShapeType` to the core draw method and applies established
render/color conventions. Curved shapes requested as wireframes are forced
to solid; the ambient rules already restrict allowed combinations. White/Gray
palette sentinels can request component contrast schemes on selected shapes.
They are semantic hooks, not simply literal uniform white/gray in every case.

`Shapes::FrameControl` lets audio supply per-frame texture changes, rotation
multipliers, a continuation callback, and overlay drawing. Audio Shapes uses
this route instead of cloning the shape renderer. `JuliaRenderer.h` likewise
shares the fractal between ambient Surfaces and audio closure control.
`StreamerGeometry.h` contains reusable arm geometry. Check these helpers before
adding a second implementation of an existing visual.

## Rotation and antialiasing

The common tumble uses differently scaled axis angles (0.5, 0.8, 0.3 times
phase/speed), with a cached `TumbleRotation` when suitable. This produces the
familiar multi-axis motion used around Merkaba and related objects; it is not
a general rigid-body physics solver. Check units: the cached rotation accepts
degrees; `helixPoint()` accepts radians; individual fixed-point draw paths
convert their own inputs.

`drawWuVoxel()` distributes one sample over up to eight neighboring voxels
with trilinear weights and saturating addition. `drawStrandVoxel()` distributes
along Z only. Distance-based sphere/shape coverage shades boundary voxels
according to geometric overlap. These are intentional fractional contributions;
one sample halfway among neighbors does not make each neighbor full intensity.
Several samples can add to a bright line, and additive RGB overlap can head
toward white. Independent primitives can alternatively use replacement or
strongest-color policies.

For moving box edges, duplicating parallel fully bright outlines is not an
acceptable replacement for continuous coverage. Inspect axis-aligned boundary
cases and the outermost strands when changing wireframe smoothing. For sparse
particles, integer nearest-voxel rendering may preserve a clear one-voxel head
but produces spatial stepping; a soft sphere trades brightness distribution
and apparent size for continuity. Pick deliberately and audition physically.

## Textures and composition

[`TextureEngine.h`](../../render-board/src/TextureEngine.h) exposes
`TextureState(shader, palette, speed, ...)`. Update state once per frame, then
sample `getColor(x,y,z)` as geometry is rasterized. Texture movement and object
movement are independent. `TexturePlan` packages the selected shader, palette,
and stable diagnostic metadata. Full descriptions and pool restrictions are
in [Palettes](PALETTES.md).

Text and icon marquees use the same materials with their own foreground,
background dimming, wrapping, and transforms. `Letters`, `Icons`, and `Hebrew`
own glyph/icon data and parsing. Their explicit background dimming is a
legibility policy, not a global geometry brightness rule.

`EyeOfSauron.h` composes a sphere and a moving pupil cut. Its approved material
is the radial-fire sampler in TextureEngine; the current pupil is black through
the volume. It is a specialized radial material, not another public shader enum
or an extra ambient Soap/Waving Cell card.

## Particle infrastructure and its limits

[`ParticleEngine.h`](../../render-board/src/ParticleEngine.h) supplies:

- `Motion`: 3D position and velocity.
- A general `Particle` record: active/type, motion, drag, gravity, life/decay,
  color, and flags. Effects can define smaller custom records.
- `ParticlePool<T>`: bounded arena storage, first-free search, spawning, and
  alive queries. Full pools reject new spawns; they do not grow the heap.
- Integration, open/bounce/wrap sides, floor restitution/roughness, sphere
  contact resolution, and grid-based collision candidate traversal.
- Particle rendering with fractional sphere edges and selectable blend
  policies (Add, strongest-color behavior, or EmptyOnly), optional full core,
  and per-frame coverage to distinguish live drawing from afterimages.

The basic integrator advances position by velocity×dt, then applies retained
velocity and acceleration. **Drag is per simulation step**, not per second.
Changing dt without retuning drag changes behavior. Collision separation uses
an approximate positional correction and impulse; visual overlap is still
possible through soft edges, afterimages, discretization, and multiple samples
in one voxel. Physics contact is not exclusive ownership of an LED.

[`ParticleEffectSupport.h`](../../render-board/src/ParticleEffectSupport.h)
provides shared sampling, volume-based population scaling, emitter motion,
spray stepping, and the ordinary fixed-step runner. Its baseline is 60 Hz,
with at most three steps of newly accrued elapsed time per display iteration.
The WLED emission-opportunity helper distributes seven opportunities per ten
60 Hz ticks (~42/s). Some effects deliberately use custom 42 Hz simulation,
more compact records, or a custom clock; the helper is not a universal timing
contract imposed on all particle effects.

WLED-style velocity/gravity conversions are centralized there. This is a
native, bounded 3D adaptation, not a copy of WLED's complete 2D engine. Settings
structs expose relevant controls per effect. Defaults and deck overrides
matter: a struct's default is not necessarily the active deck setting.

### Important consumers

| Consumer | Implementation / distinctive behavior |
| --- | --- |
| Fireworks, Atom Smasher, PS Impact | `Explosions.h`; shared particle infrastructure and scene-specific bursts/tails |
| Smashing Planes | `Planes.h`; plane lifecycle plus destruction particles |
| PS Volcano / PS Waterfall / PS Fire | `Flows.h`; different emission and simulation strategies |
| PS Spray / PS Ghost Rider | `Streamers.h`; nozzle/path-driven trails |
| PS Attractor / PS Galaxy | `Swirls.h`; attraction versus persistent spiral geometry |
| PS Box / bouncing scenes | `Balls.h`; distinct physics and rendering choices |
| PS GEQ | `ParticleEffects.h/.cpp`; frequency launches, replicated nozzle layouts, custom compact pool |

Fire uses compact records (size bounded to 12 bytes), allowing a larger pool
than the general record, and local flow/turbulence. It does not solve a fluid
volume. Waterfall simulates one source and draws rotated copies near four
faces; the copies share history and do not collide as four independent falls.
Galaxy's non-starfield presentation keeps persistent normalized spiral seeds
and rocks a full-sized disc; it is not an N-body gravity simulation.

## Brightness and performance

Distinguish four causes of lower visible light: palette entries themselves,
effect life/heat or explicit gain, spatial coverage/antialiasing, and global
FastLED brightness. Passing `255` to a palette sample only removes that sample's
brightness scale; a dark palette entry and fractional boundary can still be dim.
Full-brightness cores are opt-in where needed. PS Impact/GEQ/Ghost Rider and
other tuned effects retain intentional rendering choices; do not remove every
fade merely because global brightness also exists.

The show cap is ~60 Hz; actual display transfer and draw cost can dominate.
Fire's diagnostics separate simulation, drawing, and show time, allowing a
42 Hz simulation alongside a lower render rate. Increase emission or coverage
only after checking pool rejection, stack/arena use, and measured timing.
Replicating a source can cheaply increase visible area without multiplying
simulation state, but correlated copies and additive overlap are visible
tradeoffs. Physical appearance remains part of validation.
