#include <cassert>
#include <cstring>
#include <initializer_list>

#include "game/KalosAdventure.h"
#include "game/BattleAnimationData.h"
#include "game/Economy.h"
#include "game/MegaEvolution.h"

namespace {
void completeArea(KalosAdventureState& kalos, KalosArea area,
                  PokemonCollection& collection, uint32_t playerUid) {
  assert(KalosAdventure::begin(kalos, area,
      0x60000000UL + static_cast<uint8_t>(area), collection));
  kalos.run.step = KalosAdventure::kStepsPerArea;
  KalosRoomChoice choices[2]{};
  assert(KalosAdventure::choices(kalos, choices) == 1U);
  assert(choices[0].kind == KalosRoomKind::Leader);
  const KalosRoomResult result = KalosAdventure::choose(kalos, 0U, collection);
  assert(result.startsBattle);
  BattleState battle{};
  assert(KalosAdventure::startPendingBattle(
      kalos, battle, collection, playerUid));
  assert(battle.active && battle.kind == BattleKind::KalosAdventure);
  assert(battle.unlockedGeneration == 6U && battle.opponentCount == 3U);
  for (uint8_t slot = 0U; slot < battle.opponentCount; ++slot)
    assert(battle.opponents[slot].speciesId >= 1U &&
           battle.opponents[slot].speciesId <= 721U);
  assert(KalosAdventure::recordBattle(kalos, BattleOutcome::Victory));
}
}

int main() {
  UnovaAdventureState unova{};
  KalosAdventureState kalos{};
  assert(!KalosAdventure::available(unova));
  assert(!KalosAdventure::reconcile(kalos, unova));
  assert(!KalosAdventure::xThemeUnlocked(kalos));
  assert(!KalosAdventure::xThemeSelected(kalos));
  assert(!KalosAdventure::setXThemeSelected(kalos, true));

  unova.unlockedAreas = UnovaAdventure::kAllAreasMask;
  unova.defeatedBosses = UnovaAdventure::kAllAreasMask;
  unova.pwtCompletedTiers = 0x1FU;
  assert(UnovaAdventure::recordChampionsVictory(unova));
  assert(KalosAdventure::available(unova));
  assert(KalosAdventure::reconcile(kalos, unova));
  assert(kalos.unlockedAreas == 1U);

  PokemonCollection collection{};
  CollectionLogic::initialize(collection);
  OwnedPokemon starter = CollectionLogic::createPokemon(
      0U, 650U, 80U, false, 0x650U, 6U);
  uint32_t starterUid = 0U;
  assert(CollectionLogic::add(collection, starter, &starterUid));
  assert(CollectionLogic::setPartySlot(collection, 0U, starterUid));

  // Friend Safari is capturable and guarantees a Gen-VI Hidden Ability.
  bool foundSafari = false;
  for (uint32_t seed = 1U; seed < 5000U && !foundSafari; ++seed) {
    KalosAdventureState safari = kalos;
    assert(KalosAdventure::begin(
        safari, KalosArea::SantaluneForest, seed, collection));
    for (uint8_t step = 0U; step < KalosAdventure::kStepsPerArea; ++step) {
      safari.run.step = step;
      KalosRoomChoice choices[2]{};
      if (KalosAdventure::choices(safari, choices) != 2U ||
          choices[1].kind != KalosRoomKind::FriendSafari) continue;
      const KalosRoomResult room = KalosAdventure::choose(
          safari, 1U, collection);
      assert(room.startsBattle && room.hiddenAbilityEncounter);
      BattleState battle{};
      assert(KalosAdventure::startPendingBattle(
          safari, battle, collection, starterUid));
      assert(battle.kind == BattleKind::Wild && battle.opponentCount == 1U);
      assert(battle.opponents[0].speciesId >= 650U &&
             battle.opponents[0].speciesId <= 715U);
      assert(CollectionLogic::hasHiddenAbility(battle.opponents[0]));
      foundSafari = true;
      break;
    }
  }
  assert(foundSafari);

  for (uint8_t area = 0U; area < KalosAdventure::kAreaCount; ++area) {
    assert(KalosAdventure::isAreaUnlocked(
        kalos, static_cast<KalosArea>(area)));
    completeArea(kalos, static_cast<KalosArea>(area), collection, starterUid);
  }
  assert(KalosAdventure::championCompleted(kalos));
  assert(KalosAdventure::xThemeUnlocked(kalos));
  assert(!KalosAdventure::xThemeSelected(kalos));
  assert(KalosAdventure::setXThemeSelected(kalos, true));
  assert(KalosAdventure::xThemeSelected(kalos));
  assert(KalosAdventure::setXThemeSelected(kalos, false));
  assert(!KalosAdventure::xThemeSelected(kalos));
  assert(KalosAdventure::badgeCount(kalos) == 8U);
  assert((kalos.defeatedBosses & KalosAdventure::kAllAreasMask) ==
         KalosAdventure::kAllAreasMask);
  assert(KalosAdventure::fullyCompleted(kalos));
  Inventory kalosRewardInventory{};
  assert(KalosAdventure::reconcileCompletionReward(
      kalos, kalosRewardInventory));
  assert(kalosRewardInventory.balls[
             static_cast<uint8_t>(PokeBallType::MasterBall)] == 1U);
  assert(!KalosAdventure::reconcileCompletionReward(
      kalos, kalosRewardInventory));
  KalosAdventureState pendingKalosReward = kalos;
  pendingKalosReward.flags = static_cast<uint8_t>(
      pendingKalosReward.flags & ~KalosAdventure::kMasterBallRewardBit);
  Inventory fullKalosInventory{};
  fullKalosInventory.balls[
      static_cast<uint8_t>(PokeBallType::MasterBall)] = kInventoryStackLimit;
  assert(!KalosAdventure::reconcileCompletionReward(
      pendingKalosReward, fullKalosInventory));
  fullKalosInventory.balls[
      static_cast<uint8_t>(PokeBallType::MasterBall)] = 0U;
  assert(KalosAdventure::reconcileCompletionReward(
      pendingKalosReward, fullKalosInventory));

  // Numeric form order is shared by Collection and the generated SD paths.
  assert(CollectionLogic::formCount(666U) == 20U);
  assert(CollectionLogic::formCount(669U) == 5U);
  assert(CollectionLogic::formCount(670U) == 6U);
  assert(CollectionLogic::formCount(676U) == 10U);
  assert(CollectionLogic::formCount(710U) == 4U);
  assert(CollectionLogic::formCount(718U) == 3U);
  assert(CollectionLogic::formCount(720U) == 2U);
  assert(std::strcmp(CollectionLogic::formName(681U, 1U), "BLADE") == 0);
  assert(std::strcmp(CollectionLogic::formName(670U, 5U), "ETERNAL") == 0);
  assert(std::strcmp(CollectionLogic::formName(718U, 2U), "COMPLETE") == 0);
  assert(std::strcmp(CollectionLogic::formName(720U, 1U), "UNBOUND") == 0);

  OwnedPokemon zygardeTen = CollectionLogic::createPokemon(
      0U, 718U, 75U, false, 0x718U, 6U);
  OwnedPokemon zygardeComplete = zygardeTen;
  assert(CollectionLogic::resolvedForm(zygardeTen)==1U);
  assert(!CollectionLogic::canChangeForm(zygardeTen));
  assert(!CollectionLogic::setForm(zygardeTen,0U));
  zygardeComplete.form=2U;
  CollectionLogic::refreshDerivedStats(zygardeComplete,true);
  assert(CollectionLogic::calculatedMaximumHp(zygardeTen) <
         CollectionLogic::calculatedMaximumHp(zygardeComplete));
  assert(CollectionLogic::calculatedStat(zygardeTen, PokemonStat::Speed) >
         CollectionLogic::calculatedStat(zygardeComplete, PokemonStat::Speed));

  OwnedPokemon hoopa = CollectionLogic::createPokemon(
      0U, 720U, 75U, false, 0x720U, 6U);
  const uint16_t confinedAttack = CollectionLogic::calculatedStat(
      hoopa, PokemonStat::Attack);
  assert(!CollectionLogic::canChangeForm(hoopa));
  assert(!CollectionLogic::setForm(hoopa, 1U));
  assert(CollectionLogic::prisonBottleState(hoopa,100U)==
         PrisonBottleState::Ready);
  assert(CollectionLogic::usePrisonBottle(hoopa,100U));
  assert(CollectionLogic::calculatedStat(hoopa, PokemonStat::Attack) >
         confinedAttack);
  assert(MegaEvolution::type2(hoopa, PokemonType::Ghost) == PokemonType::Dark);

  PokemonCollection hoopaCollection{};
  CollectionLogic::initialize(hoopaCollection);
  uint32_t hoopaUid=0U;
  assert(CollectionLogic::add(hoopaCollection,hoopa,&hoopaUid));
  assert(CollectionLogic::setPartySlot(hoopaCollection,0U,hoopaUid));
  assert(!CollectionLogic::reconcileFormLifecycles(
      hoopaCollection,171U,false));
  assert(CollectionLogic::prisonBottleDaysRemaining(
      *CollectionLogic::find(hoopaCollection,hoopaUid),171U)==1U);
  assert(CollectionLogic::reconcileFormLifecycles(
      hoopaCollection,172U,false));
  OwnedPokemon* storedHoopa=CollectionLogic::find(hoopaCollection,hoopaUid);
  assert(storedHoopa&&storedHoopa->form==0U&&
         CollectionLogic::prisonBottleState(*storedHoopa,172U)==
             PrisonBottleState::Cooldown&&
         CollectionLogic::prisonBottleDaysRemaining(*storedHoopa,172U)==3U);
  assert(CollectionLogic::reconcileFormLifecycles(
      hoopaCollection,244U,false));
  assert(CollectionLogic::prisonBottleState(*storedHoopa,244U)==
         PrisonBottleState::Ready);

  // A six-day offline jump consumes both the active window and cooldown; it
  // must not start a fresh three-day cooldown when the save is reopened.
  assert(CollectionLogic::usePrisonBottle(*storedHoopa,300U));
  assert(CollectionLogic::reconcileFormLifecycles(
      hoopaCollection,444U,false));
  assert(storedHoopa->form==0U&&
         CollectionLogic::prisonBottleState(*storedHoopa,444U)==
             PrisonBottleState::Ready);

  // The Bottle form is timed, but every move surface follows the active form.
  // Confined keeps Hyperspace Hole; Unbound exposes
  // its complete ORAS table and Hyperspace Fury instead.
  const MoveId hyperspaceHole=static_cast<MoveId>(593U);
  const MoveId hyperspaceFury=static_cast<MoveId>(621U);
  MoveId confinedPool[64]{},unboundPool[64]{};
  const uint8_t confinedCount=relearnableMovesForForm(
      720U,0U,100U,confinedPool,64U,6U);
  const uint8_t unboundCount=relearnableMovesForForm(
      720U,1U,100U,unboundPool,64U,6U);
  const auto containsMove=[](const MoveId* moves,uint8_t count,MoveId wanted){
    for(uint8_t i=0U;i<count;++i)if(moves[i]==wanted)return true;
    return false;
  };
  assert(containsMove(confinedPool,confinedCount,hyperspaceHole));
  assert(!containsMove(confinedPool,confinedCount,hyperspaceFury));
  assert(containsMove(unboundPool,unboundCount,hyperspaceFury));
  assert(!containsMove(unboundPool,unboundCount,hyperspaceHole));
  assert(containsMove(unboundPool,unboundCount,static_cast<MoveId>(282U)));
  assert(containsMove(unboundPool,unboundCount,static_cast<MoveId>(399U)));
  assert(!containsMove(unboundPool,unboundCount,static_cast<MoveId>(566U)));
  assert(!containsMove(unboundPool,unboundCount,static_cast<MoveId>(247U)));
  assert(moveLearnedAtLevelForForm(720U,1U,46U,6U)==static_cast<MoveId>(282U));
  assert(moveLearnedAtLevelForForm(720U,1U,55U,6U)==static_cast<MoveId>(399U));
  assert(moveLearnedAtLevelForForm(720U,1U,85U,6U)==hyperspaceFury);
  const FullMoveData* fury=findFullMove(hyperspaceFury);
  assert(fury&&fury->power==100U&&fury->accuracy==101U&&
         fury->category==MoveCategory::Physical&&!fury->protectAffected);
  assert(std::strstr(moveDescription(hyperspaceFury),"DEFENSE"));
  assert(findBattleAnimation(621U)&&findBattleAnimationProgram(621U));

  // Every real form-specific learnset exception must be visible from the
  // same Summary surface. Eternal Floette adds Light of Ruin; the ORAS
  // legends expose their four post-X/Y signatures; non-Mega Zygarde learns
  // Core Enforcer as the source move for its Mega conversion.
  MoveId eternalPool[64]{},kyogrePool[64]{},groudonPool[64]{},rayquazaPool[64]{};
  MoveId zygardePool[64]{};
  const uint8_t eternalCount=relearnableMovesForForm(
      670U,5U,100U,eternalPool,64U,6U);
  const uint8_t kyogreCount=relearnableMovesForForm(
      382U,0U,100U,kyogrePool,64U,6U);
  const uint8_t groudonCount=relearnableMovesForForm(
      383U,0U,100U,groudonPool,64U,6U);
  const uint8_t rayquazaCount=relearnableMovesForForm(
      384U,0U,100U,rayquazaPool,64U,6U);
  const uint8_t zygardeCount=relearnableMovesForForm(
      718U,0U,100U,zygardePool,64U,6U);
  assert(containsMove(eternalPool,eternalCount,static_cast<MoveId>(617U)));
  assert(moveLearnedAtLevelForForm(670U,5U,50U,6U)==static_cast<MoveId>(617U));
  assert(containsMove(kyogrePool,kyogreCount,static_cast<MoveId>(618U)));
  assert(containsMove(groudonPool,groudonCount,static_cast<MoveId>(619U)));
  assert(containsMove(rayquazaPool,rayquazaCount,static_cast<MoveId>(620U)));
  assert(containsMove(zygardePool,zygardeCount,static_cast<MoveId>(687U)));
  assert(MegaEvolution::canTransform(384U)&&
         !MegaEvolution::canHoldMegaStone(384U));
  assert(MegaEvolution::canHoldMegaStone(382U)&&
         MegaEvolution::canHoldMegaStone(383U));

  PokemonCollection rayCollection{};
  CollectionLogic::initialize(rayCollection);
  OwnedPokemon rayquaza=CollectionLogic::createPokemon(
      0U,384U,80U,false,0x384U,6U);
  rayquaza.moves[0]=static_cast<MoveId>(620U);
  rayquaza.movePp[0]=findFullMove(rayquaza.moves[0])->pp;
  uint32_t rayUid=0U;
  assert(CollectionLogic::add(rayCollection,rayquaza,&rayUid));
  assert(CollectionLogic::setPartySlot(rayCollection,0U,rayUid));
  BattleState rayBattle{};BattleEngine::clear(rayBattle);
  rayBattle.active=true;rayBattle.kind=BattleKind::Pvp;
  rayBattle.outcome=BattleOutcome::Ongoing;rayBattle.playerUid=rayUid;
  rayBattle.opponentCount=1U;rayBattle.unlockedGeneration=6U;
  rayBattle.rngState=0x384620U;
  rayBattle.opponents[0]=CollectionLogic::createPokemon(
      0U,242U,100U,false,0x242U,6U);
  rayBattle.opponents[0].maximumHp=60000U;
  rayBattle.opponents[0].currentHp=60000U;
  rayBattle.opponents[0].moves[0]=static_cast<MoveId>(150U);
  rayBattle.opponents[0].movePp[0]=40U;
  const BattleActionResult ascent=BattleEngine::fightPvp(
      rayBattle,rayCollection,0U,0U);
  OwnedPokemon* megaRay=CollectionLogic::find(rayCollection,rayUid);
  assert(ascent.accepted&&megaRay&&
         MegaEvolution::variantFor(*megaRay)==MegaVariant::Mega&&
         rayBattle.weather==BattleWeather::StrongWinds);
  bool sawRayFormChange=false;
  for(uint8_t event=0U;event<ascent.eventCount;++event)
    if(ascent.events[event].type==BattleEventType::MoveEffect&&
       ascent.events[event].value==static_cast<uint16_t>(
           BattleMoveEffect::FormChanged))sawRayFormChange=true;
  assert(sawRayFormChange);
  assert(MegaEvolution::resetRayquazaBattleMega(*megaRay)&&
         MegaEvolution::variantFor(*megaRay)==MegaVariant::None);
  for(uint16_t move:{618U,619U,620U,621U,687U,920U}){
    assert(findFullMove(static_cast<MoveId>(move)));
    assert(findBattleAnimation(move));
    assert(findBattleAnimationProgram(move));
  }
  const FullMoveData* nihil=findFullMove(static_cast<MoveId>(920U));
  assert(nihil&&nihil->power==200U&&nihil->accuracy==100U&&nihil->pp==10U&&
         nihil->category==MoveCategory::Special&&!nihil->protectAffected);
  assert(std::strstr(moveDescription(static_cast<MoveId>(920U)),"FAIRY"));

  // Ordinary Mega Evolution does not invent a separate learnset. Audit the
  // complete authored Mega catalog and reserve the one canonical exception:
  // Mega Zygarde converts Core Enforcer into Nihil Light.
  for(uint8_t index=0U;index<MegaEvolution::officialFormCount();++index){
    const MegaFormData* mega=MegaEvolution::officialForm(index);assert(mega);
    OwnedPokemon candidate=CollectionLogic::createPokemon(
        0U,mega->speciesId,100U,false,0x6A000000U+index,6U);
    if(candidate.speciesId==718U){
      candidate.form=2U;
      CollectionLogic::refreshDerivedStats(candidate,true);
    }
    assert(MegaEvolution::setVariantChoice(candidate,mega->variant));
    candidate.heldItem=HeldItem::MegaStone;
    MoveId basePool[64]{},megaPool[64]{};
    const uint8_t baseCount=relearnableMovesForForm(
        candidate.speciesId,CollectionLogic::resolvedForm(candidate),
        candidate.level,basePool,64U,6U);
    const uint8_t megaCount=relearnableMovesForPokemon(
        candidate,megaPool,64U,6U);
    assert(baseCount==megaCount);
    if(candidate.speciesId==718U){
      assert(containsMove(basePool,baseCount,static_cast<MoveId>(687U)));
      assert(!containsMove(basePool,baseCount,static_cast<MoveId>(920U)));
      assert(!containsMove(megaPool,megaCount,static_cast<MoveId>(687U)));
      assert(containsMove(megaPool,megaCount,static_cast<MoveId>(920U)));
    }else{
      for(uint8_t move=0U;move<baseCount;++move)assert(basePool[move]==megaPool[move]);
    }
  }

  OwnedPokemon megaZygarde=CollectionLogic::createPokemon(
      0U,718U,100U,false,0x718920U,6U);
  megaZygarde.moves[0]=static_cast<MoveId>(687U);
  megaZygarde.movePp[0]=10U;
  assert(CollectionLogic::setHeldItemQuantity(
      megaZygarde,HeldItem::MegaStone,1U));
  assert(CollectionLogic::resolvedForm(megaZygarde)==1U);
  assert(MegaEvolution::variantFor(megaZygarde)==MegaVariant::None);
  assert(megaZygarde.moves[0]==static_cast<MoveId>(687U));
  megaZygarde.form=2U;
  assert(MegaEvolution::synchronizeSignatureMove(megaZygarde));
  assert(MegaEvolution::variantFor(megaZygarde)==MegaVariant::Mega);
  assert(megaZygarde.moves[0]==static_cast<MoveId>(920U));
  assert(CollectionLogic::setHeldItemQuantity(
      megaZygarde,HeldItem::None,0U));
  assert(megaZygarde.moves[0]==static_cast<MoveId>(687U));

  // Zygarde assembles exactly one stage per completed living turn. The
  // universal Mega Stone is legal in 10%/50%, but activates only when the
  // second threshold reaches Complete; that exact transition also swaps the
  // signature move and journals the maximum-HP increase after the sprite.
  const auto exerciseZygardeAssembly=[](bool withMegaStone){
    PokemonCollection owned{};
    OwnedPokemon zygarde=CollectionLogic::createPokemon(
        0U,718U,100U,false,withMegaStone?0x718A5U:0x71805U,6U);
    zygarde.moves[0]=static_cast<MoveId>(150U); // SPLASH
    zygarde.movePp[0]=40U;
    zygarde.moves[1]=static_cast<MoveId>(687U); // CORE ENFORCER
    zygarde.movePp[1]=10U;
    if(withMegaStone)
      assert(CollectionLogic::setHeldItemQuantity(
          zygarde,HeldItem::MegaStone,1U));
    uint32_t zygardeUid=0U;
    assert(CollectionLogic::add(owned,zygarde,&zygardeUid));
    owned.party[0]=zygardeUid;
    uint32_t benchUid=0U;
    assert(CollectionLogic::add(owned,CollectionLogic::createPokemon(
        0U,1U,100U,false,0x100718U,6U),&benchUid));
    owned.party[1]=benchUid;

    BattleState battle{};
    battle.active=true;
    battle.kind=BattleKind::Wild;
    battle.outcome=BattleOutcome::Ongoing;
    battle.unlockedGeneration=6U;
    battle.playerUid=zygardeUid;
    battle.opponentCount=1U;
    battle.opponents[0]=CollectionLogic::createPokemon(
        0U,129U,100U,false,0x129150U,6U);
    for(uint8_t slot=0U;slot<kMoveSlots;++slot){
      battle.opponents[0].moves[slot]=MoveId::None;
      battle.opponents[0].movePp[slot]=0U;
    }
    battle.opponents[0].moves[0]=static_cast<MoveId>(150U);
    battle.opponents[0].movePp[0]=40U;
    battle.rngState=0x71850000U+(withMegaStone?1U:0U);
    assert(BattleEngine::ensureOpponentUids(battle));
    BattleEngine::applyEntryAbilities(battle,owned);

    OwnedPokemon* active=CollectionLogic::find(owned,zygardeUid);
    assert(active&&CollectionLogic::resolvedForm(*active)==1U);
    assert(MegaEvolution::variantFor(*active)==MegaVariant::None);
    assert(active->moves[1]==static_cast<MoveId>(687U));

    const auto assertTransition=[](const BattleActionResult& result,
                                   uint8_t before,uint8_t after){
      int16_t formIndex=-1,hpIndex=-1;
      uint8_t formEvents=0U;
      for(uint8_t index=0U;index<result.eventCount;++index){
        const BattleEvent& event=result.events[index];
        if(event.type==BattleEventType::MoveEffect&&
           event.value==static_cast<uint16_t>(BattleMoveEffect::FormChanged)){
          ++formEvents;
          formIndex=index;
          assert(event.before==before&&event.after==after);
        }
        if(event.type==BattleEventType::HpChanged&&
           event.side==BattleSide::Player&&event.after>event.before)
          hpIndex=index;
      }
      assert(formEvents==1U&&formIndex>=0&&hpIndex>formIndex);
    };

    active->currentHp=static_cast<uint16_t>(
        static_cast<uint32_t>(active->maximumHp)*9U/10U+1U);
    BattleActionResult aboveNinety=BattleEngine::fight(battle,owned,0U);
    assert(aboveNinety.accepted&&CollectionLogic::resolvedForm(*active)==1U);

    // Even at half HP, the first qualifying turn may advance only 10% -> 50%.
    active->currentHp=static_cast<uint16_t>(active->maximumHp/2U);
    BattleActionResult first=BattleEngine::fight(battle,owned,0U);
    assert(first.accepted&&battle.active);
    assert(CollectionLogic::resolvedForm(*active)==0U);
    assert(MegaEvolution::variantFor(*active)==MegaVariant::None);
    assert(active->moves[1]==static_cast<MoveId>(687U));
    assertTransition(first,1U,0U);

    active->currentHp=static_cast<uint16_t>(active->maximumHp/2U+1U);
    BattleActionResult aboveHalf=BattleEngine::fight(battle,owned,0U);
    assert(aboveHalf.accepted&&CollectionLogic::resolvedForm(*active)==0U);

    active->currentHp=static_cast<uint16_t>(active->maximumHp/2U);
    BattleActionResult second=BattleEngine::fight(battle,owned,0U);
    assert(second.accepted&&battle.active);
    assert(CollectionLogic::resolvedForm(*active)==2U);
    assertTransition(second,0U,2U);
    if(withMegaStone){
      assert(MegaEvolution::variantFor(*active)==MegaVariant::Mega);
      assert(active->moves[1]==static_cast<MoveId>(920U));
    }else{
      assert(MegaEvolution::variantFor(*active)==MegaVariant::None);
      assert(active->moves[1]==static_cast<MoveId>(687U));
    }
    // Leaving battle restores 10% Forme without converting a wounded
    // Complete Zygarde into an artificial 1-HP survivor. Preserve its HP
    // percentage across the smaller maximum-HP total.
    active->currentHp=static_cast<uint16_t>(
        static_cast<uint32_t>(active->maximumHp)*37U/100U);
    const uint16_t hpBeforeReset=active->currentHp;
    const uint16_t maximumBeforeReset=active->maximumHp;
    OwnedPokemon tenPercent=*active;
    tenPercent.form=1U;
    assert(MegaEvolution::synchronizeSignatureMove(tenPercent)==withMegaStone);
    const uint16_t expectedMaximum=
        CollectionLogic::calculatedMaximumHp(tenPercent);
    const uint16_t expectedHp=static_cast<uint16_t>(
        (static_cast<uint32_t>(hpBeforeReset)*expectedMaximum+
         maximumBeforeReset/2U)/maximumBeforeReset);
    const BattleActionResult switched=BattleEngine::switchToPokemon(
        battle,owned,benchUid,true);
    assert(switched.accepted);
    assert(CollectionLogic::resolvedForm(*active)==1U);
    assert(active->maximumHp==expectedMaximum);
    assert(active->currentHp==expectedHp);
    assert(active->currentHp>1U);
    assert(MegaEvolution::variantFor(*active)==MegaVariant::None);
    assert(active->moves[1]==static_cast<MoveId>(687U));
  };
  exerciseZygardeAssembly(false);
  exerciseZygardeAssembly(true);

  static constexpr uint16_t kKalosMegas[] = {
      652U, 655U, 658U, 668U, 670U, 678U,
      687U, 689U, 691U, 701U, 718U, 719U};
  for (uint16_t species : kKalosMegas) {
    assert(MegaEvolution::canTransform(species));
    assert(MegaEvolution::variantChoiceCount(species) == 1U);
    const MegaFormData* form = MegaEvolution::formData(
        species, MegaVariant::Mega);
    assert(form && std::strcmp(form->assetVariant, "mega") == 0);
  }
  OwnedPokemon megaGreninja = CollectionLogic::createPokemon(
      0U, 658U, 75U, false, 0x658U, 6U);
  megaGreninja.heldItem = HeldItem::MegaStone;
  assert(MegaEvolution::variantFor(megaGreninja) == MegaVariant::Mega);
  assert(MegaEvolution::baseStat(megaGreninja, 2U, 122U) == 142U);
  assert(MegaEvolution::abilityId(megaGreninja, megaGreninja.abilityId) == 169U);
  OwnedPokemon femaleMeowstic{};
  for (uint32_t personality = 0U; personality < 256U; ++personality) {
    femaleMeowstic = CollectionLogic::createPokemon(
        0U, 678U, 50U, false, personality, 6U);
    if (CollectionLogic::resolvedForm(femaleMeowstic) == 1U) break;
  }
  assert(CollectionLogic::resolvedForm(femaleMeowstic) == 1U);
  assert(CollectionLogic::setHiddenAbility(femaleMeowstic, true, 6U));
  const AbilityData* ability = findAbility(femaleMeowstic.abilityId);
  assert(ability && std::strcmp(ability->name, "COMPETITIVE") == 0);

  OwnedPokemon smallPumpkaboo = CollectionLogic::createPokemon(
      0U, 710U, 50U, false, 710U, 6U);
  smallPumpkaboo.form = 1U;
  OwnedPokemon superPumpkaboo = smallPumpkaboo;
  superPumpkaboo.form = 3U;
  assert(CollectionLogic::calculatedMaximumHp(smallPumpkaboo) <
         CollectionLogic::calculatedMaximumHp(superPumpkaboo));

  // Hardware-, trade-, weather-, item- and friendship-gated X/Y evolutions
  // use Pokegochi's established deterministic level adaptation. None may be
  // lost merely because the original trigger has no equivalent on ESP32.
  uint16_t evolutions[kEvolutionChoiceCapacity]{};
  assert(evolutionChoices(650U,15U,1U,1U,6U,evolutions,
                          kEvolutionChoiceCapacity)==0U);
  assert(evolutionChoices(650U,16U,1U,1U,6U,evolutions,
                          kEvolutionChoiceCapacity)==1U&&evolutions[0]==651U);
  assert(evolutionChoices(674U,32U,1U,1U,6U,evolutions,
                          kEvolutionChoiceCapacity)==1U&&evolutions[0]==675U);
  assert(evolutionChoices(686U,30U,1U,1U,6U,evolutions,
                          kEvolutionChoiceCapacity)==1U&&evolutions[0]==687U);
  assert(evolutionChoices(705U,50U,1U,1U,6U,evolutions,
                          kEvolutionChoiceCapacity)==1U&&evolutions[0]==706U);
  assert(evolutionChoices(710U,40U,1U,1U,6U,evolutions,
                          kEvolutionChoiceCapacity)==1U&&evolutions[0]==711U);
  assert(evolutionChoices(680U,36U,1U,1U,6U,evolutions,
                          kEvolutionChoiceCapacity)==1U&&evolutions[0]==681U);
  assert(evolutionChoices(682U,40U,1U,1U,6U,evolutions,
                          kEvolutionChoiceCapacity)==1U&&evolutions[0]==683U);
  uint8_t eeveeChoices=evolutionChoices(133U,30U,1U,1U,5U,evolutions,
                                        kEvolutionChoiceCapacity);
  bool sawSylveon=false;
  for(uint8_t i=0U;i<eeveeChoices;++i)sawSylveon|=evolutions[i]==700U;
  assert(!sawSylveon);
  eeveeChoices=evolutionChoices(133U,30U,1U,1U,6U,evolutions,
                                kEvolutionChoiceCapacity);
  sawSylveon=false;
  for(uint8_t i=0U;i<eeveeChoices;++i)sawSylveon|=evolutions[i]==700U;
  assert(sawSylveon);
  return 0;
}
