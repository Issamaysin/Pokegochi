#include <cassert>
#include <cstdint>
#include <cstring>

#include "services/RecoveryUpdateProtocol.h"

int main() {
  uint8_t digest[32]{};
  for (uint8_t index = 0; index < sizeof(digest); ++index)
    digest[index] = static_cast<uint8_t>(index * 7U + 3U);

  auto marker = PokegochiRecovery::makeMarker(123456U, 2494252U, digest);
  assert(PokegochiRecovery::valid(marker));
  assert(marker.packageSequence == 123456U);
  assert(marker.firmwareSize == 2494252U);
  assert(!std::memcmp(marker.firmwareSha256, digest, sizeof(digest)));

  marker.phase = static_cast<uint8_t>(PokegochiRecovery::Phase::Candidate);
  PokegochiRecovery::seal(marker);
  assert(PokegochiRecovery::valid(marker));
  marker.firmwareSha256[9] ^= 0x80U;
  assert(!PokegochiRecovery::valid(marker));

  return 0;
}
