"""Guard the constrained ESP32 battle renderer against capacity livelocks."""

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
RENDERER = (ROOT / "src/drivers/AssetRenderer.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
ASSET_BUILDER = (ROOT / "scripts/build_sd_asset_pack.py").read_text(
    encoding="utf-8")


def main() -> None:
    # Keep fixed renderer scratch and Home palettes within the constrained
    # ESP32 DRAM budget while preserving the compositor's minimum x-map size.
    region_height = re.search(
        r"kRegionBufferHeight\s*=\s*(\d+)", RENDERER
    )
    assert region_height and int(region_height.group(1)) <= 16
    decode_rows = re.search(r"kDecodeRows\s*=\s*(\d+)", RENDERER)
    assert decode_rows and int(decode_rows.group(1)) == 4
    home_palette = re.search(
        r"kHomeLayerPaletteCapacity\s*=\s*(\d+)", RENDERER
    )
    assert home_palette and int(home_palette.group(1)) == 64
    assert "uint16_t palette[kHomeLayerPaletteCapacity]" in RENDERER
    assert "info.paletteCount > kHomeLayerPaletteCapacity" in RENDERER
    scene_kib = re.search(
        r"kSceneWorkspaceBytes\s*=\s*(\d+)U?\s*\*\s*1024U?", RENDERER
    )
    assert scene_kib and int(scene_kib.group(1)) * 1024 >= 58_144

    # GameSave must claim the only block it fits in before the renderer is
    # restored. The large Surf/Muddy planes are compacted inside that smaller
    # scene bank instead of forcing an impossible allocation order.
    storage_release = MAIN.index(
        "AssetRenderer::releaseSceneWorkspaceForStorage();"
    )
    storage_restore = MAIN.index(
        "AssetRenderer::restoreSceneWorkspace(false)", storage_release
    )
    game_save_allocation = MAIN.index(
        "gameSaveStorage = static_cast<GameSave*>", storage_release
    )
    assert storage_release < game_save_allocation < storage_restore

    assert "kResidentTiled4Bpp = 0x84U" in RENDERER
    assert "tail.bitsPerPixel != kResidentTiled4Bpp" in RENDERER
    assert "info.bitsPerPixel == kResidentTiled4Bpp" in RENDERER
    assert "def build_compact_battle_wave_assets(" in ASSET_BUILDER
    compact_builder = ASSET_BUILDER[
        ASSET_BUILDER.index("def build_compact_battle_wave_assets("):
        ASSET_BUILDER.index("def compose_battle_terrain(")
    ]
    assert "for muddy, move_name in" in compact_builder
    assert "tiled_4bpp=True" in compact_builder
    assert '"--compact-battle-waves-only"' in ASSET_BUILDER
    # Worst-case 256 unique 8x8 tiles still leave ample room beside a full
    # 320x184 4-bpp terrain; canonical waves use only 112--114 unique tiles.
    tiled_wave_maximum = 64 * 14 + 256 * 32
    terrain_bytes = 320 * 184 // 2
    assert terrain_bytes + 32 + tiled_wave_maximum < 57_204

    # Scrolling move planes are the dominant 2x/3x cost. Their linear 4/8-bpp
    # path must reuse a pre-encoded palette and keep the pixel-format branch
    # outside the inner loop. Affine particles must damage their transformed
    # rectangle instead of the former oversized square.
    assert "const bool directLinearIndexed" in RENDERER
    assert "uint16_t* const encodedPalette = scratch.stream.pixels;" in RENDERER
    assert "if (image.bitsPerPixel == 8U)" in RENDERER
    assert "void battleVisualLayerBounds(" in MAIN
    assert "battleVisualLayerBounds(layer, result.left, result.top," in MAIN

    cadence_start = MAIN.index("uint8_t battleAnimationProgramSampleStride(")
    cadence_end = MAIN.index("void playBattleAnimationProgram(", cadence_start)
    cadence = MAIN[cadence_start:cadence_end]
    assert "speed >= 3U ? 12U : 8U" in cadence
    assert "foregroundStride * 2U" in cadence

    # Both failure exits after the old arena is released must rebuild that
    # arena and its retained terrain before returning false.
    promotion_start = RENDERER.index("bool promoteBattleSceneWorkspace(")
    promotion_end = RENDERER.index("bool preloadBattlePlane(", promotion_start)
    promotion = RENDERER[promotion_start:promotion_end]
    assert "const size_t previousCapacity = sceneWorkspaceCapacity;" in promotion
    assert promotion.count("restoreBattleSceneAfterPromotionFailure(") == 2
    rollback_start = RENDERER.index(
        "bool restoreBattleSceneAfterPromotionFailure("
    )
    rollback = RENDERER[rollback_start:promotion_start]
    assert "preloadLargeAsset(retainedLargePath)" in rollback
    assert "sceneWorkspaceCapacity = sceneWorkspace ? requested : 0U;" in rollback

    # Even an unrecoverable missing package is a visual degradation, not a
    # journal lock: the procedural arena must compile for ESP32 as well as
    # Android and complete the retained frame transaction.
    arena_start = MAIN.index(
        "void drawBattleArena(OwnedPokemon* player, OwnedPokemon* opponent, "
        "bool hideOpponent) {"
    )
    arena_end = MAIN.index(
        "void drawBattleTransformedBattlers(bool backgroundLayer)", arena_start
    )
    arena = MAIN[arena_start:arena_end]
    fallback_marker = arena.index(
        "Neither backend may turn a transient scene-bank miss"
    )
    fallback_body = arena[fallback_marker:]
    fallback_end = fallback_body.index("baseDrawn = true;")
    fallback_code = fallback_body[:fallback_end]
    assert "#else" in fallback_code
    assert "display.fillRect(0, 0, 320, 96" in fallback_code
    assert "battle base unavailable" not in fallback_body

    print("Renderer capacity recovery contract passed.")


if __name__ == "__main__":
    main()
