"""Source-level regression contract for the extensible postgame region hub."""

from pathlib import Path
import re
import struct


ROOT = Path(__file__).resolve().parent.parent
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
PACKER = (ROOT / "scripts/build_sd_asset_pack.py").read_text(encoding="utf-8")
PACK_VERSION = (ROOT / "include/config/AssetPackVersion.h").read_text(
    encoding="utf-8"
)
ASSET_ROOT = ROOT / ".generated/sdcard/pokegochi/assets"


def source_between(signature: str, next_signature: str) -> str:
    start = MAIN.index(signature)
    end = MAIN.index(next_signature, start + len(signature))
    return MAIN[start:end]


def pkg_visible_bounds(relative: str) -> tuple[int, int, int, int]:
    raw = (ASSET_ROOT / relative).read_bytes()
    magic, width, height, transparent, bpp, palette_count = struct.unpack_from(
        "<4sHHHBB", raw, 0
    )
    assert magic == b"PKG2" and (width, height) == (32, 32) and bpp == 4
    palette = struct.unpack_from(f"<{palette_count}H", raw, 12)
    payload = raw[12 + palette_count * 2:]
    indices = [
        (payload[index // 2] >> 4) & 0x0F
        if index % 2 == 0 else payload[index // 2] & 0x0F
        for index in range(width * height)
    ]
    visible = [
        (index % width, index // width)
        for index, palette_index in enumerate(indices)
        if palette[palette_index] != transparent
    ]
    assert visible
    xs, ys = zip(*visible)
    return min(xs), min(ys), max(xs) + 1, max(ys) + 1


assert "ChallengeHub, RegionHub" in MAIN
assert 'case Screen::RegionHub: return "RegionHub";' in MAIN
assert 'case Screen::UnovaAdventure: return "UnovaAdventure";' in MAIN
assert 'case Screen::UnovaTournament: return "UnovaTournament";' in MAIN
assert "case Screen::RegionHub: drawRegionHub(); break;" in MAIN
assert "constexpr uint8_t kRegionHubDestinationsPerPage = 2U;" in MAIN
assert '"/pokegochi/assets/platinum/region_hub/home_icon.pkg"' in MAIN
assert (
    '"/pokegochi/assets/platinum/region_hub/sinnoh_adamant_orb.pkg"' in MAIN
)
assert "if (regionHubAvailable()) homeRegionHubButton(kHomeTopX[2]);" in MAIN
assert "screen=Screen::RegionHub;drawRegionHub();return;" in MAIN

region_hub = source_between("void drawRegionHub(){", "void drawTowerInvite(){")
card_layout = region_hub[region_hub.index("const int16_t cardX="):]
assert "const int16_t cardCenterX=" in card_layout
assert "AssetRenderer::draw(display,definition->iconPath,cardCenterX-16,63);" in card_layout
assert card_layout.index("fireRedText.setTextDatum(MC_DATUM);") < card_layout.index(
    "fireRedText.drawStringFitted(definition->name,cardCenterX"
)
assert "definition->descriptionLine1,\n                                 cardCenterX" in card_layout
assert "definition->descriptionLine2,\n                                 cardCenterX" in card_layout

pokedex = source_between(
    "void drawPokedex() {", "uint8_t pokedexDescriptionLineCount("
)
assert '"SINNOH"' not in pokedex
assert "SinnohExpedition::available" not in pokedex
assert 'button(250, 202, 55, 22, "BACK"' in pokedex

sinnoh = source_between("void drawSinnohExpedition() {", "void drawPokedexHeader() {")
assert 'button(7, 8, 52, 24, "BACK"' in sinnoh
assert '"RESEARCH %u - GYMS %u/8"' in sinnoh
assert 'badgeWon ? "BADGE WON" : "GYM"' in sinnoh
assert "SinnohExpedition::gymChallengeAvailable" in sinnoh
assert "SinnohExpedition::hasGymVictory" in sinnoh
assert re.search(
    r"screen\s*=\s*Screen::RegionHub;\s*drawRegionHub\(\);",
    MAIN,
)

assert 'PLATINUM / "res/items/icons/town_map.png"' in PACKER
assert 'PLATINUM / "res/items/icons/adamant_orb.png"' in PACKER
assert '"platinum/region_hub/home_icon.pkg"' in PACKER
assert '"platinum/region_hub/sinnoh_adamant_orb.pkg"' in PACKER
assert '"roark", "gardenia", "fantina", "maylene"' in PACKER
assert '"wake", "byron", "candice", "volkner"' in PACKER
assert 'f"trainers/sinnoh_{leader}.pkg"' in PACKER
assert 'f"platinum/trainer_card/badge_{badge}.pkg"' in PACKER
assert '"/pokegochi/assets/platinum/trainer_card/badge_%u.pkg"' in MAIN
assert "BattleKind::SinnohGym" in MAIN
assert '"unova/region_hub/unova_light_stone.pkg"' in PACKER
assert 'white_item_icon(616)' in PACKER
assert "def centered_region_marker(" in PACKER
assert "return centered_region_marker(image)" in PACKER
assert "centered_region_marker(white_item_icon(616))" in PACKER
for marker in (
    "platinum/region_hub/sinnoh_adamant_orb.pkg",
    "unova/region_hub/unova_light_stone.pkg",
):
    left, top, right, bottom = pkg_visible_bounds(marker)
    assert left == 32 - right and top == 32 - bottom, (
        marker, (left, top, right, bottom)
    )
assert '"unova/royal_unova/home_icon.pkg"' in PACKER
assert '"/pokegochi/assets/unova/region_hub/unova_light_stone.pkg"' in MAIN
assert '"/pokegochi/assets/unova/rooms/area_%02u.pkg"' in MAIN
assert '"/pokegochi/assets/unova/rooms/kind_%02u.pkg"' in MAIN
assert '"THE BAG IS SEALED IN THIS AREA."' in MAIN
assert 'button(8, 205, 304, 29, "ABANDON AREA", TFT_RED);' in MAIN
assert 'button(85, 204, 84, 31, "PARTY", themePrimary());' in MAIN
assert 'complete ? "REPLAY" : "ENTER AREA"' in MAIN
assert "UnovaAdventure::active(gameSave.unova)&&!gameSave.battle.active" in MAIN
assert "std::copy(std::begin(collection.party), std::end(collection.party)," in (
    ROOT / "src/game/UnovaAdventure.cpp"
).read_text(encoding="utf-8")
assert "UiColorTheme::White2" in MAIN
assert 'case UiColorTheme::White2: return "BLACK";' in MAIN
assert '"SAPPHIRE","RUBY","EMERALD","PLATINUM","BLACK"' in MAIN
assert '"UNOVA COMPLETE! BLACK THEME + ROYAL UNOVA!"' in MAIN
assert '"WHITE 2 MONO"' not in MAIN
assert '"WHITE 2 THEME + ROYAL UNOVA!"' not in MAIN
assert '"--region-hub-assets-only"' in PACKER
assert "merge_assets_into_archive(relatives, manifest)" in PACKER
assert "kRequiredAssetPackVersion = 41" in PACK_VERSION

print("Region hub contract passed.")
