"""Keep every Wi-Fi Trade Box sprite resident on the constrained ESP32."""

import re
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
ASSET_ROOT = ROOT / ".generated/sdcard/pokegochi/assets"
RENDERER = (ROOT / "src/drivers/AssetRenderer.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")


def pkg(path: Path) -> tuple[int, int, int, int, list[int], bytes]:
    data = path.read_bytes()
    magic, width, height, transparent, bpp, palette_count = struct.unpack_from(
        "<4sHHHBB", data
    )
    assert magic == b"PKG2", path
    palette = list(struct.unpack_from(
        f"<{palette_count}H", data, 12
    ))
    return (
        width,
        height,
        transparent,
        bpp,
        palette,
        data[12 + palette_count * 2:],
    )


def four_bpp_value(payload: bytes, pixel: int) -> int:
    packed = payload[pixel >> 1]
    return packed & 0x0F if pixel & 1 else packed >> 4


def resident_icon_bytes(path: Path) -> int:
    width, height, transparent, bpp, palette, payload = pkg(path)
    assert bpp == 4 and width <= 255 and height <= 255, path
    raw_bytes = (width * height + 1) // 2
    visible = [
        pixel
        for pixel in range(width * height)
        if (
            four_bpp_value(payload, pixel) < len(palette)
            and palette[four_bpp_value(payload, pixel)] != transparent
        )
    ]
    if not visible or transparent not in palette:
        compact_bytes = raw_bytes
    else:
        xs = [pixel % width for pixel in visible]
        ys = [pixel // width for pixel in visible]
        cropped_pixels = (max(xs) - min(xs) + 1) * (max(ys) - min(ys) + 1)
        compact_bytes = 5 + (cropped_pixels + 1) // 2
    return len(palette) * 2 + min(raw_bytes, compact_bytes)


def resident_pkg_bytes(path: Path) -> int:
    width, height, _transparent, bpp, palette, payload = pkg(path)
    assert bpp == 4 and len(payload) == (width * height + 1) // 2, path
    return len(palette) * 2 + len(payload)


def main() -> None:
    icon_paths = sorted(
        path
        for path in (ASSET_ROOT / "pokemon").rglob("*.pkg")
        if "icons" in path.parts
    )
    assert icon_paths, "The canonical generated SD pack is required."

    # Wi-Fi leaves a measured minimum 4 KiB scene spill beside the permanent
    # 13 KiB sprite arena. A full page can reference 30 different grid icons
    # plus a selected icon not present on that page. Include one alignment
    # byte per allocation and the complete FireRed Box chrome manifest.
    icon_reserves = sorted(
        (resident_icon_bytes(path) for path in icon_paths), reverse=True
    )
    fixed_paths = [
        ASSET_ROOT / "firered/pokemon_storage/scrolling_bg.pkg",
        ASSET_ROOT / "firered/pokemon_storage/box_scroll_arrow_00.pkg",
        ASSET_ROOT / "firered/pokemon_storage/box_scroll_arrow_01.pkg",
        ASSET_ROOT / "firered/pokemon_storage/cursor_00.pkg",
    ]
    asset_count = 31 + len(fixed_paths)
    worst_resident_set = (
        sum(icon_reserves[:31])
        + sum(resident_pkg_bytes(path) for path in fixed_paths)
        + asset_count
    )
    assert worst_resident_set <= 13_312 + 4_096, (
        worst_resident_set,
        "Wi-Fi Trade Box can drop its final sprite entries",
    )

    # Form-aware Mega/shiny icon paths reach exactly 72 visible bytes. The
    # local manifest therefore needs 73 including the C string terminator.
    runtime_paths = [
        "/pokegochi/assets/" + path.relative_to(ASSET_ROOT).as_posix()
        for path in icon_paths
    ]
    assert max(len(path) + 1 for path in runtime_paths) == 73
    assert "static char iconPaths[30][73];" in MAIN
    assert "static char previewPath[73];" in MAIN

    # The local Box retains its full front art; only active Trade selection
    # reuses the selected 32x32 icon and its already resident cache entry.
    prepare_start = MAIN.index("bool prepareBoxSceneAssets()")
    prepare_end = MAIN.index("void drawBoxPreviewArtwork", prepare_start)
    prepare = MAIN[prepare_start:prepare_end]
    assert "const bool compactTradePreview = multiplayerTradeSelectionActive();" in prepare
    assert re.search(
        r"compactTradePreview\s*\?\s*PokemonAssetView::Icon\s*:\s*"
        r"PokemonAssetView::Front",
        prepare,
    )
    preview_start = prepare_end
    preview_end = MAIN.index("void drawBoxPreviewPanelContents", preview_start)
    preview = MAIN[preview_start:preview_end]
    assert "selected && multiplayerTradeSelectionActive()" in preview
    assert "PokemonAssetView::Icon, 0U" in preview
    assert "else if (selected) creature(53,87,*selected,true);" in preview

    # The cache representation must remain runtime-only and preserve the
    # original canvas dimensions while decoding transparent pixels outside
    # the stored rectangle.
    assert "kResidentCropped4Bpp = 0x8CU" in RENDERER
    assert "bool isPokemonIconAsset(const char* path)" in RENDERER
    assert "compactBytes >= info.pixelBytes" in RENDERER
    assert "cropped4BppValueAt(image, x, y)" in RENDERER
    assert "installCropped4Bpp(info, croppedPlan" in RENDERER
    assert "result.header = info.header;" in RENDERER

    print(
        "Wi-Fi Trade Box sprite contract passed: "
        f"worst={worst_resident_set} capacity={13_312 + 4_096}."
    )


if __name__ == "__main__":
    main()
