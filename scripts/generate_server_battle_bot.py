#!/usr/bin/env python3
"""Generate the fixed competitive Box used by the online ESP32 bot."""

from __future__ import annotations

import re
import random
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SPECIES_SOURCE = ROOT / "src/game/PokemonDataGenerated.inc"
MOVE_SOURCES = (
    ROOT / "src/game/MoveDataGenerated.inc",
    ROOT / "src/game/PokemonData.cpp",
)
MEGA_SOURCE = ROOT / "src/game/MegaEvolution.cpp"
OUTPUT = (
    ROOT
    / "server/PokegochiOnlineServer/src/ServerBattleBotRosterGenerated.inc"
)
MOVE_AI_OUTPUT = (
    ROOT
    / "server/PokegochiOnlineServer/src/ServerBattleBotMoveAiGenerated.inc"
)
SPECIES_TYPES_OUTPUT = (
    ROOT
    / "server/PokegochiOnlineServer/src/ServerBattleBotSpeciesTypesGenerated.inc"
)

# This seed was used once when the permanent Box was authored. Its selected
# index is kept as a constant in ServerBattleBot.h so firmware rebuilds never
# silently move the one shiny member to another Pokemon.
SHINY_INDEX = 61

SPREADS = {
    "physical": ("Jolly", (4, 252, 0, 0, 0, 252)),
    "physical_power": ("Adamant", (4, 252, 0, 0, 0, 252)),
    "physical_bulky": ("Adamant", (252, 252, 0, 0, 4, 0)),
    "special": ("Timid", (4, 0, 0, 252, 0, 252)),
    "special_power": ("Modest", (4, 0, 0, 252, 0, 252)),
    "special_bulky": ("Modest", (252, 0, 4, 252, 0, 0)),
    "mixed_fast": ("Hasty", (0, 100, 0, 156, 0, 252)),
    "mixed_bulky": ("Quiet", (252, 4, 0, 252, 0, 0)),
    "wall": ("Bold", (252, 0, 128, 0, 128, 0)),
}

# species, generation, legendary, ability slot, spread, ordinary held item,
# and four authored moves. Every entry is a final evolution. The first move
# is always a damaging move and therefore supplies the compatible Z-Crystal.
CANDIDATES = [
    (3, 1, False, "a1", "special_bulky", "BlackSludge", (202, 188, 79, 235)),
    (6, 1, False, "a1", "special", "LifeOrb", (53, 403, 406, 355)),
    (9, 1, False, "a1", "special_bulky", "Leftovers", (503, 58, 396, 229)),
    (65, 1, False, "hidden", "special", "FocusSash", (94, 247, 411, 105)),
    (94, 1, False, "a1", "special", "LifeOrb", (247, 188, 85, 194)),
    (68, 1, False, "a1", "physical_bulky", "AssaultVest", (223, 282, 444, 418)),
    (143, 1, False, "a1", "physical_bulky", "Leftovers", (34, 89, 242, 156)),
    (131, 1, False, "a1", "special_bulky", "Leftovers", (58, 57, 85, 195)),
    (149, 1, False, "hidden", "physical", "WeaknessPolicy", (337, 245, 89, 355)),
    (150, 1, True, "a1", "special", "LifeOrb", (540, 396, 58, 347)),

    (154, 2, False, "a1", "special_bulky", "Leftovers", (202, 605, 73, 235)),
    (157, 2, False, "hidden", "special", "ChoiceScarf", (284, 53, 411, 326)),
    (160, 2, False, "hidden", "physical", "LifeOrb", (127, 8, 242, 349)),
    (169, 2, False, "hidden", "physical", "BlackSludge", (413, 440, 369, 355)),
    (181, 2, False, "a1", "special_power", "AssaultVest", (85, 406, 411, 521)),
    (212, 2, False, "a2", "physical_bulky", "LifeOrb", (418, 369, 14, 276)),
    (214, 2, False, "a1", "physical", "ChoiceScarf", (370, 224, 444, 282)),
    (248, 2, False, "a1", "physical_bulky", "WeaknessPolicy", (444, 242, 89, 349)),
    (230, 2, False, "a1", "special", "ScopeLens", (56, 434, 58, 240)),
    (249, 2, True, "hidden", "special_bulky", "Leftovers", (177, 94, 58, 355)),
    (202, 2, False, "a1", "wall", "Leftovers", (68, 243, 227, 194)),

    (254, 3, False, "hidden", "special", "LifeOrb", (437, 406, 411, 202)),
    (257, 3, False, "hidden", "physical", "LifeOrb", (394, 370, 444, 182)),
    (260, 3, False, "a1", "physical_bulky", "Leftovers", (127, 89, 8, 446)),
    (282, 3, False, "a1", "special", "ChoiceScarf", (585, 94, 247, 347)),
    (289, 3, False, "a1", "physical_power", "ChoiceBand", (216, 89, 400, 303)),
    (350, 3, False, "a1", "wall", "Leftovers", (503, 58, 105, 92)),
    (373, 3, False, "hidden", "physical", "LifeOrb", (337, 89, 349, 126)),
    (376, 3, False, "a1", "physical_bulky", "AssaultVest", (309, 428, 89, 418)),
    (330, 3, False, "a1", "physical", "ChoiceScarf", (89, 337, 369, 444)),
    (384, 3, True, "a1", "physical", "LifeOrb", (620, 337, 245, 89)),

    (389, 4, False, "hidden", "physical_bulky", "Leftovers", (452, 89, 444, 235)),
    (392, 4, False, "hidden", "physical", "LifeOrb", (394, 370, 369, 183)),
    (395, 4, False, "a1", "special_bulky", "Leftovers", (56, 430, 58, 446)),
    (398, 4, False, "hidden", "physical", "ChoiceBand", (413, 370, 369, 355)),
    (407, 4, False, "a1", "special", "BlackSludge", (202, 188, 79, 247)),
    (445, 4, False, "hidden", "physical", "RockyHelmet", (89, 337, 444, 14)),
    (448, 4, False, "hidden", "physical", "LifeOrb", (370, 309, 245, 14)),
    (468, 4, False, "a2", "special_bulky", "Leftovers", (403, 605, 396, 355)),
    (461, 4, False, "a1", "physical", "LifeOrb", (556, 282, 420, 67)),
    (483, 4, True, "a1", "special_bulky", "AssaultVest", (459, 430, 85, 396)),

    (497, 5, False, "hidden", "special", "Leftovers", (437, 406, 137, 202)),
    (500, 5, False, "hidden", "physical_bulky", "LifeOrb", (394, 276, 528, 457)),
    (503, 5, False, "hidden", "physical", "ExpertBelt", (534, 224, 58, 453)),
    (530, 5, False, "hidden", "physical", "AirBalloon", (89, 442, 157, 14)),
    (534, 5, False, "a1", "physical_bulky", "AssaultVest", (409, 183, 282, 444)),
    (609, 5, False, "a2", "special", "ChoiceScarf", (247, 126, 412, 347)),
    (612, 5, False, "a1", "physical", "LifeOrb", (337, 89, 398, 349)),
    (637, 5, False, "a1", "special", "Leftovers", (552, 405, 202, 483)),
    (635, 5, False, "a1", "special", "ChoiceSpecs", (434, 399, 430, 369)),
    (644, 5, True, "a1", "physical_bulky", "WeaknessPolicy", (550, 337, 242, 355)),

    (652, 6, False, "hidden", "physical_bulky", "Leftovers", (452, 409, 444, 596)),
    (655, 6, False, "hidden", "special", "ChoiceScarf", (126, 94, 605, 347)),
    (658, 6, False, "hidden", "mixed_fast", "AshsBond", (594, 399, 58, 369)),
    (663, 6, False, "hidden", "physical", "ChoiceBand", (413, 394, 355, 369)),
    (681, 6, False, "a1", "mixed_bulky", "Leftovers", (247, 533, 588, 425)),
    (706, 6, False, "hidden", "special_bulky", "AssaultVest", (434, 188, 126, 85)),
    (700, 6, False, "hidden", "special_bulky", "Leftovers", (585, 247, 347, 273)),
    (701, 6, False, "a1", "physical", "FocusSash", (560, 136, 444, 14)),
    (715, 6, False, "hidden", "special", "ChoiceSpecs", (434, 542, 53, 355)),
    (716, 6, True, "a1", "special_bulky", "Leftovers", (585, 85, 411, 601)),

    (724, 7, False, "hidden", "physical", "LifeOrb", (625, 348, 413, 14)),
    (727, 7, False, "hidden", "physical_bulky", "AssaultVest", (626, 394, 238, 339)),
    (730, 7, False, "hidden", "special_bulky", "Leftovers", (627, 585, 94, 58)),
    (738, 7, False, "a1", "special", "ChoiceSpecs", (85, 405, 412, 355)),
    (745, 7, False, "hidden", "physical", "FocusSash", (444, 663, 242, 14)),
    (778, 7, False, "a1", "physical", "LifeOrb", (583, 421, 425, 14)),
    (784, 7, False, "hidden", "physical", "WeaknessPolicy", (654, 370, 53, 349)),
    (748, 7, False, "hidden", "wall", "BlackSludge", (503, 188, 105, 92)),
    (750, 7, False, "a1", "physical_bulky", "AssaultVest", (89, 484, 157, 370)),
    (791, 7, True, "a1", "physical_bulky", "WeaknessPolicy", (667, 428, 394, 89)),

    # Additional fully-authored candidates make the permanent ten-per-region
    # selection an actual one-time draw instead of a hand-picked fixed list.
    (59, 1, False, "hidden", "physical", "LifeOrb", (394, 245, 528, 370)),
    (130, 1, False, "hidden", "physical", "LumBerry", (127, 242, 423, 349)),
    (144, 1, True, "hidden", "special_bulky", "Leftovers", (58, 542, 355, 92)),
    (145, 1, True, "hidden", "special", "Leftovers", (85, 257, 355, 521)),
    (146, 1, True, "hidden", "special", "LifeOrb", (126, 542, 355, 403)),
    (151, 1, True, "a1", "special", "LifeOrb", (94, 396, 58, 347)),

    (229, 2, False, "hidden", "special", "LifeOrb", (126, 399, 188, 417)),
    (196, 2, False, "hidden", "special", "LifeOrb", (94, 605, 247, 347)),
    (250, 2, True, "hidden", "physical_bulky", "Leftovers", (221, 413, 89, 105)),
    (251, 2, True, "a1", "special_bulky", "Leftovers", (94, 202, 605, 105)),
    (243, 2, True, "hidden", "special", "Leftovers", (85, 521, 247, 347)),

    (306, 3, False, "a1", "physical_bulky", "RockyHelmet", (484, 444, 89, 446)),
    (286, 3, False, "hidden", "physical", "ToxicOrb", (402, 409, 183, 147)),
    (383, 3, True, "a1", "physical_bulky", "LifeOrb", (619, 7, 444, 14)),
    (382, 3, True, "a1", "special_bulky", "Leftovers", (618, 58, 85, 347)),
    (381, 3, True, "a1", "special", "SoulDew", (434, 94, 57, 105)),
    (385, 3, True, "a1", "special_bulky", "Leftovers", (442, 94, 85, 273)),

    (466, 4, False, "a1", "physical", "ExpertBelt", (528, 8, 89, 238)),
    (467, 4, False, "a1", "special", "ChoiceSpecs", (126, 85, 411, 94)),
    (484, 4, True, "a1", "special_bulky", "LustrousOrb", (460, 56, 126, 396)),
    (487, 4, True, "a1", "physical_bulky", "GriseousOrb", (467, 337, 396, 156)),
    (491, 4, True, "a1", "special", "LifeOrb", (399, 188, 411, 417)),
    (493, 4, True, "a1", "special_bulky", "Leftovers", (449, 414, 58, 105)),

    (553, 5, False, "hidden", "physical", "ChoiceScarf", (89, 282, 444, 276)),
    (598, 5, False, "a1", "physical_bulky", "RockyHelmet", (438, 360, 73, 446)),
    (643, 5, True, "a1", "special_bulky", "LifeOrb", (551, 434, 414, 355)),
    (646, 5, True, "a1", "special_bulky", "Leftovers", (58, 434, 414, 355)),
    (494, 5, True, "a1", "physical", "LifeOrb", (557, 428, 550, 369)),
    (649, 5, True, "a1", "special", "ChoiceScarf", (546, 430, 58, 369)),

    (697, 6, False, "hidden", "physical", "ChoiceScarf", (457, 337, 89, 349)),
    (671, 6, False, "hidden", "special_bulky", "Leftovers", (585, 94, 202, 235)),
    (717, 6, True, "a1", "special_bulky", "LifeOrb", (613, 399, 411, 355)),
    (718, 6, True, "a1", "physical_bulky", "Leftovers", (614, 650, 349, 245)),
    (719, 6, True, "a1", "special_bulky", "Leftovers", (591, 585, 94, 414)),
    (720, 6, True, "a1", "special", "LifeOrb", (593, 94, 247, 417)),

    (768, 7, False, "a1", "physical_bulky", "AssaultVest", (623, 664, 141, 389)),
    (758, 7, False, "hidden", "special", "FocusSash", (482, 53, 406, 417)),
    (792, 7, True, "a1", "special_bulky", "Leftovers", (668, 94, 58, 355)),
    (800, 7, True, "a1", "special_bulky", "WeaknessPolicy", (675, 430, 414, 347)),
    (785, 7, True, "a1", "special", "LifeOrb", (85, 605, 521, 355)),
    (802, 7, True, "a1", "physical", "LifeOrb", (666, 370, 425, 317)),
]

ROSTER_SEED = 0x20261105

# These species are part of the bot's authored identity, not candidates that
# may disappear on a later deterministic regeneration. The remaining slots
# retain the original one-time draw and every generation still owns ten.
PINNED_SPECIES = {
    149,  # Dragonite
    202,  # Wobbuffet
    381, 384,  # Latios, Rayquaza
    448, 491,  # Lucario, Darkrai
    658, 681, 716, 718,  # Greninja, Aegislash, Xerneas, Zygarde
    778,  # Mimikyu
}


def select_permanent_roster() -> list[tuple]:
    rng = random.Random(ROSTER_SEED)
    selected: list[tuple] = []
    for generation in range(1, 8):
        pinned = [row for row in CANDIDATES
                  if row[1] == generation and row[0] in PINNED_SPECIES]
        ordinary = [row for row in CANDIDATES
                    if row[1] == generation and not row[2]
                    and row[0] not in PINNED_SPECIES]
        legendary = [row for row in CANDIDATES if row[1] == generation and row[2]]
        regional = list(pinned)
        if not any(row[2] for row in regional):
            regional.append(rng.choice(legendary))
        remaining = 10 - len(regional)
        assert remaining >= 0, f"too many pinned species in generation {generation}"
        regional.extend(rng.sample(ordinary, remaining))
        rng.shuffle(regional)
        selected.extend(regional)
    return selected


ROSTER = select_permanent_roster()

TYPE_TO_CRYSTAL = {
    "Normal": "NormaliumZ", "Fighting": "FightiniumZ",
    "Flying": "FlyiniumZ", "Poison": "PoisoniumZ",
    "Ground": "GroundiumZ", "Rock": "RockiumZ", "Bug": "BuginiumZ",
    "Ghost": "GhostiumZ", "Steel": "SteeliumZ", "Fire": "FiriumZ",
    "Water": "WateriumZ", "Grass": "GrassiumZ",
    "Electric": "ElectriumZ", "Psychic": "PsychiumZ", "Ice": "IciumZ",
    "Dragon": "DragoniumZ", "Dark": "DarkiniumZ", "Fairy": "FairiumZ",
}


def parse_species() -> dict[int, dict[str, int | str]]:
    result: dict[int, dict[str, int | str]] = {}
    for line in SPECIES_SOURCE.read_text(encoding="utf-8").splitlines():
        if not line.lstrip().startswith('{"'):
            continue
        values = [value.strip() for value in line.strip().rstrip(",").strip("{}").split(",")]
        if len(values) < 20:
            continue
        species_id = int(values[1])
        result[species_id] = {
            "type1": values[3].removeprefix("PokemonType::"),
            "type2": values[4].removeprefix("PokemonType::"),
            "base_hp": int(values[5]),
            "a1": int(values[17]),
            "a2": int(values[18]),
            "hidden": int(values[19]),
        }
    return result


def parse_moves() -> dict[int, dict[str, int | str]]:
    result: dict[int, dict[str, int | str]] = {}
    expression = re.compile(
        r'\{(\d+),\s*"([^"]+)",\s*PokemonType::(\w+),\s*(\d+),\s*'
        r'(\d+),\s*MoveCategory::(\w+),\s*(\d+),\s*(-?\d+),\s*'
        r'(\d+),\s*"([^"]+)"'
    )
    for source in MOVE_SOURCES:
        for match in expression.finditer(source.read_text(encoding="utf-8")):
            move_id = int(match.group(1))
            result[move_id] = {
                "name": match.group(2),
                "type": match.group(3),
                "power": int(match.group(4)),
                "accuracy": int(match.group(5)),
                "category": match.group(6),
                "pp": int(match.group(7)),
                "priority": int(match.group(8)),
                "effect_chance": int(match.group(9)),
                "effect": match.group(10),
            }
    return result


def parse_mega_species() -> set[int]:
    forms = re.findall(
        r"\{(\d+),\s*MegaVariant::(?:Mega|MegaX|MegaY|MegaZ|Primal),",
        MEGA_SOURCE.read_text(encoding="utf-8"),
    )
    # Rayquaza transforms through Dragon Ascent and explicitly cannot hold the
    # universal Mega Stone, so it is not a candidate for dynamic assignment.
    return {int(species_id) for species_id in forms if int(species_id) != 384}


def validate(species: dict[int, dict[str, int]], moves: dict[int, dict[str, int | str]]) -> None:
    assert len(ROSTER) == 70
    assert 0 <= SHINY_INDEX < len(ROSTER)
    assert PINNED_SPECIES <= {row[0] for row in ROSTER}, \
        "a required server-bot species was omitted"
    for generation in range(1, 8):
        regional = [row for row in ROSTER if row[1] == generation]
        assert len(regional) == 10, f"generation {generation} does not have 10 entries"
        assert any(row[2] for row in regional), f"generation {generation} lacks a legendary"
    assert len({row[0] for row in ROSTER}) == len(ROSTER), "duplicate species in bot Box"
    for row in ROSTER:
        species_id, _, _, ability, _, _, authored_moves = row
        assert species_id in species, f"unknown species {species_id}"
        assert species[species_id][ability] != 0, f"species {species_id} lacks {ability}"
        for move in authored_moves:
            assert move in moves, f"unknown move {move} for species {species_id}"
            assert moves[move]["pp"] > 0, f"move {move} has no PP"
        first = moves[authored_moves[0]]
        assert first["power"] > 0, f"species {species_id} has status Z base move"
        assert first["type"] in TYPE_TO_CRYSTAL

    generated = SPECIES_SOURCE.read_text(encoding="utf-8")
    evolution_block = generated.split("constexpr EvolutionData kEvolutions[] = {", 1)[1].split("};", 1)[0]
    evolving = {int(value) for value in re.findall(r"\{\s*(\d+)\s*,", evolution_block)}
    invalid = sorted(row[0] for row in ROSTER if row[0] in evolving)
    assert not invalid, f"bot Box contains non-final evolutions: {invalid}"


def render() -> str:
    species = parse_species()
    moves = parse_moves()
    mega_species = parse_mega_species()
    validate(species, moves)
    lines = ["// Generated by scripts/generate_server_battle_bot.py. Do not edit by hand."]
    for index, row in enumerate(ROSTER):
        species_id, generation, legendary, ability, spread, item, authored_moves = row
        nature, evs = SPREADS[spread]
        entry = species[species_id]
        pps = [int(moves[move]["pp"]) for move in authored_moves]
        z_crystal = TYPE_TO_CRYSTAL[str(moves[authored_moves[0]]["type"])]
        move_list = ", ".join(f"{move}U" for move in authored_moves)
        pp_list = ", ".join(f"{pp}U" for pp in pps)
        ev_list = ", ".join(f"{value}U" for value in evs)
        lines.append(
            "  {"
            f"{species_id}U, {{{move_list}}}, {{{pp_list}}}, {generation}U, "
            f"{entry['base_hp']}U, {entry[ability]}U, PokemonNature::{nature}, "
            f"HeldItem::{item}, HeldItem::{z_crystal}, 0U, "
            f"{'true' if species_id in mega_species else 'false'}, "
            f"{'true' if legendary else 'false'}, "
            f"{'true' if index == SHINY_INDEX else 'false'}, "
            f"{{{ev_list}}}}},"
        )
    return "\n".join(lines) + "\n"


def render_move_ai() -> str:
    moves = parse_moves()
    used_moves = sorted({move for row in ROSTER for move in row[6]})
    lines = [
        "// Generated by scripts/generate_server_battle_bot.py from the canonical move table.",
        "// Only moves authored into the fixed server roster are retained.",
    ]
    for move_id in used_moves:
        move = moves[move_id]
        lines.append(
            "  {"
            f"{move_id}U, PokemonType::{move['type']}, {move['power']}U, "
            f"{move['accuracy']}U, MoveCategory::{move['category']}, "
            f"{move['pp']}U, {move['priority']}, {move['effect_chance']}U, "
            f"\"{move['effect']}\""
            "},"
        )
    return "\n".join(lines) + "\n"


def render_species_types() -> str:
    species = parse_species()
    highest = max(species)
    assert set(species) == set(range(1, highest + 1)), \
        "species type table requires contiguous National Dex ids"
    lines = [
        "// Generated by scripts/generate_server_battle_bot.py from the canonical species table.",
        "// Index zero is an explicit unknown-species fallback.",
        "  {PokemonType::Normal, PokemonType::Normal},",
    ]
    for species_id in range(1, highest + 1):
        entry = species[species_id]
        lines.append(
            f"  {{PokemonType::{entry['type1']}, PokemonType::{entry['type2']}}},"
        )
    return "\n".join(lines) + "\n"


def main() -> None:
    rendered = render()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    for path, contents in (
        (OUTPUT, rendered),
        (MOVE_AI_OUTPUT, render_move_ai()),
        (SPECIES_TYPES_OUTPUT, render_species_types()),
    ):
        with path.open("w", encoding="utf-8", newline="\n") as generated:
            generated.write(contents)
    print(
        f"Generated {OUTPUT.relative_to(ROOT)} with {len(ROSTER)} competitive Pokemon, "
        f"{MOVE_AI_OUTPUT.relative_to(ROOT)} and {SPECIES_TYPES_OUTPUT.relative_to(ROOT)}"
    )


if __name__ == "__main__":
    main()
