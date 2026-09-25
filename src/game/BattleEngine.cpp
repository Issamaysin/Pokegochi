#include "game/BattleEngine.h"
#include "game/WorldFeatures.h"
#include "game/MegaEvolution.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "game/TrainerData.h"
#include "game/Pokedex.h"

namespace {
bool gPermanentExperienceBoost=false;
bool abilityIs(const OwnedPokemon& pokemon,const char* name);
// Shiny encounters use FireRed's original 1/8192 roll. The temporary 50%
// visual-QA override has been retired now that the shiny presentation was
// validated on hardware.
constexpr bool kWildShinyQaMode = false;

// Production progression follows FireRed's unmodified experience award.
constexpr uint8_t kWildExperienceMultiplier = 1U;
// Give the opening party enough momentum to establish a usable roster.  The
// bonus is shared exactly like ordinary Pokegochi EXP and ends as soon as the
// strongest active partner reaches Lv.10.
constexpr uint8_t kEarlyExperienceLevelLimit = 10U;
constexpr uint8_t kEarlyExperienceMultiplier = 2U;

// The battle resolver is single-threaded. This scoped override lets PvP feed
// the peer's exact move into the existing, heavily tested FireRed turn path
// without duplicating hundreds of move/ability branches. -1 selects trainer
// AI; 0xFE represents a peer switch and suppresses the opponent action.
int16_t gPvpOpponentMoveOverride = -1;
bool gPresentHealRoll = false;
uint16_t gLastCalculatedMovePower = 0;
bool gPursuitSwitchBoost = false;
BattleEvent& appendEvent(BattleActionResult& result, BattleEventType type, BattleSide side,
                         uint32_t pokemonUid = 0) {
  // Keep the final slot usable even if an unusually effect-heavy move emits
  // more presentation details than the handheld UI can reasonably display.
  const uint8_t index = result.eventCount < kBattleEventCapacity
      ? result.eventCount++ : static_cast<uint8_t>(kBattleEventCapacity - 1U);
  BattleEvent& event = result.events[index];
  event = BattleEvent{};
  event.type = type;
  event.side = side;
  event.pokemonUid = pokemonUid;
  return event;
}

void appendFaintedOnce(BattleActionResult& result, BattleSide side, OwnedPokemon& pokemon) {
  // Pokegochi treats fainting as clearing every persistent major condition.
  // The incapacity latch may keep the Pokemon unusable until full recovery,
  // but poison/burn/etc. must never survive into its eventual revival.
  pokemon.status = StatusCondition::None;
  // Several turn phases can observe the same zero HP (direct hit, then
  // residuals, delayed attacks and end-of-turn held items). Presentation
  // must receive exactly one faint event for that Pokemon/action.
  for (uint8_t index = 0; index < result.eventCount; ++index)
    if (result.events[index].type == BattleEventType::Fainted &&
        result.events[index].side == side && result.events[index].pokemonUid == pokemon.uid)
      return;
  appendEvent(result, BattleEventType::Fainted, side, pokemon.uid);
}

void appendStatusChange(BattleActionResult& result, BattleSide side, uint32_t uid,
                        StatusCondition before, StatusCondition after) {
  if (before == after) return;
  BattleEvent& event = appendEvent(result,
      after == StatusCondition::None ? BattleEventType::StatusCured : BattleEventType::StatusApplied,
      side, uid);
  // StatusCured also retains the former condition so the presentation layer
  // can reconstruct the state that existed before this event.
  event.status = after == StatusCondition::None ? before : after;
}

void appendHpChange(BattleActionResult& result, BattleSide side, uint32_t uid,
                    uint16_t before, uint16_t after) {
  if (before == after) return;
  BattleEvent& event = appendEvent(result, BattleEventType::HpChanged, side, uid);
  event.before = before;
  event.after = after;
  event.value = before > after ? static_cast<uint16_t>(before - after)
                               : static_cast<uint16_t>(after - before);
}

void appendAbilityActivation(BattleActionResult& result, BattleSide side,
                             const OwnedPokemon& pokemon) {
  if (!pokemon.abilityId) return;
  BattleEvent& event = appendEvent(result, BattleEventType::AbilityActivated,
                                   side, pokemon.uid);
  event.value = pokemon.abilityId;
}

struct StageSnapshot { int8_t values[7]{}; };
bool escapePrevented(const CombatVolatile& state);

struct VolatileFeedbackSnapshot {
  uint8_t confusionTurns = 0;
  uint8_t trappedTurns = 0;
  bool escapePrevented = false;
  bool seeded = false;
  bool protectedThisTurn = false;
  uint16_t substituteHp = 0;
  MoveId disabledMove = MoveId::None;
  MoveId encoreMove = MoveId::None;
};

VolatileFeedbackSnapshot snapshotVolatileFeedback(const CombatVolatile& state) {
  return {state.confusionTurns,state.trappedTurns,escapePrevented(state),state.seeded,
          state.protectedThisTurn, state.substituteHp, state.disabledMove,
          state.encoreMove};
}

void appendMoveEffect(BattleActionResult& result, BattleSide side, uint32_t uid,
                      MoveId move, BattleMoveEffect effect) {
  BattleEvent& event = appendEvent(result, BattleEventType::MoveEffect, side, uid);
  event.move = move;
  event.value = static_cast<uint16_t>(effect);
}

void appendWeatherEnded(BattleActionResult& result, uint32_t uid,
                        BattleWeather weather) {
  BattleEvent& event = appendEvent(result, BattleEventType::MoveEffect,
                                   BattleSide::Player, uid);
  event.value = static_cast<uint16_t>(BattleMoveEffect::WeatherEnded);
  // Weather is a field effect, so `before` identifies which original string
  // the presentation must use; side/name are intentionally ignored.
  event.before = static_cast<uint16_t>(weather);
  event.after = static_cast<uint16_t>(BattleWeather::Clear);
}

void appendWeatherContinues(BattleActionResult& result, uint32_t uid,
                            BattleWeather weather) {
  BattleEvent& event = appendEvent(result, BattleEventType::MoveEffect,
                                   BattleSide::Player, uid);
  event.value = static_cast<uint16_t>(BattleMoveEffect::WeatherContinues);
  event.before = static_cast<uint16_t>(weather);
  // FireRed's general weather animations reuse the move programs.
  event.move = weather == BattleWeather::Sandstorm
      ? static_cast<MoveId>(201) : static_cast<MoveId>(258);
}

void appendWeatherHurt(BattleActionResult& result, BattleSide side,
                       uint32_t uid, BattleWeather weather) {
  BattleEvent& event = appendEvent(result, BattleEventType::MoveEffect, side, uid);
  event.value = static_cast<uint16_t>(BattleMoveEffect::WeatherHurt);
  event.before = static_cast<uint16_t>(weather);
}

void appendVolatileFeedback(BattleActionResult& result, BattleSide side, uint32_t uid,
                            MoveId move, const VolatileFeedbackSnapshot& before,
                            const CombatVolatile& after) {
  if (!before.confusionTurns && after.confusionTurns)
    appendMoveEffect(result, side, uid, move, BattleMoveEffect::Confused);
  if (!before.seeded && after.seeded)
    appendMoveEffect(result, side, uid, move, BattleMoveEffect::Seeded);
  if (!before.trappedTurns && after.trappedTurns)
    appendMoveEffect(result, side, uid, move, BattleMoveEffect::Trapped);
  if(!before.escapePrevented&&escapePrevented(after))
    appendMoveEffect(result,side,uid,move,BattleMoveEffect::Trapped);
  if (!before.protectedThisTurn && after.protectedThisTurn)
    appendMoveEffect(result, side, uid, move, BattleMoveEffect::Protected);
  if (!before.substituteHp && after.substituteHp)
    appendMoveEffect(result, side, uid, move, BattleMoveEffect::Substitute);
  if (before.disabledMove == MoveId::None && after.disabledMove != MoveId::None)
    appendMoveEffect(result, side, uid, move, BattleMoveEffect::Disabled);
  if (before.encoreMove == MoveId::None && after.encoreMove != MoveId::None)
    appendMoveEffect(result, side, uid, move, BattleMoveEffect::Encore);
}

uint8_t moveSlotFor(const OwnedPokemon& pokemon, MoveId move, bool requirePp = true) {
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot)
    if (pokemon.moves[slot] == move && (!requirePp || pokemon.movePp[slot])) return slot;
  return kMoveSlots;
}

bool canEncoreMove(MoveId move) {
  // FireRed's Cmd_trysetencore rejects these three last-move values.
  return move != MoveId::None && move != MoveId::Struggle &&
         move != static_cast<MoveId>(227) && // ENCORE
         move != static_cast<MoveId>(119);   // MIRROR MOVE
}

void clearEncore(CombatVolatile& state, uint8_t& turns) {
  state.encoreMove = MoveId::None;
  turns = 0;
}

void storeBideDamage(BideState& state, uint16_t damage) {
  if (!state.turns || !damage) return;
  state.damage = static_cast<uint16_t>(std::min<uint32_t>(
      65535U, static_cast<uint32_t>(state.damage) + damage));
}

void cancelMultiTurnMoves(DedicatedMoveEffectState& effects,
                          CombatVolatile& volatileState,BideState& bide,
                          BattleActionResult& result,BattleSide side,uint32_t uid) {
  // Exact local equivalent of FireRed's CancelMultiTurnMoves. It is shared by
  // every attack-canceller branch so Rollout/Rampage/Uproar/Bide/two-turn
  // moves cannot leave a stale forced command behind.
  const bool endedUproar=effects.uproar;
  effects.lockedMove=MoveId::None;
  effects.lockedMoveTurns=0;
  effects.rolloutCount=0;
  effects.uproar=false;
  effects.furyCutterCount=0;
  volatileState.chargingMove=MoveId::None;
  bide=BideState{};
  if(endedUproar)
    appendMoveEffect(result,side,uid,static_cast<MoveId>(253),
                     BattleMoveEffect::UproarEnded);
}

StageSnapshot snapshotStages(const CombatVolatile& state) {
  return {{state.attackStage, state.defenseStage, state.spAttackStage, state.spDefenseStage,
           state.speedStage, state.accuracyStage, state.evasionStage}};
}

void appendStageChanges(BattleActionResult& result, BattleSide side, uint32_t uid,
                        const StageSnapshot& before, const CombatVolatile& after) {
  const int8_t values[7] = {after.attackStage, after.defenseStage, after.spAttackStage,
      after.spDefenseStage, after.speedStage, after.accuracyStage, after.evasionStage};
  for (uint8_t stat = 0; stat < 7U; ++stat) {
    if (values[stat] == before.values[stat]) continue;
    BattleEvent& event = appendEvent(result, BattleEventType::StatChanged, side, uid);
    event.value = stat; event.stageBefore = before.values[stat]; event.stageAfter = values[stat];
  }
}

void appendBattleEnd(BattleActionResult& result, const BattleState& battle) {
  if (battle.active || battle.outcome == BattleOutcome::None || battle.outcome == BattleOutcome::Ongoing) return;
  if (result.eventCount && result.events[result.eventCount - 1U].type == BattleEventType::BattleEnded) return;
  BattleEvent& event = appendEvent(result, BattleEventType::BattleEnded, BattleSide::Player, battle.playerUid);
  event.value = static_cast<uint16_t>(battle.outcome);
}

uint32_t staged(uint32_t value, int8_t stage) {
  stage = std::max<int8_t>(kMinimumBattleStatStage,
      std::min<int8_t>(kMaximumBattleStatStage, stage));
  return stage >= 0 ? value * (2U + stage) / 2U : value * 2U / (2U - stage);
}

uint32_t accuracyStaged(uint32_t value,int16_t stage) {
  // Exact sAccuracyStageRatios table from FireRed. Accuracy and evasion use
  // thirds (33%..300%), unlike the ordinary battle stats' quarters
  // (25%..400%). The combined ACC-EVAS index is clamped before lookup.
  static constexpr uint16_t numerators[]{33U,36U,43U,50U,60U,75U,100U,
      133U,166U,200U,233U,266U,300U};
  stage=std::max<int16_t>(kMinimumBattleStatStage,
      std::min<int16_t>(kMaximumBattleStatStage,stage));
  return value*numerators[stage-kMinimumBattleStatStage]/100U;
}

constexpr uint8_t kSureHitOwnerMask=0x03U;
constexpr uint8_t kCursedMask=0x04U;
constexpr uint8_t kEscapePreventedMask=0x08U;
constexpr uint8_t kDisableTimerMask=0x70U;
constexpr uint8_t kDisableTimerShift=4U;
constexpr uint8_t kSureHitFreshMask=0x80U;
uint8_t sureHitOwner(const CombatVolatile& state){return state.sureHitTurns&kSureHitOwnerMask;}
void setSureHitOwner(CombatVolatile& state,uint8_t owner){
  state.sureHitTurns=static_cast<uint8_t>((state.sureHitTurns&~kSureHitOwnerMask)|
                                         (owner&kSureHitOwnerMask));
  if(!(owner&kSureHitOwnerMask))state.sureHitTurns&=~kSureHitFreshMask;
}
void armSureHit(CombatVolatile& state,uint8_t owner){
  setSureHitOwner(state,owner);
  state.sureHitTurns|=kSureHitFreshMask;
}
void copySureHit(const CombatVolatile& source,CombatVolatile& target){
  setSureHitOwner(target,sureHitOwner(source));
  if(source.sureHitTurns&kSureHitFreshMask)target.sureHitTurns|=kSureHitFreshMask;
}
void advanceSureHit(CombatVolatile& state){
  if(!sureHitOwner(state))return;
  if(state.sureHitTurns&kSureHitFreshMask)
    state.sureHitTurns&=~kSureHitFreshMask;
  else
    setSureHitOwner(state,0);
}
bool isCursed(const CombatVolatile& state){return (state.sureHitTurns&kCursedMask)!=0;}
void setCursed(CombatVolatile& state,bool value){
  if(value)state.sureHitTurns|=kCursedMask;else state.sureHitTurns&=~kCursedMask;
}
bool escapePrevented(const CombatVolatile& state){
  return (state.sureHitTurns&kEscapePreventedMask)!=0;
}
void setEscapePrevented(CombatVolatile& state,bool value){
  if(value)state.sureHitTurns|=kEscapePreventedMask;
  else state.sureHitTurns&=~kEscapePreventedMask;
}
uint8_t disableTurns(const CombatVolatile& state){
  return static_cast<uint8_t>((state.sureHitTurns&kDisableTimerMask)>>kDisableTimerShift);
}
void setDisableTurns(CombatVolatile& state,uint8_t turns){
  state.sureHitTurns=static_cast<uint8_t>((state.sureHitTurns&~kDisableTimerMask)|
      ((std::min<uint8_t>(7U,turns)<<kDisableTimerShift)&kDisableTimerMask));
}

uint16_t ballMultiplier100(PokeBallType ball) {
  switch (ball) {
    case PokeBallType::PokeBall: return 100;
    case PokeBallType::GreatBall: return 150;
    case PokeBallType::UltraBall: return 200;
    case PokeBallType::MasterBall: return 10000;
    case PokeBallType::Count: return 0;
  }
  return 0;
}

uint16_t typeMultiplier100(PokemonType attack, const SpeciesData& defender) {
  uint16_t value = 100;
  auto apply = [&](PokemonType type) {
    auto is = [&](PokemonType a, PokemonType d) { return attack == a && type == d; };
    if (is(PokemonType::Normal,PokemonType::Ghost) || is(PokemonType::Fighting,PokemonType::Ghost) ||
        is(PokemonType::Electric,PokemonType::Ground) || is(PokemonType::Ground,PokemonType::Flying) ||
        is(PokemonType::Psychic,PokemonType::Dark) || is(PokemonType::Ghost,PokemonType::Normal) ||
        is(PokemonType::Poison,PokemonType::Steel)||is(PokemonType::Dragon,PokemonType::Fairy)) { value = 0; return; }
    const bool super =
      is(PokemonType::Fire,PokemonType::Grass)||is(PokemonType::Fire,PokemonType::Ice)||is(PokemonType::Fire,PokemonType::Bug)||is(PokemonType::Fire,PokemonType::Steel)||
      is(PokemonType::Water,PokemonType::Fire)||is(PokemonType::Water,PokemonType::Ground)||is(PokemonType::Water,PokemonType::Rock)||
      is(PokemonType::Electric,PokemonType::Water)||is(PokemonType::Electric,PokemonType::Flying)||
      is(PokemonType::Grass,PokemonType::Water)||is(PokemonType::Grass,PokemonType::Ground)||is(PokemonType::Grass,PokemonType::Rock)||
      is(PokemonType::Ice,PokemonType::Grass)||is(PokemonType::Ice,PokemonType::Ground)||is(PokemonType::Ice,PokemonType::Flying)||is(PokemonType::Ice,PokemonType::Dragon)||
      is(PokemonType::Fighting,PokemonType::Normal)||is(PokemonType::Fighting,PokemonType::Ice)||is(PokemonType::Fighting,PokemonType::Rock)||is(PokemonType::Fighting,PokemonType::Dark)||is(PokemonType::Fighting,PokemonType::Steel)||
      is(PokemonType::Poison,PokemonType::Grass)||is(PokemonType::Ground,PokemonType::Fire)||is(PokemonType::Ground,PokemonType::Electric)||is(PokemonType::Ground,PokemonType::Poison)||is(PokemonType::Ground,PokemonType::Rock)||is(PokemonType::Ground,PokemonType::Steel)||
      is(PokemonType::Flying,PokemonType::Grass)||is(PokemonType::Flying,PokemonType::Fighting)||is(PokemonType::Flying,PokemonType::Bug)||
      is(PokemonType::Psychic,PokemonType::Fighting)||is(PokemonType::Psychic,PokemonType::Poison)||
      is(PokemonType::Bug,PokemonType::Grass)||is(PokemonType::Bug,PokemonType::Psychic)||is(PokemonType::Bug,PokemonType::Dark)||
      is(PokemonType::Rock,PokemonType::Fire)||is(PokemonType::Rock,PokemonType::Ice)||is(PokemonType::Rock,PokemonType::Flying)||is(PokemonType::Rock,PokemonType::Bug)||
      is(PokemonType::Ghost,PokemonType::Psychic)||is(PokemonType::Ghost,PokemonType::Ghost)||is(PokemonType::Dragon,PokemonType::Dragon)||is(PokemonType::Dark,PokemonType::Psychic)||is(PokemonType::Dark,PokemonType::Ghost)||
      is(PokemonType::Steel,PokemonType::Ice)||is(PokemonType::Steel,PokemonType::Rock)||is(PokemonType::Steel,PokemonType::Fairy)||
      is(PokemonType::Poison,PokemonType::Fairy)||is(PokemonType::Fairy,PokemonType::Fighting)||
      is(PokemonType::Fairy,PokemonType::Dragon)||is(PokemonType::Fairy,PokemonType::Dark);
    if (super) { value *= 2; return; }
    const bool resist = is(PokemonType::Normal,PokemonType::Rock)||is(PokemonType::Normal,PokemonType::Steel)||
      is(PokemonType::Fire,PokemonType::Fire)||is(PokemonType::Fire,PokemonType::Water)||is(PokemonType::Fire,PokemonType::Rock)||is(PokemonType::Fire,PokemonType::Dragon)||
      is(PokemonType::Water,PokemonType::Water)||is(PokemonType::Water,PokemonType::Grass)||is(PokemonType::Water,PokemonType::Dragon)||
      is(PokemonType::Electric,PokemonType::Electric)||is(PokemonType::Electric,PokemonType::Grass)||is(PokemonType::Electric,PokemonType::Dragon)||
      is(PokemonType::Grass,PokemonType::Fire)||is(PokemonType::Grass,PokemonType::Grass)||is(PokemonType::Grass,PokemonType::Poison)||is(PokemonType::Grass,PokemonType::Flying)||is(PokemonType::Grass,PokemonType::Bug)||is(PokemonType::Grass,PokemonType::Dragon)||is(PokemonType::Grass,PokemonType::Steel)||
      is(PokemonType::Ice,PokemonType::Fire)||is(PokemonType::Ice,PokemonType::Water)||is(PokemonType::Ice,PokemonType::Ice)||is(PokemonType::Ice,PokemonType::Steel)||
      is(PokemonType::Fighting,PokemonType::Poison)||is(PokemonType::Fighting,PokemonType::Flying)||is(PokemonType::Fighting,PokemonType::Psychic)||is(PokemonType::Fighting,PokemonType::Bug)||
      is(PokemonType::Poison,PokemonType::Poison)||is(PokemonType::Poison,PokemonType::Ground)||is(PokemonType::Poison,PokemonType::Rock)||is(PokemonType::Poison,PokemonType::Ghost)||
      is(PokemonType::Ground,PokemonType::Grass)||is(PokemonType::Ground,PokemonType::Bug)||is(PokemonType::Flying,PokemonType::Electric)||is(PokemonType::Flying,PokemonType::Rock)||is(PokemonType::Flying,PokemonType::Steel)||
      is(PokemonType::Psychic,PokemonType::Psychic)||is(PokemonType::Psychic,PokemonType::Steel)||is(PokemonType::Bug,PokemonType::Fire)||is(PokemonType::Bug,PokemonType::Fighting)||is(PokemonType::Bug,PokemonType::Poison)||is(PokemonType::Bug,PokemonType::Flying)||is(PokemonType::Bug,PokemonType::Ghost)||is(PokemonType::Bug,PokemonType::Steel)||
      is(PokemonType::Rock,PokemonType::Fighting)||is(PokemonType::Rock,PokemonType::Ground)||is(PokemonType::Rock,PokemonType::Steel)||is(PokemonType::Ghost,PokemonType::Dark)||is(PokemonType::Dragon,PokemonType::Steel)||is(PokemonType::Dark,PokemonType::Fighting)||is(PokemonType::Dark,PokemonType::Dark)||is(PokemonType::Dark,PokemonType::Steel)||
      is(PokemonType::Steel,PokemonType::Fire)||is(PokemonType::Steel,PokemonType::Water)||is(PokemonType::Steel,PokemonType::Electric)||is(PokemonType::Steel,PokemonType::Steel)||
      is(PokemonType::Fire,PokemonType::Fairy)||is(PokemonType::Poison,PokemonType::Fairy)||
      is(PokemonType::Steel,PokemonType::Fairy)||is(PokemonType::Fairy,PokemonType::Fire)||
      is(PokemonType::Fairy,PokemonType::Poison)||is(PokemonType::Fairy,PokemonType::Steel);
    if (resist) value /= 2;
  };
  apply(defender.type1);
  if (defender.type2 != defender.type1) apply(defender.type2);
  return value;
}

uint16_t typeMultiplier100(PokemonType attack, const OwnedPokemon& defender,
                           const DedicatedMoveEffectState& effects) {
  const SpeciesData* species=findSpecies(defender.speciesId);
  if(!species)return 100;
  if(!effects.typeOverrideActive){SpeciesData temporary=*species;temporary.type1=MegaEvolution::type1(defender,species->type1);temporary.type2=MegaEvolution::type2(defender,species->type2);return typeMultiplier100(attack,temporary);}
  SpeciesData temporary=*species;
  temporary.type1=effects.type1;
  temporary.type2=effects.type2;
  return typeMultiplier100(attack,temporary);
}

uint16_t identifiedTypeMultiplier100(PokemonType attack,
                                     const OwnedPokemon& defender,
                                     const DedicatedMoveEffectState& effects) {
  if (!effects.identified ||
      (attack != PokemonType::Normal && attack != PokemonType::Fighting))
    return typeMultiplier100(attack, defender, effects);

  const SpeciesData* species = findSpecies(defender.speciesId);
  if (!species) return 100U;
  const PokemonType type1 = effects.typeOverrideActive
      ? effects.type1 : MegaEvolution::type1(defender, species->type1);
  const PokemonType type2 = effects.typeOverrideActive
      ? effects.type2 : MegaEvolution::type2(defender, species->type2);
  uint16_t value = 100U;
  const auto applyIdentifiedType = [&](PokemonType type) {
    // Gen III's TYPE_FORESIGHT marker is immediately before the two Ghost
    // immunity rows. Identification skips only those rows; matchups already
    // processed for a second type must remain in the result.
    if (type == PokemonType::Ghost) return;
    SpeciesData oneType = *species;
    oneType.type1 = type;
    oneType.type2 = type;
    value = static_cast<uint16_t>(value * typeMultiplier100(attack, oneType) / 100U);
  };
  applyIdentifiedType(type1);
  if (type2 != type1) applyIdentifiedType(type2);
  return value;
}

bool hasBattleType(const OwnedPokemon& pokemon,const DedicatedMoveEffectState& effects,
                   PokemonType type){
  if(effects.typeOverrideActive)return effects.type1==type||effects.type2==type;
  const SpeciesData* species=findSpecies(pokemon.speciesId);
  return species&&(MegaEvolution::type1(pokemon,species->type1)==type||
                    MegaEvolution::type2(pokemon,species->type2)==type);
}

const TransformBattleSnapshot& transformSnapshotFor(const BattleState& battle,
                                                     const OwnedPokemon& pokemon){
  return pokemon.uid==battle.playerUid?battle.playerTransformSnapshot:
      battle.opponentTransformSnapshot;
}

const IndividualValues& battleIvs(const BattleState& battle,
                                  const OwnedPokemon& pokemon){
  const TransformBattleSnapshot& snapshot=transformSnapshotFor(battle,pokemon);
  return snapshot.active?snapshot.ivs:pokemon.ivs;
}

uint16_t battleCalculatedStat(const BattleState& battle,const OwnedPokemon& pokemon,
                              PokemonStat stat){
  const TransformBattleSnapshot& snapshot=transformSnapshotFor(battle,pokemon);
  if(!snapshot.active)return CollectionLogic::calculatedStat(pokemon,stat);
  switch(stat){
    case PokemonStat::Attack:return snapshot.attack;
    case PokemonStat::Defense:return snapshot.defense;
    case PokemonStat::SpAttack:return snapshot.spAttack;
    case PokemonStat::SpDefense:return snapshot.spDefense;
    case PokemonStat::Speed:return snapshot.speed;
    default:return CollectionLogic::calculatedStat(pokemon,stat);
  }
}

PokemonType hiddenPowerType(const BattleState& battle,const OwnedPokemon& pokemon){
  const IndividualValues& ivs=battleIvs(battle,pokemon);
  const uint8_t bits=(ivs.hp&1U)|((ivs.attack&1U)<<1U)|
      ((ivs.defense&1U)<<2U)|((ivs.speed&1U)<<3U)|
      ((ivs.spAttack&1U)<<4U)|((ivs.spDefense&1U)<<5U);
  // FireRed's sixteen Hidden Power types are FIGHTING through DARK.
  return static_cast<PokemonType>(1U+(static_cast<uint16_t>(bits)*15U/63U));
}

uint8_t hiddenPowerPower(const BattleState& battle,const OwnedPokemon& pokemon){
  const IndividualValues& ivs=battleIvs(battle,pokemon);
  const uint8_t bits=((ivs.hp>>1U)&1U)|(((ivs.attack>>1U)&1U)<<1U)|
      (((ivs.defense>>1U)&1U)<<2U)|(((ivs.speed>>1U)&1U)<<3U)|
      (((ivs.spAttack>>1U)&1U)<<4U)|(((ivs.spDefense>>1U)&1U)<<5U);
  return static_cast<uint8_t>(30U+static_cast<uint16_t>(bits)*40U/63U);
}

PokemonType effectiveMoveType(const BattleState& battle,const FullMoveData& move,
                              const OwnedPokemon& attacker,
                              const OwnedPokemon* defender=nullptr){
  // FireRed's type-calculation commands return immediately for Struggle.
  // It is displayed as NORMAL, but is typeless for STAB, type matchups,
  // type-conversion Abilities and type-boosting items.
  if(move.id==static_cast<uint16_t>(MoveId::Struggle))return PokemonType::Normal;
  if(std::strcmp(move.effect,"HIDDEN_POWER")==0)return hiddenPowerType(battle,attacker);
  const bool suppressWeather=abilityIs(attacker,"CLOUD NINE")||
      abilityIs(attacker,"AIR LOCK")||(defender&&
      (abilityIs(*defender,"CLOUD NINE")||abilityIs(*defender,"AIR LOCK")));
  if(std::strcmp(move.effect,"WEATHER_BALL")==0&&!suppressWeather){
    if(battle.weather==BattleWeather::Rain||battle.weather==BattleWeather::HeavyRain)return PokemonType::Water;
    if(battle.weather==BattleWeather::Sun||battle.weather==BattleWeather::HarshSun)return PokemonType::Fire;
    if(battle.weather==BattleWeather::Sandstorm)return PokemonType::Rock;
    if(battle.weather==BattleWeather::Hail)return PokemonType::Ice;
  }
  if(move.type==PokemonType::Normal){
    if(abilityIs(attacker,"AERILATE"))return PokemonType::Flying;
    if(abilityIs(attacker,"PIXILATE"))return PokemonType::Fairy;
    if(abilityIs(attacker,"REFRIGERATE"))return PokemonType::Ice;
    if(abilityIs(attacker,"DRAGONIZE"))return PokemonType::Dragon;
  }
  return move.type;
}

bool isPhysicalType(PokemonType type){
  return type==PokemonType::Normal||type==PokemonType::Fighting||
      type==PokemonType::Flying||type==PokemonType::Poison||
      type==PokemonType::Ground||type==PokemonType::Rock||
      type==PokemonType::Bug||type==PokemonType::Ghost||type==PokemonType::Steel;
}

OwnedPokemon* firstHealthyPartyMember(PokemonCollection& collection, uint32_t excludedUid = 0) {
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) {
    OwnedPokemon* candidate = CollectionLogic::active(collection, slot);
    if (candidate && candidate->uid != excludedUid && candidate->currentHp > 0 && candidate->recoverySecondsRemaining == 0) return candidate;
  }
  return nullptr;
}

uint32_t moneyReward(const BattleState& battle, const PokemonCollection& collection);
bool abilityIs(const OwnedPokemon& pokemon,const char* name);
BattleSide sideOf(const BattleState& battle,const OwnedPokemon& pokemon);
void restoreOpponentTransform(BattleState& battle);

void applySpikesOnEntry(BattleState& battle,OwnedPokemon& pokemon,BattleSide side,
                        BattleActionResult& result){
  const uint8_t layers=side==BattleSide::Player?battle.playerSpikesLayers:
      battle.opponentSpikesLayers;
  if(!layers||!pokemon.currentHp)return;
  const SpeciesData* species=findSpecies(pokemon.speciesId);
  if(!species||MegaEvolution::type1(pokemon,species->type1)==PokemonType::Flying||
     MegaEvolution::type2(pokemon,species->type2)==PokemonType::Flying||
     abilityIs(pokemon,"LEVITATE"))return;
  const uint16_t divisor=layers==1U?8U:layers==2U?6U:4U;
  const uint16_t damage=std::max<uint16_t>(1U,pokemon.maximumHp/divisor);
  const uint16_t before=pokemon.currentHp;
  pokemon.currentHp=damage>=before?0U:static_cast<uint16_t>(before-damage);
  appendHpChange(result,side,pokemon.uid,before,pokemon.currentHp);
  if(!pokemon.currentHp)appendFaintedOnce(result,side,pokemon);
}

bool hasHealthyOpponentReplacement(const BattleState& battle) {
  for (uint8_t index = 0; index < battle.opponentCount; ++index) {
    if (index != battle.opponentIndex && battle.opponents[index].currentHp > 0) return true;
  }
  return false;
}

void applyPickupAfterBattle(BattleState& battle,PokemonCollection& collection){
  // Pickup is disabled in link battles.  It also checks the Pokemon's native
  // Ability, not a battle-local copy obtained through Trace/Role Play/Skill
  // Swap/Transform.
  if(battle.kind==BattleKind::Pvp)return;
  auto next=[&](){
    uint32_t x=battle.rngState?battle.rngState:0xA341316CU;
    x^=x<<13U;x^=x>>17U;x^=x<<5U;battle.rngState=x;return x;
  };
  for(uint8_t slot=0;slot<kPartyCapacity;++slot){
    OwnedPokemon* pokemon=CollectionLogic::find(collection,collection.party[slot]);
    if(!pokemon)continue;
    OwnedPokemon native=*pokemon;
    if(native.uid==battle.playerUid&&battle.playerTransformed){
      native.speciesId=battle.playerOriginalSpeciesId;
      native.form=battle.playerTransformSnapshot.originalForm;
    }
    CollectionLogic::refreshAbility(native);
    if(!abilityIs(native,"PICKUP")||pokemon->heldItem!=HeldItem::None||
       next()%10U)continue;
    const uint8_t roll=static_cast<uint8_t>(next()%100U);
    // The first 75% exactly follows FireRed. Its remaining rewards include
    // bag-only objects (TM10, PP Up, Rare Candy and Nugget), which cannot be
    // represented by OwnedPokemon::heldItem; map those bands to equivalent
    // supported held berries while preserving the original rarity curve.
    pokemon->heldItem=roll<15U?HeldItem::OranBerry:
        roll<25U?HeldItem::CheriBerry:roll<35U?HeldItem::ChestoBerry:
        roll<45U?HeldItem::PechaBerry:roll<55U?HeldItem::RawstBerry:
        roll<65U?HeldItem::AspearBerry:roll<75U?HeldItem::PersimBerry:
        roll<85U?HeldItem::LeppaBerry:roll<90U?HeldItem::SitrusBerry:
        roll<95U?HeldItem::LumBerry:roll<98U?HeldItem::LiechiBerry:
        roll<99U?HeldItem::PetayaBerry:HeldItem::StarfBerry;
  }
}

// Trainer battles advance through a fixed ordered roster automatically.  A
// link battle is different: the defeated player's next active Pokemon is a
// choice, so keep the fainted battler selected until both peers submit the
// synchronized forced-switch command.
void finishOpponentFaint(BattleState& battle, PokemonCollection& collection,
                         BattleActionResult& result) {
  // Mean Look/Block/Spider Web is tied to the battler that established it.
  // When that battler faints, the opposing position is free immediately.
  setEscapePrevented(battle.playerVolatile,false);
  if (battle.kind == BattleKind::Pvp) {
    if (hasHealthyOpponentReplacement(battle)) return;
  } else if (battle.opponentIndex + 1U < battle.opponentCount) {
    // A sure-hit mark is owned by the outgoing attacker. If that attacker
    // faints, a later teammate must not inherit Mind Reader/Lock-On.
    if(sureHitOwner(battle.playerVolatile)==2U)setSureHitOwner(battle.playerVolatile,0);
    // The fainted record is no longer battle-active. Its temporary Transform
    // snapshot must never leak into the next ordered trainer Pokemon.
    battle.opponentTransformSnapshot=TransformBattleSnapshot{};
    ++battle.opponentIndex;
    OwnedPokemon* next = BattleEngine::currentOpponent(battle);
    battle.opponentMoveEffects[battle.opponentIndex].enteredTurn=battle.turn;
    appendEvent(result, BattleEventType::SwitchedIn, BattleSide::Opponent,
                next ? next->uid : 0U);
    if(next)applySpikesOnEntry(battle,*next,BattleSide::Opponent,result);
    if(next&&next->currentHp)BattleEngine::applyEntryAbilities(
        battle,collection,BattleEntryScope::Opponent,&result);
    return;
  }
  battle.active = false;
  battle.outcome = BattleOutcome::Victory;
  result.moneyGained = moneyReward(battle, collection);
  applyPickupAfterBattle(battle,collection);
}

bool selectablePartyMember(PokemonCollection& collection, uint32_t uid) {
  OwnedPokemon* pokemon = CollectionLogic::find(collection, uid);
  return pokemon && CollectionLogic::isInParty(collection, uid) && pokemon->currentHp > 0 &&
         pokemon->recoverySecondsRemaining == 0;
}

uint8_t boundedLevel(int16_t level) {
  return static_cast<uint8_t>(std::max<int16_t>(2, std::min<int16_t>(100, level)));
}

EffortValues effortYieldFor(const SpeciesData& species) {
  return {species.evYieldHp, species.evYieldAttack, species.evYieldDefense,
          species.evYieldSpAttack, species.evYieldSpDefense, species.evYieldSpeed};
}
bool abilityIs(const OwnedPokemon& pokemon, const char* name) {
  const AbilityData* ability = findAbility(pokemon.abilityId);
  return ability && std::strcmp(ability->name, name) == 0;
}
uint32_t nextBattleRandom(BattleState& battle) {
  uint32_t x=battle.rngState?battle.rngState:0xA341316CU;
  x^=x<<13U;x^=x>>17U;x^=x<<5U;battle.rngState=x;
  return x;
}
bool moldBreakerCanIgnore(const char* name) {
  // Mold Breaker does not suppress every Ability owned by the target.  In
  // particular, effects which happen after the move (Rough Skin, Static,
  // Effect Spore, Color Change, Synchronize and Liquid Ooze) still activate.
  // Keep this explicit allow-list in sync with the official breakable flag
  // instead of treating every defender Ability as breakable.
  static const char* const kBreakable[] = {
      "BATTLE ARMOR", "STURDY", "DAMP", "LIMBER", "SAND VEIL",
      "VOLT ABSORB", "WATER ABSORB", "OBLIVIOUS", "IMMUNITY",
      "FLASH FIRE", "SHIELD DUST", "OWN TEMPO", "SUCTION CUPS",
      "WONDER GUARD", "LEVITATE", "CLEAR BODY", "INNER FOCUS",
      "MAGMA ARMOR", "WATER VEIL", "SOUNDPROOF", "CACOPHONY",
      "THICK FAT", "KEEN EYE", "HYPER CUTTER", "STICKY HOLD",
      "MARVEL SCALE", "WHITE SMOKE", "SHELL ARMOR", "MAGIC BOUNCE",
      "FILTER",
  };
  for (const char* breakable : kBreakable)
    if (std::strcmp(name, breakable) == 0) return true;
  return false;
}
bool defenderAbilityApplies(const OwnedPokemon& attacker,
                            const OwnedPokemon& defender,const char* name) {
  return abilityIs(defender,name)&&
      (!abilityIs(attacker,"MOLD BREAKER")||!moldBreakerCanIgnore(name));
}
bool absorbingAbilityBlocksMove(BattleState& battle,const FullMoveData* move,
                                const OwnedPokemon& attacker,
                                const OwnedPokemon& defender) {
  if(!move||abilityIs(attacker,"MOLD BREAKER"))return false;
  const PokemonType type=effectiveMoveType(battle,*move,attacker,&defender);
  if(type==PokemonType::Fire&&abilityIs(defender,"FLASH FIRE")&&
     defender.status!=StatusCondition::Frozen)return true;
  return move->power&&((type==PokemonType::Water&&abilityIs(defender,"WATER ABSORB"))||
      (type==PokemonType::Electric&&abilityIs(defender,"VOLT ABSORB")));
}
bool trappedByAbility(const OwnedPokemon& escapee,
                      const DedicatedMoveEffectState& escapeeEffects,
                      const OwnedPokemon& trapper){
  // FireRed does not grant the later-generation Shadow Tag mirror exemption.
  if(abilityIs(trapper,"SHADOW TAG"))return true;
  if(abilityIs(trapper,"ARENA TRAP")&&
     !hasBattleType(escapee,escapeeEffects,PokemonType::Flying)&&
     !abilityIs(escapee,"LEVITATE"))return true;
  return abilityIs(trapper,"MAGNET PULL")&&
      hasBattleType(escapee,escapeeEffects,PokemonType::Steel);
}
bool isSoundMove(const FullMoveData& move) {
  static const char* names[] = {"GROWL","ROAR","SING","SUPERSONIC","SCREECH","SNORE",
      "UPROAR","METAL SOUND","GRASS WHISTLE","HYPER VOICE"};
  // Perish Song and Heal Bell perform their own per-battler Soundproof
  // checks in Gen III. They are deliberately absent from sSoundMovesTable;
  // treating either as an ordinary sound-targeted move incorrectly lets one
  // Soundproof opponent cancel the entire side/party operation.
  for (const char* name : names) if (std::strcmp(move.name,name)==0) return true;
  return false;
}
bool semiInvulnerable(const CombatVolatile& state){
  const uint16_t move=static_cast<uint16_t>(state.chargingMove);
  return move==19U||move==91U||move==291U||move==340U; // Fly, Dig, Dive, Bounce
}

void transformInto(BattleState& battle, OwnedPokemon& source, const OwnedPokemon& target,
                   CombatVolatile& sourceVolatile,const CombatVolatile& targetVolatile,
                   DedicatedMoveEffectState& sourceEffects,
                   const DedicatedMoveEffectState& targetEffects,bool sourceIsPlayer) {
  TransformBattleSnapshot& snapshot=sourceIsPlayer?battle.playerTransformSnapshot:
      battle.opponentTransformSnapshot;
  if (sourceIsPlayer && !battle.playerTransformed) {
    battle.playerTransformed = true;
    battle.playerOriginalSpeciesId = source.speciesId;
    battle.playerOriginalAbilityId = source.abilityId;
    snapshot.originalForm = source.form;
    std::memcpy(battle.playerOriginalMoves, source.moves, sizeof(source.moves));
    std::memcpy(battle.playerOriginalMovePp, source.movePp, sizeof(source.movePp));
  }
  if(!sourceIsPlayer&&!snapshot.active){
    snapshot.originalSpeciesId=source.speciesId;
    snapshot.originalForm=source.form;
    snapshot.originalAbilityId=source.abilityId;
    std::memcpy(snapshot.originalMoves,source.moves,sizeof(source.moves));
    std::memcpy(snapshot.originalMovePp,source.movePp,sizeof(source.movePp));
  }
  snapshot.active=true;
  snapshot.attack=CollectionLogic::calculatedStat(target,PokemonStat::Attack);
  snapshot.defense=CollectionLogic::calculatedStat(target,PokemonStat::Defense);
  snapshot.spAttack=CollectionLogic::calculatedStat(target,PokemonStat::SpAttack);
  snapshot.spDefense=CollectionLogic::calculatedStat(target,PokemonStat::SpDefense);
  snapshot.speed=CollectionLogic::calculatedStat(target,PokemonStat::Speed);
  snapshot.ivs=target.ivs;
  sourceEffects.transformed=true;
  source.speciesId = target.speciesId;
  source.form = target.form;
  source.abilityId = target.abilityId;
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    source.moves[slot] = target.moves[slot];
    const FullMoveData* copied=findFullMove(target.moves[slot]);
    source.movePp[slot] = copied?std::min<uint8_t>(5U,copied->pp):0U;
  }
  // Cmd_transformdataexecution copies all seven stat stages, clears Disable,
  // and copies the target's current battle types. HP/max HP, level, status,
  // personality and held item deliberately remain the user's own.
  sourceVolatile.attackStage=targetVolatile.attackStage;
  sourceVolatile.defenseStage=targetVolatile.defenseStage;
  sourceVolatile.spAttackStage=targetVolatile.spAttackStage;
  sourceVolatile.spDefenseStage=targetVolatile.spDefenseStage;
  sourceVolatile.speedStage=targetVolatile.speedStage;
  sourceVolatile.accuracyStage=targetVolatile.accuracyStage;
  sourceVolatile.evasionStage=targetVolatile.evasionStage;
  sourceVolatile.disabledMove=MoveId::None;
  setDisableTurns(sourceVolatile,0);
  sourceEffects.typeOverrideActive=true;
  if(targetEffects.typeOverrideActive){
    sourceEffects.type1=targetEffects.type1;sourceEffects.type2=targetEffects.type2;
  }else if(const SpeciesData* species=findSpecies(target.speciesId)){
    sourceEffects.type1=MegaEvolution::type1(target,species->type1);
    sourceEffects.type2=MegaEvolution::type2(target,species->type2);
  }
}
void restorePlayerTransform(BattleState& battle, PokemonCollection& collection) {
  if (!battle.playerTransformed) return;
  if (OwnedPokemon* player = CollectionLogic::find(collection, battle.playerUid)) {
    player->speciesId = battle.playerOriginalSpeciesId;
    player->form = battle.playerTransformSnapshot.originalForm;
    player->abilityId = battle.playerOriginalAbilityId;
    std::memcpy(player->moves, battle.playerOriginalMoves, sizeof(player->moves));
    std::memcpy(player->movePp, battle.playerOriginalMovePp, sizeof(player->movePp));
  }
  battle.playerTransformed = false;
  battle.playerTransformSnapshot=TransformBattleSnapshot{};
  battle.playerMoveEffects.transformed=false;
}
void restoreOpponentTransform(BattleState& battle){
  TransformBattleSnapshot& snapshot=battle.opponentTransformSnapshot;
  if(!snapshot.active)return;
  if(OwnedPokemon* opponent=BattleEngine::currentOpponent(battle)){
    opponent->speciesId=snapshot.originalSpeciesId;
    opponent->form=snapshot.originalForm;
    opponent->abilityId=snapshot.originalAbilityId;
    std::memcpy(opponent->moves,snapshot.originalMoves,sizeof(opponent->moves));
    std::memcpy(opponent->movePp,snapshot.originalMovePp,sizeof(opponent->movePp));
  }
  snapshot=TransformBattleSnapshot{};
  if(battle.opponentIndex<kOpponentTeamCapacity)
    battle.opponentMoveEffects[battle.opponentIndex].transformed=false;
}
void restorePlayerTrace(BattleState& battle,PokemonCollection& collection){
  if(!battle.playerAbilityTraced)return;
  if(OwnedPokemon* player=CollectionLogic::find(collection,battle.playerUid))player->abilityId=battle.playerPreTraceAbilityId;
  battle.playerAbilityTraced=false;
}
void restorePlayerMimic(BattleState& battle,PokemonCollection& collection){
  if(!battle.playerMimicActive)return;
  if(OwnedPokemon* player=CollectionLogic::find(collection,battle.playerMimicUid)){
    const uint8_t slot=std::min<uint8_t>(battle.playerMimicSlot,kMoveSlots-1U);
    player->moves[slot]=battle.playerMimicOriginalMove;
    player->movePp[slot]=battle.playerMimicOriginalPp;
  }
  battle.playerMimicActive=false;
}
void restorePlayerBattleForm(BattleState& battle,PokemonCollection& collection){
  OwnedPokemon* active=CollectionLogic::find(collection,battle.playerUid);
  // Natural Cure checks the Ability the battler actually has while leaving
  // the field.  Snapshot it before undoing Trace/Skill Swap/Transform: a
  // traced Natural Cure must heal, while a native Natural Cure that was
  // swapped away must not suddenly reactivate during restoration.
  const bool naturalCureOnExit=active&&abilityIs(*active,"NATURAL CURE");
  restorePlayerMimic(battle,collection);restorePlayerTransform(battle,collection);
  restorePlayerTrace(battle,collection);
  // Skill Swap and Role Play mutate the collection-backed active record too.
  // Re-derive its native (or Mega) Ability whenever it leaves battle so no
  // temporary Ability can leak into Home, Box, a later encounter or Pickup.
  if(OwnedPokemon* player=CollectionLogic::find(collection,battle.playerUid))
    CollectionLogic::refreshAbility(*player);
  // Returning the active Pokemon at the end of a battle counts as switching
  // out for NATURAL CURE in the retail games.  Ordinary switch paths already
  // handled this, but victory/run/defeat previously left the status behind.
  if(naturalCureOnExit)
    if(OwnedPokemon* player=CollectionLogic::find(collection,battle.playerUid))
      player->status=StatusCondition::None;
  // Forecast is a battle form. Never leak SUNNY/RAINY/SNOWY into Home, Box
  // or the persistent collection after the battler leaves the field.
  if(OwnedPokemon* player=CollectionLogic::find(collection,battle.playerUid))
    if(player->speciesId==351U)player->form=0;
}

void restoreOpponentTemporaryAbility(OwnedPokemon& pokemon){
  // TRACE, ROLE PLAY and SKILL SWAP are battle-local and all end when the
  // affected battler leaves the field.  Opponent records live in BattleState,
  // so they need the same restoration the collection-backed player receives.
  CollectionLogic::refreshAbility(pokemon);
}
DedicatedMoveEffectState& moveEffectsFor(BattleState& battle, BattleSide side) {
  return side == BattleSide::Player ? battle.playerMoveEffects
      : battle.opponentMoveEffects[battle.opponentIndex];
}
const DedicatedMoveEffectState& moveEffectsFor(const BattleState& battle, BattleSide side) {
  return side == BattleSide::Player ? battle.playerMoveEffects
      : battle.opponentMoveEffects[battle.opponentIndex];
}

bool cureConditionForbiddenByAbility(OwnedPokemon& pokemon,
                                     CombatVolatile& volatileState,
                                     DedicatedMoveEffectState& effects,
                                     BattleSide side,
                                     BattleActionResult* result) {
  bool changed=false;
  bool curesMajor=false;
  switch(pokemon.status){
    case StatusCondition::Poison:
    case StatusCondition::BadlyPoisoned: curesMajor=abilityIs(pokemon,"IMMUNITY");break;
    case StatusCondition::Paralysis: curesMajor=abilityIs(pokemon,"LIMBER");break;
    case StatusCondition::Sleep:
      curesMajor=abilityIs(pokemon,"INSOMNIA")||abilityIs(pokemon,"VITAL SPIRIT");break;
    case StatusCondition::Burn: curesMajor=abilityIs(pokemon,"WATER VEIL");break;
    case StatusCondition::Frozen: curesMajor=abilityIs(pokemon,"MAGMA ARMOR");break;
    default: break;
  }
  if(curesMajor){
    const StatusCondition before=pokemon.status;
    pokemon.status=StatusCondition::None;
    volatileState.sleepTurns=0;
    volatileState.toxicCounter=0;
    if(result){
      BattleEvent& cured=appendEvent(*result,BattleEventType::StatusCured,side,pokemon.uid);
      cured.status=before;
      cured.before=pokemon.abilityId;
    }
    changed=true;
  }
  if(volatileState.confusionTurns&&abilityIs(pokemon,"OWN TEMPO")){
    volatileState.confusionTurns=0;
    if(result){
      BattleEvent& cured=appendEvent(*result,BattleEventType::MoveEffect,side,pokemon.uid);
      cured.value=static_cast<uint16_t>(BattleMoveEffect::AbilityCured);
      cured.before=pokemon.abilityId;
    }
    changed=true;
  }
  if(effects.attractedToUid&&abilityIs(pokemon,"OBLIVIOUS")){
    effects.attractedToUid=0;
    if(result){
      BattleEvent& cured=appendEvent(*result,BattleEventType::MoveEffect,side,pokemon.uid);
      cured.value=static_cast<uint16_t>(BattleMoveEffect::AbilityCured);
      cured.before=pokemon.abilityId;
    }
    changed=true;
  }
  return changed;
}

bool truantLoafsThisTurn(const BattleState& battle,const OwnedPokemon& pokemon,
                         const DedicatedMoveEffectState& effects){
  if(!abilityIs(pokemon,"TRUANT")||battle.turn<=effects.enteredTurn)return false;
  return ((battle.turn-effects.enteredTurn)&1U)==0U;
}

bool wakeAfterSleepTick(const OwnedPokemon& pokemon,CombatVolatile& volatileState){
  const uint8_t amount=abilityIs(pokemon,"EARLY BIRD")?2U:1U;
  if(volatileState.sleepTurns<=amount){volatileState.sleepTurns=0;return true;}
  volatileState.sleepTurns=static_cast<uint8_t>(volatileState.sleepTurns-amount);
  return false;
}

uint8_t castformWeatherForm(BattleWeather weather, bool suppressed) {
  if (suppressed) return 0;
  if (weather == BattleWeather::Sun || weather == BattleWeather::HarshSun) return 1;
  if (weather == BattleWeather::Rain || weather == BattleWeather::HeavyRain) return 2;
  if (weather == BattleWeather::Hail) return 3;
  // Generation III has no sand form. Sandstorm, Strong Winds and clear
  // weather all restore Castform's NORMAL form/type.
  return 0;
}

PokemonType castformWeatherType(uint8_t form) {
  switch (form) {
    case 1: return PokemonType::Fire;
    case 2: return PokemonType::Water;
    case 3: return PokemonType::Ice;
    default: return PokemonType::Normal;
  }
}

bool updateCastformForecast(BattleState& battle, OwnedPokemon& pokemon,
                            DedicatedMoveEffectState& effects, BattleSide side,
                            bool suppressed, BattleActionResult* result) {
  // Exact CastformDataTypeChange guard from FireRed/Emerald. A fainted
  // Castform and a Castform that no longer owns Forecast do not run it.
  if (pokemon.speciesId != 351U || !abilityIs(pokemon, "FORECAST") ||
      pokemon.currentHp == 0U) return false;
  const uint8_t before = std::min<uint8_t>(pokemon.form, 3U);
  const uint8_t after = castformWeatherForm(battle.weather, suppressed);
  const PokemonType type = castformWeatherType(after);
  const bool nativeNormal = after == 0U && !effects.typeOverrideActive;
  const bool alreadyCorrect = before == after &&
      (nativeNormal || (effects.typeOverrideActive && effects.type1 == type &&
                        effects.type2 == type));
  if (alreadyCorrect) return false;

  pokemon.form = after;
  effects.typeOverrideActive = true;
  effects.type1 = effects.type2 = type;
  if (result) {
    BattleEvent& changed = appendEvent(*result, BattleEventType::MoveEffect,
                                       side, pokemon.uid);
    changed.value = static_cast<uint16_t>(BattleMoveEffect::FormChanged);
    changed.before = before;
    changed.after = after;
  }
  return true;
}

void refreshForecastPair(BattleState& battle, OwnedPokemon& player,
                         DedicatedMoveEffectState& playerEffects,
                         OwnedPokemon& opponent,
                         DedicatedMoveEffectState& opponentEffects,
                         BattleActionResult* result = nullptr) {
  const bool suppressed = abilityIs(player, "CLOUD NINE") ||
      abilityIs(player, "AIR LOCK") || abilityIs(opponent, "CLOUD NINE") ||
      abilityIs(opponent, "AIR LOCK");
  updateCastformForecast(battle, player, playerEffects, BattleSide::Player,
                         suppressed, result);
  updateCastformForecast(battle, opponent, opponentEffects, BattleSide::Opponent,
                         suppressed, result);
}

void refreshForecastForms(BattleState& battle, PokemonCollection& collection,
                          BattleActionResult* result = nullptr) {
  OwnedPokemon* player = CollectionLogic::find(collection, battle.playerUid);
  OwnedPokemon* opponent = BattleEngine::currentOpponent(battle);
  if (!player || !opponent) return;
  refreshForecastPair(battle, *player, battle.playerMoveEffects, *opponent,
                      battle.opponentMoveEffects[battle.opponentIndex], result);
}

HeldItem heldItemFor(const OwnedPokemon& pokemon,const CombatVolatile& volatileState){
  if(volatileState.heldItemSuppressed)return HeldItem::None;
  return volatileState.hasHeldItemOverride?volatileState.heldItemOverride:pokemon.heldItem;
}
bool heldIs(const OwnedPokemon& pokemon,const CombatVolatile& volatileState,HeldItem item){return heldItemFor(pokemon,volatileState)==item;}
void setBattleHeldItem(CombatVolatile& volatileState,HeldItem item){volatileState.heldItemOverride=item;volatileState.hasHeldItemOverride=true;volatileState.heldItemSuppressed=false;}
void consumeHeld(OwnedPokemon& pokemon,CombatVolatile& volatileState,
                 DedicatedMoveEffectState* moveEffects=nullptr){
  const HeldItem consumed=heldItemFor(pokemon,volatileState);
  if(moveEffects&&consumed!=HeldItem::None)moveEffects->recyclableItem=consumed;
  if(volatileState.hasHeldItemOverride)setBattleHeldItem(volatileState,HeldItem::None);
  else CollectionLogic::consumeHeldItem(pokemon);
}
bool stealBattleHeldItem(OwnedPokemon& source,CombatVolatile& sourceVolatile,OwnedPokemon& target,CombatVolatile& targetVolatile){
  const HeldItem sourceItem=heldItemFor(source,sourceVolatile),targetItem=heldItemFor(target,targetVolatile);
  if(sourceItem!=HeldItem::None||targetItem==HeldItem::None||heldItemIsTransferLocked(targetItem)||
     defenderAbilityApplies(source,target,"STICKY HOLD"))return false;
  setBattleHeldItem(sourceVolatile,targetItem);setBattleHeldItem(targetVolatile,HeldItem::None);return true;
}
bool swapBattleHeldItems(OwnedPokemon& source,CombatVolatile& sourceVolatile,OwnedPokemon& target,CombatVolatile& targetVolatile){
  const HeldItem sourceItem=heldItemFor(source,sourceVolatile),targetItem=heldItemFor(target,targetVolatile);
  if((sourceItem==HeldItem::None&&targetItem==HeldItem::None)||heldItemIsTransferLocked(sourceItem)||heldItemIsTransferLocked(targetItem)||
     defenderAbilityApplies(source,target,"STICKY HOLD"))return false;
  setBattleHeldItem(sourceVolatile,targetItem);setBattleHeldItem(targetVolatile,sourceItem);return true;
}
bool swapPersistentHeldItems(OwnedPokemon& source,CombatVolatile& sourceVolatile,
                             OwnedPokemon& target,CombatVolatile& targetVolatile){
  if(sourceVolatile.heldItemSuppressed||targetVolatile.heldItemSuppressed||
     defenderAbilityApplies(source,target,"STICKY HOLD"))return false;
  const HeldItem sourceItem=heldItemFor(source,sourceVolatile);
  const HeldItem targetItem=heldItemFor(target,targetVolatile);
  if((sourceItem==HeldItem::None&&targetItem==HeldItem::None)||
     heldItemIsTransferLocked(sourceItem)||heldItemIsTransferLocked(targetItem))return false;
  const uint8_t sourceQuantity=CollectionLogic::heldItemQuantity(source);
  const uint8_t targetQuantity=CollectionLogic::heldItemQuantity(target);
  CollectionLogic::setHeldItemQuantity(source,targetItem,targetQuantity);
  CollectionLogic::setHeldItemQuantity(target,sourceItem,sourceQuantity);
  sourceVolatile.heldItemOverride=HeldItem::None;sourceVolatile.hasHeldItemOverride=false;
  sourceVolatile.heldItemSuppressed=false;sourceVolatile.choiceMove=MoveId::None;
  targetVolatile.heldItemOverride=HeldItem::None;targetVolatile.hasHeldItemOverride=false;
  targetVolatile.heldItemSuppressed=false;targetVolatile.choiceMove=MoveId::None;
  return true;
}
bool knockOffBattleHeldItem(const OwnedPokemon& source,OwnedPokemon& target,
                            CombatVolatile& targetVolatile,HeldItem* removed){
  const HeldItem item=heldItemFor(target,targetVolatile);if(item==HeldItem::None||heldItemIsTransferLocked(item)||
     defenderAbilityApplies(source,target,"STICKY HOLD"))return false;
  targetVolatile.heldItemSuppressed=true;if(removed)*removed=item;return true;
}
void storePlayerHeldItemState(BattleState& battle,const OwnedPokemon& pokemon){
  for(BattleHeldItemState& state:battle.playerHeldItems)if(state.pokemonUid==pokemon.uid||state.pokemonUid==0){
    state.pokemonUid=pokemon.uid;state.heldItemOverride=battle.playerVolatile.heldItemOverride;
    state.hasHeldItemOverride=battle.playerVolatile.hasHeldItemOverride;state.heldItemSuppressed=battle.playerVolatile.heldItemSuppressed;return;
  }
}
void loadPlayerHeldItemState(BattleState& battle,const OwnedPokemon& pokemon){
  battle.playerVolatile=CombatVolatile{};
  battle.playerBide=BideState{};
  battle.playerEncoreTurns=0;
  for(const BattleHeldItemState& state:battle.playerHeldItems)if(state.pokemonUid==pokemon.uid){
    battle.playerVolatile.heldItemOverride=state.heldItemOverride;battle.playerVolatile.hasHeldItemOverride=state.hasHeldItemOverride;
    battle.playerVolatile.heldItemSuppressed=state.heldItemSuppressed;return;
  }
}
void storePlayerMoveEffectState(BattleState& battle,const OwnedPokemon& pokemon){
  for(BattleRecyclableItemState& state:battle.playerRecyclableItems)
    if(state.pokemonUid==pokemon.uid||state.pokemonUid==0){
      state.pokemonUid=pokemon.uid;state.item=battle.playerMoveEffects.recyclableItem;return;
    }
}
void loadPlayerMoveEffectState(BattleState& battle,const OwnedPokemon& pokemon){
  battle.playerMoveEffects=DedicatedMoveEffectState{};
  for(const BattleRecyclableItemState& state:battle.playerRecyclableItems)
    if(state.pokemonUid==pokemon.uid){battle.playerMoveEffects.recyclableItem=state.item;return;}
}
void clearVolatileKeepHeld(CombatVolatile& volatileState){
  const HeldItem item=volatileState.heldItemOverride;const bool overridden=volatileState.hasHeldItemOverride;
  const bool suppressed=volatileState.heldItemSuppressed;volatileState=CombatVolatile{};
  volatileState.heldItemOverride=item;volatileState.hasHeldItemOverride=overridden;volatileState.heldItemSuppressed=suppressed;
}
void clearMoveEffectsOnSwitch(DedicatedMoveEffectState& state){
  const HeldItem recyclable=state.recyclableItem;
  state=DedicatedMoveEffectState{};
  state.recyclableItem=recyclable;
}
void applyBatonPassMoveEffects(const DedicatedMoveEffectState& passed,
                               DedicatedMoveEffectState& incoming){
  const HeldItem recyclable=incoming.recyclableItem;
  incoming=DedicatedMoveEffectState{};
  incoming.recyclableItem=recyclable;
  // SwitchInDataUpdate reloads the recipient's native types.  FireRed's
  // Baton-Pass masks likewise omit Conversion/Camouflage, Defense Curl,
  // Minimize and Foresight; none of those may leak to the incoming Pokemon.
  incoming.perishTurns=passed.perishTurns;
  incoming.ingrained=passed.ingrained;
  incoming.mudSport=passed.mudSport;
  incoming.waterSport=passed.waterSport;
}
// Gen III Baton Pass carries the battle position's stat stages and the
// volatile effects that are explicitly passable. It must not carry move
// locks, recharge/charge state, flinch, sleep counters or Choice Band locks.
// Held-item overrides belong to the individual Pokemon and are therefore
// preserved from the incoming battler rather than copied from the passer.
void applyBatonPassState(const CombatVolatile& passed, CombatVolatile& incoming) {
  const HeldItem item = incoming.heldItemOverride;
  const bool overridden = incoming.hasHeldItemOverride;
  const bool suppressed = incoming.heldItemSuppressed;
  incoming = CombatVolatile{};
  incoming.heldItemOverride = item;
  incoming.hasHeldItemOverride = overridden;
  incoming.heldItemSuppressed = suppressed;
  incoming.attackStage = passed.attackStage;
  incoming.defenseStage = passed.defenseStage;
  incoming.spAttackStage = passed.spAttackStage;
  incoming.spDefenseStage = passed.spDefenseStage;
  incoming.speedStage = passed.speedStage;
  incoming.accuracyStage = passed.accuracyStage;
  incoming.evasionStage = passed.evasionStage;
  incoming.confusionTurns = passed.confusionTurns;
  incoming.criticalStage = passed.criticalStage;
  incoming.substituteHp = passed.substituteHp;
  incoming.seeded = passed.seeded;
  // FireRed's SwitchInClearSetData keeps CURSED, ESCAPE_PREVENTION and
  // ALWAYS_HITS across Baton Pass, but explicitly drops WRAPPED and Disable.
  setCursed(incoming,isCursed(passed));
  setEscapePrevented(incoming,escapePrevented(passed));
  copySureHit(passed,incoming);
}

bool opponentBatonPass(BattleState& battle, PokemonCollection& collection,
                       BattleActionResult& result, MoveId move) {
  const uint8_t outgoingIndex = battle.opponentIndex;
  uint8_t incomingIndex = kOpponentTeamCapacity;
  for (uint8_t index = 0; index < battle.opponentCount; ++index) {
    if (index != outgoingIndex && battle.opponents[index].currentHp > 0) {
      incomingIndex = index;
      break;
    }
  }
  if (incomingIndex >= battle.opponentCount) return false;

  const uint32_t outgoingUid = battle.opponents[outgoingIndex].uid;
  const CombatVolatile passed = battle.opponentVolatiles[outgoingIndex];
  const DedicatedMoveEffectState passedMoveEffects=battle.opponentMoveEffects[outgoingIndex];
  if (abilityIs(battle.opponents[outgoingIndex], "NATURAL CURE"))
    battle.opponents[outgoingIndex].status = StatusCondition::None;
  restoreOpponentTransform(battle);
  if (battle.opponents[outgoingIndex].speciesId == 351U)
    battle.opponents[outgoingIndex].form = 0;
  restoreOpponentTemporaryAbility(battle.opponents[outgoingIndex]);

  std::swap(battle.opponents[outgoingIndex], battle.opponents[incomingIndex]);
  std::swap(battle.opponentVolatiles[outgoingIndex],
            battle.opponentVolatiles[incomingIndex]);
  std::swap(battle.opponentMoveEffects[outgoingIndex],
            battle.opponentMoveEffects[incomingIndex]);
  clearVolatileKeepHeld(battle.opponentVolatiles[incomingIndex]);
  applyBatonPassState(passed, battle.opponentVolatiles[outgoingIndex]);
  clearMoveEffectsOnSwitch(battle.opponentMoveEffects[incomingIndex]);
  applyBatonPassMoveEffects(passedMoveEffects,battle.opponentMoveEffects[outgoingIndex]);
  // A Lock-On/Mind Reader relation established by the passer changes owner
  // to the incoming battler in Gen III and receives a fresh one-turn window.
  if(sureHitOwner(battle.playerVolatile)==2U)armSureHit(battle.playerVolatile,2U);
  battle.opponentBides[outgoingIndex] = BideState{};
  battle.opponentBides[incomingIndex] = BideState{};
  battle.opponentEncoreTurns[outgoingIndex] = 0;
  battle.opponentEncoreTurns[incomingIndex] = 0;
  battle.opponentNightmares[outgoingIndex] = false;
  battle.opponentNightmares[incomingIndex] = false;

  appendMoveEffect(result, BattleSide::Opponent, outgoingUid, move,
                   BattleMoveEffect::BatonPass);
  appendEvent(result, BattleEventType::SwitchedIn, BattleSide::Opponent,
              battle.opponents[outgoingIndex].uid);
  BattleEngine::applyEntryAbilities(battle, collection,BattleEntryScope::Opponent,&result);
  return true;
}
void clearStatStages(CombatVolatile& state) {
  state.attackStage = state.defenseStage = state.spAttackStage = state.spDefenseStage = 0;
  state.speedStage = state.accuracyStage = state.evasionStage = 0;
}
void applyHeldMoveEffect(const FullMoveData& move,bool effectCanApply,
                         bool targetHadSubstitute,BattleKind battleKind,
                         BattleSide sourceSide,OwnedPokemon& source,CombatVolatile& sourceVolatile,
                         OwnedPokemon& target,CombatVolatile& targetVolatile,BattleActionResult& result){
  if(std::strstr(move.effect,"THIEF")){
    HeldItem taken=heldItemFor(target,targetVolatile);
    if(effectCanApply&&!targetHadSubstitute&&
       stealBattleHeldItem(source,sourceVolatile,target,targetVolatile)){
      result.heldItemStolen=true;result.affectedHeldItem=taken;
      BattleEvent& event=appendEvent(result,BattleEventType::MoveEffect,sourceSide,source.uid);
      event.move=static_cast<MoveId>(move.id);event.value=static_cast<uint16_t>(BattleMoveEffect::HeldItemStolen);
      event.after=static_cast<uint16_t>(taken);
    }
  }else if(std::strstr(move.effect,"TRICK")){
    // FireRed prevents an NPC/wild opponent from taking the player's item.
    // A player-initiated PvE Trick updates the owned Pokemon immediately;
    // link/PvP changes remain battle-local and can never steal from a peer.
    const HeldItem sourceItem=heldItemFor(source,sourceVolatile);
    const HeldItem targetItem=heldItemFor(target,targetVolatile);
    const bool allowed=sourceSide==BattleSide::Player||battleKind==BattleKind::Pvp;
    const bool swapped=allowed&&(battleKind==BattleKind::Pvp
        ? swapBattleHeldItems(source,sourceVolatile,target,targetVolatile)
        : swapPersistentHeldItems(source,sourceVolatile,target,targetVolatile));
    if(swapped){
      result.heldItemSwapped=true;result.affectedHeldItem=targetItem;
      sourceVolatile.choiceMove=MoveId::None;targetVolatile.choiceMove=MoveId::None;
      BattleEvent& event=appendEvent(result,BattleEventType::MoveEffect,sourceSide,source.uid);
      event.move=static_cast<MoveId>(move.id);event.value=static_cast<uint16_t>(BattleMoveEffect::HeldItemsSwapped);
      event.before=static_cast<uint16_t>(sourceItem);event.after=static_cast<uint16_t>(targetItem);
    }
  }else if(std::strstr(move.effect,"KNOCK_OFF")){
    HeldItem removed=HeldItem::None;
    if(effectCanApply&&!targetHadSubstitute&&
       knockOffBattleHeldItem(source,target,targetVolatile,&removed)){
      result.heldItemKnockedOff=true;result.affectedHeldItem=removed;
      BattleEvent& event=appendEvent(result,BattleEventType::MoveEffect,sourceSide,target.uid);
      event.move=static_cast<MoveId>(move.id);event.value=static_cast<uint16_t>(BattleMoveEffect::HeldItemKnockedOff);
      event.after=static_cast<uint16_t>(removed);
    }
  }
}
uint32_t heldRandom(BattleState& battle){uint32_t x=battle.rngState?battle.rngState:0xA341316CU;x^=x<<13U;x^=x>>17U;x^=x<<5U;battle.rngState=x;return x;}
bool resetNegativeStages(CombatVolatile& volatileState){bool changed=false;int8_t* stages[]={&volatileState.attackStage,&volatileState.defenseStage,&volatileState.spAttackStage,&volatileState.spDefenseStage,&volatileState.speedStage,&volatileState.accuracyStage,&volatileState.evasionStage};for(int8_t* stage:stages)if(*stage<0){*stage=0;changed=true;}return changed;}
void recordHeldItemActivation(BattleActionResult& result,const OwnedPokemon& pokemon,HeldItem item,BattleSide side){
  if(item==HeldItem::None||result.heldItemActivationCount>=4U)return;
  const uint8_t slot=result.heldItemActivationCount++;
  result.heldItemActivationUids[slot]=pokemon.uid;result.heldItemsActivated[slot]=item;
  BattleEvent& event=appendEvent(result,BattleEventType::HeldItemActivated,side,pokemon.uid);
  event.value=static_cast<uint16_t>(item);
}
void applyShellBellWithResult(BattleActionResult& result,OwnedPokemon& pokemon,
                              CombatVolatile& volatileState,BattleSide side,
                              uint16_t inflictedDamage){
  if(!inflictedDamage||!pokemon.currentHp||pokemon.currentHp>=pokemon.maximumHp||
     !heldIs(pokemon,volatileState,HeldItem::ShellBell))return;
  const uint16_t before=pokemon.currentHp;
  pokemon.currentHp=std::min<uint16_t>(pokemon.maximumHp,
      static_cast<uint16_t>(pokemon.currentHp+
      std::max<uint16_t>(1U,inflictedDamage/8U)));
  if(pokemon.currentHp==before)return;
  recordHeldItemActivation(result,pokemon,HeldItem::ShellBell,side);
  appendHpChange(result,side,pokemon.uid,before,pokemon.currentHp);
}
HeldItem triggerHeldItem(OwnedPokemon& pokemon,CombatVolatile& volatileState,
                         DedicatedMoveEffectState* moveEffects=nullptr){
  if(!pokemon.currentHp)return HeldItem::None;
  const HeldItem item=heldItemFor(pokemon,volatileState);
  auto heal=[&](uint16_t amount){pokemon.currentHp=std::min<uint16_t>(pokemon.maximumHp,static_cast<uint16_t>(pokemon.currentHp+amount));consumeHeld(pokemon,volatileState,moveEffects);};
  if(item==HeldItem::OranBerry&&pokemon.currentHp*2U<=pokemon.maximumHp){heal(10);return item;}
  if(item==HeldItem::SitrusBerry&&pokemon.currentHp*2U<=pokemon.maximumHp){heal(30);return item;}
  if(item==HeldItem::BerryJuice&&pokemon.currentHp*2U<=pokemon.maximumHp){heal(20);return item;}
  if(item==HeldItem::LumBerry&&(pokemon.status!=StatusCondition::None||volatileState.confusionTurns)){pokemon.status=StatusCondition::None;volatileState.confusionTurns=0;consumeHeld(pokemon,volatileState,moveEffects);return item;}
  if(item==HeldItem::PersimBerry&&volatileState.confusionTurns){volatileState.confusionTurns=0;consumeHeld(pokemon,volatileState,moveEffects);return item;}
  if(item==HeldItem::CheriBerry&&pokemon.status==StatusCondition::Paralysis){pokemon.status=StatusCondition::None;consumeHeld(pokemon,volatileState,moveEffects);return item;}
  if(item==HeldItem::ChestoBerry&&pokemon.status==StatusCondition::Sleep){pokemon.status=StatusCondition::None;consumeHeld(pokemon,volatileState,moveEffects);return item;}
  if(item==HeldItem::PechaBerry&&(pokemon.status==StatusCondition::Poison||pokemon.status==StatusCondition::BadlyPoisoned)){pokemon.status=StatusCondition::None;consumeHeld(pokemon,volatileState,moveEffects);return item;}
  if(item==HeldItem::RawstBerry&&pokemon.status==StatusCondition::Burn){pokemon.status=StatusCondition::None;consumeHeld(pokemon,volatileState,moveEffects);return item;}
  if(item==HeldItem::AspearBerry&&pokemon.status==StatusCondition::Frozen){pokemon.status=StatusCondition::None;consumeHeld(pokemon,volatileState,moveEffects);return item;}
  if(item==HeldItem::WhiteHerb&&resetNegativeStages(volatileState)){consumeHeld(pokemon,volatileState,moveEffects);return item;}
  if(item==HeldItem::MentalHerb&&moveEffects&&moveEffects->attractedToUid){moveEffects->attractedToUid=0;consumeHeld(pokemon,volatileState,moveEffects);return item;}
  if(item==HeldItem::LeppaBerry){
    for(uint8_t slot=0;slot<kMoveSlots;++slot)if(pokemon.moves[slot]!=MoveId::None&&!pokemon.movePp[slot]){
      pokemon.movePp[slot]=std::min<uint8_t>(CollectionLogic::maximumMovePp(pokemon,slot),10U);
      consumeHeld(pokemon,volatileState,moveEffects);return item;
    }
  }
  const bool pinch=pokemon.currentHp*4U<=pokemon.maximumHp;
  auto raiseAndConsume=[&](int8_t& stage,int8_t amount){
    if(stage>=kMaximumBattleStatStage)return false;
    const int16_t raised=static_cast<int16_t>(stage)+amount;
    stage=static_cast<int8_t>(std::min<int16_t>(kMaximumBattleStatStage,raised));
    consumeHeld(pokemon,volatileState,moveEffects);
    return true;
  };
  if(pinch&&item==HeldItem::LiechiBerry&&raiseAndConsume(volatileState.attackStage,1))return item;
  if(pinch&&item==HeldItem::GanlonBerry&&raiseAndConsume(volatileState.defenseStage,1))return item;
  if(pinch&&item==HeldItem::SalacBerry&&raiseAndConsume(volatileState.speedStage,1))return item;
  if(pinch&&item==HeldItem::PetayaBerry&&raiseAndConsume(volatileState.spAttackStage,1))return item;
  if(pinch&&item==HeldItem::ApicotBerry&&raiseAndConsume(volatileState.spDefenseStage,1))return item;
  if(pinch&&item==HeldItem::LansatBerry&&volatileState.criticalStage<2U){volatileState.criticalStage=2;consumeHeld(pokemon,volatileState,moveEffects);return item;}
  if(pinch&&item==HeldItem::StarfBerry){
    int8_t* stages[]={&volatileState.attackStage,&volatileState.defenseStage,&volatileState.speedStage,
                     &volatileState.spAttackStage,&volatileState.spDefenseStage};
    // FireRed samples only stats which are not already maxed. Starting from
    // a personality-derived slot preserves deterministic replays while the
    // bounded scan prevents STARF BERRY being consumed on a capped stat.
    const uint8_t first=static_cast<uint8_t>(pokemon.personality%5U);
    for(uint8_t offset=0;offset<5U;++offset){
      int8_t& candidate=*stages[(first+offset)%5U];
      if(raiseAndConsume(candidate,2))return item;
    }
  }
  if(pinch&&(item==HeldItem::IapapaBerry||item==HeldItem::WikiBerry||
             item==HeldItem::AguavBerry||item==HeldItem::FigyBerry||item==HeldItem::MagoBerry)){
    heal(std::max<uint16_t>(1U,pokemon.maximumHp/8U));return item;
  }
  return HeldItem::None;
}
HeldItem applyHeldEndTurn(OwnedPokemon& pokemon,CombatVolatile& volatileState,
                          DedicatedMoveEffectState* moveEffects=nullptr){
  if(pokemon.currentHp&&heldIs(pokemon,volatileState,HeldItem::Leftovers)&&pokemon.currentHp<pokemon.maximumHp){
    pokemon.currentHp=std::min<uint16_t>(pokemon.maximumHp,static_cast<uint16_t>(pokemon.currentHp+std::max<uint16_t>(1,pokemon.maximumHp/16U)));return HeldItem::Leftovers;
  }
  return triggerHeldItem(pokemon,volatileState,moveEffects);
}
void triggerHeldItemWithResult(BattleActionResult& result,OwnedPokemon& pokemon,CombatVolatile& volatileState,BattleSide side,DedicatedMoveEffectState* moveEffects=nullptr){
  const uint16_t hpBefore=pokemon.currentHp;const StatusCondition statusBefore=pokemon.status;
  const HeldItem item=triggerHeldItem(pokemon,volatileState,moveEffects);recordHeldItemActivation(result,pokemon,item,side);
  if(item!=HeldItem::None&&pokemon.currentHp!=hpBefore){BattleEvent& hp=appendEvent(result,BattleEventType::HpChanged,side,pokemon.uid);hp.before=hpBefore;hp.after=pokemon.currentHp;}
  appendStatusChange(result,side,pokemon.uid,statusBefore,pokemon.status);
}
void applyHeldEndTurnWithResult(BattleActionResult& result,OwnedPokemon& pokemon,CombatVolatile& volatileState,BattleSide side,DedicatedMoveEffectState* moveEffects=nullptr){
  const uint16_t hpBefore=pokemon.currentHp;const StatusCondition statusBefore=pokemon.status;
  const HeldItem item=applyHeldEndTurn(pokemon,volatileState,moveEffects);recordHeldItemActivation(result,pokemon,item,side);
  if(item!=HeldItem::None&&pokemon.currentHp!=hpBefore){BattleEvent& hp=appendEvent(result,BattleEventType::HpChanged,side,pokemon.uid);hp.before=hpBefore;hp.after=pokemon.currentHp;}
  appendStatusChange(result,side,pokemon.uid,statusBefore,pokemon.status);
}
uint16_t applyHeldDamage(BattleState& battle,OwnedPokemon& target,CombatVolatile& targetVolatile,uint16_t damage){if(damage>=target.currentHp&&target.currentHp>1&&heldIs(target,targetVolatile,HeldItem::FocusBand)&&heldRandom(battle)%10U==0)return static_cast<uint16_t>(target.currentHp-1U);return damage;}
bool partyHolds(const PokemonCollection& collection,HeldItem item){for(uint8_t slot=0;slot<kPartyCapacity;++slot){const OwnedPokemon* pokemon=CollectionLogic::find(collection,collection.party[slot]);if(pokemon&&pokemon->heldItem==item)return true;}return false;}
uint32_t moneyReward(const BattleState& battle,const PokemonCollection& collection){
  // FireRed's moneyMultiplier is applied independently to the trainer prize
  // and to the coins scattered by Pay Day.  Adding Pay Day after doubling the
  // prize made Amulet Coin underpay that second component.
  const uint32_t multiplier=partyHolds(collection,HeldItem::AmuletCoin)?2U:1U;
  return (battle.rewardMoney+battle.payDayMoney)*multiplier;
}
void applyAbsorbAbility(BattleState& battle,const FullMoveData* move,
                        const OwnedPokemon& attacker,OwnedPokemon& target,
                        uint16_t damage,BattleActionResult& result){
  if(!move||damage||!absorbingAbilityBlocksMove(battle,move,attacker,target))return;
  const PokemonType type=effectiveMoveType(battle,*move,attacker,&target);
  const BattleSide targetSide=sideOf(battle,target);
  if(type==PokemonType::Fire&&abilityIs(target,"FLASH FIRE")&&
     target.status!=StatusCondition::Frozen){
    moveEffectsFor(battle,sideOf(battle,target)).flashFireBoost=true;
    appendAbilityActivation(result,targetSide,target);
  }else if(move->power&&((type==PokemonType::Water&&abilityIs(target,"WATER ABSORB"))||
     (type==PokemonType::Electric&&abilityIs(target,"VOLT ABSORB")))){
    const uint16_t before=target.currentHp;
    target.currentHp=std::min<uint16_t>(target.maximumHp,
        static_cast<uint16_t>(target.currentHp+std::max<uint16_t>(1,target.maximumHp/4U)));
    appendAbilityActivation(result,targetSide,target);
    appendHpChange(result,targetSide,target.uid,before,target.currentHp);
  }
}
void appendBlockingDamageAbility(BattleState& battle,const FullMoveData* move,
                                 const OwnedPokemon& attacker,
                                 const OwnedPokemon& defender,uint16_t effectiveness,
                                 BattleActionResult& result){
  if(!move||!move->power||effectiveness)return;
  const PokemonType type=effectiveMoveType(battle,*move,attacker,&defender);
  const DedicatedMoveEffectState& effects=moveEffectsFor(
      battle,sideOf(battle,defender));
  if((type==PokemonType::Ground&&defenderAbilityApplies(attacker,defender,"LEVITATE"))||
     (defenderAbilityApplies(attacker,defender,"WONDER GUARD")&&
      identifiedTypeMultiplier100(type,defender,effects)<=100U))
    appendAbilityActivation(result,sideOf(battle,defender),defender);
}
BattleSide sideOf(const BattleState& battle, const OwnedPokemon& pokemon) {
  return pokemon.uid == battle.playerUid ? BattleSide::Player : BattleSide::Opponent;
}
int8_t pokemonGender(const OwnedPokemon& pokemon);
uint8_t safeguardTurnsFor(const BattleState& battle, BattleSide side) {
  return side == BattleSide::Player ? battle.playerSafeguardTurns : battle.opponentSafeguardTurns;
}
bool protectedFromOpponentStatus(const BattleState& battle, const OwnedPokemon& source,
                                 const OwnedPokemon& target) {
  const BattleSide sourceSide = sideOf(battle, source), targetSide = sideOf(battle, target);
  return sourceSide != targetSide && safeguardTurnsFor(battle, targetSide) != 0;
}
void advanceSafeguardTurns(BattleState& battle, PokemonCollection& collection,
                           BattleActionResult& result) {
  auto tick = [&](uint8_t& turns, BattleSide side, uint32_t uid) {
    if (!turns || --turns) return;
    appendMoveEffect(result, side, uid, MoveId::None, BattleMoveEffect::SafeguardEnded);
  };
  tick(battle.playerSafeguardTurns, BattleSide::Player, battle.playerUid);
  const OwnedPokemon* opponent = BattleEngine::currentOpponent(battle);
  tick(battle.opponentSafeguardTurns, BattleSide::Opponent, opponent ? opponent->uid : 0U);
  auto tickEffect = [&](uint8_t& turns, BattleSide side, uint32_t uid,
                        BattleMoveEffect ended) {
    if (!turns || --turns) return;
    appendMoveEffect(result, side, uid, MoveId::None, ended);
  };
  tickEffect(battle.playerReflectTurns, BattleSide::Player, battle.playerUid,
             BattleMoveEffect::ReflectEnded);
  tickEffect(battle.opponentReflectTurns, BattleSide::Opponent, opponent ? opponent->uid : 0U,
             BattleMoveEffect::ReflectEnded);
  tickEffect(battle.playerLightScreenTurns, BattleSide::Player, battle.playerUid,
             BattleMoveEffect::LightScreenEnded);
  tickEffect(battle.opponentLightScreenTurns, BattleSide::Opponent, opponent ? opponent->uid : 0U,
             BattleMoveEffect::LightScreenEnded);
  tickEffect(battle.playerMistTurns, BattleSide::Player, battle.playerUid,
             BattleMoveEffect::MistEnded);
  tickEffect(battle.opponentMistTurns, BattleSide::Opponent, opponent ? opponent->uid : 0U,
             BattleMoveEffect::MistEnded);
  auto tickEncore = [&](CombatVolatile& state, uint8_t& turns,
                        const OwnedPokemon* pokemon, BattleSide side, uint32_t uid) {
    if (!turns || state.encoreMove == MoveId::None) return;
    const bool exhausted=!pokemon||moveSlotFor(*pokemon,state.encoreMove)>=kMoveSlots;
    if (!exhausted && --turns) return;
    clearEncore(state, turns);
    appendMoveEffect(result, side, uid, static_cast<MoveId>(227),
                     BattleMoveEffect::EncoreEnded);
  };
  tickEncore(battle.playerVolatile, battle.playerEncoreTurns,
             CollectionLogic::find(collection,battle.playerUid),
             BattleSide::Player, battle.playerUid);
  if (battle.opponentIndex < kOpponentTeamCapacity)
    tickEncore(battle.opponentVolatiles[battle.opponentIndex],
               battle.opponentEncoreTurns[battle.opponentIndex],
               opponent,
               BattleSide::Opponent, opponent ? opponent->uid : 0U);
  auto tickDisable=[&](CombatVolatile& state,const OwnedPokemon* pokemon,
                       BattleSide side,uint32_t uid){
    uint8_t turns=disableTurns(state);
    if(state.disabledMove==MoveId::None||!turns)return;
    const bool moveGone=!pokemon||moveSlotFor(*pokemon,state.disabledMove,false)>=kMoveSlots;
    if(!moveGone&&--turns){setDisableTurns(state,turns);return;}
    state.disabledMove=MoveId::None;setDisableTurns(state,0);
    appendMoveEffect(result,side,uid,static_cast<MoveId>(50),
                     BattleMoveEffect::DisableEnded);
  };
  tickDisable(battle.playerVolatile,
              CollectionLogic::find(collection,battle.playerUid),
              BattleSide::Player,battle.playerUid);
  if(battle.opponentIndex<kOpponentTeamCapacity)
    tickDisable(battle.opponentVolatiles[battle.opponentIndex],opponent,
                BattleSide::Opponent,opponent?opponent->uid:0U);
}
void applyContactAbility(BattleState& battle,const FullMoveData* move,
                         OwnedPokemon& attacker,OwnedPokemon& defender,
                         bool defenderLostHp,BattleActionResult& result){
  if(!move||!attacker.currentHp||!defenderLostHp)return;
  DedicatedMoveEffectState& defenderEffects=moveEffectsFor(
      battle,sideOf(battle,defender));
  const PokemonType damageType=effectiveMoveType(battle,*move,attacker,&defender);
  if(defenderAbilityApplies(attacker,defender,"COLOR CHANGE")&&move->power&&move->id!=165U&&
     defender.currentHp&&!hasBattleType(defender,defenderEffects,damageType)){
    defenderEffects.typeOverrideActive=true;
    defenderEffects.type1=defenderEffects.type2=damageType;
    appendAbilityActivation(result,sideOf(battle,defender),defender);
  }
  if(!move->makesContact)return;
  if(defenderAbilityApplies(attacker,defender,"ROUGH SKIN")){
    const uint16_t hurt=std::max<uint16_t>(1U,attacker.maximumHp/16U);
    attacker.currentHp=hurt>=attacker.currentHp?0U:
        static_cast<uint16_t>(attacker.currentHp-hurt);
    appendAbilityActivation(result,sideOf(battle,defender),defender);
  }
  if(!attacker.currentHp)return;
  if(defenderAbilityApplies(attacker,defender,"CUTE CHARM")){
    if(!defender.currentHp)return;
    const uint32_t x=nextBattleRandom(battle);
    const bool wasAttracted=
        moveEffectsFor(battle,sideOf(battle,attacker)).attractedToUid!=0;
    if(x%3U==0U&&!abilityIs(attacker,"OBLIVIOUS")&&
       pokemonGender(attacker)>=0&&pokemonGender(defender)>=0&&
       pokemonGender(attacker)!=pokemonGender(defender))
      moveEffectsFor(battle,sideOf(battle,attacker)).attractedToUid=defender.uid;
    if(!wasAttracted&&
       moveEffectsFor(battle,sideOf(battle,attacker)).attractedToUid==defender.uid)
      appendAbilityActivation(result,sideOf(battle,defender),defender);
    return;
  }
  StatusCondition applied=StatusCondition::None;
  if(defenderAbilityApplies(attacker,defender,"EFFECT SPORE")){
    // FireRed checks 1/10 first, then chooses Sleep/Poison/Paralysis evenly.
    if(nextBattleRandom(battle)%10U)return;
    uint8_t roll=0;
    // The retail engine performs a second independent draw and rejects zero
    // from Random() & 3. Reusing bits from the activation roll changed both
    // the status distribution and the deterministic replay stream.
    do roll=static_cast<uint8_t>(nextBattleRandom(battle)&3U);while(!roll);
    --roll;
    applied=roll==0U?StatusCondition::Sleep:
        roll==1U?StatusCondition::Poison:StatusCondition::Paralysis;
  }else{
    StatusCondition candidate=StatusCondition::None;
    if(defenderAbilityApplies(attacker,defender,"STATIC"))candidate=StatusCondition::Paralysis;
    else if(defenderAbilityApplies(attacker,defender,"POISON POINT"))candidate=StatusCondition::Poison;
    else if(defenderAbilityApplies(attacker,defender,"FLAME BODY"))candidate=StatusCondition::Burn;
    // A contact hit against an unrelated Ability must not advance battle RNG.
    if(candidate==StatusCondition::None||nextBattleRandom(battle)%3U)return;
    applied=candidate;
  }
  // Contact Abilities roll before the ordinary status-immunity/existing-
  // condition checks in SetMoveEffect. Preserve that RNG ordering even when
  // the attacker is already burned, poisoned, asleep, etc.
  if(attacker.status!=StatusCondition::None)return;
  const DedicatedMoveEffectState& attackerEffects=moveEffectsFor(
      battle,sideOf(battle,attacker));
  const bool poisonType=hasBattleType(attacker,attackerEffects,PokemonType::Poison);
  const bool steelType=hasBattleType(attacker,attackerEffects,PokemonType::Steel);
  const bool fireType=hasBattleType(attacker,attackerEffects,PokemonType::Fire);
  if((applied==StatusCondition::Poison&&(poisonType||steelType||
      abilityIs(attacker,"IMMUNITY")))||
     (applied==StatusCondition::Burn&&(fireType||abilityIs(attacker,"WATER VEIL")))||
     (applied==StatusCondition::Paralysis&&abilityIs(attacker,"LIMBER"))||
     (applied==StatusCondition::Sleep&&(abilityIs(attacker,"INSOMNIA")||
      abilityIs(attacker,"VITAL SPIRIT")||
      ((!abilityIs(attacker,"SOUNDPROOF")&&!abilityIs(attacker,"CACOPHONY"))&&
       (battle.playerMoveEffects.uproar||
        battle.opponentMoveEffects[battle.opponentIndex].uproar)))))return;
  attacker.status=applied;
  if(applied==StatusCondition::Sleep){
    CombatVolatile& attackerVolatile=sideOf(battle,attacker)==BattleSide::Player?
        battle.playerVolatile:battle.opponentVolatiles[battle.opponentIndex];
    // EFFECT SPORE uses the same 2-5 turn sleep encoding as move-induced
    // sleep.  Leaving this counter at zero made the victim wake immediately.
    const uint32_t sleepRoll=nextBattleRandom(battle);
    attackerVolatile.sleepTurns=static_cast<uint8_t>(2U+(sleepRoll&3U));
  }
  if(applied!=StatusCondition::None)
    appendAbilityActivation(result,sideOf(battle,defender),defender);
  if(applied!=StatusCondition::None&&abilityIs(attacker,"SYNCHRONIZE")&&
     defender.status==StatusCondition::None){
    const StatusCondition reflected=applied==StatusCondition::BadlyPoisoned?
        StatusCondition::Poison:applied;
    const DedicatedMoveEffectState& effects=moveEffectsFor(
        battle,sideOf(battle,defender));
    const bool poisonBlocked=(reflected==StatusCondition::Poison)&&
        (hasBattleType(defender,effects,PokemonType::Poison)||
         hasBattleType(defender,effects,PokemonType::Steel)||
         abilityIs(defender,"IMMUNITY"));
    const bool burnBlocked=reflected==StatusCondition::Burn&&
        (hasBattleType(defender,effects,PokemonType::Fire)||
         abilityIs(defender,"WATER VEIL"));
    const bool paralysisBlocked=reflected==StatusCondition::Paralysis&&
        abilityIs(defender,"LIMBER");
    if(!poisonBlocked&&!burnBlocked&&!paralysisBlocked){
      appendAbilityActivation(result,sideOf(battle,attacker),attacker);
      defender.status=reflected;
      appendStatusChange(result,sideOf(battle,defender),defender.uid,
                         StatusCondition::None,reflected);
    }
  }
}

// Accuracy belongs to the move, not to its damage value. The previous engine
// only rolled accuracy inside calculateDamage(), which immediately returns
// zero for power-0 moves; the UI consequently called every status move a
// miss. Resolve the single hit roll once, then let both damaging and status
// effects share that outcome.
bool moveConnects(BattleState& battle, const FullMoveData& move,
                  const OwnedPokemon& attacker, const CombatVolatile& attackerVolatile,
                  const OwnedPokemon& defender, CombatVolatile& defenderVolatile) {
  if ((defenderAbilityApplies(attacker,defender,"SOUNDPROOF") ||
       defenderAbilityApplies(attacker,defender,"CACOPHONY")) && isSoundMove(move)) return false;
  // FireRed checks STATUS3_ALWAYS_HITS before the semi-invulnerable states
  // and does not consume it inside AccuracyCalcHelper.  The two-turn target
  // timer is decremented only by the end-turn phase.  Consuming the packed
  // flag here made a second internal accuracy query (and some called-move
  // paths) lose Mind Reader before the selected move had actually resolved.
  const uint8_t requiredSureHitOwner=
      sideOf(battle,attacker)==BattleSide::Player?1U:2U;
  if (std::strcmp(move.effect,"PARALYZE")==0) {
    // Unlike the other primary status scripts, Gen-III PARALYZE runs
    // `typecalc` before accuracy. This makes Thunder Wave fail on Ground and
    // Glare fail on Ghost unless Foresight/another identification effect has
    // removed that immunity.
    const PokemonType type=effectiveMoveType(battle,move,attacker,&defender);
    const DedicatedMoveEffectState& effects=
        moveEffectsFor(battle,sideOf(battle,defender));
    const uint16_t effectiveness=identifiedTypeMultiplier100(type,defender,effects);
    if(!effectiveness || (type==PokemonType::Ground&&
       defenderAbilityApplies(attacker,defender,"LEVITATE")))
      return false;
  }
  if(sureHitOwner(defenderVolatile)==requiredSureHitOwner)return true;
  // No Guard controls accuracy from either side and, unlike ordinary
  // always-hit moves, also reaches Fly/Dig/Dive/Bounce positions. Mold
  // No Guard is not marked breakable, so either battler's copy remains active
  // even when the attacker has Mold Breaker.
  const bool noGuard=abilityIs(attacker,"NO GUARD")||
      defenderAbilityApplies(attacker,defender,"NO GUARD");
  if(noGuard)return true;
  // Transform owns an explicit semi-invulnerable failure branch in its
  // battle script. Let it reach that handler so the result is "failed", not
  // an ordinary accuracy miss; all other moves use the usual hidden-target
  // reachability table here.
  if(defenderVolatile.chargingMove!=MoveId::None&&
     std::strcmp(move.effect,"TRANSFORM")!=0){
    const uint16_t hidden=static_cast<uint16_t>(defenderVolatile.chargingMove);
    const uint16_t attack=move.id;
    if((hidden==19U||hidden==340U)&&attack!=16U&&attack!=87U&&attack!=239U&&attack!=327U)
      return false;
    if(hidden==91U&&attack!=89U&&attack!=222U)return false;
    if(hidden==291U&&attack!=57U&&attack!=250U)return false;
  }
  // Self-recovery moves do not test their accuracy against the opponent's
  // evasion, BrightPowder or Sand Veil. SOFT-BOILED must either heal the user
  // or fail because its HP is already full; it can never "miss" the foe.
  if (std::strstr(move.effect,"RESTORE_HP") || std::strstr(move.effect,"SOFTBOILED") ||
      std::strstr(move.effect,"HEAL_HALF") || std::strstr(move.effect,"SYNTHESIS") ||
      std::strstr(move.effect,"MORNING_SUN") || std::strstr(move.effect,"MOONLIGHT") ||
      std::strcmp(move.effect,"REST") == 0)
    return true;
  if (std::strstr(move.effect, "OHKO")) {
    // OHKO moves do not use the ordinary accuracy pipeline in Gen III.
    // tryKO ignores accuracy/evasion stages, Compound Eyes, Hustle,
    // BrightPowder and Sand Veil, and tests (base accuracy + level delta)
    // with a 1..100 roll. No Guard is a later-generation adaptation, but it
    // still cannot bypass the level rule or Sturdy.
    const uint16_t chance=static_cast<uint16_t>(move.accuracy+attacker.level-defender.level);
    uint32_t roll = battle.rngState ? battle.rngState : 0xA341316CU;
    roll ^= roll << 13U; roll ^= roll >> 17U; roll ^= roll << 5U;
    battle.rngState = roll;
    return roll % 100U + 1U < chance;
  }
  if (!move.accuracy || std::strstr(move.effect, "ALWAYS_HIT") ||
      std::strcmp(move.effect,"VITAL_THROW")==0) return true;
  uint32_t accuracy = move.accuracy;
  // FireRed/Emerald resolve Thunder's weather rule before the ordinary
  // accuracy-stage calculation: rain guarantees the hit, while sun replaces
  // its base accuracy with 50. Cloud Nine and Air Lock suppress both rules.
  const bool suppressWeather=abilityIs(attacker,"CLOUD NINE")||abilityIs(attacker,"AIR LOCK")||
      abilityIs(defender,"CLOUD NINE")||abilityIs(defender,"AIR LOCK");
  if(std::strcmp(move.effect,"THUNDER")==0&&!suppressWeather){
    if(battle.weather==BattleWeather::Rain||battle.weather==BattleWeather::HeavyRain)
      return true;
    if(battle.weather==BattleWeather::Sun||battle.weather==BattleWeather::HarshSun)
      accuracy=50U;
  }
  const bool identified=moveEffectsFor(battle,sideOf(battle,defender)).identified;
  accuracy=accuracyStaged(accuracy,
      static_cast<int16_t>(attackerVolatile.accuracyStage)-
      (identified?0:static_cast<int16_t>(defenderVolatile.evasionStage)));
  // Preserve FireRed's operation order because every integer division floors:
  // stage ratio, Compound Eyes, Sand Veil/Hustle, then held-item evasion.
  if (abilityIs(attacker, "COMPOUND EYES")) accuracy = accuracy * 130U / 100U;
  if (!suppressWeather && battle.weather==BattleWeather::Sandstorm &&
      defenderAbilityApplies(attacker,defender,"SAND VEIL")) accuracy=accuracy*80U/100U;
  // Gen III checks the move's physical *type*, not whether it deals damage.
  // Consequently Hustle also lowers the accuracy of Normal/Fighting/etc.
  // status moves such as Sand-Attack. The damaging-only restriction belongs
  // to later physical/special-split games.
  if (isPhysicalType(effectiveMoveType(battle,move,attacker,&defender)) &&
      abilityIs(attacker, "HUSTLE")) accuracy = accuracy * 80U / 100U;
  // FireRed gives BrightPowder a 10% accuracy penalty, but Lax Incense only
  // 5%.  They share a visual role in the UI, not the same hold-effect value.
  if (heldIs(defender, defenderVolatile, HeldItem::BrightPowder))
    accuracy = accuracy * 90U / 100U;
  else if (heldIs(defender, defenderVolatile, HeldItem::LaxIncense))
    accuracy = accuracy * 95U / 100U;
  accuracy=std::min<uint32_t>(100U,accuracy);
  uint32_t roll = battle.rngState ? battle.rngState : 0xA341316CU;
  roll ^= roll << 13U; roll ^= roll >> 17U; roll ^= roll << 5U;
  battle.rngState = roll;
  return roll % 100U < accuracy;
}

enum class OhkoBlockReason : uint8_t { None, TargetHigherLevel, Sturdy };

OhkoBlockReason ohkoBlockReason(const FullMoveData* move,const OwnedPokemon& attacker,
                                const OwnedPokemon& defender){
  // tryKO runs after the accuracy command in FireRed. Mind Reader guarantees
  // that accuracy command, but deliberately cannot bypass the target being a
  // higher level or STURDY. Keep this out of moveConnects() so these cases
  // produce the move-effect failure path rather than the misleading
  // "attack missed" path.
  if(!move||!std::strstr(move->effect,"OHKO"))return OhkoBlockReason::None;
  if(defenderAbilityApplies(attacker,defender,"STURDY"))return OhkoBlockReason::Sturdy;
  if(attacker.level<defender.level)return OhkoBlockReason::TargetHigherLevel;
  return OhkoBlockReason::None;
}

void appendOhkoFailure(BattleActionResult& result,BattleSide targetSide,
                       const OwnedPokemon& target,MoveId move,
                       OhkoBlockReason reason){
  const BattleMoveEffect feedback=reason==OhkoBlockReason::Sturdy?
      BattleMoveEffect::OhkoBlockedBySturdy:
      reason==OhkoBlockReason::TargetHigherLevel?
      BattleMoveEffect::OhkoTargetHigherLevel:BattleMoveEffect::Failed;
  appendMoveEffect(result,targetSide,target.uid,move,feedback);
}

bool effectIs(const FullMoveData* move, const char* effect) {
  return move && effect && std::strcmp(move->effect, effect) == 0;
}

bool isSameTurnMultiHit(const FullMoveData* move) {
  return effectIs(move,"MULTI_HIT") || effectIs(move,"DOUBLE_HIT") ||
      effectIs(move,"TWINEEDLE") || effectIs(move,"TRIPLE_KICK") ||
      effectIs(move,"BEAT_UP");
}

bool printsMultiHitCount(const FullMoveData* move) {
  // Beat Up announces its contributing party members in FireRed instead of
  // using the generic "Hit X times!" footer.
  return isSameTurnMultiHit(move) && !effectIs(move,"BEAT_UP");
}

bool moveTargetsUser(const FullMoveData* move) {
  return move && (move->target == MoveTarget::User || effectIs(move,"MAGIC_COAT") ||
                  effectIs(move,"SNATCH"));
}

bool moveTargetsOpponent(const FullMoveData* move) {
  return move && !moveTargetsUser(move);
}

uint8_t pressurePpCost(const FullMoveData* move,const OwnedPokemon& opponent){
  if(!move)return 1U;
  // In singles, Pressure spends one additional PP only when the move can
  // affect the opposing side.  The previous unconditional deduction also
  // taxed Recover, Swords Dance, Baton Pass and Bide.  Perish Song and
  // Imprison use FireRed's explicit "count other side" Pressure commands.
  const bool checksOpponent=moveTargetsOpponent(move)||
      effectIs(move,"PERISH_SONG")||effectIs(move,"IMPRISON");
  return checksOpponent&&abilityIs(opponent,"PRESSURE")?2U:1U;
}

bool parentalBondEligible(const OwnedPokemon& pokemon,const FullMoveData* move){
  return move&&abilityIs(pokemon,"PARENTAL BOND")&&move->power&&
      move->target==MoveTarget::Selected&&!isSameTurnMultiHit(move)&&
      !effectIs(move,"FUTURE_SIGHT")&&!effectIs(move,"DOOM_DESIRE")&&
      !effectIs(move,"EXPLOSION");
}

bool pokemonKnowsMove(const OwnedPokemon& pokemon,MoveId move){
  if(move==MoveId::None)return false;
  for(uint8_t slot=0;slot<kMoveSlots;++slot)if(pokemon.moves[slot]==move)return true;
  return false;
}

int8_t pokemonGender(const OwnedPokemon& pokemon){
  const SpeciesData* species=findSpecies(pokemon.speciesId);
  if(!species||species->genderRatio==255U)return -1;
  if(species->genderRatio==0U)return 0;
  if(species->genderRatio==254U)return 1;
  return static_cast<uint8_t>(pokemon.personality&0xFFU)<species->genderRatio?1:0;
}

bool isProtectionMove(const FullMoveData* move) {
  return effectIs(move,"PROTECT") || effectIs(move,"ENDURE");
}

uint32_t dedicatedEffectRandom(BattleState& battle);

bool isChargingMove(const BattleState& battle,const FullMoveData* move,
                    const OwnedPokemon& user,const OwnedPokemon& target){
  if(!move)return false;
  const bool suppressWeather=abilityIs(user,"CLOUD NINE")||abilityIs(user,"AIR LOCK")||
      abilityIs(target,"CLOUD NINE")||abilityIs(target,"AIR LOCK");
  if(effectIs(move,"SOLAR_BEAM")&&!suppressWeather&&
     (battle.weather==BattleWeather::Sun||battle.weather==BattleWeather::HarshSun))return false;
  return std::strstr(move->effect,"SEMI_INVULNERABLE")||effectIs(move,"RAZOR_WIND")||
      effectIs(move,"SOLAR_BEAM")||effectIs(move,"SKULL_BASH")||
      effectIs(move,"SKY_ATTACK");
}

bool isMoveCaller(const FullMoveData* move){
  return effectIs(move,"METRONOME")||effectIs(move,"MIRROR_MOVE")||
      effectIs(move,"SLEEP_TALK")||effectIs(move,"NATURE_POWER")||
      effectIs(move,"ASSIST");
}

bool secondaryEffectAffectsUser(const FullMoveData* move){
  // These Gen-III damage effects encode their chance-based stage increase
  // with MOVE_EFFECT_AFFECTS_USER.  Shield Dust belongs to the defender and
  // must not suppress Metal Claw, Steel Wing, AncientPower or Silver Wind's
  // boost on the attacker.
  return effectIs(move,"ATTACK_UP_HIT")||effectIs(move,"DEFENSE_UP_HIT")||
      effectIs(move,"ALL_STATS_UP_HIT");
}

bool sheerForceApplies(const FullMoveData* move){
  if(!move||!move->power||!move->effectChance)return false;
  // Generation-III encodes several guaranteed primary mechanics in the same
  // `secondaryEffectChance` byte (Wrap, Thief, Pay Day, Uproar, recoil/stat
  // penalties). They are not Sheer Force effects. Keep an explicit audited
  // list of genuine additional effects instead of treating every non-zero
  // byte as eligible.
  static constexpr const char* kAdditionalEffects[]={
    "ACCURACY_DOWN_HIT","ALL_STATS_UP_HIT","ATTACK_DOWN_HIT",
    "ATTACK_UP_HIT","BLAZE_KICK","BURN_HIT","CONFUSE_HIT",
    "DEFENSE_DOWN_HIT","DEFENSE_UP_HIT","FLINCH_HIT",
    "FLINCH_MINIMIZE_HIT","FREEZE_HIT","PARALYZE_HIT","POISON_FANG",
    "POISON_HIT","POISON_TAIL","SECRET_POWER","SEMI_INVULNERABLE",
    "SKY_ATTACK","SNORE","SPECIAL_ATTACK_DOWN_HIT",
    "SPECIAL_DEFENSE_DOWN_HIT","SPEED_DOWN_HIT","THAW_HIT","THUNDER",
    "TRI_ATTACK","TWINEEDLE","TWISTER"
  };
  for(const char* effect:kAdditionalEffects)
    if(effectIs(move,effect))return true;
  return false;
}

bool isTwoTurnMove(const FullMoveData* move){
  return move&&(effectIs(move,"SKULL_BASH")||effectIs(move,"RAZOR_WIND")||
      effectIs(move,"SKY_ATTACK")||effectIs(move,"SOLAR_BEAM")||
      std::strstr(move->effect,"SEMI_INVULNERABLE")||effectIs(move,"BIDE"));
}

bool invalidForSleepTalkOrAssist(MoveId move){
  const FullMoveData* data=findFullMove(move);
  return !data||effectIs(data,"SLEEP_TALK")||effectIs(data,"ASSIST")||
      effectIs(data,"MIRROR_MOVE")||effectIs(data,"METRONOME");
}

bool forbiddenByMetronomeOrAssist(MoveId move){
  const FullMoveData* data=findFullMove(move);
  return !data||move==MoveId::Struggle||effectIs(data,"METRONOME")||
      effectIs(data,"SKETCH")||effectIs(data,"MIMIC")||
      effectIs(data,"COUNTER")||effectIs(data,"MIRROR_COAT")||
      effectIs(data,"PROTECT")||effectIs(data,"ENDURE")||
      effectIs(data,"DESTINY_BOND")||effectIs(data,"SLEEP_TALK")||
      effectIs(data,"THIEF")||effectIs(data,"FOLLOW_ME")||
      effectIs(data,"SNATCH")||effectIs(data,"HELPING_HAND")||
      effectIs(data,"COVET")||effectIs(data,"TRICK")||
      effectIs(data,"FOCUS_PUNCH");
}

bool mimicCopyableMove(MoveId move){
  const FullMoveData* data=findFullMove(move);
  return data&&move!=MoveId::Struggle&&!effectIs(data,"METRONOME")&&
      !effectIs(data,"SKETCH")&&!effectIs(data,"MIMIC");
}

bool mirrorMoveCallable(MoveId move){
  const FullMoveData* data=findFullMove(move);
  return move!=MoveId::None&&data&&data->mirrorMoveAffected;
}

bool callableMoveFor(const BattleState& battle,MoveId move,const FullMoveData* caller){
  const FullMoveData* data=findFullMove(move);
  if(!data||!caller)return false;
  if(effectIs(caller,"SLEEP_TALK"))
    return !invalidForSleepTalkOrAssist(move)&&!effectIs(data,"FOCUS_PUNCH")&&
        !effectIs(data,"UPROAR")&&!isTwoTurnMove(data);
  if(effectIs(caller,"ASSIST"))
    return !invalidForSleepTalkOrAssist(move)&&!forbiddenByMetronomeOrAssist(move);
  if(effectIs(caller,"METRONOME"))return !forbiddenByMetronomeOrAssist(move);
  if(effectIs(caller,"MIRROR_MOVE"))return mirrorMoveCallable(move);
  (void)battle;
  return false;
}

bool sleepTalkMoveAllowed(const BattleState& battle,
                          const PokemonCollection& collection,
                          const OwnedPokemon& user,
                          const CombatVolatile& userVolatile,
                          MoveId move,BattleSide side){
  const FullMoveData* data=findFullMove(move);
  if(!data)return false;
  // FireRed's trychoosesleeptalkmove feeds the candidate mask through
  // CheckMoveLimitations with every limitation enabled except PP.  In
  // particular, Sleep Talk must not bypass Disable, Torment, Taunt,
  // Imprison, Encore or Choice Band merely because the called move costs no
  // PP of its own.
  if(userVolatile.disabledMove==move)return false;
  const DedicatedMoveEffectState& effects=side==BattleSide::Player?
      battle.playerMoveEffects:battle.opponentMoveEffects[battle.opponentIndex];
  if(effects.tormented&&userVolatile.lastMoveUsed==move)return false;
  if(effects.tauntTurns&&data->power==0)return false;
  if(BattleEngine::moveIsImprisoned(battle,collection,move,side))return false;
  if(userVolatile.encoreMove!=MoveId::None&&userVolatile.encoreMove!=move)return false;
  if(heldIs(user,userVolatile,HeldItem::ChoiceBand)&&
     userVolatile.choiceMove!=MoveId::None&&userVolatile.choiceMove!=move)return false;
  return true;
}

MoveId resolveCalledMove(BattleState& battle,const PokemonCollection& collection,
                         const OwnedPokemon& user,const CombatVolatile& userVolatile,
                         const OwnedPokemon& target,const CombatVolatile& targetVolatile,
                         const FullMoveData* caller,BattleSide side){
  if(!caller)return MoveId::None;
  if(effectIs(caller,"MIRROR_MOVE"))
    return callableMoveFor(battle,targetVolatile.lastMoveUsed,caller)?
        targetVolatile.lastMoveUsed:MoveId::None;
  if(effectIs(caller,"NATURE_POWER")){
    // Exact FireRed sNaturePowerMoves table, indexed by gBattleTerrain.
    switch(battle.terrain){
      case BattleTerrain::Grass:return static_cast<MoveId>(78);       // STUN SPORE
      case BattleTerrain::LongGrass:return static_cast<MoveId>(75);  // RAZOR LEAF
      case BattleTerrain::Sand:return static_cast<MoveId>(89);       // EARTHQUAKE
      case BattleTerrain::Underwater:return static_cast<MoveId>(56); // HYDRO PUMP
      case BattleTerrain::Water:return static_cast<MoveId>(57);      // SURF
      case BattleTerrain::Pond:return static_cast<MoveId>(61);       // BUBBLEBEAM
      case BattleTerrain::Mountain:return static_cast<MoveId>(157);  // ROCK SLIDE
      case BattleTerrain::Cave:return static_cast<MoveId>(247);      // SHADOW BALL
      case BattleTerrain::Building:
      case BattleTerrain::Plain:return static_cast<MoveId>(129);     // SWIFT
    }
  }
  if(effectIs(caller,"SLEEP_TALK")){
    if(user.status!=StatusCondition::Sleep)return MoveId::None;
    bool legal[kMoveSlots]{};bool any=false;
    for(uint8_t slot=0;slot<kMoveSlots;++slot){
      const MoveId move=user.moves[slot];
      legal[slot]=move!=static_cast<MoveId>(caller->id)&&
          callableMoveFor(battle,move,caller)&&
          sleepTalkMoveAllowed(battle,collection,user,userVolatile,move,side);
      any=any||legal[slot];
    }
    if(!any)return MoveId::None;
    // trychoosesleeptalkmove samples an actual 0..3 move slot and retries
    // unusable slots. Choosing by modulo of a compacted candidate list gives
    // the same broad odds but a different deterministic Gen-III RNG stream.
    uint8_t slot=0;
    do slot=static_cast<uint8_t>(dedicatedEffectRandom(battle)&3U);
    while(!legal[slot]);
    return user.moves[slot];
  }
  MoveId candidates[32]{};uint8_t count=0;
  auto add=[&](MoveId move){
    if(move==MoveId::None||!callableMoveFor(battle,move,caller)||
       move==static_cast<MoveId>(caller->id))return;
    if(effectIs(caller,"SLEEP_TALK")&&
       !sleepTalkMoveAllowed(battle,collection,user,userVolatile,move,side))return;
    if(count<sizeof(candidates)/sizeof(candidates[0]))candidates[count++]=move;
  };
  if(effectIs(caller,"ASSIST")){
    if(side==BattleSide::Player){
      for(uint8_t party=0;party<kPartyCapacity;++party){
        const OwnedPokemon* member=CollectionLogic::find(collection,collection.party[party]);
        if(!member||member->uid==user.uid)continue;
        for(uint8_t slot=0;slot<kMoveSlots;++slot)add(member->moves[slot]);
      }
    }else{
      for(uint8_t member=0;member<battle.opponentCount;++member){
        if(member==battle.opponentIndex)continue;
        for(uint8_t slot=0;slot<kMoveSlots;++slot)add(battle.opponents[member].moves[slot]);
      }
    }
  }else if(effectIs(caller,"METRONOME")){
    // Cmd_metronome rolls (Random() & 0x1FF) + 1 and rejects values outside
    // MOVES_COUNT as well as the forbidden table. Do not replace this with a
    // modulo: it changes the deterministic RNG stream and biases move IDs.
    for(;;){
      const uint16_t raw=static_cast<uint16_t>((dedicatedEffectRandom(battle)&0x1FFU)+1U);
      if(raw>354U)continue;
      const MoveId rolled=static_cast<MoveId>(raw);
      if(callableMoveFor(battle,rolled,caller))return rolled;
    }
  }
  if(!count)return MoveId::None;
  if(effectIs(caller,"ASSIST")){
    // assistattackselect uses the low byte as a fixed-point fraction instead
    // of modulo. Preserve both the tiny distribution bias and exact replay.
    const uint8_t roll=static_cast<uint8_t>(dedicatedEffectRandom(battle)&0xFFU);
    return candidates[(static_cast<uint16_t>(roll)*count)>>8U];
  }
  return candidates[dedicatedEffectRandom(battle)%count];
}

bool copyLastMove(BattleState& battle,BattleActionResult& result,OwnedPokemon& user,
                  const CombatVolatile& targetVolatile,uint8_t slot,
                  const FullMoveData& copyMove,BattleSide side,bool permanent){
  // Both Mimic and Sketch use the target's last move, but FireRed rejects the
  // copy while the target is behind a Substitute.
  const MoveId copiedMove=targetVolatile.lastMoveUsed;
  const DedicatedMoveEffectState& userEffects=moveEffectsFor(battle,side);
  bool alreadyKnown=false;
  for(uint8_t knownSlot=0;knownSlot<kMoveSlots;++knownSlot){
    // Sketch ignores its own slot while checking whether the result is
    // already known; Mimic does not need that exception because its slot
    // still contains Mimic at this point.
    if(permanent&&knownSlot==slot)continue;
    if(user.moves[knownSlot]==copiedMove){alreadyKnown=true;break;}
  }
  const FullMoveData* copied=findFullMove(copiedMove);
  const bool legal=permanent?
      (copied&&copiedMove!=MoveId::Struggle&&!effectIs(copied,"SKETCH")):
      mimicCopyableMove(copiedMove);
  if(slot>=kMoveSlots||targetVolatile.substituteHp||userEffects.transformed||
     alreadyKnown||!legal){
    appendMoveEffect(result,side,user.uid,static_cast<MoveId>(copyMove.id),
                     BattleMoveEffect::Failed);return false;
  }
  if(side==BattleSide::Player&&!permanent&&!battle.playerMimicActive){
    battle.playerMimicActive=true;battle.playerMimicUid=user.uid;
    battle.playerMimicSlot=slot;battle.playerMimicOriginalMove=user.moves[slot];
    battle.playerMimicOriginalPp=user.movePp[slot];
  }
  user.moves[slot]=copiedMove;
  user.movePp[slot]=permanent?copied->pp:std::min<uint8_t>(5U,copied->pp);
  BattleEvent& copiedEvent=appendEvent(result,BattleEventType::MoveEffect,side,user.uid);
  copiedEvent.move=static_cast<MoveId>(copyMove.id);
  copiedEvent.value=static_cast<uint16_t>(BattleMoveEffect::MoveCopied);
  copiedEvent.after=static_cast<uint16_t>(copiedMove);return true;
}

bool protectionSucceeds(BattleState& battle, uint8_t& consecutiveUses,
                        bool opponentStillHasAction) {
  // FireRed's table is 1, 1/2, 1/4, 1/8 and remains capped at 1/8.
  const uint8_t shift = std::min<uint8_t>(3U, consecutiveUses);
  const uint16_t divisor = static_cast<uint16_t>(1U << shift);
  uint32_t roll = battle.rngState ? battle.rngState : 0xA341316CU;
  roll ^= roll << 13U; roll ^= roll >> 17U; roll ^= roll << 5U;
  battle.rngState = roll;
  const bool success = opponentStillHasAction &&
      (shift == 0U || roll % divisor == 0U);
  consecutiveUses = success ? static_cast<uint8_t>(std::min<uint8_t>(3U, consecutiveUses + 1U)) : 0U;
  return success;
}

bool sideAlreadyActed(const BattleActionResult& result, BattleSide side) {
  for (uint8_t index = 0; index < result.eventCount; ++index) {
    const BattleEvent& event = result.events[index];
    if (event.side != side) continue;
    if (event.type == BattleEventType::MoveUsed ||
        event.type == BattleEventType::CannotMove ||
        event.type == BattleEventType::ItemUsed ||
        event.type == BattleEventType::SwitchedIn)
      return true;
  }
  return false;
}

StatusCondition deterministicMoveStatus(const BattleState& battle,
                                         const FullMoveData* move) {
  if (!move) return StatusCondition::None;
  if (std::strstr(move->effect, "TOXIC") || effectIs(move, "POISON_FANG"))
    return StatusCondition::BadlyPoisoned;
  if (std::strstr(move->effect, "POISON")) return StatusCondition::Poison;
  if (std::strstr(move->effect, "PARALYZE") ||
      std::strcmp(move->effect, "THUNDER") == 0 || move->id == 340U ||
      (effectIs(move, "SECRET_POWER") &&
       (battle.terrain == BattleTerrain::Building ||
        battle.terrain == BattleTerrain::Plain)))
    return StatusCondition::Paralysis;
  if (std::strstr(move->effect, "BURN") ||
      std::strcmp(move->effect, "WILL_O_WISP") == 0 ||
      std::strcmp(move->effect, "BLAZE_KICK") == 0 ||
      effectIs(move, "THAW_HIT"))
    return StatusCondition::Burn;
  if (std::strstr(move->effect, "FREEZE")) return StatusCondition::Frozen;
  if (effectIs(move, "SLEEP")) return StatusCondition::Sleep;
  if (effectIs(move, "SECRET_POWER") && battle.terrain == BattleTerrain::Grass)
    return StatusCondition::Poison;
  if (effectIs(move, "SECRET_POWER") && battle.terrain == BattleTerrain::LongGrass)
    return StatusCondition::Sleep;
  if (std::strcmp(move->effect, "TWINEEDLE") == 0)
    return StatusCondition::Poison;
  return StatusCondition::None;
}

bool moveUsesStatusEffectRoll(const BattleState& battle,const FullMoveData* move){
  return move&&(deterministicMoveStatus(battle,move)!=StatusCondition::None||
      effectIs(move,"TRI_ATTACK"));
}

bool primaryStatusConflicts(const BattleState& battle, const FullMoveData* move,
                            const OwnedPokemon& target,
                            StatusCondition& attempted) {
  attempted = StatusCondition::None;
  if (!move || move->power || !moveTargetsOpponent(move)) return false;
  attempted = deterministicMoveStatus(battle, move);
  return attempted != StatusCondition::None &&
         target.status != StatusCondition::None;
}

BattleMoveEffect existingStatusFeedback(StatusCondition attempted,
                                        StatusCondition existing) {
  if (attempted == StatusCondition::Sleep && existing == StatusCondition::Sleep)
    return BattleMoveEffect::AlreadyAsleep;
  if ((attempted == StatusCondition::Poison ||
       attempted == StatusCondition::BadlyPoisoned) &&
      (existing == StatusCondition::Poison ||
       existing == StatusCondition::BadlyPoisoned))
    return BattleMoveEffect::AlreadyPoisoned;
  if (attempted == StatusCondition::Paralysis &&
      existing == StatusCondition::Paralysis)
    return BattleMoveEffect::AlreadyParalyzed;
  if (attempted == StatusCondition::Burn && existing == StatusCondition::Burn)
    return BattleMoveEffect::AlreadyBurned;
  return BattleMoveEffect::Failed;
}

bool enemyCanChooseHealingItem(const BattleState& battle,
                               const OwnedPokemon& opponent,
                               const CombatVolatile& volatileState,
                               const BideState& bide,
                               const DedicatedMoveEffectState& effects) {
  // Item and switch actions are selected ahead of attacks in FireRed. A
  // battler committed to Bide, a charge/recharge turn or a locked rampage
  // does not reach normal action selection and therefore cannot use an item.
  return battle.kind != BattleKind::Wild && battle.opponentItemUses &&
         opponent.currentHp &&
         opponent.currentHp * 3U <= opponent.maximumHp && !bide.turns &&
         !volatileState.recharging &&
         volatileState.chargingMove == MoveId::None &&
         !effects.lockedMoveTurns;
}

bool moveRequirementMet(const FullMoveData* move, const OwnedPokemon& target,
                         const CombatVolatile& targetVolatile) {
  // These are effect-code rules from the original battle engine, not guesses
  // based on a move's display name or prose description.
  if (effectIs(move, "DREAM_EATER"))
    return target.status == StatusCondition::Sleep && !targetVolatile.substituteHp;
  if (effectIs(move, "NIGHTMARE"))
    return target.status == StatusCondition::Sleep && !targetVolatile.substituteHp;
  // FireRed's trycopyability command rejects only a missing Ability and
  // Wonder Guard. Copying the same Ability is valid, and Role Play is not
  // blocked by Substitute or Protect (its original move flags are zero).
  if (effectIs(move, "ROLE_PLAY"))
    return target.abilityId != 0 && !abilityIs(target, "WONDER GUARD");
  if (effectIs(move, "TRICK")) return targetVolatile.substituteHp == 0;
  if (effectIs(move,"PAIN_SPLIT")) return targetVolatile.substituteHp == 0;
  return true;
}

bool moveUserRequirementMet(const BattleState& battle, const FullMoveData* move,
                            const OwnedPokemon& user,
                            const CombatVolatile& userVolatile,
                            const OwnedPokemon& target) {
  // In FireRed SNORE is the explicit exception to the normal sleep action
  // lock. It can execute while the user remains asleep and fails if the user
  // is already awake (including the turn on which the sleep counter expires).
  if (effectIs(move, "SNORE")) return user.status == StatusCondition::Sleep;
  if (effectIs(move, "SLEEP_TALK")) return user.status == StatusCondition::Sleep;
  if (effectIs(move, "REST"))
    return user.currentHp < user.maximumHp && !abilityIs(user,"INSOMNIA") &&
           !abilityIs(user,"VITAL SPIRIT") &&
           ((abilityIs(user,"SOUNDPROOF")||abilityIs(user,"CACOPHONY"))||
            (!battle.playerMoveEffects.uproar&&
             !battle.opponentMoveEffects[battle.opponentIndex].uproar));
  if (move && (std::strstr(move->effect,"RESTORE_HP") || std::strstr(move->effect,"SOFTBOILED") ||
      std::strstr(move->effect,"SYNTHESIS") || std::strstr(move->effect,"MORNING_SUN") ||
      std::strstr(move->effect,"MOONLIGHT")))
    return user.currentHp < user.maximumHp;
  if (effectIs(move,"STOCKPILE")) return userVolatile.stockpileCount < 3U;
  if (effectIs(move,"SPIT_UP")||effectIs(move,"SWALLOW"))
    return userVolatile.stockpileCount > 0U;
  if(effectIs(move,"BELLY_DRUM"))
    return user.currentHp>std::max<uint16_t>(1U,user.maximumHp/2U)&&
           userVolatile.attackStage<kMaximumBattleStatStage;
  if(effectIs(move,"ENDEAVOR"))return target.currentHp>user.currentHp;
  if(effectIs(move,"EXPLOSION"))
    return !abilityIs(user,"DAMP")&&
        !defenderAbilityApplies(user,target,"DAMP");
  return true;
}

bool isHalfDrainEffect(const FullMoveData* move) {
  return effectIs(move, "ABSORB") || effectIs(move, "DREAM_EATER");
}

void applyHalfDrain(const FullMoveData* move, uint16_t damage,
                    OwnedPokemon& user, const OwnedPokemon& target,
                    BattleActionResult& result, BattleSide userSide) {
  if (!isHalfDrainEffect(move) || !damage) return;
  const uint16_t drained = std::max<uint16_t>(1U, damage / 2U);
  const uint16_t before=user.currentHp;
  // Liquid Ooze reverses every HP-draining move in FireRed, including
  // Dream Eater; it is not limited to the moves whose effect is ABSORB.
  if (defenderAbilityApplies(user,target,"LIQUID OOZE")) {
    appendAbilityActivation(result,userSide==BattleSide::Player?
        BattleSide::Opponent:BattleSide::Player,target);
    user.currentHp = drained >= user.currentHp ? 0U
        : static_cast<uint16_t>(user.currentHp - drained);
  } else
    user.currentHp = std::min<uint16_t>(user.maximumHp,
        static_cast<uint16_t>(user.currentHp + drained));
  appendHpChange(result,userSide,user.uid,before,user.currentHp);
}

uint16_t applyMoveRecoil(const FullMoveData* move, uint16_t damageDealt,
                         OwnedPokemon& user, BattleActionResult& result,
                         BattleSide side) {
  if (!move || !damageDealt ||
      (!effectIs(move, "RECOIL") && !effectIs(move, "DOUBLE_EDGE"))) return 0;
  // Rock Head suppresses ordinary recoil in FireRed, but not Struggle's
  // typeless fallback recoil. TAKE DOWN/SUBMISSION/STRUGGLE use one quarter;
  // DOUBLE-EDGE and VOLT TACKLE use one third in Generation III.
  if (move->id != static_cast<uint16_t>(MoveId::Struggle) && abilityIs(user, "ROCK HEAD"))
    return 0;
  const uint16_t divisor = effectIs(move, "DOUBLE_EDGE") ? 3U : 4U;
  const uint16_t recoil = std::max<uint16_t>(1U, damageDealt / divisor);
  user.currentHp = recoil >= user.currentHp ? 0U
                                            : static_cast<uint16_t>(user.currentHp - recoil);
  appendMoveEffect(result, side, user.uid, static_cast<MoveId>(move->id),
                   BattleMoveEffect::Recoil);
  return recoil;
}

void applyCrashDamage(const FullMoveData* move,bool connected,uint16_t projectedDamage,
                      uint16_t effectiveness,uint16_t targetMaximumHp,
                      OwnedPokemon& user,BattleActionResult& result,BattleSide side){
  if(!move||connected||!effectIs(move,"RECOIL_IF_MISS")||!user.currentHp||
     !effectiveness||!projectedDamage)return;
  const uint16_t before=user.currentHp;
  // Gen III runs the ordinary damage formula without a critical, halves that
  // result, then caps it at half of the *target's* maximum HP.
  const uint16_t crash=std::min<uint16_t>(
      std::max<uint16_t>(1U,projectedDamage/2U),
      std::max<uint16_t>(1U,targetMaximumHp/2U));
  user.currentHp=crash>=before?0U:static_cast<uint16_t>(before-crash);
  appendMoveEffect(result,side,user.uid,static_cast<MoveId>(move->id),BattleMoveEffect::Recoil);
  appendHpChange(result,side,user.uid,before,user.currentHp);
}

void refreshLevelMoves(OwnedPokemon& pokemon) {
  MoveId desired[4]{}; movesForLevel(pokemon.speciesId, pokemon.level, desired);
  MoveId oldMoves[4]{};uint8_t oldPp[4]{};uint8_t oldPpUps[4]{};
  for(uint8_t old=0;old<4;++old){oldMoves[old]=pokemon.moves[old];oldPp[old]=pokemon.movePp[old];oldPpUps[old]=CollectionLogic::ppUpCount(pokemon,old);}
  pokemon.legacyCareCounterReserved&=~0xFFUL;
  for (uint8_t i = 0; i < 4; ++i) {
    pokemon.moves[i]=desired[i];
    for(uint8_t old=0;old<4;++old)if(oldMoves[old]==desired[i]){
      pokemon.legacyCareCounterReserved|=static_cast<uint32_t>(oldPpUps[old])<<(i*2U);break;
    }
  }
  for(uint8_t i=0;i<4;++i){
    bool preserved=false;
    for(uint8_t old=0;old<4;++old)if(oldMoves[old]==desired[i]){pokemon.movePp[i]=oldPp[old];preserved=true;break;}
    if(!preserved)pokemon.movePp[i]=CollectionLogic::maximumMovePp(pokemon,i);
  }
}
bool changeStage(int8_t& stage, int8_t amount) {
  const int16_t requested=static_cast<int16_t>(stage)+amount;
  const int8_t changed=static_cast<int8_t>(std::max<int16_t>(
      kMinimumBattleStatStage,std::min<int16_t>(kMaximumBattleStatStage,requested)));
  if(changed==stage)return false;
  stage=changed;
  return true;
}
void applyStageEffect(BattleState& battle, const OwnedPokemon& sourcePokemon,
                      const char* effect, CombatVolatile& self,
                      CombatVolatile& target,const OwnedPokemon& targetPokemon,
                      BattleActionResult* result=nullptr) {
  // Composite and side-condition effects have dedicated handlers below. If
  // they also pass through keyword matching, DEFENSE CURL and BULK UP apply
  // one of their boosts twice.
  if (!effect || std::strcmp(effect,"DEFENSE_CURL")==0 || std::strcmp(effect,"BULK_UP")==0 ||
      std::strcmp(effect,"CALM_MIND")==0 || std::strcmp(effect,"COSMIC_POWER")==0 ||
      std::strcmp(effect,"DRAGON_DANCE")==0 || std::strcmp(effect,"LIGHT_SCREEN")==0 ||
      std::strcmp(effect,"REFLECT")==0 || std::strcmp(effect,"MIST")==0) return;
  const bool down = std::strstr(effect,"DOWN") != nullptr; CombatVolatile& v = down ? target : self;
  const int8_t amount = (std::strstr(effect,"_2") ? 2 : 1) * (down ? -1 : 1);
  const bool opposingDrop=down&&
      sideOf(battle,sourcePokemon)!=sideOf(battle,targetPokemon);
  if (opposingDrop) {
    const BattleSide side = sideOf(battle, targetPokemon);
    const uint8_t mistTurns = side == BattleSide::Player
        ? battle.playerMistTurns : battle.opponentMistTurns;
    if (mistTurns) return;
  }
  if(opposingDrop&&(defenderAbilityApplies(sourcePokemon,targetPokemon,"CLEAR BODY")||
                    defenderAbilityApplies(sourcePokemon,targetPokemon,"WHITE SMOKE"))){
    if(result)appendAbilityActivation(*result,sideOf(battle,targetPokemon),targetPokemon);
    return;
  }
  if(opposingDrop&&std::strstr(effect,"ATTACK")&&!std::strstr(effect,"SPECIAL")&&
     defenderAbilityApplies(sourcePokemon,targetPokemon,"HYPER CUTTER")){
    if(result)appendAbilityActivation(*result,sideOf(battle,targetPokemon),targetPokemon);
    return;
  }
  if(opposingDrop&&std::strstr(effect,"ACCURACY")&&
     defenderAbilityApplies(sourcePokemon,targetPokemon,"KEEN EYE")){
    if(result)appendAbilityActivation(*result,sideOf(battle,targetPokemon),targetPokemon);
    return;
  }
  if (std::strstr(effect,"SPECIAL_ATTACK") || std::strstr(effect,"SP_ATTACK")) changeStage(v.spAttackStage,amount);
  else if (std::strstr(effect,"SPECIAL_DEFENSE") || std::strstr(effect,"SP_DEFENSE")) changeStage(v.spDefenseStage,amount);
  else if (std::strstr(effect,"ATTACK")) changeStage(v.attackStage,amount);
  else if (std::strstr(effect,"DEFENSE")) changeStage(v.defenseStage,amount);
  else if (std::strstr(effect,"SPEED")) changeStage(v.speedStage,amount);
  else if (std::strstr(effect,"ACCURACY")) changeStage(v.accuracyStage,amount);
  else if (std::strstr(effect,"EVASION")) changeStage(v.evasionStage,amount);
}
uint16_t recoveryMoveAmount(const BattleState& battle, const OwnedPokemon& user,
                            const OwnedPokemon& other, const char* effect) {
  if (!effect) return 0;
  if (std::strstr(effect,"MORNING_SUN") || std::strstr(effect,"SYNTHESIS") ||
      std::strstr(effect,"MOONLIGHT")) {
    const bool suppressed = abilityIs(user,"CLOUD NINE") || abilityIs(user,"AIR LOCK") ||
        abilityIs(other,"CLOUD NINE") || abilityIs(other,"AIR LOCK");
    if (!suppressed && (battle.weather == BattleWeather::Sun ||
                        battle.weather == BattleWeather::HarshSun))
      return std::max<uint16_t>(1U, static_cast<uint16_t>(user.maximumHp * 2U / 3U));
    if (!suppressed && battle.weather != BattleWeather::Clear)
      return std::max<uint16_t>(1U, static_cast<uint16_t>(user.maximumHp / 4U));
  }
  return std::max<uint16_t>(1U, static_cast<uint16_t>(user.maximumHp / 2U));
}

bool canBeCuredByBell(const FullMoveData& move,const OwnedPokemon& pokemon){
  // Aromatherapy ignores Soundproof. Heal Bell is a sound move and the
  // original party-status command explicitly skips every Soundproof member,
  // including benched Pokemon.
  return std::strcmp(move.name,"HEAL BELL")!=0||
      (!abilityIs(pokemon,"SOUNDPROOF")&&!abilityIs(pokemon,"CACOPHONY"));
}

uint8_t beatUpDamageHits(BattleState& battle,const PokemonCollection& collection,
                         BattleSide side,const OwnedPokemon& user,
                         const CombatVolatile& userVolatile,
                         const OwnedPokemon& target,uint16_t damage[5],
                         uint16_t effectiveness[5],bool critical[5]){
  const SpeciesData* targetSpecies=findSpecies(target.speciesId);
  const SpeciesData* userSpecies=findSpecies(user.speciesId);
  if(!targetSpecies||!userSpecies)return 0U;
  const uint16_t typeEffectiveness=typeMultiplier100(PokemonType::Dark,*targetSpecies);
  if(!typeEffectiveness)return 0U;
  uint8_t count=0;
  auto strike=[&](const OwnedPokemon* member){
    if(count>=5U||!member||!member->currentHp||
       member->status!=StatusCondition::None)return;
    const SpeciesData* species=findSpecies(member->speciesId);if(!species)return;
    uint32_t strikeDamage=((2U*member->level/5U+2U)*10U*species->baseAttack/
        std::max<uint8_t>(1U,targetSpecies->baseDefense)/50U)+2U;
    // trydobeatup builds the base damage from the participating member's
    // species Attack and level. critcalc still uses the active battler's
    // critical stage/item/ability, and adjustnormaldamage then applies the
    // active user's Dark STAB, type matchup and an independent 85..100 roll.
    static constexpr uint8_t kCriticalDivisors[]{16U,8U,4U,3U,2U};
    uint8_t criticalStage=userVolatile.criticalStage;
    if(heldIs(user,userVolatile,HeldItem::ScopeLens))++criticalStage;
    if((user.speciesId==83U&&heldIs(user,userVolatile,HeldItem::Stick))||
       (user.speciesId==113U&&heldIs(user,userVolatile,HeldItem::LuckyPunch)))
      criticalStage=static_cast<uint8_t>(criticalStage+2U);
    criticalStage=std::min<uint8_t>(criticalStage,4U);
    const bool isCritical=!defenderAbilityApplies(user,target,"BATTLE ARMOR")&&
        !defenderAbilityApplies(user,target,"SHELL ARMOR")&&
        dedicatedEffectRandom(battle)%kCriticalDivisors[criticalStage]==0U;
    if(isCritical)strikeDamage*=2U;
    if(userSpecies->type1==PokemonType::Dark||userSpecies->type2==PokemonType::Dark)
      strikeDamage=strikeDamage*3U/2U;
    strikeDamage=strikeDamage*typeEffectiveness/100U;
    strikeDamage=strikeDamage*(85U+dedicatedEffectRandom(battle)%16U)/100U;
    damage[count]=static_cast<uint16_t>(std::min<uint32_t>(65535U,
        std::max<uint32_t>(1U,strikeDamage)));
    effectiveness[count]=typeEffectiveness;
    critical[count]=isCritical;
    ++count;
  };
  if(side==BattleSide::Player){
    for(uint8_t slot=0;slot<kPartyCapacity;++slot)
      strike(CollectionLogic::find(collection,collection.party[slot]));
  }else{
    for(uint8_t index=0;index<battle.opponentCount;++index)strike(&battle.opponents[index]);
  }
  return count;
}
bool applyMoveWeather(BattleState& battle,const char* effect){
  BattleWeather requested=BattleWeather::Clear;
  if(std::strstr(effect,"RAIN_DANCE"))requested=BattleWeather::Rain;
  else if(std::strstr(effect,"SUNNY_DAY"))requested=BattleWeather::Sun;
  else if(std::strcmp(effect,"SANDSTORM")==0)requested=BattleWeather::Sandstorm;
  else if(std::strcmp(effect,"HAIL")==0)requested=BattleWeather::Hail;
  else return false;
  // Ordinary weather moves cannot replace Primordial Sea, Desolate Land or
  // Delta Stream.  Only another strong-weather Ability may do that.
  if(battle.weather==BattleWeather::HeavyRain||
     battle.weather==BattleWeather::HarshSun||
     battle.weather==BattleWeather::StrongWinds)return false;
  const bool already=(requested==BattleWeather::Rain&&
      (battle.weather==BattleWeather::Rain||battle.weather==BattleWeather::HeavyRain))||
      (requested==BattleWeather::Sun&&
       (battle.weather==BattleWeather::Sun||battle.weather==BattleWeather::HarshSun))||
      battle.weather==requested;
  if(already)return false;
  battle.weather=requested;battle.weatherTurns=5;return true;
}
bool isWeatherMoveEffect(const char* effect){
  return effect&&(std::strcmp(effect,"RAIN_DANCE")==0||std::strcmp(effect,"SUNNY_DAY")==0||
      std::strcmp(effect,"SANDSTORM")==0||std::strcmp(effect,"HAIL")==0);
}

uint32_t dedicatedEffectRandom(BattleState& battle){
  uint32_t x=battle.rngState?battle.rngState:0xA341316CU;
  x^=x<<13U;x^=x>>17U;x^=x<<5U;battle.rngState=x;return x;
}

bool forceSwitchLevelCheck(BattleState& battle,const OwnedPokemon& user,
                           const OwnedPokemon& target){
  if(user.level>=target.level)return true;
  const uint32_t roll=dedicatedEffectRandom(battle)&0xFFU;
  return ((roll*(static_cast<uint32_t>(user.level)+target.level)>>8U)+1U)>
      target.level/4U;
}

bool forceOpponentSwitch(BattleState& battle,PokemonCollection& collection,
                         BattleActionResult& result){
  uint8_t candidates[kOpponentTeamCapacity]{};uint8_t count=0;
  for(uint8_t i=0;i<battle.opponentCount;++i)
    if(i!=battle.opponentIndex&&battle.opponents[i].currentHp)candidates[count++]=i;
  if(!count)return false;
  const uint8_t active=battle.opponentIndex;
  const uint8_t incoming=candidates[dedicatedEffectRandom(battle)%count];
  OwnedPokemon& outgoing=battle.opponents[active];
  if(abilityIs(outgoing,"NATURAL CURE"))outgoing.status=StatusCondition::None;
  restoreOpponentTransform(battle);
  if(outgoing.speciesId==351U)outgoing.form=0;
  restoreOpponentTemporaryAbility(outgoing);
  setEscapePrevented(battle.playerVolatile,false);
  if(sureHitOwner(battle.playerVolatile)==2U)setSureHitOwner(battle.playerVolatile,0);
  std::swap(battle.opponents[active],battle.opponents[incoming]);
  std::swap(battle.opponentVolatiles[active],battle.opponentVolatiles[incoming]);
  std::swap(battle.opponentMoveEffects[active],battle.opponentMoveEffects[incoming]);
  std::swap(battle.opponentBides[active],battle.opponentBides[incoming]);
  std::swap(battle.opponentEncoreTurns[active],battle.opponentEncoreTurns[incoming]);
  std::swap(battle.opponentNightmares[active],battle.opponentNightmares[incoming]);
  clearVolatileKeepHeld(battle.opponentVolatiles[active]);
  clearVolatileKeepHeld(battle.opponentVolatiles[incoming]);
  clearMoveEffectsOnSwitch(battle.opponentMoveEffects[active]);
  clearMoveEffectsOnSwitch(battle.opponentMoveEffects[incoming]);
  battle.opponentMoveEffects[active].enteredTurn=battle.turn;
  battle.opponentBides[active]=battle.opponentBides[incoming]=BideState{};
  battle.opponentEncoreTurns[active]=battle.opponentEncoreTurns[incoming]=0;
  battle.opponentNightmares[active]=battle.opponentNightmares[incoming]=false;
  appendEvent(result,BattleEventType::SwitchedIn,BattleSide::Opponent,
              battle.opponents[active].uid);
  applySpikesOnEntry(battle,battle.opponents[active],BattleSide::Opponent,result);
  BattleEngine::applyEntryAbilities(battle,collection,BattleEntryScope::Opponent,&result);
  return true;
}

bool forcePlayerSwitch(BattleState& battle,PokemonCollection& collection,
                       BattleActionResult& result){
  OwnedPokemon* outgoing=CollectionLogic::find(collection,battle.playerUid);
  if(!outgoing)return false;
  uint32_t candidates[kPartyCapacity]{};uint8_t count=0;
  for(uint8_t slot=0;slot<kPartyCapacity;++slot){
    OwnedPokemon* candidate=CollectionLogic::active(collection,slot);
    if(candidate&&candidate->uid!=outgoing->uid&&candidate->currentHp&&
       !candidate->recoverySecondsRemaining)candidates[count++]=candidate->uid;
  }
  if(!count)return false;
  const uint32_t incomingUid=candidates[dedicatedEffectRandom(battle)%count];
  OwnedPokemon* incoming=CollectionLogic::find(collection,incomingUid);
  if(!incoming)return false;
  restorePlayerBattleForm(battle,collection);
  outgoing=CollectionLogic::find(collection,battle.playerUid);
  for(auto& state:battle.opponentVolatiles){
    if(sureHitOwner(state)==1U)setSureHitOwner(state,0);
    setEscapePrevented(state,false);
  }
  storePlayerHeldItemState(battle,*outgoing);
  storePlayerMoveEffectState(battle,*outgoing);
  battle.playerUid=incomingUid;
  loadPlayerHeldItemState(battle,*incoming);
  loadPlayerMoveEffectState(battle,*incoming);
  battle.playerMoveEffects.enteredTurn=battle.turn;
  battle.playerNightmare=false;
  applySpikesOnEntry(battle,*incoming,BattleSide::Player,result);
  appendEvent(result,BattleEventType::SwitchedIn,BattleSide::Player,incomingUid);
  BattleEngine::applyEntryAbilities(battle,collection,BattleEntryScope::Player,&result);
  return true;
}

PokemonType terrainPokemonType(BattleTerrain terrain){
  switch(terrain){
    case BattleTerrain::Grass:
    case BattleTerrain::LongGrass:return PokemonType::Grass;
    case BattleTerrain::Sand:return PokemonType::Ground;
    case BattleTerrain::Underwater:
    case BattleTerrain::Water:
    case BattleTerrain::Pond:return PokemonType::Water;
    case BattleTerrain::Mountain:
    case BattleTerrain::Cave:return PokemonType::Rock;
    case BattleTerrain::Building:
    case BattleTerrain::Plain:return PokemonType::Normal;
  }
  return PokemonType::Normal;
}

BattleTerrain initialBattleTerrain(BattleKind kind,const OwnedPokemon& opponent){
  if(kind!=BattleKind::Wild)return BattleTerrain::Plain;
  const SpeciesData* species=findSpecies(opponent.speciesId);
  if(!species)return BattleTerrain::Plain;
  const PokemonType type=species->type1;
  if(type==PokemonType::Water||type==PokemonType::Ice)return BattleTerrain::Water;
  if(type==PokemonType::Grass||type==PokemonType::Bug)return BattleTerrain::Grass;
  if(type==PokemonType::Ground)return BattleTerrain::Sand;
  if(type==PokemonType::Rock)return BattleTerrain::Mountain;
  return BattleTerrain::Plain;
}

bool applyDedicatedMoveEffect(BattleState& battle,BattleActionResult& result,
    const FullMoveData& move,OwnedPokemon& user,CombatVolatile& userVolatile,
    DedicatedMoveEffectState& userEffects,OwnedPokemon& target,
    CombatVolatile& targetVolatile,DedicatedMoveEffectState& targetEffects,
    BattleSide userSide){
  const BattleSide targetSide=userSide==BattleSide::Player?BattleSide::Opponent:BattleSide::Player;
  auto failed=[&](){appendMoveEffect(result,userSide,user.uid,static_cast<MoveId>(move.id),BattleMoveEffect::Failed);};
  auto effect=[&](BattleSide side,uint32_t uid,BattleMoveEffect value){
    appendMoveEffect(result,side,uid,static_cast<MoveId>(move.id),value);
  };
  if(effectIs(&move,"TELEPORT")){
    const bool trapped=userVolatile.trappedTurns||escapePrevented(userVolatile)||
        trappedByAbility(user,userEffects,target)||userEffects.ingrained;
    if(battle.kind!=BattleKind::Wild||trapped){failed();return true;}
    effect(userSide,user.uid,BattleMoveEffect::Teleported);
    battle.active=false;battle.outcome=BattleOutcome::Escaped;return true;
  }
  if(effectIs(&move,"CONVERSION")){
    // FireRed does not compact the legal types into a new list. It first
    // treats the first MOVE_NONE as the end of the moveset, then repeatedly
    // samples one of the four *real slots* with Random() & 3 until it finds a
    // usable type. Keeping those rejection draws matters for duplicate-type
    // weighting and for deterministic battles/PvP replays.
    uint8_t validMoves=0;
    while(validMoves<kMoveSlots&&user.moves[validMoves]!=MoveId::None)++validMoves;
    auto conversionType=[&](uint8_t slot){
      const FullMoveData* known=findFullMove(user.moves[slot]);
      if(!known)return PokemonType::Normal;
      // Curse is TYPE_MYSTERY in the original move table. Conversion treats
      // that as Ghost for a Ghost user and Normal for every other user.
      return known->id==174U&&
          hasBattleType(user,userEffects,PokemonType::Ghost)?
          PokemonType::Ghost:known->type;
    };
    bool possible=false;
    for(uint8_t slot=0;slot<validMoves;++slot){
      if(!hasBattleType(user,userEffects,conversionType(slot))){possible=true;break;}
    }
    if(!possible){failed();return true;}
    uint8_t selectedSlot=0;PokemonType selectedType=PokemonType::Normal;
    do{
      do{selectedSlot=static_cast<uint8_t>(dedicatedEffectRandom(battle)&
          (kMoveSlots-1U));}while(selectedSlot>=validMoves);
      selectedType=conversionType(selectedSlot);
    }while(hasBattleType(user,userEffects,selectedType));
    userEffects.typeOverrideActive=true;
    userEffects.type1=userEffects.type2=selectedType;
    BattleEvent& changed=appendEvent(result,BattleEventType::MoveEffect,userSide,user.uid);
    changed.move=static_cast<MoveId>(move.id);changed.value=static_cast<uint16_t>(BattleMoveEffect::TypeChanged);
    changed.after=static_cast<uint16_t>(userEffects.type1);return true;
  }
  if(effectIs(&move,"CONVERSION_2")){
    if(userEffects.lastLandedMove==MoveId::None){failed();return true;}
    const FullMoveData* previous=findFullMove(userEffects.lastLandedMove);
    // FireRed rejects Conversion 2 while the battler that last hit the user
    // is still in the charging half of that same two-turn move.
    if(previous&&isTwoTurnMove(previous)&&targetVolatile.chargingMove!=MoveId::None){
      failed();return true;
    }
    const PokemonType landedType=userEffects.lastLandedType;
    PokemonType candidates[17]{};uint8_t count=0;
    for(uint8_t raw=0;raw<17;++raw){
      const PokemonType candidate=static_cast<PokemonType>(raw);
      SpeciesData temporary{};temporary.type1=temporary.type2=candidate;
      if(typeMultiplier100(landedType,temporary)>50U||
         hasBattleType(user,userEffects,candidate))continue;
      candidates[count++]=candidate;
    }
    if(!count){failed();return true;}
    userEffects.typeOverrideActive=true;
    userEffects.type1=userEffects.type2=candidates[dedicatedEffectRandom(battle)%count];
    BattleEvent& changed=appendEvent(result,BattleEventType::MoveEffect,userSide,user.uid);
    changed.move=static_cast<MoveId>(move.id);changed.value=static_cast<uint16_t>(BattleMoveEffect::TypeChanged);
    changed.after=static_cast<uint16_t>(userEffects.type1);return true;
  }
  if(effectIs(&move,"DESTINY_BOND")){
    userEffects.destinyBond=true;effect(userSide,user.uid,BattleMoveEffect::DestinyBondSet);return true;
  }
  if(effectIs(&move,"PERISH_SONG")){
    bool affected=false;
    if(!abilityIs(user,"SOUNDPROOF")&&!abilityIs(user,"CACOPHONY")&&
       !userEffects.perishTurns){userEffects.perishTurns=4;affected=true;}
    if(!defenderAbilityApplies(user,target,"SOUNDPROOF")&&
       !defenderAbilityApplies(user,target,"CACOPHONY")&&
       !targetEffects.perishTurns){targetEffects.perishTurns=4;affected=true;}
    if(!affected)failed();else effect(userSide,user.uid,BattleMoveEffect::PerishSongSet);
    return true;
  }
  if(effectIs(&move,"FOCUS_ENERGY")){
    // Gen III stores Focus Energy as a battle-local volatile flag.  It adds
    // two critical-ratio stages, can be Baton Passed, and a repeated use
    // fails.  An explicit event is required because criticalStage is not a
    // conventional stat stage and is intentionally absent from the generic
    // stat/volatile feedback snapshots.
    if(userVolatile.criticalStage>=2U){failed();return true;}
    userVolatile.criticalStage=2U;
    effect(userSide,user.uid,BattleMoveEffect::FocusEnergySet);
    return true;
  }
  if(effectIs(&move,"LOCK_ON")){
    if(targetVolatile.substituteHp){failed();return true;}
    armSureHit(targetVolatile,userSide==BattleSide::Player?1U:2U);
    effect(userSide,user.uid,BattleMoveEffect::SureHitSet);return true;
  }
  if(effectIs(&move,"CURSE")){
    if(hasBattleType(user,userEffects,PokemonType::Ghost)){
      if(targetVolatile.substituteHp||isCursed(targetVolatile)){failed();return true;}
      setCursed(targetVolatile,true);
      const uint16_t cost=std::max<uint16_t>(1U,user.maximumHp/2U);
      user.currentHp=cost>=user.currentHp?0U:static_cast<uint16_t>(user.currentHp-cost);
      effect(targetSide,target.uid,BattleMoveEffect::Cursed);
      return true;
    }
    if(userVolatile.speedStage<=kMinimumBattleStatStage&&
       userVolatile.attackStage>=kMaximumBattleStatStage&&
       userVolatile.defenseStage>=kMaximumBattleStatStage){failed();return true;}
    changeStage(userVolatile.speedStage,-1);
    changeStage(userVolatile.attackStage,1);
    changeStage(userVolatile.defenseStage,1);
    return true;
  }
  if(effectIs(&move,"MEAN_LOOK")){
    if(targetVolatile.substituteHp||escapePrevented(targetVolatile)){
      failed();return true;
    }
    setEscapePrevented(targetVolatile,true);return true;
  }
  if(effectIs(&move,"DISABLE")){
    const uint8_t slot=moveSlotFor(target,targetVolatile.lastMoveUsed,false);
    if(targetVolatile.substituteHp||targetVolatile.disabledMove!=MoveId::None||
       slot>=kMoveSlots||!target.movePp[slot]){failed();return true;}
    targetVolatile.disabledMove=targetVolatile.lastMoveUsed;
    setDisableTurns(targetVolatile,
        static_cast<uint8_t>(2U+(dedicatedEffectRandom(battle)&3U)));
    return true;
  }
  if(effectIs(&move,"DEFENSE_CURL")){
    // setdefensecurlbit precedes the ordinary +1 stage command. The bit is
    // therefore set even when Defense is already maxed and the move reports
    // failure.
    userEffects.defenseCurl=true;
    if(userVolatile.defenseStage>=kMaximumBattleStatStage)failed();
    return true;
  }
  if(effectIs(&move,"MINIMIZE")){
    // setminimize has the same ordering: retain STATUS3_MINIMIZED even when
    // the following Evasion increase cannot be applied.
    userEffects.minimized=true;
    if(userVolatile.evasionStage>=kMaximumBattleStatStage)failed();
    return true;
  }
  if(effectIs(&move,"CHARGE")){
    // Generation III Charge only arms the next Electric attack. The Sp. Def
    // raise was added in Generation IV and must not leak into FireRed rules.
    userEffects.chargeTurns=2;
    effect(userSide,user.uid,BattleMoveEffect::Charged);return true;
  }
  if(effectIs(&move,"WISH")){
    uint16_t& due=userSide==BattleSide::Player?battle.playerWishDueTurn:battle.opponentWishDueTurn;
    // The original wish counter starts at two, is decremented at the end of
    // the setup turn and heals at the end of the following turn. `turn` is
    // incremented before resolving an action here, hence due = turn + 1.
    if(due){failed();return true;}due=static_cast<uint16_t>(battle.turn+1U);
    effect(userSide,user.uid,BattleMoveEffect::WishSet);return true;
  }
  if(effectIs(&move,"INGRAIN")){
    if(userEffects.ingrained){failed();return true;}userEffects.ingrained=true;
    effect(userSide,user.uid,BattleMoveEffect::Ingrained);return true;
  }
  if(effectIs(&move,"RECYCLE")){
    if(userEffects.recyclableItem==HeldItem::None||heldItemFor(user,userVolatile)!=HeldItem::None){failed();return true;}
    setBattleHeldItem(userVolatile,userEffects.recyclableItem);
    BattleEvent& recycled=appendEvent(result,BattleEventType::MoveEffect,userSide,user.uid);
    recycled.move=static_cast<MoveId>(move.id);recycled.value=static_cast<uint16_t>(BattleMoveEffect::Recycled);
    recycled.after=static_cast<uint16_t>(userEffects.recyclableItem);
    userEffects.recyclableItem=HeldItem::None;return true;
  }
  if(effectIs(&move,"IMPRISON")){
    bool shared=false;
    for(uint8_t userSlot=0;userSlot<kMoveSlots&&!shared;++userSlot)
      for(uint8_t targetSlot=0;targetSlot<kMoveSlots;++targetSlot)
        if(user.moves[userSlot]!=MoveId::None&&
           user.moves[userSlot]==target.moves[targetSlot])shared=true;
    if(userEffects.imprisoned||!shared){failed();return true;}
    userEffects.imprisoned=true;
    effect(userSide,user.uid,BattleMoveEffect::Imprisoned);return true;
  }
  if(effectIs(&move,"GRUDGE")){
    if(userEffects.grudge){failed();return true;}
    userEffects.grudge=true;effect(userSide,user.uid,BattleMoveEffect::GrudgeSet);return true;
  }
  if(effectIs(&move,"CAMOUFLAGE")){
    if(hasBattleType(user,userEffects,battle.terrainType)){failed();return true;}
    userEffects.typeOverrideActive=true;userEffects.type1=userEffects.type2=battle.terrainType;
    BattleEvent& changed=appendEvent(result,BattleEventType::MoveEffect,userSide,user.uid);
    changed.move=static_cast<MoveId>(move.id);changed.value=static_cast<uint16_t>(BattleMoveEffect::TypeChanged);
    changed.after=static_cast<uint16_t>(battle.terrainType);return true;
  }
  if(effectIs(&move,"MUD_SPORT")){
    // Gen III stores Mud Sport on the user, even though its damage halving
    // applies to the whole field.  The other battler having used it does not
    // make this use fail; only repeating it with the same battler does.
    if(userEffects.mudSport){failed();return true;}
    userEffects.mudSport=true;effect(userSide,user.uid,BattleMoveEffect::MudSportSet);return true;
  }
  if(effectIs(&move,"WATER_SPORT")){
    // Water Sport follows the same attacker-local setup rule as Mud Sport.
    if(userEffects.waterSport){failed();return true;}
    userEffects.waterSport=true;effect(userSide,user.uid,BattleMoveEffect::WaterSportSet);return true;
  }
  if(effectIs(&move,"FORESIGHT")){
    // Cmd_setforesight does not reject an already identified target in
    // FireRed/Emerald. The move still succeeds and replays its message.
    targetEffects.identified=true;
    effect(targetSide,target.uid,BattleMoveEffect::Identified);return true;
  }
  if(effectIs(&move,"SPITE")){
    const uint8_t slot=moveSlotFor(target,targetVolatile.lastMoveUsed,false);
    // Cmd_tryspiteppreduce explicitly fails at 0 or 1 PP.  If the deduction
    // reaches zero, FireRed also cancels the target's current multi-turn move.
    if(slot>=kMoveSlots||target.movePp[slot]<=1U){failed();return true;}
    const uint8_t before=target.movePp[slot];
    const uint8_t amount=static_cast<uint8_t>(2U+dedicatedEffectRandom(battle)%4U);
    target.movePp[slot]=static_cast<uint8_t>(before-std::min<uint8_t>(before,amount));
    BattleEvent& reduced=appendEvent(result,BattleEventType::MoveEffect,targetSide,target.uid);
    reduced.move=static_cast<MoveId>(move.id);
    reduced.value=static_cast<uint16_t>(BattleMoveEffect::PpReduced);
    reduced.before=before;reduced.after=target.movePp[slot];
    if(!target.movePp[slot]){
      BideState& targetBide=targetSide==BattleSide::Player?battle.playerBide:
          battle.opponentBides[battle.opponentIndex];
      cancelMultiTurnMoves(targetEffects,targetVolatile,targetBide,result,
                           targetSide,target.uid);
    }
    return true;
  }
  if(effectIs(&move,"SKILL_SWAP")){
    // Cmd_tryswapabilities only rejects ABILITY_NONE when both battlers have
    // no Ability. Swapping a real Ability with an empty slot is legal.
    if((!user.abilityId&&!target.abilityId)||abilityIs(user,"WONDER GUARD")||
       abilityIs(target,"WONDER GUARD")){failed();return true;}
    // The player's persistent Ability is restored on switch/battle end. NPC
    // and PvP opponent records already live only inside BattleState.
    OwnedPokemon& local=userSide==BattleSide::Player?user:target;
    if(!battle.playerAbilityTraced&&!battle.playerTransformed){
      battle.playerAbilityTraced=true;battle.playerPreTraceAbilityId=local.abilityId;
    }
    std::swap(user.abilityId,target.abilityId);
    // FireRed announces the Ability exchange before Forecast reacts to the
    // newly-held Ability/weather combination.
    effect(userSide,user.uid,BattleMoveEffect::AbilitiesSwapped);
    cureConditionForbiddenByAbility(user,userVolatile,userEffects,userSide,&result);
    cureConditionForbiddenByAbility(target,targetVolatile,targetEffects,targetSide,&result);
    if(userSide==BattleSide::Player)
      refreshForecastPair(battle,user,userEffects,target,targetEffects,&result);
    else
      refreshForecastPair(battle,target,targetEffects,user,userEffects,&result);
    return true;
  }
  if(effectIs(&move,"TAUNT")){
    if(targetEffects.tauntTurns){failed();return true;}
    // Gen III writes two. The setup turn's end-turn pass consumes the first
    // tick; adding an artificial third tick made Taunt last one turn too long.
    targetEffects.tauntTurns=2;
    effect(targetSide,target.uid,BattleMoveEffect::Taunted);return true;
  }
  if(effectIs(&move,"TORMENT")){
    if(targetEffects.tormented){failed();return true;}
    targetEffects.tormented=true;
    effect(targetSide,target.uid,BattleMoveEffect::Tormented);return true;
  }
  if(effectIs(&move,"YAWN")){
    const uint8_t safeguard=targetSide==BattleSide::Player?battle.playerSafeguardTurns:
        battle.opponentSafeguardTurns;
    const bool uproarKeepsAwake=
        !defenderAbilityApplies(user,target,"SOUNDPROOF")&&
        !defenderAbilityApplies(user,target,"CACOPHONY")&&
        (battle.playerMoveEffects.uproar||
         battle.opponentMoveEffects[battle.opponentIndex].uproar);
    if(targetVolatile.substituteHp||target.status!=StatusCondition::None||
       targetEffects.yawnTurns||safeguard||uproarKeepsAwake||
       defenderAbilityApplies(user,target,"INSOMNIA")||
       defenderAbilityApplies(user,target,"VITAL SPIRIT")){
      failed();return true;
    }
    targetEffects.yawnTurns=2;
    effect(targetSide,target.uid,BattleMoveEffect::Drowsy);return true;
  }
  if(effectIs(&move,"ATTRACT")){
    const int8_t userGender=pokemonGender(user),targetGender=pokemonGender(target);
    if(userGender<0||targetGender<0||userGender==targetGender||
       targetEffects.attractedToUid||targetVolatile.substituteHp||
       defenderAbilityApplies(user,target,"OBLIVIOUS")){failed();return true;}
    targetEffects.attractedToUid=user.uid;
    effect(targetSide,target.uid,BattleMoveEffect::Infatuated);return true;
  }
  if(effectIs(&move,"MEMENTO")){
    // Memento is not Explosion: Damp does not stop it. The user faints and
    // both offensive stages are lowered unless Substitute or an Ability
    // blocks them. It fails without fainting only when an unprotected target
    // already has both stages at -6 (Cmd_trymemento in FireRed).
    if(!targetVolatile.substituteHp&&
       targetVolatile.attackStage<=kMinimumBattleStatStage&&
       targetVolatile.spAttackStage<=kMinimumBattleStatStage){failed();return true;}
    if(!targetVolatile.substituteHp){
      // The retail script performs two ordinary statbuffchange commands.
      // Consequently Mist and the target's stat-loss Abilities are checked
      // independently for Attack and Sp. Atk; Memento itself still faints
      // the user when either (or both) drops are blocked.
      applyStageEffect(battle,user,"ATTACK_DOWN_2",userVolatile,
                       targetVolatile,target,&result);
      applyStageEffect(battle,user,"SPECIAL_ATTACK_DOWN_2",userVolatile,
                       targetVolatile,target,&result);
    }
    user.currentHp=0;return true;
  }
  if(effectIs(&move,"FOLLOW_ME")||effectIs(&move,"HELPING_HAND")||
     effectIs(&move,"SPLASH")){
    // Pokegochi battles are strictly singles. Follow Me and Helping Hand
    // have no legal ally/second target; Splash keeps FireRed's explicit
    // "nothing happened" result rather than silently succeeding.
    failed();return true;
  }
  if(effectIs(&move,"MAGIC_COAT")){
    // trysetmagiccoat fails when this is the final action of the turn.
    if(sideAlreadyActed(result,targetSide)){failed();return true;}
    userEffects.magicCoat=true;
    effect(userSide,user.uid,BattleMoveEffect::MagicCoatSet);return true;
  }
  if(effectIs(&move,"SNATCH")){
    // trysetsnatch has the same "acting last" failure rule.
    if(sideAlreadyActed(result,targetSide)){failed();return true;}
    userEffects.snatch=true;
    effect(userSide,user.uid,BattleMoveEffect::SnatchSet);return true;
  }
  if(effectIs(&move,"SPIKES")){
    uint8_t& layers=targetSide==BattleSide::Player?battle.playerSpikesLayers:
        battle.opponentSpikesLayers;
    if(layers>=3U){failed();return true;}
    ++layers;
    BattleEvent& set=appendEvent(result,BattleEventType::MoveEffect,targetSide,target.uid);
    set.move=static_cast<MoveId>(move.id);set.value=static_cast<uint16_t>(BattleMoveEffect::SpikesSet);
    set.after=layers;return true;
  }
  if(effectIs(&move,"PAY_DAY")){
    // MOVE_EFFECT_PAYDAY only adds money when the attacker belongs to the
    // player's side.  gPaydayMoney is a u16 in FireRed and saturates at
    // 0xFFFF on overflow.
    if(userSide==BattleSide::Player){
      battle.payDayMoney=std::min<uint32_t>(0xFFFFU,
          battle.payDayMoney+static_cast<uint32_t>(user.level)*5U);
      effect(userSide,user.uid,BattleMoveEffect::CoinsScattered);
    }
    return true;
  }
  if(effectIs(&move,"COUNTER")){
    if(!userEffects.lastDamageReceived||!userEffects.lastDamageWasPhysical)failed();
    return true;
  }
  if(effectIs(&move,"MIRROR_COAT")){
    if(!userEffects.lastDamageReceived||userEffects.lastDamageWasPhysical)failed();
    return true;
  }
  if(effectIs(&move,"RAGE")){
    userEffects.rageActive=true;return true;
  }
  if(effectIs(&move,"ROLLOUT")){
    if(!userEffects.lockedMoveTurns){
      userEffects.lockedMove=static_cast<MoveId>(move.id);
      userEffects.lockedMoveTurns=5;userEffects.rolloutCount=0;
    }
    userEffects.rolloutCount=static_cast<uint8_t>(
        std::min<uint8_t>(5U,userEffects.rolloutCount+1U));
    if(!--userEffects.lockedMoveTurns){
      userEffects.lockedMove=MoveId::None;userEffects.rolloutCount=0;
    }
    return true;
  }
  if(effectIs(&move,"RAMPAGE")||effectIs(&move,"UPROAR")){
    const bool isUproar=effectIs(&move,"UPROAR");
    if(!userEffects.lockedMoveTurns){
      userEffects.lockedMove=static_cast<MoveId>(move.id);
      userEffects.lockedMoveTurns=static_cast<uint8_t>(2U+
          dedicatedEffectRandom(battle)%(isUproar?4U:2U));
      userEffects.uproar=isUproar;
      if(isUproar)effect(userSide,user.uid,BattleMoveEffect::UproarStarted);
    }
    if(isUproar){
      const StatusCondition userBefore=user.status,targetBefore=target.status;
      if(user.status==StatusCondition::Sleep&&!abilityIs(user,"SOUNDPROOF")&&
         !abilityIs(user,"CACOPHONY")){
        user.status=StatusCondition::None;userVolatile.sleepTurns=0;
      }
      if(target.status==StatusCondition::Sleep&&
         !defenderAbilityApplies(user,target,"SOUNDPROOF")&&
         !defenderAbilityApplies(user,target,"CACOPHONY")){
        target.status=StatusCondition::None;targetVolatile.sleepTurns=0;
      }
      appendStatusChange(result,userSide,user.uid,userBefore,user.status);
      appendStatusChange(result,targetSide,target.uid,targetBefore,target.status);
    }
    if(userEffects.lockedMoveTurns&&!--userEffects.lockedMoveTurns){
      userEffects.lockedMove=MoveId::None;
      if(isUproar){userEffects.uproar=false;effect(userSide,user.uid,BattleMoveEffect::UproarEnded);}
      else if(!abilityIs(user,"OWN TEMPO")){
        userVolatile.confusionTurns=static_cast<uint8_t>(2U+dedicatedEffectRandom(battle)%4U);
        effect(userSide,user.uid,BattleMoveEffect::RampageEnded);
      }
    }
    return true;
  }
  return false;
}

void resolveFaintVows(BattleActionResult& result,BattleSide faintedSide,
    OwnedPokemon& fainted,DedicatedMoveEffectState& faintedEffects,
    OwnedPokemon& attacker,MoveId finishingMove,uint8_t finishingMoveSlot,
    uint16_t finishingDamage){
  if(faintedEffects.grudge&&finishingMoveSlot<kMoveSlots&&
     attacker.moves[finishingMoveSlot]==finishingMove)
    attacker.movePp[finishingMoveSlot]=0;
  faintedEffects.grudge=false;
  // Innards Out returns the HP actually lost to the finishing hit.  Resolve it
  // before Destiny Bond so the journal always shows the direct retaliation
  // before a possible bond knockout.
  if(abilityIs(fainted,"INNARDS OUT")&&attacker.currentHp&&finishingDamage){
    const uint16_t before=attacker.currentHp;
    attacker.currentHp=finishingDamage>=attacker.currentHp?0U:
        static_cast<uint16_t>(attacker.currentHp-finishingDamage);
    appendAbilityActivation(result,faintedSide,fainted);
    const BattleSide attackerSide=faintedSide==BattleSide::Player?
        BattleSide::Opponent:BattleSide::Player;
    appendHpChange(result,attackerSide,attacker.uid,before,attacker.currentHp);
    if(!attacker.currentHp)appendFaintedOnce(result,attackerSide,attacker);
  }
  if(!faintedEffects.destinyBond||!attacker.currentHp)return;
  faintedEffects.destinyBond=false;
  // FireRed presents the knocked-out Destiny Bond user first, then announces
  // that it is taking the attacker down.  Recording that order here also
  // prevents trainer-roster advancement from replacing the fainted battler
  // on screen before the bond has resolved.
  appendFaintedOnce(result,faintedSide,fainted);
  appendMoveEffect(result,faintedSide,fainted.uid,static_cast<MoveId>(194),
                   BattleMoveEffect::DestinyBondTriggered);
  const BattleSide attackerSide=faintedSide==BattleSide::Player?BattleSide::Opponent:BattleSide::Player;
  const uint16_t before=attacker.currentHp;attacker.currentHp=0;
  appendHpChange(result,attackerSide,attacker.uid,before,0);
  appendFaintedOnce(result,attackerSide,attacker);
}
}

namespace {
bool validTrainerTarget(const EncounterCharges& charges) {
  const uint32_t maximum = EncounterCharges::kMinimumRechargeSeconds +
      EncounterCharges::kRechargeWindowSeconds;
  return charges.rechargeTargetSeconds >= EncounterCharges::kMinimumRechargeSeconds &&
      charges.rechargeTargetSeconds <= maximum;
}

void rollTrainerTarget(EncounterCharges& charges) {
  // Derive a fresh deterministic roll from values already persisted in the
  // legacy-sized charge record.  This avoids changing the save layout while
  // giving every VS Seeker recovery its own 30-60 minute duration.
  uint32_t x = (static_cast<uint32_t>(charges.rechargeTargetSeconds) << 16U) ^
      charges.rechargeProgressSeconds ^ (static_cast<uint32_t>(charges.available) << 8U) ^
      0xC25A7E19U;
  x ^= x << 13U; x ^= x >> 17U; x ^= x << 5U;
  charges.rechargeTargetSeconds = static_cast<uint16_t>(
      EncounterCharges::kMinimumRechargeSeconds +
      x % (EncounterCharges::kRechargeWindowSeconds + 1U));
}

void normalizeTrainerTarget(EncounterCharges& charges) {
  if (!validTrainerTarget(charges)) rollTrainerTarget(charges);
}
}

void BattleEngine::setPermanentExperienceBoost(bool active){
  gPermanentExperienceBoost=active;
}
bool BattleEngine::permanentExperienceBoost(){return gPermanentExperienceBoost;}

void EncounterLogic::advance(EncounterCharges& charges, uint32_t elapsedSeconds) {
  if (charges.available > EncounterCharges::kMaximum) charges.available = EncounterCharges::kMaximum;
  if (charges.available >= EncounterCharges::kMaximum) {
    charges.available = EncounterCharges::kMaximum;
    charges.rechargeProgressSeconds = 0;
    return;
  }
  normalizeTrainerTarget(charges);
  while (elapsedSeconds && charges.available < EncounterCharges::kMaximum) {
    const uint32_t target = charges.rechargeTargetSeconds;
    const uint32_t remaining = target > charges.rechargeProgressSeconds
        ? target - charges.rechargeProgressSeconds : 0U;
    if (elapsedSeconds < remaining) {
      charges.rechargeProgressSeconds += elapsedSeconds;
      break;
    }
    elapsedSeconds -= remaining;
    charges.rechargeProgressSeconds = 0;
    ++charges.available;
    rollTrainerTarget(charges);
  }
  if (charges.available >= EncounterCharges::kMaximum) {
    charges.available = EncounterCharges::kMaximum;
    charges.rechargeProgressSeconds = 0;
  }
}

bool EncounterLogic::consumeManualCharge(EncounterCharges& charges) {
  if (charges.available == 0) return false;
  const bool wasFull = charges.available == EncounterCharges::kMaximum;
  --charges.available;
  if (wasFull) {
    charges.rechargeProgressSeconds = 0;
    rollTrainerTarget(charges);
  } else {
    normalizeTrainerTarget(charges);
  }
  return true;
}

uint32_t EncounterLogic::secondsUntilNext(const EncounterCharges& charges) {
  if (charges.available >= EncounterCharges::kMaximum) return 0;
  if (!validTrainerTarget(charges) || charges.rechargeProgressSeconds >= charges.rechargeTargetSeconds)
    return EncounterCharges::kMinimumRechargeSeconds;
  return charges.rechargeTargetSeconds - charges.rechargeProgressSeconds;
}

void EncounterLogic::advanceWild(WildEncounterClock& clock, uint32_t elapsedSeconds) {
  // This legacy record now powers the five-charge Pokemon Center. Each charge
  // returns after one fixed hour; wild encounters no longer consume it.
  if (clock.available > WildEncounterClock::kMaximum) clock.available = 0;
  if (clock.pending) {
    if (!clock.available) clock.available = 1;
    clock.pending = false;
  }

  auto nextTarget = [&clock]() {
    uint32_t x = clock.rngState ? clock.rngState : 0x91E10DA5U;
    x ^= x << 13U; x ^= x >> 17U; x ^= x << 5U;
    clock.rngState = x;
    clock.targetSeconds = WildEncounterClock::kMinimumSeconds +
        x % (WildEncounterClock::kWindowSeconds + 1U);
  };
  const uint32_t maximumTarget = WildEncounterClock::kMinimumSeconds +
      WildEncounterClock::kWindowSeconds;
  if (clock.targetSeconds < WildEncounterClock::kMinimumSeconds ||
      clock.targetSeconds > maximumTarget) nextTarget();

  // At capacity the timer stops; spending one charge starts the next period.
  while (elapsedSeconds && clock.available < WildEncounterClock::kMaximum) {
    const uint32_t remaining = clock.targetSeconds > clock.elapsedSeconds
        ? clock.targetSeconds - clock.elapsedSeconds : 0;
    if (elapsedSeconds < remaining) {
      clock.elapsedSeconds += elapsedSeconds;
      break;
    }
    elapsedSeconds -= remaining;
    clock.elapsedSeconds = 0;
    ++clock.available;
    nextTarget();
  }
  if (clock.available >= WildEncounterClock::kMaximum) {
    clock.available = WildEncounterClock::kMaximum;
    clock.elapsedSeconds = 0;
  }
}

bool EncounterLogic::consumeWildCharge(WildEncounterClock& clock) {
  if (clock.available == 0) return false;
  const bool wasFull = clock.available >= WildEncounterClock::kMaximum;
  --clock.available;
  // Only a full stock has no running timer. Spending at 3/5 or 4/5 must not
  // discard partial progress toward the next charge; otherwise frequent
  // Center use can keep postponing recovery indefinitely.
  const uint32_t maximumTarget = WildEncounterClock::kMinimumSeconds +
      WildEncounterClock::kWindowSeconds;
  const bool invalidTarget = clock.targetSeconds < WildEncounterClock::kMinimumSeconds ||
      clock.targetSeconds > maximumTarget;
  if (wasFull || invalidTarget) {
    clock.elapsedSeconds = 0;
    uint32_t x = clock.rngState ? clock.rngState : 0x91E10DA5U;
    x ^= x << 13U; x ^= x >> 17U; x ^= x << 5U;
    clock.rngState = x;
    clock.targetSeconds = WildEncounterClock::kMinimumSeconds +
        x % (WildEncounterClock::kWindowSeconds + 1U);
  }
  clock.pending = false;
  return true;
}

uint32_t EncounterLogic::secondsUntilNextWild(const WildEncounterClock& clock) {
  if (clock.available >= WildEncounterClock::kMaximum) return 0;
  const uint32_t maximumTarget = WildEncounterClock::kMinimumSeconds +
      WildEncounterClock::kWindowSeconds;
  if (clock.targetSeconds < WildEncounterClock::kMinimumSeconds ||
      clock.targetSeconds > maximumTarget || clock.elapsedSeconds >= clock.targetSeconds)
    return WildEncounterClock::kMinimumSeconds;
  return clock.targetSeconds - clock.elapsedSeconds;
}

uint32_t BattleEngine::random(BattleState& battle) {
  uint32_t x = battle.rngState ? battle.rngState : 0xA341316CU;
  x ^= x << 13U; x ^= x >> 17U; x ^= x << 5U;
  battle.rngState = x;
  return x;
}

void applySecretPowerNonStatus(BattleState& battle,BattleActionResult& result,
    const OwnedPokemon& source,CombatVolatile& sourceVolatile,
    OwnedPokemon& target,CombatVolatile& targetVolatile,BattleSide targetSide){
  // Exact FireRed Cmd_getsecretpowereffect mapping. Grass/long grass and
  // building/plain produce major status and are resolved by applyMoveStatus.
  switch(battle.terrain){
    case BattleTerrain::Sand:
      applyStageEffect(battle,source,"ACCURACY_DOWN_HIT",sourceVolatile,
                       targetVolatile,target,&result);break;
    case BattleTerrain::Underwater:
      applyStageEffect(battle,source,"DEFENSE_DOWN_HIT",sourceVolatile,
                       targetVolatile,target,&result);break;
    case BattleTerrain::Water:
      applyStageEffect(battle,source,"ATTACK_DOWN_HIT",sourceVolatile,
                       targetVolatile,target,&result);break;
    case BattleTerrain::Pond:
      applyStageEffect(battle,source,"SPEED_DOWN_HIT",sourceVolatile,
                       targetVolatile,target,&result);break;
    case BattleTerrain::Mountain:{
      if(defenderAbilityApplies(source,target,"OWN TEMPO")||targetVolatile.confusionTurns)break;
      const uint8_t safeguard=targetSide==BattleSide::Player?
          battle.playerSafeguardTurns:battle.opponentSafeguardTurns;
      if(safeguard){
        appendMoveEffect(result,targetSide,target.uid,static_cast<MoveId>(290),
                         BattleMoveEffect::SafeguardBlocked);
      }else targetVolatile.confusionTurns=static_cast<uint8_t>(
          2U+dedicatedEffectRandom(battle)%4U);
      break;
    }
    case BattleTerrain::Cave:
      if(!sideAlreadyActed(result,targetSide)&&
         !defenderAbilityApplies(source,target,"INNER FOCUS"))
        targetVolatile.flinched=true;
      break;
    default:break;
  }
}

uint16_t BattleEngine::confusionSelfDamage(BattleState& battle,
    const OwnedPokemon& pokemon,const CombatVolatile& volatileState) {
  // FireRed/Emerald call CalculateBaseDamage(user, user, POUND, power=40)
  // and then adjustnormaldamage2.  It is a typeless physical hit: Attack and
  // Defense stages, physical stat modifiers, burn and the random 85..100%
  // factor apply, but STAB, type effectiveness and critical hits do not.
  uint32_t attack=staged(battleCalculatedStat(battle,pokemon,PokemonStat::Attack),
                         volatileState.attackStage);
  uint32_t defense=std::max<uint32_t>(1U,staged(
      battleCalculatedStat(battle,pokemon,PokemonStat::Defense),
      volatileState.defenseStage));
  if(abilityIs(pokemon,"HUGE POWER")||abilityIs(pokemon,"PURE POWER"))attack*=2U;
  if(abilityIs(pokemon,"HUSTLE"))attack=attack*3U/2U;
  if(abilityIs(pokemon,"GUTS")&&pokemon.status!=StatusCondition::None)attack=attack*3U/2U;
  else if(pokemon.status==StatusCondition::Burn)attack=std::max<uint32_t>(1U,attack/2U);
  if(heldIs(pokemon,volatileState,HeldItem::ChoiceBand))attack=attack*3U/2U;
  if((pokemon.speciesId==104U||pokemon.speciesId==105U)&&
     heldIs(pokemon,volatileState,HeldItem::ThickClub))attack*=2U;
  if(pokemon.speciesId==132U&&heldIs(pokemon,volatileState,HeldItem::MetalPowder))
    defense=defense*3U/2U;
  if(abilityIs(pokemon,"MARVEL SCALE")&&pokemon.status!=StatusCondition::None)
    defense=defense*3U/2U;
  uint32_t damage=(((2U*pokemon.level/5U+2U)*40U*attack/defense)/50U)+2U;
  damage=damage*(85U+random(battle)%16U)/100U;
  return static_cast<uint16_t>(std::max<uint32_t>(1U,damage));
}
void BattleEngine::clear(BattleState& battle) {
  std::memset(&battle, 0, sizeof(battle));
  battle.rngState = 0xA341316CU; battle.trainerProfileId = 0xFF; battle.gymId = 0xFF;
}

uint8_t BattleEngine::highestPartyLevel(const PokemonCollection& collection) {
  uint8_t highest = 1;
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) {
    const OwnedPokemon* pokemon = CollectionLogic::find(collection, collection.party[slot]);
    if (pokemon) highest = std::max(highest, pokemon->level);
  }
  return highest;
}

OwnedPokemon* BattleEngine::currentOpponent(BattleState& battle) {
  return battle.opponentIndex < battle.opponentCount ? &battle.opponents[battle.opponentIndex] : nullptr;
}

const OwnedPokemon* BattleEngine::currentOpponent(const BattleState& battle) {
  return battle.opponentIndex < battle.opponentCount ? &battle.opponents[battle.opponentIndex] : nullptr;
}

void BattleEngine::orderOpponentTeamWeakestFirst(BattleState& battle) {
  const uint8_t count = std::min<uint8_t>(battle.opponentCount, kOpponentTeamCapacity);
  // Adjacent swaps make this a stable ordering. Equal-level teammates retain
  // their authored sequence, which is important when the signature Pokemon
  // and another team member deliberately share the ace level.
  for (uint8_t pass = 1; pass < count; ++pass) {
    for (uint8_t index = 0; index < count - pass; ++index) {
      if (battle.opponents[index].level <= battle.opponents[index + 1U].level) continue;
      std::swap(battle.opponents[index], battle.opponents[index + 1U]);
    }
  }
  battle.opponentIndex = 0;
}

bool BattleEngine::ensureOpponentUids(BattleState& battle) {
  bool changed = false;
  // Owned Pokemon IDs are local to the player's collection.  Opponent IDs
  // live in the separate BattleState namespace, so reserve the high nibble
  // and retain four low bits for the slot.  This remains deterministic after
  // a save/reboot and, unlike the former zero IDs, is unique inside the team.
  const uint32_t battlePart = (battle.rngState ^ 0x5A17C9E3UL) & 0x0FFFFFF0UL;
  for (uint8_t index = 0; index < battle.opponentCount; ++index) {
    bool duplicate = battle.opponents[index].uid == 0;
    for (uint8_t previous = 0; previous < index && !duplicate; ++previous)
      duplicate = battle.opponents[index].uid == battle.opponents[previous].uid;
    if (!duplicate) continue;
    battle.opponents[index].uid = 0xE0000000UL | battlePart |
                                  static_cast<uint32_t>(index + 1U);
    changed = true;
  }
  return changed;
}

bool BattleEngine::moveIsImprisoned(const BattleState& battle,
                                    const PokemonCollection& collection,
                                    MoveId move,BattleSide userSide){
  if(move==MoveId::None)return false;
  const OwnedPokemon* player=CollectionLogic::find(collection,battle.playerUid);
  const OwnedPokemon* opponent=currentOpponent(battle);
  if(!player||!opponent)return false;
  if(userSide==BattleSide::Player)
    return battle.opponentMoveEffects[battle.opponentIndex].imprisoned&&
           pokemonKnowsMove(*opponent,move);
  return battle.playerMoveEffects.imprisoned&&pokemonKnowsMove(*player,move);
}

bool BattleEngine::moveIsSelectable(const BattleState& battle,
                                    const PokemonCollection& collection,
                                    MoveId move,BattleSide userSide){
  if(move==MoveId::None||moveIsImprisoned(battle,collection,move,userSide))return false;
  const FullMoveData* data=findFullMove(move);
  const OwnedPokemon* player=CollectionLogic::find(collection,battle.playerUid);
  const OwnedPokemon* opponent=currentOpponent(battle);
  if(!data||!player||!opponent)return false;
  const DedicatedMoveEffectState& effects=userSide==BattleSide::Player?
      battle.playerMoveEffects:battle.opponentMoveEffects[battle.opponentIndex];
  const CombatVolatile& volatileState=userSide==BattleSide::Player?
      battle.playerVolatile:battle.opponentVolatiles[battle.opponentIndex];
  // Disable is a command-selection limitation in FireRed. The later
  // AtkCanceller check remains necessary for a move that was already locked
  // in (link battle, Encore or a multi-turn move), but a normal local choice
  // must be rejected without spending PP or advancing the turn.
  if(volatileState.disabledMove==move)return false;
  if(effects.tauntTurns&&data->power==0)return false;
  if(effects.tormented&&volatileState.lastMoveUsed==move)return false;
  return true;
}

bool BattleEngine::startWild(BattleState& battle, PokemonCollection& collection,
                             uint32_t playerUid, uint32_t seed, uint8_t unlockedGeneration,
                             bool beginnerProtection, uint8_t worldTimePeriod) {
  if (battle.active || !CollectionLogic::validate(collection) || !selectablePartyMember(collection, playerUid)) return false;
  clear(battle);
  battle.active = true; battle.kind = BattleKind::Wild; battle.outcome = BattleOutcome::Ongoing;
  battle.unlockedGeneration = unlockedGeneration;
  battle.playerUid = playerUid; battle.rngState = seed ? seed : 0xB5297A4DU;
  const uint8_t teamLevel = highestPartyLevel(collection);
  const OwnedPokemon* fieldLead=CollectionLogic::active(collection,0);
  battle.rewardMoney = 0;  // Wild encounters grant XP/capture only.
  // Wild Pokémon intentionally remain varied at every stage of Pokégochi.
  // Lv.5 remains in the pool once the team reaches Lv.6. This rule is limited
  // to Wild battles; trainer and Gym scaling retains its tighter curve.
  // Through Lv.20 the ceiling stays one level below the strongest selected
  // partner. From Lv.21 onward that partner's own level returns to the pool.
  const bool protectedBeginning = beginnerProtection && teamLevel < 10U;
  const uint8_t progressionCeiling = teamLevel <= 20U
      ? std::max<uint8_t>(2U, teamLevel > 1U ? static_cast<uint8_t>(teamLevel - 1U) : 1U)
      : std::max<uint8_t>(5U, teamLevel);
  const uint8_t maximumWildLevel = protectedBeginning
      ? std::min<uint8_t>(4U, progressionCeiling) : progressionCeiling;
  const uint8_t minimumWildLevel = maximumWildLevel < 5U ? 2U : 5U;
  // Roll level first, then select only a species legal at that exact level.
  // This preserves Lv.5 encounters for a high-level party without producing
  // impossible under-levelled evolutions such as Lv.7 Gyarados. A completely
  // uniform Lv.5..Lv.40 roll made useful late-game EXP unnecessarily rare, so
  // the curve gradually favours the upper end after Lv.20: one third of the
  // Lv.21-Lv.39 encounters and two thirds from Lv.40 onward use the higher of
  // two independent rolls. The remaining rolls still span the complete range,
  // therefore genuinely low-level Pokemon never disappear.
  const uint32_t levelRange =
      static_cast<uint32_t>(maximumWildLevel - minimumWildLevel + 1U);
  uint32_t levelOffset = random(battle) % levelRange;
  const uint8_t highBiasChanceThirds = maximumWildLevel >= 40U ? 2U
      : maximumWildLevel > 20U ? 1U : 0U;
  if (highBiasChanceThirds && random(battle) % 3U < highBiasChanceThirds)
    levelOffset = std::max<uint32_t>(levelOffset, random(battle) % levelRange);
  uint8_t level = static_cast<uint8_t>(minimumWildLevel + levelOffset);
  // Emerald's field effect: Hustle, Vital Spirit and Pressure give the lead
  // a 50% chance to force the encounter to the highest legal level.  This is
  // meaningful even though Pokegochi starts encounters from an icon.
  if(fieldLead&&(abilityIs(*fieldLead,"HUSTLE")||
      abilityIs(*fieldLead,"VITAL SPIRIT")||abilityIs(*fieldLead,"PRESSURE"))&&
      (random(battle)&1U)==0U)level=maximumWildLevel;
  // The production table begins at Lv.5. During the opening Lv.2-Lv.4 range,
  // use that first unevolved species pool while instantiating the lower level.
  const uint8_t speciesSelectionLevel = std::max<uint8_t>(5U, level);
  uint16_t speciesId = chooseEncounterSpecies(speciesSelectionLevel,
                                               unlockedGeneration,random(battle));
  // Emerald gives STATIC/MAGNET PULL a 50% chance to select an Electric/Steel
  // species from the current table.  Rejection sampling preserves the table's
  // configured relative weights and all level/generation legality rules.
  PokemonType preferredType=PokemonType::Normal;
  bool prefersType=false;
  if(fieldLead&&abilityIs(*fieldLead,"STATIC")){
    preferredType=PokemonType::Electric;prefersType=true;
  }else if(fieldLead&&abilityIs(*fieldLead,"MAGNET PULL")){
    preferredType=PokemonType::Steel;prefersType=true;
  }
  if(prefersType&&(random(battle)&1U)==0U){
    for(uint8_t attempt=0;attempt<96U;++attempt){
      const uint16_t candidate=chooseEncounterSpecies(
          speciesSelectionLevel,unlockedGeneration,random(battle));
      const SpeciesData* data=findSpecies(candidate);
      if(data&&(data->type1==preferredType||data->type2==preferredType)){
        speciesId=candidate;break;
      }
    }
  }
  // Time changes encounter flavour, never evolution.  Rejection-sampling the
  // existing legal table retains its level/generation rules while making the
  // morning, evening and nocturnal groups noticeably different.
  if(worldTimePeriod<=static_cast<uint8_t>(WorldTimePeriod::Night)&&
     random(battle)%4U!=0U){
    const WorldTimePeriod period=static_cast<WorldTimePeriod>(worldTimePeriod);
    for(uint8_t attempt=0;attempt<64U;++attempt){
      const uint16_t candidate=chooseEncounterSpecies(
          speciesSelectionLevel,unlockedGeneration,random(battle));
      if(WorldClock::encounterPreferred(candidate,period)){
        speciesId=candidate;break;
      }
    }
  }
  battle.opponentCount = 1;
  const uint32_t shinyRoll = random(battle);
  const bool shiny = kWildShinyQaMode ? (shinyRoll & 1U) == 0U
                                      : CollectionLogic::isShinyRoll(shinyRoll);
  battle.opponents[0] = CollectionLogic::createPokemon(0, speciesId, level,
      shiny, random(battle));
  // Cute Charm biases a gendered encounter toward the opposite sex two
  // thirds of the time.  Synchronize independently copies the lead's Nature
  // half the time, matching Emerald's field mechanics.
  if(fieldLead&&abilityIs(*fieldLead,"CUTE CHARM")&&
     pokemonGender(*fieldLead)>=0&&random(battle)%3U!=0U){
    const int8_t wanted=static_cast<int8_t>(1-pokemonGender(*fieldLead));
    for(uint8_t attempt=0;attempt<64U&&pokemonGender(battle.opponents[0])!=wanted;
        ++attempt)
      battle.opponents[0]=CollectionLogic::createPokemon(
          0,speciesId,level,shiny,random(battle));
  }
  if(fieldLead&&abilityIs(*fieldLead,"SYNCHRONIZE")&&
     (random(battle)&1U)==0U)
    battle.opponents[0].nature=fieldLead->nature;
  ensureOpponentUids(battle);
  applyEntryAbilities(battle,collection);
  return battle.opponents[0].speciesId != 0;
}

bool BattleEngine::startTrainer(BattleState& battle, EncounterCharges& charges,
                                PokemonCollection& collection, uint32_t playerUid,
                                uint32_t seed, uint8_t unlockedGeneration) {
  if (battle.active || !CollectionLogic::validate(collection) || !selectablePartyMember(collection, playerUid) ||
      charges.available == 0) return false;
  clear(battle);
  battle.active = true; battle.kind = BattleKind::Trainer; battle.outcome = BattleOutcome::Ongoing;
  battle.unlockedGeneration = unlockedGeneration;
  battle.playerUid = playerUid; battle.rngState = seed ? seed : 0x68E31DA4U;
  const TrainerProfile* profile = trainerProfileForGeneration(unlockedGeneration, random(battle));
  if (!profile) return false;
  battle.trainerProfileId = profile->id;
  const uint8_t teamLevel = highestPartyLevel(collection);
  battle.opponentItemUses = 1;
  battle.rewardMoney = static_cast<uint32_t>(teamLevel) * 40U;
  battle.opponentCount = static_cast<uint8_t>(1U + random(battle) % std::min<uint8_t>(3, 1U + teamLevel / 15U));
  for (uint8_t index = 0; index < battle.opponentCount; ++index) {
    const uint16_t speciesId = profile->species[random(battle) % profile->speciesCount];
    const int16_t offset = static_cast<int16_t>(random(battle) % 3U) - 1;
    battle.opponents[index] = CollectionLogic::createPokemon(
        0, speciesId, boundedLevel(teamLevel + offset), false, random(battle));
    if (!battle.opponents[index].speciesId) return false;
  }
  orderOpponentTeamWeakestFirst(battle);
  ensureOpponentUids(battle);
  applyEntryAbilities(battle,collection);
  return EncounterLogic::consumeManualCharge(charges);
}

void BattleEngine::applyEntryAbilities(BattleState& battle,PokemonCollection& collection,
                                       BattleEntryScope scope,
                                       BattleActionResult* result){
  OwnedPokemon* player=CollectionLogic::find(collection,battle.playerUid);
  OwnedPokemon* opponent=currentOpponent(battle);
  if(!player||!opponent)return;
  if(battle.turn==0){
    battle.terrain=initialBattleTerrain(battle.kind,*opponent);
    battle.terrainType=terrainPokemonType(battle.terrain);
  }
  const bool playerEntered=scope==BattleEntryScope::Both||scope==BattleEntryScope::Player;
  const bool opponentEntered=scope==BattleEntryScope::Both||scope==BattleEntryScope::Opponent;
  // FireRed records ON_SWITCHIN effects first, then drains the queued
  // Intimidate effects, and only then resolves Trace. Snapshot the two entry
  // Abilities so a freshly copied weather/Intimidate Ability cannot fire as
  // though it had belonged to the entrant before Trace.
  const uint8_t playerEntryAbility=player->abilityId;
  const uint8_t opponentEntryAbility=opponent->abilityId;
  const auto abilityIdIs=[](uint8_t id,const char* name){
    const AbilityData* ability=findAbility(id);
    return ability&&std::strcmp(ability->name,name)==0;
  };

  auto setWeather=[&](const OwnedPokemon& pokemon,uint8_t entryAbility,
                      BattleSide side){
    const BattleWeather before=battle.weather;
    const bool strongAlready=before==BattleWeather::HeavyRain||
        before==BattleWeather::HarshSun||before==BattleWeather::StrongWinds;
    if(abilityIdIs(entryAbility,"PRIMORDIAL SEA")){
      battle.weather=BattleWeather::HeavyRain;battle.weatherTurns=0xFF;
    }else if(abilityIdIs(entryAbility,"DESOLATE LAND")){
      battle.weather=BattleWeather::HarshSun;battle.weatherTurns=0xFF;
    }else if(abilityIdIs(entryAbility,"DELTA STREAM")){
      battle.weather=BattleWeather::StrongWinds;battle.weatherTurns=0xFF;
    }else if(!strongAlready&&abilityIdIs(entryAbility,"DRIZZLE")){
      battle.weather=BattleWeather::Rain;battle.weatherTurns=0xFF;
    }else if(!strongAlready&&(abilityIdIs(entryAbility,"DROUGHT")||
                              abilityIdIs(entryAbility,"MEGA SOL"))){
      battle.weather=BattleWeather::Sun;battle.weatherTurns=0xFF;
    }else if(!strongAlready&&abilityIdIs(entryAbility,"SAND STREAM")){
      battle.weather=BattleWeather::Sandstorm;battle.weatherTurns=0xFF;
    }
    if(result&&battle.weather!=before)appendAbilityActivation(*result,side,pokemon);
  };
  // Primal weather and Delta Stream end as soon as their last source leaves.
  // Ordinary Gen-III Drizzle/Drought/Sand Stream weather remains permanent.
  const BattleWeather strongWeatherBefore=battle.weather;
  if(battle.weatherTurns==0xFF&&battle.weather==BattleWeather::HeavyRain&&
     !abilityIs(*player,"PRIMORDIAL SEA")&&
     !abilityIs(*opponent,"PRIMORDIAL SEA")){
    battle.weather=BattleWeather::Clear;battle.weatherTurns=0;
  }else if(battle.weatherTurns==0xFF&&battle.weather==BattleWeather::HarshSun&&
           !abilityIs(*player,"DESOLATE LAND")&&
           !abilityIs(*opponent,"DESOLATE LAND")){
    battle.weather=BattleWeather::Clear;battle.weatherTurns=0;
  }else if(battle.weatherTurns==0xFF&&battle.weather==BattleWeather::StrongWinds&&
           !abilityIs(*player,"DELTA STREAM")&&
           !abilityIs(*opponent,"DELTA STREAM")){
    battle.weather=BattleWeather::Clear;battle.weatherTurns=0;
  }
  if(result&&strongWeatherBefore!=BattleWeather::Clear&&
     battle.weather==BattleWeather::Clear)
    appendWeatherEnded(*result,battle.playerUid,strongWeatherBefore);
  if(playerEntered&&opponentEntered){
    // At battle start FireRed executes switch-in Abilities from the fastest
    // battler to the slowest; therefore the slower conflicting weather is the
    // one left active. A deterministic player-first tie avoids perturbing the
    // battle RNG while retaining the same valid tie outcome.
    const uint16_t playerSpeed=battleCalculatedStat(battle,*player,PokemonStat::Speed);
    const uint16_t opponentSpeed=battleCalculatedStat(battle,*opponent,PokemonStat::Speed);
    if(playerSpeed>=opponentSpeed){
      setWeather(*player,playerEntryAbility,BattleSide::Player);
      setWeather(*opponent,opponentEntryAbility,BattleSide::Opponent);
    }else{
      setWeather(*opponent,opponentEntryAbility,BattleSide::Opponent);
      setWeather(*player,playerEntryAbility,BattleSide::Player);
    }
  }else{
    if(playerEntered)setWeather(*player,playerEntryAbility,BattleSide::Player);
    if(opponentEntered)setWeather(*opponent,opponentEntryAbility,BattleSide::Opponent);
  }

  auto blocksDrop=[](const OwnedPokemon& p){return abilityIs(p,"CLEAR BODY")||
      abilityIs(p,"WHITE SMOKE")||abilityIs(p,"HYPER CUTTER");};
  if(playerEntered&&abilityIdIs(playerEntryAbility,"INTIMIDATE")){
    if(blocksDrop(*opponent)){
      if(result)appendAbilityActivation(*result,BattleSide::Opponent,*opponent);
    }else if(!battle.opponentVolatiles[battle.opponentIndex].substituteHp&&
             !battle.opponentMistTurns){
      if(result)appendAbilityActivation(*result,BattleSide::Player,*player);
      const StageSnapshot before=snapshotStages(battle.opponentVolatiles[battle.opponentIndex]);
      changeStage(battle.opponentVolatiles[battle.opponentIndex].attackStage,-1);
      if(result)appendStageChanges(*result,BattleSide::Opponent,opponent->uid,before,
                                   battle.opponentVolatiles[battle.opponentIndex]);
    }
  }
  if(opponentEntered&&abilityIdIs(opponentEntryAbility,"INTIMIDATE")){
    if(blocksDrop(*player)){
      if(result)appendAbilityActivation(*result,BattleSide::Player,*player);
    }else if(!battle.playerVolatile.substituteHp&&!battle.playerMistTurns){
      if(result)appendAbilityActivation(*result,BattleSide::Opponent,*opponent);
      const StageSnapshot before=snapshotStages(battle.playerVolatile);
      changeStage(battle.playerVolatile.attackStage,-1);
      if(result)appendStageChanges(*result,BattleSide::Player,player->uid,before,
                                   battle.playerVolatile);
    }
  }
  if(playerEntered&&abilityIdIs(playerEntryAbility,"TRACE")&&!battle.playerAbilityTraced&&
     opponent->currentHp&&opponent->abilityId){
    battle.playerAbilityTraced=true;
    battle.playerPreTraceAbilityId=player->abilityId;
    player->abilityId=opponent->abilityId;
    if(result){
      BattleEvent& copied=appendEvent(*result,BattleEventType::MoveEffect,
                                      BattleSide::Player,player->uid);
      copied.value=static_cast<uint16_t>(BattleMoveEffect::AbilityCopied);
      copied.after=player->abilityId;
    }
  }
  if(opponentEntered&&abilityIdIs(opponentEntryAbility,"TRACE")&&player->currentHp&&
     player->abilityId){
    opponent->abilityId=player->abilityId;
    if(result){
      BattleEvent& copied=appendEvent(*result,BattleEventType::MoveEffect,
                                      BattleSide::Opponent,opponent->uid);
      copied.value=static_cast<uint16_t>(BattleMoveEffect::AbilityCopied);
      copied.after=opponent->abilityId;
    }
  }
  if(playerEntered)cureConditionForbiddenByAbility(*player,battle.playerVolatile,
      battle.playerMoveEffects,BattleSide::Player,result);
  if(opponentEntered)cureConditionForbiddenByAbility(*opponent,
      battle.opponentVolatiles[battle.opponentIndex],
      battle.opponentMoveEffects[battle.opponentIndex],BattleSide::Opponent,result);
  // Forecast runs after every switch-in Ability/weather pass. This also
  // handles Cloud Nine/Air Lock entering or leaving the field.
  refreshForecastPair(battle,*player,battle.playerMoveEffects,*opponent,
                      battle.opponentMoveEffects[battle.opponentIndex],result);
  if(playerEntered){
    if(result)triggerHeldItemWithResult(*result,*player,battle.playerVolatile,
        BattleSide::Player,&battle.playerMoveEffects);
    else triggerHeldItem(*player,battle.playerVolatile,&battle.playerMoveEffects);
  }
  if(opponentEntered){
    if(result)triggerHeldItemWithResult(*result,*opponent,
        battle.opponentVolatiles[battle.opponentIndex],BattleSide::Opponent,
        &battle.opponentMoveEffects[battle.opponentIndex]);
    else triggerHeldItem(*opponent,battle.opponentVolatiles[battle.opponentIndex],
                         &battle.opponentMoveEffects[battle.opponentIndex]);
  }
}

void BattleEngine::synchronizeWeatherForms(BattleState& battle,
                                           PokemonCollection& collection){
  refreshForecastForms(battle,collection,nullptr);
}

uint16_t BattleEngine::calculateDamage(BattleState& battle, const OwnedPokemon& attacker,
                                       const OwnedPokemon& defender, MoveId moveId,
                                       const CombatVolatile& attackerVolatile,
                                       const CombatVolatile& defenderVolatile,
                                       uint16_t* effectivenessOut,bool* criticalOut,
                                       bool allowCritical,uint16_t powerOverride) {
  if(effectivenessOut)*effectivenessOut=100;if(criticalOut)*criticalOut=false;
  gLastCalculatedMovePower=0;
  const FullMoveData* move = findFullMove(moveId);
  const SpeciesData* attackerSpecies = findSpecies(attacker.speciesId);
  const SpeciesData* defenderSpecies = findSpecies(defender.speciesId);
  if (!move || !attackerSpecies || !defenderSpecies) return 0;
  const bool typelessStruggle=move->id==static_cast<uint16_t>(MoveId::Struggle);
  const bool fixedDamage=std::strstr(move->effect,"OHKO")||
      std::strstr(move->effect,"DRAGON_RAGE")||std::strstr(move->effect,"SONICBOOM")||
      std::strstr(move->effect,"LEVEL_DAMAGE")||std::strstr(move->effect,"PSYWAVE")||
      std::strstr(move->effect,"SUPER_FANG")||effectIs(move,"ENDEAVOR");
  if(!move->power&&!fixedDamage)return 0;
  if(std::strstr(move->effect,"EXPLOSION")&&
     (abilityIs(attacker,"DAMP")||defenderAbilityApplies(attacker,defender,"DAMP")))return 0;
  const PokemonType attackType=effectiveMoveType(battle,*move,attacker,&defender);
  const BattleSide attackerSide=sideOf(battle,attacker);
  const BattleSide defenderSide=sideOf(battle,defender);
  DedicatedMoveEffectState& attackerEffects=moveEffectsFor(battle,attackerSide);
  const DedicatedMoveEffectState& defenderEffects=moveEffectsFor(battle,defenderSide);
  auto resolvedEffectiveness=[&](){
    if(typelessStruggle)return static_cast<uint16_t>(100U);
    uint16_t effectiveness=identifiedTypeMultiplier100(attackType,defender,defenderEffects);
    const bool ignoreAbility=abilityIs(attacker,"MOLD BREAKER");
    if(!ignoreAbility&&((attackType==PokemonType::Ground&&abilityIs(defender,"LEVITATE"))||
       (attackType==PokemonType::Fire&&abilityIs(defender,"FLASH FIRE")&&
        defender.status!=StatusCondition::Frozen)||
       (move->power&&attackType==PokemonType::Water&&abilityIs(defender,"WATER ABSORB"))||
       (move->power&&attackType==PokemonType::Electric&&abilityIs(defender,"VOLT ABSORB"))))
      effectiveness=0;
    if(!ignoreAbility&&move->power&&abilityIs(defender,"WONDER GUARD")&&effectiveness<=100U)
      effectiveness=0;
    return effectiveness;
  };
  // The caller performs the shared accuracy roll before dispatching either
  // damage or a power-0 effect. Keeping it outside this damage-only function
  // prevents status moves from being treated as automatic misses.
  if(fixedDamage){
    const uint16_t effectiveness=resolvedEffectiveness();
    if(!effectiveness||(std::strstr(move->effect,"OHKO")&&
       defenderAbilityApplies(attacker,defender,"STURDY"))){
      if(effectivenessOut)*effectivenessOut=0;return 0;
    }
    // Fixed-damage scripts clear super-effective/not-very-effective flags.
    // OHKO retains ordinary typecalc feedback.
    if(effectivenessOut)*effectivenessOut=std::strstr(move->effect,"OHKO")?
        effectiveness:100U;
    if(std::strstr(move->effect,"OHKO"))return defender.currentHp;
    if(std::strstr(move->effect,"DRAGON_RAGE"))return 40;
    if(std::strstr(move->effect,"SONICBOOM"))return 20;
    if(std::strstr(move->effect,"LEVEL_DAMAGE"))return attacker.level;
    if(std::strstr(move->effect,"PSYWAVE")){
      // psywavedamageeffect rejects the 11..15 results of Random() % 16,
      // rather than taking Random() % 11.  The values are distributed the
      // same, but reproducing rejection also preserves the exact RNG stream.
      uint16_t roll=0;
      do roll=static_cast<uint16_t>(random(battle)%16U);while(roll>10U);
      return std::max<uint16_t>(1,static_cast<uint16_t>(
          attacker.level*(roll*10U+50U)/100U));
    }
    if(std::strstr(move->effect,"SUPER_FANG"))return std::max<uint16_t>(1,defender.currentHp/2U);
    // Endeavor is still ordinary target damage after this calculation, so a
    // Substitute absorbs it and Focus Band/Endure can modify the result.
    return defender.currentHp>attacker.currentHp?
        static_cast<uint16_t>(defender.currentHp-attacker.currentHp):0U;
  }
  const bool physical=isPhysicalType(attackType);
  // Gen III combines every critical modifier into one stage. Focus Energy
  // contributes two stages, ordinary high-critical moves and Scope Lens one,
  // and the species-specific Stick/Lucky Punch bonuses two. Blaze Kick has a
  // dedicated burn effect but still contributes the high-critical stage.
  static constexpr uint8_t kCriticalDivisors[]{16U,8U,4U,3U,2U};
  uint8_t criticalStage=attackerVolatile.criticalStage;
  if(std::strstr(move->effect,"HIGH_CRITICAL")||effectIs(move,"POISON_TAIL")||
     effectIs(move,"SKY_ATTACK")||
     effectIs(move,"BLAZE_KICK"))++criticalStage;
  if(heldIs(attacker,attackerVolatile,HeldItem::ScopeLens))++criticalStage;
  if((attacker.speciesId==83U&&heldIs(attacker,attackerVolatile,HeldItem::Stick))||
     (attacker.speciesId==113U&&heldIs(attacker,attackerVolatile,HeldItem::LuckyPunch)))
    criticalStage=static_cast<uint8_t>(criticalStage+2U);
  criticalStage=std::min<uint8_t>(criticalStage,4U);
  const uint8_t criticalChance=kCriticalDivisors[criticalStage];
  // Spit Up's Gen-III script deliberately skips critcalc. Its stored-energy
  // multiplier is applied to the complete base-damage result below instead.
  const bool critical = allowCritical&&!effectIs(move,"SPIT_UP")&&
      !defenderAbilityApplies(attacker,defender,"BATTLE ARMOR") &&
      !defenderAbilityApplies(attacker,defender,"SHELL ARMOR") &&
      random(battle) % criticalChance == 0U;
  if (criticalOut) *criticalOut = critical;
  const int8_t attackStage = physical ? attackerVolatile.attackStage : attackerVolatile.spAttackStage;
  const int8_t defenseStage = physical ? defenderVolatile.defenseStage : defenderVolatile.spDefenseStage;
  // FireRed applies Ability stat modifiers to the raw battle stats before
  // applying +/- stages. Besides matching integer rounding, this matters on
  // critical hits because ignored stages must not discard the Ability boost.
  uint32_t rawAttack=battleCalculatedStat(battle,attacker,
      physical?PokemonStat::Attack:PokemonStat::SpAttack);
  uint32_t rawDefense=battleCalculatedStat(battle,defender,
      physical?PokemonStat::Defense:PokemonStat::SpDefense);
  if(physical&&(abilityIs(attacker,"HUGE POWER")||abilityIs(attacker,"PURE POWER")))
    rawAttack*=2U;
  if(physical&&abilityIs(attacker,"HUSTLE"))rawAttack=rawAttack*3U/2U;
  if(physical&&abilityIs(attacker,"GUTS")&&attacker.status!=StatusCondition::None)
    rawAttack=rawAttack*3U/2U;
  if(!physical&&defenderAbilityApplies(attacker,defender,"THICK FAT")&&
     (attackType==PokemonType::Fire||attackType==PokemonType::Ice))
    rawAttack=std::max<uint32_t>(1U,rawAttack/2U);
  if(physical&&defenderAbilityApplies(attacker,defender,"MARVEL SCALE")&&
     defender.status!=StatusCondition::None)rawDefense=rawDefense*3U/2U;
  uint32_t attack=staged(rawAttack,critical&&attackStage<0?0:attackStage);
  uint32_t defense=std::max<uint32_t>(1U,
      staged(rawDefense,critical&&defenseStage>0?0:defenseStage));
  if (physical && heldIs(attacker,attackerVolatile,HeldItem::ChoiceBand)) attack = attack * 3U / 2U;
  if(physical&&(attacker.speciesId==104U||attacker.speciesId==105U)&&
     heldIs(attacker,attackerVolatile,HeldItem::ThickClub))attack*=2U;
  if(!physical&&attacker.speciesId==25U&&heldIs(attacker,attackerVolatile,HeldItem::LightBall))attack*=2U;
  if(!physical&&attacker.speciesId==366U&&heldIs(attacker,attackerVolatile,HeldItem::DeepSeaTooth))attack*=2U;
  if(!physical&&(attacker.speciesId==380U||attacker.speciesId==381U)&&
     heldIs(attacker,attackerVolatile,HeldItem::SoulDew))attack=attack*3U/2U;
  const bool suppressWeather=abilityIs(attacker,"CLOUD NINE")||abilityIs(attacker,"AIR LOCK")||
      abilityIs(defender,"CLOUD NINE")||abilityIs(defender,"AIR LOCK");
  if(!physical&&!suppressWeather&&abilityIs(attacker,"SOLAR POWER")&&
     (battle.weather==BattleWeather::Sun||battle.weather==BattleWeather::HarshSun))attack=attack*3U/2U;
  // In Gen III Metal Powder strengthens both defensive stats while Ditto is
  // still Ditto. `defense` already refers to the physical or special stat
  // selected for this move, so the same multiplier covers both branches.
  if(defender.speciesId==132U&&heldIs(defender,defenderVolatile,HeldItem::MetalPowder))
    defense=defense*3U/2U;
  if(!physical&&defender.speciesId==366U&&heldIs(defender,defenderVolatile,HeldItem::DeepSeaScale))defense*=2U;
  if(!physical&&(defender.speciesId==380U||defender.speciesId==381U)&&
     heldIs(defender,defenderVolatile,HeldItem::SoulDew))defense=defense*3U/2U;
  if(physical&&effectIs(move,"EXPLOSION"))
    defense=std::max<uint32_t>(1U,defense/2U);
  uint32_t power = powerOverride ? powerOverride : move->power;
  if(effectIs(move,"COUNTER")){
    const uint16_t effectiveness=resolvedEffectiveness();
    if(effectivenessOut)*effectivenessOut=effectiveness;
    // `lastDamageWasPhysical` used to survive the turn reset.  Checking it
    // without the matching non-zero damage made Counter depend on a stale
    // category marker, unlike Mirror Coat.  A retaliation is valid only when
    // this battler actually lost HP to a physical move in the current turn.
    return effectiveness&&attackerEffects.lastDamageReceived&&
        attackerEffects.lastDamageWasPhysical?
        static_cast<uint16_t>(std::min<uint32_t>(65535U,attackerEffects.lastDamageReceived*2U)):0U;
  }
  if(effectIs(move,"MIRROR_COAT")){
    const uint16_t effectiveness=resolvedEffectiveness();
    if(effectivenessOut)*effectivenessOut=effectiveness;
    return effectiveness&&attackerEffects.lastDamageReceived&&!attackerEffects.lastDamageWasPhysical?
        static_cast<uint16_t>(std::min<uint32_t>(65535U,attackerEffects.lastDamageReceived*2U)):0U;
  }
  if(effectIs(move,"PRESENT")){
    // FireRed consumes the low byte directly: 102/256 at 40 power, 76/256
    // at 80, 26/256 at 120 and 52/256 healing the target by one quarter.
    const uint8_t roll=static_cast<uint8_t>(random(battle)&0xFFU);
    if(roll>=204U){gPresentHealRoll=true;return 0;}
    power=roll<102U?40U:roll<178U?80U:120U;
  }
  if(effectIs(move,"HIDDEN_POWER"))power=hiddenPowerPower(battle,attacker);
  else if(effectIs(move,"WEATHER_BALL")&&attackType!=move->type)power*=2U;
  else if(effectIs(move,"REVENGE")&&attackerEffects.lastDamageReceived)power*=2U;
  if(std::strstr(move->effect,"MAGNITUDE")){
    // FireRed's Magnitude table is weighted 5/10/20/30/20/10/5 percent for
    // magnitudes 4..10, rather than choosing the seven powers uniformly.
    static constexpr uint8_t powers[]{10,30,50,70,90,110,150};
    static constexpr uint8_t cumulative[]{5,15,35,65,85,95,100};
    const uint8_t roll=static_cast<uint8_t>(random(battle)%100U);
    uint8_t index=0;while(index<6U&&roll>=cumulative[index])++index;
    power=powers[index];
  }
  else if(std::strstr(move->effect,"LOW_KICK")){const PokedexEntryData* entry=pokedexEntry(defender.speciesId);const uint16_t kg10=entry?entry->weightHectograms:100;
    power=kg10<100?20:kg10<250?40:kg10<500?60:kg10<1000?80:kg10<2000?100:120;}
  else if(effectIs(move,"FURY_CUTTER"))
    power*=static_cast<uint32_t>(1U<<std::min<uint8_t>(4U,
        attackerEffects.furyCutterCount?attackerEffects.furyCutterCount-1U:0U));
  if(effectIs(move,"ROLLOUT"))
    power*=static_cast<uint32_t>(1U<<std::min<uint8_t>(4U,attackerEffects.rolloutCount));
  if(effectIs(move,"ROLLOUT")&&attackerEffects.defenseCurl)power*=2U;
  if (std::strstr(move->effect,"FLAIL")) {
    const uint32_t ratio = attacker.maximumHp ? attacker.currentHp * 48U / attacker.maximumHp : 48U;
    power = ratio <= 1 ? 200 : ratio <= 4 ? 150 : ratio <= 9 ? 100 : ratio <= 16 ? 80 : ratio <= 32 ? 40 : 20;
  } else if (std::strstr(move->effect,"ERUPTION")) power = std::max<uint32_t>(1, 150U * attacker.currentHp / attacker.maximumHp);
  else if (std::strstr(move->effect,"FACADE") && (attacker.status == StatusCondition::Poison ||
           attacker.status == StatusCondition::BadlyPoisoned || attacker.status == StatusCondition::Paralysis ||
           attacker.status == StatusCondition::Burn)) power *= 2U;
  // The retail command permits dynamic power zero at the two friendship
  // extremes. CalculateBaseDamage still contributes its final +2, so forcing
  // a minimum power of one subtly over-damages those exact cases.
  else if (std::strstr(move->effect,"RETURN")) power = attacker.friendship * 10U / 25U;
  else if (std::strstr(move->effect,"FRUSTRATION")) power = (255U - attacker.friendship) * 10U / 25U;
  else if (std::strstr(move->effect,"DREAM_EATER") && defender.status != StatusCondition::Sleep) return 0;
  else if (std::strstr(move->effect,"SMELLINGSALT") && defender.status == StatusCondition::Paralysis) power *= 2U;

  // Resolve variable base power before Ability/item multipliers. The former
  // order checked TECHNICIAN against the static table value and then let
  // Hidden Power, Low Kick, Magnitude, Flail, Return, etc. overwrite that
  // boost (and Tough Claws/type items along with it).
  if(!typelessStruggle&&attackType==PokemonType::Fire&&attackerEffects.flashFireBoost)
    power=power*3U/2U;
  if(abilityIs(attacker,"TOUGH CLAWS")&&move->makesContact)power=power*130U/100U;
  if(abilityIs(attacker,"TECHNICIAN")&&power<=60U)power=power*150U/100U;
  const auto named=[&](const char* value){return std::strcmp(move->name,value)==0;};
  if(abilityIs(attacker,"MEGA LAUNCHER")&&(named("AURA SPHERE")||named("DARK PULSE")||
     named("DRAGON PULSE")||named("WATER PULSE")))power=power*150U/100U;
  if(abilityIs(attacker,"STRONG JAW")&&(named("BITE")||named("CRUNCH")||named("HYPER FANG")||
     named("POISON FANG")||named("ICE FANG")||named("FIRE FANG")||named("THUNDER FANG")))power=power*150U/100U;
  if(abilityIs(attacker,"SHARPNESS")&&(named("CUT")||named("SLASH")||
     named("RAZOR WIND")||named("FURY CUTTER")||named("AIR CUTTER")||
     named("LEAF BLADE")))power=power*150U/100U;
  if(abilityIs(attacker,"SHEER FORCE")&&sheerForceApplies(move))power=power*130U/100U;
  // The 20% conversion boost belongs only to -ate Abilities. Weather Ball
  // and Hidden Power may also change type, but do not receive this bonus.
  if(!typelessStruggle&&move->type==PokemonType::Normal&&attackType!=PokemonType::Normal&&
     !effectIs(move,"HIDDEN_POWER")&&!effectIs(move,"WEATHER_BALL")&&
      (abilityIs(attacker,"AERILATE")||abilityIs(attacker,"PIXILATE")||
       abilityIs(attacker,"REFRIGERATE")||abilityIs(attacker,"DRAGONIZE")))
    power=power*120U/100U;
  // Pokegochi battles are one-on-one and do not carry a separate terrain
  // timer. Electric Surge's useful singles effect is adapted as a persistent
  // field charge for its holder while that Mega remains active.
  if(!typelessStruggle&&abilityIs(attacker,"ELECTRIC SURGE")&&
     attackType==PokemonType::Electric)power=power*130U/100U;
  if(!typelessStruggle&&!suppressWeather&&abilityIs(attacker,"SAND FORCE")&&battle.weather==BattleWeather::Sandstorm&&
     (attackType==PokemonType::Rock||attackType==PokemonType::Ground||attackType==PokemonType::Steel))power=power*130U/100U;
  const HeldItem attackItem=heldItemFor(attacker,attackerVolatile);
  if(!typelessStruggle&&attackItem==HeldItem::SeaIncense&&attackType==PokemonType::Water)
    power=power*105U/100U;
  else if(!typelessStruggle&&heldItemBoostsType(attackItem,attackType))power=power*110U/100U;

  // These are battlefield damage modifiers, not the move's intrinsic base
  // power, so they intentionally run after Technician's eligibility check.
  if(effectIs(move,"PURSUIT")&&gPursuitSwitchBoost)power*=2U;
  if(effectIs(move,"FLINCH_MINIMIZE_HIT")&&defenderEffects.minimized)power*=2U;
  const uint16_t hiddenMove=static_cast<uint16_t>(defenderVolatile.chargingMove);
  if(((hiddenMove==19U||hiddenMove==340U)&&(move->id==16U||move->id==239U))||
     (hiddenMove==91U&&(move->id==89U||move->id==222U))||
     (hiddenMove==291U&&(move->id==57U||move->id==250U)))power*=2U;
  if(!typelessStruggle&&attackType==PokemonType::Electric&&attackerEffects.chargeTurns)power*=2U;
  const bool mudSport=battle.playerMoveEffects.mudSport||
      battle.opponentMoveEffects[battle.opponentIndex].mudSport;
  const bool waterSport=battle.playerMoveEffects.waterSport||
      battle.opponentMoveEffects[battle.opponentIndex].waterSport;
  if(!typelessStruggle&&attackType==PokemonType::Electric&&mudSport)
    power=std::max<uint32_t>(1U,power/2U);
  if(!typelessStruggle&&attackType==PokemonType::Fire&&waterSport)
    power=std::max<uint32_t>(1U,power/2U);
  gLastCalculatedMovePower=static_cast<uint16_t>(std::min<uint32_t>(65535U,power));
  uint32_t damage = (((2U * attacker.level / 5U + 2U) * power * attack / defense) / 50U) + 2U;
  if (critical) damage *= 2U;
  if(physical&&attacker.status==StatusCondition::Burn&&!abilityIs(attacker,"GUTS"))
    damage=std::max<uint32_t>(1U,damage/2U);
  // Cmd_stockpiletobasedamage multiplies the complete CalculateBaseDamage
  // result (including its +2), not just the move's nominal power.
  if(effectIs(move,"SPIT_UP"))
    damage*=std::max<uint8_t>(1U,attackerVolatile.stockpileCount);
  if (!typelessStruggle&&hasBattleType(attacker,attackerEffects,attackType))
    damage = damage * (abilityIs(attacker,"ADAPTABILITY")?200U:150U) / 100U;
  uint16_t effectiveness = typelessStruggle?100U:
      identifiedTypeMultiplier100(attackType,defender,defenderEffects);
  // Delta Stream removes only the Flying component's Rock/Electric/Ice
  // weakness. Apply it to the type multiplier itself so Wonder Guard,
  // Filter, AI feedback and the displayed effectiveness all see the same
  // result (Steel/Flying vs Rock becomes 0.5x, not a hidden 0.5x damage with
  // a misleading neutral message).
  if(!typelessStruggle&&!suppressWeather&&battle.weather==BattleWeather::StrongWinds&&
     effectiveness>0U&&hasBattleType(defender,defenderEffects,PokemonType::Flying)&&
     (attackType==PokemonType::Rock||attackType==PokemonType::Electric||
      attackType==PokemonType::Ice))
    effectiveness=static_cast<uint16_t>(effectiveness/2U);
  if(effectivenessOut)*effectivenessOut=effectiveness;
  if (!effectiveness) return 0;
  const bool ignoreDefenderAbility=abilityIs(attacker,"MOLD BREAKER");
  if (!typelessStruggle&&!ignoreDefenderAbility&&((attackType == PokemonType::Ground && abilityIs(defender,"LEVITATE")) ||
      (attackType == PokemonType::Fire && abilityIs(defender,"FLASH FIRE")&&
       defender.status!=StatusCondition::Frozen) ||
      (move->power&&attackType == PokemonType::Water && abilityIs(defender,"WATER ABSORB")) ||
      (move->power&&attackType == PokemonType::Electric && abilityIs(defender,"VOLT ABSORB")))) {
    if(effectivenessOut)*effectivenessOut=0;return 0;
  }
  if (!typelessStruggle&&!ignoreDefenderAbility&&abilityIs(defender,"WONDER GUARD") && effectiveness <= 100U) {if(effectivenessOut)*effectivenessOut=0;return 0;}
  damage = damage * effectiveness / 100U;
  if(!typelessStruggle&&!ignoreDefenderAbility&&abilityIs(defender,"FILTER")&&effectiveness>100U)
    damage=damage*3U/4U;
  if(!ignoreDefenderAbility&&abilityIs(defender,"MULTISCALE")&&
     defender.currentHp==defender.maximumHp)
    damage=std::max<uint32_t>(1U,damage/2U);
  if(!typelessStruggle&&!suppressWeather){
    // FireRed/Emerald halve Solar Beam in rain, sandstorm and hail. Sunny
    // weather instead removes its charge turn in isChargingMove().
    if(effectIs(move,"SOLAR_BEAM")&&
       (battle.weather==BattleWeather::Rain||battle.weather==BattleWeather::HeavyRain||
        battle.weather==BattleWeather::Sandstorm||battle.weather==BattleWeather::Hail))
      damage=std::max<uint32_t>(1U,damage/2U);
    if(battle.weather==BattleWeather::HeavyRain){if(attackType==PokemonType::Fire)return 0;if(attackType==PokemonType::Water)damage=damage*3U/2U;}
    else if(battle.weather==BattleWeather::HarshSun){if(attackType==PokemonType::Water)return 0;if(attackType==PokemonType::Fire)damage=damage*3U/2U;}
    if(battle.weather==BattleWeather::Rain){if(attackType==PokemonType::Water)damage=damage*3U/2U;else if(attackType==PokemonType::Fire)damage/=2U;}
    else if(battle.weather==BattleWeather::Sun){if(attackType==PokemonType::Fire)damage=damage*3U/2U;else if(attackType==PokemonType::Water)damage/=2U;}
  }
  if (!typelessStruggle&&attacker.currentHp * 3U <= attacker.maximumHp &&
      ((abilityIs(attacker,"OVERGROW")&&attackType==PokemonType::Grass)||(abilityIs(attacker,"BLAZE")&&attackType==PokemonType::Fire)||(abilityIs(attacker,"TORRENT")&&attackType==PokemonType::Water)||(abilityIs(attacker,"SWARM")&&attackType==PokemonType::Bug))) damage=damage*3U/2U;
  // Brick Break removes Reflect/Light Screen before its own damage is
  // calculated in FireRed, so that hit cannot be reduced by the screen it
  // shatters.
  if (!critical && std::strcmp(move->effect,"BRICK_BREAK") != 0) {
    const uint8_t reflectTurns = defenderSide == BattleSide::Player
        ? battle.playerReflectTurns : battle.opponentReflectTurns;
    const uint8_t lightScreenTurns = defenderSide == BattleSide::Player
        ? battle.playerLightScreenTurns : battle.opponentLightScreenTurns;
    if ((physical && reflectTurns) || (!physical && lightScreenTurns))
      damage = std::max<uint32_t>(1U, damage / 2U);
  }
  // BattleScript_EffectSpitUp goes straight from typecalc to
  // adjustsetdamage; it has no randomdamage command in FireRed/Emerald.
  if(!effectIs(move,"SPIT_UP"))
    damage = damage * (85U + random(battle) % 16U) / 100U;
  return static_cast<uint16_t>(std::max<uint32_t>(1, damage));
}

uint8_t BattleEngine::chooseDamageHitCount(BattleState& battle,
    const OwnedPokemon& attacker,MoveId moveId) {
  const FullMoveData* move=findFullMove(moveId);
  if(!move)return 0;
  if(parentalBondEligible(attacker,move))return 2;
  if(effectIs(move,"DOUBLE_HIT")||effectIs(move,"TWINEEDLE"))return 2;
  if(effectIs(move,"TRIPLE_KICK"))return 3;
  else if(effectIs(move,"MULTI_HIT")){
    if(abilityIs(attacker,"SKILL LINK"))return 5;
    // Cmd_setmultihitcounter: 2/3 hits each occur 3/8 of the time and
    // 4/5 hits each 1/8. The count is selected once for the whole move.
    const uint8_t roll=static_cast<uint8_t>(random(battle)&7U);
    return roll<3U?2U:roll<6U?3U:roll==6U?4U:5U;
  }
  return 1;
}

uint16_t BattleEngine::calculateDelayedBaseDamage(const BattleState& battle,
    const OwnedPokemon& attacker, const OwnedPokemon& defender, MoveId moveId,
    const CombatVolatile& attackerVolatile, const CombatVolatile& defenderVolatile) {
  const FullMoveData* move = findFullMove(moveId);
  if (!move || !move->power) return 0;
  const bool physical = move->type == PokemonType::Normal || move->type == PokemonType::Fighting ||
      move->type == PokemonType::Flying || move->type == PokemonType::Poison ||
      move->type == PokemonType::Ground || move->type == PokemonType::Rock ||
      move->type == PokemonType::Bug || move->type == PokemonType::Ghost ||
      move->type == PokemonType::Steel;
  uint32_t rawAttack=battleCalculatedStat(battle,attacker,
      physical?PokemonStat::Attack:PokemonStat::SpAttack);
  uint32_t rawDefense=battleCalculatedStat(battle,defender,
      physical?PokemonStat::Defense:PokemonStat::SpDefense);
  if(physical&&(abilityIs(attacker,"HUGE POWER")||abilityIs(attacker,"PURE POWER")))
    rawAttack*=2U;
  if(physical&&abilityIs(attacker,"HUSTLE"))rawAttack=rawAttack*3U/2U;
  if(physical&&abilityIs(attacker,"GUTS")&&attacker.status!=StatusCondition::None)
    rawAttack=rawAttack*3U/2U;
  if(physical&&abilityIs(defender,"MARVEL SCALE")&&
     defender.status!=StatusCondition::None)rawDefense=rawDefense*3U/2U;
  uint32_t attack=staged(rawAttack,physical?attackerVolatile.attackStage:
                         attackerVolatile.spAttackStage);
  uint32_t defense=std::max<uint32_t>(1U,staged(rawDefense,
      physical?defenderVolatile.defenseStage:defenderVolatile.spDefenseStage));
  if (physical && heldIs(attacker, attackerVolatile, HeldItem::ChoiceBand)) attack = attack * 3U / 2U;
  if (defender.speciesId == 132U &&
      heldIs(defender, defenderVolatile, HeldItem::MetalPowder))
    defense = defense * 3U / 2U;
  uint32_t power = move->power;
  const HeldItem attackItem=heldItemFor(attacker,attackerVolatile);
  if(attackItem==HeldItem::SeaIncense&&move->type==PokemonType::Water)
    power=power*105U/100U;
  else if(heldItemBoostsType(attackItem,move->type))power=power*110U/100U;
  uint32_t damage = (((2U * attacker.level / 5U + 2U) * power * attack / defense) / 50U) + 2U;
  if(physical&&attacker.status==StatusCondition::Burn&&!abilityIs(attacker,"GUTS"))
    damage=std::max<uint32_t>(1U,damage/2U);
  const bool suppressWeather = abilityIs(attacker,"CLOUD NINE") || abilityIs(attacker,"AIR LOCK") ||
      abilityIs(defender,"CLOUD NINE") || abilityIs(defender,"AIR LOCK");
  if (!suppressWeather) {
    if (battle.weather == BattleWeather::Rain) {
      if (move->type == PokemonType::Water) damage = damage * 3U / 2U;
      else if (move->type == PokemonType::Fire) damage /= 2U;
    } else if (battle.weather == BattleWeather::Sun) {
      if (move->type == PokemonType::Fire) damage = damage * 3U / 2U;
      else if (move->type == PokemonType::Water) damage /= 2U;
    }
  }
  if (attacker.currentHp * 3U <= attacker.maximumHp &&
      ((abilityIs(attacker,"OVERGROW") && move->type == PokemonType::Grass) ||
       (abilityIs(attacker,"BLAZE") && move->type == PokemonType::Fire) ||
       (abilityIs(attacker,"TORRENT") && move->type == PokemonType::Water) ||
       (abilityIs(attacker,"SWARM") && move->type == PokemonType::Bug))) damage = damage * 3U / 2U;
  // FireRed intentionally omits critical hits, STAB, type effectiveness and
  // the 85-100% random factor here. The latter is applied on the due turn.
  return static_cast<uint16_t>(std::max<uint32_t>(1U, damage));
}

void BattleEngine::resolveDelayedAttacks(BattleState& battle, PokemonCollection& collection,
                                          BattleActionResult& result) {
  if (!battle.active) return;
  auto resolve = [&](DelayedAttackState& pending, BattleSide targetSide) {
    if (!battle.active || pending.move == MoveId::None || battle.turn < pending.dueTurn) return;
    const DelayedAttackState attack = pending;
    pending = DelayedAttackState{};
    OwnedPokemon* player = CollectionLogic::find(collection, battle.playerUid);
    OwnedPokemon* opponent = currentOpponent(battle);
    if (!player || !opponent) return;
    OwnedPokemon* target = targetSide == BattleSide::Player ? player : opponent;
    OwnedPokemon* source = targetSide == BattleSide::Player ? opponent : player;
    CombatVolatile& targetVolatile = targetSide == BattleSide::Player
        ? battle.playerVolatile : battle.opponentVolatiles[battle.opponentIndex];
    CombatVolatile& sourceVolatile = targetSide == BattleSide::Player
        ? battle.opponentVolatiles[battle.opponentIndex] : battle.playerVolatile;
    // FireRed stores the attacking battlefield position.  The future attack
    // still arrives if the original user fainted or the position has since
    // switched occupants; only a missing/fainted target cancels the impact.
    if (!target->currentHp) return;
    BattleEvent& arrival = appendEvent(result, BattleEventType::DelayedAttackHit,
                                        targetSide, target->uid);
    arrival.move = attack.move;
    const FullMoveData* move = findFullMove(attack.move);
    const BattleSide sourceSide = targetSide == BattleSide::Player
        ? BattleSide::Opponent : BattleSide::Player;
    if (!move || !moveConnects(battle, *move, *source, sourceVolatile, *target, targetVolatile)) {
      appendEvent(result, BattleEventType::MoveMissed, sourceSide, source->uid).move = attack.move;
      return;
    }
    uint16_t damage = static_cast<uint16_t>(std::max<uint32_t>(1U,
        static_cast<uint32_t>(attack.baseDamage) * (85U + random(battle) % 16U) / 100U));
    if (targetVolatile.substituteHp) {
      const uint16_t absorbed = std::min<uint16_t>(targetVolatile.substituteHp, damage);
      targetVolatile.substituteHp = static_cast<uint16_t>(targetVolatile.substituteHp - absorbed);
      // Like every Gen-III hit, one Future Sight impact cannot spill excess
      // damage through a Substitute that it breaks.
      damage = 0;
    }
    damage = applyHeldDamage(battle, *target, targetVolatile, damage);
    const uint16_t before = target->currentHp;
    target->currentHp = damage >= target->currentHp ? 0U
        : static_cast<uint16_t>(target->currentHp - damage);
    appendHpChange(result, targetSide, target->uid, before, target->currentHp);
    triggerHeldItemWithResult(result,*target,targetVolatile,targetSide,
                              &moveEffectsFor(battle,targetSide));
    if (target->currentHp) return;
    appendFaintedOnce(result, targetSide, *target);
    if (targetSide == BattleSide::Player) {
      target->recoverySecondsRemaining = 1;
      if (!firstHealthyPartyMember(collection, target->uid)) {
        battle.active = false; battle.outcome = BattleOutcome::Defeat;
      }
      return;
    }
    result.opponentDefeated = true;
    awardExperience(battle, collection, result);
    finishOpponentFaint(battle, collection, result);
  };
  resolve(battle.delayedToPlayer, BattleSide::Player);
  resolve(battle.delayedToOpponent, BattleSide::Opponent);
}

void BattleEngine::resolveNightmareTurn(BattleState& battle,
                                         PokemonCollection& collection,
                                         BattleActionResult& result) {
  (void)result;
  if (!battle.active) return;
  OwnedPokemon* player = CollectionLogic::find(collection, battle.playerUid);
  OwnedPokemon* opponent = currentOpponent(battle);
  if (!player || !opponent) return;
  // Damage is part of the single ordered Gen-III end-turn pipeline in
  // advanceEndTurnEffects.  This compatibility entry point only performs the
  // immediate wake-up cleanup used by older callers; keeping damage here as
  // well used to double-apply Nightmare on some action paths.
  if(player->status!=StatusCondition::Sleep)battle.playerNightmare=false;
  if(opponent->status!=StatusCondition::Sleep)
    battle.opponentNightmares[battle.opponentIndex]=false;
}

void resolveMagicCoatReflection(BattleState& battle,BattleActionResult& result,
    const FullMoveData& move,OwnedPokemon& reflector,CombatVolatile& reflectorVolatile,
    DedicatedMoveEffectState& reflectorEffects,OwnedPokemon& originalUser,
    CombatVolatile& originalVolatile,DedicatedMoveEffectState& originalEffects,
    BattleSide reflectorSide){
  const BattleSide originalSide=reflectorSide==BattleSide::Player?
      BattleSide::Opponent:BattleSide::Player;
  appendMoveEffect(result,reflectorSide,reflector.uid,static_cast<MoveId>(move.id),
                   BattleMoveEffect::MagicCoatReflected);
  if(!moveConnects(battle,move,reflector,reflectorVolatile,originalUser,originalVolatile)){
    appendEvent(result,BattleEventType::MoveMissed,reflectorSide,reflector.uid).move=
        static_cast<MoveId>(move.id);return;
  }
  const StageSnapshot reflectorStages=snapshotStages(reflectorVolatile);
  const StageSnapshot originalStages=snapshotStages(originalVolatile);
  const VolatileFeedbackSnapshot reflectorFeedback=snapshotVolatileFeedback(reflectorVolatile);
  const VolatileFeedbackSnapshot originalFeedback=snapshotVolatileFeedback(originalVolatile);
  const StatusCondition reflectorStatus=reflector.status,originalStatus=originalUser.status;
  applyStageEffect(battle,reflector,move.effect,reflectorVolatile,originalVolatile,
                   originalUser,&result);
  applyDedicatedMoveEffect(battle,result,move,reflector,reflectorVolatile,reflectorEffects,
                           originalUser,originalVolatile,originalEffects,reflectorSide);
  if(effectIs(&move,"TICKLE")){
    applyStageEffect(battle,reflector,"ATTACK_DOWN",reflectorVolatile,
                     originalVolatile,originalUser,&result);
    applyStageEffect(battle,reflector,"DEFENSE_DOWN",reflectorVolatile,
                     originalVolatile,originalUser,&result);
  }
  if(effectIs(&move,"SWAGGER"))changeStage(originalVolatile.attackStage,2);
  if(effectIs(&move,"FLATTER"))changeStage(originalVolatile.spAttackStage,1);
  if((std::strstr(move.effect,"CONFUSE")||effectIs(&move,"SWAGGER")||
      effectIs(&move,"FLATTER"))&&!abilityIs(originalUser,"OWN TEMPO")&&
      !originalVolatile.confusionTurns){
    const uint8_t safeguard=originalSide==BattleSide::Player?
        battle.playerSafeguardTurns:battle.opponentSafeguardTurns;
    if(safeguard)appendMoveEffect(result,originalSide,originalUser.uid,
        static_cast<MoveId>(move.id),BattleMoveEffect::SafeguardBlocked);
    else originalVolatile.confusionTurns=static_cast<uint8_t>(2U+dedicatedEffectRandom(battle)%4U);
  }
  if(effectIs(&move,"LEECH_SEED")){
    const SpeciesData* species=findSpecies(originalUser.speciesId);
    const bool grass=species&&(species->type1==PokemonType::Grass||
                              species->type2==PokemonType::Grass);
    if(!grass&&!originalVolatile.seeded&&!originalVolatile.substituteHp)
      originalVolatile.seeded=true;
  }
  BattleEngine::applyMoveStatus(battle,reflector,static_cast<MoveId>(move.id),originalUser,result);
  appendStageChanges(result,reflectorSide,reflector.uid,reflectorStages,reflectorVolatile);
  appendStageChanges(result,originalSide,originalUser.uid,originalStages,originalVolatile);
  appendVolatileFeedback(result,reflectorSide,reflector.uid,static_cast<MoveId>(move.id),
                         reflectorFeedback,reflectorVolatile);
  appendVolatileFeedback(result,originalSide,originalUser.uid,static_cast<MoveId>(move.id),
                         originalFeedback,originalVolatile);
  appendStatusChange(result,reflectorSide,reflector.uid,reflectorStatus,reflector.status);
  appendStatusChange(result,originalSide,originalUser.uid,originalStatus,originalUser.status);
}

void resolveSnatchedMove(BattleState& battle,PokemonCollection& collection,
    BattleActionResult& result,const FullMoveData& move,OwnedPokemon& snatcher,
    CombatVolatile& snatcherVolatile,DedicatedMoveEffectState& snatcherEffects,
    OwnedPokemon& originalUser,CombatVolatile& originalVolatile,
    DedicatedMoveEffectState& originalEffects,BattleSide snatcherSide){
  appendMoveEffect(result,snatcherSide,snatcher.uid,static_cast<MoveId>(move.id),
                   BattleMoveEffect::MoveSnatched);
  const StageSnapshot stages=snapshotStages(snatcherVolatile);
  const VolatileFeedbackSnapshot feedback=snapshotVolatileFeedback(snatcherVolatile);
  const uint16_t hpBefore=snatcher.currentHp;
  const StatusCondition statusBefore=snatcher.status;
  applyStageEffect(battle,snatcher,move.effect,snatcherVolatile,originalVolatile,
                   originalUser,&result);
  applyDedicatedMoveEffect(battle,result,move,snatcher,snatcherVolatile,snatcherEffects,
                           originalUser,originalVolatile,originalEffects,snatcherSide);
  const char* effect=move.effect;
  if(std::strstr(effect,"BULK_UP")){changeStage(snatcherVolatile.attackStage,1);changeStage(snatcherVolatile.defenseStage,1);}
  if(std::strstr(effect,"CALM_MIND")){changeStage(snatcherVolatile.spAttackStage,1);changeStage(snatcherVolatile.spDefenseStage,1);}
  if(std::strstr(effect,"DRAGON_DANCE")){changeStage(snatcherVolatile.attackStage,1);changeStage(snatcherVolatile.speedStage,1);}
  if(std::strstr(effect,"COSMIC_POWER")){changeStage(snatcherVolatile.defenseStage,1);changeStage(snatcherVolatile.spDefenseStage,1);}
  if(std::strstr(effect,"DEFENSE_CURL"))changeStage(snatcherVolatile.defenseStage,1);
  if(std::strstr(effect,"MINIMIZE"))changeStage(snatcherVolatile.evasionStage,1);
  if(std::strstr(effect,"PSYCH_UP")){
    snatcherVolatile.attackStage=originalVolatile.attackStage;
    snatcherVolatile.defenseStage=originalVolatile.defenseStage;
    snatcherVolatile.spAttackStage=originalVolatile.spAttackStage;
    snatcherVolatile.spDefenseStage=originalVolatile.spDefenseStage;
    snatcherVolatile.speedStage=originalVolatile.speedStage;
    snatcherVolatile.accuracyStage=originalVolatile.accuracyStage;
    snatcherVolatile.evasionStage=originalVolatile.evasionStage;
  }
  if(std::strstr(effect,"BELLY_DRUM")&&
     snatcher.currentHp>std::max<uint16_t>(1U,snatcher.maximumHp/2U)&&
     snatcherVolatile.attackStage<kMaximumBattleStatStage){
    snatcher.currentHp=static_cast<uint16_t>(snatcher.currentHp-
        std::max<uint16_t>(1U,snatcher.maximumHp/2U));
    snatcherVolatile.attackStage=kMaximumBattleStatStage;
  }
  if(std::strstr(effect,"SUBSTITUTE")&&!snatcherVolatile.substituteHp&&
     snatcher.currentHp>std::max<uint16_t>(1U,snatcher.maximumHp/4U)){
    const uint16_t cost=std::max<uint16_t>(1U,snatcher.maximumHp/4U);
    snatcher.currentHp-=cost;snatcherVolatile.substituteHp=cost;
    snatcherVolatile.trappedTurns=0;
  }
  if(std::strstr(effect,"RESTORE_HP")||std::strstr(effect,"SOFTBOILED")||
     std::strstr(effect,"SYNTHESIS")||std::strstr(effect,"MORNING_SUN")||
     std::strstr(effect,"MOONLIGHT"))
    snatcher.currentHp=std::min<uint16_t>(snatcher.maximumHp,
        static_cast<uint16_t>(snatcher.currentHp+recoveryMoveAmount(
            battle,snatcher,originalUser,effect)));
  if(effectIs(&move,"REST")&&snatcher.currentHp<snatcher.maximumHp&&
     !abilityIs(snatcher,"INSOMNIA")&&!abilityIs(snatcher,"VITAL SPIRIT")&&
     !battle.playerMoveEffects.uproar&&
     !battle.opponentMoveEffects[battle.opponentIndex].uproar){
    snatcher.currentHp=snatcher.maximumHp;snatcher.status=StatusCondition::Sleep;
    // trysetrest writes STATUS1_SLEEP_TURN(3) in both FireRed and Emerald.
    snatcherVolatile.sleepTurns=3;
  }
  if(effectIs(&move,"REFRESH")&&(snatcher.status==StatusCondition::Poison||
     snatcher.status==StatusCondition::BadlyPoisoned||snatcher.status==StatusCondition::Burn||
     snatcher.status==StatusCondition::Paralysis))snatcher.status=StatusCondition::None;
  if(effectIs(&move,"HEAL_BELL")){
    if(snatcherSide==BattleSide::Player){
      for(uint8_t slot=0;slot<kPartyCapacity;++slot)
        if(OwnedPokemon* member=CollectionLogic::active(collection,slot))
          if(canBeCuredByBell(move,*member))member->status=StatusCondition::None;
    }else for(uint8_t i=0;i<battle.opponentCount;++i)
      if(canBeCuredByBell(move,battle.opponents[i]))
        battle.opponents[i].status=StatusCondition::None;
    appendMoveEffect(result,snatcherSide,snatcher.uid,static_cast<MoveId>(move.id),BattleMoveEffect::TeamCured);
  }
  auto setSideCondition=[&](uint8_t& turns,BattleMoveEffect set){
    if(!turns){turns=5;appendMoveEffect(result,snatcherSide,snatcher.uid,
        static_cast<MoveId>(move.id),set);}
  };
  if(effectIs(&move,"SAFEGUARD"))setSideCondition(snatcherSide==BattleSide::Player?
      battle.playerSafeguardTurns:battle.opponentSafeguardTurns,BattleMoveEffect::SafeguardSet);
  if(effectIs(&move,"LIGHT_SCREEN"))setSideCondition(snatcherSide==BattleSide::Player?
      battle.playerLightScreenTurns:battle.opponentLightScreenTurns,BattleMoveEffect::LightScreenSet);
  if(effectIs(&move,"REFLECT"))setSideCondition(snatcherSide==BattleSide::Player?
      battle.playerReflectTurns:battle.opponentReflectTurns,BattleMoveEffect::ReflectSet);
  if(effectIs(&move,"MIST"))setSideCondition(snatcherSide==BattleSide::Player?
      battle.playerMistTurns:battle.opponentMistTurns,BattleMoveEffect::MistSet);
  if(effectIs(&move,"STOCKPILE")){
    snatcherVolatile.stockpileCount=std::min<uint8_t>(3U,
        static_cast<uint8_t>(snatcherVolatile.stockpileCount+1U));
  }
  if(effectIs(&move,"SWALLOW")&&snatcherVolatile.stockpileCount){
    const uint8_t count=snatcherVolatile.stockpileCount;snatcherVolatile.stockpileCount=0;
    const uint16_t healing=std::max<uint16_t>(1U,
        static_cast<uint16_t>(snatcher.maximumHp/(1U<<(3U-count))));
    snatcher.currentHp=std::min<uint16_t>(snatcher.maximumHp,
        static_cast<uint16_t>(snatcher.currentHp+healing));
  }
  appendHpChange(result,snatcherSide,snatcher.uid,hpBefore,snatcher.currentHp);
  appendStageChanges(result,snatcherSide,snatcher.uid,stages,snatcherVolatile);
  appendVolatileFeedback(result,snatcherSide,snatcher.uid,static_cast<MoveId>(move.id),
                         feedback,snatcherVolatile);
  appendStatusChange(result,snatcherSide,snatcher.uid,statusBefore,snatcher.status);
}

void BattleEngine::enemyTurn(BattleState& battle, PokemonCollection& collection,
                             OwnedPokemon& player, BattleActionResult& result, uint8_t moveSlot) {
  OwnedPokemon* opponent = currentOpponent(battle);
  if (!battle.active || !opponent || opponent->currentHp == 0) return;
  if (moveSlot == 0xFEU) { result.enemyActed = true; return; }
  CombatVolatile& enemyVolatile = battle.opponentVolatiles[battle.opponentIndex];
  BideState& enemyBide = battle.opponentBides[battle.opponentIndex];
  DedicatedMoveEffectState& enemyEffects=
      battle.opponentMoveEffects[battle.opponentIndex];
  bool forcedLocked=false;
  if (enemyBide.turns) {
    const uint8_t forcedBide = moveSlotFor(*opponent, static_cast<MoveId>(117), false);
    if (forcedBide < kMoveSlots) moveSlot = forcedBide;
    else enemyBide = BideState{};
  } else if (battle.opponentMoveEffects[battle.opponentIndex].lockedMoveTurns) {
    const uint8_t lockedSlot=moveSlotFor(*opponent,
        battle.opponentMoveEffects[battle.opponentIndex].lockedMove,false);
    if(lockedSlot<kMoveSlots){moveSlot=lockedSlot;forcedLocked=true;}
    else battle.opponentMoveEffects[battle.opponentIndex].lockedMoveTurns=0;
  } else if (enemyVolatile.chargingMove != MoveId::None) {
    const uint8_t forcedCharge=moveSlotFor(*opponent,enemyVolatile.chargingMove,false);
    if(forcedCharge<kMoveSlots)moveSlot=forcedCharge;
    else enemyVolatile.chargingMove=MoveId::None;
  } else if (enemyVolatile.encoreMove != MoveId::None) {
    const uint8_t forcedEncore = moveSlotFor(*opponent, enemyVolatile.encoreMove);
    if (forcedEncore < kMoveSlots) moveSlot = forcedEncore;
    else clearEncore(enemyVolatile, battle.opponentEncoreTurns[battle.opponentIndex]);
  }
  if (enemyCanChooseHealingItem(battle, *opponent, enemyVolatile, enemyBide,
                                enemyEffects)) {
    DedicatedMoveEffectState& effects=battle.opponentMoveEffects[battle.opponentIndex];
    effects.furyCutterCount=0;
    effects.destinyBond=false;
    effects.grudge=false;
    const uint16_t hpBefore = opponent->currentHp;
    // Every AI-controlled non-wild battle shares this path: regular trainers,
    // Gym leaders, the Elite Four/Champion and Battle Tower challengers. High
    // level teams must not keep using the mid-game Super Potion.
    const BattleItem healingItem = opponent->level >= 70 ? BattleItem::HyperPotion
        : opponent->level >= 30 ? BattleItem::SuperPotion : BattleItem::Potion;
    const uint16_t amount = healingItem == BattleItem::HyperPotion ? 200U
        : healingItem == BattleItem::SuperPotion ? 50U : 20U;
    opponent->currentHp = std::min<uint16_t>(opponent->maximumHp, static_cast<uint16_t>(opponent->currentHp + amount));
    --battle.opponentItemUses; result.enemyActed = true; result.enemyItemUsed = true;
    result.enemyItem = healingItem;
    BattleEvent& itemEvent = appendEvent(result, BattleEventType::ItemUsed, BattleSide::Opponent, opponent->uid);
    itemEvent.value = static_cast<uint16_t>(result.enemyItem);
    BattleEvent& hpEvent = appendEvent(result, BattleEventType::HpChanged, BattleSide::Opponent, opponent->uid);
    hpEvent.before = hpBefore; hpEvent.after = opponent->currentHp;
    return;
  }
  // AtkCanceller clears these flags before checking recharge, Truant,
  // flinch, sleep, confusion, paralysis, Disable, Taunt or Imprison.  The
  // previous implementation waited until the move animation path, letting a
  // failed action incorrectly preserve Destiny Bond and Grudge.
  enemyEffects.destinyBond=false;
  enemyEffects.grudge=false;
  if (enemyVolatile.recharging) {
    enemyVolatile.recharging=false;
    cancelMultiTurnMoves(enemyEffects,enemyVolatile,enemyBide,result,
                         BattleSide::Opponent,opponent->uid);
    appendEvent(result,BattleEventType::CannotMove,BattleSide::Opponent,opponent->uid).value=1;
    result.enemyActed=true;
    return;
  }
  if (truantLoafsThisTurn(battle,*opponent,enemyEffects)) {
    cancelMultiTurnMoves(enemyEffects,enemyVolatile,enemyBide,result,
                         BattleSide::Opponent,opponent->uid);
    appendEvent(result,BattleEventType::CannotMove,BattleSide::Opponent,opponent->uid).value=9;
    result.enemyActed=true;return;
  }
  if (enemyVolatile.flinched) {
    enemyVolatile.flinched=false;
    if(abilityIs(*opponent,"STEADFAST")){
      const StageSnapshot before=snapshotStages(enemyVolatile);
      if(changeStage(enemyVolatile.speedStage,1)){
        appendAbilityActivation(result,BattleSide::Opponent,*opponent);
        appendStageChanges(result,BattleSide::Opponent,opponent->uid,before,enemyVolatile);
      }
    }
    cancelMultiTurnMoves(enemyEffects,enemyVolatile,enemyBide,result,
                         BattleSide::Opponent,opponent->uid);
    // Flinch is announced when it actually prevents the target's action,
    // matching FireRed's turn order.  Every flinch source (move secondary
    // effect, King's Rock and terrain-based Secret Power) converges here.
    // The player-side AtkCanceller already emitted this event; omitting it on
    // the opponent side made the foe silently lose its turn.
    appendEvent(result,BattleEventType::CannotMove,BattleSide::Opponent,
                opponent->uid).value=2;
    result.enemyActed=true;return;
  }
  if (moveSlot >= kMoveSlots || opponent->moves[moveSlot] == MoveId::None ||
      (opponent->movePp[moveSlot] == 0&&!forcedLocked&&
       enemyVolatile.chargingMove!=opponent->moves[moveSlot]))
    moveSlot = chooseEnemyMove(battle, player);
  // Like the player, an opponent with moves but no legal command must use
  // Struggle. This includes every move reaching zero PP as well as Disable,
  // Taunt, Torment or Imprison blocking the remaining commands. A completely
  // empty synthetic battler still has no command at all.
  bool enemyKnowsMove=false;
  for(uint8_t slot=0;slot<kMoveSlots;++slot){
    if(opponent->moves[slot]!=MoveId::None&&findFullMove(opponent->moves[slot])){
      enemyKnowsMove=true;break;
    }
  }
  const bool enemyUsingStruggle=moveSlot>=kMoveSlots&&enemyKnowsMove;
  if(moveSlot>=kMoveSlots&&!enemyUsingStruggle){result.enemyActed=true;return;}
  MoveId enemyMove = enemyUsingStruggle ? MoveId::Struggle : opponent->moves[moveSlot];
  const FullMoveData* enemyMoveData = findFullMove(enemyMove);
  const DedicatedMoveEffectState& selectionEffects=
      battle.opponentMoveEffects[battle.opponentIndex];
  if((battle.playerMoveEffects.imprisoned&&pokemonKnowsMove(player,enemyMove))||
     (selectionEffects.tauntTurns&&enemyMoveData&&enemyMoveData->power==0)||
     (selectionEffects.tormented&&enemyVolatile.lastMoveUsed==enemyMove)){
    BattleEvent& blocked=appendEvent(result,BattleEventType::CannotMove,
                                     BattleSide::Opponent,opponent->uid);
    blocked.value=(selectionEffects.tauntTurns&&enemyMoveData&&enemyMoveData->power==0)?11U:
        (selectionEffects.tormented&&enemyVolatile.lastMoveUsed==enemyMove)?12U:10U;
    cancelMultiTurnMoves(enemyEffects,enemyVolatile,enemyBide,result,
                         BattleSide::Opponent,opponent->uid);
    result.enemyActed=true;return;
  }
  if (enemyVolatile.confusionTurns) {
    --enemyVolatile.confusionTurns;
    // The original attack canceller only rolls self-damage while at least
    // one confusion turn remains.  Reaching zero means the Pokemon snaps out
    // and immediately proceeds with its selected command.
    if (!enemyVolatile.confusionTurns) {
      appendMoveEffect(result, BattleSide::Opponent, opponent->uid,
                       MoveId::None, BattleMoveEffect::ConfusionEnded);
    } else {
      appendMoveEffect(result, BattleSide::Opponent, opponent->uid,
                       MoveId::None, BattleMoveEffect::ConfusionActive);
      if (random(battle) % 2U == 0U) {
        const uint16_t before = opponent->currentHp;
        const uint16_t hurt = confusionSelfDamage(battle,*opponent,enemyVolatile);
        opponent->currentHp = hurt >= opponent->currentHp ? 0U : static_cast<uint16_t>(opponent->currentHp - hurt);
        BattleEvent& blocked=appendEvent(result,BattleEventType::CannotMove,BattleSide::Opponent,opponent->uid);
        blocked.value=3;
        // Keep text and health as two ordered journal events.  Encoding the HP
        // pair inside CannotMove made the retained HUD skip this transition;
        // the following attack then started from an unrelated HP value and a
        // lethal self-hit could play its faint animation over a non-empty bar.
        appendHpChange(result,BattleSide::Opponent,opponent->uid,before,
                       opponent->currentHp);
        cancelMultiTurnMoves(enemyEffects,enemyVolatile,enemyBide,result,
                             BattleSide::Opponent,opponent->uid);
        result.enemyActed=true;
        if (!opponent->currentHp) {
          appendFaintedOnce(result,BattleSide::Opponent,*opponent);
          result.opponentDefeated=true;
          awardExperience(battle,collection,result);
          finishOpponentFaint(battle,collection,result);
        }
        return;
      }
    }
  }
  if (battle.opponentMoveEffects[battle.opponentIndex].attractedToUid&&
      battle.opponentMoveEffects[battle.opponentIndex].attractedToUid==player.uid &&
      random(battle)%2U==0U) {
    appendEvent(result,BattleEventType::CannotMove,BattleSide::Opponent,opponent->uid).value=13;
    cancelMultiTurnMoves(enemyEffects,enemyVolatile,enemyBide,result,
                         BattleSide::Opponent,opponent->uid);
    result.enemyActed=true;return;
  }
  if (enemyVolatile.disabledMove == enemyMove) {
    appendEvent(result,BattleEventType::CannotMove,BattleSide::Opponent,opponent->uid).value=4;
    cancelMultiTurnMoves(enemyEffects,enemyVolatile,enemyBide,result,
                         BattleSide::Opponent,opponent->uid);
    result.enemyActed=true;return;
  }
  if (opponent->status == StatusCondition::Sleep) {
    if (!enemyVolatile.sleepTurns) enemyVolatile.sleepTurns=static_cast<uint8_t>(2U+random(battle)%4U);
    const bool woke=wakeAfterSleepTick(*opponent,enemyVolatile);
    if (!woke && !effectIs(enemyMoveData, "SNORE")&&
        !effectIs(enemyMoveData,"SLEEP_TALK")) {
      appendEvent(result,BattleEventType::CannotMove,BattleSide::Opponent,opponent->uid).value=6;
      if(enemyEffects.lockedMoveTurns||enemyBide.turns||
         enemyVolatile.chargingMove!=MoveId::None)
        cancelMultiTurnMoves(enemyEffects,enemyVolatile,enemyBide,result,
                             BattleSide::Opponent,opponent->uid);
      result.enemyActed=true;return;
    }
    if(woke){
      opponent->status=StatusCondition::None;
      appendStatusChange(result,BattleSide::Opponent,opponent->uid,StatusCondition::Sleep,opponent->status);
    }
    if(opponent->status!=StatusCondition::Sleep)
      battle.opponentNightmares[battle.opponentIndex]=false;
  } else if (opponent->status == StatusCondition::Frozen) {
    // Flame Wheel and Sacred Fire are EFFECT_THAW_HIT: FireRed lets the
    // frozen user execute them and cures the freeze before the attack.
    if (!effectIs(enemyMoveData,"THAW_HIT")&&random(battle)%5U) {
      appendEvent(result,BattleEventType::CannotMove,BattleSide::Opponent,
                  opponent->uid).value=7;result.enemyActed=true;return;
    }
    opponent->status=StatusCondition::None;
    appendStatusChange(result,BattleSide::Opponent,opponent->uid,StatusCondition::Frozen,opponent->status);
  } else if (opponent->status == StatusCondition::Paralysis && random(battle)%4U==0U) {
    appendEvent(result,BattleEventType::CannotMove,BattleSide::Opponent,opponent->uid).value=8;
    cancelMultiTurnMoves(enemyEffects,enemyVolatile,enemyBide,result,
                         BattleSide::Opponent,opponent->uid);
    result.enemyActed=true;return;
  }
  if (enemyBide.turns) {
    result.enemyMoveUsed = static_cast<MoveId>(117);
    result.enemyActed = true;
    if (--enemyBide.turns) {
      appendMoveEffect(result, BattleSide::Opponent, opponent->uid,
                       static_cast<MoveId>(117), BattleMoveEffect::BideStoring);
      return;
    }
    const uint16_t stored = enemyBide.damage;
    enemyBide = BideState{};
    appendMoveEffect(result, BattleSide::Opponent, opponent->uid,
                     static_cast<MoveId>(117), BattleMoveEffect::BideUnleashed);
    const FullMoveData* bideMove = findFullMove(static_cast<MoveId>(117));
    const SpeciesData* playerSpecies=findSpecies(player.speciesId);
    const bool immune = playerSpecies&&typeMultiplier100(PokemonType::Normal,*playerSpecies)==0U;
    const bool protectedTarget=battle.playerVolatile.protectedThisTurn;
    const bool connects = stored && !immune && !protectedTarget && bideMove &&
        moveConnects(battle,*bideMove,*opponent,enemyVolatile,player,battle.playerVolatile);
    if (!stored || immune || protectedTarget || !bideMove) {
      appendMoveEffect(result, BattleSide::Player, player.uid, static_cast<MoveId>(117),
                       BattleMoveEffect::Failed);
      return;
    }
    if (!connects) {
      appendEvent(result,BattleEventType::MoveMissed,BattleSide::Opponent,
                  opponent->uid).move=static_cast<MoveId>(117);
      return;
    }
    uint16_t applied = static_cast<uint16_t>(std::min<uint32_t>(65535U,
        static_cast<uint32_t>(stored) * 2U));
    if (battle.playerVolatile.substituteHp) {
      const uint16_t absorbed=std::min<uint16_t>(battle.playerVolatile.substituteHp,applied);
      battle.playerVolatile.substituteHp=static_cast<uint16_t>(battle.playerVolatile.substituteHp-absorbed);
      applied=static_cast<uint16_t>(applied-absorbed);
    }
    applied=applyHeldDamage(battle,player,battle.playerVolatile,applied);
    if(battle.playerEndureThisTurn&&applied>=player.currentHp&&player.currentHp)
      applied=static_cast<uint16_t>(player.currentHp-1U);
    const uint16_t before=player.currentHp;
    player.currentHp=applied>=player.currentHp?0U:static_cast<uint16_t>(player.currentHp-applied);
    result.damageTaken=static_cast<uint16_t>(before-player.currentHp);
    appendHpChange(result,BattleSide::Player,player.uid,before,player.currentHp);
    triggerHeldItemWithResult(result,player,battle.playerVolatile,BattleSide::Player,
                              &battle.playerMoveEffects);
    if(!player.currentHp){
      resolveFaintVows(result,BattleSide::Player,player,battle.playerMoveEffects,
                       *opponent,static_cast<MoveId>(117),moveSlot,result.damageTaken);
      player.recoverySecondsRemaining=1;
      appendFaintedOnce(result,BattleSide::Player,player);
      if(!opponent->currentHp){
        result.opponentDefeated=true;
        awardExperience(battle,collection,result);
        finishOpponentFaint(battle,collection,result);
      }
      if(!firstHealthyPartyMember(collection,player.uid)){battle.active=false;battle.outcome=BattleOutcome::Defeat;}
    }
    return;
  }
  const MoveId enemyCommandMove=enemyMove;
  result.enemyMoveUsed = enemyMove;
  uint16_t opponentHpBeforeMove = opponent->currentHp;
  StatusCondition opponentStatusBeforeMove = opponent->status;
  BattleEvent& moveEvent = appendEvent(result, BattleEventType::MoveUsed, BattleSide::Opponent, opponent->uid);
  moveEvent.move = enemyCommandMove;
  BattleEvent* resolvedMoveEvent = &moveEvent;
  const uint8_t enemyFeedbackStart = result.eventCount;
  DedicatedMoveEffectState& playerEffects=battle.playerMoveEffects;
  if(isMoveCaller(enemyMoveData)){
    const MoveId called=resolveCalledMove(battle,collection,*opponent,enemyVolatile,
                                          player,battle.playerVolatile,
                                          enemyMoveData,BattleSide::Opponent);
    if(called==MoveId::None){
      appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyCommandMove,
                       BattleMoveEffect::Failed);
      result.enemyActed=true;return;
    }
    enemyMove=called;enemyMoveData=findFullMove(called);result.enemyMoveUsed=called;
    BattleEvent& calledEvent=appendEvent(result,BattleEventType::MoveUsed,
                                         BattleSide::Opponent,opponent->uid);
    calledEvent.move=called;
    resolvedMoveEvent=&calledEvent;
  }
  if(effectIs(enemyMoveData,"MIMIC")||effectIs(enemyMoveData,"SKETCH")){
    copyLastMove(battle,result,*opponent,battle.playerVolatile,moveSlot,
                 *enemyMoveData,BattleSide::Opponent,effectIs(enemyMoveData,"SKETCH"));
    result.enemyActed=true;return;
  }
  if(effectIs(enemyMoveData,"FURY_CUTTER"))
    enemyEffects.furyCutterCount=static_cast<uint8_t>(std::min<uint8_t>(5U,enemyEffects.furyCutterCount+1U));
  if(!effectIs(enemyMoveData,"RAGE"))enemyEffects.rageActive=false;
  enemyVolatile.lastMoveUsed = enemyMove;
  if (effectIs(enemyMoveData, "BATON_PASS")) {
    opponent->movePp[moveSlot] = static_cast<uint8_t>(opponent->movePp[moveSlot] -
        std::min<uint8_t>(opponent->movePp[moveSlot],
                          pressurePpCost(enemyMoveData,player)));
    if (!opponentBatonPass(battle, collection, result, enemyMove))
      appendMoveEffect(result, BattleSide::Opponent, opponent->uid, enemyMove,
                       BattleMoveEffect::Failed);
    result.enemyActed = true;
    return;
  }
  if(effectIs(enemyMoveData,"BIDE")){
    opponent->movePp[moveSlot]=static_cast<uint8_t>(opponent->movePp[moveSlot]-
        std::min<uint8_t>(opponent->movePp[moveSlot],
                          pressurePpCost(enemyMoveData,player)));
    enemyBide.damage=0;
    enemyBide.turns=2;
    appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,
                     BattleMoveEffect::BideStoring);
    result.enemyActed=true;
    return;
  }
  const bool releasingCharge=enemyVolatile.chargingMove==enemyMove;
  // FireRed's animation VM receives gAnimMoveTurn independently from move
  // damage/result data. Preserve that presentation input in the journal:
  // 0 is setup/first strike, 1 is release/second strike, and Spit Up/Swallow
  // use the actual STOCKPILE count (1..3).
  resolvedMoveEvent->stageBefore = releasingCharge ? 1 : 0;
  if (effectIs(enemyMoveData,"SPIT_UP") || effectIs(enemyMoveData,"SWALLOW"))
    resolvedMoveEvent->stageBefore = static_cast<int8_t>(enemyVolatile.stockpileCount);
  // DIG owns two visually distinct FireRed scripts.  Preserve which half of
  // the move this journal entry represents so presentation can keep the
  // battler underground between turns instead of replaying one generic cel.
  if (static_cast<uint16_t>(enemyMove) == 91U)
    resolvedMoveEvent->value = releasingCharge ? 2U : 1U;
  if(!releasingCharge&&!forcedLocked&&!enemyUsingStruggle)
    opponent->movePp[moveSlot]=static_cast<uint8_t>(opponent->movePp[moveSlot]-
        std::min<uint8_t>(opponent->movePp[moveSlot],
                          pressurePpCost(enemyMoveData,player)));
  if(isChargingMove(battle,enemyMoveData,*opponent,player)&&!releasingCharge){
    enemyVolatile.chargingMove=enemyMove;
    if(effectIs(enemyMoveData,"SKULL_BASH")){
      const int8_t before=enemyVolatile.defenseStage;
      if(changeStage(enemyVolatile.defenseStage,1)){
        BattleEvent& raised=appendEvent(result,BattleEventType::StatChanged,
                                        BattleSide::Opponent,opponent->uid);
        raised.value=1;raised.stageBefore=before;raised.stageAfter=enemyVolatile.defenseStage;
      }
    }
    result.enemyActed=true;return;
  }
  enemyVolatile.chargingMove=MoveId::None;
  if(enemyMoveData&&enemyMoveData->snatchAffected&&playerEffects.snatch){
    playerEffects.snatch=false;
    resolveSnatchedMove(battle,collection,result,*enemyMoveData,player,battle.playerVolatile,
        playerEffects,*opponent,enemyVolatile,enemyEffects,BattleSide::Player);
    result.enemyActed=true;return;
  }
  if(enemyMoveData&&enemyMoveData->magicCoatAffected&&
     (playerEffects.magicCoat||defenderAbilityApplies(*opponent,player,"MAGIC BOUNCE"))){
    if(!defenderAbilityApplies(*opponent,player,"MAGIC BOUNCE"))playerEffects.magicCoat=false;
    resolveMagicCoatReflection(battle,result,*enemyMoveData,player,battle.playerVolatile,
        playerEffects,*opponent,enemyVolatile,enemyEffects,BattleSide::Player);
    result.enemyActed=true;return;
  }
  const bool delayedAttack = effectIs(enemyMoveData, "FUTURE_SIGHT");
  const bool delayedSlotFree = battle.delayedToPlayer.move == MoveId::None;
  const bool dampBlocked=effectIs(enemyMoveData,"EXPLOSION")&&
      (abilityIs(*opponent,"DAMP")||
       defenderAbilityApplies(*opponent,player,"DAMP"));
  const OhkoBlockReason enemyOhkoBlock=ohkoBlockReason(enemyMoveData,*opponent,player);
  const bool requirementMet = moveUserRequirementMet(battle, enemyMoveData, *opponent,
      enemyVolatile,player) &&
      moveRequirementMet(enemyMoveData, player, battle.playerVolatile) &&
      enemyOhkoBlock==OhkoBlockReason::None &&
      (!effectIs(enemyMoveData,"NIGHTMARE") || !battle.playerNightmare) &&
      (!effectIs(enemyMoveData,"FOCUS_PUNCH") || !enemyEffects.lastDamageReceived) &&
      (!effectIs(enemyMoveData,"FAKE_OUT") ||
       battle.turn<=static_cast<uint16_t>(enemyEffects.enteredTurn+1U)) &&
      (!delayedAttack || delayedSlotFree);
  const bool protectAllowsMove=!enemyMoveData||!enemyMoveData->protectAffected||
      !battle.playerVolatile.protectedThisTurn;
  StatusCondition attemptedPrimaryStatus=StatusCondition::None;
  const bool primaryStatusConflict=primaryStatusConflicts(
      battle,enemyMoveData,player,attemptedPrimaryStatus);
  const bool soundproofBlocked=enemyMoveData&&isSoundMove(*enemyMoveData)&&
      (defenderAbilityApplies(*opponent,player,"SOUNDPROOF")||
       defenderAbilityApplies(*opponent,player,"CACOPHONY"));
  const bool moveAbsorbed=enemyMoveData&&
      absorbingAbilityBlocksMove(battle,enemyMoveData,*opponent,player);
  const bool connected = delayedAttack ? requirementMet : requirementMet && enemyMoveData &&
      !primaryStatusConflict&&protectAllowsMove&&(moveTargetsUser(enemyMoveData) ||
       moveConnects(battle, *enemyMoveData, *opponent, enemyVolatile, player,
                    battle.playerVolatile));
  uint16_t effectiveness = 100;
  gPresentHealRoll=false;
  uint16_t beatUpDamage[5]{},beatUpEffectiveness[5]{100,100,100,100,100};
  bool beatUpCritical[5]{};
  const bool beatUpMove=effectIs(enemyMoveData,"BEAT_UP");
  uint8_t plannedHitCount=0;
  if(connected&&!delayedAttack){
    if(beatUpMove){
      plannedHitCount=beatUpDamageHits(battle,collection,BattleSide::Opponent,*opponent,
          enemyVolatile,player,beatUpDamage,beatUpEffectiveness,beatUpCritical);
    }else plannedHitCount=chooseDamageHitCount(battle,*opponent,enemyMove);
  }
  uint32_t damageTotal=0;
  uint16_t damage=0;
  uint16_t crashEffectiveness=100U;
  const uint16_t projectedCrash=(!connected&&requirementMet&&
      effectIs(enemyMoveData,"RECOIL_IF_MISS"))?
      calculateDamage(battle,*opponent,player,enemyMove,enemyVolatile,
          battle.playerVolatile,&crashEffectiveness,nullptr,false):0U;
  if (connected && delayedAttack) {
    battle.delayedToPlayer.move = enemyMove;
    battle.delayedToPlayer.baseDamage = calculateDelayedBaseDamage(
        battle, *opponent, player, enemyMove, enemyVolatile, battle.playerVolatile);
    battle.delayedToPlayer.dueTurn = static_cast<uint16_t>(battle.turn + 2U);
    appendMoveEffect(result, BattleSide::Opponent, opponent->uid, enemyMove,
                     BattleMoveEffect::FutureAttackSet);
  }
  const uint16_t playerHpBefore = player.currentHp;
  const bool playerHadSubstitute=battle.playerVolatile.substituteHp!=0;
  StatusCondition playerStatusBefore = player.status;
  uint16_t appliedDamage=0;
  uint16_t substituteDamage=0;
  uint8_t actualHitCount=0;
  const bool parentalBondMove=parentalBondEligible(*opponent,enemyMoveData);
  const bool repeatedMove=isSameTurnMultiHit(enemyMoveData)||parentalBondMove;
  if(connected){
    for(uint8_t hit=0;hit<plannedHitCount&&player.currentHp&&opponent->currentHp;++hit){
      if(hit&&effectIs(enemyMoveData,"TRIPLE_KICK")&&
         !moveConnects(battle,*enemyMoveData,*opponent,enemyVolatile,
                       player,battle.playerVolatile))break;
      uint16_t hitEffectiveness=100U;
      bool hitCritical=false;
      uint16_t strike=0;
      if(beatUpMove){
        strike=beatUpDamage[hit];
        hitEffectiveness=beatUpEffectiveness[hit];
        hitCritical=beatUpCritical[hit];
      }else{
        const uint16_t kickPower=effectIs(enemyMoveData,"TRIPLE_KICK")?
            static_cast<uint16_t>(10U*(hit+1U)):0U;
        strike=calculateDamage(battle,*opponent,player,enemyMove,
            enemyVolatile,battle.playerVolatile,&hitEffectiveness,
            &hitCritical,true,kickPower);
        if(parentalBondMove&&hit)strike=std::max<uint16_t>(1U,strike/4U);
      }
      if(gPresentHealRoll){
        const uint16_t before=player.currentHp;
        player.currentHp=std::min<uint16_t>(player.maximumHp,
            static_cast<uint16_t>(player.currentHp+
            std::max<uint16_t>(1U,player.maximumHp/4U)));
        appendHpChange(result,BattleSide::Player,player.uid,before,player.currentHp);
        ++actualHitCount;
        break;
      }
      effectiveness=hitEffectiveness;
      if(!hitEffectiveness)break;
      if(actualHitCount&&repeatedMove){
        BattleEvent& replay=appendEvent(result,BattleEventType::MultiHitStrike,
                                         BattleSide::Opponent,opponent->uid);
        replay.move=enemyMove;
        replay.stageBefore=static_cast<int8_t>(actualHitCount);
      }
      ++actualHitCount;
      damageTotal=std::min<uint32_t>(65535U,damageTotal+strike);
      const uint16_t before=player.currentHp;
      uint16_t contactDamage=0;
      bool endedByEndure=false;
      if(battle.playerVolatile.substituteHp){
        const uint16_t absorbed=std::min<uint16_t>(battle.playerVolatile.substituteHp,strike);
        contactDamage=absorbed;
        substituteDamage=static_cast<uint16_t>(std::min<uint32_t>(65535U,
            static_cast<uint32_t>(substituteDamage)+absorbed));
        battle.playerVolatile.substituteHp=static_cast<uint16_t>(
            battle.playerVolatile.substituteHp-absorbed);
        // The hit which breaks a Substitute never spills. A later hit in the
        // same move may then reach the real target, exactly like Gen III.
      }else if(enemyMoveData&&effectIs(enemyMoveData,"FALSE_SWIPE")&&
               strike>=player.currentHp){
        strike=player.currentHp>1U?static_cast<uint16_t>(player.currentHp-1U):0U;
        player.currentHp=1U;
        contactDamage=strike;
      }else{
        if(battle.playerEndureThisTurn&&strike>=player.currentHp&&player.currentHp){
          strike=static_cast<uint16_t>(player.currentHp-1U);
          endedByEndure=true;
        }else strike=applyHeldDamage(battle,player,battle.playerVolatile,strike);
        player.currentHp=strike>=player.currentHp?0U:
            static_cast<uint16_t>(player.currentHp-strike);
        contactDamage=static_cast<uint16_t>(before-player.currentHp);
      }
      appliedDamage=static_cast<uint16_t>(std::min<uint32_t>(65535U,
          static_cast<uint32_t>(appliedDamage)+(before-player.currentHp)));
      if(before!=player.currentHp)appendHpChange(result,BattleSide::Player,
                                                  player.uid,before,player.currentHp);
      if(hitCritical)appendEvent(result,BattleEventType::CriticalHit,
                                 BattleSide::Opponent,opponent->uid);
      if(before!=player.currentHp&&playerEffects.rageActive){
        const int8_t beforeStage=battle.playerVolatile.attackStage;
        if(changeStage(battle.playerVolatile.attackStage,1)){
          BattleEvent& rage=appendEvent(result,BattleEventType::StatChanged,
                                        BattleSide::Player,player.uid);
          rage.value=0;rage.stageBefore=beforeStage;
          rage.stageAfter=battle.playerVolatile.attackStage;
        }
      }
      // FireRed's multi-hit script executes MOVEEND_NEXT_TARGET between
      // strikes.  On-damage/contact Abilities therefore resolve per hit:
      // Color Change can affect later strikes and Rough Skin/Static/
      // Effect Spore can trigger more than once during the complete move.
      const uint16_t attackerHpBefore=opponent->currentHp;
      const StatusCondition attackerStatusBefore=opponent->status;
      if(contactDamage)applyContactAbility(battle,enemyMoveData,*opponent,player,
                                            before!=player.currentHp,result);
      appendHpChange(result,BattleSide::Opponent,opponent->uid,
                     attackerHpBefore,opponent->currentHp);
      appendStatusChange(result,BattleSide::Opponent,opponent->uid,
                         attackerStatusBefore,opponent->status);
      triggerHeldItemWithResult(result,player,battle.playerVolatile,
                                BattleSide::Player,&playerEffects);
      triggerHeldItemWithResult(result,*opponent,enemyVolatile,
                                BattleSide::Opponent,&enemyEffects);
      playerStatusBefore=player.status;
      opponentStatusBeforeMove=opponent->status;
      opponentHpBeforeMove=opponent->currentHp;
      if(endedByEndure)break;
      if(!beatUpMove&&enemyCommandMove!=static_cast<MoveId>(214)&&
         opponent->status==StatusCondition::Sleep)break;
    }
  }
  damage=static_cast<uint16_t>(damageTotal);
  if(resolvedMoveEvent){
    resolvedMoveEvent->before=appliedDamage;
    resolvedMoveEvent->after=gLastCalculatedMovePower;
    resolvedMoveEvent->stageAfter=gPresentHealRoll?1:0;
    if(repeatedMove)resolvedMoveEvent->value=actualHitCount;
  }
  if(connected&&effectiveness!=100U){
    BattleEvent& event=appendEvent(result,BattleEventType::Effectiveness,
                                    BattleSide::Opponent,opponent->uid);
    event.effectiveness100=effectiveness;
  }
  if((printsMultiHitCount(enemyMoveData)||parentalBondMove)&&actualHitCount){
    BattleEvent& count=appendEvent(result,BattleEventType::MultiHitCount,
                                    BattleSide::Opponent,opponent->uid);
    count.move=enemyMove;count.value=actualHitCount;
  }
  storeBideDamage(battle.playerBide,appliedDamage);
  const uint16_t enemyInflictedDamage=static_cast<uint16_t>(std::min<uint32_t>(65535U,
      static_cast<uint32_t>(substituteDamage)+appliedDamage));
  if (!connected) {
    if(effectIs(enemyMoveData,"FURY_CUTTER"))enemyEffects.furyCutterCount=0;
    if(effectIs(enemyMoveData,"ROLLOUT")){
      enemyEffects.rolloutCount=0;enemyEffects.lockedMoveTurns=0;
      enemyEffects.lockedMove=MoveId::None;
    }
    if(primaryStatusConflict)
      appendMoveEffect(result,BattleSide::Player,player.uid,enemyMove,
                       existingStatusFeedback(attemptedPrimaryStatus,player.status));
    else if (!requirementMet){
      if(dampBlocked){
        if(abilityIs(*opponent,"DAMP"))appendAbilityActivation(
            result,BattleSide::Opponent,*opponent);
        else appendAbilityActivation(result,BattleSide::Player,player);
      }else if(enemyOhkoBlock!=OhkoBlockReason::None)
        appendOhkoFailure(result,BattleSide::Player,player,enemyMove,enemyOhkoBlock);
      else appendMoveEffect(result, BattleSide::Player, player.uid, enemyMove,
                            BattleMoveEffect::Failed);
    }else if(soundproofBlocked&&protectAllowsMove)
      appendAbilityActivation(result,BattleSide::Player,player);
    else
      appendEvent(result, BattleEventType::MoveMissed, BattleSide::Opponent,
                  opponent->uid).move = enemyMove;
  }
  else {
    // Gen III loops Fury Cutter back through furycuttercalc after a type or
    // Ability immunity, resetting the chain even though accuracy connected.
    if(effectIs(enemyMoveData,"FURY_CUTTER")&&!effectiveness)
      enemyEffects.furyCutterCount=0;
  }
  if(connected)appendBlockingDamageAbility(battle,enemyMoveData,*opponent,
                                            player,effectiveness,result);
  applyCrashDamage(enemyMoveData,connected,projectedCrash,crashEffectiveness,
                   player.maximumHp,*opponent,result,BattleSide::Opponent);
  // Protect prevents Memento's stat drops, but the original script still
  // drops the user's HP to zero. This path sits outside the connected block
  // for that exact reason.
  const bool enemyMementoProtected=effectIs(enemyMoveData,"MEMENTO")&&
      requirementMet&&!protectAllowsMove;
  if(enemyMementoProtected)
    opponent->currentHp=0;
  // Unlike an accuracy miss, Protect still runs stockpiletobasedamage in the
  // original script and consumes the stored energy.
  if(effectIs(enemyMoveData,"SPIT_UP")&&requirementMet&&!protectAllowsMove)
    enemyVolatile.stockpileCount=0;
  if(requirementMet&&!protectAllowsMove)
    cancelMultiTurnMoves(enemyEffects,enemyVolatile,enemyBide,result,
                         BattleSide::Opponent,opponent->uid);
  const bool enemyExploded=effectIs(enemyMoveData,"EXPLOSION")&&
      !abilityIs(*opponent,"DAMP")&&
      !defenderAbilityApplies(*opponent,player,"DAMP");
  if(enemyExploded)opponent->currentHp=0;
  if (connected) {
    applyAbsorbAbility(battle,enemyMoveData,*opponent,player,damage,result);
  }
  result.enemyActed = true; result.damageTaken = enemyInflictedDamage;
  if(enemyMoveData && connected){
    const StageSnapshot enemyStagesBefore = snapshotStages(enemyVolatile);
    const StageSnapshot playerStagesBefore = snapshotStages(battle.playerVolatile);
    const VolatileFeedbackSnapshot enemyVolatileBefore = snapshotVolatileFeedback(enemyVolatile);
    const VolatileFeedbackSnapshot playerVolatileBefore = snapshotVolatileFeedback(battle.playerVolatile);
    const bool moveAffected=(enemyMoveData->power==0||effectiveness!=0)&&!moveAbsorbed;
    applyHeldMoveEffect(*enemyMoveData,moveAffected,playerHadSubstitute,
                        battle.kind,BattleSide::Opponent,*opponent,enemyVolatile,
                        player,battle.playerVolatile,result);
    if(moveAffected&&moveTargetsOpponent(enemyMoveData)){
      playerEffects.lastLandedMove=enemyMove;
      playerEffects.lastLandedType=effectiveMoveType(
          battle,*enemyMoveData,*opponent,&player);
    }
    const uint8_t enemySecondaryChance=abilityIs(*opponent,"SERENE GRACE")?
        static_cast<uint8_t>(std::min<uint16_t>(100U,enemyMoveData->effectChance*2U)):
        enemyMoveData->effectChance;
    const bool targetShieldDust=defenderAbilityApplies(*opponent,player,"SHIELD DUST")&&
        !secondaryEffectAffectsUser(enemyMoveData);
    const bool statusRoll=moveUsesStatusEffectRoll(battle,enemyMoveData);
    const bool sheerSuppressed=abilityIs(*opponent,"SHEER FORCE")&&
        sheerForceApplies(enemyMoveData);
    const bool secondaryProc=statusRoll||enemySecondaryChance==0||
        (!sheerSuppressed&&random(battle)%100U<enemySecondaryChance);
    if(moveAffected&&enemyMoveData->power&&targetShieldDust&&!statusRoll&&
       !sheerSuppressed&&secondaryProc)
      appendAbilityActivation(result,BattleSide::Player,player);
    const bool applySecondary = moveAffected&&(enemyMoveData->power == 0 ||
        (!sheerSuppressed&&!targetShieldDust&&secondaryProc));
    if (applySecondary&&(moveTargetsUser(enemyMoveData)||!playerHadSubstitute))
      applyStageEffect(battle,*opponent,enemyMoveData->effect,
                       enemyVolatile,battle.playerVolatile,player,&result);
    if(effectIs(enemyMoveData,"SECRET_POWER")&&applySecondary&&!playerHadSubstitute)
      applySecretPowerNonStatus(battle,result,*opponent,enemyVolatile,player,
                                battle.playerVolatile,BattleSide::Player);
    // Enemy moves use the same mechanics as player moves.  Keeping these
    // branches symmetric prevents status/setup moves from silently becoming
    // no-ops merely because a trainer or wild Pokemon used them.
    const char* effect=enemyMoveData->effect;
    if(moveAffected)
      applyDedicatedMoveEffect(battle,result,*enemyMoveData,*opponent,enemyVolatile,
                               enemyEffects,player,battle.playerVolatile,playerEffects,
                               BattleSide::Opponent);
    if(isWeatherMoveEffect(effect)){
      if(applyMoveWeather(battle,effect)){
        BattleEvent& weather=appendEvent(result,BattleEventType::MoveEffect,
                                          BattleSide::Opponent,opponent->uid);
        weather.move=enemyMove;
        weather.value=static_cast<uint16_t>(BattleMoveEffect::WeatherSet);
        weather.after=static_cast<uint16_t>(battle.weather);
        refreshForecastForms(battle,collection,&result);
      }else appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,
                             BattleMoveEffect::Failed);
    }
    if(std::strstr(effect,"TRANSFORM")) {
      if(playerEffects.transformed||semiInvulnerable(battle.playerVolatile)){
        appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,
                         BattleMoveEffect::Failed);
      }else{
        const uint16_t sourceSpecies = opponent->speciesId;
        const uint16_t targetSpecies = player.speciesId;
        transformInto(battle,*opponent,player,enemyVolatile,battle.playerVolatile,
                      enemyEffects,playerEffects,false);
        BattleEvent& transformed = appendEvent(result, BattleEventType::MoveEffect,
                                                 BattleSide::Opponent, opponent->uid);
        transformed.move = enemyMove;
        transformed.value = static_cast<uint16_t>(BattleMoveEffect::Transformed);
        transformed.before = sourceSpecies;
        transformed.after = targetSpecies;
        cureConditionForbiddenByAbility(*opponent,enemyVolatile,enemyEffects,
                                         BattleSide::Opponent,&result);
      }
    }
    if(effectIs(enemyMoveData,"ROLE_PLAY")) {
      opponent->abilityId = player.abilityId;
      BattleEvent& copied = appendEvent(result, BattleEventType::MoveEffect,
                                         BattleSide::Opponent, opponent->uid);
      copied.move = enemyMove;
      copied.value = static_cast<uint16_t>(BattleMoveEffect::AbilityCopied);
      copied.after = opponent->abilityId;
      cureConditionForbiddenByAbility(*opponent,enemyVolatile,enemyEffects,
                                       BattleSide::Opponent,&result);
      refreshForecastPair(battle,player,playerEffects,*opponent,enemyEffects,&result);
    }
    if(effectIs(enemyMoveData,"NIGHTMARE")){
      battle.playerNightmare=true;
      appendMoveEffect(result,BattleSide::Player,player.uid,enemyMove,
                       BattleMoveEffect::NightmareApplied);
    }
    if (effectIs(enemyMoveData, "SAFEGUARD")) {
      if (!battle.opponentSafeguardTurns) {
        battle.opponentSafeguardTurns = 5;
        appendMoveEffect(result, BattleSide::Opponent, opponent->uid, enemyMove,
                         BattleMoveEffect::SafeguardSet);
      } else {
        appendMoveEffect(result, BattleSide::Opponent, opponent->uid, enemyMove,
                         BattleMoveEffect::Failed);
      }
    }
    if(effectIs(enemyMoveData,"STOCKPILE")){
      ++enemyVolatile.stockpileCount;
      BattleEvent& stockpiled=appendEvent(result,BattleEventType::MoveEffect,
                                           BattleSide::Opponent,opponent->uid);
      stockpiled.move=enemyMove;
      stockpiled.value=static_cast<uint16_t>(BattleMoveEffect::Stockpiled);
      stockpiled.after=enemyVolatile.stockpileCount;
    }
    if(effectIs(enemyMoveData,"SPIT_UP")){
      const uint8_t released=enemyVolatile.stockpileCount;
      enemyVolatile.stockpileCount=0;
      BattleEvent& release=appendEvent(result,BattleEventType::MoveEffect,
                                        BattleSide::Opponent,opponent->uid);
      release.move=enemyMove;
      release.value=static_cast<uint16_t>(BattleMoveEffect::StockpileReleased);
      release.before=released;
    }
    if(effectIs(enemyMoveData,"SWALLOW")){
      const uint8_t swallowed=enemyVolatile.stockpileCount;
      enemyVolatile.stockpileCount=0;
      if(opponent->currentHp<opponent->maximumHp){
        const uint16_t healing=std::max<uint16_t>(1U,static_cast<uint16_t>(
            opponent->maximumHp/(1U<<(3U-swallowed))));
        opponent->currentHp=std::min<uint16_t>(opponent->maximumHp,
            static_cast<uint16_t>(opponent->currentHp+healing));
        BattleEvent& swallow=appendEvent(result,BattleEventType::MoveEffect,
                                          BattleSide::Opponent,opponent->uid);
        swallow.move=enemyMove;
        swallow.value=static_cast<uint16_t>(BattleMoveEffect::Swallowed);
        swallow.before=swallowed;
      }else appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,
                             BattleMoveEffect::Failed);
    }
    const uint16_t actualDamage = appliedDamage;
    if(actualDamage){
      playerEffects.lastDamageReceived=actualDamage;
      playerEffects.lastDamagingMove=enemyMove;
      playerEffects.lastDamageWasPhysical=isPhysicalType(
          effectiveMoveType(battle,*enemyMoveData,*opponent,&player));
    }
    // Substitute damage never feeds an HP-draining move in Gen III.  Use
    // only damage removed from the real target and emit the resulting HP
    // animation (including LIQUID OOZE recoil) in the event journal.
    applyHalfDrain(enemyMoveData,appliedDamage,*opponent,player,result,
                   BattleSide::Opponent);
    // applyHalfDrain already journals its exact HP transition at the point
    // where FireRed resolves the drain.  Advance the later catch-all
    // snapshot so it records only subsequent self-HP effects (Substitute,
    // recoil, healing moves, etc.) instead of appending the same recovery a
    // second time.  The duplicate event made the HUD reach the healed value,
    // rewind to its midpoint, then rise again.
    if (isHalfDrainEffect(enemyMoveData) && appliedDamage)
      opponentHpBeforeMove = opponent->currentHp;
    applyMoveRecoil(enemyMoveData,enemyInflictedDamage,*opponent,result,
                    BattleSide::Opponent);
    if(std::strstr(effect,"HEAL_HALF")||std::strstr(effect,"RESTORE_HP")||std::strstr(effect,"SOFTBOILED")||
       std::strstr(effect,"SYNTHESIS")||std::strstr(effect,"MORNING_SUN")||std::strstr(effect,"MOONLIGHT"))
      opponent->currentHp=std::min<uint16_t>(opponent->maximumHp,
          static_cast<uint16_t>(opponent->currentHp + recoveryMoveAmount(
              battle, *opponent, player, effect)));
    if(std::strstr(effect,"REST")){opponent->currentHp=opponent->maximumHp;opponent->status=StatusCondition::Sleep;enemyVolatile.sleepTurns=3;}
    if(std::strstr(effect,"THAW_HIT")&&moveAffected&&
       opponent->status==StatusCondition::Frozen)opponent->status=StatusCondition::None;
    if(std::strstr(effect,"REFRESH")&&(opponent->status==StatusCondition::Poison||opponent->status==StatusCondition::BadlyPoisoned||opponent->status==StatusCondition::Paralysis||opponent->status==StatusCondition::Burn))opponent->status=StatusCondition::None;
    if(std::strstr(effect,"HEAL_BELL")){
      for(uint8_t i=0;i<battle.opponentCount;++i)
        if(canBeCuredByBell(*enemyMoveData,battle.opponents[i]))
          battle.opponents[i].status=StatusCondition::None;
      appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,BattleMoveEffect::TeamCured);
    }
    if(std::strstr(effect,"HAZE")){
      clearStatStages(enemyVolatile);clearStatStages(battle.playerVolatile);
      appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,BattleMoveEffect::HazeCleared);
    }
    if(std::strstr(effect,"BULK_UP")){changeStage(enemyVolatile.attackStage,1);changeStage(enemyVolatile.defenseStage,1);}
    if(std::strstr(effect,"CALM_MIND")){changeStage(enemyVolatile.spAttackStage,1);changeStage(enemyVolatile.spDefenseStage,1);}
    if(std::strstr(effect,"DRAGON_DANCE")){changeStage(enemyVolatile.attackStage,1);changeStage(enemyVolatile.speedStage,1);}
    if(std::strstr(effect,"COSMIC_POWER")){changeStage(enemyVolatile.defenseStage,1);changeStage(enemyVolatile.spDefenseStage,1);}
    if(std::strstr(effect,"SUPERPOWER")&&moveAffected){changeStage(enemyVolatile.attackStage,-1);changeStage(enemyVolatile.defenseStage,-1);}
    if(std::strstr(effect,"OVERHEAT")&&moveAffected)changeStage(enemyVolatile.spAttackStage,-2);
    if(std::strstr(effect,"DEFENSE_CURL"))changeStage(enemyVolatile.defenseStage,1);
    if(std::strstr(effect,"MINIMIZE"))changeStage(enemyVolatile.evasionStage,1);
    if (std::strcmp(effect,"LIGHT_SCREEN")==0) {
      if (!battle.opponentLightScreenTurns) {
        battle.opponentLightScreenTurns=5;
        appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,
                         BattleMoveEffect::LightScreenSet);
      } else appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,
                              BattleMoveEffect::Failed);
    }
    if (std::strcmp(effect,"REFLECT")==0) {
      if (!battle.opponentReflectTurns) {
        battle.opponentReflectTurns=5;
        appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,
                         BattleMoveEffect::ReflectSet);
      } else appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,
                              BattleMoveEffect::Failed);
    }
    if (std::strcmp(effect,"MIST")==0) {
      if (!battle.opponentMistTurns) {
        battle.opponentMistTurns=5;
        appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,
                         BattleMoveEffect::MistSet);
      } else appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,
                              BattleMoveEffect::Failed);
    }
    if(std::strstr(effect,"BRICK_BREAK")){
      battle.playerReflectTurns=0;battle.playerLightScreenTurns=0;
    }
    if(std::strstr(effect,"TICKLE")&&!playerHadSubstitute){
      applyStageEffect(battle,*opponent,"ATTACK_DOWN",enemyVolatile,
                       battle.playerVolatile,player,&result);
      applyStageEffect(battle,*opponent,"DEFENSE_DOWN",enemyVolatile,
                       battle.playerVolatile,player,&result);
    }
    if(std::strstr(effect,"ALL_STATS_UP_HIT")&&applySecondary){changeStage(enemyVolatile.attackStage,1);changeStage(enemyVolatile.defenseStage,1);changeStage(enemyVolatile.spAttackStage,1);changeStage(enemyVolatile.spDefenseStage,1);changeStage(enemyVolatile.speedStage,1);}
    if(std::strstr(effect,"PSYCH_UP")){
      enemyVolatile.attackStage=battle.playerVolatile.attackStage;enemyVolatile.defenseStage=battle.playerVolatile.defenseStage;enemyVolatile.spAttackStage=battle.playerVolatile.spAttackStage;enemyVolatile.spDefenseStage=battle.playerVolatile.spDefenseStage;enemyVolatile.speedStage=battle.playerVolatile.speedStage;enemyVolatile.accuracyStage=battle.playerVolatile.accuracyStage;enemyVolatile.evasionStage=battle.playerVolatile.evasionStage;
      appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,
                       BattleMoveEffect::StatsCopied);
    }
    if(std::strstr(effect,"BELLY_DRUM")&&
       opponent->currentHp>std::max<uint16_t>(1U,opponent->maximumHp/2U)&&
       enemyVolatile.attackStage<kMaximumBattleStatStage){
      opponent->currentHp=static_cast<uint16_t>(opponent->currentHp-
          std::max<uint16_t>(1U,opponent->maximumHp/2U));
      enemyVolatile.attackStage=kMaximumBattleStatStage;
    }
    if(std::strstr(effect,"PAIN_SPLIT")){
      const uint16_t average=static_cast<uint16_t>((opponent->currentHp+player.currentHp)/2U);opponent->currentHp=std::min(opponent->maximumHp,average);player.currentHp=std::min(player.maximumHp,average);
      appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,
                       BattleMoveEffect::PainShared);
    }
    if(std::strstr(effect,"SMELLINGSALT")&&moveAffected&&!playerHadSubstitute&&
       player.status==StatusCondition::Paralysis)player.status=StatusCondition::None;
    const bool enemyConfusionRolled = !playerHadSubstitute&&((std::strstr(effect,"CONFUSE") && applySecondary) ||
        std::strstr(effect,"TEETER_DANCE") || std::strstr(effect,"SWAGGER") ||
        std::strstr(effect,"FLATTER"))&&!battle.playerVolatile.confusionTurns;
    const bool enemyConfusionBlocked=enemyConfusionRolled&&
        defenderAbilityApplies(*opponent,player,"OWN TEMPO");
    if(enemyConfusionBlocked)appendAbilityActivation(result,BattleSide::Player,player);
    if (enemyConfusionRolled&&!enemyConfusionBlocked) {
      if (battle.playerSafeguardTurns)
        appendMoveEffect(result, BattleSide::Player, player.uid, enemyMove,
                         BattleMoveEffect::SafeguardBlocked);
      else battle.playerVolatile.confusionTurns=static_cast<uint8_t>(2U+random(battle)%4U);
    }
    if(std::strstr(effect,"SWAGGER")&&!playerHadSubstitute)changeStage(battle.playerVolatile.attackStage,2);
    if(std::strstr(effect,"FLATTER")&&!playerHadSubstitute)changeStage(battle.playerVolatile.spAttackStage,1);
    const bool enemyFlinchRolled=(std::strstr(effect,"FLINCH")||std::strstr(effect,"FAKE_OUT")||std::strstr(effect,"SNORE")||
        effectIs(enemyMoveData,"TWISTER")||
        effectIs(enemyMoveData,"SKY_ATTACK"))&&applySecondary&&!playerHadSubstitute&&
        !sideAlreadyActed(result,BattleSide::Player);
    if(enemyFlinchRolled&&defenderAbilityApplies(*opponent,player,"INNER FOCUS"))
      appendAbilityActivation(result,BattleSide::Player,player);
    else if(enemyFlinchRolled)battle.playerVolatile.flinched=true;
    const bool enemyKingsRockProc=enemyMoveData->kingsRockAffected&&
       enemyInflictedDamage&&player.currentHp&&!playerHadSubstitute&&
       !sideAlreadyActed(result,BattleSide::Player)&&
       heldIs(*opponent,enemyVolatile,HeldItem::KingsRock)&&random(battle)%10U==0;
    if(enemyKingsRockProc){
      if(defenderAbilityApplies(*opponent,player,"SHIELD DUST")||
         defenderAbilityApplies(*opponent,player,"INNER FOCUS"))
        appendAbilityActivation(result,BattleSide::Player,player);
      else battle.playerVolatile.flinched=true;
    }
    if(std::strstr(effect,"LEECH_SEED")&&!playerHadSubstitute&&
       !hasBattleType(player,playerEffects,PokemonType::Grass)&&!battle.playerVolatile.seeded)
      battle.playerVolatile.seeded=true;
    if(std::strstr(effect,"TRAP")&&moveAffected&&!playerHadSubstitute&&
       !battle.playerVolatile.trappedTurns)
      battle.playerVolatile.trappedTurns=static_cast<uint8_t>(3U+random(battle)%4U);
    if(std::strstr(effect,"RAPID_SPIN")&&moveAffected){
      // Cmd_rapidspinfree returns to the same command after each message, so
      // one successful Rapid Spin removes every applicable effect during the
      // same move (Wrap first, then Leech Seed, then the user's Spikes).
      if(enemyVolatile.trappedTurns)enemyVolatile.trappedTurns=0;
      if(enemyVolatile.seeded)enemyVolatile.seeded=false;
      if(battle.opponentSpikesLayers)battle.opponentSpikesLayers=0;
    }
    if (effectIs(enemyMoveData,"PROTECT")) {
      if (protectionSucceeds(battle,battle.opponentProtectChain,
                             !sideAlreadyActed(result,BattleSide::Player)))
        enemyVolatile.protectedThisTurn=true;
      else appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,BattleMoveEffect::Failed);
    }
    if (effectIs(enemyMoveData,"ENDURE")) {
      if (protectionSucceeds(battle,battle.opponentProtectChain,
                             !sideAlreadyActed(result,BattleSide::Player))) {
        battle.opponentEndureThisTurn=true;
        appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,BattleMoveEffect::Endured);
      } else appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,BattleMoveEffect::Failed);
    }
    if(std::strstr(effect,"SUBSTITUTE")&&!enemyVolatile.substituteHp){
      const uint16_t cost=std::max<uint16_t>(1U,opponent->maximumHp/4U);
      if(opponent->currentHp>cost){opponent->currentHp-=cost;enemyVolatile.substituteHp=cost;enemyVolatile.trappedTurns=0;}
    }
    if(std::strstr(effect,"RECHARGE")&&moveAffected)enemyVolatile.recharging=true;
    if(enemyMoveData->power&&enemyMoveData->type==PokemonType::Electric)
      enemyEffects.chargeTurns=0;
    if(effectIs(enemyMoveData,"ENCORE")&&!playerHadSubstitute&&
       battle.playerVolatile.encoreMove==MoveId::None&&
       canEncoreMove(battle.playerVolatile.lastMoveUsed)&&
       moveSlotFor(player,battle.playerVolatile.lastMoveUsed)<kMoveSlots){
      battle.playerVolatile.encoreMove=battle.playerVolatile.lastMoveUsed;
      battle.playerEncoreTurns=static_cast<uint8_t>(3U+(random(battle)&3U));
    }
    if(effectIs(enemyMoveData,"ROAR")){
      const bool suctionBlocked=defenderAbilityApplies(*opponent,player,"SUCTION CUPS");
      const bool blocked=suctionBlocked||
          battle.playerMoveEffects.ingrained||
          !forceSwitchLevelCheck(battle,*opponent,player);
      if(suctionBlocked)appendAbilityActivation(result,BattleSide::Player,player);
      if(blocked&&!suctionBlocked)
        appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,
                         BattleMoveEffect::Failed);
      else if(battle.kind==BattleKind::Wild){
        battle.active=false;battle.outcome=BattleOutcome::Escaped;
      }else if(!forcePlayerSwitch(battle,collection,result))
        appendMoveEffect(result,BattleSide::Opponent,opponent->uid,enemyMove,
                         BattleMoveEffect::Failed);
    }
    appendHpChange(result, BattleSide::Opponent, opponent->uid,
                   opponentHpBeforeMove, opponent->currentHp);
    appendStageChanges(result, BattleSide::Opponent, opponent->uid, enemyStagesBefore, enemyVolatile);
    appendStageChanges(result, BattleSide::Player, player.uid, playerStagesBefore, battle.playerVolatile);
    appendVolatileFeedback(result, BattleSide::Opponent, opponent->uid, enemyMove,
                           enemyVolatileBefore, enemyVolatile);
    appendVolatileFeedback(result, BattleSide::Player, player.uid, enemyMove,
                           playerVolatileBefore, battle.playerVolatile);
  }
  if((enemyExploded||enemyMementoProtected)&&!connected)
    appendHpChange(result,BattleSide::Opponent,opponent->uid,
                   opponentHpBeforeMove,opponent->currentHp);
  if (connected&&!moveAbsorbed&&enemyMoveData&&(!enemyMoveData->power||effectiveness)&&
      moveTargetsOpponent(enemyMoveData))
    applyMoveStatus(battle,*opponent,enemyMove,player,result,playerHadSubstitute);
  appendStatusChange(result,BattleSide::Player,player.uid,playerStatusBefore,player.status);
  appendStatusChange(result,BattleSide::Opponent,opponent->uid,opponentStatusBeforeMove,opponent->status);
  if (connected && enemyMoveData && enemyMoveData->power == 0 &&
      result.eventCount == enemyFeedbackStart) {
    const bool affectsUser=moveTargetsUser(enemyMoveData);
    appendMoveEffect(result,
        affectsUser?BattleSide::Opponent:BattleSide::Player,
        affectsUser?opponent->uid:player.uid,enemyMove,BattleMoveEffect::Failed);
  }
  applyShellBellWithResult(result,*opponent,enemyVolatile,BattleSide::Opponent,
                           enemyInflictedDamage);
  triggerHeldItemWithResult(result,player,battle.playerVolatile,BattleSide::Player,&playerEffects);
  triggerHeldItemWithResult(result,*opponent,enemyVolatile,BattleSide::Opponent,&enemyEffects);
  if(player.currentHp==0)
    resolveFaintVows(result,BattleSide::Player,player,playerEffects,*opponent,
                     enemyMove,moveSlot,enemyInflictedDamage);
  if(opponent->currentHp==0){appendFaintedOnce(result,BattleSide::Opponent,*opponent);result.opponentDefeated=true;awardExperience(battle,collection,result);finishOpponentFaint(battle,collection,result);}
  if (player.currentHp == 0) {
    appendFaintedOnce(result, BattleSide::Player, player);
    player.recoverySecondsRemaining = 1;
    OwnedPokemon* replacement = firstHealthyPartyMember(collection, player.uid);
    // FireRed stops at the party screen after a faint. The UI calls
    // switchToPokemon only after the player explicitly chooses a replacement.
    if (!replacement) { battle.active = false; battle.outcome = BattleOutcome::Defeat; }
  }
}

bool BattleEngine::useFieldSoftBoiled(PokemonCollection& collection,
                                      uint32_t userUid, uint32_t targetUid) {
  if (!userUid || !targetUid || userUid == targetUid ||
      !CollectionLogic::isInParty(collection, userUid) ||
      !CollectionLogic::isInParty(collection, targetUid)) return false;
  OwnedPokemon* user = CollectionLogic::find(collection, userUid);
  OwnedPokemon* target = CollectionLogic::find(collection, targetUid);
  if (!user || !target || !target->currentHp ||
      target->currentHp >= target->maximumHp) return false;
  bool knowsFieldMove = false;
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    if (effectIs(findFullMove(user->moves[slot]), "SOFTBOILED")) {
      knowsFieldMove = true;
      break;
    }
  }
  if (!knowsFieldMove) return false;
  const uint16_t cost = std::max<uint16_t>(1U, user->maximumHp / 5U);
  // The donor must survive the transfer, matching FireRed's field command.
  if (user->currentHp <= cost) return false;
  user->currentHp = static_cast<uint16_t>(user->currentHp - cost);
  target->currentHp = std::min<uint16_t>(target->maximumHp,
      static_cast<uint16_t>(target->currentHp + cost));
  return true;
}

uint8_t BattleEngine::chooseEnemyMove(BattleState& battle, const OwnedPokemon& player) {
  const OwnedPokemon* opponent = currentOpponent(battle);
  const SpeciesData* attacker = opponent ? findSpecies(opponent->speciesId) : nullptr;
  const SpeciesData* defender = findSpecies(player.speciesId);
  if (!opponent || !attacker || !defender) return 0xFF;
  CombatVolatile& volatileState=battle.opponentVolatiles[battle.opponentIndex];
  BideState& bide=battle.opponentBides[battle.opponentIndex];
  if(bide.turns){
    const uint8_t bideSlot=moveSlotFor(*opponent,static_cast<MoveId>(117),false);
    if(bideSlot<kMoveSlots)return bideSlot;
    bide=BideState{};
  }
  if(battle.opponentMoveEffects[battle.opponentIndex].lockedMoveTurns){
    const uint8_t forced=moveSlotFor(*opponent,
        battle.opponentMoveEffects[battle.opponentIndex].lockedMove,false);
    if(forced<kMoveSlots)return forced;
    battle.opponentMoveEffects[battle.opponentIndex].lockedMoveTurns=0;
  }
  if(volatileState.chargingMove!=MoveId::None){
    const uint8_t forced=moveSlotFor(*opponent,volatileState.chargingMove,false);
    if(forced<kMoveSlots)return forced;
    volatileState.chargingMove=MoveId::None;
  }
  if(volatileState.encoreMove!=MoveId::None){
    const uint8_t forced=moveSlotFor(*opponent,volatileState.encoreMove);
    if(forced<kMoveSlots&&isMoveAvailableInSingleBattle(opponent->moves[forced]))
      return forced;
    clearEncore(volatileState,battle.opponentEncoreTurns[battle.opponentIndex]);
  }
  const DedicatedMoveEffectState& enemyEffects =
      battle.opponentMoveEffects[battle.opponentIndex];
  const auto passesCommonRestrictions = [&](uint8_t slot) {
    if (slot >= kMoveSlots || !opponent->movePp[slot] ||
        !isMoveAvailableInSingleBattle(opponent->moves[slot])) return false;
    const FullMoveData* move = findFullMove(opponent->moves[slot]);
    if (!move || volatileState.disabledMove == opponent->moves[slot] ||
        (battle.playerMoveEffects.imprisoned &&
         pokemonKnowsMove(player, opponent->moves[slot])) ||
        (enemyEffects.tauntTurns && move->power == 0) ||
        (enemyEffects.tormented && volatileState.lastMoveUsed == opponent->moves[slot]))
      return false;
    return true;
  };
  uint8_t stockpileSlot = 0xFFU;
  uint8_t spitUpSlot = 0xFFU;
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    if (!passesCommonRestrictions(slot)) continue;
    const FullMoveData* move = findFullMove(opponent->moves[slot]);
    if (effectIs(move, "STOCKPILE")) stockpileSlot = slot;
    else if (effectIs(move, "SPIT_UP")) spitUpSlot = slot;
  }
  bool spitUpCanDamage = false;
  if (spitUpSlot < kMoveSlots) {
    const FullMoveData* spitUp = findFullMove(opponent->moves[spitUpSlot]);
    const PokemonType type = effectiveMoveType(battle, *spitUp, *opponent, &player);
    const uint16_t effectiveness = typeMultiplier100(type, *defender);
    const bool ignoresAbility = abilityIs(*opponent, "MOLD BREAKER");
    spitUpCanDamage = effectiveness != 0U &&
        (ignoresAbility ||
         !((type == PokemonType::Ground && abilityIs(player, "LEVITATE")) ||
           (type == PokemonType::Fire && abilityIs(player, "FLASH FIRE") &&
            player.status != StatusCondition::Frozen) ||
           (type == PokemonType::Water && abilityIs(player, "WATER ABSORB")) ||
           (type == PokemonType::Electric && abilityIs(player, "VOLT ABSORB")) ||
           (abilityIs(player, "WONDER GUARD") && effectiveness <= 100U)));
  }
  const auto contextuallyUsable = [&](uint8_t slot) {
    if (!passesCommonRestrictions(slot)) return false;
    const FullMoveData* move = findFullMove(opponent->moves[slot]);
    if (effectIs(move, "STOCKPILE"))
      return volatileState.stockpileCount < 3U;
    if (effectIs(move, "SPIT_UP")) {
      if (!volatileState.stockpileCount) return false;
      // Once this combo starts, finish storing all three stages before
      // releasing them.  If STOCKPILE is disabled/out of PP, retain the
      // already stored energy and allow SPIT UP instead of deadlocking.
      if (volatileState.stockpileCount < 3U && stockpileSlot < kMoveSlots &&
          spitUpCanDamage) return false;
    }
    if (effectIs(move, "SWALLOW"))
      return volatileState.stockpileCount != 0U &&
             opponent->currentHp < opponent->maximumHp;
    return true;
  };
  int32_t bestScore = -1; uint8_t best[kMoveSlots]{}, bestCount = 0;
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    const FullMoveData* move = findFullMove(opponent->moves[slot]);
    if (!move || !contextuallyUsable(slot)) continue;
    const bool typelessStruggle=move->id==static_cast<uint16_t>(MoveId::Struggle);
    int32_t score = move->power ? move->power : 28;
    if (effectIs(move, "SPIT_UP"))
      score *= volatileState.stockpileCount;
    else if (effectIs(move, "STOCKPILE") && spitUpCanDamage)
      // A complete three-stage release is the intentional Stockpile set.
      // Keep setup ahead of ordinary attacks without making an ineffective
      // Normal-type SPIT UP attractive against Ghost/Wonder Guard targets.
      score = 800 + static_cast<int32_t>(volatileState.stockpileCount) * 20;
    score = score * (move->accuracy ? move->accuracy : 100) / 100;
    if (!typelessStruggle&&(move->type == attacker->type1 || move->type == attacker->type2))
      score = score * 3 / 2;
    const PokemonType scoredType=effectiveMoveType(battle,*move,*opponent,&player);
    uint16_t effectiveness = typelessStruggle?100U:typeMultiplier100(scoredType, *defender);
    const bool aiWeatherSuppressed=abilityIs(*opponent,"CLOUD NINE")||
        abilityIs(*opponent,"AIR LOCK")||abilityIs(player,"CLOUD NINE")||
        abilityIs(player,"AIR LOCK");
    if(!typelessStruggle&&!aiWeatherSuppressed&&battle.weather==BattleWeather::StrongWinds&&
       effectiveness&&hasBattleType(player,battle.playerMoveEffects,
                                     PokemonType::Flying)&&
       (scoredType==PokemonType::Rock||scoredType==PokemonType::Electric||
        scoredType==PokemonType::Ice))
      effectiveness=static_cast<uint16_t>(effectiveness/2U);
    score = score * effectiveness / 100;
    const bool ignoresPlayerAbility=abilityIs(*opponent,"MOLD BREAKER");
    if (!effectiveness || (!typelessStruggle&&!ignoresPlayerAbility&&
        ((scoredType == PokemonType::Ground && abilityIs(player,"LEVITATE")) ||
         (scoredType == PokemonType::Fire && abilityIs(player,"FLASH FIRE")&&
          player.status!=StatusCondition::Frozen) ||
         (scoredType == PokemonType::Water && move->power&&abilityIs(player,"WATER ABSORB")) ||
         (scoredType == PokemonType::Electric && move->power&&abilityIs(player,"VOLT ABSORB")) ||
         (abilityIs(player,"WONDER GUARD")&&effectiveness<=100U)))) score = 0;
    if (!move->power && player.status != StatusCondition::None &&
        (std::strstr(move->effect,"POISON") || std::strstr(move->effect,"SLEEP") ||
         std::strstr(move->effect,"PARALYZE") || std::strstr(move->effect,"BURN"))) score = 5;
    if ((std::strstr(move->effect,"RESTORE_HP") || std::strstr(move->effect,"SOFTBOILED") ||
         std::strstr(move->effect,"HEAL_HALF") || std::strstr(move->effect,"SYNTHESIS") ||
         std::strstr(move->effect,"MORNING_SUN") || std::strstr(move->effect,"MOONLIGHT") ||
         std::strcmp(move->effect,"REST") == 0) &&
        opponent->currentHp * 2U < opponent->maximumHp) score += 100;
    score += move->priority * 12;
    if (score > bestScore) { bestScore = score; best[0] = slot; bestCount = 1; }
    else if (score == bestScore) best[bestCount++] = slot;
  }
  if (!bestCount) return 0xFF;
  if (random(battle) % 10U < 2U) {
    uint8_t usable[kMoveSlots]{}, count = 0;
    for (uint8_t slot=0;slot<kMoveSlots;++slot)
      if(contextuallyUsable(slot))
        usable[count++]=slot;
    if (count) return usable[random(battle)%count];
  }
  return best[random(battle) % bestCount];
}

void BattleEngine::applyMoveStatus(BattleState& battle, OwnedPokemon& source, MoveId move,
                                   OwnedPokemon& target,BattleActionResult& result,
                                   bool targetHadSubstitute) {
  const FullMoveData* data = findFullMove(move);
  if (!data) return;
  if (target.status != StatusCondition::None) {
    const StatusCondition attempted=deterministicMoveStatus(battle,data);
    if(!data->power&&moveTargetsOpponent(data)&&attempted!=StatusCondition::None)
      appendMoveEffect(result,sideOf(battle,target),target.uid,move,
                       existingStatusFeedback(attempted,target.status));
    return;
  }
  CombatVolatile* targetVolatile = nullptr;
  if (target.uid == battle.playerUid) targetVolatile = &battle.playerVolatile;
  else for (uint8_t index = 0; index < battle.opponentCount; ++index)
    if (battle.opponents[index].uid == target.uid) {
      targetVolatile = &battle.opponentVolatiles[index];break;
    }
  if(data->power&&abilityIs(source,"SHEER FORCE")&&sheerForceApplies(data))return;
  uint8_t chance = data->effectChance ? data->effectChance : (data->power == 0 ? 100 : 0);
  if (abilityIs(source, "SERENE GRACE"))
    chance = static_cast<uint8_t>(std::min<uint16_t>(100, chance * 2U));
  const bool triAttack = std::strcmp(data->effect,"TRI_ATTACK") == 0;
  StatusCondition status = deterministicMoveStatus(battle, data);
  if (triAttack) {
    // FireRed resolves TRI ATTACK's 20% secondary-effect check first and
    // chooses burn/freeze/paralysis only after that check succeeds. The old
    // implementation consumed the status RNG on every hit, before checking
    // the chance, which desynchronised seeded battles and replays.
    if (random(battle) % 100U >= chance) return;
    switch (random(battle)%3U) {case 0:status=StatusCondition::Burn;break;case 1:status=StatusCondition::Frozen;break;default:status=StatusCondition::Paralysis;break;}
  } else if (status == StatusCondition::None || random(battle) % 100U >= chance) return;
  const BattleSide targetSide=sideOf(battle,target);
  // FireRed performs the effect-chance roll before Substitute/Shield Dust
  // rejects the resulting added effect. This preserves both activation
  // feedback and the deterministic RNG stream.
  if(targetHadSubstitute||(targetVolatile&&targetVolatile->substituteHp))return;
  if(data->power&&defenderAbilityApplies(source,target,"SHIELD DUST")){
    appendAbilityActivation(result,targetSide,target);
    return;
  }
  DedicatedMoveEffectState& targetEffects=moveEffectsFor(battle,targetSide);
  auto statusBlocked=[&](const OwnedPokemon& pokemon,
                         const DedicatedMoveEffectState& effects,
                         StatusCondition condition,bool* blockedByAbility=nullptr){
    // Mold Breaker ignores only the opposing target's passive. It must not
    // accidentally disable the user's own IMMUNITY/LIMBER/etc. when
    // Synchronize reflects a condition back to the source.
    const auto passiveBlocks=[&](const char* name){
      return &pokemon==&source?abilityIs(pokemon,name):
          defenderAbilityApplies(source,pokemon,name);
    };
    const bool poisonAbility=(condition==StatusCondition::Poison||
        condition==StatusCondition::BadlyPoisoned)&&passiveBlocks("IMMUNITY");
    const bool burnAbility=condition==StatusCondition::Burn&&passiveBlocks("WATER VEIL");
    const bool freezeAbility=condition==StatusCondition::Frozen&&passiveBlocks("MAGMA ARMOR");
    const bool paralysisAbility=condition==StatusCondition::Paralysis&&passiveBlocks("LIMBER");
    const bool sleepAbility=condition==StatusCondition::Sleep&&
        (passiveBlocks("INSOMNIA")||passiveBlocks("VITAL SPIRIT"));
    if(blockedByAbility)*blockedByAbility=poisonAbility||burnAbility||freezeAbility||
        paralysisAbility||sleepAbility;
    if((condition==StatusCondition::Poison||condition==StatusCondition::BadlyPoisoned)&&
       (hasBattleType(pokemon,effects,PokemonType::Poison)||
        hasBattleType(pokemon,effects,PokemonType::Steel)||poisonAbility))return true;
    if(condition==StatusCondition::Burn&&
       (hasBattleType(pokemon,effects,PokemonType::Fire)||burnAbility))return true;
    if(condition==StatusCondition::Frozen&&
       (hasBattleType(pokemon,effects,PokemonType::Ice)||freezeAbility))return true;
    const bool weatherSuppressed=abilityIs(source,"CLOUD NINE")||
        abilityIs(source,"AIR LOCK")||abilityIs(pokemon,"CLOUD NINE")||
        abilityIs(pokemon,"AIR LOCK");
    if(condition==StatusCondition::Frozen&&!weatherSuppressed&&
       (battle.weather==BattleWeather::Sun||battle.weather==BattleWeather::HarshSun))
      return true;
    if(paralysisAbility)return true;
    if(condition==StatusCondition::Sleep&&(
       sleepAbility||
       ((!abilityIs(pokemon,"SOUNDPROOF")&&!abilityIs(pokemon,"CACOPHONY"))&&
        (battle.playerMoveEffects.uproar||
         battle.opponentMoveEffects[battle.opponentIndex].uproar))))return true;
    return false;
  };
  bool targetAbilityBlocked=false;
  if(statusBlocked(target,targetEffects,status,&targetAbilityBlocked)){
    if(targetAbilityBlocked)appendAbilityActivation(result,targetSide,target);
    return;
  }
  if (protectedFromOpponentStatus(battle, source, target)) {
    appendMoveEffect(result, sideOf(battle, target), target.uid, move,
                     BattleMoveEffect::SafeguardBlocked);
    return;
  }
  target.status = status; result.statusApplied = true; result.appliedStatus = status;
  if (targetVolatile) {
    if(status==StatusCondition::Sleep||status==StatusCondition::Frozen){
      BideState& targetBide=targetSide==BattleSide::Player?battle.playerBide:
          battle.opponentBides[battle.opponentIndex];
      cancelMultiTurnMoves(targetEffects,*targetVolatile,targetBide,result,
                           targetSide,target.uid);
    }
    targetVolatile->toxicCounter = 0;
    if (status == StatusCondition::Sleep) targetVolatile->sleepTurns = static_cast<uint8_t>(2U + random(battle) % 4U);
  }
  const DedicatedMoveEffectState& sourceEffects=moveEffectsFor(battle,sideOf(battle,source));
  if(defenderAbilityApplies(source,target,"SYNCHRONIZE")&&
     source.status==StatusCondition::None&&
     (status==StatusCondition::Poison||status==StatusCondition::BadlyPoisoned||status==StatusCondition::Paralysis||status==StatusCondition::Burn)) {
    // In FireRed, SetMoveEffect explicitly exempts status caused by an
    // Ability (HITMARKER_STATUS_ABILITY_EFFECT) from Safeguard. Synchronize
    // therefore reflects through the veil, just like Static/Effect Spore.
    const StatusCondition reflected=status==StatusCondition::BadlyPoisoned?
        StatusCondition::Poison:status;
    bool sourceAbilityBlocked=false;
    if(!statusBlocked(source,sourceEffects,reflected,&sourceAbilityBlocked)){
      appendAbilityActivation(result,targetSide,target);
      source.status=reflected;
    }else if(sourceAbilityBlocked)
      appendAbilityActivation(result,sideOf(battle,source),source);
  }
}

void BattleEngine::awardExperience(BattleState& battle, PokemonCollection& collection, BattleActionResult& result) {
  // Link battles operate on the players' real teams for presentation, but
  // never grant XP, EVs, evolutions or money.
  if (battle.kind == BattleKind::Pvp) return;
  const OwnedPokemon* opponent = currentOpponent(battle);
  const SpeciesData* wildSpecies = opponent ? findSpecies(opponent->speciesId) : nullptr;
  if (!wildSpecies) return;
  uint8_t recipients = 0;
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) if (CollectionLogic::active(collection, slot)) ++recipients;
  if (!recipients) return;
  uint32_t totalExperience = (static_cast<uint32_t>(wildSpecies->baseExperience) *
                              opponent->level) / 7U;
  if (battle.kind == BattleKind::Wild)
    totalExperience *= kWildExperienceMultiplier;
  if (highestPartyLevel(collection) < kEarlyExperienceLevelLimit)
    totalExperience *= kEarlyExperienceMultiplier;
  // Lucky Egg is a permanent account reward in Pokegochi. Once unlocked it
  // doubles the complete award before the equal party split and never occupies
  // a Pokemon's held-item slot.
  if(gPermanentExperienceBoost)totalExperience*=2U;
  const uint32_t sharedExperience=totalExperience/recipients;
  result.experienceGained = static_cast<uint16_t>(std::min<uint32_t>(65535U, sharedExperience));
  if (result.experienceGained) {
    BattleEvent& xp = appendEvent(result, BattleEventType::ExperienceGained,
                                   BattleSide::Player, battle.playerUid);
    xp.value = result.experienceGained;
  }
  const EffortValues evYield = effortYieldFor(*wildSpecies);
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) {
  OwnedPokemon* recipient = CollectionLogic::active(collection, slot); if (!recipient) continue;
  OwnedPokemon& player = *recipient;
  const SpeciesData* playerSpecies = findSpecies(player.speciesId); if (!playerSpecies) continue;
  auto readStats = [](const OwnedPokemon& pokemon, uint16_t output[6]) {
    output[0] = pokemon.maximumHp;
    output[1] = CollectionLogic::calculatedStat(pokemon, PokemonStat::Attack);
    output[2] = CollectionLogic::calculatedStat(pokemon, PokemonStat::Defense);
    output[3] = CollectionLogic::calculatedStat(pokemon, PokemonStat::SpAttack);
    output[4] = CollectionLogic::calculatedStat(pokemon, PokemonStat::SpDefense);
    output[5] = CollectionLogic::calculatedStat(pokemon, PokemonStat::Speed);
  };
  uint16_t statsBefore[6]{};
  readStats(player, statsBefore);
  // Every active partner receives the same share. The permanent Lucky Egg
  // bonus, when unlocked, has already been applied to that shared award.
  player.experience += result.experienceGained;
  // XP is shared among all active partners by Pokégochi's deliberate rule.
  // Gen III awards the defeated species' full EV yield to every qualifying
  // recipient, not a divided fraction, so do the same for this active team.
  CollectionLogic::grantEffortValues(player, evYield);
  while (player.level < 100 && player.experience >= experienceForLevel(playerSpecies->growthRate, player.level + 1)) {
    ++player.level;
    const MoveId learnedMove = moveLearnedAtLevel(player.speciesId, player.level);
    CollectionLogic::refreshDerivedStats(player);
    uint16_t statsAfter[6]{};
    readStats(player, statsAfter);
    if (result.levelUpCount < kLevelUpSummaryCapacity) {
      const uint8_t summaryIndex = result.levelUpCount++;
      LevelUpSummary& summary = result.levelUps[summaryIndex];
      summary.pokemonUid = player.uid;
      summary.speciesId = player.speciesId;
      summary.newLevel = player.level;
      for (uint8_t stat = 0; stat < 6U; ++stat) {
        summary.stats[stat] = statsAfter[stat];
        const uint16_t gain = statsAfter[stat] > statsBefore[stat]
            ? static_cast<uint16_t>(statsAfter[stat] - statsBefore[stat]) : 0U;
        summary.gains[stat] = static_cast<uint8_t>(std::min<uint16_t>(255U, gain));
        statsBefore[stat] = statsAfter[stat];
      }
      BattleEvent& levelEvent = appendEvent(result, BattleEventType::LevelUp,
                                             BattleSide::Player, player.uid);
      levelEvent.value = summaryIndex;
      levelEvent.before = static_cast<uint16_t>(player.level - 1U);
      levelEvent.after = player.level;
    } else {
      std::memcpy(statsBefore, statsAfter, sizeof(statsBefore));
    }
    uint16_t choices[kEvolutionChoiceCapacity]{};
    const uint8_t choiceCount=evolutionChoices(player.speciesId,player.level,
        CollectionLogic::calculatedStat(player,PokemonStat::Attack),CollectionLogic::calculatedStat(player,PokemonStat::Defense),
        battle.unlockedGeneration,choices,kEvolutionChoiceCapacity);
    // Every evolution, including a single linear destination, is deferred to
    // the presentation queue. The UI can therefore honor a FireRed-style
    // cancellation before species, Ability, stats or Nincada's bonus
    // Shedinja are committed to the persistent collection.
    if (choiceCount && result.pendingEvolutionCount < kPartyCapacity) {
      bool alreadyPending=false;
      for(uint8_t queued=0;queued<result.pendingEvolutionCount;++queued)
        if(result.pendingEvolutionUids[queued]==player.uid)alreadyPending=true;
      if(!alreadyPending){
        const uint8_t pending=result.pendingEvolutionCount++;
        result.pendingEvolutionUids[pending]=player.uid;result.pendingEvolutionChoiceCounts[pending]=choiceCount;
        for(uint8_t i=0;i<choiceCount;++i)result.pendingEvolutionChoices[pending][i]=choices[i];
      }
    }
    if (learnedMove != MoveId::None) {
      bool known=false;for(uint8_t moveSlot=0;moveSlot<kMoveSlots;++moveSlot)if(player.moves[moveSlot]==learnedMove)known=true;
      if(!known){
        uint8_t empty=kMoveSlots;for(uint8_t moveSlot=0;moveSlot<kMoveSlots;++moveSlot)if(player.moves[moveSlot]==MoveId::None){empty=moveSlot;break;}
        if(empty<kMoveSlots){player.moves[empty]=learnedMove;CollectionLogic::clearMovePpUps(player,empty);const FullMoveData* move=findFullMove(learnedMove);player.movePp[empty]=move?move->pp:0;}
        else if(result.movesToLearnCount<kPartyCapacity){const uint8_t pending=result.movesToLearnCount++;result.moveLearnerUids[pending]=player.uid;result.movesToLearn[pending]=learnedMove;}
      }
    }
  }
}
}

void BattleEngine::advanceEndTurnEffects(BattleState& battle,
                                         PokemonCollection& collection,
                                         BattleActionResult& result){
  if(!battle.active)return;
  OwnedPokemon* player=CollectionLogic::find(collection,battle.playerUid);
  OwnedPokemon* opponent=currentOpponent(battle);
  if(!player||!opponent)return;

  auto healWithEvent=[&](OwnedPokemon& pokemon,BattleSide side,uint16_t amount,
                          MoveId move,BattleMoveEffect effect){
    if(!pokemon.currentHp)return;
    // Wish's full-HP branch still announces that the wish came true before
    // reporting full HP. Other end-turn heals are silent when already full.
    if(effect==BattleMoveEffect::WishGranted)
      appendMoveEffect(result,side,pokemon.uid,move,effect);
    if(pokemon.currentHp>=pokemon.maximumHp)return;
    const uint16_t before=pokemon.currentHp;
    pokemon.currentHp=std::min<uint16_t>(pokemon.maximumHp,
        static_cast<uint16_t>(pokemon.currentHp+amount));
    if(effect!=BattleMoveEffect::WishGranted)
      appendMoveEffect(result,side,pokemon.uid,move,effect);
    appendHpChange(result,side,pokemon.uid,before,pokemon.currentHp);
  };
  if(battle.playerWishDueTurn&&battle.turn>=battle.playerWishDueTurn){
    battle.playerWishDueTurn=0;
    healWithEvent(*player,BattleSide::Player,std::max<uint16_t>(1U,player->maximumHp/2U),
                  static_cast<MoveId>(273),BattleMoveEffect::WishGranted);
  }
  if(battle.opponentWishDueTurn&&battle.turn>=battle.opponentWishDueTurn){
    battle.opponentWishDueTurn=0;
    healWithEvent(*opponent,BattleSide::Opponent,std::max<uint16_t>(1U,opponent->maximumHp/2U),
                  static_cast<MoveId>(273),BattleMoveEffect::WishGranted);
  }
  DedicatedMoveEffectState& opponentEffects=battle.opponentMoveEffects[battle.opponentIndex];

  // FireRed resolves these effects separately and in order.  Combining them
  // into one residual total caused four mechanical errors: Toxic rounded
  // after multiplication, a final Wrap turn still dealt damage, Leech Seed
  // healed more HP than it removed, and Liquid Ooze was ignored.  Keeping
  // each HP change distinct also gives the UI the original event ordering.
  auto damageWithEvent=[&](OwnedPokemon& pokemon,BattleSide side,uint16_t amount){
    if(!pokemon.currentHp||!amount)return uint16_t{0};
    const uint16_t before=pokemon.currentHp;
    pokemon.currentHp=amount>=pokemon.currentHp?0U:
        static_cast<uint16_t>(pokemon.currentHp-amount);
    appendHpChange(result,side,pokemon.uid,before,pokemon.currentHp);
    return static_cast<uint16_t>(before-pokemon.currentHp);
  };
  auto plainHeal=[&](OwnedPokemon& pokemon,BattleSide side,uint16_t amount){
    if(!pokemon.currentHp||pokemon.currentHp>=pokemon.maximumHp||!amount)return;
    const uint16_t before=pokemon.currentHp;
    pokemon.currentHp=std::min<uint16_t>(pokemon.maximumHp,
        static_cast<uint16_t>(pokemon.currentHp+amount));
    appendHpChange(result,side,pokemon.uid,before,pokemon.currentHp);
  };
  const bool weatherSuppressed=abilityIs(*player,"CLOUD NINE")||
      abilityIs(*player,"AIR LOCK")||abilityIs(*opponent,"CLOUD NINE")||
      abilityIs(*opponent,"AIR LOCK");
  if(!weatherSuppressed&&(battle.weather==BattleWeather::Sandstorm||
     battle.weather==BattleWeather::Hail))
    appendWeatherContinues(result,battle.playerUid,battle.weather);
  auto battlerEndEffects=[&](OwnedPokemon& affected,CombatVolatile& volatileState,
      DedicatedMoveEffectState& effects,OwnedPokemon& other,BattleSide side){
    if(!affected.currentHp)return;
    const BattleSide otherSide=side==BattleSide::Player?BattleSide::Opponent:
        BattleSide::Player;
    const SpeciesData* species=findSpecies(affected.speciesId);
    const PokemonType type1=species?MegaEvolution::type1(affected,species->type1):
        PokemonType::Normal;
    const PokemonType type2=species?MegaEvolution::type2(affected,species->type2):
        PokemonType::Normal;

    // Damaging field weather is resolved before per-battler Ingrain/Ability
    // effects in Gen III.
    if(!weatherSuppressed&&battle.weather==BattleWeather::Sandstorm&&
       type1!=PokemonType::Rock&&type2!=PokemonType::Rock&&
       type1!=PokemonType::Ground&&type2!=PokemonType::Ground&&
       type1!=PokemonType::Steel&&type2!=PokemonType::Steel&&
       !abilityIs(affected,"SAND VEIL")){
      appendWeatherHurt(result,side,affected.uid,BattleWeather::Sandstorm);
      damageWithEvent(affected,side,std::max<uint16_t>(1U,affected.maximumHp/16U));
    }
    if(affected.currentHp&&!weatherSuppressed&&battle.weather==BattleWeather::Hail&&
       type1!=PokemonType::Ice&&type2!=PokemonType::Ice){
      appendWeatherHurt(result,side,affected.uid,BattleWeather::Hail);
      damageWithEvent(affected,side,std::max<uint16_t>(1U,affected.maximumHp/16U));
    }
    if(!affected.currentHp)return;

    if(effects.ingrained)
      healWithEvent(affected,side,std::max<uint16_t>(1U,affected.maximumHp/16U),
                    static_cast<MoveId>(275),BattleMoveEffect::Ingrained);
    if(abilityIs(affected,"SHED SKIN")&&affected.status!=StatusCondition::None&&
       random(battle)%3U==0){
      const StatusCondition before=affected.status;
      affected.status=StatusCondition::None;
      volatileState.sleepTurns=0;
      volatileState.toxicCounter=0;
      if(side==BattleSide::Player)battle.playerNightmare=false;
      else battle.opponentNightmares[battle.opponentIndex]=false;
      appendStatusChange(result,side,affected.uid,before,affected.status);
      if(result.eventCount)result.events[result.eventCount-1U].before=affected.abilityId;
    }
    const bool completedTurnAfterEntry=effects.enteredTurn?
        battle.turn>effects.enteredTurn:battle.turn>1U;
    if(abilityIs(affected,"SPEED BOOST")&&completedTurnAfterEntry){
      const StageSnapshot before=snapshotStages(volatileState);
      if(changeStage(volatileState.speedStage,1)){
        appendAbilityActivation(result,side,affected);
        appendStageChanges(result,side,affected.uid,before,volatileState);
      }
    }
    if(!weatherSuppressed&&(battle.weather==BattleWeather::Rain||
       battle.weather==BattleWeather::HeavyRain)&&
       abilityIs(affected,"RAIN DISH")&&affected.currentHp<affected.maximumHp){
      appendAbilityActivation(result,side,affected);
      plainHeal(affected,side,std::max<uint16_t>(1U,affected.maximumHp/16U));
    }
    if(!weatherSuppressed&&(battle.weather==BattleWeather::Sun||
       battle.weather==BattleWeather::HarshSun)&&abilityIs(affected,"SOLAR POWER")){
      appendAbilityActivation(result,side,affected);
      damageWithEvent(affected,side,std::max<uint16_t>(1U,affected.maximumHp/8U));
    }
    if(!affected.currentHp)return;

    if(volatileState.seeded&&other.currentHp){
      const uint16_t wanted=std::max<uint16_t>(1U,affected.maximumHp/8U);
      const uint16_t drained=damageWithEvent(affected,side,wanted);
      if(drained&&other.currentHp){
        if(abilityIs(affected,"LIQUID OOZE"))damageWithEvent(other,otherSide,drained);
        else plainHeal(other,otherSide,drained);
      }
    }
    if(!affected.currentHp)return;
    if(affected.status==StatusCondition::Poison)
      damageWithEvent(affected,side,std::max<uint16_t>(1U,affected.maximumHp/8U));
    else if(affected.status==StatusCondition::BadlyPoisoned){
      volatileState.toxicCounter=std::min<uint8_t>(15U,
          static_cast<uint8_t>(volatileState.toxicCounter+1U));
      const uint16_t unit=std::max<uint16_t>(1U,affected.maximumHp/16U);
      damageWithEvent(affected,side,static_cast<uint16_t>(unit*volatileState.toxicCounter));
    }else if(affected.status==StatusCondition::Burn)
      damageWithEvent(affected,side,std::max<uint16_t>(1U,affected.maximumHp/8U));
    if(!affected.currentHp)return;

    bool& nightmare=side==BattleSide::Player?battle.playerNightmare:
        battle.opponentNightmares[battle.opponentIndex];
    if(nightmare){
      if(affected.status!=StatusCondition::Sleep)nightmare=false;
      else{
        appendMoveEffect(result,side,affected.uid,static_cast<MoveId>(171),
                         BattleMoveEffect::NightmareHurt);
        damageWithEvent(affected,side,std::max<uint16_t>(1U,affected.maximumHp/4U));
      }
    }
    if(!affected.currentHp)return;
    if(isCursed(volatileState)){
      appendMoveEffect(result,side,affected.uid,static_cast<MoveId>(174),
                       BattleMoveEffect::CurseHurt);
      damageWithEvent(affected,side,std::max<uint16_t>(1U,affected.maximumHp/4U));
    }
    if(!affected.currentHp)return;
    if(volatileState.trappedTurns){
      --volatileState.trappedTurns;
      if(volatileState.trappedTurns)
        damageWithEvent(affected,side,std::max<uint16_t>(1U,affected.maximumHp/16U));
    }
  };
  battlerEndEffects(*player,battle.playerVolatile,battle.playerMoveEffects,
                    *opponent,BattleSide::Player);
  battlerEndEffects(*opponent,battle.opponentVolatiles[battle.opponentIndex],
                    opponentEffects,*player,BattleSide::Opponent);
  if(battle.weatherTurns!=0xFF&&battle.weatherTurns&&!--battle.weatherTurns){
    const BattleWeather endedWeather=battle.weather;
    battle.weather=BattleWeather::Clear;
    appendWeatherEnded(result,battle.playerUid,endedWeather);
    refreshForecastForms(battle,collection,&result);
  }

  if(!opponent->currentHp||!player->currentHp){
    if(!opponent->currentHp){
      appendFaintedOnce(result,BattleSide::Opponent,*opponent);
      result.opponentDefeated=true;awardExperience(battle,collection,result);
      finishOpponentFaint(battle,collection,result);
    }
    if(!player->currentHp){
      player->recoverySecondsRemaining=1;
      appendFaintedOnce(result,BattleSide::Player,*player);
      if(!firstHealthyPartyMember(collection,player->uid)){
        battle.active=false;battle.outcome=BattleOutcome::Defeat;
      }
    }
    return;
  }

  if(battle.playerMoveEffects.chargeTurns)--battle.playerMoveEffects.chargeTurns;
  if(opponentEffects.chargeTurns)--opponentEffects.chargeTurns;

  auto tickTimedVolatiles=[&](OwnedPokemon& pokemon,DedicatedMoveEffectState& effects,
                              CombatVolatile& volatileState,BideState& bide,
                              BattleSide side){
    if(effects.tauntTurns)--effects.tauntTurns;
    if(effects.yawnTurns&&!--effects.yawnTurns&&pokemon.currentHp&&
       pokemon.status==StatusCondition::None&&!abilityIs(pokemon,"INSOMNIA")&&
       !abilityIs(pokemon,"VITAL SPIRIT")&&
       ((abilityIs(pokemon,"SOUNDPROOF")||abilityIs(pokemon,"CACOPHONY"))||
        (!battle.playerMoveEffects.uproar&&
         !battle.opponentMoveEffects[battle.opponentIndex].uproar))){
      const StatusCondition before=pokemon.status;
      // Yawn's ENDTURN_YAWN branch calls CancelMultiTurnMoves immediately
      // before applying sleep.  This clears Bide, charge moves, Rollout,
      // rampage, Uproar and the Fury Cutter chain exactly like direct sleep
      // interruption in FireRed.
      cancelMultiTurnMoves(effects,volatileState,bide,result,side,pokemon.uid);
      pokemon.status=StatusCondition::Sleep;
      volatileState.sleepTurns=static_cast<uint8_t>(2U+dedicatedEffectRandom(battle)%4U);
      appendStatusChange(result,side,pokemon.uid,before,pokemon.status);
    }
  };
  tickTimedVolatiles(*player,battle.playerMoveEffects,battle.playerVolatile,
                     battle.playerBide,
                     BattleSide::Player);
  tickTimedVolatiles(*opponent,opponentEffects,
                     battle.opponentVolatiles[battle.opponentIndex],
                     battle.opponentBides[battle.opponentIndex],
                     BattleSide::Opponent);
  // STATUS3_ALWAYS_HITS starts at two ticks in FireRed: the setup turn keeps
  // it armed, and the following end turn expires it if no move consumed it.
  advanceSureHit(battle.playerVolatile);
  advanceSureHit(battle.opponentVolatiles[battle.opponentIndex]);

  auto perishTick=[&](OwnedPokemon& pokemon,DedicatedMoveEffectState& effects,
                      BattleSide side){
    if(!effects.perishTurns||!pokemon.currentHp)return;
    --effects.perishTurns;
    BattleEvent& count=appendEvent(result,BattleEventType::MoveEffect,side,pokemon.uid);
    count.move=static_cast<MoveId>(195);
    count.value=static_cast<uint16_t>(BattleMoveEffect::PerishCount);
    count.after=effects.perishTurns;
    if(effects.perishTurns)return;
    const uint16_t before=pokemon.currentHp;pokemon.currentHp=0;
    appendHpChange(result,side,pokemon.uid,before,0);
    appendFaintedOnce(result,side,pokemon);
  };
  perishTick(*player,battle.playerMoveEffects,BattleSide::Player);
  perishTick(*opponent,opponentEffects,BattleSide::Opponent);
  if(!opponent->currentHp){
    result.opponentDefeated=true;awardExperience(battle,collection,result);
    finishOpponentFaint(battle,collection,result);
  }
  if(!player->currentHp){
    player->recoverySecondsRemaining=1;
    if(!firstHealthyPartyMember(collection,player->uid)){
      battle.active=false;battle.outcome=BattleOutcome::Defeat;
    }
  }
  // MAGIC COAT and SNATCH protect only for the turn in which they were used.
  battle.playerMoveEffects.magicCoat=false;
  battle.playerMoveEffects.snatch=false;
  if(battle.opponentIndex<kOpponentTeamCapacity){
    battle.opponentMoveEffects[battle.opponentIndex].magicCoat=false;
    battle.opponentMoveEffects[battle.opponentIndex].snatch=false;
  }
  if(battle.active)advanceSafeguardTurns(battle,collection,result);
}

void BattleEngine::fightInto(BattleState& battle, PokemonCollection& collection, uint8_t moveSlot,
                             BattleActionResult& result) {
  static const BattleActionResult emptyResult{};
  result = emptyResult;
  OwnedPokemon* player = CollectionLogic::find(collection, battle.playerUid);
  OwnedPokemon* opponent = currentOpponent(battle);
  CombatVolatile& selfVolatile=battle.playerVolatile;
  BideState& playerBide=battle.playerBide;
  bool forcedBide=false;
  bool forcedEncore=false;
  bool forcedCharge=false;
  bool forcedLocked=false;
  if(player&&playerBide.turns){
    const uint8_t bideSlot=moveSlotFor(*player,static_cast<MoveId>(117),false);
    if(bideSlot<kMoveSlots){moveSlot=bideSlot;forcedBide=true;}
    else playerBide=BideState{};
  }else if(player&&battle.playerMoveEffects.lockedMoveTurns){
    const uint8_t lockedSlot=moveSlotFor(*player,battle.playerMoveEffects.lockedMove,false);
    if(lockedSlot<kMoveSlots){moveSlot=lockedSlot;forcedLocked=true;}
    else battle.playerMoveEffects.lockedMoveTurns=0;
  }else if(player&&selfVolatile.chargingMove!=MoveId::None){
    const uint8_t chargeSlot=moveSlotFor(*player,selfVolatile.chargingMove,false);
    if(chargeSlot<kMoveSlots){moveSlot=chargeSlot;forcedCharge=true;}
    else selfVolatile.chargingMove=MoveId::None;
  }else if(player&&selfVolatile.encoreMove!=MoveId::None){
    const uint8_t encoreSlot=moveSlotFor(*player,selfVolatile.encoreMove);
    if(encoreSlot<kMoveSlots){moveSlot=encoreSlot;forcedEncore=true;}
    else clearEncore(selfVolatile,battle.playerEncoreTurns);
  }
  bool anyUsableMove = false;
  if (player) for (uint8_t slot = 0; slot < kMoveSlots; ++slot)
    if (player->moves[slot] != MoveId::None && player->movePp[slot] != 0 &&
        isMoveAvailableInSingleBattle(player->moves[slot]) &&
        moveIsSelectable(battle,collection,player->moves[slot],BattleSide::Player)) {
      anyUsableMove = true; break;
    }
  const bool usingStruggle = !forcedBide && moveSlot == kMoveSlots && !anyUsableMove;
  // A faint is a hard synchronization barrier. Presentation, PvP and save
  // restoration may observe the resolved state before the replacement has
  // been selected, but a zero-HP battler must never be allowed to spend a
  // recharge turn or execute another command.
  if (!battle.active || !player || !opponent || !player->currentHp ||
      !opponent->currentHp || (!usingStruggle && !forcedBide && !forcedCharge && !forcedLocked &&
      (moveSlot >= kMoveSlots || player->moves[moveSlot] == MoveId::None || player->movePp[moveSlot] == 0 ||
       (!forcedEncore&&heldIs(*player,battle.playerVolatile,HeldItem::ChoiceBand)&&battle.playerVolatile.choiceMove!=MoveId::None&&battle.playerVolatile.choiceMove!=player->moves[moveSlot])))) return;
  MoveId selectedMove = usingStruggle ? MoveId::Struggle : player->moves[moveSlot];
  // Imprison makes shared moves unavailable at command selection time. Do not
  // spend PP or a turn when a stale UI tap still reaches the resolver.
  if(!forcedCharge&&!forcedLocked&&
     !moveIsSelectable(battle,collection,selectedMove,BattleSide::Player)){
    // FireRed's selection-time move limitations cancel an existing chain only
    // for Torment. Disabled, Taunt and Imprison merely reject the selection.
    if(battle.playerMoveEffects.tormented&&selfVolatile.lastMoveUsed==selectedMove)
      cancelMultiTurnMoves(battle.playerMoveEffects,selfVolatile,playerBide,result,
                           BattleSide::Player,player->uid);
    return;
  }
  result.accepted = true; ++battle.turn;
  CombatVolatile& targetVolatile=battle.opponentVolatiles[battle.opponentIndex];
  DedicatedMoveEffectState& selfEffects=battle.playerMoveEffects;
  DedicatedMoveEffectState& targetEffects=battle.opponentMoveEffects[battle.opponentIndex];
  selfEffects.lastDamageReceived=0;selfEffects.lastDamagingMove=MoveId::None;
  selfEffects.lastDamageWasPhysical=false;
  targetEffects.lastDamageReceived=0;targetEffects.lastDamagingMove=MoveId::None;
  targetEffects.lastDamageWasPhysical=false;
  selfVolatile.protectedThisTurn=false;
  targetVolatile.protectedThisTurn=false;
  battle.playerEndureThisTurn=false;
  battle.opponentEndureThisTurn=false;
  const uint8_t enemyMoveSlot = gPvpOpponentMoveOverride >= 0
      ? static_cast<uint8_t>(gPvpOpponentMoveOverride) : chooseEnemyMove(battle, *player);
  const bool opponentSpentTurnSwitching = enemyMoveSlot == 0xFEU;
  const FullMoveData* plannedPlayerMove = findFullMove(selectedMove);
  const bool enemyUsingHealingItem=enemyCanChooseHealingItem(
      battle,*opponent,targetVolatile,battle.opponentBides[battle.opponentIndex],
      targetEffects);
  // Items are a separate, higher-order action in FireRed. Do not let the
  // move the AI would otherwise have chosen influence priority, Quick Claw,
  // Pursuit or the Protect chain on an item turn.
  const FullMoveData* plannedEnemyMove = !enemyUsingHealingItem&&enemyMoveSlot < kMoveSlots
      ? findFullMove(opponent->moves[enemyMoveSlot]) : nullptr;
  if (!isProtectionMove(plannedPlayerMove)) battle.playerProtectChain=0;
  if (!isProtectionMove(plannedEnemyMove)) battle.opponentProtectChain=0;
  int32_t playerSpeed = staged(battleCalculatedStat(battle,*player, PokemonStat::Speed), selfVolatile.speedStage);
  int32_t enemySpeed = staged(battleCalculatedStat(battle,*opponent, PokemonStat::Speed), targetVolatile.speedStage);
  const bool suppressWeather=abilityIs(*player,"CLOUD NINE")||abilityIs(*player,"AIR LOCK")||abilityIs(*opponent,"CLOUD NINE")||abilityIs(*opponent,"AIR LOCK");
  if(!suppressWeather){
    if(battle.weather==BattleWeather::Rain||battle.weather==BattleWeather::HeavyRain){if(abilityIs(*player,"SWIFT SWIM"))playerSpeed*=2;if(abilityIs(*opponent,"SWIFT SWIM"))enemySpeed*=2;}
    if(battle.weather==BattleWeather::Sun||battle.weather==BattleWeather::HarshSun){if(abilityIs(*player,"CHLOROPHYLL"))playerSpeed*=2;if(abilityIs(*opponent,"CHLOROPHYLL"))enemySpeed*=2;}
  }
  if (player->status == StatusCondition::Paralysis) playerSpeed /= 4;
  if (opponent->status == StatusCondition::Paralysis) enemySpeed /= 4;
  if(heldIs(*player,selfVolatile,HeldItem::MachoBrace))playerSpeed=std::max<int32_t>(1,playerSpeed/2);
  if(heldIs(*opponent,targetVolatile,HeldItem::MachoBrace))enemySpeed=std::max<int32_t>(1,enemySpeed/2);
  if(player->speciesId==132U&&heldIs(*player,selfVolatile,HeldItem::QuickPowder)&&
     !battle.playerMoveEffects.transformed)playerSpeed*=2;
  if(opponent->speciesId==132U&&heldIs(*opponent,targetVolatile,HeldItem::QuickPowder)&&
     !battle.opponentMoveEffects[battle.opponentIndex].transformed)enemySpeed*=2;
  const int8_t playerPriority = plannedPlayerMove ? static_cast<int8_t>(plannedPlayerMove->priority+
      (plannedPlayerMove->power==0&&abilityIs(*player,"PRANKSTER")?1:0)) : 0;
  const int8_t enemyPriority = plannedEnemyMove ? static_cast<int8_t>(plannedEnemyMove->priority+
      (plannedEnemyMove->power==0&&abilityIs(*opponent,"PRANKSTER")?1:0)) : 0;
  const bool playerPursuesBatonPass=effectIs(plannedPlayerMove,"PURSUIT")&&
      effectIs(plannedEnemyMove,"BATON_PASS");
  const bool playerQuick=!enemyUsingHealingItem&&heldIs(*player,selfVolatile,HeldItem::QuickClaw)&&random(battle)%5U==0;
  const bool enemyQuick=!enemyUsingHealingItem&&heldIs(*opponent,targetVolatile,HeldItem::QuickClaw)&&random(battle)%5U==0;
  const bool enemyFirst = enemyUsingHealingItem || (plannedEnemyMove && !playerPursuesBatonPass && (enemyPriority > playerPriority ||
      (enemyPriority == playerPriority && ((enemyQuick&&!playerQuick) ||
       (enemyQuick==playerQuick&&(enemySpeed > playerSpeed ||
       (enemySpeed == playerSpeed && (random(battle) & 1U))))))));
  if (enemyFirst) {
    const uint32_t actingUid = player->uid;
    enemyTurn(battle, collection, *player, result, enemyMoveSlot);
    // A faster foe may knock itself out in confusion.  Its replacement must
    // not receive the move that was selected against the fainted battler.
    if (!battle.active || battle.playerUid != actingUid || player->currentHp == 0 ||
        result.opponentDefeated) {
      if (battle.active) resolveNightmareTurn(battle, collection, result);
      resolveDelayedAttacks(battle, collection, result); result.outcome = battle.outcome;
      if(!battle.active)restorePlayerBattleForm(battle,collection);appendBattleEnd(result,battle);return;
    }
  }
  // This is the player's AtkCanceller boundary.  If a faster opponent faints
  // the user above, the old vows still trigger; once the user gets its own
  // action, even an action stopped by status, both vows expire first.
  selfEffects.destinyBond=false;
  selfEffects.grudge=false;
  auto enemyIfNeeded = [&]() {
    if (!result.enemyActed && !opponentSpentTurnSwitching)
      enemyTurn(battle, collection, *player, result, enemyMoveSlot);
  };
  auto finishInterruptedTurn = [&]() {
    resolveNightmareTurn(battle, collection, result);
    resolveDelayedAttacks(battle, collection, result); result.outcome = battle.outcome;
    if(battle.active)advanceEndTurnEffects(battle,collection,result);
    if (!battle.active) restorePlayerBattleForm(battle, collection); appendBattleEnd(result, battle);
  };
  const FullMoveData* selectedMoveData = findFullMove(selectedMove);
  if((targetEffects.imprisoned&&pokemonKnowsMove(*opponent,selectedMove))||
     (selfEffects.tauntTurns&&selectedMoveData&&selectedMoveData->power==0)||
     (selfEffects.tormented&&selfVolatile.lastMoveUsed==selectedMove)){
    BattleEvent& blocked=appendEvent(result,BattleEventType::CannotMove,
                                     BattleSide::Player,player->uid);
    blocked.value=(selfEffects.tauntTurns&&selectedMoveData&&selectedMoveData->power==0)?11U:
        (selfEffects.tormented&&selfVolatile.lastMoveUsed==selectedMove)?12U:10U;
    cancelMultiTurnMoves(selfEffects,selfVolatile,playerBide,result,
                         BattleSide::Player,player->uid);
    enemyIfNeeded();finishInterruptedTurn();return;
  }
  if(truantLoafsThisTurn(battle,*player,selfEffects)){cancelMultiTurnMoves(selfEffects,selfVolatile,playerBide,result,BattleSide::Player,player->uid);appendEvent(result,BattleEventType::CannotMove,BattleSide::Player,player->uid).value=9;enemyIfNeeded();finishInterruptedTurn();return;}
  if(selfVolatile.recharging){selfVolatile.recharging=false;cancelMultiTurnMoves(selfEffects,selfVolatile,playerBide,result,BattleSide::Player,player->uid);appendEvent(result,BattleEventType::CannotMove,BattleSide::Player,player->uid).value=1;enemyIfNeeded();finishInterruptedTurn();return;}
  if(selfVolatile.flinched){selfVolatile.flinched=false;if(abilityIs(*player,"STEADFAST")){const StageSnapshot before=snapshotStages(selfVolatile);if(changeStage(selfVolatile.speedStage,1)){appendAbilityActivation(result,BattleSide::Player,*player);appendStageChanges(result,BattleSide::Player,player->uid,before,selfVolatile);}}cancelMultiTurnMoves(selfEffects,selfVolatile,playerBide,result,BattleSide::Player,player->uid);appendEvent(result,BattleEventType::CannotMove,BattleSide::Player,player->uid).value=2;enemyIfNeeded();finishInterruptedTurn();return;}
  if(selfVolatile.confusionTurns){
    --selfVolatile.confusionTurns;
    if(!selfVolatile.confusionTurns){
      appendMoveEffect(result,BattleSide::Player,player->uid,MoveId::None,
                       BattleMoveEffect::ConfusionEnded);
    }else{
      appendMoveEffect(result,BattleSide::Player,player->uid,MoveId::None,
                       BattleMoveEffect::ConfusionActive);
      if(random(battle)%2U==0){
        const uint16_t before=player->currentHp;
        const uint16_t hurt=confusionSelfDamage(battle,*player,selfVolatile);
        player->currentHp=hurt>=player->currentHp?0:
            static_cast<uint16_t>(player->currentHp-hurt);
        BattleEvent& blocked=appendEvent(result,BattleEventType::CannotMove,
                                          BattleSide::Player,player->uid);
        blocked.value=3;
        appendHpChange(result,BattleSide::Player,player->uid,before,
                       player->currentHp);
        cancelMultiTurnMoves(selfEffects,selfVolatile,playerBide,result,
                             BattleSide::Player,player->uid);
        if(!player->currentHp){
          player->recoverySecondsRemaining=1;
          appendFaintedOnce(result,BattleSide::Player,*player);
          if(!firstHealthyPartyMember(collection,player->uid)){
            battle.active=false;
            battle.outcome=BattleOutcome::Defeat;
          }
        }else enemyIfNeeded();
        finishInterruptedTurn();return;
      }
    }
  }
  if(selfEffects.attractedToUid&&selfEffects.attractedToUid==opponent->uid&&random(battle)%2U==0U){appendEvent(result,BattleEventType::CannotMove,BattleSide::Player,player->uid).value=13;cancelMultiTurnMoves(selfEffects,selfVolatile,playerBide,result,BattleSide::Player,player->uid);enemyIfNeeded();finishInterruptedTurn();return;}
  if(selfVolatile.disabledMove==selectedMove) { appendEvent(result,BattleEventType::CannotMove,BattleSide::Player,player->uid).value=4;cancelMultiTurnMoves(selfEffects,selfVolatile,playerBide,result,BattleSide::Player,player->uid);enemyIfNeeded();finishInterruptedTurn();return; }
  if (player->status == StatusCondition::Sleep) {
    if (!selfVolatile.sleepTurns) selfVolatile.sleepTurns = static_cast<uint8_t>(2U + random(battle) % 4U);
    const bool woke=wakeAfterSleepTick(*player,selfVolatile);
    if (!woke && !effectIs(selectedMoveData, "SNORE")&&
        !effectIs(selectedMoveData,"SLEEP_TALK")) { appendEvent(result,BattleEventType::CannotMove,BattleSide::Player,player->uid).value=6;if(selfEffects.lockedMoveTurns||playerBide.turns||selfVolatile.chargingMove!=MoveId::None)cancelMultiTurnMoves(selfEffects,selfVolatile,playerBide,result,BattleSide::Player,player->uid);enemyIfNeeded(); finishInterruptedTurn(); return; }
    if(woke){
      player->status = StatusCondition::None;
      appendStatusChange(result,BattleSide::Player,player->uid,StatusCondition::Sleep,player->status);
    }
    if(player->status!=StatusCondition::Sleep)battle.playerNightmare=false;
  } else if (player->status == StatusCondition::Frozen) {
    if (!effectIs(selectedMoveData,"THAW_HIT")&&random(battle) % 5U) {
      appendEvent(result,BattleEventType::CannotMove,BattleSide::Player,
                  player->uid).value=7;enemyIfNeeded();finishInterruptedTurn();return;
    }
    player->status = StatusCondition::None;
    appendStatusChange(result,BattleSide::Player,player->uid,StatusCondition::Frozen,player->status);
  } else if (player->status == StatusCondition::Paralysis && random(battle) % 4U == 0) {
    appendEvent(result,BattleEventType::CannotMove,BattleSide::Player,player->uid).value=8;cancelMultiTurnMoves(selfEffects,selfVolatile,playerBide,result,BattleSide::Player,player->uid);enemyIfNeeded(); finishInterruptedTurn(); return;
  }
  if(playerBide.turns){
    if(--playerBide.turns){
      appendMoveEffect(result,BattleSide::Player,player->uid,static_cast<MoveId>(117),
                       BattleMoveEffect::BideStoring);
      enemyIfNeeded();finishInterruptedTurn();return;
    }
    const uint16_t stored=playerBide.damage;
    playerBide=BideState{};
    appendMoveEffect(result,BattleSide::Player,player->uid,static_cast<MoveId>(117),
                     BattleMoveEffect::BideUnleashed);
    const FullMoveData* bideMove=findFullMove(static_cast<MoveId>(117));
    const SpeciesData* targetSpecies=findSpecies(opponent->speciesId);
    const bool immune=targetSpecies&&typeMultiplier100(PokemonType::Normal,*targetSpecies)==0U;
    const bool protectedTarget=targetVolatile.protectedThisTurn;
    const bool accuracyHit=stored&&!immune&&!protectedTarget&&bideMove&&
        moveConnects(battle,*bideMove,*player,selfVolatile,*opponent,targetVolatile);
    if(!stored||immune||protectedTarget)
      appendMoveEffect(result,BattleSide::Opponent,opponent->uid,static_cast<MoveId>(117),BattleMoveEffect::Failed);
    else if(!accuracyHit)
      appendEvent(result,BattleEventType::MoveMissed,BattleSide::Player,player->uid).move=static_cast<MoveId>(117);
    else{
      uint16_t applied=static_cast<uint16_t>(std::min<uint32_t>(65535U,static_cast<uint32_t>(stored)*2U));
      if(targetVolatile.substituteHp){const uint16_t absorbed=std::min<uint16_t>(targetVolatile.substituteHp,applied);targetVolatile.substituteHp=static_cast<uint16_t>(targetVolatile.substituteHp-absorbed);applied=static_cast<uint16_t>(applied-absorbed);}
      applied=applyHeldDamage(battle,*opponent,targetVolatile,applied);
      if(battle.opponentEndureThisTurn&&applied>=opponent->currentHp&&opponent->currentHp)applied=static_cast<uint16_t>(opponent->currentHp-1U);
      const uint16_t before=opponent->currentHp;
      opponent->currentHp=applied>=opponent->currentHp?0U:static_cast<uint16_t>(opponent->currentHp-applied);
      result.hit=true;result.damageDealt=static_cast<uint16_t>(before-opponent->currentHp);
      appendHpChange(result,BattleSide::Opponent,opponent->uid,before,opponent->currentHp);
      triggerHeldItemWithResult(result,*opponent,targetVolatile,BattleSide::Opponent,&targetEffects);
      if(!opponent->currentHp){
        resolveFaintVows(result,BattleSide::Opponent,*opponent,targetEffects,*player,
                         static_cast<MoveId>(117),moveSlot,result.damageDealt);
        appendFaintedOnce(result,BattleSide::Opponent,*opponent);
        result.opponentDefeated=true;awardExperience(battle,collection,result);
        finishOpponentFaint(battle,collection,result);
        if(!player->currentHp){
          player->recoverySecondsRemaining=1;
          appendFaintedOnce(result,BattleSide::Player,*player);
          if(!firstHealthyPartyMember(collection,player->uid)){
            battle.active=false;battle.outcome=BattleOutcome::Defeat;
          }
        }
      }
    }
    if(battle.active&&opponent->currentHp)enemyIfNeeded();
    finishInterruptedTurn();return;
  }
  const bool releasingCharge = selfVolatile.chargingMove == selectedMove;
  if (!releasingCharge && !forcedLocked && !usingStruggle)
    player->movePp[moveSlot]=static_cast<uint8_t>(player->movePp[moveSlot]-
        std::min<uint8_t>(player->movePp[moveSlot],
                          pressurePpCost(selectedMoveData,*opponent)));
  if(!usingStruggle && heldIs(*player,selfVolatile,HeldItem::ChoiceBand)&&selfVolatile.choiceMove==MoveId::None)selfVolatile.choiceMove=selectedMove;
  const MoveId commandMove=selectedMove;
  const FullMoveData* usedMove = selectedMoveData;
  uint16_t playerHpBeforeMove = player->currentHp;
  StatusCondition playerStatusBeforeMove = player->status;
  BattleEvent& playerMoveEvent = appendEvent(result, BattleEventType::MoveUsed, BattleSide::Player, player->uid);
  playerMoveEvent.move = commandMove;
  BattleEvent* resolvedMoveEvent = &playerMoveEvent;
  const uint8_t playerFeedbackStart = result.eventCount;
  if(isMoveCaller(usedMove)){
    const MoveId called=resolveCalledMove(battle,collection,*player,selfVolatile,
                                          *opponent,targetVolatile,usedMove,
                                          BattleSide::Player);
    if(called==MoveId::None){
      appendMoveEffect(result,BattleSide::Player,player->uid,commandMove,
                       BattleMoveEffect::Failed);
      enemyIfNeeded();finishInterruptedTurn();return;
    }
    selectedMove=called;usedMove=findFullMove(called);
    BattleEvent& calledEvent=appendEvent(result,BattleEventType::MoveUsed,
                                         BattleSide::Player,player->uid);
    calledEvent.move=called;
    resolvedMoveEvent=&calledEvent;
  }
  if(effectIs(usedMove,"MIMIC")||effectIs(usedMove,"SKETCH")){
    copyLastMove(battle,result,*player,targetVolatile,moveSlot,*usedMove,
                 BattleSide::Player,effectIs(usedMove,"SKETCH"));
    enemyIfNeeded();finishInterruptedTurn();return;
  }
  if(effectIs(usedMove,"FURY_CUTTER"))
    selfEffects.furyCutterCount=static_cast<uint8_t>(std::min<uint8_t>(5U,selfEffects.furyCutterCount+1U));
  if(!effectIs(usedMove,"RAGE"))selfEffects.rageActive=false;
  selfVolatile.lastMoveUsed = selectedMove;
  if (effectIs(usedMove, "BATON_PASS")) {
    if (!firstHealthyPartyMember(collection, player->uid)) {
      appendMoveEffect(result, BattleSide::Player, player->uid, selectedMove,
                       BattleMoveEffect::Failed);
      enemyIfNeeded();
      finishInterruptedTurn();
      return;
    }
    appendMoveEffect(result, BattleSide::Player, player->uid, selectedMove,
                     BattleMoveEffect::BatonPass);
    result.batonPassSwitchRequired = true;
    result.batonPassOpponentMoveSlot = result.enemyActed ? 0xFEU : enemyMoveSlot;
    result.outcome = battle.outcome;
    return;
  }
  if(effectIs(usedMove,"BIDE")){
    playerBide.damage=0;playerBide.turns=2;
    appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,BattleMoveEffect::BideStoring);
    enemyIfNeeded();finishInterruptedTurn();return;
  }
  // See the opponent path above.  This is attached to the resolved move
  // event (rather than inferred later from mutable battle state), so called
  // moves and deferred journal playback retain the correct Dig phase.
  resolvedMoveEvent->stageBefore = releasingCharge ? 1 : 0;
  if (effectIs(usedMove,"SPIT_UP") || effectIs(usedMove,"SWALLOW"))
    resolvedMoveEvent->stageBefore = static_cast<int8_t>(selfVolatile.stockpileCount);
  if (static_cast<uint16_t>(selectedMove) == 91U)
    resolvedMoveEvent->value = releasingCharge ? 2U : 1U;
  if(isChargingMove(battle,usedMove,*player,*opponent)&&!releasingCharge){
    selfVolatile.chargingMove=selectedMove;
    if(effectIs(usedMove,"SKULL_BASH")){
      const int8_t before=selfVolatile.defenseStage;
      if(changeStage(selfVolatile.defenseStage,1)){
        BattleEvent& raised=appendEvent(result,BattleEventType::StatChanged,
                                        BattleSide::Player,player->uid);
        raised.value=1;raised.stageBefore=before;raised.stageAfter=selfVolatile.defenseStage;
      }
    }
    enemyIfNeeded();finishInterruptedTurn();return;
  }
  selfVolatile.chargingMove=MoveId::None;
  if(usedMove&&usedMove->snatchAffected&&targetEffects.snatch){
    targetEffects.snatch=false;
    resolveSnatchedMove(battle,collection,result,*usedMove,*opponent,targetVolatile,
        targetEffects,*player,selfVolatile,selfEffects,BattleSide::Opponent);
    enemyIfNeeded();finishInterruptedTurn();return;
  }
  if(usedMove&&usedMove->magicCoatAffected&&
     (targetEffects.magicCoat||defenderAbilityApplies(*player,*opponent,"MAGIC BOUNCE"))){
    if(!defenderAbilityApplies(*player,*opponent,"MAGIC BOUNCE"))targetEffects.magicCoat=false;
    resolveMagicCoatReflection(battle,result,*usedMove,*opponent,targetVolatile,
        targetEffects,*player,selfVolatile,selfEffects,BattleSide::Opponent);
    enemyIfNeeded();finishInterruptedTurn();return;
  }
  const bool delayedAttack = effectIs(usedMove, "FUTURE_SIGHT");
  const bool delayedSlotFree = battle.delayedToOpponent.move == MoveId::None;
  const bool dampBlocked=effectIs(usedMove,"EXPLOSION")&&
      (abilityIs(*player,"DAMP")||
       defenderAbilityApplies(*player,*opponent,"DAMP"));
  const OhkoBlockReason playerOhkoBlock=ohkoBlockReason(usedMove,*player,*opponent);
  const bool requirementMet = moveUserRequirementMet(battle, usedMove, *player,
      selfVolatile,*opponent) &&
      moveRequirementMet(usedMove, *opponent, targetVolatile) &&
      playerOhkoBlock==OhkoBlockReason::None &&
      (!effectIs(usedMove,"NIGHTMARE") ||
       !battle.opponentNightmares[battle.opponentIndex]) &&
      (!effectIs(usedMove,"FOCUS_PUNCH") || !selfEffects.lastDamageReceived) &&
      (!effectIs(usedMove,"FAKE_OUT") ||
       battle.turn<=static_cast<uint16_t>(selfEffects.enteredTurn+1U)) &&
      (!delayedAttack || delayedSlotFree);
  const bool protectAllowsMove = !usedMove || !usedMove->protectAffected ||
      !targetVolatile.protectedThisTurn;
  StatusCondition attemptedPrimaryStatus=StatusCondition::None;
  const bool primaryStatusConflict=primaryStatusConflicts(
      battle,usedMove,*opponent,attemptedPrimaryStatus);
  const bool soundproofBlocked=usedMove&&isSoundMove(*usedMove)&&
      (defenderAbilityApplies(*player,*opponent,"SOUNDPROOF")||
       defenderAbilityApplies(*player,*opponent,"CACOPHONY"));
  const bool moveAbsorbed=usedMove&&
      absorbingAbilityBlocksMove(battle,usedMove,*player,*opponent);
  const bool connected = delayedAttack ? requirementMet : requirementMet && usedMove &&
      !primaryStatusConflict&&protectAllowsMove && (moveTargetsUser(usedMove) ||
      moveConnects(battle, *usedMove, *player, selfVolatile, *opponent, targetVolatile));
  const uint16_t opponentHpBefore = opponent->currentHp;
  const bool opponentHadSubstitute=targetVolatile.substituteHp!=0;
  StatusCondition opponentStatusBefore = opponent->status;
  gPresentHealRoll=false;
  if(playerPursuesBatonPass)gPursuitSwitchBoost=true;
  uint16_t beatUpDamage[5]{},beatUpEffectiveness[5]{100,100,100,100,100};
  bool beatUpCritical[5]{};
  const bool beatUpMove=effectIs(usedMove,"BEAT_UP");
  uint8_t plannedHitCount=0;
  if(connected&&!delayedAttack){
    if(beatUpMove){
      plannedHitCount=beatUpDamageHits(battle,collection,BattleSide::Player,*player,
          selfVolatile,*opponent,beatUpDamage,beatUpEffectiveness,beatUpCritical);
    }else plannedHitCount=chooseDamageHitCount(battle,*player,selectedMove);
  }
  uint32_t damageTotal=0;
  uint16_t damage=0;
  uint16_t crashEffectiveness=100U;
  const uint16_t projectedCrash=(!connected&&requirementMet&&
      effectIs(usedMove,"RECOIL_IF_MISS"))?
      calculateDamage(battle,*player,*opponent,selectedMove,selfVolatile,
          targetVolatile,&crashEffectiveness,nullptr,false):0U;
  gPursuitSwitchBoost=false;
  if (connected && delayedAttack) {
    battle.delayedToOpponent.move = selectedMove;
    battle.delayedToOpponent.baseDamage = calculateDelayedBaseDamage(
        battle, *player, *opponent, selectedMove, selfVolatile, targetVolatile);
    battle.delayedToOpponent.dueTurn = static_cast<uint16_t>(battle.turn + 2U);
    appendMoveEffect(result, BattleSide::Player, player->uid, selectedMove,
                     BattleMoveEffect::FutureAttackSet);
  }
  result.hit = connected; result.damageDealt = damage;
  uint16_t appliedDamage=0;
  uint16_t substituteDamage=0;
  uint8_t actualHitCount=0;
  const bool parentalBondMove=parentalBondEligible(*player,usedMove);
  const bool repeatedMove=isSameTurnMultiHit(usedMove)||parentalBondMove;
  if(connected){
    for(uint8_t hit=0;hit<plannedHitCount&&opponent->currentHp&&player->currentHp;++hit){
      if(hit&&effectIs(usedMove,"TRIPLE_KICK")&&
         !moveConnects(battle,*usedMove,*player,selfVolatile,
                       *opponent,targetVolatile))break;
      uint16_t hitEffectiveness=100U;
      bool hitCritical=false;
      uint16_t strike=0;
      if(beatUpMove){
        strike=beatUpDamage[hit];
        hitEffectiveness=beatUpEffectiveness[hit];
        hitCritical=beatUpCritical[hit];
      }else{
        const uint16_t kickPower=effectIs(usedMove,"TRIPLE_KICK")?
            static_cast<uint16_t>(10U*(hit+1U)):0U;
        strike=calculateDamage(battle,*player,*opponent,selectedMove,
            selfVolatile,targetVolatile,&hitEffectiveness,
            &hitCritical,true,kickPower);
        if(parentalBondMove&&hit)strike=std::max<uint16_t>(1U,strike/4U);
      }
      if(gPresentHealRoll){
        const uint16_t before=opponent->currentHp;
        opponent->currentHp=std::min<uint16_t>(opponent->maximumHp,
            static_cast<uint16_t>(opponent->currentHp+
            std::max<uint16_t>(1U,opponent->maximumHp/4U)));
        appendHpChange(result,BattleSide::Opponent,opponent->uid,
                       before,opponent->currentHp);
        ++actualHitCount;
        break;
      }
      result.effectiveness100=hitEffectiveness;
      result.criticalHit=result.criticalHit||hitCritical;
      if(!hitEffectiveness)break;
      if(actualHitCount&&repeatedMove){
        BattleEvent& replay=appendEvent(result,BattleEventType::MultiHitStrike,
                                         BattleSide::Player,player->uid);
        replay.move=selectedMove;
        replay.stageBefore=static_cast<int8_t>(actualHitCount);
      }
      ++actualHitCount;
      damageTotal=std::min<uint32_t>(65535U,damageTotal+strike);
      const uint16_t before=opponent->currentHp;
      uint16_t contactDamage=0;
      bool endedByEndure=false;
      if(targetVolatile.substituteHp){
        const uint16_t absorbed=std::min<uint16_t>(targetVolatile.substituteHp,strike);
        contactDamage=absorbed;
        substituteDamage=static_cast<uint16_t>(std::min<uint32_t>(65535U,
            static_cast<uint32_t>(substituteDamage)+absorbed));
        targetVolatile.substituteHp=static_cast<uint16_t>(targetVolatile.substituteHp-absorbed);
      }else if(usedMove&&effectIs(usedMove,"FALSE_SWIPE")&&strike>=opponent->currentHp){
        strike=opponent->currentHp>1U?static_cast<uint16_t>(opponent->currentHp-1U):0U;
        opponent->currentHp=1U;
        contactDamage=strike;
      }else{
        if(battle.opponentEndureThisTurn&&strike>=opponent->currentHp&&opponent->currentHp){
          strike=static_cast<uint16_t>(opponent->currentHp-1U);
          endedByEndure=true;
        }else strike=applyHeldDamage(battle,*opponent,targetVolatile,strike);
        opponent->currentHp=strike>=opponent->currentHp?0U:
            static_cast<uint16_t>(opponent->currentHp-strike);
        contactDamage=static_cast<uint16_t>(before-opponent->currentHp);
      }
      appliedDamage=static_cast<uint16_t>(std::min<uint32_t>(65535U,
          static_cast<uint32_t>(appliedDamage)+(before-opponent->currentHp)));
      if(before!=opponent->currentHp)appendHpChange(result,BattleSide::Opponent,
                                                     opponent->uid,before,opponent->currentHp);
      if(hitCritical)appendEvent(result,BattleEventType::CriticalHit,
                                 BattleSide::Player,player->uid);
      if(before!=opponent->currentHp&&targetEffects.rageActive){
        const int8_t beforeStage=targetVolatile.attackStage;
        if(changeStage(targetVolatile.attackStage,1)){
          BattleEvent& rage=appendEvent(result,BattleEventType::StatChanged,
                                        BattleSide::Opponent,opponent->uid);
          rage.value=0;rage.stageBefore=beforeStage;
          rage.stageAfter=targetVolatile.attackStage;
        }
      }
      const uint16_t attackerHpBefore=player->currentHp;
      const StatusCondition attackerStatusBefore=player->status;
      if(contactDamage)applyContactAbility(battle,usedMove,*player,*opponent,
                                            before!=opponent->currentHp,result);
      appendHpChange(result,BattleSide::Player,player->uid,
                     attackerHpBefore,player->currentHp);
      appendStatusChange(result,BattleSide::Player,player->uid,
                         attackerStatusBefore,player->status);
      triggerHeldItemWithResult(result,*opponent,targetVolatile,
                                BattleSide::Opponent,&targetEffects);
      triggerHeldItemWithResult(result,*player,selfVolatile,
                                BattleSide::Player,&selfEffects);
      opponentStatusBefore=opponent->status;
      playerStatusBeforeMove=player->status;
      playerHpBeforeMove=player->currentHp;
      if(endedByEndure)break;
      if(!beatUpMove&&commandMove!=static_cast<MoveId>(214)&&
         player->status==StatusCondition::Sleep)break;
    }
  }
  damage=static_cast<uint16_t>(damageTotal);
  if(resolvedMoveEvent){
    resolvedMoveEvent->before=appliedDamage;
    resolvedMoveEvent->after=gLastCalculatedMovePower;
    resolvedMoveEvent->stageAfter=gPresentHealRoll?1:0;
    if(repeatedMove)resolvedMoveEvent->value=actualHitCount;
  }
  if(connected&&result.effectiveness100!=100U){
    BattleEvent& event=appendEvent(result,BattleEventType::Effectiveness,
                                    BattleSide::Player,player->uid);
    event.effectiveness100=result.effectiveness100;
  }
  if((printsMultiHitCount(usedMove)||parentalBondMove)&&actualHitCount){
    BattleEvent& count=appendEvent(result,BattleEventType::MultiHitCount,
                                    BattleSide::Player,player->uid);
    count.move=selectedMove;count.value=actualHitCount;
  }
  storeBideDamage(battle.opponentBides[battle.opponentIndex],appliedDamage);
  const uint16_t playerInflictedDamage=static_cast<uint16_t>(std::min<uint32_t>(65535U,
      static_cast<uint32_t>(substituteDamage)+appliedDamage));
  result.damageDealt=playerInflictedDamage;
  if (!connected) {
    if(effectIs(usedMove,"FURY_CUTTER"))selfEffects.furyCutterCount=0;
    if(effectIs(usedMove,"ROLLOUT")){
      selfEffects.rolloutCount=0;selfEffects.lockedMoveTurns=0;
      selfEffects.lockedMove=MoveId::None;
    }
    if(primaryStatusConflict)
      appendMoveEffect(result,BattleSide::Opponent,opponent->uid,selectedMove,
                       existingStatusFeedback(attemptedPrimaryStatus,opponent->status));
    else if (!requirementMet){
      if(dampBlocked){
        if(abilityIs(*player,"DAMP"))appendAbilityActivation(
            result,BattleSide::Player,*player);
        else appendAbilityActivation(result,BattleSide::Opponent,*opponent);
      }else if(playerOhkoBlock!=OhkoBlockReason::None)
        appendOhkoFailure(result,BattleSide::Opponent,*opponent,selectedMove,
                          playerOhkoBlock);
      else appendMoveEffect(result, BattleSide::Opponent, opponent->uid, selectedMove,
                            BattleMoveEffect::Failed);
    }else if(soundproofBlocked&&protectAllowsMove)
      appendAbilityActivation(result,BattleSide::Opponent,*opponent);
    else
      appendEvent(result, BattleEventType::MoveMissed, BattleSide::Player,
                  player->uid).move = selectedMove;
  }
  else {
    if(effectIs(usedMove,"FURY_CUTTER")&&!result.effectiveness100)
      selfEffects.furyCutterCount=0;
  }
  if(connected)appendBlockingDamageAbility(battle,usedMove,*player,*opponent,
                                            result.effectiveness100,result);
  applyCrashDamage(usedMove,connected,projectedCrash,crashEffectiveness,
                   opponent->maximumHp,*player,result,BattleSide::Player);
  if(effectIs(usedMove,"MEMENTO")&&requirementMet&&!protectAllowsMove)
    player->currentHp=0;
  if(effectIs(usedMove,"SPIT_UP")&&requirementMet&&!protectAllowsMove)
    selfVolatile.stockpileCount=0;
  if(requirementMet&&!protectAllowsMove)
    cancelMultiTurnMoves(selfEffects,selfVolatile,playerBide,result,
                         BattleSide::Player,player->uid);
  const bool playerExploded=effectIs(usedMove,"EXPLOSION")&&
      !abilityIs(*player,"DAMP")&&
      !defenderAbilityApplies(*player,*opponent,"DAMP");
  if(playerExploded)player->currentHp=0;
  if (connected) {
    applyAbsorbAbility(battle,usedMove,*player,*opponent,damage,result);
  }
  const uint16_t actualDamage = appliedDamage;
  if(actualDamage&&usedMove){
    targetEffects.lastDamageReceived=actualDamage;
    targetEffects.lastDamagingMove=selectedMove;
    targetEffects.lastDamageWasPhysical=isPhysicalType(
        effectiveMoveType(battle,*usedMove,*player,opponent));
  }
  applyHalfDrain(usedMove,appliedDamage,*player,*opponent,result,
                 BattleSide::Player);
  // The drain helper emits the authoritative HP event immediately. Keep the
  // generic end-of-move snapshot aligned with it so ABSORB-family moves are
  // animated once, while any later HP mutation still receives its own event.
  if (isHalfDrainEffect(usedMove) && appliedDamage)
    playerHpBeforeMove = player->currentHp;
  applyMoveRecoil(usedMove,playerInflictedDamage,*player,result,BattleSide::Player);
  const StageSnapshot playerStagesBefore = snapshotStages(selfVolatile);
  const StageSnapshot opponentStagesBefore = snapshotStages(targetVolatile);
  const VolatileFeedbackSnapshot playerVolatileBefore = snapshotVolatileFeedback(selfVolatile);
  const VolatileFeedbackSnapshot opponentVolatileBefore = snapshotVolatileFeedback(targetVolatile);
  if(usedMove && connected){
    const bool moveAffected=(usedMove->power==0||result.effectiveness100!=0)&&
        !moveAbsorbed;
    if(moveAffected&&moveTargetsOpponent(usedMove)){
      targetEffects.lastLandedMove=selectedMove;
      targetEffects.lastLandedType=effectiveMoveType(
          battle,*usedMove,*player,opponent);
    }
    const uint8_t playerSecondaryChance=abilityIs(*player,"SERENE GRACE")?
        static_cast<uint8_t>(std::min<uint16_t>(100U,usedMove->effectChance*2U)):
        usedMove->effectChance;
    const bool targetShieldDust=defenderAbilityApplies(*player,*opponent,"SHIELD DUST")&&
        !secondaryEffectAffectsUser(usedMove);
    const bool statusRoll=moveUsesStatusEffectRoll(battle,usedMove);
    const bool sheerSuppressed=abilityIs(*player,"SHEER FORCE")&&
        sheerForceApplies(usedMove);
    const bool secondaryProc=statusRoll||playerSecondaryChance==0||
        (!sheerSuppressed&&random(battle)%100U<playerSecondaryChance);
    if(moveAffected&&usedMove->power&&targetShieldDust&&!statusRoll&&
       !sheerSuppressed&&secondaryProc)
      appendAbilityActivation(result,BattleSide::Opponent,*opponent);
    const bool applySecondary=moveAffected&&(usedMove->power==0||
        (!sheerSuppressed&&!targetShieldDust&&secondaryProc));
    if(moveAffected)
      applyDedicatedMoveEffect(battle,result,*usedMove,*player,selfVolatile,selfEffects,
                               *opponent,targetVolatile,targetEffects,BattleSide::Player);
    applyHeldMoveEffect(*usedMove,moveAffected,opponentHadSubstitute,
                        battle.kind,BattleSide::Player,*player,selfVolatile,
                        *opponent,targetVolatile,result);
    if(isWeatherMoveEffect(usedMove->effect)){
      if(applyMoveWeather(battle,usedMove->effect)){
        BattleEvent& weather=appendEvent(result,BattleEventType::MoveEffect,
                                          BattleSide::Player,player->uid);
        weather.move=selectedMove;
        weather.value=static_cast<uint16_t>(BattleMoveEffect::WeatherSet);
        weather.after=static_cast<uint16_t>(battle.weather);
        refreshForecastForms(battle,collection,&result);
      }else appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,
                             BattleMoveEffect::Failed);
    }
    if(std::strstr(usedMove->effect,"TRANSFORM")) {
      if(targetEffects.transformed||semiInvulnerable(targetVolatile)){
        appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,
                         BattleMoveEffect::Failed);
      }else{
        const uint16_t sourceSpecies = player->speciesId;
        const uint16_t targetSpecies = opponent->speciesId;
        transformInto(battle,*player,*opponent,selfVolatile,targetVolatile,
                      selfEffects,targetEffects,true);
        BattleEvent& transformed = appendEvent(result, BattleEventType::MoveEffect,
                                                 BattleSide::Player, player->uid);
        transformed.move = selectedMove;
        transformed.value = static_cast<uint16_t>(BattleMoveEffect::Transformed);
        transformed.before = sourceSpecies;
        transformed.after = targetSpecies;
        cureConditionForbiddenByAbility(*player,selfVolatile,selfEffects,
                                         BattleSide::Player,&result);
      }
    }
    if(effectIs(usedMove,"ROLE_PLAY")) {
      // Role Play is battle-local. Reuse the same original-Ability snapshot
      // used by Trace so switching or ending combat restores the collection.
      // Transform already owns a complete pre-transform snapshot, so it does
      // not need a second one here.
      if (!battle.playerAbilityTraced && !battle.playerTransformed) {
        battle.playerAbilityTraced = true;
        battle.playerPreTraceAbilityId = player->abilityId;
      }
      player->abilityId = opponent->abilityId;
      BattleEvent& copied = appendEvent(result, BattleEventType::MoveEffect,
                                         BattleSide::Player, player->uid);
      copied.move = selectedMove;
      copied.value = static_cast<uint16_t>(BattleMoveEffect::AbilityCopied);
      copied.after = player->abilityId;
      cureConditionForbiddenByAbility(*player,selfVolatile,selfEffects,
                                       BattleSide::Player,&result);
      refreshForecastPair(battle,*player,selfEffects,*opponent,targetEffects,&result);
    }
    if(effectIs(usedMove,"NIGHTMARE")){
      battle.opponentNightmares[battle.opponentIndex]=true;
      appendMoveEffect(result,BattleSide::Opponent,opponent->uid,selectedMove,
                       BattleMoveEffect::NightmareApplied);
    }
    if (effectIs(usedMove, "SAFEGUARD")) {
      if (!battle.playerSafeguardTurns) {
        battle.playerSafeguardTurns = 5;
        appendMoveEffect(result, BattleSide::Player, player->uid, selectedMove,
                         BattleMoveEffect::SafeguardSet);
      } else {
        appendMoveEffect(result, BattleSide::Player, player->uid, selectedMove,
                         BattleMoveEffect::Failed);
      }
    }
    if(effectIs(usedMove,"STOCKPILE")){
      ++selfVolatile.stockpileCount;
      BattleEvent& stockpiled=appendEvent(result,BattleEventType::MoveEffect,
                                           BattleSide::Player,player->uid);
      stockpiled.move=selectedMove;
      stockpiled.value=static_cast<uint16_t>(BattleMoveEffect::Stockpiled);
      stockpiled.after=selfVolatile.stockpileCount;
    }
    if(effectIs(usedMove,"SPIT_UP")){
      const uint8_t released=selfVolatile.stockpileCount;
      selfVolatile.stockpileCount=0;
      BattleEvent& release=appendEvent(result,BattleEventType::MoveEffect,
                                        BattleSide::Player,player->uid);
      release.move=selectedMove;
      release.value=static_cast<uint16_t>(BattleMoveEffect::StockpileReleased);
      release.before=released;
    }
    if(effectIs(usedMove,"SWALLOW")){
      const uint8_t swallowed=selfVolatile.stockpileCount;
      selfVolatile.stockpileCount=0;
      if(player->currentHp<player->maximumHp){
        const uint16_t healing=std::max<uint16_t>(1U,static_cast<uint16_t>(
            player->maximumHp/(1U<<(3U-swallowed))));
        player->currentHp=std::min<uint16_t>(player->maximumHp,
            static_cast<uint16_t>(player->currentHp+healing));
        BattleEvent& swallow=appendEvent(result,BattleEventType::MoveEffect,
                                          BattleSide::Player,player->uid);
        swallow.move=selectedMove;
        swallow.value=static_cast<uint16_t>(BattleMoveEffect::Swallowed);
        swallow.before=swallowed;
      }else appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,
                             BattleMoveEffect::Failed);
    }
    if(std::strstr(usedMove->effect,"RESTORE_HP") || std::strstr(usedMove->effect,"SOFTBOILED") ||
       std::strstr(usedMove->effect,"SYNTHESIS") || std::strstr(usedMove->effect,"MORNING_SUN") ||
       std::strstr(usedMove->effect,"MOONLIGHT"))
      player->currentHp=std::min<uint16_t>(player->maximumHp,
          static_cast<uint16_t>(player->currentHp + recoveryMoveAmount(
              battle, *player, *opponent, usedMove->effect)));
    if(std::strcmp(usedMove->effect,"REST")==0){
      player->currentHp=player->maximumHp;player->status=StatusCondition::Sleep;
      selfVolatile.sleepTurns=3;
    }
    if(std::strstr(usedMove->effect,"THAW_HIT")&&moveAffected&&
       player->status==StatusCondition::Frozen)player->status=StatusCondition::None;
    if(std::strstr(usedMove->effect,"REFRESH") && (player->status==StatusCondition::Poison ||
       player->status==StatusCondition::BadlyPoisoned || player->status==StatusCondition::Paralysis ||
       player->status==StatusCondition::Burn)) player->status=StatusCondition::None;
    if(std::strstr(usedMove->effect,"HEAL_BELL")) {
      for(uint8_t i=0;i<kPartyCapacity;++i)
        if(OwnedPokemon* member=CollectionLogic::active(collection,i))
          if(canBeCuredByBell(*usedMove,*member))member->status=StatusCondition::None;
      appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,BattleMoveEffect::TeamCured);
    }
    if(std::strstr(usedMove->effect,"HAZE")){
      clearStatStages(selfVolatile);clearStatStages(targetVolatile);
      appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,BattleMoveEffect::HazeCleared);
    }
    if(std::strstr(usedMove->effect,"BULK_UP")){changeStage(selfVolatile.attackStage,1);changeStage(selfVolatile.defenseStage,1);}
    if(std::strstr(usedMove->effect,"CALM_MIND")){changeStage(selfVolatile.spAttackStage,1);changeStage(selfVolatile.spDefenseStage,1);}
    if(std::strstr(usedMove->effect,"DRAGON_DANCE")){changeStage(selfVolatile.attackStage,1);changeStage(selfVolatile.speedStage,1);}
    if(std::strstr(usedMove->effect,"COSMIC_POWER")){changeStage(selfVolatile.defenseStage,1);changeStage(selfVolatile.spDefenseStage,1);}
    if(std::strstr(usedMove->effect,"SUPERPOWER")&&moveAffected){changeStage(selfVolatile.attackStage,-1);changeStage(selfVolatile.defenseStage,-1);}
    if(std::strstr(usedMove->effect,"OVERHEAT")&&moveAffected)changeStage(selfVolatile.spAttackStage,-2);
    if(std::strstr(usedMove->effect,"DEFENSE_CURL"))changeStage(selfVolatile.defenseStage,1);
    if(std::strstr(usedMove->effect,"MINIMIZE"))changeStage(selfVolatile.evasionStage,1);
    if (std::strcmp(usedMove->effect,"LIGHT_SCREEN")==0) {
      if (!battle.playerLightScreenTurns) {
        battle.playerLightScreenTurns=5;
        appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,
                         BattleMoveEffect::LightScreenSet);
      } else appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,
                              BattleMoveEffect::Failed);
    }
    if (std::strcmp(usedMove->effect,"REFLECT")==0) {
      if (!battle.playerReflectTurns) {
        battle.playerReflectTurns=5;
        appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,
                         BattleMoveEffect::ReflectSet);
      } else appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,
                              BattleMoveEffect::Failed);
    }
    if (std::strcmp(usedMove->effect,"MIST")==0) {
      if (!battle.playerMistTurns) {
        battle.playerMistTurns=5;
        appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,
                         BattleMoveEffect::MistSet);
      } else appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,
                              BattleMoveEffect::Failed);
    }
    if(std::strstr(usedMove->effect,"BRICK_BREAK")){
      battle.opponentReflectTurns=0;battle.opponentLightScreenTurns=0;
    }
    if(std::strstr(usedMove->effect,"TICKLE")&&!opponentHadSubstitute){
      applyStageEffect(battle,*player,"ATTACK_DOWN",selfVolatile,
                       targetVolatile,*opponent,&result);
      applyStageEffect(battle,*player,"DEFENSE_DOWN",selfVolatile,
                       targetVolatile,*opponent,&result);
    }
    if(std::strstr(usedMove->effect,"ALL_STATS_UP_HIT")&&applySecondary){changeStage(selfVolatile.attackStage,1);changeStage(selfVolatile.defenseStage,1);changeStage(selfVolatile.spAttackStage,1);changeStage(selfVolatile.spDefenseStage,1);changeStage(selfVolatile.speedStage,1);}
    if(std::strstr(usedMove->effect,"PSYCH_UP")){
      selfVolatile.attackStage=targetVolatile.attackStage;selfVolatile.defenseStage=targetVolatile.defenseStage;selfVolatile.spAttackStage=targetVolatile.spAttackStage;selfVolatile.spDefenseStage=targetVolatile.spDefenseStage;selfVolatile.speedStage=targetVolatile.speedStage;selfVolatile.accuracyStage=targetVolatile.accuracyStage;selfVolatile.evasionStage=targetVolatile.evasionStage;
      appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,
                       BattleMoveEffect::StatsCopied);
    }
    if(std::strstr(usedMove->effect,"BELLY_DRUM")&&
       player->currentHp>std::max<uint16_t>(1U,player->maximumHp/2U)&&
       selfVolatile.attackStage<kMaximumBattleStatStage){
      player->currentHp=static_cast<uint16_t>(player->currentHp-
          std::max<uint16_t>(1U,player->maximumHp/2U));
      selfVolatile.attackStage=kMaximumBattleStatStage;
    }
    if(std::strstr(usedMove->effect,"PAIN_SPLIT")){
      const uint16_t average=static_cast<uint16_t>((player->currentHp+opponent->currentHp)/2U);
      player->currentHp=std::min(player->maximumHp,average);opponent->currentHp=std::min(opponent->maximumHp,average);
      appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,
                       BattleMoveEffect::PainShared);
    }
    if(std::strstr(usedMove->effect,"SMELLINGSALT")&&moveAffected&&!opponentHadSubstitute&&
       opponent->status==StatusCondition::Paralysis)
      opponent->status=StatusCondition::None;
    if (applySecondary&&(moveTargetsUser(usedMove)||!opponentHadSubstitute))
      applyStageEffect(battle,*player,usedMove->effect,
                       selfVolatile,targetVolatile,*opponent,&result);
    if(effectIs(usedMove,"SECRET_POWER")&&applySecondary&&!opponentHadSubstitute)
      applySecretPowerNonStatus(battle,result,*player,selfVolatile,*opponent,
                                targetVolatile,BattleSide::Opponent);
    const bool playerConfusionRolled = !opponentHadSubstitute&&((std::strstr(usedMove->effect,"CONFUSE") && applySecondary) ||
        std::strstr(usedMove->effect,"TEETER_DANCE") || std::strstr(usedMove->effect,"SWAGGER") ||
        std::strstr(usedMove->effect,"FLATTER"))&&!targetVolatile.confusionTurns;
    const bool playerConfusionBlocked=playerConfusionRolled&&
        defenderAbilityApplies(*player,*opponent,"OWN TEMPO");
    if(playerConfusionBlocked)
      appendAbilityActivation(result,BattleSide::Opponent,*opponent);
    if (playerConfusionRolled&&!playerConfusionBlocked) {
      if (battle.opponentSafeguardTurns)
        appendMoveEffect(result, BattleSide::Opponent, opponent->uid, selectedMove,
                         BattleMoveEffect::SafeguardBlocked);
      else targetVolatile.confusionTurns=static_cast<uint8_t>(2U+random(battle)%4U);
    }
    if(std::strstr(usedMove->effect,"SWAGGER")&&!opponentHadSubstitute)changeStage(targetVolatile.attackStage,2);
    if(std::strstr(usedMove->effect,"FLATTER")&&!opponentHadSubstitute)changeStage(targetVolatile.spAttackStage,1);
    const bool playerFlinchRolled=(std::strstr(usedMove->effect,"FLINCH")||std::strstr(usedMove->effect,"FAKE_OUT")||std::strstr(usedMove->effect,"SNORE")||
        effectIs(usedMove,"TWISTER")||
        effectIs(usedMove,"SKY_ATTACK"))&&applySecondary&&!opponentHadSubstitute&&
        !sideAlreadyActed(result,BattleSide::Opponent);
    if(playerFlinchRolled&&defenderAbilityApplies(*player,*opponent,"INNER FOCUS"))
      appendAbilityActivation(result,BattleSide::Opponent,*opponent);
    else if(playerFlinchRolled)targetVolatile.flinched=true;
    const bool playerKingsRockProc=usedMove->kingsRockAffected&&result.damageDealt&&
       opponent->currentHp&&!opponentHadSubstitute&&
       !sideAlreadyActed(result,BattleSide::Opponent)&&
       heldIs(*player,selfVolatile,HeldItem::KingsRock)&&random(battle)%10U==0;
    if(playerKingsRockProc){
      if(defenderAbilityApplies(*player,*opponent,"SHIELD DUST")||
         defenderAbilityApplies(*player,*opponent,"INNER FOCUS"))
        appendAbilityActivation(result,BattleSide::Opponent,*opponent);
      else targetVolatile.flinched=true;
    }
    if(std::strstr(usedMove->effect,"TRAP")&&moveAffected&&!opponentHadSubstitute&&
       !targetVolatile.trappedTurns)
      targetVolatile.trappedTurns=static_cast<uint8_t>(3U+random(battle)%4U);
    if(std::strstr(usedMove->effect,"LEECH_SEED")&&!opponentHadSubstitute&&
       !hasBattleType(*opponent,targetEffects,PokemonType::Grass)&&!targetVolatile.seeded)
      targetVolatile.seeded=true;
    if(std::strstr(usedMove->effect,"RAPID_SPIN")&&moveAffected){
      if(selfVolatile.trappedTurns)selfVolatile.trappedTurns=0;
      if(selfVolatile.seeded)selfVolatile.seeded=false;
      if(battle.playerSpikesLayers)battle.playerSpikesLayers=0;
    }
    if (effectIs(usedMove,"PROTECT")) {
      if (protectionSucceeds(battle,battle.playerProtectChain,!result.enemyActed))
        selfVolatile.protectedThisTurn=true;
      else appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,BattleMoveEffect::Failed);
    }
    if (effectIs(usedMove,"ENDURE")) {
      if (protectionSucceeds(battle,battle.playerProtectChain,!result.enemyActed)) {
        battle.playerEndureThisTurn=true;
        appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,BattleMoveEffect::Endured);
      } else appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,BattleMoveEffect::Failed);
    }
    if(std::strstr(usedMove->effect,"SUBSTITUTE")&&!selfVolatile.substituteHp){
      const uint16_t cost=std::max<uint16_t>(1U,player->maximumHp/4U);
      if(player->currentHp>cost){player->currentHp-=cost;selfVolatile.substituteHp=cost;selfVolatile.trappedTurns=0;}
    }
    if(std::strstr(usedMove->effect,"RECHARGE")&&moveAffected)selfVolatile.recharging=true;
    if(usedMove->power&&usedMove->type==PokemonType::Electric)selfEffects.chargeTurns=0;
    if(effectIs(usedMove,"ENCORE")&&!opponentHadSubstitute&&
       targetVolatile.encoreMove==MoveId::None&&
       canEncoreMove(targetVolatile.lastMoveUsed)&&
       moveSlotFor(*opponent,targetVolatile.lastMoveUsed)<kMoveSlots){
      targetVolatile.encoreMove=targetVolatile.lastMoveUsed;
      battle.opponentEncoreTurns[battle.opponentIndex]=static_cast<uint8_t>(3U+(random(battle)&3U));
    }
  }
  appendHpChange(result, BattleSide::Player, player->uid,
                 playerHpBeforeMove, player->currentHp);
  appendStageChanges(result, BattleSide::Player, player->uid, playerStagesBefore, selfVolatile);
  appendStageChanges(result, BattleSide::Opponent, opponent->uid, opponentStagesBefore, targetVolatile);
  appendVolatileFeedback(result, BattleSide::Player, player->uid, selectedMove,
                         playerVolatileBefore, selfVolatile);
  appendVolatileFeedback(result, BattleSide::Opponent, opponent->uid, selectedMove,
                         opponentVolatileBefore, targetVolatile);
  if (connected&&!moveAbsorbed&&usedMove&&(!usedMove->power||result.effectiveness100)&&
      moveTargetsOpponent(usedMove))
    applyMoveStatus(battle,*player,selectedMove,*opponent,result,opponentHadSubstitute);
  appendStatusChange(result,BattleSide::Opponent,opponent->uid,opponentStatusBefore,opponent->status);
  appendStatusChange(result,BattleSide::Player,player->uid,playerStatusBeforeMove,player->status);
  if (connected && usedMove && usedMove->power == 0 &&
      result.eventCount == playerFeedbackStart) {
    const bool affectsUser=moveTargetsUser(usedMove);
    appendMoveEffect(result,
        affectsUser?BattleSide::Player:BattleSide::Opponent,
        affectsUser?player->uid:opponent->uid,selectedMove,BattleMoveEffect::Failed);
  }
  applyShellBellWithResult(result,*player,selfVolatile,BattleSide::Player,
                           playerInflictedDamage);
  triggerHeldItemWithResult(result,*player,selfVolatile,BattleSide::Player,&selfEffects);
  triggerHeldItemWithResult(result,*opponent,targetVolatile,BattleSide::Opponent,&targetEffects);
  if(connected&&usedMove&&effectIs(usedMove,"ROAR")){
    const bool suctionBlocked=defenderAbilityApplies(*player,*opponent,"SUCTION CUPS");
    const bool blocked=suctionBlocked||targetEffects.ingrained||
        !forceSwitchLevelCheck(battle,*player,*opponent);
    if(suctionBlocked)appendAbilityActivation(result,BattleSide::Opponent,*opponent);
    if(blocked&&!suctionBlocked)
      appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,
                       BattleMoveEffect::Failed);
    else if(battle.kind==BattleKind::Wild){
      battle.active=false;battle.outcome=BattleOutcome::Escaped;
      result.outcome=battle.outcome;return;
    }else if(!forceOpponentSwitch(battle,collection,result))
      appendMoveEffect(result,BattleSide::Player,player->uid,selectedMove,
                       BattleMoveEffect::Failed);
  }
  const bool playerFaintedFromMove = player->currentHp == 0;
  if (playerFaintedFromMove) player->recoverySecondsRemaining = 1;
  if (opponent->currentHp == 0) {
    resolveFaintVows(result,BattleSide::Opponent,*opponent,targetEffects,*player,
                     selectedMove,moveSlot,appliedDamage);
    appendFaintedOnce(result,BattleSide::Opponent,*opponent);
    result.opponentDefeated = true; awardExperience(battle, collection, result);
    finishOpponentFaint(battle, collection, result);
    if(!player->currentHp){player->recoverySecondsRemaining=1;
      appendFaintedOnce(result,BattleSide::Player,*player);
      if(!firstHealthyPartyMember(collection,player->uid)){
        battle.active=false;battle.outcome=BattleOutcome::Defeat;
      }
    }
  } else if (playerFaintedFromMove) {
    appendFaintedOnce(result,BattleSide::Player,*player);
    if (!firstHealthyPartyMember(collection, player->uid)) { battle.active = false; battle.outcome = BattleOutcome::Defeat; }
  } else if (!result.enemyActed && !opponentSpentTurnSwitching)
    enemyTurn(battle, collection, *player, result, enemyMoveSlot);
  resolveDelayedAttacks(battle, collection, result);
  if(battle.active){applyHeldEndTurnWithResult(result,*player,battle.playerVolatile,BattleSide::Player,&battle.playerMoveEffects);if(OwnedPokemon* activeOpponent=currentOpponent(battle))applyHeldEndTurnWithResult(result,*activeOpponent,battle.opponentVolatiles[battle.opponentIndex],BattleSide::Opponent,&battle.opponentMoveEffects[battle.opponentIndex]);advanceEndTurnEffects(battle,collection,result);}
  result.outcome = battle.outcome;
  if (!battle.active) restorePlayerBattleForm(battle, collection);
  appendBattleEnd(result, battle);
}

BattleActionResult BattleEngine::fight(BattleState& battle, PokemonCollection& collection,
                                       uint8_t moveSlot) {
  BattleActionResult result;
  fightInto(battle, collection, moveSlot, result);
  return result;
}

BattleActionResult BattleEngine::fightPvp(BattleState& battle, PokemonCollection& collection,
                                           uint8_t moveSlot, uint8_t opponentMoveSlot) {
  const int16_t previous = gPvpOpponentMoveOverride;
  gPvpOpponentMoveOverride = opponentMoveSlot;
  BattleActionResult result = fight(battle, collection, moveSlot);
  gPvpOpponentMoveOverride = previous;
  return result;
}

BattleActionResult BattleEngine::run(BattleState& battle, PokemonCollection& collection) {
  BattleActionResult result;
  runInto(battle, collection, result);
  return result;
}

void BattleEngine::runInto(BattleState& battle, PokemonCollection& collection,
                           BattleActionResult& result) {
  std::memset(&result, 0, sizeof(result));
  OwnedPokemon* player = CollectionLogic::find(collection, battle.playerUid);
  const SpeciesData* playerSpecies = player ? findSpecies(player->speciesId) : nullptr;
  const OwnedPokemon* opponent = currentOpponent(battle);
  const SpeciesData* wildSpecies = opponent ? findSpecies(opponent->speciesId) : nullptr;
  if (!battle.active || battle.kind == BattleKind::None || battle.kind == BattleKind::Pvp ||
      !player || !playerSpecies || !opponent) return;
  result.accepted = true; ++battle.turn;
  // RUN is a non-move action in FireRed and clears these one-action states
  // even when the escape attempt ultimately fails.
  battle.playerMoveEffects.furyCutterCount=0;
  battle.playerMoveEffects.destinyBond=false;
  battle.playerMoveEffects.grudge=false;

  // Trainer, Gym and League battles use RUN as an explicit surrender. It is
  // intentionally immediate: the player is leaving the complete challenge,
  // rather than attempting to outrun the active Pokemon. No money, XP, badge
  // or series progress is awarded, and the invitation remains available for
  // a fresh attempt from stage zero.
  if (battle.kind != BattleKind::Wild) {
    battle.active = false;
    battle.outcome = BattleOutcome::Escaped;
    result.outcome = battle.outcome;
    restorePlayerBattleForm(battle, collection);
    appendBattleEnd(result, battle);
    return;
  }
  if (!wildSpecies) { std::memset(&result, 0, sizeof(result)); return; }

  // Pokegochi permits an encounter at any time, so RUN is no longer a
  // priority escape action. The wild Pokemon completes its turn first. This
  // also means a trapping move used now can prevent the pending escape.
  enemyTurn(battle, collection, *player, result);
  if (!battle.active || player->currentHp == 0) {
    if (battle.active) resolveNightmareTurn(battle, collection, result);
    if (battle.active) advanceEndTurnEffects(battle, collection, result);
    result.outcome = battle.outcome; if (!battle.active) restorePlayerBattleForm(battle, collection); appendBattleEnd(result, battle); return;
  }

  const uint16_t playerSpeed = battleCalculatedStat(battle,*player, PokemonStat::Speed);
  const uint16_t wildSpeed = battleCalculatedStat(battle,*opponent, PokemonStat::Speed);
  const uint16_t chance = std::min<uint16_t>(95, static_cast<uint16_t>(50 + (playerSpeed * 30U) / std::max<uint16_t>(1, wildSpeed)));
  const bool guaranteed = abilityIs(*player,"RUN AWAY") || heldIs(*player,battle.playerVolatile,HeldItem::SmokeBall);
  if (!guaranteed && (battle.playerVolatile.trappedTurns||
      escapePrevented(battle.playerVolatile)||battle.playerMoveEffects.ingrained||
      trappedByAbility(*player,battle.playerMoveEffects,*opponent))) result.escapeBlocked = true;
  else if (guaranteed || random(battle) % 100U < chance) {
    battle.active = false; battle.outcome = BattleOutcome::Escaped;
  } else result.escapeFailed = true;

  if (battle.active) {
    resolveNightmareTurn(battle, collection, result);
    resolveDelayedAttacks(battle, collection, result);
    applyHeldEndTurnWithResult(result,*player,battle.playerVolatile,BattleSide::Player,&battle.playerMoveEffects);
    if(OwnedPokemon* activeOpponent=currentOpponent(battle))
      applyHeldEndTurnWithResult(result,*activeOpponent,battle.opponentVolatiles[battle.opponentIndex],BattleSide::Opponent,&battle.opponentMoveEffects[battle.opponentIndex]);
    advanceEndTurnEffects(battle, collection, result);
  }
  result.outcome = battle.outcome; if (!battle.active) restorePlayerBattleForm(battle, collection); appendBattleEnd(result, battle);
}

BattleActionResult BattleEngine::switchPokemon(BattleState& battle, PokemonCollection& collection) {
  BattleActionResult result;
  OwnedPokemon* current = CollectionLogic::find(collection, battle.playerUid);
  OwnedPokemon* trapper=currentOpponent(battle);
  if (!battle.active || !current || battle.playerVolatile.trappedTurns ||
      escapePrevented(battle.playerVolatile)||battle.playerMoveEffects.ingrained ||
      (trapper&&trappedByAbility(*current,battle.playerMoveEffects,*trapper))) return result;
  OwnedPokemon* replacement = nullptr;
  uint8_t currentSlot = 0;
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) if (collection.party[slot] == current->uid) currentSlot = slot;
  for (uint8_t offset = 1; offset < kPartyCapacity; ++offset) {
    OwnedPokemon* candidate = CollectionLogic::active(collection, static_cast<uint8_t>((currentSlot + offset) % kPartyCapacity));
    if (candidate && candidate->currentHp > 0 && candidate->recoverySecondsRemaining == 0) { replacement = candidate; break; }
  }
  if (!replacement) return result;
  return switchToPokemon(battle,collection,replacement->uid,false);
}

BattleActionResult BattleEngine::completeBatonPassSwitch(BattleState& battle,
                                                          PokemonCollection& collection,
                                                          uint32_t uid,
                                                          uint8_t opponentMoveSlot) {
  BattleActionResult result;
  OwnedPokemon* current = CollectionLogic::find(collection, battle.playerUid);
  OwnedPokemon* replacement = CollectionLogic::find(collection, uid);
  if (!battle.active || !current || !replacement || replacement->uid == current->uid ||
      !CollectionLogic::isInParty(collection, uid) || replacement->currentHp == 0 ||
      replacement->recoverySecondsRemaining) return result;

  const CombatVolatile passed = battle.playerVolatile;
  const DedicatedMoveEffectState passedMoveEffects=battle.playerMoveEffects;
  restorePlayerBattleForm(battle, collection);
  current = CollectionLogic::find(collection, battle.playerUid);
  CollectionLogic::ensureUsableMoves(*replacement);
  // Baton Pass is the exception to ordinary switching: Lock-On/Mind Reader
  // established by this side follows the battlefield position and gets a
  // fresh one-turn window for the recipient.
  for(auto& state:battle.opponentVolatiles)
    if(sureHitOwner(state)==1U)armSureHit(state,1U);
  storePlayerHeldItemState(battle, *current);
  storePlayerMoveEffectState(battle,*current);
  battle.playerUid = replacement->uid;
  loadPlayerHeldItemState(battle, *replacement);
  loadPlayerMoveEffectState(battle,*replacement);
  battle.playerMoveEffects.enteredTurn=battle.turn;
  applyBatonPassState(passed, battle.playerVolatile);
  applyBatonPassMoveEffects(passedMoveEffects,battle.playerMoveEffects);
  applySpikesOnEntry(battle,*replacement,BattleSide::Player,result);
  battle.playerNightmare = false;
  battle.playerBide = BideState{};
  clearEncore(battle.playerVolatile, battle.playerEncoreTurns);

  result.accepted = true;
  appendEvent(result, BattleEventType::SwitchedIn, BattleSide::Player,
              replacement->uid);
  applyEntryAbilities(battle, collection,BattleEntryScope::Player,&result);
  if (opponentMoveSlot != 0xFEU)
    enemyTurn(battle, collection, *replacement, result, opponentMoveSlot);
  resolveNightmareTurn(battle, collection, result);
  resolveDelayedAttacks(battle, collection, result);
  if (battle.active) {
    applyHeldEndTurnWithResult(result, *replacement, battle.playerVolatile,
                               BattleSide::Player,&battle.playerMoveEffects);
    if (OwnedPokemon* opponent = currentOpponent(battle))
      applyHeldEndTurnWithResult(result, *opponent,
          battle.opponentVolatiles[battle.opponentIndex], BattleSide::Opponent,
          &battle.opponentMoveEffects[battle.opponentIndex]);
    advanceEndTurnEffects(battle, collection, result);
  }
  result.outcome = battle.outcome;
  if (!battle.active) restorePlayerBattleForm(battle, collection);
  appendBattleEnd(result, battle);
  return result;
}

BattleActionResult BattleEngine::switchToPokemon(BattleState& battle, PokemonCollection& collection, uint32_t uid,
                                                  bool forcedAfterFaint) {
  BattleActionResult result;
  OwnedPokemon* current = CollectionLogic::find(collection, battle.playerUid);
  OwnedPokemon* replacement = CollectionLogic::find(collection, uid);
  OwnedPokemon* trapper=currentOpponent(battle);
  if (!battle.active || !current || !replacement || replacement->uid == current->uid ||
      (!forcedAfterFaint && (battle.playerVolatile.trappedTurns||
       escapePrevented(battle.playerVolatile)||battle.playerMoveEffects.ingrained||
       (trapper&&trappedByAbility(*current,battle.playerMoveEffects,*trapper)))) ||
      !CollectionLogic::isInParty(collection, uid) || replacement->currentHp == 0 ||
      replacement->recoverySecondsRemaining) return result;
  const uint8_t opponentMoveSlot=!forcedAfterFaint?
      (gPvpOpponentMoveOverride>=0?static_cast<uint8_t>(gPvpOpponentMoveOverride):
       chooseEnemyMove(battle,*current)):0xFEU;
  const OwnedPokemon* activeOpponent=currentOpponent(battle);
  const FullMoveData* switchingMove=activeOpponent&&opponentMoveSlot<kMoveSlots?
      findFullMove(activeOpponent->moves[opponentMoveSlot]):nullptr;
  const bool pursuitBeforeSwitch=!forcedAfterFaint&&effectIs(switchingMove,"PURSUIT");
  if(pursuitBeforeSwitch){
    result.accepted=true;++battle.turn;gPursuitSwitchBoost=true;
    enemyTurn(battle,collection,*current,result,opponentMoveSlot);
    gPursuitSwitchBoost=false;
    if(!current->currentHp||!battle.active){
      result.outcome=battle.outcome;
      if(!battle.active)restorePlayerBattleForm(battle,collection);
      appendBattleEnd(result,battle);return result;
    }
  }
  restorePlayerBattleForm(battle, collection);
  current = CollectionLogic::find(collection, battle.playerUid);
  CollectionLogic::ensureUsableMoves(*replacement);
  // This is an ordinary switch (Baton Pass has its own path), so effects
  // whose source is the outgoing player must end on every opposing target.
  for(auto& state:battle.opponentVolatiles){
    if(sureHitOwner(state)==1U)setSureHitOwner(state,0);
    setEscapePrevented(state,false);
  }
  result.accepted = true; if (!forcedAfterFaint&&!pursuitBeforeSwitch) ++battle.turn; storePlayerHeldItemState(battle,*current);storePlayerMoveEffectState(battle,*current);battle.playerUid = uid;
  battle.playerNightmare=false;
  loadPlayerHeldItemState(battle,*replacement);loadPlayerMoveEffectState(battle,*replacement);
  battle.playerMoveEffects.enteredTurn=battle.turn;
  applySpikesOnEntry(battle,*replacement,BattleSide::Player,result);
  appendEvent(result, BattleEventType::SwitchedIn, BattleSide::Player, replacement->uid);
  applyEntryAbilities(battle,collection,BattleEntryScope::Player,&result);
  if (!forcedAfterFaint) {
    if (!pursuitBeforeSwitch&&opponentMoveSlot != 0xFEU)
      enemyTurn(battle, collection, *replacement, result, opponentMoveSlot);
    resolveNightmareTurn(battle, collection, result);
    resolveDelayedAttacks(battle, collection, result);
    if(battle.active){applyHeldEndTurnWithResult(result,*replacement,battle.playerVolatile,BattleSide::Player,&battle.playerMoveEffects);if(OwnedPokemon* opponent=currentOpponent(battle))applyHeldEndTurnWithResult(result,*opponent,battle.opponentVolatiles[battle.opponentIndex],BattleSide::Opponent,&battle.opponentMoveEffects[battle.opponentIndex]);advanceEndTurnEffects(battle,collection,result);}
  }
  result.outcome = battle.outcome; if (!battle.active) restorePlayerBattleForm(battle, collection); appendBattleEnd(result, battle); return result;
}

BattleActionResult BattleEngine::switchToPokemonPvp(BattleState& battle, PokemonCollection& collection,
                                                      uint32_t uid, uint8_t opponentMoveSlot,
                                                      bool forcedAfterFaint) {
  const int16_t previous = gPvpOpponentMoveOverride;
  gPvpOpponentMoveOverride = opponentMoveSlot;
  BattleActionResult result = switchToPokemon(battle, collection, uid, forcedAfterFaint);
  gPvpOpponentMoveOverride = previous;
  return result;
}

BattleActionResult BattleEngine::switchOpponentPvp(BattleState& battle, PokemonCollection& collection,
                                                    uint8_t opponentIndex) {
  BattleActionResult result;
  if (!battle.active || battle.kind != BattleKind::Pvp || opponentIndex >= battle.opponentCount ||
      opponentIndex == battle.opponentIndex || !battle.opponents[opponentIndex].currentHp ||
      battle.opponentMoveEffects[battle.opponentIndex].ingrained||
      battle.opponentVolatiles[battle.opponentIndex].trappedTurns||
      escapePrevented(battle.opponentVolatiles[battle.opponentIndex])) return result;
  OwnedPokemon& outgoing = battle.opponents[battle.opponentIndex];
  const OwnedPokemon* trapper=CollectionLogic::find(collection,battle.playerUid);
  if(trapper&&trappedByAbility(outgoing,
      battle.opponentMoveEffects[battle.opponentIndex],*trapper))return result;
  if (abilityIs(outgoing, "NATURAL CURE")) outgoing.status = StatusCondition::None;
  restoreOpponentTransform(battle);
  restoreOpponentTemporaryAbility(outgoing);
  if(outgoing.speciesId==351U)outgoing.form=0;
  if(sureHitOwner(battle.playerVolatile)==2U)setSureHitOwner(battle.playerVolatile,0);
  setEscapePrevented(battle.playerVolatile,false);
  std::swap(battle.opponents[battle.opponentIndex], battle.opponents[opponentIndex]);
  std::swap(battle.opponentVolatiles[battle.opponentIndex], battle.opponentVolatiles[opponentIndex]);
  std::swap(battle.opponentMoveEffects[battle.opponentIndex],battle.opponentMoveEffects[opponentIndex]);
  setSureHitOwner(battle.opponentVolatiles[battle.opponentIndex],0);
  setSureHitOwner(battle.opponentVolatiles[opponentIndex],0);
  clearMoveEffectsOnSwitch(battle.opponentMoveEffects[battle.opponentIndex]);
  clearMoveEffectsOnSwitch(battle.opponentMoveEffects[opponentIndex]);
  battle.opponentMoveEffects[battle.opponentIndex].enteredTurn=battle.turn;
  // Switching cancels both multi-turn locks in FireRed; neither Bide's stored
  // damage nor Encore's forced command follows a Pokemon to the bench.
  battle.opponentBides[battle.opponentIndex]=BideState{};
  battle.opponentBides[opponentIndex]=BideState{};
  clearEncore(battle.opponentVolatiles[battle.opponentIndex],
              battle.opponentEncoreTurns[battle.opponentIndex]);
  clearEncore(battle.opponentVolatiles[opponentIndex],
              battle.opponentEncoreTurns[opponentIndex]);
  battle.opponentNightmares[battle.opponentIndex]=false;
  battle.opponentNightmares[opponentIndex]=false;
  result.accepted = true;
  appendEvent(result, BattleEventType::SwitchedIn, BattleSide::Opponent,
              battle.opponents[battle.opponentIndex].uid);
  applySpikesOnEntry(battle,battle.opponents[battle.opponentIndex],
                     BattleSide::Opponent,result);
  applyEntryAbilities(battle, collection,BattleEntryScope::Opponent,&result);
  result.outcome = battle.outcome;
  return result;
}

BattleActionResult BattleEngine::forfeitPvp(BattleState& battle, PokemonCollection& collection,
                                             bool localPlayerForfeits) {
  BattleActionResult result;
  if (!battle.active || battle.kind != BattleKind::Pvp) return result;
  result.accepted = true; battle.active = false;
  battle.outcome = localPlayerForfeits ? BattleOutcome::Defeat : BattleOutcome::Victory;
  result.outcome = battle.outcome;
  restorePlayerBattleForm(battle, collection);
  appendBattleEnd(result, battle);
  return result;
}

BattleActionResult BattleEngine::throwBall(BattleState& battle, PokemonCollection& collection,
                                            Inventory& inventory, PokeBallType ball) {
  BattleActionResult result;
  throwBallInto(battle, collection, inventory, ball, result);
  return result;
}

void BattleEngine::throwBallInto(BattleState& battle, PokemonCollection& collection,
                                 Inventory& inventory, PokeBallType ball,
                                 BattleActionResult& result) {
  std::memset(&result, 0, sizeof(result));
  const uint8_t index = static_cast<uint8_t>(ball);
  OwnedPokemon* player = CollectionLogic::find(collection, battle.playerUid);
  OwnedPokemon* opponent = currentOpponent(battle);
  const SpeciesData* species = opponent ? findSpecies(opponent->speciesId) : nullptr;
  if (!battle.active || battle.kind != BattleKind::Wild || !player || !opponent || !species || index >= static_cast<uint8_t>(PokeBallType::Count) ||
      inventory.balls[index] == 0 || CollectionLogic::count(collection) >= kBoxCapacity) return;
  result.accepted = true; --inventory.balls[index]; ++battle.turn;
  battle.playerMoveEffects.furyCutterCount=0;
  battle.playerMoveEffects.destinyBond=false;
  battle.playerMoveEffects.grudge=false;
  uint32_t catchValue = ((3U * opponent->maximumHp - 2U * opponent->currentHp) * species->catchRate * ballMultiplier100(ball));
  catchValue /= std::max<uint32_t>(1U, 3U * opponent->maximumHp * 100U);
  if (opponent->status == StatusCondition::Sleep || opponent->status == StatusCondition::Frozen) catchValue *= 2U;
  else if (opponent->status != StatusCondition::None) catchValue = catchValue * 3U / 2U;
  catchValue = std::min<uint32_t>(255U, catchValue);
  bool captured = ball == PokeBallType::MasterBall || catchValue >= 255U;
  if (captured) result.captureShakes = 4;
  else if (catchValue) {
    const double threshold = 1048560.0 / std::sqrt(std::sqrt(16711680.0 / static_cast<double>(catchValue)));
    while (result.captureShakes < 4U && (random(battle) & 0xFFFFU) < static_cast<uint32_t>(threshold))
      ++result.captureShakes;
    captured = result.captureShakes == 4U;
  }
  if (captured) {
    OwnedPokemon captured = *opponent; captured.currentHp = std::max<uint16_t>(1, captured.currentHp);
    result.caught = CollectionLogic::add(collection, captured);
    if (result.caught) { battle.active = false; battle.outcome = BattleOutcome::Captured; }
  }
  if (battle.active) {enemyTurn(battle, collection, *player, result);resolveNightmareTurn(battle,collection,result);resolveDelayedAttacks(battle,collection,result);if(battle.active){applyHeldEndTurnWithResult(result,*player,battle.playerVolatile,BattleSide::Player,&battle.playerMoveEffects);if(OwnedPokemon* activeOpponent=currentOpponent(battle))applyHeldEndTurnWithResult(result,*activeOpponent,battle.opponentVolatiles[battle.opponentIndex],BattleSide::Opponent,&battle.opponentMoveEffects[battle.opponentIndex]);advanceEndTurnEffects(battle,collection,result);}}
  result.outcome = battle.outcome; if (!battle.active) restorePlayerBattleForm(battle, collection); appendBattleEnd(result, battle);
}

BattleActionResult BattleEngine::useItem(BattleState& battle, PokemonCollection& collection,
                                          Inventory& inventory, BattleItem item, uint32_t targetUid,
                                          PpItemInventory* ppItems, uint8_t moveSlot) {
  BattleActionResult result{};
  useItemInto(battle, collection, inventory, item, result, targetUid, ppItems, moveSlot);
  return result;
}

void BattleEngine::useItemInto(BattleState& battle, PokemonCollection& collection,
                               Inventory& inventory, BattleItem item,
                               BattleActionResult& result, uint32_t targetUid,
                               PpItemInventory* ppItems, uint8_t moveSlot) {
  // This type is plain persistent/event data. Clearing the caller-owned
  // journal avoids both a ~1 KiB local result and a second return-value
  // temporary in handleTap(), which could trip loopTask's stack canary when
  // a potion was followed by enemy events and a save.
  std::memset(&result, 0, sizeof(result));
  OwnedPokemon* player = CollectionLogic::find(collection, battle.playerUid);
  OwnedPokemon* target = CollectionLogic::find(collection, targetUid ? targetUid : battle.playerUid);
  const uint8_t index = static_cast<uint8_t>(item);
  if (!battle.active || !player || !target || index >= static_cast<uint8_t>(BattleItem::Count)) return;
  uint16_t* itemQuantity = nullptr;
  if (isPpInventoryItem(item)) {
    const uint8_t ppIndex = ppInventoryIndex(item);
    if (!ppItems || ppIndex >= kPpItemCount) return;
    itemQuantity = &ppItems->quantities[ppIndex];
  } else {
    if (index >= kStandardMedicineCount) return;
    itemQuantity = &inventory.medicine[index];
  }
  if (!itemQuantity || !*itemQuantity) return;

  const uint16_t hpBefore = target->currentHp;
  const StatusCondition statusBefore = target->status;
  const bool targetIsActive = target == player;
  const StageSnapshot stagesBefore = snapshotStages(battle.playerVolatile);
  bool useful = false;
  auto heal = [&](uint16_t amount) {
    if (target->currentHp && target->currentHp < target->maximumHp) {
      target->currentHp = std::min<uint16_t>(target->maximumHp,
          static_cast<uint16_t>(target->currentHp + amount));
      useful = true;
    }
  };
  auto cure = [&](StatusCondition status) {
    if (target->status == status) { target->status = StatusCondition::None; useful = true; }
  };
  switch (item) {
    case BattleItem::Potion: heal(20); break;
    case BattleItem::SuperPotion: heal(50); break;
    case BattleItem::HyperPotion: heal(200); break;
    case BattleItem::FullHeal:
      if (target->status != StatusCondition::None) { target->status = StatusCondition::None; useful = true; }
      if (targetIsActive && battle.playerVolatile.confusionTurns) {
        battle.playerVolatile.confusionTurns = 0;
        useful = true;
      }
      break;
    case BattleItem::Antidote:
      if (target->status == StatusCondition::Poison || target->status == StatusCondition::BadlyPoisoned) {
        target->status = StatusCondition::None; useful = true;
      }
      break;
    case BattleItem::ParalyzeHeal: cure(StatusCondition::Paralysis); break;
    case BattleItem::Awakening: cure(StatusCondition::Sleep); break;
    case BattleItem::BurnHeal: cure(StatusCondition::Burn); break;
    case BattleItem::IceHeal: cure(StatusCondition::Frozen); break;
    case BattleItem::XAttack:
      if(target==player)useful=changeStage(battle.playerVolatile.attackStage,1);
      break;
    case BattleItem::XDefense:
      if(target==player)useful=changeStage(battle.playerVolatile.defenseStage,1);
      break;
    case BattleItem::XSpeed:
      if(target==player)useful=changeStage(battle.playerVolatile.speedStage,1);
      break;
    case BattleItem::XAccuracy:
      if(target==player)useful=changeStage(battle.playerVolatile.accuracyStage,1);
      break;
    case BattleItem::DireHit:
      if (target==player && !battle.playerVolatile.criticalStage) { battle.playerVolatile.criticalStage = 1; useful = true; }
      break;
    case BattleItem::Revive: case BattleItem::MaxRevive:
      if (!target->currentHp || target->recoverySecondsRemaining) {
        target->currentHp = item == BattleItem::MaxRevive ? target->maximumHp
            : std::max<uint16_t>(1U, static_cast<uint16_t>(target->maximumHp / 2U));
        target->status = StatusCondition::None;
        target->recoverySecondsRemaining = 0;
        useful = true;
      }
      break;
    case BattleItem::Ether: case BattleItem::MaxEther: {
      if (moveSlot >= kMoveSlots || target->moves[moveSlot] == MoveId::None) break;
      const uint8_t maximum = CollectionLogic::maximumMovePp(*target, moveSlot);
      if (!maximum || target->movePp[moveSlot] >= maximum) break;
      target->movePp[moveSlot] = item == BattleItem::MaxEther ? maximum :
          static_cast<uint8_t>(std::min<uint16_t>(maximum,
              static_cast<uint16_t>(target->movePp[moveSlot]) + 10U));
      useful = true;
      break;
    }
    case BattleItem::Elixir: case BattleItem::MaxElixir:
      for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
        const uint8_t maximum = CollectionLogic::maximumMovePp(*target, slot);
        if (!maximum || target->movePp[slot] >= maximum) continue;
        target->movePp[slot] = item == BattleItem::MaxElixir ? maximum :
            static_cast<uint8_t>(std::min<uint16_t>(maximum,
                static_cast<uint16_t>(target->movePp[slot]) + 10U));
        useful = true;
      }
      break;
    // PP UP changes permanent move capacity and is deliberately a field-only
    // item, matching the original games.
    case BattleItem::PpUp: break;
    // Rare Candy is also field-only. The Bag owns its level/evolution/move
    // learning flow, so it can never consume a battle turn.
    case BattleItem::RareCandy: break;
    case BattleItem::Count: break;
  }
  if (!useful) return;
  if (targetIsActive && statusBefore != target->status) {
    if (statusBefore == StatusCondition::Sleep) {
      battle.playerVolatile.sleepTurns = 0;
      battle.playerNightmare = false;
    }
    if (statusBefore == StatusCondition::Poison ||
        statusBefore == StatusCondition::BadlyPoisoned)
      battle.playerVolatile.toxicCounter = 0;
  }
  --*itemQuantity; ++battle.turn; result.accepted = true;
  battle.playerMoveEffects.furyCutterCount=0;
  battle.playerMoveEffects.destinyBond=false;
  battle.playerMoveEffects.grudge=false;
  BattleEvent& itemEvent = appendEvent(result, BattleEventType::ItemUsed, BattleSide::Player, target->uid);
  itemEvent.value = index;
  if (target->currentHp != hpBefore) {
    BattleEvent& hp = appendEvent(result, BattleEventType::HpChanged, BattleSide::Player, target->uid);
    hp.before=hpBefore;hp.after=target->currentHp;hp.value=static_cast<uint16_t>(target->currentHp-hpBefore);
  }
  appendStatusChange(result,BattleSide::Player,target->uid,statusBefore,target->status);
  appendStageChanges(result, BattleSide::Player, player->uid, stagesBefore, battle.playerVolatile);
  enemyTurn(battle, collection, *player, result);
  resolveNightmareTurn(battle, collection, result);
  resolveDelayedAttacks(battle, collection, result);
  if(battle.active){applyHeldEndTurnWithResult(result,*player,battle.playerVolatile,BattleSide::Player,&battle.playerMoveEffects);if(OwnedPokemon* opponent=currentOpponent(battle))applyHeldEndTurnWithResult(result,*opponent,battle.opponentVolatiles[battle.opponentIndex],BattleSide::Opponent,&battle.opponentMoveEffects[battle.opponentIndex]);advanceEndTurnEffects(battle,collection,result);}
  result.outcome = battle.outcome; if (!battle.active) restorePlayerBattleForm(battle, collection); appendBattleEnd(result, battle);
}

const char* BattleEngine::ballName(PokeBallType ball) {
  switch (ball) {
    case PokeBallType::PokeBall: return "POKE BALL";
    case PokeBallType::GreatBall: return "GREAT BALL";
    case PokeBallType::UltraBall: return "ULTRA BALL";
    case PokeBallType::MasterBall: return "MASTER BALL";
    case PokeBallType::Count: return "UNKNOWN";
  }
  return "UNKNOWN";
}

const char* BattleEngine::itemName(BattleItem item) {
  static const char* names[] = {"POTION","SUPER POTION","HYPER POTION","FULL HEAL","ANTIDOTE",
    "PARALYZE HEAL","AWAKENING","BURN HEAL","ICE HEAL","X ATTACK","X DEFENSE","X SPEED",
    "X ACCURACY","DIRE HIT","REVIVE","MAX REVIVE","ETHER","MAX ETHER","ELIXIR","MAX ELIXIR","PP UP",
    "RARE CANDY"};
  const uint8_t index = static_cast<uint8_t>(item);
  return index < static_cast<uint8_t>(BattleItem::Count) ? names[index] : "UNKNOWN";
}
