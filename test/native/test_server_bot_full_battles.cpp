#include <cassert>
#include <cstdint>
#include <iostream>

#include "ServerBattleBot.h"
#include "game/BattleEngine.h"

namespace {

using PokegochiServerBot::BattleBag;
using PokegochiServerBot::ItemChoice;
using PokegochiServerBot::ItemChoiceContext;
using PokegochiServerBot::MoveChoice;
using PokegochiServerBot::MoveChoiceContext;
using PokegochiServerBot::Team;

struct SideChoice {
  MoveChoice move{};
  ItemChoice item{};
  bool usesItem = false;
};

uint32_t nextRandom(uint32_t& state) {
  if (!state) state = 0x91E10DA5U;
  state ^= state << 13U;
  state ^= state >> 17U;
  state ^= state << 5U;
  return state;
}

uint8_t healthyPlayerReserves(const PokemonCollection& collection,
                              uint32_t activeUid) {
  uint8_t count = 0U;
  for (uint8_t slot = 0U; slot < kPartyCapacity; ++slot) {
    const OwnedPokemon* pokemon = CollectionLogic::active(collection, slot);
    if (pokemon && pokemon->uid != activeUid && pokemon->currentHp)
      ++count;
  }
  return count;
}

uint8_t healthyOpponentReserves(const BattleState& battle) {
  uint8_t count = 0U;
  for (uint8_t slot = 0U; slot < battle.opponentCount; ++slot)
    if (slot != battle.opponentIndex && battle.opponents[slot].currentHp)
      ++count;
  return count;
}

uint32_t firstHealthyPlayerReplacement(const PokemonCollection& collection,
                                       uint32_t activeUid) {
  for (uint8_t slot = 0U; slot < kPartyCapacity; ++slot) {
    const OwnedPokemon* pokemon = CollectionLogic::active(collection, slot);
    if (pokemon && pokemon->uid != activeUid && pokemon->currentHp &&
        !pokemon->recoverySecondsRemaining)
      return pokemon->uid;
  }
  return 0U;
}

int8_t firstHealthyOpponentReplacement(const BattleState& battle) {
  for (uint8_t slot = 0U; slot < battle.opponentCount; ++slot)
    if (slot != battle.opponentIndex && battle.opponents[slot].currentHp)
      return static_cast<int8_t>(slot);
  return -1;
}

void assertHealthInvariants(const BattleState& battle,
                            const PokemonCollection& collection) {
  for (uint8_t slot = 0U; slot < kPartyCapacity; ++slot) {
    const OwnedPokemon* pokemon = CollectionLogic::active(collection, slot);
    if (pokemon) assert(pokemon->currentHp <= pokemon->maximumHp);
  }
  for (uint8_t slot = 0U; slot < battle.opponentCount; ++slot)
    assert(battle.opponents[slot].currentHp <=
           battle.opponents[slot].maximumHp);
}

SideChoice choosePlayerAction(const BattleState& battle,
                              const PokemonCollection& collection,
                              uint32_t zUid, uint8_t zMoveSlot,
                              uint32_t randomValue) {
  SideChoice action{};
  const OwnedPokemon* player =
      CollectionLogic::find(collection, battle.playerUid);
  const OwnedPokemon* opponent = BattleEngine::currentOpponent(battle);
  assert(player && opponent);

  MoveChoiceContext context{};
  context.opponent = opponent;
  context.userVolatile = &battle.playerVolatile;
  context.opponentVolatile =
      &battle.opponentVolatiles[battle.opponentIndex];
  context.userMoveEffects = &battle.playerMoveEffects;
  context.opponentMoveEffects =
      &battle.opponentMoveEffects[battle.opponentIndex];
  context.userGen4Effects = &battle.playerGen4Effects;
  context.healthyReserves = healthyPlayerReserves(collection, player->uid);
  context.zMoveAvailable = !battle.playerZMoveUsed && player->uid == zUid;
  context.zMoveSlot = zMoveSlot;
  context.randomValue = randomValue;
  action.move = PokegochiServerBot::chooseMove(*player, context);
  return action;
}

SideChoice chooseOpponentAction(const BattleState& battle,
                                const PokemonCollection& collection,
                                const BattleBag& bag, uint32_t zUid,
                                uint8_t zMoveSlot, uint32_t randomValue) {
  SideChoice action{};
  const OwnedPokemon* player =
      CollectionLogic::find(collection, battle.playerUid);
  const OwnedPokemon* opponent = BattleEngine::currentOpponent(battle);
  assert(player && opponent);

  MoveChoiceContext moveContext{};
  moveContext.opponent = player;
  moveContext.userVolatile =
      &battle.opponentVolatiles[battle.opponentIndex];
  moveContext.opponentVolatile = &battle.playerVolatile;
  moveContext.userMoveEffects =
      &battle.opponentMoveEffects[battle.opponentIndex];
  moveContext.opponentMoveEffects = &battle.playerMoveEffects;
  moveContext.userGen4Effects =
      &battle.opponentGen4Effects[battle.opponentIndex];
  moveContext.healthyReserves = healthyOpponentReserves(battle);
  moveContext.zMoveAvailable = !battle.opponentZMoveUsed &&
      opponent->uid == zUid;
  moveContext.zMoveSlot = zMoveSlot;
  moveContext.randomValue = randomValue;
  action.move = PokegochiServerBot::chooseMove(*opponent, moveContext);

  ItemChoiceContext itemContext{};
  itemContext.team = battle.opponents;
  itemContext.volatiles = battle.opponentVolatiles;
  itemContext.bag = &bag;
  itemContext.teamCount = battle.opponentCount;
  itemContext.activeSlot = battle.opponentIndex;
  action.item = PokegochiServerBot::chooseItem(itemContext);
  action.usesItem = action.item.valid &&
                    action.item.score > action.move.score;
  return action;
}

void initializeBotInventory(Inventory& inventory, PpItemInventory& ppItems) {
  inventory = Inventory{};
  ppItems = PpItemInventory{};
  inventory.medicine[static_cast<uint8_t>(BattleItem::HyperPotion)] =
      PokegochiMultiplayerProtocol::kBattleBagHyperPotionCount;
  inventory.medicine[static_cast<uint8_t>(BattleItem::FullHeal)] =
      PokegochiMultiplayerProtocol::kBattleBagFullHealCount;
  inventory.medicine[static_cast<uint8_t>(BattleItem::MaxRevive)] =
      PokegochiMultiplayerProtocol::kBattleBagMaxReviveCount;
  ppItems.quantities[ppInventoryIndex(BattleItem::MaxElixir)] =
      PokegochiMultiplayerProtocol::kBattleBagMaxElixirCount;
}

void initializeBattle(BattleState& battle, PokemonCollection& collection,
                      const Team& playerTeam, const Team& opponentTeam,
                      uint32_t seed, uint32_t& playerZUid,
                      uint32_t& opponentZUid) {
  CollectionLogic::initialize(collection);
  playerZUid = 0U;
  for (uint8_t slot = 0U; slot < PokegochiServerBot::kTeamSize; ++slot) {
    uint32_t uid = 0U;
    assert(CollectionLogic::add(collection, playerTeam.members[slot], &uid));
    assert(CollectionLogic::setPartySlot(collection, slot, uid));
    if (slot == playerTeam.zSlot) playerZUid = uid;
  }

  BattleEngine::clear(battle);
  battle.active = true;
  battle.kind = BattleKind::Pvp;
  battle.outcome = BattleOutcome::Ongoing;
  battle.playerUid = collection.party[0];
  battle.opponentCount = PokegochiServerBot::kTeamSize;
  battle.opponentIndex = 0U;
  battle.rngState = seed;
  battle.unlockedGeneration = 7U;
  opponentZUid = 0U;
  for (uint8_t slot = 0U; slot < PokegochiServerBot::kTeamSize; ++slot) {
    battle.opponents[slot] = opponentTeam.members[slot];
    if (slot == opponentTeam.zSlot)
      opponentZUid = battle.opponents[slot].uid;
  }
  BattleEngine::ensureOpponentUids(battle);
  BattleEngine::applyEntryAbilities(battle, collection);
}

bool resolveReplacementBarrier(BattleState& battle,
                               PokemonCollection& collection) {
  const uint8_t playerReason =
      static_cast<uint8_t>(battle.pendingPlayerReplacement & 0x0FU);
  const uint8_t opponentReason =
      static_cast<uint8_t>((battle.pendingPlayerReplacement >> 4U) & 0x0FU);
  OwnedPokemon* player =
      CollectionLogic::find(collection, battle.playerUid);
  OwnedPokemon* opponent = BattleEngine::currentOpponent(battle);
  if (!player || !opponent) return false;

  const bool playerFainted = !player->currentHp;
  const bool opponentFainted = !opponent->currentHp;
  bool changed = false;

  if (opponentFainted || opponentReason) {
    const int8_t replacement = firstHealthyOpponentReplacement(battle);
    if (replacement >= 0) {
      BattleActionResult switched = BattleEngine::switchOpponentPvp(
          battle, collection, static_cast<uint8_t>(replacement), true);
      assert(switched.accepted);
      changed = true;
      if (!opponentFainted && opponentReason) {
        BattleActionResult completed =
            BattleEngine::completeOpponentPivotPvp(
                battle, collection, battle.pendingOpponentMoveSlot,
                opponentReason);
        assert(completed.accepted);
      }
    }
  }

  player = CollectionLogic::find(collection, battle.playerUid);
  if (player && (!player->currentHp || playerReason)) {
    const uint32_t replacement =
        firstHealthyPlayerReplacement(collection, player->uid);
    if (replacement) {
      BattleActionResult switched = playerReason && player->currentHp
          ? BattleEngine::completeBatonPassSwitch(
                battle, collection, replacement,
                battle.pendingOpponentMoveSlot, playerReason)
          : BattleEngine::switchToPokemonPvp(
                battle, collection, replacement, 0xFEU, true);
      assert(switched.accepted);
      changed = true;
    }
  }
  return changed;
}

void runBattle(uint8_t battleNumber, uint32_t seed) {
  const Team playerTeam = PokegochiServerBot::buildTeam(
      seed ^ 0xA5A5A5A5U, 70U,
      static_cast<uint16_t>(0x400U + battleNumber));
  const Team opponentTeam = PokegochiServerBot::buildTeam(
      seed ^ 0x5A5A5A5AU, 70U,
      static_cast<uint16_t>(0x500U + battleNumber));

  PokemonCollection collection{};
  BattleState battle{};
  uint32_t playerZUid = 0U;
  uint32_t opponentZUid = 0U;
  initializeBattle(battle, collection, playerTeam, opponentTeam, seed,
                   playerZUid, opponentZUid);

  BattleBag opponentBag{};
  Inventory opponentInventory{};
  PpItemInventory opponentPpItems{};
  initializeBotInventory(opponentInventory, opponentPpItems);
  uint32_t decisionState = seed ^ 0xC001D00DU;
  uint16_t resolvedCommands = 0U;
  uint16_t replacementBarriers = 0U;
  uint16_t opponentItems = 0U;
  uint16_t struggleCommands = 0U;

  while (battle.active && resolvedCommands < 1000U) {
    if (resolveReplacementBarrier(battle, collection)) {
      ++replacementBarriers;
      assertHealthInvariants(battle, collection);
      continue;
    }

    const OwnedPokemon* player =
        CollectionLogic::find(collection, battle.playerUid);
    const OwnedPokemon* opponent = BattleEngine::currentOpponent(battle);
    assert(player && opponent && player->currentHp && opponent->currentHp);

    const SideChoice playerAction = choosePlayerAction(
        battle, collection, playerZUid, playerTeam.zMoveSlot,
        nextRandom(decisionState));
    SideChoice opponentAction = chooseOpponentAction(
        battle, collection, opponentBag, opponentZUid,
        opponentTeam.zMoveSlot, nextRandom(decisionState));
    assert(playerAction.move.slot != PokegochiServerBot::kNoMoveSlot);
    assert(opponentAction.usesItem ||
           opponentAction.move.slot != PokegochiServerBot::kNoMoveSlot);

    if (playerAction.move.slot == kMoveSlots ||
        (!opponentAction.usesItem && opponentAction.move.slot == kMoveSlots))
      ++struggleCommands;

    if (playerAction.move.useZMove) {
      assert(playerAction.move.slot < kMoveSlots);
      assert(BattleEngine::armPlayerZMove(
          battle, collection, player->moves[playerAction.move.slot]));
    }

    BattleActionResult result{};
    if (opponentAction.usesItem) {
      BattleActionResult itemResult = BattleEngine::useOpponentItemPvp(
          battle, opponentInventory, opponentPpItems,
          opponentAction.item.item, opponentAction.item.targetSlot);
      assert(itemResult.accepted);
      assert(PokegochiServerBot::consumeItem(
          opponentBag, opponentAction.item.item));
      ++opponentItems;
      result = BattleEngine::fightPvp(
          battle, collection, playerAction.move.slot, 0xFEU, false);
    } else {
      result = BattleEngine::fightPvp(
          battle, collection, playerAction.move.slot,
          opponentAction.move.slot, opponentAction.move.useZMove);
    }
    assert(result.accepted);
    assert(result.eventCount <= kBattleEventCapacity);
    ++resolvedCommands;
    assertHealthInvariants(battle, collection);
  }

  assert(!battle.active);
  assert(battle.outcome == BattleOutcome::Victory ||
         battle.outcome == BattleOutcome::Defeat);
  assert(resolvedCommands < 1000U);
  std::cout << "BATTLE " << static_cast<unsigned>(battleNumber)
            << " outcome=" << static_cast<unsigned>(battle.outcome)
            << " commands=" << resolvedCommands
            << " replacements=" << replacementBarriers
            << " bot-items=" << opponentItems
            << " struggle=" << struggleCommands << '\n';
}

}  // namespace

int main() {
  runBattle(1U, 0x39100001U);
  runBattle(2U, 0x39100002U);
  runBattle(3U, 0x39100003U);
  std::cout << "PASS: three complete BattleEngine battles against the server bot\n";
  return 0;
}
