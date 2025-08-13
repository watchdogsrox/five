<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<!-- This enum list needs to match the enum list in DispatchEnums.h -->				
<enumdef type="DispatchType">
    <enumval name="DT_Invalid"/>
    <enumval name="DT_PoliceAutomobile"/>
    <enumval name="DT_PoliceHelicopter"/>	
    <enumval name="DT_FireDepartment"/>
    <enumval name="DT_SwatAutomobile"/>
    <enumval name="DT_AmbulanceDepartment"/>
    <enumval name="DT_PoliceRiders"/>
	<enumval name="DT_PoliceVehicleRequest"/>
	<enumval name="DT_PoliceRoadBlock"/>
	<enumval name="DT_PoliceAutomobileWaitPulledOver"/>
	<enumval name="DT_PoliceAutomobileWaitCruising"/>
	<enumval name="DT_Gangs"/>
	<enumval name="DT_SwatHelicopter"/>	
	<enumval name="DT_PoliceBoat"/>
	<enumval name="DT_ArmyVehicle"/>
	<enumval name="DT_BikerBackup"/>
	<enumval name="DT_Assassins"/>
	<enumval name="DT_Max"/>
</enumdef>

<!-- This enum list needs to match the enum list in DispatchEnums.h -->
<enumdef type="DispatchAssassinLevel">
  <enumval name="AL_Invalid"/>
  <enumval name="AL_Low"/>
  <enumval name="AL_Med"/>
  <enumval name="AL_High"/>
  <enumval name="AL_Max"/>
</enumdef>

<!-- This enum list needs to match the enum list in DispatchData.h -->
<enumdef type="eZoneVehicleResponseType">
	<enumval name="VEHICLE_RESPONSE_DEFAULT"/>
	<enumval name="VEHICLE_RESPONSE_COUNTRYSIDE"/>
	<enumval name="VEHICLE_RESPONSE_ARMY_BASE"/>
	<enumval name="NUM_VEHICLE_RESPONSE_TYPES"/>
</enumdef>

<!-- This enum list needs to match the enum list in DispatchData.h -->
<enumdef type="eGameVehicleResponseType">
<enumval name="VEHICLE_RESPONSE_GAMETYPE_SP"/>
<enumval name="VEHICLE_RESPONSE_GAMETYPE_MP"/>
<enumval name="VEHICLE_RESPONSE_GAMETYPE_ALL"/>
</enumdef>

<structdef type="CConditionalVehicleSet">
	<enum name="m_ZoneType" type="eZoneVehicleResponseType" init="VEHICLE_RESPONSE_DEFAULT"/>
	<enum name="m_GameType" type="eGameVehicleResponseType" init="VEHICLE_RESPONSE_GAMETYPE_ALL"/>
	<bool name="m_IsMutuallyExclusive" init="false"/>
	<array name="m_VehicleModels" type="atArray">
		<string type="atHashString"/>
	</array>
	<array name="m_PedModels" type="atArray">
		<string type="atHashString"/>
	</array>
	<array name="m_ObjectModels" type="atArray">
		<string type="atHashString"/>
	</array>
	<array name="m_ClipSets" type="atArray">
		<string type="atHashString"/>
	</array>
</structdef>

<structdef type="CVehicleSet">
	<string name="m_Name" type="atHashString"/>
	<array name="m_ConditionalVehicleSets" type="atArray">
		<struct type="CConditionalVehicleSet"/>
	</array>
</structdef>

<structdef type="DispatchModelVariation">
  <enum name="m_Component" type="ePedVarComp" init="PV_COMP_INVALID"/>
  <u32 name="m_DrawableId" min="0" max="100" step="1" init="0"/>
  <u8 name="m_MinWantedLevel" min="0" max="6" step="1" init="0"/>
  <float name="m_Armour" min="0.0" max="100.0" step="0.1" init="0.0"/>
</structdef>
  
<structdef type="sDispatchModelVariations">
	<array name="m_ModelVariations" type="atArray">
		<struct type="DispatchModelVariation"/>
	</array>
</structdef>

<structdef type="CDispatchResponse">
    <enum name="m_DispatchType" type="DispatchType"/>
	<s8 name="m_NumPedsToSpawn"/>
	<bool name="m_UseOneVehicleSet" init="false"/>
    <array name="m_DispatchVehicleSets" type="atArray">
        <string type="atHashString"/>
    </array>
</structdef>

<structdef type="CWantedResponse">
    <array name="m_DispatchServices" type="atArray">
        <struct type="CDispatchResponse"/>
    </array>
</structdef>

<const name="CDispatchData::NUM_DISPATCH_SERVICES" value="16"/>

<structdef type="CDispatchData">
	<u32 name="m_ParoleDuration"/>
	<float name="m_InHeliRadiusMultiplier"/>
  <float name="m_ImmediateDetectionRange"/>
  <float name="m_OnScreenImmediateDetectionRange"/>
	<array name="m_LawSpawnDelayMin" type="atArray">
		<float/>
	</array>
	<array name="m_LawSpawnDelayMax" type="atArray">
		<float/>
	</array>
  <array name="m_AmbulanceSpawnDelayMin" type="atArray">
    <float/>
  </array>
  <array name="m_AmbulanceSpawnDelayMax" type="atArray">
    <float/>
  </array>
  <array name="m_FireSpawnDelayMin" type="atArray">
    <float/>
  </array>
  <array name="m_FireSpawnDelayMax" type="atArray">
    <float/>
  </array>
    <array name="m_DispatchOrderDistances" type="atFixedArray" size="CDispatchData::NUM_DISPATCH_SERVICES">
        <struct type="CDispatchData::sDispatchServiceOrderDistances"/>
    </array>
    <array name="m_WantedLevelThresholds" type="atArray">
        <int/>
    </array>
    <array name="m_SingleplayerWantedLevelRadius" type="atArray">
        <float/>
    </array>
    <array name="m_MultiplayerWantedLevelRadius" type="atArray">
        <float/>
	</array>
	<array name="m_HiddenEvasionTimes" type="atArray">
		<u32/>
	</array>
    <array name="m_VehicleSets" type="atArray">
        <struct type="CVehicleSet"/>
    </array>
    <map name="m_PedVariations" type="atBinaryMap" key="atHashString">
      <struct type="sDispatchModelVariations"/>
    </map>
    <array name="m_WantedResponses" type="atArray">
        <struct type="CWantedResponse"/>
    </array>
    <array name="m_EmergencyResponses" type="atArray">
        <struct type="CDispatchResponse"/>
    </array>
	<array name="m_MultiplayerHiddenEvasionTimes" type="atArray">
    	<u32/>
    </array>
</structdef>

<structdef type="CDispatchData::sDispatchServiceOrderDistances">
    <enum name="m_DispatchType" type="DispatchType"/>
    <float name="m_OnFootOrderDistance"/>
    <float name="m_MinInVehicleOrderDistance"/>
    <float name="m_MaxInVehicleOrderDistance"/>
</structdef>

</ParserSchema>
