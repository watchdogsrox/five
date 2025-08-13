<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<enumdef type="LockType">
		<enumval name="LT_Hard"/>
		<enumval name="LT_Soft"/>
		<enumval name="LT_None"/>
	</enumdef>
	
	<structdef type="CCurve">
		<float name="m_fInputMax" min="0.0f" max="1000.0f" step="0.01f" init="0.0f" onWidgetChanged="UpdateCurveOnChange"/>
		<float name="m_fResultMax" min="0.0f" max="1000.0f" step="0.01f" init="0.0f" onWidgetChanged="UpdateCurveOnChange"/>
		<float name="m_fPow" min="0.0f" max="10.0f" step="0.01f" init="0.5f" onWidgetChanged="UpdateCurveOnChange"/>
	</structdef>

	<structdef type="CCurveSet" onPreAddWidgets="PreAddWidgets" onPostAddWidgets="PostAddWidgets" onPostLoad="Finalise">
		<string name="m_Name" type="atHashString" hideWidgets="true" ui_key="true"/>
		<array name="m_curves" type="atArray">
			<struct type="CCurve"/>
		</array>
	</structdef>

	<const name="CTargettingDifficultyInfo::CS_Max" value="4"/>

	<structdef type="CTargettingDifficultyInfo" onPostLoad="PostLoad">
		<enum name="m_LockType" type="LockType"/>
		<bool name="m_UseFirstPersonStickyAim" init="false"/>
		<bool name="m_UseLockOnTargetSwitching" init="false"/>
		<bool name="m_UseReticuleSlowDownForRunAndGun" init="false"/>
		<bool name="m_UseReticuleSlowDown" init="true"/>
		<bool name="m_EnableBulletBending" init="true"/>
		<bool name="m_AllowSoftLockFineAim" init="true"/>
		<bool name="m_UseFineAimSpring" init="false"/>
		<bool name="m_UseNewSlowDownCode" init="true"/>
		<bool name="m_UseCapsuleTests" init="true"/>
		<bool name="m_UseDriveByAssistedAim" init="false"/>
		<bool name="m_CanPauseSoftLockTimer" init="true"/>
		<float name="m_LockOnRangeModifier" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_ReticuleSlowDownRadius" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
		<float name="m_ReticuleSlowDownCapsuleRadius" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
		<float name="m_ReticuleSlowDownCapsuleLength" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
		<float name="m_DefaultTargetAngularLimit" min="0.0f" max="360.0f" step="1.0f" init="20.0f"/>
		<float name="m_DefaultTargetAngularLimitClose" min="0.0f" max="360.0f" step="1.0f" init="60.0f"/>
		<float name="m_DefaultTargetAngularLimitCloseDistMin" min="0.0f" max="60.0f" step="0.5f" init="5.0f"/>
		<float name="m_DefaultTargetAngularLimitCloseDistMax" min="0.0f" max="60.0f" step="0.5f" init="10.0f"/>
		<float name="m_WideTargetAngularLimit" min="0.0f" max="360.0f" step="1.0f" init="30.0f"/>
		<float name="m_CycleTargetAngularLimit" min="0.0f" max="360.0f" step="1.0f" init="100.0f"/>
		<float name="m_CycleTargetAngularLimitMelee" min="0.0f" max="360.0f" step="1.0f" init="190.0f"/>
		<float name="m_DefaultTargetAimPitchMin" min="-180.0f" max="180.0f" step="1.0f" init="50.0f"/>
		<float name="m_DefaultTargetAimPitchMax" min="-180.0f" max="180.0f" step="1.0f" init="30.0f"/>
		<float name="m_NoReticuleLockOnRangeModifier" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
		<float name="m_NoReticuleMaxLockOnRange" min="0.0f" max="200.0f" step="0.1f" init="20.0f"/>
		<float name="m_NoReticuleTargetAngularLimit" min="0.0f" max="360.0f" step="1.0f" init="5.0f"/>
		<float name="m_NoReticuleTargetAngularLimitClose" min="0.0f" max="360.0f" step="1.0f" init="20.0f"/>
		<float name="m_NoReticuleTargetAngularLimitCloseDistMin" min="0.0f" max="60.0f" step="0.5f" init="5.0f"/>
		<float name="m_NoReticuleTargetAngularLimitCloseDistMax" min="0.0f" max="60.0f" step="0.5f" init="10.0f"/>
		<float name="m_NoReticuleTargetAimPitchLimit" min="0.0f" max="90.0f" step="0.1f" init="5.0f"/>
		<float name="m_MinVelocityForDriveByAssistedAim" min="0.0f" max="9999.0f" step="0.1f" init="5.0f"/>
		<float name="m_LockOnDistanceRejectionModifier" min="1.0f" max="99.0f" step="1.0f" init="1.5f"/>
		<float name="m_FineAimVerticalMovement" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
		<float name="m_FineAimDownwardsVerticalMovement" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
		<float name="m_FineAimSidewaysScale" min="0.0f" max="5.0f" step="0.01f" init="2.0f"/>
		<float name="m_MinSoftLockBreakTime" min="0.0f" max="3.0f" step="0.01f" init="0.5f"/>
		<float name="m_MinSoftLockBreakTimeCloseRange" min="0.0f" max="3.0f" step="0.01f" init="0.0f"/>
		<float name="m_MinSoftLockBreakAtMaxXStickTime" min="0.0f" max="3.0f" step="0.01f" init="0.1f"/>
		<float name="m_SoftLockBreakDistanceMin" min="0.0f" max="100.0f" step="0.01f" init="1.0f"/>
		<float name="m_SoftLockBreakDistanceMax" min="0.0f" max="100.0f" step="0.01f" init="3.5f"/>
		<float name="m_MinFineAimTime" min="0.0f" max="0.5f" step="0.01f" init="0.2f"/>
		<float name="m_MinFineAimTimeHoldingStick" min="0.0f" max="1.0f" step="0.01f" init="0.35f"/>
		<float name="m_MinNoReticuleAimTime" min="0.0f" max="2.0f" step="0.01f" init="0.25f"/>
		<float name="m_AimAssistCapsuleRadius" min="0.0f" max="1.5f" step="0.01f" init="0.75f"/>
		<float name="m_AimAssistCapsuleRadiusInVehicle" min="0.0f" max="1.5f" step="0.01f" init="0.75f"/>
		<float name="m_AimAssistCapsuleMaxLength" min="0.0f" max="1000.00f" step="1.0f" init="75.0f"/>
		<float name="m_AimAssistCapsuleMaxLengthInVehicle" min="0.0f" max="1000.00f" step="1.0f" init="75.0f"/>
		<float name="m_AimAssistCapsuleMaxLengthFreeAim" min="0.0f" max="1000.00f" step="1.0f" init="75.0f"/>
		<float name="m_AimAssistCapsuleMaxLengthFreeAimSniper" min="0.0f" max="1000.00f" step="1.0f" init="75.0f"/>
		<float name="m_AimAssistBlendInTime" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_AimAssistBlendOutTime" min="0.0f" max="1.0f" step="0.01f" init="0.2f"/>
		<float name="m_SoftLockFineAimBreakXYValue" min="0.0f" max="1.0f" step="0.01f" init="0.4f"/>
		<float name="m_SoftLockFineAimBreakZValue" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
		<float name="m_SoftLockFineAimXYAbsoluteValue" min="0.0f" max="20.0f" step="0.01f" init="1.25f"/>
		<float name="m_SoftLockFineAimXYAbsoluteValueClose" min="0.0f" max="20.0f" step="0.01f" init="0.75f"/>
		<float name="m_SoftLockBreakValue" min="0.0f" max="1.0f" step="0.01f" init="0.156f"/>
		<float name="m_SoftLockTime" min="0.0f" max="5.0f" step="0.1f" init="2.25f"/>
		<float name="m_SoftLockTimeToAcquireTarget" min="0.0f" max="5.0f" step="0.01f" init="0.25f"/>
		<float name="m_SoftLockTimeToAcquireTargetInCover" min="0.0f" max="5.0f" step="0.01f" init="0.0f"/>
		<float name="m_FineAimHorSpeedMin" min="0.0f" max="10.0f" step="0.01f" init="0.1f"/>
		<float name="m_FineAimHorSpeedMax" min="0.0f" max="10.0f" step="0.01f" init="0.25f"/>
		<float name="m_FineAimVerSpeed" min="0.0f" max="10.0f" step="0.01f" init="0.1f"/>
		<float name="m_FineAimSpeedMultiplier" min="0.0f" max="100.0f" step="0.1f" init="30.0f"/>
		<float name="m_FineAimHorWeightSpeedMultiplier" min="0.0f" max="100.0f" step="0.01f" init="3.0f"/>
		<float name="m_FineAimHorSpeedPower" min="0.0f" max="10.0f" step="0.01f" init="2.0f"/>
		<float name="m_FineAimSpeedMultiplierClose" min="0.0f" max="10.0f" step="0.01f" init="0.33f"/>
		<float name="m_FineAimSpeedMultiplierCloseDistMin" min="0.0f" max="100.0f" step="0.01f" init="5.0f"/>
		<float name="m_FineAimSpeedMultiplierCloseDistMax" min="0.0f" max="100.0f" step="0.01f" init="20.0f"/>
		<float name="m_BulletBendingNearMultiplier" min="0.0f" max="10.0f" step="0.01f" init="1.0f"/>
		<float name="m_BulletBendingFarMultiplier" min="0.0f" max="100.0f" step="0.01f" init="1.0f"/>
		<float name="m_BulletBendingZoomMultiplier" min="0.0f" max="100.0f" step="0.01f" init="1.0f"/>
		<float name="m_InVehicleBulletBendingNearMinVelocity" min="0.0f" max="100.0f" step="0.01f" init="0.2f"/>
		<float name="m_InVehicleBulletBendingFarMinVelocity" min="0.0f" max="100.0f" step="0.01f" init="0.75f"/>
		<float name="m_InVehicleBulletBendingNearMaxVelocity" min="0.0f" max="100.0f" step="0.01f" init="0.75f"/>
		<float name="m_InVehicleBulletBendingFarMaxVelocity" min="0.0f" max="100.0f" step="0.01f" init="1.0f"/>
		<float name="m_InVehicleBulletBendingMaxVelocity" min="0.0f" max="100.0f" step="0.01f" init="10.0f"/>
		<u32 name="m_LockOnSwitchTimeExtensionBreakLock" min="0" max="5000" step="1" init="0"/>
		<u32 name="m_LockOnSwitchTimeExtensionKillTarget" min="0" max="5000" step="1" init="750"/>
		<float name="m_XYDistLimitFromAimVector" min="-1.0f" max="100.0f" step="0.01f" init="-1.0f"/>
		<float name="m_ZDistLimitFromAimVector" min="-1.0f" max="100.0f" step="0.01f" init="-1.0f"/>
		<array name="m_CurveSets" type="atFixedArray" size="CTargettingDifficultyInfo::CS_Max">
			<struct type="CCurveSet" addGroupWidget="false"/>
		</array>
		<struct type="CCurveSet" name="m_AimAssistDistanceCurve" />
	</structdef>

	<structdef type="CPlayerPedTargeting::Tunables"  base="CTuning">
		<float name="m_fTargetableDistance" min="0.0f" max="1.0f" step="0.1f" init="1.0f" onWidgetChanged="SetEntityTargetableDistCB"/>
		<float name="m_fTargetThreatOverride" min="0.0f" max="100.0f" step="0.1f" init="10.0f" onWidgetChanged="SetEntityTargetThreatCB"/>
		<float name="m_ArrestHardLockDistance" min="0.0f" max="20.0f" step="0.1f" init="10.0f"/>
		<float name="m_UnarmedInCoverTargetingDistance" min="0.0f" max="500.0f" step="0.1f" init="50.0f"/>
		<float name="m_CoverDirectionOffsetForInCoverTarget" min="-1.0f" max="1.0f" step="0.01f" init="0.075f"/>
		<float name="m_MeleeLostLOSBreakTime" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
		<u32 name="m_TimeToAllowCachedStickInputForMelee" min="0" max="1000" step="1" init="150"/>
		<bool name="m_DoAynchronousProbesWhenFindingFreeAimAssistTarget" init="true"/>
		<bool name="m_AllowDriverLockOnToAmbientPeds" init="false"/>
		<bool name="m_AllowDriverLockOnToAmbientPedsInSP" init="true"/>
		<bool name="m_DisplayAimAssistIntersections" init="false"/>
		<bool name="m_DisplayAimAssistTest" init="false"/>
		<bool name="m_DisplayAimAssistCurves" init="false"/>
		<bool name="m_DisplayLockOnDistRanges" init="false"/>
		<bool name="m_DisplayLockOnAngularRanges" init="false"/>
		<bool name="m_DisplaySoftLockDebug" init="false"/>
		<bool name="m_DisplayFreeAimTargetDebug" init="false"/>
		<bool name="m_DebugLockOnTargets" init="false"/>
		<bool name="m_UseRagdollTargetIfNoAssistTarget" init="true"/>
		<bool name="m_UseReticuleSlowDownStrafeClamp" init="false"/>
		<struct type="CTargettingDifficultyInfo" name="m_EasyTargettingDifficultyInfo" />
		<struct type="CTargettingDifficultyInfo" name="m_NormalTargettingDifficultyInfo" />
		<struct type="CTargettingDifficultyInfo" name="m_HardTargettingDifficultyInfo" />
		<struct type="CTargettingDifficultyInfo" name="m_ExpertTargettingDifficultyInfo" />
		<struct type="CTargettingDifficultyInfo" name="m_FirstPersonEasyTargettingDifficultyInfo" />
		<struct type="CTargettingDifficultyInfo" name="m_FirstPersonNormalTargettingDifficultyInfo" />
		<struct type="CTargettingDifficultyInfo" name="m_FirstPersonHardTargettingDifficultyInfo" />
		<struct type="CTargettingDifficultyInfo" name="m_FirstPersonExpertTargettingDifficultyInfo" />
	</structdef>

	<structdef type="CPedTargetEvaluator::Tunables"  base="CTuning">
		<float name="m_DefaultTargetAngularLimitMelee" min="0.0f" max="360.0f" step="1.0f" init="60.0f"/>
		<float name="m_DefaultTargetAngularLimitMeleeLockOnNoStick" min="0.0f" max="360.0f" step="1.0f" init="75.0f"/>
		<float name="m_DefaultTargetDistanceWeightMelee" min="0.0f" max="10.0f" step="1.0f" init="1.0f"/>
    <float name="m_DefaultTargetDistanceWeightMeleeFPS" min="0.0f" max="10.0f" step="1.0f" init="1.0f"/>
    <float name="m_DefaultTargetDistanceWeightMeleeRunning" min="0.0f" max="10.0f" step="1.0f" init="1.0f"/>
    <float name="m_DefaultTargetDistanceWeightMeleeRunningFPS" min="0.0f" max="10.0f" step="1.0f" init="1.0f"/>
    <float name="m_DefaultTargetHeadingWeightMelee" min="0.0f" max="10.0f" step="0.1f" init="0.5f"/>
    <float name="m_DefaultTargetHeadingWeightMeleeFPS" min="0.0f" max="10.0f" step="0.1f" init="0.5f"/>
    <float name="m_DefaultTargetHeadingWeightMeleeRunning" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <float name="m_DefaultTargetHeadingWeightMeleeRunningFPS" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <float name="m_DefaultTargetAngularLimitVehicleWeapon" min="0.0f" max="360.0f" step="1.0f" init="60.0f"/>
		<float name="m_MeleeLockOnStickWeighting" min="0.0f" max="2.0f" step="0.01f" init="1.0f"/>
		<float name="m_MeleeLockOnCameraWeighting" min="0.0f" max="2.0f" step="0.01f" init="0.1f"/>
		<float name="m_MeleeLockOnCameraWeightingNoStick" min="0.0f" max="2.0f" step="0.01f" init="1.0f"/>
		<float name="m_MeleeLockOnPedWeightingNoStick" min="0.0f" max="2.0f" step="0.01f" init="0.5f"/>
		<float name="m_PrioHarmless" min="0.0f" max="10.0f" step="0.1f" init="0.0f"/>
		<float name="m_PrioNeutral" min="0.0f" max="10.0f" step="0.1f" init="0.05f"/>
		<float name="m_PrioNeutralInjured" min="0.0f" max="10.0f" step="0.1f" init="0.08f"/>
		<float name="m_PrioIngangOrFriend" min="0.0f" max="10.0f" step="0.1f" init="0.1f"/>
		<float name="m_PrioPotentialThreat" min="0.0f" max="10.0f" step="0.1f" init="0.25f"/>
		<float name="m_PrioMissionThreat" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
		<float name="m_PrioMissionThreatCuffed" min="0.0f" max="10.0f" step="0.1f" init="0.5f"/>
		<float name="m_DownedThreatModifier" min="0.0f" max="1.0f" step="0.1f" init="0.75f"/>
		<float name="m_PrioPlayer2PlayerEveryone" min="0.0f" max="10.0f" step="0.1f" init="1.1f"/>
		<float name="m_PrioPlayer2PlayerStrangers" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
		<float name="m_PrioPlayer2PlayerAttackers" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
		<float name="m_PrioPlayer2Player" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
		<float name="m_PrioPlayer2PlayerCuffed" min="0.0f" max="10.0f" step="0.1f" init="1.5f"/>
		<float name="m_PrioScriptedHighPriority" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
		<float name="m_PrioMeleeDead" min="0.0f" max="10.0f" step="0.1f" init="0.05f"/>
		<float name="m_PrioMeleeCombatThreat" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
		<float name="m_PrioMeleeDownedCombatThreat" min="0.0f" max="10.0f" step="0.1f" init="0.5f"/>
		<float name="m_PrioMeleeInjured" min="0.0f" max="10.0f" step="0.1f" init="0.08f"/>
		<float name="m_PrioMeleePotentialThreat" min="0.0f" max="10.0f" step="0.1f" init="0.4f"/>
		<float name="m_InCoverScoreModifier" min="0.0f" max="2.0f" step="0.01f" init="0.75f"/>
		<float name="m_ClosestPointToLineDist" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
		<float name="m_ClosestPointToLineBonusModifier" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
		<float name="m_MeleeHeadingOverride" min="0.0f" max="360.0f" step="0.1f" init="90.0f"/>
		<float name="m_MeleeHeadingOverrideRunning" min="0.0f" max="360.0f" step="0.1f" init="75.0f"/>
		<float name="m_MeleeHeadingFalloffPowerRunning" min="0.0f" max="10.0f" step="0.1f" init="3.0f"/>
		<float name="m_DefaultMeleeRange" min="0.0f" max="50.0f" step="0.1f" init="10.0f"/>
		<float name="m_TargetDistanceWeightingMin" min="0.0f" max="2.0f" step="0.01f" init="0.25f"/>
		<float name="m_TargetDistanceWeightingMax" min="0.0f" max="2.0f" step="0.01f" init="0.5f"/>
		<float name="m_TargetHeadingWeighting" min="0.0f" max="2.0f" step="0.01f" init="1.0f"/>
		<u32 name="m_TargetDistanceMaxWeightingAimTime" min="0" max="10000" step="100" init="2000"/>
		<float name="m_TargetDistanceFallOffMin" min="0.0f" max="2.0f" step="0.01f" init="0.25f"/>
		<float name="m_TargetDistanceFallOffMax" min="0.0f" max="2.0f" step="0.01f" init="0.25f"/>
		<float name="m_RejectLockonHeadingTheshold" min="0.0f" max="1.0f" step="0.01f" init="0.05f"/>
		<float name="m_HeadingScoreForCoverLockOnRejection" min="0.0f" max="1.0f" step="0.001f" init="0.975f"/>
		<float name="m_AircraftToAircraftRejectionModifier" min="0.0f" max="10.0f" step="0.1f" init="1.33f"/>
		<u32 name="m_TimeForTakedownTargetAcquiry" min="0" max="10000" step="100" init="250"/>
		<u32 name="m_TimeToIncreaseFriendlyFirePlayer2PlayerPriority" min="0" max="120000" step="100" init="60000"/>
		<bool name="m_UseMeleeHeadingOverride" init="true"/>
		<bool name="m_LimitMeleeRangeToDefault" init="true"/>
		<bool name="m_DebugTargetting" init="false"/>
		<bool name="m_UseNonNormalisedScoringForPlayer" init="false"/>
		<bool name="m_RejectLockIfBestTargetIsInCover" init="false"/>
  </structdef>

</ParserSchema>
