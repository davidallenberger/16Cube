#!/usr/bin/env python3
"""Measure the analog chain's frequency response with stepped test tones.

  python3 tools/audio_replay/tone_steps.py make tone_steps.wav
      Writes a 44.1 kHz stereo WAV: equal-level sine tones (-12 dBFS peak),
      3 s each, separated by 1 s of silence, low to high, twice.

  python3 tools/audio_replay/tone_steps.py read captures/NAME.log
      Finds the tones in a command-board capture (InRMS above the noise
      floor, separated by silences) and prints each tone's level relative to
      the 400 Hz tone. A flat chain reads about 0 dB everywhere.

Play the WAV at the usual listening volume through the normal chain while
capture_serial.py records. Pure standard library.
"""
import math
import re
import struct
import sys
import wave

TONES = [50, 100, 200, 400, 800, 1600, 3200, 6400, 10000, 13000]
RATE = 44100
TONE_S = 3.0
GAP_S = 1.0
LEAD_S = 2.0


def make(path):
    amp = 10 ** (-12 / 20) * 32767
    frames = bytearray()
    silence = struct.pack("<hh", 0, 0)
    frames += silence * int(LEAD_S * RATE)
    for _ in range(2):
        for f in TONES:
            n = int(TONE_S * RATE)
            fade = int(0.01 * RATE)
            for i in range(n):
                env = min(1.0, i / fade, (n - 1 - i) / fade)
                v = int(amp * env * math.sin(2 * math.pi * f * i / RATE))
                frames += struct.pack("<hh", v, v)
            frames += silence * int(GAP_S * RATE)
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(bytes(frames))
    total = LEAD_S + 2 * len(TONES) * (TONE_S + GAP_S)
    print(f"Wrote {path}: {len(TONES)} tones x2, {total:.0f} s")


def read(path):
    pat = re.compile(r"^\s*([\d.]+) ADC: InRMS=([\d.eE+-]+)")
    pts = [(float(m[1]), float(m[2])) for m in map(pat.match, open(path, errors="replace")) if m]
    floor = 2e-4
    # Group consecutive loud summaries into tones.
    groups, current = [], []
    for t, r in pts:
        if r > floor:
            current.append((t, r))
        elif current:
            groups.append(current)
            current = []
    if current:
        groups.append(current)
    # Drop the edges of each tone (fades and summary timing), keep the middle.
    levels = []
    for g in groups:
        mid = g[1:-1] if len(g) > 3 else g
        rms = sorted(r for _, r in mid)[len(mid) // 2]
        levels.append((g[0][0], 20 * math.log10(rms)))
    if len(levels) < len(TONES):
        sys.exit(f"Found {len(levels)} tones; expected {len(TONES)} (or {2 * len(TONES)}).")
    print(f"Found {len(levels)} tones.")
    for rep in range(len(levels) // len(TONES)):
        chunk = levels[rep * len(TONES):(rep + 1) * len(TONES)]
        ref = chunk[TONES.index(400)][1]
        print(f"Pass {rep + 1} (starts at {chunk[0][0]:.0f} s), level relative to 400 Hz:")
        for f, (_, db) in zip(TONES, chunk):
            print(f"  {f:6d} Hz  {db - ref:+6.1f} dB   ({db:6.1f} dBFS)")


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in ("make", "read"):
        sys.exit(__doc__)
    (make if sys.argv[1] == "make" else read)(sys.argv[2])
