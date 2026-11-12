"""Profile real Pokegochi battle animations through the cabled ESP32.

The firmware fixture is transient: it never saves the synthetic battle or the
requested speed. Opening the CH340 can reset the board, so this harness always
waits for a complete boot before sending the first command.
"""

from __future__ import annotations

import argparse
import datetime as dt
import pathlib
import re
import time

import serial


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM7")
    parser.add_argument("--moves", default="1,59,76,396,650")
    parser.add_argument("--speeds", default="1,2,3")
    parser.add_argument("--side", type=int, choices=(0, 1), default=0)
    parser.add_argument("--turn", type=int, choices=(0, 1), default=0)
    parser.add_argument(
        "--workspace", choices=("normal", "pvp"), default="normal",
        help="use the normal renderer arena or the exact borrowed PvP Box arena",
    )
    parser.add_argument(
        "--radio", choices=("off", "ble", "wifi"), default="off",
        help="keep the selected production radio runtime alive during playback",
    )
    parser.add_argument(
        "--setup-between", action="store_true",
        help="rebuild the battle fixture between moves to isolate stale-scene failures",
    )
    parser.add_argument(
        "--setup-every", type=int, default=0,
        help="rebuild the battle fixture every N moves (0 disables periodic setup)",
    )
    parser.add_argument("--timeout", type=float, default=90.0)
    parser.add_argument(
        "--require-task-field", action="store_true",
        help="fail each requested move unless the renderer measured task-owned field pixels",
    )
    parser.add_argument(
        "--log", default=".codex-debug/animation-optimization-20261030/profile.log"
    )
    args = parser.parse_args()

    moves = [int(value) for value in args.moves.split(",") if value.strip()]
    speeds = [int(value) for value in args.speeds.split(",") if value.strip()]
    log_path = pathlib.Path(args.log)
    log_path.parent.mkdir(parents=True, exist_ok=True)

    # Configure modem-control levels before opening the CH340. Constructing a
    # live Serial object first briefly asserts DTR/RTS on Windows and resets a
    # CYD before pyserial can clear them.
    connection = serial.Serial(
        port=None, baudrate=115200, timeout=0.15,
        dsrdtr=False, rtscts=False
    )
    connection.dtr = False
    connection.rts = False
    connection.port = args.port
    connection.open()

    with log_path.open("a", encoding="utf-8") as log:
        last_task_field_frames: int | None = None

        def record(line: str) -> None:
            nonlocal last_task_field_frames
            stamped = f"{dt.datetime.now().isoformat()} {line}"
            print(stamped.encode("ascii", errors="backslashreplace").decode("ascii"),
                  flush=True)
            log.write(stamped + "\n")
            log.flush()
            geometry = re.search(r"\[ANIM\] geometry .*?\bfield=(\d+)", line)
            if geometry:
                last_task_field_frames = int(geometry.group(1))
            elif line.startswith("[R]"):
                values = line[3:].split(",")
                if len(values) >= 12 and values[-1].isdigit():
                    last_task_field_frames = int(values[-1])

        def wait_for(markers: tuple[str, ...], timeout: float) -> str:
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                raw = connection.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                record(line)
                if any(marker in line for marker in markers):
                    return line
            raise TimeoutError(f"timed out waiting for {markers}")

        def send(command: str) -> None:
            record(f"[HOST] {command}")
            connection.write((command + "\n").encode("ascii"))
            connection.flush()

        send("ANIMSETUP")
        setup_deadline = time.monotonic() + 45.0
        setup_retry_at = time.monotonic() + 4.0
        while True:
            raw = connection.readline()
            if raw:
                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                record(line)
                if "[ANIM_DIAG] READY" in line:
                    break
                if "[ANIM_DIAG] SETUP REJECTED" in line:
                    raise RuntimeError("animation fixture setup was rejected")
                if "[BOOT] Ready" in line:
                    send("ANIMSETUP")
                    setup_retry_at = time.monotonic() + 4.0
            now = time.monotonic()
            if now >= setup_deadline:
                raise TimeoutError("timed out setting up the animation fixture")
            if now >= setup_retry_at:
                send("ANIMSETUP")
                setup_retry_at = now + 4.0
        if args.radio != "off":
            radio = args.radio.upper()
            send(f"ANIMRADIO {radio}")
            wait_for((f"[ANIM_DIAG] RADIO {radio} started=1 workspace=1",), 45.0)
            # Wi-Fi parks and reloads GameSave while its driver allocates, so
            # build the transient battle only after the real radio lifetime
            # is established. Repeating setup is harmless for BLE as well.
            send("ANIMSETUP")
            wait_for(("[ANIM_DIAG] READY",), 15.0)
        if args.workspace == "pvp":
            send("ANIMPVP")
            wait_for(("[ANIM_DIAG] PVP READY",), 15.0)
        try:
            for speed in speeds:
                send(f"ANIMSPEED {speed}")
                wait_for((f"[ANIM_DIAG] SPEED {speed}x",), 5.0)
                for move_index, move in enumerate(moves):
                    periodic_setup = args.setup_every > 0 and move_index and \
                        move_index % args.setup_every == 0
                    if move_index and (args.setup_between or periodic_setup):
                        send("ANIMSETUP")
                        wait_for(("[ANIM_DIAG] READY",), 15.0)
                    last_task_field_frames = None
                    send(f"ANIMPLAY {move} {args.side} {args.turn}")
                    result = wait_for(
                        ("[ANIM_DIAG] DONE", "[ANIM_DIAG] PLAY REJECTED"),
                        args.timeout)
                    if "PLAY REJECTED" in result:
                        raise RuntimeError(f"move {move} playback was rejected")
                    if (args.require_task_field and
                            (last_task_field_frames is None or
                             last_task_field_frames <= 0)):
                        raise RuntimeError(
                            f"move {move} produced no measured task-owned field frames")
        finally:
            if args.workspace == "pvp":
                send("ANIMPVP OFF")
                wait_for(("[ANIM_DIAG] PVP RELEASED",), 30.0)
        record("[HOST] animation profile complete")

    connection.close()


if __name__ == "__main__":
    main()
