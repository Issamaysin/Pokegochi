#include "game/AlolaAdventure.h"
#include <algorithm>
#include <iterator>
#include "game/AlolaForms.h"
#include "game/Economy.h"
#include "game/Pokedex.h"

namespace {
constexpr uint8_t kRunActive=1U<<0U,kHealUsed=1U<<1U,
    kChoiceEntered=1U<<2U,kChoicesCached=1U<<3U,
    kHiddenAbility=1U<<0U,kAlolanForm=1U<<1U;
uint32_t advance(uint32_t& x){if(!x)x=0x414C4F4CUL;x^=x<<13;x^=x>>17;x^=x<<5;return x;}
uint32_t mixed(uint32_t x){x^=x>>16;x*=0x7FEB352DUL;x^=x>>15;x*=0x846CA68BUL;return x^(x>>16);}

constexpr uint16_t kRosters[AlolaAdventure::kAreaCount][12]={
 {734,736,742,744,19,20,52,731,732,735,741,742},
 {739,56,66,296,619,739,744,759,766,297,307,759},
 {728,729,751,752,746,747,748,54,61,90,120,129},
 {725,726,776,757,758,105,37,38,126,218,240,324},
 {722,723,753,754,761,762,763,764,755,756,103,708},
 {74,75,76,95,299,525,524,744,745,769,770,526},
 {736,737,738,777,81,82,100,125,170,179,595,596},
 {769,770,778,779,562,563,607,708,88,89,355,442},
 {52,53,198,215,430,551,552,559,570,765,766,767},
 {782,783,784,714,715,621,622,623,624,633,634,635},
 {749,750,551,552,529,530,443,444,445,329,330,423},
 {724,727,730,733,738,743,745,750,758,763,768,784},
 {793,794,795,796,797,798,799,803,804,805,806,800},
 {34,53,105,150,382,383,487,643,646,717,791,792},
};

// Rainbow Rocket's trainer roster intentionally contains the villains'
// Legendary partners. Wild encounters must not reuse that roster or those
// partners become as common as ordinary route Pokemon.
constexpr uint16_t kRainbowRocketWildRoster[12]={
 34,53,105,20,89,110,229,430,553,625,687,758,
};
// Ultra Beasts belong to the explicit wormhole mechanic, never to an area's
// ordinary Legendary/Mythical roll. Poipole remains the rare wormhole member
// while Necrozma shares that authored 1/240 pair as the wormhole's guardian.
constexpr uint16_t kUltraWormholeWildRoster[]={
 793,794,795,796,797,798,799,804,805,806,
};
constexpr uint16_t kUltraWormholeRarePool[]={800,803};
// All ordinary rare rolls in Alola are limited to the region's own official
// Legendary and Mythical species. Ultra Beasts are intentionally absent.
constexpr uint16_t kAlolaSpecialPool[]={
 772,773,785,786,787,788,789,790,791,792,800,801,802,807,
};
constexpr uint8_t kUltraWormholeWildCount=static_cast<uint8_t>(
    sizeof(kUltraWormholeWildRoster)/sizeof(kUltraWormholeWildRoster[0]));
constexpr uint8_t kUltraWormholeRareCount=static_cast<uint8_t>(
    sizeof(kUltraWormholeRarePool)/sizeof(kUltraWormholeRarePool[0]));
constexpr uint8_t kAlolaSpecialCount=static_cast<uint8_t>(
    sizeof(kAlolaSpecialPool)/sizeof(kAlolaSpecialPool[0]));
constexpr bool ultraWormholePoolsAreSeparated(){
 for(uint16_t common:kUltraWormholeWildRoster)
  for(uint16_t rare:kUltraWormholeRarePool)
   if(common==rare)return false;
 return true;
}
constexpr bool rainbowRocketPoolsAreSeparated(){
 for(uint16_t common:kRainbowRocketWildRoster)
  for(uint16_t special:kAlolaSpecialPool)
   if(common==special)return false;
 return true;
}
static_assert(kUltraWormholeWildCount==10U,
              "Ultra Wormhole must keep its ten common Ultra Beasts");
static_assert(kUltraWormholeRareCount==2U,
              "Ultra Wormhole must keep Necrozma and Poipole rare");
static_assert(ultraWormholePoolsAreSeparated(),
              "Necrozma and Poipole must remain rare wormhole encounters");
static_assert(kAlolaSpecialCount==14U,
              "Alola must keep its eleven Legendaries and three Mythicals");
static_assert(rainbowRocketPoolsAreSeparated(),
              "Rainbow Rocket specials must not leak into common encounters");

// The pairings repeat regional species where that produces a more thematic
// hunt. Rainbow Rocket uses all fourteen; its row stays empty because its
// selection is wider than the two-slot site table.
constexpr uint16_t kSpecialEncounterPools[AlolaAdventure::kAreaCount][2]={
 {785,801}, // Verdant Cavern: Tapu Koko, Magearna
 {785,807}, // Melemele: Tapu Koko, Zeraora
 {788,789}, // Brooklet Hill: Tapu Fini, Cosmog
 {791,802}, // Wela Volcano: Solgaleo, Marshadow
 {787,801}, // Lush Jungle: Tapu Bulu, Magearna
 {786,772}, // Akala: Tapu Lele, Type: Null
 {789,807}, // Hokulani: Cosmog, Zeraora
 {802,773}, // Thrifty Megamart: Marshadow, Silvally
 {772,773}, // Ula'ula: Type: Null, Silvally
 {790,792}, // Vast Poni Canyon: Cosmoem, Lunala
 {788,800}, // Poni: Tapu Fini, Necrozma
 {791,792}, // League: Solgaleo, Lunala
 {800,790}, // Ultra Wormhole site: Necrozma, Cosmoem
 {0,0},     // Rainbow Rocket uses the complete regional pool above.
};

constexpr bool alolaSpecialSpecies(uint16_t species){
 for(uint16_t candidate:kAlolaSpecialPool)
  if(candidate==species)return true;
 return false;
}
constexpr bool specialEncounterSitesContain(uint16_t species){
 for(uint8_t area=0;area+1U<AlolaAdventure::kAreaCount;++area)
  for(uint8_t slot=0;slot<2U;++slot)
   if(kSpecialEncounterPools[area][slot]==species)return true;
 return false;
}
constexpr bool allSiteSpecialPoolsAreRegional(){
 for(uint8_t area=0;area+1U<AlolaAdventure::kAreaCount;++area)
  for(uint8_t slot=0;slot<2U;++slot)
   if(!alolaSpecialSpecies(kSpecialEncounterPools[area][slot]))return false;
 return true;
}
constexpr bool allAlolaSpecialSpeciesAreDistributed(){
 for(uint16_t species:kAlolaSpecialPool)
  if(!specialEncounterSitesContain(species))return false;
 return true;
}
static_assert(allSiteSpecialPoolsAreRegional(),
              "Alola site rare pools must contain only Gen-VII natives");
static_assert(allAlolaSpecialSpeciesAreDistributed(),
              "Every Gen-VII Legendary/Mythical needs a site encounter");
constexpr uint8_t kRainbowRocketArea=
    static_cast<uint8_t>(AlolaArea::RainbowRocket);
constexpr uint8_t kUltraWormholeArea=
    static_cast<uint8_t>(AlolaArea::UltraWormhole);
uint16_t ultraWormholeWildSpecies(uint32_t roll){
 return kUltraWormholeWildRoster[(roll>>8U)%kUltraWormholeWildCount];
}
uint16_t ultraWormholeRareSpecies(uint32_t roll){
 return kUltraWormholeRarePool[(roll>>10U)%kUltraWormholeRareCount];
}
uint16_t ordinaryWildSpecies(uint8_t area,uint32_t roll){
 const uint8_t slot=static_cast<uint8_t>((roll>>8U)%12U);
 if(area==kRainbowRocketArea)return kRainbowRocketWildRoster[slot];
 if(area==kUltraWormholeArea)return ultraWormholeWildSpecies(roll);
 return kRosters[area][slot];
}
uint16_t specialWildSpecies(uint8_t area,uint32_t roll){
 if(area==kRainbowRocketArea)
  return kAlolaSpecialPool[(roll>>10U)%kAlolaSpecialCount];
 return kSpecialEncounterPools[area][(roll>>10U)&1U];
}
constexpr uint16_t kBosses[AlolaAdventure::kAreaCount][3]={
 {734,735,20},{739,766,760},{751,752,746},{105,757,758},
 {753,754,763},{299,745,76},{738,777,462},{769,770,778},
 {53,430,768},{782,783,784},{750,423,330},{26,715,730},
 {793,804,800},{34,150,53},
};
struct TotemAura {int8_t attack,defense,spAttack,spDefense,speed;};
// Trial auras are permanent for the featured ace while it waits in the
// opponent party. Kahunas, the League and Rainbow Rocket use ordinary teams;
// Ultra Necrozma receives its characteristic omni-boost.
constexpr TotemAura kTotemAuras[AlolaAdventure::kAreaCount]={
 {0,1,0,0,0},{0,0,0,0,0},{0,0,0,0,1},{0,0,0,0,2},
 {0,0,0,0,2},{0,0,0,0,0},{0,1,0,0,0},{1,1,1,1,1},
 {0,0,0,0,0},{1,1,1,1,1},{0,0,0,0,0},{0,0,0,0,0},
 {1,1,1,1,1},{0,0,0,0,0},
};
constexpr const char* kNames[]={
 "VERDANT CAVERN","MELEMELE GRAND TRIAL","BROOKLET HILL","WELA VOLCANO",
 "LUSH JUNGLE","AKALA GRAND TRIAL","HOKULANI OBSERVATORY","THRIFTY MEGAMART",
 "ULA'ULA GRAND TRIAL","VAST PONI CANYON","PONI GRAND TRIAL","POKEMON LEAGUE",
 "ULTRA WORMHOLE","RAINBOW ROCKET",
};
constexpr const char* kSubtitles[]={
 "ILIMA'S TRIAL AND THE FIRST TOTEM","FACE KAHUNA HALA UNDER THE MOON",
 "LANA'S SPLASHING TRIAL","KIAWE'S DANCE ABOVE THE MAGMA",
 "MALLOW'S JUNGLE RECIPE","KAHUNA OLIVIA'S Z-POWER",
 "SOPHOCLES LIGHTS THE SUMMIT","ACEROLA'S HAUNTED TRIAL",
 "TEAM SKULL AND KAHUNA NANU","MINA AND KOMMO-O GUARD PONI",
 "FACE KAHUNA HAPU","BECOME ALOLA'S FIRST CHAMPION",
 "HUNT ULTRA BEASTS ACROSS SPACE","STOP GIOVANNI AT AETHER PARADISE",
};
constexpr const char* kBossNames[]={"TOTEM GUMSHOOS","HALA","TOTEM ARAQUANID",
 "TOTEM MAROWAK","TOTEM LURANTIS","OLIVIA","TOTEM VIKAVOLT","TOTEM MIMIKYU",
 "NANU","TOTEM KOMMO-O","HAPU","CHAMPION HAU","ULTRA NECROZMA","GIOVANNI"};
constexpr HeldItem kCrystals[]={HeldItem::NormaliumZ,HeldItem::FightiniumZ,
 HeldItem::WateriumZ,HeldItem::FiriumZ,HeldItem::GrassiumZ,HeldItem::RockiumZ,
 HeldItem::ElectriumZ,HeldItem::GhostiumZ,HeldItem::DarkiniumZ,HeldItem::DragoniumZ,
 HeldItem::GroundiumZ,HeldItem::FairiumZ,HeldItem::PsychiumZ,HeldItem::FlyiniumZ};

uint8_t areaIndex(AlolaArea area){return std::min<uint8_t>(static_cast<uint8_t>(area),AlolaAdventure::kAreaCount-1U);}
uint8_t levelFor(const PokemonCollection& c,uint8_t area){return std::min<uint8_t>(100U,std::max<uint8_t>(BattleEngine::highestPartyLevel(c),static_cast<uint8_t>(72U+area*2U)));}
bool healthy(const AlolaRunState& run,const PokemonCollection& c,uint32_t uid){
 bool selected=false;for(uint32_t member:run.partyUids)if(member==uid&&uid)selected=true;
 const OwnedPokemon* p=CollectionLogic::find(c,uid);return selected&&p&&CollectionLogic::isInParty(c,uid)&&p->currentHp&&!p->recoverySecondsRemaining;
}
bool enteredAnyChoice(const AlolaRunState& run){return
    (run.flags&static_cast<uint8_t>(kChoiceEntered|kHealUsed))!=0U||
    run.step!=0U||run.pendingBattle!=AlolaBattleRole::None||
    run.pendingFlags!=0U||run.pendingSpecies!=0U;}
bool regional(uint16_t id){return AlolaForms::isRegionalSpecies(id);}
void makeAlolan(OwnedPokemon& p){if(regional(p.speciesId)){p.form=1U;CollectionLogic::refreshAbility(p,7U);CollectionLogic::refreshDerivedStats(p,true);}}
void initBattle(BattleState& b,uint32_t uid,uint32_t seed,BattleKind kind){BattleEngine::clear(b);b.active=true;b.kind=kind;b.outcome=BattleOutcome::Ongoing;b.playerUid=uid;b.unlockedGeneration=7U;b.rngState=seed?seed:0x414C4F4CUL;}
bool ownsHidden(const PokemonCollection& c,uint16_t id){for(const auto& p:c.box)if(p.uid&&p.speciesId==id&&CollectionLogic::hasHiddenAbility(p))return true;return false;}
uint16_t sosSpecies(const PokemonCollection& c,uint32_t roll){
 uint16_t missing=0,total=0;for(uint16_t id=722;id<=784;++id){const auto* s=findSpecies(id);if(!s||!s->hiddenAbility)continue;++total;if(!ownsHidden(c,id))++missing;}
 uint16_t pick=static_cast<uint16_t>(roll%(missing?missing:std::max<uint16_t>(1,total)));
 for(uint16_t id=722;id<=784;++id){const auto* s=findSpecies(id);if(!s||!s->hiddenAbility||(missing&&ownsHidden(c,id)))continue;if(!pick--)return id;}return 722;
}
void applyTotemAura(BattleState& battle,uint8_t area){
 const TotemAura aura=kTotemAuras[area];
 if(!(aura.attack||aura.defense||aura.spAttack||aura.spDefense||aura.speed))return;
 for(uint8_t slot=0;slot<battle.opponentCount;++slot){
  if(battle.opponents[slot].speciesId!=kBosses[area][0])continue;
  CombatVolatile& stages=battle.opponentVolatiles[slot];
  stages.attackStage=aura.attack;stages.defenseStage=aura.defense;
  stages.spAttackStage=aura.spAttack;stages.spDefenseStage=aura.spDefense;
  stages.speedStage=aura.speed;break;
 }
}
}

bool AlolaAdventure::available(const KalosAdventureState& kalos){return KalosAdventure::championCompleted(kalos);}
bool AlolaAdventure::reconcile(AlolaAdventureState& state,const KalosAdventureState& kalos){if(!available(kalos)||state.unlockedAreas)return false;state.unlockedAreas=1U;if(!state.rngState)state.rngState=0x414C4F4CUL;return true;}
bool AlolaAdventure::isAreaUnlocked(const AlolaAdventureState& s,AlolaArea a){const uint8_t i=static_cast<uint8_t>(a);return i<kAreaCount&&(s.unlockedAreas&(1U<<i));}
bool AlolaAdventure::championCompleted(const AlolaAdventureState& s){return s.flags&kChampionCompleteBit;}
bool AlolaAdventure::fullyCompleted(const AlolaAdventureState& s){return championCompleted(s)&&(s.defeatedBosses&kAllAreasMask)==kAllAreasMask;}
bool AlolaAdventure::reconcileCompletionReward(AlolaAdventureState& s,Inventory& i){if(!fullyCompleted(s)||(s.flags&kMasterBallRewardBit))return false;uint16_t& q=i.balls[static_cast<uint8_t>(PokeBallType::MasterBall)];if(q>=kInventoryStackLimit)return false;++q;s.flags|=kMasterBallRewardBit;return true;}
bool AlolaAdventure::sunThemeUnlocked(const AlolaAdventureState& s){return championCompleted(s);}
bool AlolaAdventure::sunThemeSelected(const AlolaAdventureState& s){return sunThemeUnlocked(s)&&(s.flags&kSunThemeSelectedBit);}
bool AlolaAdventure::setSunThemeSelected(AlolaAdventureState& s,bool selected){if(selected&&!sunThemeUnlocked(s))return false;const uint8_t old=s.flags;if(selected){s.flags|=kSunThemeSelectedBit;s.flags&=static_cast<uint8_t>(~kMoonThemeSelectedBit);}else s.flags&=static_cast<uint8_t>(~kSunThemeSelectedBit);return old!=s.flags;}
bool AlolaAdventure::moonThemeUnlocked(const AlolaAdventureState& s){return championCompleted(s);}
bool AlolaAdventure::moonThemeSelected(const AlolaAdventureState& s){return moonThemeUnlocked(s)&&(s.flags&kMoonThemeSelectedBit);}
bool AlolaAdventure::setMoonThemeSelected(AlolaAdventureState& s,bool selected){if(selected&&!moonThemeUnlocked(s))return false;const uint8_t old=s.flags;if(selected){s.flags|=kMoonThemeSelectedBit;s.flags&=static_cast<uint8_t>(~kSunThemeSelectedBit);}else s.flags&=static_cast<uint8_t>(~kMoonThemeSelectedBit);return old!=s.flags;}
uint8_t AlolaAdventure::trialCount(const AlolaAdventureState& s){uint8_t n=0;for(uint8_t a=0;a<12;++a)if(s.defeatedBosses&(1U<<a))++n;return n;}
const char* AlolaAdventure::areaName(AlolaArea a){return kNames[areaIndex(a)];}
const char* AlolaAdventure::areaSubtitle(AlolaArea a){return kSubtitles[areaIndex(a)];}
const char* AlolaAdventure::bossName(AlolaArea a){return kBossNames[areaIndex(a)];}
HeldItem AlolaAdventure::areaZCrystal(AlolaArea a){return kCrystals[areaIndex(a)];}
HeldItem AlolaAdventure::areaBonusZCrystal(AlolaArea a){
 switch(areaIndex(a)){
  case 5U:return HeldItem::PoisoniumZ;
  case 7U:return HeldItem::BuginiumZ;
  case 9U:return HeldItem::IciumZ;
  case 12U:return HeldItem::SteeliumZ;
  default:return HeldItem::None;
 }
}

bool AlolaAdventure::begin(AlolaAdventureState& s,AlolaArea a,uint32_t seed,const PokemonCollection& c){if(!isAreaUnlocked(s,a))return false;bool ready=false;for(uint8_t slot=0;slot<kPartyCapacity;++slot){const auto* p=CollectionLogic::active(c,slot);ready|=p&&p->currentHp&&!p->recoverySecondsRemaining;}if(!ready)return false;const bool restore=(s.run.flags&kChoicesCached)!=0U&&(s.run.flags&kRunActive)==0U;const uint32_t cachedSeed=s.run.seed;s.run=AlolaRunState{};s.run.seed=restore?cachedSeed:(seed?seed:mixed(advance(s.rngState)));s.run.area=areaIndex(a);s.run.flags=kRunActive;std::copy(std::begin(c.party),std::end(c.party),std::begin(s.run.partyUids));return true;}
bool AlolaAdventure::willPreserveChoicesOnAbandon(const AlolaAdventureState& s){return active(s)&&!enteredAnyChoice(s.run);}
bool AlolaAdventure::abandon(AlolaAdventureState& s){if(!active(s))return false;const bool preserve=willPreserveChoicesOnAbandon(s);const uint32_t cachedSeed=s.run.seed;s.run=AlolaRunState{};if(preserve){s.run.seed=cachedSeed;s.run.flags=kChoicesCached;}return true;}
bool AlolaAdventure::active(const AlolaAdventureState& s){return s.run.flags&kRunActive;}
uint8_t AlolaAdventure::choices(const AlolaAdventureState& s,AlolaRoomChoice out[2]){
 if(!out||!active(s)||s.run.pendingBattle!=AlolaBattleRole::None)return 0;if(s.run.step>=kStepsPerArea){out[0]={AlolaRoomKind::Trial,0,true};return 1;}
 const uint32_t roll=mixed(s.run.seed^(static_cast<uint32_t>(s.run.step+1U)*0x9E3779B9UL));
 const bool skull=s.run.area==1U||s.run.area==8U||s.run.area==9U;
 const bool aether=s.run.area==11U||s.run.area>=12U;
 out[0]={skull&&s.run.step==3U?AlolaRoomKind::TeamSkull:aether&&s.run.step==3U?AlolaRoomKind::Aether:AlolaRoomKind::Trainer,0,skull||aether};
 static constexpr AlolaRoomKind wild[]={AlolaRoomKind::Grass,AlolaRoomKind::SosChain,AlolaRoomKind::IslandScan,AlolaRoomKind::Item,AlolaRoomKind::Healing,AlolaRoomKind::UltraWormhole};
 AlolaRoomKind alt=wild[(roll>>5U)%6U];if(alt==AlolaRoomKind::Healing&&(s.run.flags&kHealUsed))alt=AlolaRoomKind::Grass;if(alt==AlolaRoomKind::UltraWormhole&&s.run.area<12U)alt=AlolaRoomKind::SosChain;out[1]={alt,1,alt==AlolaRoomKind::SosChain||alt==AlolaRoomKind::UltraWormhole};return 2;
}
AlolaRoomResult AlolaAdventure::choose(AlolaAdventureState& s,uint8_t choice,
                                       PokemonCollection& c){
 AlolaRoomResult r;AlolaRoomChoice options[2]{};
 const uint8_t count=choices(s,options);if(choice>=count)return r;
 s.run.flags|=kChoiceEntered;
 r.kind=options[choice].kind;
 const uint32_t roll=mixed(s.run.seed^
     (static_cast<uint32_t>(s.run.step+1U)<<16U)^choice);
 const uint8_t area=std::min<uint8_t>(s.run.area,kAreaCount-1U);
 switch(r.kind){
  case AlolaRoomKind::Healing:r.healed=useAreaHeal(s,c);break;
  case AlolaRoomKind::Item:r.foundItem=true;
   r.rewardKind=static_cast<uint8_t>(1U+((roll>>6U)%4U));break;
  case AlolaRoomKind::Trainer:s.run.pendingBattle=AlolaBattleRole::Trainer;
   r.startsBattle=true;break;
  case AlolaRoomKind::TeamSkull:s.run.pendingBattle=AlolaBattleRole::Skull;
   r.startsBattle=true;break;
  case AlolaRoomKind::Aether:s.run.pendingBattle=AlolaBattleRole::Aether;
   r.startsBattle=true;break;
  case AlolaRoomKind::Trial:s.run.pendingBattle=AlolaBattleRole::Totem;
   r.startsBattle=true;break;
  default:{
   const bool hidden=r.kind==AlolaRoomKind::SosChain;
   const bool wormhole=r.kind==AlolaRoomKind::UltraWormhole;
   const bool special=!hidden&&
       roll%kLegendaryChanceDenominator==0U;
   uint16_t species=hidden?sosSpecies(c,roll>>7U):
       ordinaryWildSpecies(area,roll);
   if(wormhole)species=special?
       ultraWormholeRareSpecies(roll):
       ultraWormholeWildSpecies(roll);
   else if(special)species=specialWildSpecies(area,roll);
   s.run.pendingSpecies=species;
   s.run.pendingFlags=(hidden?kHiddenAbility:0U)|
       (regional(species)?kAlolanForm:0U);
   s.run.pendingBattle=AlolaBattleRole::Wild;r.startsBattle=true;
   r.speciesId=species;r.hiddenAbilityEncounter=hidden;
   r.legendaryEncounter=wormhole||special;break;
  }
 }
 if(!r.startsBattle&&s.run.step<kStepsPerArea)++s.run.step;
 return r;
}
bool AlolaAdventure::startPendingBattle(AlolaAdventureState& s,BattleState& b,PokemonCollection& c,uint32_t uid){
 if(!active(s)||s.run.pendingBattle==AlolaBattleRole::None||b.active||!healthy(s.run,c,uid))return false;const uint8_t area=std::min<uint8_t>(s.run.area,kAreaCount-1U),level=levelFor(c,area);const uint32_t seed=mixed(s.run.seed^s.run.step^(static_cast<uint32_t>(s.run.pendingBattle)<<24U));
 if(s.run.pendingBattle==AlolaBattleRole::Wild){initBattle(b,uid,seed,BattleKind::Wild);OwnedPokemon p=CollectionLogic::createPokemon(0,s.run.pendingSpecies,level,CollectionLogic::isShinyRoll(seed>>3U),seed,7U);if(!p.speciesId){BattleEngine::clear(b);return false;}if(s.run.pendingFlags&kAlolanForm)makeAlolan(p);if(s.run.pendingFlags&kHiddenAbility)CollectionLogic::setHiddenAbility(p,true,7U);b.opponents[0]=p;b.opponentCount=1;b.opponentIndex=0;}
 else{initBattle(b,uid,seed,BattleKind::AlolaAdventure);const bool boss=s.run.pendingBattle==AlolaBattleRole::Totem;const bool major=s.run.pendingBattle==AlolaBattleRole::Skull||s.run.pendingBattle==AlolaBattleRole::Aether;b.opponentCount=boss?3U:major?2U:1U;b.rewardMoney=static_cast<uint32_t>(level)*(boss?160U:major?95U:60U);for(uint8_t slot=0;slot<b.opponentCount;++slot){const uint16_t id=boss?kBosses[area][slot]:kRosters[area][(seed+slot*5U)%12U];b.opponents[slot]=CollectionLogic::createPokemon(0,id,static_cast<uint8_t>(std::min<uint16_t>(100U,level+(boss?2U:0U))),false,seed^(slot*0x9E3779B9UL),7U);if(regional(id))makeAlolan(b.opponents[slot]);}BattleEngine::orderOpponentTeamWeakestFirst(b);}
 BattleEngine::ensureOpponentUids(b);BattleEngine::applyEntryAbilities(b,c);
 if(s.run.pendingBattle==AlolaBattleRole::Totem)applyTotemAura(b,area);
 return true;
}
bool AlolaAdventure::recordBattle(AlolaAdventureState& s,BattleOutcome outcome,Gen7UniqueItemInventory& items){
 if(!active(s)||s.run.pendingBattle==AlolaBattleRole::None||outcome==BattleOutcome::Ongoing||outcome==BattleOutcome::None)return false;const auto role=s.run.pendingBattle;s.run.pendingBattle=AlolaBattleRole::None;s.run.pendingSpecies=0;s.run.pendingFlags=0;if(outcome!=BattleOutcome::Victory&&outcome!=BattleOutcome::Captured){s.run=AlolaRunState{};return true;}if(role==AlolaBattleRole::Totem){const uint8_t area=std::min<uint8_t>(s.run.area,kAreaCount-1U);s.defeatedBosses|=static_cast<uint16_t>(1U<<area);const auto grant=[&](HeldItem crystal){const int8_t index=gen7UniqueItemIndex(crystal);if(index>=0){const uint64_t bit=uint64_t{1}<<static_cast<uint8_t>(index);items.claimedMask|=bit;items.bagMask|=bit;}};grant(areaZCrystal(static_cast<AlolaArea>(area)));grant(areaBonusZCrystal(static_cast<AlolaArea>(area)));if(area+1U<kAreaCount)s.unlockedAreas|=static_cast<uint16_t>(1U<<(area+1U));if(area==11U)s.flags|=kChampionCompleteBit;s.run=AlolaRunState{};return true;}if(s.run.step<kStepsPerArea)++s.run.step;return true;
}
bool AlolaAdventure::useAreaHeal(AlolaAdventureState& s,PokemonCollection& c){if(!active(s)||(s.run.flags&kHealUsed))return false;for(uint32_t uid:s.run.partyUids){OwnedPokemon* p=CollectionLogic::find(c,uid);if(!p)continue;const bool fainted=!p->currentHp||p->recoverySecondsRemaining;if(!fainted)p->currentHp=p->maximumHp;p->status=StatusCondition::None;for(uint8_t m=0;m<kMoveSlots;++m)p->movePp[m]=CollectionLogic::maximumMovePp(*p,m);}s.run.flags|=kHealUsed;return true;}
