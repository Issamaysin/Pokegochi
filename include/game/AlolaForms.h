#pragma once
#include <cstdint>
#include "game/Collection.h"
#include "game/HeldItems.h"

struct AlolaFormState {
  // The partner is removed from the Box while fused, exactly like Kyurem's
  // donor. Keeping the complete record prevents cloning and restores every
  // move, item, stat and personality byte when Necrozma is separated.
  OwnedPokemon fusedDonor{};
  uint32_t fusedNecrozmaUid = 0;
};

namespace AlolaForms {
// Regional variants share their National Dex number with the Kanto species,
// but are permanent individual identities. They must never enter the generic
// FORM picker used by Pokemon such as Rotom and Deoxys.
bool isRegionalSpecies(uint16_t speciesId);
bool isRegionalVariant(const OwnedPokemon& pokemon);
bool supports(uint16_t speciesId);
uint8_t initialForm(uint16_t speciesId, uint32_t personality);
uint8_t resolvedForm(const OwnedPokemon& pokemon, uint8_t fallback);
uint8_t formCount(uint16_t speciesId);
const char* formName(uint16_t speciesId, uint8_t form);
bool canSelectForm(uint16_t speciesId);

uint8_t baseHp(const OwnedPokemon& pokemon, uint8_t fallback);
uint8_t baseStat(const OwnedPokemon& pokemon, uint8_t statIndex, uint8_t fallback);
PokemonType type1(const OwnedPokemon& pokemon, PokemonType fallback);
PokemonType type2(const OwnedPokemon& pokemon, PokemonType fallback);
uint8_t abilityId(const OwnedPokemon& pokemon, uint8_t fallback);

// Multi-Attack follows the held Memory. Revelation Dance follows Oricorio's
// current style; these helpers keep Summary and battle calculations aligned.
PokemonType moveTypeOverride(const OwnedPokemon& pokemon, MoveId move,
                             PokemonType fallback);
// Silvally unlocks the complete Memory case once, matching the original
// game's post-evolution handoff without bloating individual encounter drops.
bool reconcileUniqueItems(PokemonCollection& collection,
                          Gen7UniqueItemInventory& inventory);
bool hasNecrozmaFusers(const Gen7UniqueItemInventory& inventory);
bool canFuse(const PokemonCollection& collection, const AlolaFormState& state,
             uint32_t necrozmaUid, uint32_t donorUid);
bool fuse(PokemonCollection& collection, AlolaFormState& state,
          const Gen7UniqueItemInventory& inventory, uint32_t necrozmaUid,
          uint32_t donorUid);
bool unfuse(PokemonCollection& collection, AlolaFormState& state);
bool reconcileFusionState(PokemonCollection& collection, AlolaFormState& state,
                          uint32_t activeBattleUid = 0U);

// Ultra Necrozma is battle-only. Bit 15 in legacyCareCounterReserved records
// whether the persistent fused form was Dawn Wings (otherwise Dusk Mane), so
// switching or ending a battle can restore the exact form without save growth.
bool activateUltraBurst(OwnedPokemon& pokemon);
bool resetUltraBurst(OwnedPokemon& pokemon);
}
