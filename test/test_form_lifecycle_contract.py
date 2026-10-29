#!/usr/bin/env python3
"""Regression contract for persistent/timed forms and personality graphics."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
COLLECTION = (ROOT / "src/game/Collection.cpp").read_text(encoding="utf-8")
MEGA = (ROOT / "src/game/MegaEvolution.cpp").read_text(encoding="utf-8")
ENGINE = (ROOT / "src/game/BattleEngine.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
RENDERER = (ROOT / "src/drivers/AssetRenderer.cpp").read_text(encoding="utf-8")
PACKER = (ROOT / "scripts/build_sd_asset_pack.py").read_text(encoding="utf-8")
ASSETS = ROOT / ".generated/sdcard/pokegochi/assets"

assert "SINNOH_EXPANSION_FORMS" in PACKER
assert '(412, "burmy", ("", "sandy", "trash"))' in PACKER
assert '(413, "wormadam", ("", "sandy", "trash"))' in PACKER
assert '(422, "shellos", ("", "east"))' in PACKER
assert '(423, "gastrodon", ("", "east"))' in PACKER
assert "applyBurmyCloak" in COLLECTION
assert "pokemon.speciesId == 412U || pokemon.speciesId == 413U" in COLLECTION
assert "pokemon.speciesId == 422U || pokemon.speciesId == 423U" in COLLECTION
assert '"WEST SEA", "EAST SEA"' in COLLECTION
assert '"/pokegochi/assets/pokemon/forms/sinnoh/%03u/%u/%s%s.pkg"' in MAIN

assert "kFurfrouTrimHours = 5U * 24U" in COLLECTION
assert "kHoopaUnboundHours = 3U * 24U" in COLLECTION
assert "kHoopaCooldownHours = 3U * 24U" in COLLECTION
assert "setFormForWorld" in COLLECTION
assert "reconcileFormLifecycles" in COLLECTION
assert "PrisonBottleState::Cooldown" in COLLECTION
assert '"/pokegochi/assets/kalos/key_items/prison_bottle.pkg"' in MAIN
assert "usePrisonBottleFromInventory" in MAIN

assert "case PokemonType::Fairy: return 17U;" in COLLECTION
assert "if (speciesId == 493) return 18U;" in COLLECTION
assert '"FAIRY"' in COLLECTION
for relative in (
    "pokemon/forms/arceus/17/front.pkg",
    "pokemon/forms/arceus/17/back.pkg",
    "pokemon/forms/arceus/17/icons/0.pkg",
    "kalos/key_items/prison_bottle.pkg",
):
    assert (ASSETS / relative).is_file(), relative

for species, count in ((412, 3), (413, 3), (422, 2), (423, 2)):
    for form in range(count):
        prefix = ASSETS / f"pokemon/forms/sinnoh/{species:03}/{form}"
        for relative in ("front.pkg", "back.pkg", "icons/0.pkg", "icons/1.pkg"):
            assert (prefix / relative).is_file(), prefix / relative

assert "drawIndexedSpinda" in RENDERER
assert "spindaSpotPixel" in RENDERER
assert "paletteIndex + 4U" in RENDERER
assert "AssetRenderer::drawSpinda" in MAIN
assert "pokemon.personality" in MAIN

assert "speciesId!=384U&&canTransform(speciesId)" in MEGA
assert "kRayquazaBattleMegaMask" in MEGA
assert "activateRayquazaBattleMega" in ENGINE
assert "move->id==620U" in ENGINE
assert "BattleWeather::StrongWinds" in ENGINE

print("Form lifecycle contract passed: Sinnoh forms, timed Hoopa/Furfrou, Spinda, Arceus and Rayquaza.")
