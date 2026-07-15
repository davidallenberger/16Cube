#include <Arduino.h>
#include <stdint.h>
#include "CubeCore.h"
#include "BluetoothManager.h"
#include "TestDemo.h"

#include "Planes.h"
#include "Letters.h"
#include "Icons.h"
#include "Rubiks.h" 
#include "GeometryEngine.h"
#include "TextureEngine.h"
#include "Tetris.h"
#include "Shapes.h"
#include "Surfaces.h"
#include "Streamers.h"
#include "CubeSystem.h"

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println("Cube mapping test (I2S + gaps + sacrificial) with debug");

  // This handles FastLED.addLeds, brightness, clearAll, show, and seeding
  initCube();

  // Configuration of our state machine container
  CubeSystem::setup();
    
  Serial.println("setup done");
}

uint32_t globalPhase = 0;
float currentRadius = 0.0f;
float radiusDir = 0.5f; 
uint32_t lastRadiusUpdate = 0;
boolean runOnce = true;

void loop() 
{ 
// 1. THE CLASSIC SEQUENCE
// 10 Seconds. 1.0x Speed.
// The SDF math guarantees the helix fills the maximum volume and 
// uses anti-aliasing to look like a perfectly smooth, physical pipe.
Streamers::animateDNASpiral(10000, RainbowColors_p, .75f);
Streamers::animateDNASpiral(10000, RainbowColors_p, .85f);
Streamers::animateDNASpiral(10000, RainbowColors_p, 1.0f);

return;
// 1. THE DISTORTED SOLID BOX
// 10 Seconds. A perfectly static, solid 3D box. The physical geometry 
// remains completely still while the mathematical Distortion Waves 
// ripple through its ambient volume, making the box look alive.
Shapes::animateStatic(
    SHAPE_BOX,              // Physical geometry
    10000,                  // Duration (10 seconds)
    RENDER_SOLID,           // Solid structural fill
    RainbowColors_p,          // Palette for the interference mapping
    MODE_DISTORTION_WAVES,  // The new 3D shader brush
    25,                     // Speed (Irrelevant for static motion)
    false                   // No bounds stretching
);

// 2. THE LIQUID SINE WAVE
// 10 Seconds. Layering the undulating physical geometry of the sine wave 
// with the secondary flowing math of the Distortion Waves shader creates 
// an intensely complex, hyper-fluid visual.
/*
Surfaces::animateSineWaveSplit2026(
    10000, 
    360, 
    (SplitWaveMode2026)0, 
    RainbowColors_p, 
    MODE_DISTORTION_WAVES, 
    0.48f
);*/


return;
Streamers::animateColorTwinkles(10000);
return;
// 1. THE VOLUMETRIC CORE
// 10 Seconds. Medium speed. Graceful architectural sweeping.
Streamers::animateColoredBursts(10000, 96);

return;
// 1. THE LASSOED SERPENTINE (The 3D Matrix Scanner)
// 10 Seconds. Gap size locked down to 32 as requested. 
// Tightly groups the flock so they scan through the literal rows 
// of the matrix like a cohesive glowing entity.
Streamers::animateChunchunVolumetric(10000, PartyColors_p, 128, 32, CHUNCHUN_SERPENTINE);  //<--this setting works

// 2. THE LISSAJOUS WEAVE (The Rubber Band)
// 10 Seconds. Gap size 128 (Mid). 
// The flock strings out beautifully into a long, twisting ribbon 
// that sweeps the dead center of the room and banks off the walls.
Streamers::animateChunchunVolumetric(10000, RainbowColors_p, 128, 128, CHUNCHUN_LISSAJOUS); //<--this works


  // 1. THE CHECKERBOARD PISTON
// 10 Seconds. (Your previous view). The interlocking 2D scatter mapped to Z-columns.
Streamers::animateBpmVolumetric(10000, RainbowColors_p, 64, BPM_CHECKERBOARD); //<--the only one I like

// 2. THE SPHERICAL HEARTBEAT
// 10 Seconds. 3D shells of light spawn at the core and push out to the glass.
// Uses PartyColors_p and a slightly faster speed (90).
Streamers::animateBpmVolumetric(10000, PartyColors_p, 90, BPM_SPHERE);

// 3. THE DIAGONAL SLICER
// 10 Seconds. Solid, 45-degree angled planes washing through the cube.
// Uses OceanColors_p and a slow speed (40) for a sweeping sonar feel.
Streamers::animateBpmVolumetric(10000, OceanColors_p, 40, BPM_DIAGONAL);

// 4. THE TWISTING HELIX
// 10 Seconds. A solid spiral staircase of light threading up and down the Z-axis.
// Uses LavaColors_p at default speed (64) for a twisting thermal vortex.
Streamers::animateBpmVolumetric(10000, LavaColors_p, 64, BPM_HELIX);


// 1. THE CLASSIC JUGGLER (Rainbow Default)
// 10 Seconds. 128 Gravity, 128 Count. Every column behaves like a popcorn 
// popper full of tracking, independent neon balls jumping and crossing paths.
Streamers::animateBouncingBalls(10000);

// 2. THE DEEP SEA POPPER (Ocean Palette)
// 10 Seconds. Beautiful, cool-toned tracking. Because colors are fixed to 
// the balls, you can perfectly track the teal ball passing through the blue ball.
Streamers::animateBouncingBalls(10000, OceanColors_p);

// 4. THE MANIC PARTY (Party Palette)
// 10 Seconds. Cranks the ball count to 220 and the gravity/speed to 200. 
// Nearly maximizes the memory arena for a frantic, high-density kinetic explosion.
Streamers::animateBouncingBalls(10000, PartyColors_p, 200, 220);

  //xxx choose from one of these with random colors palette
// 1. THE CLASSIC LAVA LAMP
// 15 seconds. Uses default LavaColors_p. Smooth, warm red/orange 
// blobs that squish against the glass and seamlessly mix colors when they merge.
Streamers::animateLavaLamp(10000);// <--workable

// 2. THE ALIEN INCUBATOR (Acid Colors)
// 15 seconds. Fills 20% of the cube. Uses PartyColors_p for high-contrast neon 
// blobs (Cyan, Magenta, Yellow) that dynamically blend into wild secondary 
// colors (e.g. Purple, Green) as their energy fields intersect mid-air.
Streamers::animateLavaLamp(10000, 1.0f, 0.20f, PartyColors_p); // <--workable

// 3. RAPID BOILING (Fast Physics)
// 15 seconds. Runs at 2.5x speed multiplier and drops the volume to 10% 
// to create small, highly kinetic droplets that shoot up, bounce off 
// the ceiling, and violently merge on the way down.
Streamers::animateLavaLamp(10000, 2.5f, 0.10f, RainbowColors_p); //<--use this and kill the bouncing ball

// 4. THE DEEP FREEZE
// 15 seconds. Very slow movement (0.3x speed). Massive volume (20%).
// Creates huge, sluggish clouds of deep oceanic blues and greens that barely 
// separate from each other, simulating extreme viscosity.
Streamers::animateLavaLamp(10000, 0.3f, 0.20f, OceanColors_p); // <--workable



// 1. THE SATURN RING (Sync)
// Runs for 10 seconds. All stars are locked to the exact same tumbling 
// 3D plane, creating a majestic, unified ring system. 
// Uses the default Rainbow palette and Linear Flow.
Streamers::animateBlackHole(10000);

  
  CubeSystem::loop();
    return;
  
    //bukkake edit

  /*
    For each shape, you must have motion: either the texture or an expandcollapse, tumble or spin.  
    Not all combinations will be interesting for the given shape.
    For some shapes there are special color combinations that are interesting.
    Shell and Solid always make sense, but wireframe only makes sense for some shapes.
  */
// ==============================================================================
// THE V2 SHADER TEST SUITE (All Solid Boxes)
// ============================================================================== 
// UNIFORM BRUSH: 
// Behavior: The entire volume is one solid color, shifting smoothly over time given the constraining set of a palette passed.
// Was color_cycle
// New functionality is any palette as the constraining set (not interesting), so just use rainbowcolors
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, RainbowColors_p, MODE_UNIFORM_CYCLE);

// STATIC GRADIENT BRUSH: 
// Behavior: A frozen 3D heatmap. Space dictates color, ignoring time.  The Party palette or equivalent is stretched across the X, Y, and Z axes. Because 
// time is ignored in this math, the box acts as a frozen, permanently 3D-painted heatmap.
// Was originally the rainbow_spread and it can work with any palette now
// New functionality is using any palette for a spread rather than just rainbow_spread
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, PartyColors_p, MODE_SPATIAL_GRADIENT); //uses the palette to vary over space - would pair with shape movement only
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID,  RainbowColors_p, MODE_SPATIAL_GRADIENT); //uses the palette to vary over space - would pair with shape movement only

// THE LINEAR SPACE + TIME BRUSH
// Behavior: The Lava palette is stretched across the X, Y, and Z axes, but the clock 
// pushes the phase forward. You will see diagonal, flat walls of fire washing through the box.
// Was rainbow cycle
// New functionality is any palette can be spread across the shape and it will cycle over time
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, RainbowColors_p, MODE_LINEAR_FLOW);
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, LavaColors_p, MODE_LINEAR_FLOW);
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, ForestColors_p, MODE_LINEAR_FLOW);
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, OceanColors_p, MODE_LINEAR_FLOW);

// THE ORGANIC NOISE BRUSH
// Behavior: 3D Perlin math calculates "clouds" inside the volume. The Forest palette 
// is applied to the density of the noise, creating boiling, shifting pockets of green and brown.
// Was BLENDS and used to work only with lava & ocean
// There is NOT new functionality here conceptually, but I amped up the algorithm to turn the moving palette into a boiling ocean palette in 3d more strongly
//It looks alot like metaballs but more energetic
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, RainbowColors_p, MODE_NOISE_FIELD);
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, LavaColors_p, MODE_NOISE_FIELD);
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, ForestColors_p, MODE_NOISE_FIELD);
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, OceanColors_p, MODE_NOISE_FIELD);

// THE PROXIMITY BRUSH (Distance Fields)
// Behavior: The math evaluates distance from invisible orbiting centers. You will see 
// glowing white spheres (lava lamps) moving through the box, with the empty space fading into Cloud blue.
// Was metaballs
// New functionality is to be able to give a general palette to metaballs to operate - 0 speedMs makes it go at a reasonable speed
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, PartyColors_p, MODE_DISTANCE_FIELD, 0);
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, RainbowColors_p, MODE_DISTANCE_FIELD, 0);
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, LavaColors_p, MODE_DISTANCE_FIELD, 0);
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, OceanColors_p, MODE_DISTANCE_FIELD, 0);
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, ForestColors_p, MODE_NOISE_FIELD, 0);

// THE SCATTER BRUSH (Palette Variance)
// Behavior: Hash math assigns a mathematically random number to every single voxel. 
// The box looks like a dense blizzard of TV static flickering through all the palette's colors.
// Was the RANDOM (digital glitter/confetti)
// No new function - worked with any palette include solid colors - but truth is that this is lame with a solid color
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, RainbowColors_p, MODE_SCATTER);
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, ForestColors_p, MODE_SCATTER);
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, LavaColors_p, MODE_SCATTER);
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, OceanColors_p, MODE_SCATTER);

// THE SELF-CONTAINED BRUSH
// Behavior: Proves the engine can safely ignore "Paint". Even though we pass CRGB::Black 
// to satisfy the signature, the Hiphotic math overrides it and natively generates its 
// own high-contrast interference patterns.
Shapes::animateStatic(SHAPE_BOX, 10000, RENDER_SOLID, MODE_HIPHOTIC);
return;
Shapes::animateExpandingCollapsing(SHAPE_BOX, 5000, RENDER_WIREFRAME, CRGB::Green, 25, true);
Shapes::animateExpandingCollapsing(SHAPE_BOX, 5000, RENDER_SOLID, PartyColors_p, MODE_UNIFORM_CYCLE, 25, true);
  
  // Specialty Box Movements
Shapes::animateFlyingBox(5000,  10);

Shapes::animateMovingBoxes(15000, 15);  

// 5. The Elevator Block (A solid cube sliding through the floor
Shapes::animateSpring(SHAPE_BOX, 5000, RENDER_SOLID, LavaColors_p, MODE_NOISE_FIELD, 25);

#ifndef TARGET_2026_CUBOID
  //xxx kind of a whatever Shapes::animatePulseBoxSequence(4, false, RENDER_SHELL, TEX_RANDOM, PartyColors_p,15); 
#endif

  // Tumbling/spinning not interesting or relevant
  // ==========================================
  // 2. SPHERES
  // ==========================================
  // Static with Flowing Textures
  Shapes::animateStatic(SHAPE_SPHERE, 5000, RENDER_SOLID, MODE_HIPHOTIC);
  Shapes::animateStatic(SHAPE_SPHERE, 5000, RENDER_SHELL, PartyColors_p, MODE_LINEAR_FLOW);
  
  // Movement with Solid Colors (Tumbling a sphere is redundant, using Expand)
  Shapes::animateExpandingCollapsing(SHAPE_SPHERE, 5000, RENDER_SOLID, PartyColors_p, MODE_UNIFORM_CYCLE, 30);  
  Shapes::animateExpandingCollapsing(SHAPE_SPHERE, 5000, RENDER_SOLID, PartyColors_p, MODE_LINEAR_FLOW, 30);
  Shapes::animateExpandingCollapsing(SHAPE_SPHERE, 5000, RENDER_SHELL, MODE_HIPHOTIC, 10);
  Shapes::animateExpandingCollapsing(SHAPE_SPHERE, 5000, RENDER_SOLID, PartyColors_p, MODE_NOISE_FIELD, 20);
  // 1. The Perlin Cloud (MODE_NOISE_FIELD) The solid ball is painted with 3D noise. As it rolls, organic clouds of rainbow colors boil and shift across its surface.
  Shapes::animateRollingBall(5000, RENDER_SOLID, RainbowColors_p, MODE_NOISE_FIELD);
  // 2. The Digital Glitter (MODE_SCATTER) The solid ball is covered in rapidly shifting, random digital hash noise, sparkling exclusively in deep ocean blues and greens.
  Shapes::animateRollingBall(5000, RENDER_SOLID, OceanColors_p, MODE_SCATTER);
  // 3. The Generative Math Core (MODE_HIPHOTIC) The ball is driven by complex, self-contained sine/cosine math, creating trippy, organic patterns that evolve internally as it rolls.
  Shapes::animateRollingBall(5000, RENDER_SOLID, MODE_HIPHOTIC);
  // 4. The Cycling Shell (RENDER_SHELL with MODE_UNIFORM_CYCLE) A hollow ball rolling around, with its outer surface smoothly pulsing as one uniform color through the neon party palette.
  Shapes::animateRollingBall(5000, RENDER_SHELL, PartyColors_p, MODE_UNIFORM_CYCLE);  // Tumbling, spinning not interesting or relevant
  // wireframe not relevant.

  // ==========================================
  // 3. OVOIDS (The Pill)
  // ==========================================
  // Static with Flowing Textures
  //qqq BORING Shapes::animateStatic(SHAPE_OVOID, 5000, RENDER_SHELL, TEX_BLENDS, OceanColors_p); 
  Shapes::animateExpandingCollapsing(SHAPE_OVOID, 5000, RENDER_SHELL, LavaColors_p, MODE_NOISE_FIELD, 25, true);
  // Movement with Solid Colors (Ovoids tumble beautifully)
  Shapes::animateTumbling(SHAPE_OVOID, 5000, RENDER_SOLID, CRGB::Blue, 1.0f, false);

  // spinning and wireframe not relevant.

  // ==========================================
  // 4. TORUS (The Donut)
  // ==========================================
  // Static with Flowing Textures
  //xxx BORING Shapes::animateStatic(SHAPE_TORUS, 5000, RENDER_SOLID, TEX_RAINBOW_CYCLE);
  // Movement with Textures & Solid Colors
  //qqq BORING: Shapes::animateExpandingCollapsing(SHAPE_TORUS, 5000, RENDER_SOLID, TEX_HIPHOTIC, 30);
  //qqq donut stretch not interesting
  Shapes::animateTumbling(SHAPE_TORUS, 5000, RENDER_WIREFRAME, LavaColors_p, MODE_NOISE_FIELD, 1.5f);
  Shapes::animateSpinning(SHAPE_TORUS, 5000, 2.0f, RENDER_SOLID, CRGB::Orange);

  // ==========================================
  // 5. TETRAHEDRON (The Pyramid)
  // ==========================================
  // Static with Flowing Textures
  //qqq BORING Shapes::animateStatic(SHAPE_TETRAHEDRON, 5000, RENDER_WIREFRAME, TEX_BLENDS, OceanColors_p); 
  
  // Movement with Textures & Solid Colors
  Shapes::animateTumbling(SHAPE_TETRAHEDRON, 5000, RENDER_SHELL, PartyColors_p, MODE_LINEAR_FLOW, 1.5f, true);
  Shapes::animateExpandingCollapsing(SHAPE_TETRAHEDRON, 5000, RENDER_SOLID, CRGB::Purple, 25, true);
  Shapes::animateExpandingCollapsing(SHAPE_TETRAHEDRON, 5000, RENDER_SHELL, MODE_HIPHOTIC, 15);
  Shapes::animateTumbling(SHAPE_TETRAHEDRON, 5000, RENDER_SHELL, MODE_HIPHOTIC, 2.5f);
  // 7. Sweeping Pyramid (A hollow triangular point piercing the floor)
Shapes::animateSpring(SHAPE_TETRAHEDRON, 5000, RENDER_SOLID, CRGB::Cyan, 25);

#ifndef TARGET_2026_CUBOID
  Shapes::animateExpandingCollapsing(SHAPE_TETRAHEDRON, 5000, RENDER_WIREFRAME, OceanColors_p, MODE_NOISE_FIELD, 15);
#endif

  // ==========================================
  // 6. OCTAHEDRON (The Diamond)
  // ==========================================
  // Static with Flowing Textures
  Shapes::animateStatic(SHAPE_OCTAHEDRON, 5000, RENDER_SHELL, MODE_HIPHOTIC);
  // Movement with Textures & Solid Colors
  Shapes::animateSpinning(SHAPE_OCTAHEDRON, 5000, 2.0f, RENDER_SOLID, LavaColors_p, MODE_NOISE_FIELD, true);
  Shapes::animateExpandingCollapsing(SHAPE_OCTAHEDRON, 5000, RENDER_SOLID, CRGB::Green, 25, true);
  Shapes::animateExpandingCollapsing(SHAPE_OCTAHEDRON, 5000, RENDER_SOLID, MODE_HIPHOTIC);
  Shapes::animateTumbling(SHAPE_OCTAHEDRON, 5000, RENDER_SHELL, PartyColors_p, MODE_LINEAR_FLOW, 0.5f, true);
  // The Special Blue/Gold Split (Triggered by White)
  Shapes::animateTumbling(SHAPE_OCTAHEDRON, 5000, RENDER_SOLID, CRGB::Gray, 1.0f, true);

  // ==========================================
  // 6b. MERKABA
  // ==========================================
  Shapes::animateExpandingCollapsing(SHAPE_MERKABA, 5000, RENDER_SOLID, CRGB::White); // Blue/Gold hook
  Shapes::animateExpandingCollapsing(SHAPE_MERKABA, 5000, RENDER_SHELL, OceanColors_p, MODE_NOISE_FIELD);
  Shapes::animateExpandingCollapsing(SHAPE_MERKABA, 5000, RENDER_SHELL, PartyColors_p, MODE_SPATIAL_GRADIENT);
  Shapes::animateStatic(SHAPE_MERKABA, 5000, RENDER_SHELL, PartyColors_p, MODE_SCATTER);
  Shapes::animateStatic(SHAPE_MERKABA, 5000, RENDER_SOLID, MODE_HIPHOTIC);
  Shapes::animateTumbling(SHAPE_MERKABA, 5000, RENDER_SHELL, CRGB::Gray); //special multicolor hook
  // 6. Sacred Geometry Scan (Wireframe star tetrahedron intersecting the room)
  Shapes::animateSpring(SHAPE_MERKABA, 5000, RENDER_SOLID, RainbowColors_p, MODE_UNIFORM_CYCLE, 25);

  // ==========================================
  // 7. CYLINDER (The Pillar)
  // ==========================================
  // Static with Flowing Textures
  Shapes::animateStatic(SHAPE_CYLINDER, 5000, RENDER_SHELL, PartyColors_p, MODE_DISTANCE_FIELD);
  // Movement with Textures & Solid Colors
  //qqq looks stupid Shapes::animateExpandingCollapsing(SHAPE_CYLINDER, 5000, RENDER_SHELL, TEX_RANDOM, RainbowColors_p, 10);
  Shapes::animateTumbling(SHAPE_CYLINDER, 5000, RENDER_SOLID, CRGB::Cyan, 2.0f, false);

  // ==========================================
  // 8. HEMISPHERES (The Hourglass)
  // ==========================================
  // Static with Flowing Textures
  Shapes::animateStatic(SHAPE_HEMISPHERES, 5000, RENDER_SHELL, OceanColors_p, MODE_NOISE_FIELD);
  // Movement with Textures & Solid Colors
  //qqq BORING Shapes::animateExpandingCollapsing(SHAPE_HEMISPHERES, 5000, RENDER_SHELL, TEX_HIPHOTIC, 25);
  Shapes::animateTumbling(SHAPE_HEMISPHERES, 5000, RENDER_SOLID, CRGB::Magenta, 1.5f);

  
  //A bunch of vertical oriented winds, lines etc.
  // 1. The Classic Storm (Ocean colors, starts at 1.0 RPS, accelerates to 3.5 RPS)
  Streamers::animateTornado(5000, 25, OceanColors_p, 1.0f);
  // 2. Fire Whirl (Lava colors, starts violently fast at 2.5 RPS, accelerates to 8.75 RPS)
  Streamers::animateTornado(5000, 30, LavaColors_p, 2.5f);
  // 3. Magic Funnel (Rainbow cycling, slow, majestic wind-up)
  Streamers::animateTornado(5000, 25, RainbowColors_p, 0.5f);
  // 1.5 revs per second (The speed you were seeing)
  Streamers::animateHula(5000, 20, RainbowColors_p, 3.5f);
  // 4.0 revs per second (Aggressive, rapid spinning)
  Streamers::animateHula(5000, 20, LavaColors_p, 4.0f);
  // 8.0 revs per second (Helicopter mode)
  Streamers::animateHula(5000, 20, OceanColors_p, 3.0f);
  // Classic 2-streamer cyclone using 4% volume and the Rainbow cycle
  Streamers::animateCyclone(5000, 2, 0.04f, RainbowColors_p, MODE_UNIFORM_CYCLE);
  // A denser 4-streamer vortex using 3% volume and the Lava cycle
  Streamers::animateCyclone(5000, 3, 0.03f, LavaColors_p, MODE_UNIFORM_CYCLE); 
  // 1. VERTICAL STREAMER: Consumes 6.25% of the cube volume, wandering randomly
  Streamers::animateVerticalStreamer(5000, 5, 0.1875f, RainbowColors_p, MODE_UNIFORM_CYCLE);
  Streamers::animateVerticalPipes(5000, 30);

  // Version of rain
  // Uses default fallSpeed (0.4) and returnSpeed (0.8) for that fast "zoom" back up.
  Streamers::animateRandomFall(10000);
  Streamers::animateRain(5000, 2, 0.6f, PartyColors_p, MODE_SCATTER);
  // Classic solid rain in a beautiful deep blue
  Streamers::animateRain(5000, 2, 0.6f, CRGB::Blue);
  // Acid rain: constantly cycling through neon party colors
  Streamers::animateRain(5000, 3, 0.8f, PartyColors_p, MODE_UNIFORM_CYCLE);
  // Lava rain: Heavy, slower drops drawn using the noise engine
  Streamers::animateRain(5000, 1, 0.3f, LavaColors_p, MODE_NOISE_FIELD);
  
  //Misc
  // Scurries through a shifting rainbow space
  Streamers::animateWildMouse(5000, 30);
  Streamers::animateFireworks(10000);

 
  // ==========================================
  // PLANES
  // ==========================================
  Planes::animateHingeDance(30, 5);  
  Planes::animateSplat(5);
  Planes::animateBounce(5,10);  

  // ==========================================
  // HYPERBOLIC PARABOLOID (Saddle)
  // ==========================================
  Surfaces::animateHyperbolicParaboloid(5000, 2.5f, 4.0f, 1.5f, OceanColors_p, MODE_NOISE_FIELD);
  Surfaces::animateHyperbolicParaboloid(5000, 4.0f, AXIS_X, CRGB::Red); 
  Surfaces::animateHyperbolicParaboloid(5000, -4.0f, AXIS_Z, MODE_HIPHOTIC);
  Surfaces::animateHyperbolicParaboloid(5000, 1.0f, 0.0f, 5.0f, MODE_HIPHOTIC);
 
  // ==========================================
  // SINE WAVES 
  // ==========================================
  #ifndef TARGET_2026_CUBOID
  Surfaces::animateSineWave(5000, AXIS_Y, WAVE_RADIAL, 360, MODE_SOLID);                 
  Surfaces::animateSineWave(5000, AXIS_Y, WAVE_RADIAL, 360, LavaColors_p, MODE_NOISE_FIELD);  
  Surfaces::animateSineWave(5000, AXIS_X, WAVE_RADIAL, 360, PartyColors_p, MODE_SCATTER); 
  Surfaces::animateSineWave(5000, AXIS_Z, WAVE_RADIAL, 360, MODE_LINEAR_FLOW);        
  Surfaces::animateSineWave(5000, AXIS_Y, WAVE_RADIAL, 360, OceanColors_p, MODE_NOISE_FIELD); 
  Surfaces::animateSineWave(5000, AXIS_Z, WAVE_RADIAL, 360, LavaColors_p, MODE_SCATTER); 
  Surfaces::animateSineWave(5000, AXIS_Z, WAVE_RADIAL, 720, MODE_LINEAR_FLOW);
#endif
  
#ifdef TARGET_2026_CUBOID
  Surfaces::animateSineWaveSplit2026(5000, 360, SPLIT_2026_MIRRORED_Z, MODE_HIPHOTIC);  
  Surfaces::animateSineWaveSplit2026(5000, 360, SPLIT_2026_OPPOSING_X, MODE_NOISE_FIELD);  
  Surfaces::animateSineWaveSplit2026(5000, 360, SPLIT_2026_OPPOSING_Y, MODE_SPATIAL_GRADIENT);  
  Surfaces::animateSineWaveSplit2026(5000, 360, SPLIT_2026_RANDOM, CRGB::DeepSkyBlue);
#endif
starthere:
  /* =================================================================================
   * THE LED LEGIBILITY CHEAT SHEET (70% BG Dimming)
   * ---------------------------------------------------------------------------------
   * 1. Luma vs Chaos: White, Yellow, Cyan -> on -> Hiphotic, Rainbow, or Random
   * (High diode-power cuts through shifting, full-spectrum noise)
   * 2. Cool vs Warm:  Cyan, DeepSkyBlue   -> on -> Lava (TEX_BLENDS)
   * (Direct complementary contrast against deep reds and oranges)
   * 3. Warm vs Cool:  Gold, OrangeRed     -> on -> Ocean (TEX_BLENDS)
   * (Warm colors "advance" while cool backgrounds visually "recede")
   * 4. Missing Color: Magenta, HotPink    -> on -> Ocean or Forest
   * (Injects pure red light into palettes that contain zero red)
   * 5. Dual Shaders:  Warm Palette (FG)   -> on -> Cool Palette (BG)
   * (Text and background both boil independently, separated by temperature)
   * ================================================================================= */
  // ==========================================
  // LETTERS AND ICONS
  // ==========================================
  Letters::scrollTextTimed("MATH `28`", 7000, 1.5f, 1.5f, 55, CRGB::White, MODE_HIPHOTIC);
  Letters::scrollTextTimed("HOT `1`", 7000, 1.5f, 1.5f, 55, CRGB::Cyan, LavaColors_p, MODE_NOISE_FIELD);
  Letters::scrollTextTimed("WAVE `C27`", 7000, 1.5f, 1.5f, 55, CRGB::Magenta, OceanColors_p, MODE_NOISE_FIELD);
  Letters::scrollTextTimed("FIRE & ICE", 7000, 1.5f, 1.5f, 55, LavaColors_p, MODE_NOISE_FIELD, OceanColors_p, MODE_NOISE_FIELD);
  Letters::scrollTextTimed("NEBULA `12`", 7000, 1.5f, 1.5f, 55, CRGB::Green, PartyColors_p, MODE_SCATTER);
  Letters::scrollTextCycles("MIND`28`BLOWN", 2, 2.0f, 2.0f, 55, CRGB::Black, MODE_HIPHOTIC, true, 0);
  Letters::scrollTextCycles("PACMAN `(1.2.3)` ROCKS", 1, 1.5f, 1.5f, 55, CRGB::White, MODE_HIPHOTIC, false);
  Letters::scrollTextTimed("CHILL `28` OUT", 15000, 1.0f, 1.0f, 55, CRGB::White, LavaColors_p, MODE_NOISE_FIELD, false, 60);
  Letters::scrollTextCycles("WELCOME TO CAMP", 3, 1.5f, 1.5f, 55, PartyColors_p, MODE_LINEAR_FLOW, CRGB::Black);

  Letters::scrollTextCycles("Fuck Yer `27` Burn", 2, 1.0f, 1.0f, 55, CRGB::White, LavaColors_p, MODE_NOISE_FIELD, true, 0);
  //Kinky pizza
//Cubina
  Icons::scrollIcons("{(1.2.3),(4.5.6),(7.8.9.10),(11.12.13.14),(15.16.17.18),(19.20),21,22,23,24,25,26,27,28,29,30,31,32}", 10000, 55, 120, false);
  Icons::scrollIcons("{(1.2.3),(4.5.6),(7.8.9.10),(11.12.13.14),(15.16.17.18),(19.20),21,22,23,24,25,26,27,28,29,30,31,32}", 10000, 55, 120, true);

  Icons::scrollIcons("31,32,31,32", 7000, 55, 120, false);
  Icons::scrollIcons("(1.2.3),(4.5.6),C27,28", 7000, 55, 120, false);
  Icons::scrollIcons("(1.2.3),(4.5.6),C27,28", 7000, 55, 120, true);
  
#ifndef TARGET_2026_CUBOID
  Tetris game;
  game.animate(180, Tetris::SPEED_SUPERHUMAN); 
  Rubiks::runScrambleAndSolveShow();
#endif
}

//BORING
/*
 // 1. The Classic Diamond Wave (Octahedron passing through the glass)
// Shapes::animateSpring(SHAPE_OCTAHEDRON, 5000, RENDER_SOLID, TEX_COLOR_CYCLE, PartyColors_p, 25);
// 2. Liquid Bubble (A hollow sphere rising and falling)
// Shapes::animateSpring(SHAPE_SPHERE, 5000, RENDER_SOLID, TEX_BLENDS, OceanColors_p, 25);
// 3. Glowing Laser Scanner (A wireframe ring scanning up and down)
// Shapes::animateSpring(SHAPE_TORUS, 5000, RENDER_SOLID, TEX_SOLID, CRGB::Red, 25);
// 4. Stretched Power Pillar (Cylinder stretched to fill the vertical space)
// Shapes::animateSpring(SHAPE_CYLINDER, 5000, RENDER_SOLID, TEX_SOLID, CRGB::Green, 25, true);
// 8. Droplet / Egg (An elongated oval passing smoothly through)
// Shapes::animateSpring(SHAPE_OVOID, 5000, RENDER_SOLID, TEX_RAINBOW_CYCLE, RainbowColors_p, 25, true);
// 9. The Hourglass (Two opposing domes sweeping the room)
// Shapes::animateSpring(SHAPE_HEMISPHERES, 5000, RENDER_SOLID, TEX_COLOR_CYCLE, PartyColors_p, 25);
*/