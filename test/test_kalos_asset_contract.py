"""Regression contract for every Kalos base, form and Mega sprite path."""

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
for species in range(650, 722):
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
    666: 20, 669: 5, 670: 6, 671: 5, 676: 10, 678: 2,
    681: 2, 710: 4, 711: 4, 716: 2, 718: 3, 720: 2,
}
for species, count in forms.items():
    for form in range(count):
        prefix = f"pokemon/forms/kalos/{species:03}/{form}"
        required.update((
            f"{prefix}/front.pkg", f"{prefix}/back.pkg",
            f"{prefix}/shiny/front.pkg", f"{prefix}/shiny/back.pkg",
        ))
        for frame in range(2):
            required.add(f"{prefix}/icons/{frame}.pkg")
            required.add(f"{prefix}/shiny/icons/{frame}.pkg")

megas = (652, 655, 658, 668, 670, 678, 687, 689, 691, 701, 718, 719)
for species in megas:
    prefix = f"pokemon/forms/mega/{species:03}/mega"
    required.update((
        f"{prefix}/front.pkg", f"{prefix}/back.pkg",
        f"{prefix}/shiny/front.pkg", f"{prefix}/shiny/back.pkg",
    ))
    for frame in range(2):
        required.add(f"{prefix}/icons/{frame}.pkg")
        required.add(f"{prefix}/shiny/icons/{frame}.pkg")

assert len(required) == 1336
missing_manifest = sorted(required.difference(manifest))
assert not missing_manifest, f"Kalos paths absent from manifest: {missing_manifest[:8]}"
missing_loose = sorted(relative for relative in required if not (ASSET_ROOT / relative).is_file())
assert not missing_loose, f"Kalos loose assets absent: {missing_loose[:8]}"

header = struct.Struct("<4sHHH2xIII")
entry = struct.Struct("<III")
raw = ARCHIVE.read_bytes()
magic, revision, entry_size, _version, count, index_offset, data_offset = header.unpack_from(raw)
assert magic == b"PGA1" and revision == 3 and entry_size == entry.size
archive_entries = {
    key: (offset, length)
    for index in range(count)
    for key, offset, length in (
        entry.unpack_from(raw, index_offset + index * entry.size),
    )
}
archive_keys = set(archive_entries)
missing_archive = sorted(relative for relative in required if asset_hash(relative) not in archive_keys)
assert not missing_archive, f"Kalos paths absent from PGA3: {missing_archive[:8]}"

for relative in required:
    pkg = (ASSET_ROOT / relative).read_bytes()
    offset, length = archive_entries[asset_hash(relative)]
    assert raw[offset:offset + length] == pkg, f"PGA3 payload differs: {relative}"
    pkg_magic, width, height = struct.unpack_from("<4sHH", pkg)
    assert pkg_magic == b"PKG2", relative
    if "/icons/" in relative:
        assert (width, height) == (32, 32), (relative, width, height)
    else:
        assert (width, height) == (64, 64), (relative, width, height)

legacy = {
    f"pokemon/{species:03}/{suffix}"
    for species in range(650, 722)
    for suffix in (
        "front.pkg", "back.pkg", "shiny/front.pkg", "shiny/back.pkg",
        "icons/0.pkg", "icons/1.pkg", "shiny/icons/0.pkg", "shiny/icons/1.pkg",
    )
}
assert not legacy.intersection(manifest), "obsolete incremental Kalos paths remain in manifest"
assert not {asset_hash(relative) for relative in legacy}.intersection(archive_keys), (
    "obsolete incremental Kalos paths remain in PGA3"
)

print(f"Kalos asset contract passed: {len(required)} base/form/Mega runtime assets")
