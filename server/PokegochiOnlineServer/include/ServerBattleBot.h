#pragma once

#include <cstddef>
#include <cstdint>

#include "game/Collection.h"
#include "services/MultiplayerProtocol.h"

struct CombatVolatile;
struct DedicatedMoveEffectState;
struct Gen4BattlerEffectState;
enum class BattleItem : uint8_t;

namespace PokegochiServerBot {

constexpr uint32_t kPlayerId = 99999U;
constexpr uint8_t kGenerationCount = 7U;
constexpr uint8_t kPokemonPerGeneration = 10U;
constexpr uint8_t kRosterSize = kGenerationCount * kPokemonPerGeneration;
constexpr uint8_t kTeamSize = 3U;
constexpr uint8_t kShinyRosterIndex = 61U;
constexpr uint8_t kNoMoveSlot = 0xFFU;

struct RosterEntry {
  uint16_t speciesId;
  uint16_t moves[kMoveSlots];
  uint8_t movePp[kMoveSlots];
  uint8_t generation;
  uint8_t baseHp;
  uint8_t abilityId;
  PokemonNature nature;
  HeldItem ordinaryItem;
  HeldItem zCrystal;
  uint8_t zMoveSlot;
  bool megaEligible;
  bool legendary;
  bool shiny;
  EffortValues evs;
};

struct Team {
  OwnedPokemon members[kTeamSize]{};
  uint8_t rosterIndexes[kTeamSize]{};
  uint8_t megaSlot = 0xFFU;
  uint8_t zSlot = 0xFFU;
  uint8_t zMoveSlot = 0xFFU;
};

// Complete, species-agnostic context for one online bot decision. The move
// evaluator consumes canonical move metadata and the authoritative battle
// snapshot; no Pokemon owns a hardcoded behavior branch.
struct MoveChoiceContext {
  const OwnedPokemon* opponent = nullptr;
  const CombatVolatile* userVolatile = nullptr;
  const CombatVolatile* opponentVolatile = nullptr;
  const DedicatedMoveEffectState* userMoveEffects = nullptr;
  const DedicatedMoveEffectState* opponentMoveEffects = nullptr;
  const Gen4BattlerEffectState* userGen4Effects = nullptr;
  uint8_t healthyReserves = 0U;
  uint8_t zMoveSlot = kNoMoveSlot;
  bool zMoveAvailable = false;
  uint32_t randomValue = 0U;
};

struct MoveChoice {
  uint8_t slot = kNoMoveSlot;
  bool useZMove = false;
  int16_t score = INT16_MIN;
};

// The server bot receives the exact same standardized PvP Bag as a human
// player. Keeping it here (rather than in the relay loop) makes item policy
// independently testable and prevents an accidental infinite inventory.
struct BattleBag {
  uint8_t hyperPotions =
      PokegochiMultiplayerProtocol::kBattleBagHyperPotionCount;
  uint8_t fullHeals = PokegochiMultiplayerProtocol::kBattleBagFullHealCount;
  uint8_t maxRevives =
      PokegochiMultiplayerProtocol::kBattleBagMaxReviveCount;
  uint8_t maxElixirs =
      PokegochiMultiplayerProtocol::kBattleBagMaxElixirCount;
};

struct ItemChoiceContext {
  const OwnedPokemon* team = nullptr;
  const CombatVolatile* volatiles = nullptr;
  const BattleBag* bag = nullptr;
  uint8_t teamCount = 0U;
  uint8_t activeSlot = 0U;
};

struct ItemChoice {
  BattleItem item = static_cast<BattleItem>(0U);
  uint8_t targetSlot = kNoMoveSlot;
  int16_t score = INT16_MIN;
  bool valid = false;
};

const RosterEntry* roster();
size_t rosterSize();

// Builds a disposable PvP team from the fixed 70-Pokemon server Box. A level
// of 70 is used for Balanced rules and 100 for Normal rules. The supplied seed
// makes both hardware tests and production selection reproducible.
Team buildTeam(uint32_t seed, uint8_t level, uint16_t battleSerial);

// Selects only contextually useful moves. In particular, recovery at full HP,
// duplicate major status, exhausted setup, unmet combo prerequisites and
// disabled/choice-locked moves are excluded before scoring.
MoveChoice chooseMove(const OwnedPokemon& pokemon,
                      const MoveChoiceContext& context);

// Evaluates only useful item actions and returns their score on the same
// scale as chooseMove(). Ties deliberately favor a move so the finite Bag is
// conserved. consumeItem() is called only after the command is transmitted.
ItemChoice chooseItem(const ItemChoiceContext& context);
bool consumeItem(BattleBag& bag, BattleItem item);

}  // namespace PokegochiServerBot
