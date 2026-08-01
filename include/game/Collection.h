#pragma once
#include <cstdint>
#include "game/PetState.h"
#include "game/PokemonData.h"

constexpr uint16_t kBoxCapacity = 151;
constexpr uint8_t kPartyCapacity = 3;
constexpr uint8_t kMoveSlots = 4;
constexpr uint32_t kEmptyPokemonUid = 0;

struct OwnedPokemon {
  uint32_t uid = kEmptyPokemonUid;
  uint16_t speciesId = 0;
  uint32_t experience = 0;
  uint16_t currentHp = 0;
  uint16_t maximumHp = 0;
  uint8_t level = 1;
  StatusCondition status = StatusCondition::None;
  uint8_t fullness = 80;
  uint8_t happiness = 80;
  uint32_t careRemainderSeconds = 0;
  uint32_t careTickCounter = 0;
  uint32_t recoverySecondsRemaining = 0;
  MoveId moves[kMoveSlots] = {MoveId::None, MoveId::None, MoveId::None, MoveId::None};
  uint8_t movePp[kMoveSlots]{};
  uint8_t abilityId = 0;
};

struct PokemonCollection {
  OwnedPokemon box[kBoxCapacity]{};
  uint32_t party[kPartyCapacity]{};
  uint32_t nextUid = 1;
};

class CollectionLogic {
 public:
  static void initialize(PokemonCollection& collection);
  static bool chooseStarter(PokemonCollection& collection, uint16_t speciesId);
  static OwnedPokemon createPokemon(uint32_t uid, uint16_t speciesId, uint8_t level);
  static OwnedPokemon* find(PokemonCollection& collection, uint32_t uid);
  static const OwnedPokemon* find(const PokemonCollection& collection, uint32_t uid);
  static OwnedPokemon* active(PokemonCollection& collection, uint8_t slot);
  static uint8_t count(const PokemonCollection& collection);
  static bool add(PokemonCollection& collection, OwnedPokemon pokemon, uint32_t* assignedUid = nullptr);
  static bool setPartySlot(PokemonCollection& collection, uint8_t slot, uint32_t uid);
  static bool isInParty(const PokemonCollection& collection, uint32_t uid);
  static bool validate(const PokemonCollection& collection);
  static void advanceCare(OwnedPokemon& pokemon, uint32_t elapsedSeconds);
  static void care(OwnedPokemon& pokemon, CareAction action);
};
