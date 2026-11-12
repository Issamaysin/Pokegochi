#pragma once

#include <cstdint>

#include "game/Collection.h"

// Five persistent, numbered teams are available from Box -> Party. The
// selected primary preset is mirrored into PokemonCollection::party so every
// existing battle, recovery, Home and multiplayer path keeps one canonical
// active Party without knowing about the four parked teams.
constexpr uint8_t kPartyPresetCount = 5U;

struct PartyPresetState {
  static constexpr uint32_t kSchemaMarker = 0x50525459UL;  // "PRTY"

  uint32_t schemaMarker = 0U;
  uint32_t members[kPartyPresetCount][kPartyCapacity]{};
  uint8_t primary = 0U;
  uint8_t reserved[3]{};
};
static_assert(sizeof(PartyPresetState) == 68U,
              "Party preset save layout changed");

class PartyPresetLogic {
 public:
  static void initialize(PartyPresetState& state,
                         const PokemonCollection& collection);
  static bool reconcile(PartyPresetState& state,
                        PokemonCollection& collection);
  static bool capturePrimary(PartyPresetState& state,
                             PokemonCollection& collection);

  static uint32_t memberUid(const PartyPresetState& state, uint8_t preset,
                            uint8_t slot);
  static OwnedPokemon* member(PokemonCollection& collection,
                              const PartyPresetState& state, uint8_t preset,
                              uint8_t slot);
  static const OwnedPokemon* member(const PokemonCollection& collection,
                                    const PartyPresetState& state,
                                    uint8_t preset, uint8_t slot);
  static bool contains(const PartyPresetState& state, uint8_t preset,
                       uint32_t uid);
  static uint8_t count(const PartyPresetState& state, uint8_t preset);

  // A Pokemon may be reused in different presets, but never duplicated inside
  // one preset. Setting a duplicate swaps the two positions, matching the
  // original Party editor.
  static bool setSlot(PartyPresetState& state, PokemonCollection& collection,
                      uint8_t preset, uint8_t slot, uint32_t uid);
  static bool remove(PartyPresetState& state, PokemonCollection& collection,
                     uint8_t preset, uint32_t uid);
  static bool makePrimary(PartyPresetState& state,
                          PokemonCollection& collection, uint8_t preset);

  // Storage lifecycle hooks. Release/fusion removes a UID everywhere; a trade
  // preserves every preset position while replacing the local UID.
  static bool removeUid(PartyPresetState& state,
                        PokemonCollection& collection, uint32_t uid);
  static bool replaceUid(PartyPresetState& state,
                         PokemonCollection& collection, uint32_t oldUid,
                         uint32_t newUid);

  static bool validate(const PartyPresetState& state,
                       const PokemonCollection& collection);
};
