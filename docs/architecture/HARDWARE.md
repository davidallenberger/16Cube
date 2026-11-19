# Desktop and Burning Man hardware

[Documentation index](../README.md) · [Build and upload](../DEVELOPMENT.md)

## Two independent choices

[`CubeCore.h`](../../render-board/src/CubeCore.h) selects wiring topology and
render canvas separately. At this checkpoint it defines `HARDWARE_DESKTOP`,
leaves `HARDWARE_BURNCUBE` commented, and leaves `TARGET_2026_CUBOID` commented.

| Choice | Desktop/current | Burning Man installation path |
| --- | --- | --- |
| Wiring | Eight-output desktop serpentine with unused spacer pixels | Four-output Dig-Octa mapping with safety outputs |
| Logical canvas | 16×16×16 = 4,096 voxels | `TARGET_2026_CUBOID`: 10×10×16 = 1,600 voxels |
| Render centers | 7.5, 7.5, 7.5 | 4.5, 4.5, 7.5 |
| Perimeter positions | 60 | 36 |
| LED color order | GRB | RGB |
| FastLED brightness | 32/255, approximately 12.5% | 64/255, approximately 25% |
| Typical ambient speed scaling | Base rates | Many deck intervals doubled / rates halved |

Select exactly one wiring topology. Enabling the BurnCube topology does not
automatically enable the cuboid canvas. The BurnCube mapping explicitly handles
X=0…9 and 16-high strands, so it must be paired with the matching canvas.
The code contains wider historical comments mentioning 8×8×16; the actual
`TARGET_2026_CUBOID` constants currently specify **10×10×16**.

Animation code uses `RNDR_X/Y/Z`, `RNDR_CX/CY/CZ`, and `RNDR_PERIMETER`.
`CUBE_SIZE` stays 16 and is also relevant to legacy/protocol assumptions. The
depth transport is always a 16³ occupancy bit grid; it is not resized by the
render-canvas switch. Do not globally replace every 16 with a render extent.

## Coordinates and physical mapping

Z is height; X and Y form the floor. Render coordinates are voxel centers,
0…extent−1. Clockwise/top-view descriptions in effects refer to logical axes;
a physical viewing orientation still depends on the built cube.

`setVoxel()` and `getVoxel()` in
[`CubeCore.cpp`](../../render-board/src/CubeCore.cpp) map coordinates directly
to physical CRGB output arrays. There is no obligatory separate 4,096-entry
logical framebuffer. Effects must use the voxel API so physical wiring details
stay out of their geometry. `getPerimeterCoords()` and `setVoxelByAxis()`
support face wrapping and axis-oriented drawing.

### Desktop

| Output | GPIO | Physical array length |
| --- | --- | --- |
| 0 | 23 | 544 |
| 1 | 19 | 544 |
| 2 | 22 | 544 |
| 3 | 21 | 544 |
| 4 | 4 | 544 |
| 5 | 5 | 544 |
| 6 | 2 | 550 |
| 7 | 18 | 547 |

These arrays total 4,361 positions for 4,096 visible voxels. The 265 remaining
positions reflect the actual wiring. Most strands have a stride of 17 for 16
visible LEDs; the final pair of outputs also accounts for three extra wiring
pixels per plane. The map incorporates serpentine reversals and these gaps.
They are not spare visible voxels available to animations.

`CUBINA_CHECK_SPACERS=1` enables diagnostic scanning of unmapped positions.
It reports scene context and findings; it does not repair animation writes.
The normal flag is 0. `tests/spacer_check_test.cpp` checks the real mapping
with stubs, including exactly these 265 positions.

### BurnCube / Dig-Octa

Each of four output arrays reserves 480 positions; the 10-wide canvas uses
2/3/2/3-plane groups (1,600 visible LEDs, 1,920 reserved array positions).
The X-to-output assignment is:

| X planes | Output | GPIO |
| --- | --- | --- |
| 0, 1, 2 | 3 | 1, with a commented alternative of 14 |
| 3, 4 | 2 | 0 |
| 5, 6 | 0 | 4 |
| 7, 8, 9 | 1 | 5 |

The mapping reverses vertical strand order and alternates drop direction by
local plane parity. Safety outputs are GPIO 2, 3, 12, 13. Read the full wiring
comments in `CubeCore.cpp` before commissioning. GPIO 1 is also the usual USB
serial TX pin; the source explicitly records this conflict/alternative.

BurnCube compilation deliberately requires explicit `UART_RX_PIN` and
`UART_TX_PIN` definitions in `CubeCore.h` or build flags; desktop defaults
must not silently collide with LED outputs. Confirm against the actual PCB.
A transmit pin of −1 is allowed for receive-only render wiring.

## Command-board connections

Current definitions are in
[`BoardConfig.h`](../../command-board/src/BoardConfig.h) and
[`SignalAcquisition.cpp`](../../command-board/src/SignalAcquisition.cpp).

| Function | Command GPIO / setting |
| --- | --- |
| UART2 TX / RX | 27 / 14 |
| Animation / Mic / ADC / Game buttons | 18 / 19 / 32 / 25 |
| Depth-labelled button | 33; currently playback Hold |
| INMP441 data | 21 |
| PCM1808 data | 5 |
| Shared I2S WS / BCK | 23 / 22 |
| ADC SCKI clock | GPIO 4 via LEDC at 8 MHz |
| Diagnostic LED strip | GPIO 12, 15 LEDs |

Desktop render UART RX/TX defaults are 27/14 on the **other board**. Wire
command TX27 → render RX27 and share ground; pin numbers repeat because
these are separate ESP32s. If using a return connection, render TX14 goes to
command RX14. UART2 runs 921,600 baud; USB diagnostics run 115,200.

The ADC uses standard I2S (`PCM1808_LEFT_JUSTIFIED=false`), 31,250 Hz frames,
2 MHz BCK, and the separate 8 MHz SCKI clock. “Bluetooth” in wire audio mode
means an external audio receiver feeding PCM1808, not Bluetooth audio decoded
by this firmware. Bluepad32 on the command board is for controllers.

## USB identity and hub sockets

Both CP2102 adapters report VID:PID `10C4:EA60` and may report the same serial
`0001`. macOS names such as `/dev/cu.usbserial-0001` and `…-3` can swap. They
are not reliable board roles.

The checked-in assignments are render `LOCATION=0-1.3`, command `0-1.4`.
[`tools/select_upload_port.py`](../../tools/select_upload_port.py) resolves
that location immediately when the uploader command expands, requires exactly
one matching CP2102, and fails rather than guessing. Monitor configuration uses
matching `hwgrep://` location expressions. These identify hub topology/socket,
not a unique ESP32. Moving a cable to another socket or changing upstream USB
topology can invalidate the assignment. Verify the role via its USB boot log.

## Brightness and commissioning boundary

An effect's RGB value, antialiasing coverage, and global FastLED brightness
are separate multipliers. The current desktop ceiling is 32, not 64. No
automatic power-budget calculation is established by these constants. The
firmware settings are not a certification of connector, wiring, or supply
capacity. Change brightness with the physical installation in mind and record
it as a hardware configuration change.
