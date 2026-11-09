#include "game/Localization.h"

#include <cassert>
#include <cstring>

int main() {
  Localization::setLanguage(UiLanguage::English);
  assert(Localization::language() == UiLanguage::English);
  assert(std::strcmp(Localization::translate("SETTINGS"), "SETTINGS") == 0);

  Localization::setLanguage(UiLanguage::Portuguese);
  assert(Localization::isPortuguese());
  assert(std::strcmp(Localization::translate("SETTINGS"), "CONFIGURACOES") == 0);
  assert(std::strcmp(Localization::translate("LANGUAGE"), "IDIOMA") == 0);
  assert(std::strcmp(Localization::translate("SWORDS DANCE"),
                     "DANCA DAS ESPADAS") == 0);
  assert(std::strcmp(Localization::translate("PORTUGUESE (BRAZIL)"),
                     "PORTUGUES BRASIL") == 0);
  assert(std::strcmp(Localization::translate("MOBILE PHONE"),
                     "TELEFONE CELULAR") == 0);
  assert(std::strcmp(Localization::translate("FILE"), "ARQUIVO") == 0);
  assert(std::strcmp(Localization::translate("PARTY"), "EQUIPE") == 0);

  const char* dynamic = Localization::translate("PIKACHU USED TACKLE!");
  assert(std::strstr(dynamic, "PIKACHU") != nullptr);
  assert(std::strstr(dynamic, "USOU") != nullptr);
  assert(std::strstr(dynamic, "INVESTIDA") != nullptr);
  assert(std::strstr(dynamic, "USED") == nullptr);

  const char* formatted = Localization::translate(
      "WAITING FOR THE OTHER PLAYER 27");
  assert(std::strstr(formatted, "AGUARDANDO O OUTRO JOGADOR") != nullptr);
  return 0;
}
