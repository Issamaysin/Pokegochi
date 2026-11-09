from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (
    ROOT / "android/PokegochiApp/app/src/main/cpp/CMakeLists.txt"
).read_text(encoding="utf-8")
GRADLE = (ROOT / "android/PokegochiApp/app/build.gradle.kts").read_text(
    encoding="utf-8"
)
BUILD = (ROOT / "android/PokegochiApp/build.ps1").read_text(encoding="utf-8")

assert "#if defined(POKEGOCHI_ANDROID_LEGENDARY_BOX_FIXTURE)" in MAIN
assert "seedAndroidLegendaryBoxFixture();" in MAIN
assert "creativeBackgrounds.reserved[1]" in MAIN
assert "0U, speciesId, 100U, false, personalitySeed, contentGeneration" in MAIN
assert "grant(386U, kDeoxysGranted" in MAIN
assert "grant(384U, kRayquazaGranted" in MAIN
assert "grant(383U, kGroudonGranted" in MAIN
assert "grant(382U, kKyogreGranted" in MAIN
assert "grant(491U, kDarkraiGranted" in MAIN
assert "grant(807U, kZeraoraGranted, 0x5A455241U, 7U)" in MAIN
assert "POKEGOCHI_ANDROID_LEGENDARY_BOX_FIXTURE=1" in CMAKE
assert 'providers.gradleProperty("legendaryBoxFixture")' in GRADLE
assert "-PlegendaryBoxFixture=true" in BUILD

print("Android one-time level-100 legendary Box fixture contract passed.")
