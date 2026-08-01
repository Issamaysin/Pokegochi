#include "services/PersistentSave.h"
#include <Preferences.h>
#include <cassert>

int main() {
  Preferences::resetTestStorage();

  PersistentSave firstBoot;
  assert(firstBoot.begin());
  GameSave original;
  assert(firstBoot.loadOrCreate(original) == SaveLoadResult::FirstStart);
  assert(CollectionLogic::chooseStarter(original.collection, 7));
  uint32_t capturedUid = 0;
  assert(CollectionLogic::add(
      original.collection, CollectionLogic::createPokemon(0, 16, 4), &capturedUid));
  assert(CollectionLogic::setPartySlot(original.collection, 1, capturedUid));
  original.playTimeSeconds = 1234;
  original.encounterCharges.available = 1;
  original.inventory.balls[static_cast<uint8_t>(PokeBallType::PokeBall)] = 6;
  original.battle.active = true;
  original.battle.kind = BattleKind::Trainer;
  original.battle.outcome = BattleOutcome::Ongoing;
  original.battle.playerUid = original.collection.party[0];
  original.battle.opponentCount = 2;
  original.battle.opponents[0] = CollectionLogic::createPokemon(0, 19, 3);
  original.battle.opponents[1] = CollectionLogic::createPokemon(0, 16, 4);
  PokedexLogic::markCaught(original.pokedex, 7);
  assert(firstBoot.commit(original));

  // A fresh firmware instance must load the same single save, including the
  // collection, inventory-related state and Pokedex data.
  PersistentSave reboot;
  assert(reboot.begin());
  GameSave loaded;
  assert(reboot.loadOrCreate(loaded) == SaveLoadResult::Loaded);
  assert(loaded.playTimeSeconds == 1234);
  assert(loaded.encounterCharges.available == 1);
  assert(loaded.inventory.balls[static_cast<uint8_t>(PokeBallType::PokeBall)] == 6);
  assert(CollectionLogic::active(loaded.collection, 0)->speciesId == 7);
  assert(CollectionLogic::active(loaded.collection, 1)->speciesId == 16);
  assert(loaded.battle.active && loaded.battle.kind == BattleKind::Trainer);
  assert(loaded.battle.opponentCount == 2 && loaded.battle.opponents[0].speciesId == 19);
  assert(PokedexLogic::hasCaught(loaded.pokedex, 7));

  // Two alternating commits provide redundant records; the newest valid one
  // wins, while corruption of only that slot falls back to the older slot.
  loaded.playTimeSeconds = 2000;
  assert(reboot.commit(loaded));
  loaded.playTimeSeconds = 3000;
  assert(reboot.commit(loaded));
  Preferences::corruptTestByte("pokegochi", "save_b");
  PersistentSave fallbackBoot;
  assert(fallbackBoot.begin());
  GameSave fallback;
  assert(fallbackBoot.loadOrCreate(fallback) == SaveLoadResult::Loaded);
  assert(fallback.playTimeSeconds == 2000);

  // Once the permanent-start marker exists, losing both records must enter
  // recovery. It must never silently create a second/new game.
  Preferences::corruptTestByte("pokegochi", "save_a");
  PersistentSave corruptedBoot;
  assert(corruptedBoot.begin());
  GameSave untouched;
  untouched.playTimeSeconds = 9999;
  assert(corruptedBoot.loadOrCreate(untouched) == SaveLoadResult::Corrupted);
  assert(untouched.playTimeSeconds == 9999);
  return 0;
}
