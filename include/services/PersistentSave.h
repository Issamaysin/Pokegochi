#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "game/BattleEngine.h"
#include "game/Pokedex.h"
#include "game/GymSystem.h"
#include "game/Economy.h"

struct PendingMoveLearning { uint32_t pokemonUid=0; MoveId move=MoveId::None; };
struct MoveLearningQueue { PendingMoveLearning entries[kPartyCapacity]{}; uint8_t count=0; uint8_t index=0; };

struct GameSave {
  uint32_t playTimeSeconds = 0;
  uint32_t bootCount = 0;
  uint8_t flags = 0;
  uint8_t activePetSlot = 0;
  PokemonCollection collection{};
  Inventory inventory{};
  EncounterCharges encounterCharges{};
  WildEncounterClock wildEncounterClock{};
  BattleState battle{};
  PokedexState pokedex{};
  GymProgress gymProgress{};
  uint32_t money = 3000;
  MartState mart{};
  MoveLearningQueue moveLearning{};
};

enum class SaveLoadResult : uint8_t {
  Loaded,
  FirstStart,
  Corrupted,
  StorageError,
};

class PersistentSave {
 public:
  bool begin();
  SaveLoadResult loadOrCreate(GameSave& save);
  bool commit(const GameSave& save);
#if defined(POKEGOCHI_DEV_ALLOW_FACTORY_RESET)
  bool factoryResetForDevelopment();
#endif
 private:
  static constexpr uint32_t kMagic = 0x504F4B45;
  static constexpr uint16_t kFormatVersion = 12;
  struct BattleStateV9 {
    bool active; BattleKind kind; BattleOutcome outcome; uint32_t playerUid;
    OwnedPokemon opponents[kOpponentTeamCapacity]; uint8_t opponentCount; uint8_t opponentIndex;
    uint8_t trainerProfileId; uint8_t gymId; CombatVolatile playerVolatile;
    CombatVolatile opponentVolatiles[kOpponentTeamCapacity]; uint32_t rngState; uint16_t turn; uint32_t rewardMoney;
  };
  struct GameSaveV9 {
    uint32_t playTimeSeconds; uint32_t bootCount; uint8_t flags; uint8_t activePetSlot;
    PokemonCollection collection; Inventory inventory; EncounterCharges encounterCharges;
    WildEncounterClock wildEncounterClock; BattleStateV9 battle; PokedexState pokedex;
    GymProgress gymProgress; uint32_t money; MartState mart;
  };
  struct RecordV9 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV9 payload; uint32_t crc; };
  struct BattleStateV10 {
    bool active; BattleKind kind; BattleOutcome outcome; uint32_t playerUid;
    OwnedPokemon opponents[kOpponentTeamCapacity]; uint8_t opponentCount; uint8_t opponentIndex;
    uint8_t trainerProfileId; uint8_t gymId; CombatVolatile playerVolatile;
    CombatVolatile opponentVolatiles[kOpponentTeamCapacity]; uint32_t rngState; uint16_t turn;
    uint32_t rewardMoney; uint8_t opponentItemUses;
  };
  struct GameSaveV10 {
    uint32_t playTimeSeconds; uint32_t bootCount; uint8_t flags; uint8_t activePetSlot;
    PokemonCollection collection; Inventory inventory; EncounterCharges encounterCharges;
    WildEncounterClock wildEncounterClock; BattleStateV10 battle; PokedexState pokedex;
    GymProgress gymProgress; uint32_t money; MartState mart;
  };
  struct RecordV10 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV10 payload; uint32_t crc; };
  struct GameSaveV11 {
    uint32_t playTimeSeconds; uint32_t bootCount; uint8_t flags; uint8_t activePetSlot;
    PokemonCollection collection; Inventory inventory; EncounterCharges encounterCharges;
    WildEncounterClock wildEncounterClock; BattleStateV10 battle; PokedexState pokedex;
    GymProgress gymProgress; uint32_t money; MartState mart; MoveLearningQueue moveLearning;
  };
  struct RecordV11 { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSaveV11 payload; uint32_t crc; };
  struct Record { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSave payload; uint32_t crc; };
  Preferences preferences_;
  uint32_t sequence_ = 0;
  bool nextSlotA_ = true;
  bool readRecord(const char* key, Record& record);
  bool readRecordV9(const char* key, RecordV9& record);
  bool readRecordV10(const char* key, RecordV10& record);
  bool readRecordV11(const char* key, RecordV11& record);
  bool valid(const Record& record) const;
  uint32_t crc32(const uint8_t* data, size_t length) const;
  bool isNewer(uint32_t a, uint32_t b) const;
};
