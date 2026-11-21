#!/usr/bin/env python3
"""Find where a song sits in a command-board capture and solve the chain gain.

    python3 tools/audio_replay/calibrate_chain.py captures/x.log song.wav [song2.wav ...]
        [--from SECONDS] [--to SECONDS]

The board's "ADC: InRMS=" line reports the raw RMS of the latest 512-sample
window (before any visual gain). This script computes the same measurement
for every 256-sample hop of each WAV (DC removed per channel, mean of the two
channel powers), slides it along the capture in log-RMS, and reports the best
alignment. At that alignment, median(logged RMS / WAV RMS) in dB is the TOTAL
digital-to-ADC gain. Subtract the replay's normalization to get --chain-db.
Pure standard library: no numpy required.
"""
import array
import math
import re
import struct
import sys

RATE = 31250
WINDOW = 512
HOP = 256


def read_wav(path):
    with open(path, "rb") as f:
        data = f.read()
    pos = 12
    while pos < len(data):
        cid, size = data[pos:pos + 4], struct.unpack("<I", data[pos + 4:pos + 8])[0]
        if cid == b"data":
            samples = array.array("f")
            samples.frombytes(data[pos + 8:pos + 8 + size - size % 8])
            return samples
        pos += 8 + size + (size & 1)
    raise SystemExit(f"no data chunk in {path}")


def hop_rms(samples):
    """RMS per hop, matching AudioAnalysis::OverlapWindow::copyForFFT."""
    left = samples[0::2]
    right = samples[1::2]
    n = len(left)
    def prefix(values, square):
        out = array.array("d", [0.0])
        total = 0.0
        for v in values:
            total += v * v if square else v
            out.append(total)
        return out
    sl, ql, sr, qr = prefix(left, False), prefix(left, True), prefix(right, False), prefix(right, True)
    times, rms = [], []
    for end in range(WINDOW, n + 1, HOP):
        start = end - WINDOW
        def power(s, q):
            mean = (s[end] - s[start]) / WINDOW
            return max(0.0, (q[end] - q[start]) / WINDOW - mean * mean)
        rms.append(math.sqrt((power(sl, ql) + power(sr, qr)) / 2))
        times.append(end / RATE)
    return times, rms


def main():
    args = sys.argv[1:]
    lo, hi = 0.0, 1e9
    if "--from" in args:
        i = args.index("--from"); lo = float(args[i + 1]); del args[i:i + 2]
    if "--to" in args:
        i = args.index("--to"); hi = float(args[i + 1]); del args[i:i + 2]
    log_path, wavs = args[0], args[1:]
    pattern = re.compile(r"^\s*([\d.]+) ADC: InRMS=([\d.eE+-]+)")
    points = []
    for line in open(log_path, errors="replace"):
        m = pattern.match(line)
        if m and lo <= float(m.group(1)) <= hi:
            points.append((float(m.group(1)), float(m.group(2))))
    floor = 2e-4  # Ignore silence/noise-floor samples when correlating.
    loud = [(t, math.log10(r)) for t, r in points if r > floor]
    print(f"{len(points)} summaries in [{lo:g}, {min(hi, points[-1][0] if points else 0):g}] s, {len(loud)} above the noise floor")
    for wav in wavs:
        times, rms = hop_rms(read_wav(wav))
        logs = [math.log10(max(r, 1e-9)) for r in rms]
        duration = times[-1]
        best = (-2.0, 0.0)
        # Song start time within the capture, coarse then fine.
        span = [loud[0][0] - duration, loud[-1][0]]
        for step, around in ((0.25, None), (0.01, 1)):
            if around:
                span = [best[1] - 1.0, best[1] + 1.0]
            start = span[0]
            while start <= span[1]:
                xs, ys = [], []
                for t, y in loud:
                    k = int(round((t - start) * RATE / HOP)) - 2
                    if 0 <= k < len(logs) and rms[k] > 1e-5:
                        xs.append(logs[k]); ys.append(y)
                if len(xs) > 0.6 * len(loud) and len(xs) > 30:
                    mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
                    sxy = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
                    sxx = sum((x - mx) ** 2 for x in xs); syy = sum((y - my) ** 2 for y in ys)
                    r = sxy / math.sqrt(sxx * syy) if sxx and syy else 0
                    if r > best[0]:
                        best = (r, start)
                start += step
        r, start = best
        ratios = []
        for t, y in loud:
            k = int(round((t - start) * RATE / HOP)) - 2
            if 0 <= k < len(logs) and rms[k] > 1e-4:
                ratios.append(20 * (y - logs[k]))
        ratios.sort()
        med = ratios[len(ratios) // 2] if ratios else float("nan")
        q1 = ratios[len(ratios) // 4] if ratios else float("nan")
        q3 = ratios[3 * len(ratios) // 4] if ratios else float("nan")
        print(f"{wav.split('/')[-1]}: best match r={r:.3f}, song starts at {start:.2f} s in the capture; "
              f"total gain median {med:+.1f} dB (IQR {q1:+.1f} .. {q3:+.1f}, n={len(ratios)})")


if __name__ == "__main__":
    main()
