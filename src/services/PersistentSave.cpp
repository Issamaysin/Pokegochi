#include "services/PersistentSave.h"
#include <cstddef>
#include <cstring>

static_assert(sizeof(GameSave) < 8000, "GameSave no longer fits the redundant NVS layout");

bool PersistentSave::begin() { return preferences_.begin("pokegochi", false); }
uint32_t PersistentSave::crc32(const uint8_t* data, size_t length) const {
  uint32_t crc = 0xFFFFFFFFU;
  for (size_t i = 0; i < length; ++i) { crc ^= data[i]; for (uint8_t bit = 0; bit < 8; ++bit) crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U))); }
  return ~crc;
}
bool PersistentSave::readRecord(const char* key, Record& record) {
  return preferences_.getBytesLength(key) == sizeof(Record) && preferences_.getBytes(key, &record, sizeof(Record)) == sizeof(Record);
}
bool PersistentSave::readRecordV9(const char* key, RecordV9& record) {
  return preferences_.getBytesLength(key) == sizeof(RecordV9) &&
         preferences_.getBytes(key, &record, sizeof(RecordV9)) == sizeof(RecordV9) &&
         record.magic == kMagic && record.version == 9 && record.payloadSize == sizeof(GameSaveV9) &&
         record.crc == crc32(reinterpret_cast<const uint8_t*>(&record), offsetof(RecordV9, crc));
}
bool PersistentSave::readRecordV10(const char* key, RecordV10& record) {
  return preferences_.getBytesLength(key)==sizeof(RecordV10)&&preferences_.getBytes(key,&record,sizeof(record))==sizeof(record)&&
    record.magic==kMagic&&record.version==10&&record.payloadSize==sizeof(GameSaveV10)&&
    record.crc==crc32(reinterpret_cast<const uint8_t*>(&record),offsetof(RecordV10,crc));
}
bool PersistentSave::readRecordV11(const char* key, RecordV11& record) {
  return preferences_.getBytesLength(key)==sizeof(RecordV11)&&preferences_.getBytes(key,&record,sizeof(record))==sizeof(record)&&
    record.magic==kMagic&&record.version==11&&record.payloadSize==sizeof(GameSaveV11)&&
    record.crc==crc32(reinterpret_cast<const uint8_t*>(&record),offsetof(RecordV11,crc));
}
bool PersistentSave::valid(const Record& record) const {
  return record.magic == kMagic && record.version == kFormatVersion && record.payloadSize == sizeof(GameSave) &&
         record.crc == crc32(reinterpret_cast<const uint8_t*>(&record), offsetof(Record, crc));
}
bool PersistentSave::isNewer(uint32_t a, uint32_t b) const { return static_cast<int32_t>(a - b) > 0; }
SaveLoadResult PersistentSave::loadOrCreate(GameSave& save) {
  Record a{}, b{}; const bool validA = readRecord("save_a", a) && valid(a); const bool validB = readRecord("save_b", b) && valid(b);
  const bool gameWasStarted = preferences_.getBool("started", false);
  if (!validA && !validB) {
    RecordV11 v11a{},v11b{};const bool valid11a=readRecordV11("save_a",v11a),valid11b=readRecordV11("save_b",v11b);
    if(valid11a||valid11b){const RecordV11& old=valid11a&&(!valid11b||isNewer(v11a.sequence,v11b.sequence))?v11a:v11b;
      save=GameSave{};save.playTimeSeconds=old.payload.playTimeSeconds;save.bootCount=old.payload.bootCount;save.flags=old.payload.flags;save.activePetSlot=old.payload.activePetSlot;
      save.collection=old.payload.collection;save.inventory=old.payload.inventory;save.encounterCharges=old.payload.encounterCharges;save.wildEncounterClock=old.payload.wildEncounterClock;
      std::memcpy(&save.battle,&old.payload.battle,sizeof(BattleStateV10));save.pokedex=old.payload.pokedex;save.gymProgress=old.payload.gymProgress;save.money=old.payload.money;
      save.mart=old.payload.mart;save.moveLearning=old.payload.moveLearning;sequence_=old.sequence;nextSlotA_=true;if(!commit(save))return SaveLoadResult::StorageError;return SaveLoadResult::Loaded;}
    RecordV10 v10a{},v10b{};const bool valid10a=readRecordV10("save_a",v10a),valid10b=readRecordV10("save_b",v10b);
    if(valid10a||valid10b){const RecordV10& old=valid10a&&(!valid10b||isNewer(v10a.sequence,v10b.sequence))?v10a:v10b;
      save=GameSave{};save.playTimeSeconds=old.payload.playTimeSeconds;save.bootCount=old.payload.bootCount;save.flags=old.payload.flags;
      save.activePetSlot=old.payload.activePetSlot;save.collection=old.payload.collection;save.inventory=old.payload.inventory;
      save.encounterCharges=old.payload.encounterCharges;save.wildEncounterClock=old.payload.wildEncounterClock;std::memcpy(&save.battle,&old.payload.battle,sizeof(BattleStateV10));
      save.pokedex=old.payload.pokedex;save.gymProgress=old.payload.gymProgress;save.money=old.payload.money;save.mart=old.payload.mart;
      sequence_=old.sequence;nextSlotA_=true;if(!commit(save))return SaveLoadResult::StorageError;return SaveLoadResult::Loaded;}
    RecordV9 oldA{}, oldB{}; const bool validOldA = readRecordV9("save_a", oldA);
    const bool validOldB = readRecordV9("save_b", oldB);
    if (validOldA || validOldB) {
      const RecordV9& old = validOldA && (!validOldB || isNewer(oldA.sequence, oldB.sequence)) ? oldA : oldB;
      save = GameSave{}; save.playTimeSeconds=old.payload.playTimeSeconds; save.bootCount=old.payload.bootCount;
      save.flags=old.payload.flags; save.activePetSlot=old.payload.activePetSlot; save.collection=old.payload.collection;
      save.inventory=old.payload.inventory; save.encounterCharges=old.payload.encounterCharges;
      save.wildEncounterClock=old.payload.wildEncounterClock; save.battle.active=old.payload.battle.active;
      save.battle.kind=old.payload.battle.kind; save.battle.outcome=old.payload.battle.outcome;
      save.battle.playerUid=old.payload.battle.playerUid; std::memcpy(save.battle.opponents,old.payload.battle.opponents,sizeof(save.battle.opponents));
      save.battle.opponentCount=old.payload.battle.opponentCount; save.battle.opponentIndex=old.payload.battle.opponentIndex;
      save.battle.trainerProfileId=old.payload.battle.trainerProfileId; save.battle.gymId=old.payload.battle.gymId;
      save.battle.playerVolatile=old.payload.battle.playerVolatile; std::memcpy(save.battle.opponentVolatiles,old.payload.battle.opponentVolatiles,sizeof(save.battle.opponentVolatiles));
      save.battle.rngState=old.payload.battle.rngState; save.battle.turn=old.payload.battle.turn;
      save.battle.rewardMoney=old.payload.battle.rewardMoney; save.pokedex=old.payload.pokedex;
      save.gymProgress=old.payload.gymProgress; save.money=old.payload.money; save.mart=old.payload.mart;
      sequence_=old.sequence; nextSlotA_=true; if(!commit(save)) return SaveLoadResult::StorageError;
      return SaveLoadResult::Loaded;
    }
    if (gameWasStarted) return SaveLoadResult::Corrupted;
    save = GameSave{};
    if (!commit(save)) return SaveLoadResult::StorageError;
    if (preferences_.putBool("started", true) != 1) return SaveLoadResult::StorageError;
    return SaveLoadResult::FirstStart;
  }
  const bool chooseA = validA && (!validB || isNewer(a.sequence, b.sequence));
  const Record& selected = chooseA ? a : b; save = selected.payload; sequence_ = selected.sequence; nextSlotA_ = !chooseA;
  if (!gameWasStarted && preferences_.putBool("started", true) != 1) return SaveLoadResult::StorageError;
  return SaveLoadResult::Loaded;
}
bool PersistentSave::commit(const GameSave& save) {
  Record record{}; record.magic = kMagic; record.version = kFormatVersion; record.payloadSize = sizeof(GameSave); record.sequence = ++sequence_; record.payload = save;
  record.crc = crc32(reinterpret_cast<const uint8_t*>(&record), offsetof(Record, crc));
  const char* key = nextSlotA_ ? "save_a" : "save_b";
  if (preferences_.putBytes(key, &record, sizeof(Record)) != sizeof(Record)) return false;
  Record verification{};
  if (!readRecord(key, verification) || !valid(verification) || std::memcmp(&record, &verification, sizeof(Record)) != 0) return false;
  nextSlotA_ = !nextSlotA_; return true;
}

#if defined(POKEGOCHI_DEV_ALLOW_FACTORY_RESET)
bool PersistentSave::factoryResetForDevelopment() {
  sequence_ = 0;
  nextSlotA_ = true;
  return preferences_.clear();
}
#endif
