#pragma once
#include <cstdint>
#include "game/PetState.h"
#include "game/PokemonData.h"
#include "game/HeldItems.h"

// V41 was shipped with one permanent Box position per Gen I--III species.
// Sinnoh extends the National Dex to #493; keep the old capacity explicitly
// named so PersistentSave can migrate every previously shipped record without
// ever treating its trailing save bytes as Pokemon data.
constexpr uint16_t kBoxCapacityV41 = 386;
constexpr uint16_t kBoxCapacityV45 = 493;
constexpr uint16_t kBoxCapacityV46 = 720;
constexpr uint16_t kBoxCapacityV47 = 800;
// 721 slots are required for one of every National species through Kalos.
// Keep 63 additional positions for forms, shinies and duplicates.  The
// shipped V47 capacity of 800 exceeded the ESP32's largest contiguous heap
// block once the renderer was resident, so V48 compacts occupied legacy
// positions into this still-complete living-Dex capacity.
constexpr uint16_t kBoxCapacity = 784;
constexpr uint8_t kPartyCapacity = 3;
constexpr uint8_t kMoveSlots = 4;
constexpr uint32_t kEmptyPokemonUid = 0;

enum class PokemonStat : uint8_t { Attack, Defense, Speed, SpAttack, SpDefense };

// Generation III order: boosted stat * 5 + reduced stat, using
// Attack, Defense, Speed, Sp. Attack and Sp. Defense.
enum class PokemonNature : uint8_t {
  Hardy, Lonely, Brave, Adamant, Naughty,
  Bold, Docile, Relaxed, Impish, Lax,
  Timid, Hasty, Serious, Jolly, Naive,
  Modest, Mild, Quiet, Bashful, Rash,
  Calm, Gentle, Sassy, Careful, Quirky,
};

// Explicit PC sorting modes. Sorting changes only the physical Box slots;
// Party membership remains stable because Party entries reference Pokemon by
// UID rather than by their Box index.
enum class BoxSortMode : uint8_t { DexNumber, Level, EffortValues };

enum class BurmyCloak : uint8_t { Plant = 0, Sandy = 1, Trash = 2 };
enum class PrisonBottleState : uint8_t { Unavailable, Ready, Unbound, Cooldown };

struct IndividualValues {
  uint8_t hp = 0;
  uint8_t attack = 0;
  uint8_t defense = 0;
  uint8_t spAttack = 0;
  uint8_t spDefense = 0;
  uint8_t speed = 0;
};

// Gen-III stores one byte for each stat. The practical cap is enforced by
// CollectionLogic: 255 per value and 510 over the complete set. Pokegochi
// deliberately grants shiny Pokemon one additional 255-point total pool.
struct EffortValues {
  uint8_t hp = 0;
  uint8_t attack = 0;
  uint8_t defense = 0;
  uint8_t spAttack = 0;
  uint8_t spDefense = 0;
  uint8_t speed = 0;
};

struct OwnedPokemon {
  uint32_t uid = kEmptyPokemonUid;
  uint32_t personality = 0;
  uint16_t speciesId = 0;
  uint32_t experience = 0;
  uint16_t currentHp = 0;
  uint16_t maximumHp = 0;
  uint8_t level = 1;
  StatusCondition status = StatusCondition::None;
  // Persistent alternate form.  This deliberately reuses the former
  // Fullness byte, so adding Deoxys forms does not enlarge or invalidate the
  // beta save record. Unown and Spinda remain personality-derived, while
  // Castform's weather form is battle-local.
  uint8_t form = 0;
  // FireRed Friendship: 0..255. Most newly obtained Pokemon begin at 70.
  uint8_t friendship = 70;
  uint32_t friendshipRemainderSeconds = 0;
  uint32_t legacyCareCounterReserved = 0;
  uint32_t recoverySecondsRemaining = 0;
  MoveId moves[kMoveSlots] = {MoveId::None, MoveId::None, MoveId::None, MoveId::None};
  uint8_t movePp[kMoveSlots]{};
  uint8_t abilityId = 0;
  IndividualValues ivs{};
  PokemonNature nature = PokemonNature::Hardy;
  HeldItem heldItem = HeldItem::None;
  bool shiny = false;
  EffortValues evs{};
};

// Frozen V41 collection payload.  This is intentionally kept beside the
// active collection rather than reconstructed from a byte size in the save
// layer, making the V41 -> V42 migration reviewable and type-safe.
struct PokemonCollectionV41 {
  OwnedPokemon box[kBoxCapacityV41]{};
  uint32_t party[kPartyCapacity]{};
  uint32_t nextUid = 1;
};

// Frozen Sinnoh/V45 collection used only by save migration.
struct PokemonCollectionV45 {
  OwnedPokemon box[kBoxCapacityV45]{};
  uint32_t party[kPartyCapacity]{};
  uint32_t nextUid = 1;

  PokemonCollectionV45& operator=(const PokemonCollectionV41& legacy) {
    for (uint16_t index = 0; index < kBoxCapacityV45; ++index)
      box[index] = index < kBoxCapacityV41 ? legacy.box[index] : OwnedPokemon{};
    for (uint8_t index = 0; index < kPartyCapacity; ++index)
      party[index] = legacy.party[index];
    nextUid = legacy.nextUid;
    return *this;
  }
};

// Frozen Unova/V46 collection used by the V46 -> V47 migration. Kalos adds
// space for every National species through #721 plus alternate forms and
// duplicates without reinterpreting any shipped Box bytes.
struct PokemonCollectionV46 {
  OwnedPokemon box[kBoxCapacityV46]{};
  uint32_t party[kPartyCapacity]{};
  uint32_t nextUid = 1;
};

// Frozen Kalos/V47 collection.  V48 reduces only unused reserve positions so
// the complete GameSave fits one contiguous ESP32 allocation.  Migration
// compacts every occupied V47 slot instead of truncating its final 16 slots.
struct PokemonCollectionV47 {
  OwnedPokemon box[kBoxCapacityV47]{};
  uint32_t party[kPartyCapacity]{};
  uint32_t nextUid = 1;
};

struct PokemonCollection {
  OwnedPokemon box[kBoxCapacity]{};
  uint32_t party[kPartyCapacity]{};
  uint32_t nextUid = 1;

  PokemonCollection& operator=(const PokemonCollectionV41& legacy) {
    for (uint16_t index = 0; index < kBoxCapacity; ++index)
      box[index] = index < kBoxCapacityV41 ? legacy.box[index] : OwnedPokemon{};
    for (uint8_t index = 0; index < kPartyCapacity; ++index)
      party[index] = legacy.party[index];
    nextUid = legacy.nextUid;
    return *this;
  }

  PokemonCollection& operator=(const PokemonCollectionV45& legacy) {
    for (uint16_t index = 0; index < kBoxCapacity; ++index)
      box[index] = index < kBoxCapacityV45 ? legacy.box[index] : OwnedPokemon{};
    for (uint8_t index = 0; index < kPartyCapacity; ++index)
      party[index] = legacy.party[index];
    nextUid = legacy.nextUid;
    return *this;
  }

  PokemonCollection& operator=(const PokemonCollectionV46& legacy) {
    for (uint16_t index = 0; index < kBoxCapacity; ++index)
      box[index] = index < kBoxCapacityV46 ? legacy.box[index] : OwnedPokemon{};
    for (uint8_t index = 0; index < kPartyCapacity; ++index)
      party[index] = legacy.party[index];
    nextUid = legacy.nextUid;
    return *this;
  }

  bool importV47(const PokemonCollectionV47& legacy) {
    uint16_t occupied = 0;
    for (uint16_t index = 0; index < kBoxCapacityV47; ++index)
      if (legacy.box[index].uid != kEmptyPokemonUid) ++occupied;
    if (occupied > kBoxCapacity) return false;

    uint16_t destination = 0;
    for (uint16_t index = 0; index < kBoxCapacityV47; ++index) {
      if (legacy.box[index].uid == kEmptyPokemonUid) continue;
      box[destination++] = legacy.box[index];
    }
    while (destination < kBoxCapacity) box[destination++] = OwnedPokemon{};
    for (uint8_t index = 0; index < kPartyCapacity; ++index)
      party[index] = legacy.party[index];
    nextUid = legacy.nextUid;
    return true;
  }
};

struct RareCandyProgression {
  bool applied = false;
  bool levelIncreased = false;
  MoveId learnedMove = MoveId::None;
  uint16_t evolutionChoices[kEvolutionChoiceCapacity]{};
  uint8_t evolutionChoiceCount = 0;
};

class CollectionLogic {
 public:
  static void initialize(PokemonCollection& collection);
  static bool chooseStarter(PokemonCollection& collection, uint16_t speciesId, uint32_t shinyRoll = 1);
  static bool addRegionalStarter(PokemonCollection& collection, uint16_t speciesId, uint32_t shinyRoll = 1);
  static OwnedPokemon createPokemon(uint32_t uid, uint16_t speciesId, uint8_t level,
                                    bool shiny = false, uint32_t personalitySeed = 0,
                                    uint8_t unlockedGeneration = 1);
  // Older development saves may predate the move-set fields.  Repairing a
  // creature with no valid moves keeps an automatic battle replacement usable.
  static bool ensureUsableMoves(OwnedPokemon& pokemon);
  // Generation III PP Up data: two bits per move slot (0..3 uses), packed in
  // the old reserved care field so adding it does not enlarge all 386 Box
  // records. Each use raises the move's maximum PP by 20% of its base PP.
  static uint8_t ppUpCount(const OwnedPokemon& pokemon, uint8_t slot);
  static uint8_t maximumMovePp(const OwnedPokemon& pokemon, uint8_t slot);
  static bool applyPpUp(OwnedPokemon& pokemon, uint8_t slot);
  static void clearMovePpUps(OwnedPokemon& pokemon, uint8_t slot);
  static void swapMovePpUps(OwnedPokemon& pokemon, uint8_t first, uint8_t second);
  // Hidden Ability provenance uses bit 14 of the old care counter. Bits 0-7
  // are PP Ups, 8-10 Berry quantity, 11-13 Mega choice, bit 15 is reserved
  // for a battle-triggered form and 16-31 hold timed-form state. Keeping all
  // of them here preserves the compact Pokemon/save ABI.
  static bool hasHiddenAbility(const OwnedPokemon& pokemon);
  static bool setHiddenAbility(OwnedPokemon& pokemon, bool enabled,
                               uint8_t unlockedGeneration = 5);
  // Berries share the otherwise unused upper bits of the old care counter.
  // Old saves therefore read as a one-Berry stack without enlarging all 386
  // Box records. Non-Berry held items always have quantity one.
  static constexpr uint8_t kMaximumHeldBerryQuantity = 5;
  static uint8_t heldItemQuantity(const OwnedPokemon& pokemon);
  static bool setHeldItemQuantity(OwnedPokemon& pokemon, HeldItem item, uint8_t quantity);
  static bool consumeHeldItem(OwnedPokemon& pokemon);
  static const char* natureName(PokemonNature nature);
  static int8_t natureEffect(PokemonNature nature, PokemonStat stat);
  static constexpr uint16_t kMaximumTotalEffortValues = 510;
  static constexpr uint16_t kShinyMaximumTotalEffortValues = 765;
  static constexpr uint8_t kMaximumEffortValue = 255;
  static uint16_t maximumTotalEffortValues(const OwnedPokemon& pokemon);
  static uint16_t totalEffortValues(const OwnedPokemon& pokemon);
  // Replaces an already-trained Pokemon's EV spread without creating or
  // deleting any earned points. This is exposed to the Lv.90 Summary editor;
  // the regular battle award path remains grantEffortValues().
  static bool redistributeEffortValues(OwnedPokemon& pokemon, const EffortValues& replacement);
  // Awards as many EVs as the individual and species-instance total cap
  // permits, and returns the number of points actually retained. Shiny
  // Pokemon use kShinyMaximumTotalEffortValues; all others retain Gen III's
  // original total limit.
  static uint8_t grantEffortValues(OwnedPokemon& pokemon, const EffortValues& yield);
  static uint8_t resolvedBaseHp(const OwnedPokemon& pokemon);
  static uint8_t resolvedBaseStat(const OwnedPokemon& pokemon, PokemonStat stat);
  static uint16_t calculatedMaximumHp(const OwnedPokemon& pokemon);
  static uint16_t calculatedStat(const OwnedPokemon& pokemon, PokemonStat stat);
  static void refreshDerivedStats(OwnedPokemon& pokemon, bool preserveDamage = true);
  // Form changes that substantially alter base HP sometimes need to preserve
  // the battler's HP percentage instead of its absolute missing damage.
  static void refreshDerivedStatsPreservingHpRatio(OwnedPokemon& pokemon);
  // The active ability pair changes with content generation. Require every
  // caller to state its context so a late-game Hidden Ability can never be
  // silently recalculated with an early-generation default.
  static void refreshAbility(OwnedPokemon& pokemon, uint8_t unlockedGeneration);
  // Applies the newly unlocked Platinum ability pair to existing records.
  // Level-up moves are intentionally not retroactively injected; they become
  // available to future level-ups and to newly generated Sinnoh encounters.
  static void applyGenerationContent(PokemonCollection& collection,
                                     uint8_t unlockedGeneration);
  // Form 0 is the normal/default form. Deoxys supports explicit forms 0..3;
  // Sinnoh adds persistent environmental/coast forms beside selectable
  // Rotom, Giratina and Shaymin. Arceus is derived from its held Plate and
  // Unown remains personality-derived.
  static uint8_t resolvedForm(const OwnedPokemon& pokemon);
  static uint8_t formCount(uint16_t speciesId);
  static const char* formName(uint16_t speciesId, uint8_t form);
  static bool canChangeForm(const OwnedPokemon& pokemon);
  static bool setForm(OwnedPokemon& pokemon, uint8_t form);
  // Context-aware form picker. `worldHour` is the persistent local-world
  // hour (not millis()) so Furfrou trims continue expiring while powered off.
  static bool setFormForWorld(OwnedPokemon& pokemon, uint8_t form,
                              uint32_t worldHour, bool night);
  static bool normalizeForm(OwnedPokemon& pokemon);
  static bool applyBurmyCloak(PokemonCollection& collection, BurmyCloak cloak);
  static PrisonBottleState prisonBottleState(const OwnedPokemon& pokemon,
                                              uint32_t worldHour);
  static uint8_t prisonBottleDaysRemaining(const OwnedPokemon& pokemon,
                                           uint32_t worldHour);
  static bool usePrisonBottle(OwnedPokemon& pokemon, uint32_t worldHour);
  // Reverts Sky Shaymin, expired/stored Furfrou and Hoopa Unbound, and
  // releases Hoopa's cooldown after the requested three real-time days.
  static bool reconcileFormLifecycles(PokemonCollection& collection,
                                      uint32_t worldHour, bool night);
  // Black/White rotate Deerling and Sawsbuck together with the current
  // season. This updates every stored member without changing individual
  // identity, damage, moves or provenance.
  static bool applyUnovaSeason(PokemonCollection& collection, uint8_t season);
  static bool applyUnovaSeason(OwnedPokemon& pokemon, uint8_t season);
  static bool isShinyRoll(uint32_t roll) { return (roll & 8191U) == 0; }
  static OwnedPokemon* find(PokemonCollection& collection, uint32_t uid);
  static const OwnedPokemon* find(const PokemonCollection& collection, uint32_t uid);
  static OwnedPokemon* active(PokemonCollection& collection, uint8_t slot);
  static const OwnedPokemon* active(const PokemonCollection& collection, uint8_t slot);
  static uint16_t count(const PokemonCollection& collection);
  static bool add(PokemonCollection& collection, OwnedPokemon pokemon, uint32_t* assignedUid = nullptr);
  // Commits an evolution only after its cinematic has completed. Nincada's
  // paired Shedinja is created here as part of the same operation; a full Box
  // still permits Ninjask but simply leaves bonusUid empty.
  static bool evolve(PokemonCollection& collection, uint32_t uid,
                     uint16_t newSpeciesId, uint32_t* bonusUid = nullptr,
                     uint8_t unlockedGeneration = 3);
  // Applies the non-inventory part of a Rare Candy. At Lv.100 a Candy may
  // still trigger an already-eligible evolution, preventing a delayed
  // three-stage family from becoming permanently stuck after its first form
  // change. The caller remains responsible for consuming the actual item and
  // presenting move/evolution choices.
  static RareCandyProgression applyRareCandy(OwnedPokemon& pokemon,
                                             uint8_t unlockedGeneration);
  static bool setPartySlot(PokemonCollection& collection, uint8_t slot, uint32_t uid);
  static bool removeFromParty(PokemonCollection& collection, uint32_t uid);
  // Explicitly releases one stored Pokemon after the UI has shown its YES/NO
  // confirmation. The final party member is protected just like FireRed.
  static bool release(PokemonCollection& collection, uint32_t uid);
  // Atomically replaces a locally owned Pokemon with one received in a
  // multiplayer trade.  The received Pokemon gets a fresh local UID while
  // retaining species, level, moves, IVs/EVs, nature, friendship and held
  // item. If the outgoing Pokemon was active, the incoming one occupies that
  // same Party slot so a trade can never leave the Pokegochi without a pet.
  static bool tradeReplace(PokemonCollection& collection, uint32_t outgoingUid,
                           OwnedPokemon incoming, uint32_t* assignedUid = nullptr);
  static bool isInParty(const PokemonCollection& collection, uint32_t uid);
  // DEX NUMBER is ascending. LEVEL is descending so the strongest Pokemon
  // are immediately visible. EFFORT VALUES uses the accumulated total in
  // descending order, then descending level. Empty storage slots are always
  // packed at the end and equal keys are ordered deterministically.
  static void sortBox(PokemonCollection& collection, BoxSortMode mode);
  static bool validate(const PokemonCollection& collection);
  // Passive HP/PP recovery takes ten minutes for a Party Pokemon through
  // Lv.20, thirty minutes for a Party Pokemon from Lv.21 onward, and three hours in
  // the Box. `worldTimeSeconds` makes fractional recovery exact without adding
  // per-stat counters to every persistent Pokemon record.
  static void advanceRecovery(OwnedPokemon& pokemon, uint32_t elapsedSeconds,
                              bool inParty, uint32_t worldTimeSeconds);
  // Friendship is deliberately advanced only for an active party member.
  static void advanceFriendship(OwnedPokemon& pokemon, uint32_t elapsedSeconds);
  static void care(OwnedPokemon& pokemon, CareAction action);
};
