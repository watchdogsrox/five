/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/streamer/SceneLoader.cpp
// PURPOSE : handles sync/async loading of scene data at new position
// AUTHOR :  various
// CREATED : 09/08/10
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/streamer/SceneLoader.h"
#include "streaming/streaming_channel.h"

STREAMING_OPTIMISATIONS()

#include "fwscene/stores/maptypesstore.h"
#include "fwsys/gameskeleton.h"
#include "game/clock.h"
#include "modelinfo/BaseModelInfo.h"
#include "modelinfo/TimeModelInfo.h"
#include "objects/objectpopulation.h"
#include "pathserver/pathserver.h"
#include "peds/PlayerInfo.h"
#include "renderer/PlantsMgr.h"
#include "renderer/Lights/LightEntity.h"
#include "fwscene/lod/LodTypes.h"
#include "scene/portals/InteriorInst.h"
#include "scene/portals/PortalTracker.h"
#include "scene/world/GameWorld.h"
#include "fwscene/search/SearchVolumes.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/world/EntityContainer.h"
#include "streaming/streaming.h"
#include "streaming/streamingvisualize.h"
#include "scene/ipl/iplcullbox.h"
#include "scene/LoadScene.h"
#include "script/script.h"
#include "physics/physics.h"
#include "streaming/zonedassetmanager.h"

//fw headers
#include "fwscene/stores/mapdatastore.h"

#if __ASSERT
#include "scene/streamer/StreamVolume.h"
#endif

#if __BANK
#include "grcore/debugdraw.h"
#include "scene/debug/SceneStreamerDebug.h"
#include "scene/world/GameWorld.h"
#endif	//__BANK

#define LOADSCENE_SEARCH_RADIUS_LOD		(600.0f)
#define LOADSCENE_SEARCH_RADIUS_HD		(80.0f)
#define LOADSCENE_RESULTS_ALLOCSIZE		(200)
#define LOADSCENE_ASYNC_TIMEOUT			(200)

bool RequestEntityCB(CEntity* pEntity, void* UNUSED_PARAM(pData))
{
	if (pEntity)
	{
		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();

		bool bRequest = true;

#if __BANK
		if(g_HideDebug && (pModelInfo->GetAttributes()==MODEL_ATTRIBUTE_IS_DEBUG))
		{
			bRequest = false;
		}
#endif //__BANK	

		if(pEntity->IsBaseFlagSet(fwEntity::DONT_STREAM))
		{
			bRequest = false;;
		}

		CTimeInfo* pTimeInfo = pModelInfo->GetTimeInfo();
		if(pTimeInfo && !pTimeInfo->IsOn(CClock::GetHour()))
		{
			bRequest = false;;
		}


		if (bRequest)
		{
			if(pModelInfo->GetModelType() == MI_TYPE_MLO)
			{
				// add MLO objects into the request list
				(static_cast<CInteriorInst*>(pEntity))->RequestObjects(STRFLAG_LOADSCENE, false);
			}
			else
			{
				CModelInfo::RequestAssets(pEntity->GetBaseModelInfo(), STRFLAG_LOADSCENE);
			}
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	LoadScene
// PURPOSE:		synchronously loads map data etc around specified position
//////////////////////////////////////////////////////////////////////////
void CSceneLoader::LoadScene(const Vector3& vPos)
{
	g_LoadScene.Stop();

	streamDisplayf("Starting (blocking) load scene at -playercoords %f,%f,%f", vPos.GetX(), vPos.GetY(), vPos.GetZ());

	if (IsLoading())
	{
		StopLoadScene();
		Warningf("Starting a (blocking) load scene, but one is already underway. This should not happen");
	}

	m_vLoadPos = vPos;
	m_bLoadSceneThisFrame = true;

#if STREAMING_VISUALIZE
	STRVIS_ADD_MARKER(strStreamingVisualize::MARKER | strStreamingVisualize::BLOCKED_FOR_STREAMING);
	char markerText[256];
	const char *origin = "unknown";

#if __BANK
	const char *scriptName = CTheScripts::GetCurrentScriptName();
	origin = (scriptName && scriptName[0]) ? scriptName : "code";
#endif // __BANK

	formatf(markerText, "LoadScene at %.2f %.2f %.2f from %s", vPos.x, vPos.y, vPos.z, origin);
	STRVIS_SET_MARKER_TEXT(markerText);
	STRVIS_ADD_CALLSTACK_TO_MARKER();
#endif // STREAMING_VISUALIZE

	Process(SCENELOADER_STAGE_1);
	CStreaming::LoadAllRequestedObjects();
	Process(SCENELOADER_STAGE_2);
	CStreaming::LoadAllRequestedObjects();
	Process(SCENELOADER_STAGE_3);
	CStreaming::LoadAllRequestedObjects();
	Process(SCENELOADER_STAGE_4);

	streamDebugf1("End load scene\n");
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Process
// PURPOSE:		performs load scene, broken up into stages shared between sync and async code
//////////////////////////////////////////////////////////////////////////
void CSceneLoader::Process(u32 eStage)
{
	switch (eStage)
	{
	case SCENELOADER_STAGE_1:
		CObjectPopulation::FlushAllSuppressedDummyObjects();
		gRenderThreadInterface.Flush();										// releases the current visible objects
		m_bLoadingScene = true;
		strStreamingEngine::SetLoadSceneActive(true);
		CPlantMgr::Shutdown(SHUTDOWN_WITH_MAP_LOADED);
		CStreaming::PurgeRequestList();										// no longer interested in loading stuff on current request list
		CStreaming::GetCleanup().DeleteAllDrawableReferences();
		LightEntityMgr::Update();
		CIplCullBox::Update(m_vLoadPos);
		CZonedAssetManager::Reset();

		INSTANCE_STORE.GetBoxStreamer().RequestAboutPos(RCC_VEC3V(m_vLoadPos));	// request and load streamed IPL files around position
		INSTANCE_STORE.RemoveUnrequired(true);
		gRenderThreadInterface.Flush();											// allow protected archetypes to time out
		g_MapTypesStore.RemoveUnrequired();										// remove unwanted streamed archetype files

#if !__FINAL && 0
		// in debug builds, the user can debug-warp around and get a really fragmented descriptor pool.
		fwEntityContainer::DefragEntityDescPool(true, true);
#endif
		break;
	case SCENELOADER_STAGE_2:
		if(CGameWorld::GetMainPlayerInfo() && CGameWorld::GetMainPlayerInfo()->GetPlayerPed())
		{
			// update object population - perform any dummy/real object conversions required
			CObjectPopulation::ManageObjectPopulation(true);
		}
		m_bSafeToDelete = false;
		RequestAroundPos(m_vLoadPos, LOADSCENE_SEARCH_RADIUS_LOD, LOADSCENE_SEARCH_RADIUS_HD);
		break;
	case SCENELOADER_STAGE_3:
		m_bSafeToDelete = true;
		CPathServer::Process(m_vLoadPos);
		break;
	case SCENELOADER_STAGE_4:
		CStreaming::ClearFlagForAll(STRFLAG_LOADSCENE);
		m_bLoadingScene = false;
		strStreamingEngine::SetLoadSceneActive(false);
		CPlantMgr::PreUpdateOnceForNewCameraPos(m_vLoadPos);	
		// Ensure that collision is there
		CPhysics::LoadAboutPosition(RCC_VEC3V(m_vLoadPos));
		break;
	default:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RequestAroundPos
// PURPOSE:		does some world searches and requests the models
//////////////////////////////////////////////////////////////////////////
void CSceneLoader::RequestAroundPos(const Vector3& vPos, float fLodRadius, float fHdRadius)
{
	fwIsSphereIntersectingVisible lodTestSphere(spdSphere(RCC_VEC3V(vPos), ScalarV(fLodRadius)));
	fwIsSphereIntersectingVisible hdTestSphere(spdSphere(RCC_VEC3V(vPos), ScalarV(fHdRadius)));

	// search for LODs within specified radius
	CGameWorld::ForAllEntitiesIntersecting
	(
		&lodTestSphere,
		RequestEntityCB,
		NULL,
		ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING | ENTITY_TYPE_MASK_OBJECT | ENTITY_TYPE_MASK_DUMMY_OBJECT | ENTITY_TYPE_MASK_MLO,
		SEARCH_LOCATION_EXTERIORS,
		SEARCH_LODTYPE_LOWDETAIL,
		SEARCH_OPTION_FORCE_PPU_CODEPATH,	// temporarily, for debug purposes
		WORLDREP_SEARCHMODULE_STREAMING
	);

	// search for HDs within specified radius
	CGameWorld::ForAllEntitiesIntersecting
	(
		&hdTestSphere,
		RequestEntityCB,
		NULL,
		ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING | ENTITY_TYPE_MASK_OBJECT | ENTITY_TYPE_MASK_DUMMY_OBJECT | ENTITY_TYPE_MASK_MLO,
		SEARCH_LOCATION_EXTERIORS | SEARCH_LOCATION_INTERIORS,
		SEARCH_LODTYPE_HIGHDETAIL,
		SEARCH_OPTION_FORCE_PPU_CODEPATH,	// temporarily, for debug purposes
		WORLDREP_SEARCHMODULE_STREAMING
	);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StartLoadScene
// PURPOSE:		starts a non-blocking load scene
//////////////////////////////////////////////////////////////////////////
void CSceneLoader::StartLoadScene(const Vector3& vPos)
{
	Assertf(m_nState==SCENELOADER_ASYNCLOAD_STATE_DONE, "Can't start a new load scene while you are in the middle of one");

	if(m_nState == SCENELOADER_ASYNCLOAD_STATE_DONE)
	{
		streamDisplayf("Starting (non-blocking) load scene at pos (%f, %f, %f)\n", vPos.GetX(), vPos.GetY(), vPos.GetZ());
		m_vLoadPos = vPos;
		m_nState = SCENELOADER_ASYNCLOAD_STATE_PROCESS;
		m_nCurrentStage = SCENELOADER_STAGE_1;
		m_bLoadSceneThisFrame = true;

		Assertf(
			!CStreamVolumeMgr::IsVolumeActive( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY ),
			"Calling NETWORK_START_LOAD_SCENE while a streaming volume is active (owner: %s). Don't do that",
			CStreamVolumeMgr::GetVolume(CStreamVolumeMgr::VOLUME_SLOT_PRIMARY).GetDebugOwner()
		);

		UpdateLoadScene();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StopLoadScene
// PURPOSE:		stops a non-blocking load scene that has been started
//////////////////////////////////////////////////////////////////////////
void CSceneLoader::StopLoadScene()
{
	streamDisplayf("Stopping load scene");
	m_nState = SCENELOADER_ASYNCLOAD_STATE_DONE;
	m_bLoadingScene = false;
	strStreamingEngine::SetLoadSceneActive(false);
	CStreaming::ClearFlagForAll(STRFLAG_LOADSCENE);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateLoadScene
// PURPOSE:		updates a non-blocking load scene, returns true when done
//////////////////////////////////////////////////////////////////////////
bool CSceneLoader::UpdateLoadScene()
{
#if __BANK
	if (CSceneStreamerDebug::TrackLoadScene())
	{
		Vector3 vPlayerPos = CGameWorld::FindFollowPlayerCoors();
		grcDebugDraw::AddDebugOutput(
			"Processing Network Load Scene at (%f, %f, %f), player pos is (%f, %f, %f)",
			m_vLoadPos.x, m_vLoadPos.y, m_vLoadPos.z,
			vPlayerPos.x, vPlayerPos.y, vPlayerPos.z			
		);
	}
#endif	//__BANK

	Assertf(
		!CStreamVolumeMgr::IsVolumeActive( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY ),
		"Calling NETWORK_UPDATE_LOAD_SCENE while a streaming volume is active (owner: %s). Don't do that",
		CStreamVolumeMgr::GetVolume( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY ).GetDebugOwner()
	);

	streamDebugf3("Updating load scene, state=%d, stage=%d\n", m_nState, m_nCurrentStage);
	m_bLoadSceneThisFrame = true;
	switch (m_nState)
	{
	case SCENELOADER_ASYNCLOAD_STATE_PROCESS:
		Process(m_nCurrentStage);
		strStreamingEngine::GetLoader().StartLoadAllRequestedObjects();
		m_nState = SCENELOADER_ASYNCLOAD_STATE_WAITINGFORSTREAMING;
		m_asyncTimeout = 0;
		break;
	case SCENELOADER_ASYNCLOAD_STATE_WAITINGFORSTREAMING:
		m_asyncTimeout++;
		if(strStreamingEngine::GetLoader().UpdateLoadAllRequestedObjects() || m_asyncTimeout>=LOADSCENE_ASYNC_TIMEOUT)
		{
			m_nCurrentStage++;
			m_nState = ((m_nCurrentStage >= SCENELOADER_STAGE_TOTAL) ? SCENELOADER_ASYNCLOAD_STATE_DONE : SCENELOADER_ASYNCLOAD_STATE_PROCESS);
		}
		break;
	case SCENELOADER_ASYNCLOAD_STATE_DONE:
		// intentional fall-through
	default:
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	LoadInteriorRoom
// PURPOSE:		loads an interior room
//////////////////////////////////////////////////////////////////////////
void CSceneLoader::LoadInteriorRoom(class CInteriorInst* pInterior, u32 roomIdx)
{
	// flush render thread so it releases the current visible objects
	gRenderThreadInterface.Flush();

	Assert(!m_bLoadingScene);

	m_bLoadingScene = true;
	pInterior->RequestRoom(roomIdx, STRFLAG_LOADSCENE);
	// request room zero as well
	pInterior->RequestRoom(0, STRFLAG_LOADSCENE);

	CStreaming::LoadAllRequestedObjects();
	CStreaming::ClearFlagForAll(STRFLAG_LOADSCENE);


	m_bLoadingScene = false;
}
