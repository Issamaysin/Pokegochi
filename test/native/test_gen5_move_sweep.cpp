#include <cassert>
#include <cstdint>
#include <iostream>

#include "game/BattleEngine.h"
#include "game/Collection.h"
#include "game/PokemonData.h"

namespace {

constexpr uint16_t kFirstGen5Move = 468U;
constexpr uint16_t kLastGen5Move = 559U;

struct Fixture {
  PokemonCollection collection{};
  BattleState battle{};

  Fixture(MoveId move, bool enemy) {
    CollectionLogic::initialize(collection);
    assert(CollectionLogic::chooseStarter(collection, 1U, 0x51000001U));
    OwnedPokemon* player = CollectionLogic::active(collection, 0U);
    assert(player);
    const uint32_t playerUid = player->uid;
    *player = CollectionLogic::createPokemon(
        playerUid, 151U, 100U, false, 0x51000002U);
    player->maximumHp = player->currentHp = 30000U;
    player->abilityId = 1U;  // STENCH: valid and harmless for this sweep.
    player->moves[0] = enemy ? static_cast<MoveId>(150U) : move;  // SPLASH
    player->movePp[0] = findFullMove(player->moves[0])->pp;

    BattleEngine::clear(battle);
    battle.active = true;
    battle.kind = BattleKind::Pvp;
    battle.outcome = BattleOutcome::Ongoing;
    battle.playerUid = playerUid;
    battle.opponentCount = 1U;
    battle.unlockedGeneration = 5U;
    battle.rngState = 0x51000003U;
    battle.opponents[0] = CollectionLogic::createPokemon(
        0xF1000001U, 151U, 100U, false, 0x51000004U);
    OwnedPokemon& opponent = battle.opponents[0];
    opponent.maximumHp = opponent.currentHp = 30000U;
    opponent.abilityId = 1U;
    opponent.moves[0] = enemy ? move : static_cast<MoveId>(150U);
    opponent.movePp[0] = findFullMove(opponent.moves[0])->pp;

    CombatVolatile& userState = enemy ? battle.opponentVolatiles[0]
                                      : battle.playerVolatile;
    CombatVolatile& targetState = enemy ? battle.playerVolatile
                                        : battle.opponentVolatiles[0];
    userState.accuracyStage = kMaximumBattleStatStage;
    targetState.evasionStage = kMinimumBattleStatStage;

    OwnedPokemon& user = enemy ? opponent : *player;
    OwnedPokemon& target = enemy ? *player : opponent;
    // Make the audited user naturally faster without occupying Speed stages;
    // AUTOTOMIZE/SHIFT GEAR must still have room to apply their own boosts.
    user.level = 100U;
    target.level = 1U;
    if (static_cast<uint16_t>(move) == 505U)  // HEAL PULSE
      target.currentHp = 10000U;
    if (static_cast<uint16_t>(move) == 516U) {  // BESTOW
      user.heldItem = HeldItem::OranBerry;
      target.heldItem = HeldItem::None;
    }
  }

  BattleActionResult use(bool enemy) {
    return BattleEngine::fightPvp(
        battle, collection, 0U, enemy ? 0U : 0xFEU);
  }

  OwnedPokemon& player() {
    OwnedPokemon* pokemon = CollectionLogic::find(collection, battle.playerUid);
    assert(pokemon);
    return *pokemon;
  }

  OwnedPokemon& target(bool enemy) {
    return enemy ? player() : battle.opponents[0];
  }

  CombatVolatile& userVolatile(bool enemy) {
    return enemy ? battle.opponentVolatiles[0] : battle.playerVolatile;
  }
};

bool sawMove(const BattleActionResult& result, MoveId move, BattleSide side) {
  for (uint8_t index = 0; index < result.eventCount; ++index) {
    const BattleEvent& event = result.events[index];
    if (event.type == BattleEventType::MoveUsed && event.side == side &&
        event.move == move)
      return true;
  }
  return false;
}

bool sawFailure(const BattleActionResult& result, MoveId move) {
  for (uint8_t index = 0; index < result.eventCount; ++index) {
    const BattleEvent& event = result.events[index];
    if (event.type == BattleEventType::MoveEffect && event.move == move &&
        event.value == static_cast<uint16_t>(BattleMoveEffect::Failed))
      return true;
  }
  return false;
}

bool sawMiss(const BattleActionResult& result, MoveId move) {
  for (uint8_t index = 0; index < result.eventCount; ++index) {
    const BattleEvent& event = result.events[index];
    if (event.type == BattleEventType::MoveMissed && event.move == move)
      return true;
  }
  return false;
}

bool sawEffect(const BattleActionResult& result, MoveId move,
               BattleMoveEffect effect) {
  for (uint8_t index = 0; index < result.eventCount; ++index) {
    const BattleEvent& event = result.events[index];
    if (event.type == BattleEventType::MoveEffect && event.move == move &&
        event.value == static_cast<uint16_t>(effect))
      return true;
  }
  return false;
}

bool sawHpChange(const BattleActionResult& result, BattleSide side) {
  for (uint8_t index = 0; index < result.eventCount; ++index) {
    const BattleEvent& event = result.events[index];
    if (event.type == BattleEventType::HpChanged && event.side == side)
      return true;
  }
  return false;
}

bool isUnavailableInSingles(MoveId move) {
  const uint16_t id = static_cast<uint16_t>(move);
  // RAGE POWDER and ALLY SWITCH require an ally. HEAL PULSE targets another
  // battler and would only heal the opponent in Pokegochi's strict 1x1 mode.
  return id == 476U || id == 502U || id == 505U;
}

}  // namespace

int main() {
  uint16_t auditedMoves = 0U;
  uint16_t checkedSides = 0U;
  uint16_t singlesExclusions = 0U;
  for (uint16_t raw = kFirstGen5Move; raw <= kLastGen5Move; ++raw) {
    const MoveId move = static_cast<MoveId>(raw);
    const FullMoveData* data = findFullMove(move);
    assert(data && data->id == raw);
    ++auditedMoves;
    if (isUnavailableInSingles(move)) {
      assert(!isMoveAvailableInSingleBattle(move));
      ++singlesExclusions;
      std::cout << raw << " " << data->name << " SINGLES-EXCLUDED\n";
      continue;
    }
    assert(isMoveAvailableInSingleBattle(move));
    for (bool enemy : {false, true}) {
      Fixture fixture(move, enemy);
      const uint16_t hpBefore = fixture.target(enemy).currentHp;
      const BattleActionResult result = fixture.use(enemy);
      const BattleSide side = enemy ? BattleSide::Opponent : BattleSide::Player;
      const bool failed = sawFailure(result, move);
      if (failed)
        std::cerr << "Unexpected failure: " << raw << " " << data->name
                  << " side=" << (enemy ? "foe" : "player") << "\n";
      assert(result.accepted);
      assert(sawMove(result, move, side));
      assert(!sawMiss(result, move));
      assert(!failed);

      if (data->power != 0U) {
        const bool charges = raw == 507U || raw == 553U || raw == 554U;
        if (charges) {
          assert(fixture.userVolatile(enemy).chargingMove == move);
          const BattleActionResult release = fixture.use(enemy);
          assert(release.accepted);
          assert(sawMove(release, move, side));
          assert(!sawMiss(release, move));
          assert(!sawFailure(release, move));
        }
        if (fixture.target(enemy).currentHp >= hpBefore)
          std::cerr << "No damage: " << raw << " " << data->name
                    << " side=" << (enemy ? "foe" : "player") << "\n";
        assert(fixture.target(enemy).currentHp < hpBefore);
      }

      if (raw == 495U || raw == 511U)
        assert(sawEffect(result, move, BattleMoveEffect::TurnOrderAdjusted));
      ++checkedSides;
    }
    std::cout << raw << " " << data->name << " OK\n";
  }
  assert(auditedMoves == 92U);
  assert(checkedSides == 178U);
  assert(singlesExclusions == 3U);

  // Black/White encodes accuracy "--" as 101. These moves must bypass the
  // ordinary Accuracy/Evasion roll instead of being treated as literal 101%
  // moves. Seed 1 misses at the forced -6/+6 stages if that sentinel leaks
  // into the percentage pipeline, so exercise every imported row both ways.
  uint16_t noAccuracyCheckMoves = 0U;
  for (uint16_t raw = kFirstGen5Move; raw <= kLastGen5Move; ++raw) {
    const MoveId move = static_cast<MoveId>(raw);
    const FullMoveData* data = findFullMove(move);
    assert(data);
    if (data->accuracy != 101U) continue;
    ++noAccuracyCheckMoves;
    for (bool enemy : {false, true}) {
      Fixture fixture(move, enemy);
      CombatVolatile& userState = enemy ? fixture.battle.opponentVolatiles[0]
                                        : fixture.battle.playerVolatile;
      CombatVolatile& targetState = enemy ? fixture.battle.playerVolatile
                                          : fixture.battle.opponentVolatiles[0];
      userState.accuracyStage = kMinimumBattleStatStage;
      targetState.evasionStage = kMaximumBattleStatStage;
      fixture.battle.rngState = 1U;
      const BattleActionResult result = fixture.use(enemy);
      assert(result.accepted);
      assert(sawMove(result, move,
          enemy ? BattleSide::Opponent : BattleSide::Player));
      assert(!sawMiss(result, move));
      if (raw == 476U || raw == 502U) assert(sawFailure(result, move));
      else assert(!sawFailure(result, move));
    }
  }
  assert(noAccuracyCheckMoves == 22U);

  // WONDER ROOM was mechanically active but its unhandled presentation event
  // fell through to "But it failed!". Pin both activation and cancellation.
  for (bool enemy : {false, true}) {
    const MoveId wonderRoom = static_cast<MoveId>(472U);
    Fixture fixture(wonderRoom, enemy);
    const BattleActionResult started = fixture.use(enemy);
    assert(started.accepted && fixture.battle.wonderRoomTurns != 0U);
    assert(sawEffect(started, wonderRoom, BattleMoveEffect::WonderRoomSet));
    const BattleActionResult ended = fixture.use(enemy);
    assert(ended.accepted && fixture.battle.wonderRoomTurns == 0U);
    assert(sawEffect(ended, wonderRoom, BattleMoveEffect::WonderRoomEnded));
  }

  // Imported/old saves may still contain the two double-battle-only moves.
  // The normal save repair removes only those slots and keeps usable moves.
  OwnedPokemon stale = CollectionLogic::createPokemon(
      0xF1000010U, 151U, 50U, false, 0x51000010U);
  stale.moves[0] = static_cast<MoveId>(476U);
  stale.moves[1] = static_cast<MoveId>(502U);
  stale.moves[2] = static_cast<MoveId>(505U);
  stale.moves[3] = static_cast<MoveId>(495U);
  stale.movePp[0] = stale.movePp[1] = stale.movePp[2] = stale.movePp[3] = 10U;
  assert(CollectionLogic::ensureUsableMoves(stale));
  assert(stale.moves[0] == MoveId::None && stale.movePp[0] == 0U);
  assert(stale.moves[1] == MoveId::None && stale.movePp[1] == 0U);
  assert(stale.moves[2] == MoveId::None && stale.movePp[2] == 0U);
  assert(stale.moves[3] == static_cast<MoveId>(495U));

  // Keep the canonical imported handler covered even though normal 1x1
  // acquisition and AI can no longer select HEAL PULSE.
  for (bool enemy : {false, true}) {
    const MoveId healPulse = static_cast<MoveId>(505U);
    Fixture fixture(healPulse, enemy);
    const uint16_t hpBefore = fixture.target(enemy).currentHp;
    const BattleActionResult healed = fixture.use(enemy);
    const BattleSide userSide = enemy ? BattleSide::Opponent : BattleSide::Player;
    const BattleSide targetSide = enemy ? BattleSide::Player : BattleSide::Opponent;
    assert(healed.accepted && sawMove(healed, healPulse, userSide));
    assert(!sawFailure(healed, healPulse));
    assert(fixture.target(enemy).currentHp > hpBefore);
    assert(sawHpChange(healed, targetSide));
  }

  // AFTER YOU and QUASH are valid in singles if the target has not acted;
  // unlike RAGE POWDER/ALLY SWITCH, they must not be unconditional failures.
  // Once the target has already taken its action, both correctly fail.
  for (uint16_t raw : {495U, 511U}) {
    const MoveId move = static_cast<MoveId>(raw);
    for (bool enemy : {false, true}) {
      Fixture fixture(move, enemy);
      OwnedPokemon* player = CollectionLogic::find(
          fixture.collection, fixture.battle.playerUid);
      assert(player);
      OwnedPokemon& user = enemy ? fixture.battle.opponents[0] : *player;
      OwnedPokemon& target = enemy ? *player : fixture.battle.opponents[0];
      user.level = 1U;
      target.level = 100U;
      const BattleActionResult late = BattleEngine::fightPvp(
          fixture.battle, fixture.collection, 0U, 0U);
      assert(late.accepted && sawMove(late, move,
          enemy ? BattleSide::Opponent : BattleSide::Player));
      assert(sawFailure(late, move));
    }
  }
  std::cout << "Gen-V move sweep passed: 92/92 audited; 89 usable moves "
               "executed on 178/178 sides; Rage Powder, Ally Switch and "
               "Heal Pulse correctly excluded from singles.\n";
}
