#!/usr/bin/env python3
"""Exercise Cubina's unmodified TempoTracker through modeled waveform/FFT input.

Usage: python3 run_tempo_suite.py --project /path/to/16Cube --output results
Requires Python 3, NumPy, and g++. No firmware, Git, or serial port is changed.
"""

import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import time

import numpy as np


RUNNER = r'''
#include <cstdio>
#include <cassert>
#include <cmath>
#include "TempoTracker.h"
int main(int argc, char **argv) {
    assert(argc == 4);
    FILE *input = fopen(argv[1], "rb");
    FILE *output = fopen(argv[2], "wb");
    assert(input && output);
    const float seconds = std::stof(argv[3]);
    TempoTracker tracker;
    float row[18];
    while (fread(row, sizeof(float), 18, input) == 18) {
        if (row[17] != 0) tracker.reset();
        const auto result = tracker.process(row, row[16] != 0, seconds);
        assert(std::isfinite(result.bpm) && std::isfinite(result.phase));
        assert(result.phase >= 0 && result.phase < 1);
        assert(result.confidence >= 0 && result.confidence <= 1);
        assert(result.bpm == 0 || (result.bpm >= 60 && result.bpm <= 200));
        const float values[] = {result.bpm, result.phase, result.confidence,
                                float(result.active), float(result.locked), float(result.attack.event),
                                result.attack.strength, float(result.attack.bands)};
        assert(fwrite(values, sizeof(float), 8, output) == 8);
    }
    fclose(input);
    fclose(output);
}
'''


def cases():
    result = []
    def add(name, **values):
        case = dict(name=name, bpm=120.0, duration=28.0, start=2.137,
                    peak=.08, decay=.045, carrier=70.0, bed=.002,
                    hats=0.0, pattern='regular', kind='rhythmic', gain=1.0,
                    noise=0.0, seed=123, evaluate_from=16.0)
        case.update(values)
        result.append(case)
    for bpm in [60, 75, 90, 100, 110, 120, 128, 140, 150, 170, 180, 200]:
        add(f'clean_{bpm}', bpm=float(bpm), group='clean')
    for bpm in [60, 90, 120, 128, 150, 180, 200]:
        add(f'offbeats_{bpm}', bpm=float(bpm), hats=.008, group='subdivisions')
    for ratio in [.5, 1.0, 2.0, 4.0]:
        add(f'hat_ratio_{ratio:g}', hats=.08 * ratio, group='competing_accents')
    for peak in [.02, .008, .002]:
        add(f'level_{peak:g}', peak=peak, bed=peak / 40,
            hats=peak / 10, group='level')
    add('near_floor_gain1', peak=.0005, bed=.0000125, kind='quiet', group='activity_gate')
    add('near_floor_gain4', peak=.0005, bed=.0000125, gain=4, group='activity_gate')
    add('noise_floor_gate', peak=.0001, bed=0, noise=.000035,
        kind='quiet', group='activity_gate')
    add('line_noise', noise=.000035, hats=.008, group='noise')
    add('noisy_line', noise=.0003, hats=.008, group='noise')
    add('missing_every_fifth', pattern='missing', hats=.008, group='missing_accents')
    add('alternating_accents', pattern='accents', hats=.008, group='missing_accents')
    add('swung_offbeats', pattern='swing', hats=.008, group='subdivisions')
    add('jitter_5ms', pattern='jitter', hats=.008, group='jitter')
    add('treble_beats_only', carrier=3000, group='timbre')
    add('midrange_beats_only', carrier=700, group='timbre')
    add('syllable_like_bursts', carrier=1500, pattern='arrhythmic', kind='aperiodic', group='attacks')
    for carrier in [55, 100, 150]:
        add(f'kick_carrier_{carrier}', carrier=carrier, group='timbre')
    for decay in [.008, .15, .3]:
        add(f'decay_{decay:g}', decay=decay, group='envelope')
    add('sparse_60', bpm=60, bed=0, group='sparse_activity')
    add('sparse_90', bpm=90, bed=0, group='sparse_activity')
    add('sparse_120', bpm=120, bed=0, group='sparse_activity')
    add('opposite_polarity', stereo='opposite', hats=.008, group='stereo')
    add('single_channel', stereo='single', hats=.008, group='stereo')
    add('split_stereo', stereo='split', hats=.08, group='stereo')
    add('start_offset_003', start=2.003, hats=.008, group='start_offset')
    add('start_offset_247', start=2.247, hats=.008, group='start_offset')
    add('step_120_150', pattern='step', second_bpm=150, change=14,
        duration=38, evaluate_from=28, hats=.008, group='tempo_changes')
    add('step_120_90', pattern='step', second_bpm=90, change=14,
        duration=38, evaluate_from=28, hats=.008, group='tempo_changes')
    add('ramp_100_140', bpm=100, pattern='ramp', second_bpm=140,
        duration=42, evaluate_from=24, hats=.008, group='tempo_changes')
    add('silence_then_150', pattern='restart', second_bpm=150,
        change=20.137, duration=38, evaluate_from=30, hats=.008, group='recovery')
    add('read_gap_reset', gap=(13.5, 14), duration=32,
        evaluate_from=25, hats=.008, group='recovery')
    add('clock_60s_wrap', duration=72, evaluate_from=60,
        hats=.008, group='clock_wrap')
    for pattern in ['silence', 'steady70', 'steady976', 'noise', 'arrhythmic']:
        add('negative_' + pattern, pattern=pattern, kind='aperiodic',
            evaluate_from=10, group='negative_controls')
    add('negative_steady70_noise35u', pattern='steady70', noise=.000035,
        kind='aperiodic', evaluate_from=10, group='negative_controls')
    add('negative_steady70_noise300u', pattern='steady70', noise=.0003,
        kind='aperiodic', evaluate_from=10, group='negative_controls')
    add('negative_steady_bincenter', pattern='steady61.03515625',
        kind='aperiodic', evaluate_from=10, group='negative_controls')
    return result


def make_wave(case, rate):
    t = np.arange(int(case['duration'] * rate)) / rate
    rng = np.random.default_rng(case['seed'])
    x = np.maximum(t - case['start'], 0)
    bpm = np.full_like(t, case['bpm'])
    position = x * case['bpm'] / 60
    if case['pattern'] == 'step':
        before = np.maximum(np.minimum(t, case['change']) - case['start'], 0)
        after = np.maximum(t - case['change'], 0)
        position = (before * case['bpm'] + after * case['second_bpm']) / 60
        bpm[t >= case['change']] = case['second_bpm']
    elif case['pattern'] == 'ramp':
        slope = (case['second_bpm'] - case['bpm']) / (case['duration'] - case['start'])
        position = (case['bpm'] * x + .5 * slope * x * x) / 60
        bpm += slope * x
    elif case['pattern'] == 'restart':
        resumed = np.maximum(t - case['change'], 0)
        later = t >= case['change']
        position[later] = resumed[later] * case['second_bpm'] / 60
        bpm[later] = case['second_bpm']

    # A quiet continuous tone keeps "audible" true in the core isolation
    # cases. Separate sparse cases remove it to test the activity policy.
    bed = case['bed'] * np.sin(2 * np.pi * 997 * t)
    bed[t < case['start']] = 0
    if case['pattern'] == 'restart':
        bed[(t >= 13) & (t < case['change'])] = 0
    left = bed.copy()
    right = bed.copy()
    beat_times = []
    if case['pattern'] == 'restart':
        beat_times = list(np.arange(case['start'], 13, 60 / case['bpm']))
        beat_times += list(np.arange(case['change'], case['duration'], 60 / case['second_bpm']))
    elif case['pattern'] == 'arrhythmic':
        current = case['start']
        while current < case['duration']:
            beat_times.append(current)
            current += rng.uniform(.13, .97)
    else:
        selected = t >= case['start']
        beat_times = list(np.interp(np.arange(int(position[-1]) + 1),
                                    position[selected], t[selected]))

    def pulse(destination, onset, amplitude, carrier, decay):
        start = int(round(onset * rate))
        count = min(int(max(.04, decay * 9) * rate), len(destination) - start)
        if start < 0 or count <= 0:
            return
        age = np.arange(count) / rate
        envelope = np.exp(-age / decay)
        # Fade the negligible tail rather than introduce an extra hard edge.
        fade = min(int(.002 * rate), count)
        envelope[-fade:] *= np.linspace(1, 0, fade)
        destination[start:start + count] += amplitude * np.sin(2 * np.pi * carrier * age) * envelope

    for index, onset in enumerate(beat_times):
        amplitude = case['peak']
        if case['pattern'] == 'missing' and index % 5 == 3:
            amplitude = 0
        if case['pattern'] == 'accents' and index % 2:
            amplitude *= .2
        if case['pattern'] == 'jitter':
            onset += rng.uniform(-.005, .005)
        pulse(left, onset, amplitude, case['carrier'], case['decay'])
        if case.get('stereo') != 'split':
            pulse(right, onset, amplitude, case['carrier'], case['decay'])
        if index + 1 < len(beat_times):
            fraction = 2 / 3 if case['pattern'] == 'swing' else .5
            hat_time = onset + fraction * (beat_times[index + 1] - beat_times[index])
            if case.get('stereo') != 'split':
                pulse(left, hat_time, case['hats'], 3500, .008)
            pulse(right, hat_time, case['hats'], 3500, .008)

    if case.get('stereo') == 'opposite':
        right = -left
    elif case.get('stereo') == 'single':
        right[:] = 0
    if case['pattern'] == 'silence':
        left[:] = right[:] = 0
    elif case['pattern'].startswith('steady'):
        frequency = 976.5625 if case['pattern'] == 'steady976' else float(case['pattern'][6:])
        left = .04 * np.sin(2 * np.pi * frequency * t)
        right = left.copy()
    elif case['pattern'] == 'noise':
        left = rng.normal(0, .004, len(t))
        right = rng.normal(0, .004, len(t))
    if case['noise']:
        left += rng.normal(0, case['noise'], len(t))
        right += rng.normal(0, case['noise'], len(t))
    return t, left, right, position, bpm


def analyze(case, rate, window_size, hop, edges, controls):
    t, left, right, position, expected_bpm = make_wave(case, rate)
    ends = np.arange(window_size, len(left) + 1, hop)
    times = ends / rate
    # This is the same per-window DC removal, channel power selection and
    # Hamming/Parseval convention as AudioAnalysis.h, using a reference FFT.
    l = np.lib.stride_tricks.sliding_window_view(left, window_size)[::hop].copy()
    r = np.lib.stride_tricks.sliding_window_view(right, window_size)[::hop].copy()
    l -= l.mean(axis=1, keepdims=True)
    r -= r.mean(axis=1, keepdims=True)
    pl = np.mean(l * l, axis=1)
    pr = np.mean(r * r, axis=1)
    rms = np.sqrt((pl + pr) / 2)
    selected = np.where((pr > pl)[:, None], r, l)
    weights = .54 - .46 * np.cos(2 * np.pi * np.arange(window_size) / (window_size - 1))
    magnitude = np.abs(np.fft.rfft(selected * weights, axis=1))
    bands = np.stack([np.sqrt(2 * np.sum(magnitude[:, a:b] ** 2, axis=1) /
                              (window_size * np.sum(weights ** 2)))
                      for a, b in zip(edges[:-1], edges[1:])], axis=1)
    calibrated = rms * 255 * controls['master'] * case['gain'] / controls['full_scale']
    audible = (rms > controls['floor']) & (calibrated >= 5)
    reset = np.zeros(len(times))
    if 'gap' in case:
        start, end = case['gap']
        keep = ~((times >= start) & ((ends - window_size) / rate < end))
        first_after = np.flatnonzero(keep & ((ends - window_size) / rate >= end))[0]
        reset[first_after] = 1
    else:
        keep = np.ones(len(times), dtype=bool)
    rows = np.column_stack((bands, audible, reset))[keep].astype(np.float32)
    indices = np.minimum(ends - 1, len(position) - 1)[keep]
    return times[keep], rows, position[indices] % 1, expected_bpm[indices]


def metrics(case, times, output, truth_phase, truth_bpm):
    selected = times >= case['evaluate_from']
    values = output[selected]
    phase = truth_phase[selected]
    bpm = truth_bpm[selected]
    locked = values[:, 4] > .5
    active = values[:, 3] > .5
    confidence = values[:, 2]
    result = dict(case=case['name'], group=case['group'], kind=case['kind'],
                  locked_pct=float(np.mean(locked) * 100),
                  active_pct=float(np.mean(active) * 100),
                  lock_transitions=int(np.sum(np.diff(locked.astype(int)) != 0)),
                  confidence_mean=float(np.mean(confidence)),
                  evaluate_from=case['evaluate_from'])
    if case['kind'] == 'rhythmic':
        errors = np.abs(values[:, 0] - bpm)
        result['bpm_mae'] = float(np.mean(errors))
        result['bpm_within3_pct'] = float(np.mean(errors <= 3) * 100)
        result['median_bpm'] = float(np.median(values[:, 0]))
        if np.any(locked):
            difference = (values[locked, 1] - phase[locked] + .5) % 1 - .5
            absolute_ms = np.abs(difference) * 60000 / bpm[locked]
            mean_complex = np.mean(np.exp(2j * np.pi * difference))
            result['phase_mae_ms_locked'] = float(np.mean(absolute_ms))
            result['phase_p95_ms_locked'] = float(np.percentile(absolute_ms, 95))
            result['phase_p95_cycles_locked'] = float(np.percentile(np.abs(difference), 95))
            result['phase_coherence_locked'] = float(np.abs(mean_complex))
        else:
            result['phase_mae_ms_locked'] = None
            result['phase_p95_ms_locked'] = None
            result['phase_p95_cycles_locked'] = None
            result['phase_coherence_locked'] = None
        # These proposed engineering targets are declared independently of
        # results. Report each dimension; do not turn a good final BPM into
        # a blanket pass for lock/phase behavior.
        result['bpm_target_met'] = result['bpm_within3_pct'] >= 95
        result['lock_target_met'] = result['locked_pct'] >= 90
        result['phase_target_met'] = (result['phase_p95_cycles_locked'] is not None and
                                      result['phase_p95_cycles_locked'] <= .10)
    else:
        result['negative_target_met'] = result['locked_pct'] <= 5
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('tempo_results'))
    parser.add_argument('--only', nargs='+', help='Run just the named cases for a focused follow-up.')
    args = parser.parse_args()
    project = args.project.resolve()
    output_dir = args.output.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    command = project / 'command-board/src'
    analysis = (command / 'AudioAnalysis.h').read_text()
    capture = (command / 'SignalAcquisition.cpp').read_text()
    def integer(name):
        return int(re.search(r'constexpr size_t ' + name + r' = (\d+);', analysis).group(1))
    rate = integer('SampleRate')
    window_size = integer('WindowSamples')
    hop = integer('HopSamples')
    edges = [int(value) for value in re.search(r'BandEdges\[17\] = \{([^}]+)\}', analysis).group(1).split(',')]
    def setting(name):
        return float(re.search(r'float ' + name + r' = ([0-9.]+)f?;', capture).group(1))
    controls = dict(master=setting('uiMasterSensitivity'), full_scale=setting('uiFullScaleRMS'),
                    floor=setting('uiNoiseFloorRMSADC'))
    suite = cases()
    if args.only:
        known = {case['name'] for case in suite}
        if set(args.only) - known:
            parser.error('Unknown cases: ' + ', '.join(sorted(set(args.only) - known)))
        suite = [case for case in suite if case['name'] in args.only]
    results = []
    started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix='cubina-tempo-suite-') as directory:
        directory = Path(directory)
        cpp = directory / 'runner.cpp'
        cpp.write_text('#include <string>\n' + RUNNER)
        binary = directory / 'runner'
        subprocess.run(['g++', '-std=c++11', '-O2', '-I', str(command), str(cpp),
                        '-o', str(binary)], check=True)
        for index, case in enumerate(suite, 1):
            times, rows, truth_phase, truth_bpm = analyze(case, rate, window_size, hop, edges, controls)
            rows.tofile(directory / 'input.bin')
            subprocess.run([str(binary), str(directory / 'input.bin'),
                            str(directory / 'output.bin'), str(hop / rate)], check=True)
            output = np.fromfile(directory / 'output.bin', dtype=np.float32).reshape(-1, 8)
            assert len(output) == len(times)
            result = metrics(case, times, output, truth_phase, truth_bpm)
            results.append(result)
            trace = np.column_stack((times, truth_bpm, truth_phase, output))
            np.savez_compressed(output_dir / (case['name'] + '.npz'), trace=trace)
            (output_dir / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
            bpm_error = f"{result['bpm_mae']:.2f}" if 'bpm_mae' in result else 'n/a'
            print(f"[{index:02d}/{len(suite)}] {case['name']}: lock={result['locked_pct']:.1f}% "
                  f"BPM_MAE={bpm_error} "
                  f"phase_MAE_ms={result.get('phase_mae_ms_locked')}", flush=True)
    metadata = dict(sample_rate=rate, window_samples=window_size, hop_samples=hop,
                    source_controls=controls, cases=suite,
                    source_sha256={name: hashlib.sha256((command / name).read_bytes()).hexdigest()
                                   for name in ['TempoTracker.h', 'AudioAnalysis.h', 'SignalAcquisition.cpp']},
                    host_seconds=time.monotonic() - started,
                    scope='Unmodified production tracker; modeled waveform, RMS gate and reference float FFT. No electrical/ESP32 timing test.')
    (output_dir / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
    fields = sorted(set().union(*(row.keys() for row in results)))
    with (output_dir / 'results.csv').open('w', newline='') as output:
        writer = csv.DictWriter(output, fieldnames=fields)
        writer.writeheader()
        writer.writerows(results)
    print('Completed', len(results), 'cases; results in', output_dir)


if __name__ == '__main__':
    main()
