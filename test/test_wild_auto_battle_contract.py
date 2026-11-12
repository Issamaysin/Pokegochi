from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")


def _function_body(name: str, *, last: bool = False) -> str:
    start = MAIN.rindex(name) if last else MAIN.index(name)
    brace = MAIN.index("{", start)
    depth = 0
    for index in range(brace, len(MAIN)):
        if MAIN[index] == "{":
            depth += 1
        elif MAIN[index] == "}":
            depth -= 1
            if depth == 0:
                return MAIN[brace + 1:index]
    raise AssertionError(f"unterminated function: {name}")


def test_wild_auto_actions_refresh_display_inactivity_after_work():
    heartbeat = _function_body("void noteWildAutoBattleActivity(")
    assert "const uint32_t completedAt = millis();" in heartbeat
    assert "lastInteractionMs = completedAt;" in heartbeat
    assert "lastAnimationMs = completedAt;" in heartbeat

    service = _function_body("bool serviceWildAutoBattle(")
    # Journal advance, standalone message, final CONTINUE and move resolution
    # are the four automatic action paths. Every one must refresh inactivity.
    assert service.count("noteWildAutoBattleActivity(") == 4
    continue_position = service.index("handleTap(continueTap);")
    continue_heartbeat = service.index("noteWildAutoBattleActivity(", continue_position)
    assert continue_heartbeat > continue_position
    move_position = service.index("executeConfirmedBattleMove(moveSlot);")
    move_heartbeat = service.index("noteWildAutoBattleActivity(", move_position)
    assert move_heartbeat > move_position


def test_wild_auto_remains_strictly_wild_and_safety_gated():
    service = _function_body("bool serviceWildAutoBattle(")
    assert "gameSave.battle.kind != BattleKind::Wild" in service
    assert "wildAutoBattleSafetyAllowsProgress()" in service
    assert "choosePlayerAutoMove" in service


def test_wild_auto_toggle_is_sampled_inside_blocking_animations():
    toggle = _function_body(
        "void serviceWildAutoBattleToggleDuringAnimation(", last=True)
    assert "wildAutoBattleButtonVisible()" in toggle
    assert "filterGameTouch(touch.read())" in toggle
    assert "animationTouchLatched" in toggle
    assert "setWildAutoBattleEnabled(!wildAutoBattle.enabled)" in toggle

    program_frames = _function_body("void runBattleAnimationProgramFrames(")
    assert "serviceWildAutoBattleToggleDuringAnimation();" in program_frames
    hp_frames = _function_body("BattleHpAnimationOutcome animateBattleHpEvent(")
    assert "serviceWildAutoBattleToggleDuringAnimation();" in hp_frames

    # On the CYD, core 0 captures the coordinate while core 1 is blocked in
    # the LCD transaction. This is the path that makes a short A press reliable
    # instead of depending on a lucky gap between animation frames.
    monitor = _function_body("void monitorPhysicalBattleTextTouch(")
    assert "touch.read()" in monitor
    assert "physicalTouchHitsWildAutoButton(point)" in monitor
    assert "kPhysicalBattleInputToggleAuto" in monitor
    assert "kPhysicalBattleInputAdvanceAfterAnimation" in monitor
    assert "physicalBattleAnimationInputArmed || animationInputArmed" in monitor
    assert "armPhysicalBattleAnimationInputCapture();" in toggle

    hit_target = _function_body(
        "bool physicalTouchHitsWildAutoButton(", last=True)
    assert "kWildAutoBattleTouchWidth" in hit_target
    assert "kWildAutoBattleTouchHeight" in hit_target


def test_final_battle_text_uses_fast_physical_capture_without_home_leak():
    can_arm = _function_body("bool physicalBattleTextTapCanArm(")
    assert "standaloneAdvanceCaptureCanArm" in can_arm
    assert "message[0] != 0" in can_arm

    main_loop = _function_body("void loop()")
    finish = main_loop.index("if (battleFlowFinishDeferred)")
    begin = main_loop.index("beginPhysicalBattleTextPresentation();", finish)
    present = main_loop.index("presentRetainedFrame()", finish)
    assert begin < present
    assert "handleTap(continueTap);" in main_loop[main_loop.index(
        "if (battleFlowAdvanceDeferred)"):]

    # A tap made during faint/EXP playback waits for the first following text
    # frame to be committed, then advances on the next shallow loop. It cannot
    # overtake an HP/animation barrier or consume an unseen message.
    finish = _function_body("void finishPhysicalBattleTextPresentation(")
    assert "committed && physicalBattleTextTapCanArm()" in finish
    assert "takePhysicalBattleAdvanceAfterAnimation()" in finish
    assert "battleFlowAdvanceDeferred = true" in finish


if __name__ == "__main__":
    test_wild_auto_actions_refresh_display_inactivity_after_work()
    test_wild_auto_remains_strictly_wild_and_safety_gated()
    test_wild_auto_toggle_is_sampled_inside_blocking_animations()
    test_final_battle_text_uses_fast_physical_capture_without_home_leak()
    print("Wild auto-battle inactivity contract passed.")
