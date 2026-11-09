from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "include" / "game" / "BattleEngine.h").read_text(
    encoding="utf-8"
)


def between(start: str, end: str) -> str:
    begin = MAIN.index(start)
    finish = MAIN.index(end, begin)
    return MAIN[begin:finish]


assert "static_assert(sizeof(WildEncounterClock) == 16U" in HEADER
assert "firstMasterBallSurpriseCountdown" in HEADER
assert "firstMasterBallSurpriseSchema" in HEADER

history = between("bool masterBallHasBeenReceived()", "bool reconcileUnovaContent()")
for evidence in (
    "PokedexRewards::wasGranted",
    "GymSystem::kRegionRewardFirstBit",
    "SinnohExpedition::kMasterBallRewardBit",
    "UnovaAdventure::kMasterBallRewardBit",
    "KalosAdventure::kMasterBallRewardBit",
    "AlolaAdventure::kMasterBallRewardBit",
):
    assert evidence in history

normal = between(
    "bool launchWildEncounter(uint32_t playerUid)",
    "bool launchSinnohEncounter(uint32_t playerUid, SinnohSite site)",
)
sinnoh = between(
    "bool launchSinnohEncounter(uint32_t playerUid, SinnohSite site)",
    "bool launchRegionalWildEncounter(uint32_t playerUid, uint8_t generation)",
)
regional = between(
    "bool launchRegionalWildEncounter(uint32_t playerUid, uint8_t generation)",
    "uint32_t firstReadyPartyUid",
)

for launch in (normal, sinnoh, regional):
    assert "firstMasterBallSurpriseDueForNextWild()" in launch
    assert "recordFirstMasterBallWildEncounter(surpriseLegendary);" in launch

assert "surpriseLegendary)) return false;" in normal
assert "surpriseLegendary)) return false;" in sinnoh
assert "surpriseLegendary ? 1U" in regional

save_now = between("void saveNow() {", "void serviceDeferredHomeGiftSave()")
assert "reconcileFirstMasterBallSurprise" in save_now
assert "masterBallHasBeenReceived()" in save_now

print("First Master Ball surprise contract passed.")
