"""Regression contract for the complete Pokemon X/Y move-animation catalog."""

from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GENERATED = ROOT / "src/game/BattleAnimationGenerated.inc"
AUDIT = ROOT / ".generated/battle_animation_audit.json"
GEN6_IDS = set(range(560, 618))


def table_body(source: str, declaration: str) -> str:
    start = source.index(declaration)
    open_brace = source.index("{", start)
    end = source.index("\n};", open_brace)
    return source[open_brace + 1 : end]


def row_ids(body: str) -> set[int]:
    return {
        int(match.group(1))
        for match in re.finditer(r"(?m)^\s*\{(\d+),", body)
    }


def main() -> None:
    generated = GENERATED.read_text(encoding="utf-8")
    descriptor_ids = row_ids(table_body(
        generated, "constexpr BattleAnimationData kBattleAnimations[]"))
    program_ids = row_ids(table_body(
        generated, "constexpr BattleAnimationProgram kBattleAnimationPrograms[]"))

    assert GEN6_IDS <= descriptor_ids, sorted(GEN6_IDS - descriptor_ids)
    assert GEN6_IDS <= program_ids, sorted(GEN6_IDS - program_ids)

    audit = json.loads(AUDIT.read_text(encoding="utf-8"))["moves"]
    assert {int(move_id) for move_id in audit if 560 <= int(move_id) <= 617} == GEN6_IDS
    for move_id in sorted(GEN6_IDS):
        move = audit[str(move_id)]
        assert move["sourceFound"], f"move {move_id}: source script missing"
        assert not move["untranslated"], (
            f"move {move_id}: untranslated commands {move['untranslated']}")
        assert move["programCommands"] > 0, f"move {move_id}: empty program"
        assert move["visualCoverage"], f"move {move_id}: no retained visual"

    print("Gen-VI animation contract passed: 58/58 moves have audited programs.")


if __name__ == "__main__":
    main()
