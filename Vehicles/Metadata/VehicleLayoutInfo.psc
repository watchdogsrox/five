<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="CVehicleLayoutInfo::eLayoutFlags">
    <enumval name="NoDriver"/>
    <enumval name="StreamAnims"/>
    <enumval name="WaitForRerservationInGroup"/>
    <enumval name="WarpIntoAndOut"/>
    <enumval name="BikeLeansUnlessMoving"/>
    <enumval name="UsePickUpPullUp"/>
    <enumval name="AllowEarlyDoorAndSeatUnreservation"/>
    <enumval name="UseVanOpenDoorBlendParams"/>
    <enumval name="UseStillToSitTransition"/>
    <enumval name="MustCloseDoor"/>
    <enumval name="UseDoorOscillation"/>
    <enumval name="NoArmIkOnInsideCloseDoor"/>
    <enumval name="NoArmIkOnOutsideCloseDoor"/>
    <enumval name="UseLeanSteerAnims"/>
    <enumval name="NoArmIkOnOutsideOpenDoor"/>
    <enumval name="UseSteeringWheelIk"/>
    <enumval name="PreventJustPullingOut"/>
    <enumval name="DontOrientateAttachWithWorldUp"/>
    <enumval name="OnlyExitIfDoorIsClosed"/>
    <enumval name="DisableJackingAndBusting"/>
    <enumval name="ClimbUpAfterOpenDoor"/>
    <enumval name="UseFinerAlignTolerance"/>
    <enumval name="Use2DBodyBlend"/>
    <enumval name="IgnoreFrontSeatsWhenOnVehicle"/>
    <enumval name="AutomaticCloseDoor"/>
    <enumval name="WarpInWhenStoodOnTop"/>
    <enumval name="DisableFastPoseWhenDrivebying"/>	
    <enumval name="DisableTargetRearDoorOpenRatio"/>
		<enumval name="PreventInterruptAfterClimbUp"/>
		<enumval name="PreventInterruptAfterOpenDoor"/>
		<enumval name="UseLowerDoorBlockTest"/>
    <enumval name="LockedForSpecialEdition"/>
	</enumdef>

	<structdef type="CVehicleLayoutInfo::sSeat">
    <pointer name="m_SeatInfo" type="CVehicleSeatInfo" policy="external_named" toString="CVehicleSeatInfo::GetNameFromInfo" fromString="CVehicleSeatInfo::GetInfoFromName"/>
	<pointer name="m_SeatAnimInfo" type="CVehicleSeatAnimInfo" policy="external_named" toString="CVehicleSeatAnimInfo::GetNameFromInfo" fromString="CVehicleSeatAnimInfo::GetInfoFromName"/>
  </structdef>

  <structdef type="CVehicleLayoutInfo::sEntryPoint">
	<pointer name="m_EntryPointInfo" type="CVehicleEntryPointInfo" policy="external_named" toString="CVehicleEntryPointInfo::GetNameFromInfo" fromString="CVehicleEntryPointInfo::GetInfoFromName"/>
	<pointer name="m_EntryPointAnimInfo" type="CVehicleEntryPointAnimInfo" policy="external_named" toString="CVehicleEntryPointAnimInfo::GetNameFromInfo" fromString="CVehicleEntryPointAnimInfo::GetInfoFromName"/>
  </structdef>

  <structdef type="CVehicleLayoutInfo::sCellphoneClipsets">
    <string name="m_CellphoneClipsetDS" type="atHashString" init="ANIM_GROUP_CELLPHONE_IN_CAR_DS"/>
    <string name="m_CellphoneClipsetPS" type="atHashString" init="ANIM_GROUP_CELLPHONE_IN_CAR_PS"/>
    <string name="m_CellphoneClipsetDS_FPS" type="atHashString" init="ANIM_GROUP_CELLPHONE_IN_CAR_DS_FPS"/>
    <string name="m_CellphoneClipsetPS_FPS" type="atHashString" init="ANIM_GROUP_CELLPHONE_IN_CAR_DS_FPS"/>
  </structdef>
  
	<structdef type="CVehicleCoverBoundInfo">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<Vector3 name="m_Position" init="0.0f, 0.0f, 0.0f" min="-25.0f" max="25.0f" step="0.01f"/>
		<float name="m_Length" init="0.0f" min="0.0f" max="25.0f" step="0.01f"/>
		<float name="m_Width" init="0.0f" min="0.0f" max="25.0f" step="0.01f"/>
		<float name="m_Height" init="0.0f" min="0.0f" max="25.0f" step="0.01f"/>
		<array name="m_ActiveBoundExclusions" type="atArray">
			<string type="atHashString"/>
		</array>
	</structdef>

	<structdef type="CVehicleCoverBoundOffsetInfo">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<float name="m_ExtraSideOffset"/>
		<float name="m_ExtraForwardOffset"/>
		<float name="m_ExtraBackwardOffset"/>
		<float name="m_ExtraZOffset"/>
		<array name="m_CoverBoundInfos" type="atArray">
			<struct type="CVehicleCoverBoundInfo"/>
		</array>
	</structdef>

	<structdef type="CBicycleInfo::sGearClipSpeeds">
		<float name="m_Speed"/>
		<float name="m_ClipRate"/>
	</structdef>
	
	<structdef type="CBicycleInfo">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<array name="m_CruiseGearClipSpeeds" type="atArray">
			<struct type="CBicycleInfo::sGearClipSpeeds"/>
		</array>
		<array name="m_FastGearClipSpeeds" type="atArray">
			<struct type="CBicycleInfo::sGearClipSpeeds"/>
		</array>
		<float name="m_SpeedToTriggerBicycleLean" min="0.0f" max="10.0f" step="0.01f" init="2.0f"/>
		<float name="m_SpeedToTriggerStillTransition" min="0.0f" max="10.0f" step="0.01f" init="2.0f"/>
		<float name="m_DesiredSitRateMult" min="0.0f" max="10.0f" step="0.01f" init="0.7f"/>
		<float name="m_DesiredStandingInAirRateMult" min="0.0f" max="10.0f" step="0.01f" init="1.5f"/>
		<float name="m_DesiredStandingRateMult" min="0.0f" max="10.0f" step="0.01f" init="1.1f"/>
		<float name="m_StillToSitPedalGearApproachRate" min="0.0f" max="5.0f" step="0.01f" init="2.0f"/>
		<float name="m_StandPedalGearApproachRate" min="0.0f" max="5.0f" step="0.01f" init="2.0f"/>
		<float name="m_SitPedalGearApproachRate" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
		<float name="m_SitToStandPedalAccelerationScalar" min="0.0f" max="5.0f" step="0.01f" init="0.9f"/>
		<float name="m_SitToStandPedalMaxRate" min="0.0f" max="5.0f" step="0.01f" init="1.25f"/>
		<float name="m_PedalToFreewheelBlendDuration" min="0.0f" max="2.0f" step="0.01f" init="0.25f"/>
		<float name="m_FreewheelToPedalBlendDuration" min="0.0f" max="2.0f" step="0.01f" init="0.1f"/>
		<float name="m_StillToSitToSitBlendOutDuration" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
		<float name="m_SitTransitionJumpPrepBlendDuration" min="0.0f" max="2.0f" step="0.01f" init="0.2f"/>
		<float name="m_MinForcedInitialBrakeTime" min="0.0f" max="2.0f" step="0.01f" init="0.3f"/>
		<float name="m_MinForcedStillToSitTime" min="0.0f" max="2.0f" step="0.01f" init="0.4f"/>
		<float name="m_MinTimeInStandFreewheelState" min="0.0f" max="10.0f" step="0.01f" init="3.5f"/>
		<bool name="m_IsFixie" init="false"/>
		<bool name="m_HasImpactAnims" init="false"/>
		<bool name="m_UseHelmet" init="false"/>
		<bool name="m_CanTrackStand" init="false"/>
	</structdef>
	
	<structdef type="CPOVTuningInfo">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<float name="m_StickCenteredBodyLeanYSlowMin" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_StickCenteredBodyLeanYSlowMax" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_StickCenteredBodyLeanYFastMin" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_StickCenteredBodyLeanYFastMax" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_BodyLeanXDeltaFromCenterMinBikeLean" min="0.0f" max="0.5f" step="0.01f" init="0.5f"/>
		<float name="m_BodyLeanXDeltaFromCenterMaxBikeLean" min="0.0f" max="0.5f" step="0.01f" init="0.5f"/>
		<float name="m_MinForwardsPitchSlow" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_MaxForwardsPitchSlow" min="-1.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_MinForwardsPitchFast" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_MaxForwardsPitchFast" min="-1.0f" max="1.0f" step="0.01f" init="1.0f"/>
	</structdef>

	<enumdef type="CClipSetMap::eConditionFlags">
		<enumval name="CF_CommonAnims"/>
		<enumval name="CF_EntryAnims"/>
		<enumval name="CF_InVehicleAnims"/>
		<enumval name="CF_ExitAnims"/>
		<enumval name="CF_BreakInAnims"/>
		<enumval name="CF_JackingAnims"/>
	</enumdef>

	<enumdef type="CClipSetMap::eInformationFlags">
		<enumval name="IF_JackedPedExitsWillingly"/>
		<enumval name="IF_ScriptFlagOnly"/>
	</enumdef>

	<structdef type="CClipSetMap::sVariableClipSetMap">
		<string name="m_ClipSetId" type="atHashString" />
		<string name="m_VarClipSetId" type="atHashString" />
		<bitset name="m_ConditionFlags" type="fixed32" values="CClipSetMap::eConditionFlags"/>
		<bitset name="m_InformationFlags" type="fixed32" values="CClipSetMap::eInformationFlags"/>
	</structdef>
	
	<structdef type="CClipSetMap">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<array name="m_Maps" type="atArray">
			<struct type="CClipSetMap::sVariableClipSetMap"/>
		</array>
	</structdef>

	<structdef type="CAnimRateSet::sAnimRateTuningPair">
		<float name="m_SPRate" min="0.1f" max="5.0f" step="0.01f" init="1.0f"/>
		<float name="m_MPRate" min="0.1f" max="5.0f" step="0.01f" init="1.0f"/>
	</structdef>
	
	<structdef type="CAnimRateSet">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<struct name="m_NormalEntry" type="CAnimRateSet::sAnimRateTuningPair"/>
		<struct name="m_AnimCombatEntry" type="CAnimRateSet::sAnimRateTuningPair"/>
		<struct name="m_NoAnimCombatEntry" type="CAnimRateSet::sAnimRateTuningPair"/>
		<struct name="m_CombatJackEntry" type="CAnimRateSet::sAnimRateTuningPair"/>
		<struct name="m_ForcedEntry" type="CAnimRateSet::sAnimRateTuningPair"/>
		<bool name="m_UseInVehicleCombatRates" init="false"/>
		<struct name="m_NormalInVehicle" type="CAnimRateSet::sAnimRateTuningPair"/>
		<struct name="m_NoAnimCombatInVehicle" type="CAnimRateSet::sAnimRateTuningPair"/>
		<struct name="m_NormalExit" type="CAnimRateSet::sAnimRateTuningPair"/>
		<struct name="m_NoAnimCombatExit" type="CAnimRateSet::sAnimRateTuningPair"/>
	</structdef>

	<structdef type ="CFirstPersonDriveByLookAroundSideData::CAxisData">
		<float name="m_Offset" init="0.0f"/>
		<Vector2 name="m_AngleToBlendInOffset" init="0.0f, 0.0f" min="-360.0f" max="360.0f"/>
	</structdef>

	<structdef type ="CFirstPersonDriveByLookAroundSideData">
		<array name="m_Offsets" type="atFixedArray" size="3">
			<struct type="CFirstPersonDriveByLookAroundSideData::CAxisData"/>
		</array>
		<Vector2 name="m_ExtraRelativePitch" init="0.0f, 0.0f" min="-90.0f" max="90.0f"/>
		<Vector2 name="m_AngleToBlendInExtraPitch" init="45.0f, 90.0f" min="0.0f" max="360.0f"/>
	</structdef>

  <structdef type ="CFirstPersonDriveByWheelClipData">
    <Vector2 name="m_HeadingLimitsLeft" init="0.380f, 0.430f" min="0.0f" max="1.0f" />
    <Vector2 name="m_HeadingLimitsRight" init="0.480f, 0.530f" min="0.0f" max="1.0f" />
    <Vector2 name="m_PitchLimits" init="0.470f, 0.530f" min="0.0f" max="1.0f" />
    <Vector2 name="m_PitchOffset" init="0.000f, 0.050f" min="0.0f" max="1.0f" />
    <Vector2 name="m_WheelAngleLimits" init="-0.7f, 0.0f" min="-1.0f" max="0.0f" />
    <Vector2 name="m_WheelAngleOffset" init="0.010f, 0.040f" min="0.0f" max="1.0f" />
    <float name="m_MaxWheelOffsetY" init="0.170f" />
    <float name="m_WheelClipLerpInRate" init="0.150f" />
    <float name="m_WheelClipLerpOutRate" init="0.180f" />
  </structdef>

	<structdef type ="CFirstPersonDriveByLookAroundData">
		<string name="m_Name" type="atHashString" ui_key="true"/>
    <bool name="m_AllowLookback" init="true"/>
		<Vector2 name="m_HeadingLimits" init="-95.0f, 95.0f" min="-360.0f" max="360.0f"/>
		<struct name="m_DataLeft" type="CFirstPersonDriveByLookAroundSideData"/>
		<struct name="m_DataRight" type="CFirstPersonDriveByLookAroundSideData"/>
    <struct name="m_WheelClipInfo" type="CFirstPersonDriveByWheelClipData"/>
	</structdef>

	<structdef type="CVehicleLayoutInfo">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<array name="m_Seats" type="atArray">
			<struct type="CVehicleLayoutInfo::sSeat"/>
		</array>
		<array name="m_EntryPoints" type="atArray">
			<struct type="CVehicleLayoutInfo::sEntryPoint"/>
		</array>
		<bitset name="m_LayoutFlags" type="fixed32" values="CVehicleLayoutInfo::eLayoutFlags"/>
			<pointer name="m_BicycleInfo" type="CBicycleInfo" policy="external_named" toString="CBicycleInfo::GetNameFromInfo" fromString="CBicycleInfo::GetInfoFromName"/>
			<pointer name="m_AnimRateSet" type="CAnimRateSet" policy="external_named" toString="CAnimRateSet::GetNameFromInfo" fromString="CAnimRateSet::GetInfoFromName"/>
		<string name="m_HandsUpClipSetId" type="atHashString" init="busted_vehicle_std"/>
		<Vector3 name="m_SteeringWheelOffset" init="0.0f, 0.35f, 0.32f" min="-5.5f" max="5.5f"/>
		<float name="m_MaxXAcceleration" min="0.0f" max="50.0f" step="0.01f" init="25.0f"/>
		<float name="m_BodyLeanXApproachSpeed" min="0.0f" max="20.0f" step="0.01f" init="15.0f"/>
		<float name="m_BodyLeanXSmallDelta" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_LookBackApproachSpeedScale" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<array name="m_FirstPersonAdditiveIdleClipSets" type="atArray">
			<string type="atHashString"/>
		</array>
		<array name="m_FirstPersonRoadRageClipSets" type="atArray">
			<string type="atHashString"/>
		</array>
		<struct name="m_CellphoneClipsets" type="CVehicleLayoutInfo::sCellphoneClipsets"/>
	</structdef>

</ParserSchema>
