# Pokegochi V2 acceptance record

This historical checklist records the core V2 milestones. For current behavior,
timers and installation instructions, use the root `README.md` and `GAME_DESIGN.md`.

## Acceptance matrix

- [x] Permanent single save, redundant records, no production reset
- [x] Three regional starter choices, 386-species collection, three-member active team
- [x] Unlimited wild encounters, charged trainer battles and sequential fixed-level Gyms
- [x] Capture, shared EXP, level evolution, persistent status and PP
- [x] Permanent six-stat IVs, all 25 Gen-III Natures and personality-selected Abilities
- [x] Mart, money, consumables, Box, Bag and seen/caught Pokedex
- [x] Badge-gated held-item Mart pool, Bag equipment flow and Gen-III battle effects
- [x] Complete FireRed trainer catalog used by generated trainer battles
- [x] FireRed/Emerald encounter progression plus explicit placement for all 386
- [x] Complete move-effect and relevant ability compatibility matrix
- [x] Strategic opponent move choice and limited item use
- [x] Move-learning replacement choice at four moves
- [x] Three-stage Gym event and complete leader presentation/rewards
- [x] Kanto, Johto and Hoenn Elite Four + Champion gates before regional unlocks
- [x] Regional Pokedex expansion only after the post-League starter choice
- [x] Full Pokedex metadata and detail/evolution pages
- [x] One-time Master Ball reward and visible message for each completed regional Pokedex
- [x] FireRed-composed Home, Battle, Bag, Box, Pokedex and Mart screens
- [x] Every move mapped to original FireRed visual assets plus ball/capture/evolution sequences
- [x] All 354 FireRed move scripts compiled into audited ESP32 animation descriptors
- [x] Save-format migration without exposing a player reset
- [x] Persistent brightness/background settings and automatic display timeout
- [x] Touch wake into a swipe-to-unlock screen and light-sleep clock advancement
- [x] Bluetooth PvP/trading and BLE firmware plus SD-asset updates
- [x] Native regression suite, asset audit and reproducible ESP32 binaries

Each unchecked row must have a native data or behavior test before it is marked complete.
