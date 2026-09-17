"""Convert the local FireRed normal-font atlas into compact ESP32 glyph masks."""
from __future__ import annotations

import re
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
ATLAS = ROOT / ".downloads" / "fire-red-assets" / "graphics__fonts__latin_normal.png"
TEXT_SOURCE = ROOT / ".downloads" / "pokefirered-tree" / "pokefirered-master" / "src" / "text.c"
OUTPUT = ROOT / "src" / "drivers" / "FireRedFontData.inc"

# The decoded FireRed Latin atlas has a left-heavy waist pixel in digit 8
# (code 0xA9). At the ESP32 panel's native scale that missing right pixel makes
# it read as a 6. Keep the original proportions, but make the waist symmetric.
NORMAL_GLYPH_ROW_OVERRIDES: dict[int, dict[int, int]] = {
    0xA9: {6: 0x7800},
}


def widths() -> list[int]:
    source = TEXT_SOURCE.read_text(encoding="utf-8")
    match = re.search(r"sFontNormalLatinGlyphWidths\[\]\s*=\s*\{(.*?)\};", source, re.S)
    if not match:
        raise RuntimeError("FireRed normal-font width table not found")
    values = [int(value) for value in re.findall(r"\d+", match.group(1))]
    # The decomp keeps a second 256-entry extension after the Latin table.
    # The decoded Latin atlas used by the firmware corresponds to codes 0..255.
    if len(values) < 256:
        raise RuntimeError(f"Expected at least 256 glyph widths, found {len(values)}")
    return values[:256]


def main() -> None:
    image = Image.open(ATLAS)
    original_widths = widths()
    normal: list[list[int]] = []
    small: list[list[int]] = []
    for code in range(256):
        left, top = (code % 16) * 16, (code // 16) * 16
        rows: list[int] = []
        for y in range(16):
            mask = 0
            for x in range(16):
                # Palette entries 1 and 2 are the FireRed glyph ink; 0 and 3
                # are transparent/backing pixels in the decoded atlas.
                if image.getpixel((left + x, top + y)) in (1, 2):
                    mask |= 1 << (15 - x)
            rows.append(mask)
        for row, mask in NORMAL_GLYPH_ROW_OVERRIDES.get(code, {}).items():
            rows[row] = mask
        normal.append(rows)
        small.append([
            sum(1 << (7 - x) for x in range(8)
                if any(rows[y * 2 + dy] & (1 << (15 - (x * 2 + dx)))
                       for dy in range(2) for dx in range(2)))
            for y in range(8)
        ])

    lines = ["// Generated from the locally supplied FireRed font atlas. Do not edit.",
             "constexpr uint16_t kFireRedNormalGlyphs[256][16] = {"]
    lines.extend("  {" + ", ".join(f"0x{mask:04X}" for mask in rows) + "}," for rows in normal)
    lines.append("};")
    lines.append("constexpr uint8_t kFireRedNormalWidths[256] = {" + ", ".join(map(str, original_widths)) + "};")
    lines.append("constexpr uint8_t kFireRedSmallGlyphs[256][8] = {")
    lines.extend("  {" + ", ".join(f"0x{mask:02X}" for mask in rows) + "}," for rows in small)
    lines.append("};")
    lines.append("constexpr uint8_t kFireRedSmallWidths[256] = {" + ", ".join(str(max(3, (value + 1) // 2)) for value in original_widths) + "};")
    OUTPUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Generated FireRed glyph data: {OUTPUT}")


if __name__ == "__main__":
    main()
