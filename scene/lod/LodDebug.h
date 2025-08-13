/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/lod/LodDebug.h
// PURPOSE : useful debug widgets for lod hierarchy analysis
// AUTHOR :  Ian Kiigan
// CREATED : 04/03/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_LOD_LODDEBUG_H_
#define _SCENE_LOD_LODDEBUG_H_

#if __BANK

#include "atl/array.h"
#include "vector/vector2.h"
#include "vector/vector3.h"
#include "renderer/PostScan.h"

class CLodDebug
{
public:
	static void AddWidgets(bkBank* pBank);
	static void Update();

	static float GetDistFromCamera(CEntity* pEntity);
	static void FindChildren(CEntity* pLod, atArray<CEntity*>* pResults);
	static u32 GetChildLodDist(CEntity* pEntity);
	static u32 GetNumChildren(CEntity* pEntity);

private:
	static void DisplayInfoOnSelected();
	static void DisplayTreeForSelected();
	static void DrawInfo(CEntity* pEntity, float fX, float fY);
	static void DrawLink(float fParentX, float fParentY, float fChildX, float fChildY);

#if RSG_PC
	static void AllowOrphanScalingCB();
#endif // RSG_PC

	static bool ms_bDisplayInfoOnSelected;
	static bool ms_bDisplayTreeForSelected;
	static CEntity* ms_pNextSelectedEntity;
	static s32 ms_nClickCooloff;
	static bool ms_bInitialised;
};

// simple debug widget to allow editing of lod distances
class CLodDistTweaker
{
public:
	static void AddWidgets(bkBank* pBank);
	static void Update();
	static void ChangeEntityLodDist(CEntity* pEntity, u32 dist);
	static void ChangeAllChildrenLodDists(CEntity* pEntity, u32 dist);

private:
	static void EnableFadeDistTweakCB();
	static void UpdateLodDistTweaking();
	static void WriteAllDistsNow();
	template<typename poolEntityType>
	static bool WriteAllDistsForPool(void* pItem, void* UNUSED_PARAM(data));

	static void AddSelectedEntityToMatrixList(CEntity* pEntity);
	static void AddDeletedEntityToRestList(CEntity* pEntity);
	static void UpdateRestOverrideList(CEntity* pEntity, u32 dist);
	static void UpdateRestChildOverrideList(CEntity* pEntity, u32 childDist);
public:
	static bool IsEntityFromMapData(CEntity* pEntity);
private:

	static CEntity* pLastSelectedEntity;
	static bool ms_bEnableFadeDistTweak;
	static float ms_afTweakFadeDists[LODTYPES_DEPTH_TOTAL];
	static bool bTweakLodDist;
	static u32 nTweakedLodDist;
	static u32 nTweakedChildLodDist;
	static u32 nOverrideLodDistHD;
	static u32 nOverrideLodDistOrphanHD;
	static u32 nOverrideLodDistLOD;
	static u32 nOverrideLodDistSLOD1;
	static u32 nOverrideLodDistSLOD2;
	static u32 nOverrideLodDistSLOD3;
	static u32 nOverrideLodDistSLOD4;
	static bool bWriteLodDistHD;
	static bool bWriteLodDistOrphanHD;
	static bool bWriteLodDistLOD;
	static bool bWriteLodDistSLOD1;
	static bool bWriteLodDistSLOD2;
	static bool bWriteLodDistSLOD3;
	static bool bWriteLodDistSLOD4;
};

// allow tweaking of lod scales
class CLodScaleTweaker
{
public:
	static void AddWidgets(bkBank* pBank);
	static void Update();
	static void AdjustScales();

private:
	static float ms_fRootScale;
	static float ms_fGlobalScale;
	static float ms_afPerLodLevelScales[LODTYPES_DEPTH_TOTAL];
	static float ms_fGrassScale;
	static bool ms_bOverrideScales;
	static bool ms_bDisplayScales;
	static float ms_fGlobalLodDistScaleOverrideDuration;

#if RSG_PC
public:
	static bool ms_bEnableAimingScales;
#endif	//RSG_PC
};

// misc lod debug widgets
class CLodDebugMisc
{
public:
	static void AddWidgets(bkBank* pBank);
	static void Update();

	static void SubmitForcedLodInfo(CEntity* pLod, CEntity* pUnloadedChild, CGtaPvsEntry* pStart, CGtaPvsEntry* pStop);
	static inline bool GetInfoWhenLodForced() { return ms_bGetInfoWhenLodForced; }
	static inline bool FakeForceLodRequired() { return ms_bFakeLodForce; }
	static inline bool GetAllowTimeBasedFade() { return ms_bEnableTimeBasedFade; }
	static inline void SetAllowTimeBasedFade(bool bEnable) { ms_bEnableTimeBasedFade = bEnable; }

private:
	static bool IsEntityInPvsAndVisibleInGbuf(CEntity* pTestEntity, CGtaPvsEntry* pStart, CGtaPvsEntry* pStop);
	static void ForcedLodInfoEnabledCB();

	static bool ms_bGetInfoWhenLodForced;
	static bool ms_bForcedLodInfoAvailable;
	static bool ms_bFakeLodForce;
	static bool ms_bShowDetailedForcedLodInfo;
	static atArray<atString> ms_aForcedLodInfo;
	static bool ms_bEnableTimeBasedFade;
	static bool ms_bShowLevelsAllowingTimeBasedFade;
};

#endif	//__BANK

#endif	//_SCENE_LOD_LODDEBUG_H_
