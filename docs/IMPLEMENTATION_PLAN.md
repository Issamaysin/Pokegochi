# Pokegochi software completion plan

Scope: all game software that does not require physical-board validation or Bluetooth.

## Acceptance matrix

- [x] Permanent single save, redundant records, no production reset
- [x] Three starters, 151-species collection, three-member active team
- [x] Wild, charged trainer and sequential fixed-level Gym battles
- [x] Capture, shared EXP, level evolution, persistent status and PP
- [x] Mart, money, consumables, Box, Bag and seen/caught Pokedex
- [ ] Complete FireRed trainer catalog used by generated trainer battles
- [ ] FireRed-area encounter progression plus explicit placement for all 151
- [ ] Complete move-effect and relevant ability compatibility matrix
- [ ] Strategic opponent move choice, switching and item use
- [ ] Move-learning replacement choice at four moves
- [ ] Three-stage Gym event and complete leader presentation/rewards
- [ ] Full Pokedex metadata and detail/evolution pages
- [ ] FireRed-composed Home, Battle, Bag, Box, Pokedex and Mart screens
- [ ] Per-move animation scripts and complete ball/capture/evolution sequences
- [ ] Save-format migration without exposing a player reset
- [ ] Native regression suite, asset audit and reproducible ESP32 binaries

Each unchecked row must have a native data or behavior test before it is marked complete.
