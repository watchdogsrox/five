// ====================================
// renderer/shadows/paraboloidshadows.h
// (c) 2010 RockstarNorth
// ====================================

#ifndef _RENDERER_SHADOWS_PARABOLOIDSHADOWS_H_
#define _RENDERER_SHADOWS_PARABOLOIDSHADOWS_H_

#include "fwscene/world/InteriorLocation.h"
#include "fwutil/idgen.h"
#include "renderer/renderphases/renderphase.h" // for eDeferredShadowType
#include "renderer/Shadows/Shadows.h"
#include "renderer/Shadows/ShadowTypes.h"

#include "renderer/Shadows/ParaboloidShadows_shared.h" // shared defines with the shader
	 
#define MAX_PARABOLOID_SHADOW_RENDERPHASES 			(8)		// number of paraboloid shadow renderphases

#if USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS
	#define USE_TEXTURE_ARRAYS_ONLY(x) x
#else	
	#define USE_TEXTURE_ARRAYS_ONLY(x)
#endif


#define SHADOW_FLAG_DONT_RENDER				(0x00) 
#define SHADOW_FLAG_ENABLED					(0x01)
#define SHADOW_FLAG_STATIC					(0x02)  
#define SHADOW_FLAG_DYNAMIC					(0x04)  
#define SHADOW_FLAG_STATIC_AND_DYNAMIC		(SHADOW_FLAG_STATIC|SHADOW_FLAG_DYNAMIC) 
#define SHADOW_FLAG_STATIC_AND_DYNAMIC_MASK (SHADOW_FLAG_STATIC|SHADOW_FLAG_DYNAMIC) 
#define SHADOW_FLAG_ENTITY_EXCLUSION		(0x08)
#define SHADOW_FLAG_EXTERIOR				(0x10)
#define SHADOW_FLAG_INTERIOR				(0x20)
#define SHADOW_FLAG_LATE_UPDATE				(0x40)
#define SHADOW_FLAG_CUTSCENE				(0x100)  
#define SHADOW_FLAG_DELAY_RENDER			(0x200)  // Fake cutscene light, don't really need to render this, just used for visibility check
#define SHADOW_FLAG_HIGHRES					(0x400)
#define SHADOW_FLAG_REFRESH_CHECK_ONLY		(0x800)  // is static only, and it already has the right size cache, so just check later if it needs a refresh, if there is room

#define CACHE_STATUS_UNUSED					(0) // no scene light is using this cache
#define CACHE_STATUS_NOT_ACTIVE				(1) // the light using this cache is still around, but is not one of the active lights
#define CACHE_STATUS_REFRESH_CANDIDATE		(2) // it is associated with an active light, and should be checked so see if it need be generated
#define CACHE_STATUS_GENERATING				(3) // it is associated with an active light, and needs to be generated

#define CACHE_SPOT_TEX_INDEX				(6)  // the index in the cache textures array for the higher res spot texture

#define PARABOLOID_SHADOW_SPOT_RADIUS_DELTA	(ScalarVConstant<FLOAT_TO_INT(0.05f)>())
#define PARABOLOID_SHADOW_SPOT_POSITION_DELTA (ScalarV(V_HALF))
#define PARABOLOID_SHADOW_SPOT_ANGLE_DELTA	(ScalarVConstant<FLOAT_TO_INT(0.2f)>())  // this need to be large enough to push the planes out so they inscribe the cone. so 1/(cos((180/planes)*(1/2)) = 1.0195, and we rounded to .2

#define CACHE_USE_MOVE_ENGINE               (0 && RSG_DURANGO)

class CParaboloidShadowInfo : public CShadowInfo
{
public:
	__forceinline void Init()
	{
		m_type             = SHADOW_TYPE_POINT;
		m_tanFOV           = 1.0f;
		m_shadowDepthRange = 16.0f;
		m_depthBias		   = 0.001f;
		m_slopeScaleDepthBias = 2.0f;

		// not set: m_shadowMatrix
		// not set: m_shadowMatrixP
	}
};

class CParaboloidShadowActiveEntry
{
public:
	CParaboloidShadowInfo m_info;

	Vector3 m_pos;
	Vector3 m_dir;
	Vector3 m_tandir;
	fwUniqueObjId m_trackingID;
	u16     m_flags;
	s16     m_staticCacheIndex;
	s16     m_activeCacheIndex;

	fwInteriorLocation	m_interiorLocation; 

	__forceinline void Init()
	{
		m_info.Init();

		m_pos               = Vector3(0,0,0);
		m_dir               = Vector3(0,0,0); // was 0,0,1
		m_tandir            = Vector3(0,0,0); // was 0,1,0
	
		m_trackingID        = 0;
		m_flags             = SHADOW_FLAG_DONT_RENDER;

		m_staticCacheIndex	= -1;
		m_activeCacheIndex = -1;

		m_interiorLocation.MakeInvalid();
	}

	void Set(const CLightSource& light, u16 flags, bool bEnabled, const fwInteriorLocation interiorLocation);
	void SetInfo(const CLightSource& light);
	void SetCaches(u16 activeCache, u16 staticCache);
	void SetPosDir(const CLightSource& light);
};

class CParaboloidShadowCachedEntry
{
public:
	CParaboloidShadowInfo m_info;

	fwUniqueObjId m_trackingID;
	float m_priority;			// determines if a cache entry should be replaced by better candidate, we could probably make this a s16
	u16   m_ageSinceUsed;
	u16   m_ageSinceUpdated;
	s16   m_lightIndex;
	s16   m_linkedEntry;		// if we're hires this points to the low res entry we used to have and vice versa
	u32   m_staticCheckSum;		// check sum of the model hashes for the visible static geometry
	u16	  m_staticVisibleCount;
	u8	  m_status;
#if RSG_PC
	u8	  m_gpucounter;
	u8	  m_gpucounter_static;
#else
	u8	  m_dummy;
#endif

	int	  m_renderTargetIndex;	 // index into texture array

#if !USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS
	atArray<grcRenderTarget*>  *m_renderTarget[2];			
#else
	grcRenderTarget*			m_renderTarget[2];		 // 0 cube map 1 spot light texture array pointer
#endif

	__forceinline void Init()
	{
		m_info.Init();
		m_ageSinceUsed = 0;
		m_ageSinceUpdated = 65535;
		m_trackingID   = 0;
		m_status       = CACHE_STATUS_UNUSED;
		m_lightIndex   = -1;
		m_staticCheckSum = (u32)~0x0;
		m_staticVisibleCount = 255;
		m_linkedEntry = -1;

#if RSG_PC
		m_gpucounter = 0;
		m_gpucounter_static = 0;
#else
		m_dummy = 0;
#endif	
		m_renderTargetIndex = 0;
		m_renderTarget[0] = NULL;
		m_renderTarget[1] = NULL;
	}
};

class CParaboloidShadow
{
public:


	enum eColorWriteModes {eColorWriteNone, eColorWriteRGBA, eColorWriteRG, eColorWriteBA, eColorWriteDefault, eColorWriteModeCount};
	enum eCullModes {eCullModeFront, eCullModeBack, eCullModeNone, eCullModeCount};
	enum eDepthModes {eDepthModeDisable, eDepthModeEnableAlways, eDepthModeEnableCompare, eDepthModeCount};
	
	static void Init();
	static void Terminate();
	static void CreateBaseMaps(int res, int bpp);

	static int GetShadowRes();
#if RSG_PC
	static void DeviceLost();
	static void DeviceReset();
#endif
	static void InitShaders(const char* path);
	static void Update();
	static void PreRender();
	static void UpdateAfterPreRender();
	static void BeginRender();
	static void EndRender();
	static Mat44V_Out GetFacetFrustumLRTB(int s, int facet);
	static void SetDefaultStates();
	static void SetColorWrite(CParaboloidShadow::eColorWriteModes mode);
	static void SetCullMode(CParaboloidShadow::eCullModes mode);
	static void SetDepthMode(CParaboloidShadow::eDepthModes mode);
	static void EndShadowZPass();
	static void BeginShadowZPass(int s, int facet);
	static void SetShadowZShaderParamsPoint(const CShadowInfo& info, int facet);
	static void SetShadowZShaderParamsSpot(const CShadowInfo& info);
	static void SetShadowZShaderParamData(const CShadowInfo& info);
	static int UpdateLight(const CLightSource& light);
	static eLightShadowType GetShadowType(const CLightSource& light);
	static int GetLightShadowIndex(const CLightSource &light, bool visibleLastFrame);
	static bool IsShadowIndexStaticOnly(int index) {return index>=MAX_ACTIVE_PARABOLOID_SHADOWS;}
	static void UseLightGroup(int numActive);
	static eDeferredShadowType GetDeferredShadowType(int s);
	static grcRenderTarget* GetCacheEntryRT(int cacheIndex, bool getSpotTarget);
	static int GetCacheEntryRTIndex(int cacheIndex);
	static grcRenderTarget* SetDeferredShadow(int s, float blur);
	static void SetForwardLightShadowData(int shadowIndices[], int numLights);

	static void GetCMShadowFacetMatrix(Mat34V_InOut mtx, int facet, const CShadowInfo* info=NULL);
	static void CalcViewProjMtx(Mat44V_InOut viewProjMtx, const CShadowInfo &info, int facet);
	static void CalcViewProjMtx(Mat44V_InOut viewProjMtx, Mat44V_ConstRef projMtx, Mat44V_ConstRef viewMatrix);
	static void CalcViewProjMtx(Mat44V_InOut viewProjMtx, Mat44V_ConstRef projMtx, Mat34V_ConstRef shadowMatrix);
	static void CalcViewMtx(Mat44V_InOut viewMtx, Mat34V_ConstRef shadowMatrix);
	static void CalcViewMtx(Mat44V_InOut viewMtx, const CShadowInfo &info, int facet);
	
	static int GetFacetCount(eLightShadowType type) { return (type == SHADOW_TYPE_SPOT) ? 1 : (type == SHADOW_TYPE_HEMISPHERE) ? 5: 6;}

	static CShadowInfo& GetShadowInfo(int s);
	static const CShadowInfo& GetShadowInfoAndCacheIndex(int s, int &cacheIndex);

	static int  GetCacheStaticTextureSize();
	static bool IsCacheEntryHighRes(int cacheIndex);
	static bool IsStaticCacheEntry(int cacheIndex) {return cacheIndex>=0 && cacheIndex < MAX_STATIC_CACHED_PARABOLOID_SHADOWS;}
	static int  GetProjectionMtxIndex(eLightShadowType type, int facet) { return  (type==SHADOW_TYPE_HEMISPHERE && facet!=CShadowInfo::SHADOW_FACET_FRONT) ? 1 : 0;}// the non "front" facets of the cubemap is "squished" to improve text usage

	static void Synchronise();
	static int  GetRenderBufferIndex() {return m_RenderBufferIndex;}
	static int  GetUpdateBufferIndex() {return m_RenderBufferIndex ^ 0x1;}

	static void UpdateActiveCachesfromStatic();

	static void CalcPointLightMatrix(int s);
	static void CalcSpotLightMatrix(int s);
	static void CheckAndUpdateMovedLight( int s );

	static void SetupHemiSphereVP(grcViewport &VP, const CShadowInfo& info, int facet);
	static bool	TestScreenSpaceBoundsCommon(atRangeArray<Vec3V,9> &pointList, Mat44V_ConstRef viewProj, int facet, int BANK_ONLY(s));
	static void	SetConeScreenVisibility( CShadowInfo& info, int s);
	static void	SetCMFacetScreenSpaceVisibility( CShadowInfo& info, int facetCount, int s);
	static int 	GetFreeStaticCacheIndex(float priority, bool hires=false);
	static void InvalidateCacheEntry(int cacheIndex);
	static void FetchCacheMap(int cacheIndex, int destIndex, int facet, bool isSpotLight, bool waitForCopy);
	static bool DidLightMove(const CParaboloidShadowInfo & info, const CLightSource & sceneLight);
	static bool UsingAsyncStaticCacheCopy();
	static void InsertWaitForAsyncStaticCacheCopy();
	static void FlushStaticCacheCopy();
	
	static void RequestJumpCut() { m_ScriptRequestedJumpCut = true; }
#if !__PS3
	static grcRenderTarget * GetShadowDepthTarget(int cacheIndex, int faceIndex);
#endif

#if __BANK
	static void BankAddWidgets(bkBank& bk);
	static void BankUpdate();
	static void BankDebugDraw();
	static void ShadowBlit(
		grcRenderTarget* dst,
		const grcTexture* src,
		int				 face,
		float            x1,
		float            y1,
		float            x2,
		float            y2,
		float            z,
		float            u1,
		float            v1,
		float            u2,
		float            v2,
		grcEffectTechnique techniqueId
		);
#endif

	static CParaboloidShadowActiveEntry m_activeEntries[MAX_ACTIVE_PARABOLOID_SHADOWS];
	static CParaboloidShadowCachedEntry m_cachedEntries[MAX_CACHED_PARABOLOID_SHADOWS];

	enum {kShadowHiresCubeMapTargets=0, kShadowHiresSpotTargets=1, kShadowLoresCubeMapTargets=2, kShadowLoresSpotTargets=3, kShadowTargetArrayCount};

	// if we're using texture arrays, m_RenderTargets[] is an array of texture that are arrays. if we are not using texture arrays, it's an array of arrays of textures 
#if !USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS
	static atArray<grcRenderTarget*>   *m_RenderTargets[kShadowTargetArrayCount];
	static grcTexture*					m_DummyCubemap;
#else
	static grcRenderTarget*				m_RenderTargets[kShadowTargetArrayCount];		// texture arrays for hi/lo res spot/cubemaps
#endif

	static grmShader*                  	m_ShadowZShaderPointCM;
	static grmShader*                   m_ShadowSmartBlitShader;
	static s32                          m_ParaboloidDepthPointTexGroupId;
	static s32                          m_ParaboloidDepthPointTexTextureGroupId;
#if USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS
	static grcEffectGlobalVar           m_shadowTextureCacheId[kShadowTargetArrayCount];
#else
	static grcEffectGlobalVar           m_shadowTextureCacheId[LOCAL_SHADOWS_MAX_FORWARD_SHADOWS];
	static grcEffectGlobalVar           m_shadowCMTextureCacheId[LOCAL_SHADOWS_MAX_FORWARD_SHADOWS];
#endif
	static grcEffectGlobalVar			m_LocalDitherTextureId;
	static grcEffectGlobalVar           m_shadowParam0_ID;
	static grcEffectGlobalVar			m_shadowParam1_ID;
	static grcEffectGlobalVar           m_shadowMatrix_ID;
	static grcEffectGlobalVar           m_shadowRenderData_ID;
	static grcEffectGlobalVar			m_shadowTexParam_ID;
	static grcEffectVar					m_cubeMapFaceId;
	static grcEffectVar                 m_SmartBlitTextureId;
	static grcEffectVar                 m_SmartBlitCubeMapTextureId;

#if RSG_PC
	static bool							m_initialized;
	static bool							m_useShaderCopy;
	static grcEffectTechnique			m_copyTechniqueId;
#endif

#if __BANK
	static grcEffectTechnique           m_copyDebugTechniqueId;
	static grcEffectTechnique           m_copyCubeMapDebugTechniqueId;
#endif

	static int                          m_cacheUpdateSequence;
	static int                          m_shadowMapBaseRes;
	static int                          m_shadowMapBaseBpp;
	static grcTextureFormat             m_shadowMapBaseFormat;
	static int							m_RenderBufferIndex;
	static bool							m_EnableForwardShadowsOnGlass;
	static bool							m_ScriptRequestedJumpCut;
	static grcRasterizerStateHandle		m_state_R[eCullModeCount];  // 2 versions Back face and front face culling
	static grcBlendStateHandle			m_state_B[eColorWriteModeCount];  // 5 versions: none, Write RGBA, Wright RG and Write BA, default
	static grcDepthStencilStateHandle	m_state_DSS[eDepthModeCount];

#if CACHE_USE_MOVE_ENGINE
	static UINT64						sm_AsyncDMAFenceD3D;
#elif DEVICE_GPU_WAIT
	static grcFenceHandle				sm_AsyncDMAFence;
#endif

#if __BANK
	static bool                         m_debugShowStaticEntries;
	static bool                         m_debugShowActiveEntries;
	static float                        m_debugShowShadowBufferCacheInfoAlpha;
	static bool                         m_debugShowShadowBufferOldVis;
	static int                          m_debugCacheIndexStart;
	static int                          m_debugCacheIndexCount;
	static int                          m_debugDisplaySize;
	static int                          m_debugDisplaySpacing;
	static int                          m_debugDisplayBoundsX0;
	static int                          m_debugDisplayBoundsY0;
	static int                          m_debugDisplayBoundsX1;
	static int                          m_debugDisplayBoundsY1;
	static bool                         m_debugOmniOverride;
	static int                          m_debugOmniNumSamples;
	static bool                         m_debugRegenCache;
	static int                          m_debugDisplayScreenArea;
	static int                          m_debugDisplayFacetScreenArea;
	static float						m_debugScaleScreenAreaDisplay;
	static bool							m_debugParaboloidShadowRenderCullShape;
	static bool							m_debugParaboloidShadowTightSpotCullShapeEnable;
	static ScalarV						m_debugParaboloidShadowTightSpotCullShapePositionDelta;
	static ScalarV						m_debugParaboloidShadowTightSpotCullShapeRadiusDelta;
	static ScalarV						m_debugParaboloidShadowTightSpotCullShapeAngleDelta;

#endif // __BANK

};

#endif // _RENDERER_SHADOWS_PARABOLOIDSHADOWS_H_
