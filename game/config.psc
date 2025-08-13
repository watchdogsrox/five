<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<enumdef type="ConfigThreadPriority">
	<enumval name="PRIO_INHERIT" value="-1000"/>
	<enumval name="PRIO_IDLE" value="-15"/>
	<enumval name="PRIO_LOWEST" value="-2"/>
	<enumval name="PRIO_BELOW_NORMAL" value="-1"/>
	<enumval name="PRIO_NORMAL" value="0"/>
	<enumval name="PRIO_ABOVE_NORMAL" value="1"/>
	<enumval name="PRIO_HIGHEST" value="2"/>
	<enumval name="PRIO_TIME_CRITICAL" value="15"/>
</enumdef>

<enumdef type="ConfigBool">
	<enumval name="CB_INHERIT" value="-1000"/>
	<enumval name="CB_FALSE" value="0"/>
	<enumval name="CB_TRUE" value="1"/>
</enumdef>

<const name="NUM_LINK_DENSITIES" value="16"/>

<structdef type="CPopulationConfig">
	<int name="m_ScenarioPedsMultiplier_Base" init="-1"/>
	<int name="m_ScenarioPedsMultiplier" init="-1"/>
	<int name="m_AmbientPedsMultiplier_Base" init="-1"/>
	<int name="m_AmbientPedsMultiplier" init="-1"/>
	<int name="m_MaxTotalPeds_Base" init="-1"/>
	<int name="m_MaxTotalPeds" init="-1"/>
	<int name="m_PedMemoryMultiplier" init="-1"/>
	<int name="m_PedsForVehicles_Base" init="-1"/>
	<int name="m_PedsForVehicles" init="-1"/>
	<int name="m_VehicleTimesliceMaxUpdatesPerFrame_Base" init="-1"/>
	<int name="m_VehicleTimesliceMaxUpdatesPerFrame" init="-1"/>
	<int name="m_VehicleAmbientDensityMultiplier_Base" init="-1"/>
	<int name="m_VehicleAmbientDensityMultiplier" init="-1"/>
	<int name="m_VehicleMemoryMultiplier" init="-1"/>
	<int name="m_VehicleParkedDensityMultiplier_Base" init="-1"/>
	<int name="m_VehicleParkedDensityMultiplier" init="-1"/>
	<int name="m_VehicleLowPrioParkedDensityMultiplier_Base" init="-1"/>
	<int name="m_VehicleLowPrioParkedDensityMultiplier" init="-1"/>
	<int name="m_VehicleUpperLimit_Base" init="-1"/>
	<int name="m_VehicleUpperLimit" init="-1"/>
	<int name="m_VehicleUpperLimitMP" init="-1"/>
	<int name="m_VehicleParkedUpperLimit_Base" init="-1"/>
	<int name="m_VehicleParkedUpperLimit" init="-1"/>
	<int name="m_VehicleKeyholeShapeInnerThickness_Base" init="-1"/>
	<int name="m_VehicleKeyholeShapeInnerThickness" init="-1"/>
	<int name="m_VehicleKeyholeShapeOuterThickness_Base" init="-1"/>
	<int name="m_VehicleKeyholeShapeOuterThickness" init="-1"/>
	<int name="m_VehicleKeyholeShapeInnerRadius_Base" init="-1"/>
	<int name="m_VehicleKeyholeShapeInnerRadius" init="-1"/>
	<int name="m_VehicleKeyholeShapeOuterRadius_Base" init="-1"/>
	<int name="m_VehicleKeyholeShapeOuterRadius" init="-1"/>
	<int name="m_VehicleKeyholeSideWallThickness_Base" init="-1"/>
	<int name="m_VehicleKeyholeSideWallThickness" init="-1"/>
	<int name="m_VehicleMaxCreationDistance_Base" init="-1"/>
	<int name="m_VehicleMaxCreationDistance" init="-1"/>
	<int name="m_VehicleMaxCreationDistanceOffscreen_Base" init="-1"/>
	<int name="m_VehicleMaxCreationDistanceOffscreen" init="-1"/>
	<int name="m_VehicleCullRange_Base" init="-1"/>
	<int name="m_VehicleCullRange" init="-1"/>
	<int name="m_VehicleCullRangeOnScreenScale_Base" init="-1"/>
	<int name="m_VehicleCullRangeOnScreenScale" init="-1"/>
	<int name="m_VehicleCullRangeOffScreen_Base" init="-1"/>
	<int name="m_VehicleCullRangeOffScreen" init="-1"/>
	<int name="m_DensityBasedRemovalRateScale_Base" init="-1"/>
	<int name="m_DensityBasedRemovalRateScale" init="-1"/>
	<int name="m_DensityBasedRemovalTargetHeadroom_Base" init="-1"/>
	<int name="m_DensityBasedRemovalTargetHeadroom" init="-1"/>
	<array name="m_VehicleSpacing_Base" type="member" size="NUM_LINK_DENSITIES">
		<int init="-1"/>
	</array>
	<array name="m_VehicleSpacing" type="member" size="NUM_LINK_DENSITIES">
		<int init="-1"/>
	</array>
	<array name="m_PlayerRoadDensityInc_Base" type="member" size="NUM_LINK_DENSITIES">
		<int init="-1"/>
	</array>
	<array name="m_PlayerRoadDensityInc" type="member" size="NUM_LINK_DENSITIES">
		<int init="-1"/>
	</array>
	<array name="m_NonPlayerRoadDensityDec_Base" type="member" size="NUM_LINK_DENSITIES">
		<int init="-1"/>
	</array>
	<array name="m_NonPlayerRoadDensityDec" type="member" size="NUM_LINK_DENSITIES">
		<int init="-1"/>
	</array>
	<int name="m_PlayersRoadScanDistance_Base" init="-1"/>
	<int name="m_PlayersRoadScanDistance" init="-1"/>
	<int name="m_VehiclePopulationFrameRate_Base" init="-1"/>
	<int name="m_VehiclePopulationFrameRate" init="-1"/>
	<int name="m_VehiclePopulationCyclesPerFrame_Base" init="-1"/>
	<int name="m_VehiclePopulationCyclesPerFrame" init="-1"/>
	<int name="m_VehiclePopulationFrameRateMP_Base" init="-1"/>
	<int name="m_VehiclePopulationFrameRateMP" init="-1"/>
	<int name="m_VehiclePopulationCyclesPerFrameMP_Base" init="-1"/>
	<int name="m_VehiclePopulationCyclesPerFrameMP" init="-1"/>
	<int name="m_PedPopulationFrameRate_Base" init="-1"/>
	<int name="m_PedPopulationFrameRate" init="-1"/>
	<int name="m_PedPopulationCyclesPerFrame_Base" init="-1"/>
	<int name="m_PedPopulationCyclesPerFrame" init="-1"/>
	<int name="m_PedPopulationFrameRateMP_Base" init="-1"/>
	<int name="m_PedPopulationFrameRateMP" init="-1"/>
	<int name="m_PedPopulationCyclesPerFrameMP_Base" init="-1"/>
	<int name="m_PedPopulationCyclesPerFrameMP" init="-1"/>
</structdef>

<structdef type="C2dEffectConfig">
	<int name="m_MaxAttrsAudio" init="-1"/>
	<int name="m_MaxAttrsBuoyancy" init="-1"/>
	<int name="m_MaxAttrsDecal" init="-1"/>
	<int name="m_MaxAttrsExplosion" init="-1"/>
	<int name="m_MaxAttrsLadder" init="-1"/>
	<int name="m_MaxAttrsLightShaft" init="-1"/>
	<int name="m_MaxAttrsParticle" init="-1"/>
	<int name="m_MaxAttrsProcObj" init="-1"/>
	<int name="m_MaxAttrsScrollBar" init="-1"/>
	<int name="m_MaxAttrsSpawnPoint" init="-1"/>
	<int name="m_MaxAttrsWindDisturbance" init="-1"/>
	<int name="m_MaxAttrsWorldPoint" init="-1"/>
	<int name="m_MaxEffects2d" init="-1"/>
	<int name="m_MaxEffectsWorld2d" init="-1"/>
</structdef>

<structdef type="CModelInfoConfig">
	<string name="m_defaultPlayerName" type="ConstString"/>
	<string name="m_defaultProloguePlayerName" type="ConstString"/>
	<int name="m_MaxBaseModelInfos" init="-1"/>
	<int name="m_MaxCompEntityModelInfos" init="-1"/>
	<int name="m_MaxMloInstances" init="-1"/>
	<int name="m_MaxMloModelInfos" init="-1"/>
	<int name="m_MaxPedModelInfos" init="-1"/>
	<int name="m_MaxTimeModelInfos" init="-1"/>
	<int name="m_MaxVehicleModelInfos" init="-1"/>
	<int name="m_MaxWeaponModelInfos" init="-1"/>
  <int name="m_MaxExtraPedModelInfos" init="-1"/>
  <int name="m_MaxExtraVehicleModelInfos" init="-1"/>
  <int name="m_MaxExtraWeaponModelInfos" init="-1"/>
</structdef>

<structdef type="CExtensionConfig">
	<int name="m_MaxDoorExtensions" init="-1"/>
	<int name="m_MaxLightExtensions" init="-1"/>
	<int name="m_MaxSpawnPointOverrideExtensions" init="-1"/>
	<int name="m_MaxExpressionExtensions" init="-1"/>
  <int name="m_MaxClothCollisionsExtensions" init="-1"/>
</structdef>

<structdef type="CConfigStreamingEngine">
	<int name="m_ArchiveCount" init="-1"/>
	<int name="m_PhysicalStreamingBuffer" init="-1"/>
	<int name="m_VirtualStreamingBuffer" init="-1"/>
</structdef>

<structdef type="CConfigOnlineServices">
	<string name="m_RosTitleName" type="ConstString"/>
	<int name="m_RosTitleVersion" init="0"/>
	<int name="m_RosScVersion" init="0"/>
	<int name="m_SteamAppId" init="0"/>
	<string name="m_TitleDirectoryName" type="ConstString"/>
	<string name="m_MultiplayerSessionTemplateName" type="ConstString"/>
  <string name="m_ScAuthTitleId" type="ConstString"/>
</structdef>

<structdef type="CConfigUGCDescriptions">
  <int name="m_MaxDescriptionLength" init="-1"/>
  <int name="m_MaxNumDescriptions" init="-1"/>
  <int name="m_SizeOfDescriptionBuffer" init="-1"/>
</structdef>

<structdef type="CBroadcastDataEntry">
	<int name="m_SizeOfData" init="-1"/>
	<int name="m_MaxParticipants" init="-1"/>
	<int name="m_NumInstances" init="-1"/>
</structdef>

<structdef type="CConfigNetScriptBroadcastData">
	<array name="m_HostBroadcastData" type="atFixedArray" size="10">
		<struct type="CBroadcastDataEntry"/>
	</array>
	<array name="m_PlayerBroadcastData" type="atFixedArray" size="10">
		<struct type="CBroadcastDataEntry"/>
	</array>
</structdef>

<structdef type="CScriptStackSizeDataEntry">
	<string name="m_StackName" type="atHashValue"/>
	<int name="m_SizeOfStack"/>
	<int name="m_NumberOfStacksOfThisSize"/>
</structdef>

<structdef type="CConfigScriptStackSizes">
	<array name="m_StackSizeData" type="atArray">
		<struct type="CScriptStackSizeDataEntry"/>
	</array>
</structdef>


<structdef type="CScriptTextLines">
  <string name="m_NameOfScriptTextLine" type="atHashValue"/>
  <int name="m_MaximumNumber"/>
</structdef>

<structdef type="CConfigScriptTextLines">
  <array name="m_ArrayOfMaximumTextLines" type="atArray">
    <struct type="CScriptTextLines"/>
  </array>
</structdef>


<structdef type="CScriptResourceExpectedMaximum">
  <string name="m_ResourceTypeName" type="atHashValue"/>
  <int name="m_ExpectedMaximum"/>
</structdef>

<structdef type="CConfigScriptResourceExpectedMaximums">
  <array name="m_ExpectedMaximumsArray" type="atArray">
    <struct type="CScriptResourceExpectedMaximum"/>
  </array>
</structdef>

<structdef type="CThreadSetup">
  <enum name="m_Priority" type="ConfigThreadPriority" init="-1000"/>
  <array name="m_CpuAffinity" type="atArray">
    <int/>
  </array>
</structdef>

<structdef type="CConfigMediaTranscoding">
	<int name="m_TranscodingSmallObjectBuffer" init="-1"/>
	<int name="m_TranscodingSmallObjectMaxPointers" init="-1"/>
	<int name="m_TranscodingBuffer" init="-1"/>
	<int name="m_TranscodingMaxPointers" init="-1"/>
</structdef>

<structdef type="CGameConfig" base="fwConfig">
	<struct name="m_ConfigPopulation" type="CPopulationConfig"/>
	<struct name="m_Config2dEffects" type="C2dEffectConfig"/>
	<struct name="m_ConfigModelInfo" type="CModelInfoConfig"/>
	<struct name="m_ConfigExtensions" type="CExtensionConfig"/>
	<struct name="m_ConfigStreamingEngine" type="CConfigStreamingEngine"/>
  <struct name="m_ConfigOnlineServices" type="CConfigOnlineServices"/>
  <struct name="m_ConfigUGCDescriptions" type="CConfigUGCDescriptions"/>
  <struct name="m_ConfigNetScriptBroadcastData" type="CConfigNetScriptBroadcastData"/>
	<struct name="m_ConfigScriptStackSizes" type="CConfigScriptStackSizes"/>
  <struct name="m_ConfigScriptResourceExpectedMaximums" type="CConfigScriptResourceExpectedMaximums"/>
  <struct name="m_ConfigScriptTextLines" type="CConfigScriptTextLines"/>
  <struct name="m_ConfigMediaTranscoding" type="CConfigMediaTranscoding"/>
  <enum name="m_UseVehicleBurnoutTexture" type="ConfigBool" init="-1000"/>
	<enum name="m_AllowCrouchedMovement" type="ConfigBool" init="-1000"/>
	<enum name="m_AllowParachuting" type="ConfigBool" init="-1000"/>
	<enum name="m_AllowStealthMovement" type="ConfigBool" init="-1000"/>
	<string name="m_DebugScriptsPath" type="ConstString"/>
	<map name="m_Threads" key="atHashString" type="atMap">
		<struct type="CThreadSetup" />
	</map>
</structdef>

</ParserSchema>