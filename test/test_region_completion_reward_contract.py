from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GYM_H = (ROOT / "include/game/GymSystem.h").read_text(encoding="utf-8")
SINNOH_H = (ROOT / "include/game/SinnohExpedition.h").read_text(encoding="utf-8")
SINNOH_CPP = (ROOT / "src/game/SinnohExpedition.cpp").read_text(encoding="utf-8")
UNOVA_H = (ROOT / "include/game/UnovaAdventure.h").read_text(encoding="utf-8")
UNOVA_CPP = (ROOT / "src/game/UnovaAdventure.cpp").read_text(encoding="utf-8")
KALOS_H = (ROOT / "include/game/KalosAdventure.h").read_text(encoding="utf-8")
KALOS_CPP = (ROOT / "src/game/KalosAdventure.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")

# Kanto--Hoenn use a persistent ledger in spare GymProgress bits. New saves
# opt into the schema immediately, while old completed saves migrate without
# duplicating the League reward that older builds already delivered.
assert "kRegionRewardFirstBit = 3U" in GYM_H
assert "kRegionRewardSchemaBit = 1U << 6U" in GYM_H
assert "reconcileRegionCompletionRewards" in GYM_H

# Sinnoh's former aggregate battle counter now preserves both a capped count
# and one clear bit for every repeatable EXPLORE site.
for marker in (
    "kExplorationSchemaBit",
    "hasExplored",
    "allSitesExplored",
    "fullyCompleted",
):
    assert marker in SINNOH_H
assert "static_cast<uint16_t>(explored) << 8U" in SINNOH_CPP
assert "fullyCompleted(state)" in SINNOH_CPP

# Unova requires every authored branch of its endgame, including one Royal
# voyage; Kalos requires every route leader including the League.
assert "royalUnovaCompletedAtLeastOnce" in UNOVA_H
assert "state.pwtCompletedTiers & 0x1FU" in UNOVA_CPP
assert "royalUnovaLastDay != UINT32_MAX" in UNOVA_CPP
assert "fullyCompleted" in KALOS_H
assert "state.defeatedBosses & kAllAreasMask" in KALOS_CPP

# The shared result pipeline reconciles all six regions, persists the item,
# acknowledges battle-time delivery once and has a Home fallback after boot.
assert "uint8_t reconcileRegionCompletionRewards()" in MAIN
assert "UnovaAdventure::reconcileCompletionReward" in MAIN
assert "KalosAdventure::reconcileCompletionReward" in MAIN
assert "regionalMasterBallNoticeMask" in MAIN
assert "regionalMasterBallsGranted!=0U" in MAIN
assert '"%s 100%%! MASTER BALL RECEIVED!"' in MAIN
assert '"RESEARCH %u  GYM %u/8  EXP %u/8"' in MAIN

print("Regional 100% Master Ball contract passed.")
