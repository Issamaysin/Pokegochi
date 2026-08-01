#include "game/Economy.h"

namespace {
constexpr MartOffer catalog[]={{MartItem::GreatBall,600,0},{MartItem::UltraBall,1200,0},{MartItem::SuperPotion,700,0},{MartItem::HyperPotion,1200,0},{MartItem::FullHeal,600,0},{MartItem::Antidote,100,0},{MartItem::ParalyzeHeal,200,0},{MartItem::Awakening,250,0},{MartItem::BurnHeal,250,0},{MartItem::IceHeal,250,0},{MartItem::XAttack,500,0},{MartItem::XDefense,550,0},{MartItem::XSpeed,350,0},{MartItem::XAccuracy,950,0},{MartItem::DireHit,650,0}};
uint32_t next(uint32_t& x){x^=x<<13U;x^=x>>17U;x^=x<<5U;return x;}
}
void Economy::advance(MartState& mart,uint32_t seconds){mart.elapsedSeconds+=seconds;if(mart.elapsedSeconds>=86400U)rotate(mart);}
void Economy::rotate(MartState& mart){mart.elapsedSeconds%=86400U;++mart.day;mart.offers[0]={MartItem::PokeBall,200,12};mart.offers[1]={MartItem::Potion,300,8};for(uint8_t i=2;i<kMartOfferCount;++i){mart.offers[i]=catalog[next(mart.rng)%(sizeof(catalog)/sizeof(catalog[0]))];mart.offers[i].remaining=static_cast<uint8_t>(2U+next(mart.rng)%7U);}}
bool Economy::buy(MartState& mart,uint8_t offer,uint32_t& money,Inventory& inventory){if(offer>=kMartOfferCount||!mart.offers[offer].remaining||money<mart.offers[offer].price)return false;auto item=mart.offers[offer].item;if(item<=MartItem::UltraBall)++inventory.balls[static_cast<uint8_t>(item)];else ++inventory.medicine[static_cast<uint8_t>(item)-3U];money-=mart.offers[offer].price;--mart.offers[offer].remaining;return true;}
const char* Economy::name(MartItem item){static const char* names[]={"POKE BALL","GREAT BALL","ULTRA BALL","POTION","SUPER POTION","HYPER POTION","FULL HEAL","ANTIDOTE","PARALYZE HEAL","AWAKENING","BURN HEAL","ICE HEAL","X ATTACK","X DEFENSE","X SPEED","X ACCURACY","DIRE HIT"};return names[static_cast<uint8_t>(item)];}
