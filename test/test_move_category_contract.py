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
from alola_expansion_data import (move_block as alola_move_block,  # noqa: E402
                                  move_field as alola_move_field,
                                  move_symbols as alola_move_symbols)


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

expected_move_ids = set(range(1, 618)) | set(range(622, 676))
assert set(rows) == expected_move_ids, (
    f"Generated category table has {len(rows)} unique moves instead of 671"
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
white_types = (
    "Normal", "Fighting", "Flying", "Poison", "Ground", "Rock", "Bug",
    "Ghost", "Steel", "Fire", "Water", "Grass", "Electric", "Psychic",
    "Ice", "Dragon", "Dark",
)
for move_id in range(468, 560):
    record = move_record(move_id)
    expected_type = white_types[record[0]]
    expected = white_categories[record[2]]
    assert rows[move_id][1] == expected_type, (
        f"Move {move_id} {rows[move_id][0]} type is {rows[move_id][1]}, "
        f"expected {expected_type}"
    )
    assert rows[move_id][2] == expected, (
        f"Move {move_id} {rows[move_id][0]} is {rows[move_id][2]}, expected {expected}"
    )

xy_types = white_types + ("Fairy",)
for move_id in range(560, 618):
    record = y_move_record(move_id)
    expected_type = xy_types[record[0]]
    expected = white_categories[record[2]]
    assert rows[move_id][1] == expected_type, (
        f"Move {move_id} {rows[move_id][0]} type is {rows[move_id][1]}, "
        f"expected {expected_type}"
    )
    assert rows[move_id][2] == expected, (
        f"Move {move_id} {rows[move_id][0]} is {rows[move_id][2]}, expected {expected}"
    )

for move_id, symbol in sorted(alola_move_symbols().items()):
    block = alola_move_block(symbol)
    expected_type = alola_move_field(block, "type").removeprefix("TYPE_").title()
    expected = alola_move_field(block, "category").removeprefix(
        "DAMAGE_CATEGORY_").title()
    assert rows[move_id][1] == expected_type, (
        f"Move {move_id} {rows[move_id][0]} type is {rows[move_id][1]}, "
        f"expected {expected_type}"
    )
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

descriptions = (ROOT / "src/game/MoveDescriptionsGenerated.inc").read_text(
    encoding="utf-8")

def generated_array(name: str) -> list[int]:
    match = re.search(
        rf"constexpr (?:uint8_t|uint16_t) {name}(?:\[[^\]]*\])? = \{{(.*?)\}};",
        descriptions,
        re.DOTALL,
    )
    assert match, f"Missing generated array {name}"
    return [int(token, 0) for token in re.findall(r"0x[0-9A-F]+|\d+", match.group(1))]

description_offsets = generated_array("kMoveDescriptionOffsets")
word_offsets = generated_array("kMoveDescriptionWordOffsets")
word_bytes = generated_array("kMoveDescriptionWords")
description_bytes = generated_array("kMoveDescriptionData")

def decoded_description(move_id: int) -> str:
    cursor = description_offsets[move_id]
    words: list[str] = []
    while description_bytes[cursor] != 0xFF:
        token = description_bytes[cursor]
        cursor += 1
        if token:
            start, end = word_offsets[token - 1:token + 1]
            words.append(bytes(word_bytes[start:end]).decode("ascii"))
        else:
            length = description_bytes[cursor]
            cursor += 1
            words.append(bytes(description_bytes[cursor:cursor + length]).decode("ascii"))
            cursor += length
    return " ".join(words)

for move_id in (473, 540, 548):
    assert decoded_description(move_id) == (
        "A SPECIAL ATTACK THAT CALCULATES DAMAGE USING THE TARGET'S DEFENSE."
    )

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
    "Move contract passed: 671/671 categories and all 204 Gen-V--VII types; "
    f"{physical_from_special_type} physical moves use formerly-special types, "
    f"{special_from_physical_type} special moves use formerly-physical types; Summary uses category data."
)
