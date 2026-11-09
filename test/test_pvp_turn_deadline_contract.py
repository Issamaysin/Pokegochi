"""Contracts for unilateral PvP surrender and the 30-second decision clock."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
SESSION = (ROOT / "src/services/Multiplayer.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "include/services/Multiplayer.h").read_text(encoding="utf-8")


def main() -> None:
    assert "constexpr uint32_t kPvpTurnDecisionLimitMs = 30000U;" in MAIN
    assert "void servicePvpDecisionClock(uint32_t now)" in MAIN
    assert "servicePvpDecisionClock(now);" in MAIN
    assert "pvpDecisionSecondsRemaining(millis())" in MAIN
    assert '"%us"' in MAIN

    deadline = MAIN[
        MAIN.index("void servicePvpDecisionClock(uint32_t now)") :
        MAIN.index("void finishPendingBattleFlow()")
    ]
    assert "now - pvpDecisionStartedMs < kPvpTurnDecisionLimitMs" in deadline
    assert "pvpFirstUsableLocalRosterSlot()" in deadline
    assert "pvpFirstSelectableMoveSlot(BattleSide::Player)" in deadline
    assert "Multiplayer::BattleCommandKind::Move" in deadline
    assert "packPvpMoveCommand(fallbackMove, false)" in deadline
    assert '"TIME UP - POKEMON AUTO-SELECTED."' in deadline
    assert '"TIME UP - MOVE AUTO-SELECTED."' in deadline

    fallback = MAIN[
        MAIN.index("uint8_t pvpFirstSelectableMoveSlot(BattleSide side) {") :
        MAIN.index("void prependPvpEvents(")
    ]
    assert "battler->movePp[slot] > 0" in fallback
    assert "BattleEngine::moveIsSelectable(" in fallback
    assert "return kMoveSlots;" in fallback

    struggle = MAIN[
        MAIN.index("void executeStruggleBattleMove()") :
        MAIN.index("void executeSelectedBattleSwitch(")
    ]
    assert "packPvpMoveCommand(kMoveSlots, false)" in struggle
    assert "Multiplayer::BattleCommandKind::Move" in struggle
    assert "BattleEngine::fightInto" in struggle

    submit = SESSION[
        SESSION.index("bool Multiplayer::submitBattleCommand(") :
        SESSION.index("bool Multiplayer::takeBattleCommands(")
    ]
    assert "kind == BattleCommandKind::Forfeit" in submit
    assert "replacingPendingCommand" in submit
    assert "remoteBattleCommand_ = {true, turn, BattleCommandKind::Wait, 0}" in submit
    assert "bool Multiplayer::localBattleCommandPending() const" in submit
    assert "bool localBattleCommandPending() const;" in HEADER

    run_flow = MAIN[
        MAIN.index("if (runConfirmVisible) {") :
        MAIN.index("if (!gameSave.battle.active", MAIN.index("if (runConfirmVisible) {"))
    ]
    assert "BattleCommandKind::Forfeit" in run_flow
    assert "FORFEITING LINK BATTLE" in run_flow
    assert "multiplayer.disconnect();" in run_flow
    assert "WAITING FOR THE OTHER PLAYER" not in run_flow

    print("PvP unilateral RUN and 30-second decision deadline contract passed.")


if __name__ == "__main__":
    main()
