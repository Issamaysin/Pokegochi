"""Read canonical Ultra Sun tables from the user's extracted RomFS.

The Generation-VII personal archive keeps every 0x54-byte personal row in
the final GARC member.  Pokegochi uses only the National-Dex rows and the
TM01..TM100 permission bits at offset 0x28; alternate-form rows remain in the
archive for auditing but never displace species #1..807.
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

from kalos_rom_data import garc_files


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_USUM_ROMFS = ROOT / ".generated" / "research" / "alola_roms" / "romfs"
USUM_GARC_PATHS = {
    "moves": Path("a/0/1/1"),
    "learnsets": Path("a/0/1/3"),
    "evolutions": Path("a/0/1/4"),
    "personal": Path("a/0/1/7"),
}

ALOLA_FIRST_SPECIES = 722
ALOLA_LAST_SPECIES = 807
PERSONAL_RECORD_SIZE = 0x54
USUM_TM_COUNT = 100


def _find_romfs_root(path: Path) -> Path | None:
    for candidate in (path, path / "romfs", path / "RomFS", path / "ExtractedRomFS"):
        if (candidate / USUM_GARC_PATHS["personal"]).is_file():
            return candidate
    return None


def usum_romfs_path() -> Path:
    configured = os.environ.get("POKEGOCHI_USUM_ROMFS")
    requested = Path(configured).expanduser() if configured else DEFAULT_USUM_ROMFS
    resolved = _find_romfs_root(requested)
    if resolved is None:
        raise FileNotFoundError(
            "Pokemon Ultra Sun RomFS not found. Set POKEGOCHI_USUM_ROMFS or "
            f"extract the validated dump to {DEFAULT_USUM_ROMFS}; expected "
            f"{USUM_GARC_PATHS['personal']} below that directory"
        )
    return resolved


_TABLES: dict[str, list[bytes]] | None = None
_TABLE_PATHS: dict[str, Path] | None = None


def usum_tables() -> dict[str, list[bytes]]:
    global _TABLES, _TABLE_PATHS
    if _TABLES is None:
        romfs = usum_romfs_path()
        paths = {name: romfs / relative for name, relative in USUM_GARC_PATHS.items()}
        tables = {name: garc_files(path.read_bytes()) for name, path in paths.items()}

        personal_blob = tables["personal"][-1]
        if len(personal_blob) % PERSONAL_RECORD_SIZE:
            raise ValueError(
                f"Ultra Sun personal table is not aligned to 0x{PERSONAL_RECORD_SIZE:X} bytes"
            )
        if len(personal_blob) // PERSONAL_RECORD_SIZE <= ALOLA_LAST_SPECIES:
            raise ValueError("Ultra Sun personal table does not contain species #807")
        if len(tables["learnsets"]) <= ALOLA_LAST_SPECIES:
            raise ValueError("Ultra Sun learnset archive does not contain species #807")
        if len(tables["evolutions"]) <= ALOLA_LAST_SPECIES:
            raise ValueError("Ultra Sun evolution archive does not contain species #807")

        # HP/Atk/Def/Spe/SpA/SpD, Grass/Flying and the canonical three
        # ability slots. This rejects a syntactically valid archive from the
        # wrong 3DS title before it can generate persistent machine data.
        rowlet = personal_blob[
            ALOLA_FIRST_SPECIES * PERSONAL_RECORD_SIZE:
            (ALOLA_FIRST_SPECIES + 1) * PERSONAL_RECORD_SIZE
        ]
        if rowlet[:8] != bytes((68, 55, 55, 42, 50, 50, 11, 2)):
            raise ValueError("Ultra Sun personal anchor #722 (Rowlet) did not match")
        if rowlet[24:27] != bytes((65, 65, 203)):
            raise ValueError("Ultra Sun Rowlet ability anchor did not match")

        _TABLES = tables
        _TABLE_PATHS = paths
    return _TABLES


def personal_record(species_id: int) -> bytes:
    if not 0 <= species_id <= ALOLA_LAST_SPECIES:
        raise IndexError(
            f"Ultra Sun species #{species_id} is outside 0..{ALOLA_LAST_SPECIES}"
        )
    blob = usum_tables()["personal"][-1]
    start = species_id * PERSONAL_RECORD_SIZE
    return blob[start:start + PERSONAL_RECORD_SIZE]


def can_learn_tm(species_id: int, tm_number: int) -> bool:
    if not 1 <= tm_number <= USUM_TM_COUNT:
        raise IndexError(f"Ultra Sun TM number {tm_number} is outside 1..100")
    bit = tm_number - 1
    record = personal_record(species_id)
    return bool((record[0x28 + bit // 8] >> (bit % 8)) & 1)


def audit_manifest() -> dict[str, object]:
    tables = usum_tables()
    assert _TABLE_PATHS is not None
    return {
        "source": "Pokemon Ultra Sun extracted RomFS",
        "romfs": str(usum_romfs_path()),
        "national_species": ALOLA_LAST_SPECIES,
        "alola_species": ALOLA_LAST_SPECIES - ALOLA_FIRST_SPECIES + 1,
        "personal_rows_including_forms": (
            len(tables["personal"][-1]) // PERSONAL_RECORD_SIZE
        ),
        "move_container_bytes": len(tables["moves"][0]),
        "learnsets": len(tables["learnsets"]),
        "evolutions": len(tables["evolutions"]),
        "garc_sha256": {
            name: hashlib.sha256(path.read_bytes()).hexdigest()
            for name, path in _TABLE_PATHS.items()
        },
    }


if __name__ == "__main__":
    print(json.dumps(audit_manifest(), indent=2, sort_keys=True))
