#!/usr/bin/env python3
"""Build a signed Pokegochi firmware + microSD wireless-update bundle.

The resulting .pgota is a regular ZIP understood by the Android updater.  Its
binary manifest is verified again by the ESP32, so modifying either the app
binary or any SD payload after packaging is rejected by the device.
"""

from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import os
import re
import struct
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_FIRMWARE = ROOT / ".pio/build/esp32-2432S028R/firmware.bin"
SD_ROOT = ROOT / ".generated/sdcard"
PRIVATE_KEY = ROOT / ".keys/pokegochi-update-private.key"
LAST_SEQUENCE = ROOT / ".keys/pokegochi-update-sequence.txt"
PUBLIC_HEADER = ROOT / "include/config/UpdateSigningKey.h"
OUTPUT = ROOT / "dist/pokegochi-wireless-update.pgota"
OTA_SLOT_SIZE = 0x1A0000

PROTOCOL_VERSION = 1
DEVICE_MODEL = 1
ASSET_IDS = {
    1: ("archive", SD_ROOT / "pokegochi/pokegochi.pak", "payload/assets/pokegochi.pak"),
    2: ("pack-version", SD_ROOT / "pokegochi/assets/pack_version.txt", "payload/assets/pack_version.txt"),
    3: ("animation-catalog", SD_ROOT / "pokegochi/assets/animation_catalog.txt", "payload/assets/animation_catalog.txt"),
    4: ("asset-manifest", SD_ROOT / "pokegochi/assets/manifest.json", "payload/assets/manifest.json"),
}

# secp256r1 / NIST P-256 parameters.
P = 0xFFFFFFFF00000001000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFF
A = P - 3
B = 0x5AC635D8AA3A93E7B3EBBD55769886BC651D06B0CC53B0F63BCE3C3E27D2604B
GX = 0x6B17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C296
GY = 0x4FE342E2FE1A7F9B8EE7EB4A7C0F9E162BCE33576B315ECECBB6406837BF51F5
N = 0xFFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551
G = (GX, GY)


def inv(value: int, modulus: int) -> int:
    return pow(value, -1, modulus)


def point_add(left: tuple[int, int] | None, right: tuple[int, int] | None):
    if left is None:
        return right
    if right is None:
        return left
    x1, y1 = left
    x2, y2 = right
    if x1 == x2 and (y1 + y2) % P == 0:
        return None
    if left == right:
        slope = ((3 * x1 * x1 + A) * inv(2 * y1, P)) % P
    else:
        slope = ((y2 - y1) * inv((x2 - x1) % P, P)) % P
    x3 = (slope * slope - x1 - x2) % P
    return x3, (slope * (x1 - x3) - y1) % P


def scalar_multiply(scalar: int, point=G):
    result = None
    addend = point
    while scalar:
        if scalar & 1:
            result = point_add(result, addend)
        addend = point_add(addend, addend)
        scalar >>= 1
    return result


def deterministic_nonce(private: int, digest: bytes) -> int:
    key = private.to_bytes(32, "big")
    value = b"\x01" * 32
    state = b"\x00" * 32
    state = hmac.new(state, value + b"\x00" + key + digest, hashlib.sha256).digest()
    value = hmac.new(state, value, hashlib.sha256).digest()
    state = hmac.new(state, value + b"\x01" + key + digest, hashlib.sha256).digest()
    value = hmac.new(state, value, hashlib.sha256).digest()
    while True:
        value = hmac.new(state, value, hashlib.sha256).digest()
        candidate = int.from_bytes(value, "big")
        if 1 <= candidate < N:
            return candidate
        state = hmac.new(state, value + b"\x00", hashlib.sha256).digest()
        value = hmac.new(state, value, hashlib.sha256).digest()


def sign(private: int, message: bytes) -> bytes:
    digest = hashlib.sha256(message).digest()
    nonce = deterministic_nonce(private, digest)
    point = scalar_multiply(nonce)
    assert point is not None
    r = point[0] % N
    s = (inv(nonce, N) * (int.from_bytes(digest, "big") + r * private)) % N
    if s > N // 2:
        s = N - s
    return r.to_bytes(32, "big") + s.to_bytes(32, "big")


def verify(public: tuple[int, int], message: bytes, signature: bytes) -> bool:
    r = int.from_bytes(signature[:32], "big")
    s = int.from_bytes(signature[32:], "big")
    if not (1 <= r < N and 1 <= s < N):
        return False
    digest = int.from_bytes(hashlib.sha256(message).digest(), "big")
    w = inv(s, N)
    point = point_add(scalar_multiply((digest * w) % N), scalar_multiply((r * w) % N, public))
    return point is not None and point[0] % N == r


def load_or_create_key() -> tuple[int, tuple[int, int]]:
    PRIVATE_KEY.parent.mkdir(parents=True, exist_ok=True)
    if PRIVATE_KEY.exists():
        private = int(PRIVATE_KEY.read_text(encoding="ascii").strip(), 16)
    else:
        while True:
            private = int.from_bytes(os.urandom(32), "big")
            if 1 <= private < N:
                break
        PRIVATE_KEY.write_text(f"{private:064x}\n", encoding="ascii")
        print(f"Created signing key: {PRIVATE_KEY}")
        print("Back this file up securely; losing it prevents future signed updates.")
    if not 1 <= private < N:
        raise RuntimeError("Invalid P-256 private key")
    public = scalar_multiply(private)
    assert public is not None
    write_public_header(public)
    return private, public


def write_public_header(public: tuple[int, int]) -> None:
    raw = public_key_bytes(public)
    values = ", ".join(f"0x{value:02X}" for value in raw)
    expected = (
        "#pragma once\n\n#include <cstdint>\n\n"
        "// Generated by scripts/build_wireless_update.py. The matching private key\n"
        "// stays in .keys/ and is never copied to the device or Android app.\n"
        "constexpr uint8_t kPokegochiUpdatePublicKey[65] = {\n  "
        + values
        + "\n};\n"
    )
    if not PUBLIC_HEADER.exists() or PUBLIC_HEADER.read_text(encoding="ascii") != expected:
        PUBLIC_HEADER.write_text(expected, encoding="ascii")


def public_key_bytes(public: tuple[int, int]) -> bytes:
    return b"\x04" + public[0].to_bytes(32, "big") + public[1].to_bytes(32, "big")


def sha256_file(path: Path) -> bytes:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.digest()


def sha256_zip_entry(bundle: zipfile.ZipFile, name: str) -> tuple[int, bytes]:
    digest = hashlib.sha256()
    total = 0
    with bundle.open(name) as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            total += len(chunk)
            digest.update(chunk)
    return total, digest.digest()


def asset_pack_version() -> int:
    marker = SD_ROOT / "pokegochi/assets/pack_version.txt"
    return int(marker.read_text(encoding="ascii").strip())


def game_version() -> str:
    source = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    match = re.search(r'constexpr char kGameVersion\[\]\s*=\s*"([^"]+)"', source)
    return match.group(1) if match else "unknown"


def build_manifest(firmware: Path, sequence: int, private: int) -> tuple[bytes, list[dict]]:
    assets: list[dict] = []
    records = bytearray()
    for asset_id, (name, path, archive_name) in ASSET_IDS.items():
        if not path.is_file():
            raise FileNotFoundError(f"Required SD payload is missing: {path}")
        digest = sha256_file(path)
        records += struct.pack("<B3xI32s", asset_id, path.stat().st_size, digest)
        assets.append({
            "id": asset_id,
            "name": name,
            "file": archive_name,
            "size": path.stat().st_size,
            "sha256": digest.hex(),
        })
    firmware_digest = sha256_file(firmware)
    # Matches PokegochiUpdateProtocol::Manifest up to the signature member.
    unsigned = struct.pack(
        "<4sBBHIIHBB32s",
        b"PGU1",
        PROTOCOL_VERSION,
        DEVICE_MODEL,
        276,
        sequence,
        firmware.stat().st_size,
        asset_pack_version(),
        len(assets),
        0,
        firmware_digest,
    ) + records
    if len(unsigned) != 212:
        raise AssertionError(f"Unexpected unsigned manifest size: {len(unsigned)}")
    signature = sign(private, unsigned)
    return unsigned + signature, assets


def verify_bundle(path: Path, public: tuple[int, int]) -> None:
    """Independently reopen and validate the exact artifact sent to Android."""
    expected_entries = {
        "manifest.bin",
        "package.json",
        "payload/firmware.bin",
        *(archive_name for _, _, archive_name in ASSET_IDS.values()),
    }
    with zipfile.ZipFile(path, "r") as bundle:
        names = [item.filename for item in bundle.infolist()]
        if len(names) != len(set(names)) or set(names) != expected_entries:
            raise RuntimeError("Update ZIP has missing, duplicate, or unexpected entries")
        manifest = bundle.read("manifest.bin")
        if len(manifest) != 276 or not verify(public, manifest[:212], manifest[212:]):
            raise RuntimeError("Packaged manifest signature self-test failed")
        (magic, protocol, model, structure_size, sequence, firmware_size,
         pack_version, asset_count, flags, firmware_hash) = struct.unpack(
            "<4sBBHIIHBB32s", manifest[:52]
        )
        if (magic, protocol, model, structure_size, asset_count, flags) != (
                b"PGU1", PROTOCOL_VERSION, DEVICE_MODEL, 276, 4, 0):
            raise RuntimeError("Packaged manifest header is invalid")
        firmware_bytes, firmware_actual = sha256_zip_entry(bundle, "payload/firmware.bin")
        if firmware_bytes != firmware_size or firmware_actual != firmware_hash:
            raise RuntimeError("Packaged firmware does not match its manifest")
        # The archive has its own authoritative pack version (PGA revision 3).
        # Verify it agrees with the signed OTAP manifest before an artifact can
        # be published. This catches a stale .pak paired with a freshly written
        # loose pack_version.txt on the build machine.
        with bundle.open("payload/assets/pokegochi.pak") as archive:
            archive_header = archive.read(24)
        if len(archive_header) != 24:
            raise RuntimeError("Packaged asset archive header is truncated")
        (archive_magic, archive_revision, archive_entry_size,
         archive_pack_version, archive_asset_count,
         archive_index_offset, archive_data_offset) = struct.unpack(
             "<4sHHH2xIII", archive_header)
        if (archive_magic != b"PGA1" or archive_revision != 3 or
                archive_entry_size != 12 or archive_pack_version != pack_version or
                archive_asset_count == 0 or archive_index_offset < 24 or
                archive_data_offset <= archive_index_offset):
            raise RuntimeError("Packaged asset archive identity is invalid or stale")
        seen: set[int] = set()
        records_offset = 52
        for index in range(asset_count):
            offset = records_offset + index * 40
            asset_id, size, digest = struct.unpack("<B3xI32s", manifest[offset:offset + 40])
            if asset_id not in ASSET_IDS or asset_id in seen:
                raise RuntimeError("Packaged asset table is invalid")
            seen.add(asset_id)
            archive_name = ASSET_IDS[asset_id][2]
            actual_size, actual_digest = sha256_zip_entry(bundle, archive_name)
            if actual_size != size or actual_digest != digest:
                raise RuntimeError(f"Packaged asset does not match its manifest: {archive_name}")
        metadata = json.loads(bundle.read("package.json"))
        if (metadata.get("packageSequence") != sequence or
                metadata.get("assetPackVersion") != pack_version):
            raise RuntimeError("Human-readable package metadata disagrees with manifest")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--firmware", type=Path, default=DEFAULT_FIRMWARE)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--sequence", type=int)
    args = parser.parse_args()
    if args.sequence is None:
        previous = int(LAST_SEQUENCE.read_text(encoding="ascii").strip()) \
            if LAST_SEQUENCE.exists() else 0
        args.sequence = max(int(time.time()), previous + 1)
    firmware = args.firmware.resolve()
    if not firmware.is_file():
        raise FileNotFoundError(f"Build firmware first; not found: {firmware}")
    if firmware.stat().st_size > OTA_SLOT_SIZE:
        raise RuntimeError(
            f"Firmware is {firmware.stat().st_size:,} bytes but the OTA slot is only "
            f"{OTA_SLOT_SIZE:,} bytes"
        )
    private, public = load_or_create_key()
    # Prevent the subtle first-run failure where a new key header is generated
    # after firmware.bin was built: that image could never verify the package
    # that carries it. The exact uncompressed SEC1 point must be embedded.
    if public_key_bytes(public) not in firmware.read_bytes():
        raise RuntimeError(
            "Firmware was built with a different update key. Rebuild firmware, then rerun this script."
        )
    manifest, assets = build_manifest(firmware, args.sequence, private)
    if not verify(public, manifest[:212], manifest[212:]):
        raise RuntimeError("Internal signature self-test failed")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    metadata = {
        "format": "Pokegochi Wireless Update",
        "protocol": PROTOCOL_VERSION,
        "deviceModel": "ESP32-2432S028R",
        "packageSequence": args.sequence,
        "gameVersion": game_version(),
        "assetPackVersion": asset_pack_version(),
        "firmware": {
            "file": "payload/firmware.bin",
            "size": firmware.stat().st_size,
            "sha256": sha256_file(firmware).hex(),
        },
        "assets": assets,
    }
    with zipfile.ZipFile(args.output, "w", allowZip64=True) as bundle:
        bundle.writestr("manifest.bin", manifest, compress_type=zipfile.ZIP_STORED)
        bundle.writestr("package.json", json.dumps(metadata, indent=2) + "\n",
                        compress_type=zipfile.ZIP_DEFLATED)
        bundle.write(firmware, "payload/firmware.bin", compress_type=zipfile.ZIP_STORED)
        for _, path, archive_name in ASSET_IDS.values():
            bundle.write(path, archive_name, compress_type=zipfile.ZIP_STORED)
    verify_bundle(args.output, public)
    LAST_SEQUENCE.parent.mkdir(parents=True, exist_ok=True)
    LAST_SEQUENCE.write_text(f"{args.sequence}\n", encoding="ascii")
    print(f"Wireless update: {args.output}")
    print(f"Sequence: {args.sequence} | Firmware: {firmware.stat().st_size:,} bytes")
    print(f"SD payload: {sum(item['size'] for item in assets):,} bytes | Signed and reopened: P-256/SHA-256")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
