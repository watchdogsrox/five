<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<enumdef type="ePopGroupFlags">
	<enumval name="POPGROUP_IS_GANG"/>
	<enumval name="POPGROUP_AMBIENT"/>
	<enumval name="POPGROUP_SCENARIO"/>
	<enumval name="POPGROUP_RARE"/>
	<enumval name="POPGROUP_NETWORK_COMMON"/>
	<enumval name="POPGROUP_AERIAL"/>
	<enumval name="POPGROUP_AQUATIC"/>
	<enumval name="POPGROUP_WILDLIFE"/>
	<enumval name="POPGROUP_IN_VEHICLE"/>
</enumdef>

<structdef type="CPopModelAndVariations">
	<string name="m_Name" type="atHashString" ui_key="true"/>
	<pointer type="CAmbientModelVariations" name="m_Variations" policy="owner"/>
</structdef>

<structdef type="CPopulationGroup">
	<string name="m_Name" type="atHashString" ui_key="true"/>
	<array name="m_models" type="atArray">
		<struct type="CPopModelAndVariations"/> 
	</array>
	<bitset name="m_flags" type="fixed32" init="POPGROUP_AMBIENT|POPGROUP_SCENARIO" numBits="32" values="ePopGroupFlags"/>
</structdef>

<structdef type="CPopGroupList" onPostLoad="PostLoad">
	<!--array name="m_popGroups" type="atArray">
		<struct type="CPopGroup"/>
	</array-->
	<array name="m_pedGroups" type="atArray">
		<struct type="CPopulationGroup"/>
	</array>
	<array name="m_vehGroups" type="atArray">
		<struct type="CPopulationGroup"/>
	</array>
	<array name="m_wildlifeHabitats" type="atArray">
		<string type="atHashString"/>
	</array>
</structdef>

<structdef type="CPopGroupPercentage">
	<string name="m_GroupName" type="atHashString" ui_key="true"/>
	<u32 name="m_percentage" init="100"/>
</structdef>

<structdef type="CPopAllocation">
	<u8 name="m_nMaxNumAmbientPeds" init="25"/>
	<u8 name="m_nMaxNumScenarioPeds" init="15"/>
	<u8 name="m_nPercentageOfMaxCars" init="100"/>
	<u8 name="m_nMaxNumParkedCars" init="20"/>
	<u8 name="m_nMaxNumLowPrioParkedCars" init="0"/>
	<u8 name="m_nPercCopsCars" init="3"/>
	<u8 name="m_nPercCopsPeds" init="0"/>
	<u8 name="m_nMaxScenarioPedModels" init="2"/>
	<u8 name="m_nMaxScenarioVehicleModels" init="2"/>
	<u8 name="m_nMaxPreassignedParkedCars" init="1"/>
	<array name="m_pedGroupPercentages" type="atArray">
	<struct type="CPopGroupPercentage"/>
	</array>
	<array name="m_vehGroupPercentages" type="atArray">
	<struct type="CPopGroupPercentage"/>
	</array>
</structdef>

<const name="CPopSchedule::POPCYCLE_WEEKSUBDIVISIONS*CPopSchedule::POPCYCLE_DAYSUBDIVISIONS" value="12"/>

<structdef type="CPopSchedule">
	<string name="m_Name" type="atHashString" ui_key="true"/>
	<array name="m_popAllocation" type="member" size="CPopSchedule::POPCYCLE_WEEKSUBDIVISIONS*CPopSchedule::POPCYCLE_DAYSUBDIVISIONS">
		<struct type="CPopAllocation"/>
	</array>
</structdef>

<structdef type="CPopScheduleList" onPostLoad="PostLoad">
	<array name="m_schedules" type="atArray">
		<struct type="CPopSchedule"/>
	</array>
</structdef>

<enumdef type="eZoneScumminess">
	<enumval name="SCUMMINESS_POSH"/>
	<enumval name="SCUMMINESS_NICE"/>
	<enumval name="SCUMMINESS_ABOVE_AVERAGE"/>
	<enumval name="SCUMMINESS_BELOW_AVERAGE"/>
	<enumval name="SCUMMINESS_CRAP"/>
	<enumval name="SCUMMINESS_SCUM"/>
</enumdef>

<enumdef type="eLawResponseTime">
	<enumval name="LAW_RESPONSE_DELAY_SLOW"/>
	<enumval name="LAW_RESPONSE_DELAY_MEDIUM"/>
	<enumval name="LAW_RESPONSE_DELAY_FAST"/>
</enumdef>

<enumdef type="eZoneSpecialAttributeType">
	<enumval name="SPECIAL_NONE"/>
	<enumval name="SPECIAL_AIRPORT"/>
</enumdef>

<structdef type="CPopZoneData::sZone" simple="true">
    <string name="zoneName" type="ConstString" ui_key="true"/>
    <string name="spName" type="atHashString"/>
    <string name="mpName" type="atHashString"/>
    <string name="vfxRegion" type="atHashString" init="VFXREGIONINFO_DEFAULT"/>
    <u8 name="plantsMgrTxdIdx" init="7"/>

    <enum name="scumminessLevel" type="eZoneScumminess" init="SCUMMINESS_ABOVE_AVERAGE"/>
    <enum name="lawResponseTime" type="eLawResponseTime" init="LAW_RESPONSE_DELAY_MEDIUM"/>
    <enum name="lawResponseType" type="eZoneVehicleResponseType" init="VEHICLE_RESPONSE_DEFAULT"/>
    <enum name="specialZoneAttribute" type="eZoneSpecialAttributeType" init="SPECIAL_NONE"/>

    <float name="vehDirtMin" init="0.0f"/>
    <float name="vehDirtMax" init="0.0f"/>
    <float name="vehDirtGrowScale" init="0.25f"/>
    <float name="pedDirtMin" init="0.0f"/>
    <float name="pedDirtMax" init="0.0f"/>
    <u8 name="dirtRed" init="54"/>
    <u8 name="dirtGreen" init="54"/>
    <u8 name="dirtBlue" init="54"/>

    <float name="popRangeScaleStart" init="0.0f"/>
    <float name="popRangeScaleEnd" init="0.0f"/>
    <float name="popRangeMultiplier" init="1.0f"/>
    <float name="popBaseRangeScale" init="1.0f"/>

    <s32 name="mpGangTerritoryIndex" init="-1"/>

    <bool name="allowScenarioWeatherExits" init="true"/>
    <bool name="allowHurryAwayToLeavePavement" init="false"/>
</structdef>

<structdef type="CPopZoneData" simple="true" onPostSet="OnPostSetCallback">
	<array name="zones" type="atArray">
		<struct type="CPopZoneData::sZone"/>
	</array>
</structdef>

</ParserSchema>
