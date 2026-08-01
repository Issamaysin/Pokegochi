# FireRed compatibility policy

The generated move table contains all 354 FireRed moves and every Generation-I
level-up learnset. Battle effects are handled as follows.

- Native: damage, Gen-III physical/special split, STAB, full type chart, priority,
  accuracy/evasion, critical hits, PP, status, stages, recoil, drain, healing,
  multi-hit, fixed damage, OHKO, trapping, charging/recharge, Protect, Substitute,
  Disable, Encore, confusion, flinch, shared EXP and switching.
- Extended native: False Swipe, Explosion, Haze, Heal Bell, Refresh, Belly Drum,
  Pain Split, Endeavor, Facade, Return/Frustration, Flail/Reversal, Eruption,
  Dream Eater, SmellingSalt, Magnitude, Low Kick, Psywave, stat-combination moves,
  Mean Look, Rapid Spin and Roar.
- Adapted: doubles-only targeting resolves as a normal single-battle action;
  overworld-only effects have no exploration side effect; held-item theft/cycling
  resolves as its battle damage/status component because Pokegochi has no held-item
  equipment system; breeding/contest-only behavior is intentionally absent.
- Abilities use their battle behavior when relevant. Exploration-only abilities
  do not create loot or alter the explicit encounter timers. Run Away, Intimidate,
  Natural Cure, contact abilities, absorptions, immunities, Shed Skin, Speed Boost,
  Synchronize and core stat/damage abilities are active.

All moves are assigned an original FireRed visual family. Exact sprite-script timing
is allowed to be calibrated after physical-screen observation without changing game
balance or save data.
