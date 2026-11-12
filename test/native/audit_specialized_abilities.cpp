#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "game/BattleEngine.h"
#include "game/Collection.h"
#include "game/PokemonData.h"

namespace {
constexpr MoveId move(uint16_t id) { return static_cast<MoveId>(id); }
constexpr MoveId kSplash = static_cast<MoveId>(150U);

uint8_t abilityNamed(const char* name) {
  for (uint16_t id = 1U; id <= 255U; ++id) {
    const AbilityData* ability = findAbility(static_cast<uint8_t>(id));
    if (ability && std::strcmp(ability->name, name) == 0)
      return static_cast<uint8_t>(id);
  }
  return 0U;
}

struct Audit {
  uint16_t passed = 0U;
  uint16_t failed = 0U;
  void check(const char* ability, const char* scenario, bool condition,
             const std::string& detail = {}) {
    std::cout << (condition ? "PASS" : "FAIL") << ',' << ability << ','
              << scenario;
    if (!detail.empty()) std::cout << ',' << detail;
    std::cout << '\n';
    condition ? ++passed : ++failed;
  }
};

bool saw(const BattleActionResult& result, BattleEventType type,
         BattleSide side, uint16_t value = UINT16_MAX) {
  for (uint8_t index = 0U; index < result.eventCount; ++index) {
    const BattleEvent& event = result.events[index];
    if (event.type == type && event.side == side &&
        (value == UINT16_MAX || event.value == value)) return true;
  }
  return false;
}

BattleSide firstMoveSide(const BattleActionResult& result) {
  for (uint8_t index = 0U; index < result.eventCount; ++index)
    if (result.events[index].type == BattleEventType::MoveUsed)
      return result.events[index].side;
  return static_cast<BattleSide>(0xFFU);
}

struct Fixture {
  PokemonCollection collection{};
  BattleState battle{};

  Fixture(uint16_t playerSpecies = 137U, uint16_t opponentSpecies = 137U,
          MoveId playerMove = kSplash, MoveId opponentMove = kSplash,
          uint32_t seed = 0x43300001U) {
    CollectionLogic::initialize(collection);
    CollectionLogic::chooseStarter(collection, 1U, 0x43300002U);
    OwnedPokemon* owned = CollectionLogic::active(collection, 0U);
    const uint32_t uid = owned->uid;
    *owned = CollectionLogic::createPokemon(uid, playerSpecies, 50U, false,
                                             0x43300003U, 7U);
    setMove(*owned, playerMove);

    BattleEngine::clear(battle);
    battle.active = true;
    battle.kind = BattleKind::Pvp;
    battle.outcome = BattleOutcome::Ongoing;
    battle.playerUid = uid;
    battle.opponentCount = 1U;
    battle.unlockedGeneration = 7U;
    battle.rngState = seed;
    battle.opponents[0] = CollectionLogic::createPokemon(
        0xF1433001U, opponentSpecies, 50U, false, 0x43300004U, 7U);
    setMove(battle.opponents[0], opponentMove);
  }

  static void setMove(OwnedPokemon& pokemon, MoveId selected) {
    for (uint8_t slot = 0U; slot < kMoveSlots; ++slot) {
      pokemon.moves[slot] = MoveId::None;
      pokemon.movePp[slot] = 0U;
    }
    pokemon.moves[0] = selected;
    const FullMoveData* data = findFullMove(selected);
    pokemon.movePp[0] = data ? data->pp : 20U;
  }

  OwnedPokemon& player() {
    return *CollectionLogic::find(collection, battle.playerUid);
  }
  OwnedPokemon& opponent() { return battle.opponents[battle.opponentIndex]; }
  BattleActionResult act(bool opponentActs = false) {
    return BattleEngine::fightPvp(battle, collection, 0U,
                                  opponentActs ? 0U : 0xFEU);
  }
  uint32_t addBench(uint16_t species = 25U) {
    OwnedPokemon pokemon = CollectionLogic::createPokemon(
        0U, species, 50U, false, 0x43300100U + species, 7U);
    uint32_t uid = 0U;
    CollectionLogic::add(collection, pokemon, &uid);
    CollectionLogic::setPartySlot(collection, 1U, uid);
    return uid;
  }
};

struct DamageSetup {
  uint16_t attackerSpecies = 137U;
  uint16_t defenderSpecies = 137U;
  uint8_t attackerAbility = 0U;
  uint8_t defenderAbility = 0U;
  StatusCondition attackerStatus = StatusCondition::None;
  StatusCondition defenderStatus = StatusCondition::None;
  HeldItem attackerItem = HeldItem::None;
  HeldItem defenderItem = HeldItem::None;
  BattleWeather weather = BattleWeather::Clear;
  Gen6Terrain terrain = Gen6Terrain::None;
  uint16_t attackerHpPercent = 100U;
  int8_t attackerAttackStage = 0;
  int8_t attackerSpAttackStage = 0;
  int8_t defenderDefenseStage = 0;
  int8_t defenderSpDefenseStage = 0;
  uint16_t enteredTurn = 0U;
};

uint16_t damageFor(MoveId selected, const DamageSetup& setup,
                   bool* critical = nullptr) {
  Fixture fixture(setup.attackerSpecies, setup.defenderSpecies, selected);
  OwnedPokemon& attacker = fixture.player();
  OwnedPokemon& defender = fixture.opponent();
  attacker.abilityId = setup.attackerAbility;
  defender.abilityId = setup.defenderAbility;
  attacker.status = setup.attackerStatus;
  defender.status = setup.defenderStatus;
  attacker.heldItem = setup.attackerItem;
  defender.heldItem = setup.defenderItem;
  attacker.currentHp = static_cast<uint16_t>(std::max<uint32_t>(
      1U, static_cast<uint32_t>(attacker.maximumHp) * setup.attackerHpPercent /
              100U));
  fixture.battle.weather = setup.weather;
  fixture.battle.weatherTurns = setup.weather == BattleWeather::Clear ? 0U : 0xFFU;
  fixture.battle.gen6Terrain = setup.terrain;
  fixture.battle.turn = 7U;
  fixture.battle.opponentMoveEffects[0].enteredTurn = setup.enteredTurn;
  fixture.battle.playerVolatile.attackStage = setup.attackerAttackStage;
  fixture.battle.playerVolatile.spAttackStage = setup.attackerSpAttackStage;
  fixture.battle.opponentVolatiles[0].defenseStage = setup.defenderDefenseStage;
  fixture.battle.opponentVolatiles[0].spDefenseStage = setup.defenderSpDefenseStage;
  fixture.battle.rngState = 0x4330DA6EU;
  uint16_t effectiveness = 0U;
  bool wasCritical = false;
  const uint16_t result = BattleEngine::calculateDamage(
      fixture.battle, attacker, defender, selected, fixture.battle.playerVolatile,
      fixture.battle.opponentVolatiles[0], &effectiveness, &wasCritical, true);
  if (critical) *critical = wasCritical;
  return result;
}

std::string values(uint32_t a, uint32_t b = UINT32_MAX,
                   uint32_t c = UINT32_MAX) {
  std::string value = "a=" + std::to_string(a);
  if (b != UINT32_MAX) value += ":b=" + std::to_string(b);
  if (c != UINT32_MAX) value += ":c=" + std::to_string(c);
  return value;
}

void auditForms(Audit& audit) {
  {
    Fixture f(746U);
    f.player().abilityId = abilityNamed("SCHOOLING");
    f.player().level = 50U;
    f.player().currentHp = f.player().maximumHp;
    BattleActionResult result{};
    BattleEngine::applyEntryAbilities(f.battle, f.collection,
                                      BattleEntryScope::Player, &result);
    const bool school = CollectionLogic::resolvedForm(f.player()) == 1U;
    f.player().currentHp = std::max<uint16_t>(1U, f.player().maximumHp / 4U);
    f.act();
    audit.check("SCHOOLING", "school-above-quarter-and-solo-at-quarter",
                school && CollectionLogic::resolvedForm(f.player()) == 0U);
  }
  {
    Fixture f(774U);
    f.player().abilityId = abilityNamed("SHIELDS DOWN");
    f.player().form = 0U;
    f.player().currentHp = f.player().maximumHp;
    BattleActionResult entry{};
    BattleEngine::applyEntryAbilities(f.battle, f.collection,
                                      BattleEntryScope::Player, &entry);
    const bool meteor = CollectionLogic::resolvedForm(f.player()) == 0U;
    f.player().currentHp = std::max<uint16_t>(1U, f.player().maximumHp / 2U);
    f.act();
    const bool core = CollectionLogic::resolvedForm(f.player()) == 7U;
    Fixture status(774U, 137U, kSplash, move(86U));
    status.player().abilityId = abilityNamed("SHIELDS DOWN");
    status.player().form = 0U;
    status.act(true);
    audit.check("SHIELDS DOWN", "meteor-core-and-meteor-status-immunity",
                meteor && core && status.player().status == StatusCondition::None);
  }
  {
    Fixture f(778U, 137U, kSplash, move(247U));
    f.player().abilityId = abilityNamed("DISGUISE");
    const uint16_t hp = f.player().currentHp;
    BattleActionResult entry{};
    BattleEngine::applyEntryAbilities(f.battle, f.collection,
                                      BattleEntryScope::Player, &entry);
    const BattleActionResult result = f.act(true);
    const bool activated = saw(result, BattleEventType::AbilityActivated,
                               BattleSide::Player);
    audit.check("DISGUISE", "first-hit-absorbed-and-busted",
                f.player().currentHp == hp && f.player().form == 1U && activated,
                values(hp, f.player().currentHp,
                       static_cast<uint32_t>(f.player().form) * 10U + activated));
  }
  {
    Fixture f(571U, 137U, kSplash, move(33U));
    f.player().abilityId = abilityNamed("ILLUSION");
    f.addBench(6U);
    BattleActionResult entry{};
    BattleEngine::applyEntryAbilities(f.battle, f.collection,
                                      BattleEntryScope::Player, &entry);
    const bool disguised = BattleEngine::displayedSpeciesId(f.battle, f.player()) == 6U;
    f.act(true);
    audit.check("ILLUSION", "last-party-disguise-breaks-on-damage",
                disguised && BattleEngine::displayedSpeciesId(
                                 f.battle, f.player()) == 571U);
  }
  {
    Fixture f(718U);
    f.player().abilityId = abilityNamed("POWER CONSTRUCT");
    f.player().form = 1U;
    CollectionLogic::refreshDerivedStats(f.player(), true);
    f.player().currentHp = static_cast<uint16_t>(f.player().maximumHp * 8U / 10U);
    f.act();
    const bool fifty = CollectionLogic::resolvedForm(f.player()) == 0U;
    f.player().currentHp = static_cast<uint16_t>(f.player().maximumHp * 4U / 10U);
    f.act();
    audit.check("POWER CONSTRUCT", "ten-to-fifty-to-complete-one-stage-per-turn",
                fifty && CollectionLogic::resolvedForm(f.player()) == 2U);
  }
}

void auditCadenceAndOrder(Audit& audit) {
  {
    Fixture f;
    f.player().abilityId = abilityNamed("HYDRATION");
    f.player().status = StatusCondition::Poison;
    f.battle.weather = BattleWeather::Rain;
    f.battle.weatherTurns = 0xFFU;
    f.act();
    audit.check("HYDRATION", "rain-cures-major-status-at-end-turn",
                f.player().status == StatusCondition::None);
  }
  {
    auto run = [](bool full) {
      Fixture f(213U, 291U, move(16U), move(33U));
      f.player().abilityId = abilityNamed("GALE WINGS");
      if (!full) f.player().currentHp = std::max<uint16_t>(1U, f.player().maximumHp - 1U);
      return firstMoveSide(f.act(true));
    };
    const BattleSide full = run(true), damaged = run(false);
    audit.check("GALE WINGS", "gen7-priority-only-at-full-hp",
                full == BattleSide::Player && damaged == BattleSide::Opponent,
                values(static_cast<uint8_t>(full), static_cast<uint8_t>(damaged)));
  }
  const struct SpeedCase {
    const char* name;
    BattleWeather weather;
    Gen6Terrain terrain;
    StatusCondition status;
  } speedCases[] = {
      {"SWIFT SWIM", BattleWeather::Rain, Gen6Terrain::None, StatusCondition::None},
      {"CHLOROPHYLL", BattleWeather::Sun, Gen6Terrain::None, StatusCondition::None},
      {"SAND RUSH", BattleWeather::Sandstorm, Gen6Terrain::None, StatusCondition::None},
      {"SLUSH RUSH", BattleWeather::Hail, Gen6Terrain::None, StatusCondition::None},
      {"SURGE SURFER", BattleWeather::Clear, Gen6Terrain::Electric, StatusCondition::None},
      {"QUICK FEET", BattleWeather::Clear, Gen6Terrain::None, StatusCondition::Poison},
  };
  for (const SpeedCase& test : speedCases) {
    Fixture f(1U, 4U, move(33U), move(33U));
    f.player().abilityId = abilityNamed(test.name);
    f.player().status = test.status;
    f.battle.weather = test.weather;
    f.battle.weatherTurns = test.weather == BattleWeather::Clear ? 0U : 0xFFU;
    f.battle.gen6Terrain = test.terrain;
    const BattleActionResult result = f.act(true);
    audit.check(test.name, "conditional-speed-changes-turn-order",
                firstMoveSide(result) == BattleSide::Player);
  }
  {
    uint16_t misses = 0U;
    for (uint32_t seed = 1U; seed <= 128U; ++seed) {
      Fixture f(137U, 137U, move(33U), kSplash, seed);
      f.opponent().abilityId = abilityNamed("TANGLED FEET");
      f.battle.opponentVolatiles[0].confusionTurns = 4U;
      const BattleActionResult result = f.act(false);
      misses += saw(result, BattleEventType::MoveMissed, BattleSide::Player);
    }
    audit.check("TANGLED FEET", "confusion-halves-accuracy", misses > 0U,
                values(misses));
  }
  {
    Fixture f(213U, 291U, move(105U), move(98U));
    f.player().abilityId = abilityNamed("TRIAGE");
    f.player().currentHp = std::max<uint16_t>(1U, f.player().maximumHp / 2U);
    audit.check("TRIAGE", "healing-move-plus-three-priority",
                firstMoveSide(f.act(true)) == BattleSide::Player);
  }
}

void auditDamage(Audit& audit) {
  const auto id = [](const char* name) { return abilityNamed(name); };
  const auto compare = [&](const char* name, MoveId selected, DamageSetup base,
                           bool attacker, bool expectHigher) {
    const uint16_t ordinary = damageFor(selected, base);
    if (attacker) base.attackerAbility = id(name);
    else base.defenderAbility = id(name);
    const uint16_t changed = damageFor(selected, base);
    audit.check(name, expectHigher ? "damage-boost" : "damage-reduction",
                expectHigher ? changed > ordinary : changed < ordinary,
                values(ordinary, changed));
  };

  {
    DamageSetup aura{};
    aura.attackerAbility = id("FAIRY AURA");
    const uint16_t boosted = damageFor(move(583U), aura);
    DamageSetup plain{};
    const uint16_t ordinary = damageFor(move(583U), plain);
    aura.defenderAbility = id("AURA BREAK");
    const uint16_t reversed = damageFor(move(583U), aura);
    audit.check("FAIRY AURA", "fairy-damage-aura", boosted > ordinary,
                values(ordinary, boosted));
    audit.check("AURA BREAK", "reverses-fairy-aura", reversed < ordinary,
                values(ordinary, boosted, reversed));
  }
  {
    DamageSetup s{}; s.defenderAbility = id("BULLETPROOF");
    audit.check("BULLETPROOF", "aura-sphere-immunity",
                damageFor(move(396U), s) == 0U);
  }
  {
    DamageSetup s{}; s.defenderSpecies = 74U;
    compare("FILTER", move(55U), s, false, false);
    compare("SOLID ROCK", move(55U), s, false, false);
  }
  {
    DamageSetup s{}; s.attackerStatus = StatusCondition::Burn;
    compare("FLARE BOOST", move(94U), s, true, true);
  }
  {
    DamageSetup s{}; s.terrain = Gen6Terrain::Grassy;
    compare("GRASS PELT", move(33U), s, false, false);
  }
  for (const char* name : {"HEAVY METAL", "LIGHT METAL"}) {
    bool observed = false;
    uint16_t ordinary = 0U, changed = 0U;
    for (uint16_t species = 1U; species <= 807U && !observed; ++species) {
      if (!findSpecies(species)) continue;
      DamageSetup base{}; base.defenderSpecies = species;
      ordinary = damageFor(move(67U), base);
      base.defenderAbility = id(name);
      changed = damageFor(move(67U), base);
      observed = name[0] == 'H' ? changed > ordinary : changed < ordinary;
    }
    audit.check(name, "weight-changes-low-kick-power", observed,
                values(ordinary, changed));
  }
  {
    DamageSetup s{}; s.defenderSpecies = 74U;
    compare("LIQUID VOICE", move(304U), s, true, true);
  }
  {
    auto run = [&](uint8_t ability) {
      Fixture f(137U, 137U, move(33U));
      f.player().abilityId = ability;
      f.opponent().abilityId = abilityNamed("ROUGH SKIN");
      const uint16_t before = f.player().currentHp;
      f.act();
      return static_cast<uint16_t>(before - f.player().currentHp);
    };
    const uint16_t contact = run(0U), remote = run(id("LONG REACH"));
    audit.check("LONG REACH", "suppresses-defender-contact-effects",
                contact > remote, values(contact, remote));
  }
  {
    DamageSetup s{}; s.defenderStatus = StatusCondition::Poison;
    compare("MARVEL SCALE", move(33U), s, false, false);
  }
  {
    DamageSetup s{}; s.defenderStatus = StatusCondition::Poison;
    bool critical = false;
    s.attackerAbility = id("MERCILESS");
    damageFor(move(33U), s, &critical);
    audit.check("MERCILESS", "always-critical-vs-poison", critical);
  }
  for (const auto& test : std::vector<std::pair<const char*, MoveId>>{
           {"OVERGROW", move(22U)}, {"TORRENT", move(55U)},
           {"SWARM", move(450U)}}) {
    DamageSetup s{}; s.attackerHpPercent = 33U;
    compare(test.first, test.second, s, true, true);
  }
  {
    bool observed = false;
    uint16_t ordinary = 0U, rivalry = 0U;
    for (uint32_t a = 1U; a < 64U && !observed; ++a) {
      Fixture plain(25U, 25U, move(33U));
      plain.player().personality = a;
      plain.opponent().personality = a + 64U;
      DamageSetup s{}; s.attackerSpecies = 25U; s.defenderSpecies = 25U;
      ordinary = damageFor(move(33U), s);
      s.attackerAbility = id("RIVALRY");
      rivalry = damageFor(move(33U), s);
      observed = ordinary != rivalry;
    }
    // The generated fixture personalities are fixed; Rivalry must still
    // produce either its same-gender boost or opposite-gender penalty.
    DamageSetup s{}; s.attackerSpecies = 25U; s.defenderSpecies = 25U;
    ordinary = damageFor(move(33U), s);
    s.attackerAbility = id("RIVALRY"); rivalry = damageFor(move(33U), s);
    observed = ordinary != rivalry;
    audit.check("RIVALRY", "gender-dependent-power", observed,
                values(ordinary, rivalry));
  }
  {
    DamageSetup s{}; s.weather = BattleWeather::Sandstorm;
    compare("SAND FORCE", move(89U), s, true, true);
  }
  {
    DamageSetup s{}; s.defenderSpecies = 92U;
    const uint16_t immune = damageFor(move(33U), s);
    s.attackerAbility = id("SCRAPPY");
    const uint16_t hit = damageFor(move(33U), s);
    audit.check("SCRAPPY", "normal-hits-ghost", immune == 0U && hit > 0U,
                values(immune, hit));
  }
  {
    DamageSetup s{}; s.enteredTurn = 7U;
    compare("STAKEOUT", move(33U), s, true, true);
  }
  compare("STEELWORKER", move(430U), DamageSetup{}, true, true);
  {
    DamageSetup s{}; s.defenderSpecies = 4U;
    compare("TINTED LENS", move(22U), s, true, true);
  }
  {
    DamageSetup staged{}; staged.defenderDefenseStage = 6;
    const uint16_t reduced = damageFor(move(33U), staged);
    staged.attackerAbility = id("UNAWARE");
    const uint16_t ignoredDefense = damageFor(move(33U), staged);
    DamageSetup ordinary{};
    const uint16_t neutral = damageFor(move(33U), ordinary);
    DamageSetup attackerBoost{}; attackerBoost.attackerAttackStage = 6;
    const uint16_t boosted = damageFor(move(33U), attackerBoost);
    attackerBoost.defenderAbility = id("UNAWARE");
    const uint16_t ignoredAttack = damageFor(move(33U), attackerBoost);
    audit.check("UNAWARE", "ignores-opposing-offense-and-defense-stages",
                ignoredDefense == neutral && ignoredAttack < boosted,
                values(neutral, ignoredDefense, ignoredAttack));
  }
  {
    DamageSetup s{}; s.attackerSpecies = 773U; s.defenderSpecies = 1U;
    s.attackerAbility = id("RKS SYSTEM");
    const uint16_t ordinary = damageFor(move(672U), s);
    s.attackerItem = HeldItem::FireMemory;
    const uint16_t fire = damageFor(move(672U), s);
    audit.check("RKS SYSTEM", "memory-changes-multi-attack-type",
                fire > ordinary, values(ordinary, fire));
  }
}

void auditStatusContactAndField(Audit& audit) {
  const auto id = [](const char* name) { return abilityNamed(name); };
  {
    Fixture f(137U, 137U, kSplash, move(269U));
    f.player().abilityId = id("AROMA VEIL");
    const BattleActionResult result = f.act(true);
    audit.check("AROMA VEIL", "blocks-taunt",
                f.battle.playerMoveEffects.tauntTurns == 0U &&
                    saw(result, BattleEventType::AbilityActivated,
                        BattleSide::Player));
  }
  {
    bool baselineFlinched = false, protectedFlinched = false;
    for (uint32_t seed = 1U; seed <= 256U; ++seed) {
      Fixture base(1U, 4U, kSplash, move(44U), seed);
      const BattleActionResult a = base.act(true);
      baselineFlinched |= saw(a, BattleEventType::CannotMove, BattleSide::Player);
      Fixture guarded(1U, 4U, kSplash, move(44U), seed);
      guarded.player().abilityId = id("INNER FOCUS");
      const BattleActionResult b = guarded.act(true);
      protectedFlinched |= saw(b, BattleEventType::CannotMove, BattleSide::Player);
    }
    audit.check("INNER FOCUS", "prevents-flinch", baselineFlinched && !protectedFlinched);
  }
  {
    bool baselineFrozen = false, guardedFrozen = false;
    for (uint32_t seed = 1U; seed <= 256U; ++seed) {
      Fixture base(137U, 137U, kSplash, move(58U), seed);
      base.act(true); baselineFrozen |= base.player().status == StatusCondition::Frozen;
      Fixture guarded(137U, 137U, kSplash, move(58U), seed);
      guarded.player().abilityId = id("MAGMA ARMOR");
      guarded.act(true); guardedFrozen |= guarded.player().status == StatusCondition::Frozen;
    }
    audit.check("MAGMA ARMOR", "prevents-freeze", baselineFrozen && !guardedFrozen);
  }
  for (const auto& test : std::vector<std::pair<const char*, MoveId>>{
           {"JUSTIFIED", move(399U)}, {"RATTLED", move(450U)}}) {
    Fixture f(137U, 137U, kSplash, test.second);
    f.player().abilityId = id(test.first);
    f.act(true);
    const int8_t stage = std::strcmp(test.first, "JUSTIFIED") == 0
        ? f.battle.playerVolatile.attackStage : f.battle.playerVolatile.speedStage;
    audit.check(test.first, "reactive-stage-boost", stage == 1);
  }
  {
    Fixture f(137U, 137U, move(33U));
    f.player().heldItem = HeldItem::OranBerry;
    f.opponent().abilityId = id("PICKPOCKET");
    f.act();
    audit.check("PICKPOCKET", "steals-contact-attacker-item",
                f.battle.playerVolatile.hasHeldItemOverride &&
                    f.battle.playerVolatile.heldItemOverride == HeldItem::None &&
                    f.battle.opponentVolatiles[0].hasHeldItemOverride &&
                    f.battle.opponentVolatiles[0].heldItemOverride == HeldItem::OranBerry);
  }
  {
    Fixture f(137U, 137U, kSplash, move(33U));
    f.player().abilityId = id("WIMP OUT");
    f.addBench(25U);
    f.player().maximumHp = f.player().currentHp = 100U;
    f.opponent().level = 100U;
    f.act(true);
    audit.check("WIMP OUT", "crossing-half-requests-replacement",
                f.battle.playerGen7Effects.thresholdExitTriggered);
  }
  {
    Fixture f(137U, 137U, kSplash, move(28U));
    f.player().abilityId = id("KEEN EYE");
    f.act(true);
    audit.check("KEEN EYE", "blocks-opponent-accuracy-drop",
                f.battle.playerVolatile.accuracyStage == 0);
  }
  for (const auto& test : std::vector<std::pair<const char*, uint16_t>>{
           {"SHADOW TAG", 137U}, {"MAGNET PULL", 81U}}) {
    Fixture f(test.second, 137U);
    const uint32_t bench = f.addBench(25U);
    f.opponent().abilityId = id(test.first);
    const BattleActionResult result = BattleEngine::switchToPokemonPvp(
        f.battle, f.collection, bench, 0xFEU, false);
    audit.check(test.first, "prevents-matching-switch", !result.accepted);
  }
  {
    Fixture f;
    f.player().abilityId = id("GLUTTONY");
    f.player().heldItem = HeldItem::LiechiBerry;
    f.player().currentHp = static_cast<uint16_t>(f.player().maximumHp * 40U / 100U);
    f.act();
    audit.check("GLUTTONY", "pinch-berry-triggers-at-half",
                f.player().heldItem == HeldItem::None &&
                    f.battle.playerVolatile.attackStage == 1);
  }
  {
    Fixture f(137U, 137U, move(33U));
    f.player().abilityId = id("MAGICIAN");
    f.opponent().heldItem = HeldItem::OranBerry;
    f.act();
    audit.check("MAGICIAN", "steals-item-after-damaging-hit",
                f.battle.playerVolatile.hasHeldItemOverride &&
                    f.battle.playerVolatile.heldItemOverride == HeldItem::OranBerry &&
                    f.battle.opponentVolatiles[0].hasHeldItemOverride &&
                    f.battle.opponentVolatiles[0].heldItemOverride == HeldItem::None);
  }
  {
    Fixture f;
    f.battle.kind = BattleKind::Wild;
    f.player().abilityId = id("RUN AWAY");
    f.opponent().abilityId = id("SHADOW TAG");
    const BattleActionResult result = BattleEngine::run(f.battle, f.collection);
    audit.check("RUN AWAY", "guaranteed-escape-even-when-trapped",
                result.accepted && !f.battle.active &&
                    f.battle.outcome == BattleOutcome::Escaped);
  }
  {
    Fixture f;
    f.player().abilityId = id("UNNERVE");
    f.opponent().heldItem = HeldItem::OranBerry;
    f.opponent().currentHp = std::max<uint16_t>(1U, f.opponent().maximumHp / 4U);
    f.act();
    audit.check("UNNERVE", "opponent-cannot-eat-berry",
                f.opponent().heldItem == HeldItem::OranBerry);
  }
}
}  // namespace

int main() {
  std::cout << "status,ability,scenario,detail\n";
  Audit audit{};
  auditForms(audit);
  auditCadenceAndOrder(audit);
  auditDamage(audit);
  auditStatusContactAndField(audit);
  std::cout << "SUMMARY,passed=" << audit.passed << ",failed=" << audit.failed
            << '\n';
  return audit.failed ? 1 : 0;
}
