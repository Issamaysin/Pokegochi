#pragma once
#include <cstdint>
#include "game/Collection.h"

enum class PokeBallType : uint8_t { PokeBall, GreatBall, UltraBall, MasterBall, Count };
enum class BattleItem : uint8_t {
  Potion, SuperPotion, HyperPotion, FullHeal, Antidote, ParalyzeHeal,
  Awakening, BurnHeal, IceHeal, XAttack, XDefense, XSpeed, XAccuracy,
  DireHit, Revive, MaxRevive,
  Ether, MaxEther, Elixir, MaxElixir, PpUp,
  RareCandy,
  Count
};

constexpr uint8_t kStandardMedicineCount = 16;
constexpr uint8_t kPpItemCount = 5;
static_assert(static_cast<uint8_t>(BattleItem::PpUp)-static_cast<uint8_t>(BattleItem::Ether)+1U==kPpItemCount,
              "PP item inventory no longer matches BattleItem");

struct Inventory {
  uint16_t balls[static_cast<uint8_t>(PokeBallType::Count)] = {10, 3, 1, 0};
  uint16_t medicine[kStandardMedicineCount]{};
  // Mega Stone ownership uses GameSave::ownedMachines bit 63. Keeping this
  // array at the previous 38 entries preserves the complete save layout.
  uint16_t heldItems[kHeldItemInventorySlots]{};
};

// HeldItem::None can never be bought, equipped or returned to the Bag, so its
// existing count slot is persistent spare storage. Rare Candy deliberately
// uses that slot to remain save-compatible with every V34 beta board.
constexpr uint8_t kRareCandyInventorySlot = static_cast<uint8_t>(HeldItem::None);

// PP restoratives were introduced after the shipped Inventory layout. Keep
// them in a small appended save block so existing beta saves retain the exact
// offsets of balls, medicines and held items.
struct PpItemInventory {
  uint16_t quantities[kPpItemCount]{};
};

constexpr bool isPpInventoryItem(BattleItem item) {
  return item >= BattleItem::Ether && item <= BattleItem::PpUp;
}

constexpr uint8_t ppInventoryIndex(BattleItem item) {
  return static_cast<uint8_t>(item) - static_cast<uint8_t>(BattleItem::Ether);
}

struct EncounterCharges {
  static constexpr uint8_t kMaximum = 3;
  static constexpr uint32_t kMinimumRechargeSeconds = 30U * 60U;
  static constexpr uint32_t kRechargeWindowSeconds = 30U * 60U;
  uint8_t available = kMaximum;
  // Occupies legacy alignment padding before rechargeProgressSeconds, keeping
  // the persistent record layout unchanged. Each VS Seeker charge rolls a
  // new 30-60 minute target.
  uint16_t rechargeTargetSeconds = kMinimumRechargeSeconds;
  uint32_t rechargeProgressSeconds = 0;
};

struct WildEncounterClock {
  // Persistent layout retained from the former Wild-charge system. Wild
  // encounters are now unlimited; this record stores Pokemon Center energy.
  // One Pokemon Center charge is restored every hour, up to five.
  static constexpr uint32_t kMinimumSeconds = 60U * 60U;
  static constexpr uint32_t kWindowSeconds = 0U;
  static constexpr uint8_t kMaximum = 5;
  uint32_t elapsedSeconds = 0;
  uint32_t targetSeconds = kMinimumSeconds;
  uint32_t rngState = 0x91E10DA5U;
  uint8_t available = kMaximum;
  // Retained only to migrate an old one-shot pending encounter safely.
  bool pending = false;
};

enum class BattleOutcome : uint8_t { None, Ongoing, Victory, Defeat, Escaped, Captured };
// Append new kinds so existing serialized numeric values remain compatible.
enum class BattleKind : uint8_t {
  None, Wild, Trainer, Gym, League, Pvp, Tower,
  // Appended so every existing serialized BattleKind keeps its numeric value.
  MegaChallenge
};
enum class BattleWeather : uint8_t { Clear, Rain, Sun, Sandstorm, Hail, HeavyRain, HarshSun, StrongWinds };
// The first ten values intentionally match FireRed's gBattleTerrain indices.
// Gym/League visual variants resolve to Building for move mechanics.
enum class BattleTerrain : uint8_t {
  Grass, LongGrass, Sand, Underwater, Water, Pond, Mountain, Cave, Building, Plain
};

constexpr uint8_t kOpponentTeamCapacity = 3;
constexpr uint8_t kEvolutionChoiceCapacity = 5;
// FireRed stores battle stages as 0..12 with 6 as neutral. Pokegochi stores
// the equivalent signed representation, so every ordinary battle stat is
// constrained to -6..+6.
constexpr int8_t kMinimumBattleStatStage = -6;
constexpr int8_t kMaximumBattleStatStage = 6;

struct CombatVolatile {
  int8_t attackStage = 0, defenseStage = 0, spAttackStage = 0, spDefenseStage = 0;
  int8_t speedStage = 0, accuracyStage = 0, evasionStage = 0;
  uint8_t confusionTurns = 0, trappedTurns = 0;
  uint8_t criticalStage = 0;
  bool flinched = false, protectedThisTurn = false, recharging = false;
  uint16_t substituteHp = 0;
  MoveId disabledMove = MoveId::None, encoreMove = MoveId::None, chargingMove = MoveId::None;
  MoveId lastMoveUsed = MoveId::None;
  MoveId choiceMove = MoveId::None;
  // Equipment manipulation is battle-local outside PvP. This keeps a wild
  // encounter or temporary trainer from permanently deleting scarce gear.
  HeldItem heldItemOverride = HeldItem::None;
  bool hasHeldItemOverride = false;
  bool heldItemSuppressed = false;
  bool seeded = false;
  uint8_t toxicCounter = 0;
  uint8_t sleepTurns = 0;
  // FireRed stores one shared 0..3 counter for STOCKPILE, SPIT UP and
  // SWALLOW. This occupies the former trailing alignment byte, so persistent
  // BattleState size/layout remains compatible with the current save.
  uint8_t stockpileCount = 0;
  // Packed Gen-III volatile byte. Bits 0..1 store the Mind Reader / Lock-On
  // owner; bit 2 is Ghost Curse; bit 3 is escape prevention (Mean Look);
  // bits 4..6 store Disable's remaining 2..5-turn timer; bit 7 marks the
  // setup turn of Lock-On so its one-turn window expires exactly. Packing
  // these states keeps the persistent BattleState byte-for-byte compatible.
  uint8_t sureHitTurns = 0;
};

struct BattleHeldItemState {
  uint32_t pokemonUid = 0;
  HeldItem heldItemOverride = HeldItem::None;
  bool hasHeldItemOverride = false;
  bool heldItemSuppressed = false;
};

// Gen-III delayed attacks belong to a battlefield position, not to the
// individual Pokemon occupying it. Damage is frozen when the move is set;
// accuracy and the final random roll are resolved only when it lands.
struct DelayedAttackState {
  MoveId move = MoveId::None;
  uint16_t baseDamage = 0;
  uint16_t dueTurn = 0;
};

// Bide is a multi-turn lock whose stored damage belongs to the battlefield
// occupant. It cannot share chargingMove/substituteHp: both may legitimately
// coexist with Bide in FireRed.
struct BideState {
  uint16_t damage = 0;
  uint8_t turns = 0;
};

// Battle-local state for move effects that are neither major status nor stat
// stages.  These flags intentionally live outside CombatVolatile so the V31
// save layout can be migrated as one intact prefix and every formerly missing
// FireRed handler has an explicit, inspectable home.
struct DedicatedMoveEffectState {
  bool typeOverrideActive = false;
  PokemonType type1 = PokemonType::Normal;
  PokemonType type2 = PokemonType::Normal;
  bool defenseCurl = false;
  uint8_t chargeTurns = 0;
  bool destinyBond = false;
  uint8_t perishTurns = 0;
  bool ingrained = false;
  HeldItem recyclableItem = HeldItem::None;
  bool imprisoned = false;
  bool grudge = false;
  bool mudSport = false;
  bool waterSport = false;
  // FLASH FIRE is activated by absorbing a Fire move and boosts this
  // battler's own Fire attacks until it leaves the field.
  bool flashFireBoost = false;
  // Additional Gen-III volatile effects. These are explicit instead of
  // falling through as ordinary power-0 moves.
  bool identified = false;
  uint8_t tauntTurns = 0;
  bool tormented = false;
  uint8_t yawnTurns = 0;
  uint16_t lastDamageReceived = 0;
  MoveId lastDamagingMove = MoveId::None;
  bool lastDamageWasPhysical = false;
  // Persistent equivalent of Gen III's gLastLandedMoves/gLastHitByType.
  // Counter-style damage memory is cleared every turn; Conversion 2's last
  // landed move instead survives turns until the battler leaves the field.
  MoveId lastLandedMove = MoveId::None;
  PokemonType lastLandedType = PokemonType::Normal;
  uint8_t furyCutterCount = 0;
  bool rageActive = false;
  MoveId lockedMove = MoveId::None;
  uint8_t lockedMoveTurns = 0;
  bool uproar = false;
  bool minimized = false;
  uint8_t rolloutCount = 0;
  uint16_t enteredTurn = 0;
  uint32_t attractedToUid = 0;
  bool magicCoat = false;
  bool snatch = false;
  // Occupies former tail padding, preserving the V33 ABI while making the
  // Gen-III Mimic/Sketch transformed-user rejection explicit on both sides.
  bool transformed = false;
};

struct BattleRecyclableItemState {
  uint32_t pokemonUid = 0;
  HeldItem item = HeldItem::None;
};

// Transform copies the target's already-calculated battle stats and IVs, but
// not its HP, level, personality or held item.  Keeping those copied values in
// BattleState avoids mutating the persistent IV/EV/nature training data of a
// player's Pokemon and also lets an interrupted battle resume faithfully.
struct TransformBattleSnapshot {
  bool active = false;
  uint16_t attack = 0;
  uint16_t defense = 0;
  uint16_t spAttack = 0;
  uint16_t spDefense = 0;
  uint16_t speed = 0;
  IndividualValues ivs{};
  // Opponents live inside BattleState, so their temporary identity still has
  // to be restored when Transform ends on a switch.  The player already has
  // the older dedicated original-identity fields retained for compatibility.
  uint16_t originalSpeciesId = 0;
  uint8_t originalForm = 0;
  uint8_t originalAbilityId = 0;
  MoveId originalMoves[kMoveSlots]{};
  uint8_t originalMovePp[kMoveSlots]{};
};

struct BattleState {
  bool active = false;
  BattleKind kind = BattleKind::None;
  BattleOutcome outcome = BattleOutcome::None;
  uint32_t playerUid = 0;
  OwnedPokemon opponents[kOpponentTeamCapacity]{};
  uint8_t opponentCount = 0;
  uint8_t opponentIndex = 0;
  uint8_t trainerProfileId = 0xFF;
  uint8_t gymId = 0xFF;
  CombatVolatile playerVolatile{};
  BattleHeldItemState playerHeldItems[kPartyCapacity]{};
  CombatVolatile opponentVolatiles[kOpponentTeamCapacity]{};
  uint32_t rngState = 0xA341316CU;
  uint16_t turn = 0;
  uint32_t rewardMoney = 0;
  uint8_t opponentItemUses = 0;
  uint8_t gymStage = 0;
  uint8_t leagueRegion = 0;
  uint8_t leagueStage = 0;
  uint8_t unlockedGeneration = 1;
  BattleWeather weather = BattleWeather::Clear;
  uint8_t weatherTurns = 0;
  // Transform is battle-local.  The active player's original identity and
  // moves are retained here so a save/reboot in the middle of battle can be
  // restored without ever changing the collection permanently.
  bool playerTransformed = false;
  uint16_t playerOriginalSpeciesId = 0;
  uint8_t playerOriginalAbilityId = 0;
  MoveId playerOriginalMoves[kMoveSlots]{};
  uint8_t playerOriginalMovePp[kMoveSlots]{};
  bool playerAbilityTraced = false;
  uint8_t playerPreTraceAbilityId = 0;
  DelayedAttackState delayedToPlayer{};
  DelayedAttackState delayedToOpponent{};
  // Safeguard is a side condition, not a property of the active Pokemon. It
  // therefore survives switches and protects every member sent out while the
  // five-turn counter remains active.
  uint8_t playerSafeguardTurns = 0;
  uint8_t opponentSafeguardTurns = 0;
  uint8_t playerReflectTurns = 0;
  uint8_t opponentReflectTurns = 0;
  uint8_t playerLightScreenTurns = 0;
  uint8_t opponentLightScreenTurns = 0;
  uint8_t playerMistTurns = 0;
  uint8_t opponentMistTurns = 0;
  uint8_t playerProtectChain = 0;
  uint8_t opponentProtectChain = 0;
  bool playerEndureThisTurn = false;
  bool opponentEndureThisTurn = false;
  // NIGHTMARE is tied to the current battler and disappears on waking or
  // switching. Opponent state is indexed because NPC/PvP teams keep distinct
  // battle-local occupants.
  bool playerNightmare = false;
  bool opponentNightmares[kOpponentTeamCapacity]{};
  BideState playerBide{};
  BideState opponentBides[kOpponentTeamCapacity]{};
  uint8_t playerEncoreTurns = 0;
  uint8_t opponentEncoreTurns[kOpponentTeamCapacity]{};
  // Keep the V31 BattleState's two trailing alignment bytes intact so the
  // persistent prefix remains byte-for-byte migratable.
  alignas(4) DedicatedMoveEffectState playerMoveEffects{};
  BattleRecyclableItemState playerRecyclableItems[kPartyCapacity]{};
  DedicatedMoveEffectState opponentMoveEffects[kOpponentTeamCapacity]{};
  // Wish belongs to the battlefield position and therefore survives a switch.
  uint16_t playerWishDueTurn = 0;
  uint16_t opponentWishDueTurn = 0;
  // Camouflage uses the terrain chosen when the battle begins.
  PokemonType terrainType = PokemonType::Normal;
  uint8_t playerSpikesLayers = 0;
  uint8_t opponentSpikesLayers = 0;
  // Occupies the alignment byte immediately before payDayMoney, preserving
  // the serialized BattleState size and all following offsets.
  BattleTerrain terrain = BattleTerrain::Plain;
  uint32_t payDayMoney = 0;
  bool playerMimicActive = false;
  uint32_t playerMimicUid = 0;
  uint8_t playerMimicSlot = 0;
  MoveId playerMimicOriginalMove = MoveId::None;
  uint8_t playerMimicOriginalPp = 0;
  // Appended in save V34.  Appending rather than inserting preserves V33 as
  // an exact byte prefix for lossless migration of existing beta saves.
  alignas(4) TransformBattleSnapshot playerTransformSnapshot{};
  TransformBattleSnapshot opponentTransformSnapshot{};
};

enum class BattleEventType : uint8_t {
  MoveUsed, MoveMissed, HpChanged, CriticalHit, Effectiveness,
  StatusApplied, StatusCured, StatChanged, MoveEffect, CannotMove, ItemUsed, HeldItemActivated,
  DelayedAttackHit, Fainted, SwitchedIn, ExperienceGained, BattleEnded, LevelUp, PokemonAdded,
  // Appended so older numeric journal values remain stable. MultiHitStrike
  // is a silent presentation marker that replays the move animation before
  // every strike after the first; MultiHitCount owns FireRed's final
  // "Hit X times!" message.
  MultiHitStrike, MultiHitCount,
  // Appended for save/journal compatibility. `value` stores the Ability id;
  // the following HP/status/stat event carries the mechanical consequence.
  AbilityActivated
};

enum class BattleMoveEffect : uint8_t {
  Confused = 1, Seeded, Trapped, Protected, Substitute, Disabled, Encore,
  Transformed, FutureAttackSet, Failed, AbilityCopied, Recoil,
  SafeguardSet, SafeguardEnded, SafeguardBlocked,
  ReflectSet, ReflectEnded, LightScreenSet, LightScreenEnded, MistSet, MistEnded,
  Endured, WeatherSet, TeamCured, HazeCleared, NightmareApplied, NightmareHurt,
  Stockpiled, StockpileReleased, Swallowed,
  HeldItemStolen, HeldItemsSwapped, HeldItemKnockedOff,
  EncoreEnded, BideStoring, BideUnleashed, BatonPass,
  Teleported, TypeChanged, DestinyBondSet, DestinyBondTriggered, PerishSongSet, PerishCount,
  Charged, WishSet, WishGranted, Ingrained, Recycled, Imprisoned, GrudgeSet,
  MudSportSet, WaterSportSet, Identified, Taunted, Tormented, Drowsy,
  SpikesSet, PpReduced, AbilitiesSwapped, CoinsScattered, RampageEnded,
  UproarStarted, UproarEnded, MoveCopied, Infatuated,
  MagicCoatSet, MagicCoatReflected, SnatchSet, MoveSnatched, FocusEnergySet,
  Cursed, CurseHurt, DisableEnded, SureHitSet, PainShared, StatsCopied,
  // Keep appended: serialized action journals and older saves retain every
  // pre-existing numeric value. before/after carry Castform's 0..3 form.
  FormChanged,
  // OHKO has two independent failure gates after accuracy in Gen III.
  // Keep them distinct in the journal so Mind Reader success is never
  // followed by the misleading generic "But it failed!" presentation.
  OhkoTargetHigherLevel, OhkoBlockedBySturdy,
  // Primary status moves check an existing major status before accuracy in
  // FireRed.  Keep the four original dedicated messages distinct from the
  // generic failure used when a different major status is already present.
  AlreadyAsleep, AlreadyPoisoned, AlreadyParalyzed, AlreadyBurned,
  // An Ability acquired or reactivated in battle immediately removes the
  // condition it forbids (for example LIMBER after Skill Swap). `before`
  // stores the Ability id so presentation can name the passive precisely.
  AbilityCured,
  // Appended for journal/save compatibility. These effects used to clear
  // mechanically without reaching the presentation queue, so the player
  // never saw FireRed's end messages.
  ConfusionEnded, WeatherEnded,
  // FireRed announces and animates confusion at the start of every turn in
  // which it remains active, independently of whether self-damage is rolled.
  // Appended to preserve all older journal values.
  ConfusionActive,
  // Damaging weather has two distinct FireRed presentation phases: one
  // field-wide continuation message/animation, followed by an individual
  // message and HP change for every susceptible battler.  Keep both events
  // explicit so residual damage cannot silently jump the HUD.
  WeatherContinues, WeatherHurt
};

enum class BattleSide : uint8_t { Player, Opponent };
enum class BattleEntryScope : uint8_t { Both, Player, Opponent };

struct BattleEvent {
  BattleEventType type = BattleEventType::MoveUsed;
  BattleSide side = BattleSide::Player;
  MoveId move = MoveId::None;
  StatusCondition status = StatusCondition::None;
  uint32_t pokemonUid = 0;
  uint16_t value = 0;
  uint16_t before = 0;
  uint16_t after = 0;
  uint16_t effectiveness100 = 100;
  int8_t stageBefore = 0;
  int8_t stageAfter = 0;
};

constexpr uint8_t kBattleEventCapacity = 40;
constexpr uint8_t kLevelUpSummaryCapacity = 12;

// FireRed shows two level-up pages: the six gains, followed by the six new
// totals. Keep this compact snapshot in the action journal because the live
// Pokemon may already have evolved by the time presentation reaches it.
struct LevelUpSummary {
  uint32_t pokemonUid = 0;
  uint16_t speciesId = 0;
  uint8_t newLevel = 0;
  uint8_t gains[6]{};      // HP, ATK, DEF, SP.ATK, SP.DEF, SPEED
  uint16_t stats[6]{};
};

struct BattleActionResult {
  bool accepted = false;
  bool hit = false;
  bool enemyActed = false;
  MoveId enemyMoveUsed = MoveId::None;
  bool caught = false;
  // RUN is deliberately resolved after the wild Pokemon's action. These
  // flags let the presentation show the enemy move first and the escape
  // verdict afterwards, instead of collapsing both outcomes into damage text.
  bool escapeFailed = false;
  bool escapeBlocked = false;
  uint8_t captureShakes = 0;
  bool opponentDefeated = false;
  bool evolved = false;
  uint16_t evolvedSpeciesId = 0;
  uint8_t evolvedCount = 0;
  uint16_t evolvedSpeciesIds[kPartyCapacity]{};
  uint16_t evolvedFromSpeciesIds[kPartyCapacity]{};
  bool evolvedShiny[kPartyCapacity]{};
  uint8_t pendingEvolutionCount = 0;
  uint32_t pendingEvolutionUids[kPartyCapacity]{};
  uint16_t pendingEvolutionChoices[kPartyCapacity][kEvolutionChoiceCapacity]{};
  uint8_t pendingEvolutionChoiceCounts[kPartyCapacity]{};
  // Species created as a secondary consequence of evolution (Shedinja).
  // Kept outside the bounded presentation journal so Pokédex registration
  // remains reliable even on a turn that fills every visual event slot.
  uint16_t bonusPokemonSpeciesId = 0;
  bool statusApplied = false;
  StatusCondition appliedStatus = StatusCondition::None;
  uint16_t damageDealt = 0;
  uint16_t damageTaken = 0;
  uint16_t experienceGained = 0;
  uint8_t levelUpCount = 0;
  LevelUpSummary levelUps[kLevelUpSummaryCapacity]{};
  uint16_t effectiveness100 = 100;
  bool criticalHit = false;
  bool heldItemStolen = false;
  bool heldItemSwapped = false;
  bool heldItemKnockedOff = false;
  bool enemyItemUsed = false;
  BattleItem enemyItem = BattleItem::Potion;
  HeldItem affectedHeldItem = HeldItem::None;
  // BATON PASS resolves in two stages: the move is announced first, then the
  // player chooses the incoming party member. The opponent's already-chosen
  // command is retained here and executes against that replacement.
  bool batonPassSwitchRequired = false;
  uint8_t batonPassOpponentMoveSlot = 0xFF;
  // A Berry/Leftovers trigger is surfaced to the UI so it can reproduce the
  // original held-item ring/sparkle animation rather than silently changing
  // HP or a status flag inside the battle engine.
  uint8_t heldItemActivationCount = 0;
  uint32_t heldItemActivationUids[4]{};
  HeldItem heldItemsActivated[4]{};
  uint32_t moneyGained = 0;
  uint8_t movesToLearnCount = 0;
  uint32_t moveLearnerUids[kPartyCapacity]{};
  MoveId movesToLearn[kPartyCapacity]{};
  BattleOutcome outcome = BattleOutcome::None;
  // Ordered presentation journal. The engine resolves a complete turn, but
  // the UI consumes these events one at a time exactly in battle order.
  uint8_t eventCount = 0;
  BattleEvent events[kBattleEventCapacity]{};
};

class EncounterLogic {
 public:
  static void advance(EncounterCharges& charges, uint32_t elapsedSeconds);
  static bool consumeManualCharge(EncounterCharges& charges);
  static uint32_t secondsUntilNext(const EncounterCharges& charges);
  static void advanceWild(WildEncounterClock& clock, uint32_t elapsedSeconds);
  static bool consumeWildCharge(WildEncounterClock& clock);
  static uint32_t secondsUntilNextWild(const WildEncounterClock& clock);
  // Names used by current gameplay; the Wild-named methods remain as binary
  // and migration-compatible aliases for older tests/saves.
  static void advanceCenter(WildEncounterClock& charges, uint32_t elapsedSeconds) { advanceWild(charges, elapsedSeconds); }
  static bool consumeCenterCharge(WildEncounterClock& charges) { return consumeWildCharge(charges); }
  static uint32_t secondsUntilNextCenter(const WildEncounterClock& charges) { return secondsUntilNextWild(charges); }
};

class BattleEngine {
 public:
  static void clear(BattleState& battle);
  static uint8_t highestPartyLevel(const PokemonCollection& collection);
  static bool startWild(BattleState& battle, PokemonCollection& collection,
                        uint32_t playerUid, uint32_t seed, uint8_t unlockedGeneration = 1,
                        bool beginnerProtection = false);
  static bool startTrainer(BattleState& battle, EncounterCharges& charges,
                           PokemonCollection& collection, uint32_t playerUid,
                           uint32_t seed, uint8_t unlockedGeneration = 1);
  static OwnedPokemon* currentOpponent(BattleState& battle);
  static const OwnedPokemon* currentOpponent(const BattleState& battle);
  // Opponents need stable, non-zero identities because the engine resolves a
  // whole turn before the UI presents it.  The presentation journal uses
  // these IDs to keep the defeated battler on screen until its HP/faint
  // sequence has finished instead of leaking the next trainer Pokemon early.
  // NPC teams are presented weakest-to-strongest so their highest-level ace
  // is always the final Pokemon sent out. The sort is stable, preserving an
  // authored ace placed last when teammates share its level.
  static void orderOpponentTeamWeakestFirst(BattleState& battle);
  static bool ensureOpponentUids(BattleState& battle);
  static bool moveIsImprisoned(const BattleState& battle,const PokemonCollection& collection,
                               MoveId move,BattleSide userSide);
  static bool moveIsSelectable(const BattleState& battle,const PokemonCollection& collection,
                               MoveId move,BattleSide userSide);
  static BattleActionResult fight(BattleState& battle, PokemonCollection& collection, uint8_t moveSlot);
  // Stack-safe form used by the ESP32 UI. BattleActionResult contains a
  // 40-event journal and should be supplied from persistent scratch storage,
  // not materialised on loopTask's small stack.
  static void fightInto(BattleState& battle, PokemonCollection& collection, uint8_t moveSlot,
                        BattleActionResult& result);
  // Link battles use the same FireRed resolver, but the opponent move comes
  // from the peer instead of the trainer AI. 0xFE means that the opponent
  // spent the turn switching and therefore must not receive a second action.
  static BattleActionResult fightPvp(BattleState& battle, PokemonCollection& collection,
                                     uint8_t moveSlot, uint8_t opponentMoveSlot);
  static BattleActionResult run(BattleState& battle, PokemonCollection& collection);
  // Stack-safe firmware form. The complete event journal is written into
  // caller-owned storage instead of materialising a ~1 KiB return object on
  // Arduino's loopTask stack.
  static void runInto(BattleState& battle, PokemonCollection& collection,
                      BattleActionResult& result);
  static BattleActionResult switchPokemon(BattleState& battle, PokemonCollection& collection);
  static BattleActionResult switchToPokemon(BattleState& battle, PokemonCollection& collection, uint32_t uid,
                                             bool forcedAfterFaint = false);
  static BattleActionResult completeBatonPassSwitch(BattleState& battle,
                                                     PokemonCollection& collection,
                                                     uint32_t uid,
                                                     uint8_t opponentMoveSlot);
  static BattleActionResult switchToPokemonPvp(BattleState& battle, PokemonCollection& collection,
                                                uint32_t uid, uint8_t opponentMoveSlot,
                                                bool forcedAfterFaint = false);
  static BattleActionResult switchOpponentPvp(BattleState& battle, PokemonCollection& collection,
                                               uint8_t opponentIndex);
  static BattleActionResult forfeitPvp(BattleState& battle, PokemonCollection& collection,
                                       bool localPlayerForfeits);
  static BattleActionResult throwBall(BattleState& battle, PokemonCollection& collection,
                                      Inventory& inventory, PokeBallType ball);
  static void throwBallInto(BattleState& battle, PokemonCollection& collection,
                            Inventory& inventory, PokeBallType ball,
                            BattleActionResult& result);
  static BattleActionResult useItem(BattleState& battle, PokemonCollection& collection,
                                    Inventory& inventory, BattleItem item, uint32_t targetUid = 0,
                                    PpItemInventory* ppItems = nullptr, uint8_t moveSlot = 0xFF);
  // Stack-safe firmware path. BattleActionResult embeds the complete
  // presentation journal, so the caller supplies persistent scratch instead
  // of materialising it inside the small Arduino loop-task stack.
  static void useItemInto(BattleState& battle, PokemonCollection& collection,
                          Inventory& inventory, BattleItem item,
                          BattleActionResult& result, uint32_t targetUid = 0,
                          PpItemInventory* ppItems = nullptr, uint8_t moveSlot = 0xFF);
  // FireRed field behavior shared by SOFT-BOILED and MILK DRINK: transfer
  // one fifth of the user's maximum HP to another, non-fainted party member.
  // Field use does not consume battle PP.
  static bool useFieldSoftBoiled(PokemonCollection& collection, uint32_t userUid,
                                 uint32_t targetUid);
  static const char* ballName(PokeBallType ball);
  static const char* itemName(BattleItem item);
  static void applyEntryAbilities(BattleState& battle, PokemonCollection& collection,
                                  BattleEntryScope scope = BattleEntryScope::Both,
                                  BattleActionResult* result = nullptr);
  // Shared by reflected Magic Coat moves; public to let the compact internal
  // resolver reuse the exact same status/Ability/Safeguard path.
  static void applyMoveStatus(BattleState& battle, OwnedPokemon& source, MoveId move,
                              OwnedPokemon& target, BattleActionResult& result,
                              bool targetHadSubstitute = false);

 private:
  static uint32_t random(BattleState& battle);
  static uint16_t confusionSelfDamage(BattleState& battle,
                                  const OwnedPokemon& pokemon,
                                  const CombatVolatile& volatileState);
  static uint16_t calculateDamage(BattleState& battle, const OwnedPokemon& attacker,
                                  const OwnedPokemon& defender, MoveId move,
                                  const CombatVolatile& attackerVolatile,
                                  const CombatVolatile& defenderVolatile,
                                  uint16_t* effectivenessOut=nullptr,bool* criticalOut=nullptr,
                                  bool allowCritical=true,uint16_t powerOverride=0);
  static uint8_t chooseDamageHitCount(BattleState& battle,
                                  const OwnedPokemon& attacker, MoveId move);
  static uint16_t calculateDelayedBaseDamage(const BattleState& battle,
                                  const OwnedPokemon& attacker, const OwnedPokemon& defender,
                                  MoveId move, const CombatVolatile& attackerVolatile,
                                  const CombatVolatile& defenderVolatile);
  static void resolveDelayedAttacks(BattleState& battle, PokemonCollection& collection,
                                    BattleActionResult& result);
  static void resolveNightmareTurn(BattleState& battle, PokemonCollection& collection,
                                   BattleActionResult& result);
  static void advanceEndTurnEffects(BattleState& battle, PokemonCollection& collection,
                                    BattleActionResult& result);
  static void enemyTurn(BattleState& battle, PokemonCollection& collection,
                        OwnedPokemon& player, BattleActionResult& result, uint8_t moveSlot = 0xFF);
  static uint8_t chooseEnemyMove(BattleState& battle, const OwnedPokemon& player);
  static void awardExperience(BattleState& battle, PokemonCollection& collection, BattleActionResult& result);
};
