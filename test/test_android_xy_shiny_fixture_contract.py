from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts.kalos_rom_data import level_moves


MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (
    ROOT / "android/PokegochiApp/app/src/main/cpp/CMakeLists.txt"
).read_text(encoding="utf-8")
GRADLE = (ROOT / "android/PokegochiApp/app/build.gradle.kts").read_text(
    encoding="utf-8"
)
BUILD = (ROOT / "android/PokegochiApp/build.ps1").read_text(encoding="utf-8")


def latest_moves_at_level(species: int, level: int) -> set[int]:
    learned = [move for learned_level, move in level_moves(species) if learned_level <= level]
    return set(learned[-4:])


assert "#if defined(POKEGOCHI_ANDROID_XY_SHINY_FIXTURE)" in MAIN
assert "grant(716U, kXerneasGranted" in MAIN
assert "grant(717U, kYveltalGranted" in MAIN
assert "0U, speciesId, 50U, true, personalitySeed, 6U" in MAIN
assert "creativeBackgrounds.reserved[2]" in MAIN
assert "kXerneasGranted = 1U << 0U" in MAIN
assert "kYveltalGranted = 1U << 1U" in MAIN
assert "POKEGOCHI_ANDROID_XY_SHINY_FIXTURE=1" in CMAKE
assert 'providers.gradleProperty("xyShinyFixture")' in GRADLE
assert ".getOrElse(false)" in GRADLE
assert "-PxyShinyFixture=true" in BUILD

# The authored X/Y Lv.50 encounters retain their signature moves after the
# engine selects the four most recent legal level-up entries.
assert 601 in latest_moves_at_level(716, 50)  # GEOMANCY
assert 613 in latest_moves_at_level(717, 50)  # OBLIVION WING

print("Android one-time shiny Xerneas/Yveltal fixture contract passed.")
