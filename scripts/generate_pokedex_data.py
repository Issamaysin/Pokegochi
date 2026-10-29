"""Generate all 721 National Pokedex rows through Pokemon Y/Kalos."""
from __future__ import annotations

import re
import json
from pathlib import Path
from text_normalization import fire_red_ascii
from unova_rom_data import expansion_text

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / ".downloads/pokefirered-tree/pokefirered-master/src/data/pokemon"
PLATINUM = ROOT / ".downloads/pokeplatinum-tree"
OUT = ROOT / "src/game/PokedexDataGenerated.inc"


def c_string(value: str) -> str:
    value = fire_red_ascii(value, "Pokedex text")
    value = value.replace("POKÃ©MON", "POKEMON").replace("POKéMON", "POKEMON")
    value = value.replace("\\n", " ").replace("\n", " ")
    value = re.sub(r"\s+", " ", value).strip()
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


entries_source = (DATA / "pokedex_entries.h").read_text(encoding="utf-8")
text_source = (DATA / "pokedex_text_fr.h").read_text(encoding="utf-8")

texts: dict[str, str] = {}
for match in re.finditer(r"const u8 (g\w+PokedexText)\[\]\s*=\s*_\((.*?)\);", text_source, re.S):
    pieces = re.findall(r'"((?:\\.|[^"\\])*)"', match.group(2))
    texts[match.group(1)] = "".join(pieces)

rows = []
blocks = re.finditer(r"\[NATIONAL_DEX_([A-Z0-9_]+)\]\s*=\s*\{(.*?)\n\s*\},", entries_source, re.S)
for match in blocks:
    symbol, body = match.groups()
    if symbol == "NONE":
        continue
    category = re.search(r'\.categoryName\s*=\s*_\("([^"]+)"\)', body)
    height = re.search(r"\.height\s*=\s*(\d+)", body)
    weight = re.search(r"\.weight\s*=\s*(\d+)", body)
    description = re.search(r"\.description\s*=\s*(g\w+PokedexText)", body)
    if not all((category, height, weight, description)):
        continue
    rows.append((category.group(1), int(height.group(1)), int(weight.group(1)), texts.get(description.group(1), "")))
    if len(rows) == 386:
        break

if len(rows) != 386:
    raise RuntimeError(f"Expected 386 National Pokedex rows, found {len(rows)}")

species_symbols = [line.strip().removeprefix("SPECIES_")
                   for line in (PLATINUM / "generated/species.txt").read_text(
                       encoding="utf-8").splitlines() if line.strip()]
if len(species_symbols) <= 493 or species_symbols[387] != "TURTWIG":
    raise RuntimeError("Unexpected Platinum National Dex order")
for species_id in range(387, 494):
    symbol = species_symbols[species_id]
    source = json.loads(
        (PLATINUM / "res/pokemon" / symbol.lower() / "data.json").read_text(
            encoding="utf-8"))
    dex = source["pokedex_data"]["en"]
    # Platinum stores Imperial Pokedex values. The native games display the
    # same underlying metric values rounded to decimetres/hectograms.
    height = round(float(source["pokedex_data"]["height_inches"]) * 0.254)
    weight = round(float(source["pokedex_data"]["weight_pounds"]) * 4.5359237)
    description = " ".join(str(line) for line in dex["entry_text"])
    rows.append((dex["category"], int(height), int(weight), description))

if len(rows) != 493:
    raise RuntimeError(f"Expected 493 National Pokedex rows, found {len(rows)}")

gen5_source = expansion_text("src/data/pokemon/species_info/gen_5_families.h")
expansion_species = expansion_text("include/constants/species.h")
gen5_symbols = {
    int(number): symbol
    for symbol, number in re.findall(
        r"\bSPECIES_([A-Z0-9_]+)\s*=\s*(\d+)", expansion_species)
    if 494 <= int(number) <= 649
}
for species_id in range(494, 650):
    symbol = gen5_symbols[species_id]
    if symbol == "GENESECT":
        rows.append((
            "Paleozoic", 15, 825,
            "Over 300 million years ago, it was feared as the strongest of hunters. "
            "It has been modified by Team Plasma.",
        ))
        continue
    start_match = re.search(rf"\[SPECIES_{symbol}\]\s*=\s*\{{", gen5_source)
    if not start_match:
        raise RuntimeError(f"Missing Gen-V Pokedex source for {symbol}")
    next_match = re.search(r"\n\s*\[SPECIES_[A-Z0-9_]+\]\s*=", gen5_source[start_match.end():])
    end = start_match.end() + next_match.start() if next_match else len(gen5_source)
    body = gen5_source[start_match.end():end]
    category = re.search(r'\.categoryName\s*=\s*_\("([^"]+)"\)', body)
    height = re.search(r"\.height\s*=\s*(\d+)", body)
    weight = re.search(r"\.weight\s*=\s*(\d+)", body)
    description_block = re.search(r"\.description\s*=\s*COMPOUND_STRING\((.*?)\)\s*,",
                                  body, re.S)
    if not all((category, height, weight, description_block)):
        raise RuntimeError(f"Incomplete Gen-V Pokedex metadata for {symbol}")
    description = " ".join(
        piece.replace("\\n", " ")
        for piece in re.findall(r'"([^"]*)"', description_block.group(1))
    )
    rows.append((category.group(1), int(height.group(1)),
                 int(weight.group(1)), description))

if len(rows) != 649:
    raise RuntimeError(f"Expected 649 National Pokedex rows, found {len(rows)}")

gen6_source = expansion_text("src/data/pokemon/species_info/gen_6_families.h")
gen1_source = expansion_text("src/data/pokemon/species_info/gen_1_families.h")
# Sylveon is authored beside the rest of the Eevee family, while most Kalos
# species live in gen_6_families.h. Search both sources without duplicating
# the data by hand.
gen6_direct_source = gen6_source + "\n" + gen1_source
shared_dex_source = expansion_text("src/data/pokemon/species_info/shared_dex_text.h")
shared_dex_descriptions = {}
for match in re.finditer(
        r"const u8\s+(g\w+PokedexText)\[\]\s*=\s*_\((.*?)\);",
        shared_dex_source, re.S):
    shared_dex_descriptions[match.group(1)] = "".join(
        piece.replace("\\n", " ")
        for piece in re.findall(r'"([^"]*)"', match.group(2))
    )
gen6_symbols = {
    int(number): symbol
    for symbol, number in re.findall(
        r"\bSPECIES_([A-Z0-9_]+)\s*=\s*(\d+)", expansion_species)
    if 650 <= int(number) <= 721
}
# These families are expanded from C preprocessor macros in the upstream
# source, so there is no literal SPECIES_* block for the base form to parse.
# Keep only the macro arguments here; the descriptions still come from the
# shared upstream Pokedex text table above.
gen6_macro_metadata = {
    "SCATTERBUG_ICY_SNOW": ("Scatterdust", 3, 25, "gScatterbugPokedexText"),
    "SPEWPA_ICY_SNOW": ("Scatterdust", 3, 84, "gSpewpaPokedexText"),
    "FURFROU_NATURAL": ("Poodle", 12, 280, "gFurfrouPokedexText"),
}
# Vivillon keeps its form-specific description in a literal species block,
# but inherits these three fields from VIVILLON_MISC_INFO.
gen6_inherited_metadata = {
    "VIVILLON_ICY_SNOW": ("Scale", 12, 170),
    "FLABEBE_RED": ("Single Bloom", 1, 1),
    "FLOETTE_RED": ("Single Bloom", 2, 9),
    "FLORGES_RED": ("Garden", 11, 100),
}
for species_id in range(650, 722):
    symbol = gen6_symbols[species_id]
    macro_metadata = gen6_macro_metadata.get(symbol)
    if macro_metadata:
        category, height, weight, description_name = macro_metadata
        description = shared_dex_descriptions.get(description_name, "")
        if not description:
            raise RuntimeError(f"Missing shared Gen-VI Pokedex text for {symbol}")
        rows.append((category, height, weight, description))
        continue
    start_match = re.search(rf"\[SPECIES_{symbol}\]\s*=\s*\{{", gen6_direct_source)
    if not start_match:
        raise RuntimeError(f"Missing Gen-VI Pokedex source for {symbol}")
    next_match = re.search(r"\n\s*\[SPECIES_[A-Z0-9_]+\]\s*=", gen6_direct_source[start_match.end():])
    end = start_match.end() + next_match.start() if next_match else len(gen6_direct_source)
    body = gen6_direct_source[start_match.end():end]
    category = re.search(r'\.categoryName\s*=\s*_\("([^"]+)"\)', body)
    height = re.search(r"\.height\s*=\s*(\d+)", body)
    weight = re.search(r"\.weight\s*=\s*(\d+)", body)
    description_block = re.search(r"\.description\s*=\s*COMPOUND_STRING\((.*?)\)\s*,",
                                  body, re.S)
    description_symbol = re.search(r"\.description\s*=\s*(g\w+PokedexText)", body)
    inherited_metadata = gen6_inherited_metadata.get(symbol)
    if not inherited_metadata and not all((category, height, weight)):
        raise RuntimeError(f"Incomplete Gen-VI Pokedex metadata for {symbol}")
    if not (description_block or description_symbol):
        raise RuntimeError(f"Incomplete Gen-VI Pokedex metadata for {symbol}")
    if description_block:
        description = " ".join(
            piece.replace("\\n", " ")
            for piece in re.findall(r'"([^"]*)"', description_block.group(1))
        )
    else:
        description = shared_dex_descriptions.get(description_symbol.group(1), "")
        if not description:
            raise RuntimeError(f"Missing shared Gen-VI Pokedex text for {symbol}")
    if inherited_metadata:
        category_name, height_value, weight_value = inherited_metadata
    else:
        category_name = category.group(1)
        height_value = int(height.group(1))
        weight_value = int(weight.group(1))
    rows.append((category_name, height_value, weight_value, description))

if len(rows) != 721:
    raise RuntimeError(f"Expected 721 National Pokedex rows, found {len(rows)}")

lines = ["static constexpr PokedexEntryData kPokedexEntries[] = {"]
for category, height, weight, description in rows:
    lines.append(f"  {{{c_string(category)}, {height}, {weight}, {c_string(description)}}},")
lines.append("};")
OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"Generated {len(rows)} National Pokedex entries")
