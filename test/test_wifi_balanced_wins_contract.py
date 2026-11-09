"""Locks the online Balanced-win counter and compact discovery UI."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
SAVE_H = (ROOT / "include/services/PersistentSave.h").read_text(encoding="utf-8")
MULTIPLAYER_H = (ROOT / "include/services/Multiplayer.h").read_text(encoding="utf-8")
MULTIPLAYER_CPP = (ROOT / "src/services/Multiplayer.cpp").read_text(encoding="utf-8")
ONLINE_H = (ROOT / "include/services/OnlineMultiplayerProtocol.h").read_text(encoding="utf-8")
SERVER = (ROOT / "server/PokegochiOnlineServer/src/main.cpp").read_text(encoding="utf-8")
FONT = (ROOT / "src/drivers/FireRedFont.cpp").read_text(encoding="utf-8")
ANDROID_PROTOCOL = (
    ROOT / "android/PokegochiApp/app/src/main/java/com/pokegochi/app/OnlineMultiplayerProtocol.java"
).read_text(encoding="utf-8")


def test_wins_are_local_save_state_and_relayed_only_for_presence():
    assert "uint32_t wifiBalancedWins = 0;" in SAVE_H
    assert "kFormatVersion = 52" in SAVE_H
    assert "struct RecordV51" in SAVE_H
    assert "save.wifiBalancedWins = 0;" in (
        ROOT / "src/services/PersistentSave.cpp"
    ).read_text(encoding="utf-8")
    assert "completedBalancedOnlineVictory" in MAIN
    assert "multiplayer.transportKind() == Multiplayer::TransportKind::Wifi" in MAIN
    assert "multiplayer.battleRules() == Multiplayer::BattleRules::Balanced" in MAIN
    assert "++gameSave.wifiBalancedWins" in MAIN
    assert "uint32_t balancedWins" in ONLINE_H
    assert "uint32_t balancedWins = 0;" in MULTIPLAYER_H
    assert "client.balancedWins = hello.balancedWins;" in SERVER
    assert "PeerEntryPayload entry{candidate.balancedWins}" in SERVER
    assert "preferences.putUInt(\"balanced" not in SERVER
    assert "preferences.putBytes(\"balanced" not in SERVER
    assert "VERSION = 2" in ANDROID_PROTOCOL
    assert "peerBalancedWins" in ANDROID_PROTOCOL


def test_wifi_rows_have_only_player_id_and_wins_and_connected_footer_does_not_overlap_cancel():
    draw_start = MAIN.index("void drawMultiplayerPocket() {")
    draw = MAIN[draw_start : MAIN.index("void drawInventory() {", draw_start)]
    assert '"P:%05lu   WIN:%lu"' in draw
    connected_start = draw.rindex("} else if (multiplayer.connected()) {")
    connected_end = draw.index("\n  } else {\n    drawMultiplayerLinkMark", connected_start)
    connected = draw[connected_start:connected_end]
    assert "multiplayer.statusText()" not in connected
    assert '"WAITING FOR THIS PLAYER TO CHOOSE."' in connected
    assert "case '_': return 0xB0" in FONT


def test_snapshot_retries_remain_byte_identical_until_ack():
    assert "BattleStateSync* publishedBattleState_ = nullptr;" in MULTIPLAYER_H
    assert "publishedBattleState_ = frozen;" in MULTIPLAYER_CPP
    assert "const BattleStateSync& sync = *publishedBattleState_;" in MULTIPLAYER_CPP
    assert "snapshotChecksum == publishedBattleStateChecksum_" in MULTIPLAYER_CPP
