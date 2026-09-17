#include "game/GymSystem.h"

namespace {
constexpr GymDefinition kGyms[] = {
  {GymId::Pewter, "BROCK", "BOULDER", "ROCK", "leader_brock_front_pic", "SO, YOU'RE HERE. I'M BROCK.", "I'M PEWTER'S GYM LEADER.", {{74,12},{27,11},{95,14}}, 3},
  {GymId::Cerulean, "MISTY", "CASCADE", "WATER", "leader_misty_front_pic", "HI, YOU'RE A NEW FACE!", "MY POLICY IS ALL-OUT OFFENSE!", {{120,18},{118,19},{121,21}}, 3},
  {GymId::Vermilion, "LT. SURGE", "THUNDER", "ELECTRIC", "leader_lt_surge_front_pic", "HEY, KID! WHAT DO YOU THINK", "YOU'RE DOING HERE?", {{100,21},{25,18},{26,24}}, 3},
  {GymId::Celadon, "ERIKA", "RAINBOW", "GRASS", "leader_erika_front_pic", "HELLO... LOVELY WEATHER,", "ISN'T IT?", {{71,29},{114,24},{45,29}}, 3},
  // FireRed places a large amount of mandatory route content between Erika
  // and Koga. Pokegochi does not, so Kanto's latter half advances roughly
  // five levels per badge while preserving Giovanni at Lv.50.
  {GymId::Fuchsia, "KOGA", "SOUL", "POISON", "leader_koga_front_pic", "FWAHAHAHA! A MERE CHILD", "DARES TO CHALLENGE ME?", {{109,32},{89,33},{110,34}}, 3},
  {GymId::Saffron, "SABRINA", "MARSH", "PSYCHIC", "leader_sabrina_front_pic", "I HAD A VISION", "OF YOUR ARRIVAL.", {{64,37},{49,37},{65,39}}, 3},
  {GymId::Cinnabar, "BLAINE", "VOLCANO", "FIRE", "leader_blaine_front_pic", "HAH! I AM BLAINE,", "THE RED-HOT LEADER!", {{58,42},{78,42},{59,44}}, 3},
  {GymId::Viridian, "GIOVANNI", "EARTH", "GROUND", "leader_giovanni_front_pic", "FWAHAHA! WELCOME", "TO MY HIDEOUT!", {{111,48},{34,48},{112,50}}, 3},
  // Pokegochi is one continuous save rather than three independent RPGs.
  // Species and relative team order remain faithful to Gold/Emerald, while
  // levels continue from the preceding regional League.
  {GymId::Violet, "FALKNER", "ZEPHYR", "FLYING", "johto_falkner", "I'M FALKNER, THE VIOLET", "POKEMON GYM LEADER!", {{16,64},{21,65},{17,66}}, 3},
  {GymId::Azalea, "BUGSY", "HIVE", "BUG", "johto_bugsy", "I'M BUGSY! I NEVER LOSE", "WHEN IT COMES TO BUG POKEMON!", {{11,66},{14,67},{123,68}}, 3},
  {GymId::Goldenrod, "WHITNEY", "PLAIN", "NORMAL", "johto_whitney", "HI! I'M WHITNEY!", "EVERYONE WAS INTO POKEMON!", {{39,68},{35,69},{241,70}}, 3},
  {GymId::Ecruteak, "MORTY", "FOG", "GHOST", "johto_morty", "I AM MORTY OF ECRUTEAK.", "I TRAIN GHOST POKEMON.", {{93,70},{93,71},{94,72}}, 3},
  {GymId::Cianwood, "CHUCK", "STORM", "FIGHTING", "johto_chuck", "WAHAHAH! SO YOU'VE COME", "THIS FAR! LET ME TEST YOU!", {{67,72},{57,73},{62,74}}, 3},
  {GymId::Olivine, "JASMINE", "MINERAL", "STEEL", "johto_jasmine", "I USE THE STEEL TYPE.", "DO YOU KNOW ABOUT STEEL?", {{81,74},{81,75},{208,76}}, 3},
  {GymId::Mahogany, "PRYCE", "GLACIER", "ICE", "johto_pryce", "POKEMON HAVE MANY", "EXPERIENCES IN THEIR LIVES.", {{86,76},{87,77},{221,78}}, 3},
  {GymId::Blackthorn, "CLAIR", "RISING", "DRAGON", "johto_clair", "I AM CLAIR.", "THE WORLD'S BEST DRAGON MASTER.", {{148,78},{148,79},{230,80}}, 3},
  {GymId::Rustboro, "ROXANNE", "STONE", "ROCK", "leader_roxanne_front_pic", "I BECAME A GYM LEADER", "TO APPLY WHAT I LEARNED.", {{74,87},{74,87},{299,88}}, 3},
  {GymId::Dewford, "BRAWLY", "KNUCKLE", "FIGHTING", "leader_brawly_front_pic", "I'VE BEEN CHURNED IN", "THE ROUGH WAVES OF DEWFORD!", {{66,88},{307,89},{296,89}}, 3},
  {GymId::Mauville, "WATTSON", "DYNAMO", "ELECTRIC", "leader_wattson_front_pic", "I'VE GIVEN UP ON MY PLANS", "TO CONVERT THE CITY! WAHAHA!", {{309,89},{82,90},{310,90}}, 3},
  {GymId::Lavaridge, "FLANNERY", "HEAT", "FIRE", "leader_flannery_front_pic", "WELCOME! NO, I MEAN...", "PUNY TRAINER, HOW GOOD?", {{218,90},{323,91},{324,91}}, 3},
  {GymId::Petalburg, "NORMAN", "BALANCE", "NORMAL", "leader_norman_front_pic", "I'M HAPPY THAT I CAN", "HAVE A REAL BATTLE WITH YOU.", {{288,91},{264,92},{289,92}}, 3},
  {GymId::Fortree, "WINONA", "FEATHER", "FLYING", "leader_winona_front_pic", "I AM WINONA.", "I LEAD THE FLYING POKEMON.", {{279,92},{227,93},{334,93}}, 3},
  {GymId::Mossdeep, "TATE & LIZA", "MIND", "PSYCHIC", "leader_tate_and_liza_front_pic", "HEE HEE... WERE YOU", "SURPRISED TO SEE TWO OF US?", {{178,93},{337,94},{338,94}}, 3},
  {GymId::Sootopolis, "JUAN", "RAIN", "WATER", "leader_juan_front_pic", "LET ME ASK YOU.", "DID YOU KNOW I LOVE WATER?", {{364,94},{342,95},{230,96}}, 3},
};
struct Challenger { const char* trainerClass;const char* name;const char* asset;const char* line1;const char* line2;uint16_t species[2];uint8_t levels[2];uint8_t count; };
constexpr Challenger kChallengers[24][2]={
 {{"CAMPER","LIAM","camper_front_pic","STOP RIGHT THERE, KID!","YOU'RE LIGHT-YEARS FROM BROCK!",{74,27},{10,11},2},{"PICNICKER","AMARA","picnicker_front_pic","BROCK IS TOUGH.","CAN YOU HANDLE ROCK POKEMON?",{74,50},{11,11},2}},
 {{"SWIMMER","LUIS","swimmer_m_front_pic","SPLASH!","I'M FIRST UP!",{116,90},{16,16},2},{"PICNICKER","DIANA","picnicker_front_pic","MISTY WON'T HAVE TO BOTHER.","I'LL BEAT YOU!",{118,120},{17,17},2}},
 {{"SAILOR","DWAYNE","sailor_front_pic","THIS IS NO PLACE FOR KIDS!","NOT EVEN IF YOU'RE GOOD!",{25,81},{20,21},2},{"ENGINEER","BAILY","engineer_front_pic","I'M A LIGHTWEIGHT, BUT","I'M GOOD WITH ELECTRICITY!",{100,81},{21,21},2}},
 {{"LASS","KAY","lass_front_pic","ONLY REAL LADIES","ARE ALLOWED IN HERE!",{43,69},{23,24},2},{"BEAUTY","BRIDGET","beauty_front_pic","WELCOME.","I WAS GETTING BORED.",{70,102},{24,24},2}},
 {{"JUGGLER","KAYDEN","juggler_front_pic","STRENGTH ISN'T THE KEY.","POKEMON IS ABOUT STRATEGY!",{96,100},{30,30},2},{"TAMER","EDGAR","tamer_front_pic","I ALSO STUDY THE WAY","OF THE NINJA!",{24,28},{31,31},2}},
 {{"PSYCHIC","CAMERON","psychic_m_front_pic","YOU AND I, OUR POKEMON","SHALL FIGHT!",{79,64},{35,35},2},{"CHANNELER","MARTHA","channeler_front_pic","I CAN SEE YOUR FUTURE.","YOU WILL LOSE!",{92,93},{36,36},2}},
 {{"BURGLAR","QUINN","burglar_front_pic","I SURRENDER!","NO, IT'S JUST A TRICK!",{58,37},{40,40},2},{"SUPER NERD","ERIK","super_nerd_front_pic","MY POKEMON ARE ALL","HOT AND READY!",{77,126},{41,41},2}},
 {{"COOLTRAINER","SAMUEL","cool_trainer_m_front_pic","THE VIRIDIAN GYM WAS","CLOSED FOR A LONG TIME.",{111,51},{45,46},2},{"BLACK BELT","TAKASHI","black_belt_front_pic","POKEMON AND I, WE MAKE","WONDERFUL MUSIC TOGETHER!",{66,67},{47,47},2}}
 ,{{"BIRD KEEPER","ABE","bird_keeper_front_pic","THE KEY WORD IS GUTS!","THOSE HERE TRAIN NIGHT AND DAY!",{21,16},{63,64},2},{"BIRD KEEPER","ROD","bird_keeper_front_pic","LET ME SEE IF YOU ARE","GOOD ENOUGH FOR FALKNER!",{16,17},{64,65},2}}
 ,{{"BUG CATCHER","AL","bug_catcher_front_pic","BUG POKEMON EVOLVE YOUNG.","SO THEY GET STRONG FAST!",{10,13},{65,65},2},{"TWINS","AMY & MAY","twins_front_pic","WE'LL SHOW YOU WHAT","BUG POKEMON CAN DO!",{13,10},{66,66},2}}
 ,{{"BEAUTY","VICTORIA","beauty_front_pic","DON'T LET MY POKEMON'S","CUTE LOOKS FOOL YOU!",{161,39},{67,67},2},{"LASS","CARRIE","lass_front_pic","I LIKE CUTE POKEMON.","BETTER THAN STRONG ONES!",{52,35},{68,68},2}}
 ,{{"SAGE","JEFFREY","channeler_front_pic","CAN YOU INFLICT DAMAGE","ON OUR GHOSTS?",{92,93},{69,69},2},{"MEDIUM","MARTHA","channeler_front_pic","I SHALL WIN!","THE SPIRITS ARE WITH ME!",{93,92},{70,70},2}}
 ,{{"BLACK BELT","YOSHI","black_belt_front_pic","MY RAGING FISTS WILL","SHATTER YOUR POKEMON!",{66,67},{71,71},2},{"BLACK BELT","LAO","black_belt_front_pic","MY POKEMON BOUND","RIGHT INTO ACTION!",{56,67},{72,72},2}}
 ,{{"LASS","CONNIE","lass_front_pic","DON'T GET TOO CLOSE.","I'LL SHOCK YOU!",{81,179},{73,73},2},{"GENTLEMAN","PRESTON","gentleman_front_pic","STEEL IS BOTH HARD","AND COLD!",{81,82},{74,74},2}}
 ,{{"SKIER","DIANA","picnicker_front_pic","TO GET TO PRYCE,","YOU NEED OUR HELP!",{124,220},{75,75},2},{"BOARDER","BRAD","camper_front_pic","THIS GYM HAS A SLIPPERY","FLOOR. STAY SHARP!",{215,221},{76,76},2}}
 ,{{"COOLTRAINER","PAUL","cool_trainer_m_front_pic","DRAGONS ARE SACRED.","THEY'RE FULL OF LIFE ENERGY!",{147,117},{77,77},2},{"COOLTRAINER","LOLA","cool_trainer_f_front_pic","IT'S NOT AS IF WE ALL","USE DRAGON POKEMON!",{148,230},{78,78},2}}
 ,{{"SCHOOL KID","JOSH","youngster_front_pic","ROCK TYPES ARE STRONG!","I LEARNED THAT IN SCHOOL!",{74,299},{86,86},2},{"SCHOOL KID","TOMMY","youngster_front_pic","ROXANNE TAUGHT US","EVERYTHING WE KNOW!",{74,304},{87,87},2}}
 ,{{"BATTLE GIRL","LAURA","crush_girl_front_pic","I'M CRUSHING WITH","FIGHTING SPIRIT!",{307,66},{87,87},2},{"BLACK BELT","TAKASHI","black_belt_front_pic","A BATTLE IS A WAVE.","RIDE IT OR WIPE OUT!",{66,296},{88,88},2}}
 ,{{"GUITARIST","KIRK","rocker_front_pic","MY ELECTRIC SOUL","WILL SHOCK YOU!",{309,81},{88,88},2},{"BATTLE GIRL","VIVI","crush_girl_front_pic","THIS GYM'S SWITCHES","AREN'T THE ONLY TRICK!",{100,307},{89,89},2}}
 ,{{"KINDLER","COLE","burglar_front_pic","MY FIRE BURNS HOT!","CAN YOU TAKE THE HEAT?",{322,218},{89,89},2},{"NINJA BOY","RILEY","ninja_boy_front_pic","THE STEAM HIDES","MY EVERY MOVE!",{109,323},{90,90},2}}
 ,{{"COOLTRAINER","MARY","cool_trainer_f_front_pic","NORMAN'S STRENGTH","IS PERFECTLY BALANCED!",{327,288},{90,90},2},{"COOLTRAINER","GEORGE","cool_trainer_m_front_pic","NORMAL DOESN'T MEAN","ORDINARY!",{264,288},{91,91},2}}
 ,{{"BIRD KEEPER","JARED","bird_keeper_front_pic","THE SKY IS OUR DOMAIN!","LOOK UP AND BATTLE!",{333,279},{91,91},2},{"PICNICKER","KYLEE","picnicker_front_pic","OUR BIRDS RIDE","THE FORTREE WINDS!",{227,357},{92,92},2}}
 ,{{"PSYCHIC","PRESTON","psychic_m_front_pic","OUR MINDS ARE LINKED.","CAN YOUR TEAM KEEP UP?",{344,178},{92,92},2},{"PSYCHIC","MAURA","psychic_f_front_pic","I SEE YOUR FUTURE.","IT ENDS RIGHT HERE!",{337,338},{93,93},2}}
 ,{{"POKEMON RANGER","ANDREA","cool_trainer_f_front_pic","WATER FLOWS AROUND","EVERY OBSTACLE!",{340,364},{93,93},2},{"POKEMON RANGER","PARKER","cool_trainer_m_front_pic","JUAN'S STYLE IS","AS DEEP AS THE SEA!",{342,365},{94,94},2}}
};
uint32_t gymPersonalitySeed(uint8_t gym, uint8_t stage, uint8_t slot) {
  return 0x47594D31U ^ (static_cast<uint32_t>(gym) << 16U) ^
         (static_cast<uint32_t>(stage) << 8U) ^ slot;
}

void loadStage(BattleState& battle) {
  const uint8_t gym = battle.gymId;
  if (gym >= 24) return;
  battle.opponentIndex = 0;
  battle.playerSafeguardTurns = 0;
  battle.opponentSafeguardTurns = 0;
  battle.playerReflectTurns = battle.opponentReflectTurns = 0;
  battle.playerLightScreenTurns = battle.opponentLightScreenTurns = 0;
  battle.playerMistTurns = battle.opponentMistTurns = 0;
  battle.playerProtectChain = battle.opponentProtectChain = 0;
  battle.playerEndureThisTurn = battle.opponentEndureThisTurn = false;
  if (battle.gymStage < 2) {
    const Challenger& challenger = kChallengers[gym][battle.gymStage];
    battle.opponentCount = challenger.count;
    battle.rewardMoney = 0;
    battle.opponentItemUses = 0;
    for (uint8_t i = 0; i < challenger.count; ++i)
      battle.opponents[i] = CollectionLogic::createPokemon(
          0, challenger.species[i], challenger.levels[i], false,
          gymPersonalitySeed(gym, battle.gymStage, i));
  } else {
    const GymDefinition& definition = kGyms[gym];
    battle.opponentCount = definition.teamSize;
    battle.rewardMoney = static_cast<uint32_t>(GymSystem::requiredLevel(definition.id)) * 100U;
    battle.opponentItemUses = 2;
    for (uint8_t i = 0; i < definition.teamSize; ++i)
      battle.opponents[i] = CollectionLogic::createPokemon(
          0, definition.team[i].speciesId, definition.team[i].level, false,
          gymPersonalitySeed(gym, battle.gymStage, i));
  }
  BattleEngine::orderOpponentTeamWeakestFirst(battle);
  BattleEngine::ensureOpponentUids(battle);
}
}

const GymDefinition* GymSystem::definition(GymId id) {
  const uint8_t index = static_cast<uint8_t>(id);
  return index < static_cast<uint8_t>(GymId::Count) ? &kGyms[index] : nullptr;
}

bool GymSystem::hasBadge(const GymProgress& progress, GymId id) {
  const uint8_t index = static_cast<uint8_t>(id);
  return index < 24 && (progress.badgeBits & (1UL << index));
}

uint8_t GymSystem::badgeCount(const GymProgress& progress) {
  uint8_t count=0;for(uint8_t i=0;i<24;++i)if(progress.badgeBits&(1UL<<i))++count;return count;
}

GymId GymSystem::next(const GymProgress& progress) {
  const uint8_t limit = static_cast<uint8_t>(progress.unlockedGeneration * 8U);
  for (uint8_t index = 0; index < limit; ++index) {
    const GymId id = static_cast<GymId>(index);
    if (!hasBadge(progress, id)) return id;
  }
  return GymId::Count;
}

bool GymSystem::regionComplete(const GymProgress& progress, uint8_t generation) {
  if (generation < 1 || generation > 3) return false;
  const uint8_t first = static_cast<uint8_t>((generation - 1U) * 8U);
  for (uint8_t i = first; i < first + 8U; ++i)
    if ((progress.badgeBits & (1UL << i)) == 0) return false;
  return true;
}

uint8_t GymSystem::requiredLevel(GymId id) {
  const GymDefinition* gym = definition(id);
  if (!gym || !gym->teamSize) return 0U;
  uint8_t required = gym->team[0].level;
  for (uint8_t slot = 1; slot < gym->teamSize; ++slot)
    if (gym->team[slot].level > required) required = gym->team[slot].level;
  return required;
}

uint8_t GymSystem::unlockLevel(GymId id) {
  // Falkner becomes available as soon as the player's strongest selected
  // Pokemon reaches the level of his first team member. His actual team and
  // ace remain Lv.64/65/66.
  if (id == GymId::Violet) return 64U;
  return requiredLevel(id);
}

bool GymSystem::challengeAvailable(const GymProgress& progress,
                                   uint8_t highestPartyLevel) {
  const GymId gym = next(progress);
  const uint8_t required = unlockLevel(gym);
  return gym != GymId::Count && required && highestPartyLevel >= required;
}

bool GymSystem::leagueAvailable(const GymProgress& progress) {
  return regionComplete(progress, progress.unlockedGeneration) &&
         !leagueComplete(progress, progress.unlockedGeneration);
}

bool GymSystem::leagueComplete(const GymProgress& progress, uint8_t generation) {
  return generation >= 1 && generation <= 3 &&
         (progress.leagueChampionBits & (1U << (generation - 1U))) != 0;
}

bool GymSystem::recordLeagueVictory(GymProgress& progress, uint8_t generation) {
  if (generation != progress.unlockedGeneration || !leagueAvailable(progress)) return false;
  progress.leagueChampionBits |= static_cast<uint8_t>(1U << (generation - 1U));
  return true;
}

bool GymSystem::needsRegionalStarter(const GymProgress& progress) {
  if (progress.unlockedGeneration >= 3) return false;
  const uint8_t nextGeneration = static_cast<uint8_t>(progress.unlockedGeneration + 1U);
  return leagueComplete(progress, progress.unlockedGeneration) &&
         (progress.starterClaimedBits & (1U << (nextGeneration - 1U))) == 0;
}

bool GymSystem::unlockWithStarter(GymProgress& progress, uint16_t speciesId) {
  if (!needsRegionalStarter(progress)) return false;
  const uint8_t nextGeneration = static_cast<uint8_t>(progress.unlockedGeneration + 1U);
  const bool valid = nextGeneration == 2 ? (speciesId == 152 || speciesId == 155 || speciesId == 158)
                                        : (speciesId == 252 || speciesId == 255 || speciesId == 258);
  if (!valid) return false;
  progress.starterClaimedBits |= static_cast<uint8_t>(1U << (nextGeneration - 1U));
  progress.unlockedGeneration = nextGeneration;
  return true;
}

bool GymSystem::start(BattleState& battle, PokemonCollection& collection, uint32_t playerUid,
                      const GymProgress& progress, GymId id, uint32_t seed) {
  const GymDefinition* gym = definition(id);
  OwnedPokemon* player = CollectionLogic::find(collection, playerUid);
  if (!gym || battle.active || id != next(progress) ||
      !challengeAvailable(progress, BattleEngine::highestPartyLevel(collection)) || !player ||
      !CollectionLogic::isInParty(collection, playerUid) || player->currentHp == 0 ||
      player->recoverySecondsRemaining) return false;
  BattleEngine::clear(battle); battle.active = true; battle.kind = BattleKind::Gym;
  battle.outcome = BattleOutcome::Ongoing; battle.playerUid = playerUid;
  battle.unlockedGeneration = progress.unlockedGeneration;
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
  progress.badgeBits |= (1UL << static_cast<uint8_t>(id));
  return true;
}
