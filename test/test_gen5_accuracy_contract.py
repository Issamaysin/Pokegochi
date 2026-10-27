from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MOVE_DATA = (ROOT / "src/game/MoveDataGenerated.inc").read_text(encoding="utf-8")
ENGINE = (ROOT / "src/game/BattleEngine.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "include/game/BattleEngine.h").read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")

rows = re.findall(
    r'\{(\d+), "([^"]+)", PokemonType::\w+, \d+, (\d+), MoveCategory::\w+,',
    MOVE_DATA,
)
sentinel = [(int(move_id), name) for move_id, name, accuracy in rows
            if 468 <= int(move_id) <= 559 and int(accuracy) == 101]
expected_ids = [
    468, 469, 470, 471, 472, 475, 476, 477, 478, 483, 489,
    495, 499, 501, 502, 504, 505, 508, 513, 516, 526, 538,
]
assert [move_id for move_id, _ in sentinel] == expected_ids, sentinel
assert "!move.accuracy || move.accuracy > 100U" in ENGINE

enum_match = re.search(
    r"enum class BattleMoveEffect\s*:\s*uint8_t\s*\{(.*?)\};", HEADER, re.S
)
assert enum_match
enum_body = re.sub(r"//.*", "", enum_match.group(1))
effect_names = {
    match.group(1)
    for entry in enum_body.split(",")
    if (match := re.match(r"\s*([A-Za-z][A-Za-z0-9_]*)", entry))
}
presented = set(re.findall(r"case BattleMoveEffect::(\w+)", MAIN))
missing = sorted(effect_names - presented)
assert not missing, f"BattleMoveEffect values falling through presentation: {missing}"

print(
    "Gen-V accuracy/presentation contract passed: "
    f"{len(sentinel)}/22 accuracy-101 moves and {len(effect_names)}/"
    f"{len(effect_names)} move-effect messages covered."
)
