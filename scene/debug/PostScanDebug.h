/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/debug/PostScanDebug.h
// PURPOSE : debug processing on PVS list after visibility scan
// AUTHOR :  Ian Kiigan
// CREATED : 30/07/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_DEBUG_POSTSCANDEBUG_H_
#define _SCENE_DEBUG_POSTSCANDEBUG_H_

#if __BANK

#include "fwscene/scan/VisibilityFlags.h"
#include "fwrenderer/renderlistbuilder.h"
#include "file/stream.h"

class CGtaPvsEntry;
class CRenderPhase;
class CEntity;

namespace rage {
	class bkBank;
}

class CPostScanDebugSimplePvsStats
{
public:

	void Reset()
	{
		m_numEntitiesInPvs_TOTAL = 0;
		m_numEntitiesVis_GBUF = 0;
		m_numEntitiesVis_SHADOWS = 0;
		m_numEntitiesVis_REFLECTIONS = 0;
		m_numEntitiesInPvs_SELECTED = 0;
		m_numShadersInPvs_TOTAL = 0;
		m_numShadersInPvs_SELECTED = 0;
		m_numShadersVis_GBUF = 0;
		m_numShadersVis_ALPHA = 0;
	}

	u32 m_numEntitiesInPvs_TOTAL;
	u32 m_numEntitiesInPvs_SELECTED;

	u32 m_numEntitiesVis_GBUF;
	u32 m_numEntitiesVis_SHADOWS;
	u32 m_numEntitiesVis_REFLECTIONS;	

	u32 m_numShadersInPvs_TOTAL;
	u32 m_numShadersInPvs_SELECTED;
	u32 m_numShadersVis_GBUF;
	u32 m_numShadersVis_ALPHA;
};

class CPostScanDebug
{
public:
	static bool		GetCountModelsRendered();
	static void		SetCountModelsRendered(bool bEnable, int drawList = -1, bool bShowStreaming = false);
	static void		AddWidgets(bkBank* pBank);
	static void		Update();
	static void		Draw();
	static void		ForceUpAlphas();
	static void		ProcessDebug1(CGtaPvsEntry* pPvsStart, CGtaPvsEntry* pPvsStop);
	static void		ProcessDebug2(CGtaPvsEntry* pPvsStart, CGtaPvsEntry* pPvsStop);
	static bool		DoAlphaUpdate() { return ms_bDoAlphaUpdate; }
	static bool&	GetCountModelsRenderedRef() { return ms_bCountModelsRendered; }
	static bool&	GetCountShadersRenderedRef() { return ms_bCountShadersRendered; }

	static bool		ShouldPartitionPvsInsteadOfSorting() { return ms_PartitionPvsInsteadOfSorting; }
	static bool		ShouldVerifyPvsSort() { return ms_VerifyPvsSort; }

	static void		SetPvsStatsEnabled(bool bEnabled) { ms_bCollectEntityStats = bEnabled; }
	static CPostScanDebugSimplePvsStats& GetPvsStats() { return ms_simplePvsStats; }

	static int&		GetNumModelsPreRenderedRef() { return ms_NumModelPreRendered; }
	static int&		GetNumModelsPreRendered2Ref() { return ms_NumModelPreRendered2; }

	static void		ProcessBeforeAllPreRendering();
	static void		ProcessPreRenderEntity(CEntity* pEntity);
	static void		ProcessAfterAllPreRendering();

	static void		ResetAlphaCB();
	static void		AdjustAlphaBeforeLodForce(CEntity* pEntity);

	static bool&	GetOverrideGBufBitsEnabledRef() { return ms_bOverrideGBufBitsEnabled; }
	static bool		GetOverrideGBufBitsEnabled() { return ms_bOverrideGBufBitsEnabled; }
	static void		SetOverrideGBufBits(const char* renderPhaseName);
	static int		GetOverrideGBufVisibilityType();
	static void		UpdateOverrideGBufVisibilityType();

private:
	static void		DisplayAlphaAndVisibilityBlameInfoForSelected();
	static void		DisplayObjectPoolSummary();
	static void		DisplayDummyDebugOutput();
	static void		ProcessVisibilityStreamingCullshapes(CGtaPvsEntry* pStart, CGtaPvsEntry* pStop);
	static void		ProcessBlockReject(CGtaPvsEntry* pStart, CGtaPvsEntry* pStop);
	static bool		IsSizeOnScreenCulled(CEntity* pEntity);
	static bool		IsBoxCulled(CEntity* pEntity);
	static bool		IsLodLevelCulled(CEntity* pEntity, bool bForceUpAlphas);
	static bool		HasOnlyOneChild(CEntity* pEntity);
	static void		PrintNumModelsRendered(int entityListIndex, int drawListType, int waterOcclusionIndex, const char *name);
	static void		CentreCullBox();
	static void		SelectLogFile();
	static void		ShowBoxEntityCount();
	static void		AddToAlwaysPreRenderListCB();
	static void		RemoveFromAlwaysPreRenderListCB();
	static bool		IsInSelectedLodTree(CEntity* pEntity);
	static void		DrawLodRanges();
	static void		GetModelNameFromPickerCB();
	static void		GetNewModelNameFromPickerCB();
	static void		CreateNewModelSwapCB();
	static void		CreateNewModelHideCB();
	static void		CreateNewForceToObjectCB();
	static void		DeleteModelSwapCB();
	static bool		IsCulledBasedOnLodType(CEntity* pEntity);
	static void		GetForceObjPosFromCameraCB();
	static void		ClearAreaOfObjectsCB();
	static void		EnableCullSphereCB();
	static void		DisplayTimedModelInfo();

	static bool		ms_bCountModelsRendered;
	static bool		ms_bCountShadersRendered;
	static int		ms_CountModelsRenderedDrawList; // eGTADrawListType+1, starting with 0="All"
	static bool		ms_bCountModelsRenderedShowCascaShad  ;
	static bool		ms_bCountModelsRenderedShowParabShad  ;
	static bool     ms_bCountModelsRenderedShowReflections;
	static bool		ms_bCountModelsRenderedShowBreakdown  ;
	static bool		ms_bCountModelsRenderedShowPreRender  ;
	static bool		ms_bCountModelsRenderedShowStreaming  ;
	static bool		ms_bCountModelsRendered_PVS           ;
	static bool		ms_bCountModelsRendered_RPASS_VISIBLE ;
	static bool		ms_bCountModelsRendered_RPASS_LOD     ;
	static bool		ms_bCountModelsRendered_RPASS_CUTOUT  ;
	static bool		ms_bCountModelsRendered_RPASS_DECAL   ;
	static bool		ms_bCountModelsRendered_RPASS_FADING  ;
	static bool		ms_bCountModelsRendered_RPASS_ALPHA   ;
	static bool		ms_bCountModelsRendered_RPASS_WATER   ;
	static bool		ms_bCountModelsRendered_RPASS_TREE    ;
	static bool		ms_bCountModelsRendered_RPASS_IMPOSTER;

	static bool		ms_bOverrideGBufBitsEnabled;
	static int		ms_nOverrideGBufBitsPhase;
	static int		ms_nOverrideGBufVisibilityType;
	static bool		ms_bOverrideSubBucketMaskOnly;

	static bool		ms_bTimedModelInfo;
	static int		ms_exteriorCullSphereRadius;
	static bool		ms_bEnableExteriorHdCullSphere;
	static bool		ms_bEnableExteriorHdModelCull;

	static bool		ms_bDisplayModelSwaps;
	static bool		ms_bLazySwap;
	static bool		ms_bIncludeScriptObjects;
	static s32		ms_modelSwapRadius;
	static s32		ms_modelSwapIndex;
	static bool		ms_bForceUpAlpha;
	static bool		ms_bGeneratePvsLog;
	static bool		ms_bCullNonFragTypes;
	static bool		ms_bCullFragTypes;
	static bool		ms_bScreenSizeCullEnabled;
	static bool		ms_bScreenSizeCullReversed;
	static u32		ms_nScreenSizeCullMaxW;
	static u32		ms_nScreenSizeCullMaxH;
	static bool		ms_bScreenSizeCullShowGuide;
	static bool		ms_bOnlyDrawLodTrees;
	static bool		ms_bOnlyDrawSelectedLodTree;
	static bool		ms_bDrawLodRanges;
	static bool		ms_bDrawByLodLevel;
	static bool		ms_bStripLodLevel;
	static bool		ms_bStripAllExceptLodLevel;
	static bool		ms_bCullIfParented;
	static bool		ms_bCullIfHasOneChild;
	static s32		ms_nSelectedLodLevel;
	static bool		ms_bTrackSelectedEntity;
	static bool		ms_bDisplayObjects;
	static bool		ms_bDisplayDummyObjects;
	static bool		ms_bDisplayObjectPoolSummary;
	static bool		ms_bDisplayDummyDebugOutput;
	static bool		ms_bEnableForceObj;
	static bool		ms_bDrawForceObjSphere;
	static Vec3V	ms_vForceObjPos;
	static float	ms_fForceObjRadius;
	static float	ms_fClearAreaObjRadius;
	static s32		ms_ClearAreaFlag;

	static bool		ms_bBoxCull;
	static bool		ms_bShowEntityCount;
	static bool     ms_bShowOnlySelectedType;
	static s32		ms_nCullBoxX;
	static s32		ms_nCullBoxY;
	static s32		ms_nCullBoxZ;
	static s32		ms_nCullBoxW;
	static s32		ms_nCullBoxH;
	static s32		ms_nCullBoxD;
	static s32		ms_nCullBoxSelectedPolicy;
	static u8		ms_userAlpha;
	static bool		ms_bTweakAlpha;
	static bool		ms_bDoAlphaUpdate;
	static bool		ms_PartitionPvsInsteadOfSorting;
	static bool		ms_VerifyPvsSort;
	static bool		ms_bCollectEntityStats;
	static bool		ms_bDrawEntityStats;
	static bool		ms_bDisplayAlwaysPreRenderList;
	static bool		ms_bTogglePlayerPedVisibility;
	static CPostScanDebugSimplePvsStats ms_simplePvsStats;

	static int		ms_PVSModelCounts_RenderPhaseId[fwVisibilityFlags::MAX_PHASE_VISIBILITY_BITS]; // updated in CPostScanDebug::ProcessDebug
	static int		ms_PVSModelCounts_CascadeShadowsRenderPhaseId;
	static int		ms_PVSModelCounts_CascadeShadows[SUBPHASE_CASCADE_COUNT];
	static int		ms_PVSModelCounts_ReflectionsRenderPhaseId;
	static int      ms_PVSModelCounts_Reflections[SUBPHASE_REFLECT_COUNT];
	static int		ms_PSSModelCount;
	static int		ms_PSSModelSubCounts[5];

	static int		ms_NumModelPreRendered;
	static int		ms_NumModelPreRendered2;

	static bool		ms_bCullLoddedHd;
	static bool		ms_bCullOrphanHd;
	static bool		ms_bCullLod;
	static bool		ms_bCullSlod1;
	static bool		ms_bCullSlod2;
	static bool		ms_bCullSlod3;
	static bool		ms_bCullSlod4;

	static bool		ms_bDebugSelectedEntityPreRender;
	static bool		ms_bSelectedEntityWasPreRenderedThisFrame;
	static int		ms_preRenderFailTimer;

	static bool		ms_bDrawHdSphere;
	static bool		ms_bDrawHdSphereCounts;
	static bool		ms_bDrawHdSphereList;
	static bool		ms_bDrawLodSphere;
	static bool		ms_bDrawLodSphereCounts;
	static bool		ms_bDrawLodSphereList;
	static bool		ms_bDrawInteriorSphere;
	static bool		ms_bDrawInteriorSphereCounts;
	static bool		ms_bDrawInteriorSphereList;

	static bool		ms_bAdjustLodData;
	static bool		ms_bAdjustLodData_LoadedFlag;
	static u8		ms_adjustLodData_Alpha;

public:
	static bool		ms_breakIfEntityIsInPVS;
	static bool		ms_breakOnSelectedEntityInBuildRenderListFromPVS;
	static bool		ms_enableSortingAndExteriorScreenQuadScissoring;
	static bool     ms_forceRoomZeroReady;
	static bool     ms_dontScissorDynamicEntities;
	static bool		ms_renderVisFlagsForSelectedEntity;
	static bool     ms_renderClosedInteriors;
	static bool     ms_renderOpenPortalsForSelectedInterior;


	static bool	    ms_renderDebugSphere;
	static Vector3  ms_debugSpherePos; 
	static float    ms_debugSphereRadius;

	static CGtaPvsEntry ms_pvsEntryForSelectedEntity[2];
	static u16          ms_pvsEntryEntityRenderPhaseVisibilityMask[2];
	static bool			ms_enableForceLodInCCS;

	// i likes me some rag aliases
	static bool& GetDrawByLodLevelRef() { return ms_bDrawByLodLevel; }
	static s32& GetSelectedLodLevelRef() { return ms_nSelectedLodLevel; }
};

class CPostScanSqlDumper
{
private:

	static char			ms_filename[512];
	static bool			ms_dumpEnabled;
	static bool			ms_emitCreateTableStatement;
	static int			ms_captureId;

	static fiStream*	ms_dumpFile;
	static int			ms_gbufPhaseId;
	static int			ms_csmPhaseId;
	static int			ms_extReflectionPhaseId;
	static int			ms_hdStreamPhaseId;
	static int			ms_streamVolumePhaseId;

	static void EnableDump();
	
public:

	static void AddWidgets(bkBank* bank);
	static bool IsSqlDumpEnabled()			{ return ms_dumpEnabled; }

	static void OpenSqlFile();
	static void CloseSqlFile();
	static void DumpPvsEntry(const CGtaPvsEntry* entry);
};

#endif //__BANK

#endif //_SCENE_DEBUG_POSTSCANDEBUG_H_
