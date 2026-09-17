#pragma once
#include <cstdint>

enum BattleAnimationFlags : uint8_t {
  AnimShakeTarget = 1U << 0, AnimShakeScreen = 1U << 1,
  AnimBlend = 1U << 2, AnimTargetBackground = 1U << 3,
  AnimAttackerMotion = 1U << 4, AnimFlash = 1U << 5,
  AnimBackgroundChange = 1U << 6, AnimHideBattler = 1U << 7
};

struct BattleAnimationData {
  uint16_t moveId;
  // Six FireRed moves use four distinct graphics tags (Tri Attack, Heal Bell,
  // Brick Break, etc.). Keeping all four prevents the final effect layer from
  // silently disappearing during descriptor generation.
  const char* assets[4];
  // A source PNG can be a vertically stacked FireRed animation sheet.  The
  // asset pack exports every frame separately; keep its count here so the
  // runtime advances the real artwork instead of freezing on frame zero.
  uint8_t frameCounts[4];
  uint8_t assetCount;
  uint8_t particleCount;
  uint8_t frameDelayMs;
  uint8_t flags;
};

// Compact, chronological representation of FireRed's battle animation
// script.  The old BattleAnimationData descriptor above is retained as a
// compatibility/audit summary, but the runtime no longer invents a sequence
// from its first four graphics tags.  Commands and their arguments live in
// flash; only the currently active particles are materialised in RAM.
enum class BattleAnimationOpcode : uint8_t {
  SpawnSprite,
  VisualTask,
  Delay,
  WaitForVisuals,
  SetAlpha,
  SetBlendControl,
  BlendOff,
  ChangeBackground,
  FadeToBackground,
  // FireRed separates the opaque background swap (waitbgfadeout) from the
  // reveal (waitbgfadein). Tasks such as the Psychic palette cycler and the
  // scrolling Sky Attack background are started while the screen is black.
  FinishBackgroundFade,
  FadeRestoreBackground,
  RestoreBackground,
  SetBattlerLayer,
  ClearBattlerLayer,
  SetBackgroundPriority,
  SetVisibility,
  // FireRed animation scripts are small programs, not linear playlists.
  // Keeping their argument/query/branch bytecode is required for two-turn,
  // multi-hit, terrain, weather and friendship-dependent visuals.
  SetArgument,
  Query,
  Jump,
  Call,
  Return,
  JumpIfArgumentEquals,
  JumpIfMoveTurnEquals,
  End,
};

// Non-visual callbacks that feed a FireRed animation branch.  They are kept
// as explicit bytecode operations rather than being classified as one-frame
// visual tasks (which used to make every conditional path fall through).
enum class BattleAnimationQuery : uint8_t {
  IsContest,
  AttackerSide,
  TargetSide,
  IsTargetPlayerSide,
  IsTargetAttackerPartner,
  IsFuryCutterHitRight,
  FuryCutterHitCount,
  FrustrationPowerLevel,
  ReturnPowerLevel,
  IsMovePowerOver99,
  IsHealingMove,
  SeismicTossDamageLevel,
  BattleTerrain,
  RolloutCounter,
  Weather,
  IsDoomDesireHitTurn,
  IsMonInvisible,
};

enum class BattleAnimationMotion : uint8_t {
  Static,
  // Source-reviewed stationary callbacks.  Their cels do not travel, but
  // FireRed still mirrors the initial X offset in the attack direction.
  // Keeping this distinct from Static prevents hit splats and setup effects
  // from jumping to the wrong side when the opponent is the attacker.
  OrientedStatic,
  AnchorOrientedStatic,
  Projectile,
  // Exact public helper ABIs used throughout FireRed scripts.  Their six
  // arguments are start x/y, target x/y, duration and (for arcs) amplitude;
  // keeping them separate prevents a duration from becoming a screen offset.
  StandardLinearProjectile,
  StandardArcProjectile,
  // Source-faithful translations whose callbacks add a second movement
  // component or a retained state after reaching the target.  These may not
  // fall back to Projectile: their arguments are different ABIs and several
  // of them deliberately continue moving after the initial translation.
  SinWaveProjectile,
  HyperBeamOrb,
  ConfuseRayBounce,
  BulletSeedRicochet,
  WebThreadWave,
  WillOWispOrb,
  HeartWaveProjectile,
  SlidingKickWave,
  MusicNoteWave,
  SolarBeamOrb,
  ZapCannonSpark,
  PoisonGasCloud,
  SwirlingFog,
  // Callback-level profiles for FireRed state machines which happen to call
  // a shared translation helper but do not share that helper's public ABI.
  // Keeping these names explicit is intentional: the generator audit must
  // never certify a callback merely because its source mentions "Linear" or
  // "Arc" somewhere in a later state.
  AbsorptionArc,
  AcidBubbleArc,
  TargetDroplet,
  TargetLocalLinear,
  Bonemerang,
  CoinThrow,
  CrossChop,
  DirtPlume,
  LocalFixedVelocity,
  OffscreenFly,
  LocalStarVelocity,
  HyperVoiceRing,
  LeechSeedArc,
  MimicOrb,
  AbsorptionLinear,
  RaiseSprite,
  PreinitializedArc,
  SludgeArc,
  SolarBeamBigOrb,
  SpikesArc,
  SunlightRay,
  SuperpowerFireball,
  TearDropArc,
  TravelDiagonal,
  TriAttackTriangle,
  WaterGunDroplet,
  RandomBattlerHit,
  ConfettiBallistic,
  FlyingParticle,
  GreenStarRise,
  PowderParticle,
  TwisterParticle,
  PresentHealRise,
  RazorLeafDrift,
  RockFragmentLinear,
  RockScatterBounce,
  DriftingBubble,
  SporeOrbit,
  DizzyWave,
  FocusPunchWobble,
  GhostRiseFade,
  GrudgeFlameWave,
  SleepLetterWave,
  RisingHeartWave,
  ParticleBurstWave,
  PinkHeartWave,
  BatonPassBall,
  EruptionFallingRock,
  ShortLinearDrop,
  FallingCoinBounce,
  FallingFeatherWave,
  FallingRockEllipse,
  HailDiagonal,
  IngrainOrbWave,
  IngrainRoot,
  MudSportDirt,
  RedHeartRising,
  WaterSprayBallistic,
  AirWaveTaskProjectile,
  DiveBallCycle,
  FlyBallRise,
  NeedleArmSpike,
  PsychoBoostRise,
  RainDrop,
  SludgeBurst,
  SmallWaterOrb,
  SuperpowerOrbHold,
  SwallowOrb,
  WavyMusicNote,
  WeatherBallRise,
  // Shadow Ball is not a linear projectile. FireRed moves it halfway from
  // the attacker, holds it spinning in the centre, then accelerates it into
  // the target. Its three arguments are phase durations, not x/y offsets.
  ShadowBall,
  // FireRed's standard cross-battler translation ABI: initial x/y offsets,
  // target x/y offsets, duration and coordinate flags.  Keeping this apart
  // from Projectile prevents the duration and destination offsets from being
  // misread as generic screen-space coordinates.
  TargetLocationProjectile,
  // Dragon Breath/Rage use a private translation helper whose side mirroring
  // and argument layout differ from the standard projectile callback.
  DragonFire,
  // Sky Attack crosses the target after 12 frames and continues off-screen.
  SkyAttackBird,
  // Pain Split spawns one falling/bouncing spark on each selected battler.
  PainSplitBounce,
  BreathPuff,
  GuardRingRise,
  ViceGripPincer,
  GuillotinePincer,
  HealBellNote,
  DiagonalStrike,
  MegahornStrike,
  LeechLifeNeedle,
  DirtProjectile,
  OutrageFlame,
  PetalDanceBig,
  PetalDanceSmall,
  TargetConverge,
  VoltTackleSlide,
  WeatherBallDown,
  Arc,
  Falling,
  Rising,
  Orbit,
  // Callback-faithful circular families. FireRed reuses "orbit-like"
  // callbacks with incompatible argument layouts; treating argument 0/1 as
  // a generic x/y offset displaced Fire Spin, Whirlpool, Hidden Power and
  // several setup effects far away from their battler.
  Vortex,
  GrowingCircle,
  Circular,
  OrbitingProjectile,
  ConfuseRaySpiral,
  FastOrbit,
  RadialScatter,
  ExpandingSpiral,
  ExpandingEllipse,
  RadialLinear,
  DragonDanceOrbit,
  AngelSway,
  DevilOrbit,
  PerishSongNote,
  TrickBagOrbit,
  // Retained FireRed sprite state machines. These callbacks install their
  // real movement routine after creation; the old generator saw only the
  // initializer and incorrectly rendered every family below as Static.
  SwordRise,
  SliceArc,
  EllipticalGust,
  StompDrop,
  HornLunge,
  BubbleRise,
  BiteClamp,
  RoarLine,
  ElectricOrbit,
  SmokeDrift,
  EndureRise,
  FireRing,
  SoftBoiledEgg,
  SleepLetter,
  PencilScribble,
  CurseNail,
  // Assist uses literal GBA screen coordinates for both endpoints.  It is
  // neither battler-relative nor a normal attacker-to-target projectile.
  AssistPawprintTravel,
  // Callback-specific stationary/state-machine OBJ families.  Their visual
  // state is not represented by the cel/affine table alone: the callback
  // controls placement, blinking, alpha or a later sequence change.
  ArmThrustImpact,
  DefensiveWallCycle,
  ConstrictBinding,
  FalseSwipeFlicker,
  FlashingHitFlicker,
  FlatterSpotlightMask,
  FrenzyPlantRoot,
  LickFlicker,
  MetronomeFinger,
  MilkBottleDrink,
  RecycleFade,
  TimedRedX,
  SharpenBlink,
  SmellingSaltsBlink,
  SpiderWebFade,
  StringWrapBlink,
  TauntFinger,
  ThoughtBubble,
  WhiteHaloFade,
  ThunderboltOrbBlink,
  ThunderWavePairBlink,
  StaticElectricity,
  IceBallImpactShard,
  SlashFlicker,
  MovementWavesRepeat,
  PerishSongDelayedNote,
  FlyingSandCrescent,
  WeakFrustrationMark,
  DiveWaterSplash,
  BounceBallShrinkExact,
  HydroCannonChargeExact,
  DigDirtMoundExact,
  BellyDrumHandExact,
  QuestionMarkExact,
  BentSpoonExact,
  WhipHitExact,
  RevengeScratchExact,
  ClappingHandWindowMask,
  TailGlowOrbExact,
  BasicFistOrFootExact,
  SpinningKickOrPunchExact,
  InvertedHitSplatExact,
  HitSplatPersistentExact,
  // These three sprites are deliberately long-lived in FireRed. Their
  // callbacks wait for a later visual task to signal destruction, so treating
  // them as a twelve-frame static cel drops the centrepiece of Moonlight and
  // Conversion before the script reaches its fade.
  ConversionHold,
  ConversionReturn,
  MoonAbsolute,
  MoonlightSparkleFall,
  ProtectSlide,
  ForesightScan,
  DestinyShadow,
  LockOnScan,
  FalseSwipe,
  PresentBounce,
  FlameDrift,
  SpotlightSway,
  ClappingHands,
  RapidSpin,
  SweetScentPetal,
  MusicOrbit,
  SmellingSalts,
  FollowMeFinger,
  HelpingHandClap,
  WishStar,
  SuperpowerRock,
  BrickWallShake,
  BrickWallShard,
  YawnCloud,
  KnockOffStrike,
  MeteorMashStar,
  RockTombBounce,
  BlockBounce,
  WaterPulseBubble,
  WaterPulseRing,
  Wave,
  Scatter,
  // Callback-faithful ice motions. These must remain distinct: the FireRed
  // ICE_CRYSTALS tag is a tile atlas shared by several unrelated cels and
  // movement callbacks, not one generic projectile animation.
  IceBeamParticle,
  SwirlingSnowball,
  WavyBeyondTarget,
  IceImpact,
  BattlerLunge,
  BattlerDip,
  BattlerSlide,
  BattlerBow,
  BattlerShake,
  PalettePulse,
  ProceduralPulse,
};

// FireRed's createsprite anchor is the owner used by the GBA sprite engine,
// not necessarily the visual travel direction.  For example, Psybeam is
// allocated at ANIM_TARGET but travels attacker -> target, while absorption
// orbs are allocated at ANIM_ATTACKER and travel target -> attacker.
enum class BattleAnimationTravel : uint8_t {
  Local,
  AttackerToTarget,
  TargetToAttacker,
};

// Cmd_createsprite always allocates the OBJ at gBattleAnimTarget in the GBA
// engine. Its ANIM_ATTACKER/ANIM_TARGET flag only chooses subpriority; it is
// not a screen-space anchor. A callback may then explicitly move the sprite
// to the attacker, or (for generic callbacks such as AnimSpriteOnMonPos) pick
// the battler from one of its arguments.
enum class BattleAnimationOrigin : uint8_t {
  Target,
  Attacker,
  Argument0,
  Argument1,
  Argument2,
  Argument3,
  Argument4,
  Argument5,
  Argument6,
  Argument7,
};

enum class BattleAnimationTaskKind : uint8_t {
  Pulse,
  Shake,
  BattlerMotion,
  Palette,
  Background,
  PsychicBackground,
  SlidingBackground,
  SandstormBackground,
  SpotlightBackground,
  DistortionBackground,
  FogBackground,
  SunlightBackground,
  HeartBackground,
  CinematicBackground,
  // Explicit non-visual families prevent script queries and sound helpers
  // from being mistaken for invented particles. Effect is reserved for a
  // visual task whose pixels are procedural in the original engine.
  Control,
  Sound,
  Effect,
};

// createvisualtask callbacks are as important as createsprite callbacks in
// FireRed.  Keeping only the broad task kind made Acid Armor, Splash,
// Teleport, Minimize, palette flashes, etc. all look like the same horizontal
// wobble (or disappear entirely).  Every callback emitted by the generator is
// now assigned one explicit retained-renderer motion.
enum class BattleAnimationTaskMotion : uint8_t {
  None,
  ShakeHorizontal,
  ShakeVertical,
  ShakeSink,
  ShakePattern,
  Horizontal,
  Vertical,
  Lunge,
  Elliptical,
  Bounce,
  Sway,
  Spin,
  SlideOffscreen,
  Sink,
  Hide,
  Reveal,
  Teleport,
  ScalePulse,
  StretchVertical,
  Squish,
  Shrink,
  Grow,
  ClonePulse,
  Transform,
  // Source callback identities for battler motion.  These deliberately do
  // not collapse into the generic motions above: callbacks that happen to
  // move on the same axis still use different arguments, timing, visibility
  // and affine state machines in FireRed.
  AcidArmor,
  AttackerStretchAndDisappear,
  CurseStretchingBlackBg,
  DeepInhale,
  DefenseCurlDeformMon,
  DigDownMovement,
  DigUpMovement,
  DoubleTeam,
  ExtremeSpeedImpact,
  ExtremeSpeedMonReappear,
  FlailMovement,
  GrowAndGrayscale,
  GrowAndShrink,
  HelpingHandAttackerMovement,
  MeditateStretchAttacker,
  Minimize,
  MonToSubstitute,
  NightShadeClone,
  NightmareClone,
  OdorSleuthMovement,
  PainSplitMovement,
  RapidSpinMonElevation,
  RockMonBackAndForth,
  RolePlaySilhouette,
  RotateAuroraRingColors,
  RotateMonSpriteToSide,
  RotateMonToSideAndRestore,
  ScaleMonAndRestore,
  SetAllNonAttackersInvisible,
  SetAttackerInvisibleWaitForSignal,
  SetGrayscaleOrOriginalPal,
  ShrinkTargetCopy,
  SkullBashPosition,
  SlackOffSquish,
  SlideOffScreenExact,
  SmellingSaltsSquish,
  SpitUpDeformMon,
  Splash,
  SquishAndSweatDroplets,
  StockpileDeformMon,
  StretchAttackerUp,
  StretchTargetUp,
  StrongFrustrationGrowAndShrink,
  SwallowDeformMon,
  SwayMonExact,
  TeeterDanceMovement,
  TeleportExact,
  ThrashMoveMonHorizontal,
  ThrashMoveMonVertical,
  TransformMon,
  TranslateMonElliptical,
  TranslateMonEllipticalRespectSide,
  TransparentCloneGrowAndShrink,
  UproarDistortion,
  VoltTackleAttackerReappear,
  WindUpLunge,
  WithdrawExact,
  // Exact shake callback identities.
  HorizontalShakeExact,
  ShakeAndSinkMonExact,
  ShakeBattleTerrainExact,
  ShakeMonExact,
  ShakeMon2Exact,
  ShakeMonInPlaceExact,
  ShakeTargetPowerOrDamage,
  ShakeTargetInPatternExact,
  PaletteFade,
  PaletteFlash,
  PaletteInvert,
  PaletteGrayscale,
  PaletteCycle,
  PaletteTint,
  // Source callback identities for palette operations.  Argument layout and
  // affected palette masks vary substantially, so a single generic tint is
  // not a safe renderer contract.
  AlphaFadeInExact,
  AttackerFadeFromInvisibleExact,
  AttackerFadeToInvisibleExact,
  AttackerPunchWithTraceExact,
  BlendBackgroundExact,
  BlendBattleAnimPalExact,
  BlendBattleAnimPalExcludeExact,
  BlendColorCycleExact,
  BlendColorCycleExcludeExact,
  BlendMonInAndOutExact,
  BlendNonAttackerPalettesExact,
  Conversion2AlphaBlendExact,
  ConversionAlphaBlendExact,
  FacadeColorBlendExact,
  FadeScreenToWhiteExact,
  FakeOutExact,
  FlashExact,
  HardwarePaletteFadeExact,
  InitAttackerFadeFromInvisibleExact,
  InitMementoShadowExact,
  InvertScreenColorExact,
  MetallicShineExact,
  MoonlightEndFadeExact,
  ScaryFaceExact,
  SetCamouflageBlendExact,
  SpiteTargetShadowExact,
  StatusClearedEffectExact,
  TraceMonBlendedExact,
  // Palette tasks that address an OBJ palette by ANIM_TAG.  They must stay
  // separate from battler palette operations: the target is one family of
  // live particles, not the attacker/target sprite.
  TagPaletteRotate,
  TagBlendCycle,
  TagBlendInOut,
  TagBlendToColor,
  TagFlash,
  MagicalLeafPaletteCycle,
  MusicNotePaletteSetup,
  MusicNotePaletteClear,
  PaletteBackupSave,
  PaletteBackupRestore,
  PaletteBackupCommit,
  CurseWhiteLines,
  // Background/scanline task profiles translated from the actual FireRed
  // callbacks. They intentionally remain distinct: their argument layouts,
  // stopping signals and affected layer are unrelated.
  SlidingBackground,
  PsychicPaletteCycle,
  FadeScreenPaletteCycle,
  SandstormField,
  HazeFogField,
  MistBallFogField,
  HeartsField,
  MorningSunField,
  SpotlightWindow,
  ExtrasensoryDistortion,
  DragonDanceDistortion,
  HeatWaveTargetMotion,
  SketchScanline,
  MementoAttackerShadow,
  MementoTargetShadow,
  MementoBackgroundControl,
  FissureBackgroundPosition,
  SeismicTossBackground,
  SeismicTossBackgroundEnd,
  SkyUppercutBackground,
  PersistentAttackerHide,
  // AnimTask_CreateSurfWave is a 512x256 BG1 plane with a dedicated
  // scanline-alpha state machine.  Keep it separate from ordinary script
  // backgrounds: it owns its palette (Surf/Muddy Water), side-specific
  // tilemap, scroll and 136-frame blend envelope.
  SurfWave,
  DrillPeckHitSplats,
  SolarBeamOrbs,
  ElectricBoltSegments,
  SmokescreenImpact,
  GlareEyeDots,
  BarrageBall,
  DestinyBondShadow,
  RolloutDebris,
  Raindrops,
  SpeedDust,
  HailStones,
  TormentThoughtBubbles,
  ElectricChargingParticles,
  EruptionLaunchRocks,
  SkillSwapOrbs,
  ImprisonOrbs,
  GrudgeFlames,
  AirCutterProjectiles,
  WaterSpoutLaunch,
  WaterSpoutRain,
  FrozenIceCube,
  VoltTackleBolts,
  WaterSportOrbs,
  LeafBladePath,
  ShockWaveProgressingBolt,
  ShockWaveLightning,
};

enum class BattleAnimationTaskTarget : uint8_t {
  ScriptAnchor,
  Attacker,
  Target,
  BothBattlers,
  Screen,
  None,
};

struct BattleAnimationTaskResource {
  BattleAnimationTaskKind kind;
  BattleAnimationTaskMotion motion;
  BattleAnimationTaskTarget target;
  uint16_t lifetimeFrames;
  uint16_t spriteResourceIds[2];
  uint8_t spriteResourceCount;
  // Battler tasks can own an affine command stream directly instead of via
  // a SpriteTemplate. Keeping the source stream in the generated resource
  // avoids replacing Minimize, Splash, Withdraw, Uproar, and similar tasks
  // with a generic scale pulse.
  uint16_t affineSequenceOffset;
  uint8_t affineSequenceCount;
  uint8_t affineStartAge;
  // Persistent FireRed tasks decrement gAnimVisualTaskCount and therefore do
  // not block waitforvisualfinish. They end only after a matching setarg.
  bool blocking;
};

struct BattleAnimationSpriteResource {
  const char* asset;
  // FireRed palette tasks select their particles through ANIM_TAG_* rather
  // than SpriteTemplate identity.  Keeping the original numeric tag in the
  // compact resource is what lets Gust, Hyper Beam, Magical Leaf, etc. tint
  // exactly the intended cels instead of flashing a battler or doing nothing.
  uint16_t paletteTag;
  uint8_t frameCount;
  uint16_t sequenceOffset;
  uint8_t sequenceCount;
  int8_t selectorArgument;
  uint8_t selectorConstant;
  BattleAnimationMotion motion;
  BattleAnimationTravel travel;
  BattleAnimationOrigin origin;
  // FireRed callback arguments are not a common x/y ABI.  -1 means that the
  // callback has no ordinary battler-relative offset on that axis.  Keeping
  // the two selectors in generated data prevents control arguments (battler,
  // duration, frame/affine selector) from being applied as coordinates.
  int8_t xOffsetArgument;
  int8_t yOffsetArgument;
  uint8_t lifetimeFrames;
  uint16_t affineSequenceOffset;
  uint8_t affineSequenceCount;
  int8_t affineSelectorArgument;
  uint8_t affineSelectorConstant;
  // Some FireRed callbacks install their affine table only after the normal
  // cel animation ends (Amnesia's question mark is the canonical case).
  // Starting that transform at spawn visibly skips the first phase.
  uint8_t affineStartAge;
};

struct BattleAnimationFrameStep {
  uint8_t frame;
  uint8_t duration;
};

struct BattleAnimationFrameSequence {
  uint16_t stepOffset;
  uint8_t stepCount;
  // 0xFF means that an ended FireRed sequence holds its final cel. Any other
  // value is the ANIMCMD_JUMP destination and loops from that step.
  uint8_t loopStep;
};

struct BattleAnimationAffineCommand {
  int16_t xScale;
  int16_t yScale;
  int8_t rotation;
  uint8_t duration;
};

struct BattleAnimationAffineSequence {
  uint16_t commandOffset;
  uint8_t commandCount;
  uint8_t loopCommand;
};

struct BattleAnimationAffineState {
  int16_t scaleXPercent = 100;
  int16_t scaleYPercent = 100;
  uint8_t rotation = 0;
};

struct BattleAnimationCommand {
  uint16_t resourceId;
  uint16_t argumentOffset;
  uint8_t opcode;
  uint8_t anchor;
  uint8_t priority;
  uint8_t argumentCount;
};

struct BattleAnimationProgram {
  uint16_t moveId;
  uint16_t commandOffset;
  uint16_t commandCount;
};

// Every move uses the same FireRed script VM. Multi-turn presentation is an
// input to that VM (gAnimMoveTurn in FireRed), not a second hand-authored
// renderer. Keeping a second path for Dig was enough for its task order,
// visibility and assets to drift away from the original script.
enum class BattleAnimationPlaybackKind : uint8_t {
  Generated,
};

const BattleAnimationData* findBattleAnimation(uint16_t moveId);
uint16_t battleAnimationCount();
const char* battleAnimationCatalogId();
const BattleAnimationProgram* findBattleAnimationProgram(uint16_t moveId);
const BattleAnimationCommand* battleAnimationProgramCommand(
    const BattleAnimationProgram& program, uint16_t index);
const int16_t* battleAnimationCommandArguments(const BattleAnimationCommand& command);
const BattleAnimationSpriteResource* battleAnimationSpriteResource(uint16_t resourceId);
uint8_t battleAnimationSpriteFrameAtAge(
    const BattleAnimationSpriteResource& resource, uint8_t age,
    const int16_t* arguments, uint8_t argumentCount);
uint8_t battleAnimationSpriteFrameForSequenceAtAge(
    const BattleAnimationSpriteResource& resource, uint8_t sequence,
    uint8_t age);
uint8_t battleAnimationSpriteSequenceStepCount(
    const BattleAnimationSpriteResource& resource,
    const int16_t* arguments, uint8_t argumentCount);
uint8_t battleAnimationSpriteSequenceFrame(
    const BattleAnimationSpriteResource& resource, uint8_t step,
    const int16_t* arguments, uint8_t argumentCount);
bool battleAnimationSpriteAffineAtAge(
    const BattleAnimationSpriteResource& resource, uint8_t age,
    const int16_t* arguments, uint8_t argumentCount,
    BattleAnimationAffineState& state);
bool battleAnimationSpriteAffineForSequenceAtAge(
    const BattleAnimationSpriteResource& resource, uint8_t sequence,
    uint8_t age, BattleAnimationAffineState& state);
bool battleAnimationTaskAffineAtAge(
    const BattleAnimationTaskResource& resource, uint16_t age,
    BattleAnimationAffineState& state);
uint16_t battleAnimationTaskAffineDuration(
    const BattleAnimationTaskResource& resource);
const BattleAnimationTaskResource* battleAnimationTaskResource(uint16_t resourceId);
uint16_t battleAnimationProgramCount();
uint16_t battleAnimationSpriteResourceCount();
uint16_t battleAnimationTaskResourceCount();
BattleAnimationPlaybackKind battleAnimationPlaybackKind(uint16_t moveId);
