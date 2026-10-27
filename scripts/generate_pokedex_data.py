"""Generate all 649 National Pokedex rows from FireRed, Platinum and Unova."""
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

lines = ["static constexpr PokedexEntryData kPokedexEntries[] = {"]
for category, height, weight, description in rows:
    lines.append(f"  {{{c_string(category)}, {height}, {weight}, {c_string(description)}}},")
lines.append("};")
OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"Generated {len(rows)} National Pokedex entries")
