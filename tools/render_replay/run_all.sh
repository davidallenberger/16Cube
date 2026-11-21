#!/usr/bin/env bash
# Regenerate the audio-routine behavior report end to end:
#   songs (WAV) -> command-board replay (NEW and OLD) -> every enabled audio
#   routine rendered with the real engines -> per-routine report.
#
#   tools/render_replay/run_all.sh WAV_DIR [OUT_DIR]
#
# WAV_DIR must contain levels.wav, sandstorm.wav and laylow.wav (31,250 Hz float stereo;
# see tools/audio_replay/README.md for afconvert). The hardware model matches
# the saved bt-pcm1808 profile at the Apple Music listening level; edit HW
# below when the profile or listening setup changes.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
wavs="$1"
out="${2:-$root/tools/render_replay/reports}"
HW=(--no-normalize --chain-db -34 --pcm-lowpass 203 --correct 203.2 --gate 0.000061)
work="$(mktemp -d "${TMPDIR:-/tmp}/render-replay.XXXXXX")"
trap 'rm -rf "$work"' EXIT
"$root/tools/audio_replay/build.sh" >/dev/null
"$root/tools/render_replay/build.sh" >/dev/null
for song in levels sandstorm laylow; do
    for path in new old; do
        "$root/tools/audio_replay/audio_replay" "$wavs/$song.wav" "${HW[@]}" --path "$path" \
            --csv "$work/${song}_$path.csv" >/dev/null
    done
done
mkdir -p "$work/frames" "$out"
"$root/tools/render_replay/render_replay" --list | grep -v disabled | while IFS= read -r routine; do
    slug=$(echo "$routine" | tr -c 'A-Za-z0-9\n' '_')
    for song in levels sandstorm laylow; do
        for path in new old; do
            perl -e 'alarm shift; exec @ARGV' 300 "$root/tools/render_replay/render_replay" \
                "$work/${song}_$path.csv" "$routine" "$work/frames/${slug}__${song}__$path.csv" >/dev/null
        done
    done
    echo "rendered: $routine"
done
python3 "$root/tools/render_replay/report.py" "$work/frames" "$out/audio_routines.md"
cp -R "$work/frames" "$out/frames"
