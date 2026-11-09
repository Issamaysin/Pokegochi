from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src" / "main.cpp"
PLATFORMIO = ROOT / "platformio.ini"


def test_physical_pvp_uses_one_coherent_full_width_panel_window_per_region():
    source = MAIN.read_text(encoding="utf-8")
    assert "const bool coherentPvpRows = pvpSessionActive" in source
    assert "firstColumn = 0;\n    lastColumn = 320;" in source
    assert "panel.setAddrWindow(0, streamTop, 320, streamHeight);" in source
    assert "if (coherentPvpRows) {" in source
    assert "panel.pushPixels(static_cast<uint16_t*>(display.getPointer())" in source
    assert "forceAllTiles || retainedTileHash[stripe][tile] == 0U" in source
    assert "panel.pushPixels(pixels," in source
    assert "static_cast<uint32_t>(320U) * kRetainedBandHeight" in source


def test_physical_panel_clock_keeps_wifi_signal_margin():
    config = PLATFORMIO.read_text(encoding="utf-8")
    assert "-D SPI_FREQUENCY=10000000" in config
    assert "-D SPI_READ_FREQUENCY=8000000" in config
    assert "-D SPI_FREQUENCY=40000000" not in config


def test_pvp_battler_replacement_is_one_visible_coherent_panel_transaction():
    source = MAIN.read_text(encoding="utf-8")
    assert "requestPvpFullBattleSurfaceRefresh(" in source
    assert '"player-switch" : "opponent-switch"' in source
    assert 'requestPvpFullBattleSurfaceRefresh("authoritative-active")' in source
    presenter = source[source.index("bool presentRetainedFrame(uint32_t* transferredPixels)") :]
    assert "pvpFullBattleSurfaceRefreshPending" in presenter
    assert "firstPixel = 0;" in presenter
    assert "lastPixel = 240;" in presenter
    replacement_presenter = presenter[
        presenter.index("const bool fullPvpBattleRefresh"):
        presenter.index("void stopWildLedAlert()")
    ]
    assert "panel.writecommand(kIli9341DisplayOff)" not in replacement_presenter
    assert "holdPanelForPvpRefresh" not in replacement_presenter
    # A PvP refresh owns one vertical GRAM stream. Restarting RAMWR for every
    # 8-pixel stripe reproduced vertical columns on the physical CYD panel.
    assert "panel.setAddrWindow(0, streamTop, 320, streamHeight);" in presenter
    coherent_stream = presenter[presenter.index("if (coherentPvpRows) {") :]
    assert "panel.pushPixels(static_cast<uint16_t*>(display.getPointer())" in coherent_stream
    assert "No CASET/PASET/RAMWR command" in coherent_stream
    assert "pvpFullBattleSurfaceRefreshPending = false;" in presenter
    assert "bool prepareBattleGraphics(" in source
    assert "PvP full refresh waiting for canonical scene" in presenter
    assert "battleFrameCompositionFailed = true;" in source
    assert source.index("requestPvpFullBattleSurfaceRefresh(\n            event.side") > source.index("case BattleEventType::SwitchedIn:")
