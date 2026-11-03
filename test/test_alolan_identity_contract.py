from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
FORMS = (ROOT / "src" / "game" / "AlolaForms.cpp").read_text(encoding="utf-8")


def section(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


picker = section(FORMS, "bool canSelectForm", "uint8_t baseHp")
assert "regionalSpecies" not in picker, "regional variants returned to the FORM picker"
assert '"A.%s"' in MAIN, "Alolan individuals lost the A.NAME presentation"
assert MAIN.count("ownedPokemonName(") >= 20, (
    "A.NAME must cover Home, Box, Summary, battle, trade and item flows"
)

asset_router = section(
    MAIN, "if(AlolaForms::supports(species))", "if (species == 421"
)
assert "AlolaForms::isRegionalSpecies(species)||form!=0U" in asset_router
assert asset_router.count("return;") == 1, (
    "Kanto form 0 must fall through to the original classic sprite paths"
)
classic_router = section(MAIN, "if (view == PokemonAssetView::Icon)", "void centeredPokemonAssetOrigin")
assert '"/pokegochi/assets/pokemon/%sicons/%03u_%u.pkg"' in classic_router
assert '"/pokegochi/assets/pokemon/%s%s/%03u.pkg"' in classic_router

print("Alolan identity contract passed: A.NAME is permanent; Kanto keeps classic sprites.")
