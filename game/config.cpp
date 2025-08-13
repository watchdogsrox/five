#include "config.h"
#include "config_parser.h"

//-----------------------------------------------------------------------------

// Helper function for merging thread priority overrides.
/* static void sMergeThreadPriority(int &priInOut, int priMergeIn)
{
	if(priMergeIn != CGameConfig::PRIO_INHERIT)
	{
		priInOut = priMergeIn;
	}
} */

static void sMergeBool(int &inOut, int mergeIn)
{
	if(mergeIn != CGameConfig::PRIO_INHERIT)
	{
		inOut = mergeIn;
	}
}


static void sMergeNonNegInt(int &inOut, int mergeIn)
{
	if(mergeIn >= 0)
	{
		inOut = mergeIn;
	}
}

static void sMergeString(ConstString &inOut, const ConstString &mergeIn)
{
    ConstString empty("");
    if (mergeIn != empty)
    {
        inOut = mergeIn;
    }
}

//-----------------------------------------------------------------------------

CPopulationConfig::CPopulationConfig()
		: m_ScenarioPedsMultiplier_Base(0)
		, m_ScenarioPedsMultiplier(0)
		, m_AmbientPedsMultiplier_Base(0)
		, m_AmbientPedsMultiplier(0)
		, m_MaxTotalPeds_Base(0)
		, m_MaxTotalPeds(0)
		, m_PedMemoryMultiplier(0)
		, m_PedsForVehicles_Base(0)
		, m_PedsForVehicles(0)
		, m_VehicleTimesliceMaxUpdatesPerFrame_Base(0)
		, m_VehicleTimesliceMaxUpdatesPerFrame(0)
		, m_VehicleAmbientDensityMultiplier_Base(0)
		, m_VehicleAmbientDensityMultiplier(0)
		, m_VehicleMemoryMultiplier(0)
		, m_VehicleParkedDensityMultiplier_Base(0)
		, m_VehicleParkedDensityMultiplier(0)
		, m_VehicleLowPrioParkedDensityMultiplier_Base(0)
		, m_VehicleLowPrioParkedDensityMultiplier(0)
		, m_VehicleUpperLimit_Base(0)
		, m_VehicleUpperLimit(0)
		, m_VehicleUpperLimitMP(0)
		, m_VehicleParkedUpperLimit_Base(0)
		, m_VehicleParkedUpperLimit(0)
		, m_VehicleKeyholeShapeInnerThickness_Base(0)
		, m_VehicleKeyholeShapeInnerThickness(0)
		, m_VehicleKeyholeShapeOuterThickness_Base(0)
		, m_VehicleKeyholeShapeOuterThickness(0)
		, m_VehicleKeyholeShapeInnerRadius_Base(0)
		, m_VehicleKeyholeShapeInnerRadius(0)
		, m_VehicleKeyholeShapeOuterRadius_Base(0)
		, m_VehicleKeyholeShapeOuterRadius(0)
		, m_VehicleKeyholeSideWallThickness_Base(0)
		, m_VehicleKeyholeSideWallThickness(0)
		, m_VehicleMaxCreationDistance_Base(0)
		, m_VehicleMaxCreationDistance(0)
		, m_VehicleMaxCreationDistanceOffscreen_Base(0)
		, m_VehicleMaxCreationDistanceOffscreen(0)
		, m_VehicleCullRange_Base(0)
		, m_VehicleCullRange(0)
		, m_VehicleCullRangeOnScreenScale_Base(0)
		, m_VehicleCullRangeOnScreenScale(0)
		, m_VehicleCullRangeOffScreen_Base(0)
		, m_VehicleCullRangeOffScreen(0)
		, m_DensityBasedRemovalRateScale_Base(0)
		, m_DensityBasedRemovalRateScale(0)
		, m_DensityBasedRemovalTargetHeadroom_Base(0)
		, m_DensityBasedRemovalTargetHeadroom(0)
		, m_PlayersRoadScanDistance_Base(0)
		, m_PlayersRoadScanDistance(0)
		, m_VehiclePopulationFrameRate_Base(0)
		, m_VehiclePopulationFrameRate(0)
		, m_VehiclePopulationCyclesPerFrame_Base(0)
		, m_VehiclePopulationCyclesPerFrame(0)
		, m_VehiclePopulationFrameRateMP_Base(0)
		, m_VehiclePopulationFrameRateMP(0)
		, m_VehiclePopulationCyclesPerFrameMP_Base(0)
		, m_VehiclePopulationCyclesPerFrameMP(0)
		, m_PedPopulationFrameRate_Base(0)
		, m_PedPopulationFrameRate(0)
		, m_PedPopulationCyclesPerFrame_Base(0)
		, m_PedPopulationCyclesPerFrame(0)
		, m_PedPopulationFrameRateMP_Base(0)
		, m_PedPopulationFrameRateMP(0)
		, m_PedPopulationCyclesPerFrameMP_Base(0)
		, m_PedPopulationCyclesPerFrameMP(0)
{
	memset(m_VehicleSpacing_Base, 0, sizeof(int) * NUM_LINK_DENSITIES);
	memset(m_VehicleSpacing, 0, sizeof(int) * NUM_LINK_DENSITIES);
	memset(m_PlayerRoadDensityInc_Base, 0, sizeof(int) * NUM_LINK_DENSITIES);
	memset(m_PlayerRoadDensityInc, 0, sizeof(int) * NUM_LINK_DENSITIES);
	memset(m_NonPlayerRoadDensityDec_Base, 0, sizeof(int) * NUM_LINK_DENSITIES);
	memset(m_NonPlayerRoadDensityDec, 0, sizeof(int) * NUM_LINK_DENSITIES);
}

void CPopulationConfig::MergeData(const CPopulationConfig& mergeIn)
{
	sMergeNonNegInt(m_ScenarioPedsMultiplier_Base, mergeIn.m_ScenarioPedsMultiplier_Base);
	sMergeNonNegInt(m_ScenarioPedsMultiplier, mergeIn.m_ScenarioPedsMultiplier);
	sMergeNonNegInt(m_AmbientPedsMultiplier_Base, mergeIn.m_AmbientPedsMultiplier_Base);
	sMergeNonNegInt(m_AmbientPedsMultiplier, mergeIn.m_AmbientPedsMultiplier);
	sMergeNonNegInt(m_MaxTotalPeds_Base, mergeIn.m_MaxTotalPeds_Base);
	sMergeNonNegInt(m_MaxTotalPeds, mergeIn.m_MaxTotalPeds);
	sMergeNonNegInt(m_PedMemoryMultiplier, mergeIn.m_PedMemoryMultiplier);
	sMergeNonNegInt(m_PedsForVehicles_Base, mergeIn.m_PedsForVehicles_Base);
	sMergeNonNegInt(m_PedsForVehicles, mergeIn.m_PedsForVehicles);
	sMergeNonNegInt(m_VehicleTimesliceMaxUpdatesPerFrame_Base, mergeIn.m_VehicleTimesliceMaxUpdatesPerFrame_Base);
	sMergeNonNegInt(m_VehicleTimesliceMaxUpdatesPerFrame, mergeIn.m_VehicleTimesliceMaxUpdatesPerFrame);
	sMergeNonNegInt(m_VehicleAmbientDensityMultiplier_Base, mergeIn.m_VehicleAmbientDensityMultiplier_Base);
	sMergeNonNegInt(m_VehicleAmbientDensityMultiplier, mergeIn.m_VehicleAmbientDensityMultiplier);
	sMergeNonNegInt(m_VehicleMemoryMultiplier, mergeIn.m_VehicleMemoryMultiplier);
	sMergeNonNegInt(m_VehicleParkedDensityMultiplier_Base, mergeIn.m_VehicleParkedDensityMultiplier_Base);
	sMergeNonNegInt(m_VehicleParkedDensityMultiplier, mergeIn.m_VehicleParkedDensityMultiplier);
	sMergeNonNegInt(m_VehicleLowPrioParkedDensityMultiplier_Base, mergeIn.m_VehicleLowPrioParkedDensityMultiplier_Base);
	sMergeNonNegInt(m_VehicleLowPrioParkedDensityMultiplier, mergeIn.m_VehicleLowPrioParkedDensityMultiplier);
	sMergeNonNegInt(m_VehicleUpperLimit_Base, mergeIn.m_VehicleUpperLimit_Base);
	sMergeNonNegInt(m_VehicleUpperLimit, mergeIn.m_VehicleUpperLimit);
	sMergeNonNegInt(m_VehicleUpperLimitMP, mergeIn.m_VehicleUpperLimitMP);
	sMergeNonNegInt(m_VehicleParkedUpperLimit_Base, mergeIn.m_VehicleParkedUpperLimit_Base);
	sMergeNonNegInt(m_VehicleParkedUpperLimit, mergeIn.m_VehicleParkedUpperLimit);
	sMergeNonNegInt(m_VehicleKeyholeShapeInnerThickness_Base, mergeIn.m_VehicleKeyholeShapeInnerThickness_Base);
	sMergeNonNegInt(m_VehicleKeyholeShapeInnerThickness, mergeIn.m_VehicleKeyholeShapeInnerThickness);
	sMergeNonNegInt(m_VehicleKeyholeShapeOuterThickness_Base, mergeIn.m_VehicleKeyholeShapeOuterThickness_Base);
	sMergeNonNegInt(m_VehicleKeyholeShapeOuterThickness, mergeIn.m_VehicleKeyholeShapeOuterThickness);
	sMergeNonNegInt(m_VehicleKeyholeShapeInnerRadius_Base, mergeIn.m_VehicleKeyholeShapeInnerRadius_Base);
	sMergeNonNegInt(m_VehicleKeyholeShapeInnerRadius, mergeIn.m_VehicleKeyholeShapeInnerRadius);
	sMergeNonNegInt(m_VehicleKeyholeShapeOuterRadius_Base, mergeIn.m_VehicleKeyholeShapeOuterRadius_Base);
	sMergeNonNegInt(m_VehicleKeyholeShapeOuterRadius, mergeIn.m_VehicleKeyholeShapeOuterRadius);
	sMergeNonNegInt(m_VehicleKeyholeSideWallThickness_Base, mergeIn.m_VehicleKeyholeSideWallThickness_Base);
	sMergeNonNegInt(m_VehicleKeyholeSideWallThickness, mergeIn.m_VehicleKeyholeSideWallThickness);
	sMergeNonNegInt(m_VehicleMaxCreationDistance_Base, mergeIn.m_VehicleMaxCreationDistance_Base);
	sMergeNonNegInt(m_VehicleMaxCreationDistance, mergeIn.m_VehicleMaxCreationDistance);
	sMergeNonNegInt(m_VehicleMaxCreationDistanceOffscreen_Base, mergeIn.m_VehicleMaxCreationDistanceOffscreen_Base);
	sMergeNonNegInt(m_VehicleMaxCreationDistanceOffscreen, mergeIn.m_VehicleMaxCreationDistanceOffscreen);
	sMergeNonNegInt(m_VehicleCullRange_Base, mergeIn.m_VehicleCullRange_Base);
	sMergeNonNegInt(m_VehicleCullRange, mergeIn.m_VehicleCullRange);
	sMergeNonNegInt(m_VehicleCullRangeOnScreenScale_Base, mergeIn.m_VehicleCullRangeOnScreenScale_Base);
	sMergeNonNegInt(m_VehicleCullRangeOnScreenScale, mergeIn.m_VehicleCullRangeOnScreenScale);
	sMergeNonNegInt(m_VehicleCullRangeOffScreen_Base, mergeIn.m_VehicleCullRangeOffScreen_Base);
	sMergeNonNegInt(m_VehicleCullRangeOffScreen, mergeIn.m_VehicleCullRangeOffScreen);
	sMergeNonNegInt(m_DensityBasedRemovalRateScale_Base, mergeIn.m_DensityBasedRemovalRateScale_Base);
	sMergeNonNegInt(m_DensityBasedRemovalRateScale, mergeIn.m_DensityBasedRemovalRateScale);
	sMergeNonNegInt(m_DensityBasedRemovalTargetHeadroom_Base, mergeIn.m_DensityBasedRemovalTargetHeadroom_Base);
	sMergeNonNegInt(m_DensityBasedRemovalTargetHeadroom, mergeIn.m_DensityBasedRemovalTargetHeadroom);

	for (s32 i = 0; i < NUM_LINK_DENSITIES; ++i)
	{
		sMergeNonNegInt(m_VehicleSpacing_Base[i], mergeIn.m_VehicleSpacing_Base[i]);
		sMergeNonNegInt(m_VehicleSpacing[i], mergeIn.m_VehicleSpacing[i]);
	}

	sMergeNonNegInt(m_PlayersRoadScanDistance_Base, mergeIn.m_PlayersRoadScanDistance_Base);
	sMergeNonNegInt(m_PlayersRoadScanDistance, mergeIn.m_PlayersRoadScanDistance);

	for (s32 i = 0; i < NUM_LINK_DENSITIES; ++i)
	{
		sMergeNonNegInt(m_PlayerRoadDensityInc_Base[i], mergeIn.m_PlayerRoadDensityInc_Base[i]);
		sMergeNonNegInt(m_PlayerRoadDensityInc[i], mergeIn.m_PlayerRoadDensityInc[i]);
		sMergeNonNegInt(m_NonPlayerRoadDensityDec_Base[i], mergeIn.m_NonPlayerRoadDensityDec_Base[i]);
		sMergeNonNegInt(m_NonPlayerRoadDensityDec[i], mergeIn.m_NonPlayerRoadDensityDec[i]);
	}

	sMergeNonNegInt(m_VehiclePopulationFrameRate_Base, mergeIn.m_VehiclePopulationFrameRate_Base);
	sMergeNonNegInt(m_VehiclePopulationFrameRate, mergeIn.m_VehiclePopulationFrameRate);
	sMergeNonNegInt(m_VehiclePopulationCyclesPerFrame_Base, mergeIn.m_VehiclePopulationCyclesPerFrame_Base);
	sMergeNonNegInt(m_VehiclePopulationCyclesPerFrame, mergeIn.m_VehiclePopulationCyclesPerFrame);
	sMergeNonNegInt(m_VehiclePopulationFrameRateMP_Base, mergeIn.m_VehiclePopulationFrameRateMP_Base);
	sMergeNonNegInt(m_VehiclePopulationFrameRateMP, mergeIn.m_VehiclePopulationFrameRateMP);
	sMergeNonNegInt(m_VehiclePopulationCyclesPerFrameMP_Base, mergeIn.m_VehiclePopulationCyclesPerFrameMP_Base);
	sMergeNonNegInt(m_VehiclePopulationCyclesPerFrameMP, mergeIn.m_VehiclePopulationCyclesPerFrameMP);
	sMergeNonNegInt(m_PedPopulationFrameRate_Base, mergeIn.m_PedPopulationFrameRate_Base);
	sMergeNonNegInt(m_PedPopulationFrameRate, mergeIn.m_PedPopulationFrameRate);
	sMergeNonNegInt(m_PedPopulationCyclesPerFrame_Base, mergeIn.m_PedPopulationCyclesPerFrame_Base);
	sMergeNonNegInt(m_PedPopulationCyclesPerFrame, mergeIn.m_PedPopulationCyclesPerFrame);
	sMergeNonNegInt(m_PedPopulationFrameRateMP_Base, mergeIn.m_PedPopulationFrameRateMP_Base);
	sMergeNonNegInt(m_PedPopulationFrameRateMP, mergeIn.m_PedPopulationFrameRateMP);
	sMergeNonNegInt(m_PedPopulationCyclesPerFrameMP_Base, mergeIn.m_PedPopulationCyclesPerFrameMP_Base);
	sMergeNonNegInt(m_PedPopulationCyclesPerFrameMP, mergeIn.m_PedPopulationCyclesPerFrameMP);
}

//-----------------------------------------------------------------------------

C2dEffectConfig::C2dEffectConfig()
		: m_MaxAttrsAudio(280)
		, m_MaxAttrsBuoyancy(100)
		, m_MaxAttrsDecal(50)
		, m_MaxAttrsExplosion(50)
		, m_MaxAttrsLadder(122)
		, m_MaxAttrsLightShaft(600)
		, m_MaxAttrsParticle(2500)
		, m_MaxAttrsProcObj(100)
		, m_MaxAttrsScrollBar(35)
		, m_MaxAttrsSpawnPoint(10900)
		, m_MaxAttrsWindDisturbance(50)
		, m_MaxAttrsWorldPoint(250)
		, m_MaxEffects2d(13250)
		, m_MaxEffectsWorld2d(10900)
		, m_MaxAttrsAudioCollision(2000)
{
}


void C2dEffectConfig::MergeData(const C2dEffectConfig& mergeIn)
{
	sMergeNonNegInt(m_MaxAttrsAudio,		mergeIn.m_MaxAttrsAudio);
	sMergeNonNegInt(m_MaxAttrsBuoyancy,		mergeIn.m_MaxAttrsBuoyancy);
	sMergeNonNegInt(m_MaxAttrsDecal,		mergeIn.m_MaxAttrsDecal);
	sMergeNonNegInt(m_MaxAttrsExplosion,	mergeIn.m_MaxAttrsExplosion);
	sMergeNonNegInt(m_MaxAttrsLadder,		mergeIn.m_MaxAttrsLadder);
	sMergeNonNegInt(m_MaxAttrsLightShaft,	mergeIn.m_MaxAttrsLightShaft);
	sMergeNonNegInt(m_MaxAttrsParticle,		mergeIn.m_MaxAttrsParticle);
	sMergeNonNegInt(m_MaxAttrsProcObj,		mergeIn.m_MaxAttrsProcObj);
	sMergeNonNegInt(m_MaxAttrsScrollBar,	mergeIn.m_MaxAttrsScrollBar);
	sMergeNonNegInt(m_MaxAttrsSpawnPoint,	mergeIn.m_MaxAttrsSpawnPoint);
	sMergeNonNegInt(m_MaxAttrsWorldPoint,	mergeIn.m_MaxAttrsWorldPoint);
	sMergeNonNegInt(m_MaxEffects2d,			mergeIn.m_MaxEffects2d);
	sMergeNonNegInt(m_MaxEffectsWorld2d,	mergeIn.m_MaxEffectsWorld2d);
	sMergeNonNegInt(m_MaxAttrsAudioCollision, mergeIn.m_MaxAttrsAudioCollision);
}

//-----------------------------------------------------------------------------

CModelInfoConfig::CModelInfoConfig()
		: m_defaultPlayerName("")
		, m_defaultProloguePlayerName("")
		, m_MaxBaseModelInfos(2)
		, m_MaxCompEntityModelInfos(50)
		, m_MaxMloInstances(15000)
		, m_MaxMloModelInfos(220)
		, m_MaxPedModelInfos(350)
		, m_MaxTimeModelInfos(1800)
		, m_MaxVehicleModelInfos(256)
		, m_MaxWeaponModelInfos(100)
		, m_MaxExtraPedModelInfos(50)
		, m_MaxExtraVehicleModelInfos(50)
		, m_MaxExtraWeaponModelInfos(50)
{
}


void CModelInfoConfig::MergeData(const CModelInfoConfig& mergeIn)
{
	sMergeString(m_defaultPlayerName,			mergeIn.m_defaultPlayerName);
	sMergeString(m_defaultProloguePlayerName,   mergeIn.m_defaultProloguePlayerName);
	sMergeNonNegInt(m_MaxBaseModelInfos,		mergeIn.m_MaxBaseModelInfos);
	sMergeNonNegInt(m_MaxCompEntityModelInfos,	mergeIn.m_MaxCompEntityModelInfos);
	sMergeNonNegInt(m_MaxMloInstances,			mergeIn.m_MaxMloInstances);
	sMergeNonNegInt(m_MaxMloModelInfos,			mergeIn.m_MaxMloModelInfos);
	sMergeNonNegInt(m_MaxPedModelInfos,			mergeIn.m_MaxPedModelInfos);
	sMergeNonNegInt(m_MaxTimeModelInfos,		mergeIn.m_MaxTimeModelInfos);
	sMergeNonNegInt(m_MaxVehicleModelInfos,		mergeIn.m_MaxVehicleModelInfos);
	sMergeNonNegInt(m_MaxWeaponModelInfos,		mergeIn.m_MaxWeaponModelInfos);
	sMergeNonNegInt(m_MaxExtraPedModelInfos, mergeIn.m_MaxExtraPedModelInfos);
	sMergeNonNegInt(m_MaxExtraVehicleModelInfos, mergeIn.m_MaxExtraVehicleModelInfos);
	sMergeNonNegInt(m_MaxExtraWeaponModelInfos,	mergeIn.m_MaxExtraWeaponModelInfos);
}

//-----------------------------------------------------------------------------

CExtensionConfig::CExtensionConfig()
		: m_MaxDoorExtensions(30)
		, m_MaxLightExtensions(CONFIGURED_FROM_FILE)
		, m_MaxSpawnPointOverrideExtensions(20)
		, m_MaxExpressionExtensions(30)
		, m_MaxClothCollisionsExtensions(64)
{
}

void CExtensionConfig::MergeData(const CExtensionConfig& mergeIn)
{
	sMergeNonNegInt(m_MaxDoorExtensions,				mergeIn.m_MaxDoorExtensions);
	sMergeNonNegInt(m_MaxLightExtensions,				mergeIn.m_MaxLightExtensions);
	sMergeNonNegInt(m_MaxSpawnPointOverrideExtensions,	mergeIn.m_MaxSpawnPointOverrideExtensions);
	sMergeNonNegInt(m_MaxExpressionExtensions,			mergeIn.m_MaxExpressionExtensions);
	sMergeNonNegInt(m_MaxClothCollisionsExtensions,		mergeIn.m_MaxClothCollisionsExtensions);	
}

//-----------------------------------------------------------------------------

CConfigStreamingEngine::CConfigStreamingEngine()
		: m_ArchiveCount(CONFIGURED_FROM_FILE)
		, m_PhysicalStreamingBuffer(CONFIGURED_FROM_FILE)
		, m_VirtualStreamingBuffer(CONFIGURED_FROM_FILE)
{
}


void CConfigStreamingEngine::MergeData(const CConfigStreamingEngine& mergeIn)
{
	sMergeNonNegInt(m_ArchiveCount, mergeIn.m_ArchiveCount);
	sMergeNonNegInt(m_PhysicalStreamingBuffer, mergeIn.m_PhysicalStreamingBuffer);
	sMergeNonNegInt(m_VirtualStreamingBuffer, mergeIn.m_VirtualStreamingBuffer);
}

//-----------------------------------------------------------------------------

CConfigOnlineServices::CConfigOnlineServices()
: m_RosTitleName("")
, m_RosTitleVersion(0)
, m_RosScVersion(0)
, m_SteamAppId(0)
, m_TitleDirectoryName("")
, m_MultiplayerSessionTemplateName("")
, m_ScAuthTitleId("")
{
}


void CConfigOnlineServices::MergeData(const CConfigOnlineServices& mergeIn)
{
	ConstString empty("");
	if (mergeIn.m_RosTitleName != empty)
	{
		sMergeString(m_RosTitleName, mergeIn.m_RosTitleName);
		sMergeNonNegInt(m_RosTitleVersion, mergeIn.m_RosTitleVersion);
		sMergeNonNegInt(m_RosScVersion, mergeIn.m_RosScVersion);
		sMergeNonNegInt(m_SteamAppId, mergeIn.m_SteamAppId);
		sMergeString(m_TitleDirectoryName, mergeIn.m_TitleDirectoryName);
		sMergeString(m_MultiplayerSessionTemplateName, mergeIn.m_MultiplayerSessionTemplateName);
		sMergeString(m_ScAuthTitleId, mergeIn.m_ScAuthTitleId);
	}
}

//-----------------------------------------------------------------------------

CConfigUGCDescriptions::CConfigUGCDescriptions()
: m_MaxDescriptionLength(-1)
, m_MaxNumDescriptions(-1)
, m_SizeOfDescriptionBuffer(-1)
{
}

void CConfigUGCDescriptions::MergeData(const CConfigUGCDescriptions& mergeIn)
{
	sMergeNonNegInt(m_MaxDescriptionLength, mergeIn.m_MaxDescriptionLength);
	sMergeNonNegInt(m_MaxNumDescriptions, mergeIn.m_MaxNumDescriptions);
	sMergeNonNegInt(m_SizeOfDescriptionBuffer, mergeIn.m_SizeOfDescriptionBuffer);
}

//-----------------------------------------------------------------------------

CConfigNetScriptBroadcastData::CConfigNetScriptBroadcastData()
: m_HostBroadcastData(NUM_BROADCAST_DATA_ENTRIES)
, m_PlayerBroadcastData(NUM_BROADCAST_DATA_ENTRIES)
{
}

void CConfigNetScriptBroadcastData::MergeData(const CConfigNetScriptBroadcastData& mergeIn)
{
	for (int i=0; i<m_HostBroadcastData.GetCount(); i++)
	{
		sMergeNonNegInt(m_HostBroadcastData[i].m_SizeOfData, mergeIn.m_HostBroadcastData[i].m_SizeOfData);			
		sMergeNonNegInt(m_HostBroadcastData[i].m_MaxParticipants, mergeIn.m_HostBroadcastData[i].m_MaxParticipants);	
		sMergeNonNegInt(m_HostBroadcastData[i].m_NumInstances, mergeIn.m_HostBroadcastData[i].m_NumInstances);	
	}

	for (int i=0; i<m_PlayerBroadcastData.GetCount(); i++)
	{
		sMergeNonNegInt(m_PlayerBroadcastData[i].m_SizeOfData, mergeIn.m_PlayerBroadcastData[i].m_SizeOfData);			
		sMergeNonNegInt(m_PlayerBroadcastData[i].m_MaxParticipants, mergeIn.m_PlayerBroadcastData[i].m_MaxParticipants);	
		sMergeNonNegInt(m_PlayerBroadcastData[i].m_NumInstances, mergeIn.m_PlayerBroadcastData[i].m_NumInstances);	
	}
}

void CConfigScriptStackSizes::MergeData(const CConfigScriptStackSizes& mergeIn)
{
	s32 NumberOfEntriesToAdd = mergeIn.m_StackSizeData.GetCount();
	for (s32 loop = 0; loop < NumberOfEntriesToAdd; loop++)
	{
		m_StackSizeData.PushAndGrow(mergeIn.m_StackSizeData[loop]);
	}
}

void CConfigScriptResourceExpectedMaximums::MergeData(const CConfigScriptResourceExpectedMaximums& mergeIn)
{
	s32 NumberOfEntriesToAdd = mergeIn.m_ExpectedMaximumsArray.GetCount();
	for (s32 loop = 0; loop < NumberOfEntriesToAdd; loop++)
	{
		m_ExpectedMaximumsArray.PushAndGrow(mergeIn.m_ExpectedMaximumsArray[loop]);
	}
}

void CConfigScriptTextLines::MergeData(const CConfigScriptTextLines& mergeIn)
{
	s32 NumberOfEntriesToAdd = mergeIn.m_ArrayOfMaximumTextLines.GetCount();
	for (s32 loop = 0; loop < NumberOfEntriesToAdd; loop++)
	{
		m_ArrayOfMaximumTextLines.PushAndGrow(mergeIn.m_ArrayOfMaximumTextLines[loop]);
	}
}

//-----------------------------------------------------------------------------

CThreadSetup::CThreadSetup()
	: m_Priority(CGameConfig::PRIO_INHERIT)
{
}

void CThreadSetup::MergeData(const CThreadSetup &mergeIn)
{
	if (mergeIn.m_Priority != CGameConfig::PRIO_INHERIT)
	{
		m_Priority = mergeIn.m_Priority;
	}

	if (mergeIn.m_CpuAffinity.size() > 0)
	{
		m_CpuAffinity.CopyFrom(mergeIn.m_CpuAffinity.GetElements(), (unsigned short)mergeIn.m_CpuAffinity.GetCount());
	}
}

sysIpcPriority CThreadSetup::GetPriority(sysIpcPriority defaultPriority) const
{
	return (m_Priority == CGameConfig::PRIO_INHERIT) ? defaultPriority : (sysIpcPriority) m_Priority;
}

u32 CThreadSetup::GetCpuAffinityMask(u32 defaultAffinity) const
{
	if(m_CpuAffinity.size() == 0)
		return defaultAffinity;

	u32 affinity = 0;
	for(int i = 0; i < m_CpuAffinity.size(); i++)
	{
		affinity |= (1 << m_CpuAffinity[i]);
	}
	return affinity;
}


CConfigMediaTranscoding::CConfigMediaTranscoding()
	: m_TranscodingSmallObjectBuffer( -1 )
    , m_TranscodingSmallObjectMaxPointers( -1 )
	, m_TranscodingBuffer( -1 )
    , m_TranscodingMaxPointers( -1 )
{

}

void CConfigMediaTranscoding::MergeData( const CConfigMediaTranscoding &mergeIn )
{
    sMergeNonNegInt( m_TranscodingSmallObjectBuffer, mergeIn.m_TranscodingSmallObjectBuffer );
    sMergeNonNegInt( m_TranscodingSmallObjectMaxPointers, mergeIn.m_TranscodingSmallObjectMaxPointers );
    sMergeNonNegInt( m_TranscodingBuffer, mergeIn.m_TranscodingBuffer );
    sMergeNonNegInt( m_TranscodingMaxPointers, mergeIn.m_TranscodingMaxPointers );
}

//-----------------------------------------------------------------------------

CGameConfig::CGameConfig()
		: m_UseVehicleBurnoutTexture(CB_TRUE)
		, m_AllowCrouchedMovement(CB_TRUE)
		, m_AllowParachuting(CB_TRUE)
		, m_AllowStealthMovement(CB_TRUE)
        , m_DebugScriptsPath("")
{
	sysIpcSetThreadPriorityOverrideCb(ThreadPriorityOverrideCb);
}

void CGameConfig::MergeNonPoolData(const fwConfig& mergeIn)
{
	fwConfig::MergeNonPoolData(mergeIn);

	// TODO: Maybe use the parser for this, somehow?
	Assert(dynamic_cast<const CGameConfig*>(&mergeIn));

	const CGameConfig& mergeInCast = static_cast<const CGameConfig&>(mergeIn);

	m_ConfigPopulation.MergeData(mergeInCast.m_ConfigPopulation);
	m_Config2dEffects.MergeData(mergeInCast.m_Config2dEffects);
	m_ConfigModelInfo.MergeData(mergeInCast.m_ConfigModelInfo);
	m_ConfigExtensions.MergeData(mergeInCast.m_ConfigExtensions);
	m_ConfigStreamingEngine.MergeData(mergeInCast.m_ConfigStreamingEngine);
	m_ConfigOnlineServices.MergeData(mergeInCast.m_ConfigOnlineServices);
	m_ConfigUGCDescriptions.MergeData(mergeInCast.m_ConfigUGCDescriptions);
	m_ConfigNetScriptBroadcastData.MergeData(mergeInCast.m_ConfigNetScriptBroadcastData);
	m_ConfigScriptStackSizes.MergeData(mergeInCast.m_ConfigScriptStackSizes);
	m_ConfigScriptResourceExpectedMaximums.MergeData(mergeInCast.m_ConfigScriptResourceExpectedMaximums);
	m_ConfigScriptTextLines.MergeData(mergeInCast.m_ConfigScriptTextLines);
	m_ConfigMediaTranscoding.MergeData(mergeInCast.m_ConfigMediaTranscoding);

	// Merge the thread priorities.
	atMap<atHashString, CThreadSetup>::ConstIterator it = mergeInCast.m_Threads.CreateIterator();

	for (it.Start(); !it.AtEnd(); it.Next())
	{
		CThreadSetup *setup = m_Threads.Access(it.GetKey());

		if (setup)
		{
			setup->MergeData(it.GetData());
		}
		else
		{
			m_Threads[it.GetKey()] = it.GetData();
		}
	}

	// If adding other variables here, you are supposed to check
	// if mergeIn contains an override value for your variable, and if so,
	// use that, otherwise, leave it with the current value.
	sMergeBool(m_UseVehicleBurnoutTexture,		mergeInCast.m_UseVehicleBurnoutTexture);
	sMergeBool(m_AllowCrouchedMovement,			mergeInCast.m_AllowCrouchedMovement);
	sMergeBool(m_AllowParachuting,				mergeInCast.m_AllowParachuting);
	sMergeBool(m_AllowStealthMovement,			mergeInCast.m_AllowStealthMovement);

    sMergeString(m_DebugScriptsPath, mergeInCast.m_DebugScriptsPath);
}


#if !__FINAL
s32 CGameConfig::GetConfigMaxNumberOfScriptResource(const char *pResourceName) const
{
	u32 hashOfResourceName = atStringHash(pResourceName);

	const s32 num_of_array_entries = m_ConfigScriptResourceExpectedMaximums.m_ExpectedMaximumsArray.GetCount();

	for (s32 i=0; i<num_of_array_entries; i++)
	{
		if (m_ConfigScriptResourceExpectedMaximums.m_ExpectedMaximumsArray[i].m_ResourceTypeName == hashOfResourceName)
		{
			return m_ConfigScriptResourceExpectedMaximums.m_ExpectedMaximumsArray[i].m_ExpectedMaximum;
		}
	}

	return 0;
}
#endif	//	!__FINAL


s32 CGameConfig::GetConfigMaxNumberOfScriptTextLines(const char *pNameOfTextLine) const
{
	u32 hashOfNameOfTextLine = atStringHash(pNameOfTextLine);

	const s32 num_of_array_entries = m_ConfigScriptTextLines.m_ArrayOfMaximumTextLines.GetCount();

	for (s32 i=0; i<num_of_array_entries; i++)
	{
		if (m_ConfigScriptTextLines.m_ArrayOfMaximumTextLines[i].m_NameOfScriptTextLine == hashOfNameOfTextLine)
		{
			return m_ConfigScriptTextLines.m_ArrayOfMaximumTextLines[i].m_MaximumNumber;
		}
	}

	return 0;
}


const CThreadSetup *CGameConfig::GetThreadSetup(atHashString threadNameHash) const
{
	return m_Threads.Access(threadNameHash);
}

sysIpcPriority CGameConfig::GetThreadPriority(atHashString threadNameHash, sysIpcPriority defaultPriority) const
{
	const CThreadSetup *setup = CGameConfig::Get().GetThreadSetup(threadNameHash);

	if (setup)
	{
		defaultPriority = setup->GetPriority(defaultPriority);
	}

	return defaultPriority;
}


void CGameConfig::ThreadPriorityOverrideCb(u32 threadNameHash, sysIpcPriority &priority, u32 &cpuAffinityMask)
{
	const CThreadSetup *setup = CGameConfig::Get().GetThreadSetup(threadNameHash);

	if (setup)
	{
		priority = setup->GetPriority(priority);
		cpuAffinityMask = setup->GetCpuAffinityMask(cpuAffinityMask);
	}
}

//-----------------------------------------------------------------------------

// End of file game/config.cpp
