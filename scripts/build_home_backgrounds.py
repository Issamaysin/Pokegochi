"""Build Pokegochi Home backgrounds from exact FireRed/Emerald map pixels.

The firmware assets produced here are direct crops of maps decoded from the
pret decomp projects.  No generated scenery, painted pixels or resampling is
used in the backgrounds.
"""

from __future__ import annotations

import json
import itertools
import re
import struct
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from render_firered_mockup import extract_icon


ROOT = Path(__file__).resolve().parent.parent
FIRERED = ROOT / ".downloads/pokefirered-tree/pokefirered-master"
EMERALD = ROOT / ".downloads/pokeemerald-tree/pokeemerald-master"
ROM = ROOT / "rom/extracted/Pokemon - FireRed Version (USA, Europe).gba"
PREVIEW_DIR = ROOT / "docs/mockups/home-backgrounds"
VIEWPORT = (304, 134)
FIRMWARE_DATA = ROOT / "src/game/HomeBackgroundDataGenerated.inc"


@dataclass(frozen=True)
class Background:
    key: str
    label: str
    game: str
    layout: str
    crop_x: int
    crop_y: int


@dataclass(frozen=True)
class MapLayers:
    base: Image.Image
    foreground: Image.Image
    composite: Image.Image


@dataclass(frozen=True)
class PetRoute:
    x0: int
    y0: int
    x1: int
    y1: int


# Crop coordinates are in native GBA map pixels. Every viewport remains 1:1.
BACKGROUNDS = (
    Background("fr_viridian_forest", "VIRIDIAN FOREST", "firered", "ViridianForest", 32, 416),
    Background("fr_pokemon_mansion", "POKEMON MANSION", "firered", "PokemonMansion_1F", 288, 128),
    Background("fr_mt_moon", "MT. MOON", "firered", "MtMoon_1F", 224, 256),
    Background("fr_cerulean", "CERULEAN CITY", "firered", "CeruleanCity", 224, 224),
    Background("fr_vermilion", "VERMILION CITY", "firered", "VermilionCity", 224, 368),
    Background("fr_safari", "SAFARI ZONE", "firered", "SafariZone_Center", 256, 240),
    Background("fr_seafoam", "SEAFOAM ISLANDS", "firered", "SeafoamIslands_1F", 144, 112),
    Background("fr_cinnabar", "CINNABAR ISLAND", "firered", "CinnabarIsland", 32, 128),
    Background("fr_mt_ember", "MT. EMBER", "firered", "MtEmber_Summit", 0, 112),
    Background("fr_power_plant", "POWER PLANT", "firered", "PowerPlant", 320, 496),
    Background("em_pacifidlog", "PACIFIDLOG TOWN", "emerald", "PacifidlogTown", 0, 192),
    Background("em_shoal_ice", "SHOAL ICE CAVE", "emerald", "ShoalCave_LowTideIceRoom", 0, 160),
    Background("em_dewford", "DEWFORD TOWN", "emerald", "DewfordTown", 0, 128),
    Background("em_granite_cave", "GRANITE CAVE", "emerald", "GraniteCave_1F", 176, 48),
    Background("em_route_113", "ROUTE 113 ASH", "emerald", "Route113", 448, 16),
    Background("em_meteor_falls", "METEOR FALLS", "emerald", "MeteorFalls_1F_1R", 0, 400),
    Background("em_fortree", "FORTREE CITY", "emerald", "FortreeCity", 256, 16),
    Background("fr_pokemon_tower", "POKEMON TOWER", "firered", "PokemonTower_5F", 0, 128),
    Background("em_sootopolis", "SOOTOPOLIS CITY", "emerald", "SootopolisCity", 448, 288),
    Background("em_mt_chimney", "MT. CHIMNEY", "emerald", "MtChimney", 160, 144),
    Background("em_frontier_dome", "BATTLE DOME PLAZA", "emerald",
               "BattleFrontier_OutsideWest", 80, 240),
    Background("em_frontier_tower", "BATTLE TOWER PLAZA", "emerald",
               "BattleFrontier_OutsideEast", 64, 176),
    Background("em_frontier_pyramid", "BATTLE PYRAMID PLAZA", "emerald",
               "BattleFrontier_OutsideEast", 704, 192),
    Background("em_frontier_dome_lobby", "BATTLE DOME LOBBY", "emerald",
               "BattleFrontier_BattleDomeLobby", 32, 96),
    Background("em_frontier_dome_arena", "BATTLE DOME ARENA", "emerald",
               "BattleFrontier_BattleDomeBattleRoom", 0, 16),
    Background("em_frontier_pike", "BATTLE PIKE", "emerald",
               "BattleFrontier_OutsideWest", 528, 384),
    Background("em_frontier_factory_lobby", "BATTLE FACTORY LOBBY", "emerald",
               "BattleFrontier_BattleFactoryLobby", 0, 32),
    Background("em_frontier_palace_garden", "BATTLE PALACE GARDEN", "emerald",
               "BattleFrontier_OutsideEast", 512, 672),
    Background("em_frontier_arena", "BATTLE ARENA", "emerald",
               "BattleFrontier_OutsideEast", 432, 400),
    Background("em_southern_island", "SOUTHERN ISLAND", "emerald",
               "SouthernIsland_Exterior", 112, 64),
    # Additional scenes are appended rather than inserted into the three
    # original groups.  Home background ids are stored in the save, so keeping
    # ids 0..29 stable prevents an existing selection from silently changing.
    Background("fr_ember_spa", "EMBER SPA", "firered",
               "OneIsland_KindleRoad_EmberSpa", 64, 48),
    Background("fr_icefall_grotto", "ICEFALL GROTTO", "firered",
               "FourIsland_IcefallCave_Back", 48, 128),
    Background("fr_indigo_plateau", "INDIGO PLATEAU", "firered",
               "IndigoPlateau_Exterior", 32, 80),
    Background("em_route_111_desert", "ROUTE 111 DESERT", "emerald",
               "Route111", 160, 1040),
    Background("em_sky_pillar_summit", "SKY PILLAR SUMMIT", "emerald",
               "SkyPillar_Top", 64, 64),
    Background("em_faraway_island", "FARAWAY ISLAND", "emerald",
               "FarawayIsland_Interior", 80, 128),
    Background("em_frontier_pyramid_top", "BATTLE PYRAMID TOP", "emerald",
               "BattleFrontier_BattlePyramidTop", 112, 48),
    Background("em_frontier_ranking_hall", "RANKING HALL", "emerald",
               "BattleFrontier_RankingHall", 272, 48),
    Background("em_frontier_tower_lobby", "BATTLE TOWER LOBBY", "emerald",
               "BattleFrontier_BattleTowerLobby", 48, 0),
)


def _snake(symbol: str) -> str:
    value = symbol.removeprefix("gTileset_")
    return re.sub(r"(?<!^)(?=[A-Z])", "_", value).lower().replace("islands123", "islands_123")


def _palette(path: Path) -> list[tuple[int, int, int]]:
    lines = path.read_text(encoding="ascii").splitlines()
    return [tuple(map(int, line.split())) for line in lines[3:19]]


def _layout(root: Path, folder: str) -> dict:
    document = json.loads((root / "data/layouts/layouts.json").read_text(encoding="utf-8"))
    suffix = f"data/layouts/{folder}/map.bin"
    return next(entry for entry in document["layouts"] if entry.get("blockdata_filepath") == suffix)


def render_map_layers(root: Path, folder: str) -> MapLayers:
    """Decode the native lower and upper metatile layers independently."""
    layout = _layout(root, folder)
    primary = root / "data/tilesets/primary" / _snake(layout["primary_tileset"])
    secondary = root / "data/tilesets/secondary" / _snake(layout["secondary_tileset"])
    primary_tiles = Image.open(primary / "tiles.png")
    secondary_tiles = Image.open(secondary / "tiles.png")
    primary_metatiles = (primary / "metatiles.bin").read_bytes()
    secondary_metatiles = (secondary / "metatiles.bin").read_bytes()
    # Map block ids use a generation-wide boundary for secondary metatiles,
    # not the number of entries physically present in the selected primary
    # file.  Emerald's compact indoor ``building`` set, for example, contains
    # only eight blocks while Battle Frontier secondary ids still begin at
    # 512. FireRed reserves 640 primary ids.
    primary_block_limit = 640 if root == FIRERED else 512
    primary_tile_count = primary_tiles.width // 8 * (primary_tiles.height // 8)
    primary_palettes = [_palette(primary / "palettes" / f"{index:02}.pal") for index in range(16)]
    secondary_palettes = [_palette(secondary / "palettes" / f"{index:02}.pal") for index in range(16)]

    def read_attributes(folder: Path, metatile_count: int) -> tuple[int, ...]:
        raw = (folder / "metatile_attributes.bin").read_bytes()
        size = len(raw) // metatile_count
        if size not in (2, 4):
            raise ValueError(f"Unsupported metatile attribute size {size} in {folder}")
        return struct.unpack(f"<{metatile_count}{'H' if size == 2 else 'I'}", raw)

    primary_attributes = read_attributes(primary, len(primary_metatiles) // 16)
    secondary_attributes = read_attributes(secondary, len(secondary_metatiles) // 16)
    width, height = layout["width"], layout["height"]
    blocks = struct.unpack(f"<{width * height}H", (root / layout["blockdata_filepath"]).read_bytes())
    base = Image.new("RGBA", (width * 16, height * 16), (0, 0, 0, 255))
    foreground = Image.new("RGBA", (width * 16, height * 16), (0, 0, 0, 0))

    def tile_image(reference: int, secondary_block: bool,
                   transparent_zero: bool) -> Image.Image:
        tile_number = reference & 0x03FF
        palette_number = (reference >> 12) & 0x0F
        if transparent_zero and tile_number == 0:
            return Image.new("RGBA", (8, 8), (0, 0, 0, 0))
        secondary_tile = tile_number >= primary_tile_count
        sheet = secondary_tiles if secondary_tile else primary_tiles
        local_number = tile_number - primary_tile_count if secondary_tile else tile_number
        primary_palette_limit = 7 if root == FIRERED else 6
        # Palette banks are selected by the tile reference itself. Secondary
        # metatiles routinely reuse primary banks, so choosing a palette from
        # the metatile's origin corrupts their transparent/foreground pixels.
        palettes = (primary_palettes if palette_number < primary_palette_limit
                    else secondary_palettes)
        tile = sheet.crop(((local_number % 16) * 8, (local_number // 16) * 8,
                           (local_number % 16 + 1) * 8, (local_number // 16 + 1) * 8))
        rgba = Image.new("RGBA", (8, 8))
        colors = palettes[palette_number]
        rgba.putdata([(*colors[index], 0 if transparent_zero and index == 0 else 255)
                      for index in tile.get_flattened_data()])
        if reference & 0x0400:
            rgba = rgba.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        if reference & 0x0800:
            rgba = rgba.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        return rgba

    normal_backdrop_tile = tile_image(0x3014, False, False)

    for block_y in range(height):
        for block_x in range(width):
            block_id = blocks[block_y * width + block_x] & 0x03FF
            secondary_block = block_id >= primary_block_limit
            local_id = block_id - primary_block_limit if secondary_block else block_id
            metatiles = secondary_metatiles if secondary_block else primary_metatiles
            attributes = secondary_attributes if secondary_block else primary_attributes
            references = struct.unpack_from("<8H", metatiles, local_id * 16)
            attribute = attributes[local_id]
            layer_type = ((attribute >> 29) & 0x03) if root == FIRERED else ((attribute >> 12) & 0x0F)
            for quadrant in range(4):
                target_xy = (block_x * 16 + (quadrant % 2) * 8,
                             block_y * 16 + (quadrant // 2) * 8)
                # NORMAL metatiles place their bottom half on BG2. The game
                # fills BG3 below it with tile 0x3014; reproducing that layer
                # prevents transparent floor pixels from becoming black.
                if layer_type == 0:
                    base.alpha_composite(normal_backdrop_tile, target_xy)
                bottom = tile_image(references[quadrant], secondary_block, True)
                top = tile_image(references[4 + quadrant], secondary_block, True)
                base.alpha_composite(bottom, target_xy)
                if layer_type == 1:  # COVERED: both layers remain behind sprites.
                    base.alpha_composite(top, target_xy)
                else:  # NORMAL/SPLIT: the top layer can occlude walking Pokemon.
                    foreground.alpha_composite(top, target_xy)
    composite = base.copy()
    composite.alpha_composite(foreground)
    return MapLayers(base, foreground, composite)


def render_map(root: Path, folder: str) -> Image.Image:
    return render_map_layers(root, folder).composite


def collision_grid(root: Path, background: Background) -> list[list[bool]]:
    """Read collision plus terrain behavior for the 19x8 visible play grid."""
    if background.crop_x % 16 or background.crop_y % 16:
        raise ValueError(f"Background crop must align to metatiles: {background.key}")
    layout = _layout(root, background.layout)
    width, height = layout["width"], layout["height"]
    blocks = struct.unpack(f"<{width * height}H", (root / layout["blockdata_filepath"]).read_bytes())
    primary = root / "data/tilesets/primary" / _snake(layout["primary_tileset"])
    secondary = root / "data/tilesets/secondary" / _snake(layout["secondary_tileset"])
    primary_block_limit = 640 if root == FIRERED else 512

    def attributes(folder: Path) -> tuple[tuple[int, ...], int]:
        raw = (folder / "metatile_attributes.bin").read_bytes()
        count = (folder / "metatiles.bin").stat().st_size // 16
        size = len(raw) // count
        if size not in (2, 4):
            raise ValueError(f"Unsupported metatile attribute size {size} in {folder}")
        return struct.unpack(f"<{count}{'H' if size == 2 else 'I'}", raw), count

    primary_attributes, _ = attributes(primary)
    secondary_attributes, secondary_count = attributes(secondary)

    def is_land(entry: int) -> bool:
        block_id = entry & 0x03FF
        secondary_block = block_id >= primary_block_limit
        local_id = block_id - primary_block_limit if secondary_block else block_id
        if secondary_block and local_id >= secondary_count:
            return False
        value = secondary_attributes[local_id] if secondary_block else primary_attributes[local_id]
        behavior = value & 0xFF
        water_or_current = 0x10 <= behavior <= 0x1F or 0x50 <= behavior <= 0x53
        return ((entry >> 10) & 0x03) == 0 and not water_or_current

    origin_x, origin_y = background.crop_x // 16, background.crop_y // 16
    grid = [[is_land(blocks[(origin_y + y) * width + origin_x + x])
             for x in range(19)] for y in range(8)]

    # The Pyramid Top's orange sky is a decorative metatile backdrop with
    # collision 0 in Emerald because the player can never reach it.  In the
    # cropped Home scene that metadata would incorrectly let partners walk in
    # the sky and over the sun.  Restrict navigation to the visible pyramid.
    if background.key == "em_frontier_pyramid_top":
        grid = [[False] * 19 for _ in range(8)]
        for x in range(7, 12):
            grid[6][x] = True
        for x in range(5, 14):
            grid[7][x] = True
    return grid


def pet_routes(root: Path, background: Background) -> tuple[PetRoute, PetRoute, PetRoute]:
    """Derive three separated spawn points on the runtime navigation area."""
    grid = collision_grid(root, background)

    # The Battle Pike's head is the visual focus of this crop. Its two upper
    # walkable strips sit underneath foreground metatiles from the building,
    # so the generic farthest-point picker can place a partner behind the
    # Seviper facade. Keep all three partners on the open plaza below its
    # mouth while preserving a valid one-tile walking segment for each one.
    if background.key == "em_frontier_pike":
        return (
            PetRoute(104, 88, 120, 88),
            PetRoute(152, 120, 152, 104),
            PetRoute(200, 88, 184, 88),
        )

    def navigable_neighbours(x: int, y: int) -> list[tuple[int, int]]:
        return [(nx, ny) for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1))
                if 1 <= nx < 18 and 2 <= ny < 8 and grid[ny][nx]]

    unvisited = {(x, y) for y in range(2, 8) for x in range(1, 18) if grid[y][x]}
    components: list[set[tuple[int, int]]] = []
    while unvisited:
        seed = unvisited.pop()
        component = {seed}
        pending = [seed]
        while pending:
            x, y = pending.pop()
            for neighbour in navigable_neighbours(x, y):
                if neighbour not in unvisited:
                    continue
                unvisited.remove(neighbour)
                component.add(neighbour)
                pending.append(neighbour)
        components.append(component)
    # All three partners share the largest connected walking area. Choosing
    # merely the three most distant walkable cells could strand one of them
    # in a decorative two-tile pocket even though the main path was spacious.
    main_component = max(components, key=lambda points: (
        len(points), max(x for x, _ in points) - min(x for x, _ in points)))
    candidates = sorted(point for point in main_component if navigable_neighbours(*point))
    if len(candidates) < 3:
        raise ValueError(f"Not enough walkable Home positions in {background.key}")

    def distance_sq(a: tuple[int, int], b: tuple[int, int]) -> int:
        # Horizontal separation matters most for the three-character layout.
        return (a[0] - b[0]) ** 2 * 4 + (a[1] - b[1]) ** 2

    anchors = max(itertools.combinations(candidates, 3),
                  key=lambda points: (min(distance_sq(points[0], points[1]),
                                          distance_sq(points[0], points[2]),
                                          distance_sq(points[1], points[2])),
                                      sum(distance_sq(a, b) for a, b in itertools.combinations(points, 2))))
    anchors = tuple(sorted(anchors))
    routes: list[PetRoute] = []
    for index, anchor in enumerate(anchors):
        x, y = anchor
        neighbours = navigable_neighbours(x, y)
        if neighbours:
            other_anchors = anchors[:index] + anchors[index + 1:]
            target = max(neighbours, key=lambda point: (
                min(distance_sq(point, other) for other in other_anchors),
                point[1] == y))
        else:
            target = anchor
        # Coordinates represent the feet of the 32px icon, centered in a safe block.
        routes.append(PetRoute(x * 16 + 8, y * 16 + 8, target[0] * 16 + 8, target[1] * 16 + 8))
    return tuple(routes)  # type: ignore[return-value]


def build_background_layers() -> dict[str, MapLayers]:
    rendered_maps: dict[tuple[str, str], MapLayers] = {}
    images: dict[str, MapLayers] = {}
    for background in BACKGROUNDS:
        root = FIRERED if background.game == "firered" else EMERALD
        cache_key = (background.game, background.layout)
        if cache_key not in rendered_maps:
            rendered_maps[cache_key] = render_map_layers(root, background.layout)
        source = rendered_maps[cache_key]
        x, y = background.crop_x, background.crop_y
        box = (x, y, x + VIEWPORT[0], y + VIEWPORT[1])
        base = source.base.crop(box)
        foreground = source.foreground.crop(box)
        composite = source.composite.crop(box)
        if background.key == "em_frontier_pyramid_top":
            # Pyramid steps are encoded as upper map layers for the player
            # climbing animation.  Home partners instead walk across the
            # facade, so flatten the steps into the base; otherwise nearly the
            # entire 32px icon would disappear behind the next stair.
            base = composite.copy()
            foreground = Image.new("RGBA", VIEWPORT, (0, 0, 0, 0))
        images[background.key] = MapLayers(base, foreground, composite)
    return images


def build_background_images() -> dict[str, Image.Image]:
    return {key: layers.composite for key, layers in build_background_layers().items()}


def write_firmware_metadata() -> None:
    lines = ["// Generated by scripts/build_home_backgrounds.py. Do not edit."]
    for background in BACKGROUNDS:
        root = FIRERED if background.game == "firered" else EMERALD
        routes = pet_routes(root, background)
        grid = collision_grid(root, background)
        route_text = ", ".join(f"{{{r.x0}, {r.y0}, {r.x1}, {r.y1}}}" for r in routes)
        row_text = ", ".join(
            f"0x{sum((1 << x) for x, open_cell in enumerate(row) if open_cell):08X}UL"
            for row in grid
        )
        lines.append(
            f'{{"{background.label}", "{background.game.upper()}", '
            f'{{{route_text}}}, {{{row_text}}}}},'
        )
    FIRMWARE_DATA.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _contact_sheet(items: list[Background], images: dict[str, Image.Image], destination: Path) -> None:
    scale = 2
    card_w, card_h = VIEWPORT[0] * scale, VIEWPORT[1] * scale + 28
    sheet = Image.new("RGB", (card_w * 2, card_h * 5), "#202028")
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default(size=16)
    for index, background in enumerate(items):
        x, y = index % 2 * card_w, index // 2 * card_h
        preview = images[background.key].resize((VIEWPORT[0] * scale, VIEWPORT[1] * scale), Image.Resampling.NEAREST)
        sheet.paste(preview.convert("RGB"), (x, y))
        draw.text((x + 8, y + VIEWPORT[1] * scale + 5), f"{index + 1:02}. {background.label}", fill="white", font=font)
    destination.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(destination, optimize=True)


def _pokemon_previews(layers: dict[str, MapLayers]) -> dict[str, Image.Image]:
    rom = ROM.read_bytes()
    icons = []
    for species in (1, 4, 7):
        indices, palette = extract_icon(rom, species, 0)
        icon = Image.new("RGBA", (32, 32))
        icon.putdata([(*palette[value], 0 if value == 0 else 255) for value in indices])
        icons.append(icon)
    previews: dict[str, Image.Image] = {}
    for background in BACKGROUNDS:
        root = FIRERED if background.game == "firered" else EMERALD
        preview = layers[background.key].base.copy()
        for icon, route in zip(icons, pet_routes(root, background)):
            preview.alpha_composite(icon, (route.x0 - 16, route.y0 - 30))
        preview.alpha_composite(layers[background.key].foreground)
        previews[background.key] = preview
    return previews


def write_previews() -> None:
    layers = build_background_layers()
    images = {key: value.composite for key, value in layers.items()}
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    for background in BACKGROUNDS:
        images[background.key].convert("RGB").save(PREVIEW_DIR / f"{background.key}.png", optimize=True)
    _contact_sheet(list(BACKGROUNDS[:10]), images, PREVIEW_DIR / "fire-red-contact-sheet.png")
    _contact_sheet(list(BACKGROUNDS[10:20]), images, PREVIEW_DIR / "emerald-contact-sheet.png")
    _contact_sheet(list(BACKGROUNDS[20:30]), images, PREVIEW_DIR / "frontier-contact-sheet.png")
    _contact_sheet(list(BACKGROUNDS[30:]), images, PREVIEW_DIR / "additional-contact-sheet.png")
    pokemon_previews = _pokemon_previews(layers)
    _contact_sheet(list(BACKGROUNDS[:10]), pokemon_previews, PREVIEW_DIR / "fire-red-with-pokemon.png")
    _contact_sheet(list(BACKGROUNDS[10:20]), pokemon_previews, PREVIEW_DIR / "emerald-with-pokemon.png")
    _contact_sheet(list(BACKGROUNDS[20:30]), pokemon_previews, PREVIEW_DIR / "frontier-with-pokemon.png")
    _contact_sheet(list(BACKGROUNDS[30:]), pokemon_previews, PREVIEW_DIR / "additional-with-pokemon.png")
    metadata = [{"id": index, "key": item.key, "name": item.label, "game": item.game,
                 "layout": item.layout, "crop": [item.crop_x, item.crop_y]}
                for index, item in enumerate(BACKGROUNDS)]
    (PREVIEW_DIR / "backgrounds.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    write_firmware_metadata()


if __name__ == "__main__":
    write_previews()
    print(f"Built {len(BACKGROUNDS)} exact-map previews in {PREVIEW_DIR}")
