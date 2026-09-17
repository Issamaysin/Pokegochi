"""Generate FireRed move and Generation-I level-up learnset tables."""
from __future__ import annotations
import re
from pathlib import Path
from text_normalization import fire_red_ascii

ROOT = Path(__file__).resolve().parent.parent
D = ROOT / ".downloads/pokefirered-tree/pokefirered-master"
constants = (D / "include/constants/moves.h").read_text(encoding="utf-8")
info = (D / "src/data/battle_moves.h").read_text(encoding="utf-8")
description_source = (D / "src/move_descriptions.c").read_text(encoding="utf-8")
learnsets = (D / "src/data/pokemon/level_up_learnsets.h").read_text(encoding="utf-8")
pointers = (D / "src/data/pokemon/level_up_learnset_pointers.h").read_text(encoding="utf-8")
species_constants = (D / "include/constants/species.h").read_text(encoding="utf-8")
pokedex_constants = (D / "include/constants/pokedex.h").read_text(encoding="utf-8")

moves = [(int(n), s) for s, n in re.findall(r"#define MOVE_([A-Z0-9_]+)\s+(\d+)", constants)]
moves = sorted((n, s) for n, s in moves if n > 0)
move_ids = {s: n for n, s in moves}

# FireRed already contains a purpose-written description for every move. Keep
# those texts as the canonical Summary copy instead of reducing distinct
# effects such as AGILITY and SWORDS DANCE to a generic "changes stats" line.
description_literals = {
    variable: text
    for variable, text in re.findall(
        r'const u8 (gMoveDescription_[A-Za-z0-9_]+)\[\]\s*=\s*_\("(.*?)"\);',
        description_source,
    )
}
description_pointers = {
    symbol: variable
    for symbol, variable in re.findall(
        r'\[MOVE_([A-Z0-9_]+)\s*-\s*1\]\s*=\s*(gMoveDescription_[A-Za-z0-9_]+)',
        description_source,
    )
}

def normalized_description(symbol: str) -> str:
    raw = description_literals[description_pointers[symbol]]
    text = re.sub(r"\s+", " ", raw.replace(r"\n", " ")).strip()
    text = fire_red_ascii(text, f"move description {symbol}")
    text = text.replace("POKéMON", "POKEMON").replace("Pokémon", "POKEMON")
    return text.upper().replace("\\", "\\\\").replace('"', '\\"')
internal_species = {s for s, _ in re.findall(r"#define SPECIES_([A-Z0-9_]+)\s+(\d+)", species_constants)}
national_symbols = re.findall(r"^\s*NATIONAL_DEX_([A-Z0-9_]+),", pokedex_constants, re.M)
species_ids = {symbol: number for number, symbol in enumerate(national_symbols)
               if 1 <= number <= 386 and symbol in internal_species}
types = {"NORMAL":"Normal","FIGHTING":"Fighting","FLYING":"Flying","POISON":"Poison","GROUND":"Ground","ROCK":"Rock","BUG":"Bug","GHOST":"Ghost","STEEL":"Steel","FIRE":"Fire","WATER":"Water","GRASS":"Grass","ELECTRIC":"Electric","PSYCHIC":"Psychic","ICE":"Ice","DRAGON":"Dragon","DARK":"Dark"}
targets = {
    "SELECTED": "Selected", "DEPENDS": "Depends",
    "USER_OR_SELECTED": "UserOrSelected", "RANDOM": "Random",
    "BOTH": "Both", "USER": "User", "FOES_AND_ALLY": "FoesAndAlly",
    "OPPONENTS_FIELD": "OpponentsField",
}

def value(block: str, key: str, default="0") -> str:
    m = re.search(rf"\.{key}\s*=\s*([A-Z0-9_\-]+)", block)
    return m.group(1) if m else default

rows=[]
for number,symbol in moves:
    m=re.search(rf"\[MOVE_{symbol}\]\s*=\s*\{{(.*?)\n\s*\}},",info,re.S)
    if not m: continue
    b=m.group(1); type_symbol=value(b,"type").removeprefix("TYPE_")
    effect = value(b,"effect").removeprefix("EFFECT_")
    target_symbol = value(b,"target", "MOVE_TARGET_SELECTED").removeprefix("MOVE_TARGET_")
    contact = "true" if "FLAG_MAKES_CONTACT" in b else "false"
    magic_coat = "true" if "FLAG_MAGIC_COAT_AFFECTED" in b else "false"
    snatch = "true" if "FLAG_SNATCH_AFFECTED" in b else "false"
    protect = "true" if "FLAG_PROTECT_AFFECTED" in b else "false"
    mirror_move = "true" if "FLAG_MIRROR_MOVE_AFFECTED" in b else "false"
    kings_rock = "true" if "FLAG_KINGS_ROCK_AFFECTED" in b else "false"
    rows.append(f'    {{{number}, "{symbol.replace("_"," ")}", PokemonType::{types.get(type_symbol,"Normal")}, {value(b,"power")}, {value(b,"accuracy")}, {value(b,"pp")}, {value(b,"priority")}, {value(b,"secondaryEffectChance")}, "{effect}", MoveTarget::{targets[target_symbol]}, {contact}, {magic_coat}, {snatch}, {protect}, {mirror_move}, {kings_rock}}},')

entries=[]
for species_symbol,array_name in re.findall(r"\[SPECIES_([A-Z0-9_]+)\]\s*=\s*(s[A-Za-z0-9]+LevelUpLearnset)",pointers):
    sid=species_ids.get(species_symbol)
    if not sid: continue
    m=re.search(rf"static const u16 {array_name}\[\]\s*=\s*\{{(.*?)LEVEL_UP_END",learnsets,re.S)
    if not m: continue
    for level,move_symbol in re.findall(r"LEVEL_UP_MOVE\(\s*(\d+),\s*MOVE_([A-Z0-9_]+)\)",m.group(1)):
        if move_symbol in move_ids: entries.append((sid,int(level),move_ids[move_symbol]))

out='// Generated from local pokefirered battle data.\nconstexpr FullMoveData kFullMoves[] = {\n'+'\n'.join(rows)+'\n};\n\n'
out+='constexpr LearnsetEntry kLearnsets[] = {\n'+'\n'.join(f'    {{{s}, {l}, {m}}},' for s,l,m in entries)+'\n};\n'
(ROOT/'src/game/MoveDataGenerated.inc').write_text(out,encoding='utf-8',newline='\n')

description_rows = [
    f'    case {number}: return "{normalized_description(symbol)}";'
    for number, symbol in moves
]
description_out = (
    "// Generated from FireRed's original move descriptions.\n"
    "const char* generatedMoveDescription(uint16_t moveId) {\n"
    "  switch (moveId) {\n"
    + "\n".join(description_rows)
    + "\n    default: return \"NO DESCRIPTION AVAILABLE.\";\n"
      "  }\n"
      "}\n"
)
(ROOT/'src/game/MoveDescriptionsGenerated.inc').write_text(
    description_out, encoding='utf-8', newline='\n')
print(f"Generated {len(rows)} moves, {len(description_rows)} descriptions and {len(entries)} learnset entries")
