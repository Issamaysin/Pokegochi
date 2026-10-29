from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")

# The selector starts with exactly the three original choices and expands by
# pages, never by reducing each hit target as new generations unlock.
assert "constexpr uint8_t kUiThemesPerPage = 3U;" in MAIN
assert "id <= static_cast<uint8_t>(UiColorTheme::Emerald)" in MAIN
assert "SinnohExpedition::platinumThemeUnlocked(gameSave.sinnoh)" in MAIN
assert "UnovaAdventure::white2ThemeUnlocked(gameSave.unova)" in MAIN
assert "KalosAdventure::xThemeUnlocked(gameSave.kalos)" in MAIN
assert "unlockedUiThemeCount() +" in MAIN
assert "kUiThemesPerPage - 1U" in MAIN
assert "unlockedUiThemeAt(position, candidate)" in MAIN

# GAME keeps one stable entry point; the dedicated browser owns three large
# cards, a page indicator, navigation arrows and selection persistence.
assert 'button(211,kSettingsGameThemeOpenY,90,28,"CHOOSE"' in MAIN
assert 'settingsPage == 1 ? "HOME BACKGROUND" : "COLOR THEMES"' in MAIN
assert "void drawSettingsThemeBrowser()" in MAIN
assert "kSettingsThemeCardHeight = 43" in MAIN
assert 'candidate == selected ? "SELECTED" : "TAP TO USE"' in MAIN
assert 'std::snprintf(pageLabel, sizeof(pageLabel), "%u/%u"' in MAIN
assert "settingsPage=2;drawSettings();return;" in MAIN
assert "chooseUiTheme(candidate)" in MAIN
assert "saveDirty=true;saveNow();drawSettings();" in MAIN

# The old shrinking row must not return.
assert "288/choiceCount" not in MAIN
assert "kSettingsGameThemeButtonY" not in MAIN

print("Expandable theme selector contract passed.")
