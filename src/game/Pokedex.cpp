#include "game/Pokedex.h"

#include "PokedexDataGenerated.inc"

namespace {
uint8_t byteIndex(uint16_t speciesId) { return static_cast<uint8_t>((speciesId - 1U) / 8U); }
uint8_t bitMask(uint16_t speciesId) { return static_cast<uint8_t>(1U << ((speciesId - 1U) % 8U)); }
}

const PokedexEntryData* pokedexEntry(uint16_t speciesId) {
  return speciesId >= 1 && speciesId <= kPokedexSpeciesCount ? &kPokedexEntries[speciesId - 1U] : nullptr;
}

bool PokedexLogic::isValidSpecies(uint16_t speciesId) { return speciesId >= 1 && speciesId <= kPokedexSpeciesCount; }

void PokedexLogic::markSeen(PokedexState& pokedex, uint16_t speciesId) {
  if (isValidSpecies(speciesId)) pokedex.seen[byteIndex(speciesId)] |= bitMask(speciesId);
}

void PokedexLogic::markCaught(PokedexState& pokedex, uint16_t speciesId) {
  if (!isValidSpecies(speciesId)) return;
  pokedex.caught[byteIndex(speciesId)] |= bitMask(speciesId);
  markSeen(pokedex, speciesId);
}

bool PokedexLogic::hasSeen(const PokedexState& pokedex, uint16_t speciesId) {
  return isValidSpecies(speciesId) && (pokedex.seen[byteIndex(speciesId)] & bitMask(speciesId));
}

bool PokedexLogic::hasCaught(const PokedexState& pokedex, uint16_t speciesId) {
  return isValidSpecies(speciesId) && (pokedex.caught[byteIndex(speciesId)] & bitMask(speciesId));
}

uint16_t PokedexLogic::seenCount(const PokedexState& pokedex) {
  uint16_t count = 0;
  for (uint16_t id = 1; id <= kPokedexSpeciesCount; ++id) if (hasSeen(pokedex, id)) ++count;
  return count;
}

uint16_t PokedexLogic::caughtCount(const PokedexState& pokedex) {
  uint16_t count = 0;
  for (uint16_t id = 1; id <= kPokedexSpeciesCount; ++id) if (hasCaught(pokedex, id)) ++count;
  return count;
}
