"""Source-level contract for Rare Candy and regional evolution wiring."""

from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")


def source_between(signature: str, next_signature: str) -> str:
    start = MAIN.index(signature)
    end = MAIN.index(next_signature, start + len(signature))
    return MAIN[start:end]


rare_candy = source_between(
    "bool useRareCandyOutsideBattle(OwnedPokemon& target)",
    "// Shared by the Party cards and the PC storage grid.",
)
assert "CollectionLogic::applyRareCandy" in rare_candy
assert "unlockedContentGeneration()" in rare_candy
assert "gameSave.gymProgress.unlockedGeneration" not in rare_candy
assert "target.level>=100U" not in rare_candy

evolution_resolution = source_between(
    "void resolvePendingEvolution(uint8_t choiceIndex)",
    "void resetMoveLearnDescriptionScroll()",
)
assert "unlockedContentGeneration()" in evolution_resolution

wild_launch = source_between(
    "bool launchWildEncounter(uint32_t playerUid)",
    "bool launchSinnohEncounter(uint32_t playerUid, SinnohSite site)",
)
assert "unlockedContentGeneration()" in wild_launch

assert (
    "gameSave.collection,pokemon->uid,micros(),unlockedContentGeneration());"
    in MAIN
)
assert "gameSave.battle.unlockedGeneration = unlockedContentGeneration();" in MAIN

print("Evolution progression contract passed.")
