from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
FONT = (ROOT / "src/drivers/FireRedFont.cpp").read_text(encoding="utf-8")

# The selector starts with exactly the three original choices and expands by
# pages, never by reducing each hit target as new generations unlock.
assert "constexpr uint8_t kUiThemesPerPage = 3U;" in MAIN
assert "id <= static_cast<uint8_t>(UiColorTheme::Emerald)" in MAIN
assert "SinnohExpedition::platinumThemeUnlocked(gameSave.sinnoh)" in MAIN
assert "UnovaAdventure::white2ThemeUnlocked(gameSave.unova)" in MAIN
assert "UnovaAdventure::whiteThemeUnlocked(gameSave.unova)" in MAIN
assert "KalosAdventure::xThemeUnlocked(gameSave.kalos)" in MAIN
assert "AlolaAdventure::sunThemeUnlocked(gameSave.alola)" in MAIN
assert "AlolaAdventure::moonThemeUnlocked(gameSave.alola)" in MAIN
assert "static_cast<uint8_t>(UiColorTheme::Moon) + 1U" in MAIN
assert 'case UiColorTheme::Sun: return "SUN";' in MAIN
assert 'case UiColorTheme::Moon: return "MOON";' in MAIN
assert 'case UiColorTheme::White: return "WHITE";' in MAIN
assert "theme == UiColorTheme::White" in MAIN
assert "{0xFFFF, 0xFFFF, 0x0000, 0xFFFF}, // White" in MAIN
assert "{0x0000, 0x2104, 0x0000, 0x3186}, // Black" in MAIN
assert "uint16_t themePrimaryInk() { return readableInkOn(themePrimary()); }" in MAIN
assert "uint16_t themePaleInk() { return readableInkOn(themePale()); }" in MAIN
assert "const uint16_t ink = readableInkOn(color);" in MAIN
assert "if (foreground == background)" in FONT
assert "background == TFT_WHITE ? TFT_BLACK : TFT_WHITE" in FONT
assert "theme == UiColorTheme::Moon" in MAIN
assert "{0x48B1, 0x79F4, 0x2025, 0xCDBB}, // Moon" in MAIN
assert "unlockedUiThemeCount() +" in MAIN
assert "kUiThemesPerPage - 1U" in MAIN
assert "unlockedUiThemeAt(position, candidate)" in MAIN

# WHITE keeps black as its semantic ink, but the lock screen must use a white
# scene surface instead of accidentally promoting that ink to the background.
assert "selectedUiTheme() == UiColorTheme::White" in MAIN
assert "lockSurface = whiteTheme ? TFT_WHITE : themeDark()" in MAIN
assert "frameInset = whiteTheme ? TFT_BLACK : TFT_WHITE" in MAIN
assert "trackInterior = whiteTheme ? 0xDEFB : themePrimary()" in MAIN
assert "fireRedText.setTextColor(lockInk, lockSurface)" in MAIN

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
