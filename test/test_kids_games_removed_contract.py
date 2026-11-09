from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
TEST_SCRIPT = (ROOT / "scripts/test.ps1").read_text(encoding="utf-8")


def main() -> None:
    forbidden_runtime_tokens = (
        "Screen::Kids",
        "drawKids",
        "serviceKids",
        "KidsMemoryGame",
        "TowerDefense::Runtime",
        "KIDS GAMES",
    )
    for token in forbidden_runtime_tokens:
        assert token not in MAIN, f"retired Kids Games runtime remains: {token}"

    retired_files = (
        ROOT / "include/game/KidsMemoryGame.h",
        ROOT / "src/game/KidsMemoryGame.cpp",
        ROOT / "src/game/TowerDefenseSystem.cpp",
        ROOT / "src/game/KidsAnimalSpritesGenerated.inc",
        ROOT / "scripts/generate_kids_animal_sprites.py",
        ROOT / "test/native/test_kids_memory_game.cpp",
        ROOT / "test/native/test_tower_defense.cpp",
    )
    for path in retired_files:
        assert not path.exists(), f"retired Kids Games file still exists: {path}"

    assert "kids_memory_game" not in TEST_SCRIPT
    assert "tower_defense.exe" not in TEST_SCRIPT

    compatibility = (ROOT / "include/game/TowerDefenseSystem.h").read_text(
        encoding="utf-8"
    )
    assert "Compatibility-only layout" in compatibility
    assert "static_assert(sizeof(CampaignState) == 32" in compatibility

    print("Kids Games removal contract passed.")


if __name__ == "__main__":
    main()
