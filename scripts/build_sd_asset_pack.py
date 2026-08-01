"""Build a local-only RGB565 asset pack for the Pokegochi microSD card."""

from __future__ import annotations

import hashlib
import json
import struct
from pathlib import Path

from PIL import Image
from render_firered_mockup import (
    BACK_SPRITE_TABLE, EXPECTED_SHA1, extract_battle_sprite, extract_icon,
)

ROOT = Path(__file__).resolve().parent.parent
ROM = ROOT / "rom/extracted/Pokemon - FireRed Version (USA, Europe).gba"
DECOMP = ROOT / ".downloads/pokefirered-tree/pokefirered-master"
OUTPUT = ROOT / ".generated/sdcard/pokegochi/assets"
TRANSPARENT = 0xF81F


def indexed(indices: list[int], palette: list[tuple[int, int, int]], size: int) -> Image.Image:
    image = Image.new("RGBA", (size, size))
    image.putdata([(*palette[value], 0 if value == 0 else 255) for value in indices])
    return image


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


def rgb565(red: int, green: int, blue: int) -> int:
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def write_asset(relative: str, image: Image.Image, manifest: dict) -> None:
    image = image.convert("RGBA")
    destination = OUTPUT / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    pixels = []
    for red, green, blue, alpha in image.getdata():
        value = TRANSPARENT if alpha < 128 else rgb565(red, green, blue)
        if value == TRANSPARENT and alpha >= 128:
            value = 0xF81E
        pixels.append(value)
    destination.write_bytes(struct.pack("<4sHHH", b"PKG1", image.width, image.height, TRANSPARENT) +
                            struct.pack(f"<{len(pixels)}H", *pixels))
    manifest[relative] = {"width": image.width, "height": image.height}


def main() -> None:
    rom = ROM.read_bytes()
    if hashlib.sha1(rom).hexdigest().upper() != EXPECTED_SHA1:
        raise ValueError("Unexpected FireRed ROM")
    manifest: dict[str, dict] = {}
    for species in range(1, 152):
        for frame in range(2):
            indices, palette = extract_icon(rom, species, frame)
            write_asset(f"pokemon/icons/{species:03}_{frame}.pkg", indexed(indices, palette, 32), manifest)
        front_indices, front_palette = extract_battle_sprite(rom, species, 0x2350AC)
        back_indices, back_palette = extract_battle_sprite(rom, species, BACK_SPRITE_TABLE)
        write_asset(f"pokemon/front/{species:03}.pkg", indexed(front_indices, front_palette, 64), manifest)
        write_asset(f"pokemon/back/{species:03}.pkg", indexed(back_indices, back_palette, 64), manifest)

    trainer_dir = DECOMP / "graphics/trainers/front_pics"
    for path in trainer_dir.glob("*.png"):
        write_asset(f"trainers/{path.stem}.pkg", source_png(path), manifest)

    for folder in ("graphics/battle_anims/sprites", "graphics/battle_interface",
                   "graphics/pokemon_storage", "graphics/pokedex", "graphics/evolution_scene"):
        for path in (DECOMP / folder).glob("*.png"):
            image = source_png(path)
            base = f"firered/{folder.removeprefix('graphics/')}/{path.stem}"
            write_asset(f"{base}.pkg", image, manifest)
            if folder == "graphics/battle_anims/sprites" and image.height > image.width and image.height % image.width == 0:
                for frame in range(min(32, image.height // image.width)):
                    write_asset(f"{base}_{frame:02}.pkg",
                                image.crop((0, frame * image.width, image.width, (frame + 1) * image.width)), manifest)

    # FireRed Bag: keep the original background tile and pocket frames.  The
    # tiny home icon is a nearest-neighbour adaptation of the original sprite.
    bag = source_png(DECOMP / "graphics/interface/bag_male.png")
    write_asset("firered/item_menu/bg.pkg", source_png(DECOMP / "graphics/item_menu/bg.png"), manifest)
    for frame in range(bag.height // 64):
        pocket = bag.crop((0, frame * 64, 64, (frame + 1) * 64))
        write_asset(f"firered/item_menu/bag_{frame}.pkg", pocket, manifest)
        if frame == 0:
            write_asset("firered/item_menu/bag_icon.pkg", pocket.resize((20, 20), Image.Resampling.NEAREST), manifest)

    (OUTPUT / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"Built {len(manifest)} local assets in {OUTPUT}")


if __name__ == "__main__":
    main()
