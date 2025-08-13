// Title	:	Renderer.h
// Author	:	Richard Jobling
// Started	:	10/05/99

#ifndef _RENDERER_H_
#define _RENDERER_H_

// Rage headers
#include "vector/vector3.h"
#include "fwutil/xmacro.h"
#include "grcore/config_switches.h"		// for RAGE_SUPPORT_TESSELATION_TECHNIQUES

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
#include "shaderlib/rage_pntriangles.h"
#include "fwdrawlist/drawlist.h"
#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES

// Game headers
#include "Renderer/RenderListGroup.h"

namespace rage
{
	class grcViewport;
	class fwPtrList;
	class fwPtrListSingleLink;
}

#define MAX_SPU_PLANES_MEM		512 //The maximum number of occluders per phase
#define MAX_OCCLUSION_PHASES	8   //How many phases can have occlusion switched off
#define	MAX_NUMBER_OF_TOTAL_PLANES	(2048)

#define RENDER_PRESTREAMDIST (30.0f)		// Distance over which models start getting streamed in

// obviously this would be preferable to put back in - just couldn't quite get some world rep code to compile at the moment...
//#if __BANK
typedef 	bool TweakBankBool;
typedef 	int  TweakBankInt;
/*#else //__BANK
typedef 	const bool TweakBankBool;
typedef 	const int  TweakBankInt;
#endif //__BANK*/


class	CEntity;
namespace rage {
class fwEntityDesc;
}

extern	void DebugEntity(CEntity*,const char* fmt, ...);

//get rid of these asap (LOL, KS WAS HERE - removed 1 13/07/2011)

extern	bool		g_multipass_list_gen;
extern	int			g_alternativeViewport;
extern	bool		g_debug_heightmap;
extern	bool		g_SortRenderLists;
extern	bool		g_SortTreeList;

#if __BANK
extern bool g_HideDebug;
extern bool g_DrawCloth;
extern bool g_DrawPeds;
extern bool g_DrawVehicles;
extern bool	g_EnableSkipRenderingShaders;
extern bool g_SkipRenderingGrassFur;
extern bool g_SkipRenderingTrees;
#endif //__BANK

class CParaboloidShadowActiveEntry;
class CRenderPhase;
class CRenderPhaseScanned;
class CEntity;
class CFrustumClip;
class CPhysical;
class CObject;
class CPortalInst;
class CBaseModelInfo;
class CSector;
class CRepeatSector;
class CDynamicEntity;
class camFrustum;
class CRenderPhase;

#define HAS_RENDER_MODE_SHADOWS (!__PS3) // TODO -- this kind of sucks that we can't use rmShadows on PS3, can we fix this? (see BS#315353)

enum eRenderMode {
	rmNIL						= 0x0000,
	rmStandard					= 0x0001,
	rmGBuffer					= 0x0002,
	rmSimple					= 0x0004,
	rmSimpleNoFade				= 0x0008,
	rmSimpleDistFade			= 0x0010,
#if HAS_RENDER_MODE_SHADOWS
	rmShadows					= 0x0020,
	rmShadowsSkinned			= 0x0040,
#endif // HAS_RENDER_MODE_SHADOWS
	rmSpecial					= 0x0080,
	rmSpecialSkinned			= 0x0100,
	rmNoLights					= 0x0200,
#if __BANK
	rmDebugMode					= 0x0400,
#endif // __BANK
};

inline bool IsRenderingModes(eRenderMode rendermode, u32 modes)
{
	FastAssert(rendermode != rmNIL);
	return(0 != ((u32)rendermode & modes));
}

inline bool IsNotRenderingModes(eRenderMode rendermode, u32 modes)
{
	FastAssert(rendermode != rmNIL);
	return(0 == ((u32)rendermode & modes));
}

#if HAS_RENDER_MODE_SHADOWS

inline bool IsRenderingShadows(eRenderMode rendermode)
{
	FastAssert(rendermode != rmNIL);
	return(0 != (rendermode & (rmShadowsSkinned | rmShadows)));
}

inline bool IsRenderingShadowsNotSkinned(eRenderMode rendermode)
{
	FastAssert(rendermode != rmNIL);
	return((rendermode == rmShadows));
}

inline bool IsRenderingShadowsSkinned(eRenderMode rendermode)
{
	FastAssert(rendermode != rmNIL);
	return(rendermode == rmShadowsSkinned);
}

#define HAS_RENDER_MODE_SHADOWS_ONLY(x) x
#else
#define HAS_RENDER_MODE_SHADOWS_ONLY(x)
#endif

enum eRenderGeneralFlags // would call this "eRenderFlags", but fwRenderFlags is already being used
{
	RENDERFLAG_CUTOUT_USE_SUBSAMPLE_ALPHA = 0x01,
	RENDERFLAG_CUTOUT_FORCE_CLIP          = 0x02,
	RENDERFLAG_ONLY_FORCE_ALPHA           = 0x04,
	RENDERFLAG_SKIP_FORCE_ALPHA           = 0x08,
	RENDERFLAG_PUDDLE                     = 0x10,
	RENDERFLAG_BEGIN_RENDERPASS_QUERY     = 0x20,
	RENDERFLAG_END_RENDERPASS_QUERY       = 0x40,
};

class CSpuDrawEntity;

#if __XENON
class CGPRCounts
{
public:
	// These are all pixel shader reg counts, just do 128-PsRegCount to get VS reg count
	static BankInt32 ms_DefaultPsRegs;
	static BankInt32 ms_CascadedGenPsRegs;
	static BankInt32 ms_CascadedRevealPsRegs;
	static BankInt32 ms_EnvReflectGenPsRegs;
	static BankInt32 ms_EnvReflectBlurPsRegs;
	static BankInt32 ms_GBufferPsRegs;
	static BankInt32 ms_DefLightingPsRegs;
	static BankInt32 ms_WaterFogPrepassRegs;
	static BankInt32 ms_WaterCompositePsRegs;
	static BankInt32 ms_DepthFXPsRegs;
	static BankInt32 ms_PostFXPsRegs;
};
#endif

class CRenderer
{
friend class CPortal;
friend class CSpuWorldScan;
public:
	enum VisibilityState {
		ENTITY_INVISIBLE,
		ENTITY_VISIBLE,
		ENTITY_OFFSCREEN,
		ENTITY_STREAM_MODEL
	};
	
	enum RenderBucketTypes
	{
		RB_OPAQUE					= 0,
		RB_ALPHA					= 1,
		RB_DECAL					= 2,
		RB_CUTOUT					= 3,
		RB_NOSPLASH					= 4, // used by boats interiors 
		RB_NOWATER					= 5, // used by boats interiors
		RB_WATER					= 6,
		RB_DISPL_ALPHA				= 7, // displacement alpha

		RB_NUM_BASE_BUCKETS			= 8,

		RB_MODEL_DEFAULT			= 8,	// Render me in gbuf/drawscene
		RB_MODEL_SHADOW				= 9,	// Render me in shadow
		RB_MODEL_REFLECTION			= 10,	// Render me in paraboloid reflection
		RB_MODEL_MIRROR				= 11,	// Render me in mirror reflection
		RB_MODEL_WATER				= 12,	// Render me in water reflection
		RB_UNUSED_13				= 13,

		// only 16 bit supported on ps3. for now, but let's be reasonable for a minute.
		// LODGROUP LOD MASK	= 16, see lodgroup.h
		RB_HIGHEST_BUCKET
	};

	static __forceinline u32 GenerateBucketMask(u32 baseBucket) { return BUCKETMASK_GENERATE(baseBucket); }
	static __forceinline u32 GenerateBucketMask(u32 baseBucket, u32 subBucketMask) { return BUCKETMASK_GENERATE_WITH_SUB(baseBucket, subBucketMask); }
	static __forceinline u32 PackBucketMask(u32 baseBucket, u32 subBucketMask) { return (subBucketMask & 0xff00) | (baseBucket & 0xff); }

	static __forceinline bool IsBaseBucket(u32 bucketMask, u32 baseBucket) { return (bucketMask&0xff) == (GenerateBucketMask(baseBucket)&0xff); }
	static __forceinline u32 GetBaseBucket(u32 bucketMask) { return (bucketMask&0x00ff); }
	static __forceinline u32 GetSubBucketMask(u32 bucketMask) { return (bucketMask & 0xff00) >> 8; }

	static __forceinline u32 GenerateSubBucketMask(u32 bucket) { return (0x1 << (bucket - CRenderer::RB_NUM_BASE_BUCKETS)); }

public:

	static void PreLoadingScreensInit();
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

#if __BANK
	static void InitWidgets();
	static void AddWidgets();

	static bool ms_isSnowing;

	static void PlantsToggleCB();
	static void PlantsDisplayDebugInfoCB();
	static void PlantsAlphaOverDrawCB();

	static bool ms_bPlantsAllEnabled;
	static bool ms_bPlantsDisplayDebugInfo;
	static bool ms_bPlantsAlphaOverdraw;
#endif

	static void ConstructRenderListNew(u32 renderFlags, CRenderPhaseScanned *renderPhase);
	static void PreRender();
	
	static void ClearRenderLists();


	static	void FillSPUBuildRenderlistInfo(CRenderPhase **p_rparray,int PhaseCount);

	static int GetRenderListVisibleCount(int list);

#if !__FINAL
	static void ToggleRenderCollisionGeometry();
#endif
	static float GetFarClip() {return ms_fFarClipPlane;}

#if __PS3
	static float SetDepthBoundsFromRange(float nearClip, float farClip, float nearZ, float farZ, bool excludeSky=true);
#endif

	static	void SetCanRenderFlags(u32 canAlpha, u32 canWater, u32 canScreenDoor,u32 canDecal,u32 canPreRender) {ms_canRenderDecal=canDecal;ms_canRenderWater=canWater;ms_canRenderScreenDoor=canScreenDoor;ms_canRenderAlpha=canAlpha;ms_canPreRender=canPreRender;}
	static	void OrCanRenderFlags(u32 canAlpha,u32 canWater, u32 canScreenDoor,u32 canDecal,u32 canPreRender) {ms_canRenderDecal|=canDecal;ms_canRenderWater|=canWater;ms_canRenderScreenDoor|=canScreenDoor;ms_canRenderAlpha|=canAlpha;ms_canPreRender|=canPreRender;}
	static	u32  GetCanRenderAlphaFlags()  {return ms_canRenderAlpha;}
	static	u32  GetCanRenderWaterFlags()  {return ms_canRenderWater;}
	static	u32  GetCanRenderScreenDoorFlags()  {return ms_canRenderScreenDoor;}
	static	u32  GetCanRenderDecalFlags()  {return ms_canRenderDecal;}
	static	u32  GetCanPreRenderFlags()  {return ms_canPreRender;}

#if __XENON
	static int GetBackbufferColorExpBiasHDR(){return sm_backbufferColorExpBiasHDR;}
	static int GetBackbufferColorExpBiasLDR(){return sm_backbufferColorExpBiasLDR;}
#endif

	static void IssueAllScanTasksNew();

	static void PreRenderEntities();

	static bool TwoPassTreeNormalFlipping();
	static bool DoubleSidedTreesCutoutOptimization();
	static bool TwoPassTreeRenderingEnabled();
	static bool TwoPassTreeShadowRenderingEnabled();

	static bool UseDelayedGbuffer2TileResolve();

	static int GetSceneToGBufferListBits()  {return m_SceneToGBufferBits;}
	static void SetSceneToGBufferListBits(u32 bits) {m_SceneToGBufferBits=bits;}

	static int GetMirrorListBits()  {return m_MirrorBits;}
	static void SetMirrorListBits(u32 bits) {m_MirrorBits=bits;}

	//The SPU wants to know which phases are shadow phases, these are special in that they ignore the alpha fade except they only draw if alpha is 255
	static int GetShadowPhases()  {return m_ShadowPhases;}
	static void SetShadowPhases(u32 bits) {m_ShadowPhases=bits;}
	static void OrInShadowPhases(u32 bits) {m_ShadowPhases|=bits;}

#if __BANK
	static int GetDebugPhases()  {return m_DebugPhases;}
	static void SetDebugPhases(u32 bits) {m_DebugPhases=bits;}
	static void OrInDebugPhases(u32 bits) {m_DebugPhases|=bits;}
#endif // __BANK

	static void UpdateScannedVisiblePortal(CPortalInst* portalInst);
	static s32 GetScannedVisiblePortalInteriorProxyID();
	static void SetScannedVisiblePortalInteriorProxyID(s32 id);
	static s32 GetScannedVisiblePortalInteriorProxyMainTCModifier();
	static void SetScannedVisiblePortalInteriorProxyMainTCModifier(s32 id);
	static s32 GetScannedVisiblePortalInteriorProxySecondaryTCModifier();
	static void SetScannedVisiblePortalInteriorProxySecondaryTCModifier(s32 id);
	
	static const grcViewport& FixupScannedVisiblePortalViewport(const grcViewport& viewport);
	static void ResetScannedVisiblePortal();
	
	static float GetPreStreamDistance()	{return ms_PreStreamDistance;}

	static void SetCameraHeading(float cameraHeading)		{ ms_fCameraHeading = cameraHeading; }
	static void SetFarClipPlane(float farClipPlane)			{ ms_fFarClipPlane = farClipPlane; }

	static void SetThisCameraPosition(Vector3::Vector3Param camPos)		{ ms_ThisCameraPosition = camPos; }
	static void SetPrevCameraPosition(Vector3::Vector3Param camPos)		{ ms_PrevCameraPosition = camPos; }

	static Vector3 GetThisCameraPosition()					{ return ms_ThisCameraPosition; }

	static float GetPrevCameraVelocity()					{ return ms_PrevCameraVelocity; }
	static void SetPrevCameraVelocity(float velocity)		{ ms_PrevCameraVelocity = velocity; }

	static void SetPreStreamDistance(float distance)		{ ms_PreStreamDistance = distance; }

	static bool GetDisableArtificialLights()			{ return sm_disableArtificialLights; }
	static void SetDisableArtificialLights(bool b)		{ sm_disableArtificialLights = b; }

	static bool GetDisableArtificialVehLights()			{ return sm_disableArtificialVehLights; }
	static void SetDisableArtificialVehLights(bool b)	{ sm_disableArtificialVehLights = b; }

#if __BANK
	static void PrintNumShadersRendered(s32 list, const char *phaseName);
#if RSG_DURANGO
	static void PrintFrameStatistic();
#endif // RSG_DURANGO
#endif

protected:
	static Vector3	ms_PrevCameraPosition;
	static Vector3	ms_ThisCameraPosition;
	static float	ms_PreStreamDistance;
	static float	ms_fCameraHeading;
	static float	ms_fFarClipPlane;

	static bool		m_loadingPriority;
	static bool		ms_bInTheSky;
public:

	static	float   ms_PrevCameraVelocity;
#if !__FINAL
	static bool		sm_bRenderCollisionGeometry;
#endif

	static	const	int MAX_LODS_WITH_UNLOADED=2048;
	static	const   int RENDERER_MAX_MLO_ENTITIES=1024;
	static	const   int MAX_LOD_ENTITIES=512;

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	static void		SetTessellationQuality(int quality);
	static bool		IsTessellationEnabled();
	static void		SetTessellationVars(const grcViewport &viewport, const float aspect);
#endif // RAGE_SUPPORT_TESSELLATION_TECHNIQUES

# if !__FINAL
	static void SetTerrainTintGlobals();
#endif // !__FINAL

protected:

	static	int		m_SceneToGBufferBits;
	static	int		m_MirrorBits;
	static	int		m_ShadowPhases;
#if __BANK
	static	int		m_DebugPhases;
#endif // __BANK

	static	u32		ms_canRenderAlpha;
	static	u32		ms_canRenderWater;
	static	u32		ms_canRenderScreenDoor;
	static	u32		ms_canRenderDecal;
	static	u32		ms_canPreRender;

	static float		ms_bestScannedVisiblePortalArea;
	static grcViewport	ms_bestScannedVisiblePortalViewport;
	static s32			ms_bestScannedVisiblePortalIntProxyID;
	static s32			ms_bestScannedVisiblePortalMainTCModifier;
	static s32			ms_bestScannedVisiblePortalSecondaryTCModifier;

#if __XENON
	static int sm_backbufferColorExpBiasHDR;
	static int sm_backbufferColorExpBiasLDR;
#endif

	static bool sm_disableArtificialLights;
	static bool sm_disableArtificialVehLights;

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	class TessellationVars: public rage::PNTriangleTessellationGlobals
	{
	public:
		TessellationVars() : PNTriangleTessellationGlobals() {}
		~TessellationVars() {}

		void SetGlobalVarV4(grcEffectGlobalVar var, const Vec4V &value)
		{
			DLC_SET_GLOBAL_VAR(var, value);
		}

		void SetGlobalVarV4Array(grcEffectGlobalVar var, const Vec4V *paValues, int iCount)
		{
			DLC_SET_GLOBAL_VAR_ARRAY(var, paValues, iCount);
		}
	};

	static int ms_WheelTessellationFactor;
	static int ms_TreeTessellationFactor;
	static float ms_TreeTessellation_PNTriangle_K;

	static float ms_LinearNear;
	static float ms_LinearFar;
	static float ms_ScreenSpaceErrorInPixels;
	static float ms_FrustumEpsilon;
	static float ms_BackFaceEpsilon;
	static bool  ms_CullEnable;
	static int ms_MaxTessellation;
	static TessellationVars ms_TessellationVars;
#if !__FINAL
	static bool ms_DisablePOM;
	static bool m_POMDistanceFade;
	static bool ms_VisPOMDistanceFade;
	static bool ms_VisPOMVertexHeight;

	static float ms_POMNear;
	static float ms_POMFar;
	static float ms_POMForwardOffset;
	static float ms_POMMinSamp;
	static float ms_POMMaxSamp;

	static grcEffectGlobalVar ms_POMValuesVar;
	static grcEffectGlobalVar ms_POMValues2Var;

	static void SetDisplacementGlobals();
#endif // !__FINAL

	static float ms_GlobalHeightScale;
	static grcEffectGlobalVar ms_GlobalHeightScaleVar;

#if !__FINAL || RSG_PC
	static grcEffectGlobalVar ms_POMFlagsVar;
#endif
#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	
#if !__FINAL
	static float ms_TerrainTintBlendNear; 
	static float ms_TerrainTintBlendFar;

	static grcEffectGlobalVar ms_TerrainTintBlendValuesVar;
#endif 
};

inline s32 CRenderer::GetScannedVisiblePortalInteriorProxyID()
{
	return ms_bestScannedVisiblePortalIntProxyID;
}

inline void CRenderer::SetScannedVisiblePortalInteriorProxyID(s32 id)
{
	ms_bestScannedVisiblePortalIntProxyID = id;
}

inline s32 CRenderer::GetScannedVisiblePortalInteriorProxyMainTCModifier()
{
	return ms_bestScannedVisiblePortalMainTCModifier;
}

inline void CRenderer::SetScannedVisiblePortalInteriorProxyMainTCModifier(s32 id)
{
	ms_bestScannedVisiblePortalMainTCModifier = id;
}

inline s32 CRenderer::GetScannedVisiblePortalInteriorProxySecondaryTCModifier()
{
	return ms_bestScannedVisiblePortalSecondaryTCModifier;
}

inline void CRenderer::SetScannedVisiblePortalInteriorProxySecondaryTCModifier(s32 id)
{
	ms_bestScannedVisiblePortalSecondaryTCModifier = id;
}

#endif


