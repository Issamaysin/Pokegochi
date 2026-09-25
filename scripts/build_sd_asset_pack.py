"""Build a local-only paletted FireRed asset pack for the Pokegochi microSD card."""

from __future__ import annotations

import hashlib
import json
import re
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from render_firered_mockup import (
    BACK_SPRITE_TABLE, EXPECTED_SHA1, extract_battle_sprite, extract_icon,
)
from render_firered_asset_mockups import render_overworld_map
from build_home_backgrounds import (
    BACKGROUNDS, build_background_layers, render_map_layers,
    write_firmware_metadata,
)

ROOT = Path(__file__).resolve().parent.parent
ROM = ROOT / "rom/extracted/Pokemon - FireRed Version (USA, Europe).gba"
DECOMP = ROOT / ".downloads/pokefirered-tree/pokefirered-master"
EMERALD = ROOT / ".downloads/pokeemerald-tree/pokeemerald-master"
CRYSTAL = ROOT / ".downloads/pokecrystal-tree/pokecrystal-master"
EXPANSION = ROOT / ".downloads/pokeemerald-expansion"
OUTPUT = ROOT / ".generated/sdcard/pokegochi/assets"
ARCHIVE = ROOT / ".generated/sdcard/pokegochi/pokegochi.pak"
_ASSET_VERSION_HEADER = (ROOT / "include/config/AssetPackVersion.h").read_text(
    encoding="ascii")
_ASSET_VERSION_MATCH = re.search(
    r"kRequiredAssetPackVersion\s*=\s*(\d+)", _ASSET_VERSION_HEADER)
if not _ASSET_VERSION_MATCH:
    raise RuntimeError("AssetPackVersion.h does not define kRequiredAssetPackVersion")
ASSET_PACK_VERSION = int(_ASSET_VERSION_MATCH.group(1))
TRANSPARENT = 0xF81F
# These are the original FireRed battle terrain tilemaps.  Keep the battle
# renderer's palette faithful to the source game instead of synthesising a
# coloured background in firmware.
BATTLE_TERRAINS = (
    "grass", "longgrass", "water", "pond", "sand", "cave", "mountain",
    "building", "indoor", "underwater",
)

# FireRed reuses two terrain tile sets with alternate official palettes for
# its special indoor battles.  Export those combinations separately so the
# firmware can give Gyms, the League and link battles their native identity
# without recolouring anything at runtime.
BATTLE_TERRAIN_VARIANTS = (
    # asset name, tile/map folder, palette folder, palette name
    ("gym", "building", "indoor", "gym"),
    ("leader", "building", "indoor", "leader"),
    ("indoor_2", "indoor", "indoor", "2"),
    ("lorelei", "indoor", "indoor", "lorelei"),
    ("bruno", "indoor", "indoor", "bruno"),
    ("agatha", "indoor", "indoor", "agatha"),
    ("lance", "indoor", "indoor", "lance"),
    ("champion", "indoor", "indoor", "champion"),
    ("link", "building", "indoor", "link"),
)

# FireRed's exact move-background table, including attacker-side variants.
# Contest variants are exported as well because keeping ids aligned with the
# source constants makes the generated animation bytecode self-validating.
BATTLE_ANIMATION_BACKGROUNDS = (
    ("dark", "dark", None), ("dark", "dark", None),
    ("ghost", "ghost", None), ("psychic", "psychic", None),
    ("impact", "impact_opponent", None), ("impact", "impact_player", None),
    ("impact", "impact_contests", None), ("drill", "drill", None),
    ("drill", "drill_contests", None),
    ("highspeed", "highspeed_opponent", None),
    ("highspeed", "highspeed_player", None), ("thunder", "thunder", None),
    ("guillotine", "guillotine_opponent", None),
    ("guillotine", "guillotine_player", None),
    ("guillotine", "guillotine_contests", None), ("ice", "ice", None),
    ("cosmic", "cosmic", None), ("in_air", "in_air", None),
    ("drill", "drill", "sky"), ("drill", "drill_contests", "sky"),
    ("aurora", "aurora", None), ("fissure", "fissure", None),
    ("highspeed", "highspeed_opponent", "bug"),
    ("highspeed", "highspeed_player", "bug"),
    ("impact", "impact_opponent", "solarbeam"),
    ("impact", "impact_player", "solarbeam"),
    ("impact", "impact_contests", "solarbeam"),
)

# Whole BG1 planes created by visual tasks rather than changebg. These are
# real 32x32 FireRed tilemaps, not procedural substitutes. Keep index zero
# opaque: unlike OBJ sprites these planes alpha-blend every palette entry.
BATTLE_ANIMATION_TASK_BACKGROUNDS = (
    ("sandstorm", "battle_anims/backgrounds/sandstorm_brew.png",
     "battle_anims/backgrounds/sandstorm_brew.bin"),
    ("fog", "weather/fog_horizontal.png",
     "battle_anims/backgrounds/fog.bin"),
    ("hearts", "battle_anims/backgrounds/attract.png",
     "battle_anims/backgrounds/attract.bin"),
    ("sunlight", "battle_anims/masks/light_beam.png",
     "battle_anims/masks/light_beam.bin"),
    # Curse uses this BG1 through the attacker's OBJ-window mask. Palette
    # index zero is transparent for our retained compositor; the white line
    # pixels are clipped again by the battler sprite at runtime.
    ("curse", "battle_anims/masks/curse.png",
     "battle_anims/masks/curse.bin"),
    ("scary_face_player", "battle_anims/backgrounds/scary_face.png",
     "battle_anims/backgrounds/scary_face_player.bin"),
    ("scary_face_opponent", "battle_anims/backgrounds/scary_face.png",
     "battle_anims/backgrounds/scary_face_opponent.bin"),
    ("cure_bubbles", "battle_anims/masks/cure_bubbles.png",
     "battle_anims/masks/cure_bubbles.bin"),
)

# Only official Mega Evolutions whose base species is in #001-386, plus the
# two official Primal Reversions. Extra expansion folders (for example fan
# forms) are intentionally absent from this allow-list.
MEGA_FORMS = (
    (26,"raichu","mega_x"),(26,"raichu","mega_y"),(36,"clefable","mega"),
    (71,"victreebel","mega"),(121,"starmie","mega"),(149,"dragonite","mega"),
    (154,"meganium","mega"),(160,"feraligatr","mega"),(227,"skarmory","mega"),
    (358,"chimecho","mega"),(359,"absol","mega_z"),
    (3,"venusaur","mega"),(6,"charizard","mega_x"),(6,"charizard","mega_y"),
    (9,"blastoise","mega"),(15,"beedrill","mega"),(18,"pidgeot","mega"),
    (65,"alakazam","mega"),(80,"slowbro","mega"),(94,"gengar","mega"),
    (115,"kangaskhan","mega"),(127,"pinsir","mega"),(130,"gyarados","mega"),
    (142,"aerodactyl","mega"),(150,"mewtwo","mega_x"),(150,"mewtwo","mega_y"),
    (181,"ampharos","mega"),(208,"steelix","mega"),(212,"scizor","mega"),
    (214,"heracross","mega"),(229,"houndoom","mega"),(248,"tyranitar","mega"),
    (254,"sceptile","mega"),(257,"blaziken","mega"),(260,"swampert","mega"),
    (282,"gardevoir","mega"),(302,"sableye","mega"),(303,"mawile","mega"),
    (306,"aggron","mega"),(308,"medicham","mega"),(310,"manectric","mega"),
    (319,"sharpedo","mega"),(323,"camerupt","mega"),(334,"altaria","mega"),
    (354,"banette","mega"),(359,"absol","mega"),(362,"glalie","mega"),
    (373,"salamence","mega"),(376,"metagross","mega"),(380,"latias","mega"),
    (381,"latios","mega"),(382,"kyogre","primal"),(383,"groudon","primal"),
    (384,"rayquaza","mega"),
)

# Ten authored healing-room views. Most Pokemon Centers in one region share
# a layout, so the two classic rooms use different real city populations,
# while the remaining entries select every visually distinct Nurse location
# available across FireRed and Emerald. Nothing in these scenes is invented:
# map tiles, counters, Nurse Joy and visible NPCs all come from the decomps.
POKECENTER_SCENES = (
    ("firered", "ViridianCity_PokemonCenter_1F"),
    ("firered", "OneIsland_PokemonCenter_1F"),
    ("firered", "IndigoPlateau_PokemonCenter_1F"),
    ("firered", "TrainerTower_Lobby"),
    ("firered", "SevenIsland_PokemonCenter_1F"),
    ("emerald", "RustboroCity_PokemonCenter_1F"),
    ("emerald", "LavaridgeTown_PokemonCenter_1F"),
    ("emerald", "EverGrandeCity_PokemonLeague_1F"),
    ("emerald", "TrainerHill_Entrance"),
    ("emerald", "UnionRoom"),
)


def indexed(indices: list[int], palette: list[tuple[int, int, int]], size: int) -> Image.Image:
    image = Image.new("RGBA", (size, size))
    image.putdata([(*palette[value], 0 if value == 0 else 255) for value in indices])
    return image

def jasc_palettes(path: Path) -> list[list[tuple[int, int, int]]]:
    """Read one or more 16-colour OBJ palettes from a JASC file."""
    lines = path.read_text(encoding="ascii").splitlines()
    if len(lines) < 3 or lines[:2] != ["JASC-PAL", "0100"]:
        raise ValueError(f"Invalid palette {path}")
    colour_count = int(lines[2])
    if colour_count < 16 or colour_count % 16:
        raise ValueError(f"Invalid OBJ palette colour count in {path}: {colour_count}")
    colours = [tuple(map(int, line.split()))
               for line in lines[3:3 + colour_count]]
    return [colours[offset:offset + 16]
            for offset in range(0, colour_count, 16)]


def jasc_palette(path: Path) -> list[tuple[int, int, int]]:
    return jasc_palettes(path)[0]

def shiny_icon(image: Image.Image, normal: list[tuple[int,int,int]], shiny: list[tuple[int,int,int]]) -> Image.Image:
    image=image.convert("RGBA");out=Image.new("RGBA",image.size)
    converted=[]
    for red,green,blue,alpha in image.get_flattened_data():
        if alpha<128:converted.append((red,green,blue,0));continue
        nearest=min(range(1,16),key=lambda i:(red-normal[i][0])**2+(green-normal[i][1])**2+(blue-normal[i][2])**2)
        converted.append((*shiny[nearest],alpha))
    out.putdata(converted);return out


def source_png(path: Path) -> Image.Image:
    source = Image.open(path)
    if source.mode == "P":
        indices = list(source.get_flattened_data())
        rgba = source.convert("RGBA")
        alpha = Image.new("L", source.size)
        alpha.putdata([0 if value == 0 else 255 for value in indices])
        rgba.putalpha(alpha)
        return rgba
    return source.convert("RGBA")


def indexed_source_png(path: Path, palette: list[tuple[int, int, int]]) -> Image.Image:
    """Render a paletted source with the palette selected by game data.

    Pokemon decomps treat sprite pixels and palettes as separate resources.
    In particular, every Mega back PNG currently embeds the shiny palette even
    though its indices are shared by both variants. Opening that PNG as RGBA
    therefore makes a normal player's back sprite look shiny. Reapply the
    requested JASC palette by index exactly as the GBA engine does.
    """
    source = Image.open(path)
    if source.mode != "P":
        raise RuntimeError(f"Expected indexed Pokemon source: {path}")
    indices = list(source.get_flattened_data())
    invalid = next((value for value in indices if value >= len(palette)), None)
    if invalid is not None:
        raise RuntimeError(
            f"Palette index {invalid} exceeds {len(palette)} colours in {path}")
    output = Image.new("RGBA", source.size)
    output.putdata([
        (*palette[value], 0 if value == 0 else 255)
        for value in indices
    ])
    return output


def animation_palettes(name: str) -> list[list[tuple[int, int, int]]]:
    folder = DECOMP / "graphics/battle_anims/sprites"
    palette_path = folder / f"{name}.pal"
    if palette_path.exists():
        return jasc_palettes(palette_path)
    png_path = folder / f"{name}.png"
    if png_path.exists():
        source = Image.open(png_path)
        raw = source.getpalette()
        if source.mode == "P" and raw and len(raw) >= 48:
            return [[tuple(raw[index * 3:index * 3 + 3])
                     for index in range(16)]]
    raise RuntimeError(f"Missing FireRed animation palette source: {name}")


def animation_source_png(path: Path, palette: list[tuple[int, int, int]]) -> Image.Image:
    """Render one FireRed animation sheet with the ANIM_TAG's real palette.

    Several tags intentionally share tile graphics while selecting a different
    palette (STUN_SPORE/SLEEP_POWDER and the coloured orb families are common
    examples). Opening the source PNG directly loses that distinction.
    """
    source = Image.open(path)
    if source.mode != "P":
        return source_png(path)
    output = Image.new("RGBA", source.size)
    output.putdata([(*palette[value], 0 if value == 0 else 255)
                    for value in source.get_flattened_data()])
    return output


def palette_index_plane(image: Image.Image) -> Image.Image:
    """Return numeric palette entries, never their greyscale luminance.

    Pillow's ``P -> L`` conversion applies the embedded RGB palette. That is
    destructive for GBA graphics, where animation callbacks address entries
    0..15 by number. Exact battle assets all pass through this helper so an
    accidental conversion cannot turn palette id 7 into luminance 145.
    """
    if image.mode == "L":
        return image.copy()
    if image.mode != "P":
        raise RuntimeError(f"Expected indexed P/L image, got {image.mode}")
    output = Image.new("L", image.size, 0)
    output.putdata(list(image.get_flattened_data()))
    return output


def export_exact_battle_animation_assets(animation_audit: dict, manifest: dict) -> dict:
    """Export audited OAM cels for every visual tag used by a move."""
    exported_frames = 0
    opaque_frames: list[str] = []
    intended_opaque = {"conversion_00.pkg"}
    intended_empty: set[str] = set()
    oversized_frames: list[str] = []
    empty_frames: list[str] = []
    for key, spec in sorted(animation_audit["visualAssets"].items()):
        palettes = animation_palettes(spec["palette"])
        sources: dict[str, Image.Image] = {}
        for source_name in spec["sources"]:
            source_path = DECOMP / "graphics/battle_anims/sprites" / f"{source_name}.png"
            source = Image.open(source_path)
            if source.mode != "P":
                raise RuntimeError(
                    f"Battle animation sprite lost GBA indices: {source_path}")
            sources[source_name] = palette_index_plane(source)
        for frame_index, frame in enumerate(spec["frames"]):
            # MUSIC_NOTES_2 is the one FireRed OBJ palette which contains
            # several 16-colour banks. The original script selects these
            # banks dynamically; cycling them keeps that colour animation in
            # the compact ESP32 frame stream.
            palette_index = frame_index % len(palettes)
            palette = palettes[palette_index]
            if "tiles" in frame:
                indexed_frame = Image.new(
                    "L", (frame["width"], frame["height"]), 0)
                for tile in frame["tiles"]:
                    source = sources[tile["source"]]
                    tile_image = source.crop((tile["x"], tile["y"],
                                              tile["x"] + 8, tile["y"] + 8))
                    indexed_frame.paste(
                        tile_image, (tile["destX"], tile["destY"]))
                if frame.get("hFlip"):
                    indexed_frame = indexed_frame.transpose(
                        Image.Transpose.FLIP_LEFT_RIGHT)
                if frame.get("vFlip"):
                    indexed_frame = indexed_frame.transpose(
                        Image.Transpose.FLIP_TOP_BOTTOM)
            else:
                source = sources[frame["source"]]
                left, top = frame["x"], frame["y"]
                indexed_frame = source.crop(
                    (left, top, left + frame["width"], top + frame["height"]))
            image = indexed_rgba(indexed_frame, palette, 0)
            alpha = image.getchannel("A")
            frame_name = f"{key}_{frame_index:02}.pkg"
            if alpha.getbbox() is None:
                empty_frames.append(frame_name)
                # Ice Ball's last exact cel is deliberately transparent: the
                # original ANIMCMD sequence uses it to remove the shattered
                # chunk. Treat it as authored timing, not a broken crop.
                if (spec.get("template") == "gIceBallChunkSpriteTemplate" and
                        frame_index == spec["frameCount"] - 1):
                    intended_empty.add(frame_name)
            if alpha.getextrema()[0] == 255:
                opaque_frames.append(frame_name)
                # Conversion's 8x8 mosaic tiles intentionally cover their
                # complete OAM cell. Template-specific exports retain that
                # property just like the legacy tag-level frame.
                if spec.get("tag") == "CONVERSION":
                    intended_opaque.add(frame_name)
            if image.width > 64 or image.height > 64:
                oversized_frames.append(frame_name)
            write_indexed_asset(
                f"firered/battle_anims/sprites/{frame_name}",
                indexed_frame, palette, manifest, transparent_index=0)
            exported_frames += 1
    unexpected_opaque = [name for name in opaque_frames
                         if name not in intended_opaque]
    unexpected_empty = [name for name in empty_frames
                        if name not in intended_empty]
    if unexpected_empty or unexpected_opaque or oversized_frames:
        raise RuntimeError(
            "Battle sprite audit failed: "
            f"{len(unexpected_empty)} empty, {len(unexpected_opaque)} opaque, "
            f"{len(oversized_frames)} oversized; "
            f"empty={unexpected_empty[:8]}, opaque={unexpected_opaque[:8]}, "
            f"oversized={oversized_frames[:8]}")
    result = {
        "visualTags": len(animation_audit["visualAssets"]),
        "frames": exported_frames,
        "emptyFrames": unexpected_empty,
        "intendedEmptyFrames": sorted(intended_empty),
        "opaqueFrames": unexpected_opaque,
        "intendedOpaqueFrames": sorted(set(opaque_frames) & intended_opaque),
        "oversizedFrames": oversized_frames,
    }
    (ROOT / ".generated/battle_sprite_audit.json").write_text(
        json.dumps(result, indent=2), encoding="utf-8")
    return result


def compose_tilemap_image(tiles: Image.Image, map_path: Path, columns: int = 64,
                          tile_id_offset: int = 0) -> Image.Image:
    tiles = tiles.convert("RGBA")
    values = struct.unpack(f"<{map_path.stat().st_size // 2}H", map_path.read_bytes())
    rows = len(values) // columns
    output = Image.new("RGBA", (columns * 8, rows * 8), (0, 0, 0, 255))
    tiles_per_row = tiles.width // 8
    for index, entry in enumerate(values):
        tile_id = (entry & 0x3FF) - tile_id_offset; tx = tile_id % tiles_per_row * 8; ty = tile_id // tiles_per_row * 8
        if tile_id < 0: continue
        if ty + 8 > tiles.height: continue
        tile = tiles.crop((tx, ty, tx + 8, ty + 8))
        if entry & 0x400: tile = tile.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        if entry & 0x800: tile = tile.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        output.alpha_composite(tile, ((index % columns) * 8, (index // columns) * 8))
    return output


def compose_tilemap_indices(tiles: Image.Image, map_path: Path,
                            columns: int = 64,
                            tile_id_offset: int = 0) -> Image.Image:
    """Compose a GBA tilemap without converting or renumbering its indices.

    Runtime palette tasks address entries by their original numeric index.
    Going through RGBA and rebuilding a palette from first pixel occurrence
    preserves the still image but breaks those effects.  This L-mode plane is
    therefore the authoritative representation for battle animation assets.
    """
    if tiles.mode != "P":
        raise RuntimeError("Indexed tilemap composition requires a P-mode source")
    values = struct.unpack(f"<{map_path.stat().st_size // 2}H",
                           map_path.read_bytes())
    rows = len(values) // columns
    output = Image.new("L", (columns * 8, rows * 8), 0)
    source = palette_index_plane(tiles)
    tiles_per_row = tiles.width // 8
    for index, entry in enumerate(values):
        tile_id = (entry & 0x3FF) - tile_id_offset
        if tile_id < 0:
            continue
        tx = tile_id % tiles_per_row * 8
        ty = tile_id // tiles_per_row * 8
        if ty + 8 > tiles.height:
            continue
        tile = source.crop((tx, ty, tx + 8, ty + 8))
        if entry & 0x400:
            tile = tile.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        if entry & 0x800:
            tile = tile.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        output.paste(tile, ((index % columns) * 8, (index // columns) * 8))
    return output


def compose_wide_text_bg_indices(tiles: Image.Image,
                                 map_path: Path) -> Image.Image:
    """Decode a GBA text BG with size=1 (512x256) exactly.

    A 64x32 text background is stored as two consecutive 32x32
    screenblocks, not as one row-major array with 64 columns.  Fissure is the
    only ``changebg`` plane in FireRed that uses this layout.  Treating its
    2048 entries as 32 columns produced a 256x512 image: the right half was
    rotated underneath the left half, the scroll origin was wrong, and the
    resulting PKG could not coexist with the battle terrain in the ESP32's
    64 KiB scene bank.
    """
    if tiles.mode != "P":
        raise RuntimeError("Indexed tilemap composition requires a P-mode source")
    values = struct.unpack(f"<{map_path.stat().st_size // 2}H",
                           map_path.read_bytes())
    if len(values) != 2048:
        raise RuntimeError(
            f"Wide GBA text BG must contain two screenblocks: {map_path}")
    output = Image.new("L", (512, 256), 0)
    source = palette_index_plane(tiles)
    tiles_per_row = tiles.width // 8
    for row in range(32):
        for column in range(64):
            entry = values[(column // 32) * 1024 + row * 32 +
                           (column % 32)]
            tile_id = entry & 0x3FF
            tx = tile_id % tiles_per_row * 8
            ty = tile_id // tiles_per_row * 8
            if ty + 8 > tiles.height:
                continue
            tile = source.crop((tx, ty, tx + 8, ty + 8))
            if entry & 0x400:
                tile = tile.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            if entry & 0x800:
                tile = tile.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
            output.paste(tile, (column * 8, row * 8))
    return output


def embedded_palette(image: Image.Image, count: int = 16) -> list[tuple[int, int, int]]:
    if image.mode != "P" or image.getpalette() is None:
        raise RuntimeError("Expected an indexed PNG with an embedded palette")
    raw = image.getpalette()
    return [tuple(raw[index * 3:index * 3 + 3]) for index in range(count)]


def indexed_rgba(indices: Image.Image,
                 palette: list[tuple[int, int, int]],
                 transparent_index: int | None = None) -> Image.Image:
    output = Image.new("RGBA", indices.size)
    output.putdata([
        (*palette[value], 0 if value == transparent_index else 255)
        for value in indices.get_flattened_data()
    ])
    return output


def battle_animation_indexed_plane(
        gfx_name: str, map_name: str,
        palette_name: str | None) -> tuple[Image.Image, list[tuple[int, int, int]]]:
    folder = DECOMP / "graphics/battle_anims/backgrounds"
    source = Image.open(folder / f"{gfx_name}.png")
    if source.mode != "P":
        raise RuntimeError(f"Move background is not indexed: {gfx_name}")
    palette = (jasc_palette(folder / f"{palette_name}.pal")
               if palette_name else embedded_palette(source))
    map_path = folder / f"{map_name}.bin"
    # BG3 uses screenSize=1 in battle: 4 KiB maps are two horizontal
    # screenblocks.  Smaller maps occupy the first block and remain ordinary
    # row-major 32-column planes.
    indices = (compose_wide_text_bg_indices(source, map_path)
               if map_path.stat().st_size == 4096
               else compose_tilemap_indices(source, map_path, 32))
    return indices, palette


def compose_tilemap(tiles_path: Path, map_path: Path, columns: int = 64,
                    tile_id_offset: int = 0) -> Image.Image:
    return compose_tilemap_image(Image.open(tiles_path), map_path, columns,
                                 tile_id_offset)


def compose_battle_animation_background(gfx_name: str, map_name: str,
                                        palette_name: str | None) -> Image.Image:
    # FireRed renders a 240x160 camera, then covers its bottom 48 rows with the
    # dialogue/action windows.  Several source maps therefore deliberately
    # leave their last tile rows unused (BG_DARK uses palette index zero there,
    # whose debugging colour is bright green).  Centring all 160 rows in our
    # taller arena promoted those hidden rows into visible scenery and caused
    # the infamous solid green/black strip below Dark, Ghost and Hyper Beam.
    #
    # The TFT uses the same 4:3 nearest-neighbour pixel scale as the rest of
    # the adapted GBA interface.  A 320x184 arena corresponds to exactly the
    # first 240x138 source pixels at that scale.  Crop that authored camera
    # area first and scale it once.  There is no edge wrapping, reflection,
    # stale terrain margin or interpolated colour, and every destination pixel
    # has one deterministic FireRed source pixel.
    full_indices, palette = battle_animation_indexed_plane(
        gfx_name, map_name, palette_name)
    source_width = 240
    source_height = 184 * 3 // 4
    native_indices = full_indices.crop((0, 0, source_width, source_height))
    output_indices = native_indices.resize((320, 184), Image.Resampling.NEAREST)
    native = indexed_rgba(native_indices, palette)
    output = indexed_rgba(output_indices, palette)
    # Guard the pixel mapping itself.  A later bilinear resize would invent
    # colours and make the generated audit image look plausible while the
    # palette-cycling runtime no longer matched the source indices.
    source_pixels = native.load()
    output_pixels = output.load()
    for destination_x, destination_y in ((0, 0), (319, 0), (0, 183),
                                          (319, 183), (160, 92)):
        source_x = destination_x * 3 // 4
        source_y = destination_y * 3 // 4
        if output_pixels[destination_x, destination_y] != source_pixels[source_x, source_y]:
            raise RuntimeError(
                f"Move background scaling changed: {gfx_name}/{map_name}")
    return output


def compose_battle_animation_tiled_background(gfx_name: str, map_name: str,
                                               palette_name: str | None) -> Image.Image:
    """Return the untouched 256x256 GBA screenblock used by BG3 scrolling."""
    indices, palette = battle_animation_indexed_plane(
        gfx_name, map_name, palette_name)
    return indexed_rgba(indices, palette)


def battle_animation_task_indexed_plane(
        gfx_relative: str,
        map_relative: str) -> tuple[Image.Image,
                                    list[tuple[int, int, int]],
                                    int | None]:
    source_path = DECOMP / "graphics" / gfx_relative
    map_path = DECOMP / "graphics" / map_relative
    tiles = Image.open(source_path)
    if tiles.mode != "P":
        raise RuntimeError(f"Task background is not indexed: {gfx_relative}")
    output = compose_tilemap_indices(tiles, map_path, 32)
    if output.size != (256, 256):
        raise RuntimeError(f"Task background has wrong size: {gfx_relative}")
    palette = embedded_palette(tiles)
    transparent = 0 if (gfx_relative.endswith("/curse.png") or
                        gfx_relative.endswith("/cure_bubbles.png")) else None
    return output, palette, transparent


def compose_battle_animation_task_background(gfx_relative: str,
                                              map_relative: str) -> Image.Image:
    indices, palette, transparent = battle_animation_task_indexed_plane(
        gfx_relative, map_relative)
    return indexed_rgba(indices, palette, transparent)


def surf_wave_indexed_plane(
        side: str, muddy: bool = False
        ) -> tuple[Image.Image, list[tuple[int, int, int]]]:
    """Extract the complete horizontal Surf strip with native palette ids.

    Surf does not create an OBJ sprite in the original game. Its visual task
    scrolls a 512x256 two-screenblock BG over the arena and alpha-blends it.
    Only one 112px vertical band contains authored crest pixels; the rest is
    the same flat water colour. Retaining the full 512px width and that band
    preserves its wrap/direction while fitting beside battle terrain in RAM.
    Alpha is applied once by the renderer from FireRed's BLDALPHA envelope;
    the package itself remains opaque.
    """
    if side not in ("player", "opponent"):
        raise ValueError(f"Invalid Surf side: {side}")
    folder = DECOMP / "graphics/battle_anims/backgrounds"
    water_source = Image.open(folder / "water.png")
    if water_source.mode != "P":
        raise RuntimeError("Expected indexed FireRed Surf background")
    palette = (jasc_palette(folder / "water_muddy.pal")
               if muddy else embedded_palette(water_source))
    map_path = folder / f"water_{side}.bin"
    # BG_ANIM_SCREEN_SIZE=1 is two horizontal 32x32 screenblocks. The generic
    # 64-column decoder would read the second screenblock as row-major; Surf's
    # map instead uses the GBA screenblock address layout explicitly.
    values = struct.unpack(f"<{map_path.stat().st_size // 2}H",
                           map_path.read_bytes())
    full = Image.new("L", (512, 256), 0)
    tiles = palette_index_plane(water_source)
    tiles_per_row = water_source.width // 8
    for row in range(32):
        for column in range(64):
            entry = values[(column // 32) * 1024 + row * 32 +
                           (column % 32)]
            tile_id = entry & 0x3FF
            tx = tile_id % tiles_per_row * 8
            ty = tile_id // tiles_per_row * 8
            if ty + 8 > water_source.height:
                continue
            tile = tiles.crop((tx, ty, tx + 8, ty + 8))
            if entry & 0x400:
                tile = tile.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            if entry & 0x800:
                tile = tile.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
            full.paste(tile, (column * 8, row * 8))

    top = 0 if side == "player" else 144
    return full.crop((0, top, 512, top + 112)), palette


def compose_surf_wave(side: str, muddy: bool = False) -> Image.Image:
    indices, palette = surf_wave_indexed_plane(side, muddy)
    return indexed_rgba(indices, palette)


def compose_battle_terrain(tiles_path: Path, map_path: Path,
                           palette_path: Path) -> Image.Image:
    """Decode the 64x32 FireRed battle map with palette index 0 transparent.

    Unlike ordinary menu maps, battle terrain is placed in two separate OBJ
    planes by the original battle engine. Initialising unused map cells to
    opaque black made the unoccupied middle of the widescreen viewport cover
    both battlers on the ESP32. Keep those cells transparent here, then lay
    the two genuine FireRed platform regions over the battle backdrop.
    """
    source = Image.open(tiles_path)
    if source.mode != "P":
        raise RuntimeError(f"Expected indexed battle terrain: {tiles_path}")
    source_indices = list(source.get_flattened_data())
    palettes = jasc_palettes(palette_path)
    tile_sheets: list[Image.Image] = []
    for palette in palettes:
        sheet = Image.new("RGBA", source.size)
        sheet.putdata([(*palette[value], 0 if value == 0 else 255)
                       for value in source_indices])
        tile_sheets.append(sheet)
    values = struct.unpack(f"<{map_path.stat().st_size // 2}H", map_path.read_bytes())
    output = Image.new("RGBA", (512, 256), (0, 0, 0, 0))
    tiles_per_row = source.width // 8
    for row in range(32):
        for column in range(64):
            # Battle maps use two 32x32 GBA screenblocks laid out side by
            # side, not a single row-major 64-column map.
            entry = values[(column // 32) * 1024 + row * 32 + (column % 32)]
            tile_id = entry & 0x3FF
            # The three terrain palettes are loaded into GBA BG banks 2..4.
            # Bank zero is the untouched/empty screenblock and must remain
            # transparent.  The previous exporter ignored these bank bits,
            # flattening the platform and backdrop into the same palette.
            gba_palette_bank = (entry >> 12) & 0xF
            if gba_palette_bank < 2:
                continue
            palette_bank = gba_palette_bank - 2
            if palette_bank >= len(tile_sheets):
                raise RuntimeError(
                    f"Terrain map requests palette bank {gba_palette_bank}, "
                    f"but {palette_path} has only {len(tile_sheets)} banks")
            tiles = tile_sheets[palette_bank]
            tx = (tile_id % tiles_per_row) * 8
            ty = (tile_id // tiles_per_row) * 8
            if ty + 8 > tiles.height:
                continue
            tile = tiles.crop((tx, ty, tx + 8, ty + 8))
            if entry & 0x400:
                tile = tile.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            if entry & 0x800:
                tile = tile.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
            output.alpha_composite(tile, (column * 8, row * 8))
    return output


def battle_terrain_viewport(terrain: Image.Image) -> Image.Image:
    """Centre FireRed's native 240x160 battle frame in a 320x184 arena.

    The previous widescreen adapter separated the two platforms and extended
    every source row with one dominant colour.  On hardware that produced long
    solid bars and clipped platform fragments which looked like stretched
    artwork. Keep the ROM's complete frame and geometry instead. Only the
    empty 40 px side margins and 12 px vertical margins are completed from
    clean scanline colours already present in that same terrain.
    """
    native = terrain.crop((0, 0, 240, 160))
    source_native = native.copy()
    # Keep every opaque native pixel, including both platforms. Full backdrop
    # rows are extended only through the 40 px side margins; no platform is
    # cropped, moved or resized. Rows below the GBA tilemap are transparent,
    # so continue the nearest backdrop scanline there and then overlay any
    # remaining native platform pixels.
    native_pixels = list(native.get_flattened_data())
    rows = [native_pixels[y * native.width:(y + 1) * native.width]
            for y in range(native.height)]
    # The GBA camera contains only the upper, clipped edge of the player's
    # platform at x=0..127, y=96..111. Remove that fragment before adapting
    # the viewport. A complete copy of the foe platform is placed below after
    # the backdrop has been assembled.
    for y in range(96, 112):
        background = rows[y][-1]
        rows[y][:128] = [background] * 128
    full_rows = [index for index, row in enumerate(rows)
                 if sum(pixel[3] >= 128 for pixel in row) > native.width // 2]
    fallback = (230, 255, 230, 255)
    viewport_pixels: list[tuple[int, int, int, int]] = []
    for output_y in range(184):
        source_y = min(native.height - 1, max(0, output_y - 12))
        row = rows[source_y]
        opaque = [pixel for pixel in row if pixel[3] >= 128]
        if len(opaque) > native.width // 2:
            # Enemy platform occupies the right half of the upper rows; the
            # player's platform occupies the left half of the lower rows.
            # Sample the opposite clean edge so a platform-coloured horizontal
            # stroke can never be mistaken for backdrop and stretched outward.
            edge = row if source_y < 72 else reversed(row)
            background = next(pixel for pixel in edge if pixel[3] >= 128)
            output_row = ([background] * 40 +
                          [pixel if pixel[3] >= 128 else background for pixel in row] +
                          [background] * 40)
        else:
            nearest = min(full_rows, key=lambda index: abs(index - source_y)) if full_rows else source_y
            nearest_opaque = [pixel for pixel in rows[nearest] if pixel[3] >= 128]
            background = (max(set(nearest_opaque), key=nearest_opaque.count)
                          if nearest_opaque else fallback)
            output_row = [background] * 320
            for x, pixel in enumerate(row):
                if pixel[3] >= 128:
                    output_row[x + 40] = pixel
        viewport_pixels.extend(output_row)
    viewport = Image.new("RGBA", (320, 184))
    viewport.putdata(viewport_pixels)
    # Use the *same complete FireRed platform* on both sides. The foe platform
    # occupies x=112..239, y=46..79 in the native scene.  Comparing the crop
    # against a backdrop colour is not a valid mask: several terrain palettes
    # deliberately reuse that colour inside the platform, which was why the
    # player's grass/water pad lost its middle on hardware.  The source pad is
    # an ellipse; retain every source pixel inside that exact silhouette,
    # mirror only on X, and place it at the equivalent lower-left anchor.
    player_platform = Image.new("RGBA", (128, 34), (0, 0, 0, 0))
    source_pixels = source_native.load()
    platform_pixels = player_platform.load()
    radius_x = 64.0
    radius_y = 17.0
    for local_y, source_y in enumerate(range(46, 80)):
        # Pixel-centre ellipse.  A half-pixel inset avoids a one-pixel box at
        # the four extrema without erasing any interior palette entries.
        norm_y = (local_y + 0.5 - radius_y) / radius_y
        for local_x, source_x in enumerate(range(112, 240)):
            norm_x = (local_x + 0.5 - radius_x) / radius_x
            if norm_x * norm_x + norm_y * norm_y <= 1.0:
                platform_pixels[local_x, local_y] = source_pixels[source_x, source_y]
    player_platform = player_platform.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    viewport.alpha_composite(player_platform, (40, 114))
    return viewport


def rgb565(red: int, green: int, blue: int) -> int:
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def top_down_tall_grass_icon() -> Image.Image:
    """Return one complete, transparent FireRed tall-grass metatile.

    Tall grass is part of Route 1's base map layer, not its transparent
    foreground layer.  The old crop therefore produced a fully transparent
    24x24 asset.  Keep the real 16x16 metatile, remove its flat ground colour,
    and discard disconnected ground-detail specks around the main tuft.
    """
    route = render_map_layers(DECOMP, "Route1").composite
    tuft = route.crop((160, 112, 176, 128)).convert("RGBA")
    flat_ground = route.crop((112, 32, 128, 48)).convert("RGBA")
    # This normal Route 1 ground metatile has exactly the two colours used by
    # the floor and its tiny decorative blades. Use them only to discover the
    # outside silhouette. Do *not* erase matching pixels inside that contour:
    # FireRed reuses those colours as highlights between the leaves, and doing
    # so created conspicuous transparent/white holes on the Home button.
    ground_colors = set(flat_ground.get_flattened_data())
    source_pixels = list(tuft.get_flattened_data())
    masked_pixels: list[tuple[int, int, int, int]] = []
    for y in range(tuft.height):
        plant_x = [x for x in range(tuft.width)
                   if source_pixels[y * tuft.width + x] not in ground_colors]
        first = min(plant_x) if plant_x else tuft.width
        last = max(plant_x) if plant_x else -1
        for x in range(tuft.width):
            pixel = source_pixels[y * tuft.width + x]
            masked_pixels.append((*pixel[:3], 255 if first <= x <= last else 0))
    tuft.putdata(masked_pixels)
    if not tuft.getbbox():
        raise RuntimeError("Tall-grass icon extraction produced an empty image")
    return tuft.resize((24, 24), Image.Resampling.NEAREST)


def write_asset(relative: str, image: Image.Image, manifest: dict) -> None:
    image = image.convert("RGBA")
    destination = OUTPUT / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    palette: list[int] = []
    palette_indices: dict[int, int] = {}
    indices: list[int] = []
    for red, green, blue, alpha in image.getdata():
        value = TRANSPARENT if alpha < 128 else rgb565(red, green, blue)
        if value == TRANSPARENT and alpha >= 128:
            value = 0xF81E
        index = palette_indices.get(value)
        if index is None:
            index = len(palette)
            palette_indices[value] = index
            palette.append(value)
        indices.append(index)

    # 4bpp covers virtually all FireRed sprites and tiles. Composited maps
    # commonly use several of the GBA's 16-colour sub-palettes at once, but
    # still remain below 64 colours. Packing those indices at 6bpp preserves
    # every source colour while allowing both Home layers to coexist with
    # NVS, FAT and the save on a non-PSRAM ESP32.
    bits_per_pixel = 4 if len(palette) <= 16 else 6 if len(palette) <= 64 else 8
    if bits_per_pixel == 4:
        payload = bytearray((len(indices) + 1) // 2)
        for pixel, index in enumerate(indices):
            if pixel & 1:
                payload[pixel >> 1] |= index
            else:
                payload[pixel >> 1] = index << 4
    elif bits_per_pixel == 6:
        payload = bytearray((len(indices) * 6 + 7) // 8)
        bit_offset = 0
        for index in indices:
            for bit in range(6):
                if index & (1 << (5 - bit)):
                    absolute = bit_offset + bit
                    payload[absolute >> 3] |= 1 << (7 - (absolute & 7))
            bit_offset += 6
    else:
        payload = bytes(indices)

    encoded = (
        struct.pack("<4sHHHBB", b"PKG2", image.width, image.height, TRANSPARENT,
                    bits_per_pixel, len(palette)) +
        struct.pack(f"<{len(palette)}H", *palette) + payload)
    # Preserve timestamps for byte-identical assets. Robocopy can then update
    # an inserted microSD with only genuinely changed graphics instead of
    # rewriting six thousand tiny FAT entries after every catalog rebuild.
    if not destination.exists() or destination.read_bytes() != encoded:
        destination.write_bytes(encoded)
    manifest[relative] = {"width": image.width, "height": image.height}


def write_indexed_asset(relative: str, image: Image.Image,
                        palette_rgb: list[tuple[int, int, int]],
                        manifest: dict,
                        transparent_index: int | None = None) -> None:
    """Write a PKG2 while preserving every original GBA palette index.

    This is intentionally separate from ``write_asset``: general UI/map
    composites may compact equivalent colours, but battle animation palette
    callbacks address numeric entries.  Re-indexing those images makes an
    otherwise correct cel flash/rotate unrelated colours.
    """
    if image.mode not in ("L", "P"):
        raise RuntimeError(f"Indexed asset is not L/P mode: {relative}")
    indices = list(palette_index_plane(image).get_flattened_data())
    if not palette_rgb or len(palette_rgb) > 255:
        raise RuntimeError(f"Invalid indexed palette size for {relative}")
    invalid = next((value for value in indices if value >= len(palette_rgb)), None)
    if invalid is not None:
        raise RuntimeError(
            f"Palette index {invalid} exceeds {len(palette_rgb)} colours in {relative}")
    if transparent_index is not None and not 0 <= transparent_index < len(palette_rgb):
        raise RuntimeError(f"Invalid transparent index for {relative}")

    palette: list[int] = []
    for index, (red, green, blue) in enumerate(palette_rgb):
        value = TRANSPARENT if index == transparent_index else rgb565(red, green, blue)
        # F81F is the renderer's transparency key. Preserve an authored opaque
        # near-magenta as its adjacent RGB565 value, matching write_asset.
        if value == TRANSPARENT and index != transparent_index:
            value = 0xF81E
        palette.append(value)

    bits_per_pixel = 4 if len(palette) <= 16 else 6 if len(palette) <= 64 else 8
    if bits_per_pixel == 4:
        payload = bytearray((len(indices) + 1) // 2)
        for pixel, index in enumerate(indices):
            if pixel & 1:
                payload[pixel >> 1] |= index
            else:
                payload[pixel >> 1] = index << 4
    elif bits_per_pixel == 6:
        payload = bytearray((len(indices) * 6 + 7) // 8)
        bit_offset = 0
        for index in indices:
            for bit in range(6):
                if index & (1 << (5 - bit)):
                    absolute = bit_offset + bit
                    payload[absolute >> 3] |= 1 << (7 - (absolute & 7))
            bit_offset += 6
    else:
        payload = bytes(indices)

    encoded = (
        struct.pack("<4sHHHBB", b"PKG2", image.width, image.height,
                    TRANSPARENT, bits_per_pixel, len(palette)) +
        struct.pack(f"<{len(palette)}H", *palette) + payload
    )
    destination = OUTPUT / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.read_bytes() != encoded:
        destination.write_bytes(encoded)
    manifest[relative] = {
        "width": image.width,
        "height": image.height,
        "paletteIndexed": True,
    }


def asset_hash(relative: str) -> int:
    """Stable FNV-1a key shared with the ESP32 asset reader.

    The archive intentionally stores only this key and the byte offset.  The
    paths are compile-time strings in the firmware, and collision checking
    here makes a compact 8-byte index safe without spending tens of KiB of
    precious ESP32 RAM on 4,774 full filenames.
    """
    value = 2166136261
    for byte in relative.encode("ascii"):
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def write_asset_archive(manifest: dict[str, dict]) -> None:
    """Build one sequential, read-only asset stream for the microSD card.

    Header: magic, version, index-entry size, asset count, index offset,
    data offset. Each hash-sorted index entry is
    `<uint32 fnv1a, uint32 offset, uint32 length>`. Payloads are written in
    path order, which places all numbered cels of one animation contiguously.
    The device opens this stream only for one bounded preload transaction;
    it never retains an SD descriptor while rendering or accepting input.
    """
    entries = sorted((asset_hash(relative), relative) for relative in manifest)
    collisions = [(left, right) for (left_hash, left), (right_hash, right) in zip(entries, entries[1:])
                  if left_hash == right_hash]
    if collisions:
        raise RuntimeError(f"FNV-1a archive key collision: {collisions[0]}")

    ARCHIVE.parent.mkdir(parents=True, exist_ok=True)
    # PGA revision 3 carries the asset-pack version inside the archive itself.
    # A loose pack_version.txt can be committed even when a damaged/update-
    # interrupted card still contains the previous .pak.  Encoding the same
    # version in the archive makes that mixed state impossible to accept at
    # boot.
    header = struct.Struct("<4sHHH2xIII")
    entry = struct.Struct("<III")
    index_offset = header.size
    data_offset = index_offset + len(entries) * entry.size
    data_entries = sorted(manifest)
    ranges: dict[str, tuple[int, int]] = {}
    cursor = data_offset
    for relative in data_entries:
        size = (OUTPUT / relative).stat().st_size
        ranges[relative] = (cursor, size)
        cursor += size

    with ARCHIVE.open("wb") as destination:
        destination.write(header.pack(
            b"PGA1", 3, entry.size, ASSET_PACK_VERSION,
            len(entries), index_offset, data_offset))
        for key, relative in entries:
            offset, length = ranges[relative]
            destination.write(entry.pack(key, offset, length))
        for relative in data_entries:
            destination.write((OUTPUT / relative).read_bytes())
    print(f"Packed {len(entries)} assets into {ARCHIVE} ({ARCHIVE.stat().st_size} bytes)")


def patch_asset_archive(relative: str) -> None:
    """Replace one payload in an existing PGA3 archive atomically.

    This is intentionally used for tiny visual corrections: rebuilding the
    archive from thousands of loose files is extremely slow on Windows/FAT,
    while a PGA archive already contains every immutable neighbouring asset.
    """
    header = struct.Struct("<4sHHH2xIII")
    entry = struct.Struct("<III")
    archive = bytearray(ARCHIVE.read_bytes())
    if len(archive) < header.size:
        raise RuntimeError("Existing asset archive is truncated")
    (magic, revision, entry_size, _pack_version, asset_count,
     index_offset, data_offset) = header.unpack_from(archive)
    if (magic != b"PGA1" or revision != 3 or entry_size != entry.size or
            index_offset < header.size or data_offset <= index_offset or
            data_offset > len(archive)):
        raise RuntimeError("Existing asset archive identity is invalid")

    wanted = asset_hash(relative)
    target_entry_offset = None
    target_payload_offset = None
    target_payload_length = None
    entries: list[tuple[int, int, int, int]] = []
    for index in range(asset_count):
        position = index_offset + index * entry.size
        key, offset, length = entry.unpack_from(archive, position)
        if offset < data_offset or offset + length > len(archive):
            raise RuntimeError("Existing asset archive index is corrupt")
        entries.append((position, key, offset, length))
        if key == wanted:
            target_entry_offset = position
            target_payload_offset = offset
            target_payload_length = length
    if target_entry_offset is None:
        raise RuntimeError(f"Asset is absent from archive: {relative}")

    replacement = (OUTPUT / relative).read_bytes()
    start = int(target_payload_offset)
    end = start + int(target_payload_length)
    delta = len(replacement) - int(target_payload_length)
    patched = archive[:start] + replacement + archive[end:]
    for position, key, offset, length in entries:
        if position == target_entry_offset:
            length = len(replacement)
        elif offset > start:
            offset += delta
        entry.pack_into(patched, position, key, offset, length)
    header.pack_into(patched, 0, magic, revision, entry_size,
                     ASSET_PACK_VERSION, asset_count, index_offset, data_offset)

    temporary = ARCHIVE.with_suffix(".pak.tmp")
    temporary.write_bytes(patched)
    temporary.replace(ARCHIVE)
    print(f"Patched {relative} in {ARCHIVE} ({ARCHIVE.stat().st_size} bytes)")


def home_box_icon() -> Image.Image:
    image = Image.new("RGBA", (24, 24)); draw = ImageDraw.Draw(image)
    draw.rectangle((3, 7, 20, 20), fill=(168, 112, 56), outline=(72, 64, 56))
    draw.rectangle((1, 4, 22, 9), fill=(232, 176, 88), outline=(72, 64, 56))
    draw.rectangle((10, 4, 13, 20), fill=(248, 216, 128))
    draw.ellipse((8, 10, 15, 17), fill=(248, 248, 232), outline=(72, 64, 56))
    draw.rectangle((8, 13, 15, 14), fill=(200, 56, 48))
    draw.ellipse((11, 12, 13, 14), fill=(248, 248, 232), outline=(72, 64, 56))
    return image


def home_mart_icon() -> Image.Image:
    image = Image.new("RGBA", (24, 24)); draw = ImageDraw.Draw(image)
    draw.rectangle((3, 9, 21, 21), fill=(248, 248, 232), outline=(48, 64, 80))
    draw.polygon(((1, 9), (5, 3), (19, 3), (23, 9)), fill=(64, 136, 208), outline=(48, 64, 80))
    for stripe in range(4, 21, 5): draw.rectangle((stripe, 6, stripe + 2, 10), fill=(168, 216, 240))
    draw.rectangle((5, 13, 10, 21), fill=(96, 176, 216))
    draw.rectangle((13, 13, 19, 18), fill=(160, 224, 240), outline=(48, 64, 80))
    draw.ellipse((9, 4, 15, 10), fill=(248, 248, 232), outline=(48, 64, 80))
    draw.rectangle((9, 7, 15, 8), fill=(200, 56, 48))
    return image


def home_route_background() -> Image.Image:
    route = render_overworld_map("Route1", "pallet_town", 24, 40)
    output = Image.new("RGBA", (304, 134))
    grass = route.crop((64, 224, 80, 240))
    for y in range(0, 134, 16):
        for x in range(0, 304, 16): output.alpha_composite(grass, (x, y))
    output.alpha_composite(route.crop((32, 566, 336, 590)), (0, 0))
    output.alpha_composite(route.crop((32, 184, 64, 232)), (0, 42))
    output.alpha_composite(route.crop((320, 184, 352, 232)), (272, 49))
    output.alpha_composite(route.crop((32, 72, 64, 104)), (20, 91))
    output.alpha_composite(route.crop((304, 32, 336, 64)), (252, 92))
    return output


def _object_event_frame(root: Path, graphics_id: str,
                        movement_type: str) -> Image.Image | None:
    """Return one transparent, correctly-facing 16x32 overworld frame."""
    if not graphics_id.startswith("OBJ_EVENT_GFX_") or "_VAR_" in graphics_id:
        return None
    source_name = graphics_id.removeprefix("OBJ_EVENT_GFX_").lower()
    if source_name == "union_room_nurse":
        source_name = "union_room_attendant"
    path = root / "graphics/object_events/pics/people" / f"{source_name}.png"
    if not path.exists():
        return None
    sheet = Image.open(path)
    direction = 0
    if "FACE_UP" in movement_type:
        direction = 1
    elif "FACE_LEFT" in movement_type:
        direction = 2
    elif "FACE_RIGHT" in movement_type:
        direction = 3
    if (direction + 1) * 16 > sheet.width:
        direction = 0
    frame = sheet.crop((direction * 16, 0, direction * 16 + 16, 32))
    rgba = frame.convert("RGBA")
    if frame.mode == "P":
        alpha = Image.new("L", frame.size)
        alpha.putdata([0 if index == 0 else 255
                       for index in frame.get_flattened_data()])
        rgba.putalpha(alpha)
    return rgba


def pokemon_center_scenes() -> list[Image.Image]:
    """Compose ten original-map Nurse views from FireRed and Emerald.

    Object events live outside layout block data in both engines. Render the
    lower map layer, insert each selected map's authored people, then restore
    the foreground priority layer so counters and furniture correctly occlude
    their lower pixels. The final 240x120 GBA crop scales uniformly to the
    320x160 upper area reserved by the firmware's Nurse screen.
    """
    output: list[Image.Image] = []
    layout_documents: dict[Path, dict] = {}
    for game, map_name in POKECENTER_SCENES:
        root = DECOMP if game == "firered" else EMERALD
        map_document = json.loads(
            (root / "data/maps" / map_name / "map.json").read_text(
                encoding="utf-8"))
        if root not in layout_documents:
            layout_documents[root] = json.loads(
                (root / "data/layouts/layouts.json").read_text(
                    encoding="utf-8"))
        layout = next(entry for entry in layout_documents[root]["layouts"]
                      if entry.get("id") == map_document["layout"])
        layout_folder = Path(layout["blockdata_filepath"]).parent.name
        layers = render_map_layers(root, layout_folder)
        scene = layers.base.copy()
        nurse_event = next(
            (event for event in map_document.get("object_events", [])
             if "NURSE" in event.get("graphics_id", "")), None)
        if nurse_event is None:
            raise RuntimeError(f"Nurse event missing from {game}:{map_name}")
        for event in map_document.get("object_events", []):
            frame = _object_event_frame(
                root, str(event.get("graphics_id", "")),
                str(event.get("movement_type", "")))
            if frame is None:
                continue
            # Matches the Gen-III object-event anchor: the visible part of a
            # standing 16x32 sheet rests on the event's metatile coordinate.
            x = int(event["x"]) * 16
            y = int(event["y"]) * 16 - 25
            scene.alpha_composite(frame, (x, y))
        scene.alpha_composite(layers.foreground)

        crop_width, crop_height = 240, 120
        maximum_x = max(0, scene.width - crop_width)
        maximum_y = max(0, scene.height - crop_height)
        crop_x = max(0, min(maximum_x, int(nurse_event["x"]) * 16 - 112))
        crop_y = max(0, min(maximum_y, int(nurse_event["y"]) * 16 - 32))
        crop = scene.crop((crop_x, crop_y,
                           crop_x + crop_width, crop_y + crop_height))
        output.append(crop.resize((320, 160), Image.Resampling.NEAREST))
    return output


def normalized_home_icon(source: Image.Image, size: int = 24,
                         visible_size: int = 22) -> Image.Image:
    """Centre a Home glyph by its visible pixels, not its source canvas.

    Several genuine FireRed assets have asymmetric transparent padding.  The
    old scaler preserved that padding, so correctly positioned 24 px packages
    still looked displaced inside otherwise identical Home buttons.  Trim only
    the transparent border, fit every glyph into the same 22 px visual field,
    and retain a one-pixel safety margin for the parchment frame.
    """
    image = source.convert("RGBA")
    if image.width <= 0 or image.height <= 0:
        return Image.new("RGBA", (size, size), (0, 0, 0, 0))
    alpha_bounds = image.getchannel("A").getbbox()
    if alpha_bounds is None:
        return Image.new("RGBA", (size, size), (0, 0, 0, 0))
    image = image.crop(alpha_bounds)
    visual_size = max(1, min(size, visible_size))
    scale = min(visual_size / image.width, visual_size / image.height)
    width = max(1, min(visual_size, round(image.width * scale)))
    height = max(1, min(visual_size, round(image.height * scale)))
    image = image.resize((width, height), Image.Resampling.NEAREST)
    output = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    output.alpha_composite(image, ((size - width) // 2, (size - height) // 2))
    return output


def _stacked_sprite_frames(path: Path, frame_width: int,
                           frame_height: int) -> list[Image.Image]:
    """Split an original GBA OBJ/field sheet into its authored cels."""
    source = source_png(path)
    if source.width % frame_width or source.height % frame_height:
        raise RuntimeError(
            f"Home effect sheet has an invalid cel grid: {path} {source.size} "
            f"for {frame_width}x{frame_height}")
    frames: list[Image.Image] = []
    for top in range(0, source.height, frame_height):
        for left in range(0, source.width, frame_width):
            frame = source.crop((left, top, left + frame_width,
                                 top + frame_height))
            if frame.getchannel("A").getbbox() is not None:
                frames.append(frame)
    if not frames:
        raise RuntimeError(f"Home effect sheet contains no visible cels: {path}")
    return frames


def _fit_home_effect_cel(source: Image.Image, phase: int,
                         cell_size: int) -> Image.Image:
    """Fit one untouched pixel-art cel inside a compact atlas cell."""
    alpha_box = source.getchannel("A").getbbox()
    if alpha_box is None:
        raise RuntimeError("Attempted to place an empty Home effect cel")
    source = source.crop(alpha_box)
    maximum = cell_size - 1
    scale = min(maximum / source.width, maximum / source.height)
    width = max(1, round(source.width * scale))
    height = max(1, round(source.height * scale))
    source = source.resize((width, height), Image.Resampling.NEAREST)
    cell = Image.new("RGBA", (cell_size, cell_size), (0, 0, 0, 0))
    # A one-pixel bob keeps single-cel effects alive without surrounding the
    # source art with invented particles.
    bob = (0, -1, 0, 1, 0, -1)[phase % 6]
    left = (cell_size - width) // 2
    top = max(0, min(cell_size - height,
                     (cell_size - height) // 2 + bob))
    cell.alpha_composite(source, (left, top))
    return cell


def home_move_effect_atlas() -> Image.Image:
    """Build six authentic FireRed cels for each of the 18 battle types.

    Fly, Dig, Surf, Cut/grass movement, the generic field-move streak and
    field sparkle use their genuine overworld effects. Types without a
    matching field move use original FireRed battle OBJ art instead of the
    old hand-drawn firmware glyphs.
    """
    field = DECOMP / "graphics/field_effects/pics"
    battle = DECOMP / "graphics/battle_anims/sprites"
    # Enum order matches PokemonType in firmware: Normal through Fairy.
    specifications = (
        (field / "field_move_streaks_outdoors.png", 32, 32),  # Normal
        (battle / "red_fist.png", 32, 32),                   # Fighting
        (field / "bird.png", 64, 64),                        # Flying / Fly
        (battle / "toxic_bubble.png", 16, 16),               # Poison
        (field / "sand_pile.png", 16, 8),                    # Ground / Dig
        (battle / "rocks.png", 32, 32),                      # Rock
        (battle / "web.png", 32, 32),                        # Bug
        (battle / "wisp_fire.png", 32, 32),                  # Ghost
        (battle / "torn_metal.png", 32, 32),                 # Steel
        (battle / "small_ember.png", 32, 32),                # Fire
        (field / "ripple.png", 16, 16),                      # Water / Surf
        (field / "tall_grass.png", 16, 16),                  # Grass / Cut
        (battle / "electricity.png", 32, 32),                # Electric
        (field / "small_sparkle.png", 16, 16),               # Psychic / Teleport
        (battle / "ice_crystals_0.png", 16, 16),             # Ice
        (battle / "breath.png", 16, 16),                     # Dragon
        (battle / "shadow_ball.png", 32, 32),                # Dark
        (battle / "pink_heart_2.png", 32, 32),               # Fairy
    )
    cell_size = 18
    phase_count = 6
    atlas = Image.new("RGBA", (len(specifications) * cell_size,
                                phase_count * cell_size), (0, 0, 0, 0))
    for type_index, (path, frame_width, frame_height) in enumerate(specifications):
        frames = _stacked_sprite_frames(path, frame_width, frame_height)
        for phase in range(phase_count):
            if len(frames) >= phase_count:
                frame_index = round(phase * (len(frames) - 1) /
                                    (phase_count - 1))
            else:
                frame_index = phase % len(frames)
            cell = _fit_home_effect_cel(frames[frame_index], phase, cell_size)
            atlas.alpha_composite(cell, (type_index * cell_size,
                                         phase * cell_size))

    # One shared 63-colour palette plus transparency keeps this at 6bpp
    # (26 KiB), so it remains in RAM beside the 30 KiB Home map.
    alpha = atlas.getchannel("A")
    opaque_rgb = Image.new("RGB", atlas.size, (0, 0, 0))
    opaque_rgb.paste(atlas.convert("RGB"), mask=alpha)
    quantized = opaque_rgb.quantize(
        colors=63, method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.NONE).convert("RGBA")
    quantized.putalpha(alpha)
    visible_colors = {pixel[:3] for pixel in quantized.getdata()
                      if pixel[3] >= 128}
    if len(visible_colors) > 63:
        raise RuntimeError("Home move atlas exceeded its 6bpp colour budget")
    for type_index in range(len(specifications)):
        for phase in range(phase_count):
            cell = quantized.crop((type_index * cell_size, phase * cell_size,
                                   (type_index + 1) * cell_size,
                                   (phase + 1) * cell_size))
            if cell.getchannel("A").getbbox() is None:
                raise RuntimeError(
                    f"Empty Home move effect: type={type_index} phase={phase}")
    preview = ROOT / ".generated/home_move_effect_atlas.png"
    preview.parent.mkdir(parents=True, exist_ok=True)
    quantized.save(preview)
    return quantized


def remove_connected_light_background(source: Image.Image) -> Image.Image:
    """Remove only the near-white photo background connected to an edge.

    The user-approved Pokedex reference is a JPEG rather than a native RGBA
    sprite.  Treating its opaque canvas as an icon produced a conspicuous
    white square on Home.  A simple global white-key would also erase the
    Pokedex screen and D-pad, so flood only pale, neutral pixels reachable
    from the outside border and preserve enclosed white details.
    """
    image = source.convert("RGBA")
    mask = Image.new("L", image.size, 0)
    mask.putdata([
        255 if min(red, green, blue) >= 215 and
               max(red, green, blue) - min(red, green, blue) <= 28 else 0
        for red, green, blue, _ in image.get_flattened_data()
    ])
    draw = ImageDraw.Draw(mask)
    border = (
        *((x, 0) for x in range(image.width)),
        *((x, image.height - 1) for x in range(image.width)),
        *((0, y) for y in range(image.height)),
        *((image.width - 1, y) for y in range(image.height)),
    )
    for point in border:
        if mask.getpixel(point) == 255:
            ImageDraw.floodfill(mask, point, 128, thresh=0)
    alpha = Image.new("L", image.size, 255)
    alpha.putdata([0 if value == 128 else 255
                   for value in mask.get_flattened_data()])
    image.putalpha(alpha)
    return image


def home_pokecenter_icon() -> Image.Image:
    """Build a compact Center glyph from Nurse Joy's genuine FRLG sprite."""
    nurse = source_png(DECOMP / "graphics/object_events/pics/people/nurse.png")
    # First 16px column is the front-facing frame.  Retain her cap, face and
    # shoulders; omitting the lower uniform makes the portrait legible inside
    # a 24px action button and leaves room for the charge badge at top-right.
    portrait = nurse.crop((0, 9, 16, 27))
    return normalized_home_icon(portrait)


def home_settings_icon() -> Image.Image:
    """A legible Pokégear control in the palette and pixel scale of FireRed.

    FireRed has no cog item.  This uses the user-approved Poké Ball centred
    gear silhouette, with deliberately pale teeth and a heavy dark outline
    so it reads at the 24px Home-control size.
    """
    image = Image.new("RGBA", (24, 24), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    outline = (48, 56, 72, 255)
    shadow = (112, 136, 144, 255)
    steel = (200, 220, 224, 255)
    shine = (248, 252, 248, 255)
    red = (208, 72, 64, 255)
    # The eight lobes are intentionally rounded/stepped rather than square:
    # they match the reference gear's friendly silhouette at GBA scale.
    lobes = (
        ((9, 0), (14, 0), (16, 3), (16, 7), (8, 7), (8, 3)),
        ((9, 17), (14, 17), (16, 21), (14, 23), (9, 23), (7, 21), (7, 17)),
        ((0, 9), (3, 7), (7, 8), (7, 16), (3, 16), (0, 14)),
        ((17, 8), (21, 7), (23, 9), (23, 14), (21, 16), (17, 16)),
        ((3, 3), (6, 2), (10, 6), (7, 10), (2, 6)),
        ((14, 6), (18, 2), (21, 3), (22, 6), (17, 10)),
        ((2, 17), (7, 14), (10, 18), (6, 22), (3, 21)),
        ((17, 14), (22, 18), (21, 21), (18, 22), (14, 18)),
    )
    for points in lobes:
        draw.polygon(points, fill=steel, outline=outline)
    draw.ellipse((3, 3, 20, 20), fill=outline)
    draw.ellipse((5, 5, 18, 18), fill=steel)
    draw.arc((5, 5, 18, 18), 185, 325, fill=shadow, width=2)
    # Centre Poké Ball: red top, white lower half, black belt and button.
    draw.ellipse((7, 7, 16, 16), fill=outline)
    draw.pieslice((8, 8, 15, 15), 180, 360, fill=shine)
    draw.pieslice((8, 8, 15, 15), 0, 180, fill=red)
    draw.rectangle((8, 11, 15, 12), fill=outline)
    draw.ellipse((10, 10, 13, 13), fill=outline)
    draw.rectangle((11, 11, 12, 12), fill=shine)
    return image


def home_vs_icon() -> Image.Image:
    """Make FireRed's battle-transition V and S readable at Home size.

    The original transition sheet stacks the two letters vertically inside a
    64x128 canvas. Scaling that complete canvas into a 24px button makes each
    letter only a few pixels wide. The exact source glyphs are instead cropped
    and placed side by side as a conventional, legible VS badge.
    """
    sheet = source_png(DECOMP / "graphics/battle_transitions/vs.png")
    icon = Image.new("RGBA", (24, 24), (0, 0, 0, 0))
    v = sheet.crop((12, 10, 54, 57)).resize((10, 18), Image.Resampling.NEAREST)
    s = sheet.crop((12, 66, 54, 120)).resize((10, 18), Image.Resampling.NEAREST)
    icon.alpha_composite(v, (1, 3))
    icon.alpha_composite(s, (13, 3))
    return icon


def battle_tower_home_icon() -> Image.Image:
    """Return option A's official Frontier medal on a transparent canvas."""
    medal_folder = EMERALD / "graphics/frontier_pass"
    medal_indices = palette_index_plane(
        Image.open(medal_folder / "medals.png")).crop((0, 0, 16, 16))
    # Palette index zero is only the rectangular sheet background. Keying
    # precisely that index preserves every authored white/gold/brown pixel in
    # the symbol while removing the unwanted square around the Home icon.
    home = indexed_rgba(medal_indices,
                        jasc_palette(medal_folder / "gold.pal"), 0)
    home_canvas = Image.new("RGBA", (24, 24), (0, 0, 0, 0))
    home_canvas.alpha_composite(home, (4, 4))
    return home_canvas


def battle_tower_assets() -> tuple[Image.Image, Image.Image]:
    """Return the official Tower symbol and invitation facade from Emerald.

    The 16x16 Ability Symbol is the Battle Tower's own first Frontier Pass
    medal. This is the user-selected option A; the invitation still uses the
    real OutsideEast map facade so the destination remains recognisable.
    """
    frontier = render_map_layers(EMERALD, "BattleFrontier_OutsideEast").composite
    nearest = Image.Resampling.NEAREST
    # Keep the exact native 16x16 cel selected in option A. Enlarging it to
    # 22px made its triangular centre turn into a vague round coin on the TFT.
    home_canvas = battle_tower_home_icon()
    facade = frontier.crop((112, 0, 400, 336)).resize((80, 108), nearest)
    return home_canvas, facade


def pokemon_league_assets() -> tuple[Image.Image, Image.Image,
                                     Image.Image, Image.Image]:
    """Build one clear Elite Four glyph and region-correct invitation scenes.

    Indigo Plateau serves both Kanto and Johto in the original games, while
    Hoenn uses Ever Grande.  At Home size, four original Poke Ball item sprites
    surrounding FireRed's Star Piece communicate the four-member challenge
    much more clearly than either full building.  Invitation art remains the
    region's original decoded map.
    """
    indigo = render_map_layers(DECOMP, "IndigoPlateau_Exterior").composite
    ever_grande = render_map_layers(EMERALD, "EverGrandeCity").composite
    nearest = Image.Resampling.NEAREST

    ball = source_png(DECOMP / "graphics/items/icons/poke_ball.png")
    star = source_png(DECOMP / "graphics/items/icons/star_piece.png")
    ball.thumbnail((8, 8), nearest)
    star.thumbnail((8, 8), nearest)
    league_home = Image.new("RGBA", (24, 24), (0, 0, 0, 0))
    for x, y in ((2, 2), (14, 2), (2, 14), (14, 14)):
        league_home.alpha_composite(ball, (x, y))
    league_home.alpha_composite(star, (8, 8))

    indigo_facade = indigo.crop((48, 0, 272, 304)).resize((80, 108), nearest)
    hoenn_facade = ever_grande.crop((192, 0, 448, 346)).resize((80, 108), nearest)
    return league_home, indigo_facade, league_home.copy(), hoenn_facade


def hall_of_fame_victory_background(root: Path, map_name: str) -> Image.Image:
    """Turn the original Hall of Fame room into a 320x240 victory backdrop.

    The source rooms are portrait maps, whereas Pokegochi is landscape.  Keep
    the complete room at its original aspect ratio and extend its floor colour
    into the side gutters instead of stretching the GBA pixels.  The firmware
    adds the title and result cards on top; this asset remains entirely sourced
    from the corresponding game's map and palette.
    """
    room = render_map_layers(root, map_name).composite.convert("RGBA")
    opaque = [pixel for pixel in room.get_flattened_data() if pixel[3] >= 128]
    if not opaque:
        raise RuntimeError(f"Hall of Fame map rendered empty: {map_name}")
    # The most frequent opaque colour is the room's native floor.  It makes
    # the landscape gutters blend into the open central arena without adding
    # a foreign colour or the old black void.
    floor = max(set(opaque), key=opaque.count)
    room_pixels = list(room.get_flattened_data())
    lower_edge = room.height * 7 // 8
    room.putdata([
        floor if y >= lower_edge and pixel[3] >= 128 and
                 max(pixel[:3]) <= 8 else pixel
        for y in range(room.height)
        for pixel in room_pixels[y * room.width:(y + 1) * room.width]
    ])
    canvas = Image.new("RGBA", (320, 240), floor)
    scale = min(320 / room.width, 240 / room.height)
    fitted = room.resize(
        (max(1, round(room.width * scale)), max(1, round(room.height * scale))),
        Image.Resampling.NEAREST,
    )
    canvas.alpha_composite(fitted, ((320 - fitted.width) // 2,
                                    (240 - fitted.height) // 2))
    return canvas


def main() -> None:
    rom = ROM.read_bytes()
    if hashlib.sha1(rom).hexdigest().upper() != EXPECTED_SHA1:
        raise ValueError("Unexpected FireRed ROM")
    manifest: dict[str, dict] = {}
    species_constants=(DECOMP/"include/constants/species.h").read_text(encoding="utf-8")
    pokedex_constants=(DECOMP/"include/constants/pokedex.h").read_text(encoding="utf-8")
    internal={s:int(n) for s,n in re.findall(r"#define SPECIES_([A-Z0-9_]+)\s+(\d+)",species_constants)}
    national_symbols=re.findall(r"^\s*NATIONAL_DEX_([A-Z0-9_]+),",pokedex_constants,re.M)
    national_to_internal={number:internal[symbol] for number,symbol in enumerate(national_symbols)
                          if 1<=number<=386 and symbol in internal}
    pokemon_folders={re.sub(r"[^a-z0-9]","",p.name.lower()):p for p in (DECOMP/"graphics/pokemon").iterdir() if p.is_dir()}
    for species in range(1, 387):
        rom_species = national_to_internal[species]
        symbol=national_symbols[species]
        folder=pokemon_folders.get(re.sub(r"[^a-z0-9]","",symbol.lower()))
        palette_folder=folder/"normal" if folder and (folder/"normal/shiny.pal").exists() else folder
        if not palette_folder or not (palette_folder/"shiny.pal").exists():raise RuntimeError(f"Missing shiny palette for {species} {symbol}")
        normal_palette=jasc_palette(palette_folder/"normal.pal");shiny_palette=jasc_palette(palette_folder/"shiny.pal")
        for frame in range(2):
            indices, palette = extract_icon(rom, rom_species, frame)
            write_asset(f"pokemon/icons/{species:03}_{frame}.pkg", indexed(indices, palette, 32), manifest)
            icon_frame=indexed(indices,palette,32)
            write_asset(f"pokemon/shiny/icons/{species:03}_{frame}.pkg",shiny_icon(icon_frame,normal_palette,shiny_palette),manifest)
        front_indices, front_palette = extract_battle_sprite(rom, rom_species, 0x2350AC)
        back_indices, back_palette = extract_battle_sprite(rom, rom_species, BACK_SPRITE_TABLE)
        front_image = indexed(front_indices, front_palette, 64)
        write_asset(f"pokemon/front/{species:03}.pkg", front_image, manifest)
        write_asset(f"pokemon/back/{species:03}.pkg", indexed(back_indices, back_palette, 64), manifest)
        write_asset(f"pokemon/shiny/front/{species:03}.pkg", indexed(front_indices, shiny_palette, 64), manifest)
        write_asset(f"pokemon/shiny/back/{species:03}.pkg", indexed(back_indices, shiny_palette, 64), manifest)
        # FireRed cycles solid silhouettes of the old and new species during
        # evolution.  Export them once so the ESP32 does not have to recolour
        # a sprite (or reopen the SD card) in the middle of the cinematic.
        alpha = front_image.getchannel("A")
        for tone, color in (("dark", (24, 32, 72, 255)), ("light", (248, 248, 248, 255))):
            silhouette = Image.new("RGBA", front_image.size, color)
            silhouette.putalpha(alpha)
            write_asset(f"pokemon/evolution/{tone}/{species:03}.pkg", silhouette, manifest)

    # Unown's 28 letters are separate graphics in FireRed, not separate
    # species table entries. Export every authentic front/back/icon form so
    # the firmware can preserve the personality-derived letter everywhere.
    unown_folder = DECOMP / "graphics/pokemon/unown"
    unown_forms = [*list("abcdefghijklmnopqrstuvwxyz"), "exclamation_mark", "question_mark"]
    unown_normal = jasc_palette(unown_folder / "normal.pal")
    unown_shiny = jasc_palette(unown_folder / "shiny.pal")
    for form_index, form_name in enumerate(unown_forms):
        form_folder = unown_folder / form_name
        for view in ("front", "back", "icon"):
            image = source_png(form_folder / f"{view}.png")
            if view == "icon":
                # Storage/Home animate two icon frames. Unown's source icon is
                # one 32x64 sheet, matching the regular species icon layout.
                frames = [image.crop((0, frame * 32, 32, frame * 32 + 32))
                          if image.height >= 64 else image for frame in range(2)]
                for frame, icon in enumerate(frames):
                    write_asset(f"pokemon/forms/unown/{form_index:02}/icons/{frame}.pkg", icon, manifest)
                    write_asset(f"pokemon/forms/unown/{form_index:02}/shiny/icons/{frame}.pkg",
                                shiny_icon(icon, unown_normal, unown_shiny), manifest)
            else:
                write_asset(f"pokemon/forms/unown/{form_index:02}/{view}.pkg", image, manifest)
                write_asset(f"pokemon/forms/unown/{form_index:02}/shiny/{view}.pkg",
                            shiny_icon(image, unown_normal, unown_shiny), manifest)

    # Castform's SUNNY/RAINY/SNOWY artwork is stored as four authentic GBA
    # form folders. The menu icon remains the normal Castform icon in Gen III;
    # only battle front/back graphics switch with Forecast.
    castform_root = DECOMP / "graphics/pokemon/castform"
    castform_forms = ("normal", "sunny", "rainy", "snowy")
    for form_index, form_name in enumerate(castform_forms):
        folder = castform_root / form_name
        normal_palette = jasc_palette(folder / "normal.pal")
        shiny_palette = jasc_palette(folder / "shiny.pal")
        for view in ("front", "back"):
            image = source_png(folder / f"{view}.png").crop((0, 0, 64, 64))
            write_asset(f"pokemon/forms/castform/{form_index}/{view}.pkg", image, manifest)
            write_asset(f"pokemon/forms/castform/{form_index}/shiny/{view}.pkg",
                        shiny_icon(image, normal_palette, shiny_palette), manifest)

    # Deoxys' four Generation-III forms were split between the original
    # cartridges.  The expansion decomp keeps the authentic GBA artwork in one
    # tree, allowing a single Pokegochi to change form without synthesising or
    # rescaling replacement art.  Form order matches CollectionLogic:
    # NORMAL, ATTACK, DEFENSE, SPEED.
    deoxys_root = EXPANSION / "graphics/pokemon/deoxys"
    if not deoxys_root.exists():
        raise RuntimeError("Missing Deoxys form sources; initialise pokeemerald-expansion")
    deoxys_forms = ("normal", "attack", "defense", "speed")
    for form_index, form_name in enumerate(deoxys_forms):
        folder = deoxys_root if form_index == 0 else deoxys_root / form_name
        normal_palette = jasc_palette(folder / "normal_gba.pal")
        shiny_palette = jasc_palette(folder / "shiny_gba.pal")
        front_path = folder / ("anim_front_gba.png" if form_index == 0 else "front_gba.png")
        back_path = folder / "back_gba.png"
        icon_path = folder / ("icon_gba.png" if form_index == 0 else "icon.png")
        front = source_png(front_path).crop((0, 0, 64, 64))
        back = source_png(back_path).crop((0, 0, 64, 64))
        icon_sheet = source_png(icon_path)
        write_asset(f"pokemon/forms/deoxys/{form_index}/front.pkg", front, manifest)
        write_asset(f"pokemon/forms/deoxys/{form_index}/back.pkg", back, manifest)
        write_asset(f"pokemon/forms/deoxys/{form_index}/shiny/front.pkg",
                    shiny_icon(front, normal_palette, shiny_palette), manifest)
        write_asset(f"pokemon/forms/deoxys/{form_index}/shiny/back.pkg",
                    shiny_icon(back, normal_palette, shiny_palette), manifest)
        for frame in range(2):
            icon = icon_sheet.crop((0, frame * 32, 32, frame * 32 + 32))
            write_asset(f"pokemon/forms/deoxys/{form_index}/icons/{frame}.pkg", icon, manifest)
            write_asset(f"pokemon/forms/deoxys/{form_index}/shiny/icons/{frame}.pkg",
                        shiny_icon(icon, normal_palette, shiny_palette), manifest)

    # GBA-style official Mega/Primal art from pokeemerald-expansion. The
    # runtime treats these as held-item forms of the existing species, so the
    # National Dex and collection identity remain unchanged.
    for species, folder_name, variant in MEGA_FORMS:
        folder = EXPANSION / "graphics/pokemon" / folder_name / variant
        if not folder.exists():
            raise RuntimeError(f"Missing official form source: {folder_name}/{variant}")
        normal_palette = jasc_palette(folder / "normal.pal")
        shiny_palette = jasc_palette(folder / "shiny.pal")
        front_path = folder / ("anim_front.png" if (folder / "anim_front.png").exists() else "front.png")
        # The decompressed PNG supplies only pixel indices; normal.pal and
        # shiny.pal decide their colours. All 43 Mega back PNGs embed the
        # shiny palette, so source_png(back.png) is observably wrong here.
        front = indexed_source_png(front_path, normal_palette).crop((0, 0, 64, 64))
        back = indexed_source_png(folder / "back.png", normal_palette).crop((0, 0, 64, 64))
        shiny_front = indexed_source_png(front_path, shiny_palette).crop((0, 0, 64, 64))
        shiny_back = indexed_source_png(folder / "back.png", shiny_palette).crop((0, 0, 64, 64))
        icon_sheet = source_png(folder / "icon.png")
        prefix = f"pokemon/forms/mega/{species:03}/{variant}"
        write_asset(f"{prefix}/front.pkg", front, manifest)
        write_asset(f"{prefix}/back.pkg", back, manifest)
        write_asset(f"{prefix}/shiny/front.pkg", shiny_front, manifest)
        write_asset(f"{prefix}/shiny/back.pkg", shiny_back, manifest)
        for frame in range(2):
            top = frame * 32 if icon_sheet.height >= 64 else 0
            icon = icon_sheet.crop((0, top, 32, top + 32))
            write_asset(f"{prefix}/icons/{frame}.pkg", icon, manifest)
            write_asset(f"{prefix}/shiny/icons/{frame}.pkg",
                        shiny_icon(icon, normal_palette, shiny_palette), manifest)

    trainer_dir = DECOMP / "graphics/trainers/front_pics"
    for path in trainer_dir.glob("*.png"):
        write_asset(f"trainers/{path.stem}.pkg", source_png(path), manifest)
    # Emerald-native leader art missing from FireRed (notably Juan).
    emerald_leaders = EMERALD / "graphics/trainers/front_pics"
    for path in emerald_leaders.glob("*.png"):
        write_asset(f"trainers/emerald_{path.stem}.pkg", source_png(path), manifest)
    for leader in ("roxanne", "brawly", "wattson", "flannery", "norman", "winona", "tate_and_liza", "juan"):
        path = emerald_leaders / f"leader_{leader}.png"
        if path.exists(): write_asset(f"trainers/leader_{leader}_front_pic.pkg", source_png(path), manifest)
    for leader in ("falkner", "bugsy", "whitney", "morty", "chuck", "jasmine", "pryce", "clair"):
        path=CRYSTAL/"gfx/trainers"/f"{leader}.png"
        if path.exists():
            sprite=source_png(path).resize((64,64),Image.Resampling.NEAREST)
            write_asset(f"trainers/johto_{leader}.pkg",sprite,manifest)
    for path in (CRYSTAL/"gfx/trainers").glob("*.png"):
        sprite=source_png(path).resize((64,64),Image.Resampling.NEAREST)
        write_asset(f"trainers/johto_{path.stem}.pkg",sprite,manifest)
    for ball in ("poke_ball", "great_ball", "ultra_ball", "master_ball"):
        path=DECOMP/"graphics/items/icons"/f"{ball}.png"
        if path.exists():write_asset(f"firered/items/{ball}.pkg",source_png(path),manifest)
    # The Bag icons above are static. Capture uses the separate 3-frame ball
    # sheets from FireRed's battle renderer (normal, opening, closing), not a
    # hand-drawn substitute.
    battle_ball_sources = {"poke_ball": "poke", "great_ball": "great",
                           "ultra_ball": "ultra", "master_ball": "master"}
    for output_name, source_name in battle_ball_sources.items():
        source = source_png(DECOMP / "graphics/interface/ball" / f"{source_name}.png")
        # FireRed stores two visible 16px cels per ball (closed and open),
        # followed by a transparent padding row. Capture's drop/shake state
        # is the closed cel again. Exporting that padding row as frame three
        # produced a fully transparent ball, while a
        # generic red ball_open overlay changed every specialised ball into a
        # normal Poke Ball at impact.
        closed = source.crop((0, 0, 16, 16))
        opened = source.crop((0, 16, 16, 32))
        if source.width != 16 or source.height < 32 or closed.getchannel("A").getbbox() is None or opened.getchannel("A").getbbox() is None:
            raise RuntimeError(f"Invalid capture-ball sheet: {source_name} {source.size}")
        for frame, frame_image in enumerate((closed, opened, closed)):
            write_asset(f"firered/battle_items/ball_{output_name}_{frame:02}.pkg",
                        frame_image.resize((24, 24), Image.Resampling.NEAREST), manifest)
    # Poke Mart's lower information pane uses the original item sprites. Some
    # consumables deliberately share FireRed's base icon with a palette
    # variant; keeping that original silhouette is preferable to invented art.
    mart_icons = {
        "poke_ball": "poke_ball", "great_ball": "great_ball", "ultra_ball": "ultra_ball",
        "potion": "potion", "large_potion": "large_potion", "full_heal": "full_heal",
        "antidote": "antidote", "status_heal": "status_heal", "battle_stat_item": "battle_stat_item",
    }
    for output_name, source_name in mart_icons.items():
        path = DECOMP / "graphics/items/icons" / f"{source_name}.png"
        if path.exists(): write_asset(f"firered/items/mart_{output_name}.pkg", source_png(path), manifest)
    # The daily Mart can also rotate held items. Their enum order is stable,
    # so numbered files let firmware select the exact original icon cheaply.
    held_icons = (
        "oran_berry", "sitrus_berry", "lum_berry", "persim_berry", "cheri_berry",
        "chesto_berry", "pecha_berry", "rawst_berry", "aspear_berry", "in_battle_herb",
        "leftovers", "shell_bell", "choice_band", "quick_claw", "scope_lens",
        "bright_powder", "focus_band", "kings_rock", "amulet_coin", "smoke_ball",
        "silk_scarf", "black_belt", "sharp_beak", "poison_barb", "soft_sand",
        "hard_stone", "silver_powder", "spell_tag", "metal_coat", "charcoal",
        "mystic_water", "miracle_seed", "magnet", "twisted_spoon", "never_melt_ice",
        "dragon_fang", "black_glasses",
    )
    for index, source_name in enumerate(held_icons, start=1):
        path = DECOMP / "graphics/items/icons" / f"{source_name}.png"
        if not path.exists(): raise RuntimeError(f"Missing held item icon: {source_name}")
        write_asset(f"firered/items/held_{index:02}.pkg", source_png(path), manifest)

    # Berry Garden is a real Route 123 berry patch, not a firmware-drawn set
    # of generic cards.  The six authored soil beds are already part of this
    # Emerald map crop; the matching object-event sheets supply each species'
    # actual sapling, flowering tree and fruit-bearing tree.
    route123 = render_map_layers(EMERALD, "Route123").composite
    garden = route123.crop((9 * 16, 0, 19 * 16, 6 * 16)).resize(
        (320, 192), Image.Resampling.NEAREST)
    write_asset("emerald/berry_garden/background.pkg", garden, manifest)

    berry_tree_names = (
        # Numeric order matches HeldItem and held_XX.pkg.
        "oran", "sitrus", "lum", "persim", "cheri", "chesto",
        "pecha", "rawst", "aspear",
    )
    tree_root = EMERALD / "graphics/object_events/pics/berry_trees"
    sprout_sheet = source_png(tree_root / "sprout.png")
    for frame in range(2):
        # Early-stage objects use a 16x16 cel aligned to the bottom half of
        # the later 16x32 tree canvas.  Keeping one 32x64 destination size
        # lets firmware place every growth stage at the same coordinates.
        sprout = Image.new("RGBA", (16, 32), (0, 0, 0, 0))
        sprout.alpha_composite(sprout_sheet.crop((frame * 16, 0, frame * 16 + 16, 16)),
                               (0, 16))
        write_asset(f"emerald/berry_garden/sprout_{frame}.pkg",
                    sprout.resize((32, 64), Image.Resampling.NEAREST), manifest)
    for berry_index, berry_name in enumerate(berry_tree_names, start=1):
        tree_sheet = source_png(tree_root / f"{berry_name}.png")
        if tree_sheet.size != (96, 32):
            raise RuntimeError(f"Invalid Emerald berry-tree sheet: {berry_name} {tree_sheet.size}")
        # Emerald stages 2/3/4 use authored frame pairs 0/1, 2/3 and 4/5.
        for stage in range(2, 5):
            for frame in range(2):
                source_frame = (stage - 2) * 2 + frame
                tree = tree_sheet.crop((source_frame * 16, 0,
                                        source_frame * 16 + 16, 32))
                write_asset(
                    f"emerald/berry_garden/tree_{berry_index:02}_stage_{stage}_{frame}.pkg",
                    tree.resize((32, 64), Image.Resampling.NEAREST), manifest)
    mega_stone_icon = EXPANSION / "graphics/items/icons/key_stone.png"
    if not mega_stone_icon.exists(): raise RuntimeError("Missing official Key Stone icon")
    write_asset("firered/items/held_38.pkg", source_png(mega_stone_icon), manifest)
    lucky_egg_icon = DECOMP / "graphics/items/icons/lucky_egg.png"
    if not lucky_egg_icon.exists(): raise RuntimeError("Missing original Lucky Egg icon")
    write_asset("firered/items/lucky_egg.pkg", source_png(lucky_egg_icon), manifest)
    home_icons = {
        "feed": DECOMP / "graphics/items/icons/oran_berry.png",
        "bathe": DECOMP / "graphics/items/icons/fresh_water.png",
        "play": DECOMP / "graphics/items/icons/poke_doll.png",
    }
    for name, path in home_icons.items():
        if path.exists():
            write_asset(f"firered/ui/home_{name}.pkg",
                        normalized_home_icon(source_png(path)), manifest)
    # One complete top-down tall-grass tuft from Route 1, with its map ground
    # removed so the Home button keeps its own white background.
    write_asset("firered/ui/home_wild.pkg",
                normalized_home_icon(top_down_tall_grass_icon()), manifest)
    write_asset("firered/ui/home_battle.pkg",
                normalized_home_icon(source_png(DECOMP / "graphics/items/icons/vs_seeker.png")), manifest)
    home_effects = home_move_effect_atlas()
    for type_index in range(18):
        write_asset(
            f"firered/ui/home_move_effect_{type_index:02}.pkg",
            home_effects.crop((type_index * 18, 0,
                               (type_index + 1) * 18, 6 * 18)),
            manifest,
        )
    tower_home,tower_facade=battle_tower_assets()
    write_asset("emerald/battle_tower/home_icon.pkg",tower_home,manifest)
    write_asset("emerald/battle_tower/facade.pkg",tower_facade,manifest)
    indigo_home,indigo_facade,hoenn_league_home,hoenn_league_facade=pokemon_league_assets()
    write_asset("firered/pokemon_league/home_icon.pkg",indigo_home,manifest)
    write_asset("firered/pokemon_league/facade.pkg",indigo_facade,manifest)
    write_asset("emerald/pokemon_league/home_icon.pkg",hoenn_league_home,manifest)
    write_asset("emerald/pokemon_league/facade.pkg",hoenn_league_facade,manifest)
    write_asset(
        "firered/pokemon_league/victory_bg.pkg",
        hall_of_fame_victory_background(DECOMP, "PokemonLeague_HallOfFame"),
        manifest,
    )
    write_asset(
        "emerald/pokemon_league/victory_bg.pkg",
        hall_of_fame_victory_background(EMERALD, "EverGrandeCity_HallOfFame"),
        manifest,
    )
    write_asset("firered/items/mart_tm_hm.pkg",
                source_png(DECOMP / "graphics/items/icons/tm_hm.png"), manifest)
    # The physical CYD is mounted in landscape with the panel's native scan
    # direction opposite to this bespoke icon's design coordinate system.
    # Other assets come from GBA tilemaps and are already oriented by their
    # source; rotate only the custom gear so its Pokeball centre is upright.
    write_asset("firered/ui/home_settings.pkg",
                normalized_home_icon(home_settings_icon().rotate(180)), manifest)
    write_asset("firered/ui/home_pokecenter.pkg",
                home_pokecenter_icon(), manifest)
    pokecenter_scenes = pokemon_center_scenes()
    if len(pokecenter_scenes) != 10 or len({scene.tobytes() for scene in pokecenter_scenes}) != 10:
        raise RuntimeError("Pokemon Center scene set must contain ten distinct views")
    for index, scene in enumerate(pokecenter_scenes):
        write_asset(f"pokecenter/scene_{index:02}.pkg", scene, manifest)
    write_asset("firered/ui/home_route.pkg", home_route_background(), manifest)
    background_images = build_background_layers()
    write_firmware_metadata()
    for index, background in enumerate(BACKGROUNDS):
        write_asset(f"firered/ui/home_background_{index:02}.pkg",
                    background_images[background.key].composite, manifest)
        write_asset(f"firered/ui/home_background_base_{index:02}.pkg",
                    background_images[background.key].base, manifest)
        write_asset(f"firered/ui/home_background_foreground_{index:02}.pkg",
                    background_images[background.key].foreground, manifest)
    # Home visitors are actual FireRed overworld sprites. The first downward-
    # facing cel is the same 16x32 frame the original map engine displays
    # while the character is standing still.
    for index, name in enumerate(("youngster", "lass", "old_man_1", "policeman")):
        sheet = source_png(DECOMP / "graphics/object_events/pics/people" / f"{name}.png")
        write_asset(f"firered/ui/home_npc_{index}.pkg", sheet.crop((0, 0, 16, 32)), manifest)
    write_asset("firered/ui/box_icon_v2.pkg",
                normalized_home_icon(home_box_icon()), manifest)
    write_asset("firered/ui/mart_icon.pkg", home_mart_icon(), manifest)
    egg_folder=DECOMP/"graphics/pokemon/egg"
    egg_icon=source_png(egg_folder/"icon.png")
    write_asset("firered/ui/egg_icon.pkg",
                normalized_home_icon(egg_icon.crop((0,0,32,32)), 18, 17), manifest)
    write_asset("firered/ui/egg.pkg",source_png(egg_folder/"front.png"),manifest)
    # Dedicated 64x64 Egg Hatch sequence. FireRed stores the initial egg as a
    # 16x16 animation cel, the cracked/open egg as 32x32 cels, and the three
    # breaking stages vertically in one 32x96 sheet. Exporting a uniform size
    # keeps the firmware animation centred and lets it preload every frame.
    hatch_folder = DECOMP / "graphics/battle_anims/sprites"
    hatch_fresh = source_png(hatch_folder / "fresh_egg.png")
    hatch_cracked = source_png(hatch_folder / "cracked_egg.png")
    hatch_breaking = source_png(hatch_folder / "breaking_egg.png")
    hatch_shell = source_png(hatch_folder / "hatched_egg.png")
    nearest = Image.Resampling.NEAREST
    write_asset("firered/ui/egg_hatch_fresh.pkg",
                hatch_fresh.crop((0, 0, 16, 16)).resize((64, 64), nearest), manifest)
    write_asset("firered/ui/egg_hatch_cracked.pkg",
                hatch_cracked.crop((0, 0, 32, 32)).resize((64, 64), nearest), manifest)
    for frame in range(3):
        write_asset(f"firered/ui/egg_hatch_breaking_{frame}.pkg",
                    hatch_breaking.crop((0, frame * 32, 32, (frame + 1) * 32)).resize((64, 64), nearest),
                    manifest)
    write_asset("firered/ui/egg_hatch_shell.pkg",
                hatch_shell.crop((0, 0, 32, 32)).resize((64, 64), nearest), manifest)
    # Compose the original FireRed Summary-screen tilemaps into complete
    # scene backgrounds. The source uses magenta as its transparent tile
    # colour, so key it out before layering the per-page frames over the
    # shared header/card layout.
    summary = DECOMP / "graphics/summary_screen"
    def summary_tilemap(name):
        image = compose_tilemap(summary / "bg.png", summary / f"{name}.bin", 32)
        keyed = []
        for red, green, blue, alpha in image.getdata():
            keyed.append((red, green, blue, 0 if (red, green, blue) == (255, 0, 255) else alpha))
        image.putdata(keyed)
        return image
    summary_base = summary_tilemap("moves_info_page")
    for name, frame in (("info", "page_info"), ("skills", "page_skills"), ("moves", "page_moves")):
        page = summary_base.copy()
        page.alpha_composite(summary_tilemap(frame))
        # Preserve the original 3:2 handheld aspect ratio; the CYD uses the
        # remaining 27px as a native touch footer instead of stretching the
        # FireRed UI vertically.
        page = page.crop((0, 0, 240, 160)).resize((320, 213), Image.Resampling.NEAREST)
        write_asset(f"firered/summary_screen/{name}_page.pkg", page, manifest)
    battle_terrain_audit_images: list[tuple[str, Image.Image]] = []
    battle_move_bg_audit_images: list[tuple[str, Image.Image]] = []
    for terrain in BATTLE_TERRAINS:
        folder=DECOMP/"graphics/battle_terrain"/terrain
        palette = folder / ("1.pal" if terrain == "indoor" else "terrain.pal")
        composed=compose_battle_terrain(folder/"terrain.png",folder/"terrain.bin",palette)
        viewport=battle_terrain_viewport(composed)
        battle_terrain_audit_images.append((terrain, viewport.copy()))
        write_asset(f"firered/battle_terrain/{terrain}.pkg",viewport,manifest)
    for asset_name, tile_folder, palette_folder, palette_name in BATTLE_TERRAIN_VARIANTS:
        folder = DECOMP / "graphics/battle_terrain" / tile_folder
        palette = DECOMP / "graphics/battle_terrain" / palette_folder / f"{palette_name}.pal"
        composed = compose_battle_terrain(folder / "terrain.png", folder / "terrain.bin", palette)
        viewport = battle_terrain_viewport(composed)
        battle_terrain_audit_images.append((asset_name, viewport.copy()))
        write_asset(f"firered/battle_terrain/{asset_name}.pkg", viewport, manifest)
    for background_id, (gfx_name, map_name, palette_name) in enumerate(
            BATTLE_ANIMATION_BACKGROUNDS):
        tiled_indices, animation_palette = battle_animation_indexed_plane(
            gfx_name, map_name, palette_name)
        static_indices = tiled_indices.crop((0, 0, 240, 138)).resize(
            (320, 184), Image.Resampling.NEAREST)
        background = indexed_rgba(static_indices, animation_palette)
        battle_move_bg_audit_images.append((f"bg_{background_id:02}",
                                            background.copy()))
        write_indexed_asset(
            f"firered/battle_anims/backgrounds/bg_{background_id:02}.pkg",
            static_indices, animation_palette, manifest)
        # Fissure is a 512x256 BG whose complete right screenblock is one
        # solid palette entry.  Store only the authored left block; the
        # renderer reconstructs the solid half virtually while sampling the
        # original 512px coordinate space.  This is lossless and leaves room
        # for the resident battle terrain in the ESP32's 64 KiB scene bank.
        tiled_runtime = (tiled_indices.crop((0, 0, 256, 256))
                         if background_id == 21 else tiled_indices)
        write_indexed_asset(
            f"firered/battle_anims/backgrounds/tiled_bg_{background_id:02}.pkg",
            tiled_runtime, animation_palette, manifest)
        runtime_zero_offset = indexed_rgba(
            tiled_indices.crop((0, 0, 240, 138)).resize(
                (320, 184), Image.Resampling.NEAREST), animation_palette)
        if runtime_zero_offset.tobytes() != background.tobytes():
            raise RuntimeError(
                f"Static/tiled camera mismatch: bg_{background_id:02}")
    for name, gfx_relative, map_relative in BATTLE_ANIMATION_TASK_BACKGROUNDS:
        task_indices, task_palette, transparent = \
            battle_animation_task_indexed_plane(gfx_relative, map_relative)
        write_indexed_asset(
            f"firered/battle_anims/backgrounds/task_{name}.pkg",
            task_indices, task_palette, manifest,
            transparent_index=transparent)
    # Substitute is not one of the move script's SpriteTemplates: the visual
    # task creates the battle substitute directly from these two canonical
    # FireRed sheets. Keep the front/back distinction intact.
    for side in ("front", "back"):
        source_name = "substitute_back.png" if side == "back" else "substitute.png"
        write_asset(
            f"firered/battle_anims/sprites/substitute_{side}.pkg",
            source_png(DECOMP / "graphics/battle_anims/sprites" / source_name),
            manifest)
    battle_background_audit = {
        "arenaSize": [320, 184],
        "sourceViewport": [0, 0, 240, 138],
        "nearestScale": [4, 3],
        "terrains": [],
        "moveBackgrounds": [],
    }
    for name, image in battle_terrain_audit_images:
        if image.size != (320, 184):
            raise RuntimeError(f"Battle terrain has wrong size: {name} {image.size}")
        if image.convert("RGBA").getextrema()[3] != (255, 255):
            raise RuntimeError(f"Battle terrain is not fully opaque: {name}")
        battle_background_audit["terrains"].append({
            "name": name,
            "sha256": hashlib.sha256(image.tobytes()).hexdigest(),
        })
    for name, image in battle_move_bg_audit_images:
        if image.size != (320, 184):
            raise RuntimeError(f"Move background has wrong size: {name} {image.size}")
        if image.convert("RGBA").getextrema()[3] != (255, 255):
            raise RuntimeError(f"Move background is not fully opaque: {name}")
        battle_background_audit["moveBackgrounds"].append({
            "name": name,
            "sha256": hashlib.sha256(image.tobytes()).hexdigest(),
        })
    generated = ROOT / ".generated"
    generated.mkdir(parents=True, exist_ok=True)
    (generated / "battle_background_audit.json").write_text(
        json.dumps(battle_background_audit, indent=2), encoding="utf-8")
    # A deterministic contact sheet makes future platform/crop regressions
    # visible without needing to discover them on the physical TFT.
    all_battle_backgrounds = battle_terrain_audit_images + battle_move_bg_audit_images
    contact_columns = 4
    contact_rows = (len(all_battle_backgrounds) + contact_columns - 1) // contact_columns
    contact = Image.new("RGBA", (contact_columns * 320, contact_rows * 200),
                        (24, 24, 24, 255))
    label_font = ImageFont.load_default()
    contact_draw = ImageDraw.Draw(contact)
    for index, (name, image) in enumerate(all_battle_backgrounds):
        x = (index % contact_columns) * 320
        y = (index // contact_columns) * 200
        contact.alpha_composite(image, (x, y))
        contact_draw.text((x + 4, y + 185), name, fill=(255, 255, 255, 255),
                          font=label_font)
    contact.save(generated / "battle_background_contact.png")
    for side in ("player", "opponent"):
        surf_indices, surf_palette = surf_wave_indexed_plane(side)
        write_indexed_asset(f"firered/battle_anims/surf_{side}.pkg",
                            surf_indices, surf_palette, manifest)
        muddy_indices, muddy_palette = surf_wave_indexed_plane(side, True)
        write_indexed_asset(f"firered/battle_anims/muddy_water_{side}.pkg",
                            muddy_indices, muddy_palette, manifest)
    transition_folder=DECOMP/"graphics/battle_transitions"
    big_pokeball=compose_tilemap(transition_folder/"big_pokeball.png",
                                transition_folder/"big_pokeball_tilemap.bin",30)
    # The original transition is 240x160. Scale with nearest-neighbour and
    # centre-crop it to the CYD's 320x240 landscape display.
    big_pokeball=big_pokeball.resize((360,240),Image.Resampling.NEAREST).crop((20,0,340,240))
    write_asset("firered/ui/wild_transition.pkg",big_pokeball,manifest)

    # New-game and regional hand-off presentation.  Both scenes are built
    # exclusively from FireRed art: Oak's native speech background/picture,
    # the real Pallet laboratory tilemap and the overworld item-ball sprite.
    oak_speech = DECOMP / "graphics/oak_speech"
    oak_background = compose_tilemap(oak_speech / "oak_speech_bg.png",
                                     oak_speech / "oak_speech_bg.bin", 32)
    oak_scene = oak_background.crop((0, 0, 240, 160)).resize(
        (320, 160), Image.Resampling.NEAREST)
    oak_picture = source_png(oak_speech / "oak" / "pic.png").resize(
        (80, 120), Image.Resampling.NEAREST)
    oak_scene.alpha_composite(oak_picture, (120, 32))
    write_asset("firered/starter/oak_intro.pkg", oak_scene, manifest)

    laboratory = render_overworld_map("PalletTown_ProfessorOaksLab", "lab",
                                      13, 14, "building")
    # Zoom the upper laboratory into the 320x160 selection stage.  The real
    # central table is enlarged once more over that view, hiding its smaller
    # map instance and giving the three choices enough touch/display space.
    laboratory_scene = laboratory.resize((320, 344), Image.Resampling.NEAREST).crop(
        (0, 0, 320, 160))
    starter_table = laboratory.crop((128, 64, 176, 96)).resize(
        (216, 96), Image.Resampling.NEAREST)
    laboratory_scene.alpha_composite(starter_table, (52, 48))
    item_ball = source_png(DECOMP / "graphics/object_events/pics/misc/item_ball.png").resize(
        (24, 24), Image.Resampling.NEAREST)
    for center_x in (80, 160, 240):
        laboratory_scene.alpha_composite(item_ball, (center_x - 12, 111))
    write_asset("firered/starter/oaks_lab.pkg", laboratory_scene, manifest)

    # FireRed's link-trade scene. Keep the original sprites, but downscale the
    # two large hardware/signal layers so the complete cinematic working set
    # fits the ESP32's small preloaded-asset arena without opening SD files
    # while the TFT owns the shared SPI bus.
    trade_folder = DECOMP / "graphics/trade"
    write_asset("firered/trade/gba.pkg",
                source_png(trade_folder / "gba_affine.png").resize((64, 40), Image.Resampling.NEAREST), manifest)
    write_asset("firered/trade/wireless_signal.pkg",
                source_png(trade_folder / "wireless_signal.png").resize((64, 48), Image.Resampling.NEAREST), manifest)
    write_asset("firered/trade/link_mon_glow.pkg",
                source_png(trade_folder / "link_mon_glow.png"), manifest)
    trade_ball = source_png(trade_folder / "pokeball.png")
    for frame in range(12):
        write_asset(f"firered/trade/pokeball_{frame:02}.pkg",
                    trade_ball.crop((0, frame * 16, 16, frame * 16 + 16)), manifest)
    evolution_folder = DECOMP / "graphics/evolution_scene"
    evolution_map = compose_tilemap(evolution_folder / "bg.png",
                                    evolution_folder / "bg.bin", 32)
    # The GBA scene is 240x160. Preserve its pixel-art composition while
    # adapting it to the 320x240 landscape panel; the bottom 40 pixels are
    # reserved for the original-style message window in firmware.
    # The source tilemap is 256 px wide and its vortex is centred at x=128.
    # FireRed displays a centred 240 px viewport; cropping from x=0 shifted
    # the vortex ten panel pixels to the right after landscape scaling while
    # the Pokemon remained at x=160. Preserve the original art, but centre
    # its viewport before resizing it for the Pokegochi screen.
    evolution_scene = evolution_map.crop((8, 0, 248, 160)).resize(
        (320, 213), Image.Resampling.NEAREST)
    write_asset("firered/evolution_scene/scene.pkg", evolution_scene, manifest)
    dex_reference = remove_connected_light_background(
        Image.open(ROOT / ".downloads/pokedex-icon-reference.jpg"))
    write_asset("firered/ui/pokedex_icon.pkg",
                normalized_home_icon(dex_reference), manifest)
    box_source=source_png(DECOMP/"graphics/pokemon_storage/menu.png")
    write_asset("firered/ui/box_icon.pkg",box_source.resize((20,20),Image.Resampling.NEAREST),manifest)
    badge_sheet=source_png(DECOMP/"graphics/trainer_card/badges.png")
    for index in range(8):
        badge=badge_sheet.crop((index*16,0,(index+1)*16,16))
        write_asset(f"firered/trainer_card/badge_{index}.pkg",badge,manifest)
        write_asset(f"firered/ui/home_badge_{index}.pkg",
                    normalized_home_icon(badge),manifest)
    hoenn_badges=source_png(EMERALD/"graphics/trainer_card/badges.png")
    for index in range(8):
        badge=hoenn_badges.crop((index*16,0,(index+1)*16,16))
        write_asset(f"firered/trainer_card/badge_{16+index}.pkg",badge,manifest)
        write_asset(f"firered/ui/home_badge_{16+index}.pkg",
                    normalized_home_icon(badge),manifest)
    johto_sheet=Image.open(CRYSTAL/"gfx/trainer_card/badges.png").convert("L")
    for index in range(8):
        gray=johto_sheet.crop((0,index*16,16,(index+1)*16))
        badge=Image.new("RGBA",(16,16))
        badge.putdata([(248,224,112,0) if value>=250 else
                       (248,224,112,255) if value>=160 else
                       (120,152,184,255) if value>=80 else (48,56,64,255)
                       for value in gray.get_flattened_data()])
        write_asset(f"firered/trainer_card/badge_{8+index}.pkg",badge,manifest)
        write_asset(f"firered/ui/home_badge_{8+index}.pkg",
                    normalized_home_icon(badge),manifest)
    shop=compose_tilemap(DECOMP/"graphics/shop_menu/shop_menu.png",DECOMP/"graphics/shop_menu/shop_tilemap.bin",32)
    write_asset("firered/shop/background.pkg",shop.crop((0,0,240,160)).resize((320,240),Image.Resampling.NEAREST),manifest)

    animation_audit=json.loads((ROOT/".generated/battle_animation_audit.json").read_text(encoding="utf-8"))

    for folder in ("graphics/battle_anims/sprites", "graphics/battle_interface",
                   "graphics/pokemon_storage", "graphics/pokedex", "graphics/evolution_scene"):
        for path in (DECOMP / folder).glob("*.png"):
            image = source_png(path)
            base = f"firered/{folder.removeprefix('graphics/')}/{path.stem}"
            write_asset(f"{base}.pkg", image, manifest)
            if folder == "graphics/battle_anims/sprites":
                frame_size = image.width if image.height >= image.width and image.height % image.width == 0 else image.height
                frame_count = max(1, min(32, image.height // frame_size))
                for frame in range(frame_count):
                    write_asset(f"{base}_{frame:02}.pkg",
                                image.crop((0, frame * frame_size, image.width, (frame + 1) * frame_size)), manifest)

    # Overwrite the heuristic raw-PNG slices with exact tag-specific OAM cels
    # and palettes. This is the production move-animation catalog.
    battle_sprite_audit = export_exact_battle_animation_assets(animation_audit, manifest)

    # The PC hand cursor and its two scroll arrows are sprite sheets in the
    # original game.  Export their individual frames so the firmware can put
    # the exact FireRed pointer over a selected icon without drawing all
    # orientations from the sheet at once.
    storage = DECOMP / "graphics/pokemon_storage"
    cursor = source_png(storage / "cursor.png")
    for frame in range(cursor.height // 32):
        write_asset(f"firered/pokemon_storage/cursor_{frame:02}.pkg",
                    cursor.crop((0, frame * 32, cursor.width, (frame + 1) * 32)), manifest)
    box_arrows = source_png(storage / "box_scroll_arrow.png")
    for frame in range(box_arrows.height // 16):
        write_asset(f"firered/pokemon_storage/box_scroll_arrow_{frame:02}.pkg",
                    box_arrows.crop((0, frame * 16, box_arrows.width, (frame + 1) * 16)), manifest)
    # These tilemaps are the actual FireRed Party menu and its two slot
    # states. Their tiles start at character-base tile 256, unlike the other
    # exported interface maps.
    write_asset("firered/pokemon_storage/party_menu.pkg",
                compose_tilemap(storage / "menu.png", storage / "party_menu.bin", 12, 256), manifest)
    write_asset("firered/pokemon_storage/party_slot_empty.pkg",
                compose_tilemap(storage / "menu.png", storage / "party_slot_empty.bin", 4, 256), manifest)
    write_asset("firered/pokemon_storage/party_slot_filled.pkg",
                compose_tilemap(storage / "menu.png", storage / "party_slot_filled.bin", 4, 256), manifest)

    # FireRed Bag: keep the original background tile and pocket frames.  The
    # tiny home icon is a nearest-neighbour adaptation of the original sprite.
    bag = source_png(DECOMP / "graphics/interface/bag_male.png")
    write_asset("firered/item_menu/bg.pkg", source_png(DECOMP / "graphics/item_menu/bg.png"), manifest)
    for frame in range(bag.height // 64):
        pocket = bag.crop((0, frame * 64, 64, (frame + 1) * 64))
        write_asset(f"firered/item_menu/bag_{frame}.pkg", pocket, manifest)
        if frame == 0:
            write_asset("firered/item_menu/bag_icon.pkg",
                        normalized_home_icon(pocket), manifest)

    required=[*(f"pokemon/front/{i:03}.pkg" for i in range(1,387)),*(f"pokemon/back/{i:03}.pkg" for i in range(1,387)),
      *(f"pokemon/icons/{i:03}_{frame}.pkg" for i in range(1,387) for frame in range(2)),"firered/item_menu/bag_icon.pkg",
      *(f"pokemon/shiny/front/{i:03}.pkg" for i in range(1,387)),*(f"pokemon/shiny/back/{i:03}.pkg" for i in range(1,387)),
      *(f"pokemon/shiny/icons/{i:03}_{frame}.pkg" for i in range(1,387) for frame in range(2)),
      *(f"pokemon/evolution/{tone}/{i:03}.pkg" for tone in ("dark", "light") for i in range(1,387)),
      "firered/evolution_scene/scene.pkg","firered/evolution_scene/sparkle.pkg",
      "firered/ui/pokedex_icon.pkg","firered/ui/box_icon.pkg",
      *(f"firered/battle_terrain/{terrain}.pkg" for terrain in BATTLE_TERRAINS),
      *(f"firered/battle_terrain/{asset_name}.pkg"
        for asset_name, *_ in BATTLE_TERRAIN_VARIANTS),
      *(f"firered/battle_anims/backgrounds/bg_{background_id:02}.pkg"
        for background_id in range(len(BATTLE_ANIMATION_BACKGROUNDS))),
      *(f"firered/battle_anims/backgrounds/tiled_bg_{background_id:02}.pkg"
        for background_id in range(len(BATTLE_ANIMATION_BACKGROUNDS))),
      *(f"firered/battle_anims/backgrounds/task_{name}.pkg"
        for name, *_ in BATTLE_ANIMATION_TASK_BACKGROUNDS),
      "firered/battle_anims/sprites/substitute_front.pkg",
      "firered/battle_anims/sprites/substitute_back.pkg",
      "firered/battle_anims/surf_player.pkg","firered/battle_anims/surf_opponent.pkg",
      "firered/battle_anims/muddy_water_player.pkg","firered/battle_anims/muddy_water_opponent.pkg",
      "firered/ui/home_route.pkg","firered/ui/box_icon_v2.pkg","firered/ui/mart_icon.pkg",
      "firered/ui/home_settings.pkg","firered/ui/home_pokecenter.pkg",
      *(f"pokecenter/scene_{index:02}.pkg" for index in range(len(POKECENTER_SCENES))),
      "firered/ui/home_wild.pkg",
      "firered/ui/home_battle.pkg",
      *(f"firered/ui/home_move_effect_{type_index:02}.pkg"
        for type_index in range(18)),
      "firered/ui/wild_transition.pkg",
      "emerald/battle_tower/home_icon.pkg","emerald/battle_tower/facade.pkg",
      "firered/pokemon_league/home_icon.pkg","firered/pokemon_league/facade.pkg",
      "emerald/pokemon_league/home_icon.pkg","emerald/pokemon_league/facade.pkg",
      "firered/pokemon_league/victory_bg.pkg","emerald/pokemon_league/victory_bg.pkg",
      *(f"firered/ui/home_badge_{i}.pkg" for i in range(24)),
      "firered/starter/oak_intro.pkg","firered/starter/oaks_lab.pkg",
      "firered/trade/gba.pkg","firered/trade/wireless_signal.pkg","firered/trade/link_mon_glow.pkg",
      *(f"firered/trade/pokeball_{frame:02}.pkg" for frame in range(12)),
      *(f"firered/ui/home_background_{i:02}.pkg" for i in range(len(BACKGROUNDS))),
      *(f"firered/ui/home_background_base_{i:02}.pkg" for i in range(len(BACKGROUNDS))),
      *(f"firered/ui/home_background_foreground_{i:02}.pkg" for i in range(len(BACKGROUNDS))),
      *(f"firered/ui/home_npc_{i}.pkg" for i in range(4)),
      "firered/ui/egg_icon.pkg","firered/ui/egg.pkg",
      "firered/ui/egg_hatch_fresh.pkg","firered/ui/egg_hatch_cracked.pkg",
      *(f"firered/ui/egg_hatch_breaking_{frame}.pkg" for frame in range(3)),
      "firered/ui/egg_hatch_shell.pkg",
      *(f"firered/summary_screen/{name}_page.pkg" for name in ("info", "skills", "moves")),
      "firered/pokemon_storage/party_menu.pkg","firered/pokemon_storage/party_slot_empty.pkg",
      "firered/pokemon_storage/party_slot_filled.pkg",
      *(f"trainers/{asset}.pkg" for asset in (
        "elite_four_lorelei_front_pic","elite_four_bruno_front_pic","elite_four_agatha_front_pic",
        "elite_four_lance_front_pic","champion_rival_front_pic","johto_will","johto_koga",
        "johto_bruno","johto_karen","johto_champion","emerald_elite_four_sidney",
        "emerald_elite_four_phoebe","emerald_elite_four_glacia","emerald_elite_four_drake",
        "emerald_champion_wallace","red_front_pic")),
      "firered/shop/background.pkg","firered/items/poke_ball.pkg","firered/items/great_ball.pkg","firered/items/ultra_ball.pkg",
      *(f"firered/battle_items/ball_{ball}_{frame:02}.pkg" for ball in ("poke_ball","great_ball","ultra_ball","master_ball") for frame in range(3)),
      *(f"firered/items/mart_{name}.pkg" for name in ("poke_ball","great_ball","ultra_ball","potion","large_potion","full_heal","antidote","status_heal","battle_stat_item")),
      *(f"firered/items/held_{index:02}.pkg" for index in range(1,39)),
      "firered/items/lucky_egg.pkg",
      "emerald/berry_garden/background.pkg",
      *(f"emerald/berry_garden/sprout_{frame}.pkg" for frame in range(2)),
      *(f"emerald/berry_garden/tree_{berry:02}_stage_{stage}_{frame}.pkg"
        for berry in range(1,10) for stage in range(2,5) for frame in range(2)),
      *(f"pokemon/forms/mega/{species:03}/{variant}/{shiny}{view}.pkg"
        for species, _folder, variant in MEGA_FORMS for shiny in ("","shiny/")
        for view in ("front","back")),
      *(f"pokemon/forms/mega/{species:03}/{variant}/{shiny}icons/{frame}.pkg"
        for species, _folder, variant in MEGA_FORMS for shiny in ("","shiny/")
        for frame in range(2)),
      *(f"pokemon/forms/castform/{form}/{shiny}{view}.pkg"
        for form in range(4) for shiny in ("", "shiny/")
        for view in ("front", "back")),
      *(f"firered/trainer_card/badge_{i}.pkg" for i in range(24))]
    # Validate every frame the runtime can request, not merely frame zero.
    # A partially copied/exported atlas previously passed boot and then made
    # only particular moves disappear halfway through their animation.
    runtime_animation_frames = [
        f"firered/battle_anims/sprites/{key}_{frame:02}.pkg"
        for key, spec in animation_audit["visualAssets"].items()
        for frame in range(spec["frameCount"])
    ]
    required.extend(runtime_animation_frames)
    missing=[name for name in required if name not in manifest]
    trainer_count=sum(1 for name in manifest if name.startswith("trainers/"))
    if missing or trainer_count<100:raise RuntimeError(f"Asset audit failed: {len(missing)} missing, {trainer_count} trainers")
    # Firmware checks this tiny marker before enabling START.  A visually
    # plausible but older card can contain the core Home assets while missing
    # newer per-frame battle sprites; without an explicit version that error
    # surfaced only when the affected move was used.
    (OUTPUT / "pack_version.txt").write_text(f"{ASSET_PACK_VERSION}\n", encoding="ascii")
    (OUTPUT / "animation_catalog.txt").write_text(
        f"{animation_audit['catalogHash']}\n", encoding="ascii")
    (OUTPUT / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    (OUTPUT / "asset_audit.json").write_text(json.dumps({
        "assets": len(manifest), "trainers": trainer_count,
        "required": len(required), "missing": missing,
        "runtimeAnimationFrames": len(runtime_animation_frames),
        "battleSprites": battle_sprite_audit}, indent=2), encoding="utf-8")
    write_asset_archive(manifest)
    print(f"Built {len(manifest)} local assets in {OUTPUT}")


if __name__ == "__main__":
    if "--battle-tower-icon-only" in sys.argv:
        manifest_path = OUTPUT / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        write_asset("emerald/battle_tower/home_icon.pkg",
                    battle_tower_home_icon(), manifest)
        (OUTPUT / "pack_version.txt").write_text(
            f"{ASSET_PACK_VERSION}\n", encoding="ascii")
        manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
        patch_asset_archive("emerald/battle_tower/home_icon.pkg")
        print("Rebuilt the transparent Battle Tower icon and asset archive")
    elif "--archive-only" in sys.argv:
        write_asset_archive(json.loads((OUTPUT / "manifest.json").read_text(encoding="utf-8")))
    else:
        main()
