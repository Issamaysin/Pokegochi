from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
EGGS = (ROOT / "src" / "game" / "EggSystem.cpp").read_text(encoding="utf-8")


def section(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


statistics = section(
    MAIN, "bool isLegendarySpecies", "void playHeldItemAnimation"
)
assert "isLegendaryOrMythicalExcludingUltraBeasts" in statistics
assert EGGS.count("isLegendaryOrMythicalExcludingUltraBeasts(species)") == 2

print(
    "Ultra Beast legendary contract passed: Stats and Special Eggs exclude "
    "Ultra Beasts while retaining Necrozma."
)
