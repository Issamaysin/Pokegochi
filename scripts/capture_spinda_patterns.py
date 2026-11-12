"""Capture exact Spinda personality patterns from a physical ILI9341."""

from __future__ import annotations

import argparse
import re
import time
from pathlib import Path

import serial
from PIL import Image


CAPTURE = re.compile(
    rb"^\[SPINDA_CAP\] personality=([0-9A-F]{8}) shiny=([01]) "
    rb"width=(\d+) height=(\d+) committed=([01]) pixels=(\d+) data=([0-9A-F]+)$"
)


def rgb332(value: int) -> tuple[int, int, int]:
    return (
        ((value >> 5) & 0x07) * 255 // 7,
        ((value >> 2) & 0x07) * 255 // 7,
        (value & 0x03) * 255 // 3,
    )


def capture(port: serial.Serial, personality: int, shiny: bool,
            output: Path) -> Path:
    port.reset_input_buffer()
    port.write(f"ANIMSPINDA {personality:08X} {int(shiny)}\n".encode("ascii"))
    port.flush()
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        line = port.readline().strip()
        if not line:
            continue
        match = CAPTURE.match(line)
        if not match:
            if line == b"[SPINDA_CAP] REJECTED":
                raise RuntimeError("Physical Spinda fixture was rejected")
            continue
        width, height = int(match.group(3)), int(match.group(4))
        if match.group(5) != b"1":
            raise RuntimeError("Physical Spinda frame was not committed")
        encoded = match.group(7).decode("ascii")
        if len(encoded) != width * height * 2:
            raise RuntimeError("Physical Spinda payload is truncated")
        values = [int(encoded[index:index + 2], 16)
                  for index in range(0, len(encoded), 2)]
        image = Image.new("RGB", (width, height))
        image.putdata([rgb332(value) for value in values])
        image = image.resize((width * 4, height * 4),
                             Image.Resampling.NEAREST)
        destination = output / (
            f"spinda-{personality:08X}-{'shiny' if shiny else 'normal'}.png")
        destination.parent.mkdir(parents=True, exist_ok=True)
        image.save(destination)
        return destination
    raise TimeoutError("Timed out waiting for the Spinda physical capture")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM7")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--personalities", default="00000000,12345678,89ABCDEF,FFFFFFFF")
    parser.add_argument("--include-shiny", action="store_true")
    args = parser.parse_args()
    personalities = [int(value, 16)
                     for value in args.personalities.split(",")]
    port = serial.Serial()
    port.port = args.port
    port.baudrate = 115200
    port.timeout = 0.5
    port.write_timeout = 3
    port.dtr = False
    port.rts = False
    port.open()
    try:
        time.sleep(0.5)
        for personality in personalities:
            print(capture(port, personality, False, args.output), flush=True)
            if args.include_shiny:
                print(capture(port, personality, True, args.output), flush=True)
    finally:
        port.dtr = False
        port.rts = False
        port.close()


if __name__ == "__main__":
    main()
