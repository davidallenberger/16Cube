#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
CXX=${CXX:-g++}
# Apple ld and GNU ld use different dead-code elimination switches. The
# acquisition tests deliberately link only the production functions they use.
if [[ "$(uname -s)" == Darwin ]]; then
    discard_unused=(-Wl,-dead_strip)
else
    discard_unused=(-Wl,--gc-sections)
fi
if [[ -d command-board/src && -d render-board/src ]]; then
    command_src="$root/command-board/src"
    command_include="$root/command-board/include"
    render_src="$root/render-board/src"
    render_include="$root/render-board/include"
elif [[ -d command && -d render ]]; then
    # Also support running the tests in the original extracted migration archive.
    command_src="$root/command"
    command_include="$root/command"
    render_src="$root/render"
    render_include="$root/render"
else
    echo 'Place tests/ beside command-board/, render-board/, and shared/.' >&2
    exit 1
fi
[[ -f shared/SharedProtocol.h ]] || { echo 'Missing shared/SharedProtocol.h' >&2; exit 1; }
work="$(mktemp -d "${TMPDIR:-/tmp}/cubina-host-tests.XXXXXX")"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/command" "$work/render"
# Isolate the board modules from the full project's unrelated SDK headers.
# These are byte-for-byte source copies. Firmware builds remain separate.
stage() {
    local src="$1" inc="$2" dest="$3" file
    shift 3
    for file in "$@"; do
        if [[ -f "$src/$file" ]]; then
            cp "$src/$file" "$dest/$file"
        elif [[ -f "$inc/$file" ]]; then
            cp "$inc/$file" "$dest/$file"
        else
            echo "Missing migrated file: $src/$file (or $inc/$file)" >&2
            exit 1
        fi
    done
}
stage "$command_src" "$command_include" "$work/command" \
    main.cpp BoardConfig.h CommandState.cpp CommandState.h \
    SignalAcquisition.cpp SignalAcquisition.h AudioAnalysis.h TempoTracker.h AttackDetector.h UARTSender.cpp UARTSender.h SendTiming.h \
    BluetoothManager.cpp BluetoothManager.h SourceAdapters.cpp SourceAdapters.h ModeButton.h
stage "$render_src" "$render_include" "$work/render" \
    CubeCore.cpp CubeCore.h CubeSystem.cpp CubeSystem.h \
    TestDemo.h TestDemoAudio.cpp SpacerCheck.h SpacerCheck.cpp UARTReceiver.cpp UARTReceiver.h AudioFrame.h AudioFrameClock.h MidrangeBoost.h Audio.cpp Audio.h Depth.cpp Depth.h
# Compile the production BPM section co-resident in Streamers.h. Full
# Streamers effects require the ESP32/FastLED build, verified separately.
sed '/^#include "GeometryEngine.h"/,$d' "$render_src/Streamers.h" > "$work/render/Streamers.h"
common=(-DCUBINA_ENABLE_UART_RECEIVER=1 -std=c++11 -I "$root/tests/stubs" -I "$root/shared")
"$CXX" "${common[@]}" -Wall -Wextra -pedantic -fsanitize=address,undefined -g \
    tests/protocol_test.cpp -o "$work/protocol_test"
ASAN_OPTIONS=detect_leaks=0 "$work/protocol_test"
for bt in 0 1; do
    "$CXX" "${common[@]}" -fsyntax-only -DCOMMAND_ENABLE_GAMEPAD_BLUETOOTH=$bt \
        -I "$work/command" "$work/command/"*.cpp
done
"$CXX" "${common[@]}" -fsyntax-only -I "$work/render" "$work/render/"*.cpp
echo 'Host syntax checks passed (API stubs, not a firmware/toolchain build).'
"$CXX" "${common[@]}" -pthread -I "$work/command" tests/state_test.cpp -o "$work/state_test"
"$work/state_test"
"$CXX" "${common[@]}" -pthread \
    -I "$work/render" tests/receiver_test.cpp -o "$work/receiver_test"
"$work/receiver_test"

"$CXX" "${common[@]}" -I "$work/render" tests/receiver_startup_test.cpp -o "$work/receiver_startup_test"
"$work/receiver_startup_test"

# Exercise the real physical LED arrays/mapping, rather than setVoxel stubs.
# The enabled diagnostic test reads the real desktop arrays and proves that
# all 265 unmapped positions (and only those) are checked, without repairs or
# serial output inside a frame. Normal host tests leave the scanner disabled.
"$CXX" "${common[@]}" -UCUBINA_ENABLE_UART_RECEIVER -DCUBINA_ENABLE_UART_RECEIVER=0 \
    -DCUBINA_CHECK_SPACERS=1 -DFASTLED_ESP32_I2S_NUM_DMA_BUFFERS=2 \
    -I "$work/render" -fsanitize=address,undefined -g tests/spacer_check_test.cpp \
    -o "$work/spacer_check_test"
ASAN_OPTIONS=detect_leaks=0 "$work/spacer_check_test"

# Exercise the real physical LED arrays/mapping, rather than setVoxel stubs.
# Check both supported canvases using only a temporary header copy; the real
# project's hardware selection stays untouched.
sed '/^#define TARGET_2026_CUBOID/d' "$work/render/CubeCore.h" > "$work/render/CubeCore.full.h"
mv "$work/render/CubeCore.h" "$work/render/CubeCore.saved.h"
mv "$work/render/CubeCore.full.h" "$work/render/CubeCore.h"
for canvas in full cuboid; do
    canvas_flags=()
    if [[ "$canvas" == cuboid ]]; then
        canvas_flags=(-DTARGET_2026_CUBOID)
    fi
    # Bash 3.2 treats an empty array as unset under nounset. Expand only
    # when populated, preserving each flag as its own argument.
    "$CXX" "${common[@]}" ${canvas_flags[@]+"${canvas_flags[@]}"} -I "$work/render" \
        -fsanitize=address,undefined -g tests/bpm_output_test.cpp \
        -o "$work/bpm_output_$canvas"
    ASAN_OPTIONS=detect_leaks=0 "$work/bpm_output_$canvas"

    # Run the SAME production audio routines on both canvas dimensions.
    # Density should scale coverage while preserving bounds and event times.
    "$CXX" "${common[@]}" ${canvas_flags[@]+"${canvas_flags[@]}"} -I "$work/render" \
        -DAUDIO_SERIAL_DEBUG=0 -fsanitize=address,undefined -g \
        tests/audio_playback_test.cpp -o "$work/audio_playback_$canvas"
    ASAN_OPTIONS=detect_leaks=0 "$work/audio_playback_$canvas"
done
mv "$work/render/CubeCore.saved.h" "$work/render/CubeCore.h"

"$CXX" "${common[@]}" -I "$work/command" tests/mode_button_test.cpp -o "$work/mode_button_test"
"$work/mode_button_test"

"$CXX" "${common[@]}" -I "$work/command" tests/tempo_tracker_test.cpp -o "$work/tempo_tracker_test"
"$work/tempo_tracker_test"

"$CXX" "${common[@]}" -DCOMMAND_ENABLE_GAMEPAD_BLUETOOTH=1 \
    -I "$work/command" tests/gamepad_mapping_test.cpp -o "$work/gamepad_mapping_test"
"$work/gamepad_mapping_test"

# The numerical reference FFT is only for host verification, never firmware.
"$CXX" -I "$root/tests/audio_stubs" "${common[@]}" -I "$work/command" \
    -I "$root/tests/stubs" -ffunction-sections -fdata-sections "${discard_unused[@]}" \
    -DCOMMAND_ENABLE_GAMEPAD_BLUETOOTH=0 -fsanitize=address,undefined -g \
    tests/audio_analysis_test.cpp -o "$work/audio_analysis_test"
ASAN_OPTIONS=detect_leaks=0 "$work/audio_analysis_test"

# Test production packet construction and the scheduler independently of UART.
"$CXX" "${common[@]}" -I "$work/command" -ffunction-sections -fdata-sections \
    "${discard_unused[@]}" tests/sender_timing_test.cpp -o "$work/sender_timing_test"
"$work/sender_timing_test"

"$CXX" "${common[@]}" -I "$work/command" -fsanitize=address,undefined -g \
    tests/sender_rate_limit_test.cpp -o "$work/sender_rate_limit_test"
ASAN_OPTIONS=detect_leaks=0 "$work/sender_rate_limit_test"

"$CXX" "${common[@]}" -I "$work/command" tests/attack_detector_test.cpp -o "$work/attack_detector_test"
"$work/attack_detector_test"

# Exercise the actual private adapter/public frame contract independently of
# drawing: timing helpers, stale data, event groups, latches and sensitivity.
"$CXX" "${common[@]}" -I "$work/render" -fsanitize=address,undefined -g \
    tests/audio_frame_test.cpp -o "$work/audio_frame_test"
ASAN_OPTIONS=detect_leaks=0 "$work/audio_frame_test"

# Test the actual TestDemo audio player with deterministic drawing routines,
# independently of the physical cube or musical event detector.
"$CXX" "${common[@]}" -I "$work/render" -fsanitize=address,undefined -g \
    tests/audio_deck_test.cpp -o "$work/audio_deck_test"
ASAN_OPTIONS=detect_leaks=0 "$work/audio_deck_test"

# Verify the optional standalone receiver backend independently. Firmware
# retains the tested UART-enabled configuration.
# This separate OFF test defines no Serial/Serial2, millis or FreeRTOS symbols:
# an accidental dependency on those services makes the link fail.
"$CXX" "${common[@]}" -UCUBINA_ENABLE_UART_RECEIVER -DCUBINA_ENABLE_UART_RECEIVER=0 \
    -I "$work/render" -fsanitize=address,undefined -g \
    tests/receiver_disabled_test.cpp -o "$work/receiver_disabled_test"
ASAN_OPTIONS=detect_leaks=0 "$work/receiver_disabled_test"

# The production Julia body and ordered selector run on both canvas sizes.
python3 tests/julia_test.py
