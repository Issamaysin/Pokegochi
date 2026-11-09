"""Contracts for the forced-switch barrier and Internet relay fallback."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
MULTIPLAYER_H = (ROOT / "include/services/Multiplayer.h").read_text(encoding="utf-8")
MULTIPLAYER_CPP = (ROOT / "src/services/Multiplayer.cpp").read_text(encoding="utf-8")
RELAY_CONFIG = (ROOT / "include/config/OnlineRelayConfig.h").read_text(encoding="utf-8")
ANDROID_MQTT = (
    ROOT
    / "android/PokegochiApp/app/src/main/java/com/pokegochi/app/MqttRelayConnection.java"
).read_text(encoding="utf-8")
ANDROID_WIFI = (
    ROOT
    / "android/PokegochiApp/app/src/main/java/com/pokegochi/app/WifiMultiplayer.java"
).read_text(encoding="utf-8")
ANDROID_JNI = (
    ROOT
    / "android/PokegochiApp/app/src/main/cpp/pokegochi_firmware_jni.cpp"
).read_text(encoding="utf-8")
SERVER = (ROOT / "server/PokegochiOnlineServer/src/main.cpp").read_text(encoding="utf-8")


def quoted(text: str, name: str) -> str:
    match = re.search(rf"{name}(?:\[\])?\s*=\s*\"([^\"]+)\"", text)
    assert match, f"missing {name}"
    return match.group(1)


def main() -> None:
    # The local faint picker must be visible before the authoritative state
    # round trip. A fast Android tap is staged and submitted only after the
    # sync barrier clears, so it cannot be discarded or sent for the old turn.
    finish = MAIN[MAIN.index("void finishPendingBattleFlow()") :]
    immediate_picker = finish.index("CHOOSE YOUR NEXT POKEMON.")
    generic_sync = finish.index("SYNCING LINK BATTLE...")
    assert immediate_picker < generic_sync
    assert "pvpStagedForcedRosterSlot = static_cast<int8_t>(rosterSlot);" in MAIN
    assert "SWITCH CHOSEN. SYNCING..." in MAIN
    assert "SET_LOOP_TASK_STACK_SIZE(12 * 1024);" in MAIN
    assert "pvpExpectedForcedRosterSlot = static_cast<int8_t>(rosterSlot);" in MAIN
    assert "ignored stale forced-switch snapshot" in MAIN
    authoritative = MAIN[
        MAIN.index("bool applyPvpAuthoritativeState(") :
        MAIN.index("bool useRareCandyOutsideBattle(")
    ]
    assert "sync.guestActiveRosterSlot != expected" in authoritative
    assert "sync.guestParty[expected].currentHp == 0U" in authoritative
    assert authoritative.index("sync.guestActiveRosterSlot != expected") < authoritative.index(
        "*local = sync.guestParty[slot];"
    )
    forced_phase = MAIN[
        MAIN.index("void updatePvpForcedSwitchPhase()") :
        MAIN.index("void finishPendingBattleFlow()")
    ]
    assert "pvpStagedForcedRosterSlot >= 0" in forced_phase
    assert "Multiplayer::BattleCommandKind::Switch" in forced_phase
    assert "!pvpStatePublishPending && !pvpAwaitingAuthoritativeState" in forced_phase
    # Move-driven replacements are transactions, not ordinary switches
    # inferred again from a transient BattleState nibble. The selected cause
    # must cross the snapshot/ACK barrier and be supplied explicitly to both
    # local and remote engine completion paths.
    assert "void latchPvpReplacementRequirements()" in MAIN
    assert "pvpStagedReplacementReason = pvpLocalReplacementReason;" in MAIN
    assert "pvpSubmittedReplacementReason = pvpStagedReplacementReason" in MAIN
    assert "localReplacementReason = pvpSubmittedReplacementReason" in MAIN
    assert "gameSave.battle,gameSave.collection,queuedPlayerMove," in MAIN
    assert "remoteReplacementReason);" in MAIN
    assert "gameSave.battle.pendingOpponentMoveSlot," in MAIN
    assert "localReplacementReason):" in MAIN
    assert MAIN.index("latchPvpReplacementRequirements();", MAIN.index(
        "void resolvePvpCommands(")) < MAIN.index(
        "const uint8_t localReplacementReason", MAIN.index("void resolvePvpCommands("))
    assert "uint16_t incomingBattlePokemonMask_" in MULTIPLAYER_H
    assert "uint16_t incomingBattleVolatileMask_" in MULTIPLAYER_H
    assert "pvpStateAckRetryPending" in MAIN
    assert "authoritative ACK retry sent" in MAIN
    assert "snapshotChecksum" in MULTIPLAYER_H
    assert "battleStateSyncChecksum" in MULTIPLAYER_CPP
    assert "rejected mixed snapshot" in MULTIPLAYER_CPP
    assert "duplicate authoritative snapshot ignored" in MAIN
    assert "stale authoritative ACK ignored" in MAIN
    assert "publishedBattleStateChecksum()" in MAIN
    assert "BattleStateSync* publishedBattleState_" in MULTIPLAYER_H
    assert "if (!publishedBattleState_)" in MULTIPLAYER_CPP
    assert "const BattleStateSync& sync = *publishedBattleState_;" in MULTIPLAYER_CPP
    assert "clearPublishedBattleStateCache()" in MULTIPLAYER_CPP
    assert "if (!pvpSessionActive) for (uint32_t uid" in MAIN
    assert 'requestPvpFullBattleSurfaceRefresh("faint-barrier")' in MAIN
    assert "snapshotPackets > kOutgoingPacketCapacity - outgoingPacketCount_" in MULTIPLAYER_CPP
    assert "snapshot queue busy" in MULTIPLAYER_CPP
    assert "sleep_for(std::chrono::milliseconds(1))" in ANDROID_JNI
    assert "WiFi.setSleep(false)" in MULTIPLAYER_CPP
    assert "gOnlineClient.setNoDelay(false)" in MULTIPLAYER_CPP
    assert "heartbeat write failed; closing stale socket" in MULTIPLAYER_CPP
    assert "heartbeat deferred by relay backpressure" in MULTIPLAYER_CPP
    # A relay envelope and game packet must enter TCP as one complete frame.
    # Splitting them can enqueue only the header when the physical ESP32 runs
    # out of lwIP pbufs during party/snapshot bursts, permanently desynchronizing
    # the byte stream before the first faint.
    assert "onlineTxFrame" in MULTIPLAYER_CPP
    assert "lwip_send(gOnlineClient.fd(), onlineTxFrame, frameLength" in MULTIPLAYER_CPP
    assert "gOnlinePendingFrame" in MULTIPLAYER_CPP
    assert "flushPendingOnlineFrame()" in MULTIPLAYER_CPP
    assert "NetworkClient::write()" in MULTIPLAYER_CPP
    assert "gOnlineClient.write(onlineTxFrame, frameLength)" not in MULTIPLAYER_CPP
    assert "gOnlineClient.availableForWrite()" not in MULTIPLAYER_CPP
    assert "gOnlineClient.write(reinterpret_cast<const uint8_t*>(&header)" not in MULTIPLAYER_CPP
    # READY contains multiple game packets. Physical Wi-Fi has a deliberately
    # single-frame outbox, so the Party snapshot must resume over loop ticks
    # after EAGAIN rather than rolling the accepted touch back halfway.
    assert "continueWifiBattleReadyTransmission()" in MULTIPLAYER_H
    assert "wifiBattleReadyTransmissionPending_" in MULTIPLAYER_H
    assert "SENDING PARTY TO PLAYER..." in MULTIPLAYER_CPP
    assert "if (gOnlinePendingLength) return;" in MULTIPLAYER_CPP
    assert "[WIFI] READY transaction complete" in MULTIPLAYER_CPP
    # Bot preparation is six ordered game frames. LAN sends them as one
    # sub-MSS TCP batch, avoiding delayed ACK/Nagle stalls before remote READY.
    assert "sendBotPreparation" in SERVER
    assert "[BOT] preparation batch bytes=%u frames=6" in SERVER
    assert "client.socket.write(batch, batchLength)" in SERVER
    assert "sendBotPreparation(client, request, header.transaction)" in SERVER
    assert "client.socket.write(reinterpret_cast<const uint8_t*>(&header)" not in SERVER
    assert "[LINK] remote READY count=%u" in MULTIPLAYER_CPP
    assert "[LINK] remote lead slot=%u" in MULTIPLAYER_CPP

    # LAN discovery remains the fast path, but both Android and the physical
    # client must fall back automatically to the same public MQTT route. No
    # player-facing IP/port configuration is involved.
    broker = quoted(RELAY_CONFIG, "kBrokerHost")
    root = quoted(RELAY_CONFIG, "kTopicRoot")
    assert quoted(ANDROID_MQTT, "BROKER_HOST") == broker
    assert quoted(ANDROID_MQTT, "TOPIC_ROOT") == root
    assert "LOCAL_DISCOVERY_WINDOW_MS" in ANDROID_WIFI
    assert "useCloud = true;" in ANDROID_WIFI
    assert "cloud.connect(firmware.multiplayerPlayerId())" in ANDROID_WIFI
    assert "WiFi.localIP" not in ANDROID_WIFI
    assert "PubSubClient" in SERVER
    assert "cloudMqtt.subscribe" in SERVER
    assert "ClientTransport::CloudMqtt" in SERVER
    assert "SESSION_TIMEOUT_MS = 120000L" in ANDROID_WIFI
    assert "kClientSessionTimeoutMs = 120000U" in SERVER
    # MQTT reconnect is synchronous in PubSubClient and may not starve a live
    # local/bot room. LAN frames are consumed first, then an active session
    # defers reconnecting the optional public route.
    assert "reconnect deferred until active session ends" in SERVER
    network_service = SERVER[
        SERVER.index("void serviceNetwork()") : SERVER.index("void serviceSerial()")
    ]
    assert network_service.index("serviceClients();") < network_service.index(
        "serviceCloudTransport(now);"
    )

    # The server bot must treat a forced replacement as a same-turn subphase.
    # Deduplicating only by turn used to discard the ordinary move immediately
    # after the bot's first faint and froze the battle forever.
    assert "lastForcedSnapshotChecksum" in SERVER
    assert "forcedCommandPending" in SERVER
    assert "battleBot.forcedCommandPending && !guestFainted && !hostFainted" in SERVER
    assert "battleBot.lastCommandTurn = 0xFFFFU;" in SERVER
    assert "battleBot.lastForcedSnapshotChecksum == checksum" in SERVER
    assert "if (battleBot.forcedCommandPending) break;" in SERVER
    assert "[BOT] forced switch confirmed" in SERVER
    # Peer-list evidence stays available on the headless relay, including the
    # singleton bot busy state, so Android discovery can be diagnosed without
    # relying on a phone-side file dump.
    assert "[SERVER] peer-list player=" in SERVER
    assert "bot-busy=%u" in SERVER

    print("PvP faint barrier and real Internet fallback contract passed.")


if __name__ == "__main__":
    main()
