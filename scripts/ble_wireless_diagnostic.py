#!/usr/bin/env python3
"""Read a Pokegochi diagnostic report over the authenticated BLE updater link.

Open Settings > Wireless Update > Open Updater on the device, then run for
example:

    python scripts/ble_wireless_diagnostic.py --code 123456
    python scripts/ble_wireless_diagnostic.py --code 123456 --section animation --move 370

The command is read-only. It never starts an OTA transfer, changes the save,
or writes anything to the SD card.
"""

from __future__ import annotations

import argparse
import asyncio
import struct
import time

from bleak import BleakClient

from ble_wireless_update import (
    AUTHENTICATE,
    CONTROL_UUID,
    DATA_UUID,
    MAXIMUM_DATA_BYTES,
    QUERY_STATUS,
    STATUS_AUTHENTICATED,
    STATUS_ERROR,
    STATUS_UUID,
    VERSION,
    scan,
    parse_status,
)


DIAGNOSTIC_READ = 17
STATUS_DIAGNOSTIC_READY = 17
SECTION_SUMMARY = 1
SECTION_ANIMATION = 2
DIAGNOSTIC_HEADER = struct.Struct("<2sBBHHH")


class DiagnosticClient:
    def __init__(self, client: BleakClient):
        self.client = client
        self.statuses: asyncio.Queue = asyncio.Queue()
        self.packets: asyncio.Queue[bytes] = asyncio.Queue()

    def on_status(self, _sender, raw: bytearray) -> None:
        status = parse_status(raw)
        if status is not None:
            self.statuses.put_nowait(status)

    def on_data(self, _sender, raw: bytearray) -> None:
        packet = bytes(raw)
        if len(packet) >= DIAGNOSTIC_HEADER.size and packet[:2] == b"DG":
            self.packets.put_nowait(packet)

    async def wait_status(self, expected: int, timeout: float = 20.0):
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"timeout waiting for BLE status {expected}")
            status = await asyncio.wait_for(self.statuses.get(), remaining)
            if status.code == STATUS_ERROR:
                raise RuntimeError(
                    f"Pokegochi rejected diagnostic request (error {status.error})"
                )
            if status.code == expected:
                return status

    async def read(self, pairing_code: int, section: int, selector: int) -> str:
        await self.client.start_notify(DATA_UUID, self.on_data)
        await self.client.start_notify(STATUS_UUID, self.on_status)
        await self.client.write_gatt_char(CONTROL_UUID, bytes([QUERY_STATUS]), response=True)
        await self.client.write_gatt_char(
            CONTROL_UUID, struct.pack("<BBI", AUTHENTICATE, VERSION, pairing_code),
            response=True,
        )
        await self.wait_status(STATUS_AUTHENTICATED)
        request = struct.pack("<BBH", DIAGNOSTIC_READ, section, selector)
        await self.client.write_gatt_char(CONTROL_UUID, request, response=True)
        # The device emits DiagnosticReady after the notify stream. Waiting
        # for it first makes an old/rolled-back firmware report its protocol
        # error instead of leaving the host with a misleading data timeout.
        await self.wait_status(STATUS_DIAGNOSTIC_READY)

        chunks: dict[int, bytes] = {}
        total: int | None = None
        deadline = time.monotonic() + 30.0
        while total is None or sum(len(chunk) for chunk in chunks.values()) < total:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("timeout waiting for diagnostic data")
            packet = await asyncio.wait_for(self.packets.get(), remaining)
            magic, version, packet_section, packet_selector, offset, packet_total = \
                DIAGNOSTIC_HEADER.unpack_from(packet)
            if magic != b"DG" or version != VERSION:
                continue
            if packet_section != section or packet_selector != selector:
                continue
            payload = packet[DIAGNOSTIC_HEADER.size:]
            total = packet_total
            chunks[offset] = payload

        report = bytearray()
        offset = 0
        while offset < (total or 0):
            payload = chunks.get(offset)
            if payload is None:
                raise RuntimeError(f"diagnostic response has a gap at offset {offset}")
            report.extend(payload)
            offset += len(payload)
        await self.client.stop_notify(DATA_UUID)
        await self.client.stop_notify(STATUS_UUID)
        return report[:total].decode("utf-8", errors="replace")


async def diagnose(address: str, code: int, section: int, selector: int,
                   advertised_maximum: int | None) -> None:
    async with BleakClient(address, timeout=30.0) as client:
        if not client.is_connected:
            raise RuntimeError("could not connect to the Pokegochi")
        mtu = max(23, int(getattr(client, "mtu_size", 23) or 23))
        maximum = min(advertised_maximum or MAXIMUM_DATA_BYTES, max(1, mtu - 7))
        print(f"Connected. MTU={mtu}, advertised block={maximum} bytes")
        if mtu <= 23:
            print("Warning: BLE MTU stayed at 23; the diagnostic will still use small packets.")
        reader = DiagnosticClient(client)
        report = await reader.read(code, section, selector)
        print("--- POKEGOCHI DIAGNOSTIC ---")
        print(report, end="" if report.endswith("\n") else "\n")


async def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--address", help="BLE address printed by --scan")
    parser.add_argument("--code", type=int, help="six-digit code shown on the Pokegochi")
    parser.add_argument("--scan", action="store_true", help="scan and exit")
    parser.add_argument("--scan-seconds", type=float, default=8.0)
    parser.add_argument("--section", choices=("summary", "animation"), default="summary")
    parser.add_argument("--move", type=int, default=0,
                        help="move id when --section animation is selected")
    args = parser.parse_args()
    if args.address and not args.scan:
        code = args.code
        if code is None:
            code = int(input("Enter the six-digit pairing code shown on the Pokegochi: ").strip())
        if not 100000 <= code <= 999999:
            raise SystemExit("Pairing code must be six digits")
        section = SECTION_ANIMATION if args.section == "animation" else SECTION_SUMMARY
        if section == SECTION_ANIMATION and not 1 <= args.move <= 65535:
            raise SystemExit("--move must be a valid move id (1..65535)")
        await diagnose(args.address, code, section, args.move, MAXIMUM_DATA_BYTES)
        return 0
    found = await scan(args.scan_seconds)
    if args.scan:
        return 0
    if not found and not args.address:
        return 0
    if args.address:
        selected = next((item for item in found
                         if item[0].address.lower() == args.address.lower()), None)
        if selected is None:
            address = args.address
            advertised_maximum = MAXIMUM_DATA_BYTES
        else:
            address = selected[0].address
            advertised_maximum = selected[2][1]
    else:
        address = found[0][0].address
        advertised_maximum = found[0][2][1]
    code = args.code
    if code is None:
        code = int(input("Enter the six-digit pairing code shown on the Pokegochi: ").strip())
    if not 100000 <= code <= 999999:
        raise SystemExit("Pairing code must be six digits")
    section = SECTION_ANIMATION if args.section == "animation" else SECTION_SUMMARY
    if section == SECTION_ANIMATION and not 1 <= args.move <= 65535:
        raise SystemExit("--move must be a valid move id (1..65535)")
    await diagnose(address, code, section, args.move, advertised_maximum)
    return 0


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
