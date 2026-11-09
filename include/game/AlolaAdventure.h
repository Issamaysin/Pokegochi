#pragma once
#include <cstdint>
#include "game/BattleEngine.h"
#include "game/HeldItems.h"
#include "game/KalosAdventure.h"

enum class AlolaArea : uint8_t {
  VerdantCavern, MelemeleGrandTrial, BrookletHill, WelaVolcano,
  LushJungle, AkalaGrandTrial, HokulaniObservatory, ThriftyMegamart,
  UlaulaGrandTrial, VastPoniCanyon, PoniGrandTrial, PokemonLeague,
  UltraWormhole, RainbowRocket, Count,
};
enum class AlolaRoomKind : uint8_t {
  Trainer, Grass, SosChain, IslandScan, Item, Healing, TeamSkull,
  Aether, UltraWormhole, Trial,
};
enum class AlolaBattleRole : uint8_t {None, Wild, Trainer, Skull, Aether, Totem};

struct AlolaRoomChoice {AlolaRoomKind kind=AlolaRoomKind::Trainer;uint8_t route=0;bool recommended=false;};
struct AlolaRoomResult {
  AlolaRoomKind kind=AlolaRoomKind::Trainer;bool startsBattle=false,healed=false,
      foundItem=false,hiddenAbilityEncounter=false,legendaryEncounter=false;
  uint16_t speciesId=0;uint8_t rewardKind=0;
};
struct AlolaRunState {
  uint32_t seed=0;uint8_t area=0,step=0,flags=0;
  AlolaBattleRole pendingBattle=AlolaBattleRole::None;uint8_t pendingFlags=0;
  uint16_t pendingSpecies=0;uint32_t partyUids[kPartyCapacity]{};
};
struct AlolaAdventureState {
  uint32_t rngState=0x414C4F4CUL; // ALOL
  uint16_t unlockedAreas=0,defeatedBosses=0;
  uint8_t flags=0;
  AlolaRunState run{};
};

class AlolaAdventure {
 public:
  static constexpr uint8_t kAreaCount=static_cast<uint8_t>(AlolaArea::Count);
  static constexpr uint8_t kStepsPerArea=5U;
  static constexpr uint16_t kAllAreasMask=(1U<<kAreaCount)-1U;
  static constexpr uint8_t kChampionCompleteBit=1U<<0U;
  static constexpr uint8_t kSunThemeSelectedBit=1U<<1U;
  static constexpr uint8_t kMasterBallRewardBit=1U<<2U;
  static constexpr uint8_t kMoonThemeSelectedBit=1U<<3U;
  // Ordinary wild rooms can reveal a route-specific Gen-VII Legendary or
  // Mythical. Ultra Beasts are excluded and belong to explicit wormhole rooms.
  // This mirrors Kalos' 1/240 hunt without mixing generations or mechanics.
  static constexpr uint16_t kLegendaryChanceDenominator=240U;

  static bool available(const KalosAdventureState& kalos);
  static bool reconcile(AlolaAdventureState& state,const KalosAdventureState& kalos);
  static bool isAreaUnlocked(const AlolaAdventureState& state,AlolaArea area);
  static bool championCompleted(const AlolaAdventureState& state);
  static bool fullyCompleted(const AlolaAdventureState& state);
  static bool reconcileCompletionReward(AlolaAdventureState& state,Inventory& inventory);
  static bool sunThemeUnlocked(const AlolaAdventureState& state);
  static bool sunThemeSelected(const AlolaAdventureState& state);
  static bool setSunThemeSelected(AlolaAdventureState& state,bool selected);
  static bool moonThemeUnlocked(const AlolaAdventureState& state);
  static bool moonThemeSelected(const AlolaAdventureState& state);
  static bool setMoonThemeSelected(AlolaAdventureState& state,bool selected);
  static uint8_t trialCount(const AlolaAdventureState& state);
  static const char* areaName(AlolaArea area);
  static const char* areaSubtitle(AlolaArea area);
  static const char* bossName(AlolaArea area);
  static HeldItem areaZCrystal(AlolaArea area);
  static HeldItem areaBonusZCrystal(AlolaArea area);

  static bool begin(AlolaAdventureState& state,AlolaArea area,uint32_t seed,
                    const PokemonCollection& collection);
  static bool willPreserveChoicesOnAbandon(
      const AlolaAdventureState& state);
  static bool abandon(AlolaAdventureState& state);
  static bool active(const AlolaAdventureState& state);
  static uint8_t choices(const AlolaAdventureState& state,AlolaRoomChoice output[2]);
  static AlolaRoomResult choose(AlolaAdventureState& state,uint8_t choice,
                               PokemonCollection& collection);
  static bool startPendingBattle(AlolaAdventureState& state,BattleState& battle,
                                 PokemonCollection& collection,uint32_t playerUid);
  static bool recordBattle(AlolaAdventureState& state,BattleOutcome outcome,
                           Gen7UniqueItemInventory& items);
  static bool useAreaHeal(AlolaAdventureState& state,PokemonCollection& collection);
};
