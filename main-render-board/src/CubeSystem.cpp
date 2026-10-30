#include "CubeSystem.h"
#include "AppMemory.h"
#include "TestDemo.h"
#include "Shapes.h"
#include "Audio.h"
#include "Depth.h"
#include "UARTReceiver.h"
#include "Tetris.h"
#include "Tetris2d.h"
#include "Rubiks.h"
#include "Breakout.h"
#include "Breakout2d.h"
#include "Snake.h"
#include "Snake2d.h"
#include "Asteroids3d.h"

SystemState CubeSystem::currentState = SYS_STATE_RUN_APP;
int16_t CubeSystem::selectedMenuIndex = 0;
uint32_t CubeSystem::stateTimer = 0;

// handleApp() establishes the return point in this buffer using setjmp().
// Only the animation task may jump back to that point. The UART task must
// never use this buffer: it has a different call stack.
jmp_buf engineJumpBuffer;

// True only while an app is running inside handleApp()'s active return point.
// Checkpoints also run in the menu, where no app escape is needed.
bool teleportArmed = false;

namespace {
    bool menuNeedsRedraw = true;

    // These values remember the LAST command-board demand we have acted on.
    // They are owned by the animation task; the receiver does not write them.
    //
    // appliedControl: Have we accepted any remote demand since render boot?
    // appliedSession: Which boot of the command board issued that demand?
    //                 The command board chooses a new session ID on reboot.
    // appliedRevision: Which mode-button demand within that boot was applied?
    //                  A new button demand increments the revision, even if
    //                  it requests the same mode again.
    //
    // Repeated UART packets carry the same session/revision. Remembering this
    // pair stops each heartbeat or audio packet from restarting the app.
    bool appliedControl = false;
    uint32_t appliedSession = 0;
    uint32_t appliedRevision = 0;

    // The command board's last accepted mode. This also determines whether
    // a held gamepad Home button is allowed to return to the game menu.
    CubinaWire::Mode demandedMode = CubinaWire::Mode::Animation;

    void runAmbientArt () {
        for (;;) {
            TestDemo::runEngine (false);
        }
    }

    void renderAmbientIcon (TextureState&) {
        Shapes::animateStatic (
            SHAPE_MERKABA,
            10,
            RENDER_SOLID,
            MODE_SPATIAL_GRADIENT
        );
    }

    void runDiagnostics () {
        for (;;) {
            CubeSystem::handlePairingSandbox ();
        }
    }
}

static void renderDiagIcon (TextureState& tex) {
    uint8_t pulse = beatsin8 (50, 20, 255);
    CRGB col = CRGB (pulse, 0, 0);

    for (int z = 5; z <= 10; z++) {
        setVoxel (3, 3, z, col);
        setVoxel (4, 3, z, col);
        setVoxel (3, 4, z, col);
        setVoxel (4, 4, z, col);
    }

    for (int x = 1; x <= 6; x++) {
        if (x == 3 || x == 4) {
            continue;
        }

        setVoxel (x, 3, 7, col);
        setVoxel (x, 4, 7, col);
        setVoxel (x, 3, 8, col);
        setVoxel (x, 4, 8, col);
    }

    for (int y = 1; y <= 6; y++) {
        if (y == 3 || y == 4) {
            continue;
        }

        setVoxel (3, y, 7, col);
        setVoxel (4, y, 7, col);
        setVoxel (3, y, 8, col);
        setVoxel (4, y, 8, col);
    }
}

// Registry positions are also used by the mode-to-app mapping below.
// If entries are reordered, update AUDIO_APP, DEPTH_APP, and the game default.
static const CubeApp appRegistry[] = {
    {"Ambient Art", renderAmbientIcon, runAmbientArt},
    {"Tetris 3D", Tetris::renderIcon, Tetris::run},
    {"Tetris 2D", Tetris2d::renderIcon, Tetris2d::run},
    {"Asteroids 3D", Asteroids3d::renderIcon, Asteroids3d::run},
    {"Breakout 3D", Breakout::renderIcon, Breakout::run},
    {"Breakout 2D", Breakout2d::renderIcon, Breakout2d::run},
    {"Snake 3D", Snake::renderIcon, Snake::run},
    {"Snake 2D", Snake2d::renderIcon, Snake2d::run},
    {"Rubiks", Rubiks::renderIcon, Rubiks::run},
    {"Diagnostics", renderDiagIcon, runDiagnostics},
    {"Audio", Audio::renderIcon, Audio::run},
    {"Depth Occupancy", Depth::renderIcon, Depth::run}
};

static constexpr int AUDIO_APP = 10;
static constexpr int DEPTH_APP = 11;

void CubeSystem::setup () {
    // The render board consumes UART input. Bluetooth pairing and acquisition
    // belong to the command board; no Bluepad32/NVS setup is performed here.
    RemoteInputs::begin ();
    initAppMemory ();

    selectedMenuIndex = 0;
    changeState (SYS_STATE_RUN_APP);

    // Normal boot starts Ambient Art, including when no UART is connected.
    // Run TestDemo::runHardwareWiringTest() explicitly when commissioning.
}

void CubeSystem::changeState (SystemState s) {
    currentState = s;
    stateTimer = millis ();
    menuNeedsRedraw = true;
    clearAll ();

    Serial.printf ("Cube OS state=%u app=%d\n", unsigned (s), selectedMenuIndex);
}

void CubeSystem::frameCheckpoint () {
    // This is the ANIMATION TASK's checkpoint, not the UART receive loop.
    //
    // UART task: validate packets and store their latest values under a mutex.
    // Animation task: copy those values here, choose the app, and escape from
    // the old app if needed. longjmp must stay on the task that called setjmp.
    //
    // control() returns a value copy and releases its internal mutex before
    // returning. Consequently no receiver mutex is held when we jump below.
    const RemoteInputs::Control c = RemoteInputs::control ();

    // A linked command is new to us if this is the first one, the command
    // board rebooted (session changed), or another mode demand was issued
    // (revision changed). Ordinary packets repeating the same pair do nothing.
    if (c.linked &&
        (!appliedControl ||
         c.session != appliedSession ||
         c.revision != appliedRevision)) {

        // Mark the demand as handled BEFORE escaping the old app. Otherwise
        // the next checkpoint would see it as new and jump again forever.
        appliedControl = true;
        appliedSession = c.session;
        appliedRevision = c.revision;
        demandedMode = c.mode;

        // Command-board mode buttons have authority over the current app.
        // All audio sources share the Puddles app; acquisition chooses which
        // source supplies its audio. Game mode opens the local game menu.
        if (CubinaWire::audioMode (c.mode)) {
            selectedMenuIndex = AUDIO_APP;
            changeState (SYS_STATE_RUN_APP);
        } else if (c.mode == CubinaWire::Mode::Depth) {
            selectedMenuIndex = DEPTH_APP;
            changeState (SYS_STATE_RUN_APP);
        } else if (c.mode == CubinaWire::Mode::Game) {
            selectedMenuIndex = 5; // Initially highlight Breakout 2D.
            changeState (SYS_STATE_MENU);
        } else {
            selectedMenuIndex = 0;
            changeState (SYS_STATE_RUN_APP);
        }

        // Return to handleApp() with code 2. It leaves the state selected
        // above intact, so the OS can launch the newly demanded mode.
        // In a menu/between apps, no jump is needed: the normal loop sees it.
        if (teleportArmed) {
            teleportArmed = false;
            longjmp (engineJumpBuffer, 2);
        }
    }

    // Publish one frame's gamepad inputs, including latched short presses.
    // The accessors manage their own mutexes; app code sees copied values.
    RemoteInputs::beginFrame ();

    static uint32_t homeStart = 0;
    static bool homeConsumed = false;

    // Home uses actual held state, rather than a latched momentary press,
    // because returning to the menu requires holding it for 500 ms.
    const GamepadState p1 = RemoteInputs::heldGamepad (ROLE_MASTER);
    const GamepadState p2 = RemoteInputs::heldGamepad (ROLE_PLAYER_2);
    const bool home =
        (p1.connected && (p1.miscButtons & CubinaInput::System)) ||
        (p2.connected && (p2.miscButtons & CubinaInput::System));

    // Only System/Home starts this timer. Select, Start, and Capture remain
    // available to games and cannot accidentally send the player to the menu.
    if (!home) {
        homeStart = 0;
        homeConsumed = false;
    } else if (demandedMode == CubinaWire::Mode::Game) {
        if (!homeStart) {
            homeStart = millis ();
        }

        if (!homeConsumed && uint32_t (millis () - homeStart) >= 500) {
            // Consume before jumping: a continuously held button must not
            // trigger another escape immediately when the menu appears.
            homeConsumed = true;

            if (teleportArmed) {
                teleportArmed = false;
                longjmp (engineJumpBuffer, 1);
            }
        }
    }
}

void CubeSystem::loop () {
    // Catch remote demands between app calls as well. While an app is inside
    // its own loop, showCube() supplies the checkpoint for each shown frame.
    if (currentState == SYS_STATE_RUN_APP) {
        frameCheckpoint ();
    }

    switch (currentState) {
        case SYS_STATE_BOOT_PAIRING:
            handlePairingSandbox ();
            break;

        case SYS_STATE_MENU:
            handleMenu ();
            break;

        case SYS_STATE_RUN_APP:
            handleApp ();
            break;
    }
}

void CubeSystem::handleMenu () {
    GamepadState p1 = RemoteInputs::gamepad (ROLE_MASTER);
    GamepadState p2 = RemoteInputs::gamepad (ROLE_PLAYER_2);
    static uint32_t lastInput = 0;

    // The Game-mode menu offers games and diagnostics (indices 1 through 9).
    // It does not change the audio source or the command board's chosen mode.
    if (uint32_t (millis () - lastInput) > 200) {
        bool down = (p1.dpad & (CubinaInput::DpadDown | CubinaInput::DpadRight)) ||
                    p1.axisY > 200 || p1.axisX > 200 ||
                    (p2.dpad & (CubinaInput::DpadDown | CubinaInput::DpadRight)) ||
                    p2.axisY > 200 || p2.axisX > 200;

        bool up = (p1.dpad & (CubinaInput::DpadUp | CubinaInput::DpadLeft)) ||
                  p1.axisY < -200 || p1.axisX < -200 ||
                  (p2.dpad & (CubinaInput::DpadUp | CubinaInput::DpadLeft)) ||
                  p2.axisY < -200 || p2.axisX < -200;

        bool select = p1.a || p1.b || p1.x || p1.y ||
                      p2.a || p2.b || p2.x || p2.y;

        if (down || up) {
            selectedMenuIndex += down ? 1 : -1;

            if (selectedMenuIndex > 9) {
                selectedMenuIndex = 1;
            }

            if (selectedMenuIndex < 1) {
                selectedMenuIndex = 9;
            }

            lastInput = millis ();
            menuNeedsRedraw = true;
        } else if (select) {
            lastInput = millis ();
            changeState (SYS_STATE_RUN_APP);
            return;
        }
    }

    if (menuNeedsRedraw) {
        clearAll ();
        static TextureState tex (MODE_UNIFORM_CYCLE, RainbowColors_p);
        appRegistry[selectedMenuIndex].renderIcon (tex);
        menuNeedsRedraw = false;
    }

    // Even an unchanged icon must service remote mode demands regularly.
    showCube ();
}

void CubeSystem::handleApp () {
    clearAll ();

    // Establish this task's escape destination BEFORE calling the app.
    // setjmp returns 0 initially; a later longjmp makes it return 1 or 2.
    //
    // This legacy mechanism bypasses normal stack unwinding. App code must
    // not hold mutexes or automatic objects requiring nontrivial destruction
    // across showCube()/frameCheckpoint(), since longjmp skips that cleanup.
    switch (setjmp (engineJumpBuffer)) {
        case 1:
            // A held gamepad Home button requested the game menu.
            teleportArmed = false;
            changeState (SYS_STATE_MENU);
            return;

        case 2:
            // A command-board mode demand already selected the next state.
            teleportArmed = false;
            return;

        default:
            break;
    }

    teleportArmed = true;
    Serial.printf ("Launching %s\n", appRegistry[selectedMenuIndex].name);
    appRegistry[selectedMenuIndex].run ();

    // An app can also return normally, without going through the checkpoint.
    teleportArmed = false;
    changeState (SYS_STATE_MENU);
}

void CubeSystem::drawNumberOnFaces (uint8_t num, TextureState& tex) {
    uint8_t font[3][5] = {
        {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
        {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
        {0b111, 0b001, 0b111, 0b100, 0b111}  // 2
    };
    uint8_t zOffset = 5;

    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 3; x++) {
            if (font[num][4 - y] & (1 << (2 - x))) {
                CRGB color = tex.getColor (x, y, zOffset);

                // Center the three-pixel-wide digit; mirrored coordinates
                // make the digit readable on the opposing cube faces.
                int offset = (RNDR_X - 3) / 2;
                int fx = offset + (2 - x);
                int inv_fx = offset + x;

                setVoxel (fx, 0, y + zOffset, color);
                setVoxel (inv_fx, RNDR_Y - 1, y + zOffset, color);
                setVoxel (0, inv_fx, y + zOffset, color);
                setVoxel (RNDR_X - 1, fx, y + zOffset, color);
            }
        }
    }
}

void CubeSystem::handlePairingSandbox () {
    // The historical name is retained, but this now displays received
    // controller state. Pairing itself happens only on the command board.
    const GamepadState p1 = RemoteInputs::gamepad (ROLE_MASTER);
    const GamepadState p2 = RemoteInputs::gamepad (ROLE_PLAYER_2);

    clearAll ();
    static TextureState tex (MODE_UNIFORM_CYCLE, RainbowColors_p);
    drawNumberOnFaces (unsigned (p1.connected) + unsigned (p2.connected), tex);

    if (p1.connected) {
        if (p1.a) {
            setVoxel (0, 0, 0, CRGB::Blue);
        }

        if (p1.b) {
            setVoxel (1, 0, 0, CRGB::Red);
        }

        if (p1.x) {
            setVoxel (2, 0, 0, CRGB::Green);
        }

        if (p1.y) {
            setVoxel (3, 0, 0, CRGB::Yellow);
        }
    }

    showCube ();
}
