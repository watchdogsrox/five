// 
// audio/environment/occlusion/occlusionmanager.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 



#include "audio/ambience/ambientaudioentity.h"
#include "audio/debugaudio.h"
#include "audio/environment/environment.h"
#include "audio/environment/occlusion/occlusionmanager.h"
#include "audio/environment/occlusion/occlusionportalentity.h"
#include "audio/environment/occlusion/occlusionportalinfo.h"
#include "audio/northaudioengine.h"
#include "audioengine/engine.h"
#include "camera/CamInterface.h"
#include "camera/system/CameraManager.h"
#include "camera/base/BaseCamera.h"
#include "Control/gamelogic.h"
#include "Control/leveldata.h"
#include "debug/debugdraw.h"
#include "debug/debugglobals.h"
#include "fwsys/metadatastore.h"
#include "modelinfo/MloModelInfo.h"
#include "Objects/Door.h"
#include "Peds/Ped.h"
#include "profile/profiler.h"
#include "vehicles/vehicle.h"
#include "scene/portals/InteriorProxy.h"
#include "scene/portals/portal.h"
#include "scene/portals/PortalInst.h"
#include "scene/streamer/SceneStreamerMgr.h"

#if __ASSERT
#include "scene/playerswitch/PlayerSwitchInterface.h"
#endif

#if __BANK
#include "audio/frontendaudioentity.h"
#include "frontend/loadingscreens.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "scene/LoadScene.h"
#endif

AUDIO_ENVIRONMENT_OPTIMISATIONS()

PARAM(audDisablePortalOcclusion, "Will disable portals for occlusion, and instead use probes / zones");

#if __BANK
PARAM(audOcclusionBuildEnabled, "Enable building occlusion");
PARAM(audOcclusionBuildStart, "Will kick off a new audio occlusion build once the game has loaded");
PARAM(audOcclusionBuildContinue, "Will continue an in progress audio occlusion build once the game has loaded");
PARAM(audOcclusionBuildList, "Build the list of interiors from the InteriorProxy pool and then stop");
PARAM(audOcclusionBuildTempDir, "Specifies where the build tool temporary data lives which we read from in-game");
PARAM(audOcclusionBuildAssetsDir, "Specifies where the interior.pso.meta files live");
#endif

#define YACHT_TV_OCCLUSION_HACK 1

// Parameters limiting the amount of occlusion paths that can be generated, as it can get expensive quickly
const f32 naOcclusionManager::sm_MaxOcclusionPathDistance = 100.0f;	// Path distance before we consider it fully occluded

bool naOcclusionManager::sm_IsPortalOcclusionEnabled = true;

atFixedArray<const CInteriorProxy*, 64> naOcclusionManager::sm_InteriorMetadataLoadList;

const char* g_OcclusionPathOutsideStringName = "Outside";	// We want all Interiors room 0 to have the same hash, so we override values with this string
const u32 g_OcclusionPathOutsideHashkey = atStringHash(g_OcclusionPathOutsideStringName);
	
bool g_EnableOcclusionProbes = true;

f32 g_ScanLength = 40.0f;
f32 g_BlockedSmoothRate = 1.0f;

f32 g_ExteriorOcclusionSmoothRate = 2.0;
f32 g_RadialCoverageScalar = 3.0f;

f32 g_MinBlockedInInterior = 0.0f;	// @OCC TODO - 0.25f for GTAV

f32 g_MaxOcclusionDampingFactor = 1.0f;	// Will dampen all occlusion results by this factor, unless a lesser one is set elsewhere in code.

f32 g_OcclusionScanPositionZValues[AUD_NUM_OCC_PROBE_ELEV_INDICES] = {
	3.73f,		// 70 degrees from x-y plane
	1.0f,		// 45 degrees from x-y plane
	0.577f,		// 30 degrees from x-y plane
	0.176f,		// 10 degrees from x-y plane
	0,			// 0 degrees - level
	-0.131f,	// -7.5 degrees from x-y plane
	-0.577f,	// -30 degrees from x-y plane
	-1.0f,		// -45 degrees from x-y plane
	-3.73f		// -70 degrees from x-y plane
};

f32 g_ProbeZDistanceTolerance = 2.0f;	

bool g_UseNewBlockedCoveredAlgorithm = true;
f32 g_MaxBlockedWithoutCover = 0.5f;
f32 g_UnderCoverBlockedFactor = 0.8f;
bool g_AlwaysBlockedInInteriors = true;

extern Vector3 g_Directions[4];

#if __BANK
bool naOcclusionManager::sm_EnableEmitterOcclusion = true;
bool naOcclusionManager::sm_EnableAmbientOcclusion = true;
bool naOcclusionManager::sm_EnableSpawnEffectsOcclusion = true;
bool naOcclusionManager::sm_EnableRequestedEffectsOcclusion = true;
bool naOcclusionManager::sm_EnableCollisionOcclusion = true;

bool g_DisplayOcclusionDebugInfo = false;
bool g_DrawBlockedFactors = false;
bool g_DrawExteriorOcclusionPerDirection = false;
bool g_DrawPortalOcclusionPaths = false;
bool g_DrawPortalOcclusionPathDistances = false;
bool g_DrawPortalOcclusionPathInfo = false;
bool g_DrawLoadedOcclusionPaths = false;
bool g_DrawChildWeights = false;
bool g_DebugPortalInfo = false;
bool g_DebugDoorOcclusion = false;
bool g_DrawBrokenGlass = false;
bool g_AssertOnMissingOcclusionPaths = false;
bool g_DebugProbeOcclusionFromPlayer = false;
bool g_DebugProbeOcclusionFromListenerPosition = false;
bool g_DrawProbes = false;
bool g_ShowInteriorPoolCount = false;
bool g_DebugDrawLocalEnvironmentS = false;
bool g_DebugDrawLocalEnvironmentSSW = false;
bool g_DebugDrawLocalEnvironmentSW = false;
bool g_DebugDrawLocalEnvironmentWWS = false;
bool g_DebugDrawLocalEnvironmentW = false;
bool g_DebugDrawLocalEnvironmentWWN = false;
bool g_DebugDrawLocalEnvironmentNW = false;
bool g_DebugDrawLocalEnvironmentNNW = false;
bool g_DebugDrawLocalEnvironmentN = false;
bool g_DebugOWO = false;
bool g_DrawLevelLimits = false;

f32 g_DebugPortalInfoRange = -1.f;
u32 g_OcclusionDebugIndex = 0;
const u32 g_NumDebugOcclusionInfos = 64;
struct OcclusionDebugInfo
{
	u32 time;
	Vector3 intersectionPosition;
	Vector3 startPosition;
	s32 radialIndex;
	char debugText[64];
	naOcclusionProbeElevation elevationIndex;
}g_OcclusionDebugInfo[g_NumDebugOcclusionInfos];
#endif

#if __ASSERT
extern CPlayerSwitchInterface g_PlayerSwitch;
const u32 g_NumFramesLoadFailedToAssert = 20;
#endif

#if __BANK
char g_OcclusionBuildDataDir[256];
char g_OcclusionBuildInteriorListFile[256];
char g_OcclusionBuildInteriorBuildListFile[256];
char g_OcclusionBuildUnbuiltInteriorFile[256];
char g_OcclusionFailureFile[256];
char g_OcclusionBuildProgressFile[256];
char g_OcclusionBuiltIntraPathsFile[256];
char g_OcclusionBuiltInterPathsFile[256];
char g_OcclusionBuiltTunnelPathsFile[256];

const u32 g_NumFramesToUnloadInteriors = 90;
const bool g_DisableNonLinkedInterInteriorPaths = false;

atRangeArray<atArray<u32>, 2> g_SwapInteriorList;
#endif

void naOcclusionManager::Init()
{
	m_BlockedToLinearVolume.Init(ATSTRINGHASH("BLOCKED_TO_LINEAR_VOLUME", 0x0983f07d4));
	m_DistanceToBlockedCurve.Init(ATSTRINGHASH("DISTANCE_TO_BLOCKED", 0x0be554d32));
	m_AverageBlockedToBlockedFactor.Init(ATSTRINGHASH("AVERAGE_BLOCKED_TO_BLOCKED_FACTOR", 0x1d643a99));
	m_AngleFactorToOcclusionFactor.Init(ATSTRINGHASH("ANGLE_FACTOR_TO_OCCLUSION_FACTOR", 0x0d1f6a203));
	m_VerticalAngleToVehicleOcclusion.Init(ATSTRINGHASH("VERTICAL_ANGLE_TO_VEHICLE_OCCLUSION", 0x17abf7ea));

	for (s32 i=0; i<24; i++)
	{
		m_RelativeScanPositions[i].Zero();
		f32 angleInRadians = 2.0f * PI * ((f32)i)/24.0f;
		m_RelativeScanPositions[i].x = rage::Sinf(angleInRadians);
		m_RelativeScanPositions[i].y = rage::Cosf(angleInRadians);	
	}

	f32 verticalHeights[4];
	verticalHeights[0] = g_OcclusionScanPositionZValues[AUD_OCC_PROBE_ELEV_N];
	verticalHeights[1] = g_OcclusionScanPositionZValues[AUD_OCC_PROBE_ELEV_NNW];	
	verticalHeights[2] = g_OcclusionScanPositionZValues[AUD_OCC_PROBE_ELEV_SSW];
	verticalHeights[3] = g_OcclusionScanPositionZValues[AUD_OCC_PROBE_ELEV_S];
	f32 midHeights[4];
	midHeights[0] = g_OcclusionScanPositionZValues[AUD_OCC_PROBE_ELEV_NW];	
	midHeights[1] = g_OcclusionScanPositionZValues[AUD_OCC_PROBE_ELEV_WWN];		
	midHeights[2] = g_OcclusionScanPositionZValues[AUD_OCC_PROBE_ELEV_WWS];	
	midHeights[3] = g_OcclusionScanPositionZValues[AUD_OCC_PROBE_ELEV_SW];		

	// So we don't end up with occlusion nodes if we're lined up with a telephone pole, add in some slight radial offsets for each elevation
	f32 angleOffset[4] = {	2.0f * PI * 1.0f/45.0f * -1.0f,
							2.0f * PI * 1.0f/90.0f * -1.0f,
							2.0f * PI * 1.0f/90.0f,
							2.0f * PI * 1.0f/45.0f };

	for (s32 j=0; j<4; j++)
	{
		for (s32 i=0; i<8; i++)
		{
			m_RelativeMidScanPositions[j][i].Zero();
			f32 angleInRadians = 2.0f * PI * ((f32)i)/8.0f + angleOffset[j];
			m_RelativeMidScanPositions[j][i].x = rage::Sinf(angleInRadians);
			m_RelativeMidScanPositions[j][i].y = rage::Cosf(angleInRadians);
			m_RelativeMidScanPositions[j][i].z = midHeights[j];
			m_RelativeMidScanPositions[j][i].Normalize();
		}
		for (s32 i=0; i<4; i++)
		{
			m_RelativeVerticalScanPositions[j][i].Zero();
			f32 angleInRadians = 2.0f * PI * ((f32)i)/4.0f + angleOffset[j];
			m_RelativeVerticalScanPositions[j][i].x = rage::Sinf(angleInRadians);
			m_RelativeVerticalScanPositions[j][i].y = rage::Cosf(angleInRadians);
			m_RelativeVerticalScanPositions[j][i].z = verticalHeights[j];
			m_RelativeVerticalScanPositions[j][i].Normalize();
		}
	}

	m_GlobalOcclusionDampingFactor = 1.0f;

	m_OutsideWorldOcclusion = 0.f;
	m_OutsideWorldOcclusionSmoother.Init(1.f / 350.f, true);
	m_OutsideWorldOcclusionIgnoringInteriors = 0.f;
	m_OutsideWorldOcclusionSmootherIgnoringInteriors.Init(1.f / 350.f, true);

	m_CurrentLocalEnvironmentUpdateIndex = 0;

	m_LocalEnvironmentScanPositionLastFrame.Zero();

	for (s32 i=0; i<24; i++)
	{
		m_LevelWallDistances[i] = 0.0f;;
		m_LevelYLimits[i] = 0.0f;
		m_LevelXLimits[i] = 0.0f;
		m_LevelZLimits[i] = 0.0f;
	}

	for (s32 i=0; i<4; i++)
	{
		for (s32 j=0; j<8; j++)
		{
			m_MidWallDistances[i][j] = 0.0f;
			m_MidYLimits[i][j] = 0.0f;
			m_MidXLimits[i][j] = 0.0f;
			m_MidZLimits[i][j] = 0.0f;
			m_OcclusionBoundingBoxes[i][j].Zero();
			m_OcclusionBoundingBoxesExtend[i][j].Zero();
			m_ExteriorOcclusionFactor[i][j] = 0.0f;
			m_ExteriorOcclusionSmoother[i][j].Init(g_ExteriorOcclusionSmoothRate/1000.0f, true);
		}
	}

	for (s32 i=0; i<4; i++)
	{
		for (s32 j=0; j<4; j++)
		{
			m_VerticalWallDistances[i][j]  = 0.0f;
			m_VerticalYLimits[i][j] = 0.0f;
			m_VerticalXLimits[i][j] = 0.0f;
			m_VerticalZLimits[i][j] = 0.0f;
		}
	}

	for(s32 radial = 0; radial < 24; radial++)
	{
		m_OcclusionIntersectionLevel[radial].ClearAll();
	}
	for(s32 elevation = 0; elevation < 4; elevation++)
	{
		for(s32 radial = 0; radial < 8; radial++)
		{
			m_OcclusionIntersectionMid[elevation][radial].ClearAll();
		}

		for(s32 radial = 0; radial < 4; radial++)
		{
			m_OcclusionIntersectionVertical[elevation][radial].ClearAll();
		}
	}

	for (s32 i=0; i<4; i++)
	{
		m_BlockedInDirection[i] = 0.0f;
		m_BlockedInDirectionSmoother[i].Init(g_BlockedSmoothRate/1000.0f, true);
		m_BlockedLinearVolume[i] = 1.0f;
	}

	m_OutsideWorldOcclusionAfterPortals = 1.0f;

	m_LastFrameProbeUsedToDetectDoor = 0;

#if __BANK
	if (PARAM_audOcclusionBuildEnabled.Get())
	{
		const char* tempDir;
		if(PARAM_audOcclusionBuildTempDir.Get(tempDir))
		{
			formatf(g_OcclusionBuildDataDir, "assets:/export/audio/occlusion/");
			formatf(g_OcclusionBuildInteriorListFile, "%s\\InteriorList.xml", tempDir);
			formatf(g_OcclusionBuildInteriorBuildListFile, "%s\\InteriorBuildList.xml", tempDir);
			formatf(g_OcclusionBuildUnbuiltInteriorFile, "%s\\Unbuilt.xml", tempDir);
			formatf(g_OcclusionFailureFile, "%s\\Failures.xml", tempDir);
			formatf(g_OcclusionBuildProgressFile, "%s\\Progress.txt", tempDir);
			formatf(g_OcclusionBuiltIntraPathsFile, "%s\\BuiltIntraPaths.txt", tempDir);
			formatf(g_OcclusionBuiltInterPathsFile, "%s\\BuiltInterPaths.txt", tempDir);
			formatf(g_OcclusionBuiltTunnelPathsFile, "%s\\BuiltTunnelPaths.txt", tempDir);
		}
		else
		{
			Quitf(ERR_AUD_INVALIDRESOURCE, "Need to specify -audOcclusionBuildTempDir param when building occlusion paths");
		}

		const char* assetsDir;
		if(PARAM_audOcclusionBuildAssetsDir.Get(assetsDir))
		{
			formatf(g_OcclusionBuildDataDir, "%s", assetsDir);
		}
		else
		{
			Quitf("Need to specify -audOcclusionBuildAssetsDir param when building occlusion paths");
		}

		m_AudioOcclusionBuildToolEnabled = true;
		m_AudioOcclusionBuildToolStartBuild = PARAM_audOcclusionBuildStart.Get();
		m_AudioOcclusionBuildToolContinueBuild = PARAM_audOcclusionBuildContinue.Get();
		m_AudioOcclusionBuildToolBuildInteriorList = PARAM_audOcclusionBuildList.Get();
		m_IsBuildingOcclusionPaths = false;
		m_CurrentIntProxySlot = 0;
		m_CurrentInteriorLoadStartFrame = 0;
		m_HasStartedWaitingForPortalToLoad = false;
		m_InteriorUnloadStartFrame = 0;
		m_InteriorLoadStartFrame = 0;
		m_SwapInteriorSetIndex = 0;
		m_HasInteriorUnloadTimerStarted = false;
		m_HasInteriorLoadTimerStarted = false;
		m_CurrentOcclusionBuildStep = NUM_AUD_OCC_BUILD_STEPS;
		m_NumAttemptsAtCurrentBuildStep = 0;
		m_HasStartedLoadScene = false;
		m_FreezeInteriorLoading = false;

		g_SwapInteriorList[0].PushAndGrow(atStringHash("v_31_tun_swap"));
		g_SwapInteriorList[1].PushAndGrow(atStringHash("v_31_Tun_02"));
	}
	else
	{
		m_AudioOcclusionBuildToolEnabled = false;
		m_AudioOcclusionBuildToolStartBuild = false;
		m_AudioOcclusionBuildToolContinueBuild = false;
		m_AudioOcclusionBuildToolBuildInteriorList = false;
	}
#endif

	naOcclusionPortalInfo::InitClass();
	naOcclusionPortalEntity::InitClass();
	naOcclusionPathNode::InitClass();


	naOcclusionInteriorInfo::InitPool(MEMBUCKET_AUDIO);
	naOcclusionPortalEntity::InitPool(MEMBUCKET_AUDIO);
	naOcclusionPortalInfo::InitPool(MEMBUCKET_AUDIO);
	naOcclusionPathNode::InitPool(MEMBUCKET_AUDIO);
}

void naOcclusionManager::Shutdown()
{
	// Pools only reclaim memory, so we need to make sure we clean everything up via the object destructors
	atMap<const CInteriorProxy*, naOcclusionInteriorInfo*>::Iterator entry = m_LoadedInteriorInfos.CreateIterator();
	for(entry.Start(); !entry.AtEnd(); )
	{
		const CInteriorProxy* intProxy = entry.GetKey();
		entry.Next();
		UnloadOcclusionDataForInterior(intProxy);
	}

	m_LoadedInteriorInfos.Reset();
	sm_InteriorMetadataLoadList.Reset();

	naOcclusionInteriorInfo::ShutdownPool();
	naOcclusionPortalEntity::ShutdownPool();
	naOcclusionPortalInfo::ShutdownPool();
	naOcclusionPathNode::ShutdownPool();
}

void naOcclusionManager::Update()
{
	// Reset this each frame, and then set it down from there.  Don't use the set fn because that takes the Min() so if multiple places want to dampen, we used the lesser value.
	m_GlobalOcclusionDampingFactor = 1.0f;

#if __BANK
	if(PARAM_audOcclusionBuildEnabled.Get() && GetIsBuildingOcclusionPaths())
	{
	}
	else
#endif
	{
		if(GetIsPortalOcclusionEnabled())
		{
			UpdatePortalInfoList();
		}

		UpdateOcclusionProbes();

#if __BANK
		if(g_DrawLoadedOcclusionPaths)
		{
			DrawLoadedOcclusionPaths();
		}
#endif
	}
}

void naOcclusionManager::UpdateInteriorMetadata()
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() && audNorthAudioEngine::IsMPDataRequested() && !audNorthAudioEngine::HasLoadedMPDataSet())
	{
		return;
	}
#endif

#if __BANK
	if(PARAM_audOcclusionBuildEnabled.Get() && (GetIsBuildingOcclusionPaths() || PARAM_audOcclusionBuildStart.Get() || PARAM_audOcclusionBuildContinue.Get()))
	{
		return;
	}
#endif

	for(s32 i = sm_InteriorMetadataLoadList.GetCount() - 1; i >= 0; i--)
	{
		bool deleteEntry = false;
		const CInteriorProxy* intProxy = sm_InteriorMetadataLoadList[i];
		if(intProxy && intProxy->GetCurrentState() >= CInteriorProxy::PS_FULL)
		{
			const strLocalIndex fileIdx = intProxy->GetAudioOcclusionMetadataFileIndex();
			const bool hasObjectLoaded = g_fwMetaDataStore.HasObjectLoaded(fileIdx);
			naOcclusionInteriorInfo** intInfoPtr = m_LoadedInteriorInfos.Access(intProxy);

			if(hasObjectLoaded && !intInfoPtr && intProxy->GetInteriorInst() && intProxy->GetInteriorInst()->IsPopulated())
			{
				if(LoadOcclusionDataForInterior(intProxy))
				{
					deleteEntry = true;
				}

#if __ASSERT
				REPLAY_ONLY(if(!audNorthAudioEngine::sm_AreReplayBanksLoaded))
				{
				// Check for the validity of the interior game object data as that's mostly occlusion related
				const CInteriorInst* intInst = intProxy->GetInteriorInst();
				const InteriorSettings* intSettings = audNorthAudioEngine::GetObject<InteriorSettings>(intInst->GetMloModelInfo()->GetModelNameHash());
				if(naVerifyf(intSettings, "We don't have an InteriorSettings game object set up for CInteriorInst: %s", intInst->GetMloModelInfo()->GetModelName()))
				{
					// Start at room 1 since room 0 is "limbo" or outside
					for(u32 instRoomIdx = 1; instRoomIdx < intInst->GetNumRooms(); instRoomIdx++)
					{
						bool foundRoom = false;
						const u32 roomHash = intInst->GetRoomHashcode(instRoomIdx);
						for(u32 settingsRoomIdx = 0; settingsRoomIdx < intSettings->numRooms; settingsRoomIdx++)
						{
							const InteriorRoom* room = audNorthAudioEngine::GetObject<InteriorRoom>(intSettings->Room[settingsRoomIdx].Ref);
							if(room && room->RoomName == roomHash)
							{
								foundRoom = true;
							}
						}

						naAssertf(foundRoom, "We don't have a RoomSettings game object set up for Interior: %s Room: %s", 
							intInst->GetMloModelInfo()->GetModelName(), intInst->GetRoomName(instRoomIdx));
					}
				}
				}				
#endif
			}
		}
		else
		{
			deleteEntry = true;
		}

		if(deleteEntry)
		{
			sm_InteriorMetadataLoadList.Delete(i);
		}
	}

	atMap<const CInteriorProxy*, naOcclusionInteriorInfo*>::Iterator entry = m_LoadedInteriorInfos.CreateIterator();
	for(entry.Start(); !entry.AtEnd(); )
	{
		const CInteriorProxy* intProxy = entry.GetKey();
		entry.Next();
		if(intProxy && !g_fwMetaDataStore.HasObjectLoaded(intProxy->GetAudioOcclusionMetadataFileIndex()))
		{
			UnloadOcclusionDataForInterior(intProxy);
		}
	}
}

// Go through all the portals, and if a portal has a door attached, see if the game data that contains the door is loaded in
// Doors can either be hooked up to the portal, hooked up to the matching portal if it's a linked MLO, or in an exterior scene
// in which case we need to probe to get the door since there is no other hookup.
void naOcclusionManager::UpdatePortalInfoList()
{
	naOcclusionPortalInfo::Pool* pool = naOcclusionPortalInfo::GetPool();
	if(pool)
	{
		s32 poolSize = pool->GetSize();
		while(poolSize--)
		{
			naOcclusionPortalInfo* portalInfo = pool->GetSlot(poolSize);
			if(portalInfo)
			{
				portalInfo->Update();
			}
		}
	}
}

void naOcclusionManager::UpdateOcclusionProbes()
{
	if(g_EnableOcclusionProbes)
	{
		UpdateOcclusionProbesAsync();
	}
}

//************************************************************************************************
//	UpdateOcclusionProbesAsync
//	Asynchronous version of this function which issues line tests to ease cpu load.  
//  There will be an inbuilt latency from this approach, since probes will be
//	retrieved a frame or more late.

void naOcclusionManager::UpdateOcclusionProbesAsync()
{
	// Collect all the results we can from the WorldProbeAsync
	m_CachedLos.GetAllProbeResults();

	// If all were complete, then process them.
	// Otherwise wait another frame until they are all complete.
	if(m_CachedLos.AreAllComplete())
	{
		// Process probes issues the previous frame
		ProcessAsyncOcclusionProbes();

		// Issue a new set of probes
		IssueAsyncOcclusionProbes();
	}
}

void naOcclusionManager::ProcessAsyncOcclusionProbes()
{
	//*********************
	// The main LOS tests

	for(s32 i=0; i<audCachedLosMultipleResults::NumMainLOS; i++)
	{
		audCachedLos & los = m_CachedLos.m_MainLos[i];
		const Vector3 & startPosition = los.m_vStart;

		s32 currentIndex = m_CurrentLocalEnvironmentUpdateIndex + i*8;

		if(los.m_bContainsProbe)
		{
			Vector3 levelReflectionNormal(Vector3::ZeroType);

			if(los.m_bHitSomething==false)
			{
				m_LevelWallDistances[currentIndex] = -1.0f;
				levelReflectionNormal.Set(1.0f); // we never use this, but we normalise it at some point, so don't do Zero()
				if (currentIndex > 18 || currentIndex < 6)
				{
					m_LevelYLimits[currentIndex] = startPosition.y + 500.0f;
				}
				else if (currentIndex > 6 && currentIndex < 18)
				{
					m_LevelYLimits[currentIndex] = startPosition.y - 500.0f;
				}
				else
				{
					m_LevelYLimits[currentIndex] = startPosition.y;
				}
				if (currentIndex > 0 && currentIndex < 12)
				{
					m_LevelXLimits[currentIndex] = startPosition.x + 500.0f;
				}
				else if (currentIndex > 12)
				{
					m_LevelXLimits[currentIndex] = startPosition.x - 500.0f;
				}
				else
				{
					m_LevelXLimits[currentIndex] = startPosition.x;
				}
			}
			else
			{
				Vector3 intersectionPosition = los.m_vIntersectPos;
				m_LevelWallDistances[currentIndex] = (intersectionPosition - startPosition).Mag();
				levelReflectionNormal = los.m_vIntersectNormal;
				m_LevelYLimits[currentIndex] = intersectionPosition.y;
				m_LevelXLimits[currentIndex] = intersectionPosition.x;
				m_OcclusionIntersectionLevel[currentIndex].m_Position[0] = los.m_vIntersectPos;
			}

			audNorthAudioEngine::GetGtaEnvironment()->SetLevelReflectionNormal(levelReflectionNormal, currentIndex);

#if __BANK
			if(g_DrawProbes)
			{
				DebugAudioImpact(los.m_vIntersectPos, startPosition, currentIndex, AUD_OCC_PROBE_ELEV_W);
			}
#endif
		}
	}

	//********************
	// The mid LOS tests

	for(s32 elevationIndex=0; elevationIndex<audCachedLosMultipleResults::NumMidLOS; elevationIndex++)
	{
		audCachedLos & los = m_CachedLos.m_MidLos[elevationIndex];
		const Vector3 & startPosition = los.m_vStart;
		s32 currentIndex = m_CurrentLocalEnvironmentUpdateIndex;

		if(los.m_bContainsProbe)
		{
			Vector3 upperReflectionNormal(Vector3::ZeroType);

			if(los.m_bHitSomething==false)
			{
				m_MidWallDistances[elevationIndex][currentIndex] = -1.0f;
				// Y limits
				if (currentIndex > 6 || currentIndex < 2)
				{
					m_MidYLimits[elevationIndex][currentIndex] = startPosition.y + 500.0f;
				}
				else if (currentIndex > 2 && currentIndex < 6)
				{
					m_MidYLimits[elevationIndex][currentIndex] = startPosition.y - 500.0f;
				}
				else
				{
					m_MidYLimits[elevationIndex][currentIndex] = startPosition.y;
				}
				// X limits
				if (currentIndex > 0 && currentIndex < 4)
				{
					m_MidXLimits[elevationIndex][currentIndex] = startPosition.x + 500.0f;
				}
				else if (currentIndex > 4)
				{
					m_MidXLimits[elevationIndex][currentIndex] = startPosition.x - 500.0f;
				}
				else
				{
					m_MidXLimits[elevationIndex][currentIndex] = startPosition.x;
				}
				// Z limits
				if (elevationIndex < 2)
				{
					m_MidZLimits[elevationIndex][currentIndex] = startPosition.z + 500.0f;
				}
				else
				{
					m_MidZLimits[elevationIndex][currentIndex] = startPosition.z - 500.0f;
				}

				if (elevationIndex == 1) // MidElevIdx[1] = AUD_OCC_PROBE_ELEV_WWN we're looking up a wee bit - use this one for echos 
				{
					upperReflectionNormal.Set(1.0f); // we never use this, but we normalise it at some point, so don't do Zero()
				}
			}
			else
			{
				Vector3 intersectionPosition = los.m_vIntersectPos;
				m_MidWallDistances[elevationIndex][currentIndex] = (intersectionPosition - startPosition).Mag();
				m_MidYLimits[elevationIndex][currentIndex] = intersectionPosition.y;
				m_MidXLimits[elevationIndex][currentIndex] = intersectionPosition.x;
				m_MidZLimits[elevationIndex][currentIndex] = intersectionPosition.z;

				m_OcclusionIntersectionMid[elevationIndex][currentIndex].m_Position[0] = los.m_vIntersectPos;

				if (elevationIndex == 1) // MidElevIdx[1] = AUD_OCC_PROBE_ELEV_WWN we're looking up a wee bit - use this one for echoes 
				{
					upperReflectionNormal = los.m_vIntersectNormal;
					upperReflectionNormal.Normalize();
				}
			}

			if (elevationIndex == 1)
			{
				audNorthAudioEngine::GetGtaEnvironment()->SetUpperReflectionNormal(upperReflectionNormal, currentIndex);
			}

#if __BANK
			if(g_DrawProbes)
			{
				// change the 0-4 elevationIndex for mids over to the audElevationIndex 
				naOcclusionProbeElevation occlusionProbeElevationIndex;
				switch(elevationIndex)
				{
				case 0:
					occlusionProbeElevationIndex = AUD_OCC_PROBE_ELEV_NW;
					break;
				case 1:
					occlusionProbeElevationIndex = AUD_OCC_PROBE_ELEV_WWN;
					break;
				case 2:
					occlusionProbeElevationIndex = AUD_OCC_PROBE_ELEV_WWS;
					break;
				case 3:
					occlusionProbeElevationIndex = AUD_OCC_PROBE_ELEV_SW;
					break;
				default:
					naAssertf(0, "Shouldn't be here");
					occlusionProbeElevationIndex = AUD_OCC_PROBE_ELEV_NW;
					break;
				}

				DebugAudioImpact(los.m_vIntersectPos, startPosition, currentIndex, occlusionProbeElevationIndex);
			}
#endif
		}
	}

	//******************************
	// The near-vertical LOS tests

	for(s32 i=0; i<audCachedLosMultipleResults::NumNearVertLOS; i++)
	{
		audCachedLos & los = m_CachedLos.m_NearVerticalLos[i];
		const Vector3 & startPosition = los.m_vStart;

		s32 elevationIndex = 2* (m_CurrentLocalEnvironmentUpdateIndex >> 2); // so two or zero
		elevationIndex += i; // so (0 and 1) or (2 and 3)
		s32 currentIndex = m_CurrentLocalEnvironmentUpdateIndex & 3;

		if(los.m_bContainsProbe)
		{
			if(los.m_bHitSomething==false)
			{
				m_VerticalWallDistances[elevationIndex][currentIndex] = -1.0f;
				// Y limits
				if (currentIndex == 0 && elevationIndex!=0 && elevationIndex!=3)
				{
					m_VerticalYLimits[elevationIndex][currentIndex] = startPosition.y + 500.0f;
				}
				else if (currentIndex == 2 && elevationIndex!=0 && elevationIndex!=3)
				{
					m_VerticalYLimits[elevationIndex][currentIndex] = startPosition.y - 500.0f;
				}
				else
				{
					m_VerticalYLimits[elevationIndex][currentIndex] = startPosition.y;
				}
				// X limits
				if (currentIndex == 1 && elevationIndex!=0 && elevationIndex!=3)
				{
					m_VerticalXLimits[elevationIndex][currentIndex] = startPosition.x + 500.0f;
				}
				else if (currentIndex == 3 && elevationIndex!=0 && elevationIndex!=3)
				{
					m_VerticalXLimits[elevationIndex][currentIndex] = startPosition.x - 500.0f;
				}
				else
				{
					m_VerticalXLimits[elevationIndex][currentIndex] = startPosition.x;
				}
				// Z limits
				if (elevationIndex < 2)
				{
					m_VerticalZLimits[elevationIndex][currentIndex] = startPosition.z + 500.0f;
				}
				else
				{
					m_VerticalZLimits[elevationIndex][currentIndex] = startPosition.z - 500.0f;
				}
			}
			else
			{
				Vector3 intersectionPosition = los.m_vIntersectPos;
				m_VerticalWallDistances[elevationIndex][currentIndex] = (intersectionPosition - startPosition).Mag();
				m_VerticalYLimits[elevationIndex][currentIndex] = intersectionPosition.y;
				m_VerticalXLimits[elevationIndex][currentIndex] = intersectionPosition.x;
				m_VerticalZLimits[elevationIndex][currentIndex] = intersectionPosition.z;

				m_OcclusionIntersectionVertical[elevationIndex][currentIndex].m_Position[0] = los.m_vIntersectPos;
			}

#if __BANK
			if(g_DrawProbes)
			{
				naOcclusionProbeElevation occlusionElevationIndex;
				switch(elevationIndex)
				{
				case 0:
					occlusionElevationIndex = AUD_OCC_PROBE_ELEV_N;
					break;
				case 1:
					occlusionElevationIndex = AUD_OCC_PROBE_ELEV_NNW;
					break;
				case 2:
					occlusionElevationIndex = AUD_OCC_PROBE_ELEV_SSW;
					break;
				case 3:
					occlusionElevationIndex = AUD_OCC_PROBE_ELEV_S;
					break;
				default:
					naAssertf(0, "Shouldn't be here");
					occlusionElevationIndex = AUD_OCC_PROBE_ELEV_N;
					break;
				}

				DebugAudioImpact(los.m_vIntersectPos, startPosition, currentIndex, occlusionElevationIndex);
			}
#endif
		}
	}

	m_CachedLos.ClearAll();
}

void naOcclusionManager::IssueAsyncOcclusionProbes()
{
	m_CurrentLocalEnvironmentUpdateIndex = (m_CurrentLocalEnvironmentUpdateIndex + 1) % 8;

	Vector3 startPosition;
	const camBaseCamera* camera = camInterface::GetDominantRenderedCamera();
	if(camera)
	{
		startPosition = camera->GetFrame().GetPosition();
	}
	else
	{
		startPosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition(0));
	}
	
#if __BANK
	if(g_DebugProbeOcclusionFromPlayer)
	{
		CPed* player = FindPlayerPed();
		if(player)
		{
			startPosition = VEC3V_TO_VECTOR3(player->GetTransform().GetPosition());
		}
	}
	else if(g_DebugProbeOcclusionFromListenerPosition)
	{
		startPosition = audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition();
	}
#endif

	// Do 3 of the 24 level radial probes each frame
	for (s32 k=0; k<3; k++)
	{
		s32 currentIndex = m_CurrentLocalEnvironmentUpdateIndex + k*8;

		naAssertf(currentIndex < g_audNumLevelProbeRadials, "naEnvironmentGroupManager::IssueAsyncOcclusionProbes Bad index issuing level async probes: %d", currentIndex);

		Vector3 offset = m_RelativeScanPositions[currentIndex];
		offset.Scale(g_ScanLength);
		Vector3 endPosition = startPosition + offset;

		naAssertf(k<audCachedLosMultipleResults::NumMainLOS, "Index k (%d) is out of bounds of m_MainLos[]", k);
		m_CachedLos.m_MainLos[k].AddProbe(startPosition, endPosition, NULL, ArchetypeFlags::GTA_ALL_MAP_TYPES);
	}

	// Do the mid ones - 8 each, and 4 of them, two above, two below
	for (s32 elevationIndex=0; elevationIndex<4; elevationIndex++)
	{
		s32 currentIndex = m_CurrentLocalEnvironmentUpdateIndex;

		naAssertf(currentIndex < g_audNumMidProbeRadials, "naEnvironmentGroupManager::IssueAsyncOcclusionProbes Bad index issuing mid async probes: %d", currentIndex);

		Vector3 offset = m_RelativeMidScanPositions[elevationIndex][currentIndex];
		offset.Scale(g_ScanLength);
		Vector3 endPosition = startPosition + offset;

		naAssertf(elevationIndex<audCachedLosMultipleResults::NumMidLOS, "Index elevationIndex (%d) is out of bounds of m_MidLos[]", elevationIndex);
		m_CachedLos.m_MidLos[elevationIndex].AddProbe(startPosition, endPosition, NULL, ArchetypeFlags::GTA_ALL_MAP_TYPES);
	}

	// Do the (near) vertical ones - 4 each, and 4 of them
	for (s32 j=0; j<2; j++)
	{
		s32 elevationIndex = 2* (m_CurrentLocalEnvironmentUpdateIndex >> 2); // so two or zero
		elevationIndex += j; // so (0 and 1) or (2 and 3)
		s32 currentIndex = m_CurrentLocalEnvironmentUpdateIndex & 3;
		naAssertf(elevationIndex >= 0 && elevationIndex < g_audNumVertProbeRadials, "Bad elevationIndex: %d", elevationIndex);
		Vector3 offset = m_RelativeVerticalScanPositions[elevationIndex][currentIndex];
		offset.Scale(g_ScanLength);
		Vector3 endPosition = startPosition + offset;

		naAssertf(j<audCachedLosMultipleResults::NumNearVertLOS, "Index j (%d) is out of bounds of m_CachedLos.m_NearVerticalLos[]", j);
		m_CachedLos.m_NearVerticalLos[j].AddProbe(startPosition, endPosition, NULL, ArchetypeFlags::GTA_ALL_MAP_TYPES);
	}
}

f32 naOcclusionManager::GetWallDistance(naOcclusionProbeElevation elevationIndex, s32 radialIndex)
{
	f32 distance = 1.0f;

	switch(elevationIndex)
	{
	case AUD_OCC_PROBE_ELEV_W:
		distance = GetLevelWallDistance(radialIndex);
		break;
	case AUD_OCC_PROBE_ELEV_NW:
	case AUD_OCC_PROBE_ELEV_WWN:
	case AUD_OCC_PROBE_ELEV_WWS:
	case AUD_OCC_PROBE_ELEV_SW:
		distance = GetMidWallDistance(elevationIndex, radialIndex);
		break;
	case AUD_OCC_PROBE_ELEV_N:
	case AUD_OCC_PROBE_ELEV_NNW:
	case AUD_OCC_PROBE_ELEV_SSW:
	case AUD_OCC_PROBE_ELEV_S:
		distance = GetVerticalWallDistance(elevationIndex, radialIndex);
		break;
	default:
		naAssertf(0, "Bad elevationIndex passed into GetWallDistance: %u", elevationIndex);
		break;
	}

	return distance;
}

f32 naOcclusionManager::GetLevelWallDistance(s32 radialIndex)
{
	if(naVerifyf(radialIndex >= 0 && radialIndex < g_audNumLevelProbeRadials, "Bad radialIndex passed trying to get LevelWallDistance: %d", radialIndex))
	{
		return m_LevelWallDistances[radialIndex];
	}
	return 1.0f;
}

f32 naOcclusionManager::GetMidWallDistance(naOcclusionProbeElevation elevationIndex, s32 radialIndex)
{
	if(naVerifyf(radialIndex >= 0 && radialIndex < g_audNumMidProbeRadials, "Bad radialIndex passed trying to get MidWallDistance: %d", radialIndex))
	{
		s32 midIndex = GetArrayElevationIndexFromOcclusionElevationIndex(elevationIndex);
		return m_MidWallDistances[midIndex][radialIndex];
	}
	return 1.0f;
}

f32 naOcclusionManager::GetVerticalWallDistance(naOcclusionProbeElevation elevationIndex, s32 radialIndex)
{
	if(naVerifyf(radialIndex >= 0 && radialIndex < g_audNumVertProbeRadials, "Bad radialIndex passed trying to get VerticalWallDistance: %d", radialIndex))
	{
		s32 verticalIndex = GetArrayElevationIndexFromOcclusionElevationIndex(elevationIndex);
		return m_VerticalWallDistances[verticalIndex][radialIndex];
	}
	return 1.0f;
}

const Vector3& naOcclusionManager::GetRelativeScanPosition(s32 radialIndex)
{
	if(naVerifyf(radialIndex >= 0 && radialIndex < g_audNumLevelProbeRadials, "Bad radialIndex passed trying to get levelRelativeScanPosition: %d", radialIndex))
	{
		return m_RelativeScanPositions[radialIndex];
	}
	return m_RelativeScanPositions[0];	// return north if we're passing in bad data
}

// Start game-audio thread ********************************************************************************************************************************************

void naOcclusionManager::SetGlobalOcclusionDampingFactor(const f32 dampingFactor)
{
	if(naVerifyf(dampingFactor >= 0.0f && dampingFactor <= 1.0f, "Trying to set out of bounds global occlusion damping factor: %f", dampingFactor)) 
	{
		m_GlobalOcclusionDampingFactor = Min(m_GlobalOcclusionDampingFactor, dampingFactor);
	}
}

void naOcclusionManager::UpdateOcclusionMetrics()
{
	// The occlusion build process takes a long time, so don't try using metrics we haven't finished building
#if __BANK
	if(PARAM_audOcclusionBuildEnabled.Get() && GetIsBuildingOcclusionPaths())
	{
		return;
	}
#endif

	// Taking this out - it seems to be causing slight drops in occlusion as the bounds get pushed out over the walls that the probes are hitting
	// Talked to Matt, and he seems to think there used to be a difference in how we were storing the limits, as distances rather than positions.
	// Our line scans are a frame out of date, so update them for camera movement 
	//UpdateOcclusionMetricsForNewCameraPosition();

	// Update the occlusion boxes with the new limits based on our camera movement
	UpdateOcclusionBoundingBoxes();

	// Get how occluded we are for 4 elevations * 8 radials based on portals leading outside, doors, shattered glass, interior settings, etc.
	UpdateExteriorOcclusion();

	// Simple occlusion for the 4 cardinal directions based on how close we are to walls, so if we're right up against a wall we occlude sounds behind it.
	UpdateBlockedFactors();

	// A single value for occluded the outside world is based on the exteriorOcclusion or if you're in a vehicle how far in that vehicle you are.
	UpdateOutsideWorldOcclusion();

	//// Don't do this until async streaming is set up
	//if(GetIsPortalOcclusionEnabled())
	//{
	//	UpdateLoadedInteriorList();
	//}

	// Dampen all occlusion by this factor, it's basically a global scalar.  This can be set elsewhere, and if lesser will override this value.
	SetGlobalOcclusionDampingFactor(g_MaxOcclusionDampingFactor);

#if __BANK
	DrawDebug();
#endif
}

void naOcclusionManager::UpdateOcclusionMetricsForNewCameraPosition()
{
	Vector3 cameraMoved = audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition() - m_LocalEnvironmentScanPositionLastFrame;

	m_LocalEnvironmentScanPositionLastFrame = audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition();

	// X
	if (cameraMoved.x > 0.0f)
	{
		for (s32 i=1; i<12; i++)
		{
			m_LevelXLimits[i] += cameraMoved.x;
		}
		for (s32 i=0; i<4; i++)
		{
			for (s32 j=1; j<4; j++)
			{
				m_MidXLimits[i][j] += cameraMoved.x;
			}
			m_VerticalXLimits[i][1] += cameraMoved.x;
		}
	}
	else
	{
		for (s32 i=13; i<24; i++)
		{
			m_LevelXLimits[i] += cameraMoved.x;
		}
		for (s32 i=0; i<4; i++)
		{
			for (s32 j=5; j<8; j++)
			{
				m_MidXLimits[i][j] += cameraMoved.x;
			}
			m_VerticalXLimits[i][3] += cameraMoved.x;
		}
	}
	// Y
	if (cameraMoved.y > 0.0f)
	{
		for (s32 i=0; i<6; i++)
		{
			m_LevelYLimits[i] += cameraMoved.y;
		}
		for (s32 i=19; i<24; i++)
		{
			m_LevelYLimits[i] += cameraMoved.y;
		}
		for (s32 i=0; i<4; i++)
		{
			for (s32 j=0; j<2; j++)
			{
				m_MidYLimits[i][j] += cameraMoved.y;
			}
			m_MidYLimits[i][7] += cameraMoved.y;
			m_VerticalYLimits[i][0] += cameraMoved.y;
		}
	}
	else
	{
		for (s32 i=7; i<18; i++)
		{
			m_LevelYLimits[i] += cameraMoved.y;
		}
		for (s32 i=0; i<4; i++)
		{
			for (s32 j=3; j<6; j++)
			{
				m_MidYLimits[i][j] += cameraMoved.y;
			}
			m_VerticalYLimits[i][2] += cameraMoved.y;
		}
	}
	// Z
	if (cameraMoved.z > 0.0f)
	{
		for (s32 i=0; i<2; i++)
		{
			for (s32 j=0; j<8; j++)
			{
				m_MidZLimits[i][j] += cameraMoved.z;
			}
			for (s32 j=0; j<4; j++)
			{
				m_VerticalZLimits[i][0] += cameraMoved.z;
			}
		}
	}
	else
	{
		for (s32 i=2; i<4; i++)
		{
			for (s32 j=0; j<8; j++)
			{
				m_MidZLimits[i][j] += cameraMoved.z;
			}
			for (s32 j=0; j<4; j++)
			{
				m_VerticalZLimits[i][0] += cameraMoved.z;
			}
		}
	}
}

void naOcclusionManager::UpdateOcclusionBoundingBoxes()
{
	// radialIndices
	// The X, Y, and Z coordinates for each radial uses the probes within the current radial to the next radial index, which is 45 degrees in this case (360 / 8).
	//		8 mid probes, we look at the probe along the current radial, and the probe along the next radial.
	//		4 vertical probes, we look at whichever vertical probe is along the same radial as one of the mid probes we're looking at.
	//		24 level probes, we look at the current radial, the next radial, and the 2 probes in between those radials.
	//		For the extended occlusion boxes we also look at the level radials just outside the 45 degree arc at the 2 probes on either side of the 2 radials we're looking at.

	// boudingBoxElevationIndices
	// The X, Y, and Z coordinates for each of the different elevations use the following probe intersections	
	//
	//	0 - UP					- Vertical[0], Vertical[1] probes for X, Y, and Z values
	//							- Uses Mid[0] for the extended bounding boxes
	//
	//	1 - SLIGHTLY UP			- Mid[0], Mid[1] probes for Z
	//							- Vertical[1] probes for extended Z boxes
	//							- Mid[0], Mid[1], and Level probes for X and Y
	//							- Uses Level probes on either side of 2 for the extended bounding boxes
	//
	//	2 - SLIGHTLY DOWN		- Mid[3], Mid[2] probes for Z
	//							- Vertical[2] probes for extended Z boxes
	//							- Mid[3], Mid[2], and Level probes for X and Y 
	//							- Uses Level probes on either side of 2 for the extended bounding boxes
	//
	//	3 - DOWN				- Vertical[3], Vertical[2] probes for X, Y, and Z values
	//							- Uses Mid[2] for the extended bounding boxes

	// Do the very vertical ones
	for (s32 elevationIndex=0; elevationIndex<2; elevationIndex++)
	{
		s32 boundingBoxElevationIndex = elevationIndex*3;
		s32 verticalIndex = 0;
		s32 verticalIndex2 = 1;
		s32 midIndex = 0;

		if (elevationIndex==1)
		{
			verticalIndex = 3;
			verticalIndex2 = 2;
			midIndex = 3;
		}
		f32 zMultiplier = 1.0f;
		if (elevationIndex==1)
		{
			zMultiplier = -1.0f;
		}
		for (s32 radialIndex=0; radialIndex<8; radialIndex++)
		{
			//  radialIndex4 converts the radial index from 0-8 to 0-4, which we use for the radial values on the vertical limits since they only use 4 probes
			//  Since there are twice as many mid probes as vertical probes, this will ensure we're using a vertical probe with the same radial as one of the 2 mid probes.
			s32 radialIndex4 = ((s32)(((f32)radialIndex+1.0f)/2.0f)) % 4;

			//  radialIndex2 gives the index to the right of the radialIndex going clockwise if looking down.
			s32 radialIndex2 = (radialIndex+1) % 8;
			f32 yMultiplier = 1.0f;
			if (radialIndex>1 && radialIndex<6)
			{
				yMultiplier = -1.0f;
			}
			f32 xMultiplier = 1.0f;
			if (radialIndex>3)
			{
				xMultiplier = -1.0f;
			}

			f32 maxZ = Max(zMultiplier*m_VerticalZLimits[verticalIndex][radialIndex4], zMultiplier*m_VerticalZLimits[verticalIndex2][radialIndex4]);
			f32 maxZextended = Max(maxZ, zMultiplier*m_MidZLimits[midIndex][radialIndex], zMultiplier*m_MidZLimits[midIndex][radialIndex2]);
			maxZ *= zMultiplier;
			maxZextended *= zMultiplier;
			m_OcclusionBoundingBoxes[boundingBoxElevationIndex][radialIndex].z = maxZ;
			m_OcclusionBoundingBoxesExtend[boundingBoxElevationIndex][radialIndex].z = maxZextended;

			f32 maxY = Max(yMultiplier*m_VerticalYLimits[verticalIndex][radialIndex4], yMultiplier*m_VerticalYLimits[verticalIndex2][radialIndex4]);
			f32 maxYextended = Max(maxY, yMultiplier*m_MidYLimits[midIndex][radialIndex], yMultiplier*m_MidYLimits[midIndex][radialIndex2]);
			maxY *= yMultiplier;
			maxYextended *= yMultiplier;
			m_OcclusionBoundingBoxes[boundingBoxElevationIndex][radialIndex].y = maxY;
			m_OcclusionBoundingBoxesExtend[boundingBoxElevationIndex][radialIndex].y = maxYextended;

			f32 maxX = Max(xMultiplier*m_VerticalXLimits[verticalIndex][radialIndex4], xMultiplier*m_VerticalXLimits[verticalIndex2][radialIndex4]);
			f32 maxXextended = Max(maxX, xMultiplier*m_MidXLimits[midIndex][radialIndex], xMultiplier*m_MidXLimits[midIndex][radialIndex2]);
			maxX *= xMultiplier;
			maxXextended *= xMultiplier;
			m_OcclusionBoundingBoxes[boundingBoxElevationIndex][radialIndex].x = maxX;
			m_OcclusionBoundingBoxesExtend[boundingBoxElevationIndex][radialIndex].x = maxXextended;
		}
	}

	// Mid ones
	for (s32 elevationIndex=1; elevationIndex<3; elevationIndex++)
	{
		s32 midIndex = 0;
		s32 midIndex2 = 1;
		s32 midIndex3 = 2;	// This is the mid on the opposite hemisphere as the other 2 mids
		s32 verticalIndex = 1;
		f32 zMultiplier = 1.0f;
		if (elevationIndex==2)
		{
			midIndex = 3;
			midIndex2 = 2;
			midIndex3 = 1;	// This is the mid on the opposite hemisphere as the other 2 mids
			verticalIndex = 2;
			zMultiplier = -1.0f;
		}
		for (s32 radialIndex=0; radialIndex<8; radialIndex++)
		{
			s32 radialIndex4 = ((s32)(((f32)radialIndex+1.0f)/2.0f)) % 4;
			s32 radialIndex2 = (radialIndex+1) % 8;
			s32 levelRadialIndex1 = radialIndex*3;				//Convert radial index to 0-24
			s32 levelRadialIndex2 = (levelRadialIndex1+1) % 24;	//1 of 2 probes between the 2 mid radials
			s32 levelRadialIndex3 = (levelRadialIndex1+2) % 24;	//2 of 2 probes between the 2 mid radials
			s32 levelRadialIndex4 = (levelRadialIndex1+3) % 24;	//Basically radialIndex2 converted to 0-24, it's the next radial to the right mid radial
			s32 levelRadialIndexExtend1 = (levelRadialIndex1+23) % 24;	//The level radial just before the current mid radial
			s32 levelRadialIndexExtend2 = (levelRadialIndex1+4) % 24;	//The level radial just after the radialIndex2 mid radial
			f32 yMultiplier = 1.0f;
			if (radialIndex>1 && radialIndex<6)
			{
				yMultiplier = -1.0f;
			}
			f32 xMultiplier = 1.0f;
			if (radialIndex>3)
			{
				xMultiplier = -1.0f;
			}

			f32 maxZ1 = Max(zMultiplier*m_MidZLimits[midIndex][radialIndex], zMultiplier*m_MidZLimits[midIndex2][radialIndex]);
			f32 maxZ2 = Max(zMultiplier*m_MidZLimits[midIndex][radialIndex2], zMultiplier*m_MidZLimits[midIndex2][radialIndex2]);
			f32 maxZ = Max(maxZ1, maxZ2);
			f32 maxZvertical = zMultiplier*m_VerticalZLimits[verticalIndex][radialIndex4];
			f32 maxZextended = Max(maxZ, maxZvertical);
			maxZ *= zMultiplier;
			maxZextended *= zMultiplier;
			m_OcclusionBoundingBoxes[elevationIndex][radialIndex].z = maxZ;
			m_OcclusionBoundingBoxesExtend[elevationIndex][radialIndex].z = maxZextended;

			// Get max Y
			f32 maxY1 = Max(yMultiplier*m_MidYLimits[midIndex][radialIndex], yMultiplier*m_MidYLimits[midIndex2][radialIndex]);
			f32 maxY2 = Max(yMultiplier*m_MidYLimits[midIndex][radialIndex2], yMultiplier*m_MidYLimits[midIndex2][radialIndex2]);
			f32 maxLevelY1 = Max(yMultiplier*m_LevelYLimits[levelRadialIndex1], yMultiplier*m_LevelYLimits[levelRadialIndex2]);
			f32 maxLevelY2 = Max(yMultiplier*m_LevelYLimits[levelRadialIndex3], yMultiplier*m_LevelYLimits[levelRadialIndex4]);
			f32 maxLevelY = Max(maxLevelY1, maxLevelY2);
			f32 maxY = Max(maxY1, maxY2, maxLevelY);
			f32 maxYextendedLevel = Max(yMultiplier*m_LevelYLimits[levelRadialIndexExtend1], yMultiplier*m_LevelYLimits[levelRadialIndexExtend2]);
			f32 maxYextendedMid = Max(yMultiplier*m_MidYLimits[midIndex3][radialIndex], yMultiplier*m_MidYLimits[midIndex3][radialIndex2]);
			f32 maxYextended = Max(maxY, maxYextendedLevel, maxYextendedMid);
			maxY *= yMultiplier;
			maxYextended *= yMultiplier;
			m_OcclusionBoundingBoxes[elevationIndex][radialIndex].y = maxY;
			m_OcclusionBoundingBoxesExtend[elevationIndex][radialIndex].y = maxYextended;

			// Get max X
			f32 maxX1 = Max(xMultiplier*m_MidXLimits[midIndex][radialIndex], xMultiplier*m_MidXLimits[midIndex2][radialIndex]);
			f32 maxX2 = Max(xMultiplier*m_MidXLimits[midIndex][radialIndex2], xMultiplier*m_MidXLimits[midIndex2][radialIndex2]);
			f32 maxLevelX1 = Max(xMultiplier*m_LevelXLimits[levelRadialIndex1], 
				xMultiplier*m_LevelXLimits[levelRadialIndex2]);
			f32 maxLevelX2 = Max(xMultiplier*m_LevelXLimits[levelRadialIndex3], 
				xMultiplier*m_LevelXLimits[levelRadialIndex4]);
			f32 maxLevelX = Max(maxLevelX1, maxLevelX2); 
			f32 maxX = Max(maxX1, maxX2, maxLevelX);
			f32 maxXextendedLevel = Max(xMultiplier*m_LevelXLimits[levelRadialIndexExtend1], xMultiplier*m_LevelXLimits[levelRadialIndexExtend2]);
			f32 maxXextendedMid = Max(xMultiplier*m_MidXLimits[midIndex3][radialIndex], xMultiplier*m_MidXLimits[midIndex3][radialIndex2]);
			f32 maxXextended = Max(maxX, maxXextendedLevel, maxXextendedMid);
			maxX *= xMultiplier;
			maxXextended *= xMultiplier;
			m_OcclusionBoundingBoxes[elevationIndex][radialIndex].x = maxX;
			m_OcclusionBoundingBoxesExtend[elevationIndex][radialIndex].x = maxXextended;
		}
	}
}

void naOcclusionManager::UpdateExteriorOcclusion()
{
	f32 exteriorOcclusionFactor[g_audNumOccZoneElevations][g_audNumOccZoneRadials];

	CInteriorInst* intInst = audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorInstance();
	const InteriorSettings* interiorSettings = audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorSettingsPtr();

	if(intInst && interiorSettings)
	{
		// Set our interior occlusion zones to be fully occluded.  We'll then scale them down by how much each portal un-occludes them.
		for(u32 elev = 0; elev < g_audNumOccZoneElevations; elev++)
		{
			for(u32 rad = 0; rad < g_audNumOccZoneRadials; rad++)
			{
				exteriorOcclusionFactor[elev][rad] = 1.0f;
			}
		}

		// Get the position we're going to take our occlusion from - this needs to be pretty close to where we're getting our interior
		// and room ids from, otherwise we risk getting nonsense portal directions
		Vector3 cameraPosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition(0));

		f32 amountToOccludeOutsideWorld = 1.0f;
		const InteriorRoom* room = audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorRoomPtr();
		if(room)
		{
			amountToOccludeOutsideWorld = 1.0f - room->ExteriorAudibility;
		}

		// First go through all the linked interiors and see how much leakage is coming from those rooms
		for(u32 linkedInteriorSlot = 0; linkedInteriorSlot < AUD_MAX_LINKED_ROOMS; linkedInteriorSlot++)
		{
			fwInteriorLocation linkedRoom = audNorthAudioEngine::GetGtaEnvironment()->GetLinkedRoom(linkedInteriorSlot);
			if(linkedRoom.IsValid())
			{
				CInteriorInst* linkedIntInst = CInteriorInst::GetInteriorForLocation(linkedRoom);
				if(linkedIntInst)
				{
					UpdateExteriorOcclusionWithInterior(linkedIntInst, cameraPosition, exteriorOcclusionFactor);
				}
			}
		}

		// Now see how much leakage is coming from the portals leading outside in our listener's interior
		UpdateExteriorOcclusionWithInterior(intInst, cameraPosition, exteriorOcclusionFactor);

		// Now with the final values scale them by the exteriorAudibility
		for (s32 elev = 0; elev < g_audNumOccZoneElevations; elev++)
		{
			for (s32 rad = 0; rad < g_audNumOccZoneRadials; rad++)
			{
				exteriorOcclusionFactor[elev][rad] *= amountToOccludeOutsideWorld;
			}
		}
	}
	else	// We're outside
	{
		// Set our interior occlusion zones to be fully unoccluded.
		for (s32 elev = 0; elev < g_audNumOccZoneElevations; elev++)
		{
			for (s32 rad = 0; rad < g_audNumOccZoneRadials; rad++)
			{
				exteriorOcclusionFactor[elev][rad] = 0.0f;
			}
		}
	}

	// smooth those to get our current usable values
#if __BANK
	for (s32 elev = 0; elev < g_audNumOccZoneElevations; elev++)
	{
		for (s32 rad = 0; rad < g_audNumOccZoneRadials; rad++)
		{
			m_ExteriorOcclusionSmoother[elev][rad].SetRate(g_ExteriorOcclusionSmoothRate/1000.0f);
		}
	}
#endif
	for (s32 elev = 0; elev < g_audNumOccZoneElevations; elev++)
	{
		for (s32 rad = 0; rad < g_audNumOccZoneRadials; rad++)
		{
			m_ExteriorOcclusionFactor[elev][rad] = m_ExteriorOcclusionSmoother[elev][rad].CalculateValue(exteriorOcclusionFactor[elev][rad], fwTimer::GetTimeStep());
		}
	}
}

void naOcclusionManager::UpdateExteriorOcclusionWithInterior(CInteriorInst* intInst, const Vector3& cameraPos, f32 exteriorOcclusionFactor[][g_audNumOccZoneRadials])
{
	if (!intInst->IsPopulated())
	{
		return;
	}

	if(naVerifyf(intInst && intInst->GetProxy(), "naEnvironmentGroupManager::UpdateExteriorOcclusionWithInterior Passed NULL CInteriorInst*"))
	{
		CMloModelInfo* modelInfo = intInst->GetMloModelInfo();
		if(!naVerifyf(modelInfo, "naEnvironmentGroupManager::UpdateExteriorOcclusion Unable to get CMloModelInfo in audio exterior occlusion update"))
		{
			return;
		}

		// Room '0' is outside or 'limbo'.  So if we cycle through all the portals in room '0' we'll look at all the portals leading outside.
		u32 numPortals = intInst->GetNumPortalsInRoom(INTLOC_ROOMINDEX_LIMBO);

		for(u32 portalIdx = 0; portalIdx < numPortals; portalIdx++)
		{
			// Use the information on the room that the portal leads to, not just the room that the listener is in.  
			// So if we mark portals as being blocked in the next room and not blocked in the listeners room, the portals in the next room are still blocked.
			s32 globalPortalIdx = intInst->GetPortalIdxInRoom(INTLOC_ROOMINDEX_LIMBO, portalIdx);
			CPortalFlags portalFlags;
			u32 destRoomIdx;
			u32 roomIdx1, roomIdx2;
			modelInfo->GetPortalData(globalPortalIdx, roomIdx1, roomIdx2, portalFlags);

			// Check to make sure it's not a link portal or mirror portal, in which case it's leading outside
			if(!portalFlags.GetIsLinkPortal() && !portalFlags.GetIsMirrorPortal())
			{
				// Figure out which of those is not the room we're in (not INTLOC_ROOMINDEX_LIMBO)
				destRoomIdx = roomIdx1 == INTLOC_ROOMINDEX_LIMBO ? roomIdx2 : roomIdx1;

				// Get the audio data that corresponds to this roomIdx
				const InteriorSettings* interiorSettings = NULL;
				const InteriorRoom* interiorRoom = NULL;
				audNorthAudioEngine::GetGtaEnvironment()->GetInteriorSettingsForInterior(intInst, destRoomIdx, interiorSettings, interiorRoom);
				if(interiorSettings)
				{
					fwPortalCorners portalCorners;
					intInst->GetPortalInRoom(INTLOC_ROOMINDEX_LIMBO, portalIdx, portalCorners);	// Uses the room-specific portalIdx
					// work out which occlusion zones the four corners are in. Then those, and any inbetween, all get the appropriate un-occludedness.
					// A portal corner at 89 degrees will be considered to be in the occlusionZone[1], whose radial is at 45 degrees based on the way occlusion zones are defined.
					// See GetOcclusionZoneIndices
					s32 radialIndex[4];
					s32 elevationIndex[4];
					f32 radialRatio[4];
					f32 elevationRatio[4];
					for (s32 i=0; i<4; i++)
					{
						GetOcclusionZoneIndices(cameraPos, portalCorners.GetCorner(i), radialRatio[i], radialIndex[i], elevationRatio[i], elevationIndex[i]);
					}

					Vector3 centre = portalCorners.GetCorner(0) + portalCorners.GetCorner(1) + portalCorners.GetCorner(2) + portalCorners.GetCorner(3);
					centre.Scale(0.25f);

					s32 minElevation = Min(elevationIndex[0], elevationIndex[1], elevationIndex[2], elevationIndex[3]);
					s32 maxElevation = Max(elevationIndex[0], elevationIndex[1], elevationIndex[2], elevationIndex[3]);
					bool elevationSpanned[4] = {false, false, false, false};
					naAssertf(minElevation<4 && maxElevation<4, 
						"naEnvironmentGroupManager::UpdateExteriorOcclusion Invalid minElevation (%d) or maxElevation (%d)", 
						minElevation, maxElevation);
					for (s32 i=minElevation; i<=maxElevation; i++)
					{
						elevationSpanned[i] = true;
					}
					bool radialSpanned[8] = {false, false, false, false, false, false, false, false};
					f32 radialCoverage[8] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
					// for every pair of portal corners, we span everything in-between them
					for (s32 i=0; i<3; i++)
					{
						for (s32 j=i+1; j<4; j++)
						{
							const s32 minRadIdxCornerIdx = radialIndex[i] <= radialIndex[j] ? i : j;
							const s32 maxRadIdxCornerIdx = minRadIdxCornerIdx == j ? i : j;

							const s32 minRadIdx = radialIndex[minRadIdxCornerIdx];
							const s32 maxRadIdx = radialIndex[maxRadIdxCornerIdx];

							// See how much of this index the portal covers
							if(minRadIdx == maxRadIdx)
							{
								radialCoverage[minRadIdx] = Max(radialCoverage[minRadIdx], Abs(radialRatio[maxRadIdxCornerIdx] - radialRatio[minRadIdxCornerIdx]));
								radialSpanned[minRadIdx] = true;
							}
							else
							{
								// Does the portal span from the minRadIdx to the maxRadIdx, or from the maxRadIdx to the minRadIdx (wrapping around N)
								bool spansMinToMaxIndices;
								if((maxRadIdx - minRadIdx) < 4)
								{
									spansMinToMaxIndices = true;
								}
								else if(maxRadIdx - minRadIdx > 4)
								{
									spansMinToMaxIndices = false;
								}
								// maxRadIdx - minRadIdx == 4, so use the radial ratios to determine which side it's on (will be the least degrees)
								else if(radialRatio[minRadIdxCornerIdx] >= radialRatio[maxRadIdxCornerIdx])	
								{
									spansMinToMaxIndices = true;
								}
								else
								{
									spansMinToMaxIndices = false;
								}

								if(spansMinToMaxIndices)
								{
									for (s32 k=minRadIdx; k<=maxRadIdx; k++)
									{
										naAssertf(k>=0 && k<8, "Index k (%d) is out of bounds of radialSpanned[]", k);
										if(k == minRadIdx)
										{
											radialCoverage[k] = Max(radialCoverage[k], 1.0f - radialRatio[minRadIdxCornerIdx]);
										}
										else if(k == maxRadIdx)
										{
											radialCoverage[k] = Max(radialCoverage[k], radialRatio[maxRadIdxCornerIdx]);
										}
										else
										{
											radialCoverage[k] = 1.0f;
										}
										radialSpanned[k] = true;
									}
								}
								else
								{
									for (s32 k=maxRadIdx; k<=(minRadIdx+8); k++)
									{
										s32 index = k%8;
										naAssertf(index>=0 && index<8, "index (%d) is out of bounds of radialSpanned[]", index);
										if(index == minRadIdx)
										{
											radialCoverage[index] = Max(radialCoverage[index], radialRatio[minRadIdxCornerIdx]);
										}
										else if(index == maxRadIdx)
										{
											radialCoverage[index] = Max(radialCoverage[index], 1.0f - radialRatio[maxRadIdxCornerIdx]);
										}
										else
										{
											radialCoverage[index] = 1.0f;
										}
										radialSpanned[index] = true;
									}
								}
							}
						}
					}

					// Slightly tricky, pass in the InteriorSettings and roomIdx for the room we're in to get the distances and damping etc.
					// and pass in the portal corners for the portal we got while cycling through the limbo room, or roomIdx of 0.
					const f32 exteriorBlockage = CalculateExteriorBlockageForPortal(cameraPos, interiorSettings, interiorRoom, &portalCorners);
					f32 entityBlockage = 0.0f;
					naOcclusionPortalInfo* portalInfo = GetPortalInfoFromParameters(GetUniqueProxyHashkey(intInst->GetProxy()), INTLOC_ROOMINDEX_LIMBO, portalIdx);
					if(portalInfo)
					{
						entityBlockage = portalInfo->GetEntityBlockage(interiorSettings, interiorRoom, true);
					}
					const f32 portalOccluded = Lerp(entityBlockage, exteriorBlockage, 1.0f);

					// go through all portal-spanned exterior occlusion zones, and if they're not occluded, mark that in the overall one
					for (s32 elev = 0; elev < g_audNumOccZoneElevations; elev++)
					{
						for (s32 rad = 0; rad < g_audNumOccZoneRadials; rad++)
						{
							if (radialSpanned[rad] && elevationSpanned[elev])
							{
								// Factor in how much of the space this portal actually covers (radial only right now)
								// Scale the coverage, we don't want to mostly occlude a radial with a door because it only takes up 1/3 of the radial area
								const f32 radialCoverageScaled = Clamp(radialCoverage[rad] * g_RadialCoverageScalar, 0.0f, 1.0f);
								const f32 radialOcclusion = Lerp(radialCoverageScaled, 1.0f, portalOccluded);
								exteriorOcclusionFactor[elev][rad] = Min(radialOcclusion, exteriorOcclusionFactor[elev][rad]);
							}
						}
					}
				}
			}
		}
	}
}

f32 naOcclusionManager::CalculateExteriorBlockageForPortal(const Vector3& listenerPosition, const InteriorSettings* interiorSettings, const InteriorRoom* intRoom, 
															const fwPortalCorners* portalCorners, f32 currentPathDistance)
{
	// Set ourselves to be fully occluded if we don't have valid data or we're set to ignore portals 
	f32 portalOccluded = 1.0f;

	if(portalCorners && interiorSettings && intRoom)
	{
		// We have valid data so set ourselves to be fully un-occluded, and override it if we are going to use the distance metrics
		portalOccluded = 0.0f;

		// For big rooms where you don't want sounds leaking in from across the room, you can set distances that a portal will let in outside sound.
		// If those distances are set to the same value then we don't want to use distance as a factor.
		f32 distanceToFullyIncludePortalFactor = intRoom->DistanceFromPortalForOcclusion;
		f32 distanceToNotIncludePortalFactor = intRoom->DistanceFromPortalFadeDistance;

		if(distanceToFullyIncludePortalFactor != distanceToNotIncludePortalFactor)
		{
			f32 distanceToPortal = portalCorners->CalcDistanceToPoint(RCC_VEC3V(listenerPosition)).Getf();
			distanceToPortal += currentPathDistance;

			// If we're within the distance to FullyInclude the portal's occlusion factor, then we use the portals occlusion factor
			// If we're between the distanceToFullyIncludePortalFactor, and distanceToNotIncludePortalFactor then
			// we get a ratio (0-1) of where we are within those distance, and then use that percentage of the portals occlusion factor
			// If we're above the distanceToNotIncludePortalFactor we stop including it and it has no affect on the m_ExteriorOcclusionFactor
			f32 influenceOfThisPortal = distanceToPortal - distanceToFullyIncludePortalFactor;
			influenceOfThisPortal /= (distanceToNotIncludePortalFactor - distanceToFullyIncludePortalFactor);
			influenceOfThisPortal = Clamp(influenceOfThisPortal, 0.0f, 1.0f);
			influenceOfThisPortal = 1.0f - influenceOfThisPortal;

			//Factor this into the portalOccluded value, which in turn affects the m_ExteriorOcclusionFactor
			portalOccluded = 1.0f - influenceOfThisPortal;
		}
	}

	return portalOccluded;
}

f32 naOcclusionManager::GetExteriorOcclusionForDirection(audAmbienceDirection direction)
{
	naAssertf(direction < 4, "naEnvironmentGroupManager::GetExteriorOcclusionForDirection Bad audAmbienceDirection passed trying to get exterior occlusion: %u", direction);

	// Get the 2 radial that make up the 90 degree fov that define the direction we passed in, so for North we want 7 and 0, and East 1 and 2
	s32 leftRadial = ((direction * (g_audNumOccZoneRadials/4)) + (g_audNumOccZoneRadials - 1)) % g_audNumOccZoneRadials;
	s32 rightRadial = direction * (g_audNumOccZoneRadials/4);

	// Get the least amount of occlusion for each radial and then average them together.  A small window will allow a lot of outside sound in, and
	// the distance settings on the InteriorSettings during the m_ExteriorOcclusionFactor computation will keep it from letting in too much outside sound.
	f32 leftExteriorOcclusion = 1.0f;
	f32 rightExteriorOcclusion = 1.0f;
	for(u32 elev = 0; elev < g_audNumOccZoneElevations; elev++)
	{
		leftExteriorOcclusion = Min(leftExteriorOcclusion, m_ExteriorOcclusionFactor[elev][leftRadial]);
		rightExteriorOcclusion = Min(rightExteriorOcclusion, m_ExteriorOcclusionFactor[elev][rightRadial]);
	}

	f32 exteriorOcclusion = (leftExteriorOcclusion + rightExteriorOcclusion) * 0.5f;

	return exteriorOcclusion;	
}

void naOcclusionManager::UpdateBlockedFactors()
{

#if __BANK
	for (s32 i=0; i<4; i++)
	{
		m_BlockedInDirectionSmoother[i].SetRate(g_BlockedSmoothRate/1000.0f);
	}
#endif

	// Get an average of the AUD_OCC_PROBE_ELEV_W, _WWN, _NW, and _NNW probe distance to blocked values
	for(s32 dir = 0; dir < 4; dir++)
	{
		// Look at the 7 radials that are in the 90 field of view of each direction, so east = 45 to 135
		f32 averageBlockedW = 0.0f;
		for(s32 radial = -3; radial <= 3; radial++)
		{
			// This will give you the indices based on a 24 spoke wheel in the direction you're looking, so for east you get 3 - 9
			s32 radial24 = ((dir * (g_audNumLevelProbeRadials/4)) + radial + g_audNumLevelProbeRadials) % g_audNumLevelProbeRadials;
			averageBlockedW += m_DistanceToBlockedCurve.CalculateValue(GetWallDistance(AUD_OCC_PROBE_ELEV_W, radial24));
		}
		averageBlockedW /= 7.0f;

		// Look at the 3 upper-mid radials in the 90 degree field of view
		f32 averageBlockedWWN = 0.0f;
		f32 averageBlockedNW = 0.0f;
		for(s32 radial = -1; radial <= 1; radial++)
		{
			// This will give you the indices based on a 8 spoke wheel in the direction you're looking, so for east you get 1 - 3
			s32 radial8 = ((dir * (g_audNumMidProbeRadials/4)) + radial + g_audNumMidProbeRadials) % g_audNumMidProbeRadials;
			averageBlockedWWN += m_DistanceToBlockedCurve.CalculateValue(GetWallDistance(AUD_OCC_PROBE_ELEV_WWN, radial8));
			averageBlockedNW += m_DistanceToBlockedCurve.CalculateValue(GetWallDistance(AUD_OCC_PROBE_ELEV_NW, radial8));
		}
		averageBlockedWWN /= 3.0f;
		averageBlockedNW /= 3.0f;

		// Look at the 3 lower-mid radials
		f32 averageBlockedWWS = 0.0f;
		for(s32 radial = -1; radial <= 1; radial++)
		{
			// This will give you the indices based on a 8 spoke wheel in the direction you're looking, so for east you get 1 - 3
			s32 radial8 = ((dir * (g_audNumMidProbeRadials/4)) + radial + g_audNumMidProbeRadials) % g_audNumMidProbeRadials;
			averageBlockedWWS += m_DistanceToBlockedCurve.CalculateValue(GetWallDistance(AUD_OCC_PROBE_ELEV_WWS, radial8));
		}
		averageBlockedWWS /= 3.0f;

		s32 radial4 = dir;
		f32 averageBlockedNNW = m_DistanceToBlockedCurve.CalculateValue(GetWallDistance(AUD_OCC_PROBE_ELEV_NNW, radial4));

		// For now just average the elevations, without any weights
		const f32 averageBlocked = (averageBlockedW + averageBlockedWWN + averageBlockedNW + averageBlockedNNW + averageBlockedWWS) / 5.0f;
		naAssertf(averageBlocked >= 0.0f && averageBlocked <= 1.0f, "Invalid averageBlocked metric when updating blocked factor");
		m_BlockedInDirection[dir] = m_AverageBlockedToBlockedFactor.CalculateValue(averageBlocked);
		m_BlockedInDirection[dir] = m_BlockedInDirectionSmoother[dir].CalculateValue(m_BlockedInDirection[dir], fwTimer::GetTimeInMilliseconds());
		m_BlockedLinearVolume[dir] = m_BlockedToLinearVolume.CalculateValue(m_BlockedInDirection[dir]);
	}
}

f32 naOcclusionManager::GetBlockedFactorForDirection(audAmbienceDirection sector) const
{
	naAssertf(sector<AUD_AMB_DIR_LOCAL, "naEnvironmentGroupManager::GetBlockedFactorForDirection Invalid audAmbienceDirection passed into GetBlockedFactorForDirection; out of bounds");
	return m_BlockedInDirection[sector];
}

f32 naOcclusionManager::GetBlockedLinearVolumeForDirection(audAmbienceDirection sector) const
{
	naAssertf(sector<AUD_AMB_DIR_LOCAL, "Invalid audAmbienceDirection passed into GetBlockedLinearVolumeForDirection; out of bounds");
	return m_BlockedLinearVolume[sector];
}

void naOcclusionManager::UpdateOutsideWorldOcclusion()
{
	f32 vehicleOWO = 0.0f, interiorOWO = 0.0f, minInteriorOWOInDirection = 1.0f;

	if(audNorthAudioEngine::GetGtaEnvironment()->IsPlayerInVehicle())
	{
		CVehicle* veh = CGameWorld::FindLocalPlayerVehicle();
		if(!veh)
		{
			veh = audPedAudioEntity::GetBJVehicle();
		}
		if(veh)
		{
			vehicleOWO = veh->GetVehicleAudioEntity()->ComputeOutsideWorldOcclusion();
		}
	}

	// For interiors figure out which direction is the least occluded and use that one
	if(audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior())
	{
		interiorOWO = 1.f;
		for(u32 dir = 0; dir < 4; dir++)
		{
			minInteriorOWOInDirection = Min(minInteriorOWOInDirection, GetExteriorOcclusionForDirection((audAmbienceDirection)dir));
		}
		minInteriorOWOInDirection = Lerp(minInteriorOWOInDirection, g_MinBlockedInInterior, 1.0f);
	}
	else
	{
		minInteriorOWOInDirection = 0.0f;
	}

	m_OutsideWorldOcclusion = m_OutsideWorldOcclusionSmoother.CalculateValue(Max(vehicleOWO, interiorOWO), fwTimer::GetTimeInMilliseconds());
	m_OutsideWorldOcclusionIgnoringInteriors = m_OutsideWorldOcclusionSmootherIgnoringInteriors.CalculateValue(vehicleOWO, fwTimer::GetTimeInMilliseconds());
	m_OutsideWorldOcclusionAfterPortals = minInteriorOWOInDirection;
}

f32 naOcclusionManager::GetOcclusionFactorForRadialAndElevation(s32 occlusionZoneRadialIndex, s32 occlusionZoneElevationIndex, const Vector3& position)
{
	f32 occlusionFactor = 1.0f;

	// are we within the box for x
	bool clearX = false;
	bool clearXextend = false;
	if (occlusionZoneRadialIndex<4)
	{
		if (m_OcclusionBoundingBoxes[occlusionZoneElevationIndex][occlusionZoneRadialIndex].x > position.x)
		{
			clearX = true;
			clearXextend = true;
		}
		else if (m_OcclusionBoundingBoxesExtend[occlusionZoneElevationIndex][occlusionZoneRadialIndex].x > position.x)
		{
			clearXextend = true;
		}
	}
	else
	{
		if (m_OcclusionBoundingBoxes[occlusionZoneElevationIndex][occlusionZoneRadialIndex].x < position.x)
		{
			clearX = true;
			clearXextend = true;
		}
		else if (m_OcclusionBoundingBoxesExtend[occlusionZoneElevationIndex][occlusionZoneRadialIndex].x < position.x)
		{
			clearXextend = true;
		}
	}

	// are we within the box for y
	bool clearY = false;
	bool clearYextend = false;
	if (occlusionZoneRadialIndex<2 || occlusionZoneRadialIndex>5)
	{
		if (m_OcclusionBoundingBoxes[occlusionZoneElevationIndex][occlusionZoneRadialIndex].y > position.y)
		{
			clearY = true;
			clearYextend = true;
		}
		else if (m_OcclusionBoundingBoxesExtend[occlusionZoneElevationIndex][occlusionZoneRadialIndex].y > position.y)
		{
			clearYextend = true;
		}
	}
	else
	{
		if (m_OcclusionBoundingBoxes[occlusionZoneElevationIndex][occlusionZoneRadialIndex].y < position.y)
		{
			clearY = true;
			clearYextend = true;
		}
		else if (m_OcclusionBoundingBoxesExtend[occlusionZoneElevationIndex][occlusionZoneRadialIndex].y < position.y)
		{
			clearYextend = true;
		}
	}

	// are we within the box for z
	bool clearZ = false;
	bool clearZextend = false;
	if (occlusionZoneElevationIndex<2)
	{
		if (m_OcclusionBoundingBoxes[occlusionZoneElevationIndex][occlusionZoneRadialIndex].z > position.z)
		{
			clearZ = true;
			clearZextend = true;
		}
		else if (m_OcclusionBoundingBoxesExtend[occlusionZoneElevationIndex][occlusionZoneRadialIndex].z > position.z)
		{
			clearZextend = true;
		}
	}
	else
	{
		// The g_ProbeZDistanceTolerance is to prevent occluding when something is down a hill, and the probes are hitting before
		// the environmentGroup, thereby causing the clearZ to be false.  We use a small value so we can still occlude cars going under tunnels
		if (m_OcclusionBoundingBoxes[occlusionZoneElevationIndex][occlusionZoneRadialIndex].z < position.z + g_ProbeZDistanceTolerance)
		{
			clearZ = true;
			clearZextend = true;
		}
		else if (m_OcclusionBoundingBoxesExtend[occlusionZoneElevationIndex][occlusionZoneRadialIndex].z < position.z + g_ProbeZDistanceTolerance)
		{
			clearZextend = true;
		}
	}

	if (clearX && clearY && clearZ)
	{
		occlusionFactor = 0.0f;
	}
	else if (clearXextend && clearYextend && clearZextend)
	{
		occlusionFactor = 0.5f;
	}

	return occlusionFactor;
}

f32 naOcclusionManager::GetBoundingBoxOcclusionFactorForPosition(const Vector3& pos)
{
	// Get the radial and elevation index for the zone in which the sound source is positioned, and a ratio of where it lies within the zone.
	f32 radialRatio, elevationRatio;	// 0.0f = at the current radial/elevation index, 1.0f = at the next radial index to the right or elevation index below
	s32 primaryRadialOcclusionZoneIndex, primaryElevationOcclusionZoneIndex;

	Vector3 listenerPos = audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition();
#if __BANK
	if(g_DebugProbeOcclusionFromPlayer)
	{
		CPed* player = FindPlayerPed();
		if(player)
		{
			listenerPos = VEC3V_TO_VECTOR3(player->GetTransform().GetPosition());
		}
	}
#endif

	GetOcclusionZoneIndices(listenerPos, pos, radialRatio, primaryRadialOcclusionZoneIndex, elevationRatio, primaryElevationOcclusionZoneIndex);

	// Since the indices actually define zones, use the data from the zone the sound is in along with the adjacent zones proportional to the angle.
	s32 secondaryRadialOcclusionZoneIndex = GetSecondaryRadialOcclusionZoneIndex(primaryRadialOcclusionZoneIndex, radialRatio);
	s32 secondaryElevationOcclusionZoneIndex = GetSecondaryElevationOcclusionZoneIndex(primaryElevationOcclusionZoneIndex, elevationRatio);

	// Get the averaged probe data for the 4 zones.  This function will return a value based on whether the sound source is within computed bounds we get from the
	// level, mid and vertical probe intersections.  See UpdateLocalEnvironmentMetricsForNewCameraPosition() for how that value is calculated.
	// pR = primaryRadial sR = secondaryRadial pE = primaryElevation sE = secondaryElevation, so sRpE == secondary radial at the primary elevation
	f32 pRpE = GetOcclusionFactorForRadialAndElevation(primaryRadialOcclusionZoneIndex, primaryElevationOcclusionZoneIndex, pos);
	f32 pRsE = GetOcclusionFactorForRadialAndElevation(primaryRadialOcclusionZoneIndex, secondaryElevationOcclusionZoneIndex, pos);
	f32 sRpE = GetOcclusionFactorForRadialAndElevation(secondaryRadialOcclusionZoneIndex, primaryElevationOcclusionZoneIndex, pos);
	f32 sRsE = GetOcclusionFactorForRadialAndElevation(secondaryRadialOcclusionZoneIndex, secondaryElevationOcclusionZoneIndex, pos);

	f32 amountToUseSecondaryRadialZone = GetAmountToUseSecondaryRadialOcclusionZone(radialRatio);
	f32 amountToUseSecondaryElevationZone = GetAmountToUseSecondaryElevationOcclusionZone(primaryElevationOcclusionZoneIndex, elevationRatio);

	// Combine the elevations so we have the data for 2 radials.
	f32 pROcclusion = Lerp(amountToUseSecondaryElevationZone, pRpE, pRsE);
	f32 sROcclusion = Lerp(amountToUseSecondaryElevationZone, sRpE, sRsE);

	// Finally combine the 2 radials into a single occlusion value.
	f32 occlusionFactor = Lerp(amountToUseSecondaryRadialZone, pROcclusion, sROcclusion);

	naAssertf(occlusionFactor >= 0.0f && occlusionFactor <= 1.0f, "Bad bounding box occlusion factor: %f", occlusionFactor);

	return occlusionFactor;
}

bool naOcclusionManager::ComputeOcclusionPathMetrics(naEnvironmentGroup* environmentGroup, Vector3& playPos, f32& pathDistance, f32& occlusionFactor)
{
	bool successfullyProcessedPaths = false;

	naAssertf(environmentGroup, "naEnvironmentGroupManager::PopulateOcclusionMetricsFromPathTree Passed NULL occlusion group");

	// It's ok if our CInteriorInst*'s are NULL, because we handle that as being "outside"
	const CInteriorInst* listenerIntInst = audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorInstance();
	const s32 listenerRoomIdx = audNorthAudioEngine::GetGtaEnvironment()->GetInteriorRoomId();

	const CInteriorInst* envGroupIntInst = environmentGroup->GetInteriorInst();
	const s32 envGroupRoomIdx = environmentGroup->GetRoomIdx();

	// Check on the validity because the CInteriorInst* may not have streamed in yet and make sure we're not going from outside->outside.
	// Or the paths are out of date, and we have a roomIdx that's outside the bounds of the IntInst room array
	// Again, it's ok if we have a NULL CInteriorInst*, but if we do make sure that we have a valid roomIdx for that interior.
	if(listenerRoomIdx != INTLOC_INVALID_INDEX && envGroupRoomIdx != INTLOC_INVALID_INDEX
		&& naVerifyf(listenerRoomIdx != INTLOC_ROOMINDEX_LIMBO || envGroupRoomIdx != INTLOC_ROOMINDEX_LIMBO, "Shouldn't be going outside->outside with portal occlusion")
		&& (!listenerIntInst || naVerifyf(listenerRoomIdx < listenerIntInst->GetNumRooms(), "Bad roomIdx: %d on listener in CInteriorInst: %s", listenerRoomIdx, listenerIntInst->GetModelName()))
		&& (!envGroupIntInst || naVerifyf(envGroupRoomIdx < envGroupIntInst->GetNumRooms(), "Bad roomIdx: %d on envGroup in CInteriorInst: %s", envGroupRoomIdx, envGroupIntInst->GetModelName())))
	{
		// Figure out which interior holds the paths for the combination.
		naOcclusionInteriorInfo** interiorInfoPtr = NULL;

		if(listenerIntInst && listenerIntInst->GetProxy() && g_fwMetaDataStore.IsValidSlot(listenerIntInst->GetProxy()->GetAudioOcclusionMetadataFileIndex()))
		{
			if(g_fwMetaDataStore.HasObjectLoaded(listenerIntInst->GetProxy()->GetAudioOcclusionMetadataFileIndex()))
			{
				interiorInfoPtr = m_LoadedInteriorInfos.Access(listenerIntInst->GetProxy());
			}
		}
		else if(envGroupIntInst && envGroupIntInst->GetProxy() && g_fwMetaDataStore.IsValidSlot(envGroupIntInst->GetProxy()->GetAudioOcclusionMetadataFileIndex()))
		{
			if(g_fwMetaDataStore.HasObjectLoaded(envGroupIntInst->GetProxy()->GetAudioOcclusionMetadataFileIndex()))
			{
				interiorInfoPtr = m_LoadedInteriorInfos.Access(envGroupIntInst->GetProxy());
			}
		}

		if(interiorInfoPtr && *interiorInfoPtr)
		{
			// Here's where we access a map which uses the key to get an index into the interiorData
			naOcclusionPathNode* pathTreeRoot = NULL;

			// First try and get the path node at the forced path depth and then go down from there
			if(environmentGroup->GetForceMaxPathDepth())
			{
				u8 pathDepthToUse = environmentGroup->GetMaxPathDepth();
				while(!pathTreeRoot && pathDepthToUse <= environmentGroup->GetMaxPathDepth() && pathDepthToUse <= sm_MaxOcclusionPathDepth && pathDepthToUse > 0)
				{
					const u32 key = GenerateOcclusionPathKey(listenerIntInst, listenerRoomIdx, envGroupIntInst, envGroupRoomIdx, pathDepthToUse);
					pathTreeRoot = (*interiorInfoPtr)->GetPathNodeFromKey(key);
					pathDepthToUse--;
				}
			}
			else
			{
				// Otherwise try the least path depth possible, and in the case where we're at our full depth and can't find a path, go into the extended paths
				u8 pathDepthToUse = Min(environmentGroup->GetMaxPathDepth(), sm_MaxOcclusionFullPathDepth);
				while(!pathTreeRoot && pathDepthToUse <= environmentGroup->GetMaxPathDepth() && pathDepthToUse <= sm_MaxOcclusionPathDepth)
				{
					const u32 key = GenerateOcclusionPathKey(listenerIntInst, listenerRoomIdx, envGroupIntInst, envGroupRoomIdx, pathDepthToUse);
					pathTreeRoot = (*interiorInfoPtr)->GetPathNodeFromKey(key);
					pathDepthToUse++;
				}
			}

			if(pathTreeRoot)
			{
				// We determine the player/listener's CInteriorInst/Room information from the CPortalVisTracker class, which uses the camera position.
				// We also need to use the camera position for the StartPosition of the occlusion path so we don't end up starting from a position
				// outside the starting room, which will yield acute angles due to backtracking and occlude heavily.
				Vector3 startPos = camInterface::GetPos();

				Vector3 simulatedPlayPos(0.0f, 0.0f, 0.0f);

				// As we use the same path node data tree to avoid allocating/deallocating memory, reset the data
				m_OcclusionPathNodeDataRoot.ResetData();

				// our first node is the listener location 
				m_OcclusionPathNodeDataRoot.pathSegmentStart = startPos;
				m_OcclusionPathNodeDataRoot.pathSegmentEnd = startPos;

#if __BANK
				m_OcclusionPathNodeDataRoot.drawDebug = true;
#endif

				// We need to know where we start, so we can make sure we don't double back through this room when processing paths
				if(listenerIntInst)
				{
					m_OcclusionPathNodeDataRoot.interiorHash = GetUniqueProxyHashkey(listenerIntInst->GetProxy());
					m_OcclusionPathNodeDataRoot.roomIdx = listenerRoomIdx;
				}
				else
				{
					m_OcclusionPathNodeDataRoot.interiorHash = 0;
					m_OcclusionPathNodeDataRoot.roomIdx = INTLOC_ROOMINDEX_LIMBO;
				}
				
				// Get the mixed down child data and weights
				pathTreeRoot->ProcessOcclusionPathTree(&m_OcclusionPathNodeDataRoot, environmentGroup, simulatedPlayPos);
				if(m_OcclusionPathNodeDataRoot.usePath)
				{
					occlusionFactor = m_AngleFactorToOcclusionFactor.CalculateValue(m_OcclusionPathNodeDataRoot.angleFactor);
					occlusionFactor = Min(1.f, Lerp(m_OcclusionPathNodeDataRoot.blockageFactor, occlusionFactor, 1.0f));

					playPos = simulatedPlayPos;
					pathDistance = m_OcclusionPathNodeDataRoot.pathDistance;

					successfullyProcessedPaths = true;
				}
			}
#if YACHT_TV_OCCLUSION_HACK
			// This a really crazy edge case.  We're going from a pathDepth of 5 to a pathDepth of 6 from the TV room to the Bed3/Study.  
			// Our max path depth is 5, and we don't build paths that deep, so the portal occlusion isn't used, and it sets the occlusion to 1.0.  
			// However, it's smoothed so it ramps up to 1.0 over a second from .3 because there are no doors.  
			// Since we're no longer using the portal occlusion, it's not using the distance based on the path or adjusted position, but the actual world position.
			// That world position just happens to be right above you only a few meters away, so it momentarily snaps back in audibly until the occlusion smooths back to 1.0f.
			// The proper fix for this is increasing the max path depth globally, but adding a new field to InteriorSettings GO's defaulting to our current path depth,
			// but allowing certain interiors ( such as the yacht ) to go beyond that.
			else if(listenerIntInst && listenerIntInst->GetModelNameHash() == ATSTRINGHASH("apa_mpapa_yacht", 0x22428F46)
				&& envGroupIntInst && envGroupIntInst->GetModelNameHash() == ATSTRINGHASH("apa_mpapa_yacht", 0x22428F46))
			{
				const u32 listenerRoomHash = listenerIntInst->GetRoomHashcode(listenerRoomIdx);
				if((listenerRoomHash == ATSTRINGHASH("YachtRm_Bed3", 0xB79F3417) || listenerRoomHash == ATSTRINGHASH("YachtRm_Study", 0x37A8906D) || listenerRoomHash == ATSTRINGHASH("YachtRm_weebathroom", 0xDDFEF464))
					&& envGroupIntInst->GetRoomHashcode(envGroupRoomIdx) == ATSTRINGHASH("YachtRm_bar", 0x7C1E2711))
				{
					occlusionFactor = 1.0f;
					pathDistance = 25.0f;
					playPos.Set(V_ZERO);
					successfullyProcessedPaths = true;
				}
			}
#endif

#if __BANK
			if(g_AssertOnMissingOcclusionPaths)
			{
				naAssertf(pathTreeRoot, "Couldn't find a portal path for the given key %s %s to %s %s",
					listenerIntInst ? listenerIntInst->GetMloModelInfo()->GetModelName() : g_OcclusionPathOutsideStringName, 
					listenerIntInst ? listenerIntInst->GetRoomName(listenerRoomIdx) : "", 
					envGroupIntInst ? envGroupIntInst->GetMloModelInfo()->GetModelName() : g_OcclusionPathOutsideStringName,
					envGroupIntInst ? envGroupIntInst->GetRoomName(environmentGroup->GetRoomIdx()) : "");
			}
#endif
		}
	}

	return successfullyProcessedPaths;
}

f32 naOcclusionManager::ComputeVehicleOcclusion(const CVehicle* veh, const bool isInAir)
{
	// We know the vehicle is on the road if it's a land vehicle, so the only way we could be below it is if it's on an overpass
	// or in some very specific hill situations (those situations are mostly fixed via the curve variance)
	// So see what angle we're at and occlude accordingly
	f32 vehOcclusion = 0.0f;
	if(!isInAir && veh && veh->GetIsLandVehicle())
	{
		const Vec3V up = veh->GetTransform().GetUp();
		const Vec3V vehToList = g_AudioEngine.GetEnvironment().GetPanningListenerPosition() - veh->GetTransform().GetPosition();
		const Vec3V vehToListNorm = NormalizeSafe(vehToList, Vec3V(V_ZERO));
		if(!IsEqualAll(vehToListNorm, Vec3V(V_ZERO)))
		{
			const f32 cosAngle = Dot(up, vehToListNorm).Getf();
			const f32 angle = AcosfSafe(cosAngle);
			const f32 angleDegrees = angle * RtoD;
			vehOcclusion = m_VerticalAngleToVehicleOcclusion.CalculateValue(angleDegrees);
		}
	}
	return vehOcclusion;
}

u32 naOcclusionManager::GenerateOcclusionPathKey(const CInteriorInst* listenerIntInst, s32 listenerRoomIdx, const CInteriorInst* envGroupIntInst, s32 envGroupRoomIdx, const u32 pathDepth)
{
	if(listenerRoomIdx == INTLOC_INVALID_INDEX)
		listenerRoomIdx = INTLOC_ROOMINDEX_LIMBO;
	if(envGroupRoomIdx == INTLOC_INVALID_INDEX)
		envGroupRoomIdx = INTLOC_ROOMINDEX_LIMBO;

	// We want collisions between keys for limbo rooms, or outside, so Int1 RoomIdx 0 is the same room (outside) as Int2 RoomIdx 0
	// So start as though we're outside, and if we have valid interior data then use that instead.
	u32 listenerIntInstHash = g_OcclusionPathOutsideHashkey;
	u32 envGroupIntInstHash = g_OcclusionPathOutsideHashkey;
	u32 listenerRoomHash = 0;
	u32 envGroupRoomHash = 0;

	// If we have an interior, and we're not in room'limbo' or outside (room == 0), then get the hashkey for the Interior and the Room.
	// Because we have multiple instances of the same interior in different locations with the same name, use the positions to create a unique key that can separate them
	// In a way this is also good because if they move the interior we won't find the key, so we'll know to build new paths for the new location
	// We only do this when we have an interior and we're not in limbo, so this won't screw up deliberate collisions between different room limbo paths
	if(listenerIntInst && listenerIntInst->GetProxy() && listenerRoomIdx != INTLOC_ROOMINDEX_LIMBO)
	{
		listenerIntInstHash = GetUniqueProxyHashkey(listenerIntInst->GetProxy());
		listenerRoomHash = listenerIntInst->GetRoomHashcode(listenerRoomIdx);
	}

	if(envGroupIntInst && envGroupIntInst->GetProxy() && envGroupRoomIdx != INTLOC_ROOMINDEX_LIMBO)
	{
		envGroupIntInstHash = GetUniqueProxyHashkey(envGroupIntInst->GetProxy());
		envGroupRoomHash = envGroupIntInst->GetRoomHashcode(envGroupRoomIdx);
	}

	const u32 uniqueListenerHash = listenerIntInstHash ^ listenerRoomHash;
	const u32 uniqueEnvGroupHash = envGroupIntInstHash ^ envGroupRoomHash;

	// We need the Int1->Int2 path tree as well as the Int2->Int1 path tree, so forming the key this way
	// will make sure we don't get collisions between those trees.
	// Also avoids clashes between paths going from interiors with the same names to rooms with the same names
	const u32 pathKey = (uniqueListenerHash - uniqueEnvGroupHash) + pathDepth;

	return pathKey;

}

void naOcclusionManager::GetPortalDestinationInfo(const CInteriorInst* currentIntInst, const s32 currentRoomIdx, const s32 portalIdx, 
														CInteriorInst** destIntInstPtr, s32* destRoomIdxPtr, s32* linkedIntRoomLimboPortalIdxPtr)
{
	CInteriorInst* destIntInst = NULL;
	s32 destRoomIdx = INTLOC_INVALID_INDEX;
	s32 linkedIntRoomLimboPortalIdx = INTLOC_INVALID_INDEX;

	if(naVerifyf(currentIntInst, "naEnvironmentGroupManager::GetPortalDestinationInfo Passing NULL CInteriorInst* to GetPortalDestinationInfo")
		&& currentRoomIdx < currentIntInst->GetNumRooms() && portalIdx < currentIntInst->GetNumPortalsInRoom(currentRoomIdx)
		&& portalIdx != INTLOC_INVALID_INDEX)
	{
		CMloModelInfo* currentModelInfo = currentIntInst->GetMloModelInfo();
		if(naVerifyf(currentModelInfo, "naEnvironmentGroupManager::GetPortalDestinationInfo Unable to get CMloModelInfo in audio occlusion update"))
		{
			CPortalFlags portalFlags;
			u32 roomIdx1, roomIdx2;

			s32 globalPortalIdx = currentIntInst->GetPortalIdxInRoom(currentRoomIdx, (u32)portalIdx);
			currentModelInfo->GetPortalData(globalPortalIdx, roomIdx1, roomIdx2, portalFlags);

			if(!portalFlags.GetIsMirrorPortal())
			{
				// If it's a link portal, go through and get the room and CInteriorInst* info
				if(portalFlags.GetIsLinkPortal())
				{
					// Only look for link portals if we're not starting outside (roomIdx of 0 = limbo = outside).
					// This is because when our start is room = limbo, it'll look at these link portals, and since the
					// link portals will be going to another interior, not outside, we don't want to use them when we're outside trying to get back inside.
					if(currentRoomIdx != INTLOC_ROOMINDEX_LIMBO)
					{
						CPortalInst* portalInst = currentIntInst->GetMatchingPortalInst(currentRoomIdx, (u32)portalIdx);
						if(portalInst && portalInst->IsLinkPortal())
						{
							s32 linkedIntRoomLimboPortalIdxSigned;
							portalInst->GetDestinationData(currentIntInst, destIntInst, linkedIntRoomLimboPortalIdxSigned);
							linkedIntRoomLimboPortalIdx = linkedIntRoomLimboPortalIdxSigned;
							if(destIntInst)
							{
								destRoomIdx = destIntInst->GetDestThruPortalInRoom(INTLOC_ROOMINDEX_LIMBO, linkedIntRoomLimboPortalIdx);		//entry/exit portal so we know it's room 0
							}
						}
					}
				}
				else
				{
					//Figure out which of those is not the room we're in
					destRoomIdx = (s32)roomIdx1 == currentRoomIdx ? roomIdx2 : roomIdx1;
					destIntInst = const_cast<CInteriorInst*>(currentIntInst);
				}
			}
		}
	}

	if(naVerifyf(destIntInstPtr, "naEnvironmentGroupManager::GetPortalDestinationInfo Passed NULL CInteriorInst**"))
	{
		*destIntInstPtr = destIntInst;
	}
	if(naVerifyf(destRoomIdxPtr, "naEnvironmentGroupManager::GetPortalDestinationInfo Passed NULL s32* destRoomIdxPtr"))
	{
		*destRoomIdxPtr = destRoomIdx;
	}
	if(linkedIntRoomLimboPortalIdxPtr)
	{
		*linkedIntRoomLimboPortalIdxPtr = linkedIntRoomLimboPortalIdx;
	}
}

naOcclusionPortalInfo* naOcclusionManager::GetPortalInfoFromParameters(const u32 intProxyHashkey, const s32 roomIdx, const u32 portalIdx)
{
	naOcclusionPortalInfo* portalInfo = NULL;

	naOcclusionPortalInfo::Pool* pool = naOcclusionPortalInfo::GetPool();
	if(pool)
	{
		s32 poolCount = pool->GetSize();
		while(poolCount--)
		{
			portalInfo = pool->GetSlot(poolCount);
			if(portalInfo)
			{
				u32 currPortalIdx = (u32)portalInfo->GetPortalIdx();
				s32 currRoomIdx = portalInfo->GetRoomIdx();
				u32 currIntHash = portalInfo->GetUniqueInteriorProxyHashkey();
				if(intProxyHashkey == currIntHash && roomIdx == currRoomIdx && portalIdx == currPortalIdx)
				{
					return portalInfo;
				}
			}
		}
	}

	return NULL;
}

bool naOcclusionManager::LoadOcclusionDataForInterior(const CInteriorProxy* intProxy)
{
	if(intProxy && intProxy->GetInteriorInst() && intProxy->GetInteriorInst()->IsPopulated())
	{
		naOcclusionInteriorMetadata* interiorMetadata = g_fwMetaDataStore.Get(strLocalIndex(intProxy->GetAudioOcclusionMetadataFileIndex()))->GetObject<naOcclusionInteriorMetadata>();
		if(interiorMetadata)
		{
			// See if we have enough space in the path node pool to load this in, otherwise we need to wait until others are unloaded
			// This happens on switches where the old interiors haven't been unloaded yet
			if(interiorMetadata->m_PathNodeList.GetCount() <= naOcclusionPathNode::GetPool()->GetNoOfFreeSpaces())
			{
				// Create a new entry for this interior so later we can deallocate all the data from the pools
				naOcclusionInteriorInfo* interiorInfo = naOcclusionInteriorInfo::Allocate();
				if(interiorInfo)
				{
					naDebugf2("Loading occlusion data for interior: %s", intProxy->GetModelName());

					naOcclusionInteriorInfo** interiorInfoPtr = &m_LoadedInteriorInfos[intProxy];
					*interiorInfoPtr = interiorInfo;
					bool initSuccessful = interiorInfo->InitFromMetadata(interiorMetadata);
					if(initSuccessful)
					{
#if __ASSERT
						m_FailedInteriorLoadFrameCountMap.Delete(intProxy);
#endif
						return true;
					}
					else
					{
#if __ASSERT
						if(!g_PlayerSwitch.IsActive())
						{
							u32* numFramesLoadFailed = m_FailedInteriorLoadFrameCountMap.Access(intProxy);
							if(numFramesLoadFailed)
							{
								(*numFramesLoadFailed)++;
								naAssertf(*numFramesLoadFailed < g_NumFramesLoadFailedToAssert, 
									"Interior occlusion data failed to load for %u frames player switch wasn't taking place.  NumPortalInfosAbovePoolSize: %u NumPathNodesAbovePoolSize: %u",
									g_NumFramesLoadFailedToAssert, interiorInfo->GetNumPortalInfosAbovePoolSize(), interiorInfo->GetNumPathNodesAbovePoolSize());
							}
							else
							{
								m_FailedInteriorLoadFrameCountMap.Insert(intProxy, 1);
							}
						}
#endif

						UnloadOcclusionDataForInterior(intProxy);
					}
				}
			}
#if __ASSERT
			// Track how many frames we weren't able to load the interior while a player switch wasn't active
			else if(!g_PlayerSwitch.IsActive())
			{
				u32* numFramesLoadFailed = m_FailedInteriorLoadFrameCountMap.Access(intProxy);
				if(numFramesLoadFailed)
				{
					(*numFramesLoadFailed)++;
					naAssertf(*numFramesLoadFailed < g_NumFramesLoadFailedToAssert,
						"Interior occlusion data failed to load for %u frames player switch wasn't taking place.  We don't have enough room in the naOcclusionPathNode pool.  Over by %u",
						g_NumFramesLoadFailedToAssert, interiorMetadata->m_PathNodeList.GetCount() - naOcclusionPathNode::GetPool()->GetNoOfFreeSpaces());
				}
				else
				{
					m_FailedInteriorLoadFrameCountMap.Insert(intProxy, 1);
				}
			}
#endif
		}
	}

	return false;
}

void naOcclusionManager::UnloadOcclusionDataForInterior(const CInteriorProxy* intProxy)
{
	if(!intProxy)
	{
		return;
	}

	naDebugf2("Unloading occlusion data for interior: %s", intProxy->GetModelName());

	// Need to delete all the data for this interior out of the pools, and then delete this interior from the map
	naOcclusionInteriorInfo** interiorInfoPtr = m_LoadedInteriorInfos.Access(intProxy);
	if(interiorInfoPtr && *interiorInfoPtr)
	{
		delete (*interiorInfoPtr);
		naVerifyf(m_LoadedInteriorInfos.Delete(intProxy), "Couldn't find CInteriorProxy: %s to remove in our loaded interior map", intProxy->GetModelName());
	}
}

bool naOcclusionManager::AreOcclusionPathsLoadedForInterior(CInteriorProxy* intProxy)
{
	naOcclusionInteriorInfo** interiorInfoPtr = m_LoadedInteriorInfos.Access(intProxy);
	if(interiorInfoPtr && *interiorInfoPtr)
	{
		return true;
	}
	return false;
}

u32 naOcclusionManager::GetUniqueProxyHashkey(const CInteriorProxy* intProxy) const
{
	u32 uniqueHashkey = 0;
	if(intProxy)
	{
		Vec3V pos;
		intProxy->GetPosition(pos);
		uniqueHashkey = intProxy->GetNameHash() ^ (u32)(pos.GetXf()*100.0f) ^ (u32)(pos.GetYf()*100.0f) ^ (u32)(pos.GetZf()*100.0f);
	}

	return uniqueHashkey;
}

CInteriorProxy* naOcclusionManager::GetInteriorProxyFromUniqueHashkey(const u32 hashkey)
{
	CInteriorProxy::Pool* pool = CInteriorProxy::GetPool();
	if(pool)
	{
		s32 numSlots = pool->GetSize();
		for(s32 slot = 0; slot < numSlots; slot++)
		{
			CInteriorProxy* intProxy = pool->GetSlot(slot);
			if(intProxy)
			{
				// SP interiors and MP interiors will have the same unique hashkey, but have different slots in the CInteriorProxy pool.
				// Checking against GetIsDisabled() will find the correct one based on the state.
				if(GetUniqueProxyHashkey(intProxy) == hashkey && !intProxy->GetIsDisabled())
				{
					return intProxy;
				}
			}
		}
	}

	return NULL;
}

f32 naOcclusionManager::GetExteriorOcclusionFor3DPosition(const Vector3& pos)
{
	// Get the radial and elevation index for the zone in which the sound source is positioned, and a ratio of where it lies within the zone.
	f32 radialRatio, elevationRatio;	// 0.0f = at the current radial/elevation index, 1.0f = at the next radial index to the right or elevation index below
	s32 primaryRadialOcclusionZoneIndex, primaryElevationOcclusionZoneIndex;

	GetOcclusionZoneIndices(audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition(), pos, 
		radialRatio, primaryRadialOcclusionZoneIndex, elevationRatio, primaryElevationOcclusionZoneIndex);

	// Since the indices actually define zones, use the data from the zone the sound is in along with the adjacent zones proportional to the angle.
	s32 secondaryRadialOcclusionZoneIndex = GetSecondaryRadialOcclusionZoneIndex(primaryRadialOcclusionZoneIndex, radialRatio);
	s32 secondaryElevationOcclusionZoneIndex = GetSecondaryElevationOcclusionZoneIndex(primaryElevationOcclusionZoneIndex, elevationRatio);

	// Get the portal data for each of the 4 zones.  This function will return a value based on whether there are open portals going outside within this zone.
	// eof = exterior occlusion factor, pRpE = primaryRadialprimaryElevation
	f32 eofpRpE = GetExteriorOcclusionFactorForZone(primaryElevationOcclusionZoneIndex, primaryRadialOcclusionZoneIndex);
	f32 eofpRsE = GetExteriorOcclusionFactorForZone(secondaryElevationOcclusionZoneIndex, primaryRadialOcclusionZoneIndex);
	f32 eofsRpE = GetExteriorOcclusionFactorForZone(primaryElevationOcclusionZoneIndex, secondaryRadialOcclusionZoneIndex);
	f32 eofsRsE = GetExteriorOcclusionFactorForZone(secondaryElevationOcclusionZoneIndex, secondaryRadialOcclusionZoneIndex);

	f32 amountToUseSecondaryRadialZone = GetAmountToUseSecondaryRadialOcclusionZone(radialRatio);
	f32 amountToUseSecondaryElevationZone = GetAmountToUseSecondaryElevationOcclusionZone(primaryElevationOcclusionZoneIndex, elevationRatio);

	// Combine the elevations so we have the data for 2 radials.
	f32 eofpR = (eofpRpE * (1.0f - amountToUseSecondaryElevationZone)) + (eofpRsE * amountToUseSecondaryElevationZone);
	f32 eofsR = (eofsRpE * (1.0f - amountToUseSecondaryElevationZone)) + (eofsRsE * amountToUseSecondaryElevationZone);

	// Finally combine the 2 radials into a single occlusion value.
	f32 occlusionFactor = (eofpR * (1.0f - amountToUseSecondaryRadialZone)) + (eofsR * amountToUseSecondaryRadialZone);

	naAssertf(occlusionFactor >= 0.0f && occlusionFactor <= 1.0f, "Bad exterior occlusion factor: %f", occlusionFactor);

	return occlusionFactor;
}

f32 naOcclusionManager::GetExteriorOcclusionFor2DPosition(const Vector2& position)
{
	// Where is it relative to us
	const Vector3 listenerPosition = audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition();
	Vector2 directionRelative = position - Vector2(listenerPosition.x, listenerPosition.y);
	directionRelative.NormalizeSafe();
	f32 exteriorOcclusionFactor = 0.0f;
	for (u32 i=0; i<4; i++) // 4 directional vectors
	{
		Vector2 direction(g_Directions[i].x, g_Directions[i].y);
		f32 dotProd = directionRelative.Dot(direction);
		// Don't care about ones more than 90degs away
		dotProd = Max(0.0f, dotProd);
		// Scale our contribution by the elevation
		f32 contribution = dotProd * dotProd;
		exteriorOcclusionFactor += (contribution * GetExteriorOcclusionForDirection(audAmbienceDirection(i)));
	}
	exteriorOcclusionFactor = Clamp(exteriorOcclusionFactor, 0.0f, 1.0f);	// We'll end up with some values just above 1 due to floating point error.
	return exteriorOcclusionFactor;	
}

f32 naOcclusionManager::GetBlockedFactorFor3DPosition(const Vector3& position)
{
	// Where is it relative to us
	Vector3 directionRelative = position - audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition();
	directionRelative.NormalizeSafe();
	Vector3 positionOnUnitCircle = directionRelative;
	// see how elevated we are - elevation==1 implies we're not at all elevated!
	positionOnUnitCircle.z = 0.0f;
	f32 elevation = positionOnUnitCircle.Mag2() / directionRelative.Mag2();
	positionOnUnitCircle.NormalizeSafe();
	f32 blockedFactor = 0.0f;
	for (u32 i=0; i<4; i++) // 4 directional vectors
	{
		f32 dotProd = positionOnUnitCircle.Dot(g_Directions[i]);
		// Don't care about ones more than 90degs away
		dotProd = Max(0.0f, dotProd);
		// Scale our contribution by the elevation
		f32 contribution = ((1.0f - elevation) * (1.0f/4.0f)) + (elevation * dotProd * dotProd);
		blockedFactor += (contribution * m_BlockedInDirection[i]);
	}
	
	blockedFactor = Clamp(blockedFactor, 0.0f, 1.0f);
	return blockedFactor;	
}

f32 naOcclusionManager::GetBlockedFactorFor2DPosition(const Vector2& position)
{
	// Where is it relative to us
	const Vector3 listenerPosition = audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition();
	Vector2 directionRelative = position - Vector2(listenerPosition.x, listenerPosition.y);
	directionRelative.NormalizeSafe();
	f32 blockedFactor = 0.0f;
	for (u32 i=0; i<4; i++) // 4 directional vectors
	{
		Vector2 direction(g_Directions[i].x, g_Directions[i].y);
		f32 dotProd = directionRelative.Dot(direction);
		// Don't care about ones more than 90degs away
		dotProd = Max(0.0f, dotProd);
		// Scale our contribution by the elevation
		f32 contribution = dotProd * dotProd;
		blockedFactor += (contribution * m_BlockedInDirection[i]);
	}

	blockedFactor = Clamp(blockedFactor, 0.0f, 1.0f);
	return blockedFactor;	
}

// This will get the radial which defines the zone the angle is in.  Because of the way we use our probe data in UpdateOcclusionBoxes()
// each radial index covers the 45 degrees from itself to the next radial. 
// So m_OcclusionBoundingBoxes[elevation][1] (45 degrees if North = 0 degrees) is the zone from 45 degrees to 90 degrees.
// Each elevation covers the 45 degrees from itself to the next elevation index as well.
void naOcclusionManager::GetOcclusionZoneIndices(const Vector3& cameraPosition, const Vector3& position, f32& radialRatio, s32& occlusionZoneRadialIndex, 
												f32& elevationRatio, s32& occlusionZoneElevationIndex)
{
	f32 angleNorth = audNorthAudioEngine::GetGtaEnvironment()->GetVectorToNorthXYAngle(position - cameraPosition);

	// Get the radial and elevation indices that defines the zone our angle falls in.
	occlusionZoneRadialIndex = (s32)(angleNorth*(8.0f/360.0f)) % 8;	// Wrap around, so 360 = zone 0

	// Get the ratio of how close our angle is to the 2 radials and 2 elevations that border our zone
	radialRatio = angleNorth*(8.0f/360.0f) - (f32)occlusionZoneRadialIndex;
	naCErrorf(radialRatio >= 0.0f && radialRatio <= 1.1f, "rightToLeftRadialRatio: %f is invalid", radialRatio);
	radialRatio = Clamp(radialRatio, 0.0f, 1.0f);

	// Handle elevations slightly different as we don't wrap around and we're skewed in that the mid zones use probes from 0 - 45 and verts use 30 - 90,
	// so we get more resolution at lower elevations.  
	f32 angleUp = audNorthAudioEngine::GetGtaEnvironment()->GetVectorToUpAngle(position - cameraPosition);
	angleUp = Clamp(angleUp, 0.0f, 180.0f);

	if(angleUp <= 60.0f)
	{
		occlusionZoneElevationIndex = 0;
		elevationRatio = 1.0f - ((60.0f - angleUp) / 60.0f);
	}
	else if(angleUp > 60.0f && angleUp <= 90.0f)
	{
		occlusionZoneElevationIndex = 1;
		elevationRatio = 1.0f - ((90.0f - angleUp) / 30.0f);
	}
	else if(angleUp > 90.0f && angleUp <= 120.0f)
	{
		occlusionZoneElevationIndex = 2;
		elevationRatio = 1.0f - ((120.0f - angleUp) / 30.0f);
	}
	else if(angleUp > 120.0f)
	{
		occlusionZoneElevationIndex = 3;
		elevationRatio = 1.0f - ((180.0f - angleUp) / 60.0f);
	}
}

s32 naOcclusionManager::GetSecondaryRadialOcclusionZoneIndex(s32 primaryRadialOcclusionZoneIndex, f32 radialRatio)
{
	// Based on the angle, check which other zone to use, and how much to use it.
	s32 secondaryRadialOcclusionZoneIndex = 0;
	if(radialRatio < 0.5f)
	{
		// We're in the left half of the primary zone, so we want the radial that covers the zone to the left
		secondaryRadialOcclusionZoneIndex = primaryRadialOcclusionZoneIndex - 1;
		if(secondaryRadialOcclusionZoneIndex < 0)
		{
			secondaryRadialOcclusionZoneIndex = 7;	// Wrapped around
		}
	}
	else
	{
		// We're in the right half of the primary zone, so get the radial that covers the zone to the right
		secondaryRadialOcclusionZoneIndex = primaryRadialOcclusionZoneIndex + 1;
		if(secondaryRadialOcclusionZoneIndex > 7)
		{
			secondaryRadialOcclusionZoneIndex = 0;	// Wrapped around
		}
	}
	return secondaryRadialOcclusionZoneIndex;
}

// We only use 1 zone if we're looking straight up or straight down, so we could end up with the same zone for primary and secondary.
s32 naOcclusionManager::GetSecondaryElevationOcclusionZoneIndex(s32 primaryElevationOcclusionZoneIndex, f32 elevationRatio)
{
	return elevationRatio < 0.5f ? Max(primaryElevationOcclusionZoneIndex -1, 0) : Min(primaryElevationOcclusionZoneIndex +1, 3);
}

// Change the ratio from a 0-1 value of how far away we are from the index that defines the zone to a 0-0.5 value of how much to use the adjacent zone.
// If we're right between 2 zone indices, we want to use them equally so return .5f, if we're in the middle of a zone we want to only use that zone, so return .0f.
f32 naOcclusionManager::GetAmountToUseSecondaryRadialOcclusionZone(f32 ratio)
{
	// With a ratio < 0.5 we're on the left or top half of the zone, > 0.5 we're on the right or bottom half, and == 0.5f we're in the middle of the zone.
	return ratio < 0.5f ? 0.5f - ratio : 0.5f - (1.0f - ratio);
}

// Change the ratio from a 0-1 value of how far away we are from the index that defines the zone to a 0-0.5 value of how much to use the adjacent zone.
// If we're right between 2 zone indices, we want to use them equally so return .5f, if we're in the middle of a zone we want to only use that zone, so return .0f.
f32 naOcclusionManager::GetAmountToUseSecondaryElevationOcclusionZone(s32 primaryElevationOcclusionZoneIndex, f32 ratio)
{
	// Be more forgiving on the super vertical zones ( up and down / indices 0 and 3 ) and use the mid zones until we're literally straight up and down
	// If we're right on the border of any of the zones, then use those 2 equally, hence the linear interp from 0.0 - 0.5
	if(primaryElevationOcclusionZoneIndex == 0)
	{
		return Lerp(ratio, 0.0f, 0.5f);	// ratio of 0 is 0 degrees, ratio of 1.0 is 60 degrees
	}
	else if(primaryElevationOcclusionZoneIndex == 1 || primaryElevationOcclusionZoneIndex == 2)
	{
		return ratio < 0.5f ? 0.5f - ratio : 0.5f - (1.0f - ratio);
	}
	else	// primaryElevationOcclusionZoneIndex == 3
	{
		return Lerp(ratio, 0.5f, 0.0f); // ratio of 0 is 120 degrees, ratio of 1.0 is 180 degrees
	}
}

s32 naOcclusionManager::GetArrayElevationIndexFromOcclusionElevationIndex(naOcclusionProbeElevation elevationIndex)
{
	s32 index = 0;
	switch(elevationIndex)
	{
	case AUD_OCC_PROBE_ELEV_N:	// Vertical[0]
	case AUD_OCC_PROBE_ELEV_NW:	// Mid[0]
		index = 0;
		break;
	case AUD_OCC_PROBE_ELEV_NNW:	// Vertical[1]
	case AUD_OCC_PROBE_ELEV_WWN:	// Mid[1]
		index = 1;
		break;
	case AUD_OCC_PROBE_ELEV_SSW:	// Vertical[2]
	case AUD_OCC_PROBE_ELEV_WWS:	// Mid[2]
		index = 2;
		break;
	case AUD_OCC_PROBE_ELEV_S:	// Vertical[3]
	case AUD_OCC_PROBE_ELEV_SW:	// Mid[3]
		index = 3;
		break;
	default:
		naAssertf(0, "Bad elevation passed trying to get the local elevation array index from the global audOcclusionElevationIndex: %u", elevationIndex);
		break;
	}

	return index;
}

bool naOcclusionManager::ComputeIsEntityADoor(const CEntity* entity)
{
	if(entity && entity->GetIsTypeObject() && static_cast<const CObject*>((CEntity*)entity)->IsADoor())
	{
		return true;
	}
	return false;
}

bool naOcclusionManager::ComputeIsEntityGlass(const CEntity* entity)
{
	if(entity)
	{
		fragInst* frag = entity->GetFragInst();
		if(frag)
		{
			u8 numGroups = frag->GetTypePhysics()->GetNumChildGroups();
			for(u8 i = 0; i < numGroups; i++)
			{
				fragTypeGroup* fragGroup = frag->GetTypePhysics()->GetGroup(i);
				if(fragGroup)
				{
					if(fragGroup->GetMadeOfGlass())
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

void naOcclusionManager::FinishEnvironmentGroupListUpdate()
{
	// For optimization reasons, we don't reclaim memory when making the data tree between each path we process, 
	// so at the end of the environmentgroupmanager update we reset this to get the memory back
	m_OcclusionPathNodeDataRoot.children.Reset();
}

void naOcclusionManager::AddInteriorToMetadataLoadList(const CInteriorProxy* intProxy)
{
	if(sm_InteriorMetadataLoadList.IsFull())
	{
		naAssertf(false, "sm_InteriorMetadataLoadList is full ( > 64), need to increase size");
		return;
	}

	// Check if it's already in the list
	for(s32 i = 0; i < sm_InteriorMetadataLoadList.GetCount(); i++)
	{
		if(intProxy == sm_InteriorMetadataLoadList[i])
		{
			return;
		}
	}

	// Also check that it's not already in the loaded interior proxy map, as a couple quick CInteriorProxy state changes
	// could mean that the interior would throw multiple add calls before being removed
	naOcclusionInteriorInfo** intInfoPtr = m_LoadedInteriorInfos.Access(intProxy);
	if(intInfoPtr)
	{
		return;
	}

	sm_InteriorMetadataLoadList.Push(intProxy);
}

void naOcclusionManager::FinishUpdate()
{
#if __BANK
	if (PARAM_audOcclusionBuildEnabled.Get())
	{
		CPed* playerPed = FindPlayerPed();
		if(playerPed)
		{
			Vec3V playerPos = playerPed->GetTransform().GetPosition();
			f32 fadeLevel = camInterface::GetFadeLevel();
			bool hasLoaded = fwMapDataStore::GetStore().GetBoxStreamer().HasLoadedAboutPos(playerPos, fwBoxStreamerAsset::MASK_MAPDATA);
			if(hasLoaded && fadeLevel == 0.0f && !g_FrontendAudioEntity.IsLoadingSceneActive())
			{
				if(m_AudioOcclusionBuildToolStartBuild)
				{
					m_AudioOcclusionBuildToolStartBuild = false;
					SetToBuildOcclusionPaths(true);
				}
				else if(m_AudioOcclusionBuildToolContinueBuild)
				{
					m_AudioOcclusionBuildToolContinueBuild = false;
					SetToBuildOcclusionPaths(false);
				}
				else if(m_AudioOcclusionBuildToolBuildInteriorList)
				{
					m_AudioOcclusionBuildToolBuildInteriorList = false;
					CreateInteriorList();
				}
			}
		}

		if(GetIsBuildingOcclusionPaths())
		{
			// Temporarily disable processing portal occlusion until we've finished rebuilding the paths
			SetIsPortalOcclusionEnabled(false);

			// Check to see if we've finished computing and building all the path information
			if(BuildOcclusionPaths())
			{
				SetIsPortalOcclusionEnabled(true);
				SetIsBuildingOcclusionPaths(false);
				WriteOcclusionBuildProgress();
			}
		}
	}

#endif // __BANK
}

#if __BANK
f32 naOcclusionManager::ComputeMaxDoorOcclusion(const CEntity* entity)
{
	if(entity)
	{
		const CObject* object = static_cast<const CObject*>(entity);
		if(object && object->IsADoor())
		{
			const CDoor* door = static_cast<const CDoor*>(object);
			if(door && door->GetCurrentPhysicsInst())
			{
				const audDoorAudioEntity* doorAudioEntity = door->GetDoorAudioEntity();
				if(doorAudioEntity)
				{
					const DoorAudioSettings* doorSettings = doorAudioEntity->GetDoorAudioSettings();
					if(doorSettings)
					{
						return doorSettings->MaxOcclusion;
					}
				}
			}
		}
	}

	return 0.0f;
}

void naOcclusionManager::DebugAudioImpact(const Vector3& intersectionPos, const Vector3& startPos, const s32 radialIndex, const naOcclusionProbeElevation elevationIndex)
{
	if(g_DrawProbes && intersectionPos != Vector3(0.0f, 0.0f, 0.0f))
	{
		sysCriticalSection lock(m_Lock);
		g_OcclusionDebugInfo[g_OcclusionDebugIndex].time = fwTimer::GetTimeInMilliseconds();
		g_OcclusionDebugInfo[g_OcclusionDebugIndex].intersectionPosition = intersectionPos;
		g_OcclusionDebugInfo[g_OcclusionDebugIndex].startPosition = startPos;
		g_OcclusionDebugInfo[g_OcclusionDebugIndex].radialIndex = radialIndex;
		g_OcclusionDebugInfo[g_OcclusionDebugIndex].elevationIndex = elevationIndex;

		formatf(g_OcclusionDebugInfo[g_OcclusionDebugIndex].debugText, sizeof(g_OcclusionDebugInfo[g_OcclusionDebugIndex].debugText)-1, 
			"rad: %d dist: %f", radialIndex, (intersectionPos - startPos).Mag());

		g_OcclusionDebugIndex = (g_OcclusionDebugIndex+1)%g_NumDebugOcclusionInfos;
	}
}

void naOcclusionManager::DrawDebug()
{
	if(g_ShowInteriorPoolCount)
	{
#define NEXT_LINE	grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString ); lineBase += lineInc;

		char tempString[64];
		static bank_float lineInc = 0.015f;
		f32 lineBase = 0.1f;
		sprintf(tempString, "Interior Pool Count : %d", naOcclusionInteriorInfo::GetPool()->GetNoOfFreeSpaces()); NEXT_LINE		

	}
	if(g_DrawProbes)
	{
		for(u32 i = 0; i < g_NumDebugOcclusionInfos; i++)
		{
			switch(g_OcclusionDebugInfo[i].elevationIndex)
			{
			case AUD_OCC_PROBE_ELEV_S:
				if(g_DebugDrawLocalEnvironmentS)
				{
					grcDebugDraw::Text(g_OcclusionDebugInfo[i].intersectionPosition, Color_pink, g_OcclusionDebugInfo[i].debugText);
					grcDebugDraw::Line(g_OcclusionDebugInfo[i].startPosition, g_OcclusionDebugInfo[i].intersectionPosition, Color_red, Color_red3);
				}
				break;
			case AUD_OCC_PROBE_ELEV_SSW:
				if(g_DebugDrawLocalEnvironmentSSW)
				{
					grcDebugDraw::Text(g_OcclusionDebugInfo[i].intersectionPosition, Color_red, g_OcclusionDebugInfo[i].debugText);
					grcDebugDraw::Line(g_OcclusionDebugInfo[i].startPosition, g_OcclusionDebugInfo[i].intersectionPosition, Color_orange, Color_orange3);
				}
				break;
			case AUD_OCC_PROBE_ELEV_SW:
				if(g_DebugDrawLocalEnvironmentSW)
				{
					grcDebugDraw::Text(g_OcclusionDebugInfo[i].intersectionPosition, Color_orange, g_OcclusionDebugInfo[i].debugText);
					grcDebugDraw::Line(g_OcclusionDebugInfo[i].startPosition, g_OcclusionDebugInfo[i].intersectionPosition, Color_gold, Color_gold3);
				}
				break;
			case AUD_OCC_PROBE_ELEV_WWS:
				if(g_DebugDrawLocalEnvironmentWWS)
				{
					grcDebugDraw::Text(g_OcclusionDebugInfo[i].intersectionPosition, Color_orange, g_OcclusionDebugInfo[i].debugText);
					grcDebugDraw::Line(g_OcclusionDebugInfo[i].startPosition, g_OcclusionDebugInfo[i].intersectionPosition, Color_yellow, Color_yellow3);
				}
				break;
			case AUD_OCC_PROBE_ELEV_W:
				if(g_DebugDrawLocalEnvironmentW)
				{
					grcDebugDraw::Text(g_OcclusionDebugInfo[i].intersectionPosition, Color_brown, g_OcclusionDebugInfo[i].debugText);
					grcDebugDraw::Line(g_OcclusionDebugInfo[i].startPosition, g_OcclusionDebugInfo[i].intersectionPosition, Color_white, Color_white);
				}
				break;
			case AUD_OCC_PROBE_ELEV_WWN:
				if(g_DebugDrawLocalEnvironmentWWN)
				{
					grcDebugDraw::Text(g_OcclusionDebugInfo[i].intersectionPosition, Color_yellow, g_OcclusionDebugInfo[i].debugText);
					grcDebugDraw::Line(g_OcclusionDebugInfo[i].startPosition, g_OcclusionDebugInfo[i].intersectionPosition, Color_green, Color_green3);
				}
				break;
			case AUD_OCC_PROBE_ELEV_NW:
				if(g_DebugDrawLocalEnvironmentNW)
				{
					grcDebugDraw::Text(g_OcclusionDebugInfo[i].intersectionPosition, Color_grey, g_OcclusionDebugInfo[i].debugText);
					grcDebugDraw::Line(g_OcclusionDebugInfo[i].startPosition, g_OcclusionDebugInfo[i].intersectionPosition, Color_aquamarine, Color_aquamarine3);
				}
				break;
			case AUD_OCC_PROBE_ELEV_NNW:
				if(g_DebugDrawLocalEnvironmentNNW)
				{
					grcDebugDraw::Text(g_OcclusionDebugInfo[i].intersectionPosition, Color_pink, g_OcclusionDebugInfo[i].debugText);
					grcDebugDraw::Line(g_OcclusionDebugInfo[i].startPosition, g_OcclusionDebugInfo[i].intersectionPosition, Color_blue, Color_blue3);
				}
				break;
			case AUD_OCC_PROBE_ELEV_N:
				if(g_DebugDrawLocalEnvironmentN)
				{
					grcDebugDraw::Text(g_OcclusionDebugInfo[i].intersectionPosition, Color_green, g_OcclusionDebugInfo[i].debugText);
					grcDebugDraw::Line(g_OcclusionDebugInfo[i].startPosition, g_OcclusionDebugInfo[i].intersectionPosition, Color_purple, Color_purple3);
				}
				break;
			default:
				break;
			}
		}
	}

	if(g_DrawLevelLimits)
	{
		char strDebug[256] = "";

		for(s32 i=0; i<24; i++)
		{
			sprintf(strDebug, "d %6.1f : x %6.1f : y %6.1f : z %6.1f", m_LevelWallDistances[i], m_LevelXLimits[i], m_LevelYLimits[i], m_LevelZLimits[i]);
			grcDebugDraw::PrintToScreenCoors(strDebug, 5,7+i);

		}

		// We're drawing at geometry so you can't really see anything, bring it in a bit.
		// Move the bounds closer to the listener position
		f32 boundsDrawDistanceOffset = 1.0f;
		for(s32 elevation = 0; elevation < 4; elevation++)
		{
			for(s32 radial = 0; radial < 8; radial++)
			{
				s32 nextRadial = (radial + 1) % 8;
				Vector3 boundToListener = (audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition() - m_OcclusionBoundingBoxes[elevation][radial]);
				boundToListener.NormalizeSafe();
				boundToListener.Scale(boundsDrawDistanceOffset);

				Vector3 bound2ToListener = (audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition() - m_OcclusionBoundingBoxes[elevation][nextRadial]);
				bound2ToListener.NormalizeSafe();
				bound2ToListener.Scale(boundsDrawDistanceOffset);

				if(elevation == 0)
				{
					grcDebugDraw::Line((m_OcclusionBoundingBoxes[elevation][radial] + boundToListener), (m_OcclusionBoundingBoxes[elevation][nextRadial] + bound2ToListener), Color_red);
				}
				else if(elevation == 1)
				{
					grcDebugDraw::Line((m_OcclusionBoundingBoxes[elevation][radial] + boundToListener), (m_OcclusionBoundingBoxes[elevation][nextRadial] + bound2ToListener), Color_green);
				}
				else if(elevation == 2)
				{
					grcDebugDraw::Line((m_OcclusionBoundingBoxes[elevation][radial] + boundToListener), (m_OcclusionBoundingBoxes[elevation][nextRadial] + bound2ToListener), Color_blue);
				}
				else
				{
					grcDebugDraw::Line((m_OcclusionBoundingBoxes[elevation][radial] + boundToListener), (m_OcclusionBoundingBoxes[elevation][nextRadial] + bound2ToListener), Color_orange);
				}
			}
		}
	}

	if(g_DrawBlockedFactors)
	{
		const Matrix34 listenerMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix());
		Vector3 right = listenerMatrix.a;

		right.NormalizeFast();
		const f32 cosangle = right.Dot(g_Directions[AUD_AMB_DIR_NORTH]);
		const f32 angle = AcosfSafe(cosangle);
		const f32 degrees = RtoD*angle;
		f32 actualDegrees;
		if(right.Dot(g_Directions[AUD_AMB_DIR_EAST]) <= 0.0f)
		{
			actualDegrees = 360.0f - degrees;
		}
		else
		{
			actualDegrees = degrees;
		}

		//Adjust the degrees so North is 0 degrees and get it between 0-360
		actualDegrees += 270.f;
		while(actualDegrees > 360.0f)
		{
			actualDegrees -= 360.0f;
		}

		static audMeterList meter[4];
		static f32 meterBasePan[4] = {0, 90, 180, 270};
		static const char* directionName[4] = {"N", "E", "S", "W"};
		static f32 directionValue[4];

		for(u32 i = 0; i < 4; i++)
		{ 
			directionValue[i] = m_BlockedInDirection[i];

			f32 cirleDegrees = meterBasePan[i] + (360.0f - actualDegrees) + 270.0f;
			while(cirleDegrees > 360.0f)
			{
				cirleDegrees -= 360.0f;
			}
			const f32 angle = cirleDegrees * (PI/180);

			meter[i].left = 150.f + (75.f * rage::Cosf(angle));
			meter[i].width = 50.f;
			meter[i].bottom = 175.f + (75.f * rage::Sinf(angle));
			meter[i].height = 50.f;
			meter[i].names = &directionName[i];
			meter[i].values = &directionValue[i];
			meter[i].numValues = 1;
			audNorthAudioEngine::DrawLevelMeters(&meter[i]);
		}
	}

	if(g_DrawExteriorOcclusionPerDirection)
	{
		const Matrix34 listenerMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix());
		Vector3 right = listenerMatrix.a;

		right.NormalizeFast();
		const f32 cosangle = right.Dot(g_Directions[AUD_AMB_DIR_NORTH]);
		const f32 angle = AcosfSafe(cosangle);
		const f32 degrees = RtoD*angle;
		f32 actualDegrees;
		if(right.Dot(g_Directions[AUD_AMB_DIR_EAST]) <= 0.0f)
		{
			actualDegrees = 360.0f - degrees;
		}
		else
		{
			actualDegrees = degrees;
		}

		//Adjust the degrees so North is 0 degrees and get it between 0-360
		actualDegrees += 270.f;
		while(actualDegrees > 360.0f)
		{
			actualDegrees -= 360.0f;
		}

		static audMeterList meter[4];
		static f32 meterBasePan[4] = {0, 90, 180, 270};
		static const char* directionName[4] = {"N", "E", "S", "W"};
		static f32 directionValue[4];

		for(u32 i = 0; i < 4; i++)
		{ 
			directionValue[i] = GetExteriorOcclusionForDirection((audAmbienceDirection)i);

			f32 cirleDegrees = meterBasePan[i] + (360.0f - actualDegrees) + 270.0f;
			while(cirleDegrees > 360.0f)
			{
				cirleDegrees -= 360.0f;
			}
			const f32 angle = cirleDegrees * (PI/180);

			meter[i].left = 150.f + (75.f * rage::Cosf(angle));
			meter[i].width = 50.f;
			meter[i].bottom = 400.f + (75.f * rage::Sinf(angle));
			meter[i].height = 50.f;
			meter[i].names = &directionName[i];
			meter[i].values = &directionValue[i];
			meter[i].numValues = 1;
			audNorthAudioEngine::DrawLevelMeters(&meter[i]);
		}
	}

	if(g_DebugOWO)
	{
		static audMeterList owoMeter;
		static const char* owoMeterName = "OutsideWorldOcclusion";
		static f32 owoMeterValue;

		owoMeterValue = m_OutsideWorldOcclusion;

		owoMeter.left = 150.f;
		owoMeter.width = 50.f;
		owoMeter.bottom = 350.f;
		owoMeter.height = 50.f;
		owoMeter.names = &owoMeterName;
		owoMeter.values = &owoMeterValue;
		owoMeter.numValues = 1;
		audNorthAudioEngine::DrawLevelMeters(&owoMeter);
	}
}

void naOcclusionManager::DrawLoadedOcclusionPaths()
{
	char buffer[255];
	s32 textYLocation = 15;

	formatf(buffer, sizeof(buffer)-1, "naOcclusionInteriorInfo pool - Slots: %u/%u  Memory: %u/%u", 
		(u32)naOcclusionInteriorInfo::GetPool()->GetNoOfUsedSpaces(), 
		(u32)naOcclusionInteriorInfo::GetPool()->GetSize(),
		(u32)(naOcclusionInteriorInfo::GetPool()->GetStorageSize() * naOcclusionInteriorInfo::GetPool()->GetNoOfUsedSpaces()),
		(u32)(naOcclusionInteriorInfo::GetPool()->GetStorageSize() * naOcclusionInteriorInfo::GetPool()->GetSize()));
	grcDebugDraw::PrintToScreenCoors(buffer, 10, textYLocation);
	textYLocation++;

	formatf(buffer, sizeof(buffer)-1, "naOcclusionPathNode pool - Slots: %u/%u  Memory: %u/%u", 
		(u32)naOcclusionPathNode::GetPool()->GetNoOfUsedSpaces(), 
		(u32)naOcclusionPathNode::GetPool()->GetSize(),
		(u32)(naOcclusionPathNode::GetPool()->GetStorageSize() * naOcclusionPathNode::GetPool()->GetNoOfUsedSpaces()),
		(u32)(naOcclusionPathNode::GetPool()->GetStorageSize() * naOcclusionPathNode::GetPool()->GetSize()));
	grcDebugDraw::PrintToScreenCoors(buffer, 10, textYLocation);
	textYLocation++;

	formatf(buffer, sizeof(buffer)-1, "naOcclusionPortalInfo pool - Slots: %u/%u  Memory: %u/%u", 
		(u32)naOcclusionPortalInfo::GetPool()->GetNoOfUsedSpaces(), 
		(u32)naOcclusionPortalInfo::GetPool()->GetSize(),
		(u32)(naOcclusionPortalInfo::GetPool()->GetStorageSize() * naOcclusionPortalInfo::GetPool()->GetNoOfUsedSpaces()),
		(u32)(naOcclusionPortalInfo::GetPool()->GetStorageSize() * naOcclusionPortalInfo::GetPool()->GetSize()));
	grcDebugDraw::PrintToScreenCoors(buffer, 10, textYLocation);
	textYLocation++;

	formatf(buffer, sizeof(buffer)-1, "naOcclusionPortalEntity pool - Slots: %u/%u  Memory: %u/%u", 
		(u32)naOcclusionPortalEntity::GetPool()->GetNoOfUsedSpaces(), 
		(u32)naOcclusionPortalEntity::GetPool()->GetSize(),
		(u32)(naOcclusionPortalEntity::GetPool()->GetStorageSize() * naOcclusionPortalEntity::GetPool()->GetNoOfUsedSpaces()),
		(u32)(naOcclusionPortalEntity::GetPool()->GetStorageSize() * naOcclusionPortalEntity::GetPool()->GetSize()));
	grcDebugDraw::PrintToScreenCoors(buffer, 10, textYLocation);
	textYLocation++;

	atMap<const CInteriorProxy*, naOcclusionInteriorInfo*>::Iterator entry = m_LoadedInteriorInfos.CreateIterator();
	s32 textXLoc = 50;
	for(entry.Start(); !entry.AtEnd(); entry.Next())
	{
		textYLocation++;
		const CInteriorProxy* intProxy = entry.GetKey();
		naOcclusionInteriorInfo** interiorInfoPtr = m_LoadedInteriorInfos.Access(entry.GetKey());
		if(interiorInfoPtr && *interiorInfoPtr && intProxy)
		{
			const char* loadingStatus;
			if(g_fwMetaDataStore.HasObjectLoaded(intProxy->GetAudioOcclusionMetadataFileIndex()))
			{
				loadingStatus = "LOADED";
			}
			else if(g_fwMetaDataStore.IsObjectLoading(intProxy->GetAudioOcclusionMetadataFileIndex()))
			{
				loadingStatus = "LOADING";
			}
			else
			{
				loadingStatus = "NOT LOADED";
			}

			formatf(buffer, sizeof(buffer)-1, "Interior: %s - %s Num PathNodes: %d NumPortalInfos: %d", 
					intProxy->GetModelName(), loadingStatus, (*interiorInfoPtr)->GetNumPathNodes(), (*interiorInfoPtr)->GetNumPortalInfos());
			grcDebugDraw::PrintToScreenCoors(buffer, textXLoc, textYLocation);
		}
	}
}

void naOcclusionManager::AddWidgets(bkBank& bank)
{
	sm_IsPortalOcclusionEnabled = !(PARAM_audDisablePortalOcclusion.Get());

	bank.PushGroup("Occlusion",false);
		bank.AddToggle("EnableEmitterOcclusion", &sm_EnableEmitterOcclusion);
		bank.AddToggle("EnableAmbientOcclusion", &sm_EnableAmbientOcclusion);
		bank.AddToggle("EnableSpawnEffectsOcclusion", &sm_EnableSpawnEffectsOcclusion);
		bank.AddToggle("EnableRequestedEffectsOcclusion", &sm_EnableRequestedEffectsOcclusion);
		bank.AddToggle("EnableCollisionOcclusion", &sm_EnableCollisionOcclusion);

		bank.AddToggle("EnablePortalOcclusion", &sm_IsPortalOcclusionEnabled);
		bank.AddSlider("MaxOcclusionDampingFactor", &g_MaxOcclusionDampingFactor, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("MinBlockedInInterior", &g_MinBlockedInInterior, 0.0, 1.0f, 0.01f);

		bank.AddToggle("UseNewBlockedCoveredAlgorithm", &g_UseNewBlockedCoveredAlgorithm);

		bank.PushGroup("Occlusion Debug Info", true);
			bank.AddToggle("DisplayOcclusionDebugInfo", &g_DisplayOcclusionDebugInfo);
			bank.AddToggle("DrawPortalOcclusionPaths", &g_DrawPortalOcclusionPaths);
			bank.AddToggle("DrawPortalOcclusionPathDistances", &g_DrawPortalOcclusionPathDistances);
			bank.AddToggle("DrawPortalOcclusionPathInfo", &g_DrawPortalOcclusionPathInfo);
			bank.AddToggle("DrawLoadedOcclusionPathData", &g_DrawLoadedOcclusionPaths);
			bank.AddToggle("DrawChildWeights", &g_DrawChildWeights);
			bank.AddToggle("DebugPortalInfo", &g_DebugPortalInfo);
			bank.AddSlider("DebugPortalInfoRange", &g_DebugPortalInfoRange, -1.0f, 1000.0f, 1.f);			
			bank.AddToggle("DebugDoorOcclusion", &g_DebugDoorOcclusion);
			bank.AddToggle("DrawBrokenGlass", &g_DrawBrokenGlass);
			bank.AddToggle("AssertOnMissingOcclusionPaths", &g_AssertOnMissingOcclusionPaths);
			
			
			bank.AddToggle("ShowLevelLimits", &g_DrawLevelLimits);

			bank.AddToggle("DrawBlockedFactors", &g_DrawBlockedFactors);
			bank.AddSlider("BlockedSmoothRate", &g_BlockedSmoothRate, 0.0f, 100.0f, 0.01f);
			bank.AddToggle("DrawExteriorOcclusionPerDirection", &g_DrawExteriorOcclusionPerDirection);
			bank.AddSlider("ExteriorOcclusionSmoothRate", &g_ExteriorOcclusionSmoothRate, 0.0, 100.0f, 0.1f);
			bank.AddSlider("RadialCoverageScalar", &g_RadialCoverageScalar, 0.0f, 10.0f, 0.01f);
			bank.AddToggle("DrawOutsideWorldOcclusion", &g_DebugOWO);
		bank.PopGroup();

		bank.PushGroup("Occlusion Probes", true);
			bank.AddToggle("DebugProbeOcclusionFromPlayer", &g_DebugProbeOcclusionFromPlayer);
			bank.AddToggle("DebugProbeOcclusionFromListenerPosition", &g_DebugProbeOcclusionFromListenerPosition);
			bank.AddToggle("Draw Probes", &g_DrawProbes);
			bank.AddToggle("DrawProbe N", &g_DebugDrawLocalEnvironmentN);
			bank.AddToggle("DrawProbe NNW", &g_DebugDrawLocalEnvironmentNNW);
			bank.AddToggle("DrawProbe NW", &g_DebugDrawLocalEnvironmentNW);
			bank.AddToggle("DrawProbe WWN", &g_DebugDrawLocalEnvironmentWWN);
			bank.AddToggle("DrawProbe W", &g_DebugDrawLocalEnvironmentW);
			bank.AddToggle("DrawProbe WWS", &g_DebugDrawLocalEnvironmentWWS);
			bank.AddToggle("DrawProbe SW", &g_DebugDrawLocalEnvironmentSW);
			bank.AddToggle("DrawProbe SSW", &g_DebugDrawLocalEnvironmentSSW);
			bank.AddToggle("DrawProbe S", &g_DebugDrawLocalEnvironmentS);
			bank.AddSlider("ScanLength", &g_ScanLength, 20.0f, 100.0f, 1.0f);
		bank.PopGroup();

		naEnvironmentGroup::AddWidgets(bank);
		naOcclusionPathNode::AddWidgets(bank);

	bank.PopGroup();
}

#endif // __BANK

#if __BANK

void naOcclusionManager::SetToBuildOcclusionPaths(const bool isNewBuild)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	FileHandle fileHandle;
	
	fileHandle = isNewBuild ? CFileMgr::OpenFileForWriting(g_OcclusionBuildProgressFile) : CFileMgr::OpenFileForAppending(g_OcclusionBuildProgressFile);
	if(naVerifyf(CFileMgr::IsValidFileHandle(fileHandle), "Could not open %s, make sure it's checked out or writable", g_OcclusionBuildProgressFile))
	{
		CFileMgr::CloseFile(fileHandle);
	}

	fileHandle = isNewBuild ? CFileMgr::OpenFileForWriting(g_OcclusionBuiltIntraPathsFile) : CFileMgr::OpenFileForAppending(g_OcclusionBuiltIntraPathsFile);
	if(naVerifyf(CFileMgr::IsValidFileHandle(fileHandle), "Could not open %s, make sure it's checked out or writable", g_OcclusionBuiltIntraPathsFile))
	{
		CFileMgr::CloseFile(fileHandle);
	}

	fileHandle = isNewBuild ? CFileMgr::OpenFileForWriting(g_OcclusionBuiltInterPathsFile) : CFileMgr::OpenFileForAppending(g_OcclusionBuiltInterPathsFile);
	if(naVerifyf(CFileMgr::IsValidFileHandle(fileHandle), "Could not open %s, make sure it's checked out or writable", g_OcclusionBuiltInterPathsFile))
	{
		CFileMgr::CloseFile(fileHandle);
	}

	fileHandle = isNewBuild ? CFileMgr::OpenFileForWriting(g_OcclusionBuiltTunnelPathsFile) : CFileMgr::OpenFileForAppending(g_OcclusionBuiltTunnelPathsFile);
	if(naVerifyf(CFileMgr::IsValidFileHandle(fileHandle), "Could not open %s, make sure it's checked out or writable", g_OcclusionBuiltTunnelPathsFile))
	{
		CFileMgr::CloseFile(fileHandle);
	}

	naOcclusionInteriorInfo::GetPool()->DeleteAll();
	naOcclusionPortalEntity::GetPool()->DeleteAll();
	naOcclusionPortalInfo::GetPool()->DeleteAll();
	naOcclusionPathNode::GetPool()->DeleteAll();

	naOcclusionInteriorInfo::ShutdownPool();
	naOcclusionPortalEntity::ShutdownPool();
	naOcclusionPortalInfo::ShutdownPool();
	naOcclusionPathNode::ShutdownPool();

	fwConfigManager::SetUseLargerPoolFromCodeOrData(true);
	naOcclusionInteriorInfo::InitPool(naOcclusionInteriorInfo::sm_OcclusionBuildInteriorInfoPoolSize, MEMBUCKET_AUDIO);
	naOcclusionPortalEntity::InitPool(naOcclusionPortalEntity::sm_OcclusionBuildPortalEntityPoolSize, MEMBUCKET_AUDIO);
	naOcclusionPortalInfo::InitPool(naOcclusionPortalInfo::sm_OcclusionBuildPortalInfoPoolSize, MEMBUCKET_AUDIO);
	naOcclusionPathNode::InitPool(naOcclusionPathNode::sm_OcclusionBuildPathNodePoolSize, MEMBUCKET_AUDIO);
	fwConfigManager::SetUseLargerPoolFromCodeOrData(false);

	m_OcclusionPathKeyTracker.Reset();
	m_LoadedInteriorInfos.Reset();
	sm_InteriorMetadataLoadList.Reset();
	m_InteriorMetadata.Kill();
	m_SwappedInteriorHashkey.Kill();
	m_CurrentOcclusionBuildStep = AUD_OCC_BUILD_STEP_START;
	m_IsBuildingOcclusionPaths = true;

	// Create a list of previous failures from file
	CreateFailureList();

	// Make a list of interiors we shouldn't build and write it out to file
	CreateDoNotBuildList();

	// Create a list of all the interiors that the audOcclusionBuilder tool has specified
	CreateInteriorBuildList();
}

bool naOcclusionManager::BuildOcclusionPaths()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	WriteOcclusionBuildProgress();
	m_CurrentOcclusionBuildStep = UpdateOcclusionBuildProcess();
	return m_CurrentOcclusionBuildStep == AUD_OCC_BUILD_STEP_FINISH;
}

void naOcclusionManager::CreateFailureList()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	INIT_PARSER;

	parTree* tree = PARSER.LoadTree(g_OcclusionFailureFile, "xml");

	if (tree)
	{
		parTreeNode* root = tree->GetRoot();

		parTreeNode::ChildNodeIterator childrenStart = root->BeginChildren();
		parTreeNode::ChildNodeIterator childrenEnd = root->EndChildren();

		for(parTreeNode::ChildNodeIterator ci = childrenStart; ci != childrenEnd; ++ci)
		{
			parTreeNode* curNode = *ci;

			s32 interiorHashAsInt = 0;
			curNode->FindValueFromPath("@InteriorHash", interiorHashAsInt);
			s32 buildStepAsInt = NUM_AUD_OCC_BUILD_STEPS;
			curNode->FindValueFromPath("@BuildStep", buildStepAsInt);

			if(interiorHashAsInt != 0 && buildStepAsInt >= AUD_OCC_BUILD_STEP_START && buildStepAsInt < NUM_AUD_OCC_BUILD_STEPS)
			{
				naOcclusionFailure* failure = &m_FailureList.Grow();
				failure->uniqueInteriorHash = (u32)interiorHashAsInt;
				failure->buildStep = (naOcclusionBuildStep)buildStepAsInt;
			}
		}

		delete tree;
	}
	else
	{
		naAssertf(false, "Missing %s file", g_OcclusionFailureFile);
	}
}

void naOcclusionManager::CreateDoNotBuildList()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	for(s32 i = 0; i < CInteriorProxy::GetPool()->GetSize(); i++)
	{
		const CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(i);
		if(intProxy)
		{
			// See if we've already tried this interior and failed
			const u32 uniqueHashKey = GetUniqueProxyHashkey(intProxy);
			const u32 numFailedAttemptsIntra = GetNumFailedAttemptsAtBuildStep(AUD_OCC_BUILD_STEP_INTRA_PATHS, uniqueHashKey);
			const u32 numFailedAttemptsInter = GetNumFailedAttemptsAtBuildStep(AUD_OCC_BUILD_STEP_INTER_PATHS, uniqueHashKey);
			const u32 numFailedAttemptsTunnel = GetNumFailedAttemptsAtBuildStep(AUD_OCC_BUILD_STEP_TUNNEL_PATHS, uniqueHashKey);
			if(numFailedAttemptsIntra >= 2 || numFailedAttemptsInter >= 2 || numFailedAttemptsTunnel >= 2)
			{
				m_InteriorDoNotUseList.PushAndGrow(uniqueHashKey);
			}
		}
	}

	INIT_PARSER;

	parTree* tree = rage_new parTree();

	if(tree)
	{
		parTreeNode* root = rage_new parTreeNode("Interiors");
		tree->SetRoot(root);

		// Write this out to file so that the audOcclusionBuilder tool can display failed interiors
		for(s32 i = 0; i < m_InteriorDoNotUseList.GetCount(); i++)
		{
			const CInteriorProxy* intProxy = GetInteriorProxyFromUniqueHashkey(m_InteriorDoNotUseList[i]);
			if(intProxy)
			{
				parTreeNode* interiorNode = rage_new parTreeNode("Interior");
				interiorNode->AppendAsChildOf(root);

				interiorNode->AppendStdLeafChild<s64>("InteriorHash", (s64)m_InteriorDoNotUseList[i]);
				interiorNode->AppendStdLeafChild<const char*>("InteriorName", intProxy->GetModelName());
			}
		}

		PARSER.SaveTree(g_OcclusionBuildUnbuiltInteriorFile, "xml", tree);
		delete tree;
	}
}

void naOcclusionManager::CreateInteriorList()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	INIT_PARSER;

	parTree* tree = rage_new parTree();

	if(tree)
	{
		parTreeNode* root = rage_new parTreeNode("Interiors");
		tree->SetRoot(root);

		atArray<u32> uniqueHashkeyTracker;

		for(s32 i = 0; i < CInteriorProxy::GetPool()->GetSize(); i++)
		{
			const CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(i);
			if(intProxy)
			{
				const u32 uniqueHashkey = GetUniqueProxyHashkey(intProxy);
				if(uniqueHashkeyTracker.Find(uniqueHashkey) == -1)
				{
					uniqueHashkeyTracker.PushAndGrow(uniqueHashkey);

					parTreeNode* interiorNode = rage_new parTreeNode("Interior");
					interiorNode->AppendAsChildOf(root);

					interiorNode->AppendStdLeafChild<const char*>("InteriorName", intProxy->GetModelName());
					interiorNode->AppendStdLeafChild<s64>("InteriorHash", (s32)uniqueHashkey);
					interiorNode->AppendStdLeafChild<s64>("MapSlotIndex", (s32)intProxy->GetMapDataSlotIndex().Get());
					Vec3V pos;
					intProxy->GetPosition(pos);
					interiorNode->AppendStdLeafChild<float>("PosX", pos.GetXf());
					interiorNode->AppendStdLeafChild<float>("PosY", pos.GetYf());
					interiorNode->AppendStdLeafChild<float>("PosZ", pos.GetZf());
				}

			}
		}

		PARSER.SaveTree(g_OcclusionBuildInteriorListFile, "xml", tree);
		delete tree;
	}
}

s32 QSortInteriorHashkeyCompareFunc(const u32* a, const u32* b)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	if(*a < *b)
	{
		return -1;
	}
	else if(*a > *b)
	{
		return 1;
	}
	else
	{
		naAssertf(0, "We shouldn't have duplicate keys in the interior build list array");
		return 0;
	}
}

void naOcclusionManager::CreateInteriorBuildList()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	INIT_PARSER;

	parTree* tree = PARSER.LoadTree(g_OcclusionBuildInteriorBuildListFile, "xml");

	if (tree)
	{
		parTreeNode* root = tree->GetRoot();

		// Create a list of all the interior hashkey's that the tool wants to build
		if(root && root->FindNumChildren())
		{
			parTreeNode::ChildNodeIterator childrenStart = root->BeginChildren();
			parTreeNode::ChildNodeIterator childrenEnd = root->EndChildren();

			for(parTreeNode::ChildNodeIterator ci = childrenStart; ci != childrenEnd; ++ci)
			{
				parTreeNode* curNode = *ci;

				s32 uniqueProxyHashkeyAsInt = 0;
				curNode->FindValueFromPath("@value", uniqueProxyHashkeyAsInt);
				m_InteriorBuildList.PushAndGrow((u32)uniqueProxyHashkeyAsInt);
			}
		}
		// If the tool hasn't selected any interiors, then assume we should build them all
		else
		{
			for(s32 i = 0; i < CInteriorProxy::GetPool()->GetSize(); i++)
			{
				const CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(i);
				if(intProxy && m_InteriorBuildList.Find(GetUniqueProxyHashkey(intProxy)) == -1)
				{
					m_InteriorBuildList.PushAndGrow(GetUniqueProxyHashkey(intProxy));
				}
			}
		}

		// Sort the list so we always build the interiors in the same order to reduce diffs between build iterations
		m_InteriorBuildList.QSort(0, -1, QSortInteriorHashkeyCompareFunc);

		delete tree;
	}
	else
	{
		naAssertf(false, "Missing %s file", g_OcclusionBuildInteriorBuildListFile);
	}
}

void naOcclusionManager::WriteOcclusionBuildProgress()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	FileHandle fileHandle = CFileMgr::OpenFileForWriting(g_OcclusionBuildProgressFile);
	if(naVerifyf(CFileMgr::IsValidFileHandle(fileHandle), "Could not open %s make sure it exists,", g_OcclusionBuildProgressFile))
	{
		char buf[255];

		CInteriorProxy* intProxy = NULL;
		if(m_CurrentIntProxySlot < CInteriorProxy::GetPool()->GetSize())
		{
			intProxy = CInteriorProxy::GetPool()->GetSlot(m_CurrentIntProxySlot);
		}

		formatf(buf, sizeof(buf)-1, "%u\n", m_CurrentOcclusionBuildStep);
		CFileMgr::Write(fileHandle, buf, istrlen(buf));
		formatf(buf, sizeof(buf)-1, "%u\n", m_CurrentOcclusionBuildStep == AUD_OCC_BUILD_STEP_FINISH ? 1 : 0);
		CFileMgr::Write(fileHandle, buf, istrlen(buf));
		formatf(buf, sizeof(buf)-1, "%u\n", GetUniqueProxyHashkey(intProxy));
		CFileMgr::Write(fileHandle, buf, istrlen(buf));
		formatf(buf, sizeof(buf)-1, "%s\n", intProxy ? intProxy->GetModelName() : "");
		CFileMgr::Write(fileHandle, buf, istrlen(buf));
		formatf(buf, sizeof(buf)-1, "%s\n", intProxy && intProxy->GetInteriorInst() ? intProxy->GetInteriorInst()->GetRoomName((u32)m_CurrentRoomIdx) : "");
		CFileMgr::Write(fileHandle, buf, istrlen(buf));
		formatf(buf, sizeof(buf)-1, "%u\n", m_CurrentPortalIdx);
		CFileMgr::Write(fileHandle, buf, istrlen(buf));
		formatf(buf, sizeof(buf)-1, "%u\n", m_CurrentIntProxySlot);
		CFileMgr::Write(fileHandle, buf, istrlen(buf));
		formatf(buf, sizeof(buf)-1, "%u\n", CInteriorProxy::GetPool()->GetSize());
		CFileMgr::Write(fileHandle, buf, istrlen(buf));
		CFileMgr::CloseFile(fileHandle);
	}
}

u32 naOcclusionManager::GetNumFailedAttemptsAtBuildStep(naOcclusionBuildStep buildStep, u32 intProxyHash)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	u32 numAttempts = 0;

	for(s32 i = 0; i < m_FailureList.GetCount(); i++)
	{
		naAssertf(m_FailureList[i].buildStep < NUM_AUD_OCC_BUILD_STEPS, "Invalid naOcclusionBuildStep %u", m_FailureList[i].buildStep);
		if(m_FailureList[i].buildStep > AUD_OCC_BUILD_STEP_START)
		{
			if(m_FailureList[i].buildStep <= AUD_OCC_BUILD_STEP_INTRA_PATHS && buildStep <= AUD_OCC_BUILD_STEP_INTRA_PATHS && m_FailureList[i].uniqueInteriorHash == intProxyHash)
			{
				numAttempts++;
			}

			if(m_FailureList[i].buildStep >= AUD_OCC_BUILD_STEP_INTER_PATHS && m_FailureList[i].buildStep <= AUD_OCC_BUILD_STEP_INTER_UNLOAD 
				&& buildStep >= AUD_OCC_BUILD_STEP_INTER_PATHS && buildStep <= AUD_OCC_BUILD_STEP_INTER_PATHS 
				&& m_FailureList[i].uniqueInteriorHash == intProxyHash)
			{
				numAttempts++;
			}

			if(m_FailureList[i].buildStep >= AUD_OCC_BUILD_STEP_TUNNEL_PATHS && m_FailureList[i].buildStep <= AUD_OCC_BUILD_STEP_TUNNEL_UNLOAD
				&& buildStep >= AUD_OCC_BUILD_STEP_TUNNEL_PATHS && buildStep <= AUD_OCC_BUILD_STEP_TUNNEL_UNLOAD
				&& m_FailureList[i].uniqueInteriorHash == intProxyHash)
			{
				numAttempts++;
			}
		}
	}

	return numAttempts;
}

naOcclusionBuildStep naOcclusionManager::UpdateOcclusionBuildProcess()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	switch(m_CurrentOcclusionBuildStep)
	{
		case AUD_OCC_BUILD_STEP_START:
		{
			naDisplayf("Interiors to build paths for:");
			for(s32 i = 0; i < CInteriorProxy::GetPool()->GetSize(); i++)
			{
				const CInteriorProxy* intProxy1 = CInteriorProxy::GetPool()->GetSlot(i);
				if(intProxy1)
				{
					Vec3V pos1;
					intProxy1->GetPosition(pos1);
					const u32 uniqueHashkey1 = GetUniqueProxyHashkey(intProxy1);
					naDisplayf("%s - Unique hashkey: %u at pos: %f %f %f", intProxy1->GetModelName(), uniqueHashkey1, pos1.GetXf(), pos1.GetYf(), pos1.GetZf());

					for(s32 j = 0; j < CInteriorProxy::GetPool()->GetSize(); j++)
					{
						const CInteriorProxy* intProxy2 = CInteriorProxy::GetPool()->GetSlot(j);
						if(intProxy2 && intProxy1 != intProxy2)
						{
							Vec3V pos2;
							intProxy2->GetPosition(pos2);

							// Only look for collisions between interiors that don't have the same name and are in the same position,
							// which will be the case for interiors in the same place on different maps, such as dlc.
							if(intProxy1->GetNameHash() != intProxy2->GetNameHash() || !IsEqualAll(pos1, pos2))
							{
								naAssertf(uniqueHashkey1 != GetUniqueProxyHashkey(intProxy2), "\tHad a collision with %s - Unique hashkey: %u at pos: %f %f %f", 
									intProxy2->GetModelName(), GetUniqueProxyHashkey(intProxy2), pos2.GetXf(), pos2.GetYf(), pos2.GetZf());
							}
						}
					}
				}
			}

			m_CurrentIntProxySlot = 0;
			m_CurrentRoomIdx = 0;
			m_CurrentPortalIdx = 0;
			m_CurrentInteriorLoadStartFrame = 0;
			m_HasStartedWaitingForPortalToLoad = false;
			m_InteriorUnloadStartFrame = 0;
			m_InteriorLoadStartFrame = 0;
			m_SwapInteriorSetIndex = 0;
			m_HasInteriorUnloadTimerStarted = false;
			m_HasInteriorLoadTimerStarted = false;
			m_HasStartedLoadScene = false;
			m_FreezeInteriorLoading = false;

			UpdateActiveIPLSwapInteriorList();
			BuildIPLSwapHashkeyMap();

			for(s32 i = 0; i < CInteriorProxy::GetPool()->GetSize(); i++)
			{
				const CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(i);
				if(intProxy)
				{
					const bool shouldBuildIntraPaths = VerifyShouldBuildPathsForInterior(intProxy, AUD_OCC_BUILD_STEP_INTRA_PATHS);
					if(shouldBuildIntraPaths)
					{
						m_CurrentIntProxySlot = i;
						return AUD_OCC_BUILD_STEP_INTRA_LOAD;
					}
				}
			}

			for(s32 i = 0; i < CInteriorProxy::GetPool()->GetSize(); i++)
			{
				const CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(i);
				if(intProxy)
				{
					const bool shouldBuildInterPaths = VerifyShouldBuildPathsForInterior(intProxy, AUD_OCC_BUILD_STEP_INTER_PATHS);
					if(shouldBuildInterPaths)
					{
						m_CurrentIntProxySlot = i;
						return AUD_OCC_BUILD_STEP_INTER_PATHS;
					}
				}
			}

			for(s32 i = 0; i < CInteriorProxy::GetPool()->GetSize(); i++)
			{
				const CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(i);
				if(intProxy)
				{
					const bool shouldBuildTunnelPaths = VerifyShouldBuildPathsForInterior(intProxy, AUD_OCC_BUILD_STEP_TUNNEL_PATHS);
					if(shouldBuildTunnelPaths)
					{
						m_CurrentIntProxySlot = i;
						return AUD_OCC_BUILD_STEP_TUNNEL_PATHS;
					}
				}
			}

			return AUD_OCC_BUILD_STEP_FINISH;
		}
		case AUD_OCC_BUILD_STEP_INTRA_LOAD:
		{
			if(m_CurrentIntProxySlot >= CInteriorProxy::GetPool()->GetSize())
			{
				m_CurrentIntProxySlot = 0;
				return AUD_OCC_BUILD_STEP_INTER_PATHS;
			}

			CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(m_CurrentIntProxySlot);

			const bool shouldBuildIntraPaths = VerifyShouldBuildPathsForInterior(intProxy, AUD_OCC_BUILD_STEP_INTRA_PATHS);
			if(shouldBuildIntraPaths)
			{
				// If this interior is swapped out, then go to the next swap set until we find one where it's not swapped out.
				// If it's swapped out and we've gone though all the sets, then we're done, so just build the paths
				const bool isProxyInIPLSwapList = IsInteriorInIPLSwapList(intProxy);
				if(isProxyInIPLSwapList)
				{
					naVerifyf(IncrementSwapSet(), "We should have already built paths if we didn't have anymore valid swap sets");
					UpdateActiveIPLSwapInteriorList();
					return AUD_OCC_BUILD_STEP_INTRA_LOAD;
				}

				TeleportPedToCurrentInterior(intProxy);

				// Only need to load this interior since we're only building the paths that are formed within this interior, so use path depth of 1
				bool areInteriorsLoaded = LoadInteriorsForPathBuild(AUD_OCC_BUILD_STEP_INTRA_LOAD, intProxy, CInteriorProxy::PS_FULL_WITH_COLLISIONS, 1);
				if(areInteriorsLoaded)
				{
					// Creates the naOcclusionInteriorInfo objects for all nearby interiors
					CreateInteriorInfoEntries();

					// Based on the # of loaded interiors we have, resize the map accordingly
					//RecomputeMetadataMap();

					// Resize the metadata stores to be able to hold all the data for all nearby/loaded portals and their entities
					// We store pointers to the metadata arrays, so we can't use grow or else we'll break all our pointer relationships
					CreateMetadataContainers();

					return AUD_OCC_BUILD_STEP_INTRA_PORTALS;
				}
			}
			else
			{
				GoToNextInteriorProxy();
			}

			return AUD_OCC_BUILD_STEP_INTRA_LOAD;
		}
		case AUD_OCC_BUILD_STEP_INTRA_PORTALS:
		{
			CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(m_CurrentIntProxySlot);

			TeleportPedToCurrentPortal(intProxy);

			// Only need to load this interior since we're only building the paths that are formed within this interior, so use path depth of 1
			bool areInteriorsLoaded = LoadInteriorsForPathBuild(AUD_OCC_BUILD_STEP_INTRA_PORTALS, intProxy, CInteriorProxy::PS_FULL_WITH_COLLISIONS, 1);
			if(areInteriorsLoaded)
			{
				// Need to wait a bit for all the windows and doors in the exterior scene to load in
				const bool areEntitiesLoaded = ArePortalEntitiesLoaded(intProxy);
				if(!m_HasStartedWaitingForPortalToLoad)
				{
					for(s32 i = 0; i < CInteriorProxy::GetPool()->GetSize(); i++)
					{
						CInteriorProxy* loadedIntProxy = CInteriorProxy::GetPool()->GetSlot(i);
						if(loadedIntProxy && loadedIntProxy->GetCurrentState() == CInteriorProxy::PS_FULL_WITH_COLLISIONS)
						{
							naDisplayf("Interior %s Loaded", loadedIntProxy->GetModelName());
						}
					}

					m_CurrentInteriorLoadStartFrame = fwTimer::GetFrameCount();
					m_HasStartedWaitingForPortalToLoad = true;
				}

				if((areEntitiesLoaded && fwTimer::GetFrameCount() > m_CurrentInteriorLoadStartFrame + 5)
					|| fwTimer::GetFrameCount() > m_CurrentInteriorLoadStartFrame + 3000)
				{
					m_HasStartedWaitingForPortalToLoad = false;

					CreatePortalInfoEntry(intProxy, m_CurrentRoomIdx, m_CurrentPortalIdx);

					m_CurrentPortalIdx++;
					if(m_CurrentPortalIdx >= intProxy->GetInteriorInst()->GetNumPortalsInRoom(m_CurrentRoomIdx))
					{
						m_CurrentPortalIdx = 0;
						m_CurrentRoomIdx++;
						if(m_CurrentRoomIdx >= intProxy->GetInteriorInst()->GetNumRooms())
						{
							m_CurrentPortalIdx = 0;
							m_CurrentRoomIdx = 0;

							return AUD_OCC_BUILD_STEP_INTRA_PATHS;
						}
					}
				}
			}

			return AUD_OCC_BUILD_STEP_INTRA_PORTALS;

		}
		case AUD_OCC_BUILD_STEP_INTRA_PATHS:
		{		
			CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(m_CurrentIntProxySlot);

			TeleportPedToCurrentInterior(intProxy);

			// Only need to load this interior since we're only building the paths that are formed within this interior, so use path depth of 1
			bool areInteriorsLoaded = LoadInteriorsForPathBuild(AUD_OCC_BUILD_STEP_INTRA_PATHS, intProxy, CInteriorProxy::PS_FULL_WITH_COLLISIONS, 1);
			if(areInteriorsLoaded)
			{
				BuildIntraInteriorPaths(intProxy);

				// See if we have any interiors that were swapped, or can swap.
				// If we do then increment the swap set, see if we can use that swap set for the current interior, and if so
				// then unload everything so that we can build again
				if(IsAnySwapInteriorLoaded() && IncrementSwapSet())
				{
					m_FreezeInteriorLoading = true;
					m_OcclusionPathKeyTracker.Reset();
					return AUD_OCC_BUILD_STEP_INTRA_UNLOAD;
				}

				WriteOcclusionDataForInteriorToFile(intProxy, true);
				WriteInteriorToBuiltPathsFile(intProxy, AUD_OCC_BUILD_STEP_INTRA_PATHS);
				GoToNextInteriorProxy();

				if(IsAnySwapInteriorLoaded())
				{
					m_FreezeInteriorLoading = true;
					return AUD_OCC_BUILD_STEP_INTRA_UNLOAD;
				}

				return AUD_OCC_BUILD_STEP_INTRA_LOAD;
			}
			
			return AUD_OCC_BUILD_STEP_INTRA_PATHS;
		}
		case AUD_OCC_BUILD_STEP_INTRA_UNLOAD:
		{
			if(!m_HasInteriorUnloadTimerStarted)
			{
				m_HasInteriorUnloadTimerStarted = true;
				m_InteriorUnloadStartFrame = fwTimer::GetFrameCount();
			}

			CPed* playerPed = FindPlayerPed();
			playerPed->Teleport(Vector3(-5000.0f, -5000.0f, 500.0f), playerPed->GetCurrentHeading());
			UnloadInteriorsForPathBuild();

			if(fwTimer::GetFrameCount() >= m_InteriorUnloadStartFrame + g_NumFramesToUnloadInteriors)
			{
				// We already incremented the swap set, but process and change the active list here because we need to 
				// get away from the other interiors or else we crash due to bad link portals
				UpdateActiveIPLSwapInteriorList();
				m_FreezeInteriorLoading = false;
				m_HasInteriorUnloadTimerStarted = false;
				return AUD_OCC_BUILD_STEP_INTRA_LOAD;
			}

			return AUD_OCC_BUILD_STEP_INTRA_UNLOAD;
		}
		case AUD_OCC_BUILD_STEP_INTER_PATHS:
		{
			if(m_CurrentIntProxySlot >= CInteriorProxy::GetPool()->GetSize())
			{
				naDisplayf("Finished building inter interior occlusion paths");
				m_CurrentIntProxySlot = 0;

				return AUD_OCC_BUILD_STEP_TUNNEL_PATHS;
			}

			CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(m_CurrentIntProxySlot);

			// Will return false if intProxy is NULL
			const bool shouldBuildInterPaths = VerifyShouldBuildPathsForInterior(intProxy, AUD_OCC_BUILD_STEP_INTER_PATHS);
			if(shouldBuildInterPaths)
			{
				// If this interior is swapped out, then go to the next swap set until we find one where it's not swapped out.
				// If it's swapped out and we've gone though all the sets, then we're done, so just build the paths
				const bool isProxyInIPLSwapList = IsInteriorInIPLSwapList(intProxy);
				if(isProxyInIPLSwapList)
				{
					naVerifyf(IncrementSwapSet(), "We should have already built paths if we didn't have anymore valid swap sets");
					UpdateActiveIPLSwapInteriorList();
					return AUD_OCC_BUILD_STEP_INTER_PATHS;
				}

				TeleportPedToCurrentInterior(intProxy);

				// Need to load all interiors we could eventually reach otherwise we won't get full paths because a CInteriorInst is streamed out or not populated
				const bool areInteriorsLoaded = LoadInteriorsForPathBuild(AUD_OCC_BUILD_STEP_INTER_PATHS, intProxy, CInteriorProxy::PS_FULL, sm_MaxOcclusionFullPathDepth);
				if(areInteriorsLoaded)
				{
					CreateInteriorInfoEntries();
					//RecomputeMetadataMap();

					// We have already written the intra-interior paths to file, so load that metadata
					LoadMetadataContainersFromFile();

					BuildInterInteriorPaths(intProxy);

					// See if we have any interiors that were swapped, or can swap.
					// If we do then increment the swap set, see if we can use that swap set for the current interior, and if so
					// then unload everything so that we can build again.
					if(IsAnySwapInteriorLoaded() && IncrementSwapSet())
					{
						m_FreezeInteriorLoading = true;
						m_OcclusionPathKeyTracker.Reset();
						return AUD_OCC_BUILD_STEP_INTER_UNLOAD;
					}

					WriteOcclusionDataForInteriorToFile(intProxy, false);
					WriteInteriorToBuiltPathsFile(intProxy, AUD_OCC_BUILD_STEP_INTER_PATHS);
					GoToNextInteriorProxy();

					m_FreezeInteriorLoading = true;
					return AUD_OCC_BUILD_STEP_INTER_UNLOAD;
				}
			}
			else
			{
				GoToNextInteriorProxy();
			}

			return AUD_OCC_BUILD_STEP_INTER_PATHS;
		}
		// Since we're force loading a bunch of assets, if we don't wait for them to unload, then we'll run out of streamed archetypes every other interior
		case AUD_OCC_BUILD_STEP_INTER_UNLOAD:
		{
			if(!m_HasInteriorUnloadTimerStarted)
			{
				m_HasInteriorUnloadTimerStarted = true;
				m_InteriorUnloadStartFrame = fwTimer::GetFrameCount();
			}

			CPed* playerPed = FindPlayerPed();
			playerPed->Teleport(Vector3(-5000.0f, -5000.0f, 500.0f), playerPed->GetCurrentHeading());
			UnloadInteriorsForPathBuild();

			if(fwTimer::GetFrameCount() >= m_InteriorUnloadStartFrame + g_NumFramesToUnloadInteriors)
			{
				// We already incremented the swap set, but process and change the active list here because we need to 
				// get away from the other interiors or else we crash due to bad link portals
				UpdateActiveIPLSwapInteriorList();
				m_FreezeInteriorLoading = false;
				m_HasInteriorUnloadTimerStarted = false;
				return AUD_OCC_BUILD_STEP_INTER_PATHS;
			}

			return AUD_OCC_BUILD_STEP_INTER_UNLOAD;
		}
		// This step builds all the paths between linked interiors that the inter-interior paths didn't pick up:
		// So a tunnel interior arrangement is:   LIMBO <-> A <-> B <-> C <-> D <-> E <-> LIMBO
		// Intra Interior path build will build A -> LIMBO, 
		// Intra Interior path build will NOT build B -> LIMBO, because A -> LIMBO may not have been built yet depending on the order it builds the interiors.
		// Inter Interior path build will build B -> LIMBO, because it builds B -> A and A -> LIMBO is already built from the intra path build.
		// Inter Interior path build will NOT build C -> LIMBO, because B -> LIMBO may not have been built yet depending on the order it builds the interiors.
		// Tunnel path build will build C -> LIMBO because it checks C -> B and B -> LIMBO was built during the inter-interior path build so it now has C -> LIMBO.
		case AUD_OCC_BUILD_STEP_TUNNEL_PATHS:		
		{
			if(m_CurrentIntProxySlot >= CInteriorProxy::GetPool()->GetSize())
			{
				naDisplayf("Finished building tunnel occlusion paths");
				return AUD_OCC_BUILD_STEP_FINISH;
			}

			CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(m_CurrentIntProxySlot);

			// Will return false if intProxy is NULL
			const bool shouldBuildTunnelPaths = VerifyShouldBuildPathsForInterior(intProxy, AUD_OCC_BUILD_STEP_TUNNEL_PATHS);
			if(shouldBuildTunnelPaths)
			{
				// If this interior is swapped out, then go to the next swap set until we find one where it's not swapped out.
				// If it's swapped out and we've gone though all the sets, then we're done, so just build the paths
				const bool isProxyInIPLSwapList = IsInteriorInIPLSwapList(intProxy);
				if(isProxyInIPLSwapList)
				{
					naVerifyf(IncrementSwapSet(), "We should have already built paths if we didn't have anymore valid swap sets");
					UpdateActiveIPLSwapInteriorList();
					return AUD_OCC_BUILD_STEP_TUNNEL_PATHS;
				}

				TeleportPedToCurrentInterior(intProxy);

				// Need to load all interiors we could eventually reach otherwise we won't get full paths because a CInteriorInst is streamed out or not populated
				const bool areInteriorsLoaded = LoadInteriorsForPathBuild(AUD_OCC_BUILD_STEP_TUNNEL_PATHS, intProxy, CInteriorProxy::PS_FULL, sm_MaxOcclusionFullPathDepth);
				if(areInteriorsLoaded)
				{
					CreateInteriorInfoEntries();
					//RecomputeMetadataMap();

					// We have already written the intra-interior paths to file, so load that metadata
					LoadMetadataContainersFromFile();

					BuildInterInteriorPaths(intProxy);

					// See if we have any interiors that were swapped, or can swap.
					// If we do then increment the swap set, see if we can use that swap set for the current interior, and if so
					// then unload everything so that we can build again.
					if(IsAnySwapInteriorLoaded() && IncrementSwapSet())
					{
						m_FreezeInteriorLoading = true;
						m_OcclusionPathKeyTracker.Reset();
						return AUD_OCC_BUILD_STEP_TUNNEL_UNLOAD;
					}

					WriteOcclusionDataForInteriorToFile(intProxy, false);
					WriteInteriorToBuiltPathsFile(intProxy, AUD_OCC_BUILD_STEP_TUNNEL_PATHS);
					GoToNextInteriorProxy();

					m_FreezeInteriorLoading = true;
					return AUD_OCC_BUILD_STEP_TUNNEL_UNLOAD;
				}
			}
			else
			{
				GoToNextInteriorProxy();
			}

			return AUD_OCC_BUILD_STEP_TUNNEL_PATHS;
		}
		case AUD_OCC_BUILD_STEP_TUNNEL_UNLOAD:
		{
			if(!m_HasInteriorUnloadTimerStarted)
			{
				m_HasInteriorUnloadTimerStarted = true;
				m_InteriorUnloadStartFrame = fwTimer::GetFrameCount();
			}

			CPed* playerPed = FindPlayerPed();
			playerPed->Teleport(Vector3(-5000.0f, -5000.0f, 500.0f), playerPed->GetCurrentHeading());
			UnloadInteriorsForPathBuild();

			if(fwTimer::GetFrameCount() >= m_InteriorUnloadStartFrame + g_NumFramesToUnloadInteriors)
			{
				// We already incremented the swap set, but process and change the active list here because we need to 
				// get away from the other interiors or else we crash due to bad link portals
				UpdateActiveIPLSwapInteriorList();
				m_FreezeInteriorLoading = false;
				m_HasInteriorUnloadTimerStarted = false;
				return AUD_OCC_BUILD_STEP_TUNNEL_PATHS;
			}

			return AUD_OCC_BUILD_STEP_TUNNEL_UNLOAD;
		}
		default:
		{
			naAssertf(0, "Hit default case in audio occlusion build process");
			return AUD_OCC_BUILD_STEP_FINISH;
		}
	}
}

bool naOcclusionManager::ShouldAllowInteriorToLoad(const CInteriorProxy* intProxy) const
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	return !m_FreezeInteriorLoading && !IsInteriorInIPLSwapList(intProxy);
}

void naOcclusionManager::UpdateActiveIPLSwapInteriorList()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	m_ActiveInteriorSwapList.Reset();
	atArray<u32>& swapSet = g_SwapInteriorList[m_SwapInteriorSetIndex];
	for(s32 i = 0; i < swapSet.GetCount(); i++)
	{
		m_ActiveInteriorSwapList.PushAndGrow(swapSet[i]);
	}
}

void naOcclusionManager::BuildIPLSwapHashkeyMap()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	u32 v_31_tun_swapHashkey = 0;
	u32 v_31_Tun_02Hashkey = 0;
	for(s32 i = 0; i < CInteriorProxy::GetPool()->GetSize(); i++)
	{
		const CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(i);
		if(intProxy && intProxy->GetNameHash() == atStringHash("v_31_tun_swap"))
		{
			v_31_tun_swapHashkey = GetUniqueProxyHashkey(intProxy);
		}
		if(intProxy && intProxy->GetNameHash() == atStringHash("v_31_Tun_02"))
		{
			v_31_Tun_02Hashkey = GetUniqueProxyHashkey(intProxy);
		}
	}

	m_SwappedInteriorHashkey.Insert(v_31_tun_swapHashkey, v_31_Tun_02Hashkey);
}

bool naOcclusionManager::IsAnySwapInteriorLoaded() const
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	for(s32 i = 0; i < CInteriorProxy::GetPool()->GetSize(); i++)
	{
		const CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(i);
		if(intProxy && intProxy->GetInteriorInst() && intProxy->GetInteriorInst()->IsPopulated())
		{
			for(s32 j = 0; j < g_SwapInteriorList.GetMaxCount(); j++)
			{
				atArray<u32>& swapSet = g_SwapInteriorList[j];
				for(s32 k = 0; k < swapSet.GetCount(); k++)
				{
					if(swapSet[k] == intProxy->GetNameHash())
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool naOcclusionManager::IncrementSwapSet()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	bool haveValidSwapSet = false;
	const CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(m_CurrentIntProxySlot);
	while(naVerifyf(intProxy, "NULL m_CurrentIntProxySlot") && !haveValidSwapSet && m_SwapInteriorSetIndex < g_SwapInteriorList.GetMaxCount()-1)
	{
		m_SwapInteriorSetIndex++;
		haveValidSwapSet = true;
		atArray<u32>& swapSet = g_SwapInteriorList[m_SwapInteriorSetIndex];
		for(s32 i = 0; i < swapSet.GetCount(); i++)
		{
			if(intProxy->GetNameHash() == swapSet[i])
			{
				haveValidSwapSet = false;
			}
		}
	}
	return haveValidSwapSet;
}

u32 naOcclusionManager::GetSwappedHashForInterior(u32 destInteriorHash) const
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	const u32* swappedHash = m_SwappedInteriorHashkey.Access(destInteriorHash);
	if(swappedHash)
	{
		return *swappedHash;
	}
	else
	{
		return destInteriorHash;
	}
}

void naOcclusionManager::TeleportPedToCurrentInterior(const CInteriorProxy* intProxy)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	naAssertf(intProxy, "Passed NULL CInteriorProxy* when building interior paths");
	naDisplayf("Teleporting/Loading nearby and linked interiors for %s", intProxy->GetModelName());

	CPed* playerPed = FindPlayerPed();
	if(playerPed)
	{
		Vec3V intPos;
		intProxy->GetPosition(intPos);
		if(!m_HasStartedLoadScene)
		{		
			const Vec3V lookAtPos = playerPed->GetTransform().GetForward();
			g_LoadScene.Start(intPos, lookAtPos, 10.0f, false, 0, CLoadScene::LOADSCENE_OWNER_DEBUG);
			m_HasStartedLoadScene = true;
		}
		playerPed->Teleport(VEC3V_TO_VECTOR3(intPos), playerPed->GetCurrentHeading());
	}
}

void naOcclusionManager::TeleportPedToCurrentPortal(const CInteriorProxy* intProxy)
{	
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	const CInteriorInst* intInst = intProxy->GetInteriorInst();
	if(intInst && intInst->IsPopulated() && intInst->GetNumRooms() && intInst->GetNumPortals() && intInst->GetNumPortalsInRoom(m_CurrentRoomIdx) > 0)
	{
		s32 globalPortalIdx = intInst->GetPortalIdxInRoom(m_CurrentRoomIdx, m_CurrentPortalIdx);
		fwPortalCorners portalCorners = intProxy->GetInteriorInst()->GetPortal(globalPortalIdx);
		Vector3 portalCenter = portalCorners.GetCorner(0) + portalCorners.GetCorner(1) + portalCorners.GetCorner(2) + portalCorners.GetCorner(3);
		portalCenter.Scale(0.25f);

		// Get the normal from the plane defined by the portal
		Vector4 portalPlane;
		portalPlane.ComputePlane(portalCorners.GetCorner(0), portalCorners.GetCorner(1), portalCorners.GetCorner(2));
		Vector3 portalNormal(portalPlane.x, portalPlane.y, portalPlane.z);

		Vector3 position = portalCenter + portalNormal;
		f32 heading = fwAngle::GetRadianAngleBetweenPoints(portalCenter.x, portalCenter.y, position.x, position.y);
		Vector3 lookAtPosition = portalCenter;

		CPed* playerPed = FindPlayerPed();
		if(playerPed)
		{
			playerPed->Teleport(position, heading);
		}

		camInterface::GetGameplayDirector().SetRelativeCameraHeading();
		camInterface::GetGameplayDirector().SetRelativeCameraPitch();
	}
	else
	{
		TeleportPedToCurrentInterior(intProxy);
	}
}

bool naOcclusionManager::VerifyShouldBuildPathsForInterior(const CInteriorProxy* intProxy, const naOcclusionBuildStep buildStep)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	bool shouldBuildPaths = false;
	if(intProxy)
	{ 
		const u32 uniqueProxyHashkey = GetUniqueProxyHashkey(intProxy);

		// Check if we were able to build intra interior paths without failing
		const bool isInDoNotUseList = IsInteriorInDoNotUseList(uniqueProxyHashkey);

		const bool isInBuildList = IsInteriorInBuildList(uniqueProxyHashkey);

		const bool isProcessed = IsPathDataBuiltForInterior(intProxy, buildStep);

		const bool isBuildStepEnabled = IsBuildStepEnabledForInterior(intProxy, buildStep);

		if(!isInDoNotUseList && isInBuildList && !isProcessed && isBuildStepEnabled && !intProxy->GetIsDisabledByDLC())
		{
			shouldBuildPaths = true;
		}

		if(isInDoNotUseList)
		{
			naDisplayf("Skipping %s as it's in the DO NOT USE list due to failing at a previous build step", intProxy->GetModelName());
		}
		if(isProcessed)
		{
			naDisplayf("Paths for Interior: %s at build step: %u are already built, moving to next interior", intProxy->GetModelName(), buildStep);
		}
		if(!isBuildStepEnabled)
		{
			naDisplayf("Build step is disabled for interior: %s, skipping", intProxy->GetModelName());
		}
		if(intProxy->GetIsDisabledByDLC())
		{
			naDisplayf("Interior is disabled by DLC: %s skipping", intProxy->GetModelName());
		}
	}

	return shouldBuildPaths;
}

bool naOcclusionManager::IsPathDataBuiltForInterior(const CInteriorProxy* intProxy, const naOcclusionBuildStep buildStep)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	bool isPathDataBuilt = false;
	if(intProxy)
	{
		// Now see if we've already built this interior

		FileHandle fileHandle;
		switch(buildStep)
		{
		case AUD_OCC_BUILD_STEP_INTRA_LOAD:
		case AUD_OCC_BUILD_STEP_INTRA_PORTALS:
		case AUD_OCC_BUILD_STEP_INTRA_PATHS:
			{
				fileHandle = CFileMgr::OpenFile(g_OcclusionBuiltIntraPathsFile);
				break;
			}
		case AUD_OCC_BUILD_STEP_INTER_PATHS:
		case AUD_OCC_BUILD_STEP_INTER_UNLOAD:
			{
				fileHandle = CFileMgr::OpenFile(g_OcclusionBuiltInterPathsFile);
				break;
			}
		case AUD_OCC_BUILD_STEP_TUNNEL_PATHS:
		case AUD_OCC_BUILD_STEP_TUNNEL_UNLOAD:
			{
				fileHandle = CFileMgr::OpenFile(g_OcclusionBuiltTunnelPathsFile);
				break;
			}
		default:
			{
				naAssertf(0, "Shouldn't be in default case here");
				fileHandle = NULL;
			}
		}

		if(CFileMgr::IsValidFileHandle(fileHandle))
		{
			char* line;
			// Parse through and see if we've already built the portal info for this interior
			while ((line = LoadNextOcclusionPathFileLine(fileHandle)) != NULL)
			{
				u32 intProxyHashkey;
				(void)naVerifyf(sscanf(line, "%u", &intProxyHashkey) == 1, "Read incorrect amount of values loading built path data");
				if(GetUniqueProxyHashkey(intProxy) == intProxyHashkey)
				{
					isPathDataBuilt = true;
				}
			}
			CFileMgr::CloseFile(fileHandle);
		}
	}

	return isPathDataBuilt;
}

void naOcclusionManager::GoToNextInteriorProxy()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	// Set that we don't need the interiors loaded for our current index
	UnloadInteriorsForPathBuild();

	// Clean out all the path and portalinfo data as we'll generate whatever we need again for our next interior
	naOcclusionInteriorInfo::GetPool()->DeleteAll();
	naOcclusionPathNode::GetPool()->DeleteAll();
	naOcclusionPortalInfo::GetPool()->DeleteAll();
	naOcclusionPortalEntity::GetPool()->DeleteAll();
	m_OcclusionPathKeyTracker.Reset();
	m_LoadedInteriorInfos.Reset();
	sm_InteriorMetadataLoadList.Reset();
	m_InteriorMetadata.Kill();
	m_HasStartedLoadScene = false;
	m_SwapInteriorSetIndex = 0;
	UpdateActiveIPLSwapInteriorList();

	// Now advance our slot idx
	CInteriorProxy* intProxy = NULL;
	while(!intProxy && ++m_CurrentIntProxySlot < CInteriorProxy::GetPool()->GetSize())
	{
		intProxy = CInteriorProxy::GetPool()->GetSlot(m_CurrentIntProxySlot);
	}
	m_CurrentRoomIdx = 0;
	m_CurrentPortalIdx = 0;
}

bool naOcclusionManager::IsInteriorInDoNotUseList(const u32 intProxyHashkey) const
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	for(s32 i = 0; i < m_InteriorDoNotUseList.GetCount(); i++)
	{
		if(intProxyHashkey == m_InteriorDoNotUseList[i])
		{
			return true;
		}
	}
	return false;
}

bool naOcclusionManager::IsInteriorInBuildList(const u32 intProxyHashkey) const
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	for(s32 i = 0; i < m_InteriorBuildList.GetCount(); i++)
	{
		if(intProxyHashkey == m_InteriorBuildList[i])
		{
			return true;
		}
	}
	return false;
}

bool naOcclusionManager::IsBuildStepEnabledForInterior(const CInteriorProxy* intProxy, const naOcclusionBuildStep buildStep) const
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	bool isBuildStepEnabled = true;
	if(naVerifyf(intProxy, "Passed NULL CInteriorProxy*"))
	{
		const InteriorSettings* intSettings = audNorthAudioEngine::GetObject<InteriorSettings>(intProxy->GetNameHash());
		if(buildStep > AUD_OCC_BUILD_STEP_INTRA_PATHS 
			&& intSettings
			&& AUD_GET_TRISTATE_VALUE(intSettings->Flags, FLAG_ID_INTERIORSETTINGS_BUILDINTERINTERIORPATHS) == AUD_TRISTATE_FALSE)
		{
			isBuildStepEnabled = false;
		}
	}
	return isBuildStepEnabled;
}

bool naOcclusionManager::IsInteriorInIPLSwapList(const CInteriorProxy* intProxy) const
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	if(naVerifyf(intProxy, "Passed NULL CInteriorProxy*"))
	{
		for(s32 i = 0; i < m_ActiveInteriorSwapList.GetCount(); i++)
		{
			if(m_ActiveInteriorSwapList[i] == intProxy->GetNameHash())
			{
				return true;
			}
		}
	}
	return false;
}

bool naOcclusionManager::LoadInteriorsForPathBuild(const naOcclusionBuildStep buildStep, CInteriorProxy* intProxy, CInteriorProxy::eProxy_State requestedState, u8 maxDepth)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	bool areInteriorsLoaded = false;

	if(naVerifyf(intProxy, "Passed NULL proxy to be loaded for path build"))
	{
		if(!m_HasInteriorLoadTimerStarted)
		{
			m_HasInteriorLoadTimerStarted = true;
			m_InteriorLoadStartFrame = fwTimer::GetFrameCount();
		}

		// Load the main interior and all interiors linked to it
		Vec3V startPos(V_ZERO);
		areInteriorsLoaded = LoadInteriorAndLinkedInteriors(buildStep, intProxy, requestedState, 0, maxDepth, startPos);

		// Intentionally get stuck in an infinite loop here if it's the 1st time we're stuck - the occlusion builder machine will restart the game 
		// Don't cut losses if this the 1st time it's gotten stuck at this interior - as we'll probably be fine after a restart
		if(!areInteriorsLoaded
			&& fwTimer::GetFrameCount() > m_InteriorLoadStartFrame + 300 
			&& GetNumFailedAttemptsAtBuildStep((naOcclusionBuildStep)m_CurrentOcclusionBuildStep, GetUniqueProxyHashkey(intProxy)) > 0)
		{
			// Try loading again, and keep decrementing the path and loading at a lower proxy state, so we just cut losses if we can't load an interior that's far away
			s32 depthToLoad = maxDepth;
			while(depthToLoad >= 0 && !areInteriorsLoaded)
			{
				// Try loading at our requested depth with our requested state
				Vec3V startPos(V_ZERO);
				areInteriorsLoaded = LoadInteriorAndLinkedInteriors(buildStep, intProxy, requestedState, 0, (u8)depthToLoad, startPos);

				// For some reason, certain interiors just don't want to load, so try just loading at PS_FULL if we still don't have loaded interiors
				if(!areInteriorsLoaded)
				{
					Vec3V startPos2(V_ZERO);
					areInteriorsLoaded = LoadInteriorAndLinkedInteriors(buildStep, intProxy, CInteriorProxy::PS_FULL, 0, (u8)depthToLoad, startPos2);
				}

				depthToLoad--;
			}
		}

		if(areInteriorsLoaded)
		{
			m_HasInteriorLoadTimerStarted = false;
			m_InteriorLoadStartFrame = 0;
		}
	}

	return areInteriorsLoaded;
}

bool naOcclusionManager::LoadInteriorAndLinkedInteriors(const naOcclusionBuildStep buildStep, CInteriorProxy* intProxy, CInteriorProxy::eProxy_State requestedState, 
														u8 currentDepth, const u8 maxDepth, const Vec3V& currentPos, f32 currentDistance)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	bool areInteriorsLoaded = true;

	bool isInDoNotUseList = IsInteriorInDoNotUseList(GetUniqueProxyHashkey(intProxy));
	bool isInIPLSwapList = IsInteriorInIPLSwapList(intProxy);
	bool isBuildStepEnabled = IsBuildStepEnabledForInterior(intProxy, buildStep);
	if(intProxy && !isInDoNotUseList && !isInIPLSwapList && !intProxy->GetIsDisabledByDLC() && currentDepth <= maxDepth && currentDistance <= sm_MaxOcclusionPathDistance && isBuildStepEnabled)
	{
		// If this interior is loaded and we're still not up to the max path depth then keep digging down
		if(LoadInteriorPortalsAndPortalObjects(intProxy, requestedState))
		{
			currentDepth++;

			// We return false on LoadInteriorPortalsAndPortalObjects if we have a NULL InteriorInst* or it's not populated so no need to check again
			const CInteriorInst* intInst = intProxy->GetInteriorInst();	
			const CMloModelInfo* modelInfo = intInst->GetMloModelInfo();

			for(s32 roomIdx = 0; roomIdx < intInst->GetNumRooms(); roomIdx++)
			{
				for(s32 portalIdx = 0; portalIdx < intInst->GetNumPortalsInRoom(roomIdx); portalIdx++)
				{
					s32 globalPortalIdx = intInst->GetPortalIdxInRoom(roomIdx, portalIdx);
					fwPortalCorners portalCorners = intInst->GetPortal(globalPortalIdx);
					Vec3V portalCenter = VECTOR3_TO_VEC3V(Vector3(portalCorners.GetCorner(0) + portalCorners.GetCorner(1) + portalCorners.GetCorner(2) + portalCorners.GetCorner(3)));
					portalCenter = Scale(portalCenter, ScalarV(V_QUARTER));
					CPortalFlags portalFlags = modelInfo->GetPortalFlags(globalPortalIdx);

					f32 distanceForCurrentPortal = currentDistance;
					if(!IsZeroAll(currentPos))
					{
						f32 distFromLastPosToCurrentPortal = Mag(currentPos - portalCenter).Getf();
						distanceForCurrentPortal += distFromLastPosToCurrentPortal;
					}

					// If the portal goes outside, we need to try and get to every other interior within the path distance
					if(roomIdx == INTLOC_ROOMINDEX_LIMBO && !portalFlags.GetIsLinkPortal())
					{
						for(s32 slot = 0; slot < CInteriorProxy::GetPool()->GetSize(); slot++)
						{
							CInteriorProxy* destIntProxy = CInteriorProxy::GetPool()->GetSlot(slot);
							if(destIntProxy && destIntProxy != intProxy)
							{
								Vec3V destPortalPos = portalCenter;
								f32 portalToBoundDistance = 0.0f;

								spdAABB boundBox;
								destIntProxy->GetBoundBox(boundBox);

								const Vec3V boundCenter = boundBox.GetCenter();
								const Vec3V bountExtent = boundBox.GetExtent();
								const Vec3V portalToBoundCenter = boundCenter - portalCenter;

								const ScalarV boundExtentMag = Mag(bountExtent);
								const ScalarV portalToBoundCenterMag = Mag(portalToBoundCenter);
								const ScalarV portalToBoundMag = portalToBoundCenterMag - boundExtentMag;
								
								// Make sure we're not inside the proxy bounds - shouldn't be possible but who knows...
								if(IsGreaterThan(portalToBoundMag, ScalarV(V_ZERO)).Getb())
								{
									const Vec3V portalToBoundNormal = Normalize(portalToBoundCenter);
									const Vec3V portalToBound = Scale(portalToBoundNormal, portalToBoundMag);
									
									portalToBoundDistance =  portalToBoundMag.Getf();
									destPortalPos = portalCenter + portalToBound;								
								}

								// We're actually going through 2 portals here - inside -> outside -> back inside so increment the path depth
								if(!LoadInteriorAndLinkedInteriors(buildStep, destIntProxy, requestedState, currentDepth + 1, maxDepth, destPortalPos, distanceForCurrentPortal + portalToBoundDistance))
								{
									areInteriorsLoaded = false;
								}
							}
						}
					}

					// If the portal goes to a linked interior
					if(roomIdx != INTLOC_ROOMINDEX_LIMBO && portalFlags.GetIsLinkPortal())
					{
						CInteriorInst* destIntInst;
						s32 destRoomIdx;
						GetPortalDestinationInfo(intInst, roomIdx, portalIdx, &destIntInst, &destRoomIdx);

						if(destIntInst)
						{
							// Keep travelling through all the linked interiors until we have all linked interiors up to our max path distance loaded
							if(!LoadInteriorAndLinkedInteriors(buildStep, destIntInst->GetProxy(), requestedState, currentDepth, maxDepth, portalCenter, distanceForCurrentPortal))
							{
								areInteriorsLoaded = false;
							}
						}
						else
						{
							// We couldn't get the CInteriorInst*, which means the linked interior isn't loaded in, so we won't be able to compute paths and portal info.
							// So we'll need to force it to load, because in regular gameplay this interior will obviously be loaded in and we'll need paths for it.
							bool foundLinkedInteriorProxy = false;

							// We're right at the entrance to the linked portal, so this probe should be certain to find the linked CInteriorProxy, 
							// and then from there we can set it to load the CinteriorInst* via state changes
							fwIsSphereIntersecting intersection(spdSphere(portalCenter, ScalarV(V_ONE)));
							fwPtrListSingleLink	scanList;
							CInteriorProxy::FindActiveIntersecting(intersection, scanList);

							fwPtrNode* node = scanList.GetHeadPtr();
							while(node != NULL)
							{
								CInteriorProxy* probedIntProxy = reinterpret_cast<CInteriorProxy*>(node->GetPtr());
								if(probedIntProxy && probedIntProxy != intInst->GetProxy())
								{
									foundLinkedInteriorProxy = true;
									if(!LoadInteriorAndLinkedInteriors(buildStep, probedIntProxy, requestedState, currentDepth, maxDepth, portalCenter, distanceForCurrentPortal))
									{
										areInteriorsLoaded = false;
									}
								}
								node = node->GetNextPtr();
							}
							if(!naVerifyf(foundLinkedInteriorProxy, "Couldn't find linked interior with probe trying to load all nearby interiors"))
							{
								areInteriorsLoaded = false;
							}
						}
					}
				}
			}
		}
		else
		{
			areInteriorsLoaded = false;
		}
	}

	return areInteriorsLoaded;
}

void naOcclusionManager::UnloadInteriorsForPathBuild()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	for(s32 i = 0; i < CInteriorProxy::GetPool()->GetSize(); i++)
	{
		CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(i);
		if(intProxy)
		{
			intProxy->SetRequestedState(CInteriorProxy::RM_SCRIPT, CInteriorProxy::PS_NONE);
			intProxy->SetRequiredByLoadScene(false);
		}
	}
}

bool naOcclusionManager::LoadInteriorPortalsAndPortalObjects(CInteriorProxy* intProxy, CInteriorProxy::eProxy_State requestedState)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	if(intProxy && !intProxy->GetIsDisabledByDLC())
	{
		// Script driven interiors need to have their static bounds file requested manually, so just do it for all interiors we're trying to load.
		if(intProxy->GetStaticBoundsSlotIndex() != -1)
		{
			intProxy->SetRequiredByLoadScene(true);
			intProxy->RequestContainingImapFile();
			intProxy->RequestStaticBoundsFile();
		}

		intProxy->SetIsDisabled(false);
		intProxy->SetIsCappedAtPartial(false);

		// Use RM_SCRIPT request module, because that will pin that interior to be loaded until we decide we want to unload it
		intProxy->SetRequestedState(CInteriorProxy::RM_SCRIPT, requestedState);
		if(intProxy->GetCurrentState() >= (u32)requestedState && intProxy->GetInteriorInst() && intProxy->GetInteriorInst()->IsPopulated())
		{
			return true;
		}
	}
	return false;
}

void naOcclusionManager::WriteOcclusionDataForInteriorToFile(const CInteriorProxy* intProxy, const bool isBuildingIntraInteriorPaths)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	if(intProxy)
	{
		char buffer[255];
		formatf(buffer, sizeof(buffer)-1, "%s/%u.pso", g_OcclusionBuildDataDir, GetUniqueProxyHashkey(intProxy));

		naOcclusionInteriorInfo** interiorInfoPtr = m_LoadedInteriorInfos.Access(intProxy);
		naAssert(interiorInfoPtr && *interiorInfoPtr);

		naOcclusionInteriorMetadata interiorMetadata;

		(*interiorInfoPtr)->PopulateMetadataFromInteriorInfo(&interiorMetadata, isBuildingIntraInteriorPaths);

		USE_DEBUG_MEMORY();

		naVerifyf(PARSER.SaveObject(buffer, "meta", &interiorMetadata, parManager::XML), "Failed to save Occlusion Portal Info XML '%s'.", buffer);
	}
}

void naOcclusionManager::BuildIntraInteriorPaths(const CInteriorProxy* intProxy)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	if(intProxy)
	{
		const CInteriorInst* intInst = intProxy->GetInteriorInst();
		if(intInst)
		{
			for(u32 pathDepth = 1; pathDepth <= sm_MaxOcclusionPathDepth; pathDepth++)
			{
				for(u32 startRoomIdx = 0; startRoomIdx < intInst->GetNumRooms(); startRoomIdx++)
				{
					for(u32 finishRoomIdx = 0; finishRoomIdx < intInst->GetNumRooms(); finishRoomIdx++)
					{
						if(startRoomIdx != finishRoomIdx)
						{
							BuildOcclusionPathTreeForCombination(intInst, startRoomIdx, intInst, finishRoomIdx, pathDepth, false);
						}
					}
				}

				naDisplayf("Finished building intra interior paths for path depth %u", pathDepth);
				PrintMemoryInfo();
			}
		}
	}
}

void naOcclusionManager::BuildInterInteriorPaths(const CInteriorProxy* startIntProxy)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	if(startIntProxy)
	{
		const CInteriorInst* startIntInst = startIntProxy->GetInteriorInst();
		if(startIntInst)
		{
			// Only build our "full" paths, otherwise we'll end up with a million combinations if we're building paths 5 deep between different interiors
			for(u32 pathDepth = 1; pathDepth <= sm_MaxOcclusionFullPathDepth; pathDepth++)
			{
				for(s32 finishIntProxySlot = 0; finishIntProxySlot < CInteriorProxy::GetPool()->GetSize(); finishIntProxySlot++)
				{
					CInteriorProxy* finishIntProxy = CInteriorProxy::GetPool()->GetSlot(finishIntProxySlot);
					if(finishIntProxy)
					{
						bool isInDoNotUseList = IsInteriorInDoNotUseList(GetUniqueProxyHashkey(finishIntProxy));
						const bool isInIPLSwapList = IsInteriorInIPLSwapList(finishIntProxy);
						if(!isInDoNotUseList && !isInIPLSwapList)
						{
							// We should have all the CInteriorInst's loaded for any interior that's reachable within the sm_MaxPathDistance
							const CInteriorInst* finishIntInst = finishIntProxy->GetInteriorInst();
							if(finishIntInst)
							{
								// RoomIdx of 0 is Limbo or outside, so this loop will catch all the paths that start inside and go outside, and vice versa
								for(u32 startRoomIdx = 0; startRoomIdx < startIntInst->GetNumRooms(); startRoomIdx++)
								{
									for(u32 finishRoomIdx = 0; finishRoomIdx < finishIntInst->GetNumRooms(); finishRoomIdx++)
									{
										// Only build paths between interiors if we're not starting outside
										// OR
										// we're building paths that start outside and go back inside to our start interior
										if((startRoomIdx != INTLOC_ROOMINDEX_LIMBO && startIntInst != finishIntInst) 
											|| (startRoomIdx == INTLOC_ROOMINDEX_LIMBO && startIntInst == finishIntInst && finishRoomIdx != INTLOC_ROOMINDEX_LIMBO))
										{
											BuildOcclusionPathTreeForCombination(startIntInst, startRoomIdx, finishIntInst, finishRoomIdx, pathDepth, true);
										}
									}
								}
							}
						}
					}
				}

				naDisplayf("Finished building inter interior paths for path depth %u", pathDepth);
				PrintMemoryInfo();
			}
		}
	}
}

void naOcclusionManager::CreateInteriorInfoEntries()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	for(s32 i = 0; i < CInteriorProxy::GetPool()->GetSize(); i++)
	{
		CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(i);
		naOcclusionInteriorInfo** interiorInfoPtr = m_LoadedInteriorInfos.Access(intProxy);
		if(!interiorInfoPtr && intProxy && intProxy->GetInteriorInst() && intProxy->GetInteriorInst()->IsPopulated())
		{
			// We haven't loaded any data previously from a temp file and we have no metadata, 
			// so create a new naOcclusionInteriorInfo object to contain all the path data that we're about to build
			naOcclusionInteriorInfo* interiorInfo = naOcclusionInteriorInfo::Allocate();
			naAssertf(interiorInfo, "naOcclusionInteriorInfo pool is full when building occlusion paths");

			// Create an entry in the map and assign it to the data
			naOcclusionInteriorInfo** interiorInfoPtr = &m_LoadedInteriorInfos[intProxy];
			*interiorInfoPtr = interiorInfo;
		}
	}
}

void naOcclusionManager::RecomputeMetadataMap()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	s32 numLoadedInteriors = 0;
	for(s32 i = 0; i < CInteriorProxy::GetPool()->GetSize(); i++)
	{
		CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(i);
		if(intProxy && intProxy->GetInteriorInst() && intProxy->GetInteriorInst()->IsPopulated())
		{
			numLoadedInteriors++;
		}
	}

	m_InteriorMetadata.Recompute((unsigned short)numLoadedInteriors);
}

void naOcclusionManager::CreateMetadataContainers()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	for(s32 i = 0; i < CInteriorProxy::GetPool()->GetSize(); i++)
	{
		CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(i);
		if(intProxy && intProxy->GetInteriorInst() && intProxy->GetInteriorInst()->IsPopulated())
		{
			naOcclusionInteriorInfo** interiorInfoPtr = m_LoadedInteriorInfos.Access(intProxy);
			if(naVerifyf(interiorInfoPtr, "We should have already set up a naOcclusionInteriorInfo entry in our map"))
			{
				if(!(*interiorInfoPtr)->GetInteriorMetadata())
				{
					// Create the metadata, and reserve enough room where we never have to Grow() which rearranges things and messes up all our pointers to our metadata
					naOcclusionInteriorMetadata* interiorMetadata = &m_InteriorMetadata[intProxy];
					interiorMetadata->m_PortalInfoList.Reserve(naOcclusionPortalInfo::GetPool()->GetSize());

					(*interiorInfoPtr)->SetInteriorMetadata(interiorMetadata);
				}
			}
		}
	}
}

void naOcclusionManager::LoadMetadataContainersFromFile()
{		
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	// Get all the data we have so far, should be all the portalinfo's needed and all the intra interior paths for all the loaded interiors
	// If we have all the intra paths for all the loaded interiors, we should be able to find all the inter interior paths.
	for(s32 i = 0; i < CInteriorProxy::GetPool()->GetSize(); i++)
	{
		CInteriorProxy* intProxy = CInteriorProxy::GetPool()->GetSlot(i);
		if(intProxy 
			&& intProxy->GetInteriorInst() 
			&& intProxy->GetInteriorInst()->IsPopulated() 
			&& intProxy->GetCurrentState() >= CInteriorProxy::PS_FULL
			&& !IsInteriorInDoNotUseList(GetUniqueProxyHashkey(intProxy)))
		{
			naDisplayf("Loading occlusion path data for interior %s", intProxy->GetModelName());

			naOcclusionInteriorInfo** interiorInfoPtr = m_LoadedInteriorInfos.Access(intProxy);
			naAssertf(interiorInfoPtr && *interiorInfoPtr, "Don't have an naOcclusionInteriorInfo entry for proxy: %s", intProxy->GetModelName());

			if(!(*interiorInfoPtr)->GetInteriorMetadata())
			{
				// Create a new metadata entry in our map, and then fill it out from the file
				naOcclusionInteriorMetadata* interiorMetadata = &m_InteriorMetadata[intProxy];

				USE_DEBUG_MEMORY();

				// Try loading from the asset directory that we've specified in the occlusion tool, if we can't find it then fall back to the default asset directory
				bool doesObjectExist = true;

				char filename[RAGE_MAX_PATH];
				formatf(filename, sizeof(filename)-1, "%s/%u.pso", g_OcclusionBuildDataDir, GetUniqueProxyHashkey(intProxy));
				if(!ASSET.Exists(filename, "meta"))
				{
					formatf(filename, sizeof(filename)-1, "assets:/export/audio/occlusion/%u.pso", GetUniqueProxyHashkey(intProxy));
					if(!naVerifyf(ASSET.Exists(filename, "meta"), "Unable to find interior metadata for: %s in either asset directories: %s/%u.pso  or  %s", 
						intProxy->GetModelName(), g_OcclusionBuildDataDir, GetUniqueProxyHashkey(intProxy), filename))
					{
						doesObjectExist = false;
					}
				}

				if(doesObjectExist)
				{
					if(naVerifyf(PARSER.LoadObject(filename, "meta", (*interiorMetadata)), "Unable to LoadObject: %s", filename))
					{
						(*interiorInfoPtr)->InitFromMetadata(interiorMetadata);
					}
				}
			}
		}
	}
}

naOcclusionPortalInfo* naOcclusionManager::CreatePortalInfoEntry(const CInteriorProxy* intProxy, const s32 roomIdx, const u32 portalIdx)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	const u32 uniqueProxyHashkey = GetUniqueProxyHashkey(intProxy);
	// This will go through the pool and find the portal info that matches.  This is especially good because all naOcclusionInteriorInfo's store their portalinfo's in that pool, 
	// so if we're building an inter-interior path then we'll already have that portal info with all the correct entity info loaded 
	naOcclusionPortalInfo* portalInfo = GetPortalInfoFromParameters(uniqueProxyHashkey, roomIdx, portalIdx);
	if(!portalInfo)
	{
		naOcclusionInteriorInfo** interiorInfoPtr = m_LoadedInteriorInfos.Access(intProxy);
		if(interiorInfoPtr && *interiorInfoPtr)
		{
			// Get destination to where this portal leads
			CInteriorInst* destIntInst;
			s32 destRoomIdx;
			audNorthAudioEngine::GetOcclusionManager()->GetPortalDestinationInfo(intProxy->GetInteriorInst(), roomIdx, portalIdx, &destIntInst, &destRoomIdx);

			if(destIntInst && destRoomIdx != INTLOC_INVALID_INDEX)
			{
				const u32 destUniqueProxyHashkey = GetUniqueProxyHashkey(destIntInst->GetProxy());
				const bool isDestinationInDoNotUseList = IsInteriorInDoNotUseList(destUniqueProxyHashkey);
				const bool isDestinationInIPLSwapList = IsInteriorInIPLSwapList(destIntInst->GetProxy());
				if(!isDestinationInDoNotUseList && naVerifyf(!isDestinationInIPLSwapList, "Should have an IPL swap list interior loaded"))
				{
					portalInfo = (*interiorInfoPtr)->CreatePortalInfo(intProxy, roomIdx, portalIdx, destUniqueProxyHashkey, destRoomIdx);
					naAssertf(portalInfo, "Unable to create a portal info - Need to investigate");
				}
			}
		}
	}

	return portalInfo;
}

void naOcclusionManager::BuildOcclusionPathTreeForCombination(const CInteriorInst* startIntInst, const s32 startRoomIdx, 
															const CInteriorInst* finishIntInst, const s32 finishRoomIdx, 
															const u32 pathDepth, const bool buildInterInteriorPaths)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	const u32 keyHash = GenerateOcclusionPathKey(startIntInst, startRoomIdx, finishIntInst, finishRoomIdx, pathDepth);

	bool isDuplicateKey = false;
	for(s32 key = 0; key < m_OcclusionPathKeyTracker.GetCount(); key++)
	{
		if(m_OcclusionPathKeyTracker[key] == keyHash)
		{
			// We should only have a duplicate key if we're trying to get int->outside or outside->int because
			// we're overriding the room 0 (INTLOC_ROOMINDEX_LIMBO) hashes.  We also don't care if we hit a collision going between
			// 2 instances of the same interiorproxy, because we'll build paths a->b, and then build the same paths from b->a.
			naAssertf(startRoomIdx == INTLOC_ROOMINDEX_LIMBO || finishRoomIdx == INTLOC_ROOMINDEX_LIMBO || startIntInst->GetModelNameHash() == finishIntInst->GetModelNameHash(), 
				"Got a duplicate key for %s %s to %s %s",
				startIntInst->GetMloModelInfo()->GetModelName(), startIntInst->GetRoomName(startRoomIdx), 
				finishIntInst->GetMloModelInfo()->GetModelName(), finishIntInst->GetRoomName(finishRoomIdx));
			isDuplicateKey = true;
		}
	}

	if(!isDuplicateKey)
	{
		// Store this key
		m_OcclusionPathKeyTracker.PushAndGrow(keyHash);

		naOcclusionInteriorInfo** interiorInfoPtr = m_LoadedInteriorInfos.Access(startIntInst->GetProxy());
		naAssertf(interiorInfoPtr, "Don't have a naOcclusionInteriorInfo object setup prior to building paths");

		// If this is a path greater than our sm_MaxOcclusionFullPathDepth then only build it if we don't already have a combination at full
		// Unless we manually specify that we want to still build extended paths which is done per interior in the InteriorSettings GO
		bool shouldBuildPath = true;
		if(pathDepth > sm_MaxOcclusionFullPathDepth)
		{
			const u32 keyHashAtFullDepth = GenerateOcclusionPathKey(startIntInst, startRoomIdx, finishIntInst, finishRoomIdx, sm_MaxOcclusionFullPathDepth);
			const naOcclusionPathNode* fullPathTree = (*interiorInfoPtr)->GetPathNodeFromKey(keyHashAtFullDepth);
			if(fullPathTree)
			{
				bool forceBuildExtendedPath = false;
				InteriorSettings* intSettings = audNorthAudioEngine::GetObject<InteriorSettings>(startIntInst->GetMloModelInfo()->GetHashKey());
				if(intSettings && AUD_GET_TRISTATE_VALUE(intSettings->Flags, FLAG_ID_INTERIORSETTINGS_BUILDALLEXTENDEDPATHS) == AUD_TRISTATE_TRUE)
				{
					forceBuildExtendedPath = true;
				}

				if(!forceBuildExtendedPath)
				{
					shouldBuildPath = false;
				}
			}
		}

		if(shouldBuildPath)
		{
			// We may already have this key, because we built it when building the inter/intra paths,
			// in which case build it again as we need to pick up any addition inter-interior paths.
			// So for path combo IntA->Outside, we want to pick up any paths that go IntA->IntB->Outside that we didn't pick up in the intra path build.
			naOcclusionPathNode* pathTree = (*interiorInfoPtr)->GetPathNodeFromKey(keyHash);

			// Because we build certain paths twice due to IPL swaps, we need to combine both variations (IntA->IntBver1->IntC && IntA->IntBver2->IntC)
			naOcclusionPathNode pathTreeBackup;

			if(pathTree)
			{
				// Copy this tree into our temporary backup
				for(s32 i = 0; i < pathTree->GetNumChildren(); i++)
				{
					naOcclusionPathNodeChild* pathChild = pathTree->GetChild(i);
					if(naVerifyf(pathChild, "NULL naOcclusionPathNodeChild*")
						&& naVerifyf(pathChild->portalInfo, "NULL naOcclusionPortalInfo* on path child"))
					{
						pathTreeBackup.CreateChild(pathChild->pathNode, pathChild->portalInfo);
					}
				}
				
				// Now clear it so we can build again
				pathTree->ResetChildArray();
			}
			else
			{
				pathTree = (*interiorInfoPtr)->CreatePathNodeFromKey(keyHash);
			}

			// Don't go inside->outside->inside when looking for paths within the same interior or if building longer paths
			bool allowOutside = true;
			if(g_DisableNonLinkedInterInteriorPaths || pathDepth > sm_MaxOcclusionFullPathDepth || startIntInst == finishIntInst)
			{
				allowOutside = false;
			}

			// Build the paths between the 2 interiors.  This will return true if we eventually found the finish room.
			bool foundDestination = pathTree->BuildPathTree(startIntInst, startRoomIdx, finishIntInst, finishRoomIdx, pathDepth, allowOutside, buildInterInteriorPaths);

			// If we didn't have a previous path that reached the destination, and the path we just built also didn't reach the destination, then delete it
			if(!foundDestination && pathTreeBackup.GetNumChildren() == 0)
			{
				// Couldn't find a valid path within our constraints, so remove it from the map
				// We don't need to worry about fixing up parent ptrs in the maps since this key will be at the end of the map
				(*interiorInfoPtr)->DeletePathTreeWithKey(keyHash);
				naOcclusionPathNode::GetPool()->Delete(pathTree);
			}
			// Otherwise, combine the backed up path with the newly built one so we have all variations
			else
			{
				for(s32 i = 0; i < pathTreeBackup.GetNumChildren(); i++)
				{
					naOcclusionPathNodeChild* backedUpPathTreeChild = pathTreeBackup.GetChild(i);
					if(naVerifyf(backedUpPathTreeChild, "NULL naOcclusionPatNodeChild*"))
					{
						const u32 backedUpBuiltKey = GetKeyForPathNode(backedUpPathTreeChild->pathNode);
						const s32 backedUpPortalIdx = backedUpPathTreeChild->portalInfo->GetPortalIdx();
						const s32 backedUpRoomIdx = backedUpPathTreeChild->portalInfo->GetRoomIdx();
						const u32 backedUpIntHash = backedUpPathTreeChild->portalInfo->GetUniqueInteriorProxyHashkey();

						bool addBackedUpChild = true;
						for(s32 j = 0; j < pathTree->GetNumChildren(); j++)
						{
							naOcclusionPathNodeChild* newlyBuiltPathTreeChild = pathTree->GetChild(j);
							if(naVerifyf(newlyBuiltPathTreeChild, "NULL naOcclusionPathNodeChild*"))
							{
								const u32 newlyBuiltKey = GetKeyForPathNode(newlyBuiltPathTreeChild->pathNode);
								const s32 newlyBuiltPortalIdx = newlyBuiltPathTreeChild->portalInfo->GetPortalIdx();
								const s32 newlyBuiltRoomIdx = newlyBuiltPathTreeChild->portalInfo->GetRoomIdx();
								const u32 newlyBuiltIntHash = newlyBuiltPathTreeChild->portalInfo->GetUniqueInteriorProxyHashkey();

								if(newlyBuiltKey == backedUpBuiltKey
									&& newlyBuiltPortalIdx == backedUpPortalIdx
									&& newlyBuiltRoomIdx == backedUpRoomIdx
									&& newlyBuiltIntHash == backedUpIntHash)
								{
									addBackedUpChild = false;
								}
							}
						}

						if(addBackedUpChild)
						{
							pathTree->CreateChild(backedUpPathTreeChild->pathNode, backedUpPathTreeChild->portalInfo);
						}
					}
				}
			}
		}
	}
}

void naOcclusionManager::WriteInteriorToBuiltPathsFile(const CInteriorProxy* intProxy, const naOcclusionBuildStep buildStep)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	if(intProxy)
	{
		// Now see if we've already built this interior
		FileHandle fileHandle;
		switch(buildStep)
		{
		case AUD_OCC_BUILD_STEP_INTRA_LOAD:
		case AUD_OCC_BUILD_STEP_INTRA_PORTALS:
		case AUD_OCC_BUILD_STEP_INTRA_PATHS:
			{
				fileHandle = CFileMgr::OpenFileForAppending(g_OcclusionBuiltIntraPathsFile);
				break;
			}
		case AUD_OCC_BUILD_STEP_INTER_PATHS:
		case AUD_OCC_BUILD_STEP_INTER_UNLOAD:
			{
				fileHandle = CFileMgr::OpenFileForAppending(g_OcclusionBuiltInterPathsFile);
				break;
			}
		case AUD_OCC_BUILD_STEP_TUNNEL_PATHS:
		case AUD_OCC_BUILD_STEP_TUNNEL_UNLOAD:
			{
				fileHandle = CFileMgr::OpenFileForAppending(g_OcclusionBuiltTunnelPathsFile);
				break;
			}
		default:
			{
				naAssertf(0, "Shouldn't be here");
				fileHandle = NULL;
			}
		}

		if(CFileMgr::IsValidFileHandle(fileHandle))	
		{
			char buffer[255];

			formatf(buffer, sizeof(buffer)-1, "%u\n", GetUniqueProxyHashkey(intProxy));
			CFileMgr::Write(fileHandle, buffer, istrlen(buffer));

			CFileMgr::CloseFile(fileHandle);
		}
	}
}

bool naOcclusionManager::ArePortalEntitiesLoaded(const CInteriorProxy* intProxy)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	bool areEntitiesLoaded = true;
	if(naVerifyf(intProxy, "Passed NULL CInteriorProxy*"))
	{
		const CInteriorInst* intInst = intProxy->GetInteriorInst();
		if(intInst && intInst->IsPopulated() && intInst->GetNumPortals() != 0)
		{
			const s32 globalPortalIdx = intInst->GetPortalIdxInRoom(m_CurrentRoomIdx, m_CurrentPortalIdx);

			if(globalPortalIdx >= 0)
			{
				const u32 numPortalObjects = intInst->GetNumObjInPortal(globalPortalIdx);
				// Need to go through all of them since we're not guaranteed that if the 1st slot is NULL the 2nd is NULL as well
				for(u32 i = 0; i < numPortalObjects; i++)
				{
					const CEntity* entity = intInst->GetPortalObjInstancePtr(m_CurrentRoomIdx, m_CurrentPortalIdx, i);
					if(entity && entity->GetIsTypeObject())
					{
						const CObject* object = static_cast<const CObject*>(entity);
						if(object && !object->GetCurrentPhysicsInst())
						{
							areEntitiesLoaded = false;
						}
					}
				}
			}			
		}
	}
	return areEntitiesLoaded;
}

char* naOcclusionManager::LoadNextOcclusionPathFileLine(FileHandle& fileHandle)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	char* line = CFileLoader::LoadLine(fileHandle);

	//Skip non-data lines
	if(line && (*line == '#' || *line == '\0'))
	{
		line = LoadNextOcclusionPathFileLine(fileHandle);
	}

	return line;
}

void naOcclusionManager::PrintMemoryInfo()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");

	naDisplayf("interiorInfo pool - Slots: %u/%u  Memory: %u/%u", 
		(u32)naOcclusionInteriorInfo::GetPool()->GetNoOfUsedSpaces(), 
		(u32)naOcclusionInteriorInfo::GetPool()->GetSize(),
		(u32)(naOcclusionInteriorInfo::GetPool()->GetStorageSize() * naOcclusionInteriorInfo::GetPool()->GetNoOfUsedSpaces()),
		(u32)(naOcclusionInteriorInfo::GetPool()->GetStorageSize() * naOcclusionInteriorInfo::GetPool()->GetSize()));
	naDisplayf("pathnode pool - Slots: %u/%u  Memory: %u/%u", 
		(u32)naOcclusionPathNode::GetPool()->GetNoOfUsedSpaces(), 
		(u32)naOcclusionPathNode::GetPool()->GetSize(),
		(u32)(naOcclusionPathNode::GetPool()->GetStorageSize() * naOcclusionPathNode::GetPool()->GetNoOfUsedSpaces()),
		(u32)(naOcclusionPathNode::GetPool()->GetStorageSize() * naOcclusionPathNode::GetPool()->GetSize()));
	naDisplayf("portalInfo pool - Slots: %u/%u  Memory: %u/%u", 
		(u32)naOcclusionPortalInfo::GetPool()->GetNoOfUsedSpaces(), 
		(u32)naOcclusionPortalInfo::GetPool()->GetSize(),
		(u32)(naOcclusionPortalInfo::GetPool()->GetStorageSize() * naOcclusionPortalInfo::GetPool()->GetNoOfUsedSpaces()),
		(u32)(naOcclusionPortalInfo::GetPool()->GetStorageSize() * naOcclusionPortalInfo::GetPool()->GetSize()));
	naDisplayf("portalEntity pool - Slots: %u/%u  Memory: %u/%u", 
		(u32)naOcclusionPortalEntity::GetPool()->GetNoOfUsedSpaces(), 
		(u32)naOcclusionPortalEntity::GetPool()->GetSize(),
		(u32)(naOcclusionPortalEntity::GetPool()->GetStorageSize() * naOcclusionPortalEntity::GetPool()->GetNoOfUsedSpaces()),
		(u32)(naOcclusionPortalEntity::GetPool()->GetStorageSize() * naOcclusionPortalEntity::GetPool()->GetSize()));
}

u32 naOcclusionManager::GetKeyForPathNode(naOcclusionPathNode* pathNode)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	atMap<const CInteriorProxy*, naOcclusionInteriorInfo*>::Iterator entry = m_LoadedInteriorInfos.CreateIterator();
	for(entry.Start(); !entry.AtEnd(); entry.Next())
	{
		naOcclusionInteriorInfo** interiorInfoPtr = m_LoadedInteriorInfos.Access(entry.GetKey());
		if(interiorInfoPtr && *interiorInfoPtr)
		{
			u32 key = (*interiorInfoPtr)->GetKeyFromPathNode(pathNode);
			if(key)
			{
				return key;
			}
		}
	}
	return 0;
}

naOcclusionPathNode* naOcclusionManager::GetPathNodeFromKey(const u32 key)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	atMap<const CInteriorProxy*, naOcclusionInteriorInfo*>::Iterator entry = m_LoadedInteriorInfos.CreateIterator();
	for(entry.Start(); !entry.AtEnd(); entry.Next())
	{
		naOcclusionInteriorInfo** interiorInfoPtr = m_LoadedInteriorInfos.Access(entry.GetKey());
		if(interiorInfoPtr && *interiorInfoPtr)
		{
			naOcclusionPathNode* pathNode = (*interiorInfoPtr)->GetPathNodeFromKey(key);
			if(pathNode)
			{
				return pathNode;
			}
		}
	}

	return NULL;
}

#endif // __BANK
