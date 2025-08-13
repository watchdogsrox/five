/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/streamer/SceneStreamerMgr.cpp
// PURPOSE : Primary interface to scene streamers
// AUTHOR :  Ian Kiigan
// CREATED : 06/08/10
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/streamer/SceneStreamerMgr.h"

#include <algorithm>

#include "atl/map.h"
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "control/replay/replay.h"
#include "debug/DebugScene.h"
#include "grcore/debugdraw.h"
#include "scene/streamer/SceneStreamer.h"
#include "scene/streamer/SceneScoring.h"
#include "scene/streamer/StreamVolume.h"
#include "fwdebug/vectormap.h"
#include "fwscene/search/SearchEntities.h"
#include "fwscene/scan/ScanDebug.h"
#include "fwsys/timer.h"
#include "modelinfo/BaseModelInfo.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelinfo/TimeModelInfo.h"
#include "peds/Ped.h"
#include "profile/element.h"
#include "scene/ContinuityMgr.h"
#include "scene/Entity.h"
#include "scene/LoadScene.h"
#include "scene/lod/LodScale.h"
#include "scene/portals/InteriorInst.h"
#include "scene/portals/InteriorProxy.h"
#include "scene/portals/PortalInst.h"
#include "scene/streamer/SceneStreamerList.h"
#include "scene/world/GameWorld.h"
#include "scene/world/GameScan.h"
#include "script/script.h"
#include "streaming/streaming.h"
#include "streaming/streamingdebug.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingrequestlist.h"
#include "streaming/streamingvisualize.h"
#include "system/memops.h"
#include "system/param.h"
#include "vehicles/vehicle.h"
#include "vector/color32.h"
#include "vector/colors.h"

#if !__NO_OUTPUT
#include "Network/NetworkInterface.h"
#endif

SCENE_OPTIMISATIONS();

#define SHRINK_LOD_STREAMER_AT_SPEED (0)

PARAM(nonewscenestreamer, "[scene] Disable new scene streamer that has per-device quotas");
PARAM(streamingreadahead, "[scene] Factor by which the focal point is moved forward");
PARAM(streamingdrivemode, "[scene] Prefer streaming LODs when moving fast");
PARAM(interiordrivemode, "[scene] Throttle interior streaming when moving fast");

CSceneStreamerMgr g_SceneStreamerMgr;
CSceneLoader CSceneStreamerMgr::ms_sceneLoader;
bool CSceneStreamerMgr::ms_bStreamFromPVS = true;
bool CSceneStreamerMgr::ms_bStreamByProximity_HD = true;
bool CSceneStreamerMgr::ms_bStreamMapSwaps = true;
bool CSceneStreamerMgr::ms_bStreamVolumes = true;
bool CSceneStreamerMgr::ms_bStreamRequiredInteriors = true;

float CSceneStreamerMgr::ms_fStreamRadius_HD = 80.0f;
float CSceneStreamerMgr::ms_fStreamShortRadius_HD = 20.0f;
float CSceneStreamerMgr::ms_fStreamShortRadius_Interior = 12.0f;
float CSceneStreamerMgr::ms_fStreamRadius_Interior = 20.0f;
float CSceneStreamerMgr::ms_fStreamRadius_LOD = 4000.0f;

float CSceneStreamerMgr::ms_fCachedNeededScore = 0.0f;

Vec3V CSceneStreamerMgr::ms_vSphericalStreamingPos;
bool CSceneStreamerMgr::ms_bScriptForceSphericalStreamerTrackWithCamera = false;

u32 CSceneStreamerMgr::ms_ReplayTimeoutMS = 10000;

#if __STATS
EXT_PF_TIMER(FindLowestScoring);
EXT_PF_TIMER(SortBuckets);
EXT_PF_TIMER(WaitForSPUSort);
EXT_PF_TIMER(IssueRequests);
EXT_PF_VALUE_INT(ActiveEntities);
EXT_PF_VALUE_INT(NeededEntities);
#endif // __STATS


#if !__FINAL
// Temporary function to track down streaming problems
void VerifyHasLoaded(CEntity *entity, CBaseModelInfo *modelInfo)
{
	if (!modelInfo->GetHasLoaded())
	{
		Errorf("Model %s didn't load", modelInfo->GetModelName());
		Errorf("Model missing: %d, is composite: %d, is light %d, is MLO: %d, is portal: %d"
			, (int) modelInfo->IsModelMissing(), (int) entity->GetIsTypeComposite()
			, (int) entity->GetIsTypeLight(), (int) entity->GetIsTypeMLO()
			, (int) entity->GetIsTypePortal());

		fwModelId modelId = entity->GetModelId();
		strLocalIndex objIndex = CModelInfo::LookupLocalIndex(modelId);

		Errorf("Model ID: %d, objIndex: %d", modelId.GetModelIndex(), objIndex.Get());
		if (objIndex == CModelInfo::INVALID_LOCAL_INDEX){		
			Errorf("INVALID INDEX");
			return;
		}

		strIndex index = fwArchetypeManager::GetStreamingModule()->GetStreamingIndex(objIndex);
		Errorf("strIndex: %d", index.Get());

		strStreamingModule *module = strStreamingEngine::GetInfo().GetModule(index);

		Errorf("Module: %s", module->GetModuleName());

		if (module != CModelInfo::GetStreamingModule())
		{
			Errorf("Not using the archetype module");
		}

		switch (modelInfo->GetDrawableType()) {
			case fwArchetype::DT_DRAWABLE:
				Errorf("Drawable");
				break;

			case fwArchetype::DT_DRAWABLEDICTIONARY:
				Errorf("Drawable dictionary");
				break;

			case fwArchetype::DT_FRAGMENT:
				Errorf("Fragment");
				break;

			case fwArchetype::DT_UNINITIALIZED:
				Errorf("Uninitialized");
				break;

			case fwArchetype::DT_ASSETLESS:
				Errorf("Assetless");
				break;

			default:
				Errorf("Bad type: %d", modelInfo->GetDrawableType());
		}
	}
}
#endif // !__FINAL


void CSceneStreamerMgr::Init()
{
	m_StreamingLists.Init();
	m_BucketCutoff = STREAMBUCKET_VEHICLESANDPEDS;

	if (PARAM_nonewscenestreamer.Get())
	{
		m_streamer.sm_NewSceneStreamer = false;
	}

	float readAheadFactor = 0.5f;
	if (PARAM_streamingreadahead.Get(readAheadFactor))
	{
		*(g_SceneStreamerMgr.GetStreamingLists().GetReadAheadFactorPtr()) = readAheadFactor;
	}

	*(g_SceneStreamerMgr.GetStreamingLists().GetDriveModePtr()) |= PARAM_streamingdrivemode.Get();
	*(g_SceneStreamerMgr.GetStreamingLists().GetInteriorDriveModePtr()) |= PARAM_interiordrivemode.Get();
}

void CSceneStreamerMgr::Shutdown()
{
	m_StreamingLists.Shutdown();
}

void CSceneStreamerMgr::PreScanUpdate()
{
	PF_AUTO_PUSH_TIMEBAR("CSceneStreamerMgr::PreScanUpdate");

	CacheNeededScore();

	// We now partially process the streaming lists while we 
	// process the PVS entries. Make sure the streaming lists 
	// are updated and ready to go.
	m_StreamingLists.UpdateInteriorAndCamera();

	// Make sure we didn't collect too many entities.
	PF_PUSH_TIMEBAR_DETAIL("RemoveExcessEntities");
	CStreaming::GetCleanup().RemoveExcessEntities();
	PF_POP_TIMEBAR_DETAIL();

	PF_PUSH_TIMEBAR_DETAIL("PreGenerateLists");
	m_StreamingLists.PreGenerateLists();
	PF_POP_TIMEBAR_DETAIL();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CacheNeededScore
// PURPOSE:		useful for detecting when scene streaming is under pressure
//////////////////////////////////////////////////////////////////////////
void CSceneStreamerMgr::CacheNeededScore()
{
	ms_fCachedNeededScore = 0.0f;
	CStreamingBucketSet &neededBucketSet = m_StreamingLists.GetNeededEntityList().GetBucketSet();
	BucketEntry::BucketEntryArray &neededList = neededBucketSet.GetBucketEntries();
	neededBucketSet.WaitForSorting();
	int neededListCount = neededList.GetCount();
	if (neededListCount > 0)
	{
		ms_fCachedNeededScore = neededList[neededListCount-1].m_Score;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Process
// PURPOSE:		primary update point for scene streaming - builds up lists, issues requests
//////////////////////////////////////////////////////////////////////////
void CSceneStreamerMgr::Process()
{
#if GTA_REPLAY && REPLAY_FORCE_LOAD_SCENE_DURING_EXPORT
	if (CReplayMgr::IsExporting())
	{
		BlockAndLoad();
	}
#endif

	GenerateLists();

#if __BANK
	CStreamingDebug::StorePvsIfNeeded();
#endif // __BANK

	PF_PUSH_TIMEBAR("IssueRequestsForLists");
	IssueRequestsForLists();
	PF_POP_TIMEBAR();
	CheckForMapSwapCompletion();					// can we do this elsewhere? not really a scene streaming operation
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GenerateLists
// PURPOSE:		produces entity lists for all that require scene streaming
//////////////////////////////////////////////////////////////////////////
void CSceneStreamerMgr::GenerateLists()
{
	PF_PUSH_TIMEBAR_DETAIL("GenerateLists");

	m_lists.Reset();

#if STREAMING_VISUALIZE
	if((!CStreaming::IsStreamingPaused()) && !strStreamingEngine::IsStreamingDisabled())
	{
		const grcViewport &vp = g_SceneToGBufferPhaseNew->GetGrcViewport();
		STRVIS_SET_CAMERA(vp.GetCameraPosition(), vp.GetCameraMtx().GetCol2(), vp.GetCameraMtx().GetCol1(),
			vp.GetFarClip(), vp.GetTanHFOV(), vp.GetTanVFOV());

		Vec3V streamCamPos = GetStreamingLists().GetCameraPositionForFade();
		Vec3V streamCamDir = GetStreamingLists().GetCameraDirection();
		STRVIS_SET_STREAM_CAMERA(streamCamPos, streamCamDir);

		Vec3V svPos(V_ZERO), svDir(V_ZERO);
		float radius = 0.0f;
		u32 flags = 0;

		for (int slot=0; slot<CStreamVolumeMgr::VOLUME_SLOT_TOTAL; slot++)
		{
			strStreamingVisualize::StreamingVolumeType type = strStreamingVisualize::SVT_NONE;

			if (CStreamVolumeMgr::IsVolumeActive( slot ))
			{
				CStreamVolume& volume = CStreamVolumeMgr::GetVolume( slot );
				if (volume.IsTypeLine())
				{
					type = strStreamingVisualize::SVT_LINE;
					svPos = volume.GetPos();
					svDir = volume.GetDir();
				}
				else if (volume.IsTypeSphere())
				{
					type = strStreamingVisualize::SVT_SPHERE;
					svPos = volume.GetPos();
					radius = volume.GetSphere().GetRadiusf();
				}
				else if (volume.IsTypeFrustum())
				{
					type = strStreamingVisualize::SVT_LINE;
					svPos = volume.GetPos();
					svDir = volume.GetDir();
					radius = volume.GetFarClip();
				}

				flags = volume.GetAssetFlags();
			}

			STRVIS_SET_STREAMING_VOLUME(slot, type, svPos, svDir, radius, flags);
		}

		// Also add all the box searches.
		const atArray<fwBoxStreamerSearch>& searches = g_MapDataStore.GetBoxStreamer().GetSearches();

		for (int x=0; x<searches.GetCount(); x++)
		{
			const fwBoxStreamerSearch &search = searches[x];
			if (( search.GetAssetFlags() & fwBoxStreamerAsset::MASK_MAPDATA) != 0)
			{
				STRVIS_ADD_BOX_SEARCH(search.GetBox(), search.GetType());
			}
		}
	}
#endif // STREAMING_VISUALIZE

#if __BANK
	bool mayBlock = true;

	// If we're driving at at least a moderate speed, do not permit blocking any longer - it means
	// that blocking is the result of the bounds loading in too slowly, and we need to analyze that.
	CPed *ped = CGameWorld::FindLocalPlayer();
	if (ped)
	{
		CVehicle *vehicle = ped->GetVehiclePedInside();

		if (vehicle)
		{
			float speed = vehicle->GetVelocity().Mag2();
			mayBlock = speed < 150.0f;
		}
	}
	fwBoxStreamer::SetBlockingPermitted(mayBlock || !camInterface::IsFadedIn());
#endif // __BANK

	if (ms_bStreamFromPVS)
	{
		m_StreamingLists.CreateDrawablesForEntities();
	}

	if (ms_bStreamMapSwaps && INSTANCE_STORE.GetGroupSwapper().GetIsSwapActive())
	{
		PF_START_TIMEBAR_DETAIL("List From Map Group Swap");
		m_StreamingLists.AddFromMapGroupSwap();
	}

	if (ms_bStreamVolumes && CStreamVolumeMgr::IsAnyVolumeActive())
	{
		PF_START_TIMEBAR_DETAIL("List From Streaming Volume");
		m_StreamingLists.AddFromStreamingVolume();
	}


	if (ms_bStreamRequiredInteriors)
	{
		PF_START_TIMEBAR_DETAIL("List From player interior");
		m_StreamingLists.AddFromRequiredInteriors();
	}

	m_StreamingLists.PostGenerateLists();

	PF_POP_TIMEBAR_DETAIL();

#if STREAMING_VISUALIZE
	if (strStreamingVisualize::IsInstantiated())
	{
		UpdateStreamingVisualizeVisibility();
	}
#endif // STREAMING_VISUALIZE


#if __BANK && __DEV
	if (CDebugScene::bDisplayLineAboveAllEntities)
	{
		// Set lineOffset's up axis to fEntityDebugLineLength.
		Vec3V lineOffset = And(Vec3VFromF32(CDebugScene::fEntityDebugLineLength), Vec3V(V_MASK_UP_AXIS));

		CPostScan::WaitForSortPVSDependency();
		CGtaPvsEntry* pPvsStart = gPostScan.GetPVSBase();
		CGtaPvsEntry* pPvsStop = gPostScan.GetPVSEnd();

		while(pPvsStart != pPvsStop)
		{
			CEntity* pEntity = pPvsStart->GetEntity();
			if (pEntity)
			{
				// Green for static.
				Color32 color(Color_green);

				if (pEntity->GetIsDynamic())
				{
					// Red for inactive, magenta for active.
					// Note that "GetIsStatic() && GetIsDynamic()" means it's an inactive
					// dynamic entity.
					color = (pEntity->GetIsStatic()) ? Color_red : Color_magenta;
				}

				Vec3V p1 = pEntity->GetTransform().GetPosition();
				Vec3V p2 = p1;

				// Replace the up axis with the top of the bounding box, and add our own offset.
				// Transform the bounding box into world space first.
				spdAABB localbbox;
				p2.SetZ(pEntity->GetBoundBox(localbbox).GetMax().GetZ());
				p2 += lineOffset;
				grcDebugDraw::Line(p1, p2, color);
			}

			pPvsStart++;
		}
	}
#endif // __BANK && __DEV
}

#if STREAMING_VISUALIZE
static atSimplePooledMapType<CEntity *, u64> s_VisFlags[2];
static int s_CurrentVisFlagMap;

void CSceneStreamerMgr::UpdateStreamingVisualizeVisibility()
{
	if (!s_VisFlags[0].m_Map.GetNumSlots())
	{
		USE_DEBUG_MEMORY();

		for (int x=0; x<2; x++)
		{
			s_VisFlags[x].Init(CSceneStreamerList::MAX_OBJ_INSTANCES);
		}
	}

	CPostScan::WaitForSortPVSDependency();
	CGtaPvsEntry* pPvsStart = gPostScan.GetPVSBase();
	CGtaPvsEntry* pPvsStop = gPostScan.GetPVSEnd();

	atFixedArray<strStreamingVisualize::VisibilityEntry, CSceneStreamerList::MAX_OBJ_INSTANCES> changedEntities;

	int prevMap = s_CurrentVisFlagMap;
	int newMap = 1 - prevMap;

	while(pPvsStart != pPvsStop)
	{
		CEntity* pEntity = pPvsStart->GetEntity();
		if (pEntity)
		{
			u64 visFlags = ((u64) pPvsStart->GetVisFlags().GetPhaseVisFlags()) << 32;
			visFlags |= (u64) pPvsStart->GetVisFlags().GetOptionFlags();

			u64 *prevFlags = s_VisFlags[prevMap].Access(pEntity);

			if (!prevFlags || *prevFlags != visFlags)
			{
				strStreamingVisualize::VisibilityEntry &entry = changedEntities.Append();
				entry.m_Entity = pEntity;
				entry.m_PhaseFlags = pPvsStart->GetVisFlags().GetPhaseVisFlags();
				entry.m_OptionFlags = pPvsStart->GetVisFlags().GetOptionFlags();
			}

			if (prevFlags)
			{
				s_VisFlags[prevMap].Delete(pEntity);
			}

			s_VisFlags[newMap].m_Map[pEntity] = visFlags;
		}

		pPvsStart++;
	}

	// Everything left in the prevMap is no longer visible.
	atSimplePooledMapType<CEntity *, u64>::Iterator it = s_VisFlags[prevMap].CreateIterator();
	for (it.Start(); !it.AtEnd(); it.Next())
	{
		strStreamingVisualize::VisibilityEntry &entry = changedEntities.Append();
		entry.m_Entity = it.GetKey();
		entry.m_PhaseFlags = 0;
		entry.m_OptionFlags = 0;
	}

	s_VisFlags[prevMap].Reset();
	s_CurrentVisFlagMap = newMap;

	// Send the list of changes to SV.
	if (changedEntities.GetCount())
	{
		STRVIS_UPDATE_VISFLAGS(changedEntities.GetElements(), changedEntities.GetCount());
	}	
}
#endif // STREAMING_VISUALIZE


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IssueRequestsForLists
// PURPOSE:		for each entity list, issue streaming requests
//////////////////////////////////////////////////////////////////////////
void CSceneStreamerMgr::IssueRequestsForLists()
{
	PF_FUNC(IssueRequests);

	strStreamingEngine::SetIsLoadingPriorityObjects(false);
	m_streamer.Reset();

	CStreamingBucketSet &neededBucketSet = GetStreamingLists().GetNeededEntityList().GetBucketSet();
	BucketEntry::BucketEntryArray &neededList = neededBucketSet.GetBucketEntries();
	neededBucketSet.WaitForSorting();

	int neededListCount = neededList.GetCount();

#if STREAMING_VISUALIZE
	// Send the needed list to SV. BucketEntry is game-level, we need to convert it to a RAGE-level format.
	if (neededList.GetCount() && strStreamingVisualize::IsInstantiated())
	{
		strStreamingVisualize::NeededListEntry *neededListArray = Alloca(strStreamingVisualize::NeededListEntry, neededListCount);
		for (int x=0; x<neededListCount; x++)
		{
			neededListArray[x].m_Entity = neededList[x].GetEntity();
			neededListArray[x].m_Score = neededList[x].m_Score;
		}

		STRVIS_ADD_TO_NEEDED_LIST(neededListArray, neededListCount);
	}
#endif // STREAMING_VISUALIZE

	m_streamer.IssueRequests(neededList, m_BucketCutoff);
	STRVIS_SET_MASTER_CUTOFF(GetStreamingLists().GetMasterCutoff());

#if 1
	STRVIS_SET_CONTEXT(strStreamingVisualize::INSTANTFULFILL);

	for (u32 j=0; j<neededListCount; j++)
	{
		CEntity* pEntity = neededList[j].GetEntity();
		if (pEntity && !pEntity->GetDrawHandlerPtr())
		{
			CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
			strLocalIndex objIndex = strLocalIndex(pEntity->GetModelIndex());
			if (objIndex == CModelInfo::INVALID_LOCAL_INDEX){		
				continue;				// no streaming index allocated to this model info, so skip 
			}

			if (pModelInfo && !pModelInfo->GetHasLoaded() && AllDepsSatisfied(objIndex))
			{
				if (CStreaming::IsSceneStreamingDisabled() || CModelInfo::RequestAssets(pEntity->GetBaseModelInfo(), 0))
				{
					strIndex index = fwArchetypeManager::GetStreamingModule()->GetStreamingIndex(objIndex);
					strStreamingEngine::GetInfo().ForceLoadDummyRequest(index);
#if !__FINAL
					VerifyHasLoaded(pEntity, pModelInfo);
#endif // !__FINAL
					Assertf(pModelInfo->GetHasLoaded() || pModelInfo->IsModelMissing(), "Failed to load the dummy %s", strStreamingEngine::GetObjectName(index));
					if (pModelInfo->GetHasLoaded())
					{
						pEntity->CreateDrawable();
					}
				}
			}
		}
	}

	STRVIS_SET_CONTEXT(strStreamingVisualize::NONE);
#endif
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CheckForMapSwapCompletion
// PURPOSE:		check if an imap group swap has its streaming requests fully satisfied
//////////////////////////////////////////////////////////////////////////
void CSceneStreamerMgr::CheckForMapSwapCompletion()
{
	fwImapGroupMgr& imapGroupMgr = INSTANCE_STORE.GetGroupSwapper();
	for (u32 i=0; i<fwImapGroupMgr::MAX_CONCURRENT_SWAPS; i++)
	{
		if (imapGroupMgr.GetIsActive(i) && imapGroupMgr.GetIsSwapImapLoaded(i))
		{
			bool bSwapComplete = true;
			atArray<fwEntity*> entities;
			imapGroupMgr.GetSwapEntities(i, entities);
			for (s32 j=0; j<entities.GetCount(); j++)
			{
				CEntity* pEntity = (CEntity*) entities[j];
				if (pEntity && !pEntity->GetIsTypeComposite())
				{
					CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
					Assertf( pModelInfo!=NULL, "An entity with no modelinfo has been passed to scene streamer: Model %s, Type:%d",
						pEntity->GetModelName(), pEntity->GetType() );
					if (pModelInfo && !pModelInfo->GetHasLoaded())
					{
						bSwapComplete = false;
					}
				}
			}
			imapGroupMgr.SetIsLoaded(i, bSwapComplete);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AllDepsSatisfied
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
bool CSceneStreamerMgr::AllDepsSatisfied(strLocalIndex localIndex)
{
	strIndex deps[STREAMING_MAX_DEPENDENCIES];
	s32 numDeps = CModelInfo::GetStreamingModule()->GetDependencies(localIndex, &deps[0], STREAMING_MAX_DEPENDENCIES);
	for(s32 i=0; i<numDeps; i++)
	{
		strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(deps[i]);
		if (info.GetStatus() != STRINFO_LOADED)
		{
			return false;
		}
	}
	return true;
}

float CSceneStreamerMgr::GetIntStreamSphereRadius()
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	Assert(pPlayerPed);
	if (pPlayerPed)
	{
		fwInteriorLocation interiorLocation = pPlayerPed->GetInteriorLocation();
		if (interiorLocation.IsValid())
		{
			return ms_fStreamRadius_Interior;
		}
	}

	return ms_fStreamShortRadius_Interior;
}

float CSceneStreamerMgr::GetHdStreamSphereRadius()
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	Assert(pPlayerPed);

	// if indoors and not close to an exterior portal, reduce range of high detail stream sphere
	fwInteriorLocation interiorLocation = pPlayerPed->GetInteriorLocation();
	bool bExteriorPortalNearby = false;
	Vec3V vPlayerPos = pPlayerPed->GetTransform().GetPosition();
	const ScalarV vPortalSearchRadiusSqd(10.0f * 10.0f);

	if (interiorLocation.IsValid())
	{
		CInteriorProxy* pProxy = CInteriorProxy::GetFromLocation(interiorLocation);
		if (pProxy)
		{
			CInteriorInst* pInst = pProxy->GetInteriorInst();
			if (pInst)
			{
				const atArray<CPortalInst*>& portalInsts = pInst->GetRoomZeroPortalInsts();

				for (s32 i=0; i<portalInsts.GetCount(); i++)
				{
					CPortalInst* pPortalInst = portalInsts[i];
					if (pPortalInst)
					{
						const Vec3V vPortalPos = pPortalInst->GetTransform().GetPosition();
						const ScalarV distSqd = DistSquared( vPortalPos, vPlayerPos );
						if (IsLessThanOrEqualAll(distSqd, vPortalSearchRadiusSqd))
						{
							bExteriorPortalNearby = true;
							break;
						}
					}
				}
			}
		}
	}

	if ( (interiorLocation.IsValid() && !bExteriorPortalNearby) /*|| g_LodScale.GetGlobalScale()>1.0f*/ )
	{
		return ms_fStreamShortRadius_HD;
	}
	return ms_fStreamRadius_HD;
	
}

void CSceneStreamerMgr::UpdateSphericalStreamerPosAndRadius()
{
	ms_vSphericalStreamingPos = VECTOR3_TO_VEC3V( ms_bScriptForceSphericalStreamerTrackWithCamera ? camInterface::GetPos() : CFocusEntityMgr::GetMgr().GetPos() );
	ms_bScriptForceSphericalStreamerTrackWithCamera = false;

	const float fMagdemoFudgeFactor = 1000.0f;		// what's needed here instead of using GetFar, get the distance to the corners of the clip plane

	float fRadiusMax = ( camInterface::GetFar() + fMagdemoFudgeFactor );

	if ( gStreamingRequestList.IsPlaybackActive() && gStreamingRequestList.IsAboutToFinish() )
	{
		// return to play after SRL playback, so start pulling in distant lods
		fRadiusMax = 6000.0f;
	}

#if SHRINK_LOD_STREAMER_AT_SPEED
	const float fRadiusMin = 1.0f;
	const float fNewRadius = g_ContinuityMgr.HighSpeedMovement() ? (ms_fStreamRadius_LOD - 75.0f) : fRadiusMax;

	// clamp final value and return it
	ms_fStreamRadius_LOD = Clamp(fNewRadius, fRadiusMin, fRadiusMax);
#else
	ms_fStreamRadius_LOD = fRadiusMax;
#endif	//#if SHRINK_LOD_STREAMER_AT_SPEED

}

#if GTA_REPLAY
//////////////////////////////////////////////////////////////////////////
// FUNCTION:	BlockAndLoad
// PURPOSE:		block the game until the entire visible scene is in memory. for replay export
//////////////////////////////////////////////////////////////////////////
void CSceneStreamerMgr::BlockAndLoad()
{
	CPostScan::WaitForSortPVSDependency();

	const bool bBurstMode = strStreamingEngine::GetBurstMode();
	strStreamingEngine::SetBurstMode(true);

	CGtaPvsEntry* pPvsStart = gPostScan.GetPVSBase();
	CGtaPvsEntry* pPvsStop = gPostScan.GetPVSEnd();

	while(pPvsStart != pPvsStop)
	{
		CEntity* pEntity = pPvsStart->GetEntity();
		if (pEntity && (pPvsStart->GetVisFlags().GetOptionFlags() & VIS_FLAG_ONLY_STREAM_ENTITY)==0
			&& CStreamingLists::IsRequired(pEntity) && !pEntity->GetIsTypePed() && !pEntity->GetIsTypeVehicle() )
		{
			CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
			if (pModelInfo && !pModelInfo->IsModelMissing())
			{
				CModelInfo::RequestAssets(pModelInfo, STRFLAG_LOADSCENE);
			}
		}
		pPvsStart++;
	}

#if RSG_PC
	CStreaming::LoadAllRequestedObjects();
#else
	CStreaming::LoadAllRequestedObjectsWithTimeout(false, ms_ReplayTimeoutMS);
#endif

	pPvsStart = gPostScan.GetPVSBase();
	while (pPvsStart != pPvsStop)
	{
		CEntity* pEntity = pPvsStart->GetEntity();
		if (pEntity && (pPvsStart->GetVisFlags().GetOptionFlags() & VIS_FLAG_ONLY_STREAM_ENTITY)==0 && pEntity->GetDrawHandlerPtr()==NULL)
		{
			CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
			if (pModelInfo && !pModelInfo->IsModelMissing() && pModelInfo->GetHasLoaded())
			{
				pEntity->CreateDrawable();
			}
		}

		pPvsStart++;
	}

	strStreamingEngine::SetBurstMode(bBurstMode);

	strStreamingEngine::GetInfo().ClearFlagForAll(STRFLAG_LOADSCENE);
}
#endif

#if __BANK
void CSceneStreamerMgr::AddWidgets(bkGroup &group)
{
	GetStreamingLists().AddWidgets(group);

	group.AddToggle("Add master cut-off for stability", &CSceneStreamer::sm_EnableMasterCutoff);
	group.AddToggle("New scene streamer", &CSceneStreamer::sm_NewSceneStreamer);
	group.AddSlider("Bucket cut-off for priority requests", (int *) &m_BucketCutoff, 0, STREAMBUCKET_COUNT - 1, 1);
	group.AddSlider("Read-ahead when moving", GetStreamingLists().GetReadAheadFactorPtr(), 0.0f, 10.0f, 0.001f);
	group.AddButton("Dump Streaming Interests", datCallback(MFA(CSceneStreamerMgr::DumpStreamingInterests), this));
	group.AddToggle("Enable Drive Mode", GetStreamingLists().GetDriveModePtr());
	group.AddToggle("Enable Interior Drive Mode", GetStreamingLists().GetInteriorDriveModePtr());
#if GTA_REPLAY
	group.AddSlider("Replay Timeout (MS)", &ms_ReplayTimeoutMS, 0, 30000, 100);
#endif

	m_streamer.AddWidgets(group);
}
#endif // __BANK


#if !__NO_OUTPUT
void CSceneStreamerMgr::DumpPvsAndBucketStats()
{
	CPostScan::WaitForSortPVSDependency();
	CGtaPvsEntry* pPvsStart = gPostScan.GetPVSBase();
	CGtaPvsEntry* pPvsStop = gPostScan.GetPVSEnd();

	Displayf("Active bucket size: %d", m_StreamingLists.GetActiveEntityList().GetBucketSet().GetEntryCount());
	Displayf("Needed bucket size: %d", m_StreamingLists.GetNeededEntityList().GetBucketSet().GetEntryCount());
	Displayf("Entries in PVS: %" SIZETFMT "u", (size_t)pPvsStop - (size_t)pPvsStart);
}

void CSceneStreamerMgr::DumpStreamingInterests(bool toTTY)
{
	if (g_SceneToGBufferPhaseNew)
	{
		Vec3V camPos = RCC_VEC3V(g_SceneToGBufferPhaseNew->GetCameraPositionForFade());
		Vec3V camDir = RCC_VEC3V(g_SceneToGBufferPhaseNew->GetCameraDirection());

		StreamerDebugPrintf(toTTY, "Primary camera: Position: %.2f %.2f %.2f, Dir: %.2f %.2f %.2f", 
			camPos.GetXf(), camPos.GetYf(), camPos.GetZf(), camDir.GetXf(), camDir.GetYf(), camDir.GetZf());
	}

	// spectator cam
	StreamerDebugPrintf(toTTY, "... MP spectator mode active: %s", (NetworkInterface::IsInSpectatorMode() ? "YES" : "NO" ) );
	
	// streaming focus
	Vector3 vFocusPos = CFocusEntityMgr::GetMgr().GetPos();
	Vector3 vPedPos = CGameWorld::FindFollowPlayerCoors();

	StreamerDebugPrintf(toTTY, "... focus pos is (%.2f, %.2f, %.2f), player pos is (%.2f, %.2f, %.2f)", vFocusPos.x, vFocusPos.y, vFocusPos.z, vPedPos.x, vPedPos.y, vPedPos.z);
	if (CFocusEntityMgr::GetMgr().IsPlayerPed())
	{
		StreamerDebugPrintf(toTTY, "... focus pos is in default state (current player ped)");
	}
	else
	{
		StreamerDebugPrintf(toTTY, "... focus pos has been overridden");
	}

	// new load scene
	StreamerDebugPrintf(toTTY, "... new load scene active: %s", (g_LoadScene.IsActive() ? "YES" : "NO") );
	if (g_LoadScene.IsActive())
	{
#if __BANK
		Vector3 vLoadScenePos = VEC3V_TO_VECTOR3( g_LoadScene.GetPos() );
		StreamerDebugPrintf(toTTY, "... loading scene at position (%.2f, %.2f, %.2f)", vLoadScenePos.x, vLoadScenePos.y, vLoadScenePos.z);
#else // __BANK
		StreamerDebugPrintf(toTTY, "... loading scene is active");
#endif // __BANK

#if __BANK
		if (g_LoadScene.WasStartedByScript())
		{
			StreamerDebugPrintf(toTTY, "... started by script %s", g_LoadScene.GetScriptName().c_str());
			StreamerDebugPrintf(toTTY, "... started by script");
		}
#endif // __BANK
	}

#if !__FINAL
	// Box searches.
	const atArray<fwBoxStreamerSearch>& searches = g_MapDataStore.GetBoxStreamer().GetSearches();

	for (int x=0; x<searches.GetCount(); x++)
	{
		const fwBoxStreamerSearch &search = searches[x];
		if (( search.GetAssetFlags() & fwBoxStreamerAsset::MASK_MAPDATA) != 0)
		{
			StreamerDebugPrintf(toTTY, "Mapdata search %d: Center (%.2f, %.2f, %.2f), Type %s, Flags %d", x,
				search.GetPos().GetXf(), search.GetPos().GetYf(), search.GetPos().GetZf(), search.GetTypeName(), search.GetAssetFlags() );
		}
	}
#endif // !__FINAL

	StreamerDebugPrintf(toTTY, "********************************************");

	for (s32 i=0; i<CStreamVolumeMgr::VOLUME_SLOT_TOTAL; i++)
	{
		CStreamVolume &volume = CStreamVolumeMgr::GetVolume(i);

		if (volume.IsActive())
		{
			if ((volume.GetAssetFlags() & fwBoxStreamerAsset::MASK_MAPDATA) == 0)
			{
				StreamerDebugPrintf(toTTY, "Volume %d is active, but doesn't load map data");
			}
			else
			{
				//////////////////////////////////////////////////////////////////////////
				const char* origin = volume.GetDebugOwner();

				if (volume.IsTypeSphere())
				{
					spdSphere &sphere = volume.GetSphere();
					Vec3V center = sphere.GetCenter();

					StreamerDebugPrintf(toTTY, "Streaming volume: SPHERE (%s)", origin);
					StreamerDebugPrintf(toTTY, "Position: %.2f %.2f %.2f, radius: %.2f", center.GetXf(), center.GetYf(), center.GetZf(), sphere.GetRadiusf());
				}
				else if (volume.IsTypeFrustum())
				{
					StreamerDebugPrintf(toTTY, "Streaming volume: FRUSTUM (%s)", origin);
					Vec3V pos = volume.GetPos();
					Vec3V dir = volume.GetDir();

					StreamerDebugPrintf(toTTY, "Position: %.2f %.2f %.2f, Dir: %.2f %.2f %.2f, Far Clip: %.2f",
						pos.GetXf(), pos.GetYf(), pos.GetZf(), dir.GetXf(), dir.GetYf(), dir.GetZf(), volume.GetFarClip());
				}
				else if (volume.IsTypeLine())
				{
					Vec3V vPos1 = volume.GetPos1();
					Vec3V vPos2 = volume.GetPos2();
					StreamerDebugPrintf(toTTY, "Streaming volume: LINE (%s)", origin);
					StreamerDebugPrintf(toTTY, "Position1: %.2f %.2f %.2f, Position2: %.2f %.2f %.2f", vPos1.GetXf(), vPos1.GetYf(), vPos1.GetZf(), vPos2.GetXf(), vPos2.GetYf(), vPos2.GetZf() );
				}
			}
			//////////////////////////////////////////////////////////////////////////
		}

	}
}

void CSceneStreamerMgr::StreamerDebugPrintf(bool toTTY, const char *format, ...)
{
	va_list args;
	va_start(args, format);

	char msg[1024];
	vformatf(msg, sizeof(msg), format, args);

	if (toTTY)
	{
		Displayf("%s", msg);
	}
	else
	{
#if DEBUG_DRAW
		grcDebugDraw::AddDebugOutput(msg);
#endif // DEBUG_DRAW
	}

	va_end(args);
}
#endif // !__FINAL

void CSceneStreamerMgrWrapper::PreScanUpdate()
{
	g_SceneStreamerMgr.PreScanUpdate();
}
