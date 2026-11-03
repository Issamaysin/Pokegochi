"""Profile real Pokegochi battle animations through the cabled ESP32.

The firmware fixture is transient: it never saves the synthetic battle or the
requested speed. Opening the CH340 can reset the board, so this harness always
waits for a complete boot before sending the first command.
"""

from __future__ import annotations

import argparse
import datetime as dt
import pathlib
import time

import serial


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM7")
    parser.add_argument("--moves", default="1,59,76,396,650")
    parser.add_argument("--speeds", default="1,2,3")
    parser.add_argument("--side", type=int, choices=(0, 1), default=0)
    parser.add_argument("--timeout", type=float, default=90.0)
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
        def record(line: str) -> None:
            stamped = f"{dt.datetime.now().isoformat()} {line}"
            print(stamped.encode("ascii", errors="backslashreplace").decode("ascii"),
                  flush=True)
            log.write(stamped + "\n")
            log.flush()

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
        for speed in speeds:
            send(f"ANIMSPEED {speed}")
            wait_for((f"[ANIM_DIAG] SPEED {speed}x",), 5.0)
            for move in moves:
                send(f"ANIMPLAY {move} {args.side}")
                wait_for(("[ANIM_DIAG] DONE", "[ANIM_DIAG] PLAY REJECTED"),
                         args.timeout)
        record("[HOST] animation profile complete")

    connection.close()


if __name__ == "__main__":
    main()
