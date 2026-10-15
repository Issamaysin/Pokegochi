"""Generate FireRed/Platinum move and level-up learnset tables."""
from __future__ import annotations

import json
import re
from pathlib import Path

from text_normalization import fire_red_ascii


ROOT = Path(__file__).resolve().parent.parent
FIRERED = ROOT / ".downloads/pokefirered-tree/pokefirered-master"
PLATINUM = ROOT / ".downloads/pokeplatinum-tree"

constants = (FIRERED / "include/constants/moves.h").read_text(encoding="utf-8")
info = (FIRERED / "src/data/battle_moves.h").read_text(encoding="utf-8")
description_source = (FIRERED / "src/move_descriptions.c").read_text(encoding="utf-8")
learnsets = (FIRERED / "src/data/pokemon/level_up_learnsets.h").read_text(encoding="utf-8")
pointers = (FIRERED / "src/data/pokemon/level_up_learnset_pointers.h").read_text(encoding="utf-8")
species_constants = (FIRERED / "include/constants/species.h").read_text(encoding="utf-8")
pokedex_constants = (FIRERED / "include/constants/pokedex.h").read_text(encoding="utf-8")

moves = [(int(number), symbol) for symbol, number in re.findall(
    r"#define MOVE_([A-Z0-9_]+)\s+(\d+)", constants)]
moves = sorted((number, symbol) for number, symbol in moves if number > 0)
move_ids = {symbol: number for number, symbol in moves}

platinum_move_symbols = [line.strip().removeprefix("MOVE_")
                         for line in (PLATINUM / "generated/moves.txt").read_text(
                             encoding="utf-8").splitlines() if line.strip()]
if len(platinum_move_symbols) <= 467 or platinum_move_symbols[355] != "ROOST":
    raise RuntimeError("Unexpected Platinum move order")
for number in range(355, 468):
    symbol = platinum_move_symbols[number]
    moves.append((number, symbol))
    move_ids[symbol] = number

description_literals = {
    variable: text for variable, text in re.findall(
        r'const u8 (gMoveDescription_[A-Za-z0-9_]+)\[\]\s*=\s*_\("(.*?)"\);',
        description_source,
    )
}
description_pointers = {
    symbol: variable for symbol, variable in re.findall(
        r"\[MOVE_([A-Z0-9_]+)\s*-\s*1\]\s*=\s*(gMoveDescription_[A-Za-z0-9_]+)",
        description_source,
    )
}


def c_string(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def normalized_description(symbol: str) -> str:
    raw = description_literals[description_pointers[symbol]]
    text = re.sub(r"\s+", " ", raw.replace(r"\n", " ")).strip()
    text = fire_red_ascii(text, f"move description {symbol}")
    text = text.replace("\r", " ").replace("\n", " ")
    text = re.sub(r"\s+", " ", text).strip()
    text = text.replace("POKÃ©MON", "POKEMON").replace("PokÃ©mon", "POKEMON")
    return c_string(text.upper())


def symbol_key(value: str) -> str:
    return re.sub(r"[^A-Z0-9]+", "_", value.upper()).strip("_")


platinum_move_data: dict[str, dict] = {}
for folder in (PLATINUM / "res/moves").iterdir():
    path = folder / "data.json"
    if path.is_file():
        data = json.loads(path.read_text(encoding="utf-8"))
        platinum_move_data[symbol_key(data["name"])] = data


internal_species = {symbol for symbol, _ in re.findall(
    r"#define SPECIES_([A-Z0-9_]+)\s+(\d+)", species_constants)}
national_symbols = re.findall(r"^\s*NATIONAL_DEX_([A-Z0-9_]+),", pokedex_constants, re.M)
species_ids = {symbol: number for number, symbol in enumerate(national_symbols)
               if 1 <= number <= 386 and symbol in internal_species}
types = {
    "NORMAL": "Normal", "FIGHTING": "Fighting", "FLYING": "Flying",
    "POISON": "Poison", "GROUND": "Ground", "ROCK": "Rock", "BUG": "Bug",
    "GHOST": "Ghost", "STEEL": "Steel", "FIRE": "Fire", "WATER": "Water",
    "GRASS": "Grass", "ELECTRIC": "Electric", "PSYCHIC": "Psychic",
    "ICE": "Ice", "DRAGON": "Dragon", "DARK": "Dark",
}
targets = {
    "SELECTED": "Selected", "DEPENDS": "Depends", "USER_OR_SELECTED": "UserOrSelected",
    "RANDOM": "Random", "BOTH": "Both", "USER": "User",
    "FOES_AND_ALLY": "FoesAndAlly", "OPPONENTS_FIELD": "OpponentsField",
}
physical_types = {"NORMAL", "FIGHTING", "FLYING", "POISON", "GROUND",
                  "ROCK", "BUG", "GHOST", "STEEL"}


def value(block: str, key: str, default: str = "0") -> str:
    found = re.search(rf"\.{key}\s*=\s*([A-Z0-9_\-]+)", block)
    return found.group(1) if found else default


rows: list[str] = []
for number, symbol in moves:
    if number > 354:
        continue
    match = re.search(rf"\[MOVE_{symbol}\]\s*=\s*\{{(.*?)\n\s*\}},", info, re.S)
    if not match:
        continue
    block = match.group(1)
    type_symbol = value(block, "type").removeprefix("TYPE_")
    effect = value(block, "effect").removeprefix("EFFECT_")
    target_symbol = value(block, "target", "MOVE_TARGET_SELECTED").removeprefix("MOVE_TARGET_")
    power = value(block, "power")
    # The project uses the Generation-IV physical/special split for every
    # battle.  Keep FireRed's pre-split effects and flags, but source each
    # category from Platinum so moves such as Weather Ball and Shadow Ball
    # resolve against the correct offensive/defensive stat.
    platinum = platinum_move_data.get(symbol)
    category = (platinum["class"].removeprefix("CLASS_").title()
                if platinum else ("Status" if power == "0" else
                ("Physical" if type_symbol in physical_types else "Special")))
    flags = {
        "contact": "true" if "FLAG_MAKES_CONTACT" in block else "false",
        "magic_coat": "true" if "FLAG_MAGIC_COAT_AFFECTED" in block else "false",
        "snatch": "true" if "FLAG_SNATCH_AFFECTED" in block else "false",
        "protect": "true" if "FLAG_PROTECT_AFFECTED" in block else "false",
        "mirror_move": "true" if "FLAG_MIRROR_MOVE_AFFECTED" in block else "false",
        "kings_rock": "true" if "FLAG_KINGS_ROCK_AFFECTED" in block else "false",
    }
    rows.append(
        f'    {{{number}, "{symbol.replace("_", " ")}", PokemonType::{types.get(type_symbol, "Normal")}, '
        f'{power}, {value(block, "accuracy")}, MoveCategory::{category}, {value(block, "pp")}, '
        f'{value(block, "priority")}, {value(block, "secondaryEffectChance")}, "{effect}", '
        f'MoveTarget::{targets[target_symbol]}, {flags["contact"]}, {flags["magic_coat"]}, '
        f'{flags["snatch"]}, {flags["protect"]}, {flags["mirror_move"]}, '
        f'{flags["kings_rock"]}}},')


platinum_targets = {
    "RANGE_SINGLE_TARGET": "Selected", "RANGE_ADJACENT_OPPONENTS": "Selected",
    "RANGE_RANDOM_OPPONENT": "Random", "RANGE_USER": "User",
    "RANGE_USER_OR_SELECTED": "UserOrSelected", "RANGE_ALL_ADJACENT": "Both",
    "RANGE_ALL_ADJACENT_OPPONENTS": "Selected", "RANGE_OPPONENTS_FIELD": "OpponentsField",
}
platinum_flags = {
    "MOVE_FLAG_MAKES_CONTACT": "contact", "MOVE_FLAG_CAN_MAGIC_COAT": "magic_coat",
    "MOVE_FLAG_CAN_SNATCH": "snatch", "MOVE_FLAG_CAN_PROTECT": "protect",
    "MOVE_FLAG_CAN_MIRROR_MOVE": "mirror_move", "MOVE_FLAG_KINGS_ROCK_AFFECTED": "kings_rock",
}
effect_aliases = {
    "RECOVER_HALF_DAMAGE_DEALT": "ABSORB", "LOWER_SP_DEF_HIT": "SPECIAL_DEFENSE_DOWN_HIT",
    "LOWER_SP_DEF_2_HIT": "SPECIAL_DEFENSE_DOWN_2_HIT",
    "RAISE_SP_ATK_HIT": "SPECIAL_ATTACK_UP_HIT",
    "RAISE_ALL_STATS_HIT": "ALL_STATS_UP_HIT", "HEAL_HALF_REMOVE_FLYING_TYPE": "RESTORE_HP",
    "HIT_TWICE": "DOUBLE_HIT", "RECOIL_THIRD": "DOUBLE_EDGE",
    "DOUBLE_POWER_WHEN_BELOW_HALF": "BRINE", "DOUBLE_POWER_IF_HIT": "AVALANCHE",
    "DOUBLE_POWER_IF_TARGET_HIT": "ASSURANCE", "BYPASS_ACCURACY": "ALWAYS_HIT",
    "PRIORITY_1": "QUICK_ATTACK", "LOWER_ATTACK_HIT": "ATTACK_DOWN_HIT",
    "LOWER_DEFENSE_HIT": "DEFENSE_DOWN_HIT", "LOWER_SPEED_HIT": "SPEED_DOWN_HIT",
    "HIGH_CRITICAL": "HIGH_CRITICAL", "FLINCH_HIT": "FLINCH_HIT",
    "MULTI_HIT": "MULTI_HIT", "RECHARGE_AFTER": "RECHARGE",
    "PARALYZE_HIT": "PARALYZE_HIT", "BURN_HIT": "BURN_HIT",
    "POISON_HIT": "POISON_HIT", "CONFUSE_HIT": "CONFUSE_HIT",
    "FREEZE_HIT": "FREEZE_HIT", "BIND_HIT": "TRAP",
    "RECOIL_HALF": "RECOIL_HALF", "RECOIL_BURN_HIT": "RECOIL_BURN_HIT",
    "USER_SP_ATK_DOWN_2": "OVERHEAT", "STATUS_SLEEP": "SLEEP",
    # Keep Gen-IV spellings aligned with the battle engine's established
    # effect vocabulary.  This makes the shared stat/status code apply to
    # new moves instead of reducing them to cosmetic metadata.
    "SP_ATK_UP_2": "SPECIAL_ATTACK_UP_2",
    "SP_ATK_DOWN_2_OPPOSITE_GENDER": "SPECIAL_ATTACK_DOWN_2_OPPOSITE_GENDER",
    "POWER_BASED_ON_LOW_SPEED": "GYRO_BALL",
    "DOUBLE_POWER_HEAL_SLEEP": "WAKE_UP_SLAP",
    "DOUBLE_POWER_IF_MOVING_SECOND": "PAYBACK",
    "HIGHER_POWER_WHEN_LOW_PP": "TRUMP_CARD",
    "INCREASE_POWER_WITH_MORE_HP": "WRING_OUT",
    "INCREASE_POWER_WITH_MORE_STAT_UP": "PUNISHMENT",
    "INCREASE_POWER_WITH_WEIGHT": "LOW_KICK",
    "DEF_SPD_DOWN_HIT": "CLOSE_COMBAT", "DEF_SPD_UP": "DEFEND_ORDER",
    "SHADOW_FORCE": "SEMI_INVULNERABLE", "NO_EFFECT": "SPLASH",
}
for number, symbol in moves:
    if number <= 354:
        continue
    data = platinum_move_data.get(symbol)
    if data is None:
        raise RuntimeError(f"Missing Platinum move data for {number} {symbol}")
    effect = data["effect"]["type"].removeprefix("BATTLE_EFFECT_")
    effect = effect_aliases.get(effect, effect)
    move_type = data["type"].removeprefix("TYPE_")
    category = data["class"].removeprefix("CLASS_").title()
    if category not in {"Physical", "Special", "Status"}:
        raise RuntimeError(f"Unknown category for {symbol}: {data['class']}")
    # Platinum's data file omits Me First's +1 execution priority because the
    # original battle script handles that range specially. The generated
    # table is consumed by the shared priority resolver, so keep the actual
    # move metadata explicit here instead of patching the generated artifact.
    priority = 1 if symbol == "ME_FIRST" else data["priority"]
    flag_values = {name: ("true" if flag in set(data.get("flags", [])) else "false")
                   for flag, name in platinum_flags.items()}
    target = platinum_targets.get(data["range"], "Selected")
    rows.append(
        f'    {{{number}, "{symbol.replace("_", " ")}", PokemonType::{types.get(move_type, "Normal")}, '
        f'{data["power"]}, {data["accuracy"]}, MoveCategory::{category}, {data["pp"]}, '
        f'{priority}, {data["effect"].get("chance", 0)}, "{effect}", '
        f'MoveTarget::{target}, {flag_values["contact"]}, {flag_values["magic_coat"]}, '
        f'{flag_values["snatch"]}, {flag_values["protect"]}, {flag_values["mirror_move"]}, '
        f'{flag_values["kings_rock"]}}},')

entries: list[tuple[int, int, int]] = []
for species_symbol, array_name in re.findall(
        r"\[SPECIES_([A-Z0-9_]+)\]\s*=\s*(s[A-Za-z0-9]+LevelUpLearnset)", pointers):
    species_id = species_ids.get(species_symbol)
    if not species_id:
        continue
    match = re.search(rf"static const u16 {array_name}\[\]\s*=\s*\{{(.*?)LEVEL_UP_END", learnsets, re.S)
    if not match:
        continue
    for level, move_symbol in re.findall(
        r"LEVEL_UP_MOVE\(\s*(\d+),\s*MOVE_([A-Z0-9_]+)\)", match.group(1)):
        if move_symbol in move_ids:
            entries.append((species_id, int(level), move_ids[move_symbol], 1))

platinum_species_symbols = [line.strip().removeprefix("SPECIES_")
                            for line in (PLATINUM / "generated/species.txt").read_text(
                                encoding="utf-8").splitlines() if line.strip()]
# Platinum also revised the level-up learnsets of older species. Keep those
# rows tagged as Gen-IV content so a pre-Sinnoh save continues using its
# FireRed moves, while a Sinnoh battle/level-up can access the complete table.
for species_id in range(1, 494):
    symbol = platinum_species_symbols[species_id]
    data = json.loads((PLATINUM / "res/pokemon" / symbol.lower() / "data.json").read_text(
        encoding="utf-8"))
    for level, move in data["learnset"]["by_level"]:
        move_symbol = str(move).removeprefix("MOVE_")
        if move_symbol in move_ids:
            entries.append((species_id, int(level), move_ids[move_symbol], 4))

out = ('// Generated from local FireRed and Platinum battle data.\n'
       'constexpr FullMoveData kFullMoves[] = {\n' + '\n'.join(rows) +
       '\n};\n\nconstexpr LearnsetEntry kLearnsets[] = {\n' +
       '\n'.join(f'    {{{species}, {level}, {move}, {generation}}},'
                 for species, level, move, generation in entries) + '\n};\n')
(ROOT / 'src/game/MoveDataGenerated.inc').write_text(out, encoding='utf-8', newline='\n')

description_rows = [
    f'    case {number}: return "{normalized_description(symbol)}";'
    for number, symbol in moves if number <= 354
]
for number, symbol in moves:
    if number <= 354:
        continue
    description = " ".join(str(line) for line in platinum_move_data[symbol]["description"])
    description = fire_red_ascii(description, f"Platinum move description {symbol}")
    description = description.replace("\r", " ").replace("\n", " ")
    description = re.sub(r"\s+", " ", description).strip()
    description_rows.append(
        f'    case {number}: return "{c_string(description.upper())}";')
description_out = (
    '// Generated from FireRed and Platinum original move descriptions.\n'
    'const char* generatedMoveDescription(uint16_t moveId) {\n'
    '  switch (moveId) {\n' + '\n'.join(description_rows) +
    '\n    default: return "NO DESCRIPTION AVAILABLE.";\n'
    '  }\n}\n'
)
(ROOT / 'src/game/MoveDescriptionsGenerated.inc').write_text(
    description_out, encoding='utf-8', newline='\n')
print(f"Generated {len(rows)} moves, {len(description_rows)} descriptions and {len(entries)} learnset entries")
