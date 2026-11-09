import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
EXPANSION = ROOT / ".downloads/pokeemerald-expansion"

ITEMS = (
    ("LifeOrb", "LIFE_ORB", "life_orb", "life_orb", 4),
    ("FocusSash", "FOCUS_SASH", "focus_sash", "focus_sash", 4),
    ("ChoiceScarf", "CHOICE_SCARF", "choice_scarf", "choice_scarf", 4),
    ("ChoiceSpecs", "CHOICE_SPECS", "choice_specs", "choice_specs", 4),
    ("ExpertBelt", "EXPERT_BELT", "expert_belt", "expert_belt", 4),
    ("BlackSludge", "BLACK_SLUDGE", "black_sludge", "black_sludge", 4),
    ("ToxicOrb", "TOXIC_ORB", "toxic_orb", "toxic_orb", 4),
    ("FlameOrb", "FLAME_ORB", "flame_orb", "flame_orb", 4),
    ("Eviolite", "EVIOLITE", "eviolite", "eviolite", 5),
    ("RockyHelmet", "ROCKY_HELMET", "rocky_helmet", "rocky_helmet", 5),
    ("AirBalloon", "AIR_BALLOON", "air_balloon", "air_balloon", 5),
    ("RedCard", "RED_CARD", "red_card", "red_card", 5),
    ("EjectButton", "EJECT_BUTTON", "eject_button", "eject_button", 5),
    ("AssaultVest", "ASSAULT_VEST", "assault_vest", "assault_vest", 6),
    ("WeaknessPolicy", "WEAKNESS_POLICY", "weakness_policy", "weakness_policy", 6),
    ("SafetyGoggles", "SAFETY_GOGGLES", "safety_goggles", "safety_goggles", 6),
    ("TerrainExtender", "TERRAIN_EXTENDER", "terrain_extender", "terrain_extender", 7),
    ("ProtectivePads", "PROTECTIVE_PADS", "protective_pads", "protective_pads", 7),
    ("AdrenalineOrb", "ADRENALINE_ORB", "adrenaline_orb", "adrenaline_orb", 7),
    ("ElectricSeed", "ELECTRIC_SEED", "terrain_seed", "electric_seed", 7),
    ("GrassySeed", "GRASSY_SEED", "terrain_seed", "grassy_seed", 7),
    ("MistySeed", "MISTY_SEED", "terrain_seed", "misty_seed", 7),
    ("PsychicSeed", "PSYCHIC_SEED", "terrain_seed", "psychic_seed", 7),
)


def main() -> None:
    assert EXPANSION.is_dir(), "pokeemerald-expansion source checkout is required"
    rom_items = (EXPANSION / "src/data/items.h").read_text(encoding="utf-8")
    held_header = (ROOT / "include/game/HeldItems.h").read_text(encoding="utf-8")
    held_source = (ROOT / "src/game/HeldItems.cpp").read_text(encoding="utf-8")
    battle = (ROOT / "src/game/BattleEngine.cpp").read_text(encoding="utf-8")
    audit = (ROOT / "docs/MODERN_HELD_ITEM_AUDIT.md").read_text(encoding="utf-8")
    builder = (ROOT / "scripts/build_sd_asset_pack.py").read_text(encoding="utf-8")
    save_header = (ROOT / "include/services/PersistentSave.h").read_text(encoding="utf-8")
    save_source = (ROOT / "src/services/PersistentSave.cpp").read_text(encoding="utf-8")
    economy = (ROOT / "src/game/Economy.cpp").read_text(encoding="utf-8")

    assert len(ITEMS) == 23
    for offset, (enum_name, rom_name, icon, palette, generation) in enumerate(ITEMS):
        assert f"ITEM_{rom_name}" in rom_items, rom_name
        assert re.search(rf"\b{enum_name}\b", held_header), enum_name
        assert f"HeldItem::{enum_name}" in held_source, enum_name
        assert f"HeldItem::{enum_name}" in battle, enum_name
        display_name = re.sub(r"(?<!^)(?=[A-Z])", " ", enum_name)
        assert display_name in audit, display_name
        assert (EXPANSION / f"graphics/items/icons/{icon}.png").is_file(), icon
        assert (EXPANSION / f"graphics/items/icon_palettes/{palette}.pal").is_file(), palette
        assert f'("{icon}", "{palette}")' in builder
        assert f"held_{128 + offset:02}.pkg" in builder or "start=128" in builder
        expected_range = {
            4: "item>=HeldItem::LifeOrb&&item<=HeldItem::FlameOrb",
            5: "item>=HeldItem::Eviolite&&item<=HeldItem::EjectButton",
            6: "item>=HeldItem::AssaultVest&&item<=HeldItem::SafetyGoggles",
            7: "item>=HeldItem::TerrainExtender&&item<=HeldItem::PsychicSeed",
        }[generation]
        assert expected_range in held_source

    assert "range(128,151)" in builder.replace(" ", "")
    for snippet in (
        "assaultVestBlocksMove",
        '!effectIs(move,"USE_MOVE_FIRST")',
        "popAirBalloon",
        'abilityIs(affected,"CORROSION")',
        "HeldItem::ProtectivePads",
        "wildHolderEndsBattle",
        "playerContactShield",
        "opponentContactShield",
    ):
        assert snippet in battle, snippet

    assert "ModernHeldItemInventory modernHeldItems{};" in save_header
    format_match = re.search(r"kFormatVersion\s*=\s*(\d+)", save_header)
    assert format_match and int(format_match.group(1)) >= 51
    assert "GameSaveV50" in save_source
    assert "save.modernHeldItems = ModernHeldItemInventory{};" in save_source
    assert "MartItem::Potion" in economy
    assert "heldItemGeneration" in economy
    print("Modern held-item ROM/source/asset contract passed.")


if __name__ == "__main__":
    main()
