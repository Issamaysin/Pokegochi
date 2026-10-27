#include "game/UnovaTournament.h"

#include <algorithm>
#include <iterator>

namespace {
constexpr const char* kTierNames[]={"KANTO LEADERS","JOHTO LEADERS","HOENN LEADERS",
                                    "SINNOH LEADERS","UNOVA LEADERS"};
constexpr const char* kPwtNames[5][3]={
  {"BROCK","MISTY","GIOVANNI"},{"FALKNER","JASMINE","CLAIR"},
  {"ROXANNE","WALLACE","NORMAN"},{"ROARK","VOLKNER","CYNTHIA"},
  {"CHEREN","MARLON","IRIS"},
};
constexpr uint16_t kPwtTeams[5][3][3]={
 {{95,76,142},{121,131,134},{31,34,115}},
 {{164,227,18},{208,212,181},{130,230,149}},
 {{476,369,142},{321,350,260},{289,295,327}},
 {{409,464,476},{466,405,445},{448,468,445}},
 {{508,573,628},{565,594,604},{635,612,621}},
};
constexpr const char* kChampionNames[]={"RED","LANCE","STEVEN","CYNTHIA","ALDER"};
constexpr uint16_t kChampionTeams[5][3]={
 {3,6,9},{149,248,230},{376,306,260},{445,448,468},{637,626,589},
};
constexpr const char* kRoyalNames[]={"ACE TRAINER","VETERAN","RANGER","SOCIALITE",
                                     "BACKPACKER","BLACK BELT","PSYCHIC"};
constexpr uint16_t kRoyalTeams[7][3]={
 {521,530,537},{628,553,604},{586,614,589},{573,576,594},
 {512,555,565},{538,539,534},{518,579,609},
};
uint32_t mix(uint32_t x){x^=x>>16U;x*=0x7FEB352DUL;x^=x>>15U;x*=0x846CA68BUL;return x^(x>>16U);}
void healParty(PokemonCollection& collection){
  for(uint8_t slot=0;slot<kPartyCapacity;++slot){
    OwnedPokemon* pokemon=CollectionLogic::active(collection,slot);if(!pokemon)continue;
    pokemon->currentHp=pokemon->maximumHp;pokemon->status=StatusCondition::None;
    pokemon->recoverySecondsRemaining=0U;
    for(uint8_t move=0;move<kMoveSlots;++move)
      pokemon->movePp[move]=CollectionLogic::maximumMovePp(*pokemon,move);
  }
}
bool validPlayer(const PokemonCollection& collection,uint32_t uid){
  const OwnedPokemon* pokemon=CollectionLogic::find(collection,uid);
  return pokemon&&pokemon->currentHp&&CollectionLogic::isInParty(collection,uid);
}
}

bool UnovaTournament::active(const UnovaAdventureState& state){
  return state.tournament.mode!=UnovaTournamentMode::None;
}
bool UnovaTournament::championsUnlocked(const UnovaAdventureState& state){
  return UnovaAdventure::allAreasCompleted(state)&&
      state.pwtCompletedTiers==0x1FU;
}
bool UnovaTournament::beginPwt(UnovaAdventureState& state,uint8_t tier,uint32_t seed,
                               uint8_t season){
  if(active(state)||tier>=5U||!UnovaAdventure::pwtTierUnlocked(state,tier))return false;
  state.tournament={seed?seed:mix(state.rngState++),UINT32_MAX,
                    UnovaTournamentMode::Pwt,tier,0U,3U,
                    static_cast<uint8_t>(std::min<uint8_t>(season,3U))};return true;
}
bool UnovaTournament::beginChampions(UnovaAdventureState& state,uint32_t seed,
                                     uint8_t season){
  if(active(state)||!championsUnlocked(state))return false;
  state.tournament={seed?seed:mix(state.rngState++),UINT32_MAX,
                    UnovaTournamentMode::Champions,0U,0U,5U,
                    static_cast<uint8_t>(std::min<uint8_t>(season,3U))};return true;
}
bool UnovaTournament::beginRoyal(UnovaAdventureState& state,uint32_t localDay,uint32_t seed,
                                 uint8_t season){
  if(active(state)||!UnovaAdventure::royalAvailableToday(state,localDay))return false;
  state.tournament={seed?seed:mix(state.rngState++),localDay,
                    UnovaTournamentMode::RoyalUnova,0U,0U,
                    UnovaAdventure::royalTrainerCount(localDay),
                    static_cast<uint8_t>(std::min<uint8_t>(season,3U))};return true;
}
bool UnovaTournament::abandon(UnovaAdventureState& state){
  if(!active(state))return false;state.tournament=UnovaTournamentRunState{};return true;
}
const char* UnovaTournament::tierName(uint8_t tier){return tier<5U?kTierNames[tier]:"PWT";}
const char* UnovaTournament::opponentName(const UnovaAdventureState& state){
  if(!active(state))return "";
  const uint8_t round=state.tournament.round;
  if(state.tournament.mode==UnovaTournamentMode::Pwt)
    return kPwtNames[std::min<uint8_t>(state.tournament.tier,4U)][round%3U];
  if(state.tournament.mode==UnovaTournamentMode::Champions)return kChampionNames[round%5U];
  return kRoyalNames[(mix(state.tournament.seed^round))%7U];
}
bool UnovaTournament::startBattle(UnovaAdventureState& state,BattleState& battle,
                                  PokemonCollection& collection,uint32_t playerUid){
  if(!active(state)||battle.active||state.tournament.round>=state.tournament.rounds)return false;
  healParty(collection);
  if(!validPlayer(collection,playerUid)){
    for(uint8_t slot=0;slot<kPartyCapacity&&!validPlayer(collection,playerUid);++slot)
      if(const OwnedPokemon* pokemon=CollectionLogic::active(collection,slot))playerUid=pokemon->uid;
  }
  if(!validPlayer(collection,playerUid))return false;
  BattleEngine::clear(battle);battle.active=true;battle.outcome=BattleOutcome::Ongoing;
  battle.kind=state.tournament.mode==UnovaTournamentMode::RoyalUnova?
      BattleKind::RoyalUnova:BattleKind::Pwt;
  battle.playerUid=playerUid;battle.unlockedGeneration=5U;
  battle.rngState=mix(state.tournament.seed^state.tournament.round);
  battle.opponentCount=3U;battle.opponentIndex=0U;
  uint16_t selected[3]{};
  if(state.tournament.mode==UnovaTournamentMode::Pwt){
    const auto& source=kPwtTeams[std::min<uint8_t>(state.tournament.tier,4U)]
                                [state.tournament.round%3U];
    std::copy(std::begin(source),std::end(source),selected);
  }else if(state.tournament.mode==UnovaTournamentMode::Champions){
    const auto& source=kChampionTeams[state.tournament.round%5U];
    std::copy(std::begin(source),std::end(source),selected);
  }else{
    const auto& source=kRoyalTeams[(mix(state.tournament.seed^state.tournament.round))%7U];
    std::copy(std::begin(source),std::end(source),selected);
  }
  const uint8_t base=std::max<uint8_t>(60U,BattleEngine::highestPartyLevel(collection));
  for(uint8_t slot=0;slot<3U;++slot)
  {
    battle.opponents[slot]=CollectionLogic::createPokemon(0U,selected[slot],base,false,
        battle.rngState^(slot*0x9E3779B9UL),5U);
    CollectionLogic::applyUnovaSeason(battle.opponents[slot],
                                      state.tournament.season);
  }
  BattleEngine::orderOpponentTeamWeakestFirst(battle);
  BattleEngine::ensureOpponentUids(battle);
  BattleEngine::applyEntryAbilities(battle,collection);
  return true;
}
bool UnovaTournament::recordBattle(UnovaAdventureState& state,BattleOutcome outcome,
                                   bool& completedChallenge){
  completedChallenge=false;if(!active(state)||outcome==BattleOutcome::None||
      outcome==BattleOutcome::Ongoing)return false;
  if(outcome!=BattleOutcome::Victory){state.tournament=UnovaTournamentRunState{};return true;}
  if(++state.tournament.round<state.tournament.rounds)return true;
  const UnovaTournamentMode mode=state.tournament.mode;
  const uint8_t tier=state.tournament.tier;const uint32_t day=state.tournament.localDay;
  state.tournament=UnovaTournamentRunState{};completedChallenge=true;
  if(mode==UnovaTournamentMode::Pwt)return UnovaAdventure::recordPwtVictory(state,tier)||true;
  if(mode==UnovaTournamentMode::Champions)return UnovaAdventure::recordChampionsVictory(state)||true;
  if(mode==UnovaTournamentMode::RoyalUnova)return UnovaAdventure::recordRoyalCompletion(state,day)||true;
  return true;
}
