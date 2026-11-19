# Palette, shader, and color policy

[Documentation index](../README.md) · [Graphics](GRAPHICS.md)

## Catalog and selection

[`WledPalettes.cpp`](../../render-board/src/WledPalettes.cpp) contains 44 curated
WLED keepers plus five original FastLED palettes: Party (6), Lava (8), Ocean
(9), Forest (10), Rainbow (11). There are **49** catalog entries, not every
palette distributed by WLED and not hundreds of external gradients. IDs and
names follow the imported catalog. Imported gradient data retains attribution
and the [upstream license](../licenses/WLED-EUPL-1.2.txt). Gradient loading
approximates the expected WLED gamma output; identical appearance on different
hardware is not guaranteed.

`INCLUDE_WLED_PALETTES=1` is set in render `platformio.ini`. In this mode the
general and organic pickers draw equally from the eligible entries of one
catalog; classic palettes get no extra preference. Their names persist for
API compatibility. The header fallback is 0, so the build file matters.

| Picker / plan factory in `PaletteUtils` | Policy |
| --- | --- |
| `getRandomPalette(allowDark=false)` | Enabled catalog: uniform eligible entry; flag off: five classic palettes |
| `getRandomOrganicPalette(allowDark=false)` | Enabled catalog: same eligible pool; flag off: Ocean/Lava/Forest/Party |
| `getRandomClassicOrganicPalette()` | Always the four classic organic palettes; used by Snake games |
| `getRandomVibrantColor()` | Random full-saturation/full-value HSV solid; not a catalog palette |
| `getSmartPalette(shader,useFullPool=false,allowDark=false)` | Shader-aware choice described below |
| `getRandomNonSolidShader()` | All shader enum values except Solid; includes frozen Spatial Gradient |
| `getRandomNonSolidTexturePlan(...)` | Chooses that shader plus its material policy and diagnostic metadata |
| `getRandomTexturePlan(...)` | 33% Solid, otherwise the non-solid plan factory |
| `getRandomMovingTexturePlan()` | Linear Flow, Noise, Distance, Hiphotic, Distortion, Soap, Waving Cell; excludes Solid, Uniform Cycle, frozen Gradient, Scatter |

`useFullPool=true` expands eligible shader palettes; it does **not** permit
dark palettes. `allowDark=true` is a separate geometry permission. Waving Cell
retains its own Rainbow/Party choice even with full-pool selection; Hiphotic
uses native hues. A renderer using its palette directly may bypass Smart's
shader policy intentionally; consult its actual entry point.

## Shader behavior

| Shader | What changes | Default Smart policy / qualifications |
| --- | --- | --- |
| Solid | One color, fixed in time/space | Vibrant HSV solid |
| Uniform Cycle | Whole object changes palette position together | Smart defaults Party; non-solid plan defaults Rainbow; full pool overrides |
| Spatial Gradient | Fixed coordinate-indexed palette field | Smart normally picks a single hue; non-solid plan can use a full organic palette; full pool expands it |
| Linear Flow | Coordinate gradient plus advancing phase | Organic/general eligible pool |
| Noise Field | Evolving coherent 3D noise palette coordinates | Organic/general eligible pool |
| Scatter | Spatial hash-like palette indices plus phase | Smart normally Party; non-solid plan uses organic; full pool expands it |
| Distance Field | Moving field cores determine palette index | Organic/general eligible pool |
| Hiphotic | Structured trigonometric hue field | Native HSV, ignores the palette for visible coloring |
| Distortion Waves | Spatial interference/distortion field | Organic/general eligible pool |
| Soap | Noise-displaced dye field sampled in 3D | General eligible pool; no Forest ban remains |
| Waving Cell | Coherent depth-phased extension of 2D waves | Rainbow or Party only |

A shader describes material motion, not shape motion. A non-solid shader can
be static. The stationary Box/Sphere deck policy deliberately uses evolving
materials. Soap and Waving Cell are available as materials and guaranteed
full-box brush auditions; they are not separate enabled ambient scenes.

## Nine dark palettes

The user-curated `Entry::isDarkPalette` list is explicit:

| ID | Name |
| --- | --- |
| 12 | Rainbow Bands |
| 32 | Grintage |
| 39 | Autumn |
| 45 | Tiamat |
| 46 | April Night |
| 50 | Aurora |
| 63 | Aqua Flash |
| 66 | Red Flash |
| 67 | Blink Red |

The default pool therefore contains 40 choices. Filled boxes and flat planes
may opt into all 49, moving or stationary. Wireframes, curved shapes, particles,
and streamers do not gain that permission merely by being large. Positive
opt-ins currently include non-wireframe Box/Flying Box, filled Moving Boxes,
Hinge Dance, Splat, Bounce, and Smashing Planes. Passing Planes uses curated
mixing recipes; Tartan uses fixed Rainbow. Direct palette loading for the
review audition can show all entries regardless of dark classification.

This list records visual curation, not an automatic test for any zero RGB
entry. A palette not on it can still look dim in a particular scene. Add new
members deliberately after auditioning and keep the catalog flag authoritative.

## Per-effect policies and diagnostics

Not every effect uses the general catalog. Fixed Rainbow/ROYGBIV, fire heat,
paired component colors, ocean water, and per-particle hue policies remain
intentional. Waverly keeps its own approved Rainbow/Party/native looks.
Plasma Arms, Vector Arms, Drift Rose, Comets, Spray, Pipes, Ghost Rider, DNA,
and other self-managed effects have their own policies; enabling WLED does
not rewrite their internal palettes. The active dispatch and implementation
are the source of truth; an opening “Internal” label is not proof of a random
palette selection.

Requested full-pool consumers include Hula, Chunchun variants, Wild Mouse's
palette branch, What The Helix, Vert Streamer, Wobbling Dish, Saddle, and
Sine Wave. Each retains its shape/solid/no-solid choices. The rocking Sine
Wave now receives the same selected shader and palette as the main version.

`CUBINA_LOG_ANIMATION_COLORS=1` enables selection-time logs such as:

```text
Colors | Bouncing Ball | object 3 | Noise Field | ID 43 Yelblu
```

`PaletteDescription` is copied into `TexturePlan`, so a later object's random
choice does not overwrite the earlier object's identity. Some self-managed
scenes report fixed schemes or several later per-object selections. Log when
choosing a material, not per voxel/frame. The shared palette-name buffer still
supports legacy initial headers; it alone cannot describe multiple objects.

The disabled Palette Audition card iterates all 49 entries in catalog order
as a full Box with Linear Flow, holds until Next, and prints position/ID/name.
Re-enable its rule in TestDemo for another curation session. The ordinary Box
rotation has nine brush slots, including Soap and Waving Cell, distributed
through shuffled playback or stepped within ordered playback.
