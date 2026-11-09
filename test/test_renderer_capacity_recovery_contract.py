"""Guard the constrained ESP32 battle renderer against capacity livelocks."""

import re
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
RENDERER = (ROOT / "src/drivers/AssetRenderer.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
ASSET_BUILDER = (ROOT / "scripts/build_sd_asset_pack.py").read_text(
    encoding="utf-8")
ASSET_ROOT = ROOT / ".generated/sdcard/pokegochi/assets"


def compact_lz_length(source: bytes) -> int:
    """Return the exact encoded length of one firmware block-LZ block."""
    match_depth = 10
    table = [[-1] * match_depth for _ in range(512)]
    position = 0
    literal_start = 0
    used = 0

    def extension_bytes(length: int) -> int:
        return length // 255 + 1

    def remember(bucket: int, candidate: int) -> None:
        history = table[bucket]
        history[1:] = history[:-1]
        history[0] = candidate

    while position + 4 <= len(source):
        mixed = (
            (source[position] << 16)
            ^ (source[position + 1] << 8)
            ^ source[position + 2]
            ^ (source[position + 3] * 2654435761)
        )
        bucket = mixed & 511
        reference = -1
        match_length = 3
        for possible in table[bucket]:
            if (
                possible < 0
                or possible >= position
                or source[possible:possible + 4]
                != source[position:position + 4]
            ):
                continue
            possible_length = 4
            while (
                position + possible_length < len(source)
                and source[possible + possible_length]
                == source[position + possible_length]
            ):
                possible_length += 1
            if possible_length > match_length:
                reference = possible
                match_length = possible_length
        remember(bucket, position)
        if reference < 0:
            position += 1
            continue
        literal_length = position - literal_start
        used += 1 + literal_length + 2
        if literal_length >= 15:
            used += extension_bytes(literal_length - 15)
        match_extra = match_length - 4
        if match_extra >= 15:
            used += extension_bytes(match_extra - 15)

        match_end = position + match_length
        for cursor in range(position + 1, match_end - 3):
            cursor_mixed = (
                (source[cursor] << 16)
                ^ (source[cursor + 1] << 8)
                ^ source[cursor + 2]
                ^ (source[cursor + 3] * 2654435761)
            )
            remember(cursor_mixed & 511, cursor)
        position = match_end
        literal_start = position

    if literal_start < len(source):
        literal_length = len(source) - literal_start
        used += 1 + literal_length
        if literal_length >= 15:
            used += extension_bytes(literal_length - 15)
    return used


def decoded_pkg_indices(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    magic, width, height, _transparent, bpp, palette_count = struct.unpack_from(
        "<4sHHHBB", data
    )
    assert magic == b"PKG2" and bpp in (4, 6, 8), path
    payload = data[12 + palette_count * 2:]
    indices = bytearray(width * height)
    if bpp == 8:
        indices[:] = payload[:len(indices)]
    elif bpp == 4:
        for pixel in range(len(indices)):
            packed = payload[pixel >> 1]
            indices[pixel] = packed & 0x0F if pixel & 1 else packed >> 4
    else:
        for pixel in range(len(indices)):
            bit = pixel * 6
            byte = bit >> 3
            shift = bit & 7
            pair = payload[byte] << 8
            if byte + 1 < len(payload):
                pair |= payload[byte + 1]
            indices[pixel] = (pair >> (10 - shift)) & 0x3F
    return width, height, bytes(indices)


def compact_ui_resident_bytes(path: Path) -> int:
    width, height, indices = decoded_pkg_indices(path)
    block_count = (height + 7) // 8
    used = (block_count + 1) * 2
    for block in range(block_count):
        first_row = block * 8
        rows = min(8, height - first_row)
        begin = first_row * width
        used += compact_lz_length(indices[begin:begin + rows * width])
    return used


def compact_flipped_tile_bytes(path: Path) -> int:
    """Return the exact resident size of the firmware's mirrored tile map."""
    width, height, indices = decoded_pkg_indices(path)
    map_width = (width + 7) // 8
    map_height = (height + 7) // 8
    unique_tiles: list[bytes] = []

    def flipped(tile: bytes, horizontal: bool, vertical: bool) -> bytes:
        return bytes(
            tile[(7 - y if vertical else y) * 8 +
                 (7 - x if horizontal else x)]
            for y in range(8)
            for x in range(8)
        )

    for tile_y in range(map_height):
        for tile_x in range(map_width):
            source_tile = bytearray(64)
            for local_y in range(8):
                y = tile_y * 8 + local_y
                if y >= height:
                    continue
                for local_x in range(8):
                    x = tile_x * 8 + local_x
                    if x < width:
                        source_tile[local_y * 8 + local_x] = indices[y * width + x]
            tile = bytes(source_tile)
            found = False
            for resident in unique_tiles:
                if (
                    tile == resident
                    or tile == flipped(resident, True, False)
                    or tile == flipped(resident, False, True)
                    or tile == flipped(resident, True, True)
                ):
                    found = True
                    break
            if not found:
                unique_tiles.append(tile)
    return map_width * map_height * 2 + len(unique_tiles) * 64


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
    assert "bool AssetRenderer::normalSceneWorkspaceReady()" in RENDERER
    assert "sceneWorkspaceCapacity >= kSceneWorkspaceBytes" in RENDERER
    assert "bool AssetRenderer::uiSceneWorkspaceReady()" in RENDERER
    assert "sceneWorkspaceCapacity >= kCompactUiSceneWorkspaceBytes" in RENDERER
    compact_home_limit = re.search(
        r"kMaximumAuditedCompactHomeSceneBytes\s*=\s*(\d+)U", RENDERER
    )
    compact_home_guard = re.search(
        r"kCompactHomeSceneGuardBytes\s*=\s*(\d+)U", RENDERER
    )
    compact_ui_limit = re.search(
        r"kMaximumAuditedCompactUiSceneBytes\s*=\s*(\d+)U", RENDERER
    )
    compact_ui_guard = re.search(
        r"kCompactUiSceneGuardBytes\s*=\s*(\d+)U", RENDERER
    )
    assert compact_home_limit and compact_home_guard
    assert compact_ui_limit and compact_ui_guard
    audited_home_maximum = int(compact_home_limit.group(1))
    compact_home_limit_bytes = (
        audited_home_maximum + int(compact_home_guard.group(1))
    )
    audited_maximum = int(compact_ui_limit.group(1))
    compact_ui_limit_bytes = audited_maximum + int(compact_ui_guard.group(1))
    assert (
        "kCompactHomeSceneWorkspaceBytes =\n"
        "    kMaximumAuditedCompactHomeSceneBytes + kCompactHomeSceneGuardBytes"
    ) in RENDERER
    assert (
        "kCompactUiSceneWorkspaceBytes =\n"
        "    kMaximumAuditedCompactUiSceneBytes + kCompactUiSceneGuardBytes"
    ) in RENDERER
    compact_ui_assets = sorted(
        (ASSET_ROOT / "firered/summary_screen").glob("*.pkg")
    ) + sorted((ASSET_ROOT / "pokecenter").glob("*.pkg"))
    assert compact_ui_assets
    compact_sizes = {
        path.relative_to(ASSET_ROOT).as_posix(): compact_ui_resident_bytes(path)
        for path in compact_ui_assets
    }
    largest_compact_asset, largest_compact_bytes = max(
        compact_sizes.items(), key=lambda item: item[1]
    )
    assert largest_compact_bytes == audited_maximum, (
        f"audited maximum is stale: {largest_compact_asset} uses "
        f"{largest_compact_bytes} bytes, source says {audited_maximum}"
    )
    assert largest_compact_bytes <= compact_ui_limit_bytes, (
        f"{largest_compact_asset} needs {largest_compact_bytes} bytes, "
        f"above the post-radio contract {compact_ui_limit_bytes}"
    )
    home_assets = sorted(
        (ASSET_ROOT / "firered/ui").glob("home_background_[0-9][0-9].pkg")
    )
    assert home_assets
    home_sizes = {
        path.relative_to(ASSET_ROOT).as_posix(): compact_flipped_tile_bytes(path)
        for path in home_assets
    }
    largest_home_asset, largest_home_bytes = max(
        home_sizes.items(), key=lambda item: item[1]
    )
    assert largest_home_bytes == audited_home_maximum, (
        f"audited Home maximum is stale: {largest_home_asset} uses "
        f"{largest_home_bytes} bytes, source says {audited_home_maximum}"
    )
    # COM7 measured a 10,740-byte largest block after a real peer-initiated
    # BLE disconnect; restoreSceneWorkspace keeps 128 bytes outside the arena.
    post_remote_disconnect_workspace = 10_740 - 128
    assert compact_home_limit_bytes <= post_remote_disconnect_workspace
    assert compact_ui_limit_bytes <= post_remote_disconnect_workspace
    assert "kResidentBlockLz8Bpp = 0x8AU" in RENDERER
    assert "kCompactLzMatchDepth = 10U" in RENDERER
    assert "kCompactLzHashBytes" in RENDERER
    assert "kCompactLzHashBytes <= kCacheArenaBytes" in RENDERER
    assert "cacheArenaUsed != 0U" in RENDERER
    assert "loadCompactBlockLzIndexed(" in RENDERER
    assert "decodeCompactLzBlock(" in RENDERER
    assert "kCompactBlockRows = 8U" in RENDERER
    assert "kResidentWideTiled8Bpp" in RENDERER
    assert "kResidentFlippedTiled8Bpp = 0x8BU" in RENDERER
    assert "compactTileMatches(" in RENDERER
    assert "residentBytes, true, true" in RENDERER
    assert 'std::strstr(path, "/summary_screen/")' in RENDERER
    assert 'std::strstr(path, "/pokecenter/")' in RENDERER
    assert "const bool compactBorrowedBattle" in RENDERER
    assert "sceneWorkspaceBorrowed && isOpaqueBattleReplacement(path)" in RENDERER
    assert "!loaded && compactBorrowedBattle" in RENDERER
    assert "reserve <= sceneWorkspaceCapacity" in RENDERER
    assert "residentBytes >= reserve" in RENDERER
    task_capacity = re.search(
        r"kBattleAnimationTaskCapacity\s*=\s*(\d+)", MAIN
    )
    assert task_capacity and int(task_capacity.group(1)) >= 20
    # Active Wi-Fi leaves too little heap for the canonical scene arena. PvP
    # must therefore borrow only the volatile Box tail, protect it from every
    # delete/promotion path, and reload the durable save before traversing the
    # collection again.
    assert "bool sceneWorkspaceBorrowed = false;" in RENDERER
    assert "bool AssetRenderer::borrowSceneWorkspace(" in RENDERER
    assert "void AssetRenderer::releaseBorrowedSceneWorkspace()" in RENDERER
    assert "if (sceneWorkspaceBorrowed) return false;" in RENDERER
    assert RENDERER.count("if (!sceneWorkspaceBorrowed) delete[] sceneWorkspace;") >= 2
    assert "if (sceneWorkspace && !sceneWorkspaceBorrowed) delete[] sceneWorkspace;" in RENDERER
    assert "constexpr size_t kRadioHeadroom = 4U * 1024U;" in RENDERER
    pvp_start = MAIN[
        MAIN.index("void startPvpBattle(const Multiplayer::BattleLaunch& launch) {"):
        MAIN.index("void resolvePvpCommands(")
    ]
    pvp_restore = MAIN[
        MAIN.index("void restorePartyAfterPvp() {"):
        MAIN.index("void startPvpBattle(const Multiplayer::BattleLaunch& launch) {")
    ]
    assert "&gameSave.collection.box[kPartyCapacity]" in pvp_start
    assert "AssetRenderer::borrowSceneWorkspace" in pvp_start
    # The transient Party copy must not push the physical build over its tiny
    # DRAM margin. RTC_NOINIT is safe only because every launch clears it
    # before any record is copied or read.
    assert "RTC_NOINIT_ATTR OwnedPokemon pvpPartyBackup[kPartyCapacity];" in MAIN
    assert pvp_start.index(
        "std::memset(pvpPartyBackup, 0, sizeof(pvpPartyBackup));"
    ) < pvp_start.index("pvpPartyBackup[slot] = *pokemon")
    assert "AssetRenderer::releaseBorrowedSceneWorkspace();" in pvp_restore
    assert pvp_restore.index("releaseBorrowedSceneWorkspace") < pvp_restore.index(
        "saves.loadOrCreate(gameSave)"
    )
    save_now = MAIN[
        MAIN.index("void saveNow() {"):
        MAIN.index("bool suspendGameSaveForWifi()", MAIN.index("void saveNow() {"))
    ]
    assert "if (pvpCollectionWorkspaceBorrowed) return;" in save_now
    assert save_now.index("if (pvpCollectionWorkspaceBorrowed) return;") < save_now.index(
        "saves.commit(gameSave)"
    )
    diagnostic_pvp = MAIN[
        MAIN.index('!std::strcmp(line, "ANIMPVP OFF")'):
        MAIN.index('!std::strncmp(line, "ANIMPLAY ", 9U)',
                   MAIN.index('!std::strcmp(line, "ANIMPVP OFF")'))
    ]
    assert "restorePartyAfterPvp();" in diagnostic_pvp
    assert "pvpCollectionWorkspaceBorrowed = true;" in diagnostic_pvp
    assert "pvpSessionActive = true;" in diagnostic_pvp
    assert "if (saveDirty) saveNow();" in diagnostic_pvp

    # Consecutive opaque move scenes are rebuilt in memory before the next
    # preload. They must not force an intermediate TFT commit, which used to
    # leave every later journal animation waiting behind a retained frame.
    next_restore_start = MAIN.index("bool restoreBattleArenaBeforeAnimation() {")
    next_restore_end = MAIN.index(
        "bool battleAnimationRuntimeActive() {", next_restore_start
    )
    next_restore = MAIN[next_restore_start:next_restore_end]
    assert "prepareBattleGraphics(" in next_restore
    assert "battleAnimationArenaRestorePending = false;" in next_restore
    assert "battleAnimationAtomicRevealPending = true;" in next_restore
    assert not re.search(
        r"^\s*(?:const bool \w+\s*=\s*)?presentRetainedFrame\(\);",
        next_restore,
        re.MULTILINE,
    )

    # Multi-background programs page one opaque plane through the same scene
    # slot. The deferred restore must track the page currently resident.
    ensure_page_start = MAIN.index("const auto ensureBackgroundPage =")
    ensure_page_end = MAIN.index(
        "const auto beginFadeToBackground =", ensure_page_start
    )
    ensure_page = MAIN[ensure_page_start:ensure_page_end]
    assert "battleAnimationPreparedBackgroundPath, nextPath" in ensure_page

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
