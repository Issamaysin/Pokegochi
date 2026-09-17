"""Generate compact Generation-I gameplay tables from the local pokefirered decomp."""

from __future__ import annotations

import re
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DECOMP = ROOT / ".downloads" / "pokefirered-tree" / "pokefirered-master"
OUT = ROOT / "src" / "game" / "PokemonDataGenerated.inc"

constants = (DECOMP / "include/constants/species.h").read_text(encoding="utf-8")
pokedex_constants = (DECOMP / "include/constants/pokedex.h").read_text(encoding="utf-8")
species_source = (DECOMP / "src/data/pokemon/species_info.h").read_text(encoding="utf-8")
ability_constants = (DECOMP / "include/constants/abilities.h").read_text(encoding="utf-8")
ability_text_source = (DECOMP / "src/data/text/abilities.h").read_text(encoding="utf-8")
evolution_source = (DECOMP / "src/data/pokemon/evolution.h").read_text(encoding="utf-8")

internal_ids = {symbol: int(number) for symbol, number in re.findall(r"#define SPECIES_([A-Z0-9_]+)\s+(\d+)", constants)}
national_symbols = re.findall(r"^\s*NATIONAL_DEX_([A-Z0-9_]+),", pokedex_constants, re.M)
species = [(number, symbol) for number, symbol in enumerate(national_symbols)
           if 1 <= number <= 386 and symbol in internal_ids]
symbol_to_id = {symbol: number for number, symbol in species}

type_map = {
    "NORMAL": "Normal", "FIGHTING": "Fighting", "FLYING": "Flying",
    "POISON": "Poison", "GROUND": "Ground", "ROCK": "Rock", "BUG": "Bug",
    "GHOST": "Ghost", "STEEL": "Steel", "MYSTERY": "Normal", "FIRE": "Fire",
    "WATER": "Water", "GRASS": "Grass", "ELECTRIC": "Electric",
    "PSYCHIC": "Psychic", "ICE": "Ice", "DRAGON": "Dragon", "DARK": "Dark",
}
growth_map = {
    "MEDIUM_FAST": "MediumFast", "ERRATIC": "Erratic", "FLUCTUATING": "Fluctuating",
    "MEDIUM_SLOW": "MediumSlow", "FAST": "Fast", "SLOW": "Slow",
}
display_names = {
    "NIDORAN_F": "NIDORAN F", "NIDORAN_M": "NIDORAN M", "MR_MIME": "MR. MIME",
    "FARFETCHD": "FARFETCH'D",
}

def field(block: str, name: str) -> str:
    match = re.search(rf"\.{name}\s*=\s*([A-Z0-9_]+)", block)
    if not match:
        raise ValueError(f"Missing {name}")
    return match.group(1)

def gender_ratio(block: str) -> int:
    match = re.search(r"\.genderRatio\s*=\s*([^,\n]+)", block)
    if not match:
        raise ValueError("Missing genderRatio")
    raw = match.group(1).strip()
    if raw == "MON_MALE":
        return 0
    if raw == "MON_FEMALE":
        return 254
    if raw == "MON_GENDERLESS":
        return 255
    percent = re.fullmatch(r"PERCENT_FEMALE\(([0-9.]+)\)", raw)
    if percent:
        return min(254, int(float(percent.group(1)) * 255 / 100))
    raise ValueError(f"Unknown genderRatio {raw}")

rows = []
for number, symbol in species:
    match = re.search(rf"\[SPECIES_{symbol}\]\s*=\s*\{{(.*?)\n\s*\}},", species_source, re.S)
    if not match:
        raise ValueError(f"Missing species block {symbol}")
    block = match.group(1)
    types = re.search(r"\.types\s*=\s*\{TYPE_([A-Z]+),\s*TYPE_([A-Z]+)\}", block)
    if not types:
        raise ValueError(f"Missing types {symbol}")
    name = display_names.get(symbol, symbol.replace("_", " "))
    abilities = re.search(r"\.abilities\s*=\s*\{ABILITY_([A-Z0-9_]+),\s*ABILITY_([A-Z0-9_]+)\}", block)
    if not abilities: raise ValueError(f"Missing abilities {symbol}")
    ability_ids = {s: int(n) for s, n in re.findall(r"#define ABILITY_([A-Z0-9_]+)\s+(\d+)", ability_constants)}
    rows.append(
        f'    {{{number}, "{name}", PokemonType::{type_map[types.group(1)]}, '
        f'PokemonType::{type_map[types.group(2)]}, {field(block,"baseHP")}, '
        f'{field(block,"baseAttack")}, {field(block,"baseDefense")}, {field(block,"baseSpAttack")}, {field(block,"baseSpDefense")}, {field(block,"baseSpeed")}, '
        f'{field(block,"catchRate")}, {field(block,"expYield")}, GrowthRate::{growth_map[field(block,"growthRate").removeprefix("GROWTH_")]}, {ability_ids[abilities.group(1)]}, {ability_ids[abilities.group(2)]}, '
        f'{field(block,"evYield_HP")}, {field(block,"evYield_Attack")}, {field(block,"evYield_Defense")}, '
        f'{field(block,"evYield_SpAttack")}, {field(block,"evYield_SpDefense")}, {field(block,"evYield_Speed")}, '
        f'{gender_ratio(block)}}},'
    )

# Keep original level evolutions. Convert stones/trades/friendship to deterministic
# levels across the National Dex. Every branch is retained for the in-game chooser.
evolutions = []
# An entry's outer initializer contains nested braces, so a non-greedy regular
# expression would stop after its first destination. Scan the balanced braces
# instead and retain every possible target (Eevee, Wurmple, etc.).
evolution_blocks = []
for match in re.finditer(r"\[SPECIES_([A-Z0-9_]+)\]\s*=\s*", evolution_source):
    start = evolution_source.find("{", match.end())
    if start < 0:
        continue
    depth = 0
    end = start
    while end < len(evolution_source):
        if evolution_source[end] == "{":
            depth += 1
        elif evolution_source[end] == "}":
            depth -= 1
            if depth == 0:
                break
        end += 1
    if depth == 0:
        evolution_blocks.append((match.group(1), evolution_source[start + 1:end]))

for source_symbol, entries in evolution_blocks:
    source_id = symbol_to_id.get(source_symbol)
    if not source_id:
        continue
    candidates = re.findall(r"\{(EVO_[A-Z_]+),\s*([^,]+),\s*SPECIES_([A-Z0-9_]+)\}", entries)
    for method, parameter, target_symbol in candidates:
        target_id = symbol_to_id.get(target_symbol)
        if not target_id:
            continue
        if method == "EVO_LEVEL" or method.startswith("EVO_LEVEL_"):
            level = int(parameter)
        elif method.startswith("EVO_ITEM"):
            level = 36
        elif method.startswith("EVO_TRADE"):
            level = 40
        elif method.startswith("EVO_FRIENDSHIP"):
            level = 30
        else:
            level = 36
        evolutions.append((source_id, target_id, level))

content = "// Generated by scripts/generate_pokemon_data.py from local pokefirered data.\n"
content += "constexpr SpeciesData kSpecies[] = {\n" + "\n".join(rows) + "\n};\n\n"
content += "constexpr EvolutionData kEvolutions[] = {\n"
content += "\n".join(f"    {{{a}, {b}, {level}}}," for a, b, level in evolutions)
content += "\n};\n"
content += "\nconstexpr AbilityData kAbilities[] = {\n"
description_values = {
    variable: text.replace("POKéMON", "POKEMON").replace("POKÃ©MON", "POKEMON")
                  .replace("“", '"').replace("”", '"').replace("â€œ", '"').replace("â€", '"')
    for variable, text in re.findall(
        r'static const u8\s+(\w+Description)\[\]\s*=\s*_\("([^"]*)"\);', ability_text_source
    )
}
description_symbols = {
    symbol: variable
    for symbol, variable in re.findall(
        r'\[ABILITY_([A-Z0-9_]+)\]\s*=\s*(\w+Description)', ability_text_source
    )
}
for symbol, number in sorted(ability_ids.items(), key=lambda item: item[1]):
    description = description_values.get(description_symbols.get(symbol, ""), "No special ability.")
    description = description.replace('\\', '\\\\').replace('"', '\\"')
    content += f'    {{{number}, "{symbol.replace("_", " ")}", "{description.upper()}"}},\n'
content += "};\n"

wild_data = json.loads((DECOMP / "src/data/wild_encounters.json").read_text(encoding="utf-8"))
wild_levels: dict[int, int] = {}
wild_counts: dict[int, int] = {}
for group in wild_data["wild_encounter_groups"]:
    for encounter in group["encounters"]:
        for field_name, field_value in encounter.items():
            if not field_name.endswith("_mons") or not isinstance(field_value, dict):
                continue
            for mon in field_value.get("mons", []):
                symbol = mon["species"].removeprefix("SPECIES_")
                species_id = symbol_to_id.get(symbol)
                if not species_id:
                    continue
                wild_levels[species_id] = min(wild_levels.get(species_id, 100), int(mon["min_level"]))
                wild_counts[species_id] = wild_counts.get(species_id, 0) + 1

# Johto progression comes from the user-matched Pokemon Gold decomp. Gold's
# grass/water tables use "level, species" rows; fishing is deliberately left
# out because its row order differs and every fishing species is represented
# elsewhere by the Pokegochi fallback table.
gold_root = ROOT / ".downloads/pokegold-tree/pokegold-master/data/wild"
for filename in ("johto_grass.asm", "johto_water.asm", "bug_contest_mons.asm", "swarm_grass.asm", "swarm_water.asm"):
    text = (gold_root / filename).read_text(encoding="utf-8")
    for level, symbol in re.findall(r"\bdb\s+(\d+),\s+([A-Z0-9_]+)", text):
        species_id = symbol_to_id.get(symbol)
        if not species_id or not 152 <= species_id <= 251: continue
        wild_levels[species_id] = min(wild_levels.get(species_id, 100), int(level))
        wild_counts[species_id] = wild_counts.get(species_id, 0) + 1

# Hoenn progression uses Emerald's generated wild-encounter JSON.
emerald_wild = json.loads((ROOT / ".downloads/pokeemerald-tree/pokeemerald-master/src/data/wild_encounters.json").read_text(encoding="utf-8"))
for group in emerald_wild["wild_encounter_groups"]:
    for encounter in group["encounters"]:
        for field_name, field_value in encounter.items():
            if not field_name.endswith("_mons") or not isinstance(field_value, dict): continue
            for mon in field_value.get("mons", []):
                symbol = mon["species"].removeprefix("SPECIES_")
                species_id = symbol_to_id.get(symbol)
                if not species_id or species_id < 252: continue
                wild_levels[species_id] = min(wild_levels.get(species_id, 100), int(mon["min_level"]))
                wild_counts[species_id] = wild_counts.get(species_id, 0) + 1

evolution_unlock = {target: level for _, target, level in evolutions}
evolved_species = {target for _, target, _ in evolutions}
legendary = {144,145,146,150,151,243,244,245,249,250,251,377,378,379,380,381,382,383,384,385,386}
starters = {1,4,7,152,155,158,252,255,258}
# Starters are exclusive rewards in Pokegochi. Exclude every descendant as
# well (Ivysaur/Charmeleon/etc.), not only the three base forms.
starter_families = set(starters)
changed = True
while changed:
    changed = False
    for source, target, _ in evolutions:
        if source in starter_families and target not in starter_families:
            starter_families.add(target)
            changed = True

# Encounter rarity is a gameplay value, not the raw number of ROM slots in
# which a species happens to occur. The old `max(2, wild_counts)` mapping put
# 149 ordinary/gift Pokemon at weight 2, almost indistinguishable from a
# legendary at weight 1. Keep broad ROM occurrence as the baseline, then use
# explicit categories for the species whose identity depends on being rare.
rare_weight_overrides = {
    # Kanto gifts, Safari prizes, fossils and rare evolution families.
    113: 5, 115: 6, 123: 6, 127: 6, 128: 6, 131: 6,
    132: 6, 133: 10, 134: 4, 135: 4, 136: 4, 137: 5,
    138: 5, 139: 3, 140: 5, 141: 3, 142: 4, 143: 5,
    147: 6, 148: 4, 149: 3,
    # Johto gifts/rare singles and the pseudo-legendary family.
    175: 6, 176: 4, 185: 6, 201: 10, 214: 6, 215: 6,
    227: 6, 235: 6, 246: 6, 247: 4, 248: 3,
    # Hoenn fossils, notoriously rare encounters and pseudo-legendaries.
    345: 5, 346: 3, 347: 5, 348: 3, 349: 5, 351: 6,
    358: 5, 359: 6, 371: 6, 372: 4, 373: 3,
    374: 6, 375: 4, 376: 3,
}

def encounter_weight(species_id: int) -> int:
    if species_id in legendary:
        return 1
    if species_id in rare_weight_overrides:
        return rare_weight_overrides[species_id]
    count = wild_counts.get(species_id)
    if count is not None:
        # One/two ROM slots now mean uncommon (5/6), not near-legendary.
        return min(20, 4 + count)
    # Pokemon absent from wild tables are usually gifts or evolved forms.
    # Base gifts remain uncommon; directly catchable evolutions are rarer.
    return 3 if species_id in evolved_species else 6

content += "\nconstexpr EncounterSpecies kEncounterSpecies[] = {\n"
for species_id, _ in species:
    # A ROM may contain deliberately under-levelled evolved encounters (for
    # example fishing Gyarados). Pokegochi's agreed rule is stricter: an
    # evolved species never appears below its adapted evolution level.
    observed_level = wild_levels.get(species_id, evolution_unlock.get(species_id, 25))
    unlock = max(observed_level, evolution_unlock.get(species_id, 5))
    weight = encounter_weight(species_id)
    if species_id in legendary:
        unlock = 70 if species_id == 150 else 50
    if species_id in starter_families:
        continue
    generation = 1 if species_id <= 151 else 2 if species_id <= 251 else 3
    content += f"    {{{species_id}, {unlock}, {weight}, {generation}}},\n"
content += "};\n"

assert encounter_weight(133) == 10 and encounter_weight(132) == 6
assert encounter_weight(147) == 6 and encounter_weight(149) == 3
# Pin the same rarity tiers across every unlocked region, not only Kanto.
# Rare base species stay clearly more available than a legendary, while their
# evolved forms remain progressively harder to encounter directly.
assert encounter_weight(201) == 10  # Unown
assert encounter_weight(246) == 6 and encounter_weight(248) == 3  # Larvitar family
assert encounter_weight(349) == 5  # Feebas
assert encounter_weight(371) == 6 and encounter_weight(373) == 3  # Bagon family
assert encounter_weight(374) == 6 and encounter_weight(376) == 3  # Beldum family
assert all(encounter_weight(species_id) == 1 for species_id in legendary)
assert all(species_id in legendary or encounter_weight(species_id) >= 3
           for species_id, _ in species)
OUT.write_text(content, encoding="utf-8", newline="\n")
print(f"Generated {len(rows)} species and {len(evolutions)} National evolutions")
