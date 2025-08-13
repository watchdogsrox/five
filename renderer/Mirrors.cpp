////////////////////////////////////////////////////////////////////////////////////
// Title	:	Mirrors.cpp
// Author	:	Obbe
// Started	:	14/6/05
//			:	Things having to do with mirrors go here.
//				This file works closely with multirender.cpp
////////////////////////////////////////////////////////////////////////////////////

// Rage headers
#include "vectormath/classes.h"
#include "grcore/stateblock.h"
#include "grcore/stateblock_internal.h"
#include "grmodel/shadergroup.h"
#include "system/param.h"
#include "system/nelem.h"
#if __XENON
#include "grcore/texturexenon.h"
#include "system/xtl.h"
#define DBG 0
#include <xgraphics.h>
#undef DBG
#endif // __XENON
#if __D3D11
#include "grcore/rendertarget_d3d11.h"
#include "grcore/texturefactory_d3d11.h"
#endif
#if RSG_ORBIS
#include "grcore/rendertarget_gnm.h"
#endif

// Framework headers
#include "fwdebug/debugdraw.h"
#include "fwdrawlist/drawlistmgr.h"
#include "fwmaths/vector.h"
#include "fwscene/scan/Scan.h"
#include "fwscene/scan/ScanDebug.h" // [MIRROR FLIP]
#include "fwscene/world/InteriorSceneGraphNode.h"
#include "fwsys/timer.h"

// Suite
#include "rmptfx/ptxmanager.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h" // [MIRROR FLIP]
#include "camera/viewports/ViewportManager.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "debug/Debug.h"
#include "debug/TiledScreenCapture.h"
#include "game/wind.h"
#include "modelinfo/MloModelInfo.h"
#include "renderer/Deferred/GBuffer.h"
#include "renderer/Mirrors.h"
#include "renderer/Renderer.h"
#include "renderer/rendertargets.h"
#include "renderer/renderthread.h"
#include "renderer/sprite2d.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/Water.h"
#include "scene/debug/PostScanDebug.h"
#include "scene/portals/PortalTracker.h"
#include "scene/Scene.h"

#define MIRROR_RT_NAME "MIRROR_RT"
#define MIRROR_RT_NAME_RESOLVED "MIRROR_RT_RESOLVED"
#define MIRROR_DT_NAME "MIRROR_DT"
#define MIRROR_DT_NAME_RO "MIRROR_DT_READ_ONLY"
#define MIRROR_DT_NAME_RESOLVED "MIRROR_DT_RESOLVED"
#define MIRROR_DT_NAME_COPY "MIRROR_DT_COPY"

#if __D3D11 || RSG_ORBIS
XPARAM(highColorPrecision);
#endif // __D3D11 || RSG_ORBIS

CRenderPhaseScanned*    CMirrors::ms_pRenderPhaseNew = NULL;
CPortalFlags            CMirrors::ms_mirrorPortalFlags = 0;

u32                     CMirrors::ms_scanResultsFrameIndex = 0;
bool                    CMirrors::ms_scanResultsMirrorVisible = false;
bool                    CMirrors::ms_scanResultsMirrorVisibleInProximityOnly = false;
bool                    CMirrors::ms_scanResultsMirrorVisibleWaterSurfaceInProximity = false;
bool                    CMirrors::ms_scanResultsMirrorFrustumCullDisabled = false;
fwRoomSceneGraphNode*   CMirrors::ms_scanResultsMirrorRoomSceneNode = NULL;
int                     CMirrors::ms_scanResultsMirrorPortalIndex = -1;
Vec3V                   CMirrors::ms_scanResultsMirrorCorners[];

Vec3V                   CMirrors::ms_mirrorCorners[];
Vec4V                   CMirrors::ms_mirrorPlane;
Vec4V                   CMirrors::ms_mirrorBounds = Vec4V(0.5f, 0.5f, -0.5f, -0.5f);

grcRenderTarget*        CMirrors::ms_pRenderTarget;
grcRenderTarget*		CMirrors::ms_pRenderTargetResolved;
grcRenderTarget*        CMirrors::ms_pDepthTarget;
grcRenderTarget*        CMirrors::ms_pWaterRenderTarget;
#if RSG_PC
grcRenderTarget*        CMirrors::ms_pWaterMSAARenderTarget;
#endif
#if __XENON
grcRenderTarget*		CMirrors::ms_pWaterRenderTargetAlias;
#endif //__XENON
grcRenderTarget*        CMirrors::ms_pWaterDepthTarget;
#if __D3D11
grcRenderTarget*        CMirrors::ms_pDepthTarget_CopyOrReadOnly = NULL;
grcRenderTarget*        CMirrors::ms_pWaterDepthTarget_CopyOrReadOnly;
grcRenderTarget*		CMirrors::ms_pDepthTargetResolved;
#endif // __D3D11

Vec4V                   CMirrors::ms_mirrorParams[2][MIRROR_PARAM_COUNT];

#if MIRROR_DEV
grmShader*              CMirrors::ms_mirrorShader;
grcEffectTechnique      CMirrors::ms_mirrorTechnique;
grcEffectVar            CMirrors::ms_mirrorReflectionTexture_ID;
grcEffectVar            CMirrors::ms_mirrorParams_ID;
#if MIRROR_USE_STENCIL_MASK
grcEffectVar            CMirrors::ms_mirrorDepthBuffer_ID;
grcEffectVar            CMirrors::ms_mirrorProjectionParams_ID;
#endif // MIRROR_USE_STENCIL_MASK
#if MIRROR_USE_CRACK_TEXTURE
grcTexture*             CMirrors::ms_mirrorCrackTexture = NULL;
grcEffectVar            CMirrors::ms_mirrorCrackTexture_ID;
grcEffectVar            CMirrors::ms_mirrorCrackParams_ID;
#endif // MIRROR_USE_CRACK_TEXTURE
#endif // MIRROR_DEV

#if RSG_PC
bool					CMirrors::ms_Initialized = false;
#endif

#if MIRROR_DEV
bool                    CMirrors::ms_debugMirrorShowInfo                                  = false;
char                    CMirrors::ms_debugMirrorShowInfoMessage[]                         = "";
bool                    CMirrors::ms_debugMirrorUseWaterReflectionTechniqueAlways         = false;
bool                    CMirrors::ms_debugMirrorUseWaterReflectionTechniqueNever          = false;
bool                    CMirrors::ms_debugMirrorShouldAlwaysForceUpAlpha                  = false;
bool                    CMirrors::ms_debugMirrorAllowAlphaCullInPostScan                  = true;
bool                    CMirrors::ms_debugMirrorAllowFading                               = true;
float                   CMirrors::ms_debugMirrorScanExteriorDistance                      = MIRROR_DEFAULT_EXTERIOR_DISTANCE;
bool                    CMirrors::ms_debugMirrorScanCurrentRoom                           = true;
bool                    CMirrors::ms_debugMirrorScanCurrentRoomVisibleInFrustum           = true;
bool                    CMirrors::ms_debugMirrorTraceSelectedEntityOnCameraFlip           = false;
bool                    CMirrors::ms_debugMirrorTraceScanVisibilityDumpOneFrame           = false;
bool                    CMirrors::ms_debugMirrorDrawDebugShader                           = false;
bool                    CMirrors::ms_debugMirrorDrawDebugShaderShowRT                     = false;
float                   CMirrors::ms_debugMirrorDrawDebugShaderShowRTOffsetX              = 0.05f;
float                   CMirrors::ms_debugMirrorDrawDebugShaderShowRTOffsetY              = 0.05f;
float                   CMirrors::ms_debugMirrorDrawDebugShaderShowRTSize                 = 1.0f;
float                   CMirrors::ms_debugMirrorScale                                     = 1.0f;
float                   CMirrors::ms_debugMirrorOffset                                    = 0.0f;
Vec3V                   CMirrors::ms_debugMirrorOffsetV                                   = Vec3V(V_ZERO);
Color32                 CMirrors::ms_debugMirrorColour                                    = Color32(255,255,255,255);
float                   CMirrors::ms_debugMirrorBrightness                                = 1.0f;
#if MIRROR_USE_STENCIL_MASK
bool                    CMirrors::ms_debugMirrorStencilMaskEnabled                        = false;
bool                    CMirrors::ms_debugMirrorStencilMaskScissor                        = true;
#endif // MIRROR_USE_STENCIL_MASK
#if MIRROR_USE_CRACK_TEXTURE
bool                    CMirrors::ms_debugMirrorCrackEnabled                              = false;
float                   CMirrors::ms_debugMirrorCrackAmount                               = 1.0f;
float                   CMirrors::ms_debugMirrorCrackRefraction                           = 8.0f;
float                   CMirrors::ms_debugMirrorCrackLuminance                            = 0.25f;
#endif // MIRROR_USE_CRACK_TEXTURE
bool                    CMirrors::ms_debugMirrorFloor                                     = false;
bool                    CMirrors::ms_debugMirrorFloorScanFromGBufStartingNodeOn           = false;
bool                    CMirrors::ms_debugMirrorFloorScanFromGBufStartingNodeOff          = false;
bool                    CMirrors::ms_debugMirrorFloorUseLODs                              = true;
bool                    CMirrors::ms_debugMirrorWireframe                                 = false;
bool                    CMirrors::ms_debugMirrorLocalLightIntensityOverrideEnabled        = false;
float                   CMirrors::ms_debugMirrorLocalLightIntensityOverride               = 1.0f;
bool                    CMirrors::ms_debugMirrorRenderDistantLights                       = false;
bool                    CMirrors::ms_debugMirrorRenderCoronas                             = true;
bool                    CMirrors::ms_debugMirrorRenderSkyAlways                           = false;
int                     CMirrors::ms_debugMirrorRenderSkyMode                             = MIRROR_RENDER_SKY_MODE_DEFAULT;
bool                    CMirrors::ms_debugMirrorDrawFilter_RENDER_OPAQUE                  = true;
bool                    CMirrors::ms_debugMirrorDrawFilter_RENDER_ALPHA                   = true;
bool                    CMirrors::ms_debugMirrorDrawFilter_RENDER_FADING                  = true;
bool                    CMirrors::ms_debugMirrorDrawFilter_RENDER_TREES                   = false;
bool                    CMirrors::ms_debugMirrorDrawFilter_RENDER_DECALS_CUTOUTS          = true;
bool                    CMirrors::ms_debugMirrorUseAlphaTestForCutout                     = true;
float                   CMirrors::ms_debugMirrorUseAlphaTestForCutoutAlphaRef             = 128.f/255.f;
bool                    CMirrors::ms_debugMirrorForceDirectionalLightingOnForExteriorView = true; // TODO -- can we just enable directional light but no shadows?
bool                    CMirrors::ms_debugMirrorForceDirectionalLightingOn                = false;
bool                    CMirrors::ms_debugMirrorForceDirectionalLightingOff               = false;
bool                    CMirrors::ms_debugMirrorForcePortalTraversalOn                    = false;
bool                    CMirrors::ms_debugMirrorForcePortalTraversalOff                   = false;
bool                    CMirrors::ms_debugMirrorForceExteriorViewOn                       = false;
bool                    CMirrors::ms_debugMirrorForceExteriorViewOff                      = false;
bool                    CMirrors::ms_debugMirrorExteriorViewRenderLODs                    = false;
bool                    CMirrors::ms_debugMirrorClipAndStorePortals                       = false; // TODO -- re-enable this once BS#992094 is resolved
bool                    CMirrors::ms_debugMirrorCullingPlanesEnabled                      = true;
u32                     CMirrors::ms_debugMirrorCullingPlaneFlags                         = 0xff;
//#if __PS3
bool                    CMirrors::ms_debugMirrorUseObliqueProjection                      = __PS3 ? true : false;
float                   CMirrors::ms_debugMirrorUseObliqueProjectionScale                 = MIRROR_DEFAULT_OBLIQUE_PROJ_SCALE;
//#endif // __PS3
#if !__PS3
bool                    CMirrors::ms_debugMirrorUseHWClipPlane                            = true;
#endif // !__PS3
bool                    CMirrors::ms_debugMirrorInteriorWaterOffsetEnabled                = true;
float                   CMirrors::ms_debugMirrorInteriorWaterOffset                       = MIRROR_DEFAULT_INTERIOR_WATER_OFFSET;
bool                    CMirrors::ms_debugMirrorInteriorWaterSlopeEnabled                 = true;
float                   CMirrors::ms_debugMirrorInteriorWaterSlope                        = MIRROR_DEFAULT_INTERIOR_WATER_SLOPE;
bool                    CMirrors::ms_debugMirrorRestrictedProjection                      = true;
bool                    CMirrors::ms_debugMirrorMaxPixelDensityEnabled                    = true;
float                   CMirrors::ms_debugMirrorMaxPixelDensity                           = MIRROR_DEFAULT_MAX_PIXEL_DENSITY;
float                   CMirrors::ms_debugMirrorMaxPixelDensityBorder                     = MIRROR_DEFAULT_MAX_PIXEL_DENSITY_BORDER;
int                     CMirrors::ms_debugMirrorClipMode                                  = CMirrors::MIRROR_CLIP_MODE_VIEWSPACE;
bool                    CMirrors::ms_debugMirrorShowBounds                                = false;
bool                    CMirrors::ms_debugMirrorShowBoundsRT                              = false;
u32                     CMirrors::ms_debugMirrorPrivateFlags                              = 0;
#endif // MIRROR_DEV

#if MIRROR_DEV && __PS3
PARAM(mirdebugnoheap_ps3,"mirdebugnoheap_ps3"); // allocate mirror reflection target separately, so it can be viewed from rag
#endif // MIRROR_DEV && __PS3

#if MIRROR_DEV && __XENON
PARAM(mirdebugnoheap_360,"mirdebugnoheap_360"); // allocate mirror reflection target separately, so it can be viewed from rag
PARAM(mirrorusegbuf1_360,"mirrorusegbuf1_360"); // allocate mirror reflection target from gbuffer1 (as it used to be), instead of gbuffer23
#endif // MIRROR_DEV && __XENON

#if MIRROR_USE_CRACK_TEXTURE
PARAM(mirdebugcrack,"mirdebugcrack"); // load special crack texture
#endif // MIRROR_USE_CRACK_TEXTURE

void CMirrors::Init()
{
	CreateRenderTargets();

#if MIRROR_DEV
	ASSET.PushFolder("common:/shaders");
	{
		ms_mirrorShader = grmShaderFactory::GetInstance().Create();
		ms_mirrorShader->Load("debug_mirror");
	}
	ASSET.PopFolder();

	ms_mirrorTechnique            = ms_mirrorShader->LookupTechnique("dbg_mirror");
	ms_mirrorReflectionTexture_ID = ms_mirrorShader->LookupVar("mirrorReflectionTexture");
	ms_mirrorParams_ID            = ms_mirrorShader->LookupVar("mirrorParams");
#if MIRROR_USE_STENCIL_MASK
	ms_mirrorDepthBuffer_ID       = ms_mirrorShader->LookupVar("mirrorDepthBuffer");
	ms_mirrorProjectionParams_ID  = ms_mirrorShader->LookupVar("mirrorProjectionParams");
#endif // MIRROR_USE_STENCIL_MASK
#if MIRROR_USE_CRACK_TEXTURE
	if (PARAM_mirdebugcrack.Get())
	{
		ms_mirrorCrackTexture    = grcTextureFactory::CreateTexture("X:/gta5/art/Debug/graphics/mirror_crack2.dds");
		ms_mirrorCrackTexture_ID = ms_mirrorShader->LookupVar("mirrorCrackTexture");
		ms_mirrorCrackParams_ID  = ms_mirrorShader->LookupVar("mirrorCrackParams");
	}
#endif // MIRROR_USE_CRACK_TEXTURE
#endif // MIRROR_DEV

	for (unsigned i=0; i<2; ++i)
	{
		ms_mirrorParams[i][MIRROR_PARAM_BOUNDS] = Vec4V(0.5f, 0.5f, -0.5f, -0.5f);
		ms_mirrorParams[i][MIRROR_PARAM_COLOUR] = Vec4V(V_ONE);
	}
}

void CMirrors::Shutdown()
{
}


void CMirrors::CreateRenderTargets()
{
#if RSG_PC
	ms_Initialized = true;
#endif

	grcTextureFactory::CreateParams params;

	params.UseFloat  = true;
	params.HasParent = true;
	params.Parent    = NULL;

#if __XENON

	const int mirrorResX = 512;
	const int mirrorResY = 256;
	const int tbpp       = 32;

	params.IsResolvable = false;
	params.UseHierZ     = true;
	params.HiZBase      = static_cast<grcRenderTargetXenon*>(grcTextureFactory::GetInstance().GetBackBufferDepth(false))->GetHiZSize();
	params.Multisample  = 2;
//	params.Parent       = grcTextureFactory::GetInstance().GetBackBufferDepth(false);
	params.Parent       = grcTextureFactory::GetInstance().GetBackBuffer(false);

	ms_pDepthTarget = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, MIRROR_DT_NAME, grcrtDepthBuffer, mirrorResX, mirrorResY, 32, &params);

	params.Parent       = ms_pDepthTarget;
	params.Format       = grctfA2B10G10R10;
	params.IsResolvable = true;

#if MIRROR_DEV
	if (PARAM_mirdebugnoheap_360.Get())
	{
		ms_pRenderTarget = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, MIRROR_RT_NAME, grcrtPermanent, mirrorResX, mirrorResY, tbpp, &params);
	}
	else
#endif // MIRROR_DEV
	{
		/*
		BS#722008 status -
		crashes only when using kMirrorHeap, i can't repro the crash when using kReflectionHeap or kPostFxGeneralHeap, or when unpooled
		to test using other heaps, i made the target 64x64 and increased the pool size by 32kb
		i've tried running with USE_D3D_DEBUG_LIBS and looking at the pix dump, but this has provided no additional info
		maybe the error only happens for one frame, hence the pix analysis did not show anything wrong?
		also, dave's assert only checks for fragment samplers and only checks for the base addr being equal ..
		.. what if the memory overlaps but the base is not equal?
		i also tried forcing all samplers to NULL before i set the mirror RT, this did not prevent the crash
		i've tried disabling the sampler shadow (always setting the texture), this also did not help
		using a white texture instead of the mirror RT when rendering the mirror does prevent the crash
		*/

#if MIRROR_DEV
		if (PARAM_mirrorusegbuf1_360.Get())
		{
			ms_pRenderTarget = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GBUFFER1, MIRROR_RT_NAME, grcrtPermanent, mirrorResX, mirrorResY, tbpp, &params, kGBuffer1MirrorHeap);
		}
		else
#endif // MIRROR_DEV
		{
			ms_pRenderTarget = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GENERAL, MIRROR_RT_NAME, grcrtPermanent, mirrorResX, mirrorResY, tbpp, &params, kReflectionHeap);
		}

		ms_pRenderTarget->AllocateMemoryFromPool();

		params.Parent       = CRenderTargets::GetBackBuffer();
		params.Format       = grctfA8R8G8B8;
		ms_pWaterRenderTargetAlias = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GENERAL, "Water Reflection Alias", grcrtPermanent, 320, 180, 32, &params, kWaterReflectionAliasHeap);
		ms_pWaterRenderTargetAlias->AllocateMemoryFromPool();
	}

#elif __PS3

	const int mirrorResX = 512;
	const int mirrorResY = 256;
	const int tbpp       = 32;

	params.IsSwizzled    = false;
	params.UseHierZ      = true;
	params.InTiledMemory = true;
	params.Format        = grctfA8R8G8B8;
	params.UseFloat      = false;
	params.IsSRGB        = true;

#if MIRROR_DEV
	int resX = mirrorResX;
	if (PARAM_mirdebugnoheap_ps3.Get(resX))
	{
		ms_pRenderTarget = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, MIRROR_RT_NAME, grcrtPermanent, resX, resX == 512 ? mirrorResY : (resX*720)/1280, tbpp, &params);
	}
	else
#endif // MIRROR_DEV
	{
		ms_pRenderTarget = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P2048_TILED_LOCAL_COMPMODE_C32_2X1, MIRROR_RT_NAME, grcrtPermanent, mirrorResX, mirrorResY, tbpp, &params, 0);
		ms_pRenderTarget->AllocateMemoryFromPool();
	}

	params.UseFloat = true;
	params.IsSRGB   = false;
	params.Format   = grctfR32F;

#if MIRROR_DEV
	if (PARAM_mirdebugnoheap_ps3.Get())
	{
		ms_pDepthTarget = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, MIRROR_DT_NAME, grcrtDepthBuffer, mirrorResX, mirrorResY, 32, &params);
	}
	else
#endif // MIRROR_DEV
	{
		//This dummy RT setups the the zcull region to be a size that fits the water refraction depth(640x360), instead of (512x256)
		grcRenderTarget* zcullDummy	= CRenderTargets::CreateRenderTarget(
																			PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_DISABLED, "ZCull Dummy", grcrtDepthBuffer, 
																			VideoResManager::GetSceneWidth()/2, VideoResManager::GetSceneHeight()/2, 32, &params, 17);
		zcullDummy->AllocateMemoryFromPool();

		ms_pDepthTarget = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_DISABLED, MIRROR_DT_NAME, grcrtDepthBuffer, mirrorResX, mirrorResY, 32, &params, 4);
		ms_pDepthTarget->AllocateMemoryFromPool();
	}

#else // not __PS3

	const int mirrorResX = GetResolutionX();
	const int mirrorResY = GetResolutionY();
	params.Multisample = GetMSAA();
#if DEVICE_EQAA
	params.EnableHighQualityResolve = true;
	params.ForceFragmentMask = true;
#endif
#if RSG_ORBIS
	params.EnableFastClear = true;
#endif

	int tbpp       = 64;
	params.Format = grctfA16B16G16R16F;

#if __D3D11 || RSG_ORBIS
	if (!PARAM_highColorPrecision.Get())
	{
		params.Format = grctfR11G11B10F;
		tbpp = 32;
	}
#endif // __D3D11 || RSG_ORBIS

#if RSG_PC
	params.StereoRTMode = grcDevice::MONO;
#endif
	
	ms_pRenderTarget = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, MIRROR_RT_NAME, grcrtPermanent, mirrorResX, mirrorResY, tbpp, &params, 0 WIN32PC_ONLY(, ms_pRenderTarget));

	if (params.Multisample)
	{
		params.Multisample = grcDevice::MSAA_None;
		ms_pRenderTargetResolved = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, MIRROR_RT_NAME_RESOLVED, grcrtPermanent, mirrorResX, mirrorResY, tbpp, &params, 0 WIN32PC_ONLY(, ms_pRenderTargetResolved));
		params.Multisample = GetMSAA();
	}else
	{
		if (ms_pRenderTargetResolved)
		{
			delete ms_pRenderTargetResolved;
		}
		ms_pRenderTargetResolved = NULL;
	}

#if !__D3D11
	ms_pDepthTarget  = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, MIRROR_DT_NAME, grcrtDepthBuffer, mirrorResX, mirrorResY, 32);
#else // not !__D3D11
	grcTextureFactoryDX11& textureFactory11 = static_cast<grcTextureFactoryDX11&>(grcTextureFactoryDX11::GetInstance()); 

	params.Format = grctfD32FS8;
	int depthBpp      = 40;

	ms_pDepthTarget  = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, MIRROR_DT_NAME, grcrtDepthBuffer, mirrorResX, mirrorResY, depthBpp, &params, 0 WIN32PC_ONLY(, ms_pDepthTarget));

	if (GRCDEVICE.IsReadOnlyDepthAllowed())
		ms_pDepthTarget_CopyOrReadOnly = textureFactory11.CreateRenderTarget(MIRROR_DT_NAME_RO, ms_pDepthTarget->GetTexturePtr(), params.Format, DepthStencilReadOnly WIN32PC_ONLY(, ms_pDepthTarget_CopyOrReadOnly));
	else
		ms_pDepthTarget_CopyOrReadOnly  = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, MIRROR_DT_NAME_COPY, grcrtDepthBuffer, mirrorResX, mirrorResY, depthBpp, &params, 0 WIN32PC_ONLY(,ms_pDepthTarget_CopyOrReadOnly));

	if(params.Multisample)
	{
		params.Multisample = grcDevice::MSAA_None;
		ms_pDepthTargetResolved = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, MIRROR_DT_NAME_RESOLVED, grcrtDepthBuffer, mirrorResX, mirrorResY, depthBpp, &params, 0 WIN32PC_ONLY(, ms_pDepthTargetResolved));
		params.Multisample = GetMSAA();
	}
	else
	{
		if (ms_pDepthTargetResolved)
		{
			delete ms_pDepthTargetResolved;
		}
		ms_pDepthTargetResolved = NULL;
	}

#endif // not !__D3D11

#endif // not __PS3
}


#if RSG_PC
static bool s_requirePowerOf2 = false;

int CMirrors::GetResolutionSetting(int width, int height) {
	//	#define MIRROR_REFLECTION_RES_BASELINE 128
	//	return MIRROR_REFLECTION_RES_BASELINE << CSettingsManager::GetInstance().GetSettings().m_graphics.m_ReflectionQuality;
	int reflectionQuality = CSettingsManager::GetInstance().GetSettings().m_graphics.m_ReflectionQuality;
	int reflectionSize = Max(32, (width+height) >> (4 - Min(3, reflectionQuality)));
	
	if (s_requirePowerOf2)
	{
		u32 closestPow2Size = 32;
		while (reflectionSize > 3*closestPow2Size/2)
			closestPow2Size <<= 1;
		return closestPow2Size;
	}else
	{
		return reflectionSize & ~0xF;
	}
}
#endif //RSG_PC

#if DEVICE_MSAA

#define MIRROR_RES_X_ORBISDURANGO (RSG_ORBIS ? 1536 : 1024)
#define MIRROR_RES_Y_ORBISDURANGO (RSG_ORBIS ? 768  : 512)

int CMirrors::GetResolutionX() 
{
#if RSG_PC
	return GetResolutionSetting(GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight());
#else
	return MIRROR_RES_X_ORBISDURANGO;
#endif
}

int CMirrors::GetResolutionY()
{
#if RSG_PC
	return GetResolutionSetting(GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight()) / 2;
#else
	return MIRROR_RES_Y_ORBISDURANGO;
#endif
}

grcDevice::MSAAMode CMirrors::GetMSAA()
{
#if RSG_PC
	return GRCDEVICE.GetMSAA()>=4 ? GRCDEVICE.GetMSAA() / 2 : grcDevice::MSAA_None;
#else
	return GRCDEVICE.GetMSAA();
#endif
}

void CMirrors::ResolveMirrorRenderTarget()
{
	if (ms_pRenderTargetResolved)
	{
		GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
#if DEVICE_EQAA
		//eqaa::ResolveArea area(); // if only we could narrow down the used portion of this texture
#endif
		static_cast<grcRenderTargetMSAA*>(ms_pRenderTarget)->Resolve(ms_pRenderTargetResolved);
	}
}

#if __D3D11
void CMirrors::ResolveMirrorDepthTarget()
{
	if ((GetMSAA() > 1) && ms_pDepthTargetResolved)
	{
		CShaderLib::SetMSAAParams( GetMSAA(),
# if DEVICE_EQAA
			GRCDEVICE.IsEQAA() );
# else
			false );
# endif // DEVICE_EQAA
		CRenderTargets::ResolveSourceDepth(depthResolve_Closest, ms_pDepthTarget, ms_pDepthTargetResolved);

		CShaderLib::SetMSAAParams( GRCDEVICE.GetMSAA(),
# if DEVICE_EQAA
			GRCDEVICE.IsEQAA() );
# else
			false );
# endif // DEVICE_EQAA
	}

}
#endif //__D3D11
#endif //DEVICE_MSAA

#if RSG_PC
void CMirrors::DeviceLost()
{

}

void CMirrors::DeviceReset()
{
	if (ms_Initialized)
	{
		CreateRenderTargets();
	}
}
#endif
#if MIRROR_DEV
static void MirrorShowReflectionRT_button()
{
	if (!CRenderTargets::IsShowingRenderTarget(MIRROR_RT_NAME))
	{
		CRenderTargets::ShowRenderTarget(MIRROR_RT_NAME);
		CRenderTargets::ShowRenderTargetBrightness(8.0f); // reflection map looks kinda dark, make it brighter
		CRenderTargets::ShowRenderTargetNativeRes(true, true, true);
		CRenderTargets::ShowRenderTargetFullscreen(false);
		CRenderTargets::ShowRenderTargetInfo(false); // we're probably displaying model counts too, so don't clutter up the screen
	}
	else
	{
		CRenderTargets::ShowRenderTargetReset();
	}
}

static void MirrorShowReflectionDT_button()
{
	if (!CRenderTargets::IsShowingRenderTarget(MIRROR_DT_NAME))
	{
		CRenderTargets::ShowRenderTarget(MIRROR_DT_NAME);
		CRenderTargets::ShowRenderTargetBrightness(1.0f);
		CRenderTargets::ShowRenderTargetNativeRes(true, true, true);
		CRenderTargets::ShowRenderTargetFullscreen(false);
		CRenderTargets::ShowRenderTargetInfo(false); // we're probably displaying model counts too, so don't clutter up the screen
	}
	else
	{
		CRenderTargets::ShowRenderTargetReset();
	}
}

template <int i> static void MirrorShowRenderingStats_button()
{
	if (CPostScanDebug::GetCountModelsRendered())
	{
		CPostScanDebug::SetCountModelsRendered(false);
		gDrawListMgr->SetStatsHighlight(false);
	}
	else
	{
		CPostScanDebug::SetCountModelsRendered(true, DL_RENDERPHASE_MIRROR_REFLECTION);
		gDrawListMgr->SetStatsHighlight(true, "DL_MIRROR_REFLECTION", i == 0, i == 1, false);
		grcDebugDraw::SetDisplayDebugText(true);
	}
}
#endif // MIRROR_DEV

#if __BANK
void CMirrors::AddWidgets(bkGroup& MIRROR_DEV_ONLY(bk))
{
#if MIRROR_DEV
	const char* renderSkyModeStrings[] =
	{
		"MIRROR_RENDER_SKY_MODE_NONE",
		"MIRROR_RENDER_SKY_MODE_FIRST",
		"MIRROR_RENDER_SKY_MODE_LAST",
		"MIRROR_RENDER_SKY_MODE_BEFORE_ALPHA",
	};
	CompileTimeAssert(NELEM(renderSkyModeStrings) == MIRROR_RENDER_SKY_MODE_COUNT);

	const char* clipModeStrings[] =
	{
		"MIRROR_CLIP_MODE_NONE",
		"MIRROR_CLIP_MODE_SCREENSPACE",
		"MIRROR_CLIP_MODE_VIEWSPACE",
	};
	CompileTimeAssert(NELEM(clipModeStrings) == MIRROR_CLIP_MODE_COUNT);

	bkGroup& advanced = *bk.AddGroup("Advanced", false);

	class CameraFlip_button { public: static void func()
	{
		camInterface::GetGameplayDirector().DebugFlipRelativeOrbitOrientation();
		CMirrors::ms_debugMirrorTraceScanVisibilityDumpOneFrame = true;
	}};

	class CameraSpin_button { public: static void func()
	{
		camInterface::GetGameplayDirector().DebugSpinRelativeOrbitOrientation();
		CMirrors::ms_debugMirrorTraceScanVisibilityDumpOneFrame = true;
	}};

	advanced.AddButton("Flip Camera", CameraFlip_button::func);
	advanced.AddButton("Spin Camera", CameraSpin_button::func);

	advanced.AddToggle("Mirror Show Info"                                      , &ms_debugMirrorShowInfo);
	advanced.AddToggle("Mirror Use Water Reflection Technique Always"          , &ms_debugMirrorUseWaterReflectionTechniqueAlways);
	advanced.AddToggle("Mirror Use Water Reflection Technique Never"           , &ms_debugMirrorUseWaterReflectionTechniqueNever);
	advanced.AddToggle("Mirror Should Always Force Up Alpha"                   , &ms_debugMirrorShouldAlwaysForceUpAlpha);
	advanced.AddToggle("Mirror Allow Alpha Cull In Post Scan"                  , &ms_debugMirrorAllowAlphaCullInPostScan);
	advanced.AddToggle("Mirror Allow Fading"                                   , &ms_debugMirrorAllowFading);

	bkGroup& flagsGroup = *advanced.AddGroup("Mirror Flags (READ ONLY)", false);
	{
		flagsGroup.AddToggle("O - one way portal"                              , &ms_mirrorPortalFlags.m_flags, INTINFO_PORTAL_ONE_WAY);
		flagsGroup.AddToggle("L - link portal"                                 , &ms_mirrorPortalFlags.m_flags, INTINFO_PORTAL_LINK);
		flagsGroup.AddToggle("M - mirror portal"                               , &ms_mirrorPortalFlags.m_flags, INTINFO_PORTAL_MIRROR);
		flagsGroup.AddToggle("D - mirror can see directional light"            , &ms_mirrorPortalFlags.m_flags, INTINFO_PORTAL_MIRROR_CAN_SEE_DIRECTIONAL_LIGHT);
		flagsGroup.AddToggle("E - mirror can see exterior view"                , &ms_mirrorPortalFlags.m_flags, INTINFO_PORTAL_MIRROR_CAN_SEE_EXTERIOR_VIEW);
		flagsGroup.AddToggle("P - mirror using portal traversal"               , &ms_mirrorPortalFlags.m_flags, INTINFO_PORTAL_MIRROR_USING_PORTAL_TRAVERSAL);
		flagsGroup.AddToggle("F - mirror floor"                                , &ms_mirrorPortalFlags.m_flags, INTINFO_PORTAL_MIRROR_FLOOR);
		flagsGroup.AddToggle("W - water surface"                               , &ms_mirrorPortalFlags.m_flags, INTINFO_PORTAL_WATER_SURFACE);
		flagsGroup.AddToggle("I - ignore modifier"                             , &ms_mirrorPortalFlags.m_flags, INTINFO_PORTAL_IGNORE_MODIFIER);
		flagsGroup.AddToggle("X - low LOD only"                                , &ms_mirrorPortalFlags.m_flags, INTINFO_PORTAL_LOWLODONLY);
		flagsGroup.AddToggle("C - allow closing"                               , &ms_mirrorPortalFlags.m_flags, INTINFO_PORTAL_ALLOWCLOSING);
	}

	advanced.AddSlider("Mirror Scan Exterior Distance"                         , &ms_debugMirrorScanExteriorDistance, 0.0f, 1000.0f, 1.0f/32.0f);
	advanced.AddToggle("Mirror Scan Current Room"                              , &ms_debugMirrorScanCurrentRoom);
	advanced.AddToggle("Mirror Scan Current Room Visible In Frustum"           , &ms_debugMirrorScanCurrentRoomVisibleInFrustum);
	advanced.AddToggle("Mirror Trace Selected Entity On Camera Flip"           , &ms_debugMirrorTraceSelectedEntityOnCameraFlip);
	advanced.AddToggle("Mirror Draw Debug Shader"                              , &ms_debugMirrorDrawDebugShader);
	advanced.AddToggle("Mirror Draw Debug Shader Show RT"                      , &ms_debugMirrorDrawDebugShaderShowRT);
	advanced.AddSlider(" - offset X"                                           , &ms_debugMirrorDrawDebugShaderShowRTOffsetX, 0.0f, 1.0f, 1.0f/32.0f);
	advanced.AddSlider(" - offset Y"                                           , &ms_debugMirrorDrawDebugShaderShowRTOffsetY, 0.0f, 1.0f, 1.0f/32.0f);
	advanced.AddSlider(" - size"                                               , &ms_debugMirrorDrawDebugShaderShowRTSize, 0.0f, 1.0f, 1.0f/32.0f);
	advanced.AddSlider("Mirror Scale"                                          , &ms_debugMirrorScale, 0.0f, 16.0f, 1.0f/32.0f);
	advanced.AddSlider("Mirror Offset"                                         , &ms_debugMirrorOffset, 0.0f, 1.0f, 1.0f/128.0f);
	advanced.AddVector("Mirror Offset V"                                       , &ms_debugMirrorOffsetV, -5.0f, 5.0f, 1.0f/32.0f);
	advanced.AddColor ("Mirror Colour"                                         , &ms_debugMirrorColour);
	advanced.AddSlider("Mirror Brightness"                                     , &ms_debugMirrorBrightness, 0.0f, 10.0f, 1.0f/32.0f);
#if MIRROR_USE_STENCIL_MASK
	advanced.AddSeparator();
	advanced.AddToggle("Mirror Stencil Mask Enabled"                           , &ms_debugMirrorStencilMaskEnabled);
	advanced.AddToggle("Mirror Stencil Mask Scissor"                           , &ms_debugMirrorStencilMaskScissor);
#endif // MIRROR_USE_STENCIL_MASK
#if MIRROR_USE_CRACK_TEXTURE
	if (PARAM_mirdebugcrack.Get())
	{
		advanced.AddSeparator();
		advanced.AddToggle("Mirror Crack Enabled"                              , &ms_debugMirrorCrackEnabled);
		advanced.AddSlider("Mirror Crack Amount"                               , &ms_debugMirrorCrackAmount, 0.0f, 1.0f, 1.0f/32.0f);
		advanced.AddSlider("Mirror Crack Refraction"                           , &ms_debugMirrorCrackRefraction, -20.0f, 20.0f, 1.0f/32.0f);
		advanced.AddSlider("Mirror Crack Luminance"                            , &ms_debugMirrorCrackLuminance, -1.0f, 1.0f, 1.0f/32.0f);
	}
#endif // MIRROR_USE_CRACK_TEXTURE

	advanced.AddSeparator();

	advanced.AddToggle("Mirror Floor"                                          , &ms_debugMirrorFloor);
	advanced.AddToggle("Mirror Floor Scan From GBuf Start Node On"             , &ms_debugMirrorFloorScanFromGBufStartingNodeOn);
	advanced.AddToggle("Mirror Floor Scan From GBuf Start Node Off"            , &ms_debugMirrorFloorScanFromGBufStartingNodeOff);
	advanced.AddToggle("Mirror Floor Use Drawable/Ped/Vehicle LODs"            , &ms_debugMirrorFloorUseLODs);

	advanced.AddSeparator();

	advanced.AddToggle("Mirror Wireframe"                                      , &ms_debugMirrorWireframe);
	advanced.AddToggle("Mirror Local Light Override Enabled"                   , &ms_debugMirrorLocalLightIntensityOverrideEnabled);
	advanced.AddSlider("Mirror Local Light Override"                           , &ms_debugMirrorLocalLightIntensityOverride, 0.0f, 16.0f, 1.0f/32.0f);
	advanced.AddToggle("Mirror Render Distant Lights"                          , &ms_debugMirrorRenderDistantLights);
	advanced.AddToggle("Mirror Render Coronas"                                 , &ms_debugMirrorRenderCoronas);
	advanced.AddToggle("Mirror Render Sky Always"                              , &ms_debugMirrorRenderSkyAlways);
	advanced.AddCombo ("Mirror Render Sky Mode"                                , &ms_debugMirrorRenderSkyMode, NELEM(renderSkyModeStrings), renderSkyModeStrings);
	advanced.AddToggle("Mirror Draw Filter - RENDER_OPAQUE"                    , &ms_debugMirrorDrawFilter_RENDER_OPAQUE);
	advanced.AddToggle("Mirror Draw Filter - RENDER_ALPHA"                     , &ms_debugMirrorDrawFilter_RENDER_ALPHA);
	advanced.AddToggle("Mirror Draw Filter - RENDER_FADING"                    , &ms_debugMirrorDrawFilter_RENDER_FADING);
	advanced.AddToggle("Mirror Draw Filter - RENDER_TREES"                     , &ms_debugMirrorDrawFilter_RENDER_TREES);
	advanced.AddToggle("Mirror Draw Filter - RENDER_DECALS_CUTOUTS"            , &ms_debugMirrorDrawFilter_RENDER_DECALS_CUTOUTS);
	advanced.AddToggle("Mirror Use Alpha Test For Cutout"                      , &ms_debugMirrorUseAlphaTestForCutout);
	advanced.AddSlider("Mirror Cutout Alpha Ref"                               , &ms_debugMirrorUseAlphaTestForCutoutAlphaRef, 0.f, 1.f, 0.01f);
	advanced.AddToggle("Mirror Force Directional Lighting On For Exterior View", &ms_debugMirrorForceDirectionalLightingOnForExteriorView);
	advanced.AddToggle("Mirror Force Directional Lighting On"                  , &ms_debugMirrorForceDirectionalLightingOn);
	advanced.AddToggle("Mirror Force Directional Lighting Off"                 , &ms_debugMirrorForceDirectionalLightingOff);
	advanced.AddToggle("Mirror Force Portal Traversal On"                      , &ms_debugMirrorForcePortalTraversalOn);
	advanced.AddToggle("Mirror Force Portal Traversal Off"                     , &ms_debugMirrorForcePortalTraversalOff);
	advanced.AddToggle("Mirror Force Exterior View On"                         , &ms_debugMirrorForceExteriorViewOn);
	advanced.AddToggle("Mirror Force Exterior View Off"                        , &ms_debugMirrorForceExteriorViewOff);
	advanced.AddToggle("Mirror Exterior View Render LODs"                      , &ms_debugMirrorExteriorViewRenderLODs);
	advanced.AddToggle("Mirror Clip And Store Portals"                         , &ms_debugMirrorClipAndStorePortals);
	advanced.AddToggle("Mirror Culling Planes Enabled"                         , &ms_debugMirrorCullingPlanesEnabled);
	advanced.AddToggle("Mirror Culling Plane - CLIP_PLANE_LEFT"                , &ms_debugMirrorCullingPlaneFlags, BIT(0));
	advanced.AddToggle("Mirror Culling Plane - CLIP_PLANE_RIGHT"               , &ms_debugMirrorCullingPlaneFlags, BIT(1));
	advanced.AddToggle("Mirror Culling Plane - CLIP_PLANE_TOP"                 , &ms_debugMirrorCullingPlaneFlags, BIT(2));
	advanced.AddToggle("Mirror Culling Plane - CLIP_PLANE_BOTTOM"              , &ms_debugMirrorCullingPlaneFlags, BIT(3));
	advanced.AddToggle("Mirror Culling Plane - SILHOUETTE 0-1"                 , &ms_debugMirrorCullingPlaneFlags, BIT(4));
	advanced.AddToggle("Mirror Culling Plane - SILHOUETTE 1-2"                 , &ms_debugMirrorCullingPlaneFlags, BIT(5));
	advanced.AddToggle("Mirror Culling Plane - SILHOUETTE 2-3"                 , &ms_debugMirrorCullingPlaneFlags, BIT(6));
	advanced.AddToggle("Mirror Culling Plane - SILHOUETTE 3-0"                 , &ms_debugMirrorCullingPlaneFlags, BIT(7));
	advanced.AddToggle("Mirror Use Oblique Projection"                         , &ms_debugMirrorUseObliqueProjection);
	advanced.AddSlider("Mirror Use Oblique Projection Scale"                   , &ms_debugMirrorUseObliqueProjectionScale, 0.0f, 2.0f, 1.0f/64.0f);
#if !__PS3
	advanced.AddToggle("Mirror Use HW Clip Plane"                              , &ms_debugMirrorUseHWClipPlane);
#endif // !__PS3
	advanced.AddToggle("Mirror Interior Water Offset Enabled"                  , &ms_debugMirrorInteriorWaterOffsetEnabled);
	advanced.AddSlider("Mirror Interior Water Offset"                          , &ms_debugMirrorInteriorWaterOffset, -0.5f, 0.5f, 0.001f);
	advanced.AddToggle("Mirror Interior Water Slope Enabled"                   , &ms_debugMirrorInteriorWaterSlopeEnabled);
	advanced.AddSlider("Mirror Interior Water Slope"                           , &ms_debugMirrorInteriorWaterSlope, -0.1f, 0.1f, 0.001f);

	advanced.AddSeparator();

	advanced.AddToggle("Mirror Restricted Projection"                          , &ms_debugMirrorRestrictedProjection);
	advanced.AddToggle("Mirror Max Pixel Density Enabled"                      , &ms_debugMirrorMaxPixelDensityEnabled);
	advanced.AddSlider("Mirror Max Pixel Density"                              , &ms_debugMirrorMaxPixelDensity, 1.0f/8.0f, 4.0f, 1.0f/32.0f);
	advanced.AddSlider("Mirror Max Pixel Density Border"                       , &ms_debugMirrorMaxPixelDensityBorder, 0.0f, 16.0f, 1.0f/32.0f);
	advanced.AddCombo ("Mirror Clip Mode"                                      , &ms_debugMirrorClipMode, NELEM(clipModeStrings), clipModeStrings);
	advanced.AddToggle("Mirror Show Bounds"                                    , &ms_debugMirrorShowBounds);
	advanced.AddToggle("Mirror Show Bounds RT"                                 , &ms_debugMirrorShowBoundsRT);

	advanced.AddButton("Show MIRROR_RT"                                        , MirrorShowReflectionRT_button);
	advanced.AddButton("Show MIRROR_DT"                                        , MirrorShowReflectionDT_button);
	advanced.AddButton("Show Rendering Stats"                                  , MirrorShowRenderingStats_button<0>);
	advanced.AddButton("Show Context Stats"                                    , MirrorShowRenderingStats_button<1>);

	advanced.AddSeparator();

	for (int i = 0; i < 8; i++)
	{
		advanced.AddToggle(atVarString("Private Flags[%d]", i).c_str()         , &ms_debugMirrorPrivateFlags, BIT(i));
	}
#endif // MIRROR_DEV
}
#endif // __BANK

const grcViewport* CMirrors::GetMirrorViewport()
{
	if (ms_pRenderPhaseNew)
	{
		return &ms_pRenderPhaseNew->GetGrcViewport();
	}

	return NULL;
}

Vec4V_Out CMirrors::GetAdjustedClipPlaneForInteriorWater(Vec4V_In plane)
{
	const bool bNearInteriorWater = fwScan::GetScanResults().GetWaterSurfaceVisible();
	Vec4V temp = plane;

	if (bNearInteriorWater && !GetIsMirrorFrustumCullDisabled()) // TODO -- this doesn't work for planes which are not near zero (e.g. v_lab/Lab_poolroom)
	{
		if (MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorInteriorWaterOffsetEnabled, true))
		{
			const float offset = MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorInteriorWaterOffset, MIRROR_DEFAULT_INTERIOR_WATER_OFFSET);

			temp -= Vec4V(Vec3V(V_ZERO), ScalarV(offset));
		}

		if (!Water::IsCameraUnderwater() && MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorInteriorWaterSlopeEnabled, true))
		{
			const float slope = MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorInteriorWaterSlope, MIRROR_DEFAULT_INTERIOR_WATER_SLOPE);

			const Vec3V camPos = +gVpMan.GetCurrentGameGrcViewport()->GetCameraMtx().GetCol3();
			const Vec3V camDir = -gVpMan.GetCurrentGameGrcViewport()->GetCameraMtx().GetCol2();

			const Vec3V p = Vec3V(camPos.GetXY(), -temp.GetW());
			const Vec3V n = Normalize(Vec3V(V_Z_AXIS_WZERO) - camDir*ScalarV(slope));

			temp = BuildPlane(p, n);
		}
	}

	return temp;
}

#if __BANK
PARAM(checkmirrorpv,"");
#endif // __BANK

// this is actually called from CRenderPhaseWaterReflection::UpdateViewport, because of annoying dependencies ..
void CMirrors::UpdateViewportForScan()
{
	// i've disabled this assert for now, to get around BS#644071 (it only triggers when you save and load a game, but it seems tricky to fix)
	//Assert(ms_scanResultsFrameIndex < fwTimer::GetSystemFrameCount());

#if MIRROR_DEV
	if (ms_debugMirrorTraceScanVisibilityDumpOneFrame)
	{
		ms_debugMirrorTraceScanVisibilityDumpOneFrame = false;
		//CPostScanDebug::CountModelsRenderedDumpOneFrame();

		if (CMirrors::ms_debugMirrorTraceSelectedEntityOnCameraFlip)
		{
			if (g_scanDebugFlags & SCAN_DEBUG_PROCESS_VISIBILITY_ON_PPU)
			{
				g_scanDebugFlags |= SCAN_DEBUG_TRACE_SELECTED_ENTITY;
			}
		}
	}

	ms_debugMirrorShowInfoMessage[0] = '\0';
#endif // MIRROR_DEV

	ms_scanResultsFrameIndex = fwTimer::GetSystemFrameCount();
	ms_scanResultsMirrorVisible = false;
	ms_scanResultsMirrorVisibleInProximityOnly = false;
	ms_scanResultsMirrorVisibleWaterSurfaceInProximity = false;
	ms_scanResultsMirrorFrustumCullDisabled = false;
	ms_scanResultsMirrorRoomSceneNode = NULL;
	ms_scanResultsMirrorPortalIndex = -1;

	const CInteriorInst* pCameraInteriorInst = CPortalVisTracker::GetPrimaryInteriorInst();
	const CInteriorProxy* pCameraInteriorProxy = (pCameraInteriorInst && pCameraInteriorInst->m_bIsPopulated) ? pCameraInteriorInst->GetProxy() : NULL;

	bool bMirrorVisibleLastFrame = fwScan::GetScanResults().GetMirrorVisible();

	// [HACK GTAV] BS#2527755 - Force mirror reflection during the cutscene "low_fin_int"
	if (CutSceneManager::GetInstance() &&
		CutSceneManager::GetInstance()->IsRunning() &&
		CutSceneManager::GetInstance()->GetSceneHashString()->GetHash() == ATSTRINGHASH("low_fin_int", 0xAC546E91))
	{
		bMirrorVisibleLastFrame = fwScan::GetScanResults().GetMirrorInteriorLocation().IsValid() && fwScan::GetScanResults().GetMirrorInteriorLocation().IsAttachedToPortal();
	}

	if (bMirrorVisibleLastFrame) // mirror was visible in the previous frame, let's use this result and hope for the best ..
	{
		const fwInteriorLocation mirrorIntLoc = fwScan::GetScanResults().GetMirrorInteriorLocation();

		Assert(mirrorIntLoc.IsValid());
		Assert(mirrorIntLoc.IsAttachedToPortal());

		const CInteriorInst* pInteriorInst = CInteriorInst::GetInteriorForLocation(mirrorIntLoc);
		const CInteriorProxy* pInteriorProxy = (pInteriorInst && pInteriorInst->m_bIsPopulated) ? pInteriorInst->GetProxy() : NULL;

		if (pInteriorProxy && pInteriorProxy->GetCurrentState() >= CInteriorProxy::PS_FULL)
		{
			const fwPortalSceneGraphNode* pMirrorPortalNode = pInteriorInst->GetPortalSceneGraphNode(mirrorIntLoc.GetPortalIndex());
			Assert(pMirrorPortalNode);

			fwSceneGraphNode* pMirrorRoomNode = pMirrorPortalNode->GetNegativePortalEnd();
			Assert(pMirrorRoomNode && pMirrorRoomNode->IsTypeRoom());

			const bool bCameraIsInInterior = (CPortalVisTracker::GetPrimaryInteriorInst() != NULL);
			const float mirrorDistance = fwScan::GetScanResults().GetMirrorDistance();
			const bool bMirrorIsCloseEnough = (mirrorDistance <= MIRROR_DEV_SWITCH(ms_debugMirrorScanExteriorDistance, MIRROR_DEFAULT_EXTERIOR_DISTANCE));
			const u32 interiorNameHash = pInteriorInst->GetArchetype()->GetModelNameHash();

			// [HACK GTAV]
			const bool bIsMirrorIn247Supermarket = (interiorNameHash == ATSTRINGHASH("v_shop_247", 0x9B39307B));
			const bool bIsMirrorInLab = (interiorNameHash == ATSTRINGHASH("v_lab", 0xA6091244));
			const bool bIsMirrorInBarbers = (interiorNameHash == ATSTRINGHASH("v_barbers", 0x97158871));

			bool bAllowMirror = true;

			// [HACK GTAV] BS#1527996,1544858,1580701 - force mirror to not be visible if > 10 metres from v_shop_247 or v_barbers
			if ((bIsMirrorIn247Supermarket || bIsMirrorInBarbers) && mirrorDistance > 10.0f)
			{
				bAllowMirror = false;
			}

			// [HACK GTAV] BS#1560089,1560092 - disable these useless mirrors (too late to fix in art)
			if ((interiorNameHash == ATSTRINGHASH("v_31_newtunnel1", 0xB1CB2462)) ||
				(interiorNameHash == ATSTRINGHASH("v_31_tun_08", 0xCDF24C2) && !pMirrorPortalNode->GetIsWaterSurface()))
			{
				bAllowMirror = false;
			}

			// [HACK GTAV] BS#879061 - force mirror to not be visible if the camera is outside and the mirror is more than 20 metres away and water is visible
			if (bAllowMirror && (bCameraIsInInterior || (bMirrorIsCloseEnough && !fwScan::GetScanResults().GetWaterSurfaceVisible())))
			{
				ms_scanResultsMirrorVisible                = true;
				ms_scanResultsMirrorVisibleInProximityOnly = false;
				ms_scanResultsMirrorFrustumCullDisabled    = bIsMirrorInLab;
				ms_scanResultsMirrorRoomSceneNode          = static_cast<fwRoomSceneGraphNode*>(pMirrorRoomNode);
				ms_scanResultsMirrorPortalIndex            = mirrorIntLoc.GetPortalIndex();
				ms_scanResultsMirrorCorners[0]             = VECTOR3_TO_VEC3V(fwScan::GetScanResults().GetMirrorCorners().GetCorner(0));
				ms_scanResultsMirrorCorners[1]             = VECTOR3_TO_VEC3V(fwScan::GetScanResults().GetMirrorCorners().GetCorner(1));
				ms_scanResultsMirrorCorners[2]             = VECTOR3_TO_VEC3V(fwScan::GetScanResults().GetMirrorCorners().GetCorner(2));
				ms_scanResultsMirrorCorners[3]             = VECTOR3_TO_VEC3V(fwScan::GetScanResults().GetMirrorCorners().GetCorner(3));

				MIRROR_DEV_ONLY(formatf(ms_debugMirrorShowInfoMessage, "mirror visible:"));
			}
			else
			{
				MIRROR_DEV_ONLY(formatf(ms_debugMirrorShowInfoMessage, "mirror is too far away (%.2f) and water is visible (%d pixels), or disabled via some hack", mirrorDistance, Water::GetWaterPixelsRenderedPlanarReflective()));
			}
		}
		else
		{
			MIRROR_DEV_ONLY(formatf(ms_debugMirrorShowInfoMessage, "mirror not visible because room is not populated"));
		}
	}
	else MIRROR_DEV_ONLY(if (ms_debugMirrorScanCurrentRoom)) // .. mirror was not visible in the previous frame, let's check the room we're in
	{
		if (pCameraInteriorProxy && pCameraInteriorProxy->GetCurrentState() >= CInteriorProxy::PS_FULL)
		{
			const int roomIdx = CPortalVisTracker::GetPrimaryRoomIdx();

			if (roomIdx > 0 && roomIdx != INTLOC_INVALID_INDEX)
			{
				CMloModelInfo* pMloModelInfo = pCameraInteriorInst->GetMloModelInfo();

				if (pMloModelInfo && pMloModelInfo->GetIsStreamType() && (pMloModelInfo->GetRooms()[roomIdx].m_flags & ROOM_MIRROR_POTENTIALLY_VISIBLE) != 0)
				{
					MIRROR_DEV_ONLY(int numMirrorsInProximity = 0);

					// TODO -- find some way to avoid having to iterate over all portals in the MLO,
					// maybe we could store the mirror portal indices in the CMloModelInfo? i'm
					// fairly sure there would never be more than about 8 .. maybe a bit more if we
					// need to consider mirror floors too (which this proximity system doesn't really
					// handle properly at the moment).

					const grcViewport& gameVP = *gVpMan.GetUpdateGameGrcViewport();
					ScalarV distSqrMin(FLT_MAX); // closest mirror distance (squared)

					// [HACK GTAV]
					const bool bIsCameraInLab = (pCameraInteriorInst->GetArchetype()->GetModelNameHash() == ATSTRINGHASH("v_lab", 0xA6091244));

					for (int i = 0; i < pMloModelInfo->GetPortals().GetCount(); i++)
					{
						const CMloPortalDef& portal = pMloModelInfo->GetPortals()[i];

						if (portal.m_flags & INTINFO_PORTAL_MIRROR) 
						{
							if ((portal.m_flags & INTINFO_PORTAL_WATER_SURFACE) != 0 && !bIsCameraInLab)
							{
								continue; // don't process water surface portals here - this fixes BS#1248290 .. except when the camera is in v_lab, for BS#1463394
							}

							Assert(pMloModelInfo->GetRooms()[portal.m_roomFrom].m_flags & ROOM_MIRROR_POTENTIALLY_VISIBLE);

							const Mat34V matrix = pCameraInteriorInst->GetMatrix();
							const Vec3V corners[4] =
							{
								Transform(matrix, RCC_VEC3V(portal.m_corners[0])),
								Transform(matrix, RCC_VEC3V(portal.m_corners[1])),
								Transform(matrix, RCC_VEC3V(portal.m_corners[2])),
								Transform(matrix, RCC_VEC3V(portal.m_corners[3])),
							};

							bool bMirrorIsVisibleInProximity = true;

							const Vec3V bmin = Min(corners[0], corners[1], corners[2], corners[3]);
							const Vec3V bmax = Max(corners[0], corners[1], corners[2], corners[3]);

							MIRROR_DEV_ONLY(numMirrorsInProximity++);

							MIRROR_DEV_ONLY(if (ms_debugMirrorScanCurrentRoomVisibleInFrustum))
							{
								if (!grcViewport::IsAABBVisible(bmin.GetIntrin128(), bmax.GetIntrin128(), gameVP.GetFrustumLRTB()))
								{
									bMirrorIsVisibleInProximity = false;
								}
								else
								{
									Vec3V clipped[4 + 5];

									if (PolyClipToFrustumWithoutNearPlane(clipped, corners, 4, gameVP.GetCompositeMtx()) < 3)
									{
										bMirrorIsVisibleInProximity = false;
									}
								}
							}

							if (bMirrorIsVisibleInProximity)
							{
								const ScalarV distSqr = MagSquared((bmax + bmin)*ScalarV(V_HALF) - gameVP.GetCameraPosition());
								const Vec3V mirrorNormal = Cross(corners[0] - corners[1], corners[2] - corners[1]); // not normalised, we only need the sign

								if (IsLessThanAll(distSqr, distSqrMin) && IsGreaterThanAll(Dot(gameVP.GetCameraPosition() - corners[0], mirrorNormal), ScalarV(V_ZERO)))
								{
									distSqrMin = distSqr;

									ms_scanResultsMirrorVisible                = true;
									ms_scanResultsMirrorVisibleInProximityOnly = true;
									ms_scanResultsMirrorRoomSceneNode          = pCameraInteriorInst->GetRoomSceneGraphNode(portal.m_roomFrom);
									ms_scanResultsMirrorPortalIndex            = i; // is this the right portal index?
									ms_scanResultsMirrorCorners[0]             = corners[0];
									ms_scanResultsMirrorCorners[1]             = corners[1];
									ms_scanResultsMirrorCorners[2]             = corners[2];
									ms_scanResultsMirrorCorners[3]             = corners[3];

									// [HACK GTAV] BS#1463394 - the only mirror is v_lab is a water surface
									if (bIsCameraInLab)
									{
										if (AssertVerify(portal.m_flags & INTINFO_PORTAL_WATER_SURFACE))
										{
											ms_scanResultsMirrorVisibleWaterSurfaceInProximity = true;
											ms_scanResultsMirrorFrustumCullDisabled            = true;
										}
									}
								}
							}
						}
					}

					if (ms_scanResultsMirrorVisible)
					{
						MIRROR_DEV_ONLY(formatf(ms_debugMirrorShowInfoMessage, "mirror visible in proximity:"));
					}
					else
					{
						MIRROR_DEV_ONLY(formatf(ms_debugMirrorShowInfoMessage, "mirror not visible (%d mirror portals in proximity)", numMirrorsInProximity));
					}
				}
				else
				{
#if MIRROR_DEV
					if (pMloModelInfo == NULL)
					{
						formatf(ms_debugMirrorShowInfoMessage, "mirror not visible (MLO modelinfo is NULL)");
					}
					else if (!pMloModelInfo->GetIsStreamType())
					{
						formatf(ms_debugMirrorShowInfoMessage, "mirror not visible (MLO modelinfo is not stream type)");
					}
					else
					{
						formatf(ms_debugMirrorShowInfoMessage, "mirror not visible (no potentially visible mirror)");
					}
#endif // MIRROR_DEV
				}
			}
			else
			{
				MIRROR_DEV_ONLY(formatf(ms_debugMirrorShowInfoMessage, "mirror not visible (roomIdx is invalid)"));
			}
		}
		else
		{
			MIRROR_DEV_ONLY(formatf(ms_debugMirrorShowInfoMessage, "mirror not visible because room is not populated"));
		}
	}

#if __BANK
	if (PARAM_checkmirrorpv.Get() && ms_scanResultsMirrorVisible)
	{
		const CInteriorInst* pInteriorInst = CPortalVisTracker::GetPrimaryInteriorInst();
		const CInteriorProxy* pInteriorProxy = (pInteriorInst && pInteriorInst->m_bIsPopulated) ? pInteriorInst->GetProxy() : NULL;

		if (pInteriorProxy && pInteriorProxy->GetCurrentState() >= CInteriorProxy::PS_FULL)
		{
			const int roomIdx = CPortalVisTracker::GetPrimaryRoomIdx();

			if (roomIdx > 0 && roomIdx != INTLOC_INVALID_INDEX)
			{
				CMloModelInfo* pMloModelInfo = pInteriorInst->GetMloModelInfo();

				if (pMloModelInfo && pMloModelInfo->GetIsStreamType())
				{
					const CMloRoomDef& roomDef = pMloModelInfo->GetRooms()[roomIdx];

					if ((roomDef.m_flags & ROOM_MIRROR_POTENTIALLY_VISIBLE) == 0)
					{
						char msg[512] = "";
						formatf(msg, "%s/%s does not have \"Mirror Potentially Visible\" flag set, mirror might pop if camera spins around quickly", pInteriorInst->GetModelName(), roomDef.m_name.c_str());
						Assertf(0, "%s", msg);

						grcDebugDraw::AddDebugOutput(Color32(255,128,128,255), msg);
					}
				}
			}
		}
	}
#endif // __BANK
}

bool CMirrors::GetIsMirrorVisible(bool ASSERT_ONLY(bNoAssert))
{
#if __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		return false;
	}

	if (Water::m_bForceMirrorReflectionOff) // if we're forcing water planar reflection to NOT use mirror, we don't want the mirror to be visible at all (otherwise it will prevent water reflection entirely)
	{
		return false;
	}
#endif // __BANK

	Assert(bNoAssert || fwTimer::GetSystemFrameCount() == ms_scanResultsFrameIndex);
	return ms_scanResultsMirrorVisible;
}

bool CMirrors::GetIsMirrorVisibleWaterSurfaceInProximity()
{
	return ms_scanResultsMirrorVisibleWaterSurfaceInProximity;
}

bool CMirrors::GetIsMirrorUsingWaterReflectionTechnique(bool bIsUsingMirrorWaterSurface)
{
#if MIRROR_DEV
	if (ms_debugMirrorUseWaterReflectionTechniqueAlways)
	{
		return true;
	}
	else if (ms_debugMirrorUseWaterReflectionTechniqueNever)
	{
		return false;
	}
#endif // MIRROR_DEV

	if (bIsUsingMirrorWaterSurface)
	{
		const fwInteriorLocation mirrorIntLoc = fwScan::GetScanResults().GetMirrorInteriorLocation();

		if (mirrorIntLoc.IsValid())
		{
			const CInteriorInst* pInteriorInst = CInteriorInst::GetInteriorForLocation(mirrorIntLoc);

			if (pInteriorInst)
			{
				const u32 interiorNameHash = pInteriorInst->GetArchetype()->GetModelNameHash();

				// [HACK GTAV] -- use water reflection technique for interior water in vbca tunnel
				if (interiorNameHash == ATSTRINGHASH("vbca_tunnel1", 0xB7B6F801) ||
					interiorNameHash == ATSTRINGHASH("vbca_tunnel2", 0x667E558D) ||
					interiorNameHash == ATSTRINGHASH("vbca_tunnel3", 0x5A6EBD6E) ||
					interiorNameHash == ATSTRINGHASH("vbca_tunnel4", 0x1271AD75) ||
					interiorNameHash == ATSTRINGHASH("vbca_tunnel5", 0xFFBC080A))
				{
					return true;
				}
			}
		}
	}

	return false;
}

void CMirrors::SetShaderParams()
{
	const Vec4V params_bounds = ms_mirrorBounds;
	const Vec4V params_colour = MIRROR_DEV_SWITCH(ms_debugMirrorColour.GetRGBA()*Vec4V(Vec3V(ScalarV(ms_debugMirrorBrightness)), ScalarV(V_ONE)), Vec4V(V_ONE));

	const unsigned idx = gRenderThreadInterface.GetUpdateBuffer();
	ms_mirrorParams[idx][MIRROR_PARAM_BOUNDS] = params_bounds;
	ms_mirrorParams[idx][MIRROR_PARAM_COLOUR] = params_colour;
}

#if MIRROR_DEV
void CMirrors::RenderDebug(bool MIRROR_USE_STENCIL_MASK_ONLY(bStencilMask))
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	if (!ms_debugMirrorDrawDebugShader && !ms_debugMirrorDrawDebugShaderShowRT)
		return;
	if (ms_debugMirrorScale <= 0.0f)
		return;
#if MIRROR_USE_STENCIL_MASK
	if (CMirrors::ms_debugMirrorStencilMaskEnabled && !bStencilMask)
		return;
#endif // MIRROR_USE_STENCIL_MASK

#if MIRROR_USE_CRACK_TEXTURE
	bool bUseCrack = false;
#endif // MIRROR_USE_CRACK_TEXTURE

#if MIRROR_USE_STENCIL_MASK
	if (bStencilMask)
	{
		ms_mirrorShader->SetVar(ms_mirrorDepthBuffer_ID, PS3_SWITCH(GBuffer::GetDepthTargetPatched(), CRenderTargets::GetDepthBuffer()));
		ms_mirrorShader->SetVar(ms_mirrorProjectionParams_ID, ShaderUtil::CalculateProjectionParams(gVpMan.GetRenderGameGrcViewport()));
	}
	else
#endif // MIRROR_USE_STENCIL_MASK
	{
		ms_mirrorShader->SetVar(ms_mirrorReflectionTexture_ID, GetMirrorTexture());
		ms_mirrorShader->SetVar(ms_mirrorParams_ID, ms_mirrorParams[gRenderThreadInterface.GetRenderBuffer()], NELEM(ms_mirrorParams[0]));

#if MIRROR_USE_CRACK_TEXTURE
		if (ms_debugMirrorCrackEnabled && ms_debugMirrorCrackAmount != 0.0f && ms_mirrorCrackTexture)
		{
			ms_mirrorShader->SetVar(ms_mirrorCrackTexture_ID, ms_mirrorCrackTexture);
			ms_mirrorShader->SetVar(ms_mirrorCrackParams_ID, Vec4f(
				ms_debugMirrorCrackAmount*ms_debugMirrorCrackRefraction,
				ms_debugMirrorCrackAmount*ms_debugMirrorCrackLuminance,
				0.0f,
				0.0f
			));

			bUseCrack = true;
		}
#endif // MIRROR_USE_CRACK_TEXTURE
	}

	grcViewport::SetCurrentWorldIdentity();

	AssertVerify(ms_mirrorShader->BeginDraw(grmShader::RMC_DRAW, true, ms_mirrorTechnique));
	{
		const Vec3V* mirrorCorners = ms_mirrorCorners;

		const Color32 col = Color32(0);
		int pass = 0;

#if 0 && __XENON
		static grcBlendStateHandle bs_handle = grcStateBlock::BS_Invalid;

		if (bs_handle == grcStateBlock::BS_Invalid);
		{
			grcBlendStateDesc bs;
			grcStateBlock::GetBlendStateDesc(BS_Normal, bs);
			bs_handle = grcStateBlock::CreateBlendState(bs);
		}
#else
		const grcBlendStateHandle bs_handle = grcStateBlock::BS_Normal;
#endif

#if MIRROR_USE_STENCIL_MASK
		if (bStencilMask)
		{
			pass = 3;

			static grcDepthStencilStateHandle ds_handle = grcStateBlock::DSS_Invalid;
			static grcBlendStateHandle        bs_handle = grcStateBlock::BS_Invalid;

			if (ds_handle == grcStateBlock::DSS_Invalid)
			{
				grcDepthStencilStateDesc ds;
				grcBlendStateDesc        bs;

				ds.DepthEnable                  = true;
				ds.DepthWriteMask               = true;
				ds.DepthFunc                    = grcRSV::CMP_ALWAYS;
				ds.StencilEnable                = true;
				ds.StencilReadMask              = 96;
				ds.StencilWriteMask             = 96;
				ds.FrontFace.StencilFailOp      = grcRSV::STENCILOP_KEEP;
				ds.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
				ds.FrontFace.StencilPassOp      = grcRSV::STENCILOP_REPLACE;
				ds.FrontFace.StencilFunc        = grcRSV::CMP_ALWAYS;
				ds.BackFace                     = ds.FrontFace;

				bs.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;

				ds_handle = grcStateBlock::CreateDepthStencilState(ds);
				bs_handle = grcStateBlock::CreateBlendState       (bs);
			}

			grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, ds_handle, bs_handle);
		}
		else
#endif // MIRROR_USE_STENCIL_MASK
#if MIRROR_USE_CRACK_TEXTURE
		if (bUseCrack)
		{
			pass = 2;

			grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_LessEqual, bs_handle);
		}
		else
#endif // MIRROR_USE_CRACK_TEXTURE
		{
			pass = ms_debugMirrorDrawDebugShaderShowRT ? 1 : 0;

			grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_LessEqual, bs_handle);
		}

		ms_mirrorShader->Bind(pass);
		grcBegin(drawTriStrip, 4);

		if (ms_debugMirrorDrawDebugShaderShowRT)
		{
			const grcViewport* vp = gVpMan.GetRenderGameGrcViewport();

			const float x0 = vp->GetTanHFOV()*(2.0f*ms_debugMirrorDrawDebugShaderShowRTOffsetX - 1.0f);
			const float y0 = vp->GetTanVFOV()*(2.0f*ms_debugMirrorDrawDebugShaderShowRTOffsetY - 1.0f);
			const float x1 = vp->GetTanVFOV()*(2.0f*ms_debugMirrorDrawDebugShaderShowRTSize) + x0;
			const float y1 = vp->GetTanVFOV()*(2.0f*ms_debugMirrorDrawDebugShaderShowRTSize)*(float)ms_pRenderTarget->GetHeight()/(float)ms_pRenderTarget->GetWidth() + y0;

			const Vec3V p00 = Transform(vp->GetCameraMtx(), Vec3V(x0, -y1, -1.0f)*ScalarV(vp->GetNearClip() + 0.01f));
			const Vec3V p10 = Transform(vp->GetCameraMtx(), Vec3V(x1, -y1, -1.0f)*ScalarV(vp->GetNearClip() + 0.01f));
			const Vec3V p01 = Transform(vp->GetCameraMtx(), Vec3V(x0, -y0, -1.0f)*ScalarV(vp->GetNearClip() + 0.01f));
			const Vec3V p11 = Transform(vp->GetCameraMtx(), Vec3V(x1, -y0, -1.0f)*ScalarV(vp->GetNearClip() + 0.01f));

			grcVertex(VEC3V_ARGS(p00), 0.0f, 0.0f, 0.0f, col, 1.0f, 1.0f);
			grcVertex(VEC3V_ARGS(p10), 0.0f, 0.0f, 0.0f, col, 0.0f, 1.0f);
			grcVertex(VEC3V_ARGS(p01), 0.0f, 0.0f, 0.0f, col, 1.0f, 0.0f);
			grcVertex(VEC3V_ARGS(p11), 0.0f, 0.0f, 0.0f, col, 0.0f, 0.0f);
		}
		else
		{
			grcVertex(VEC3V_ARGS(mirrorCorners[0]), 0.0f, 0.0f, 0.0f, col, 0.0f, 0.0f);
			grcVertex(VEC3V_ARGS(mirrorCorners[1]), 0.0f, 0.0f, 0.0f, col, 1.0f, 0.0f);
			grcVertex(VEC3V_ARGS(mirrorCorners[3]), 0.0f, 0.0f, 0.0f, col, 0.0f, 1.0f);
			grcVertex(VEC3V_ARGS(mirrorCorners[2]), 0.0f, 0.0f, 0.0f, col, 1.0f, 1.0f);
		}

		grcEnd();
		ms_mirrorShader->UnBind();
	}

	ms_mirrorShader->EndDraw();
}
#endif // MIRROR_DEV
