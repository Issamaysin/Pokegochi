"""Read canonical Generation-VI tables from an extracted Pokemon Y RomFS.

Pokemon X/Y store gameplay data in GARC archives rather than the NDS
NARC/FAT containers used by Black/White.  This module intentionally owns the
small, read-only subset of GARC needed by Pokegochi so generation does not
depend on a GUI ROM editor.  The binary layouts and RomFS paths mirror the
open-source pk3DS Gen-VI readers, while all input bytes still come from the
user's local dump.
"""

from __future__ import annotations

import hashlib
import json
import os
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_Y_ROMFS = ROOT / ".generated" / "research" / "kalos_roms" / "romfs"

# pk3DS GARCReference_XY numbers.  A decimal reference ABC maps to a/A/B/C.
Y_GARC_PATHS = {
    "moves": Path("a/2/1/2"),
    "learnsets": Path("a/2/1/4"),
    "evolutions": Path("a/2/1/5"),
    "mega_evolutions": Path("a/2/1/6"),
    "personal": Path("a/2/1/8"),
}

KALOS_FIRST_SPECIES = 650
KALOS_LAST_SPECIES = 721
XY_LAST_MOVE = 617
PERSONAL_RECORD_SIZE = 0x40
# Move6 exposes a 0x22-byte payload; retail X/Y GARC members retain two bytes
# of member alignment after it, so every canonical row occupies 0x24 bytes.
MOVE_RECORD_SIZE = 0x24
EVOLUTION_RECORD_SIZE = 8 * 6


def _u16(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 2 > len(data):
        raise ValueError(f"Truncated u16 at 0x{offset:X}")
    return struct.unpack_from("<H", data, offset)[0]


def _u32(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise ValueError(f"Truncated u32 at 0x{offset:X}")
    return struct.unpack_from("<I", data, offset)[0]


def _expect_magic(data: bytes, offset: int, *values: bytes) -> None:
    actual = data[offset:offset + 4]
    if actual not in values:
        expected = "/".join(value.decode("ascii", "replace") for value in values)
        raise ValueError(
            f"Unexpected GARC magic {actual!r} at 0x{offset:X}; expected {expected}"
        )


def garc_entries(blob: bytes) -> list[list[bytes | None]]:
    """Return every GARC entry and its up-to-32 indexed subfiles.

    X/Y uses GARC version 4.  Accepting version 6 as well costs nothing and
    makes corruption diagnostics clearer if the wrong 3DS generation is
    supplied.  Reverse FourCCs are the byte order used by retail archives;
    forward spellings are accepted for synthetic fixtures and tooling output.
    """
    if len(blob) < 0x1C:
        raise ValueError("Truncated GARC header")
    _expect_magic(blob, 0, b"CRAG", b"GARC")
    header_size = _u32(blob, 4)
    version = _u16(blob, 10)
    chunk_count = _u32(blob, 12)
    data_offset = _u32(blob, 16)
    file_size = _u32(blob, 20)
    if version not in (0x0400, 0x0600):
        raise ValueError(f"Unsupported GARC version 0x{version:04X}")
    expected_header_size = 0x1C if version == 0x0400 else 0x24
    if header_size != expected_header_size:
        raise ValueError(
            f"Unexpected GARC header size 0x{header_size:X} for version 0x{version:04X}"
        )
    if chunk_count != 4:
        raise ValueError(f"Unexpected GARC chunk count {chunk_count}")
    if file_size > len(blob) or file_size < data_offset:
        raise ValueError(f"Invalid GARC file size {file_size} for {len(blob)} bytes")

    offset = header_size
    _expect_magic(blob, offset, b"OTAF", b"FATO")
    fato_size = _u32(blob, offset + 4)
    entry_count = _u16(blob, offset + 8)
    if fato_size < 12 + 4 * entry_count or offset + fato_size > len(blob):
        raise ValueError("Invalid GARC FATO size")
    offset += fato_size

    _expect_magic(blob, offset, b"BTAF", b"FATB")
    fatb_size = _u32(blob, offset + 4)
    file_count = _u32(blob, offset + 8)
    if fatb_size < 12 or offset + fatb_size > len(blob):
        raise ValueError("Invalid GARC FATB size")
    cursor = offset + 12
    ranges: list[list[tuple[int, int, int] | None]] = []
    counted_files = 0
    for _ in range(entry_count):
        vector = _u32(blob, cursor)
        cursor += 4
        subfiles: list[tuple[int, int, int] | None] = [None] * 32
        for subfile in range(32):
            if not vector & (1 << subfile):
                continue
            if cursor + 12 > offset + fatb_size:
                raise ValueError("GARC FATB entry exceeds its chunk")
            start, end, length = struct.unpack_from("<III", blob, cursor)
            cursor += 12
            if start > end or length > end - start:
                raise ValueError(
                    f"Invalid GARC member range {start}..{end} with length {length}"
                )
            subfiles[subfile] = (start, end, length)
            counted_files += 1
        ranges.append(subfiles)
    if cursor > offset + fatb_size:
        raise ValueError("GARC FATB parser crossed the chunk boundary")
    if file_count != counted_files:
        raise ValueError(
            f"GARC FATB declares {file_count} files but indexes {counted_files}"
        )
    offset += fatb_size

    _expect_magic(blob, offset, b"BMIF", b"FIMG")
    fimg_size = _u32(blob, offset + 4)
    image_size = _u32(blob, offset + 8)
    if fimg_size < 12 or data_offset < offset + fimg_size:
        raise ValueError("Invalid GARC FIMG/data offset")
    if data_offset + image_size > file_size:
        raise ValueError("GARC image bytes exceed the declared file size")

    result: list[list[bytes | None]] = []
    for subfiles in ranges:
        unpacked: list[bytes | None] = [None] * 32
        for index, member in enumerate(subfiles):
            if member is None:
                continue
            start, _end, length = member
            absolute = data_offset + start
            if absolute + length > data_offset + image_size:
                raise ValueError("GARC member points outside FIMG")
            unpacked[index] = blob[absolute:absolute + length]
        result.append(unpacked)
    return result


def garc_files(blob: bytes) -> list[bytes]:
    """Return the primary member of every GARC entry.

    Gameplay tables use subfile zero.  Failing explicitly on a vectorized
    entry prevents a language/file-layout mismatch from being silently read
    as canonical data.
    """
    result: list[bytes] = []
    for index, entry in enumerate(garc_entries(blob)):
        if entry[0] is None:
            raise ValueError(f"GARC entry {index} has no primary member")
        result.append(entry[0])
    return result


def _find_romfs_root(path: Path) -> Path | None:
    candidates = (path, path / "romfs", path / "RomFS", path / "ExtractedRomFS")
    for candidate in candidates:
        if (candidate / Y_GARC_PATHS["personal"]).is_file():
            return candidate
    return None


def y_romfs_path() -> Path:
    configured = os.environ.get("POKEGOCHI_Y_ROMFS")
    requested = Path(configured).expanduser() if configured else DEFAULT_Y_ROMFS
    resolved = _find_romfs_root(requested)
    if resolved is None:
        raise FileNotFoundError(
            "Pokemon Y RomFS not found. Set POKEGOCHI_Y_ROMFS or extract the "
            f"validated dump to {DEFAULT_Y_ROMFS}; expected "
            f"{Y_GARC_PATHS['personal']} below that directory"
        )
    return resolved


_Y_TABLES: dict[str, list[bytes]] | None = None
_Y_TABLE_PATHS: dict[str, Path] | None = None


def y_tables() -> dict[str, list[bytes]]:
    global _Y_TABLES, _Y_TABLE_PATHS
    if _Y_TABLES is None:
        romfs = y_romfs_path()
        paths = {name: romfs / relative for name, relative in Y_GARC_PATHS.items()}
        tables = {name: garc_files(path.read_bytes()) for name, path in paths.items()}

        # X/Y has National species #1-721 plus alternate-form personal rows.
        personal_blob = tables["personal"][-1]
        if len(personal_blob) % PERSONAL_RECORD_SIZE:
            raise ValueError(
                f"Pokemon Y personal table is not aligned to 0x{PERSONAL_RECORD_SIZE:X} bytes"
            )
        if len(personal_blob) // PERSONAL_RECORD_SIZE <= KALOS_LAST_SPECIES:
            raise ValueError("Pokemon Y personal table does not contain species #721")
        if len(tables["learnsets"]) <= KALOS_LAST_SPECIES:
            raise ValueError("Pokemon Y learnset archive does not contain species #721")
        if len(tables["evolutions"]) <= KALOS_LAST_SPECIES:
            raise ValueError("Pokemon Y evolution archive does not contain species #721")
        if len(tables["moves"]) != XY_LAST_MOVE + 1:
            raise ValueError(
                f"Unexpected Pokemon Y move count: {len(tables['moves'])}, "
                f"expected {XY_LAST_MOVE + 1}"
            )

        # Anchors catch the most dangerous failure mode: a syntactically valid
        # GARC set from a different game/version.  Order is HP/Atk/Def/Spe/SpA/SpD.
        chespin = personal_blob[
            KALOS_FIRST_SPECIES * PERSONAL_RECORD_SIZE:
            (KALOS_FIRST_SPECIES + 1) * PERSONAL_RECORD_SIZE
        ]
        if chespin[:8] != bytes((56, 61, 65, 38, 48, 45, 11, 11)):
            raise ValueError("Pokemon Y personal anchor #650 (Chespin) did not match")
        light_of_ruin = tables["moves"][XY_LAST_MOVE]
        if len(light_of_ruin) != MOVE_RECORD_SIZE or light_of_ruin[:6] != bytes((17, 0, 2, 140, 90, 5)):
            raise ValueError("Pokemon Y move anchor #617 (Light of Ruin) did not match")

        _Y_TABLES = tables
        _Y_TABLE_PATHS = paths
    return _Y_TABLES


def personal_record(species_id: int) -> bytes:
    if not 0 <= species_id <= KALOS_LAST_SPECIES:
        raise IndexError(f"Pokemon Y species #{species_id} is outside 0..{KALOS_LAST_SPECIES}")
    blob = y_tables()["personal"][-1]
    start = species_id * PERSONAL_RECORD_SIZE
    return blob[start:start + PERSONAL_RECORD_SIZE]


def level_moves(species_id: int) -> list[tuple[int, int]]:
    if not 0 <= species_id < len(y_tables()["learnsets"]):
        raise IndexError(f"Pokemon Y learnset #{species_id} is outside the archive")
    data = y_tables()["learnsets"][species_id]
    if len(data) % 4:
        raise ValueError(f"Misaligned Pokemon Y learnset for #{species_id}")
    result: list[tuple[int, int]] = []
    for offset in range(0, len(data), 4):
        move, level = struct.unpack_from("<hh", data, offset)
        if move == -1:
            break
        if move < 0 or level < 0:
            raise ValueError(f"Invalid Pokemon Y learnset row for #{species_id}")
        result.append((level, move))
    return result


def evolutions(species_id: int) -> list[tuple[int, int, int]]:
    if not 0 <= species_id < len(y_tables()["evolutions"]):
        raise IndexError(f"Pokemon Y evolution #{species_id} is outside the archive")
    data = y_tables()["evolutions"][species_id]
    if len(data) != EVOLUTION_RECORD_SIZE:
        raise ValueError(
            f"Unexpected Pokemon Y evolution record for #{species_id}: {len(data)} bytes"
        )
    result: list[tuple[int, int, int]] = []
    for offset in range(0, len(data), 6):
        method, argument, target = struct.unpack_from("<HHH", data, offset)
        if method and target:
            result.append((method, argument, target))
    return result


def move_record(move_id: int) -> bytes:
    if not 0 <= move_id <= XY_LAST_MOVE:
        raise IndexError(f"Pokemon Y move #{move_id} is outside 0..{XY_LAST_MOVE}")
    record = y_tables()["moves"][move_id]
    if len(record) != MOVE_RECORD_SIZE:
        raise ValueError(f"Unexpected Pokemon Y move record #{move_id}: {len(record)} bytes")
    return record


def audit_manifest() -> dict[str, object]:
    """Return stable provenance/counts without copying copyrighted table data."""
    tables = y_tables()
    assert _Y_TABLE_PATHS is not None
    personal_rows = len(tables["personal"][-1]) // PERSONAL_RECORD_SIZE
    return {
        "source": "Pokemon Y extracted RomFS",
        "romfs": str(y_romfs_path()),
        "national_species": KALOS_LAST_SPECIES,
        "kalos_species": KALOS_LAST_SPECIES - KALOS_FIRST_SPECIES + 1,
        "personal_rows_including_forms": personal_rows,
        "moves": len(tables["moves"]),
        "learnsets": len(tables["learnsets"]),
        "evolutions": len(tables["evolutions"]),
        "mega_evolutions": len(tables["mega_evolutions"]),
        "garc_sha256": {
            name: hashlib.sha256(path.read_bytes()).hexdigest()
            for name, path in _Y_TABLE_PATHS.items()
        },
    }


if __name__ == "__main__":
    print(json.dumps(audit_manifest(), indent=2, sort_keys=True))
