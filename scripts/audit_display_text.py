"""Reject UTF-8 characters from generated firmware display strings."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FILES = (
    ROOT / "src/game/TrainerDataGenerated.inc",
    ROOT / "src/game/BattleTowerDataGenerated.inc",
    ROOT / "src/game/PokedexDataGenerated.inc",
    ROOT / "src/game/MoveDescriptionsGenerated.inc",
)

failures: list[str] = []
for path in FILES:
    source = path.read_text(encoding="utf-8")
    for line_number, line in enumerate(source.splitlines(), start=1):
        # Comments can contain prose; only literals reach the display.
        for literal in re.findall(r'"(?:\\.|[^"\\])*"', line):
            symbols = sorted({character for character in literal if ord(character) > 0x7F})
            if symbols:
                codes = ", ".join(f"U+{ord(character):04X}" for character in symbols)
                failures.append(f"{path.name}:{line_number}: {codes}")

if failures:
    raise SystemExit("Unsupported generated display text:\n" + "\n".join(failures))
print(f"Display-text audit passed ({len(FILES)} generated tables, ASCII only).")
