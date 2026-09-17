"""Generate the Pokegochi TM/HM catalog and compatibility table from Emerald.

The firmware uses National Pokédex numbers, while pokeemerald's internal
species IDs are deliberately non-National after Celebi.  This generator joins
the two datasets by their SPECIES_* symbol so Treecko through Deoxys cannot be
silently assigned another Pokémon's learnset.
"""
from __future__ import annotations

import hashlib
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
EMERALD = ROOT / ".downloads/pokeemerald-tree/pokeemerald-master"
TMHM_CONSTANTS = EMERALD / "include/constants/tms_hms.h"
MOVE_CONSTANTS = EMERALD / "include/constants/moves.h"
POKEDEX_CONSTANTS = EMERALD / "include/constants/pokedex.h"
LEARNSETS = EMERALD / "src/data/pokemon/tmhm_learnsets.h"
OUTPUT = ROOT / "src/game/MachineDataGenerated.inc"
OWNER = ROOT / "src/game/Economy.cpp"


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

masks = [0]
compatibility_count = 0
for species_symbol in national_symbols:
    allowed = species_learnsets[species_symbol]
    unknown = sorted(allowed.difference(machine_symbols))
    if unknown:
        raise RuntimeError(f"Unknown machines for {species_symbol}: {unknown}")
    mask = 0
    for machine_id, machine_symbol in enumerate(machine_symbols):
        if machine_symbol in allowed:
            mask |= 1 << machine_id
            compatibility_count += 1
    masks.append(mask)

move_rows = []
for offset in range(0, len(machine_symbols), 10):
    move_rows.append(
        "  " + ", ".join(str(move_ids[symbol]) for symbol in machine_symbols[offset:offset + 10]) + ","
    )
mask_rows = [f"  UINT64_C(0x{mask:016X}), // {index:03d}" for index, mask in enumerate(masks)]
payload = "\n".join(move_rows + mask_rows)
catalog_hash = hashlib.sha256(payload.encode("utf-8")).hexdigest()[:16]
output = (
    "// Generated from pokeemerald's tms_hms.h and tmhm_learnsets.h.\n"
    f'constexpr char kMachineCatalogId[] = "{catalog_hash}";\n'
    "constexpr uint16_t machineMoves[kMachineCount] = {\n"
    + "\n".join(move_rows)
    + "\n};\n"
    "// Index is the National Pokédex number; bit is TM01..TM50, HM01..HM08.\n"
    "constexpr uint64_t machineLearnsets[387] = {\n"
    + "\n".join(mask_rows)
    + "\n};\n"
    f"constexpr uint16_t machineCompatibilityCount = {compatibility_count};\n"
)
OUTPUT.write_text(output, encoding="utf-8", newline="\n")

# Stamp the owning translation unit because PlatformIO's Windows dependency
# scanner has previously linked stale generated includes in this project.
owner = OWNER.read_text(encoding="utf-8")
stamp = f"// Emerald TM/HM catalog: {catalog_hash}"
stamp_pattern = r"// Emerald TM/HM catalog: [0-9a-f]{16}"
stamped = re.sub(stamp_pattern, stamp, owner) if re.search(stamp_pattern, owner) else owner.rstrip() + f"\n\n{stamp}\n"
if stamped != owner:
    OWNER.write_text(stamped, encoding="utf-8", newline="\n")

print(
    f"Generated {len(machine_symbols)} Emerald machines and "
    f"{compatibility_count} species-machine compatibilities ({catalog_hash})"
)
