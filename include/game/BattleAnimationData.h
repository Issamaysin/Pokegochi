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

// The generated callback catalog now needs more than 256 distinct motion
// profiles.  Keeping an 8-bit underlying type would alias the last profiles
// after future additions, so motion identity is deliberately 16-bit.
enum class BattleAnimationMotion : uint16_t {
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
  // Shell Smash's two jaw halves have a callback-specific ABI: arguments
  // 0/1 are local offsets, 3/4 are fixed-point velocity, and 5/6 are the
  // travel/hold durations.  It must not share LocalFixedVelocity's ABI.
  LockingJaw,
  OffscreenFly,
  LocalStarVelocity,
  HyperVoiceRing,
  LeechSeedArc,
  MimicOrb,
  AbsorptionLinear,
  // Reflect Type and Geomancy use a callback with no script arguments: the
  // guard ring starts 40 px below the attacker and rises 72 px in 13 ticks.
  // It cannot share AnimRaiseSprite's five-argument Ancient Power ABI.
  SurroundingRingRise,
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
  // Expansion callbacks with multi-phase or callback-specific ABIs. Volt
  // Switch arcs to the target and then returns linearly to the user; the
  // growing superpower projectile always translates for sixteen ticks while
  // its authored affine stream doubles the cel.
  VoltSwitchReturn,
  GrowingSuperpower,
  PledgeOrbRise,
  FallingObject,
  GeyserRise,
  RandomAffineOrientedStatic,
  RandomCentredAffineHit,
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
  // Mind Blown's explosion rays use a target-local four-argument state
  // machine: direction flag, destination offset and exact duration.
  MindBlownExplosion,
  CoreEnforcerHit,
  PhotonGeyserBeam,
  BlastBurnTargetPlume,
  SoulStealingStar,
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
  TrumpCardArc,
  PoisonJabProjectile,
  AquaTailSweep,
  DracoMeteorRock,
  DragonRushStrike,
  SmallCloudDrift,
  SpriteOnMonDuration,
  MaxFlutterbyArc,
  WringOutCircle,
  WoodHammerHammer,
  PowerSwapGuardSwapArc,
  TargetSinWave,
  GunkShotParticleWave,
  PsychoCutStrike,
  StealthRockArc,
  WorrySeedArc,
  LavaPlumeDrift,
  PluckParticleJump,
  TrumpCardParticleScatter,
  RockPolishStreak,
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
  ZenHeadbuttExact,
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
  // FireRed's dummy SpriteTemplates move the selected battler itself.  They
  // are not a generic wave: the callback arguments are a battler selector,
  // target offset, mirror flag and linear duration.
  BattlerSlideToOffset,
  BattlerRestorePosition,
  BattlerSlideToOffsetAndBack,
  BattlerBow,
  BattlerShake,
  PalettePulse,
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
  CompressTargetHorizontally,
  CompressTargetHorizontallyFast,
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
  GrowTarget,
  TwinkleTackleLaunch,
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
  RotateVerticallyExact,
  ScaleMonAndRestore,
  SnatchOpposingMonMoveExact,
  SetAllNonAttackersInvisible,
  AllBattlersInvisible,
  AllBattlersVisible,
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
  NightSlashPalette,
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
  DoomDesireLightBeam,
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
  BestowItemThrow,
  DestinyBondShadow,
  RolloutDebris,
  TectonicRageRollout,
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
  IonRain,
};

// pokeemerald-expansion's canonical internal presentation IDs. Pokegochi
// keeps each base move as the mechanical MoveId and selects these rows only
// after the battle engine resolves a Z-Move.
constexpr uint16_t kFirstGenericZMoveAnimationMoveId = 848U;
constexpr uint16_t kLastGenericZMoveAnimationMoveId = 865U;
constexpr uint16_t kFirstSignatureZMoveAnimationMoveId = 866U;
constexpr uint16_t kLastSignatureZMoveAnimationMoveId = 882U;
constexpr uint16_t kLightThatBurnsTheSkyAnimationMoveId = 881U;

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
  uint16_t sequenceOffset;
  uint16_t affineSequenceOffset;
  // Some source callbacks create a second SpriteTemplate directly from the
  // first sprite (Close Combat's fist/foot impact is the retained example).
  uint16_t childResourceId = 0xFFFFU;
  BattleAnimationMotion motion;
  // Grouping the 16-bit members above removes alignment holes from every one
  // of the 908 generated callback resources without packing unaligned words.
  uint8_t frameCount;
  uint8_t sequenceCount;
  // Every argument selector is -1 or 0..7. Pairing their signed nibbles keeps
  // each resource 4-byte aligned while reducing the table from 28 to 24 bytes.
  int8_t selectorArgument : 4;
  int8_t affineSelectorArgument : 4;
  uint8_t selectorConstant;
  BattleAnimationTravel travel : 2;
  BattleAnimationOrigin origin : 4;
  // FireRed callback arguments are not a common x/y ABI.  -1 means that the
  // callback has no ordinary battler-relative offset on that axis.  Keeping
  // the two selectors in generated data prevents control arguments (battler,
  // duration, frame/affine selector) from being applied as coordinates.
  int8_t xOffsetArgument : 4;
  int8_t yOffsetArgument : 4;
  uint8_t lifetimeFrames;
  uint8_t affineSequenceCount;
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
  // The complete animation VM contains more than fifteen thousand commands.
  // Store each in exactly 40 bits: resource 10, opcode 5, anchor 2, priority
  // dictionary index 5, argument count 4 and argument offset 14. Besides
  // saving one byte per command, the tenth resource bit fixes IDs 512..573,
  // which the previous nine-bit layout silently wrapped.
  uint8_t packed[5];

  static constexpr uint8_t priorityValues[29] = {
      0, 1, 2, 3, 4, 5, 6, 9, 10, 11, 12, 13, 14, 16,
      19, 20, 40, 41, 50, 51, 66, 70, 120, 130, 131, 132, 133, 136, 240};

  static constexpr uint8_t priorityCode(uint8_t priority) {
    for (uint8_t index = 0; index < sizeof(priorityValues); ++index)
      if (priorityValues[index] == priority) return index;
    return 0;
  }

  constexpr BattleAnimationCommand(uint16_t resource, uint16_t arguments,
                                   uint8_t opcodeValue, uint8_t anchorValue,
                                   uint8_t priorityValue,
                                   uint8_t argumentCountValue)
      : packed{
            static_cast<uint8_t>(resource & 0xFFU),
            static_cast<uint8_t>(((resource >> 8U) & 0x03U) |
                                 ((opcodeValue & 0x1FU) << 2U) |
                                 ((anchorValue & 0x01U) << 7U)),
            static_cast<uint8_t>(((anchorValue >> 1U) & 0x01U) |
                                 (priorityCode(priorityValue) << 1U) |
                                 ((argumentCountValue & 0x03U) << 6U)),
            static_cast<uint8_t>(((argumentCountValue >> 2U) & 0x03U) |
                                 ((arguments & 0x003FU) << 2U)),
            static_cast<uint8_t>((arguments >> 6U) & 0xFFU)} {}

  constexpr uint16_t resourceId() const {
    return static_cast<uint16_t>(packed[0] |
                                 ((packed[1] & 0x03U) << 8U));
  }
  constexpr uint8_t opcode() const {
    return static_cast<uint8_t>((packed[1] >> 2U) & 0x1FU);
  }
  constexpr uint8_t anchor() const {
    return static_cast<uint8_t>(((packed[1] >> 7U) & 0x01U) |
                                ((packed[2] & 0x01U) << 1U));
  }
  constexpr uint8_t priority() const {
    const uint8_t code = static_cast<uint8_t>((packed[2] >> 1U) & 0x1FU);
    return code < sizeof(priorityValues) ? priorityValues[code] : 0U;
  }
  constexpr uint8_t argumentCount() const {
    return static_cast<uint8_t>(((packed[2] >> 6U) & 0x03U) |
                                ((packed[3] & 0x03U) << 2U));
  }
  constexpr uint16_t argumentOffset() const {
    return static_cast<uint16_t>(((packed[3] >> 2U) & 0x3FU) |
                                 (static_cast<uint16_t>(packed[4]) << 6U));
  }
};

static_assert(sizeof(BattleAnimationCommand) == 5,
              "Battle animation command must stay compact in flash");

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
bool battleAnimationDecodeArguments(uint16_t byteOffset, uint8_t count,
                                    int16_t* output,
                                    uint8_t outputCapacity);
bool battleAnimationDecodeCommandArguments(
    const BattleAnimationCommand& command, int16_t* output,
    uint8_t outputCapacity);
const int16_t* battleAnimationCommandArguments(const BattleAnimationCommand& command);
const int16_t* battleAnimationArgumentsAt(uint16_t byteOffset, uint8_t count);
bool battleAnimationArgumentsValid(const int16_t* arguments, uint8_t count);
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
