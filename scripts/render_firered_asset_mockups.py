"""Render 320x240 UI previews from the original FireRed graphics assets."""

from __future__ import annotations

import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw

from render_firered_mockup import (
    BACK_SPRITE_TABLE,
    EXPECTED_SHA1,
    FRONT_SPRITE_TABLE,
    extract_battle_sprite,
    extract_icon,
)
import hashlib


ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / ".downloads" / "fire-red-assets"
DECOMP = ROOT / ".downloads" / "pokefirered-tree" / "pokefirered-master"
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


def render_fixed_tilemap(tile_path: Path, map_path: Path, columns: int) -> Image.Image:
    """Decode a regular GBA tilemap whose width is known by the caller."""
    tiles = Image.open(tile_path).convert("RGBA")
    entries = struct.unpack(f"<{map_path.stat().st_size // 2}H", map_path.read_bytes())
    rows = len(entries) // columns
    output = Image.new("RGBA", (columns * 8, rows * 8))
    tiles_per_row = tiles.width // 8
    for index, value in enumerate(entries):
        tile_number = value & 0x03FF
        tile = tiles.crop(((tile_number % tiles_per_row) * 8,
                           (tile_number // tiles_per_row) * 8,
                           (tile_number % tiles_per_row + 1) * 8,
                           (tile_number // tiles_per_row + 1) * 8))
        if value & 0x0400:
            tile = tile.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        if value & 0x0800:
            tile = tile.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        output.alpha_composite(tile, ((index % columns) * 8, (index // columns) * 8))
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
FONT_CODES.update({"!": 0xAB, "?": 0xAC, ".": 0xAD, "-": 0xAE, ":": 0xF0, "/": 0xBA, "+": 0x2E})


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

    def width(self, text: str) -> int:
        total = 0
        for character in text:
            bbox = self.glyph(character).getbbox()
            total += max(4, (bbox[2] if bbox else 4) - (bbox[0] if bbox else 0) + 1)
        return total


def paste_masked(canvas: Image.Image, image: Image.Image, xy: tuple[int, int], scale: int = 1) -> None:
    if scale != 1:
        image = image.resize((image.width * scale, image.height * scale), NEAREST)
    canvas.alpha_composite(image, xy)


def item_icon(name: str) -> Image.Image:
    path = asset(f"graphics/items/icons/{name}.png")
    if not path.exists():
        path = asset("graphics/items/icons/poke_ball.png")
    return transparent_palette_image(path)


def ui_crop(name: str, box: tuple[int, int, int, int], size: tuple[int, int] = (24, 24)) -> Image.Image:
    image = Image.open(asset(name)).convert("RGBA").crop(box)
    return image.resize(size, NEAREST)


def load_jasc_palette(path: Path) -> list[tuple[int, int, int]]:
    lines = path.read_text().splitlines()
    return [tuple(map(int, line.split())) for line in lines[3:19]]


def render_overworld_map(layout_name: str, secondary_name: str,
                         width: int, height: int,
                         primary_name: str = "general") -> Image.Image:
    """Decode a FireRed map from its real blocks, metatiles, tiles and palettes."""
    data = DECOMP / "data"
    primary = data / "tilesets" / "primary" / primary_name
    secondary = data / "tilesets" / "secondary" / secondary_name
    primary_tiles = Image.open(primary / "tiles.png")
    secondary_tiles = Image.open(secondary / "tiles.png")
    primary_metatiles = (primary / "metatiles.bin").read_bytes()
    secondary_metatiles = (secondary / "metatiles.bin").read_bytes()
    primary_palettes = [load_jasc_palette(primary / "palettes" / f"{i:02}.pal") for i in range(16)]
    secondary_palettes = [load_jasc_palette(secondary / "palettes" / f"{i:02}.pal") for i in range(16)]
    blocks = struct.unpack(f"<{width * height}H", (data / "layouts" / layout_name / "map.bin").read_bytes())
    output = Image.new("RGBA", (width * 16, height * 16))

    def tile_image(reference: int, use_secondary: bool) -> Image.Image:
        tile_number = reference & 0x03FF
        palette_number = (reference >> 12) & 0x0F
        sheet = secondary_tiles if tile_number >= 640 else primary_tiles
        local_number = tile_number - 640 if tile_number >= 640 else tile_number
        palettes = secondary_palettes if use_secondary else primary_palettes
        tile = sheet.crop(((local_number % 16) * 8, (local_number // 16) * 8,
                           (local_number % 16 + 1) * 8, (local_number // 16 + 1) * 8))
        rgba = Image.new("RGBA", (8, 8))
        colors = palettes[palette_number]
        rgba.putdata([(*colors[index], 0 if index == 0 else 255) for index in tile.get_flattened_data()])
        if reference & 0x0400:
            rgba = rgba.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        if reference & 0x0800:
            rgba = rgba.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        return rgba

    for block_y in range(height):
        for block_x in range(width):
            block_id = blocks[block_y * width + block_x] & 0x03FF
            use_secondary = block_id >= 640
            local_id = block_id - 640 if use_secondary else block_id
            metatiles = secondary_metatiles if use_secondary else primary_metatiles
            references = struct.unpack_from("<8H", metatiles, local_id * 16)
            for layer in range(2):
                for quadrant in range(4):
                    tile = tile_image(references[layer * 4 + quadrant], use_secondary)
                    output.alpha_composite(tile, (block_x * 16 + (quadrant % 2) * 8,
                                                  block_y * 16 + (quadrant // 2) * 8))
    return output


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
    canvas = Image.new("RGBA", (320, 240), (230, 255, 230, 255))
    canvas.alpha_composite(terrain.crop((0, 0, 320, 240)), (0, 0))

    draw = ImageDraw.Draw(canvas)
    # Tamagotchi-like control split: four functions above the pet and four below.
    # The play field stays unobstructed between both rows.
    draw.rounded_rectangle((5, 48, 139, 91), radius=5, fill=(65, 65, 65))
    draw.rounded_rectangle((8, 51, 136, 88), radius=4, fill=(255, 255, 222), outline=(222, 181, 82), width=2)
    font.text(canvas, (14, 53), "BULBASAUR  LV5")
    draw.rounded_rectangle((14, 72, 35, 83), radius=3, fill=(222, 181, 82))
    font.text(canvas, (17, 70), "HP")
    draw.rectangle((37, 75, 126, 82), fill=(65, 65, 65))
    draw.rectangle((39, 77, 124, 80), fill=(82, 197, 115))
    paste_masked(canvas, pokemon_icon(rom, 16, 50), (29, 119))
    paste_masked(canvas, pokemon_icon(rom, 1, 72), (119, 91))
    paste_masked(canvas, pokemon_icon(rom, 19, 50), (239, 119))

    # The PC/storage button is cut from the real Pokemon Storage menu. The
    # Pokedex button is a miniature made from the real Kanto dex UI tiles.
    box_icon = ui_crop("graphics/pokemon_storage/menu.png", (0, 16, 24, 40))
    dex_icon = Image.open(ROOT / ".downloads" / "pokedex-icon-reference.jpg").convert("RGBA").resize((24, 24), NEAREST)

    bag_icon = Image.open(DECOMP / "graphics" / "interface" / "bag_male.png").convert("RGBA").crop((0, 0, 64, 64)).resize((24, 24), NEAREST)
    top_icons = (bag_icon, item_icon("poke_ball"), dex_icon, box_icon)
    bottom_icons = (item_icon("oran_berry"), item_icon("fresh_water"), item_icon("poke_doll"), item_icon("vs_seeker"))
    for row, icons in ((3, top_icons), (190, bottom_icons)):
        for index, icon in enumerate(icons):
            fire_red_button(canvas, (5 + index * 78, row), icon.resize((24, 24), NEAREST))
    # Trainer-battle charges belong to the VS Seeker itself.
    draw.rounded_rectangle((285, 191, 311, 207), radius=4, fill=(48, 88, 112), outline=(255, 255, 238), width=1)
    font.text(canvas, (288, 191), "1/3", light=True)

    output.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(output, optimize=True)


def render_home_portrait(output: Path) -> None:
    """Render the actual 240x320 portrait home layout using FireRed assets."""
    rom = ROM.read_bytes()
    if hashlib.sha1(rom).hexdigest().upper() != EXPECTED_SHA1:
        raise ValueError("Unexpected FireRed ROM")
    font = FireRedFont()
    terrain = render_tilemap(
        asset("graphics/battle_terrain/grass/terrain.png"),
        asset("graphics/battle_terrain/grass/terrain.bin"),
    )
    canvas = Image.new("RGBA", (240, 320), (230, 255, 230, 255))
    draw = ImageDraw.Draw(canvas)

    # One continuous FireRed-style outer frame.
    draw.rounded_rectangle((1, 1, 238, 318), radius=8, fill=(48, 48, 56), outline=(24, 24, 32), width=1)
    draw.rounded_rectangle((4, 4, 235, 315), radius=7, fill=(222, 181, 82))
    draw.rounded_rectangle((7, 7, 232, 312), radius=5, fill=(49, 91, 116))

    # Top controls: food, bath and the display-only battery gauge.
    top_icons = (item_icon("oran_berry"), item_icon("fresh_water"))
    for index, icon in enumerate(top_icons):
        x = 11 + index * 51
        draw.rounded_rectangle((x, 11, x + 45, 54), radius=4, fill=(65, 65, 65))
        draw.rounded_rectangle((x + 3, 14, x + 42, 51), radius=3,
                               fill=(255, 255, 238), outline=(222, 181, 82), width=2)
        paste_masked(canvas, icon.resize((24, 24), NEAREST), (x + 11, 21))
    draw.rounded_rectangle((122, 17, 224, 48), radius=4, fill=(255, 255, 238), outline=(65, 65, 65), width=2)
    draw.rounded_rectangle((130, 26, 184, 39), radius=3, fill=(65, 65, 65))
    draw.rectangle((133, 29, 174, 36), fill=(82, 197, 115))
    draw.rectangle((184, 29, 188, 36), fill=(65, 65, 65))
    font.text(canvas, (193, 24), "82")

    # Route-like pet field. Keep it inside the outer frame; unlike the battle
    # terrain, this is a flat overworld-style lawn with scenery around its rim.
    field_box = (8, 59, 231, 198)
    draw.rectangle(field_box, fill=(168, 224, 160))
    # Subtle 8 px overworld tile rhythm and scattered grass blades.
    for tile_y in range(63, 195, 16):
        for tile_x in range(12 + ((tile_y // 16) & 1) * 8, 228, 16):
            draw.line((tile_x, tile_y + 5, tile_x + 2, tile_y + 2), fill=(112, 192, 120), width=1)
            draw.line((tile_x + 3, tile_y + 5, tile_x + 3, tile_y + 1), fill=(112, 192, 120), width=1)
            draw.line((tile_x + 4, tile_y + 5, tile_x + 6, tile_y + 3), fill=(112, 192, 120), width=1)
    # FireRed-like fence across the back of the small yard.
    draw.rectangle((18, 72, 221, 76), fill=(104, 112, 112))
    draw.rectangle((18, 77, 221, 80), fill=(224, 224, 200))
    for x in range(18, 222, 18):
        draw.polygon(((x, 67), (x + 5, 63), (x + 10, 67)), fill=(248, 248, 224))
        draw.rectangle((x, 67, x + 10, 84), fill=(232, 232, 216))
        draw.rectangle((x + 7, 68, x + 10, 84), fill=(152, 160, 152))
        draw.rectangle((x + 2, 70, x + 5, 82), fill=(255, 255, 240))
    # Decorative route flowers and edge bushes, deliberately outside the
    # Pokemon movement lanes.
    for fx, fy in ((24, 92), (196, 94), (28, 177), (198, 176)):
        draw.rectangle((fx + 3, fy + 5, fx + 5, fy + 12), fill=(56, 152, 80))
        draw.rectangle((fx, fy + 8, fx + 8, fy + 11), fill=(48, 136, 72))
        draw.rectangle((fx + 1, fy + 1, fx + 7, fy + 7), fill=(224, 80, 48))
        draw.rectangle((fx + 3, fy + 3, fx + 5, fy + 5), fill=(248, 208, 72))
    for bx, by in ((8, 112), (215, 119)):
        draw.rectangle((bx, by + 8, bx + 15, by + 28), fill=(56, 136, 72))
        draw.rectangle((bx + 3, by + 2, bx + 18, by + 25), fill=(72, 168, 80))
        draw.rectangle((bx + 7, by, bx + 14, by + 20), fill=(104, 192, 96))
        draw.rectangle((bx + 3, by + 6, bx + 6, by + 9), fill=(152, 216, 128))

    # Three real ROM PC/party icons, at the size used by the firmware mockup.
    paste_masked(canvas, pokemon_icon(rom, 1, 46), (97, 91))
    paste_masked(canvas, pokemon_icon(rom, 4, 38), (38, 137))
    paste_masked(canvas, pokemon_icon(rom, 7, 38), (165, 137))

    # FireRed dialogue window: two lines fit without shrinking the original font.
    draw.rounded_rectangle((8, 199, 231, 250), radius=5, fill=(65, 65, 65))
    draw.rounded_rectangle((11, 202, 228, 247), radius=4, fill=(222, 181, 82))
    draw.rounded_rectangle((15, 206, 224, 243), radius=2, fill=(255, 255, 238))
    font.text(canvas, (21, 210), "BULBASAUR IS")
    font.text(canvas, (21, 227), "FEELING GREAT!")

    # Bottom row uses only graphics present in the FireRed assets.
    bag = Image.open(DECOMP / "graphics" / "interface" / "bag_male.png").convert("RGBA").crop((0, 0, 64, 64)).resize((24, 24), NEAREST)
    dex = transparent_palette_image(DECOMP / "graphics" / "object_events" / "pics" / "misc" / "pokedex.png").resize((24, 24), NEAREST)
    # The raw storage/shop sheets are fragments assembled by the GBA engine
    # and are ambiguous alone. Draw compact, readable derivatives instead.
    box = Image.new("RGBA", (24, 24))
    bd = ImageDraw.Draw(box)
    bd.rectangle((3, 7, 20, 20), fill=(168, 112, 56), outline=(72, 64, 56))
    bd.rectangle((1, 4, 22, 9), fill=(232, 176, 88), outline=(72, 64, 56))
    bd.rectangle((10, 4, 13, 20), fill=(248, 216, 128))
    bd.ellipse((8, 10, 15, 17), fill=(248, 248, 232), outline=(72, 64, 56))
    bd.rectangle((8, 13, 15, 14), fill=(200, 56, 48))
    bd.ellipse((11, 12, 13, 14), fill=(248, 248, 232), outline=(72, 64, 56))
    mart = Image.new("RGBA", (24, 24))
    md = ImageDraw.Draw(mart)
    md.rectangle((3, 9, 21, 21), fill=(248, 248, 232), outline=(48, 64, 80))
    md.polygon(((1, 9), (5, 3), (19, 3), (23, 9)), fill=(64, 136, 208), outline=(48, 64, 80))
    for stripe in range(4, 21, 5):
        md.rectangle((stripe, 6, stripe + 2, 10), fill=(168, 216, 240))
    md.rectangle((5, 13, 10, 21), fill=(96, 176, 216))
    md.rectangle((13, 13, 19, 18), fill=(160, 224, 240), outline=(48, 64, 80))
    md.ellipse((9, 4, 15, 10), fill=(248, 248, 232), outline=(48, 64, 80))
    md.rectangle((9, 7, 15, 8), fill=(200, 56, 48))
    icons = (bag, item_icon("vs_seeker"), dex, box, mart)
    for index, icon in enumerate(icons):
        x = 9 + index * 45
        draw.rounded_rectangle((x, 258, x + 42, 306), radius=4, fill=(65, 65, 65))
        draw.rounded_rectangle((x + 3, 261, x + 39, 303), radius=3,
                               fill=(255, 255, 238), outline=(222, 181, 82), width=2)
        paste_masked(canvas, icon.resize((24, 24), NEAREST), (x + 9, 267))
    draw.rounded_rectangle((57, 287, 85, 302), radius=3, fill=(49, 91, 116), outline=(255, 255, 238), width=1)
    font.text(canvas, (60, 286), "3/3", light=True)

    output.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(output, optimize=True)


def render_home_landscape(output: Path) -> None:
    """Final 320x240 home preview with an unmodified real Route 1 crop."""
    rom = ROM.read_bytes()
    if hashlib.sha1(rom).hexdigest().upper() != EXPECTED_SHA1:
        raise ValueError("Unexpected FireRed ROM")
    font = FireRedFont()
    route = render_overworld_map("Route1", "pallet_town", 24, 40)
    canvas = Image.new("RGBA", (320, 240), (49, 91, 116, 255))
    draw = ImageDraw.Draw(canvas)

    # Complete outer FireRed frame.
    draw.rounded_rectangle((1, 1, 318, 238), radius=8, fill=(48, 48, 56), outline=(24, 24, 32), width=1)
    draw.rounded_rectangle((4, 4, 315, 235), radius=7, fill=(222, 181, 82))
    draw.rounded_rectangle((7, 7, 312, 232), radius=5, fill=(49, 91, 116))

    # Feeding and bath controls above the pet field.
    for index, icon in enumerate((item_icon("oran_berry"), item_icon("fresh_water"))):
        x = 11 + index * 55
        draw.rounded_rectangle((x, 10, x + 49, 45), radius=4, fill=(65, 65, 65))
        draw.rounded_rectangle((x + 3, 13, x + 46, 42), radius=3,
                               fill=(255, 255, 238), outline=(222, 181, 82), width=2)
        paste_masked(canvas, icon.resize((24, 24), NEAREST), (x + 13, 16))

    # Battery is information only.
    draw.rounded_rectangle((224, 12, 307, 43), radius=4, fill=(255, 255, 238), outline=(65, 65, 65), width=2)
    draw.rounded_rectangle((231, 21, 273, 34), radius=3, fill=(65, 65, 65))
    draw.rectangle((234, 24, 267, 31), fill=(82, 197, 115))
    draw.rectangle((273, 24, 277, 31), fill=(65, 65, 65))
    font.text(canvas, (282, 20), "82")
    draw.rectangle((300, 23, 301, 24), fill=(56, 56, 56))
    draw.rectangle((297, 29, 298, 30), fill=(56, 56, 56))
    draw.line((299, 28, 302, 25), fill=(56, 56, 56), width=1)

    # Compose a clean pet yard exclusively from exact Route 1 pixels. The
    # scenery is rearranged for the UI, but no grass, fence, flower or tree is
    # generated/redrawn.
    route_crop = Image.new("RGBA", (304, 134))
    grass_tile = route.crop((64, 224, 80, 240))
    for gy in range(0, 134, 16):
        for gx in range(0, 304, 16):
            route_crop.alpha_composite(grass_tile, (gx, gy))
    fence = route.crop((32, 566, 336, 590))
    route_crop.alpha_composite(fence, (0, 0))
    route_crop.alpha_composite(route.crop((32, 184, 64, 232)), (0, 42))
    route_crop.alpha_composite(route.crop((320, 184, 352, 232)), (272, 49))
    route_crop.alpha_composite(route.crop((32, 72, 64, 104)), (20, 91))
    route_crop.alpha_composite(route.crop((304, 32, 336, 64)), (252, 92))
    canvas.alpha_composite(route_crop, (8, 49))

    # Real FireRed party/PC icons from the ROM.
    paste_masked(canvas, pokemon_icon(rom, 4, 38), (64, 91))
    paste_masked(canvas, pokemon_icon(rom, 1, 46), (137, 75))
    paste_masked(canvas, pokemon_icon(rom, 7, 38), (224, 96))

    # Compact original-font status window over the lower edge of the route.
    draw.rounded_rectangle((12, 146, 307, 184), radius=5, fill=(65, 65, 65))
    draw.rounded_rectangle((15, 149, 304, 181), radius=4, fill=(222, 181, 82))
    draw.rounded_rectangle((19, 153, 300, 177), radius=2, fill=(255, 255, 238))
    font.text(canvas, (27, 157), "BULBASAUR IS FEELING GREAT!")

    bag = Image.open(DECOMP / "graphics" / "interface" / "bag_male.png").convert("RGBA").crop((0, 0, 64, 64)).resize((24, 24), NEAREST)
    dex = transparent_palette_image(DECOMP / "graphics" / "object_events" / "pics" / "misc" / "pokedex.png").resize((24, 24), NEAREST)
    box = Image.new("RGBA", (24, 24)); bd = ImageDraw.Draw(box)
    bd.rectangle((3, 7, 20, 20), fill=(168, 112, 56), outline=(72, 64, 56))
    bd.rectangle((1, 4, 22, 9), fill=(232, 176, 88), outline=(72, 64, 56))
    bd.rectangle((10, 4, 13, 20), fill=(248, 216, 128))
    bd.ellipse((8, 10, 15, 17), fill=(248, 248, 232), outline=(72, 64, 56))
    bd.rectangle((8, 13, 15, 14), fill=(200, 56, 48))
    bd.ellipse((11, 12, 13, 14), fill=(248, 248, 232), outline=(72, 64, 56))
    mart = Image.new("RGBA", (24, 24)); md = ImageDraw.Draw(mart)
    md.rectangle((3, 9, 21, 21), fill=(248, 248, 232), outline=(48, 64, 80))
    md.polygon(((1, 9), (5, 3), (19, 3), (23, 9)), fill=(64, 136, 208), outline=(48, 64, 80))
    for stripe in range(4, 21, 5): md.rectangle((stripe, 6, stripe + 2, 10), fill=(168, 216, 240))
    md.rectangle((5, 13, 10, 21), fill=(96, 176, 216))
    md.rectangle((13, 13, 19, 18), fill=(160, 224, 240), outline=(48, 64, 80))
    md.ellipse((9, 4, 15, 10), fill=(248, 248, 232), outline=(48, 64, 80))
    md.rectangle((9, 7, 15, 8), fill=(200, 56, 48))
    for index, icon in enumerate((bag, item_icon("vs_seeker"), dex, box, mart)):
        x = 9 + index * 62
        draw.rounded_rectangle((x, 190, x + 57, 229), radius=4, fill=(65, 65, 65))
        draw.rounded_rectangle((x + 3, 193, x + 54, 226), radius=3,
                               fill=(255, 255, 238), outline=(222, 181, 82), width=2)
        paste_masked(canvas, icon.resize((24, 24), NEAREST), (x + 17, 197))
    draw.rounded_rectangle((79, 213, 108, 228), radius=3, fill=(49, 91, 116), outline=(255, 255, 238), width=1)
    font.text(canvas, (82, 212), "3/3", light=True)

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
    canvas.alpha_composite(terrain.crop((144, 8, 320, 88)), (220, 10))
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

    commands = ("FIGHT", "BAG", "POKEMON", "RUN")
    for index, command in enumerate(commands):
        x = 9 + index * 78
        draw.rounded_rectangle((x, 190, x + 67, 237), radius=4, fill=(65, 65, 65))
        draw.rounded_rectangle((x + 3, 193, x + 64, 234), radius=3,
                               fill=(255, 255, 238), outline=(222, 181, 82), width=3)
        font.text(canvas, (x + (67 - font.width(command)) // 2, 205), command)

    output.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(output, optimize=True)


def framed_window(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int], fill=(255, 255, 238)) -> None:
    x0, y0, x1, y1 = box
    draw.rounded_rectangle(box, radius=5, fill=(64, 64, 72))
    draw.rounded_rectangle((x0 + 3, y0 + 3, x1 - 3, y1 - 3), radius=4,
                           fill=(224, 181, 82))
    draw.rounded_rectangle((x0 + 6, y0 + 6, x1 - 6, y1 - 6), radius=2, fill=fill)


def render_pokedex(output: Path) -> None:
    rom = ROM.read_bytes()
    font = FireRedFont()
    canvas = Image.new("RGBA", (320, 240), (176, 40, 48, 255))
    draw = ImageDraw.Draw(canvas)
    draw.rounded_rectangle((3, 3, 316, 236), radius=13, fill=(112, 24, 32),
                           outline=(48, 40, 48), width=3)
    draw.rounded_rectangle((9, 8, 310, 230), radius=10, fill=(216, 64, 56),
                           outline=(248, 144, 96), width=2)
    framed_window(draw, (17, 18, 205, 205), (248, 248, 232))
    framed_window(draw, (210, 18, 302, 205), (224, 240, 216))
    draw.rectangle((24, 26, 198, 47), fill=(48, 88, 112))
    font.text(canvas, (31, 28), "KANTO POKEDEX", light=True)
    entries = ("021 SPEAROW", "022 FEAROW", "023 EKANS", "024 ARBOK",
               "025 PIKACHU", "026 RAICHU", "027 SANDSHREW")
    for i, label in enumerate(entries):
        y = 53 + i * 20
        if i == 4:
            draw.rounded_rectangle((25, y - 1, 197, y + 17), radius=2, fill=(248, 216, 112))
            draw.polygon(((28, y + 4), (34, y + 8), (28, y + 12)), fill=(56, 56, 64))
        if i in (0, 2, 4):
            ball = item_icon("poke_ball").resize((14, 14), NEAREST)
            paste_masked(canvas, ball, (36, y + 1))
        font.text(canvas, (53, y), label)
    paste_masked(canvas, pokemon_icon(rom, 25, 58), (227, 39))
    font.text(canvas, (219, 103), "NO.025")
    font.text(canvas, (217, 121), "PIKACHU")
    font.text(canvas, (217, 135), "MOUSE")
    font.text(canvas, (217, 149), "POKEMON")
    font.text(canvas, (218, 168), "SEEN 34")
    font.text(canvas, (218, 184), "OWNED 18")
    framed_window(draw, (17, 209, 302, 231), (248, 248, 232))
    font.text(canvas, (30, 211), "A: DATA        B: BACK")
    output.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(output, optimize=True)


def render_mart(output: Path) -> None:
    font = FireRedFont()
    canvas = Image.new("RGBA", (320, 240), (200, 224, 192, 255))
    draw = ImageDraw.Draw(canvas)
    # A restrained tile-like shop backdrop remains visible around the windows.
    for y in range(0, 240, 16):
        for x in range(0, 320, 16):
            draw.rectangle((x, y, x + 15, y + 15), outline=(176, 200, 168))
    draw.rectangle((0, 0, 320, 23), fill=(48, 88, 112))
    font.text(canvas, (11, 4), "POKE MART", light=True)
    framed_window(draw, (8, 29, 220, 183), (255, 255, 238))
    framed_window(draw, (224, 29, 314, 91), (255, 255, 238))
    items = (("poke_ball", "POKE BALL", "200"), ("potion", "POTION", "300"),
             ("great_ball", "GREAT BALL", "600"), ("super_potion", "SUPER POTION", "700"),
             ("antidote", "ANTIDOTE", "100"), ("x_attack", "X ATTACK", "500"),
             ("full_heal", "FULL HEAL", "600"))
    for i, (icon_name, name, price) in enumerate(items):
        y = 37 + i * 20
        if i == 0:
            draw.rounded_rectangle((15, y - 1, 213, y + 17), radius=2, fill=(248, 216, 112))
            draw.polygon(((17, y + 4), (23, y + 8), (17, y + 12)), fill=(56, 56, 64))
        paste_masked(canvas, item_icon(icon_name).resize((16, 16), NEAREST), (26, y))
        font.text(canvas, (47, y), name)
        font.text(canvas, (174, y), price)
    font.text(canvas, (232, 38), "MONEY")
    font.text(canvas, (234, 56), "3420")
    font.text(canvas, (232, 73), "IN BAG 12")
    framed_window(draw, (8, 188, 314, 232), (255, 255, 238))
    font.text(canvas, (18, 193), "A TOOL USED FOR CATCHING")
    font.text(canvas, (18, 209), "WILD POKEMON.   A: BUY B: BACK")
    output.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(output, optimize=True)


def render_box(output: Path) -> None:
    rom = ROM.read_bytes(); font = FireRedFont()
    canvas = Image.new("RGBA", (320, 240), (128, 200, 224, 255)); draw = ImageDraw.Draw(canvas)
    draw.rounded_rectangle((3, 3, 316, 236), radius=7, fill=(64, 64, 72))
    draw.rounded_rectangle((7, 7, 312, 232), radius=5, fill=(200, 216, 216))
    draw.rounded_rectangle((10, 10, 309, 229), radius=4, fill=(112, 184, 216))
    font.text(canvas, (16, 12), "POKEMON BOX")
    font.text(canvas, (248, 12), "12/60")
    draw.rounded_rectangle((10, 32, 241, 188), radius=4, fill=(48, 88, 112), outline=(255, 255, 238), width=2)
    for col in range(1, 6): draw.line((10 + col * 38, 33, 10 + col * 38, 187), fill=(80, 136, 168))
    for row in range(1, 5): draw.line((11, 32 + row * 31, 240, 32 + row * 31), fill=(80, 136, 168))
    species = (1, 4, 7, 16, 19, 25, 10, 13, 21, 29, 32, 35)
    for index, species_id in enumerate(species):
        x, y = 13 + index % 6 * 38, 33 + index // 6 * 31
        if index == 5: draw.rounded_rectangle((x, y, x + 34, y + 29), radius=3, outline=(255, 224, 80), width=2)
        paste_masked(canvas, pokemon_icon(rom, species_id, 28), (x + 3, y + 1))
    draw.rounded_rectangle((247, 32, 309, 188), radius=4, fill=(48, 128, 144), outline=(255, 255, 238), width=2)
    font.text(canvas, (257, 36), "PARTY", light=True)
    for slot, species_id in enumerate((1, 25, 16)):
        y = 53 + slot * 43; paste_masked(canvas, pokemon_icon(rom, species_id, 30), (263, y))
        draw.rounded_rectangle((255, y - 2, 303, y + 34), radius=3, outline=(255, 224, 80) if slot == 0 else (255,255,238), width=2)
    framed_window(draw, (10, 194, 309, 228), (255, 255, 238))
    font.text(canvas, (18, 200), "PIKACHU  LV12  HP 32/32")
    output.parent.mkdir(parents=True, exist_ok=True); canvas.convert("RGB").save(output, optimize=True)


def trainer_sprite(name: str, size: tuple[int, int] = (96, 96)) -> Image.Image:
    image = transparent_palette_image(DECOMP / "graphics" / "trainers" / "front_pics" / f"{name}.png")
    return image.resize(size, NEAREST)


def render_trainer_encounter(output: Path) -> None:
    font=FireRedFont();canvas=Image.new("RGBA",(320,240),(184,224,248,255));draw=ImageDraw.Draw(canvas)
    for y in range(0,150,8): draw.line((0,y,319,y),fill=(176+y//8*2,216+y//8,240))
    paste_masked(canvas,trainer_sprite("youngster_front_pic",(112,112)),(104,28))
    draw.rounded_rectangle((5,151,314,235),radius=6,fill=(64,64,72));draw.rounded_rectangle((9,155,310,231),radius=4,fill=(49,91,116),outline=(222,181,82),width=4)
    font.text(canvas,(18,162),"YOUNGSTER BEN",light=True)
    font.text(canvas,(18,185),"HEY! YOU HAVE POKEMON!",light=True)
    font.text(canvas,(18,205),"COME ON! LET'S BATTLE!",light=True)
    font.text(canvas,(225,220),"TOUCH",light=True)
    output.parent.mkdir(parents=True,exist_ok=True);canvas.convert("RGB").save(output,optimize=True)


def render_wild_appeared(output: Path) -> None:
    """Render the exact pre-battle choice shown when a wild encounter becomes due."""
    rom = ROM.read_bytes(); font = FireRedFont()
    transition = DECOMP / "graphics" / "battle_transitions"
    source = render_fixed_tilemap(transition / "big_pokeball.png",
                                  transition / "big_pokeball_tilemap.bin", 30)
    canvas = source.resize((360, 240), NEAREST).crop((20, 0, 340, 240)); draw = ImageDraw.Draw(canvas)

    draw.rounded_rectangle((5, 5, 315, 60), radius=6, fill=(0, 0, 0),
                           outline=(255, 255, 255), width=1)

    heading = "A WILD POKEMON APPEARED!"
    subtitle = "CHOOSE YOUR BATTLER"
    font.text(canvas, ((320 - font.width(heading)) // 2, 14), heading, light=True)
    font.text(canvas, ((320 - font.width(subtitle)) // 2, 39), subtitle, light=True)

    party = (
        (1, "BULBASAUR", 15, 42, 42),
        (25, "PIKACHU", 12, 32, 32),
        (16, "PIDGEY", 11, 27, 30),
    )
    for slot, (species, name, level, hp, maximum_hp) in enumerate(party):
        center_x = 54 + slot * 106
        draw.rounded_rectangle((center_x - 48, 64, center_x + 48, 218), radius=6,
                               outline=(255, 255, 255), width=1)
        indices, palette = extract_battle_sprite(rom, species, FRONT_SPRITE_TABLE)
        paste_masked(canvas, indexed_sprite(indices, palette), (center_x - 32, 68))
        draw.rounded_rectangle((center_x - 45, 128, center_x + 45, 170), radius=4,
                               fill=(0, 0, 0))
        font.text(canvas, (center_x - font.width(name) // 2, 132), name, light=True)
        stats = f"LV{level} {hp}/{maximum_hp}"
        font.text(canvas, (center_x - font.width(stats) // 2, 149), stats, light=True)

        x, y, width, height = center_x - 43, 174, 86, 38
        draw.rounded_rectangle((x, y, x + width, y + height), radius=6,
                               fill=(48, 112, 64), outline=(255, 255, 255), width=1)
        label = "CHOOSE"
        font.text(canvas, (x + (width - font.width(label)) // 2, y + 10), label, light=True)

    output.parent.mkdir(parents=True, exist_ok=True)
    canvas.convert("RGB").save(output, optimize=True)


def render_gym_invite(output: Path) -> None:
    font=FireRedFont();canvas=Image.new("RGBA",(320,240),(232,232,216,255));draw=ImageDraw.Draw(canvas)
    draw.rounded_rectangle((4,4,315,235),radius=8,fill=(64,64,72));draw.rounded_rectangle((8,8,311,231),radius=6,fill=(255,255,238))
    draw.rectangle((12,12,307,43),fill=(192,48,48));font.text(canvas,(91,19),"GYM CHALLENGE",light=True)
    paste_masked(canvas,trainer_sprite("leader_brock_front_pic",(96,96)),(199,50))
    font.text(canvas,(22,56),"PEWTER CITY GYM");font.text(canvas,(22,78),"LEADER BROCK")
    font.text(canvas,(22,106),"BOULDER BADGE");font.text(canvas,(22,128),"RECOMMENDED LV12-14")
    font.text(canvas,(22,153),"3 CONSECUTIVE BATTLES");font.text(canvas,(22,170),"NO VS SEEKER CHARGE")
    for x,label,color in ((17,"CHALLENGE",(48,128,72)),(165,"LATER",(184,48,48))):
        draw.rounded_rectangle((x,194,x+137,228),radius=5,fill=(64,64,72));draw.rounded_rectangle((x+3,197,x+134,225),radius=3,fill=color,outline=(255,255,238),width=2)
        font.text(canvas,(x+(137-font.width(label))//2,203),label,light=True)
    output.parent.mkdir(parents=True,exist_ok=True);canvas.convert("RGB").save(output,optimize=True)


def render_level_up(output: Path) -> None:
    rom=ROM.read_bytes();font=FireRedFont();canvas=Image.new("RGBA",(320,240),(232,244,216,255));draw=ImageDraw.Draw(canvas)
    terrain=render_tilemap(asset("graphics/battle_terrain/grass/terrain.png"),asset("graphics/battle_terrain/grass/terrain.bin"))
    canvas.alpha_composite(terrain.crop((0,72,176,160)),(0,67));paste_masked(canvas,indexed_sprite(*extract_battle_sprite(rom,1,BACK_SPRITE_TABLE)),(39,84))
    framed_window(draw,(151,24,310,152),(255,255,238));font.text(canvas,(163,31),"BULBASAUR  LV6")
    stats=(("MAX HP","+3","23"),("ATTACK","+2","12"),("DEFENSE","+2","13"),("SP. ATK","+2","14"),("SP. DEF","+2","14"),("SPEED","+1","10"))
    for i,(name,gain,total) in enumerate(stats):
        y=53+i*15;font.text(canvas,(161,y),name);font.text(canvas,(247,y),gain);font.text(canvas,(281,y),total)
    draw.rounded_rectangle((5,157,314,235),radius=6,fill=(64,64,72));draw.rounded_rectangle((9,161,310,231),radius=4,fill=(49,91,116),outline=(222,181,82),width=4)
    font.text(canvas,(18,171),"BULBASAUR GREW TO",light=True);font.text(canvas,(18,194),"LEVEL 6!",light=True)
    output.parent.mkdir(parents=True,exist_ok=True);canvas.convert("RGB").save(output,optimize=True)


if __name__ == "__main__":
    mode = sys.argv[2] if len(sys.argv) > 2 else "battle"
    destination = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "docs" / "mockups" / f"{mode}-fire-red-assets.png"
    if mode == "home":
        render_home(destination)
    elif mode == "home-portrait":
        render_home_portrait(destination)
    elif mode == "home-landscape":
        render_home_landscape(destination)
    elif mode == "battle":
        render_battle(destination)
    elif mode == "pokedex":
        render_pokedex(destination)
    elif mode == "mart":
        render_mart(destination)
    elif mode == "box":
        render_box(destination)
    elif mode == "trainer":
        render_trainer_encounter(destination)
    elif mode == "wild-appeared":
        render_wild_appeared(destination)
    elif mode == "gym":
        render_gym_invite(destination)
    elif mode == "levelup":
        render_level_up(destination)
    else:
        raise SystemExit("mode must be home, home-portrait, home-landscape, battle, pokedex, mart, box, trainer, wild-appeared, gym or levelup")
