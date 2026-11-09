#!/usr/bin/env python3
"""Locks the player-facing Wi-Fi setup to SSID + password only."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
MULTIPLAYER = (ROOT / "src" / "services" / "Multiplayer.cpp").read_text(
    encoding="utf-8"
)
SDKCONFIG = (ROOT / "sdkconfig.esp32-2432S028R").read_text(encoding="utf-8")
ANDROID = (
    ROOT
    / "android"
    / "PokegochiApp"
    / "app"
    / "src"
    / "main"
    / "java"
    / "com"
    / "pokegochi"
    / "app"
    / "WifiMultiplayer.java"
).read_text(encoding="utf-8")
ACTIVITY = (
    ROOT
    / "android"
    / "PokegochiApp"
    / "app"
    / "src"
    / "main"
    / "java"
    / "com"
    / "pokegochi"
    / "app"
    / "MainActivity.java"
).read_text(encoding="utf-8")
MANIFEST = (
    ROOT
    / "android"
    / "PokegochiApp"
    / "app"
    / "src"
    / "main"
    / "AndroidManifest.xml"
).read_text(encoding="utf-8")
SERVER = (
    ROOT / "server" / "PokegochiOnlineServer" / "src" / "main.cpp"
).read_text(encoding="utf-8")


setup_start = MAIN.index("void drawWifiSetup()")
setup_end = MAIN.index("void drawMultiplayerPocket()", setup_start)
setup_ui = MAIN[setup_start:setup_end]
setup_touch_start = MAIN.index("void handleWifiSetupTouch(")
setup_touch_end = MAIN.index("void drawMultiplayerPocket()", setup_touch_start)
setup_touch = MAIN[setup_touch_start:setup_touch_end]

for required in (
    '"WIFI NETWORK"',
    '"SELECT A NETWORK"',
    '"SCAN"',
    '"WIFI PASSWORD"',
    '"SERVER FOUND AUTOMATICALLY"',
    '"AUTO-CONNECT: %u/4 NETWORKS SAVED"',
    '"USE NETWORK"',
):
    assert required in setup_ui, f"missing Wi-Fi setup element: {required}"

for forbidden in (
    "SERVER ADDRESS",
    "SERVER PORT",
    "ACCESS CODE",
    "WifiSetupField::Host",
    "WifiSetupField::AccessCode",
):
    assert forbidden not in setup_ui, f"player-facing endpoint field returned: {forbidden}"

assert "wifiScanRequestPending_ = true" in MULTIPLAYER
assert "mdns_query_ptr(" in MULTIPLAYER
assert 'std::snprintf(serviceLabel, sizeof(serviceLabel), "_%s"' in MULTIPLAYER
assert 'std::snprintf(transportLabel, sizeof(transportLabel), "_%s"' in MULTIPLAYER
assert "mdns_query_results_free(results)" in MULTIPLAYER
assert "mdns_free()" in MULTIPLAYER
assert "selectedAddressRank" in MULTIPLAYER
assert "candidateRank >= selectedAddressRank" in MULTIPLAYER
assert "result && !resolved" not in MULTIPLAYER
assert "onlineConfigurationForced_" in MULTIPLAYER
assert "using cabled diagnostic relay" in MULTIPLAYER
assert '"CABLE_WIFI_RELAY "' in MAIN
assert "kMdnsServiceName" in MULTIPLAYER
assert 'kMaximumSavedWifiNetworks = 4' in (
    ROOT / "include" / "services" / "Multiplayer.h"
).read_text(encoding="utf-8")
assert 'kWifiMruKey[] = "wifi-mru"' in MULTIPLAYER
assert "rememberSuccessfulWifiNetwork(wifiSsid_, wifiPassword_)" in MULTIPLAYER
assert "wifiManualCandidate_ = true" in MULTIPLAYER
assert "scanPhysicalWifiNetworks(" in MULTIPLAYER
assert "esp_wifi_scan_start(&config, true)" in MULTIPLAYER
assert "const int found = WiFi.scanNetworks(" not in MULTIPLAYER
assert MULTIPLAYER.index("rememberSuccessfulWifiNetwork(wifiSsid_, wifiPassword_)") > (
    MULTIPLAYER.index("WiFi.status() == WL_CONNECTED")
)
assert "count < kMaximumSavedWifiNetworks" in MULTIPLAYER
assert "wifiNetworkIsSaved(" in setup_ui
assert "savedWifiPassword(" in setup_touch

assert 'SERVICE_TYPE = "_pokegochi._tcp."' in ANDROID
assert "nsdManager.discoverServices(" in ANDROID
assert "firmware.multiplayerStartWifiPlayerSearch()" not in ANDROID
assert "firmware.multiplayerCompleteWifiPlayerSearch()" in ANDROID
assert "peerRefreshSentForScan" in ANDROID
assert "state == STATE_SCANNING && !peerRefreshSentForScan" in ANDROID
assert "REFRESH_MS" not in ANDROID
welcome = ANDROID[
    ANDROID.index("case OnlineMultiplayerProtocol.WELCOME"):
    ANDROID.index("case OnlineMultiplayerProtocol.PEER_ENTRY")
]
assert "multiplayerStartWifiPlayerSearch" not in welcome
assert "Tap Find Player to scan" in welcome
complete_scan = MULTIPLAYER[
    MULTIPLAYER.index("void Multiplayer::completeOnlinePeerScan()"):
    MULTIPLAYER.index("void Multiplayer::transportDiscovered(")
]
assert "setState(State::Advertising" in complete_scan
assert "TAP FIND PLAYER" in complete_scan
assert "onlinePeerSearchPending_ = false" in complete_scan
assert "!onlinePeerSearchPending_" in complete_scan
discovered = MULTIPLAYER[
    MULTIPLAYER.index("void Multiplayer::transportDiscovered("):
    MULTIPLAYER.index("bool Multiplayer::choosePeer(")
]
assert "transportKind_ == TransportKind::Wifi" in discovered
assert "!onlinePeerSearchPending_" in discovered
assert "peerSearchInProgress()" in MAIN
assert 'stateLabel = "SELECT ONLINE PLAYER"' in MAIN
assert "uint8_t multiplayerPeerPage = 0" in MAIN
assert '"SELECT PLAYER %u/%u"' in MAIN
assert "multiplayer.peer(firstPeer + row)" in MAIN
assert "multiplayer.choosePeer(firstPeer + row)" in MAIN
assert 'kMaximumPeers = 12' in (
    ROOT / "include" / "services" / "Multiplayer.h"
).read_text(encoding="utf-8")
for forbidden in (
    "WifiManager",
    "WifiNetworkSpecifier",
    "startScan()",
    "getScanResults()",
    "multiplayerWifiSsid",
    "multiplayerWifiPassword",
    "onPermissionsRequired",
):
    assert forbidden not in ANDROID, f"Android still provisions Wi-Fi: {forbidden}"

assert "requestWifiPermissions" not in ACTIVITY
assert 'android:name="android.permission.NEARBY_WIFI_DEVICES"' not in MANIFEST
assert 'android:name="android.permission.CHANGE_WIFI_STATE"' not in MANIFEST
assert (
    'android:name="android.permission.ACCESS_FINE_LOCATION" '
    'android:maxSdkVersion="30"' in MANIFEST
), "legacy location permission must remain scoped only to old Android BLE"

assert "constexpr bool kWifiNeedsLocalSetup = false" in MAIN
assert "constexpr bool kWifiNeedsLocalSetup = true" in MAIN
assert "wifi && kWifiNeedsLocalSetup" in MAIN
assert "btMemRelease(BT_MODE_CLASSIC_BT)" in MAIN
assert "btMarkMemReleased(BT_MODE_CLASSIC_BT)" in MAIN
assert "radioMemoryReady = btMemRelease(BT_MODE_BLE)" in MAIN
assert "resumeBlePocketMarker = kResumeBlePocketMagic" in MAIN
assert "resumed BLE multiplayer page after Wi-Fi mode" in MAIN
assert 'wifi ? "STARTING WIFI..." : "STARTING BLUETOOTH..."' in MAIN
assert 'wifi ? "STOPPING WIFI..." : "STOPPING BLUETOOTH..."' in MAIN
toggle_feedback = MAIN[
    MAIN.index("multiplayerRadioTransitionActive = true;"):
    MAIN.index("if (enable) {", MAIN.index("multiplayerRadioTransitionActive = true;"))
]
assert "drawMultiplayerPocket();" in toggle_feedback
assert "presentRetainedFrame();" in toggle_feedback
assert "btMemRelease(BT_MODE_BLE)" not in MULTIPLAYER
assert "btMarkMemReleased(BT_MODE_BLE)" not in MULTIPLAYER
assert "wifiReleasedBleMemory" not in MULTIPLAYER
assert "multiplayerRadioModeRequiresRestart()" not in MAIN
assert "char battleAnimationPreparedBackgroundPath[80]{};" in MAIN
assert len("/pokegochi/assets/firered/battle_anims/backgrounds/tiled_bg_80.pkg") + 1 <= 80
assert 'disableMultiplayerRadioAndRestoreGraphics("Switching WIFI to BLE")' not in MAIN
# The physical client keeps BLE available in the same boot, so Wi-Fi and the
# 54 KiB GameSave must coexist. Pin the supported low-memory profile that was
# validated on the no-PSRAM CYD instead of allowing generic server-sized lwIP
# pools to regress and route Wi-Fi ON into the fatal save-recovery restart.
for required in (
    "CONFIG_ESP_TIMER_TASK_STACK_SIZE=4096",
    "CONFIG_ESP_WIFI_MGMT_SBUF_NUM=6",
    "CONFIG_LWIP_MAX_SOCKETS=8",
    "CONFIG_LWIP_MAX_ACTIVE_TCP=4",
    "CONFIG_LWIP_MAX_LISTENING_TCP=4",
    "CONFIG_LWIP_MAX_UDP_PCBS=8",
    "CONFIG_LWIP_MAX_RAW_PCBS=4",
    "CONFIG_LWIP_TCPIP_TASK_STACK_SIZE=3072",
    "CONFIG_MDNS_MAX_INTERFACES=1",
    "CONFIG_MDNS_MAX_SERVICES=1",
):
    assert required in SDKCONFIG, required
assert "bounded.nvs_enable = 0" in MULTIPLAYER
assert "bounded.espnow_max_encrypt_num = 0" in MULTIPLAYER
assert "bounded.mgmt_sbuf_num = std::min(bounded.mgmt_sbuf_num, 6)" in MULTIPLAYER
wifi_mode = MULTIPLAYER.index("WiFi.mode(WIFI_STA)")
assert MULTIPLAYER.rfind("WiFi.persistent(false)", 0, wifi_mode) != -1
assert "AssetRenderer::uiSceneWorkspaceReady()" in MAIN
assert "AssetRenderer::releaseSceneWorkspaceForRadio()" in setup_touch
assert "multiplayer.scanWifiNetworks()" in setup_touch
assert "AssetRenderer::restoreSceneWorkspace(true)" in setup_touch
assert setup_touch.index("AssetRenderer::releaseSceneWorkspaceForRadio()") < setup_touch.index(
    "multiplayer.scanWifiNetworks()"
) < setup_touch.index("AssetRenderer::restoreSceneWorkspace(true)")

assert "MDNS.addService(kMdnsServiceName, kMdnsTransport" in SERVER
assert "MDNS.addServiceTxt(kMdnsServiceName" in SERVER
assert "header.sessionId == client.sessionId" in SERVER
assert "rolled back ready=%u/%u" in SERVER
assert "kClientIdleTimeoutMs = 30000U" in SERVER
assert "kClientSessionTimeoutMs = 120000U" in SERVER
assert "client.sessionId" in SERVER
for required in (
    "pivotMovePending",
    "pivotMoveExecuted",
    "botMoveRequiresChosenReplacement",
    "sendBotPivotReplacement",
    "sync.pokemon.movePp[battleBot.lastPivotMoveSlot] <",
    "command.turn == battleBot.lastPivotMoveTurn",
):
    assert required in SERVER, f"server bot pivot barrier missing: {required}"

print(
    "PASS: physical device keeps four successful Wi-Fi networks in MRU order; "
    "Android uses its active network with one explicit player scan per tap; "
    "relay closes rooms defensively"
)
