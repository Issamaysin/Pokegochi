from __future__ import annotations

import collections
import re
from pathlib import Path

root = Path(__file__).resolve().parent.parent
source = (root / "src/game/MoveDataGenerated.inc").read_text(encoding="utf-8")
rows = re.findall(
    r'^\s*\{(\d+), "([^"]+)".*?"([^"]+)", MoveTarget', source, re.M
)
effects: dict[str, list[tuple[int, str]]] = collections.defaultdict(list)
for raw_id, name, effect in rows:
    move_id = int(raw_id)
    if move_id >= 468:
        effects[effect].append((move_id, name))
for effect in sorted(effects):
    print(f"{effect}: " + ", ".join(f"{move_id} {name}" for move_id, name in effects[effect]))
