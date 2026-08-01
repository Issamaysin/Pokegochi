"""Generate normal FireRed trainer profiles from map scripts and trainer tables."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
D = ROOT / ".downloads/pokefirered-tree/pokefirered-master"
OUT = ROOT / "src/game/TrainerDataGenerated.inc"


def clean(value: str) -> str:
    value = (value.replace("POKÃ©MON", "POKEMON").replace("POKÃ©", "POKE")
             .replace("â€¦", "...").replace("â™‚", " M").replace("â™€", " F"))
    value = re.sub(r"\\[npl]", " ", value).replace("$", "")
    value = re.sub(r"\{[^}]+\}", "", value)
    return re.sub(r"\s+", " ", value).strip()


def quote(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


species_constants = (D / "include/constants/species.h").read_text(encoding="utf-8")
species: dict[str, int] = {}
for symbol, value in re.findall(r"#define SPECIES_([A-Z0-9_]+)\s+(\d+)", species_constants):
    numeric = int(value)
    if 1 <= numeric <= 151:
        species[symbol] = numeric

class_source = (D / "src/data/text/trainer_class_names.h").read_text(encoding="utf-8")
classes = {symbol: clean(name) for symbol, name in re.findall(
    r"\[TRAINER_CLASS_([A-Z0-9_]+)\]\s*=\s*_\(\"([^\"]+)\"\)", class_source)}

party_source = (D / "src/data/trainer_parties.h").read_text(encoding="utf-8")
parties: dict[str, list[int]] = {}
for name, body in re.findall(r"static const struct \w+ (sParty_\w+)\[\]\s*=\s*\{(.*?)\n\};", party_source, re.S):
    ids = [species[s] for s in re.findall(r"\.species\s*=\s*SPECIES_([A-Z0-9_]+)", body) if s in species]
    if ids:
        parties[name] = ids[-3:]

trainer_source = (D / "src/data/trainers.h").read_text(encoding="utf-8")
trainers: dict[str, tuple[str, str, str, str]] = {}
for symbol, body in re.findall(r"\[TRAINER_([A-Z0-9_]+)\]\s*=\s*\{(.*?)\n\s*\},", trainer_source, re.S):
    trainer_class = re.search(r"\.trainerClass\s*=\s*TRAINER_CLASS_([A-Z0-9_]+)", body)
    pic = re.search(r"\.trainerPic\s*=\s*TRAINER_PIC_([A-Z0-9_]+)", body)
    name = re.search(r'\.trainerName\s*=\s*_\("([^"]*)"\)', body)
    party = re.search(r"\.party\s*=\s*\w+\((sParty_\w+)\)", body)
    if trainer_class and pic and name and party:
        trainers[symbol] = (trainer_class.group(1), pic.group(1), clean(name.group(1)), party.group(1))

profiles = []
seen = set()
excluded = ("LEADER", "ELITE", "CHAMPION", "RIVAL", "GIOVANNI", "ROCKET_BOSS")
assets = {p.stem for p in (D / "graphics/trainers/front_pics").glob("*.png")}
for scripts in sorted((D / "data/maps").glob("*/scripts.inc")):
    script_source = scripts.read_text(encoding="utf-8")
    text_path = scripts.with_name("text.inc")
    if not text_path.exists():
        continue
    text_source = text_path.read_text(encoding="utf-8")
    text_values = {}
    for label, body in re.findall(r"^(\w+)::\s*\n((?:\s*\.string.*\n)+)", text_source, re.M):
        text_values[label] = clean("".join(re.findall(r'\.string\s+"((?:\\.|[^"\\])*)"', body)))
    for trainer_symbol, intro_label in re.findall(r"trainerbattle_single\s+TRAINER_([A-Z0-9_]+),\s*(\w+)", script_source):
        if trainer_symbol in seen or any(word in trainer_symbol for word in excluded) or re.search(r"_\d+$", trainer_symbol):
            continue
        record = trainers.get(trainer_symbol)
        intro = text_values.get(intro_label, "")
        if not record or not intro:
            continue
        class_symbol, pic_symbol, name, party_name = record
        party = parties.get(party_name)
        asset = pic_symbol.lower() + "_front_pic"
        if not party or not name or asset not in assets:
            continue
        trainer_class = classes.get(class_symbol, class_symbol.replace("_", " "))
        words = intro.split(); first, second = [], []
        for word in words:
            target = first if len(" ".join(first + [word])) <= 28 else second
            if len(" ".join(target + [word])) <= 30:
                target.append(word)
        profiles.append((trainer_class, name, asset, " ".join(first), " ".join(second), party))
        seen.add(trainer_symbol)

if len(profiles) < 40:
    raise RuntimeError(f"Trainer extraction unexpectedly found only {len(profiles)} profiles")

lines = ["static constexpr TrainerProfile kProfiles[] = {"]
for index, (trainer_class, name, asset, line1, line2, party) in enumerate(profiles[:240]):
    padded = party + [0] * (3 - len(party))
    lines.append(f"  {{{index}, {quote(trainer_class)}, {quote(name)}, {quote(asset)}, {quote(line1)}, {quote(line2)}, "
                 f"{{{padded[0]},{padded[1]},{padded[2]}}}, {len(party)}}},")
lines.append("};")
OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"Generated {min(len(profiles), 240)} original FireRed trainer profiles")
