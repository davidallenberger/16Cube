# System documentation

This is the current-state guide to 16Cube/Cubina for maintainers, artists, and
AI coding agents. It describes the implementation at source checkpoint
`f7840b540a18e69876e9b73ab6e67da266285dc8`, reviewed on 2026-11-19. These
pages are a specification of that snapshot, not a claim that every available
hardware combination has been commissioned. Update the relevant page when
changing a contract, default, deck membership, or wiring configuration.

## Read in this order

| Document | Purpose |
| --- | --- |
| [Architecture](architecture/SYSTEM.md) | Board responsibilities, tasks, application lifecycle, memory, and source map |
| [Data and audio](architecture/DATA_AND_AUDIO.md) | UART contract, freshness/events, PCM analysis, and the animation input API |
| [Hardware variants](architecture/HARDWARE.md) | Desktop versus Burning Man wiring, canvas, output, and USB identity |
| [Graphics](architecture/GRAPHICS.md) | Geometry, textures, rotation, antialiasing, particles, and brightness |
| [Palettes](architecture/PALETTES.md) | Central pickers, shader restrictions, dark palettes, and diagnostics |
| [Animation catalog](ANIMATIONS.md) | Actual ambient/audio play order, combined cards, disabled experiments |
| [Development](DEVELOPMENT.md) | Build/monitor commands, validation, commissioning, and change discipline |
| [State and open work](STATE.md) | Current defaults, accepted work, auditions, and unfinished integrations |

[AUDIO_ANIMATIONS.md](AUDIO_ANIMATIONS.md) is the earlier audio development
journal. It contains successive experiments and superseded defaults. Use the
current-state documents above and the referenced source for operational
answers; use the journal to understand how a decision evolved.

## Authority and boundaries

The checked-in source and each board's `platformio.ini` determine behavior.
This guide links to those files rather than treating old chat descriptions,
commented demonstration calls, or an upstream WLED GIF as executable policy.
WLED-inspired effects have intentionally diverged in several places to work
in a sparse physical 3D display. “PS” identifies their lineage; it does not
mean this repository embeds WLED's entire particle engine or UI.

Documentation paths are repository-relative so the guide survives cloning to
another machine. Runtime serial device names are discovered, not permanent
board identities. Physical power capacity, final PCB pin assignments, and
large-cube visual acceptance require hardware evidence.
