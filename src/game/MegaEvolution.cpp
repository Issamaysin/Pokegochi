#include "game/MegaEvolution.h"
#include "game/Collection.h"

namespace {
enum CustomAbility : uint8_t {
  ToughClaws=128, MegaLauncher, Adaptability, NoGuard, ParentalBond,
  Aerilate, MoldBreaker, Steadfast, SandForce, Technician, SkillLink,
  SolarPower, Pixilate, MagicBounce, Filter, StrongJaw, SheerForce,
  Prankster, Refrigerate, PrimordialSea, DesolateLand, DeltaStream,
  Multiscale, InnardsOut, ElectricSurge, Stalwart, Sharpness, Dragonize,
  MegaSol,
};

constexpr AbilityData kCustomAbilities[] = {
  {ToughClaws,"TOUGH CLAWS","BOOSTS CONTACT MOVES BY 30%."},
  {MegaLauncher,"MEGA LAUNCHER","BOOSTS PULSE AND AURA MOVES BY 50%."},
  {Adaptability,"ADAPTABILITY","RAISES THE SAME-TYPE BONUS TO 2X."},
  {NoGuard,"NO GUARD","BOTH BATTLERS' MOVES ALWAYS HIT."},
  {ParentalBond,"PARENTAL BOND","DAMAGING MOVES STRIKE A SECOND TIME."},
  {Aerilate,"AERILATE","MAKES NORMAL MOVES FLYING AND BOOSTS THEM."},
  {MoldBreaker,"MOLD BREAKER","IGNORES ABILITIES THAT BLOCK A MOVE."},
  {Steadfast,"STEADFAST","RAISES SPEED AFTER FLINCHING."},
  {SandForce,"SAND FORCE","BOOSTS ROCK, GROUND AND STEEL IN SAND."},
  {Technician,"TECHNICIAN","BOOSTS MOVES WITH 60 POWER OR LESS."},
  {SkillLink,"SKILL LINK","MULTI-HIT MOVES ALWAYS STRIKE FIVE TIMES."},
  {SolarPower,"SOLAR POWER","BOOSTS SP. ATK IN SUN BUT COSTS HP."},
  {Pixilate,"PIXILATE","MAKES NORMAL MOVES FAIRY AND BOOSTS THEM."},
  {MagicBounce,"MAGIC BOUNCE","REFLECTS STATUS MOVES BACK."},
  {Filter,"FILTER","REDUCES SUPER-EFFECTIVE DAMAGE BY 25%."},
  {StrongJaw,"STRONG JAW","BOOSTS BITING MOVES BY 50%."},
  {SheerForce,"SHEER FORCE","BOOSTS MOVES BUT REMOVES ADDED EFFECTS."},
  {Prankster,"PRANKSTER","GIVES STATUS MOVES PRIORITY."},
  {Refrigerate,"REFRIGERATE","MAKES NORMAL MOVES ICE AND BOOSTS THEM."},
  {PrimordialSea,"PRIMORDIAL SEA","HEAVY RAIN MAKES FIRE MOVES FAIL."},
  {DesolateLand,"DESOLATE LAND","HARSH SUN MAKES WATER MOVES FAIL."},
  {DeltaStream,"DELTA STREAM","STRONG WINDS PROTECT FLYING TYPES."},
  {Multiscale,"MULTISCALE","HALVES DAMAGE WHILE HP IS FULL."},
  {InnardsOut,"INNARDS OUT","HURTS THE FOE WHEN THE HOLDER FAINTS."},
  {ElectricSurge,"ELECTRIC SURGE","CHARGES THE FIELD WITH ELECTRICITY."},
  {Stalwart,"STALWART","IGNORES MOVE REDIRECTION."},
  {Sharpness,"SHARPNESS","BOOSTS SLICING MOVES BY 50%."},
  {Dragonize,"DRAGONIZE","MAKES NORMAL MOVES DRAGON AND BOOSTS THEM."},
  {MegaSol,"MEGA SOL","THE HOLDER BATTLES AS IF IN SUNLIGHT."},
};

// Official Mega Evolutions whose base species exists in National Dex #001-386,
// plus the two official Primal Reversions. Stats/types/abilities are the
// canonical forms; no Radical Red fan form or ordinary Gen-IV+ species enters
// this table.
constexpr MegaFormData kForms[] = {
 {26,MegaVariant::MegaX,60,135,95,90,95,110,PokemonType::Electric,PokemonType::Electric,ElectricSurge,"mega_x","MEGA X"},
 {26,MegaVariant::MegaY,60,100,55,160,80,130,PokemonType::Electric,PokemonType::Electric,NoGuard,"mega_y","MEGA Y"},
 {36,MegaVariant::Mega,95,80,93,135,110,70,PokemonType::Fairy,PokemonType::Flying,MagicBounce,"mega","MEGA"},
 {71,MegaVariant::Mega,80,125,85,135,95,70,PokemonType::Grass,PokemonType::Poison,InnardsOut,"mega","MEGA"},
 {121,MegaVariant::Mega,60,100,105,130,105,120,PokemonType::Water,PokemonType::Psychic,37,"mega","MEGA"},
 {149,MegaVariant::Mega,91,124,115,145,125,100,PokemonType::Dragon,PokemonType::Flying,Multiscale,"mega","MEGA"},
 {154,MegaVariant::Mega,80,92,115,143,115,80,PokemonType::Grass,PokemonType::Fairy,MegaSol,"mega","MEGA"},
 {160,MegaVariant::Mega,85,160,125,89,93,78,PokemonType::Water,PokemonType::Dragon,Dragonize,"mega","MEGA"},
 {227,MegaVariant::Mega,65,140,110,40,100,110,PokemonType::Steel,PokemonType::Flying,Stalwart,"mega","MEGA"},
 {358,MegaVariant::Mega,75,50,110,135,120,65,PokemonType::Psychic,PokemonType::Steel,26,"mega","MEGA"},
 {3,MegaVariant::Mega,80,100,123,122,120,80,PokemonType::Grass,PokemonType::Poison,47,"mega","MEGA"},
 {6,MegaVariant::MegaX,78,130,111,130,85,100,PokemonType::Fire,PokemonType::Dragon,ToughClaws,"mega_x","MEGA X"},
 {6,MegaVariant::MegaY,78,104,78,159,115,100,PokemonType::Fire,PokemonType::Flying,70,"mega_y","MEGA Y"},
 {9,MegaVariant::Mega,79,103,120,135,115,78,PokemonType::Water,PokemonType::Water,MegaLauncher,"mega","MEGA"},
 {15,MegaVariant::Mega,65,150,40,15,80,145,PokemonType::Bug,PokemonType::Poison,Adaptability,"mega","MEGA"},
 {18,MegaVariant::Mega,83,80,80,135,80,121,PokemonType::Normal,PokemonType::Flying,NoGuard,"mega","MEGA"},
 {65,MegaVariant::Mega,55,50,65,175,105,150,PokemonType::Psychic,PokemonType::Psychic,36,"mega","MEGA"},
 {80,MegaVariant::Mega,95,75,180,130,80,30,PokemonType::Water,PokemonType::Psychic,75,"mega","MEGA"},
 {94,MegaVariant::Mega,60,65,80,170,95,130,PokemonType::Ghost,PokemonType::Poison,23,"mega","MEGA"},
 {115,MegaVariant::Mega,105,125,100,60,100,100,PokemonType::Normal,PokemonType::Normal,ParentalBond,"mega","MEGA"},
 {127,MegaVariant::Mega,65,155,120,65,90,105,PokemonType::Bug,PokemonType::Flying,Aerilate,"mega","MEGA"},
 {130,MegaVariant::Mega,95,155,109,70,130,81,PokemonType::Water,PokemonType::Dark,MoldBreaker,"mega","MEGA"},
 {142,MegaVariant::Mega,80,135,85,70,95,150,PokemonType::Rock,PokemonType::Flying,ToughClaws,"mega","MEGA"},
 {150,MegaVariant::MegaX,106,190,100,154,100,130,PokemonType::Psychic,PokemonType::Fighting,Steadfast,"mega_x","MEGA X"},
 {150,MegaVariant::MegaY,106,150,70,194,120,140,PokemonType::Psychic,PokemonType::Psychic,15,"mega_y","MEGA Y"},
 {181,MegaVariant::Mega,90,95,105,165,110,45,PokemonType::Electric,PokemonType::Dragon,MoldBreaker,"mega","MEGA"},
 {208,MegaVariant::Mega,75,125,230,55,95,30,PokemonType::Steel,PokemonType::Ground,SandForce,"mega","MEGA"},
 {212,MegaVariant::Mega,70,150,140,65,100,75,PokemonType::Bug,PokemonType::Steel,Technician,"mega","MEGA"},
 {214,MegaVariant::Mega,80,185,115,40,105,75,PokemonType::Bug,PokemonType::Fighting,SkillLink,"mega","MEGA"},
 {229,MegaVariant::Mega,75,90,90,140,90,115,PokemonType::Dark,PokemonType::Fire,SolarPower,"mega","MEGA"},
 {248,MegaVariant::Mega,100,164,150,95,120,71,PokemonType::Rock,PokemonType::Dark,45,"mega","MEGA"},
 {254,MegaVariant::Mega,70,110,75,145,85,145,PokemonType::Grass,PokemonType::Dragon,31,"mega","MEGA"},
 {257,MegaVariant::Mega,80,160,80,130,80,100,PokemonType::Fire,PokemonType::Fighting,3,"mega","MEGA"},
 {260,MegaVariant::Mega,100,150,110,95,110,70,PokemonType::Water,PokemonType::Ground,33,"mega","MEGA"},
 {282,MegaVariant::Mega,68,85,65,165,135,100,PokemonType::Psychic,PokemonType::Fairy,Pixilate,"mega","MEGA"},
 {302,MegaVariant::Mega,50,85,125,85,115,20,PokemonType::Dark,PokemonType::Ghost,MagicBounce,"mega","MEGA"},
 {303,MegaVariant::Mega,50,105,125,55,95,50,PokemonType::Steel,PokemonType::Fairy,37,"mega","MEGA"},
 {306,MegaVariant::Mega,70,140,230,60,80,50,PokemonType::Steel,PokemonType::Steel,Filter,"mega","MEGA"},
 {308,MegaVariant::Mega,60,100,85,80,85,100,PokemonType::Fighting,PokemonType::Psychic,74,"mega","MEGA"},
 {310,MegaVariant::Mega,70,75,80,135,80,135,PokemonType::Electric,PokemonType::Electric,22,"mega","MEGA"},
 {319,MegaVariant::Mega,70,140,70,110,65,105,PokemonType::Water,PokemonType::Dark,StrongJaw,"mega","MEGA"},
 {323,MegaVariant::Mega,70,120,100,145,105,20,PokemonType::Fire,PokemonType::Ground,SheerForce,"mega","MEGA"},
 {334,MegaVariant::Mega,75,110,110,110,105,80,PokemonType::Dragon,PokemonType::Fairy,Pixilate,"mega","MEGA"},
 {354,MegaVariant::Mega,64,165,75,93,83,75,PokemonType::Ghost,PokemonType::Ghost,Prankster,"mega","MEGA"},
 {359,MegaVariant::Mega,65,150,60,115,60,115,PokemonType::Dark,PokemonType::Dark,MagicBounce,"mega","MEGA"},
 {359,MegaVariant::MegaZ,65,154,60,75,60,151,PokemonType::Dark,PokemonType::Ghost,Sharpness,"mega_z","MEGA Z"},
 {362,MegaVariant::Mega,80,120,80,120,80,100,PokemonType::Ice,PokemonType::Ice,Refrigerate,"mega","MEGA"},
 {373,MegaVariant::Mega,95,145,130,120,90,120,PokemonType::Dragon,PokemonType::Flying,Aerilate,"mega","MEGA"},
 {376,MegaVariant::Mega,80,145,150,105,110,110,PokemonType::Steel,PokemonType::Psychic,ToughClaws,"mega","MEGA"},
 {380,MegaVariant::Mega,80,100,120,140,150,110,PokemonType::Dragon,PokemonType::Psychic,26,"mega","MEGA"},
 {381,MegaVariant::Mega,80,130,100,160,120,110,PokemonType::Dragon,PokemonType::Psychic,26,"mega","MEGA"},
 {382,MegaVariant::Primal,100,150,90,180,160,90,PokemonType::Water,PokemonType::Water,PrimordialSea,"primal","PRIMAL"},
 {383,MegaVariant::Primal,100,180,160,150,90,90,PokemonType::Ground,PokemonType::Fire,DesolateLand,"primal","PRIMAL"},
 {384,MegaVariant::Mega,105,180,100,180,100,115,PokemonType::Dragon,PokemonType::Flying,DeltaStream,"mega","MEGA"},
};
}

namespace MegaEvolution {
namespace {
constexpr uint8_t kVariantChoiceShift=11U;
constexpr uint32_t kVariantChoiceMask=0x07UL<<kVariantChoiceShift;
}
const MegaFormData* formData(uint16_t speciesId,MegaVariant variant){
  for(const auto& form:kForms)if(form.speciesId==speciesId&&form.variant==variant)return &form;
  return nullptr;
}
bool canTransform(uint16_t speciesId){for(const auto& form:kForms)if(form.speciesId==speciesId)return true;return false;}
uint8_t variantChoiceCount(uint16_t speciesId){
  uint8_t count=0;for(const auto& form:kForms)if(form.speciesId==speciesId)++count;return count;
}
MegaVariant variantChoice(uint16_t speciesId,uint8_t wanted){
  uint8_t index=0;for(const auto& form:kForms)if(form.speciesId==speciesId){
    if(index++==wanted)return form.variant;
  }return MegaVariant::None;
}
bool setVariantChoice(OwnedPokemon& pokemon,MegaVariant variant){
  if(!formData(pokemon.speciesId,variant))return false;
  pokemon.legacyCareCounterReserved=(pokemon.legacyCareCounterReserved&~kVariantChoiceMask)|
      (static_cast<uint32_t>(variant)<<kVariantChoiceShift);
  return true;
}
MegaVariant variantFor(const OwnedPokemon& pokemon){
  if(pokemon.heldItem!=HeldItem::MegaStone)return MegaVariant::None;
  const MegaVariant selected=static_cast<MegaVariant>(
      (pokemon.legacyCareCounterReserved&kVariantChoiceMask)>>kVariantChoiceShift);
  if(selected!=MegaVariant::None&&formData(pokemon.speciesId,selected))return selected;
  // Saves made before explicit selection fall back to the first authored form
  // until the player next equips the stone and chooses a form.
  return variantChoice(pokemon.speciesId,0);
}
uint8_t baseHp(const OwnedPokemon& pokemon,uint8_t fallback){const auto* f=formData(pokemon.speciesId,variantFor(pokemon));return f?f->baseHp:fallback;}
uint8_t baseStat(const OwnedPokemon& pokemon,uint8_t statIndex,uint8_t fallback){const auto* f=formData(pokemon.speciesId,variantFor(pokemon));if(!f)return fallback;const uint8_t s[]={f->baseAttack,f->baseDefense,f->baseSpeed,f->baseSpAttack,f->baseSpDefense};return statIndex<5?s[statIndex]:fallback;}
PokemonType type1(const OwnedPokemon& pokemon,PokemonType fallback){const auto* f=formData(pokemon.speciesId,variantFor(pokemon));return f?f->type1:fallback;}
PokemonType type2(const OwnedPokemon& pokemon,PokemonType fallback){const auto* f=formData(pokemon.speciesId,variantFor(pokemon));return f?f->type2:fallback;}
uint8_t abilityId(const OwnedPokemon& pokemon,uint8_t fallback){const auto* f=formData(pokemon.speciesId,variantFor(pokemon));return f?f->abilityId:fallback;}
const AbilityData* customAbility(uint8_t id){for(const auto& a:kCustomAbilities)if(a.id==id)return &a;return nullptr;}
const char* variantName(const OwnedPokemon& pokemon){const auto* f=formData(pokemon.speciesId,variantFor(pokemon));return f?f->displayName:"NORMAL";}
uint8_t officialFormCount(){return static_cast<uint8_t>(sizeof(kForms)/sizeof(kForms[0]));}
}
