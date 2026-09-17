"""Audit every Generation-III move effect used by Pokegochi.

The two pret decompilations are the source of truth.  This gate deliberately
does not accept an unknown effect through a default branch: every effect must
either have an explicit engine reference or belong to one of the small,
mechanically generic families below.  It also verifies that FireRed and
Emerald agree on the complete 354-move battle table used by the firmware.
"""
from __future__ import annotations

import re
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def reference(name: str) -> Path:
    candidates = [
        ROOT / ".references" / name,
        ROOT / ".downloads" / f"{name}-tree" / f"{name}-master",
    ]
    for candidate in candidates:
        if (candidate / "src/data/battle_moves.h").is_file():
            return candidate
    raise FileNotFoundError(f"missing local {name} reference")


FIRERED = reference("pokefirered")
EMERALD = reference("pokeemerald")


@dataclass(frozen=True)
class MoveRow:
    effect: str
    power: str
    type: str
    accuracy: str
    pp: str
    secondary: str
    target: str
    priority: str
    flags: tuple[str, ...]


def field(block: str, name: str, default: str = "0") -> str:
    found = re.search(rf"\.{name}\s*=\s*([A-Z0-9_\-]+)", block)
    return found.group(1) if found else default


def move_rows(repo: Path) -> dict[str, MoveRow]:
    text = (repo / "src/data/battle_moves.h").read_text(encoding="utf-8")
    rows: dict[str, MoveRow] = {}
    for symbol, block in re.findall(
        r"\[MOVE_([A-Z0-9_]+)\]\s*=\s*\{(.*?)\n\s*\},", text, re.S
    ):
        if symbol == "NONE":
            continue
        flags = tuple(sorted(re.findall(r"FLAG_[A-Z0-9_]+", block)))
        rows[symbol] = MoveRow(
            effect=field(block, "effect"),
            power=field(block, "power"),
            type=field(block, "type"),
            accuracy=field(block, "accuracy"),
            pp=field(block, "pp"),
            secondary=field(block, "secondaryEffectChance"),
            target=field(block, "target", "MOVE_TARGET_SELECTED"),
            priority=field(block, "priority"),
            flags=flags,
        )
    return rows


def effect_scripts(repo: Path) -> list[tuple[str, str]]:
    text = (repo / "data/battle_scripts_1.s").read_text(encoding="utf-8")
    return [
        (effect, script)
        for script, effect in re.findall(
            r"\.4byte\s+(BattleScript_[A-Za-z0-9_]+)\s+@ EFFECT_([A-Z0-9_]+)",
            text,
        )
    ]


# These effects are fully described by the generated move row plus the common
# accuracy, damage, priority and target pipeline.  Their source battle script
# is BattleScript_EffectHit (or an equivalent direct-hit wrapper).
DIRECT_PIPELINE = {
    "HIT",
    "QUICK_ATTACK",
    "EARTHQUAKE",
    "GUST",
    "SKY_UPPERCUT",
}

# One-versus-one Pokegochi has no legal ally.  The engine intentionally emits
# Failed for these doubles-only commands, which is the documented adaptation.
SINGLE_BATTLE_ADAPTATIONS = {"FOLLOW_ME", "HELPING_HAND", "SPLASH"}

# The retail Gen-III tables are not byte-for-byte identical in exactly these
# three rows. Pokegochi deliberately keeps FireRed as its canonical ruleset;
# Emerald is used as a second implementation reference for the effect logic.
# Any additional table divergence is still a hard audit failure.
KNOWN_VERSION_ROW_DIFFERENCES = {"KINESIS", "NATURE_POWER", "SNATCH"}


def generic_family(effect: str) -> str | None:
    # Stat effects are resolved by applyStageEffect.  The exact stat, target,
    # one/two-stage magnitude and secondary-hit chance remain encoded in the
    # original EFFECT_* name.
    if re.fullmatch(
        r"(ATTACK|DEFENSE|SPEED|SPECIAL_ATTACK|SPECIAL_DEFENSE|ACCURACY|EVASION)"
        r"_(UP|DOWN)(?:_2|_HIT)?",
        effect,
    ):
        return "generic-stat-stage"
    if effect in {
        "BURN_HIT",
        "CONFUSE_HIT",
        "FLINCH_HIT",
        "FREEZE_HIT",
        "PARALYZE_HIT",
        "POISON_HIT",
    }:
        return "generic-secondary-status"
    return None


def main() -> int:
    fire_rows = move_rows(FIRERED)
    emerald_rows = move_rows(EMERALD)
    if len(fire_rows) != 354 or len(emerald_rows) != 354:
        raise AssertionError(
            f"expected 354 moves, got FireRed={len(fire_rows)} Emerald={len(emerald_rows)}"
        )
    differing = {
        symbol
        for symbol in set(fire_rows) | set(emerald_rows)
        if fire_rows.get(symbol) != emerald_rows.get(symbol)
    }
    if differing != KNOWN_VERSION_ROW_DIFFERENCES:
        raise AssertionError(
            "unexpected FireRed/Emerald move-table divergence: "
            + ", ".join(sorted(differing))
        )

    fire_effects = effect_scripts(FIRERED)
    emerald_effects = effect_scripts(EMERALD)
    if fire_effects != emerald_effects:
        raise AssertionError("FireRed/Emerald effect dispatch tables differ")

    engine = (ROOT / "src/game/BattleEngine.cpp").read_text(encoding="utf-8")
    moves_by_effect: dict[str, list[str]] = defaultdict(list)
    for move, row in fire_rows.items():
        moves_by_effect[row.effect.removeprefix("EFFECT_")].append(move)
    used_effects = set(moves_by_effect)
    if len(used_effects) != 198:
        raise AssertionError(f"expected 198 effects used by moves, got {len(used_effects)}")
    source_scripts = dict(fire_effects)
    if not used_effects <= source_scripts.keys():
        raise AssertionError("move table references an effect absent from source dispatch")

    classified: dict[str, str] = {}
    missing: list[str] = []
    for effect in sorted(used_effects):
        source_script=source_scripts[effect]
        if f'"{effect}"' in engine:
            classified[effect] = "explicit-engine-handler"
        elif effect in DIRECT_PIPELINE:
            classified[effect] = "common-damage-pipeline"
        elif effect in SINGLE_BATTLE_ADAPTATIONS:
            classified[effect] = "documented-single-battle-adaptation"
        elif family := generic_family(effect):
            classified[effect] = family
        else:
            moves = ", ".join(moves_by_effect.get(effect, [])) or "<unused>"
            missing.append(f"{effect} via {source_script}: {moves}")

    if missing:
        print("Unclassified move effects (default handling is forbidden):", file=sys.stderr)
        for entry in missing:
            print(f"  - {entry}", file=sys.stderr)
        return 1

    counts: dict[str, int] = defaultdict(int)
    for category in classified.values():
        counts[category] += 1
    summary = ", ".join(f"{name}={count}" for name, count in sorted(counts.items()))
    if "--verbose" in sys.argv:
        for effect in sorted(classified):
            moves=",".join(moves_by_effect[effect])
            print(f"{effect:28} {classified[effect]:36} {source_scripts[effect]:42} {moves}")
    print(
        "Move-effect audit passed: 354 moves, 198 effects; "
        "3 known version rows use FireRed; " + summary
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
