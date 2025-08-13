<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">


  

  <enumdef type="VehicleType">
    <enumval name="VEHICLE_TYPE_NONE" value="-1"/>
    
    <enumval name="VEHICLE_TYPE_CAR" value="0"/>
    <enumval name="VEHICLE_TYPE_PLANE"/>
    <enumval name="VEHICLE_TYPE_TRAILER"/>
    <enumval name="VEHICLE_TYPE_QUADBIKE"/>
    <enumval name="VEHICLE_TYPE_DRAFT"/>
    <enumval name="VEHICLE_TYPE_SUBMARINECAR"/>
    <enumval name="VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE"/>
    <enumval name="VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE"/>

    <enumval name="VEHICLE_TYPE_HELI"/>
    <enumval name="VEHICLE_TYPE_BLIMP"/>
    <enumval name="VEHICLE_TYPE_AUTOGYRO"/>
    
    <enumval name="VEHICLE_TYPE_BIKE"/>
    <enumval name="VEHICLE_TYPE_BICYCLE"/>
    
    <enumval name="VEHICLE_TYPE_BOAT"/>
    <enumval name="VEHICLE_TYPE_TRAIN"/>
    <enumval name="VEHICLE_TYPE_SUBMARINE"/>
  </enumdef>


  <enumdef type="eExtraIncludes">
    <enumval name="EXTRA_1" value="1"/>
    <enumval name="EXTRA_2"/>
    <enumval name="EXTRA_3"/>
    <enumval name="EXTRA_4"/>
    <enumval name="EXTRA_5"/>
    <enumval name="EXTRA_6"/>
    <enumval name="EXTRA_7"/>
    <enumval name="EXTRA_8"/>
    <enumval name="EXTRA_9"/>
    <enumval name="EXTRA_10"/>
    <enumval name="EXTRA_11"/>
    <enumval name="EXTRA_12"/>
    <enumval name="EXTRA_13"/>
  </enumdef>

  <enumdef type="eDoorId">
    <enumval name="VEH_EXT_DOOR_INVALID_ID" value="-1" />
    <enumval name="VEH_EXT_DOOR_DSIDE_F" value="0" />
    <enumval name="VEH_EXT_DOOR_DSIDE_R" />
    <enumval name="VEH_EXT_DOOR_PSIDE_F" />
    <enumval name="VEH_EXT_DOOR_PSIDE_R" />
    <enumval name="VEH_EXT_BONNET" />
    <enumval name="VEH_EXT_BOOT" />
  </enumdef>

  <enumdef type="eWindowId">
    <enumval name="VEH_EXT_WINDOWS_INVALID_ID" value="-1" />
    <enumval name="VEH_EXT_WINDSCREEN" value="0" />
    <enumval name="VEH_EXT_WINDSCREEN_R" />
    <enumval name="VEH_EXT_WINDOW_LF" />
    <enumval name="VEH_EXT_WINDOW_RF" />
    <enumval name="VEH_EXT_WINDOW_LR" />
    <enumval name="VEH_EXT_WINDOW_RR" />
    <enumval name="VEH_EXT_WINDOW_LM" />
    <enumval name="VEH_EXT_WINDOW_RM" />
  </enumdef>
  
  <enumdef type="eSwankness">
    <enumval name="SWANKNESS_0" value="0" />
    <enumval name="SWANKNESS_1" />
    <enumval name="SWANKNESS_2" />
    <enumval name="SWANKNESS_3" />
    <enumval name="SWANKNESS_4" />
    <enumval name="SWANKNESS_5" />
  </enumdef>

  <enumdef type="eVehicleWheelType">
      <enumval name="VWT_SPORT" value="0"/>
      <enumval name="VWT_MUSCLE" />
      <enumval name="VWT_LOWRIDER" />
      <enumval name="VWT_SUV" />
      <enumval name="VWT_OFFROAD" />
      <enumval name="VWT_TUNER" />
      <enumval name="VWT_BIKE" />
      <enumval name="VWT_HIEND" />
      <enumval name="VWT_SUPERMOD1" />
      <enumval name="VWT_SUPERMOD2" />
      <enumval name="VWT_SUPERMOD3" />
      <enumval name="VWT_SUPERMOD4" />
      <enumval name="VWT_SUPERMOD5" />
  </enumdef>

  <enumdef type="eVehiclePlateType">
      <enumval name="VPT_FRONT_AND_BACK_PLATES" value="0"/>
      <enumval name="VPT_FRONT_PLATES" />
      <enumval name="VPT_BACK_PLATES" />
      <enumval name="VPT_NONE" />
  </enumdef>

  <enumdef type="eVehicleDashboardType">
    <enumval name="VDT_BANSHEE" value="0"/>
    <enumval name="VDT_BOBCAT" />
    <enumval name="VDT_CAVALCADE" />
    <enumval name="VDT_COMET" />
    <enumval name="VDT_DUKES" />
    <enumval name="VDT_FACTION" />
    <enumval name="VDT_FELTZER" />
    <enumval name="VDT_FEROCI" />
    <enumval name="VDT_FUTO" />
    <enumval name="VDT_GENTAXI" />
    <enumval name="VDT_MAVERICK" />
    <enumval name="VDT_PEYOTE" />
    <enumval name="VDT_RUINER" />
    <enumval name="VDT_SPEEDO" />
    <enumval name="VDT_SULTAN" />
    <enumval name="VDT_SUPERGT" />
    <enumval name="VDT_TAILGATER" />
    <enumval name="VDT_TRUCK" />
    <enumval name="VDT_TRUCKDIGI" />
    <enumval name="VDT_INFERNUS" />
    <enumval name="VDT_ZTYPE" />
    <enumval name="VDT_LAZER" />
    <enumval name="VDT_SPORTBK" />
    <enumval name="VDT_RACE" />
    <enumval name="VDT_LAZER_VINTAGE" />
    <enumval name="VDT_PBUS2" />
  </enumdef>

  <enumdef type="eVehicleClass">
      <enumval name="VC_COMPACT" value="0"/>
      <enumval name="VC_SEDAN" />
      <enumval name="VC_SUV" />
      <enumval name="VC_COUPE" />
      <enumval name="VC_MUSCLE" />
      <enumval name="VC_SPORT_CLASSIC" />
      <enumval name="VC_SPORT" />
      <enumval name="VC_SUPER" />
      <enumval name="VC_MOTORCYCLE" />
      <enumval name="VC_OFF_ROAD" />
      <enumval name="VC_INDUSTRIAL" />
      <enumval name="VC_UTILITY" />
      <enumval name="VC_VAN" />
      <enumval name="VC_CYCLE" />
      <enumval name="VC_BOAT" />
      <enumval name="VC_HELICOPTER" />
      <enumval name="VC_PLANE" />
      <enumval name="VC_SERVICE" />
      <enumval name="VC_EMERGENCY" />
      <enumval name="VC_MILITARY" />
      <enumval name="VC_COMMERCIAL" />
      <enumval name="VC_RAIL" />
      <enumval name="VC_OPEN_WHEEL" />
  </enumdef>

  <const name="VLT_MAX" value="6"/>
  
  <structdef type ="CDriverInfo">
    <string name="m_driverName" type="atHashString"/>
    <string name="m_npcName" type="atHashString"/>
  </structdef>

  <structdef type="CDoorStiffnessInfo">
    <enum name="m_doorId" type="eDoorId" init="VEH_EXT_DOORS_INVALID_ID" />
    <float name="m_stiffnessMult" init="1.0" />
  </structdef>

  <structdef type ="CVfxExtraInfo">
    <bitset name="m_ptFxExtras" type="fixed" numBits="16" values="eExtraIncludes"/>
    <string name="m_ptFxName" type="atHashString"/>
    <Vector3 name="m_ptFxOffset" init="0.0, 0.0, 0.0" />
    <float name="m_ptFxRange" init="40.0f"/>
    <float name="m_ptFxSpeedEvoMin" init="5.0f"/>
    <float name="m_ptFxSpeedEvoMax" init="20.0f"/>
  </structdef>

  <structdef type ="CMobilePhoneSeatIKOffset">
	<Vector3 name="m_Offset" init="0.0, 0.0, 0.0" />
	<u8 name="m_SeatIndex" init="0" />
  </structdef>

  <structdef type ="CVehicleModelInfo::CVehicleOverrideRagdollThreshold">
    <int name="m_MinComponent" init="-1"/>
    <int name="m_MaxComponent" init="-1"/>
    <float name="m_ThresholdMult" init="1.0f"/>
  </structdef>

  <structdef type ="CAdditionalVfxWaterSample">
    <Vector3 name="m_position" init="0.0, 0.0, 0.0" />
    <float name="m_size" init="0.0" />
    <int name="m_component" init="0" />
  </structdef>
  
  <structdef type="CVehicleModelInfo::InitData">
    <string name="m_modelName" type="ConstString" description="Model filename" ui_key="true"/>
    <string name="m_txdName" type="ConstString" description="Texture dictionary filename"/>
    <string name="m_handlingId" type="ConstString" description="Vehicle handling Id"/>
    <string name="m_gameName" type="ConstString"/>
    <string name="m_vehicleMakeName" type="ConstString"/>
    <string name="m_expressionDictName" type="ConstString"/>
    <string name="m_expressionName" type="ConstString"/>
    <string name="m_animConvRoofDictName" type="ConstString"/>
    <string name="m_animConvRoofName" type="ConstString"/>
    <array name="m_animConvRoofWindowsAffected" type="atArray">
      <enum type="eWindowId" init="VEH_INVALID_ID" />
    </array>
	  <string name="m_ptfxAssetName" type="ConstString"/>
		
    <string name="m_audioNameHash" type="atHashString" init=""/>
    <string name="m_layout" type="atHashString" init="LAYOUT_STANDARD"/>
    <string name="m_POVTuningInfo" type="atHashString" init=""/>
    <string name="m_coverBoundOffsets" type="atHashString" init="STANDARD_COVER_OFFSET_INFO"/>
    <string name="m_explosionInfo" type="atHashString" init="EXPLOSION_INFO_DEFAULT"/>
    <string name="m_scenarioLayout" type="atHashString" init=""/>
    <string name="m_cameraName" type="atHashString" init="DEFAULT_FOLLOW_VEHICLE_CAMERA"/>
    <string name="m_aimCameraName" type="atHashString" init="DEFAULT_THIRD_PERSON_VEHICLE_AIM_CAMERA"/>
    <string name="m_bonnetCameraName" type="atHashString" init=""/>
    <string name="m_povCameraName" type="atHashString" init=""/>
    <string name="m_povTurretCameraName" type="atHashString" init=""/>
    <array name="m_firstPersonDrivebyData" type="atArray">
		<string type="atHashString"/>
	</array>
	<Vector3 name="m_FirstPersonDriveByIKOffset" init="0.0f, 0.0f, 0.0f" />
	<Vector3 name="m_FirstPersonDriveByUnarmedIKOffset" init="0.0f, 0.0f, 0.0f" />
	<Vector3 name="m_FirstPersonDriveByLeftPassengerIKOffset" init="0.0f, 0.0f, 0.0f" />
	<Vector3 name="m_FirstPersonDriveByRightPassengerIKOffset" init="0.0f, 0.0f, 0.0f" />
  <Vector3 name="m_FirstPersonDriveByRightRearPassengerIKOffset" init="0.0f, 0.0f, 0.0f" />
	<Vector3 name="m_FirstPersonDriveByLeftPassengerUnarmedIKOffset" init="0.0f, 0.0f, 0.0f" />
	<Vector3 name="m_FirstPersonDriveByRightPassengerUnarmedIKOffset" init="0.0f, 0.0f, 0.0f" />
	<Vector3 name="m_FirstPersonProjectileDriveByIKOffset" init="0.0f, 0.0f, 0.0f" />
  <Vector3 name="m_FirstPersonProjectileDriveByPassengerIKOffset" init="0.0f, 0.0f, 0.0f" />
  <Vector3 name="m_FirstPersonProjectileDriveByRearLeftIKOffset" init="0.0f, 0.0f, 0.0f" />
  <Vector3 name="m_FirstPersonProjectileDriveByRearRightIKOffset" init="0.0f, 0.0f, 0.0f" />
  <Vector3 name="m_FirstPersonVisorSwitchIKOffset" init="-0.025f, 0.025f, -0.030f" />
	<Vector3 name="m_FirstPersonMobilePhoneOffset" init="0.0f, 0.0f, 0.0f" />
	<Vector3 name="m_FirstPersonPassengerMobilePhoneOffset" init="0.0f, 0.0f, 0.0f" />
	<array name="m_FirstPersonMobilePhoneSeatIKOffset" type="atArray">
		<struct type="CMobilePhoneSeatIKOffset"/>
	</array>
	<Vector3 name="m_PovCameraOffset" init="0.0f, 0.0f, 0.0f" />
    <float name="m_PovCameraVerticalAdjustmentForRollCage" init="0.0f"/>
    <Vector3 name="m_PovPassengerCameraOffset" init="0.0f, 0.0f, 0.0f" />
    <Vector3 name="m_PovRearPassengerCameraOffset" init="0.0f, 0.0f, 0.0f" />
  	<string name="m_vfxInfoName" type="atHashString" init="VFXVEHICLEINFO_CAR_GENERIC"/>
    <bool name="m_shouldUseCinematicViewMode" init="true"/>
    <bool name="m_shouldCameraTransitionOnClimbUpDown" init="false"/>
    <bool name="m_shouldCameraIgnoreExiting" init="false"/>
    <bool name="m_AllowPretendOccupants" init="true"/>
    <bool name="m_AllowJoyriding" init="true"/>
    <bool name="m_AllowSundayDriving" init="true"/>
    <bool name="m_AllowBodyColorMapping" init="true"/>
    <float name="m_wheelScale"/>
    <float name="m_wheelScaleRear"/>
    <float name="m_dirtLevelMin"/>
    <float name="m_dirtLevelMax"/>
    <float name="m_envEffScaleMin" min="0.0" max="1.0"/>
    <float name="m_envEffScaleMax" min="0.0" max="1.0"/>
    <float name="m_envEffScaleMin2" min="0.0" max="1.0"/>
    <float name="m_envEffScaleMax2" min="0.0" max="1.0"/>
    <float name="m_damageMapScale" min="0.0" max="1.0"/>
    <float name="m_damageOffsetScale" min="0.0" max="1.0" init="1.0f" description="Reduction in total vehicle deformation, must match artist deformation texture"/>
    <Color32 name="m_diffuseTint" init="0x00FFFFFF"/>
    <float name="m_steerWheelMult" init="1.2f"/>
    <float name="m_firstPersonSteerWheelMult" init="-1.0f"/>
    <float name="m_HDTextureDist" init="5.0f"/>
    <array name="m_lodDistances" type="member" size="VLT_MAX">
      <float/>
    </array>
    
    <u8 name="m_identicalModelSpawnDistance" init="0" />
    <u8 name="m_maxNumOfSameColor" init="10" />
    <float name="m_defaultBodyHealth" init="1000.0f" />
	
    <float name="m_pretendOccupantsScale" min="0.0" max="4.0" init="1.0"/>
    <float name="m_visibleSpawnDistScale" min="0.1" max="4.0" init="1.0"/>
    <float name="m_trackerPathWidth" init="2.0" />

    <float name="m_weaponForceMult" init="1.0" />
    
    <u16 name="m_frequency"/>    
    <enum name="m_swankness" type="eSwankness" init="SWANKNESS_0" />

    <u16 name="m_maxNum" max="65535"/>
   
    <bitset name="m_flags" type="fixed" numBits="use_values" values="CVehicleModelInfoFlags::Flags"/>

    <enum name="m_type" type="VehicleType" init="VEHICLE_TYPE_CAR"/>

    <enum name="m_plateType" type="eVehiclePlateType" init="VPT_FRONT_AND_BACK_PLATES"/>

    <enum name="m_vehicleClass" type="eVehicleClass" init="VC_COMPACT"/>

    <enum name="m_dashboardType" type="eVehicleDashboardType" init="VDT_TAILGATER"/>

    <enum name="m_wheelType" type="eVehicleWheelType" init="VWT_SPORT"/>

    <array name="m_trailers" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_additionalTrailers" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_drivers" type="atArray">
      <struct type="CDriverInfo"/>
    </array>
  	<array name="m_extraIncludes" type="atArray">
          <bitset type="fixed" numBits="16" values="eExtraIncludes"/>
  	</array>
    <array name="m_vfxExtraInfos" type="atArray">
      <struct type="CVfxExtraInfo"/>
    </array>
    <array name="m_doorsWithCollisionWhenClosed" type="atArray">
      <enum type="eDoorId" init="VEH_EXT_DOORS_INVALID_ID" />
    </array>
    <array name="m_driveableDoors" type="atArray">
      <enum type="eDoorId" init="VEH_EXT_DOORS_INVALID_ID" />
    </array>
    <array name="m_doorStiffnessMultipliers" type="atArray">
      <struct type="CDoorStiffnessInfo" />
    </array>
    <bool name="m_bumpersNeedToCollideWithMap" init="false" />
    <bool name="m_needsRopeTexture" init="false" />
    <bitset name="m_requiredExtras" type="fixed" numBits="16" values="eExtraIncludes"/>
    <array name="m_rewards" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="m_cinematicPartCamera" type="atArray">
      <string type="atHashString"/>
    </array>
    <string name="m_NmBraceOverrideSet" type="atHashString"/>
    <Vector3 name="m_buoyancySphereOffset" init="0.0, 0.0, 0.0" />
    <float name="m_buoyancySphereSizeScale" init="1.0" />
    <array name="m_additionalVfxWaterSamples" type="atArray">
      <struct type="CAdditionalVfxWaterSample" />
    </array>
    <pointer name="m_pOverrideRagdollThreshold" type="CVehicleModelInfo::CVehicleOverrideRagdollThreshold" policy="owner" />

    <float name="m_maxSteeringWheelAngle" init="109.0" />
    <float name="m_maxSteeringWheelAnimAngle" init="90.0" />

    <float name="m_minSeatHeight" init ="-1.0" />

    <Vector3 name="m_lockOnPositionOffset" init="0.0f, 0.0f, 0.0f" />
    
    <float name="m_LowriderArmWindowHeight" min="0.0" max="1.0" init="1.0"/>
    <float name="m_LowriderLeanAccelModifier" min="0.0" max="10.0" init="1.0"/>

    <s32 name="m_numSeatsOverride" init="-1"/>
  
  </structdef>
	
  <structdef type="CVehicleModelInfo::InitDataList">
    <string name="m_residentTxd" type="atString"/>
    <array name="m_residentAnims" type="atArray">
      <string type="atString"/>
    </array>
    <array name="m_InitDatas" type="atArray">
      <struct type="CVehicleModelInfo::InitData"/>
    </array>
    <array name="m_txdRelationships" type="atArray">
      <struct type="CTxdRelationship"/>
    </array>
  </structdef>
  
</ParserSchema>
