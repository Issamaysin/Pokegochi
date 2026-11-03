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
NOTES = (ROOT / "docs/RELEASE_NOTES_V4.0.md").read_text(encoding="utf-8")


def test_coordinated_v4_v3_release_identity():
    assert 'constexpr char kGameVersion[] = "4.0 ALOLA";' in MAIN
    assert '"APP 3.0 RELEASE"' in MAIN
    assert "versionCode = 104" in GRADLE
    assert 'versionName = "3.0"' in GRADLE
    assert '"android-app-" + BuildConfig.VERSION_NAME + "-r317-"' in ACTIVITY
    assert "kRequiredAssetPackVersion = 54" in PACK
    assert "kFormatVersion = 51" in SAVE
    assert "constexpr uint8_t kVersion = 7;" in PROTOCOL
    assert README.startswith("# Pokegochi Firmware 4.0 / Android App 3.0")
    assert "docs/RELEASE_NOTES_V4.0.md" in README
    assert NOTES.startswith("# Pokegochi Firmware 4.0 / Android App 3.0")


if __name__ == "__main__":
    test_coordinated_v4_v3_release_identity()
    print("Release identity contract passed.")
