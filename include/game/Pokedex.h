#pragma once
#include <cstdint>

constexpr uint16_t kPokedexSpeciesCountV41 = 386;
constexpr uint8_t kPokedexBytesV41 = 49;
constexpr uint16_t kPokedexSpeciesCountV45 = 493;
constexpr uint8_t kPokedexBytesV45 = 62;
constexpr uint16_t kPokedexSpeciesCount = 649;
constexpr uint8_t kPokedexBytes = 82;

struct PokedexStateV41 {
  uint8_t seen[kPokedexBytesV41]{};
  uint8_t caught[kPokedexBytesV41]{};
};

struct PokedexStateV45 {
  uint8_t seen[kPokedexBytesV45]{};
  uint8_t caught[kPokedexBytesV45]{};

  PokedexStateV45& operator=(const PokedexStateV41& legacy) {
    for (uint8_t index = 0; index < kPokedexBytesV45; ++index) {
      seen[index] = index < kPokedexBytesV41 ? legacy.seen[index] : 0U;
      caught[index] = index < kPokedexBytesV41 ? legacy.caught[index] : 0U;
    }
    return *this;
  }
};

struct PokedexState {
  uint8_t seen[kPokedexBytes]{};
  uint8_t caught[kPokedexBytes]{};

  PokedexState& operator=(const PokedexStateV41& legacy) {
    for (uint8_t index = 0; index < kPokedexBytes; ++index) {
      seen[index] = index < kPokedexBytesV41 ? legacy.seen[index] : 0U;
      caught[index] = index < kPokedexBytesV41 ? legacy.caught[index] : 0U;
    }
    return *this;
  }

  PokedexState& operator=(const PokedexStateV45& legacy) {
    for (uint8_t index = 0; index < kPokedexBytes; ++index) {
      seen[index] = index < kPokedexBytesV45 ? legacy.seen[index] : 0U;
      caught[index] = index < kPokedexBytesV45 ? legacy.caught[index] : 0U;
    }
    return *this;
  }
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
  // Shared classification for statistics and systems that include the
  // Gen-IV legendary/mythical IDs without duplicating the table.
  static bool isLegendaryOrMythical(uint16_t speciesId);
  static void markSeen(PokedexState& pokedex, uint16_t speciesId);
  static void markCaught(PokedexState& pokedex, uint16_t speciesId);
  static bool hasSeen(const PokedexState& pokedex, uint16_t speciesId);
  static bool hasCaught(const PokedexState& pokedex, uint16_t speciesId);
  static uint16_t seenCount(const PokedexState& pokedex);
  static uint16_t caughtCount(const PokedexState& pokedex);
  static uint16_t generationSpeciesCount(uint8_t generation);
  static uint16_t generationCaughtCount(const PokedexState& pokedex, uint8_t generation);
  static bool isGenerationComplete(const PokedexState& pokedex, uint8_t generation);
};
