#include "game/PetState.h"

#include <cassert>
#include <cstring>
#include <iostream>

int main() {
  assert(std::strcmp(PetLogic::statusName(StatusCondition::Poison), "POISON") == 0);
  assert(std::strcmp(PetLogic::statusAdjective(StatusCondition::Poison), "POISONED") == 0);
  assert(std::strcmp(PetLogic::statusAdjective(StatusCondition::BadlyPoisoned), "BADLY POISONED") == 0);
  assert(std::strcmp(PetLogic::statusAdjective(StatusCondition::Paralysis), "PARALYZED") == 0);
  assert(std::strcmp(PetLogic::statusAdjective(StatusCondition::Burn), "BURNED") == 0);
  assert(std::strcmp(PetLogic::statusAdjective(StatusCondition::Sleep), "ASLEEP") == 0);
  assert(std::strcmp(PetLogic::statusAdjective(StatusCondition::Frozen), "FROZEN") == 0);
  PetState pet;
  PetLogic::advanceFriendship(pet, 3599);
  assert(pet.friendship == 70);
  PetLogic::advanceFriendship(pet, 1);
  assert(pet.friendship == 75);

  pet.status = StatusCondition::Paralysis;
  pet.currentHp = 1;
  PetLogic::care(pet, CareAction::Center);
  assert(pet.status == StatusCondition::None);
  assert(pet.currentHp == pet.maximumHp);

  pet.status = StatusCondition::Burn;
  PetLogic::defeat(pet, 1000, 7200);
  assert(pet.status == StatusCondition::None);
  assert(!PetLogic::canBattle(pet, 8199));
  PetLogic::recoverIfReady(pet, 8200);
  assert(PetLogic::canBattle(pet, 8200));
  assert(pet.currentHp == pet.maximumHp);

  std::cout << "PetState tests passed\n";
  return 0;
}
