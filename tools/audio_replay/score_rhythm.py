#!/usr/bin/env python3
"""Score the command board's tempo, beat phase, kicks and attacks against a
reference (reference.py output). Pure standard library.

    python3 score_rhythm.py replay.csv reference.json [--tolerance 0.07]

replay.csv comes from audio_replay --csv. Times are matched within the
tolerance (70 ms, the usual beat-tracking window). The median signed offset
is reported so a constant pipeline latency is visible rather than hidden.
"""
import bisect
import csv
import json
import statistics
import sys


def match(events, reference, tolerance):
    """Greedy one-to-one matching; returns (hits, offsets)."""
    used = set()
    offsets = []
    for t in events:
        i = bisect.bisect_left(reference, t - tolerance)
        best = None
        while i < len(reference) and reference[i] <= t + tolerance:
            if i not in used and (best is None or abs(reference[i] - t) < abs(reference[best] - t)):
                best = i
            i += 1
        if best is not None:
            used.add(best)
            offsets.append(t - reference[best])
    return len(offsets), offsets


def prf(events, reference, tolerance):
    hits, offsets = match(events, reference, tolerance)
    p = hits / len(events) if events else 0.0
    r = hits / len(reference) if reference else 0.0
    f = 2 * p * r / (p + r) if p + r else 0.0
    off = statistics.median(offsets) * 1000 if offsets else float("nan")
    return p, r, f, off


def main():
    replay, ref_path = sys.argv[1], sys.argv[2]
    tolerance = float(sys.argv[sys.argv.index("--tolerance") + 1]) if "--tolerance" in sys.argv else 0.07
    ref = json.load(open(ref_path))
    rows = list(csv.DictReader(open(replay)))
    duration = float(rows[-1]["t"])
    ref_beats = [t for t in ref["beats"] if t <= duration]
    ref_onsets = [t for t in ref["onsets"] if t <= duration]
    strong = [t for t, s in zip(ref["onsets"], ref["onset_strength"]) if s >= 0.5 and t <= duration]
    music = [t for t in ref_beats]
    start, end = (music[0], music[-1]) if music else (0.0, duration)

    kicks, attacks, predicted, predicted_active = [], [], [], []
    bpm_active = bpm_active_good = 0
    locked_rows = active_rows = music_rows = 0
    bpm_good = bpm_octave = bpm_locked = 0
    prev = None
    for row in rows:
        t = float(row["t"])
        flags = int(row["flags"])
        phase = int(row["phase"])
        bpm = float(row["bpm"])
        in_music = start <= t <= end
        music_rows += in_music
        active_rows += in_music and bool(flags & 1)
        locked = bool(flags & 2)
        if in_music and flags & 1 and bpm > 0:
            bpm_active += 1
            bpm_active_good += abs(bpm / ref["tempo"] - 1) < 0.04
        if in_music and locked:
            locked_rows += 1
            if bpm > 0:
                bpm_locked += 1
                ratio = bpm / ref["tempo"]
                if abs(ratio - 1) < 0.04:
                    bpm_good += 1
                elif min(abs(ratio - 2), abs(ratio - 0.5)) < 0.08:
                    bpm_octave += 1
        if prev is not None:
            if int(row["beatCount"]) > int(prev["beatCount"]):
                kicks.append(t)
            if int(row["attackCount"]) > int(prev["attackCount"]):
                attacks.append(t)
            # A predicted beat is where the phase wraps (65535 -> 0).
            wrapped = phase < int(prev["phase"]) - 32768
            if locked and wrapped:
                predicted.append(t)
            if flags & 1 and bpm > 0 and wrapped and in_music:
                predicted_active.append(t)
        prev = row

    minutes = (end - start) / 60 or 1
    print(f"reference: {ref['tempo']} BPM, {len(ref_beats)} beats, {len(ref_onsets)} onsets ({len(strong)} strong)")
    print(f"tempo: active {active_rows / music_rows:.0%}, locked {locked_rows / music_rows:.0%}; "
          f"while locked BPM right {bpm_good / max(1, bpm_locked):.0%}, half/double {bpm_octave / max(1, bpm_locked):.0%}")
    print(f"while active: BPM right {bpm_active_good / max(1, bpm_active):.0%}")
    p, r, f, off = prf(predicted_active, ref_beats, tolerance)
    print(f"predicted beats (all active): F {f:.2f} (precision {p:.0%}, recall {r:.0%}), median offset {off:+.0f} ms")
    p, r, f, off = prf(predicted, ref_beats, tolerance)
    print(f"predicted beats (locked only): F {f:.2f} (precision {p:.0%}, recall {r:.0%}), median offset {off:+.0f} ms")
    p, r, f, off = prf(kicks, ref_beats, tolerance)
    print(f"kicks vs beats: {len(kicks) / minutes:.0f}/min, F {f:.2f} (precision {p:.0%}, recall {r:.0%}), offset {off:+.0f} ms")
    p, r, f, off = prf(attacks, ref_onsets, tolerance)
    print(f"attacks vs onsets: {len(attacks) / minutes:.0f}/min, F {f:.2f} (precision {p:.0%}, recall {r:.0%}), offset {off:+.0f} ms")
    p, r, f, off = prf(attacks, strong, tolerance)
    print(f"attacks vs strong onsets: precision {p:.0%}, recall {r:.0%}")
    dense = [t for t in ref.get("onsets_dense", []) if t <= duration]
    if dense:
        p, r, f, off = prf(attacks, dense, tolerance)
        print(f"attacks vs any onset (dense, {len(dense) / minutes:.0f}/min): precision {p:.0%}")
    p, r, f, off = prf(attacks, ref_beats, tolerance)
    print(f"attacks on a beat: {p:.0%}")
    # Per group (bits match the renderer: low 0x000F, mid 0x07F0, high 0xF800).
    names = ("low", "mid", "high")
    masks = (0x000F, 0x07F0, 0xF800)
    per = {n: [] for n in names}
    prev = None
    for row in rows:
        if prev is not None and int(row["attackCount"]) > int(prev["attackCount"]):
            for n, m in zip(names, masks):
                if int(row["attackBands"]) & m:
                    per[n].append(float(row["t"]))
        prev = row
    parts = []
    for n in names:
        p, r, f, off = prf(per[n], ref_onsets, tolerance)
        parts.append(f"{n} {len(per[n]) / minutes:.0f}/min ({p:.0%} on onsets)")
    print("attack groups: " + ", ".join(parts))
    # Rate by 15 s section: hits should follow the music (busier drops).
    sections = {}
    for t in attacks:
        sections[int(t // 15)] = sections.get(int(t // 15), 0) + 1
    print("attacks per 15 s: " + " ".join(str(sections.get(k, 0)) for k in range(int(duration // 15) + 1)))


if __name__ == "__main__":
    main()
