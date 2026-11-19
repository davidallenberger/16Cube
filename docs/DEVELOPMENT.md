# Build, flash, monitor, and maintain

[Documentation index](README.md) · [Hardware](architecture/HARDWARE.md)

## Check the target first

There are two PlatformIO projects, not one root firmware project. Commands
below assume the repository root and the usual installed CLI path. If `pio`
or `platformio` is already on PATH, the shorter command can replace that path.
The CLI runs from any terminal; board selection depends on project directory,
configuration, and hub location, not which terminal opened it.

```sh
~/.platformio/penv/bin/platformio run -d render-board
~/.platformio/penv/bin/platformio run -d command-board
```

Render pins `espressif32@6.5.0`, `FastLED@3.10.3`, four I2S DMA buffers,
interrupts enabled, and C++ exceptions enabled. The I2S driver define lives
before the first FastLED include in CubeCore.h. Build-wide FastLED flags must
also reach FastLED's own compilation units; keep DMA configuration in the
PlatformIO build flags.

Command uses its own `espressif32` selection, FastLED 3.6.0, arduinoFFT ^2.0.2,
and the Bluepad32 Arduino framework ZIP from an upstream `main` branch. That
framework URL is **not pinned to an immutable release**. A clean dependency
cache can therefore change the command toolchain. Do not casually update it
while comparing behavior. Switching projects can cause PlatformIO to download
the appropriate framework/esptool packages again; it is not evidence of a
firmware edit. Check the resolved versions in the build output.

## Stable board selection

List attached devices and confirm the physical role/socket mapping:

```sh
~/.platformio/penv/bin/platformio device list
~/.platformio/penv/bin/platformio run -d render-board -t check_upload_port
~/.platformio/penv/bin/platformio run -d command-board -t check_upload_port
```

The last two targets print the resolved uploader command without opening the
serial device. [`tools/select_upload_port.py`](../tools/select_upload_port.py)
selects exactly one CP2102 by VID/PID and configured USB location when the
uploader command is expanded. Missing/ambiguous matches fail; the deliberate
invalid `upload_port` fallback prevents silent guessing.

Current assignments: render `0-1.3`, command `0-1.4`. These identify USB topology,
not silicon identity. Keep each board in its verified hub socket; moving the
hub to another upstream connection can change the location path. Verify by
board-specific startup diagnostics if mapping is uncertain. Both adapters
may share serial `0001`, and `cu.usbserial-*` names can exchange roles.

```sh
~/.platformio/penv/bin/platformio run -d render-board -t upload
~/.platformio/penv/bin/platformio run -d command-board -t upload
```

For dynamic monitoring from any directory:

```sh
# Render
~/.platformio/penv/bin/platformio device monitor --port 'hwgrep://VID:PID=10C4:EA60.*LOCATION=0-1[.]3$' --baud 115200
# Command
~/.platformio/penv/bin/platformio device monitor --port 'hwgrep://VID:PID=10C4:EA60.*LOCATION=0-1[.]4$' --baud 115200
```

The same monitor expressions are checked into each `platformio.ini`. Close a
monitor before uploading if it holds the port. USB logging uses 115200, while
the inter-board binary link uses 921600. Never write diagnostic text to Serial2.

## Configuration reference

| Setting | Location / checked-in value | Purpose |
| --- | --- | --- |
| Wiring and canvas | CubeCore.h: Desktop, 16³ | Independent physical map and rendering dimensions |
| Brightness | CubeCore.cpp: Desktop 32, BurnCube 64 | Global output scale |
| UART receiver | Render build flag `CUBINA_ENABLE_UART_RECEIVER=1` | 0 compiles out receiver/task/mutex/parser and gives neutral inputs |
| Spacer checks | Render build flag `CUBINA_CHECK_SPACERS=0` | Optional desktop unmapped-pixel diagnostics; enabling also forces shuffled ambient order |
| WLED palette catalog | Render build flag `INCLUDE_WLED_PALETTES=1` | 0 returns to classic picker pools |
| Color diagnostics | Render build flag `CUBINA_LOG_ANIMATION_COLORS=1` | Selection-time actual materials |
| Ambient ordering | TestDemo.cpp `animationOrder()` | Descending; alternatives Sequential/Shuffled |
| Ambient manual hold | TestDemo.h `animationHoldUntilNext()=true` | Keeps selected animation until Next |
| Audio timing/order | TestDemo.h `AudioPlaybackSettings` | Descending, 30000 ms; 0 means hold |
| Effect gains | Audio.h `Audio::Controls` | Artistic response on render |
| Source calibration | SignalAcquisition.cpp `ui*` controls | Mic/ADC scaling, gates, smoothing, detector sensitivity |
| Gamepad Bluetooth | Command BoardConfig.h, default enabled | Compile-time Bluepad32 inclusion |

Header fallback values are not always the actual project build values. In
particular the receiver and imported-palette headers default off while the
render build explicitly turns them on.

## Validation

For transport, state, mode buttons, audio analysis, event/tempo interpretation,
playback, and selected renderer regressions:

```sh
PYTHONDONTWRITEBYTECODE=1 bash tests/check.sh
PYTHONDONTWRITEBYTECODE=1 python3 tests/render_regression_test.py
PYTHONDONTWRITEBYTECODE=1 python3 tests/vertical_streamer_test.py
```

The suite compiles production modules or extracted production bodies against
host API stubs; some runs use AddressSanitizer/UndefinedBehaviorSanitizer.
It tests real protocol encoding/decoding, sender timing, receiver freshness and
ordering, frame-frozen events, clock behavior, Next/Hold/source preservation,
physical mapping/spacers, Julia geometry, and selected canvas-scaled drawing.
The rendering scripts cover specific effects, not the entire catalog.
`tempo_waveform_suite.py` provides additional generated-signal tempo exercises.
Read the script when selecting a narrower check.

Host syntax checks do not validate ESP32 driver/task timing or the full graphics
integration. Firmware compilation does not establish visual quality. Hardware
audition must cover the actual changed scene, Next during/after it, source
switches, silence, and relevant larger-canvas behavior. For a shared particle
engine change, regress its consumers together: Impact, Volcano, Waterfall,
Fire, Box, Ghost Rider, Spray, Attractor, Galaxy, PS GEQ, Fireworks, Atom
Smasher, and Smashing Planes as applicable to the changed helper.

## Diagnosing failures

| Symptom | First checks |
| --- | --- |
| Wrong board flashed/monitored | Location-to-role mapping, resolver output, occupied serial port; do not infer role from `…-0001` |
| Same scene restarts on Next | Session/revision log, cursor policy, between-app checkpoint; distinguish Box texture steps from actual scene repeats |
| Audio mode opens but signal is absent | Command source readiness, I2S error/slot logs, physical ADC/mic selection, wire version/baud, render accepted/rejected packets |
| Audio-specific hang while ambient works | First selected audio effect, bounds, stack/arena allocation, callback/continuation; isolate cards rather than assuming wiring changed |
| Stack canary panic | Decode against the exact flashed ELF; inspect nested frame-local arrays and effect transition stack usage |
| Dim/sparse display | Raw vs display bins, noise floor, gain, palette darkness, spatial coverage, effect fade, global brightness |
| Low frame rate | Separate sim/draw/show time, catch-up/missed steps, pool occupancy/rejections; inspect Fire timing output |
| Unused desktop wiring LEDs light | Enable spacer diagnostics and identify scene/material; inspect any physical-array writes bypassing voxel mapping |

Use the matching `render-board/.pio/build/esp32dev/firmware.elf` for address
translation with the installed ESP32 `addr2line`; rebuilding first can make
addresses refer to different code. Preserve a panic's ELF hash and binary.

## Adding or changing an effect

1. Choose the family file and reuse geometry, TextureState/pickers, particle
   helpers, and the existing audio frame API. Add a reusable primitive to
   GeometryEngine when necessary; do not clone it into a scene.
2. Keep motion, geometry, and materials independently understandable. Derive
   dimensions and density from the render canvas. Particle capacity remains
   bounded by the shared arena even when a desired population scales up.
3. Register ambient effects through the concept/name/rule/ordered tables in
   TestDemo; register audio effects in TestDemoAudio. Keep disabled auditions
   explicit and use a separate card for comparison experiments.
4. Respect no-solid and dark-palette permissions. Print actual material choice
   once when selected, keeping per-object metadata for independent objects.
5. Preserve checkpoint calls, C++ unwinding, duration/Hold/Next, frame-frozen
   input, and source independence. Never introduce another input task/parser
   or mutate render controls from a UART task.
6. Validate the affected shared consumers and canvases, audition on hardware,
   update these docs/catalog/state, then commit source and useful tests/docs.

Generated `.pio`, editor settings, `.DS_Store`, and Python cache files are not
part of a source checkpoint merely because they appear in `git status`.
