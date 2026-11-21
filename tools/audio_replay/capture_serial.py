#!/usr/bin/env python3
"""Record the command board's USB serial log, with Mac-side timestamps.

Run with PlatformIO's bundled Python (it already has pyserial):

    ~/.platformio/penv/bin/python tools/audio_replay/capture_serial.py levels

It finds the command board (CP210x on hub location 0-1.4, or the only CP210x
present), writes every line to tools/audio_replay/captures/<name>.log prefixed
with seconds since the capture started, and stops on Ctrl-C. It only reads:
it never resets, flashes, or sends anything to the board.
"""
import os
import sys
import time

import serial
import serial.tools.list_ports

CP210X = (0x10C4, 0xEA60)


def find_port():
    ports = [p for p in serial.tools.list_ports.comports() if (p.vid, p.pid) == CP210X]
    preferred = [p for p in ports if (p.location or "").endswith("0-1.4")]
    if preferred:
        return preferred[0].device
    if len(ports) == 1:
        return ports[0].device
    if not ports:
        sys.exit("No command board found. Plug it in by USB (it is a CP210x serial device).")
    sys.exit("Several CP210x boards and none at hub location 0-1.4: " +
             ", ".join(f"{p.device} ({p.location})" for p in ports))


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "capture"
    here = os.path.dirname(os.path.abspath(__file__))
    os.makedirs(os.path.join(here, "captures"), exist_ok=True)
    path = os.path.join(here, "captures", name + ".log")
    device = find_port()
    # Keep DTR/RTS low so opening the port does not reset the ESP32.
    port = serial.Serial()
    port.port = device
    port.baudrate = 115200
    port.timeout = 0.2
    port.dtr = False
    port.rts = False
    port.open()
    start = time.monotonic()
    lines = 0
    audio_lines = 0
    # On macOS the CP2102 driver may still pulse reset while opening, so the
    # board can reboot into Animation mode. Select ADC AFTER this starts.
    print(f"Recording {device} -> {path}\nAFTER this line, press ADC on the command board (opening the port may "
          "reboot it), start the song, Ctrl-C when it ends.")
    with open(path, "w") as out:
        buffer = b""
        try:
            while True:
                buffer += port.read(4096)
                while b"\n" in buffer:
                    raw, buffer = buffer.split(b"\n", 1)
                    text = raw.decode("utf-8", "replace").rstrip("\r")
                    out.write(f"{time.monotonic() - start:9.3f} {text}\n")
                    lines += 1
                    if text.startswith("ADC: InRMS="):
                        audio_lines += 1
                        if audio_lines % 20 == 1:
                            print(f"  {time.monotonic() - start:6.0f}s  {text[:90]}")
                out.flush()
        except KeyboardInterrupt:
            pass
    print(f"\nSaved {lines} lines ({audio_lines} audio summaries) to {path}")
    if audio_lines == 0:
        print("No 'ADC: InRMS=' lines: was the board in ADC mode with COMMAND_AUDIO_DEBUG=1?")


if __name__ == "__main__":
    main()
