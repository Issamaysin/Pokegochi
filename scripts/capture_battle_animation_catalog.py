"""Capture one real panel peak frame for every authored move.

This is a collection tool, not an automated visual validator.  It drives the
cable-only animation-profile firmware, saves the richest committed ILI9341
frame for each requested move, and builds labelled contact sheets for a human
to inspect move by move.
"""

from __future__ import annotations

import argparse
import json
import re
import time
from pathlib import Path

import serial
from PIL import Image, ImageDraw, ImageFont

from capture_battle_animation_frame import CAPTURE_RE, command, save_capture


EXTREME_RE = re.compile(
    rb"^\[F\](\d+),(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)$"
)


def parse_moves(value: str | None, available: list[int]) -> list[int]:
    if not value:
        return available
    selected: set[int] = set()
    for part in value.split(","):
        bounds = part.strip().split("-", 1)
        first = int(bounds[0])
        last = int(bounds[1]) if len(bounds) == 2 else first
        selected.update(range(min(first, last), max(first, last) + 1))
    available_set = set(available)
    return sorted(selected & available_set)


def contact_sheets(entries: list[tuple[int, str, int, Path]], output: Path,
                   prefix: str, page_size: int = 25) -> None:
    if not entries:
        return
    columns = 5
    label_height = 30
    cell_width = 320
    cell_height = 122
    font = ImageFont.load_default()
    for page, start in enumerate(range(0, len(entries), page_size), 1):
        page_entries = entries[start:start + page_size]
        rows = (len(page_entries) + columns - 1) // columns
        sheet = Image.new("RGB", (columns * cell_width, rows * cell_height),
                          (28, 28, 28))
        draw = ImageDraw.Draw(sheet)
        for index, (move, symbol, presentation, path) in enumerate(page_entries):
            image = Image.open(path).convert("RGB")
            # The cable payload samples the physical 320x184 arena to 76x23 so
            # it remains reliable over the CH340 serial bridge. Enlarge only
            # for the human contact sheet, with nearest-neighbour sampling so
            # no interpolation invents or hides animation pixels.
            if image.width <= 80 and image.height <= 24:
                image = image.resize((image.width * 4, image.height * 4),
                                     Image.Resampling.NEAREST)
            x = (index % columns) * cell_width
            y = (index // columns) * cell_height
            sheet.paste(image, (x + (cell_width - image.width) // 2,
                                y + label_height))
            draw.text((x + 6, y + 6),
                      f"{move:03d} {symbol}  peak p{presentation}",
                      fill=(255, 255, 255), font=font)
        target = output / f"{prefix}-sheet-{page:03d}.png"
        target.parent.mkdir(parents=True, exist_ok=True)
        sheet.save(target)


def write_catalog_results(entries: list[tuple[int, str, int, Path]],
                          failures: list[str], output: Path, side: int,
                          turn: int, requested: int,
                          arena_extremes: list[dict[str, int | str]] | None = None) -> None:
    prefix = f"side-{side}-turn-{turn}"
    contact_sheets(entries, output / "sheets", prefix)
    manifest = {
        "side": side,
        "turn": turn,
        "captured": len(entries),
        "requested": requested,
        "failures": failures,
        "arenaExtremes": arena_extremes or [],
    }
    (output / f"{prefix}-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM7")
    parser.add_argument("--audit", type=Path,
                        default=Path(".generated/battle_animation_audit.json"))
    parser.add_argument("--moves", help="comma-separated IDs/ranges")
    parser.add_argument("--side", type=int, choices=(0, 1), default=0)
    parser.add_argument("--turn", type=int, choices=(0, 1), default=0)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--setup-every", type=int, default=0,
        help=("rebuild the fixture every N captures; disabled by default "
              "because repeated ANIMSETUP calls accumulate diagnostic-only "
              "scene state on the ESP32"))
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("--no-resume", action="store_true")
    parser.add_argument(
        "--sheets-only", action="store_true",
        help="rebuild sheets/manifest from captures already on disk")
    args = parser.parse_args()

    audit = json.loads(args.audit.read_text(encoding="utf-8"))
    move_rows: dict[str, dict[str, object]] = audit["moves"]
    available = sorted(int(value) for value in move_rows)
    moves = parse_moves(args.moves, available)
    args.output.mkdir(parents=True, exist_ok=True)
    entries: list[tuple[int, str, int, Path]] = []
    failures: list[str] = []
    arena_extremes: list[dict[str, int | str]] = []

    if args.sheets_only:
        for ordinal, move in enumerate(moves, 1):
            symbol = str(move_rows[str(move)]["symbol"])
            existing = sorted(args.output.glob(
                f"move-{move}-side-{args.side}-turn-{args.turn}-peak-p*.png"))
            if not existing:
                failures.append(f"{move} {symbol}: missing physical capture")
                continue
            match = re.search(r"-p(\d+)\.png$", existing[-1].name)
            presentation = int(match.group(1)) if match else 0
            entries.append((move, symbol, presentation, existing[-1]))
            print(f"[{ordinal}/{len(moves)}] sheet {move} {symbol}", flush=True)
        write_catalog_results(entries, failures, args.output, args.side,
                              args.turn, len(moves), arena_extremes)
        if failures:
            raise SystemExit(f"{len(failures)} animation captures missing")
        return

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
        command(port, "ANIMSPEED 3", b"[ANIM_DIAG] SPEED", 5)
        for ordinal, move in enumerate(moves, 1):
            symbol = str(move_rows[str(move)]["symbol"])
            existing = sorted(args.output.glob(
                f"move-{move}-side-{args.side}-turn-{args.turn}-peak-p*.png"))
            if existing and not args.no_resume:
                match = re.search(r"-p(\d+)\.png$", existing[-1].name)
                presentation = int(match.group(1)) if match else 0
                entries.append((move, symbol, presentation, existing[-1]))
                print(f"[{ordinal}/{len(moves)}] resume {move} {symbol}", flush=True)
                continue
            if args.setup_every > 0 and ordinal > 1 and \
                    (ordinal - 1) % args.setup_every == 0:
                command(port, "ANIMSETUP", b"[ANIM_DIAG] READY", 45)
                command(port, "ANIMSPEED 3", b"[ANIM_DIAG] SPEED", 5)
            try:
                lines = command(
                    port, f"ANIMPEAK {move} {args.side} {args.turn}",
                    b"[ANIM_DIAG] DONE", args.timeout)
                capture = next((line for line in lines if line.startswith(
                    b"[ANIM_CAP] width=")), None)
                if capture is None:
                    raise RuntimeError("missing physical capture")
                match = CAPTURE_RE.match(capture)
                if not match or match.group(4) != b"1":
                    raise RuntimeError(f"invalid physical capture: {capture[:200]!r}")
                presentation = int(match.group(3))
                path = args.output / (
                    f"move-{move}-side-{args.side}-turn-{args.turn}"
                    f"-peak-p{presentation}.png")
                save_capture(capture, path, True)
                entries.append((move, symbol, presentation, path))
                extreme = next((EXTREME_RE.match(line) for line in lines
                                if EXTREME_RE.match(line)), None)
                if extreme:
                    arena_extremes.append({
                        "move": move,
                        "symbol": symbol,
                        "bright": int(extreme.group(1)),
                        "dark": int(extreme.group(2)),
                        "inverted": int(extreme.group(3)),
                        "transitions": int(extreme.group(4)),
                        "blankWhite": int(extreme.group(5)),
                        "blankBlack": int(extreme.group(6)),
                        "maxBlend": int(extreme.group(7)),
                    })
                print(f"[{ordinal}/{len(moves)}] {move} {symbol} p{presentation}",
                      flush=True)
            except Exception as error:  # continue catalog, preserve evidence
                failure = f"{move} {symbol}: {error}"
                failures.append(failure)
                print(f"[{ordinal}/{len(moves)}] FAIL {failure}", flush=True)
    finally:
        port.dtr = False
        port.rts = False
        port.close()

    write_catalog_results(entries, failures, args.output, args.side,
                          args.turn, len(moves), arena_extremes)
    if failures:
        raise SystemExit(f"{len(failures)} animation captures failed")


if __name__ == "__main__":
    main()
