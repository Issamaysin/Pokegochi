#!/usr/bin/env python3
"""Fail the build when a declared Pokemon Ability has no audited behavior.

Generation-III abilities are sourced from the generated FireRed table; Mega
abilities are the explicitly supported official-form extensions.  A literal
reference in the battle/field engine is required unless the ability has a
documented Pokegochi adaptation below.
"""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

generated = (ROOT / "src/game/PokemonDataGenerated.inc").read_text(encoding="utf-8")
start = generated.index("constexpr AbilityData kAbilities[]")
end = generated.index("};", start)
base = re.findall(r'\{\s*(\d+)\s*,\s*"([A-Z0-9 .\'-]+)"\s*,', generated[start:end])
base = [name for ability_id, name in base if int(ability_id) != 0]

mega_text = (ROOT / "src/game/MegaEvolution.cpp").read_text(encoding="utf-8")
start = mega_text.index("constexpr AbilityData kCustomAbilities[]")
end = mega_text.index("};", start)
custom = re.findall(r'\{\s*\w+\s*,\s*"([A-Z0-9 .\'-]+)"\s*,', mega_text[start:end])

abilities = list(dict.fromkeys(base + custom))
engine = "\n".join(
    (ROOT / relative).read_text(encoding="utf-8", errors="strict")
    for relative in ("src/game/BattleEngine.cpp", "src/game/EggSystem.cpp")
)

# Every supported Ability belongs to an explicitly reviewed mechanic family.
# This is intentionally independent of the C++ token scan below: merely
# mentioning an Ability in a comment can no longer make an unreviewed Ability
# look implemented. A newly generated Ability therefore fails CI until it is
# classified here and receives an engine handler plus behavioral coverage.
HANDLER_GROUPS = {
    "entry/weather/form": {
        "DRIZZLE", "INTIMIDATE", "TRACE", "FORECAST", "SAND STREAM",
        "DROUGHT", "PRIMORDIAL SEA", "DESOLATE LAND", "DELTA STREAM",
        "MEGA SOL",
    },
    "end-turn/cadence": {
        "SPEED BOOST", "RAIN DISH", "SHED SKIN", "TRUANT", "SOLAR POWER",
    },
    "accuracy/priority/PP": {
        "SAND VEIL", "COMPOUND EYES", "PRESSURE", "HUSTLE", "NO GUARD",
        "PRANKSTER",
    },
    "damage and critical modifiers": {
        "BATTLE ARMOR", "SHELL ARMOR", "STURDY", "DAMP", "SHIELD DUST",
        "WONDER GUARD", "THICK FAT", "HUGE POWER", "PURE POWER", "GUTS",
        "MARVEL SCALE", "OVERGROW", "BLAZE", "TORRENT", "SWARM",
        "ROCK HEAD", "TOUGH CLAWS", "MEGA LAUNCHER", "ADAPTABILITY",
        "PARENTAL BOND", "AERILATE", "MOLD BREAKER", "SAND FORCE",
        "TECHNICIAN", "SKILL LINK", "PIXILATE", "FILTER", "STRONG JAW",
        "SHEER FORCE", "REFRIGERATE", "MULTISCALE", "INNARDS OUT",
        "SHARPNESS", "DRAGONIZE", "ELECTRIC SURGE",
    },
    "type immunity/absorption/weather suppression": {
        "CLOUD NINE", "AIR LOCK", "VOLT ABSORB", "WATER ABSORB",
        "FLASH FIRE", "LEVITATE",
    },
    "status immunity/cure": {
        "LIMBER", "OBLIVIOUS", "INSOMNIA", "IMMUNITY", "OWN TEMPO",
        "INNER FOCUS", "MAGMA ARMOR", "WATER VEIL", "VITAL SPIRIT",
        "NATURAL CURE", "STEADFAST", "EARLY BIRD",
    },
    "contact/post-damage": {
        "STATIC", "COLOR CHANGE", "ROUGH SKIN", "EFFECT SPORE",
        "SYNCHRONIZE", "POISON POINT", "FLAME BODY", "CUTE CHARM",
        "LIQUID OOZE",
    },
    "stat-loss prevention": {
        "CLEAR BODY", "KEEN EYE", "HYPER CUTTER", "WHITE SMOKE",
    },
    "sound/forced switch/trapping": {
        "SUCTION CUPS", "SHADOW TAG", "MAGNET PULL", "SOUNDPROOF",
        "ARENA TRAP", "CACOPHONY",
    },
    "items/escape/field": {"RUN AWAY", "PICKUP", "STICKY HOLD"},
    "weather speed": {"SWIFT SWIM", "CHLOROPHYLL"},
    "secondary effects/reflection": {"SERENE GRACE", "MAGIC BOUNCE"},
}

# These are not accidental omissions. Pokegochi is strictly one-on-one and
# starts encounters explicitly from the Home icon, so the original field or
# doubles-only trigger does not exist. Keeping this list explicit makes every
# future ability addition fail CI until it gains a handler or an adaptation.
ADAPTED_NO_EFFECT = {
    "STENCH": "random-step encounter rate does not exist",
    "ILLUMINATE": "random-step encounter rate does not exist",
    "LIGHTNING ROD": "Generation III effect redirects only in double battles",
    "PLUS": "requires an allied MINUS user in a double battle",
    "MINUS": "requires an allied PLUS user in a double battle",
    "STALWART": "move redirection does not exist in one-on-one battles",
}

errors: list[str] = []
classified: dict[str, str] = {}
for group, names in HANDLER_GROUPS.items():
    for name in names:
        if name in classified:
            errors.append(f"{name}: classified twice ({classified[name]}, {group})")
        classified[name] = group

for name in abilities:
    if name in ADAPTED_NO_EFFECT:
        continue
    if name not in classified:
        errors.append(f"{name}: engine token exists but behavior is not classified")
        continue
    if f'"{name}"' not in engine:
        errors.append(f"{name}: no engine handler and no documented adaptation")

unknown_classifications = sorted(set(classified) - set(abilities))
if unknown_classifications:
    errors.append("stale behavior classifications: " + ", ".join(unknown_classifications))

unknown_adaptations = sorted(set(ADAPTED_NO_EFFECT) - set(abilities))
if unknown_adaptations:
    errors.append("stale adaptations: " + ", ".join(unknown_adaptations))

if len(base) != 77:
    errors.append(f"expected 77 FireRed abilities, parsed {len(base)}")
if len(custom) != 29:
    errors.append(f"expected 29 custom Mega/Primal abilities, parsed {len(custom)}")

if errors:
    print("Ability audit failed:", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    raise SystemExit(1)

print(
    f"Ability audit passed: {len(base)} FireRed + {len(custom)} Mega/Primal; "
    f"{len(abilities) - len(ADAPTED_NO_EFFECT)} implemented, "
    f"{len(ADAPTED_NO_EFFECT)} documented singles/manual adaptations"
)
