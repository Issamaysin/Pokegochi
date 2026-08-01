#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <cstdio>
#include <cstring>
#include "config/BoardConfig.h"
#include "drivers/Backlight.h"
#include "drivers/AssetRenderer.h"
#include "drivers/ScreenButton.h"
#include "drivers/TouchDriver.h"
#include "game/BattleEngine.h"
#include "game/Collection.h"
#include "game/Pokedex.h"
#include "game/TrainerData.h"
#include "game/Economy.h"
#include "services/PersistentSave.h"

namespace {
enum class Screen : uint8_t { Diagnostic, Starter, Home, ChooseBattler, TrainerIntro, Battle, MoveSelect, MoveLearn, BattleParty, BattleSummary, Bag, Inventory, Box, Pokedex, PokedexDetail, Mart, Recovery };
TFT_eSPI display;
SPIClass sdSpi(VSPI);
Backlight backlight;
ScreenButton screenButton;
TouchDriver touch;
PersistentSave saves;
GameSave gameSave;
Screen screen = Screen::Diagnostic;
bool sdReady = false, saveReady = false, wasTouched = false, saveDirty = false;
uint32_t lastSecondMs = 0, lastSaveMs = 0, selectedUid = 0;
uint32_t lastAnimationMs = 0;
uint8_t iconFrame = 0;
uint8_t boxPage = 0;
uint8_t dexPage = 0;
uint8_t bagPage = 0;
uint16_t selectedDexSpecies = 1;
int8_t battlePartySelection = -1;
char message[64] = "";
BattleKind pendingBattleKind = BattleKind::None;

bool inside(const TouchPoint& p, int16_t x, int16_t y, int16_t w, int16_t h) {
  return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h;
}

void button(int16_t x, int16_t y, int16_t w, int16_t h, const char* label, uint16_t color) {
  display.fillRoundRect(x, y, w, h, 6, color); display.drawRoundRect(x, y, w, h, 6, TFT_WHITE);
  display.setTextColor(TFT_WHITE, color); display.setTextDatum(MC_DATUM);
  display.drawString(label, x + w / 2, y + h / 2, 2); display.setTextDatum(TL_DATUM);
}

void bar(int16_t x, int16_t y, const char* label, uint16_t value, uint16_t maximum, uint16_t color) {
  display.setTextColor(TFT_WHITE, TFT_BLACK); display.drawString(label, x, y, 1);
  display.drawRect(x + 42, y, 94, 9, TFT_DARKGREY);
  const uint16_t width = maximum ? static_cast<uint16_t>(88U * value / maximum) : 0;
  display.fillRect(x + 45, y + 2, width, 5, color);
}

uint16_t speciesColor(uint16_t id) {
  if (id == 1 || id == 10) return TFT_GREEN;
  if (id == 4) return TFT_ORANGE;
  if (id == 7) return TFT_CYAN;
  if (id == 13) return TFT_YELLOW;
  if (id == 16) return TFT_LIGHTGREY;
  return TFT_MAGENTA;
}

void creature(int16_t x, int16_t y, uint16_t speciesId, bool wild = false) {
  if (sdReady && speciesId >= 1 && speciesId <= 151) {
    char path[64];
    if (wild) std::snprintf(path, sizeof(path), "/pokegochi/assets/pokemon/front/%03u.pkg", speciesId);
    else std::snprintf(path, sizeof(path), "/pokegochi/assets/pokemon/icons/%03u_%u.pkg", speciesId, iconFrame);
    const int16_t size = wild ? 64 : 32;
    if (AssetRenderer::draw(display, path, x - size / 2, y - size / 2)) return;
  }
  const uint16_t color = speciesColor(speciesId);
  display.fillCircle(x, y, wild ? 24 : 34, color);
  display.fillTriangle(x - 24, y - 18, x - 15, y - 37, x - 8, y - 17, color);
  display.fillTriangle(x + 8, y - 17, x + 17, y - 37, x + 24, y - 18, color);
  display.fillCircle(x - 10, y - 4, 3, TFT_BLACK); display.fillCircle(x + 10, y - 4, 3, TFT_BLACK);
}

void battleBackSprite(int16_t x, int16_t y, uint16_t speciesId) {
  if (sdReady && speciesId >= 1 && speciesId <= 151) {
    char path[64]; std::snprintf(path, sizeof(path), "/pokegochi/assets/pokemon/back/%03u.pkg", speciesId);
    if (AssetRenderer::draw(display, path, x - 32, y - 32)) return;
  }
  creature(x, y, speciesId, true);
}

void playMoveAnimation(MoveId move) {
  const FullMoveData* data=findFullMove(move);const char* effect=data?data->effect:"HIT";
  const char* asset = "hit"; uint8_t frames = 1;
  if (move == MoveId::VineWhip) { asset = "leaf"; frames = 9; }
  else if (move == MoveId::Scratch) { asset = "claw_slash"; frames = 1; }
  else if (move == MoveId::Ember) { asset = "fire"; frames = 8; }
  else if (move == MoveId::WaterGun) { asset = "bubble"; frames = 3; }
  else if (move == MoveId::Gust) { asset = "gust"; frames = 2; }
  else if (move == MoveId::StringShot) { asset = "web_thread"; frames = 1; }
  else if (move == MoveId::PoisonSting) { asset = "poison_bubble"; frames = 3; }
  else if(std::strstr(effect,"EXPLOSION")){asset="explosion";frames=1;}
  else if(std::strstr(effect,"PROTECT")){asset="protect";frames=1;}
  else if(std::strstr(effect,"SUBSTITUTE")){asset="substitute";frames=1;}
  else if(std::strstr(effect,"SLEEP")){asset="letter_z";frames=1;}
  else if(std::strstr(effect,"CONFUSE")){asset="thought_bubble";frames=1;}
  else if(std::strstr(effect,"POISON")){asset="toxic_bubble";frames=1;}
  else if(std::strstr(effect,"PAIN_SPLIT")){asset="pain_split";frames=1;}
  else if(data)switch(data->type){
    case PokemonType::Fire:asset="fire";frames=8;break;case PokemonType::Water:asset="bubble";frames=3;break;
    case PokemonType::Electric:asset="lightning";break;case PokemonType::Grass:asset="leaf";frames=9;break;
    case PokemonType::Ice:asset="ice_crystals_0";break;case PokemonType::Psychic:asset="bent_spoon";break;
    case PokemonType::Ghost:asset="ghostly_spirit";break;case PokemonType::Ground:asset="flying_dirt";break;
    case PokemonType::Rock:asset="rocks";break;case PokemonType::Bug:asset="web";break;
    case PokemonType::Flying:asset="gust";frames=2;break;case PokemonType::Fighting:asset="punch_impact";break;
    case PokemonType::Dragon:asset="breath";break;case PokemonType::Steel:asset="torn_metal";break;
    case PokemonType::Dark:asset="black_ball";break;case PokemonType::Poison:asset="poison_bubble";frames=3;break;
    default:asset=data->power?"hit":"sparkle_1";break;}
  for (uint8_t frame = 0; frame < frames; ++frame) {
    char path[112];
    if (frames == 1) std::snprintf(path, sizeof(path), "/pokegochi/assets/firered/battle_anims/sprites/%s_00.pkg", asset);
    else std::snprintf(path, sizeof(path), "/pokegochi/assets/firered/battle_anims/sprites/%s_%02u.pkg", asset, frame);
    AssetRenderer::draw(display, path, 225, 47); delay(65);
  }
}

const char* ballAsset(PokeBallType ball){switch(ball){case PokeBallType::GreatBall:return "great_ball";case PokeBallType::UltraBall:return "ultra_ball";case PokeBallType::MasterBall:return "master_ball";default:return "poke_ball";}}
void playBallAnimation(PokeBallType ball) {
  for (uint8_t step = 0; step < 8; ++step) {
    const int16_t x = 92 + step * 19, y = 108 - static_cast<int16_t>(step < 4 ? step * 12 : (7 - step) * 12);
    char path[72];std::snprintf(path,sizeof(path),"/pokegochi/assets/firered/items/%s.pkg",ballAsset(ball));AssetRenderer::draw(display,path,x,y); delay(55);
  }
}
void playBallResult(PokeBallType ball,bool caught){char path[72];std::snprintf(path,sizeof(path),"/pokegochi/assets/firered/items/%s.pkg",ballAsset(ball));
  for(uint8_t shake=0;shake<3;++shake){for(int8_t dx=-4;dx<=4;dx+=2){display.fillRect(238,86,34,30,TFT_WHITE);AssetRenderer::draw(display,path,243+dx,89);delay(45);}}
  if(caught){display.fillCircle(256,101,20,TFT_YELLOW);AssetRenderer::draw(display,path,244,89);}else{display.fillCircle(256,101,25,TFT_WHITE);delay(90);}}

void playEvolutionAnimation(uint16_t fromSpecies, uint16_t toSpecies) {
  display.fillScreen(TFT_NAVY);
  AssetRenderer::draw(display, "/pokegochi/assets/firered/evolution_scene/bg.pkg", 0, 0);
  for (uint8_t phase = 0; phase < 10; ++phase) {
    display.fillRect(112, 66, 96, 96, TFT_NAVY);
    const uint16_t species = phase & 1U ? toSpecies : fromSpecies;
    char path[72]; std::snprintf(path, sizeof(path), "/pokegochi/assets/pokemon/front/%03u.pkg", species);
    AssetRenderer::draw(display, path, 128, 82); delay(static_cast<uint16_t>(170 - phase * 10));
  }
  AssetRenderer::draw(display, "/pokegochi/assets/firered/evolution_scene/sparkle.pkg", 144, 92);
  const SpeciesData* evolved = findSpecies(toSpecies);
  display.setTextColor(TFT_WHITE, TFT_NAVY); display.drawString("CONGRATULATIONS!", 82, 178, 2);
  display.drawString(evolved ? evolved->name : "EVOLVED!", 118, 204, 2); delay(900);
}

bool testSd() {
  sdSpi.begin(board::kSdSckPin, board::kSdMisoPin, board::kSdMosiPin, board::kSdCsPin);
  if (!SD.begin(board::kSdCsPin, sdSpi, 10000000) || SD.cardType() == CARD_NONE) return false;
  if (!SD.exists("/pokegochi")) SD.mkdir("/pokegochi");
  File file = SD.open("/pokegochi/diagnostic.tmp", FILE_WRITE); if (!file) return false;
  const size_t written = file.println("Pokegochi SD write test"); file.close();
  if (!written) return false; SD.remove("/pokegochi/diagnostic.tmp"); return true;
}

void statusLine(const char* label, bool ok, int16_t y) {
  display.setTextColor(ok ? TFT_GREEN : TFT_RED, TFT_BLACK);
  display.drawString(label, 12, y, 2); display.drawString(ok ? "OK" : "FAIL", 245, y, 2);
}

void drawDiagnostic() {
  display.fillScreen(TFT_BLACK); display.setTextColor(TFT_YELLOW); display.drawString("POKEGOCHI", 12, 10, 4);
  display.setTextColor(TFT_WHITE); display.drawString("Hardware diagnostic", 12, 44, 2);
  statusLine("ILI9341 display", true, 78); statusLine("XPT2046 touch", true, 104);
  statusLine("microSD read/write", sdReady, 130); statusLine("permanent save", saveReady, 156);
  button(105, 190, 110, 38, "START", TFT_DARKGREEN);
}

void drawStarter() {
  display.fillScreen(TFT_BLACK); display.setTextDatum(MC_DATUM);
  display.setTextColor(TFT_YELLOW); display.drawString("CHOOSE YOUR PARTNER", 160, 22, 2);
  const uint16_t ids[3] = {1, 4, 7};
  for (uint8_t i = 0; i < 3; ++i) {
    const SpeciesData* data = findSpecies(ids[i]); const int16_t x = 54 + i * 106;
    creature(x, 91, ids[i]); display.setTextColor(TFT_WHITE); display.drawString(data->name, x, 137, 2);
    button(10 + i * 105, 165, 90, 48, "CHOOSE", speciesColor(ids[i]));
  }
  display.setTextColor(TFT_ORANGE); display.drawString("THIS CHOICE IS PERMANENT", 160, 229, 1);
  display.setTextDatum(TL_DATUM);
}

OwnedPokemon* currentPet() {
  OwnedPokemon* pet = CollectionLogic::active(gameSave.collection, gameSave.activePetSlot);
  if (!pet) { gameSave.activePetSlot = 0; pet = CollectionLogic::active(gameSave.collection, 0); }
  return pet;
}

void drawParty() {
  for (uint8_t i = 0; i < kPartyCapacity; ++i) {
    OwnedPokemon* pet = CollectionLogic::active(gameSave.collection, i);
    const int16_t x = 190 + i * 25;
    display.fillCircle(x, 14, 9, pet ? speciesColor(pet->speciesId) : TFT_DARKGREY);
    if (i == gameSave.activePetSlot) display.drawCircle(x, 14, 11, TFT_WHITE);
  }
  button(270, 3, 46, 22, "", TFT_NAVY);
  if(!sdReady||!AssetRenderer::draw(display,"/pokegochi/assets/firered/ui/box_icon.pkg",283,4)){display.setTextColor(TFT_WHITE,TFT_NAVY);display.drawString("BOX",278,9,1);}
}

void drawHome() {
  OwnedPokemon* pet = currentPet(); if (!pet) { screen = Screen::Starter; drawStarter(); return; }
  const SpeciesData* data = findSpecies(pet->speciesId);
  display.fillScreen(TFT_BLACK); display.fillRect(0, 0, 320, 29, TFT_DARKGREEN);
  display.setTextColor(TFT_WHITE, TFT_DARKGREEN); display.drawString(data ? data->name : "UNKNOWN", 7, 7, 2);
  button(139, 3, 44, 22, "", TFT_RED);
  if(!sdReady||!AssetRenderer::draw(display,"/pokegochi/assets/firered/ui/pokedex_icon.pkg",151,4)){display.setTextColor(TFT_WHITE,TFT_RED);display.drawString("DEX",147,9,1);}
  button(91, 3, 44, 22, "MART", TFT_ORANGE);
  button(43, 3, 44, 22, "", TFT_NAVY);
  if (!sdReady || !AssetRenderer::draw(display, "/pokegochi/assets/firered/item_menu/bag_icon.pkg", 55, 4)) {
    display.setTextColor(TFT_WHITE, TFT_NAVY); display.drawString("BAG", 50, 9, 1);
  }
  drawParty();
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) {
    OwnedPokemon* member = CollectionLogic::active(gameSave.collection, slot);
    if (!member) continue;
    const int16_t x = 80 + slot * 80;
    creature(x, slot == gameSave.activePetSlot ? 88 : 101, member->speciesId);
    if (slot == gameSave.activePetSlot) display.drawRoundRect(x - 19, 67, 38, 43, 5, TFT_YELLOW);
  }
  char level[12]; std::snprintf(level, sizeof(level), "LV %u", pet->level);
  display.setTextColor(TFT_WHITE); display.drawString(level, 8, 35, 2);
  bar(8, 143, "FOOD", pet->fullness, 100, TFT_ORANGE);
  bar(8, 160, "HAPPY", pet->happiness, 100, TFT_MAGENTA);
  display.setTextColor(pet->status == StatusCondition::None ? TFT_GREEN : TFT_ORANGE);
  display.drawString(PetLogic::statusName(pet->status), 174, 143, 1);
  char hp[24]; std::snprintf(hp, sizeof(hp), "HP %u/%u", pet->currentHp, pet->maximumHp);
  display.setTextColor(TFT_WHITE); display.drawString(hp, 174, 160, 1);
  char charges[16]; std::snprintf(charges, sizeof(charges), "BATTLE %u/3", gameSave.encounterCharges.available);
  button(6, 187, 72, 45, "FEED", TFT_DARKGREEN); button(84, 187, 72, 45, "BATHE", TFT_BLUE);
  button(162, 187, 72, 45, "PLAY", TFT_PURPLE); button(240, 187, 74, 45, charges, TFT_RED);
}

void drawChooseBattler() {
  display.fillScreen(TFT_BLACK); display.setTextDatum(MC_DATUM);
  display.setTextColor(TFT_YELLOW);
  const char* heading = pendingBattleKind == BattleKind::Wild ? "A WILD POKEMON APPEARED!" :
                        pendingBattleKind == BattleKind::Gym ? "GYM CHALLENGE" : "TRAINER BATTLE";
  display.drawString(heading, 160, 22, 2);
  display.setTextColor(TFT_WHITE); display.drawString("CHOOSE YOUR BATTLER", 160, 48, 2);
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) {
    OwnedPokemon* pokemon = CollectionLogic::active(gameSave.collection, slot);
    const int16_t x = 54 + slot * 106;
    if (!pokemon) { display.drawRoundRect(x - 40, 70, 80, 100, 6, TFT_DARKGREY); continue; }
    const SpeciesData* data = findSpecies(pokemon->speciesId);
    creature(x, 100, pokemon->speciesId, true);
    display.setTextColor(TFT_WHITE); display.drawString(data ? data->name : "UNKNOWN", x, 135, 1);
    char stats[24]; std::snprintf(stats, sizeof(stats), "LV%u  %u/%u", pokemon->level, pokemon->currentHp, pokemon->maximumHp);
    display.drawString(stats, x, 150, 1);
    const bool ready = pokemon->currentHp > 0 && pokemon->recoverySecondsRemaining == 0;
    button(x - 43, 174, 86, 38, ready ? "CHOOSE" : "RECOVERING", ready ? TFT_DARKGREEN : TFT_DARKGREY);
  }
  if (pendingBattleKind == BattleKind::Trainer) button(120, 216, 80, 21, "BACK", TFT_RED);
  display.setTextDatum(TL_DATUM);
}

void drawTrainerIntro() {
  const TrainerProfile* profile = trainerProfile(gameSave.battle.trainerProfileId);
  const GymDefinition* gym = gameSave.battle.kind == BattleKind::Gym
      ? GymSystem::definition(static_cast<GymId>(gameSave.battle.gymId)) : nullptr;
  display.fillScreen(TFT_WHITE);
  display.fillRect(0, 0, 320, 153, TFT_SKYBLUE);
  if (profile || gym) {
    const char* asset = gym ? GymSystem::opponentAsset(gameSave.battle) : profile->frontAsset;
    char path[96]; std::snprintf(path, sizeof(path), "/pokegochi/assets/trainers/%s.pkg", asset);
    AssetRenderer::draw(display, path, 128, 38);
  }
  display.fillRoundRect(5, 151, 310, 84, 6, TFT_DARKGREY);
  display.fillRoundRect(9, 155, 302, 76, 4, TFT_NAVY);
  display.setTextColor(TFT_WHITE, TFT_NAVY);
  if (gym) {
    char title[40]; std::snprintf(title, sizeof(title), "%s %s", GymSystem::opponentClass(gameSave.battle),GymSystem::opponentName(gameSave.battle));
    display.drawString(title, 17, 162, 2); display.drawString(GymSystem::opponentLine1(gameSave.battle), 17, 187, 1); display.drawString(GymSystem::opponentLine2(gameSave.battle), 17, 203, 1);
  } else if (profile) {
    char title[40]; std::snprintf(title, sizeof(title), "%s %s", profile->trainerClass, profile->name);
    display.drawString(title, 17, 162, 2); display.drawString(profile->introLine1, 17, 187, 1); display.drawString(profile->introLine2, 17, 203, 1);
  }
  display.drawString("TOUCH TO BATTLE", 205, 220, 1);
}

void drawBattle() {
  OwnedPokemon* player = CollectionLogic::find(gameSave.collection, gameSave.battle.playerUid);
  OwnedPokemon* opponent = BattleEngine::currentOpponent(gameSave.battle);
  const SpeciesData* wild = opponent ? findSpecies(opponent->speciesId) : nullptr;
  const SpeciesData* own = player ? findSpecies(player->speciesId) : nullptr;
  display.fillScreen(TFT_BLACK);const char* terrain=gameSave.battle.kind==BattleKind::Wild?"grass":"indoor";char terrainPath[72];
  std::snprintf(terrainPath,sizeof(terrainPath),"/pokegochi/assets/firered/battle_terrain/%s.pkg",terrain);
  if(!AssetRenderer::draw(display,terrainPath,0,0))display.fillRect(0,0,320,184,TFT_WHITE);display.setTextColor(TFT_BLACK);
  display.drawString(wild ? wild->name : "WILD", 8, 8, 2);
  if (opponent) bar(8, 29, "HP", opponent->currentHp, opponent->maximumHp, TFT_GREEN);
  creature(245, 71, opponent ? opponent->speciesId : 0, true);
  battleBackSprite(72, 132, player ? player->speciesId : 0);
  display.drawString(own ? own->name : "PARTNER", 165, 104, 2);
  if (player) bar(165, 126, "HP", player->currentHp, player->maximumHp, TFT_GREEN);
  display.fillRect(0, 153, 320, 31, TFT_DARKGREY); display.setTextColor(TFT_WHITE, TFT_DARKGREY);
  display.drawString(message[0] ? message : "A WILD POKEMON APPEARED!", 7, 164, 1);
  if (gameSave.battle.active) {
    button(3, 190, 74, 43, "FIGHT", TFT_RED);
    button(83, 190, 74, 43, "BAG", TFT_BLUE);
    button(163, 190, 74, 43, "SWITCH", TFT_PURPLE);
    button(243, 190, 74, 43, gameSave.battle.kind == BattleKind::Wild ? "RUN" : "NO RUN",
           gameSave.battle.kind == BattleKind::Wild ? TFT_DARKGREEN : TFT_DARKGREY);
  } else button(92, 190, 136, 43, "CONTINUE", TFT_DARKGREEN);
}

void drawMoveSelect() {
  OwnedPokemon* pokemon = CollectionLogic::find(gameSave.collection, gameSave.battle.playerUid);
  display.fillScreen(TFT_NAVY); display.setTextColor(TFT_WHITE, TFT_NAVY); display.drawString("CHOOSE A MOVE", 10, 9, 2);
  for (uint8_t slot = 0; slot < 4; ++slot) {
    const int16_t x = 8 + (slot % 2) * 156, y = 38 + (slot / 2) * 82;
    const FullMoveData* move = pokemon ? findFullMove(pokemon->moves[slot]) : nullptr;
    display.fillRoundRect(x, y, 148, 72, 6, move && pokemon->movePp[slot] ? TFT_WHITE : TFT_DARKGREY);
    display.drawRoundRect(x, y, 148, 72, 6, TFT_YELLOW);
    display.setTextColor(move ? TFT_BLACK : TFT_LIGHTGREY, move && pokemon->movePp[slot] ? TFT_WHITE : TFT_DARKGREY);
    display.drawString(move ? move->name : "---", x + 8, y + 10, 2);
    if (move) { char pp[24]; std::snprintf(pp, sizeof(pp), "PP %u/%u", pokemon->movePp[slot], move->pp); display.drawString(pp, x + 8, y + 39, 1); }
  }
  button(115, 207, 90, 27, "BACK", TFT_RED);
}

void queueMoveLearning(const BattleActionResult& result) {
  for(uint8_t i=0;i<result.movesToLearnCount&&gameSave.moveLearning.count<kPartyCapacity;++i){
    PendingMoveLearning& pending=gameSave.moveLearning.entries[gameSave.moveLearning.count++];
    pending.pokemonUid=result.moveLearnerUids[i];pending.move=result.movesToLearn[i];
  }
}

void drawMoveLearn() {
  if(gameSave.moveLearning.index>=gameSave.moveLearning.count){gameSave.moveLearning=MoveLearningQueue{};screen=Screen::Battle;drawBattle();return;}
  const PendingMoveLearning& pending=gameSave.moveLearning.entries[gameSave.moveLearning.index];
  OwnedPokemon* pokemon=CollectionLogic::find(gameSave.collection,pending.pokemonUid);
  const SpeciesData* species=pokemon?findSpecies(pokemon->speciesId):nullptr;const FullMoveData* learned=findFullMove(pending.move);
  display.fillScreen(TFT_WHITE);display.fillRoundRect(4,4,312,232,7,TFT_DARKGREY);display.fillRoundRect(8,8,304,224,5,TFT_NAVY);
  display.setTextColor(TFT_WHITE,TFT_NAVY);char title[64];std::snprintf(title,sizeof(title),"%s WANTS TO LEARN",species?species->name:"POKEMON");display.drawString(title,15,16,2);
  display.setTextColor(TFT_YELLOW,TFT_NAVY);display.drawString(learned?learned->name:"A NEW MOVE",15,38,2);
  display.setTextColor(TFT_WHITE,TFT_NAVY);display.drawString("CHOOSE A MOVE TO FORGET",15,61,1);
  for(uint8_t slot=0;slot<kMoveSlots;++slot){const FullMoveData* move=pokemon?findFullMove(pokemon->moves[slot]):nullptr;char label[40];
    std::snprintf(label,sizeof(label),"%s  PP %u",move?move->name:"---",pokemon?pokemon->movePp[slot]:0);button(17,80+slot*29,286,24,label,TFT_BLUE);}
  button(75,202,170,27,"DO NOT LEARN",TFT_RED);
}

void drawBattleParty() {
  display.fillScreen(TFT_LIGHTGREY);
  display.fillRect(0, 0, 320, 27, TFT_DARKCYAN);
  display.setTextColor(TFT_WHITE, TFT_DARKCYAN); display.drawString("CHOOSE A POKEMON", 9, 7, 2);
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) {
    OwnedPokemon* pokemon = CollectionLogic::active(gameSave.collection, slot);
    const int16_t y = 34 + slot * 54;
    const bool inBattle = pokemon && pokemon->uid == gameSave.battle.playerUid;
    const bool selected = battlePartySelection == static_cast<int8_t>(slot);
    const uint16_t background = selected ? TFT_YELLOW : inBattle ? TFT_SKYBLUE : TFT_WHITE;
    display.fillRoundRect(7, y, 306, 48, 7, TFT_DARKGREY);
    display.fillRoundRect(10, y + 3, 300, 42, 5, background);
    if (!pokemon) { display.setTextColor(TFT_DARKGREY, background); display.drawString("EMPTY", 65, y + 17, 2); continue; }
    char path[64]; std::snprintf(path, sizeof(path), "/pokegochi/assets/pokemon/icons/%03u_%u.pkg", pokemon->speciesId, iconFrame);
    AssetRenderer::draw(display, path, 17, y + 7);
    const SpeciesData* species = findSpecies(pokemon->speciesId);
    display.setTextColor(TFT_DARKGREY, background); display.drawString(species ? species->name : "UNKNOWN", 57, y + 8, 2);
    char level[12]; std::snprintf(level, sizeof(level), "Lv%u", pokemon->level); display.drawString(level, 222, y + 8, 1);
    display.drawRect(57, y + 28, 150, 8, TFT_DARKGREY);
    const uint16_t hpWidth = pokemon->maximumHp ? static_cast<uint16_t>(146U * pokemon->currentHp / pokemon->maximumHp) : 0;
    display.fillRect(59, y + 30, hpWidth, 4, pokemon->currentHp * 4U > pokemon->maximumHp ? TFT_GREEN : pokemon->currentHp * 8U > pokemon->maximumHp ? TFT_ORANGE : TFT_RED);
    char hp[22]; std::snprintf(hp, sizeof(hp), "%u/%u", pokemon->currentHp, pokemon->maximumHp); display.drawString(hp, 218, y + 27, 1);
    if (inBattle) display.drawString("IN BATTLE", 243, y + 37, 1);
    else if (pokemon->currentHp == 0 || pokemon->recoverySecondsRemaining) display.drawString("UNABLE", 252, y + 37, 1);
    else if (pokemon->status != StatusCondition::None) display.drawString(PetLogic::statusName(pokemon->status), 248, y + 37, 1);
  }
  if (battlePartySelection >= 0) {
    button(12, 202, 88, 31, "SHIFT", TFT_DARKGREEN);
    button(116, 202, 88, 31, "SUMMARY", TFT_NAVY);
    button(220, 202, 88, 31, "CANCEL", TFT_RED);
  } else button(110, 202, 100, 31, "CANCEL", TFT_RED);
}

void drawBattleSummary() {
  OwnedPokemon* pokemon = battlePartySelection >= 0 ? CollectionLogic::active(gameSave.collection, static_cast<uint8_t>(battlePartySelection)) : nullptr;
  if (!pokemon) { screen = Screen::BattleParty; drawBattleParty(); return; }
  const SpeciesData* species = findSpecies(pokemon->speciesId); const AbilityData* ability = findAbility(pokemon->abilityId);
  display.fillScreen(TFT_WHITE); display.fillRect(0,0,320,30,TFT_NAVY);
  display.setTextColor(TFT_WHITE,TFT_NAVY); display.drawString("POKEMON SUMMARY",8,8,2);
  char path[64]; std::snprintf(path,sizeof(path),"/pokegochi/assets/pokemon/front/%03u.pkg",pokemon->speciesId); AssetRenderer::draw(display,path,18,42);
  display.setTextColor(TFT_BLACK,TFT_WHITE); display.drawString(species?species->name:"UNKNOWN",94,43,2);
  char info[40]; std::snprintf(info,sizeof(info),"Lv%u  HP %u/%u",pokemon->level,pokemon->currentHp,pokemon->maximumHp); display.drawString(info,94,68,1);
  display.drawString("ABILITY",94,88,1); display.drawString(ability?ability->name:"NONE",152,88,1);
  for(uint8_t slot=0;slot<4;++slot){const FullMoveData* move=findFullMove(pokemon->moves[slot]);const int16_t y=116+slot*22;display.fillRoundRect(16,y,288,19,3,TFT_LIGHTGREY);display.setTextColor(TFT_BLACK,TFT_LIGHTGREY);display.drawString(move?move->name:"---",23,y+5,1);if(move){char pp[18];std::snprintf(pp,sizeof(pp),"PP %u/%u",pokemon->movePp[slot],move->pp);display.drawString(pp,244,y+5,1);}}
  button(110,210,100,25,"BACK",TFT_RED);
}

void drawBag() {
  display.fillScreen(TFT_BLACK); display.setTextColor(TFT_YELLOW);
  display.drawString(bagPage == 0 ? "POKE BALL POCKET" : "MEDICINE POCKET", 8, 8, 2);
  if (bagPage == 0) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(PokeBallType::Count); ++i) {
      const PokeBallType type = static_cast<PokeBallType>(i); const int16_t y = 36 + i * 34;
      char label[32]; std::snprintf(label, sizeof(label), "%s  x%u", BattleEngine::ballName(type), gameSave.inventory.balls[i]);
      const bool usable = gameSave.battle.kind == BattleKind::Wild && gameSave.inventory.balls[i];
      button(25, y, 270, 27, label, usable ? TFT_BLUE : TFT_DARKGREY);
    }
  } else {
    const uint8_t first = static_cast<uint8_t>((bagPage - 1U) * 5U);
    for (uint8_t row = 0; row < 5; ++row) {
      const uint8_t i = first + row; if (i >= static_cast<uint8_t>(BattleItem::Count)) break;
      char label[34]; std::snprintf(label, sizeof(label), "%s  x%u", BattleEngine::itemName(static_cast<BattleItem>(i)), gameSave.inventory.medicine[i]);
      button(25, 36 + row * 30, 270, 25, label, gameSave.inventory.medicine[i] ? TFT_DARKGREEN : TFT_DARKGREY);
    }
  }
  button(5, 207, 64, 28, "<", bagPage ? TFT_BLUE : TFT_DARKGREY);
  button(77, 207, 64, 28, ">", bagPage < 3 ? TFT_BLUE : TFT_DARKGREY);
  button(213, 207, 102, 28, "BACK", TFT_RED);
}

void drawInventory() {
  display.fillScreen(TFT_LIGHTGREY);
  for (int16_t y = 0; y < 240; y += 40) for (int16_t x = 0; x < 96; x += 96)
    AssetRenderer::draw(display, "/pokegochi/assets/firered/item_menu/bg.pkg", x, y);
  display.fillRect(96, 0, 224, 240, TFT_WHITE);
  display.fillRect(100, 4, 216, 25, TFT_NAVY);
  display.setTextColor(TFT_WHITE, TFT_NAVY);
  display.drawString(bagPage == 0 ? "POKE BALLS" : "ITEMS", 108, 10, 2);
  char bagAsset[64]; std::snprintf(bagAsset, sizeof(bagAsset),
      "/pokegochi/assets/firered/item_menu/bag_%u.pkg", bagPage);
  AssetRenderer::draw(display, bagAsset, 16, 66);
  if (bagPage == 0) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(PokeBallType::Count); ++i) {
      char label[32]; std::snprintf(label, sizeof(label), "%s  x%u",
          BattleEngine::ballName(static_cast<PokeBallType>(i)), gameSave.inventory.balls[i]);
      const int16_t y = 38 + i * 32; display.fillRect(103, y, 211, 27, i == 0 ? TFT_LIGHTGREY : TFT_WHITE);
      display.setTextColor(TFT_BLACK, i == 0 ? TFT_LIGHTGREY : TFT_WHITE); display.drawString(label, 110, y + 7, 1);
    }
  } else {
    const uint8_t first = static_cast<uint8_t>((bagPage - 1U) * 5U);
    for (uint8_t row = 0; row < 5; ++row) {
      const uint8_t i = first + row; if (i >= static_cast<uint8_t>(BattleItem::Count)) break;
      char label[34]; std::snprintf(label, sizeof(label), "%s  x%u",
          BattleEngine::itemName(static_cast<BattleItem>(i)), gameSave.inventory.medicine[i]);
      const int16_t y = 38 + row * 29; display.fillRect(103, y, 211, 25, row == 0 ? TFT_LIGHTGREY : TFT_WHITE);
      display.setTextColor(TFT_BLACK, row == 0 ? TFT_LIGHTGREY : TFT_WHITE); display.drawString(label, 110, y + 6, 1);
    }
  }
  button(7, 202, 38, 30, "<", bagPage ? TFT_NAVY : TFT_DARKGREY);
  button(50, 202, 38, 30, ">", bagPage < 3 ? TFT_NAVY : TFT_DARKGREY);
  button(213, 202, 102, 30, "CANCEL", TFT_RED);
}

void drawBox() {
  display.fillScreen(TFT_SKYBLUE);
  display.fillRoundRect(3, 3, 314, 234, 7, TFT_DARKGREY);
  display.fillRoundRect(6, 6, 308, 228, 6, TFT_LIGHTGREY);
  display.fillRoundRect(9, 9, 302, 222, 4, TFT_SKYBLUE);
  display.setTextColor(TFT_WHITE, TFT_SKYBLUE); display.drawString("POKEMON BOX", 12, 11, 2);
  char countText[20]; std::snprintf(countText, sizeof(countText), "%u/%u", CollectionLogic::count(gameSave.collection), kBoxCapacity);
  display.drawString(countText, 245, 11, 1);
  button(278, 7, 29, 21, "X", TFT_RED);
  display.fillRoundRect(10, 31, 232, 158, 4, TFT_NAVY);
  display.drawRoundRect(10, 31, 232, 158, 4, TFT_WHITE);
  for (uint8_t col = 1; col < 6; ++col) display.drawFastVLine(10 + col * 38, 32, 156, TFT_BLUE);
  for (uint8_t row = 1; row < 5; ++row) display.drawFastHLine(11, 31 + row * 31, 230, TFT_BLUE);
  display.fillRoundRect(247, 31, 63, 158, 4, TFT_DARKCYAN);
  display.drawRoundRect(247, 31, 63, 158, 4, TFT_WHITE);
  display.setTextColor(TFT_WHITE, TFT_DARKCYAN); display.drawString("PARTY", 259, 35, 1);
  uint8_t visible = 0, skipped = 0;
  for (const auto& pokemon : gameSave.collection.box) {
    if (pokemon.uid == 0) continue;
    if (skipped++ < boxPage * 30U) continue;
    if (visible >= 30) break;
    const int16_t x = 13 + (visible % 6) * 38, y = 31 + (visible / 6) * 31;
    if (pokemon.uid == selectedUid) display.drawRoundRect(x, y, 34, 31, 3, TFT_YELLOW);
    char path[64]; std::snprintf(path, sizeof(path), "/pokegochi/assets/pokemon/icons/%03u_%u.pkg", pokemon.speciesId, iconFrame);
    if (!sdReady || !AssetRenderer::draw(display, path, x + 1, y)) creature(x + 17, y + 16, pokemon.speciesId);
    ++visible;
  }
  for (uint8_t slot = 0; slot < 3; ++slot) {
    OwnedPokemon* pokemon = CollectionLogic::active(gameSave.collection, slot);
    const int16_t y = 52 + slot * 44;
    if (pokemon) { char path[64]; std::snprintf(path, sizeof(path), "/pokegochi/assets/pokemon/icons/%03u_%u.pkg", pokemon->speciesId, iconFrame); AssetRenderer::draw(display, path, 262, y); }
    display.drawRoundRect(255, y - 2, 48, 38, 3, slot == gameSave.activePetSlot ? TFT_YELLOW : TFT_WHITE);
  }
  button(255,166,48,20,"OUT",TFT_RED);
  display.fillRoundRect(10, 194, 300, 33, 4, TFT_WHITE);
  const OwnedPokemon* selected = CollectionLogic::find(gameSave.collection, selectedUid);
  const SpeciesData* data = selected ? findSpecies(selected->speciesId) : nullptr;
  display.setTextColor(TFT_DARKGREY, TFT_WHITE);
  if (selected && data) { char info[64]; std::snprintf(info, sizeof(info), "%s  LV%u  HP %u/%u", data->name, selected->level, selected->currentHp, selected->maximumHp); display.drawString(info, 17, 204, 1); }
  button(200, 7, 28, 21, "<", TFT_NAVY); button(231, 7, 28, 21, ">", TFT_NAVY);
}

void drawPokedex() {
  display.fillScreen(TFT_RED);
  display.fillRoundRect(3, 3, 314, 234, 8, TFT_DARKGREY);
  display.fillRoundRect(7, 7, 306, 226, 6, TFT_WHITE);
  display.fillRoundRect(11, 11, 298, 218, 4, TFT_RED);
  display.fillRoundRect(15, 32, 174, 165, 4, TFT_WHITE);
  display.fillRoundRect(193, 32, 112, 165, 4, TFT_LIGHTGREY);
  display.setTextColor(TFT_WHITE, TFT_RED); display.drawString("KANTO POKEDEX", 18, 13, 2);
  char totals[32];
  std::snprintf(totals, sizeof(totals), "SEEN %u  CAUGHT %u",
                PokedexLogic::seenCount(gameSave.pokedex), PokedexLogic::caughtCount(gameSave.pokedex));
  display.drawString(totals, 184, 16, 1);
  for (uint8_t row = 0; row < 10; ++row) {
    const uint16_t id = static_cast<uint16_t>(dexPage * 10U + row + 1U); if (id > 151) break;
    const SpeciesData* data = findSpecies(id); char entry[32];
    const bool seen = PokedexLogic::hasSeen(gameSave.pokedex, id), caught = PokedexLogic::hasCaught(gameSave.pokedex, id);
    if (!seen) std::snprintf(entry, sizeof(entry), "%03u  ----------", id);
    else std::snprintf(entry, sizeof(entry), "%03u  %s", id, data ? data->name : "UNKNOWN");
    const int16_t y = 37 + row * 15;
    if (id == selectedDexSpecies) display.fillRect(18, y - 2, 168, 14, TFT_YELLOW);
    display.setTextColor(caught ? TFT_DARKGREEN : TFT_DARKGREY, id == selectedDexSpecies ? TFT_YELLOW : TFT_WHITE);
    display.drawString(caught ? "O" : seen ? "+" : " ", 20, y, 1); display.drawString(entry, 32, y, 1);
  }
  const bool selectedSeen = PokedexLogic::hasSeen(gameSave.pokedex, selectedDexSpecies);
  const SpeciesData* selectedData = selectedSeen ? findSpecies(selectedDexSpecies) : nullptr;
  if (selectedData) {
    char path[64]; std::snprintf(path, sizeof(path), "/pokegochi/assets/pokemon/front/%03u.pkg", selectedDexSpecies);
    AssetRenderer::draw(display, path, 217, 43);
    display.setTextColor(TFT_DARKGREY, TFT_LIGHTGREY); display.drawString(selectedData->name, 199, 113, 1);
    char number[16]; std::snprintf(number, sizeof(number), "No.%03u", selectedDexSpecies); display.drawString(number, 199, 129, 1);
    display.drawString(PokedexLogic::hasCaught(gameSave.pokedex, selectedDexSpecies) ? "CAUGHT" : "SEEN", 199, 145, 1);
  } else { display.setTextColor(TFT_DARKGREY, TFT_LIGHTGREY); display.drawString("NO DATA", 220, 110, 2); }
  button(18, 202, 42, 22, "<", TFT_NAVY); button(65, 202, 42, 22, ">", TFT_NAVY);
  button(250, 202, 55, 22, "BACK", TFT_DARKGREY);
}

void drawPokedexDetail() {
  const SpeciesData* species = findSpecies(selectedDexSpecies);
  const PokedexEntryData* entry = pokedexEntry(selectedDexSpecies);
  const bool caught = PokedexLogic::hasCaught(gameSave.pokedex, selectedDexSpecies);
  display.fillScreen(TFT_RED); display.fillRoundRect(4, 4, 312, 232, 8, TFT_DARKGREY);
  display.fillRoundRect(8, 8, 304, 224, 6, TFT_WHITE);
  display.setTextColor(TFT_BLACK, TFT_WHITE);
  char heading[32]; std::snprintf(heading, sizeof(heading), "No.%03u  %s", selectedDexSpecies,
      species ? species->name : "UNKNOWN"); display.drawString(heading, 18, 16, 2);
  if (caught && species && entry) {
    char path[64]; std::snprintf(path, sizeof(path), "/pokegochi/assets/pokemon/front/%03u.pkg", selectedDexSpecies);
    AssetRenderer::draw(display, path, 20, 47);
    char metadata[48]; std::snprintf(metadata, sizeof(metadata), "%s POKEMON", entry->category);
    display.drawString(metadata, 99, 52, 1);
    std::snprintf(metadata, sizeof(metadata), "HT %u.%u m", entry->heightDecimeters / 10U, entry->heightDecimeters % 10U);
    display.drawString(metadata, 99, 70, 1);
    std::snprintf(metadata, sizeof(metadata), "WT %u.%u kg", entry->weightHectograms / 10U, entry->weightHectograms % 10U);
    display.drawString(metadata, 99, 86, 1);
    const EvolutionData* evolution=evolutionFor(selectedDexSpecies);
    if(evolution){const SpeciesData* evolved=findSpecies(evolution->toSpeciesId);std::snprintf(metadata,sizeof(metadata),"EVOLVES: %s LV%u",evolved?evolved->name:"?",evolution->level);display.drawString(metadata,99,102,1);}
    display.drawFastHLine(17, 119, 286, TFT_DARKGREY);
    const char* cursor = entry->description; int16_t y = 128;
    while (*cursor && y < 195) {
      char line[47]{}; uint8_t length = 0, lastSpace = 0;
      while (cursor[length] && length < 45) { if (cursor[length] == ' ') lastSpace = length; ++length; }
      if (cursor[length] && lastSpace) length = lastSpace;
      std::memcpy(line, cursor, length); line[length] = 0; display.drawString(line, 18, y, 1);
      cursor += length; while (*cursor == ' ') ++cursor; y += 15;
    }
  } else {
    display.drawString("NO DETAILED DATA", 88, 105, 2);
    display.drawString("CATCH THIS POKEMON TO UNLOCK.", 56, 130, 1);
  }
  button(110, 202, 100, 25, "BACK", TFT_RED);
}

void drawMart() {
  display.fillScreen(TFT_WHITE);AssetRenderer::draw(display,"/pokegochi/assets/firered/shop/background.pkg",0,0);display.fillRoundRect(4,3,312,29,5,TFT_RED);
  display.setTextColor(TFT_WHITE,TFT_RED); display.drawString("POKE MART",10,8,2);
  char cash[24]; std::snprintf(cash,sizeof(cash),"$%lu",static_cast<unsigned long>(gameSave.money)); display.drawString(cash,245,9,2);
  for(uint8_t i=0;i<kMartOfferCount;++i){const MartOffer& offer=gameSave.mart.offers[i];const int16_t y=35+i*24;display.fillRect(9,y,302,21,TFT_WHITE);display.drawRect(9,y,302,21,TFT_DARKGREY);display.setTextColor(TFT_BLACK,TFT_WHITE);display.drawString(Economy::name(offer.item),18,y+5,1);char details[32];std::snprintf(details,sizeof(details),"$%u  x%u",offer.price,offer.remaining);display.drawString(details,224,y+5,1);}
  button(110,207,100,27,"BACK",TFT_RED);
}

void drawRecovery() {
  display.fillScreen(TFT_BLACK); display.setTextDatum(MC_DATUM); display.setTextColor(TFT_RED);
  display.drawString("SAVE RECOVERY", 160, 75, 4); display.setTextColor(TFT_WHITE);
  display.drawString("NO NEW GAME WAS CREATED", 160, 125, 2); display.setTextDatum(TL_DATUM);
}

void saveNow() {
  if (!saveReady || !saveDirty) return;
  if (saves.commit(gameSave)) { saveDirty = false; lastSaveMs = millis(); }
}

void care(CareAction action) {
  OwnedPokemon* pet = currentPet(); if (!pet) return;
  CollectionLogic::care(*pet, action); saveDirty = true; saveNow(); drawHome();
}

void battleResultMessage(const BattleActionResult& result) {
  if (result.evolved) { const SpeciesData* evolved = findSpecies(result.evolvedSpeciesId); std::snprintf(message, sizeof(message), "EVOLVED INTO %s!", evolved ? evolved->name : "A NEW FORM"); }
  else if (result.outcome == BattleOutcome::Victory) std::snprintf(message, sizeof(message), "YOU WON! +%u XP", result.experienceGained);
  else if (result.outcome == BattleOutcome::Defeat) std::snprintf(message, sizeof(message), "YOUR PARTNER NEEDS TO RECOVER");
  else if (result.outcome == BattleOutcome::Captured) std::snprintf(message, sizeof(message), "CAUGHT! SENT TO YOUR BOX");
  else if (result.outcome == BattleOutcome::Escaped) std::snprintf(message, sizeof(message), "GOT AWAY SAFELY");
  else if (!result.hit && result.accepted) std::snprintf(message, sizeof(message), "THE ATTACK MISSED");
  else if (result.criticalHit) std::snprintf(message,sizeof(message),"A CRITICAL HIT!  DAMAGE %u",result.damageDealt);
  else if (result.effectiveness100>100) std::snprintf(message,sizeof(message),"IT'S SUPER EFFECTIVE!");
  else if (result.effectiveness100<100) std::snprintf(message,sizeof(message),"IT'S NOT VERY EFFECTIVE...");
  else if (result.accepted) std::snprintf(message, sizeof(message), "DEALT %u, TOOK %u", result.damageDealt, result.damageTaken);
}

void handleTap(const TouchPoint& p) {
  if (screen == Screen::Diagnostic && inside(p, 105, 190, 110, 38) && saveReady) {
    if (CollectionLogic::count(gameSave.collection) == 0) { screen = Screen::Starter; drawStarter(); }
    else if (gameSave.moveLearning.index < gameSave.moveLearning.count) { screen=Screen::MoveLearn;drawMoveLearn(); }
    else if (gameSave.battle.active) { screen = Screen::Battle; drawBattle(); }
    else { screen = Screen::Home; drawHome(); }
    return;
  }
  if (screen == Screen::Starter) {
    const uint16_t ids[3] = {1, 4, 7};
    for (uint8_t i = 0; i < 3; ++i) if (inside(p, 10 + i * 105, 165, 90, 48)) {
      if (CollectionLogic::chooseStarter(gameSave.collection, ids[i])) {
        PokedexLogic::markCaught(gameSave.pokedex, ids[i]);
        saveDirty = true; saveNow(); screen = Screen::Home; drawHome();
      }
      return;
    }
  }
  if (screen == Screen::Home) {
    if (inside(p, 43, 3, 44, 22)) { bagPage = 0; screen = Screen::Inventory; drawInventory(); return; }
    if (inside(p, 139, 3, 44, 22)) { screen = Screen::Pokedex; drawPokedex(); return; }
    if (inside(p, 91, 3, 44, 22)) { if(!gameSave.mart.offers[0].remaining) Economy::rotate(gameSave.mart); screen=Screen::Mart; drawMart(); return; }
    if (inside(p, 270, 3, 46, 22)) { selectedUid = currentPet() ? currentPet()->uid : 0; screen = Screen::Box; drawBox(); return; }
    for (uint8_t i = 0; i < 3; ++i) if (inside(p, 179 + i * 25, 2, 23, 24) && CollectionLogic::active(gameSave.collection, i)) {
      gameSave.activePetSlot = i; saveDirty = true; saveNow(); drawHome(); return;
    }
    if (inside(p, 6, 187, 72, 45)) care(CareAction::Feed);
    else if (inside(p, 84, 187, 72, 45)) care(CareAction::Bathe);
    else if (inside(p, 162, 187, 72, 45)) care(CareAction::Play);
    else if (inside(p, 240, 187, 74, 45)) {
      if (gameSave.encounterCharges.available) { pendingBattleKind = BattleKind::Trainer; screen = Screen::ChooseBattler; drawChooseBattler(); }
      else { display.fillRect(5, 172, 310, 12, TFT_BLACK); display.setTextColor(TFT_ORANGE); display.drawString("NO TRAINER CHARGES", 6, 174, 1); }
    }
    return;
  }
  if (screen == Screen::ChooseBattler) {
    if (pendingBattleKind == BattleKind::Trainer && inside(p, 120, 216, 80, 21)) { pendingBattleKind = BattleKind::None; screen = Screen::Home; drawHome(); return; }
    for (uint8_t slot = 0; slot < kPartyCapacity; ++slot) {
      if (!inside(p, 11 + slot * 106, 174, 86, 38)) continue;
      OwnedPokemon* pokemon = CollectionLogic::active(gameSave.collection, slot);
      if (!pokemon || pokemon->currentHp == 0 || pokemon->recoverySecondsRemaining) return;
      const bool started = pendingBattleKind == BattleKind::Wild
          ? BattleEngine::startWild(gameSave.battle, gameSave.collection, pokemon->uid, micros())
          : pendingBattleKind == BattleKind::Gym
              ? GymSystem::start(gameSave.battle, gameSave.collection, pokemon->uid, gameSave.gymProgress,
                                 GymSystem::next(gameSave.gymProgress), micros())
              : BattleEngine::startTrainer(gameSave.battle, gameSave.encounterCharges, gameSave.collection, pokemon->uid, micros());
      if (!started) return;
      OwnedPokemon* opponent = BattleEngine::currentOpponent(gameSave.battle);
      if (gameSave.battle.kind == BattleKind::Wild && opponent) {
        PokedexLogic::markSeen(gameSave.pokedex, opponent->speciesId);
        EncounterLogic::acknowledgeWild(gameSave.wildEncounterClock);
      }
      if (gameSave.battle.kind == BattleKind::Gym) EncounterLogic::acknowledgeWild(gameSave.wildEncounterClock);
      pendingBattleKind = BattleKind::None; message[0] = 0; saveDirty = true; saveNow();
      if (gameSave.battle.kind == BattleKind::Trainer || gameSave.battle.kind == BattleKind::Gym) { screen = Screen::TrainerIntro; drawTrainerIntro(); }
      else { screen = Screen::Battle; drawBattle(); }
      return;
    }
    return;
  }
  if (screen == Screen::TrainerIntro) { screen = Screen::Battle; drawBattle(); return; }
  if (screen == Screen::Battle) {
    if (!gameSave.battle.active && inside(p, 92, 190, 136, 43)) { BattleEngine::clear(gameSave.battle); saveDirty = true; saveNow(); screen = Screen::Home; drawHome(); return; }
    BattleActionResult result;
    if (inside(p, 3, 190, 74, 43)) { screen = Screen::MoveSelect; drawMoveSelect(); return; }
    else if (inside(p, 83, 190, 74, 43)) { bagPage = 0; screen = Screen::Bag; drawBag(); return; }
    else if (inside(p, 163, 190, 74, 43)) { battlePartySelection = -1; screen = Screen::BattleParty; drawBattleParty(); return; }
    else if (inside(p, 243, 190, 74, 43) && gameSave.battle.kind == BattleKind::Wild) result = BattleEngine::run(gameSave.battle, gameSave.collection);
    if (result.accepted) {
      for (uint8_t i = 0; i < result.evolvedCount; ++i) PokedexLogic::markCaught(gameSave.pokedex, result.evolvedSpeciesIds[i]);
      bool nextGymStage=false;
      if (result.outcome == BattleOutcome::Victory && gameSave.battle.kind == BattleKind::Gym) {
        nextGymStage=GymSystem::advance(gameSave.battle,gameSave.collection);
        if(!nextGymStage)GymSystem::recordVictory(gameSave.gymProgress, static_cast<GymId>(gameSave.battle.gymId));
      }
      gameSave.money += result.moneyGained;
      if (result.caught) { const OwnedPokemon* opponent = BattleEngine::currentOpponent(gameSave.battle); if (opponent) PokedexLogic::markCaught(gameSave.pokedex, opponent->speciesId); }
      battleResultMessage(result); saveDirty = true; saveNow(); if(nextGymStage){screen=Screen::TrainerIntro;drawTrainerIntro();}else drawBattle();
    }
    return;
  }
  if (screen == Screen::MoveSelect) {
    if (inside(p, 115, 207, 90, 27)) { screen = Screen::Battle; drawBattle(); return; }
    for (uint8_t slot = 0; slot < 4; ++slot) {
      const int16_t x = 8 + (slot % 2) * 156, y = 38 + (slot / 2) * 82;
      if (!inside(p, x, y, 148, 72)) continue;
      OwnedPokemon* attacker = CollectionLogic::find(gameSave.collection, gameSave.battle.playerUid);
      if (!attacker || attacker->moves[slot] == MoveId::None || attacker->movePp[slot] == 0) return;
      playMoveAnimation(attacker->moves[slot]);
      const BattleActionResult result = BattleEngine::fight(gameSave.battle, gameSave.collection, slot);
      if (result.accepted) {
        queueMoveLearning(result);
        for (uint8_t i = 0; i < result.evolvedCount; ++i) { PokedexLogic::markCaught(gameSave.pokedex, result.evolvedSpeciesIds[i]); playEvolutionAnimation(result.evolvedFromSpeciesIds[i], result.evolvedSpeciesIds[i]); }
        bool nextGymStage=false;if (result.outcome == BattleOutcome::Victory && gameSave.battle.kind == BattleKind::Gym){nextGymStage=GymSystem::advance(gameSave.battle,gameSave.collection);if(!nextGymStage)GymSystem::recordVictory(gameSave.gymProgress, static_cast<GymId>(gameSave.battle.gymId));}
        gameSave.money += result.moneyGained; battleResultMessage(result); saveDirty = true; saveNow();
      }
      if(gameSave.moveLearning.index<gameSave.moveLearning.count){screen=Screen::MoveLearn;drawMoveLearn();}
      else if(gameSave.battle.active&&gameSave.battle.kind==BattleKind::Gym&&gameSave.battle.opponentIndex==0&&gameSave.battle.gymStage>0){screen=Screen::TrainerIntro;drawTrainerIntro();}
      else {screen = Screen::Battle; drawBattle();} return;
    }
    return;
  }
  if(screen==Screen::MoveLearn){
    PendingMoveLearning pending=gameSave.moveLearning.entries[gameSave.moveLearning.index];OwnedPokemon* pokemon=CollectionLogic::find(gameSave.collection,pending.pokemonUid);
    bool resolved=false;
    for(uint8_t slot=0;slot<kMoveSlots;++slot)if(inside(p,17,80+slot*29,286,24)&&pokemon){pokemon->moves[slot]=pending.move;const FullMoveData* move=findFullMove(pending.move);pokemon->movePp[slot]=move?move->pp:0;resolved=true;}
    if(inside(p,75,202,170,27))resolved=true;
    if(resolved){++gameSave.moveLearning.index;saveDirty=true;saveNow();if(gameSave.moveLearning.index<gameSave.moveLearning.count)drawMoveLearn();else{gameSave.moveLearning=MoveLearningQueue{};saveDirty=true;saveNow();if(gameSave.battle.active&&gameSave.battle.kind==BattleKind::Gym&&gameSave.battle.gymStage>0&&gameSave.battle.opponentIndex==0){screen=Screen::TrainerIntro;drawTrainerIntro();}else{screen=Screen::Battle;drawBattle();}}}
    return;
  }
  if (screen == Screen::BattleParty) {
    for (uint8_t slot=0;slot<kPartyCapacity;++slot) if(inside(p,7,34+slot*54,306,48)){OwnedPokemon* pokemon=CollectionLogic::active(gameSave.collection,slot);if(pokemon)battlePartySelection=slot;drawBattleParty();return;}
    if (inside(p,220,202,88,31) || (battlePartySelection<0 && inside(p,110,202,100,31))) { battlePartySelection=-1; screen=Screen::Battle; drawBattle(); return; }
    if (battlePartySelection>=0 && inside(p,116,202,88,31)) { screen=Screen::BattleSummary; drawBattleSummary(); return; }
    if (battlePartySelection>=0 && inside(p,12,202,88,31)) {
      OwnedPokemon* target=CollectionLogic::active(gameSave.collection,static_cast<uint8_t>(battlePartySelection));
      const BattleActionResult result=target?BattleEngine::switchToPokemon(gameSave.battle,gameSave.collection,target->uid):BattleActionResult{};
      if(result.accepted){battleResultMessage(result);saveDirty=true;saveNow();battlePartySelection=-1;screen=Screen::Battle;drawBattle();} return;
    }
    return;
  }
  if(screen==Screen::BattleSummary){if(inside(p,110,210,100,25)){screen=Screen::BattleParty;drawBattleParty();}return;}
  if (screen == Screen::Bag) {
    if (inside(p, 213, 207, 102, 28)) { screen = Screen::Battle; drawBattle(); return; }
    if (inside(p, 5, 207, 64, 28) && bagPage) { --bagPage; drawBag(); return; }
    if (inside(p, 77, 207, 64, 28) && bagPage < 3) { ++bagPage; drawBag(); return; }
    if (bagPage == 0) for (uint8_t i = 0; i < static_cast<uint8_t>(PokeBallType::Count); ++i) {
      if (!inside(p, 25, 36 + i * 34, 270, 27)) continue;
      const PokeBallType ball=static_cast<PokeBallType>(i);playBallAnimation(ball); const BattleActionResult result = BattleEngine::throwBall(gameSave.battle, gameSave.collection,
          gameSave.inventory, ball);if(result.accepted)playBallResult(ball,result.caught);
      if (result.accepted) {
        if (result.caught) { const OwnedPokemon* opponent = BattleEngine::currentOpponent(gameSave.battle); if (opponent) PokedexLogic::markCaught(gameSave.pokedex, opponent->speciesId); }
        battleResultMessage(result); saveDirty = true; saveNow(); screen = Screen::Battle; drawBattle();
      }
      return;
    }
    if (bagPage > 0) for (uint8_t row = 0; row < 5; ++row) {
      const uint8_t i = static_cast<uint8_t>((bagPage - 1U) * 5U + row);
      if (i >= static_cast<uint8_t>(BattleItem::Count) || !inside(p, 25, 36 + row * 30, 270, 25)) continue;
      const BattleActionResult result = BattleEngine::useItem(gameSave.battle, gameSave.collection,
          gameSave.inventory, static_cast<BattleItem>(i));
      if (result.accepted) { battleResultMessage(result); saveDirty = true; saveNow(); screen = Screen::Battle; drawBattle(); }
      return;
    }
  }
  if (screen == Screen::Inventory) {
    if (inside(p, 213, 202, 102, 30)) { screen = Screen::Home; drawHome(); return; }
    if (inside(p, 7, 202, 38, 30) && bagPage) { --bagPage; drawInventory(); return; }
    if (inside(p, 50, 202, 38, 30) && bagPage < 3) { ++bagPage; drawInventory(); return; }
    return;
  }
  if (screen == Screen::Box) {
    if (inside(p, 200, 7, 28, 21) && boxPage > 0) { --boxPage; drawBox(); return; }
    if (inside(p, 231, 7, 28, 21) && boxPage < (kBoxCapacity - 1U) / 30U) { ++boxPage; drawBox(); return; }
    if (inside(p, 278, 7, 29, 21)) { screen = Screen::Home; drawHome(); return; }
    uint8_t visible = 0, skipped = 0;
    for (const auto& pokemon : gameSave.collection.box) {
      if (!pokemon.uid) continue; if (skipped++ < boxPage * 30U) continue; if (visible >= 30) break;
      const int16_t x = 13 + (visible % 6) * 38, y = 31 + (visible / 6) * 31;
      if (inside(p, x, y, 34, 31)) { selectedUid = pokemon.uid; drawBox(); return; } ++visible;
    }
    for (uint8_t slot = 0; slot < 3; ++slot) if (inside(p, 255, 50 + slot * 44, 48, 38) && selectedUid) {
      if (CollectionLogic::setPartySlot(gameSave.collection, slot, selectedUid)) { gameSave.activePetSlot = slot; saveDirty = true; saveNow(); drawBox(); }
      return;
    }
    if(inside(p,255,166,48,20)&&selectedUid&&CollectionLogic::removeFromParty(gameSave.collection,selectedUid)){
      gameSave.activePetSlot=0;saveDirty=true;saveNow();drawBox();return;
    }
  }
  if (screen == Screen::Pokedex) {
    if (inside(p, 18, 202, 42, 22) && dexPage > 0) { --dexPage; selectedDexSpecies = dexPage * 10U + 1U; drawPokedex(); return; }
    if (inside(p, 65, 202, 42, 22) && dexPage < 15) { ++dexPage; selectedDexSpecies = dexPage * 10U + 1U; drawPokedex(); return; }
    if (inside(p, 250, 202, 55, 22)) { screen = Screen::Home; drawHome(); return; }
    if (inside(p, 193, 32, 112, 165) && PokedexLogic::hasSeen(gameSave.pokedex, selectedDexSpecies)) {
      screen = Screen::PokedexDetail; drawPokedexDetail(); return;
    }
    for (uint8_t row = 0; row < 10; ++row) if (inside(p, 18, 35 + row * 15, 168, 14)) {
      const uint16_t id = dexPage * 10U + row + 1U; if (id <= 151) { selectedDexSpecies = id; drawPokedex(); } return;
    }
  }
  if (screen == Screen::PokedexDetail) {
    if (inside(p, 110, 202, 100, 25)) { screen = Screen::Pokedex; drawPokedex(); }
    return;
  }
  if(screen==Screen::Mart){if(inside(p,110,207,100,27)){screen=Screen::Home;drawHome();return;}for(uint8_t i=0;i<kMartOfferCount;++i)if(inside(p,10,35+i*24,300,21)){if(Economy::buy(gameSave.mart,i,gameSave.money,gameSave.inventory)){saveDirty=true;saveNow();}drawMart();return;}return;}
}
}

void setup() {
  Serial.begin(115200); delay(250); backlight.begin(board::kBacklightPin, board::kBacklightOnLevel);
  screenButton.begin(board::kScreenButtonPin, board::kScreenButtonPressedLevel);
  display.init(); display.setRotation(board::kDisplayRotation); display.setTextDatum(TL_DATUM);
  touch.begin(); sdReady = testSd();
  if (saves.begin()) {
    const SaveLoadResult result = saves.loadOrCreate(gameSave);
    saveReady = result == SaveLoadResult::Loaded || result == SaveLoadResult::FirstStart;
    if (result == SaveLoadResult::Corrupted) screen = Screen::Recovery;
  }
  if (saveReady) { ++gameSave.bootCount; saveDirty = true; saveNow(); }
  if (screen == Screen::Recovery) drawRecovery(); else drawDiagnostic(); lastSecondMs = millis();
}

void loop() {
  if (screenButton.pressed()) { saveNow(); backlight.toggle(); }
  const TouchPoint point = touch.read();
  if (point.touched && !wasTouched && backlight.isEnabled()) handleTap(point); wasTouched = point.touched;
  const uint32_t now = millis();
  if (saveReady && now - lastSecondMs >= 1000) {
    const uint32_t seconds = (now - lastSecondMs) / 1000; lastSecondMs += seconds * 1000; gameSave.playTimeSeconds += seconds;
    EncounterLogic::advance(gameSave.encounterCharges, seconds);
    Economy::advance(gameSave.mart, seconds);
    EncounterLogic::advanceWild(gameSave.wildEncounterClock, seconds);
    // Recovery belongs to the Pokemon, not to its current party slot. Advancing
    // the whole collection prevents a fainted Pokemon from being frozen forever
    // when the player moves it to the Box.
    for (auto& pokemon : gameSave.collection.box) {
      if (pokemon.uid != kEmptyPokemonUid) CollectionLogic::advanceCare(pokemon, seconds);
    }
    saveDirty = true;
  }
  if (saveReady && screen == Screen::Home && !gameSave.battle.active && gameSave.wildEncounterClock.pending) {
    const bool gymAvailable = GymSystem::next(gameSave.gymProgress) != GymId::Count;
    pendingBattleKind = gymAvailable && gameSave.wildEncounterClock.rngState % 20U == 0 ? BattleKind::Gym : BattleKind::Wild;
    screen = Screen::ChooseBattler; drawChooseBattler();
  }
  if (saveDirty && now - lastSaveMs >= 60000) saveNow(); delay(2);
  if (backlight.isEnabled() && now - lastAnimationMs >= 650 && (screen == Screen::Home || screen == Screen::Box)) {
    lastAnimationMs = now; iconFrame ^= 1U;
    if (screen == Screen::Home) drawHome(); else drawBox();
  }
}
