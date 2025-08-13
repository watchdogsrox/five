<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CTaskVehicleFSM::Tunables" base="CTuning">
		<bool name="m_AllowEntryToMPWarpInSeats" init="false"/>
		<bool name="m_ForceStreamingFailure" init="false"/>
		<float name="m_PushAngleDotTolerance" min="-1.0f" max="1.0f" step="0.01f" init="-0.75f"/>
		<float name="m_TowardsDoorPushAngleDotTolerance" min="-1.0f" max="1.0f" step="0.01f" init="-0.75f"/>
		<float name="m_DeadZoneAnyInputDirection" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_DisallowGroundProbeVelocity" min="0.0f" max="10.0f" step="0.01f" init="1.0f"/>
		<float name="m_MinPedSpeedToActivateRagdoll" min="0.0f" max="10.0f" step="0.01f" init="5.0f"/>
		<float name="m_MinPhysSpeedToActivateRagdoll" min="0.0f" max="2.0f" step="0.01f" init="1.0f"/>
		<float name="m_MaxHoverHeightDistToWarpIntoHeli" min="0.0f" max="10.0f" step="0.01f" init="4.0f"/>
		<float name="m_MinTimeToConsiderPedGoingToDoorPriority" min="0.0f" max="20.0f" step="0.01f" init="0.5f"/>
		<float name="m_MaxTimeToConsiderPedGoingToDoorPriority" min="0.0f" max="20.0f" step="0.01f" init="7.5f"/>
		<float name="m_MaxDistToConsiderPedGoingToDoorPriority" min="0.0f" max="20.0f" step="0.01f" init="7.5f"/>
		<float name="m_BikeEntryCapsuleRadiusScale" min="0.0f" max="2.0f" step="0.01f" init="0.8f"/>
		<float name="m_VehEntryCapsuleRadiusScale" min="0.0f" max="2.0f" step="0.01f" init="0.8f"/>
		<float name="m_MinRollToDoExtraTest" min="0.0f" max="2.0f" step="0.01f" init="0.1f"/>
		<float name="m_MinPitchToDoExtraTest" min="0.0f" max="2.0f" step="0.01f" init="0.1f"/>
		<u32 name="m_TimeToConsiderEnterInputValid" min="0" max="1000" step="10" init="350"/>
	</structdef>

	<structdef type="CTaskInVehicleBasic::Tunables" base="CTuning">
		<float name="m_fSecondsInAirBeforePassengerComment" min="0.0f" max="10.0f" step="0.1f" init="1.0f" description="Time vehicle must be in air before passengers comment on it."/>
	</structdef>
	
	<structdef type="CTaskEnterVehicle::Tunables" base="CTuning">
		<bool name="m_UseCombatEntryForAiJack" init="true"/>
		<bool name="m_EnableJackRateOverride" init="false"/>
		<bool name="m_DisableDoorHandleArmIk" init="false"/>
		<bool name="m_DisableBikeHandleArmIk" init="false"/>
		<bool name="m_DisableSeatBoneArmIk" init="false"/>
		<bool name="m_DisableTagSyncIntoAlign" init="true"/>
		<bool name="m_DisableMoverFixups" init="false"/>
		<bool name="m_DisableBikePickPullUpOffsetScale" init="true"/>
		<bool name="m_EnableNewBikeEntry" init="false"/>
		<bool name="m_ForcedDoorHandleArmIk" init="false"/>
		<bool name="m_IgnoreRotationBlend" init="true"/>
		<bool name="m_EnableBikePickUpAlign" init="true"/>
		<float name="m_MPEntryPickUpPullUpRate" min="0.0f" max="5.0f" step="0.01f" init="1.1f"/>
		<float name="m_CombatEntryPickUpPullUpRate" min="0.0f" max="5.0f" step="0.01f" init="1.1f"/>
		<float name="m_BikePickUpAlignBlendDuration" min="0.0f" max="5.0f" step="0.01f" init="0.25f"/>
		<float name="m_GetInRate" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
		<float name="m_MinMagForBikeToBeOnSide" min="0.0f" max="5.0f" step="0.1f" init="0.5f"/>
		<float name="m_DistanceToEvaluateDoors" min="0.0f" max="50.0f" step="0.1f" init="15.0f"/>
		<float name="m_NetworkBlendDuration" min="0.0f" max="2.0f" step="0.05f" init="0.5f"/>
		<float name="m_NetworkBlendDurationOpenDoorCombat" min="0.0f" max="2.0f" step="0.05f" init="0.75f"/>
		<float name="m_DoorRatioToConsiderDoorOpenSteps" min="0.0f" max="1.0f" step="0.01f" init="0.35f"/>
		<float name="m_DoorRatioToConsiderDoorOpen" min="0.0f" max="1.0f" step="0.01f" init="0.75f"/>
		<float name="m_DoorRatioToConsiderDoorOpenCombat" min="0.0f" max="1.0f" step="0.01f" init="0.0"/>
		<float name="m_DoorRatioToConsiderDoorClosed" min="0.0f" max="1.0f" step="0.01f" init="0.6"/>
		<float name="m_DistToEntryToAllowForcedActionMode" min="0.0f" max="10.0f" step="0.01f" init="3.0"/>
		<float name="m_VaultDepth" min="0.0f" max="5.0f" step="0.01f" init="1.5f"/>
		<float name="m_VaultHorizClearance" min="0.0f" max="5.0f" step="0.01f" init="1.5f"/>
		<float name="m_VaultVertClearance" min="0.0f" max="5.0f" step="0.01f" init="1.5f"/>
		<float name="m_LeftPickUpTargetLerpPhaseStart" min="0.0f" max="1.0f" step="0.01f" init="0.4f"/>
		<float name="m_LeftPickUpTargetLerpPhaseEnd" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_LeftPullUpTargetLerpPhaseStart" min="0.0f" max="1.0f" step="0.01f" init="0.4f"/>
		<float name="m_LeftPullUpTargetLerpPhaseEnd" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_RightPickUpTargetLerpPhaseStart" min="0.0f" max="1.0f" step="0.01f" init="0.4f"/>
		<float name="m_RightPickUpTargetLerpPhaseEnd" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_RightPullUpTargetLerpPhaseStart" min="0.0f" max="1.0f" step="0.01f" init="0.4f"/>
		<float name="m_RightPullUpTargetLerpPhaseEnd" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_LeftPickUpTargetLerpPhaseStartBicycle" min="0.0f" max="1.0f" step="0.01f" init="0.4f"/>
		<float name="m_LeftPickUpTargetLerpPhaseEndBicycle" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_LeftPullUpTargetLerpPhaseStartBicycle" min="0.0f" max="1.0f" step="0.01f" init="0.4f"/>
		<float name="m_LeftPullUpTargetLerpPhaseEndBicycle" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_RightPickUpTargetLerpPhaseStartBicycle" min="0.0f" max="1.0f" step="0.01f" init="0.4f"/>
		<float name="m_RightPickUpTargetLerpPhaseEndBicycle" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_RightPullUpTargetLerpPhaseStartBicycle" min="0.0f" max="1.0f" step="0.01f" init="0.4f"/>
		<float name="m_RightPullUpTargetLerpPhaseEndBicycle" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_TimeBetweenPositionUpdates" min="0.0f" max="10.0f" step="0.01f" init="1.0f"/>
		<float name="m_MinSpeedToAbortOpenDoor" min="0.0f" max="30.0f" step="0.1f" init="1.0f"/>
		<float name="m_MinSpeedToAbortOpenDoorCombat" min="0.0f" max="30.0f" step="0.1f" init="5.0f"/>
		<float name="m_MinSpeedToAbortOpenDoorPlayer" min="0.0f" max="30.0f" step="0.1f" init="3.0f"/>
    <float name="m_MinSpeedToRagdollOpenDoor" min="0.0f" max="30.0f" step="0.1f" init="0.5f"/>
    <float name="m_MinSpeedToRagdollOpenDoorCombat" min="0.0f" max="30.0f" step="0.1f" init="0.5f"/>
    <float name="m_MinSpeedToRagdollOpenDoorPlayer" min="0.0f" max="30.0f" step="0.1f" init="3.0f"/>
		<float name="m_DefaultJackRate" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
		<float name="m_BikeEnterForce" min="-10.0f" max="0.0f" step="0.01f" init="-2.0f"/>
		<float name="m_BicycleEnterForce" min="-10.0f" max="0.0f" step="0.01f" init="-0.5f"/>
		<float name="m_FastEnterExitRate" min="0.0f" max="5.0f" step="0.1f" init="1.2f"/>
		<float name="m_TargetRearDoorOpenRatio" min="0.0f" max="1.0f" step="0.01f" init="0.4f"/>
		<float name="m_MaxOpenRatioForOpenDoorInitialOutside" min="0.0f" max="1.0f" step="0.01f" init="0.8f"/>
		<float name="m_MaxOpenRatioForOpenDoorOutside" min="0.0f" max="1.0f" step="0.01f" init="0.9f"/>
		<float name="m_MaxOscillationDisplacementOutside" min="0.0f" max="1.0f" step="0.001f" init="0.075f"/>
		<float name="m_MaxOpenRatioForOpenDoorInitialInside" min="0.0f" max="1.0f" step="0.01f" init="0.95f"/>
		<float name="m_MaxOpenRatioForOpenDoorInside" min="0.0f" max="1.0f" step="0.01f" init="0.9f"/>
		<float name="m_MaxOscillationDisplacementInside" min="0.0f" max="1.0f" step="0.001f" init="0.1f"/>
		<float name="m_BikeEnterLeanAngleOvershootAmt" min="0.0f" max="10.0f" step="0.001f" init="0.65f"/>
		<float name="m_BikeEnterLeanAngleOvershootRate" min="0.0f" max="10.0f" step="0.001f" init="1.0f"/>
		<float name="m_MaxDistanceToCheckEntryCollisionWhenIgnoring" min="0.0f" max="50.0f" step="0.05f" init="8.0f"/>
		<float name="m_CombatEntryBlendDuration" min="0.0f" max="2.0f" step="0.01f" init="0.15f"/>
		<float name="m_MaxDistanceToReactToJackForGoToDoor" min="0.0f" max="10.0f" step="0.01f" init="2.0f"/>
		<float name="m_MaxTimeStreamClipSetInBeforeWarpSP" min="0.0f" max="20.0f" step="0.01f" init="6.0f"/>
		<float name="m_MaxTimeStreamClipSetInBeforeWarpMP" min="0.0f" max="20.0f" step="0.01f" init="6.0f"/>
		<float name="m_MaxTimeStreamClipSetInBeforeSkippingCloseDoor" min="0.0f" max="20.0f" step="0.01f" init="1.0f"/>
		<float name="m_MaxTimeStreamShuffleClipSetInBeforeWarp" min="0.0f" max="20.0f" step="0.01f" init="1.0f"/>
		<float name="m_ClimbAlignTolerance" min="0.0f" max="2.0f" step="0.01f" init="0.35f"/>
		<float name="m_TimeBetweenDoorChecks" min="0.0f" max="1.0f" step="0.01f" init="0.75f"/>
		<bool name="m_UseSlowInOut" init="true"/>
		<float name="m_OpenDoorBlendDurationFromNormalAlign" min="0.0f" max="2.0f" step="0.01f" init="0.15f"/>
		<float name="m_OpenDoorBlendDurationFromOnVehicleAlign" min="0.0f" max="2.0f" step="0.01f" init="0.5f"/>
		<float name="m_OpenDoorToJackBlendDuration" min="0.0f" max="2.0f" step="0.01f" init="0.25f"/>
		<float name="m_GroupMemberWaitMinTime" min="0.0f" max="10.0f" step="0.01f" init="0.5f"/>
		<float name="m_GroupMemberSlowDownDistance" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
		<float name="m_GroupMemberWalkCloseDistance" min="0.0f" max="5.0f" step="0.01f" init="3.0f"/>
		<float name="m_GroupMemberWaitDistance" min="0.0f" max="5.0f" step="0.01f" init="1.5f"/>
		<float name="m_SecondsBeforeWarpToLeader" min="0.0f" max="30.0f" step="0.1f" init="10.0f"/>
		<float name="m_MinVelocityToConsiderMoving" min="0.0f" max="30.0f" step="0.1f" init="0.5f"/>
		<u32 name="m_MinTimeStationaryToIgnorePlayerDriveTest" min="0" max="6000" step="1" init="500"/>
		<u32 name="m_DurationHeldDownEnterButtonToJackFriendly" min="0" max="6000" step="1" init="1000"/>
		<string name="m_DefaultJackAlivePedFromOutsideClipId" type="atHashString" init="jack_base_perp"/>
		<string name="m_DefaultJackAlivePedFromOutsideAltClipId" type="atHashString" init="jack_base_perp_alt"/>
		<string name="m_DefaultJackDeadPedFromOutsideClipId" type="atHashString" init="jack_dead_perp"/>
		<string name="m_DefaultJackDeadPedFromOutsideAltClipId" type="atHashString" init="jack_dead_perp_alt"/>
		<string name="m_DefaultJackAlivePedFromWaterClipId" type="atHashString" init="jack_base_water_perp"/>
		<string name="m_DefaultJackDeadPedFromWaterClipId" type="atHashString" init="jack_dead_water_perp"/>
    <string name="m_DefaultJackPedFromOnVehicleClipId" type="atHashString" init="jack_on_vehicle_base_perp"/>
    <string name="m_DefaultJackDeadPedFromOnVehicleClipId" type="atHashString" init="jack_on_vehicle_dead_perp"/>
    <string name="m_DefaultJackPedOnVehicleIntoWaterClipId" type="atHashString" init="jack_on_vehicle_base_water_perp"/>
    <string name="m_DefaultJackDeadPedOnVehicleIntoWaterClipId" type="atHashString" init="jack_on_vehicle_dead_water_perp"/>
    <string name="m_DefaultClimbUpClipId" type="atHashString" init="climb_up"/>
    <string name="m_DefaultClimbUpNoDoorClipId" type="atHashString" init="climb_up_no_door"/>
    <string name="m_CustomShuffleJackDeadClipId" type="atHashString" init="shuffle_jack_dead_perp"/>
    <string name="m_CustomShuffleJackClipId" type="atHashString" init="shuffle_jack_base_perp"/>
		<string name="m_CustomShuffleJackDead_180ClipId" type="atHashString" init="shuffle_jack_dead_perp_180"/>
		<string name="m_CustomShuffleJack_180ClipId" type="atHashString" init="shuffle_jack_base_perp_180"/>
		<string name="m_ShuffleClipId" type="atHashString" init="shuffle_seat"/>
		<string name="m_Shuffle_180ClipId" type="atHashString" init="shuffle_seat_180"/>
		<string name="m_Shuffle2ClipId" type="atHashString" init="shuffle_seat2"/>
		<string name="m_Shuffle2_180ClipId" type="atHashString" init="shuffle_seat2_180"/>
    <bool name="m_OnlyJackDeadPedsInTurret" init="true"/>
    <bool name="m_ChooseTurretSeatWhenOnGround" init="false"/>
	<float name="m_DuckShuffleBlendDuration" min="0.0f" max="2.0f" step="0.01f" init="0.25f"/>
	</structdef>

	<structdef type="CTaskEnterVehicleAlign::Tunables" base="CTuning">
		<bool name="m_UseAttachDuringAlign" init="false"/>
		<bool name="m_RenderDebugToTTY" init="false"/>
		<bool name="m_ApplyRotationScaling" init="true"/>
		<bool name="m_ApplyTranslationScaling" init="true"/>
		<bool name="m_DisableRotationOvershootCheck" init="false"/>
		<bool name="m_DisableTranslationOvershootCheck" init="false"/>
		<bool name="m_ReverseLeftFootAlignAnims" init="true"/>
		<bool name="m_ForceStandEnterOnly" init="false"/>
		<float name="m_TranslationChangeRate" min="0.0f" max="10.0f" step="0.1f" init="6.0f"/>
		<float name="m_RotationChangeRate" min="0.0f" max="10.0f" step="0.1f" init="5.0f"/>
		<float name="m_DefaultAlignRate" min="0.0f" max="3.0f" step="0.1f" init="1.0f"/>
		<float name="m_FastAlignRate" min="0.0f" max="3.0f" step="0.1f" init="2.0f"/>
		<float name="m_CombatAlignRate" min="0.0f" max="3.0f" step="0.1f" init="2.5f"/>
		<float name="m_ActionCombatAlignRate" min="0.0f" max="3.0f" step="0.1f" init="1.2f"/>
		<float name="m_StandAlignMaxDist" min="0.0f" max="2.0f" step="0.1f" init="0.5f"/>
		<float name="m_AlignSuccessMaxDist" min="0.0f" max="2.0f" step="0.01f" init="0.1f"/>
		<float name="m_DefaultAlignStartFixupPhase" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_DefaultAlignEndFixupPhase" min="0.0f" max="1.0f" step="0.01f" init="0.9f"/>
		<float name="m_TargetRadiusForOrientatedAlignWalk" min="0.0f" max="5.0f" step="0.1f" init="0.75f"/>
		<float name="m_TargetRadiusForOrientatedAlignRun" min="0.0f" max="5.0f" step="0.1f" init="1.15f"/>
		<float name="m_MinRotationalSpeedScale" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
		<float name="m_MaxRotationalSpeedScale" min="0.0f" max="5.0f" step="0.01f" init="3.0f"/>
		<float name="m_MaxRotationalSpeed" min="0.0f" max="5.0f" step="0.01f" init="2.0f"/>
		<float name="m_MinTranslationalScale" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
		<float name="m_MaxTranslationalScale" min="0.0f" max="5.0f" step="0.01f" init="1.25f"/>
		<float name="m_MaxTranslationalStandSpeed" min="0.0f" max="25.0f" step="0.01f" init="0.5f"/>
		<float name="m_MaxTranslationalMoveSpeed" min="0.0f" max="25.0f" step="0.01f" init="2.0f"/>
		<float name="m_HeadingReachedTolerance" min="0.0f" max="0.25f" step="0.001f" init="0.05f"/>
		<float name="m_StdVehicleMinPhaseToStartRotFixup" min="0.0f" max="1.0f" step="0.01f" init="0.05f"/>
		<float name="m_BikeVehicleMinPhaseToStartRotFixup" min="0.0f" max="1.0f" step="0.01f" init="0.75f"/>
		<float name="m_VaultExtraZGroundTest" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
		<float name="m_MinSqdDistToSetPos" min="0.0f" max="0.1f" step="0.001f" init="0.01f"/>
		<float name="m_MinDistAwayFromEntryPointToConsiderFinished" min="0.0f" max="0.5f" step="0.001f" init="0.05f"/>
		<float name="m_MinPedFwdToEntryDotToClampInitialOrientation" min="-1.0f" max="1.0f" step="0.001f" init="0.5f"/>
		<float name="m_MinDistToAlwaysClampInitialOrientation" min="-1.0f" max="1.0f" step="0.001f" init="0.1f"/>
	</structdef>

	<structdef type="CTaskEnterVehicleSeat::Tunables" base="CTuning">
		<float name="m_MinVelocityToRagdollPed" min="0.0f" max="20.0f" step="0.1f" init="2.0f"/>
		<float name="m_MaxVelocityToEnterBike" min="0.0f" max="20.0f" step="0.1f" init="5.0f"/>
		<string name="m_DefaultGetInClipId" type="atHashString" init="get_in"/>
		<string name="m_GetOnQuickClipId" type="atHashString" init="get_on_quick"/>
		<string name="m_GetInFromWaterClipId" type="atHashString" init="get_in_from_water"/>
		<string name="m_GetInStandOnClipId" type="atHashString" init="get_in_stand_on"/>
		<string name="m_GetInCombatClipId" type="atHashString" init="get_in_combat"/>
	</structdef>

	<structdef type="CTaskCloseVehicleDoorFromInside::Tunables" base="CTuning">
		<bool name="m_EnableCloseDoorHandIk" init="true"/>
		<float name="m_DefaultCloseDoorStartPhase" min="0.0f" max="1.0f" step="0.01f" init="0.3f"/>
		<float name="m_DefaultCloseDoorEndPhase" min="0.0f" max="1.0f" step="0.01f" init="0.7f"/>
		<float name="m_DefaultCloseDoorStartIkPhase" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_DefaultCloseDoorEndIkPhase" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_MinBlendWeightToUseFarClipEvents" min="0.0f" max="1.0f" step="0.01f" init="0.35f"/>
		<float name="m_CloseDoorForceMultiplier" min="0.0f" max="10.0f" step="0.01f" init="2.5f"/>
		<float name="m_VehicleSpeedToAbortCloseDoor" min="0.0f" max="10.0f" step="0.01f" init="1.0f"/>
		<float name="m_PedTestXOffset" min="-2.0f" max="2.0f" step="0.01f" init="1.0f"/>
		<float name="m_PedTestYOffset" min="-2.0f" max="2.0f" step="0.01f" init="0.0f"/>
		<float name="m_PedTestZStartOffset" min="-2.0f" max="2.0f" step="0.01f" init="0.2f"/>
		<float name="m_PedTestZOffset" min="-2.0f" max="2.0f" step="0.01f" init="0.7f"/>
		<float name="m_PedTestRadius" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
		<float name="m_MinOpenDoorRatioToUseArmIk" min="0.0f" max="1.0f" step="0.01f" init="0.2f"/>
	</structdef>

	<structdef type="CTaskOpenVehicleDoorFromOutside::Tunables" base="CTuning">
		<bool name="m_EnableOpenDoorHandIk" init="true"/>
		<float name="m_DefaultOpenDoorStartPhase" min="0.0f" max="1.0f" step="0.01f" init="0.3f"/>
		<float name="m_DefaultOpenDoorEndPhase" min="0.0f" max="1.0f" step="0.01f" init="0.7f"/>
		<float name="m_DefaultOpenDoorStartIkPhase" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_DefaultOpenDoorEndIkPhase" min="0.0f" max="1.0f" step="0.01f" init="0.45f"/>
		<float name="m_MinBlendWeightToUseHighClipEvents" min="0.0f" max="1.0f" step="0.01f" init="0.35f"/>
		<float name="m_DefaultOpenDoorRate" min="0.0f" max="2.0f" step="0.01f" init="1.0f"/>
		<float name="m_MinHandleHeightDiffVan" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
		<float name="m_MaxHandleHeightDiffVan" min="0.0f" max="1.0f" step="0.01f" init="0.6f"/>
		<float name="m_MaxHandleHeightDiff" min="0.0f" max="1.0f" step="0.01f" init="0.35f"/>
		<string name="m_DefaultOpenDoorClipId" type="atHashString" init="d_open_out"/>
		<string name="m_HighOpenDoorClipId" type="atHashString" init="d_open_out_high"/>
		<string name="m_CombatOpenDoorClipId" type="atHashString" init="d_open_out_combat"/>
		<string name="m_DefaultTryLockedDoorClipId" type="atHashString" init="d_locked"/>
		<string name="m_DefaultForcedEntryClipId" type="atHashString" init="d_force_entry"/>
		<string name="m_WaterOpenDoorClipId" type="atHashString" init="d_open_out_water"/>
	</structdef>

	<structdef type="CTaskMotionInVehicle::Tunables" base="CTuning">
		<bool name="m_DisableCloseDoor" init="false"/>
		<float name="m_MaxVehVelocityToKeepStairsDown" min="0.0f" max="40.0f" step="0.1f" init="5.0f"/>
		<float name="m_MinSpeedForVehicleToBeConsideredStillSqr" min="0.0f" max="20.0f" step="0.01f" init="0.1f"/>
		<float name="m_VelocityDeltaThrownOut" min="0.0f" max="100.0f" step="0.1f" init="25.0f"/>
		<float name="m_VelocityDeltaThrownOutPlayerSP" min="0.0f" max="100.0f" step="0.1f" init="28.0f"/>
		<float name="m_VelocityDeltaThrownOutPlayerMP" min="0.0f" max="100.0f" step="0.1f" init="40.0f"/>
		<float name="m_MinRateForInVehicleAnims" min="0.0f" max="1.0f" step="0.01f" init="0.9f"/>
		<float name="m_MaxRateForInVehicleAnims" min="1.0f" max="2.0f" step="0.01f" init="1.1f"/>
		<float name="m_HeavyBrakeYAcceleration" min="0.0f" max="1.0f" step="0.01f" init="0.15f"/>
		<float name="m_MinRatioForClosingDoor" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_InAirZAccelTrigger" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_InAirProbeDistance" min="0.0f" max="10.0f" step="0.1f" init="1.5f"/>
		<float name="m_InAirProbeForwardOffset" min="0.0f" max="10.0f" step="0.1f" init="0.0f"/>
		<float name="m_MinPitchDefault" min="-1.57f" max="1.57f" step="0.01f" init="-0.55f"/>
		<float name="m_MaxPitchDefault" min="-1.57f" max="1.57f" step="0.01f" init="0.55f"/>
		<float name="m_MinPitchInAir" min="-1.57f" max="1.57f" step="0.01f" init="-1.57f"/>
		<float name="m_MaxPitchInAir" min="-1.57f" max="1.57f" step="0.01f" init="1.57f"/>
		<float name="m_DefaultPitchSmoothingRate" min="0.0f" max="1.0f" step="0.0001f" init="0.0375f"/>
		<float name="m_BikePitchSmoothingRate" min="0.0f" max="1.0f" step="0.0001f" init="1.0f"/>
		<float name="m_BikePitchSmoothingPassengerRate" min="0.0f" max="1.0f" step="0.0001f" init="0.15f"/>
		<float name="m_WheelieAccelerateControlThreshold" min="-1.0f" max="1.0f" step="0.01f" init="0.8f"/>
		<float name="m_WheelieMaxSpeedThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.8f"/>
		<float name="m_WheelieUpDownControlThreshold" min="-1.0f" max="1.0f" step="0.01f" init="0.8f"/>
		<float name="m_WheelieDesiredLeanAngleTol" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_StillAccTol" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_StillPitchAngleTol" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_AccelerationSmoothing" min="0.0f" max="1.0f" step="0.01f" init="0.2f"/>
		<float name="m_AccelerationSmoothingBike" min="0.0f" max="1.0f" step="0.01f" init="0.8f"/>
		<float name="m_AccelerationScaleBike" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_MinTimeInCurrentStateForStill" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
		<float name="m_AccelerationToStartLeaning" min="0.0f" max="10.0f" step="0.1f" init="3.0f"/>
		<float name="m_ZAccelerationToStartLeaning" min="0.0f" max="10.0f" step="0.1f" init="0.1f"/>
		<float name="m_MaxAccelerationForLean" min="0.0f" max="20.0f" step="0.1f" init="8.0f"/>
		<float name="m_MaxXYAccelerationForLeanBike" min="0.0f" max="20.0f" step="0.1f" init="8.0f"/>
		<float name="m_MaxZAccelerationForLeanBike" min="0.0f" max="20.0f" step="0.1f" init="8.0f"/>
		<float name="m_StillDelayTime" min="0.0f" max="3.0f" step="0.1f" init="0.1f"/>
		<float name="m_ShuntAccelerateMag" min="0.0f" max="4.0f" step="0.01f" init="2.0f"/>
		<float name="m_ShuntAccelerateMagBike" min="0.0f" max="4.0f" step="0.01f" init="1.0f"/>
		<float name="m_MinTimeInShuntStateBeforeRestart" min="0.0f" max="2.0f" step="0.01f" init="0.25f"/>
		<float name="m_MaxAbsThrottleForCloseDoor" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_MaxVehSpeedToConsiderClosingDoor" min="0.0f" max="3.0f" step="0.01f" init="0.5f"/>
		<float name="m_MaxDoorSpeedToConsiderClosingDoor" min="0.0f" max="3.0f" step="0.01f" init="0.15f"/>
		<float name="m_MinVehVelocityToGoThroughWindscreen" min="0.0f" max="100.0f" step="0.1f" init="20.0f"/>
		<float name="m_MinVehVelocityToGoThroughWindscreenMP" min="0.0f" max="100.0f" step="0.1f" init="35.0f"/>
		<float name="m_MaxZComponentForCollisionNormal" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
		<float name="m_MaxTimeStreamInVehicleClipSetBeforeStartingEngine" min="0.0f" max="5.0f" step="0.1f" init="2.0f"/>
		<u32 name="m_MinTimeSinceDriverValidToShuffle" min="0" max="5000" step="100" init="1500"/>
	</structdef>

	<structdef type="CTaskMotionOnBicycle::Tunables" base="CTuning">
		<float name="m_LeanAngleSmoothingRate" min="0.0f" max="1.0f" step="0.01f" init="0.3f"/>
		<float name="m_StillToSitPedalGearApproachRate" min="0.0f" max="3.0f" step="0.01f" init="2.0f"/>
		<float name="m_PedalGearApproachRate" min="0.0f" max="3.0f" step="0.01f" init="0.5f"/>
		<float name="m_MinXYVelForWantsToMove" min="0.0f" max="10.0f" step="0.01f" init="2.0f"/>
		<float name="m_MaxSpeedForStill" min="0.0f" max="1.0f" step="0.01f" init="0.3f"/>
		<float name="m_MaxSpeedForStillReverse" min="0.0f" max="1.0f" step="0.01f" init="0.2f"/>
		<float name="m_MaxThrottleForStill" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_DefaultPedalToFreewheelBlendDuration" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
		<float name="m_SlowPedalToFreewheelBlendDuration" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
		<float name="m_MaxRateForSlowBlendDuration" min="0.0f" max="1.0f" step="0.01f" init="0.45f"/>
		<float name="m_StillToSitLeanRate" min="0.0f" max="2.0f" step="0.01f" init="0.05f"/>
		<float name="m_StillToSitApproachRate" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
		<float name="m_UpHillMinPitchToStandUp" min="0.0f" max="5.0f" step="0.01f" init="0.15f"/>
		<float name="m_DownHillMinPitchToStandUp" min="0.0f" max="5.0f" step="0.01f" init="0.15f"/>
		<float name="m_MinTimeInStandState" min="0.0f" max="5.0f" step="0.01f" init="0.15f"/>
		<float name="m_MinTimeInStandStateFPS" min="0.0f" max="5.0f" step="0.01f" init="0.0f"/>
		<float name="m_MinSprintResultToStand" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
		<float name="m_MaxTimeSinceShiftedWeightForwardToAllowWheelie" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
		<float name="m_WheelieShiftThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.51f"/>
		<float name="m_MinPitchDefault" min="-1.57f" max="1.57f" step="0.01f" init="-0.55f"/>
		<float name="m_MaxPitchDefault" min="-1.57f" max="1.57f" step="0.01f" init="0.55f"/>
		<float name="m_MinForwardsPitchSlope" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_MaxForwardsPitchSlope" min="0.0f" max="1.0f" step="0.01f" init="0.9f"/>
		<float name="m_OnSlopeThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_MaxJumpHeightForSmallImpact" min="0.0f" max="5.0f" step="0.01f" init="1.5f"/>
		<float name="m_LongitudinalBodyLeanApproachRateSlope" min="0.0f" max="10.0f" step="0.01f" init="2.0f"/>
		<float name="m_LongitudinalBodyLeanApproachRate" min="0.0f" max="10.0f" step="0.01f" init="3.0f"/>
		<float name="m_LongitudinalBodyLeanApproachRateSlow" min="0.0f" max="10.0f" step="0.01f" init="0.75f"/>
		<float name="m_SideZoneThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.8f"/>
		<float name="m_ReturnZoneThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.6f"/>
		<float name="m_MaxYIntentionToUseSlowApproach" min="0.0f" max="1.0f" step="0.01f" init="0.3f"/>
		<float name="m_MinTimeToStayUprightAfterImpact" min="0.0f" max="5.0f" step="0.01f" init="0.25f"/>
		<float name="m_DefaultSitToStandBlendDuration" min="0.0f" max="1.0f" step="0.01f" init="0.35f"/>
		<float name="m_WheelieSitToStandBlendDuration" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
		<float name="m_WheelieStickPullBackMinIntention" min="0.0f" max="1.0f" step="0.01f" init="0.8f"/>
		<float name="m_TimeSinceNotWantingToTrackStandToAllowStillTransition" min="0.0f" max="3.0f" step="0.01f" init="0.75f"/>
		<float name="m_MinTimeInSitToStillStateToReverse" min="0.0f" max="3.0f" step="0.01f" init="0.15f"/>
		<bool name="m_PreventDirectTransitionToReverseFromSit" init="true"/>
		<string name="m_DefaultSmallImpactCharClipId" type="atHashString" init="impact_small_char"/>
		<string name="m_DefaultImpactCharClipId" type="atHashString" init="impact_char"/>
		<string name="m_DefaultSmallImpactBikeClipId" type="atHashString" init="impact_small_bike"/>
		<string name="m_DefaultImpactBikeClipId" type="atHashString" init="impact_bike"/>
		<string name="m_DownHillSmallImpactCharClipId" type="atHashString" init="impact_small_downhill_char"/>
		<string name="m_DownHillImpactCharClipId" type="atHashString" init="impact_downhill_char"/>
		<string name="m_DownHillSmallImpactBikeClipId" type="atHashString" init="impact_downhill_small_bike"/>
		<string name="m_DownHillImpactBikeClipId" type="atHashString" init="impact_downhill_bike"/>
		<string name="m_SitToStillCharClipId" type="atHashString" init="sit_to_still_char"/>
		<string name="m_SitToStillBikeClipId" type="atHashString" init="sit_to_still_bike"/>
		<string name="m_TrackStandToStillLeftCharClipId" type="atHashString" init="balance_to_still_left_char"/>
		<string name="m_TrackStandToStillLeftBikeClipId" type="atHashString" init="balance_to_still_left_bike"/>
		<string name="m_TrackStandToStillRightCharClipId" type="atHashString" init="balance_to_still_right_char"/>
		<string name="m_TrackStandToStillRightBikeClipId" type="atHashString" init="balance_to_still_right_bike"/>
	</structdef>

	<structdef type="CTaskMotionOnBicycleController::Tunables" base="CTuning">
		<float name="m_MinTimeInStateToAllowTransitionFromFixieSkid" min="0.0f" max="3.0f" step="0.01f" init="0.45f"/>
		<float name="m_TimeStillToTransitionToTrackStand" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
		<float name="m_MinTimeInPedalState" min="0.0f" max="5.0f" step="0.01f" init="0.05f"/>
		<float name="m_MinTimeInFreewheelState" min="0.0f" max="5.0f" step="0.01f" init="0.05f"/>
		<float name="m_MinAiSpeedForStandingUp" min="0.0f" max="20.0f" step="0.1f" init="10.0f"/>
		<float name="m_MaxSpeedToTriggerTrackStandTransition" min="0.0f" max="20.0f" step="0.1f" init="3.0f"/>
		<float name="m_MaxSpeedToTriggerFixieSkidTransition" min="0.0f" max="20.0f" step="0.1f" init="3.0f"/>
		<string name="m_CruiseDuckPrepLeftCharClipId" type="atHashString" init="cruise_duck_prep_char"/>
		<string name="m_CruiseDuckPrepRightCharClipId" type="atHashString" init="cruise_duck_prep_r_char"/>
		<string name="m_CruiseDuckPrepLeftBikeClipId" type="atHashString" init="cruise_duck_prep_bike"/>
		<string name="m_CruiseDuckPrepRightBikeClipId" type="atHashString" init="cruise_duck_prep_r_bike"/>
		<string name="m_FastDuckPrepLeftCharClipId" type="atHashString" init="fast_duck_prep_char"/>
		<string name="m_FastDuckPrepRightCharClipId" type="atHashString" init="fast_duck_prep_r_char"/>
		<string name="m_FastDuckPrepLeftBikeClipId" type="atHashString" init="fast_duck_prep_bike"/>
		<string name="m_FastDuckPrepRightBikeClipId" type="atHashString" init="fast_duck_prep_r_bike"/>
		<string name="m_LaunchLeftCharClipId" type="atHashString" init="launch_l_char"/>
		<string name="m_LaunchRightCharClipId" type="atHashString" init="launch_r_char"/>
		<string name="m_LaunchLeftBikeClipId" type="atHashString" init="launch_l_bike"/>
		<string name="m_LaunchRightBikeClipId" type="atHashString" init="launch_r_bike"/>
		<string name="m_TrackStandLeftCharClipId" type="atHashString" init="balance_left_char"/>
		<string name="m_TrackStandRightCharClipId" type="atHashString" init="balance_right_char"/>
		<string name="m_TrackStandLeftBikeClipId" type="atHashString" init="balance_left_bike"/>
		<string name="m_TrackStandRightBikeClipId" type="atHashString" init="balance_right_bike"/>
		<string name="m_FixieSkidLeftCharClip0Id" type="atHashString" init="fixieskid_left_still_char"/>
		<string name="m_FixieSkidLeftCharClip1Id" type="atHashString" init="fixieskid_left_fast_char"/>
		<string name="m_FixieSkidRightCharClip0Id" type="atHashString" init="fixieskid_right_still_char"/>
		<string name="m_FixieSkidRightCharClip1Id" type="atHashString" init="fixieskid_right_fast_char"/>
		<string name="m_FixieSkidLeftBikeClip0Id" type="atHashString" init="fixieskid_left_still_bike"/>
		<string name="m_FixieSkidLeftBikeClip1Id" type="atHashString" init="fixieskid_left_fast_bike"/>
		<string name="m_FixieSkidRightBikeClip0Id" type="atHashString" init="fixieskid_right_still_bike"/>
		<string name="m_FixieSkidRightBikeClip1Id" type="atHashString" init="fixieskid_right_fast_bike"/>
		<string name="m_FixieSkidToBalanceLeftCharClip1Id" type="atHashString" init="fixieskid_to_balance_left_char"/>
		<string name="m_FixieSkidToBalanceRightCharClip1Id" type="atHashString" init="fixieskid_to_balance_right_char"/>
		<string name="m_FixieSkidToBalanceLeftBikeClip1Id" type="atHashString" init="fixieskid_to_balance_left_bike"/>
		<string name="m_FixieSkidToBalanceRightBikeClip1Id" type="atHashString" init="fixieskid_to_balance_right_bike"/>
		<string name="m_CruisePedalCharClipId" type="atHashString" init="cruise_pedal_char"/>
		<string name="m_InAirFreeWheelCharClipId" type="atHashString" init="in_air_freewheel_char"/>
		<string name="m_InAirFreeWheelBikeClipId" type="atHashString" init="in_air_freewheel_bike"/>
		<string name="m_DownHillInAirFreeWheelCharClipId" type="atHashString" init="in_air_freewheel_downhill_char"/>
		<string name="m_DownHillInAirFreeWheelBikeClipId" type="atHashString" init="in_air_freewheel_downhill_bike"/>
		<string name="m_TuckFreeWheelToTrackStandRightCharClipId" type="atHashString" init="tuck_freewheel_to_balance_right_char"/>
		<string name="m_TuckFreeWheelToTrackStandRightBikeClipId" type="atHashString" init="tuck_freewheel_to_balance_right_bike"/>
	</structdef>

	<structdef type="CBikeLeanAngleHelper::Tunables" base="CTuning">
		<bool name="m_UseReturnOvershoot" init="true"/>
		<bool name="m_UseInitialLeanForcing" init="true"/>
		<float name="m_DesiredLeanAngleTolToBringLegIn" min="0.0f" max="1.0f" step="0.01f" init="0.01f"/>
		<float name="m_DesiredSpeedToBringLegIn" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
		<float name="m_DesiredLeanAngleRate" min="0.0f" max="5.0f" step="0.01f" init="0.25f"/>
		<float name="m_DesiredLeanAngleRateQuad" min="0.0f" max="5.0f" step="0.01f" init="0.075f"/>
		<float name="m_LeanAngleReturnRate" min="0.0f" max="5.0f" step="0.01f" init="0.05f"/>
		<float name="m_LeanAngleDefaultRate" min="0.0f" max="5.0f" step="0.01f" init="0.15f"/>
		<float name="m_LeanAngleDefaultRatePassenger" min="0.0f" max="5.0f" step="0.01f" init="0.05f"/>
		<float name="m_DesiredOvershootLeanAngle" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_LeanAngleReturnedTol" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_HasStickInputThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_LeaningExtremeThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
	</structdef>

	<structdef type="CTaskMotionInAutomobile::Tunables" base="CTuning">
		<bool name="m_TestLowLodIdle" init="false"/>
		<float name="m_DefaultShuntToIdleBlendDuration" min="0.0f" max="2.0f" step="0.1f" init="0.15f"/>
		<float name="m_PanicShuntToIdleBlendDuration" min="0.0f" max="2.0f" step="0.1f" init="0.25f"/>
		<float name="m_MinTimeInHornState" min="0.0f" max="2.0f" step="0.1f" init="0.5f"/>
		<float name="m_MaxVelocityForSitIdles" min="0.0f" max="20.0f" step="0.1f" init="10.0f"/>
		<float name="m_MaxSteeringAngleForSitIdles" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<u32 name="m_MinCentredSteeringAngleTimeForSitIdles" min="0" max="2000" step="10" init="50"/>
		<float name="m_LeanSidewaysAngleSmoothingRateMin" min="0.0f" max="1.0f" step="0.01f" init="0.3f"/>
		<float name="m_LeanSidewaysAngleSmoothingRateMax" min="0.0f" max="1.0f" step="0.01f" init="0.6f"/>
		<float name="m_LeanSidewaysAngleSmoothingAcc" min="0.0f" max="1.0f" step="0.01f" init="0.6f"/>
		<float name="m_LeanSidewaysAngleMinAccAngle" min="0.0f" max="1.0f" step="0.01f" init="0.6f"/>
		<float name="m_LeanSidewaysAngleMaxAccAngle" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_LeftRightStickInputSmoothingRate" min="0.0f" max="1.0f" step="0.01f" init="0.3f"/>
		<float name="m_LeftRightStickInputMin" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_LeanForwardsAngleSmoothingRate" min="0.0f" max="1.0f" step="0.01f" init="0.3f"/>
		<float name="m_UpDownStickInputSmoothingRate" min="0.0f" max="1.0f" step="0.01f" init="0.3f"/>
		<float name="m_UpDownStickInputMin" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_ZAccForLowImpact" min="-25.0f" max="25.0f" step="0.01f" init="0.1f"/>
		<float name="m_ZAccForMedImpact" min="-25.0f" max="25.0f" step="0.01f" init="15.0f"/>
		<float name="m_ZAccForHighImpact" min="-25.0f" max="25.0f" step="0.01f" init="-8.0f"/>
		<bool name="m_UseLegIkOnBikes" init="false"/>
		<float name="m_LargeVerticalAccelerationDelta" min="0.0f" max="1.0f" step="0.01f" init="0.3f"/>
		<s32 name="m_NumFramesToPersistLargeVerticalAcceleration" min="0" max="30" step="1" init="3"/>
		<string name="m_LowLodIdleClipSetId" type="atHashString" />
		<float name="m_SeatDisplacementSmoothingRateDriver" min="0.0f" max="1.0f" step="0.001f" init="0.3f"/>
		<float name="m_SeatDisplacementSmoothingRatePassenger" min="0.0f" max="1.0f" step="0.001f" init="0.15f"/>
		<float name="m_SeatDisplacementSmoothingStandUpOnJetski" min="0.0f" max="10.0f" step="0.001f" init="2.5f"/>
		<float name="m_StartEngineForce" min="-10.0f" max="0.0f" step="0.01f" init="-1.0f"/>

		<float name="m_MinForwardsPitchSlope" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_MaxForwardsPitchSlope" min="0.0f" max="1.0f" step="0.01f" init="0.9f"/>
		<float name="m_MinForwardsPitchSlopeBalance" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_MaxForwardsPitchSlopeBalance" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_TimeInWheelieToEnforceMinPitch" min="0.0f" max="3.0f" step="0.01f" init="0.5f"/>
		<float name="m_MinForwardsPitchWheelieBalance" min="0.0f" max="1.0f" step="0.01f" init="0.45f"/>
		<float name="m_MaxForwardsPitchWheelieBalance" min="0.0f" max="1.0f" step="0.01f" init="0.65f"/>
		<float name="m_MinForwardsPitchWheelieBegin" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_SlowFastSpeedThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.45f"/>
		<float name="m_MinForwardsPitchSlowSpeed" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_MaxForwardsPitchSlowSpeed" min="0.0f" max="1.0f" step="0.01f" init="0.65f"/>
		<float name="m_MinForwardsPitchFastSpeed" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_MaxForwardsPitchFastSpeed" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_SlowApproachRate" min="0.0f" max="3.0f" step="0.01f" init="0.75f"/>
		<float name="m_FastApproachRate" min="0.0f" max="3.0f" step="0.01f" init="1.0f"/>
		<float name="m_WheelieApproachRate" min="0.0f" max="3.0f" step="0.01f" init="0.5f"/>
		<float name="m_NewLeanSteerApproachRate" min="0.0f" max="10.0f" step="0.01f" init="0.75f"/>
		<float name="m_MinTimeBetweenCloseDoorAttempts" min="0.0f" max="10.0f" step="0.01f" init="1.0f"/>
		<float name="m_ShuntDamageMultiplierAI" min="0.0f" max="100.0f" step="0.01f" init="20.0f"/>
		<float name="m_ShuntDamageMultiplierPlayer" min="0.0f" max="100.0f" step="0.01f" init="3.0f"/>
		<float name="m_MinDamageTakenToApplyDamageAI" min="0.0f" max="100.0f" step="0.1f" init="30.0f"/>
		<float name="m_MinDamageTakenToApplyDamagePlayer" min="0.0f" max="100.0f" step="0.1f" init="30.0f"/>
		<float name="m_MinTimeInTaskToCheckForDamage" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
    	<float name="m_MinDamageToCheckForRandomDeath" min="0.0f" max="100.0f" step="1.0f" init="18.0f"/>
   		<float name="m_MaxDamageToCheckForRandomDeath" min="0.0f" max="100.0f" step="1.0f" init="30.0f"/>
		<float name="m_MinHeavyCrashDeathChance" min="0.0f" max="100.0f" step="1.0f" init="35.0f"/>
		<float name="m_MaxHeavyCrashDeathChance" min="0.0f" max="100.0f" step="1.0f" init="60.0f"/>
    	<u32 name="m_SteeringDeadZoneCentreTimeMS" min="0" max="1000" step="1" init="100"/>
    	<u32 name="m_SteeringDeadZoneTimeMS" min="0" max="1000" step="1" init="150"/>
   		<float name="m_SteeringDeadZone" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_SteeringChangeToStartProcessMoveSignals" min="0.0f" max="1.0f" step="0.001f" init="0.004f" description="When timeslicing, how much does the steering have to change before we start to update the MoVE signal every frame."/>
		<float name="m_SteeringChangeToStopProcessMoveSignals" min="0.0f" max="1.0f" step="0.001f" init="0.0001f" description="When timeslicing, how much does the steering have to change before we stop updating the MoVE signal every frame."/>
		<float name="m_SeatBlendLinSpeed" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
		<float name="m_SeatBlendAngSpeed" min="0.0f" max="5.0f" step="0.001f" init="2.0f"/>
		<float name="m_HoldLegOutVelocity" min="0.0f" max="10.0f" step="0.01f" init="4.0f"/>
		<float name="m_MinVelStillStart" min="0.0f" max="5.0f" step="0.01f" init="0.3f"/>
		<float name="m_MinVelStillStop" min="0.0f" max="5.0f" step="0.01f" init="3.0f"/>
		<float name="m_ForcedLegUpVelocity" min="0.0f" max="10.0f" step="0.01f" init="5.0f"/>
		<float name="m_BurnOutBlendInTol" min="0.0f" max="5.0f" step="0.01f" init="0.2f"/>
		<float name="m_BurnOutBlendInSpeed" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
		<float name="m_BurnOutBlendOutSpeed" min="0.0f" max="5.0f" step="0.01f" init="2.0f"/>
		<float name="m_BikeInAirDriveToStandUpTimeMin" min="0.0f" max="5.0f" step="0.01f" init="0.15f"/>
		<float name="m_BikeInAirDriveToStandUpTimeMax" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
		<float name="m_MinSpeedToBlendInDriveFastFacial" min="0.0f" max="100.0f" step="0.1f" init="20.0f"/>
		<float name="m_MinDisplacementScale" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_DisplacementScaleApproachRateIn" min="0.0f" max="15.0f" step="0.01f" init="3.0f"/>
		<float name="m_DisplacementScaleApproachRateOut" min="0.0f" max="15.0f" step="0.01f" init="2.0f"/>
		<float name="m_SteeringWheelBlendInApproachSpeed" min="0.0f" max="50.0f" step="0.01f" init="5.0f"/>
		<float name="m_SteeringWheelBlendOutApproachSpeed" min="0.0f" max="50.0f" step="0.01f" init="5.0f"/>
		<float name="m_FirstPersonSteeringWheelAngleApproachSpeed" min="0.0f" max="50.0f" step="0.01f" init="2.2f"/>
    <float name="m_FirstPersonSteeringTangentT0" min="-10.0f" max="10.0f" step="0.001f" init="0.0f"/>
    <float name="m_FirstPersonSteeringTangentT1" min="-10.0f" max="10.0f" step="0.001f" init="1.0f"/>
    <float name="m_FirstPersonSteeringSplineStart" min="0.1f" max="1.0f" step="0.01f" init="0.7f"/>
    <float name="m_FirstPersonSteeringSplineEnd" min="0.1f" max="1.0f" step="0.01f" init="0.95f"/>
    <float name="m_FirstPersonSteeringSafeZone" min="0.0f" max="0.5f" step="0.01f" init="0.150f"/>
    <float name="m_FirstPersonSmoothRateToFastSteering" min="0.0f" max="10.0f" step="0.01f" init="0.05f"/>
    <float name="m_FirstPersonSmoothRateToSlowSteering" min="0.0f" max="10.0f" step="0.01f" init="0.7f"/>
    <float name="m_FirstPersonMaxSteeringRate" min="0.0f" max="20.0f" step="0.1f" init="7.0f"/>
    <float name="m_FirstPersonMinSteeringRate" min="0.0f" max="20.0f" step="0.1f" init="1.5f"/>
    <float name="m_FirstPersonMinVelocityToModifyRate" min="0.0f" max="100.0f" step="0.1f" init="7.5f"/>
    <float name="m_FirstPersonMaxVelocityToModifyRate" min="0.0f" max="100.0f" step="0.1f" init="20.0f"/>
    <u32 name="m_FirstPersonTimeToAllowFastSteering" min="0" max="10000" step="100" init="150"/>
    <u32 name="m_FirstPersonTimeToAllowSlowSteering" min="0" max="10000" step="100" init="225"/>
    <u32 name="m_MinTimeSinceDriveByForAgitate" min="0" max="30000" step="100" init="10000"/>
    <u32 name="m_MinTimeSinceDriveByForCloseDoor" min="0" max="30000" step="100" init="1500"/>
		<string name="m_StartEngineClipId" type="atHashString" init="start_engine"/>
		<string name="m_FirstPersonStartEngineClipId" type="atHashString" init="pov_start_engine"/>
    <string name="m_HotwireClipId" type="atHashString" init="hotwire"/>
    <string name="m_FirstPersonHotwireClipId" type="atHashString" init="pov_hotwire"/>
    <string name="m_PutOnHelmetClipId" type="atHashString" init="put_on_helmet"/>
    <string name="m_PutOnHelmetFPSClipId" type="atHashString" init="put_on_helmet"/>
    <string name="m_PutHelmetVisorUpClipId" type="atHashString" init="VISOR_UP"/>
    <string name="m_PutHelmetVisorDownClipId" type="atHashString" init="VISOR_DOWN"/>
    <string name="m_PutHelmetVisorUpPOVClipId" type="atHashString" init="POV_VISOR_UP"/>
    <string name="m_PutHelmetVisorDownPOVClipId" type="atHashString" init="POV_VISOR_DOWN"/>
		<string name="m_ChangeStationClipId" type="atHashString" init="change_station"/>
		<string name="m_StillToSitClipId" type="atHashString" init="still_to_sit"/>
		<string name="m_SitToStillClipId" type="atHashString" init="sit_to_still"/>
		<string name="m_BurnOutClipId" type="atHashString" init="burnout"/>
    <string name="m_BikeHornIntroClipId" type="atHashString" init="horn_intro"/>
    <string name="m_BikeHornClipId" type="atHashString" init="horn"/>
    <string name="m_BikeHornOutroClipId" type="atHashString" init="horn_outro"/>
		<float name="m_RestartIdleBlendDuration" min="0.0f" max="1.0f" step="0.01f" init="0.125f"/>
		<float name="m_RestartIdleBlendDurationpassenger" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
		<float name="m_DriveByToDuckBlendDuration" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_MinTimeDucked" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
		<float name="m_DuckControlThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_DuckBikeSteerThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.2f"/>
		<float name="m_MinTimeToNextFirstPersonAdditiveIdle" min="0.0f" max="30.0f" step="0.01f" init="5.0f"/>
		<float name="m_MaxTimeToNextFirstPersonAdditiveIdle" min="0.0f" max="30.0f" step="0.01f" init="10.0f"/>
		<float name="m_FirstPersonAdditiveIdleSteeringCentreThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.2f"/>
    <float name="m_MaxDuckBreakLockOnTime" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
    <float name="m_SitToStillForceTriggerVelocity" min="0.0f" max="10.0f" step="0.01f" init="0.5f"/>
    <float name="m_MinVelocityToAllowAltClipSet" min="0.0f" max="50.0f" step="0.01f" init="4.0f"/>
    <float name="m_MaxVelocityToAllowAltClipSet" min="0.0f" max="50.0f" step="0.01f" init="25.0f"/>
    <float name="m_MaxVelocityToAllowAltClipSetWheelie" min="0.0f" max="50.0f" step="0.01f" init="30.0f"/>
    <float name="m_LeanThresholdToAllowAltClipSet" min="0.0f" max="0.5f" step="0.01f" init="0.2f"/>
    <float name="m_AltLowRiderBlendDuration" min="0.0f" max="5.0f" step="0.01f" init="0.25f"/>
    <float name="m_MaxThrottleForAltLowRiderPose" min="0.0f" max="1.0f" step="0.01f" init="0.9f"/>
    <float name="m_MaxPitchForBikeAltAnims" min="0.0f" max="2.0f" step="0.01f" init="0.6f"/>
    <float name="m_MinTimeInLowRiderClipset" min="0.0f" max="2.0f" step="0.01f" init="1.0f"/>
    <float name="m_TimeSinceLastDamageToAllowAltLowRiderPoseLowRiderPose" min="0.0f" max="100.0f" step="0.1f" init="10.0f"/>
  </structdef>

	<structdef type="CTaskExitVehicle::Tunables" base="CTuning">
		<float name="m_TimeSinceLastSpottedToLeaveEngineOn" min="0.0f" max="200.0f" step="0.1f" init="3.0f"/>
		<float name="m_BeJackedBlendInDuration" min="0.0f" max="1.0f" step="0.01f" init="0.35f"/>
		<float name="m_ExitVehicleBlendInDuration" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_ThroughWindScreenBlendInDuration" min="0.0f" max="1.0f" step="0.01f" init="0.125f"/>
		<float name="m_ExitVehicleBlendOutDuration" min="0.0f" max="1.0f" step="0.01f" init="0.35f"/>
		<float name="m_ExitVehicleUnderWaterBlendOutDuration" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
    <float name="m_ExitVehicleAttempToFireBlendOutDuration" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
    <float name="m_FleeExitVehicleBlendOutDuration" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
		<float name="m_LeaderExitVehicleDistance" min="0.0f" max="5.0f" step="0.1f" init="1.5f"/>
		<float name="m_ExitProbeDistance" min="0.0f" max="15.0f" step="0.01f" init="5.0f"/>
		<float name="m_ExitDistance" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
		<float name="m_RearExitSideOffset" min="0.0f" max="5.0f" step="0.01f" init="0.25f"/>
		<float name="m_MinVelocityToRagdollPed" min="0.0f" max="25.0f" step="0.01f" init="1.5f"/>
		<float name="m_MaxTimeToReserveComponentBeforeWarp" min="1.0f" max="10.0f" step="0.01f" init="3.0f"/>
		<float name="m_MaxTimeToReserveComponentBeforeWarpLonger" min="1.0f" max="10.0f" step="0.01f" init="5.0f"/>
		<float name="m_ExtraOffsetForGroundCheck" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
		<u32 name="m_JumpOutofSubNeutralBuoyancyTime" min="0" max="10000" step="100" init="2500"/>
		<string name="m_DefaultClimbDownClipId" type="atHashString" init="climb_down"/>
		<string name="m_DefaultClimbDownWaterClipId" type="atHashString" init="climb_down_water"/>
		<string name="m_DefaultClimbDownNoDoorClipId" type="atHashString" init="climb_down_no_door"/>
		<string name="m_DefaultClimbDownWaterNoDoorClipId" type="atHashString" init="climb_down_no_door_water"/>
	</structdef>

  <enumdef type="CTaskExitVehicleSeat::eSeatPosition">
    <enumval name="SF_FrontDriverSide"/>
    <enumval name="SF_FrontPassengerSide"/>
    <enumval name="SF_BackDriverSide"/>
    <enumval name="SF_BackPassengerSide"/>
    <enumval name="SF_AltFrontDriverSide"/>
    <enumval name="SF_AltFrontPassengerSide"/>
    <enumval name="SF_AltBackDriverSide"/>
    <enumval name="SF_AltBackPassengerSide"/>
  </enumdef>
  
  <structdef type="CTaskExitVehicleSeat::ExitToAimSeatInfo">
    <string name="m_ExitToAimClipsName" type="atHashString" />
    <string name="m_OneHandedClipSetName" type="atHashString" />
    <string name="m_TwoHandedClipSetName" type="atHashString" />
    <enum name="m_SeatPosition" type="CTaskExitVehicleSeat::eSeatPosition"/> 
  </structdef>

  <structdef type="CTaskExitVehicleSeat::ExitToAimClipSet">
    <string name="m_Name" type="atHashString" />
    <array name="m_Clips" type="atArray">
      <string type="atHashString"/>
    </array>    
  </structdef>

  <structdef type="CTaskExitVehicleSeat::ExitToAimVehicleInfo">
    <string name="m_Name" type="atHashString" />
    <array name="m_Seats" type="atArray">
      <struct type="CTaskExitVehicleSeat::ExitToAimSeatInfo"/>
    </array>
  </structdef>
  
	<structdef type="CTaskExitVehicleSeat::Tunables" base="CTuning">
		<float name="m_MinTimeInStateToAllowRagdollFleeExit" min="0.0f" max="2.0f" step="0.01f" init="0.8f"/>
		<float name="m_AdditionalWindscreenRagdollForceFwd" min="0.0f" max="10.0f" step="0.01f" init="0.25f"/>
		<float name="m_AdditionalWindscreenRagdollForceUp" min="0.0f" max="30.0f" step="0.1f" init="3.0f"/>
		<float name="m_SkyDiveProbeDistance" min="0.0f" max="30.0f" step="0.1f" init="15.0f"/>
		<float name="m_InAirProbeDistance" min="0.0f" max="15.0f" step="0.1f" init="2.0f"/>
		<float name="m_ArrestProbeDistance" min="0.0f" max="15.0f" step="0.05f" init="1.25"/>
    <float name="m_InWaterExitDepth" min="0.0f" max="15.0f" step="0.1f" init="1.25f"/>
    <float name="m_InWaterExitProbeLength" min="0.0f" max="15.0f" step="0.1f" init="2.5f"/>
    <float name="m_BikeVelocityToUseAnimatedJumpOff" min="0.0f" max="30.0f" step="0.1f" init="15.0f"/>
		<float name="m_BicycleVelocityToUseAnimatedJumpOff" min="0.0f" max="30.0f" step="0.1f" init="4.0f"/>
		<float name="m_DefaultGetOutBlendDuration" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_DefaultGetOutNoWindBlendDuration" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
		<float name="m_MaxTimeForArrestBreakout" min="0.0f" max="10.0f" step="0.01f" init="3.0f"/>
		<float name="m_ThroughWindscreenDamagePlayer" min="0.0f" max="400.0f" step="0.01f" init="40.0f"/>
		<float name="m_ThroughWindscreenDamageAi" min="0.0f" max="400.0f" step="0.01f" init="200.0f"/>
		<string name="m_DefaultCrashExitOnSideClipId" type="atHashString" init="crash_exit_on_side"/>
		<string name="m_DefaultBeJackedAlivePedFromOutsideClipId" type="atHashString" init="jack_base_victim"/>
		<string name="m_DefaultBeJackedAlivePedFromOutsideAltClipId" type="atHashString" init="jack_base_victim_alt"/>
		<string name="m_DefaultBeJackedDeadPedFromOutsideClipId" type="atHashString" init="jack_dead_victim"/>
		<string name="m_DefaultBeJackedDeadPedFromOutsideAltClipId" type="atHashString" init="jack_dead_victim_alt"/>
		<string name="m_DefaultBeJackedAlivePedFromWaterClipId" type="atHashString" init="jack_base_water_victim"/>
		<string name="m_DefaultBeJackedDeadPedFromWaterClipId" type="atHashString" init="jack_dead_water_victim"/>
    <string name="m_DefaultBeJackedAlivePedOnVehicleClipId" type="atHashString" init="jack_on_vehicle_base_victim"/>
    <string name="m_DefaultBeJackedDeadPedOnVehicleClipId" type="atHashString" init="jack_on_vehicle_dead_victim"/>
    <string name="m_DefaultBeJackedAlivePedOnVehicleIntoWaterClipId" type="atHashString" init="jack_on_vehicle_base_water_victim"/>
    <string name="m_DefaultBeJackedDeadPedOnVehicleIntoWaterClipId" type="atHashString" init="jack_on_vehicle_dead_water_victim"/>
    <string name="m_DefaultFleeExitClipId" type="atHashString" init="get_out_to_run"/>
		<string name="m_DefaultGetOutClipId" type="atHashString" init="get_out"/>
		<string name="m_DefaultGetOutToWaterClipId" type="atHashString" init="get_out_into_water"/>
    <string name="m_DefaultGetOutOnToVehicleClipId" type="atHashString" init="get_out_to_stand"/>
    <string name="m_DefaultGetOutNoWingId" type="atHashString" init="get_out_no_wing"/>
		<string name="m_DefaultJumpOutClipId" type="atHashString" init="jump_out"/>
		<string name="m_DeadFallOutClipId" type="atHashString" init="dead_fall_out"/>
		<array name="m_ExitToAimClipSets" type="atArray">
      <struct type="CTaskExitVehicleSeat::ExitToAimClipSet"/>
    </array>
    <array name="m_ExitToAimVehicleInfos" type="atArray">
      <struct type="CTaskExitVehicleSeat::ExitToAimVehicleInfo"/>
    </array>
		<float name="m_BikeExitForce" min="-10.0f" max="0.0f" step="0.01f" init="-2.0f"/>
    <float name="m_RagdollIntoWaterVelocity" min="0.0f" max="100.0f" step="0.1f" init="7.5f"/>
    <float name="m_GroundFixupHeight" min="0.0f" max="100.0f" step="0.1f" init="2.5f"/>
    <float name="m_GroundFixupHeightLarge" min="0.0f" max="100.0f" step="0.1f" init="5.0f"/>
    <float name="m_GroundFixupHeightLargeOffset" min="0.0f" max="100.0f" step="0.1f" init="2.0f"/>
    <float name="m_GroundFixupHeightBoatInWaterInitial" min="0.0f" max="100.0f" step="0.1f" init="1.25f"/>
    <float name="m_GroundFixupHeightBoatInWater" min="0.0f" max="100.0f" step="0.1f" init="2.0f"/>
    <float name="m_ExtraWaterZGroundFixup" min="0.0f" max="100.0f" step="0.1f" init="0.35f"/>
    <float name="m_FleeExitExtraRotationSpeed" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
		<float name="m_FleeExitExtraTranslationSpeed" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
		<string name="m_CustomShuffleBeJackedDeadClipId" type="atHashString" init="shuffle_jack_dead_victim"/>
		<string name="m_CustomShuffleBeJackedClipId" type="atHashString" init="shuffle_jack_base_victim"/>
	</structdef>

	<structdef type="CTaskTryToGrabVehicleDoor::Tunables" base="CTuning">
		<u32 name="m_MinGrabTime" min="0" max="65535" step="1" init="1000"/>
		<u32 name="m_MaxGrabTime" min="0" max="65535" step="1" init="3000"/>
    <float name="m_MaxHandToHandleDistance" min="0.0f" max="30.0f" step="0.1f" init="0.35f"/>
  </structdef>

	<structdef type="CVehicleClipRequestHelper::Tunables" base="CTuning">
		<float name="m_MinDistanceToScanForNearbyVehicle" min="0.0f" max="30.0f" step="0.1f" init="10.0f"/>
		<float name="m_MaxDistanceToScanForNearbyVehicle" min="0.0f" max="30.0f" step="0.1f" init="25.0f"/>
		<float name="m_MinDistUpdateFrequency" min="0.0f" max="10.0f" step="0.01f" init="1.0f"/>
		<float name="m_MaxDistUpdateFrequency" min="0.0f" max="10.0f" step="0.01f" init="0.1f"/>
		<float name="m_MinDistPercentageToScaleScanArc" min="0.0f" max="1.0f" step="0.1f" init="0.5f"/>
		<float name="m_MinDistScanArc" min="0.0f" max="3.14f" step="0.01f" init="3.14f"/>
		<float name="m_MaxDistScanArc" min="0.0f" max="3.14f" step="0.01f" init="1.57f"/>
		<bool name="m_DisableVehicleDependencies" init="false"/>
		<bool name="m_DisableStreamedVehicleAnimRequestHelper" init="false"/>
		<bool name="m_EnableStreamedEntryAnims" init="true"/>
		<bool name="m_EnableStreamedInVehicleAnims" init="true"/>
		<bool name="m_EnableStreamedEntryVariationAnims" init="true"/>
		<bool name="m_StreamConnectedSeatAnims" init="false"/>
		<bool name="m_StreamInVehicleAndEntryAnimsTogether" init="false"/>
		<bool name="m_StreamEntryAndInVehicleAnimsTogether" init="false"/>
	</structdef>

	<structdef type="CPrioritizedClipSetRequestManager::Tunables" base="CTuning">
		<bool name="m_RenderDebugDraw" init="false"/>
		<Vector2 name="m_vScroll" init="10.750f, 22.0f" min="-100.0f" max="100.0f" step="0.25f"/>
		<float name="m_fIndent" min="0.0f" max="50.0f" step="0.1f" init="2.0f"/>
		<s32 name="m_MaxNumRequestsPerContext" min="0" max="1000"  init="500"/>
	</structdef>

	<structdef type="CTaskRideTrain::Tunables" base="CTuning">
		<float name="m_MinDelayForGetOff" min="0.0f" max="60.0f" step="0.01f" init="0.0f"/>
		<float name="m_MaxDelayForGetOff" min="0.0f" max="60.0f" step="0.01f" init="2.0f"/>
		<float name="m_fMaxWaitSeconds"   min="0.0f" max="60.0f" step="0.1f"  init="10.0f"/>
	</structdef>

	<structdef type="CTaskTrainBase::Tunables" base="CTuning">
		<float name="m_TargetRadius" min="0.0f" max="3.0f" step="0.01f" init="0.25f"/>
		<float name="m_CompletionRadius" min="0.0f" max="3.0f" step="0.01f" init="0.25f"/>
		<float name="m_SlowDownDistance" min="0.0f" max="3.0f" step="0.01f" init="1.0f"/>
	</structdef>
	
	<structdef type="CTaskPlayerDrive::Tunables" base="CTuning">
		<u32 name="m_StealthNoisePeriodMS" min="0" max="10000" step="100" init="500" description="Time between stealth noise blips, in ms."/>
		<float name="m_StealthSpeedThresholdLow" min="0.0f" max="100.0f" step="1.0f" init="2.0f" description="Threshold for what's considered low speed for stealth noise purposes, in m/s."/>
		<float name="m_StealthSpeedThresholdHigh" min="0.0f" max="100.0f" step="1.0f" init="10.0f" description="Threshold for what's considered high speed for stealth noise purposes, in m/s."/>
		<float name="m_StealthVehicleTypeFactorBicycles" min="0.0f" max="5.0f" step="0.05f" init="0.3f" description="Noise range scaling factor when riding a bicycle."/>
		<float name="m_MinPlayerJumpOutSpeedBike" min="0.0f" max="25.0f" step="0.1f" init="12.0f" description="Minimum speed player can jump off a bike."/>
		<float name="m_MinPlayerJumpOutSpeedCar" min="0.0f" max="25.0f" step="0.1f" init="10.0f" description="Minimum speed player can jump off a bike."/>
		<float name="m_TimeBetweenAddingDangerousVehicleEvents" min="0.0f" max="10.0f" step ="0.1f" init="3.0f" description="Time between adding dangerous vehicle shocking events."/>
	</structdef>

	<structdef type="CTaskReactToBeingAskedToLeaveVehicle::Tunables" base="CTuning">
		<float name="m_MaxTimeToWatchVehicle" min="0.0f" max="100.0f" step="1.0f" init="10.0f"/>
		<float name="m_MaxDistanceToWatchVehicle" min="0.0f" max="100.0f" step="1.0f" init="20.f"/>
	</structdef>

	<structdef type="CTaskCarReactToVehicleCollision::Tunables::SlowDown">
		<float name="m_MinTimeToReact" min="0.0f" max="5.0f" step="0.1f" init="0.5f"/>
		<float name="m_MaxTimeToReact" min="0.0f" max="5.0f" step="0.1f" init="1.5f"/>
		<float name="m_MaxCruiseSpeed" min="0.0f" max="100.0f" step="1.0f" init="3.0f"/>
		<float name="m_ChancesToHonk" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
		<float name="m_ChancesToHonkHeldDown" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
		<float name="m_ChancesToFlipOff" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
		<float name="m_MinTime" min="0.0f" max="10.0f" step="0.1f" init="3.5f"/>
		<float name="m_MaxTime" min="0.0f" max="10.0f" step="0.1f" init="5.5f"/>
	</structdef>
	
	<structdef type="CTaskCarReactToVehicleCollision::Tunables" base="CTuning">
		<struct name="m_SlowDown" type="CTaskCarReactToVehicleCollision::Tunables::SlowDown"/>
		<float name="m_MaxDamageToIgnore" min="-1.0f" max="25.0f" step="1.0f" init="5.0f"/>
	</structdef>

	<structdef type="CTaskMountFollowNearestRoad::Tunables" base="CTuning" cond="ENABLE_HORSE">
		<float name="m_fCutoffDistForNodeSearch" min="0.0f" max="100000.0f" step="10.0f" init="250.0f" description="Cutoff distance used to determine the closest path"/>
		<float name="m_fMinDistanceBetweenNodes" min="0.0f" max="100.0f" step="1.0f" init="5.0f" description="How close together must the nodes be to be considered"/>
		<float name="m_fForwardProjectionInSeconds" min="0.0f" max="100.0f" step="1.0f" init="2.0f" description="How close together must the nodes be to be considered"/>
		<float name="m_fDistSqBeforeMovingToNextNode" min="0.0f" max="2500.0f" step="1.0f" init="25.0f" description="Config value for how far away the entity can be befor moving to the next node"/>
		<float name="m_fDistBeforeMovingToNextNode" min="0.0f" max="50.0f" step="1.0f" init="5.0f" description="Config value for how far away the entity can be before moving to the next node"/>
		<float name="m_fMaxHeadingAdjustDeg" min="0.0f" max="180.0f" step="1.0f" init="45.0f" description="Config value for the max heading adjustment to allow the entity to follow the road"/>
		<float name="m_fMaxDistSqFromRoad" min="0.0f" max="2500.0f" step="1.0f" init="100.0f" description="Config value for how far away the entity can be from the road"/>
		<float name="m_fTimeBeforeRestart" min="0.0f" max="25.0f" step="1.0f" init="1.0f" description="How many seconds before retesting for a valid path"/>
		<bool name="m_bIgnoreSwitchedOffNodes" init="false" description="Ignored nodes that are turned off"/>
		<bool name="m_bUseWaterNodes" init="false" description="Include water nodes in the search"/>
		<bool name="m_bUseOnlyHighwayNodes" init="false" description="Only use highway nodes"/>
		<bool name="m_bSearchUpFromPosition" init="false" description="Search above position"/>
	</structdef>


</ParserSchema>
