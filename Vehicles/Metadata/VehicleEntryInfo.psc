<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CEntryAnimVariations" >
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<bitset name="m_Flags" type="fixed32" values="CConditionalAnims::eConditionalAnimFlags"/>
		<array name="m_Conditions" type="atArray">
			<pointer type="CScenarioCondition" policy="owner"/>
		</array>
		<array name="m_EntryAnims" type="atArray">
			<pointer type="CConditionalClipSet" policy="owner"/>
		</array>
		<array name="m_JackingAnims" type="atArray">
			<pointer type="CConditionalClipSet" policy="owner"/>
		</array>
		<array name="m_ExitAnims" type="atArray">
			<pointer type="CConditionalClipSet" policy="owner"/>
		</array>
	</structdef>

	<enumdef type="CVehicleEntryPointAnimInfo::eEnterVehicleMoveNetwork">
    <enumval name="ENTER_VEHICLE_STD"/>
    <enumval name="MOUNT_ANIMAL"/>    
  </enumdef>

	<enumdef type="CVehicleEntryPointAnimInfo::eEntryPointFlags">
		<enumval name="UseVehicleRelativeEntryPosition"/>
		<enumval name="HasClimbUp"/>
		<enumval name="WarpOut"/>
		<enumval name="IgnoreMoverFixUp"/>
		<enumval name="HasVaultUp"/>
		<enumval name="DisableEntryIfBikeOnSide"/>
		<enumval name="HasGetInFromWater"/>
		<enumval name="ExitAnimHasLongDrop"/>
		<enumval name="JackIncludesGetIn"/>
		<enumval name="ForcedEntryIncludesGetIn"/>
		<enumval name="UsesNoDoorTransitionForEnter"/>
		<enumval name="UsesNoDoorTransitionForExit"/>
		<enumval name="HasGetOutToVehicle"/>
		<enumval name="UseNewPlaneSystem"/>
		<enumval name="DontCloseDoorInside"/>
		<enumval name="DisableJacking"/>
		<enumval name="UseOpenDoorBlendAnims"/>
		<enumval name="PreventJackInterrupt"/>
		<enumval name="DontCloseDoorOutside"/>
		<enumval name="HasOnVehicleEntry"/>
		<enumval name="HasClimbDown"/>
		<enumval name="FixUpMoverToEntryPointOnExit"/>
		<enumval name="CannotBeUsedByCuffedPed"/>
		<enumval name="HasClimbUpFromWater"/>
   	<enumval name="NavigateToSeatFromClimbup"/>
   	<enumval name="UseSeatClipSetAnims"/>	
		<enumval name="NavigateToWarpEntryPoint"/>
		<enumval name="UseGetUpAfterJack"/>
		<enumval name="JackVariationsIncludeGetIn"/>
		<enumval name="DeadJackIncludesGetIn"/>
		<enumval name="HasCombatEntry"/>
    <enumval name="UseStandOnGetIn"/>
	</enumdef>

  <structdef type="CVehicleEntryPointAnimInfo" >
    <string name="m_Name" type="atHashString" ui_key="true"/>
	<pointer name="m_CommonClipSetMap" type="CClipSetMap" policy="external_named" toString="CClipSetMap::GetNameFromInfo" fromString="CClipSetMap::GetInfoFromName"/>
	<pointer name="m_EntryClipSetMap" type="CClipSetMap" policy="external_named" toString="CClipSetMap::GetNameFromInfo" fromString="CClipSetMap::GetInfoFromName"/>
	<pointer name="m_ExitClipSetMap" type="CClipSetMap" policy="external_named" toString="CClipSetMap::GetNameFromInfo" fromString="CClipSetMap::GetInfoFromName"/>
	<string name="m_AlternateTryLockedDoorClipId" type="atHashString"/>
	<string name="m_AlternateForcedEntryClipId" type="atHashString"/>
	<string name="m_AlternateJackFromOutSideClipId" type="atHashString"/>
	<string name="m_AlternateBeJackedFromOutSideClipId" type="atHashString"/>
		<string name="m_AlternateEntryPointClipSetId" type="atHashString"/>
    <enum name="m_EnterVehicleMoveNetwork" type="CVehicleEntryPointAnimInfo::eEnterVehicleMoveNetwork"/>
    <Vector3 name="m_EntryTranslation"/>
	<Vector2 name="m_OpenDoorTranslation"/>
	<float name="m_OpenDoorHeadingChange" min="-3.14f" max="3.14f" step="0.01f" init="0.0f"/>
	<float name="m_EntryHeadingChange" min="-3.14f" max="3.14f" step="0.01f" init="0.0f"/>
    <float name="m_ExtraZForMPPlaneWarp"  min="-10.0f" max="10.0f" step="0.01f" init="-2.0f"/>
    <bitset name="m_EntryPointFlags" type="fixed32" values="CVehicleEntryPointAnimInfo::eEntryPointFlags"/>
    <pointer name="m_EntryAnimVariations" type="CEntryAnimVariations" policy="external_named" toString="CEntryAnimVariations::GetNameFromInfo" fromString="CEntryAnimVariations::GetInfoFromName"/>
    <string name="m_NMJumpFromVehicleTuningSet" type="atHashString"/>
  </structdef>

  <enumdef type="CVehicleEntryPointInfo::eWindowId">
    <enumval name="FRONT_LEFT"/>
    <enumval name="FRONT_RIGHT"/>
    <enumval name="REAR_LEFT"/>
    <enumval name="REAR_RIGHT"/>
    <enumval name="FRONT_WINDSCREEN"/>
    <enumval name="REAR_WINDSCREEN"/>
    <enumval name="MID_LEFT"/>
    <enumval name="MID_RIGHT"/>
    <enumval name="INVALID"/>
  </enumdef>

	<enumdef type="CVehicleEntryPointInfo::eSide">
		<enumval name="SIDE_LEFT"/>
		<enumval name="SIDE_RIGHT"/>
    <enumval name="SIDE_REAR"/>
	</enumdef>

	<enumdef type="CVehicleEntryPointInfo::eEntryPointFlags">
		<enumval name="IsPassengerOnlyEntry"/>
		<enumval name="IgnoreSmashWindowCheck"/>
		<enumval name="TryLockedDoorOnGround"/>
		<enumval name="MPWarpInOut"/>
		<enumval name="SPEntryAllowedAlso"/>
		<enumval name="WarpOutInPlace"/>
    <enumval name="BlockJackReactionUntilJackerIsReady"/>
    <enumval name="UseFirstMulipleAccessSeatForDirectEntryPoint"/>
    <enumval name="IsPlaneHatchEntry"/>
    <enumval name="ClimbUpOnly"/>
    <enumval name="NotUsableOutsideVehicle"/>
    <enumval name="AutoCloseThisDoor"/>
    <enumval name="HasClimbDownToWater"/>
    <enumval name="ForceSkyDiveExitInAir"/>
  </enumdef>

  <enumdef type="CExtraVehiclePoint::eLocationType">
    <enumval name="BONE"/>
    <enumval name="SEAT_RELATIVE"/>
    <enumval name="DRIVER_SEAT_RELATIVE"/>
    <enumval name="WHEEL_GROUND_RELATIVE"/>
    <enumval name="ENTITY_RELATIVE"/>
  </enumdef>

  <enumdef type="CExtraVehiclePoint::ePointType">
    <enumval name="GET_IN"/>
    <enumval name="GET_IN_2"/>
    <enumval name="GET_IN_3"/>
    <enumval name="GET_IN_4"/>
    <enumval name="GET_OUT"/>
    <enumval name="VAULT_HAND_HOLD"/>
    <enumval name="UPSIDE_DOWN_EXIT"/>
    <enumval name="PICK_UP_POINT"/>
    <enumval name="PULL_UP_POINT"/>
    <enumval name="QUICK_GET_ON_POINT"/>
    <enumval name="EXIT_TEST_POINT"/>
    <enumval name="CLIMB_UP_FIXUP_POINT"/>
    <enumval name="ON_BOARD_JACK"/>
    <enumval name="POINT_TYPE_MAX"/>
  </enumdef>
	
	<structdef type="CExtraVehiclePoint" >
		<enum name="m_LocationType" type="CExtraVehiclePoint::eLocationType"/>
		<enum name="m_PointType" type="CExtraVehiclePoint::ePointType"/>
		<Vector3 name="m_Position"/>
		<float name="m_Heading" init="0.0f"/>
		<float name="m_Pitch" min="-3.14f" max="3.14f" step="0.01f" init="0.0f"/>
		<string name="m_BoneName" type="atString"/>
	</structdef>

	<structdef type="CVehicleExtraPointsInfo" >
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<array name="m_ExtraVehiclePoints" type="atArray">
			<struct type="CExtraVehiclePoint"/>
		</array>
	</structdef>

  <structdef type="CVehicleEntryPointInfo" >
    <string name="m_Name" type="atHashString" ui_key="true"/>
    <string name="m_DoorBoneName" type="atString"/>
		<string name="m_SecondDoorBoneName" type="atString"/>
    <string name="m_DoorHandleBoneName" type="atString"/>
    <enum name="m_WindowId" type="CVehicleEntryPointInfo::eWindowId"/>
    <enum name="m_VehicleSide" type="CVehicleEntryPointInfo::eSide"/>
    <array name="m_AccessableSeats" type="atArray">
			<pointer name="m_Seat" type="CVehicleSeatInfo" policy="external_named" toString="CVehicleSeatInfo::GetNameFromInfo" fromString="CVehicleSeatInfo::GetInfoFromName"/>
    </array>
		<pointer name="m_VehicleExtraPointsInfo" type="CVehicleExtraPointsInfo" policy="external_named" toString="CVehicleExtraPointsInfo::GetNameFromInfo" fromString="CVehicleExtraPointsInfo::GetInfoFromName"/>
		<bitset name="m_Flags" type="fixed32" values="CVehicleEntryPointInfo::eEntryPointFlags"/>
    <array name="m_BlockJackReactionSides" type="atArray">
      <enum type="CVehicleEntryPointInfo::eSide"/>
    </array>
    <enum name="m_BreakoutTestPoint" type="CExtraVehiclePoint::ePointType" init="CExtraVehiclePoint::POINT_TYPE_MAX"/>
	</structdef>

</ParserSchema>
