"""Authoritative Ultra Sun personal-table and TM bitset contract."""

from __future__ import annotations

import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from alola_rom_data import audit_manifest, can_learn_tm, personal_record  # noqa: E402


def main() -> None:
    rowlet = personal_record(722)
    zeraora = personal_record(807)
    assert len(rowlet) == 0x54 and rowlet[:8] == bytes((68, 55, 55, 42, 50, 50, 11, 2))
    assert len(zeraora) == 0x54 and zeraora[:8] == bytes((88, 112, 75, 143, 102, 80, 12, 12))

    # Stable positive/negative anchors exercise different bytes in the
    # TM01..TM100 bitset instead of only checking species metadata.
    assert can_learn_tm(722, 1) and can_learn_tm(722, 96)
    assert not can_learn_tm(722, 2) and not can_learn_tm(722, 99)
    assert can_learn_tm(778, 28) and can_learn_tm(778, 99)
    assert can_learn_tm(807, 59) and not can_learn_tm(807, 67)

    audit = audit_manifest()
    assert audit["national_species"] == 807
    assert audit["personal_rows_including_forms"] == 976
    assert audit["learnsets"] == 976 and audit["evolutions"] == 976
    print("Ultra Sun RomFS contract passed: 807 species and TM01..TM100 bitsets.")


if __name__ == "__main__":
    main()
