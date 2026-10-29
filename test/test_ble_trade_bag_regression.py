from __future__ import annotations

import ast
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

    header = struct.Struct("<4sHHH2xIII")
    entry = struct.Struct("<III")
    archive = ARCHIVE_PATH.read_bytes()
    magic, revision, entry_size, version, count, index_offset, _ = (
        header.unpack_from(archive)
    )
    assert (magic, revision, entry_size, version) == (b"PGA1", 3, entry.size, 46)
    entries = {
        key: (offset, length)
        for index in range(count)
        for key, offset, length in (
            entry.unpack_from(archive, index_offset + index * entry.size),
        )
    }
    for item_id in (*range(1, 66), *range(67, 92)):
        relative = f"firered/items/held_{item_id:02}.pkg"
        loose = (ASSET_ROOT / relative).read_bytes()
        offset, length = entries[asset_hash(relative)]
        assert archive[offset:offset + length] == loose, relative


def test_ble_trade_refreshes_display_deadline() -> None:
    source = MAIN_PATH.read_text(encoding="utf-8")
    trade = function_body(
        source,
        "void serviceMultiplayerTradeCommit()",
        "void serviceMultiplayerScreenUpdates()",
    )
    assert trade.count("lastInteractionMs = millis();") >= 2
    assert "playTradeAnimation(tradeOutgoingVisual, tradeIncomingVisual);" in trade

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


if __name__ == "__main__":
    test_complete_held_item_icon_catalogue()
    test_ble_trade_refreshes_display_deadline()
    print("BLE trade and Bag asset regression checks passed")
