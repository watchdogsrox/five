<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="CDriveByInfo::eDriveByFlags">
    <enumval name="AdjustHeight"/>
    <enumval name="DampenRecoil"/>
    <enumval name="AllowDamageToVehicle"/>
    <enumval name="AllowDamageToVehicleOccupants"/>
    <enumval name="UsePedAsCamEntity"/>
	<enumval name="NeedToOpenDoors"/>
	<enumval name="UseThreeAnimIntroOutro"/>
    <enumval name="UseMountedProjectileTask"/>
    <enumval name="UseThreeAnimThrow"/>
	<enumval name="UseSpineAdditive"/>
	<enumval name="LeftHandedProjctiles"/>
	<enumval name="LeftHandedFirstPersonAnims"/>
	<enumval name="LeftHandedUnarmedFirstPersonAnims"/>
    <enumval name="WeaponAttachedToLeftHand1HOnly"/>
  <enumval name="UseBlockingAnimsOutsideAimRange"/>
  <enumval name="AllowSingleSideOvershoot"/>
    <enumval name="UseAimCameraInThisSeat"/>
  </enumdef>

  <enumdef type="CVehicleDriveByAnimInfo::eDrivebyNetworkType">
    <enumval name="STD_CAR_DRIVEBY"/>
    <enumval name="BIKE_DRIVEBY"/>
    <enumval name="TRAILER_DRIVEBY"/>
    <enumval name="REAR_HELI_DRIVEBY"/>
    <enumval name="THROWN_GRENADE_DRIVEBY"/>
    <enumval name="MOUNTED_DRIVEBY"/>
    <enumval name="MOUNTED_THROW"/>
  </enumdef>

  <structdef type="CDrivebyWeaponGroup">
    <string name="m_Name" type="atHashString" ui_key="true"/>
    <array name="m_WeaponGroupNames" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_WeaponTypeNames" type="atArray">
      <string type="atHashString"/>
    </array>
  </structdef>

  <structdef type="CVehicleDriveByAnimInfo">
    <string name="m_Name" type="atHashString" ui_key="true"/>    
    <pointer name="m_WeaponGroup" type="CDrivebyWeaponGroup" policy="external_named" toString="CDrivebyWeaponGroup::GetNameFromInfo" fromString="CDrivebyWeaponGroup::GetInfoFromName"/>
    <string name="m_DriveByClipSet" type="atHashString"/>
	  <string name="m_FirstPersonDriveByClipSet" type="atHashString"/>
    <array name="m_AltFirstPersonDriveByClipSets" type="atArray">
      <struct type="CVehicleDriveByAnimInfo::sAltClips"/>
    </array>
    <string name="m_RestrictedDriveByClipSet" type="atHashString"/>
    <string name="m_VehicleMeleeClipSet" type="atHashString"/>
    <string name="m_FirstPersonVehicleMeleeClipSet" type="atHashString"/>
    <enum name="m_Network" type="CVehicleDriveByAnimInfo::eDrivebyNetworkType"/>
    <bool name="m_UseOverrideAngles" init="false"/>
    <bool name="m_OverrideAnglesInThirdPersonOnly" init="false"/>
    <float name="m_OverrideMinAimAngle" min="-360.0f" max="360.0f" step="0.01f" init="0.0f"/>
    <float name="m_OverrideMaxAimAngle" min="-360.0f" max="360.0f" step="0.01f" init="0.0f"/>
    <float name="m_OverrideMinRestrictedAimAngle" min="-360.0f" max="360.0f" step="0.01f" init="0.0f"/>
    <float name="m_OverrideMaxRestrictedAimAngle" min="-360.0f" max="360.0f" step="0.01f" init="0.0f"/>
  </structdef>

  <structdef type="CVehicleDriveByAnimInfo::sAltClips">
    <array name="m_Weapons" type="atArray">
      <string type="atHashString"/>
    </array>
    <string name="m_ClipSet" type="atHashString"/>
  </structdef>

  <structdef type="CVehicleDriveByInfo">
    <string name="m_Name" type="atHashString" ui_key="true"/>
    <float name="m_MinAimSweepHeadingAngleDegs"/>
    <float name="m_MaxAimSweepHeadingAngleDegs"/>
	<float name="m_FirstPersonMinAimSweepHeadingAngleDegs" min="-360.0f" max="360.0f" step="0.01f" init="-180.0f"/>
	<float name="m_FirstPersonMaxAimSweepHeadingAngleDegs" min="-360.0f" max="360.0f" step="0.01f" init="180.0f"/>
	<float name="m_FirstPersonUnarmedMinAimSweepHeadingAngleDegs" min="-360.0f" max="360.0f" step="0.01f" init="-180.0f"/>
	<float name="m_FirstPersonUnarmedMaxAimSweepHeadingAngleDegs" min="-360.0f" max="360.0f" step="0.01f" init="180.0f"/>
    <float name="m_MinRestrictedAimSweepHeadingAngleDegs"/>
    <float name="m_MaxRestrictedAimSweepHeadingAngleDegs"/>
    <float name="m_MinSmashWindowAngleDegs" init="0.0f"/>
    <float name="m_MaxSmashWindowAngleDegs" init="0.0f"/>
	<float name="m_MinSmashWindowAngleFirstPersonDegs" init="0.0f"/>
	<float name="m_MaxSmashWindowAngleFirstPersonDegs" init="0.0f"/> 
    <float name="m_MaxSpeedParam" min="0.0f" max="0.5f" step="0.01f" init="0.5f"/>
    <float name="m_MaxLongitudinalLeanBlendWeightDelta" min="0.0f" max="0.5f" step="0.01f" init="0.25f"/>
    <float name="m_MaxLateralLeanBlendWeightDelta" min="0.0f" max="0.5f" step="0.01f" init="0.25f"/>
    <float name="m_ApproachSpeedToWithinMaxBlendDelta" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <float name="m_SpineAdditiveBlendInDelay" min="0.0f" max="2.0f" step="0.01f" init="0.05f"/>
    <float name="m_SpineAdditiveBlendInDurationStill" min="0.0f" max="2.0f" step="0.01f" init="0.15f"/>
    <float name="m_SpineAdditiveBlendInDuration" min="0.0f" max="2.0f" step="0.01f" init="0.75f"/>
    <float name="m_SpineAdditiveBlendOutDelay" min="0.0f" max="2.0f" step="0.01f" init="0.0f"/>
    <float name="m_SpineAdditiveBlendOutDuration" min="0.0f" max="2.0f" step="0.01f" init="0.5f"/>  
	<float name="m_MinUnarmedDrivebyYawIfWindowRolledUp" min="-3.14f" max="3.14f" step="0.01f" init="-3.14f"/>
	<float name="m_MaxUnarmedDrivebyYawIfWindowRolledUp" min="-3.14f" max="3.14f" step="0.01f" init="3.14f"/>
    <array name="m_DriveByAnimInfos" type="atArray">
			<pointer type="CVehicleDriveByAnimInfo" policy="external_named" toString="CVehicleDriveByAnimInfo::GetNameFromInfo" fromString="CVehicleDriveByAnimInfo::GetInfoFromName"/>
    </array>
    <string name="m_DriveByCamera" type="atHashString"/>
    <string name="m_PovDriveByCamera" type="atHashString"/>
    <bitset name="m_DriveByFlags" type="fixed32" values="CDriveByInfo::eDriveByFlags"/>
  </structdef>

  <enumdef type="CVehicleSeatInfo::eDefaultCarTask">
    <enumval name="TASK_DRIVE_WANDER"/>
    <enumval name="TASK_HANGING_ON_VEHICLE"/>
  </enumdef>

	<enumdef type="CVehicleSeatInfo::eSeatFlags">
		<enumval name="IsFrontSeat"/>
		<enumval name="IsIdleSeat"/>
		<enumval name="NoIndirectExit"/>
		<enumval name="PedInSeatTargetable"/>
		<enumval name="KeepOnHeadProp"/>
		<enumval name="DontDetachOnWorldCollision"/>
		<enumval name="DisableFinerAlignTolerance"/>
		<enumval name="IsExtraShuffleSeat"/>
		<enumval name="DisableWhenTrailerAttached"/>
		<enumval name="UseSweepsForTurret"/>
		<enumval name="UseSweepLoopsForTurret"/>
		<enumval name="GivePilotHelmetForSeat"/>
		<enumval name="FakeSeat"/>
		<enumval name="CameraSkipWaitingForTurretAlignment"/>
		<enumval name="DisallowHeadProp"/>
   		<enumval name="PreventJackInWater"/>
	</enumdef>

  <structdef type="CVehicleSeatInfo">
    <string name="m_Name" type="atHashString" ui_key="true"/>
    <string name="m_SeatBoneName" type="atString"/>
    <string name="m_ShuffleLink" type="atHashString"/>
    <string name="m_RearSeatLink" type="atHashString"/>
    <enum name="m_DefaultCarTask" type="CVehicleSeatInfo::eDefaultCarTask"/>
    <bitset name="m_SeatFlags" type="fixed32" values="CVehicleSeatInfo::eSeatFlags"/>
    <float name="m_HairScale" init="0.0f"/>
    <string name="m_ShuffleLink2" type="atHashString"/>
  </structdef>

  <enumdef type="CVehicleSeatAnimInfo::eInVehicleMoveNetwork">
    <enumval name="VEHICLE_DEFAULT"/>
    <enumval name="VEHICLE_BICYCLE"/>
    <enumval name="VEHICLE_TURRET_SEATED"/>
    <enumval name="VEHICLE_TURRET_STANDING"/>
  </enumdef>

  <enumdef type="CVehicleSeatAnimInfo::eSeatAnimFlags">
		<enumval name="CanDetachViaRagdoll"/>
		<enumval name="WeaponAttachedToLeftHand"/>
		<enumval name="WeaponRemainsVisible"/>
		<enumval name="AttachLeftHand"/>
		<enumval name="AttachLeftFoot"/>
		<enumval name="AttachRightHand"/>
		<enumval name="AttachRightFoot"/>
		<enumval name="CannotBeJacked"/>
		<enumval name="UseStandardInVehicleAnims"/>
		<enumval name="UseBikeInVehicleAnims"/>
		<enumval name="UseJetSkiInVehicleAnims"/>
		<enumval name="HasPanicAnims"/>
		<enumval name="UseCloseDoorBlendAnims"/>
		<enumval name="SupportsInAirState"/>
		<enumval name="UseBasicAnims"/>
		<enumval name="PreventShuffleJack"/>
		<enumval name="RagdollWhenVehicleUpsideDown"/>
		<enumval name="FallsOutWhenDead"/>
		<enumval name="CanExitEarlyInCombat"/>
		<enumval name="UseBoatInVehicleAnims"/>
		<enumval name="CanWarpToDriverSeatIfNoDriver"/>
		<enumval name="NoIK"/>
		<enumval name="DisableAbnormalExits"/>
		<enumval name="SimulateBumpiness"/>
		<enumval name="IsLowerPrioritySeatInSP"/>
		<enumval name="KeepCollisionOnWhenInVehicle"/>
		<enumval name="UseDirectEntryOnlyWhenEntering"/>
		<enumval name="UseTorsoLeanIK"/>
		<enumval name="UseRestrictedTorsoLeanIK"/>
		<enumval name="WarpIntoSeatIfStoodOnIt"/>
		<enumval name="RagdollAtExtremeVehicleOrientation"/>
		<enumval name="NoShunts"/>
	</enumdef>

  <structdef type="CVehicleSeatAnimInfo">
    <string name="m_Name" type="atHashString" ui_key="true"/>
	<pointer name="m_DriveByInfo" type="CVehicleDriveByInfo" policy="external_named" toString="CVehicleDriveByInfo::GetNameFromInfo" fromString="CVehicleDriveByInfo::GetInfoFromName"/>
	<pointer name="m_InsideClipSetMap" type="CClipSetMap" policy="external_named" toString="CClipSetMap::GetNameFromInfo" fromString="CClipSetMap::GetInfoFromName"/>
    <string name="m_PanicClipSet" type="atHashString"/>
    <string name="m_AgitatedClipSet" type="atHashString"/>
    <string name="m_DuckedClipSet" type="atHashString"/>
    <string name="m_FemaleClipSet" type="atHashString"/>
    <string name="m_LowriderLeanClipSet" type="atHashString"/>
    <string name="m_AltLowriderLeanClipSet" type="atHashString"/>
    <string name="m_LowLODIdleAnim" type="atHashString"/>
    <string name="m_SeatAmbientContext" type="atHashString"/>
    <enum name="m_InVehicleMoveNetwork" type="CVehicleSeatAnimInfo::eInVehicleMoveNetwork"/>
    <bitset name="m_SeatAnimFlags" type="fixed32" values="CVehicleSeatAnimInfo::eSeatAnimFlags"/>
		<float name="m_SteeringSmoothing" init="0.1f"/>
    <string name="m_ExitToAimInfoName" type="atHashString"/>
		<string name="m_MaleGestureClipSetId" type="atHashString" init="" />
		<string name="m_FemaleGestureClipSetId" type="atHashString" init="" />
    <float name="m_FPSMinSteeringRateOverride" init="-1.0f"/>
    <float name="m_FPSMaxSteeringRateOverride" init="-1.0f"/>
    <Vector3 name="m_SeatCollisionBoundsOffset" init="0.0f, 0.0f, 0.0f" />
	</structdef>

	<enumdef type="CSeatOverrideAnimInfo::eSeatOverrideAnimFlags">
		<enumval name="WeaponVisibleAttachedToRightHand"/>
    <enumval name="UseBasicAnims"/>
	</enumdef>
	
	<structdef type="CSeatOverrideAnimInfo">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<string name="m_SeatOverrideClipSet" type="atHashString"/>
		<bitset name="m_SeatOverrideAnimFlags" type="fixed32" values="CSeatOverrideAnimInfo::eSeatOverrideAnimFlags"/>
	</structdef>

	<structdef type="CSeatOverrideInfo">
		<pointer name="m_SeatAnimInfo" type="CVehicleSeatAnimInfo" policy="external_named" toString="CVehicleSeatAnimInfo::GetNameFromInfo" fromString="CVehicleSeatAnimInfo::GetInfoFromName"/>
		<pointer name="m_SeatOverrideAnimInfo" type="CSeatOverrideAnimInfo" policy="external_named" toString="CSeatOverrideAnimInfo::GetNameFromInfo" fromString="CSeatOverrideAnimInfo::GetInfoFromName"/>
	</structdef>

	<structdef type="CInVehicleOverrideInfo">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<array name="m_SeatOverrideInfos" type="atArray">
			<pointer type="CSeatOverrideInfo" policy="owner"/>
		</array>
	</structdef>
	
</ParserSchema>
