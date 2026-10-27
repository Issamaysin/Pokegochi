#pragma once

#include <cstdint>

#include "game/Collection.h"
#include "game/HeldItems.h"

struct UnovaFormState {
  OwnedPokemon fusedDonor{};
  uint32_t fusedKyuremUid = 0;
  MoveId kyuremOriginalMoves[kMoveSlots]{};
  uint8_t kyuremOriginalPp[kMoveSlots]{};
};

namespace UnovaForms {
bool reconcileUniqueItems(const PokemonCollection& collection,
                          Gen5UniqueItemInventory& items, bool unovaUnlocked);
bool hasDnaSplicers(const Gen5UniqueItemInventory& items);
bool canFuse(const PokemonCollection& collection, const UnovaFormState& state,
             uint32_t kyuremUid, uint32_t donorUid);
bool fuse(PokemonCollection& collection, UnovaFormState& state,
          const Gen5UniqueItemInventory& items, uint32_t kyuremUid,
          uint32_t donorUid);
bool unfuse(PokemonCollection& collection, UnovaFormState& state);
inline PokemonType genesectTechnoBlastType(const OwnedPokemon& pokemon) {
  if (pokemon.speciesId != 649U) return PokemonType::Normal;
  switch (pokemon.heldItem) {
    case HeldItem::DouseDrive: return PokemonType::Water;
    case HeldItem::ShockDrive: return PokemonType::Electric;
    case HeldItem::BurnDrive: return PokemonType::Fire;
    case HeldItem::ChillDrive: return PokemonType::Ice;
    default: return PokemonType::Normal;
  }
}
}
