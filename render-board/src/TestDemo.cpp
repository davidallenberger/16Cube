#include "TestDemo.h"
#include "CubeCore.h"
#include "SpacerCheck.h"
#include "Balls.h"
#include "Boxes.h"
#include "Patterns.h"
#include "Swirls.h"
#include "Explosions.h"
#include "EyeOfSauron.h"
#include "Flows.h"
#include "Planes.h"
#include "Letters.h"
#include "Icons.h"
#include "Games/Rubiks.h"
#include "GeometryEngine.h"
#include "TextureEngine.h"
#include "Games/Tetris.h"
#include "Shapes.h"
#include "Surfaces.h"
#include "Streamers.h"

// =========================================================================================
// ⚙️ INTERNAL ENGINE STATE & DICTIONARIES (Completely Hidden from main.cpp)
// =========================================================================================
namespace {
    const float SLOW_10 = 1.10f;
    const float SLOW_20 = 1.20f;
    const float SLOW_25 = 1.25f;
    const float SLOW_30 = 1.30f;
    const float SLOW_33 = 1.33f;
    const float SLOW_40 = 1.40f;
    const float SLOW_50 = 1.50f;

    #ifdef HARDWARE_BURNCUBE
        const float SCALE_MS   = 2.0f;  
        const float SCALE_RATE = 0.5f;  
    #else
        const float SCALE_MS   = 1.0f;  
        const float SCALE_RATE = 1.0f;  
    #endif

    const uint32_t SPEED_MOTION_STATIC          = (uint32_t)(25 * SCALE_MS); 
    const uint32_t SPEED_MOTION_EXPAND_COLLAPSE = (uint32_t)(50 * SCALE_MS); 
    const uint32_t SPEED_MOTION_SPRING          = (uint32_t)(50 * SCALE_MS); 
    const float    SPEED_MOTION_TUMBLING        = (1.0f * SCALE_RATE); 
    const float    SPEED_MOTION_SPINNING        = (2.2f * SCALE_RATE); 

    const uint32_t SPEED_SCENE_HINGE_DANCE      = (uint32_t)(22 * SCALE_MS);  
    const uint32_t SPEED_SCENE_SPLAT            = (uint32_t)(19 * SCALE_MS);  
    const uint32_t SPEED_SCENE_BOUNCE           = (uint32_t)(37 * SCALE_MS);  
    const float    SPEED_SCENE_SADDLE           = (2.33f * SCALE_RATE); 
    const float    SPEED_SCENE_SINE_WAVE        = (0.48f * SCALE_RATE);
    const float    SPEED_SCENE_RAIN             = (0.58f * SCALE_RATE); 
    const float    SPEED_SCENE_CYCLONE          = (0.60f * SCALE_RATE);
    const uint32_t SPEED_SCENE_VERT_STREAMER    = (uint32_t)(25 * SCALE_MS);   
    const float    SPEED_SCENE_TORNADO          = (2.0f * SCALE_RATE)/SLOW_25; 
    const float    SPEED_SCENE_HULA             = (2.33f * SCALE_RATE)/SLOW_25; 
    const float    SPEED_SCENE_RANDOM_FALL      = (0.8f * SCALE_RATE); 
    const uint32_t SPEED_SCENE_PIPES            = (uint32_t)(15 * SCALE_MS);   
    const uint32_t SPEED_SCENE_WILD_MOUSE       = (uint32_t)(25 * SCALE_MS);   
    const uint32_t SPEED_SCENE_WOBBLING_DISH    = (45.0f); //going to take a risk that we shouldn't slow down the wobbling dish to preserve the effect  
    const uint32_t SPEED_SCENE_HELIX            = (uint32_t)(50 * SCALE_MS);  
    const uint32_t SPEED_SCENE_FLYING_BOX       = (uint32_t)(25 * SCALE_MS);  
    const uint32_t SPEED_SCENE_MOVING_BOXES     = (uint32_t)(40 * SCALE_MS);  
    const uint32_t SPEED_SCENE_ROLLING_BALL     = (uint32_t)(25 * SCALE_MS);   
    const uint32_t SPEED_SCENE_BOUNCING_BALL    = (uint32_t)(25 * SCALE_MS);   
    const float    SPEED_SCENE_LAVA_LAMP        = (.125f * SCALE_RATE);

    const float   SPEED_SCENE_DNA_SPIRAL       = (0.85f * SCALE_RATE); 
    const uint8_t SPEED_SCENE_DRIFT_ROSE       = (uint8_t)(200 * SCALE_RATE);
    const uint8_t SPEED_SCENE_PINWHEEL_ARMS    = (uint8_t)(32 * SCALE_RATE);
    const uint8_t SPEED_SCENE_VECTOR_ARMS      = (uint8_t)(132 * SCALE_RATE);
    const uint8_t SPEED_SCENE_BPM_CHECKERBOARD = (uint8_t)(64 * SCALE_RATE);  
    const uint8_t SPEED_SCENE_COLOR_TWINKLES   = (uint8_t)(128 * SCALE_RATE); 
    const uint8_t SPEED_SCENE_CHUNCHUN         = (uint8_t)(128 * SCALE_RATE); 
    #ifndef HARDWARE_BURNCUBE
        const uint32_t SPEED_TEXT_ICONS         = (uint32_t)(95 * SCALE_MS);  
    #else
        const uint32_t SPEED_TEXT_ICONS         = (uint32_t)(150);  
    #endif

    enum MotionType { MOTION_STATIC=0, MOTION_EXPAND_COLLAPSE=1, MOTION_SPRING=2, MOTION_TUMBLING=3, MOTION_SPINNING=4 };

    #define MASK_STATIC          (1 << MOTION_STATIC)
    #define MASK_EXPAND_COLLAPSE (1 << MOTION_EXPAND_COLLAPSE)
    #define MASK_SPRING          (1 << MOTION_SPRING)
    #define MASK_TUMBLING        (1 << MOTION_TUMBLING)
    #define MASK_SPINNING        (1 << MOTION_SPINNING)

    #define MASK_SOLID           (1 << RENDER_SOLID)
    #define MASK_SHELL           (1 << RENDER_SHELL)
    #define MASK_WIREFRAME       (1 << RENDER_WIREFRAME)

   enum ConceptType {
        // Shapes
        CONCEPT_SHAPE_BOX = 0, CONCEPT_SHAPE_SPHERE = 1, CONCEPT_SHAPE_OVOID = 2, CONCEPT_SHAPE_TETRAHEDRON = 3,
        CONCEPT_SHAPE_OCTAHEDRON = 4, CONCEPT_SHAPE_MERKABA = 5, CONCEPT_SHAPE_TORUS = 6, CONCEPT_SHAPE_CYLINDER = 7,
        CONCEPT_SHAPE_HEMISPHERES = 8, CONCEPT_SHAPE_HOURGLASS = 9,
        // Scenes
        CONCEPT_SCENE_HINGE_DANCE = 10, CONCEPT_SCENE_SPLAT = 11, CONCEPT_SCENE_BOUNCE = 12, CONCEPT_SCENE_SADDLE = 13,
        CONCEPT_SCENE_SINE_WAVE = 14, CONCEPT_SCENE_RAIN = 15, CONCEPT_SCENE_CYCLONE = 16, CONCEPT_SCENE_VERT_STREAMER = 17,
        CONCEPT_SCENE_TORNADO = 18, CONCEPT_SCENE_HULA = 19, CONCEPT_SCENE_RANDOM_FALL = 20,
        CONCEPT_SCENE_PIPES, CONCEPT_SCENE_WILD_MOUSE, CONCEPT_SCENE_WOBBLING_DISH,
        CONCEPT_SCENE_HELIX, CONCEPT_SCENE_FLYING_BOX, CONCEPT_SCENE_MOVING_BOXES, CONCEPT_SCENE_ROLLING_BALL,
        CONCEPT_SCENE_DNA_SPIRAL, CONCEPT_SCENE_DRIFT_ROSE, CONCEPT_SCENE_COLOR_TWINKLES,
        CONCEPT_SCENE_VECTOR_ARMS, CONCEPT_SCENE_CHUNCHUN_SERP, CONCEPT_SCENE_CHUNCHUN_LISSAJOUS,
        CONCEPT_SCENE_BPM_CHECKERBOARD, CONCEPT_SCENE_BOUNCING_BALLS,
        // Text & Icons
        CONCEPT_SCENE_TEXT_FYB, CONCEPT_SCENE_ICONS_CCW, CONCEPT_SCENE_ICONS_CW, CONCEPT_SCENE_TEXT_PIZZA,
        CONCEPT_SCENE_TEXT_CUBINA, CONCEPT_SCENE_TEXT_YES, CONCEPT_SCENE_TEXT_YEAH, CONCEPT_SCENE_TEXT_ISRAEL,
        CONCEPT_SCENE_RUBIKS, CONCEPT_SCENE_TETRIS, CONCEPT_SCENE_JULIA, CONCEPT_SCENE_ATOMIC_ORBITS,
        CONCEPT_SCENE_TARTAN_OR_PASSING_PLANES,
        CONCEPT_SCENE_COMETS,
        CONCEPT_SCENE_PLANE_TRAILS,
        CONCEPT_SCENE_NOISE_CLOUDS, CONCEPT_SCENE_METEOR,
        CONCEPT_SCENE_SMASHING_PLANES,
        CONCEPT_SCENE_PS_IMPACT,
        CONCEPT_SCENE_PS_GHOST_RIDER,
        CONCEPT_SCENE_PS_BOX,
        CONCEPT_SCENE_ATOM_SMASHER, CONCEPT_SCENE_FIREWORKS,
        CONCEPT_SCENE_LAVA_LAMP,
        CONCEPT_SCENE_PS_SPRAY,
        CONCEPT_SCENE_THE_MATRIX,
        CONCEPT_SCENE_PS_ATTRACTOR, CONCEPT_SCENE_PS_WATERFALL,
        CONCEPT_SCENE_PS_VOLCANO,
        CONCEPT_SCENE_PS_FIRE,
        CONCEPT_SCENE_PINWHEEL_ARMS,
        CONCEPT_SCENE_EYE_OF_SAURON,
        CONCEPT_SCENE_BLACKHOLE,
        CONCEPT_SCENE_PS_GALAXY,
        CONCEPT_SCENE_BOUNCING_BALL,
        CONCEPT_SCENE_PLASMA_ARMS,
        CONCEPT_SCENE_AURORA,
        CONCEPT_SCENE_PALETTE_AUDITION,
        CONCEPT_COUNT
    };

    static const int NUM_SHAPES = 10;
    static const int TOTAL_CONCEPTS = CONCEPT_COUNT;

    // Ordered playback defaults to reading this list from the bottom up.
    // Concept IDs still index the rule and name tables and the shuffled deck.
    constexpr ConceptType orderedConcepts[] = {
        // Disabled auditions
        CONCEPT_SCENE_METEOR,
        CONCEPT_SCENE_NOISE_CLOUDS,
        CONCEPT_SCENE_TEXT_ISRAEL,
        CONCEPT_SCENE_ICONS_CCW,
        CONCEPT_SHAPE_TORUS,
        CONCEPT_SHAPE_OVOID,
        // Marquee
        CONCEPT_SCENE_TEXT_FYB,
        CONCEPT_SCENE_ICONS_CW,
        CONCEPT_SCENE_TEXT_PIZZA,
        CONCEPT_SCENE_TEXT_CUBINA,
        CONCEPT_SCENE_TEXT_YES,
        CONCEPT_SCENE_TEXT_YEAH,
        // Games
        CONCEPT_SCENE_RUBIKS,
        CONCEPT_SCENE_TETRIS,
        // Eye of Sauron
        CONCEPT_SCENE_EYE_OF_SAURON,
        // Flows
        CONCEPT_SCENE_PS_VOLCANO,
        CONCEPT_SCENE_PS_WATERFALL,
        CONCEPT_SCENE_PS_FIRE,
        // Patterns
        CONCEPT_SCENE_THE_MATRIX,
        CONCEPT_SCENE_BPM_CHECKERBOARD,
        CONCEPT_SCENE_RANDOM_FALL,
        CONCEPT_SCENE_RAIN,
        CONCEPT_SCENE_COLOR_TWINKLES,
        // Balls
        CONCEPT_SCENE_LAVA_LAMP,
        CONCEPT_SCENE_BOUNCING_BALL,
        CONCEPT_SCENE_PS_BOX,
        CONCEPT_SCENE_BOUNCING_BALLS,
        CONCEPT_SCENE_ROLLING_BALL,
        // Boxes
        CONCEPT_SCENE_MOVING_BOXES,
        CONCEPT_SCENE_FLYING_BOX,
        // Explosions
        CONCEPT_SCENE_PS_IMPACT,
        CONCEPT_SCENE_FIREWORKS,
        CONCEPT_SCENE_ATOM_SMASHER,
        // Swirls
        CONCEPT_SCENE_ATOMIC_ORBITS,
        CONCEPT_SCENE_PINWHEEL_ARMS,
        CONCEPT_SCENE_TORNADO,
        CONCEPT_SCENE_CYCLONE,
        CONCEPT_SCENE_PS_ATTRACTOR,
        CONCEPT_SCENE_BLACKHOLE,
        CONCEPT_SCENE_PS_GALAXY,
        // Streamers
        CONCEPT_SCENE_PLASMA_ARMS,
        CONCEPT_SCENE_VECTOR_ARMS,
        CONCEPT_SCENE_DRIFT_ROSE,
        CONCEPT_SCENE_COMETS,
        CONCEPT_SCENE_PS_SPRAY,
        CONCEPT_SCENE_WILD_MOUSE,
        CONCEPT_SCENE_PIPES,
        CONCEPT_SCENE_HULA,
        CONCEPT_SCENE_PS_GHOST_RIDER,
        CONCEPT_SCENE_CHUNCHUN_SERP,
        CONCEPT_SCENE_CHUNCHUN_LISSAJOUS,
        CONCEPT_SCENE_DNA_SPIRAL,
        CONCEPT_SCENE_HELIX,
        CONCEPT_SCENE_VERT_STREAMER,
        // Surfaces
        CONCEPT_SCENE_AURORA,
        CONCEPT_SCENE_WOBBLING_DISH,
        CONCEPT_SCENE_JULIA,
        CONCEPT_SCENE_SINE_WAVE,
        CONCEPT_SCENE_SADDLE,
        // Planes
        CONCEPT_SCENE_TARTAN_OR_PASSING_PLANES,
        CONCEPT_SCENE_PLANE_TRAILS,
        CONCEPT_SCENE_SMASHING_PLANES,
        CONCEPT_SCENE_BOUNCE,
        CONCEPT_SCENE_SPLAT,
        CONCEPT_SCENE_HINGE_DANCE,
        // Shapes
        CONCEPT_SHAPE_HOURGLASS,
        CONCEPT_SHAPE_HEMISPHERES,
        CONCEPT_SHAPE_CYLINDER,
        CONCEPT_SHAPE_MERKABA,
        CONCEPT_SHAPE_OCTAHEDRON,
        CONCEPT_SHAPE_TETRAHEDRON,
        CONCEPT_SHAPE_SPHERE,
        CONCEPT_SHAPE_BOX,
        CONCEPT_SCENE_PALETTE_AUDITION,
    };
    static_assert(sizeof(orderedConcepts) / sizeof(orderedConcepts[0]) == TOTAL_CONCEPTS,
                  "Ordered deck must contain every concept");

    struct ShapeRule { bool enabled; uint8_t demoBoost; uint16_t allowedMotions; uint8_t allowedRenders; };
    struct SceneRule { bool enabled; uint8_t demoBoost; };

    // One shuffled Box slot for each texture in its full-volume rotation.
    constexpr ShaderMode boxShaders[] = {MODE_UNIFORM_CYCLE, MODE_LINEAR_FLOW,
        MODE_NOISE_FIELD, MODE_SCATTER, MODE_DISTANCE_FIELD, MODE_HIPHOTIC,
        MODE_DISTORTION_WAVES, MODE_SOAP, MODE_WAVING_CELL};
    constexpr uint8_t boxShaderCount = sizeof(boxShaders) / sizeof(boxShaders[0]);

    const ShapeRule SHAPE_RULES[] = {
        /* BOX         */ { true,  boxShaderCount, MASK_STATIC, MASK_SOLID },
        /* SPHERE      */ { true,  1, MASK_STATIC | MASK_EXPAND_COLLAPSE, MASK_SOLID  },
        /* OVOID       */ { false, 1, MASK_STATIC | MASK_EXPAND_COLLAPSE | MASK_TUMBLING, MASK_SOLID  },
        /* TETRAHEDRON */ { true, 1, MASK_EXPAND_COLLAPSE | MASK_TUMBLING, MASK_SOLID | MASK_WIREFRAME },
        /* OCTAHEDRON  */ { true, 1, MASK_SPINNING, MASK_SOLID },
        /* MERKABA     */ { true,  2, MASK_TUMBLING, MASK_SOLID },
        /* TORUS       */ { false, 1, MASK_TUMBLING | MASK_SPINNING, MASK_SOLID },
        /* CYLINDER    */ { true,  1, MASK_TUMBLING, MASK_SOLID },
        /* HEMISPHERES */ { true, 1, MASK_EXPAND_COLLAPSE | MASK_TUMBLING, MASK_SOLID },
        /* HOURGLASS   */ { true,  1, MASK_TUMBLING, MASK_SOLID }
    };

    const SceneRule SCENE_RULES[] = {
        /* Hinge Dance  */ { true,  2 }, /* Plane Splat  */ { true,  2 }, /* Plane Bounce */ { true,  2 },
        /* Saddle       */ { true,  1 }, /* Sine Wave    */ { true,  3 }, /* Rain         */ { true,  1 },
        /* Cyclone      */ { true,  2 }, /* Vert Stream  */ { true,  1 }, /* Tornado      */ { true,  1 },
        /* Hula         */ { true,  1 }, /* Random Fall  */ { true,  1 },
        /* Vert Pipes   */ { true,  1 }, /* Wild Mouse   */ { true,  1 },
        /* Wobbling Dish*/ { true,  1 }, /* Helix        */ { true,  1 }, /* Flying Box   */ { true,  2 },
        /* Moving Boxes */ { true,  2 }, /* Rolling Ball */ { true,  1 },
        /* DNA Spiral   */ { true,  1 }, /* Drift Rose   */ { true,  1 }, /* Twinkles */     { true,  1 }, /* Vector Arms  */ { true,  1 },
        /* Chunchun S   */ { true,  1 }, /* Chunchun L   */ { true,  1 }, /* BPM Checker  */ { true,  1 }, /* Bounce Ballsz*/ { true,  1 },
        /* Text FYB     */ { true,  1 }, /* Icons CCW    */ { false,  1 }, /* Icons CW     */ { true,  2},
        /* Text Pizza   */ { true,  1 }, /* Text Cubina  */ { true,  1 }, /* Text Just Y. */ { true,  1 },
        /* Text F Yeah  */ { true,  1 }, /* Text Israel  */ { false, 1 }, /* Rubiks       */ { true,  1 },
        /* Tetris       */ { true,  1 }, /* Julia */ { true, 1 }, /* Atomic Orbits */ { true, 1 },
        /* Tartan / Passing Planes */ { true, 1 },
        /* Comets */ { true, 1 },
        /* Plane Trails */ { true, 1 },
        /* Noise Clouds */ { false, 1 }, /* Meteor 3D */ { false, 1 },
        /* Smashing Planes */ { true, 1 }, /* PS Impact */ { true, 1 },
        /* PS Ghost Rider */ { true, 1 },
        /* PS Box */ { true, 1 },
        /* Atom Smasher */ { true, 1 }, /* Fireworks */ { true, 1 },
        /* Lava Lamp */ { true, 2 },
        /* PS Spray */ { true, 1 },
        /* The Matrix */ { true, 1 },
        /* PS Attractor */ { true, 1 }, /* PS Waterfall */ { true, 1 },
        /* PS Volcano */ { true, 1 }, /* PS Fire */ { true, 1 },
        /* Pinwheel Arms */ { true, 1 },
        /* Eye of Sauron */ { true, 1 },
        /* Black Hole */ { true, 1 },
        /* PS Galaxy */ { true, 1 },
        /* Bouncing Ball */ { true, 1 },
        /* Plasma Arms */ { true, 1 },
        /* Aurora */ { true, 1 },
        /* Palette Audition */ { false, 0 } // Enable for a palette review.
    };

    static const char* sceneNames[] = {
        "Box", "Sphere", "Ovoid", "Tetrahedron", "Octahedron", "Merkaba", "Torus", "Cylinder", "Hemispheres", "Hourglass",
        "Hinge Dance", "Splat", "Bounce", "Saddle", "Sine Wave", "Rain", "Cyclone", "Vert Streamer", "Tornado", "Hula", 
        "Random Fall", "Pipes", "Wild Mouse", "Wobbling Dish", "What The Helix",
        "Flying Box", "Moving Boxes", "Rolling Ball", "DNA Spiral", "Drift Rose 3D",
        "Color Twinkles", "Vector Arms", "Chunchun (Serp)", "Chunchun (Liss)", "BPM Volumetric", "Bouncing Balls",
        "Text: FYB", "Icons: CCW", "Icons: CW", "Text: Pizza", "Text: Cubina", "Text: Just Yes", "Text: F Yeah", "Text: Israel", 
        "Rubiks Cube", "Tetris", "Julia 3D", "Atomic Orbits",
        "Tartan / Passing Planes",
        "Comets", "Plane Trails",
        "Noise Clouds", "Meteor 3D",
        "Smashing Planes", "PS Impact",
        "PS Ghost Rider", "PS Box",
        "Atom Smasher", "Fireworks",
        "Lava Lamp",
        "PS Spray",
        "The Matrix",
        "PS Attractor", "PS Waterfall",
        "PS Volcano", "PS Fire", "Pinwheel Arms",
        "Eye of Sauron", "Black Hole", "PS Galaxy", "Bouncing Ball",
        "Plasma Arms", "Aurora", "Palette Audition"
    };

    static_assert(sizeof(SCENE_RULES) / sizeof(SCENE_RULES[0]) == TOTAL_CONCEPTS - NUM_SHAPES,
                  "Scene rules must match the concept deck");
    static_assert(sizeof(sceneNames) / sizeof(sceneNames[0]) == TOTAL_CONCEPTS,
                  "Scene names must match the concept deck");

    static const char* renderNames[] = {"Solid", "Shell", "Wireframe"};
    static const char* motionNames[] = {"Static", "Expand/Collapse", "Spring", "Tumbling", "Spinning"};

    template <typename T>
    static uint8_t popLowestBit(T& currentMask, T validMask) {
        if (currentMask == 0) currentMask = validMask; 
        if (currentMask == 0) return 0; 
        for (int i = 0; i < sizeof(T) * 8; i++) {
            if (currentMask & (1 << i)) {
                currentMask &= ~(1 << i); return i; 
            }
        }
        return 0; 
    }

    struct ConceptDebugState {
        uint16_t motionMask = 0; 
    };

    // --- Static Engine State Variables ---
    uint8_t artDeck[256]; 
    int artIndex = 999; 
    int artDeckSize = 0;
    
    uint8_t billboardDeck[256]; 
    int billboardIndex = 999; 
    int billboardDeckSize = 0;
    
    bool decksBuilt = false;
    bool playingArt = true;
    
    int currentArtRep = 0;
    int currentBillboardRep = 0;
    
    int seqIndex = -1; // Initialized from the selected ordered direction.
    ConceptDebugState states[TOTAL_CONCEPTS]; 
    uint8_t orderedBoxStep = 0;
#if INCLUDE_WLED_PALETTES
    size_t paletteAuditionIndex = 0;
#endif
    uint8_t shuffledBoxStep = 0;

    // Only playback state lives here. Rendering, durations and weighted normal
    // deck membership below remain unchanged. Audio has a separate index bag.
    bool animationSceneActive = false;
    bool animationSceneOrdered = false;

    uint16_t pickNormalDeckIndex(uint16_t limit) {
        // Keep FastLED's existing random generator for the normal decks.
        // A full 256-entry deck needs the unbounded byte form at that bound.
        return limit == 256 ? random8() : random8(uint8_t(limit));
    }

    bool isSelfManagedScene(uint8_t concept) {
        // Lava Lamp uses the deck's palette selection.
        if (concept == CONCEPT_SCENE_LAVA_LAMP) return false;
        return (concept >= CONCEPT_SCENE_HINGE_DANCE && concept <= CONCEPT_SCENE_BOUNCE) ||
               (concept >= CONCEPT_SCENE_RANDOM_FALL && concept <= CONCEPT_SCENE_HELIX) ||
               concept == CONCEPT_SCENE_FLYING_BOX || concept == CONCEPT_SCENE_MOVING_BOXES ||
               (concept >= CONCEPT_SCENE_DNA_SPIRAL && concept <= CONCEPT_SCENE_VECTOR_ARMS) ||
               (concept >= CONCEPT_SCENE_TEXT_FYB && concept < TOTAL_CONCEPTS);
    }

    // --- The Dispatcher ---
    void renderScene(uint8_t concept, ShapeType shape, uint8_t motionIdx, RenderMode rMode, ShaderMode sModeTracker, CRGBPalette16 pal, bool stretch, uint32_t durationMs, bool isTestMode, int currentStep, int totalSteps) {
        MotionType motion = (MotionType)motionIdx;
        const float spinSpeed = shape == SHAPE_OCTAHEDRON ? SPEED_MOTION_SPINNING * 0.5f : SPEED_MOTION_SPINNING;

        if (concept < NUM_SHAPES) {
            if (motion == MOTION_TUMBLING && (shape == SHAPE_OVOID || shape == SHAPE_CYLINDER || shape == SHAPE_TETRAHEDRON)) rMode = RENDER_SOLID;
            if (motion == MOTION_TUMBLING && shape == SHAPE_BOX) { rMode = RENDER_SOLID; stretch = false; }
        }

        const bool rockingSineWave = concept == CONCEPT_SCENE_SINE_WAVE && random8(3) == 0;
        const bool tartanOrPassingPlanes = concept == CONCEPT_SCENE_TARTAN_OR_PASSING_PLANES;
        const bool useTartan = tartanOrPassingPlanes && random8(2) == 0;
        const char* sceneName = sceneNames[concept];
        if (tartanOrPassingPlanes) sceneName = useTartan ? "Tartan" : "Passing Planes";
        const bool isSelfManaged = isSelfManagedScene(concept);
        const char* displayShader = isSelfManaged ? "Internal" : PaletteUtils::shaderName(sModeTracker);
        const char* displayColor = isSelfManaged ? "Per-object choices follow" : (sModeTracker == MODE_HIPHOTIC ? "N/A" : PaletteUtils::getPaletteNameBuffer());
#if INCLUDE_WLED_PALETTES
        const auto *paletteAudition = concept == CONCEPT_SCENE_PALETTE_AUDITION
            ? &WledPalettes::entries[paletteAuditionIndex] : nullptr;
        if (paletteAudition) {
            displayShader = "Linear Flow";
            displayColor = paletteAudition->name;
        }
#endif
        switch (concept) {
            case CONCEPT_SCENE_THE_MATRIX: displayColor = "Green trails / pale tips"; break;
            case CONCEPT_SCENE_NOISE_CLOUDS: displayColor = "Rainbow / noise brightness"; break;
            case CONCEPT_SCENE_METEOR: displayColor = "Rainbow / pale head"; break;
            case CONCEPT_SCENE_PS_IMPACT: displayColor = "Random HSV per meteor / matching splash"; break;
            case CONCEPT_SCENE_PS_FIRE: displayColor = "Fire gradient / heat mapped"; break;
            case CONCEPT_SCENE_EYE_OF_SAURON: displayColor = "Radial Fire / black pupil / blue removed"; break;
            case CONCEPT_SCENE_PINWHEEL_ARMS: displayColor = "Rainbow / paired arms cycling"; break;
            case CONCEPT_SCENE_PS_BOX: displayColor = "C9 New (ID 53) per ball / 250ms fade"; break;
            case CONCEPT_SCENE_PS_WATERFALL: displayColor = "Ocean (ID 9) / four face copies"; break;
            case CONCEPT_SCENE_PS_GHOST_RIDER: displayColor = "Rainbow (ID 11) / age and hue cycle / white head"; break;
            case CONCEPT_SCENE_PS_GALAXY: displayColor = "Rainbow (ID 11) / spiral color sweep"; break;
            case CONCEPT_SCENE_PS_SPRAY: displayColor = "Party (ID 6) / hue advances at emission"; break;
            case CONCEPT_SCENE_PS_ATTRACTOR: displayColor = "Rainbow (ID 11) / spatial and age wave"; break;
            case CONCEPT_SCENE_PS_VOLCANO: displayColor = "Red / Orange Red / Orange / Yellow by age"; break;
            case CONCEPT_SCENE_ATOMIC_ORBITS: displayColor = "Rainbow (ID 11) rings / cycling HSV nucleus"; break;
            case CONCEPT_SCENE_JULIA: displayColor = "Canonical ROYGBIV / 4s hold + 2s transition"; break;
            case CONCEPT_SCENE_AURORA: displayColor = "Green / Pink / Teal / Violet by altitude"; break;
            case CONCEPT_SCENE_COMETS: displayColor = "Rainbow (ID 11) / fading trails"; break;
            case CONCEPT_SCENE_PLANE_TRAILS: displayColor = "Rainbow (ID 11) / fading lanes"; break;
            case CONCEPT_SCENE_DNA_SPIRAL: displayColor = "Rainbow (ID 11)"; break;
            case CONCEPT_SCENE_DRIFT_ROSE: displayColor = "Rainbow (ID 11)"; break;
            case CONCEPT_SCENE_VECTOR_ARMS: displayColor = "Rainbow (ID 11)"; break;
            case CONCEPT_SCENE_PLASMA_ARMS: displayColor = "Rainbow (ID 11)"; break;
            case CONCEPT_SCENE_BLACKHOLE: displayColor = "Rainbow Linear Flow / white center"; break;
            case CONCEPT_SCENE_COLOR_TWINKLES: displayColor = "Rainbow (ID 11) / random swatches"; break;
            case CONCEPT_SCENE_RANDOM_FALL: displayColor = "Blue to Red by height"; break;
            case CONCEPT_SCENE_PIPES: displayColor = "Cycling HSV / common hue"; break;
            case CONCEPT_SCENE_ATOM_SMASHER: displayColor = "Two random HSV atoms / Party sparks / white-hot highlights"; break;
            case CONCEPT_SCENE_FIREWORKS: displayColor = "Burst color choices follow"; break;
            default: break;
        }
        switch (concept) {
            case CONCEPT_SCENE_HULA:
            case CONCEPT_SCENE_TORNADO:
            case CONCEPT_SCENE_CHUNCHUN_SERP:
            case CONCEPT_SCENE_CHUNCHUN_LISSAJOUS:
            case CONCEPT_SCENE_LAVA_LAMP:
            case CONCEPT_SCENE_BPM_CHECKERBOARD:
                displayShader = "Internal palette animation";
                break;
            case CONCEPT_SCENE_BOUNCING_BALLS:
                displayShader = "Stable color per ball ID";
                break;
            default: break;
        }
        if (concept == CONCEPT_SCENE_EYE_OF_SAURON) displayShader = "Radial Fire";
        if (tartanOrPassingPlanes) {
            displayColor = useTartan ? "Rainbow (ID 11)" : "Curated mix recipe follows";
        }
        char speedBuf[32] = "N/A";

        if (concept < NUM_SHAPES) {
            if (motion == MOTION_STATIC) sprintf(speedBuf, "%u ms", SPEED_MOTION_STATIC);
            else if (motion == MOTION_EXPAND_COLLAPSE) sprintf(speedBuf, "%u ms", SPEED_MOTION_EXPAND_COLLAPSE);
            else if (motion == MOTION_SPRING) sprintf(speedBuf, "%u ms", SPEED_MOTION_SPRING);
            else if (motion == MOTION_TUMBLING) sprintf(speedBuf, "%.2f RPS", SPEED_MOTION_TUMBLING);
            else if (motion == MOTION_SPINNING) sprintf(speedBuf, "%.2f RPS", spinSpeed);

            const char* displayStretch = (shape == SHAPE_BOX || shape == SHAPE_TETRAHEDRON || shape == SHAPE_OCTAHEDRON) ? (stretch ? "Yes" : "No") : "N/A";
            Serial.printf("[%02d/%02d] Concept: %02d | Shape: %-12s | Render: %-9s | Shader: %-16s | Color: %-22s | Motion: %-10s | Stretch: %-3s | Speed: %s\n", 
                currentStep, totalSteps, concept, sceneNames[concept], renderNames[rMode], displayShader, displayColor, motionNames[motion], displayStretch, speedBuf);
        }
#if INCLUDE_WLED_PALETTES
        else if (paletteAudition) {
            Serial.printf("Palette audition [%02u/%02u] | ID %02u | %s | Full Box / Linear Flow\n",
                          unsigned(paletteAuditionIndex + 1), unsigned(WledPalettes::count),
                          unsigned(paletteAudition->id), paletteAudition->name);
        }
#endif
        else {
            if (concept == CONCEPT_SCENE_HINGE_DANCE) sprintf(speedBuf, "%u ms", SPEED_SCENE_HINGE_DANCE);
            else if (concept == CONCEPT_SCENE_SPLAT)  sprintf(speedBuf, "%u ms", SPEED_SCENE_SPLAT);
            else if (concept == CONCEPT_SCENE_BOUNCE) sprintf(speedBuf, "%u ms", SPEED_SCENE_BOUNCE);
            else if (concept == CONCEPT_SCENE_SADDLE) sprintf(speedBuf, "%.2f step", SPEED_SCENE_SADDLE);
            else if (concept == CONCEPT_SCENE_SINE_WAVE) sprintf(speedBuf, "%.2f phase", SPEED_SCENE_SINE_WAVE);
            else if (concept == CONCEPT_SCENE_RAIN) sprintf(speedBuf, "%.2f vel", SPEED_SCENE_RAIN);
            else if (concept == CONCEPT_SCENE_CYCLONE) sprintf(speedBuf, "%.2f RPS", SPEED_SCENE_CYCLONE);
            else if (concept == CONCEPT_SCENE_VERT_STREAMER) sprintf(speedBuf, "%u ms", SPEED_SCENE_VERT_STREAMER);
            else if (concept == CONCEPT_SCENE_TORNADO) sprintf(speedBuf, "%.2f RPS", SPEED_SCENE_TORNADO);
            else if (concept == CONCEPT_SCENE_HULA) sprintf(speedBuf, "%.2f RPS", SPEED_SCENE_HULA);
            else if (concept == CONCEPT_SCENE_RANDOM_FALL) sprintf(speedBuf, "%.2f vel", SPEED_SCENE_RANDOM_FALL);
            else if (concept == CONCEPT_SCENE_FIREWORKS) sprintf(speedBuf, "Self-Timed");
            else if (concept == CONCEPT_SCENE_PIPES) sprintf(speedBuf, "%u ms", SPEED_SCENE_PIPES);
            else if (concept == CONCEPT_SCENE_WILD_MOUSE) sprintf(speedBuf, "%u ms", SPEED_SCENE_WILD_MOUSE);
            else if (concept == CONCEPT_SCENE_ATOM_SMASHER) sprintf(speedBuf, "Self-Timed");
            // This speed is stored as an integer. printf's %f requires a
            // double argument; supplying the integer was undefined behavior.
            else if (concept == CONCEPT_SCENE_WOBBLING_DISH) snprintf(speedBuf, sizeof(speedBuf), "%.2f RPM", static_cast<double>(SPEED_SCENE_WOBBLING_DISH));
            else if (concept == CONCEPT_SCENE_HELIX) sprintf(speedBuf, "%u ms", SPEED_SCENE_HELIX);
            else if (concept == CONCEPT_SCENE_FLYING_BOX) sprintf(speedBuf, "%u ms", SPEED_SCENE_FLYING_BOX);
            else if (concept == CONCEPT_SCENE_MOVING_BOXES) sprintf(speedBuf, "%u ms", SPEED_SCENE_MOVING_BOXES);
            else if (concept == CONCEPT_SCENE_ROLLING_BALL) sprintf(speedBuf, "%u ms", SPEED_SCENE_ROLLING_BALL);
            else if (concept == CONCEPT_SCENE_BOUNCING_BALL) sprintf(speedBuf, "%u ms", SPEED_SCENE_BOUNCING_BALL);
            else if (concept == CONCEPT_SCENE_LAVA_LAMP) sprintf(speedBuf, "%.2fx", SPEED_SCENE_LAVA_LAMP);
            else if (concept == CONCEPT_SCENE_DNA_SPIRAL) sprintf(speedBuf, "%.2fx", SPEED_SCENE_DNA_SPIRAL);
            else if (concept == CONCEPT_SCENE_DRIFT_ROSE) sprintf(speedBuf, "Spd: %u", SPEED_SCENE_DRIFT_ROSE);
            else if (concept == CONCEPT_SCENE_COLOR_TWINKLES) sprintf(speedBuf, "Spd: %u", SPEED_SCENE_COLOR_TWINKLES);
            else if (concept == CONCEPT_SCENE_VECTOR_ARMS) sprintf(speedBuf, "Spd: %u", SPEED_SCENE_VECTOR_ARMS);
            else if (concept == CONCEPT_SCENE_CHUNCHUN_SERP) sprintf(speedBuf, "Spd: %u", SPEED_SCENE_CHUNCHUN);
            else if (concept == CONCEPT_SCENE_CHUNCHUN_LISSAJOUS) sprintf(speedBuf, "Spd: %u", SPEED_SCENE_CHUNCHUN);
            else if (concept == CONCEPT_SCENE_BPM_CHECKERBOARD) sprintf(speedBuf, "Spd: %u", SPEED_SCENE_BPM_CHECKERBOARD);
            else if (concept == CONCEPT_SCENE_BOUNCING_BALLS) sprintf(speedBuf, "Self-Timed");
            else if (concept == CONCEPT_SCENE_PINWHEEL_ARMS) sprintf(speedBuf, "Spd: %u",SPEED_SCENE_PINWHEEL_ARMS);
            else if (concept == CONCEPT_SCENE_BLACKHOLE) sprintf(speedBuf, "Self-Timed");
            else if (concept >= CONCEPT_SCENE_TEXT_FYB && concept <= CONCEPT_SCENE_TEXT_ISRAEL) sprintf(speedBuf, "%u ms", SPEED_TEXT_ICONS);
            else if (concept == CONCEPT_SCENE_RUBIKS) sprintf(speedBuf, "Self-Timed");
            else if (concept == CONCEPT_SCENE_TETRIS) snprintf(speedBuf, sizeof(speedBuf), "%lu sec", static_cast<unsigned long>(max((uint32_t)30, durationMs / 1000)));
            
            Serial.printf("[%02d/%02d] Concept: %02d | Scene: %-17s | Shader: %-16s | Color: %-22s | Speed: %s\n", 
                currentStep, totalSteps, concept, sceneName, displayShader, displayColor, speedBuf);
        }

#if CUBINA_CHECK_SPACERS
        SpacerCheck::SceneInfo checkScene;
        checkScene.name = sceneName;
        checkScene.render = concept < NUM_SHAPES ? renderNames[rMode] : "N/A";
        checkScene.motion = concept < NUM_SHAPES ? motionNames[motion] : "Internal";
        checkScene.shader = displayShader;
        checkScene.palette = displayColor;
        checkScene.step = currentStep;
        checkScene.totalSteps = totalSteps;
        checkScene.ordered = isTestMode;

        // Self-managed marquees hide their fixed palette/texture in the old
        // header log. Record the actual background and foreground here so a
        // Pizza/Yes/Cubina failure has useful labels in its one final summary.
        switch (concept) {
            case CONCEPT_SCENE_TEXT_PIZZA:
            case CONCEPT_SCENE_TEXT_CUBINA:
                checkScene.shader = "Noise Field";
                checkScene.palette = "FG Gold / BG Ocean";
                break;
            case CONCEPT_SCENE_TEXT_FYB:
            case CONCEPT_SCENE_TEXT_YEAH:
                checkScene.shader = "Noise Field";
                checkScene.palette = "FG Yellow / BG Lava";
                break;
            case CONCEPT_SCENE_TEXT_YES:
                checkScene.shader = "Noise Field";
                checkScene.palette = "FG White / BG Forest";
                break;
            case CONCEPT_SCENE_TEXT_ISRAEL:
                checkScene.shader = "Linear Flow";
                checkScene.palette = "FG White / BG Ocean";
                break;
            default:
                break;
        }

        SpacerCheck::beginScene(checkScene);
#endif

        if (concept < NUM_SHAPES) { 
            switch(motion) {
                case MOTION_STATIC:          Shapes::animateStatic(shape, durationMs, rMode, pal, sModeTracker, SPEED_MOTION_STATIC, stretch); break;
                case MOTION_EXPAND_COLLAPSE: Shapes::animateExpandingCollapsing(shape, durationMs, rMode, pal, sModeTracker, SPEED_MOTION_EXPAND_COLLAPSE, stretch); break;
                case MOTION_SPRING:          Shapes::animateSpring(shape, durationMs, rMode, pal, sModeTracker, SPEED_MOTION_SPRING, stretch); break;
                case MOTION_TUMBLING:        Shapes::animateTumbling(shape, durationMs, rMode, pal, sModeTracker, SPEED_MOTION_TUMBLING, stretch); break;
                case MOTION_SPINNING:        Shapes::animateSpinning(shape, durationMs, spinSpeed, rMode, pal, sModeTracker, stretch); break;
            }
        } 
        else if (concept >= CONCEPT_SCENE_HINGE_DANCE && concept <= CONCEPT_SCENE_BOUNCE) { 
            if (concept == CONCEPT_SCENE_HINGE_DANCE) Planes::animateHingeDance(durationMs, SPEED_SCENE_HINGE_DANCE);
            else if (concept == CONCEPT_SCENE_SPLAT)  Planes::animateSplat(durationMs, SPEED_SCENE_SPLAT);
            else Planes::animateBounce(durationMs, SPEED_SCENE_BOUNCE);
        }
        else if (concept >= CONCEPT_SCENE_SADDLE && concept <= CONCEPT_SCENE_SINE_WAVE) { 
            if (concept == CONCEPT_SCENE_SADDLE) Surfaces::animateHyperbolicParaboloid(durationMs, SPEED_SCENE_SADDLE, (Orientation)random8(3), pal, sModeTracker, 15);
            else if (rockingSineWave) {
                Surfaces::animateRockingSineWave(durationMs, SPEED_SCENE_SINE_WAVE, pal, sModeTracker);
            } else {
                #ifndef TARGET_2026_CUBOID
                Surfaces::animateSineWave(durationMs, (Orientation)random8(3), WAVE_RADIAL, 360, pal, sModeTracker, SPEED_SCENE_SINE_WAVE);
                #else
                Surfaces::animateSineWaveSplit2026(durationMs, 360, (SplitWaveMode2026)random8(4), pal, sModeTracker, SPEED_SCENE_SINE_WAVE);
                #endif
            }
        }
        else { 
            switch(concept) {
                case CONCEPT_SCENE_TARTAN_OR_PASSING_PLANES:
                    if (useTartan) Patterns::animateTartan(durationMs);
                    else Planes::animatePassingPlanes(durationMs);
                    break;
                case CONCEPT_SCENE_SMASHING_PLANES:      Planes::animateSmashingPlanes(durationMs); break;
                case CONCEPT_SCENE_PLASMA_ARMS:     Streamers::animatePlasmaArms(durationMs); break;
                case CONCEPT_SCENE_AURORA:    Surfaces::animateAurora(durationMs); break;
#if INCLUDE_WLED_PALETTES
                case CONCEPT_SCENE_PALETTE_AUDITION:
                    Shapes::animateStatic(SHAPE_BOX, durationMs, RENDER_SOLID,
                                          WledPalettes::load(*paletteAudition), MODE_LINEAR_FLOW,
                                          SPEED_MOTION_STATIC);
                    break;
#endif
                case CONCEPT_SCENE_COMETS:        Streamers::animateComets(durationMs); break;
                case CONCEPT_SCENE_PLANE_TRAILS: Planes::animatePlaneTrails(durationMs); break;
                case CONCEPT_SCENE_THE_MATRIX:     Patterns::animateTheMatrix(durationMs); break;
                case CONCEPT_SCENE_NOISE_CLOUDS:    Streamers::animateNoiseClouds(durationMs); break;
                case CONCEPT_SCENE_METEOR:         Streamers::animateMeteor(durationMs); break;
                case CONCEPT_SCENE_PS_IMPACT:      Explosions::animatePSImpact(durationMs); break;
                case CONCEPT_SCENE_PS_FIRE:        Flows::animateFire(durationMs); break;
                case CONCEPT_SCENE_EYE_OF_SAURON:
                    EyeOfSauron::animateEyeOfSauron(durationMs);
                    break;
                case CONCEPT_SCENE_PS_VOLCANO: {
                    Flows::PSVolcanoSettings settings;
                    settings.plumeWidthScale = 1.5f;
                    Flows::animatePSVolcano(durationMs, settings);
                    break;
                }
                case CONCEPT_SCENE_PS_BOX:         Balls::animatePSBox(durationMs); break;
                case CONCEPT_SCENE_PS_WATERFALL: Flows::animatePSWaterfall(durationMs); break;
                case CONCEPT_SCENE_PS_GHOST_RIDER: Streamers::animateGhostRider(durationMs); break;
                case CONCEPT_SCENE_PS_GALAXY: {
                    Swirls::GalaxySettings settings;
                    settings.arms = random8(2, 4);
                    Swirls::animatePSGalaxy(durationMs, settings);
                    break;
                }
                case CONCEPT_SCENE_PS_SPRAY: {
                    Streamers::SpraySettings settings;
                    // Leave room at the walls and floor for the spray to develop.
                    settings.leftRight = random8(38, 218);
                    settings.upDown = random8(38, 218);
                    settings.angle = random8(32);
                    Streamers::animateSpray(durationMs, settings);
                    break;
                }
                case CONCEPT_SCENE_PS_ATTRACTOR: {
                    Swirls::AttractorSettings settings;
                    settings.ageColor = true;
                    Swirls::animateAttractor(durationMs, settings, RainbowColors_p);
                    break;
                }
                case CONCEPT_SCENE_ATOMIC_ORBITS:    Swirls::animateAtomicOrbits(durationMs); break;
                case CONCEPT_SCENE_JULIA:          Surfaces::animateJulia(durationMs, 40 * SCALE_MS); break;
                case CONCEPT_SCENE_RAIN:           Patterns::animateRain(durationMs, random8(1, 4), SPEED_SCENE_RAIN, pal, sModeTracker); break;
                case CONCEPT_SCENE_CYCLONE:        Swirls::animateCyclone(durationMs, random8(2, 3), 0.04f, pal, sModeTracker, SPEED_SCENE_CYCLONE); break;
                case CONCEPT_SCENE_VERT_STREAMER:  Streamers::animateVerticalStreamer(durationMs, SPEED_SCENE_VERT_STREAMER, 0.1875f, pal, sModeTracker); break;
                case CONCEPT_SCENE_TORNADO:        Swirls::animateTornado(durationMs, 25, pal, SPEED_SCENE_TORNADO); break;
                case CONCEPT_SCENE_HULA:           Streamers::animateHula(durationMs, 20, pal, SPEED_SCENE_HULA); break;
                case CONCEPT_SCENE_RANDOM_FALL:    Patterns::animateRandomFall(durationMs, SPEED_SCENE_RANDOM_FALL, SPEED_SCENE_RANDOM_FALL * 2.0f); break;
                case CONCEPT_SCENE_FIREWORKS:      Explosions::animateFireworks(durationMs); break;
                case CONCEPT_SCENE_PIPES:          Streamers::animateVerticalPipes(durationMs, SPEED_SCENE_PIPES); break;
                case CONCEPT_SCENE_WILD_MOUSE:
                    Streamers::animateWildMouse(durationMs, SPEED_SCENE_WILD_MOUSE);
                    break;
                case CONCEPT_SCENE_ATOM_SMASHER:   Explosions::animateAtomSmasher(durationMs); break;
                case CONCEPT_SCENE_WOBBLING_DISH:  Surfaces::animateWobblingDish(durationMs, false, SPEED_SCENE_WOBBLING_DISH); break;
                case CONCEPT_SCENE_HELIX:          Streamers::animateWhatTheHelix(durationMs, SPEED_SCENE_HELIX); break;
                case CONCEPT_SCENE_FLYING_BOX:     Boxes::animateFlyingBox(durationMs, SPEED_SCENE_FLYING_BOX); break;
                case CONCEPT_SCENE_MOVING_BOXES:   Boxes::animateMovingBoxes(durationMs, SPEED_SCENE_MOVING_BOXES); break;
                case CONCEPT_SCENE_ROLLING_BALL:   Balls::animateRollingBall(durationMs, (RenderMode)(random8(100) < 30 ? RENDER_SHELL : RENDER_SOLID), pal, sModeTracker, SPEED_SCENE_ROLLING_BALL); break;
                case CONCEPT_SCENE_BOUNCING_BALL:  Balls::animateBouncingBall(durationMs, SPEED_SCENE_BOUNCING_BALL); break;
                
                case CONCEPT_SCENE_DNA_SPIRAL:     Streamers::animateDNASpiral(durationMs, RainbowColors_p, SPEED_SCENE_DNA_SPIRAL); break;
                case CONCEPT_SCENE_DRIFT_ROSE:     Streamers::animateDriftRose3D(durationMs, RainbowColors_p, SPEED_SCENE_DRIFT_ROSE); break;
                case CONCEPT_SCENE_COLOR_TWINKLES: Patterns::animateColorTwinkles(durationMs, SPEED_SCENE_COLOR_TWINKLES, 128); break;
                case CONCEPT_SCENE_VECTOR_ARMS: Streamers::animateVectorArms(durationMs, SPEED_SCENE_VECTOR_ARMS); break;
                case CONCEPT_SCENE_CHUNCHUN_SERP:  Streamers::animateChunchunVolumetric(durationMs, pal, SPEED_SCENE_CHUNCHUN, 32, CHUNCHUN_SERPENTINE); break;
                case CONCEPT_SCENE_CHUNCHUN_LISSAJOUS: Streamers::animateChunchunVolumetric(durationMs, pal, SPEED_SCENE_CHUNCHUN, 128, CHUNCHUN_LISSAJOUS); break;
                case CONCEPT_SCENE_BPM_CHECKERBOARD:   Patterns::animateBpmVolumetric(durationMs, pal, SPEED_SCENE_BPM_CHECKERBOARD, BPM_CHECKERBOARD); break;
                case CONCEPT_SCENE_BOUNCING_BALLS:    Balls::animateBouncingBalls(durationMs, pal); break;
                case CONCEPT_SCENE_PINWHEEL_ARMS:
                    Swirls::animatePinwheelArms(max(durationMs, Swirls::pinwheelArmsCycleMs), SPEED_SCENE_PINWHEEL_ARMS);
                    break;
                case CONCEPT_SCENE_BLACKHOLE:      Swirls::animateBlackHole(durationMs); break;

                case CONCEPT_SCENE_LAVA_LAMP: {
                    uint8_t lavaR = random8(3);
                    if (lavaR == 0) Balls::animateLavaLamp(durationMs, 1.0f * SCALE_RATE, 0.20f, pal);
                    else if (lavaR == 1) Balls::animateLavaLamp(durationMs, 2.5f * SCALE_RATE, 0.10f, pal);
                    else Balls::animateLavaLamp(durationMs, 0.3f * SCALE_RATE, 0.20f, pal);
                    break;
                }

                // --- BILLBOARD DECK ---
                case CONCEPT_SCENE_TEXT_FYB: 
                    Letters::scrollTextCycles("Fuck Yer `C27` Burn", 2, 1.0f, 1.0f, SPEED_TEXT_ICONS, CRGB::Yellow, LavaColors_p, MODE_NOISE_FIELD, true, 75); break;
                case CONCEPT_SCENE_ICONS_CCW: 
                    Icons::scrollIcons("{(1.2.3),(4.5.6),(7.8.9.10),(11.12.13.14),(15.16.17.18),(19.20),21,22,23,24,25,26,27,28,30,31,32}", durationMs, SPEED_TEXT_ICONS, 120, false); break;
                case CONCEPT_SCENE_ICONS_CW: 
                    Icons::scrollIcons("{(1.2.3),(4.5.6),(7.8.9.10),(11.12.13.14),(15.16.17.18),(19.20),21,22,23,24,25,26,27,28,30,31,32}", durationMs, SPEED_TEXT_ICONS, 120, true); break;
                case CONCEPT_SCENE_TEXT_PIZZA: 
                    Letters::scrollTextCycles("`R30` KINKY PIZZA `R30`", 2, 1.0f, 1.0f, SPEED_TEXT_ICONS, CRGB::Gold, OceanColors_p, MODE_NOISE_FIELD, false, 75); break;
                case CONCEPT_SCENE_TEXT_CUBINA: 
                    Letters::scrollTextCycles("`(4.5.6)` CUBINA `C27`", 2, 1.0f, 1.0f, SPEED_TEXT_ICONS, CRGB::Gold, OceanColors_p, MODE_NOISE_FIELD, false, 75); break;
                case CONCEPT_SCENE_TEXT_YES: 
                    Letters::scrollTextCycles("`25` Just Say Yes `24`", 2, 1.0f, 1.0f, SPEED_TEXT_ICONS, CRGB::White, ForestColors_p, MODE_NOISE_FIELD, false, 85); break;
                case CONCEPT_SCENE_TEXT_YEAH: 
                    Letters::scrollTextCycles("`C27` FUCK `23` YEAH! `28`", 2, 1.0f, 1.0f, SPEED_TEXT_ICONS, CRGB::Yellow, LavaColors_p, MODE_NOISE_FIELD, false, 75); break;
                case CONCEPT_SCENE_TEXT_ISRAEL: 
                    Letters::scrollTextCycles("`HeM` `HySral` `HHy` `29`", 2, 1.0f, 1.0f, SPEED_TEXT_ICONS, CRGB::White, OceanColors_p, MODE_LINEAR_FLOW, false, 75, true); 
                    break;
                            
                // --- GAMES ---
                case CONCEPT_SCENE_RUBIKS:
                    Rubiks::runScrambleAndSolveShow(); break;
                case CONCEPT_SCENE_TETRIS: {
                    Tetris game; game.animate(max((uint32_t)30, durationMs / 1000), Tetris::SPEED_FAST); break;
                }
            }
        }

#if CUBINA_CHECK_SPACERS
        // Every show() above has completed before this point. Emit exactly
        // one summary for this invocation, after its routine has returned.
        SpacerCheck::endScene();
#endif
    }
}

// =========================================================================================
// PUBLIC NAMESPACE IMPLEMENTATION
// =========================================================================================

namespace TestDemo {

    PlaybackOrder &animationOrder() {
        static PlaybackOrder order = PlaybackOrder::Descending;
#if CUBINA_CHECK_SPACERS
        // The requested baseline is the complete RANDOMIZED playlist.
        // Keep any local ten-second duration settings elsewhere in this file.
        order = PlaybackOrder::Shuffled;
#endif
        return order;
    }

    void runAnimation() {
        stopPlayback();

        for (;;) {
            runEngine(animationOrder() != PlaybackOrder::Shuffled);
        }
    }

    void Detail::advanceAnimation() {
        if (animationSceneOrdered && seqIndex >= 0 && seqIndex < TOTAL_CONCEPTS) {
            // Shuffled decks already moved their cursor BEFORE drawing.
            // Ordered playback also accepts Next between app calls, when
            // stopPlayback has cleared animationSceneActive. CubeSystem
            // consumes the revision at that checkpoint before relaunching.
            if (orderedConcepts[seqIndex] == CONCEPT_SHAPE_BOX && orderedBoxStep != 0) return;
#if INCLUDE_WLED_PALETTES
            if (orderedConcepts[seqIndex] == CONCEPT_SCENE_PALETTE_AUDITION) {
                if (++paletteAuditionIndex < WledPalettes::count) return;
                paletteAuditionIndex = 0;
            }
#endif
            seqIndex += animationOrder() == PlaybackOrder::Descending ? -1 : 1;
        }
    }

    void Detail::stopAnimation() {
        Detail::animationDurationHeld() = false;
        animationSceneActive = false;
    }

  void runHardwareWiringTest() {
        Serial.println("\n--- STARTING FAST BOOT DIAGNOSTIC ---");
        
        // 16 distinct colors wrapping the hue wheel
        CRGB testColors[16] = { 
            CRGB::Red, CRGB::OrangeRed, CRGB::Orange, CRGB::Gold, 
            CRGB::Yellow, CRGB::Chartreuse, CRGB::Green, CRGB::MediumSpringGreen, 
            CRGB::Cyan, CRGB::DeepSkyBlue, CRGB::Blue, CRGB::Indigo, 
            CRGB::DarkViolet, CRGB::Magenta, CRGB::DeepPink, CRGB::White
        };

        Serial.println("Test 1: Full Rainbow Hold (Infinite)...");
        clearAll(); 
        for (uint8_t x = 0; x < RNDR_X; x++) {
            CRGB planeColor = testColors[x % 16]; 
            for (uint8_t y = 0; y < RNDR_Y; y++) {
                for (uint8_t z = 0; z < RNDR_Z; z++) setVoxel(x, y, z, planeColor); 
            }
        }
        showCube(); 
        Serial.println("Test 1: Showcube rainbow hold start");
        
        delay(5000); 

        // Trap the system here indefinitely for wiring inspection
        /*
        while (true) { 
            delay(3000); 
             Serial.println("infinite hold");
        } */
        
        Serial.println("Test 1: End Rainbow hold...");
        
        // (Unreachable due to infinite loop, but modulo updated for future use)
        Serial.println("Test 2: Sweeping X-Planes (Left to Right)...");
        for (uint8_t x = 0; x < RNDR_X; x++) {
            clearAll(); CRGB planeColor = testColors[x % 16];
            for (uint8_t y = 0; y < RNDR_Y; y++) {
                for (uint8_t z = 0; z < RNDR_Z; z++) setVoxel(x, y, z, planeColor); 
            }
            showCube(); delay(600); 
        }
        clearAll(); showCube();
        Serial.println("--- BOOT DIAGNOSTIC COMPLETE ---\n");
    }

    void runEngine(bool isTestMode) {
        const int ART_DECK_REPETITIONS       = 1;  
        const int BILLBOARD_DECK_REPETITIONS = 1;  
        // Normal playback durations. The ordered test branch below retains
        // its separate ten-second duration for deliberate regression runs.
        //const uint32_t DURATION_ART_MS = 30000;
        //const uint32_t DURATION_BILLBOARD_MS = 20000;
        const uint32_t DURATION_ART_MS = 10000;
        const uint32_t DURATION_BILLBOARD_MS = 10000;
        
        
        uint32_t durationMs;


        uint8_t concept = 0;
        int currentStep = 0, totalSteps = 0;

        if (isTestMode) {
            const int direction = animationOrder() == PlaybackOrder::Descending ? -1 : 1;
            // Signed cursor wraps in either direction and skips disabled cards.
            // Shape motion exhaustion still happens before moving to a card.
            for (int checked = 0; checked < TOTAL_CONCEPTS; ++checked) {
                if (seqIndex < 0 || seqIndex >= TOTAL_CONCEPTS) {
                    seqIndex = direction < 0 ? TOTAL_CONCEPTS - 1 : 0;
                }
                const ConceptType candidate = orderedConcepts[seqIndex];
                const bool enabled = candidate < NUM_SHAPES ?
                    SHAPE_RULES[candidate].enabled : SCENE_RULES[candidate - NUM_SHAPES].enabled;
                if (enabled) {
                    break;
                }
                seqIndex += direction;
            }
            concept = orderedConcepts[seqIndex];
            durationMs = 10000;
            currentStep = direction < 0 ? TOTAL_CONCEPTS - seqIndex : seqIndex + 1;
            totalSteps = TOTAL_CONCEPTS;

        } else {
            if (!decksBuilt) {
                for (int i = 0; i < TOTAL_CONCEPTS; i++) {
                    bool enabled = (i < NUM_SHAPES) ? SHAPE_RULES[i].enabled : SCENE_RULES[i - NUM_SHAPES].enabled;
                    if (enabled) {
                        uint8_t weight = (i < NUM_SHAPES) ? SHAPE_RULES[i].demoBoost : SCENE_RULES[i - NUM_SHAPES].demoBoost;
                        bool isBillboard = (i >= CONCEPT_SCENE_TEXT_FYB && i <= CONCEPT_SCENE_TEXT_ISRAEL);
                        
                        for (int w = 0; w < weight; w++) {
                            if (isBillboard && billboardDeckSize < 256) billboardDeck[billboardDeckSize++] = i;
                            else if (!isBillboard && artDeckSize < 256) artDeck[artDeckSize++] = i;
                        }
                    }
                }
                decksBuilt = true;
            }

            if (playingArt && artDeckSize > 0) {
                if (artIndex >= artDeckSize) {
                    currentArtRep++;
                    if (currentArtRep > ART_DECK_REPETITIONS && billboardDeckSize > 0) {
                        // Switch to Billboard
                        playingArt = false; 
                        currentBillboardRep = 1; // Start cleanly at 1
                        billboardIndex = 0;
                        
                        Detail::shuffleDeck(billboardDeck, billboardDeckSize, pickNormalDeckIndex);
                        Serial.printf("\n=======================================\n   SHUFFLING THE BILLBOARD DECK (Play %d of %d)! \n=======================================\n", currentBillboardRep, BILLBOARD_DECK_REPETITIONS);
                    } else {
                        // Loop Art
                        artIndex = 0;
                        shuffledBoxStep = 0;
                        Detail::shuffleDeck(artDeck, artDeckSize, pickNormalDeckIndex);
                        Serial.printf("\n=======================================\n   SHUFFLING THE ART DECK (Play %d of %d)! \n=======================================\n", currentArtRep, ART_DECK_REPETITIONS);
                    }
                }
            } else if (!playingArt && billboardDeckSize > 0) {
                if (billboardIndex >= billboardDeckSize) {
                    currentBillboardRep++;
                    if (currentBillboardRep > BILLBOARD_DECK_REPETITIONS && artDeckSize > 0) {
                        // Switch to Art
                        playingArt = true; 
                        currentArtRep = 1; // Start cleanly at 1
                        artIndex = 0; 
                        shuffledBoxStep = 0;
                        
                        Detail::shuffleDeck(artDeck, artDeckSize, pickNormalDeckIndex);
                        Serial.printf("\n=======================================\n   SHUFFLING THE ART DECK (Play %d of %d)! \n=======================================\n", currentArtRep, ART_DECK_REPETITIONS);
                    } else {
                        // Loop Billboard
                        billboardIndex = 0;
                        Detail::shuffleDeck(billboardDeck, billboardDeckSize, pickNormalDeckIndex);
                        Serial.printf("\n=======================================\n   SHUFFLING THE BILLBOARD DECK (Play %d of %d)! \n=======================================\n", currentBillboardRep, BILLBOARD_DECK_REPETITIONS);
                    }
                }
            }

            // --- FETCHING THE CONCEPT ---
            if (playingArt && artDeckSize > 0) {
                 concept = artDeck[artIndex++];
                 durationMs = DURATION_ART_MS; 
                 currentStep = artIndex; 
                 totalSteps = artDeckSize;
            } else if (!playingArt && billboardDeckSize > 0) {
                 concept = billboardDeck[billboardIndex++];
                 durationMs = DURATION_BILLBOARD_MS; 
                 currentStep = billboardIndex; 
                 totalSteps = billboardDeckSize;
            } else {
                concept = 0; durationMs = 15000; currentStep = 1; totalSteps = 1;
            }
        }

        ShapeType shape = SHAPE_SPHERE;
        uint8_t motion = 0;
        RenderMode rMode = RENDER_SOLID;
        ShaderMode sModeTracker = MODE_SOLID;
        CRGBPalette16 pal = PartyColors_p;
        bool stretch = true;

        if (concept < NUM_SHAPES) {
            shape = (ShapeType)concept;
            uint16_t allowedMotions = SHAPE_RULES[concept].allowedMotions;
            uint8_t allowedRenders = SHAPE_RULES[concept].allowedRenders;

            if (isTestMode) motion = popLowestBit(states[concept].motionMask, allowedMotions);
            else {
                uint8_t mPool[5]; uint8_t mn = 0;
                for (int i = 0; i <= MOTION_SPINNING; i++) if (allowedMotions & (1 << i)) mPool[mn++] = i;
                motion = mPool[random8(mn)];
            }

            uint8_t rPool[3]; uint8_t rn = 0;
            for (int i = 0; i < 3; i++) if (allowedRenders & (1 << i)) rPool[rn++] = i;
            rMode = (RenderMode)rPool[random8(rn)];
            
            bool allowSolid = (rMode != RENDER_WIREFRAME) && (random8(100) < 25); 
            if (shape == SHAPE_BOX) stretch = (motion != MOTION_TUMBLING);
            else stretch = (motion != MOTION_SPRING) && (random8(2) == 0);
            
#if !INCLUDE_WLED_PALETTES
            // Preserve the original Box shuffle when the expanded pool is disabled.
            static CRGBPalette16 boxPalettes[5] = {RainbowColors_p, LavaColors_p, ForestColors_p, OceanColors_p, PartyColors_p};
            static const char* boxPaletteNames[5] = {"Rainbow", "Lava", "Forest", "Ocean", "Party"};
#endif
            
            uint8_t &boxStep = isTestMode ? orderedBoxStep : shuffledBoxStep;
            if (motion == MOTION_STATIC) {
                if (shape == SHAPE_BOX) { 
                    sModeTracker = boxShaders[boxStep];
#if !INCLUDE_WLED_PALETTES
                    if (boxStep == 0) {
                        for (int i = 4; i > 0; i--) {
                            int j = random8(i + 1);
                            CRGBPalette16 tempPal = boxPalettes[i]; boxPalettes[i] = boxPalettes[j]; boxPalettes[j] = tempPal;
                            const char* tempName = boxPaletteNames[i]; boxPaletteNames[i] = boxPaletteNames[j]; boxPaletteNames[j] = tempName;
                        }
                    }
#endif
                } else {
                    sModeTracker = boxShaders[random8(boxShaderCount)];
                }
            } else {
                if (allowSolid && random8(2) == 0) sModeTracker = MODE_SOLID;
                else {
                    sModeTracker = PaletteUtils::getRandomNonSolidShader();
                }
            }
            
            pal = PaletteUtils::getSmartPalette(sModeTracker, false,
                shape == SHAPE_BOX && rMode != RENDER_WIREFRAME);
            
            // ==========================================
            // THE BOX PALETTE ENFORCER
            // ==========================================
            if (shape == SHAPE_BOX) {
                if (sModeTracker == MODE_UNIFORM_CYCLE) { 
                    pal = RainbowColors_p; 
                    strcpy(PaletteUtils::getPaletteNameBuffer(), "Rainbow"); 
                }
                else if (sModeTracker == MODE_HIPHOTIC) {
                    strcpy(PaletteUtils::getPaletteNameBuffer(), "Hiphotic Native");
                }
                else if (sModeTracker == MODE_DISTORTION_WAVES) {
                    pal = PaletteUtils::getRandomOrganicPalette(rMode != RENDER_WIREFRAME);
                }
                else if (sModeTracker != MODE_SOAP && sModeTracker != MODE_WAVING_CELL) {
#if INCLUDE_WLED_PALETTES
                    pal = PaletteUtils::getRandomPalette(rMode != RENDER_WIREFRAME);
#else
                    // The four original box brushes use the shuffled palettes.
                    const int palIndex = boxStep >= 1 && boxStep <= 4 ? boxStep - 1 : random8(5);
                    pal = boxPalettes[palIndex];
                    strcpy(PaletteUtils::getPaletteNameBuffer(), boxPaletteNames[palIndex]);
#endif
                }
                
                // Increment tracker AFTER assignment is complete
                boxStep = (boxStep + 1) % boxShaderCount;
            } 
            else {
                if (sModeTracker == MODE_UNIFORM_CYCLE) { pal = RainbowColors_p; strcpy(PaletteUtils::getPaletteNameBuffer(), "Rainbow"); }
            }

            if (shape == SHAPE_OCTAHEDRON || shape == SHAPE_MERKABA || shape == SHAPE_CYLINDER || shape == SHAPE_HEMISPHERES || shape == SHAPE_HOURGLASS) {
                sModeTracker = MODE_SOLID; 
                if (shape == SHAPE_MERKABA && random8(2) == 0) { pal = CRGBPalette16(CRGB::White); strcpy(PaletteUtils::getPaletteNameBuffer(), "Solid White (Merkaba Symmetry)"); }
                else { pal = CRGBPalette16(CRGB::Gray); strcpy(PaletteUtils::getPaletteNameBuffer(), "Solid Gray (Dynamic Contrast)"); }
            }
        } else {
            if (isSelfManagedScene(concept)) { sModeTracker = MODE_SOLID; pal = PartyColors_p; }
            else {
                // Ensure the required "No Solids" scenes are fully protected from random solid rolls
                bool allowSolid = (concept != CONCEPT_SCENE_HULA && concept != CONCEPT_SCENE_VERT_STREAMER && concept != CONCEPT_SCENE_TORNADO && concept != CONCEPT_SCENE_CHUNCHUN_SERP && concept != CONCEPT_SCENE_CHUNCHUN_LISSAJOUS && concept != CONCEPT_SCENE_BPM_CHECKERBOARD && concept != CONCEPT_SCENE_BOUNCING_BALLS) && (random8(100) < 25);
                
                if (concept == CONCEPT_SCENE_SADDLE || concept == CONCEPT_SCENE_SINE_WAVE || concept == CONCEPT_SCENE_ROLLING_BALL) {
                    if (allowSolid && random8(2) == 0) sModeTracker = MODE_SOLID;
                    else sModeTracker = PaletteUtils::getRandomNonSolidShader();
                } 
                else { 
                    if (allowSolid) sModeTracker = MODE_SOLID; 
                    else {
                        constexpr ShaderMode sceneFlow[] = {MODE_UNIFORM_CYCLE, MODE_LINEAR_FLOW,
                            MODE_NOISE_FIELD, MODE_SCATTER, MODE_DISTORTION_WAVES, MODE_SOAP, MODE_WAVING_CELL};
                        sModeTracker = sceneFlow[random8(sizeof(sceneFlow) / sizeof(sceneFlow[0]))];
                    }
                }
                
                if (concept == CONCEPT_SCENE_HULA || concept == CONCEPT_SCENE_CHUNCHUN_SERP ||
                    concept == CONCEPT_SCENE_CHUNCHUN_LISSAJOUS) {
                    pal = PaletteUtils::getRandomPalette();
                }
                else if (concept == CONCEPT_SCENE_VERT_STREAMER || concept == CONCEPT_SCENE_SADDLE ||
                         concept == CONCEPT_SCENE_SINE_WAVE) {
                    pal = PaletteUtils::getSmartPalette(sModeTracker, true);
                }
                else if (sModeTracker == MODE_SOAP || sModeTracker == MODE_WAVING_CELL) {
                    pal = PaletteUtils::getSmartPalette(sModeTracker);
                }
                else if (concept == CONCEPT_SCENE_TORNADO || concept == CONCEPT_SCENE_LAVA_LAMP ||
                         concept == CONCEPT_SCENE_BPM_CHECKERBOARD || concept == CONCEPT_SCENE_BOUNCING_BALLS) {
                    pal = PaletteUtils::getRandomOrganicPalette(); // Satisfies "random palette no solids" requirement
                }
                else {
                    pal = PaletteUtils::getSmartPalette(sModeTracker);
                }
            }        
        }

        Detail::animationDurationHeld() = animationHoldUntilNext() || concept == CONCEPT_SCENE_PALETTE_AUDITION;
        animationSceneActive = true;
        animationSceneOrdered = isTestMode;
        // Finite shows (for example Rubiks) may return without a duration
        // loop. While held, repeat this same selected scene and its parameters.
        do {
            renderScene(concept, shape, motion, rMode, sModeTracker, pal, stretch, durationMs, isTestMode, currentStep, totalSteps);
        } while (TestDemo::playbackHeld() || Detail::animationDurationHeld());
        Detail::animationDurationHeld() = false;
        animationSceneActive = false;

        if (isTestMode) {
            const bool moreBoxTextures = concept == CONCEPT_SHAPE_BOX && orderedBoxStep != 0;
            const bool moreMotions = concept < NUM_SHAPES && states[concept].motionMask != 0;
            if (!moreBoxTextures && !moreMotions) {
                seqIndex += animationOrder() == PlaybackOrder::Descending ? -1 : 1;
            }
        }
    }
}
