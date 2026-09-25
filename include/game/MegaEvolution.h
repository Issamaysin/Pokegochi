#pragma once
#include <cstdint>
#include "game/HeldItems.h"
#include "game/PokemonData.h"

struct OwnedPokemon;

enum class MegaVariant : uint8_t { None, Mega, MegaX, MegaY, Primal, MegaZ };

struct MegaFormData {
  uint16_t speciesId;
  MegaVariant variant;
  uint8_t baseHp, baseAttack, baseDefense, baseSpAttack, baseSpDefense, baseSpeed;
  PokemonType type1, type2;
  uint8_t abilityId;
  const char* assetVariant;
  const char* displayName;
};

namespace MegaEvolution {
constexpr uint8_t kFirstCustomAbilityId = 128;
const MegaFormData* formData(uint16_t speciesId, MegaVariant variant);
MegaVariant variantFor(const OwnedPokemon& pokemon);
bool canTransform(uint16_t speciesId);
uint8_t variantChoiceCount(uint16_t speciesId);
MegaVariant variantChoice(uint16_t speciesId, uint8_t index);
bool setVariantChoice(OwnedPokemon& pokemon, MegaVariant variant);
uint8_t baseHp(const OwnedPokemon& pokemon, uint8_t fallback);
uint8_t baseStat(const OwnedPokemon& pokemon, uint8_t statIndex, uint8_t fallback);
PokemonType type1(const OwnedPokemon& pokemon, PokemonType fallback);
PokemonType type2(const OwnedPokemon& pokemon, PokemonType fallback);
uint8_t abilityId(const OwnedPokemon& pokemon, uint8_t fallback);
const AbilityData* customAbility(uint8_t id);
const char* variantName(const OwnedPokemon& pokemon);
uint8_t officialFormCount();
}
