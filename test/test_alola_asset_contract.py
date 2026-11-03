"""Regression contract for every Alola base/form/campaign runtime asset."""

import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ASSET_ROOT = ROOT / ".generated/sdcard/pokegochi/assets"
ARCHIVE = ROOT / ".generated/sdcard/pokegochi/pokegochi.pak"


def asset_hash(relative: str) -> int:
    value = 2166136261
    for byte in relative.replace("\\", "/").lower().encode("utf-8"):
        value ^= byte
        value = (value * 16777619) & 0xFFFFFFFF
    return value


manifest = json.loads((ASSET_ROOT / "manifest.json").read_text(encoding="utf-8"))
required: set[str] = set()
for species in range(722, 808):
    required.update((
        f"pokemon/front/{species:03}.pkg",
        f"pokemon/back/{species:03}.pkg",
        f"pokemon/shiny/front/{species:03}.pkg",
        f"pokemon/shiny/back/{species:03}.pkg",
        f"pokemon/evolution/dark/{species:03}.pkg",
        f"pokemon/evolution/light/{species:03}.pkg",
    ))
    for frame in range(2):
        required.add(f"pokemon/icons/{species:03}_{frame}.pkg")
        required.add(f"pokemon/shiny/icons/{species:03}_{frame}.pkg")

forms = {
    **{species: 2 for species in
       (19, 20, 26, 27, 28, 37, 38, 50, 51, 52, 53, 74, 75, 76,
        88, 89, 103, 105, 658, 746, 778, 801)},
    741: 4, 745: 3, 773: 18, 774: 14, 800: 4,
}
for species, count in forms.items():
    for form in range(count):
        prefix = f"pokemon/forms/alola/{species:03}/{form}"
        required.update((
            f"{prefix}/front.pkg", f"{prefix}/back.pkg",
            f"{prefix}/shiny/front.pkg", f"{prefix}/shiny/back.pkg",
        ))
        for frame in range(2):
            required.add(f"{prefix}/icons/{frame}.pkg")
            required.add(f"{prefix}/shiny/icons/{frame}.pkg")

required.update(f"alola/rooms/area_{area:02}.pkg" for area in range(14))
required.update(f"alola/rooms/kind_{kind:02}.pkg" for kind in range(10))
required.add("alola/region_hub/alola_sun.pkg")
required.add("alola/key_items/necrozma_fusers.pkg")
required.update(f"firered/items/held_{item:02}.pkg" for item in range(92, 127))
assert len(required) == 1617

missing_manifest = sorted(required.difference(manifest))
assert not missing_manifest, f"Alola paths absent from manifest: {missing_manifest[:8]}"
missing_loose = sorted(path for path in required if not (ASSET_ROOT / path).is_file())
assert not missing_loose, f"Alola loose assets absent: {missing_loose[:8]}"

header = struct.Struct("<4sHHH2xIII")
entry = struct.Struct("<III")
raw = ARCHIVE.read_bytes()
magic, revision, entry_size, _version, count, index_offset, _data_offset = \
    header.unpack_from(raw)
assert magic == b"PGA1" and revision == 3 and entry_size == entry.size
archive_entries = {
    key: (offset, length)
    for index in range(count)
    for key, offset, length in (
        entry.unpack_from(raw, index_offset + index * entry.size),
    )
}
missing_archive = sorted(
    path for path in required if asset_hash(path) not in archive_entries)
assert not missing_archive, f"Alola paths absent from PGA3: {missing_archive[:8]}"

for relative in required:
    pkg = (ASSET_ROOT / relative).read_bytes()
    offset, length = archive_entries[asset_hash(relative)]
    assert raw[offset:offset + length] == pkg, f"PGA3 payload differs: {relative}"
    pkg_magic, width, height = struct.unpack_from("<4sHH", pkg)
    assert pkg_magic == b"PKG2", relative
    if "/icons/" in relative or "/kind_" in relative or "region_hub" in relative:
        assert width <= 64 and height <= 64, (relative, width, height)
    elif relative.startswith("pokemon/"):
        assert (width, height) == (64, 64), (relative, width, height)

print(f"Alola asset contract passed: {len(required)} base/form/campaign assets")
