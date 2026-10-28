"""Generate FireRed/Platinum move and level-up learnset tables."""
from __future__ import annotations

import json
import re
from pathlib import Path

from text_normalization import fire_red_ascii
from unova_rom_data import expansion_text, level_moves, move_record


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

expansion_move_constants = expansion_text("include/constants/moves.h")
gen5_moves = sorted(
    (int(number), symbol)
    for symbol, number in re.findall(
        r"\bMOVE_([A-Z0-9_]+)\s*=\s*(\d+)", expansion_move_constants)
    if 468 <= int(number) <= 559
)
if not any(number == 468 for number, _ in gen5_moves):
    gen5_moves.insert(0, (468, "HONE_CLAWS"))
if len(gen5_moves) != 92 or gen5_moves[0][1] != "HONE_CLAWS" or gen5_moves[-1][1] != "FUSION_BOLT":
    raise RuntimeError("Unexpected expansion Generation-V move order")
for number, symbol in gen5_moves:
    moves.append((number, symbol))
    move_ids[symbol] = number

expansion_moves_info = expansion_text("src/data/moves_info.h")

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


platinum_move_data: dict[int, dict] = {}
for number, symbol in enumerate(platinum_move_symbols[:468]):
    if number == 0:
        continue
    path = PLATINUM / "res/moves" / symbol.lower() / "data.json"
    if not path.is_file():
        raise RuntimeError(f"Missing Platinum move data for {number} {symbol}")
    platinum_move_data[number] = json.loads(path.read_text(encoding="utf-8"))


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
    # Match by canonical move ID, not by the display name. A few Platinum
    # display names intentionally omit separators (ThunderPunch, SonicBoom,
    # AncientPower), so normalizing the prose name silently fell back to the
    # obsolete type-wide split for exactly those moves.
    platinum = platinum_move_data.get(number)
    if platinum is None:
        raise RuntimeError(f"Missing Platinum category for {number} {symbol}")
    category = platinum["class"].removeprefix("CLASS_").title()
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
    "RANGE_ALL_ADJACENT_OPPONENTS": "Selected",
    # Pokegochi battles are singles.  Field/side moves still need a
    # non-opponent target so accuracy, Protect and Substitute are not applied
    # to them as if they targeted the active foe.
    "RANGE_FIELD": "User", "RANGE_USER_SIDE": "User",
    "RANGE_USER_OR_ALLY": "User",
    "RANGE_SINGLE_TARGET_SPECIAL": "Selected",
    "RANGE_SINGLE_TARGET_ME_FIRST": "Selected",
    "RANGE_OPPONENT_SIDE": "OpponentsField",
    "RANGE_OPPONENTS_FIELD": "OpponentsField",
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
    "RAISE_ALL_STATS_HIT": "ALL_STATS_UP_HIT", "HEAL_HALF_REMOVE_FLYING_TYPE": "ROOST",
    "HIT_TWICE": "DOUBLE_HIT", "RECOIL_THIRD": "DOUBLE_EDGE",
    "DOUBLE_POWER_WHEN_BELOW_HALF": "BRINE", "DOUBLE_POWER_IF_HIT": "AVALANCHE",
    "DOUBLE_POWER_IF_TARGET_HIT": "ASSURANCE", "BYPASS_ACCURACY": "ALWAYS_HIT",
    "PRIORITY_1": "QUICK_ATTACK", "LOWER_ATTACK_HIT": "ATTACK_DOWN_HIT",
    "LOWER_DEFENSE_HIT": "DEFENSE_DOWN_HIT", "LOWER_SPEED_HIT": "SPEED_DOWN_HIT",
    "LOWER_SPEED_USER_HIT": "HAMMER_ARM",
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
    "SHADOW_FORCE": "SHADOW_FORCE", "NO_EFFECT": "SPLASH",
}
for number, symbol in moves:
    if number <= 354 or number > 467:
        continue
    data = platinum_move_data.get(number)
    if data is None:
        raise RuntimeError(f"Missing Platinum move data for {number} {symbol}")
    effect = data["effect"]["type"].removeprefix("BATTLE_EFFECT_")
    effect = effect_aliases.get(effect, effect)
    # Platinum reuses BATTLE_EFFECT_LOWER_SPEED_HIT for both target speed
    # drops and Hammer Arm's guaranteed user penalty; the move script
    # distinguishes Hammer Arm by ID.
    if symbol == "HAMMER_ARM":
        effect = "HAMMER_ARM"
    move_type = data["type"].removeprefix("TYPE_")
    category = data["class"].removeprefix("CLASS_").title()
    if category not in {"Physical", "Special", "Status"}:
        raise RuntimeError(f"Unknown category for {symbol}: {data['class']}")
    # Platinum's data file omits Me First's +1 execution priority because the
    # original battle script handles that range specially. The generated
    # table is consumed by the shared priority resolver, so keep the actual
    # move metadata explicit here instead of patching the generated artifact.
    priority = data["priority"]
    secondary_chance = data["effect"].get("chance", 0)
    # Chatter normally derives confusion odds from the recorded cry. The
    # device has no microphone recording mechanic, so use Platinum's maximum
    # 30% chance as the explicit deterministic adaptation.
    if symbol == "CHATTER":
        secondary_chance = 30
    flag_values = {name: ("true" if flag in set(data.get("flags", [])) else "false")
                   for flag, name in platinum_flags.items()}
    target = platinum_targets.get(data["range"], "Selected")
    rows.append(
        f'    {{{number}, "{symbol.replace("_", " ")}", PokemonType::{types.get(move_type, "Normal")}, '
        f'{data["power"]}, {data["accuracy"]}, MoveCategory::{category}, {data["pp"]}, '
        f'{priority}, {secondary_chance}, "{effect}", '
        f'MoveTarget::{target}, {flag_values["contact"]}, {flag_values["magic_coat"]}, '
        f'{flag_values["snatch"]}, {flag_values["protect"]}, {flag_values["mirror_move"]}, '
        f'{flag_values["kings_rock"]}}},')


def expansion_move_block(symbol: str) -> str:
    match = re.search(
        rf"\[MOVE_{symbol}\]\s*=\s*\{{(.*?)\n\s*\}},",
        expansion_moves_info, re.S)
    if not match:
        raise RuntimeError(f"Missing expansion move info for {symbol}")
    return match.group(1)


def expansion_field(block: str, name: str, default: str = "") -> str:
    match = re.search(rf"\.{name}\s*=\s*([A-Z0-9_\-]+)", block)
    return match.group(1) if match else default


gen5_targets = {
    "TARGET_SELECTED": "Selected", "TARGET_OPPONENT": "Selected",
    "TARGET_DEPENDS": "Depends", "TARGET_USER_OR_SELECTED": "UserOrSelected",
    "TARGET_RANDOM": "Random", "TARGET_BOTH": "Both",
    "TARGET_ALL_BATTLERS": "Both", "TARGET_FOES_AND_ALLY": "FoesAndAlly",
    "TARGET_ALL_OPPONENTS": "Selected", "TARGET_OPPONENTS_FIELD": "OpponentsField",
    "TARGET_FIELD": "User", "TARGET_USER_SIDE": "User", "TARGET_ALLY_SIDE": "User",
    "TARGET_USER": "User", "TARGET_ALLY": "User", "TARGET_ALLY_OR_USER": "User",
}
gen5_types = [
    "Normal", "Fighting", "Flying", "Poison", "Ground", "Rock", "Bug",
    "Ghost", "Steel", "Fire", "Water", "Grass", "Electric", "Psychic",
    "Ice", "Dragon", "Dark",
]
gen5_categories = ["Status", "Physical", "Special"]
gen5_effect_aliases = {
    "STAT_CHANGE": "STAT_CHANGE", "PROTECT": "PROTECT", "HIT": "HIT",
    "SPECIAL_DEFENSE_DOWN_2": "SPECIAL_DEFENSE_DOWN_2",
    "SPEED_DOWN_HIT": "SPEED_DOWN_HIT", "ATTACK_DOWN_HIT": "ATTACK_DOWN_HIT",
    "SPECIAL_ATTACK_DOWN_HIT": "SPECIAL_ATTACK_DOWN_HIT",
    "DEFENSE_DOWN_HIT": "DEFENSE_DOWN_HIT", "FLINCH_HIT": "FLINCH_HIT",
    "PARALYZE_HIT": "PARALYZE_HIT", "BURN_HIT": "BURN_HIT",
    "POISON_HIT": "POISON_HIT", "CONFUSE_HIT": "CONFUSE_HIT",
    "HIGH_CRITICAL": "HIGH_CRITICAL", "RECHARGE": "RECHARGE",
}

# Expansion's modern tables intentionally collapse several old mechanics into
# callbacks/additional-effect macros. Keep an explicit one-row contract for
# every Generation-V move whose Black/White behavior cannot be reconstructed
# from the compact fields above.
gen5_move_effect_overrides = {
    # Keep all 92 rows explicit. This is intentionally a mechanical contract
    # against Pokemon White rather than trusting modern expansion callbacks
    # whose names and decomposition change between upstream revisions.
    468: "HONE_CLAWS", 469: "WIDE_GUARD", 470: "GUARD_SPLIT",
    471: "POWER_SPLIT", 472: "WONDER_ROOM", 473: "PSYSHOCK",
    474: "DOUBLE_POWER_ON_ARG_STATUS", 475: "AUTOTOMIZE",
    476: "FOLLOW_ME", 477: "TELEKINESIS", 478: "MAGIC_ROOM",
    479: "SMACK_DOWN", 480: "HIGH_CRITICAL", 481: "HIT",
    482: "POISON_HIT", 483: "QUIVER_DANCE", 484: "HEAT_CRASH",
    485: "SYNCHRONOISE", 486: "ELECTRO_BALL", 487: "SOAK",
    488: "SPEED_UP_HIT", 489: "COIL", 490: "SPEED_DOWN_HIT",
    491: "SPECIAL_DEFENSE_DOWN_2_HIT", 492: "FOUL_PLAY",
    493: "SIMPLE_BEAM", 494: "ENTRAINMENT", 495: "AFTER_YOU",
    496: "ROUND", 497: "ECHOED_VOICE", 498: "CHIP_AWAY",
    499: "CLEAR_SMOG", 500: "STORED_POWER", 501: "QUICK_GUARD",
    502: "ALLY_SWITCH", 503: "BURN_HIT", 504: "SHELL_SMASH",
    505: "HEAL_PULSE", 506: "DOUBLE_POWER_ON_ARG_STATUS",
    507: "SKY_DROP", 508: "SHIFT_GEAR", 509: "FORCE_SWITCH",
    510: "INCINERATE", 511: "QUASH", 512: "ACROBATICS",
    513: "REFLECT_TYPE", 514: "RETALIATE", 515: "FINAL_GAMBIT",
    516: "BESTOW", 517: "BURN_HIT", 518: "PLEDGE",
    519: "PLEDGE", 520: "PLEDGE", 521: "SWITCH_HIT",
    522: "SPECIAL_ATTACK_DOWN_HIT", 523: "SPEED_DOWN_HIT",
    524: "HIGH_CRITICAL", 525: "FORCE_SWITCH", 526: "WORK_UP",
    527: "SPEED_DOWN_HIT", 528: "RECOIL", 529: "HIGH_CRITICAL",
    530: "DOUBLE_HIT", 531: "FLINCH_HIT", 532: "ABSORB",
    533: "SACRED_SWORD", 534: "DEFENSE_DOWN_HIT",
    535: "HEAT_CRASH", 536: "ACCURACY_DOWN_HIT",
    537: "FLINCH_MINIMIZE_HIT", 538: "COTTON_GUARD",
    539: "ACCURACY_DOWN_HIT", 540: "PSYSHOCK", 541: "MULTI_HIT",
    542: "CONFUSE_HIT", 543: "RECOIL", 544: "DOUBLE_HIT",
    545: "BURN_HIT", 546: "CHANGE_TYPE_ON_ITEM", 547: "RELIC_SONG",
    548: "PSYSHOCK", 549: "SPEED_DOWN_HIT", 550: "PARALYZE_HIT",
    551: "BURN_HIT", 552: "SPECIAL_ATTACK_UP_HIT",
    553: "FREEZE_SHOCK", 554: "ICE_BURN",
    555: "SPECIAL_ATTACK_DOWN_HIT", 556: "FLINCH_HIT",
    557: "V_CREATE", 558: "FUSION_COMBO", 559: "FUSION_COMBO",
}


def gen5_effect(block: str) -> str:
    effect = expansion_field(block, "effect", "EFFECT_HIT").removeprefix("EFFECT_")
    # Modern expansion data expresses many Generation-V secondary effects in
    # an additionalEffects block. Convert the single-effect cases back to the
    # compact vocabulary consumed by Pokegochi's one-on-one resolver.
    additional = re.search(r"\.additionalEffects\s*=\s*ADDITIONAL_EFFECTS\(\{(.*?)\}\)",
                           block, re.S)
    if effect == "HIT" and additional:
        extra = additional.group(1)
        move_effect = expansion_field(extra, "moveEffect", "").removeprefix("MOVE_EFFECT_")
        direct = {
            "BURN": "BURN_HIT", "PARALYSIS": "PARALYZE_HIT",
            "POISON": "POISON_HIT", "CONFUSION": "CONFUSE_HIT",
            "FLINCH": "FLINCH_HIT",
        }
        if move_effect in direct:
            return direct[move_effect]
        if move_effect == "STAT_MINUS":
            stages = {
                "attack": "ATTACK_DOWN", "defense": "DEFENSE_DOWN",
                "speed": "SPEED_DOWN", "spAttack": "SPECIAL_ATTACK_DOWN",
                "spDefense": "SPECIAL_DEFENSE_DOWN", "accuracy": "ACCURACY_DOWN",
            }
            for field_name, label in stages.items():
                amount = expansion_field(extra, field_name, "")
                if amount:
                    return f"{label}_{'2_' if amount == '2' else ''}HIT"
        if move_effect == "STAT_PLUS":
            stages = {
                "attack": "ATTACK_UP", "defense": "DEFENSE_UP",
                "speed": "SPEED_UP", "spAttack": "SPECIAL_ATTACK_UP",
                "spDefense": "SPECIAL_DEFENSE_UP", "accuracy": "ACCURACY_UP",
            }
            for field_name, label in stages.items():
                amount = expansion_field(extra, field_name, "")
                if amount:
                    return f"{label}_{'2_' if amount == '2' else ''}HIT"
    return gen5_effect_aliases.get(effect, effect)


for number, symbol in gen5_moves:
    block = expansion_move_block(symbol)
    record = move_record(number)
    target_symbol = expansion_field(block, "target", "TARGET_SELECTED")
    if target_symbol not in gen5_targets:
        raise RuntimeError(f"Unknown Gen-V target {target_symbol} for {symbol}")
    priority_raw = expansion_field(block, "priority", "0")
    # The ROM stores the authoritative signed priority at byte 6 and avoids
    # evaluating expansion build-time conditionals such as Magic Room.
    priority = int.from_bytes(record[6:7], "little", signed=True)
    effect = gen5_move_effect_overrides.get(number, gen5_effect(block))
    makes_contact = "true" if re.search(r"\.makesContact\s*=\s*TRUE", block) else "false"
    magic_coat = "true" if re.search(r"\.magicCoatAffected\s*=\s*TRUE", block) else "false"
    snatch = "true" if re.search(r"\.snatchAffected\s*=\s*TRUE", block) else "false"
    target = gen5_targets[target_symbol]
    protect = "false" if re.search(r"\.ignoresProtect\s*=\s*TRUE", block) else (
        "true" if target in {"Selected", "Random", "FoesAndAlly", "Both"} else "false")
    mirror = "false" if re.search(r"\.mirrorMoveBanned\s*=\s*TRUE", block) else "true"
    kings_rock = "true" if record[3] else "false"
    # White stores the original direct secondary at byte 10. Callback-based
    # stat effects occupy up to three later effect slots, whose chances are
    # bytes 27..29 (e.g. Fiery Dance=50, V-create=100/100/100).
    effect_chance = max(record[10], record[27], record[28], record[29])
    rows.append(
        f'    {{{number}, "{symbol.replace("_", " ")}", PokemonType::{gen5_types[record[0]]}, '
        f'{record[3]}, {record[4]}, MoveCategory::{gen5_categories[record[2]]}, {record[5]}, '
        f'{priority}, {effect_chance}, "{effect}", MoveTarget::{target}, '
        f'{makes_contact}, {magic_coat}, {snatch}, {protect}, {mirror}, {kings_rock}}},')

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

# White supplies the complete Generation-V level-up table for every National
# species. Tag the rows as generation 5 so old saves gain them only when
# Unova is unlocked, matching the retroactive Sinnoh behavior.
for species_id in range(1, 650):
    for level, move in level_moves(species_id):
        # Older species already retain their FireRed and Platinum pools. The
        # retroactive unlock adds only genuinely new Gen-V moves, avoiding a
        # second copy of thousands of unchanged historical rows in flash.
        if 0 < move <= 559 and (species_id >= 494 or move >= 468):
            entries.append((species_id, int(level), int(move), 5))

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
    if number <= 354 or number > 467:
        continue
    description = " ".join(str(line) for line in platinum_move_data[number]["description"])
    description = fire_red_ascii(description, f"Platinum move description {symbol}")
    description = description.replace("\r", " ").replace("\n", " ")
    description = re.sub(r"\s+", " ", description).strip()
    description_rows.append(
        f'    case {number}: return "{c_string(description.upper())}";')

for number, symbol in gen5_moves:
    block = expansion_move_block(symbol)
    description_match = re.search(
        r"\.description\s*=\s*COMPOUND_STRING\((.*?)\)\s*,", block, re.S)
    if description_match:
        pieces = re.findall(r'"([^"]*)"', description_match.group(1))
        description = " ".join(piece.replace("\\n", " ") for piece in pieces)
    else:
        description = "GENERATION V MOVE."
    description = fire_red_ascii(re.sub(r"\s+", " ", description).strip(),
                                 f"Gen-V move description {symbol}")
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
