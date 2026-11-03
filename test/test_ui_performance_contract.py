"""Guard measured ESP32 menu optimizations against silent regressions."""

import re
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

    # Z-POWER is contextual in the move confirmation. A normal move owns a
    # compact two-row OK/CANCEL modal and the middle touch band becomes
    # CANCEL; only a compatible, currently usable Z-Move gets the third row.
    z_option_start = MAIN.index("bool battleMoveZPowerAvailable(")
    z_option_end = MAIN.index("void drawMoveSelect()", z_option_start)
    z_option = MAIN[z_option_start:z_option_end]
    assert "BattleEngine::canUseZMove(" in z_option

    move_draw_start = z_option_end
    move_draw_end = MAIN.index("void queueMoveLearning(", move_draw_start)
    move_draw = MAIN[move_draw_start:move_draw_end]
    assert "const int16_t confirmationHeight=zAvailable?90:60;" in move_draw
    assert "drawFireRedBattleWindow(112,42,96,confirmationHeight" in move_draw
    assert "if(zAvailable){" in move_draw
    assert 'fireRedText.drawString("CANCEL",132,zAvailable?109:80,1);' in move_draw
    assert "TFT_LIGHTGREY" not in move_draw[move_draw.index("if (moveConfirmSlot >= 0)"):]

    move_touch_start = MAIN.index("if (screen == Screen::MoveSelect) {")
    move_touch_end = MAIN.index("if(screen==Screen::EvolutionChoose)", move_touch_start)
    move_touch = MAIN[move_touch_start:move_touch_end]
    assert "const bool zAvailable=battleMoveZPowerAvailable(" in move_touch
    assert "if(zAvailable&&inside(p,112,72,96,30))" in move_touch
    assert "const int16_t cancelY=zAvailable?102:72;" in move_touch
    assert "THIS MOVE CAN'T USE THAT Z-CRYSTAL" not in move_touch

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
    contents_start = MAIN.index("void drawKeyItemPocketContents()")
    contents_end = MAIN.index("void drawInventoryEntryRow(", contents_start)
    contents = MAIN[contents_start:contents_end]
    assert "availableKeyItems(items)" in contents
    assert "keyItemPageCount()" in contents
    assert "keyItemPage * kKeyItemsPerPage" in contents
    assert "keyItemName(item)" in contents
    assert "constexpr uint8_t kKeyItemsPerPage = 3;" in MAIN
    assert "keyItemIconExtent(item)" in contents
    assert "kKeyItemIconCenterX - iconExtent / 2" in contents
    assert "centerY - iconExtent / 2" in contents
    assert "39 - extent / 2, 204 - extent / 2" in contents
    assert MAIN.count("drawKeyItemPocketContents();") == 2

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
    assert "pages > 1U && inside(p, 282, 8, 27, 55)" in inventory_touch
    assert "pages > 1U && inside(p, 282, 108, 27, 55)" in inventory_touch
    assert inventory_touch.count("activateSelectedKeyItem();") == 2
    assert "megaStoneKeyItemSelected() || dnaSplicersSelected() ||" in MAIN
    assert "necrozmaFusersSelected()) ? \"USE\" : \"OPEN\"" in MAIN
    assert '"PRISON BOTTLE"' in MAIN
    assert '"NECROZMA FUSERS"' in MAIN
    key_action_start = MAIN.index("void activateSelectedKeyItem()")
    key_action_end = MAIN.index("struct BoxContextEntry", key_action_start)
    key_action = MAIN[key_action_start:key_action_end]
    assert "openDnaSplicersFromInventory();" in key_action
    assert "usePrisonBottleFromInventory();" in key_action
    assert "openNecrozmaFusersFromInventory();" in key_action
    assert "openMegaStoneFromInventory();" in key_action

    # Mega Stone is a permanent Key Item shortcut after its challenge. The
    # current holder remains visually Mega while the player browses Party/Box;
    # confirmation transfers the unique item, while cancellation changes
    # nothing and a failed target restores the previous holder.
    assert "MegaChallengeSystem::completed(gameSave.gymProgress)" in MAIN
    mega_key_start = MAIN.index("bool openMegaStoneFromInventory()")
    mega_key_end = MAIN.index("bool transferMegaStoneFromKeyItem(", mega_key_start)
    mega_key = MAIN[mega_key_start:mega_key_end]
    assert "pokemon.heldItem == HeldItem::MegaStone" in mega_key
    assert "unequipHeldItem(" not in mega_key
    assert "gameSave.ownedMachines |= kMegaStoneOwnershipBit" in mega_key
    assert "inventoryMode = InventoryMode::ChooseTarget;" in mega_key
    transfer_end = MAIN.index("void activateSelectedKeyItem()", mega_key_end)
    transfer = MAIN[mega_key_end:transfer_end]
    assert "if (previous == &target) return true;" in transfer
    assert "unequipHeldItem(*previous" in transfer
    assert "applyInventoryItem(megaEntry, target)" in transfer
    assert "if (previous) applyInventoryItem(megaEntry, *previous);" in transfer
    assert MAIN.count("transferMegaStoneFromKeyItem(") == 3
    target_start = MAIN.index("void drawInventoryTarget()")
    target_end = MAIN.index("void drawMegaFormSelect()", target_start)
    target_picker = MAIN[target_start:target_end]
    assert "member->speciesId" in target_picker
    assert "member->shiny" in target_picker
    assert "CollectionLogic::resolvedForm(*member)" in target_picker
    assert "inventoryTargetEntry()" in MAIN
    assert MAIN.count("inventoryEntryName(inventoryTargetEntry())") == 2

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
    # Keep the full retained-layer table in the otherwise spare RTC slow bank.
    assert "RTC_NOINIT_ATTR BattleVisualLayer" in MAIN
    assert "battleVisualLayerStorage[kBattleVisualLayerCapacity]" in MAIN
    assert "battleVisualLayers = battleVisualLayerStorage" in MAIN
    assert "std::memset(battleVisualLayerStorage" in MAIN

    # Kids Paint clears its packed canvas every time the screen opens. Keep the
    # zero-filled surface out of firmware.bin so production remains OTA-safe.
    assert "RTC_NOINIT_ATTR alignas(4) uint8_t kidsPaintPixelsStorage" in MAIN
    paint_open = MAIN.index("bool beginKidsPaintCanvas()")
    paint_close = MAIN.index("void endKidsPaintCanvas()", paint_open)
    assert "kidsPaintPixels = kidsPaintPixelsStorage;" in MAIN[paint_open:paint_close]
    assert "std::memset(kidsPaintPixels, 0, kKidsCanvasBytes);" in MAIN[
        paint_open:paint_close
    ]
    esp_layers_start = MAIN.index(
        "#if defined(ARDUINO_ARCH_ESP32) && !defined(POKEGOCHI_ANDROID)\n"
        "// setup() clears this table before first use, so storing thousands of zero\n"
    )
    esp_layers_end = MAIN.index("#endif", esp_layers_start)
    assert "std::calloc" not in MAIN[esp_layers_start:esp_layers_end]
    assert "std::memset(battleVisualLayerStorage" in MAIN

    # The generated battle path is at most 76 bytes. Larger logical GBA OBJ
    # sets use the generic task-child pager instead of consuming more RTC or
    # dropping objects. Keep the catalog and retained terminator coupled.
    assert "char path[77]{};" in MAIN
    assert "constexpr uint8_t kBattleVisualLayerCapacity = 36;" in MAIN
    generated_animation = (
        ROOT / "src" / "game" / "BattleAnimationGenerated.inc"
    ).read_text(encoding="utf-8")
    sprite_names = re.findall(r'"([A-Za-z0-9_]+)"', generated_animation)
    battle_prefix = "/pokegochi/assets/firered/battle_anims/sprites/"
    assert max(len(f"{battle_prefix}{name}_00.pkg") for name in sprite_names) <= 76
    assert "battleAnimationVisualTaskPressureCounts()" in MAIN
    assert "battleAnimationTaskChildPagePending" in MAIN
    assert "(taskPressureCounts & 0x8000U)" in MAIN

    # The physical move sweep found a valid GBA scene which needs an opaque
    # replacement and a task field at the same time (Trick-or-Treat: Nightmare
    # + Scary Face). Keep those resources in the renderer's two independent
    # banks; merging them back into one path rejects the authored program.
    preload_start = MAIN.index("bool preloadMoveAnimation(")
    preload_end = MAIN.index("bool preloadMoveAnimationWithCapacityRetry(", preload_start)
    preload_move = MAIN[preload_start:preload_end]
    assert "char replacementPlanePath[112]{};" in preload_move
    assert "char effectPlanePath[112]{};" in preload_move
    assert "selectBattlePlane(replacementPlanePath, path, true);" in preload_move
    assert (
        "selectBattlePlane(effectPlanePath, battleAnimationFieldPath(field),\n"
        "                          false);"
    ) in preload_move
    assert preload_move.index("AssetRenderer::preload(replacementPlanePath)") < preload_move.index(
        "AssetRenderer::prepareBattlePlane("
    )
    assert "AssetRenderer::isPreloaded(replacementPlanePath)" in preload_move
    assert "AssetRenderer::isPreloaded(effectPlanePath)" in preload_move

    # A reachable program can replace BG1 more than once (Continental Crush
    # is the physical regression). These are sequential pages, while two task
    # fields would still compete for the same simultaneous plane. Playback
    # must page the later opaque background outside retained composition and
    # rebuild every resource that can spill from the scene workspace.
    assert "bool sequentialReplacement" in preload_move
    assert "std::strcmp(selected, path) != 0 && !sequentialReplacement" in preload_move
    playback_start = MAIN.index("void playBattleAnimationProgram(")
    playback_end = MAIN.index("bool preloadMoveAnimation(", playback_start)
    playback = MAIN[playback_start:playback_end]
    assert "const auto ensureBackgroundPage =" in playback
    assert "AssetRenderer::setCompositionActive(false);" in playback
    assert "preloadPresentedBattleBattlers()" in playback
    assert "preloadBattleAnimationSpriteManifest(" in playback
    assert "case BattleAnimationOpcode::ChangeBackground:" in playback
    assert "if (!ensureBackgroundPage(" in playback

    # Gen VII scripts legitimately exceed the GBA's 64 simultaneous OBJ
    # records before the physical 36-layer presenter pages them. The logical
    # ledger must retain every authored emission rather than recycling slots.
    assert "constexpr uint8_t kBattleAnimationParticleCapacity = 192;" in MAIN

    # Dense 3x programs repeatedly resolve the same immutable resident paths.
    # The fixed cache has no LRU eviction, so its existing stamp word is a path
    # hash and avoids a full strcmp against almost every slot on every stripe.
    assert "entry.stamp == hash" in RENDERER
    assert "slot->stamp = assetPathHash(path);" in RENDERER
    assert "cacheStamp" not in RENDERER

    # Full-field backgrounds must use mapped rows and precomputed palettes;
    # falling back to per-pixel division/palette transforms made 3x stutter.
    scrolled_start = RENDERER.index("bool drawIndexedScrolled(")
    scrolled_end = RENDERER.index("bool drawIndexedTransformed(", scrolled_start)
    scrolled = RENDERER[scrolled_start:scrolled_end]
    assert "uint16_t* sourceXMap" in scrolled
    assert "const bool directLinearIndexed" in scrolled
    assert "const bool directTiled4" in scrolled

    composite_start = RENDERER.index("bool AssetRenderer::drawPlaneComposite(")
    composite_end = RENDERER.index("bool AssetRenderer::drawRegion(", composite_start)
    composite = RENDERER[composite_start:composite_end]
    assert "uint16_t* baseXMap" in composite
    assert "uint16_t* overlayXMap" in composite
    assert "directPalette" in composite
    assert "scratch.stream.pixels" in composite

    # Alola grows GameSave beyond the largest post-FAT block. The scene bank
    # is still empty at this point, so boot must lend and restore that exact
    # allocation around GameSave before PersistentSave obtains its migration
    # scratch. This preserves both the full Box and the full renderer arena.
    game_save_allocation = MAIN.index(
        "gameSaveStorage = static_cast<GameSave*>(heap_caps_calloc"
    )
    loan = MAIN.rfind(
        "AssetRenderer::releaseSceneWorkspaceForStorage();",
        0,
        game_save_allocation,
    )
    restore = MAIN.index(
        "AssetRenderer::restoreSceneWorkspace(false)", game_save_allocation
    )
    persistent_begin = MAIN.index("saves.begin(", restore)
    assert loan != -1 and loan < game_save_allocation < restore < persistent_begin
    # The production build keeps this diagnostic deliberately short to stay
    # inside the OTA slot; pin the restore guard itself, not old prose.
    assert "if (!AssetRenderer::restoreSceneWorkspace(false))" in MAIN[
        game_save_allocation:persistent_begin
    ]

    commit_start = SAVE.index("bool PersistentSave::commit(")
    commit_end = SAVE.index("bool PersistentSave::reset(", commit_start)
    commit_body = SAVE[commit_start:commit_end]
    assert "kSaveWriteChunkBytes = 512U" in commit_body
    assert "writeAll(&save, sizeof(save))" in commit_body
    print("UI performance contract passed.")


if __name__ == "__main__":
    main()
