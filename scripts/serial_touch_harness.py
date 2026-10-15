"""Duplex Pokegochi serial monitor used for hardware UI regression tests.

Lines typed on stdin are forwarded to the firmware (for example ``STATE`` or
``TAP 160 210``), while device output remains visible and is appended to the
chosen log.  DTR/RTS stay released so attaching the harness does not reset the
ESP32.
"""

from __future__ import annotations

import argparse
import datetime as dt
import threading

import serial


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM6")
    parser.add_argument("--log", default=".codex-debug/serial-touch-harness.log")
    parser.add_argument(
        "--keepalive-seconds",
        type=float,
        default=0.0,
        help="periodically inject a neutral TAP so hardware light-sleep stays off",
    )
    args = parser.parse_args()

    connection = serial.Serial(args.port, 115200, timeout=0.2, dsrdtr=False, rtscts=False)
    connection.dtr = False
    connection.rts = False
    output_lock = threading.Lock()
    write_lock = threading.Lock()
    stop_event = threading.Event()

    def send_command(command: str) -> None:
        with write_lock:
            connection.write((command.rstrip("\r\n") + "\n").encode("ascii"))
            connection.flush()

    def reader() -> None:
        with open(args.log, "a", encoding="utf-8") as log:
            while connection.is_open:
                try:
                    raw = connection.readline()
                except (serial.SerialException, OSError, TypeError, AttributeError):
                    # On Windows, Ctrl-C closes the OVERLAPPED handle before
                    # the reader thread leaves readline().  That is a normal
                    # harness shutdown, not a device/firmware failure.
                    break
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                stamped = f"{dt.datetime.now().isoformat()} {line}"
                with output_lock:
                    print(stamped, flush=True)
                    log.write(stamped + "\n")
                    log.flush()

    thread = threading.Thread(target=reader, daemon=True)
    thread.start()
    keepalive_thread = None
    if args.keepalive_seconds > 0:
        def keepalive() -> None:
            while not stop_event.wait(args.keepalive_seconds):
                try:
                    # This coordinate is outside every battle command, move
                    # cell and confirmation control, but still traverses the
                    # real touch path and refreshes the inactivity timer.
                    send_command("TAP 10 150")
                except (serial.SerialException, OSError):
                    break

        keepalive_thread = threading.Thread(target=keepalive, daemon=True)
        keepalive_thread.start()
    print(f"[HARNESS] connected {args.port}; use STATE or TAP x y", flush=True)
    try:
        while True:
            command = input()
            send_command(command)
    except (EOFError, KeyboardInterrupt):
        pass
    finally:
        stop_event.set()
        try:
            connection.close()
        except (serial.SerialException, OSError, KeyboardInterrupt):
            pass
        # A second Windows Ctrl-C can arrive while Python is waiting for an
        # OVERLAPPED read handle to unwind. Shutdown is already complete at
        # that point, so do not turn a clean diagnostic exit into a traceback.
        try:
            thread.join(timeout=1.0)
            if keepalive_thread is not None:
                keepalive_thread.join(timeout=1.0)
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
