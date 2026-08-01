#include "game/PetState.h"

#include <cassert>
#include <iostream>

int main() {
  PetState pet;
  PetLogic::advance(pet, 899);
  assert(pet.fullness == 80);
  PetLogic::advance(pet, 1);
  assert(pet.fullness == 79);

  PetLogic::advance(pet, 900UL * 80U);
  assert(pet.fullness == 0);
  assert(pet.happiness < 80);

  PetLogic::care(pet, CareAction::Feed);
  assert(pet.fullness == 25);
  pet.status = StatusCondition::Paralysis;
  PetLogic::care(pet, CareAction::Bathe);
  assert(pet.status == StatusCondition::None);
  PetLogic::care(pet, CareAction::Play);
  assert(pet.happiness <= 100);

  PetLogic::defeat(pet, 1000, 7200);
  assert(!PetLogic::canBattle(pet, 8199));
  PetLogic::recoverIfReady(pet, 8200);
  assert(PetLogic::canBattle(pet, 8200));
  assert(pet.currentHp == pet.maximumHp);

  std::cout << "PetState tests passed\n";
  return 0;
}
