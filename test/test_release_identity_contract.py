from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
GRADLE = (ROOT / "android/PokegochiApp/app/build.gradle.kts").read_text(
    encoding="utf-8"
)
ACTIVITY = (
    ROOT
    / "android/PokegochiApp/app/src/main/java/com/pokegochi/app/MainActivity.java"
).read_text(encoding="utf-8")
PACK = (ROOT / "include/config/AssetPackVersion.h").read_text(encoding="utf-8")
SAVE = (ROOT / "include/services/PersistentSave.h").read_text(encoding="utf-8")
PROTOCOL = (ROOT / "include/services/MultiplayerProtocol.h").read_text(
    encoding="utf-8"
)
README = (ROOT / "README.md").read_text(encoding="utf-8")
NOTES = (ROOT / "docs/RELEASE_NOTES_V4.1.md").read_text(encoding="utf-8")


def test_coordinated_v41_v31_release_identity():
    assert 'constexpr char kGameVersion[] = "4.1 WIFI";' in MAIN
    assert '"APP 3.1 RELEASE"' in MAIN
    assert "versionCode = 117" in GRADLE
    assert 'versionName = "3.1"' in GRADLE
    assert '"android-app-" + BuildConfig.VERSION_NAME + "-r410-"' in ACTIVITY
    assert "kRequiredAssetPackVersion = 55" in PACK
    assert "kFormatVersion = 52" in SAVE
    assert "constexpr uint8_t kVersion = 10;" in PROTOCOL
    assert README.startswith("# Pokegochi Firmware 4.1 / Android App 3.1")
    assert "docs/RELEASE_NOTES_V4.1.md" in README
    assert NOTES.startswith("# Pokegochi Firmware 4.1 / Android App 3.1")


if __name__ == "__main__":
    test_coordinated_v41_v31_release_identity()
    print("Release identity contract passed.")
