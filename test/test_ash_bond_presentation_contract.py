#!/usr/bin/env python3
"""Regression contract for Ash's Bond and Ash-Greninja presentation."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ENGINE = (ROOT / "src/game/BattleEngine.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
ALOLA = (ROOT / "src/game/AlolaForms.cpp").read_text(encoding="utf-8")
ALOLA_HEADER = (ROOT / "include/game/AlolaForms.h").read_text(encoding="utf-8")
ASSETS = ROOT / ".generated/sdcard/pokegochi/assets/pokemon/forms/alola/658"

# The form change must be journaled after the defeated opponent, not inferred
# from the already-mutated live object. This is the protection against the
# early/vanished sprite class of regression previously seen on Mega Rayquaza.
helper_start = ENGINE.index("void resolveAshBondAfterFaint")
helper_end = ENGINE.index("\n}\n", helper_start) + 3
helper = ENGINE[helper_start:helper_end]
for token in (
    "HeldItem::AshsBond",
    "attacker.form=1U",
    "BattleMoveEffect::FormChanged",
    "changed.before=0U;changed.after=1U",
):
    assert token in helper, token
assert "hasHealthyOpponentReplacement(battle)" not in helper

# The persistent form uses the existing reserved bits, is finalized by every
# terminal battle path and advances only while no battle is active.
for token in (
    "AlolaForms::finishAshBondBattle(collection)",
    "AlolaForms::advanceAshBondOutsideTime(",
):
    assert token in ENGINE or token in MAIN, token
for token in (
    "kAshBondOutsideDurationSeconds = 5U * 60U",
    "uint16_t ashBondOutsideSecondsRemaining",
    "bool beginAshBondBattle",
    "bool finishAshBondBattle",
    "bool advanceAshBondOutsideTime",
):
    assert token in ALOLA_HEADER or token in ALOLA, token
assert (
    "if(!gameSave.battle.active&&AlolaForms::advanceAshBondOutsideTime("
    in MAIN
)
reset_start = ENGINE.index("bool resetBattleTriggeredForms")
reset_end = ENGINE.index("\n}\n", reset_start) + 3
assert "speciesId==658U" not in ENGINE[reset_start:reset_end]

# Every player-side KO path activates the local Greninja, and enemy/server-bot
# Greninja uses the same mechanic with opponent-side volatile state and events.
assert ENGINE.count(
    "resolveAshBondAfterFaint(battle,result,*player,BattleSide::Player);"
) == 3
assert ENGINE.count(
    "resolveAshBondAfterFaint(battle,result,*opponent,BattleSide::Opponent);"
) == 2

# Both journal creation and playback use event.before/event.after. They must
# never read the final live form when the same action has already ended battle.
for token in (
    "battleFlow.displayPlayerForm = playerFormBefore",
    "battleFlow.displayOpponentForm = opponentFormBefore",
    "staged.form=static_cast<uint8_t>(event.before)",
    "staged.form=static_cast<uint8_t>(event.after)",
    "battleFlow.displayPlayerForm=staged.form",
    "battleFlow.displayOpponentForm=staged.form",
):
    assert token in MAIN, token

# Existing saves must receive the item at boot/Home without opening Alola,
# and newly captured/traded Greninja must use the same reconciliation path.
assert "bool reconcileAlolaSpeciesItems(bool announceAshsBond)" in MAIN
assert "reconcileAlolaSpeciesItems(true);" in MAIN
assert MAIN.count("reconcileAlolaSpeciesItems(false)") >= 4
capture_gate = MAIN[MAIN.index("if (result.outcome == BattleOutcome::Captured &&"):]
capture_gate = capture_gate[:capture_gate.index("if (completedKind != BattleKind::None")]
assert "alolaUnlocked()" not in capture_gate

# Normal and Ash forms are prebuilt for every route the app/firmware can use.
for form in (0, 1):
    for shiny in ("", "shiny/"):
        for view in ("front.pkg", "back.pkg", "icons/0.pkg", "icons/1.pkg"):
            path = ASSETS / str(form) / shiny / view
            assert path.is_file() and path.stat().st_size > 0, path

print(
    "Ash's Bond presentation contract passed: KO ordering, staged form journal, "
    "five-minute outside lifecycle and normal/Ash assets are complete."
)
