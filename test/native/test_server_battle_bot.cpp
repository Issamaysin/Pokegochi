#include <cassert>
#include <cstdint>

#include "game/BattleEngine.h"
#include "ServerBattleBot.h"

namespace {

OwnedPokemon battlePokemon(uint16_t species, uint16_t hp, uint16_t maximumHp,
                           MoveId first, MoveId second,
                           MoveId third = MoveId::None,
                           MoveId fourth = MoveId::None) {
  OwnedPokemon pokemon{};
  pokemon.uid = species;
  pokemon.speciesId = species;
  pokemon.currentHp = hp;
  pokemon.maximumHp = maximumHp;
  pokemon.level = 70U;
  pokemon.friendship = 255U;
  pokemon.moves[0] = first;
  pokemon.moves[1] = second;
  pokemon.moves[2] = third;
  pokemon.moves[3] = fourth;
  for (uint8_t slot = 0U; slot < kMoveSlots; ++slot)
    pokemon.movePp[slot] = pokemon.moves[slot] == MoveId::None ? 0U : 10U;
  return pokemon;
}

}  // namespace

int main() {
  using namespace PokegochiServerBot;
  assert(kPlayerId == 99999U);
  assert(rosterSize() == 70U);

  uint8_t generationCounts[8]{};
  uint8_t legendaryCounts[8]{};
  uint8_t shinyCount = 0U;
  constexpr uint16_t requiredSpecies[] = {
      149U, 202U, 381U, 384U, 448U, 491U,
      658U, 681U, 716U, 718U, 778U};
  bool requiredFound[sizeof(requiredSpecies) / sizeof(requiredSpecies[0])]{};
  for (size_t index = 0; index < rosterSize(); ++index) {
    const RosterEntry& entry = roster()[index];
    assert(entry.speciesId != 0U);
    assert(entry.generation >= 1U && entry.generation <= 7U);
    ++generationCounts[entry.generation];
    if (entry.legendary) ++legendaryCounts[entry.generation];
    if (entry.shiny) {
      ++shinyCount;
      assert(index == kShinyRosterIndex);
    }
    for (size_t required = 0;
         required < sizeof(requiredSpecies) / sizeof(requiredSpecies[0]);
         ++required)
      if (entry.speciesId == requiredSpecies[required])
        requiredFound[required] = true;
    uint16_t evTotal = entry.evs.hp + entry.evs.attack + entry.evs.defense +
        entry.evs.spAttack + entry.evs.spDefense + entry.evs.speed;
    assert(evTotal <= 510U);
    assert(entry.abilityId != 0U);
    assert(entry.zMoveSlot < kMoveSlots);
    assert(entry.zCrystal >= HeldItem::NormaliumZ &&
           entry.zCrystal <= HeldItem::FairiumZ);
    for (uint8_t move = 0; move < kMoveSlots; ++move) {
      assert(entry.moves[move] != 0U);
      assert(entry.movePp[move] != 0U);
    }
  }
  assert(shinyCount == 1U);
  for (bool found : requiredFound) assert(found);
  for (uint8_t generation = 1U; generation <= 7U; ++generation) {
    assert(generationCounts[generation] == 10U);
    assert(legendaryCounts[generation] >= 1U);
  }

  for (uint32_t seed = 1U; seed <= 2048U; ++seed) {
    const Team normal = buildTeam(seed, 100U, static_cast<uint16_t>(seed));
    const Team repeat = buildTeam(seed, 100U, static_cast<uint16_t>(seed));
    for (uint8_t slot = 0; slot < kTeamSize; ++slot) {
      assert(normal.rosterIndexes[slot] == repeat.rosterIndexes[slot]);
      assert(normal.members[slot].speciesId == repeat.members[slot].speciesId);
      assert(normal.members[slot].uid != 0U);
      assert(normal.members[slot].level == 100U);
      assert(normal.members[slot].maximumHp == normal.members[slot].currentHp);
      assert(normal.members[slot].ivs.hp == 31U);
      assert(normal.rosterIndexes[slot] < rosterSize());
      for (uint8_t other = 0; other < slot; ++other)
        assert(normal.rosterIndexes[slot] != normal.rosterIndexes[other]);
    }
    if (normal.megaSlot != 0xFFU) {
      assert(normal.megaSlot < kTeamSize);
      assert(roster()[normal.rosterIndexes[normal.megaSlot]].megaEligible);
      assert(normal.members[normal.megaSlot].heldItem == HeldItem::MegaStone);
    } else {
      for (uint8_t slot = 0; slot < kTeamSize; ++slot)
        assert(!roster()[normal.rosterIndexes[slot]].megaEligible ||
               (normal.members[slot].speciesId == 658U &&
                normal.members[slot].heldItem == HeldItem::AshsBond));
    }
    assert(normal.zSlot < kTeamSize);
    assert(normal.zSlot != normal.megaSlot);
    assert(normal.zMoveSlot < kMoveSlots);
    assert(normal.members[normal.zSlot].heldItem >= HeldItem::NormaliumZ &&
           normal.members[normal.zSlot].heldItem <= HeldItem::FairiumZ);
    for (uint8_t slot = 0; slot < kTeamSize; ++slot)
      if (normal.members[slot].speciesId == 658U)
        assert(normal.members[slot].heldItem == HeldItem::AshsBond &&
               normal.megaSlot != slot && normal.zSlot != slot);
  }

  const Team balanced = buildTeam(0x12345678U, 70U, 42U);
  for (const OwnedPokemon& pokemon : balanced.members)
    assert(pokemon.level == 70U && pokemon.currentHp == pokemon.maximumHp);

  OwnedPokemon venusaur = battlePokemon(
      3U, 100U, 100U, static_cast<MoveId>(202U),
      static_cast<MoveId>(79U));
  OwnedPokemon charizard = battlePokemon(
      6U, 100U, 100U, static_cast<MoveId>(105U),
      static_cast<MoveId>(53U), static_cast<MoveId>(14U));
  CombatVolatile userVolatile{};
  CombatVolatile targetVolatile{};
  DedicatedMoveEffectState userEffects{};
  DedicatedMoveEffectState targetEffects{};
  Gen4BattlerEffectState userGen4{};
  MoveChoiceContext context{};
  context.opponent = &venusaur;
  context.userVolatile = &userVolatile;
  context.opponentVolatile = &targetVolatile;
  context.userMoveEffects = &userEffects;
  context.opponentMoveEffects = &targetEffects;
  context.userGen4Effects = &userGen4;

  // RECOVER must never compete at full HP, regardless of tie-breaking seed.
  for (uint32_t seed = 0U; seed < 256U; ++seed) {
    context.randomValue = seed;
    const MoveChoice choice = chooseMove(charizard, context);
    assert(choice.slot == 1U);
    assert(!choice.useZMove);
  }

  // In the red, recovery is deliberately preferable to an ordinary attack.
  charizard.currentHp = 20U;
  assert(chooseMove(charizard, context).slot == 0U);

  // Heal Block makes recovery contextually illegal even at critical HP.
  userGen4.healBlockTurns = 2U;
  assert(chooseMove(charizard, context).slot == 1U);
  userGen4.healBlockTurns = 0U;

  // Setup is useful while healthy, but a stat already at +6 removes it from
  // consideration instead of repeatedly burning turns.
  charizard.currentHp = charizard.maximumHp;
  charizard.moves[0] = static_cast<MoveId>(14U);  // Swords Dance.
  charizard.moves[1] = static_cast<MoveId>(183U); // Mach Punch.
  charizard.moves[2] = MoveId::None;
  charizard.movePp[2] = 0U;
  assert(chooseMove(charizard, context).slot == 0U);
  userVolatile.attackStage = kMaximumBattleStatStage;
  assert(chooseMove(charizard, context).slot == 1U);
  userVolatile = CombatVolatile{};

  // A major status move cannot target an already-statused opponent.
  charizard.moves[0] = static_cast<MoveId>(92U);  // Toxic.
  charizard.moves[1] = static_cast<MoveId>(53U);
  venusaur.status = StatusCondition::Burn;
  assert(chooseMove(charizard, context).slot == 1U);
  venusaur.status = StatusCondition::None;

  // The canonical type table drives matchup selection; this does not depend
  // on a species-specific switch in the bot.
  OwnedPokemon gyarados = battlePokemon(
      130U, 100U, 100U, static_cast<MoveId>(85U),
      static_cast<MoveId>(216U));
  OwnedPokemon blastoise = battlePokemon(
      9U, 100U, 100U, static_cast<MoveId>(57U),
      static_cast<MoveId>(58U));
  context.opponent = &blastoise;
  assert(chooseMove(gyarados, context).slot == 0U);  // Thunderbolt.

  // Pivot attacks require a living recipient; without one the evaluator
  // chooses a normal attack and cannot deadlock on a bogus replacement.
  charizard.moves[0] = static_cast<MoveId>(369U);  // U-turn.
  charizard.moves[1] = static_cast<MoveId>(53U);
  context.opponent = &venusaur;
  context.healthyReserves = 0U;
  assert(chooseMove(charizard, context).slot == 1U);

  // Z-Moves are attached only if their base move independently wins the
  // decision; they are no longer fired blindly on the first available turn.
  charizard.moves[0] = static_cast<MoveId>(105U);
  charizard.moves[1] = static_cast<MoveId>(53U);
  context.zMoveAvailable = true;
  context.zMoveSlot = 0U;
  MoveChoice choice = chooseMove(charizard, context);
  assert(choice.slot == 1U && !choice.useZMove);
  context.zMoveSlot = 1U;
  choice = chooseMove(charizard, context);
  assert(choice.slot == 1U && choice.useZMove);

  // The bot receives the same finite PvP Bag as a human. Healthy battlers do
  // not spend anything, while critical HP makes a potion a valid action.
  OwnedPokemon itemTeam[kTeamSize]{charizard, venusaur, blastoise};
  CombatVolatile itemVolatiles[kTeamSize]{};
  BattleBag bag{};
  assert(bag.hyperPotions == 3U);
  assert(bag.fullHeals == 4U);
  assert(bag.maxRevives == 1U);
  assert(bag.maxElixirs == 1U);
  ItemChoiceContext itemContext{};
  itemContext.team = itemTeam;
  itemContext.volatiles = itemVolatiles;
  itemContext.bag = &bag;
  itemContext.teamCount = kTeamSize;
  itemContext.activeSlot = 0U;
  itemTeam[0].currentHp = itemTeam[0].maximumHp;
  assert(!chooseItem(itemContext).valid);
  itemTeam[0].currentHp = 20U;
  ItemChoice itemChoice = chooseItem(itemContext);
  assert(itemChoice.valid && itemChoice.item == BattleItem::HyperPotion);
  assert(itemChoice.targetSlot == 0U);

  // Inventory is real, not infinite, and a failed/empty stock cannot produce
  // an action. Consumption is deliberately separate from evaluation.
  bag.hyperPotions = 0U;
  assert(!chooseItem(itemContext).valid);
  bag.hyperPotions = 2U;
  assert(consumeItem(bag, BattleItem::HyperPotion));
  assert(bag.hyperPotions == 1U);
  assert(consumeItem(bag, BattleItem::HyperPotion));
  assert(!consumeItem(bag, BattleItem::HyperPotion));

  // Sleep/freeze and confusion justify Full Heal, but Embargo on the active
  // battler blocks every active-target Bag action just like BattleEngine.
  bag = BattleBag{};
  itemTeam[0].currentHp = itemTeam[0].maximumHp;
  itemTeam[0].status = StatusCondition::Sleep;
  itemChoice = chooseItem(itemContext);
  assert(itemChoice.valid && itemChoice.item == BattleItem::FullHeal);
  itemTeam[0].status = StatusCondition::None;
  itemVolatiles[0].confusionTurns = 3U;
  itemChoice = chooseItem(itemContext);
  assert(itemChoice.valid && itemChoice.item == BattleItem::FullHeal);
  itemVolatiles[0].heldItemSuppressed = 0x06U;  // Three Embargo turns.
  assert(!chooseItem(itemContext).valid);
  itemVolatiles[0] = CombatVolatile{};

  // Max Revive is aimed at the actual fainted bench slot. It never targets a
  // healthy member or the living active Pokemon.
  itemTeam[1].currentHp = 0U;
  itemChoice = chooseItem(itemContext);
  assert(itemChoice.valid && itemChoice.item == BattleItem::MaxRevive);
  assert(itemChoice.targetSlot == 1U);
  itemTeam[1].currentHp = itemTeam[1].maximumHp;

  // Max Elixir is reserved for severe PP exhaustion and wins decisively when
  // the active battler otherwise has no usable move. Slot 4 is the protocol's
  // explicit Struggle command, never an exhausted authored slot.
  for (uint8_t slot = 0U; slot < kMoveSlots; ++slot)
    itemTeam[0].movePp[slot] = 0U;
  const MoveChoice struggleChoice = chooseMove(itemTeam[0], context);
  assert(struggleChoice.slot == kMoveSlots && !struggleChoice.useZMove);
  itemChoice = chooseItem(itemContext);
  assert(itemChoice.valid && itemChoice.item == BattleItem::MaxElixir);
  assert(itemChoice.targetSlot == 0U && itemChoice.score == 600);
  itemTeam[0] = charizard;
  itemTeam[0].currentHp = itemTeam[0].maximumHp;
  assert(!chooseItem(itemContext).valid);
  return 0;
}
