#!/usr/bin/env python3
"""Keep the eight Unova Mega forms complete across engine and asset pack."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MEGA = (ROOT / "src/game/MegaEvolution.cpp").read_text(encoding="utf-8")
ENGINE = (ROOT / "src/game/BattleEngine.cpp").read_text(encoding="utf-8")
PACKER = (ROOT / "scripts/build_sd_asset_pack.py").read_text(encoding="utf-8")
PACK_VERSION = (ROOT / "include/config/AssetPackVersion.h").read_text(
    encoding="ascii")
EXPANSION = ROOT / ".downloads/pokeemerald-expansion/graphics/pokemon"

FORMS = {
    500: "emboar",
    530: "excadrill",
    531: "audino",
    545: "scolipede",
    560: "scrafty",
    604: "eelektross",
    609: "chandelure",
    623: "golurk",
}

for species, folder_name in FORMS.items():
    assert f"{{{species},MegaVariant::Mega," in MEGA, species
    assert f'({species},"{folder_name}","mega")' in PACKER, species
    folder = EXPANSION / folder_name / "mega"
    for required in ("front.png", "back.png", "icon.png",
                     "normal.pal", "shiny.pal"):
        assert (folder / required).is_file(), folder / required

for ability in ("PIERCING DRILL", "EELEVATE", "UNSEEN FIST"):
    assert f'"{ability}"' in MEGA
    assert f'"{ability}"' in ENGINE

assert ENGINE.count("contactBypassesProtection(") >= 6
assert ENGINE.count("hasGroundImmunityAbility(") >= 8
assert "groundImmunityAbilityApplies" in ENGINE
assert 'abilityIs(attacker,"EELEVATE")' in ENGINE
assert '"--gen5-mega-assets-only"' in PACKER
assert "kRequiredAssetPackVersion = 46" in PACK_VERSION

print("Generation 5 Mega contract passed: 8 forms, 3 abilities, 64 assets.")
