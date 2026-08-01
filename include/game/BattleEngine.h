#pragma once
#include <cstdint>
#include "game/Collection.h"

enum class PokeBallType : uint8_t { PokeBall, GreatBall, UltraBall, MasterBall, Count };
enum class BattleItem : uint8_t {
  Potion, SuperPotion, HyperPotion, FullHeal, Antidote, ParalyzeHeal,
  Awakening, BurnHeal, IceHeal, XAttack, XDefense, XSpeed, XAccuracy,
  DireHit, Count
};

struct Inventory {
  uint16_t balls[static_cast<uint8_t>(PokeBallType::Count)] = {10, 3, 1, 0};
  uint16_t medicine[16]{};
};

struct EncounterCharges {
  static constexpr uint8_t kMaximum = 3;
  static constexpr uint32_t kRechargeSeconds = 3U * 60U * 60U;
  uint8_t available = kMaximum;
  uint32_t rechargeProgressSeconds = 0;
};

struct WildEncounterClock {
  static constexpr uint32_t kMinimumSeconds = 60U * 60U;
  static constexpr uint32_t kWindowSeconds = 2U * 60U * 60U;
  uint32_t elapsedSeconds = 0;
  uint32_t targetSeconds = kMinimumSeconds;
  uint32_t rngState = 0x91E10DA5U;
  bool pending = false;
};

enum class BattleOutcome : uint8_t { None, Ongoing, Victory, Defeat, Escaped, Captured };
enum class BattleKind : uint8_t { None, Wild, Trainer, Gym, Pvp };

constexpr uint8_t kOpponentTeamCapacity = 3;

struct CombatVolatile {
  int8_t attackStage = 0, defenseStage = 0, spAttackStage = 0, spDefenseStage = 0;
  int8_t speedStage = 0, accuracyStage = 0, evasionStage = 0;
  uint8_t confusionTurns = 0, trappedTurns = 0;
  uint8_t criticalStage = 0;
  bool flinched = false, protectedThisTurn = false, recharging = false;
  uint16_t substituteHp = 0;
  MoveId disabledMove = MoveId::None, encoreMove = MoveId::None, chargingMove = MoveId::None;
};

struct BattleState {
  bool active = false;
  BattleKind kind = BattleKind::None;
  BattleOutcome outcome = BattleOutcome::None;
  uint32_t playerUid = 0;
  OwnedPokemon opponents[kOpponentTeamCapacity]{};
  uint8_t opponentCount = 0;
  uint8_t opponentIndex = 0;
  uint8_t trainerProfileId = 0xFF;
  uint8_t gymId = 0xFF;
  CombatVolatile playerVolatile{};
  CombatVolatile opponentVolatiles[kOpponentTeamCapacity]{};
  uint32_t rngState = 0xA341316CU;
  uint16_t turn = 0;
  uint32_t rewardMoney = 0;
};

struct BattleActionResult {
  bool accepted = false;
  bool hit = false;
  bool enemyActed = false;
  bool caught = false;
  bool opponentDefeated = false;
  bool evolved = false;
  uint16_t evolvedSpeciesId = 0;
  uint8_t evolvedCount = 0;
  uint16_t evolvedSpeciesIds[kPartyCapacity]{};
  uint16_t evolvedFromSpeciesIds[kPartyCapacity]{};
  bool statusApplied = false;
  StatusCondition appliedStatus = StatusCondition::None;
  uint16_t damageDealt = 0;
  uint16_t damageTaken = 0;
  uint16_t experienceGained = 0;
  uint32_t moneyGained = 0;
  BattleOutcome outcome = BattleOutcome::None;
};

class EncounterLogic {
 public:
  static void advance(EncounterCharges& charges, uint32_t elapsedSeconds);
  static bool consumeManualCharge(EncounterCharges& charges);
  static uint32_t secondsUntilNext(const EncounterCharges& charges);
  static void advanceWild(WildEncounterClock& clock, uint32_t elapsedSeconds);
  static void acknowledgeWild(WildEncounterClock& clock);
};

class BattleEngine {
 public:
  static void clear(BattleState& battle);
  static uint8_t highestPartyLevel(const PokemonCollection& collection);
  static bool startWild(BattleState& battle, PokemonCollection& collection,
                        uint32_t playerUid, uint32_t seed);
  static bool startTrainer(BattleState& battle, EncounterCharges& charges,
                           PokemonCollection& collection, uint32_t playerUid, uint32_t seed);
  static OwnedPokemon* currentOpponent(BattleState& battle);
  static const OwnedPokemon* currentOpponent(const BattleState& battle);
  static BattleActionResult fight(BattleState& battle, PokemonCollection& collection, uint8_t moveSlot);
  static BattleActionResult run(BattleState& battle, PokemonCollection& collection);
  static BattleActionResult switchPokemon(BattleState& battle, PokemonCollection& collection);
  static BattleActionResult switchToPokemon(BattleState& battle, PokemonCollection& collection, uint32_t uid);
  static BattleActionResult throwBall(BattleState& battle, PokemonCollection& collection,
                                      Inventory& inventory, PokeBallType ball);
  static BattleActionResult useItem(BattleState& battle, PokemonCollection& collection,
                                    Inventory& inventory, BattleItem item);
  static const char* ballName(PokeBallType ball);
  static const char* itemName(BattleItem item);

 private:
  static uint32_t random(BattleState& battle);
  static uint16_t calculateDamage(BattleState& battle, const OwnedPokemon& attacker,
                                  const OwnedPokemon& defender, MoveId move,
                                  const CombatVolatile& attackerVolatile,
                                  const CombatVolatile& defenderVolatile);
  static void enemyTurn(BattleState& battle, PokemonCollection& collection,
                        OwnedPokemon& player, BattleActionResult& result, uint8_t moveSlot = 0xFF);
  static void awardExperience(BattleState& battle, PokemonCollection& collection, BattleActionResult& result);
  static void applyMoveStatus(BattleState& battle, const OwnedPokemon& source, MoveId move, OwnedPokemon& target,
                              BattleActionResult& result);
};
