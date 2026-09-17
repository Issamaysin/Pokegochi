"""Normalize source-game text for Pokegochi's byte-oriented FireRed font."""
from __future__ import annotations

import unicodedata


_DISPLAY_REPLACEMENTS = {
    "POKéMON": "POKEMON", "Pokémon": "Pokemon", "POKÉMON": "POKEMON",
    "POKé": "POKE", "Poké": "Poke", "POKÉ": "POKE",
    "…": "...", "♂": " M", "♀": " F",
    "‘": "'", "’": "'", "“": '"', "”": '"',
    "–": "-", "—": "-", " ": " ",
    # Known mojibake found in some upstream snapshots.
    "POKÃƒÂ©MON": "POKEMON", "POKÃ©MON": "POKEMON",
    "POKÃƒÂ©": "POKE", "Ã¢€¦": "...", "â€¦": "...",
    "Ã¢„¢â€š": " M", "Ã¢„¢â€¬": " F",
}


def fire_red_ascii(value: str, context: str = "display text") -> str:
    """Return display-safe ASCII and fail loudly on an unknown symbol."""
    for source, replacement in _DISPLAY_REPLACEMENTS.items():
        value = value.replace(source, replacement)
    value = unicodedata.normalize("NFKD", value)
    value = "".join(character for character in value
                    if not unicodedata.combining(character))
    unsupported = sorted({character for character in value if ord(character) > 0x7F})
    if unsupported:
        rendered = ", ".join(f"U+{ord(character):04X}" for character in unsupported)
        raise ValueError(f"Unsupported character(s) in {context}: {rendered}")
    return value
