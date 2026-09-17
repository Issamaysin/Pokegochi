# FireRed compatibility policy

The generated move table contains all 354 FireRed moves and every Generation-I
level-up learnset. Battle effects are handled as follows.

- Native: damage, Gen-III physical/special split, STAB, full type chart, priority,
  accuracy/evasion, critical hits, PP, status, stages, recoil, drain, healing,
  multi-hit, fixed damage, OHKO, trapping, charging/recharge, Protect, Endure,
  Safeguard, Reflect, Light Screen, Mist, Substitute, Disable, Encore,
  confusion, flinch, shared EXP and switching.
- Extended native: False Swipe, Explosion, Haze, Heal Bell, Refresh, Belly Drum,
  Pain Split, Endeavor, Facade, Return/Frustration, Flail/Reversal, Eruption,
  Dream Eater, SmellingSalt, Magnitude, Low Kick, Psywave, stat-combination moves,
  Mean Look, Rapid Spin, Roar, Attract, Beat Up, Magic Coat, Snatch, Pursuit,
  Memento and terrain-dependent Secret Power.
- Adapted: doubles-only targeting resolves as a normal single-battle action;
  overworld-only effects have no exploration side effect; breeding/contest-only
  behavior is intentionally absent. Trick follows FireRed's item exchange in
  PvE and updates the player's held item; an NPC/wild opponent may not use it
  to steal from the player. PvP item changes remain battle-local so a battle
  can never replace the explicit, mutually confirmed Trade flow. Thief, Covet
  and Knock Off retain their battle-local anti-loss adaptation.
- Abilities use their battle behavior when relevant. Exploration-only abilities
  do not create loot or alter the explicit encounter timers. Run Away, Intimidate,
  Natural Cure, contact abilities, absorptions, immunities, Shed Skin, Speed Boost,
  Synchronize and core stat/damage abilities are active.

All moves are assigned an original FireRed visual family. Exact sprite-script timing
is allowed to be calibrated after physical-screen observation without changing game
balance or save data.

The exhaustive audit of FireRed's 67 `MOVE_TARGET_USER` moves, including the
dedicated handlers that are not yet declared complete, is maintained in
`docs/SELF_TARGET_MOVE_AUDIT.md`.
