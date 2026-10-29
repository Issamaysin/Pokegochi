"""Container-level contract for the Pokemon Y GARC reader."""

from __future__ import annotations

import struct
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from kalos_rom_data import garc_entries, garc_files  # noqa: E402


def build_garc(files: list[bytes]) -> bytes:
    image = bytearray()
    ranges: list[tuple[int, int, int]] = []
    for data in files:
        while len(image) % 4:
            image.append(0)
        start = len(image)
        image.extend(data)
        end = len(image)
        ranges.append((start, end, len(data)))

    header_size = 0x1C
    fato_size = 12 + 4 * len(files)
    fatb_size = 12 + 16 * len(files)
    fimg_size = 12
    data_offset = header_size + fato_size + fatb_size + fimg_size
    file_size = data_offset + len(image)

    result = bytearray()
    result.extend(struct.pack(
        "<4sIHHIIII",
        b"CRAG", header_size, 0xFEFF, 0x0400, 4,
        data_offset, file_size, max(map(len, files), default=0),
    ))
    result.extend(struct.pack("<4sIHH", b"OTAF", fato_size, len(files), 0xFFFF))
    for index in range(len(files)):
        result.extend(struct.pack("<I", index * 16))
    result.extend(struct.pack("<4sII", b"BTAF", fatb_size, len(files)))
    for start, end, length in ranges:
        result.extend(struct.pack("<IIII", 1, start, end, length))
    result.extend(struct.pack("<4sII", b"BMIF", fimg_size, len(image)))
    result.extend(image)
    assert len(result) == file_size
    return bytes(result)


def main() -> None:
    source = [b"CHESPIN", b"LIGHT OF RUIN", bytes(range(64))]
    archive = build_garc(source)
    assert garc_files(archive) == source
    entries = garc_entries(archive)
    assert len(entries) == 3 and entries[1][0] == source[1]

    corrupt = bytearray(archive)
    corrupt[0:4] = b"NARC"
    try:
        garc_files(bytes(corrupt))
    except ValueError as error:
        assert "magic" in str(error).lower()
    else:
        raise AssertionError("Invalid GARC magic was accepted")
    print("Pokemon Y GARC reader contract passed.")


if __name__ == "__main__":
    main()
