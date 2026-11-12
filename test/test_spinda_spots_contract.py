#!/usr/bin/env python3
"""Exact Generation-III Spinda personality/spot rendering contract."""

from __future__ import annotations

import re
import struct
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
RENDERER = (ROOT / "src/drivers/AssetRenderer.cpp").read_text(encoding="utf-8")
ASSETS = ROOT / ".generated/sdcard/pokegochi/assets"
SOURCE = ROOT / ".downloads/pokefirered-tree/pokefirered-master/graphics/pokemon/spinda"
TRANSPARENT = 0xF81F


def rgb565(red: int, green: int, blue: int) -> int:
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def palette(path: Path) -> list[tuple[int, int, int]]:
    lines = path.read_text(encoding="ascii").splitlines()
    assert lines[:3] == ["JASC-PAL", "0100", "16"]
    return [tuple(map(int, line.split())) for line in lines[3:19]]


def package(path: Path) -> tuple[list[int], list[int]]:
    data = path.read_bytes()
    magic, width, height, transparent, bits, count = struct.unpack_from(
        "<4sHHHBB", data)
    assert (magic, width, height, transparent, bits, count) == (
        b"PKG2", 64, 64, TRANSPARENT, 4, 16)
    colors = list(struct.unpack_from(f"<{count}H", data, 12))
    payload = data[12 + count * 2:]
    indices = [
        ((payload[pixel // 2] >> 4) & 0xF)
        if pixel % 2 == 0 else payload[pixel // 2] & 0xF
        for pixel in range(width * height)
    ]
    return colors, indices


# Exact source coordinates, byte order and the original -8..+7 displacement.
assert "kBaseX[4] = {16U,40U,22U,34U}" in RENDERER
assert "kBaseY[4] = { 7U, 8U,25U,26U}" in RENDERER
assert "personality >> (8U * spot)" in RENDERER
assert "static_cast<int16_t>(pidByte & 0x0FU) - 8" in RENDERER
assert "static_cast<int16_t>(pidByte >> 4U) - 8" in RENDERER
assert "kMasks[spot][localY] >> localX" in RENDERER

# The four exact 16x16 masks from pokeemerald must remain complete.
mask_block = RENDERER[
    RENDERER.index("static constexpr uint16_t kMasks[4][16]"):
    RENDERER.index("static constexpr uint8_t kBaseX[4]")
]
assert len(re.findall(r"0x[0-9A-F]{4}", mask_block)) == 64

# Generated normal and shiny fronts retain the original numeric pixels and
# palettes. This is the property DrawSpindaSpots relies on for 1..3 -> 5..7.
source = Image.open(SOURCE / "front.png")
assert source.mode == "P" and source.size == (64, 64)
source_indices = list(source.getdata())
normal_palette = palette(SOURCE / "normal.pal")
for relative, palette_name in (
    ("pokemon/front/327.pkg", "normal.pal"),
    ("pokemon/shiny/front/327.pkg", "shiny.pal"),
):
    colors, indices = package(ASSETS / relative)
    expected = palette(SOURCE / palette_name)
    assert colors[0] == TRANSPARENT
    assert colors[1:] == [rgb565(*color) for color in expected[1:]]
    assert indices == source_indices
    assert all(indices.count(index) > 0 for index in (1, 2, 3, 5, 6, 7))

# Compatibility with already-deployed v55 PAKs: their RGBA writer compacted
# entries by first on-screen occurrence. Confirm the exact legacy shuffle and
# that the renderer contains every authored body/spot colour used to recover
# the semantic mapping without rewriting the microSD.
legacy_colors: list[int] = []
legacy_map: dict[int, int] = {}
for source_index in source_indices:
    color = TRANSPARENT if source_index == 0 else rgb565(
        *normal_palette[source_index])
    if color not in legacy_colors:
        legacy_colors.append(color)
    legacy_map[source_index] = legacy_colors.index(color)
assert [legacy_map[index] for index in (1, 2, 3)] == [5, 3, 4]
assert [legacy_map[index] for index in (5, 6, 7)] == [8, 11, 9]
for color in normal_palette[1:4] + normal_palette[5:8]:
    assert f"0x{rgb565(*color):04X}U" in RENDERER

print("Spinda spots contract passed: exact masks, personality bytes, coordinates and palettes.")
