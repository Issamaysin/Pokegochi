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
struct Challenger { const char* trainerClass;const char* name;const char* asset;const char* line1;const char* line2;uint16_t species[2];uint8_t levels[2];uint8_t count; };
constexpr Challenger kChallengers[8][2]={
 {{"CAMPER","LIAM","camper_front_pic","STOP RIGHT THERE, KID!","YOU'RE LIGHT-YEARS FROM BROCK!",{74,27},{10,11},2},{"PICNICKER","AMARA","picnicker_front_pic","BROCK IS TOUGH.","CAN YOU HANDLE ROCK POKEMON?",{74,50},{11,11},2}},
 {{"SWIMMER","LUIS","swimmer_m_front_pic","SPLASH!","I'M FIRST UP!",{116,90},{16,16},2},{"PICNICKER","DIANA","picnicker_front_pic","MISTY WON'T HAVE TO BOTHER.","I'LL BEAT YOU!",{118,120},{17,17},2}},
 {{"SAILOR","DWAYNE","sailor_front_pic","THIS IS NO PLACE FOR KIDS!","NOT EVEN IF YOU'RE GOOD!",{25,81},{20,21},2},{"ENGINEER","BAILY","engineer_front_pic","I'M A LIGHTWEIGHT, BUT","I'M GOOD WITH ELECTRICITY!",{100,81},{21,21},2}},
 {{"LASS","KAY","lass_front_pic","ONLY REAL LADIES","ARE ALLOWED IN HERE!",{43,69},{23,24},2},{"BEAUTY","BRIDGET","beauty_front_pic","WELCOME.","I WAS GETTING BORED.",{70,102},{24,24},2}},
 {{"JUGGLER","KAYDEN","juggler_front_pic","STRENGTH ISN'T THE KEY.","POKEMON IS ABOUT STRATEGY!",{96,100},{34,34},2},{"TAMER","EDGAR","tamer_front_pic","I ALSO STUDY THE WAY","OF THE NINJA!",{24,28},{35,35},2}},
 {{"PSYCHIC","CAMERON","psychic_m_front_pic","YOU AND I, OUR POKEMON","SHALL FIGHT!",{79,64},{37,37},2},{"CHANNELER","MARTHA","channeler_front_pic","I CAN SEE YOUR FUTURE.","YOU WILL LOSE!",{92,93},{38,38},2}},
 {{"BURGLAR","QUINN","burglar_front_pic","I SURRENDER!","NO, IT'S JUST A TRICK!",{58,37},{40,40},2},{"SUPER NERD","ERIK","super_nerd_front_pic","MY POKEMON ARE ALL","HOT AND READY!",{77,126},{41,41},2}},
 {{"COOLTRAINER","SAMUEL","cool_trainer_m_front_pic","THE VIRIDIAN GYM WAS","CLOSED FOR A LONG TIME.",{111,51},{43,43},2},{"BLACK BELT","TAKASHI","black_belt_front_pic","POKEMON AND I, WE MAKE","WONDERFUL MUSIC TOGETHER!",{66,67},{44,44},2}}
};
void loadStage(BattleState& battle){const uint8_t gym=battle.gymId;if(gym>=8)return;if(battle.gymStage<2){const Challenger& c=kChallengers[gym][battle.gymStage];battle.opponentCount=c.count;battle.opponentIndex=0;battle.rewardMoney=0;battle.opponentItemUses=0;for(uint8_t i=0;i<c.count;++i)battle.opponents[i]=CollectionLogic::createPokemon(0,c.species[i],c.levels[i]);}
 else {const GymDefinition& g=kGyms[gym];battle.opponentCount=g.teamSize;battle.opponentIndex=0;battle.rewardMoney=static_cast<uint32_t>(g.team[g.teamSize-1].level)*100U;battle.opponentItemUses=2;for(uint8_t i=0;i<g.teamSize;++i)battle.opponents[i]=CollectionLogic::createPokemon(0,g.team[i].speciesId,g.team[i].level);}}
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
  battle.rngState = seed ? seed : 0xC0FFEE11U; battle.gymStage=0;
  battle.gymId = static_cast<uint8_t>(id);
  loadStage(battle);
  BattleEngine::applyEntryAbilities(battle,collection);
  return true;
}
bool GymSystem::advance(BattleState& battle,PokemonCollection& collection){if(battle.kind!=BattleKind::Gym||battle.gymStage>=2)return false;++battle.gymStage;battle.active=true;battle.outcome=BattleOutcome::Ongoing;loadStage(battle);BattleEngine::applyEntryAbilities(battle,collection);return true;}
bool GymSystem::isLeaderStage(const BattleState& battle){return battle.kind==BattleKind::Gym&&battle.gymStage==2;}
const char* GymSystem::opponentClass(const BattleState& b){return b.gymStage<2?kChallengers[b.gymId][b.gymStage].trainerClass:"LEADER";}
const char* GymSystem::opponentName(const BattleState& b){return b.gymStage<2?kChallengers[b.gymId][b.gymStage].name:kGyms[b.gymId].leader;}
const char* GymSystem::opponentAsset(const BattleState& b){return b.gymStage<2?kChallengers[b.gymId][b.gymStage].asset:kGyms[b.gymId].frontAsset;}
const char* GymSystem::opponentLine1(const BattleState& b){return b.gymStage<2?kChallengers[b.gymId][b.gymStage].line1:kGyms[b.gymId].introLine1;}
const char* GymSystem::opponentLine2(const BattleState& b){return b.gymStage<2?kChallengers[b.gymId][b.gymStage].line2:kGyms[b.gymId].introLine2;}

bool GymSystem::recordVictory(GymProgress& progress, GymId id) {
  if (id != next(progress)) return false;
  progress.badgeBits |= static_cast<uint8_t>(1U << static_cast<uint8_t>(id));
  return true;
}
