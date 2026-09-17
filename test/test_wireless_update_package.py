#!/usr/bin/env python3
"""Protocol-level signing tests that do not require the private project assets."""

from __future__ import annotations

import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "scripts/build_wireless_update.py"
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
    print("Wireless update signing tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
