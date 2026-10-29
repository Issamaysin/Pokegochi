import json
import re
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
AUDIT = ROOT / ".generated/battle_animation_audit.json"
ASSET_ROOT = ROOT / ".generated/sdcard/pokegochi"
GENERATED = ROOT / "src/game/BattleAnimationGenerated.inc"
VERSION_HEADER = ROOT / "include/config/AssetPackVersion.h"
ANDROID_GRADLE = ROOT / "android/PokegochiApp/app/build.gradle.kts"
ANDROID_BUILD = ROOT / "android/PokegochiApp/build.ps1"


audit = json.loads(AUDIT.read_text(encoding="utf-8"))
expected_catalog = str(audit.get("catalogHash", ""))
assert re.fullmatch(r"[0-9a-f]{16}", expected_catalog), expected_catalog

generated_match = re.search(
    r'kBattleAnimationCatalogId\[\]\s*=\s*"([0-9a-f]{16})"',
    GENERATED.read_text(encoding="utf-8"),
)
assert generated_match, "generated native catalog identifier is missing"
assert generated_match.group(1) == expected_catalog

packaged_catalog = (ASSET_ROOT / "assets/animation_catalog.txt").read_text(
    encoding="ascii"
).strip()
assert packaged_catalog == expected_catalog, (
    f"packaged catalog {packaged_catalog} != native catalog {expected_catalog}"
)

version_match = re.search(
    r"kRequiredAssetPackVersion\s*=\s*(\d+)",
    VERSION_HEADER.read_text(encoding="ascii"),
)
assert version_match, "asset pack version is missing"
required_version = int(version_match.group(1))
packaged_version = int(
    (ASSET_ROOT / "assets/pack_version.txt").read_text(encoding="ascii").strip()
)
assert packaged_version == required_version

archive = ASSET_ROOT / "pokegochi.pak"
with archive.open("rb") as source:
    header = source.read(struct.calcsize("<4sHHH2xIII"))
magic, revision, entry_size, archive_version, asset_count, index_offset, data_offset = (
    struct.unpack("<4sHHH2xIII", header)
)
assert magic == b"PGA1" and revision == 3 and entry_size == 12
assert archive_version == required_version
assert asset_count > 0 and index_offset == len(header) and data_offset > index_offset

gradle = ANDROID_GRADLE.read_text(encoding="utf-8")
assert "outputs.upToDateWhen { false }" in gradle
assert 'verify${variant}ApkAssetPack' in gradle
assert "APK asset pack is stale" in gradle
build_script = ANDROID_BUILD.read_text(encoding="utf-8")
assert ":app:verifyReleaseApkAssetPack" in build_script
assert ":app:verifyDebugApkAssetPack" in build_script

manifest = json.loads((ASSET_ROOT / "assets/manifest.json").read_text(encoding="utf-8"))
required_frames = {
    f"firered/battle_anims/sprites/{key}_{frame:02}.pkg"
    for key, spec in audit["visualAssets"].items()
    for frame in range(int(spec["frameCount"]))
}
missing_frames = sorted(required_frames.difference(manifest))
assert not missing_frames, f"animation frames absent from pack manifest: {missing_frames[:5]}"

print(
    "Android asset catalog contract passed: "
    f"catalog={expected_catalog} pack={required_version} assets={asset_count} "
    f"animationFrames={len(required_frames)}"
)
