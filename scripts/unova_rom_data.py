"""Read canonical Generation-V tables from the local Pokemon White ROM.

The project already requires locally owned ROM/decomp inputs for generated
assets.  This reader deliberately implements only the two container formats
needed here (the NDS FAT and NARC), avoiding a runtime dependency on ndspy.
"""

from __future__ import annotations

import os
import struct
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
EXPANSION = ROOT / ".downloads" / "pokeemerald-expansion"
DEFAULT_WHITE_ROM = ROOT / ".generated" / "research" / "unova_roms" / "pokemon_white.nds"

# File ids verified from Pokemon White (IRAO).  Names in the NitroFS are,
# respectively: personal, level-up learnsets, evolutions and move data.
WHITE_FILE_IDS = {
    "personal": 258,
    "learnsets": 260,
    "evolutions": 261,
    "moves": 263,
    "item_icons": 267,
}

# a/0/2/5 is an interleaved NCGR/NCLR archive. These member pairs were
# verified against the IRAO file table and the corresponding canonical item
# ids; keeping the mapping beside the ROM reader prevents the asset builder
# from silently substituting a merely similar icon.
WHITE_ITEM_ICON_MEMBERS = {
    616: (823, 824),  # Light Stone
}


def white_rom_path() -> Path:
    configured = os.environ.get("POKEGOCHI_WHITE_ROM")
    path = Path(configured) if configured else DEFAULT_WHITE_ROM
    if not path.is_file():
        raise FileNotFoundError(
            "Pokemon White ROM not found. Set POKEGOCHI_WHITE_ROM or place the "
            f"validated IRAO ROM at {DEFAULT_WHITE_ROM}"
        )
    return path


def _u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def _u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def nds_file(rom: bytes, file_id: int) -> bytes:
    if rom[:12].rstrip(b"\0") != b"POKEMON W" or rom[12:16] != b"IRAO":
        raise ValueError("Unexpected ROM: Generation-V generation requires Pokemon White IRAO")
    fat_offset = _u32(rom, 0x48)
    fat_size = _u32(rom, 0x4C)
    if file_id < 0 or (file_id + 1) * 8 > fat_size:
        raise IndexError(f"NDS file id {file_id} outside FAT")
    start = _u32(rom, fat_offset + file_id * 8)
    end = _u32(rom, fat_offset + file_id * 8 + 4)
    if start > end or end > len(rom):
        raise ValueError(f"Invalid FAT range for file {file_id}: {start}..{end}")
    return rom[start:end]


def narc_files(blob: bytes) -> list[bytes]:
    if blob[:4] != b"NARC" or len(blob) < 16:
        raise ValueError("Expected NARC archive")
    chunk_count = _u16(blob, 14)
    offset = _u16(blob, 12)
    allocation: list[tuple[int, int]] | None = None
    image: bytes | None = None
    for _ in range(chunk_count):
        if offset + 8 > len(blob):
            raise ValueError("Truncated NARC chunk")
        magic = blob[offset:offset + 4]
        size = _u32(blob, offset + 4)
        if size < 8 or offset + size > len(blob):
            raise ValueError("Invalid NARC chunk size")
        if magic == b"BTAF":
            count = _u16(blob, offset + 8)
            allocation = [
                (_u32(blob, offset + 12 + index * 8),
                 _u32(blob, offset + 16 + index * 8))
                for index in range(count)
            ]
        elif magic == b"GMIF":
            image = blob[offset + 8:offset + size]
        offset += size
    if allocation is None or image is None:
        raise ValueError("NARC is missing BTAF or GMIF")
    files = []
    for start, end in allocation:
        if start > end or end > len(image):
            raise ValueError("NARC member points outside GMIF")
        files.append(image[start:end])
    return files


_WHITE_TABLES: dict[str, list[bytes]] | None = None
_EXPANSION_OBJECT_CACHE: dict[str, bytes] = {}
_EXPANSION_CAT_FILE: subprocess.Popen[bytes] | None = None


def white_tables() -> dict[str, list[bytes]]:
    global _WHITE_TABLES
    if _WHITE_TABLES is None:
        rom = white_rom_path().read_bytes()
        _WHITE_TABLES = {
            name: narc_files(nds_file(rom, file_id))
            for name, file_id in WHITE_FILE_IDS.items()
        }
        expected = {"personal": 669, "learnsets": 668,
                    "evolutions": 668, "moves": 560,
                    "item_icons": 1005}
        for name, count in expected.items():
            if len(_WHITE_TABLES[name]) != count:
                raise ValueError(
                    f"Unexpected Pokemon White {name} count: "
                    f"{len(_WHITE_TABLES[name])}, expected {count}"
                )
    return _WHITE_TABLES


def white_item_icon_resources(item_id: int) -> tuple[bytes, bytes]:
    """Return the exact NCGR/NCLR pair for a canonical Generation-V item."""
    try:
        graphics_member, palette_member = WHITE_ITEM_ICON_MEMBERS[item_id]
    except KeyError as error:
        raise KeyError(f"No verified Pokemon White icon mapping for item {item_id}") from error
    files = white_tables()["item_icons"]
    graphics, palette = files[graphics_member], files[palette_member]
    if graphics[:4] != b"RGCN" or palette[:4] != b"RLCN":
        raise ValueError(f"Pokemon White item {item_id} icon members changed")
    return graphics, palette


def expansion_text(path: str) -> str:
    return expansion_bytes(path).decode("utf-8")


def expansion_bytes(path: str) -> bytes:
    checked_out = EXPANSION / path
    if checked_out.is_file():
        return checked_out.read_bytes()
    cached = _EXPANSION_OBJECT_CACHE.get(path)
    if cached is not None:
        return cached

    # A sparse checkout keeps the multi-generation graphics tree small, but
    # spawning `git show` six times per Pokemon made a complete Gen-V asset
    # export take hours on Windows. One long-lived cat-file process serves the
    # same immutable HEAD objects with a tiny framed request per file.
    global _EXPANSION_CAT_FILE
    if _EXPANSION_CAT_FILE is None or _EXPANSION_CAT_FILE.poll() is not None:
        _EXPANSION_CAT_FILE = subprocess.Popen(
            ["git", "-C", str(EXPANSION), "cat-file", "--batch"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    process = _EXPANSION_CAT_FILE
    if process.stdin is None or process.stdout is None:
        raise RuntimeError("Unable to open expansion git object reader")
    process.stdin.write(f"HEAD:{path}\n".encode("utf-8"))
    process.stdin.flush()
    header = process.stdout.readline().decode("ascii", "replace").strip()
    fields = header.split()
    if len(fields) != 3 or fields[1] != "blob":
        raise FileNotFoundError(f"Expansion object {path}: {header}")
    size = int(fields[2])
    data = process.stdout.read(size)
    terminator = process.stdout.read(1)
    if len(data) != size or terminator != b"\n":
        raise RuntimeError(f"Truncated expansion object {path}")
    _EXPANSION_OBJECT_CACHE[path] = data
    return data


def internal_ability_id(canonical_id: int) -> int:
    """Map DS ids around FireRed's shipped Cacophony slot.

    Pokegochi permanently serialized Cacophony=76 and Air Lock=77.  The DS
    removed Cacophony, so every canonical ability from Air Lock onward is one
    slot lower.  Keeping the +1 translation preserves every existing save.
    """
    if not 0 <= canonical_id <= 254:
        raise ValueError(f"Ability id outside persistent byte: {canonical_id}")
    return canonical_id if canonical_id < 76 else canonical_id + 1


def personal_record(species_id: int) -> bytes:
    record = white_tables()["personal"][species_id]
    if len(record) != 60:
        raise ValueError(f"Unexpected personal record size for #{species_id}: {len(record)}")
    return record


def level_moves(species_id: int) -> list[tuple[int, int]]:
    data = white_tables()["learnsets"][species_id]
    if len(data) % 4:
        raise ValueError(f"Misaligned learnset for #{species_id}")
    result: list[tuple[int, int]] = []
    for offset in range(0, len(data), 4):
        move, level = struct.unpack_from("<HH", data, offset)
        if move == 0xFFFF and level == 0xFFFF:
            break
        result.append((level, move))
    return result


def evolutions(species_id: int) -> list[tuple[int, int, int]]:
    data = white_tables()["evolutions"][species_id]
    if len(data) != 42:
        raise ValueError(f"Unexpected evolution record for #{species_id}")
    result = []
    for offset in range(0, len(data), 6):
        method, parameter, target = struct.unpack_from("<HHH", data, offset)
        if method and target:
            result.append((method, parameter, target))
    return result


def move_record(move_id: int) -> bytes:
    record = white_tables()["moves"][move_id]
    if len(record) != 36:
        raise ValueError(f"Unexpected move record size for #{move_id}: {len(record)}")
    return record
