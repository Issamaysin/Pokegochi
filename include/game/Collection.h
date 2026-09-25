#pragma once
#include <cstdint>
#include "game/PetState.h"
#include "game/PokemonData.h"
#include "game/HeldItems.h"

constexpr uint16_t kBoxCapacity = 386;
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

struct PokemonCollection {
  OwnedPokemon box[kBoxCapacity]{};
  uint32_t party[kPartyCapacity]{};
  uint32_t nextUid = 1;
};

class CollectionLogic {
 public:
  static void initialize(PokemonCollection& collection);
  static bool chooseStarter(PokemonCollection& collection, uint16_t speciesId, uint32_t shinyRoll = 1);
  static bool addRegionalStarter(PokemonCollection& collection, uint16_t speciesId, uint32_t shinyRoll = 1);
  static OwnedPokemon createPokemon(uint32_t uid, uint16_t speciesId, uint8_t level,
                                    bool shiny = false, uint32_t personalitySeed = 0);
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
  static void refreshAbility(OwnedPokemon& pokemon);
  // Form 0 is the normal/default form. Deoxys supports explicit forms 0..3;
  // Unown is derived from personality and therefore cannot be changed.
  static uint8_t resolvedForm(const OwnedPokemon& pokemon);
  static const char* formName(uint16_t speciesId, uint8_t form);
  static bool canChangeForm(const OwnedPokemon& pokemon);
  static bool setForm(OwnedPokemon& pokemon, uint8_t form);
  static bool normalizeForm(OwnedPokemon& pokemon);
  static bool isShinyRoll(uint32_t roll) { return (roll & 8191U) == 0; }
  static OwnedPokemon* find(PokemonCollection& collection, uint32_t uid);
  static const OwnedPokemon* find(const PokemonCollection& collection, uint32_t uid);
  static OwnedPokemon* active(PokemonCollection& collection, uint8_t slot);
  static uint16_t count(const PokemonCollection& collection);
  static bool add(PokemonCollection& collection, OwnedPokemon pokemon, uint32_t* assignedUid = nullptr);
  // Commits an evolution only after its cinematic has completed. Nincada's
  // paired Shedinja is created here as part of the same operation; a full Box
  // still permits Ninjask but simply leaves bonusUid empty.
  static bool evolve(PokemonCollection& collection, uint32_t uid,
                     uint16_t newSpeciesId, uint32_t* bonusUid = nullptr);
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
