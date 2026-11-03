"""Generate the Pokegochi TM/HM catalog and compatibility table from Emerald.

The firmware uses National Pokédex numbers, while pokeemerald's internal
species IDs are deliberately non-National after Celebi.  This generator joins
the two datasets by their SPECIES_* symbol so Treecko through Deoxys cannot be
silently assigned another Pokémon's learnset.
"""
from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path

from unova_rom_data import personal_record
from kalos_rom_data import personal_record as y_personal_record
from alola_rom_data import can_learn_tm as usum_can_learn_tm


ROOT = Path(__file__).resolve().parent.parent
EMERALD = ROOT / ".downloads/pokeemerald-tree/pokeemerald-master"
PLATINUM = ROOT / ".downloads/pokeplatinum-tree"
TMHM_CONSTANTS = EMERALD / "include/constants/tms_hms.h"
MOVE_CONSTANTS = EMERALD / "include/constants/moves.h"
POKEDEX_CONSTANTS = EMERALD / "include/constants/pokedex.h"
LEARNSETS = EMERALD / "src/data/pokemon/tmhm_learnsets.h"
OUTPUT = ROOT / "src/game/MachineDataGenerated.inc"
OWNER = ROOT / "src/game/Economy.cpp"
PROJECT_MOVE_DATA = ROOT / "src/game/MoveDataGenerated.inc"

# Pokemon White's canonical TM01..TM95 move order. Existing Emerald and
# Platinum machines remain stable; only moves that debuted as TMs in Unova are
# appended to the persistent catalog.
BW_TM_MOVES = [
    468,337,473,347,46,92,258,339,474,237,241,269,58,59,63,113,182,240,
    477,219,218,76,479,85,87,89,216,91,94,247,280,104,115,482,53,188,
    201,126,317,332,259,263,488,156,213,168,490,496,497,315,502,411,
    412,206,503,374,451,507,510,511,261,512,373,153,421,371,514,416,
    397,148,444,521,86,360,14,522,244,523,524,157,404,525,526,398,
    138,447,207,365,369,164,430,433,528,249,555,
]
BW_HM_MOVES = [15,19,57,70,127,291]
UNOVA_TM_NUMBERS = [1,3,9,19,23,34,43,47,48,49,51,55,58,59,60,62,67,72,76,78,79,82,83,93,95]
UNOVA_TM_MOVES = [BW_TM_MOVES[number - 1] for number in UNOVA_TM_NUMBERS]
UNOVA_TM_PRICES = [3000,3000,2500,3000,3000,3000,3000,3000,2500,2500,
                   4000,5000,4000,3000,3000,4000,4000,5500,4000,4000,
                   4000,4000,3000,5500,5000]
assert len(BW_TM_MOVES) == 95 and len(UNOVA_TM_MOVES) == 25

# X/Y's complete TM01..TM100/HM01..HM05 order. Append only moves not already
# represented by the stable Emerald/Platinum/Unova catalog, preserving every
# shipped machine ID. Nature Power became a TM in Kalos and is therefore part
# of this list alongside the four Generation-VI moves.
XY_TM_MOVES = [
    468,337,473,347,46,92,258,339,474,237,241,269,58,59,63,113,182,240,
    355,219,218,76,479,85,87,89,216,91,94,247,280,104,115,482,53,188,
    201,126,317,332,259,263,488,156,213,168,490,496,497,315,211,411,
    412,206,503,374,451,507,510,511,261,512,373,153,421,371,514,416,
    397,148,444,521,86,360,14,522,244,523,524,157,404,525,611,398,
    138,447,207,214,369,164,430,433,528,249,555,267,399,612,605,590,
]
XY_HM_MOVES = [15, 19, 57, 70, 127]
KALOS_TM_NUMBERS = [83, 96, 98, 99, 100]
KALOS_TM_MOVES = [XY_TM_MOVES[number - 1] for number in KALOS_TM_NUMBERS]
KALOS_TM_PRICES = [4000, 4000, 5000, 7000, 3000]
assert len(XY_TM_MOVES) == 100 and KALOS_TM_MOVES == [611, 267, 612, 605, 590]

# Ultra Sun's TM01..TM100 order, sourced from the extracted personal-table
# bitset contract. Canonical Gen-VII move ids 693/684/694 map to Pokegochi's
# compact regular-move ids 656/647/657 because Z-Moves are runtime actions,
# not ordinary selectable move rows in this project.
USUM_TM_MOVES_CANONICAL = [
    526,337,473,347,46,92,258,339,474,237,241,269,58,59,63,113,182,240,
    355,219,218,76,479,85,87,89,216,141,94,247,280,104,115,482,53,188,
    201,126,317,332,259,263,488,156,213,168,490,496,497,315,211,411,
    412,206,503,374,451,507,693,511,261,512,373,153,421,371,684,416,
    397,694,444,521,86,360,14,19,244,523,524,157,404,525,611,398,138,
    447,207,214,369,164,430,433,528,57,555,267,399,127,605,590,
]
USUM_PROJECT_MOVE_IDS = {693: 656, 684: 647, 694: 657}
USUM_TM_MOVES = [USUM_PROJECT_MOVE_IDS.get(move, move)
                 for move in USUM_TM_MOVES_CANONICAL]
ALOLA_TM_NUMBERS = [28, 59, 67, 70]
ALOLA_TM_MOVES = [USUM_TM_MOVES[number - 1] for number in ALOLA_TM_NUMBERS]
ALOLA_TM_PRICES = [7000, 5000, 5000, 7000]
assert len(USUM_TM_MOVES) == 100
assert ALOLA_TM_MOVES == [141, 656, 647, 657]


def white_can_learn(species_id: int, move_id: int) -> bool:
    record = personal_record(species_id)
    bit_indices = [index for index, candidate in enumerate(BW_TM_MOVES)
                   if candidate == move_id]
    bit_indices += [95 + index for index, candidate in enumerate(BW_HM_MOVES)
                    if candidate == move_id]
    return any((record[40 + index // 8] >> (index % 8)) & 1
               for index in bit_indices)


def y_can_learn(species_id: int, move_id: int) -> bool:
    record = y_personal_record(species_id)
    bit_indices = [index for index, candidate in enumerate(XY_TM_MOVES)
                   if candidate == move_id]
    bit_indices += [100 + index for index, candidate in enumerate(XY_HM_MOVES)
                    if candidate == move_id]
    return any((record[40 + index // 8] >> (index % 8)) & 1
               for index in bit_indices)


def usum_can_learn(species_id: int, move_id: int) -> bool:
    return any(usum_can_learn_tm(species_id, index + 1)
               for index, candidate in enumerate(USUM_TM_MOVES)
               if candidate == move_id)


def macro_body(source: str, name: str, next_name: str) -> str:
    match = re.search(
        rf"#define\s+{re.escape(name)}\(F\)\s+\\(.*?)"
        rf"(?=\n#define\s+{re.escape(next_name)}\(F\))",
        source,
        re.S,
    )
    if not match:
        raise RuntimeError(f"Could not parse {name} from Emerald constants")
    return match.group(1)


tmhm_source = TMHM_CONSTANTS.read_text(encoding="utf-8")
machine_symbols = re.findall(
    r"F\(([A-Z0-9_]+)\)",
    macro_body(tmhm_source, "FOREACH_TM", "FOREACH_HM")
    + macro_body(tmhm_source, "FOREACH_HM", "FOREACH_TMHM"),
)
if len(machine_symbols) != 58 or len(set(machine_symbols)) != 58:
    raise RuntimeError(f"Expected Emerald's 58 unique machines, got {len(machine_symbols)}")

move_ids = {
    symbol: int(number)
    for symbol, number in re.findall(
        r"^#define\s+MOVE_([A-Z0-9_]+)\s+(\d+)\s*$",
        MOVE_CONSTANTS.read_text(encoding="utf-8"),
        re.M,
    )
}
missing_moves = [symbol for symbol in machine_symbols if symbol not in move_ids]
if missing_moves:
    raise RuntimeError(f"Moves missing from Emerald constants: {missing_moves}")

# Platinum's JSON uses MOVE_* symbols while the firmware's generated move
# table is already keyed by the canonical National move number. Reuse that
# table instead of maintaining a second hand-written Gen-IV ID map.
project_move_ids = {
    symbol.replace(" ", "_"): int(number)
    for number, symbol in re.findall(
        r'^\s*\{(\d+), "([A-Z0-9 ]+)"',
        PROJECT_MOVE_DATA.read_text(encoding="utf-8"),
        re.M,
    )
}

platinum_move_ids: list[int] = []
platinum_prices: list[int] = []
for number in range(51, 93):
    item_path = PLATINUM / "res/items/data" / f"tm{number:02d}.json"
    item = json.loads(item_path.read_text(encoding="utf-8"))
    symbol = item["teachesMove"].removeprefix("MOVE_")
    if symbol not in project_move_ids:
        raise RuntimeError(f"Platinum TM{number:02d} move missing from project: {symbol}")
    platinum_move_ids.append(project_move_ids[symbol])
    platinum_prices.append(int(item["price"]))

pokedex_source = POKEDEX_CONSTANTS.read_text(encoding="utf-8")
national_symbols = re.findall(r"^\s*NATIONAL_DEX_([A-Z0-9_]+),", pokedex_source, re.M)
if not national_symbols or national_symbols[0] != "NONE":
    raise RuntimeError("Could not parse Emerald National Pokédex order")
national_symbols = national_symbols[1:387]
if len(national_symbols) != 386:
    raise RuntimeError(f"Expected 386 National species, got {len(national_symbols)}")

learnset_source = LEARNSETS.read_text(encoding="utf-8")
species_learnsets: dict[str, set[str]] = {}
for match in re.finditer(
    r"\[SPECIES_([A-Z0-9_]+)\]\s*=\s*\{\s*\.learnset\s*=\s*\{(.*?)\}\s*\},",
    learnset_source,
    re.S,
):
    species_learnsets[match.group(1)] = set(
        re.findall(r"\.([A-Z0-9_]+)\s*=\s*TRUE", match.group(2))
    )

missing_species = [symbol for symbol in national_symbols if symbol not in species_learnsets]
if missing_species:
    raise RuntimeError(f"National species missing TM/HM learnsets: {missing_species}")

platinum_species = [line.strip().removeprefix("SPECIES_")
                    for line in (PLATINUM / "generated/species.txt").read_text(
                        encoding="utf-8").splitlines() if line.strip()]
if len(platinum_species) <= 493 or platinum_species[387] != "TURTWIG":
    raise RuntimeError("Unexpected Platinum National species order")

legacy_masks = [0]
sinnoh_masks = [0]
unova_masks = [0]
compatibility_count = 0
for species_id in range(1, 808):
    platinum_allowed: set[str] = set()
    if species_id <= 493:
        platinum_data = json.loads(
            (PLATINUM / "res/pokemon" / platinum_species[species_id].lower() / "data.json")
            .read_text(encoding="utf-8")
        )
        platinum_allowed = set(platinum_data["learnset"].get("by_tm", []))
    if species_id <= 386:
        allowed = species_learnsets[national_symbols[species_id - 1]]
        unknown = sorted(allowed.difference(machine_symbols))
        if unknown:
            raise RuntimeError(
                f"Unknown Emerald machines for {national_symbols[species_id - 1]}: {unknown}"
            )
    elif species_id <= 493:
        allowed = platinum_allowed
    mask = 0
    for machine_id, machine_symbol in enumerate(machine_symbols):
        label = (f"TM{machine_id + 1:02d}" if machine_id < 50
                 else f"HM{machine_id - 49:02d}")
        compatible = ((machine_symbol in allowed) if species_id <= 386 else
                      ((label in allowed) if species_id <= 493 else
                       (white_can_learn(species_id, move_ids[machine_symbol])
                        if species_id <= 649 else
                        (y_can_learn(species_id, move_ids[machine_symbol])
                         if species_id <= 721 else
                         usum_can_learn(species_id, move_ids[machine_symbol])))))
        if compatible:
            mask |= 1 << machine_id
            compatibility_count += 1
    legacy_masks.append(mask)

    sinnoh_mask = 0
    for machine_id in range(42):
        compatible = (f"TM{machine_id + 51:02d}" in platinum_allowed
                      if species_id <= 493 else
                      (white_can_learn(species_id, platinum_move_ids[machine_id])
                       if species_id <= 649 else
                       (y_can_learn(species_id, platinum_move_ids[machine_id])
                        if species_id <= 721 else
                        usum_can_learn(species_id, platinum_move_ids[machine_id]))))
        if compatible:
            sinnoh_mask |= 1 << machine_id
            compatibility_count += 1
    sinnoh_masks.append(sinnoh_mask)

    unova_mask = 0
    regional_moves = UNOVA_TM_MOVES + KALOS_TM_MOVES + ALOLA_TM_MOVES
    for machine_id, move_id in enumerate(regional_moves):
        if machine_id < len(UNOVA_TM_MOVES):
            compatible = (white_can_learn(species_id, move_id)
                          if species_id <= 649 else
                          (y_can_learn(species_id, move_id)
                           if species_id <= 721 else
                           usum_can_learn(species_id, move_id)))
        elif machine_id < len(UNOVA_TM_MOVES) + len(KALOS_TM_MOVES):
            compatible = (y_can_learn(species_id, move_id)
                          if species_id <= 721 else
                          usum_can_learn(species_id, move_id))
        else:
            compatible = usum_can_learn(species_id, move_id)
        if compatible:
            unova_mask |= 1 << machine_id
            compatibility_count += 1
    unova_masks.append(unova_mask)

move_rows = []
all_machine_moves = ([move_ids[symbol] for symbol in machine_symbols] +
                     platinum_move_ids + UNOVA_TM_MOVES + KALOS_TM_MOVES +
                     ALOLA_TM_MOVES)
for offset in range(0, len(all_machine_moves), 10):
    move_rows.append("  " + ", ".join(str(value) for value in all_machine_moves[offset:offset + 10]) + ",")
price_values = ([3000] * 50 + [5000] * 8 + platinum_prices +
                UNOVA_TM_PRICES + KALOS_TM_PRICES + ALOLA_TM_PRICES)
price_rows = []
for offset in range(0, len(price_values), 10):
    price_rows.append("  " + ", ".join(str(value) for value in price_values[offset:offset + 10]) + ",")
machine_mask_rows = []
for index, (legacy, sinnoh, regional) in enumerate(
        zip(legacy_masks, sinnoh_masks, unova_masks)):
    mask = legacy | (sinnoh << 58) | (regional << 100)
    machine_mask_rows.append(
        "  {" + ", ".join(
            f"0x{(mask >> (8 * byte)) & 0xFF:02X}" for byte in range(17)) +
        f"}}, // {index:03d}")
payload = "\n".join(move_rows + price_rows + machine_mask_rows)
catalog_hash = hashlib.sha256(payload.encode("utf-8")).hexdigest()[:16]
output = (
    "// Generated from pokeemerald and pokeplatinum machine data.\n"
    f'constexpr char kMachineCatalogId[] = "{catalog_hash}";\n'
    "constexpr uint16_t machineMoves[kMachineCount] = {\n"
    + "\n".join(move_rows)
    + "\n};\n"
    "constexpr uint16_t machinePrices[kMachineCount] = {\n"
    + "\n".join(price_rows)
    + "\n};\n"
    "// National-Dex row followed by one bit for every contiguous machine id.\n"
    "constexpr uint8_t machineLearnsets[808][17] = {\n"
    + "\n".join(machine_mask_rows)
    + "\n};\n"
    "// Unova-only additions retain their canonical TM numbers.\n"
    "constexpr uint8_t unovaMachineNumbers[kUnovaMachineCount] = {\n  "
    + ", ".join(str(number) for number in UNOVA_TM_NUMBERS)
    + ",\n};\n"
    "// Kalos-only additions retain X/Y's canonical TM numbers.\n"
    "constexpr uint8_t kalosMachineNumbers[kKalosMachineCount] = {\n  "
    + ", ".join(str(number) for number in KALOS_TM_NUMBERS)
    + ",\n};\n"
    "// Alola-only additions retain Ultra Sun's canonical TM numbers.\n"
    "constexpr uint8_t alolaMachineNumbers[kAlolaMachineCount] = {\n  "
    + ", ".join(str(number) for number in ALOLA_TM_NUMBERS)
    + ",\n};\n"
    f"constexpr uint16_t machineCompatibilityCount = {compatibility_count};\n"
)
with OUTPUT.open("w", encoding="utf-8", newline="\n") as generated:
    generated.write(output)

# Stamp the owning translation unit because PlatformIO's Windows dependency
# scanner has previously linked stale generated includes in this project.
owner = OWNER.read_text(encoding="utf-8")
stamp = f"// Emerald/Platinum TM/HM catalog: {catalog_hash}"
stamp_pattern = r"// (?:Emerald|Emerald/Platinum) TM/HM catalog: [0-9a-f]{16}"
stamped = re.sub(stamp_pattern, stamp, owner) if re.search(stamp_pattern, owner) else owner.rstrip() + f"\n\n{stamp}\n"
if stamped != owner:
    with OWNER.open("w", encoding="utf-8", newline="\n") as source:
        source.write(stamped)

print(
    f"Generated {len(all_machine_moves)} machines ({len(platinum_move_ids)} Sinnoh + "
    f"{len(UNOVA_TM_MOVES)} Unova + {len(KALOS_TM_MOVES)} Kalos + "
    f"{len(ALOLA_TM_MOVES)} Alola TMs) and {compatibility_count} "
    f"species-machine compatibilities through Alola "
    f"({catalog_hash})"
)
