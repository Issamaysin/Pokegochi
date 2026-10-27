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

    # PGA offsets do not change between scene batches. Regressing to clearing
    # this cache in begin/endPreloadBatch makes every menu revisit perform a
    # binary search with many tiny SD seeks for each asset (more than a second
    # on the physical board). Mount/unmount are the only valid boundaries.
    begin_start = SOURCE.index("bool AssetRenderer::beginPreloadBatch()")
    begin_end = SOURCE.index("bool AssetRenderer::primePreload(", begin_start)
    end_start = SOURCE.index("void AssetRenderer::endPreloadBatch()")
    end_end = SOURCE.index("bool AssetRenderer::begin()", end_start)
    assert "archiveRangeCacheCount = 0" not in SOURCE[begin_start:begin_end]
    assert "archiveRangeCacheCount = 0" not in SOURCE[end_start:end_end]

    mount_start = SOURCE.index("void AssetRenderer::storageUnmounted()")
    mount_end = SOURCE.index("bool AssetRenderer::beginPreloadBatch()", mount_start)
    assert SOURCE[mount_start:mount_end].count("archiveRangeCacheCount = 0") == 2
    assert SOURCE[mount_start:mount_end].count(
        "archiveRangeCacheNextReplacement = 0"
    ) == 2

    # A full cache must keep learning the current scene. Otherwise the prime
    # pass and the following payload load each repeat the same binary search.
    lookup_start = SOURCE.index("bool archiveAssetRange(")
    lookup_end = SOURCE.index("// Storage/Bag/Summary", lookup_start)
    lookup_body = SOURCE[lookup_start:lookup_end]
    cache_start = SOURCE.index("void cacheArchiveRange(")
    cache_end = SOURCE.index("bool readFileFully(", cache_start)
    cache_body = SOURCE[cache_start:cache_end]
    assert "cacheArchiveRange(key, offset, length)" in lookup_body
    assert "archiveRangeCacheNextReplacement" in cache_body
    assert "% kArchiveRangeCacheCapacity" in cache_body

    # Multi-asset UI manifests resolve through one lower-bound plus a bounded
    # sequential index scan, never hundreds of independent random seeks.
    bulk_start = SOURCE.index("primeArchiveRangesSequential(")
    bulk_end = SOURCE.index("void AssetRenderer::endPreloadBatch()", bulk_start)
    bulk_body = SOURCE[bulk_start:bulk_end]
    assert "kSequentialLookupThreshold = 6U" in bulk_body
    assert "kEntriesPerRead = 16U" in bulk_body
    assert "archiveHeader.indexOffset + low * archiveHeader.entrySize" in bulk_body
    assert "cacheArchiveRange(entry.key, entry.offset, entry.length)" in bulk_body
    prime_start = SOURCE.index("bool AssetRenderer::primePreloads(")
    prime_end = SOURCE.index("void AssetRenderer::endPreloadBatch()", prime_start)
    prime_body = SOURCE[prime_start:prime_end]
    assert "count < kSequentialLookupThreshold" in prime_body
    assert "return primeArchiveRangesSequential(paths, count);" in prime_body
    print("AssetRenderer archive-path audit passed.")


if __name__ == "__main__":
    main()
