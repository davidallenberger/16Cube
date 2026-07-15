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
    const uint32_t SPEED_SCENE_WOBBLING_DISH     = (45.0f * SCALE_RATE);  
    const uint32_t SPEED_SCENE_HELIX            = (uint32_t)(50 * SCALE_MS);  
    const uint32_t SPEED_SCENE_FLYING_BOX       = (uint32_t)(25 * SCALE_MS);  
    const uint32_t SPEED_SCENE_MOVING_BOXES     = (uint32_t)(40 * SCALE_MS);  
    const uint32_t SPEED_SCENE_ROLLING_BALL     = (uint32_t)(25 * SCALE_MS);   
    const uint32_t SPEED_SCENE_BOUNCING_BALL    = (uint32_t)(25 * SCALE_MS);   
    const float    SPEED_SCENE_LAVA_LAMP        = (.125f * SCALE_RATE);

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
        CONCEPT_SCENE_TORNADO = 18, CONCEPT_SCENE_HULA = 19, CONCEPT_SCENE_RANDOM_FALL = 20, CONCEPT_SCENE_FIREWORKS = 21,
        CONCEPT_SCENE_PIPES = 22, CONCEPT_SCENE_WILD_MOUSE = 23, CONCEPT_SCENE_ATOM_SMASHER = 24, CONCEPT_SCENE_WOBBLING_DISH = 25,
        CONCEPT_SCENE_HELIX = 26, CONCEPT_SCENE_FLYING_BOX = 27, CONCEPT_SCENE_MOVING_BOXES = 28, CONCEPT_SCENE_ROLLING_BALL = 29,
        CONCEPT_SCENE_BOUNCING_BALL = 30, CONCEPT_SCENE_LAVA_LAMP = 31, // <-- ADDED HERE
        // Text & Icons (Shifted down by 1)
        CONCEPT_SCENE_TEXT_FYB = 32, CONCEPT_SCENE_ICONS_CCW = 33, CONCEPT_SCENE_ICONS_CW = 34, CONCEPT_SCENE_TEXT_PIZZA = 35,
        CONCEPT_SCENE_TEXT_CUBINA = 36, CONCEPT_SCENE_TEXT_YES = 37, CONCEPT_SCENE_TEXT_YEAH = 38, CONCEPT_SCENE_TEXT_ISRAEL = 39,
        // Games (Shifted down by 1)
        CONCEPT_SCENE_RUBIKS = 40, CONCEPT_SCENE_TETRIS = 41
    };

    static const int NUM_SHAPES = 10;
    static const int NUM_SCENES = 32; 
    static const int TOTAL_CONCEPTS = NUM_SHAPES + NUM_SCENES;

    struct ShapeRule { bool enabled; uint8_t demoBoost; uint16_t allowedMotions; uint8_t allowedRenders; };
    struct SceneRule { bool enabled; uint8_t demoBoost; };

    const ShapeRule SHAPE_RULES[] = {
        /* BOX         */ { true,  6, MASK_STATIC, MASK_SOLID }, 
        /* SPHERE      */ { true,  1, MASK_STATIC | MASK_EXPAND_COLLAPSE, MASK_SOLID  },
        /* OVOID       */ { false, 1, MASK_STATIC | MASK_EXPAND_COLLAPSE | MASK_TUMBLING, MASK_SOLID  },
        /* TETRAHEDRON */ { false, 1, MASK_STATIC | MASK_EXPAND_COLLAPSE | MASK_TUMBLING, MASK_SOLID | MASK_WIREFRAME },
        /* OCTAHEDRON  */ { false, 1, MASK_STATIC | MASK_SPINNING, MASK_SOLID | MASK_WIREFRAME },
        /* MERKABA     */ { true,  2, MASK_TUMBLING, MASK_SOLID },
        /* TORUS       */ { false, 1, MASK_TUMBLING | MASK_SPINNING, MASK_SOLID },
        /* CYLINDER    */ { true,  1, MASK_TUMBLING, MASK_SOLID },
        /* HEMISPHERES */ { false, 1, MASK_STATIC | MASK_EXPAND_COLLAPSE | MASK_TUMBLING, MASK_SOLID },
        /* HOURGLASS   */ { true,  1, MASK_TUMBLING, MASK_SOLID }
    };

    const SceneRule SCENE_RULES[] = {
        /* Hinge Dance  */ { true,  2 }, /* Splat        */ { true,  2 }, /* Bounce       */ { true,  2 },
        /* Saddle       */ { true,  1 }, /* Sine Wave    */ { true,  3 }, /* Rain         */ { true,  1 },
        /* Cyclone      */ { true,  1 }, /* Vert Stream  */ { true,  1 }, /* Tornado      */ { true,  1 },
        /* Hula         */ { true,  1 }, /* Random Fall  */ { true,  1 }, /* Fireworks    */ { true,  1 },
        /* Pipes        */ { true,  1 }, /* Wild Mouse   */ { true,  1 }, /* Atom Smasher */ { true,  1 },
        /* Wobbling Dish*/ { true,  1 }, /* Helix        */ { true,  1 }, /* Flying Box   */ { true,  2 },
        /* Moving Boxes */ { true,  2 }, /* Rolling Ball */ { true,  1 }, /* Bouncing Ball*/ { true,  1 }, /* Lava Lamp    */ { true,  1 },
        /* Text FYB     */ { true,  1 }, /* Icons CCW    */ { true,  1 }, /* Icons CW     */ { true,  1 },
        /* Text Pizza   */ { true,  1 }, /* Text Cubina  */ { true,  1 }, /* Text Yes   */ { true,  1 },
        /* Text Yeah    */ { true,  1 }, /* Text Israel  */ { false,  1 }, /* Rubiks       */ { true,  1 },
        /* Tetris       */ { true,  1 }
    };

    static const char* sceneNames[] = {
        "Box", "Sphere", "Ovoid", "Tetrahedron", "Octahedron", "Merkaba", "Torus", "Cylinder", "Hemispheres", "Hourglass",
        "Hinge Dance", "Splat", "Bounce", "Saddle", "Sine Wave", "Rain", "Cyclone", "Vert Streamer", "Tornado", "Hula", 
        "Random Fall", "Fireworks", "Pipes", "Wild Mouse", "Atom Smasher", "Wobbling Dish", "What The Helix",
        "Flying Box", "Moving Boxes", "Rolling Ball", "Bouncing Ball", "Lava Lamp", "Text: FYB", "Icons: CCW", "Icons: CW", "Text: Pizza", 
        "Text: Cubina", "Text: Yes", "Text: Yeah", "Text: Israel", "Rubiks Cube", "Tetris"
    };

    static const char* renderNames[] = {"Solid", "Shell", "Wireframe"};
    static const char* shaderModeNames[] = {"Solid", "Uniform Cycle", "Spatial Gradient", "Linear Flow", "Noise Field", "Scatter", "Distance Field", "Hiphotic"};
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
        uint8_t shaderTracker = 0; 
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
    
    uint8_t seqConcept = 0;
    ConceptDebugState states[TOTAL_CONCEPTS]; 

    // --- The Dispatcher ---
    void renderScene(uint8_t concept, ShapeType shape, uint8_t motionIdx, RenderMode rMode, ShaderMode sModeTracker, CRGBPalette16 pal, bool stretch, uint32_t durationMs, bool isTestMode, int currentStep, int totalSteps) {
        MotionType motion = (MotionType)motionIdx;

        if (concept < NUM_SHAPES) {
            if (motion == MOTION_TUMBLING && (shape == SHAPE_OVOID || shape == SHAPE_CYLINDER || shape == SHAPE_TETRAHEDRON)) rMode = RENDER_SOLID;
            if (motion == MOTION_TUMBLING && shape == SHAPE_BOX) { rMode = RENDER_SOLID; stretch = false; }
        }

        bool isSelfManaged = false;
        if (concept >= NUM_SHAPES) {
            isSelfManaged = (concept >= CONCEPT_SCENE_HINGE_DANCE && concept <= CONCEPT_SCENE_BOUNCE) || 
                            (concept >= CONCEPT_SCENE_RANDOM_FALL && concept <= CONCEPT_SCENE_HELIX) ||
                            (concept == CONCEPT_SCENE_FLYING_BOX) || (concept == CONCEPT_SCENE_MOVING_BOXES) ||
                            (concept >= CONCEPT_SCENE_TEXT_FYB && concept <= CONCEPT_SCENE_TETRIS);
        }

        const char* displayShader = isSelfManaged ? "Internal" : shaderModeNames[sModeTracker];
        const char* displayColor = isSelfManaged ? "Self-Managed" : (sModeTracker == MODE_HIPHOTIC ? "N/A" : PaletteUtils::getPaletteNameBuffer());
        char speedBuf[32] = "N/A";

        if (concept < NUM_SHAPES) {
            if (motion == MOTION_STATIC) sprintf(speedBuf, "%u ms", SPEED_MOTION_STATIC);
            else if (motion == MOTION_EXPAND_COLLAPSE) sprintf(speedBuf, "%u ms", SPEED_MOTION_EXPAND_COLLAPSE);
            else if (motion == MOTION_SPRING) sprintf(speedBuf, "%u ms", SPEED_MOTION_SPRING);
            else if (motion == MOTION_TUMBLING) sprintf(speedBuf, "%.2f RPS", SPEED_MOTION_TUMBLING);
            else if (motion == MOTION_SPINNING) sprintf(speedBuf, "%.2f RPS", SPEED_MOTION_SPINNING);

            const char* displayStretch = (shape == SHAPE_BOX || shape == SHAPE_TETRAHEDRON || shape == SHAPE_OCTAHEDRON) ? (stretch ? "Yes" : "No") : "N/A";
            Serial.printf("[%02d/%02d] Concept: %02d | Shape: %-12s | Render: %-9s | Shader: %-16s | Color: %-22s | Motion: %-10s | Stretch: %-3s | Speed: %s\n", 
                currentStep, totalSteps, concept, sceneNames[concept], renderNames[rMode], displayShader, displayColor, motionNames[motion], displayStretch, speedBuf);
        } else {
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
            else if (concept == CONCEPT_SCENE_WOBBLING_DISH) sprintf(speedBuf, "%.2f RPM", SPEED_SCENE_WOBBLING_DISH);
            else if (concept == CONCEPT_SCENE_HELIX) sprintf(speedBuf, "%u ms", SPEED_SCENE_HELIX);
            else if (concept == CONCEPT_SCENE_FLYING_BOX) sprintf(speedBuf, "%u ms", SPEED_SCENE_FLYING_BOX);
            else if (concept == CONCEPT_SCENE_MOVING_BOXES) sprintf(speedBuf, "%u ms", SPEED_SCENE_MOVING_BOXES);
            else if (concept == CONCEPT_SCENE_ROLLING_BALL) sprintf(speedBuf, "%u ms", SPEED_SCENE_ROLLING_BALL);
            else if (concept == CONCEPT_SCENE_BOUNCING_BALL) sprintf(speedBuf, "%u ms", SPEED_SCENE_BOUNCING_BALL);
            else if (concept == CONCEPT_SCENE_LAVA_LAMP) sprintf(speedBuf, "%.2fx", SPEED_SCENE_LAVA_LAMP);
            else if (concept >= CONCEPT_SCENE_TEXT_FYB && concept <= CONCEPT_SCENE_TEXT_ISRAEL) sprintf(speedBuf, "%u ms", SPEED_TEXT_ICONS);
            else if (concept == CONCEPT_SCENE_RUBIKS) sprintf(speedBuf, "Self-Timed");
            else if (concept == CONCEPT_SCENE_TETRIS) sprintf(speedBuf, "%lu sec", max((uint32_t)30, durationMs / 1000));
            
            Serial.printf("[%02d/%02d] Concept: %02d | Scene: %-15s | Shader: %-16s | Color: %-22s | Speed: %s\n", 
                currentStep, totalSteps, concept, sceneNames[concept], displayShader, displayColor, speedBuf);
        }

        if (concept < NUM_SHAPES) { 
            switch(motion) {
                case MOTION_STATIC:          Shapes::animateStatic(shape, durationMs, rMode, pal, sModeTracker, SPEED_MOTION_STATIC, stretch); break;
                case MOTION_EXPAND_COLLAPSE: Shapes::animateExpandingCollapsing(shape, durationMs, rMode, pal, sModeTracker, SPEED_MOTION_EXPAND_COLLAPSE, stretch); break;
                case MOTION_SPRING:          Shapes::animateSpring(shape, durationMs, rMode, pal, sModeTracker, SPEED_MOTION_SPRING, stretch); break;
                case MOTION_TUMBLING:        Shapes::animateTumbling(shape, durationMs, rMode, pal, sModeTracker, SPEED_MOTION_TUMBLING, stretch); break;
                case MOTION_SPINNING:        Shapes::animateSpinning(shape, durationMs, SPEED_MOTION_SPINNING, rMode, pal, sModeTracker, stretch); break;
            }
        } 
        else if (concept >= CONCEPT_SCENE_HINGE_DANCE && concept <= CONCEPT_SCENE_BOUNCE) { 
            if (concept == CONCEPT_SCENE_HINGE_DANCE) Planes::animateHingeDance(durationMs, SPEED_SCENE_HINGE_DANCE);
            else if (concept == CONCEPT_SCENE_SPLAT)  Planes::animateSplat(durationMs, SPEED_SCENE_SPLAT);
            else Planes::animateBounce(durationMs, SPEED_SCENE_BOUNCE);
        }
        else if (concept >= CONCEPT_SCENE_SADDLE && concept <= CONCEPT_SCENE_SINE_WAVE) { 
            if (concept == CONCEPT_SCENE_SADDLE) Surfaces::animateHyperbolicParaboloid(durationMs, SPEED_SCENE_SADDLE, (Orientation)random8(3), pal, sModeTracker, 15);
            else {
                #ifndef TARGET_2026_CUBOID
                Surfaces::animateSineWave(durationMs, (Orientation)random8(3), WAVE_RADIAL, 360, pal, sModeTracker, SPEED_SCENE_SINE_WAVE);
                #else
                Surfaces::animateSineWaveSplit2026(durationMs, 360, (SplitWaveMode2026)random8(4), pal, sModeTracker, SPEED_SCENE_SINE_WAVE);
                #endif
            }
        }
        else { 
            switch(concept) {
                case CONCEPT_SCENE_RAIN:           Streamers::animateRain(durationMs, random8(1, 4), SPEED_SCENE_RAIN, pal, sModeTracker); break;
                case CONCEPT_SCENE_CYCLONE:        Streamers::animateCyclone(durationMs, random8(2, 3), 0.04f, pal, sModeTracker, SPEED_SCENE_CYCLONE); break;
                case CONCEPT_SCENE_VERT_STREAMER:  Streamers::animateVerticalStreamer(durationMs, SPEED_SCENE_VERT_STREAMER, 0.1875f, pal, sModeTracker); break;
                case CONCEPT_SCENE_TORNADO:        Streamers::animateTornado(durationMs, 25, pal, SPEED_SCENE_TORNADO); break;
                case CONCEPT_SCENE_HULA:           Streamers::animateHula(durationMs, 20, pal, SPEED_SCENE_HULA); break;
                case CONCEPT_SCENE_RANDOM_FALL:    Streamers::animateRandomFall(durationMs, SPEED_SCENE_RANDOM_FALL, SPEED_SCENE_RANDOM_FALL * 2.0f); break;
                case CONCEPT_SCENE_FIREWORKS:      Streamers::animateFireworks(durationMs); break;
                case CONCEPT_SCENE_PIPES:          Streamers::animateVerticalPipes(durationMs, SPEED_SCENE_PIPES); break;
                case CONCEPT_SCENE_WILD_MOUSE:     Streamers::animateWildMouse(durationMs, SPEED_SCENE_WILD_MOUSE); break;
                case CONCEPT_SCENE_ATOM_SMASHER:   Streamers::animateAtomSmasher(durationMs); break;
                case CONCEPT_SCENE_WOBBLING_DISH:  Surfaces::animateWobblingDish(durationMs, false, SPEED_SCENE_WOBBLING_DISH); break;
                case CONCEPT_SCENE_HELIX:          Streamers::animateWhatTheHelix(durationMs, SPEED_SCENE_HELIX); break;
                case CONCEPT_SCENE_FLYING_BOX:     Shapes::animateFlyingBox(durationMs, SPEED_SCENE_FLYING_BOX); break;
                case CONCEPT_SCENE_MOVING_BOXES:   Shapes::animateMovingBoxes(durationMs, SPEED_SCENE_MOVING_BOXES); break;
                case CONCEPT_SCENE_ROLLING_BALL:   Shapes::animateRollingBall(durationMs, (RenderMode)(random8(100) < 30 ? RENDER_SHELL : RENDER_SOLID), pal, sModeTracker, SPEED_SCENE_ROLLING_BALL); break;
                case CONCEPT_SCENE_BOUNCING_BALL:  Shapes::animateBouncingBall(durationMs, SPEED_SCENE_BOUNCING_BALL); break;
                case CONCEPT_SCENE_LAVA_LAMP:      Streamers::animateLavaLamp(durationMs, SPEED_SCENE_LAVA_LAMP); break;

                // --- BILLBOARD DECK ---
                case CONCEPT_SCENE_TEXT_FYB: 
                    Letters::scrollTextCycles("Fuck Yer `27` Burn", 2, 1.0f, 1.0f, SPEED_TEXT_ICONS, CRGB::White, LavaColors_p, MODE_NOISE_FIELD, true, 75); break;
                case CONCEPT_SCENE_ICONS_CCW: 
                    Icons::scrollIcons("{(1.2.3),(4.5.6),(7.8.9.10),(11.12.13.14),(15.16.17.18),(19.20),21,22,23,24,25,26,27,28,30,31,32}", durationMs, SPEED_TEXT_ICONS, 120, false); break;
                case CONCEPT_SCENE_ICONS_CW: 
                    Icons::scrollIcons("{(1.2.3),(4.5.6),(7.8.9.10),(11.12.13.14),(15.16.17.18),(19.20),21,22,23,24,25,26,27,28,30,31,32}", durationMs, SPEED_TEXT_ICONS, 120, true); break;
                case CONCEPT_SCENE_TEXT_PIZZA: 
                    Letters::scrollTextCycles("`R30` KINKY PIZZA `R30`", 2, 1.0f, 1.0f, SPEED_TEXT_ICONS, CRGB::Gold, OceanColors_p, MODE_NOISE_FIELD, false, 75); break;
                case CONCEPT_SCENE_TEXT_CUBINA: 
                    Letters::scrollTextCycles("`(4.5.6)` CUBINA `C27`", 2, 1.0f, 1.0f, SPEED_TEXT_ICONS, CRGB::Cyan, LavaColors_p, MODE_NOISE_FIELD, false, 75); break;
                case CONCEPT_SCENE_TEXT_YES: 
                    Letters::scrollTextCycles("`25` Just Say Yes `24`", 2, 1.0f, 1.0f, SPEED_TEXT_ICONS, CRGB::White, ForestColors_p, MODE_NOISE_FIELD, false, 85); break;
                case CONCEPT_SCENE_TEXT_YEAH: 
                    Letters::scrollTextCycles("`C27` FUCK `23` YEAH! `28`", 2, 1.0f, 1.0f, SPEED_TEXT_ICONS, CRGB::White, LavaColors_p, MODE_NOISE_FIELD, false, 75); break;
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
    }
}

// =========================================================================================
// PUBLIC NAMESPACE IMPLEMENTATION
// =========================================================================================

namespace TestDemo {

    void runHardwareWiringTest() {
        Serial.println("\n--- STARTING FAST BOOT DIAGNOSTIC ---");
        CRGB testColors[8] = { CRGB::Red, CRGB::OrangeRed, CRGB::Yellow, CRGB::Green, CRGB::Blue, CRGB::Magenta, CRGB::DarkTurquoise, CRGB::White };

        Serial.println("Test 1: Full Rainbow Hold (5 Seconds)...");
        clearAll(); 
        for (uint8_t x = 0; x < RNDR_X; x++) {
            CRGB planeColor = testColors[x % 8];
            for (uint8_t y = 0; y < RNDR_Y; y++) {
                for (uint8_t z = 0; z < RNDR_Z; z++) setVoxel(x, y, z, planeColor); 
            }
        }
        showCube(); delay(5000); 

        Serial.println("Test 2: Sweeping X-Planes (Left to Right)...");
        for (uint8_t x = 0; x < RNDR_X; x++) {
            clearAll(); CRGB planeColor = testColors[x % 8];
            for (uint8_t y = 0; y < RNDR_Y; y++) {
                for (uint8_t z = 0; z < RNDR_Z; z++) setVoxel(x, y, z, planeColor); 
            }
            showCube(); delay(600); 
        }
        clearAll(); showCube();
        Serial.println("--- BOOT DIAGNOSTIC COMPLETE ---\n");
    }

    void runEngine(bool isTestMode) {
        /*const int ART_DECK_REPETITIONS       = 3;  //about 40 minutes
        const int BILLBOARD_DECK_REPETITIONS = 2;  //about 5 minutes
        const uint32_t DURATION_ART_MS       = 20000;  
        const uint32_t DURATION_BILLBOARD_MS = 20000;  
        */
        const int ART_DECK_REPETITIONS       = 1;  
        const int BILLBOARD_DECK_REPETITIONS = 1;  
        const uint32_t DURATION_ART_MS       = 30000;  
        const uint32_t DURATION_BILLBOARD_MS = 20000;  
       
        uint32_t durationMs;


        uint8_t concept = 0;
        int currentStep = 0, totalSteps = 0;

        if (isTestMode) {
            while (seqConcept < TOTAL_CONCEPTS) {
                bool enabled = (seqConcept < NUM_SHAPES) ? SHAPE_RULES[seqConcept].enabled : SCENE_RULES[seqConcept - NUM_SHAPES].enabled;
                if (enabled) break;
                seqConcept++;
            }
            if (seqConcept >= TOTAL_CONCEPTS) {
                seqConcept = 0;
                Serial.println("\n=========================================================================================");
                Serial.println("   END OF TEST CYCLE. RESTARTING INDEPENDENT AXIS EXHAUSTION");
                Serial.println("=========================================================================================");
                while (seqConcept < TOTAL_CONCEPTS) {
                    bool enabled = (seqConcept < NUM_SHAPES) ? SHAPE_RULES[seqConcept].enabled : SCENE_RULES[seqConcept - NUM_SHAPES].enabled;
                    if (enabled) break;
                    seqConcept++;
                }
            }
            concept = seqConcept;
            durationMs = 10000;
            currentStep = seqConcept + 1;
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
                        
                        for (int i = billboardDeckSize - 1; i > 0; i--) {            
                            int j = random8(i + 1); uint8_t temp = billboardDeck[i]; billboardDeck[i] = billboardDeck[j]; billboardDeck[j] = temp;
                        }
                        Serial.printf("\n=======================================\n   SHUFFLING THE BILLBOARD DECK (Play %d of %d)! \n=======================================\n", currentBillboardRep, BILLBOARD_DECK_REPETITIONS);
                    } else {
                        // Loop Art
                        artIndex = 0;
                        for (int i = artDeckSize - 1; i > 0; i--) {            
                            int j = random8(i + 1); uint8_t temp = artDeck[i]; artDeck[i] = artDeck[j]; artDeck[j] = temp;
                        }
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
                        
                        for (int i = artDeckSize - 1; i > 0; i--) {            
                            int j = random8(i + 1); uint8_t temp = artDeck[i]; artDeck[i] = artDeck[j]; artDeck[j] = temp;
                        }
                        Serial.printf("\n=======================================\n   SHUFFLING THE ART DECK (Play %d of %d)! \n=======================================\n", currentArtRep, ART_DECK_REPETITIONS);
                    } else {
                        // Loop Billboard
                        billboardIndex = 0;
                        for (int i = billboardDeckSize - 1; i > 0; i--) {            
                            int j = random8(i + 1); uint8_t temp = billboardDeck[i]; billboardDeck[i] = billboardDeck[j]; billboardDeck[j] = temp;
                        }
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
            
            // ==========================================
            // THE SURGICAL BOX SHUFFLER STATE
            // ==========================================
            static CRGBPalette16 boxPalettes[5] = {RainbowColors_p, LavaColors_p, ForestColors_p, OceanColors_p, PartyColors_p};
            static const char* boxPaletteNames[5] = {"Rainbow", "Lava", "Forest", "Ocean", "Party"};
            
            if (motion == MOTION_STATIC) {
                ShaderMode flow[] = {MODE_UNIFORM_CYCLE, MODE_LINEAR_FLOW, MODE_NOISE_FIELD, MODE_SCATTER, MODE_DISTANCE_FIELD, MODE_HIPHOTIC};
                if (shape == SHAPE_BOX) { 
                    uint8_t step = states[concept].shaderTracker % 6;
                    sModeTracker = flow[step]; 
                    
                    // Trigger a private shuffle at the start of every 6-step Box cycle
                    if (step == 0) {
                        for (int i = 4; i > 0; i--) {
                            int j = random8(i + 1);
                            CRGBPalette16 tempPal = boxPalettes[i]; boxPalettes[i] = boxPalettes[j]; boxPalettes[j] = tempPal;
                            const char* tempName = boxPaletteNames[i]; boxPaletteNames[i] = boxPaletteNames[j]; boxPaletteNames[j] = tempName;
                        }
                    }
                } else {
                    sModeTracker = flow[random8(6)];
                }
            } else {
                if (allowSolid && random8(2) == 0) sModeTracker = MODE_SOLID;
                else {
                    ShaderMode comp[] = {MODE_UNIFORM_CYCLE, MODE_SPATIAL_GRADIENT, MODE_LINEAR_FLOW, MODE_NOISE_FIELD, MODE_SCATTER, MODE_DISTANCE_FIELD, MODE_HIPHOTIC};
                    sModeTracker = comp[random8(7)];
                }
            }
            
            pal = PaletteUtils::getSmartPalette(sModeTracker);
            
            // ==========================================
            // THE BOX PALETTE ENFORCER
            // ==========================================
            if (shape == SHAPE_BOX) {
                uint8_t step = states[concept].shaderTracker % 6;
                
                if (sModeTracker == MODE_UNIFORM_CYCLE) { 
                    pal = RainbowColors_p; 
                    strcpy(PaletteUtils::getPaletteNameBuffer(), "Rainbow"); 
                }
                else if (sModeTracker == MODE_HIPHOTIC) {
                    strcpy(PaletteUtils::getPaletteNameBuffer(), "Hiphotic Native");
                }
                else {
                    // Steps 1, 2, 3, and 4 grab the first 4 palettes from the freshly shuffled pool
                    int palIndex = step - 1; 
                    pal = boxPalettes[palIndex];
                    strcpy(PaletteUtils::getPaletteNameBuffer(), boxPaletteNames[palIndex]);
                }
                
                // Increment tracker AFTER assignment is complete
                states[concept].shaderTracker++; 
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
            bool isSelfManaged = (concept >= CONCEPT_SCENE_HINGE_DANCE && concept <= CONCEPT_SCENE_BOUNCE) || 
                                 (concept >= CONCEPT_SCENE_RANDOM_FALL && concept <= CONCEPT_SCENE_HELIX) ||
                                 (concept == CONCEPT_SCENE_FLYING_BOX) || (concept == CONCEPT_SCENE_MOVING_BOXES) ||
                                 (concept >= CONCEPT_SCENE_TEXT_FYB && concept <= CONCEPT_SCENE_TETRIS);

            if (isSelfManaged) { sModeTracker = MODE_SOLID; pal = PartyColors_p; } 
            else {
                bool allowSolid = (concept != CONCEPT_SCENE_HULA && concept != CONCEPT_SCENE_VERT_STREAMER && concept != CONCEPT_SCENE_TORNADO) && (random8(100) < 25);
                if (concept == CONCEPT_SCENE_SADDLE || concept == CONCEPT_SCENE_SINE_WAVE || concept == CONCEPT_SCENE_ROLLING_BALL || concept == CONCEPT_SCENE_BOUNCING_BALL) { 
                    if (allowSolid && random8(2) == 0) sModeTracker = MODE_SOLID;
                    else { ShaderMode comp[] = {MODE_UNIFORM_CYCLE, MODE_SPATIAL_GRADIENT, MODE_LINEAR_FLOW, MODE_NOISE_FIELD, MODE_SCATTER, MODE_DISTANCE_FIELD, MODE_HIPHOTIC}; sModeTracker = comp[random8(7)]; }
                } 
                else if (concept == CONCEPT_SCENE_SINE_WAVE) {
                    // ISOLATED: De-seizured pool for Sine Wave (Removed Noise & Distance Fields)
                    if (allowSolid && random8(2) == 0) sModeTracker = MODE_SOLID;
                    else { ShaderMode comp[] = {MODE_UNIFORM_CYCLE, MODE_SPATIAL_GRADIENT, MODE_LINEAR_FLOW, MODE_SCATTER, MODE_HIPHOTIC}; sModeTracker = comp[random8(5)];} 
                }
                else { 
                    if (allowSolid) sModeTracker = MODE_SOLID; 
                    else { ShaderMode flow[] = {MODE_UNIFORM_CYCLE, MODE_LINEAR_FLOW, MODE_NOISE_FIELD, MODE_SCATTER}; sModeTracker = flow[random8(4)]; }
                }
                if (concept == CONCEPT_SCENE_TORNADO || concept == CONCEPT_SCENE_WILD_MOUSE || concept == CONCEPT_SCENE_HULA || concept == CONCEPT_SCENE_LAVA_LAMP) pal = PaletteUtils::getRandomOrganicPalette();
                else pal = PaletteUtils::getSmartPalette(sModeTracker);
            }        
        }

        renderScene(concept, shape, motion, rMode, sModeTracker, pal, stretch, durationMs, isTestMode, currentStep, totalSteps);

        if (isTestMode) {
            if (concept < NUM_SHAPES && states[concept].motionMask != 0) { /* Holding */ }
            else seqConcept++; 
        }
    }
}