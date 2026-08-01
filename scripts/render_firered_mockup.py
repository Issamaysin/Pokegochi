"""Extract a FireRed sprite from the local ROM and render a 320x240 UI mockup.

The ROM and extracted sprite remain local project inputs. Nothing here embeds
or downloads copyrighted game data.
"""

from __future__ import annotations

import hashlib
import struct
import sys
import zlib
from pathlib import Path


WIDTH, HEIGHT = 320, 240
EXPECTED_SHA1 = "41CB23D8DCCC8EBD7C649CD8FBB58EEACE6E2FDC"
FRONT_SPRITE_TABLE = 0x2350AC
NORMAL_PALETTE_TABLE = 0x23730C
BACK_SPRITE_TABLE = 0x23654C
ICON_PALETTES = 0x3D3740
ICON_POINTER_TABLE = 0x3D37A0
ICON_PALETTE_LOOKUP = 0x3D3E80

FONT = {
    " ": (0, 0, 0, 0, 0, 0, 0),
    "-": (0, 0, 0, 31, 0, 0, 0), "/": (1, 2, 4, 8, 16, 0, 0),
    ".": (0, 0, 0, 0, 0, 12, 12), ":": (0, 12, 12, 0, 12, 12, 0),
    "0": (14, 17, 19, 21, 25, 17, 14), "1": (4, 12, 4, 4, 4, 4, 14),
    "2": (14, 17, 1, 2, 4, 8, 31), "3": (30, 1, 1, 14, 1, 1, 30),
    "4": (2, 6, 10, 18, 31, 2, 2), "5": (31, 16, 16, 30, 1, 1, 30),
    "6": (14, 16, 16, 30, 17, 17, 14), "7": (31, 1, 2, 4, 8, 8, 8),
    "8": (14, 17, 17, 14, 17, 17, 14), "9": (14, 17, 17, 15, 1, 1, 14),
    "A": (14, 17, 17, 31, 17, 17, 17), "B": (30, 17, 17, 30, 17, 17, 30),
    "C": (14, 17, 16, 16, 16, 17, 14), "D": (30, 17, 17, 17, 17, 17, 30),
    "E": (31, 16, 16, 30, 16, 16, 31), "F": (31, 16, 16, 30, 16, 16, 16),
    "G": (14, 17, 16, 23, 17, 17, 15), "H": (17, 17, 17, 31, 17, 17, 17),
    "I": (14, 4, 4, 4, 4, 4, 14), "J": (7, 2, 2, 2, 18, 18, 12),
    "K": (17, 18, 20, 24, 20, 18, 17), "L": (16, 16, 16, 16, 16, 16, 31),
    "M": (17, 27, 21, 21, 17, 17, 17), "N": (17, 25, 21, 19, 17, 17, 17),
    "O": (14, 17, 17, 17, 17, 17, 14), "P": (30, 17, 17, 30, 16, 16, 16),
    "Q": (14, 17, 17, 17, 21, 18, 13), "R": (30, 17, 17, 30, 20, 18, 17),
    "S": (15, 16, 16, 14, 1, 1, 30), "T": (31, 4, 4, 4, 4, 4, 4),
    "U": (17, 17, 17, 17, 17, 17, 14), "V": (17, 17, 17, 17, 17, 10, 4),
    "W": (17, 17, 17, 21, 21, 21, 10), "X": (17, 17, 10, 4, 10, 17, 17),
    "Y": (17, 17, 10, 4, 4, 4, 4), "Z": (31, 1, 2, 4, 8, 16, 31),
}


class Canvas:
    def __init__(self, width: int, height: int, color: tuple[int, int, int]):
        self.width, self.height = width, height
        self.pixels = bytearray(color * (width * height))

    def pixel(self, x: int, y: int, color: tuple[int, int, int]) -> None:
        if 0 <= x < self.width and 0 <= y < self.height:
            offset = (y * self.width + x) * 3
            self.pixels[offset : offset + 3] = bytes(color)

    def rect(self, x: int, y: int, w: int, h: int, color: tuple[int, int, int]) -> None:
        for row in range(max(0, y), min(self.height, y + h)):
            start = (row * self.width + max(0, x)) * 3
            end = (row * self.width + min(self.width, x + w)) * 3
            self.pixels[start:end] = bytes(color) * ((end - start) // 3)

    def frame(self, x: int, y: int, w: int, h: int, fill: tuple[int, int, int]) -> None:
        self.rect(x, y, w, h, (56, 64, 72))
        self.rect(x + 2, y + 2, w - 4, h - 4, (232, 120, 40))
        self.rect(x + 4, y + 4, w - 8, h - 8, (248, 224, 136))
        self.rect(x + 6, y + 6, w - 12, h - 12, fill)

    def line(self, x0: int, y0: int, x1: int, y1: int, color: tuple[int, int, int], thickness=1) -> None:
        dx, sx = abs(x1 - x0), 1 if x0 < x1 else -1
        dy, sy = -abs(y1 - y0), 1 if y0 < y1 else -1
        error = dx + dy
        while True:
            self.rect(x0 - thickness // 2, y0 - thickness // 2, thickness, thickness, color)
            if x0 == x1 and y0 == y1:
                break
            twice = 2 * error
            if twice >= dy:
                error += dy
                x0 += sx
            if twice <= dx:
                error += dx
                y0 += sy

    def circle(self, cx: int, cy: int, radius: int, color: tuple[int, int, int], fill=True) -> None:
        radius_squared = radius * radius
        inner_squared = (radius - 2) * (radius - 2)
        for y in range(cy - radius, cy + radius + 1):
            for x in range(cx - radius, cx + radius + 1):
                distance = (x - cx) ** 2 + (y - cy) ** 2
                if distance <= radius_squared and (fill or distance >= inner_squared):
                    self.pixel(x, y, color)

    def text(self, x: int, y: int, value: str, color=(48, 48, 56), scale: int = 1) -> None:
        cursor = x
        for character in value.upper():
            rows = FONT.get(character, FONT[" "])
            for row, bits in enumerate(rows):
                for column in range(5):
                    if bits & (1 << (4 - column)):
                        self.rect(cursor + column * scale, y + row * scale, scale, scale, color)
            cursor += 6 * scale

    def blit_indexed(self, x: int, y: int, indices: list[int], palette: list[tuple[int, int, int]], scale=1) -> None:
        for source_y in range(64):
            for source_x in range(64):
                index = indices[source_y * 64 + source_x]
                if index:
                    self.rect(x + source_x * scale, y + source_y * scale, scale, scale, palette[index])

    def png(self, path: Path) -> None:
        raw = b"".join(b"\x00" + self.pixels[y * self.width * 3 : (y + 1) * self.width * 3] for y in range(self.height))
        def chunk(kind: bytes, data: bytes) -> bytes:
            return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", self.width, self.height, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def gba_pointer(rom: bytes, table: int, species: int) -> int:
    pointer = struct.unpack_from("<I", rom, table + species * 8)[0]
    if not 0x08000000 <= pointer < 0x0A000000:
        raise ValueError(f"Invalid GBA pointer 0x{pointer:08X}")
    return pointer - 0x08000000


def lz77(rom: bytes, offset: int) -> bytes:
    if rom[offset] != 0x10:
        raise ValueError(f"Expected GBA LZ77 block at 0x{offset:X}")
    target_size = int.from_bytes(rom[offset + 1 : offset + 4], "little")
    cursor, output = offset + 4, bytearray()
    while len(output) < target_size:
        flags, cursor = rom[cursor], cursor + 1
        for bit in range(7, -1, -1):
            if len(output) >= target_size:
                break
            if flags & (1 << bit):
                first, second = rom[cursor], rom[cursor + 1]
                cursor += 2
                length = (first >> 4) + 3
                distance = ((first & 0x0F) << 8 | second) + 1
                for _ in range(length):
                    output.append(output[-distance])
            else:
                output.append(rom[cursor])
                cursor += 1
    return bytes(output)


def extract_species(rom: bytes, species: int) -> tuple[list[int], list[tuple[int, int, int]]]:
    return extract_battle_sprite(rom, species, FRONT_SPRITE_TABLE)


def extract_battle_sprite(rom: bytes, species: int, table: int) -> tuple[list[int], list[tuple[int, int, int]]]:
    graphics = lz77(rom, gba_pointer(rom, table, species))
    colors = lz77(rom, gba_pointer(rom, NORMAL_PALETTE_TABLE, species))
    indices = [0] * (64 * 64)
    for tile_y in range(8):
        for tile_x in range(8):
            tile = (tile_y * 8 + tile_x) * 32
            for pixel_y in range(8):
                for pair_x in range(4):
                    packed = graphics[tile + pixel_y * 4 + pair_x]
                    indices[(tile_y * 8 + pixel_y) * 64 + tile_x * 8 + pair_x * 2] = packed & 0x0F
                    indices[(tile_y * 8 + pixel_y) * 64 + tile_x * 8 + pair_x * 2 + 1] = packed >> 4
    palette = []
    for index in range(16):
        color = struct.unpack_from("<H", colors, index * 2)[0]
        red, green, blue = color & 31, (color >> 5) & 31, (color >> 10) & 31
        palette.append((red * 255 // 31, green * 255 // 31, blue * 255 // 31))
    return indices, palette


def extract_icon(rom: bytes, species: int, frame: int = 0) -> tuple[list[int], list[tuple[int, int, int]]]:
    pointer = struct.unpack_from("<I", rom, ICON_POINTER_TABLE + species * 4)[0]
    if not 0x08000000 <= pointer < 0x0A000000:
        raise ValueError(f"Invalid icon pointer 0x{pointer:08X}")
    start = pointer - 0x08000000 + frame * 512
    graphics = rom[start : start + 512]
    indices = [0] * (32 * 32)
    for tile_y in range(4):
        for tile_x in range(4):
            tile = (tile_y * 4 + tile_x) * 32
            for pixel_y in range(8):
                for pair_x in range(4):
                    packed = graphics[tile + pixel_y * 4 + pair_x]
                    row = tile_y * 8 + pixel_y
                    column = tile_x * 8 + pair_x * 2
                    indices[row * 32 + column] = packed & 0x0F
                    indices[row * 32 + column + 1] = packed >> 4
    palette_index = rom[ICON_PALETTE_LOOKUP + species]
    palette = []
    for index in range(16):
        color = struct.unpack_from("<H", rom, ICON_PALETTES + palette_index * 32 + index * 2)[0]
        red, green, blue = color & 31, (color >> 5) & 31, (color >> 10) & 31
        palette.append((red * 255 // 31, green * 255 // 31, blue * 255 // 31))
    return indices, palette


def blit_icon(canvas: Canvas, x: int, y: int, icon: list[int], palette: list[tuple[int, int, int]], scale: int) -> None:
    for source_y in range(32):
        for source_x in range(32):
            index = icon[source_y * 32 + source_x]
            if index:
                canvas.rect(x + source_x * scale, y + source_y * scale, scale, scale, palette[index])


def draw_icon(canvas: Canvas, kind: str, cx: int, cy: int) -> None:
    ink = (48, 56, 64)
    if kind == "feed":
        canvas.circle(cx, cy + 2, 8, (208, 64, 56), True)
        canvas.line(cx, cy - 5, cx + 4, cy - 11, ink, 2)
        canvas.line(cx + 3, cy - 10, cx + 9, cy - 9, (72, 152, 72), 3)
        canvas.pixel(cx - 3, cy, (248, 184, 112))
    elif kind == "bathe":
        canvas.line(cx, cy - 11, cx - 7, cy + 1, (64, 136, 208), 3)
        canvas.line(cx - 7, cy + 1, cx, cy + 9, (64, 136, 208), 3)
        canvas.line(cx, cy + 9, cx + 7, cy + 1, (64, 136, 208), 3)
        canvas.line(cx + 7, cy + 1, cx, cy - 11, (64, 136, 208), 3)
        canvas.circle(cx + 10, cy - 8, 3, (112, 192, 224), False)
    elif kind == "play":
        canvas.circle(cx, cy, 10, (232, 72, 64), True)
        canvas.rect(cx - 10, cy - 2, 20, 4, (248, 248, 232))
        canvas.circle(cx, cy, 4, ink, True)
        canvas.circle(cx, cy, 2, (248, 248, 232), True)
    elif kind == "battle":
        canvas.line(cx - 10, cy + 9, cx + 8, cy - 10, (96, 112, 128), 4)
        canvas.line(cx + 10, cy + 9, cx - 8, cy - 10, (96, 112, 128), 4)
        canvas.line(cx - 11, cy + 5, cx - 5, cy + 11, (232, 120, 40), 3)
        canvas.line(cx + 11, cy + 5, cx + 5, cy + 11, (232, 120, 40), 3)
    elif kind == "box":
        canvas.rect(cx - 10, cy - 7, 20, 16, (184, 120, 64))
        canvas.rect(cx - 12, cy - 10, 24, 5, (232, 176, 88))
        canvas.rect(cx - 2, cy - 10, 4, 19, ink)
        canvas.rect(cx - 4, cy - 2, 8, 4, (248, 224, 136))
    elif kind == "dex":
        canvas.rect(cx - 9, cy - 12, 18, 24, (200, 56, 48))
        canvas.rect(cx - 6, cy - 9, 12, 7, (104, 184, 200))
        canvas.circle(cx, cy + 5, 4, (248, 248, 232), True)
        canvas.circle(cx, cy + 5, 2, ink, True)
    elif kind == "switch":
        canvas.circle(cx - 8, cy - 5, 5, (88, 168, 104), True)
        canvas.circle(cx + 7, cy - 5, 5, (232, 144, 64), True)
        canvas.circle(cx, cy + 8, 5, (104, 160, 208), True)
        canvas.line(cx - 3, cy + 1, cx + 3, cy + 1, ink, 2)
        canvas.line(cx + 3, cy + 1, cx + 1, cy - 2, ink, 2)
    elif kind == "run":
        canvas.line(cx - 9, cy - 8, cx + 1, cy + 2, (112, 104, 88), 5)
        canvas.line(cx + 1, cy + 2, cx + 10, cy + 6, (112, 104, 88), 5)
        canvas.line(cx - 2, cy + 8, cx + 10, cy + 8, ink, 2)


def render_home(rom: bytes, output_path: Path, icon_frame: int = 0) -> None:
    bulbasaur, bulbasaur_palette = extract_icon(rom, 1, icon_frame)
    pidgey, pidgey_palette = extract_icon(rom, 16, icon_frame)
    rattata, rattata_palette = extract_icon(rom, 19, icon_frame)
    canvas = Canvas(WIDTH, HEIGHT, (216, 232, 176))
    # FireRed-like route backdrop: light grass, sparse darker tufts, and a
    # warm, double-line dialogue/menu frame.
    canvas.rect(0, 0, WIDTH, 22, (48, 88, 120))
    canvas.rect(0, 19, WIDTH, 3, (112, 168, 184))
    canvas.text(8, 5, "BULBASAUR", (248, 248, 240), 1)
    canvas.text(111, 5, "LV 5", (248, 248, 240), 1)
    canvas.text(252, 5, "CHG 3/3", (248, 248, 240), 1)
    for y in range(31, 144, 16):
        for x in range((y // 16 % 2) * 10, WIDTH, 24):
            canvas.rect(x, y, 2, 5, (152, 192, 112))
            canvas.pixel(x - 1, y + 3, (152, 192, 112))
            canvas.pixel(x + 2, y + 2, (152, 192, 112))
    # The selected partner is prominent; the other two active team members
    # share the home scene instead of being reduced to abstract slot markers.
    blit_icon(canvas, 112, 39, bulbasaur, bulbasaur_palette, scale=3)
    blit_icon(canvas, 46, 79, pidgey, pidgey_palette, scale=2)
    blit_icon(canvas, 236, 78, rattata, rattata_palette, scale=2)

    canvas.frame(6, 29, 82, 48, (248, 248, 232))
    canvas.text(14, 38, "HP", (56, 64, 72), 1)
    canvas.rect(32, 39, 47, 7, (72, 80, 72))
    canvas.rect(34, 41, 43, 3, (64, 184, 88))
    canvas.text(14, 54, "20/20", (56, 64, 72), 1)

    canvas.frame(5, 151, 310, 84, (248, 248, 232))
    actions = ("feed", "bathe", "play", "battle", "box", "dex")
    for index, action in enumerate(actions):
        x = 11 + index * 50
        canvas.frame(x, 162, 44, 42, (240, 240, 216))
        draw_icon(canvas, action, x + 22, 183)
    canvas.text(16, 216, "STATUS OK", (48, 112, 64), 1)
    canvas.text(245, 216, "TEAM 3/3", (48, 56, 64), 1)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.png(output_path)


def render_battle(rom: bytes, output_path: Path) -> None:
    bulbasaur, bulbasaur_palette = extract_battle_sprite(rom, 1, BACK_SPRITE_TABLE)
    rattata, rattata_palette = extract_species(rom, 19)
    canvas = Canvas(WIDTH, HEIGHT, (232, 240, 208))
    canvas.rect(0, 0, WIDTH, 22, (48, 88, 120))
    canvas.rect(0, 19, WIDTH, 3, (112, 168, 184))
    canvas.text(8, 5, "WILD ENCOUNTER", (248, 248, 240), 1)
    for y in range(30, 151, 15):
        for x in range((y // 15 % 2) * 9, WIDTH, 22):
            canvas.rect(x, y, 2, 4, (176, 208, 136))

    canvas.frame(7, 29, 132, 48, (248, 248, 232))
    canvas.text(15, 37, "RATTATA  LV 3", (48, 56, 64), 1)
    canvas.text(15, 55, "HP", (48, 56, 64), 1)
    canvas.rect(34, 56, 94, 7, (72, 80, 72))
    canvas.rect(36, 58, 90, 3, (64, 184, 88))
    canvas.blit_indexed(204, 24, rattata, rattata_palette, scale=2)

    canvas.blit_indexed(7, 62, bulbasaur, bulbasaur_palette, scale=2)
    canvas.frame(170, 103, 143, 46, (248, 248, 232))
    canvas.text(179, 111, "BULBASAUR  LV 5", (48, 56, 64), 1)
    canvas.text(179, 129, "HP", (48, 56, 64), 1)
    canvas.rect(198, 130, 102, 7, (72, 80, 72))
    canvas.rect(200, 132, 98, 3, (64, 184, 88))

    canvas.frame(5, 151, 310, 34, (248, 248, 232))
    canvas.text(17, 164, "A WILD RATTATA APPEARED!", (48, 56, 64), 1)
    actions = ("battle", "play", "switch", "run")
    for index, action in enumerate(actions):
        x = 10 + index * 78
        canvas.frame(x, 189, 68, 45, (240, 240, 216))
        draw_icon(canvas, action, x + 34, 211)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.png(output_path)


def render(rom_path: Path, output_path: Path, mode: str = "home") -> None:
    rom = rom_path.read_bytes()
    digest = hashlib.sha1(rom).hexdigest().upper()
    if digest != EXPECTED_SHA1:
        raise ValueError(f"Unsupported ROM SHA-1: {digest}")
    if mode == "home":
        render_home(rom, output_path, 0)
    elif mode == "home-frame-2":
        render_home(rom, output_path, 1)
    elif mode == "battle":
        render_battle(rom, output_path)
    else:
        raise ValueError(f"Unknown mode: {mode}")


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        raise SystemExit("usage: render_firered_mockup.py ROM.gba OUTPUT.png [home|home-frame-2|battle]")
    render(Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3] if len(sys.argv) == 4 else "home")
