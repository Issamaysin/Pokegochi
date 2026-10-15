"""Guard cold scene draws against bypassing the versioned asset archive."""

from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SOURCE = (ROOT / "src/drivers/AssetRenderer.cpp").read_text(encoding="utf-8")


def main() -> None:
    start = SOURCE.index("bool AssetRenderer::draw(")
    end = SOURCE.index("bool AssetRenderer::drawHorizontalMirror", start)
    draw_body = SOURCE[start:end]

    forbidden = (
        "if (preloadHomeLayer(path))",
        "if (preloadBattlePlane(path))",
        "if (preloadLargeAsset(path))",
    )
    for call in forbidden:
        assert call not in draw_body, f"cold draw bypasses archive wrapper: {call}"

    # PKG1 battle planes can be 512px wide. Their RGB565-to-indexed
    # conversion shares the 320x4 decode scratch, so a fixed four-row read
    # would overwrite the renderer state before the sprite is cached.
    conversion_start = SOURCE.index("bool convertLegacyToIndexed(")
    conversion_end = SOURCE.index("bool loadIndexed(", conversion_start)
    conversion_body = SOURCE[conversion_start:conversion_end]
    assert "kStreamPixelCapacity / info.header.width" in conversion_body
    assert "line = static_cast<uint16_t>(line + rows);" in conversion_body

    # Home layers, battle planes, large scenes and small sprites must all use
    # the wrapper which opens pokegochi.pak when the versioned archive exists.
    assert draw_body.count("if (preload(path))") >= 4
    print("AssetRenderer archive-path audit passed.")


if __name__ == "__main__":
    main()
