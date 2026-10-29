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
  PokemonCollectionV41 collection; Inventory inventory; EncounterCharges encounterCharges;
  WildEncounterClock wildEncounterClock; BattleStateV9Test battle; PokedexStateV41 pokedex;
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
struct PokemonCollectionV21Test { OwnedPokemonV21Test box[kBoxCapacityV41]{}; uint32_t party[kPartyCapacity]{}; uint32_t nextUid = 1; };
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
  WildEncounterClock wildEncounterClock{}; BattleStateV21Test battle{}; PokedexStateV41 pokedex{};
  GymProgress gymProgress{}; uint32_t money = 3000; MartStateLegacy7Test mart{}; MoveLearningQueue moveLearning{};
  EvolutionQueue evolutionQueue{}; EggState egg{}; UserSettings settings{};
};
struct RecordV21Test { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV21Test payload; uint32_t crc; };
struct BattleStateV33Test {
  alignas(BattleState) uint8_t bytes[684]{};
};
struct GameSaveV32Test {
  uint32_t playTimeSeconds=0,bootCount=0;uint8_t flags=0,activePetSlot=0;
  PokemonCollectionV41 collection{};Inventory inventory{};EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};BattleStateV33Test battle{};PokedexStateV41 pokedex{};
  GymProgress gymProgress{};uint32_t money=3000;MartStateLegacy10Test mart{};
  MoveLearningQueue moveLearning{};EvolutionQueue evolutionQueue{};EggState egg{};UserSettings settings{};
  uint64_t ownedMachines=0;PlayerStatistics statistics{};uint32_t martSeenDay=0;
};
struct RecordV32Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV32Test payload;uint32_t crc;};
struct GameSaveV33Test : GameSaveV32Test { PpItemInventory ppItems{}; };
struct RecordV33Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV33Test payload;uint32_t crc;};
struct BattleStateV34Test {alignas(BattleState) uint8_t bytes[768]{};};
BattleState& legacyBattle(BattleStateV34Test& state) {
  return *reinterpret_cast<BattleState*>(state.bytes);
}
struct GameSaveV34Test {
  uint32_t playTimeSeconds=0,bootCount=0;uint8_t flags=0,activePetSlot=0;
  PokemonCollectionV41 collection{};Inventory inventory{};EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};BattleStateV34Test battle{};PokedexStateV41 pokedex{};
  GymProgress gymProgress{};uint32_t money=3000;MartStateLegacy10Test mart{};
  MoveLearningQueue moveLearning{};EvolutionQueue evolutionQueue{};EggState egg{};UserSettings settings{};
  uint64_t ownedMachines=0;PlayerStatistics statistics{};uint32_t martSeenDay=0;PpItemInventory ppItems{};
};
struct RecordV34Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV34Test payload;uint32_t crc;};
struct GameSaveV35Test {
  uint32_t playTimeSeconds=0,bootCount=0;uint8_t flags=0,activePetSlot=0;
  PokemonCollectionV41 collection{};Inventory inventory{};EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};BattleStateV34Test battle{};PokedexStateV41 pokedex{};
  GymProgress gymProgress{};uint32_t money=3000;MartStateLegacy15Test mart{};
  MoveLearningQueue moveLearning{};EvolutionQueue evolutionQueue{};EggState egg{};UserSettings settings{};
  uint64_t ownedMachines=0;PlayerStatistics statistics{};uint32_t martSeenDay=0;PpItemInventory ppItems{};
};
struct RecordV35Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV35Test payload;uint32_t crc;};
struct GameSaveV36Test {
  uint32_t playTimeSeconds=0,bootCount=0;uint8_t flags=0,activePetSlot=0;
  PokemonCollectionV41 collection{};Inventory inventory{};EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};BattleStateV34Test battle{};PokedexStateV41 pokedex{};
  GymProgress gymProgress{};uint32_t money=3000;MartStateLegacy15Test mart{};
  MoveLearningQueue moveLearning{};EvolutionQueue evolutionQueue{};EggState egg{};UserSettings settings{};
  uint64_t ownedMachines=0;PlayerStatistics statistics{};uint32_t martSeenDay=0;PpItemInventory ppItems{};
  HomePetPlacementState homePetPlacement{};
};
struct RecordV36Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV36Test payload;uint32_t crc;};
struct GameSaveV37Test {
  uint32_t playTimeSeconds=0,bootCount=0;uint8_t flags=0,activePetSlot=0;
  PokemonCollectionV41 collection{};Inventory inventory{};EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};BattleStateV34Test battle{};PokedexStateV41 pokedex{};
  GymProgress gymProgress{};uint32_t money=3000;MartState mart{};
  MoveLearningQueue moveLearning{};EvolutionQueue evolutionQueue{};EggState egg{};UserSettings settings{};
  uint64_t ownedMachines=0;PlayerStatistics statistics{};uint32_t martSeenDay=0;PpItemInventory ppItems{};
  HomePetPlacementState homePetPlacement{};
};
struct RecordV37Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV37Test payload;uint32_t crc;};
struct GameSaveV38Test {
  uint32_t playTimeSeconds=0,bootCount=0;uint8_t flags=0,activePetSlot=0;
  PokemonCollectionV41 collection{};Inventory inventory{};EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};BattleStateV34Test battle{};PokedexStateV41 pokedex{};
  GymProgress gymProgress{};uint32_t money=3000;MartState mart{};
  MoveLearningQueue moveLearning{};EvolutionQueue evolutionQueue{};EggState egg{};UserSettings settings{};
  uint64_t ownedMachines=0;PlayerStatistics statistics{};uint32_t martSeenDay=0;
  PpItemInventory ppItems{};HomePetPlacementState homePetPlacement{};
  BerryGardenState berryGarden{};CreativeBackgroundState creativeBackgrounds{};
  SpecialHeldItemInventory specialHeldItems{};uint32_t lastNpcGiftDay=UINT32_MAX;
  uint32_t clockLocalEpochSeconds=0;
};
struct RecordV38Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV38Test payload;uint32_t crc;};
struct GameSaveV39Test : GameSaveV38Test { uint32_t bossCooldownSecondsRemaining=0; };
struct RecordV39Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV39Test payload;uint32_t crc;};
struct BossEncounterStateV40Test {
  uint32_t personality=0;uint16_t speciesId=0;uint16_t currentHp=0;
  uint8_t form=0;MegaVariant megaVariant=MegaVariant::None;uint16_t reserved=0;
};
struct GameSaveV40Test : GameSaveV39Test { BossEncounterStateV40Test bossEncounter{}; };
struct RecordV40Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV40Test payload;uint32_t crc;};
// Exact V41 payload before Sinnoh widened the permanent Box and Pokédex.
struct GameSaveV41Test {
  uint32_t playTimeSeconds=0,bootCount=0;uint8_t flags=0,activePetSlot=0;
  PokemonCollectionV41 collection{};Inventory inventory{};EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};BattleStateV34Test battle{};PokedexStateV41 pokedex{};
  GymProgress gymProgress{};uint32_t money=3000;MartState mart{};
  MoveLearningQueue moveLearning{};EvolutionQueue evolutionQueue{};EggState egg{};UserSettings settings{};
  uint64_t ownedMachines=0;PlayerStatistics statistics{};uint32_t martSeenDay=0;
  PpItemInventory ppItems{};HomePetPlacementState homePetPlacement{};
  BerryGardenState berryGarden{};CreativeBackgroundState creativeBackgrounds{};
  SpecialHeldItemInventory specialHeldItems{};uint32_t lastNpcGiftDay=UINT32_MAX;
  uint32_t clockLocalEpochSeconds=0,bossCooldownSecondsRemaining=0;
  BossEncounterState bossEncounter{};EggInventory eggInventory{};
};
struct RecordV41Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV41Test payload;uint32_t crc;};
// Exact V42 payload before Poke Defense appended its independent campaign
// state. Keep this explicit so a complete Sinnoh save cannot be mistaken for
// a current record merely because the prefix happens to look compatible.
struct GameSaveV42Test {
  uint32_t playTimeSeconds=0,bootCount=0;uint8_t flags=0,activePetSlot=0;
  PokemonCollectionV45 collection{};Inventory inventory{};EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};BattleStateV34Test battle{};PokedexStateV45 pokedex{};
  GymProgress gymProgress{};uint32_t money=3000;MartState mart{};
  MoveLearningQueue moveLearning{};EvolutionQueue evolutionQueue{};EggState egg{};UserSettings settings{};
  uint64_t ownedMachines=0;PlayerStatistics statistics{};uint32_t martSeenDay=0;
  PpItemInventory ppItems{};HomePetPlacementState homePetPlacement{};
  BerryGardenState berryGarden{};CreativeBackgroundState creativeBackgrounds{};
  SpecialHeldItemInventory specialHeldItems{};uint32_t lastNpcGiftDay=UINT32_MAX;
  uint32_t clockLocalEpochSeconds=0,bossCooldownSecondsRemaining=0;
  BossEncounterState bossEncounter{};EggInventory eggInventory{};SinnohExpeditionState sinnoh{};
};
struct RecordV42Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV42Test payload;uint32_t crc;};
// Exact final Sinnoh-only record. V46 must expand only Box/Dex capacity and
// append clean Unova state while preserving this complete prefix.
struct BattleStateV45Test {alignas(BattleState) uint8_t bytes[856]{};};
struct GameSaveV45Test {
  uint32_t playTimeSeconds=0,bootCount=0;uint8_t flags=0,activePetSlot=0;
  PokemonCollectionV45 collection{};Inventory inventory{};EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};BattleStateV45Test battle{};PokedexStateV45 pokedex{};
  GymProgress gymProgress{};uint32_t money=3000;MartState mart{};
  MoveLearningQueue moveLearning{};EvolutionQueue evolutionQueue{};EggState egg{};UserSettings settings{};
  uint64_t ownedMachines=0;PlayerStatistics statistics{};uint32_t martSeenDay=0;
  PpItemInventory ppItems{};HomePetPlacementState homePetPlacement{};
  BerryGardenState berryGarden{};CreativeBackgroundState creativeBackgrounds{};
  SpecialHeldItemInventory specialHeldItems{};uint32_t lastNpcGiftDay=UINT32_MAX;
  uint32_t clockLocalEpochSeconds=0,bossCooldownSecondsRemaining=0;
  BossEncounterState bossEncounter{};EggInventory eggInventory{};SinnohExpeditionState sinnoh{};
  TowerDefense::CampaignState towerDefense{};Gen4UniqueItemInventory gen4UniqueItems{};
};
struct RecordV45Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV45Test payload;uint32_t crc;};
// Exact shipped Unova record. V47 may enlarge the Box/Dex and append Kalos
// state, but it must preserve all 720 stored slots and every Gen-V sidecar.
struct BattleStateV46Test {alignas(BattleState) uint8_t bytes[944]{};};
struct GameSaveV46Test {
  uint32_t playTimeSeconds=0,bootCount=0;uint8_t flags=0,activePetSlot=0;
  PokemonCollectionV46 collection{};Inventory inventory{};EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};BattleStateV46Test battle{};PokedexStateV46 pokedex{};
  GymProgress gymProgress{};uint32_t money=3000;MartState mart{};
  MoveLearningQueue moveLearning{};EvolutionQueue evolutionQueue{};EggState egg{};UserSettings settings{};
  uint64_t ownedMachines=0;PlayerStatistics statistics{};uint32_t martSeenDay=0;
  PpItemInventory ppItems{};HomePetPlacementState homePetPlacement{};
  BerryGardenState berryGarden{};CreativeBackgroundState creativeBackgrounds{};
  SpecialHeldItemInventory specialHeldItems{};uint32_t lastNpcGiftDay=UINT32_MAX;
  uint32_t clockLocalEpochSeconds=0,bossCooldownSecondsRemaining=0;
  BossEncounterState bossEncounter{};EggInventory eggInventory{};SinnohExpeditionState sinnoh{};
  TowerDefense::CampaignState towerDefense{};Gen4UniqueItemInventory gen4UniqueItems{};
  UnovaAdventureState unova{};Gen5UniqueItemInventory gen5UniqueItems{};
  UnovaFormState unovaForms{};
};
struct RecordV46Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV46Test payload;uint32_t crc;};
// Exact shipped Kalos/V47 record. V48 compacts occupied positions from this
// 800-slot collection into the ESP32-safe active collection.
struct GameSaveV47Test {
  uint32_t playTimeSeconds=0,bootCount=0;uint8_t flags=0,activePetSlot=0;
  PokemonCollectionV47 collection{};Inventory inventory{};EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};BattleState battle{};PokedexState pokedex{};
  GymProgress gymProgress{};uint32_t money=3000;MartState mart{};
  MoveLearningQueue moveLearning{};EvolutionQueue evolutionQueue{};EggState egg{};UserSettings settings{};
  uint64_t ownedMachines=0;PlayerStatistics statistics{};uint32_t martSeenDay=0;
  PpItemInventory ppItems{};HomePetPlacementState homePetPlacement{};
  BerryGardenState berryGarden{};CreativeBackgroundState creativeBackgrounds{};
  SpecialHeldItemInventory specialHeldItems{};uint32_t lastNpcGiftDay=UINT32_MAX;
  uint32_t clockLocalEpochSeconds=0,bossCooldownSecondsRemaining=0;
  BossEncounterState bossEncounter{};EggInventory eggInventory{};SinnohExpeditionState sinnoh{};
  TowerDefense::CampaignState towerDefense{};Gen4UniqueItemInventory gen4UniqueItems{};
  UnovaAdventureState unova{};Gen5UniqueItemInventory gen5UniqueItems{};
  UnovaFormState unovaForms{};KalosAdventureState kalos{};
};
struct RecordV47Test {uint32_t magic;uint16_t version;uint16_t payloadSize;uint32_t sequence;GameSaveV47Test payload;uint32_t crc;};
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
static_assert(sizeof(GameSaveV37Test)==26064);
static_assert(sizeof(GameSaveV38Test)==26344);
static_assert(offsetof(RecordV38Test,crc)==26360);
static_assert(sizeof(GameSaveV39Test)==26352);
static_assert(offsetof(RecordV39Test,crc)==26368);
static_assert(sizeof(BossEncounterStateV40Test)==12);
static_assert(sizeof(GameSaveV40Test)==26360);
static_assert(offsetof(RecordV40Test,crc)==26376);
static_assert(sizeof(RecordV40Test)==26384);
static_assert(sizeof(GameSaveV41Test)==26624);
static_assert(offsetof(RecordV41Test,crc)==26640);
static_assert(sizeof(RecordV41Test)==26648);
static_assert(sizeof(GameSaveV42Test) < sizeof(GameSave));
static_assert(sizeof(GameSaveV45Test) < sizeof(GameSave));
static_assert(sizeof(BattleStateV46Test)==944U);
static_assert(sizeof(GameSaveV46Test) < sizeof(GameSave));
static_assert(sizeof(GameSaveV47Test) > sizeof(GameSave));
uint32_t crc32(const uint8_t* data,size_t length){uint32_t crc=0xFFFFFFFFU;for(size_t i=0;i<length;++i){crc^=data[i];for(uint8_t bit=0;bit<8;++bit)crc=(crc>>1U)^(0xEDB88320U&(0U-(crc&1U)));}return ~crc;}

PokemonCollectionV41 legacyCollectionWithStarter(uint16_t speciesId) {
  PokemonCollection current{};
  CollectionLogic::initialize(current);
  assert(CollectionLogic::chooseStarter(current, speciesId));
  PokemonCollectionV41 legacy{};
  std::memcpy(legacy.box, current.box, sizeof(legacy.box));
  std::memcpy(legacy.party, current.party, sizeof(legacy.party));
  legacy.nextUid = current.nextUid;
  return legacy;
}

PokemonCollectionV45 legacyCollectionV45WithStarter(uint16_t speciesId) {
  PokemonCollection current{};
  CollectionLogic::initialize(current);
  assert(CollectionLogic::chooseStarter(current, speciesId));
  PokemonCollectionV45 legacy{};
  std::memcpy(legacy.box, current.box, sizeof(legacy.box));
  std::memcpy(legacy.party, current.party, sizeof(legacy.party));
  legacy.nextUid = current.nextUid;
  return legacy;
}
}

int main() {
  Preferences::resetTestStorage();

  PersistentSave firstBoot;
  assert(firstBoot.begin());
  GameSave original;
  assert(firstBoot.loadOrCreate(original) == SaveLoadResult::FirstStart);
  // Zero in the formerly unused bits is the backward-compatible Android 3x
  // default, while the original ESP32 runtime deliberately ignores it.
  assert(original.settings.battleSpeedMultiplier() == 3);
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
  original.settings.setBattleSpeedMultiplier(2);
  original.settings.setAutoBattleText(true);
  original.settings.setColorTheme(UiColorTheme::Platinum);
  original.settings.setTouchInputMode(TouchInputMode::Stylus);
  original.homePetPlacement.background = 19;
  original.homePetPlacement.validMask = 0x03;
  original.homePetPlacement.gridX[0] = 4;
  original.homePetPlacement.gridY[0] = 6;
  original.homePetPlacement.gridX[1] = 12;
  original.homePetPlacement.gridY[1] = 3;
  original.berryGarden.plots[2].berry=HeldItem::SitrusBerry;
  original.berryGarden.plots[2].remainingSeconds=123;
  // Preserve nonzero retired creator bytes to prove that keeping its frozen
  // layout does not corrupt older V38 records.
  original.creativeBackgrounds.slots[3].used=1;
  original.creativeBackgrounds.slots[3].theme=2;
  original.creativeBackgrounds.slots[3].decorationCount=1;
  original.creativeBackgrounds.slots[3].decorations[0]={
      static_cast<uint8_t>(CreativeDecorationKind::Desk),7,5};
  original.creativeBackgrounds.activeSlot=3;
  original.specialHeldItems.quantities[0]=1;
  original.specialHeldItems.claimedMask=1;
  original.lastNpcGiftDay=4;
  original.clockLocalEpochSeconds=1770000000U;
  original.bossCooldownSecondsRemaining=32123U;
  original.bossEncounter.personality=0xB0551234U;
  original.bossEncounter.speciesId=384U;
  original.bossEncounter.currentHp=167890U;
  original.bossEncounter.form=0U;
  original.bossEncounter.megaVariant=MegaVariant::Mega;
  original.eggInventory.count=2U;
  original.eggInventory.slots[0]={0xE6610001U,1U,3U,EggRarity::Special};
  original.eggInventory.slots[1]={0xE6610002U,151U,3U,EggRarity::Special};
  original.unova.unlockedAreas=0x003FU;
  original.unova.defeatedBosses=0x001FU;
  original.unova.pwtCompletedTiers=0x03U;
  original.unova.unovaMachineOwnership=0x00012345U;
  original.unova.run.seed=0x554E4F56U;
  original.unova.run.area=5U;
  original.unova.run.depth=3U;
  original.unova.run.flags=1U;
  original.gen5UniqueItems.keyItems=kDnaSplicersBit|kRevealGlassBit;
  original.gen5UniqueItems.driveQuantities[2]=1U;
  original.unovaForms.fusedKyuremUid=0x646U;
  original.unovaForms.fusedDonor=CollectionLogic::createPokemon(
      0x643U,643U,75U,false,0x643U,5U);
  original.unovaForms.kyuremOriginalMoves[0]=static_cast<MoveId>(549U);
  assert(original.settings.screenTimeout() == 3);
  assert(original.settings.battleSpeedMultiplier() == 2);
  assert(original.settings.autoBattleText());
  assert(original.settings.colorTheme() == UiColorTheme::Platinum);
  assert(original.settings.touchInputMode() == TouchInputMode::Stylus);
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
  assert(loaded.settings.battleSpeedMultiplier() == 2);
  assert(loaded.settings.autoBattleText());
  assert(loaded.settings.colorTheme() == UiColorTheme::Platinum);
  assert(loaded.settings.touchInputMode() == TouchInputMode::Stylus);
  assert(loaded.homePetPlacement.background == 19);
  assert(loaded.homePetPlacement.validMask == 0x03);
  assert(loaded.homePetPlacement.gridX[0] == 4 && loaded.homePetPlacement.gridY[0] == 6);
  assert(loaded.homePetPlacement.gridX[1] == 12 && loaded.homePetPlacement.gridY[1] == 3);
  assert(loaded.berryGarden.plots[2].berry==HeldItem::SitrusBerry&&
         loaded.berryGarden.plots[2].remainingSeconds==123);
  assert(loaded.creativeBackgrounds.activeSlot==3&&
         loaded.creativeBackgrounds.slots[3].decorationCount==1);
  assert(loaded.specialHeldItems.quantities[0]==1&&
         loaded.specialHeldItems.claimedMask==1);
  assert(loaded.lastNpcGiftDay==4);
  assert(loaded.clockLocalEpochSeconds==1770000000U);
  assert(loaded.bossCooldownSecondsRemaining==32123U);
  assert(loaded.bossEncounter.personality==0xB0551234U&&
         loaded.bossEncounter.speciesId==384U&&
         loaded.bossEncounter.currentHp==167890U&&
         loaded.bossEncounter.megaVariant==MegaVariant::Mega);
  assert(loaded.eggInventory.count==2U&&
         loaded.eggInventory.slots[0].speciesId==1U&&
         loaded.eggInventory.slots[1].speciesId==151U);
  assert(loaded.unova.unlockedAreas==0x003FU&&
         loaded.unova.defeatedBosses==0x001FU&&
         loaded.unova.pwtCompletedTiers==0x03U&&
         loaded.unova.unovaMachineOwnership==0x00012345U&&
         loaded.unova.run.seed==0x554E4F56U&&loaded.unova.run.area==5U&&
         loaded.unova.run.depth==3U&&loaded.unova.run.flags==1U);
  assert(loaded.gen5UniqueItems.keyItems==(kDnaSplicersBit|kRevealGlassBit)&&
         loaded.gen5UniqueItems.driveQuantities[2]==1U);
  assert(loaded.unovaForms.fusedKyuremUid==0x646U&&
         loaded.unovaForms.fusedDonor.speciesId==643U&&
         loaded.unovaForms.kyuremOriginalMoves[0]==static_cast<MoveId>(549U));
  loaded.settings.setScreenTimeout(2);
  assert(loaded.settings.battleSpeedMultiplier() == 2);
  assert(loaded.settings.colorTheme() == UiColorTheme::Platinum);
  loaded.settings.setAutoBattleText(false);
  assert(loaded.settings.colorTheme() == UiColorTheme::Platinum);
  assert(loaded.settings.screenTimeout() == 2);
  assert(loaded.settings.touchInputMode() == TouchInputMode::Stylus);
  loaded.settings.setBattleSpeedMultiplier(1);
  assert(loaded.settings.battleSpeedMultiplier() == 1);
  assert(loaded.settings.screenTimeout() == 2);
  assert(loaded.settings.colorTheme() == UiColorTheme::Platinum);
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
  old.payload.collection=legacyCollectionWithStarter(4);
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
  old32.payload.collection=legacyCollectionWithStarter(1);
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
  old33.payload.collection=legacyCollectionWithStarter(7);
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
  old34.payload.collection=legacyCollectionWithStarter(4);
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
  old35.payload.collection=legacyCollectionWithStarter(1);
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
  old36.payload.collection=legacyCollectionWithStarter(7);
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

  // V38 appends the garden, creative backgrounds and species-item inventory.
  // A V37 record keeps every prior byte and initializes only those new fields.
  Preferences::resetTestStorage();
  RecordV37Test old37{};old37.magic=0x504F4B45;old37.version=37;
  old37.payloadSize=sizeof(GameSaveV37Test);old37.sequence=71237;
  old37.payload.collection=legacyCollectionWithStarter(4);
  old37.payload.playTimeSeconds=56789;old37.payload.money=98765;
  old37.payload.mart.offers[19]={MartItem::UltraBall,HeldItem::None,1200,6};
  old37.payload.homePetPlacement.background=7;
  old37.payload.homePetPlacement.validMask=1;
  old37.payload.homePetPlacement.gridX[0]=11;
  old37.payload.homePetPlacement.gridY[0]=6;
  old37.crc=crc32(reinterpret_cast<const uint8_t*>(&old37),offsetof(RecordV37Test,crc));
  assert(raw.putBytes("save_b",&old37,sizeof(old37))==sizeof(old37));
  assert(raw.putBool("started",true)==1);
  PersistentSave v37Migrator;assert(v37Migrator.begin());GameSave v37Migrated;
  assert(v37Migrator.loadOrCreate(v37Migrated)==SaveLoadResult::Loaded);
  assert(v37Migrated.playTimeSeconds==56789&&v37Migrated.money==98765);
  assert(CollectionLogic::active(v37Migrated.collection,0)->speciesId==4);
  assert(v37Migrated.mart.offers[19].remaining==6);
  assert(v37Migrated.homePetPlacement.background==7&&
         v37Migrated.homePetPlacement.gridX[0]==11&&
         v37Migrated.homePetPlacement.gridY[0]==6);
  assert(v37Migrated.creativeBackgrounds.activeSlot==0xFF);
  assert(v37Migrated.berryGarden.rngState==0x42455252UL);
  assert(v37Migrated.specialHeldItems.claimedMask==0&&
         v37Migrated.lastNpcGiftDay==UINT32_MAX);

  // V39 appends only the Boss cooldown. A complete V38 record keeps its
  // synchronized clock and every existing subsystem, then starts Boss-ready.
  Preferences::resetTestStorage();
  GameSaveV38Test v38Source{};
  v38Source.collection=legacyCollectionWithStarter(7);
  v38Source.playTimeSeconds=67890U;
  v38Source.money=24680U;
  v38Source.clockLocalEpochSeconds=1771000000U;
  v38Source.berryGarden.rngState=0xBEEFBEEFU;
  v38Source.lastNpcGiftDay=77U;
  RecordV38Test old38{};old38.magic=0x504F4B45;old38.version=38;
  old38.payloadSize=sizeof(GameSaveV38Test);old38.sequence=81338U;
  old38.payload=v38Source;
  old38.crc=crc32(reinterpret_cast<const uint8_t*>(&old38),
                  offsetof(RecordV38Test,crc));
  assert(raw.putBytes("save_a",&old38,sizeof(old38))==sizeof(old38));
  assert(raw.putBool("started",true)==1);
  PersistentSave v38Migrator;assert(v38Migrator.begin());GameSave v38Migrated;
  assert(v38Migrator.loadOrCreate(v38Migrated)==SaveLoadResult::Loaded);
  assert(v38Migrated.playTimeSeconds==67890U&&v38Migrated.money==24680U);
  assert(CollectionLogic::active(v38Migrated.collection,0)->speciesId==7U);
  assert(v38Migrated.clockLocalEpochSeconds==1771000000U);
  assert(v38Migrated.berryGarden.rngState==0xBEEFBEEFU&&
         v38Migrated.lastNpcGiftDay==77U);
  assert(v38Migrated.bossCooldownSecondsRemaining==0U);
  assert(v38Migrated.bossEncounter.speciesId==0U&&
         v38Migrated.bossEncounter.currentHp==0U);

  // V40 preserves every V39 field and recovers an in-progress/just-ended Boss
  // opponent as the persistent encounter, including its exact remaining HP.
  Preferences::resetTestStorage();
  GameSaveV39Test v39Source{};
  v39Source.collection=legacyCollectionWithStarter(4);
  v39Source.playTimeSeconds=78901U;
  v39Source.money=13579U;
  v39Source.clockLocalEpochSeconds=1772000000U;
  v39Source.bossCooldownSecondsRemaining=0U;
  BattleState& v39Battle=legacyBattle(v39Source.battle);
  v39Battle.kind=BattleKind::Boss;
  v39Battle.outcome=BattleOutcome::Defeat;
  v39Battle.opponentCount=1U;
  v39Battle.opponents[0]=CollectionLogic::createPokemon(
      0xB0550040U,150U,100U,false,0xB0550040U);
  v39Battle.opponents[0].maximumHp=10000U;
  v39Battle.opponents[0].currentHp=3456U;
  v39Battle.opponents[0].heldItem=HeldItem::MegaStone;
  assert(MegaEvolution::setVariantChoice(v39Battle.opponents[0],
                                         MegaVariant::MegaY));
  const uint32_t migratedBossPersonality=
      v39Battle.opponents[0].personality;
  RecordV39Test old39{};old39.magic=0x504F4B45;old39.version=39;
  old39.payloadSize=sizeof(GameSaveV39Test);old39.sequence=91439U;
  old39.payload=v39Source;
  old39.crc=crc32(reinterpret_cast<const uint8_t*>(&old39),
                  offsetof(RecordV39Test,crc));
  assert(raw.putBytes("save_b",&old39,sizeof(old39))==sizeof(old39));
  assert(raw.putBool("started",true)==1);
  PersistentSave v39Migrator;assert(v39Migrator.begin());GameSave v39Migrated;
  assert(v39Migrator.loadOrCreate(v39Migrated)==SaveLoadResult::Loaded);
  assert(v39Migrated.playTimeSeconds==78901U&&v39Migrated.money==13579U&&
         v39Migrated.clockLocalEpochSeconds==1772000000U);
  assert(v39Migrated.bossEncounter.speciesId==150U&&
         v39Migrated.bossEncounter.currentHp==69120U&&
         v39Migrated.bossEncounter.personality==migratedBossPersonality&&
         v39Migrated.bossEncounter.megaVariant==MegaVariant::MegaY);
  assert(v39Migrated.battle.payDayMoney==69120U&&
         v39Migrated.battle.opponents[0].maximumHp==BossSystem::kBattleHpLayer&&
         v39Migrated.battle.opponents[0].currentHp==BossSystem::kBattleHpLayer&&
         v39Migrated.eggInventory.count==0U);

  // V41 appends the Egg Bag and widens persistent Boss HP. A V40 record keeps
  // every previous byte, scales the old 10,000-point percentage by twenty,
  // and initializes the new pocket empty.
  Preferences::resetTestStorage();
  GameSaveV40Test v40Prefix{};
  v40Prefix.collection=legacyCollectionWithStarter(7);
  v40Prefix.playTimeSeconds=89012U;v40Prefix.money=24681U;
  BattleState& v40Battle=legacyBattle(v40Prefix.battle);
  v40Battle.kind=BattleKind::Boss;v40Battle.active=true;
  v40Battle.outcome=BattleOutcome::Ongoing;
  v40Battle.opponentCount=1U;
  v40Battle.opponents[0]=CollectionLogic::createPokemon(
      0xB0550041U,384U,100U,false,0xB0550041U);
  v40Battle.opponents[0].maximumHp=10000U;
  v40Battle.opponents[0].currentHp=7500U;
  BossEncounterStateV40Test oldBoss{};
  oldBoss.personality=v40Battle.opponents[0].personality;
  oldBoss.speciesId=384U;oldBoss.currentHp=7500U;
  RecordV40Test old40{};old40.magic=0x504F4B45;old40.version=40;
  old40.payloadSize=sizeof(GameSaveV40Test);old40.sequence=101440U;
  old40.payload=v40Prefix;
  old40.payload.bossEncounter=oldBoss;
  old40.crc=crc32(reinterpret_cast<const uint8_t*>(&old40),
                  offsetof(RecordV40Test,crc));
  assert(raw.putBytes("save_a",&old40,sizeof(old40))==sizeof(old40));
  assert(raw.putBool("started",true)==1);
  PersistentSave v40Migrator;assert(v40Migrator.begin());GameSave v40Migrated;
  assert(v40Migrator.loadOrCreate(v40Migrated)==SaveLoadResult::Loaded);
  assert(v40Migrated.playTimeSeconds==89012U&&v40Migrated.money==24681U);
  assert(v40Migrated.bossEncounter.speciesId==384U&&
         v40Migrated.bossEncounter.currentHp==150000U&&
         v40Migrated.bossEncounter.personality==oldBoss.personality);
  assert(v40Migrated.battle.payDayMoney==150000U&&
         v40Migrated.battle.opponents[0].maximumHp==BossSystem::kBattleHpLayer&&
         v40Migrated.battle.opponents[0].currentHp==BossSystem::kBattleHpLayer&&
         v40Migrated.eggInventory.count==0U);

  // V42 preserves the final Gen-I--III save byte-for-byte while zeroing only
  // the new Sinnoh range. The old 386-slot collection and 49-byte Pokédex
  // must never be read past their source allocation during this migration.
  Preferences::resetTestStorage();
  GameSaveV41Test v41Prefix{};
  v41Prefix.collection=legacyCollectionWithStarter(7U);
  v41Prefix.playTimeSeconds=99001U;v41Prefix.money=86420U;
  // National #386 is byte 48, bit 1: bit indexing is speciesId - 1.
  v41Prefix.pokedex.caught[48]=0x02U;
  v41Prefix.eggInventory.count=1U;
  v41Prefix.eggInventory.slots[0]={0xE6610041U,151U,2U,EggRarity::Special};
  RecordV41Test old41{};old41.magic=0x504F4B45;old41.version=41;
  old41.payloadSize=sizeof(GameSaveV41Test);old41.sequence=101441U;
  old41.payload=v41Prefix;
  old41.crc=crc32(reinterpret_cast<const uint8_t*>(&old41),
                  offsetof(RecordV41Test,crc));
  assert(raw.putBytes("save_a",&old41,sizeof(old41))==sizeof(old41));
  assert(raw.putBool("started",true)==1);
  PersistentSave v41Migrator;assert(v41Migrator.begin());GameSave v41Migrated;
  assert(v41Migrator.loadOrCreate(v41Migrated)==SaveLoadResult::Loaded);
  assert(v41Migrated.playTimeSeconds==99001U&&v41Migrated.money==86420U);
  assert(CollectionLogic::active(v41Migrated.collection,0)->speciesId==7U);
  assert(PokedexLogic::hasCaught(v41Migrated.pokedex,386U));
  assert(!PokedexLogic::hasCaught(v41Migrated.pokedex,387U));
  assert(v41Migrated.eggInventory.count==1U&&
         v41Migrated.eggInventory.slots[0].speciesId==151U);
  assert(v41Migrated.sinnoh.research==0U&&v41Migrated.sinnoh.unlockedSites==0U&&
         v41Migrated.sinnoh.rngState==0x53494E4EU);

  // V43 leaves every completed Sinnoh field intact and appends only Poke
  // Defense's isolated campaign record with a playable first arena.
  Preferences::resetTestStorage();
  GameSaveV42Test v42Prefix{};
  v42Prefix.collection=legacyCollectionWithStarter(4U);
  v42Prefix.playTimeSeconds=100002U;v42Prefix.money=86421U;
  v42Prefix.sinnoh.research=123U;v42Prefix.sinnoh.unlockedSites=0x07U;
  v42Prefix.sinnoh.selectedSite=2U;v42Prefix.sinnoh.rngState=0x12345678U;
  RecordV42Test old42{};old42.magic=0x504F4B45;old42.version=42;
  old42.payloadSize=sizeof(GameSaveV42Test);old42.sequence=101442U;
  old42.payload=v42Prefix;
  old42.crc=crc32(reinterpret_cast<const uint8_t*>(&old42),
                  offsetof(RecordV42Test,crc));
  assert(raw.putBytes("save_a",&old42,sizeof(old42))==sizeof(old42));
  assert(raw.putBool("started",true)==1);
  PersistentSave v42Migrator;assert(v42Migrator.begin());GameSave v42Migrated;
  assert(v42Migrator.loadOrCreate(v42Migrated)==SaveLoadResult::Loaded);
  assert(v42Migrated.playTimeSeconds==100002U&&v42Migrated.money==86421U);
  assert(CollectionLogic::active(v42Migrated.collection,0)->speciesId==4U);
  assert(v42Migrated.sinnoh.research==123U&&v42Migrated.sinnoh.unlockedSites==0x07U&&
         v42Migrated.sinnoh.selectedSite==2U&&v42Migrated.sinnoh.rngState==0x12345678U);
  assert(v42Migrated.towerDefense.unlockedStages==1U&&
         v42Migrated.towerDefense.enabledDefendersMask==0x07U&&
         v42Migrated.towerDefense.totalScore==0U);

  // V46 migrates the last Sinnoh-only V45 record without touching any of its
  // permanent data. The new 720-slot Box, #649 Dex range and all Unova
  // sidecars start clean and immediately accept Generation-V records.
  Preferences::resetTestStorage();
  GameSaveV45Test v45Prefix{};
  v45Prefix.collection=legacyCollectionV45WithStarter(7U);
  PokemonCollection expandedFromV45{};
  expandedFromV45=v45Prefix.collection;
  uint32_t arceusUid=0U;
  assert(CollectionLogic::add(expandedFromV45,
      CollectionLogic::createPokemon(0U,493U,80U,false,0x493U,4U),
      &arceusUid));
  std::memcpy(v45Prefix.collection.box,expandedFromV45.box,
              sizeof(v45Prefix.collection.box));
  v45Prefix.collection.nextUid=expandedFromV45.nextUid;
  v45Prefix.playTimeSeconds=145045U;v45Prefix.money=64949U;
  v45Prefix.settings.setColorTheme(UiColorTheme::Platinum);
  v45Prefix.sinnoh.unlockedSites=SinnohExpedition::kAllSitesMask;
  v45Prefix.sinnoh.research=987U;
  v45Prefix.gen4UniqueItems.quantities[0]=1U;
  v45Prefix.gen4UniqueItems.claimedMask=1U;
  v45Prefix.pokedex.caught[(493U-1U)/8U]|=
      static_cast<uint8_t>(1U<<((493U-1U)%8U));
  RecordV45Test old45{};old45.magic=0x504F4B45;old45.version=45;
  old45.payloadSize=sizeof(GameSaveV45Test);old45.sequence=101445U;
  old45.payload=v45Prefix;
  old45.crc=crc32(reinterpret_cast<const uint8_t*>(&old45),
                  offsetof(RecordV45Test,crc));
  assert(raw.putBytes("save_a",&old45,sizeof(old45))==sizeof(old45));
  assert(raw.putBool("started",true)==1);
  PersistentSave v45Migrator;assert(v45Migrator.begin());GameSave v45Migrated;
  assert(v45Migrator.loadOrCreate(v45Migrated)==SaveLoadResult::Loaded);
  assert(v45Migrated.playTimeSeconds==145045U&&v45Migrated.money==64949U);
  assert(CollectionLogic::active(v45Migrated.collection,0U)->speciesId==7U);
  assert(CollectionLogic::find(v45Migrated.collection,arceusUid)->speciesId==493U);
  assert(PokedexLogic::hasCaught(v45Migrated.pokedex,493U)&&
         !PokedexLogic::hasCaught(v45Migrated.pokedex,494U));
  assert(v45Migrated.sinnoh.unlockedSites==SinnohExpedition::kAllSitesMask&&
         v45Migrated.sinnoh.research==987U&&
         v45Migrated.gen4UniqueItems.quantities[0]==1U&&
         v45Migrated.gen4UniqueItems.claimedMask==1U);
  assert(v45Migrated.unova.unlockedAreas==0U&&
         v45Migrated.unova.defeatedBosses==0U&&
         v45Migrated.unova.rngState==0x554E4F56U&&
         v45Migrated.unova.royalUnovaLastDay==UINT32_MAX&&
         v45Migrated.gen5UniqueItems.keyItems==0U&&
         v45Migrated.unovaForms.fusedKyuremUid==0U);
  uint32_t genesectUid=0U;
  assert(CollectionLogic::add(v45Migrated.collection,
      CollectionLogic::createPokemon(0U,649U,70U,false,0x649U,5U),
      &genesectUid));
  assert(CollectionLogic::find(v45Migrated.collection,genesectUid)->speciesId==649U);

  // V47 upgrades a real Unova save in place. The Box tail, active battle
  // prefix, National Dex bits and campaign/form sidecars must all survive;
  // only the new Kalos capacity starts empty.
  Preferences::resetTestStorage();
  GameSaveV46Test v46Prefix{};
  PokemonCollection v46Expanded{};
  CollectionLogic::initialize(v46Expanded);
  assert(CollectionLogic::chooseStarter(v46Expanded,1U));
  uint32_t zekromUid=0U,genesectV46Uid=0U;
  assert(CollectionLogic::add(v46Expanded,
      CollectionLogic::createPokemon(0U,644U,82U,false,0x644U,5U),
      &zekromUid));
  assert(CollectionLogic::add(v46Expanded,
      CollectionLogic::createPokemon(0U,649U,75U,false,0x649U,5U),
      &genesectV46Uid));
  std::memcpy(v46Prefix.collection.box,v46Expanded.box,
              sizeof(v46Prefix.collection.box));
  std::memcpy(v46Prefix.collection.party,v46Expanded.party,
              sizeof(v46Prefix.collection.party));
  v46Prefix.collection.nextUid=v46Expanded.nextUid;
  v46Prefix.playTimeSeconds=246046U;v46Prefix.money=721649U;
  v46Prefix.pokedex.caught[(649U-1U)/8U]|=
      static_cast<uint8_t>(1U<<((649U-1U)%8U));
  v46Prefix.unova.unlockedAreas=UnovaAdventure::kAllAreasMask;
  v46Prefix.unova.defeatedBosses=UnovaAdventure::kAllAreasMask;
  v46Prefix.unova.pwtCompletedTiers=0x1FU;
  v46Prefix.gen5UniqueItems.keyItems=0x15U;
  v46Prefix.unovaForms.fusedKyuremUid=zekromUid;
  BattleState v46Battle{};
  v46Battle.active=true;v46Battle.playerUid=zekromUid;
  v46Battle.unlockedGeneration=5U;v46Battle.turn=46U;
  std::memcpy(v46Prefix.battle.bytes,&v46Battle,sizeof(v46Prefix.battle.bytes));
  RecordV46Test old46{};old46.magic=0x504F4B45;old46.version=46U;
  old46.payloadSize=sizeof(GameSaveV46Test);old46.sequence=101446U;
  old46.payload=v46Prefix;
  old46.crc=crc32(reinterpret_cast<const uint8_t*>(&old46),
                  offsetof(RecordV46Test,crc));
  assert(raw.putBytes("save_a",&old46,sizeof(old46))==sizeof(old46));
  assert(raw.putBool("started",true)==1);
  PersistentSave v46Migrator;assert(v46Migrator.begin());GameSave v46Migrated;
  assert(v46Migrator.loadOrCreate(v46Migrated)==SaveLoadResult::Loaded);
  assert(v46Migrated.playTimeSeconds==246046U&&v46Migrated.money==721649U);
  assert(CollectionLogic::find(v46Migrated.collection,zekromUid)->speciesId==644U);
  assert(CollectionLogic::find(v46Migrated.collection,genesectV46Uid)->speciesId==649U);
  assert(PokedexLogic::hasCaught(v46Migrated.pokedex,649U)&&
         !PokedexLogic::hasCaught(v46Migrated.pokedex,650U));
  assert(v46Migrated.unova.unlockedAreas==UnovaAdventure::kAllAreasMask&&
         v46Migrated.unova.defeatedBosses==UnovaAdventure::kAllAreasMask&&
         v46Migrated.unova.pwtCompletedTiers==0x1FU);
  assert(v46Migrated.gen5UniqueItems.keyItems==0x15U&&
         v46Migrated.unovaForms.fusedKyuremUid==zekromUid);
  assert(v46Migrated.battle.active&&v46Migrated.battle.playerUid==zekromUid&&
         v46Migrated.battle.turn==46U&&
         v46Migrated.battle.playerGen6Effects.extraTypeActive==false&&
         v46Migrated.battle.gen6Terrain==Gen6Terrain::None);
  assert(v46Migrated.kalos.unlockedAreas==0U&&
         v46Migrated.kalos.defeatedBosses==0U&&
         v46Migrated.kalos.run.flags==0U);

  // V48 must preserve a Pokemon deliberately stored in V47's final slot.
  // It compacts occupied slots and never interprets the capacity reduction as
  // permission to truncate the old Box.
  Preferences::resetTestStorage();
  GameSaveV47Test v47Prefix{};
  v47Prefix.playTimeSeconds=347047U;v47Prefix.money=650721U;
  OwnedPokemon starterV47=CollectionLogic::createPokemon(41U,650U,72U,false,0x650U,6U);
  OwnedPokemon tailV47=CollectionLogic::createPokemon(79941U,721U,88U,true,0x721U,6U);
  v47Prefix.collection.box[0]=starterV47;
  v47Prefix.collection.box[kBoxCapacityV47-1U]=tailV47;
  v47Prefix.collection.party[0]=starterV47.uid;
  v47Prefix.collection.party[1]=tailV47.uid;
  v47Prefix.collection.nextUid=79942U;
  v47Prefix.kalos.unlockedAreas=0x07U;
  v47Prefix.kalos.defeatedBosses=0x03U;
  v47Prefix.pokedex.caught[(721U-1U)/8U]|=
      static_cast<uint8_t>(1U<<((721U-1U)%8U));
  RecordV47Test old47{};old47.magic=0x504F4B45;old47.version=47U;
  old47.payloadSize=sizeof(GameSaveV47Test);old47.sequence=101447U;
  old47.payload=v47Prefix;
  old47.crc=crc32(reinterpret_cast<const uint8_t*>(&old47),
                  offsetof(RecordV47Test,crc));
  assert(raw.putBytes("save_a",&old47,sizeof(old47))==sizeof(old47));
  assert(raw.putBool("started",true)==1);
  PersistentSave v47Migrator;assert(v47Migrator.begin());GameSave v47Migrated;
  assert(v47Migrator.loadOrCreate(v47Migrated)==SaveLoadResult::Loaded);
  assert(v47Migrated.playTimeSeconds==347047U&&v47Migrated.money==650721U);
  assert(CollectionLogic::count(v47Migrated.collection)==2U);
  assert(CollectionLogic::find(v47Migrated.collection,starterV47.uid)->speciesId==650U);
  const OwnedPokemon* migratedTail=CollectionLogic::find(v47Migrated.collection,tailV47.uid);
  assert(migratedTail&&migratedTail->speciesId==721U&&migratedTail->shiny);
  assert(v47Migrated.collection.party[1]==tailV47.uid&&
         v47Migrated.collection.nextUid==79942U);
  assert(PokedexLogic::hasCaught(v47Migrated.pokedex,721U)&&
         v47Migrated.kalos.unlockedAreas==0x07U&&
         v47Migrated.kalos.defeatedBosses==0x03U);
  PersistentSave v48Reload;assert(v48Reload.begin());GameSave v48Reloaded;
  assert(v48Reload.loadOrCreate(v48Reloaded)==SaveLoadResult::Loaded);
  assert(CollectionLogic::find(v48Reloaded.collection,tailV47.uid)->speciesId==721U);

  PokemonCollectionV47 overfullV47{};
  for(uint16_t i=0;i<=kBoxCapacity;++i)overfullV47.box[i].uid=static_cast<uint32_t>(i)+1U;
  PokemonCollection rejectedV47{};
  assert(!rejectedV47.importV47(overfullV47));

  // User-requested reset writes a new, complete blank record instead of
  // deleting the two redundant slots first. It must survive a fresh loader.
  assert(v40Migrator.reset(v40Migrated));
  assert(v40Migrated.money==3000U&&v40Migrated.playTimeSeconds==0U);
  assert(CollectionLogic::count(v40Migrated.collection)==0U);
  PersistentSave resetLoader;assert(resetLoader.begin());GameSave resetReloaded;
  assert(resetLoader.loadOrCreate(resetReloaded)==SaveLoadResult::Loaded);
  assert(resetReloaded.money==3000U&&resetReloaded.playTimeSeconds==0U);
  assert(CollectionLogic::count(resetReloaded.collection)==0U);
  return 0;
}
