import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
generator = (ROOT / "scripts/generate_battle_animation_data.py").read_text(
    encoding="utf-8"
)
runtime = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
generated = (ROOT / "src/game/BattleAnimationGenerated.inc").read_text(
    encoding="utf-8"
)
source_script = (
    ROOT / ".downloads/pokeemerald-expansion/data/battle_anim_scripts.s"
).read_text(encoding="utf-8")

# Keep the imported ROM-hack choreography intact: one red 0->11->0 cycle,
# three battler shakes and eight radial fire plumes at 32-angle intervals.
script = source_script.split("gBattleAnimMove_LavaPlume::", 1)[1].split(
    "\n\n", 1
)[0]
assert "num_blends=2" in script
assert "target_blend_y=11" in script
assert script.count("AnimTask_ShakeMon2") == 3
phases = [
    int(value)
    for value in re.findall(
        r"createsprite gLavaPlumeSpriteTemplate[^\n]*, (-?\d+)\s*$",
        script,
        flags=re.MULTILINE,
    )
]
assert phases == list(range(0, 256, 32))

# The generated sprite must retain the dedicated callback mapping. Runtime
# derives its real 7..28-frame lifetime from phase, side and native bounds;
# 32 is only a safe generated fallback, never a fixed cutoff.
assert (
    '"AnimLavaPlumeOrbitScatter": '
    '("BattleAnimationMotion::LavaPlumeDrift", 32)'
    in generator
)
rows = [line for line in generated.splitlines() if "gLavaPlumeSpriteTemplate" in line]
assert len(rows) == 1
assert "BattleAnimationMotion::LavaPlumeDrift" in rows[0]
assert ", 32," in rows[0]
assert "battleAnimationLavaPlumeLifetime" in runtime
assert "nativeX < -16 || nativeX > 256" in runtime

# BlendColorCycle is shared by many imported moves. It must obey numBlends
# and its argument-dependent lifetime instead of looping for a generic 128.
assert "battleAnimationBlendColorCycleLifetime" in runtime
assert "battleAnimationBlendColorCycleAmount" in runtime
assert "const uint8_t terminal = cycles == 1U ? target : 0U" in runtime
assert "(void)cycles" not in runtime

print("Lava Plume animation contract OK")
