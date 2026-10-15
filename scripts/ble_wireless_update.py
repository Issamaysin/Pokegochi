#!/usr/bin/env python3
"""Send a signed .pgota package directly from a Windows BLE adapter.

This is the desktop counterpart of the Android updater.  It deliberately uses
the same authenticated, acknowledged protocol; the ESP32 still verifies the
manifest signature, every object hash, and the OTA slot before committing.
"""

from __future__ import annotations

import argparse
import asyncio
import hashlib
import struct
import time
import zipfile
from dataclasses import dataclass
from pathlib import Path

from bleak import BleakClient, BleakScanner


SERVICE_UUID = "70564743-4849-5550-8000-504f4b45474f"
CONTROL_UUID = "70564743-4849-5550-8001-504f4b45474f"
DATA_UUID = "70564743-4849-5550-8002-504f4b45474f"
STATUS_UUID = "70564743-4849-5550-8003-504f4b45474f"

VERSION = 1
MODEL = 1
MANIFEST_SIZE = 276
LEGACY_DATA_BYTES = 184
MAXIMUM_DATA_BYTES = 508

AUTHENTICATE = 1
MANIFEST_BEGIN = 2
MANIFEST_CHUNK = 3
MANIFEST_FINISH = 4
OBJECT_BEGIN = 5
OBJECT_FINISH = 6
COMMIT = 7
QUERY_STATUS = 9
SYNCHRONIZE_TIME = 10

STATUS_AUTHENTICATED = 2
STATUS_MANIFEST_ACCEPTED = 3
STATUS_OBJECT_READY = 4
STATUS_DATA_ACCEPTED = 5
STATUS_OBJECT_VERIFIED = 6
STATUS_COMMIT_ACCEPTED = 7
STATUS_ERROR = 10
STATUS_TIME_SYNCHRONIZED = 11

OBJECT_FIRMWARE = 0
OBJECT_ASSET = 1

ASSET_ENTRIES = {
    1: "payload/assets/pokegochi.pak",
    2: "payload/assets/pack_version.txt",
    3: "payload/assets/animation_catalog.txt",
    4: "payload/assets/manifest.json",
}


@dataclass(frozen=True)
class Payload:
    kind: int
    asset_id: int
    entry: str
    size: int
    digest: bytes


@dataclass(frozen=True)
class Package:
    path: Path
    manifest: bytes
    payloads: tuple[Payload, ...]
    total_bytes: int


@dataclass(frozen=True)
class Status:
    code: int
    state: int
    error: int
    next_offset: int
    object_size: int
    package_done: int
    package_total: int


def parse_status(raw: bytearray | bytes) -> Status | None:
    if len(raw) != 24 or raw[:2] != b"UP" or raw[2] != VERSION:
        return None
    unpacked = struct.unpack("<2sBBBBBBIIII", raw)
    (_magic, _version, code, state, error, _kind, _asset,
     next_offset, object_size, package_done, package_total) = unpacked
    return Status(code, state, error, next_offset, object_size,
                  package_done, package_total)


def advertisement_payload(advertisement) -> bytes | None:
    for company_id, value in advertisement.manufacturer_data.items():
        # Windows/Bleak removes the two-byte Bluetooth company identifier
        # from each value.  NimBLE's manufacturer payload has no custom
        # company bytes, so reconstruct the original record before matching
        # the PGOT signature (0x4750 is "PG" in little-endian form).
        raw = struct.pack("<H", company_id) + bytes(value)
        candidates = (raw, bytes(value))
        marker = -1
        source = raw
        for candidate in candidates:
            marker = candidate.find(b"PGOT")
            if marker >= 0:
                source = candidate
                break
        if marker >= 0 and len(source) - marker >= 13:
            return source[marker:]
    return None


def parse_advertisement(advertisement):
    payload = advertisement_payload(advertisement)
    if payload is None or payload[4] != VERSION or payload[5] != MODEL:
        return None
    player_id = struct.unpack_from("<I", payload, 6)[0]
    maximum = LEGACY_DATA_BYTES
    if len(payload) >= 12 and LEGACY_DATA_BYTES <= struct.unpack_from("<H", payload, 10)[0] <= MAXIMUM_DATA_BYTES:
        maximum = struct.unpack_from("<H", payload, 10)[0]
    capabilities = payload[12] if len(payload) >= 13 else 0
    return player_id, maximum, capabilities


def load_package(path: Path) -> Package:
    with zipfile.ZipFile(path, "r") as bundle:
        manifest = bundle.read("manifest.bin")
        if len(manifest) != MANIFEST_SIZE:
            raise ValueError("manifest.bin has the wrong size")
        unpacked = struct.unpack("<4sBBHIIHBB32s", manifest[:52])
        (magic, version, model, structure_size, _sequence, firmware_size,
         _pack_version, asset_count, _flags, firmware_hash) = unpacked
        if (magic, version, model, structure_size, asset_count) != (
                b"PGU1", VERSION, MODEL, MANIFEST_SIZE, 4):
            raise ValueError("incompatible Pokegochi update package")

        payloads: list[Payload] = []
        for index in range(4):
            offset = 52 + index * 40
            asset_id, size, digest = struct.unpack(
                "<B3xI32s", manifest[offset:offset + 40])
            entry = ASSET_ENTRIES.get(asset_id)
            if entry is None or not size:
                raise ValueError("invalid asset table in manifest")
            payloads.append(Payload(OBJECT_ASSET, asset_id, entry, size, digest))
        payloads.append(Payload(OBJECT_FIRMWARE, 0, "payload/firmware.bin",
                                firmware_size, firmware_hash))

        for payload in payloads:
            info = bundle.getinfo(payload.entry)
            if info.file_size != payload.size:
                raise ValueError(f"size mismatch: {payload.entry}")
            digest = hashlib.sha256()
            with bundle.open(info) as source:
                for chunk in iter(lambda: source.read(1024 * 1024), b""):
                    digest.update(chunk)
            if digest.digest() != payload.digest:
                raise ValueError(f"SHA-256 mismatch: {payload.entry}")
    return Package(path, manifest, tuple(payloads), sum(p.size for p in payloads))


async def scan(seconds: float) -> list[tuple[object, object, tuple[int, int, int]]]:
    devices = await BleakScanner.discover(timeout=seconds, return_adv=True)
    found = []
    for device, advertisement in devices.values():
        parsed = parse_advertisement(advertisement)
        if parsed is None:
            continue
        found.append((device, advertisement, parsed))
        player_id, maximum, capabilities = parsed
        print(f"FOUND {device.address}  {device.name or '(unnamed)'}  "
              f"player={player_id:05d} block={maximum} capabilities=0x{capabilities:02X}")
    if not found:
        print("No Pokegochi update advertisement found.")
        print("Open Settings > Wireless Update > Open Updater on the console.")
    return found


class DirectUpdater:
    def __init__(self, client: BleakClient, package: Package,
                 maximum_data_bytes: int):
        self.client = client
        self.package = package
        self.maximum_data_bytes = maximum_data_bytes
        self.statuses: asyncio.Queue[Status] = asyncio.Queue()
        self.last_progress = 0.0

    def on_status(self, _sender, raw: bytearray) -> None:
        status = parse_status(raw)
        if status is not None:
            self.statuses.put_nowait(status)

    async def write_control(self, packet: bytes, expected: int | None = None) -> Status | None:
        await self.client.write_gatt_char(CONTROL_UUID, packet, response=True)
        if expected is None:
            return None
        return await self.wait_for(expected)

    async def write_data(self, packet: bytes) -> Status:
        await self.client.write_gatt_char(DATA_UUID, packet, response=True)
        return await self.wait_for(STATUS_DATA_ACCEPTED)

    async def wait_for(self, expected: int, timeout: float = 120.0) -> Status:
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"timeout waiting for status {expected}")
            status = await asyncio.wait_for(self.statuses.get(), remaining)
            if status.code == STATUS_ERROR:
                raise RuntimeError(f"Pokegochi rejected update (error {status.error})")
            if status.code == expected:
                return status

    async def send_manifest(self) -> None:
        await self.write_control(struct.pack("<BH", MANIFEST_BEGIN, MANIFEST_SIZE),
                                 STATUS_DATA_ACCEPTED)
        maximum = max(1, min(189, self.maximum_data_bytes - 3))
        for offset in range(0, len(self.package.manifest), maximum):
            chunk = self.package.manifest[offset:offset + maximum]
            packet = struct.pack("<BH", MANIFEST_CHUNK, offset) + chunk
            await self.write_control(packet, STATUS_DATA_ACCEPTED)
        await self.write_control(bytes([MANIFEST_FINISH]), STATUS_MANIFEST_ACCEPTED)

    async def send_payload(self, payload: Payload, bundle: zipfile.ZipFile,
                           payload_index: int) -> None:
        print(f"OBJECT {payload_index}/{len(self.package.payloads)}: "
              f"{payload.entry} ({payload.size:,} bytes)")
        reply = await self.write_control(
            bytes([OBJECT_BEGIN, payload.kind, payload.asset_id]), STATUS_OBJECT_READY)
        assert reply is not None
        if reply.object_size != payload.size or reply.next_offset > payload.size:
            raise RuntimeError("device returned an invalid resume offset")
        offset = reply.next_offset
        info = bundle.getinfo(payload.entry)
        with bundle.open(info) as source:
            source.seek(offset)
            maximum = max(1, min(self.maximum_data_bytes, 508))
            while offset < payload.size:
                chunk = source.read(min(maximum, payload.size - offset))
                if not chunk:
                    raise RuntimeError(f"package ended unexpectedly: {payload.entry}")
                status = await self.write_data(struct.pack("<I", offset) + chunk)
                expected_offset = offset + len(chunk)
                if status.next_offset != expected_offset:
                    raise RuntimeError("device and host offsets diverged")
                offset = status.next_offset
                now = time.monotonic()
                if now - self.last_progress >= 0.5 or offset == payload.size:
                    self.last_progress = now
                    total_done = status.package_done
                    total = status.package_total or self.package.total_bytes
                    print(f"  {total_done:,}/{total:,} bytes ({100 * total_done / total:.1f}%)")
        await self.write_control(bytes([OBJECT_FINISH]), STATUS_OBJECT_VERIFIED)

    async def run(self, pairing_code: int) -> None:
        await self.client.start_notify(STATUS_UUID, self.on_status)
        await self.write_control(bytes([QUERY_STATUS]))
        await self.write_control(struct.pack("<BBI", AUTHENTICATE, VERSION, pairing_code),
                                 STATUS_AUTHENTICATED)
        print("Authenticated.")
        await self.send_manifest()
        print("Signed manifest accepted.")
        with zipfile.ZipFile(self.package.path, "r") as bundle:
            for index, payload in enumerate(self.package.payloads, 1):
                await self.send_payload(payload, bundle, index)
        await self.write_control(bytes([COMMIT]), STATUS_COMMIT_ACCEPTED)
        print("OTA candidate accepted. The Pokegochi is rebooting and will self-test it.")
        await self.client.stop_notify(STATUS_UUID)


async def update(address: str, package_path: Path, pairing_code: int,
                 advertised_maximum: int | None) -> None:
    package = load_package(package_path)
    print(f"Package verified locally: {package.path.name}, "
          f"{package.total_bytes:,} payload bytes")
    async with BleakClient(address, timeout=30.0) as client:
        if not client.is_connected:
            raise RuntimeError("could not connect to the Pokegochi")
        negotiated = getattr(client, "mtu_size", 23)
        mtu_bytes = max(23, int(negotiated or 23))
        mtu_maximum = max(1, mtu_bytes - 7)
        maximum = min(advertised_maximum or MAXIMUM_DATA_BYTES, mtu_maximum)
        if mtu_bytes <= 23:
            print("Warning: Windows kept the BLE MTU at 23; transfer will be slower.")
        print(f"Connected. MTU={mtu_bytes}, acknowledged data payload={maximum} bytes")
        updater = DirectUpdater(client, package, maximum)
        await updater.run(pairing_code)


async def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path,
                        default=Path("dist/pokegochi-wireless-update-latest.pgota"))
    parser.add_argument("--address", help="BLE address printed by --scan")
    parser.add_argument("--code", type=int, help="six-digit code shown on the Pokegochi")
    parser.add_argument("--scan", action="store_true", help="scan and exit")
    parser.add_argument("--scan-seconds", type=float, default=8.0)
    args = parser.parse_args()
    # Windows BLE discovery can remain blocked while a known device is
    # already in updater mode. A supplied address is sufficient: update()
    # negotiates the MTU and uses the advertised maximum as a safe fallback.
    if args.address and not args.scan:
        code = args.code
        if code is None:
            code = int(input("Enter the six-digit pairing code shown on the Pokegochi: ").strip())
        if not 100000 <= code <= 999999:
            raise SystemExit("Pairing code must be six digits")
        await update(args.address, args.package, code, MAXIMUM_DATA_BYTES)
        return 0
    found = await scan(args.scan_seconds)
    if args.scan:
        return 0 if args.scan or not found else 1
    if not found and args.address:
        print(f"No advertisement received; trying cached BLE address {args.address}.")
        code = args.code
        if code is None:
            code = int(input("Enter the six-digit pairing code shown on the Pokegochi: ").strip())
        await update(args.address, args.package, code, MAXIMUM_DATA_BYTES)
        return 0
    if not found:
        return 0
    device = next((item for item in found
                   if not args.address or item[0].address.lower() == args.address.lower()), None)
    if device is None:
        raise SystemExit("Requested BLE address was not found")
    address = device[0].address
    advertised_maximum = device[2][1]
    code = args.code
    if code is None:
        code = int(input("Enter the six-digit pairing code shown on the Pokegochi: ").strip())
    if not 100000 <= code <= 999999:
        raise SystemExit("Pairing code must be six digits")
    await update(address, args.package, code, advertised_maximum)
    return 0


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
