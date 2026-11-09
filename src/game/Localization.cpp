#include "game/Localization.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

struct TranslationEntry {
  uint32_t hash;
  const char* english;
  const char* portuguese;
};

// Generated from every player-visible sentence, Pokedex entry, move
// description/name, Ability, item and trainer line in the game.
#include "PortugueseLocalizationGenerated.inc"

UiLanguage selectedLanguage = UiLanguage::English;
// Exact long-form prose lives in flash. This scratch is only for short
// formatted runtime messages, keeping the ESP32's BLE/Wi-Fi DRAM margin.
constexpr size_t kTranslatedTextCapacity = 768U;
char translatedText[kTranslatedTextCapacity]{};

constexpr uint32_t fnv1a(const char* text) {
  uint32_t hash = 2166136261U;
  if (!text) return hash;
  while (*text) {
    hash ^= static_cast<uint8_t>(*text++);
    hash *= 16777619U;
  }
  return hash;
}

const char* exactTranslation(const char* english) {
  if (!english || !english[0]) return english;
  const uint32_t wanted = fnv1a(english);
  size_t first = 0U;
  size_t last = sizeof(kPortugueseTranslations) /
      sizeof(kPortugueseTranslations[0]);
  while (first < last) {
    const size_t middle = first + (last - first) / 2U;
    if (kPortugueseTranslations[middle].hash < wanted)
      first = middle + 1U;
    else
      last = middle;
  }
  for (size_t index = first;
       index < sizeof(kPortugueseTranslations) /
           sizeof(kPortugueseTranslations[0]) &&
       kPortugueseTranslations[index].hash == wanted;
       ++index) {
    if (std::strcmp(kPortugueseTranslations[index].english, english) == 0)
      return kPortugueseTranslations[index].portuguese;
  }
  return nullptr;
}

bool asciiLetter(char value) {
  return (value >= 'A' && value <= 'Z') ||
      (value >= 'a' && value <= 'z');
}

char upperAscii(char value) {
  return value >= 'a' && value <= 'z'
      ? static_cast<char>(value - 'a' + 'A') : value;
}

bool equalAsciiInsensitive(char left, char right) {
  return upperAscii(left) == upperAscii(right);
}

struct PhraseTranslation {
  const char* english;
  const char* portuguese;
};

// Word order matters in the most frequent formatted battle/system messages.
// Complete static sentences never reach this fallback because they are in
// the generated exact catalogue above.
constexpr PhraseTranslation kFormattedPhrases[] = {
    {"WAITING FOR THE OTHER PLAYER", "AGUARDANDO O OUTRO JOGADOR"},
    {"THE OTHER PLAYER", "O OUTRO JOGADOR"},
    {"CHOOSE YOUR NEXT POKEMON", "ESCOLHA SEU PROXIMO POKEMON"},
    {"CHOOSE YOUR POKEMON", "ESCOLHA SEU POKEMON"},
    {"CHOOSE YOUR BATTLER", "ESCOLHA SEU LUTADOR"},
    {"SENT TO YOUR BOX", "ENVIADO PARA SUA CAIXA"},
    {"WANTS TO LEARN", "QUER APRENDER"},
    {"FOR WINNING", "POR VENCER"},
    {"ATTACK MISSED", "ATAQUE ERROU"},
    {"SUPER EFFECTIVE", "SUPER EFETIVO"},
    {"NOT VERY EFFECTIVE", "POUCO EFETIVO"},
    {"CRITICAL HIT", "ACERTO CRITICO"},
    {"EVOLVED INTO", "EVOLUIU PARA"},
    {"GREW TO", "CHEGOU AO"},
    {"THE FOE", "O OPONENTE"},
    {"YOUR PARTY", "SUA EQUIPE"},
    {"THE PARTY", "A EQUIPE"},
    {"YOUR BOX", "SUA CAIXA"},
    {"PLAYER GOT", "JOGADOR RECEBEU"},
    {"PLAYER WON", "JOGADOR VENCEU"},
    {"PLAYER LOST", "JOGADOR PERDEU"},
    {"LINK BATTLE", "BATALHA LINK"},
    {"BATTLE TOWER", "TORRE DE BATALHA"},
    {"POKEMON LEAGUE", "LIGA POKEMON"},
    {"MASTER BALL", "MASTER BALL"},
};

bool phraseMatches(const char* cursor, const char* phrase) {
  if (!cursor || !phrase) return false;
  size_t length = 0U;
  while (phrase[length]) {
    if (!cursor[length] ||
        !equalAsciiInsensitive(cursor[length], phrase[length])) return false;
    ++length;
  }
  return !asciiLetter(phrase[length - 1U]) || !asciiLetter(cursor[length]);
}

void appendText(const char* source, size_t& output) {
  if (!source) return;
  while (*source && output + 1U < kTranslatedTextCapacity)
    translatedText[output++] = *source++;
}

const char* tokenTranslation(const char* english) {
  size_t output = 0U;
  const char* cursor = english;
  while (*cursor && output + 1U < kTranslatedTextCapacity) {
    bool replacedPhrase = false;
    for (const PhraseTranslation& phrase : kFormattedPhrases) {
      if (!phraseMatches(cursor, phrase.english)) continue;
      appendText(phrase.portuguese, output);
      cursor += std::strlen(phrase.english);
      replacedPhrase = true;
      break;
    }
    if (replacedPhrase) continue;

    if (asciiLetter(*cursor)) {
      char word[64]{};
      size_t length = 0U;
      while (asciiLetter(cursor[length]) && length + 1U < sizeof(word)) {
        word[length] = upperAscii(cursor[length]);
        ++length;
      }
      word[length] = 0;
      const char* translated = exactTranslation(word);
      appendText(translated ? translated : word, output);
      cursor += length;
      continue;
    }
    translatedText[output++] = *cursor++;
  }
  translatedText[output] = 0;
  return translatedText;
}

}  // namespace

namespace Localization {

void setLanguage(UiLanguage language) { selectedLanguage = language; }
UiLanguage language() { return selectedLanguage; }
bool isPortuguese() { return selectedLanguage == UiLanguage::Portuguese; }

const char* translate(const char* english) {
  if (!english || !isPortuguese()) return english;
  if (const char* exact = exactTranslation(english)) return exact;
  return tokenTranslation(english);
}

}  // namespace Localization
