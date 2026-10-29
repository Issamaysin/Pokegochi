"""Guard measured ESP32 menu optimizations against silent regressions."""

from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
SAVE = (ROOT / "src/services/PersistentSave.cpp").read_text(encoding="utf-8")
RENDERER = (ROOT / "src/drivers/AssetRenderer.cpp").read_text(encoding="utf-8")
FONT = (ROOT / "src/drivers/FireRedFont.cpp").read_text(encoding="utf-8")


def main() -> None:
    scene_start = MAIN.index("bool prepareUiScene(")
    scene_end = MAIN.index("void invalidatePixels(", scene_start)
    scene = MAIN[scene_start:scene_end]
    assert scene.index("AssetRenderer::primePreloads(assets, count)") < scene.index(
        "AssetRenderer::preload(assets[item])"
    )

    # Summary is repainted once per 8-row TFT stripe. Its expensive learnset
    # traversal must be cached, and Pokegochi-only pages must not load an SD
    # tilemap that their opaque body immediately covers.
    summary_assets_start = MAIN.index("bool prepareSummarySceneAssets(")
    summary_assets_end = MAIN.index("void drawBattleParty()", summary_assets_start)
    summary_assets = MAIN[summary_assets_start:summary_assets_end]
    assert "if (page >= 3U) return true;" in summary_assets
    assert "const bool needsFront = page == 0U;" in summary_assets

    summary_background_start = MAIN.index("bool drawFireRedSummaryBackground(")
    summary_background_end = MAIN.index("void rebuildSummaryMovePool(", summary_background_start)
    summary_background = MAIN[summary_background_start:summary_background_end]
    assert "if (page >= 3U)" in summary_background
    assert summary_background.index("if (page >= 3U)") < summary_background.index(
        "AssetRenderer::draw(display, path, 0, 0)"
    )

    summary_pool_start = MAIN.index("void rebuildSummaryMovePool(")
    summary_pool_end = MAIN.index("uint8_t drawSummaryScrollableText(", summary_pool_start)
    summary_pool = MAIN[summary_pool_start:summary_pool_end]
    assert "summaryMovePoolCacheValid && summaryMovePoolCacheKey == cacheKey" in summary_pool
    assert summary_pool.index("summaryMovePoolCacheValid &&") < summary_pool.index(
        "relearnableMovesForPokemon("
    )
    assert "MegaEvolution::variantFor(pokemon)" in summary_pool

    summary_touch_start = MAIN.index("if(screen==Screen::BattleSummary){")
    summary_touch_end = MAIN.index("if (screen == Screen::Bag) {", summary_touch_start)
    summary_touch = MAIN[summary_touch_start:summary_touch_end]
    assert "invalidatePixels(9,48,184,160);requestRetainedFrame();" in summary_touch
    assert "if(summaryMoveSlot!=slot)" in summary_touch

    font_string_start = FONT.index("void FireRedFont::drawString(")
    font_string_end = FONT.index("void FireRedFont::drawStringFitted(", font_string_start)
    font_string = FONT[font_string_start:font_string_end]
    assert "display_.logicalClipBottom()" in font_string
    assert font_string.index("display_.logicalClipBottom()") < font_string.index(
        "std::fill_n(textRaster"
    )

    # The ordinary Trainer Card must not advertise a future Kyurem reward.
    # Both a full Bag composition and a retained partial refresh share one
    # body; that body reveals DNA Splicers only after its real ownership bit
    # has been granted by obtaining Kyurem.
    assert "DNA SPLICERS APPEAR HERE WHEN KYUREM IS OWNED." not in MAIN
    assert '"CHECK BADGES AND LEAGUE PROGRESS."' in MAIN
    contents_start = MAIN.index("void drawTrainerCardPocketContents()")
    contents_end = MAIN.index("void drawInventoryEntryRow(", contents_start)
    contents = MAIN[contents_start:contents_end]
    assert "UnovaForms::hasDnaSplicers(gameSave.gen5UniqueItems)" in contents
    assert contents.index("if (showDnaSplicers)") < contents.index('"DNA SPLICERS"')
    assert "drawInventoryMessage(kTrainerCardPocketDescription" in contents
    assert MAIN.count("drawTrainerCardPocketContents();") == 2

    # DNA Splicers is a real Key Item action, not decorative artwork beside a
    # Trainer Card button. It must locate Kyurem's physical Box page and open
    # the same form selector used by BOX -> FORM.
    splicers_start = MAIN.index("bool openDnaSplicersFromInventory()")
    splicers_end = MAIN.index("struct BoxContextEntry", splicers_start)
    splicers = MAIN[splicers_start:splicers_end]
    assert "candidate.speciesId != 646U" in splicers
    assert "boxPage = static_cast<uint8_t>(kyuremIndex / kBoxPageSize);" in splicers
    assert "boxOverlay = BoxOverlay::FormSelect;" in splicers
    assert "screen = Screen::Box;" in splicers

    inventory_touch_start = MAIN.index("if (screen == Screen::Inventory) {")
    inventory_touch_end = MAIN.index("if(screen==Screen::TrainerCard)", inventory_touch_start)
    inventory_touch = MAIN[inventory_touch_start:inventory_touch_end]
    assert "inside(p, 106, kDnaSplicersY, 203, kKeyItemCompactH)" in inventory_touch
    assert inventory_touch.count("openDnaSplicersFromInventory();") == 2
    assert "(dnaSplicersSelected() || prisonBottleSelected()) ? \"USE\" : \"OPEN\"" in MAIN
    assert '"PRISON BOTTLE"' in contents
    assert "usePrisonBottleFromInventory();" in inventory_touch

    mart_start = MAIN.index(
        "if (inside(p, kHomeBottomX[5], kHomeBottomY, kHomeButtonWidth, "
        "kHomeButtonHeight))"
    )
    mart_end = MAIN.index("// Restore only the local balloon area", mart_start)
    mart_handler = MAIN[mart_start:mart_end]

    assert "const bool rotationChanged = Economy::refreshIfDue(" in mart_handler
    assert "const bool seenDayChanged = visibleMartSeenDay() != gameSave.mart.day;" in mart_handler
    assert "if (rotationChanged || seenDayChanged)" in mart_handler

    save_call = mart_handler.index("saveNow();")
    guarded_save = mart_handler.index("if (rotationChanged || seenDayChanged)")
    assert save_call > guarded_save, "unchanged Mart still performs a synchronous save"

    # The expanded living-dex record leaves no contiguous SPIFFS write block
    # beside a full scene on the physical no-PSRAM ESP32. A save must lend and
    # restore that arena under pressure, invalidate stale scene descriptors,
    # and stream the payload in bounded writes.
    save_start = MAIN.index("void saveNow() {")
    save_end = MAIN.index("void serviceDeferredHomeGiftSave()", save_start)
    save_body = MAIN[save_start:save_end]
    assert "kSaveMinimumContiguousHeap" in save_body
    assert "AssetRenderer::releaseSceneWorkspaceForStorage()" in save_body
    assert "wirelessUpdate.liveLogActive()" in save_body
    assert "AssetRenderer::restoreSceneWorkspace(rendererRadioActive)" in save_body
    assert "preparedUiSceneComplete = false" in save_body
    assert "rendererSceneRewarmPending = restored" in save_body
    assert "battleFlow.phase != BattleFlowPhase::Idle" in save_body
    assert "battleAnimationAtomicCommitActive" in save_body

    # Every persistent battle action starts its presentation journal before
    # attempting the forced checkpoint. saveNow() then defers through the
    # non-idle flow instead of lending the scene arena between resolution and
    # the first move frame.
    run_start = MAIN.index("void executeBattleRunAttempt()")
    run_end = MAIN.index("void executeStruggleBattleMove()", run_start)
    run_body = MAIN[run_start:run_end]
    assert run_body.index("beginBattleEventFlow(") < run_body.index("saveNow();")

    struggle_start = run_end
    struggle_end = MAIN.index("void executeSelectedBattleSwitch(", struggle_start)
    struggle_body = MAIN[struggle_start:struggle_end]
    assert struggle_body.index("beginBattleEventFlow(") < struggle_body.index("saveNow();")

    switch_start = struggle_end
    switch_end = MAIN.index("bool executeBattleBall(", switch_start)
    switch_body = MAIN[switch_start:switch_end]
    assert switch_body.index("beginBattleEventFlow(") < switch_body.index("saveNow();")

    ball_start = switch_end
    ball_end = MAIN.index("void handleTap(", ball_start)
    ball_body = MAIN[ball_start:ball_end]
    assert ball_body.index("beginBattleEventFlow(") < ball_body.index("saveNow();")

    checkpoint_start = MAIN.index("bool saveCheckpointDue(")
    checkpoint_end = MAIN.index("void saveNow()", checkpoint_start)
    checkpoint = MAIN[checkpoint_start:checkpoint_end]
    assert "elapsed < 0x80000000UL" in checkpoint
    assert "elapsed >= 60000U" in checkpoint
    assert "now - lastSaveMs" not in MAIN
    assert "now-lastSaveMs" not in MAIN

    # A full BLE BattleStateSync is several KiB. It must live in a noinline
    # service frame which has returned before loop() enters authored battle
    # animations, otherwise the ESP32 task stack is nearly exhausted.
    sync_start = MAIN.index("void servicePvpSynchronization(")
    sync_end = MAIN.index("void serviceMultiplayerTradeCommit()", sync_start)
    sync_body = MAIN[sync_start:sync_end]
    assert "Multiplayer::BattleStateSync authoritativeState{};" in sync_body
    loop_start = MAIN.index("void loop()")
    loop_end = MAIN.index("#if defined(POKEGOCHI_ANDROID)", loop_start)
    loop_prefix = MAIN[loop_start:loop_end]
    assert "servicePvpSynchronization(now);" in MAIN[loop_start:]
    assert "Multiplayer::BattleStateSync" not in MAIN[loop_start:]

    rewarm_start = MAIN.index(
        "if (rendererSceneRewarmPending && !renderingRetainedFrame)"
    )
    present_start = MAIN.index("presentRetainedFrame();", rewarm_start)
    rewarm_body = MAIN[rewarm_start:present_start]
    assert "redrawCurrentScreen();" in rewarm_body
    assert "rendererSceneRewarmPending = false" in rewarm_body

    presentation_start = MAIN.index("bool beginScreenPresentation(")
    presentation_end = MAIN.index("constexpr int16_t kHomeButtonWidth", presentation_start)
    presentation = MAIN[presentation_start:presentation_end]
    assert "const bool forcedWorkspaceRebuild = rendererSceneRewarmPending;" in presentation
    assert "if (forcedWorkspaceRebuild) rendererSceneRewarmPending = false;" in presentation
    assert "const bool entering = forcedWorkspaceRebuild ||" in presentation

    release_start = RENDERER.index("void releaseSceneWorkspace(")
    release_end = RENDERER.index("bool AssetRenderer::restoreSceneWorkspace", release_start)
    assert "AssetRenderer::clearCache()" in RENDERER[release_start:release_end]

    # The Kalos living Dex is 53,712 bytes. On the no-PSRAM board the SD/FAT
    # mount leaves a 53,236-byte ordinary-heap block when the retained visual
    # layer table is there, causing an immediate reboot before save loading.
    # Keep the full 36-layer table in the otherwise spare RTC slow bank.
    assert "RTC_DATA_ATTR BattleVisualLayer" in MAIN
    assert "battleVisualLayerStorage[kBattleVisualLayerCapacity]" in MAIN
    assert "battleVisualLayers = battleVisualLayerStorage" in MAIN
    assert "std::memset(battleVisualLayerStorage" in MAIN
    esp_layers_start = MAIN.index(
        "#if defined(ARDUINO_ARCH_ESP32) && !defined(POKEGOCHI_ANDROID)\n"
        "RTC_DATA_ATTR BattleVisualLayer"
    )
    esp_layers_end = MAIN.index("#endif", esp_layers_start)
    assert "std::calloc" not in MAIN[esp_layers_start:esp_layers_end]

    commit_start = SAVE.index("bool PersistentSave::commit(")
    commit_end = SAVE.index("bool PersistentSave::reset(", commit_start)
    commit_body = SAVE[commit_start:commit_end]
    assert "kSaveWriteChunkBytes = 512U" in commit_body
    assert "writeAll(&save, sizeof(save))" in commit_body
    print("UI performance contract passed.")


if __name__ == "__main__":
    main()
