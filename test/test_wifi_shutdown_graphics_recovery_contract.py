from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")


MULTIPLAYER = (ROOT / "src" / "services" / "Multiplayer.cpp").read_text(
    encoding="utf-8"
)


def test_network_runtime_is_released_after_wifi_driver_shutdown():
    """Arduino's permanent event task must not fragment the post-Wi-Fi UI."""
    helper_start = MULTIPLAYER.index("void releaseArduinoNetworkRuntime()")
    helper_end = MULTIPLAYER.index("bool discoverOnlineRelay", helper_start)
    helper = MULTIPLAYER[helper_start:helper_end]
    assert "Network.~NetworkManager()" in helper
    assert "NetworkManager()" in helper
    assert "esp_event_loop_delete_default()" in helper
    assert "esp_netif_deinit()" not in helper

    disable_start = MULTIPLAYER.index("if (!enable) {")
    disable_end = MULTIPLAYER.index("return true;", disable_start)
    disable = MULTIPLAYER[disable_start:disable_end]
    assert disable.index("WiFi.mode(WIFI_OFF)") < disable.index(
        "releaseArduinoNetworkRuntime()"
    )

    setup_start = MAIN.index("void setup()")
    setup = MAIN[setup_start:]
    renderer = setup.index("AssetRenderer::begin()")
    netif = setup.index("esp_netif_init()")
    sd = setup.index('Serial.println("[BOOT] microSD")')
    assert renderer < netif < sd
    assert "netifPlacementGuard" not in setup


def test_radio_close_restores_save_before_complete_ui_workspace():
    start = MAIN.index("bool disableMultiplayerRadioAndRestoreGraphics(")
    end = MAIN.index("bool disableMultiplayerRadioAfterBagClose()", start)
    close = MAIN[start:end]
    assert close.index("multiplayer.setEnabled(false)") < close.index(
        "resumeGameSaveAfterRadioClose()"
    )
    assert close.index("resumeGameSaveAfterRadioClose()") < close.index(
        "AssetRenderer::restoreSceneWorkspace(false)"
    )
    assert "AssetRenderer::uiSceneWorkspaceReady()" in close
