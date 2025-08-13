//
// scene/LoadScene.cpp
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#include "scene/LoadScene.h"

#include "fwscene/stores/maptypesstore.h"
#include "streaming/streaming.h"
#include "Objects/objectpopulation.h"
#include "scene/streamer/StreamVolume.h"
#include "script/gta_thread.h"
#include "script/script.h"
#include "camera/CamInterface.h"
#include "renderer/Lights/LightEntity.h"

CLoadScene g_LoadScene;

//////////////////////////////////////////////////////////////////////////
#if __BANK
#include "camera/CamInterface.h"
#include "grcore/debugdraw.h"

static const char* apszDbgStateNames[] =
{
	"STATE_PROBE_INTERIOR_PHYSICS",
	"STATE_POPULATING_INTERIOR",
	"STATE_STREAMINGVOLUME"
};

bool CLoadScene::ms_bDbgShowInfo = false;
bool CLoadScene::ms_bDbgUseFrustum = false;
bool CLoadScene::ms_bDbgLoadCollision = false;
bool CLoadScene::ms_bDbgLongSwitchCutscene = false;
float CLoadScene::ms_fDbgRange = 15.0f;
atString CLoadScene::ms_dbgScriptName;
#endif	//__BANK
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Start
// PURPOSE:		starts a new load scene at the specified position
//////////////////////////////////////////////////////////////////////////
void CLoadScene::Start(Vec3V_In vPos, Vec3V_In vDir, float fRange, bool bUseFrustum, s32 controlFlags, eLoadSceneOwner owner)
{
	streamDisplayf("Starting new load scene at -playercoords %4.2f,%4.2f,%4.2f", vPos.GetXf(), vPos.GetYf(), vPos.GetZf());

#if STREAMING_VISUALIZE
	char markerText[256];
	const char *origin = "unknown";

#if __BANK
	origin = m_bOwnedByScript? ms_dbgScriptName.c_str() : "Code";
#endif // __BANK

	formatf(markerText, "New LoadScene from %s at %.2f, %.2f %.2f", origin, vPos.GetXf(), vPos.GetYf(), vPos.GetZf());
	STRVIS_SET_MARKER_TEXT(markerText);
	STRVIS_ADD_MARKER(strStreamingVisualize::MARKER);
	STRVIS_ADD_CALLSTACK_TO_MARKER();
#endif // STREAMING_VISUALIZE

	Assertf(!bUseFrustum || !IsEqualAll(vDir, Vec3V(V_ZERO)) , "vDir should be nonzero");

	if (m_bActive)
	{
#if __BANK
		Warningf( "Starting a new load scene, but one is already active at -playercoords %4.2f,%4.2f,%4.2f owned by %s",
			vPos.GetXf(), vPos.GetYf(), vPos.GetZf(),
			( m_bOwnedByScript? ms_dbgScriptName.c_str() : "CODE" )
		);
#endif	//__BANK

		Stop();
	}

	m_owner = owner;
	m_controlFlags = controlFlags;
	m_bUseFrustum = bUseFrustum;
	m_bUseCameraMatrix = false;
	m_bUseOverriddenAssetRequirements = false;
	m_fRange = fRange;
	m_bOwnedByScript = false;
	m_bActive = true;
	m_vPos = vPos;
	m_vDir = vDir;
	m_pInteriorProxy = NULL;
	m_roomIdx = -1;
	m_eCurrentState = STATE_PROBE_INTERIOR_PHYSICS;
	STRVIS_SET_MARKER_TEXT("LoadScene - start probing interior physics");

	strStreamingEngine::SetLoadSceneActive(true);

	CObjectPopulation::FlushAllSuppressedDummyObjects();

	CStreaming::RequestRenderThreadFlush();

	if (camInterface::IsFadedOut())
	{
		LightEntityMgr::Flush();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StartFromScript
// PURPOSE:		starts a new load scene at specified position, owned by the specified script
//////////////////////////////////////////////////////////////////////////
void CLoadScene::StartFromScript(Vec3V_In vPos, Vec3V_In vDir, float fRange, scrThreadId scriptThreadId, bool bUseFrustum, s32 controlFlags/*=0*/)
{
	streamDisplayf("Script %s requesting new load scene", GtaThread::GetThreadWithThreadId(scriptThreadId)->GetScriptName());

	Assertf(
		!bUseFrustum || !IsEqualAll(vDir, Vec3V(V_ZERO)),
		"Script %s started new load scene with invalid camera direction (should be nonzero)",
		GtaThread::GetThreadWithThreadId(scriptThreadId)->GetScriptName()
	);

#if STREAMING_VISUALIZE
	char markerText[256];
	formatf(markerText, "LoadScene from %s: ", GtaThread::GetThreadWithThreadId(scriptThreadId)->GetScriptName());
	STRVIS_SET_MARKER_TEXT(markerText);
	formatf(markerText, "(%s)", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	STRVIS_SET_MARKER_TEXT(markerText);
#endif // STREAMING_VISUALIZE

	Start(vPos, vDir, fRange, bUseFrustum, controlFlags, LOADSCENE_OWNER_SCRIPT);
	SetScriptThreadId(scriptThreadId);

#if __BANK
	ms_dbgScriptName = GtaThread::GetThreadWithThreadId(scriptThreadId)->GetScriptName();
#endif
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		returns true if still running, false otherwise
//////////////////////////////////////////////////////////////////////////
void CLoadScene::Update()
{
	if (m_bActive)
	{
		switch (m_eCurrentState)
		{
		case STATE_PROBE_INTERIOR_PHYSICS:
			UpdateProbeInteriorPhysics();
			break;
		case STATE_POPULATING_INTERIOR:
			UpdatePopulatingInterior();
			break;
		case STATE_STREAMINGVOLUME:
			UpdateStreamingVolume();
			break;
		default:
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsLoaded
// PURPOSE:		returns true if there is an active load scene and all requirements are in memory
//////////////////////////////////////////////////////////////////////////
bool CLoadScene::IsLoaded()
{
	if (m_bActive && m_eCurrentState==STATE_STREAMINGVOLUME)
	{
		return CStreamVolumeMgr::HasLoaded( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY );
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Stop
// PURPOSE:		ends load scene and releases any resources
//////////////////////////////////////////////////////////////////////////
void CLoadScene::Stop()
{
	if (!m_bActive)
		return;

	STRVIS_SET_MARKER_TEXT("LoadScene ends");

	if (CStreamVolumeMgr::IsVolumeActive( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY ))
	{
		CStreamVolumeMgr::Deregister( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY );
	}

	if (m_pInteriorProxy)
	{
		m_pInteriorProxy->SetRequiredByLoadScene(false);
		m_pInteriorProxy->SetRequestedState( CInteriorProxy::RM_LOADSCENE, CInteriorProxy::PS_NONE, true);
		m_pInteriorProxy->ReleaseStaticBoundsFile();
		m_pInteriorProxy->UpdateImapPinState();
	}

	m_bActive = false;

	//if ( m_owner != LOADSCENE_OWNER_PLAYERSWITCH )	// player switch takes responsibilty for clearing loadscene flags
	{
		strStreamingEngine::GetInfo().ClearFlagForAll(STRFLAG_LOADSCENE);
	}
	
	strStreamingEngine::SetLoadSceneActive(false);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateProbeInteriorPhysics
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CLoadScene::UpdateProbeInteriorPhysics()
{
	Vector3 vCurrentPos = VEC3V_TO_VECTOR3(m_vPos);
	CInteriorProxy* pIntProxy = NULL;
	bool bKnownResult = CInteriorInst::RequestInteriorAtLocation(vCurrentPos, pIntProxy, m_roomIdx);

	if (pIntProxy)
	{
		m_pInteriorProxy = pIntProxy;
	}

	if (bKnownResult)
	{
		if (m_pInteriorProxy)
		{
			StartPopulatingInterior();
		}
		else
		{
			StartStreamingForExterior();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdatePopulatingInterior
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CLoadScene::UpdatePopulatingInterior()
{
	if (m_pInteriorProxy && m_pInteriorProxy->GetCurrentState()== CInteriorProxy::PS_FULL_WITH_COLLISIONS)
	{
		StartStreamingForInterior();
	} else
	{
		Displayf("*** WAITING FOR INTERIOR TO POPULATE! (%s - CurrentState:%d RequestedState:%d ImapActive:%i IsDisabled:%i IsCapped:%i) ***",
			m_pInteriorProxy->GetName().TryGetCStr(), 
			m_pInteriorProxy->GetCurrentState(), 
			m_pInteriorProxy->GetRequestedState(CInteriorProxy::RM_LOADSCENE),
			m_pInteriorProxy->IsContainingImapActive(), 
			m_pInteriorProxy->GetIsDisabled(),
			m_pInteriorProxy->GetIsCappedAtPartial() );
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateStreamingVolume
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CLoadScene::UpdateStreamingVolume()
{
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StartStreamingForExteriors
// PURPOSE:		creates a streaming volume for exterior map streaming
//////////////////////////////////////////////////////////////////////////
void CLoadScene::StartStreamingForExterior()
{
	STRVIS_SET_MARKER_TEXT("LoadScene - start streaming for exterior");
	m_eCurrentState = STATE_STREAMINGVOLUME;

	// make sure there is no volume already running
	CStreamVolumeMgr::Deregister(CStreamVolumeMgr::VOLUME_SLOT_PRIMARY);

	fwBoxStreamerAssetFlags assetFlags = m_bUseOverriddenAssetRequirements ? m_overrideAssetFlags : fwBoxStreamerAsset::MASK_MAPDATA;
	if ((m_controlFlags & FLAG_INCLUDE_COLLISION)!=0)
	{
		assetFlags |= fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER;
	}
	
	const bool bLongSwitchCutscene = ((m_controlFlags & FLAG_LONGSWITCH_CUTSCENE) != 0);

	if (m_bUseFrustum)
	{
		CStreamVolumeMgr::RegisterFrustum(m_vPos, m_vDir, m_fRange, assetFlags, true, CStreamVolumeMgr::VOLUME_SLOT_PRIMARY, bLongSwitchCutscene);

		if (m_bUseCameraMatrix)
		{
			CStreamVolume& volume = CStreamVolumeMgr::GetVolume(CStreamVolumeMgr::VOLUME_SLOT_PRIMARY);
			volume.SetCameraMtx(m_rageCameraMatrix);
		}
	}
	else
	{
		spdSphere searchVolume(m_vPos, ScalarV(m_fRange));
		CStreamVolumeMgr::RegisterSphere(searchVolume, assetFlags, true, CStreamVolumeMgr::VOLUME_SLOT_PRIMARY, bLongSwitchCutscene, true);
	}

	// for long switches in the middle of a cutscene, only stream high detail
	if (bLongSwitchCutscene)
	{
		CStreamVolume& volume = CStreamVolumeMgr::GetVolume( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY );
		volume.SetLodMask( LODTYPES_FLAG_HD | LODTYPES_FLAG_ORPHANHD );
	}
	else if (m_bUseOverriddenAssetRequirements)
	{
		CStreamVolume& volume = CStreamVolumeMgr::GetVolume( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY );
		volume.SetLodMask( m_overrideLodMask );
	}

#if !__FINAL
	if (m_bOwnedByScript)
	{
		CStreamVolumeMgr::SetVolumeScriptInfo(
			m_scriptThreadId,
			( GtaThread::GetThreadWithThreadId(m_scriptThreadId)
			? GtaThread::GetThreadWithThreadId(m_scriptThreadId)->GetScriptName()
			: "<not running>" )
			);
	}
#endif	//!__FINAL
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StartPopulatingInterior
// PURPOSE:		start firing up an interior, may involve some streaming in the future
//////////////////////////////////////////////////////////////////////////
void CLoadScene::StartPopulatingInterior()
{
	STRVIS_SET_MARKER_TEXT("LoadScene - start populating interior");
	m_eCurrentState = STATE_POPULATING_INTERIOR;
	m_pInteriorProxy->SkipShellAssetBlocking(true);
	m_pInteriorProxy->SetRequestedState( CInteriorProxy::RM_LOADSCENE, CInteriorProxy::PS_FULL_WITH_COLLISIONS, true);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StartStreamingForInterior
// PURPOSE:		creates a streaming volume for interior streaming
//////////////////////////////////////////////////////////////////////////
void CLoadScene::StartStreamingForInterior()
{
	STRVIS_SET_MARKER_TEXT("LoadScene - start streaming interior");
	m_eCurrentState = STATE_STREAMINGVOLUME;

	// make sure there is no volume already running
	CStreamVolumeMgr::Deregister(CStreamVolumeMgr::VOLUME_SLOT_PRIMARY);

	fwBoxStreamerAssetFlags assetFlags = m_bUseOverriddenAssetRequirements ? m_overrideAssetFlags : 0;

	if ((m_controlFlags & FLAG_INTERIOR_AND_EXTERIOR)!=0)
	{
		assetFlags |= fwBoxStreamerAsset::MASK_MAPDATA;
	}

	if ((m_controlFlags & FLAG_INCLUDE_COLLISION)!=0)
	{
		assetFlags |= fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER;
	}

	const bool bLongSwitchCutscene = ((m_controlFlags & FLAG_LONGSWITCH_CUTSCENE) != 0);

	if (m_bUseFrustum)
	{
		CStreamVolumeMgr::RegisterFrustum(m_vPos, m_vDir, m_fRange, assetFlags, true, CStreamVolumeMgr::VOLUME_SLOT_PRIMARY, bLongSwitchCutscene);

		if (m_bUseCameraMatrix)
		{
			CStreamVolume& volume = CStreamVolumeMgr::GetVolume(CStreamVolumeMgr::VOLUME_SLOT_PRIMARY);
			volume.SetCameraMtx(m_rageCameraMatrix);
		}
	}
	else
	{
		spdSphere searchVolume(m_vPos, ScalarV(m_fRange));
		CStreamVolumeMgr::RegisterSphere(searchVolume, assetFlags, true, CStreamVolumeMgr::VOLUME_SLOT_PRIMARY, bLongSwitchCutscene, true);
	}

	// for long switches in the middle of a cutscene, only stream high detail
	if (bLongSwitchCutscene)
	{
		CStreamVolume& volume = CStreamVolumeMgr::GetVolume( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY );
		volume.SetLodMask( LODTYPES_FLAG_HD | LODTYPES_FLAG_ORPHANHD );
	}


#if !__FINAL
	if (m_bOwnedByScript)
	{
		CStreamVolumeMgr::SetVolumeScriptInfo(
			m_scriptThreadId,
			( GtaThread::GetThreadWithThreadId(m_scriptThreadId)
				? GtaThread::GetThreadWithThreadId(m_scriptThreadId)->GetScriptName()
				: "<not running>" )
		);
	}
#endif	//!__FINAL
}

#if __BANK

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitWidgets
// PURPOSE:		some debug widgets for testing a load scene
//////////////////////////////////////////////////////////////////////////
void CLoadScene::InitWidgets(bkBank& bank)
{
	bank.PushGroup("New Load Scene");
	bank.AddToggle("Show active load scene", &ms_bDbgShowInfo);
	bank.AddSlider("Range", &ms_fDbgRange, 1.0f, 4500.0f, 1.0f);
	bank.AddToggle("Use frustum", &ms_bDbgUseFrustum);
	bank.AddToggle("Load collision", &ms_bDbgLoadCollision);
	bank.AddToggle("Long switch cut", &ms_bDbgLongSwitchCutscene);
	bank.AddButton("Start load scene at cam pos", StartLoadSceneCB);
	bank.AddButton("Stop load scene", StopLoadSceneCB);
	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateDebug
// PURPOSE:		debug draw etc
//////////////////////////////////////////////////////////////////////////
void CLoadScene::UpdateDebug()
{
	if (ms_bDbgShowInfo && m_bActive)
	{
		// pos and state
		grcDebugDraw::AddDebugOutput("Load scene active at (%4.2f, %4.2f, %4.2f) range %4.2fm", m_vPos.GetXf(), m_vPos.GetYf(), m_vPos.GetZf(), m_fRange);
		grcDebugDraw::AddDebugOutput("Current state: %s", apszDbgStateNames[m_eCurrentState]);
		grcDebugDraw::AddDebugOutput("Control flags: 0x%x", m_controlFlags);

		// owner
		if (m_bOwnedByScript)
		{
			grcDebugDraw::AddDebugOutput("Owned by: script %s", GtaThread::GetThreadWithThreadId(m_scriptThreadId)->GetScriptName());
		}
		else
		{
			grcDebugDraw::AddDebugOutput("Owned by: code");
		}

		// interiors
		if (m_pInteriorProxy)
		{
			grcDebugDraw::AddDebugOutput("interior: %s (%d)", m_pInteriorProxy->GetModelName(), m_pInteriorProxy->GetCurrentState());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StartLoadSceneCB
// PURPOSE:		debug callback for rag to initiate a load scene
//////////////////////////////////////////////////////////////////////////
void CLoadScene::StartLoadSceneCB()
{
	s32 controlFlags = 0;
	if (ms_bDbgLoadCollision)
	{
		controlFlags |= FLAG_INCLUDE_COLLISION;
	}
	if (ms_bDbgLongSwitchCutscene)
	{
		controlFlags |= FLAG_LONGSWITCH_CUTSCENE;
	}
	g_LoadScene.Start( VECTOR3_TO_VEC3V(camInterface::GetPos()), VECTOR3_TO_VEC3V(camInterface::GetFront()), ms_fDbgRange, ms_bDbgUseFrustum, controlFlags, LOADSCENE_OWNER_DEBUG );
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StopLoadSceneCB
// PURPOSE:		debug callback for rag to terminate a load scene
//////////////////////////////////////////////////////////////////////////
void CLoadScene::StopLoadSceneCB()
{
	g_LoadScene.Stop();
}

#endif	//__BANK
