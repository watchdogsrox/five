<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="CAmmoInfo::Flags">
    <enumval name="InfiniteAmmo"/>
    <enumval name="AddSmokeOnExplosion"/>
    <enumval name="Fuse"/>
    <enumval name="FixedAfterExplosion"/>
  </enumdef>

  <enumdef type="CAmmoInfo::AmmoSpecialType">
    <enumval name="None" value="0"/>
    <enumval name="ArmorPiercing"/>
    <enumval name="Explosive"/>
    <enumval name="FMJ"/>
    <enumval name="HollowPoint"/>
    <enumval name="Incendiary"/>
    <enumval name="Tracer"/>
  </enumdef>

  <structdef type="CAmmoInfo" base="CItemInfo">
    <int name="m_AmmoMax" init="1" min="1" max="15000"/>
    <int name="m_AmmoMax50" init="1" min="1" max="15000"/>
    <int name="m_AmmoMax100" init="1" min="1" max="15000"/>
    <int name="m_AmmoMaxMP" init="1" min="1" max="15000"/>
    <int name="m_AmmoMax50MP" init="1" min="1" max="15000"/>
    <int name="m_AmmoMax100MP" init="1" min="1" max="15000"/>
    <bitset name="m_AmmoFlags" type="fixed8" values="CAmmoInfo::Flags"/>
    <enum name="m_AmmoSpecialType" type="CAmmoInfo::AmmoSpecialType"/>
  </structdef>

  <enumdef type="CAmmoProjectileInfo::Flags">
    <enumval name="Sticky"/>
    <enumval name="DestroyOnImpact"/>
    <enumval name="ProcessImpacts"/>
    <enumval name="HideDrawable"/>
    <enumval name="TrailFxInactiveOnceWet"/>
    <enumval name="TrailFxRemovedOnImpact"/>
    <enumval name="DoGroundDisturbanceFx"/>
    <enumval name="CanBePlaced"/>
    <enumval name="NoPullPin"/>
    <enumval name="DelayUntilSettled"/>
    <enumval name="CanBeDestroyedByDamage"/>
    <enumval name="CanBounce"/>
    <enumval name="DoubleDamping"/>
    <enumval name="StickToPeds"/>
    <enumval name="DontAlignOnStick"/>
    <enumval name="ThrustUnderwater"/>
    <enumval name="ApplyDamageOnImpact"/>
    <enumval name="SetOnFireOnImpact"/>
    <enumval name="DontFireAnyEvents"/>
    <enumval name="AlignWithTrajectory"/>
    <enumval name="ExplodeAtTrailFxPos"/>
    <enumval name="ProximityDetonation"/>
    <enumval name="AlignWithTrajectoryYAxis"/>
    <enumval name="HomingAttractor"/>
    <enumval name="Cluster"/>
    <enumval name="PreventMaxProjectileHelpText"/>
    <enumval name="ProximityRepeatedDetonation"/>
    <enumval name="UseGravityOutOfWater"/>
  </enumdef>

  <structdef type="CAmmoProjectileInfo" base="CAmmoInfo" onPostLoad="OnPostLoad">
    <float name="m_Damage"/>
    <float name="m_LifeTime"/>
    <float name="m_FromVehicleLifeTime"/>
    <float name="m_LifeTimeAfterImpact" init="0.0f"/>
    <float name="m_LifeTimeAfterExplosion" init="0.0f"/>
    <float name="m_ExplosionTime" init="0.0f"/>
    <float name="m_LaunchSpeed"/>
    <float name="m_SeparationTime" init="0.0f"/>
    <float name="m_TimeToReachTarget" init="1"/>
    <float name="m_Damping"/>
    <float name="m_GravityFactor"/>
    <float name="m_RicochetTolerance"/>
    <float name="m_PedRicochetTolerance"/>
    <float name="m_VehicleRicochetTolerance"/>
    <float name="m_FrictionMultiplier" init="1.0f"/>
    <struct name="m_Explosion" type="CAmmoProjectileInfo::sExplosion"/>
    <string name="m_FuseFx" type="atHashString"/>
    <string name="m_ProximityFx" type="atHashString"/>
    <string name="m_TrailFx" type="atHashString"/>
    <string name="m_TrailFxUnderWater" type="atHashString"/>
    <string name="m_FuseFxFP" type="atHashString"/>
    <string name="m_PrimedFxFP" type="atHashString"/>
    <float name="m_TrailFxFadeInTime"/>
    <float name="m_TrailFxFadeOutTime"/>
    <string name="m_PrimedFx" type="atHashString"/>
    <string name="m_DisturbFxDefault" type="atHashString"/>
    <string name="m_DisturbFxSand" type="atHashString"/>
    <string name="m_DisturbFxWater" type="atHashString"/>
    <string name="m_DisturbFxDirt" type="atHashString"/>
    <string name="m_DisturbFxFoliage" type="atHashString"/>
    <float name="m_DisturbFxProbeDist"/>
    <float name="m_DisturbFxScale"/>
    <float name="m_GroundFxProbeDistance" init="2.5f"/>
    <bool name="m_FxAltTintColour" init="false"/>
    <bool name="m_LightOnlyActiveWhenStuck"/>
    <bool name="m_LightFlickers"/>
    <bool name="m_LightSpeedsUp" init="false"/>
    <struct name="m_LightBone" type="CWeaponBoneId"/>
    <Vector3 name="m_LightColour"/>
    <float name="m_LightIntensity"/>
    <float name="m_LightRange"/>
    <float name="m_LightFalloffExp"/>
    <float name="m_LightFrequency"/>
    <float name="m_LightPower"/>
    <float name="m_CoronaSize"/>
    <float name="m_CoronaIntensity"/>
    <float name="m_CoronaZBias"/>
    <bitset name="m_ProjectileFlags" type="fixed32" values="CAmmoProjectileInfo::Flags"/>
    <float name="m_ProximityActivationTime" init="3.0f"/>
    <float name="m_ProximityRepeatedDetonationActivationTime" init="2.0f"/>
    <float name="m_ProximityTriggerRadius" init="-1.0f"/>
    <float name="m_ProximityFuseTimePed" init="1.0f"/>
    <float name="m_ProximityFuseTimeVehicleMin" init="0.5f"/>
    <float name="m_ProximityFuseTimeVehicleMax" init="0.05f"/>
    <float name="m_ProximityFuseTimeVehicleSpeed" init="30.0f"/>
    <Vector3 name="m_ProximityLightColourUntriggered"/>
    <float name="m_ProximityLightFrequencyMultiplierTriggered" init="4.0f"/>
    <bool name="m_ProximityAffectsFiringPlayer" init="true"/>
	<bool name="m_ProximityCanBeTriggeredByPeds" init="true"/>
    <float name="m_TimeToIgnoreOwner" init="0.0f"/>
    <float name="m_ChargedLaunchTime" init="-1.0"/>
    <float name="m_ChargedLaunchSpeedMult" init="1.0"/>
	<enum name="m_ClusterExplosionTag" type="eExplosionTag"/>
    <u32 name="m_ClusterExplosionCount" init="5"/>
    <float name="m_ClusterMinRadius" init="5.0f"/>
    <float name="m_ClusterMaxRadius" init="10.0f"/>
    <float name="m_ClusterInitialDelay" init="0.5f"/>
    <float name="m_ClusterInbetweenDelay" init="0.25f"/>

  </structdef>

  <structdef type="CAmmoProjectileInfo::sExplosion">
    <enum name="Default" type="eExplosionTag"/>
    <enum name="HitCar" type="eExplosionTag"/>
    <enum name="HitTruck" type="eExplosionTag"/>
    <enum name="HitBike" type="eExplosionTag"/>
    <enum name="HitBoat" type="eExplosionTag"/>
    <enum name="HitPlane" type="eExplosionTag"/>
  </structdef>

  <structdef type="CAmmoProjectileInfo::sHomingRocketParams">
    <bool name="ShouldUseHomingParamsFromInfo" init="false"/>
    <bool name="ShouldIgnoreOwnerCombatBehaviour" init="false"/>
    <float name="TimeBeforeStartingHoming" init="0.0f" min="0.0f" max="100.0f"/>
    <float name="TimeBeforeHomingAngleBreak" init="0.0f" min="0.0f" max="100.0f"/>
    <float name="TurnRateModifier" init="0.0f" min="0.0f" max="100.0f"/>
    <float name="PitchYawRollClamp" init="0.0f" min="0.0f" max="100.0f"/>
    <float name="DefaultHomingRocketBreakLockAngle" init="0.0f" min="-10.0f" max="10.0f"/>
    <float name="DefaultHomingRocketBreakLockAngleClose" init="0.0f" min="-10.0f" max="10.0f"/>
    <float name="DefaultHomingRocketBreakLockCloseDistance" init="0.0f" min="0.0f" max="1000.0f"/>
  </structdef>
  
  <structdef type="CAmmoRocketInfo" base="CAmmoProjectileInfo">
    <float name="m_ForwardDragCoeff"/>
    <float name="m_SideDragCoeff"/>
    <float name="m_TimeBeforeHoming"/>
    <float name="m_TimeBeforeSwitchTargetMin"/>
    <float name="m_TimeBeforeSwitchTargetMax"/>
    <float name="m_ProximityRadius"/>
    <float name="m_PitchChangeRate"/>
    <float name="m_YawChangeRate"/>
    <float name="m_RollChangeRate"/>
    <float name="m_MaxRollAngleSin"/>
    <float name="m_LifeTimePlayerVehicleLockedOverrideMP" init="-1.0f"/>
    <struct name="m_HomingRocketParams" type="CAmmoProjectileInfo::sHomingRocketParams"/>
  </structdef>

  <structdef type="CAmmoThrownInfo" base="CAmmoProjectileInfo">
    <float name="m_ThrownForce"/>
    <float name="m_ThrownForceFromVehicle"/>
    <int name="m_AmmoMaxMPBonus" init="0" min="0" max="100"/>
  </structdef>

</ParserSchema>
