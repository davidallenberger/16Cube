# Render replay

Runs the **real** render-board audio routines on the Mac: the real
UARTReceiver and AudioFrameClock, the real effects, and the real FastLED,
texture, geometry, shape and particle engines (FastLED's host "stub"
platform). Input is a song replayed through the command-board analysis
(`tools/audio_replay`), turned into the board's 60 Hz packets.

Each frame is logged with what the routine drew (fill, brightness, motion,
height, spread) next to what it was given (volume, bands, hits, tempo, beat),
plus the routine's own debug line. Timing follows the board: a 60 Hz cap in
`showCube`, routines' own `delay()` steps, and a clock that only the replay
advances, so runs are deterministic.

```bash
tools/render_replay/build.sh                 # FastLED is compiled for the host once
tools/render_replay/render_replay --list
tools/render_replay/render_replay song.csv "GEQ" frames.csv
tools/render_replay/run_all.sh WAV_DIR       # everything -> reports/audio_routines.md
```

`report.py` summarizes each routine for both songs, NEW vs OLD processing.
It is the measurable side of each routine's intent: the user describes what a
routine should look like, and the report is checked against that description.

Not modeled: the physical LEDs' color rendering and power limits, and the
command board's exact serial timing (packets are the latest analysis result at
each 60 Hz frame, with attacks merged between frames).
