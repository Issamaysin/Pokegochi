"""Render 320x240 UI previews from the original FireRed graphics assets."""

from __future__ import annotations

import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw

from render_firered_mockup import (
    BACK_SPRITE_TABLE,
    EXPECTED_SHA1,
    extract_battle_sprite,
    extract_icon,
)
import hashlib


ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / ".downloads" / "fire-red-assets"
ROM = ROOT / "rom" / "extracted" / "Pokemon - FireRed Version (USA, Europe).gba"
NEAREST = Image.Resampling.NEAREST


def asset(name: str) -> Path:
    return ASSETS / name.replace("/", "__")


def transparent_palette_image(path: Path, transparent_index: int = 0) -> Image.Image:
    source = Image.open(path)
    rgba = source.convert("RGBA")
    alpha = Image.new("L", source.size, 255)
    alpha.putdata([0 if value == transparent_index else 255 for value in source.get_flattened_data()])
    rgba.putalpha(alpha)
    return rgba


def render_tilemap(tile_path: Path, map_path: Path) -> Image.Image:
    tiles = transparent_palette_image(tile_path)
    entries = struct.unpack("<2048H", map_path.read_bytes())
    output = Image.new("RGBA", (512, 256))
    for y in range(32):
        for x in range(64):
            # Two horizontal GBA screenblocks, each 32x32 tiles.
            value = entries[(x // 32) * 1024 + y * 32 + (x % 32)]
            tile_number = value & 0x03FF
            tile = tiles.crop(((tile_number % 8) * 8, (tile_number // 8) * 8,
                               (tile_number % 8 + 1) * 8, (tile_number // 8 + 1) * 8))
            if value & 0x0400:
                tile = tile.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            if value & 0x0800:
                tile = tile.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
            output.alpha_composite(tile, (x * 8, y * 8))
    return output


def indexed_sprite(indices: list[int], palette: list[tuple[int, int, int]], size: int = 64) -> Image.Image:
    output = Image.new("RGBA", (size, size))
    pixels = []
    for index in indices:
        pixels.append((*palette[index], 0 if index == 0 else 255))
    output.putdata(pixels)
    return output


FONT_CODES = {" ": 0x00, **{str(i): 0xA1 + i for i in range(10)}}
FONT_CODES.update({chr(ord("A") + i): 0xBB + i for i in range(26)})
FONT_CODES.update({"!": 0xAB, "?": 0xAC, ".": 0xAD, "-": 0xAE, ":": 0xF0, "/": 0xBA})


class FireRedFont:
    def __init__(self) -> None:
        self.sheet = Image.open(asset("graphics/fonts/latin_normal.png"))

    def glyph(self, character: str, light: bool = False) -> Image.Image:
        code = FONT_CODES.get(character.upper(), 0)
        tile = self.sheet.crop(((code % 16) * 16, (code // 16) * 16,
                                (code % 16 + 1) * 16, (code // 16 + 1) * 16))
        # Index 3 is the white backing of each glyph cell, not part of the
        # letter. FireRed swaps the remaining two ink colours per window.
        pixels = []
        for value in tile.get_flattened_data():
            if value in (0, 3):
                pixels.append((0, 0, 0, 0))
            elif light:
                pixels.append((255, 255, 255, 255) if value == 1 else (152, 184, 208, 255))
            else:
                pixels.append((56, 56, 56, 255) if value == 1 else (168, 168, 152, 255))
        rgba = Image.new("RGBA", tile.size)
        rgba.putdata(pixels)
        return rgba

    def text(self, canvas: Image.Image, xy: tuple[int, int], text: str,
             max_width: int | None = None, light: bool = False) -> None:
        x, y = xy
        origin = x
        for character in text:
            if character == "\n":
                x, y = origin, y + 17
                continue
            glyph = self.glyph(character, light)
            bbox = glyph.getbbox()
            width = max(4, (bbox[2] if bbox else 4) - (bbox[0] if bbox else 0) + 1)
            if max_width and x + width > origin + max_width:
                x, y = origin, y + 17
            canvas.alpha_composite(glyph, (x, y))
            x += width


def paste_masked(canvas: Image.Image, image: Image.Image, xy: tuple[int, int], scale: int = 1) -> None:
    if scale != 1:
        image = image.resize((image.width * scale, image.height * scale), NEAREST)
    canvas.alpha_composite(image, xy)


def item_icon(name: str) -> Image.Image:
    return transparent_palette_image(asset(f"graphics/items/icons/{name}.png"))


def ui_crop(name: str, box: tuple[int, int, int, int], size: tuple[int, int] = (24, 24)) -> Image.Image:
    image = Image.open(asset(name)).convert("RGBA").crop(box)
    return image.resize(size, NEAREST)


def pokemon_icon(rom: bytes, species: int, size: int = 24) -> Image.Image:
    indices, palette = extract_icon(rom, species, 0)
    return indexed_sprite(indices, palette, 32).resize((size, size), NEAREST)


def fire_red_button(canvas: Image.Image, xy: tuple[int, int], icon: Image.Image) -> None:
    x, y = xy
    draw = ImageDraw.Draw(canvas)
    draw.rounded_rectangle((x, y, x + 67, y + 47), radius=4, fill=(65, 65, 65), outline=(32, 32, 32), width=1)
    draw.rounded_rectangle((x + 3, y + 3, x + 64, y + 44), radius=3, fill=(255, 255, 222), outline=(222, 181, 82), width=3)
    paste_masked(canvas, icon, (x + 22, y + 12), scale=1)


def small_fire_red_button(canvas: Image.Image, xy: tuple[int, int], icon: Image.Image) -> None:
    x, y = xy
    draw = ImageDraw.Draw(canvas)
    draw.rounded_rectangle((x, y, x + 45, y + 47), radius=4, fill=(65, 65, 65), outline=(32, 32, 32), width=1)
    draw.rounded_rectangle((x + 3, y + 3, x + 42, y + 44), radius=3, fill=(255, 255, 222), outline=(222, 181, 82), width=3)
    paste_masked(canvas, icon, (x + 11, y + 12))


def render_home(output: Path) -> None:
    rom = ROM.read_bytes()
    if hashlib.sha1(rom).hexdigest().upper() != EXPECTED_SHA1:
        raise ValueError("Unexpected FireRed ROM")
    font = FireRedFont()
    terrain = render_tilemap(
        asset("graphics/battle_terrain/grass/terrain.png"),
        asset("graphics/battle_terrain/grass/terrain.bin"),
    )
    canvas = Image.new("RGBA", (320, 240), (230, 255, 230, 255))
    canvas.alpha_composite(terrain.crop((0, 0, 320, 240)), (0, 0))

    draw = ImageDraw.Draw(canvas)
    draw.rounded_rectangle((6, 7, 139, 57), radius=5, fill=(65, 65, 65))
    draw.rounded_rectangle((3, 4, 135, 53), radius=5, fill=(255, 255, 222), outline=(65, 65, 65), width=2)
    font.text(canvas, (12, 8), "BULBASAUR  LV5")
    draw.rounded_rectangle((12, 31, 33, 42), radius=3, fill=(222, 181, 82))
    font.text(canvas, (15, 29), "HP")
    draw.rectangle((35, 34, 124, 41), fill=(65, 65, 65))
    draw.rectangle((37, 36, 122, 39), fill=(82, 197, 115))

    draw.rounded_rectangle((151, 5, 226, 27), radius=4, fill=(255, 255, 222), outline=(65, 65, 65), width=2)
    font.text(canvas, (158, 8), "CHG 3/3")

    paste_masked(canvas, pokemon_icon(rom, 16, 58), (19, 124))
    paste_masked(canvas, pokemon_icon(rom, 1, 88), (77, 79))
    paste_masked(canvas, pokemon_icon(rom, 19, 58), (169, 124))

    # FireRed Start-menu silhouette: shadow, dark rim, pale list and cursor.
    draw.rectangle((236, 12, 318, 228), fill=(32, 32, 40))
    draw.rectangle((231, 7, 313, 223), fill=(65, 65, 78))
    draw.rectangle((235, 11, 309, 219), fill=(255, 255, 246))
    draw.rectangle((239, 15, 305, 215), outline=(184, 168, 184), width=2)

    # The PC/storage button is cut from the real Pokemon Storage menu. The
    # Pokedex button is a miniature made from the real Kanto dex UI tiles.
    box_icon = ui_crop("graphics/pokemon_storage/menu.png", (0, 16, 24, 40))
    dex_page = Image.open(asset("graphics/pokedex/mini_page.png")).convert("RGBA").resize((24, 14), NEAREST)
    dex_icon = Image.new("RGBA", (24, 24), (0, 0, 0, 0))
    dex_icon.alpha_composite(dex_page, (0, 6))
    dex_icon.alpha_composite(pokemon_icon(rom, 25, 13), (6, 1))

    icons = (
        item_icon("oran_berry"),
        item_icon("fresh_water"),
        item_icon("poke_doll"),
        item_icon("vs_seeker"),
        box_icon,
        dex_icon,
    )
    for index, icon in enumerate(icons):
        y = 20 + index * 32
        if index == 3:
            draw.polygon(((239, y + 9), (245, y + 13), (239, y + 17)), fill=(65, 65, 78))
        paste_masked(canvas, icon.resize((24, 24), NEAREST), (263, y + 3))
        if index < 5:
            draw.line((249, y + 30, 297, y + 30), fill=(224, 216, 208))

    output.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(output, optimize=True)


def render_battle(output: Path) -> None:
    rom = ROM.read_bytes()
    if hashlib.sha1(rom).hexdigest().upper() != EXPECTED_SHA1:
        raise ValueError("Unexpected FireRed ROM")
    font = FireRedFont()
    canvas = Image.new("RGBA", (320, 240), (230, 255, 230, 255))

    terrain = render_tilemap(
        asset("graphics/battle_terrain/grass/terrain.png"),
        asset("graphics/battle_terrain/grass/terrain.bin"),
    )
    # The ROM map is 512 px wide for horizontal battle scrolling. This crop
    # is the stationary singles-battle viewport, extended to the TFT width.
    # Use the two exact platform regions independently so the widescreen TFT
    # extension does not split the original 240 px battle map.
    canvas.alpha_composite(terrain.crop((144, 8, 320, 88)), (144, 10))
    canvas.alpha_composite(terrain.crop((0, 72, 176, 160)), (0, 72))

    wild_indices, wild_palette = extract_battle_sprite(rom, 19, 0x2350AC)
    own_indices, own_palette = extract_battle_sprite(rom, 1, BACK_SPRITE_TABLE)
    paste_masked(canvas, indexed_sprite(wild_indices, wild_palette), (224, 25))
    paste_masked(canvas, indexed_sprite(own_indices, own_palette), (38, 86))

    draw = ImageDraw.Draw(canvas)
    # Healthbox sprite sheets in FireRed are assembled from several OBJ tiles
    # at runtime. Recreate that composition with the original palette and
    # geometry rather than showing the raw sheet pieces.
    draw.rounded_rectangle((18, 24, 145, 66), radius=5, fill=(65, 65, 65))
    draw.polygon(((118, 66), (145, 66), (145, 76)), fill=(65, 65, 65))
    draw.rounded_rectangle((14, 20, 139, 61), radius=5, fill=(255, 255, 222), outline=(65, 65, 65), width=2)
    draw.rounded_rectangle((177, 108, 312, 153), radius=5, fill=(65, 65, 65))
    draw.polygon(((177, 108), (165, 99), (177, 128)), fill=(65, 65, 65))
    draw.rounded_rectangle((173, 103, 307, 148), radius=5, fill=(255, 255, 222), outline=(65, 65, 65), width=2)
    font.text(canvas, (22, 24), "RATTATA")
    font.text(canvas, (102, 24), "LV3")
    font.text(canvas, (181, 107), "BULBASAUR")
    font.text(canvas, (272, 107), "LV5")

    draw.rounded_rectangle((24, 42, 45, 53), radius=3, fill=(222, 181, 82))
    font.text(canvas, (27, 40), "HP")
    draw.rectangle((47, 45, 130, 52), fill=(65, 65, 65))
    draw.rectangle((49, 47, 128, 50), fill=(82, 197, 115))
    draw.rounded_rectangle((184, 126, 205, 137), radius=3, fill=(222, 181, 82))
    font.text(canvas, (187, 124), "HP")
    draw.rectangle((207, 129, 298, 136), fill=(65, 65, 65))
    draw.rectangle((209, 131, 296, 134), fill=(82, 197, 115))
    font.text(canvas, (257, 136), "20/20")

    # Original FireRed-style dialogue panel, resized only vertically to leave
    # a dedicated row for four large touch targets.
    draw.rounded_rectangle((5, 151, 314, 187), radius=5, fill=(65, 65, 65))
    draw.rounded_rectangle((8, 154, 311, 184), radius=4, fill=(222, 181, 82))
    draw.rounded_rectangle((12, 158, 307, 181), radius=2, fill=(49, 91, 116))
    font.text(canvas, (19, 162), "A WILD RATTATA APPEARED!", light=True)

    command_icons = (
        transparent_palette_image(asset("graphics/battle_anims/sprites/cross_impact.png")),
        item_icon("poke_ball"),
        pokemon_icon(rom, 1),
        item_icon("escape_rope"),
    )
    for index, icon in enumerate(command_icons):
        fire_red_button(canvas, (9 + index * 78, 190), icon)

    output.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(output, optimize=True)


if __name__ == "__main__":
    mode = sys.argv[2] if len(sys.argv) > 2 else "battle"
    destination = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "docs" / "mockups" / f"{mode}-fire-red-assets.png"
    if mode == "home":
        render_home(destination)
    elif mode == "battle":
        render_battle(destination)
    else:
        raise SystemExit("mode must be home or battle")
