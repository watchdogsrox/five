<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<enumdef type="eWeaponWheelSlot">
		<enumval name="WHEEL_PISTOL"/>
		<enumval name="WHEEL_SMG"/>
		<enumval name="WHEEL_RIFLE"/>
		<enumval name="WHEEL_SNIPER"/>
		<enumval name="WHEEL_UNARMED_MELEE"/>
		<enumval name="WHEEL_SHOTGUN"/>
		<enumval name="WHEEL_HEAVY"/>
		<enumval name="WHEEL_THROWABLE_SPECIAL"/>
  </enumdef>

  <structdef type="CWeaponTintSpecValues">
    <string name="m_Name" type="atHashString" ui_key="true"/>
    <array name="m_Tints" type="atArray">
      <struct type="CWeaponTintSpecValues::CWeaponSpecValue"/>
    </array>
  </structdef>

  <structdef type="CWeaponTintSpecValues::CWeaponSpecValue">
    <float name="SpecFresnel" init="0.97f" min="0.0f" max="1.0f"/>
    <float name="SpecFalloffMult" init="100.0f" min="0.0f" max="512.0f"/>
    <float name="SpecIntMult" init="0.125f" min="0.0f" max="1.0f"/>
    <float name="Spec2Factor" init="40.0f" min="0.0f" max="512.0f"/>
    <float name="Spec2ColorInt" init="1.0f" min="0.0f" max="4.0f"/>
    <Color32 name="Spec2Color" init="1.0f"/>
  </structdef>

  <structdef type="CWeaponFiringPatternAliases">
    <string name="m_Name" type="atHashString" ui_key="true"/>
    <array name="m_Aliases" type="atArray">
      <struct type="CWeaponFiringPatternAliases::CFiringPatternAlias"/>
    </array>
  </structdef>

  <structdef type="CWeaponFiringPatternAliases::CFiringPatternAlias">
    <string name="FiringPattern" type="atHashString" ui_key="true"/>
    <string name="Alias" type="atHashString"/>
  </structdef>

  <const name="CWeaponUpperBodyFixupExpressionData::Max" value="4"/>
  
  <structdef type="CWeaponUpperBodyFixupExpressionData">
    <string name="m_Name" type="atHashString" ui_key="true"/>
    <array name="m_Data" type="member" size="CWeaponUpperBodyFixupExpressionData::Max">
      <struct type="CWeaponUpperBodyFixupExpressionData::Data"/>
    </array>
  </structdef>

  <enumdef type="CWeaponUpperBodyFixupExpressiCWeaponUpperBodyFixupExpressionDataon::Type">
    <enumval name="Normal"/>
    <enumval name="Action"/>
    <enumval name="Stealth"/>
    <enumval name="Aiming"/>
  </enumdef>
  
  <structdef type="CWeaponUpperBodyFixupExpressionData::Data">
    <float name="Idle" init="1.0"/>
    <float name="Walk" init="1.0"/>
    <float name="Run" init="1.0"/>
  </structdef>
  
  <structdef type="CAimingInfo">
    <string name="m_Name" type="atHashString" ui_key="true" />
    <float name="m_HeadingLimit" min="-90.0f" max="90.0f"/>
    <float name="m_SweepPitchMin" min="-90.0f" max="90.0f"/>
    <float name="m_SweepPitchMax" min="-90.0f" max="90.0f"/>
  </structdef>

  <const name="CWeaponComponentPoint::MAX_WEAPON_COMPONENTS" value="11"/>
  
  <structdef type="CWeaponComponentPoint" onPostLoad="OnPostLoad">
    <string name="m_AttachBone" type="atHashString"/>
    <array name="m_Components" type="atFixedArray" size="CWeaponComponentPoint::MAX_WEAPON_COMPONENTS">
      <struct type="CWeaponComponentPoint::sComponent"/>
    </array>
  </structdef>

  <structdef type="CWeaponComponentPoint::sComponent">
    <string name="m_Name" type="atHashString" ui_key="true" />
    <bool name="m_Default" init="false"/>
  </structdef>

  <structdef type="CWeaponInfo::sExplosion">
    <enum name="Default" type="eExplosionTag"/>
    <enum name="HitCar" type="eExplosionTag"/>
    <enum name="HitTruck" type="eExplosionTag"/>
    <enum name="HitBike" type="eExplosionTag"/>
    <enum name="HitBoat" type="eExplosionTag"/>
    <enum name="HitPlane" type="eExplosionTag"/>
  </structdef>

  <structdef type="CWeaponInfo::sFrontClearTestParams">
    <bool name="ShouldPerformFrontClearTest" init="false"/>
    <float name="ForwardOffset" init="0.0f"/>
    <float name="VerticalOffset" init="0.0f"/>
    <float name="HorizontalOffset" init="0.0f"/>
    <float name="CapsuleRadius" init="0.0f"/>
    <float name="CapsuleLength" init="0.0f"/>
  </structdef>

  <structdef type="CWeaponInfo::sFirstPersonScopeAttachmentData">
    <string name="m_Name" type="atHashString"/>
    <float name="m_FirstPersonScopeAttachmentFov" init="0.0f" min="0.0f" max="130.0f"/>
    <Vector3 name="m_FirstPersonScopeAttachmentOffset"/>
    <Vector3 name="m_FirstPersonScopeAttachmentRotationOffset"/>
  </structdef>

  <const name="CWeaponInfo::MAX_ATTACH_POINTS" value="7"/>
  
  <structdef type="CWeaponInfo" base="CItemInfo" onPostLoad="OnPostLoad">
    <enum name="m_DamageType" type="eDamageType"/>
    <struct name="m_Explosion" type="CWeaponInfo::sExplosion"/>
    <enum name="m_FireType" type="eFireType"/>
		<enum name="m_WheelSlot" type="eWeaponWheelSlot"/>
    <string name="m_Group" type="atHashString"/>
    <pointer name="m_AmmoInfo" type="CAmmoInfo" policy="external_named" toString="CAmmoInfo::GetNameFromInfo" fromString="CAmmoInfo::GetInfoFromName"/>
    <pointer name="m_AimingInfo" type="CAimingInfo" policy="external_named" toString="CAimingInfo::GetNameFromInfo" fromString="CAimingInfo::GetInfoFromName"/>
    <u32 name="m_ClipSize"/>
    <float name="m_AccuracySpread"/>
    <float name="m_AccurateModeAccuracyModifier" init="0.5f"/>
    <float name="m_RunAndGunAccuracyModifier" init="0.5f"/>
    <float name="m_RunAndGunAccuracyMinOverride" init="-1.0f"/>
    <float name="m_RecoilAccuracyMax" init="1.0f"/>
    <float name="m_RecoilErrorTime"/>
    <float name="m_RecoilRecoveryRate" init="1.0f"/>
    <float name="m_RecoilAccuracyToAllowHeadShotAI" init="1000.0f"/>
    <float name="m_MinHeadShotDistanceAI" init="1000.0f"/>
    <float name="m_MaxHeadShotDistanceAI" init="1000.0f"/>
    <float name="m_HeadShotDamageModifierAI" init="1000.0f"/>
    <float name="m_RecoilAccuracyToAllowHeadShotPlayer" init="0.175f"/>
    <float name="m_MinHeadShotDistancePlayer" init="5.0f"/>
    <float name="m_MaxHeadShotDistancePlayer" init="40.0f"/>
    <float name="m_HeadShotDamageModifierPlayer" init="3.0f"/>
    <float name="m_Damage"/>
    <float name="m_DamageTime" init="0.0f"/>
    <float name="m_DamageTimeInVehicle" init="0.0f"/>
    <float name="m_DamageTimeInVehicleHeadShot" init="0.0f"/>
    <float name="m_EnduranceDamage"/>
    <string name="m_PlayerDamageOverTimeWeapon" type="atHashString"/>
    <float name="m_HitLimbsDamageModifier" init="0.5f"/>
    <float name="m_NetworkHitLimbsDamageModifier" init="0.8f"/>
    <float name="m_LightlyArmouredDamageModifier" init="0.75f"/>
    <float name="m_VehicleDamageModifier" init="1.0f"/>
    <float name="m_Force"/>
    <float name="m_ForceHitPed" init="50.0f"/>
    <float name="m_ForceHitVehicle" init="50.0f"/>
    <float name="m_ForceHitFlyingHeli" init="50.0f"/>
    <array name="m_OverrideForces" type="atArray">
      <struct type="CWeaponInfo::sHitBoneForceOverride"/>
    </array>
    <float name="m_ForceMaxStrengthMult" init="1.0f"/>
    <float name="m_ForceFalloffRangeStart" init="0.0f"/>
    <float name="m_ForceFalloffRangeEnd" init="50.0f"/>
    <float name="m_ForceFalloffMin" init="1.0f"/>
    <float name="m_ProjectileForce"/>
    <float name="m_FragImpulse"/>
    <float name="m_Penetration" init="1.0f"/>
    <float name="m_VerticalLaunchAdjustment" init="0.0f"/>
    <float name="m_DropForwardVelocity" init="0.0f"/>    
    <float name="m_Speed" init="2000.0f"/>
    <u32 name="m_BulletsInBatch" init="1"/>
    <float name="m_BatchSpread" init="0.0f"/>
    <float name="m_ReloadTimeMP" init="-1.0f"/>
    <float name="m_ReloadTimeSP" init="-1.0f"/>
    <float name="m_VehicleReloadTime" init="1.0f"/>
    <float  name="m_AnimReloadRate" init="1.0f"/>
    <s32 name="m_BulletsPerAnimLoop" init="1"/>
    <float name="m_TimeBetweenShots"/>
    <float name="m_TimeLeftBetweenShotsWhereShouldFireIsCached" init="-1.0f"/>
    <float name="m_SpinUpTime" init="0.0f"/>
    <float name="m_SpinTime" init="0.0f"/>
    <float name="m_SpinDownTime" init="0.0f"/>
    <float name="m_AlternateWaitTime" init="-1.0f"/>
    <float name="m_BulletBendingNearRadius"/>
    <float name="m_BulletBendingFarRadius"/>
    <float name="m_BulletBendingZoomedRadius"/>
	<float name="m_FirstPersonBulletBendingNearRadius" init="0.0f"/>
	<float name="m_FirstPersonBulletBendingFarRadius" init="0.0f"/>
	<float name="m_FirstPersonBulletBendingZoomedRadius" init="0.0f"/>
    <struct name="m_Fx" type="CWeaponInfo::sFx"/>
    <s32 name="m_InitialRumbleDuration"/>
    <float name="m_InitialRumbleIntensity"/>
    <float name="m_InitialRumbleIntensityTrigger" init="0.0f"/>
    <s32 name="m_RumbleDuration"/>
    <float name="m_RumbleIntensity"/>
    <float name="m_RumbleIntensityTrigger" init="0.0f"/>
    <float name="m_RumbleDamageIntensity" init ="1.0f"/>
    <s32 name="m_InitialRumbleDurationFps" init="0"/>
    <float name="m_InitialRumbleIntensityFps" init="0.0f"/>
    <s32 name="m_RumbleDurationFps" init="0"/>
    <float name="m_RumbleIntensityFps" init="0.0f"/>
    <float name="m_NetworkPlayerDamageModifier" init="1.0f"/>
    <float name="m_NetworkPedDamageModifier" init="1.0f"/>
    <float name="m_NetworkHeadShotPlayerDamageModifier" init="1.0f"/>
    <float name="m_LockOnRange"/>
    <float name="m_WeaponRange"/>
    <float name="m_BulletDirectionOffsetInDegrees" init="0.0f"/>
    <float name="m_BulletDirectionPitchOffset" init="0.0f"/>
    <float name="m_BulletDirectionPitchHomingOffset" init="0.0f"/>
    <float name="m_AiSoundRange" min="-1.0f" max="1000.0f" init="-1.0f"/>
    <float name="m_AiPotentialBlastEventRange" min="-1.0f" max="1000.0f" init="-1.0f"/>
    <float name="m_DamageFallOffRangeMin"/>
    <float name="m_DamageFallOffRangeMax"/>
    <float name="m_DamageFallOffModifier" init="1.0f"/>
    <string name="m_VehicleWeaponHash" type="atHashString"/>
    <string name="m_DefaultCameraHash" type="atHashString"/>
    <string name="m_AimCameraHash" type="atHashString"/>
    <string name="m_FireCameraHash" type="atHashString"/>
    <string name="m_CoverCameraHash" type="atHashString"/>
    <string name="m_CoverReadyToFireCameraHash" type="atHashString"/>
    <string name="m_RunAndGunCameraHash" type="atHashString"/>
    <string name="m_CinematicShootingCameraHash" type="atHashString"/>
    <string name="m_AlternativeOrScopedCameraHash" type="atHashString"/>
    <string name="m_RunAndGunAlternativeOrScopedCameraHash" type="atHashString"/>
    <string name="m_CinematicShootingAlternativeOrScopedCameraHash" type="atHashString"/>
    <string name="m_PovTurretCameraHash" type="atHashString"/>
    <float name="m_CameraFov" init="45.0f" min="1.0f" max="130.0f"/>
    <float name="m_FirstPersonAimFovMin" init="42.0f" min="1.0f" max="130.0f"/>
    <float name="m_FirstPersonAimFovMax" init="47.0f" min="1.0f" max="130.0f"/>
    <float name="m_FirstPersonScopeFov" init="0.0f" min="0.0f" max="130.0f"/>
    <float name="m_FirstPersonScopeAttachmentFov" init="0.0f" min="0.0f" max="130.0f"/>
    <Vector3 name="m_FirstPersonDrivebyIKOffset" init="0.0f, 0.0f, 0.0f"/>
    <Vector3 name="m_FirstPersonRNGOffset"/>
    <Vector3 name="m_FirstPersonRNGRotationOffset"/>
    <Vector3 name="m_FirstPersonLTOffset"/>
    <Vector3 name="m_FirstPersonLTRotationOffset"/>
    <Vector3 name="m_FirstPersonScopeOffset"/>
    <Vector3 name="m_FirstPersonScopeAttachmentOffset"/>
    <Vector3 name="m_FirstPersonScopeRotationOffset"/>
    <Vector3 name="m_FirstPersonScopeAttachmentRotationOffset"/>
    <Vector3 name="m_FirstPersonAsThirdPersonIdleOffset"/>
    <Vector3 name="m_FirstPersonAsThirdPersonRNGOffset"/>
    <Vector3 name="m_FirstPersonAsThirdPersonLTOffset"/>
    <Vector3 name="m_FirstPersonAsThirdPersonScopeOffset"/>
    <Vector3 name="m_FirstPersonAsThirdPersonWeaponBlockedOffset"/>
    <float name="m_FirstPersonDofSubjectMagnificationPowerFactorNear" init="1.025f" min="0.0f" max="10.0f"/>
    <float name="m_FirstPersonDofMaxNearInFocusDistance" init="0.0f" min="0.0f" max="99999.0f"/>
    <float name="m_FirstPersonDofMaxNearInFocusDistanceBlendLevel" init="0.3f" min="0.0f" max="1.0f"/>
    <array name="m_FirstPersonScopeAttachmentData" type="atArray">
      <struct type="CWeaponInfo::sFirstPersonScopeAttachmentData"/>
    </array>
    <float name="m_ZoomFactorForAccurateMode" init="1.0f" min="1.0f" max="10.0f"/>
    <string name="m_RecoilShakeHash" type="atHashString"/>
    <string name="m_RecoilShakeHashFirstPerson" type="atHashString"/>
    <string name="m_AccuracyOffsetShakeHash" type="atHashString"/>
    <u32 name="m_MinTimeBetweenRecoilShakes" init="150" min="0" max="1000"/>
    <float name="m_RecoilShakeAmplitude" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_ExplosionShakeAmplitude" init="-1.0f" min="-1.0f" max="10.0f"/>
    <float name="m_IkRecoilDisplacement" init="0.0f" min="0.0f" max="1.0f"/>
    <float name="m_IkRecoilDisplacementScope" init="0.0f" min="0.0f" max="1.0f"/>
    <float name="m_IkRecoilDisplacementScaleBackward" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_IkRecoilDisplacementScaleVertical" init="0.4f" min="0.0f" max="10.0f"/>
    <Vector2 name="m_ReticuleHudPosition"/>
    <Vector2 name="m_ReticuleHudPositionOffsetForPOVTurret"/>
    <Vector3 name="m_AimOffsetMin"/> 
    <float name="m_AimProbeLengthMin"/>
    <Vector3 name="m_AimOffsetMax"/>
    <float name="m_AimProbeLengthMax"/>
    <Vector3 name="m_AimOffsetMinFPSIdle"/>
    <Vector3 name="m_AimOffsetMedFPSIdle"/>
    <Vector3 name="m_AimOffsetMaxFPSIdle"/>
    <Vector3 name="m_AimOffsetMinFPSLT"/>
    <Vector3 name="m_AimOffsetMaxFPSLT"/>
    <Vector3 name="m_AimOffsetMinFPSRNG"/>
    <Vector3 name="m_AimOffsetMaxFPSRNG"/>
    <Vector3 name="m_AimOffsetMinFPSScope"/>
    <Vector3 name="m_AimOffsetMaxFPSScope"/>
    <Vector3 name="m_AimOffsetEndPosMinFPSIdle"/>
    <Vector3 name="m_AimOffsetEndPosMedFPSIdle"/>
    <Vector3 name="m_AimOffsetEndPosMaxFPSIdle"/>
    <Vector3 name="m_AimOffsetEndPosMinFPSLT"/>
    <Vector3 name="m_AimOffsetEndPosMedFPSLT"/>
    <Vector3 name="m_AimOffsetEndPosMaxFPSLT"/>
    <float name="m_AimProbeRadiusOverrideFPSIdle"/>
    <float name="m_AimProbeRadiusOverrideFPSIdleStealth"/>
    <float name="m_AimProbeRadiusOverrideFPSLT"/>
    <float name="m_AimProbeRadiusOverrideFPSRNG"/>
    <float name="m_AimProbeRadiusOverrideFPSScope"/>
    <Vector2 name="m_TorsoAimOffset"/>
    <Vector2 name="m_TorsoCrouchedAimOffset"/>
    <Vector3 name="m_LeftHandIkOffset"/>
    <float name="m_ReticuleMinSizeStanding"/>
    <float name="m_ReticuleMinSizeCrouched"/>
    <float name="m_ReticuleScale"/>
    <string name="m_ReticuleStyleHash" type="atHashString"/>
    <string name="m_FirstPersonReticuleStyleHash" type="atHashString"/>
    <string name="m_PickupHash" type="atHashString"/>
    <string name="m_MPPickupHash" type="atHashString"/>
    <string name="m_HumanNameHash" type="atHashString" init="WT_INVALID"/>
    <string name="m_MovementModeConditionalIdle" type="atHashString"/>
    <string name="m_StatName" type="ConstString"/>
    <s32 name="m_KnockdownCount" init="-1"/>
    <float name="m_KillshotImpulseScale" init="1.0f"/>
    <string name="m_NmShotTuningSet" type="atHashString" init="normal"/>
    <array name="m_AttachPoints" type="atFixedArray" size="CWeaponInfo::MAX_ATTACH_POINTS">
      <struct type="CWeaponComponentPoint"/>
    </array>
    <struct name="m_GunFeedBone" type="CWeaponBoneId"/>
    <string name="m_TargetSequenceGroup" type="atHashString"/>
    <bitset name="m_WeaponFlags" type="fixed" numBits="use_values" values="CWeaponInfoFlags::Flags"/>
    <pointer name="m_TintSpecValues" type="CWeaponTintSpecValues" policy="external_named" toString="CWeaponTintSpecValues::GetNameFromPointer" fromString="CWeaponTintSpecValues::GetPointerFromName"/>
    <pointer name="m_FiringPatternAliases" type="CWeaponFiringPatternAliases" policy="external_named" toString="CWeaponFiringPatternAliases::GetNameFromPointer" fromString="CWeaponFiringPatternAliases::GetPointerFromName"/>
    <pointer name="m_ReloadUpperBodyFixupExpressionData" type="CWeaponUpperBodyFixupExpressionData" policy="external_named" toString="CWeaponUpperBodyFixupExpressionData::GetNameFromPointer" fromString="CWeaponUpperBodyFixupExpressionData::GetPointerFromName"/>
    <u8 name="m_AmmoDiminishingRate" init="0" min="0" max="255"/>
    <float name="m_AimingBreathingAdditiveWeight" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_FiringBreathingAdditiveWeight" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_StealthAimingBreathingAdditiveWeight" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_StealthFiringBreathingAdditiveWeight" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_AimingLeanAdditiveWeight" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_FiringLeanAdditiveWeight" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_StealthAimingLeanAdditiveWeight" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_StealthFiringLeanAdditiveWeight" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_ExpandPedCapsuleRadius" init="0.0f" min="0.0f" max="1.0f"/>
    <string name="m_AudioCollisionHash" type="atHashString"/> 

	<s8 name="m_HudDamage"	  init="-1"/>
	<s8 name="m_HudSpeed"	  init="-1"/>
	<s8 name="m_HudCapacity"  init="-1"/>
	<s8 name="m_HudAccuracy"  init="-1"/>
	<s8 name="m_HudRange"     init="-1"/>

	<float name="m_VehicleAttackAngle" init="25.0f" min="0.0f" max="90.0f"/>

  <float name="m_TorsoIKAngleLimit" init="-1.0f" min="-100.0f" max="100.0f"/>

  <float name="m_MeleeDamageMultiplier" init="-1.0f" min="-1.0f" max="10.0f"/>
  <float name="m_MeleeRightFistTargetHealthDamageScaler" init="-1.0f" min="-1.0f" max="10.0f"/>

  <float name="m_AirborneAircraftLockOnMultiplier" init="1.0f" min="1.0f" max="10.0f"/>

  <float name="m_ArmouredVehicleGlassDamageOverride" init="-1.0f" min="-1.0f" max="100.0f"/>

    <map name="m_CamoDiffuseTexIdxs" type="atBinaryMap" key="atHashString">
      <map type="atBinaryMap" key="u32">
        <u32/>
      </map>
    </map>

    <struct name="m_RotateBarrelBone" type="CWeaponBoneId"/>
    <struct name="m_RotateBarrelBone2" type="CWeaponBoneId"/>
    <struct name="m_FrontClearTestParams" type="CWeaponInfo::sFrontClearTestParams"/>
  </structdef>

  <structdef type="CWeaponInfo::sFx">
    <enum name="EffectGroup" type="eWeaponEffectGroup"/>
    <string name="FlashFx" type="atHashString"/>
    <string name="FlashFxAlt" type="atHashString"/>
    <string name="FlashFxFP" type="atHashString"/>
    <string name="FlashFxFPAlt" type="atHashString"/>
    <string name="MuzzleSmokeFx" type="atHashString"/>
    <string name="MuzzleSmokeFxFP" type="atHashString"/>
    <float name="MuzzleSmokeFxMinLevel" init="0.0f"/>
    <float name="MuzzleSmokeFxIncPerShot" init="0.0f"/>
    <float name="MuzzleSmokeFxDecPerSec" init="0.0f"/>
    <Vector3 name="MuzzleOverrideOffset"/>
    <bool name="MuzzleUseProjTintColour" init="false" />
    <string name="ShellFx" type="atHashString"/>
    <string name="ShellFxFP" type="atHashString"/>
    <string name="TracerFx" type="atHashString"/>
    <string name="PedDamageHash" type="atHashString"/>
    <float name="TracerFxChanceSP" init="0.0f"/>
    <float name="TracerFxChanceMP" init="0.0f"/>
    <bool name="TracerFxIgnoreCameraIntersection" init="false"/>
    <float name="FlashFxChanceSP" init="0.0f"/>
    <float name="FlashFxChanceMP" init="0.0f"/>
    <float name="FlashFxAltChance" init="0.0f"/>
    <float name="FlashFxScale" init="1.0f"/>
    <bool name="FlashFxLightEnabled" init="false"/>
    <bool name="FlashFxLightCastsShadows" init="false"/>
    <float name="FlashFxLightOffsetDist" init="0.2f"/>
    <Vector3 name="FlashFxLightRGBAMin"/>
    <Vector3 name="FlashFxLightRGBAMax"/>
    <Vector2 name="FlashFxLightIntensityMinMax"/>
    <Vector2 name="FlashFxLightRangeMinMax"/>
    <Vector2 name="FlashFxLightFalloffMinMax"/>
    <bool name="GroundDisturbFxEnabled" init="false"/>
    <float name="GroundDisturbFxDist" init="5.0f"/>
    <string name="GroundDisturbFxNameDefault" type="atHashString"/>
    <string name="GroundDisturbFxNameSand" type="atHashString"/>
    <string name="GroundDisturbFxNameDirt" type="atHashString"/>
    <string name="GroundDisturbFxNameWater" type="atHashString"/>
    <string name="GroundDisturbFxNameFoliage" type="atHashString"/>
  </structdef>

  <structdef type="CWeaponInfo::sHitBoneForceOverride">
    <enum name="BoneTag" type="eAnimBoneTag"/>
    <float name="ForceFront" init="50.0f"/>
    <float name="ForceBack" init="50.0f"/>
  </structdef>

</ParserSchema>
