"""Canonical Generation-VII tables from the local pokeemerald-expansion tree.

Ultra Sun is useful as a final binary reference, but the expansion checkout is
the auditable source used by the generators: its National-Dex ids, Ultra Sun
stats, level-up pools, evolutions, move metadata and ability text are all
human-readable. Only #722..807 belong to the 3DS Alola stage. Pokegochi's
compact move ABI stores Ultra Sun's 54 ordinary selectable moves at #622..675;
the official Z-Move-only rows are generated at runtime from crystals instead
of occupying ordinary moveset slots. Let's Go's #808/#809 remain excluded.
"""

from __future__ import annotations

import re
from dataclasses import dataclass

from unova_rom_data import expansion_text, internal_ability_id


FIRST_SPECIES = 722
LAST_SPECIES = 807
FIRST_MOVE = 622
LAST_MOVE = 675


def _initializer(source: str, marker: str) -> str:
    match = re.search(marker, source)
    if not match:
        raise RuntimeError(f"Missing expansion initializer: {marker}")
    start = source.find("{", match.end())
    if start < 0:
        raise RuntimeError(f"Missing opening brace after {marker}")
    depth = 0
    for end in range(start, len(source)):
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
            if depth == 0:
                return source[start + 1:end]
    raise RuntimeError(f"Unclosed expansion initializer: {marker}")


def _field(block: str, name: str, default: str | None = None) -> str:
    match = re.search(rf"\.{name}\s*=\s*([^,\n]+)", block)
    if match:
        return match.group(1).strip()
    if default is not None:
        return default
    raise RuntimeError(f"Missing field {name}")


def _gen7_int(expression: str) -> int:
    """Resolve the compact build-time expressions present in Gen-VII rows."""
    expression = expression.strip()
    if re.fullmatch(r"-?\d+", expression):
        return int(expression)
    ternary = re.fullmatch(r"(.+?)\?\s*(-?\d+)\s*:\s*(-?\d+)", expression)
    if ternary:
        condition, when_true, when_false = ternary.groups()
        generation = re.search(r"GEN_(\d+)", condition)
        threshold = int(generation.group(1)) if generation else 99
        if ">=" in condition:
            enabled = 7 >= threshold
        elif "<=" in condition:
            enabled = 7 <= threshold
        elif ">" in condition:
            enabled = 7 > threshold
        elif "<" in condition:
            enabled = 7 < threshold
        else:
            enabled = False
        return int(when_true if enabled else when_false)
    raise RuntimeError(f"Unsupported Gen-VII integer expression: {expression}")


def species_symbols() -> dict[int, str]:
    source = expansion_text("include/constants/species.h")
    result: dict[int, str] = {}
    for symbol, number in re.findall(r"\bSPECIES_([A-Z0-9_]+)\s*=\s*(\d+)", source):
        value = int(number)
        if FIRST_SPECIES <= value <= LAST_SPECIES and value not in result:
            result[value] = symbol
    if sorted(result) != list(range(FIRST_SPECIES, LAST_SPECIES + 1)):
        raise RuntimeError(f"Incomplete Alola National-Dex symbols: {len(result)}")
    return result


def move_symbols() -> dict[int, str]:
    source = expansion_text("include/constants/moves.h")
    result = {
        int(number): symbol
        for symbol, number in re.findall(r"\bMOVE_([A-Z0-9_]+)\s*=\s*(\d+)", source)
        if FIRST_MOVE <= int(number) <= LAST_MOVE
    }
    result[FIRST_MOVE] = "SHORE_UP"  # equals MOVES_COUNT_GEN6 upstream
    if sorted(result) != list(range(FIRST_MOVE, LAST_MOVE + 1)):
        raise RuntimeError(f"Incomplete Alola move symbols: {len(result)}")
    return result


def ability_ids() -> dict[str, int]:
    source = expansion_text("include/constants/abilities.h")
    canonical = {
        symbol: int(number)
        for symbol, number in re.findall(r"\bABILITY_([A-Z0-9_]+)\s*=\s*(\d+)", source)
    }
    canonical["STAMINA"] = 192  # equals ABILITIES_COUNT_GEN6 upstream
    canonical["NONE"] = 0
    # The upstream enum also has sentinel/count aliases above the persistent
    # byte range. Alola species can only reference canonical abilities 0..233.
    return {symbol: internal_ability_id(number) for symbol, number in canonical.items()
            if 0 <= number <= 233}


def _gender(expression: str) -> int:
    constants = {"MON_MALE": 0, "MON_FEMALE": 254, "MON_GENDERLESS": 255}
    if expression in constants:
        return constants[expression]
    percent = re.fullmatch(r"PERCENT_FEMALE\(([0-9.]+)\)", expression)
    if not percent:
        raise RuntimeError(f"Unsupported Alola gender expression: {expression}")
    return min(254, int(float(percent.group(1)) * 255 / 100))


@dataclass(frozen=True)
class SpeciesRow:
    species_id: int
    symbol: str
    name: str
    type1: str
    type2: str
    hp: int
    attack: int
    defense: int
    sp_attack: int
    sp_defense: int
    speed: int
    catch_rate: int
    experience: int
    growth: str
    ability1: int
    ability2: int
    hidden_ability: int
    ev_hp: int
    ev_attack: int
    ev_defense: int
    ev_sp_attack: int
    ev_sp_defense: int
    ev_speed: int
    gender_ratio: int
    block: str


def species_rows() -> list[SpeciesRow]:
    source = expansion_text("src/data/pokemon/species_info/gen_7_families.h")
    abilities = ability_ids()
    rows: list[SpeciesRow] = []
    for species_id, symbol in species_symbols().items():
        block = _initializer(source, rf"\[SPECIES_{re.escape(symbol)}\]\s*=")
        # These two National rows are emitted through C preprocessor macros in
        # expansion. Spell their Ultra Sun base forms out here so generation
        # remains deterministic without running the whole decomp preprocessor.
        if symbol == "SILVALLY_NORMAL":
            rows.append(SpeciesRow(
                species_id, symbol, "SILVALLY", "Normal", "Normal",
                95, 95, 95, 95, 95, 95, 3, 257, "Slow",
                abilities["RKS_SYSTEM"], abilities["RKS_SYSTEM"], 0,
                3, 0, 0, 0, 0, 0, 255,
                ".levelUpLearnset = sSilvallyLevelUpLearnset",
            ))
            continue
        if symbol == "MINIOR_METEOR_RED":
            rows.append(SpeciesRow(
                species_id, symbol, "MINIOR", "Rock", "Flying",
                60, 60, 100, 60, 100, 60, 30, 154, "MediumSlow",
                abilities["SHIELDS_DOWN"], abilities["SHIELDS_DOWN"], 0,
                0, 0, 1, 0, 1, 0, 255,
                ".levelUpLearnset = sMiniorLevelUpLearnset",
            ))
            continue
        types = re.search(
            r"\.types\s*=\s*MON_TYPES\(TYPE_([A-Z]+)(?:,\s*TYPE_([A-Z]+))?\)",
            block,
        )
        if not types:
            raise RuntimeError(f"Missing Alola types for {symbol}")
        type1 = types.group(1).title()
        type2 = (types.group(2) or types.group(1)).title()
        ability_match = re.search(
            r"\.abilities\s*=\s*\{\s*ABILITY_([A-Z0-9_]+)\s*,\s*"
            r"ABILITY_([A-Z0-9_]+)\s*,\s*ABILITY_([A-Z0-9_]+)\s*\}",
            block,
        )
        if not ability_match:
            raise RuntimeError(f"Missing Alola abilities for {symbol}")
        ability_symbols = ability_match.groups()
        try:
            ability_values = tuple(abilities[value] for value in ability_symbols)
        except KeyError as error:
            raise RuntimeError(f"Unknown ability {error.args[0]} for {symbol}") from error
        name_match = re.search(r'\.speciesName\s*=\s*_\("([^"]+)"\)', block)
        if not name_match:
            raise RuntimeError(f"Missing Alola species name for {symbol}")
        rows.append(SpeciesRow(
            species_id=species_id,
            symbol=symbol,
            name=name_match.group(1).upper(),
            type1=type1,
            type2=type2,
            hp=_gen7_int(_field(block, "baseHP")),
            attack=_gen7_int(_field(block, "baseAttack")),
            defense=_gen7_int(_field(block, "baseDefense")),
            sp_attack=_gen7_int(_field(block, "baseSpAttack")),
            sp_defense=_gen7_int(_field(block, "baseSpDefense")),
            speed=_gen7_int(_field(block, "baseSpeed")),
            catch_rate=_gen7_int(_field(block, "catchRate")),
            experience=_gen7_int(_field(block, "expYield")),
            growth=_field(block, "growthRate").removeprefix("GROWTH_").title().replace("_", ""),
            ability1=ability_values[0],
            ability2=ability_values[1] or ability_values[0],
            hidden_ability=ability_values[2],
            ev_hp=_gen7_int(_field(block, "evYield_HP", "0")),
            ev_attack=_gen7_int(_field(block, "evYield_Attack", "0")),
            ev_defense=_gen7_int(_field(block, "evYield_Defense", "0")),
            ev_sp_attack=_gen7_int(_field(block, "evYield_SpAttack", "0")),
            ev_sp_defense=_gen7_int(_field(block, "evYield_SpDefense", "0")),
            ev_speed=_gen7_int(_field(block, "evYield_Speed", "0")),
            gender_ratio=_gender(_field(block, "genderRatio")),
            block=block,
        ))
    return rows


_EVOLUTION_LEVEL_OVERRIDES = {
    737: 36,   # Charjabug, magnetic-field adaptation
    739: 36,   # Crabrawler, Mount Lanakila adaptation
    762: 29,   # Steenee learns Stomp at this point in the imported pool
    772: 30,   # Type: Null friendship
    803: 40,   # Poipole after learning Dragon Pulse
}


def evolutions() -> list[tuple[int, int, int]]:
    symbols = species_symbols()
    symbol_ids = {symbol: species_id for species_id, symbol in symbols.items()}
    rows: list[tuple[int, int, int]] = []
    for row in species_rows():
        for method, parameter, target_symbol in re.findall(
            r"\{(EVO_[A-Z0-9_]+)\s*,\s*([^,{}]+)\s*,\s*SPECIES_([A-Z0-9_]+)",
            row.block,
        ):
            target = symbol_ids.get(target_symbol)
            if target is None or not FIRST_SPECIES <= target <= LAST_SPECIES:
                # Regional/future alternate evolutions (for example Hisuian
                # Decidueye) are not part of Ultra Sun. Alolan alternate forms
                # such as Lycanroc remain one National species and are chosen
                # by the form layer after the canonical Midday destination.
                continue
            numeric = re.search(r"\d+", parameter)
            level = int(numeric.group()) if method.startswith("EVO_LEVEL") and numeric else 36
            if level == 0:
                level = _EVOLUTION_LEVEL_OVERRIDES.get(row.species_id, 36)
            candidate = (row.species_id, target, level)
            if target != row.species_id and candidate not in rows:
                rows.append(candidate)
    return rows


def level_moves(species_id: int) -> list[tuple[int, int]]:
    row = next(row for row in species_rows() if row.species_id == species_id)
    pointer = re.search(r"\.levelUpLearnset\s*=\s*(s[A-Za-z0-9_]+LevelUpLearnset)", row.block)
    if not pointer:
        raise RuntimeError(f"Missing level-up pointer for {row.symbol}")
    source = expansion_text("src/data/pokemon/level_up_learnsets/gen_7.h")
    block = _initializer(source, rf"static const struct LevelUpMove\s+{pointer.group(1)}\[\]\s*=")
    symbols = {symbol: move_id for move_id, symbol in move_symbols().items()}
    # Older move ids are all explicit numeric constants in the same enum.
    constants = expansion_text("include/constants/moves.h")
    for symbol, number in re.findall(r"\bMOVE_([A-Z0-9_]+)\s*=\s*(\d+)", constants):
        symbols.setdefault(symbol, int(number))
    result: list[tuple[int, int]] = []
    for level, move_symbol in re.findall(
        r"LEVEL_UP_MOVE\(\s*(\d+)\s*,\s*MOVE_([A-Z0-9_]+)\s*\)", block
    ):
        move_id = symbols.get(move_symbol)
        if move_id is not None and 0 < move_id <= LAST_MOVE:
            result.append((int(level), move_id))
    return result


def move_block(symbol: str) -> str:
    return _initializer(
        expansion_text("src/data/moves_info.h"),
        rf"\[MOVE_{re.escape(symbol)}\]\s*=",
    )


def move_field(block: str, name: str, default: str | None = None) -> str:
    return _field(block, name, default)


def move_int(block: str, name: str, default: str = "0") -> int:
    return _gen7_int(_field(block, name, default))
