#!/usr/bin/env python3
"""Keep the authored Kalos Megas and late Generation-VI forms complete."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MEGA = (ROOT / "src/game/MegaEvolution.cpp").read_text(encoding="utf-8")
ENGINE = (ROOT / "src/game/BattleEngine.cpp").read_text(encoding="utf-8")
COLLECTION = (ROOT / "src/game/Collection.cpp").read_text(encoding="utf-8")
POKEMON_DATA = (ROOT / "src/game/PokemonData.cpp").read_text(encoding="utf-8")
RENDERER = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
PACKER = (ROOT / "scripts/build_sd_asset_pack.py").read_text(encoding="utf-8")
EXPANSION = ROOT / ".downloads/pokeemerald-expansion/graphics/pokemon"

MEGAS = {
    652: "chesnaught", 655: "delphox", 658: "greninja",
    668: "pyroar", 670: "floette", 678: "meowstic",
    687: "malamar", 689: "barbaracle", 691: "dragalge",
    701: "hawlucha", 718: "zygarde", 719: "diancie",
}

for species, folder_name in MEGAS.items():
    assert f"{{{species},MegaVariant::Mega," in MEGA, species
    assert f'({species},"{folder_name}","mega")' in PACKER, species
    folder = EXPANSION / folder_name / "mega"
    for required in ("front.png", "back.png", "icon.png",
                     "normal.pal", "shiny.pal"):
        assert (folder / required).is_file(), folder / required

assert '(718, "zygarde", ("", "10_percent", "complete"))' in PACKER
assert '(720, "hoopa", ("", "unbound"))' in PACKER
assert "if(speciesId==718U)return 3U;" in COLLECTION
assert "if(speciesId==720U)return 2U;" in COLLECTION
assert 'form==2U?"COMPLETE":"50 PERCENT"' in COLLECTION
assert 'return form?"UNBOUND":"CONFINED"' in COLLECTION
assert "kDeoxysAttackLearnset" in POKEMON_DATA
assert "kDeoxysDefenseLearnset" in POKEMON_DATA
assert "kDeoxysSpeedLearnset" in POKEMON_DATA
assert "kEternalFloetteLearnset" in POKEMON_DATA
assert "kHoopaUnboundLearnset" in POKEMON_DATA
assert '"HYPERSPACE FURY"' in POKEMON_DATA
assert '"CORE ENFORCER"' in POKEMON_DATA
assert '"NIHIL LIGHT"' in POKEMON_DATA
assert "relearnableMovesForPokemon" in POKEMON_DATA
assert "synchronizeSignatureMove" in MEGA
assert "CollectionLogic::resolvedForm(pokemon)!=2U" in MEGA
assert "resetBattleTriggeredForms" in ENGINE
assert "updateZygardeAssembly" in ENGINE
assert "refreshDerivedStatsPreservingHpRatio" in ENGINE
assert "refreshDerivedStatsPreservingHpRatio" in COLLECTION
assert "pokemon.currentHp)*10U" in ENGINE
assert "pokemon.currentHp)*2U" in ENGINE
assert 'effectIs(move,"NIHIL_LIGHT")' in ENGINE
assert "species==716U||species==718U||species==720U" in RENDERER
assert "uint8_t playerForm = 0;" in RENDERER
assert "uint16_t playerMaximumHp = 0;" in RENDERER
assert "snapshot.playerForm = CollectionLogic::resolvedForm(*player);" in RENDERER
assert "snapshot.playerMaximumHp = player->maximumHp;" in RENDERER
assert "battleFlow.displayPlayerForm = playerFormBefore;" in RENDERER
assert "battleFlow.displayPlayerMaximumHp = playerMaximumHpBefore;" in RENDERER
assert 'abilityIs(attacker,"FIRE MANE")' in ENGINE
assert 'attackType==PokemonType::Fire)power=power*130U/100U' in ENGINE
assert "canHoldMegaStone" in MEGA
assert "speciesId!=384U&&canTransform(speciesId)" in MEGA
assert "activateRayquazaBattleMega" in ENGINE
assert "move->id==620U" in ENGINE
assert "kRayquazaBattleMegaMask" in MEGA
assert "usePrisonBottle" in COLLECTION
assert "kHoopaUnboundHours = 3U * 24U" in COLLECTION
assert "kHoopaCooldownHours = 3U * 24U" in COLLECTION
assert '"kalos/key_items/prison_bottle.pkg"' in PACKER

print("Generation 6 form contract passed: 12 Megas, Zygarde and Hoopa forms.")
