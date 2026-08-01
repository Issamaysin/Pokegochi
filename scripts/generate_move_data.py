"""Generate FireRed move and Generation-I level-up learnset tables."""
from __future__ import annotations
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
D = ROOT / ".downloads/pokefirered-tree/pokefirered-master"
constants = (D / "include/constants/moves.h").read_text(encoding="utf-8")
info = (D / "src/data/battle_moves.h").read_text(encoding="utf-8")
learnsets = (D / "src/data/pokemon/level_up_learnsets.h").read_text(encoding="utf-8")
pointers = (D / "src/data/pokemon/level_up_learnset_pointers.h").read_text(encoding="utf-8")
species_constants = (D / "include/constants/species.h").read_text(encoding="utf-8")

moves = [(int(n), s) for s, n in re.findall(r"#define MOVE_([A-Z0-9_]+)\s+(\d+)", constants)]
moves = sorted((n, s) for n, s in moves if n > 0)
move_ids = {s: n for n, s in moves}
species_ids = {s: int(n) for s, n in re.findall(r"#define SPECIES_([A-Z0-9_]+)\s+(\d+)", species_constants) if 1 <= int(n) <= 151}
types = {"NORMAL":"Normal","FIGHTING":"Fighting","FLYING":"Flying","POISON":"Poison","GROUND":"Ground","ROCK":"Rock","BUG":"Bug","GHOST":"Ghost","STEEL":"Steel","FIRE":"Fire","WATER":"Water","GRASS":"Grass","ELECTRIC":"Electric","PSYCHIC":"Psychic","ICE":"Ice","DRAGON":"Dragon","DARK":"Dark"}

def value(block: str, key: str, default="0") -> str:
    m = re.search(rf"\.{key}\s*=\s*([A-Z0-9_\-]+)", block)
    return m.group(1) if m else default

rows=[]
for number,symbol in moves:
    m=re.search(rf"\[MOVE_{symbol}\]\s*=\s*\{{(.*?)\n\s*\}},",info,re.S)
    if not m: continue
    b=m.group(1); type_symbol=value(b,"type").removeprefix("TYPE_")
    effect = value(b,"effect").removeprefix("EFFECT_")
    rows.append(f'    {{{number}, "{symbol.replace("_"," ")}", PokemonType::{types.get(type_symbol,"Normal")}, {value(b,"power")}, {value(b,"accuracy")}, {value(b,"pp")}, {value(b,"priority")}, {value(b,"secondaryEffectChance")}, "{effect}"}},')

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
print(f"Generated {len(rows)} moves and {len(entries)} learnset entries")
