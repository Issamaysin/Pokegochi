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
    source_indices = list(pack.palette_index_plane(source).get_flattened_data())
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
    for red, green, blue, alpha in source.get_flattened_data():
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
    if len(moves) != 354 or animation["program"]["moves"] != 354:
        raise RuntimeError("The generated VM does not contain all 354 moves")
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
            solar_beam["programSprites"] != 28):
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
    if "phaseChanged || commandBoundary || regularSample" not in runtime_source:
        raise RuntimeError("Discrete animation phases are not presentation-mandatory")
    if "kBattleAnimationSourceFps = 60U" not in runtime_source:
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
    band = re.search(r"kRetainedBandHeight\s*=\s*(\d+)", runtime_source)
    if not band or int(band.group(1)) < 12:
        raise RuntimeError("Retained compositor regressed below twelve scanlines")
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
            if set(right.get_flattened_data()) != {3}:
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
    # Kept in lockstep with AssetRenderer.  The generated-asset loop below is
    # the proof that the 61 KiB bank still holds every legal terrain/secondary
    # plane pair; its reclaimed 3 KiB and the smaller retained hash table fund
    # the wider TFT band.
    scene_capacity = 61 * 1024
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
    cache_bytes_match = re.search(
        r"kCacheArenaBytes\s*=\s*(\d+)", renderer_source)
    scene_bytes_match = re.search(
        r"kSceneWorkspaceBytes\s*=\s*(\d+)U?\s*\*\s*1024U?",
        renderer_source)
    cache_slots_match = re.search(r"kCacheSlots\s*=\s*(\d+)", renderer_source)
    if not cache_bytes_match or not scene_bytes_match or not cache_slots_match:
        raise RuntimeError("Cannot audit renderer cache constants")
    small_arena_bytes = int(cache_bytes_match.group(1))
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
