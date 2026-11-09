#include "game/BattleAnimationData.h"
#include <algorithm>
#include "BattleAnimationGenerated.inc"

namespace {
uint16_t authoredAnimationMove(uint16_t moveId) {
  switch (moveId) {
    case 687U: return 406U; // Core Enforcer -> Dragon Pulse
    case 920U: return 617U; // Nihil Light -> Light of Ruin
    default: return moveId;
  }
}

#if defined(ARDUINO)
// The retained ESP32 renderer executes only the generated VM below. The old
// per-move summary duplicated roughly 19 KiB of asset names/flags and is used
// only by native audit helpers and Android's Aurora Beam preload. Keep one
// presence descriptor on-device so existing lookup guards stay cheap without
// reducing or aliasing any authored command program.
constexpr BattleAnimationData kEmbeddedAnimationPresence = {
    0U, {"", "", "", ""}, {0U, 0U, 0U, 0U}, 0U, 1U, 65U, 0U};
#endif
}

const BattleAnimationData* findBattleAnimation(uint16_t moveId) {
  moveId = authoredAnimationMove(moveId);
#if defined(ARDUINO)
  return findBattleAnimationProgram(moveId)
      ? &kEmbeddedAnimationPresence : nullptr;
#else
  uint16_t lo=0,hi=static_cast<uint16_t>(sizeof(kBattleAnimations)/sizeof(kBattleAnimations[0]));
  while(lo<hi){const uint16_t mid=static_cast<uint16_t>((lo+hi)/2);if(kBattleAnimations[mid].moveId<moveId)lo=mid+1;else hi=mid;}
  return lo<battleAnimationCount()&&kBattleAnimations[lo].moveId==moveId?&kBattleAnimations[lo]:nullptr;
#endif
}
uint16_t battleAnimationCount(){
#if defined(ARDUINO)
  return battleAnimationProgramCount();
#else
  return static_cast<uint16_t>(sizeof(kBattleAnimations)/sizeof(kBattleAnimations[0]));
#endif
}
const char* battleAnimationCatalogId(){return kBattleAnimationCatalogId;}

const BattleAnimationProgram* findBattleAnimationProgram(uint16_t moveId) {
  moveId = authoredAnimationMove(moveId);
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
  constexpr uint8_t kCommandIndexBits = 13U;
  constexpr uint16_t kCommandIndexMask = (1U << kCommandIndexBits) - 1U;
  const size_t streamIndex = static_cast<size_t>(program.commandOffset) + index;
  if (streamIndex >= kBattleAnimationCommandIndexCount) return nullptr;
  const size_t bitOffset = streamIndex * kCommandIndexBits;
  const size_t byteOffset = bitOffset >> 3U;
  uint32_t packed = 0U;
  for (uint8_t byte = 0U; byte < 3U; ++byte) {
    const size_t source = byteOffset + byte;
    if (source < sizeof(kBattleAnimationCommandIndices))
      packed |= static_cast<uint32_t>(kBattleAnimationCommandIndices[source]) << (byte * 8U);
  }
  const uint16_t commandIndex = static_cast<uint16_t>(
      (packed >> (bitOffset & 7U)) & kCommandIndexMask);
  constexpr size_t kCommandCount =
      sizeof(kBattleAnimationCommands) / sizeof(kBattleAnimationCommands[0]);
  return commandIndex < kCommandCount ? &kBattleAnimationCommands[commandIndex] : nullptr;
}

bool battleAnimationDecodeArguments(uint16_t byteOffset, uint8_t count,
                                    int16_t* output,
                                    uint8_t outputCapacity) {
  if (!count) return true;
  if (!output || count > outputCapacity || count > 8U ||
      byteOffset >= sizeof(kBattleAnimationArguments))
    return false;
  size_t offset = byteOffset;
  for (uint8_t index = 0U; index < count; ++index) {
    uint32_t encoded = 0U;
    uint8_t shift = 0U;
    bool complete = false;
    while (offset < sizeof(kBattleAnimationArguments) && shift <= 14U) {
      const uint8_t byte = kBattleAnimationArguments[offset++];
      encoded |= static_cast<uint32_t>(byte & 0x7FU) << shift;
      if (!(byte & 0x80U)) { complete = true; break; }
      shift = static_cast<uint8_t>(shift + 7U);
    }
    if (!complete || encoded > 0xFFFFU) return false;
    output[index] = static_cast<int16_t>(
        (encoded >> 1U) ^ static_cast<uint32_t>(-
            static_cast<int32_t>(encoded & 1U)));
  }
  return true;
}

bool battleAnimationDecodeCommandArguments(
    const BattleAnimationCommand& command, int16_t* output,
    uint8_t outputCapacity) {
  return battleAnimationDecodeArguments(command.argumentOffset(),
                                        command.argumentCount(), output,
                                        outputCapacity);
}

const int16_t* battleAnimationArgumentsAt(uint16_t byteOffset, uint8_t count) {
  static int16_t decoded[8]{};
  if (!count) return nullptr;
  if (!battleAnimationDecodeArguments(byteOffset, count, decoded,
                                      sizeof(decoded) / sizeof(decoded[0])))
    return nullptr;
  return decoded;
}

const int16_t* battleAnimationCommandArguments(const BattleAnimationCommand& command) {
  return battleAnimationArgumentsAt(command.argumentOffset(),
                                    command.argumentCount());
}

bool battleAnimationArgumentsValid(const int16_t* arguments, uint8_t count) {
  if (!count) return true;
  return arguments && count <= 8U;
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

// Battle animation data: 3681dc64797d3c5c
