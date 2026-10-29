#include <algorithm>
#include <cassert>
#include <cstring>

#include "game/Economy.h"
#include "game/MegaEvolution.h"
#include "game/Pokedex.h"
#include "game/UnovaAdventure.h"
#include "game/UnovaForms.h"
#include "game/UnovaTournament.h"

namespace {
bool containsMove(const MoveId moves[], uint8_t count, MoveId expected) {
  for (uint8_t index = 0; index < count; ++index)
    if (moves[index] == expected) return true;
  return false;
}

bool abilityIs(const OwnedPokemon& pokemon, const char* expected) {
  const AbilityData* ability = findAbility(pokemon.abilityId);
  return ability && std::strcmp(ability->name, expected) == 0;
}

bool sameMovePool(uint16_t species, uint8_t leftForm, uint8_t rightForm) {
  MoveId left[64]{}, right[64]{};
  const uint8_t leftCount = relearnableMovesForForm(
      species, leftForm, 100U, left, 64U, 5U);
  const uint8_t rightCount = relearnableMovesForForm(
      species, rightForm, 100U, right, 64U, 5U);
  return leftCount == rightCount &&
      std::equal(left, left + leftCount, right);
}

bool containsSpecies(const BattleState& battle, uint16_t species) {
  for (uint8_t index = 0; index < battle.opponentCount; ++index)
    if (battle.opponents[index].speciesId == species) return true;
  return false;
}

bool sawCannotMove(const BattleActionResult& result, BattleSide side,
                   uint16_t reason) {
  for (uint8_t index = 0; index < result.eventCount; ++index) {
    const BattleEvent& event = result.events[index];
    if (event.type == BattleEventType::CannotMove && event.side == side &&
        event.value == reason) return true;
  }
  return false;
}

bool sawFormChange(const BattleActionResult& result, BattleSide side,
                   uint8_t before, uint8_t after) {
  for (uint8_t index = 0; index < result.eventCount; ++index) {
    const BattleEvent& event = result.events[index];
    if (event.type == BattleEventType::MoveEffect && event.side == side &&
        event.value == static_cast<uint16_t>(BattleMoveEffect::FormChanged) &&
        event.before == before && event.after == after) return true;
  }
  return false;
}

void completeArea(UnovaAdventureState& state, UnovaArea area,
                  PokemonCollection& collection, uint32_t playerUid,
                  BattleState* completedBoss = nullptr) {
  assert(UnovaAdventure::begin(
      state, area, 0x51000000U + static_cast<uint8_t>(area), collection));
  while (UnovaAdventure::active(state)) {
    UnovaRoomChoice choices[3]{};
    const uint8_t count = UnovaAdventure::choices(state, choices);
    assert(count > 0U);
    uint8_t choice = 0U;
    if (count == 3U && state.run.depth == state.run.gateDepth) {
      while (choice < count &&
             choices[choice].kind != UnovaRoomKind::GateTrainer) ++choice;
      assert(choice < count);
    }
    const UnovaBattleRole pendingBefore = state.run.pendingBattle;
    (void)pendingBefore;
    const UnovaRoomResult room = UnovaAdventure::choose(state, choice, collection);
    if (!room.startsBattle) continue;
    const UnovaBattleRole role = state.run.pendingBattle;
    BattleState battle{};
    assert(UnovaAdventure::startPendingBattle(
        state, battle, collection, playerUid));
    assert(battle.unlockedGeneration == 5U && battle.opponentCount > 0U);
    if (role == UnovaBattleRole::GateTrainer)
      assert(battle.opponentCount == 2U);
    if (role == UnovaBattleRole::Boss) {
      assert(battle.opponentCount == 3U);
      if (completedBoss) *completedBoss = battle;
    }
    assert(UnovaAdventure::recordBattle(state, BattleOutcome::Victory));
  }
}

void initializeSingleBattle(BattleState& battle, PokemonCollection& collection,
                            uint32_t playerUid, uint16_t opponentSpecies,
                            MoveId opponentMove = static_cast<MoveId>(150U)) {
  BattleEngine::clear(battle);
  battle.active = true;
  battle.kind = BattleKind::Trainer;
  battle.outcome = BattleOutcome::Ongoing;
  battle.playerUid = playerUid;
  battle.opponentCount = 1U;
  battle.unlockedGeneration = 5U;
  battle.rngState = 0x554E4F56U;
  battle.opponents[0] = CollectionLogic::createPokemon(
      0U, opponentSpecies, 70U, false, 0x10050001U, 5U);
  battle.opponents[0].moves[0] = opponentMove;
  battle.opponents[0].movePp[0] = findFullMove(opponentMove)->pp;
  battle.opponents[0].maximumHp = battle.opponents[0].currentHp = 5000U;
  BattleEngine::ensureOpponentUids(battle);
}
}  // namespace

int main() {
  static_assert(kBoxCapacityV46 == 720U);
  static_assert(kPokedexSpeciesCountV46 == 649U);
  assert(UnovaAdventure::kLegendaryChanceDenominator == 265U);
  assert(UnovaAdventure::kSpecialLegendaryChanceDenominator == 240U);
  for (uint8_t area = 0U; area < UnovaAdventure::kAreaCount; ++area)
    assert(UnovaAdventure::hiddenGrottoGeneration(
        static_cast<UnovaArea>(area)) == area / 2U + 1U);
  for (uint16_t legendary : {494U, 638U, 639U, 640U, 641U, 642U,
                             643U, 644U, 645U, 646U, 647U, 648U, 649U})
    assert(PokedexLogic::isLegendaryOrMythical(legendary));

  // The generated National-Dex names must identify the species, never the
  // base form. Otherwise every alternate sprite is paired with a stale label
  // such as "BASCULIN RED STRIPED" or "TORNADUS INCARNATE".
  const struct { uint16_t species; const char* name; } formSpeciesNames[] = {
      {550U,"BASCULIN"},{555U,"DARMANITAN"},{585U,"DEERLING"},
      {586U,"SAWSBUCK"},{641U,"TORNADUS"},{642U,"THUNDURUS"},
      {645U,"LANDORUS"},{647U,"KELDEO"},{648U,"MELOETTA"},
  };
  for (const auto& expected : formSpeciesNames)
    assert(std::strcmp(findSpecies(expected.species)->name, expected.name) == 0);

  // Complete Generation-V form catalog. This protects the persistent form
  // numbers shared by save data, sprites, Summary and battle presentation.
  const struct { uint16_t species; uint8_t forms; } formCounts[] = {
      {550U,2U},{555U,2U},{585U,4U},{586U,4U},{641U,2U},{642U,2U},
      {645U,2U},{646U,3U},{647U,2U},{648U,2U},{649U,5U},
  };
  for (const auto& expected : formCounts)
    assert(CollectionLogic::formCount(expected.species) == expected.forms);
  assert(std::strcmp(CollectionLogic::formName(585U,2U),"AUTUMN") == 0);
  assert(std::strcmp(CollectionLogic::formName(646U,1U),"BLACK") == 0);
  assert(std::strcmp(CollectionLogic::formName(649U,4U),"CHILL") == 0);

  // Stripe and Ability slot are independent personality rolls. Both stripes
  // can have Adaptability; only Red gets Reckless and only Blue gets Rock
  // Head. Hidden Mold Breaker remains shared by both.
  bool basculinCombination[2][2]{};
  for (uint32_t seed=1U;seed<4096U;++seed) {
    OwnedPokemon basculin=CollectionLogic::createPokemon(
        0U,550U,30U,false,seed,5U);
    const uint8_t form=CollectionLogic::resolvedForm(basculin);
    const uint8_t abilitySlot=static_cast<uint8_t>(basculin.personality&1U);
    assert(form==static_cast<uint8_t>((basculin.personality>>1U)&1U));
    basculinCombination[form][abilitySlot]=true;
    assert(abilityIs(basculin,abilitySlot?"ADAPTABILITY":
        form?"ROCK HEAD":"RECKLESS"));
  }
  for (const auto& stripe:basculinCombination)
    for (bool found:stripe) assert(found);
  OwnedPokemon hiddenBasculin=CollectionLogic::createPokemon(
      0U,550U,30U,false,550U,5U);
  hiddenBasculin.form=1U;
  assert(CollectionLogic::setHiddenAbility(hiddenBasculin,true,5U));
  assert(abilityIs(hiddenBasculin,"MOLD BREAKER"));

  // Forms without an authored learnset difference must retain exactly the
  // same pool. Their differences belong to sprites, stats/types/Abilities or
  // battle state, not invented moves.
  for (uint16_t species:{550U,555U,641U,642U,645U,647U,648U})
    assert(sameMovePool(species,0U,1U));
  for (uint16_t species:{585U,586U})
    for (uint8_t form=1U;form<4U;++form)
      assert(sameMovePool(species,0U,form));
  for (uint8_t form=1U;form<5U;++form)
    assert(sameMovePool(649U,0U,form));

  PokemonCollection collection{};
  CollectionLogic::initialize(collection);
  assert(CollectionLogic::chooseStarter(collection, 1U));
  OwnedPokemon* starter = CollectionLogic::active(collection, 0U);
  assert(starter);
  starter->level = 80U;
  CollectionLogic::refreshDerivedStats(*starter, false);

  SinnohExpeditionState sinnoh{};
  UnovaAdventureState unova{};
  assert(!UnovaAdventure::available(sinnoh));
  sinnoh.unlockedSites = SinnohExpedition::kAllSitesMask;
  sinnoh.rewardFlags = SinnohExpedition::kGymSchemaBit;
  assert(!UnovaAdventure::available(sinnoh));
  sinnoh.gymVictoryMask = SinnohExpedition::kAllSitesMask;
  assert(UnovaAdventure::reconcile(unova, sinnoh));
  assert(UnovaAdventure::isAreaUnlocked(unova, UnovaArea::PinwheelForest));

  // A failed attempt clears only the volatile run. Permanent area progress
  // remains available for an immediate fresh attempt.
  assert(UnovaAdventure::begin(
      unova, UnovaArea::PinwheelForest, 0xFA110001U, collection));
  UnovaRoomChoice failureChoices[3]{};
  while (UnovaAdventure::active(unova) &&
         unova.run.pendingBattle == UnovaBattleRole::None) {
    const uint8_t count = UnovaAdventure::choices(unova, failureChoices);
    assert(count > 0U);
    uint8_t battleChoice = 0U;
    while (battleChoice < count &&
           failureChoices[battleChoice].kind == UnovaRoomKind::Healing)
      ++battleChoice;
    if (battleChoice == count) battleChoice = 0U;
    UnovaAdventure::choose(unova, battleChoice, collection);
  }
  assert(unova.run.pendingBattle != UnovaBattleRole::None);
  assert(UnovaAdventure::recordBattle(unova, BattleOutcome::Defeat));
  assert(!UnovaAdventure::active(unova));
  assert(UnovaAdventure::isAreaUnlocked(unova, UnovaArea::PinwheelForest));

  // Find a deterministic healing room. It restores HP, PP and status once,
  // then remains consumed for the rest of that area's attempt. Box members
  // outside the three-UID entry snapshot are deliberately untouched.
  uint32_t benchUid=0U;
  assert(CollectionLogic::add(collection,
      CollectionLogic::createPokemon(0U,4U,40U,false,0xBEEFU,5U),&benchUid));
  OwnedPokemon* bench=CollectionLogic::find(collection,benchUid);
  assert(bench);bench->currentHp=1U;bench->status=StatusCondition::Poison;
  bench->currentHp=bench->maximumHp;bench->status=StatusCondition::None;
  assert(UnovaAdventure::begin(
      unova,UnovaArea::PinwheelForest,0x50415254U,collection));
  assert(unova.run.partyUids[0]==starter->uid&&
         unova.run.partyUids[1]==kEmptyPokemonUid);
  unova.run.pendingBattle=UnovaBattleRole::Trainer;
  collection.party[0]=benchUid;
  BattleState substitutedParty{};
  assert(!UnovaAdventure::startPendingBattle(
      unova,substitutedParty,collection,benchUid));
  collection.party[0]=starter->uid;
  UnovaAdventure::abandon(unova);
  bench->currentHp=1U;bench->status=StatusCondition::Poison;
  uint32_t faintedUid=0U;
  uint32_t recoveringUid=0U;
  assert(CollectionLogic::add(collection,
      CollectionLogic::createPokemon(0U,7U,40U,false,0xFA170001U,5U),
      &faintedUid));
  assert(CollectionLogic::add(collection,
      CollectionLogic::createPokemon(0U,25U,40U,false,0xFA170002U,5U),
      &recoveringUid));
  OwnedPokemon* fainted=CollectionLogic::find(collection,faintedUid);
  OwnedPokemon* recovering=CollectionLogic::find(collection,recoveringUid);
  assert(fainted&&recovering);
  fainted->currentHp=0U;
  fainted->recoverySecondsRemaining=1U;
  recovering->currentHp=static_cast<uint16_t>(recovering->maximumHp/2U);
  recovering->recoverySecondsRemaining=1U;
  const uint16_t recoveringHpBeforeHeal=recovering->currentHp;
  collection.party[1]=faintedUid;
  collection.party[2]=recoveringUid;
  bool testedHeal = false;
  for (uint32_t seed = 1U; seed < 20000U && !testedHeal; ++seed) {
    assert(UnovaAdventure::begin(
        unova, UnovaArea::PinwheelForest, seed, collection));
    UnovaRoomChoice choices[3]{};
    const uint8_t count = UnovaAdventure::choices(unova, choices);
    for (uint8_t choice = 0; choice < count; ++choice) {
      if (choices[choice].kind != UnovaRoomKind::Healing) continue;
      starter->currentHp = 1U;
      starter->status = StatusCondition::Burn;
      starter->movePp[0] = 0U;
      const UnovaRoomResult room = UnovaAdventure::choose(
          unova, choice, collection);
      assert(room.healed && starter->currentHp == starter->maximumHp &&
             starter->status == StatusCondition::None &&
             starter->movePp[0] == CollectionLogic::maximumMovePp(*starter, 0U));
      assert(fainted->currentHp==0U&&fainted->recoverySecondsRemaining==1U);
      assert(recovering->currentHp==recoveringHpBeforeHeal&&
             recovering->recoverySecondsRemaining==1U);
      CollectionLogic::advanceRecovery(*fainted,1U,true,1U);
      CollectionLogic::advanceRecovery(*recovering,1U,true,1U);
      assert(fainted->currentHp==0U&&fainted->recoverySecondsRemaining==1U);
      assert(recovering->currentHp<recovering->maximumHp&&
             recovering->recoverySecondsRemaining==1U);
      assert(bench->currentHp==1U&&bench->status==StatusCondition::Poison);
      starter->currentHp = static_cast<uint16_t>(starter->maximumHp - 1U);
      assert(!UnovaAdventure::useAreaHeal(unova, collection));
      assert(starter->currentHp == starter->maximumHp - 1U);
      testedHeal = true;
      break;
    }
    UnovaAdventure::abandon(unova);
  }
  assert(testedHeal);
  collection.party[1]=kEmptyPokemonUid;
  collection.party[2]=kEmptyPokemonUid;
  starter->currentHp = starter->maximumHp;

  // Hidden Grottos are real event forks: they can yield an item or a wild
  // Pokemon whose Hidden Ability provenance is explicitly persisted. Two
  // areas serve each National-Dex generation, making retroactive Gen I-IV
  // Hidden Abilities discoverable without mixing every species in one pool.
  bool sawGrottoItem = false;
  static PokemonCollection grottoCollection;
  grottoCollection = collection;
  UnovaAdventureState grottoState = unova;
  grottoState.unlockedAreas = UnovaAdventure::kAllAreasMask;
  const uint16_t generationFirst[] = {1U, 152U, 252U, 387U, 494U};
  const uint16_t generationLast[] = {151U, 251U, 386U, 493U, 649U};
  uint16_t firstGenerationOneSpecies = 0U;
  for (uint8_t generation = 1U; generation <= 5U; ++generation) {
    const UnovaArea area = static_cast<UnovaArea>((generation - 1U) * 2U);
    bool sawGenerationEncounter = false;
    for (uint32_t seed = 1U; seed < 100000U && !sawGenerationEncounter;
         ++seed) {
      assert(UnovaAdventure::begin(grottoState, area, seed,
                                    grottoCollection));
      UnovaRoomChoice choices[3]{};
      const uint8_t count = UnovaAdventure::choices(grottoState, choices);
      for (uint8_t choice = 0; choice < count; ++choice) {
        if (choices[choice].kind != UnovaRoomKind::HiddenGrotto) continue;
        const UnovaRoomResult room = UnovaAdventure::choose(
            grottoState, choice, grottoCollection);
        if (room.foundItem) {
          assert(room.rewardKind >= 1U && room.rewardKind <= 3U);
          sawGrottoItem = true;
          break;
        }
        assert(room.startsBattle && room.hiddenAbilityEncounter &&
               room.speciesId >= generationFirst[generation - 1U] &&
               room.speciesId <= generationLast[generation - 1U]);
        const SpeciesData* species = findSpecies(room.speciesId);
        assert(species && species->hiddenAbility != 0U);
        if (room.legendaryEncounter) {
          assert(PokedexLogic::isLegendaryOrMythical(room.speciesId));
          break;
        }
        assert(!PokedexLogic::isLegendaryOrMythical(room.speciesId));
        BattleState grotto{};
        assert(UnovaAdventure::startPendingBattle(
            grottoState, grotto, grottoCollection,
            grottoCollection.party[0]));
        assert(grotto.opponentCount == 1U &&
               CollectionLogic::hasHiddenAbility(grotto.opponents[0]) &&
               grotto.opponents[0].abilityId == species->hiddenAbility);
        if (generation == 1U)
          firstGenerationOneSpecies = grotto.opponents[0].speciesId;
        assert(CollectionLogic::add(grottoCollection, grotto.opponents[0]));
        bool persisted = false;
        for (const OwnedPokemon& pokemon : grottoCollection.box) {
          if (pokemon.speciesId == room.speciesId &&
              CollectionLogic::hasHiddenAbility(pokemon)) persisted = true;
        }
        assert(persisted);
        sawGenerationEncounter = true;
        break;
      }
      UnovaAdventure::abandon(grottoState);
    }
    assert(sawGenerationEncounter);
  }
  assert(sawGrottoItem && firstGenerationOneSpecies != 0U);

  // Once a HA variant is stored, the collector-friendly pool excludes that
  // species until every other legal Gen-I variant has been obtained.
  bool sawDifferentGenerationOneSpecies = false;
  for (uint32_t seed = 100001U; seed < 200000U &&
       !sawDifferentGenerationOneSpecies; ++seed) {
    assert(UnovaAdventure::begin(grottoState, UnovaArea::PinwheelForest,
                                  seed, grottoCollection));
    UnovaRoomChoice choices[3]{};
    const uint8_t count = UnovaAdventure::choices(grottoState, choices);
    for (uint8_t choice = 0; choice < count; ++choice) {
      if (choices[choice].kind != UnovaRoomKind::HiddenGrotto) continue;
      const UnovaRoomResult room = UnovaAdventure::choose(
          grottoState, choice, grottoCollection);
      if (!room.foundItem) {
        assert(room.speciesId != firstGenerationOneSpecies);
        sawDifferentGenerationOneSpecies = true;
      }
      break;
    }
    UnovaAdventure::abandon(grottoState);
  }
  assert(sawDifferentGenerationOneSpecies);

  // Hidden-Ability legendaries remain genuinely rare, but are no longer
  // replaced by a normal-ability legendary that violates the Grotto promise.
  bool sawHiddenAbilityLegendary = false;
  for (uint32_t seed = 1U; seed < 500000U && !sawHiddenAbilityLegendary;
       ++seed) {
    assert(UnovaAdventure::begin(grottoState, UnovaArea::PinwheelForest,
                                  seed, grottoCollection));
    UnovaRoomChoice choices[3]{};
    const uint8_t count = UnovaAdventure::choices(grottoState, choices);
    for (uint8_t choice = 0; choice < count; ++choice) {
      if (choices[choice].kind != UnovaRoomKind::HiddenGrotto) continue;
      const UnovaRoomResult room = UnovaAdventure::choose(
          grottoState, choice, grottoCollection);
      if (room.legendaryEncounter) {
        assert(room.startsBattle && room.hiddenAbilityEncounter &&
               room.speciesId >= 1U && room.speciesId <= 151U &&
               PokedexLogic::isLegendaryOrMythical(room.speciesId));
        const SpeciesData* species = findSpecies(room.speciesId);
        assert(species && species->hiddenAbility != 0U);
        BattleState legendaryGrotto{};
        assert(UnovaAdventure::startPendingBattle(
            grottoState, legendaryGrotto, grottoCollection,
            grottoCollection.party[0]));
        assert(legendaryGrotto.opponentCount == 1U &&
               CollectionLogic::hasHiddenAbility(
                   legendaryGrotto.opponents[0]) &&
               legendaryGrotto.opponents[0].abilityId ==
                   species->hiddenAbility);
        sawHiddenAbilityLegendary = true;
      }
      break;
    }
    UnovaAdventure::abandon(grottoState);
  }
  assert(sawHiddenAbilityLegendary);

  // A wrong combat fork on the Gate floor consumes that trainer battle but
  // cannot advance beyond the still-undefeated Gate Trainer and strand the
  // run. The non-combat clue fork follows the same rule in choose().
  UnovaAdventureState forkState=unova;
  assert(UnovaAdventure::begin(
      forkState,UnovaArea::PinwheelForest,0x47415445U,collection));
  while(forkState.run.depth<forkState.run.gateDepth){
    UnovaRoomChoice choices[3]{};
    assert(UnovaAdventure::choices(forkState,choices)==3U);
    const UnovaRoomResult room=UnovaAdventure::choose(forkState,0U,collection);
    if(room.startsBattle){
      BattleState pathBattle{};
      assert(UnovaAdventure::startPendingBattle(
          forkState,pathBattle,collection,starter->uid));
      assert(UnovaAdventure::recordBattle(forkState,BattleOutcome::Victory));
    }
  }
  UnovaRoomChoice gateForks[3]{};
  assert(UnovaAdventure::choices(forkState,gateForks)==3U);
  uint8_t trainerFork=0U;
  while(trainerFork<3U&&gateForks[trainerFork].kind!=UnovaRoomKind::Trainer)
    ++trainerFork;
  assert(trainerFork<3U);
  const uint8_t gateDepthBefore=forkState.run.depth;
  assert(UnovaAdventure::choose(
      forkState,trainerFork,collection).startsBattle);
  BattleState wrongForkBattle{};
  assert(UnovaAdventure::startPendingBattle(
      forkState,wrongForkBattle,collection,starter->uid));
  assert(UnovaAdventure::recordBattle(forkState,BattleOutcome::Victory));
  assert(forkState.run.depth==gateDepthBefore);
  UnovaRoomChoice consumedForks[3]{};
  assert(UnovaAdventure::choices(forkState,consumedForks)==3U);
  assert(consumedForks[trainerFork].kind==UnovaRoomKind::ClueNpc);
  assert(UnovaAdventure::abandon(forkState));

  // Complete all ten areas. Every boss permanently opens exactly the next
  // area; White Treehollow ends with White 2 Benga's Latias/Garchomp/Volcarona
  // team and never injects a shiny-Dratini reward.
  BattleState benga{};
  for (uint8_t area = 0; area < UnovaAdventure::kAreaCount; ++area) {
    assert(UnovaAdventure::isAreaUnlocked(
        unova, static_cast<UnovaArea>(area)));
    completeArea(unova, static_cast<UnovaArea>(area), collection,
                 starter->uid,
                 area + 1U == UnovaAdventure::kAreaCount ? &benga : nullptr);
  }
  assert(UnovaAdventure::allAreasCompleted(unova));
  assert(containsSpecies(benga, 380U) && containsSpecies(benga, 445U) &&
         containsSpecies(benga, 637U));
  for (uint8_t index = 0; index < benga.opponentCount; ++index)
    assert(!benga.opponents[index].shiny);

  // The generation gate is retroactive for old species' new level-up moves,
  // but the dedicated Keldeo/Meloetta event moves are still absent before it.
  MoveId relearnable[64]{};
  uint8_t count = relearnableMovesForForm(4U, 0U, 100U, relearnable, 64U, 4U);
  assert(!containsMove(relearnable, count, static_cast<MoveId>(481U)));
  count = relearnableMovesForForm(4U, 0U, 100U, relearnable, 64U, 5U);
  assert(containsMove(relearnable, count, static_cast<MoveId>(481U)));
  count = relearnableMovesForForm(647U, 0U, 100U, relearnable, 64U, 4U);
  assert(!containsMove(relearnable, count, static_cast<MoveId>(548U)));
  count = relearnableMovesForForm(647U, 0U, 100U, relearnable, 64U, 5U);
  assert(containsMove(relearnable, count, static_cast<MoveId>(548U)));
  count = relearnableMovesForForm(648U, 0U, 100U, relearnable, 64U, 5U);
  assert(containsMove(relearnable, count, static_cast<MoveId>(547U)));

  // B2W2 has three distinct Kyurem level-up tables. Summary/relearner,
  // ordinary leveling and Rare Candy must all resolve the same table.
  MoveId normalKyurem[64]{}, blackKyurem[64]{}, whiteKyurem[64]{};
  const uint8_t normalKyuremCount=relearnableMovesForForm(
      646U,0U,100U,normalKyurem,64U,5U);
  const uint8_t blackKyuremCount=relearnableMovesForForm(
      646U,1U,100U,blackKyurem,64U,5U);
  const uint8_t whiteKyuremCount=relearnableMovesForForm(
      646U,2U,100U,whiteKyurem,64U,5U);
  assert(containsMove(normalKyurem,normalKyuremCount,static_cast<MoveId>(184U))&&
         containsMove(normalKyurem,normalKyuremCount,static_cast<MoveId>(549U))&&
         !containsMove(normalKyurem,normalKyuremCount,static_cast<MoveId>(559U))&&
         !containsMove(normalKyurem,normalKyuremCount,static_cast<MoveId>(558U)));
  assert(!containsMove(blackKyurem,blackKyuremCount,static_cast<MoveId>(184U))&&
         !containsMove(blackKyurem,blackKyuremCount,static_cast<MoveId>(549U))&&
         containsMove(blackKyurem,blackKyuremCount,static_cast<MoveId>(559U))&&
         containsMove(blackKyurem,blackKyuremCount,static_cast<MoveId>(553U)));
  assert(!containsMove(whiteKyurem,whiteKyuremCount,static_cast<MoveId>(184U))&&
         !containsMove(whiteKyurem,whiteKyuremCount,static_cast<MoveId>(549U))&&
         containsMove(whiteKyurem,whiteKyuremCount,static_cast<MoveId>(558U))&&
         containsMove(whiteKyurem,whiteKyuremCount,static_cast<MoveId>(554U)));
  assert(moveLearnedAtLevelForForm(646U,0U,43U,5U)==static_cast<MoveId>(184U));
  assert(moveLearnedAtLevelForForm(646U,1U,43U,5U)==static_cast<MoveId>(559U));
  assert(moveLearnedAtLevelForForm(646U,2U,43U,5U)==static_cast<MoveId>(558U));
  assert(moveLearnedAtLevelForForm(646U,1U,50U,5U)==static_cast<MoveId>(553U));
  assert(moveLearnedAtLevelForForm(646U,2U,50U,5U)==static_cast<MoveId>(554U));
  OwnedPokemon candyKyurem=CollectionLogic::createPokemon(
      0U,646U,42U,false,0x64642U,5U);
  candyKyurem.form=1U;
  RareCandyProgression candyProgression=
      CollectionLogic::applyRareCandy(candyKyurem,5U);
  assert(candyProgression.levelIncreased&&
         candyProgression.learnedMove==static_cast<MoveId>(559U));

  OwnedPokemon keldeo = CollectionLogic::createPokemon(
      0U, 647U, 100U, false, 0x647U, 5U);
  assert(CollectionLogic::resolvedForm(keldeo) == 0U);
  keldeo.moves[0] = static_cast<MoveId>(548U);
  assert(CollectionLogic::resolvedForm(keldeo) == 1U);

  // Exact B2W2 combat identities for every stat/type-changing Unova form.
  const struct {
    uint16_t species;
    uint8_t attack,defense,speed,spAttack,spDefense;
    const char* ability;
  } therians[] = {
      {641U,100U,80U,121U,110U,90U,"REGENERATOR"},
      {642U,105U,70U,101U,145U,80U,"VOLT ABSORB"},
      {645U,145U,90U,91U,105U,80U,"INTIMIDATE"},
  };
  for (const auto& expected:therians) {
    OwnedPokemon genie=CollectionLogic::createPokemon(
        0U,expected.species,70U,false,expected.species,5U);
    assert(CollectionLogic::setForm(genie,1U));
    assert(CollectionLogic::resolvedBaseStat(genie,PokemonStat::Attack)==expected.attack&&
           CollectionLogic::resolvedBaseStat(genie,PokemonStat::Defense)==expected.defense&&
           CollectionLogic::resolvedBaseStat(genie,PokemonStat::Speed)==expected.speed&&
           CollectionLogic::resolvedBaseStat(genie,PokemonStat::SpAttack)==expected.spAttack&&
           CollectionLogic::resolvedBaseStat(genie,PokemonStat::SpDefense)==expected.spDefense&&
           abilityIs(genie,expected.ability));
  }
  OwnedPokemon zen=CollectionLogic::createPokemon(
      0U,555U,70U,false,555U,5U);
  assert(CollectionLogic::setHiddenAbility(zen,true,5U));
  zen.form=1U;
  assert(CollectionLogic::resolvedBaseStat(zen,PokemonStat::Attack)==30U&&
         CollectionLogic::resolvedBaseStat(zen,PokemonStat::Defense)==105U&&
         CollectionLogic::resolvedBaseStat(zen,PokemonStat::Speed)==55U&&
         CollectionLogic::resolvedBaseStat(zen,PokemonStat::SpAttack)==140U&&
         CollectionLogic::resolvedBaseStat(zen,PokemonStat::SpDefense)==105U&&
         MegaEvolution::type1(zen,findSpecies(555U)->type1)==PokemonType::Fire&&
         MegaEvolution::type2(zen,findSpecies(555U)->type2)==PokemonType::Psychic&&
         abilityIs(zen,"ZEN MODE"));
  PokemonCollection zenCollection{};
  CollectionLogic::initialize(zenCollection);
  zen.form=0U;
  uint32_t zenUid=0U;
  assert(CollectionLogic::add(zenCollection,zen,&zenUid));
  assert(CollectionLogic::setPartySlot(zenCollection,0U,zenUid));
  OwnedPokemon* storedZen=CollectionLogic::find(zenCollection,zenUid);
  storedZen->currentHp=storedZen->maximumHp/2U;
  BattleState zenBattle{};
  initializeSingleBattle(zenBattle,zenCollection,zenUid,137U);
  const BattleActionResult enterZen=BattleEngine::fightPvp(
      zenBattle,zenCollection,0U,0xFEU);
  assert(enterZen.accepted&&storedZen->form==1U&&
         sawFormChange(enterZen,BattleSide::Player,0U,1U));
  storedZen->currentHp=storedZen->maximumHp;
  const BattleActionResult leaveZen=BattleEngine::fightPvp(
      zenBattle,zenCollection,0U,0xFEU);
  assert(leaveZen.accepted&&storedZen->form==0U&&
         sawFormChange(leaveZen,BattleSide::Player,1U,0U));
  for (uint8_t form=1U;form<=2U;++form) {
    OwnedPokemon resolvedKyurem=CollectionLogic::createPokemon(
        0U,646U,70U,false,646U+form,5U);
    resolvedKyurem.form=form;
    CollectionLogic::refreshAbility(resolvedKyurem,5U);
    assert(CollectionLogic::resolvedBaseStat(
               resolvedKyurem,PokemonStat::Attack)==(form==1U?170U:120U)&&
           CollectionLogic::resolvedBaseStat(
               resolvedKyurem,PokemonStat::SpAttack)==(form==1U?120U:170U)&&
           abilityIs(resolvedKyurem,form==1U?"TERAVOLT":"TURBOBLAZE"));
  }

  // A locked Market can never leak the 25 Gen-V machines. Once Unova opens,
  // owning every older machine forces the guaranteed slot into that range.
  MartState mart{};
  Economy::rotate(mart, 8U, ~uint64_t{0}, ~uint64_t{0}, true, 0U, false);
  for (const MartOffer& offer : mart.offers)
    assert(!offer.isMachine() || offer.machineId() < 100U);
  mart.elapsedSeconds = Economy::kEndgameRotationSeconds;
  Economy::rotate(mart, 8U, ~uint64_t{0}, ~uint64_t{0}, true, 0U, true);
  bool sawUnovaMachine = false;
  for (const MartOffer& offer : mart.offers)
    if (offer.isMachine() && offer.machineId() >= 100U) sawUnovaMachine = true;
  assert(sawUnovaMachine);

  // Deerling and Sawsbuck follow the world season together rather than
  // receiving a random permanent coat. Evolution keeps the active season.
  PokemonCollection seasonalCollection{};
  CollectionLogic::initialize(seasonalCollection);
  uint32_t deerlingUid=0U;
  assert(CollectionLogic::add(seasonalCollection,
      CollectionLogic::createPokemon(0U,585U,40U,false,585U,5U),
      &deerlingUid));
  assert(CollectionLogic::resolvedForm(
      *CollectionLogic::find(seasonalCollection,deerlingUid))==0U);
  assert(CollectionLogic::applyUnovaSeason(seasonalCollection,3U));
  assert(CollectionLogic::resolvedForm(
      *CollectionLogic::find(seasonalCollection,deerlingUid))==3U);
  assert(CollectionLogic::evolve(
      seasonalCollection,deerlingUid,586U,nullptr,5U));
  assert(CollectionLogic::resolvedForm(
      *CollectionLogic::find(seasonalCollection,deerlingUid))==3U);

  // Signature key items and all five official Genesect drive forms.
  OwnedPokemon genesect = CollectionLogic::createPokemon(
      0U, 649U, 70U, false, 91U, 5U);
  uint32_t genesectUid = 0U;
  assert(CollectionLogic::add(collection, genesect, &genesectUid));
  Gen5UniqueItemInventory items{};
  assert(UnovaForms::reconcileUniqueItems(collection, items, true));
  for (uint8_t index = 0; index < kGen5DriveCount; ++index)
    assert(items.driveQuantities[index] == 1U);
  OwnedPokemon* storedGenesect = CollectionLogic::find(collection, genesectUid);
  assert(storedGenesect);
  storedGenesect->heldItem = HeldItem::BurnDrive;
  assert(CollectionLogic::resolvedForm(*storedGenesect) == 3U);
  assert(UnovaForms::genesectTechnoBlastType(*storedGenesect) ==
         PokemonType::Fire);

  // The Forces of Nature use the regular Box FORM action without a key item.
  OwnedPokemon tornadus = CollectionLogic::createPokemon(
      0U, 641U, 70U, false, 92U, 5U);
  uint32_t tornadusUid = 0U;
  assert(CollectionLogic::add(collection, tornadus, &tornadusUid));
  assert(!UnovaForms::reconcileUniqueItems(collection, items, true));
  assert((items.keyItems & kRevealGlassBit) == 0U);
  OwnedPokemon* storedTornadus = CollectionLogic::find(collection, tornadusUid);
  assert(storedTornadus);
  const uint8_t incarnateAttack = CollectionLogic::resolvedBaseStat(
      *storedTornadus, PokemonStat::Attack);
  const uint8_t incarnateAbility = storedTornadus->abilityId;
  assert(CollectionLogic::canChangeForm(*storedTornadus));
  assert(CollectionLogic::setForm(*storedTornadus, 1U));
  assert(CollectionLogic::resolvedForm(*storedTornadus) == 1U &&
         CollectionLogic::resolvedBaseStat(*storedTornadus, PokemonStat::Attack) !=
             incarnateAttack &&
         storedTornadus->abilityId != incarnateAbility);
  for (uint16_t species : {642U, 645U}) {
    OwnedPokemon genie = CollectionLogic::createPokemon(
        0U, species, 70U, false, species, 5U);
    assert(CollectionLogic::canChangeForm(genie));
    assert(CollectionLogic::setForm(genie, 1U));
    assert(CollectionLogic::resolvedForm(genie) == 1U);
  }

  // DNA Splicers remove the donor into the save sidecar, replace both Kyurem
  // signature moves and restore the exact donor/moves when separated. Test
  // both Black and White Kyurem, not only one branch.
  OwnedPokemon kyurem = CollectionLogic::createPokemon(
      0U, 646U, 75U, false, 101U, 5U);
  kyurem.moves[0] = static_cast<MoveId>(184U);
  kyurem.moves[1] = static_cast<MoveId>(549U);
  kyurem.moves[2] = static_cast<MoveId>(16U);
  kyurem.movePp[2] = findFullMove(kyurem.moves[2])->pp;
  OwnedPokemon zekrom = CollectionLogic::createPokemon(
      0U, 644U, 75U, false, 102U, 5U);
  OwnedPokemon reshiram = CollectionLogic::createPokemon(
      0U, 643U, 75U, false, 103U, 5U);
  uint32_t kyuremUid = 0U, zekromUid = 0U, reshiramUid = 0U;
  assert(CollectionLogic::add(collection, kyurem, &kyuremUid));
  assert(CollectionLogic::add(collection, zekrom, &zekromUid));
  assert(CollectionLogic::add(collection, reshiram, &reshiramUid));
  assert(UnovaForms::reconcileUniqueItems(collection, items, true));
  UnovaFormState forms{};
  assert(UnovaForms::fuse(
      collection, forms, items, kyuremUid, zekromUid));
  OwnedPokemon* storedKyurem = CollectionLogic::find(collection, kyuremUid);
  assert(storedKyurem && CollectionLogic::resolvedForm(*storedKyurem) == 1U &&
         containsMove(storedKyurem->moves, kMoveSlots,
                      static_cast<MoveId>(559U)) &&
         containsMove(storedKyurem->moves, kMoveSlots,
                      static_cast<MoveId>(553U)));
  assert(!CollectionLogic::find(collection, zekromUid));
  // A normal edit made while fused must survive separation; only the two
  // fusion-bound moves revert to Scary Face and Glaciate.
  storedKyurem->moves[2]=static_cast<MoveId>(33U);
  storedKyurem->movePp[2]=findFullMove(storedKyurem->moves[2])->pp;
  assert(UnovaForms::unfuse(collection, forms));
  storedKyurem = CollectionLogic::find(collection, kyuremUid);
  assert(storedKyurem&&storedKyurem->moves[0]==static_cast<MoveId>(184U)&&
         storedKyurem->moves[1]==static_cast<MoveId>(549U)&&
         storedKyurem->moves[2]==static_cast<MoveId>(33U));
  assert(CollectionLogic::find(collection, zekromUid));
  assert(UnovaForms::fuse(
      collection, forms, items, kyuremUid, reshiramUid));
  storedKyurem = CollectionLogic::find(collection, kyuremUid);
  assert(storedKyurem && CollectionLogic::resolvedForm(*storedKyurem) == 2U &&
         containsMove(storedKyurem->moves, kMoveSlots,
                      static_cast<MoveId>(558U)) &&
         containsMove(storedKyurem->moves, kMoveSlots,
                      static_cast<MoveId>(554U)));
  assert(UnovaForms::unfuse(collection, forms));

  // Relic Song toggles Meloetta only inside battle and battle teardown
  // always restores Aria form.
  PokemonCollection meloettaCollection{};
  CollectionLogic::initialize(meloettaCollection);
  OwnedPokemon meloetta = CollectionLogic::createPokemon(
      0U, 648U, 70U, false, 648U, 5U);
  meloetta.moves[0] = static_cast<MoveId>(547U);
  meloetta.movePp[0] = findFullMove(meloetta.moves[0])->pp;
  uint32_t meloettaUid = 0U;
  assert(CollectionLogic::add(
      meloettaCollection, meloetta, &meloettaUid));
  assert(CollectionLogic::setPartySlot(
      meloettaCollection, 0U, meloettaUid));
  BattleState relicBattle{};
  initializeSingleBattle(
      relicBattle, meloettaCollection, meloettaUid, 137U);
  const BattleActionResult relicResult = BattleEngine::fightPvp(
      relicBattle, meloettaCollection, 0U, 0xFEU);
  assert(relicResult.accepted &&
         CollectionLogic::find(meloettaCollection, meloettaUid)->form == 1U);
  const OwnedPokemon* pirouette=CollectionLogic::find(
      meloettaCollection,meloettaUid);
  assert(pirouette&&
         CollectionLogic::resolvedBaseStat(*pirouette,PokemonStat::Attack)==128U&&
         CollectionLogic::resolvedBaseStat(*pirouette,PokemonStat::Speed)==128U&&
         CollectionLogic::resolvedBaseStat(*pirouette,PokemonStat::SpAttack)==77U&&
         MegaEvolution::type2(*pirouette,findSpecies(648U)->type2)==
             PokemonType::Fighting);
  assert(BattleEngine::run(relicBattle, meloettaCollection).accepted);
  assert(CollectionLogic::find(meloettaCollection, meloettaUid)->form == 0U);

  // Sky Drop owns both turns: the target cannot act while carried, carriage
  // clears on release, and a Flying target is lifted but takes no damage.
  PokemonCollection skyCollection{};
  CollectionLogic::initialize(skyCollection);
  OwnedPokemon skyUser = CollectionLogic::createPokemon(
      0U, 142U, 70U, false, 507U, 5U);
  skyUser.moves[0] = static_cast<MoveId>(507U);
  skyUser.movePp[0] = findFullMove(skyUser.moves[0])->pp;
  uint32_t skyUid = 0U;
  assert(CollectionLogic::add(skyCollection, skyUser, &skyUid));
  assert(CollectionLogic::setPartySlot(skyCollection, 0U, skyUid));
  BattleState skyBattle{};
  initializeSingleBattle(skyBattle, skyCollection, skyUid, 137U);
  const BattleActionResult lift = BattleEngine::fightPvp(
      skyBattle, skyCollection, 0U, 0U);
  assert(lift.accepted && skyBattle.opponentGen5Effects[0].skyDropCarried &&
         sawCannotMove(lift, BattleSide::Opponent, 14U));
  const uint16_t groundedBefore = skyBattle.opponents[0].currentHp;
  const BattleActionResult drop = BattleEngine::fightPvp(
      skyBattle, skyCollection, 0U, 0U);
  assert(drop.accepted && !skyBattle.opponentGen5Effects[0].skyDropCarried &&
         skyBattle.opponents[0].currentHp < groundedBefore);
  BattleState flyingDrop{};
  initializeSingleBattle(flyingDrop, skyCollection, skyUid, 16U);
  assert(BattleEngine::fightPvp(
      flyingDrop, skyCollection, 0U, 0U).accepted);
  const uint16_t flyingBefore = flyingDrop.opponents[0].currentHp;
  assert(BattleEngine::fightPvp(
      flyingDrop, skyCollection, 0U, 0U).accepted);
  assert(flyingDrop.opponents[0].currentHp == flyingBefore);

  // PWT checkpoints unlock after every two area bosses. Champions is the
  // conclusion gate and unlocks both White 2 and the daily Royal Unova.
  // Entering any tournament battle must not be a free Pokecenter: damage,
  // status, PP and recovery timers persist even if the player immediately
  // runs from the match.
  starter->currentHp=static_cast<uint16_t>(starter->maximumHp/3U);
  starter->status=StatusCondition::Burn;
  starter->movePp[0]=1U;
  bench->currentHp=1U;
  bench->status=StatusCondition::Poison;
  bench->recoverySecondsRemaining=11U;
  collection.party[1]=benchUid;
  const auto assertTournamentPartyUnchanged=[&](){
    assert(starter->currentHp==starter->maximumHp/3U);
    assert(starter->status==StatusCondition::Burn);
    assert(starter->movePp[0]==1U);
    assert(bench->currentHp==1U);
    assert(bench->status==StatusCondition::Poison);
    assert(bench->recoverySecondsRemaining==11U);
  };
  for (uint8_t tier = 0U; tier < 5U; ++tier) {
    assert(UnovaAdventure::pwtTierUnlocked(unova, tier));
    assert(UnovaTournament::beginPwt(unova, tier, 222U + tier));
    if(tier==0U){
      starter->recoverySecondsRemaining=2U;
      BattleState blockedPwt{};
      assert(!UnovaTournament::startBattle(
          unova,blockedPwt,collection,starter->uid));
      assert(starter->recoverySecondsRemaining==2U);
      starter->recoverySecondsRemaining=0U;
    }
    for (uint8_t round = 0; round < 3U; ++round) {
      BattleState pwt{};
      assert(UnovaTournament::startBattle(
          unova, pwt, collection, starter->uid));
      assert(pwt.kind == BattleKind::Pwt && pwt.opponentCount == 3U);
      assertTournamentPartyUnchanged();
      bool complete = false;
      assert(UnovaTournament::recordBattle(
          unova, BattleOutcome::Victory, complete));
      assert(complete == (round == 2U));
    }
  }
  assert(unova.pwtCompletedTiers == 0x1FU);
  assert(UnovaTournament::championsUnlocked(unova));
  assert(UnovaTournament::beginChampions(unova, 333U));
  for (uint8_t round = 0; round < 5U; ++round) {
    BattleState champion{};
    assert(UnovaTournament::startBattle(
        unova, champion, collection, starter->uid));
    assertTournamentPartyUnchanged();
    bool complete = false;
    assert(UnovaTournament::recordBattle(
        unova, BattleOutcome::Victory, complete));
    assert(complete == (round == 4U));
  }
  assert(UnovaAdventure::championsCompleted(unova));
  assert(UnovaAdventure::white2ThemeUnlocked(unova));
  assert(!UnovaAdventure::royalUnovaCompletedAtLeastOnce(unova));
  assert(!UnovaAdventure::fullyCompleted(unova));
  Inventory unovaRewardInventory{};
  assert(!UnovaAdventure::reconcileCompletionReward(
      unova, unovaRewardInventory));
  assert(UnovaTournament::championsUnlocked(unova));
  assert(UnovaTournament::beginChampions(unova, 334U));
  assert(UnovaTournament::abandon(unova));
  assert(UnovaAdventure::royalTrainerCount(0U) == 3U &&
         UnovaAdventure::royalTrainerCount(4U) == 7U &&
         UnovaAdventure::royalTrainerCount(5U) == 5U &&
         UnovaAdventure::royalTrainerCount(6U) == 4U &&
         UnovaAdventure::royalTrainerCount(7U) == 3U);
  assert(UnovaTournament::beginRoyal(unova, 12U, 444U));
  assert(unova.tournament.rounds == 5U);
  for (uint8_t round = 0; round < 5U; ++round) {
    BattleState royal{};
    assert(UnovaTournament::startBattle(
        unova, royal, collection, starter->uid));
    assert(royal.kind == BattleKind::RoyalUnova);
    assertTournamentPartyUnchanged();
    bool complete = false;
    assert(UnovaTournament::recordBattle(
        unova, BattleOutcome::Victory, complete));
  }
  assert(!UnovaAdventure::royalAvailableToday(unova, 12U));
  assert(UnovaAdventure::royalAvailableToday(unova, 13U));
  assert(UnovaAdventure::royalUnovaCompletedAtLeastOnce(unova));
  assert(UnovaAdventure::fullyCompleted(unova));
  assert(UnovaAdventure::reconcileCompletionReward(
      unova, unovaRewardInventory));
  assert(unovaRewardInventory.balls[
             static_cast<uint8_t>(PokeBallType::MasterBall)] == 1U);
  assert(!UnovaAdventure::reconcileCompletionReward(
      unova, unovaRewardInventory));

  UnovaAdventureState pendingUnovaReward = unova;
  pendingUnovaReward.flags = static_cast<uint8_t>(
      pendingUnovaReward.flags & ~UnovaAdventure::kMasterBallRewardBit);
  Inventory fullUnovaInventory{};
  fullUnovaInventory.balls[
      static_cast<uint8_t>(PokeBallType::MasterBall)] = kInventoryStackLimit;
  assert(!UnovaAdventure::reconcileCompletionReward(
      pendingUnovaReward, fullUnovaInventory));
  fullUnovaInventory.balls[
      static_cast<uint8_t>(PokeBallType::MasterBall)] = 0U;
  assert(UnovaAdventure::reconcileCompletionReward(
      pendingUnovaReward, fullUnovaInventory));
  return 0;
}
