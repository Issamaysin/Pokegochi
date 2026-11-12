#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "services/PersistentSave.h"

namespace {

constexpr uint32_t kSaveMagic = 0x504F4B45U;
constexpr uint16_t kCurrentSaveVersion = PersistentSave::formatVersion();
constexpr uint16_t kCurrentPayloadSize = sizeof(GameSave);
constexpr size_t kHeaderSize = 16U;
constexpr uint16_t kStarterSpecies[] = {
    1U, 4U, 7U, 152U, 155U, 158U, 252U, 255U, 258U};

uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFFFFFFU;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8U; ++bit)
      crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
  }
  return ~crc;
}

template <typename T>
T readValue(const std::vector<uint8_t>& bytes, size_t offset) {
  T value{};
  if (offset + sizeof(value) <= bytes.size())
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
  return value;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: inspect_save_record <pokegochi_save_a.bin> [...]\n";
    return 2;
  }

  int result = 0;
  for (int argument = 1; argument < argc; ++argument) {
    const std::string path = argv[argument];
    std::ifstream input(path, std::ios::binary);
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                                     std::istreambuf_iterator<char>());
    if (!input && bytes.empty()) {
      std::cerr << path << ": read failed\n";
      result = 1;
      continue;
    }

    const uint32_t magic = readValue<uint32_t>(bytes, 0U);
    const uint16_t version = readValue<uint16_t>(bytes, 4U);
    const uint16_t payloadSize = readValue<uint16_t>(bytes, 6U);
    const uint32_t sequence = readValue<uint32_t>(bytes, 8U);
    const size_t crcOffset = kHeaderSize + payloadSize;
    const uint32_t storedCrc = readValue<uint32_t>(bytes, crcOffset);
    const bool shapeValid = magic == kSaveMagic &&
        version == kCurrentSaveVersion &&
        payloadSize == kCurrentPayloadSize &&
        bytes.size() >= crcOffset + 2U * sizeof(uint32_t) &&
        bytes.size() <= crcOffset + 3U * sizeof(uint32_t);
    const bool crcValid = shapeValid && storedCrc == crc32(bytes.data(), crcOffset);

    GameSave save{};
    if (shapeValid && kHeaderSize + sizeof(save) <= bytes.size())
      std::memcpy(&save, bytes.data() + kHeaderSize, sizeof(save));
    const PokemonCollection& collection = save.collection;
    const uint32_t playTime = readValue<uint32_t>(bytes, kHeaderSize);
    const uint32_t bootCount = readValue<uint32_t>(bytes, kHeaderSize + 4U);

    uint16_t occupied = 0U;
    uint16_t validSpecies = 0U;
    uint16_t invalidSpecies = 0U;
    std::cout << path << "\n"
              << "  size=" << bytes.size()
              << " version=" << version
              << " payload=" << payloadSize
              << " sequence=" << sequence
              << " shape=" << (shapeValid ? "ok" : "bad")
              << " crc=" << (crcValid ? "ok" : "bad") << "\n";
    if (!shapeValid) {
      result = 1;
      continue;
    }

    std::cout << "  party=" << collection.party[0] << ','
              << collection.party[1] << ',' << collection.party[2]
              << " nextUid=" << collection.nextUid
              << " playTime=" << playTime
              << " bootCount=" << bootCount << "\n";
    std::cout << "  starter caught:";
    for (uint16_t species : kStarterSpecies)
      std::cout << ' ' << species << '='
                << (PokedexLogic::hasCaught(save.pokedex, species) ? 1 : 0);
    std::cout << "\n  active egg: active=" << (save.egg.active ? 1 : 0)
              << " species=" << save.egg.speciesId
              << " rarity=" << static_cast<unsigned>(save.egg.rarity)
              << " remaining=" << save.egg.remainingSeconds
              << "\n  egg bag count="
              << static_cast<unsigned>(save.eggInventory.count) << " slots:";
    for (uint8_t slot = 0; slot < save.eggInventory.count &&
                           slot < EggInventory::kCapacity; ++slot) {
      const StoredEgg& egg = save.eggInventory.slots[slot];
      std::cout << " [" << static_cast<unsigned>(slot)
                << ":species=" << egg.speciesId
                << ",rarity=" << static_cast<unsigned>(egg.rarity) << ']';
    }
    std::cout << '\n';
    std::cout << "  occupied:";
    for (uint16_t slot = 0; slot < kBoxCapacity; ++slot) {
      const OwnedPokemon& pokemon = collection.box[slot];
      if (pokemon.uid == kEmptyPokemonUid) continue;
      ++occupied;
      if (pokemon.speciesId >= 1U && pokemon.speciesId <= 807U)
        ++validSpecies;
      else
        ++invalidSpecies;
      std::cout << " [" << slot << ":uid=" << pokemon.uid
                << ",species=" << pokemon.speciesId
                << ",lv=" << static_cast<unsigned>(pokemon.level)
                << ",form=" << static_cast<unsigned>(pokemon.form) << ']';
    }
    std::cout << "\n  totals occupied=" << occupied
              << " validSpecies=" << validSpecies
              << " invalidSpecies=" << invalidSpecies << "\n";
  }
  return result;
}
