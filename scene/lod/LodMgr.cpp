/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/lod/LodMgr.cpp
// PURPOSE : Primary interface to lod tree systems
// AUTHOR :  Ian Kiigan
// CREATED : 05/03/10
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/lod/LodMgr.h"

#include "modelinfo/BaseModelInfo.h"
#include "modelInfo/ModelInfo.h"
#include "modelInfo/ModelInfo_Factories.h"
#include "renderer/Renderer.h"	// just for GetPreStreamDistance()
#include "scene/Entity.h"
#include "scene/ContinuityMgr.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "timecycle/TimeCycleConfig.h"
#include "fwscene/stores/boxstreamerassets.h"
#include "fwscene/Lod/LodAttach.h"
#if __BANK
#include "scene/lod/LodDebug.h"
#endif

SCENE_OPTIMISATIONS();

CLodMgr g_LodMgr;

// this table tells us which lod levels are stored in which type of .imap file
static u32 sMapDataAssetMasks[LODTYPES_DEPTH_TOTAL] =
{
	fwBoxStreamerAsset::MASK_MAPDATA_HIGH,		//LODTYPES_DEPTH_HD
	fwBoxStreamerAsset::FLAG_MAPDATA_MEDIUM,	//LODTYPES_DEPTH_LOD
	fwBoxStreamerAsset::FLAG_MAPDATA_MEDIUM,	//LODTYPES_DEPTH_SLOD1
	fwBoxStreamerAsset::FLAG_MAPDATA_LOW,		//LODTYPES_DEPTH_SLOD2
	fwBoxStreamerAsset::FLAG_MAPDATA_LOW,		//LODTYPES_DEPTH_SLOD3
	fwBoxStreamerAsset::MASK_MAPDATA_HIGH,		//LODTYPES_DEPTH_ORPHANHD,
	fwBoxStreamerAsset::FLAG_MAPDATA_LOW		//LODTYPES_DEPTH_SLOD4
};

u32 CLodMgr::GetMapDataAssetMaskForLodLevel(u32 lodLevel) const
{
	return sMapDataAssetMasks[lodLevel];
}

u32 CLodMgr::GetMapDataAssetMaskForLodMask(u32 lodLevelMask) const
{
	u32 assetFlags = 0;

	for (s32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
	{
		if ((lodLevelMask & (1 << i))!=0)
		{
			assetFlags |= sMapDataAssetMasks[i];
		}
	}
	return assetFlags;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CLodMgr
// PURPOSE:		Default constructor
//////////////////////////////////////////////////////////////////////////
CLodMgr::CLodMgr()
: m_bAllowTimeBasedFading(true), m_bScriptForceAllowTimeBasedFading(false)
{
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Reset
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CLodMgr::Reset()
{
	m_bScriptRequestHdOnly = false;

	m_slodMode.Reset();

	m_aFadeDists[LODTYPES_DEPTH_ORPHANHD] = g_visualSettings.Get("lod.fadedist.orphanhd", LODTYPES_FADE_DISTF);
	m_aFadeDists[LODTYPES_DEPTH_HD] = g_visualSettings.Get("lod.fadedist.hd", LODTYPES_FADE_DISTF);
	m_aFadeDists[LODTYPES_DEPTH_LOD] = g_visualSettings.Get("lod.fadedist.lod", LODTYPES_FADE_DISTF);
	m_aFadeDists[LODTYPES_DEPTH_SLOD1] = g_visualSettings.Get("lod.fadedist.slod1", LODTYPES_FADE_DISTF);
	m_aFadeDists[LODTYPES_DEPTH_SLOD2] = g_visualSettings.Get("lod.fadedist.slod2", LODTYPES_FADE_DISTF);
	m_aFadeDists[LODTYPES_DEPTH_SLOD3] = g_visualSettings.Get("lod.fadedist.slod3", LODTYPES_FADE_DISTF);
	m_aFadeDists[LODTYPES_DEPTH_SLOD4] = g_visualSettings.Get("lod.fadedist.slod4", LODTYPES_FADE_DISTF);

	g_LodScale.Init();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	PreUpdate
// PURPOSE:		This should be called before alpha update - sets the camera pos etc
//////////////////////////////////////////////////////////////////////////
void CLodMgr::PreUpdate(bool bMajorCameraTransitionOccurred)
{
	m_bAllowTimeBasedFading = !bMajorCameraTransitionOccurred;

	if (m_bScriptForceAllowTimeBasedFading)
	{
		if (!g_ContinuityMgr.HasLodScaleChangeOccurred())
		{
			m_bAllowTimeBasedFading = true;
		}
		
		m_bScriptForceAllowTimeBasedFading = false;
	}

#if __BANK
	if (m_bAllowTimeBasedFading)
	{
		m_bAllowTimeBasedFading = CLodDebugMisc::GetAllowTimeBasedFade();
	}
#endif	//__BANK

	DecideWhatLodLevelsCanDoTimeBasedFade();

	// update global lod dist scale
	m_vCameraPos = g_SceneToGBufferPhaseNew->GetCameraPositionForFade();

	// make sure script can't leave LOD filter enabled
	if (m_bScriptRequestHdOnly)
	{
		StartSlodMode(LODTYPES_FLAG_HD | LODTYPES_FLAG_ORPHANHD, LODFILTER_MODE_SCRIPT);
		m_bScriptRequestHdOnly = false;
	}
	else if (m_slodMode.IsActive() && m_filterMode==LODFILTER_MODE_SCRIPT)
	{
		StopSlodMode();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	WithinVisibleRange
// PURPOSE:		returns true if specified distance is within visible distance
//////////////////////////////////////////////////////////////////////////
bool CLodMgr::WithinVisibleRange(CEntity* pEntity, bool bIncludePreStream, float fadeDistMod)
{
	if (pEntity)
	{
		float fPreStreamDist = bIncludePreStream ? CRenderer::GetPreStreamDistance() : 0.0f;
		Vector3 vPos;
		GetBasisPivot(pEntity, vPos);
		const float fDist2 = (vPos-m_vCameraPos).Mag2();
		const float fTypeScale = g_LodScale.GetEntityTypeScale(pEntity->GetType());
		const float fLodDist = fTypeScale * pEntity->GetLodDistance() * g_LodScale.GetPerLodLevelScale(pEntity->GetLodData().GetLodType());
		float fTmp = (fLodDist + (LODTYPES_FADE_DIST * fadeDistMod) + fPreStreamDist);
		return fDist2 <= (fTmp*fTmp);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DecideWhatLodLevelsCanDoTimeBasedFade
// PURPOSE:		only specific levels of detail are permitted to fade up over time. based on player behaviour, decide which ones.
//////////////////////////////////////////////////////////////////////////
void CLodMgr::DecideWhatLodLevelsCanDoTimeBasedFade()
{
	m_lodFlagsForTimeBasedFade = 0;

	if (g_LodScale.GetRootScale()<=1.0f && g_ContinuityMgr.IsAboveHeightMap() && g_ContinuityMgr.IsFlyingAircraft())
	{
		// above height map, so all levels can fade over time
		m_lodFlagsForTimeBasedFade |= LODTYPES_MASK_ALL;
	}
	else
	{
		// not above height map, so only allow HD & LOD to fade over time
		// (or root LODs)
		m_lodFlagsForTimeBasedFade |= LODTYPES_FLAG_HD;
		m_lodFlagsForTimeBasedFade |= LODTYPES_FLAG_ORPHANHD;
		//m_lodFlagsForTimeBasedFade |= LODTYPES_FLAG_LOD;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	OverrideLodDist
// PURPOSE:		override lod distance for script entities etc
//////////////////////////////////////////////////////////////////////////
void CLodMgr::OverrideLodDist(CEntity* pEntity, u32 lodDist)
{
	if (pEntity && pEntity->GetLodData().IsOrphanHd())
	{
		pEntity->SetLodDistance(lodDist);
		pEntity->RequestUpdateInWorld();
	}
}

#if __BANK

bool CLodMgr::ms_abSlodModeLevels[LODTYPES_DEPTH_TOTAL];

void CLodMgr::StartSlodModeCb()
{
	u32 lodMask = 0;
	for (s32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
	{
		if (ms_abSlodModeLevels[i])
		{
			lodMask |= (1 << i);
		}
	}
	g_LodMgr.m_slodMode.Start(lodMask);
}

void CLodMgr::StopSlodModeCb()
{
	g_LodMgr.m_slodMode.Stop();
}


#endif	//__BANK
