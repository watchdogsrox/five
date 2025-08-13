/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/streamer/StreamVolume.cpp
// PURPOSE : interface for pinning and requesting a specific volume of map
// AUTHOR :  Ian Kiigan
// CREATED : 23/08/11
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/streamer/StreamVolume.h"

SCENE_OPTIMISATIONS();

#include "fwscene/search/SearchVolumes.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "scene/Entity.h"
#include "scene/world/GameScan.h"
#include "scene/world/GameWorld.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "script/script.h"
#include "spatialdata/sphere.h"

//////////////////////////////////////////////////////////////////////////
#if __BANK
#include "bank/bank.h" 
#include "bank/bkmgr.h"
#include "camera/CamInterface.h"
#include "grcore/debugdraw.h"
#include "modelinfo/BaseModelInfo.h"
#include "modelinfo/ModelInfo.h"

#include "scene/portals/FrustumDebug.h"
#include "camera/helpers/Frame.h"

const char* apszVolumeSlots[] =
{
	"VOLUME_SLOT_PRIMARY",
	"VOLUME_SLOT_SECONDARY"
};

bool CStreamVolumeMgr::ms_abLodLevels[LODTYPES_DEPTH_TOTAL];
s32 CStreamVolumeMgr::ms_nDbgSlot = 0;
bool CStreamVolumeMgr::ms_bShowActiveVolumes = false;
bool CStreamVolumeMgr::ms_bShowUnloadedEntities = false;
s32 CStreamVolumeMgr::ms_radius = 50;
bool CStreamVolumeMgr::ms_bStaticBoundsMover = false;
bool CStreamVolumeMgr::ms_bStaticBoundsWeapon = false;
bool CStreamVolumeMgr::ms_bMapDataHigh = false;
bool CStreamVolumeMgr::ms_bMapDataHighCritical = false;
bool CStreamVolumeMgr::ms_bMapDataMedium = false;
bool CStreamVolumeMgr::ms_bMapDataLow = false;
bool CStreamVolumeMgr::ms_bMapDataAll = false;
bool CStreamVolumeMgr::ms_bUseSceneStreaming = false;
bool CStreamVolumeMgr::ms_bPrintEntities = false;
Vec3V CStreamVolumeMgr::ms_vDebugLinePos1;
Vec3V CStreamVolumeMgr::ms_vDebugLinePos2;
bool CStreamVolumeMgr::ms_bMinimalVolume = false;
#endif	//__BANK
//////////////////////////////////////////////////////////////////////////

CStreamVolume CStreamVolumeMgr::m_volumes[VOLUME_SLOT_TOTAL];

#if !__FINAL
atArray<strIndex> CStreamVolumeMgr::ms_streamingIndices;	// used for debug memory budgets
#endif	//!__FINAL

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitAsSphere
// PURPOSE:		sets up spherical stream volume and asset store dependencies
//////////////////////////////////////////////////////////////////////////
void CStreamVolume::InitAsSphere(const spdSphere& sceneVol, fwBoxStreamerAssetFlags assetFlags, bool bSceneStreamingEnabled, bool bMinimal, bool bMinimalMoverCollision)
{
	m_volumeType = TYPE_SPHERE;
	m_sphere = sceneVol;
	m_mapDataSphere = sceneVol;
	m_staticBoundsSphere = bMinimalMoverCollision ? spdSphere(sceneVol.GetCenter(), ScalarV(10.0f)) : sceneVol;

	InitCommon(assetFlags, bSceneStreamingEnabled, bMinimal);	
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitAsFrustum
// PURPOSE:		sets up frustum stream volume and asset store dependencies
//////////////////////////////////////////////////////////////////////////
void CStreamVolume::InitAsFrustum(Vec3V_In vPos, Vec3V_In vDir, float fFarClip, fwBoxStreamerAssetFlags assetFlags, bool bSceneStreamingEnabled, bool bMinimal)
{
	m_volumeType = TYPE_FRUSTUM;
	m_sphere = spdSphere(vPos, ScalarV(V_ONE));		// use a 1m sphere for collisions etc
	m_mapDataSphere = spdSphere(vPos, ScalarV(fFarClip));
	m_staticBoundsSphere = spdSphere(vPos, ScalarV(10.0f));
	m_vFrustumDir = vDir;
	m_fFarClip = fFarClip;

	InitCommon(assetFlags, bSceneStreamingEnabled, bMinimal);	
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitAsLine
// PURPOSE:		sets up line seg streaming volume and asset store dependencies
//////////////////////////////////////////////////////////////////////////
void CStreamVolume::InitAsLine(Vec3V_In vPos1, Vec3V_In vPos2, fwBoxStreamerAssetFlags assetFlags, bool bSceneStreamingEnabled, float fMaxBoxStreamerZ)
{
	m_volumeType = TYPE_LINE;
	m_vLinePos1 = vPos1;
	m_vLinePos2 = vPos2;
	m_vBoxStreamerLinePos1 = vPos1;
	m_vBoxStreamerLinePos2 = vPos2;

	if (vPos1.GetZf() > fMaxBoxStreamerZ)
	{
		m_vBoxStreamerLinePos1.SetZf(fMaxBoxStreamerZ);
	}
	if (vPos2.GetZf() > fMaxBoxStreamerZ)
	{
		m_vBoxStreamerLinePos2.SetZf(fMaxBoxStreamerZ);
	}
	
	m_sphere = spdSphere(vPos1, ScalarV(V_ZERO));
	m_sphere.GrowPoint(vPos2);
	m_mapDataSphere = m_sphere;
	m_staticBoundsSphere = m_sphere;

	InitCommon(assetFlags, bSceneStreamingEnabled, false);
}

bool CStreamVolume::ms_disableHeist4Hack = false;
void CStreamVolume::InitTunables()
{
	ms_disableHeist4Hack = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("HEIST4_DISABLE_STREAM_VOLUME_HACK", 0x71fae26f), ms_disableHeist4Hack);
}
//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitCommon
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CStreamVolume::InitCommon(fwBoxStreamerAssetFlags assetFlags, bool bSceneStreamingEnabled, bool bMinimal)
{
	m_lodMask = LODTYPES_MASK_ALL;

	m_bUsesCameraMatrix = false;
	m_assetFlags = assetFlags;
	m_bSceneStreamingEnabled = bSceneStreamingEnabled;
	m_bCreatedByScript = false;
	m_reqdImaps.Reset();
	m_reqdStaticBounds.Reset();
	m_bAllEntitiesLoaded = false;
	m_bProcessedByVisibility = false;
	m_bMinimalMapStreaming = bMinimal;

	if (!m_bMinimalMapStreaming)
	{
		m_mapDataSphere.SetRadiusf(1.0f);	// if not minimal volume, request map data based on a point, but respect pre-stream and fade distances
	}

	// search asset stores for dependencies
	if ((assetFlags & fwBoxStreamerAsset::MASK_MAPDATA) != 0)
	{
		if (IsTypeLine())
		{
			fwMapDataStore::GetStore().GetBoxStreamer().GetIntersectingLine(m_vBoxStreamerLinePos1, m_vBoxStreamerLinePos2, assetFlags, m_reqdImaps, true);
		}
		else
		{
			fwMapDataStore::GetStore().GetBoxStreamer().GetIntersectingAABB(spdAABB(m_mapDataSphere), assetFlags, m_reqdImaps, !m_bMinimalMapStreaming);
		}
	}
	if ((assetFlags & (fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER|fwBoxStreamerAsset::FLAG_STATICBOUNDS_WEAPONS)) != 0)
	{
		if (IsTypeLine())
		{
			g_StaticBoundsStore.GetBoxStreamer().GetIntersectingLine(m_vLinePos1, m_vLinePos2, assetFlags, m_reqdStaticBounds, false);
		}
		else
		{
			g_StaticBoundsStore.GetBoxStreamer().GetIntersectingAABB(spdAABB(m_staticBoundsSphere), assetFlags, m_reqdStaticBounds, false);
		}
	}

	// set pinned flag for asset stores
	// HACK for url:bugstar:6841812 - Filter the results in the heist island so we're not pinning the main island imaps
	if(CIslandHopper::GetInstance().IsAnyIslandActive() && !ms_disableHeist4Hack)
	{
		if (!IsTypeLine())
		{
			CIplCullBox::RemoveCulledAtPosition(m_mapDataSphere.GetCenter(), m_reqdImaps);
		}
	}
	
	for (s32 i=0; i<m_reqdImaps.GetCount(); i++)
	{
		strLocalIndex slotIndex = strLocalIndex(m_reqdImaps[i]);
		fwMapDataDef* pDef = g_MapDataStore.GetSlot(slotIndex);
		if (pDef)
		{
			pDef->SetIsPinned(true);
		}
		fwMapDataStore::GetStore().GetBoxStreamer().SetIsPinnedByVolume(slotIndex.Get(), true);
	}

	SetActive(true);

#if STREAMING_VISUALIZE
	const char *origin = CTheScripts::GetCurrentScriptNameAndProgramCounter();
	if (!origin || origin[0] == ' ' || origin[0] == 0)
	{
		origin = "Code";
	}

	const char *type = (IsTypeLine()) ? "Line" : (IsTypeFrustum()) ? "Frustum" : "Sphere";

	STRVIS_ADD_MARKER(strStreamingVisualize::MARKER);
	Vec3V pos = GetPos();
	STRVIS_SET_MARKER_TEXT("New %s%s Streaming volume from %s at %.2f %.2f %.2f", (bMinimal) ? "minimal ": "", type, origin, pos.GetXf(), pos.GetYf(), pos.GetZf());
	STRVIS_ADD_CALLSTACK_TO_MARKER();
#endif // STREAMING_VISUALIZE

}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Shutdown
// PURPOSE:		clears dependencies on asset stores etc
//////////////////////////////////////////////////////////////////////////
void CStreamVolume::Shutdown()
{
	// clear pinned flags for asset stores
	for (s32 i=0; i<m_reqdImaps.GetCount(); i++)
	{
 		strLocalIndex slotIndex = strLocalIndex(m_reqdImaps[i]);

		fwMapDataDef* pDef = g_MapDataStore.GetSlot( slotIndex );
		if (pDef)
		{
			pDef->SetIsPinned(false);
		}

		fwMapDataStore::GetStore().GetBoxStreamer().SetIsPinnedByVolume(slotIndex.Get(), false);
	}

	m_reqdImaps.Reset();
	m_reqdStaticBounds.Reset();

	SetActive(false);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetScriptInfo
// PURPOSE:		stores script thread id etc for cleanup purposes
//////////////////////////////////////////////////////////////////////////
void CStreamVolume::SetScriptInfo(scrThreadId scriptThreadId, const char* pszName)
{
	m_bCreatedByScript = true;
	m_scriptThreadId = scriptThreadId;
	m_scriptName = pszName;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	HasLoaded
// PURPOSE:		returns true if all requirements are met, false otherwise
//////////////////////////////////////////////////////////////////////////
bool CStreamVolume::HasLoaded() const
{
	if (!IsActive()) { return false; }

	// check all static bounds are loaded
	for (s32 i=0; i<m_reqdStaticBounds.GetCount(); i++)
	{
		strLocalIndex slotIndex = strLocalIndex(m_reqdStaticBounds[i]);
		if (!g_StaticBoundsStore.GetPtr(slotIndex))
		{
			return false;
		}
	}

	// check all map files are loaded
	for (s32 i=0; i<m_reqdImaps.GetCount(); i++)
	{
		strLocalIndex slotIndex = strLocalIndex(m_reqdImaps[i]);
		if (!fwMapDataStore::GetStore().GetPtr(slotIndex))
		{
			return false;
		}
	}
	
	// check all entities are loaded
	if (m_bSceneStreamingEnabled)
	{
		if (m_bProcessedByVisibility)
		{
			return m_bAllEntitiesLoaded;
		}
		else
		{
			return false;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CheckForValidResults
// PURPOSE:		called by visibility code init, to indicate whether or not
//				this frame's PVS results will be potentially complete (for
//				purposes of checking if the volume is fully loaded)
//////////////////////////////////////////////////////////////////////////
void CStreamVolume::CheckForValidResults()
{
	if (!m_bSceneStreamingEnabled)
		return;

	// check all map files are loaded
	bool bAllImapsLoaded = true;
	for (s32 i=0; i<m_reqdImaps.GetCount(); i++)
	{
		strLocalIndex slotIndex = strLocalIndex(m_reqdImaps[i]);
		if (!fwMapDataStore::GetStore().GetPtr(slotIndex))
		{
			bAllImapsLoaded = false;
			break;
		}
	}
	SetProcessedByVisibility(bAllImapsLoaded);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ExtractEntitiesFromMapData
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CStreamVolume::ExtractEntitiesFromMapData(atArray<CEntity*>& slodList)
{
	Assertf(IsTypeLine(), "Only used for line seg streaming!");

	fwMapDataStore& mapStore = fwMapDataStore::GetStore();
	for (s32 i=0; i<m_reqdImaps.GetCount(); i++)
	{
		strLocalIndex slotIndex = strLocalIndex(m_reqdImaps[i]);
		fwMapDataDef* pDef = mapStore.GetSlot(slotIndex);
		Assert(pDef);

		if (pDef->IsLoaded())
		{
			fwEntity** entities = pDef->GetEntities();
			u32 numEntities = pDef->GetNumEntities();

			for (u32 j=0; j<numEntities; j++)
			{
				CEntity* pEntity = (CEntity*) entities[j];

				// if valid entity, and required lod level, and intersects with swept frustum
				if ( pEntity && ((m_lodMask & pEntity->GetLodData().GetLodFlag())!=0) && m_lineSegHelper.IntersectsAABB(pEntity->GetBoundBox()) )
				{
					slodList.PushAndGrow(pEntity);
				}
			}
		}
	}
}

#if __BANK

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DebugDraw
// PURPOSE:		draw debug info about volume
//////////////////////////////////////////////////////////////////////////
void CStreamVolume::DebugDraw()
{
	const Vec3V vPos = GetPos();

	switch (m_volumeType)
	{
	case TYPE_SPHERE:
		{	
			grcDebugDraw::Sphere(m_sphere.GetCenter(), m_sphere.GetRadiusf(), Color32(255,0,0), false, 1, 64);
			grcDebugDraw::AddDebugOutputEx(
				false, Color32(255, 255, 255),
				"StreamVol (sphere) pos=(%4.2f, %4.2f, %4.2f), radius=%0.0f",
				vPos.GetXf(), vPos.GetYf(), vPos.GetZf(), m_sphere.GetRadiusf()
			);
		}
		
		break;
	case TYPE_FRUSTUM:
		{
			Matrix34 matFrustum;
			camFrame::ComputeWorldMatrixFromFront(VEC3V_TO_VECTOR3(m_vFrustumDir), matFrustum);
			matFrustum.d = VEC3V_TO_VECTOR3(vPos);

			CFrustumDebug dbgFrustum(matFrustum, 0.50f, m_fFarClip, 50.0f, 1.0f, 1.0f);
			dbgFrustum.DebugRender(&dbgFrustum, Color32(64, 128, 255, 80));

			grcDebugDraw::AddDebugOutputEx(
				false, Color32(255, 255, 255),
				"StreamVol (frustum) pos=(%4.2f, %4.2f, %4.2f), dir=(%4.2f, %4.2f, %4.2f), farClip=%4.2f",
				vPos.GetXf(), vPos.GetYf(), vPos.GetZf(), m_vFrustumDir.GetXf(), m_vFrustumDir.GetYf(), m_vFrustumDir.GetZf(), m_fFarClip
			);
		}
		break;
	case TYPE_LINE:
		grcDebugDraw::Sphere(m_sphere.GetCenter(), m_sphere.GetRadiusf(), Color32(0,255,0), false, 1, 64);
		grcDebugDraw::Line(VEC3V_TO_VECTOR3(m_vLinePos1), VEC3V_TO_VECTOR3(m_vLinePos2), Color32(255,0,0), 1);
		grcDebugDraw::AddDebugOutputEx(
			false, Color32(255, 255, 255),
			"StreamVol (line) pos1=(%4.2f, %4.2f, %4.2f), pos2=(%4.2f, %4.2f, %4.2f)",
			m_vLinePos1.GetXf(), m_vLinePos1.GetYf(), m_vLinePos1.GetZf(),
			m_vLinePos2.GetXf(), m_vLinePos2.GetYf(), m_vLinePos2.GetZf()
		);
		break;

	default:
		break;
	}

	grcDebugDraw::AddDebugOutputEx(
		false, Color32(255,255,255),
		"staticbnd(m)=%s, staticbnd(w)=%s, mapdatahi=%s, mapdatahicrit=%s, mapdatamed=%s mapdatalo=%s scenestream=%s",
		((m_assetFlags & fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER)!=0) ? "Y" : "N",
		((m_assetFlags & fwBoxStreamerAsset::FLAG_STATICBOUNDS_WEAPONS)!=0) ? "Y" : "N",
		((m_assetFlags & fwBoxStreamerAsset::FLAG_MAPDATA_HIGH)!=0) ? "Y" : "N",
		((m_assetFlags & fwBoxStreamerAsset::FLAG_MAPDATA_HIGH_CRITICAL)!=0) ? "Y" : "N",
		((m_assetFlags & fwBoxStreamerAsset::FLAG_MAPDATA_MEDIUM)!=0) ? "Y" : "N",
		((m_assetFlags & fwBoxStreamerAsset::FLAG_MAPDATA_LOW)!=0) ? "Y" : "N",
		(UsesSceneStreaming() ? "Y" : "N")
	);

	if (UsesSceneStreaming())
	{
		for (u32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
		{
			if (RequiresLodLevel(i))
			{
				grcDebugDraw::AddDebugOutputEx(	false, Color32(255, 255, 255), "... requires LOD level: %s", fwLodData::ms_apszLevelNames[i] );
			}
		}
	}
}

#endif	//__BANK

#if !__NO_OUTPUT

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetDebugOwner
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
const char* CStreamVolume::GetDebugOwner()
{
	if (IsActive())
	{
		if (WasCreatedByScript())
		{
			return GetOwningScriptName();
		}
		return "CODE";
	}
	return "NONE";
}

#endif	//__NO_OUTPUT

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RegisterSphere
// PURPOSE:		register a new spherical scene volume of interest
//////////////////////////////////////////////////////////////////////////
void CStreamVolumeMgr::RegisterSphere(const spdSphere& sceneVol, fwBoxStreamerAssetFlags assetFlags, bool bSceneStreamingEnabled, s32 volumeSlot, bool bMinimal, bool bMinimalMoverCollision)
{
	CStreamVolume& volume = GetVolume(volumeSlot);

#if !__FINAL
	if ( !Verifyf(!volume.IsActive(), "Volume already in use! Deleting existing volume (owned by: %s) to create new one", volume.GetDebugOwner()) )
#else
	if ( volume.IsActive() )
#endif
	{
		Deregister(volumeSlot);
	}
	volume.InitAsSphere(sceneVol, assetFlags, bSceneStreamingEnabled, bMinimal, bMinimalMoverCollision);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RegisterFrustum
// PURPOSE:		register a new frustum scene volume of interest
//////////////////////////////////////////////////////////////////////////
void CStreamVolumeMgr::RegisterFrustum(Vec3V_In vPos, Vec3V_In vDir, float fFarClip, fwBoxStreamerAssetFlags assetFlags, bool bSceneStreamingEnabled, s32 volumeSlot, bool bMinimal)
{
	CStreamVolume& volume = GetVolume(volumeSlot);
	if ( !Verifyf(!volume.IsActive(), "Volume already in use! Deleting existing volume (owned by: %s) to create new one", volume.GetDebugOwner()) )
	{
		Deregister(volumeSlot);
	}
	volume.InitAsFrustum(vPos, vDir, fFarClip, assetFlags, bSceneStreamingEnabled, bMinimal);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RegisterLine
// PURPOSE:		register a special case line seg streaming volume used for player switching etc
//////////////////////////////////////////////////////////////////////////
void CStreamVolumeMgr::RegisterLine(Vec3V_In vPos1, Vec3V_In vPos2, fwBoxStreamerAssetFlags assetFlags, bool bSceneStreamingEnabled, s32 volumeSlot, float fMaxBoxStreamerZ/*=FLT_MAX*/)
{
	CStreamVolume& volume = GetVolume(volumeSlot);
	if ( !Verifyf(!volume.IsActive(), "Volume already in use! Deleting existing volume (owned by: %s) to create new one", volume.GetDebugOwner()) )
	{
		Deregister(volumeSlot);
	}
	volume.InitAsLine(vPos1, vPos2, assetFlags, bSceneStreamingEnabled, fMaxBoxStreamerZ);
}

//////////////////////////////////////////////////////////////////////////
// PURPOSE:		Deregister
// PURPOSE:		shuts down volume
//////////////////////////////////////////////////////////////////////////
void CStreamVolumeMgr::Deregister(s32 volumeSlot)
{
	CStreamVolume& volume = GetVolume(volumeSlot);

	if (volume.IsActive())
	{
		volume.Shutdown();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DeregisterForScript
// PURPOSE:		shut down any active volumes for specified script
void CStreamVolumeMgr::DeregisterForScript(scrThreadId scriptThreadId)
{
	CStreamVolume& volume = GetVolume(VOLUME_SLOT_PRIMARY);

	if (volume.IsActive() && volume.IsOwnedByScript(scriptThreadId))
	{
		Deregister( VOLUME_SLOT_PRIMARY );
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddSearches
// PURPOSE:		append box streamer search for active volumes
//////////////////////////////////////////////////////////////////////////
void CStreamVolumeMgr::AddSearches(atArray<fwBoxStreamerSearch>& searchList, fwBoxStreamerAssetFlags supportedAssetFlags)
{
	for (s32 i=0; i<VOLUME_SLOT_TOTAL; i++)
	{
		CStreamVolume& volume = GetVolume(i);

		//////////////////////////////////////////////////////////////////////////
		if (volume.IsActive())
		{
			bool bIsMapData = ( supportedAssetFlags & fwBoxStreamerAsset::MASK_MAPDATA ) != 0;
			bool bRequiresPreStream = ( bIsMapData || volume.IsTypeFrustum() ) && !volume.m_bMinimalMapStreaming;

			if (volume.IsTypeLine())
			{
				searchList.Grow() = fwBoxStreamerSearch(
					volume.m_vBoxStreamerLinePos1, volume.m_vBoxStreamerLinePos2,
					fwBoxStreamerSearch::TYPE_STREAMINGVOLUME,
					volume.GetAssetFlags(),
					bRequiresPreStream
				);
			}
			else
			{
				searchList.Grow() = fwBoxStreamerSearch(
					( bIsMapData  ? volume.GetMapDataSphere() : volume.GetStaticBoundsSphere() ),
					fwBoxStreamerSearch::TYPE_STREAMINGVOLUME,
					volume.GetAssetFlags(),
					bRequiresPreStream
				);
			}
		}
		//////////////////////////////////////////////////////////////////////////
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateLoadedState
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CStreamVolume::UpdateLoadedState(s32 volumeSlot)
{
	u32 streamingVolumeVisMask = 0;

	switch (volumeSlot)
	{
	case CStreamVolumeMgr::VOLUME_SLOT_PRIMARY:
		streamingVolumeVisMask = ( 1 << CGameScan::GetStreamVolumeVisibilityBit1() );
		break;
	case CStreamVolumeMgr::VOLUME_SLOT_SECONDARY:
		streamingVolumeVisMask = ( 1 << CGameScan::GetStreamVolumeVisibilityBit2() );
		break;
	default:
		Assert(0);
		break;
	}

	if ( IsTypeLine() )
	{
		//////////////////////////////////////////////////////////////////////////
		atArray<CEntity*> volumeEntities;
		ExtractEntitiesFromMapData(volumeEntities);

		for (s32 i=0; i<volumeEntities.GetCount(); i++)
		{
			CEntity* pEntity = volumeEntities[i];
			if (pEntity)
			{
				CBaseModelInfo* pMI = pEntity->GetBaseModelInfo();
				if (pMI 
					&& !pMI->IsModelMissing() 
					&& !pMI->GetHasLoaded()
					&& pMI->GetDrawableType()!=fwArchetype::DT_ASSETLESS
					&& pMI->GetDrawableType()!=fwArchetype::DT_UNINITIALIZED)
				{
					SetEntitiesLoaded(false);
					return;
				}
			}
		}
		// if we get this far, all the required entities must be loaded
		SetEntitiesLoaded(true);
		return;
		//////////////////////////////////////////////////////////////////////////
	}
	else
	{
		//////////////////////////////////////////////////////////////////////////
		CPostScan::WaitForSortPVSDependency();
		CGtaPvsEntry* pPvsEntry = gPostScan.GetPVSBase();
		CGtaPvsEntry* pPvsStop = gPostScan.GetPVSEnd();
		while (pPvsEntry != pPvsStop)
		{
			if (pPvsEntry->GetVisFlags() & streamingVolumeVisMask)
			{
				CEntity* pEntity = pPvsEntry->GetEntity();
				if (pEntity)
				{
					CBaseModelInfo* pMI = pEntity->GetBaseModelInfo();
					if (pMI 
						&& !pMI->IsModelMissing() 
						&& !pMI->GetHasLoaded()
						&& pMI->GetDrawableType()!=fwArchetype::DT_ASSETLESS
						&& pMI->GetDrawableType()!=fwArchetype::DT_UNINITIALIZED)
					{
						SetEntitiesLoaded(false);
						return;
					}
				}
			}
			pPvsEntry++;
		}
		// if we get this far, all the required entities must be loaded
		SetEntitiesLoaded(true);
		return;
		//////////////////////////////////////////////////////////////////////////
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateVolumesLoadedState
// PURPOSE:		update whether or not a volume that uses scene streaming has been loaded
//////////////////////////////////////////////////////////////////////////
void CStreamVolumeMgr::UpdateVolumesLoadedState()
{
	for (s32 i=0; i<VOLUME_SLOT_TOTAL; i++)
	{
		CStreamVolume& volume = GetVolume(i);

		if ( volume.IsActive() && volume.UsesSceneStreaming() && volume.HasBeenProcessedByVisibility() )
		{
			volume.UpdateLoadedState(i);

#if __BANK && 0
			//////////////////////////////////////////////////////////////////////////
			if (ms_bPrintEntities && ms_nDbgSlot==i)
			{
				atArray<CEntity*> volumeEntities;
				ExtractVolumeEntities(i, pPvsStart, pPvsStop, volumeEntities);
				volumeEntities.QSort(0, -1, SortByAnythingCB);

				for (s32 i=0; i<volumeEntities.GetCount(); i++)
				{
					CEntity* pEntity = volumeEntities[i];
					if (pEntity)
					{
						CBaseModelInfo* pMI = pEntity->GetBaseModelInfo();
						if (pMI)
						{
							grcDebugDraw::AddDebugOutput("Volume Entity %s: Loaded:%s Drawable:%s Alpha:%d",
								pMI->GetModelName(), pMI->GetHasLoaded()?"Y":"N", pEntity->GetDrawHandlerPtr()?"Y":"N", pEntity->GetAlpha());
						}

					}
				}
			}
			//////////////////////////////////////////////////////////////////////////
#endif	//__BANK
		}
	}

#if !__FINAL
	UpdateScriptMemUse();
#endif	//!__FINAL

}

#if !__FINAL

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateScriptMemUse
// PURPOSE:		updates array of streaming indices used for tracking script mem use
//////////////////////////////////////////////////////////////////////////
void CStreamVolumeMgr::UpdateScriptMemUse()
{
	CStreamVolume& volume = GetVolume(VOLUME_SLOT_PRIMARY);
	if ( volume.IsActive() && volume.UsesSceneStreaming() && volume.WasCreatedByScript() )
	{
		const u32	streamingVolumeVisMask = ( 1 << CGameScan::GetStreamVolumeVisibilityBit1() );

		ms_streamingIndices.Reset();

		CPostScan::WaitForSortPVSDependency();
		CGtaPvsEntry* pPvsEntry = gPostScan.GetPVSBase();
		CGtaPvsEntry* pPvsStop = gPostScan.GetPVSEnd();

		while (pPvsEntry != pPvsStop)
		{
			if (pPvsEntry->GetVisFlags() & streamingVolumeVisMask)
			{
				CEntity* pEntity = pPvsEntry->GetEntity();
				if (pEntity)
				{
					strLocalIndex localIndex = CModelInfo::LookupLocalIndex(pEntity->GetModelId());
					ms_streamingIndices.Grow() = CModelInfo::GetStreamingModule()->GetStreamingIndex(localIndex);
				}
			}
			pPvsEntry++;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetStreamingIndices
// PURPOSE:		appends streaming indices required by streaming volume, if active
//////////////////////////////////////////////////////////////////////////
int CStreamVolumeMgr::GetStreamingIndicesCount()
{
	int count = 0;
	CStreamVolume& volume = GetVolume(VOLUME_SLOT_PRIMARY);
	
	if (volume.IsActive() && volume.WasCreatedByScript())
	{
		count += volume.m_reqdStaticBounds.GetCount() + volume.m_reqdImaps.GetCount();
		if (volume.UsesSceneStreaming())
			count += ms_streamingIndices.GetCount();

		return count;
	}

	return count;
}

void CStreamVolumeMgr::GetStreamingIndices(atArray<strIndex>& streamingIndices)
{
	CStreamVolume& volume = GetVolume(VOLUME_SLOT_PRIMARY);

	if (!volume.IsActive())
		return;

	if (!volume.WasCreatedByScript())
		return;

	// static bounds and map data
	for (s32 i=0; i<volume.m_reqdStaticBounds.GetCount(); i++)
	{
		streamingIndices.Grow() = g_StaticBoundsStore.GetStreamingIndex( strLocalIndex((s32) volume.m_reqdStaticBounds[i]) );
	}
	for (s32 i=0; i<volume.m_reqdImaps.GetCount(); i++)
	{
		streamingIndices.Grow() = fwMapDataStore::GetStore().GetStreamingIndex( strLocalIndex((s32) volume.m_reqdImaps[i]) );
	}

	// entities
	if (volume.UsesSceneStreaming())
	{
		for (s32 i=0; i<ms_streamingIndices.GetCount(); i++)
		{
			streamingIndices.Grow() = ms_streamingIndices[i];
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsThereAVolumeOwnedBy
// PURPOSE:		returns true if there is an active streaming volume owned by the
//				specified thread id
//////////////////////////////////////////////////////////////////////////
bool CStreamVolumeMgr::IsThereAVolumeOwnedBy(scrThreadId scriptThreadId)
{
	CStreamVolume& volume = GetVolume(VOLUME_SLOT_PRIMARY);
	return volume.IsActive() && volume.IsOwnedByScript(scriptThreadId);
}

#endif	//!__FINAL

#if __BANK

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DebugRegisterSphereVolume
// PURPOSE:		creates and registers a stream volume around camera position
//////////////////////////////////////////////////////////////////////////
void CStreamVolumeMgr::DebugRegisterSphereVolume()
{
	Vector3 vCamPos = camInterface::GetPos();
	spdSphere newSphere(RCC_VEC3V(vCamPos), ScalarV((float)ms_radius));
	fwBoxStreamerAssetFlags assetFlags = 0;
	assetFlags |= (ms_bStaticBoundsMover * fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER);
	assetFlags |= (ms_bStaticBoundsWeapon * fwBoxStreamerAsset::FLAG_STATICBOUNDS_WEAPONS);
	assetFlags |= (ms_bMapDataHigh * fwBoxStreamerAsset::FLAG_MAPDATA_HIGH);
	assetFlags |= (ms_bMapDataHighCritical * fwBoxStreamerAsset::FLAG_MAPDATA_HIGH_CRITICAL);
	assetFlags |= (ms_bMapDataMedium * fwBoxStreamerAsset::FLAG_MAPDATA_MEDIUM);
	assetFlags |= (ms_bMapDataLow * fwBoxStreamerAsset::FLAG_MAPDATA_LOW);
	assetFlags |= (ms_bMapDataAll * fwBoxStreamerAsset::MASK_MAPDATA);

	RegisterSphere(newSphere, assetFlags, ms_bUseSceneStreaming, ms_nDbgSlot, ms_bMinimalVolume);

	CStreamVolume& volume = GetVolume(ms_nDbgSlot);
	volume.SetLodMask( DebugGetLodMask() );
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DebugRegisterFrustumVolume
// PURPOSE:		creates and registers a stream volume around camera position
//////////////////////////////////////////////////////////////////////////
void CStreamVolumeMgr::DebugRegisterFrustumVolume()
{
	Vector3 vCamPos = camInterface::GetPos();
	Vector3 vCamDir = camInterface::GetFront();
	fwBoxStreamerAssetFlags assetFlags = 0;
	assetFlags |= (ms_bStaticBoundsMover * fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER);
	assetFlags |= (ms_bStaticBoundsWeapon * fwBoxStreamerAsset::FLAG_STATICBOUNDS_WEAPONS);
	assetFlags |= (ms_bMapDataHigh * fwBoxStreamerAsset::FLAG_MAPDATA_HIGH);
	assetFlags |= (ms_bMapDataHighCritical * fwBoxStreamerAsset::FLAG_MAPDATA_HIGH_CRITICAL);
	assetFlags |= (ms_bMapDataMedium * fwBoxStreamerAsset::FLAG_MAPDATA_MEDIUM);
	assetFlags |= (ms_bMapDataLow * fwBoxStreamerAsset::FLAG_MAPDATA_LOW);
	assetFlags |= (ms_bMapDataAll * fwBoxStreamerAsset::MASK_MAPDATA);

	RegisterFrustum(RCC_VEC3V(vCamPos), RCC_VEC3V(vCamDir), (float)ms_radius, assetFlags, ms_bUseSceneStreaming, ms_nDbgSlot, ms_bMinimalVolume);

	CStreamVolume& volume = GetVolume(ms_nDbgSlot);
	volume.SetLodMask( DebugGetLodMask() );
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DebugRegisterLineVolume
// PURPOSE:		creates and registers a  stream volume for line seg to camera pos
//////////////////////////////////////////////////////////////////////////
void CStreamVolumeMgr::DebugRegisterLineVolume()
{
	ms_vDebugLinePos2 = VECTOR3_TO_VEC3V( camInterface::GetPos() );
	fwBoxStreamerAssetFlags assetFlags = 0;
	assetFlags |= (ms_bStaticBoundsMover * fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER);
	assetFlags |= (ms_bStaticBoundsWeapon * fwBoxStreamerAsset::FLAG_STATICBOUNDS_WEAPONS);
	assetFlags |= (ms_bMapDataHigh * fwBoxStreamerAsset::FLAG_MAPDATA_HIGH);
	assetFlags |= (ms_bMapDataHighCritical * fwBoxStreamerAsset::FLAG_MAPDATA_HIGH_CRITICAL);
	assetFlags |= (ms_bMapDataMedium * fwBoxStreamerAsset::FLAG_MAPDATA_MEDIUM);
	assetFlags |= (ms_bMapDataLow * fwBoxStreamerAsset::FLAG_MAPDATA_LOW);
	assetFlags |= (ms_bMapDataAll * fwBoxStreamerAsset::MASK_MAPDATA);

	RegisterLine(ms_vDebugLinePos1, ms_vDebugLinePos2, assetFlags, ms_bUseSceneStreaming, ms_nDbgSlot);

	CStreamVolume& volume = GetVolume(ms_nDbgSlot);
	volume.SetLodMask( DebugGetLodMask() );
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DebugDeregisterVolume
// PURPOSE:		destroy running volume
//////////////////////////////////////////////////////////////////////////
void CStreamVolumeMgr::DebugDeregisterVolume()
{
	Deregister(ms_nDbgSlot);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DebugGetLodMask
// PURPOSE:		calculate lod mask based on the toggles for each lod level
//////////////////////////////////////////////////////////////////////////
u32 CStreamVolumeMgr::DebugGetLodMask()
{
	u32 retVal = 0;
	for (u32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
	{
		if (ms_abLodLevels[i])
		{
			retVal |= (1 << i);
		}
	}
	return retVal;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DebugSetPos1OfLine
// PURPOSE:		sets first pos of line segment to camera pos, for testing
//////////////////////////////////////////////////////////////////////////
void CStreamVolumeMgr::DebugSetPos1OfLine()
{
	ms_vDebugLinePos1 = VECTOR3_TO_VEC3V( camInterface::GetPos() );
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddWidgets
// PURPOSE:		adds useful debug widgets for testing reserved scene behaviour
//////////////////////////////////////////////////////////////////////////
void CStreamVolumeMgr::AddWidgets(bkBank* pBank)
{
	Assert(pBank);
	pBank->PushGroup("Stream Volumes");
	pBank->AddTitle("Stream Volumes");
	pBank->AddToggle("Show all scenes", &ms_bShowActiveVolumes);
	pBank->AddCombo("Volume slot", &ms_nDbgSlot, VOLUME_SLOT_TOTAL, &apszVolumeSlots[0]);
	pBank->AddToggle("Display entities", &ms_bPrintEntities);
	pBank->AddSlider("Range", &ms_radius, 1, 5000, 1);
	pBank->AddToggle("ASSET_STATICBOUNDS_MOVER", &ms_bStaticBoundsMover);
	pBank->AddToggle("ASSET_STATICBOUNDS_WEAPON", &ms_bStaticBoundsWeapon);
	pBank->AddToggle("ASSET_MAPDATA_HIGH", &ms_bMapDataHigh);
	pBank->AddToggle("ASSET_MAPDATA_HIGH_CRITICAL", &ms_bMapDataHighCritical);
	pBank->AddToggle("ASSET_MAPDATA_MEDIUM", &ms_bMapDataMedium);
	pBank->AddToggle("ASSET_MAPDATA_LOW", &ms_bMapDataLow);
	pBank->AddToggle("ASSET_MAPDATA_ALL", &ms_bMapDataAll);
	pBank->AddToggle("Include scene streaming", &ms_bUseSceneStreaming);
	pBank->AddToggle("Minimal map data", &ms_bMinimalVolume);
	pBank->AddButton("Set pos1 of line", DebugSetPos1OfLine);
	pBank->AddButton("Start sphere at camera", DebugRegisterSphereVolume);
	pBank->AddButton("Start frustum at camera", DebugRegisterFrustumVolume);
	pBank->AddButton("Start line to camera", DebugRegisterLineVolume);
	pBank->AddButton("Stop volume", DebugDeregisterVolume);
	
	// enable all lod levels by default
	for (s32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
	{
		pBank->AddToggle(fwLodData::ms_apszLevelNames[i], &ms_abLodLevels[i]);
		ms_abLodLevels[i] = true;
	}

	pBank->PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DebugDraw
// PURPOSE:		displays some debug text and debug draw for all active reserved scenes
//////////////////////////////////////////////////////////////////////////
void CStreamVolumeMgr::DebugDraw()
{
	if (ms_bShowActiveVolumes)
	{
		for (s32 i=0; i<VOLUME_SLOT_TOTAL; i++)
		{
			CStreamVolume& volume = GetVolume(i);
			if (volume.IsActive())
			{
				grcDebugDraw::AddDebugOutputEx(false, Color32(255,255,255), "Slot: %s", apszVolumeSlots[i]);
				volume.DebugDraw();

				grcDebugDraw::AddDebugOutputEx(false, Color32(255,255,0), "Created by %s", volume.GetDebugOwner());

				if (volume.HasLoaded())
				{
					grcDebugDraw::AddDebugOutputEx(false, Color32(0,255,0), "ALL LOADED");
				}
				else
				{
					grcDebugDraw::AddDebugOutputEx(false, Color32(255,0,0), "NOT LOADED");
				}
	
				//////////////////////////////////////////////////////////////////////////
				// print required asset store slots
				const atArray<u32>& requiredBounds = volume.GetRequiredStaticBounds();
				for (s32 j=0; j<requiredBounds.GetCount(); j++)
				{
					const strLocalIndex slotIndex = strLocalIndex(requiredBounds[j]);
					grcDebugDraw::AddDebugOutputEx(false, Color32(255,255,255),
						"... required bounds=%s slot=%d loaded=%s",
						g_StaticBoundsStore.GetName(slotIndex),
						slotIndex,
						( g_StaticBoundsStore.GetPtr(slotIndex) ? "Y" : "N" )
					);
				}

				const atArray<u32>& requiredImaps = volume.GetRequiredImaps();
				for (s32 j=0; j<requiredImaps.GetCount(); j++)
				{
					const strLocalIndex slotIndex = strLocalIndex(requiredImaps[j]);
					grcDebugDraw::AddDebugOutputEx(false, Color32(255,255,255),
						"... required IMAP=%s slot=%d loaded=%s",
						fwMapDataStore::GetStore().GetName(slotIndex),
						slotIndex,
						( fwMapDataStore::GetStore().GetPtr(slotIndex) ? "Y" : "N" )
					);
				}

				//////////////////////////////////////////////////////////////////////////
				// scene streaming info
#if 0
				if (volume.UsesSceneStreaming())
				{
					u32 numEntities;
					u32 numEntitiesWithAllDepsStreamedIn;
					u32 numEntitiesWithDrawables;
					volume.GetDebugEntityCounts(numEntities, numEntitiesWithDrawables, numEntitiesWithAllDepsStreamedIn);

					grcDebugDraw::AddDebugOutputEx(false, Color32(255,255,255),
						"... numEntities=%d, numEntitiesWithDrawables=%d, numEntitiesStreamedIn=%d",
						numEntities, numEntitiesWithDrawables, numEntitiesWithAllDepsStreamedIn
					);
				}

				if (ms_bShowUnloadedEntities)
				{
					volume.DrawDebugUnloadedEntities();
				}
#endif
				//////////////////////////////////////////////////////////////////////////
			}
		}
	}
}

#endif	//__BANK
