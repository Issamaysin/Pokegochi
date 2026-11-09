from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
ACTIVITY = (ROOT / "android/PokegochiUpdater/app/src/main/java/com/pokegochi/updater/MainActivity.java").read_text(
    encoding="utf-8"
)
GRADLE = (ROOT / "android/PokegochiUpdater/app/build.gradle.kts").read_text(
    encoding="utf-8"
)
ANDROID_PROTOCOL = (ROOT / "android/PokegochiUpdater/app/src/main/java/com/pokegochi/updater/UpdateProtocol.java").read_text(
    encoding="utf-8"
)
FIRMWARE_PROTOCOL = (ROOT / "include/services/WirelessUpdateProtocol.h").read_text(
    encoding="utf-8"
)
WIRELESS_UPDATE = (ROOT / "src/services/WirelessUpdate.cpp").read_text(
    encoding="utf-8"
)
PACKAGE_BUILDER = (ROOT / "scripts/build_wireless_update.py").read_text(
    encoding="utf-8"
)


def method_body(name: str, following_marker: str) -> str:
    start = ACTIVITY.index(f"private void {name}(")
    end = ACTIVITY.index(following_marker, start)
    return ACTIVITY[start:end]


def test_document_picker_does_not_filter_project_formats_by_provider_mime():
    package_picker = method_body("selectPackage", "private void chooseBackupDestination(")
    save_picker = method_body("chooseSaveToRestore", "@Override protected void onActivityResult(")
    assert 'intent.setType("*/*")' in package_picker
    assert 'intent.setType("*/*")' in save_picker
    assert "EXTRA_MIME_TYPES" not in package_picker
    assert 'intent.setType("application/octet-stream")' not in save_picker


def test_android_and_firmware_protocol_constants_stay_aligned():
    android_version = int(re.search(r"static final int VERSION = (\d+);", ANDROID_PROTOCOL).group(1))
    firmware_version = int(re.search(r"constexpr uint8_t kVersion = (\d+);", FIRMWARE_PROTOCOL).group(1))
    builder_version = int(re.search(r"PROTOCOL_VERSION = (\d+)", PACKAGE_BUILDER).group(1))
    assert android_version == firmware_version == builder_version

    android_save = int(re.search(r"static final int SAVE_BACKUP_VERSION = (\d+);", ANDROID_PROTOCOL).group(1))
    firmware_save = int(re.search(r"constexpr uint16_t kSaveBackupVersion = (\d+);", FIRMWARE_PROTOCOL).group(1))
    assert android_save == firmware_save
    assert "static_assert(sizeof(SaveBackupHeader) == 24" in FIRMWARE_PROTOCOL
    assert "static final int SAVE_HEADER_BYTES = 24" in ANDROID_PROTOCOL


def test_large_current_save_is_copied_from_spiffs_in_bounded_reads():
    start = WIRELESS_UPDATE.index("bool WirelessUpdate::finishSaveImport()")
    end = WIRELESS_UPDATE.index("bool WirelessUpdate::resetSave()", start)
    body = WIRELESS_UPDATE[start:end]
    assert "while (copied < saveBytes_)" in body
    assert "std::min<size_t>(sizeof(buffer), saveBytes_ - copied)" in body
    assert "input.read(destination + copied, requested)" in body
    assert "input.read(reinterpret_cast<uint8_t*>(saveData_), saveBytes_)" not in body


def test_updater_release_identity_was_advanced_for_format_fix():
    assert "versionCode = 7" in GRADLE
    assert 'versionName = "2.5"' in GRADLE


def main() -> None:
    test_document_picker_does_not_filter_project_formats_by_provider_mime()
    test_android_and_firmware_protocol_constants_stay_aligned()
    test_large_current_save_is_copied_from_spiffs_in_bounded_reads()
    test_updater_release_identity_was_advanced_for_format_fix()
    print("Android updater current-format contract tests passed")


if __name__ == "__main__":
    main()
