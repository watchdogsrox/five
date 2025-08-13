//
// FILE :    drawlistMgr.h
// PURPOSE : manage drawlist & execute operations on them
// AUTHOR :  john.
// CREATED : 30/8/06
//
/////////////////////////////////////////////////////////////////////////////////



#ifndef INC_DRAWLISTMGR_H_
#define INC_DRAWLISTMGR_H_

#include "fwutil/xmacro.h"
#include "fwdrawlist/drawlistmgr.h"
#include "fwrenderer/renderphaseswitches.h"

#include "renderer/DrawLists/DrawList.h"
#include "renderer/renderSettings.h"
#include "profile/timebars.h"
#include "drawListMgr_config.h"

#define MAX_TXD_REFS				(32)
#define MAX_PRT_REFS				(384 * 2 * 2)	// max of 384 systems, in up to 2 renderphases, in up to 2 drawlist, tardo.
#define FORCED_TECHNIQUE_STACK_SIZE	(2)
#define AVERAGE_CLOTH_DRAWS_PER_FRAME (4)
#define MAX_CLOTH					(160 * AVERAGE_CLOTH_DRAWS_PER_FRAME)
#define MAX_CLOTH_REFS				(MAX_CLOTH * 2 * 2)

enum eGTADrawListType {
	DL_RENDERPHASE                   =0,	// 00
	DL_RENDERPHASE_LIGHTING          ,		// 01
	DL_RENDERPHASE_RADAR             ,		// 02
	DL_RENDERPHASE_PRE_RENDER_VP     ,		// 03
	DL_RENDERPHASE_TIMEBARS          ,		// 04
	DL_RENDERPHASE_SCRIPT            ,		// 05
	DL_RENDERPHASE_TREE              ,		// 06
	DL_RENDERPHASE_DRAWSCENE         ,		// 07
	DL_RENDERPHASE_WATER_SURFACE     ,		// 08
	DL_RENDERPHASE_WATER_REFLECTION  ,		// 09
	DL_RENDERPHASE_REFLECTION_MAP    ,		// 10
	DL_RENDERPHASE_SEETHROUGH_MAP    ,		// 11
	DL_RENDERPHASE_PARABOLOID_SHADOWS,		// 12
	DL_RENDERPHASE_CASCADE_SHADOWS   ,		// 13
	DL_RENDERPHASE_PHONE             ,		// 14
	DL_RENDERPHASE_HUD               ,		// 15
	DL_RENDERPHASE_FRONTEND          ,		// 16
	DL_RENDERPHASE_PREAMBLE          ,		// 17
	DL_RENDERPHASE_HEIGHT_MAP        ,		// 18
	DL_RENDERPHASE_GBUF              ,		// 19
	DL_RENDERPHASE_CLOUD_GEN         ,		// 20
	DL_RENDERPHASE_PED_DAMAGE_GEN    ,		// 21
	DL_RENDERPHASE_PLAYER_SETTINGS   ,		// 22
	DL_RENDERPHASE_RAIN_UPDATE       ,		// 23
	DL_RENDERPHASE_RAIN_COLLISION_MAP,		// 24
	DL_RENDERPHASE_MIRROR_REFLECTION ,      // 25
	DL_RENDERPHASE_DEBUG             ,      // 26
#if __BANK
	DL_RENDERPHASE_DEBUG_OVERLAY     ,		// 27
#endif // __BANK
	DL_RENDERPHASE_LENSDISTORTION	 ,		// 28
	DL_BEGIN_DRAW                    ,		// 29 / 28
	DL_END_DRAW                      ,		// 30 / 29

#if DRAW_LIST_MGR_SPLIT_GBUFFER
	// 4 per fwDrawListAddPass (see s_OpaquePass[] in RenderListBuilder for example).
	DL_RENDERPHASE_GBUF_FIRST,
	DL_RENDERPHASE_GBUF_000, DL_RENDERPHASE_GBUF_001, DL_RENDERPHASE_GBUF_002, DL_RENDERPHASE_GBUF_003,
	DL_RENDERPHASE_GBUF_010, DL_RENDERPHASE_GBUF_011, DL_RENDERPHASE_GBUF_012, DL_RENDERPHASE_GBUF_013,
	DL_RENDERPHASE_GBUF_020, DL_RENDERPHASE_GBUF_021, DL_RENDERPHASE_GBUF_022, DL_RENDERPHASE_GBUF_023,
	DL_RENDERPHASE_GBUF_030, DL_RENDERPHASE_GBUF_031, DL_RENDERPHASE_GBUF_032, DL_RENDERPHASE_GBUF_033,
	DL_RENDERPHASE_GBUF_040, DL_RENDERPHASE_GBUF_041, DL_RENDERPHASE_GBUF_042, DL_RENDERPHASE_GBUF_043,
	DL_RENDERPHASE_GBUF_050, DL_RENDERPHASE_GBUF_051, DL_RENDERPHASE_GBUF_052, DL_RENDERPHASE_GBUF_053,
	DL_RENDERPHASE_GBUF_060, DL_RENDERPHASE_GBUF_061, DL_RENDERPHASE_GBUF_062, DL_RENDERPHASE_GBUF_063,
	DL_RENDERPHASE_GBUF_070, DL_RENDERPHASE_GBUF_071, DL_RENDERPHASE_GBUF_072, DL_RENDERPHASE_GBUF_073,
	DL_RENDERPHASE_GBUF_080, DL_RENDERPHASE_GBUF_081, DL_RENDERPHASE_GBUF_082, DL_RENDERPHASE_GBUF_083,
	DL_RENDERPHASE_GBUF_090, DL_RENDERPHASE_GBUF_091, DL_RENDERPHASE_GBUF_092, DL_RENDERPHASE_GBUF_093,
	DL_RENDERPHASE_GBUF_100, DL_RENDERPHASE_GBUF_101, DL_RENDERPHASE_GBUF_102, DL_RENDERPHASE_GBUF_103,
	DL_RENDERPHASE_GBUF_110, DL_RENDERPHASE_GBUF_111, DL_RENDERPHASE_GBUF_112, DL_RENDERPHASE_GBUF_113,
	DL_RENDERPHASE_GBUF_AMBT_LIGHTS,
	DL_RENDERPHASE_GBUF_LAST,
#endif // DRAW_LIST_MGR_SPLIT_GBUFFER

#if DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS
	DL_RENDERPHASE_PARABOLOID_SHADOWS_FIRST,
	DL_RENDERPHASE_PARABOLOID_SHADOWS_0, // Must be able to accommodate all active paraboloid shadows (see MAX_ACTIVE_PARABOLOID_SHADOWS in paraboloid shadows.h)
	DL_RENDERPHASE_PARABOLOID_SHADOWS_1,
	DL_RENDERPHASE_PARABOLOID_SHADOWS_2,
	DL_RENDERPHASE_PARABOLOID_SHADOWS_3,
	DL_RENDERPHASE_PARABOLOID_SHADOWS_4,
	DL_RENDERPHASE_PARABOLOID_SHADOWS_5,
	DL_RENDERPHASE_PARABOLOID_SHADOWS_6,
	DL_RENDERPHASE_PARABOLOID_SHADOWS_7,
	DL_RENDERPHASE_PARABOLOID_SHADOWS_LAST,
#endif // DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS

#if DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS
	DL_RENDERPHASE_CASCADE_SHADOWS_FIRST,
	DL_RENDERPHASE_CASCADE_SHADOWS_0,
	DL_RENDERPHASE_CASCADE_SHADOWS_1,
	DL_RENDERPHASE_CASCADE_SHADOWS_2,
	DL_RENDERPHASE_CASCADE_SHADOWS_3,
	DL_RENDERPHASE_CASCADE_SHADOWS_RVL,
	DL_RENDERPHASE_CASCADE_SHADOWS_LAST,
#endif // DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS

#if DRAW_LIST_MGR_SPLIT_DRAWSCENE
	// If updating, please make sure you update the CRenderPhaseDrawScene constructor.
	DL_RENDERPHASE_DRAWSCENE_FIRST,
	DL_RENDERPHASE_DRAWSCENE_PRE_ALPHA,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_01,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_02,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_03,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_04,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_05,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_06,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_07,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_08,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_09,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_10,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_11,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_12,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_13,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_14,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_15,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_16,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_17,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_18,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_19,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_20,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_21,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_22,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_23,
	DL_RENDERPHASE_DRAWSCENE_ALPHA_24,
	DL_RENDERPHASE_DRAWSCENE_POST_ALPHA,
	DL_RENDERPHASE_DRAWSCENE_NON_DEPTH_FX,
	DL_RENDERPHASE_DRAWSCENE_DEBUG,
	DL_RENDERPHASE_DRAWSCENE_LAST,
#endif // DRAW_LIST_MGR_SPLIT_DRAWSCENE

	DL_MAX_TYPES
};


#if __BANK
inline const char** GetUIStrings_eGTADrawListType(bool bStartWithAll)
{
	static const char* strings[] = // keep this in sync with eGTADrawListType
	{
		"All",
		STRING(DL_RENDERPHASE                   ),
		STRING(DL_RENDERPHASE_LIGHTING          ),
		STRING(DL_RENDERPHASE_RADAR             ),
		STRING(DL_RENDERPHASE_PRE_RENDER_VP     ),
		STRING(DL_RENDERPHASE_TIMEBARS          ),
		STRING(DL_RENDERPHASE_SCRIPT            ),
		STRING(DL_RENDERPHASE_TREE              ),
		STRING(DL_RENDERPHASE_DRAWSCENE         ),
		STRING(DL_RENDERPHASE_WATER_SURFACE     ),
		STRING(DL_RENDERPHASE_WATER_REFLECTION  ),
		STRING(DL_RENDERPHASE_REFLECTION_MAP    ),
		STRING(DL_RENDERPHASE_SEETHROUGH_MAP    ),
		STRING(DL_RENDERPHASE_PARABOLOID_SHADOWS),
		STRING(DL_RENDERPHASE_CASCADE_SHADOWS   ),
		STRING(DL_RENDERPHASE_PHONE             ),
		STRING(DL_RENDERPHASE_HUD               ),
		STRING(DL_RENDERPHASE_FRONTEND          ),
		STRING(DL_RENDERPHASE_PREAMBLE          ),
		STRING(DL_RENDERPHASE_HEIGHT_MAP        ),
		STRING(DL_RENDERPHASE_GBUF              ),
		STRING(DL_RENDERPHASE_CLOUD_GEN         ),
		STRING(DL_RENDERPHASE_PED_DAMAGE_GEN    ),
		STRING(DL_RENDERPHASE_PLAYER_SETTINGS   ),
		STRING(DL_RENDERPHASE_RAIN_UPDATE       ),
		STRING(DL_RENDERPHASE_RAIN_COLLISION_MAP),
		STRING(DL_RENDERPHASE_MIRROR_REFLECTION ),
		STRING(DL_RENDERPHASE_DEBUG             ),
#if __BANK
		STRING(DL_RENDERPHASE_DEBUG_OVERLAY     ),
#endif // __BANK
		STRING(DL_BEGIN_DRAW                    ),
		STRING(DL_END_DRAW                      ),

#if DRAW_LIST_MGR_SPLIT_GBUFFER
		STRING(DL_RENDERPHASE_GBUF_FIRST),
		STRING(DL_RENDERPHASE_GBUF_000), STRING(DL_RENDERPHASE_GBUF_001), STRING(DL_RENDERPHASE_GBUF_002), STRING(DL_RENDERPHASE_GBUF_003),
		STRING(DL_RENDERPHASE_GBUF_010), STRING(DL_RENDERPHASE_GBUF_011), STRING(DL_RENDERPHASE_GBUF_012), STRING(DL_RENDERPHASE_GBUF_013),
		STRING(DL_RENDERPHASE_GBUF_020), STRING(DL_RENDERPHASE_GBUF_021), STRING(DL_RENDERPHASE_GBUF_022), STRING(DL_RENDERPHASE_GBUF_023),
		STRING(DL_RENDERPHASE_GBUF_030), STRING(DL_RENDERPHASE_GBUF_031), STRING(DL_RENDERPHASE_GBUF_032), STRING(DL_RENDERPHASE_GBUF_033),
		STRING(DL_RENDERPHASE_GBUF_040), STRING(DL_RENDERPHASE_GBUF_041), STRING(DL_RENDERPHASE_GBUF_042), STRING(DL_RENDERPHASE_GBUF_043),
		STRING(DL_RENDERPHASE_GBUF_050), STRING(DL_RENDERPHASE_GBUF_051), STRING(DL_RENDERPHASE_GBUF_052), STRING(DL_RENDERPHASE_GBUF_053),
		STRING(DL_RENDERPHASE_GBUF_060), STRING(DL_RENDERPHASE_GBUF_061), STRING(DL_RENDERPHASE_GBUF_062), STRING(DL_RENDERPHASE_GBUF_063),
		STRING(DL_RENDERPHASE_GBUF_070), STRING(DL_RENDERPHASE_GBUF_071), STRING(DL_RENDERPHASE_GBUF_072), STRING(DL_RENDERPHASE_GBUF_073),
		STRING(DL_RENDERPHASE_GBUF_080), STRING(DL_RENDERPHASE_GBUF_081), STRING(DL_RENDERPHASE_GBUF_082), STRING(DL_RENDERPHASE_GBUF_083),
		STRING(DL_RENDERPHASE_GBUF_090), STRING(DL_RENDERPHASE_GBUF_091), STRING(DL_RENDERPHASE_GBUF_092), STRING(DL_RENDERPHASE_GBUF_093),
		STRING(DL_RENDERPHASE_GBUF_100), STRING(DL_RENDERPHASE_GBUF_101), STRING(DL_RENDERPHASE_GBUF_102), STRING(DL_RENDERPHASE_GBUF_103),
		STRING(DL_RENDERPHASE_GBUF_110), STRING(DL_RENDERPHASE_GBUF_111), STRING(DL_RENDERPHASE_GBUF_112), STRING(DL_RENDERPHASE_GBUF_113),
		STRING(DL_RENDERPHASE_GBUF_AMBT_LIGHTS),
		STRING(DL_RENDERPHASE_GBUF_LAST),
#endif // DRAW_LIST_MGR_SPLIT_GBUFFER

#if DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS
		STRING(DL_RENDERPHASE_PARABOLOID_SHADOWS_FIRST),
		STRING(DL_RENDERPHASE_PARABOLOID_SHADOWS_0),
		STRING(DL_RENDERPHASE_PARABOLOID_SHADOWS_1),
		STRING(DL_RENDERPHASE_PARABOLOID_SHADOWS_2),
		STRING(DL_RENDERPHASE_PARABOLOID_SHADOWS_3),
		STRING(DL_RENDERPHASE_PARABOLOID_SHADOWS_4),
		STRING(DL_RENDERPHASE_PARABOLOID_SHADOWS_5),
		STRING(DL_RENDERPHASE_PARABOLOID_SHADOWS_6),
		STRING(DL_RENDERPHASE_PARABOLOID_SHADOWS_7),
		STRING(DL_RENDERPHASE_PARABOLOID_SHADOWS_LAST),
#endif //DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS

#if DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS
		STRING(DL_RENDERPHASE_CASCADE_SHADOWS_FIRST),
		STRING(DL_RENDERPHASE_CASCADE_SHADOWS_0),
		STRING(DL_RENDERPHASE_CASCADE_SHADOWS_1),
		STRING(DL_RENDERPHASE_CASCADE_SHADOWS_2),
		STRING(DL_RENDERPHASE_CASCADE_SHADOWS_3),
		STRING(DL_RENDERPHASE_CASCADE_SHADOWS_RVL),
		STRING(DL_RENDERPHASE_CASCADE_SHADOWS_LAST),
#endif // DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS

#if DRAW_LIST_MGR_SPLIT_DRAWSCENE
		STRING(DL_RENDERPHASE_DRAWSCENE_FIRST),
		STRING(DL_RENDERPHASE_DRAWSCENE_PRE_ALPHA),
		STRING(DL_RENDERPHASE_DRAWSCENE_ALPHA_01),
		STRING(DL_RENDERPHASE_DRAWSCENE_ALPHA_02),
		STRING(DL_RENDERPHASE_DRAWSCENE_ALPHA_03),
		STRING(DL_RENDERPHASE_DRAWSCENE_ALPHA_04),
		STRING(DL_RENDERPHASE_DRAWSCENE_ALPHA_05),
		STRING(DL_RENDERPHASE_DRAWSCENE_ALPHA_06),
		STRING(DL_RENDERPHASE_DRAWSCENE_ALPHA_07),
		STRING(DL_RENDERPHASE_DRAWSCENE_ALPHA_08),
		STRING(DL_RENDERPHASE_DRAWSCENE_POST_ALPHA),
		STRING(DL_RENDERPHASE_DRAWSCENE_NON_DEPTH_FX),
		STRING(DL_RENDERPHASE_DRAWSCENE_DEBUG),
		STRING(DL_RENDERPHASE_DRAWSCENE_LAST),
#endif // DRAW_LIST_MGR_SPLIT_DRAWSCENE
	};

	return bStartWithAll ? strings : &strings[1];
}
#endif // __BANK

class fragInstGta;
class CPtFxSortedEntity;

class CDrawListMgr : public dlDrawListMgr {

	friend class dlCmdBase;
	friend class dlCmdNewDrawList;
	friend class dlCmdEndDrawList;

public:
	CDrawListMgr();
	~CDrawListMgr();

	void Initialise();
	virtual void Shutdown();
	virtual void Reset();
	void Update();

	virtual void	RemoveAllRefs(u32 flags=0);

	static const CRenderSettings& GetExecutionRenderSettings();

	eRenderMode GetRtRenderMode(void)		{ return (eRenderMode)m_rtRenderMode; }
	eRenderMode GetUpdateRenderMode(void)	{ return (eRenderMode)m_updtRenderMode; }

	int GetDrawListLODFlagShift(void);
	int GetNumAlphaEntityLists();

#if !__FINAL
	// PURPOSE: Return the GPU time of a draw list.
	virtual float GetDrawListGPUTime(int drawListIndex) const;
#endif // !__FINAL

	// from the context of executing drawlist instructions, are we executing a shadow draw list?

	static bool IsGBufDrawList(int dliType)				{ return dliType == DL_RENDERPHASE_GBUF                 DRAW_LIST_MGR_SPLIT_GBUFFER_ONLY(            || (dliType >= DL_RENDERPHASE_GBUF_FIRST               && dliType <= DL_RENDERPHASE_GBUF_LAST)               ); }
	static bool IsParaboloidShadowDrawList(int dliType)	{ return dliType == DL_RENDERPHASE_PARABOLOID_SHADOWS   DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS_ONLY( || (dliType >= DL_RENDERPHASE_PARABOLOID_SHADOWS_FIRST && dliType <= DL_RENDERPHASE_PARABOLOID_SHADOWS_LAST) ); }
	static bool IsCascadeShadowDrawList(int dliType)	{ return dliType == DL_RENDERPHASE_CASCADE_SHADOWS      DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS_ONLY(    || (dliType >= DL_RENDERPHASE_CASCADE_SHADOWS_FIRST    && dliType <= DL_RENDERPHASE_CASCADE_SHADOWS_LAST)    ); }
	static bool IsShadowDrawList(int dliType)			{ return IsParaboloidShadowDrawList(dliType) || IsCascadeShadowDrawList(dliType); }
	static bool IsLightingDrawList(int dliType)			{ return dliType == DL_RENDERPHASE_LIGHTING;           }
	static bool IsGlobalReflectionDrawList(int dliType)	{ return dliType == DL_RENDERPHASE_REFLECTION_MAP;     }
	static bool IsMirrorReflectionDrawList(int dliType)	{ return dliType == DL_RENDERPHASE_MIRROR_REFLECTION;  }
	static bool IsWaterReflectionDrawList(int dliType)	{ return dliType == DL_RENDERPHASE_WATER_REFLECTION;   }
	static bool IsHudDrawList(int dliType)				{ return dliType == DL_RENDERPHASE_HUD;                }
	static bool IsRainCollisionMapDrawList(int dliType)	{ return dliType == DL_RENDERPHASE_RAIN_COLLISION_MAP; }
	static bool IsDrawSceneDrawList(int dliType)		{ return dliType == DL_RENDERPHASE_DRAWSCENE            DRAW_LIST_MGR_SPLIT_DRAWSCENE_ONLY(          || (dliType >= DL_RENDERPHASE_DRAWSCENE_FIRST           && dliType <= DL_RENDERPHASE_DRAWSCENE_LAST)         ); }
	static bool IsSeeThroughDrawList(int dliType)		{ return dliType == DL_RENDERPHASE_SEETHROUGH_MAP; }
	static bool IsScriptDrawList(int dliType)			{ return dliType == DL_RENDERPHASE_SCRIPT;                }
#if __BANK
	static bool IsDebugDrawList(int dliType)			{ return dliType == DL_RENDERPHASE_DEBUG;              }
	static bool IsDebugOverlayDrawList(int dliType)		{ return dliType == DL_RENDERPHASE_DEBUG_OVERLAY;      }
#endif // __BANK

	bool IsExecutingGBufDrawList()				{ FastAssert(sysThreadType::IsRenderThread()); return pCurrExecutingList && IsGBufDrawList(             pCurrExecutingList->m_type ); }
	bool IsExecutingLightingDrawList()			{ FastAssert(sysThreadType::IsRenderThread()); return pCurrExecutingList && IsLightingDrawList(         pCurrExecutingList->m_type ); }
	bool IsExecutingParaboloidShadowDrawList()	{ FastAssert(sysThreadType::IsRenderThread()); return pCurrExecutingList && IsParaboloidShadowDrawList( pCurrExecutingList->m_type ); }
	bool IsExecutingCascadeShadowDrawList()		{ FastAssert(sysThreadType::IsRenderThread()); return pCurrExecutingList && IsCascadeShadowDrawList(    pCurrExecutingList->m_type ); }
	bool IsExecutingShadowDrawList()			{ FastAssert(sysThreadType::IsRenderThread()); return pCurrExecutingList && IsShadowDrawList(           pCurrExecutingList->m_type ); }
	bool IsExecutingGlobalReflectionDrawList()	{ FastAssert(sysThreadType::IsRenderThread()); return pCurrExecutingList && IsGlobalReflectionDrawList( pCurrExecutingList->m_type ); }
	bool IsExecutingMirrorReflectionDrawList()	{ FastAssert(sysThreadType::IsRenderThread()); return pCurrExecutingList && IsMirrorReflectionDrawList( pCurrExecutingList->m_type ); }
	bool IsExecutingWaterReflectionDrawList()	{ FastAssert(sysThreadType::IsRenderThread()); return pCurrExecutingList && IsWaterReflectionDrawList(  pCurrExecutingList->m_type ); }
	bool IsExecutingHudDrawList()				{ FastAssert(sysThreadType::IsRenderThread()); return pCurrExecutingList && IsHudDrawList(              pCurrExecutingList->m_type ); }
	bool IsExecutingRainCollisionMapDrawList()	{ FastAssert(sysThreadType::IsRenderThread()); return pCurrExecutingList && IsRainCollisionMapDrawList( pCurrExecutingList->m_type ); }
	bool IsExecutingDrawSceneDrawList()			{ FastAssert(sysThreadType::IsRenderThread()); return pCurrExecutingList && IsDrawSceneDrawList(        pCurrExecutingList->m_type ); }
	bool IsExecutingSeeThroughDrawList()		{ FastAssert(sysThreadType::IsRenderThread()); return pCurrExecutingList && IsSeeThroughDrawList(       pCurrExecutingList->m_type ); }
#if __BANK
	bool IsExecutingDebugDrawList()				{ FastAssert(sysThreadType::IsRenderThread()); return pCurrExecutingList && IsDebugDrawList(            pCurrExecutingList->m_type ); }
	bool IsExecutingDebugOverlayDrawList()		{ FastAssert(sysThreadType::IsRenderThread()); return pCurrExecutingList && IsDebugOverlayDrawList(     pCurrExecutingList->m_type ); }
#endif // __BANK

	bool IsBuildingGBufDrawList()				{ FastAssert(sysThreadType::IsUpdateThread()); return pCurrBuildingList && IsGBufDrawList(             pCurrBuildingList->m_type ); }
	bool IsBuildingSeeThroughDrawList()			{ FastAssert(sysThreadType::IsUpdateThread()); return pCurrBuildingList && IsSeeThroughDrawList(       pCurrBuildingList->m_type ); }
	bool IsBuildingParaboloidShadowDrawList()	{ FastAssert(sysThreadType::IsUpdateThread()); return pCurrBuildingList && IsParaboloidShadowDrawList( pCurrBuildingList->m_type ); }
	bool IsBuildingCascadeShadowDrawList()		{ FastAssert(sysThreadType::IsUpdateThread()); return pCurrBuildingList && IsCascadeShadowDrawList(    pCurrBuildingList->m_type ); }
	bool IsBuildingShadowDrawList()				{ FastAssert(sysThreadType::IsUpdateThread()); return pCurrBuildingList && IsShadowDrawList(           pCurrBuildingList->m_type ); }
	bool IsBuildingGlobalReflectionDrawList()	{ FastAssert(sysThreadType::IsUpdateThread()); return pCurrBuildingList && IsGlobalReflectionDrawList( pCurrBuildingList->m_type ); }
	bool IsBuildingMirrorReflectionDrawList()	{ FastAssert(sysThreadType::IsUpdateThread()); return pCurrBuildingList && IsMirrorReflectionDrawList( pCurrBuildingList->m_type ); }
	bool IsBuildingWaterReflectionDrawList()	{ FastAssert(sysThreadType::IsUpdateThread()); return pCurrBuildingList && IsWaterReflectionDrawList(  pCurrBuildingList->m_type ); }
	bool IsBuildingHudDrawList()				{ FastAssert(sysThreadType::IsUpdateThread()); return pCurrBuildingList && IsHudDrawList(              pCurrBuildingList->m_type ); }
	bool IsBuildingRainCollisionMapDrawList()	{ FastAssert(sysThreadType::IsUpdateThread()); return pCurrBuildingList && IsRainCollisionMapDrawList( pCurrBuildingList->m_type ); }
	bool IsBuildingDrawSceneDrawList()			{ FastAssert(sysThreadType::IsUpdateThread()); return pCurrBuildingList && IsDrawSceneDrawList(        pCurrBuildingList->m_type ); }
	bool IsBuildingScriptDrawList()				{ FastAssert(sysThreadType::IsUpdateThread()); return pCurrBuildingList && IsScriptDrawList(        pCurrBuildingList->m_type ); }
#if __BANK
	bool IsBuildingDebugDrawList()				{ FastAssert(sysThreadType::IsUpdateThread()); return pCurrBuildingList && IsDebugDrawList(            pCurrBuildingList->m_type ); }
	bool IsBuildingDebugOverlayDrawList()		{ FastAssert(sysThreadType::IsUpdateThread()); return pCurrBuildingList && IsDebugOverlayDrawList(     pCurrBuildingList->m_type ); }
#endif // __BANK

#if __BANK
	static void RequestShaderReload() { ms_ShaderReloadRequested = true; }
	virtual bool SetupWidgets(bkBank& bank);
#endif

	void ClothCleanup();
 	void CharClothCleanup();

private:

#if __BANK
	void PrintDrawListType(eDrawListType drawListType);
	void DumpDebugData();
#endif //__BANK

#if DRAW_LIST_MGR_SPLIT_DRAWSCENE
	static int ms_nNumAlphaEntityLists;
#endif
};

#if !__ASSERT
inline int CDrawListMgr::GetDrawListLODFlagShift(void)
{
	FastAssert(sysThreadType::IsRenderThread());
	return dlCmdNewDrawList::GetCurrDrawListLODFlagShift();
}
#endif // !__ASSERT

#define DRAWLISTMGR ((CDrawListMgr *) gDrawListMgr)

#endif // INC_DRAWLISTMGR_H_
