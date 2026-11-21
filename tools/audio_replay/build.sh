#!/usr/bin/env bash
# Build the offline audio replay against the unmodified command-board sources
# and the real arduinoFFT library from the PlatformIO cache.
#
#   tools/audio_replay/build.sh            # builds tools/audio_replay/audio_replay
#
# Decode an MP3 to the format it reads (macOS built-in tool):
#   afconvert -f WAVE -d LEF32@31250 --src-complexity bats -r 127 song.mp3 song.wav
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
fft="$root/command-board/.pio/libdeps/esp32dev/arduinoFFT/src"
if [[ ! -f "$fft/arduinoFFT.cpp" ]]; then
    echo "Missing $fft. Build the command board once with PlatformIO to fetch arduinoFFT." >&2
    exit 1
fi
CXX=${CXX:-g++}
if [[ "$(uname -s)" == Darwin ]]; then strip_unused=(-Wl,-dead_strip); else strip_unused=(-Wl,--gc-sections); fi
# The real FFT header must be found before the test stub of the same name.
"$CXX" -std=c++11 -O2 -I "$fft" -I "$root/tests/stubs" -I "$root/shared" -I "$root/command-board/src" \
    -DCOMMAND_ENABLE_GAMEPAD_BLUETOOTH=0 -ffunction-sections -fdata-sections "${strip_unused[@]}" \
    "$root/tools/audio_replay/audio_replay.cpp" "$fft/arduinoFFT.cpp" \
    -o "$root/tools/audio_replay/audio_replay"
echo "Built tools/audio_replay/audio_replay"
