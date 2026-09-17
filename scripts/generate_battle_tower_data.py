"""Generate a compact Emerald Battle Tower catalog for Pokegochi.

The source of truth is the local pokeemerald decomp: all 300 facility
trainers, their legal mon pools and all 882 authored moveset/item/EV/nature
templates are retained.  Runtime code still chooses only three distinct
Pokemon, matching Pokegochi's party limit.
"""
from __future__ import annotations

import re
from pathlib import Path
from text_normalization import fire_red_ascii

ROOT = Path(__file__).resolve().parent.parent
EMERALD = ROOT / ".downloads" / "pokeemerald-tree" / "pokeemerald-master"
OUT = ROOT / "src" / "game" / "BattleTowerDataGenerated.inc"


def read(path: str) -> str:
    target = EMERALD / path
    if not target.exists():
        raise RuntimeError(f"Missing Emerald source: {target}")
    return target.read_text(encoding="utf-8")


def quote(value: str) -> str:
    value = fire_red_ascii(value, "generated Battle Tower text")
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def clean_text(value: str) -> str:
    value = value.replace("{PKMN}", "POKEMON")
    value = (value.replace("{PKMN}", "POKEMON").replace("POKÃ©MON", "POKEMON")
             .replace("POKÃƒÂ©MON", "POKEMON").replace("â™‚", " M")
             .replace("â™€", " F"))
    return re.sub(r"\s+", " ", value).strip()


def enum_constants(source: str, prefix: str) -> dict[str, int]:
    return {name: int(value) for name, value in re.findall(
        rf"#define\s+{re.escape(prefix)}([A-Z0-9_]+)\s+(\d+)", source)}


# Emerald internal species IDs contain old Unown slots.  Facility data uses
# symbols, so convert through NATIONAL_DEX_* instead of copying internal IDs.
pokedex = read("include/constants/pokedex.h")
national_symbols = re.findall(r"^\s*NATIONAL_DEX_([A-Z0-9_]+),", pokedex, re.M)
species_ids = {symbol: number for number, symbol in enumerate(national_symbols)
               if 1 <= number <= 386}

move_ids = enum_constants(read("include/constants/moves.h"), "MOVE_")
frontier_mon_ids = enum_constants(read("include/constants/battle_frontier_mons.h"),
                                  "FRONTIER_MON_")
trainer_ids = enum_constants(read("include/constants/battle_frontier_trainers.h"),
                              "FRONTIER_TRAINER_")

nature_order = [
    "HARDY", "LONELY", "BRAVE", "ADAMANT", "NAUGHTY",
    "BOLD", "DOCILE", "RELAXED", "IMPISH", "LAX",
    "TIMID", "HASTY", "SERIOUS", "JOLLY", "NAIVE",
    "MODEST", "MILD", "QUIET", "BASHFUL", "RASH",
    "CALM", "GENTLE", "SASSY", "CAREFUL", "QUIRKY",
]
nature_ids = {name: index for index, name in enumerate(nature_order)}

# Existing player items keep their persistent numeric values.  The final
# entries are Battle-Tower-only HeldItem values appended after MegaStone.
item_map = {
    "NONE": "None", "KINGS_ROCK": "KingsRock", "SITRUS_BERRY": "SitrusBerry",
    "ORAN_BERRY": "OranBerry", "CHESTO_BERRY": "ChestoBerry",
    "HARD_STONE": "HardStone", "FOCUS_BAND": "FocusBand",
    "PERSIM_BERRY": "PersimBerry", "MIRACLE_SEED": "MiracleSeed",
    "BERRY_JUICE": "BerryJuice", "MACHO_BRACE": "MachoBrace",
    "SILVER_POWDER": "SilverPowder", "CHERI_BERRY": "CheriBerry",
    "BLACK_GLASSES": "BlackGlasses", "BLACK_BELT": "BlackBelt",
    "SOUL_DEW": "SoulDew", "CHOICE_BAND": "ChoiceBand", "MAGNET": "Magnet",
    "SILK_SCARF": "SilkScarf", "WHITE_HERB": "WhiteHerb",
    "DEEP_SEA_SCALE": "DeepSeaScale", "DEEP_SEA_TOOTH": "DeepSeaTooth",
    "MYSTIC_WATER": "MysticWater", "SHARP_BEAK": "SharpBeak",
    "QUICK_CLAW": "QuickClaw", "LEFTOVERS": "Leftovers",
    "RAWST_BERRY": "RawstBerry", "LIGHT_BALL": "LightBall",
    "POISON_BARB": "PoisonBarb", "NEVER_MELT_ICE": "NeverMeltIce",
    "ASPEAR_BERRY": "AspearBerry", "SPELL_TAG": "SpellTag",
    "BRIGHT_POWDER": "BrightPowder", "LEPPA_BERRY": "LeppaBerry",
    "SCOPE_LENS": "ScopeLens", "TWISTED_SPOON": "TwistedSpoon",
    "METAL_COAT": "MetalCoat", "MENTAL_HERB": "MentalHerb",
    "CHARCOAL": "Charcoal", "PECHA_BERRY": "PechaBerry",
    "SOFT_SAND": "SoftSand", "LUM_BERRY": "LumBerry",
    "DRAGON_SCALE": "DragonScale", "DRAGON_FANG": "DragonFang",
    "IAPAPA_BERRY": "IapapaBerry", "WIKI_BERRY": "WikiBerry",
    "SEA_INCENSE": "SeaIncense", "SHELL_BELL": "ShellBell",
    "SALAC_BERRY": "SalacBerry", "LANSAT_BERRY": "LansatBerry",
    "APICOT_BERRY": "ApicotBerry", "STARF_BERRY": "StarfBerry",
    "LIECHI_BERRY": "LiechiBerry", "STICK": "Stick",
    "LAX_INCENSE": "LaxIncense", "AGUAV_BERRY": "AguavBerry",
    "FIGY_BERRY": "FigyBerry", "THICK_CLUB": "ThickClub",
    "MAGO_BERRY": "MagoBerry", "METAL_POWDER": "MetalPowder",
    "PETAYA_BERRY": "PetayaBerry", "LUCKY_PUNCH": "LuckyPunch",
    "GANLON_BERRY": "GanlonBerry",
}


def parse_or_mask(raw: str, prefix: str) -> int:
    values = {
        "HP": 1 << 0, "ATTACK": 1 << 1, "DEFENSE": 1 << 2,
        "SPEED": 1 << 3, "SP_ATTACK": 1 << 4, "SP_DEFENSE": 1 << 5,
    }
    result = 0
    for symbol in re.findall(rf"{re.escape(prefix)}([A-Z0-9_]+)", raw):
        result |= values[symbol]
    return result


mons_source = read("src/data/battle_frontier/battle_frontier_mons.h")
mon_rows: list[tuple[int, list[int], str, int, int] | None] = [None] * len(frontier_mon_ids)
for symbol, body in re.findall(
        r"\[FRONTIER_MON_([A-Z0-9_]+)\]\s*=\s*\{(.*?)\n\s*\},?", mons_source, re.S):
    index = frontier_mon_ids.get(symbol)
    species_match = re.search(r"\.species\s*=\s*SPECIES_([A-Z0-9_]+)", body)
    moves_match = re.search(r"\.moves\s*=\s*\{([^}]+)\}", body)
    item_match = re.search(r"\.itemTableId\s*=\s*BATTLE_FRONTIER_ITEM_([A-Z0-9_]+)", body)
    nature_match = re.search(r"\.nature\s*=\s*NATURE_([A-Z0-9_]+)", body)
    ev_match = re.search(r"\.evSpread\s*=\s*([^,\n]+(?:\|[^,\n]+)*)", body)
    if index is None or not all((species_match, moves_match, item_match, nature_match, ev_match)):
        continue
    species = species_ids.get(species_match.group(1))
    moves = [move_ids.get(name, 0) for name in re.findall(r"MOVE_([A-Z0-9_]+)", moves_match.group(1))]
    moves = (moves + [0, 0, 0, 0])[:4]
    item = item_map.get(item_match.group(1))
    nature = nature_ids.get(nature_match.group(1))
    if not species or item is None or nature is None:
        raise RuntimeError(f"Unmapped facility mon {symbol}: species/item/nature")
    mon_rows[index] = (species, moves, item, parse_or_mask(ev_match.group(1), "F_EV_SPREAD_"), nature)

if any(row is None for row in mon_rows):
    missing = [name for name, index in frontier_mon_ids.items() if index < len(mon_rows) and mon_rows[index] is None]
    raise RuntimeError(f"Missing {len(missing)} facility mon templates: {missing[:8]}")


# Expand the macro-composed trainer pools into one packed uint16 table.
pool_source = read("src/data/battle_frontier/battle_frontier_trainer_mons.h")
macros: dict[str, list[str]] = {}
lines = pool_source.splitlines()
i = 0
while i < len(lines):
    match = re.match(r"\s*#define\s+([A-Z0-9_]+)\s*(.*)", lines[i])
    if not match:
        i += 1
        continue
    name, tail = match.groups()
    pieces = [tail]
    while lines[i].rstrip().endswith("\\") and i + 1 < len(lines):
        i += 1
        pieces.append(lines[i])
    macros[name] = re.findall(r"FRONTIER_(?:MON|MONS)_[A-Z0-9_]+", " ".join(pieces))
    i += 1


def expand_token(token: str, stack: tuple[str, ...] = ()) -> list[int]:
    if token.startswith("FRONTIER_MON_") and token not in macros:
        symbol = token.removeprefix("FRONTIER_MON_")
        if symbol not in frontier_mon_ids:
            raise RuntimeError(f"Unknown frontier mon {token}")
        return [frontier_mon_ids[symbol]]
    if token in stack:
        raise RuntimeError(f"Recursive trainer pool macro: {token}")
    if token not in macros:
        raise RuntimeError(f"Unknown trainer pool macro: {token}")
    result: list[int] = []
    for child in macros[token]:
        result.extend(expand_token(child, stack + (token,)))
    return result


pools: dict[str, list[int]] = {}
for name, body in re.findall(r"const\s+u16\s+(gBattleFrontierTrainerMons_[A-Za-z0-9_]+)\[\]\s*=\s*\{(.*?)\n\};",
                             pool_source, re.S):
    entries: list[int] = []
    for token in re.findall(r"FRONTIER_(?:MON|MONS)_[A-Z0-9_]+", body):
        entries.extend(expand_token(token))
    pools[name] = entries


# Convert facility classes to the exact Emerald trainer class and front pic.
lookup_source = read("src/data/pokemon/trainer_class_lookups.h")
pic_by_facility = dict(re.findall(
    r"\[FACILITY_CLASS_([A-Z0-9_]+)\]\s*=\s*TRAINER_PIC_([A-Z0-9_]+)", lookup_source))
class_by_facility = dict(re.findall(
    r"\[FACILITY_CLASS_([A-Z0-9_]+)\]\s*=\s*TRAINER_CLASS_([A-Z0-9_]+)", lookup_source))
class_source = read("src/data/text/trainer_class_names.h")
class_names = {symbol: clean_text(name) for symbol, name in re.findall(
    r"\[TRAINER_CLASS_([A-Z0-9_]+)\]\s*=\s*_\(\"([^\"]+)\"\)", class_source)}


def easy_chat_word(token: str) -> str:
    if token == "EC_EMPTY_WORD":
        return ""
    special = {"EC_WORD_EXCL": "!", "EC_WORD_EXCL_EXCL": "!!",
               "EC_WORD_QUES": "?", "EC_WORD_ELLIPSIS": "..."}
    if token in special:
        return special[token]
    call = re.match(r"EC_(?:MOVE2?|POKEMON_NATIONAL)\(([A-Z0-9_]+)\)", token)
    raw = call.group(1) if call else token.removeprefix("EC_WORD_")
    replacements = {
        "AREN_T": "AREN'T", "CAN_T": "CAN'T", "COULDN_T": "COULDN'T",
        "DIDN_T": "DIDN'T", "DOESN_T": "DOESN'T", "DON_T": "DON'T",
        "HASN_T": "HASN'T", "ISN_T": "ISN'T", "IT_S": "IT'S",
        "THAT_S": "THAT'S", "THEY_RE": "THEY'RE", "WASN_T": "WASN'T",
        "WEREN_T": "WEREN'T", "WON_T": "WON'T", "WOULDN_T": "WOULDN'T",
        "YOU_RE": "YOU'RE", "I_VE": "I'VE", "CHILD_S_PLAY": "CHILD'S PLAY",
    }
    return replacements.get(raw, raw.replace("_", " "))


def speech_lines(raw: str) -> tuple[str, str]:
    tokens = re.findall(r"EC_[A-Z0-9_]+(?:\([A-Z0-9_]+\))?", raw)
    words = [easy_chat_word(token) for token in tokens]
    words = [word for word in words if word]
    first: list[str] = []
    second: list[str] = []
    for word in words:
        target = first if len(" ".join(first + [word])) <= 28 else second
        if len(" ".join(target + [word])) <= 30:
            target.append(word)
    return " ".join(first), " ".join(second)


trainer_source = read("src/data/battle_frontier/battle_frontier_trainers.h")
trainer_rows: list[tuple[str, str, str, str, str, str] | None] = [None] * len(trainer_ids)
for symbol, body in re.findall(
        r"\[FRONTIER_TRAINER_([A-Z0-9_]+)\]\s*=\s*\{(.*?)\n\s*\},?", trainer_source, re.S):
    index = trainer_ids.get(symbol)
    facility = re.search(r"\.facilityClass\s*=\s*FACILITY_CLASS_([A-Z0-9_]+)", body)
    name = re.search(r"\.trainerName\s*=\s*_\(\"([^\"]+)\"\)", body)
    speech = re.search(r"\.speechBefore\s*=\s*\{([^}]+)\}", body)
    pool = re.search(r"\.monSet\s*=\s*(gBattleFrontierTrainerMons_[A-Za-z0-9_]+)", body)
    if index is None or not all((facility, name, speech, pool)):
        continue
    facility_symbol = facility.group(1)
    pic = pic_by_facility.get(facility_symbol)
    trainer_class = class_by_facility.get(facility_symbol)
    if not pic or not trainer_class or pool.group(1) not in pools:
        raise RuntimeError(f"Unmapped Battle Tower trainer {symbol}")
    line1, line2 = speech_lines(speech.group(1))
    trainer_rows[index] = (class_names.get(trainer_class, trainer_class.replace("_", " ")),
                           clean_text(name.group(1)), "emerald_" + pic.lower(),
                           line1 or "LET'S BATTLE!", line2, pool.group(1))

if any(row is None for row in trainer_rows):
    missing = [name for name, index in trainer_ids.items() if index < len(trainer_rows) and trainer_rows[index] is None]
    raise RuntimeError(f"Missing {len(missing)} Battle Tower trainers: {missing[:8]}")

packed_pools: list[int] = []
trainer_output = []
for trainer_id, row in enumerate(trainer_rows):
    assert row is not None
    trainer_class, name, asset, line1, line2, pool_name = row
    pool = pools[pool_name]
    if len(pool) < 3:
        raise RuntimeError(f"Trainer {trainer_id} has only {len(pool)} templates")
    offset = len(packed_pools)
    packed_pools.extend(pool)
    iv = 3 if trainer_id < 100 else min(31, 6 + ((trainer_id - 100) // 20) * 3)
    trainer_output.append((trainer_class, name, asset, line1, line2, offset, len(pool), iv))

out = [
    "// Generated by scripts/generate_battle_tower_data.py from pokeemerald.",
    "static constexpr BattleTowerMonTemplate kBattleTowerMons[] = {",
]
for row in mon_rows:
    assert row is not None
    species, moves, item, ev_mask, nature = row
    out.append(f"  {{{species}, {{{moves[0]}, {moves[1]}, {moves[2]}, {moves[3]}}}, "
               f"HeldItem::{item}, 0x{ev_mask:02X}, static_cast<PokemonNature>({nature})}},")
out.append("};")
out.append("static constexpr uint16_t kBattleTowerPoolEntries[] = {")
for start in range(0, len(packed_pools), 20):
    out.append("  " + ", ".join(str(value) for value in packed_pools[start:start + 20]) + ",")
out.append("};")
out.append("static constexpr BattleTowerTrainerDefinition kBattleTowerTrainers[] = {")
for trainer_class, name, asset, line1, line2, offset, count, iv in trainer_output:
    out.append(f"  {{{quote(trainer_class)}, {quote(name)}, {quote(asset)}, {quote(line1)}, "
               f"{quote(line2)}, {offset}, {count}, {iv}}},")
out.append("};")
OUT.write_text("\n".join(out) + "\n", encoding="utf-8")
print(f"Generated {len(mon_rows)} sets, {len(trainer_rows)} trainers and "
      f"{len(packed_pools)} packed trainer-set references")
