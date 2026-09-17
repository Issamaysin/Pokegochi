#include "game/BattleAnimationData.h"
#include <algorithm>
#include "BattleAnimationGenerated.inc"

const BattleAnimationData* findBattleAnimation(uint16_t moveId) {
  uint16_t lo=0,hi=static_cast<uint16_t>(sizeof(kBattleAnimations)/sizeof(kBattleAnimations[0]));
  while(lo<hi){const uint16_t mid=static_cast<uint16_t>((lo+hi)/2);if(kBattleAnimations[mid].moveId<moveId)lo=mid+1;else hi=mid;}
  return lo<battleAnimationCount()&&kBattleAnimations[lo].moveId==moveId?&kBattleAnimations[lo]:nullptr;
}
uint16_t battleAnimationCount(){return static_cast<uint16_t>(sizeof(kBattleAnimations)/sizeof(kBattleAnimations[0]));}
const char* battleAnimationCatalogId(){return kBattleAnimationCatalogId;}

const BattleAnimationProgram* findBattleAnimationProgram(uint16_t moveId) {
  uint16_t lo = 0;
  uint16_t hi = battleAnimationProgramCount();
  while (lo < hi) {
    const uint16_t mid = static_cast<uint16_t>((lo + hi) / 2U);
    if (kBattleAnimationPrograms[mid].moveId < moveId) lo = mid + 1U;
    else hi = mid;
  }
  return lo < battleAnimationProgramCount() && kBattleAnimationPrograms[lo].moveId == moveId
      ? &kBattleAnimationPrograms[lo] : nullptr;
}

const BattleAnimationCommand* battleAnimationProgramCommand(
    const BattleAnimationProgram& program, uint16_t index) {
  if (index >= program.commandCount) return nullptr;
  return &kBattleAnimationCommands[program.commandOffset + index];
}

const int16_t* battleAnimationCommandArguments(const BattleAnimationCommand& command) {
  return command.argumentCount ? &kBattleAnimationArguments[command.argumentOffset] : nullptr;
}

const BattleAnimationSpriteResource* battleAnimationSpriteResource(uint16_t resourceId) {
  return resourceId < battleAnimationSpriteResourceCount()
      ? &kBattleAnimationSpriteResources[resourceId] : nullptr;
}

namespace {
const BattleAnimationFrameSequence& selectedSpriteSequence(
    const BattleAnimationSpriteResource& resource,
    const int16_t* arguments, uint8_t argumentCount) {
  uint8_t selector = resource.selectorConstant;
  if (resource.selectorArgument >= 0 && arguments &&
      static_cast<uint8_t>(resource.selectorArgument) < argumentCount)
    selector = static_cast<uint8_t>(arguments[resource.selectorArgument]);
  const uint8_t sequenceCount = resource.sequenceCount ? resource.sequenceCount : 1U;
  const uint16_t index = static_cast<uint16_t>(
      resource.sequenceOffset + selector % sequenceCount);
  return kBattleAnimationFrameSequences[index];
}
}

uint8_t battleAnimationSpriteSequenceStepCount(
    const BattleAnimationSpriteResource& resource,
    const int16_t* arguments, uint8_t argumentCount) {
  return selectedSpriteSequence(resource, arguments, argumentCount).stepCount;
}

uint8_t battleAnimationSpriteSequenceFrame(
    const BattleAnimationSpriteResource& resource, uint8_t step,
    const int16_t* arguments, uint8_t argumentCount) {
  const BattleAnimationFrameSequence& sequence =
      selectedSpriteSequence(resource, arguments, argumentCount);
  if (!sequence.stepCount) return 0U;
  const uint8_t bounded = std::min<uint8_t>(step, sequence.stepCount - 1U);
  return kBattleAnimationFrameSteps[sequence.stepOffset + bounded].frame;
}

uint8_t battleAnimationSpriteFrameAtAge(
    const BattleAnimationSpriteResource& resource, uint8_t age,
    const int16_t* arguments, uint8_t argumentCount) {
  const BattleAnimationFrameSequence& sequence =
      selectedSpriteSequence(resource, arguments, argumentCount);
  if (!sequence.stepCount) return 0U;

  uint16_t prefixDuration = 0U;
  uint16_t totalDuration = 0U;
  for (uint8_t step = 0; step < sequence.stepCount; ++step) {
    const uint8_t duration = std::max<uint8_t>(
        1U, kBattleAnimationFrameSteps[sequence.stepOffset + step].duration);
    if (step < sequence.loopStep) prefixDuration += duration;
    totalDuration += duration;
  }
  uint16_t cursor = age;
  if (sequence.loopStep < sequence.stepCount && cursor >= prefixDuration) {
    const uint16_t loopDuration = totalDuration - prefixDuration;
    if (loopDuration) cursor = static_cast<uint16_t>(
        prefixDuration + (cursor - prefixDuration) % loopDuration);
  } else if (cursor >= totalDuration) {
    cursor = totalDuration ? totalDuration - 1U : 0U;
  }
  for (uint8_t step = 0; step < sequence.stepCount; ++step) {
    const BattleAnimationFrameStep& frame =
        kBattleAnimationFrameSteps[sequence.stepOffset + step];
    const uint8_t duration = std::max<uint8_t>(1U, frame.duration);
    if (cursor < duration) return frame.frame;
    cursor = static_cast<uint16_t>(cursor - duration);
  }
  return kBattleAnimationFrameSteps[
      sequence.stepOffset + sequence.stepCount - 1U].frame;
}

uint8_t battleAnimationSpriteFrameForSequenceAtAge(
    const BattleAnimationSpriteResource& resource, uint8_t selector,
    uint8_t age) {
  if (!resource.sequenceCount) return 0U;
  const BattleAnimationFrameSequence& sequence = kBattleAnimationFrameSequences[
      resource.sequenceOffset + selector % resource.sequenceCount];
  if (!sequence.stepCount) return 0U;
  uint16_t prefixDuration = 0U;
  uint16_t totalDuration = 0U;
  for (uint8_t step = 0; step < sequence.stepCount; ++step) {
    const uint8_t duration = std::max<uint8_t>(
        1U, kBattleAnimationFrameSteps[sequence.stepOffset + step].duration);
    if (step < sequence.loopStep) prefixDuration += duration;
    totalDuration += duration;
  }
  uint16_t cursor = age;
  if (sequence.loopStep < sequence.stepCount && cursor >= prefixDuration) {
    const uint16_t loopDuration = totalDuration - prefixDuration;
    if (loopDuration)
      cursor = static_cast<uint16_t>(
          prefixDuration + (cursor - prefixDuration) % loopDuration);
  } else if (cursor >= totalDuration) {
    cursor = totalDuration ? totalDuration - 1U : 0U;
  }
  for (uint8_t step = 0; step < sequence.stepCount; ++step) {
    const BattleAnimationFrameStep& frame =
        kBattleAnimationFrameSteps[sequence.stepOffset + step];
    const uint8_t duration = std::max<uint8_t>(1U, frame.duration);
    if (cursor < duration) return frame.frame;
    cursor = static_cast<uint16_t>(cursor - duration);
  }
  return kBattleAnimationFrameSteps[
      sequence.stepOffset + sequence.stepCount - 1U].frame;
}

static bool battleAnimationAffineSequenceAtAge(
    uint16_t sequenceOffset, uint8_t sequenceCount, uint8_t selector,
    uint16_t age, uint16_t startAge, BattleAnimationAffineState& result) {
  result = BattleAnimationAffineState{};
  if (!sequenceCount || age < startAge) return false;
  const BattleAnimationAffineSequence& sequence =
      kBattleAnimationAffineSequences[sequenceOffset + selector % sequenceCount];
  if (!sequence.commandCount) return false;

  int16_t scaleX = 0x100;
  int16_t scaleY = 0x100;
  uint8_t rotation = 0U;
  uint16_t cursor = static_cast<uint16_t>(age - startAge);
  uint8_t commandIndex = 0U;
  // age is at most 255 and every command consumes at least one tick. The guard
  // protects malformed source data without changing any valid FireRed loop.
  for (uint16_t guard = 0; guard < 512U; ++guard) {
    const BattleAnimationAffineCommand& command =
        kBattleAnimationAffineCommands[sequence.commandOffset + commandIndex];
    const uint8_t ticks = std::max<uint8_t>(1U, command.duration);
    if (command.duration == 0U) {
      scaleX = command.xScale;
      scaleY = command.yScale;
      rotation = static_cast<uint8_t>(command.rotation);
      if (cursor == 0U) break;
      --cursor;
    } else {
      bool complete = false;
      for (uint8_t tick = 0; tick < ticks; ++tick) {
        scaleX = static_cast<int16_t>(scaleX + command.xScale);
        scaleY = static_cast<int16_t>(scaleY + command.yScale);
        rotation = static_cast<uint8_t>(rotation + command.rotation);
        if (cursor == 0U) { complete = true; break; }
        --cursor;
      }
      if (complete) break;
    }
    ++commandIndex;
    if (commandIndex >= sequence.commandCount) {
      if (sequence.loopCommand < sequence.commandCount)
        commandIndex = sequence.loopCommand;
      else
        break;
    }
  }
  // GBA OAM affine values are inverse source-space matrices: 0x80 draws a
  // sprite at 2x, while 0x200 draws it at 0.5x.  Treating those values as a
  // direct destination percentage reversed every grow/shrink animation.
  const auto visibleScale = [](int16_t matrixScale) -> int16_t {
    if (!matrixScale) return matrixScale < 0 ? -250 : 250;
    const int32_t sign = matrixScale < 0 ? -1 : 1;
    const int32_t magnitude = std::abs(static_cast<int32_t>(matrixScale));
    return static_cast<int16_t>(sign * std::clamp<int32_t>(25600 / magnitude,
                                                           1, 250));
  };
  result.scaleXPercent = visibleScale(scaleX);
  result.scaleYPercent = visibleScale(scaleY);
  result.rotation = rotation;
  return true;
}

bool battleAnimationSpriteAffineAtAge(
    const BattleAnimationSpriteResource& resource, uint8_t age,
    const int16_t* arguments, uint8_t argumentCount,
    BattleAnimationAffineState& result) {
  uint8_t selector = resource.affineSelectorConstant;
  if (resource.affineSelectorArgument >= 0 && arguments &&
      static_cast<uint8_t>(resource.affineSelectorArgument) < argumentCount)
    selector = static_cast<uint8_t>(arguments[resource.affineSelectorArgument]);
  return battleAnimationAffineSequenceAtAge(
      resource.affineSequenceOffset, resource.affineSequenceCount, selector,
      age, resource.affineStartAge, result);
}

bool battleAnimationSpriteAffineForSequenceAtAge(
    const BattleAnimationSpriteResource& resource, uint8_t selector,
    uint8_t age, BattleAnimationAffineState& result) {
  return battleAnimationAffineSequenceAtAge(
      resource.affineSequenceOffset, resource.affineSequenceCount, selector,
      age, 0U, result);
}

bool battleAnimationTaskAffineAtAge(
    const BattleAnimationTaskResource& resource, uint16_t age,
    BattleAnimationAffineState& result) {
  return battleAnimationAffineSequenceAtAge(
      resource.affineSequenceOffset, resource.affineSequenceCount, 0U,
      age, resource.affineStartAge, result);
}

uint16_t battleAnimationTaskAffineDuration(
    const BattleAnimationTaskResource& resource) {
  if (!resource.affineSequenceCount) return 0U;
  const BattleAnimationAffineSequence& sequence =
      kBattleAnimationAffineSequences[resource.affineSequenceOffset];
  if (sequence.loopCommand < sequence.commandCount) return 0xFFFFU;
  uint32_t duration = resource.affineStartAge;
  for (uint8_t index = 0; index < sequence.commandCount; ++index) {
    const BattleAnimationAffineCommand& command =
        kBattleAnimationAffineCommands[sequence.commandOffset + index];
    duration += std::max<uint8_t>(1U, command.duration);
  }
  return static_cast<uint16_t>(std::min<uint32_t>(0xFFFEU, duration));
}

const BattleAnimationTaskResource* battleAnimationTaskResource(uint16_t resourceId) {
  return resourceId < battleAnimationTaskResourceCount()
      ? &kBattleAnimationTaskResources[resourceId] : nullptr;
}

uint16_t battleAnimationProgramCount() {
  return static_cast<uint16_t>(sizeof(kBattleAnimationPrograms) /
                               sizeof(kBattleAnimationPrograms[0]));
}

uint16_t battleAnimationSpriteResourceCount() {
  return static_cast<uint16_t>(sizeof(kBattleAnimationSpriteResources) /
                               sizeof(kBattleAnimationSpriteResources[0]));
}

uint16_t battleAnimationTaskResourceCount() {
  return static_cast<uint16_t>(sizeof(kBattleAnimationTaskResources) /
                               sizeof(kBattleAnimationTaskResources[0]));
}

BattleAnimationPlaybackKind battleAnimationPlaybackKind(uint16_t moveId) {
  (void)moveId;
  return BattleAnimationPlaybackKind::Generated;
}

// Battle animation data: 2f014e73a55d7991
