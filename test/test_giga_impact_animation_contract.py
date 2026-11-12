import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
generator = (ROOT / "scripts/generate_battle_animation_data.py").read_text(
    encoding="utf-8"
)
source = (
    ROOT / ".downloads/pokeemerald-expansion/data/battle_anim_scripts.s"
).read_text(encoding="utf-8")
audit = json.loads(
    (ROOT / ".generated/battle_animation_audit.json").read_text(encoding="utf-8")
)

# Giga Impact chooses a distinct authored field for an opponent target, a
# player target and Contests.  The latter is unreachable in Pokegochi, but it
# must remain in the bytecode/asset union instead of changing label offsets.
script = source.split("gBattleAnimMove_GigaImpact::", 1)[1].split(
    "\ngBattleAnimMove_", 1
)[0]
assert "jumprettrue SetGigaImpactContestsBG" in script
assert "jumpretfalse SetGigaImpactOpponentBG" in script
assert "fadetobg BG_GIGA_IMPACT_OPPONENT" in script
assert "fadetobg BG_GIGA_IMPACT_PLAYER" in script
assert "fadetobg BG_GIGA_IMPACT_CONTESTS" in script

move = audit["moves"]["416"]
assert move["conditionalBranches"] == 2
assert move["backgroundIds"] == [28, 29, 30]
assert move["programCommands"] == 35
assert not move["untranslated"]

# Control-flow mnemonics are also assembler macros in expansion. They must be
# preserved symbolically until the Pokegochi VM compiler consumes them; fully
# expanding them would leave only .byte/.4byte directives and erase branches.
for opcode in ("jumpargeq", "jumprettrue", "jumpretfalse", "jumpreteq"):
    assert f'"{opcode}"' in generator
assert "parts[0].lower() in BATTLE_SCRIPT_OPCODES" in generator

print("Giga Impact side-specific animation contract OK")
