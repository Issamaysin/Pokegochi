"""Audit every move category and the Summary category presentation."""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PLATINUM = ROOT / ".downloads/pokeplatinum-tree"
sys.path.insert(0, str(ROOT / "scripts"))
from unova_rom_data import move_record  # noqa: E402
from kalos_rom_data import move_record as y_move_record  # noqa: E402


generated = (ROOT / "src/game/MoveDataGenerated.inc").read_text(encoding="utf-8")
row_pattern = re.compile(
    r'^\s*\{(\d+), "([^"]+)", PokemonType::(\w+), \d+, \d+, '
    r'MoveCategory::(\w+),',
    re.MULTILINE,
)
rows: dict[int, tuple[str, str, str]] = {}
for raw_id, name, move_type, category in row_pattern.findall(generated):
    move_id = int(raw_id)
    if move_id not in rows:
        rows[move_id] = (name, move_type, category)

assert set(rows) == set(range(1, 618)), (
    f"Generated category table has {len(rows)} unique moves instead of 617"
)

platinum_symbols = [
    line.strip().removeprefix("MOVE_")
    for line in (PLATINUM / "generated/moves.txt").read_text(encoding="utf-8").splitlines()
    if line.strip()
]
for move_id in range(1, 468):
    path = PLATINUM / "res/moves" / platinum_symbols[move_id].lower() / "data.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    expected = data["class"].removeprefix("CLASS_").title()
    assert rows[move_id][2] == expected, (
        f"Move {move_id} {rows[move_id][0]} is {rows[move_id][2]}, expected {expected}"
    )

white_categories = ("Status", "Physical", "Special")
for move_id in range(468, 560):
    expected = white_categories[move_record(move_id)[2]]
    assert rows[move_id][2] == expected, (
        f"Move {move_id} {rows[move_id][0]} is {rows[move_id][2]}, expected {expected}"
    )

for move_id in range(560, 618):
    expected = white_categories[y_move_record(move_id)[2]]
    assert rows[move_id][2] == expected, (
        f"Move {move_id} {rows[move_id][0]} is {rows[move_id][2]}, expected {expected}"
    )

assert rows[9][2] == "Physical"       # Thunder Punch
assert rows[49][2] == "Special"       # Sonic Boom
assert rows[246][2] == "Special"      # Ancient Power

main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
summary_start = main.index("const char* moveCategoryName(")
summary_end = main.index("const char* summaryNatureText(", summary_start)
summary = main[summary_start:summary_end]
assert "moveCategoryName(move->category)" in summary
assert "moveUsesSpecialAttack" not in main

old_physical_types = {
    "Normal", "Fighting", "Flying", "Poison", "Ground", "Rock", "Bug", "Ghost", "Steel"
}
physical_from_special_type = sum(
    category == "Physical" and move_type not in old_physical_types
    for _, move_type, category in rows.values()
)
special_from_physical_type = sum(
    category == "Special" and move_type in old_physical_types
    for _, move_type, category in rows.values()
)
print(
    "Move category contract passed: 617/617; "
    f"{physical_from_special_type} physical moves use formerly-special types, "
    f"{special_from_physical_type} special moves use formerly-physical types; Summary uses category data."
)
