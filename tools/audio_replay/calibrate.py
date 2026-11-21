#!/usr/bin/env python3
"""Calibrate the command board's audio input in one run, no button presses.

    ~/.platformio/penv/bin/python tools/audio_replay/calibrate.py            # measure + verify, no save
    ~/.platformio/penv/bin/python tools/audio_replay/calibrate.py --save     # also store on the board (NVS)

What it does, through the NORMAL listening chain (the Mac's default output,
e.g. the Bluetooth receiver, at whatever volume is set now):
  1. Opens the command board's USB serial (this reboots it), pauses its debug
     summary, selects ADC.
  2. Silence: measures the real noise floor with nothing playing.
  3. Tone steps on the OLD (uncorrected) path: measures the input's frequency
     response and fits the ADC module's first-order low-pass corner.
  4. Sets the correction corner, a noise gate 6 dB above the loudest silence
     window, and a kick floor above the bass noise.
  5. Verifies on the NEW path: tone steps should read flat, silence dark.
  6. With --save, stores the values on the board; otherwise they last until
     reboot. Writes a JSON report to tools/audio_replay/captures/.

Uses macOS afplay for playback. Needs pyserial (PlatformIO's Python has it).
Once a hardware setup is final, copy the saved values into the firmware.
"""
import argparse
import datetime
import json
import math
import os
import re
import struct
import subprocess
import sys
import tempfile
import time
import wave

import serial
import serial.tools.list_ports

RATE_OUT = 44100            # Playback file rate (the ADC runs at 31,250 Hz).
ADC_RATE = 31250
SLOT_HZ = ADC_RATE / 512    # One FFT slot on the board.
BAND_EDGES = [1, 2, 3, 4, 5, 7, 9, 12, 16, 22, 30, 42, 58, 82, 116, 164, 256]
# Tones exactly on FFT slots, roughly octave spaced, up to ~12.8 kHz.
TONE_SLOTS = [1, 2, 4, 7, 13, 26, 52, 105, 164, 210]
REFERENCE_SLOT = 2          # Response is reported relative to ~122 Hz.
TONE_SECONDS = 3.5
LEAD_SECONDS = 1.3          # Bluetooth/afplay start-up latency allowance (varies).
MEASURE_SECONDS = 1.4
HERE = os.path.dirname(os.path.abspath(__file__))


# ---------------------------------------------------------------- audio files
def write_wav(path, samples):
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE_OUT)
        w.writeframes(b"".join(struct.pack("<hh", s, s) for s in samples))


def make_tone(path, hz, dbfs=-12.0, seconds=TONE_SECONDS):
    amp = 10 ** (dbfs / 20) * 32767
    n = int(seconds * RATE_OUT)
    fade = int(0.02 * RATE_OUT)
    write_wav(path, [int(amp * min(1.0, i / fade, (n - 1 - i) / fade) *
                         math.sin(2 * math.pi * hz * i / RATE_OUT)) for i in range(n)])


# ---------------------------------------------------------------- board link
class Board:
    def __init__(self, device):
        self.port = serial.Serial()
        self.port.port = device
        self.port.baudrate = 115200
        self.port.timeout = 0.1
        self.port.dtr = False
        self.port.rts = False
        self.port.open()
        self.buffer = b""
        self.log = []

    def lines(self, seconds):
        end = time.time() + seconds
        while time.time() < end:
            self.buffer += self.port.read(4096)
            while b"\n" in self.buffer:
                raw, self.buffer = self.buffer.split(b"\n", 1)
                text = raw.decode("utf-8", "replace").strip()
                self.log.append(text)
                yield text

    def wait_for(self, pattern, seconds):
        for line in self.lines(seconds):
            if re.search(pattern, line):
                return line
        return None

    def command(self, text, expect=r"^CAL ", seconds=3.0):
        line = None
        for attempt in range(2):  # One resend: a reply can occasionally be missed.
            self.port.write((text + "\n").encode())
            line = self.wait_for(expect, seconds)
            if line is not None:
                break
        if line is None:
            raise SystemExit(f"No reply to '{text}'. Last lines:\n  " + "\n  ".join(self.log[-8:]))
        if line.startswith("CAL ERR"):
            raise SystemExit(f"Board refused '{text}': {line}")
        return line

    def measure(self, seconds):
        line = None
        for attempt in range(2):  # One retry: a result line can occasionally be lost.
            self.command(f"measure {seconds:.2f}", r"^CAL (OK measure|ERR)")
            line = self.wait_for(r"^CAL MEASURE", seconds + 5)
            if line is not None:
                break
        if line is None:
            raise SystemExit("Measurement did not complete. Last lines:\n  " + "\n  ".join(self.log[-8:]))
        m = re.search(r"path=(\w+) hops=(\d+) rms=([-\d.]+) rmsmax=([-\d.]+) gate=([-\d.]+) "
                      r"gatemax=([-\d.]+) vraw=(\d+) bands=([-\d.,]+) bins=([\d,]+)", line)
        return {"path": m[1], "hops": int(m[2]), "rms": float(m[3]), "rmsmax": float(m[4]),
                "gate": float(m[5]), "gatemax": float(m[6]), "vraw": int(m[7]),
                "bands": [float(x) for x in m[8].split(",")],
                "bins": [int(x) for x in m[9].split(",")]}


def find_command_board():
    ports = [p for p in serial.tools.list_ports.comports() if (p.vid, p.pid) == (0x10C4, 0xEA60)]
    for p in ports:
        if (p.location or "").endswith("0-1.4"):
            return p.device
    if len(ports) == 1:
        return ports[0].device
    raise SystemExit("Command board not found on hub location 0-1.4")


def default_output():
    try:
        text = subprocess.run(["system_profiler", "SPAudioDataType"], capture_output=True,
                              text=True, timeout=20).stdout
    except Exception:
        return "unknown"
    name = "unknown"
    for line in text.splitlines():
        if re.match(r"^\s{8}\S.*:$", line):
            current = line.strip()[:-1]
        if "Default Output Device: Yes" in line:
            name = current
    return name


def output_volume():
    try:
        return subprocess.run(["osascript", "-e", "output volume of (get volume settings)"],
                              capture_output=True, text=True, timeout=5).stdout.strip()
    except Exception:
        return "unknown"


# ---------------------------------------------------------------- analysis
def band_of(slot):
    for b in range(16):
        if BAND_EDGES[b] <= slot < BAND_EDGES[b + 1]:
            return b
    return 15


def tone_level_db(measurement, slot):
    """Tone power summed over the bands its Hamming main lobe touches."""
    bands = sorted({band_of(s) for s in range(max(1, slot - 2), min(255, slot + 2) + 1)})
    power = sum(10 ** (measurement["bands"][b] / 10) for b in bands)
    return 10 * math.log10(max(power, 1e-20))


def fit_lowpass(freqs, levels_db):
    """Least squares fit of G - 10*log10(1 + (f/fc)^2); returns (fc, rms error)."""
    best = (None, 1e9)
    fc = 20.0
    while fc < 40000:
        model = [-10 * math.log10(1 + (f / fc) ** 2) for f in freqs]
        g = sum(l - m for l, m in zip(levels_db, model)) / len(freqs)
        err = math.sqrt(sum((l - m - g) ** 2 for l, m in zip(levels_db, model)) / len(freqs))
        if err < best[1]:
            best = (fc, err)
        fc *= 1.01
    return best


def play_and_measure(board, wav):
    player = subprocess.Popen(["afplay", wav])
    time.sleep(LEAD_SECONDS)
    result = board.measure(MEASURE_SECONDS)
    player.wait()
    return result


def tone_sweep(board, files, label, silence, expected=None):
    """Measure each tone; retry when playback was not sounding in the window.

    A tone counts as present when the input (full band, or above 150 Hz for
    the module-attenuated high tones) is 15 dB above silence and, on a
    verification sweep, within 3 dB of the first sweep (the correction never
    changes the uncorrected input RMS).
    """
    results = []
    for index, (slot, wav) in enumerate(files):
        for attempt in range(3):
            m = play_and_measure(board, wav)
            present = (m["rms"] > silence["rms"] + 15 or m["gate"] > silence["gate"] + 15) and (
                expected is None or abs(m["rms"] - expected[index]) <= 3.0)
            # A tone that started or stopped inside the window reads below its
            # loudest window; a steady tone reads within ~1 dB of it.
            steady = m["gatemax"] - m["gate"] < 1.0  # Above 150 Hz: free of the rumble bursts.
            present = present and steady
            if present:
                break
            print(f"  {label} {slot * SLOT_HZ:7.0f} Hz  not heard in the window (input {m['rms']:.1f} dBFS), retrying")
        else:
            raise SystemExit(f"Tone {slot * SLOT_HZ:.0f} Hz never reached the board: check playback/output device")
        results.append((slot, m))
        print(f"  {label} {slot * SLOT_HZ:7.0f} Hz  input {m['rms']:6.1f} dBFS  tone band {tone_level_db(m, slot):6.1f} dB")
    return results


# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--save", action="store_true", help="store the result on the board (NVS)")
    ap.add_argument("--profile", default="bt-pcm1808",
                    help="hardware configuration name: source-digitizer, e.g. bt-pcm1808, dj-pcm1808 "
                         "(a-z, 0-9, '-', at most 11 characters)")
    ap.add_argument("--silence", type=float, default=6.0, help="seconds of silence to measure")
    args = ap.parse_args()

    out_name = default_output()
    volume = output_volume()
    print(f"Playback: default output '{out_name}', Mac volume {volume}/100. "
          "These must be the usual listening setup.")

    work = tempfile.mkdtemp(prefix="cubina-cal-")
    files = []
    for slot in TONE_SLOTS:
        path = os.path.join(work, f"tone_{slot}.wav")
        make_tone(path, slot * SLOT_HZ)
        files.append((slot, path))

    board = Board(find_command_board())
    print("Waiting for the command board to boot...")
    board.wait_for(r"Demand mode: Animation|CAL (LOADED|COMPILED)", 8)
    board.wait_for(r"Demand mode", 3)
    board.command("debug off")
    board.command("mode adc")
    before = board.command(f"profile use {args.profile}", r"^CAL (LOADED|COMPILED|ERR)")
    print(" ", before)
    list(board.lines(1.5))  # Let I2S start and fill its first window.

    # 1. Silence on the OLD path (nothing playing).
    board.command("ab old")
    print(f"Measuring {args.silence:.0f} s of silence (keep the room/source quiet)...")
    silence = board.measure(args.silence)
    print(f"  noise floor {silence['rms']:.1f} dBFS (loudest window {silence['rmsmax']:.1f}); "
          f"above 150 Hz {silence['gate']:.1f} dBFS (loudest {silence['gatemax']:.1f})")

    # 2. Raw response.
    print("Tone steps, uncorrected input:")
    raw = tone_sweep(board, files, "raw", silence)
    freqs = [slot * SLOT_HZ for slot, _ in raw]
    levels = [m["rms"] for _, m in raw]  # Input RMS is always uncorrected.
    ref = levels[TONE_SLOTS.index(REFERENCE_SLOT)]
    fc, err = fit_lowpass(freqs, levels)
    flat = fc > 12000
    print(f"  fitted first-order corner {fc:.0f} Hz, fit error {err:.2f} dB"
          + ("  -> response is already flat: correction off" if flat else ""))
    if err > 2.0:
        print("  WARNING: response is not a clean first-order low-pass; a per-band table may be needed.")

    # 3. Choose values.
    gate = 10 ** ((silence["gatemax"] + 6.0) / 20)  # Gate listens above 150 Hz; refined below.
    bass_noise_db = 10 * math.log10(sum(10 ** (silence["bands"][b] / 10) for b in range(3)))
    kick_min = max(0.0003, 4.0 * 10 ** (bass_noise_db / 20))
    board.command(f"set corr_hz {0 if flat else fc:.1f}")
    board.command(f"set gate {gate:.7f}")
    board.command(f"set kick_min {kick_min:.7f}")
    chosen = board.command("get")
    print(" ", chosen)

    # 4. Verify on the NEW path.
    board.command("ab new")
    print("Tone steps, corrected (NEW):")
    new = tone_sweep(board, files, "new", silence, [m["rms"] for _, m in raw])
    new_levels = [tone_level_db(m, slot) for slot, m in new]
    new_ref = new_levels[TONE_SLOTS.index(REFERENCE_SLOT)]
    deviation = [l - new_ref for l in new_levels]
    worst = max(abs(d) for d in deviation[:-1])  # Top tone sits near Nyquist; report separately.
    print("  corrected response vs 122 Hz: " + " ".join(f"{f:.0f}:{d:+.1f}" for f, d in zip(freqs, deviation)))
    # Convergent gate: raise 3 dB at a time until NEW silence is fully dark.
    for attempt in range(5):
        print(f"Measuring silence on NEW (gate {20 * math.log10(gate):.1f} dBFS)...")
        dark = board.measure(4.0)
        silent_ok = dark["vraw"] == 0 and max(dark["bins"]) == 0
        print(f"  max VRaw {dark['vraw']}, max bin {max(dark['bins'])} -> {'DARK' if silent_ok else 'NOT DARK'}")
        if silent_ok:
            break
        gate *= 10 ** (3.0 / 20)
        board.command(f"set gate {gate:.7f}")
    chosen = board.command("get")
    # The test tones' level says nothing about music level: the music player has
    # its own volume. Compare the gate with a known quiet passage from a capture
    # (Levels' breakdown at the usual volume read about -62 dBFS on 2026-11-19).
    print(f"  final gate {20 * math.log10(gate):.1f} dBFS")
    flat_ok = worst <= 2.0
    print(f"RESULT: response {'FLAT' if flat_ok else 'NOT FLAT'} (worst {worst:.1f} dB to 10 kHz), "
          f"silence {'DARK' if silent_ok else 'NOT DARK'}")

    if args.save:
        print(" ", board.command("save"))
    else:
        print("  Not saved: values last until the board reboots. Rerun with --save to store them.")
    board.command("debug on")

    os.makedirs(os.path.join(HERE, "captures"), exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    report = {
        "time": stamp, "profile": args.profile, "output": out_name, "mac_volume": volume, "before": before, "after": chosen,
        "saved": args.save, "silence": silence, "dark": dark,
        "raw": [{"hz": slot * SLOT_HZ, **m} for slot, m in raw],
        "new": [{"hz": slot * SLOT_HZ, **m} for slot, m in new],
        "fit": {"corner_hz": fc, "error_db": err, "flat": flat},
        "gate": gate, "kick_min": kick_min, "worst_flatness_db": worst,
        "flat_ok": flat_ok, "silent_ok": silent_ok,
    }
    path = os.path.join(HERE, "captures", f"calibration_{args.profile}_{stamp}.json")
    with open(path, "w") as f:
        json.dump(report, f, indent=1)
    with open(os.path.join(HERE, "captures", f"calibration_{args.profile}_{stamp}.log"), "w") as f:
        f.write("\n".join(board.log))
    print(f"Report: {path}")


if __name__ == "__main__":
    main()
