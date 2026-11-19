# 16Cube / Cubina

An ESP32 volumetric LED system with ambient animations, audio-reactive art,
marquee graphics, and games. A command board acquires audio and gamepad input;
a separate render board runs the graphics engine and drives the LEDs.

The desktop prototype renders 16×16×16 voxels. A separate Dig-Octa/Burning Man
wiring map and a 10×10×16 render canvas support the larger installation.
Wiring topology and render dimensions are independent configuration choices.

## Documentation

Start with the [system documentation index](docs/README.md). It links the
architecture, data flow, hardware variants, graphics and particle engines,
palette policy, complete deck catalog, development workflow, and current state.

- [Architecture and ownership](docs/architecture/SYSTEM.md)
- [Data flow and audio contract](docs/architecture/DATA_AND_AUDIO.md)
- [Desktop and Burning Man hardware](docs/architecture/HARDWARE.md)
- [Graphics engine](docs/architecture/GRAPHICS.md)
- [Palettes and shaders](docs/architecture/PALETTES.md)
- [Animation play order and catalog](docs/ANIMATIONS.md)
- [Build, flash, monitor, and test](docs/DEVELOPMENT.md)
- [Current state and deferred work](docs/STATE.md)

## Quick start

Run from the repository root with PlatformIO installed:

```sh
~/.platformio/penv/bin/platformio run -d render-board
~/.platformio/penv/bin/platformio run -d command-board
PYTHONDONTWRITEBYTECODE=1 bash tests/check.sh
```

The checked-in configuration selects desktop wiring, the full 16³ canvas,
expanded WLED palettes, and the board-to-board UART receiver. Boot runs the
wiring diagnostic, then Ambient Art. Ambient playback currently holds each
card until Next; audio has its own descending deck and 30-second timer.
See the development guide before uploading to a different board or hub socket.

## Status

Aurora remains a full-size-cube audition: confirm that thin curtains and faint
vertical wisps read clearly through depth at ordinary viewing angles. PS GEQ
remains experimental. See [the state page](docs/STATE.md) for other limitations.

## Licensing

The project license is [MIT](LICENSE). Imported WLED palette data carries its
upstream attribution and [EUPL-1.2 license](docs/licenses/WLED-EUPL-1.2.txt).
