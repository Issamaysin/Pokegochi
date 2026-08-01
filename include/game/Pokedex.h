#pragma once
#include <cstdint>

constexpr uint16_t kPokedexSpeciesCount = 151;
constexpr uint8_t kPokedexBytes = 19;

struct PokedexState {
  uint8_t seen[kPokedexBytes]{};
  uint8_t caught[kPokedexBytes]{};
};

struct PokedexEntryData {
  const char* category;
  uint16_t heightDecimeters;
  uint16_t weightHectograms;
  const char* description;
};

const PokedexEntryData* pokedexEntry(uint16_t speciesId);

class PokedexLogic {
 public:
  static bool isValidSpecies(uint16_t speciesId);
  static void markSeen(PokedexState& pokedex, uint16_t speciesId);
  static void markCaught(PokedexState& pokedex, uint16_t speciesId);
  static bool hasSeen(const PokedexState& pokedex, uint16_t speciesId);
  static bool hasCaught(const PokedexState& pokedex, uint16_t speciesId);
  static uint16_t seenCount(const PokedexState& pokedex);
  static uint16_t caughtCount(const PokedexState& pokedex);
};
