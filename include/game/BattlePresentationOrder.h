#pragma once

#include <algorithm>
#include <cstdint>

#include "game/BattleEngine.h"

namespace BattlePresentationOrder {

enum class HpDirection : uint8_t {
  None,
  Falling,
  Rising,
};

// The battle resolver completes a whole turn before the presenter starts.
// Most HP events can therefore identify their healthbox through the live
// displayed Pokemon, but a finished wild battle may temporarily lose that
// lookup while the already-authored journal still owns the visible battler.
// Keep the recovery sources explicit: a non-zero staged/arena UID or an
// adjacent HP -> Fainted journal pair is authoritative, while an unrelated
// reserve-party HP event remains ignored.
enum class HpTargetMatch : uint8_t {
  None,
  DisplayedPokemon,
  FlowIdentity,
  ArenaIdentity,
  KnockoutJournal,
};

inline HpTargetMatch hpTargetMatch(const BattleEvent& event,
                                   bool displayedPokemonPresent,
                                   uint32_t displayedPokemonUid,
                                   uint32_t flowUid,
                                   uint32_t arenaUid,
                                   bool matchingFaintFollows) {
  if (event.type != BattleEventType::HpChanged)
    return HpTargetMatch::None;
  // Presence makes UID zero valid for old persisted opponent records. Do not
  // let zero-valued default flow/arena fields claim an arbitrary HP event.
  if (displayedPokemonPresent && event.pokemonUid == displayedPokemonUid)
    return HpTargetMatch::DisplayedPokemon;
  if (flowUid && event.pokemonUid == flowUid)
    return HpTargetMatch::FlowIdentity;
  if (arenaUid && event.pokemonUid == arenaUid)
    return HpTargetMatch::ArenaIdentity;
  return matchingFaintFollows ? HpTargetMatch::KnockoutJournal
                              : HpTargetMatch::None;
}

inline bool hpEventFollowedByMatchingFaint(const BattleActionResult& result,
                                           uint8_t hpEventIndex) {
  if (hpEventIndex >= result.eventCount ||
      static_cast<uint16_t>(hpEventIndex) + 1U >= result.eventCount)
    return false;
  const BattleEvent& hp = result.events[hpEventIndex];
  const BattleEvent& faint = result.events[hpEventIndex + 1U];
  return hp.type == BattleEventType::HpChanged && hp.after == 0U &&
      faint.type == BattleEventType::Fainted && faint.side == hp.side &&
      faint.pokemonUid == hp.pokemonUid;
}

// Physical and serial taps can accumulate while an ESP32 move animation owns
// the loop for several seconds.  A tap received before one of these deferred
// stages is committed belongs to the animation that just ended, not to the
// next journal message.  Advancing first would consume a silent HP event
// before its meter was ever presented.
inline bool journalAdvanceBlocked(bool levelUpPanelPending,
                                  bool showEventPending,
                                  bool presentationPending,
                                  bool statusAnimationPending,
                                  bool finishPending) {
  return levelUpPanelPending || showEventPending || presentationPending ||
      statusAnimationPending || finishPending;
}

// HP events are silent journal entries. Without an explicit boundary the
// presenter can finish the last meter sample and immediately paint the next
// message/animation in the same call stack. Damage happened to hide this in
// most turns, while healing (Potion, drain, Leftovers, etc.) visibly lagged
// behind the following event. Classify both directions through one rule so
// every real transition owns a completed physical frame.
struct HpFrameBarrier {
  HpDirection direction = HpDirection::None;
  uint16_t finalHp = 0;

  bool required() const { return direction != HpDirection::None; }
  bool rising() const { return direction == HpDirection::Rising; }
};

inline HpFrameBarrier hpFrameBarrier(const BattleEvent& event) {
  HpFrameBarrier barrier{};
  if (event.type != BattleEventType::HpChanged || event.before == event.after)
    return barrier;
  barrier.direction = event.after > event.before
      ? HpDirection::Rising : HpDirection::Falling;
  barrier.finalHp = event.after;
  return barrier;
}

// A cancelled retained frame must be retried from the last value that really
// reached the panel, not from the live battle object (which already contains
// the end-of-turn result).  Accept a staged value only when it belongs to this
// exact transition; stale values from another battler/event fall back to the
// journal's authored starting point.
inline int32_t resumeHpValue(int32_t before, int32_t after,
                             int32_t lastCommitted) {
  if (lastCommitted < 0) return before;
  const int32_t lower = std::min(before, after);
  const int32_t upper = std::max(before, after);
  return lastCommitted >= lower && lastCommitted <= upper
      ? lastCommitted : before;
}

struct PresentedHpFrame {
  int32_t hp = -1;
  int16_t pixels = -1;
};

// Candidate values must be staged before composition so the painter can see
// them. Resolve the transaction afterwards: a cancelled presentation keeps
// the previous logical sample, exactly matching the unchanged physical LCD.
inline PresentedHpFrame resolveHpFrameDelivery(
    const PresentedHpFrame& previous, const PresentedHpFrame& candidate,
    bool committed) {
  return committed ? candidate : previous;
}

// Finishing a retained composition is not, by itself, proof that the LCD saw
// a new HP sample: the tile deduplicator can legitimately publish zero pixels.
// That is a successful no-op only when the rendered value did not change. A
// visible meter/number transition owns a real panel transfer before the
// journal may advance to the opponent's action.
inline bool hpFramePhysicallyCommitted(bool compositionSucceeded,
                                       bool visualChangeRequired,
                                       uint32_t transferredPixels) {
  return compositionSucceeded &&
      (!visualChangeRequired || transferredPixels != 0U);
}

struct AccumulatedHpTransition {
  uint32_t before = 0;
  uint32_t after = 0;
  bool found = false;
};

// Boss HP is wider than BattleEvent::before/after, so its journal stores the
// signed delta of each visible 65,535-point layer. Rebuild the absolute target
// from the turn snapshot and all earlier matching events. This also makes a
// partially presented event safely resumable without applying its delta twice.
inline AccumulatedHpTransition accumulatedHpTransition(
    uint32_t initialHp, const BattleActionResult& result, uint8_t eventIndex,
    BattleSide side, uint32_t pokemonUid, uint32_t maximumHp) {
  AccumulatedHpTransition transition{};
  int64_t current = static_cast<int64_t>(
      std::min<uint32_t>(initialHp, maximumHp));
  transition.before = transition.after = static_cast<uint32_t>(current);
  const uint8_t end = std::min<uint8_t>(
      eventIndex, result.eventCount ? result.eventCount - 1U : 0U);
  if (!result.eventCount) return transition;
  for (uint8_t index = 0; index <= end; ++index) {
    const BattleEvent& event = result.events[index];
    if (event.type != BattleEventType::HpChanged || event.side != side ||
        event.pokemonUid != pokemonUid) continue;
    const int64_t next = std::max<int64_t>(0, std::min<int64_t>(
        static_cast<int64_t>(maximumHp), current +
        static_cast<int32_t>(event.after) -
        static_cast<int32_t>(event.before)));
    if (index == eventIndex) {
      transition.before = static_cast<uint32_t>(current);
      transition.after = static_cast<uint32_t>(next);
      transition.found = true;
      return transition;
    }
    current = next;
  }
  return transition;
}

// A level-up card belongs to the most recently defeated opponent in the
// journal. Keep the two visual prerequisites explicit so the UI can commit an
// exact zero-HP frame before it paints either of the stat pages.
struct KnockoutBarrier {
  int16_t hpEvent = -1;
  int16_t faintEvent = -1;

  bool hasFaint() const { return faintEvent >= 0; }
  bool complete() const { return hpEvent >= 0 && faintEvent > hpEvent; }
};

inline KnockoutBarrier knockoutBeforeLevelUp(const BattleActionResult& result,
                                              uint8_t levelEventIndex,
                                              uint32_t opponentUid) {
  KnockoutBarrier barrier{};
  const uint8_t end = std::min<uint8_t>(levelEventIndex, result.eventCount);
  for (uint8_t index = 0; index < end; ++index) {
    const BattleEvent& event = result.events[index];
    if (event.side != BattleSide::Opponent ||
        event.pokemonUid != opponentUid) continue;
    if (event.type == BattleEventType::HpChanged && event.after == 0U) {
      barrier.hpEvent = index;
      barrier.faintEvent = -1;
    } else if (event.type == BattleEventType::Fainted) {
      barrier.faintEvent = index;
    }
  }
  return barrier;
}

}  // namespace BattlePresentationOrder
