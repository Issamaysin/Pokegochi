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
// Keep fan-Mega abilities above every canonical Generation-V ability. The
// original 128..156 range collided with Unova's canonical IDs 127..155 and
// made the same byte resolve to two unrelated abilities.
constexpr uint8_t kFirstCustomAbilityId = 192;
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
// Stable read-only enumeration used by systems that may select any authored
// Mega form. The returned pointer refers to the static form catalog.
const MegaFormData* officialForm(uint8_t index);
}
