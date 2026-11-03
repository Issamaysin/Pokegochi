"""Regression contract for Alola and every authored Z-Move animation."""

from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXPANSION = ROOT / ".downloads/pokeemerald-expansion"
GENERATED = ROOT / "src/game/BattleAnimationGenerated.inc"
AUDIT = ROOT / ".generated/battle_animation_audit.json"
ROUTER = ROOT / "src/game/BattleAnimationData.cpp"
RUNTIME = ROOT / "src/main.cpp"
MOVES_INFO = EXPANSION / "src/data/moves_info.h"
SCRIPTS = EXPANSION / "data/battle_anim_scripts.s"
GEN7_IDS = set(range(622, 676))
GENERIC_Z = {
    848: "BREAKNECK_BLITZ",
    849: "ALL_OUT_PUMMELING",
    850: "SUPERSONIC_SKYSTRIKE",
    851: "ACID_DOWNPOUR",
    852: "TECTONIC_RAGE",
    853: "CONTINENTAL_CRUSH",
    854: "SAVAGE_SPIN_OUT",
    855: "NEVER_ENDING_NIGHTMARE",
    856: "CORKSCREW_CRASH",
    857: "INFERNO_OVERDRIVE",
    858: "HYDRO_VORTEX",
    859: "BLOOM_DOOM",
    860: "GIGAVOLT_HAVOC",
    861: "SHATTERED_PSYCHE",
    862: "SUBZERO_SLAMMER",
    863: "DEVASTATING_DRAKE",
    864: "BLACK_HOLE_ECLIPSE",
    865: "TWINKLE_TACKLE",
}
SIGNATURE_Z = {
    866: "CATASTROPIKA",
    867: "10000000_VOLT_THUNDERBOLT",
    868: "STOKED_SPARKSURFER",
    869: "EXTREME_EVOBOOST",
    870: "PULVERIZING_PANCAKE",
    871: "GENESIS_SUPERNOVA",
    872: "SINISTER_ARROW_RAID",
    873: "MALICIOUS_MOONSAULT",
    874: "OCEANIC_OPERETTA",
    875: "SPLINTERED_STORMSHARDS",
    876: "LETS_SNUGGLE_FOREVER",
    877: "CLANGOROUS_SOULBLAZE",
    878: "GUARDIAN_OF_ALOLA",
    879: "SEARING_SUNRAZE_SMASH",
    880: "MENACING_MOONRAZE_MAELSTROM",
    881: "LIGHT_THAT_BURNS_THE_SKY",
    882: "SOUL_STEALING_7_STAR_STRIKE",
}


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


def source_script_for(moves_info: str, symbol: str) -> str:
    match = re.search(
        rf"\[MOVE_{re.escape(symbol)}\]\s*=\s*\{{(.*?)(?=^\s*\[MOVE_|\Z)",
        moves_info,
        re.MULTILINE | re.DOTALL,
    )
    assert match, f"romhack move row missing: {symbol}"
    script = re.search(r"\.battleAnimScript\s*=\s*(gBattleAnimMove_[A-Za-z0-9_]+)",
                       match.group(1))
    assert script, f"romhack animation script missing: {symbol}"
    return script.group(1)


def main() -> None:
    generated = GENERATED.read_text(encoding="utf-8")
    descriptor_ids = row_ids(table_body(
        generated, "constexpr BattleAnimationData kBattleAnimations[]"))
    program_ids = row_ids(table_body(
        generated, "constexpr BattleAnimationProgram kBattleAnimationPrograms[]"))
    expected = GEN7_IDS | set(GENERIC_Z) | set(SIGNATURE_Z)
    assert expected <= descriptor_ids, sorted(expected - descriptor_ids)
    assert expected <= program_ids, sorted(expected - program_ids)

    audit = json.loads(AUDIT.read_text(encoding="utf-8"))
    moves = audit["moves"]
    assert {int(move_id) for move_id in moves if 622 <= int(move_id) <= 675} == GEN7_IDS
    assert {int(move_id) for move_id in moves if 848 <= int(move_id) <= 865} == set(GENERIC_Z)
    assert {int(move_id) for move_id in moves if 866 <= int(move_id) <= 882} == set(SIGNATURE_Z)
    for move_id in sorted(expected):
        move = moves[str(move_id)]
        assert move["sourceFound"], f"move {move_id}: source script missing"
        assert not move["untranslated"], (
            f"move {move_id}: untranslated commands {move['untranslated']}")
        assert move["programCommands"] > 0, f"move {move_id}: empty program"
        assert move["programSprites"] == move["scriptSprites"], (
            f"move {move_id}: dropped sprites")
        assert move["visualCoverage"], f"move {move_id}: no retained visual"

    moves_info = MOVES_INFO.read_text(encoding="utf-8")
    scripts = SCRIPTS.read_text(encoding="utf-8")
    for move_id, symbol in (GENERIC_Z | SIGNATURE_Z).items():
        assert moves[str(move_id)]["symbol"] == symbol
        source_label = source_script_for(moves_info, symbol)
        assert re.search(rf"(?m)^{re.escape(source_label)}::", scripts), source_label

    light = moves["881"]
    assert light["programCommands"] >= 150
    assert "AnimBackgroundChange" in light["flags"]
    assert "Sprite:PhotonGeyserBeam" in light["visualCoverage"]
    assert "Sprite:BlastBurnTargetPlume" in light["visualCoverage"]

    # Gen-VII rows must never be redirected to a merely similar legacy move.
    aliases = {
        int(value) for value in re.findall(
            r"case\s+(\d+)U\s*:\s*return", ROUTER.read_text(encoding="utf-8"))
    }
    assert not aliases & expected, sorted(aliases & expected)
    assert audit["program"]["moves"] >= 710

    # A Z-Move keeps the base move for targeting, power and battle semantics,
    # while IDs 848..882 select presentation-only romhack programs. Passing a
    # presentation ID to findFullMove() makes preload fail because those IDs
    # intentionally do not exist in the mechanical move table.
    runtime = RUNTIME.read_text(encoding="utf-8")
    assert runtime.count(
        "const MoveId renderMove = presentationMove != MoveId::None") == 2
    assert re.search(
        r"preparation\.ready\s*=\s*preloadMoveAnimationWithCapacityRetry\(\s*"
        r"event\.move,.*?preparation\.move\);",
        runtime,
        re.DOTALL,
    )
    assert re.search(
        r"playMoveAnimation\(event\.move,\s*enemyAttacking,\s*event\.value,.*?"
        r"animationMove\);",
        runtime,
        re.DOTALL,
    )

    print("Gen-VII animation contract passed: 54/54 moves and all 35 "
          "generic/signature Z-Moves retain exact romhack programs and "
          "separate mechanical/visual runtime IDs.")


if __name__ == "__main__":
    main()
