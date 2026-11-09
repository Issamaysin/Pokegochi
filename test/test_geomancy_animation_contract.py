from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
generator = (ROOT / "scripts/generate_battle_animation_data.py").read_text(
    encoding="utf-8"
)
header = (ROOT / "include/game/BattleAnimationData.h").read_text(encoding="utf-8")
runtime = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
generated = (ROOT / "src/game/BattleAnimationGenerated.inc").read_text(
    encoding="utf-8"
)

# Geomancy's charge and release halves must remain distinct VM branches.
assert "{601," in generated
assert "BattleAnimationOpcode::JumpIfMoveTurnEquals" in generated

# SpriteCB_SurroundingRing has no arguments in the ROM hack. It must retain
# its own fixed +40 -> -32 trajectory rather than AnimRaiseSprite's ABI.
assert '"SpriteCB_SurroundingRing":\n        ("BattleAnimationMotion::SurroundingRingRise", 14)' in generator
assert "SurroundingRingRise," in header
assert "case BattleAnimationMotion::SurroundingRingRise:" in runtime
assert "attackerY + 40" in runtime
assert "static_cast<int32_t>(72) * progress / denominator" in runtime

# The regenerated catalog must route the actual Geomancy ring resource to
# the dedicated motion, not merely leave the source mapping unconsumed.
ring_rows = [
    line
    for line in generated.splitlines()
    if "gGeomancyRingTemplate / SpriteCB_SurroundingRing" in line
]
assert len(ring_rows) == 1
assert "BattleAnimationMotion::SurroundingRingRise" in ring_rows[0]

print("Geomancy two-turn animation contract OK")
