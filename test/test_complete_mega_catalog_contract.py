#!/usr/bin/env python3
"""Require every authored Mega/Primal through Alola to be fully integrated."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
EXPANSION = ROOT / ".downloads/pokeemerald-expansion"
POKEMON_ART = EXPANSION / "graphics/pokemon"
POKEDEX = (EXPANSION / "include/constants/pokedex.h").read_text(
    encoding="utf-8")
MEGA = (ROOT / "src/game/MegaEvolution.cpp").read_text(encoding="utf-8")
PACKER = (ROOT / "scripts/build_sd_asset_pack.py").read_text(encoding="utf-8")
RENDERER = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
PACK_VERSION = (ROOT / "include/config/AssetPackVersion.h").read_text(
    encoding="ascii")

NATIONAL_SYMBOLS = re.findall(
    r"^\s*NATIONAL_DEX_([A-Z0-9_]+),", POKEDEX, re.M)
SYMBOL_TO_ID = {symbol.lower(): number
                for number, symbol in enumerate(NATIONAL_SYMBOLS)}

PACKED_FORMS = {
    (int(species), variant)
    for species, _folder, variant in re.findall(
        r'\((\d+),\s*"([^"]+)",\s*"([^"]+)"(?:,\s*"[^"]+")?\)',
        PACKER)
}

VARIANT_ENUM = {
    "mega": "Mega", "mega_x": "MegaX", "mega_y": "MegaY",
    "mega_z": "MegaZ", "primal": "Primal",
}

authored_forms = set()
for species_folder in POKEMON_ART.iterdir():
    if not species_folder.is_dir() or species_folder.name not in SYMBOL_TO_ID:
        continue
    species = SYMBOL_TO_ID[species_folder.name]
    if species > 807:  # Pokegochi currently ends at Ultra Sun/Ultra Moon.
        continue
    for form_folder in species_folder.iterdir():
        variant = form_folder.name
        if not form_folder.is_dir() or variant not in VARIANT_ENUM:
            continue
        authored_forms.add((species, variant))
        assert (species, variant) in PACKED_FORMS, (species, variant)
        enum_name = VARIANT_ENUM[variant]
        assert re.search(
            rf"\{{{species},MegaVariant::{enum_name},", MEGA), (species, variant)
        assert ((form_folder / "front.png").is_file() or
                (form_folder / "anim_front.png").is_file()), form_folder
        for required in ("back.png", "icon.png", "normal.pal", "shiny.pal"):
            assert (form_folder / required).is_file(), form_folder / required

assert len(authored_forms) == 90, len(authored_forms)
assert MEGA.count("MegaVariant::") >= 90

# Original Color Magearna is a persistent base form and needs separate Mega
# pixels even though it shares the same combat row as regular Magearna.
original_mega = POKEMON_ART / "magearna/original_color/mega"
for required in ("front.png", "back.png", "icon.png",
                 "normal.pal", "shiny.pal"):
    assert (original_mega / required).is_file(), original_mega / required
assert '(801,"magearna/original_color","mega","mega_original")' in PACKER
assert 'species==801U&&mega==MegaVariant::Mega&&' in RENDERER
assert '?"mega_original":megaForm->assetVariant' in RENDERER

# These were the exact omissions found by the full source/catalog comparison.
expected_additions = {
    (398, "Mega"), (445, "MegaZ"), (448, "MegaZ"),
    (478, "Mega"), (485, "Mega"), (491, "Mega"),
    (740, "Mega"), (768, "Mega"), (780, "Mega"),
    (801, "Mega"), (807, "Mega"),
}
for species, variant in expected_additions:
    assert re.search(rf"\{{{species},MegaVariant::{variant},", MEGA)

version = re.search(r"kRequiredAssetPackVersion\s*=\s*(\d+)", PACK_VERSION)
assert version and int(version.group(1)) >= 53
assert '"--missing-mega-assets-only"' in PACKER

print("Complete Mega catalog contract passed: 90 forms plus Original Color Magearna.")
