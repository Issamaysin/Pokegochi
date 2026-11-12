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
NOTES = (ROOT / "docs/RELEASE_NOTES_V4.3.md").read_text(encoding="utf-8")


def test_coordinated_v43_v33_release_identity():
    assert 'constexpr char kGameVersion[] = "4.3";' in MAIN
    assert '"APP 3.3 RELEASE"' in MAIN
    assert "versionCode = 121" in GRADLE
    assert 'versionName = "3.3"' in GRADLE
    assert '"android-app-" + BuildConfig.VERSION_NAME + "-r442-"' in ACTIVITY
    assert "kRequiredAssetPackVersion = 55" in PACK
    assert "kFormatVersion = 53" in SAVE
    assert "constexpr uint8_t kVersion = 10;" in PROTOCOL
    assert README.startswith("# Pokegochi Firmware 4.3 / Android App 3.3")
    assert "docs/RELEASE_NOTES_V4.3.md" in README
    assert NOTES.startswith("# Pokegochi Firmware 4.3 / Android App 3.3")


if __name__ == "__main__":
    test_coordinated_v43_v33_release_identity()
    print("Release identity contract passed.")
