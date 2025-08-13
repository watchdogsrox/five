// ======================================
// renderer/shadows/paraboloidshadows.cpp
// (c) 2010 RockstarNorth
// ======================================

#include "fwutil/xmacro.h"
#include "fwtl/pool.h"
#include "grcore/allocscope.h"
#include "grcore/debugdraw.h"
#include "system/typeinfo.h"

#include "Cutscene/CutSceneManagerNew.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/syncedscene/SyncedSceneDirector.h"
#include "camera/scripted/ScriptDirector.h"
#include "renderer/Lights/LightEntity.h"
#include "renderer/Lights/LightCulling.h"
#include "renderer/Util/Util.h" // SAFE_DELETE, SAFE_RELEASE
#include "renderer/spupm/spupmmgr.h"
#include "renderer/shadows/shadowsprivate.h"
#include "renderer/shadows/paraboloidshadows.h"
#include "scene/portals/Portal.h"
#include "scene/portals/PortalTracker.h"
#include "system/SettingsManager.h"
#include "Renderer/Deferred/DeferredLighting.h"
#include "Renderer/RenderPhases/RenderPhaseCascadeShadows.h"

#if __BANK
#include "debug/Rendering/DebugLighting.h"
#include "debug/Rendering/DebugLights.h"
#include "debug/BudgetDisplay.h"
#include "Cutscene/CutSceneDebugManager.h"
#endif

#if RSG_ORBIS
#include "grcore/gfxcontext_gnm.h"
#endif

RENDER_OPTIMISATIONS()

#if RSG_PC
PARAM(use_slishadowcopy,"use copysubresourceregion or shader copy tech for copying shadowmap");
#endif

#if RSG_ORBIS || RSG_DURANGO
NOSTRIP_XPARAM(shadowQuality);
#endif

bank_float					  s_DepthBias		   = 0.001f;
bank_float					  s_SlopeDepthBias	   = 2.0f;

// ================================================================================================

void CParaboloidShadowActiveEntry::Set(const CLightSource& light, u16 shadowFlags, bool bEnabled, const fwInteriorLocation interiorLocation)
{
	// TODO -- CLightSource should share a class with shadow active entries ..
	
	SetPosDir(light);
	m_trackingID = light.GetShadowTrackingId();
	m_flags      = shadowFlags;
	
	if (bEnabled)
		m_flags |= SHADOW_FLAG_ENABLED;

	if (light.GetFlag(LIGHTFLAG_INTERIOR_ONLY))
		m_flags |= SHADOW_FLAG_INTERIOR;
	else if (light.GetFlag(LIGHTFLAG_EXTERIOR_ONLY))
		m_flags |= SHADOW_FLAG_EXTERIOR;
	else
		m_flags |= SHADOW_FLAG_EXTERIOR | SHADOW_FLAG_INTERIOR;

	if (light.GetFlag(LIGHTFLAG_DELAY_RENDER))
		m_flags |= SHADOW_FLAG_DELAY_RENDER;
	
	if (light.GetFlag(LIGHTFLAG_CUTSCENE)) 
		m_flags |= SHADOW_FLAG_CUTSCENE;

	if (light.GetShadowExclusionEntitiesCount() > 0)
		m_flags |= SHADOW_FLAG_ENTITY_EXCLUSION;

	m_interiorLocation = interiorLocation;
}

void CParaboloidShadowActiveEntry::SetInfo(const CLightSource& light)
{
	m_info.m_type = CParaboloidShadow::GetShadowType(light);

	m_info.m_tanFOV = (light.GetType()==LIGHT_TYPE_SPOT) ? (light.GetConeBaseRadius()/light.GetConeHeight()) : 1.0f;
	
	if (!Verifyf (m_info.m_tanFOV!=0.0f && FPIsFinite(m_info.m_tanFOV), "Invalid %s Light at (%f,%f,%f), ConeBase = %f, ConeHeight = %f", ((light.GetType() == LIGHT_TYPE_SPOT)?"Spot":"Point"), light.GetPosition().x, light.GetPosition().y, light.GetPosition().z, light.GetConeBaseRadius(),light.GetConeHeight()))
		m_info.m_tanFOV = 1.0f; // default to 90 degrees, better than nothing

	Assert(FPIsFinite(light.GetShadowNearClip()) && (light.GetShadowNearClip() != 0.0f));
	Assert(light.GetRadius() > 0.0f);	   

	m_info.m_nearClip   = Max(0.01f,light.GetShadowNearClip());
	m_info.m_shadowDepthRange = Max(0.1f,light.GetRadius());
	
	if (m_info.m_nearClip >= m_info.m_shadowDepthRange) // if the light's ShadowNearClip value is larger than the radius, adjust the near clip plane to avoid an invalid projection matrix.
		m_info.m_nearClip = m_info.m_shadowDepthRange - 0.05f;

	float depthBiasAdjust = light.GetShadowDepthBiasScale();

	if (depthBiasAdjust==1.0f)	// temporary hack until helicopters and character spot lights in shops/wardrobes set their depth bias scale..
	{
		if (light.GetType()==LIGHT_TYPE_SPOT)
		{
			if (light.GetFlag(LIGHTFLAG_MOVING_LIGHT_SOURCE) )
			{
				if (m_info.m_tanFOV<0.09f && m_info.m_shadowDepthRange>200.0f) // chopper search light
					depthBiasAdjust = 0.5f;
			}
			else if ((camInterface::GetSyncedSceneDirector().IsRendering() || camInterface::GetScriptDirector().IsRendering()) &&  CPortal::IsInteriorScene()) // character spot light in shop?
			{
				if ((light.GetFlags() & (LIGHTFLAG_INTERIOR_ONLY|LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS|LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS|LIGHTFLAG_CAST_SHADOWS))==(LIGHTFLAG_INTERIOR_ONLY|LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS|LIGHTFLAG_CAST_SHADOWS))	
					depthBiasAdjust = 0.5f;
			}
		}
	}

	m_info.m_depthBias = s_DepthBias * depthBiasAdjust;
	m_info.m_slopeScaleDepthBias = s_SlopeDepthBias * (0.5f + 0.5f * depthBiasAdjust); // don't scale slope by as much
}

void CParaboloidShadowActiveEntry::SetPosDir(const CLightSource& light)
{
	m_pos		= light.GetPosition();
	m_dir		= light.GetDirection(); 
	m_tandir	= light.GetTangent();

	Assertf(m_pos.FiniteElements() && m_dir.Mag2()>0.01f && m_tandir.Mag2()>0.01f, "Invalid %s Light at (%f,%f,%f), Direction = (%f,%f,%f),TangentDir = (%f,%f,%f)", ((light.GetType() == LIGHT_TYPE_SPOT)?"Spot":"Point"), light.GetPosition().x, light.GetPosition().y, light.GetPosition().z, m_dir.x, m_dir.y, m_dir.z, m_tandir.x, m_tandir.y, m_tandir.z);

	if (m_dir.Mag2()<=0.01f)
		m_dir = -ZAXIS;

	if (m_tandir.Mag2()<=0.01f)
		m_tandir = -XAXIS;

	m_dir.Normalize();
	m_tandir.Normalize();
}

void CParaboloidShadowActiveEntry::SetCaches(u16 activeCacheIndex, u16 staticCacheIndex)
{
	m_activeCacheIndex = activeCacheIndex + MAX_STATIC_CACHED_PARABOLOID_SHADOWS;
	m_staticCacheIndex = staticCacheIndex;
}


// ================================================================================================

CParaboloidShadowActiveEntry  CParaboloidShadow::m_activeEntries[MAX_ACTIVE_PARABOLOID_SHADOWS];
CParaboloidShadowCachedEntry  CParaboloidShadow::m_cachedEntries[MAX_CACHED_PARABOLOID_SHADOWS];

#if !USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS
atArray<grcRenderTarget*>    *CParaboloidShadow::m_RenderTargets[4] = { NULL, NULL, NULL, NULL };			
grcTexture*					  CParaboloidShadow::m_DummyCubemap = NULL;
#else
grcRenderTarget*		      CParaboloidShadow::m_RenderTargets[4] = { NULL, NULL, NULL, NULL };
#endif

grmShader*					  CParaboloidShadow::m_ShadowZShaderPointCM               = NULL;
grmShader*                    CParaboloidShadow::m_ShadowSmartBlitShader              = NULL;

s32                           CParaboloidShadow::m_ParaboloidDepthPointTexGroupId     = -1;
s32                           CParaboloidShadow::m_ParaboloidDepthPointTexTextureGroupId = -1;

#if USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS
grcEffectGlobalVar            CParaboloidShadow::m_shadowTextureCacheId[kShadowTargetArrayCount];
#else
grcEffectGlobalVar            CParaboloidShadow::m_shadowTextureCacheId[LOCAL_SHADOWS_MAX_FORWARD_SHADOWS];
grcEffectGlobalVar            CParaboloidShadow::m_shadowCMTextureCacheId[LOCAL_SHADOWS_MAX_FORWARD_SHADOWS];
#endif

grcEffectGlobalVar            CParaboloidShadow::m_LocalDitherTextureId;
grcEffectGlobalVar            CParaboloidShadow::m_shadowParam0_ID;
grcEffectGlobalVar            CParaboloidShadow::m_shadowParam1_ID;
grcEffectGlobalVar            CParaboloidShadow::m_shadowMatrix_ID;
grcEffectGlobalVar            CParaboloidShadow::m_shadowRenderData_ID;
grcEffectGlobalVar			  CParaboloidShadow::m_shadowTexParam_ID;

grcEffectVar                  CParaboloidShadow::m_cubeMapFaceId;


grcEffectVar                  CParaboloidShadow::m_SmartBlitTextureId;
grcEffectVar                  CParaboloidShadow::m_SmartBlitCubeMapTextureId;

#if RSG_PC
bool						  CParaboloidShadow::m_initialized = false;
bool						  CParaboloidShadow::m_useShaderCopy;
grcEffectTechnique            CParaboloidShadow::m_copyTechniqueId;
#endif

#if __BANK
grcEffectTechnique            CParaboloidShadow::m_copyDebugTechniqueId;
grcEffectTechnique            CParaboloidShadow::m_copyCubeMapDebugTechniqueId;
#endif
int                           CParaboloidShadow::m_cacheUpdateSequence;
int                           CParaboloidShadow::m_shadowMapBaseRes;
int                           CParaboloidShadow::m_shadowMapBaseBpp;
grcTextureFormat              CParaboloidShadow::m_shadowMapBaseFormat;
grcRasterizerStateHandle	  CParaboloidShadow::m_state_R[eCullModeCount]; 
grcBlendStateHandle			  CParaboloidShadow::m_state_B[eColorWriteModeCount];
grcDepthStencilStateHandle	  CParaboloidShadow::m_state_DSS[eDepthModeCount];

#if CACHE_USE_MOVE_ENGINE
UINT64                        CParaboloidShadow::sm_AsyncDMAFenceD3D;
#elif DEVICE_GPU_WAIT
grcFenceHandle                CParaboloidShadow::sm_AsyncDMAFence;
#endif

int							  CParaboloidShadow::m_RenderBufferIndex				   = 0;

#if RSG_PC
bool						  CParaboloidShadow::m_EnableForwardShadowsOnGlass		   = false;
#endif
bool						  CParaboloidShadow::m_ScriptRequestedJumpCut			   = false;
#if __BANK
bool                          CParaboloidShadow::m_debugShowStaticEntries              = false;
bool                          CParaboloidShadow::m_debugShowActiveEntries              = false;
float                         CParaboloidShadow::m_debugShowShadowBufferCacheInfoAlpha = 1.0f;

int                           CParaboloidShadow::m_debugCacheIndexStart                = 0;
int                           CParaboloidShadow::m_debugCacheIndexCount                = 8; 
int                           CParaboloidShadow::m_debugDisplaySize                    = 128;
int                           CParaboloidShadow::m_debugDisplaySpacing                 = 4;
int                           CParaboloidShadow::m_debugDisplayBoundsX0                = 400;
int                           CParaboloidShadow::m_debugDisplayBoundsY0                = 832;
int                           CParaboloidShadow::m_debugDisplayBoundsX1                = 0;
int                           CParaboloidShadow::m_debugDisplayBoundsY1                = 52;
bool                          CParaboloidShadow::m_debugOmniOverride                   = true;
int                           CParaboloidShadow::m_debugOmniNumSamples                 = 5;
bool                          CParaboloidShadow::m_debugRegenCache                     = false;
int                           CParaboloidShadow::m_debugDisplayScreenArea			   = -1;
int                           CParaboloidShadow::m_debugDisplayFacetScreenArea         = 0;
float                         CParaboloidShadow::m_debugScaleScreenAreaDisplay	       = 1.0f;  // reduce volumes radius so they are easier to see/debug

bool						  CParaboloidShadow::m_debugParaboloidShadowRenderCullShape					= false;
bool						  CParaboloidShadow::m_debugParaboloidShadowTightSpotCullShapeEnable		= true;
ScalarV                       CParaboloidShadow::m_debugParaboloidShadowTightSpotCullShapeRadiusDelta	= PARABOLOID_SHADOW_SPOT_RADIUS_DELTA;
ScalarV                       CParaboloidShadow::m_debugParaboloidShadowTightSpotCullShapePositionDelta	= PARABOLOID_SHADOW_SPOT_POSITION_DELTA;
ScalarV                       CParaboloidShadow::m_debugParaboloidShadowTightSpotCullShapeAngleDelta	= PARABOLOID_SHADOW_SPOT_ANGLE_DELTA;

atRangeArray<Vec3V,9>		  s_DebugScreenVolume;
static float				  s_HemisphereShadowCutoffInAngles = 61.0f;				  
static fwUniqueObjId 		  s_SingleOutTrackingID = 0;  // debug helper. (cutscene lights are hard to isolate)
static bool					  s_DumpCacheInfo = false;
static bool					  s_DebugEnableParaboloidShadowInfoUpdate=false;
static bool					  s_BreakpointDebugCutsceneCameraCuts = false;
static int					  s_DumpShadowLights = 0;
static bool					  s_DebugClearCache = false;
static bool					  s_SaveIsolatedLightShadowTrackingIndex = false;
static bool					  s_SkipForwardShadow = false;

bool						  g_ParaboloidShadowDebugBool1 = false;
bool						  g_ParaboloidShadowDebugBool2 = false;
int							  g_ParaboloidShadowDebugInt1  = 0;
int							  g_ParaboloidShadowDebugInt2  = 0;
#endif // __BANK

bank_bool					  s_debugForceInteriorCheck = false;

bank_float                    s_MinHiresThreshold  = 0.5f;
bank_float					  s_HiresFlagPriorityBoost = 2.0f;
bank_float					  s_NonVisiblePriorityScale = .5f;

bank_bool					  s_FlipCull = false;

bank_float					  s_HemisphereShadowCutoff = 1.804047755271423937381784748237f; // tan(61)	 

bank_bool					  s_AsyncStaticCacheCopy = RSG_DURANGO | RSG_ORBIS; // not sure on PC yet

static grcViewport			  s_VP[NUMBER_OF_RENDER_THREADS];

// TODO -- do not use separate globals here!!
static CParaboloidShadowActiveEntry s_paraboloidShadowMapActive[2][MAX_ACTIVE_PARABOLOID_SHADOWS];
static CParaboloidShadowCachedEntry s_paraboloidShadowMapCache[2][MAX_CACHED_PARABOLOID_SHADOWS];

#if CACHE_USE_MOVE_ENGINE
bank_bool s_useDMA1 = true;
#endif

#if RSG_PC
static bool g_bRegenCache = false;
#endif

void CParaboloidShadow::Init()
{
	// the active Paraboloid shadow count should match the number of Paraboloid shadow renderphases
	CompileTimeAssert(MAX_ACTIVE_PARABOLOID_SHADOWS == MAX_PARABOLOID_SHADOW_RENDERPHASES);

	m_ParaboloidDepthPointTexGroupId		= -1;
	m_ParaboloidDepthPointTexTextureGroupId	= -1;

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	m_ShadowZShaderPointCM       = NULL;
#endif
	m_ShadowSmartBlitShader      = NULL;
	m_cacheUpdateSequence        = 0;

	for (int i = 0; i < MAX_ACTIVE_PARABOLOID_SHADOWS; i++)
	{
		m_activeEntries[i].Init();
	}
	for (int i = 0; i < MAX_CACHED_PARABOLOID_SHADOWS; i++)
	{
		m_cachedEntries[i].Init();
	}

	m_shadowMapBaseRes    = -1;
	m_shadowMapBaseBpp    = -1;
	m_shadowMapBaseFormat = grctfNone;

	// Setup render state blocks

	// depth states read/write enabled but test is always, read write disable, etc
	grcDepthStencilStateDesc defaultStateDSS;
	defaultStateDSS.DepthEnable = FALSE;
	defaultStateDSS.DepthWriteMask = FALSE;
	defaultStateDSS.DepthFunc = grcRSV::CMP_LESS;
	m_state_DSS[eDepthModeDisable] = grcStateBlock::CreateDepthStencilState(defaultStateDSS);
	
	defaultStateDSS.DepthEnable = TRUE;
	defaultStateDSS.DepthWriteMask = TRUE;
	defaultStateDSS.DepthFunc = grcRSV::CMP_ALWAYS;
	m_state_DSS[eDepthModeEnableAlways] = grcStateBlock::CreateDepthStencilState(defaultStateDSS);
	
	m_state_DSS[eDepthModeEnableCompare] =  grcStateBlock::DSS_LessEqual;

	// need two version of rasterizer state to flip Cull modes
	grcRasterizerStateDesc defaultStateR;

	defaultStateR.CullMode = grcRSV::CULL_FRONT;
	m_state_R[eCullModeFront] = grcStateBlock::CreateRasterizerState(defaultStateR);
	
	defaultStateR.CullMode = grcRSV::CULL_BACK;
	m_state_R[eCullModeBack] = grcStateBlock::CreateRasterizerState(defaultStateR);

	defaultStateR.CullMode = grcRSV::CULL_NONE;
	m_state_R[eCullModeNone] = grcStateBlock::CreateRasterizerState(defaultStateR);
	
	// need 4 version of blend state state to toggle various write masks
	grcBlendStateDesc defaultStateB;
	defaultStateB.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	m_state_B[eColorWriteNone] = grcStateBlock::CreateBlendState(defaultStateB);

	defaultStateB.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = RSG_PC || RSG_DURANGO || RSG_ORBIS ? grcRSV::COLORWRITEENABLE_NONE : grcRSV::COLORWRITEENABLE_ALL;
	m_state_B[eColorWriteRGBA] = grcStateBlock::CreateBlendState(defaultStateB);

	defaultStateB.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = RSG_PC || RSG_DURANGO || RSG_ORBIS ? grcRSV::COLORWRITEENABLE_NONE : grcRSV::COLORWRITEENABLE_RED PPU_ONLY(| grcRSV::COLORWRITEENABLE_GREEN);
	m_state_B[eColorWriteRG]= grcStateBlock::CreateBlendState(defaultStateB);

	defaultStateB.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = RSG_PC || RSG_DURANGO || RSG_ORBIS ? grcRSV::COLORWRITEENABLE_NONE : grcRSV::COLORWRITEENABLE_BLUE PPU_ONLY(| grcRSV::COLORWRITEENABLE_ALPHA);
	m_state_B[eColorWriteBA] = grcStateBlock::CreateBlendState(defaultStateB);
	m_state_B[eColorWriteDefault] = m_state_B[(__PPU)?eColorWriteNone:eColorWriteRGBA];

	// clear viewport owners since the array was created in the main thread. the first use will assign the real owner
	for (int i=0;i<NUMBER_OF_RENDER_THREADS;i++)
		s_VP[i].ClearOwner(); 

#if !CACHE_USE_MOVE_ENGINE && DEVICE_GPU_WAIT
	sm_AsyncDMAFence = GRCDEVICE.AllocFence();
#endif
}


void CParaboloidShadow::Terminate()
{
#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	SAFE_DELETE(m_ShadowZShaderPointCM);
#endif
	SAFE_DELETE(m_ShadowSmartBlitShader);
}

// this returns the amount of texture memory used by the cached part of the shadow cache, so ped damage can skip over it and only overlap the active entries
int CParaboloidShadow::GetCacheStaticTextureSize()
{
	Assertf(m_shadowMapBaseRes > -1, "CParaboloidShadow::GetCacheTextureSize() Called before the shadows are initialized");

	return (m_shadowMapBaseRes*2)*(m_shadowMapBaseRes*2*3)*(MAX_STATIC_CACHED_HIRES_PARABOLOID_SHADOWS)+
			 (m_shadowMapBaseRes)*(m_shadowMapBaseRes*3)*(MAX_STATIC_CACHED_LORES_PARABOLOID_SHADOWS);
}


void CParaboloidShadow::Synchronise() 
{
	m_RenderBufferIndex ^= 1;
}


// allocates and array of cubemaps and pointmaps, hopefully overlapping at some point
#if USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS 
void AllocateTextureArrays(grcRenderTarget*&cubemapArray, grcRenderTarget*&pointArray, const char* name, int count, int baseRes, int bpp)
#else
void AllocateTextureArrays(atArray<grcRenderTarget*>* &cubemapArray, atArray<grcRenderTarget*>*&pointArray, const char* name, int count, int baseRes, int bpp)
#endif
{
	grcTextureFactory::CreateParams params;

	params.Parent               = NULL;
	params.UseHierZ             = false;
	params.UseFloat             = true;

	params.Format		        = (bpp==16) ? grctfD16 : grctfD24S8;

	params.Multisample          = grcDevice::MSAA_None;
	params.MipLevels            = 1;

	params.HasParent            = false;
	params.IsResolvable         = true;
	params.IsRenderable         = true;

	params.EnableCompression    = false;

#if RSG_ORBIS 
	params.EnableFastClear      = false;
	params.EnableStencil        = false;
#endif

	char nameBuffer[256];

#if USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS
	params.ArraySize = u8(6*count);
	params.PerArraySliceRTV = params.ArraySize;

	formatf(nameBuffer, 256, "POINT_SHADOW_%s",name);
	cubemapArray = CRenderTargets::CreateRenderTarget(
		RTMEMPOOL_NONE,
		nameBuffer,
		RSG_ORBIS?grcrtDepthBufferCubeMap:grcrtCubeMap,
		baseRes,  
		baseRes,
		bpp,
		&params,
		0
		WIN32PC_ONLY(,cubemapArray)
		);	

	params.ArraySize = u8(count);
	params.PerArraySliceRTV = params.ArraySize;

	// TODO: this could overlap with the cubemap version above
	formatf(nameBuffer, 256, "SPOT_SHADOW_%s",name);
	pointArray = CRenderTargets::CreateRenderTarget(
		RTMEMPOOL_NONE,
		nameBuffer,
		RSG_ORBIS?grcrtShadowMap:grcrtPermanent,
		baseRes*2,  
		baseRes*3,
		bpp,
		&params,
		0
		WIN32PC_ONLY(, pointArray)
		);	
#else

	atArray<grcRenderTarget*> *pOldCubeArray = cubemapArray;
	atArray<grcRenderTarget*> *pOldPointArray = pointArray;

	cubemapArray = rage_new atArray<grcRenderTarget*>(0,count);
	pointArray = rage_new atArray<grcRenderTarget*>(0,count);

	for (int i=0;i<count;i++)
	{
		params.ArraySize = (u8)6;
		params.PerArraySliceRTV = params.ArraySize;

		formatf(nameBuffer, 256, "POINT_SHADOW_%s_%d",name,i);
		grcRenderTarget* & cubeTarget = cubemapArray->Append();
		WIN32PC_ONLY(grcRenderTarget* pOldCubeTarget = pOldCubeArray ? (*pOldCubeArray)[i] : NULL);

		cubeTarget = CRenderTargets::CreateRenderTarget(
			RTMEMPOOL_NONE,
			nameBuffer,
			RSG_ORBIS?grcrtDepthBufferCubeMap:grcrtCubeMap,
			baseRes,  
			baseRes,
			bpp,
			&params,
			0
			WIN32PC_ONLY(, pOldCubeTarget)
			);	
	
		// TODO: this could overlap with the cubemap version above
		params.ArraySize = (u8)1;
		params.PerArraySliceRTV = params.ArraySize;

		formatf(nameBuffer, 256, "SPOT_SHADOW_%s_%d",name,i);
		grcRenderTarget* & pointTarget = pointArray->Append();
		WIN32PC_ONLY(grcRenderTarget* pOldPointTarget = pOldPointArray ? (*pOldPointArray)[i] : NULL);

		pointTarget = CRenderTargets::CreateRenderTarget(
			RTMEMPOOL_NONE,
			nameBuffer,
			RSG_ORBIS?grcrtShadowMap:grcrtPermanent,
			baseRes*2,  
			baseRes*3,
			bpp,
			&params,
			0
			WIN32PC_ONLY(, pOldPointTarget)
#if LOCAL_SHADOWS_OVERLAP_RENDER_TARGETS	
			, cubeTarget  // not sure how we will do this on PC, since, since the rendertearget pointer is used for something else
#endif
			);	
	}

	if(pOldPointArray)
		delete pOldPointArray;

	if(pOldCubeArray)
		delete pOldCubeArray;
#endif
}

void CParaboloidShadow::CreateBaseMaps(int res, int bpp)
{
#if RSG_PC
	m_initialized = true;
#endif
	RAGE_TRACK(Shadows);
	USE_MEMBUCKET(MEMBUCKET_RENDER);

	Assert(res > 0);

	if (m_shadowMapBaseRes    != res ||
		m_shadowMapBaseBpp    != bpp )
	{
		m_cacheUpdateSequence        = 0;

		for (int i = 0; i < MAX_ACTIVE_PARABOLOID_SHADOWS; i++)
		{
			m_activeEntries[i].Init();
		}
		for (int i = 0; i < MAX_CACHED_PARABOLOID_SHADOWS; i++)
		{
			m_cachedEntries[i].Init();
		}

		m_shadowMapBaseRes    = res;
		m_shadowMapBaseBpp    = bpp;
		
		int baseLores = Max(1, m_shadowMapBaseRes/2);
		AllocateTextureArrays(m_RenderTargets[kShadowLoresCubeMapTargets],m_RenderTargets[kShadowLoresSpotTargets],"LORES",MAX_STATIC_CACHED_LORES_PARABOLOID_SHADOWS+MAX_ACTIVE_LORES_PARABOLOID_SHADOWS+MAX_ACTIVE_LORES_OVERLAPPING,baseLores,bpp);
		AllocateTextureArrays(m_RenderTargets[kShadowHiresCubeMapTargets],m_RenderTargets[kShadowHiresSpotTargets],"HIRES",MAX_STATIC_CACHED_HIRES_PARABOLOID_SHADOWS+MAX_ACTIVE_HIRES_PARABOLOID_SHADOWS,m_shadowMapBaseRes,bpp);

		// map the new index and base texture to the old system for now.
		int highIndex=0;
		int lowIndex=0;

		// put the active render targets first  (could change the cache setup too, but that touches a lot of code)
		for (int i=MAX_STATIC_CACHED_PARABOLOID_SHADOWS; i<MAX_STATIC_CACHED_PARABOLOID_SHADOWS+MAX_ACTIVE_HIRES_PARABOLOID_SHADOWS; i++)
		{
			m_cachedEntries[i].m_renderTargetIndex = highIndex++;  // index into hires list
			m_cachedEntries[i].m_renderTarget[0] = m_RenderTargets[kShadowHiresCubeMapTargets];
			m_cachedEntries[i].m_renderTarget[1] = m_RenderTargets[kShadowHiresSpotTargets];
#if RSG_PC
			m_cachedEntries[i].m_gpucounter_static = 0;
#endif
		}

		for (int i=MAX_STATIC_CACHED_PARABOLOID_SHADOWS+MAX_ACTIVE_HIRES_PARABOLOID_SHADOWS; i<MAX_CACHED_PARABOLOID_SHADOWS; i++)
		{
			m_cachedEntries[i].m_renderTargetIndex = lowIndex++;  // index into lores list
			m_cachedEntries[i].m_renderTarget[0] = m_RenderTargets[kShadowLoresCubeMapTargets];
			m_cachedEntries[i].m_renderTarget[1] = m_RenderTargets[kShadowLoresSpotTargets];
#if RSG_PC
			m_cachedEntries[i].m_gpucounter_static = 0;
#endif
		}
	
		for (int i=0; i<MAX_STATIC_CACHED_HIRES_PARABOLOID_SHADOWS; i++)
		{
			m_cachedEntries[i].m_renderTargetIndex = highIndex++;  // index into hires list
			m_cachedEntries[i].m_renderTarget[0] = m_RenderTargets[kShadowHiresCubeMapTargets];
			m_cachedEntries[i].m_renderTarget[1] = m_RenderTargets[kShadowHiresSpotTargets];
#if RSG_PC
			m_cachedEntries[i].m_gpucounter_static = 0;
#endif
		}

		for (int i=MAX_STATIC_CACHED_HIRES_PARABOLOID_SHADOWS; i<MAX_STATIC_CACHED_PARABOLOID_SHADOWS; i++)
		{
			m_cachedEntries[i].m_renderTargetIndex = lowIndex++;  // index into lores list
			m_cachedEntries[i].m_renderTarget[0] = m_RenderTargets[kShadowLoresCubeMapTargets];
			m_cachedEntries[i].m_renderTarget[1] = m_RenderTargets[kShadowLoresSpotTargets];
#if RSG_PC
			m_cachedEntries[i].m_gpucounter_static = 0;
#endif
		}

#if !USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS
		// we need a dummy cubemap to bind when the forward light is not used.
		if (grcImage *const image = grcImage::Create(1, 1, 1, grcImage::R32F, grcImage::CUBE, 0, 5))
		{
			sysMemSet(image->GetBits(),0,image->GetSize());
			BANK_ONLY(grcTexture::SetCustomLoadName("DummyShadowCubemap");)
			m_DummyCubemap = grcTextureFactory::GetInstance().Create(image);
			BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
			Assert(m_DummyCubemap);
			image->Release();
		}
		else
		{
			m_DummyCubemap = NULL;
		}
#endif
	}
}

int CParaboloidShadow::GetShadowRes()
{
#if RSG_ORBIS || RSG_DURANGO
	int shadowQuality;
	if (PARAM_shadowQuality.Get(shadowQuality))
	{
		return (shadowQuality < 2) ? 256 : 512;
	}
	return 512;
#elif __D3D11
	const Settings& settings = CSettingsManager::GetInstance().GetSettings();

	// should only be used on fairly beefy machines it is pretty expensive
	m_EnableForwardShadowsOnGlass = (settings.m_graphics.m_ShadowQuality >= CSettings::High) && (settings.m_graphics.m_ShaderQuality >= CSettings::High); 

	if (settings.m_graphics.m_ShadowQuality == (CSettings::eSettingsLevel) (0)) return 2;
	return (settings.m_graphics.m_ShadowQuality < 2) ? 256 : 512;
	//	return (settings.m_graphics.m_SpotShadowRes) ? 512 : 256;
#else
	return 256;
#endif
}

#if RSG_PC
void CParaboloidShadow::DeviceLost()
{

}

#if __DEV && RSG_PC
	XPARAM(testDeviceReset);
#endif

void CParaboloidShadow::DeviceReset()
{
	if (m_initialized)
	{
#if __DEV && RSG_PC
		if (PARAM_testDeviceReset.Get())
		{
			m_shadowMapBaseRes = 0;
		}
#endif
		CreateBaseMaps(GetShadowRes(), 16); 

#if RSG_PC
		if (GRCDEVICE.UsingMultipleGPUs())
			g_bRegenCache = true;
#endif
	}
}

#endif

void CParaboloidShadow::InitShaders(const char* path)
{
	ASSET.PushFolder(path);

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	m_ShadowZShaderPointCM = grmShaderFactory::GetInstance().Create();
	m_ShadowZShaderPointCM->Load("shadowZCM"); 
#endif
	m_ShadowSmartBlitShader = grmShaderFactory::GetInstance().Create();
	m_ShadowSmartBlitShader->Load("shadowSmartBlit");


	m_LocalDitherTextureId				= grcEffect::LookupGlobalVar("gLocalDitherTexture");

#if USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS
	m_shadowTextureCacheId[kShadowHiresCubeMapTargets]	= grcEffect::LookupGlobalVar("gLocalLightShadowHiresCubeArray");
	m_shadowTextureCacheId[kShadowHiresSpotTargets]		= grcEffect::LookupGlobalVar("gLocalLightShadowHiresSpotArray");
	m_shadowTextureCacheId[kShadowLoresCubeMapTargets]	= grcEffect::LookupGlobalVar("gLocalLightShadowLoresCubeArray");
	m_shadowTextureCacheId[kShadowLoresSpotTargets]		= grcEffect::LookupGlobalVar("gLocalLightShadowLoresSpotArray");
#else
	for (int i=0;i<LOCAL_SHADOWS_MAX_FORWARD_SHADOWS;i++)
	{
		char nameBuffer[256];
		formatf(nameBuffer, 256, "gLocalLightShadowSpot%d",i);
		m_shadowTextureCacheId[i]	= grcEffect::LookupGlobalVar(nameBuffer);			
		formatf(nameBuffer, 256, "gLocalLightShadowCM%d",i);
		m_shadowCMTextureCacheId[i]	= grcEffect::LookupGlobalVar(nameBuffer);	
	}
#endif   

	m_shadowParam0_ID                    = grcEffect::LookupGlobalVar("ShadowParam0");
	m_shadowParam1_ID                    = grcEffect::LookupGlobalVar("ShadowParam1");
	m_shadowMatrix_ID                    = grcEffect::LookupGlobalVar("ShadowMatrix");
	m_shadowRenderData_ID				 = grcEffect::LookupGlobalVar("gLocalLightShadowData");
	m_shadowTexParam_ID					 = grcEffect::LookupGlobalVar("gShadowTexParam");

	m_cubeMapFaceId                      = m_ShadowSmartBlitShader->LookupVar("CubeMapFace");
	m_SmartBlitTextureId                 = m_ShadowSmartBlitShader->LookupVar("SmartBlitTexture");
	m_SmartBlitCubeMapTextureId          = m_ShadowSmartBlitShader->LookupVar("SmartBlitCubeMapTexture");

#if RSG_PC
	// use shadercopy solution for SLI 
	if (GRCDEVICE.UsingMultipleGPUs() && (GRCDEVICE.GetManufacturer() == NVIDIA))
	{
		if (PARAM_use_slishadowcopy.Get())
			m_useShaderCopy					 = CSettingsManager::GetInstance().GetSettings().m_graphics.m_DX_Version==0; // less than feature level 10_1
		else
			m_useShaderCopy					 = true;
	}
	else
		m_useShaderCopy					 = CSettingsManager::GetInstance().GetSettings().m_graphics.m_DX_Version==0; // less than feature level 10_1
	m_copyTechniqueId					 = m_ShadowSmartBlitShader->LookupTechnique("copyDepthBlit");
#endif

#if __BANK
	m_copyDebugTechniqueId				 = m_ShadowSmartBlitShader->LookupTechnique("copyColorDepthDebugBlit");
	m_copyCubeMapDebugTechniqueId		 = m_ShadowSmartBlitShader->LookupTechnique("copyColorDepthCubeMapDebugBlit");
#endif
	
	m_ParaboloidDepthPointTexGroupId   = grmShaderFx::FindTechniqueGroupId("ld");
	Assert(m_ParaboloidDepthPointTexGroupId != -1);
	
	m_ParaboloidDepthPointTexTextureGroupId  = grmShaderFx::FindTechniqueGroupId("shadtexture_ld"); 
	Assert(m_ParaboloidDepthPointTexTextureGroupId != -1);

	ASSET.PopFolder();

	CreateBaseMaps(GetShadowRes(), 16); 
}



////////////////////////

static Vec3V_Out AddScaled3( Vec3V_In pos,Vec3V_In v1, ScalarV_In v1s,Vec3V_In v2,ScalarV_In v2s)
{
      return AddScaled( AddScaled( pos, v1, v1s), v2, v2s );
}

// just return if the facet has any area on screen.
bool CParaboloidShadow::TestScreenSpaceBoundsCommon(atRangeArray<Vec3V,9> &pointList, Mat44V_ConstRef viewProj, int BANK_ONLY(facet), int BANK_ONLY(s))
{

#if __BANK
	if ((s==CParaboloidShadow::m_debugDisplayScreenArea) && (facet == CParaboloidShadow::m_debugDisplayFacetScreenArea))
	{
		for (int i=0;i<pointList.GetMaxCount();i++)
			s_DebugScreenVolume[i] = Vec3V(V_ZERO);
	}
#endif
	
	const grcViewport * vp = gVpMan.GetCurrentGameGrcViewport(); 
	spdAABB bound( (float*)&pointList[0],  pointList.GetMaxCount(), sizeof(Vec3V) );

	// first check if bound around all points are out of the view 
	grcCullStatus status = grcViewport::GetAABBCullStatus(bound.GetMinVector3(), bound.GetMaxVector3(), vp->GetFrustumLRTB(), vp->GetFrustumNFNF());
	if (status == cullOutside)
	{
		return false;
	}
	else
	{
		// now generate a screen space bounding box from the projected points. this pick up many cases due to the nature of the faces.
		// TODO: since we no longer need actual per facet screen space depths for depth bounds, this could be replaced by a real cone vs frustum intersection test.	
		Vec3V minVec;
		Vec3V maxVec;
		
		atRangeArray<Vec4V,9> projectedPoints;

		int  useFullScreen = 0;

		minVec = Vec3V(V_FLT_MAX);
		maxVec = Vec3V(V_NEG_FLT_MAX);

		for (int i = 0; i < pointList.GetMaxCount(); ++i)
		{
			// project the box into screen-space
			Vec4V projPoint = Multiply(viewProj, Vec4V(pointList[i],ScalarV(V_ONE)));  // todo: use faster And and Or to set 1.0
			projectedPoints[i] = projPoint;

			// TODO: should use a select here to avoid the branch, etc;
			// Negative Z indicates that we went through the near clip plane.
			if( IsGreaterThanAll( SplatZ( projPoint), ScalarV(V_ZERO) ) == 0 )
			{
				useFullScreen++;
			}
			else
			{	
				const ScalarV invW = InvertFast( SplatW( projPoint) );   // this is probably not optimal
				Vec3V normalizedPoint = (projPoint*invW).GetXYZ();

				minVec = Min(minVec, normalizedPoint);
				maxVec = Max(maxVec, normalizedPoint);
			}
		}

		if ( useFullScreen > 0  )
		{
			if ( useFullScreen == pointList.GetMaxCount())
			{
				return false; // all points are behind near plane
			}
			else
			{
				// TODO: need to compute extends of the edges clipped against the near clip plane
				minVec = Vec3V(Vec2V(V_NEGONE),ScalarV(V_ZERO)); // for now set to fullscreen
				maxVec = Vec3V(Vec2V(V_ONE),maxVec.GetZ());

				//ComputeClippedBound( projectedBox, spotEdges, 16, aabb );
			}
		}
	#if __BANK
		else
		{
			if ((s==CParaboloidShadow::m_debugDisplayScreenArea) && (facet == CParaboloidShadow::m_debugDisplayFacetScreenArea))
			{
				for (int i=0;i<pointList.GetMaxCount();i++)
					s_DebugScreenVolume[i] = projectedPoints[i].GetXYZ()/projectedPoints[i].GetW();
			}
		}
	#endif

		const Vec3V NNZ = Vec3V(Vec2V(V_NEGONE),ScalarV(V_ZERO)); 
		
		minVec = Clamp( minVec, NNZ, Vec3V(V_ONE) );
		maxVec = Clamp( maxVec, NNZ, Vec3V(V_ONE) );

		// if any of the mins are > 1.0 or any of the maxes <-1, set to zero so we can easily skip the whole thing
		if (!IsEqualNone(minVec,Vec3V(V_ONE)) || !IsEqualNone(maxVec,NNZ)) 
		{
			return false;
		}
	}

	return true;
}


void CParaboloidShadow::SetConeScreenVisibility(CShadowInfo& info, int s)
{
	Mat44V viewProj;
	const grcViewport * vp = gVpMan.GetCurrentGameGrcViewport(); 
	Multiply(viewProj, vp->GetProjection(), vp->GetViewMtx()); 

	const Mat34V & mtx = info.m_shadowMatrix;
	const Vec3V a = mtx.a();
	const Vec3V b = mtx.b();
	const Vec3V c = mtx.c();

	const float radius = info.m_shadowDepthRange;
	const float tanFOV = info.m_tanFOV;

	// Get location of the cap.
	const ScalarV computedRange = ScalarVFromF32(BANK_ONLY(CParaboloidShadow::m_debugScaleScreenAreaDisplay*)radius); 
	const Vec3V endcap = AddScaled( mtx.d(), c, -computedRange );

	const ScalarV spread = Scale( computedRange, ScalarVFromF32( tanFOV ) );
	const ScalarV halfSpread = Scale( spread, ScalarV(V_HALF));

	// Compute the eight points on an octagon, it's a better fit for a circle

	atRangeArray<Vec3V,9> pointList;
	pointList[0] = mtx.d();

	pointList[1] = AddScaled3( endcap, a,  spread, b,  halfSpread);
	pointList[2] = AddScaled3( endcap, a,  spread, b, -halfSpread);
	pointList[3] = AddScaled3( endcap, b, -spread, a,  halfSpread);
	pointList[4] = AddScaled3( endcap, b, -spread, a, -halfSpread);
	pointList[5] = AddScaled3( endcap, a, -spread, b, -halfSpread);
	pointList[6] = AddScaled3( endcap, a, -spread, b,  halfSpread);
	pointList[7] = AddScaled3( endcap, b,  spread, a, -halfSpread);
	pointList[8] = AddScaled3( endcap, b,  spread, a,  halfSpread);

	info.m_facetVisibility = TestScreenSpaceBoundsCommon(pointList, viewProj, 0, s)?0x1:0x0;
}

void CParaboloidShadow::SetCMFacetScreenSpaceVisibility(CShadowInfo& info, int facetCount, int s )
{
	Mat44V viewProj;
	const grcViewport * vp = gVpMan.GetCurrentGameGrcViewport(); 
	Multiply(viewProj, vp->GetProjection(), vp->GetViewMtx()); 

	for (int facet = 0; facet < facetCount; facet++)
	{
		Mat34V mtx;
		GetCMShadowFacetMatrix(mtx, facet, &info); 

		const Vec3V a = mtx.a();
		const Vec3V b = mtx.b();
		const Vec3V c = -mtx.c();

		const float radius = info.m_shadowDepthRange;

		const ScalarV insideSpread  = ScalarVFromF32(M_SQRT2 - M_SQRT2 * (M_SQRT3-1.0f)) ;
		const ScalarV outsideLength = ScalarVFromF32(BANK_ONLY(CParaboloidShadow::m_debugScaleScreenAreaDisplay*)radius*(M_SQRT3/M_SQRT2)); // want to scale -c by radius
		const ScalarV insideLength  = ScalarVFromF32(BANK_ONLY(CParaboloidShadow::m_debugScaleScreenAreaDisplay*)radius);
	
		const Vec3V & pos = info.m_shadowMatrix.d();
		atRangeArray<Vec3V,9> pointList;
		pointList[0] = pos;

		// Compute the four corner radius points 		
		pointList[1] = AddScaled(pos, Normalize(Add(c, Add(a,b))),	     outsideLength);			// a+b
		pointList[2] = AddScaled(pos, Normalize(Add(c, Subtract(a,b))),  outsideLength);			// a-b
		pointList[3] = AddScaled(pos, Normalize(Add(c, Subtract(-a,b))), outsideLength);			// -a-b
		pointList[4] = AddScaled(pos, Normalize(Add(c, Subtract(b,a))),  outsideLength);			// -a+b
		// now add four more points that create "diamond shaped facet" 
		pointList[5] = AddScaled(pos, AddScaled (c, Add(a,b),	   insideSpread), insideLength);	// a+b
		pointList[6] = AddScaled(pos, AddScaled (c, Subtract(a,b), insideSpread), insideLength);	// a-b
		pointList[7] = AddScaled(pos, AddScaled (c, Subtract(-a,b),insideSpread), insideLength);	// -a-b
		pointList[8] = AddScaled(pos, AddScaled (c, Subtract(b,a), insideSpread), insideLength);	// -a+b

		info.m_facetVisibility |= (TestScreenSpaceBoundsCommon(pointList, viewProj, facet, s)?0x1:0x0)<<facet;
	}
}


// the Rage Normalize() does not return  (0,1,0) when normalizing (0,1,0), I need these to be extremelt precise
Vec3V_Out PreciseNormal(Vec3V_In A)
{
	return A/Sqrt(Dot( A, A ));
}


void CParaboloidShadow::SetupHemiSphereVP(grcViewport &VP, const CShadowInfo& info, int facet)
{
	Assert(facet!=CShadowInfo::SHADOW_FACET_BACK);

	if (facet==CShadowInfo::SHADOW_FACET_BACK || 1)  // TODO look into hemisphere side facet compression
	{
		VP.Perspective(90.0f,1.0f,info.m_nearClip,info.m_shadowDepthRange); 
		VP.SetPerspectiveShear(0.0f,0.0f);
	}
	else
	{
		// make some slightly hacky calulations so we can use the existing Perspective() function to mimic the 
		// D3DXMatrixPerspectiveOffCenterLH, function to map an off axis projection to just the bottom half of 
		// the 90 degree side fustra
		static float scale = 2.0f;
		static float shear = -1.0f;
		float fov = 2*RtoD*atanf(tanf(DtoR*0.5f*90.0f)/scale); // this is really a constant

		VP.Perspective(fov,scale,info.m_nearClip,info.m_shadowDepthRange); 
		VP.SetPerspectiveShear(0.0f,shear);
	}
}


void CParaboloidShadow::CalcPointLightMatrix(int s)
{
	Assert(s >= 0);

	CShadowInfo& info = m_activeEntries[s].m_info;
	
	info.m_shadowMatrix.Setd(VECTOR3_TO_VEC3V(m_activeEntries[s].m_pos));  // m_pos should be a Vec3V
	  
	if (info.m_type==SHADOW_TYPE_POINT )
	{
		info.m_shadowMatrix.Seta(-Vec3V(V_X_AXIS_WZERO));
		info.m_shadowMatrix.Setb(-Vec3V(V_Y_AXIS_WZERO));
		info.m_shadowMatrix.Setc(Vec3V(V_Z_AXIS_WZERO));  
		
		// make a projection matrix for this light
		info.m_shadowMatrixP[0] = CalculatePerspectiveProjectionMatrix(ScalarV(info.m_nearClip),ScalarV(info.m_shadowDepthRange));
	}
	else // hemisphere, need actual direction
	{
		Vec3V C = PreciseNormal(-(VECTOR3_TO_VEC3V(m_activeEntries[s].m_dir)));
		Vec3V B = (Abs(m_activeEntries[s].m_dir.y)>.95f) ? Vec3V(V_X_AXIS_WZERO): -Vec3V(V_Y_AXIS_WZERO);
		Vec3V A = PreciseNormal(Cross(B,C));
		B = Cross(C,A);

		info.m_shadowMatrix.Seta(A);
		info.m_shadowMatrix.Setb(B);
		info.m_shadowMatrix.Setc(C);

		grcViewport VP; 
		SetupHemiSphereVP(VP, info, CShadowInfo::SHADOW_FACET_FRONT); 	// down is special for hemi
		info.m_shadowMatrixP[0] = VP.GetProjection();			
 		SetupHemiSphereVP(VP, info, CShadowInfo::SHADOW_FACET_RIGHT);	// all the sides use the same, so just pick one
 		info.m_shadowMatrixP[1] = VP.GetProjection();
	}

	int facetCount = CParaboloidShadow::GetFacetCount(info.m_type);
	SetCMFacetScreenSpaceVisibility(info, facetCount, s);

	// make sure the static cache has the same data
	const int cacheIndex = m_activeEntries[s].m_staticCacheIndex;
	if (cacheIndex != -1 && (CParaboloidShadow::m_cachedEntries[cacheIndex].m_status==CACHE_STATUS_REFRESH_CANDIDATE || CParaboloidShadow::m_cachedEntries[cacheIndex].m_status==CACHE_STATUS_GENERATING))
		CParaboloidShadow::m_cachedEntries[cacheIndex].m_info = m_activeEntries[s].m_info;
}


void CParaboloidShadow::CalcSpotLightMatrix(int s)
{
	Assert(s >= 0);

	CShadowInfo& info = m_activeEntries[s].m_info;

	LookDown(info.m_shadowMatrix, -(VECTOR3_TO_VEC3V(m_activeEntries[s].m_dir)), -((Abs(m_activeEntries[s].m_dir.y)>.95f) ? Vec3V(V_X_AXIS_WZERO):Vec3V(V_Y_AXIS_WZERO)));
	info.m_shadowMatrix.Setd(VECTOR3_TO_VEC3V(m_activeEntries[s].m_pos));

	// make a projection matrix for this light
	grcViewport VP;
	float fov = RtoD * 2.0f * rage::Atan2f( info.m_tanFOV,1.0f); // don't need to check tanFOV here for 0.0, since we check all th inputs, too many asserts don't actually help
	
	// calc the combined viewproj matrix
	VP.Perspective(fov,1.0f,info.m_nearClip,info.m_shadowDepthRange);
	info.m_shadowMatrixP[1] =  VP.GetProjection(); // save the pure projection viewport in case we need it later.
	
	CalcViewProjMtx(info.m_shadowMatrixP[0], VP.GetProjection(), info.m_shadowMatrix); // combine dir and projection matrix once and store it

	// set the screen space bounds
	SetConeScreenVisibility(info, s);
	
	// make sure the static cache has the same data
	const int cacheIndex = m_activeEntries[s].m_staticCacheIndex;
	if (cacheIndex != -1 && (CParaboloidShadow::m_cachedEntries[cacheIndex].m_status==CACHE_STATUS_REFRESH_CANDIDATE || CParaboloidShadow::m_cachedEntries[cacheIndex].m_status==CACHE_STATUS_GENERATING))
		CParaboloidShadow::m_cachedEntries[cacheIndex].m_info = m_activeEntries[s].m_info;
}


bool CParaboloidShadow::DidLightMove(const CParaboloidShadowInfo & info, const CLightSource & sceneLight)
{
 	if (sceneLight.GetFlag(LIGHTFLAG_MOVING_LIGHT_SOURCE)) // assume these always move
 		return true;

	// these test are really only needed until all moving lights are flagged properly.
	bool fovChange = (sceneLight.GetType()==LIGHT_TYPE_SPOT) ? (info.m_tanFOV != (sceneLight.GetConeBaseRadius()/sceneLight.GetConeHeight())) : false;
	bool posChange = !IsEqualAll(info.m_shadowMatrix.d(),VECTOR3_TO_VEC3V(sceneLight.GetPosition()));
	bool dirChange = (sceneLight.GetType()==LIGHT_TYPE_SPOT) && (IsLessThanOrEqualAll(Dot(info.m_shadowMatrix.c(),-VECTOR3_TO_VEC3V(sceneLight.GetDirection())),ScalarV(0.999999f))?true:false);
	bool sizeChange = info.m_shadowDepthRange != Max(0.1f,sceneLight.GetRadius()); 
	bool typeChange = info.m_type != GetShadowType(sceneLight); // this can happen when they turn on/off the LIGHTFLAG_DRAW_VOLUME flag (switches from Hemisphere to spot)
	
#if __BANK
	bool nearClipChange = info.m_nearClip != sceneLight.GetShadowNearClip(); // this really only changes via widget.
#endif

	return (fovChange || posChange || dirChange || sizeChange || typeChange BANK_ONLY( || nearClipChange || m_debugRegenCache)
#if RSG_PC
		|| g_bRegenCache
#endif		
		);
}


// this is for a late update (post visibility pass) needed for moving lights, otherwise the shadow map is 1 frame behind
// it looks for a current frame light that matched the one this shadow is associated with, and updates with it's position before generating the shadow map
// NOTE: this is not 100% correct, since the visibility pass has already run with the last position, but there is not much we can do about that
void CParaboloidShadow::CheckAndUpdateMovedLight(int s )
{
	CParaboloidShadowActiveEntry & entry = m_activeEntries[s];

	CLightSource * sceneLights = Lights::m_sceneLights;
	int numLights = Lights::m_numSceneLights;

	// find a light that has the same shadow tracking ID
	for (int i=0;i< numLights;i++)
	{
		if (sceneLights[i].GetShadowTrackingId() == entry.m_trackingID)
		{
			CParaboloidShadowInfo& info = entry.m_info;
		
			// check if it moved (or is flagged as a moving light)
			bool detectedMove =	DidLightMove(info, sceneLights[i]); 

			if (detectedMove || ((entry.m_flags&SHADOW_FLAG_LATE_UPDATE)!=0) )
			{
				entry.SetPosDir(sceneLights[i]);
				entry.SetInfo(sceneLights[i]);
				
				if (detectedMove && !sceneLights[i].GetFlag(LIGHTFLAG_MOVING_LIGHT_SOURCE)) // if we detected the move, but it was not flagged. let's mark it.
						sceneLights[i].SetFlag(LIGHTFLAG_MOVING_LIGHT_SOURCE); // this will get copied to the previous light list and used next frame.

				// update the cache entry for this shadow and force it's regen (set before matrix calc, the other data gets propagated the the static cache)
				if (entry.m_staticCacheIndex >= 0)
				{
					m_cachedEntries[entry.m_staticCacheIndex].m_status = CACHE_STATUS_GENERATING;
#if RSG_PC
					if (GRCDEVICE.UsingMultipleGPUs())
					{
						m_cachedEntries[entry.m_staticCacheIndex].m_gpucounter = (u8)(GRCDEVICE.GetGPUCount(true));
					}
#endif
				}

				if (info.m_type==SHADOW_TYPE_SPOT)
					CalcSpotLightMatrix(s); 
				else
					CalcPointLightMatrix(s);
			}
			
			break;
		}
	}
}


eLightShadowType CParaboloidShadow::GetShadowType(const CLightSource& lightSrc)
{
	eLightShadowType type = (lightSrc.GetType()==LIGHT_TYPE_POINT) ? SHADOW_TYPE_POINT:SHADOW_TYPE_SPOT;
	if (type==SHADOW_TYPE_SPOT)
	{
		//Add condition to restrict one face for spot light if its a volume also because volume lights don't support multi-faceted shadows
		// NOTE : this new condition can resulting massive shadow aliasing when very wide spotlights are being flagged with DRAW _VOLUME. 
		//        Also, sometimes LIGHTFLAG_DRAW_VOLUME is set on a light that was already a hemisphere light causing a visible shadow quality drop.
		 if (lightSrc.GetConeBaseRadius()/lightSrc.GetConeHeight() > s_HemisphereShadowCutoff)
				type = SHADOW_TYPE_HEMISPHERE;
	}

	return type;
}


static float CalcPriority(const CLightSource & sceneLight, const Vector3 & eyePos, const grcViewport & VP, bool bCameraCut, bool &isVisible)
{
	// TODO: should be Vectorized

	Vector3 tdir = eyePos - sceneLight.GetPosition();

	float distToCenter = tdir.Mag();
	float lightRadius = sceneLight.GetRadius();
	isVisible = true;

	// for non hemisphere lights, an AABB can be a big win
	if(CParaboloidShadow::GetShadowType(sceneLight) == SHADOW_TYPE_SPOT)
	{
		spdAABB bound;
		Lights::GetBound(sceneLight, bound);
		isVisible = VP.GetAABBCullStatus(bound.GetMin().GetIntrin128(), bound.GetMax().GetIntrin128(), VP.GetFrustumLRTB(), VP.GetFrustumNFNF()) != cullOutside;
	}

	if (isVisible && distToCenter>lightRadius)
	{
		// for all lights we are not inside of, use a sphere test (it's better than AABB for some spot lights, so test them again) 
		isVisible = VP.IsSphereVisible(sceneLight.GetPosition().x, sceneLight.GetPosition().y, sceneLight.GetPosition().z, lightRadius) != cullOutside;

		// during camera cuts we also do occlusion checking (since these light did not go through the normal visibility pass)
		if (isVisible && bCameraCut)
		{
			isVisible = IsLightVisible(sceneLight);
		}
	}

	float priority = 0;

	const float tanFOV  = VP.GetTanHFOV();
	priority = (distToCenter==0.0f) ? 10000.0f : (sceneLight.GetRadius()/(tanFOV*distToCenter)); // NOTE: this is not real screen space it's 1/2 the sphere radius in X, but it's proportional.

	priority = Min(10000.0f,priority);

	if(!isVisible)
		priority *= s_NonVisiblePriorityScale;
	
	// boost priority of flagged lights
	if (sceneLight.GetFlag(LIGHTFLAG_CAST_HIGHER_RES_SHADOWS))
		priority *= s_HiresFlagPriorityBoost; 
	
	return priority;
}


class ParaboloidShadowSortedLightInfo {
public:
	ParaboloidShadowSortedLightInfo() : m_Priority(0), m_LightIndex(-1), m_CacheIndex(-1), m_Flags(0) {}
	void Set(float priority, s16 lightIndex, s16 cacheIndex, u16 flags) {m_Priority= priority; m_LightIndex = lightIndex; m_CacheIndex = cacheIndex; m_Flags=flags;}
	float m_Priority;
	s16   m_LightIndex;
	s16   m_CacheIndex;
	u16   m_Flags;
	u16   m_Dummy16;	// for alignment
};
	
// second pass sort moves refresh candidates to the end (and sorts them by age), and give hires priority
int ParaboloidShadowSortedLightInfoCompareSecondPass(const ParaboloidShadowSortedLightInfo* A, const ParaboloidShadowSortedLightInfo* B)
{
	bool aIsHires = (A->m_Flags&SHADOW_FLAG_HIGHRES)!=0;
	bool bIsHires = (B->m_Flags&SHADOW_FLAG_HIGHRES)!=0;
	bool aIsRefreshOnly = (A->m_Flags&SHADOW_FLAG_REFRESH_CHECK_ONLY)!=0;
	bool bIsRefreshOnly = (B->m_Flags&SHADOW_FLAG_REFRESH_CHECK_ONLY)!=0;

	if (aIsRefreshOnly != bIsRefreshOnly) 
	{
		if (bIsRefreshOnly)
			return -1;				// B is refresh only, but A is not, so A wins
		else
			return 1;				// B is not refresh only, but A is, so b wins
	}
	else
	{
		if (aIsRefreshOnly)			// if both are refresh, they have a cache entry, so sort by it's age.
		{
			int aAge = (int)CParaboloidShadow::m_cachedEntries[A->m_CacheIndex].m_ageSinceUpdated;
			int bAge = (int)CParaboloidShadow::m_cachedEntries[B->m_CacheIndex].m_ageSinceUpdated;
			if (aAge!=bAge)
			{
				return (bAge < aAge) ? -1 :  1;  // want oldest cache
			}
		}
		
		if (aIsHires != bIsHires)
		{
			if (aIsHires)
				return -1;				// a is hires, b is not, a wins
			else
				return 1;				// a is not hires, b is, b wins
		}
		else
		{
			// just go by priority 
			float diff = (B->m_Priority - A->m_Priority);
			return (diff < 0.0f) ? -1 : ((diff >0.0f) ? 1: 0);
		}
	}
}

int ParaboloidShadowSortedLightInfoCompare(const ParaboloidShadowSortedLightInfo* A, const ParaboloidShadowSortedLightInfo* B)
{
	float diff = (B->m_Priority - A->m_Priority);
	return (diff < 0.0f) ? -1 : ((diff >0.0f) ? 1: 0);
}


inline bool CheckForHires(int count, float priority, u32 flags, bool isInCutscene)
{
	return (count<MAX_ACTIVE_HIRES_PARABOLOID_SHADOWS) && (priority>s_MinHiresThreshold || isInCutscene) && ((flags&LIGHTFLAG_CAST_ONLY_LOWRES_SHADOWS) == 0);
}


static BankBool s_SkipJumpCutTest = false;  

//  only during cutscene when the next frame is a camera cut, check to see if there are any shadow casting lights the new camera will see, that are not already added
void CheckForJumpCutLightsShadowLights(const grcViewport &VP, bool bInCutscene)
{
	if (s_SkipJumpCutTest)
		return;

	CLightSource * sceneLights = Lights::m_prevSceneShadowLights;
	int numLights = Lights::m_numPrevSceneShadowLights;

	BANK_ONLY(if(!DebugLights::m_freeze))
	{
		CLightEntity::Pool* lightEntityPool = CLightEntity::GetPool();

		const u32 maxNumLightEntities = (u32) lightEntityPool->GetSize();

		for(u32 i = 0; i < maxNumLightEntities; i++)
		{
			if(!lightEntityPool->GetIsFree(i))
			{
				CLightEntity* pLightEntity = lightEntityPool->GetSlot(i);

				if (const CEntity* pLightParent = pLightEntity->GetParent())
				{
					// see if it casts a shadow
					if ( pLightEntity->m_nFlags.bLightsCanCastStaticShadows==1 || pLightEntity->m_nFlags.bLightsCanCastDynamicShadows==1)
					{
						if (pLightParent->GetDrawable() && pLightEntity->Get2dEffectType() == ET_LIGHT)
						{
							if (const CLightAttr *pAttr = pLightEntity->GetLight())
							{
								if ((pAttr->m_flags & (LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS|LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS)) == 0) 
									continue;

								if (pAttr->m_lightType != LIGHT_TYPE_POINT && pAttr->m_lightType != LIGHT_TYPE_SPOT )
									continue;

								if ((pAttr->m_flags & LIGHTFLAG_CORONA_ONLY) != 0)
									continue;
								
								if (bInCutscene && (pAttr->m_flags & LIGHTFLAG_DONT_USE_IN_CUTSCENE) != 0) 
									continue;

								const bool ignoreDayNightSettings = (bool)pLightEntity->m_nFlags.bLightsIgnoreDayNightSetting || (bool)pLightParent->m_nFlags.bLightsIgnoreDayNightSetting;
								if (pLightEntity->CalcLightIntensity(ignoreDayNightSettings, pAttr) <= 0.0f)
									continue;
							}

							if (bInCutscene)
							{
								if (!pLightEntity->GetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE))
									continue; 

								if (!pLightParent->GetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE))
									continue; 
							}

							// is it visible now?		
							spdSphere sphere;
							pLightEntity->GetBoundSphere(sphere);

							if (VP.IsSphereVisible(sphere.GetV4Ref())) // should we also do a radius test? (rarely more than 10m-20m)
							{

								if (VP.GetAABBCullStatus(pLightEntity->GetBoundingBoxMin(),pLightEntity->GetBoundingBoxMax(),VP.GetFrustumLRTB(),VP.GetFrustumNFNF())!=cullOutside)
								{
									// is it already in the sceneLight list?
									fwUniqueObjId shadowTrackingID = pLightEntity->CalcShadowTrackingID();

									// look through Previous Shadow Scene Lights, to see if we were there
									int index;
									int duplicateLight = -1;
									
									for (index=0;index<numLights;index++)
									{
										if (sceneLights[index].GetShadowTrackingId()==shadowTrackingID)
										{
											if ((sceneLights[index].GetFlags() & (LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS|LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS)) == 0)
											{
												// we found the light in the previous light list, but it's shadow flags were turned off (probably it was farther than the shadow cutoff last frame)
												// we will add a new one with the proper flags and intensity for this frame
												duplicateLight = index;  // we'll need to copy the exclusion list below
											}
											break;
										}
									}

									if (index>=numLights || duplicateLight>=0)
									{
										int numLights = Lights::m_numPrevSceneShadowLights;

										pLightEntity->ProcessLightSource(true, false, true);
									
										if (duplicateLight>=0 && numLights!=Lights::m_numPrevSceneShadowLights) // if the light was really added and were duplicating an existing one
										{
											// we may need to copy the old exclusion list to this new light 
											if (Lights::m_prevSceneShadowLights[duplicateLight].GetShadowExclusionEntitiesCount()>0)
												Lights::m_prevSceneShadowLights[numLights].CopyShadowExclusionEntities(Lights::m_prevSceneShadowLights[duplicateLight]);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}


void CParaboloidShadow::Update()
{
#if RSG_PC
	if (GRCDEVICE.UsingMultipleGPUs())
	{
		bool bSkip = false;
		for (int i = 0; i < MAX_ACTIVE_PARABOLOID_SHADOWS; i++)
		{
			int static_cacheindex = m_activeEntries[i].m_staticCacheIndex;
			if (m_activeEntries[i].m_flags != SHADOW_FLAG_DONT_RENDER)
			{
				if (m_cachedEntries[static_cacheindex].m_gpucounter_static > 1)
				{
					if (gRenderThreadInterface.IsUsingRender())
						m_cachedEntries[static_cacheindex].m_gpucounter_static--;

					m_cachedEntries[static_cacheindex].m_status = CACHE_STATUS_GENERATING;
					bSkip = true;
				}
				else
				{
					m_cachedEntries[static_cacheindex].m_gpucounter_static = 0;
					m_cachedEntries[static_cacheindex].m_status = CACHE_STATUS_NOT_ACTIVE;
				}
			}
		}

		if (bSkip)
			return;
		else
		{
			for (int i = 0; i < MAX_ACTIVE_PARABOLOID_SHADOWS; i++)
				m_activeEntries[i].m_flags &= ~SHADOW_FLAG_ENABLED;
		}
	}
#endif

	const grcViewport &VP     = gVpMan.GetCurrentViewport()->GetGrcViewport();
	const Matrix34& eyeMatrix = RCC_MATRIX34(VP.GetCameraMtx());
	const Vector3   eyePos    = eyeMatrix.d; 
	const Vector3   eyeDir    = eyeMatrix.c; 
	bool isInterior			  = CPortal::IsInteriorScene();

	m_cacheUpdateSequence = (m_cacheUpdateSequence + 1)%(MAX_STATIC_CACHED_PARABOLOID_SHADOWS);

	// check for camera cuts (so we can try to stick the new lights in the previous light list)
	const bool bScriptRequestedJumpCut = m_ScriptRequestedJumpCut;
	m_ScriptRequestedJumpCut = false;
	const bool bOtherCameraCut = camInterface::GetFrame().GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
	const bool bInCutscene = CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning();
	const bool bCutsceneCameraCut = bInCutscene && CutSceneManager::GetInstance()->GetHasCutThisFrame();
	const bool bLookBackCam = (camInterface::IsDominantRenderedDirector(camInterface::GetGameplayDirector()) && camInterface::GetGameplayDirector().IsLookingBehind());
	const bool bSyncedSceneDirectorRendering = camInterface::GetSyncedSceneDirector().IsRendering();
	const bool bScriptDirectorRendering = camInterface::GetScriptDirector().IsRendering();
	const bool bPlayerControlled = !bInCutscene && !bSyncedSceneDirectorRendering && !bScriptDirectorRendering;
	static bool sLastLookBackCam = false;
	static bool sLastPlayerControlled = false;
	static Vector3 sLastEyePos;
	static Vector3 sLastViewDir;
	BANK_ONLY(static int sCutCount=0;)
	bool bCameraHasCut = false;
	bool bLongJumpCut = false;

	if (bCutsceneCameraCut || bOtherCameraCut || bScriptRequestedJumpCut)
	{
		bool isFalseCut = false;

		float distSquared = eyePos.Dist2(sLastEyePos);

		// during script director control, they mark everything as a cut, even if it was just a small translation combine with a view direction (as when moving the camera around the player in a shop)
		// we'll also check for false jumps when the cutscene returns to the player's view after a cutscene ends)
		if (!bScriptRequestedJumpCut && !bCutsceneCameraCut && bOtherCameraCut && (bLookBackCam==sLastLookBackCam) && (bSyncedSceneDirectorRendering || bScriptDirectorRendering || (bPlayerControlled && !sLastPlayerControlled)))
		{
			static float distCheckSquared = 0.83666f*0.83666f;
			if ( distSquared < distCheckSquared)
			{
				float dot = eyeDir.Dot(sLastViewDir);
				static float dotThreshold = 0.9903f;  // about 8 degrees direction change
				isFalseCut = (dot > dotThreshold);	
			}
		}
		if(!isFalseCut && (bInCutscene || (bLookBackCam!=sLastLookBackCam) || (bPlayerControlled && !sLastPlayerControlled) || bSyncedSceneDirectorRendering || bScriptDirectorRendering || bScriptRequestedJumpCut))
		{
			CheckForJumpCutLightsShadowLights(VP, bInCutscene);
			bCameraHasCut = true;
		
			static float longCheckSquared = 10.0f*10.0f; // 2M movement is a pretty big jump
			bLongJumpCut = (distSquared>longCheckSquared);
			
			BANK_ONLY(if (bInCutscene) sCutCount=3;) // for debugging single stepping a few frames after a cut helps find pops
		}
	}
	
	if (bInCutscene && !isInterior) // some scenes are not registering as interior when they start, so we pick up lot of street lights sometimes during the into cuts.
	{
		s32 probeRoomIdx;
		CInteriorInst*	pProbedIntInst = NULL;
		isInterior = CPortalTracker::ProbeForInterior(eyePos, pProbedIntInst, probeRoomIdx, NULL, CPortalTracker::FAR_PROBE_DIST);
	}

#if __BANK
	if (sCutCount>0)
	{
		sCutCount--; 
		if(s_BreakpointDebugCutsceneCameraCuts) // put in t break points on camera cut, and 2 frames after, to help spot shadow pops.
		{
			__debugbreak();
		}
	}
#endif

	sLastLookBackCam = bLookBackCam;
	sLastPlayerControlled = bPlayerControlled;
	sLastEyePos  = eyePos; 
	sLastViewDir = eyeDir; 

	CLightSource * sceneLights = Lights::m_prevSceneShadowLights;
	int numLights = Lights::m_numPrevSceneShadowLights;

	atFixedArray<ParaboloidShadowSortedLightInfo, MAX_STORED_LIGHTS> sortedLights;   

	for (int i = 0; i < MAX_STATIC_CACHED_PARABOLOID_SHADOWS; i++)
	{
	
#if __BANK
		if (s_DebugClearCache)
		{
			m_cachedEntries[i].m_trackingID = 0;
			m_cachedEntries[i].m_status  = CACHE_STATUS_UNUSED;
		}
#endif

		// [INFO]
		if (m_cachedEntries[i].m_status != CACHE_STATUS_UNUSED)
		{
			m_cachedEntries[i].m_status = CACHE_STATUS_NOT_ACTIVE;
			m_cachedEntries[i].m_lightIndex = -1;
			
			if (m_cachedEntries[i].m_ageSinceUsed<65535)
				m_cachedEntries[i].m_ageSinceUsed++;

			if (m_cachedEntries[i].m_ageSinceUpdated<65535)
				m_cachedEntries[i].m_ageSinceUpdated++;

			// if the camera moved a lot, reset the refresh age, so we reevaluate the higher priority lights first, instead of just using the round robin candidate. this picks up important LOD changes faster.
			if (bLongJumpCut) 
				m_cachedEntries[i].m_ageSinceUpdated = 65535; 
			
			// TODO: if we not active this frame, we might want to calc the priority anyway, to give a better cache reuse choice
			m_cachedEntries[i].m_priority   = -1000;
		}
	}


	if (bCameraHasCut) // if there was a camera cut, we will use the occlusion since our light lists are not really very good from last scene
		COcclusion::WaitForScanToFinish(SCAN_OCCLUSION_STORAGE_PRIMARY); 
	
	BANK_ONLY(bool firstDump = true;)

	// loop over the active lights, updating priority for each one
	for (int i = 0; i < numLights; i++)
	{
		const CLightSource& sceneLight = sceneLights[i];

#if __BANK
		if (s_SaveIsolatedLightShadowTrackingIndex)
		{
			if(DebugLights::m_debug && DebugLights::m_isolateLight)			
			{
				if (sceneLight.GetLightEntity() == DebugLights::m_currentLightEntity)
				{
					s_SingleOutTrackingID = sceneLight.GetShadowTrackingId();
					s_SaveIsolatedLightShadowTrackingIndex = false;
				}
			}
			else
			{
				s_SingleOutTrackingID = 0;
				s_SaveIsolatedLightShadowTrackingIndex = false;
			}
		}
		bool bSkipLight = (sceneLight.GetShadowTrackingId()!=s_SingleOutTrackingID && s_SingleOutTrackingID!=0) || (DebugLights::m_debug && DebugLights::m_isolateLight && (sceneLight.GetLightEntity() != DebugLights::m_currentLightEntity));
		bSkipLight = bSkipLight || SBudgetDisplay::GetInstance().IsLightDisabled(sceneLight) || CutSceneManager::GetInstance()->GetDebugManager().IsLightDisabled(sceneLight);
		if (bSkipLight)
			continue;
#endif // __BANK

		// if it's not a spot of point light it's will not cast a shadow
		if (sceneLight.GetType() != LIGHT_TYPE_POINT && sceneLight.GetType() != LIGHT_TYPE_SPOT )
			continue;

		// if it is not marked as casting a shadow, skip it
		if (!sceneLight.GetFlag(LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS | LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS))
			continue;

		if (sceneLight.GetFlag(LIGHTFLAG_DONT_USE_IN_CUTSCENE) && (CutSceneManager::GetInstance()  && CutSceneManager::GetInstance()->IsCutscenePlayingBackAndNotCutToGame())) 
			continue; // remove non cutscene lights on the first frame of the cutscene (can help with shadow pop in some cases)

		bool isVisible;

		float priority = CalcPriority(sceneLight, eyePos, VP, bCameraHasCut, isVisible); 

#if __BANK
		if (s_DumpShadowLights!=0)
		{
			if (firstDump)
			{
				firstDump = false;
				const Mat34V& camMtx = VP.GetCameraMtx();

				Displayf("Camera: Position = (%f, %f, %f), Dir = (%f, %f, %f), FOV = %f",
					camMtx.GetCol3().GetXf(), camMtx.GetCol3().GetYf(),camMtx.GetCol3().GetZf(), 
					camMtx.GetCol2().GetXf(),camMtx.GetCol2().GetYf(),camMtx.GetCol2().GetZf(), 
					2*(180.0/PI)*atan(VP.GetTanHFOV()));

				Displayf("Lights Before Processing:");
			}
			char flagString[256];

			formatf(flagString,"(%s%s%s%s)",				
				sceneLight.GetFlag(LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS)?"S":"",
				sceneLight.GetFlag(LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS)?"D":"",
				sceneLight.GetFlag(LIGHTFLAG_MOVING_LIGHT_SOURCE)?"M":"",
				sceneLight.GetFlag(LIGHTFLAG_CUTSCENE)?"C":""
			);

			Displayf("Light %2d: Pos = (%8.2f,%8.2f,%8.2f), dir = (%7.4f,%7.4f,%7.4f), intensity = %7.3f, type = %5s %-6s, interior =%4d, TrackingID = 0x%010lx, priority = %8.6f, %s visible", i,
				sceneLight.GetPosition().x,  sceneLight.GetPosition().y,  sceneLight.GetPosition().z, 	
				sceneLight.GetDirection().x, sceneLight.GetDirection().y, sceneLight.GetDirection().z,
				sceneLight.GetIntensity(),
				sceneLight.GetType()==LIGHT_TYPE_POINT?"point":"spot",
				flagString,
				sceneLight.GetInteriorLocation().GetInteriorProxyIndex(),
				sceneLight.GetShadowTrackingId(),
				priority,
				isVisible?"   ":"not"
				);
		}
#endif

		if ((bInCutscene || bSyncedSceneDirectorRendering || bScriptDirectorRendering) && isInterior)
		{
			if (sceneLight.GetFlag(LIGHTFLAG_EXTERIOR_ONLY))
			{
				priority *= 0.4f; // if we are inside, reduce the exterior only (usually street lights), they are huge and over power the light in the room we are in sometimes.
			}
		}

		u16 shadowFlags  = sceneLight.GetFlag(LIGHTFLAG_MOVING_LIGHT_SOURCE) ? SHADOW_FLAG_LATE_UPDATE : 0;
		    shadowFlags |= sceneLight.GetFlag(LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS) ? SHADOW_FLAG_DYNAMIC : 0;
		    shadowFlags |= sceneLight.GetFlag(LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS) ? SHADOW_FLAG_STATIC : 0;

		const bool bDynamicOnly = (shadowFlags&(SHADOW_FLAG_DYNAMIC|SHADOW_FLAG_STATIC))==SHADOW_FLAG_DYNAMIC;

		int cacheIndex = sceneLight.GetShadowIndex();
		if (cacheIndex !=-1 && (m_cachedEntries[cacheIndex].m_trackingID != sceneLight.GetShadowTrackingId()))
			cacheIndex = -1; // this one is no longer valid.

		// is we had an old cache entry update it's priority info.
		if (!bDynamicOnly && cacheIndex != -1)
		{
			u8 cacheStatus = CACHE_STATUS_NOT_ACTIVE;

			// if we moved, we need to flag this cache entry as needing regeneration
			if (DidLightMove(m_cachedEntries[cacheIndex].m_info, sceneLight))  // can't check size, since the cached entry does not save size.
			{
				cacheStatus = CACHE_STATUS_GENERATING;
				shadowFlags |= SHADOW_FLAG_LATE_UPDATE; // we' also want to mark this light for a late update later (in case it was not flagged by the user)
			}
			
			// [INFO]
			m_cachedEntries[cacheIndex].m_status     = cacheStatus;
			m_cachedEntries[cacheIndex].m_lightIndex = (s16)i;
			m_cachedEntries[cacheIndex].m_priority   = priority;
			if (isVisible)
				m_cachedEntries[cacheIndex].m_ageSinceUsed=0; // if it's visible reset it's age to 0

			// if it has a linked cache entry, update it's priority too, so it will not be reused unless really needed.
			s16 linkedEntry = m_cachedEntries[cacheIndex].m_linkedEntry;
			if (linkedEntry>=0)
			{
				if (m_cachedEntries[linkedEntry].m_linkedEntry == cacheIndex)
				{
					m_cachedEntries[linkedEntry].m_status     = cacheStatus;
					m_cachedEntries[linkedEntry].m_lightIndex = (s16)i;
					m_cachedEntries[linkedEntry].m_priority   = priority;
					if (isVisible)
						m_cachedEntries[linkedEntry].m_ageSinceUsed=0;
				}
			}
		}

		// let's not add them to the list unless they are visible (we still want the priority set even if not visible, so we keep the once very close longer)
		if (isVisible)
			sortedLights.Append().Set(priority, (s16)i, (s16)cacheIndex, shadowFlags);
	}

	// sort the list based on priority
	sortedLights.QSort(0,-1,ParaboloidShadowSortedLightInfoCompare);

#if __BANK
	if (s_DumpShadowLights!=0)
	{	
		if (!firstDump && s_DumpShadowLights!=-1)
			s_DumpShadowLights--;

		if (sortedLights.GetCount()>0)
			Displayf("Lights After processing:");

		for (int n = 0; n < sortedLights.GetCount(); n++)
		{
			const CLightSource & sceneLight = sceneLights[sortedLights[n].m_LightIndex];

			char flagString[256];

			formatf(flagString,"(%s%s%s%s)",				
				sceneLight.GetFlag(LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS)?"S":"",
				sceneLight.GetFlag(LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS)?"D":"",
				(sortedLights[n].m_Flags&SHADOW_FLAG_LATE_UPDATE)?"+L":"",
				sceneLight.GetFlag(LIGHTFLAG_CUTSCENE)?"C":""
				);

			Displayf("Light %2d: Pos = (%8.2f,%8.2f,%8.2f), dir = (%7.4f,%7.4f,%7.4f), intensity = %7.3f, type = %5s %-6s, interior =%4d, TrackingID = 0x%010lx, priority = %8.6f", n,
				sceneLight.GetPosition().x,  sceneLight.GetPosition().y,  sceneLight.GetPosition().z, 	
				sceneLight.GetDirection().x, sceneLight.GetDirection().y, sceneLight.GetDirection().z,
				sceneLight.GetIntensity(),
				sceneLight.GetType()==LIGHT_TYPE_POINT?"point":"spot",
				flagString,
				sceneLight.GetInteriorLocation().GetInteriorProxyIndex(),
				sceneLight.GetShadowTrackingId(),
				sortedLights[n].m_Priority
				);
		}
	}
#endif

	// make a pass over the list, and demote all the active entries that were high res, but now are not (to free up hires cache entries, so we don't steal one unnecessarily)
	int hiresCount = BANK_SWITCH(g_ParaboloidShadowDebugInt1,0);

	int loresCount = 0;
	int nonRefreshCount = 0;

	for (int n = 0; n < sortedLights.GetCount(); n++)
	{
		const CLightSource & sceneLight = sceneLights[sortedLights[n].m_LightIndex];
		int cacheIndex = sortedLights[n].m_CacheIndex;
		
		// is this entry a Hires candidate; todo add light flag for low res only shadow
		int usedHiresSlots = hiresCount+Max(0,loresCount-MAX_ACTIVE_LORES_PARABOLOID_SHADOWS); // some forced low res higher priority shadows may have borrowed the hires slots.
		bool isHires = CheckForHires(usedHiresSlots, sortedLights[n].m_Priority, sceneLight.GetFlags(), bInCutscene);

		if (isHires)
		{
			hiresCount++;
			sortedLights[n].m_Flags |= SHADOW_FLAG_HIGHRES;
			// TODO: now that there is more Hires cache space, we should probably look at making the link work for hi to low transitions well. there are lot of gotchas in do that though, so be careful
		}
		else
		{
			loresCount++;
			// so we're not hires now, were we previously?
			if (IsStaticCacheEntry(cacheIndex) && IsCacheEntryHighRes(cacheIndex))
			{
				// okay we need to demote this one. 
				s16 linkedEntry = m_cachedEntries[cacheIndex].m_linkedEntry;
			
				//  we can't use this one again, so release it
				sortedLights[n].m_CacheIndex = -1;

				// See if it has a linked cache we can reactivate.
				if (linkedEntry>=0)
				{
					if (m_cachedEntries[linkedEntry].m_linkedEntry == cacheIndex) // looks good
					{
						// re hook up the tracking ID.
						m_cachedEntries[linkedEntry].m_trackingID = sceneLight.GetShadowTrackingId();
						// update the list cache index
						sortedLights[n].m_CacheIndex = linkedEntry;
					}
				}

				// free up the old hires version so it's available
				InvalidateCacheEntry(cacheIndex); 
				cacheIndex = sortedLights[n].m_CacheIndex;
			}	
		}

		// if we hit the limit on dynamic shadows, we turn the rest into static only for now, so at least they can get a static cache entry set up
		if ((sortedLights.GetCount()>MAX_ACTIVE_PARABOLOID_SHADOWS) && nonRefreshCount > (MAX_ACTIVE_LORES_PARABOLOID_SHADOWS + MAX_ACTIVE_LORES_OVERLAPPING) - SHADOW_CACHE_ACTIVE_STATIC_UPDATE_RESERVE  )
		{
			sortedLights[n].m_Flags &= ~SHADOW_FLAG_DYNAMIC; // at least they might get a static cache entry so they can show some thing.
		}

		nonRefreshCount++;
		if(cacheIndex>=0 && ((sortedLights[n].m_Flags&(SHADOW_FLAG_STATIC_AND_DYNAMIC_MASK|SHADOW_FLAG_LATE_UPDATE))==SHADOW_FLAG_STATIC))  // looking for static only with a cache entry that does not need to change from camera movement
		{
			if ((IsCacheEntryHighRes(cacheIndex)==((sortedLights[n].m_Flags&SHADOW_FLAG_HIGHRES)!=0)) && (m_cachedEntries[cacheIndex].m_trackingID==sceneLight.GetShadowTrackingId())) // is the cache entry the same res as we need now
			{
				sortedLights[n].m_Flags |= SHADOW_FLAG_REFRESH_CHECK_ONLY;

				// don't count this one towards the total for disabling dynamic flag
				nonRefreshCount--;
			}
		}
	}

	// second sort, this time move refresh only to the back, sort by refresh off, hires flag, priority
	sortedLights.QSort(0,-1,ParaboloidShadowSortedLightInfoCompareSecondPass);

	// pick the best PARABOLOID_SHADOWS_RENDERPHASES candidates for updating.
	hiresCount = BANK_SWITCH(g_ParaboloidShadowDebugInt1,0);
	loresCount = 0;

	for (int n = 0; n < sortedLights.GetCount() && (hiresCount + loresCount < MAX_ACTIVE_PARABOLOID_SHADOWS); n++)
	{
		const CLightSource & sceneLight = sceneLights[sortedLights[n].m_LightIndex];

		u16 shadowFlags = sortedLights[n].m_Flags;

		const bool bStatic = (shadowFlags&SHADOW_FLAG_STATIC)!=0 && (sceneLight.GetFlag(LIGHTFLAG_MOVING_LIGHT_SOURCE)==0); // if were a moving light, we don't need (or want the overhead of ) a static cache.
	
		if((shadowFlags&SHADOW_FLAG_STATIC_AND_DYNAMIC_MASK)==0) // a demoted shadow has nothing left to do.
			continue;

		s16 cacheIndex = sortedLights[n].m_CacheIndex;

		bool isHires =  (sortedLights[n].m_Flags & SHADOW_FLAG_HIGHRES)!=0;
		if (isHires) 
		{
			hiresCount++;
		}
		else
		{
			int availableLowResSlots = (MAX_ACTIVE_LORES_PARABOLOID_SHADOWS + MAX_ACTIVE_LORES_OVERLAPPING) - loresCount - hiresCount;

			if (availableLowResSlots<=0)
				continue; // no room for more low res active shadows.
			loresCount++;
		}

		s16 linkedCacheIndex = -1;

		if (bStatic) // if it needs a static cache
		{
			// if we are hires and the cache we have is low res, we need to allocate a high res cache entry and link it to the low for later...
			if(cacheIndex>=0 && isHires && !IsCacheEntryHighRes(cacheIndex))
			{
				linkedCacheIndex = cacheIndex; // save this cache index for linking
				cacheIndex = -1;			   // force a new allocation
			}

			if(cacheIndex==-1)  // if it does not have one, let's assign one 
			{
				cacheIndex = (s16)GetFreeStaticCacheIndex(sortedLights[n].m_Priority, isHires); 
				// if we fail to get a hires should we drop back to low?
				if (!Verifyf(cacheIndex>=0 || !isHires, "Failed to find a hires cache entry, this should not be possible"))
				{
					if (linkedCacheIndex>=0)
					{
						cacheIndex = linkedCacheIndex; // take out old one back...
						linkedCacheIndex = -1;
					}
					else
					{
						// grab a low res one for now (but this will be given up next frame, since we are asking for hires)
						cacheIndex = (s16)GetFreeStaticCacheIndex(sortedLights[n].m_Priority, false); 
					}
				}

				if (cacheIndex>=0)
				{
					// [INFO]
					m_cachedEntries[cacheIndex].m_ageSinceUsed		 = 0;
					m_cachedEntries[cacheIndex].m_ageSinceUpdated	 = 0;
					m_cachedEntries[cacheIndex].m_trackingID		 = sceneLight.GetShadowTrackingId();
					m_cachedEntries[cacheIndex].m_status			 = CACHE_STATUS_GENERATING;
					m_cachedEntries[cacheIndex].m_lightIndex		 = -1;
					m_cachedEntries[cacheIndex].m_linkedEntry		 = linkedCacheIndex; // this should only be cleared for lores? for hires we need to remember the old entry
					m_cachedEntries[cacheIndex].m_priority			 =  sortedLights[n].m_Priority;
					m_cachedEntries[cacheIndex].m_staticCheckSum 	 = (u32)~0x0;
					m_cachedEntries[cacheIndex].m_staticVisibleCount = 65535;

					if (linkedCacheIndex>=0)
					{
						m_cachedEntries[cacheIndex].m_info = m_cachedEntries[linkedCacheIndex].m_info; // probably don't need this, but ...
						m_cachedEntries[linkedCacheIndex].m_linkedEntry = cacheIndex;	// link the new one to the old one
						m_cachedEntries[linkedCacheIndex].m_trackingID = 0; // disconnect the tracking of the lowres one so we don't pick it up accidentally
					}
				}
				else
				{
					// we failed to get any cache entry!  really this should not happen...
					continue; // now what?
				}
			}
			else
			{
			}
		}
		else if(cacheIndex>=0) // no static part, just do a quick check to see if we were static before (and still have a cache entry)
		{
			// we're don't need a static cache now, but just in case we did and it got turned off, free up our link
			if (m_cachedEntries[cacheIndex].m_trackingID == sceneLight.GetShadowTrackingId())
				InvalidateCacheEntry(cacheIndex);
			cacheIndex = -1;
		}

		fwInteriorLocation  interiorLocation = sceneLight.GetInteriorLocation();
		
		// for cutscene lights, if they don't provide an interior, we may have to search for one
		if (s_debugForceInteriorCheck || (sceneLight.GetFlag(LIGHTFLAG_CUTSCENE) && !interiorLocation.IsValid() && !sceneLight.GetFlag(LIGHTFLAG_EXTERIOR_ONLY)))
		{
			// it would be nice if this was cached at the higher level, but the light seems to be created every frame, so at least here we only do it for visible lights.
			s32 probeRoomIdx;
			CInteriorInst*	pProbedIntInst = NULL;
			CPortalTracker::ProbeForInterior(sceneLight.GetPosition(), pProbedIntInst, probeRoomIdx, NULL, CPortalTracker::FAR_PROBE_DIST);

			if (pProbedIntInst)
			{
				interiorLocation = CInteriorInst::CreateLocation( pProbedIntInst, probeRoomIdx );
			}
			else if (sceneLight.GetFlag(LIGHTFLAG_INTERIOR_ONLY)) // if we interior only and did not find one, there must be one, so at least try the camera's room
			{
				// fall back if we did not find one with a probe.
				if(CViewport * pViewport = gVpMan.GetCurrentViewport())
				{
					if (CInteriorProxy * pIntProxy = pViewport->GetInteriorProxy())
					{
						interiorLocation = CInteriorProxy::CreateLocation(pIntProxy, pViewport->GetRoomIdx());
					}
				}
			}
		}

		int activeIndex = hiresCount + loresCount -1;
		u16 activeCacheIndex = (u16)((isHires) ? hiresCount-1 : MAX_ACTIVE_CACHED_PARABOLOID_SHADOWS - loresCount);  

		m_activeEntries[activeIndex].Set(sceneLight, shadowFlags, true, interiorLocation);
		m_activeEntries[activeIndex].SetInfo(sceneLight);
		m_activeEntries[activeIndex].SetCaches(activeCacheIndex, cacheIndex); 
		m_cachedEntries[m_activeEntries[activeIndex].m_activeCacheIndex].m_priority = sortedLights[n].m_Priority;

		if (cacheIndex>=0)
		{
			// mark the associated static cache as a regen candidate unless we're already forcing regen.
			if (m_cachedEntries[cacheIndex].m_status != CACHE_STATUS_GENERATING)
			{

				m_cachedEntries[cacheIndex].m_status = CACHE_STATUS_REFRESH_CANDIDATE;
			}
		}
	}

	for (int i=hiresCount+loresCount; i < MAX_ACTIVE_PARABOLOID_SHADOWS; i++)
	{
		m_activeEntries[i].m_flags = SHADOW_FLAG_DONT_RENDER;
		m_activeEntries[i].m_trackingID = 0;
	}

	BANK_ONLY(BankUpdate());

#if RSG_PC
	if (GRCDEVICE.UsingMultipleGPUs() && g_bRegenCache)
	{
		if (gRenderThreadInterface.IsUsingRender())
		{
			g_bRegenCache = false;
			Warningf("Regen Cache!!!!!!!\n");
		}
	}
#endif
}

bool CParaboloidShadow::UsingAsyncStaticCacheCopy()
{
	return s_AsyncStaticCacheCopy;
}

#if CACHE_USE_MOVE_ENGINE

void InsertDMAFence()
{
	ID3D11DmaEngineContextX * dmaContext = s_useDMA1 ? GRCDEVICE.GetDmaEngine1() : GRCDEVICE.GetDmaEngine2();
	CParaboloidShadow::sm_AsyncDMAFenceD3D = (UINT64)dmaContext->InsertFence(0);
}

void CParaboloidShadow::InsertWaitForAsyncStaticCacheCopy()
{
	g_grcCurrentContext->InsertWaitOnFence(0, sm_AsyncDMAFenceD3D);
}

#elif DEVICE_GPU_WAIT

static void MarkDMAFencePending()
{
	GRCDEVICE.GpuMarkFencePending(CParaboloidShadow::sm_AsyncDMAFence);
}

static void InsertDMAFence()
{
	GRCDEVICE.GpuMarkFenceDone(CParaboloidShadow::sm_AsyncDMAFence, grcDevice::GPU_WRITE_FENCE_AFTER_SHADER_BUFFER_WRITES);
}

void CParaboloidShadow::InsertWaitForAsyncStaticCacheCopy()
{
	GRCDEVICE.GpuWaitOnFence(sm_AsyncDMAFence, grcDevice::GPU_WAIT_ON_FENCE_INPUT_SHADER_FETCHES);
	FlushStaticCacheCopy();
}

void CParaboloidShadow::FlushStaticCacheCopy()
{
#if RSG_ORBIS
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP();
	GRCDEVICE.SynchronizeComputeToGraphics(); // temp extra sync for now until GpuWaitOnPreviousWrites() works 100%
#elif RSG_DURANGO
	g_grcCurrentContext->InsertWaitUntilIdle(0);
	g_grcCurrentContext->FlushGpuCacheRange(static_cast<UINT>(D3D11_FLUSH_TEXTURE_L1_INVALIDATE|D3D11_FLUSH_TEXTURE_L2_WRITEBACK), 0, 0);
#endif
}

#endif

// add calls the the drawlist to copy the static cache RTs to the dynamic cache as needed.
// done ahead of time with DMA, so we don't have to wait.
void CParaboloidShadow::UpdateActiveCachesfromStatic()
{
	if (s_AsyncStaticCacheCopy)
	{
		DLCPushTimebar("Shadow Async Copy");

#if !CACHE_USE_MOVE_ENGINE && DEVICE_GPU_WAIT
		bool fenceMarkedPending = false;
#endif

		// loop over the active shadows.
		for (int activeIndex = 0;activeIndex<MAX_ACTIVE_PARABOLOID_SHADOWS; activeIndex++)
		{	
			const CParaboloidShadowActiveEntry& entry = CParaboloidShadow::m_activeEntries[activeIndex];

			// if this shadow has a static and Dynamic cache entry, lets start a DMA copy so the static is copied to the dynamic in time for use.
			if ((entry.m_flags&SHADOW_FLAG_ENABLED) && ((entry.m_flags&SHADOW_FLAG_STATIC_AND_DYNAMIC_MASK) == SHADOW_FLAG_STATIC_AND_DYNAMIC))
			{
				const int staticCacheIndex = entry.m_staticCacheIndex;
				const int activeCacheIndex = entry.m_activeCacheIndex;

				if (staticCacheIndex != -1 && activeCacheIndex != -1 )
				{
					// if the static is regenerating we can skip the copy. NOTE: if it is a candidate for refresh, we may copy twice, but that's not very common
					if (CParaboloidShadow::m_cachedEntries[staticCacheIndex].m_status!=CACHE_STATUS_GENERATING ) 
					{
#if !CACHE_USE_MOVE_ENGINE && DEVICE_GPU_WAIT
						if (!fenceMarkedPending)
						{
							DLC_Add(MarkDMAFencePending);
							fenceMarkedPending = true;
						}
#endif
						int faceCount = CParaboloidShadow::GetFacetCount(entry.m_info.m_type);
						DLC_Add(CParaboloidShadow::FetchCacheMap, staticCacheIndex, activeCacheIndex, faceCount, entry.m_info.m_type == SHADOW_TYPE_SPOT, false); 
					}
				}
			}
		}

#if CACHE_USE_MOVE_ENGINE
		DLC_Add(InsertDMAFence);
#elif DEVICE_GPU_WAIT
		if (fenceMarkedPending)
			DLC_Add(InsertDMAFence);
#endif

		DLCPopTimebar();
	}
}

void CParaboloidShadow::PreRender()
{
	// nothing
}



void CParaboloidShadow::BeginShadowZPass(int s, int facet)
{
	// Yes this does seem a bit wrong closing the scope here, since the viewport
	// needs to be preserved.  But it is ok since the viewport will regenerate
	// in the command buffer if necessary.
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	

	Assert(s >= 0);

	const int buf = GetRenderBufferIndex();
	const CShadowInfo& info = s_paraboloidShadowMapActive[buf][s].m_info;

	if (info.m_type == SHADOW_TYPE_POINT || info.m_type == SHADOW_TYPE_HEMISPHERE)
	{
		SetShadowZShaderParamsPoint(info, facet);
	}
	else // if (info.m_type == CShadowInfo::SHADOW_TYPE_SPOT)
	{
		SetShadowZShaderParamsSpot(info);
	}

	grmModel::SetForceShader(m_ShadowZShaderPointCM);
	grmShaderFx::SetForcedTechniqueGroupId(-1);
	CParaboloidShadow::SetDefaultStates();
}

void CParaboloidShadow::SetShadowZShaderParamData(const CShadowInfo& info)
{
	CShaderLib::SetGlobalVarInContext(m_shadowParam0_ID, Vec4V(info.m_shadowMatrix.d(),ScalarV(1.0f/info.m_shadowDepthRange)));
	CShaderLib::SetGlobalVarInContext(m_shadowParam1_ID, Vec4V(ScalarV(info.m_depthBias),ScalarV(info.m_slopeScaleDepthBias),ScalarV(0.123456789f),ScalarV(.987654321f)));
}

void CParaboloidShadow::SetShadowZShaderParamsPoint(const CShadowInfo& info, int facet)
{
	SetShadowZShaderParamData(info);

	// set up a per facet viewport so edge clipping, etc works as normal
	if(info.m_type==SHADOW_TYPE_HEMISPHERE)
	{
		SetupHemiSphereVP(s_VP[g_RenderThreadIndex], info, facet);
	}
	else
	{
		s_VP[g_RenderThreadIndex].Perspective(90.0f,1.0f,info.m_nearClip,info.m_shadowDepthRange); 
		s_VP[g_RenderThreadIndex].SetPerspectiveShear(0.0f,0.0f); // in case a hemisphere light used the VP last
	}

	Mat34V shadowMatrix; 
	GetCMShadowFacetMatrix(shadowMatrix, facet, &info); 
	
	s_VP[g_RenderThreadIndex].SetCameraMtx(shadowMatrix); 
	s_VP[g_RenderThreadIndex].SetWorldIdentity(); 
	s_VP[g_RenderThreadIndex].ResetWindow();				// not sure why, but sometimes the viewport did not pick up the rendertarget rez change
	grcViewport::SetCurrent(&s_VP[g_RenderThreadIndex]);	// make it current so the model culling works.
	Mat44V viewMtx = s_VP[g_RenderThreadIndex].GetViewMtx();
	viewMtx.SetCol3(Vec4V(V_ZERO_WONE));
	
	Mat44V viewProjMtx;	
	CalcViewProjMtx(viewProjMtx, info.m_shadowMatrixP[GetProjectionMtxIndex(info.m_type,facet)], viewMtx);
	CShaderLib::SetGlobalVarInContext(m_shadowMatrix_ID, viewProjMtx); // send down the ProjMtx with translation removed	
}

void CParaboloidShadow::SetShadowZShaderParamsSpot(const CShadowInfo& info)
{
	SetShadowZShaderParamData(info);
	CShaderLib::SetGlobalVarInContext(m_shadowMatrix_ID, info.m_shadowMatrixP[0]);
}


void CParaboloidShadow::UpdateAfterPreRender()
{
	// check for moving lights and update info if needed
	for (int i=0; i < MAX_ACTIVE_PARABOLOID_SHADOWS; i++)
	{
		if (m_activeEntries[i].m_flags != SHADOW_FLAG_DONT_RENDER)
			CheckAndUpdateMovedLight(i);
	}

	const int buf = GetUpdateBufferIndex();
	sysMemCpy(s_paraboloidShadowMapActive[buf], m_activeEntries, sizeof(m_activeEntries));
	sysMemCpy(s_paraboloidShadowMapCache[buf], m_cachedEntries, sizeof(m_cachedEntries));
}

eDeferredShadowType CParaboloidShadow::GetDeferredShadowType(int s)
{
	Assert(s>=0);
	
	const eLightShadowType type = GetShadowInfo(s).m_type;

	return GetDeferredShadowTypeCommon(type);
}


void  CParaboloidShadow::CalcViewProjMtx( Mat44V_InOut viewProjMtx, const CShadowInfo &info, int facet)
{
	Mat34V shadowMatrix;
	GetCMShadowFacetMatrix(shadowMatrix, facet, &info); 
	CalcViewProjMtx(viewProjMtx,  info.m_shadowMatrixP[GetProjectionMtxIndex(info.m_type, facet)], shadowMatrix);
}

void  CParaboloidShadow::CalcViewMtx(Mat44V_InOut viewMtx,  Mat34V_ConstRef shadowMtx)
{
	Mat34V temp;		
	InvertTransformOrtho(temp, shadowMtx);  // TODO: we should build invert transform directly, since we just need the rotation part now... 

	viewMtx.SetCols(Vec4V(temp.a(),ScalarV(V_ZERO)),	// TODO: this is a bit silly. GetCMShadowFacetMatrix should just return a Mat44V 
					Vec4V(temp.b(),ScalarV(V_ZERO)),
					Vec4V(temp.c(),ScalarV(V_ZERO)),
					Vec4V(V_ZERO_WONE));				// we strip the view transform out now and do it explicitly in the shader, before the rotation
}


void  CParaboloidShadow::CalcViewMtx(Mat44V_InOut viewMtx, const CShadowInfo &info, int facet)
{
	Mat34V shadowMatrix;
	GetCMShadowFacetMatrix(shadowMatrix, facet, &info); 

	CalcViewMtx(viewMtx,shadowMatrix);
}

void  CParaboloidShadow::CalcViewProjMtx(Mat44V_InOut viewProjMtx, Mat44V_ConstRef projMtx, Mat44V_ConstRef viewMtx)
{
	Multiply(viewProjMtx, projMtx, viewMtx);
}

void  CParaboloidShadow::CalcViewProjMtx(Mat44V_InOut viewProjMtx, Mat44V_ConstRef projMtx, Mat34V_ConstRef shadowMtx)
{
	Mat44V viewMtx;
	CalcViewMtx(viewMtx,shadowMtx);
	Multiply(viewProjMtx, projMtx, viewMtx);
}

void SetShadowData(const CShadowInfo& info, Vec3V_ConstRef cameraPos, int cacheIndex, float blur, Mat44V_Ref out)
{
	int shadowArrayIndex = CParaboloidShadow::GetCacheEntryRTIndex(cacheIndex);
	bool isHires = CParaboloidShadow::IsCacheEntryHighRes(cacheIndex);
	
	Assert(FPIsFinite(info.m_tanFOV));
	Assert(FPIsFinite(info.m_shadowDepthRange) && info.m_shadowDepthRange != 0.0f);

	out.SetCol0(Vec4V(info.m_shadowMatrix.GetCol0()/ScalarV(info.m_type==SHADOW_TYPE_SPOT?info.m_tanFOV:1.0f), ScalarV(float(info.m_type)))); // spot lights have Tans fov included in the mtx.
	out.SetCol1(Vec4V(info.m_shadowMatrix.GetCol1()/ScalarV(info.m_type==SHADOW_TYPE_SPOT?info.m_tanFOV:1.0f), ScalarV(float(shadowArrayIndex)+(isHires?0.5f:0.0f)))); // store hi res flag in fractional part
	out.SetCol2(Vec4V(info.m_shadowMatrix.GetCol2(), ScalarV(1.0f/info.m_shadowDepthRange)));
	out.SetCol3(Vec4V(cameraPos-info.m_shadowMatrix.GetCol3(), ScalarV(blur/(float)CParaboloidShadow::GetShadowRes())));
}

grcRenderTarget* CParaboloidShadow::SetDeferredShadow(int s, float blur)
{
	Assert(s >= 0);

	int cacheIndex = s;

	const CShadowInfo& info = GetShadowInfoAndCacheIndex(s,cacheIndex);
	grcRenderTarget * shadowTarget = GetCacheEntryRT(cacheIndex, info.m_type==SHADOW_TYPE_SPOT);
	
	Mat44V data; // this will be a 2d array at some point holding all the data
	SetShadowData(info, gVpMan.GetCurrentGameGrcViewport()->GetCameraPosition(), cacheIndex, blur, data);

	Assert(!gDrawListMgr->IsBuildingDrawList());

	grcEffect::SetGlobalVar(m_shadowRenderData_ID, &data, 1); // just the first slot is used during deferred rendering

// for deferred pass, just use the first global textures
#if USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS
	grcEffect::SetGlobalVar(m_shadowTextureCacheId[kShadowHiresCubeMapTargets],	m_RenderTargets[kShadowHiresCubeMapTargets]);
	grcEffect::SetGlobalVar(m_shadowTextureCacheId[kShadowHiresSpotTargets],	m_RenderTargets[kShadowHiresSpotTargets]); // probably should not set these each time just once before we start
	grcEffect::SetGlobalVar(m_shadowTextureCacheId[kShadowLoresCubeMapTargets],	m_RenderTargets[kShadowLoresCubeMapTargets]);
	grcEffect::SetGlobalVar(m_shadowTextureCacheId[kShadowLoresSpotTargets],	m_RenderTargets[kShadowLoresSpotTargets]);
#else
	grcEffect::SetGlobalVar(m_shadowTextureCacheId[0],   GetCacheEntryRT(cacheIndex, true));
	grcEffect::SetGlobalVar(m_shadowCMTextureCacheId[0], GetCacheEntryRT(cacheIndex, false));
#endif

	float shadowSizeX = 1.0f;
	float shadowSizeY = 1.0f;

	if(shadowTarget)
	{
		shadowSizeX = (float)shadowTarget->GetWidth();
		shadowSizeY = (float)shadowTarget->GetHeight();
	}

	grcEffect::SetGlobalVar(m_shadowTexParam_ID, Vec4V(shadowSizeX, shadowSizeY, 1.0f/shadowSizeX, 1.0f/shadowSizeY));
	grcEffect::SetGlobalVar(m_LocalDitherTextureId, CRenderPhaseCascadeShadowsInterface::GetDitherTexture()); // this does not change and could be set up front

	return shadowTarget;
}

void CParaboloidShadow::SetForwardLightShadowData(int shadowIndices[], int numLights)
{
	Mat44V data[LOCAL_SHADOWS_MAX_FORWARD_SHADOWS]; // this may change if we get arrays working. we could send all the shadow data down, and just have the lights set an index
	Vec3V_ConstRef camerPos = gVpMan.GetCurrentGameGrcViewport()->GetCameraPosition();

	for (int i=0;i<LOCAL_SHADOWS_MAX_FORWARD_SHADOWS;i++)
	{
		if (i < numLights && shadowIndices[i] >= 0 BANK_ONLY(&& !s_SkipForwardShadow)) 
		{
			int cacheIndex =  shadowIndices[i];
	 		const CShadowInfo& info = GetShadowInfoAndCacheIndex( shadowIndices[i],cacheIndex);
			SetShadowData(info, camerPos, cacheIndex, 0, data[i]);
#if !USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS
			grcEffect::SetGlobalVar(m_shadowTextureCacheId[i], GetCacheEntryRT(cacheIndex, true));
			grcEffect::SetGlobalVar(m_shadowCMTextureCacheId[i], GetCacheEntryRT(cacheIndex, false));
#endif
		}
		else
		{
			data[i].SetCol0(Vec4V(ScalarV(V_ZERO), ScalarV(V_ZERO), ScalarV(V_ZERO), ScalarV(float(SHADOW_TYPE_NONE))));  // Disable shadows for unused lights
			data[i].SetCol1(Vec4V(V_ZERO));
			data[i].SetCol2(Vec4V(V_ZERO));
			data[i].SetCol3(Vec4V(V_ZERO));

#if !USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS
			grcEffect::SetGlobalVar(m_shadowTextureCacheId[i], grcTexture::NoneDepth);
			grcEffect::SetGlobalVar(m_shadowCMTextureCacheId[i], m_DummyCubemap); 
#endif
		}

	}
	grcEffect::SetGlobalVar(m_shadowRenderData_ID, data, LOCAL_SHADOWS_MAX_FORWARD_SHADOWS);

#if RSG_PC
	grcEffect::SetGlobalVar(m_shadowTexParam_ID, Vec4V(ScalarV(m_EnableForwardShadowsOnGlass?1.0f:0.0f),ScalarV(V_ZERO),ScalarV(V_ZERO),ScalarV(V_ZERO)));
#endif

#if USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS
	// TODO: should not set these each time just once before we start of forward pass
	grcEffect::SetGlobalVar(m_shadowTextureCacheId[kShadowHiresCubeMapTargets],	m_RenderTargets[kShadowHiresCubeMapTargets]);
	grcEffect::SetGlobalVar(m_shadowTextureCacheId[kShadowHiresSpotTargets],	m_RenderTargets[kShadowHiresSpotTargets]); 
	grcEffect::SetGlobalVar(m_shadowTextureCacheId[kShadowLoresCubeMapTargets],	m_RenderTargets[kShadowLoresCubeMapTargets]);
	grcEffect::SetGlobalVar(m_shadowTextureCacheId[kShadowLoresSpotTargets],	m_RenderTargets[kShadowLoresSpotTargets]);
#endif

}



int CParaboloidShadow::GetCacheEntryRTIndex(int USE_TEXTURE_ARRAYS_ONLY(cacheIndex))
{
#if USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS
	if (AssertVerify(cacheIndex>=0)
	{
		return m_cachedEntries[cacheIndex].m_renderTargetIndex;
	}
	else
#endif
	return 0;
}

grcRenderTarget* CParaboloidShadow::GetCacheEntryRT(int cacheIndex, bool getSpotTarget)
{
	if (AssertVerify(cacheIndex>=0))
	{
#if USE_LOCAL_LIGHT_SHADOW_TEXTURE_ARRAYS
	return m_cachedEntries[cacheIndex].m_renderTarget[getSpotTarget?1:0]; // this points to the base array (hi/lo res, etc)
#else
	return (*m_cachedEntries[cacheIndex].m_renderTarget[getSpotTarget?1:0])[m_cachedEntries[cacheIndex].m_renderTargetIndex]; // this points to a specific entry
#endif
	}
	else
	{
		return NULL;
	}
}

const CShadowInfo& CParaboloidShadow::GetShadowInfoAndCacheIndex(int s, int &cacheIndex)
{			 
	if (s < MAX_ACTIVE_PARABOLOID_SHADOWS)
		cacheIndex = s_paraboloidShadowMapActive[GetRenderBufferIndex()][s].m_activeCacheIndex;				//  active shadows no longer have a one for one entry into the active part of the cache
	else
		cacheIndex = s - MAX_ACTIVE_PARABOLOID_SHADOWS;

	return GetShadowInfo(s);
}

bool CParaboloidShadow::IsCacheEntryHighRes(int cacheIndex) 
{
	// high res static cache, low res static cache, high res active, low res active,
	return ((cacheIndex>=0 && cacheIndex<MAX_STATIC_CACHED_HIRES_PARABOLOID_SHADOWS) || 
			(cacheIndex>=MAX_STATIC_CACHED_PARABOLOID_SHADOWS && cacheIndex<MAX_STATIC_CACHED_PARABOLOID_SHADOWS+MAX_ACTIVE_HIRES_PARABOLOID_SHADOWS));
}

CShadowInfo& CParaboloidShadow::GetShadowInfo(int s)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	Assert(s >= 0);

	const int buf = GetRenderBufferIndex();

	return (s < MAX_ACTIVE_PARABOLOID_SHADOWS)? s_paraboloidShadowMapActive[buf][s].m_info : s_paraboloidShadowMapCache[buf][s-MAX_ACTIVE_PARABOLOID_SHADOWS].m_info;
}

void CParaboloidShadow::GetCMShadowFacetMatrix(Mat34V & mtx, int facet, const CShadowInfo * info)
{
	facet = (facet>=0)?facet:0;

	static const Mat34V s_RotMatrices[6] = {
					Mat34V( 0, 0, 1,  0,-1, 0,  1, 0, 0,  0, 0, 0),  // right - Cubemap Face 0 
					Mat34V( 0, 0,-1,  0,-1, 0, -1, 0, 0,  0, 0, 0),  // left  - Cubemap Face 1
					Mat34V( -1, 0, 0, 0, 0, 1,  0, 1, 0,  0, 0, 0),  // down  - Cubemap Face 2
					Mat34V( -1, 0, 0, 0, 0,-1,  0,-1, 0,  0, 0, 0),  // up    - Cubemap Face 3
					Mat34V(-1, 0, 0,  0,-1, 0,  0, 0, 1,  0, 0, 0),  // front - Cubemap Face 4 
					Mat34V( 1, 0, 0,  0,-1, 0,  0, 0,-1,  0, 0, 0)   // back  - Cubemap Face 5
	};
		
	mtx = s_RotMatrices[facet];
	
	if (info)
	{
		Transform3x3( mtx, info->m_shadowMatrix, mtx);
		mtx.Setd(info->m_shadowMatrix.d());	
	}
}


int CParaboloidShadow::UpdateLight(const CLightSource& light)
{
	// don't return and active cache index, we just want the static ones
	for (int i = 0; i < MAX_STATIC_CACHED_PARABOLOID_SHADOWS; i++)
	{
		if (light.GetShadowTrackingId() == m_cachedEntries[i].m_trackingID)
		{
			return i;
		}
	}
	return -1;
}
	

int CParaboloidShadow::GetLightShadowIndex(const CLightSource &light, bool visibleLastFrame)
{
	fwUniqueObjId shadowTrackID = light.GetShadowTrackingId();
	
	// if there is no shadow tracking ID, don't bother looking
	if (shadowTrackID == 0)
		return -1;

	int flags = light.GetFlags();
	
	// if this light has dynamic shadows, check if it's got an active shadow
	if ((flags & LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS) || light.GetFlag(LIGHTFLAG_MOVING_LIGHT_SOURCE) )  // moving light source just render directly to the dynamic cache.
	{
		for (int d = 0; d < MAX_ACTIVE_PARABOLOID_SHADOWS; d++)  
		{
			const int buf = GetRenderBufferIndex();
			if (shadowTrackID == s_paraboloidShadowMapActive[buf][d].m_trackingID)
			{
				if ( (s_paraboloidShadowMapActive[buf][d].m_flags&SHADOW_FLAG_REFRESH_CHECK_ONLY)==0)  // if it's an active entry that is just updating a static buffer, let them use the static version below (sometimes we do not update the dynamic buffer part)
					return d;
			}
		}
	}

	int shadowMapIndex = light.GetShadowIndex();
	
	// if we had a static cache entry at some point, see if it's still good. (only if it if it's not moved)
	if (shadowMapIndex != -1 && (flags & LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS))
	{
		const int buf = GetRenderBufferIndex();
		if (s_paraboloidShadowMapCache[buf][shadowMapIndex].m_trackingID == shadowTrackID)
		{
			// if got an old static shadow cache entry and we were not visible last frame, we need to make sure the old cache entry is okay to use
			if ( !visibleLastFrame )
			{
				// if age is 0, then we already checked for movement, etc. (also, should we skip the test unless we're flagged as moving?)
				if (s_paraboloidShadowMapCache[buf][shadowMapIndex].m_ageSinceUsed > 0 /* && (flags & LIGHTFLAG_MOVING_LIGHT_SOURCE) */)
				{	
					if (CParaboloidShadow::DidLightMove(s_paraboloidShadowMapCache[buf][shadowMapIndex].m_info, light))
						return -1;
				}
			}

			return shadowMapIndex + MAX_ACTIVE_PARABOLOID_SHADOWS;  // number > active are to the static cache..
		}
	}	

	return -1;
}

DECLARE_MTR_THREAD static bool s_prevLighting;

void CParaboloidShadow::BeginRender()
{
	s_prevLighting = grcLightState::SetEnabled(false);
	SetDefaultStates();
}

void CParaboloidShadow::EndRender()
{
	grcLightState::SetEnabled(s_prevLighting);
}


Mat44V_Out CParaboloidShadow::GetFacetFrustumLRTB(int s, int facet)
{
	CParaboloidShadowActiveEntry & entry = m_activeEntries[s];
	Assert(entry.m_info.m_type == SHADOW_TYPE_POINT || entry.m_info.m_type  == SHADOW_TYPE_HEMISPHERE);

	grcViewport VP;
	VP.Perspective(90.0f, 1.0f, 0.01f, entry.m_info.m_shadowDepthRange);  
	Mat34V shadowMatrix; 
	GetCMShadowFacetMatrix(shadowMatrix, facet, &entry.m_info); 
	VP.SetCameraMtx(shadowMatrix);
	return VP.GetFrustumLRTB();
}


void CParaboloidShadow::SetDefaultStates()
{
	SetDepthMode(eDepthModeEnableCompare);
	SetColorWrite(eColorWriteDefault);
	SetCullMode(eCullModeBack);
}

void CParaboloidShadow::SetDepthMode(CParaboloidShadow::eDepthModes mode)
{
	grcStateBlock::SetDepthStencilState(m_state_DSS[mode]);
}

void CParaboloidShadow::SetColorWrite(CParaboloidShadow::eColorWriteModes mode)
{
	grcStateBlock::SetBlendState(m_state_B[mode], ~0U, ~0ULL);
	CShaderLib::SetAlphaTestRef(0.f);
}

void CParaboloidShadow::SetCullMode(CParaboloidShadow::eCullModes mode)
{
	grcStateBlock::SetRasterizerState(m_state_R[(mode == eCullModeNone) ? eCullModeNone : (s_FlipCull?(1-mode):mode)]);
}



void CParaboloidShadow::EndShadowZPass()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP();
	grmModel::SetForceShader((grmShader*)NULL);
	grmShaderFx::SetForcedTechniqueGroupId(-1);
	CParaboloidShadow::SetDefaultStates();

#if RSG_ORBIS // reenable compression
// 	sce::Gnm::DbRenderControl dbRenderControl;
// 	dbRenderControl.init();
// 	gfxc.setDbRenderControl(dbRenderControl);

#endif

}

int CParaboloidShadow::GetFreeStaticCacheIndex(float priority, bool hires)
{
	// TODO: Need to add support for lights with low/hi priority set

	float minPriority		= priority;
	int   minIndex			= -1;
	float minNonvisPriority	= FLT_MAX;
	int   minNonVisIndex	= -1;
	u16   oldestAge			= 0;
	int   oldestAgeIndex	= -1;
	u16   oldestPriorityAge	= 0;
	u16   oldestNonVisAge	= 0;

	int start = ((hires)?0:MAX_STATIC_CACHED_HIRES_PARABOLOID_SHADOWS);
	int end =   ((hires)?MAX_STATIC_CACHED_HIRES_PARABOLOID_SHADOWS:MAX_STATIC_CACHED_PARABOLOID_SHADOWS);

	for (int i = start; i < end; i++)
	{
		if (m_cachedEntries[i].m_status == CACHE_STATUS_UNUSED) // these entries are no longer used
		{
			if (m_cachedEntries[i].m_trackingID == 0) // if the entry is not tracking anything, just use it.
				return i;

			if (m_cachedEntries[i].m_ageSinceUsed >= oldestAge)
			{
				oldestAge      = m_cachedEntries[i].m_ageSinceUsed;
				oldestAgeIndex = i;
			}
		}
		else if (oldestAgeIndex==-1) //  if we have not found a "free" entry, we'll look for lower priority entries
		{
			// look to see if any of the active lights are lower priority than us
			if (m_cachedEntries[i].m_priority <= minPriority)  
			{
				// update if we're lower priority or the same priority and older
				if (m_cachedEntries[i].m_priority < minPriority || m_cachedEntries[i].m_ageSinceUsed>oldestPriorityAge)
				{
					minPriority		  = m_cachedEntries[i].m_priority;
					oldestPriorityAge = m_cachedEntries[i].m_ageSinceUsed;
					minIndex		  = i;
				}
			}

			// if m_ageSinceUsed>0, it is not visible this frame
			if (m_cachedEntries[i].m_ageSinceUsed > 0 && m_cachedEntries[i].m_priority <= minNonvisPriority)  
			{
				// update if we're lower priority or the same priority and older
				if (m_cachedEntries[i].m_priority < minNonvisPriority || m_cachedEntries[i].m_ageSinceUsed>oldestNonVisAge) 
				{
					minNonvisPriority = m_cachedEntries[i].m_priority;
					oldestNonVisAge   = m_cachedEntries[i].m_ageSinceUsed;
					minNonVisIndex	  = i;
				}

			}
		}
	}

	if (oldestAgeIndex != -1)		// if we found an non active one, return the oldest of those
		return oldestAgeIndex;
	else if (minIndex != -1)		// other wise of the current lights, return the lowest priority that is lower than our priority
		return minIndex;	 
	else
		return minNonVisIndex;		// if there are none lower than us, return the lowest non visible priority, or -1 if there an non of those (unlikely)
}

void CParaboloidShadow::InvalidateCacheEntry(int cacheIndex)
{
	if (cacheIndex>=0 && cacheIndex<MAX_CACHED_PARABOLOID_SHADOWS)
	{
		if (m_cachedEntries[cacheIndex].m_linkedEntry>0 && 
			m_cachedEntries[m_cachedEntries[cacheIndex].m_linkedEntry].m_linkedEntry==cacheIndex) // if we were link, break the link...
				m_cachedEntries[m_cachedEntries[cacheIndex].m_linkedEntry].m_linkedEntry = -1;

		m_cachedEntries[cacheIndex].m_ageSinceUpdated = 0;
		m_cachedEntries[cacheIndex].m_ageSinceUsed = 0;
		m_cachedEntries[cacheIndex].m_trackingID   = 0;
		m_cachedEntries[cacheIndex].m_status       = CACHE_STATUS_UNUSED;
		m_cachedEntries[cacheIndex].m_lightIndex   = -1;
		m_cachedEntries[cacheIndex].m_priority     = 0.0f;
		m_cachedEntries[cacheIndex].m_linkedEntry  = -1;
#if RSG_PC
		m_cachedEntries[cacheIndex].m_gpucounter = 0;
#endif
	}
}


void CParaboloidShadow::FetchCacheMap(int cacheIndex, int destIndex, int facetCount, bool isSpotLight, bool waitForCopy)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP();
	int srcIndex = GetCacheEntryRTIndex(cacheIndex);
	int dstIndex = GetCacheEntryRTIndex(destIndex);
	grcRenderTarget * srcTarget = GetCacheEntryRT(cacheIndex, isSpotLight);
	grcRenderTarget * dstTarget = GetCacheEntryRT(destIndex, isSpotLight);

	if (AssertVerify(srcTarget!=NULL && dstTarget!=NULL))
	{
#if RSG_ORBIS
		const sce::Gnm::Texture *const srcTexture = srcTarget->GetTexturePtr();
		const sce::Gnm::Texture *const dstTexture = dstTarget->GetTexturePtr();

		for (int facet = 0; facet< facetCount; facet++)
		{
			uint64_t srcOffset, dstOffset;
			uint64_t srcSize, dstSize;
			sce::GpuAddress::computeTextureSurfaceOffsetAndSize(&srcOffset, &srcSize, srcTexture, 0, srcIndex*(isSpotLight?1:6) + facet);
			sce::GpuAddress::computeTextureSurfaceOffsetAndSize(&dstOffset, &dstSize, dstTexture, 0, dstIndex*(isSpotLight?1:6) + facet);
			Assert(srcSize==dstSize);

			void * dst = ((u8*)dstTexture->getBaseAddress()) + dstOffset;
			void * src = ((u8*)srcTexture->getBaseAddress()) + srcOffset;

			GRCDEVICE.ShaderMemcpy(dst, src, srcSize);
		}

		if (waitForCopy)
		{
			GRCDEVICE.GpuWaitOnPreviousWrites();
			FlushStaticCacheCopy();
		}

#elif RSG_DURANGO && CACHE_USE_MOVE_ENGINE
		ID3D11Texture2D* poSrc = (ID3D11Texture2D*)(srcTarget->GetTexturePtr());
		ID3D11Texture2D* poDst = (ID3D11Texture2D*)(dstTarget->GetTexturePtr());
		Assert(poSrc != NULL && poDst != NULL);	

		if (waitForCopy) // use this version when waiting, because the fence is not reliable from sub threads.
		{
			for (int facet = 0; facet< facetCount; facet++)
			{
				// can't use grcRenderTargetDurango::Copy(), because it does not support src and dst array index being different
				u32 srcSubResource = D3D11CalcSubresource(0, srcIndex*(isSpotLight?1:6) + facet, 1);
				u32 dstSubResource = D3D11CalcSubresource(0, dstIndex*(isSpotLight?1:6) + facet, 1);
				g_grcCurrentContext->CopySubresourceRegion(poDst, dstSubResource, 0, 0, 0, poSrc, srcSubResource, NULL);
			}
		}
		else
		{
			ID3D11DmaEngineContextX * dmaContext = s_useDMA1 ? GRCDEVICE.GetDmaEngine1() : GRCDEVICE.GetDmaEngine2();
			const UINT64 fence = g_grcCurrentContext->InsertFence();
			dmaContext->InsertWaitOnFence(0, fence);

			for (int facet = 0; facet< facetCount; facet++)
			{
				u32 srcSubResource = D3D11CalcSubresource(0, srcIndex*(isSpotLight?1:6) + facet, 1);
				u32 dstSubResource = D3D11CalcSubresource(0, dstIndex*(isSpotLight?1:6) + facet, 1);
				dmaContext->CopySubresourceRegion(poDst, dstSubResource, 0, 0, 0, poSrc, srcSubResource, NULL,0);
			}
		}

#elif RSG_DURANGO
		// for depth targets planes 0 is Htile and 1 is depth. these are conviently saved for us in grcRenderTargetDurango::CreateSurface() 
		u64* srcPlaneOffsets = smart_cast<grcRenderTargetDurango*>(srcTarget)->GetPlaneOffsets();
		u64* srcPlaneSizes   = smart_cast<grcRenderTargetDurango*>(srcTarget)->GetPlaneSizes();
		u64* dstPlaneOffsets = smart_cast<grcRenderTargetDurango*>(dstTarget)->GetPlaneOffsets();
		u64* dstPlaneSizes   = smart_cast<grcRenderTargetDurango*>(dstTarget)->GetPlaneSizes();

		int srcArraySize = srcTarget->GetArraySize();
		int dstArraySize = dstTarget->GetArraySize();

		char *const srcBase = (char*)(smart_cast<grcRenderTargetDurango*>(srcTarget)->GetAllocationStart());
		char *const dstBase = (char*)(smart_cast<grcRenderTargetDurango*>(dstTarget)->GetAllocationStart());
	
		char *src;
		char *dst;

		if (srcPlaneSizes[0] != 0 && dstPlaneSizes[0] != 0)
		{
			src = srcBase + srcPlaneOffsets[0] + srcIndex*(isSpotLight?1:6)*(srcPlaneSizes[0]/srcArraySize);
			dst = dstBase + dstPlaneOffsets[0] + dstIndex*(isSpotLight?1:6)*(dstPlaneSizes[0]/dstArraySize);;

			GRCDEVICE.ShaderMemcpy(dst, src, facetCount*(srcPlaneSizes[0]/srcArraySize));
		}

		u64 facetSize = dstPlaneSizes[1]/dstArraySize;
		u64 arrayEntrySize = (isSpotLight?1:6)*dstPlaneSizes[1]/dstArraySize;
		src = srcBase + srcPlaneOffsets[1] + arrayEntrySize * srcIndex;
	    dst = dstBase + dstPlaneOffsets[1] + arrayEntrySize * dstIndex;

		GRCDEVICE.ShaderMemcpy(dst, src, facetCount*facetSize);

		if (waitForCopy)
		{
			GRCDEVICE.GpuWaitOnPreviousWrites();
			FlushStaticCacheCopy();
		}
#else
		if (m_useShaderCopy)// Some cards with a D3D feature level less than 10_1 do not support CopySubresourceRegion on depth buffers, so we use a shader copy
		{
			m_ShadowSmartBlitShader->SetVar(m_SmartBlitTextureId, srcTarget);
			m_ShadowSmartBlitShader->SetVar(m_SmartBlitCubeMapTextureId, srcTarget);

			for (int facet = 0; facet< facetCount; facet++)
			{
				grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, dstTarget, dstIndex*(isSpotLight?1:6) + facet);
				PUSH_DEFAULT_SCREEN();

				m_ShadowSmartBlitShader->SetVar(m_cubeMapFaceId, float(facet));
				grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull,grcStateBlock::DSS_ForceDepth,grcStateBlock::BS_Default_WriteMaskNone);

				if (m_ShadowSmartBlitShader->BeginDraw(grmShader::RMC_DRAW,true,m_copyTechniqueId))
				{
					m_ShadowSmartBlitShader->Bind((isSpotLight)?0:1);
					grcDrawSingleQuadf(-1.0f, -1.0f, 1.0f, 1.0f, float(srcIndex), 0.0f, 1.0f, 1.0f, 0.0f, CRGBA_White());
					m_ShadowSmartBlitShader->UnBind();
					m_ShadowSmartBlitShader->EndDraw();
				}
				POP_DEFAULT_SCREEN();

				grcTextureFactory::GetInstance().UnlockRenderTarget(0,NULL);
			}		
		}	
		else 
		{
			UNREFERENCED_PARAMETER(waitForCopy);
			ID3D11Texture2D* poSrc = (ID3D11Texture2D*)(srcTarget->GetTexturePtr());
			ID3D11Texture2D* poDst = (ID3D11Texture2D*)(dstTarget->GetTexturePtr());
			Assert(poSrc != NULL && poDst != NULL);	

			for (int facet = 0; facet< facetCount; facet++)
			{
				// can't use grcTextureDX11::Copy(), because it does not support src and dst array index being different
				u32 srcSubResource = D3D11CalcSubresource(0, srcIndex*(isSpotLight?1:6) + facet, 1);
				u32 dstSubResource = D3D11CalcSubresource(0, dstIndex*(isSpotLight?1:6) + facet, 1);
				g_grcCurrentContext->CopySubresourceRegion(poDst, dstSubResource, 0, 0, 0, poSrc, srcSubResource, NULL);
			}
		}
#endif	
	}
}



#if __BANK
static bool                  gs_bankParaboloidShadowInfo_Inited = false;
static CParaboloidShadowInfo gs_bankParaboloidShadowInfo;
static void UpdateHemiCutoff()
{
	s_HemisphereShadowCutoff = tanf(s_HemisphereShadowCutoffInAngles*DtoR);
}


void CParaboloidShadow::BankAddWidgets(bkBank& bk)
{
	gs_bankParaboloidShadowInfo.Init();
	gs_bankParaboloidShadowInfo_Inited = true;

	bk.PushGroup("Paraboloid Shadows", false);
	{		
		bk.AddToggle("Force Regen Cache",         &m_debugRegenCache);
		bk.AddToggle("Force ReCalc Interior",	  &s_debugForceInteriorCheck);
		bk.AddToggle("Show Active Entries",       &m_debugShowActiveEntries);
		bk.AddToggle("Show Static Cache Entries", &m_debugShowStaticEntries);
		bk.AddToggle("Skip forward shadows",	  &s_SkipForwardShadow);
		bk.AddToggle("Clear Cache",				  &s_DebugClearCache);
		bk.AddSlider("Dump Light info",			  &s_DumpShadowLights, -1, 32, 1,NullCB,"dump number of frames of light info (-1 is continuous)");
		bk.AddToggle("Dump Cache Info",			  &s_DumpCacheInfo);
		bk.AddToggle("Save Shadow Id",			  &s_SaveIsolatedLightShadowTrackingIndex,NullCB,"Saves the isolated debug light's Shadow Tracking index, then only processes that light's shadow (even it's no longer isolated)");
#if RSG_PC
		bk.AddToggle("Use Shader Copy",           &m_useShaderCopy);
#else
		bk.AddToggle("Enable Async Copy",	      &s_AsyncStaticCacheCopy);
#endif
		bk.AddSlider("Cache Index Start",         &m_debugCacheIndexStart,    0, MAX_CACHED_PARABOLOID_SHADOWS - 1, 1);
		bk.AddSlider("Cache Index Count",         &m_debugCacheIndexCount,    0, 8, 1);

		bk.AddSlider("Display Size",              &m_debugDisplaySize, 32, 512, 1);
		
		bk.AddSlider("Show Cache Info Alpha",     &m_debugShowShadowBufferCacheInfoAlpha, 0.0f, 1.0f, 1.0f/64.0f);
		bk.AddSlider("Display X",				  &m_debugDisplayBoundsX0, 0, 1000, 1);
		bk.AddSlider("Display Y",				  &m_debugDisplayBoundsY0, 0, 1000, 1);
	
		bk.AddToggle("Debug Breakpoint",		  &s_BreakpointDebugCutsceneCameraCuts);
		bk.PushGroup("Debug Vars", false);
		bk.AddToggle("Debug bool 1",			  &g_ParaboloidShadowDebugBool1);
		bk.AddToggle("Debug bool 2",			  &g_ParaboloidShadowDebugBool2);
		bk.AddSlider("Debug int1",				  &g_ParaboloidShadowDebugInt1,-1024,1024,1);
		bk.AddSlider("Debug int2",				  &g_ParaboloidShadowDebugInt2,-1024,1024,1);
		bk.PopGroup();

		
		bk.AddSlider("Show Screen Space",		  &m_debugDisplayScreenArea, -1, MAX_ACTIVE_PARABOLOID_SHADOWS, 1);
		bk.AddSlider("Screen Space Facet",		  &m_debugDisplayFacetScreenArea, 0, 5, 1);
		bk.AddSlider("Screen Space Facet Scale",  &m_debugScaleScreenAreaDisplay, 0.1f, 1.0f, 0.1f);
		bk.AddSlider("Hemisphere Cutoff Angle",	  &s_HemisphereShadowCutoffInAngles, 1.0f, 89.0f, 0.01f,CFA(UpdateHemiCutoff),"Angle (in degrees) at which spot light casts a Hemispheric shadows");
		bk.AddSlider("Hires Cache Threshold",	  &s_MinHiresThreshold, 0.0f, 4.0f, 0.05f);
		bk.AddSlider("Hires Flag Priority Boost", &s_HiresFlagPriorityBoost, 1.0f, 4.0f, 0.1f);
		bk.AddSlider("NonVisble Light Priority Scale", &s_NonVisiblePriorityScale, 0.0f, 1.0f, 0.01f);
		
		bk.AddSlider("DepthBias",				  &s_DepthBias, -1.0f, 1.0f, 0.0001f);
		bk.AddSlider("SlopeDepthBias",		      &s_SlopeDepthBias, -1.0f, 8.0f, 0.01f);
		bk.AddToggle("Flip Cull Direction",		  &s_FlipCull);
		bk.AddToggle("Disable Cutscene Camera Cut Test",  &s_SkipJumpCutTest);
		bk.AddToggle("Enable ParaboloidShadowInfo update", &s_DebugEnableParaboloidShadowInfoUpdate);

		bk.AddToggle("Render Cull Shape",		  &m_debugParaboloidShadowRenderCullShape);
		bk.AddToggle("Use Tight Cull Shape For Spot Light Shadows", &m_debugParaboloidShadowTightSpotCullShapeEnable);
		bk.AddSlider("Additional Radius % for Spot Light Cull Shape", &m_debugParaboloidShadowTightSpotCullShapeRadiusDelta, 0.0f, 1.0f, 0.001f);
		bk.AddSlider("Position Delta for Spot Light Cull Shape", &m_debugParaboloidShadowTightSpotCullShapePositionDelta, 0.0f, 100.0f, 0.001f);		
		bk.AddSlider("Additional % Angle for Spot Light Cull Shape", &m_debugParaboloidShadowTightSpotCullShapeAngleDelta, 0.0f, 1.0f, 0.001f);

		gs_bankParaboloidShadowInfo.BankAddWidgets(bk);
	}
	bk.PopGroup();
}

void CParaboloidShadow::BankUpdate()
{
	if (s_DebugEnableParaboloidShadowInfoUpdate && gs_bankParaboloidShadowInfo_Inited)
	{
		m_activeEntries[0].m_info.BankUpdate(gs_bankParaboloidShadowInfo);
	}
}

void CParaboloidShadow::ShadowBlit(
	grcRenderTarget* dst,
	const grcTexture * src,
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
	grcEffectTechnique technique
	)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	Assert(dst);
	Assert(src);

	const float dx1 = 2.0f*x1/dst->GetWidth () - 1.0f;
	const float dy1 = 2.0f*y1/dst->GetHeight() - 1.0f;
	const float dx2 = 2.0f*x2/dst->GetWidth () - 1.0f;
	const float dy2 = 2.0f*y2/dst->GetHeight() - 1.0f;

	const float offx = __PPU ? 0.0f : 0.5f/src->GetWidth(); // TODO -- get rid of this retarded half-pixel offset crap
	const float offy = __PPU ? 0.0f : 0.5f/src->GetHeight();

	m_ShadowSmartBlitShader->SetVar(m_SmartBlitTextureId, src);
	m_ShadowSmartBlitShader->SetVar(m_SmartBlitCubeMapTextureId, src);
	m_ShadowSmartBlitShader->SetVar(m_cubeMapFaceId, float(face));

	const float sx1 = u1/src->GetWidth ();
	const float sy1 = v1/src->GetHeight();
	const float sx2 = u2/src->GetWidth ();
	const float sy2 = v2/src->GetHeight();

	m_ShadowSmartBlitShader->TWODBlit(
		+dx1,
		-dy1,
		+dx2,
		-dy2,
		z,
		sx1 + offx,
		sy1 + offy,
		sx2 + offx,
		sy2 + offy,
		CRGBA_White(),
		technique
		);
}

void DebugLine(Vec3V_ConstRef ptA, Vec3V_ConstRef ptB, const Color32 & color)
{
	const Vec3V screenScale(ScalarV(0.5f*GRCDEVICE.GetWidth()),ScalarV(-0.5f*GRCDEVICE.GetHeight()),ScalarV(V_ZERO)); 
	const Vec3V screenOffset(ScalarV(0.5f*GRCDEVICE.GetWidth()),ScalarV(0.5f*GRCDEVICE.GetHeight()),ScalarV(V_ZERO));
	grcDrawLine(VEC3V_TO_VECTOR3(ptA*screenScale+screenOffset), VEC3V_TO_VECTOR3(ptB*screenScale+screenOffset), color);
}


void CParaboloidShadow::BankDebugDraw()
{
	if (gs_bankParaboloidShadowInfo_Inited)
	{	  
		if ( m_debugDisplayScreenArea>=0 || m_debugShowStaticEntries || m_debugShowActiveEntries)
		{
			grcStateBlock::SetStates(grcStateBlock::RS_Default,grcStateBlock::DSS_IgnoreDepth,grcStateBlock::BS_Default);
			
			const int buf = GetRenderBufferIndex();
			const CParaboloidShadowActiveEntry* activeList = s_paraboloidShadowMapActive[buf];
			const CParaboloidShadowCachedEntry* cacheList = s_paraboloidShadowMapCache[buf];

			if (m_debugShowStaticEntries || m_debugShowActiveEntries)
			{
				float x = (float)(m_debugDisplayBoundsX0);
				float y = (float)(m_debugDisplayBoundsY0);

				int rowCount = (m_debugShowActiveEntries && m_debugShowStaticEntries) ?2:1;
				int rowHeight = (3*m_debugDisplaySize/2 + m_debugDisplaySpacing);

				if (y+rowHeight*rowCount>(GRCDEVICE.GetHeight() - m_debugDisplayBoundsY1)) // if we're on the bottom, move up so we fit
				{
					y = (float)((GRCDEVICE.GetHeight() - m_debugDisplayBoundsY1) - rowHeight*rowCount);
				}

				int start1  = m_debugCacheIndexStart;
				int end1 = m_debugCacheIndexStart;
				if (m_debugShowStaticEntries)
					end1 = MAX_STATIC_CACHED_PARABOLOID_SHADOWS-1;
				
				int start2 = MAX_CACHED_PARABOLOID_SHADOWS;
				int end2 = MAX_CACHED_PARABOLOID_SHADOWS;

				if (m_debugShowActiveEntries) // lets do the two sections the cache and the active...
				{
					start2 = MAX_STATIC_CACHED_PARABOLOID_SHADOWS;
					end2 = MAX_STATIC_CACHED_PARABOLOID_SHADOWS + 8; // there are just 8, always draw them if active display is on, so the start and count can be used the move though the static cache
				}
				int start = Min(start1,start2);
				int end = Max(end1,end2);

				int ySize = m_debugDisplaySize;
				int drawnCount = 0;

				for (int i = start; i < end; i++)
				{
					if (i>=end1 && i<start2)
						continue;
					
					if (i >= 0 && i < MAX_CACHED_PARABOLOID_SHADOWS) // just to make sure
					{
						bool bValidEntry;
						
						if(i<=end1)
							bValidEntry = (cacheList[i].m_lightIndex != -1) && (cacheList[i].m_trackingID != 0);
						else
							bValidEntry = (activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_trackingID != 0);

						if (!bValidEntry)
						{
							continue;
						}

						if (x + (float)(m_debugDisplaySize) > (float)(GRCDEVICE.GetWidth() - m_debugDisplayBoundsX1) || (end1!=start1 && i==start2 && x != (float)(m_debugDisplayBoundsX0)))
						{
							x  = (float)(m_debugDisplayBoundsX0);
							y += (float)(3*m_debugDisplaySize/2 + m_debugDisplaySpacing);
						}

						if (y + (float)(ySize) > (float)(GRCDEVICE.GetHeight() - m_debugDisplayBoundsY1))
						{
							break; // can't fit any more
						}
						
						float notActive = false;
						int cacheIndex = i;

						if(i<=end1)	
						{
							notActive = cacheList[i].m_trackingID==0;

							if (++drawnCount>m_debugCacheIndexCount)  // we'll stop after n actual cache entries, many are not active
								continue;
						}	
						else if (i>=start2)
						{
							cacheIndex = activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_activeCacheIndex;
							notActive = activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_flags==SHADOW_FLAG_DONT_RENDER || cacheIndex <0;
							
							// if we're static only, just show that, shadowing a stale dynamic target on the ps3 is distracting
							if( (activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_flags&SHADOW_FLAG_DYNAMIC) == 0)
								cacheIndex = activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_staticCacheIndex;
						}

						if (m_debugShowShadowBufferCacheInfoAlpha > 0.0f)
						{
							float x0 = x + 2.0f;
							float y0 = y + 4.0f;

							const int alpha = (int)Clamp<float>(m_debugShowShadowBufferCacheInfoAlpha*255.0f + 0.5f, 0.0f, 255.0f);
							CRGBA colourGreen = CRGBA_Green((u8)alpha);
							CRGBA colourWhite = CRGBA_White((u8)alpha);
							float xscale = Min(1.0f,m_debugDisplaySize/200.0f); 
							BankFloat yscale = 1.0f;
							
							char text[2048] = "";
							if(i<=end1)				
							{
								sprintf(text, "CACHEINDEX= %d [%d]", i, GetCacheEntryRTIndex(i)		);	 grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourGreen, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								sprintf(text, "PRIORITY  = %.2f", cacheList[i].m_priority			);	 grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourWhite, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								sprintf(text, "AGE USED  = %d",	  cacheList[i].m_ageSinceUsed		);	 grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourWhite, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								sprintf(text, "AGE UPDATE= %d",	  cacheList[i].m_ageSinceUpdated	);	 grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourWhite, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								sprintf(text, "TRACKINGID= 0x%" SIZETFMT "X",cacheList[i].m_trackingID); grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourWhite, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								sprintf(text, "STATUS    = %d",	cacheList[i].m_status				);	 grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourWhite, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								sprintf(text, "LIGHTINDEX= %d",	cacheList[i].m_lightIndex			);	 grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourWhite, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								sprintf(text, "VISCOUNT  = %d",	cacheList[i].m_staticVisibleCount	);	 grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourWhite, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								sprintf(text, "LINKED    = %d",	cacheList[i].m_linkedEntry			);	 grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourWhite, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								sprintf(text, "CHECKSUM  = 0x%08X",cacheList[i].m_staticCheckSum	);	 grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourWhite, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
							 
								if (s_DumpCacheInfo) 
								{
									Displayf("TANFOV    = %f",cacheList[i].m_info.m_tanFOV);
									Displayf("Type      = %d",cacheList[i].m_info.m_type);
									Displayf("");
								}
							}
							else if (i>=start2)
							{
								sprintf(text, "ACTIVEINDEX= %d",	i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS                          );			grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourGreen, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								sprintf(text, "TRACKING ID= 0x%" SIZETFMT "X",activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_trackingID );grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourWhite, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								sprintf(text, "ACTIVECACHE= %d [%d]",	 activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_activeCacheIndex,m_cachedEntries[activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_activeCacheIndex].m_renderTargetIndex);	grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourWhite, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								if (activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_staticCacheIndex>=0)
								{
									sprintf(text, "STATICCACHE= %d [%d]",	 activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_staticCacheIndex,m_cachedEntries[activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_staticCacheIndex].m_renderTargetIndex);	grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourWhite, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								}
								sprintf(text, "FLAGS      = 0x%03X", activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_flags	    );			grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourWhite, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								if (activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_activeCacheIndex>=0)
								{
									sprintf(text, "PRIORITY   = %.2f", cacheList[activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_activeCacheIndex].m_priority);			grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourWhite, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								}
								sprintf(text, "ACTIVE RES = %s", IsCacheEntryHighRes( activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_activeCacheIndex)?"HIGH":"LOW" );	grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourWhite, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								if (activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_staticCacheIndex>=0)
								{
										sprintf(text, "STATIC RES = %s", IsCacheEntryHighRes( activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_staticCacheIndex)?"HIGH":"LOW" );grcDebugDraw::Text(Vector2(x0,y0), DD_ePCS_Pixels, colourWhite, text,true,xscale,yscale); y0 += 10.0f; if (s_DumpCacheInfo) Displayf("%s",text);
								}
								if (s_DumpCacheInfo) 
								{
									const Vector3 & pos = activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_pos;
									const Vector3 & dir = activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_dir;
														
									Displayf("POS       = %f,%f,%f", pos.x, pos.y, pos.z);
									Displayf("DIR       = %f,%f,%f", dir.x, dir.y, dir.z);
									Displayf("TANFOV    = %f",activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_info.m_tanFOV);
									Displayf("Type      = %d",activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_info.m_type);
									Displayf("");
								}
							}
						}
						
						int width = m_debugDisplaySize/2;
						int height = m_debugDisplaySize/2;
						const eLightShadowType type = (i<=end1) ? cacheList[i].m_info.m_type : activeList[i-MAX_STATIC_CACHED_PARABOLOID_SHADOWS].m_info.m_type;
						int count = (type==SHADOW_TYPE_HEMISPHERE)?5:6;
						float y0 = y;

						if (type==SHADOW_TYPE_SPOT)
						{
							width  *= 2;
							height *= 3;
							count = 1;
						}
						else if (type==SHADOW_TYPE_HEMISPHERE)
						{
							count = 5;
						}
					
						const grcTexture * src = notActive ? grcTexture::NoneBlack : GetCacheEntryRT(cacheIndex, type==SHADOW_TYPE_SPOT);
						const int arrayIndex = notActive ? 0 : GetCacheEntryRTIndex(cacheIndex);
						if (src==NULL)
							src = grcTexture::NoneBlack;

						for (int side=0;side<count;side++)
						{
							float x1 = x + (width+1)*(side%2);
							float y1 = y0 + (height+1)*(side/2);
							ShadowBlit( grcTextureFactory::GetInstance().GetBackBuffer(false),
										src,
										(notActive)?0:side,
										x1, y1,
										(x1+width), (y1+height),
										float(arrayIndex),
										0.0f, 0.0f,
										1.0f*src->GetWidth(), 1.0f*src->GetHeight(),
										(type==SHADOW_TYPE_SPOT)?m_copyDebugTechniqueId:m_copyCubeMapDebugTechniqueId);

						
						}

						x += (float)(m_debugDisplaySize + m_debugDisplaySpacing + 2);
					}
				}
			}

			if (m_debugDisplayScreenArea>=0 && (activeList[m_debugDisplayScreenArea].m_flags&SHADOW_FLAG_ENABLED))
			{				
				PUSH_DEFAULT_SCREEN();
				grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull,grcStateBlock::DSS_IgnoreDepth,grcStateBlock::BS_Default);
				
				grcWorldIdentity();

				if (activeList[m_debugDisplayScreenArea].m_info.m_type==SHADOW_TYPE_SPOT)
				{
					for (int i=0;i<8;i++)
					{
						DebugLine(s_DebugScreenVolume[0],   s_DebugScreenVolume[i+1], Color32(0,255,0,255));
						DebugLine(s_DebugScreenVolume[i+1], s_DebugScreenVolume[((i+1)%8)+1], Color32(0,255,0,255));
					}
				}
				else
				{
					for (int i=0;i<4;i++)
					{
						DebugLine(s_DebugScreenVolume[0],   s_DebugScreenVolume[i+1], Color32(255,0,255,255));
						DebugLine(s_DebugScreenVolume[i+1], s_DebugScreenVolume[((i+1)%4) + 1], Color32(255,0,255,255));
						DebugLine(s_DebugScreenVolume[i+1], s_DebugScreenVolume[i + 5], Color32(255,0,255,255));
						DebugLine(s_DebugScreenVolume[i+5], s_DebugScreenVolume[((i+1)%4) + 5], Color32(255,0,255,255));
					}
				}

				POP_DEFAULT_SCREEN();
			}
		}
		s_DumpCacheInfo = false;
	}
}

#endif // __BANK
