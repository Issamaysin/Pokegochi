#include "game/PartyPresets.h"

#include <cstring>

namespace {

bool initialized(const PartyPresetState& state) {
  return state.schemaMarker == PartyPresetState::kSchemaMarker &&
         state.primary < kPartyPresetCount;
}

bool sameParty(const uint32_t left[kPartyCapacity],
               const uint32_t right[kPartyCapacity]) {
  for (uint8_t slot = 0U; slot < kPartyCapacity; ++slot)
    if (left[slot] != right[slot]) return false;
  return true;
}

void copyParty(uint32_t destination[kPartyCapacity],
               const uint32_t source[kPartyCapacity]) {
  std::memcpy(destination, source,
              static_cast<size_t>(kPartyCapacity) * sizeof(uint32_t));
}

bool normalizeParty(uint32_t party[kPartyCapacity],
                    const PokemonCollection& collection) {
  uint32_t normalized[kPartyCapacity]{};
  uint8_t count = 0U;
  for (uint8_t slot = 0U; slot < kPartyCapacity; ++slot) {
    const uint32_t uid = party[slot];
    if (!uid || !CollectionLogic::find(collection, uid)) continue;
    bool duplicate = false;
    for (uint8_t existing = 0U; existing < count; ++existing)
      if (normalized[existing] == uid) duplicate = true;
    if (!duplicate) normalized[count++] = uid;
  }
  if (sameParty(party, normalized)) return false;
  copyParty(party, normalized);
  return true;
}

void mirrorPrimary(const PartyPresetState& state,
                   PokemonCollection& collection) {
  copyParty(collection.party, state.members[state.primary]);
}

}  // namespace

void PartyPresetLogic::initialize(PartyPresetState& state,
                                  const PokemonCollection& collection) {
  state = PartyPresetState{};
  state.schemaMarker = PartyPresetState::kSchemaMarker;
  state.primary = 0U;
  copyParty(state.members[0], collection.party);
}

bool PartyPresetLogic::reconcile(PartyPresetState& state,
                                 PokemonCollection& collection) {
  bool changed = false;
  if (!initialized(state)) {
    initialize(state, collection);
    changed = true;
  }

  for (uint8_t preset = 0U; preset < kPartyPresetCount; ++preset)
    changed = normalizeParty(state.members[preset], collection) || changed;

  if (!state.members[state.primary][0]) {
    uint32_t active[kPartyCapacity]{};
    copyParty(active, collection.party);
    normalizeParty(active, collection);
    if (active[0]) {
      copyParty(state.members[state.primary], active);
      changed = true;
    } else {
      for (uint8_t preset = 0U; preset < kPartyPresetCount; ++preset) {
        if (!state.members[preset][0]) continue;
        state.primary = preset;
        changed = true;
        break;
      }
    }
  }

  if (!sameParty(collection.party, state.members[state.primary])) {
    mirrorPrimary(state, collection);
    changed = true;
  }
  return changed;
}

bool PartyPresetLogic::capturePrimary(PartyPresetState& state,
                                      PokemonCollection& collection) {
  if (!initialized(state)) initialize(state, collection);
  bool changed = !sameParty(state.members[state.primary], collection.party);
  copyParty(state.members[state.primary], collection.party);
  for (uint8_t preset = 0U; preset < kPartyPresetCount; ++preset)
    changed = normalizeParty(state.members[preset], collection) || changed;
  mirrorPrimary(state, collection);
  return changed;
}

uint32_t PartyPresetLogic::memberUid(const PartyPresetState& state,
                                     uint8_t preset, uint8_t slot) {
  return initialized(state) && preset < kPartyPresetCount &&
                 slot < kPartyCapacity
             ? state.members[preset][slot]
             : kEmptyPokemonUid;
}

OwnedPokemon* PartyPresetLogic::member(PokemonCollection& collection,
                                       const PartyPresetState& state,
                                       uint8_t preset, uint8_t slot) {
  return CollectionLogic::find(collection, memberUid(state, preset, slot));
}

const OwnedPokemon* PartyPresetLogic::member(
    const PokemonCollection& collection, const PartyPresetState& state,
    uint8_t preset, uint8_t slot) {
  return CollectionLogic::find(collection, memberUid(state, preset, slot));
}

bool PartyPresetLogic::contains(const PartyPresetState& state, uint8_t preset,
                                uint32_t uid) {
  if (!uid || !initialized(state) || preset >= kPartyPresetCount) return false;
  for (uint32_t memberUid : state.members[preset])
    if (memberUid == uid) return true;
  return false;
}

uint8_t PartyPresetLogic::count(const PartyPresetState& state,
                                uint8_t preset) {
  if (!initialized(state) || preset >= kPartyPresetCount) return 0U;
  uint8_t total = 0U;
  for (uint32_t uid : state.members[preset])
    if (uid) ++total;
  return total;
}

bool PartyPresetLogic::setSlot(PartyPresetState& state,
                               PokemonCollection& collection, uint8_t preset,
                               uint8_t slot, uint32_t uid) {
  if (preset >= kPartyPresetCount || slot >= kPartyCapacity ||
      !CollectionLogic::find(collection, uid)) return false;
  if (!initialized(state)) initialize(state, collection);

  uint32_t* party = state.members[preset];
  for (uint8_t other = 0U; other < kPartyCapacity; ++other) {
    if (other == slot || party[other] != uid) continue;
    const uint32_t replaced = party[slot];
    party[slot] = uid;
    party[other] = replaced;
    if (preset == state.primary) mirrorPrimary(state, collection);
    return true;
  }
  party[slot] = uid;
  normalizeParty(party, collection);
  if (preset == state.primary) mirrorPrimary(state, collection);
  return true;
}

bool PartyPresetLogic::remove(PartyPresetState& state,
                              PokemonCollection& collection, uint8_t preset,
                              uint32_t uid) {
  if (!initialized(state) || preset >= kPartyPresetCount || !uid ||
      !contains(state, preset, uid)) return false;
  if (preset == state.primary && count(state, preset) <= 1U) return false;
  uint32_t* party = state.members[preset];
  uint8_t destination = 0U;
  for (uint8_t source = 0U; source < kPartyCapacity; ++source)
    if (party[source] && party[source] != uid)
      party[destination++] = party[source];
  while (destination < kPartyCapacity)
    party[destination++] = kEmptyPokemonUid;
  if (preset == state.primary) mirrorPrimary(state, collection);
  return true;
}

bool PartyPresetLogic::makePrimary(PartyPresetState& state,
                                   PokemonCollection& collection,
                                   uint8_t preset) {
  if (!initialized(state) || preset >= kPartyPresetCount) return false;
  normalizeParty(state.members[preset], collection);
  if (!state.members[preset][0]) return false;
  state.primary = preset;
  mirrorPrimary(state, collection);
  return CollectionLogic::validate(collection);
}

bool PartyPresetLogic::removeUid(PartyPresetState& state,
                                 PokemonCollection& collection,
                                 uint32_t uid) {
  if (!initialized(state) || !uid) return false;
  bool changed = false;
  for (uint8_t preset = 0U; preset < kPartyPresetCount; ++preset) {
    uint32_t* party = state.members[preset];
    uint8_t destination = 0U;
    for (uint8_t source = 0U; source < kPartyCapacity; ++source) {
      if (!party[source] || party[source] == uid) {
        changed = changed || party[source] == uid;
        continue;
      }
      party[destination++] = party[source];
    }
    while (destination < kPartyCapacity)
      party[destination++] = kEmptyPokemonUid;
  }
  if (!state.members[state.primary][0] && collection.party[0])
    copyParty(state.members[state.primary], collection.party);
  mirrorPrimary(state, collection);
  return changed;
}

bool PartyPresetLogic::replaceUid(PartyPresetState& state,
                                  PokemonCollection& collection,
                                  uint32_t oldUid, uint32_t newUid) {
  if (!initialized(state) || !oldUid || !newUid ||
      !CollectionLogic::find(collection, newUid)) return false;
  bool changed = false;
  for (uint8_t preset = 0U; preset < kPartyPresetCount; ++preset) {
    for (uint8_t slot = 0U; slot < kPartyCapacity; ++slot) {
      if (state.members[preset][slot] != oldUid) continue;
      state.members[preset][slot] = newUid;
      changed = true;
    }
    normalizeParty(state.members[preset], collection);
  }
  mirrorPrimary(state, collection);
  return changed;
}

bool PartyPresetLogic::validate(const PartyPresetState& state,
                                const PokemonCollection& collection) {
  if (!initialized(state) || !state.members[state.primary][0] ||
      !sameParty(collection.party, state.members[state.primary])) return false;
  for (uint8_t preset = 0U; preset < kPartyPresetCount; ++preset) {
    for (uint8_t slot = 0U; slot < kPartyCapacity; ++slot) {
      const uint32_t uid = state.members[preset][slot];
      if (uid && !CollectionLogic::find(collection, uid)) return false;
      for (uint8_t other = static_cast<uint8_t>(slot + 1U);
           other < kPartyCapacity; ++other)
        if (uid && uid == state.members[preset][other]) return false;
    }
  }
  return CollectionLogic::validate(collection);
}
