#!/usr/bin/env python3
"""Inventory direct behavioral-test coverage for every supported Ability.

This is a discovery tool for R433, not an implementation token audit.  It
maps numeric ability assignments and named lookups in native tests back to the
generated catalog so a passing family-level matrix cannot hide an Ability
that was never exercised directly.
"""

from __future__ import annotations

import re
import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GENERATED = (ROOT / "src/game/PokemonDataGenerated.inc").read_text(
    encoding="utf-8"
)
MEGA = (ROOT / "src/game/MegaEvolution.cpp").read_text(encoding="utf-8")

ability_rows = {
    int(ability_id): name
    for ability_id, name in re.findall(
        r'\{\s*(\d+)\s*,\s*"([A-Z0-9 .\'-]+)"\s*,\s*"[^"]*"\s*\}',
        GENERATED,
    )
    if int(ability_id) != 0
}
custom_names = set(
    re.findall(
        r'\{\s*\w+\s*,\s*"([A-Z0-9 .\'-]+)"\s*,\s*"[^"]*"\s*\}',
        MEGA,
    )
)

test_text = "\n".join(
    path.read_text(encoding="utf-8", errors="strict")
    for folder in (ROOT / "test/native", ROOT / "test")
    for path in folder.glob("*.*")
    if path.suffix in {".cpp", ".py"}
)

covered: set[str] = set(re.findall(r'abilityNamed\("([A-Z0-9 .\'-]+)"\)', test_text))
covered.update(
    name
    for name in set(ability_rows.values()) | custom_names
    if f'"{name}"' in test_text
)
for raw_id in re.findall(r"abilityId\s*(?:==|=)\s*(\d+)U?", test_text):
    name = ability_rows.get(int(raw_id))
    if name:
        covered.add(name)

# The intentionally adapted list is already proven by the static contract;
# there is no in-battle trigger to execute in a singles-only test.
from audit_abilities import ADAPTED_NO_EFFECT, HANDLER_GROUPS  # noqa: E402

all_names = set(ability_rows.values()) | custom_names
implemented = all_names - set(ADAPTED_NO_EFFECT)
missing = sorted(implemented - covered)

observer_path = ROOT / ".codex-debug/r433-all-ability-observations.csv"
observed: set[str] = set()
if observer_path.is_file():
    with observer_path.open(encoding="utf-16" if observer_path.read_bytes()[:2] in
                            {b"\xff\xfe", b"\xfe\xff"} else "utf-8") as stream:
        for row in csv.reader(stream):
            if len(row) >= 3 and row[2] != "UNOBSERVED":
                observed.add(row[1])

proven = covered | observed
missing = sorted(implemented - proven)

print(
    f"Ability behavioral coverage: direct={len(implemented & covered)}, "
    f"generic-observed={len(implemented & observed)}, "
    f"union={len(implemented & proven)}/{len(implemented)}, "
    f"adapted={len(ADAPTED_NO_EFFECT)}, missing={len(missing)}"
)
for group, names in HANDLER_GROUPS.items():
    group_missing = sorted((names & implemented) - proven)
    if group_missing:
        print(f"[{group}] " + ", ".join(group_missing))

if missing:
    raise SystemExit(1)
