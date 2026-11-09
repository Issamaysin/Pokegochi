from __future__ import annotations

import ast
import json
import re
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILDER_PATH = ROOT / "scripts" / "build_sd_asset_pack.py"
MAIN_PATH = ROOT / "src" / "main.cpp"
ASSET_ROOT = ROOT / ".generated" / "sdcard" / "pokegochi" / "assets"
ARCHIVE_PATH = ROOT / ".generated" / "sdcard" / "pokegochi" / "pokegochi.pak"


def tuple_assignment(tree: ast.AST, name: str) -> tuple[str, ...]:
    for node in ast.walk(tree):
        if not isinstance(node, ast.Assign):
            continue
        if any(isinstance(target, ast.Name) and target.id == name
               for target in node.targets):
            value = ast.literal_eval(node.value)
            assert isinstance(value, tuple)
            return value
    raise AssertionError(f"missing tuple assignment: {name}")


def function_body(source: str, start: str, end: str) -> str:
    begin = source.index(start)
    finish = source.index(end, begin)
    return source[begin:finish]


def asset_hash(relative: str) -> int:
    value = 2166136261
    for byte in relative.encode("ascii"):
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def test_complete_held_item_icon_catalogue() -> None:
    source = BUILDER_PATH.read_text(encoding="utf-8")
    tree = ast.parse(source)
    assert len(tuple_assignment(tree, "held_icons")) == 37
    assert tuple_assignment(tree, "battle_tower_held_icons") == (
        "berry_juice", "macho_brace", "soul_dew", "deep_sea_scale",
        "deep_sea_tooth", "light_ball", "leppa_berry", "in_battle_herb",
        "dragon_scale", "iapapa_berry", "wiki_berry", "sea_incense",
        "salac_berry", "lansat_berry", "apicot_berry", "starf_berry",
        "liechi_berry", "stick", "lax_incense", "aguav_berry", "figy_berry",
        "thick_club", "mago_berry", "metal_powder", "petaya_berry",
        "lucky_punch", "ganlon_berry",
    )
    assert "enumerate(battle_tower_held_icons, start=39)" in source
    assert '"graphics/items/icons/ditto_powder.png"' in source
    assert '"graphics/items/icon_palettes/quick_powder.pal"' in source
    assert 'write_asset("firered/items/held_67.pkg", quick_powder, manifest)' in source
    assert "for index in range(1,66)" in source
    assert '"firered/items/held_67.pkg"' in source
    assert "for index in range(68,92)" in source
    assert len(tuple_assignment(tree, "gen7_unique_icons")) == 35
    assert "enumerate(gen7_unique_icons, start=92)" in source
    assert "for index in range(92,127)" in source
    assert 'write_asset("firered/items/held_151.pkg"' in source

    header = struct.Struct("<4sHHH2xIII")
    entry = struct.Struct("<III")
    archive = ARCHIVE_PATH.read_bytes()
    magic, revision, entry_size, version, count, index_offset, _ = (
        header.unpack_from(archive)
    )
    version_source = (ROOT / "include/config/AssetPackVersion.h").read_text(
        encoding="ascii")
    expected_version = int(re.search(
        r"kRequiredAssetPackVersion\s*=\s*(\d+)", version_source).group(1))
    assert (magic, revision, entry_size, version) == (
        b"PGA1", 3, entry.size, expected_version)
    entries = {
        key: (offset, length)
        for index in range(count)
        for key, offset, length in (
            entry.unpack_from(archive, index_offset + index * entry.size),
        )
    }
    for item_id in (*range(1, 66), *range(67, 127), 151):
        relative = f"firered/items/held_{item_id:02}.pkg"
        loose = (ASSET_ROOT / relative).read_bytes()
        offset, length = entries[asset_hash(relative)]
        assert archive[offset:offset + length] == loose, relative

    audit = json.loads(
        (ROOT / ".generated" / "battle_animation_audit.json").read_text(
            encoding="utf-8"))
    water_shuriken = audit["moves"]["594"]
    assert set(water_shuriken["spriteTemplates"]) == {
        "gWaterShurikenStarTemplate",
        "gWaterShurikenRingTemplate",
        "gWaterShurikenImpactTemplate",
    }
    ring_asset = audit["spriteHandlers"][
        "gWaterShurikenRingTemplate"]["asset"]
    ring_visual = audit["visualAssets"][ring_asset]
    assert ring_visual["gfx"] == "yellow_star"
    assert (ring_visual["cellWidth"], ring_visual["cellHeight"]) == (32, 32)


def test_ble_trade_refreshes_display_deadline() -> None:
    source = MAIN_PATH.read_text(encoding="utf-8")
    trade = function_body(
        source,
        "void serviceMultiplayerTradeCommit()",
        "void serviceMultiplayerScreenUpdates()",
    )
    assert trade.count("lastInteractionMs = millis();") >= 2
    assert "playTradeAnimation(tradeOutgoingVisual, tradeIncomingVisual);" in trade

    cinematic = function_body(
        source,
        "void playTradeAnimation(",
        "bool verifySdAssetStream()",
    )
    assert cinematic.count("pokemonAssetPath(") >= 2
    assert '"/pokegochi/assets/pokemon/%sfront/%03u.pkg"' not in cinematic

    multiplayer_ui = function_body(
        source,
        "void drawMultiplayerPocket()",
        "void drawInventory()",
    )
    assert "multiplayer.localTradeConfirmed()" in multiplayer_ui
    assert '"CONFIRMED - WAITING FOR PLAYER..."' in multiplayer_ui
    assert 'mineConfirmed ? "CONFIRMED" : "CONFIRM"' in multiplayer_ui

    updates = function_body(
        source,
        "void serviceMultiplayerScreenUpdates()",
        "void loop()",
    )
    assert updates.index("if (!multiplayer.takeChanged()) return;") < updates.index(
        "lastInteractionMs = millis();"
    )
    assert updates.index("lastInteractionMs = millis();") < updates.index(
        "if (displaySleeping || swipeUnlock.active()) return;"
    )

    timeout = function_body(
        source,
        "void loop()",
        'extern "C" uint32_t pokegochi_android_frame_generation()',
    )
    assert "const uint32_t inactivityNow = millis();" in timeout
    assert "inactivityNow - lastInteractionMs" in timeout
    assert "now - lastInteractionMs" not in timeout


def test_unexpected_disconnect_rebuilds_home() -> None:
    source = MAIN_PATH.read_text(encoding="utf-8")
    assert "RTC_NOINIT_ATTR uint32_t multiplayerHomeResumeMarker;" not in source
    assert "restartForMultiplayerGraphicsRecovery" not in source
    assert "multiplayerRadioModeRequiresRestart" not in source
    renderer = (ROOT / "src/drivers/AssetRenderer.cpp").read_text(
        encoding="utf-8"
    )
    assert "bool AssetRenderer::normalSceneWorkspaceReady()" in renderer
    assert "sceneWorkspaceCapacity >= kSceneWorkspaceBytes" in renderer
    assert "bool AssetRenderer::homeSceneWorkspaceReady()" in renderer
    assert "sceneWorkspaceCapacity >= kCompactHomeSceneWorkspaceBytes" in renderer
    assert "bool AssetRenderer::uiSceneWorkspaceReady()" in renderer
    assert "sceneWorkspaceCapacity >= kCompactUiSceneWorkspaceBytes" in renderer
    assert "loadCompactTiledIndexed" in renderer
    assert "kResidentTiled8Bpp" in renderer
    assert "kResidentWideTiled8Bpp" in renderer
    assert 'std::strstr(path, "/summary_screen/")' in renderer
    assert 'std::strstr(path, "/pokecenter/")' in renderer

    radio_close = function_body(
        source,
        "bool disableMultiplayerRadioAndRestoreGraphics(const char* reason)",
        "bool disableMultiplayerRadioAfterBagClose()",
    )
    assert "AssetRenderer::uiSceneWorkspaceReady()" in radio_close
    assert "multiplayerGraphicsRestorePending = !ready;" in radio_close
    assert "ESP.restart()" not in radio_close
    assert "suspendGameSaveForWifi()" in radio_close
    assert "resumeGameSaveAfterRadioClose()" in radio_close
    assert radio_close.index("suspendGameSaveForWifi()") < radio_close.index(
        "multiplayer.setEnabled(false)"
    )
    assert radio_close.index("multiplayer.setEnabled(false)") < radio_close.index(
        "AssetRenderer::releaseSceneWorkspaceForRadio()"
    )
    assert radio_close.index("resumeGameSaveAfterRadioClose()") < radio_close.index(
        "AssetRenderer::restoreSceneWorkspace(false)"
    )

    bag_close = function_body(
        source,
        "void closeInventoryToHome()",
        "void serviceMultiplayerGraphicsRecovery(uint32_t now)",
    )
    assert "if (!multiplayerGraphicsRecoveryRequiredForBagClose())" in bag_close
    assert bag_close.index("if (!multiplayerGraphicsRecoveryRequiredForBagClose())") < (
        bag_close.index("disableMultiplayerRadioAfterBagClose()")
    )
    direct_close = bag_close[:bag_close.index("if (!disableMultiplayerRadioAfterBagClose())")]
    assert "screen = Screen::Home;" in direct_close
    assert "drawHome();" in direct_close
    assert "disableMultiplayerRadioAfterBagClose()" not in direct_close
    assert bag_close.index("disableMultiplayerRadioAfterBagClose()") < bag_close.index(
        "drawMultiplayerHomeRecoveryTransition();"
    )
    assert "panel.drawString(\"RESTORING DISPLAY...\"" in source

    recovery = function_body(
        source,
        "void serviceMultiplayerConnectionAndLaunch()",
        "void servicePvpSynchronization(uint32_t now)",
    )
    assert "multiplayer.takeUnexpectedDisconnect()" in recovery
    assert "restorePartyAfterPvp();" in recovery
    assert "inventoryMode = InventoryMode::Browse;" in recovery
    assert "boxOverlay = BoxOverlay::None;" in recovery
    assert 'disableMultiplayerRadioAndRestoreGraphics(' in recovery
    assert 'unexpectedDisconnect ? "Peer disconnected"' in recovery
    assert '"PLAYER DISCONNECTED."' in recovery
    assert "if (graphicsReady)" in recovery
    assert "multiplayerHomeReturnPending = true;" in recovery
    assert recovery.index("disableMultiplayerRadioAndRestoreGraphics(") < recovery.index(
        "drawHome();"
    )

    graphics_recovery = function_body(
        source,
        "void serviceMultiplayerGraphicsRecovery(uint32_t now)",
        "void refreshInventoryPanels(bool refreshPocket)",
    )
    assert "AssetRenderer::uiSceneWorkspaceReady()" in graphics_recovery
    assert "restartForMultiplayerGraphicsRecovery" not in graphics_recovery
    assert "ESP.restart()" not in graphics_recovery
    assert graphics_recovery.index("uiSceneWorkspaceReady()") < graphics_recovery.index(
        "screen = Screen::Home;"
    )
    assert graphics_recovery.index("screen = Screen::Home;") < graphics_recovery.index(
        "drawHome();"
    )

    setup = function_body(
        source,
        "void setup()",
        "__attribute__((noinline)) void serviceMultiplayerConnectionAndLaunch()",
    )
    assert "multiplayerHomeResumeMarker" not in setup
    assert "[BOOT] Resume multiplayer Home" not in setup

    loop = function_body(
        source,
        "void loop()",
        'extern "C" uint32_t pokegochi_android_frame_generation()',
    )
    assert loop.index("serviceMultiplayerConnectionAndLaunch();") < loop.index(
        "serviceMultiplayerTradeCommit();"
    )
    assert loop.index("serviceMultiplayerConnectionAndLaunch();") < loop.index(
        "serviceMultiplayerGraphicsRecovery(now);"
    )
    assert loop.index("serviceMultiplayerGraphicsRecovery(now);") < loop.index(
        "serviceMultiplayerTradeCommit();"
    )


def test_home_backgrounds_fit_lossless_compact_cache() -> None:
    backgrounds = sorted((ASSET_ROOT / "firered" / "ui").glob(
        "home_background_[0-9][0-9].pkg"
    ))
    assert len(backgrounds) == 47
    largest = 0
    for path in backgrounds:
        data = path.read_bytes()
        magic, width, height, _transparent, bpp, palette_count = (
            struct.unpack_from("<4sHHHBB", data)
        )
        assert magic == b"PKG2"
        assert (width, height) == (304, 134)
        assert bpp in (4, 6, 8)
        payload = data[12 + palette_count * 2:]

        def index_at(x: int, y: int) -> int:
            pixel = y * width + x
            if bpp == 4:
                packed = payload[pixel >> 1]
                return packed & 0x0F if pixel & 1 else packed >> 4
            if bpp == 6:
                bit = pixel * 6
                byte = bit >> 3
                shift = bit & 7
                pair = payload[byte] << 8
                if byte + 1 < len(payload):
                    pair |= payload[byte + 1]
                return (pair >> (10 - shift)) & 0x3F
            return payload[pixel]

        map_width = (width + 7) // 8
        map_height = (height + 7) // 8
        unique_tiles: set[bytes] = set()
        for tile_y in range(map_height):
            for tile_x in range(map_width):
                tile = bytes(
                    index_at(x, y) if x < width and y < height else 0
                    for local_y in range(8)
                    for local_x in range(8)
                    for x, y in ((tile_x * 8 + local_x,
                                  tile_y * 8 + local_y),)
                )
                unique_tiles.add(tile)
        assert len(unique_tiles) <= 256, path.name
        resident = map_width * map_height + len(unique_tiles) * 64
        largest = max(largest, resident)
        assert resident <= 14 * 1024, (path.name, resident)
    assert largest > 0


if __name__ == "__main__":
    test_complete_held_item_icon_catalogue()
    test_ble_trade_refreshes_display_deadline()
    test_unexpected_disconnect_rebuilds_home()
    test_home_backgrounds_fit_lossless_compact_cache()
    print("BLE trade and Bag asset regression checks passed")
