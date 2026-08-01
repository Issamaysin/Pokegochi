#include "game/GymSystem.h"

namespace {
constexpr GymDefinition kGyms[] = {
  {GymId::Pewter, "BROCK", "BOULDER", "leader_brock_front_pic", "SO, YOU'RE HERE. I'M BROCK.", "I'M PEWTER'S GYM LEADER.", {{74,12},{27,11},{95,14}}, 3},
  {GymId::Cerulean, "MISTY", "CASCADE", "leader_misty_front_pic", "HI, YOU'RE A NEW FACE!", "MY POLICY IS ALL-OUT OFFENSE!", {{120,18},{118,19},{121,21}}, 3},
  {GymId::Vermilion, "LT. SURGE", "THUNDER", "leader_lt_surge_front_pic", "HEY, KID! WHAT DO YOU THINK", "YOU'RE DOING HERE?", {{100,21},{25,18},{26,24}}, 3},
  {GymId::Celadon, "ERIKA", "RAINBOW", "leader_erika_front_pic", "HELLO... LOVELY WEATHER,", "ISN'T IT?", {{71,29},{114,24},{45,29}}, 3},
  {GymId::Fuchsia, "KOGA", "SOUL", "leader_koga_front_pic", "FWAHAHAHA! A MERE CHILD", "DARES TO CHALLENGE ME?", {{110,43},{89,39},{109,37}}, 3},
  {GymId::Saffron, "SABRINA", "MARSH", "leader_sabrina_front_pic", "I HAD A VISION", "OF YOUR ARRIVAL.", {{65,43},{64,38},{49,38}}, 3},
  {GymId::Cinnabar, "BLAINE", "VOLCANO", "leader_blaine_front_pic", "HAH! I AM BLAINE,", "THE RED-HOT LEADER!", {{59,47},{58,42},{78,42}}, 3},
  {GymId::Viridian, "GIOVANNI", "EARTH", "leader_giovanni_front_pic", "FWAHAHA! WELCOME", "TO MY HIDEOUT!", {{112,50},{111,45},{34,45}}, 3},
};
}

const GymDefinition* GymSystem::definition(GymId id) {
  const uint8_t index = static_cast<uint8_t>(id);
  return index < static_cast<uint8_t>(GymId::Count) ? &kGyms[index] : nullptr;
}

bool GymSystem::hasBadge(const GymProgress& progress, GymId id) {
  const uint8_t index = static_cast<uint8_t>(id);
  return index < 8 && (progress.badgeBits & (1U << index));
}

GymId GymSystem::next(const GymProgress& progress) {
  for (uint8_t index = 0; index < 8; ++index) {
    const GymId id = static_cast<GymId>(index);
    if (!hasBadge(progress, id)) return id;
  }
  return GymId::Count;
}

bool GymSystem::start(BattleState& battle, PokemonCollection& collection, uint32_t playerUid,
                      const GymProgress& progress, GymId id, uint32_t seed) {
  const GymDefinition* gym = definition(id);
  OwnedPokemon* player = CollectionLogic::find(collection, playerUid);
  if (!gym || battle.active || id != next(progress) || !player ||
      !CollectionLogic::isInParty(collection, playerUid) || player->currentHp == 0 ||
      player->recoverySecondsRemaining) return false;
  BattleEngine::clear(battle); battle.active = true; battle.kind = BattleKind::Gym;
  battle.outcome = BattleOutcome::Ongoing; battle.playerUid = playerUid;
  battle.rngState = seed ? seed : 0xC0FFEE11U; battle.opponentCount = gym->teamSize;
  battle.rewardMoney = static_cast<uint32_t>(gym->team[gym->teamSize - 1].level) * 100U;
  battle.opponentItemUses = 2;
  battle.gymId = static_cast<uint8_t>(id);
  for (uint8_t i = 0; i < gym->teamSize; ++i) {
    battle.opponents[i] = CollectionLogic::createPokemon(0, gym->team[i].speciesId, gym->team[i].level);
    if (!battle.opponents[i].speciesId) { BattleEngine::clear(battle); return false; }
  }
  return true;
}

bool GymSystem::recordVictory(GymProgress& progress, GymId id) {
  if (id != next(progress)) return false;
  progress.badgeBits |= static_cast<uint8_t>(1U << static_cast<uint8_t>(id));
  return true;
}
