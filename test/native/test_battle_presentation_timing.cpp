#include <cassert>

#include "game/BattlePresentationOrder.h"
#include "game/BattlePresentationTiming.h"

int main() {
  using namespace BattlePresentationTiming;

  assert(normalizeSpeed(0U) == 1U);
  assert(normalizeSpeed(1U) == 1U);
  assert(normalizeSpeed(2U) == 2U);
  assert(normalizeSpeed(3U) == 3U);
  assert(normalizeSpeed(9U) == 3U);

  assert(scaledDurationMs(0U, 3U) == 0U);
  assert(scaledDurationMs(1200U, 1U) == 1200U);
  assert(scaledDurationMs(1200U, 2U) == 600U);
  assert(scaledDurationMs(1200U, 3U) == 400U);
  assert(scaledDurationMs(1U, 3U) == 1U);

  assert(mobileSampleStride(1U) == 1U);
  assert(mobileSampleStride(2U) == 2U);
  assert(mobileSampleStride(3U) == 3U);
  assert(panelSampleStride(1U) == 4U);
  assert(panelSampleStride(2U) == 8U);
  assert(panelSampleStride(3U) == 12U);
  assert(panelSampleStride(9U) == 12U);
  assert(panelSampleStride(2U, 0U) == 2U);
  assert(commandBoundaryRequiresFrame(1U, false, false, false, false));
  assert(!commandBoundaryRequiresFrame(2U, false, false, false, false));
  assert(!commandBoundaryRequiresFrame(3U, false, false, false, false));
  assert(commandBoundaryRequiresFrame(2U, true, false, false, false));
  assert(commandBoundaryRequiresFrame(2U, false, true, false, false));
  assert(commandBoundaryRequiresFrame(3U, false, false, true, false));
  // A short impact cannot disappear merely because its authored spawn fell
  // between two physical LCD samples in a faster presentation mode.
  assert(commandBoundaryRequiresFrame(2U, false, false, false, true));
  assert(commandBoundaryRequiresFrame(3U, false, false, false, true));
  assert(sourceTickDeadlineMs(60U, 1U) == 1000U);
  assert(sourceTickDeadlineMs(60U, 2U) == 500U);
  assert(sourceTickDeadlineMs(60U, 3U) == 334U);
  assert(sourceTickDeadlineMs(3U, 3U) == 17U);

  // The level-up presenter must bind to the completed KO that precedes the
  // level event. A later HP event (for example the next trainer Pokemon's
  // weather damage) must never satisfy or move this barrier.
  BattleActionResult journal{};
  journal.eventCount = 6U;
  journal.events[0].type = BattleEventType::MoveUsed;
  journal.events[1].type = BattleEventType::HpChanged;
  journal.events[1].side = BattleSide::Opponent;
  journal.events[1].pokemonUid = 42U;
  journal.events[1].before = 17U;
  journal.events[1].after = 0U;
  journal.events[2].type = BattleEventType::Fainted;
  journal.events[2].side = BattleSide::Opponent;
  journal.events[2].pokemonUid = 42U;
  journal.events[3].type = BattleEventType::ExperienceGained;
  journal.events[4].type = BattleEventType::LevelUp;
  journal.events[5].type = BattleEventType::HpChanged;
  journal.events[5].side = BattleSide::Opponent;
  journal.events[5].pokemonUid = 99U;
  journal.events[5].before = 30U;
  journal.events[5].after = 20U;
  const BattlePresentationOrder::KnockoutBarrier barrier =
      BattlePresentationOrder::knockoutBeforeLevelUp(journal, 4U, 42U);
  assert(barrier.complete());
  assert(barrier.hpEvent == 1 && barrier.faintEvent == 2);
  assert(!BattlePresentationOrder::knockoutBeforeLevelUp(
      journal, 1U, 42U).hasFaint());
  assert(!BattlePresentationOrder::knockoutBeforeLevelUp(
      journal, 4U, 99U).hasFaint());

  // Silent HP journal entries in either direction require their own physical
  // frame before presentation advances. Healing used to miss this boundary
  // and could be visually overtaken by the next message or move animation.
  BattleEvent healing{};
  healing.type = BattleEventType::HpChanged;
  healing.side = BattleSide::Player;
  healing.pokemonUid = 7U;
  healing.before = 19U;
  healing.after = 71U;
  const BattlePresentationOrder::HpFrameBarrier healingBarrier =
      BattlePresentationOrder::hpFrameBarrier(healing);
  assert(healingBarrier.required());
  assert(healingBarrier.rising());
  assert(healingBarrier.finalHp == 71U);

  BattleEvent damage = healing;
  damage.before = 71U;
  damage.after = 12U;
  const BattlePresentationOrder::HpFrameBarrier damageBarrier =
      BattlePresentationOrder::hpFrameBarrier(damage);
  assert(damageBarrier.required());
  assert(!damageBarrier.rising());
  assert(damageBarrier.finalHp == 12U);

  BattleEvent unchanged = healing;
  unchanged.after = unchanged.before;
  assert(!BattlePresentationOrder::hpFrameBarrier(unchanged).required());
  BattleEvent unrelated = healing;
  unrelated.type = BattleEventType::MoveUsed;
  assert(!BattlePresentationOrder::hpFrameBarrier(unrelated).required());

  // Hardware reproduction R151: Dragon Claw's authored HP event was present
  // immediately before Goldeen's Fainted event, but a transient presentation
  // identity mismatch consumed it without ever starting HP_ANIM. A live
  // pointer remains the preferred identity; staged and arena UIDs recover a
  // lost lookup, and the immutable HP(->0) -> Fainted pair is the final safe
  // recovery. Reserve-party events match none of those and stay hidden.
  BattleActionResult knockoutJournal{};
  knockoutJournal.eventCount = 3U;
  knockoutJournal.events[0].type = BattleEventType::MoveUsed;
  knockoutJournal.events[1].type = BattleEventType::HpChanged;
  knockoutJournal.events[1].side = BattleSide::Opponent;
  knockoutJournal.events[1].pokemonUid = 118042U;
  knockoutJournal.events[1].before = 138U;
  knockoutJournal.events[1].after = 0U;
  knockoutJournal.events[2].type = BattleEventType::Fainted;
  knockoutJournal.events[2].side = BattleSide::Opponent;
  knockoutJournal.events[2].pokemonUid = 118042U;
  assert(BattlePresentationOrder::hpEventFollowedByMatchingFaint(
      knockoutJournal, 1U));
  const BattleEvent& knockoutHp = knockoutJournal.events[1];
  using HpTargetMatch = BattlePresentationOrder::HpTargetMatch;
  assert(BattlePresentationOrder::hpTargetMatch(
      knockoutHp, true, 118042U, 118042U, 118042U, true) ==
      HpTargetMatch::DisplayedPokemon);
  assert(BattlePresentationOrder::hpTargetMatch(
      knockoutHp, false, 0U, 118042U, 0U, true) ==
      HpTargetMatch::FlowIdentity);
  assert(BattlePresentationOrder::hpTargetMatch(
      knockoutHp, false, 0U, 0U, 118042U, true) ==
      HpTargetMatch::ArenaIdentity);
  assert(BattlePresentationOrder::hpTargetMatch(
      knockoutHp, false, 0U, 0U, 0U, true) ==
      HpTargetMatch::KnockoutJournal);
  assert(BattlePresentationOrder::hpTargetMatch(
      knockoutHp, false, 0U, 0U, 0U, false) == HpTargetMatch::None);
  BattleEvent reserveHp = knockoutHp;
  reserveHp.pokemonUid = 777U;
  assert(BattlePresentationOrder::hpTargetMatch(
      reserveHp, true, 118042U, 118042U, 118042U, false) ==
      HpTargetMatch::None);
  BattleEvent legacyHp = knockoutHp;
  legacyHp.pokemonUid = 0U;
  assert(BattlePresentationOrder::hpTargetMatch(
      legacyHp, true, 0U, 0U, 0U, false) ==
      HpTargetMatch::DisplayedPokemon);
  knockoutJournal.events[2].pokemonUid = 999U;
  assert(!BattlePresentationOrder::hpEventFollowedByMatchingFaint(
      knockoutJournal, 1U));

  // Hardware reproduction R152: taps buffered during Roar of Time reached
  // loop() together with the deferred HP event. The old scheduler processed
  // Advance before ShowEvent and silently consumed HP -> 0. Every deferred
  // presentation stage must therefore own its loop boundary before another
  // journal advance can be accepted.
  assert(!BattlePresentationOrder::journalAdvanceBlocked(
      false, false, false, false, false));
  assert(BattlePresentationOrder::journalAdvanceBlocked(
      true, false, false, false, false));
  assert(BattlePresentationOrder::journalAdvanceBlocked(
      false, true, false, false, false));
  assert(BattlePresentationOrder::journalAdvanceBlocked(
      false, false, true, false, false));
  assert(BattlePresentationOrder::journalAdvanceBlocked(
      false, false, false, true, false));
  assert(BattlePresentationOrder::journalAdvanceBlocked(
      false, false, false, false, true));
  assert(BattlePresentationOrder::journalAdvanceBlocked(
      false, true, true, true, true));

  // A presentation retry resumes from the last physically committed sample.
  // Values outside this event's interval belong to another event and must not
  // skip or reverse any part of the authored transition.
  assert(BattlePresentationOrder::resumeHpValue(71, 12, 48) == 48);
  assert(BattlePresentationOrder::resumeHpValue(71, 12, 12) == 12);
  assert(BattlePresentationOrder::resumeHpValue(71, 12, -1) == 71);
  assert(BattlePresentationOrder::resumeHpValue(71, 12, 90) == 71);
  assert(BattlePresentationOrder::resumeHpValue(19, 71, 43) == 43);
  assert(BattlePresentationOrder::resumeHpValue(19, 71, 4) == 19);

  // Model a renderer that accepts one sample, defers two, then recovers. A
  // cancelled frame can never move the logical meter ahead of the LCD.
  BattlePresentationOrder::PresentedHpFrame delivered{100, 48};
  delivered = BattlePresentationOrder::resolveHpFrameDelivery(
      delivered, {90, 43}, true);
  assert(delivered.hp == 90 && delivered.pixels == 43);
  delivered = BattlePresentationOrder::resolveHpFrameDelivery(
      delivered, {80, 38}, false);
  assert(delivered.hp == 90 && delivered.pixels == 43);
  delivered = BattlePresentationOrder::resolveHpFrameDelivery(
      delivered, {70, 34}, false);
  assert(delivered.hp == 90 && delivered.pixels == 43);
  delivered = BattlePresentationOrder::resolveHpFrameDelivery(
      delivered, {60, 29}, true);
  assert(delivered.hp == 60 && delivered.pixels == 29);

  // A completed composition that the tile deduplicator reduced to zero panel
  // writes cannot consume a visibly different HP sample. Equal visual samples
  // (for example two source HP ticks mapping to the same foe-bar pixel) remain
  // valid no-ops, while an actual transfer commits the changed frame.
  assert(!BattlePresentationOrder::hpFramePhysicallyCommitted(
      true, true, 0U));
  assert(BattlePresentationOrder::hpFramePhysicallyCommitted(
      true, false, 0U));
  assert(BattlePresentationOrder::hpFramePhysicallyCommitted(
      true, true, 128U));
  assert(!BattlePresentationOrder::hpFramePhysicallyCommitted(
      false, false, 128U));

  // Boss events expose only the signed change of the current uint16 HP layer.
  // Reconstructing their absolute target from the turn snapshot prevents a
  // retry after one committed frame from applying the same damage twice.
  BattleActionResult bossJournal{};
  bossJournal.eventCount = 4U;
  bossJournal.events[0].type = BattleEventType::MoveUsed;
  bossJournal.events[1].type = BattleEventType::HpChanged;
  bossJournal.events[1].side = BattleSide::Opponent;
  bossJournal.events[1].pokemonUid = 900U;
  bossJournal.events[1].before = 65535U;
  bossJournal.events[1].after = 65000U;
  bossJournal.events[2].type = BattleEventType::MoveEffect;
  bossJournal.events[3] = bossJournal.events[1];
  bossJournal.events[3].before = 65000U;
  bossJournal.events[3].after = 64200U;
  const BattlePresentationOrder::AccumulatedHpTransition firstBossHit =
      BattlePresentationOrder::accumulatedHpTransition(
          200000U, bossJournal, 1U, BattleSide::Opponent, 900U, 200000U);
  assert(firstBossHit.found);
  assert(firstBossHit.before == 200000U);
  assert(firstBossHit.after == 199465U);
  const BattlePresentationOrder::AccumulatedHpTransition secondBossHit =
      BattlePresentationOrder::accumulatedHpTransition(
          200000U, bossJournal, 3U, BattleSide::Opponent, 900U, 200000U);
  assert(secondBossHit.found);
  assert(secondBossHit.before == 199465U);
  assert(secondBossHit.after == 198665U);
  assert(BattlePresentationOrder::resumeHpValue(
      static_cast<int32_t>(secondBossHit.before),
      static_cast<int32_t>(secondBossHit.after), 199100) == 199100);

  return 0;
}
