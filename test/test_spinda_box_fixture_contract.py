#!/usr/bin/env python3
"""Keep the physical three-Spinda save fixture isolated and idempotent."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
PLATFORMIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")


assert "[env:spinda-box-fixture]" in PLATFORMIO
assert "-D POKEGOCHI_SPINDA_BOX_FIXTURE=1" in PLATFORMIO
production = PLATFORMIO.split("[env:esp32-2432S028R]", 1)[1].split("[env:", 1)[0]
assert "POKEGOCHI_SPINDA_BOX_FIXTURE" not in production

guard = "#if defined(POKEGOCHI_SPINDA_BOX_FIXTURE)"
assert MAIN.count(guard) == 1
fixture = MAIN.split(guard, 1)[1].split("#endif", 1)[0]
for token in (
    "line[0] == 'A'",
    "line[9] == 'S'",
    "line[10] == '\\0'",
    "0x00000000U",
    "0x12345678U",
    "0x89ABCDEFU",
    "existing.speciesId == 327U",
    "existing.personality == personality",
    "pokemon.personality = personality",
    "pokemon.nature = static_cast<PokemonNature>(personality % 25U)",
    "CollectionLogic::refreshAbility(pokemon, 7U)",
    "CollectionLogic::add(gameSave.collection, pokemon, &assignedUid)",
    "saveNow()",
):
    assert token in fixture, token

assert "gameSave.collection.party" not in fixture
print("Spinda Box fixture contract passed.")
