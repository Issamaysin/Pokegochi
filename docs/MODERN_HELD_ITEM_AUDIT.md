# Modern held-item audit

Request index: `R20261106-373`

Scope: all 23 ordinary held items introduced after Emerald and currently
available through Pokegochi's region-gated Mart rotation. The expected
behaviour was checked item by item against the bundled
`pokeemerald-expansion` source and exercised through the native battle engine.

| Generation | Item | Verified battle contract | Regression coverage |
| --- | --- | --- | --- |
| IV | Life Orb | 30% regular-damage boost; 1/10 max-HP recoil after damage, including Substitute; Magic Guard and a Sheer Force-boosted move suppress recoil without suppressing power. | Damage comparison, retained item, recoil event and Sheer Force suppression. |
| IV | Focus Sash | At full HP only, a direct knockout hit leaves 1 HP and consumes the item; an already chipped holder is knocked out normally. | Full-HP survival and chipped-holder negative case. |
| IV | Choice Scarf | Multiplies Speed by 1.5 and locks the holder into the first selected move until it leaves the field or loses item effect. | Action order and selection lock. |
| IV | Choice Specs | Multiplies Sp. Atk by 1.5 and uses the shared Choice lock. | Special-damage comparison and shared lock path. |
| IV | Expert Belt | Multiplies only super-effective regular damage by 1.2. | Super-effective damage comparison. |
| IV | Black Sludge | Poison holder heals 1/16 max HP; other types lose 1/8; Heal Block and Magic Guard are respected. | Poison heal and non-Poison damage. |
| IV | Toxic Orb | Badly poisons a legal, status-free holder at end of turn; type, Ability, weather and Misty Terrain immunities are respected. | Ordinary activation and Corrosion interaction. |
| IV | Flame Orb | Burns a legal, status-free holder at end of turn; Fire type and burn-prevention Abilities are respected. | Ordinary activation. |
| V | Eviolite | Multiplies both defenses by 1.5 whenever the species has a valid evolution. | Special-defense damage comparison; physical and special paths share the modifier. |
| V | Rocky Helmet | Every real contact hit costs the attacker 1/6 max HP; Substitute, Protective Pads and Magic Guard suppress it. | Contact recoil and Protective Pads suppression. |
| V | Air Balloon | Grants Ground immunity until a damaging hit reaches the holder or its Substitute; Disguise also pops it; entry announces the Balloon without consuming it. | Ground immunity, ordinary pop and Substitute pop. |
| V | Red Card | A surviving holder consumes it after the last hit and forces the living attacker out; Sheer Force suppresses it; Substitute suppresses it; Suction Cups/Ingrain consume it but block the switch; a wild holder ends the encounter. | Trainer switch, wild escape, anchored attacker and Sheer Force negative case. |
| V | Eject Button | A surviving holder consumes it after the last hit and leaves the field; player chooses a PvE replacement; it still works if the attacker faints to recoil; wild holders and teams without reserves do not activate it; Sheer Force suppresses it. | Opponent switch, player pending picker and recoil-faint attacker. |
| VI | Assault Vest | Multiplies Sp. Def by 1.5 and blocks status move selection/action; Me First remains legal because it calls the target's damaging move. | Damage comparison, blocked status move and Me First exception. |
| VI | Weakness Policy | After real super-effective HP damage, consumes and attempts +2 Attack/+2 Sp. Atk; Substitute and neutral/fixed effectiveness do not trigger it. | Super-effective activation and Substitute negative case. |
| VI | Safety Goggles | Blocks powder moves, Effect Spore, sandstorm damage and hail damage. | Powder immunity and sandstorm immunity; all four users share the same guards. |
| VII | Terrain Extender | Terrain created by the holder lasts eight turns. | Move-created terrain duration; Surge uses the same duration helper. |
| VII | Protective Pads | Blocks only effects caused by making contact, including Rocky Helmet, contact Abilities and King's/Spiky/Baneful shields; does not suppress the attacker's Poison Touch and no longer announces itself on inert contact. | Rocky Helmet suppression, real activation event and inert-contact negative case. |
| VII | Adrenaline Orb | If Intimidate actually lowers Attack, consumes and attempts +1 Speed; blocked Intimidate does not consume it. | Entry Intimidate, Attack drop, Speed raise and consumption. |
| VII | Electric Seed | Electric Terrain consumes it for +1 Defense, even if the holder is Flying/Levitate; stays held if the resulting stage cannot change. | Ungrounded activation and capped-stage negative case. |
| VII | Grassy Seed | Grassy Terrain consumes it for +1 Defense, independent of grounding. | Ungrounded activation. |
| VII | Misty Seed | Misty Terrain consumes it for +1 Sp. Def, independent of grounding. | Ungrounded activation. |
| VII | Psychic Seed | Psychic Terrain consumes it for +1 Sp. Def, independent of grounding. | Ungrounded activation. |

## Shared invariants

- Embargo, Klutz and Magic Room suppress every item's effect through
  `heldItemFor`; the item remains physically present for Knock Off, Trick and
  related moves.
- Consumable items prime Recycle/Unburden through the common `consumeHeld`
  path.
- Red Card and Eject Button resolve only after the complete multi-hit move.
- PvP keeps deterministic automatic Eject Button replacement for now because
  its two-peer command protocol has no symmetric mid-turn party-choice packet;
  all PvE modes use the original explicit party picker.
