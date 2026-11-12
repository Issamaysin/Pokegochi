"""Capture committed battle-animation frames from the physical ILI9341.

Requires the cable-only ``animation-profile`` firmware.  ANIMSETUP changes
only RAM, and ANIMCAP reads the panel GRAM after the requested presentation;
neither command writes the player's save or SD card.
"""

from __future__ import annotations

import argparse
import re
import time
from pathlib import Path

import serial
from PIL import Image


CAPTURE_RE = re.compile(
    rb"^\[ANIM_CAP\] width=(\d+) height=(\d+) present=(\d+) "
    rb"(?:format=RGB(?:565|332) )?captured=(\d+) data=([0-9A-F]*)$"
)
RENDER_RE = re.compile(rb"^\[R\](\d+),")


def rgb565(value: int) -> tuple[int, int, int]:
    return (
        ((value >> 11) & 0x1F) * 255 // 31,
        ((value >> 5) & 0x3F) * 255 // 63,
        (value & 0x1F) * 255 // 31,
    )


def rgb332(value: int) -> tuple[int, int, int]:
    return (
        ((value >> 5) & 0x07) * 255 // 7,
        ((value >> 2) & 0x07) * 255 // 7,
        (value & 0x03) * 255 // 3,
    )


def wait_for(port: serial.Serial, token: bytes, timeout: float) -> list[bytes]:
    deadline = time.monotonic() + timeout
    lines: list[bytes] = []
    while time.monotonic() < deadline:
        line = port.readline().strip()
        if not line:
            continue
        lines.append(line)
        # A 7 KiB physical-frame line can briefly fill the CH340/USB receive
        # path. PySerial may then return a timeout fragment without a newline,
        # splitting a later marker (for example ``[ANIM_D`` + ``IAG] DONE``)
        # across two reads. Match the rolling byte stream as well as one line.
        if token in line or token in b"".join(lines[-8:]):
            return lines
    rendered = b"\n".join(lines[-20:]).decode("utf-8", "replace")
    raise TimeoutError(f"Timed out waiting for {token!r}:\n{rendered}")


def command(port: serial.Serial, value: str, token: bytes,
            timeout: float) -> list[bytes]:
    port.write(value.encode("ascii") + b"\n")
    port.flush()
    return wait_for(port, token, timeout)


def save_capture(line: bytes, output: Path, byte_swap: bool) -> None:
    match = CAPTURE_RE.match(line)
    if not match or match.group(4) != b"1":
        raise RuntimeError(f"Invalid physical capture: {line[:240]!r}")
    width, height = int(match.group(1)), int(match.group(2))
    encoded = match.group(5).decode("ascii")
    pixel_count = width * height
    if len(encoded) not in (pixel_count * 2, pixel_count * 4):
        raise RuntimeError(
            f"Capture payload is {len(encoded)} hex chars, expected "
            f"{pixel_count * 2} or {pixel_count * 4}")
    rgb332_payload = len(encoded) == pixel_count * 2
    step = 2 if rgb332_payload else 4
    values = [int(encoded[index:index + step], 16)
              for index in range(0, len(encoded), step)]
    if byte_swap and not rgb332_payload:
        values = [((value & 0xFF) << 8) | (value >> 8) for value in values]
    image = Image.new("RGB", (width, height))
    image.putdata([(rgb332(value) if rgb332_payload else rgb565(value))
                   for value in values])
    scale = max(1, 320 // width)
    image = image.resize((width * scale, height * scale),
                         Image.Resampling.NEAREST)
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM7")
    parser.add_argument("--move", type=int, required=True)
    parser.add_argument("--side", type=int, choices=(0, 1), default=0)
    parser.add_argument("--turn", type=int, choices=(0, 1), default=0)
    parser.add_argument("--speed", type=int, choices=(1, 2, 3),
                        help="force the diagnostic playback speed after setup")
    parser.add_argument("--presents",
                        help="Comma-separated committed presentation numbers")
    parser.add_argument("--peak", action="store_true",
                        help="capture the richest committed frame for human review")
    parser.add_argument(
        "--sequence", action="store_true",
        help="capture every committed presentation (discovers the count from the diagnostic trace)",
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--byte-swap", action="store_true")
    args = parser.parse_args()
    selected_modes = int(args.peak) + int(bool(args.presents)) + int(args.sequence)
    if selected_modes != 1:
        parser.error("select exactly one of --peak, --presents or --sequence")
    presentations = ([int(value) for value in args.presents.split(",")]
                     if args.presents else [])

    # Configure modem-control lines before opening.  CH340 otherwise pulses
    # EN as SerialPort applies its defaults, racing ANIMSETUP against boot.
    port = serial.Serial()
    port.port = args.port
    port.baudrate = 115200
    port.timeout = 0.25
    port.write_timeout = 3
    port.dtr = False
    port.rts = False
    port.open()
    try:
        time.sleep(0.5)
        port.reset_input_buffer()
        command(port, "ANIMSETUP", b"[ANIM_DIAG] READY", 45)
        speed = args.speed or 3
        command(port, f"ANIMSPEED {speed}",
                f"[ANIM_DIAG] SPEED {speed}x".encode("ascii"), 5)
        if args.peak or args.sequence:
            lines = command(
                port,
                f"ANIMPEAK {args.move} {args.side} {args.turn}",
                b"[ANIM_DIAG] DONE",
                120,
            )
            capture = next((line for line in lines if line.startswith(
                b"[ANIM_CAP] width=")), None)
            if capture is None:
                raise RuntimeError("ANIMPEAK returned no physical frame")
            match = CAPTURE_RE.match(capture)
            if not match:
                raise RuntimeError(f"Invalid ANIMPEAK response: {capture[:240]!r}")
            presentation = int(match.group(3))
            if args.peak:
                suffix = (f"move-{args.move}-side-{args.side}-turn-{args.turn}"
                          f"-peak-p{presentation}.png")
                save_capture(capture, args.output / suffix, args.byte_swap)
                print(args.output / suffix)
            else:
                rendered = next((RENDER_RE.match(line) for line in lines
                                 if RENDER_RE.match(line)), None)
                if rendered is None:
                    raise RuntimeError("ANIMPEAK returned no render presentation count")
                presentations = list(range(1, int(rendered.group(1)) + 1))
                print(f"sequence presentations={len(presentations)}", flush=True)
        for presentation in presentations:
            lines = command(
                port,
                f"ANIMCAP {args.move} {args.side} {args.turn} {presentation}",
                b"[ANIM_DIAG] DONE",
                120,
            )
            capture = next((line for line in lines if line.startswith(
                b"[ANIM_CAP] width=")), None)
            if capture is None:
                raise RuntimeError("ANIMCAP returned no physical frame")
            suffix = f"move-{args.move}-side-{args.side}-turn-{args.turn}-p{presentation}.png"
            save_capture(capture, args.output / suffix, args.byte_swap)
            print(args.output / suffix)
    finally:
        port.dtr = False
        port.rts = False
        port.close()


if __name__ == "__main__":
    main()
