#include "services/PersistentSave.h"
#include <Preferences.h>
#include <cassert>
#include <cstddef>

namespace {
struct BattleStateV9Test {
  bool active; BattleKind kind; BattleOutcome outcome; uint32_t playerUid;
  OwnedPokemon opponents[kOpponentTeamCapacity]; uint8_t opponentCount; uint8_t opponentIndex;
  uint8_t trainerProfileId; uint8_t gymId; CombatVolatile playerVolatile;
  CombatVolatile opponentVolatiles[kOpponentTeamCapacity]; uint32_t rngState; uint16_t turn; uint32_t rewardMoney;
};
struct GameSaveV9Test {
  uint32_t playTimeSeconds; uint32_t bootCount; uint8_t flags; uint8_t activePetSlot;
  PokemonCollection collection; Inventory inventory; EncounterCharges encounterCharges;
  WildEncounterClock wildEncounterClock; BattleStateV9Test battle; PokedexState pokedex;
  GymProgress gymProgress; uint32_t money; MartState mart;
};
struct RecordV9Test { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV9Test payload; uint32_t crc; };
uint32_t crc32(const uint8_t* data,size_t length){uint32_t crc=0xFFFFFFFFU;for(size_t i=0;i<length;++i){crc^=data[i];for(uint8_t bit=0;bit<8;++bit)crc=(crc>>1U)^(0xEDB88320U&(0U-(crc&1U)));}return ~crc;}
}

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

  // A genuine version-9 record is upgraded in place to version 10. No reset,
  // second save or loss of collection/progression is permitted.
  Preferences::resetTestStorage();
  RecordV9Test old{}; old.magic=0x504F4B45;old.version=9;old.payloadSize=sizeof(GameSaveV9Test);old.sequence=41;
  CollectionLogic::initialize(old.payload.collection);assert(CollectionLogic::chooseStarter(old.payload.collection,4));
  old.payload.playTimeSeconds=777;old.payload.money=5432;old.payload.battle.active=true;
  old.payload.battle.kind=BattleKind::Trainer;old.payload.battle.playerUid=old.payload.collection.party[0];
  old.payload.battle.opponentCount=1;old.payload.battle.opponents[0]=CollectionLogic::createPokemon(0,19,8);
  old.crc=crc32(reinterpret_cast<const uint8_t*>(&old),offsetof(RecordV9Test,crc));
  Preferences raw;assert(raw.begin("pokegochi",false));assert(raw.putBytes("save_a",&old,sizeof(old))==sizeof(old));assert(raw.putBool("started",true)==1);
  PersistentSave migrator;assert(migrator.begin());GameSave migrated;
  assert(migrator.loadOrCreate(migrated)==SaveLoadResult::Loaded);
  assert(migrated.playTimeSeconds==777&&migrated.money==5432);
  assert(CollectionLogic::active(migrated.collection,0)->speciesId==4);
  assert(migrated.battle.active&&migrated.battle.opponents[0].speciesId==19);
  assert(migrated.battle.opponentItemUses==0);
  return 0;
}
