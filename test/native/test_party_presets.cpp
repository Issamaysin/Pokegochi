#include <cassert>

#include "game/PartyPresets.h"

int main() {
  PokemonCollection collection{};
  CollectionLogic::initialize(collection);
  assert(CollectionLogic::chooseStarter(collection, 1U, 0x1001U));
  const uint32_t starterUid = collection.party[0];

  uint32_t pikachuUid = 0U;
  uint32_t eeveeUid = 0U;
  assert(CollectionLogic::add(
      collection,
      CollectionLogic::createPokemon(0U, 25U, 20U, false, 0x2501U),
      &pikachuUid));
  assert(CollectionLogic::add(
      collection,
      CollectionLogic::createPokemon(0U, 133U, 20U, false, 0x1331U),
      &eeveeUid));

  PartyPresetState presets{};
  PartyPresetLogic::initialize(presets, collection);
  assert(presets.primary == 0U);
  assert(PartyPresetLogic::memberUid(presets, 0U, 0U) == starterUid);
  assert(PartyPresetLogic::validate(presets, collection));

  // Editing a parked team does not alter the Party used by battles.
  assert(PartyPresetLogic::setSlot(presets, collection, 1U, 0U, pikachuUid));
  assert(PartyPresetLogic::setSlot(presets, collection, 1U, 1U, eeveeUid));
  assert(collection.party[0] == starterUid && collection.party[1] == 0U);
  assert(PartyPresetLogic::count(presets, 1U) == 2U);

  // Reusing a Pokemon in another preset is legal, while selecting it twice in
  // one preset swaps positions instead of creating a duplicate.
  assert(PartyPresetLogic::setSlot(presets, collection, 2U, 0U, starterUid));
  assert(PartyPresetLogic::setSlot(presets, collection, 1U, 2U, pikachuUid));
  assert(PartyPresetLogic::memberUid(presets, 1U, 0U) == 0U);
  assert(PartyPresetLogic::memberUid(presets, 1U, 1U) == eeveeUid);
  assert(PartyPresetLogic::memberUid(presets, 1U, 2U) == pikachuUid);

  // Making a preset primary compacts it and atomically changes the legacy
  // active Party that every battle path already consumes.
  assert(PartyPresetLogic::makePrimary(presets, collection, 1U));
  assert(presets.primary == 1U);
  assert(collection.party[0] == eeveeUid);
  assert(collection.party[1] == pikachuUid);
  assert(PartyPresetLogic::validate(presets, collection));
  assert(!PartyPresetLogic::makePrimary(presets, collection, 4U));

  assert(PartyPresetLogic::remove(presets, collection, 1U, pikachuUid));
  assert(collection.party[0] == eeveeUid && collection.party[1] == 0U);
  assert(!PartyPresetLogic::remove(presets, collection, 1U, eeveeUid));

  // A trade retains every team position by replacing the local UID.
  OwnedPokemon incoming = CollectionLogic::createPokemon(
      0U, 143U, 30U, false, 0x1431U);
  uint32_t receivedUid = 0U;
  assert(CollectionLogic::tradeReplace(collection, starterUid, incoming,
                                       &receivedUid));
  assert(PartyPresetLogic::replaceUid(presets, collection, starterUid,
                                      receivedUid));
  assert(PartyPresetLogic::memberUid(presets, 0U, 0U) == receivedUid);
  assert(PartyPresetLogic::memberUid(presets, 2U, 0U) == receivedUid);

  // Release/fusion cleanup removes stale references from every parked team.
  assert(PartyPresetLogic::removeUid(presets, collection, receivedUid));
  assert(!PartyPresetLogic::contains(presets, 0U, receivedUid));
  assert(!PartyPresetLogic::contains(presets, 2U, receivedUid));
  assert(PartyPresetLogic::validate(presets, collection));

  // A pre-V53 state is initialized from the active Party without changing it.
  PartyPresetState migrated{};
  assert(PartyPresetLogic::reconcile(migrated, collection));
  assert(migrated.schemaMarker == PartyPresetState::kSchemaMarker);
  assert(migrated.members[0][0] == collection.party[0]);
  assert(PartyPresetLogic::validate(migrated, collection));
  return 0;
}
