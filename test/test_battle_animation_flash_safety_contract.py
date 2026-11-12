from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_neutral_full_field_flashes_are_softened_centrally() -> None:
    source = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    assert "kBattleArenaNeutralBlendCap = 10U" in source
    assert source.count("presentedBattleArenaBlendAmount(") >= 6
    assert "battleVisualOverlay.arenaFadeToBlack = presentedBattleArenaBlendAmount(" in source
    assert "battleVisualOverlay.arenaBlendAmount = presentedBattleArenaBlendAmount(" in source


def test_local_sprite_flashes_are_not_globally_capped() -> None:
    source = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    helper_start = source.index("uint8_t presentedBattleArenaBlendAmount")
    helper_end = source.index("void drawBattleArenaFade", helper_start)
    helper = source[helper_start:helper_end]
    assert "battleVisualOverlay.flash" not in helper
    assert "BattleVisualLayer" not in helper


if __name__ == "__main__":
    test_neutral_full_field_flashes_are_softened_centrally()
    test_local_sprite_flashes_are_not_globally_capped()
    print("Battle animation flash safety contract passed")
