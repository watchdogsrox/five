//
// FILE :    drawlistMgr.cpp
// PURPOSE : manage drawlist & execute operations on them
// AUTHOR :  john.
// CREATED : 30/8/06
//
/////////////////////////////////////////////////////////////////////////////////
#include "renderer/DrawLists/DrawListMgr.h"

// Rage headers
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/button.h"
#include "bank/combo.h"
#include "bank/slider.h"
#include "grmodel/shaderfx.h"
#include "grcore/gcmdebug.h"
#include "profile/timebars.h"
#include "grcore/debugdraw.h"
#include "fwdrawlist/drawlist_channel.h"
#include "fwdrawlist/drawlistmgr.h"
#include "fwscene/stores/txdstore.h"
#include "fwpheffects/clothmanager.h"
#include "cloth/environmentcloth.h"

// game headers
#include "camera/viewports/ViewportManager.h"
#include "renderer/Drawlists/drawList.h"
#include "renderer/renderThread.h"
#include "Peds/rendering/PedProps.h"
#include "Peds/rendering/PedVariationStream.h"
#include "physics/gtainst.h"
#include "physics/physics.h"
#include "scene/Building.h"
#include "scene/debug/PostScanDebug.h"
#include "scene/lod/LodDrawable.h"
#include "profile/cputrace.h"
#include "grprofile/gcmtrace.h"
#include "vfx/particles/PtFxEntity.h"

#if	__PPU
#include "grcore/wrapper_gcm.h"
using namespace cell::Gcm;
#endif

RENDER_OPTIMISATIONS()

//PARAM(RT, "[RenderThread] Use render thread");
PARAM(DRAWLIST, "[DrawLists] Use drawlists to render");

#if __BANK
static bool bShowDrawlistStats = false;
bool bShowDrawlistExec = false;
bool bCheckListsOnceBuilt = false;
//s32 CDrawListMgr::ms_drawListStats[DL_MAX_TYPES] = {0,};
//s32 CDrawListMgr::ms_numDrawlists = 0;
//bool CDrawListMgr::ms_ShaderReloadRequested = false;
bool bSafeMode = false;
bool bPedFastDraw = false;
#endif //__BANK

#if DRAW_LIST_MGR_SPLIT_DRAWSCENE
int CDrawListMgr::ms_nNumAlphaEntityLists = 4;
#endif

namespace rage {
	extern dlDrawCommandBuffer *gDCBuffer;
}

#if RAGETRACE
pfTraceCounterId gpuDrawListTrace[DL_MAX_TYPES];
pfTraceCounterId cpuDrawListTrace[DL_MAX_TYPES];
#endif

CDrawListMgr::CDrawListMgr(){
	
	m_rtRenderMode = m_updtRenderMode = rmNIL;
}

CDrawListMgr::~CDrawListMgr(){
}

dlDrawListInfo executionFence;

#if __ASSERT
int CDrawListMgr::GetDrawListLODFlagShift(void)
{
	FastAssert(sysThreadType::IsRenderThread());

	switch (dlCmdNewDrawList::GetCurrDrawListLODFlagShift())
	{
		// NOTE -- accessing GetOverrideGBufBitsEnabled is not threadsafe, so to prevent asserts i've just disabled these for now
	case LODDrawable::LODFLAG_SHIFT_SHADOW		: break;//Assert(CPostScanDebug::GetOverrideGBufBitsEnabled() || IsExecutingShadowDrawList()); break;
	case LODDrawable::LODFLAG_SHIFT_REFLECTION	: break;//Assert(CPostScanDebug::GetOverrideGBufBitsEnabled() || IsExecutingGlobalReflectionDrawList()); break;
	case LODDrawable::LODFLAG_SHIFT_WATER		: break;//Assert(CPostScanDebug::GetOverrideGBufBitsEnabled() || IsExecutingWaterReflectionDrawList()); break;
	case LODDrawable::LODFLAG_SHIFT_MIRROR		: break;//Assert(CPostScanDebug::GetOverrideGBufBitsEnabled() || IsExecutingMirrorReflectionDrawList()); break;
	case 0:
		Assert(!IsExecutingShadowDrawList());
		Assert(!IsExecutingGlobalReflectionDrawList());
		Assert(!IsExecutingWaterReflectionDrawList());
		Assert(!IsExecutingMirrorReflectionDrawList());
		break;
	default:
		Assert(0);
	}

	return dlCmdNewDrawList::GetCurrDrawListLODFlagShift();
}
#endif // __ASSERT

int CDrawListMgr::GetNumAlphaEntityLists()
{
	return DRAW_LIST_MGR_SPLIT_DRAWSCENE_SWITCH(ms_nNumAlphaEntityLists, 1);
}

void CDrawListMgr::Initialise(){
	USE_MEMBUCKET(MEMBUCKET_RENDER);
	gDCBuffer = rage_new CDrawCommandBuffer();

	dlDrawListMgr::Init(DL_MAX_TYPES);

	// Initialize all GTA-specific buckets
#if MULTIPLE_RENDER_THREADS_ALLOW_DRAW_LISTS_ON_RENDER_THREAD
	const int CPU_MAIN = dlDrawListMgr::SUBCPUS_MAIN;
	(void) CPU_MAIN;
#endif
	const int CPU_SYNC_MAIN = dlDrawListMgr::SUBCPUS_SYNC_MAIN;
	const int CPU_ALL = dlDrawListMgr::SUBCPUS_ALL;
	const int CPU_1 = (1<<0)&CPU_ALL;
	const int CPU_2 = (1<<1)&CPU_ALL;
	const int CPU_LIGHTS = CPU_1; // The g-buffer renderphase outputs the lights for the lighting renderphase.
	(void) CPU_1;
	(void) CPU_2;
#if MULTIPLE_RENDER_THREADS == 4
	const int CPU_3 = (1<<2)&CPU_ALL;
	const int CPU_4 = (1<<3)&CPU_ALL;
	(void) CPU_3;
	(void) CPU_4;

	// PostFX use a UAV buffer with an append counter.  This is currently not thread safe
	// (https://forums.xboxlive.com/AnswerPage.aspx?qid=44191dbc-0bd9-40e8-b5cf-ec86e94ef53b&tgt=1).
	// PostFX is generally done in DL_RENDERPHASE_DRAWSCENE_NON_DEPTH_FX, but can also be executed
	// in DL_HUD.
	//
	// TODO: Re-implement shader without using D3D append/consume counters (ie, do it manually with
	// atomic operations from the shader).  Then CPU_DRAWSCENE can be replaced with CPU_ALL.  This
	// is just a workaround for the thread safety issue, it is also required for moving to the
	// set fast semantics contexts since they do not support append consume counters either.
	//
#if MULTIPLE_RENDER_THREADS_ALLOW_DRAW_LISTS_ON_RENDER_THREAD
	const int CPU_DRAWSCENE = __D3D11 ? CPU_MAIN : CPU_ALL;
	const int CPU_DEBUG = __D3D11 ? CPU_MAIN : CPU_ALL;
#else
	const int CPU_DRAWSCENE = CPU_ALL;
	const int CPU_DEBUG = CPU_ALL;
#endif

	SetDrawListInfo(DL_RENDERPHASE,                         "INVALID",                      CPU_SYNC_MAIN,      0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_LIGHTING,                "DL_LIGHTING",                  CPU_LIGHTS,         0,                                      true,  0.0f,  1.0f,  9.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_RADAR,                   "DL_RADAR",                     CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_PRE_RENDER_VP,           "DL_PRE_RENDER_VP",             CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_TIMEBARS,                "DL_TIMEBARS",                  CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_SCRIPT,                  "DL_SCRIPT",                    CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_TREE,                    "DL_TREE",                      CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);

#if DRAW_LIST_MGR_SPLIT_DRAWSCENE
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE,               "DL_DRAWSCENE",                 CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);	
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_FIRST,         "DL_DRAWSCENE_FIRST",           CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);	
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_PRE_ALPHA,     "DL_DRAWSCENE_PRE_ALPHA",       CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_01,		"DL_DRAWSCENE_ALPHA_01",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_02,		"DL_DRAWSCENE_ALPHA_02",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_03,		"DL_DRAWSCENE_ALPHA_03",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_04,		"DL_DRAWSCENE_ALPHA_04",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_05,		"DL_DRAWSCENE_ALPHA_05",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_06,		"DL_DRAWSCENE_ALPHA_06",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_07,		"DL_DRAWSCENE_ALPHA_07",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_08,		"DL_DRAWSCENE_ALPHA_08",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_09,		"DL_DRAWSCENE_ALPHA_09",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_10,		"DL_DRAWSCENE_ALPHA_10",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_11,		"DL_DRAWSCENE_ALPHA_11",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_12,		"DL_DRAWSCENE_ALPHA_12",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_13,		"DL_DRAWSCENE_ALPHA_13",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_14,		"DL_DRAWSCENE_ALPHA_14",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_15,		"DL_DRAWSCENE_ALPHA_15",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_16,		"DL_DRAWSCENE_ALPHA_16",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_17,		"DL_DRAWSCENE_ALPHA_17",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_18,		"DL_DRAWSCENE_ALPHA_18",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_19,		"DL_DRAWSCENE_ALPHA_19",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_20,		"DL_DRAWSCENE_ALPHA_20",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_21,		"DL_DRAWSCENE_ALPHA_21",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_22,		"DL_DRAWSCENE_ALPHA_22",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_23,		"DL_DRAWSCENE_ALPHA_23",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_24,		"DL_DRAWSCENE_ALPHA_24",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_POST_ALPHA,    "DL_DRAWSCENE_POST_ALPHA",      CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_NON_DEPTH_FX,  "DL_DRAWSCENE_NON_DEPTH_FX",    CPU_DRAWSCENE,      0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_DEBUG,         "DL_DRAWSCENE_DEBUG",           CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_LAST,			"DL_DRAWSCENE_LAST",			CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);	
#else
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE,               "DL_DRAWSCENE",                 CPU_DRAWSCENE,      0,                                      true,  0.0f,  3.0f,  6.0f,  false);
#endif

	SetDrawListInfo(DL_RENDERPHASE_WATER_SURFACE,           "DL_WATER_SURFACE",             CPU_ALL,            0,                                      true,  0.0f,  0.5f,  0.5f,  false);
	SetDrawListInfo(DL_RENDERPHASE_WATER_REFLECTION,        "DL_WATER_REFLECTION",          CPU_ALL,            LODDrawable::LODFLAG_SHIFT_WATER,       true,  0.0f,  0.0f,  3.5f,  true);
	SetDrawListInfo(DL_RENDERPHASE_REFLECTION_MAP,          "DL_REFLECTION_MAP",            CPU_ALL,            LODDrawable::LODFLAG_SHIFT_REFLECTION,  true,  0.0f,  4.0f,  4.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_SEETHROUGH_MAP,          "DL_SEETHROUGH_MAP",            CPU_ALL,            0,                                      true,  0.0f,  0.0f,  0.0f,  true);

	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS,      "DL_PARABOLOID_SHADOWS",        CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  !DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS);
#if DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_FIRST,"DL_PARABOLOID_SHADOWS_0",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_0,    "DL_PARABOLOID_SHADOWS_0",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_1,    "DL_PARABOLOID_SHADOWS_1",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_2,    "DL_PARABOLOID_SHADOWS_2",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_3,    "DL_PARABOLOID_SHADOWS_3",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_4,    "DL_PARABOLOID_SHADOWS_4",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_5,    "DL_PARABOLOID_SHADOWS_5",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_6,    "DL_PARABOLOID_SHADOWS_6",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_7,    "DL_PARABOLOID_SHADOWS_7",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_LAST, "DL_PARABOLOID_SHADOWS_7",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
#endif

	SetDrawListInfo(DL_RENDERPHASE_CASCADE_SHADOWS,         "DL_CASCADE_SHADOWS",           CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  !DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS);
#if DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS
	SetDrawListInfo(DL_RENDERPHASE_CASCADE_SHADOWS_FIRST,   "DL_CASCADE_SHADOWS_FIRST",     CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_CASCADE_SHADOWS_0,       "DL_CASCADE_SHADOWS_0",         CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_CASCADE_SHADOWS_1,       "DL_CASCADE_SHADOWS_1",         CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_CASCADE_SHADOWS_2,       "DL_CASCADE_SHADOWS_2",         CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_CASCADE_SHADOWS_3,       "DL_CASCADE_SHADOWS_3",         CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_CASCADE_SHADOWS_RVL,     "DL_CASCADE_SHADOWS_REVEAL",    CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_CASCADE_SHADOWS_LAST,    "DL_CASCADE_SHADOWS_LAST",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
#endif

	SetDrawListInfo(DL_RENDERPHASE_PHONE,                   "DL_PHONE",                     CPU_ALL,            0,                                      false, 0.0f,  0.5f,  0.5f,  true);
	SetDrawListInfo(DL_RENDERPHASE_HUD,                     "DL_HUD",                       CPU_DRAWSCENE,      0,                                      false, 0.0f,  0.5f,  0.5f,  true);
	SetDrawListInfo(DL_RENDERPHASE_FRONTEND,                "DL_FRONTEND",                  CPU_ALL,            0,                                      false, 0.0f,  0.5f,  0.5f,  true);
	SetDrawListInfo(DL_RENDERPHASE_PREAMBLE,                "DL_PREAMBLE",                  CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_HEIGHT_MAP,              "DL_HEIGHT_MAP",                CPU_ALL,            0,                                      true,  0.0f,  0.0f,  0.0f,  true);

#if DRAW_LIST_MGR_SPLIT_GBUFFER
	SetDrawListInfo(DL_RENDERPHASE_GBUF,                    "DL_GBUF",                      CPU_ALL,            0,                                      true,  0.0f,  3.0f,  5.0f,  false);
    SetDrawListInfo(DL_RENDERPHASE_GBUF_FIRST,              "DL_GBUF_FIRST",                CPU_ALL,            0,                                      true,  0.0f,  3.0f,  5.0f,  false);
	for(int i=DL_RENDERPHASE_GBUF_000; i<=DL_RENDERPHASE_GBUF_113; i++)
	{
	    char str[256];
	    int idx = i - DL_RENDERPHASE_GBUF_000;
	    sprintf(str, "DL_RENDERPHASE_GBUF_%02d", idx);
	    SetDrawListInfo(i,                                  str,                            CPU_ALL,            0,                                      true,  0.0f,  3.0f,  5.0f,  false);
	}
	SetDrawListInfo(DL_RENDERPHASE_GBUF_AMBT_LIGHTS,        "DL_GBUF_AMBT_LIGHTS",          CPU_LIGHTS,         0,                                      true,  0.0f,  3.0f,  5.0f,  false);
    SetDrawListInfo(DL_RENDERPHASE_GBUF_LAST,               "DL_GBUF_LAST",                 CPU_ALL,            0,                                      true,  0.0f,  3.0f,  5.0f,  false);
#else
	SetDrawListInfo(DL_RENDERPHASE_GBUF,                    "DL_GBUF",                      CPU_LIGHTS,         0,                                      true,  0.0f,  3.0f,  5.0f,  true);
#endif

	SetDrawListInfo(DL_RENDERPHASE_CLOUD_GEN,               "DL_CLOUD_GEN",                 CPU_ALL,            0,                                      false, 0.0f,  0.10f, 0.25f, true);
	SetDrawListInfo(DL_RENDERPHASE_PED_DAMAGE_GEN,          "DL_PED_DAMAGE_GEN",            CPU_ALL,            0,                                      true,  0.0f,  0.25f, 0.25f, true);
	SetDrawListInfo(DL_RENDERPHASE_PLAYER_SETTINGS,         "DL_PLAYER_SETTINGS",           CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_RAIN_UPDATE,             "DL_RAIN_UPDATE",               CPU_ALL,            0,                                      false, 0.0f,  0.25f, 0.5f,  true);
	SetDrawListInfo(DL_RENDERPHASE_RAIN_COLLISION_MAP,      "DL_RAIN_COLLISION_MAP",        CPU_ALL,            0,                                      true,  0.0f,  0.1f,  0.1f,  true);
	SetDrawListInfo(DL_RENDERPHASE_MIRROR_REFLECTION,       "DL_MIRROR_REFLECTION",         CPU_ALL,            LODDrawable::LODFLAG_SHIFT_MIRROR,      true,  0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_DEBUG,                   "DL_DEBUG",                     CPU_DEBUG,          0,                                      false, 0.0f,  0.0f,  0.0f,  true);
#if __BANK
	SetDrawListInfo(DL_RENDERPHASE_DEBUG_OVERLAY,           "DL_DEBUG_OVERLAY",             CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
#endif
	SetDrawListInfo(DL_RENDERPHASE_LENSDISTORTION,          "DL_RENDERPHASE_LENSDISTORTION",CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_BEGIN_DRAW,                          "DL_BEGIN_DRAW",                CPU_SYNC_MAIN,      0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_END_DRAW,                            "DL_END_DRAW",                  CPU_SYNC_MAIN,      0,                                      false, 0.0f,  0.0f,  0.0f,  true);

#else // MULTIPLE_RENDER_THREADS == 4

	SetDrawListInfo(DL_RENDERPHASE,                         "INVALID",                      CPU_SYNC_MAIN,      0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_LIGHTING,                "DL_LIGHTING",                  CPU_LIGHTS,         0,                                      true,  0.0f,  1.0f,  9.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_RADAR,                   "DL_RADAR",                     CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_PRE_RENDER_VP,           "DL_PRE_RENDER_VP",             CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_TIMEBARS,                "DL_TIMEBARS",                  CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_SCRIPT,                  "DL_SCRIPT",                    CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_TREE,                    "DL_TREE",                      CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);

#if DRAW_LIST_MGR_SPLIT_DRAWSCENE
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE,               "DL_DRAWSCENE",					CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_FIRST,			"DL_DRAWSCENE_FIRST",			CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_PRE_ALPHA,     "DL_DRAWSCENE_PRE_ALPHA",		CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_01,		"DL_DRAWSCENE_ALPHA_01",		CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_02,		"DL_DRAWSCENE_ALPHA_02",		CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_03,		"DL_DRAWSCENE_ALPHA_03",		CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_04,		"DL_DRAWSCENE_ALPHA_04",		CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_05,		"DL_DRAWSCENE_ALPHA_05",		CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_06,		"DL_DRAWSCENE_ALPHA_06",		CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_07,		"DL_DRAWSCENE_ALPHA_07",		CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_08,		"DL_DRAWSCENE_ALPHA_08",		CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_09,		"DL_DRAWSCENE_ALPHA_09",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_10,		"DL_DRAWSCENE_ALPHA_10",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_11,		"DL_DRAWSCENE_ALPHA_11",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_12,		"DL_DRAWSCENE_ALPHA_12",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_13,		"DL_DRAWSCENE_ALPHA_13",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_14,		"DL_DRAWSCENE_ALPHA_14",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_15,		"DL_DRAWSCENE_ALPHA_15",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_16,		"DL_DRAWSCENE_ALPHA_16",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_17,		"DL_DRAWSCENE_ALPHA_17",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_18,		"DL_DRAWSCENE_ALPHA_18",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_19,		"DL_DRAWSCENE_ALPHA_19",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_20,		"DL_DRAWSCENE_ALPHA_20",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_21,		"DL_DRAWSCENE_ALPHA_21",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_22,		"DL_DRAWSCENE_ALPHA_22",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_23,		"DL_DRAWSCENE_ALPHA_23",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_ALPHA_24,		"DL_DRAWSCENE_ALPHA_24",        CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_POST_ALPHA,    "DL_DRAWSCENE_POST_ALPHA",      CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_NON_DEPTH_FX,  "DL_DRAWSCENE_NON_DEPTH_FX",    CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_DEBUG,         "DL_DRAWSCENE_DEBUG",           CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE_LAST,			"DL_DRAWSCENE_LAST",			CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
#else
	SetDrawListInfo(DL_RENDERPHASE_DRAWSCENE,               "DL_DRAWSCENE",                 CPU_ALL,            0,                                      true,  0.0f,  3.0f,  6.0f,  false);
#endif

	SetDrawListInfo(DL_RENDERPHASE_WATER_SURFACE,           "DL_WATER_SURFACE",             CPU_ALL,            0,                                      true,  0.0f,  0.5f,  0.5f,  false);
	SetDrawListInfo(DL_RENDERPHASE_WATER_REFLECTION,        "DL_WATER_REFLECTION",          CPU_ALL,            LODDrawable::LODFLAG_SHIFT_WATER,       true,  0.0f,  0.0f,  3.5f,  true);
	SetDrawListInfo(DL_RENDERPHASE_REFLECTION_MAP,          "DL_REFLECTION_MAP",            CPU_ALL,            LODDrawable::LODFLAG_SHIFT_REFLECTION,  true,  0.0f,  4.0f,  4.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_SEETHROUGH_MAP,          "DL_SEETHROUGH_MAP",            CPU_ALL,            0,                                      true,  0.0f,  0.0f,  0.0f,  true);

	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS,      "DL_PARABOLOID_SHADOWS",        CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  !DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS);
#if DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_FIRST,"DL_PARABOLOID_SHADOWS_FIRST",  CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_0,    "DL_PARABOLOID_SHADOWS_0",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_1,    "DL_PARABOLOID_SHADOWS_1",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_2,    "DL_PARABOLOID_SHADOWS_2",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_3,    "DL_PARABOLOID_SHADOWS_3",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_4,    "DL_PARABOLOID_SHADOWS_4",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_5,    "DL_PARABOLOID_SHADOWS_5",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_6,    "DL_PARABOLOID_SHADOWS_6",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_7,    "DL_PARABOLOID_SHADOWS_7",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_PARABOLOID_SHADOWS_LAST, "DL_PARABOLOID_SHADOWS_LAST",   CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
#endif

	SetDrawListInfo(DL_RENDERPHASE_CASCADE_SHADOWS,         "DL_CASCADE_SHADOWS",           CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  !DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS && !DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS);
#if DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS
	SetDrawListInfo(DL_RENDERPHASE_CASCADE_SHADOWS_FIRST,   "DL_CASCADE_SHADOWS_FIRST",     CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_CASCADE_SHADOWS_0,       "DL_CASCADE_SHADOWS_0",         CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_CASCADE_SHADOWS_1,       "DL_CASCADE_SHADOWS_1",         CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_CASCADE_SHADOWS_2,       "DL_CASCADE_SHADOWS_2",         CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_CASCADE_SHADOWS_3,       "DL_CASCADE_SHADOWS_3",         CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_CASCADE_SHADOWS_RVL,     "DL_CASCADE_SHADOWS_REVEAL",    CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_CASCADE_SHADOWS_LAST,    "DL_CASCADE_SHADOWS_LAST",      CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  2.0f,  4.0f,  5.0f,  false);
#endif

	SetDrawListInfo(DL_RENDERPHASE_PHONE,                   "DL_PHONE",                     CPU_ALL,            0,                                      false, 0.0f,  0.5f,  0.5f,  true);
	SetDrawListInfo(DL_RENDERPHASE_HUD,                     "DL_HUD",                       CPU_ALL,            0,                                      false, 0.0f,  0.5f,  0.5f,  true);
	SetDrawListInfo(DL_RENDERPHASE_FRONTEND,                "DL_FRONTEND",                  CPU_ALL,            0,                                      false, 0.0f,  0.5f,  0.5f,  true);
	SetDrawListInfo(DL_RENDERPHASE_PREAMBLE,                "DL_PREAMBLE",                  CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_HEIGHT_MAP,              "DL_HEIGHT_MAP",                CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  0.0f,  0.0f,  0.0f,  true);

#if DRAW_LIST_MGR_SPLIT_GBUFFER
	SetDrawListInfo(DL_RENDERPHASE_GBUF,                    "DL_GBUF",                      CPU_ALL,            0,                                      true,  0.0f,  3.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_GBUF_FIRST,              "DL_GBUF_FIRST",                CPU_ALL,            0,                                      true,  0.0f,  3.0f,  5.0f,  false);
	for(int i=DL_RENDERPHASE_GBUF_000; i<=DL_RENDERPHASE_GBUF_113; i++)
	{
	    char str[256];
	    int idx = i - DL_RENDERPHASE_GBUF_000;
	    sprintf(str, "DL_RENDERPHASE_GBUF_%02d", idx);
	    SetDrawListInfo(i,                                  str,                            CPU_ALL,            0,                                      true,  0.0f,  3.0f,  5.0f,  false);
	}
	SetDrawListInfo(DL_RENDERPHASE_GBUF_AMBT_LIGHTS,        "DL_GBUF_AMBT_LIGHTS",          CPU_LIGHTS,         0,                                      true,  0.0f,  3.0f,  5.0f,  false);
	SetDrawListInfo(DL_RENDERPHASE_GBUF_LAST,               "DL_GBUF_LAST",                 CPU_ALL,            0,                                      true,  0.0f,  3.0f,  5.0f,  false);
#else
	SetDrawListInfo(DL_RENDERPHASE_GBUF,                    "DL_GBUF",                      CPU_LIGHTS,         0,                                      true,  0.0f,  3.0f,  5.0f,  true);
#endif // DRAW_LIST_MGR_SPLIT_GBUFFER

	SetDrawListInfo(DL_RENDERPHASE_CLOUD_GEN,               "DL_CLOUD_GEN",                 CPU_ALL,            0,                                      false, 0.0f,  0.10f, 0.25f, true);
	SetDrawListInfo(DL_RENDERPHASE_PED_DAMAGE_GEN,          "DL_PED_DAMAGE_GEN",            CPU_ALL,            0,                                      true,  0.0f,  0.25f, 0.25f, true);
	SetDrawListInfo(DL_RENDERPHASE_PLAYER_SETTINGS,         "DL_PLAYER_SETTINGS",           CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_RAIN_UPDATE,             "DL_RAIN_UPDATE",               CPU_ALL,            0,                                      false, 0.0f,  0.25f, 0.5f,  true);
	SetDrawListInfo(DL_RENDERPHASE_RAIN_COLLISION_MAP,      "DL_RAIN_COLLISION_MAP",        CPU_ALL,            LODDrawable::LODFLAG_SHIFT_SHADOW,      true,  0.0f,  0.1f,  0.1f,  true);
	SetDrawListInfo(DL_RENDERPHASE_MIRROR_REFLECTION,       "DL_MIRROR_REFLECTION",         CPU_ALL,            LODDrawable::LODFLAG_SHIFT_MIRROR,      true,  0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_DEBUG,                   "DL_DEBUG",                     CPU_ALL,			0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_RENDERPHASE_LENSDISTORTION,          "DL_LENSDISTORTION",            CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
#if __BANK
	SetDrawListInfo(DL_RENDERPHASE_DEBUG_OVERLAY,           "DL_DEBUG_OVERLAY",             CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
#endif // __BANK
	SetDrawListInfo(DL_RENDERPHASE_LENSDISTORTION,          "DL_RENDERPHASE_LENSDISTORTION",CPU_ALL,            0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_BEGIN_DRAW,                          "DL_BEGIN_DRAW",                CPU_SYNC_MAIN,      0,                                      false, 0.0f,  0.0f,  0.0f,  true);
	SetDrawListInfo(DL_END_DRAW,                            "DL_END_DRAW",                  CPU_SYNC_MAIN,      0,                                      false, 0.0f,  0.0f,  0.0f,  true);
#endif // MULTIPLE_RENDER_THREADS == 4

#if RAGETRACE
	gpuDrawListTrace[DL_BEGIN_DRAW].Init(GetDrawListName(DL_BEGIN_DRAW)+3, Color32(0));
	gpuDrawListTrace[DL_END_DRAW].Init(GetDrawListName(DL_END_DRAW)+3, Color32(0));

	cpuDrawListTrace[DL_BEGIN_DRAW] = gpuDrawListTrace[DL_BEGIN_DRAW];
	cpuDrawListTrace[DL_END_DRAW] = gpuDrawListTrace[DL_END_DRAW];
#endif

#if DRAW_LIST_MGR_SPLIT_GBUFFER
	GroupDrawListTimebars(DL_RENDERPHASE_GBUF_FIRST,                DL_RENDERPHASE_GBUF_LAST,               "DL_RENDERPHASE_GBUF");
#endif
#if DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS
	GroupDrawListTimebars(DL_RENDERPHASE_PARABOLOID_SHADOWS_FIRST,  DL_RENDERPHASE_PARABOLOID_SHADOWS_LAST, "DL_RENDERPHASE_PARABOLOID_SHADOWS");
#endif
#if DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS
	GroupDrawListTimebars(DL_RENDERPHASE_CASCADE_SHADOWS_FIRST,     DL_RENDERPHASE_CASCADE_SHADOWS_LAST,    "DL_RENDERPHASE_CASCADE_SHADOWS");
#endif
#if DRAW_LIST_MGR_SPLIT_DRAWSCENE
	GroupDrawListTimebars(DL_RENDERPHASE_DRAWSCENE_FIRST,     DL_RENDERPHASE_DRAWSCENE_LAST,    "DL_RENDERPHASE_DRAWSCENE");
#endif
}

void CDrawListMgr::Shutdown(){

	dlDrawListMgr::Shutdown();
	
	delete gDCBuffer;
	gDCBuffer = NULL;
}

// reset buffer and all list info
void CDrawListMgr::Reset(){
	dlDrawListMgr::Reset();

}

void CDrawListMgr::Update(){

#if __BANK
	DumpDebugData();
#endif // __BANK
}

void CDrawListMgr::ClothCleanup()
{
	for( int i = 0; i < m_Cloths.GetCount(); i++ )
	{
		if( m_Cloths[i]->m_removeRequested == true && m_Cloths[i]->m_refCount == 0 )
		{
			environmentCloth* envCloth = (environmentCloth*)(m_Cloths[i]->m_envCloth);
			Assert(envCloth);
			if(envCloth && envCloth->GetFlag(environmentCloth::CLOTH_IsUpdating) )
				continue;

#if USE_PRE_ALLOC_MEMORY
			Assert( m_Cloths[i]->m_memPtr );
			CPhysics::GetClothManager()->FreeClothVB( m_Cloths[i]->m_memPtr ) ;
#endif
			
			sysMemAllocator& oldAllocator = sysMemAllocator::GetCurrent();
			sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());
			if(envCloth)
			{
				delete envCloth;
			}
			sysMemAllocator::SetCurrent(oldAllocator);

			m_ClothsPool.Delete( m_Cloths[i] );
			m_Cloths.DeleteFast( i );
			i--; // Due to the way the array delete stuff, we need to ensure we stay on the same line.
		}
	}
}

void CDrawListMgr::CharClothCleanup()
{
	for( int i = 0; i < m_CharCloths.GetCount(); i++ )
	{
		if( m_CharCloths[i]->m_removeRequested == true && m_CharCloths[i]->m_refCount == 0 )
		{
			clothVariationData* clothVarData = m_CharCloths[i]->m_clothVarData;
			Assert( clothVarData );

			clothManager* cManager = CPhysics::GetClothManager();
			Assert( cManager );

			clothVarData->Reset();
			cManager->DeallocateVarData( clothVarData );

			m_CharClothsPool.Delete( m_CharCloths[i] );
			m_CharCloths.DeleteFast( i );
			i--; // Due to the way the array delete stuff, we need to ensure we stay on the same line...
		}
	}
}



/*
void DumpMemCB(void){
	dlCmdBase::RequestDumpData();
}
*/


// assume draw lists have finished rendering, so (hopefully) safely remove all references to model infos
void	CDrawListMgr::RemoveAllRefs(u32 flags){
		
	CPedPropRenderGfx::RemoveRefs();
	CPedStreamRenderGfx::RemoveRefs();
	CVehicleStreamRenderGfx::RemoveRefs();

	dlDrawListMgr::RemoveAllRefs(flags);
}


#if !__FINAL
float CDrawListMgr::GetDrawListGPUTime(int drawListIndex) const{
	int first, last;
#if DRAW_LIST_MGR_SPLIT_GBUFFER
	if (drawListIndex == DL_RENDERPHASE_GBUF){
		first = DL_RENDERPHASE_GBUF_FIRST;
		last  = DL_RENDERPHASE_GBUF_LAST;
	}
	else
#endif
#if DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS
	if (drawListIndex == DL_RENDERPHASE_PARABOLOID_SHADOWS){
		first = DL_RENDERPHASE_PARABOLOID_SHADOWS_FIRST;
		last  = DL_RENDERPHASE_PARABOLOID_SHADOWS_FIRST;
	}
	else
#endif
#if DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS
	if (drawListIndex == DL_RENDERPHASE_CASCADE_SHADOWS){
		first = DL_RENDERPHASE_CASCADE_SHADOWS_FIRST;
		last  = DL_RENDERPHASE_CASCADE_SHADOWS_LAST;
	}
	else
#endif
#if DRAW_LIST_MGR_SPLIT_DRAWSCENE
	if (drawListIndex == DL_RENDERPHASE_DRAWSCENE){
		first = DL_RENDERPHASE_DRAWSCENE_FIRST;
		last  = DL_RENDERPHASE_DRAWSCENE_LAST;
	}
	else
#endif
	{
		first = drawListIndex;
		last  = drawListIndex;
	}

	float tot = 0.f;
	for(int i=first; i<=last; ++i){
		tot += dlDrawListMgr::GetDrawListGPUTime(i);
	}
	return tot;
}
#endif // !__FINAL


#if __BANK
// request regeneration of all of the entities DL cache entries
void RegenDLCachesCB(void){
	CBuilding::RegenDLAlphas();
}
#endif //__BANK




/*----------------- widget stuff ----------------*/
#if __BANK


bool CDrawListMgr::SetupWidgets(bkBank& bank)
{
	bank.PushGroup("Draw Lists");
		bank.AddToggle("Show drawlist stats", &bShowDrawlistStats);
		bank.AddToggle("Show drawlist execution", &bShowDrawlistExec);
		bank.AddToggle("Check lists once built", &bCheckListsOnceBuilt);
		//bank.AddButton("Dump out mem useage",DumpMemCB);
		bank.AddButton("Regenerate DL Caches", RegenDLCachesCB);
		DRAW_LIST_MGR_SPLIT_DRAWSCENE_ONLY( bank.AddSlider("Num Alpha Entity lists", &ms_nNumAlphaEntityLists, 1, 4, 1) );

		bank.PushGroup("Draw List Framework");
			dlDrawListMgr::SetupWidgets(bank); // Setup rage draw list widgets
		bank.PopGroup();
	bank.PopGroup();

	return true;
}

void CDrawListMgr::PrintDrawListType(eDrawListType drawListType){

	grcDebugDraw::AddDebugOutput("%s", GetDrawListName(drawListType));
}



void CDrawListMgr::DumpDebugData(void){

	if (bShowDrawlistStats){
		grcDebugDraw::AddDebugOutput("Drawlist stats:");

		grcDebugDraw::AddDebugOutput("Buf max : %d", CDrawCommandBuffer::ms_maxBufSize);
		
		grcDebugDraw::AddDebugOutput("Total draw lists rendered : %d",ms_numDrawlists);
		ms_numDrawlists = 0;

		for(u32 i=0;i<DL_MAX_TYPES;i++){
			grcDebugDraw::AddDebugOutput("%s : %d", GetDrawListName(i), ms_drawListStats[i]);
			ms_drawListStats[i] = 0;
		}
	}
}
#endif //__BANK
