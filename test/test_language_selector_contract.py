from pathlib import Path
import ast
import re
import runpy


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
FONT = (ROOT / "src/drivers/FireRedFont.cpp").read_text(encoding="utf-8")
SAVE_LAYOUT = (ROOT / "include/game/TowerDefenseSystem.h").read_text(
    encoding="utf-8"
)
CATALOG = ROOT / "src/game/PortugueseLocalizationGenerated.inc"
POKEMON_DATA = ROOT / "src/game/PokemonDataGenerated.inc"


def main() -> None:
    # Language selection is temporarily hidden on both targets. Existing PT-BR
    # saves are normalized to English while the dormant persistence/catalogue
    # infrastructure stays intact for a future return of the feature.
    assert "void drawSettingsLanguageCard()" not in MAIN
    assert "kSettingsLanguage" not in MAIN
    assert "drawSettingsLanguageCard();" not in MAIN
    assert "selectUiLanguage(chosen);" not in MAIN
    normalize_start = MAIN.index("void normalizeSettings()")
    normalize_end = MAIN.index("\nconst char* screenTimeoutLabel", normalize_start)
    normalize = MAIN[normalize_start:normalize_end]
    assert "savedUiLanguage() == UiLanguage::Portuguese" in normalize
    assert "selectUiLanguage(UiLanguage::English);" in normalize
    assert "if (languageWasPortuguese) saveDirty = true;" in normalize

    # All renderer entry points translate before measuring, fitting, wrapping
    # or painting, so no screen can silently bypass the selected language.
    assert '#include "game/Localization.h"' in FONT
    assert "drawStringLocalized(Localization::translate(text)" in FONT
    assert FONT.count("text = Localization::translate(text);") >= 2
    assert "drawStringLocalized(textBoxLines[line]" in FONT

    # Persistence consumes only the retired block's former padding. The V52
    # GameSave ABI and every field after it remain byte-for-byte unchanged.
    assert "gameSave.towerDefense.reserved" in MAIN
    assert "kLanguageSignature" in MAIN
    assert "uint8_t reserved[3]{};" in SAVE_LAYOUT
    assert "static_assert(sizeof(CampaignState) == 32" in SAVE_LAYOUT

    assert CATALOG.exists(), "Portuguese catalogue was not generated"
    catalogue = CATALOG.read_text(encoding="utf-8")
    row_pattern = re.compile(
        r'^  \{0x[0-9A-F]{8}U,\s*("(?:\\.|[^"\\])*")\s*,'
        r'\s*("(?:\\.|[^"\\])*")\},$',
        re.MULTILINE,
    )
    parsed_rows = [
        (ast.literal_eval(english), ast.literal_eval(portuguese))
        for english, portuguese in row_pattern.findall(catalogue)
    ]
    rows = len(parsed_rows)
    assert rows >= 3000, f"localization catalogue is unexpectedly small: {rows}"
    assert '"SETTINGS", "CONFIGURACOES"' in catalogue
    assert '"LANGUAGE", "IDIOMA"' in catalogue
    assert '"SWORDS DANCE", "DANCA DAS ESPADAS"' in catalogue
    assert '"PORTUGUESE (BRAZIL)", "PORTUGUES BRASIL"' in catalogue
    assert "// Translation engine: ptbr-pob-ct2-v3." in catalogue

    # Compare the checked-in catalogue with the current player-visible source
    # inventory. This prevents a new menu, move, trainer line or Pokedex entry
    # from going stale while the selector is dormant.
    generator = runpy.run_path(
        str(ROOT / "scripts/generate_portuguese_localization.py")
    )
    expected, _dynamic_words = generator["collect_catalogue"]()
    actual = {english for english, _portuguese in parsed_rows}
    missing = sorted(expected - actual)
    assert not missing, f"{len(missing)} visible strings lack PT: {missing[:20]}"
    assert all(portuguese and portuguese.isascii()
               for _english, portuguese in parsed_rows)
    assert all(portuguese == portuguese.upper()
               for _english, portuguese in parsed_rows)
    ptpt_markers = {
        "EQUIPA", "ECRAS", "ECRA", "FICHEIRO", "FICHEIROS",
        "TELEMOVEL", "TELEMOVEIS", "RAPARIGA", "RAPARIGAS",
    }
    offenders = sorted(
        (english, portuguese)
        for english, portuguese in parsed_rows
        if set(re.findall(r"[A-Z]+", portuguese)) & ptpt_markers
    )
    assert not offenders, f"PT-PT vocabulary leaked into PT-BR: {offenders[:5]}"
    party_offenders = sorted(
        (english, portuguese)
        for english, portuguese in parsed_rows
        if re.search(r"\bPARTY\b", english, re.IGNORECASE)
        and set(re.findall(r"[A-Z]+", portuguese)) &
        {"FESTA", "PARTIDO", "GRUPO", "TIME"}
    )
    assert not party_offenders, (
        f"Pokemon PARTY was mistranslated: {party_offenders[:5]}"
    )
    tackle_offenders = sorted(
        (english, portuguese)
        for english, portuguese in parsed_rows
        if re.search(r"\bTACKLE\b", english, re.IGNORECASE)
        and set(re.findall(r"[A-Z]+", portuguese)) &
        {"EQUIPAMENTO", "APARELHO", "PESCA"}
    )
    assert not tackle_offenders, (
        f"Pokemon TACKLE was mistranslated: {tackle_offenders[:5]}"
    )
    assert all("??" not in portuguese or "??" in english
               for english, portuguese in parsed_rows)

    # Species are proper names in both languages. The translation model must
    # never localize, misspell or drop one inside Pokedex prose/dialogue.
    pokemon_source = POKEMON_DATA.read_text(encoding="utf-8")
    ability_start = pokemon_source.index("constexpr AbilityData kAbilities[]")
    species_names = set(re.findall(
        r'^\s*\{"([A-Z0-9 .\-]+)",\s*\d+,',
        pokemon_source[:ability_start],
        re.MULTILINE,
    ))
    species_pattern = re.compile(
        r"(?<![A-Z0-9])(?:" +
        "|".join(sorted((re.escape(name) for name in species_names),
                        key=len, reverse=True)) +
        r")(?![A-Z0-9])"
    )
    proper_name_offenders = []
    for english, portuguese in parsed_rows:
        english_upper = english.upper()
        portuguese_upper = portuguese.upper()
        source_names = set(species_pattern.findall(english_upper))
        target_names = set(species_pattern.findall(portuguese_upper))
        for species in source_names - target_names:
            proper_name_offenders.append((species, english, portuguese))
    assert not proper_name_offenders, (
        f"Pokemon proper names changed in PT-BR ({len(proper_name_offenders)}): "
        f"{proper_name_offenders[:100]}"
    )
    print(f"English-only UI/dormant localization contract passed ({rows} entries).")


if __name__ == "__main__":
    main()
