#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
CXX=${CXX:-g++}
if [[ -d command-board/src && -d main-render-board/src ]]; then
    command_src="$root/command-board/src"
    command_include="$root/command-board/include"
    render_src="$root/main-render-board/src"
    render_include="$root/main-render-board/include"
elif [[ -d command && -d render ]]; then
    # Also support running the tests in the original extracted migration archive.
    command_src="$root/command"
    command_include="$root/command"
    render_src="$root/render"
    render_include="$root/render"
else
    echo 'Place tests/ beside command-board/, main-render-board/, and shared/.' >&2
    exit 1
fi
[[ -f shared/SharedProtocol.h ]] || { echo 'Missing shared/SharedProtocol.h' >&2; exit 1; }
work="$(mktemp -d "${TMPDIR:-/tmp}/cubina-host-tests.XXXXXX")"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/command" "$work/render"
# Isolate the migration modules from the full project's unrelated SDK headers.
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
    SignalAcquisition.cpp SignalAcquisition.h AudioAnalysis.h TempoTracker.h UARTSender.cpp UARTSender.h SendTiming.h \
    BluetoothManager.cpp BluetoothManager.h SourceAdapters.cpp SourceAdapters.h ModeButton.h
stage "$render_src" "$render_include" "$work/render" \
    CubeCore.cpp CubeCore.h CubeSystem.cpp CubeSystem.h \
    UARTReceiver.cpp UARTReceiver.h Audio.cpp Audio.h BpmRenderer.h BpmDiagnostic.h Depth.cpp Depth.h
common=(-std=c++11 -I "$root/tests/stubs" -I "$root/shared")
"$CXX" "${common[@]}" -Wall -Wextra -pedantic -fsanitize=address,undefined -g \
    tests/protocol_test.cpp -o "$work/protocol_test"
ASAN_OPTIONS=detect_leaks=0 "$work/protocol_test"
for bt in 0 1; do
    "$CXX" "${common[@]}" -fsyntax-only -DCOMMAND_ENABLE_GAMEPAD_BLUETOOTH=$bt \
        -I "$work/command" "$work/command/"*.cpp
done
"$CXX" "${common[@]}" -fsyntax-only -I "$work/render" "$work/render/"*.cpp
for fixed in 0 1; do
    "$CXX" "${common[@]}" -fsyntax-only -I "$work/render" \
        -DCUBINA_BPM_DIAGNOSTIC=1 -DCUBINA_AUDIO_FIXED_BPM=$fixed \
        "$work/render/CubeSystem.cpp"
done
echo 'Host syntax checks passed (API stubs, not a firmware/toolchain build).'
"$CXX" "${common[@]}" -pthread -I "$work/command" tests/state_test.cpp -o "$work/state_test"
"$work/state_test"
"$CXX" "${common[@]}" -pthread -DCUBINA_BPM_DIAGNOSTIC=0 \
    -I "$work/render" tests/receiver_test.cpp -o "$work/receiver_test"
"$work/receiver_test"

"$CXX" "${common[@]}" -I "$work/render" -fsanitize=address,undefined -g \
    tests/bpm_diagnostic_test.cpp -o "$work/bpm_diagnostic_test"
ASAN_OPTIONS=detect_leaks=0 "$work/bpm_diagnostic_test"

"$CXX" "${common[@]}" -I "$work/render" tests/receiver_startup_test.cpp -o "$work/receiver_startup_test"
"$work/receiver_startup_test"

# Exercise the real physical LED arrays/mapping, rather than setVoxel stubs.
# Repeat with the full 16-cube canvas without changing a user's hardware file.
"$CXX" "${common[@]}" -I "$work/render" -fsanitize=address,undefined -g \
    tests/bpm_output_test.cpp -o "$work/bpm_output_test"
ASAN_OPTIONS=detect_leaks=0 "$work/bpm_output_test"
sed '/^#define TARGET_2026_CUBOID/d' "$work/render/CubeCore.h" > "$work/render/CubeCore.full.h"
mv "$work/render/CubeCore.h" "$work/render/CubeCore.saved.h"
mv "$work/render/CubeCore.full.h" "$work/render/CubeCore.h"
"$CXX" "${common[@]}" -I "$work/render" -fsanitize=address,undefined -g \
    tests/bpm_output_test.cpp -o "$work/bpm_output_full_test"
ASAN_OPTIONS=detect_leaks=0 "$work/bpm_output_full_test"
mv "$work/render/CubeCore.saved.h" "$work/render/CubeCore.h"

"$CXX" "${common[@]}" -I "$work/command" tests/mode_button_test.cpp -o "$work/mode_button_test"
"$work/mode_button_test"

"$CXX" "${common[@]}" -I "$work/render" -DAUDIO_SERIAL_DEBUG=0 -fsanitize=address,undefined -g \
    tests/audio_playback_test.cpp -o "$work/audio_playback_test"
ASAN_OPTIONS=detect_leaks=0 "$work/audio_playback_test"

"$CXX" "${common[@]}" -I "$work/command" tests/tempo_tracker_test.cpp -o "$work/tempo_tracker_test"
"$work/tempo_tracker_test"

"$CXX" "${common[@]}" -DCOMMAND_ENABLE_GAMEPAD_BLUETOOTH=1 \
    -I "$work/command" tests/gamepad_mapping_test.cpp -o "$work/gamepad_mapping_test"
"$work/gamepad_mapping_test"

# The numerical reference FFT is only for host verification, never firmware.
"$CXX" -I "$root/tests/audio_stubs" "${common[@]}" -I "$work/command" \
    -I "$root/tests/stubs" -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -DCOMMAND_ENABLE_GAMEPAD_BLUETOOTH=0 -fsanitize=address,undefined -g \
    tests/audio_analysis_test.cpp -o "$work/audio_analysis_test"
ASAN_OPTIONS=detect_leaks=0 "$work/audio_analysis_test"

# Test production packet construction and the scheduler independently of UART.
"$CXX" "${common[@]}" -I "$work/command" -ffunction-sections -fdata-sections \
    -Wl,--gc-sections tests/sender_timing_test.cpp -o "$work/sender_timing_test"
"$work/sender_timing_test"

"$CXX" "${common[@]}" -I "$work/command" -fsanitize=address,undefined -g \
    tests/sender_rate_limit_test.cpp -o "$work/sender_rate_limit_test"
ASAN_OPTIONS=detect_leaks=0 "$work/sender_rate_limit_test"
