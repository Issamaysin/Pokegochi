#include "services/PersistentSave.h"
#include <Preferences.h>
#include <cassert>
#include <cstddef>
#include <cstring>

namespace {
struct MartStateLegacy7Test { MartOffer offers[7]{}; uint32_t elapsedSeconds=86400; uint32_t day=0; uint32_t rng=0x4D415254U; };
struct MartStateLegacy10Test { MartOffer offers[10]{}; uint32_t elapsedSeconds=86400; uint32_t day=0; uint32_t rng=0x4D415254U; };
struct MartStateLegacy15Test { MartOffer offers[15]{}; uint32_t elapsedSeconds=86400; uint32_t day=0; uint32_t rng=0x4D415254U; };
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
  GymProgress gymProgress; uint32_t money; MartStateLegacy7Test mart;
};
struct RecordV9Test { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV9Test payload; uint32_t crc; };
// Exact V21 record layout: this regression test proves a board with the beta
// firmware can receive the EV update without losing its permanent save.
struct OwnedPokemonV21Test {
  uint32_t uid = kEmptyPokemonUid; uint32_t personality = 0; uint16_t speciesId = 0; uint32_t experience = 0;
  uint16_t currentHp = 0; uint16_t maximumHp = 0; uint8_t level = 1; StatusCondition status = StatusCondition::None;
  uint8_t fullness = 80; uint8_t happiness = 80; uint32_t careRemainderSeconds = 0; uint32_t careTickCounter = 0;
  uint32_t recoverySecondsRemaining = 0; MoveId moves[kMoveSlots] = {MoveId::None, MoveId::None, MoveId::None, MoveId::None};
  uint8_t movePp[kMoveSlots]{}; uint8_t abilityId = 0; IndividualValues ivs{};
  PokemonNature nature = PokemonNature::Hardy; HeldItem heldItem = HeldItem::None; bool shiny = false;
};
struct PokemonCollectionV21Test { OwnedPokemonV21Test box[kBoxCapacity]{}; uint32_t party[kPartyCapacity]{}; uint32_t nextUid = 1; };
struct CombatVolatileV21Test {
  int8_t attackStage=0,defenseStage=0,spAttackStage=0,spDefenseStage=0,speedStage=0,accuracyStage=0,evasionStage=0;
  uint8_t confusionTurns=0,trappedTurns=0,criticalStage=0;bool flinched=false,protectedThisTurn=false,recharging=false;
  uint16_t substituteHp=0;MoveId disabledMove=MoveId::None,encoreMove=MoveId::None,chargingMove=MoveId::None;
  MoveId choiceMove=MoveId::None;HeldItem heldItemOverride=HeldItem::None;bool hasHeldItemOverride=false,heldItemSuppressed=false;
};
struct BattleStateV21Test {
  bool active = false; BattleKind kind = BattleKind::None; BattleOutcome outcome = BattleOutcome::None; uint32_t playerUid = 0;
  OwnedPokemonV21Test opponents[kOpponentTeamCapacity]{}; uint8_t opponentCount = 0; uint8_t opponentIndex = 0;
  uint8_t trainerProfileId = 0xFF; uint8_t gymId = 0xFF; CombatVolatileV21Test playerVolatile{};
  BattleHeldItemState playerHeldItems[kPartyCapacity]{}; CombatVolatileV21Test opponentVolatiles[kOpponentTeamCapacity]{};
  uint32_t rngState = 0xA341316CU; uint16_t turn = 0; uint32_t rewardMoney = 0; uint8_t opponentItemUses = 0;
  uint8_t gymStage = 0; uint8_t leagueRegion = 0; uint8_t leagueStage = 0; uint8_t unlockedGeneration = 1;
};
struct GameSaveV21Test {
  uint32_t playTimeSeconds = 0; uint32_t bootCount = 0; uint8_t flags = 0; uint8_t activePetSlot = 0;
  PokemonCollectionV21Test collection{}; Inventory inventory{}; EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{}; BattleStateV21Test battle{}; PokedexState pokedex{};
  GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy7Test mart{}; MoveLearningQueue moveLearning{};
  EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
};
struct RecordV21Test { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV21Test payload; uint32_t crc; };
struct BattleStateV33Test {
  alignas(BattleState) uint8_t bytes[684]{};
};
struct GameSaveV32Test {
  uint32_t playTimeSeconds=0,bootCount=0;uint8_t flags=0,activePetSlot=0;
  PokemonCollection collection{};Inventory inventory{};EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};BattleStateV33Test battle{};PokedexState pokedex{};
  GymProgress gymProgress{};uint32_t money=3000;MartStateLegacy10Test mart{};
  MoveLearningQueue moveLearning{};EvolutionQueue evolutionQueue{};EggState egg{};UserSettings settings{};
  uint64_t ownedMachines=0;PlayerStatistics statistics{};uint32_t martSeenDay=0;
};
struct RecordV32Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV32Test payload;uint32_t crc;};
struct GameSaveV33Test : GameSaveV32Test { PpItemInventory ppItems{}; };
struct RecordV33Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV33Test payload;uint32_t crc;};
struct BattleStateV34Test {alignas(BattleState) uint8_t bytes[768]{};};
struct GameSaveV34Test {
  uint32_t playTimeSeconds=0,bootCount=0;uint8_t flags=0,activePetSlot=0;
  PokemonCollection collection{};Inventory inventory{};EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};BattleStateV34Test battle{};PokedexState pokedex{};
  GymProgress gymProgress{};uint32_t money=3000;MartStateLegacy10Test mart{};
  MoveLearningQueue moveLearning{};EvolutionQueue evolutionQueue{};EggState egg{};UserSettings settings{};
  uint64_t ownedMachines=0;PlayerStatistics statistics{};uint32_t martSeenDay=0;PpItemInventory ppItems{};
};
struct RecordV34Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV34Test payload;uint32_t crc;};
struct GameSaveV35Test {
  uint32_t playTimeSeconds=0,bootCount=0;uint8_t flags=0,activePetSlot=0;
  PokemonCollection collection{};Inventory inventory{};EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};BattleState battle{};PokedexState pokedex{};
  GymProgress gymProgress{};uint32_t money=3000;MartStateLegacy15Test mart{};
  MoveLearningQueue moveLearning{};EvolutionQueue evolutionQueue{};EggState egg{};UserSettings settings{};
  uint64_t ownedMachines=0;PlayerStatistics statistics{};uint32_t martSeenDay=0;PpItemInventory ppItems{};
};
struct RecordV35Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV35Test payload;uint32_t crc;};
struct GameSaveV36Test {
  uint32_t playTimeSeconds=0,bootCount=0;uint8_t flags=0,activePetSlot=0;
  PokemonCollection collection{};Inventory inventory{};EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};BattleState battle{};PokedexState pokedex{};
  GymProgress gymProgress{};uint32_t money=3000;MartStateLegacy15Test mart{};
  MoveLearningQueue moveLearning{};EvolutionQueue evolutionQueue{};EggState egg{};UserSettings settings{};
  uint64_t ownedMachines=0;PlayerStatistics statistics{};uint32_t martSeenDay=0;PpItemInventory ppItems{};
  HomePetPlacementState homePetPlacement{};
};
struct RecordV36Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV36Test payload;uint32_t crc;};
static_assert(sizeof(BattleStateV33Test)==684);
static_assert(sizeof(GameSaveV33Test)==25912);
static_assert(sizeof(RecordV33Test)==25936);
static_assert(sizeof(GameSaveV34Test)==26000);
static_assert(offsetof(RecordV34Test,crc)==26016);
static_assert(sizeof(RecordV34Test)==26024);
static_assert(sizeof(GameSaveV35Test)==26032);
static_assert(offsetof(RecordV35Test,crc)==26048);
static_assert(sizeof(RecordV35Test)==26056);
static_assert(sizeof(GameSaveV36Test)==26040);
static_assert(offsetof(RecordV36Test,crc)==26056);
static_assert(sizeof(RecordV36Test)==26064);
uint32_t crc32(const uint8_t* data,size_t length){uint32_t crc=0xFFFFFFFFU;for(size_t i=0;i<length;++i){crc^=data[i];for(uint8_t bit=0;bit<8;++bit)crc=(crc>>1U)^(0xEDB88320U&(0U-(crc&1U)));}return ~crc;}
}

int main() {
  Preferences::resetTestStorage();

  PersistentSave firstBoot;
  assert(firstBoot.begin());
  GameSave original;
  assert(firstBoot.loadOrCreate(original) == SaveLoadResult::FirstStart);
  assert((original.ownedMachines & kMegaStoneOwnershipBit) == 0U);
  assert(original.inventory.medicine[static_cast<uint8_t>(BattleItem::Potion)] == 5);
  assert(CollectionLogic::chooseStarter(original.collection, 7));
  uint32_t capturedUid = 0;
  assert(CollectionLogic::add(
      original.collection, CollectionLogic::createPokemon(0, 16, 4), &capturedUid));
  assert(CollectionLogic::setPartySlot(original.collection, 1, capturedUid));
  original.playTimeSeconds = 1234;
  original.encounterCharges.available = 1;
  original.inventory.balls[static_cast<uint8_t>(PokeBallType::PokeBall)] = 6;
  original.inventory.heldItems[static_cast<uint8_t>(HeldItem::ShellBell)] = 2;
  original.ppItems.quantities[ppInventoryIndex(BattleItem::Ether)] = 7;
  original.settings.homeBackground = 19;
  original.settings.brightnessPercent = 40;
  original.settings.setScreenTimeout(3);
  original.settings.setAutoBattleText(true);
  original.settings.setColorTheme(UiColorTheme::Emerald);
  original.homePetPlacement.background = 19;
  original.homePetPlacement.validMask = 0x03;
  original.homePetPlacement.gridX[0] = 4;
  original.homePetPlacement.gridY[0] = 6;
  original.homePetPlacement.gridX[1] = 12;
  original.homePetPlacement.gridY[1] = 3;
  assert(original.settings.screenTimeout() == 3);
  assert(original.settings.autoBattleText());
  assert(original.settings.colorTheme() == UiColorTheme::Emerald);
  CollectionLogic::active(original.collection,0)->heldItem=HeldItem::Leftovers;
  original.battle.active = true;
  original.battle.kind = BattleKind::Trainer;
  original.battle.outcome = BattleOutcome::Ongoing;
  original.battle.playerUid = original.collection.party[0];
  original.battle.opponentCount = 2;
  original.battle.playerSafeguardTurns = 3;
  original.battle.opponentSafeguardTurns = 2;
  original.battle.playerReflectTurns = 4;
  original.battle.opponentLightScreenTurns = 3;
  original.battle.playerMistTurns = 2;
  original.battle.playerNightmare = true;
  original.battle.opponentNightmares[0] = true;
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
  assert(loaded.inventory.heldItems[static_cast<uint8_t>(HeldItem::ShellBell)]==2);
  assert(loaded.ppItems.quantities[ppInventoryIndex(BattleItem::Ether)]==7);
  assert(loaded.settings.homeBackground == 19);
  assert(loaded.settings.brightnessPercent == 40);
  assert(loaded.settings.screenTimeout() == 3);
  assert(loaded.settings.autoBattleText());
  assert(loaded.settings.colorTheme() == UiColorTheme::Emerald);
  assert(loaded.homePetPlacement.background == 19);
  assert(loaded.homePetPlacement.validMask == 0x03);
  assert(loaded.homePetPlacement.gridX[0] == 4 && loaded.homePetPlacement.gridY[0] == 6);
  assert(loaded.homePetPlacement.gridX[1] == 12 && loaded.homePetPlacement.gridY[1] == 3);
  loaded.settings.setScreenTimeout(2);
  assert(loaded.settings.colorTheme() == UiColorTheme::Emerald);
  loaded.settings.setAutoBattleText(false);
  assert(loaded.settings.colorTheme() == UiColorTheme::Emerald);
  assert(loaded.settings.screenTimeout() == 2);
  assert(CollectionLogic::active(loaded.collection,0)->heldItem==HeldItem::Leftovers);
  assert(CollectionLogic::active(loaded.collection, 0)->speciesId == 7);
  assert(CollectionLogic::active(loaded.collection, 1)->speciesId == 16);
  assert(CollectionLogic::active(loaded.collection, 0)->personality ==
         CollectionLogic::active(original.collection, 0)->personality);
  assert(CollectionLogic::active(loaded.collection, 0)->nature ==
         CollectionLogic::active(original.collection, 0)->nature);
  assert(std::memcmp(&CollectionLogic::active(loaded.collection, 0)->ivs,
                     &CollectionLogic::active(original.collection, 0)->ivs,
                     sizeof(IndividualValues)) == 0);
  assert(loaded.battle.active && loaded.battle.kind == BattleKind::Trainer);
  assert(loaded.battle.opponentCount == 2 && loaded.battle.opponents[0].speciesId == 19);
  assert(loaded.battle.playerSafeguardTurns == 3 && loaded.battle.opponentSafeguardTurns == 2);
  assert(loaded.battle.playerReflectTurns == 4 && loaded.battle.opponentLightScreenTurns == 3 &&
         loaded.battle.playerMistTurns == 2);
  assert(loaded.battle.playerNightmare && loaded.battle.opponentNightmares[0]);
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

  // A real V21 record differs only by the six EV bytes per Pokémon. It must
  // migrate all established game state and begin every pre-update Pokémon at
  // zero EVs, rather than forcing a second/new save.
  Preferences::resetTestStorage();
  RecordV21Test old21{}; old21.magic=0x504F4B45;old21.version=21;old21.payloadSize=sizeof(GameSaveV21Test);old21.sequence=87;
  const OwnedPokemon currentStarter=CollectionLogic::createPokemon(19,7,23,false,9876);
  std::memcpy(&old21.payload.collection.box[0],&currentStarter,sizeof(OwnedPokemonV21Test));
  old21.payload.collection.party[0]=19;old21.payload.collection.nextUid=20;old21.payload.playTimeSeconds=4321;
  old21.payload.money=7654;old21.payload.settings.homeBackground=7;
  old21.crc=crc32(reinterpret_cast<const uint8_t*>(&old21),offsetof(RecordV21Test,crc));
  assert(raw.putBytes("save_a",&old21,sizeof(old21))==sizeof(old21));assert(raw.putBool("started",true)==1);
  PersistentSave evMigrator;assert(evMigrator.begin());GameSave evMigrated;
  assert(evMigrator.loadOrCreate(evMigrated)==SaveLoadResult::Loaded);
  const OwnedPokemon* evMigratedStarter=CollectionLogic::active(evMigrated.collection,0);
  assert(evMigratedStarter&&evMigratedStarter->speciesId==7&&evMigratedStarter->level==23);
  assert(CollectionLogic::totalEffortValues(*evMigratedStarter)==0);
  assert(evMigrated.playTimeSeconds==4321&&evMigrated.money==7654&&evMigrated.settings.homeBackground==7);

  // V33 adds only PP-item stacks. A V32 board must preserve the entire save,
  // start those stacks empty and clear the formerly-reserved per-Pokemon bits
  // before they become PP Up counters.
  Preferences::resetTestStorage();
  RecordV32Test old32{};old32.magic=0x504F4B45;old32.version=32;
  old32.payloadSize=sizeof(GameSaveV32Test);old32.sequence=99;
  CollectionLogic::initialize(old32.payload.collection);
  assert(CollectionLogic::chooseStarter(old32.payload.collection,1));
  old32.payload.collection.box[0].legacyCareCounterReserved=0xFFFFFFFFU;
  old32.payload.playTimeSeconds=7654;old32.payload.money=9876;
  old32.crc=crc32(reinterpret_cast<const uint8_t*>(&old32),offsetof(RecordV32Test,crc));
  assert(raw.putBytes("save_a",&old32,sizeof(old32))==sizeof(old32));assert(raw.putBool("started",true)==1);
  PersistentSave ppMigrator;assert(ppMigrator.begin());GameSave ppMigrated;
  assert(ppMigrator.loadOrCreate(ppMigrated)==SaveLoadResult::Loaded);
  assert(ppMigrated.playTimeSeconds==7654&&ppMigrated.money==9876);
  assert(CollectionLogic::active(ppMigrated.collection,0)->legacyCareCounterReserved==0);
  for(uint8_t i=0;i<kPpItemCount;++i)assert(ppMigrated.ppItems.quantities[i]==0);

  // V33 was shipped with a 684-byte BattleState prefix. Later V34-only
  // fields must never make this installed record appear corrupt.
  Preferences::resetTestStorage();
  RecordV33Test old33{};old33.magic=0x504F4B45;old33.version=33;
  old33.payloadSize=sizeof(GameSaveV33Test);old33.sequence=37679;
  CollectionLogic::initialize(old33.payload.collection);
  assert(CollectionLogic::chooseStarter(old33.payload.collection,7));
  old33.payload.playTimeSeconds=8765;old33.payload.money=12345;
  old33.payload.ppItems.quantities[0]=4;
  old33.crc=crc32(reinterpret_cast<const uint8_t*>(&old33),offsetof(RecordV33Test,crc));
  assert(raw.putBytes("save_b",&old33,sizeof(old33))==sizeof(old33));assert(raw.putBool("started",true)==1);
  PersistentSave v33Migrator;assert(v33Migrator.begin());GameSave v33Migrated;
  assert(v33Migrator.loadOrCreate(v33Migrated)==SaveLoadResult::Loaded);
  assert(v33Migrated.playTimeSeconds==8765&&v33Migrated.money==12345);
  assert(CollectionLogic::active(v33Migrated.collection,0)->speciesId==7);
  assert(v33Migrated.ppItems.quantities[0]==4);

  // V35 grows only the Mart from ten to fifteen products. A real V34 save
  // keeps its complete collection/progression and expires only the obsolete
  // ten-entry shop rotation so the next opening can populate all 15 entries.
  Preferences::resetTestStorage();
  RecordV34Test old34{};old34.magic=0x504F4B45;old34.version=34;
  old34.payloadSize=sizeof(GameSaveV34Test);old34.sequence=41234;
  CollectionLogic::initialize(old34.payload.collection);
  assert(CollectionLogic::chooseStarter(old34.payload.collection,4));
  old34.payload.playTimeSeconds=23456;old34.payload.money=65432;
  old34.payload.mart.offers[0]={MartItem::PokeBall,HeldItem::None,200,7};
  old34.payload.mart.day=9;old34.payload.mart.rng=0x12345678U;
  old34.payload.ppItems.quantities[ppInventoryIndex(BattleItem::MaxEther)]=3;
  old34.crc=crc32(reinterpret_cast<const uint8_t*>(&old34),offsetof(RecordV34Test,crc));
  assert(raw.putBytes("save_a",&old34,sizeof(old34))==sizeof(old34));
  assert(raw.putBool("started",true)==1);
  PersistentSave v34Migrator;assert(v34Migrator.begin());GameSave v34Migrated;
  assert(v34Migrator.loadOrCreate(v34Migrated)==SaveLoadResult::Loaded);
  assert(v34Migrated.playTimeSeconds==23456&&v34Migrated.money==65432);
  assert(CollectionLogic::active(v34Migrated.collection,0)->speciesId==4);
  assert(v34Migrated.ppItems.quantities[ppInventoryIndex(BattleItem::MaxEther)]==3);
  assert(v34Migrated.mart.day==9&&v34Migrated.mart.rng==0x12345678U);
  assert(v34Migrated.mart.offers[0].remaining==0);

  // V36 adds only manual Home coordinates. A V35 save keeps every gameplay
  // field and starts with no custom placement until the player uses MOVE.
  Preferences::resetTestStorage();
  RecordV35Test old35{};old35.magic=0x504F4B45;old35.version=35;
  old35.payloadSize=sizeof(GameSaveV35Test);old35.sequence=51235;
  CollectionLogic::initialize(old35.payload.collection);
  assert(CollectionLogic::chooseStarter(old35.payload.collection,1));
  old35.payload.playTimeSeconds=34567;old35.payload.money=76543;
  old35.payload.mart.offers[0]={MartItem::UltraBall,HeldItem::None,1200,4};
  old35.payload.ppItems.quantities[ppInventoryIndex(BattleItem::Elixir)]=2;
  old35.crc=crc32(reinterpret_cast<const uint8_t*>(&old35),offsetof(RecordV35Test,crc));
  assert(raw.putBytes("save_b",&old35,sizeof(old35))==sizeof(old35));
  assert(raw.putBool("started",true)==1);
  PersistentSave v35Migrator;assert(v35Migrator.begin());GameSave v35Migrated;
  assert(v35Migrator.loadOrCreate(v35Migrated)==SaveLoadResult::Loaded);
  assert(v35Migrated.playTimeSeconds==34567&&v35Migrated.money==76543);
  assert(CollectionLogic::active(v35Migrated.collection,0)->speciesId==1);
  assert(v35Migrated.mart.offers[0].remaining==0);
  assert(v35Migrated.ppItems.quantities[ppInventoryIndex(BattleItem::Elixir)]==2);
  assert(v35Migrated.homePetPlacement.background==0xFF&&v35Migrated.homePetPlacement.validMask==0);

  // V37 grows the current V36 Mart from fifteen to twenty offers. Collection,
  // progress and manual Home placement survive; only the incomplete old
  // rotation is expired so the next opening fills all four pages.
  Preferences::resetTestStorage();
  RecordV36Test old36{};old36.magic=0x504F4B45;old36.version=36;
  old36.payloadSize=sizeof(GameSaveV36Test);old36.sequence=61236;
  CollectionLogic::initialize(old36.payload.collection);
  assert(CollectionLogic::chooseStarter(old36.payload.collection,7));
  old36.payload.playTimeSeconds=45678;old36.payload.money=87654;
  old36.payload.mart.offers[0]={MartItem::GreatBall,HeldItem::None,600,5};
  old36.payload.mart.day=23;old36.payload.mart.rng=0xCAFEBABEU;
  old36.payload.homePetPlacement.background=4;old36.payload.homePetPlacement.validMask=1;
  old36.payload.homePetPlacement.gridX[0]=9;old36.payload.homePetPlacement.gridY[0]=5;
  old36.crc=crc32(reinterpret_cast<const uint8_t*>(&old36),offsetof(RecordV36Test,crc));
  assert(raw.putBytes("save_a",&old36,sizeof(old36))==sizeof(old36));
  assert(raw.putBool("started",true)==1);
  PersistentSave v36Migrator;assert(v36Migrator.begin());GameSave v36Migrated;
  assert(v36Migrator.loadOrCreate(v36Migrated)==SaveLoadResult::Loaded);
  assert(v36Migrated.playTimeSeconds==45678&&v36Migrated.money==87654);
  assert(CollectionLogic::active(v36Migrated.collection,0)->speciesId==7);
  assert(v36Migrated.mart.day==23&&v36Migrated.mart.rng==0xCAFEBABEU);
  assert(v36Migrated.mart.offers[0].remaining==0);
  assert(v36Migrated.homePetPlacement.background==4&&v36Migrated.homePetPlacement.validMask==1);
  assert(v36Migrated.homePetPlacement.gridX[0]==9&&v36Migrated.homePetPlacement.gridY[0]==5);
  return 0;
}
