/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/debug/SceneStreamerDebug.cpp
// PURPOSE : debug widgets related to the scene streamer
// AUTHOR :  Ian Kiigan
// CREATED : 13/10/10
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/debug/SceneStreamerDebug.h"

SCENE_OPTIMISATIONS();

#if __BANK
#include "camera/CamInterface.h"
#include "grcore/debugdraw.h"
#include "objects/DummyObject.h"
#include "scene/playerswitch/PlayerSwitchDebug.h"
#include "scene/debug/SceneCostTracker.h"
#include "scene/streamer/StreamVolume.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "streaming/streamingengine.h"

CSceneStreamerDebug g_SceneStreamerDebug;
s32 CSceneStreamerDebug::ms_nSelectedLoader						= SCENESTREAMERMGR_LOADER_BLOCKING;
bool CSceneStreamerDebug::ms_bTraceSelected						= false;
bool CSceneStreamerDebug::ms_bColorizeScene						= false;
bool CSceneStreamerDebug::ms_bTrackSceneLoader					= false;
bool CSceneStreamerDebug::ms_bAsyncLoadScenePending 			= false;
bool CSceneStreamerDebug::ms_bDrawSceneStreamerRequestsOnVmap	= false;
bool CSceneStreamerDebug::ms_bDrawSceneStreamerDeletionsOnVmap	= false;
bool CSceneStreamerDebug::ms_bDrawSceneStreamerRequiredOnVmap	= false;
bool CSceneStreamerDebug::ms_bDrawSceneStreamerSelectedOnVmap	= false;
bool CSceneStreamerDebug::ms_SceneStreamerRequestsAllPriorities	= false;

const char* apszSceneLoaders[SCENESTREAMERMGR_LOADER_TOTAL] =
{
	"Blocking",
	"Non-blocking"
};

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitWidgets
// PURPOSE:		rag widgets to allow selection of streamer code etc
//////////////////////////////////////////////////////////////////////////
void CSceneStreamerDebug::AddWidgets(bkBank* pBank)
{
	Assert(pBank);

	pBank->PushGroup("Network Load Scene");
	pBank->AddToggle("Track network load scenes", &ms_bTrackSceneLoader);
	pBank->PopGroup();

	pBank->PushGroup("SceneStreamer", false);
	pBank->AddTitle("Selected PVS Streamer");
	pBank->AddToggle("Trace selected entity", &ms_bTraceSelected);
	pBank->AddSeparator();
	pBank->AddToggle("Stream from PVS", &g_SceneStreamerMgr.ms_bStreamFromPVS);
	pBank->AddToggle("Stream by proximity (HD)", &g_SceneStreamerMgr.ms_bStreamByProximity_HD);
	pBank->AddToggle("Stream map swaps", &g_SceneStreamerMgr.ms_bStreamMapSwaps);
	pBank->AddSlider("Stream short radius (HD)", &g_SceneStreamerMgr.ms_fStreamShortRadius_HD, 0.0f, 500.0f, 10.0f);
	pBank->AddSlider("Stream long radius (HD)", &g_SceneStreamerMgr.ms_fStreamRadius_HD, 0.0f, 500.0f, 10.0f);
	pBank->AddSlider("Stream short radius (Interior)", &g_SceneStreamerMgr.ms_fStreamShortRadius_Interior, 0.0f, 50.0f, 1.0f);
	pBank->AddSlider("Stream long radius (Interior)", &g_SceneStreamerMgr.ms_fStreamRadius_Interior, 0.0f, 100.0f, 1.0f);
	pBank->AddSeparator();

	pBank->AddCombo("LoadScene type", &ms_nSelectedLoader, SCENESTREAMERMGR_LOADER_TOTAL, &apszSceneLoaders[0]);
	pBank->AddButton("Flush Scene Now", datCallback(FlushSceneNow));
	pBank->AddButton("Load Scene Now", datCallback(LoadSceneNow));
	pBank->AddSeparator();

	pBank->AddToggle("Show requests on vmap", &ms_bDrawSceneStreamerRequestsOnVmap);
	pBank->AddToggle("Show deletions on vmap", &ms_bDrawSceneStreamerDeletionsOnVmap);
	pBank->AddToggle("Show required on vmap", &ms_bDrawSceneStreamerRequiredOnVmap);
	pBank->AddToggle("Show selected on on vmap", &ms_bDrawSceneStreamerSelectedOnVmap);
	pBank->AddToggle("Request all priorities", &ms_SceneStreamerRequestsAllPriorities);
	pBank->AddSeparator();

	CStreamVolumeMgr::AddWidgets(pBank);

	pBank->PopGroup();

	CPlayerSwitchDbg::InitWidgets(*pBank);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	FlushSceneNow
// PURPOSE:		request a scene flush - useful for testing load scene fn
//////////////////////////////////////////////////////////////////////////
void CSceneStreamerDebug::FlushSceneNow()
{
	g_strStreamingInterface->RequestFlush(false);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	LoadSceneNow
// PURPOSE:		rag-driven fn to do a test load scene around current camera pos
//////////////////////////////////////////////////////////////////////////
void CSceneStreamerDebug::LoadSceneNow()
{
	switch (ms_nSelectedLoader)
	{
	case SCENESTREAMERMGR_LOADER_BLOCKING:
		g_SceneStreamerMgr.ms_sceneLoader.LoadScene(camInterface::GetPos());
		break;
	case SCENESTREAMERMGR_LOADER_NONBLOCKING:
		ms_bAsyncLoadScenePending = true;
		g_SceneStreamerMgr.ms_sceneLoader.StartLoadScene(camInterface::GetPos());
		break;
	default:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsHighPriorityEntity
// PURPOSE:		uses the current streamer to assess whether an entity is high priority
//////////////////////////////////////////////////////////////////////////
bool CSceneStreamerDebug::IsHighPriorityEntity(const CEntity* pEntity)
{
	CSceneStreamerBase* pStreamer = g_SceneStreamerMgr.GetStreamer();
	const Vector3& vCamPos = camInterface::GetPos();
	const Vector3& vCamDir = camInterface::GetFront();
	return pStreamer->IsHighPriorityEntity(pEntity, vCamDir, vCamPos);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		debug update
//////////////////////////////////////////////////////////////////////////
void CSceneStreamerDebug::Update()
{
	if (g_SceneStreamerMgr.ms_sceneLoader.IsLoading())
	{
		const Vector3& vPos = g_SceneStreamerMgr.ms_sceneLoader.GetLoadPos();
		grcDebugDraw::AddDebugOutput("Load Scene Active at (%4.2f, %4.2f, %4.2f)", vPos.x, vPos.y, vPos.z);
	}

	CStreamVolumeMgr::DebugDraw();

	CPlayerSwitchDbg::Update();

	if (ms_bTrackSceneLoader)
	{
		grcDebugDraw::AddDebugOutput("Dummy Object Pool: (%d / %d)", CDummyObject::GetPool()->GetNoOfUsedSpaces(), CDummyObject::GetPool()->GetSize());
	}
	if (ms_bAsyncLoadScenePending)
	{
		if (g_SceneStreamerMgr.ms_sceneLoader.UpdateLoadScene())
		{
			ms_bAsyncLoadScenePending = false;
			g_SceneStreamerMgr.ms_sceneLoader.StopLoadScene();
		}
	}
}

#endif //__BANK