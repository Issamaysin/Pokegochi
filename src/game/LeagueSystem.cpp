#include "game/LeagueSystem.h"

#include <algorithm>

namespace {
constexpr LeagueMemberDefinition kMembers[] = {
  {1,0,"ELITE FOUR","LORELEI","elite_four_lorelei_front_pic","WELCOME TO THE POKEMON LEAGUE.","I AM LORELEI OF THE ELITE FOUR.",{{87,52},{124,54},{131,54}}},
  {1,1,"ELITE FOUR","BRUNO","elite_four_bruno_front_pic","I AM BRUNO OF THE ELITE FOUR!","WE CAN BECOME STRONGER WITHOUT LIMIT.",{{107,53},{95,54},{68,56}}},
  {1,2,"ELITE FOUR","AGATHA","elite_four_agatha_front_pic","I AM AGATHA OF THE ELITE FOUR.","I HEAR OAK HAS TAKEN INTEREST IN YOU.",{{94,54},{24,56},{94,58}}},
  {1,3,"ELITE FOUR","LANCE","elite_four_lance_front_pic","I LEAD THE ELITE FOUR.","CALL ME LANCE, THE DRAGON TRAINER.",{{130,56},{142,58},{149,60}}},
  // The Kanto Champion team is replaced at load time so his starter counters the player's.
  {1,4,"CHAMPION","RIVAL","champion_rival_front_pic","HEY! I WAS LOOKING FORWARD TO THIS.","MY RIVAL SHOULD KEEP ME SHARP!",{{112,59},{130,61},{6,63}}},

  // Continuous-save calibration: Johto follows the Lv.63 Kanto Champion and
  // Hoenn consumes the remaining headroom up to the canonical Lv.100 cap.
  {2,0,"ELITE FOUR","WILL","johto_will","WELCOME TO THE POKEMON LEAGUE.","ALLOW ME TO INTRODUCE MYSELF: WILL.",{{103,80},{124,80},{178,81}}},
  {2,1,"ELITE FOUR","KOGA","johto_koga","FWAHAHAHA! I AM KOGA","OF THE ELITE FOUR.",{{89,81},{205,81},{169,82}}},
  {2,2,"ELITE FOUR","BRUNO","johto_bruno","I AM BRUNO OF THE ELITE FOUR.","I ALWAYS TRAIN TO THE EXTREME!",{{107,81},{95,82},{68,83}}},
  {2,3,"ELITE FOUR","KAREN","johto_karen","I AM KAREN OF THE ELITE FOUR.","YOU ARE THE CHALLENGER? HOW AMUSING.",{{198,82},{94,83},{229,84}}},
  {2,4,"CHAMPION","LANCE","johto_champion","I HAVE BEEN WAITING FOR YOU.","I ACCEPT YOUR CHALLENGE!",{{149,85},{149,85},{149,86}}},

  {3,0,"ELITE FOUR","SIDNEY","emerald_elite_four_sidney","WELCOME, CHALLENGER!","I AM SIDNEY OF THE ELITE FOUR.",{{342,96},{275,96},{359,97}}},
  {3,1,"ELITE FOUR","PHOEBE","emerald_elite_four_phoebe","AHAHAHA! I AM PHOEBE","OF THE ELITE FOUR.",{{354,97},{302,97},{356,98}}},
  {3,2,"ELITE FOUR","GLACIA","emerald_elite_four_glacia","WELCOME. MY NAME IS GLACIA","OF THE ELITE FOUR.",{{364,98},{362,98},{365,98}}},
  {3,3,"ELITE FOUR","DRAKE","emerald_elite_four_drake","I AM DRAKE, THE DRAGON MASTER!","I AM THE LAST OF THE ELITE FOUR.",{{330,99},{334,99},{373,99}}},
  {3,4,"CHAMPION","WALLACE","emerald_champion_wallace","WELCOME, CHALLENGER.","SHOW ME ALL THAT YOU HAVE LEARNED!",{{272,100},{321,100},{350,100}}},
};

bool starterLinePresent(const PokemonCollection& collection, uint16_t first) {
  for (const auto& pokemon : collection.box)
    if (pokemon.uid && pokemon.speciesId >= first && pokemon.speciesId <= first + 2U) return true;
  return false;
}

uint32_t leaguePersonalitySeed(const BattleState& battle, uint8_t slot) {
  return 0x4C454147U ^ (static_cast<uint32_t>(battle.leagueRegion) << 16U) ^
         (static_cast<uint32_t>(battle.leagueStage) << 8U) ^ slot;
}

void loadKantoChampion(BattleState& battle, const PokemonCollection& collection) {
  // FireRed selects the rival's starter as the type-advantaged answer to the player's choice.
  uint16_t ace = 6, second = 130;
  if (starterLinePresent(collection, 4)) { ace = 9; second = 103; }
  else if (starterLinePresent(collection, 7)) { ace = 3; second = 59; }
  battle.opponents[0] = CollectionLogic::createPokemon(0, 112, 59, false, leaguePersonalitySeed(battle, 0));
  battle.opponents[1] = CollectionLogic::createPokemon(0, second, 61, false, leaguePersonalitySeed(battle, 1));
  battle.opponents[2] = CollectionLogic::createPokemon(0, ace, 63, false, leaguePersonalitySeed(battle, 2));
}

void loadMember(BattleState& battle, PokemonCollection& collection) {
  const LeagueMemberDefinition* member = LeagueSystem::definition(battle.leagueRegion, battle.leagueStage);
  if (!member) return;
  battle.opponentCount = kOpponentTeamCapacity;
  battle.opponentIndex = 0;
  battle.turn = 0;
  battle.playerVolatile = CombatVolatile{};
  for (auto& state : battle.opponentVolatiles) state = CombatVolatile{};
  // Each Elite Four member is a separate battle. Side conditions such as
  // Safeguard must not leak into the next member's field.
  battle.playerSafeguardTurns = 0;
  battle.opponentSafeguardTurns = 0;
  battle.playerReflectTurns = battle.opponentReflectTurns = 0;
  battle.playerLightScreenTurns = battle.opponentLightScreenTurns = 0;
  battle.playerMistTurns = battle.opponentMistTurns = 0;
  battle.playerProtectChain = battle.opponentProtectChain = 0;
  battle.playerEndureThisTurn = battle.opponentEndureThisTurn = false;
  battle.opponentItemUses = battle.leagueStage == 4 ? 4 : 2;
  battle.rewardMoney = static_cast<uint32_t>(member->team[kOpponentTeamCapacity - 1U].level) *
                       (battle.leagueStage == 4 ? 200U : 100U);
  for (uint8_t i = 0; i < kOpponentTeamCapacity; ++i)
    battle.opponents[i] = CollectionLogic::createPokemon(
        0, member->team[i].speciesId, member->team[i].level, false,
        leaguePersonalitySeed(battle, i));
  if (battle.leagueRegion == 1 && battle.leagueStage == 4) loadKantoChampion(battle, collection);
  BattleEngine::orderOpponentTeamWeakestFirst(battle);
  BattleEngine::ensureOpponentUids(battle);
}

OwnedPokemon* firstHealthy(PokemonCollection& collection) {
  for(uint8_t slot=0;slot<kPartyCapacity;++slot){
    OwnedPokemon* pokemon=CollectionLogic::active(collection,slot);
    if(pokemon&&pokemon->currentHp&&pokemon->recoverySecondsRemaining==0)return pokemon;
  }
  return nullptr;
}
}  // namespace

const LeagueMemberDefinition* LeagueSystem::definition(uint8_t region, uint8_t stage) {
  if (region < 1 || region > 3 || stage >= kMemberCount) return nullptr;
  return &kMembers[(region - 1U) * kMemberCount + stage];
}

bool LeagueSystem::start(BattleState& battle, PokemonCollection& collection, uint32_t playerUid,
                         const GymProgress& progress, uint32_t seed) {
  OwnedPokemon* player = CollectionLogic::find(collection, playerUid);
  if (battle.active || !GymSystem::leagueAvailable(progress) || !player ||
      !CollectionLogic::isInParty(collection, playerUid) || !player->currentHp ||
      player->recoverySecondsRemaining) return false;
  BattleEngine::clear(battle);
  battle.active = true;
  battle.kind = BattleKind::League;
  battle.outcome = BattleOutcome::Ongoing;
  battle.playerUid = playerUid;
  battle.unlockedGeneration = progress.unlockedGeneration;
  battle.rngState = seed ? seed : 0x1EA64E11U;
  battle.leagueRegion = progress.unlockedGeneration;
  battle.leagueStage = 0;
  loadMember(battle, collection);
  BattleEngine::applyEntryAbilities(battle, collection);
  return true;
}

bool LeagueSystem::advance(BattleState& battle, PokemonCollection& collection) {
  if (battle.kind != BattleKind::League || battle.outcome != BattleOutcome::Victory ||
      battle.leagueStage + 1U >= kMemberCount) return false;
  OwnedPokemon* current=CollectionLogic::find(collection,battle.playerUid);
  if(!current||!current->currentHp||current->recoverySecondsRemaining){
    current=firstHealthy(collection);
    if(!current){battle.active=false;battle.outcome=BattleOutcome::Defeat;return false;}
    battle.playerUid=current->uid;
  }
  ++battle.leagueStage;
  battle.active = true;
  battle.outcome = BattleOutcome::Ongoing;
  loadMember(battle, collection);
  BattleEngine::applyEntryAbilities(battle, collection);
  return true;
}

bool LeagueSystem::isChampionStage(const BattleState& battle) {
  return battle.kind == BattleKind::League && battle.leagueStage == kMemberCount - 1U;
}

bool LeagueSystem::reconcileKantoLuckyEggReward(const GymProgress& progress,
                                                PokemonCollection& collection,
                                                uint64_t& ownedMachines) {
  if (!GymSystem::leagueComplete(progress, 1U)) return false;
  bool removedLegacyHolder=false;
  for (OwnedPokemon& pokemon : collection.box)
    if (pokemon.uid != kEmptyPokemonUid && pokemon.heldItem == HeldItem::LuckyEgg){
      pokemon.heldItem=HeldItem::None;removedLegacyHolder=true;
    }
  const bool newlyUnlocked=(ownedMachines&kLuckyEggOwnershipBit)==0;
  ownedMachines |= kLuckyEggOwnershipBit;
  return newlyUnlocked||removedLegacyHolder;
}

uint8_t LeagueSystem::minimumLevel(uint8_t region) {
  uint8_t level = 100;
  for (uint8_t stage = 0; stage < kMemberCount; ++stage) {
    const auto* member = definition(region, stage);
    if (!member) continue;
    for (const auto& pokemon : member->team) level = std::min(level, pokemon.level);
  }
  return level == 100 ? 0 : level;
}

uint8_t LeagueSystem::maximumLevel(uint8_t region) {
  uint8_t level = 0;
  for (uint8_t stage = 0; stage < kMemberCount; ++stage) {
    const auto* member = definition(region, stage);
    if (!member) continue;
    for (const auto& pokemon : member->team) level = std::max(level, pokemon.level);
  }
  return level;
}
