#include "game/EggSystem.h"
#include <algorithm>
#include <cstring>

namespace{
uint32_t rng(uint32_t& x){if(!x)x=0x6D2B79F5U;x^=x<<13;x^=x>>17;x^=x<<5;return x;}
constexpr uint16_t kStarterSpecies[]={1,4,7,152,155,158,252,255,258};
bool excluded(uint16_t id){switch(id){case 1:case 4:case 7:case 152:case 155:case 158:case 252:case 255:case 258:
 case 144:case 145:case 146:case 150:case 151:case 243:case 244:case 245:case 249:case 250:case 251:
 case 377:case 378:case 379:case 380:case 381:case 382:case 383:case 384:case 385:case 386:return true;default:return false;}}
bool basic(uint16_t id){for(uint16_t from=1;from<=386;++from){const EvolutionData* e=evolutionFor(from);if(e&&e->toSpeciesId==id)return false;}return true;}
uint16_t limit(uint8_t region){return region==1?151:region==2?251:386;}
EggRarity rarityFor(const SpeciesData& s){return s.catchRate<=45?EggRarity::Rare:s.catchRate<=120?EggRarity::Uncommon:EggRarity::Common;}
bool baby(uint16_t id){switch(id){case 172:case 173:case 174:case 175:case 236:case 238:case 239:case 240:case 298:case 360:return true;default:return false;}}
uint16_t choose(uint8_t region,EggRarity rarity,uint32_t& seed){uint16_t weight=0;for(uint16_t id=1;id<=limit(region);++id){const SpeciesData* s=findSpecies(id);if(s&&!excluded(id)&&basic(id)&&rarityFor(*s)==rarity)weight+=baby(id)?4:1;}if(!weight)return 16;uint16_t pick=static_cast<uint16_t>(rng(seed)%weight);for(uint16_t id=1;id<=limit(region);++id){const SpeciesData* s=findSpecies(id);if(!s||excluded(id)||!basic(id)||rarityFor(*s)!=rarity)continue;const uint8_t w=baby(id)?4:1;if(pick<w)return id;pick-=w;}return 16;}
uint8_t badges(const GymProgress& p,uint8_t region){uint8_t n=0,first=(region-1U)*8U;for(uint8_t i=0;i<8;++i)if(p.badgeBits&(1UL<<(first+i)))++n;return n;}
void makeOffer(EggState& e,uint8_t region,uint32_t seed){const uint32_t roll=rng(seed)%100U;e.rarity=roll<75?EggRarity::Common:roll<97?EggRarity::Uncommon:EggRarity::Rare;e.region=region;e.speciesId=choose(region,e.rarity,seed);e.shinyRoll=rng(seed);e.offerPending=true;}
uint32_t bonus(EggState& e,uint32_t seconds){if(!e.active||!e.remainingSeconds)return 0;const uint32_t room=EggSystem::kDailyBonusLimitSeconds-e.dailyBonusUsedSeconds;const uint32_t used=std::min({seconds,room,e.remainingSeconds});e.remainingSeconds-=used;e.dailyBonusUsedSeconds+=used;return used;}
StoredEgg makeSpecialEgg(const PokedexState& pokedex,uint8_t unlockedGeneration,
                         uint32_t seed){
  uint16_t availableStarters[sizeof(kStarterSpecies)/sizeof(kStarterSpecies[0])]{};
  uint8_t availableCount=0;
  for(uint16_t species:kStarterSpecies)
    if(!PokedexLogic::hasCaught(pokedex,species))availableStarters[availableCount++]=species;
  const uint8_t generation=std::max<uint8_t>(1U,
      std::min<uint8_t>(5U,unlockedGeneration));
  const uint16_t speciesLimit=PokedexLogic::generationSpeciesCount(generation);
  uint8_t legendaryCount=0;
  for(uint16_t species=1;species<=speciesLimit;++species)
    if(PokedexLogic::isLegendaryOrMythical(species))++legendaryCount;
  StoredEgg egg;
  egg.speciesId=availableCount?availableStarters[rng(seed)%availableCount]:
      0U;
  if(!egg.speciesId&&legendaryCount){
    uint8_t pick=static_cast<uint8_t>(rng(seed)%legendaryCount);
    for(uint16_t species=1;species<=speciesLimit;++species){
      if(!PokedexLogic::isLegendaryOrMythical(species))continue;
      if(!pick){egg.speciesId=species;break;}
      --pick;
    }
  }
  // Every supported generation has at least one legendary. Keep a defensive
  // Kanto fallback for malformed progression values without expanding the
  // starter pool beyond the established Gen-I--III rewards.
  if(!egg.speciesId)egg.speciesId=144U;
  egg.shinyRoll=rng(seed);egg.region=generation;egg.rarity=EggRarity::Special;
  return egg;
}
void beginIncubation(EggState& state,const StoredEgg& stored){
  const uint8_t gifts=state.giftResolvedBits;
  state=EggState{};state.giftResolvedBits=gifts;
  state.active=true;state.speciesId=stored.speciesId;state.shinyRoll=stored.shinyRoll;
  state.region=stored.region;state.rarity=stored.rarity;
  state.remainingSeconds=stored.rarity==EggRarity::Common?EggSystem::kCommonSeconds:
      stored.rarity==EggRarity::Uncommon?EggSystem::kUncommonSeconds:EggSystem::kRareSeconds;
}
}
bool EggSystem::daycareUnlocked(const GymProgress& p,uint8_t region){return region>=1&&region<=p.unlockedGeneration&&badges(p,region)>=3;}
bool EggSystem::offerRegionalGift(EggState& e,const GymProgress& p,uint32_t seed){if(e.active||e.offerPending)return false;for(uint8_t region=1;region<=p.unlockedGeneration;++region){const uint8_t bit=1U<<(region-1U);if(daycareUnlocked(p,region)&&!(e.giftResolvedBits&bit)){makeOffer(e,region,seed);return true;}}return false;}
bool EggSystem::offerAfterTrainer(EggState& e,const GymProgress& p,uint32_t seed){if(e.active||e.offerPending||!daycareUnlocked(p,p.unlockedGeneration))return false;if((rng(seed)&63U)!=0)return false;makeOffer(e,p.unlockedGeneration,seed);return true;}
bool EggSystem::grantBattleTowerSpecial(EggState& e,const PokedexState& pokedex,
                                        uint8_t unlockedGeneration,uint32_t seed){
  if(e.active||e.offerPending)return false;
  beginIncubation(e,makeSpecialEgg(pokedex,unlockedGeneration,seed));
  return true;
}
bool EggSystem::storeBattleTowerSpecial(EggInventory& inventory,
                                        const PokedexState& pokedex,
                                        uint8_t unlockedGeneration,uint32_t seed,
                                        uint16_t* speciesId){
  if(inventory.count>=EggInventory::kCapacity)return false;
  const StoredEgg egg=makeSpecialEgg(pokedex,unlockedGeneration,seed);
  inventory.slots[inventory.count++]=egg;
  if(speciesId)*speciesId=egg.speciesId;
  return true;
}
bool EggSystem::incubateStored(EggInventory& inventory,uint8_t index,
                               EggState& state){
  if(state.active||state.offerPending||index>=inventory.count)return false;
  beginIncubation(state,inventory.slots[index]);
  for(uint8_t slot=index;slot+1U<inventory.count;++slot)
    inventory.slots[slot]=inventory.slots[slot+1U];
  --inventory.count;inventory.slots[inventory.count]=StoredEgg{};
  return true;
}
bool EggSystem::accept(EggState& e){if(!e.offerPending)return false;e.offerPending=false;e.active=true;e.remainingSeconds=e.rarity==EggRarity::Common?kCommonSeconds:e.rarity==EggRarity::Uncommon?kUncommonSeconds:kRareSeconds;e.dailyBonusUsedSeconds=0;e.bonusWindowSeconds=86400;e.giftResolvedBits|=1U<<(e.region-1U);return true;}
void EggSystem::reject(EggState& e){if(!e.offerPending)return;e.giftResolvedBits|=1U<<(e.region-1U);e.offerPending=false;e.speciesId=0;}
void EggSystem::advance(EggState& e,uint32_t seconds,
                        const PokemonCollection* collection){
  bool warmParty=false;
  if(collection)for(uint8_t slot=0;slot<kPartyCapacity;++slot){
    const OwnedPokemon* pokemon=CollectionLogic::find(*collection,
                                                       collection->party[slot]);
    const AbilityData* ability=pokemon?findAbility(pokemon->abilityId):nullptr;
    if(ability&&(std::strcmp(ability->name,"FLAME BODY")==0||
                 std::strcmp(ability->name,"MAGMA ARMOR")==0)){
      warmParty=true;break;
    }
  }
  const uint32_t eggSeconds=warmParty&&seconds<=UINT32_MAX/2U?seconds*2U:
      (warmParty?UINT32_MAX:seconds);
  if(e.active&&e.remainingSeconds)e.remainingSeconds=eggSeconds>=e.remainingSeconds?
      0:e.remainingSeconds-eggSeconds;
  while(seconds>=e.bonusWindowSeconds){seconds-=e.bonusWindowSeconds;
    e.bonusWindowSeconds=86400;e.dailyBonusUsedSeconds=0;}
  e.bonusWindowSeconds-=seconds;
}
uint32_t EggSystem::addInteractionBonus(EggState& e){return bonus(e,10U*60U);}
uint32_t EggSystem::addBattleBonus(EggState& e){return bonus(e,15U*60U);}
uint8_t EggSystem::hatchLevel(const GymProgress& progress){
  uint8_t level=5;
  for(uint8_t index=0;index<static_cast<uint8_t>(GymId::Count);++index){
    const GymId gym=static_cast<GymId>(index);
    if(GymSystem::hasBadge(progress,gym))
      level=std::max<uint8_t>(level,GymSystem::requiredLevel(gym));
  }
  return std::min<uint8_t>(89U,level);
}
bool EggSystem::hatch(EggState& e,PokemonCollection& c,const GymProgress& progress,
                      uint32_t* uid){
  if(!ready(e))return false;
  OwnedPokemon p=CollectionLogic::createPokemon(
      0,e.speciesId,hatchLevel(progress),CollectionLogic::isShinyRoll(e.shinyRoll),
      e.shinyRoll^0xE661B1A5U);
  if(!CollectionLogic::add(c,p,uid))return false;
  const uint8_t gifts=e.giftResolvedBits;e=EggState{};e.giftResolvedBits=gifts;
  return true;
}
const char* EggSystem::rarityName(EggRarity r){return r==EggRarity::Common?"COMMON":r==EggRarity::Uncommon?"UNCOMMON":r==EggRarity::Special?"SPECIAL":"RARE";}
const char* EggSystem::stageText(const EggState& e){if(ready(e))return "IT'S ABOUT TO HATCH!";const uint32_t total=e.rarity==EggRarity::Common?kCommonSeconds:e.rarity==EggRarity::Uncommon?kUncommonSeconds:kRareSeconds;const uint32_t pct=total?100U*(total-e.remainingSeconds)/total:0;if(pct<20)return "IT WILL TAKE A LONG TIME.";if(pct<45)return "WHAT WILL HATCH FROM THIS?";if(pct<70)return "IT MOVES OCCASIONALLY.";if(pct<90)return "SOUNDS COME FROM INSIDE!";return "IT'S ABOUT TO HATCH!";}
