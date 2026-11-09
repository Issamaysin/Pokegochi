#!/usr/bin/env python3
"""Regression contract for Dragon Ascent's staged battle appearance."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
ASSETS = ROOT / ".generated/sdcard/pokegochi/assets/pokemon"

# Dragon Ascent changes a battle-only Mega bit, not OwnedPokemon::form.  The
# UI must journal that identity independently from the engine's live record.
for token in (
    "MegaVariant displayPlayerMega = MegaVariant::None",
    "MegaVariant displayOpponentMega = MegaVariant::None",
    "MegaVariant playerMega = MegaVariant::None",
    "MegaVariant opponentMega = MegaVariant::None",
    "snapshot.playerMega = MegaEvolution::variantFor(*player)",
    "snapshot.opponentMega = MegaEvolution::variantFor(*opponent)",
    "battleFlow.displayPlayerMega = playerMegaBefore",
    "battleFlow.displayOpponentMega = opponentMegaBefore",
    "MegaVariant presentedBattleMegaVariant",
    "void applyPresentedBattleAppearance",
    "event.move==static_cast<MoveId>(620U)",
    "battleArenaPlayerMega != playerMega",
    "battleArenaOpponentMega != opponentMega",
    "battleArenaPlayerMega = presentedBattleMegaVariant(player,true)",
    "battleArenaOpponentMega = presentedBattleMegaVariant(opponent,false)",
):
    assert token in MAIN, token

# Every path that can paint or preload a battler during an animation must use
# the staged copy; reading the live Rayquaza marker reintroduces early/vanished
# Mega sprites when the resolving hit also ends the battle.
assert MAIN.count("applyPresentedBattleAppearance(") >= 11
assert "mixKey(static_cast<uint8_t>(presentedBattleMegaVariant(player,true)))" in MAIN
assert "mixKey(static_cast<uint8_t>(presentedBattleMegaVariant(opponent,false)))" in MAIN

for relative in (
    "front/384.pkg",
    "back/384.pkg",
    "forms/mega/384/mega/front.pkg",
    "forms/mega/384/mega/back.pkg",
):
    path = ASSETS / relative
    assert path.is_file() and path.stat().st_size > 0, path

print("Rayquaza presentation contract passed: base/Mega identity is journaled and cached independently.")
