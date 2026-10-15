#!/usr/bin/env python3
"""Capture renderer telemetry before and during a Pokegochi crash.

Open Settings > Wireless Update on the device, leave it on the pairing
screen, then run for example:

    python scripts/ble_live_diagnostic.py \
        --address 20:50:0D:34:0F:A2 --code 123456 \
        --output dist/live-render.log

After authentication the device leaves the updater screen automatically while
this process stays connected. Keep this terminal open, reproduce the broken
move/weather effect, and stop the process with Ctrl+C only after the failure.
Every received event is flushed to disk immediately, so a board reset does
not erase the last event already delivered over BLE.
"""

from __future__ import annotations

import argparse
import asyncio
import datetime as dt
import struct
from pathlib import Path

from bleak import BleakClient

from ble_wireless_update import (
    AUTHENTICATE,
    CONTROL_UUID,
    QUERY_STATUS,
    STATUS_AUTHENTICATED,
    STATUS_ERROR,
    STATUS_UUID,
    VERSION,
    parse_status,
)


LIVE_LOG_UUID = "70564743-4849-5550-8004-504f4b45474f"
LIVE_LOG_START = 18
LIVE_LOG_STOP = 19
STATUS_LIVE_LOG_READY = 18
STATUS_LIVE_LOG_STOPPED = 19
LIVE_LOG_HEADER = struct.Struct("<2sBBIIH")


class LiveLogClient:
    def __init__(self, client: BleakClient, output: Path):
        self.client = client
        self.output = output
        self.statuses: asyncio.Queue = asyncio.Queue()
        self.packets: asyncio.Queue[bytes] = asyncio.Queue()
        self.started = False

    def on_status(self, _sender, raw: bytearray) -> None:
        status = parse_status(raw)
        if status is not None:
            self.statuses.put_nowait(status)

    def on_live_log(self, _sender, raw: bytearray) -> None:
        packet = bytes(raw)
        if len(packet) >= LIVE_LOG_HEADER.size and packet[:2] == b"LG":
            self.packets.put_nowait(packet)

    async def wait_status(self, expected: int, timeout: float = 20.0):
        deadline = asyncio.get_running_loop().time() + timeout
        while True:
            remaining = deadline - asyncio.get_running_loop().time()
            if remaining <= 0:
                raise TimeoutError(f"timeout waiting for BLE status {expected}")
            status = await asyncio.wait_for(self.statuses.get(), remaining)
            if status.code == STATUS_ERROR:
                raise RuntimeError(
                    f"Pokegochi rejected live diagnostics (error {status.error})"
                )
            if status.code == expected:
                return status

    async def start(self, pairing_code: int) -> None:
        self.output.parent.mkdir(parents=True, exist_ok=True)
        await self.client.start_notify(STATUS_UUID, self.on_status)
        await self.client.start_notify(LIVE_LOG_UUID, self.on_live_log)
        await self.client.write_gatt_char(
            CONTROL_UUID, bytes([QUERY_STATUS]), response=True
        )
        await self.client.write_gatt_char(
            CONTROL_UUID,
            struct.pack("<BBI", AUTHENTICATE, VERSION, pairing_code),
            response=True,
        )
        await self.wait_status(STATUS_AUTHENTICATED)
        await self.client.write_gatt_char(
            CONTROL_UUID, bytes([LIVE_LOG_START]), response=True
        )
        await self.wait_status(STATUS_LIVE_LOG_READY)
        self.started = True

    async def stop(self) -> None:
        if self.started and self.client.is_connected:
            try:
                await self.client.write_gatt_char(
                    CONTROL_UUID, bytes([LIVE_LOG_STOP]), response=True
                )
                await self.wait_status(STATUS_LIVE_LOG_STOPPED, timeout=3.0)
            except Exception:
                # A reset or a lost adapter is exactly what this tool is meant
                # to observe; never mask the log with a cleanup error.
                pass
        for uuid in (LIVE_LOG_UUID, STATUS_UUID):
            try:
                await self.client.stop_notify(uuid)
            except Exception:
                pass

    async def capture(self) -> int:
        print(f"Live capture active. Saving to {self.output}", flush=True)
        print("Play now; leave this process running until after the crash.", flush=True)
        with self.output.open("a", encoding="utf-8", buffering=1) as stream:
            stream.write(
                f"# capture_started={dt.datetime.now().isoformat()}\n"
            )
            while self.client.is_connected:
                try:
                    packet = await asyncio.wait_for(self.packets.get(), 1.0)
                except asyncio.TimeoutError:
                    continue
                magic, version, kind, sequence, uptime_ms, payload_bytes = \
                    LIVE_LOG_HEADER.unpack_from(packet)
                payload = packet[LIVE_LOG_HEADER.size:
                                 LIVE_LOG_HEADER.size + payload_bytes]
                if magic != b"LG" or version != VERSION or kind != 1:
                    continue
                text = payload.decode("utf-8", errors="replace").replace("\x00", "")
                line = f"seq={sequence} uptime={uptime_ms} {text}"
                print(line, flush=True)
                stream.write(line + "\n")
                stream.flush()
        stream_line = f"# disconnected={dt.datetime.now().isoformat()}"
        print("BLE disconnected; the last received event is saved.", flush=True)
        # The file is already closed here, so append the marker with a short
        # separate open and keep the capture recoverable after a hard reset.
        with self.output.open("a", encoding="utf-8", buffering=1) as stream:
            stream.write(stream_line + "\n")
        return 0


async def capture(address: str, code: int, output: Path) -> int:
    async with BleakClient(address, timeout=30.0) as client:
        if not client.is_connected:
            raise RuntimeError("could not connect to the Pokegochi")
        mtu = max(23, int(getattr(client, "mtu_size", 23) or 23))
        print(f"Connected. MTU={mtu}", flush=True)
        if mtu < 50:
            raise RuntimeError(
                "BLE MTU stayed too low for live telemetry; reconnect with the "
                "Windows adapter and try again so MTU negotiation can reach 517."
            )
        if mtu - 3 < LIVE_LOG_HEADER.size + 32:
            raise RuntimeError("BLE notification payload is too small for live telemetry")
        reader = LiveLogClient(client, output)
        try:
            await reader.start(code)
            return await reader.capture()
        finally:
            await reader.stop()


async def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--address", required=True, help="BLE address of the Pokegochi")
    parser.add_argument("--code", type=int, help="six-digit pairing code shown on device")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("dist") / "live-render.log",
        help="append-only capture file (default: dist/live-render.log)",
    )
    args = parser.parse_args()
    code = args.code
    if code is None:
        code = int(input("Enter the six-digit pairing code shown on the Pokegochi: ").strip())
    if not 100000 <= code <= 999999:
        raise SystemExit("Pairing code must be six digits")
    return await capture(args.address, code, args.output)


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
