#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "game/BattleEngine.h"
#include "game/Pokedex.h"
#include "game/GymSystem.h"
#include "game/Economy.h"

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
  static constexpr uint16_t kFormatVersion = 9;
  struct Record { uint32_t magic; uint16_t version; uint16_t payloadSize; uint32_t sequence; GameSave payload; uint32_t crc; };
  Preferences preferences_;
  uint32_t sequence_ = 0;
  bool nextSlotA_ = true;
  bool readRecord(const char* key, Record& record);
  bool valid(const Record& record) const;
  uint32_t crc32(const uint8_t* data, size_t length) const;
  bool isNewer(uint32_t a, uint32_t b) const;
};
