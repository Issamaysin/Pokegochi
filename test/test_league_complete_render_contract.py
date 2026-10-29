import re
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
RENDERER = (ROOT / "src/drivers/AssetRenderer.cpp").read_text(encoding="utf-8")
ASSETS = ROOT / ".generated/sdcard/pokegochi/assets"


league_start = MAIN.index("void drawLeagueComplete()")
league_end = MAIN.index("void playBadgeAward(", league_start)
league_body = MAIN[league_start:league_end]
restore = league_body.index("AssetRenderer::restoreSceneWorkspace(false)")
manifest = league_body.index("prepareUiScene(")
assert restore < manifest, "LeagueComplete must restore the post-battle workspace first"
assert "!renderingRetainedFrame" in league_body[:manifest]

large_start = RENDERER.index("bool shouldBankLargeAsset(")
large_end = RENDERER.index("void clearHomeLayers()", large_start)
assert '"/pokemon_league/victory_bg.pkg"' in RENDERER[large_start:large_end]

version_header = (ROOT / "include/config/AssetPackVersion.h").read_text(
    encoding="ascii"
)
assert re.search(r"kRequiredAssetPackVersion\s*=\s*\d+", version_header)

for relative in (
    "firered/pokemon_league/victory_bg.pkg",
    "emerald/pokemon_league/victory_bg.pkg",
):
    payload = (ASSETS / relative).read_bytes()
    magic, width, height, _transparent = struct.unpack_from("<4sHHH", payload)
    bits_per_pixel, palette_count = struct.unpack_from("<BB", payload, 10)
    pixel_bytes = (width * height * bits_per_pixel + 7) // 8
    assert magic == b"PKG2" and (width, height) == (320, 240)
    assert bits_per_pixel in (4, 6, 8) and palette_count > 0
    assert pixel_bytes <= 57 * 1024, (
        f"{relative} needs {pixel_bytes} bytes and no longer fits the normal scene bank"
    )

print("LeagueComplete renderer contract passed.")
