"""Regression contract for initiator-owned activities and PvP Z commands."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
SESSION = (ROOT / "src/services/Multiplayer.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "include/services/Multiplayer.h").read_text(encoding="utf-8")


def main() -> None:
    assert "bool initiatedConnection() const;" in HEADER
    assert "!outbound_ || state_ != State::Connected" in SESSION
    assert "connected() || outbound_ || state_ != State::Connected" in SESSION
    assert "struct ActivityPayload { uint8_t activity; uint8_t rules; };" in SESSION
    assert "Reserved legacy packet. Protocol v9" in SESSION

    connected_ui = MAIN[MAIN.index("} else if (multiplayer.connected()) {") :]
    assert "multiplayer.initiatedConnection()" in connected_ui
    assert '"BATTLE"' in connected_ui and '"TRADE"' in connected_ui
    assert "bagPage == kMultiplayerWifiPage" not in connected_ui[
        : connected_ui.index("fireRedText.setTextColor(TFT_DARKGREY, kPanel);")
    ]

    assert "constexpr uint8_t kPvpMoveZFlag = 0x80U;" in MAIN
    assert "packPvpMoveCommand(" in MAIN
    assert "pvpCommandUsesZMove(remote.value)" in MAIN
    assert "remoteMoveSlot, remoteUsesZMove" in MAIN

    # ActivityCancel closes one battle/trade activity, not the reusable
    # physical link. A completed battle must return both peers to the same
    # connected room so a second battle can start without reconnecting.
    service_start = MAIN.index(
        "__attribute__((noinline)) void serviceMultiplayerConnectionAndLaunch()"
    )
    service_end = MAIN.index(
        "__attribute__((noinline)) void servicePvpSynchronization", service_start
    )
    service = MAIN[service_start:service_end]
    assert "const bool logicalBattleEnded" in service
    assert "multiplayer.connected() &&" in service
    logical_start = service.index("if (logicalBattleEnded)")
    physical_start = service.index(
        "if (unexpectedDisconnect || physicalLinkBattleEnded)", logical_start
    )
    logical_path = service[logical_start:physical_start]
    assert "restorePartyAfterPvp();" in logical_path
    assert "bagPage = activeMultiplayerPage();" in logical_path
    assert "drawMultiplayerPocket();" in logical_path
    assert "disableMultiplayerRadioAndRestoreGraphics" not in logical_path

    # Remote READY is only a status update. The local preparation screen must
    # retain OPEN BOX until this player commits their own Party snapshot.
    assert "else if (localBattleReady_)" in SESSION
    assert 'setState(State::BattlePreparing, remoteBattleReady_' in SESSION
    preparing_ui = MAIN[
        MAIN.index("} else if (state == Multiplayer::State::BattlePreparing) {") :
        MAIN.index("} else if (state == Multiplayer::State::BattleWaitingReady) {")
    ]
    assert "multiplayer.remoteBattleReady()" in preparing_ui
    assert '"OPEN BOX"' in preparing_ui
    assert '"READY"' in preparing_ui
    waiting_ui = MAIN[
        MAIN.index("} else if (state == Multiplayer::State::BattleWaitingReady) {") :
        MAIN.index("} else if (state == Multiplayer::State::BattleChoosingLead) {")
    ]
    assert '"YOUR PARTY IS LOCKED."' in waiting_ui
    assert '"OPEN BOX"' not in waiting_ui

    print("Multiplayer invitation authority, Wi-Fi trade and PvP Z command contract passed.")


if __name__ == "__main__":
    main()
