#include "game/Collection.h"

namespace {
uint16_t calculateHp(const SpeciesData& species, uint8_t level) {
  return static_cast<uint16_t>(((2U * species.baseHp * level) / 100U) + level + 10U);
}
}

OwnedPokemon CollectionLogic::createPokemon(uint32_t uid, uint16_t speciesId, uint8_t level) {
  OwnedPokemon pokemon;
  const SpeciesData* species = findSpecies(speciesId);
  if (!species) return pokemon;
  pokemon.uid = uid;
  pokemon.speciesId = speciesId;
  pokemon.level = level;
  pokemon.experience = experienceForLevel(species->growthRate, level);
  pokemon.maximumHp = calculateHp(*species, level);
  pokemon.currentHp = pokemon.maximumHp;
  pokemon.abilityId = species->ability2 && ((uid + speciesId + level) & 1U) ? species->ability2 : species->ability1;
  const uint8_t moveCount = movesForLevel(speciesId, level, pokemon.moves);
  if (!moveCount) pokemon.moves[0] = defaultMoveForSpecies(speciesId);
  for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
    const FullMoveData* move = findFullMove(pokemon.moves[slot]);
    pokemon.movePp[slot] = move ? move->pp : 0;
  }
  return pokemon;
}

void CollectionLogic::initialize(PokemonCollection& collection) {
  collection = PokemonCollection{};
}

bool CollectionLogic::chooseStarter(PokemonCollection& collection, uint16_t speciesId) {
  if (count(collection) != 0 || (speciesId != 1 && speciesId != 4 && speciesId != 7)) return false;
  collection.box[0] = createPokemon(collection.nextUid++, speciesId, 5);
  collection.party[0] = collection.box[0].uid;
  return true;
}

OwnedPokemon* CollectionLogic::find(PokemonCollection& collection, uint32_t uid) {
  if (uid == kEmptyPokemonUid) return nullptr;
  for (auto& pokemon : collection.box) if (pokemon.uid == uid) return &pokemon;
  return nullptr;
}

const OwnedPokemon* CollectionLogic::find(const PokemonCollection& collection, uint32_t uid) {
  if (uid == kEmptyPokemonUid) return nullptr;
  for (const auto& pokemon : collection.box) if (pokemon.uid == uid) return &pokemon;
  return nullptr;
}

OwnedPokemon* CollectionLogic::active(PokemonCollection& collection, uint8_t slot) {
  return slot < kPartyCapacity ? find(collection, collection.party[slot]) : nullptr;
}

uint8_t CollectionLogic::count(const PokemonCollection& collection) {
  uint8_t total = 0;
  for (const auto& pokemon : collection.box) if (pokemon.uid != kEmptyPokemonUid) ++total;
  return total;
}

bool CollectionLogic::add(PokemonCollection& collection, OwnedPokemon pokemon, uint32_t* assignedUid) {
  for (auto& slot : collection.box) {
    if (slot.uid != kEmptyPokemonUid) continue;
    pokemon.uid = collection.nextUid++;
    if (pokemon.uid == kEmptyPokemonUid) pokemon.uid = collection.nextUid++;
    slot = pokemon;
    if (assignedUid) *assignedUid = pokemon.uid;
    return true;
  }
  return false;
}

bool CollectionLogic::isInParty(const PokemonCollection& collection, uint32_t uid) {
  for (uint32_t member : collection.party) if (member == uid && uid != kEmptyPokemonUid) return true;
  return false;
}

bool CollectionLogic::setPartySlot(PokemonCollection& collection, uint8_t slot, uint32_t uid) {
  if (slot >= kPartyCapacity || !find(collection, uid)) return false;
  for (uint8_t i = 0; i < kPartyCapacity; ++i) {
    if (i != slot && collection.party[i] == uid) {
      const uint32_t replaced = collection.party[slot];
      collection.party[slot] = uid;
      collection.party[i] = replaced;
      return true;
    }
  }
  collection.party[slot] = uid;
  return true;
}

bool CollectionLogic::removeFromParty(PokemonCollection& collection, uint32_t uid) {
  uint8_t count=0,found=kPartyCapacity;for(uint8_t i=0;i<kPartyCapacity;++i){if(collection.party[i])++count;if(collection.party[i]==uid)found=i;}
  if(count<=1||found>=kPartyCapacity)return false;
  for(uint8_t i=found;i+1<kPartyCapacity;++i)collection.party[i]=collection.party[i+1];
  collection.party[kPartyCapacity-1]=kEmptyPokemonUid;return validate(collection);
}

bool CollectionLogic::validate(const PokemonCollection& collection) {
  if (collection.party[0] == kEmptyPokemonUid) return false;
  for (uint8_t i = 0; i < kPartyCapacity; ++i) {
    if (collection.party[i] != kEmptyPokemonUid && !find(collection, collection.party[i])) return false;
    for (uint8_t j = i + 1; j < kPartyCapacity; ++j) {
      if (collection.party[i] != kEmptyPokemonUid && collection.party[i] == collection.party[j]) return false;
    }
  }
  return true;
}

void CollectionLogic::advanceCare(OwnedPokemon& pokemon, uint32_t elapsedSeconds) {
  if (pokemon.recoverySecondsRemaining != 0) {
    if (elapsedSeconds >= pokemon.recoverySecondsRemaining) {
      pokemon.recoverySecondsRemaining = 0;
      pokemon.currentHp = pokemon.maximumHp;
    } else {
      pokemon.recoverySecondsRemaining -= elapsedSeconds;
    }
  }
  PetState state;
  state.fullness = pokemon.fullness;
  state.happiness = pokemon.happiness;
  state.careRemainderSeconds = pokemon.careRemainderSeconds;
  state.careTickCounter = pokemon.careTickCounter;
  PetLogic::advance(state, elapsedSeconds);
  pokemon.fullness = state.fullness;
  pokemon.happiness = state.happiness;
  pokemon.careRemainderSeconds = state.careRemainderSeconds;
  pokemon.careTickCounter = state.careTickCounter;
}

void CollectionLogic::care(OwnedPokemon& pokemon, CareAction action) {
  PetState state;
  state.fullness = pokemon.fullness;
  state.happiness = pokemon.happiness;
  state.status = pokemon.status;
  PetLogic::care(state, action);
  pokemon.fullness = state.fullness;
  pokemon.happiness = state.happiness;
  pokemon.status = state.status;
  if (action == CareAction::Bathe) {
    for (uint8_t slot = 0; slot < kMoveSlots; ++slot) {
      const FullMoveData* move = findFullMove(pokemon.moves[slot]);
      pokemon.movePp[slot] = move ? move->pp : 0;
    }
  }
}
