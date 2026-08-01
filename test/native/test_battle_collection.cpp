#include "game/BattleEngine.h"
#include "game/Pokedex.h"
#include "game/GymSystem.h"
#include "game/Economy.h"
#include "game/TrainerData.h"
#include <cassert>

int main() {
  assert(trainerProfileCount() >= 100);
  for (uint8_t index = 0; index < trainerProfileCount(); ++index) {
    const TrainerProfile* profile = trainerProfile(index);
    assert(profile && profile->trainerClass[0] && profile->name[0] && profile->frontAsset[0]);
    assert(profile->speciesCount >= 1 && profile->speciesCount <= 3);
    for (uint8_t member = 0; member < profile->speciesCount; ++member) assert(findSpecies(profile->species[member]));
  }
  for (uint16_t speciesId = 1; speciesId <= 151; ++speciesId) assert(findSpecies(speciesId));
  for (uint16_t speciesId = 1; speciesId <= 151; ++speciesId) {
    const PokedexEntryData* entry = pokedexEntry(speciesId);
    assert(entry && entry->category[0] && entry->description[0] && entry->heightDecimeters && entry->weightHectograms);
  }
  const EvolutionData* pikachuEvolution = evolutionFor(25);
  assert(pikachuEvolution && pikachuEvolution->toSpeciesId == 26 && pikachuEvolution->level == 36);
  assert(findSpecies(150)->catchRate == 3);
  PokemonCollection collection;
  CollectionLogic::initialize(collection);
  assert(CollectionLogic::count(collection) == 0);
  assert(CollectionLogic::chooseStarter(collection, 4));
  assert(!CollectionLogic::chooseStarter(collection, 7));
  assert(CollectionLogic::validate(collection));
  assert(CollectionLogic::active(collection, 0)->speciesId == 4);
  OwnedPokemon* starter = CollectionLogic::active(collection, 0);
  starter->status = StatusCondition::Poison;
  starter->movePp[0] = 0;
  CollectionLogic::care(*starter, CareAction::Bathe);
  assert(starter->status == StatusCondition::None);
  assert(starter->movePp[0] == findFullMove(starter->moves[0])->pp);

  EncounterCharges charges;
  BattleState battle;
  const uint32_t starterUid = collection.party[0];
  assert(BattleEngine::startWild(battle, collection, starterUid, 12345));
  assert(charges.available == 3);  // Random wild encounters never use charges.
  assert(battle.active && battle.kind == BattleKind::Wild && BattleEngine::currentOpponent(battle));
  assert(battle.rewardMoney == 0);

  Inventory inventory;
  inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] = 1;
  const uint8_t before = CollectionLogic::count(collection);
  const BattleActionResult capture = BattleEngine::throwBall(
      battle, collection, inventory, PokeBallType::MasterBall);
  assert(capture.caught && capture.outcome == BattleOutcome::Captured);
  assert(CollectionLogic::count(collection) == before + 1);

  const OwnedPokemon extra = CollectionLogic::createPokemon(0, 16, 4);
  uint32_t extraUid = 0;
  assert(CollectionLogic::add(collection, extra, &extraUid));
  assert(CollectionLogic::setPartySlot(collection, 1, extraUid));
  assert(CollectionLogic::validate(collection));
  assert(CollectionLogic::removeFromParty(collection,extraUid));
  assert(!CollectionLogic::isInParty(collection,extraUid));
  assert(!CollectionLogic::removeFromParty(collection,starterUid));
  assert(CollectionLogic::setPartySlot(collection,1,extraUid));

  // A selected team member can enter the active battle and the opposing turn
  // still resolves after switching, matching the touch UI's SWITCH action.
  assert(BattleEngine::startTrainer(battle, charges, collection, starterUid, 67890));
  assert(battle.rewardMoney > 0);
  assert(battle.opponentItemUses == 1);
  assert(charges.available == 2);
  const uint32_t firstBattlerUid = battle.playerUid;
  const BattleActionResult switched = BattleEngine::switchPokemon(battle, collection);
  assert(switched.accepted);
  assert(battle.playerUid == extraUid);
  assert(battle.playerUid != firstBattlerUid);

  // A complete fight awards XP and leaves a terminal battle result that can
  // be persisted before the UI returns home.
  battle.opponentCount = 1;
  battle.opponentItemUses = 0;
  BattleEngine::currentOpponent(battle)->currentHp = 1;
  const uint32_t xpBefore = CollectionLogic::find(collection, extraUid)->experience;
  const uint32_t starterXpBefore = CollectionLogic::find(collection, starterUid)->experience;
  const BattleActionResult victory = BattleEngine::fight(battle, collection, 0);
  assert(victory.accepted && victory.outcome == BattleOutcome::Victory);
  assert(victory.experienceGained > 0);
  assert(CollectionLogic::find(collection, extraUid)->experience > xpBefore);
  assert(CollectionLogic::find(collection, extraUid)->experience - xpBefore ==
         CollectionLogic::find(collection, starterUid)->experience - starterXpBefore);

  // Mart inventory has two guaranteed staples plus five daily rotating offers.
  MartState mart;
  Economy::rotate(mart);
  assert(mart.offers[0].item == MartItem::PokeBall);
  assert(mart.offers[1].item == MartItem::Potion);
  for (uint8_t i = 2; i < kMartOfferCount; ++i) {
    assert(mart.offers[i].item != MartItem::PokeBall && mart.offers[i].item != MartItem::Potion);
  }
  uint32_t money = 1000;
  const uint16_t potionsBefore = inventory.medicine[static_cast<uint8_t>(BattleItem::Potion)];
  assert(Economy::buy(mart, 1, money, inventory));
  assert(money == 700 && inventory.medicine[static_cast<uint8_t>(BattleItem::Potion)] == potionsBefore + 1);

  // Medicine and X-items are usable in any active battle, consume a turn and
  // mutate only temporary combat stages where appropriate.
  BattleEngine::clear(battle);
  battle.active = true; battle.outcome = BattleOutcome::Ongoing; battle.kind = BattleKind::Trainer;
  battle.playerUid = starterUid; battle.opponentCount = 1;
  battle.opponents[0] = CollectionLogic::createPokemon(0, 10, 2);
  battle.opponents[0].moves[0] = MoveId::None;
  OwnedPokemon* itemTarget = CollectionLogic::find(collection, starterUid);
  itemTarget->currentHp = static_cast<uint16_t>(itemTarget->maximumHp - 1);
  inventory.medicine[static_cast<uint8_t>(BattleItem::Potion)] = 1;
  assert(BattleEngine::useItem(battle, collection, inventory, BattleItem::Potion).accepted);
  assert(itemTarget->currentHp == itemTarget->maximumHp);
  assert(inventory.medicine[static_cast<uint8_t>(BattleItem::Potion)] == 0);
  inventory.medicine[static_cast<uint8_t>(BattleItem::XAttack)] = 1;
  assert(BattleEngine::useItem(battle, collection, inventory, BattleItem::XAttack).accepted);
  assert(battle.playerVolatile.attackStage == 1);
  BattleEngine::clear(battle);

  PokedexState pokedex;
  PokedexLogic::markSeen(pokedex, 16);
  assert(PokedexLogic::hasSeen(pokedex, 16));
  assert(!PokedexLogic::hasCaught(pokedex, 16));
  PokedexLogic::markCaught(pokedex, 16);
  assert(PokedexLogic::hasCaught(pokedex, 16));
  assert(PokedexLogic::seenCount(pokedex) == 1);
  assert(PokedexLogic::caughtCount(pokedex) == 1);

  charges.available = 0;
  EncounterLogic::advance(charges, EncounterCharges::kRechargeSeconds - 1);
  assert(charges.available == 0);
  EncounterLogic::advance(charges, 1);
  assert(charges.available == 1);
  EncounterLogic::advance(charges, EncounterCharges::kRechargeSeconds * 10U);
  assert(charges.available == EncounterCharges::kMaximum);

  // No charge is consumed if a battle is already active or none is available.
  assert(BattleEngine::startTrainer(battle, charges, collection, starterUid, 111));
  const uint8_t afterStart = charges.available;
  assert(!BattleEngine::startTrainer(battle, charges, collection, starterUid, 222));
  assert(charges.available == afterStart);
  battle = BattleState{};
  charges.available = 0;
  assert(!BattleEngine::startTrainer(battle, charges, collection, starterUid, 333));
  assert(charges.available == 0);

  // The persistent Box has one slot per Generation-I species. A full Box
  // rejects both direct additions and capture attempts without consuming a ball.
  while (CollectionLogic::count(collection) < kBoxCapacity) {
    assert(CollectionLogic::add(collection, CollectionLogic::createPokemon(0, 19, 2)));
  }
  assert(!CollectionLogic::add(collection, CollectionLogic::createPokemon(0, 16, 3)));
  battle = BattleState{};
  battle.active = true;
  battle.outcome = BattleOutcome::Ongoing;
  battle.playerUid = collection.party[0];
  battle.kind = BattleKind::Wild;
  battle.opponentCount = 1;
  battle.opponents[0] = CollectionLogic::createPokemon(0, 16, 3);
  inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] = 1;
  const BattleActionResult fullBoxCapture = BattleEngine::throwBall(
      battle, collection, inventory, PokeBallType::MasterBall);
  assert(!fullBoxCapture.accepted && !fullBoxCapture.caught);
  assert(inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] == 1);

  // Trainer teams can contain up to three Pokemon and can never be captured.
  PokemonCollection trainerCollection;
  CollectionLogic::initialize(trainerCollection);
  assert(CollectionLogic::chooseStarter(trainerCollection, 1));
  CollectionLogic::active(trainerCollection, 0)->level = 45;
  EncounterCharges trainerCharges;
  BattleState trainerBattle;
  assert(BattleEngine::startTrainer(trainerBattle, trainerCharges, trainerCollection,
                                    trainerCollection.party[0], 999));
  assert(trainerBattle.kind == BattleKind::Trainer);
  assert(trainerBattle.opponentCount >= 1 && trainerBattle.opponentCount <= 3);
  for (uint8_t i = 0; i < trainerBattle.opponentCount; ++i) {
    assert(trainerBattle.opponents[i].level >= 44 && trainerBattle.opponents[i].level <= 46);
  }
  const uint16_t ballCount = inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)];
  assert(!BattleEngine::throwBall(trainerBattle, trainerCollection, inventory,
                                  PokeBallType::MasterBall).accepted);
  assert(inventory.balls[static_cast<uint8_t>(PokeBallType::MasterBall)] == ballCount);

  WildEncounterClock wildClock;
  EncounterLogic::advanceWild(wildClock, WildEncounterClock::kMinimumSeconds - 1);
  assert(!wildClock.pending);
  EncounterLogic::advanceWild(wildClock, 1);
  assert(wildClock.pending);
  EncounterLogic::acknowledgeWild(wildClock);
  assert(!wildClock.pending && wildClock.elapsedSeconds == 0);
  assert(wildClock.targetSeconds >= WildEncounterClock::kMinimumSeconds);
  assert(wildClock.targetSeconds <= WildEncounterClock::kMinimumSeconds + WildEncounterClock::kWindowSeconds);

  GymProgress gyms;
  assert(GymSystem::next(gyms) == GymId::Pewter);
  const GymDefinition* brock = GymSystem::definition(GymId::Pewter);
  const GymDefinition* giovanni = GymSystem::definition(GymId::Viridian);
  assert(brock && brock->teamSize == 3 && brock->team[1].speciesId == 27);
  assert(giovanni && giovanni->teamSize == 3 && giovanni->team[0].level == 50);
  assert(!GymSystem::recordVictory(gyms, GymId::Cerulean));
  assert(GymSystem::recordVictory(gyms, GymId::Pewter));
  assert(GymSystem::hasBadge(gyms, GymId::Pewter) && GymSystem::next(gyms) == GymId::Cerulean);

  PokemonCollection gymCollection; CollectionLogic::initialize(gymCollection);
  assert(CollectionLogic::chooseStarter(gymCollection,7)); BattleState gymBattle;
  GymProgress freshGyms; assert(GymSystem::start(gymBattle,gymCollection,gymCollection.party[0],freshGyms,GymId::Pewter,44));
  assert(gymBattle.gymStage==0&&gymBattle.opponentItemUses==0);
  gymBattle.active=false;gymBattle.outcome=BattleOutcome::Victory;assert(GymSystem::advance(gymBattle,gymCollection));
  assert(gymBattle.gymStage==1&&gymBattle.active);
  gymBattle.active=false;gymBattle.outcome=BattleOutcome::Victory;assert(GymSystem::advance(gymBattle,gymCollection));
  assert(gymBattle.gymStage==2&&gymBattle.opponentItemUses==2&&gymBattle.opponentCount==3);
  assert(!GymSystem::advance(gymBattle,gymCollection));

  PokemonCollection learnCollection;CollectionLogic::initialize(learnCollection);assert(CollectionLogic::chooseStarter(learnCollection,4));
  OwnedPokemon* learner=CollectionLogic::active(learnCollection,0);const uint32_t learnerUid=learner->uid;
  *learner=CollectionLogic::createPokemon(learnerUid,4,18);learner->experience=experienceForLevel(findSpecies(4)->growthRate,19)-1;
  BattleState learnBattle;learnBattle.active=true;learnBattle.kind=BattleKind::Trainer;learnBattle.outcome=BattleOutcome::Ongoing;
  learnBattle.playerUid=learnerUid;learnBattle.opponentCount=1;learnBattle.opponents[0]=CollectionLogic::createPokemon(0,150,100);
  learnBattle.opponents[0].currentHp=1;for(uint8_t i=0;i<kMoveSlots;++i)learnBattle.opponents[0].moves[i]=MoveId::None;
  const BattleActionResult learnResult=BattleEngine::fight(learnBattle,learnCollection,0);
  assert(learnResult.movesToLearnCount>=1&&learnResult.moveLearnerUids[0]==learnerUid);
  assert(learnResult.movesToLearn[0]==moveLearnedAtLevel(4,19));
  return 0;
}
