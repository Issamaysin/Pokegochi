#!/usr/bin/env python3
"""Protocol-level signing tests that do not require the private project assets."""

from __future__ import annotations

import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "scripts/build_wireless_update.py"
FIRMWARE_SOURCE = (ROOT / "src/services/WirelessUpdate.cpp").read_text(
    encoding="utf-8"
)
RECOVERY_SOURCE = (ROOT / "src/recovery_main.cpp").read_text(encoding="utf-8")
SPEC = importlib.util.spec_from_file_location("pokegochi_update_builder", MODULE_PATH)
assert SPEC and SPEC.loader
builder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(builder)


def main() -> int:
    private = 1
    public = builder.scalar_multiply(private)
    assert public == builder.G
    message = bytes(range(212))
    signature = builder.sign(private, message)
    assert len(signature) == 64
    assert builder.verify(public, message, signature)
    assert not builder.verify(public, message[:-1] + b"X", signature)
    tampered = bytearray(signature)
    tampered[-1] ^= 1
    assert not builder.verify(public, message, bytes(tampered))
    # Deterministic signing makes the same release reproducible.
    assert signature == builder.sign(private, message)
    hash_file = FIRMWARE_SOURCE.split(
        "bool WirelessUpdate::hashFile", 1
    )[1].split("bool WirelessUpdate::hashPartition", 1)[0]
    # Asset verification must remain available under BLE heap pressure. A
    # heap-backed 4 KiB buffer caused valid full PAKs to be rejected on ESP32.
    assert "uint8_t buffer[1024]" in hash_file
    assert "std::malloc" not in hash_file
    assert "hashedBytes == fileBytes" in hash_file
    # The factory recovery helpers call into SD/VFS and SHA-256 from the
    # Arduino loop task. Their transfer arena belongs in BSS, not repeatedly
    # on the constrained loopTask stack.
    assert "uint8_t gIoBuffer[4096]" in RECOVERY_SOURCE
    assert "uint8_t buffer[4096]" not in RECOVERY_SOURCE
    assert RECOVERY_SOURCE.count("gIoBuffer") >= 12
    print("Wireless update signing tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
