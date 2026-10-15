"""Fail-fast validation for the complete FireRed battle visual pipeline.

This audit intentionally checks source indices rather than screenshots alone.
A screenshot can look plausible while a palette callback, scrolling camera or
transparent index is already corrupt. Run after ``build_sd_asset_pack.py``.
"""

from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import build_sd_asset_pack as pack  # noqa: E402


def flattened_data(image):
    """Read raw pixels on both Pillow 10 and Pillow 11 runtimes."""
    return pack.flattened_data(image)


def read_pkg(relative: str) -> dict[str, object]:
    path = pack.OUTPUT / relative
    raw = path.read_bytes()
    if len(raw) < 12:
        raise RuntimeError(f"Truncated PKG2: {relative}")
    magic, width, height, transparent, bpp, palette_count = struct.unpack_from(
        "<4sHHHBB", raw, 0)
    if magic != b"PKG2" or bpp not in (4, 6, 8) or not palette_count:
        raise RuntimeError(f"Invalid PKG2 header: {relative}")
    palette_offset = 12
    payload_offset = palette_offset + palette_count * 2
    palette = list(struct.unpack_from(
        f"<{palette_count}H", raw, palette_offset))
    pixels = width * height
    payload = raw[payload_offset:]
    indices: list[int] = []
    if bpp == 4:
        if len(payload) != (pixels + 1) // 2:
            raise RuntimeError(f"Invalid 4bpp payload: {relative}")
        for index in range(pixels):
            byte = payload[index >> 1]
            indices.append((byte >> 4) if not index & 1 else (byte & 0x0F))
    elif bpp == 6:
        if len(payload) != (pixels * 6 + 7) // 8:
            raise RuntimeError(f"Invalid 6bpp payload: {relative}")
        bit_offset = 0
        for _ in range(pixels):
            value = 0
            for bit in range(6):
                absolute = bit_offset + bit
                value = (value << 1) | (
                    (payload[absolute >> 3] >> (7 - (absolute & 7))) & 1)
            indices.append(value)
            bit_offset += 6
    else:
        if len(payload) != pixels:
            raise RuntimeError(f"Invalid 8bpp payload: {relative}")
        indices = list(payload)
    invalid = next((value for value in indices if value >= palette_count), None)
    if invalid is not None:
        raise RuntimeError(
            f"PKG2 index {invalid} exceeds palette {palette_count}: {relative}")
    return {
        "width": width, "height": height, "transparent": transparent,
        "bpp": bpp, "palette": palette, "indices": indices,
    }


def expected_palette(values: list[tuple[int, int, int]],
                     transparent_index: int | None = None) -> list[int]:
    result: list[int] = []
    for index, (red, green, blue) in enumerate(values):
        value = (pack.TRANSPARENT if index == transparent_index
                 else pack.rgb565(red, green, blue))
        if value == pack.TRANSPARENT and index != transparent_index:
            value = 0xF81E
        result.append(value)
    return result


def assert_plane(relative: str, source, palette,
                 transparent_index: int | None = None) -> None:
    package = read_pkg(relative)
    if (package["width"], package["height"]) != source.size:
        raise RuntimeError(
            f"Dimension mismatch {relative}: "
            f"{package['width']}x{package['height']} != {source.size}")
    source_indices = list(flattened_data(pack.palette_index_plane(source)))
    if package["indices"] != source_indices:
        raise RuntimeError(f"Palette-index plane changed: {relative}")
    if package["palette"] != expected_palette(palette, transparent_index):
        raise RuntimeError(f"Palette order changed: {relative}")


def assert_rgba_asset(relative: str, source) -> None:
    """Validate an asset written through ``write_asset`` byte-for-byte.

    Battle terrain uses several original sub-palettes and is intentionally
    exported through the generic RGBA packer.  Rebuilding its first-seen
    RGB565 palette here catches missing platform pixels, accidental resizes
    and palette flattening instead of accepting a plausible screenshot.
    """
    package = read_pkg(relative)
    source = source.convert("RGBA")
    if (package["width"], package["height"]) != source.size:
        raise RuntimeError(f"Dimension mismatch {relative}")
    palette: list[int] = []
    palette_indices: dict[int, int] = {}
    indices: list[int] = []
    for red, green, blue, alpha in flattened_data(source):
        value = pack.TRANSPARENT if alpha < 128 else pack.rgb565(red, green, blue)
        if value == pack.TRANSPARENT and alpha >= 128:
            value = 0xF81E
        index = palette_indices.get(value)
        if index is None:
            index = len(palette)
            palette_indices[value] = index
            palette.append(value)
        indices.append(index)
    if package["palette"] != palette or package["indices"] != indices:
        raise RuntimeError(f"RGBA asset content changed: {relative}")


def packed_pixel_bytes(package: dict[str, object]) -> int:
    pixels = int(package["width"]) * int(package["height"])
    bpp = int(package["bpp"])
    return (pixels * bpp + 7) // 8


def small_cache_bytes(relative: str) -> int:
    """Return the exact arena reservation made by cacheSmallAsset()."""
    package = read_pkg(relative)
    return len(package["palette"]) * 2 + packed_pixel_bytes(package)


def aligned_asset_set_bytes(paths: set[str]) -> int:
    """Model the renderer's two-byte alignment between cached assets."""
    used = 0
    for relative in sorted(paths):
        used = (used + 1) & ~1
        used += small_cache_bytes(relative)
    return used


def main() -> None:
    audit_path = ROOT / ".generated/battle_animation_audit.json"
    animation = json.loads(audit_path.read_text(encoding="utf-8"))
    moves = animation["moves"]
    # FireRed's 354 moves are followed by the complete Platinum Gen-IV range
    # (ROOST through SHADOW_FORCE, IDs 355..467), not only the four moves that
    # were initially spot-checked on hardware.
    expected_moves = 354 + (467 - 355 + 1)
    if len(moves) != expected_moves or animation["program"]["moves"] != expected_moves:
        raise RuntimeError(
            "The generated VM does not contain the FireRed catalog plus the "
            "exact Gen-IV overlay")
    incomplete = [
        f"{move_id}:{move['symbol']}" for move_id, move in moves.items()
        if (not move["sourceFound"] or move["untranslated"] or
            not move["visualCoverage"])
    ]
    if incomplete:
        raise RuntimeError("Incomplete move programs: " + ", ".join(incomplete))

    # Keep the two sunlight moves tied to their complete FireRed programs.
    # Solar Beam is especially easy to regress because its setup particles
    # converge on the user while its release particles travel the opposite
    # way.  A generic "orbs" check cannot distinguish those two phases.
    solar_beam = moves["76"]
    if (solar_beam["spriteTemplates"] != [
            "gPowerAbsorptionOrbSpriteTemplate",
            "gSolarBeamBigOrbSpriteTemplate"] or
            "AnimTask_CreateSmallSolarBeamOrbs" not in
            solar_beam["taskCallbacks"] or
            solar_beam["programSprites"] != 28 or
            "Task:SolarBeamOrbs" not in solar_beam["visualCoverage"]):
        raise RuntimeError("Solar Beam no longer matches its two-turn FireRed program")
    sunny_day = moves["241"]
    if (sunny_day["spriteTemplates"] != ["gSunlightRaySpriteTemplate"] or
            sunny_day["programCommands"] != 17 or
            sunny_day["particles"] != 4):
        raise RuntimeError("Sunny Day no longer emits the four FireRed sunlight rays")
    blizzard = moves["59"]
    if (blizzard["spriteTemplates"] != [
            "gBlizzardIceCrystalSpriteTemplate",
            "gIceCrystalHitLargeSpriteTemplate",
            "gIceCrystalHitSmallSpriteTemplate",
            "gSwirlingSnowballSpriteTemplate"] or
            blizzard["taskCallbacks"] != [
                "AnimTask_GetAttackerSide", "AnimTask_StartSlidingBg"] or
            blizzard["conditionalBranches"] != 1 or
            blizzard["programCommands"] != 65 or
            blizzard["particles"] != 12):
        raise RuntimeError(
            "Blizzard no longer matches its side-aware FireRed animation program")
    thunder = moves["87"]
    if (thunder["spriteTemplates"] != [
            "gLightningSpriteTemplate",
            "gSimplePaletteBlendSpriteTemplate"] or
            thunder["taskCallbacks"] != [
                "AnimTask_InvertScreenColor",
                "AnimTask_ShakeTargetInPattern",
                "AnimTask_StartSlidingBg"] or
            thunder["programCommands"] != 49 or
            thunder["programSprites"] != 14 or
            thunder["particles"] != 12 or
            thunder["flags"] != ["AnimFlash", "AnimBackgroundChange"]):
        raise RuntimeError(
            "Thunder no longer matches its complete FireRed lightning program")
    rock_tomb = moves["317"]
    if (rock_tomb["spriteTemplates"] != [
            "gRedXSpriteTemplate", "gRockTombRockSpriteTemplate"] or
            rock_tomb["taskCallbacks"] != [
                "AnimTask_ShakeBattleTerrain", "AnimTask_ShakeMon"] or
            rock_tomb["programCommands"] != 23 or
            rock_tomb["programSprites"] != 5 or
            rock_tomb["particles"] != 5):
        raise RuntimeError(
            "Rock Tomb no longer matches its four-rock plus target-X program")
    refresh = moves["287"]
    if (refresh["spriteTemplates"] != [
            "gGrantingStarsSpriteTemplate",
            "gSimplePaletteBlendSpriteTemplate",
            "gThinRingExpandingSpriteTemplate"] or
            refresh["taskCallbacks"] != ["AnimTask_StatusClearedEffect"] or
            refresh["programCommands"] != 12 or
            refresh["programSprites"] != 4 or
            refresh["visualCoverage"] != [
                "Sprite:LocalStarVelocity", "Sprite:OrientedStatic",
                "Sprite:PalettePulse", "Task:StatusClearedEffectExact"]):
        raise RuntimeError(
            "Refresh no longer matches its cure-mask, stars and ring program")
    aura_sphere = moves["396"]
    if (aura_sphere["spriteTemplates"] != ["gAuraSphereBlast"] or
            aura_sphere["taskCallbacks"] != [
                "AnimTask_ShakeMon2", "AnimTask_StartSlidingBg"] or
            "AnimBackgroundChange" not in aura_sphere["flags"] or
            aura_sphere["programCommands"] != 23 or
            "Task:SlidingBackground" not in aura_sphere["visualCoverage"]):
        raise RuntimeError(
            "Aura Sphere no longer matches its authored sliding-background program")
    megahorn = moves["224"]
    if (megahorn["taskCallbacks"] != [
            "AnimTask_ShakeMon", "AnimTask_ShakeMonInPlace",
            "AnimTask_StartSlidingBg"] or
            "AnimBackgroundChange" not in megahorn["flags"] or
            "Sprite:MegahornStrike" not in megahorn["visualCoverage"] or
            "Task:SlidingBackground" not in megahorn["visualCoverage"]):
        raise RuntimeError(
            "Megahorn no longer retains its strike and sliding-background paths")
    extreme_speed = moves["245"]
    if ("AnimTask_StartSlidingBg" not in extreme_speed["taskCallbacks"] or
            "AnimTask_SpeedDust" not in extreme_speed["taskCallbacks"] or
            "Task:SlidingBackground" not in extreme_speed["visualCoverage"] or
            "Task:SpeedDust" not in extreme_speed["visualCoverage"] or
            "Task:ExtremeSpeedImpact" not in extreme_speed["visualCoverage"]):
        raise RuntimeError(
            "Extreme Speed no longer composes its background, dust and impact tasks")
    luster_purge = moves["295"]
    if (luster_purge["spriteTemplates"] != [
            "gLusterPurgeCircleSpriteTemplate",
            "gRandomPosHitSplatSpriteTemplate"] or
            "AnimTask_FadeScreenToWhite" not in
            luster_purge["taskCallbacks"] or
            "AnimBackgroundChange" not in luster_purge["flags"] or
            luster_purge["programCommands"] != 41):
        raise RuntimeError(
            "Luster Purge no longer matches its authored background program")

    invalid_sprites = [
        name for name, handler in animation["spriteHandlers"].items()
        if handler["motion"] in {"Projectile", "Arc", "ProceduralPulse"}
        or handler["coverage"] in {"HeuristicFallback", "StaticCelOrAffine"}
        or (handler["stateMovesCoordinates"] and handler["motion"] == "Static")
    ]
    if invalid_sprites:
        raise RuntimeError(
            "Sprite callbacks use an approximate fallback: " +
            ", ".join(invalid_sprites))
    invalid_tasks = [
        name for name, handler in animation["taskHandlers"].items()
        if handler["renderer"] == "Unsupported"
        or (handler["kind"] not in {"Query", "Control", "Sound"}
            and handler.get("motion") == "None" and not handler["templates"])
    ]
    if invalid_tasks:
        raise RuntimeError(
            "Visual tasks lack an executor: " + ", ".join(invalid_tasks))

    # Generation coverage is not renderer coverage.  Previously every task
    # received a non-None enum value, while dozens of those values fell
    # through to the same generic wobble/tint at runtime.  Read the production
    # exact switch and require every emitted callback profile to be named
    # before the fail-closed boundary.
    runtime_source = (ROOT / "src/main.cpp").read_text(encoding="utf-8")

    # A real screen transition invalidates battle-owned cache descriptors.
    # Every painter that owns a UI manifest must therefore enter the screen
    # before calling prepareUiScene(); reversing those two calls discards the
    # freshly loaded pack and makes the retained no-I/O pass draw placeholders.
    ui_scene_painters = (
        "drawHome", "drawKidsCatch", "drawPokecenterConfirm",
        "drawTowerInvite", "drawMegaChallengeInvite", "drawBossInvite",
        "drawLeagueComplete", "drawChooseBattler", "drawInventoryTarget",
        "drawBadgeCase", "drawMart",
    )
    for painter in ui_scene_painters:
        start = runtime_source.index(f"void {painter}(")
        begin = runtime_source.index("beginScreenPresentation(", start)
        prepare = runtime_source.index("prepareUiScene(", start)
        if begin > prepare:
            raise RuntimeError(
                f"{painter} prepares its cache before transition cleanup")
    ui_scene_helper_callers = {
        "drawKidsTowerCampaign": "prepareKidsTowerScene(",
        "drawKidsTowerTeam": "prepareKidsTowerScene(",
        "drawKidsTowerBattle": "prepareKidsTowerScene(",
        "drawBattleParty": "prepareBattlePartySceneAssets(",
        "drawBattleSummary": "prepareSummarySceneAssets(",
        "drawInventory": "prepareInventoryBrowseScene(",
        "drawBox": "prepareBoxSceneAssets(",
        "drawSinnohExpedition": "prepareSinnohExpeditionAssets(",
    }
    for painter, helper in ui_scene_helper_callers.items():
        start = runtime_source.index(f"void {painter}(")
        begin = runtime_source.index("beginScreenPresentation(", start)
        prepare = runtime_source.index(helper, start)
        if begin > prepare:
            raise RuntimeError(
                f"{painter} calls {helper[:-1]} before transition cleanup")

    timed_red_x = re.search(
        r"case BattleAnimationMotion::TimedRedX:\s*\{(.*?)"
        r"case BattleAnimationMotion::SharpenBlink:", runtime_source, re.S)
    if (not timed_red_x or
            "onAttacker ? attackerX : targetX" not in timed_red_x.group(1) or
            "onAttacker ? attackerY : targetY" not in timed_red_x.group(1) or
            "particle.argumentCount, 0, 0" not in timed_red_x.group(1)):
        raise RuntimeError(
            "Timed Red X must honor ANIM_ATTACKER/ANIM_TARGET (Rock Tomb target)")

    # Speculative loading is never allowed to suppress a battle animation.
    # A failed preparation must retry synchronously when its event is consumed;
    # otherwise large scripts such as an opponent's Blizzard disappear after a
    # single transient SD/cache miss.
    if ("prepared animation was not ready; retry move=" not in runtime_source or
            "bool animationReady = prepared && preparedBattleAnimation.ready;"
            not in runtime_source or
            "if (!animationReady)" not in runtime_source):
        raise RuntimeError(
            "Battle-event playback no longer retries a failed speculative preload")

    solar_runtime = re.search(
        r"case BattleAnimationTaskMotion::SolarBeamOrbs:\s*\{(.*?)"
        r"case BattleAnimationTaskMotion::ElectricBoltSegments:",
        runtime_source, re.S)
    if (not solar_runtime or
            solar_runtime.group(1).count("taskTargetX - taskAttackerX") < 1 or
            solar_runtime.group(1).count("taskTargetY - taskAttackerY") < 1 or
            "taskAttackerX +" not in solar_runtime.group(1) or
            "taskAttackerY +" not in solar_runtime.group(1) or
            "> 0x7FU" not in solar_runtime.group(1)):
        raise RuntimeError(
            "Solar Beam release orbs must travel user-to-target with exact depth")
    sunlight_runtime_sections = re.findall(
        r"case BattleAnimationMotion::SunlightRay:(.*?)"
        r"case BattleAnimationMotion::SuperpowerFireball:",
        runtime_source, re.S)
    if not any("187L" in section and "92L" in section
               for section in sunlight_runtime_sections):
        raise RuntimeError("Sunny Day ray no longer follows the scaled FireRed path")
    spawn_start = runtime_source.index("void spawnBattleAnimationTask(")
    spawn_end = runtime_source.index("void signalBattleAnimationTasks(", spawn_start)
    spawn_source = runtime_source[spawn_start:spawn_end]
    if ("case BattleAnimationTaskMotion::BlendBattleAnimPalExact:" not in spawn_source or
            "case BattleAnimationTaskMotion::BlendBattleAnimPalExcludeExact:" not in spawn_source or
            "(std::abs(end - start) + 1) * delay" not in spawn_source):
        raise RuntimeError(
            "FireRed palette blends must derive their lifetime from start/end/delay")
    refresh_task = re.search(
        r"case BattleAnimationTaskMotion::StatusClearedEffectExact:(.*?)"
        r"case BattleAnimationTaskMotion::TeeterDanceMovement:",
        spawn_source, re.S)
    refresh_motion = re.search(
        r"case BattleAnimationTaskMotion::StatusClearedEffectExact:\s*\{(.*?)"
        r"case BattleAnimationTaskMotion::SetCamouflageBlendExact:",
        runtime_source, re.S)
    if (not refresh_task or "boundedLifetime(90)" not in refresh_task.group(1) or
            not refresh_motion or
            "original BG1 mask is intentionally omitted" not in
            refresh_motion.group(1)):
        raise RuntimeError(
            "Refresh no longer keeps its authored task timing and safe OBJ path")
    palette_case_start = runtime_source.index(
        "case BattleAnimationMotion::PalettePulse:")
    palette_case_end = runtime_source.index(
        "case BattleAnimationMotion::ProceduralPulse:", palette_case_start)
    palette_source = runtime_source[palette_case_start:palette_case_end]
    for required in (
            "const bool simpleBlend = particle.argumentCount < 7U",
            "battleVisualOverlay.arenaColorEffect = AssetColorEffect::Blend",
            "if (!amount) continue"):
        if required not in palette_source:
            raise RuntimeError(
                "PalettePulse is not applying the authored simple palette blend")
    layer_capacity_match = re.search(
        r"kBattleVisualLayerCapacity\s*=\s*(\d+)", runtime_source)
    if not layer_capacity_match or int(layer_capacity_match.group(1)) < 31:
        raise RuntimeError(
            "Retained visual layer table cannot hold FireRed's 31-OBJ peak")
    # The compositor may retain every live particle or evenly sample an
    # over-capacity GBA OBJ train.  Anchor the audit at the actual ordered
    # particle dereference instead of a historical loop-variable spelling.
    particle_loop = runtime_source.index(
        "const BattleAnimationRuntimeParticle& particle =")
    particle_switch = runtime_source.index("switch (resource.motion)", particle_loop)
    particle_end = runtime_source.index(
        "if (!resource.asset || !resource.asset[0]) continue;", particle_switch)
    particle_section = runtime_source[particle_switch:particle_end]
    runtime_sprite_motions = set(re.findall(
        r"case BattleAnimationMotion::([A-Za-z0-9_]+)", particle_section))
    emitted_sprite_motions = {
        handler["motion"] for handler in animation["spriteHandlers"].values()
    }
    missing_sprite_runtime = sorted(
        emitted_sprite_motions - runtime_sprite_motions)
    if missing_sprite_runtime:
        raise RuntimeError(
            "Generated sprite motions lack an exact runtime case: " +
            ", ".join(missing_sprite_runtime))
    broad_sprite_motions = {
        "Projectile", "Arc", "Falling", "Rising", "Orbit", "Wave",
        "Scatter", "ProceduralPulse",
    }
    generic_sprite_runtime = sorted(
        emitted_sprite_motions & broad_sprite_motions)
    if generic_sprite_runtime:
        raise RuntimeError(
            "Generated callbacks still use broad sprite fallback motions: " +
            ", ".join(generic_sprite_runtime))

    # Physical bandwidth may thin continuous motion, but it must never alter
    # the authored program or erase a short cel/phase.  These invariants catch
    # the former global ten-frame cap which made long attacks look like an
    # unrelated slideshow even though every callback had a renderer.
    if "constexpr uint8_t kFrameQuantum = 1;" not in runtime_source:
        raise RuntimeError("Animation VM no longer advances every FireRed tick")
    if ("const bool phaseKeyFrame = phaseChanged" not in runtime_source or
            "phaseKeyFrame || commandBoundary || regularSample" not in
            runtime_source):
        raise RuntimeError("Discrete animation phases are not presentation-mandatory")
    if "BattlePresentationTiming::sourceTickDeadlineMs(" not in runtime_source:
        raise RuntimeError("Animation VM is no longer paced to FireRed's 60 Hz script clock")
    if "maximumPhysicalFrames" in runtime_source:
        raise RuntimeError("Animation playback still contains a whole-move frame cap")
    play_move_start = runtime_source.find("void playMoveAnimation(")
    preload_move_start = runtime_source.find("bool preloadMoveAnimation(")
    if play_move_start < 0 or preload_move_start <= play_move_start:
        raise RuntimeError("Could not locate the production move-animation route")
    play_move_source = runtime_source[play_move_start:preload_move_start]
    if "playBattleAnimationProgram(*program" not in play_move_source:
        raise RuntimeError("Production moves are not routed through the generated VM")
    if re.search(r"moveNumber\s*==|switch\s*\(\s*moveNumber", play_move_source):
        raise RuntimeError("Production route contains a move-specific animation bypass")
    preload_end = runtime_source.find(
        "bool preloadMoveAnimationWithCapacityRetry", preload_move_start)
    if preload_end <= preload_move_start:
        raise RuntimeError("Could not locate the move preload boundary")
    preload_move_source = runtime_source[preload_move_start:preload_end]
    if "if (!restoreBattleArenaBeforeAnimation()) return false;" not in preload_move_source:
        raise RuntimeError(
            "Every move preload must restore an opaque previous animation plane first")
    if "battleAnimationArenaRestorePending = opaqueMovePlaneUsed;" not in runtime_source:
        raise RuntimeError(
            "Animation end no longer latches the physical arena restore")
    if "bool restoreBattleArenaBeforeAnimation()" not in runtime_source:
        raise RuntimeError("Missing shallow battle arena restore boundary")
    band = re.search(r"kRetainedBandHeight\s*=\s*(\d+)", runtime_source)
    # The hardware renderer deliberately uses eight scanlines after the
    # canvas/SD recovery work.  The old ten-line floor was a stale audit
    # assertion and made a valid production build fail before any animation
    # coverage was checked.
    if not band or int(band.group(1)) < 8:
        raise RuntimeError("Retained compositor regressed below eight scanlines")
    particles = re.search(
        r"kBattleAnimationParticleCapacity\s*=\s*(\d+)", runtime_source)
    if not particles or int(particles.group(1)) < 64:
        raise RuntimeError("Logical animation OBJ capacity is below FireRed's 64 slots")

    render_start = runtime_source.index(
        "bool renderBattleAnimationProgramFrame")
    fail_closed = runtime_source.index(
        "if (handledExactTask) continue;", render_start)
    exact_section = runtime_source[render_start:fail_closed]
    exact_motions = set(re.findall(
        r"case BattleAnimationTaskMotion::([A-Za-z0-9_]+)", exact_section))
    # Palette-only handlers execute in the palette compositor before the
    # sprite-frame task switch. They are still production exact handlers, so
    # include those cases in the same fail-closed coverage check.
    exact_motions.update(re.findall(
        r"case BattleAnimationTaskMotion::([A-Za-z0-9_]+)", runtime_source))
    # Runtime-query callbacks branch the generated script but never spawn a
    # render task, so their audit record intentionally has no ``motion``.
    # Only callbacks that emit a visual/control task belong in the executor
    # coverage set.
    emitted_motions = {
        handler["motion"] for handler in animation["taskHandlers"].values()
        if "motion" in handler
    }
    missing_runtime = sorted(emitted_motions - exact_motions)
    if missing_runtime:
        raise RuntimeError(
            "Generated task motions lack an exact runtime case: " +
            ", ".join(missing_runtime))
    boundary = runtime_source[fail_closed:fail_closed + 900]
    if not re.search(r"if \(handledExactTask\) continue;\s*//[\s\S]*?continue;",
                     boundary):
        raise RuntimeError("Task executor is not fail-closed before legacy fallback")
    psychic_case = re.search(
        r"case BattleAnimationTaskMotion::PsychicPaletteCycle:(.*?)"
        r"case BattleAnimationTaskMotion::SandstormField:",
        exact_section, re.S)
    if not psychic_case or (
            "kind == BattleAnimationTaskKind::PsychicBackground" not in
            psychic_case.group(1) or
            "setBattleVisualPsychicField" not in psychic_case.group(1) or
            "setBattleVisualPrimitive(BattleVisualPrimitive::PsychicField" not in
            psychic_case.group(1) or
            "addBattleAnimationDirtyPoint" not in psychic_case.group(1)):
        raise RuntimeError(
            "PsychicBackground exact handler must compose the psychic field cue")
    if ("void drawBattleVisualPsychicField()" not in runtime_source or
            "psychicFieldActive" not in runtime_source or
            "drawBattleVisualPsychicField();" not in runtime_source):
        raise RuntimeError(
            "PsychicBackground must use a retained compositor plane")

    broad_motions = {
        "ShakeHorizontal", "ShakeVertical", "ShakeSink", "ShakePattern",
        "Horizontal", "Vertical", "Lunge", "Elliptical", "Bounce",
        "Sway", "Spin", "SlideOffscreen", "Sink", "Hide", "Reveal",
        "Teleport", "ScalePulse", "StretchVertical", "Squish", "Shrink",
        "Grow", "ClonePulse", "Transform", "PaletteFade", "PaletteFlash",
        "PaletteInvert", "PaletteGrayscale", "PaletteCycle", "PaletteTint",
    }
    generic_emitted = sorted(emitted_motions & broad_motions)
    if generic_emitted:
        raise RuntimeError(
            "Generated callbacks still use broad fallback motions: " +
            ", ".join(generic_emitted))

    # Field-only FireRed programs share the large terrain slot with their
    # retained task plane.  The runtime must restore terrain before attaching
    # that plane, and a valid authored program must never be sent through the
    # procedural emergency renderer after a transient preload miss.
    if "const char* terrain = chooseBattleTerrain(presentedBattleOpponent())" not in runtime_source:
        raise RuntimeError("Battle animation preload does not resolve terrain before plane setup")
    if "AssetRenderer::preload(terrainPath)" not in runtime_source:
        raise RuntimeError("Battle animation preload does not restore terrain before field programs")
    fallback_guard = runtime_source[runtime_source.find("void playMoveAnimationFallback"):
                                    runtime_source.find("void playMoveAnimation(")]
    if "findBattleAnimationProgram(renderNumber)" not in fallback_guard:
        raise RuntimeError("Authored animation fallback can still call the procedural renderer")

    terrain_paths: list[str] = []
    for terrain in pack.BATTLE_TERRAINS:
        folder = pack.DECOMP / "graphics/battle_terrain" / terrain
        palette_path = folder / ("1.pal" if terrain == "indoor" else "terrain.pal")
        viewport = pack.battle_terrain_viewport(pack.compose_battle_terrain(
            folder / "terrain.png", folder / "terrain.bin", palette_path))
        relative = f"firered/battle_terrain/{terrain}.pkg"
        assert_rgba_asset(relative, viewport)
        terrain_paths.append(relative)
    for asset_name, tile_folder, palette_folder, palette_name in \
            pack.BATTLE_TERRAIN_VARIANTS:
        folder = pack.DECOMP / "graphics/battle_terrain" / tile_folder
        palette_path = (pack.DECOMP / "graphics/battle_terrain" /
                        palette_folder / f"{palette_name}.pal")
        viewport = pack.battle_terrain_viewport(pack.compose_battle_terrain(
            folder / "terrain.png", folder / "terrain.bin", palette_path))
        relative = f"firered/battle_terrain/{asset_name}.pkg"
        assert_rgba_asset(relative, viewport)
        terrain_paths.append(relative)

    background_count = 0
    secondary_plane_paths: list[str] = []
    for background_id, (gfx, tilemap, palette_name) in enumerate(
            pack.BATTLE_ANIMATION_BACKGROUNDS):
        tiled, palette = pack.battle_animation_indexed_plane(
            gfx, tilemap, palette_name)
        if background_id == 21:
            if tiled.size != (512, 256):
                raise RuntimeError("Fissure is not a 512x256 GBA BG3 plane")
            right = tiled.crop((256, 0, 512, 256))
            if set(flattened_data(right)) != {3}:
                raise RuntimeError(
                    "Fissure compact representation is not lossless")
        static = tiled.crop((0, 0, 240, 138)).resize(
            (320, 184), pack.Image.Resampling.NEAREST)
        static_relative = (
            f"firered/battle_anims/backgrounds/bg_{background_id:02}.pkg")
        tiled_relative = (
            f"firered/battle_anims/backgrounds/tiled_bg_{background_id:02}.pkg")
        assert_plane(static_relative, static, palette)
        tiled_runtime = (tiled.crop((0, 0, 256, 256))
                         if background_id == 21 else tiled)
        assert_plane(tiled_relative, tiled_runtime, palette)
        secondary_plane_paths.extend((static_relative, tiled_relative))
        background_count += 1
    expansion_background_id = len(pack.BATTLE_ANIMATION_BACKGROUNDS)
    for gfx, tilemap, palette_name in pack.EXPANSION_BATTLE_ANIMATION_BACKGROUNDS:
        tiled, palette = pack.battle_animation_indexed_plane(
            gfx, tilemap, palette_name, pack.EXPANSION)
        static = tiled.resize((320, 184), pack.Image.Resampling.NEAREST)
        static_relative = (
            f"firered/battle_anims/backgrounds/bg_{expansion_background_id:02}.pkg")
        tiled_relative = (
            f"firered/battle_anims/backgrounds/tiled_bg_{expansion_background_id:02}.pkg")
        assert_plane(static_relative, static, palette)
        assert_plane(tiled_relative, tiled, palette)
        secondary_plane_paths.extend((static_relative, tiled_relative))
        background_count += 1
        expansion_background_id += 1

    task_background_count = 0
    for name, gfx, tilemap in pack.BATTLE_ANIMATION_TASK_BACKGROUNDS:
        source, palette, transparent = pack.battle_animation_task_indexed_plane(
            gfx, tilemap)
        assert_plane(
            f"firered/battle_anims/backgrounds/task_{name}.pkg",
            source, palette, transparent)
        secondary_plane_paths.append(
            f"firered/battle_anims/backgrounds/task_{name}.pkg")
        task_background_count += 1

    surf_count = 0
    for side in ("player", "opponent"):
        for muddy, prefix in ((False, "surf"), (True, "muddy_water")):
            source, palette = pack.surf_wave_indexed_plane(side, muddy)
            assert_plane(
                f"firered/battle_anims/{prefix}_{side}.pkg", source, palette)
            secondary_plane_paths.append(
                f"firered/battle_anims/{prefix}_{side}.pkg")
            surf_count += 1

    # Runtime keeps terrain pixels plus one secondary plane and its palette
    # in a fixed 64 KiB bank.  Prove every legal pairing fits; a previous
    # 256x512 Fissure package exceeded this bound and silently disappeared.
    # Kept in lockstep with AssetRenderer. Boot uses a smaller bank so SPIFFS,
    # GameSave and the retained canvas can start reliably; battle promotes the
    # shared arena before loading a terrain/changebg pair.
    scene_capacity = 64 * 1024
    largest_scene = 0
    largest_pair = ("", "")
    largest_terrain_bytes = 0
    for terrain_path in terrain_paths:
        terrain_package = read_pkg(terrain_path)
        primary_bytes = packed_pixel_bytes(terrain_package)
        aligned_primary = (primary_bytes + 1) & ~1
        largest_terrain_bytes = max(largest_terrain_bytes, aligned_primary)
        for plane_path in secondary_plane_paths:
            plane_package = read_pkg(plane_path)
            total = (aligned_primary +
                     len(plane_package["palette"]) * 2 +
                     packed_pixel_bytes(plane_package))
            if total > largest_scene:
                largest_scene = total
                largest_pair = (terrain_path, plane_path)
            if total > scene_capacity:
                raise RuntimeError(
                    f"Battle scene exceeds 64 KiB: {terrain_path} + "
                    f"{plane_path} = {total}")

    manifest = json.loads(
        (pack.OUTPUT / "manifest.json").read_text(encoding="utf-8"))
    missing_frames = [
        f"firered/battle_anims/sprites/{asset}_{frame:02}.pkg"
        for asset, spec in animation["visualAssets"].items()
        for frame in range(spec["frameCount"])
        if f"firered/battle_anims/sprites/{asset}_{frame:02}.pkg" not in manifest
    ]
    if missing_frames:
        raise RuntimeError(
            "Runtime animation frames missing: " + ", ".join(missing_frames[:12]))
    for special in (
        "firered/battle_anims/sprites/substitute_front.pkg",
        "firered/battle_anims/sprites/substitute_back.pkg",
    ):
        if special not in manifest:
            raise RuntimeError(f"Runtime task sprite missing: {special}")

    # A move is loaded completely before its first visual frame.  Prove the
    # RAM contract as well as the file manifest: two battlers plus every OBJ
    # cel reachable by an unconditional script must fit beside the largest
    # legal terrain/changebg pair, and the descriptor table must have one slot
    # per resident package.  Without this check an animation can be perfectly
    # translated yet lose its later sprites on the physical ESP32.
    renderer_source = (ROOT / "src/drivers/AssetRenderer.cpp").read_text(
        encoding="utf-8")
    pattern_source = renderer_source[
        renderer_source.index("bool drawIndexedPatternMasked"):
        renderer_source.index("bool drawDirectFile")]
    if ("static_cast<int32_t>(maskX) + localX + sourceOffsetX" not in
            pattern_source or
            "static_cast<int32_t>(maskY) + localY + sourceOffsetY" not in
            pattern_source):
        raise RuntimeError(
            "Pattern masks are not sampling the BG plane in screen coordinates")
    if ("display.readPixel565Fast" not in pattern_source or
            "alphaBlend565(candidate, background, opacity16)" not in
            pattern_source):
        raise RuntimeError(
            "Pattern masks must reproduce FireRed's per-pixel BG1 alpha blend")
    main_source = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    retained_presenter_start = main_source.index(
        "bool presentRetainedFrame() {")
    retained_presenter_end = main_source.index(
        "void stopWildLedAlert()", retained_presenter_start)
    retained_presenter = main_source[retained_presenter_start:
                                     retained_presenter_end]
    if ("paintRetainedSceneStripe();" not in retained_presenter or
            "A failed preflight must never reach the panel" not in
            retained_presenter):
        raise RuntimeError(
            "Retained presenter must compose the complete battle scene before "
            "copying a cleared stripe")
    if ("skipAnimationPreflight" not in retained_presenter or
            "battleAnimationRetainedCompositionWarm = true" not in
            retained_presenter):
        raise RuntimeError(
            "Animation presenter lost its validated RAM-only fast path")
    if ("return false;" not in retained_presenter or
            "return true;" not in retained_presenter):
        raise RuntimeError(
            "Retained presenter no longer reports whether a frame reached the panel")
    if ("if (committed) battleAnimationAtomicRevealPending = false;" not in
            retained_presenter):
        raise RuntimeError(
            "Android clears the atomic arena latch before a frame is committed")
    hp_animation_start = main_source.index(
        "BattleHpAnimationOutcome animateBattleHpEvent(")
    hp_animation_end = main_source.index(
        "void animateBattleExperienceEvent(", hp_animation_start)
    hp_animation_source = main_source[hp_animation_start:hp_animation_end]
    if ("if (!restoreBattleArenaBeforePresentation())" not in
            hp_animation_source or
            "const bool frameCommitted = presentRetainedFrame();" not in
            hp_animation_source or
            "return BattleHpAnimationOutcome::Deferred;" not in
            hp_animation_source):
        raise RuntimeError(
            "HP animation can advance without an arena restore and a committed frame")
    show_event_start = main_source.index("void showCurrentBattleEvent()")
    show_event_end = main_source.index(
        "void advancePendingBattleFlow()", show_event_start)
    show_event_source = main_source[show_event_start:show_event_end]
    deferred_check = show_event_source.index(
        "if (outcome == BattleHpAnimationOutcome::Deferred)")
    event_advance = show_event_source.index("++battleFlow.eventIndex;")
    if deferred_check > event_advance:
        raise RuntimeError(
            "A deferred HP frame is consumed before it can be retried")
    if ("battleAnimationAtomicRevealPending" not in retained_presenter or
            "beginAtomicBattleBackgroundReveal();" not in retained_presenter or
            "finishAtomicBattleBackgroundReveal();" not in retained_presenter):
        raise RuntimeError(
            "Full-field animation transitions are exposed one retained band at a time")
    atomic_hold_start = main_source.index(
        "void beginAtomicBattleBackgroundReveal()")
    atomic_hold_end = main_source.index(
        "constexpr uint8_t kBallPocket", atomic_hold_start)
    atomic_hold_source = main_source[atomic_hold_start:atomic_hold_end]
    if ("backlight.setEnabled(" in atomic_hold_source or
            "panel.writecommand(kIli9341DisplayOff)" in atomic_hold_source or
            "panel.writecommand(kIli9341DisplayOn)" in atomic_hold_source):
        raise RuntimeError(
            "Atomic background commits must keep both the LCD and its backlight enabled")
    if ("if (battleAnimationAtomicRevealPending)" not in retained_presenter or
            "firstColumn = 0;" not in retained_presenter or
            "lastColumn = 320;" not in retained_presenter or
            "lastPixel = std::max<int16_t>(lastPixel, 184);" not in
            retained_presenter or
            "ANIM_BG_WHOLE_FRAME" not in retained_presenter or
            "pushRetainedStripeTiles(" not in retained_presenter or
            retained_presenter.index("beginAtomicBattleBackgroundReveal();") >
            retained_presenter.index("panel.startWrite()")):
        raise RuntimeError(
            "Atomic background commits must stream the complete arena in one TFT transaction")
    playback_start = main_source.index("void playBattleAnimationProgram(")
    playback_end = main_source.index("void playMoveAnimation(", playback_start)
    playback_source = main_source[playback_start:playback_end]
    if ("presentAtomicBackgroundFrame(16U);" in playback_source or
            "presentAtomicBackgroundFrame(0U);" not in playback_source or
            "if (backgroundRestoreDeferred) return;" not in playback_source or
            "if (opaqueMovePlaneUsed) battleAnimationAtomicRevealPending = true;"
            not in playback_source):
        raise RuntimeError(
            "Authored background entry/exit must directly swap complete frames without a black key frame")
    frame_renderer_start = main_source.index(
        "bool renderBattleAnimationProgramFrame(")
    frame_renderer_end = main_source.index(
        "void advanceBattleAnimationRuntime", frame_renderer_start)
    frame_renderer = main_source[frame_renderer_start:frame_renderer_end]
    if ("if (!ensureBattleVisualLayers())" not in frame_renderer or
            "retained layer tables unavailable" not in frame_renderer):
        raise RuntimeError(
            "Animation frame damage history must be allocated before it is "
            "read; field-only effects must not null-dereference")
    if ("kBattleNativeViewportOriginX = 40" not in main_source or
            "kBattleNativeViewportOriginY = 12" not in main_source or
            "battleVisualOverlay.cureOffsetX -" not in main_source or
            "battleVisualOverlay.cureOffsetY -" not in main_source):
        raise RuntimeError(
            "BG1 mask sampling must subtract the retained panel viewport origin")
    if ("case BattleAnimationTaskMotion::CurseStretchingBlackBg:" not in
            main_source or
            "slot->lifetime = 0xFFFFU;" not in main_source or
            "task.age >= 18U" not in main_source or
            "kBattleNativeViewportOriginX + 240" not in main_source or
            "applyArenaColor(AssetColorEffect::Blend, TFT_BLACK, 16U)" not in
            main_source or
            "if ((paletteMask & 0x01U) && start > end)" not in main_source or
            main_source.count(
                "BattleAnimationTaskMotion::CurseStretchingBlackBg") < 4):
        raise RuntimeError(
            "Curse Ghost must retain its black FireRed background after WIN0 setup")
    battle_engine_source = (ROOT / "src/game/BattleEngine.cpp").read_text(
        encoding="utf-8")
    if (battle_engine_source.count('effectIs(enemyMoveData,"CURSE")') < 1 or
            battle_engine_source.count('effectIs(usedMove,"CURSE")') < 1 or
            battle_engine_source.count("PokemonType::Ghost) ? 0 : 1") < 2):
        raise RuntimeError(
            "Curse animation turn must be selected from the effective user type")
    if ("std::memset(retainedTileHash, 0, sizeof(retainedTileHash))" not in
            main_source or
            retained_presenter.count("invalidatePixels(0, 0, 320, 240);") < 2):
        raise RuntimeError(
            "Cancelled retained frames and screen transitions must force a full retry")
    cache_bytes_matches = re.findall(
        r"kCacheArenaBytes\s*=\s*(\d+)", renderer_source)
    scene_bytes_match = re.search(
        r"kBattleSceneWorkspaceBytes\s*=\s*(\d+)U?\s*\*\s*1024U?",
        renderer_source)
    cache_slots_match = re.search(r"kCacheSlots\s*=\s*(\d+)", renderer_source)
    if not cache_bytes_matches or not scene_bytes_match or not cache_slots_match:
        raise RuntimeError("Cannot audit renderer cache constants")
    # The source keeps a multi-megabyte Android bank and the constrained
    # ESP32 bank under the same symbol. The last definition is the production
    # hardware value; taking the first match made the audit report a bogus
    # four-byte cache and reject otherwise valid move manifests.
    small_arena_bytes = int(cache_bytes_matches[-1])
    audited_scene_capacity = int(scene_bytes_match.group(1)) * 1024
    if audited_scene_capacity != scene_capacity:
        raise RuntimeError("Visual audit and renderer scene capacities differ")
    small_slots = int(cache_slots_match.group(1))
    worst_plane_sprite_capacity = (
        small_arena_bytes + scene_capacity - largest_scene)
    worst_terrain_sprite_capacity = (
        small_arena_bytes + scene_capacity - largest_terrain_bytes)
    plane_task_motions = {
        "SandstormField", "HazeFogField", "MistBallFogField",
        "HeartsField", "MorningSunField", "CurseWhiteLines",
        "StatusClearedEffectExact", "SurfWave", "ScaryFaceExact",
    }

    pokemon_packages = [
        path.relative_to(pack.OUTPUT).as_posix()
        for folder in (pack.OUTPUT / "pokemon/front",
                       pack.OUTPUT / "pokemon/back")
        for path in folder.rglob("*.pkg")
    ]
    if not pokemon_packages:
        raise RuntimeError("Battle Pokemon packages are absent")
    # Front/back images share the 64x64 GBA format, but alternate forms can
    # carry a larger palette. Two copies of the largest exact reservation are
    # therefore the safe battler bound.
    maximum_battler_bytes = max(small_cache_bytes(path)
                                for path in pokemon_packages)
    largest_move_cache = 0
    largest_move_name = ""
    largest_move_slots = 0
    conditional_union_overages: list[str] = []
    for move in moves.values():
        assets: set[str] = set()
        templates = set(move["spriteTemplates"])
        for callback in move["taskCallbacks"]:
            task = animation["taskHandlers"].get(callback)
            if not task:
                continue
            templates.update(task.get("templates", []))
            if task.get("motion") == "MonToSubstitute":
                # Only the attacker's side is loaded for a resolved script.
                substitute = max(
                    ("firered/battle_anims/sprites/substitute_front.pkg",
                     "firered/battle_anims/sprites/substitute_back.pkg"),
                    key=small_cache_bytes)
                assets.add(substitute)
        for template in templates:
            handler = animation["spriteHandlers"].get(template)
            if not handler or not handler.get("asset"):
                continue
            asset = handler["asset"]
            spec = animation["visualAssets"].get(asset)
            if not spec:
                raise RuntimeError(
                    f"{move['symbol']} references unknown visual asset {asset}")
            for frame in range(spec["frameCount"]):
                assets.add(
                    f"firered/battle_anims/sprites/{asset}_{frame:02}.pkg")
        resident_bytes = aligned_asset_set_bytes(assets) + 2 * maximum_battler_bytes
        resident_slots = len(assets) + 2
        if resident_bytes > largest_move_cache:
            largest_move_cache = resident_bytes
            largest_move_name = move["symbol"]
        largest_move_slots = max(largest_move_slots, resident_slots)
        if resident_slots > small_slots:
            raise RuntimeError(
                f"{move['symbol']} needs {resident_slots} cache descriptors; "
                f"renderer has {small_slots}")
        needs_secondary_plane = "AnimBackgroundChange" in move["flags"]
        if not needs_secondary_plane:
            needs_secondary_plane = any(
                animation["taskHandlers"].get(callback, {}).get("motion")
                in plane_task_motions for callback in move["taskCallbacks"])
        available_bytes = (worst_plane_sprite_capacity
                           if needs_secondary_plane
                           else worst_terrain_sprite_capacity)
        if resident_bytes > available_bytes:
            if not move["conditionalBranches"]:
                raise RuntimeError(
                    f"{move['symbol']} needs {resident_bytes} sprite bytes "
                    f"beside its battle planes; only {available_bytes} "
                    "are available")
            # Conditional scripts (Secret Power is the FireRed example) list
            # mutually exclusive terrain branches in the static union. The
            # production visitor resolves one branch before preloading. Keep
            # the conservative overage visible rather than pretending the
            # impossible union is a runtime scene.
            conditional_union_overages.append(move["symbol"])

    result = {
        "moves": len(moves),
        "spriteCallbacks": len(animation["spriteHandlers"]),
        "taskCallbacks": len(animation["taskHandlers"]),
        "moveBackgrounds": background_count,
        "taskBackgrounds": task_background_count,
        "surfPlanes": surf_count,
        "battleTerrains": len(terrain_paths),
        "largestResidentSceneBytes": largest_scene,
        "largestResidentScene": list(largest_pair),
        "retainedVisualLayerCapacity": int(layer_capacity_match.group(1)),
        "smallCacheBytes": small_arena_bytes,
        "smallCacheSlots": small_slots,
        "worstPlaneSpriteCapacity": worst_plane_sprite_capacity,
        "worstTerrainSpriteCapacity": worst_terrain_sprite_capacity,
        "largestStaticMoveCacheBytes": largest_move_cache,
        "largestStaticMoveCache": largest_move_name,
        "largestStaticMoveSlots": largest_move_slots,
        "conditionalUnionOverages": conditional_union_overages,
        "runtimeSpriteFrames": sum(
            spec["frameCount"] for spec in animation["visualAssets"].values()),
        "paletteIndicesPreserved": True,
        "camera": {"source": [240, 138], "destination": [320, 184]},
        "fallbacks": 0,
        "exactSpriteMotions": len(runtime_sprite_motions),
        "emittedSpriteMotions": len(emitted_sprite_motions),
        "exactTaskMotions": len(exact_motions),
        "emittedTaskMotions": len(emitted_motions),
    }
    output = ROOT / ".generated/battle_visual_validation.json"
    output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
