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


def compose_tilemap(tiles_path: Path, map_path: Path, columns: int = 64) -> Image.Image:
    tiles = Image.open(tiles_path).convert("RGBA")
    values = struct.unpack(f"<{map_path.stat().st_size // 2}H", map_path.read_bytes())
    rows = len(values) // columns
    output = Image.new("RGBA", (columns * 8, rows * 8), (0, 0, 0, 255))
    tiles_per_row = tiles.width // 8
    for index, entry in enumerate(values):
        tile_id = entry & 0x3FF; tx = tile_id % tiles_per_row * 8; ty = tile_id // tiles_per_row * 8
        if ty + 8 > tiles.height: continue
        tile = tiles.crop((tx, ty, tx + 8, ty + 8))
        if entry & 0x400: tile = tile.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        if entry & 0x800: tile = tile.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        output.alpha_composite(tile, ((index % columns) * 8, (index // columns) * 8))
    return output


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
    for ball in ("poke_ball", "great_ball", "ultra_ball", "master_ball"):
        path=DECOMP/"graphics/items/icons"/f"{ball}.png"
        if path.exists():write_asset(f"firered/items/{ball}.pkg",source_png(path),manifest)
    for terrain in ("grass", "indoor"):
        folder=DECOMP/"graphics/battle_terrain"/terrain
        composed=compose_tilemap(folder/"terrain.png",folder/"terrain.bin")
        viewport=composed.crop((0,0,240,160)).resize((320,184),Image.Resampling.NEAREST)
        write_asset(f"firered/battle_terrain/{terrain}.pkg",viewport,manifest)
    dex_reference=Image.open(ROOT/".downloads/pokedex-icon-reference.jpg").convert("RGBA")
    dex_reference.thumbnail((20,20),Image.Resampling.NEAREST)
    write_asset("firered/ui/pokedex_icon.pkg",dex_reference,manifest)
    box_source=source_png(DECOMP/"graphics/pokemon_storage/menu.png")
    write_asset("firered/ui/box_icon.pkg",box_source.resize((20,20),Image.Resampling.NEAREST),manifest)
    shop=compose_tilemap(DECOMP/"graphics/shop_menu/shop_menu.png",DECOMP/"graphics/shop_menu/shop_tilemap.bin",32)
    write_asset("firered/shop/background.pkg",shop.crop((0,0,240,160)).resize((320,240),Image.Resampling.NEAREST),manifest)

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

    # FireRed Bag: keep the original background tile and pocket frames.  The
    # tiny home icon is a nearest-neighbour adaptation of the original sprite.
    bag = source_png(DECOMP / "graphics/interface/bag_male.png")
    write_asset("firered/item_menu/bg.pkg", source_png(DECOMP / "graphics/item_menu/bg.png"), manifest)
    for frame in range(bag.height // 64):
        pocket = bag.crop((0, frame * 64, 64, (frame + 1) * 64))
        write_asset(f"firered/item_menu/bag_{frame}.pkg", pocket, manifest)
        if frame == 0:
            write_asset("firered/item_menu/bag_icon.pkg", pocket.resize((20, 20), Image.Resampling.NEAREST), manifest)

    required=[*(f"pokemon/front/{i:03}.pkg" for i in range(1,152)),*(f"pokemon/back/{i:03}.pkg" for i in range(1,152)),
      *(f"pokemon/icons/{i:03}_{frame}.pkg" for i in range(1,152) for frame in range(2)),"firered/item_menu/bag_icon.pkg",
      "firered/ui/pokedex_icon.pkg","firered/ui/box_icon.pkg","firered/battle_terrain/grass.pkg","firered/battle_terrain/indoor.pkg",
      "firered/shop/background.pkg","firered/items/poke_ball.pkg","firered/items/great_ball.pkg","firered/items/ultra_ball.pkg"]
    missing=[name for name in required if name not in manifest]
    trainer_count=sum(1 for name in manifest if name.startswith("trainers/"))
    if missing or trainer_count<100:raise RuntimeError(f"Asset audit failed: {len(missing)} missing, {trainer_count} trainers")
    (OUTPUT / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    (OUTPUT / "asset_audit.json").write_text(json.dumps({"assets":len(manifest),"trainers":trainer_count,"required":len(required),"missing":missing},indent=2),encoding="utf-8")
    print(f"Built {len(manifest)} local assets in {OUTPUT}")


if __name__ == "__main__":
    main()
