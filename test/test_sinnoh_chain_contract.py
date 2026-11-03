"""Regression contract for shared 1-50 Sinnoh and regional wild chains."""

from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CHAIN = (ROOT / "include/game/WildChainSystem.h").read_text(encoding="utf-8")


def source_between(signature: str, next_signature: str) -> str:
    start = MAIN.index(signature)
    end = MAIN.index(next_signature, start + len(signature))
    return MAIN[start:end]


selector = source_between("void drawWildChainSelect() {", "bool launchWildEncounter(")
touch = source_between(
    "void handleTap(const TouchPoint& p) {", "void redrawCurrentScreen() {"
)
launch_sinnoh = source_between(
    "bool launchSinnohEncounter(uint32_t playerUid, SinnohSite site) {",
    "uint32_t firstReadyPartyUid(",
)
presentation = source_between(
    "bool presentStartedWildEncounter(", "bool launchWildEncounter("
)

assert "kMaximumEncounters = 50" in CHAIN
assert 'sinnohEncounterPending ? "SINNOH EXPLORE"' in selector
assert '"%s - CHOOSE 1 TO 50"' in selector
assert "regionalWildPendingGeneration ? regionalWildTitle(" in selector
assert '"GEN %u - CHOOSE 1 TO 50"' in selector
assert "sinnohEncounterSite = sinnohSiteCursor;" in touch
assert "screen = Screen::WildChainSelect;" in touch
assert "const bool returnToSinnoh = sinnohEncounterPending;" in touch
assert "const uint8_t returnToGeneration = regionalWildPendingGeneration;" in touch
assert "WildChainSystem::begin(wildChain, pendingWildEncounterCount" in touch
assert "const bool launchInSinnoh = sinnohEncounterPending;" in touch
assert "const uint8_t regionalGeneration = regionalWildPendingGeneration;" in touch
assert "static_cast<SinnohSite>(sinnohEncounterSite)" in touch
assert "const bool completedSinnoh = sinnohBattleActive;" in touch
assert "const uint8_t completedRegionalWild = regionalWildBattleGeneration;" in touch
assert "launchRegionalWildEncounter(" in touch
assert "openRegionalWildSelector(5U)" in touch
assert "openRegionalWildSelector(6U)" in touch
assert "openRegionalWildSelector(7U)" in touch
assert "UnovaAdventure::allAreasCompleted(gameSave.unova)" in MAIN
assert "KalosAdventure::fullyCompleted(gameSave.kalos)" in MAIN
assert "AlolaAdventure::fullyCompleted(gameSave.alola)" in MAIN
assert '"GEN VII - NO ULTRA BEASTS - 1 TO 50"' in MAIN
assert "BattleEngine::startRegionalWild(" in launch_sinnoh
assert '"A WILD %s APPEARED! (%u/%u)"' in presentation
assert "SinnohExpedition::recordBattle(" in MAIN

print("Shared chain contract passed: Sinnoh and Gen V-VII regional wilds use 1-50 encounters.")
