//
// scene/LoadScene.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef _SCENE_LOADSCENE_H_
#define _SCENE_LOADSCENE_H_

#include "fwscene/stores/boxstreamerassets.h"
#include "scene/portals/InteriorProxy.h"
#include "script/thread.h"
#include "system/bit.h"
#include "vectormath/vec3v.h"

#if __BANK
#include "atl/string.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#endif


// WIP:
// new loadscene which probes interior collision and streams the correct interior
// or exterior volume using streaming volumes
class CLoadScene
{
public:

	enum eLoadSceneOwner
	{
		LOADSCENE_OWNER_DEFAULT,
		LOADSCENE_OWNER_SCRIPT,
		LOADSCENE_OWNER_PLAYERSWITCH,
		LOADSCENE_OWNER_WARPMGR,
		LOADSCENE_OWNER_DEBUG
	};
	enum
	{
		FLAG_INCLUDE_COLLISION		= BIT(0),
		FLAG_LONGSWITCH_CUTSCENE	= BIT(1),
		FLAG_INTERIOR_AND_EXTERIOR	= BIT(2)
	};

	CLoadScene() : m_bActive(false), m_bOwnedByScript(false) {}
	~CLoadScene() {}

	void Start(Vec3V_In vPos, Vec3V_In vDir, float fRange, bool bUseFrustum, s32 controlFlags, eLoadSceneOwner owner=LOADSCENE_OWNER_DEFAULT);
	void StartFromScript(Vec3V_In vPos, Vec3V_In vDir, float fRange, scrThreadId scriptThreadId, bool bUseFrustum, s32 controlFlags);
	void Update();
	void Stop();
	void DeregisterForScript(scrThreadId scriptThreadId)
	{
		if (m_bOwnedByScript && m_scriptThreadId==scriptThreadId)
		{
			Stop();
		}
	}

	bool IsActive() { return m_bActive; }
	bool IsLoaded();
	void SetScriptThreadId(scrThreadId scriptThreadId) { m_bOwnedByScript=true; m_scriptThreadId=scriptThreadId; }

	void SetCameraMatrix(Mat34V_In rageCameraMatrix) { m_bUseCameraMatrix=true; m_rageCameraMatrix=rageCameraMatrix; }

	bool IsSafeToQueryInteriorProxy() { return m_eCurrentState == STATE_STREAMINGVOLUME; }

	void SkipInteriorChecks() {
		if (IsActive() && m_eCurrentState!=STATE_STREAMINGVOLUME)
		{
			m_pInteriorProxy = NULL;
			StartStreamingForExterior();
		}
	}

	CInteriorProxy* GetInteriorProxy()
	{
		if (IsActive())
		{
			return m_pInteriorProxy;
		}
		return NULL;
	}

	s32 GetRoomIdx()
	{
		if (IsActive())
		{
			return m_roomIdx;
		}
		return -1;
	}

	void OverrideRequiredAssets(const fwBoxStreamerAssetFlags assetFlags, const u32 lodMask)
	{
		m_bUseOverriddenAssetRequirements = true;
		m_overrideAssetFlags = assetFlags;
		m_overrideLodMask = lodMask;
	}

private:
	enum eState
	{
		STATE_PROBE_INTERIOR_PHYSICS = 0,
		STATE_POPULATING_INTERIOR,
		STATE_STREAMINGVOLUME,

		STATE_TOTAL
	};

	void StartPopulatingInterior();
	void StartStreamingForInterior();
	void StartStreamingForExterior();
	void UpdateProbeInteriorPhysics();
	void UpdateStreamingVolume();
	void UpdatePopulatingInterior();

	eLoadSceneOwner m_owner;
	s32 m_controlFlags;
	float m_fRange;
	bool m_bOwnedByScript;
	bool m_bActive;
	eState m_eCurrentState;
	scrThreadId m_scriptThreadId;
	Vec3V m_vPos;
	Vec3V m_vDir;
	CInteriorProxy* m_pInteriorProxy;
	s32 m_roomIdx;
	bool m_bUseFrustum;
	bool m_bUseCameraMatrix;
	Mat34V m_rageCameraMatrix;

	bool m_bUseOverriddenAssetRequirements;
	fwBoxStreamerAssetFlags m_overrideAssetFlags;
	u32 m_overrideLodMask;

	//////////////////////////////////////////////////////////////////////////
#if __BANK
public:
	void InitWidgets(bkBank& bank);
	void UpdateDebug();
	atString& GetScriptName() { return ms_dbgScriptName; }
private:
	static void StartLoadSceneCB();
	static void StopLoadSceneCB();
	static bool ms_bDbgShowInfo;
	static bool ms_bDbgUseFrustum;
	static bool ms_bDbgLoadCollision;
	static bool ms_bDbgLongSwitchCutscene;
	static float ms_fDbgRange;
	static atString ms_dbgScriptName;
#endif	//__BANK

#if __BANK
public:
	Vec3V_Out GetPos() const { return m_vPos; }
	bool WasStartedByScript() const { return m_bOwnedByScript; }
#endif	//__BANK

	//////////////////////////////////////////////////////////////////////////

};

extern CLoadScene g_LoadScene;

#endif	//_SCENE_LOADSCENE_H_