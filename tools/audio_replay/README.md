# Audio replay

Runs the **unmodified** command-board analysis (`SignalAcquisition.cpp`, the
real arduinoFFT) on a WAV file on the Mac and logs every published
`AudioPayload`. Use it to baseline and A/B audio-processing changes without
hardware.

```bash
tools/audio_replay/build.sh
afconvert -f WAVE -d LEF32@31250 --src-complexity bats -r 127 song.mp3 song.wav
tools/audio_replay/audio_replay song.wav --no-normalize --chain-db -34 --bpm 126 --csv song.csv
```

Defaults model the current hardware: `--pcm-lowpass 203` (the ADC module's
filter as measured by `calibrate.py`) and the firmware's NEW processing (AGC + input correction).
`--path old` replays the previous processing; `--pcm-lowpass 0 --correct 0`
models an ideal flat module. With the filter modeled, replay and board
spectra agree within about 1 dB in every band (Levels, Apple Music at Mac
volume 9: `--no-normalize --chain-db -34`).

## Hardware calibration profiles (`calibrate.py`)

One run per hardware configuration (source + digitizer), no button presses:

```bash
~/.platformio/penv/bin/python tools/audio_replay/calibrate.py --profile bt-pcm1808 --save
```

It plays test tones through the Mac's default output at the current volume,
drives the command board over USB, measures silence and the frequency
response, fits the module's low-pass corner, sets the noise gate (which
listens above 150 Hz) until silence is dark, verifies NEW is flat, and with
`--save` stores the profile on the board. Each input (ADC, Mic) remembers its
assigned profile; `mic-inmp441` defaults to no correction. A future source on
the same input, e.g. a DJ deck, gets its own profile (`--profile dj-pcm1808`).
Reports land in `captures/` (git-ignored).

2026-11-19, `bt-pcm1808` (T88 receiver, Mac volume 56/100): corner 203.2 Hz
(fit error 0.05 dB), corrected response within +/-0.3 dB from 122 Hz to
6.4 kHz (-1.3 dB at 10 kHz, -2.4 dB at 12.8 kHz), gate -84.3 dBFS, silence dark.

## Level calibration

The listening chain (player volume, Mac volume, Bluetooth receiver, PCM1808
input) behaves as one gain. The replay models it as loudness normalization
(`--target-lufs`, default Spotify's -14; `--no-normalize` to skip) plus
`--chain-db`.

To calibrate, record the command board while a song plays, then align:

```bash
~/.platformio/penv/bin/python tools/audio_replay/capture_serial.py NAME   # select ADC after it starts
python3 tools/audio_replay/calibrate_chain.py tools/audio_replay/captures/NAME.log song.wav
```

The result is the total file-to-ADC gain; subtract the replay's printed
normalization to get `--chain-db`.

### 2026-11-19 first calibration (Spotify, normalization assumed -14 LUFS; made BEFORE the module filter was modeled, so superseded by the -34 dB Apple Music figure above)

| Song (YouTube MP3) | Total gain | Normalization | Chain |
|---|---|---|---|
| Levels | -40.1 dB | -4.5 dB | -35.6 dB |
| Sandstorm (first 39 s) | -40.7 dB | -5.1 dB | -35.6 dB |

Against the board log at -35.6 dB, the replay matched kick counts (292 vs
299), tempo lock pattern and BPM (126.0 both), and quiet-section volume. The
board's drops ran ~5 dB hotter and produced ~40% more attacks, most likely
because Spotify's master differs from the YouTube MP3. Playing the same file
from the Mac would remove that difference.

Captures and decoded audio stay local (`.gitignore`).
