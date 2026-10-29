# FireRed + expansion battle animation coverage

Generated from every reachable branch of the original 354 FireRed move scripts plus the exact expansion overlay selected for Gen-IV and Gen-V moves. Generation fails if a visual task has no renderer, if an installed sprite state callback moves but resolves to Static, if any SpriteTemplate still depends on an unaudited name heuristic, or if a non-dummy frame/affine stream cannot be parsed. Frame variants, durations and GBA affine commands are retained rather than flattened into a generic carousel.

## Sprite callback proof

| Proof | SpriteTemplates |
|---|---:|
| ExactCallbackProfile | 527 |
| ExplicitReviewedCallback | 42 |
| FireRedStandardHelper | 1 |
| SourceParticleMotion | 1 |
| SourceVerticalMotion | 3 |

| SpriteTemplate | Callback | Frame seq. / selector | Affine seq. / selector | Lifetime | Installed states | Motion | Travel | Origin | Proof |
|---|---|---|---|---|---|---|---|---|---|
| gAbsorptionOrbSpriteTemplate | AnimAbsorptionOrb | 1 / SingleSequence | 1 / SingleSequence | 40 (MotionProfile) | AnimAbsorptionOrb_Step | AbsorptionArc | TargetToAttacker | Target | ExactCallbackProfile |
| gAcidPoisonBubbleSpriteTemplate | AnimAcidPoisonBubble | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | AnimAcidPoisonBubble_Step | AcidBubbleArc | AttackerToTarget | Argument3 | ExactCallbackProfile |
| gAcidPoisonDropletSpriteTemplate | AnimAcidPoisonDroplet | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetDroplet | Local | Target | ExactCallbackProfile |
| gAcrobaticsSlashesSpriteTemplate | AnimAcrobaticsSlashes | 1 / SingleSequence | 16 / DefaultInitialSequence | 27 (frame=27) | - | Scatter | Local | Target | SourceParticleMotion |
| gAcupressureSpriteTemplate | AnimMoveAccupressure | 1 / SingleSequence | 2 / DefaultInitialSequence | 60 (affine=60) | AnimMoveAccupressureTransition | StandardLinearProjectile | Local | Target | ExactCallbackProfile |
| gAfterYouGreenRageTemplate | AnimAngerMark | 1 / SingleSequence | 1 / SingleSequence | 16 (affine=16) | - | AnchorOrientedStatic | Local | Target | ExactCallbackProfile |
| gAirCutterSliceSpriteTemplate | AnimAirCutterSlice | 1 / SingleSequence | 0 / NoAffine | 23 (MotionProfile) | AnimSlice_Step | SliceArc | Local | Target | ExactCallbackProfile |
| gAirWaveCrescentSpriteTemplate | AnimAirWaveCrescent | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | StandardLinearProjectile | AttackerToTarget | Argument6 | ExactCallbackProfile |
| gAirWaveProjectileSpriteTemplate | AnimAirWaveProjectile | 1 / SingleSequence | 0 / NoAffine | 56 (MotionProfile) | AnimAirWaveProjectile_Step1, AnimAirWaveProjectile_Step2 | AirWaveTaskProjectile | Local | Target | ExactCallbackProfile |
| gAncientPowerRockSpriteTemplate | AnimRaiseSprite | 6 / Argument4 | 0 / NoAffine | 64 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | RaiseSprite | Local | Attacker | ExactCallbackProfile |
| gAngelSpriteTemplate | AnimAngel | 1 / SingleSequence | 0 / NoAffine | 101 (MotionProfile) | - | AngelSway | Local | Target | ExactCallbackProfile |
| gAngerMarkSpriteTemplate | AnimAngerMark | 1 / SingleSequence | 1 / SingleSequence | 16 (affine=16) | - | AnchorOrientedStatic | Local | Target | ExactCallbackProfile |
| gAquaTailHitSpriteTemplate | AnimAquaTail | 1 / SingleSequence | 4 / Argument3 | 32 (affine=9) | - | Static | Local | Argument2 | ExplicitReviewedCallback |
| gAquaTailKnockOffSpriteTemplate | AnimKnockOffAquaTail | 1 / SingleSequence | 2 / Constant1 | 32 (frame=8) | AnimKnockOffAquaTailStep | AquaTailSweep | Local | Target | ExactCallbackProfile |
| gArmThrustHandSpriteTemplate | AnimArmThrustHit | 5 / DefaultInitialSequence | 0 / NoAffine | 24 (MotionProfile) | AnimArmThrustHit_Step | ArmThrustImpact | Local | Target | ExactCallbackProfile |
| gAromatherapyBigFlowerSpriteTemplate | AnimFlyingParticle | 1 / SingleSequence | 1 / SingleSequence | 96 (MotionProfile) | AnimFlyingParticle_Step | FlyingParticle | Local | Target | ExactCallbackProfile |
| gAromatherapySmallFlowerSpriteTemplate | AnimFlyingParticle | 1 / SingleSequence | 0 / NoAffine | 96 (MotionProfile) | AnimFlyingParticle_Step | FlyingParticle | Local | Target | ExactCallbackProfile |
| gAssistPawprintSpriteTemplate | AnimAssistPawprint | 1 / SingleSequence | 0 / NoAffine | 37 (MotionProfile) | InitAndRunAnimFastLinearTranslation, AnimFastTranslateLinearWaitEnd | AssistPawprintTravel | Local | Target | ExactCallbackProfile |
| gAttackOrderParticleSpriteTemplate | AnimMoveTwisterParticle | 2 / DefaultInitialSequence | 0 / NoAffine | 96 (MotionProfile) | AnimMoveTwisterParticle_Step | TwisterParticle | Local | Target | ExactCallbackProfile |
| gAuraSphereBlast | AnimSuperpowerFireball | 1 / SingleSequence | 0 / NoAffine | 17 (MotionProfile) | AnimTranslateLinear_WithFollowup | SuperpowerFireball | AttackerToTarget | Target | ExactCallbackProfile |
| gAuroraBeamRingSpriteTemplate | AnimAuroraBeamRings | 2 / DefaultInitialSequence | 1 / SingleSequence | 31 (MotionProfile) | AnimAuroraBeamRings_Step | StandardLinearProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gAutotomizeMetalShardsTemplate | AnimTearDrop | 1 / SingleSequence | 2 / Constant1 | 33 (MotionProfile) | AnimTearDrop_Step | TearDropArc | Local | Target | ExactCallbackProfile |
| gAvalancheSpriteTemplate | AvalancheAnim_Step | 3 / Argument1 | 0 / NoAffine | 64 (MotionProfile) | AvalancheAnim_Step2, TranslateSpriteInEllipse, TranslateSpriteInEllipse | FallingRockEllipse | Local | Target | ExactCallbackProfile |
| gBarrageBallSpriteTemplate | SpriteCallbackDummy | 1 / SingleSequence | 2 / DefaultInitialSequence | 255 (MotionProfile) | - | Static | Local | Target | ExplicitReviewedCallback |
| gBarrierWallSpriteTemplate | AnimDefensiveWall | 1 / SingleSequence | 0 / NoAffine | 62 (MotionProfile) | AnimDefensiveWall_Step2, AnimDefensiveWall_Step3, AnimDefensiveWall_Step4, AnimDefensiveWall_Step5 | DefensiveWallCycle | Local | Attacker | ExactCallbackProfile |
| gBasicHitSplatSpriteTemplate | AnimHitSplatBasic | 1 / SingleSequence | 4 / Argument3 | 9 (affine=9) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gBatonPassPokeballSpriteTemplate | AnimBatonPassPokeball | 1 / SingleSequence | 0 / NoAffine | 42 (MotionProfile) | - | BatonPassBall | Local | Attacker | ExactCallbackProfile |
| gBattleAnimSpriteTemplate_LeafStorm | AnimAirWaveCrescent | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | StandardLinearProjectile | AttackerToTarget | Argument6 | ExactCallbackProfile |
| gBattleAnimSpriteTemplate_LeafStorm2 | AnimNeedleArmSpike | 2 / DefaultInitialSequence | 1 / SingleSequence | 64 (MotionProfile) | AnimNeedleArmSpike_Step | NeedleArmSpike | Local | Argument4 | ExactCallbackProfile |
| gBelchBerryTemplate | AnimMissileArc | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimMissileArc_Step | StandardArcProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gBellSpriteTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 105 (frame=105) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gBellyDrumHandSpriteTemplate | AnimBellyDrumHand | 1 / SingleSequence | 0 / NoAffine | 9 (MotionProfile) | - | BellyDrumHandExact | Local | Attacker | ExactCallbackProfile |
| gBentSpoonSpriteTemplate | AnimBentSpoon | 2 / Constant1 | 0 / NoAffine | 187 (frame=187) | - | BentSpoonExact | Local | Attacker | ExactCallbackProfile |
| gBlackBallSpriteTemplate | AnimThrowProjectile | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimThrowProjectile_Step | StandardArcProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gBlackSmokeSpriteTemplate | AnimBlackSmoke | 1 / SingleSequence | 0 / NoAffine | 28 (MotionProfile) | AnimBlackSmoke_Step | SmokeDrift | Local | Target | ExactCallbackProfile |
| gBlendThinRingExpandingSpriteTemplate | AnimBlendThinRing | 1 / SingleSequence | 2 / DefaultInitialSequence | 31 (frame=1+affine=31) | AnimSpriteOnMonPos | OrientedStatic | Local | Target | ExactCallbackProfile |
| gBlizzardIceCrystalSpriteTemplate | AnimMoveParticleBeyondTarget | 1 / SingleSequence | 0 / NoAffine | 18 (MotionProfile) | AnimWiggleParticleTowardsTarget | WavyBeyondTarget | AttackerToTarget | Argument7 | ExactCallbackProfile |
| gBlockXSpriteTemplate | AnimBlockX | 1 / SingleSequence | 0 / NoAffine | 74 (MotionProfile) | AnimBlockX_Step | BlockBounce | Local | Target | ExactCallbackProfile |
| gBlueFlareBurnTemplate | AnimFireSpread | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | TranslateSpriteLinearFixedPoint | LocalFixedVelocity | Local | Target | ExactCallbackProfile |
| gBlueFlareFlameJabTemplate | AnimNeedleArmSpike | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimNeedleArmSpike_Step | NeedleArmSpike | Local | Argument4 | ExactCallbackProfile |
| gBlueFlareFlameSwirlTemplate | AnimParticleInVortex | 1 / SingleSequence | 0 / NoAffine | 30 (MotionProfile) | AnimParticleInVortex_Step | Vortex | Local | Argument6 | ExactCallbackProfile |
| gBonemerangSpriteTemplate | AnimBonemerangProjectile | 1 / SingleSequence | 1 / SingleSequence | 42 (MotionProfile) | AnimBonemerangProjectile_Step, AnimBonemerangProjectile_End | Bonemerang | AttackerToTarget | Target | ExactCallbackProfile |
| gBounceBallLandSpriteTemplate | AnimBounceBallLand | 1 / SingleSequence | 1 / SingleSequence | 36 (MotionProfile) | - | BlockBounce | Local | Target | ExactCallbackProfile |
| gBounceBallShrinkSpriteTemplate | AnimBounceBallShrink | 1 / SingleSequence | 1 / SingleSequence | 24 (affine=24) | - | BounceBallShrinkExact | Local | Attacker | ExactCallbackProfile |
| gBowMonSpriteTemplate | AnimBowMon | 1 / SingleSequence | 0 / NoAffine | 20 (MotionProfile) | AnimBowMon_Step1, AnimBowMon_Step1_Callback, AnimBowMon_Step4, TranslateSpriteLinearById, AnimBowMon_Step2, AnimBowMon_Step3, AnimBowMon_Step3_Callback, AnimBowMon_Step4 | BattlerBow | Local | Attacker | ExactCallbackProfile |
| gBreathPuffSpriteTemplate | AnimBreathPuff | 2 / DefaultInitialSequence | 0 / NoAffine | 53 (MotionProfile) | TranslateSpriteLinearFixedPoint | BreathPuff | Local | Attacker | ExactCallbackProfile |
| gBrickBreakWallShardSpriteTemplate | AnimBrickBreakWallShard | 4 / OamTileArgument1 | 0 / NoAffine | 41 (MotionProfile) | AnimBrickBreakWallShard_Step | BrickWallShard | Local | Argument0 | ExactCallbackProfile |
| gBrickBreakWallSpriteTemplate | AnimBrickBreakWall | 1 / SingleSequence | 0 / NoAffine | 36 (MotionProfile) | AnimBrickBreakWall_Step | BrickWallShake | Local | Argument0 | ExactCallbackProfile |
| gBulletSeedSpriteTemplate | AnimBulletSeed | 1 / SingleSequence | 1 / SingleSequence | 37 (MotionProfile) | AnimBulletSeed_Step1, AnimBulletSeed_Step2, StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | BulletSeedRicochet | AttackerToTarget | Target | ExactCallbackProfile |
| gCelebrateBagTemplate | AnimFollowMeFinger | 1 / SingleSequence | 2 / DefaultInitialSequence | 74 (affine=74) | AnimFollowMeFinger_Step1, AnimFollowMeFinger_Step2, AnimMetronomeFinger_Step | FollowMeFinger | Local | Target | ExactCallbackProfile |
| gChatterSingNotesTemplate | AnimIceEffectParticle | 8 / DefaultInitialSequence | 1 / SingleSequence | 20 (affine=17) | AnimFlickerIceEffectParticle | IceImpact | Local | Target | ExactCallbackProfile |
| gCircleThrowRingTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 2 / DefaultInitialSequence | 31 (frame=1+affine=31) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gClampJawSpriteTemplate | AnimBite | 1 / SingleSequence | 8 / Argument2 | 16 (MotionProfile) | AnimBite_Step1, AnimBite_Step2 | BiteClamp | Local | Target | ExactCallbackProfile |
| gClappingHand2SpriteTemplate | AnimClappingHand2 | 1 / SingleSequence | 0 / NoAffine | 36 (MotionProfile) | - | ClappingHandWindowMask | Local | Target | ExactCallbackProfile |
| gClappingHandSpriteTemplate | AnimClappingHand | 1 / SingleSequence | 0 / NoAffine | 36 (MotionProfile) | AnimClappingHand_Step | ClappingHands | Local | Attacker | ExactCallbackProfile |
| gClawSlashSpriteTemplate | AnimClawSlash | 2 / Argument2 | 0 / NoAffine | 20 (frame=20) | - | Static | Local | Target | ExplicitReviewedCallback |
| gCoinThrowSpriteTemplate | AnimCoinThrow | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | InitAnimLinearTranslationWithSpeedAndPos, AnimTranslateLinear_WithFollowup | CoinThrow | AttackerToTarget | Target | ExactCallbackProfile |
| gComplexPaletteBlendSpriteTemplate | AnimComplexPaletteBlend | 1 / SingleSequence | 0 / NoAffine | 24 (MotionProfile) | AnimComplexPaletteBlend_Step1, AnimComplexPaletteBlend_Step2 | PalettePulse | Local | Attacker | ExactCallbackProfile |
| gConfideBubbleTemplate | AnimThoughtBubble | 4 / DefaultInitialSequence | 0 / NoAffine | 64 (frame=8) | AnimThoughtBubble_Step | ThoughtBubble | Local | Argument0 | ExactCallbackProfile |
| gConfuseRayBallBounceSpriteTemplate | AnimConfuseRayBallBounce | 1 / SingleSequence | 1 / SingleSequence | 42 (MotionProfile) | AnimConfuseRayBallBounce_Step1, AnimConfuseRayBallBounce_Step2, DestroyAnimSpriteAndDisableBlend | ConfuseRayBounce | AttackerToTarget | Target | ExactCallbackProfile |
| gConfuseRayBallSpiralSpriteTemplate | AnimConfuseRayBallSpiral | 1 / SingleSequence | 0 / NoAffine | 61 (MotionProfile) | AnimConfuseRayBallSpiral_Step | ConfuseRaySpiral | Local | Target | ExactCallbackProfile |
| gConstrictBindingSpriteTemplate | AnimConstrictBinding | 2 / DefaultInitialSequence | 2 / Argument2 | 255 (affine=13) | AnimConstrictBinding_Step1, AnimConstrictBinding_Step2 | ConstrictBinding | Local | Target | ExactCallbackProfile |
| gContinentalCrushGrowingRockSpriteTemplate | AnimGrowingChargeOrb | 1 / SingleSequence | 3 / DefaultInitialSequence | 172 (affine=172) | - | Static | Local | Argument0 | ExplicitReviewedCallback |
| gContinentalCrushNeedleArmSpriteTemplate | AnimNeedleArmSpike | 3 / DefaultInitialSequence | 0 / NoAffine | 64 (MotionProfile) | AnimNeedleArmSpike_Step | NeedleArmSpike | Local | Argument4 | ExactCallbackProfile |
| gConversion2SpriteTemplate | AnimConversion2 | 1 / SingleSequence | 1 / SingleSequence | 166 (MotionProfile) | AnimConversion2_Step, StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | ConversionReturn | Local | Target | ExactCallbackProfile |
| gConversionSpriteTemplate | AnimConversion | 1 / SingleSequence | 1 / SingleSequence | 255 (MotionProfile) | - | ConversionHold | Local | Attacker | ExactCallbackProfile |
| gCottonGuardSporeTemplate | AnimPowerAbsorptionOrb | 2 / DefaultInitialSequence | 1 / SingleSequence | 64 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | AbsorptionLinear | TargetToAttacker | Target | ExactCallbackProfile |
| gCraftyShieldPinkConversionTemplate | AnimConversion | 1 / SingleSequence | 1 / SingleSequence | 255 (MotionProfile) | - | ConversionHold | Local | Attacker | ExactCallbackProfile |
| gCrossChopHandSpriteTemplate | AnimCrossChopHand | 1 / SingleSequence | 0 / NoAffine | 48 (MotionProfile) | AnimCrossChopHand_Step, StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup, StartAnimLinearTranslation | CrossChop | Local | Target | ExactCallbackProfile |
| gCrossImpactSpriteTemplate | AnimCrossImpact | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gCrushGripExplosionTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 20 (frame=20) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gCrushGripHandTemplate | AnimRockBlastRock | 1 / SingleSequence | 2 / Constant1 | 26 (MotionProfile) | - | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gCurseGhostSpriteTemplate | AnimGhostStatusSprite | 1 / SingleSequence | 0 / NoAffine | 48 (MotionProfile) | AnimGhostStatusSprite_End | GhostRiseFade | Local | Target | ExactCallbackProfile |
| gCurseNailSpriteTemplate | AnimCurseNail | 4 / DefaultInitialSequence | 0 / NoAffine | 76 (MotionProfile) | AnimCurseNail_Step1, AnimCurseNail_Step2, AnimCurseNail_End | CurseNail | Local | Attacker | ExactCallbackProfile |
| gCuttingSliceSpriteTemplate | AnimCuttingSlice | 1 / SingleSequence | 0 / NoAffine | 23 (MotionProfile) | AnimSlice_Step | SliceArc | Local | Target | ExactCallbackProfile |
| gDarkPulseSpriteTemplate | AnimShadowBall | 1 / SingleSequence | 1 / SingleSequence | 41 (MotionProfile) | AnimShadowBall_Step | ShadowBall | Local | Attacker | ExactCallbackProfile |
| gDarkVoidPurpleStarsTemplate | AnimGrantingStars | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | TranslateSpriteLinearFixedPoint | LocalStarVelocity | Local | Attacker | ExactCallbackProfile |
| gDestinyBondWhiteShadowSpriteTemplate | AnimDestinyBondWhiteShadow | 1 / SingleSequence | 0 / NoAffine | 48 (MotionProfile) | AnimDestinyBondWhiteShadow_Step | DestinyShadow | Local | Argument0 | ExactCallbackProfile |
| gDevilSpriteTemplate | AnimDevil | 2 / Constant0 | 0 / NoAffine | 91 (MotionProfile) | - | DevilOrbit | Local | Target | ExactCallbackProfile |
| gDiamondStormBlizzardTemplate | AnimMoveParticleBeyondTarget | 1 / SingleSequence | 0 / NoAffine | 18 (MotionProfile) | AnimWiggleParticleTowardsTarget | WavyBeyondTarget | AttackerToTarget | Argument7 | ExactCallbackProfile |
| gDiamondStormDiamondsTemplate | AnimMoveTwisterParticle | 1 / SingleSequence | 2 / DefaultInitialSequence | 96 (MotionProfile) | AnimMoveTwisterParticle_Step | TwisterParticle | Local | Target | ExactCallbackProfile |
| gDirtMoundSpriteTemplate | AnimDigDirtMound | 2 / OamTileArgument1 | 0 / NoAffine | 96 (MotionProfile) | - | DigDirtMoundExact | Local | Argument0 | ExactCallbackProfile |
| gDirtPlumeSpriteTemplate | AnimDirtPlumeParticle | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimDirtPlumeParticle_Step | DirtPlume | Local | Target | ExactCallbackProfile |
| gDiveBallSpriteTemplate | AnimDiveBall | 1 / SingleSequence | 1 / SingleSequence | 96 (MotionProfile) | AnimDiveBall_Step1, AnimDiveBall_Step2 | DiveBallCycle | Local | Attacker | ExactCallbackProfile |
| gDiveWaterSplashSpriteTemplate | AnimDiveWaterSplash | 1 / SingleSequence | 0 / NoAffine | 25 (MotionProfile) | - | DiveWaterSplash | Local | Argument0 | ExactCallbackProfile |
| gDizzyPunchDuckSpriteTemplate | AnimDizzyPunchDuck | 1 / SingleSequence | 0 / NoAffine | 42 (MotionProfile) | - | DizzyWave | Local | Target | ExactCallbackProfile |
| gDracoMeteorRocksSpriteTemplate | AnimDracoMeteorRock | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimDracoMeteorRock_Step | DracoMeteorRock | Local | Target | ExactCallbackProfile |
| gDracoMeteorTailSpriteTemplate | AnimDracoMeteorRock | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | AnimDracoMeteorRock_Step | DracoMeteorRock | Local | Target | ExactCallbackProfile |
| gDragonBreathFireSpriteTemplate | AnimDragonFireToTarget | 2 / Constant1 | 2 / Constant1 | 21 (MotionProfile) | - | DragonFire | AttackerToTarget | Target | ExactCallbackProfile |
| gDragonDanceOrbSpriteTemplate | AnimDragonDanceOrb | 1 / SingleSequence | 0 / NoAffine | 82 (MotionProfile) | AnimDragonDanceOrb_Step | DragonDanceOrbit | Local | Attacker | ExactCallbackProfile |
| gDragonPulseSpriteTemplate | TranslateAnimSpriteToTargetMonLocation | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gDragonRageFirePlumeSpriteTemplate | AnimDragonRageFirePlume | 1 / SingleSequence | 0 / NoAffine | 25 (frame=25) | - | OrientedStatic | Local | Argument0 | ExactCallbackProfile |
| gDragonRageFireSpitSpriteTemplate | AnimDragonFireToTarget | 2 / Constant1 | 2 / Constant1 | 21 (MotionProfile) | - | DragonFire | AttackerToTarget | Target | ExactCallbackProfile |
| gDragonRushSpriteTemplate | AnimDragonRush | 1 / SingleSequence | 2 / Constant1 | 8 (frame=8) | AnimDragonRushStep | DragonRushStrike | Local | Target | ExactCallbackProfile |
| gDualChopImpactTemplate | AnimHitSplatBasic | 1 / SingleSequence | 4 / Argument3 | 9 (affine=9) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gEclipsingOrbSpriteTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 42 (frame=42) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gEerieImpulseImpactTemplate | AnimGrantingStars | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | TranslateSpriteLinearFixedPoint | LocalStarVelocity | Local | Attacker | ExactCallbackProfile |
| gEerieImpulseRingTemplate | AnimHyperVoiceRing | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | AnimHyperVoiceRing_WaitEnd | HyperVoiceRing | AttackerToTarget | Target | ExactCallbackProfile |
| gEggThrowSpriteTemplate | AnimThrowProjectile | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimThrowProjectile_Step | StandardArcProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gElectricChargingParticlesSpriteTemplate | SpriteCallbackDummy | 2 / DefaultInitialSequence | 0 / NoAffine | 255 (MotionProfile) | - | Static | Local | Target | ExplicitReviewedCallback |
| gElectricPuffSpriteTemplate | AnimElectricPuff | 1 / SingleSequence | 0 / NoAffine | 12 (frame=12) | - | Static | Local | Argument0 | ExplicitReviewedCallback |
| gElectricTerrainFlyingBallTemplate | AnimOrbitScatter | 2 / DefaultInitialSequence | 0 / NoAffine | 32 (MotionProfile) | AnimOrbitScatter_Step | RadialScatter | Local | Attacker | ExactCallbackProfile |
| gElectricTerrainOrbsTemplate | AnimOrbitFast | 2 / DefaultInitialSequence | 0 / NoAffine | 52 (MotionProfile) | AnimOrbitFast_Step | FastOrbit | Local | Attacker | ExactCallbackProfile |
| gElectricitySpriteTemplate | AnimElectricity | 3 / OamTileArgument3 | 0 / NoAffine | 64 (MotionProfile) | - | OrientedStatic | Local | Target | ExactCallbackProfile |
| gElectrifyRingTemplate | SpriteCB_SurroundingRing | 1 / SingleSequence | 2 / DefaultInitialSequence | 14 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | RaiseSprite | Local | Attacker | ExactCallbackProfile |
| gElectrifyYellowRingTemplate | AnimUproarRing | 1 / SingleSequence | 2 / Constant1 | 31 (frame=1+affine=31) | AnimSpriteOnMonPos | OrientedStatic | Local | Target | ExactCallbackProfile |
| gElectroBallCannonBallTemplate | AnimShadowBall | 1 / SingleSequence | 1 / SingleSequence | 41 (MotionProfile) | AnimShadowBall_Step | ShadowBall | Local | Attacker | ExactCallbackProfile |
| gEllipticalGustSpriteTemplate | AnimEllipticalGust | 1 / SingleSequence | 0 / NoAffine | 71 (MotionProfile) | AnimEllipticalGust_Step | EllipticalGust | Local | Target | ExactCallbackProfile |
| gEmberFlareSpriteTemplate | AnimEmberFlare | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimTravelDiagonally, StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TravelDiagonal | Local | Target | ExactCallbackProfile |
| gEmberSpriteTemplate | TranslateAnimSpriteToTargetMonLocation | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gEndureEnergySpriteTemplate | AnimEndureEnergy | 1 / SingleSequence | 0 / NoAffine | 28 (frame=24) | AnimEndureEnergy_Step | EndureRise | Local | Argument0 | ExactCallbackProfile |
| gEnergyBallSpriteTemplate | AnimShadowBall | 1 / SingleSequence | 1 / SingleSequence | 41 (MotionProfile) | AnimShadowBall_Step | ShadowBall | Local | Attacker | ExactCallbackProfile |
| gEruptionFallingRockSpriteTemplate | AnimEruptionFallingRock | 4 / OamTileArgument4 | 0 / NoAffine | 96 (MotionProfile) | AnimEruptionFallingRock_Step | EruptionFallingRock | Local | Target | ExactCallbackProfile |
| gEruptionLaunchRockSpriteTemplate | AnimEruptionLaunchRock | 1 / SingleSequence | 0 / NoAffine | 112 (MotionProfile) | - | Static | Local | Target | ExplicitReviewedCallback |
| gExplosionSpriteTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 20 (frame=20) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gEyeSparkleSpriteTemplate | AnimEyeSparkle | 1 / SingleSequence | 0 / NoAffine | 20 (frame=20) | AnimEyeSparkle_Step | OrientedStatic | Local | Attacker | ExactCallbackProfile |
| gFacadeSweatDropSpriteTemplate | AnimFacadeSweatDrop | 1 / SingleSequence | 0 / NoAffine | 7 (MotionProfile) | - | ShortLinearDrop | Local | Target | ExactCallbackProfile |
| gFairyWindCloudTemplate | AnimTranslateLinearSingleSineWave | 1 / SingleSequence | 1 / SingleSequence | 31 (MotionProfile) | AnimTranslateLinearSingleSineWave_Step | StandardArcProjectile | AttackerToTarget | Argument6 | ExactCallbackProfile |
| gFallingCoinSpriteTemplate | AnimFallingCoin | 1 / SingleSequence | 1 / SingleSequence | 52 (MotionProfile) | AnimFallingCoin_Step | FallingCoinBounce | Local | Target | ExactCallbackProfile |
| gFallingFeatherSpriteTemplate | AnimFallingFeather | 2 / DefaultInitialSequence | 0 / NoAffine | 128 (frame=1) | AnimFallingFeather_Step | FallingFeatherWave | Local | Target | ExactCallbackProfile |
| gFallingRockSpriteTemplate | AnimFallingRock | 3 / Argument1 | 0 / NoAffine | 64 (MotionProfile) | AnimFallingRock_Step, TranslateSpriteInEllipse, TranslateSpriteInEllipse | FallingRockEllipse | Local | Target | ExactCallbackProfile |
| gFalseSwipePositionedSliceSpriteTemplate | AnimFalseSwipePositionedSlice | 2 / Constant1 | 0 / NoAffine | 22 (MotionProfile) | AnimFalseSwipeSlice_Step3 | FalseSwipeFlicker | Local | Target | ExactCallbackProfile |
| gFalseSwipeSliceSpriteTemplate | AnimFalseSwipeSlice | 2 / DefaultInitialSequence | 0 / NoAffine | 30 (frame=16) | AnimFalseSwipeSlice_Step1, AnimFalseSwipeSlice_Step2, AnimFalseSwipeSlice_Step3, TranslateSpriteLinear | FalseSwipe | Local | Target | ExactCallbackProfile |
| gFangSpriteTemplate | AnimFang | 1 / SingleSequence | 1 / SingleSequence | 32 (frame=32) | - | Static | Local | Target | ExplicitReviewedCallback |
| gFastFlyingMusicNotesSpriteTemplate | AnimFlyingMusicNotes | 8 / Argument0 | 1 / SingleSequence | 48 (MotionProfile) | AnimFlyingMusicNotes_Step | MusicOrbit | Local | Attacker | ExactCallbackProfile |
| gFeintSwipeSpriteTemplate | AnimMoveFeintSwipe | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimMoveFeintSwipeStep | SliceArc | Local | Target | ExactCallbackProfile |
| gFeintZoomSpriteTemplate | AnimMoveFeintZoom | 1 / SingleSequence | 1 / SingleSequence | 64 (affine=11) | - | Static | Local | Target | ExplicitReviewedCallback |
| gFinalGambitBlueYawnTemplate | AnimShadowBall | 1 / SingleSequence | 1 / SingleSequence | 41 (MotionProfile) | AnimShadowBall_Step | ShadowBall | Local | Attacker | ExactCallbackProfile |
| gFinalGambitExplosionTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 20 (frame=20) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gFireBlastCrossSpriteTemplate | AnimFireCross | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | TranslateSpriteLinear | LocalFixedVelocity | Local | Target | ExactCallbackProfile |
| gFireBlastRingSpriteTemplate | AnimFireRing | 1 / SingleSequence | 0 / NoAffine | 74 (MotionProfile) | AnimFireRing_Step1, AnimFireRing_Step2, AnimFireRing_Step3 | FireRing | Local | Attacker | ExactCallbackProfile |
| gFirePledgeSpriteTemplate | AnimFlyUpTarget | 1 / SingleSequence | 0 / NoAffine | 48 (MotionProfile) | AnimFlyUpTarget_Step | Rising | Local | Target | ExplicitReviewedCallback |
| gFirePlumeSpriteTemplate | AnimFirePlume | 1 / SingleSequence | 0 / NoAffine | 36 (MotionProfile) | AnimLargeFlame_Step | FlameDrift | Local | Attacker | ExactCallbackProfile |
| gFireSpinSpriteTemplate | AnimParticleInVortex | 1 / SingleSequence | 0 / NoAffine | 30 (MotionProfile) | AnimParticleInVortex_Step | Vortex | Local | Argument6 | ExactCallbackProfile |
| gFireSpiralInwardSpriteTemplate | AnimFireSpiralInward | 2 / DefaultInitialSequence | 0 / NoAffine | 30 (MotionProfile) | TranslateSpriteInGrowingCircle | GrowingCircle | Local | Target | ExactCallbackProfile |
| gFireSpiralOutwardSpriteTemplate | AnimFireSpiralOutward | 1 / SingleSequence | 0 / NoAffine | 30 (MotionProfile) | AnimFireSpiralOutward_Step1, AnimFireSpiralOutward_Step2 | ExpandingSpiral | Local | Attacker | ExactCallbackProfile |
| gFireSpreadSpriteTemplate | AnimFireSpread | 2 / DefaultInitialSequence | 0 / NoAffine | 64 (MotionProfile) | TranslateSpriteLinearFixedPoint | LocalFixedVelocity | Local | Target | ExactCallbackProfile |
| gFistFootRandomPosSpriteTemplate | AnimFistOrFootRandomPos | 5 / Argument2 | 0 / NoAffine | 24 (MotionProfile) | AnimFistOrFootRandomPos_Step | RandomBattlerHit | Local | Argument0 | ExactCallbackProfile |
| gFistFootSpriteTemplate | AnimBasicFistOrFoot | 5 / Argument4 | 0 / NoAffine | 64 (MotionProfile) | - | BasicFistOrFootExact | Local | Argument3 | ExactCallbackProfile |
| gFlameChargeEmberTemplate | AnimPetalDanceBigFlower | 1 / SingleSequence | 0 / NoAffine | 141 (MotionProfile) | AnimPetalDanceBigFlower_Step | PetalDanceBig | Local | Attacker | ExactCallbackProfile |
| gFlamethrowerFlameSpriteTemplate | AnimToTargetInSinWave | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimToTargetInSinWave_Step | SinWaveProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gFlashCannonBallMovementTemplate | AnimShadowBall | 1 / SingleSequence | 1 / SingleSequence | 41 (MotionProfile) | AnimShadowBall_Step | ShadowBall | Local | Attacker | ExactCallbackProfile |
| gFlashCannonGrayChargeTemplate | AnimGrowingChargeOrb | 1 / SingleSequence | 3 / DefaultInitialSequence | 172 (affine=172) | - | Static | Local | Argument0 | ExplicitReviewedCallback |
| gFlashingHitSplatSpriteTemplate | AnimFlashingHitSplat | 1 / SingleSequence | 4 / Argument3 | 14 (MotionProfile) | AnimFlashingHitSplat_Step | FlashingHitFlicker | Local | Argument2 | ExactCallbackProfile |
| gFlatterConfettiSpriteTemplate | AnimFlatterConfetti | 12 / DefaultInitialSequence | 0 / NoAffine | 31 (MotionProfile) | AnimFlatterConfetti_Step | ConfettiBallistic | Local | Target | ExactCallbackProfile |
| gFlatterSpotlightSpriteTemplate | AnimFlatterSpotlight | 1 / SingleSequence | 2 / DefaultInitialSequence | 123 (affine=21) | AnimFlatterSpotlight_Step | FlatterSpotlightMask | Local | Target | ExactCallbackProfile |
| gFlyBallAttackSpriteTemplate | AnimFlyBallAttack | 1 / SingleSequence | 2 / Constant1 | 64 (MotionProfile) | AnimFlyBallAttack_Step | OffscreenFly | AttackerToTarget | Target | ExactCallbackProfile |
| gFlyBallUpSpriteTemplate | AnimFlyBallUp | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | AnimFlyBallUp_Step | FlyBallRise | Local | Attacker | ExactCallbackProfile |
| gFlyingSandCrescentSpriteTemplate | AnimFlyingSandCrescent | 1 / SingleSequence | 0 / NoAffine | 192 (MotionProfile) | - | FlyingSandCrescent | Local | Target | ExactCallbackProfile |
| gFocusPunchFistSpriteTemplate | AnimFocusPunchFist | 5 / DefaultInitialSequence | 1 / SingleSequence | 48 (affine=9) | - | FocusPunchWobble | Local | Target | ExactCallbackProfile |
| gFollowMeFingerSpriteTemplate | AnimFollowMeFinger | 1 / SingleSequence | 2 / DefaultInitialSequence | 74 (affine=74) | AnimFollowMeFinger_Step1, AnimFollowMeFinger_Step2, AnimMetronomeFinger_Step | FollowMeFinger | Local | Target | ExactCallbackProfile |
| gForcePalmSpriteTemplate | AnimForcePalm | 1 / SingleSequence | 4 / Argument3 | 64 (affine=9) | - | Static | Local | Argument2 | ExplicitReviewedCallback |
| gForesightMagnifyingGlassSpriteTemplate | AnimForesightMagnifyingGlass | 1 / SingleSequence | 0 / NoAffine | 90 (MotionProfile) | AnimForesightMagnifyingGlass_Step | ForesightScan | Local | Attacker | ExactCallbackProfile |
| gForestsCurseIngrainTemplate | AnimWaterPulseBubble | 4 / DefaultInitialSequence | 0 / NoAffine | 38 (MotionProfile) | AnimWaterPulseBubble_Step | WaterPulseBubble | Local | Target | ExactCallbackProfile |
| gFoulPlayImpactTemplate | AnimHitSplatBasic | 1 / SingleSequence | 4 / Argument3 | 9 (affine=9) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gFoulPlayRingTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 2 / DefaultInitialSequence | 31 (frame=1+affine=31) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gFreezeShockCircleTemplate | AnimGrowingShockWaveOrb | 1 / SingleSequence | 3 / Constant2 | 172 (affine=172) | - | Static | Local | Attacker | ExplicitReviewedCallback |
| gFreezeShockIceBallTemplate | TranslateAnimSpriteToTargetMonLocation | 2 / DefaultInitialSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gFrenzyPlantRootSpriteTemplate | AnimFrenzyPlantRoot | 4 / Argument4 | 0 / NoAffine | 64 (MotionProfile) | AnimRootFlickerOut | FrenzyPlantRoot | Local | Target | ExactCallbackProfile |
| gFrostBreathBlueBreathTemplate | AnimDragonFireToTarget | 2 / Constant1 | 2 / Constant1 | 21 (MotionProfile) | - | DragonFire | AttackerToTarget | Target | ExactCallbackProfile |
| gFrostBreathBlueRageTemplate | AnimDragonRageFirePlume | 1 / SingleSequence | 0 / NoAffine | 25 (frame=25) | - | OrientedStatic | Local | Argument0 | ExactCallbackProfile |
| gFurySwipesSpriteTemplate | AnimFurySwipes | 2 / Argument2 | 0 / NoAffine | 16 (frame=16) | - | Static | Local | Target | ExplicitReviewedCallback |
| gFusionBoltBallTemplate | AnimFlyBallAttack | 1 / SingleSequence | 2 / Constant1 | 64 (MotionProfile) | AnimFlyBallAttack_Step | OffscreenFly | AttackerToTarget | Target | ExactCallbackProfile |
| gFusionFlareRedBallTemplate | AnimStompFoot | 1 / SingleSequence | 0 / NoAffine | 23 (MotionProfile) | AnimStompFootStep, AnimStompFootEnd, StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | StompDrop | Local | Target | ExactCallbackProfile |
| gFusionFlareRedBallUpTemplate | AnimWeatherBallUp | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimWeatherBallUp_Step | WeatherBallRise | Local | Attacker | ExactCallbackProfile |
| gFusionFlareRedBubblesTemplate | AnimWaterPulseBubble | 2 / DefaultInitialSequence | 0 / NoAffine | 38 (MotionProfile) | AnimWaterPulseBubble_Step | WaterPulseBubble | Local | Target | ExactCallbackProfile |
| gFusionFlareRedRingTemplate | AnimUproarRing | 1 / SingleSequence | 2 / Constant1 | 31 (frame=1+affine=31) | AnimSpriteOnMonPos | OrientedStatic | Local | Target | ExactCallbackProfile |
| gGearGrindTemplate | AnimBite | 1 / SingleSequence | 1 / SingleSequence | 16 (MotionProfile) | AnimBite_Step1, AnimBite_Step2 | BiteClamp | Local | Target | ExactCallbackProfile |
| gGeomancyBlueCellRaiseTemplate | AnimRaiseSprite | 2 / Argument4 | 0 / NoAffine | 64 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | RaiseSprite | Local | Attacker | ExactCallbackProfile |
| gGeomancyBlueCellVortexTemplate | AnimParticleInVortex | 2 / DefaultInitialSequence | 0 / NoAffine | 30 (MotionProfile) | AnimParticleInVortex_Step | Vortex | Local | Argument6 | ExactCallbackProfile |
| gGeomancyGreenCellRaiseTemplate | AnimRaiseSprite | 2 / Argument4 | 0 / NoAffine | 64 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | RaiseSprite | Local | Attacker | ExactCallbackProfile |
| gGeomancyGreenCellVortexTemplate | AnimParticleInVortex | 2 / DefaultInitialSequence | 0 / NoAffine | 30 (MotionProfile) | AnimParticleInVortex_Step | Vortex | Local | Argument6 | ExactCallbackProfile |
| gGeomancyRedCellRaiseTemplate | AnimRaiseSprite | 2 / Argument4 | 0 / NoAffine | 64 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | RaiseSprite | Local | Attacker | ExactCallbackProfile |
| gGeomancyRedCellVortexTemplate | AnimParticleInVortex | 2 / DefaultInitialSequence | 0 / NoAffine | 30 (MotionProfile) | AnimParticleInVortex_Step | Vortex | Local | Argument6 | ExactCallbackProfile |
| gGeomancyRingTemplate | SpriteCB_SurroundingRing | 1 / SingleSequence | 2 / DefaultInitialSequence | 14 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | RaiseSprite | Local | Attacker | ExactCallbackProfile |
| gGeomancyYellowRageTemplate | AnimDragonRageFirePlume | 1 / SingleSequence | 0 / NoAffine | 25 (frame=25) | - | OrientedStatic | Local | Argument0 | ExactCallbackProfile |
| gGlaciateSmokeTemplate | AnimBlackSmoke | 1 / SingleSequence | 0 / NoAffine | 28 (MotionProfile) | AnimBlackSmoke_Step | SmokeDrift | Local | Target | ExactCallbackProfile |
| gGlareEyeDotSpriteTemplate | AnimGlareEyeDot | 1 / SingleSequence | 0 / NoAffine | 37 (MotionProfile) | - | Static | Local | Target | ExplicitReviewedCallback |
| gGoldRingSpriteTemplate | TranslateAnimSpriteToTargetMonLocation | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gGrantingStarsSpriteTemplate | AnimGrantingStars | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | TranslateSpriteLinearFixedPoint | LocalStarVelocity | Local | Attacker | ExactCallbackProfile |
| gGrassKnotSpriteTemplate | AnimGrassKnot | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimGrassKnotStep | TargetSinWave | Local | Target | ExactCallbackProfile |
| gGrassPledgeSpriteTemplate | AnimFlyUpTarget | 2 / DefaultInitialSequence | 0 / NoAffine | 48 (MotionProfile) | AnimFlyUpTarget_Step | Rising | Local | Target | ExplicitReviewedCallback |
| gGrassyTerrainOrbsTemplate | AnimOrbitFast | 1 / SingleSequence | 1 / SingleSequence | 52 (MotionProfile) | AnimOrbitFast_Step | FastOrbit | Local | Attacker | ExactCallbackProfile |
| gGrassyTerrainStarTemplate | AnimOrbitScatter | 1 / SingleSequence | 0 / NoAffine | 32 (MotionProfile) | AnimOrbitScatter_Step | RadialScatter | Local | Attacker | ExactCallbackProfile |
| gGreenPoisonBubble | AnimAcidPoisonBubble | 3 / Constant2 | 1 / SingleSequence | 64 (MotionProfile) | AnimAcidPoisonBubble_Step | AcidBubbleArc | AttackerToTarget | Argument3 | ExactCallbackProfile |
| gGreenPoisonDrip | AnimAcidPoisonDroplet | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetDroplet | Local | Target | ExactCallbackProfile |
| gGreenStarSpriteTemplate | AnimGreenStar | 3 / DefaultInitialSequence | 0 / NoAffine | 96 (MotionProfile) | AnimGreenStar_Callback, AnimGreenStar_Step1, AnimGreenStar_Step2 | GreenStarRise | Local | Attacker | ExactCallbackProfile |
| gGrowingChargeOrb2SpriteTemplate | AnimGrowingChargeOrb | 1 / SingleSequence | 1 / SingleSequence | 172 (affine=81) | - | Static | Local | Argument0 | ExplicitReviewedCallback |
| gGrowingChargeOrbSpriteTemplate | AnimGrowingChargeOrb | 1 / SingleSequence | 3 / DefaultInitialSequence | 172 (affine=172) | - | Static | Local | Argument0 | ExplicitReviewedCallback |
| gGrowingShockWaveOrbSpriteTemplate | AnimGrowingShockWaveOrb | 1 / SingleSequence | 3 / Constant2 | 172 (affine=172) | - | Static | Local | Attacker | ExplicitReviewedCallback |
| gGrudgeFlameSpriteTemplate | AnimGrudgeFlame | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | - | GrudgeFlameWave | Local | Target | ExactCallbackProfile |
| gGuardRingSpriteTemplate | AnimGuardRing | 1 / SingleSequence | 2 / Constant1 | 14 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | GuardRingRise | Local | Attacker | ExactCallbackProfile |
| gGuardSwapOrbs1Template | AnimMimicOrb | 1 / SingleSequence | 4 / DefaultInitialSequence | 64 (affine=9) | InitAndRunAnimFastLinearTranslation, AnimFastTranslateLinearWaitEnd | MimicOrb | TargetToAttacker | Target | ExactCallbackProfile |
| gGuardSwapOrbs2Template | AnimMissileArc | 1 / SingleSequence | 4 / DefaultInitialSequence | 31 (MotionProfile) | AnimMissileArc_Step | StandardArcProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gGuillotineSpriteTemplate | AnimGuillotinePincer | 2 / Argument0 | 0 / NoAffine | 70 (frame=5) | AnimGuillotinePincer_Step1, AnimGuillotinePincer_Step2, AnimGuillotinePincer_Step3 | GuillotinePincer | Local | Target | ExactCallbackProfile |
| gGunkShoParticlesSpriteTemplate | AnimGunkShotParticles | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimGunkShotParticlesStep | GunkShotParticleWave | Local | Target | ExactCallbackProfile |
| gGunkShotImpactSpriteTemplate | AnimGunkShotImpact | 1 / SingleSequence | 4 / Argument3 | 64 (affine=9) | - | Static | Local | Argument2 | ExplicitReviewedCallback |
| gGustToTargetSpriteTemplate | AnimGustToTarget | 1 / SingleSequence | 1 / SingleSequence | 31 (affine=25) | AnimGustToTarget_Step | StandardLinearProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gHandleInvertHitSplatSpriteTemplate | AnimHitSplatHandleInvert | 1 / SingleSequence | 4 / Argument3 | 16 (MotionProfile) | - | InvertedHitSplatExact | Local | Argument2 | ExactCallbackProfile |
| gHappyHourCoinShowerTemplate | AnimHappyHourCoinShower | 1 / SingleSequence | 0 / NoAffine | 18 (MotionProfile) | AnimFallingRock_Step, TranslateSpriteInEllipse, TranslateSpriteInEllipse | Falling | Local | Attacker | SourceVerticalMotion |
| gHealBellMusicNoteSpriteTemplate | AnimHealBellMusicNote | 1 / SingleSequence | 0 / NoAffine | 36 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | HealBellNote | Local | Attacker | ExactCallbackProfile |
| gHealingBlueStarSpriteTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 21 (frame=21) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gHeartStampSpinningHeartTemplate | AnimSpinningKickOrPunch | 2 / Argument2 | 1 / SingleSequence | 96 (MotionProfile) | AnimSpinningKickOrPunchFinish | SpinningKickOrPunchExact | Local | Target | ExactCallbackProfile |
| gHeartSwapOrbSpriteTemplate | AnimSkillSwapOrb | 1 / SingleSequence | 4 / DefaultInitialSequence | 64 (MotionProfile) | - | PreinitializedArc | Local | Target | ExactCallbackProfile |
| gHeatCrashEruptionRockTemplate | AnimStompFoot | 1 / SingleSequence | 0 / NoAffine | 23 (MotionProfile) | AnimStompFootStep, AnimStompFootEnd, StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | StompDrop | Local | Target | ExactCallbackProfile |
| gHelpingHandClapSpriteTemplate | AnimHelpingHandClap | 2 / DefaultInitialSequence | 0 / NoAffine | 92 (MotionProfile) | AnimHelpingHandClap_Step | HelpingHandClap | Local | Target | ExactCallbackProfile |
| gHiddenPowerOrbScatterSpriteTemplate | AnimOrbitScatter | 1 / SingleSequence | 1 / SingleSequence | 32 (MotionProfile) | AnimOrbitScatter_Step | RadialScatter | Local | Attacker | ExactCallbackProfile |
| gHiddenPowerOrbSpriteTemplate | AnimOrbitFast | 1 / SingleSequence | 1 / SingleSequence | 52 (MotionProfile) | AnimOrbitFast_Step | FastOrbit | Local | Attacker | ExactCallbackProfile |
| gHoldBackRingTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 2 / DefaultInitialSequence | 31 (frame=1+affine=31) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gHoldBackStarsTemplate | AnimDizzyPunchDuck | 1 / SingleSequence | 0 / NoAffine | 42 (MotionProfile) | - | DizzyWave | Local | Target | ExactCallbackProfile |
| gHoldBackSwipeTemplate | AnimRevengeScratch | 3 / DefaultInitialSequence | 0 / NoAffine | 24 (frame=12) | - | RevengeScratchExact | Local | Argument2 | ExactCallbackProfile |
| gHoldHandsHeartTemplate | AnimPetalDanceBigFlower | 1 / SingleSequence | 0 / NoAffine | 141 (MotionProfile) | AnimPetalDanceBigFlower_Step | PetalDanceBig | Local | Attacker | ExactCallbackProfile |
| gHorizontalLungeSpriteTemplate | DoHorizontalLunge | 1 / SingleSequence | 0 / NoAffine | 12 (MotionProfile) | ReverseHorizontalLungeDirection, TranslateSpriteLinearById, TranslateSpriteLinearById | BattlerLunge | Local | Target | ExactCallbackProfile |
| gHornHitSpriteTemplate | AnimHornHit | 1 / SingleSequence | 0 / NoAffine | 18 (MotionProfile) | AnimHornHit_Step | HornLunge | Local | Target | ExactCallbackProfile |
| gHornLeechHornTemplate | AnimShadowBall | 1 / SingleSequence | 0 / NoAffine | 41 (MotionProfile) | AnimShadowBall_Step | ShadowBall | Local | Attacker | ExactCallbackProfile |
| gHydroCannonBeamSpriteTemplate | AnimHydroCannonBeam | 1 / SingleSequence | 1 / SingleSequence | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | StandardLinearProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gHydroCannonChargeSpriteTemplate | AnimHydroCannonCharge | 1 / SingleSequence | 1 / SingleSequence | 80 (affine=80) | AnimHydroCannonCharge_Step | HydroCannonChargeExact | Local | Attacker | ExactCallbackProfile |
| gHydroPumpOrbSpriteTemplate | AnimToTargetInSinWave | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimToTargetInSinWave_Step | SinWaveProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gHyperBeamOrbSpriteTemplate | AnimHyperBeamOrb | 7 / DefaultInitialSequence | 0 / NoAffine | 31 (MotionProfile) | AnimHyperBeamOrb_Step | HyperBeamOrb | AttackerToTarget | Target | ExactCallbackProfile |
| gHyperVoiceRingSpriteTemplate | AnimHyperVoiceRing | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | AnimHyperVoiceRing_WaitEnd | HyperVoiceRing | AttackerToTarget | Target | ExactCallbackProfile |
| gHyperspaceHoleImpactTemplate | AnimIceEffectParticle | 1 / SingleSequence | 1 / SingleSequence | 20 (affine=17) | AnimFlickerIceEffectParticle | IceImpact | Local | Target | ExactCallbackProfile |
| gIceBallChunkSpriteTemplate | InitIceBallAnim | 2 / DefaultInitialSequence | 5 / DefaultInitialSequence | 31 (frame=16) | AnimThrowIceBall | StandardArcProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gIceBeamInnerCrystalSpriteTemplate | AnimIceBeamParticle | 1 / SingleSequence | 1 / SingleSequence | 20 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | IceBeamParticle | AttackerToTarget | Target | ExactCallbackProfile |
| gIceBeamOuterCrystalSpriteTemplate | AnimIceBeamParticle | 1 / SingleSequence | 0 / NoAffine | 20 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | IceBeamParticle | AttackerToTarget | Target | ExactCallbackProfile |
| gIceBurnSmokeTemplate | AnimBlackSmoke | 1 / SingleSequence | 0 / NoAffine | 28 (MotionProfile) | AnimBlackSmoke_Step | SmokeDrift | Local | Target | ExactCallbackProfile |
| gIceCrystalHitLargeSpriteTemplate | AnimIceEffectParticle | 1 / SingleSequence | 1 / SingleSequence | 20 (affine=17) | AnimFlickerIceEffectParticle | IceImpact | Local | Target | ExactCallbackProfile |
| gIceCrystalHitSmallSpriteTemplate | AnimIceEffectParticle | 1 / SingleSequence | 1 / SingleSequence | 20 (affine=17) | AnimFlickerIceEffectParticle | IceImpact | Local | Target | ExactCallbackProfile |
| gIceCrystalSpiralInwardLarge | AnimIcePunchSwirlingParticle | 1 / SingleSequence | 1 / SingleSequence | 30 (MotionProfile) | TranslateSpriteInGrowingCircle | GrowingCircle | Local | Target | ExactCallbackProfile |
| gIceCrystalSpiralInwardSmall | AnimIcePunchSwirlingParticle | 1 / SingleSequence | 0 / NoAffine | 30 (MotionProfile) | TranslateSpriteInGrowingCircle | GrowingCircle | Local | Target | ExactCallbackProfile |
| gIceGroundSpikeSpriteTemplate | AnimWaveFromCenterOfTarget | 1 / SingleSequence | 0 / NoAffine | 35 (frame=35) | - | OrientedStatic | Local | Target | ExactCallbackProfile |
| gIceHammerPunchStompTemplate | AnimStompFoot | 5 / DefaultInitialSequence | 0 / NoAffine | 23 (MotionProfile) | AnimStompFootStep, AnimStompFootEnd, StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | StompDrop | Local | Target | ExactCallbackProfile |
| gIceShardSpriteTemplate | SpriteCB_MaxFlutterby | 1 / SingleSequence | 2 / DefaultInitialSequence | 16 (MotionProfile) | SpriteCB_MaxFlutterbyStep1, SpriteCB_MaxFlutterbyStep2 | MaxFlutterbyArc | Local | Target | ExactCallbackProfile |
| gIcicleCrashSpearTemplate | AnimFallingRock | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | AnimFallingRock_Step, TranslateSpriteInEllipse, TranslateSpriteInEllipse | FallingRockEllipse | Local | Target | ExactCallbackProfile |
| gIcicleSpearSpriteTemplate | AnimMissileArc | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimMissileArc_Step | StandardArcProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gIncinerateSpriteTemplate | TranslateAnimSpriteToTargetMonLocation | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gInfestationBubbleTemplate | AnimParticleInVortex | 2 / DefaultInitialSequence | 0 / NoAffine | 30 (MotionProfile) | AnimParticleInVortex_Step | Vortex | Local | Argument6 | ExactCallbackProfile |
| gIngrainOrbSpriteTemplate | AnimIngrainOrb | 1 / SingleSequence | 0 / NoAffine | 96 (MotionProfile) | - | IngrainOrbWave | Local | Attacker | ExactCallbackProfile |
| gIngrainRootSpriteTemplate | AnimIngrainRoot | 4 / Argument3 | 0 / NoAffine | 64 (MotionProfile) | AnimRootFlickerOut | IngrainRoot | Local | Attacker | ExactCallbackProfile |
| gIonSpriteTemplate | AnimIon | 1 / SingleSequence | 0 / NoAffine | 18 (frame=18) | AnimIon_Step | Static | Local | Target | ExplicitReviewedCallback |
| gJaggedMusicNoteSpriteTemplate | AnimJaggedMusicNote | 2 / OamTileArgument3 | 0 / NoAffine | 18 (MotionProfile) | AnimJaggedMusicNote_Step | RoarLine | Local | Target | ExactCallbackProfile |
| gJudgmentBlackChargeTemplate | AnimGrowingChargeOrb | 1 / SingleSequence | 1 / SingleSequence | 172 (affine=166) | - | Static | Local | Argument0 | ExplicitReviewedCallback |
| gJudgmentGrayInwardOrbsTemplate | AnimNeedleArmSpike | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimNeedleArmSpike_Step | NeedleArmSpike | Local | Argument4 | ExactCallbackProfile |
| gJudgmentGrayOutwardSpikesTemplate | AnimNeedleArmSpike | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimNeedleArmSpike_Step | NeedleArmSpike | Local | Argument4 | ExactCallbackProfile |
| gJumpKickSpriteTemplate | AnimJumpKick | 5 / Argument6 | 0 / NoAffine | 18 (MotionProfile) | - | DiagonalStrike | Local | Target | ExactCallbackProfile |
| gKarateChopSpriteTemplate | AnimSlideHandOrFootToTarget | 5 / Argument6 | 0 / NoAffine | 18 (MotionProfile) | - | DiagonalStrike | Local | Target | ExactCallbackProfile |
| gKinesisZapEnergySpriteTemplate | AnimKinesisZapEnergy | 1 / SingleSequence | 0 / NoAffine | 42 (frame=42) | - | OrientedStatic | Local | Attacker | ExactCallbackProfile |
| gKnockOffStrikeSpriteTemplate | AnimKnockOffStrike | 1 / SingleSequence | 2 / Constant1 | 34 (frame=8) | AnimKnockOffStrike_Step | KnockOffStrike | Local | Target | ExactCallbackProfile |
| gLandsWrathVortexTemplate | AnimParticleInVortex | 1 / SingleSequence | 1 / SingleSequence | 30 (MotionProfile) | AnimParticleInVortex_Step | Vortex | Local | Argument6 | ExactCallbackProfile |
| gLargeFlameScatterSpriteTemplate | AnimLargeFlame | 1 / SingleSequence | 0 / NoAffine | 36 (MotionProfile) | AnimLargeFlame_Step | FlameDrift | Local | Target | ExactCallbackProfile |
| gLargeFlameSpriteTemplate | AnimLargeFlame | 1 / SingleSequence | 1 / SingleSequence | 36 (MotionProfile) | AnimLargeFlame_Step | FlameDrift | Local | Target | ExactCallbackProfile |
| gLavaPlumeSpriteTemplate | AnimLavaPlumeOrbitScatter | 1 / SingleSequence | 1 / SingleSequence | 20 (MotionProfile) | AnimLavaPlumeOrbitScatterStep | LavaPlumeDrift | Local | Attacker | ExactCallbackProfile |
| gLeafBladeSpriteTemplate | SpriteCallbackDummy | 7 / DefaultInitialSequence | 0 / NoAffine | 255 (MotionProfile) | - | Static | Local | Target | ExplicitReviewedCallback |
| gLeafTornadoVortexTemplate | AnimParticleInVortex | 2 / DefaultInitialSequence | 0 / NoAffine | 30 (MotionProfile) | AnimParticleInVortex_Step | Vortex | Local | Argument6 | ExactCallbackProfile |
| gLeechLifeNeedleSpriteTemplate | AnimLeechLifeNeedle | 1 / SingleSequence | 3 / Constant2 | 13 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | LeechLifeNeedle | Local | Target | ExactCallbackProfile |
| gLeechSeedSpriteTemplate | AnimLeechSeed | 2 / DefaultInitialSequence | 0 / NoAffine | 96 (MotionProfile) | AnimLeechSeed_Step, AnimLeechSeedSprouts | LeechSeedArc | AttackerToTarget | Target | ExactCallbackProfile |
| gLeerSpriteTemplate | AnimLeer | 1 / SingleSequence | 0 / NoAffine | 15 (frame=15) | - | OrientedStatic | Local | Attacker | ExactCallbackProfile |
| gLetterZSpriteTemplate | AnimLetterZ | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | - | SleepLetterWave | Local | Attacker | ExactCallbackProfile |
| gLickSpriteTemplate | AnimLick | 1 / SingleSequence | 0 / NoAffine | 51 (frame=10) | AnimLick_Step | LickFlicker | Local | Target | ExactCallbackProfile |
| gLightOfRuinPinkDischargeTemplate | AnimElectricPuff | 1 / SingleSequence | 0 / NoAffine | 12 (frame=12) | - | Static | Local | Argument0 | ExplicitReviewedCallback |
| gLightOfRuinPinkExplosionTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 20 (frame=20) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gLightOfRuinPinkOrbsTemplate | AnimHyperBeamOrb | 7 / DefaultInitialSequence | 0 / NoAffine | 31 (MotionProfile) | AnimHyperBeamOrb_Step | HyperBeamOrb | AttackerToTarget | Target | ExactCallbackProfile |
| gLightScreenWallSpriteTemplate | AnimDefensiveWall | 1 / SingleSequence | 0 / NoAffine | 62 (MotionProfile) | AnimDefensiveWall_Step2, AnimDefensiveWall_Step3, AnimDefensiveWall_Step4, AnimDefensiveWall_Step5 | DefensiveWallCycle | Local | Attacker | ExactCallbackProfile |
| gLightningSpriteTemplate | AnimLightning | 1 / SingleSequence | 0 / NoAffine | 28 (frame=28) | AnimLightning_Step | OrientedStatic | Local | Target | ExactCallbackProfile |
| gLinearStingerSpriteTemplate | AnimTranslateStinger | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | StandardLinearProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gLockOnMoveTargetSpriteTemplate | AnimLockOnMoveTarget | 1 / SingleSequence | 0 / NoAffine | 58 (MotionProfile) | AnimLockOnTarget, AnimLockOnTarget_Step1, AnimLockOnTarget_Step2, AnimLockOnTarget_Step3, AnimLockOnTarget_Step4, AnimLockOnTarget_Step5, AnimLockOnTarget_Step6, StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | LockOnScan | Local | Target | ExactCallbackProfile |
| gLockOnTargetSpriteTemplate | AnimLockOnTarget | 1 / SingleSequence | 0 / NoAffine | 58 (MotionProfile) | AnimLockOnTarget_Step1, AnimLockOnTarget_Step2, AnimLockOnTarget_Step3, AnimLockOnTarget_Step4, AnimLockOnTarget_Step5, AnimLockOnTarget_Step6, StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | LockOnScan | Local | Target | ExactCallbackProfile |
| gLuckyChantBigStarsSpriteTemplate | AnimPetalDanceBigFlower | 1 / SingleSequence | 0 / NoAffine | 141 (MotionProfile) | AnimPetalDanceBigFlower_Step | PetalDanceBig | Local | Attacker | ExactCallbackProfile |
| gLuckyChantSmallStarsSpriteTemplate | AnimPetalDanceSmallFlower | 1 / SingleSequence | 0 / NoAffine | 101 (MotionProfile) | AnimPetalDanceSmallFlower_Step | PetalDanceSmall | Local | Attacker | ExactCallbackProfile |
| gLusterPurgeCircleSpriteTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 1 / SingleSequence | 121 (frame=1+affine=121) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gMagentaHeartSpriteTemplate | AnimMagentaHeart | 1 / SingleSequence | 0 / NoAffine | 60 (MotionProfile) | - | RisingHeartWave | Local | Attacker | ExactCallbackProfile |
| gMagicCoatWallSpriteTemplate | AnimDefensiveWall | 1 / SingleSequence | 0 / NoAffine | 62 (MotionProfile) | AnimDefensiveWall_Step2, AnimDefensiveWall_Step3, AnimDefensiveWall_Step4, AnimDefensiveWall_Step5 | DefensiveWallCycle | Local | Attacker | ExactCallbackProfile |
| gMagneticFluxUproarTemplate | AnimUproarRing | 1 / SingleSequence | 2 / Constant1 | 31 (frame=1+affine=31) | AnimSpriteOnMonPos | OrientedStatic | Local | Target | ExactCallbackProfile |
| gMatBlockGreenConversionTemplate | AnimConversion | 1 / SingleSequence | 1 / SingleSequence | 255 (MotionProfile) | - | ConversionHold | Local | Attacker | ExactCallbackProfile |
| gMeanLookEyeSpriteTemplate | AnimMeanLookEye | 1 / SingleSequence | 2 / DefaultInitialSequence | 48 (affine=7) | AnimMeanLookEye_Step1, AnimMeanLookEye_Step2, AnimMeanLookEye_Step3, AnimMeanLookEye_Step4 | SpotlightSway | Local | Target | ExactCallbackProfile |
| gMegaPunchKickSpriteTemplate | AnimSpinningKickOrPunch | 5 / Argument2 | 1 / SingleSequence | 96 (MotionProfile) | AnimSpinningKickOrPunchFinish | SpinningKickOrPunchExact | Local | Target | ExactCallbackProfile |
| gMegahornHornSpriteTemplate | AnimMegahornHorn | 1 / SingleSequence | 3 / DefaultInitialSequence | 7 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | MegahornStrike | Local | Target | ExactCallbackProfile |
| gMetalSoundSpriteTemplate | TranslateAnimSpriteToTargetMonLocation | 1 / SingleSequence | 1 / SingleSequence | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gMeteorMashStarSpriteTemplate | AnimMeteorMashStar | 1 / SingleSequence | 0 / NoAffine | 28 (MotionProfile) | AnimMeteorMashStar_Step | MeteorMashStar | Local | Target | ExactCallbackProfile |
| gMetronomeFingerSpriteTemplate | AnimMetronomeFinger | 1 / SingleSequence | 2 / DefaultInitialSequence | 74 (affine=74) | AnimMetronomeFinger_Step | MetronomeFinger | Local | Argument0 | ExactCallbackProfile |
| gMilkBottleSpriteTemplate | AnimMilkBottle | 1 / SingleSequence | 2 / DefaultInitialSequence | 108 (MotionProfile) | AnimMilkBottle_Step1 | MilkBottleDrink | Local | Target | ExactCallbackProfile |
| gMimicOrbSpriteTemplate | AnimMimicOrb | 1 / SingleSequence | 2 / DefaultInitialSequence | 64 (affine=15) | InitAndRunAnimFastLinearTranslation, AnimFastTranslateLinearWaitEnd | MimicOrb | TargetToAttacker | Target | ExactCallbackProfile |
| gMirrorCoatWallSpriteTemplate | AnimDefensiveWall | 1 / SingleSequence | 0 / NoAffine | 62 (MotionProfile) | AnimDefensiveWall_Step2, AnimDefensiveWall_Step3, AnimDefensiveWall_Step4, AnimDefensiveWall_Step5 | DefensiveWallCycle | Local | Attacker | ExactCallbackProfile |
| gMistBallSpriteTemplate | AnimThrowMistBall | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | TranslateAnimSpriteToTargetMonLocation, StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gMistCloudSpriteTemplate | InitSwirlingFogAnim | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimSwirlingFogAnim | SwirlingFog | Local | Argument4 | ExactCallbackProfile |
| gMistyTerrainOrbsTemplate | AnimOrbitFast | 1 / SingleSequence | 1 / SingleSequence | 52 (MotionProfile) | AnimOrbitFast_Step | FastOrbit | Local | Attacker | ExactCallbackProfile |
| gMistyTerrainStarTemplate | AnimOrbitScatter | 1 / SingleSequence | 0 / NoAffine | 32 (MotionProfile) | AnimOrbitScatter_Step | RadialScatter | Local | Attacker | ExactCallbackProfile |
| gMonEdgeHitSplatSpriteTemplate | AnimHitSplatOnMonEdge | 1 / SingleSequence | 4 / Argument3 | 9 (affine=9) | - | Static | Local | Argument0 | ExplicitReviewedCallback |
| gMoonSpriteTemplate | AnimMoon | 1 / SingleSequence | 0 / NoAffine | 255 (MotionProfile) | AnimMoon_Step | MoonAbsolute | Local | Target | ExactCallbackProfile |
| gMoonlightSparkleSpriteTemplate | AnimMoonlightSparkle | 1 / SingleSequence | 0 / NoAffine | 255 (MotionProfile) | AnimMoonlightSparkle_Step | MoonlightSparkleFall | Local | Attacker | ExactCallbackProfile |
| gMovementWavesSpriteTemplate | AnimMovementWaves | 2 / DefaultInitialSequence | 0 / NoAffine | 96 (frame=32) | AnimMovementWaves_Step | MovementWavesRepeat | Local | Argument0 | ExactCallbackProfile |
| gMudBombSplash | AnimSludgeBombHitParticle | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimSludgeBombHitParticle_Step | SludgeBurst | Local | Target | ExactCallbackProfile |
| gMudBombToss | AnimThrowProjectile | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimThrowProjectile_Step | StandardArcProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gMudShotOrbSpriteTemplate | AnimToTargetInSinWave | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimToTargetInSinWave_Step | SinWaveProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gMudSlapMudSpriteTemplate | AnimDirtScatter | 1 / SingleSequence | 0 / NoAffine | 18 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | DirtProjectile | Local | Target | ExactCallbackProfile |
| gMudsportMudSpriteTemplate | AnimMudSportDirt | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimMudSportDirtFalling, AnimMudSportDirtRising | MudSportDirt | Local | Attacker | ExactCallbackProfile |
| gNeedleArmSpikeSpriteTemplate | AnimNeedleArmSpike | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimNeedleArmSpike_Step | NeedleArmSpike | Local | Argument4 | ExactCallbackProfile |
| gNightDazeVioletCirclesTemplate | AnimOrbitScatter | 1 / SingleSequence | 1 / SingleSequence | 32 (MotionProfile) | AnimOrbitScatter_Step | RadialScatter | Local | Attacker | ExactCallbackProfile |
| gNightDazeVioletRingsTemplate | AnimUproarRing | 1 / SingleSequence | 2 / Constant1 | 31 (frame=1+affine=31) | AnimSpriteOnMonPos | OrientedStatic | Local | Target | ExactCallbackProfile |
| gNightSlashLeftSpriteTemplate | AnimNightSlash | 2 / DefaultInitialSequence | 0 / NoAffine | 34 (frame=16) | AnimSlashSlice, AnimFalseSwipeSlice_Step3 | SlashFlicker | Local | Target | ExactCallbackProfile |
| gNightSlashRightSpriteTemplate | AnimNightSlash | 2 / DefaultInitialSequence | 0 / NoAffine | 34 (frame=16) | AnimSlashSlice, AnimFalseSwipeSlice_Step3 | SlashFlicker | Local | Target | ExactCallbackProfile |
| gOblivionWingBeamTemplate | TranslateAnimSpriteToTargetMonLocation | 1 / SingleSequence | 1 / SingleSequence | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gOctazookaBallSpriteTemplate | TranslateAnimSpriteToTargetMonLocation | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gOctazookaSmokeSpriteTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 20 (frame=15) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gOpeningEyeSpriteTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 88 (frame=88) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gOutrageFlameSpriteTemplate | AnimOutrageFlame | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | TranslateSpriteLinearAndFlicker | OutrageFlame | Local | Attacker | ExactCallbackProfile |
| gOverheatFlameSpriteTemplate | AnimOverheatFlame | 1 / SingleSequence | 0 / NoAffine | 30 (MotionProfile) | AnimOverheatFlame_Step | RadialLinear | Local | Attacker | ExactCallbackProfile |
| gPainSplitProjectileSpriteTemplate | AnimPainSplitProjectile | 1 / SingleSequence | 0 / NoAffine | 28 (frame=19) | - | PainSplitBounce | Local | Argument2 | ExactCallbackProfile |
| gPalmSpriteTemplate | AnimBasicFistOrFoot | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | - | BasicFistOrFootExact | Local | Argument3 | ExactCallbackProfile |
| gPencilSpriteTemplate | AnimPencil | 1 / SingleSequence | 0 / NoAffine | 68 (MotionProfile) | AnimPencil_Step | PencilScribble | Local | Target | ExactCallbackProfile |
| gPerishSongMusicNote2SpriteTemplate | AnimPerishSongMusicNote2 | 8 / DefaultInitialSequence | 3 / DefaultInitialSequence | 200 (MotionProfile) | - | PerishSongDelayedNote | Local | Target | ExactCallbackProfile |
| gPerishSongMusicNoteSpriteTemplate | AnimPerishSongMusicNote | 8 / Argument1 | 3 / Constant1 | 120 (MotionProfile) | AnimPerishSongMusicNote_Step1, AnimPerishSongMusicNote_Step2 | PerishSongNote | Local | Target | ExactCallbackProfile |
| gPersistHitSplatSpriteTemplate | AnimHitSplatPersistent | 1 / SingleSequence | 4 / Argument3 | 128 (affine=9) | - | HitSplatPersistentExact | Local | Argument2 | ExactCallbackProfile |
| gPetalBlizzardTwister1Template | AnimMoveTwisterParticle | 1 / SingleSequence | 0 / NoAffine | 96 (MotionProfile) | AnimMoveTwisterParticle_Step | TwisterParticle | Local | Target | ExactCallbackProfile |
| gPetalBlizzardTwister2Template | AnimMoveTwisterParticle | 1 / SingleSequence | 0 / NoAffine | 96 (MotionProfile) | AnimMoveTwisterParticle_Step | TwisterParticle | Local | Target | ExactCallbackProfile |
| gPetalDanceBigFlowerSpriteTemplate | AnimPetalDanceBigFlower | 1 / SingleSequence | 0 / NoAffine | 141 (MotionProfile) | AnimPetalDanceBigFlower_Step | PetalDanceBig | Local | Attacker | ExactCallbackProfile |
| gPetalDanceSmallFlowerSpriteTemplate | AnimPetalDanceSmallFlower | 1 / SingleSequence | 0 / NoAffine | 101 (MotionProfile) | AnimPetalDanceSmallFlower_Step | PetalDanceSmall | Local | Attacker | ExactCallbackProfile |
| gPinMissileSpriteTemplate | AnimMissileArc | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimMissileArc_Step | StandardArcProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gPinkHeartSpriteTemplate | AnimPinkHeart | 1 / SingleSequence | 0 / NoAffine | 102 (MotionProfile) | AnimPinkHeart_Step | PinkHeartWave | Local | Target | ExactCallbackProfile |
| gPluckParticleSpriteTemplate | AnimPluck | 1 / SingleSequence | 0 / NoAffine | 32 (MotionProfile) | AnimPluckParticle | PluckParticleJump | Local | Target | ExactCallbackProfile |
| gPoisonBubbleSpriteTemplate | AnimBubbleEffect | 1 / SingleSequence | 1 / SingleSequence | 30 (affine=21) | AnimBubbleEffect_Step | BubbleRise | Local | Target | ExactCallbackProfile |
| gPoisonGasCloudSpriteTemplate | InitPoisonGasCloudAnim | 1 / SingleSequence | 0 / NoAffine | 160 (MotionProfile) | MovePoisonGasCloud | PoisonGasCloud | AttackerToTarget | Target | ExactCallbackProfile |
| gPoisonJabProjectileSpriteTemplate | AnimPoisonJabProjectile | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | PoisonJabProjectile | Local | Target | ExactCallbackProfile |
| gPoisonPowderParticleSpriteTemplate | AnimMovePowderParticle | 1 / SingleSequence | 0 / NoAffine | 96 (MotionProfile) | AnimMovePowderParticle_Step | PowderParticle | Local | Target | ExactCallbackProfile |
| gPowderBlackSporeTemplate | AnimSporeParticle | 2 / Argument4 | 0 / NoAffine | 96 (MotionProfile) | AnimSporeParticle_Step | SporeOrbit | Local | Target | ExactCallbackProfile |
| gPowderSnowSnowballSpriteTemplate | AnimMoveParticleBeyondTarget | 1 / SingleSequence | 0 / NoAffine | 18 (MotionProfile) | AnimWiggleParticleTowardsTarget | WavyBeyondTarget | AttackerToTarget | Argument7 | ExactCallbackProfile |
| gPowerAbsorptionOrbSpriteTemplate | AnimPowerAbsorptionOrb | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | AbsorptionLinear | TargetToAttacker | Target | ExactCallbackProfile |
| gPowerGemOrbScatterSpriteTemplate | AnimOrbitScatter | 1 / SingleSequence | 1 / SingleSequence | 32 (MotionProfile) | AnimOrbitScatter_Step | RadialScatter | Local | Attacker | ExactCallbackProfile |
| gPowerGemOrbSpriteTemplate | AnimOrbitFast | 1 / SingleSequence | 1 / SingleSequence | 52 (MotionProfile) | AnimOrbitFast_Step | FastOrbit | Local | Attacker | ExactCallbackProfile |
| gPowerOrbs_Float | AnimSporeParticle | 1 / SingleSequence | 0 / NoAffine | 96 (MotionProfile) | AnimSporeParticle_Step | SporeOrbit | Local | Target | ExactCallbackProfile |
| gPowerSplitOrbs1Template | AnimMimicOrb | 1 / SingleSequence | 4 / DefaultInitialSequence | 64 (affine=9) | InitAndRunAnimFastLinearTranslation, AnimFastTranslateLinearWaitEnd | MimicOrb | TargetToAttacker | Target | ExactCallbackProfile |
| gPowerSplitOrbs2Template | AnimMissileArc | 1 / SingleSequence | 4 / DefaultInitialSequence | 31 (MotionProfile) | AnimMissileArc_Step | StandardArcProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gPowerSwapGuardSwapSpriteTemplate | AnimMovePowerSwapGuardSwap | 6 / Argument2 | 0 / NoAffine | 31 (MotionProfile) | AnimMovePowerSwapGuardSwapWait | PowerSwapGuardSwapArc | Local | Argument3 | ExactCallbackProfile |
| gPowerTrickSpriteTemplate | SpriteCB_SpriteOnMonForDuration | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | AnimBrickBreakWall_Step | SpriteOnMonDuration | Local | Target | ExactCallbackProfile |
| gPresentHealParticleSpriteTemplate | AnimPresentHealParticle | 1 / SingleSequence | 0 / NoAffine | 32 (frame=16) | - | PresentHealRise | Local | Target | ExactCallbackProfile |
| gPresentSpriteTemplate | AnimPresent | 1 / SingleSequence | 2 / DefaultInitialSequence | 34 (MotionProfile) | AnimItemSteal_Step1, AnimItemSteal_Step2 | PresentBounce | Local | Target | ExactCallbackProfile |
| gProtectSpriteTemplate | AnimProtect | 1 / SingleSequence | 0 / NoAffine | 48 (MotionProfile) | AnimProtect_Step, DestroyAnimSpriteAndDisableBlend | ProtectSlide | Local | Attacker | ExactCallbackProfile |
| gPsychUpSpiralSpriteTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 1 / SingleSequence | 121 (frame=1+affine=121) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gPsychoBoostOrbSpriteTemplate | AnimPsychoBoost | 1 / SingleSequence | 2 / DefaultInitialSequence | 198 (affine=198) | - | PsychoBoostRise | Local | Attacker | ExactCallbackProfile |
| gPsychoCutSpiralSpriteTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 1 / SingleSequence | 121 (frame=1+affine=121) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gPsychoCutSpriteTemplate | AnimPsychoCut | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | PsychoCutStrike | Local | Target | ExactCallbackProfile |
| gPsyshockOrbSpriteTemplate | AnimPoisonJabProjectile | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | PoisonJabProjectile | Local | Target | ExactCallbackProfile |
| gPsyshockSmokeSpriteTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 20 (frame=15) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gPsywaveRingSpriteTemplate | AnimToTargetInSinWave | 1 / SingleSequence | 1 / SingleSequence | 31 (MotionProfile) | AnimToTargetInSinWave_Step | SinWaveProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gPunishmentImpactSpriteTemplate | AnimPunishment | 1 / SingleSequence | 4 / Argument3 | 64 (affine=9) | - | Static | Local | Argument2 | ExplicitReviewedCallback |
| gPunishmentSpriteTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 20 (frame=20) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gQuashArmHitTemplate | AnimStompFoot | 1 / SingleSequence | 0 / NoAffine | 23 (MotionProfile) | AnimStompFootStep, AnimStompFootEnd, StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | StompDrop | Local | Target | ExactCallbackProfile |
| gQuestionMarkSpriteTemplate | AnimQuestionMark | 1 / SingleSequence | 1 / SingleSequence | 102 (frame=54+affine=48+sequential) | AnimQuestionMark_Step1, AnimQuestionMark_Step2 | QuestionMarkExact | Local | Attacker | ExactCallbackProfile |
| gQuickGuardArmImpactTemplate | AnimBasicFistOrFoot | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | - | BasicFistOrFootExact | Local | Argument3 | ExactCallbackProfile |
| gRagePowderRedPowderTemplate | AnimSporeParticle | 2 / Argument4 | 0 / NoAffine | 96 (MotionProfile) | AnimSporeParticle_Step | SporeOrbit | Local | Target | ExactCallbackProfile |
| gRainDropSpriteTemplate | AnimRainDrop | 1 / SingleSequence | 0 / NoAffine | 18 (frame=18) | AnimRainDrop_Step | RainDrop | Local | Target | ExactCallbackProfile |
| gRandomPosHitSplatSpriteTemplate | AnimHitSplatRandom | 1 / SingleSequence | 4 / Argument1 | 24 (affine=9) | - | RandomBattlerHit | Local | Argument0 | ExactCallbackProfile |
| gRapidSpinSpriteTemplate | AnimRapidSpin | 1 / SingleSequence | 0 / NoAffine | 36 (MotionProfile) | AnimRapidSpin_Step | RapidSpin | Local | Argument0 | ExactCallbackProfile |
| gRazorLeafCutterSpriteTemplate | AnimTranslateLinearSingleSineWave | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimTranslateLinearSingleSineWave_Step | StandardArcProjectile | AttackerToTarget | Argument6 | ExactCallbackProfile |
| gRazorLeafParticleSpriteTemplate | AnimRazorLeafParticle | 2 / DefaultInitialSequence | 0 / NoAffine | 128 (MotionProfile) | AnimRazorLeafParticle_Step1, AnimRazorLeafParticle_Step2 | RazorLeafDrift | Local | Attacker | ExactCallbackProfile |
| gRazorShellTemplate | SpriteCB_SpriteOnMonForDuration | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | AnimBrickBreakWall_Step | SpriteOnMonDuration | Local | Target | ExactCallbackProfile |
| gRazorWindTornadoSpriteTemplate | AnimRazorWindTornado | 1 / SingleSequence | 1 / SingleSequence | 30 (MotionProfile) | TranslateSpriteInCircle | Circular | Local | Attacker | ExactCallbackProfile |
| gReallyBigRockBlastRockSpriteTemplate | AnimRockBlastRock | 1 / SingleSequence | 0 / NoAffine | 26 (MotionProfile) | - | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gRecycleSpriteTemplate | AnimRecycle | 1 / SingleSequence | 1 / SingleSequence | 139 (MotionProfile) | AnimRecycle_Step | RecycleFade | Local | Attacker | ExactCallbackProfile |
| gRedHeartBurstSpriteTemplate | AnimParticleBurst | 1 / SingleSequence | 0 / NoAffine | 42 (MotionProfile) | - | ParticleBurstWave | Local | Target | ExactCallbackProfile |
| gRedHeartCharmSpriteTemplate | AnimMagentaHeart | 1 / SingleSequence | 0 / NoAffine | 60 (MotionProfile) | - | RisingHeartWave | Local | Attacker | ExactCallbackProfile |
| gRedHeartProjectileSpriteTemplate | AnimRedHeartProjectile | 1 / SingleSequence | 0 / NoAffine | 96 (MotionProfile) | AnimRedHeartProjectile_Step | HeartWaveProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gRedHeartRisingSpriteTemplate | AnimRedHeartRising | 1 / SingleSequence | 0 / NoAffine | 96 (MotionProfile) | AnimRedHeartRising_Step | RedHeartRising | Local | Target | ExactCallbackProfile |
| gRedXSpriteTemplate | AnimRedX | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimRedX_Step | TimedRedX | Local | Attacker | ExactCallbackProfile |
| gReflectSparkleSpriteTemplate | AnimWallSparkle | 1 / SingleSequence | 0 / NoAffine | 20 (frame=15) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gReflectTypeBlueRingTemplate | SpriteCB_SurroundingRing | 1 / SingleSequence | 2 / DefaultInitialSequence | 14 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | RaiseSprite | Local | Attacker | ExactCallbackProfile |
| gReflectTypeBlueStringTemplate | SpriteCB_ToxicThreadWrap | 1 / SingleSequence | 0 / NoAffine | 51 (MotionProfile) | AnimStringWrap_Step | StringWrapBlink | Local | Target | ExactCallbackProfile |
| gReflectTypePinkRingTemplate | SpriteCB_SurroundingRing | 1 / SingleSequence | 2 / DefaultInitialSequence | 14 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | RaiseSprite | Local | Attacker | ExactCallbackProfile |
| gReflectTypeVioletRingTemplate | SpriteCB_SurroundingRing | 1 / SingleSequence | 2 / DefaultInitialSequence | 14 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | RaiseSprite | Local | Attacker | ExactCallbackProfile |
| gReflectTypeVioletStringTemplate | SpriteCB_ToxicThreadWrap | 1 / SingleSequence | 0 / NoAffine | 51 (MotionProfile) | AnimStringWrap_Step | StringWrapBlink | Local | Target | ExactCallbackProfile |
| gReflectTypeWhiteRingTemplate | SpriteCB_SurroundingRing | 1 / SingleSequence | 2 / DefaultInitialSequence | 14 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | RaiseSprite | Local | Attacker | ExactCallbackProfile |
| gReflectTypeWhiteStringTemplate | SpriteCB_ToxicThreadWrap | 1 / SingleSequence | 0 / NoAffine | 51 (MotionProfile) | AnimStringWrap_Step | StringWrapBlink | Local | Target | ExactCallbackProfile |
| gReflectWallSpriteTemplate | AnimDefensiveWall | 1 / SingleSequence | 0 / NoAffine | 62 (MotionProfile) | AnimDefensiveWall_Step2, AnimDefensiveWall_Step3, AnimDefensiveWall_Step4, AnimDefensiveWall_Step5 | DefensiveWallCycle | Local | Attacker | ExactCallbackProfile |
| gRevengeBigScratchSpriteTemplate | AnimRevengeScratch | 3 / DefaultInitialSequence | 0 / NoAffine | 24 (frame=12) | - | RevengeScratchExact | Local | Argument2 | ExactCallbackProfile |
| gRevengeSmallScratchSpriteTemplate | AnimRevengeScratch | 3 / DefaultInitialSequence | 0 / NoAffine | 24 (frame=12) | - | RevengeScratchExact | Local | Argument2 | ExactCallbackProfile |
| gReversalOrbSpriteTemplate | AnimReversalOrb | 1 / SingleSequence | 0 / NoAffine | 52 (MotionProfile) | AnimReversalOrb_Step | FastOrbit | Local | Attacker | ExactCallbackProfile |
| gRoarNoiseLineSpriteTemplate | AnimRoarNoiseLine | 2 / Constant1 | 0 / NoAffine | 14 (MotionProfile) | AnimRoarNoiseLine_Step | RoarLine | Local | Attacker | ExactCallbackProfile |
| gRoarOfTimeBombTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 20 (frame=20) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gRockBlastRockSpriteTemplate | AnimRockBlastRock | 6 / DefaultInitialSequence | 2 / Constant1 | 26 (MotionProfile) | - | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gRockFragmentSpriteTemplate | AnimRockFragment | 3 / Argument5 | 0 / NoAffine | 96 (MotionProfile) | TranslateSpriteLinearFixedPoint | RockFragmentLinear | Local | Target | ExactCallbackProfile |
| gRockPolishSparkleSpriteTemplate | AnimRockPolishSparkle | 1 / SingleSequence | 0 / NoAffine | 32 (frame=28) | - | Static | Local | Attacker | ExplicitReviewedCallback |
| gRockPolishStreakSpriteTemplate | AnimRockPolishStreak | 1 / SingleSequence | 16 / DefaultInitialSequence | 27 (frame=27) | - | RockPolishStreak | Local | Attacker | ExactCallbackProfile |
| gRockScatterSpriteTemplate | AnimRockScatter | 6 / Argument3 | 2 / DefaultInitialSequence | 19 (MotionProfile) | AnimRockScatter_Step | RockScatterBounce | Local | Target | ExactCallbackProfile |
| gRockTombRockSpriteTemplate | AnimRockTomb | 6 / Argument4 | 0 / NoAffine | 38 (MotionProfile) | AnimRockTomb_Step | RockTombBounce | Local | Target | ExactCallbackProfile |
| gRolloutMudSpriteTemplate | AnimRolloutParticle | 1 / SingleSequence | 0 / NoAffine | 48 (MotionProfile) | - | PreinitializedArc | Local | Target | ExactCallbackProfile |
| gRolloutRockSpriteTemplate | AnimRolloutParticle | 1 / SingleSequence | 0 / NoAffine | 48 (MotionProfile) | - | PreinitializedArc | Local | Target | ExactCallbackProfile |
| gSacredSwordBladesTemplate | AnimFireSpread | 2 / DefaultInitialSequence | 0 / NoAffine | 64 (MotionProfile) | TranslateSpriteLinearFixedPoint | LocalFixedVelocity | Local | Target | ExactCallbackProfile |
| gSacredSwordCutTemplate | AnimCuttingSlice | 1 / SingleSequence | 0 / NoAffine | 23 (MotionProfile) | AnimSlice_Step | SliceArc | Local | Target | ExactCallbackProfile |
| gSandAttackDirtSpriteTemplate | AnimDirtScatter | 1 / SingleSequence | 0 / NoAffine | 18 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | DirtProjectile | Local | Target | ExactCallbackProfile |
| gScratchSpriteTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 20 (frame=20) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gScreechRingSpriteTemplate | TranslateAnimSpriteToTargetMonLocation | 1 / SingleSequence | 1 / SingleSequence | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gSearingShotEruptionImpactTemplate | SpriteCB_SearingShotRock | 5 / Argument2 | 1 / SingleSequence | 96 (MotionProfile) | AnimSpinningKickOrPunchFinish | SpinningKickOrPunchExact | Local | Target | ExactCallbackProfile |
| gSearingShotEruptionRockTemplate | AnimOverheatFlame | 1 / SingleSequence | 0 / NoAffine | 30 (MotionProfile) | AnimOverheatFlame_Step | RadialLinear | Local | Attacker | ExactCallbackProfile |
| gSearingShotRedChargeTemplate | AnimGrowingChargeOrb | 1 / SingleSequence | 3 / DefaultInitialSequence | 172 (affine=172) | - | Static | Local | Argument0 | ExplicitReviewedCallback |
| gSecretSwordBladesTemplate | AnimFireSpread | 2 / DefaultInitialSequence | 0 / NoAffine | 64 (MotionProfile) | TranslateSpriteLinearFixedPoint | LocalFixedVelocity | Local | Target | ExactCallbackProfile |
| gSeedFlareGreenChargeTemplate | AnimGrowingChargeOrb | 1 / SingleSequence | 3 / DefaultInitialSequence | 172 (affine=172) | - | Static | Local | Argument0 | ExplicitReviewedCallback |
| gSeedFlareGreenCirclesTemplate | AnimPowerAbsorptionOrb | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | AbsorptionLinear | TargetToAttacker | Target | ExactCallbackProfile |
| gSeedFlareGreenWavesTemplate | AnimFlyingSandCrescent | 1 / SingleSequence | 0 / NoAffine | 192 (MotionProfile) | - | FlyingSandCrescent | Local | Target | ExactCallbackProfile |
| gShadowBallSpriteTemplate | AnimShadowBall | 1 / SingleSequence | 1 / SingleSequence | 41 (MotionProfile) | AnimShadowBall_Step | ShadowBall | Local | Attacker | ExactCallbackProfile |
| gShadowSneakImpactSpriteTemplate | AnimIceEffectParticle | 1 / SingleSequence | 1 / SingleSequence | 20 (affine=17) | AnimFlickerIceEffectParticle | IceImpact | Local | Target | ExactCallbackProfile |
| gShakeMonOrPlatformSpriteTemplate | AnimShakeMonOrBattlePlatforms | 1 / SingleSequence | 0 / NoAffine | 24 (MotionProfile) | AnimShakeMonOrBattlePlatforms_Step | BattlerShake | Local | Target | ExactCallbackProfile |
| gShakeMonOrTerrainSpriteTemplate | AnimShakeMonOrBattleTerrain | 1 / SingleSequence | 0 / NoAffine | 24 (MotionProfile) | AnimShakeMonOrBattleTerrain_Step | BattlerShake | Local | Attacker | ExactCallbackProfile |
| gSharpTeethSpriteTemplate | AnimBite | 1 / SingleSequence | 8 / Argument2 | 16 (MotionProfile) | AnimBite_Step1, AnimBite_Step2 | BiteClamp | Local | Target | ExactCallbackProfile |
| gSharpenSphereSpriteTemplate | AnimSharpenSphere | 1 / SingleSequence | 0 / NoAffine | 192 (frame=192) | AnimSharpenSphere_Step | SharpenBlink | Local | Attacker | ExactCallbackProfile |
| gShellSmashLeftShellSpriteTemplate | SpriteCB_LockingJaw | 1 / SingleSequence | 8 / Argument2 | 40 (MotionProfile) | SpriteCB_LockingJawStep, SpriteCB_LockingJawFinish | LockingJaw | Local | Target | ExactCallbackProfile |
| gShellSmashPurpleRocksSpriteTemplate | AnimRockFragment | 3 / Argument5 | 0 / NoAffine | 96 (MotionProfile) | TranslateSpriteLinearFixedPoint | RockFragmentLinear | Local | Target | ExactCallbackProfile |
| gShellSmashRightShellSpriteTemplate | SpriteCB_LockingJaw | 1 / SingleSequence | 8 / Argument2 | 40 (MotionProfile) | SpriteCB_LockingJawStep, SpriteCB_LockingJawFinish | LockingJaw | Local | Target | ExactCallbackProfile |
| gShiftGearGearsTemplate | SpriteCB_SpriteOnMonForDuration | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | AnimBrickBreakWall_Step | SpriteOnMonDuration | Local | Target | ExactCallbackProfile |
| gSignalBeamGreenOrbSpriteTemplate | AnimToTargetInSinWave | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimToTargetInSinWave_Step | SinWaveProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gSignalBeamRedOrbSpriteTemplate | AnimToTargetInSinWave | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimToTargetInSinWave_Step | SinWaveProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gSilverWindBigSparkSpriteTemplate | AnimFlyingParticle | 1 / SingleSequence | 1 / SingleSequence | 96 (MotionProfile) | AnimFlyingParticle_Step | FlyingParticle | Local | Target | ExactCallbackProfile |
| gSilverWindMediumSparkSpriteTemplate | AnimFlyingParticle | 1 / SingleSequence | 1 / SingleSequence | 96 (MotionProfile) | AnimFlyingParticle_Step | FlyingParticle | Local | Target | ExactCallbackProfile |
| gSilverWindSmallSparkSpriteTemplate | AnimFlyingParticle | 1 / SingleSequence | 1 / SingleSequence | 96 (MotionProfile) | AnimFlyingParticle_Step | FlyingParticle | Local | Target | ExactCallbackProfile |
| gSimpleBeamBrownRingTemplate | AnimUproarRing | 1 / SingleSequence | 2 / Constant1 | 31 (frame=1+affine=31) | AnimSpriteOnMonPos | OrientedStatic | Local | Target | ExactCallbackProfile |
| gSimpleBeamBrownTemplate | TranslateAnimSpriteToTargetMonLocation | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gSimpleBeamPinkRingTemplate | AnimUproarRing | 1 / SingleSequence | 2 / Constant1 | 31 (frame=1+affine=31) | AnimSpriteOnMonPos | OrientedStatic | Local | Target | ExactCallbackProfile |
| gSimpleBeamPinkTemplate | TranslateAnimSpriteToTargetMonLocation | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gSimplePaletteBlendSpriteTemplate | AnimSimplePaletteBlend | 1 / SingleSequence | 0 / NoAffine | 24 (MotionProfile) | AnimSimplePaletteBlend_Step | PalettePulse | Local | Attacker | ExactCallbackProfile |
| gSkyAttackBirdSpriteTemplate | AnimSkyAttackBird | 1 / SingleSequence | 0 / NoAffine | 30 (MotionProfile) | AnimSkyAttackBird_Step | SkyAttackBird | AttackerToTarget | Target | ExactCallbackProfile |
| gSkyDropFlyBallTemplate | AnimThrowMistBall | 1 / SingleSequence | 1 / SingleSequence | 31 (MotionProfile) | TranslateAnimSpriteToTargetMonLocation, StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gSkyDropTargetFlyingTemplate | AnimSkyDropBallUp | 1 / SingleSequence | 1 / SingleSequence | 18 (MotionProfile) | AnimFlyBallUp_Step | Falling | Local | Target | SourceVerticalMotion |
| gSlamHitSpriteTemplate | AnimWhipHit | 2 / Constant1 | 0 / NoAffine | 32 (frame=15) | AnimWhipHit_WaitEnd | WhipHitExact | Local | Target | ExactCallbackProfile |
| gSlashSliceSpriteTemplate | AnimSlashSlice | 2 / DefaultInitialSequence | 0 / NoAffine | 34 (frame=16) | AnimFalseSwipeSlice_Step3 | SlashFlicker | Local | Argument0 | ExactCallbackProfile |
| gSleepLetterZSpriteTemplate | AnimSleepLetterZ | 1 / SingleSequence | 2 / Constant1 | 64 (MotionProfile) | AnimSleepLetterZ_Step | SleepLetter | Local | Attacker | ExactCallbackProfile |
| gSleepPowderParticleSpriteTemplate | AnimMovePowderParticle | 1 / SingleSequence | 0 / NoAffine | 96 (MotionProfile) | AnimMovePowderParticle_Step | PowderParticle | Local | Target | ExactCallbackProfile |
| gSlideMonToOffsetAndBackSpriteTemplate | SlideMonToOffsetAndBack | 1 / SingleSequence | 0 / NoAffine | 16 (MotionProfile) | SlideMonToOffsetAndBack_End, TranslateSpriteLinearByIdFixedPoint | BattlerSlideToOffsetAndBack | Local | Target | ExactCallbackProfile |
| gSlideMonToOffsetPartnerSpriteTemplate | SlideMonToOffsetPartner | 1 / SingleSequence | 0 / NoAffine | 16 (MotionProfile) | TranslateSpriteLinearByIdFixedPoint | BattlerSlideToOffset | Local | Target | ExactCallbackProfile |
| gSlideMonToOffsetSpriteTemplate | SlideMonToOffset | 1 / SingleSequence | 0 / NoAffine | 16 (MotionProfile) | TranslateSpriteLinearByIdFixedPoint | BattlerSlideToOffset | Local | Target | ExactCallbackProfile |
| gSlideMonToOriginalPosPartnerSpriteTemplate | SlideMonToOriginalPosPartner | 1 / SingleSequence | 0 / NoAffine | 16 (MotionProfile) | SlideMonToOriginalPos_Step | BattlerRestorePosition | Local | Target | ExactCallbackProfile |
| gSlideMonToOriginalPosSpriteTemplate | SlideMonToOriginalPos | 1 / SingleSequence | 0 / NoAffine | 16 (MotionProfile) | SlideMonToOriginalPos_Step | BattlerRestorePosition | Local | Target | ExactCallbackProfile |
| gSlidingKickSpriteTemplate | AnimSlidingKick | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimSlidingKick_Step | SlidingKickWave | Local | Target | ExactCallbackProfile |
| gSlowFlyingMusicNotesSpriteTemplate | AnimSlowFlyingMusicNotes | 8 / Argument1 | 1 / SingleSequence | 41 (MotionProfile) | AnimSlowFlyingMusicNotes_Step | MusicNoteWave | Local | Attacker | ExactCallbackProfile |
| gSludgeBombHitParticleSpriteTemplate | AnimSludgeBombHitParticle | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | AnimSludgeBombHitParticle_Step | SludgeBurst | Local | Target | ExactCallbackProfile |
| gSludgeProjectileSpriteTemplate | AnimSludgeProjectile | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | AnimSludgeProjectile_Step | SludgeArc | AttackerToTarget | Argument3 | ExactCallbackProfile |
| gSmallBubblePairSpriteTemplate | AnimSmallBubblePair | 1 / SingleSequence | 0 / NoAffine | 30 (MotionProfile) | AnimSmallBubblePair_Step | BubbleRise | Local | Argument3 | ExactCallbackProfile |
| gSmallCloudTemplate | AnimMoveSmallCloud | 1 / SingleSequence | 4 / Argument2 | 64 (affine=22) | AnimMoveSmallCloudAnimate | SmallCloudDrift | Local | Target | ExactCallbackProfile |
| gSmallDriftingBubblesSpriteTemplate | AnimSmallDriftingBubbles | 1 / SingleSequence | 0 / NoAffine | 21 (MotionProfile) | AnimSmallDriftingBubbles_Step | DriftingBubble | Local | Target | ExactCallbackProfile |
| gSmallWaterOrbSpriteTemplate | AnimSmallWaterOrb | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | - | SmallWaterOrb | Local | Target | ExactCallbackProfile |
| gSmellingSaltExclamationSpriteTemplate | AnimSmellingSaltExclamation | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimSmellingSaltExclamation_Step | SmellingSaltsBlink | Local | Argument0 | ExactCallbackProfile |
| gSmellingSaltsHandSpriteTemplate | AnimSmellingSaltsHand | 1 / SingleSequence | 0 / NoAffine | 54 (MotionProfile) | AnimSmellingSaltsHand_Step | SmellingSalts | Local | Target | ExactCallbackProfile |
| gSmogCloudSpriteTemplate | InitSwirlingFogAnim | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimSwirlingFogAnim | SwirlingFog | Local | Argument4 | ExactCallbackProfile |
| gSnoreZSpriteTemplate | AnimTravelDiagonally | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TravelDiagonal | Local | Argument5 | ExactCallbackProfile |
| gSoftBoiledEggSpriteTemplate | AnimSoftBoiledEgg | 3 / DefaultInitialSequence | 3 / DefaultInitialSequence | 64 (affine=56) | AnimSoftBoiledEgg_Step1, AnimSoftBoiledEgg_Step2, AnimSoftBoiledEgg_Step3, AnimSoftBoiledEgg_Step3_Callback1, AnimSoftBoiledEgg_Step3_Callback2, AnimSoftBoiledEgg_Step4, AnimSoftBoiledEgg_Step4_Callback | SoftBoiledEgg | Local | Attacker | ExactCallbackProfile |
| gSolarBeamBigOrbSpriteTemplate | AnimSolarBeamBigOrb | 7 / Argument3 | 0 / NoAffine | 96 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | SolarBeamBigOrb | AttackerToTarget | Target | ExactCallbackProfile |
| gSolarBeamSmallOrbSpriteTemplate | AnimSolarBeamSmallOrb | 1 / SingleSequence | 0 / NoAffine | 81 (MotionProfile) | AnimSolarBeamSmallOrb_Step | SolarBeamOrb | AttackerToTarget | Target | ExactCallbackProfile |
| gSonicBoomSpriteTemplate | AnimSonicBoomProjectile | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | StandardLinearProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gSpacialRendBladesTemplate | AnimFireSpiralOutward | 1 / SingleSequence | 0 / NoAffine | 30 (MotionProfile) | AnimFireSpiralOutward_Step1, AnimFireSpiralOutward_Step2 | ExpandingSpiral | Local | Attacker | ExactCallbackProfile |
| gSpacialRendBladesTemplate2 | AnimFireSpread | 2 / DefaultInitialSequence | 0 / NoAffine | 64 (MotionProfile) | TranslateSpriteLinearFixedPoint | LocalFixedVelocity | Local | Target | ExactCallbackProfile |
| gSparkBeamSpriteTemplate | AnimToTargetInSinWave | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimToTargetInSinWave_Step | SinWaveProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gSparkElectricityFlashingSpriteTemplate | AnimSparkElectricityFlashing | 3 / OamTileArgument6 | 1 / SingleSequence | 32 (MotionProfile) | AnimSparkElectricityFlashing_Step | ElectricOrbit | Local | Target | ExactCallbackProfile |
| gSparkElectricitySpriteTemplate | AnimSparkElectricity | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | - | StaticElectricity | Local | Target | ExactCallbackProfile |
| gSparklingStarsSpriteTemplate | AnimSparklingStars | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | TranslateSpriteLinearFixedPoint | LocalStarVelocity | Local | Target | ExactCallbackProfile |
| gSpecialScreenSparkleSpriteTemplate | AnimWallSparkle | 1 / SingleSequence | 0 / NoAffine | 20 (frame=20) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gSpeedDustSpriteTemplate | AnimSpeedDust | 1 / SingleSequence | 0 / NoAffine | 15 (frame=15) | - | Static | Local | Target | ExplicitReviewedCallback |
| gSpiderWebSpriteTemplate | AnimSpiderWeb | 1 / SingleSequence | 1 / SingleSequence | 53 (MotionProfile) | AnimSpiderWeb_Step, AnimSpiderWeb_End | SpiderWebFade | Local | Target | ExactCallbackProfile |
| gSpikesSpriteTemplate | AnimSpikes | 1 / SingleSequence | 0 / NoAffine | 96 (MotionProfile) | AnimSpikes_Step1, AnimSpikes_Step2 | SpikesArc | AttackerToTarget | Target | ExactCallbackProfile |
| gSpinningBoneSpriteTemplate | AnimBoneHitProjectile | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocalLinear | Local | Target | ExactCallbackProfile |
| gSpinningHandOrFootSpriteTemplate | AnimSpinningKickOrPunch | 5 / Argument2 | 1 / SingleSequence | 96 (MotionProfile) | AnimSpinningKickOrPunchFinish | SpinningKickOrPunchExact | Local | Target | ExactCallbackProfile |
| gSpinningSparkleSpriteTemplate | AnimSpinningSparkle | 1 / SingleSequence | 0 / NoAffine | 15 (frame=15) | - | OrientedStatic | Local | Attacker | ExactCallbackProfile |
| gSpinningVineSpriteTemplate | AnimBoneHitProjectile | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocalLinear | Local | Target | ExactCallbackProfile |
| gSpitUpOrbSpriteTemplate | AnimSpitUpOrb | 1 / SingleSequence | 1 / SingleSequence | 24 (MotionProfile) | AnimSpitUpOrb_Step | RadialScatter | Local | Attacker | ExactCallbackProfile |
| gSporeParticleSpriteTemplate | AnimSporeParticle | 2 / Argument4 | 0 / NoAffine | 96 (MotionProfile) | AnimSporeParticle_Step | SporeOrbit | Local | Target | ExactCallbackProfile |
| gSpotlightSpriteTemplate | AnimSpotlight | 1 / SingleSequence | 2 / DefaultInitialSequence | 64 (affine=21) | AnimSpotlight_Step1, AnimSpotlight_Step2 | SpotlightSway | Local | Target | ExactCallbackProfile |
| gSprayWaterDropletSpriteTemplate | AnimSprayWaterDroplet | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimSprayWaterDroplet_Step | WaterSprayBallistic | Local | Argument1 | ExactCallbackProfile |
| gStealthRockSpriteTemplate | AnimStealthRock | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimStealthRockStep, AnimStealthRockStep2 | StealthRockArc | Local | Target | ExactCallbackProfile |
| gSteamEruptionBreathTemplate | AnimDragonFireToTarget | 2 / Constant1 | 2 / Constant1 | 21 (MotionProfile) | - | DragonFire | AttackerToTarget | Target | ExactCallbackProfile |
| gStockpileAbsorptionOrbSpriteTemplate | AnimPowerAbsorptionOrb | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | AbsorptionLinear | TargetToAttacker | Target | ExactCallbackProfile |
| gStompFootSpriteTemplate | AnimStompFoot | 1 / SingleSequence | 0 / NoAffine | 23 (MotionProfile) | AnimStompFootStep, AnimStompFootEnd, StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | StompDrop | Local | Target | ExactCallbackProfile |
| gStoneEdgeSpriteTemplate | AnimParticleInVortex | 1 / SingleSequence | 0 / NoAffine | 30 (MotionProfile) | AnimParticleInVortex_Step | Vortex | Local | Argument6 | ExactCallbackProfile |
| gStringWrapSpriteTemplate | AnimStringWrap | 1 / SingleSequence | 0 / NoAffine | 51 (MotionProfile) | AnimStringWrap_Step | StringWrapBlink | Local | Target | ExactCallbackProfile |
| gStunSporeParticleSpriteTemplate | AnimMovePowderParticle | 1 / SingleSequence | 0 / NoAffine | 96 (MotionProfile) | AnimMovePowderParticle_Step | PowderParticle | Local | Target | ExactCallbackProfile |
| gSuckerPunchSpriteTemplate | AnimSuckerPunch | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimSuckerPunchStep | TargetSinWave | Local | Target | ExactCallbackProfile |
| gSunlightRaySpriteTemplate | AnimSunlight | 1 / SingleSequence | 1 / SingleSequence | 61 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | SunlightRay | AttackerToTarget | Target | ExactCallbackProfile |
| gSuperFangSpriteTemplate | AnimSuperFang | 1 / SingleSequence | 0 / NoAffine | 8 (frame=8) | - | Static | Local | Target | ExplicitReviewedCallback |
| gSuperpowerFireballSpriteTemplate | AnimSuperpowerFireball | 1 / SingleSequence | 0 / NoAffine | 17 (MotionProfile) | AnimTranslateLinear_WithFollowup | SuperpowerFireball | AttackerToTarget | Target | ExactCallbackProfile |
| gSuperpowerOrbSpriteTemplate | AnimSuperpowerOrb | 1 / SingleSequence | 1 / SingleSequence | 197 (MotionProfile) | AnimSuperpowerOrb_Step, AnimTranslateLinear_WithFollowup | SuperpowerOrbHold | Local | Target | ExactCallbackProfile |
| gSuperpowerRockSpriteTemplate | AnimSuperpowerRock | 5 / OamTileArgument2 | 0 / NoAffine | 44 (MotionProfile) | AnimSuperpowerRock_Step1, AnimSuperpowerRock_Step2 | SuperpowerRock | Local | Target | ExactCallbackProfile |
| gSupersonicRingSpriteTemplate | TranslateAnimSpriteToTargetMonLocation | 1 / SingleSequence | 1 / SingleSequence | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gSwallowBlueOrbSpriteTemplate | AnimSwallowBlueOrb | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | - | SwallowOrb | Local | Attacker | ExactCallbackProfile |
| gSweetScentPetalSpriteTemplate | AnimSweetScentPetal | 3 / Argument1 | 0 / NoAffine | 52 (MotionProfile) | AnimSweetScentPetal_Step | SweetScentPetal | Local | Target | ExactCallbackProfile |
| gSwiftStarSpriteTemplate | AnimTranslateLinearSingleSineWave | 1 / SingleSequence | 1 / SingleSequence | 31 (MotionProfile) | AnimTranslateLinearSingleSineWave_Step | StandardArcProjectile | AttackerToTarget | Argument6 | ExactCallbackProfile |
| gSwirlingDirtSpriteTemplate | AnimParticleInVortex | 1 / SingleSequence | 0 / NoAffine | 30 (MotionProfile) | AnimParticleInVortex_Step | Vortex | Local | Argument6 | ExactCallbackProfile |
| gSwirlingSnowballSpriteTemplate | AnimSwirlingSnowball | 1 / SingleSequence | 0 / NoAffine | 20 (MotionProfile) | AnimSwirlingSnowball_Step1, AnimSwirlingSnowball_Step2, AnimSwirlingSnowball_End, InitAnimFastLinearTranslationWithSpeedAndPos, AnimFastTranslateLinearWaitEnd | SwirlingSnowball | AttackerToTarget | Argument5 | ExactCallbackProfile |
| gSwordsDanceBladeSpriteTemplate | AnimSwordsDanceBlade | 1 / SingleSequence | 1 / SingleSequence | 45 (affine=45) | AnimSwordsDanceBlade_Step, StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | SwordRise | Local | Attacker | ExactCallbackProfile |
| gSynchronoiseAeroWheelTemplate | AnimFireSpread | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | TranslateSpriteLinearFixedPoint | LocalFixedVelocity | Local | Target | ExactCallbackProfile |
| gSynchronoiseBlueRingTemplate | AnimHyperVoiceRing | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | AnimHyperVoiceRing_WaitEnd | HyperVoiceRing | AttackerToTarget | Target | ExactCallbackProfile |
| gSynchronoiseVioletRingTemplate | AnimHyperVoiceRing | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | AnimHyperVoiceRing_WaitEnd | HyperVoiceRing | AttackerToTarget | Target | ExactCallbackProfile |
| gSynchronoiseYellowRingTemplate | AnimHyperVoiceRing | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | AnimHyperVoiceRing_WaitEnd | HyperVoiceRing | AttackerToTarget | Target | ExactCallbackProfile |
| gTailGlowOrbSpriteTemplate | AnimTailGlowOrb | 1 / SingleSequence | 1 / SingleSequence | 115 (affine=115) | - | TailGlowOrbExact | Local | Argument0 | ExactCallbackProfile |
| gTailSlapTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 20 (frame=20) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gTauntFingerSpriteTemplate | AnimTauntFinger | 4 / DefaultInitialSequence | 0 / NoAffine | 47 (frame=28) | AnimTauntFinger_Step1, AnimTauntFinger_Step2 | TauntFinger | Local | Argument0 | ExactCallbackProfile |
| gTealAlertSpriteTemplate | AnimTealAlert | 1 / SingleSequence | 0 / NoAffine | 7 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetConverge | Local | Target | ExactCallbackProfile |
| gTearDropSpriteTemplate | AnimTearDrop | 1 / SingleSequence | 2 / Constant1 | 33 (MotionProfile) | AnimTearDrop_Step | TearDropArc | Local | Target | ExactCallbackProfile |
| gTechnoBlastWhiteBlastTemplate | TranslateAnimSpriteToTargetMonLocation | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gTechnoBlastWhiteChargeTemplate | AnimGrowingChargeOrb | 1 / SingleSequence | 3 / DefaultInitialSequence | 172 (affine=172) | - | Static | Local | Argument0 | ExplicitReviewedCallback |
| gTechnoBlastWhiteCircleTemplate | AnimPowerAbsorptionOrb | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | AbsorptionLinear | TargetToAttacker | Target | ExactCallbackProfile |
| gTechnoBlastWhiteSmokeTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 20 (frame=15) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gTechnoBlastWhiteSparkTemplate | AnimZapCannonSpark | 3 / OamTileArgument6 | 1 / SingleSequence | 31 (MotionProfile) | AnimZapCannonSpark_Step | ZapCannonSpark | AttackerToTarget | Target | ExactCallbackProfile |
| gThinRingExpandingSpriteTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 2 / DefaultInitialSequence | 31 (frame=1+affine=31) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gThinRingShrinkingSpriteTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 1 / SingleSequence | 31 (frame=1+affine=31) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gThoughtBubbleSpriteTemplate | AnimThoughtBubble | 4 / DefaultInitialSequence | 0 / NoAffine | 64 (frame=8) | AnimThoughtBubble_Step | ThoughtBubble | Local | Argument0 | ExactCallbackProfile |
| gThousandArrowsGreenArrowTemplate | AnimMudSportDirt | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimMudSportDirtFalling, AnimMudSportDirtRising | MudSportDirt | Local | Attacker | ExactCallbackProfile |
| gThousandArrowsGreenChargeTemplate | AnimGrowingChargeOrb | 1 / SingleSequence | 3 / DefaultInitialSequence | 172 (affine=172) | - | Static | Local | Argument0 | ExplicitReviewedCallback |
| gThousandArrowsGreenDischargeTemplate | AnimElectricPuff | 1 / SingleSequence | 0 / NoAffine | 12 (frame=12) | - | Static | Local | Argument0 | ExplicitReviewedCallback |
| gThousandArrowsGreenHexTemplate | SpriteCB_Geyser | 2 / DefaultInitialSequence | 0 / NoAffine | 18 (MotionProfile) | AnimMudSportDirtRising | Rising | Local | Attacker | SourceVerticalMotion |
| gThousandWavesGreenRecoverTemplate | AnimPowerAbsorptionOrb | 2 / DefaultInitialSequence | 1 / SingleSequence | 64 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | AbsorptionLinear | TargetToAttacker | Target | ExactCallbackProfile |
| gThousandWavesGreenWaveTemplate | AnimFlyingSandCrescent | 1 / SingleSequence | 0 / NoAffine | 192 (MotionProfile) | - | FlyingSandCrescent | Local | Target | ExactCallbackProfile |
| gThousandWavesGreenWheelTemplate | AnimDragonDanceOrb | 2 / DefaultInitialSequence | 0 / NoAffine | 82 (MotionProfile) | AnimDragonDanceOrb_Step | DragonDanceOrbit | Local | Attacker | ExactCallbackProfile |
| gThousandWavesPoundImpactTemplate | AnimHitSplatOnMonEdge | 1 / SingleSequence | 4 / Argument3 | 9 (affine=9) | - | Static | Local | Argument0 | ExplicitReviewedCallback |
| gThousandWavesRotatingImpactTemplate | AnimParticleInVortex | 2 / DefaultInitialSequence | 1 / SingleSequence | 30 (MotionProfile) | AnimParticleInVortex_Step | Vortex | Local | Argument6 | ExactCallbackProfile |
| gThunderWaveSpriteTemplate | AnimThunderWave | 2 / DefaultInitialSequence | 0 / NoAffine | 51 (MotionProfile) | AnimThunderWave_Step | ThunderWavePairBlink | Local | Target | ExactCallbackProfile |
| gThunderboltOrbSpriteTemplate | AnimThunderboltOrb | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | AnimThunderboltOrb_Step | ThunderboltOrbBlink | Local | Target | ExactCallbackProfile |
| gToxicBubbleSpriteTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 20 (frame=20) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gToxicSpikesSpriteTemplate | AnimSpikes | 1 / SingleSequence | 0 / NoAffine | 96 (MotionProfile) | AnimSpikes_Step1, AnimSpikes_Step2 | SpikesArc | AttackerToTarget | Target | ExactCallbackProfile |
| gTriAttackTriangleSpriteTemplate | AnimTriAttackTriangle | 1 / SingleSequence | 1 / SingleSequence | 82 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TriAttackTriangle | AttackerToTarget | Target | ExactCallbackProfile |
| gTrickBagSpriteTemplate | AnimTrickBag | 1 / SingleSequence | 4 / DefaultInitialSequence | 64 (affine=40) | AnimTrickBag_Step1, AnimTrickBag_Step2, AnimTrickBag_Step3 | TrickBagOrbit | Local | Target | ExactCallbackProfile |
| gTrumpCardParticleSpriteTempalte | AnimMoveTrumpCardParticle | 3 / Argument2 | 5 / Argument6 | 32 (MotionProfile) | AnimMoveTrumpCardParticleAlive | TrumpCardParticleScatter | Local | Target | ExactCallbackProfile |
| gTrumpCardSpriteTemplate | AnimMoveTrumpCard | 3 / Argument2 | 0 / NoAffine | 31 (MotionProfile) | AnimMoveTrumpCardArc | TrumpCardArc | Local | Target | ExactCallbackProfile |
| gTwisterLeafSpriteTemplate | AnimMoveTwisterParticle | 2 / DefaultInitialSequence | 0 / NoAffine | 96 (MotionProfile) | AnimMoveTwisterParticle_Step | TwisterParticle | Local | Target | ExactCallbackProfile |
| gTwisterRockSpriteTemplate | AnimMoveTwisterParticle | 1 / SingleSequence | 2 / DefaultInitialSequence | 96 (MotionProfile) | AnimMoveTwisterParticle_Step | TwisterParticle | Local | Target | ExactCallbackProfile |
| gUTurnBallBackSpriteTemplate | AnimAbsorptionOrb | 1 / SingleSequence | 1 / SingleSequence | 40 (MotionProfile) | AnimAbsorptionOrb_Step | AbsorptionArc | TargetToAttacker | Target | ExactCallbackProfile |
| gUTurnBallSpriteTemplate | AnimShadowBall | 1 / SingleSequence | 1 / SingleSequence | 41 (MotionProfile) | AnimShadowBall_Step | ShadowBall | Local | Attacker | ExactCallbackProfile |
| gUproarRingSpriteTemplate | AnimUproarRing | 1 / SingleSequence | 2 / Constant1 | 31 (frame=1+affine=31) | AnimSpriteOnMonPos | OrientedStatic | Local | Target | ExactCallbackProfile |
| gVCreateFlameTemplate | AnimNeedleArmSpike | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimNeedleArmSpike_Step | NeedleArmSpike | Local | Argument4 | ExactCallbackProfile |
| gVCreateRedOrbTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 0 / NoAffine | 42 (frame=42) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gVCreateRedRingTemplate | AnimSpriteOnMonPos | 1 / SingleSequence | 1 / SingleSequence | 31 (frame=1+affine=31) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gVenomDrenchAcidTemplate | AnimFallingRock | 3 / Argument1 | 0 / NoAffine | 64 (MotionProfile) | AnimFallingRock_Step, TranslateSpriteInEllipse, TranslateSpriteInEllipse | FallingRockEllipse | Local | Target | ExactCallbackProfile |
| gVerticalDipSpriteTemplate | DoVerticalDip | 1 / SingleSequence | 0 / NoAffine | 12 (MotionProfile) | ReverseVerticalDipDirection, TranslateSpriteLinearById, TranslateSpriteLinearById | BattlerDip | Local | Argument2 | ExactCallbackProfile |
| gViceGripSpriteTemplate | AnimViceGripPincer | 2 / Constant1 | 0 / NoAffine | 26 (frame=26) | AnimViceGripPincer_Step, StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | ViceGripPincer | Local | Target | ExactCallbackProfile |
| gVineWhipSpriteTemplate | AnimWhipHit | 2 / Constant1 | 0 / NoAffine | 32 (frame=15) | AnimWhipHit_WaitEnd | WhipHitExact | Local | Target | ExactCallbackProfile |
| gVoltSwitchSpriteTemplate | AnimTask_VoltSwitch | 1 / SingleSequence | 1 / SingleSequence | 18 (MotionProfile) | VoltSwitch_Step, StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | Arc | AttackerToTarget | Target | FireRedStandardHelper |
| gVoltTackleBoltSpriteTemplate | AnimVoltTackleBolt | 4 / DefaultInitialSequence | 1 / SingleSequence | 13 (MotionProfile) | - | Static | Local | Target | ExplicitReviewedCallback |
| gVoltTackleOrbSlideSpriteTemplate | AnimVoltTackleOrbSlide | 1 / SingleSequence | 3 / Constant1 | 66 (MotionProfile) | AnimVoltTackleOrbSlide_Step | VoltTackleSlide | Local | Attacker | ExactCallbackProfile |
| gWaterBubbleProjectileSpriteTemplate | AnimWaterBubbleProjectile | 1 / SingleSequence | 1 / SingleSequence | 50 (frame=11) | AnimWaterBubbleProjectile_Step1, AnimWaterBubbleProjectile_Step2, AnimWaterBubbleProjectile_Step3 | OrbitingProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gWaterBubbleSpriteTemplate | AnimBubbleEffect | 1 / SingleSequence | 1 / SingleSequence | 30 (affine=21) | AnimBubbleEffect_Step | BubbleRise | Local | Target | ExactCallbackProfile |
| gWaterGunDropletSpriteTemplate | AnimWaterGunDroplet | 1 / SingleSequence | 1 / SingleSequence | 64 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | WaterGunDroplet | Local | Target | ExactCallbackProfile |
| gWaterGunProjectileSpriteTemplate | AnimThrowProjectile | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimThrowProjectile_Step | StandardArcProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gWaterHitSplatSpriteTemplate | AnimHitSplatBasic | 1 / SingleSequence | 4 / Argument3 | 9 (affine=9) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gWaterPledgeOrbSpriteTemplate | AnimFlyUpTarget | 1 / SingleSequence | 0 / NoAffine | 48 (MotionProfile) | AnimFlyUpTarget_Step | Rising | Local | Target | ExplicitReviewedCallback |
| gWaterPulseBubbleSpriteTemplate | AnimWaterPulseBubble | 2 / DefaultInitialSequence | 0 / NoAffine | 38 (MotionProfile) | AnimWaterPulseBubble_Step | WaterPulseBubble | Local | Target | ExactCallbackProfile |
| gWaterPulseRingSpriteTemplate | AnimWaterPulseRing | 1 / SingleSequence | 1 / SingleSequence | 34 (MotionProfile) | AnimWaterPulseRing_Step | WaterPulseRing | Local | Target | ExactCallbackProfile |
| gWaterShurikenImpactTemplate | AnimHitSplatBasic | 1 / SingleSequence | 4 / Argument3 | 9 (affine=9) | - | OrientedStatic | Local | Argument2 | ExactCallbackProfile |
| gWaterShurikenRingTemplate | AnimWaterPulseRing | 1 / SingleSequence | 1 / SingleSequence | 34 (MotionProfile) | AnimWaterPulseRing_Step | WaterPulseRing | Local | Target | ExactCallbackProfile |
| gWaterShurikenStarTemplate | AnimShadowBall | 1 / SingleSequence | 1 / SingleSequence | 41 (MotionProfile) | AnimShadowBall_Step | ShadowBall | Local | Attacker | ExactCallbackProfile |
| gWavyMusicNotesSpriteTemplate | AnimWavyMusicNotes | 8 / Argument0 | 1 / SingleSequence | 41 (MotionProfile) | AnimWavyMusicNotes_Step | WavyMusicNote | Local | Target | ExactCallbackProfile |
| gWeakFrustrationAngerMarkSpriteTemplate | AnimWeakFrustrationAngerMark | 1 / SingleSequence | 0 / NoAffine | 96 (MotionProfile) | - | WeakFrustrationMark | Local | Attacker | ExactCallbackProfile |
| gWeatherBallFireDownSpriteTemplate | AnimWeatherBallDown | 1 / SingleSequence | 0 / NoAffine | 26 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | WeatherBallDown | Local | Target | ExactCallbackProfile |
| gWeatherBallIceDownSpriteTemplate | AnimWeatherBallDown | 1 / SingleSequence | 1 / SingleSequence | 26 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | WeatherBallDown | Local | Target | ExactCallbackProfile |
| gWeatherBallNormalDownSpriteTemplate | AnimWeatherBallDown | 1 / SingleSequence | 0 / NoAffine | 26 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | WeatherBallDown | Local | Target | ExactCallbackProfile |
| gWeatherBallRockDownSpriteTemplate | AnimWeatherBallDown | 1 / SingleSequence | 2 / DefaultInitialSequence | 26 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | WeatherBallDown | Local | Target | ExactCallbackProfile |
| gWeatherBallUpSpriteTemplate | AnimWeatherBallUp | 1 / SingleSequence | 0 / NoAffine | 64 (MotionProfile) | AnimWeatherBallUp_Step | WeatherBallRise | Local | Attacker | ExactCallbackProfile |
| gWeatherBallWaterDownSpriteTemplate | AnimWeatherBallDown | 1 / SingleSequence | 1 / SingleSequence | 26 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | WeatherBallDown | Local | Target | ExactCallbackProfile |
| gWebThreadSpriteTemplate | AnimTranslateWebThread | 1 / SingleSequence | 0 / NoAffine | 36 (MotionProfile) | AnimTranslateWebThread_Step | WebThreadWave | AttackerToTarget | Argument4 | ExactCallbackProfile |
| gWhirlpoolSpriteTemplate | AnimParticleInVortex | 1 / SingleSequence | 1 / SingleSequence | 30 (MotionProfile) | AnimParticleInVortex_Step | Vortex | Local | Argument6 | ExactCallbackProfile |
| gWhirlwindLineSpriteTemplate | AnimWhirlwindLine | 1 / SingleSequence | 0 / NoAffine | 18 (MotionProfile) | AnimWhirlwindLine_Step | RoarLine | Local | Argument2 | ExactCallbackProfile |
| gWhiteHaloSpriteTemplate | AnimWhiteHalo | 1 / SingleSequence | 0 / NoAffine | 99 (MotionProfile) | AnimWhiteHalo_Step1, AnimWhiteHalo_Step2 | WhiteHaloFade | Local | Target | ExactCallbackProfile |
| gWideGuardBlueConversionTemplate | AnimConversion | 1 / SingleSequence | 1 / SingleSequence | 255 (MotionProfile) | - | ConversionHold | Local | Attacker | ExactCallbackProfile |
| gWillOWispFireSpriteTemplate | AnimWillOWispFire | 1 / SingleSequence | 0 / NoAffine | 30 (MotionProfile) | - | ExpandingEllipse | Local | Target | ExactCallbackProfile |
| gWillOWispOrbSpriteTemplate | AnimWillOWispOrb | 4 / Argument2 | 0 / NoAffine | 64 (MotionProfile) | AnimWillOWispOrb_Step | WillOWispOrb | AttackerToTarget | Target | ExactCallbackProfile |
| gWishStarSpriteTemplate | AnimWishStar | 1 / SingleSequence | 0 / NoAffine | 48 (MotionProfile) | AnimWishStar_Step | WishStar | Local | Target | ExactCallbackProfile |
| gWoodHammerHammerSpriteTemplate | AnimWoodHammerHammer | 1 / SingleSequence | 4 / DefaultInitialSequence | 84 (affine=41) | AnimWoodHammerHammer_WaitForPunch, AnimWoodHammerHammer_WaitForDestruction | WoodHammerHammer | Local | Target | ExactCallbackProfile |
| gWoodHammerSmallSpriteTemplate | AnimWoodHammerSmall | 3 / Argument5 | 0 / NoAffine | 31 (MotionProfile) | TranslateSpriteLinearFixedPoint | TargetLocalLinear | Local | Target | ExactCallbackProfile |
| gWorrySeedSpriteTemplate | AnimMoveWorrySeed | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | AnimMoveWorrySeedWait | WorrySeedArc | Local | Target | ExactCallbackProfile |
| gWringOutHandSpriteTemplate | AnimMoveWringOut | 1 / SingleSequence | 0 / NoAffine | 80 (MotionProfile) | AnimMoveWringOutCircle | WringOutCircle | Local | Target | ExactCallbackProfile |
| gYawnCloudSpriteTemplate | AnimYawnCloud | 1 / SingleSequence | 3 / Argument0 | 64 (MotionProfile) | AnimYawnCloud_Step | YawnCloud | Local | Attacker | ExactCallbackProfile |
| gZapCannonBallSpriteTemplate | TranslateAnimSpriteToTargetMonLocation | 1 / SingleSequence | 0 / NoAffine | 31 (MotionProfile) | StartAnimLinearTranslation, AnimTranslateLinear_WithFollowup | TargetLocationProjectile | AttackerToTarget | Target | ExactCallbackProfile |
| gZapCannonSparkSpriteTemplate | AnimZapCannonSpark | 3 / OamTileArgument6 | 1 / SingleSequence | 31 (MotionProfile) | AnimZapCannonSpark_Step | ZapCannonSpark | AttackerToTarget | Target | ExactCallbackProfile |
| gZenHeadbuttSpriteTemplate | AnimateZenHeadbutt | 1 / SingleSequence | 1 / SingleSequence | 115 (affine=115) | - | ZenHeadbuttExact | Local | Argument0 | ExactCallbackProfile |
| sElectricBoltSegmentSpriteTemplate | AnimElectricBoltSegment | 1 / SingleSequence | 0 / NoAffine | 15 (MotionProfile) | - | Static | Local | Target | ExplicitReviewedCallback |
| sFrozenIceCubeSpriteTemplate | SpriteCallbackDummy | 1 / SingleSequence | 0 / NoAffine | 255 (MotionProfile) | - | Static | Local | Target | ExplicitReviewedCallback |
| sHailParticleSpriteTemplate | AnimHailBegin | 1 / SingleSequence | 3 / DefaultInitialSequence | 48 (MotionProfile) | AnimHailContinue | HailDiagonal | Local | Target | ExactCallbackProfile |
| sImprisonOrbSpriteTemplate | SpriteCallbackDummy | 1 / SingleSequence | 0 / NoAffine | 255 (MotionProfile) | - | Static | Local | Target | ExplicitReviewedCallback |
| sShockWaveProgressingBoltSpriteTemplate | AnimShockWaveProgressingBolt | 1 / SingleSequence | 0 / NoAffine | 13 (MotionProfile) | - | Static | Local | Target | ExplicitReviewedCallback |
| sSkillSwapOrbSpriteTemplate | AnimSkillSwapOrb | 1 / SingleSequence | 4 / DefaultInitialSequence | 64 (MotionProfile) | - | PreinitializedArc | Local | Target | ExactCallbackProfile |
| sSmokescreenImpactSpriteTemplate | SpriteCB_SmokescreenImpact | 1 / SingleSequence | 0 / NoAffine | 12 (frame=1) | - | Static | Local | Target | ExplicitReviewedCallback |

## Move coverage

| ID | Move | Branches | Sprite callbacks | Task callbacks | Retained renderers |
|---:|---|---:|---:|---:|---|
| 1 | POUND | 0 | 1 | 1 | Sprite:OrientedStatic, Task:ShakeMonExact |
| 2 | KARATE_CHOP | 0 | 2 | 1 | Sprite:DiagonalStrike, Sprite:OrientedStatic, Task:ShakeMonExact |
| 3 | DOUBLE_SLAP | 1 | 1 | 1 | Sprite:OrientedStatic, Task:ShakeMonExact |
| 4 | COMET_PUNCH | 1 | 2 | 1 | Sprite:BasicFistOrFootExact, Sprite:OrientedStatic, Task:ShakeMonExact |
| 5 | MEGA_PUNCH | 0 | 4 | 4 | Sprite:OrientedStatic, Sprite:PalettePulse, Sprite:SpinningKickOrPunchExact, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact |
| 6 | PAY_DAY | 0 | 3 | 1 | Sprite:CoinThrow, Sprite:FallingCoinBounce, Sprite:OrientedStatic, Task:ShakeMon2Exact |
| 7 | FIRE_PUNCH | 0 | 4 | 2 | Sprite:BasicFistOrFootExact, Sprite:GrowingCircle, Sprite:LocalFixedVelocity, Sprite:OrientedStatic, Task:BlendBattleAnimPalExact, Task:ShakeMonExact |
| 8 | ICE_PUNCH | 0 | 7 | 2 | Sprite:BasicFistOrFootExact, Sprite:GrowingCircle, Sprite:IceImpact, Sprite:OrientedStatic, Sprite:PalettePulse, Task:BlendBattleAnimPalExact, Task:ShakeMonExact |
| 9 | THUNDER_PUNCH | 0 | 4 | 2 | Sprite:BasicFistOrFootExact, Sprite:OrientedStatic, Sprite:PalettePulse, Task:InvertScreenColorExact, Task:ShakeMonExact |
| 10 | SCRATCH | 0 | 1 | 1 | Sprite:OrientedStatic, Task:ShakeMonExact |
| 11 | VICE_GRIP | 0 | 2 | 1 | Sprite:OrientedStatic, Sprite:ViceGripPincer, Task:ShakeMon2Exact |
| 12 | GUILLOTINE | 0 | 3 | 2 | Sprite:GuillotinePincer, Sprite:OrientedStatic, Sprite:PalettePulse, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact |
| 13 | RAZOR_WIND | 1 | 2 | 1 | Sprite:Circular, Sprite:StandardLinearProjectile, Task:ShakeMon2Exact |
| 14 | SWORDS_DANCE | 0 | 1 | 2 | Sprite:SwordRise, Task:TagFlash, Task:TranslateMonEllipticalRespectSide |
| 15 | CUT | 0 | 1 | 1 | Sprite:SliceArc, Task:ShakeMonExact |
| 16 | GUST | 0 | 2 | 2 | Sprite:EllipticalGust, Sprite:OrientedStatic, Task:ShakeMon2Exact, Task:TagPaletteRotate |
| 17 | WING_ATTACK | 0 | 4 | 2 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:StandardLinearProjectile, Task:TagPaletteRotate, Task:TranslateMonElliptical |
| 18 | WHIRLWIND | 0 | 1 | 3 | Sprite:RoarLine, Task:ShakeMon2Exact, Task:SlideOffScreenExact, Task:TranslateMonEllipticalRespectSide |
| 19 | FLY | 1 | 3 | 1 | Sprite:FlyBallRise, Sprite:OffscreenFly, Sprite:OrientedStatic, Task:ShakeMonExact |
| 20 | BIND | 0 | 0 | 2 | Task:ScaleMonAndRestore, Task:SwayMonExact |
| 21 | SLAM | 0 | 4 | 1 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:WhipHitExact, Task:ShakeMonInPlaceExact |
| 22 | VINE_WHIP | 0 | 2 | 1 | Sprite:BattlerLunge, Sprite:WhipHitExact, Task:ShakeMon2Exact |
| 23 | STOMP | 0 | 2 | 1 | Sprite:OrientedStatic, Sprite:StompDrop, Task:ShakeMonExact |
| 24 | DOUBLE_KICK | 0 | 1 | 1 | Sprite:RandomBattlerHit, Task:ShakeMonExact |
| 25 | MEGA_KICK | 0 | 4 | 4 | Sprite:OrientedStatic, Sprite:PalettePulse, Sprite:SpinningKickOrPunchExact, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact |
| 26 | JUMP_KICK | 0 | 3 | 1 | Sprite:BattlerLunge, Sprite:DiagonalStrike, Sprite:OrientedStatic, Task:ShakeMonExact |
| 27 | ROLLING_KICK | 0 | 4 | 2 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:SlidingKickWave, Task:ShakeMonExact, Task:TranslateMonEllipticalRespectSide |
| 28 | SAND_ATTACK | 0 | 3 | 0 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:DirtProjectile |
| 29 | HEADBUTT | 0 | 2 | 2 | Sprite:BattlerBow, Sprite:FlashingHitFlicker, Task:ShakeMonExact, Task:ShakeMonInPlaceExact |
| 30 | HORN_ATTACK | 0 | 3 | 2 | Sprite:BattlerBow, Sprite:FlashingHitFlicker, Sprite:HornLunge, Task:ShakeMonExact, Task:ShakeMonInPlaceExact |
| 31 | FURY_ATTACK | 1 | 2 | 2 | Sprite:FlashingHitFlicker, Sprite:HornLunge, Task:RotateMonSpriteToSide, Task:ShakeMonExact |
| 32 | HORN_DRILL | 0 | 3 | 2 | Sprite:BattlerBow, Sprite:FlashingHitFlicker, Sprite:HornLunge, Task:ShakeMonInPlaceExact, Task:SlidingBackground |
| 33 | TACKLE | 0 | 2 | 1 | Sprite:BattlerLunge, Sprite:OrientedStatic, Task:ShakeMonExact |
| 34 | BODY_SLAM | 0 | 4 | 1 | Sprite:BattlerDip, Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Task:ShakeMonInPlaceExact |
| 35 | WRAP | 0 | 0 | 2 | Task:ScaleMonAndRestore, Task:TranslateMonEllipticalRespectSide |
| 36 | TAKE_DOWN | 0 | 4 | 2 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:PalettePulse, Task:ShakeMonInPlaceExact, Task:WindUpLunge |
| 37 | THRASH | 0 | 1 | 3 | Sprite:RandomBattlerHit, Task:ShakeMonInPlaceExact, Task:ThrashMoveMonHorizontal, Task:ThrashMoveMonVertical |
| 38 | DOUBLE_EDGE | 0 | 5 | 3 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:PalettePulse, Task:RotateMonSpriteToSide, Task:ShakeMonInPlaceExact, Task:TranslateMonEllipticalRespectSide |
| 39 | TAIL_WHIP | 0 | 0 | 1 | Task:TranslateMonEllipticalRespectSide |
| 40 | POISON_STING | 0 | 3 | 1 | Sprite:BubbleRise, Sprite:OrientedStatic, Sprite:StandardLinearProjectile, Task:ShakeMon2Exact |
| 41 | TWINEEDLE | 0 | 2 | 1 | Sprite:InvertedHitSplatExact, Sprite:StandardLinearProjectile, Task:ShakeMon2Exact |
| 42 | PIN_MISSILE | 0 | 2 | 1 | Sprite:OrientedStatic, Sprite:StandardArcProjectile, Task:ShakeMon2Exact |
| 43 | LEER | 0 | 1 | 2 | Sprite:OrientedStatic, Task:ScaleMonAndRestore, Task:ShakeMon2Exact |
| 44 | BITE | 0 | 2 | 1 | Sprite:BiteClamp, Sprite:OrientedStatic, Task:ShakeMonExact |
| 45 | GROWL | 0 | 1 | 3 | Sprite:RoarLine, Task:ShakeMon2Exact |
| 46 | ROAR | 0 | 1 | 4 | Sprite:RoarLine, Task:ScaleMonAndRestore, Task:SlideOffScreenExact |
| 47 | SING | 0 | 1 | 2 | Sprite:WavyMusicNote, Task:MusicNotePaletteClear, Task:MusicNotePaletteSetup |
| 48 | SUPERSONIC | 0 | 1 | 1 | Sprite:TargetLocationProjectile, Task:ShakeMon2Exact |
| 49 | SONIC_BOOM | 0 | 2 | 1 | Sprite:OrientedStatic, Sprite:StandardLinearProjectile, Task:ShakeMonExact |
| 50 | DISABLE | 0 | 1 | 1 | Sprite:OrientedStatic, Task:GrowAndGrayscale |
| 51 | ACID | 0 | 2 | 2 | Sprite:AcidBubbleArc, Sprite:TargetDroplet, Task:BlendColorCycleExact, Task:ShakeMon2Exact |
| 52 | EMBER | 0 | 2 | 0 | Sprite:TargetLocationProjectile, Sprite:TravelDiagonal |
| 53 | FLAMETHROWER | 0 | 1 | 2 | Sprite:SinWaveProjectile, Task:ShakeMonExact |
| 54 | MIST | 0 | 1 | 1 | Sprite:SwirlingFog, Task:BlendColorCycleExact |
| 55 | WATER_GUN | 0 | 3 | 1 | Sprite:OrientedStatic, Sprite:StandardArcProjectile, Sprite:WaterGunDroplet, Task:ShakeMon2Exact |
| 56 | HYDRO_PUMP | 0 | 2 | 2 | Sprite:OrientedStatic, Sprite:SinWaveProjectile, Task:ShakeMonExact |
| 57 | SURF | 0 | 0 | 1 | Task:SurfWave |
| 58 | ICE_BEAM | 0 | 5 | 1 | Sprite:IceBeamParticle, Sprite:IceImpact, Sprite:PalettePulse, Task:ShakeMon2Exact |
| 59 | BLIZZARD | 1 | 4 | 2 | Sprite:IceImpact, Sprite:SwirlingSnowball, Sprite:WavyBeyondTarget, Task:SlidingBackground |
| 60 | PSYBEAM | 0 | 1 | 3 | Sprite:TargetLocationProjectile, Task:BlendColorCycleExact, Task:PsychicPaletteCycle, Task:SwayMonExact |
| 61 | BUBBLE_BEAM | 0 | 2 | 1 | Sprite:BubbleRise, Sprite:OrbitingProjectile, Task:SwayMonExact |
| 62 | AURORA_BEAM | 0 | 1 | 2 | Sprite:StandardLinearProjectile, Task:RotateAuroraRingColors, Task:ShakeMon2Exact |
| 63 | HYPER_BEAM | 0 | 2 | 4 | Sprite:HyperBeamOrb, Sprite:PalettePulse, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact, Task:ShakeMonExact, Task:TagFlash |
| 64 | PECK | 0 | 1 | 1 | Sprite:FlashingHitFlicker, Task:RotateMonToSideAndRestore |
| 65 | DRILL_PECK | 0 | 2 | 2 | Sprite:BattlerBow, Sprite:BattlerRestorePosition, Task:DrillPeckHitSplats, Task:ShakeMon2Exact |
| 66 | SUBMISSION | 0 | 1 | 1 | Sprite:OrientedStatic, Task:TranslateMonElliptical |
| 67 | LOW_KICK | 0 | 4 | 1 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:SlidingKickWave, Task:RotateMonSpriteToSide |
| 68 | COUNTER | 0 | 4 | 2 | Sprite:BasicFistOrFootExact, Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Task:ShakeMonExact, Task:TranslateMonEllipticalRespectSide |
| 69 | SEISMIC_TOSS | 3 | 2 | 4 | Sprite:OrientedStatic, Sprite:RockScatterBounce, Task:SeismicTossBackground, Task:SeismicTossBackgroundEnd, Task:ShakeMonExact |
| 70 | STRENGTH | 0 | 2 | 3 | Sprite:BattlerRestorePosition, Sprite:OrientedStatic, Task:ShakeAndSinkMonExact, Task:ShakeMon2Exact, Task:TranslateMonEllipticalRespectSide |
| 71 | ABSORB | 0 | 4 | 1 | Sprite:AbsorptionArc, Sprite:OrientedStatic, Sprite:PalettePulse, Task:ShakeMonExact |
| 72 | MEGA_DRAIN | 0 | 4 | 1 | Sprite:AbsorptionArc, Sprite:OrientedStatic, Sprite:PalettePulse, Task:ShakeMonExact |
| 73 | LEECH_SEED | 0 | 1 | 0 | Sprite:LeechSeedArc |
| 74 | GROWTH | 0 | 0 | 2 | Task:BlendColorCycleExact, Task:ScaleMonAndRestore |
| 75 | RAZOR_LEAF | 0 | 2 | 1 | Sprite:RazorLeafDrift, Sprite:StandardArcProjectile, Task:ShakeMon2Exact |
| 76 | SOLAR_BEAM | 1 | 2 | 6 | Sprite:AbsorptionLinear, Sprite:SolarBeamBigOrb, Task:BlendBattleAnimPalExact, Task:BlendColorCycleExact, Task:ShakeMon2Exact, Task:SolarBeamOrbs |
| 77 | POISON_POWDER | 0 | 1 | 0 | Sprite:PowderParticle |
| 78 | STUN_SPORE | 0 | 1 | 0 | Sprite:PowderParticle |
| 79 | SLEEP_POWDER | 0 | 1 | 0 | Sprite:PowderParticle |
| 80 | PETAL_DANCE | 0 | 5 | 2 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:PetalDanceBig, Sprite:PetalDanceSmall, Task:ShakeMonExact, Task:TranslateMonEllipticalRespectSide |
| 81 | STRING_SHOT | 0 | 3 | 0 | Sprite:PalettePulse, Sprite:StringWrapBlink, Sprite:WebThreadWave |
| 82 | DRAGON_RAGE | 0 | 4 | 1 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:DragonFire, Sprite:OrientedStatic, Task:ShakeMonExact |
| 83 | FIRE_SPIN | 0 | 1 | 1 | Sprite:Vortex, Task:ShakeMonExact |
| 84 | THUNDER_SHOCK | 0 | 1 | 2 | Sprite:OrientedStatic, Task:BlendBattleAnimPalExact, Task:ElectricBoltSegments |
| 85 | THUNDERBOLT | 0 | 3 | 2 | Sprite:ElectricOrbit, Sprite:OrientedStatic, Sprite:ThunderboltOrbBlink, Task:BlendBattleAnimPalExact, Task:ElectricBoltSegments |
| 86 | THUNDER_WAVE | 0 | 1 | 2 | Sprite:ThunderWavePairBlink, Task:BlendBattleAnimPalExact, Task:ElectricBoltSegments |
| 87 | THUNDER | 0 | 2 | 3 | Sprite:OrientedStatic, Sprite:PalettePulse, Task:InvertScreenColorExact, Task:ShakeTargetInPatternExact, Task:SlidingBackground |
| 88 | ROCK_THROW | 0 | 2 | 1 | Sprite:BattlerShake, Sprite:FallingRockEllipse, Task:ShakeMonExact |
| 89 | EARTHQUAKE | 0 | 1 | 1 | Sprite:PalettePulse, Task:HorizontalShakeExact |
| 90 | FISSURE | 0 | 2 | 2 | Sprite:DirtPlume, Sprite:PalettePulse, Task:FissureBackgroundPosition, Task:HorizontalShakeExact |
| 91 | DIG | 1 | 3 | 3 | Sprite:DigDirtMoundExact, Sprite:DirtPlume, Sprite:OrientedStatic, Task:DigDownMovement, Task:DigUpMovement, Task:ShakeMonExact |
| 92 | TOXIC | 0 | 2 | 0 | Sprite:BubbleRise, Sprite:OrientedStatic |
| 93 | CONFUSION | 0 | 0 | 5 | Task:BlendColorCycleExact, Task:PsychicPaletteCycle, Task:ScaleMonAndRestore, Task:ShakeMon2Exact, Task:ShakeMonExact |
| 94 | PSYCHIC | 0 | 0 | 5 | Task:BlendColorCycleExact, Task:PsychicPaletteCycle, Task:ScaleMonAndRestore, Task:ShakeMon2Exact, Task:ShakeMonExact |
| 95 | HYPNOSIS | 0 | 1 | 2 | Sprite:TargetLocationProjectile, Task:BlendColorCycleExact, Task:PsychicPaletteCycle |
| 96 | MEDITATE | 0 | 0 | 2 | Task:MeditateStretchAttacker, Task:PsychicPaletteCycle |
| 97 | AGILITY | 0 | 0 | 2 | Task:TraceMonBlendedExact, Task:TranslateMonEllipticalRespectSide |
| 98 | QUICK_ATTACK | 0 | 1 | 3 | Sprite:OrientedStatic, Task:ShakeMonExact, Task:TraceMonBlendedExact, Task:TranslateMonEllipticalRespectSide |
| 99 | RAGE | 0 | 3 | 2 | Sprite:AnchorOrientedStatic, Sprite:BattlerLunge, Sprite:OrientedStatic, Task:BlendMonInAndOutExact, Task:ShakeTargetPowerOrDamage |
| 100 | TELEPORT | 0 | 0 | 2 | Task:PsychicPaletteCycle, Task:TeleportExact |
| 101 | NIGHT_SHADE | 0 | 0 | 3 | Task:BlendColorCycleExact, Task:NightShadeClone, Task:ShakeMon2Exact |
| 102 | MIMIC | 0 | 1 | 2 | Sprite:MimicOrb, Task:BlendColorCycleExact, Task:ShrinkTargetCopy |
| 103 | SCREECH | 0 | 1 | 2 | Sprite:TargetLocationProjectile, Task:ShakeMon2Exact, Task:SwayMonExact |
| 104 | DOUBLE_TEAM | 0 | 0 | 1 | Task:DoubleTeam |
| 105 | RECOVER | 0 | 2 | 1 | Sprite:AbsorptionLinear, Sprite:OrientedStatic, Task:BlendColorCycleExact |
| 106 | HARDEN | 0 | 0 | 1 | Task:MetallicShineExact |
| 107 | MINIMIZE | 0 | 0 | 1 | Task:Minimize |
| 108 | SMOKESCREEN | 0 | 2 | 1 | Sprite:SmokeDrift, Sprite:StandardArcProjectile, Task:SmokescreenImpact |
| 109 | CONFUSE_RAY | 0 | 2 | 2 | Sprite:ConfuseRayBounce, Sprite:ConfuseRaySpiral, Task:TagBlendCycle |
| 110 | WITHDRAW | 0 | 0 | 1 | Task:WithdrawExact |
| 111 | DEFENSE_CURL | 0 | 1 | 2 | Sprite:OrientedStatic, Task:DefenseCurlDeformMon, Task:SetGrayscaleOrOriginalPal |
| 112 | BARRIER | 0 | 1 | 0 | Sprite:DefensiveWallCycle |
| 113 | LIGHT_SCREEN | 0 | 2 | 0 | Sprite:DefensiveWallCycle, Sprite:OrientedStatic |
| 114 | HAZE | 0 | 0 | 2 | Task:BlendBattleAnimPalExact, Task:HazeFogField |
| 115 | REFLECT | 0 | 2 | 0 | Sprite:DefensiveWallCycle, Sprite:OrientedStatic |
| 116 | FOCUS_ENERGY | 0 | 1 | 2 | Sprite:EndureRise, Task:BlendColorCycleExact, Task:ShakeMon2Exact |
| 117 | BIDE | 1 | 3 | 4 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Task:BlendBattleAnimPalExact, Task:BlendColorCycleExact, Task:ShakeMon2Exact, Task:ShakeMonInPlaceExact |
| 118 | METRONOME | 0 | 2 | 0 | Sprite:MetronomeFinger, Sprite:ThoughtBubble |
| 119 | MIRROR_MOVE | 0 | 1 | 1 | Sprite:OrientedStatic, Task:ShakeMonExact |
| 120 | SELF_DESTRUCT | 0 | 1 | 2 | Sprite:OrientedStatic, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact |
| 121 | EGG_BOMB | 0 | 2 | 1 | Sprite:OrientedStatic, Sprite:StandardArcProjectile, Task:ShakeMon2Exact |
| 122 | LICK | 0 | 1 | 1 | Sprite:LickFlicker, Task:ShakeMon2Exact |
| 123 | SMOG | 0 | 1 | 2 | Sprite:SwirlingFog, Task:BlendColorCycleExact, Task:ShakeMon2Exact |
| 124 | SLUDGE | 0 | 2 | 2 | Sprite:BubbleRise, Sprite:SludgeArc, Task:BlendColorCycleExact, Task:ShakeMonExact |
| 125 | BONE_CLUB | 0 | 3 | 1 | Sprite:OrientedStatic, Sprite:PalettePulse, Sprite:TargetLocalLinear, Task:ShakeMonExact |
| 126 | FIRE_BLAST | 0 | 2 | 2 | Sprite:FireRing, Sprite:LocalFixedVelocity, Task:BlendBattleAnimPalExact, Task:ShakeMonExact |
| 127 | WATERFALL | 0 | 4 | 2 | Sprite:BattlerLunge, Sprite:BubbleRise, Sprite:DriftingBubble, Sprite:OrientedStatic, Task:ShakeMon2Exact, Task:ShakeMonExact |
| 128 | CLAMP | 0 | 2 | 1 | Sprite:BiteClamp, Sprite:OrientedStatic, Task:ShakeMonExact |
| 129 | SWIFT | 0 | 1 | 1 | Sprite:StandardArcProjectile, Task:ShakeMon2Exact |
| 130 | SKULL_BASH | 1 | 3 | 3 | Sprite:BattlerSlideToOffsetAndBack, Sprite:FlashingHitFlicker, Sprite:PalettePulse, Task:RotateMonSpriteToSide, Task:ShakeMonInPlaceExact, Task:SkullBashPosition |
| 131 | SPIKE_CANNON | 0 | 3 | 2 | Sprite:BattlerRestorePosition, Sprite:InvertedHitSplatExact, Sprite:StandardLinearProjectile, Task:ShakeMon2Exact, Task:WindUpLunge |
| 132 | CONSTRICT | 0 | 1 | 1 | Sprite:ConstrictBinding, Task:ShakeMon2Exact |
| 133 | AMNESIA | 0 | 1 | 1 | Sprite:QuestionMarkExact, Task:PsychicPaletteCycle |
| 134 | KINESIS | 0 | 2 | 1 | Sprite:BentSpoonExact, Sprite:OrientedStatic, Task:PsychicPaletteCycle |
| 135 | SOFT_BOILED | 0 | 4 | 1 | Sprite:OrientedStatic, Sprite:PalettePulse, Sprite:SoftBoiledEgg, Task:ShakeMonExact |
| 136 | HI_JUMP_KICK | 0 | 4 | 1 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:DiagonalStrike, Sprite:OrientedStatic, Task:ShakeMonInPlaceExact |
| 137 | GLARE | 0 | 1 | 4 | Sprite:OrientedStatic, Task:BlendBattleAnimPalExact, Task:GlareEyeDots, Task:ScaryFaceExact, Task:ShakeTargetInPatternExact |
| 138 | DREAM_EATER | 0 | 2 | 3 | Sprite:AbsorptionArc, Sprite:OrientedStatic, Task:PsychicPaletteCycle, Task:ScaleMonAndRestore, Task:ShakeMonExact |
| 139 | POISON_GAS | 0 | 1 | 1 | Sprite:PoisonGasCloud, Task:BlendColorCycleExact |
| 140 | BARRAGE | 0 | 1 | 2 | Sprite:BattlerShake, Task:BarrageBall, Task:ShakeMonExact |
| 141 | LEECH_LIFE | 0 | 5 | 1 | Sprite:AbsorptionArc, Sprite:LeechLifeNeedle, Sprite:OrientedStatic, Sprite:PalettePulse, Task:ShakeMonExact |
| 142 | LOVELY_KISS | 0 | 2 | 0 | Sprite:DevilOrbit, Sprite:PinkHeartWave |
| 143 | SKY_ATTACK | 1 | 1 | 10 | Sprite:SkyAttackBird, Task:AttackerFadeFromInvisibleExact, Task:AttackerFadeToInvisibleExact, Task:BlendBattleAnimPalExact, Task:BlendBattleAnimPalExcludeExact, Task:HorizontalShakeExact, Task:ShakeMon2Exact, Task:SlidingBackground |
| 144 | TRANSFORM | 0 | 0 | 1 | Task:TransformMon |
| 145 | BUBBLE | 0 | 2 | 0 | Sprite:BubbleRise, Sprite:OrbitingProjectile |
| 146 | DIZZY_PUNCH | 0 | 4 | 1 | Sprite:BasicFistOrFootExact, Sprite:BattlerLunge, Sprite:DizzyWave, Sprite:OrientedStatic, Task:ShakeMon2Exact |
| 147 | SPORE | 0 | 1 | 1 | Sprite:SporeOrbit |
| 148 | FLASH | 0 | 0 | 1 | Task:FlashExact |
| 149 | PSYWAVE | 0 | 1 | 3 | Sprite:SinWaveProjectile, Task:BlendColorCycleExact, Task:PsychicPaletteCycle |
| 150 | SPLASH | 0 | 0 | 1 | Task:Splash |
| 151 | ACID_ARMOR | 0 | 0 | 1 | Task:AcidArmor |
| 152 | CRABHAMMER | 0 | 5 | 1 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:BubbleRise, Sprite:OrientedStatic, Sprite:PalettePulse, Task:ShakeMonExact |
| 153 | EXPLOSION | 0 | 2 | 2 | Sprite:OrientedStatic, Sprite:PalettePulse, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact |
| 154 | FURY_SWIPES | 0 | 2 | 1 | Sprite:BattlerLunge, Sprite:Static, Task:ShakeMon2Exact |
| 155 | BONEMERANG | 0 | 3 | 1 | Sprite:BattlerLunge, Sprite:Bonemerang, Sprite:OrientedStatic, Task:ShakeMonExact |
| 156 | REST | 0 | 1 | 0 | Sprite:SleepLetter |
| 157 | ROCK_SLIDE | 0 | 2 | 1 | Sprite:BattlerShake, Sprite:FallingRockEllipse, Task:ShakeMonExact |
| 158 | HYPER_FANG | 2 | 1 | 3 | Sprite:Static, Task:ShakeMonExact |
| 159 | SHARPEN | 0 | 1 | 0 | Sprite:SharpenBlink |
| 160 | CONVERSION | 0 | 1 | 2 | Sprite:ConversionHold, Task:ConversionAlphaBlendExact, Task:TagFlash |
| 161 | TRI_ATTACK | 0 | 6 | 2 | Sprite:FlameDrift, Sprite:IceImpact, Sprite:OrientedStatic, Sprite:PalettePulse, Sprite:TriAttackTriangle, Task:InvertScreenColorExact, Task:ShakeTargetInPatternExact |
| 162 | SUPER_FANG | 0 | 3 | 3 | Sprite:BattlerLunge, Sprite:PalettePulse, Sprite:Static, Task:BlendMonInAndOutExact, Task:ShakeMonExact, Task:ShakeMonInPlaceExact |
| 163 | SLASH | 0 | 1 | 1 | Sprite:SlashFlicker, Task:ShakeMon2Exact |
| 164 | SUBSTITUTE | 0 | 0 | 1 | Task:MonToSubstitute |
| 165 | STRUGGLE | 0 | 2 | 1 | Sprite:MovementWavesRepeat, Sprite:OrientedStatic, Task:ShakeMonInPlaceExact |
| 166 | SKETCH | 0 | 1 | 2 | Sprite:PencilScribble, Task:SketchScanline, Task:Splash |
| 167 | TRIPLE_KICK | 2 | 2 | 1 | Sprite:BasicFistOrFootExact, Sprite:OrientedStatic, Task:ShakeMonExact |
| 168 | THIEF | 0 | 2 | 1 | Sprite:BattlerLunge, Sprite:OrientedStatic, Task:ShakeMon2Exact |
| 169 | SPIDER_WEB | 0 | 3 | 0 | Sprite:PalettePulse, Sprite:SpiderWebFade, Sprite:WebThreadWave |
| 170 | MIND_READER | 0 | 3 | 1 | Sprite:OrientedStatic, Sprite:TargetConverge, Sprite:WhiteHaloFade, Task:BlendColorCycleExact |
| 171 | NIGHTMARE | 0 | 0 | 3 | Task:BlendMonInAndOutExact, Task:NightmareClone, Task:ShakeMonExact |
| 172 | FLAME_WHEEL | 0 | 4 | 2 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:ExpandingSpiral, Sprite:LocalFixedVelocity, Task:BlendMonInAndOutExact, Task:ShakeMonExact |
| 173 | SNORE | 0 | 2 | 2 | Sprite:BattlerShake, Sprite:TravelDiagonal, Task:ScaleMonAndRestore, Task:ShakeMon2Exact |
| 174 | CURSE | 1 | 3 | 5 | Sprite:CurseNail, Sprite:GhostRiseFade, Sprite:PalettePulse, Task:BlendColorCycleExact, Task:CurseStretchingBlackBg, Task:CurseWhiteLines, Task:ShakeMon2Exact, Task:SwayMonExact |
| 175 | FLAIL | 0 | 1 | 2 | Sprite:RandomBattlerHit, Task:FlailMovement, Task:ShakeTargetPowerOrDamage |
| 176 | CONVERSION_2 | 0 | 1 | 1 | Sprite:ConversionReturn, Task:Conversion2AlphaBlendExact |
| 177 | AEROBLAST | 0 | 2 | 2 | Sprite:OrientedStatic, Sprite:StandardLinearProjectile, Task:ShakeMonExact, Task:SlidingBackground |
| 178 | COTTON_SPORE | 0 | 1 | 0 | Sprite:SporeOrbit |
| 179 | REVERSAL | 0 | 5 | 2 | Sprite:BasicFistOrFootExact, Sprite:BattlerLunge, Sprite:FastOrbit, Sprite:OrientedStatic, Sprite:PalettePulse, Task:BlendColorCycleExact, Task:ShakeTargetPowerOrDamage |
| 180 | SPITE | 0 | 0 | 2 | Task:BlendColorCycleExact, Task:SpiteTargetShadowExact |
| 181 | POWDER_SNOW | 0 | 4 | 0 | Sprite:IceImpact, Sprite:PalettePulse, Sprite:WavyBeyondTarget |
| 182 | PROTECT | 0 | 1 | 0 | Sprite:ProtectSlide |
| 183 | MACH_PUNCH | 1 | 2 | 4 | Sprite:BasicFistOrFootExact, Sprite:OrientedStatic, Task:AttackerPunchWithTraceExact, Task:ShakeMonExact, Task:SlidingBackground |
| 184 | SCARY_FACE | 0 | 2 | 2 | Sprite:OrientedStatic, Sprite:PalettePulse, Task:ScaryFaceExact, Task:ShakeTargetInPatternExact |
| 185 | FAINT_ATTACK | 0 | 1 | 6 | Sprite:OrientedStatic, Task:AttackerFadeFromInvisibleExact, Task:AttackerFadeToInvisibleExact, Task:InitAttackerFadeFromInvisibleExact, Task:PersistentAttackerHide, Task:ShakeMon2Exact, Task:TranslateMonEllipticalRespectSide |
| 186 | SWEET_KISS | 0 | 2 | 0 | Sprite:AngelSway, Sprite:ParticleBurstWave |
| 187 | BELLY_DRUM | 0 | 2 | 3 | Sprite:BellyDrumHandExact, Sprite:MusicNoteWave, Task:MusicNotePaletteClear, Task:MusicNotePaletteSetup, Task:ShakeMonExact |
| 188 | SLUDGE_BOMB | 0 | 3 | 2 | Sprite:BubbleRise, Sprite:SludgeArc, Sprite:SludgeBurst, Task:BlendColorCycleExact, Task:ShakeMon2Exact |
| 189 | MUD_SLAP | 0 | 3 | 0 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:DirtProjectile |
| 190 | OCTAZOOKA | 0 | 2 | 0 | Sprite:OrientedStatic, Sprite:TargetLocationProjectile |
| 191 | SPIKES | 0 | 1 | 0 | Sprite:SpikesArc |
| 192 | ZAP_CANNON | 0 | 3 | 1 | Sprite:OrientedStatic, Sprite:TargetLocationProjectile, Sprite:ZapCannonSpark, Task:ShakeMon2Exact |
| 193 | FORESIGHT | 0 | 1 | 1 | Sprite:ForesightScan, Task:BlendMonInAndOutExact |
| 194 | DESTINY_BOND | 0 | 0 | 3 | Task:BlendBattleAnimPalExcludeExact, Task:DestinyBondShadow, Task:ShakeMonInPlaceExact |
| 195 | PERISH_SONG | 0 | 3 | 1 | Sprite:PalettePulse, Sprite:PerishSongDelayedNote, Sprite:PerishSongNote, Task:SetGrayscaleOrOriginalPal |
| 196 | ICY_WIND | 0 | 2 | 1 | Sprite:OrientedStatic, Sprite:SwirlingSnowball, Task:BlendBattleAnimPalExact |
| 197 | DETECT | 0 | 2 | 1 | Sprite:OrientedStatic, Sprite:PalettePulse, Task:BlendBattleAnimPalExact |
| 198 | BONE_RUSH | 0 | 2 | 1 | Sprite:OrientedStatic, Sprite:TargetLocalLinear, Task:ShakeMonExact |
| 199 | LOCK_ON | 0 | 2 | 0 | Sprite:LockOnScan |
| 200 | OUTRAGE | 0 | 1 | 3 | Sprite:OutrageFlame, Task:BlendColorCycleExact, Task:ShakeMon2Exact, Task:TranslateMonEllipticalRespectSide |
| 201 | SANDSTORM | 0 | 1 | 1 | Sprite:FlyingSandCrescent, Task:SandstormField |
| 202 | GIGA_DRAIN | 0 | 4 | 1 | Sprite:AbsorptionArc, Sprite:OrientedStatic, Sprite:PalettePulse, Task:ShakeMonExact |
| 203 | ENDURE | 0 | 1 | 2 | Sprite:EndureRise, Task:BlendColorCycleExact, Task:ShakeMon2Exact |
| 204 | CHARM | 0 | 1 | 1 | Sprite:RisingHeartWave, Task:RockMonBackAndForth |
| 205 | ROLLOUT | 0 | 1 | 2 | Sprite:OrientedStatic, Task:RolloutDebris, Task:ShakeTargetPowerOrDamage |
| 206 | FALSE_SWIPE | 0 | 3 | 1 | Sprite:FalseSwipe, Sprite:FalseSwipeFlicker, Sprite:OrientedStatic, Task:ShakeMonExact |
| 207 | SWAGGER | 0 | 2 | 1 | Sprite:AnchorOrientedStatic, Sprite:BreathPuff, Task:GrowAndShrink |
| 208 | MILK_DRINK | 0 | 3 | 0 | Sprite:MilkBottleDrink, Sprite:OrientedStatic |
| 209 | SPARK | 0 | 5 | 2 | Sprite:BattlerLunge, Sprite:ElectricOrbit, Sprite:OrientedStatic, Sprite:StaticElectricity, Task:BlendColorCycleExact, Task:ShakeMonExact |
| 210 | FURY_CUTTER | 1 | 2 | 3 | Sprite:PalettePulse, Sprite:SliceArc, Task:ShakeMonExact |
| 211 | STEEL_WING | 0 | 4 | 3 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:StandardLinearProjectile, Task:MetallicShineExact, Task:TagPaletteRotate, Task:TranslateMonElliptical |
| 212 | MEAN_LOOK | 0 | 2 | 0 | Sprite:PalettePulse, Sprite:SpotlightSway |
| 213 | ATTRACT | 0 | 3 | 3 | Sprite:HeartWaveProjectile, Sprite:ParticleBurstWave, Sprite:RedHeartRising, Task:BlendColorCycleExact, Task:HeartsField, Task:SwayMonExact |
| 214 | SLEEP_TALK | 0 | 1 | 1 | Sprite:SleepLetterWave, Task:SwayMonExact |
| 215 | HEAL_BELL | 0 | 5 | 4 | Sprite:HealBellNote, Sprite:LocalStarVelocity, Sprite:OrientedStatic, Task:BlendBattleAnimPalExact, Task:BlendBattleAnimPalExcludeExact |
| 216 | RETURN | 4 | 4 | 5 | Sprite:BattlerDip, Sprite:BattlerLunge, Sprite:OrientedStatic, Sprite:PalettePulse, Task:ShakeMonExact, Task:TraceMonBlendedExact |
| 217 | PRESENT | 2 | 4 | 1 | Sprite:OrientedStatic, Sprite:PresentBounce, Sprite:PresentHealRise |
| 218 | FRUSTRATION | 3 | 4 | 5 | Sprite:AnchorOrientedStatic, Sprite:BattlerLunge, Sprite:OrientedStatic, Sprite:WeakFrustrationMark, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact, Task:StrongFrustrationGrowAndShrink, Task:SwayMonExact |
| 219 | SAFEGUARD | 0 | 1 | 1 | Sprite:GuardRingRise, Task:BlendColorCycleExact |
| 220 | PAIN_SPLIT | 0 | 1 | 1 | Sprite:PainSplitBounce, Task:PainSplitMovement |
| 221 | SACRED_FIRE | 0 | 3 | 1 | Sprite:FlameDrift, Task:InvertScreenColorExact |
| 222 | MAGNITUDE | 2 | 1 | 2 | Sprite:PalettePulse, Task:HorizontalShakeExact |
| 223 | DYNAMIC_PUNCH | 0 | 3 | 2 | Sprite:BasicFistOrFootExact, Sprite:OrientedStatic, Task:ShakeMon2Exact, Task:ShakeMonExact |
| 224 | MEGAHORN | 0 | 5 | 3 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:MegahornStrike, Sprite:OrientedStatic, Sprite:PalettePulse, Task:ShakeMonExact, Task:ShakeMonInPlaceExact, Task:SlidingBackground |
| 225 | DRAGON_BREATH | 0 | 1 | 2 | Sprite:DragonFire, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact |
| 226 | BATON_PASS | 0 | 1 | 1 | Sprite:BatonPassBall, Task:BlendColorCycleExact |
| 227 | ENCORE | 0 | 3 | 5 | Sprite:ClappingHandWindowMask, Sprite:ClappingHands, Sprite:SpotlightSway, Task:HardwarePaletteFadeExact, Task:SpotlightWindow, Task:SwayMonExact |
| 228 | PURSUIT | 1 | 1 | 1 | Sprite:OrientedStatic, Task:ShakeTargetPowerOrDamage |
| 229 | RAPID_SPIN | 0 | 2 | 2 | Sprite:OrientedStatic, Sprite:RapidSpin, Task:RapidSpinMonElevation, Task:ShakeTargetPowerOrDamage |
| 230 | SWEET_SCENT | 0 | 1 | 1 | Sprite:SweetScentPetal, Task:BlendColorCycleExact |
| 231 | IRON_TAIL | 0 | 2 | 3 | Sprite:BattlerLunge, Sprite:OrientedStatic, Task:MetallicShineExact, Task:SetGrayscaleOrOriginalPal, Task:ShakeMonExact |
| 232 | METAL_CLAW | 0 | 3 | 1 | Sprite:BattlerLunge, Sprite:BattlerShake, Sprite:Static, Task:MetallicShineExact |
| 233 | VITAL_THROW | 0 | 3 | 1 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Task:TranslateMonEllipticalRespectSide |
| 234 | MORNING_SUN | 0 | 2 | 2 | Sprite:GreenStarRise, Sprite:OrientedStatic, Task:BlendBattleAnimPalExact, Task:MorningSunField |
| 235 | SYNTHESIS | 0 | 2 | 1 | Sprite:LocalStarVelocity, Sprite:OrientedStatic, Task:BlendColorCycleExact |
| 236 | MOONLIGHT | 0 | 4 | 2 | Sprite:MoonAbsolute, Sprite:MoonlightSparkleFall, Sprite:OrientedStatic, Sprite:PalettePulse, Task:AlphaFadeInExact, Task:MoonlightEndFadeExact |
| 237 | HIDDEN_POWER | 0 | 2 | 2 | Sprite:FastOrbit, Sprite:RadialScatter, Task:BlendMonInAndOutExact, Task:ScaleMonAndRestore |
| 238 | CROSS_CHOP | 0 | 3 | 1 | Sprite:CrossChop, Sprite:OrientedStatic, Sprite:PalettePulse, Task:ShakeMonExact |
| 239 | TWISTER | 0 | 4 | 1 | Sprite:OrientedStatic, Sprite:RandomBattlerHit, Sprite:TwisterParticle, Task:ShakeMonInPlaceExact |
| 240 | RAIN_DANCE | 0 | 0 | 2 | Task:BlendBattleAnimPalExact, Task:Raindrops |
| 241 | SUNNY_DAY | 0 | 1 | 1 | Sprite:SunlightRay, Task:BlendBattleAnimPalExact |
| 242 | CRUNCH | 0 | 2 | 1 | Sprite:BiteClamp, Sprite:OrientedStatic, Task:ShakeMonExact |
| 243 | MIRROR_COAT | 0 | 2 | 0 | Sprite:DefensiveWallCycle, Sprite:OrientedStatic |
| 244 | PSYCH_UP | 0 | 1 | 4 | Sprite:OrientedStatic, Task:BlendBattleAnimPalExact, Task:BlendColorCycleExcludeExact, Task:ScaleMonAndRestore, Task:SwayMonExact |
| 245 | EXTREME_SPEED | 1 | 1 | 7 | Sprite:Static, Task:AttackerStretchAndDisappear, Task:ExtremeSpeedImpact, Task:ExtremeSpeedMonReappear, Task:PersistentAttackerHide, Task:SlidingBackground, Task:SpeedDust |
| 246 | ANCIENT_POWER | 0 | 5 | 1 | Sprite:BattlerRestorePosition, Sprite:BattlerShake, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:RaiseSprite, Task:ShakeMon2Exact |
| 247 | SHADOW_BALL | 0 | 1 | 1 | Sprite:ShadowBall, Task:ShakeMon2Exact |
| 248 | FUTURE_SIGHT | 0 | 0 | 3 | Task:BlendColorCycleExact, Task:PsychicPaletteCycle, Task:ScaleMonAndRestore |
| 249 | ROCK_SMASH | 0 | 3 | 1 | Sprite:BasicFistOrFootExact, Sprite:OrientedStatic, Sprite:RockFragmentLinear, Task:ShakeMonExact |
| 250 | WHIRLPOOL | 0 | 2 | 1 | Sprite:PalettePulse, Sprite:Vortex, Task:ShakeMonExact |
| 251 | BEAT_UP | 1 | 2 | 1 | Sprite:BasicFistOrFootExact, Sprite:OrientedStatic, Task:ShakeMonExact |
| 252 | FAKE_OUT | 0 | 1 | 3 | Sprite:PalettePulse, Task:FakeOutExact, Task:ShakeMon2Exact, Task:StretchTargetUp |
| 253 | UPROAR | 0 | 2 | 1 | Sprite:OrientedStatic, Sprite:RoarLine, Task:UproarDistortion |
| 254 | STOCKPILE | 0 | 2 | 2 | Sprite:AbsorptionLinear, Sprite:PalettePulse, Task:BlendColorCycleExact, Task:StockpileDeformMon |
| 255 | SPIT_UP | 2 | 2 | 3 | Sprite:FlashingHitFlicker, Sprite:RadialScatter, Task:ShakeMon2Exact, Task:ShakeTargetPowerOrDamage, Task:SpitUpDeformMon |
| 256 | SWALLOW | 2 | 2 | 2 | Sprite:OrientedStatic, Sprite:SwallowOrb, Task:ShakeMon2Exact, Task:SwallowDeformMon |
| 257 | HEAT_WAVE | 0 | 1 | 4 | Sprite:FlyingSandCrescent, Task:BlendBackgroundExact, Task:HeatWaveTargetMotion, Task:SandstormField, Task:TagBlendToColor |
| 258 | HAIL | 0 | 0 | 2 | Task:BlendBattleAnimPalExact, Task:HailStones |
| 259 | TORMENT | 0 | 1 | 2 | Sprite:AnchorOrientedStatic, Task:BlendMonInAndOutExact, Task:TormentThoughtBubbles |
| 260 | FLATTER | 0 | 3 | 5 | Sprite:BattlerDip, Sprite:ConfettiBallistic, Sprite:FlatterSpotlightMask, Task:HardwarePaletteFadeExact, Task:SpotlightWindow |
| 261 | WILL_O_WISP | 0 | 2 | 2 | Sprite:ExpandingEllipse, Sprite:WillOWispOrb, Task:ShakeMon2Exact |
| 262 | MEMENTO | 0 | 0 | 4 | Task:InitMementoShadowExact, Task:MementoAttackerShadow, Task:MementoBackgroundControl, Task:MementoTargetShadow |
| 263 | FACADE | 0 | 0 | 2 | Task:FacadeColorBlendExact, Task:SquishAndSweatDroplets |
| 264 | FOCUS_PUNCH | 0 | 2 | 3 | Sprite:FocusPunchWobble, Sprite:OrientedStatic, Task:ShakeMonExact |
| 265 | SMELLING_SALT | 0 | 2 | 2 | Sprite:SmellingSalts, Sprite:SmellingSaltsBlink, Task:ShakeMon2Exact, Task:SmellingSaltsSquish |
| 266 | FOLLOW_ME | 0 | 1 | 0 | Sprite:FollowMeFinger |
| 267 | NATURE_POWER | 0 | 5 | 1 | Sprite:BattlerRestorePosition, Sprite:BattlerShake, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:RaiseSprite, Task:ShakeMon2Exact |
| 268 | CHARGE | 0 | 3 | 1 | Sprite:PalettePulse, Sprite:Static, Task:ElectricChargingParticles |
| 269 | TAUNT | 0 | 3 | 0 | Sprite:AnchorOrientedStatic, Sprite:TauntFinger, Sprite:ThoughtBubble |
| 270 | HELPING_HAND | 0 | 1 | 3 | Sprite:HelpingHandClap, Task:BlendMonInAndOutExact, Task:HelpingHandAttackerMovement, Task:ShakeMon2Exact |
| 271 | TRICK | 0 | 1 | 3 | Sprite:TrickBagOrbit, Task:ShakeMonExact, Task:StretchAttackerUp, Task:StretchTargetUp |
| 272 | ROLE_PLAY | 0 | 1 | 2 | Sprite:PalettePulse, Task:BlendBattleAnimPalExact, Task:RolePlaySilhouette |
| 273 | WISH | 0 | 3 | 0 | Sprite:LocalStarVelocity, Sprite:PalettePulse, Sprite:WishStar |
| 274 | ASSIST | 0 | 1 | 0 | Sprite:AssistPawprintTravel |
| 275 | INGRAIN | 0 | 2 | 0 | Sprite:IngrainOrbWave, Sprite:IngrainRoot |
| 276 | SUPERPOWER | 0 | 4 | 2 | Sprite:BattlerShake, Sprite:SuperpowerFireball, Sprite:SuperpowerOrbHold, Sprite:SuperpowerRock, Task:ShakeMon2Exact |
| 277 | MAGIC_COAT | 0 | 1 | 0 | Sprite:DefensiveWallCycle |
| 278 | RECYCLE | 0 | 1 | 1 | Sprite:RecycleFade, Task:BlendMonInAndOutExact |
| 279 | REVENGE | 0 | 4 | 2 | Sprite:BattlerLunge, Sprite:HitSplatPersistentExact, Sprite:RevengeScratchExact, Task:BlendColorCycleExact, Task:ShakeMon2Exact |
| 280 | BRICK_BREAK | 1 | 6 | 1 | Sprite:BasicFistOrFootExact, Sprite:BattlerLunge, Sprite:BrickWallShake, Sprite:BrickWallShard, Sprite:OrientedStatic, Sprite:PalettePulse, Task:WindUpLunge |
| 281 | YAWN | 0 | 1 | 1 | Sprite:YawnCloud, Task:DeepInhale |
| 282 | KNOCK_OFF | 0 | 6 | 1 | Sprite:BattlerLunge, Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:KnockOffStrike, Sprite:OrientedStatic, Sprite:PalettePulse, Task:ShakeMonInPlaceExact |
| 283 | ENDEAVOR | 0 | 1 | 3 | Sprite:OrientedStatic, Task:BlendMonInAndOutExact, Task:ShakeTargetPowerOrDamage, Task:SquishAndSweatDroplets |
| 284 | ERUPTION | 0 | 2 | 2 | Sprite:EruptionFallingRock, Sprite:PalettePulse, Task:EruptionLaunchRocks, Task:HorizontalShakeExact |
| 285 | SKILL_SWAP | 0 | 0 | 3 | Task:BlendMonInAndOutExact, Task:PsychicPaletteCycle, Task:SkillSwapOrbs |
| 286 | IMPRISON | 0 | 1 | 3 | Sprite:TimedRedX, Task:HorizontalShakeExact, Task:ImprisonOrbs, Task:PsychicPaletteCycle |
| 287 | REFRESH | 0 | 3 | 1 | Sprite:LocalStarVelocity, Sprite:OrientedStatic, Sprite:PalettePulse, Task:StatusClearedEffectExact |
| 288 | GRUDGE | 0 | 0 | 1 | Task:GrudgeFlames |
| 289 | SNATCH | 0 | 0 | 1 | Task:WindUpLunge |
| 290 | SECRET_POWER | 9 | 18 | 10 | Sprite:BasicFistOrFootExact, Sprite:BattlerLunge, Sprite:BattlerRestorePosition, Sprite:BattlerShake, Sprite:BattlerSlideToOffset, Sprite:BiteClamp, Sprite:BubbleRise, Sprite:DriftingBubble, Sprite:FallingRockEllipse, Sprite:NeedleArmSpike, Sprite:OrbitingProjectile, Sprite:OrientedStatic, Sprite:RazorLeafDrift, Sprite:SinWaveProjectile, Sprite:StandardArcProjectile, Sprite:WhipHitExact, Task:MagicalLeafPaletteCycle, Task:ShakeAndSinkMonExact, Task:ShakeMon2Exact, Task:ShakeMonExact, Task:ShakeMonInPlaceExact, Task:SurfWave, Task:SwayMonExact, Task:TranslateMonEllipticalRespectSide |
| 291 | DIVE | 1 | 5 | 1 | Sprite:DiveBallCycle, Sprite:DiveWaterSplash, Sprite:DriftingBubble, Sprite:OrientedStatic, Sprite:WaterSprayBallistic, Task:ShakeMon2Exact |
| 292 | ARM_THRUST | 1 | 3 | 2 | Sprite:ArmThrustImpact, Sprite:BattlerLunge, Sprite:OrientedStatic, Task:RotateMonSpriteToSide, Task:ShakeMonExact |
| 293 | CAMOUFLAGE | 0 | 0 | 3 | Task:AttackerFadeFromInvisibleExact, Task:AttackerFadeToInvisibleExact, Task:SetCamouflageBlendExact |
| 294 | TAIL_GLOW | 0 | 2 | 0 | Sprite:PalettePulse, Sprite:TailGlowOrbExact |
| 295 | LUSTER_PURGE | 0 | 2 | 5 | Sprite:OrientedStatic, Sprite:RandomBattlerHit, Task:BlendBattleAnimPalExcludeExact, Task:FadeScreenPaletteCycle, Task:HorizontalShakeExact, Task:TagBlendToColor |
| 296 | MIST_BALL | 0 | 2 | 3 | Sprite:PalettePulse, Sprite:TargetLocationProjectile, Task:BlendBattleAnimPalExact, Task:MistBallFogField, Task:ShakeMonExact |
| 297 | FEATHER_DANCE | 0 | 1 | 0 | Sprite:FallingFeatherWave |
| 298 | TEETER_DANCE | 0 | 1 | 1 | Sprite:MusicOrbit, Task:TeeterDanceMovement |
| 299 | BLAZE_KICK | 0 | 4 | 2 | Sprite:LocalFixedVelocity, Sprite:OrientedStatic, Sprite:PalettePulse, Sprite:SpinningKickOrPunchExact, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact |
| 300 | MUD_SPORT | 0 | 1 | 1 | Sprite:MudSportDirt, Task:Splash |
| 301 | ICE_BALL | 1 | 1 | 2 | Sprite:StandardArcProjectile |
| 302 | NEEDLE_ARM | 0 | 3 | 1 | Sprite:BasicFistOrFootExact, Sprite:NeedleArmSpike, Sprite:OrientedStatic, Task:ShakeMon2Exact |
| 303 | SLACK_OFF | 0 | 1 | 1 | Sprite:OrientedStatic, Task:SlackOffSquish |
| 304 | HYPER_VOICE | 0 | 2 | 4 | Sprite:HyperVoiceRing, Sprite:PalettePulse, Task:ScaleMonAndRestore, Task:ShakeBattleTerrainExact, Task:ShakeMon2Exact |
| 305 | POISON_FANG | 0 | 2 | 2 | Sprite:BubbleRise, Sprite:Static, Task:BlendColorCycleExact, Task:ShakeMonExact |
| 306 | CRUSH_CLAW | 0 | 2 | 1 | Sprite:BattlerLunge, Sprite:Static, Task:ShakeMon2Exact |
| 307 | BLAST_BURN | 0 | 2 | 3 | Sprite:FlameDrift, Sprite:OrientedStatic, Task:InvertScreenColorExact, Task:ShakeBattleTerrainExact, Task:ShakeMonExact |
| 308 | HYDRO_CANNON | 0 | 3 | 2 | Sprite:HydroCannonChargeExact, Sprite:OrientedStatic, Sprite:StandardLinearProjectile, Task:InvertScreenColorExact, Task:ShakeMonExact |
| 309 | METEOR_MASH | 0 | 3 | 1 | Sprite:MeteorMashStar, Sprite:OrientedStatic, Sprite:SpinningKickOrPunchExact, Task:ShakeMon2Exact |
| 310 | ASTONISH | 0 | 2 | 2 | Sprite:BattlerLunge, Sprite:WaterSprayBallistic, Task:ShakeMon2Exact, Task:StretchTargetUp |
| 311 | WEATHER_BALL | 5 | 12 | 3 | Sprite:BattlerDip, Sprite:IceImpact, Sprite:OrientedStatic, Sprite:PalettePulse, Sprite:RockScatterBounce, Sprite:WeatherBallDown, Sprite:WeatherBallRise, Task:ShakeMon2Exact, Task:ShakeMonExact |
| 312 | AROMATHERAPY | 0 | 5 | 1 | Sprite:FlyingParticle, Sprite:LocalStarVelocity, Sprite:OrientedStatic, Sprite:PalettePulse, Task:StatusClearedEffectExact |
| 313 | FAKE_TEARS | 0 | 1 | 2 | Sprite:TearDropArc, Task:RockMonBackAndForth, Task:TagBlendToColor |
| 314 | AIR_CUTTER | 0 | 1 | 2 | Sprite:SliceArc, Task:AirCutterProjectiles, Task:ShakeMon2Exact |
| 315 | OVERHEAT | 0 | 3 | 6 | Sprite:OrientedStatic, Sprite:PalettePulse, Sprite:RadialLinear, Task:PaletteBackupCommit, Task:PaletteBackupRestore, Task:PaletteBackupSave, Task:ShakeMonExact |
| 316 | ODOR_SLEUTH | 0 | 2 | 1 | Sprite:BattlerLunge, Sprite:PalettePulse, Task:OdorSleuthMovement |
| 317 | ROCK_TOMB | 0 | 2 | 2 | Sprite:RockTombBounce, Sprite:TimedRedX, Task:ShakeBattleTerrainExact, Task:ShakeMonExact |
| 318 | SILVER_WIND | 1 | 3 | 4 | Sprite:FlyingParticle, Task:BlendBattleAnimPalExact, Task:BlendBattleAnimPalExcludeExact, Task:SlidingBackground |
| 319 | METAL_SOUND | 0 | 1 | 1 | Sprite:TargetLocationProjectile, Task:ShakeMon2Exact |
| 320 | GRASS_WHISTLE | 0 | 2 | 2 | Sprite:PalettePulse, Sprite:WavyMusicNote, Task:MusicNotePaletteClear, Task:MusicNotePaletteSetup |
| 321 | TICKLE | 0 | 2 | 2 | Sprite:OrientedStatic, Sprite:PalettePulse, Task:RockMonBackAndForth, Task:SwayMonExact |
| 322 | COSMIC_POWER | 0 | 1 | 4 | Sprite:LocalStarVelocity, Task:BlendNonAttackerPalettesExact, Task:SlidingBackground |
| 323 | WATER_SPOUT | 0 | 0 | 2 | Task:WaterSpoutLaunch, Task:WaterSpoutRain |
| 324 | SIGNAL_BEAM | 0 | 3 | 2 | Sprite:PalettePulse, Sprite:SinWaveProjectile, Task:ShakeMonExact |
| 325 | SHADOW_PUNCH | 0 | 2 | 2 | Sprite:BasicFistOrFootExact, Sprite:OrientedStatic, Task:AttackerPunchWithTraceExact, Task:ShakeMonExact |
| 326 | EXTRASENSORY | 0 | 0 | 4 | Task:BlendMonInAndOutExact, Task:ExtrasensoryDistortion, Task:PsychicPaletteCycle, Task:TransparentCloneGrowAndShrink |
| 327 | SKY_UPPERCUT | 0 | 3 | 3 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Task:ShakeMon2Exact, Task:ShakeMonInPlaceExact, Task:SkyUppercutBackground |
| 328 | SAND_TOMB | 0 | 2 | 1 | Sprite:PalettePulse, Sprite:Vortex, Task:ShakeMonExact |
| 329 | SHEER_COLD | 0 | 0 | 1 | Task:FrozenIceCube |
| 330 | MUDDY_WATER | 0 | 0 | 1 | Task:SurfWave |
| 331 | BULLET_SEED | 0 | 1 | 1 | Sprite:BulletSeedRicochet, Task:ShakeMon2Exact |
| 332 | AERIAL_ACE | 0 | 2 | 3 | Sprite:PalettePulse, Sprite:SliceArc, Task:ShakeMonExact, Task:TraceMonBlendedExact, Task:TranslateMonEllipticalRespectSide |
| 333 | ICICLE_SPEAR | 0 | 2 | 1 | Sprite:OrientedStatic, Sprite:StandardArcProjectile, Task:ShakeMon2Exact |
| 334 | IRON_DEFENSE | 0 | 1 | 1 | Sprite:PalettePulse, Task:MetallicShineExact |
| 335 | BLOCK | 0 | 1 | 0 | Sprite:BlockBounce |
| 336 | HOWL | 0 | 1 | 2 | Sprite:RoarLine, Task:DeepInhale |
| 337 | DRAGON_CLAW | 0 | 4 | 3 | Sprite:BattlerLunge, Sprite:BattlerShake, Sprite:Static, Sprite:Vortex, Task:BlendBattleAnimPalExact, Task:ShakeMonExact |
| 338 | FRENZY_PLANT | 0 | 3 | 1 | Sprite:FrenzyPlantRoot, Sprite:OrientedStatic, Sprite:PalettePulse, Task:ShakeMonExact |
| 339 | BULK_UP | 0 | 1 | 1 | Sprite:BreathPuff, Task:GrowAndShrink |
| 340 | BOUNCE | 1 | 3 | 1 | Sprite:BlockBounce, Sprite:BounceBallShrinkExact, Sprite:OrientedStatic, Task:ShakeMonExact |
| 341 | MUD_SHOT | 0 | 1 | 2 | Sprite:SinWaveProjectile, Task:ShakeMonExact |
| 342 | POISON_TAIL | 0 | 3 | 3 | Sprite:BattlerLunge, Sprite:BubbleRise, Sprite:OrientedStatic, Task:MetallicShineExact, Task:SetGrayscaleOrOriginalPal, Task:ShakeMonExact |
| 343 | COVET | 0 | 1 | 2 | Sprite:RisingHeartWave, Task:RockMonBackAndForth, Task:ShakeMon2Exact |
| 344 | VOLT_TACKLE | 0 | 2 | 4 | Sprite:Static, Sprite:VoltTackleSlide, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact, Task:VoltTackleAttackerReappear, Task:VoltTackleBolts |
| 345 | MAGICAL_LEAF | 0 | 3 | 2 | Sprite:OrientedStatic, Sprite:RazorLeafDrift, Sprite:StandardArcProjectile, Task:MagicalLeafPaletteCycle, Task:ShakeMon2Exact |
| 346 | WATER_SPORT | 0 | 0 | 1 | Task:WaterSportOrbs |
| 347 | CALM_MIND | 0 | 1 | 2 | Sprite:OrientedStatic, Task:BlendBattleAnimPalExcludeExact, Task:SetAllNonAttackersInvisible |
| 348 | LEAF_BLADE | 0 | 1 | 2 | Sprite:OrientedStatic, Task:LeafBladePath, Task:ShakeMon2Exact |
| 349 | DRAGON_DANCE | 0 | 1 | 2 | Sprite:DragonDanceOrbit, Task:DragonDanceDistortion, Task:TagBlendInOut |
| 350 | ROCK_BLAST | 0 | 4 | 1 | Sprite:BattlerLunge, Sprite:OrientedStatic, Sprite:RockFragmentLinear, Sprite:TargetLocationProjectile, Task:ShakeMonExact |
| 351 | SHOCK_WAVE | 0 | 2 | 5 | Sprite:PalettePulse, Sprite:Static, Task:BlendBattleAnimPalExact, Task:ElectricChargingParticles, Task:ShakeMonExact, Task:ShockWaveLightning, Task:ShockWaveProgressingBolt |
| 352 | WATER_PULSE | 0 | 3 | 1 | Sprite:PalettePulse, Sprite:WaterPulseBubble, Sprite:WaterPulseRing, Task:ShakeMonExact |
| 353 | DOOM_DESIRE | 0 | 1 | 3 | Sprite:PalettePulse, Task:ScaleMonAndRestore, Task:SetGrayscaleOrOriginalPal |
| 354 | PSYCHO_BOOST | 0 | 1 | 4 | Sprite:PsychoBoostRise, Task:BlendColorCycleExact, Task:FadeScreenPaletteCycle, Task:ShakeMon2Exact, Task:ShakeMonExact |
| 355 | ROOST | 0 | 2 | 0 | Sprite:FallingFeatherWave, Sprite:OrientedStatic |
| 356 | GRAVITY | 0 | 0 | 3 | Task:ShakeAndSinkMonExact, Task:ShakeMonExact |
| 357 | MIRACLE_EYE | 0 | 3 | 2 | Sprite:OrientedStatic, Sprite:TargetConverge, Sprite:WhiteHaloFade, Task:BlendColorCycleExact, Task:PsychicPaletteCycle |
| 358 | WAKE_UP_SLAP | 0 | 3 | 1 | Sprite:DiagonalStrike, Sprite:OrientedStatic, Sprite:SmellingSaltsBlink, Task:ShakeMonExact |
| 359 | HAMMER_ARM | 0 | 1 | 1 | Sprite:StompDrop, Task:SmellingSaltsSquish |
| 360 | GYRO_BALL | 0 | 3 | 4 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Task:MetallicShineExact, Task:RotateMonSpriteToSide, Task:ShakeMonInPlaceExact, Task:TranslateMonEllipticalRespectSide |
| 361 | HEALING_WISH | 0 | 1 | 2 | Sprite:MoonlightSparkleFall, Task:AlphaFadeInExact, Task:MoonlightEndFadeExact |
| 362 | BRINE | 0 | 0 | 1 | Task:Raindrops |
| 363 | NATURAL_GIFT | 0 | 2 | 3 | Sprite:DragonDanceOrbit, Sprite:LocalStarVelocity, Task:BlendColorCycleExact, Task:DragonDanceDistortion, Task:TagBlendInOut |
| 364 | FEINT | 0 | 2 | 0 | Sprite:SliceArc, Sprite:Static |
| 365 | PLUCK | 0 | 2 | 0 | Sprite:OrientedStatic, Sprite:PluckParticleJump |
| 366 | TAILWIND | 0 | 1 | 4 | Sprite:FlyingSandCrescent, Task:SlidingBackground, Task:TraceMonBlendedExact, Task:TranslateMonEllipticalRespectSide |
| 367 | ACUPRESSURE | 0 | 2 | 0 | Sprite:OrientedStatic, Sprite:StandardLinearProjectile |
| 368 | METAL_BURST | 0 | 1 | 3 | Sprite:RadialScatter, Task:DefenseCurlDeformMon, Task:ScaleMonAndRestore, Task:SetGrayscaleOrOriginalPal |
| 369 | U_TURN | 0 | 3 | 1 | Sprite:AbsorptionArc, Sprite:OrientedStatic, Sprite:ShadowBall, Task:ShakeMonExact |
| 370 | CLOSE_COMBAT | 0 | 1 | 3 | Sprite:RandomBattlerHit, Task:ShakeMonInPlaceExact, Task:SlidingBackground |
| 371 | PAYBACK | 0 | 1 | 1 | Sprite:OrientedStatic, Task:ShakeMon2Exact |
| 372 | ASSURANCE | 0 | 2 | 0 | Sprite:BasicFistOrFootExact, Sprite:OrientedStatic |
| 373 | EMBARGO | 0 | 1 | 1 | Sprite:SporeOrbit, Task:ShakeMon2Exact |
| 374 | FLING | 0 | 2 | 1 | Sprite:OrientedStatic, Sprite:StandardArcProjectile, Task:ShakeMonExact |
| 375 | PSYCHO_SHIFT | 0 | 2 | 0 | Sprite:DragonDanceOrbit, Sprite:OrientedStatic |
| 376 | TRUMP_CARD | 0 | 3 | 1 | Sprite:SliceArc, Sprite:TrumpCardArc, Sprite:TrumpCardParticleScatter, Task:ShakeMonExact |
| 377 | HEAL_BLOCK | 0 | 1 | 1 | Sprite:OrientedStatic, Task:BlendBattleAnimPalExact |
| 378 | WRING_OUT | 0 | 1 | 0 | Sprite:WringOutCircle |
| 379 | POWER_TRICK | 0 | 1 | 1 | Sprite:SpriteOnMonDuration, Task:UproarDistortion |
| 380 | GASTRO_ACID | 0 | 2 | 2 | Sprite:AcidBubbleArc, Sprite:TargetDroplet, Task:BlendColorCycleExact, Task:ShakeMon2Exact |
| 381 | LUCKY_CHANT | 0 | 2 | 0 | Sprite:PetalDanceBig, Sprite:PetalDanceSmall |
| 382 | ME_FIRST | 0 | 3 | 3 | Sprite:MetronomeFinger, Sprite:MimicOrb, Sprite:ThoughtBubble, Task:BlendColorCycleExact, Task:ShrinkTargetCopy, Task:TagBlendToColor |
| 383 | COPYCAT | 0 | 3 | 2 | Sprite:AbsorptionLinear, Sprite:OrientedStatic, Sprite:PalettePulse, Task:BlendColorCycleExact, Task:StockpileDeformMon |
| 384 | POWER_SWAP | 0 | 1 | 0 | Sprite:PowerSwapGuardSwapArc |
| 385 | GUARD_SWAP | 0 | 1 | 0 | Sprite:PowerSwapGuardSwapArc |
| 386 | PUNISHMENT | 0 | 2 | 1 | Sprite:OrientedStatic, Sprite:Static, Task:ShakeMonExact |
| 387 | LAST_RESORT | 0 | 5 | 4 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:PalettePulse, Task:RotateMonSpriteToSide, Task:ShakeMonInPlaceExact, Task:SlidingBackground, Task:TranslateMonEllipticalRespectSide |
| 388 | WORRY_SEED | 0 | 2 | 0 | Sprite:SmallCloudDrift, Sprite:WorrySeedArc |
| 389 | SUCKER_PUNCH | 0 | 4 | 1 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:TargetSinWave, Task:RotateMonSpriteToSide |
| 390 | TOXIC_SPIKES | 0 | 2 | 0 | Sprite:BubbleRise, Sprite:SpikesArc |
| 391 | HEART_SWAP | 0 | 3 | 4 | Sprite:LocalStarVelocity, Sprite:ParticleBurstWave, Sprite:RisingHeartWave, Task:BlendBattleAnimPalExact, Task:BlendMonInAndOutExact, Task:ScaleMonAndRestore, Task:SkillSwapOrbs |
| 392 | AQUA_RING | 0 | 2 | 3 | Sprite:BubbleRise, Sprite:GuardRingRise, Task:BlendColorCycleExact, Task:ShakeMonExact, Task:TagBlendToColor |
| 393 | MAGNET_RISE | 0 | 1 | 2 | Sprite:StaticElectricity, Task:BlendColorCycleExact, Task:WindUpLunge |
| 394 | FLARE_BLITZ | 0 | 5 | 6 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:ExpandingSpiral, Sprite:LocalFixedVelocity, Sprite:OrientedStatic, Task:BlendBattleAnimPalExact, Task:BlendMonInAndOutExact, Task:ShakeMon2Exact, Task:TranslateMonEllipticalRespectSide |
| 395 | FORCE_PALM | 0 | 2 | 1 | Sprite:DiagonalStrike, Sprite:Static, Task:ShakeMonExact |
| 396 | AURA_SPHERE | 0 | 1 | 2 | Sprite:SuperpowerFireball, Task:ShakeMon2Exact, Task:SlidingBackground |
| 397 | ROCK_POLISH | 0 | 3 | 0 | Sprite:PalettePulse, Sprite:RockPolishStreak, Sprite:Static |
| 398 | POISON_JAB | 0 | 3 | 2 | Sprite:BubbleRise, Sprite:OrientedStatic, Sprite:PoisonJabProjectile, Task:BlendMonInAndOutExact, Task:SwayMonExact |
| 399 | DARK_PULSE | 0 | 1 | 2 | Sprite:ShadowBall, Task:BlendColorCycleExact, Task:SwayMonExact |
| 400 | NIGHT_SLASH | 0 | 3 | 2 | Sprite:PalettePulse, Sprite:SlashFlicker, Task:NightSlashPalette, Task:ShakeMon2Exact |
| 401 | AQUA_TAIL | 0 | 7 | 2 | Sprite:AquaTailSweep, Sprite:BattlerLunge, Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:BubbleRise, Sprite:PalettePulse, Sprite:Static, Task:ShakeMonExact, Task:ShakeMonInPlaceExact |
| 402 | SEED_BOMB | 0 | 2 | 1 | Sprite:BulletSeedRicochet, Sprite:OrientedStatic, Task:ShakeMon2Exact |
| 403 | AIR_SLASH | 0 | 1 | 2 | Sprite:SlashFlicker, Task:ShakeMon2Exact, Task:SlidingBackground |
| 404 | X_SCISSOR | 0 | 2 | 3 | Sprite:PalettePulse, Sprite:SliceArc, Task:ShakeMonExact, Task:TraceMonBlendedExact, Task:TranslateMonEllipticalRespectSide |
| 405 | BUG_BUZZ | 0 | 2 | 5 | Sprite:OrientedStatic, Sprite:RoarLine, Task:BlendBattleAnimPalExact, Task:BlendBattleAnimPalExcludeExact, Task:SlidingBackground, Task:UproarDistortion |
| 406 | DRAGON_PULSE | 0 | 2 | 2 | Sprite:PalettePulse, Sprite:TargetLocationProjectile, Task:BlendColorCycleExact, Task:SwayMonExact |
| 407 | DRAGON_RUSH | 0 | 5 | 1 | Sprite:BattlerLunge, Sprite:DragonRushStrike, Sprite:OrientedStatic, Sprite:PalettePulse, Sprite:RockFragmentLinear, Task:ShakeMonExact |
| 408 | POWER_GEM | 0 | 3 | 2 | Sprite:FastOrbit, Sprite:PalettePulse, Sprite:RadialScatter, Task:BlendMonInAndOutExact, Task:ScaleMonAndRestore |
| 409 | DRAIN_PUNCH | 0 | 4 | 1 | Sprite:AbsorptionArc, Sprite:BasicFistOrFootExact, Sprite:OrientedStatic, Task:ShakeMonExact |
| 410 | VACUUM_WAVE | 0 | 3 | 1 | Sprite:BasicFistOrFootExact, Sprite:OrientedStatic, Task:ShakeMonExact |
| 411 | FOCUS_BLAST | 0 | 1 | 2 | Sprite:SuperpowerOrbHold, Task:ShakeMon2Exact, Task:SlidingBackground |
| 412 | ENERGY_BALL | 0 | 2 | 1 | Sprite:PalettePulse, Sprite:ShadowBall, Task:ShakeMon2Exact |
| 413 | BRAVE_BIRD | 0 | 1 | 7 | Sprite:SkyAttackBird, Task:AttackerFadeFromInvisibleExact, Task:AttackerFadeToInvisibleExact, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact, Task:SlidingBackground |
| 414 | EARTH_POWER | 0 | 2 | 3 | Sprite:BattlerRestorePosition, Sprite:OrientedStatic, Task:FissureBackgroundPosition, Task:HorizontalShakeExact, Task:ShakeMonExact |
| 415 | SWITCHEROO | 0 | 1 | 3 | Sprite:TrickBagOrbit, Task:ShakeMonExact, Task:StretchAttackerUp, Task:StretchTargetUp |
| 416 | GIGA_IMPACT | 0 | 4 | 3 | Sprite:BattlerDip, Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Task:ShakeMonInPlaceExact |
| 417 | NASTY_PLOT | 0 | 1 | 1 | Sprite:QuestionMarkExact, Task:FadeScreenPaletteCycle |
| 418 | BULLET_PUNCH | 0 | 2 | 1 | Sprite:BasicFistOrFootExact, Sprite:OrientedStatic, Task:ShakeMonExact |
| 419 | AVALANCHE | 0 | 2 | 1 | Sprite:BattlerShake, Sprite:FallingRockEllipse, Task:ShakeMonExact |
| 420 | ICE_SHARD | 0 | 4 | 1 | Sprite:IceImpact, Sprite:MaxFlutterbyArc, Sprite:PalettePulse, Task:ShakeMon2Exact |
| 421 | SHADOW_CLAW | 0 | 2 | 1 | Sprite:BattlerLunge, Sprite:Static, Task:ShakeMon2Exact |
| 422 | THUNDER_FANG | 0 | 4 | 2 | Sprite:BiteClamp, Sprite:OrientedStatic, Sprite:PalettePulse, Task:InvertScreenColorExact, Task:ShakeMonExact |
| 423 | ICE_FANG | 0 | 7 | 2 | Sprite:BiteClamp, Sprite:GrowingCircle, Sprite:IceImpact, Sprite:OrientedStatic, Sprite:PalettePulse, Task:BlendBattleAnimPalExact, Task:ShakeMonExact |
| 424 | FIRE_FANG | 0 | 4 | 2 | Sprite:BiteClamp, Sprite:GrowingCircle, Sprite:LocalFixedVelocity, Sprite:OrientedStatic, Task:BlendBattleAnimPalExact, Task:ShakeMonExact |
| 425 | SHADOW_SNEAK | 0 | 1 | 6 | Sprite:IceImpact, Task:AttackerFadeToInvisibleExact, Task:BlendBattleAnimPalExact, Task:BlendColorCycleExact, Task:DestinyBondShadow, Task:ShakeMonExact, Task:TranslateMonEllipticalRespectSide |
| 426 | MUD_BOMB | 0 | 2 | 1 | Sprite:SludgeBurst, Sprite:StandardArcProjectile, Task:ShakeMonExact |
| 427 | PSYCHO_CUT | 0 | 3 | 4 | Sprite:OrientedStatic, Sprite:PsychoCutStrike, Task:BlendBattleAnimPalExact, Task:ShakeMonExact, Task:SwayMonExact, Task:TagBlendToColor |
| 428 | ZEN_HEADBUTT | 0 | 4 | 2 | Sprite:BattlerBow, Sprite:PalettePulse, Sprite:Static, Sprite:ZenHeadbuttExact, Task:ShakeMonExact, Task:ShakeMonInPlaceExact |
| 429 | MIRROR_SHOT | 0 | 1 | 5 | Sprite:RandomBattlerHit, Task:BlendBattleAnimPalExcludeExact, Task:HorizontalShakeExact, Task:MetallicShineExact, Task:TagBlendToColor |
| 430 | FLASH_CANNON | 0 | 2 | 4 | Sprite:ShadowBall, Sprite:Static, Task:BlendBattleAnimPalExact, Task:ElectricChargingParticles, Task:FlashExact, Task:ShakeMon2Exact |
| 431 | ROCK_CLIMB | 0 | 3 | 3 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Task:RolloutDebris, Task:ShakeMonInPlaceExact, Task:ShakeTargetPowerOrDamage |
| 432 | DEFOG | 0 | 1 | 2 | Sprite:OrientedStatic, Task:BlendBattleAnimPalExcludeExact, Task:TagBlendToColor |
| 433 | TRICK_ROOM | 0 | 0 | 1 | Task:ScaleMonAndRestore |
| 434 | DRACO_METEOR | 0 | 3 | 4 | Sprite:DracoMeteorRock, Sprite:OrientedStatic, Task:BlendBattleAnimPalExact, Task:HorizontalShakeExact, Task:ShakeMonExact, Task:TagBlendToColor |
| 435 | DISCHARGE | 0 | 4 | 5 | Sprite:OrientedStatic, Sprite:RandomBattlerHit, Sprite:StaticElectricity, Task:BlendBattleAnimPalExcludeExact, Task:BlendColorCycleExact, Task:HorizontalShakeExact, Task:TagBlendToColor |
| 436 | LAVA_PLUME | 0 | 1 | 2 | Sprite:LavaPlumeDrift, Task:BlendColorCycleExact, Task:ShakeMon2Exact |
| 437 | LEAF_STORM | 0 | 4 | 1 | Sprite:NeedleArmSpike, Sprite:OrientedStatic, Sprite:PalettePulse, Sprite:StandardLinearProjectile, Task:ShakeMonExact |
| 438 | POWER_WHIP | 0 | 3 | 2 | Sprite:BattlerLunge, Sprite:OrientedStatic, Sprite:TargetLocalLinear, Task:ShakeMonExact |
| 439 | ROCK_WRECKER | 0 | 6 | 3 | Sprite:NeedleArmSpike, Sprite:OrientedStatic, Sprite:RockFragmentLinear, Sprite:Static, Sprite:TargetLocationProjectile, Task:ShakeMon2Exact, Task:ShakeMonExact, Task:SlidingBackground |
| 440 | CROSS_POISON | 0 | 3 | 1 | Sprite:BubbleRise, Sprite:OrientedStatic, Sprite:PalettePulse, Task:ShakeMonExact |
| 441 | GUNK_SHOT | 0 | 3 | 3 | Sprite:BubbleRise, Sprite:GunkShotParticleWave, Sprite:Static, Task:ShakeMonExact, Task:SlidingBackground |
| 442 | IRON_HEAD | 0 | 2 | 4 | Sprite:BattlerBow, Sprite:FlashingHitFlicker, Task:MetallicShineExact, Task:SetGrayscaleOrOriginalPal, Task:ShakeMonExact, Task:ShakeMonInPlaceExact |
| 443 | MAGNET_BOMB | 0 | 4 | 2 | Sprite:FastOrbit, Sprite:OrientedStatic, Sprite:RadialScatter, Sprite:StaticElectricity, Task:BlendMonInAndOutExact, Task:ScaleMonAndRestore |
| 444 | STONE_EDGE | 0 | 3 | 1 | Sprite:OrientedStatic, Sprite:RandomBattlerHit, Sprite:Vortex, Task:ShakeMonInPlaceExact |
| 445 | CAPTIVATE | 0 | 2 | 1 | Sprite:LocalStarVelocity, Sprite:RisingHeartWave, Task:RockMonBackAndForth |
| 446 | STEALTH_ROCK | 0 | 1 | 0 | Sprite:StealthRockArc |
| 447 | GRASS_KNOT | 0 | 4 | 1 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:TargetSinWave, Task:RotateMonSpriteToSide |
| 448 | CHATTER | 0 | 3 | 2 | Sprite:IceImpact, Sprite:OrientedStatic, Sprite:RoarLine, Task:UproarDistortion |
| 449 | JUDGMENT | 0 | 3 | 3 | Sprite:NeedleArmSpike, Sprite:Static, Task:BlendBattleAnimPalExact, Task:ShakeMonExact, Task:TagBlendToColor |
| 450 | BUG_BITE | 0 | 1 | 1 | Sprite:RockFragmentLinear, Task:ShakeMonExact |
| 451 | CHARGE_BEAM | 0 | 3 | 4 | Sprite:PalettePulse, Sprite:SinWaveProjectile, Sprite:Static, Task:BlendBattleAnimPalExact, Task:ElectricChargingParticles, Task:ShakeMon2Exact, Task:ShakeMonExact |
| 452 | WOOD_HAMMER | 0 | 3 | 3 | Sprite:OrientedStatic, Sprite:TargetLocalLinear, Sprite:WoodHammerHammer, Task:ShakeMonInPlaceExact, Task:SmellingSaltsSquish, Task:TranslateMonEllipticalRespectSide |
| 453 | AQUA_JET | 0 | 4 | 4 | Sprite:DiveWaterSplash, Sprite:DriftingBubble, Sprite:OrientedStatic, Sprite:WaterSprayBallistic, Task:AttackerStretchAndDisappear, Task:ExtremeSpeedMonReappear, Task:PersistentAttackerHide, Task:ShakeMon2Exact |
| 454 | ATTACK_ORDER | 0 | 3 | 1 | Sprite:OrientedStatic, Sprite:RandomBattlerHit, Sprite:TwisterParticle, Task:ShakeMonInPlaceExact |
| 455 | DEFEND_ORDER | 0 | 1 | 2 | Sprite:TwisterParticle, Task:BlendColorCycleExact, Task:ShakeMon2Exact |
| 456 | HEAL_ORDER | 0 | 2 | 0 | Sprite:OrientedStatic, Sprite:TwisterParticle |
| 457 | HEAD_SMASH | 0 | 2 | 4 | Sprite:FlashingHitFlicker, Sprite:RockScatterBounce, Task:ShakeMonInPlaceExact, Task:SkullBashPosition |
| 458 | DOUBLE_HIT | 0 | 2 | 1 | Sprite:BattlerLunge, Sprite:OrientedStatic, Task:ShakeMonExact |
| 459 | ROAR_OF_TIME | 0 | 3 | 3 | Sprite:AbsorptionLinear, Sprite:OrientedStatic, Task:BlendBattleAnimPalExact, Task:ShakeMonInPlaceExact |
| 460 | SPACIAL_REND | 0 | 2 | 2 | Sprite:ExpandingSpiral, Sprite:LocalFixedVelocity, Task:BlendBattleAnimPalExact, Task:ShakeMonExact |
| 461 | LUNAR_DANCE | 0 | 4 | 4 | Sprite:DragonDanceOrbit, Sprite:MoonAbsolute, Sprite:MoonlightSparkleFall, Sprite:PalettePulse, Task:AlphaFadeInExact, Task:DragonDanceDistortion, Task:MoonlightEndFadeExact, Task:TagBlendInOut |
| 462 | CRUSH_GRIP | 0 | 2 | 1 | Sprite:OrientedStatic, Sprite:TargetLocationProjectile, Task:CompressTargetHorizontally |
| 463 | MAGMA_STORM | 0 | 1 | 4 | Sprite:Vortex, Task:BlendColorCycleExact, Task:SeismicTossBackground, Task:SeismicTossBackgroundEnd, Task:ShakeMonExact |
| 464 | DARK_VOID | 0 | 5 | 2 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:LocalStarVelocity, Task:DestinyBondShadow, Task:SlidingBackground |
| 465 | SEED_FLARE | 0 | 4 | 2 | Sprite:AbsorptionLinear, Sprite:FlyingSandCrescent, Sprite:StandardArcProjectile, Sprite:Static, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact |
| 466 | OMINOUS_WIND | 0 | 3 | 4 | Sprite:FlyingParticle, Task:BlendBattleAnimPalExact, Task:BlendBattleAnimPalExcludeExact, Task:SlidingBackground |
| 467 | SHADOW_FORCE | 1 | 1 | 7 | Sprite:PalettePulse, Task:AttackerFadeToInvisibleExact, Task:BlendColorCycleExact, Task:FadeScreenPaletteCycle, Task:NightShadeClone, Task:SetGrayscaleOrOriginalPal, Task:ShakeMon2Exact, Task:TranslateMonEllipticalRespectSide |
| 468 | HONE_CLAWS | 0 | 2 | 1 | Sprite:BattlerLunge, Sprite:Static, Task:TagBlendToColor |
| 469 | WIDE_GUARD | 0 | 1 | 2 | Sprite:ConversionHold, Task:ConversionAlphaBlendExact, Task:RockMonBackAndForth |
| 470 | GUARD_SPLIT | 0 | 2 | 1 | Sprite:MimicOrb, Sprite:StandardArcProjectile, Task:BlendBattleAnimPalExact |
| 471 | POWER_SPLIT | 0 | 2 | 1 | Sprite:MimicOrb, Sprite:StandardArcProjectile, Task:BlendBattleAnimPalExact |
| 472 | WONDER_ROOM | 0 | 0 | 1 | Task:ScaleMonAndRestore |
| 473 | PSYSHOCK | 0 | 2 | 4 | Sprite:OrientedStatic, Sprite:PoisonJabProjectile, Task:BlendColorCycleExact, Task:BlendMonInAndOutExact, Task:ShakeMon2Exact, Task:ShakeMonExact |
| 474 | VENOSHOCK | 0 | 1 | 0 | Sprite:OrientedStatic |
| 475 | AUTOTOMIZE | 0 | 1 | 2 | Sprite:TearDropArc, Task:TraceMonBlendedExact, Task:TranslateMonEllipticalRespectSide |
| 476 | RAGE_POWDER | 0 | 1 | 1 | Sprite:SporeOrbit, Task:BlendColorCycleExact |
| 477 | TELEKINESIS | 0 | 1 | 2 | Sprite:OrientedStatic, Task:BlendBattleAnimPalExact, Task:SwayMonExact |
| 478 | MAGIC_ROOM | 0 | 0 | 1 | Task:ScaleMonAndRestore |
| 479 | SMACK_DOWN | 0 | 3 | 3 | Sprite:OrientedStatic, Sprite:RockScatterBounce, Sprite:TargetLocationProjectile, Task:SeismicTossBackgroundEnd, Task:ShakeMonExact, Task:SmokescreenImpact |
| 480 | STORM_THROW | 0 | 2 | 1 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Task:RotateMonSpriteToSide |
| 481 | FLAME_BURST | 0 | 3 | 1 | Sprite:DragonFire, Sprite:LocalFixedVelocity, Sprite:OrientedStatic, Task:ShakeMon2Exact |
| 482 | SLUDGE_WAVE | 0 | 0 | 1 | Task:SurfWave |
| 483 | QUIVER_DANCE | 0 | 1 | 6 | Sprite:DragonDanceOrbit, Task:BlendBattleAnimPalExact, Task:BlendBattleAnimPalExcludeExact, Task:DragonDanceDistortion, Task:SlidingBackground, Task:TagBlendInOut |
| 484 | HEAVY_SLAM | 0 | 4 | 2 | Sprite:BattlerDip, Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Task:MetallicShineExact, Task:ShakeMonInPlaceExact |
| 485 | SYNCHRONOISE | 0 | 4 | 1 | Sprite:HyperVoiceRing, Sprite:LocalFixedVelocity, Task:BlendBattleAnimPalExact |
| 486 | ELECTRO_BALL | 0 | 3 | 1 | Sprite:OrientedStatic, Sprite:ShadowBall, Sprite:Static, Task:ShakeMon2Exact |
| 487 | SOAK | 0 | 3 | 2 | Sprite:DriftingBubble, Sprite:OrientedStatic, Sprite:StandardArcProjectile, Task:BlendColorCycleExact, Task:ShakeMon2Exact |
| 488 | FLAME_CHARGE | 0 | 5 | 2 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:LocalFixedVelocity, Sprite:OrientedStatic, Sprite:PetalDanceBig, Task:ShakeMonExact, Task:TranslateMonEllipticalRespectSide |
| 489 | COIL | 0 | 1 | 2 | Sprite:ConstrictBinding, Task:ShakeMon2Exact, Task:StretchTargetUp |
| 490 | LOW_SWEEP | 0 | 4 | 0 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:SlidingKickWave |
| 491 | ACID_SPRAY | 0 | 2 | 2 | Sprite:AcidBubbleArc, Sprite:TargetDroplet, Task:BlendColorCycleExact, Task:ShakeMon2Exact |
| 492 | FOUL_PLAY | 0 | 3 | 2 | Sprite:BattlerLunge, Sprite:OrientedStatic, Task:BlendBattleAnimPalExact, Task:ShakeMonExact |
| 493 | SIMPLE_BEAM | 0 | 4 | 2 | Sprite:OrientedStatic, Sprite:TargetLocationProjectile, Task:BlendBattleAnimPalExact, Task:SwayMonExact |
| 494 | ENTRAINMENT | 0 | 0 | 2 | Task:ShakeMonExact, Task:TeeterDanceMovement |
| 495 | AFTER_YOU | 0 | 1 | 1 | Sprite:AnchorOrientedStatic, Task:SwayMonExact |
| 496 | ROUND | 0 | 1 | 5 | Sprite:WavyMusicNote, Task:MusicNotePaletteClear, Task:MusicNotePaletteSetup, Task:ShakeMon2Exact, Task:UproarDistortion |
| 497 | ECHOED_VOICE | 0 | 1 | 1 | Sprite:TargetLocationProjectile, Task:ShakeMon2Exact |
| 498 | CHIP_AWAY | 0 | 1 | 3 | Sprite:OrientedStatic, Task:BlendBattleAnimPalExact, Task:ShakeMonExact, Task:TranslateMonEllipticalRespectSide |
| 499 | CLEAR_SMOG | 0 | 1 | 3 | Sprite:SwirlingFog, Task:BlendColorCycleExact, Task:ShakeMon2Exact, Task:TagBlendToColor |
| 500 | STORED_POWER | 0 | 2 | 4 | Sprite:FlashingHitFlicker, Sprite:RadialScatter, Task:PsychicPaletteCycle, Task:ShakeMon2Exact, Task:ShakeTargetPowerOrDamage, Task:SpitUpDeformMon |
| 501 | QUICK_GUARD | 0 | 2 | 1 | Sprite:BasicFistOrFootExact, Sprite:OrientedStatic, Task:BlendColorCycleExact |
| 502 | ALLY_SWITCH | 0 | 0 | 3 | Task:DoubleTeam, Task:PsychicPaletteCycle |
| 503 | SCALD | 0 | 2 | 3 | Sprite:OrientedStatic, Sprite:SinWaveProjectile, Task:BlendColorCycleExact, Task:ShakeMonExact |
| 504 | SHELL_SMASH | 0 | 4 | 1 | Sprite:LockingJaw, Sprite:OrientedStatic, Sprite:RockFragmentLinear, Task:ShakeMonExact |
| 505 | HEAL_PULSE | 0 | 3 | 0 | Sprite:OrientedStatic, Sprite:PresentHealRise |
| 506 | HEX | 0 | 1 | 4 | Sprite:GhostRiseFade, Task:BlendBattleAnimPalExact, Task:BlendColorCycleExact, Task:GrudgeFlames, Task:ShakeMonExact |
| 507 | SKY_DROP | 1 | 4 | 1 | Sprite:BlockBounce, Sprite:Falling, Sprite:OrientedStatic, Sprite:TargetLocationProjectile, Task:ShakeMonExact |
| 508 | SHIFT_GEAR | 0 | 1 | 0 | Sprite:SpriteOnMonDuration |
| 509 | CIRCLE_THROW | 0 | 4 | 1 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Task:TranslateMonEllipticalRespectSide |
| 510 | INCINERATE | 0 | 2 | 3 | Sprite:BattlerRestorePosition, Sprite:TargetLocationProjectile, Task:BlendMonInAndOutExact, Task:ShakeMonExact, Task:SwayMonExact |
| 511 | QUASH | 0 | 1 | 1 | Sprite:StompDrop, Task:SmellingSaltsSquish |
| 512 | ACROBATICS | 0 | 2 | 2 | Sprite:FlyBallRise, Sprite:Scatter, Task:ShakeMon2Exact, Task:SlidingBackground |
| 513 | REFLECT_TYPE | 0 | 7 | 0 | Sprite:RaiseSprite, Sprite:StringWrapBlink |
| 514 | RETALIATE | 0 | 1 | 3 | Sprite:GuillotinePincer, Task:AttackerPunchWithTraceExact, Task:BlendBattleAnimPalExact, Task:ShakeMonExact |
| 515 | FINAL_GAMBIT | 0 | 2 | 5 | Sprite:OrientedStatic, Sprite:ShadowBall, Task:BlendBattleAnimPalExact, Task:BlendColorCycleExact, Task:ScaleMonAndRestore, Task:ShakeMon2Exact, Task:ShakeMonExact |
| 516 | BESTOW | 0 | 0 | 1 | Task:BestowItemThrow |
| 517 | INFERNO | 0 | 1 | 4 | Sprite:Vortex, Task:BlendBattleAnimPalExact, Task:ShakeMonExact |
| 518 | WATER_PLEDGE | 0 | 2 | 2 | Sprite:OrientedStatic, Sprite:Rising, Task:BlendColorCycleExact, Task:ShakeMonExact |
| 519 | FIRE_PLEDGE | 0 | 1 | 2 | Sprite:Rising, Task:BlendColorCycleExact, Task:ShakeMonExact |
| 520 | GRASS_PLEDGE | 0 | 2 | 2 | Sprite:Rising, Sprite:WhipHitExact, Task:BlendColorCycleExact, Task:ShakeMonExact |
| 521 | VOLT_SWITCH | 0 | 2 | 1 | Sprite:Arc, Sprite:OrientedStatic, Task:ShakeMon2Exact |
| 522 | STRUGGLE_BUG | 0 | 2 | 4 | Sprite:MovementWavesRepeat, Sprite:RandomBattlerHit, Task:BlendBattleAnimPalExact, Task:FlailMovement, Task:ShakeTargetPowerOrDamage |
| 523 | BULLDOZE | 0 | 1 | 2 | Sprite:PalettePulse, Task:HorizontalShakeExact, Task:Splash |
| 524 | FROST_BREATH | 0 | 4 | 2 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:DragonFire, Sprite:OrientedStatic, Task:ShakeMonExact, Task:SlidingBackground |
| 525 | DRAGON_TAIL | 0 | 5 | 2 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:RockScatterBounce, Sprite:WhipHitExact, Task:ShakeMonInPlaceExact, Task:TagBlendToColor |
| 526 | WORK_UP | 0 | 1 | 2 | Sprite:EndureRise, Task:BlendColorCycleExact, Task:ShakeMon2Exact |
| 527 | ELECTROWEB | 0 | 3 | 1 | Sprite:OrientedStatic, Sprite:PalettePulse, Sprite:SpiderWebFade, Task:TagBlendToColor |
| 528 | WILD_CHARGE | 0 | 6 | 2 | Sprite:BattlerDip, Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:Static, Task:BlendBattleAnimPalExact, Task:ShakeMonExact |
| 529 | DRILL_RUN | 0 | 3 | 2 | Sprite:BattlerBow, Sprite:FlashingHitFlicker, Sprite:HornLunge, Task:FissureBackgroundPosition, Task:ShakeMonInPlaceExact |
| 530 | DUAL_CHOP | 0 | 3 | 1 | Sprite:BattlerLunge, Sprite:DiagonalStrike, Sprite:OrientedStatic, Task:ShakeMonExact |
| 531 | HEART_STAMP | 0 | 1 | 2 | Sprite:SpinningKickOrPunchExact, Task:ShakeMon2Exact, Task:StretchTargetUp |
| 532 | HORN_LEECH | 0 | 4 | 1 | Sprite:AbsorptionArc, Sprite:FlashingHitFlicker, Sprite:OrientedStatic, Sprite:ShadowBall, Task:ShakeMonExact |
| 533 | SACRED_SWORD | 0 | 3 | 2 | Sprite:LocalFixedVelocity, Sprite:SliceArc, Sprite:SwordRise, Task:ShakeMonExact, Task:TagFlash |
| 534 | RAZOR_SHELL | 0 | 2 | 1 | Sprite:SliceArc, Sprite:SpriteOnMonDuration, Task:ShakeMonExact |
| 535 | HEAT_CRASH | 0 | 3 | 1 | Sprite:OrientedStatic, Sprite:RockScatterBounce, Sprite:StompDrop, Task:ShakeMonExact |
| 536 | LEAF_TORNADO | 0 | 2 | 2 | Sprite:EllipticalGust, Sprite:Vortex, Task:ShakeMonExact, Task:TagPaletteRotate |
| 537 | STEAMROLLER | 0 | 3 | 1 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Task:ShakeMonInPlaceExact |
| 538 | COTTON_GUARD | 0 | 2 | 0 | Sprite:AbsorptionLinear, Sprite:SporeOrbit |
| 539 | NIGHT_DAZE | 0 | 2 | 2 | Sprite:OrientedStatic, Sprite:RadialScatter, Task:BlendBattleAnimPalExact, Task:ShakeMonExact |
| 540 | PSYSTRIKE | 0 | 0 | 5 | Task:BlendColorCycleExact, Task:InvertScreenColorExact, Task:ScaleMonAndRestore, Task:ShakeMon2Exact, Task:ShakeMonExact |
| 541 | TAIL_SLAP | 1 | 1 | 1 | Sprite:OrientedStatic, Task:ShakeMonExact |
| 542 | HURRICANE | 0 | 1 | 3 | Sprite:EllipticalGust, Task:ShakeMonExact, Task:SlidingBackground, Task:TagPaletteRotate |
| 543 | HEAD_CHARGE | 0 | 2 | 1 | Sprite:BattlerBow, Sprite:FlashingHitFlicker, Task:ShakeMonExact |
| 544 | GEAR_GRIND | 0 | 2 | 1 | Sprite:BiteClamp, Sprite:OrientedStatic, Task:ShakeMonExact |
| 545 | SEARING_SHOT | 0 | 3 | 5 | Sprite:RadialLinear, Sprite:SpinningKickOrPunchExact, Sprite:Static, Task:BlendBattleAnimPalExact, Task:ElectricChargingParticles, Task:ShakeMonExact, Task:SlidingBackground, Task:TagBlendToColor |
| 546 | TECHNO_BLAST | 0 | 5 | 3 | Sprite:AbsorptionLinear, Sprite:OrientedStatic, Sprite:Static, Sprite:TargetLocationProjectile, Sprite:ZapCannonSpark, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact |
| 547 | RELIC_SONG | 0 | 5 | 5 | Sprite:OrientedStatic, Sprite:RandomBattlerHit, Sprite:RoarLine, Sprite:WavyMusicNote, Task:MusicNotePaletteClear, Task:MusicNotePaletteSetup, Task:ShakeMonInPlaceExact, Task:UproarDistortion |
| 548 | SECRET_SWORD | 0 | 3 | 3 | Sprite:LocalFixedVelocity, Sprite:SliceArc, Sprite:SwordRise, Task:BlendBattleAnimPalExact, Task:ShakeMonExact, Task:TagFlash |
| 549 | GLACIATE | 0 | 3 | 1 | Sprite:IceImpact, Sprite:SmokeDrift, Task:BlendBattleAnimPalExact |
| 550 | BOLT_STRIKE | 0 | 5 | 4 | Sprite:BattlerRestorePosition, Sprite:BattlerSlideToOffset, Sprite:OrientedStatic, Sprite:Static, Task:BlendBattleAnimPalExact, Task:ElectricChargingParticles, Task:ShakeMon2Exact, Task:TranslateMonEllipticalRespectSide |
| 551 | BLUE_FLARE | 0 | 3 | 2 | Sprite:LocalFixedVelocity, Sprite:NeedleArmSpike, Sprite:Vortex, Task:BlendBattleAnimPalExact, Task:SlidingBackground |
| 552 | FIERY_DANCE | 0 | 1 | 2 | Sprite:Vortex, Task:BlendBattleAnimPalExact, Task:ShakeMonExact |
| 553 | FREEZE_SHOCK | 1 | 4 | 3 | Sprite:OrientedStatic, Sprite:Static, Sprite:TargetLocationProjectile, Sprite:ZapCannonSpark, Task:BlendBattleAnimPalExact, Task:HorizontalShakeExact, Task:ShakeMon2Exact |
| 554 | ICE_BURN | 1 | 4 | 2 | Sprite:IceImpact, Sprite:LocalFixedVelocity, Sprite:SmokeDrift, Task:BlendBattleAnimPalExact, Task:HorizontalShakeExact |
| 555 | SNARL | 0 | 4 | 2 | Sprite:OrientedStatic, Sprite:RandomBattlerHit, Sprite:RoarLine, Task:ShakeMonInPlaceExact, Task:UproarDistortion |
| 556 | ICICLE_CRASH | 0 | 1 | 1 | Sprite:FallingRockEllipse, Task:ShakeMonExact |
| 557 | V_CREATE | 0 | 7 | 5 | Sprite:BattlerLunge, Sprite:BattlerRestorePosition, Sprite:NeedleArmSpike, Sprite:OrientedStatic, Sprite:Static, Task:BlendBattleAnimPalExact, Task:ShakeMonExact, Task:ShakeMonInPlaceExact, Task:SlidingBackground, Task:WindUpLunge |
| 558 | FUSION_FLARE | 0 | 6 | 5 | Sprite:BattlerDip, Sprite:EndureRise, Sprite:OrientedStatic, Sprite:StompDrop, Sprite:WaterPulseBubble, Sprite:WeatherBallRise, Task:BlendBattleAnimPalExact, Task:BlendColorCycleExact, Task:HorizontalShakeExact, Task:ShakeMon2Exact, Task:TagBlendToColor |
| 559 | FUSION_BOLT | 0 | 3 | 5 | Sprite:OffscreenFly, Sprite:Static, Sprite:VoltTackleSlide, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact, Task:TagBlendToColor, Task:VoltTackleAttackerReappear, Task:VoltTackleBolts |
| 560 | FLYING_PRESS | 0 | 3 | 1 | Sprite:BlockBounce, Sprite:FlyBallRise, Sprite:OrientedStatic, Task:SmellingSaltsSquish |
| 561 | MAT_BLOCK | 0 | 1 | 1 | Sprite:ConversionHold, Task:ConversionAlphaBlendExact |
| 562 | BELCH | 0 | 2 | 1 | Sprite:OrientedStatic, Sprite:StandardArcProjectile, Task:ShakeMonInPlaceExact |
| 563 | ROTOTILLER | 0 | 2 | 2 | Sprite:DigDirtMoundExact, Sprite:DirtPlume, Task:BlendBattleAnimPalExact, Task:TranslateMonEllipticalRespectSide |
| 564 | STICKY_WEB | 0 | 3 | 0 | Sprite:PalettePulse, Sprite:SpiderWebFade, Sprite:WebThreadWave |
| 565 | FELL_STINGER | 0 | 2 | 1 | Sprite:OrientedStatic, Sprite:StandardLinearProjectile, Task:ShakeMon2Exact |
| 566 | PHANTOM_FORCE | 1 | 1 | 9 | Sprite:RandomBattlerHit, Task:AttackerFadeToInvisibleExact, Task:BlendColorCycleExact, Task:DestinyBondShadow, Task:GrudgeFlames, Task:NightShadeClone, Task:ShakeMon2Exact, Task:TagBlendToColor, Task:TranslateMonEllipticalRespectSide |
| 567 | TRICK_OR_TREAT | 0 | 1 | 3 | Sprite:GhostRiseFade, Task:BlendColorCycleExact, Task:ScaryFaceExact, Task:ShakeMon2Exact |
| 568 | NOBLE_ROAR | 0 | 2 | 3 | Sprite:PalettePulse, Sprite:RoarLine, Task:ScaleMonAndRestore, Task:ShakeMon2Exact |
| 569 | ION_DELUGE | 0 | 0 | 2 | Task:BlendBattleAnimPalExact, Task:IonRain |
| 570 | PARABOLIC_CHARGE | 0 | 3 | 6 | Sprite:OrientedStatic, Sprite:PalettePulse, Sprite:Static, Task:BlendBattleAnimPalExact, Task:ElectricChargingParticles, Task:ShakeMonExact, Task:ShockWaveLightning, Task:ShockWaveProgressingBolt |
| 571 | FORESTS_CURSE | 0 | 2 | 2 | Sprite:GhostRiseFade, Sprite:WaterPulseBubble, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact |
| 572 | PETAL_BLIZZARD | 0 | 5 | 1 | Sprite:OrientedStatic, Sprite:RandomBattlerHit, Sprite:TwisterParticle, Task:ShakeMonInPlaceExact |
| 573 | FREEZE_DRY | 0 | 5 | 3 | Sprite:GrowingCircle, Sprite:IceImpact, Sprite:PalettePulse, Task:BlendBattleAnimPalExact, Task:FrozenIceCube, Task:ShakeMonExact |
| 574 | DISARMING_VOICE | 0 | 1 | 4 | Sprite:RoarLine, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact |
| 575 | PARTING_SHOT | 0 | 1 | 3 | Sprite:RoarLine, Task:ShakeMon2Exact |
| 576 | TOPSY_TURVY | 0 | 1 | 1 | Sprite:PalettePulse, Task:RotateVerticallyExact |
| 577 | DRAINING_KISS | 0 | 5 | 2 | Sprite:AbsorptionArc, Sprite:HeartWaveProjectile, Sprite:OrientedStatic, Sprite:ParticleBurstWave, Task:ShakeMonExact, Task:SwayMonExact |
| 578 | CRAFTY_SHIELD | 0 | 1 | 3 | Sprite:ConversionHold, Task:BlendBattleAnimPalExact, Task:ConversionAlphaBlendExact, Task:TagFlash |
| 579 | FLOWER_SHIELD | 0 | 2 | 1 | Sprite:PetalDanceBig, Sprite:PetalDanceSmall, Task:BlendBattleAnimPalExact |
| 580 | GRASSY_TERRAIN | 0 | 2 | 3 | Sprite:FastOrbit, Sprite:RadialScatter, Task:BlendBattleAnimPalExact, Task:BlendMonInAndOutExact, Task:ScaleMonAndRestore |
| 581 | MISTY_TERRAIN | 0 | 2 | 3 | Sprite:FastOrbit, Sprite:RadialScatter, Task:BlendBattleAnimPalExact, Task:BlendMonInAndOutExact, Task:ScaleMonAndRestore |
| 582 | ELECTRIFY | 0 | 5 | 2 | Sprite:OrientedStatic, Sprite:RaiseSprite, Sprite:StaticElectricity, Sprite:ZapCannonSpark, Task:BlendBattleAnimPalExact, Task:ShakeMonExact |
| 583 | PLAY_ROUGH | 0 | 3 | 1 | Sprite:DizzyWave, Sprite:OrientedStatic, Sprite:PinkHeartWave, Task:TranslateMonElliptical |
| 584 | FAIRY_WIND | 0 | 1 | 2 | Sprite:StandardArcProjectile, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact |
| 585 | MOONBLAST | 0 | 3 | 3 | Sprite:MoonAbsolute, Sprite:PalettePulse, Sprite:TargetLocationProjectile, Task:AlphaFadeInExact, Task:MoonlightEndFadeExact, Task:ShakeMonExact |
| 586 | BOOMBURST | 0 | 2 | 6 | Sprite:HyperVoiceRing, Sprite:OrientedStatic, Task:InvertScreenColorExact, Task:ScaleMonAndRestore, Task:ShakeMon2Exact |
| 587 | FAIRY_LOCK | 0 | 0 | 2 | Task:BlendBattleAnimPalExact, Task:VoltTackleBolts |
| 588 | KINGS_SHIELD | 0 | 1 | 1 | Sprite:ProtectSlide, Task:TagBlendToColor |
| 589 | PLAY_NICE | 0 | 1 | 1 | Sprite:HeartWaveProjectile, Task:SwayMonExact |
| 590 | CONFIDE | 0 | 1 | 1 | Sprite:ThoughtBubble, Task:Splash |
| 591 | DIAMOND_STORM | 0 | 5 | 3 | Sprite:OrientedStatic, Sprite:RandomBattlerHit, Sprite:SwirlingSnowball, Sprite:TwisterParticle, Sprite:WavyBeyondTarget, Task:BlendBattleAnimPalExact, Task:ShakeMonInPlaceExact, Task:TagBlendToColor |
| 592 | STEAM_ERUPTION | 0 | 1 | 2 | Sprite:DragonFire, Task:BlendBattleAnimPalExact, Task:ShakeMon2Exact |
| 593 | HYPERSPACE_HOLE | 0 | 1 | 5 | Sprite:IceImpact, Task:BlendBattleAnimPalExact, Task:DestinyBondShadow, Task:ShakeMon2Exact, Task:SnatchOpposingMonMoveExact |
| 594 | WATER_SHURIKEN | 0 | 3 | 1 | Sprite:OrientedStatic, Sprite:ShadowBall, Sprite:WaterPulseRing, Task:ShakeMon2Exact |
| 595 | MYSTICAL_FIRE | 0 | 1 | 3 | Sprite:FlameDrift, Task:BlendColorCycleExact, Task:PsychicPaletteCycle, Task:ShakeMon2Exact |
| 596 | SPIKY_SHIELD | 0 | 2 | 0 | Sprite:NeedleArmSpike, Sprite:ProtectSlide |
| 597 | AROMATIC_MIST | 0 | 1 | 2 | Sprite:SweetScentPetal, Task:BlendBattleAnimPalExact, Task:BlendColorCycleExact |
| 598 | EERIE_IMPULSE | 0 | 4 | 0 | Sprite:HyperVoiceRing, Sprite:LocalStarVelocity, Sprite:OrientedStatic, Sprite:ZapCannonSpark |
| 599 | VENOM_DRENCH | 0 | 1 | 0 | Sprite:FallingRockEllipse |
| 600 | POWDER | 0 | 1 | 0 | Sprite:SporeOrbit |
| 601 | GEOMANCY | 1 | 9 | 2 | Sprite:EndureRise, Sprite:OrientedStatic, Sprite:RaiseSprite, Sprite:Vortex, Task:BlendBattleAnimPalExact, Task:ShakeMonExact |
| 602 | MAGNETIC_FLUX | 0 | 2 | 2 | Sprite:OrientedStatic, Sprite:StaticElectricity, Task:BlendBattleAnimPalExact, Task:DefenseCurlDeformMon |
| 603 | HAPPY_HOUR | 0 | 1 | 0 | Sprite:Falling |
| 604 | ELECTRIC_TERRAIN | 0 | 2 | 3 | Sprite:FastOrbit, Sprite:RadialScatter, Task:BlendBattleAnimPalExact, Task:BlendMonInAndOutExact, Task:ScaleMonAndRestore |
| 605 | DAZZLING_GLEAM | 0 | 3 | 4 | Sprite:LocalStarVelocity, Sprite:PalettePulse, Sprite:RandomBattlerHit, Task:BlendColorCycleExact, Task:ShakeMonExact, Task:TagBlendToColor |
| 606 | CELEBRATE | 0 | 1 | 0 | Sprite:FollowMeFinger |
| 607 | HOLD_HANDS | 0 | 2 | 4 | Sprite:HelpingHandClap, Sprite:PetalDanceBig, Task:BlendMonInAndOutExact, Task:HelpingHandAttackerMovement, Task:ShakeMon2Exact, Task:TranslateMonEllipticalRespectSide |
| 608 | BABY_DOLL_EYES | 0 | 1 | 3 | Sprite:OrientedStatic, Task:BlendBattleAnimPalExact, Task:ScaleMonAndRestore, Task:ShakeMon2Exact |
| 609 | NUZZLE | 0 | 3 | 1 | Sprite:OrientedStatic, Sprite:RisingHeartWave, Task:RockMonBackAndForth |
| 610 | HOLD_BACK | 0 | 4 | 1 | Sprite:DizzyWave, Sprite:OrientedStatic, Sprite:RevengeScratchExact, Task:ShakeMon2Exact |
| 611 | INFESTATION | 0 | 1 | 2 | Sprite:Vortex, Task:BlendBattleAnimPalExact, Task:ShakeMonExact |
| 612 | POWER_UP_PUNCH | 0 | 1 | 2 | Sprite:OrientedStatic, Task:GrowAndShrink, Task:ShakeMonExact |
| 613 | OBLIVION_WING | 0 | 3 | 4 | Sprite:FlyBallRise, Sprite:OrientedStatic, Sprite:TargetLocationProjectile, Task:BlendBattleAnimPalExact, Task:BlendColorCycleExact, Task:ShakeMonExact, Task:TagBlendToColor |
| 614 | THOUSAND_ARROWS | 0 | 4 | 2 | Sprite:MudSportDirt, Sprite:Rising, Sprite:Static, Task:BlendBattleAnimPalExact, Task:HorizontalShakeExact |
| 615 | THOUSAND_WAVES | 0 | 5 | 4 | Sprite:AbsorptionLinear, Sprite:DragonDanceOrbit, Sprite:FlyingSandCrescent, Sprite:Static, Sprite:Vortex, Task:AttackerStretchAndDisappear, Task:BlendBattleAnimPalExact, Task:ExtremeSpeedMonReappear, Task:HeatWaveTargetMotion |
| 616 | LANDS_WRATH | 0 | 1 | 3 | Sprite:Vortex, Task:BlendBattleAnimPalExact, Task:FissureBackgroundPosition, Task:HorizontalShakeExact |
| 617 | LIGHT_OF_RUIN | 0 | 3 | 6 | Sprite:HyperBeamOrb, Sprite:OrientedStatic, Sprite:Static, Task:BlendBattleAnimPalExact, Task:ElectricChargingParticles, Task:ShakeMon2Exact, Task:ShakeMonExact, Task:TagBlendToColor, Task:TagFlash |
