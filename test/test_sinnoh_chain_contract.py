"""Regression contract for 1-50 sequential encounters in Sinnoh EXPLORE."""

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

assert "kMaximumEncounters = 50" in CHAIN
assert 'sinnohEncounterPending ? "SINNOH EXPLORE"' in selector
assert '"%s - CHOOSE 1 TO 50"' in selector
assert "sinnohEncounterSite = sinnohSiteCursor;" in touch
assert "screen = Screen::WildChainSelect;" in touch
assert "const bool returnToSinnoh = sinnohEncounterPending;" in touch
assert "WildChainSystem::begin(wildChain, pendingWildEncounterCount" in touch
assert "const bool launchInSinnoh = sinnohEncounterPending;" in touch
assert "static_cast<SinnohSite>(sinnohEncounterSite)" in touch
assert "const bool completedSinnoh = sinnohBattleActive;" in touch
assert "completedSinnoh\n            ? launchSinnohEncounter(" in touch
assert '"A WILD %s APPEARED! (%u/%u)"' in launch_sinnoh
assert "SinnohExpedition::recordBattle(" in MAIN

print("Sinnoh EXPLORE chain contract passed (1-50, fixed site, Sinnoh return).")
