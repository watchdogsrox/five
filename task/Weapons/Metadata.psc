<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CTaskGun::Tunables" base="CTuning">
    <s32 name="m_iMinLookAtTime" min="0" max="10000" step="1" init="160"/>
    <s32 name="m_iMaxLookAtTime" min="0" max="10000" step="1" init="240"/>
    <float name="m_fMinTimeBetweenBulletReactions" min="0.0f" max="100.0f" step="0.05f" init="2.75f"/>
    <float name="m_fMaxTimeBetweenBulletReactions" min="0.0f" max="100.0f" step="0.05f" init="4.0f"/>
    <float name="m_fMaxDistForOverheadReactions" min="0.0f" max="10.0f" step="0.1f" init="0.75f"/>
    <float name="m_fMaxAboveHeadForOverheadReactions" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
    <float name="m_fBulletReactionPosAdjustmentZ" min="0.0f" max="5.0f" step="0.1f" init="0.2f"/>
    <float name="m_fMinTimeBetweenLookAt" min="0.0f" max="100.0f" step="0.05f" init="2.1f"/>
    <float name="m_fMaxTimeBetweenLookAt" min="0.0f" max="100.0f" step="0.05f" init="3.9f"/>
    <bool name="m_bDisable2HandedGetups" init="true"/>
    <float name="m_TimeForEyeIk" min="0.0f" max="10.0f" step="0.1f" init="3.0f"/>
    <float name="m_MinTimeBetweenEyeIkProcesses" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <float name="m_MinDotToPointGunAtPositionWhenUnableToTurn" min="-1.0f" max="1.0f" step="0.001f" init="0.707f"/>
    <string name="m_AssistedAimCamera" type="atHashString" init="DEFAULT_THIRD_PERSON_PED_ASSISTED_AIM_CAMERA" />
    <string name="m_RunAndGunAimCamera" type="atHashString" init="DEFAULT_THIRD_PERSON_PED_RUN_AND_GUN_AIM_CAMERA" />
    <u32 name="m_AssistedAimInterpolateInDuration" min="0" max="10000" init="750"/>
    <u32 name="m_RunAndGunInterpolateInDuration" min="0" max="10000" init="250"/>
    <u32 name="m_MinTimeBetweenOverheadBulletReactions" min="0" max="10000" init="250"/>
    <float name="m_MaxTimeInBulletReactionState" min="0.0f" max="10.0f" step="0.05f" init="2.25f"/>
	<u32 name="m_uFirstPersonRunAndGunWhileSprintTime" min="0" max="10000" init="1000"/>
    <u32 name="m_uFirstPersonMinTimeToSprint" min="0" max="10000" init="100"/>
  </structdef>

  <structdef type="CTaskAimGunOnFoot::Tunables" base="CTuning">
    <float name="m_MinTimeBetweenFiringVariations" min="0.0f" max="60.0f" step="1.0f" init="10.0f"/>
    <float name="m_IdealPitchForFiringVariation" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
    <float name="m_MaxPitchDifferenceForFiringVariation" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
    <float name="m_AssistedAimOutroTime" min="0.0f" max="10.0f" step="0.01f" init="1.0f"/>
	  <float name="m_RunAndGunOutroTime" min="0.0f" max="10.0f" step="0.01f" init="5.0f"/>
	  <float name="m_AimOutroTime" min="0.0f" max="10.0f" step="0.01f" init="1.0f"/>
    <float name="m_AimOutroTimeIfAimingOnStick" min="0.0f" max="100.0f" step="0.01f" init="5.0f"/>
    <float name="m_AimOutroMinTaskTimeWhenRunPressed" min="0.0f" max="100.0f" step="0.01f" init="1.0f"/>
    <float name="m_AimingOnStickExitCooldown" min="0.0f" max="10.0f" step="0.01f" init="0.25f"/>
    <float name="m_TimeForRunAndGunOutroDelays" min="0.0f" max="9999.0f" step="0.01f" init="1.0f"/>
    <float name="m_DampenRootTargetWeight" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_DampenRootTargetHeight" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_AlternativeAnimBlockedHeight" min="0.0f" max="1.0f" step="0.01f" init="0.220f"/>
    <Vector3 name="m_CoverAimOffsetFromBlocked" init="0.0f, 0.0f, 0.5f" min="0.0f" max="100.0f"/>
	<u32 name="m_DelayTimeWhenOutOfAmmoInScope" min="0" max="10000" init="500"/>
  </structdef>

  <structdef type="CTaskAimAndThrowProjectile::Tunables" base="CTuning">
    <bool name="m_bEnableGaitAdditive" init="true"/>
    <float name="m_fMinHoldThrowPitch" min="0.0f" max="1.0f" step="0.01f" init="0.35f"/>
    <u32 name="m_iMaxRandomExplosionTime" min="0" max="4000" step="1" init="300"/>
  </structdef>

	<structdef type="CTaskAimGunVehicleDriveBy::Tunables" base="CTuning">
		<float name="m_MinTimeBetweenInsults" min="0.0f" max="10.0f" step="1.0f" init="5.0f"/>
		<float name="m_MaxDistanceToInsult" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
		<float name="m_MinDotToInsult" min="-1.0f" max="1.0f" step="0.01f" init="0.707f"/>
		<u32 name="m_MinAimTimeMs" min="0" max="10000" init="850"/> 
		<u32 name="m_MaxAimTimeOnStickMs" min="0" max="10000" init="5000"/> 
		<u32 name="m_AimingOnStickCooldownMs" min="0" max="10000" init="400"/>
		<string name="m_BicycleDrivebyFilterId" type="atHashString" init="Upperbody_filter"/>
		<string name="m_BikeDrivebyFilterId" type="atHashString" init="Upperbody_filter"/>
		<string name="m_JetskiDrivebyFilterId" type="atHashString" init="Upperbody_filter"/>
		<string name="m_ParachutingFilterId" type="atHashString" init="Upperbody_filter"/>
		<string name="m_FirstPersonGripLeftClipId" type="atHashString" init="AIM_GRIP_DRIVEBY_LEFT"/>
		<string name="m_FirstPersonGripRightClipId" type="atHashString" init="AIM_GRIP_DRIVEBY"/>
		<string name="m_FirstPersonFireLeftClipId" type="atHashString" init="FIRE_RECOIL_DRIVEBY_LEFT"/>
		<string name="m_FirstPersonFireRightClipId" type="atHashString" init="FIRE_RECOIL_DRIVEBY"/>
	</structdef>

	<structdef type="CTaskSwapWeapon::Tunables" base="CTuning">
		<float name="m_OnFootClipRate" min="0.0f" max="3.0f" step="0.01f" init="1.5f"/>
		<float name="m_OnFootBlendInDuration" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
		<float name="m_LowCoverClipRate" min="0.0f" max="3.0f" step="0.01f" init="1.0f"/>
		<float name="m_LowCoverBlendInDuration" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
		<float name="m_HighCoverClipRate" min="0.0f" max="3.0f" step="0.01f" init="1.0f"/>
		<float name="m_HighCoverBlendInDuration" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
    <float name="m_ActionClipRate" min="0.0f" max="3.0f" step="0.01f" init="1.0f"/>
    <float name="m_ActionBlendInDuration" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_BlendOutDuration" min="0.0f" max="1.0f" step="0.01f" init="0.3f"/>
    <float name="m_SwimmingClipRate" min="0.0f" max="3.0f" step="0.01f" init="0.85f"/>
    <bool name="m_DebugSwapInstantly" init="false"/>
		<bool name="m_SkipHolsterWeapon" init="true"/>
	</structdef>
	
</ParserSchema>
