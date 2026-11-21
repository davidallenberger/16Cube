#!/usr/bin/env python3
"""Reference beats and onsets for a song, for scoring the cube's detectors.

Mac-side only: this uses librosa (an offline music-analysis library) and is
never part of either firmware. It looks at the whole song at once, so it is a
stronger (non-real-time) estimate than anything the command board can do.
Treat it as a reference, not ground truth: check suspicious songs by ear.

    python reference.py song.wav out.json

Install once into a throwaway environment (not the PlatformIO one):
    python3 -m venv /tmp/ref_venv && /tmp/ref_venv/bin/pip install librosa soundfile
"""
import json
import sys

import librosa
import numpy as np


def align_to_kick(y, sr, beats):
    """librosa can lock to off-beat open hi-hats (it did on Levels and
    Sandstorm). Fold the rise of 40-100 Hz energy (the kick) over the beat
    grid; if kicks land nearer the half beat, move every beat by half."""
    from scipy.signal import butter, sosfilt
    if len(beats) < 8:
        return beats, False
    x = sosfilt(butter(4, [40, 100], btype="band", fs=sr, output="sos"), y)
    hop = int(sr * 0.004)
    env = np.sqrt(np.convolve(x * x, np.ones(hop) / hop, "same"))[::hop]
    rise = np.maximum(0, np.diff(env, prepend=env[0]))
    t = np.arange(len(rise)) * hop / sr
    i = np.searchsorted(beats, t, side="right") - 1
    ok = (i >= 0) & (i + 1 < len(beats))
    phase = (t[ok] - beats[i[ok]]) / (beats[i[ok] + 1] - beats[i[ok]])
    # Circular mean of kick-rise phase: near 0 = on the beat, near 0.5 = off.
    angle = np.angle(np.sum(rise[ok] * np.exp(2j * np.pi * phase)))
    if abs(angle) < np.pi / 2:
        return beats, False
    mid = (beats[:-1] + beats[1:]) / 2
    return mid, True


def main():
    wav, out = sys.argv[1], sys.argv[2]
    y, sr = librosa.load(wav, sr=None, mono=True)
    hop = 256  # ~8 ms at 31,250 Hz.
    onset_env = librosa.onset.onset_strength(y=y, sr=sr, hop_length=hop)
    tempo, beats = librosa.beat.beat_track(onset_envelope=onset_env, sr=sr, hop_length=hop, units="time")
    onsets = librosa.onset.onset_detect(onset_envelope=onset_env, sr=sr, hop_length=hop, units="time")
    # Dense list (lower peak threshold): "is this hit a real onset at all?"
    dense = librosa.onset.onset_detect(onset_envelope=onset_env, sr=sr, hop_length=hop, units="time",
                                       delta=0.02, wait=3)
    beats, shifted = align_to_kick(y, sr, np.asarray(beats))
    # Onset strength at each onset, normalized to the song (0..1, 95th pct = 1),
    # so "strong for this song" can be scored separately from every onset.
    frames = librosa.time_to_frames(onsets, sr=sr, hop_length=hop)
    strength = onset_env[np.clip(frames, 0, len(onset_env) - 1)]
    scale = float(np.percentile(strength, 95)) if len(strength) else 1.0
    # Band onsets: low (<150 Hz, kicks), mid (150-2k), high (>2k).
    bands = {}
    spec = np.abs(librosa.stft(y, n_fft=1024, hop_length=hop))
    freqs = librosa.fft_frequencies(sr=sr, n_fft=1024)
    for name, lo, hi in (("low", 0, 150), ("mid", 150, 2000), ("high", 2000, sr / 2)):
        sel = (freqs >= lo) & (freqs < hi)
        env = librosa.onset.onset_strength(S=librosa.amplitude_to_db(spec[sel], ref=np.max), sr=sr, hop_length=hop)
        bands[name] = [round(float(t), 4) for t in
                       librosa.onset.onset_detect(onset_envelope=env, sr=sr, hop_length=hop, units="time")]
    result = {
        "wav": wav,
        "duration": round(float(len(y) / sr), 3),
        "tempo": round(float(np.atleast_1d(tempo)[0]), 2),
        "kick_aligned_shift": bool(shifted),
        "beats": [round(float(t), 4) for t in beats],
        "onsets": [round(float(t), 4) for t in onsets],
        "onsets_dense": [round(float(t), 4) for t in dense],
        "onset_strength": [round(float(s / scale), 3) for s in strength],
        "band_onsets": bands,
    }
    with open(out, "w") as f:
        json.dump(result, f)
    print(f"{wav}: tempo {result['tempo']} BPM, {len(beats)} beats (half-beat shift to kick: {shifted}), {len(onsets)} onsets, "
          f"low/mid/high {len(bands['low'])}/{len(bands['mid'])}/{len(bands['high'])}")


if __name__ == "__main__":
    main()
