#!/usr/bin/env python3
"""Summarize render_replay frame logs into a per-routine report.

    python3 tools/render_replay/report.py FRAMES_DIR OUT.md

FRAMES_DIR holds files named <routine>__<song>__<path>.csv written by
render_replay. For every routine the report compares songs and the NEW/OLD
processing paths on measurable behavior: how much of the cube it fills, how
bright and how busy it is, how it follows loudness, bass/mid/treble, hits and
the beat, and what it does in silence. Pure standard library.
"""
import csv
import glob
import math
import os
import re
import sys
from collections import defaultdict


def pct(values, q):
    if not values:
        return float("nan")
    v = sorted(values)
    return v[min(len(v) - 1, int(q * (len(v) - 1) + 0.5))]


def mean(values):
    return sum(values) / len(values) if values else float("nan")


def corr(a, b):
    n = len(a)
    if n < 3:
        return float("nan")
    ma, mb = mean(a), mean(b)
    sa = math.sqrt(sum((x - ma) ** 2 for x in a))
    sb = math.sqrt(sum((y - mb) ** 2 for y in b))
    if sa == 0 or sb == 0:
        return float("nan")
    return sum((x - ma) * (y - mb) for x, y in zip(a, b)) / (sa * sb)


def event_lift(rows, flag, window_s=0.15):
    """Mean frame-to-frame change just after an event's onset, relative to overall."""
    base = mean([r["change"] for r in rows])
    if not base:
        return float("nan"), 0
    after, onsets, last = [], 0, 0
    for i, r in enumerate(rows):
        if r[flag] and not last:
            onsets += 1
            t0 = r["t"]
            j = i
            while j < len(rows) and rows[j]["t"] - t0 <= window_s:
                after.append(rows[j]["change"])
                j += 1
        last = r[flag]
    return (mean(after) / base if after else float("nan")), onsets


def analyze(path):
    rows, details = [], []
    with open(path) as f:
        for r in csv.DictReader(f):
            details.append((float(r["t"]), r.get("detail", "")))
            rows.append({k: float(r[k]) for k in ("t", "lit", "light", "change", "height", "radius",
                                                  "volumeSm", "volumeRaw", "low", "mid", "high",
                                                  "bassKick", "midAttack", "highAttack", "bpm", "beatPulse")})
    music = [r for r in rows if r["volumeRaw"] > 0 or r["volumeSm"] > 2]
    quiet = [r for r in rows if r["volumeRaw"] == 0 and r["volumeSm"] == 0]
    minutes = (rows[-1]["t"] - rows[0]["t"]) / 60 if rows else 1
    vols = sorted(r["volumeSm"] for r in music)
    lo_cut, hi_cut = pct(vols, 0.25), pct(vols, 0.75)
    loud = [r["light"] for r in music if r["volumeSm"] >= hi_cut]
    soft = [r["light"] for r in music if r["volumeSm"] <= lo_cut]
    m = {
        "fps": len(rows) / max(1e-9, rows[-1]["t"] - rows[0]["t"]) if rows else 0,
        "lit_p10": pct([r["lit"] for r in music], .1), "lit_p50": pct([r["lit"] for r in music], .5),
        "lit_p90": pct([r["lit"] for r in music], .9),
        "full_pct": 100 * mean([r["lit"] > 0.85 for r in music]) if music else float("nan"),
        "dark_pct": 100 * mean([r["lit"] < 0.005 for r in music]) if music else float("nan"),
        "light_p50": pct([r["light"] for r in music], .5), "light_p90": pct([r["light"] for r in music], .9),
        "change_p50": pct([r["change"] for r in music], .5), "change_p90": pct([r["change"] for r in music], .9),
        "c_vol_lit": corr([r["volumeSm"] for r in music], [r["lit"] for r in music]),
        "c_vol_light": corr([r["volumeSm"] for r in music], [r["light"] for r in music]),
        "c_vol_height": corr([r["volumeSm"] for r in music], [r["height"] for r in music]),
        "c_vol_radius": corr([r["volumeSm"] for r in music], [r["radius"] for r in music]),
        "c_low": corr([r["low"] for r in music], [r["light"] for r in music]),
        "c_mid": corr([r["mid"] for r in music], [r["light"] for r in music]),
        "c_high": corr([r["high"] for r in music], [r["light"] for r in music]),
        # beatPulse is 0 on the beat and 1 between beats; flip it so a positive
        # number means "brightest on the beat".
        "c_beat": corr([1.0 - r["beatPulse"] for r in music], [r["light"] for r in music]),
        "contrast": (mean(loud) / mean(soft)) if soft and mean(soft) > 0 else float("nan"),
        "silence_lit": mean([r["lit"] for r in quiet]) if quiet else float("nan"),
        "silence_change": mean([r["change"] for r in quiet]) if quiet else float("nan"),
    }
    for name, flag in (("bass", "bassKick"), ("mid", "midAttack"), ("high", "highAttack")):
        lift, n = event_lift(rows, flag)
        m[f"lift_{name}"] = lift
        m[f"rate_{name}"] = n / minutes
    m.update(element_stats(details, minutes))
    return m


def _nums(text, key):
    match = re.search(key + r"=([-0-9.]+(?:/[-0-9.]+)*)", text)
    return [float(v) for v in match.group(1).split("/")] if match else None


def element_stats(details, minutes):
    """Per-element pegging from each routine's detail line (sampled ~2 Hz).

    peg = share of logged samples where a size/height/color element sits at its
    maximum (for multi-element routines: the average element and the worst one).
    """
    samples = [d for t, d in details if "=" in d]
    out = {"peg": float("nan"), "peg_worst": float("nan"), "peg_what": "", "changes_pm": float("nan")}
    if not samples:
        return out
    text = samples[0]
    per_element = None
    flags = None
    if "Octahedron" in text or "Cylinder" in text:
        cap = 4.0 if "Octahedron" in text else 3.0
        flags = [(_nums(d, "cubeEdge") or [0])[0] >= cap - 0.01 for d in samples]
        out["peg_what"] = "corner cubes at max size"
        counts = [(_nums(d, "changes") or [None])[0] for d in samples]
        counts = [c for c in counts if c is not None]
        if len(counts) > 1:
            out["changes_pm"] = (counts[-1] - counts[0]) / minutes
    elif "Bouncing Balls" in text:
        hops = [(_nums(d, "hopHeight") or [0])[0] for d in samples]
        flags = [h >= 14.9 for h in hops if h > 0]
        out["peg_what"] = "hops reaching the ceiling"
    elif "Burst phase" in text:
        flags = [(_nums(d, "fill") or [0])[0] >= 21.9 for d in samples]
        out["peg_what"] = "fill at its cap"
    elif "Drift Rose" in text:
        flags = ["reach=100%" in d for d in samples]
        out["peg_what"] = "petals at full reach"
    elif "Frequency Shapes" in text or "Frequency Planes" in text:
        per_element = []
        for d in samples:
            levels, full = _nums(d, "levels"), _nums(d, "full")
            if levels and full:
                per_element.append([v >= full[0] for v in levels])
        out["peg_what"] = "bands at full size (avg / worst band)"
    elif "GEQ" in text and "bars" in text:
        per_element = [[v >= 14.95 for v in (_nums(d, "bars") or [])] for d in samples]
        per_element = [e for e in per_element if e]
        out["peg_what"] = "columns at full height (avg / worst column)"
    elif "Funky Plank" in text:
        codes = []
        for d in samples:
            match = re.search(r"colors=(\S+)", d)
            if match:
                codes.append(match.group(1).strip('"'))
        # Current palette "-IBVROAW" tops out at W; the original "-ROYGBIV" (Y, G) at V.
        legacy = any(c in "YG" for code in codes for c in code)
        top = "V" if legacy else "W"
        flags = [c == top for code in codes for c in code]
        out["peg_what"] = "blocks in the top color"
    elif "Waverly" in text:
        flags = [(_nums(d, "height") or [0])[0] >= 8.0 for d in samples]
        out["peg_what"] = "cavern touching the middle (closing)"
    if flags:
        out["peg"] = 100.0 * mean(flags)
    if per_element:
        columns = list(zip(*per_element))
        shares = [100.0 * mean(c) for c in columns]
        out["peg"] = mean(shares)
        out["peg_worst"] = max(shares)
    return out


ROWS = [
    ("Frame rate", "fps", "{:.0f}"),
    ("Cube filled (typical / range)", None, None),
    ("Time nearly full (>85%)", "full_pct", "{:.0f}%"),
    ("Time dark during music", "dark_pct", "{:.0f}%"),
    ("Brightness typical / peak", None, None),
    ("Motion typical / peak", None, None),
    ("Loud vs soft sections (light ratio)", "contrast", "{:.1f}x"),
    ("Follows volume: fill / light", None, None),
    ("Follows volume: height / spread", None, None),
    ("Follows bass / mid / treble (light)", None, None),
    ("Brightest on the beat (light)", "c_beat", "{:+.2f}"),
    ("Reacts to bass hits (x motion, per min)", None, None),
    ("Reacts to mid hits", None, None),
    ("Reacts to treble hits", None, None),
    ("In silence: fill / motion", None, None),
    ("Pegged elements", None, None),
    ("Color changes per minute", "changes_pm", "{:.0f}"),
]


def cell(m, label):
    f = lambda v, s="{:.2f}": "–" if v != v else s.format(v)
    if label == "Cube filled (typical / range)":
        return f"{f(m['lit_p50'], '{:.0%}')} ({f(m['lit_p10'], '{:.0%}')}–{f(m['lit_p90'], '{:.0%}')})"
    if label == "Brightness typical / peak":
        return f"{f(m['light_p50'])} / {f(m['light_p90'])}"
    if label == "Motion typical / peak":
        return f"{f(m['change_p50'], '{:.3f}')} / {f(m['change_p90'], '{:.3f}')}"
    if label == "Follows volume: fill / light":
        return f"{f(m['c_vol_lit'], '{:+.2f}')} / {f(m['c_vol_light'], '{:+.2f}')}"
    if label == "Follows volume: height / spread":
        return f"{f(m['c_vol_height'], '{:+.2f}')} / {f(m['c_vol_radius'], '{:+.2f}')}"
    if label == "Follows bass / mid / treble (light)":
        return f"{f(m['c_low'], '{:+.2f}')} / {f(m['c_mid'], '{:+.2f}')} / {f(m['c_high'], '{:+.2f}')}"
    for name in ("bass", "mid", "high"):
        title = {"bass": "Reacts to bass hits (x motion, per min)", "mid": "Reacts to mid hits",
                 "high": "Reacts to treble hits"}[name]
        if label == title:
            return f"{f(m['lift_' + name], '{:.1f}x')} ({m['rate_' + name]:.0f}/min)"
    if label == "In silence: fill / motion":
        return f"{f(m['silence_lit'], '{:.0%}')} / {f(m['silence_change'], '{:.3f}')}"
    if label == "Pegged elements":
        if m["peg"] != m["peg"]:
            return "–"
        worst = "" if m["peg_worst"] != m["peg_worst"] else f" / {m['peg_worst']:.0f}%"
        return f"{m['peg']:.0f}%{worst} ({m['peg_what']})"
    key, fmt = next((k, s) for t, k, s in ROWS if t == label)
    return f(m[key], fmt)


def main():
    frames, out_path = sys.argv[1], sys.argv[2]
    runs = defaultdict(dict)
    for path in sorted(glob.glob(os.path.join(frames, "*__*__*.csv"))):
        routine, song, proc = os.path.basename(path)[:-4].rsplit("__", 2)
        runs[" ".join(routine.replace("_", " ").split())][(song, proc)] = analyze(path)
    columns = [("levels", "new"), ("levels", "old"), ("sandstorm", "new"), ("sandstorm", "old"),
               ("laylow", "new"), ("laylow", "old")]
    titles = {"levels": "Levels", "sandstorm": "Sandstorm", "laylow": "Lay Low"}
    lines = ["# Audio routine behavior report", "",
             "Generated by `tools/render_replay/report.py` from replays of Levels, Sandstorm and Lay Low through the",
             "calibrated `bt-pcm1808` hardware model (Apple Music level), NEW (AGC + treble correction) vs OLD.",
             "Fill = share of voxels lit; light = average voxel brightness (0-1); motion = average frame-to-frame",
             "change (0-1). Correlations run from -1 to +1 (0 = unrelated). Hit reaction = motion in the 150 ms",
             "after a hit relative to the routine's average motion (1.0x = no visible reaction).", ""]
    for routine in sorted(runs):
        present = [c for c in columns if c in runs[routine]] or columns[:4]
        lines += [f"## {routine}", "",
                  "| | " + " | ".join(f"{titles[s]} {p.upper()}" for s, p in present) + " |",
                  "|---" * (len(present) + 1) + "|"]
        for label, _, _ in ROWS:
            lines.append(f"| {label} | " + " | ".join(
                cell(runs[routine][c], label) for c in present) + " |")
        lines.append("")
    with open(out_path, "w") as f:
        f.write("\n".join(lines))
    print(f"Wrote {out_path} ({len(runs)} routines)")


if __name__ == "__main__":
    main()
