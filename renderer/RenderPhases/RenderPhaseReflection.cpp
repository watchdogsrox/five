// filename:	RenderPhaseReflection.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "RenderPhaseReflection.h"

#include <stdarg.h>
// C headers

// Rage headers
#include "profile/profiler.h"
#include "profile/timebars.h"
#include "grcore/allocscope.h"
#include "grcore/device.h"
#include "grcore/effect.h"
#include "grcore/stateblock.h"
#include "grcore/stateblock_internal.h"
#include "grcore/viewport.h"
#include "grcore/viewport_inline.h"
#include "grcore/texturegcm.h"
#include "grcore/quads.h"
#include "grmodel/model.h"
#include "grmodel/shaderfx.h"
#include "system/xtl.h"
#include "system/nelem.h"
#if RSG_PC
#include "renderer/computeshader.h"
#endif
#if RSG_ORBIS
#include "grcore/rendertarget_gnm.h"
#include "grcore/texturefactory_gnm.h"
#include "grcore/gfxcontext_gnm.h"
#endif

// Framework headers
#include "entity/drawdata.h"
#include "fwdrawlist/drawlistmgr.h"
#include "fwrenderer/renderlistgroup.h"
#include "fwscene/world/worldmgr.h"
#include "fwscene/scan/Scan.h"
#include "fwscene/scan/ScanResults.h"
#include "fwscene/scan/RenderPhaseCullShape.h"
#include "fwscene/search/SearchVolumes.h"
#include "fwmaths/vectorutil.h"
#include "fwutil/xmacro.h"

// Game headers
#include "camera/base/BaseCamera.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/ThirdPersonCamera.h" 
#include "camera/gameplay/GameplayDirector.h" 
#include "camera/helpers/CatchUpHelper.h"
#include "camera/helpers/ThirdPersonFrameInterpolator.h"
#include "camera/helpers/FrameInterpolator.h"
#include "camera/system/CameraManager.h"
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportManager.h"
#include "Cutscene/CutSceneCameraEntity.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "debug/LightProbe.h"
#include "debug/rendering/DebugDeferred.h"
#include "debug/TiledScreenCapture.h"
#include "game/clock.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Entities/EntityDrawHandler.h"
#include "renderer/Lights/lights.h"
#include "renderer/occlusion.h"
#include "renderer/PostProcessFX.h"
#include "renderer/Renderer.h"
#include "renderer/renderListGroup.h"
#include "renderer/RenderListBuilder.h"
#include "renderer/rendertargets.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/sprite2d.h"
#include "renderer/water.h"
#include "shaders/shaderlib.h"
#include "shaders/CustomShaderEffectMirror.h"
#include "scene/debug/PostScanDebug.h"
#include "scene/scene.h"
#include "scene/world/GameWorld.h"
#include "task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "timecycle/timecycle.h"
#include "timecycle/TimeCycleConfig.h"
#include "modelinfo/MloModelInfo.h"
#include "Vfx/VisualEffects.h"
#include "Vfx/Misc/MovieManager.h"
#include "Vfx/sky/Sky.h"

#if RSG_ORBIS
#include "grcore/gnmx.h"
#include "grcore/wrapper_gnm.h"
#endif

RENDER_OPTIMISATIONS();

#define CUBEMAPREF_SIZE			(REFLECTION_TARGET_HEIGHT/2)
#if REFLECTION_CUBEMAP_SAMPLING
	#define CUBEMAPREF_MIP			4
#else
	#define CUBEMAPREF_MIP			2
#endif

// double-buffering the depth is a workaround for B#1880337
#define DOUBLE_BUFFER_COLOR			(!RSG_PC)
#define DOUBLE_BUFFER_DEPTH			(!RSG_PC)

PF_PAGE(ReflectionCameraZPage, "Reflection Camera Z");
PF_GROUP(ReflectionCameraZ);
PF_LINK(ReflectionCameraZPage, ReflectionCameraZ);

PF_VALUE_FLOAT(BaseCameraZ, ReflectionCameraZ);
PF_VALUE_FLOAT(TargetCameraZ, ReflectionCameraZ);
PF_VALUE_FLOAT(FinalCameraZ, ReflectionCameraZ);

EXT_PF_GROUP(BuildRenderList);
PF_TIMER(RenderPhaseReflectionRL,BuildRenderList);

EXT_PF_GROUP(RenderPhase);
PF_TIMER(RenderPhaseReflection,RenderPhase);

extern bool gLastGenMode;
extern bool g_cheapmodeHogEnabled;

namespace rage 
{
	extern __THREAD grcContextHandle *g_grcCurrentContext;
}

// ---------------------------------------------------------------------------------------------- //

#if __BANK
	bool g_bRenderExteriorReflection = true;
	bool g_drawCullShapeSphere = false;
	bool g_drawInteriorCullShapeSphere = false;
	bool g_drawCullShapeSpheresInWireFrame = false;
	bool g_freezeCameraPosition = false;
	bool g_drawFacets = false;
	float gCentrePointRadius = 0.10f;
	spdSphere gCullShapeDebugSphere;
	spdSphere gInteriorCullShapeDebugSphere;
	bool  CRenderPhaseReflection::ms_enableLodRangeCulling = true;
	bool  CRenderPhaseReflection::ms_overrideLodRanges = false;

	bool g_drawFacet[CRenderPhaseReflection::FACET_COUNT] = { true, true, true, true, true, true };
	spdTransposedPlaneSet4 gDebugPlanes[CRenderPhaseReflection::FACET_COUNT];
#endif // __BANK

// ---------------------------------------------------------------------------------------------- //

#if RSG_PC
float CRenderPhaseReflection::ms_lodRanges[LODTYPES_DEPTH_TOTAL][2] = { {-1,-1}, {0.4f,100.0f}, {-1,-1}, {-1,-1}, {100.0f, 1500.335f}, {-1,-1}, {-1, -1} };
#else
float CRenderPhaseReflection::ms_lodRanges[LODTYPES_DEPTH_TOTAL][2] = { {-1,-1}, {0,50}, {-1,-1}, {-1,-1}, {150, 3000}, {-1,-1}, {-1, -1} };
#endif

// ---------------------------------------------------------------------------------------------- //

float CRenderPhaseReflection::ms_interiorRange = 10.0f;
float CRenderPhaseReflection::ms_targetLodRange[2] = { 0.0f, 5.0f};
float CRenderPhaseReflection::ms_targetSlodRange[2] = { 50.0f, 300.0f };
float CRenderPhaseReflection::ms_currentLodRange[2] = { 0.0f, 5.0f};;
float CRenderPhaseReflection::ms_currentSlodRange[2] = { 50.0f, 300.0f };
bool  CRenderPhaseReflection::ms_enableTimeBasedLerp[2] = { false, false };
float CRenderPhaseReflection::ms_adjustSpeed[2] = { 0.0f, 0.0f };

// ---------------------------------------------------------------------------------------------- //

grcEffectGlobalVar         CRenderPhaseReflection::ms_reflectionTextureId = grcegvNONE;
grcEffectGlobalVar         CRenderPhaseReflection::ms_reflectionMipCount = grcegvNONE;

// ---------------------------------------------------------------------------------------------- //
#if REFLECTION_CUBEMAP_SAMPLING
// ---------------------------------------------------------------------------------------------- //
grcEffectGlobalVar         CRenderPhaseReflection::ms_reflectionCubeTextureId = grcegvNONE;
// ---------------------------------------------------------------------------------------------- //
#endif
// ---------------------------------------------------------------------------------------------- //

bool CRenderPhaseReflection::ms_EnableComputeShaderMipBlur = false;
bool CRenderPhaseReflection::ms_EnableMipBlur = false;

// ---------------------------------------------------------------------------------------------- //

grcRenderTarget*			CRenderPhaseReflection::ms_renderTarget = NULL;

#if USE_REFLECTION_COPY
grcRenderTarget*			CRenderPhaseReflection::ms_renderTargetCopy = NULL;
#endif //USE_REFLECTION_COPY

Vec3V						CRenderPhaseReflection::ms_cameraPos = Vec3V(V_ZERO);

// ---------------------------------------------------------------------------------------------- //

#if __DEV
float CRenderPhaseReflection::ms_reflectionBlurSizeScale = 1.0f;
float CRenderPhaseReflection::ms_reflectionBlurSizeScalePoisson = 1.5f;
Vec3V CRenderPhaseReflection::ms_reflectionBlurSizeScaleV = Vec3V(V_ONE);
bool  CRenderPhaseReflection::ms_reflectionUsePoissonFilter[] = {false,false,false,false};
int   CRenderPhaseReflection::ms_reflectionBlurSize[] = {0,1,3,5,7,9};
#endif

// ---------------------------------------------------------------------------------------------- //

enum kCubeFacetOrdering
{
	CUBE_FACET_RIGHT = 0,
	CUBE_FACET_LEFT,
	CUBE_FACET_FRONT,
	CUBE_FACET_BACK,
	CUBE_FACET_TOP,
	CUBE_FACET_BOTTOM,
	CUBE_FACET_COUNT
};

// ---------------------------------------------------------------------------------------------- //

#if GS_INSTANCED_CUBEMAP
grcRenderTarget*			CRenderPhaseReflection::ms_renderTargetCubeDepth = NULL;
#else
grcRenderTarget*			CRenderPhaseReflection::ms_renderTargetFacet[2] = { NULL, NULL };
grcRenderTarget*			CRenderPhaseReflection::ms_renderTargetFacetDepth[2] = { NULL, NULL };
int							CRenderPhaseReflection::ms_renderTargetFacetIndex = 0;
#endif

grcRenderTarget*			CRenderPhaseReflection::ms_renderTargetCube = NULL;
#if RSG_ORBIS
	grcRenderTarget*			CRenderPhaseReflection::ms_renderTargetCubeArray = NULL;
#endif // RSG_ORBIS
#if REFLECTION_CUBEMAP_SAMPLING
	#if RSG_DURANGO
	grcRenderTarget*			CRenderPhaseReflection::ms_renderTargetCubeCopy= NULL;
	#endif
#endif

// ---------------------------------------------------------------------------------------------- //

Mat34V						CRenderPhaseReflection::ms_faceMatrices[6];
grcDepthStencilStateHandle	CRenderPhaseReflection::ms_interiorDepthStencilState = grcStateBlock::DSS_Invalid;
grcDepthStencilStateHandle	CRenderPhaseReflection::ms_lodDepthStencilState = grcStateBlock::DSS_Invalid;
grcDepthStencilStateHandle	CRenderPhaseReflection::ms_slod3DepthStencilState = grcStateBlock::DSS_Invalid;
grcRasterizerStateHandle	CRenderPhaseReflection::ms_cullingModeState = grcStateBlock::RS_Invalid;

#if GS_INSTANCED_CUBEMAP
bool						CRenderPhaseReflection::m_bCubeMapInst = false;
#endif

bool						CRenderPhaseReflection::ms_skyRendered = false;



spdTransposedPlaneSet4 CRenderPhaseReflection::ms_facetPlaneSets[FACET_COUNT];

// ---------------------------------------------------------------------------------------------- //

static void GetCameraFacetMatrix(Mat34V& mtx, u32 facet, Vec3V_In pos);

// ---------------------------------------------------------------------------------------------- //

CRenderPhaseReflection::CRenderPhaseReflection(CViewport* pGameViewport) : CRenderPhaseScanned(pGameViewport, "Cube-map Reflection", DL_RENDERPHASE_REFLECTION_MAP, 0.0f)
{
	SetRenderFlags(RENDER_SETTING_RENDER_ALPHA_POLYS | RENDER_SETTING_RENDER_CUTOUT_POLYS);
	SetVisibilityType(VIS_PHASE_PARABOLOID_REFLECTION);
	if (CSettingsManager::GetReflectionQuality() == (CSettings::eSettingsLevel) (0))
	{
		GetEntityVisibilityMask().SetAllFlags(VIS_ENTITY_MASK_HILOD_BUILDING);
	}
	else
	{
		GetEntityVisibilityMask().SetAllFlags(VIS_ENTITY_MASK_LOLOD_BUILDING | VIS_ENTITY_MASK_HILOD_BUILDING | VIS_ENTITY_MASK_LIGHT | VIS_ENTITY_MASK_OBJECT | VIS_ENTITY_MASK_DUMMY_OBJECT);
	}
	SetEntityListIndex(gRenderListGroup.RegisterRenderPhase(RPTYPE_REFLECTION, this));	
}

// ---------------------------------------------------------------------------------------------- //

CRenderPhaseReflection::~CRenderPhaseReflection()
{
}

// ---------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::InitClass()
{
	ms_reflectionTextureId = grcEffect::LookupGlobalVar("ReflectionTex");
	ms_reflectionMipCount = grcEffect::LookupGlobalVar("gReflectionMipCount");
#if REFLECTION_CUBEMAP_SAMPLING
	ms_reflectionCubeTextureId = grcEffect::LookupGlobalVar("ReflectionCubeTex");
#endif
	
	// ---------------------------------------------------------------------------------------------- //

	// ---------------------------------------------------------------------------------------------- //

	ms_faceMatrices[0] = Mat34V( 0, 0, -1,  0, 1, 0, -1, 0, 0,   0, 0, 0);  // +X
	ms_faceMatrices[1] = Mat34V( 0, 0,  1,  0, 1, 0,  1, 0, 0,   0, 0, 0);  // -X
	
	ms_faceMatrices[2] = Mat34V( 1, 0, 0,  0, 0,-1,  0, -1, 0,  0, 0, 0);  // +Y
	ms_faceMatrices[3] = Mat34V( 1, 0, 0,  0, 0, 1,  0,  1, 0,  0, 0, 0);  // -Y

	ms_faceMatrices[4] = Mat34V( 1, 0, 0,  0,  1, 0,  0, 0, -1,  0, 0, 0);  // +Z
	ms_faceMatrices[5] = Mat34V(-1, 0, 0,  0,  1, 0,  0, 0,  1,  0, 0, 0);  // -Z

	// Depth stencil state (interior/lod/slod3)

	grcDepthStencilStateDesc interiorDepthStencilState;
	interiorDepthStencilState.StencilEnable           = true;
	interiorDepthStencilState.DepthEnable			  = true;
	interiorDepthStencilState.DepthWriteMask		  = true;
	interiorDepthStencilState.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	interiorDepthStencilState.BackFace.StencilPassOp  = grcRSV::STENCILOP_REPLACE;
	interiorDepthStencilState.FrontFace.StencilFunc   = grcRSV::CMP_LESSEQUAL;
	interiorDepthStencilState.BackFace.StencilFunc    = grcRSV::CMP_LESSEQUAL;
	interiorDepthStencilState.StencilReadMask		  = 0xFF;     
	interiorDepthStencilState.StencilWriteMask		  = 0xFF;	  
	interiorDepthStencilState.DepthFunc               = grcRSV::CMP_LESSEQUAL;
	ms_interiorDepthStencilState = grcStateBlock::CreateDepthStencilState(interiorDepthStencilState, "ms_interiorDepthStencilState");

	grcDepthStencilStateDesc lodDepthStencilState;
	lodDepthStencilState.StencilEnable           = true;
	lodDepthStencilState.DepthEnable             = true;
	lodDepthStencilState.DepthWriteMask		     = true;
	lodDepthStencilState.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	lodDepthStencilState.BackFace.StencilPassOp  = grcRSV::STENCILOP_REPLACE;
	lodDepthStencilState.FrontFace.StencilFunc   = grcRSV::CMP_LESSEQUAL;
	lodDepthStencilState.BackFace.StencilFunc    = grcRSV::CMP_LESSEQUAL;
	lodDepthStencilState.StencilReadMask		 = 0xFF;     
	lodDepthStencilState.StencilWriteMask		 = 0xFF;	 
	lodDepthStencilState.DepthFunc               = grcRSV::CMP_LESSEQUAL;
	ms_lodDepthStencilState = grcStateBlock::CreateDepthStencilState(lodDepthStencilState, "ms_lodDepthStencilState");

	grcDepthStencilStateDesc slod3DepthStencilState;
	slod3DepthStencilState.StencilEnable           = true;
	slod3DepthStencilState.DepthEnable			   = true;
	slod3DepthStencilState.DepthWriteMask		   = true;
	slod3DepthStencilState.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	slod3DepthStencilState.BackFace.StencilPassOp  = grcRSV::STENCILOP_REPLACE;
	slod3DepthStencilState.FrontFace.StencilFunc   = grcRSV::CMP_LESSEQUAL;
	slod3DepthStencilState.BackFace.StencilFunc    = grcRSV::CMP_LESSEQUAL;
	slod3DepthStencilState.StencilReadMask		   = 0xFF;   
	slod3DepthStencilState.StencilWriteMask		   = 0xFF;   
	slod3DepthStencilState.DepthFunc               = grcRSV::CMP_LESSEQUAL;
	ms_slod3DepthStencilState = grcStateBlock::CreateDepthStencilState(slod3DepthStencilState, "ms_slod3DepthStencilState");

	grcRasterizerStateDesc cullingModeState;
	cullingModeState.CullMode = grcRSV::CULL_CCW;
	ms_cullingModeState = grcStateBlock::CreateRasterizerState(cullingModeState);
}

void CRenderPhaseReflection::CreateCubeMapRenderTarget(int res)
{
#if !RSG_PC
	res = CUBEMAPREF_SIZE;
#endif

	grcTextureFactory::CreateParams params;

	params.Format = grctfR11G11B10F;
	params.MipLevels = CUBEMAPREF_MIP;
	params.ArraySize = 6;
#if __BANK
	if (LightProbe::IsEnabled())
	{
		// TODO --
		// if we support R11G11B10_FLOAT format, then we need to convert from this format when saving out the dds
		// we can use the function PackedVector::XMLoadFloat3PK in C:\Program Files (x86)\Microsoft Durango XDK\xdk\Include\um\DirectXPackedVector.inl
		// or #include <xnamath.h> ..
		// also, if we support the full mip chain then we need to calculate the stride per face correctly (i'm not sure how to do this)
		res = 512;
		params.Format = grctfA16B16G16R16F;
		params.Lockable = true;
		params.MipLevels = 4;
	}
#endif // __BANK
	const int bpp = grcTextureFactory::GetInstance().GetBitsPerPixel(params.Format);

	// this corrupts gbuf0 when reflection phase is placed after cascade shadow
#if RSG_DURANGO
	#if !GS_INSTANCED_CUBEMAP
		params.PerArraySliceRTV = params.ArraySize;
	#endif
#endif

#if REFLECTION_CUBEMAP_SAMPLING
	ms_renderTargetCube = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "REFLECTION_MAP_COLOUR_CUBE", grcrtCubeMap, res, res, bpp, &params);
	#if RSG_DURANGO
		params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_REFLECTION_MAP;
		ms_renderTargetCubeCopy = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "REFLECTION_MAP_COLOUR_CUBE_COPY", grcrtCubeMap, res, res, bpp, &params);
	#endif
#else
	#if RSG_DURANGO
		params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_REFLECTION_MAP;
	#endif
	ms_renderTargetCube = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "REFLECTION_MAP_COLOUR_CUBE", grcrtCubeMap, res, res, bpp, &params WIN32PC_ONLY(, 0, ms_renderTargetCube));
	#if RSG_ORBIS
		// Allocate a texture array that overlaps with the cubemap so we can use it to generate mipmaps
		ms_renderTargetCubeArray = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "REFLECTION_MAP_COLOUR_CUBE_ARRAY", grcrtPermanent, res, res, bpp, &params, 0, ms_renderTargetCube);
	#endif
#endif

#if GS_INSTANCED_CUBEMAP
	params.Format = grctfD24S8;
	params.Lockable = false;
	params.MipLevels = 1;
	params.ArraySize = 6;
	ms_renderTargetCubeDepth = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "REFLECTION_MAP_COLOUR_CUBE_DEPTH", grcrtDepthBuffer, res, res, 32, &params WIN32PC_ONLY(, 0, ms_renderTargetCubeDepth));

	m_bCubeMapInst = false;
#else
	// Setup facet targets to render to
	params.ArraySize = 1;
	params.MipLevels = 1;
	params.PerArraySliceRTV = 1;
	params.HasParent = true;
	params.Parent = NULL;
	params.IsResolvable = true;
	params.IsRenderable = true;
	params.Lockable = 0;
	#if RSG_ORBIS
	if (GRCDEVICE.IsEQAA())
	{
		// we don't want EQAA on reflections if it's not used for the game
		params.EnableFastClear = true;
		params.ForceFragmentMask = true;
	}
	#endif
	#if DEVICE_EQAA
	params.Multisample = grcDevice::MSAAMode(4, 2);
	params.ForceFragmentMask = true;
	#if RSG_ORBIS
		// This is needed as with 2 fragments the PS4 generates a NaN for some pixels
		// in the 2nd fragment causing resolve issues. This was first spotted here for the 
		// cube map facets.
		params.EnableNanDetect = true;
	#endif
	#else
	#if RSG_PC && __D3D11
		const Settings& settings = CSettingsManager::GetInstance().GetSettings();
		int MSAA = settings.m_graphics.m_ReflectionMSAA;

		params.Multisample = (GRCDEVICE.GetDxFeatureLevel() >= 1010 && MSAA > 1) ? MSAA : grcDevice::MSAA_None;
		if(params.Multisample)
		{
			CViewportManager::ComputeVendorMSAA(params);
		}
	#else
		params.Multisample = (GRCDEVICE.GetDxFeatureLevel() >= 1100) ? grcDevice::MSAAMode(2) : grcDevice::MSAA_None;
	#endif	// #if RSG_PC
	#endif //DEVICE_EQAA
	
	for (int i = 0; i < 2; i++)
	{
		char targetFacetName[256];
		formatf(targetFacetName, 254, "REFLECTION_MAP_COLOUR_FACET %d", i);

		ms_renderTargetFacet[i] =
		#if !DOUBLE_BUFFER_COLOR
			i ? ms_renderTargetFacet[0] :
		#endif
			CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, targetFacetName, grcrtPermanent, res, res, bpp, &params WIN32PC_ONLY(, 0, ms_renderTargetFacet[i]));
	}

	params.Format = grctfD24S8;
	params.UseHierZ = true;
	params.IsResolvable = false;
	params.Lockable = 0;
	
	for (int i = 0; i < 2; i++)
	{
		ms_renderTargetFacetDepth[i] =
		#if !DOUBLE_BUFFER_DEPTH
			//only Orbis really needs to have different depth targets
			i ? ms_renderTargetFacetDepth[0] :
		#endif
			CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "REFLECTION_MAP_DEPTH", grcrtDepthBuffer, res, res, 32, &params WIN32PC_ONLY(, 0, ms_renderTargetFacetDepth[i]));
	}
#endif //GS_INSTANCED_CUBEMAP
}

// ---------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::UpdateViewport()
{
#if __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		return;
	}
#endif // __BANK

	SetActive(true);

	CRenderPhaseScanned::UpdateViewport();

	Vector3 cameraPos = camInterface::GetPos() + camInterface::GetFront() * 0.1f;
	Vector3 newCameraPos = cameraPos;

	if (!camInterface::IsRenderingFirstPersonCamera())
	{
		float gameplayBlendLevel = 0.0f;

		const atArray<tRenderedCameraObjectSettings>& renderedDirectors = camInterface::GetRenderedDirectors();

		for (u32 i = 0; i < renderedDirectors.GetCount(); i++)
		{
			const camBaseObject* obj = static_cast<const camBaseObject*>(renderedDirectors[i].m_Object);
		
			if (obj && obj->GetIsClassId(camGameplayDirector::GetStaticClassId()))
			{
				gameplayBlendLevel = renderedDirectors[i].m_BlendLevel;
				break;
			}
		}

		if (gameplayBlendLevel >= SMALL_FLOAT)
		{
			const camBaseCamera* renderedCamera = camInterface::GetGameplayDirector().GetRenderedCamera();

			if (renderedCamera && renderedCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
			{
				const camThirdPersonCamera* sourceCamera = static_cast<const camThirdPersonCamera*>(renderedCamera);
				const float sourceCameraZ = sourceCamera->GetFrame().GetPosition().z;
			
				float zToConsider = sourceCamera->GetBasePivotPosition().z;

				const camCatchUpHelper* catchUpHelper = sourceCamera->GetCatchUpHelper(); 
				if(catchUpHelper) 
				{ 
					const float blendLevel = catchUpHelper->GetBlendLevel(); 
					zToConsider = Lerp(blendLevel, zToConsider, sourceCameraZ);
				}
				else
				{
					const camFrameInterpolator* sourceFrameInterpolator = sourceCamera->GetFrameInterpolator();
					if(sourceFrameInterpolator && sourceFrameInterpolator->GetIsClassId(camThirdPersonFrameInterpolator::GetStaticClassId()))
					{
						const camThirdPersonFrameInterpolator* sourceThirdPersonFrameInterpolator = static_cast<const camThirdPersonFrameInterpolator*>(sourceFrameInterpolator);

						bool isSourceCameraCatchUpActive = false;
						const camBaseCamera* interpolationSourceCamera = sourceThirdPersonFrameInterpolator->GetSourceCamera();
						if(interpolationSourceCamera && interpolationSourceCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
						{
							const camThirdPersonCamera* interpolationSourceThirdPersonCamera = static_cast<const camThirdPersonCamera*>(interpolationSourceCamera);

							const camCatchUpHelper* catchUpHelper = interpolationSourceThirdPersonCamera->GetCatchUpHelper();
							if(catchUpHelper)
							{
								isSourceCameraCatchUpActive = true;

								const float interpolationBlendLevel = sourceThirdPersonFrameInterpolator->GetBlendLevel();
								const float catchUpBlendLevel = catchUpHelper->GetBlendLevel(); 
								const float blendLevel = Lerp(interpolationBlendLevel, catchUpBlendLevel, 0.0f);
								zToConsider = Lerp(blendLevel, zToConsider, sourceCameraZ);
							}
						}

						if(!isSourceCameraCatchUpActive)
						{
							zToConsider = sourceThirdPersonFrameInterpolator->GetInterpolatedBasePivotPosition().z;
						}
					}
				}

				PF_SET(TargetCameraZ, zToConsider);
				newCameraPos.z = Lerp(gameplayBlendLevel, cameraPos.z, zToConsider);

				// Do a world probe between the old a new position
				WorldProbe::CShapeTestProbeDesc probeParams;

				WorldProbe::CShapeTestHitPoint probeHitPoint;
				WorldProbe::CShapeTestResults probeResults(probeHitPoint);

				probeParams.SetStartAndEnd(cameraPos, newCameraPos);
				probeParams.SetIsDirected(true);
				probeParams.SetResultsStructure(&probeResults);
				probeParams.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
				probeParams.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_OBJECT_TYPE);
				probeParams.SetContext(WorldProbe::ENotSpecified);
				probeParams.SetTreatPolyhedralBoundsAsPrimitives(true);

				const bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(probeParams);

				if (hasHit)
				{
					newCameraPos = probeResults[0].GetHitPosition() + probeResults[0].GetHitNormal() * 0.05f;
				}
			}
		}	

		PF_SET(BaseCameraZ, camInterface::GetPos().z);
		PF_SET(FinalCameraZ, newCameraPos.z);
	}

#if __BANK
	if (!g_freezeCameraPosition)
#endif
	{
		ms_cameraPos = RCC_VEC3V(newCameraPos);

		// this prevents the camera from falling under the water surface when the player is swimming
		float waterHeight	= Water::GetWaterLevel();
		float cameraHeight	= ms_cameraPos.GetZf();
		bool isWaterHeightDefault = Water::IsWaterHeightDefault();

		if(!Water::IsCameraUnderwater() && 
		   (!isWaterHeightDefault && ((cameraHeight < waterHeight + 0.5f) && (cameraHeight > waterHeight - 25.0f))))
		{
			ms_cameraPos.SetZf(waterHeight + 0.5f);
		}
	}

	GetGrcViewport().SetCameraIdentity();
	GetGrcViewport().SetWorldIdentity();
	GetGrcViewport().SetProjection(Mat44V(V_IDENTITY));
	
	SetLodMask(fwRenderPhase::LODMASK_HD_INTERIOR | fwRenderPhase::LODMASK_LOD | fwRenderPhase::LODMASK_SLOD3 | fwRenderPhase::LODMASK_LIGHT);

	BuildFacetPlaneSets();
	#if __BANK
		if (!g_freezeCameraPosition)
		{
			for (int i= 0; i < FACET_COUNT; ++i)
			{
				gDebugPlanes[i] = ms_facetPlaneSets[i];
			}
		}
	#endif

#if __BANK
	u8 alpha = g_drawCullShapeSpheresInWireFrame ? 255 : 100;
	if (g_drawCullShapeSphere)
	{
		Color32 color(0, 255,   0, alpha);
		grcDebugDraw::Sphere(gCullShapeDebugSphere.GetCenter(), gCullShapeDebugSphere.GetRadiusf(),	 color, !g_drawCullShapeSpheresInWireFrame);
		grcDebugDraw::Sphere(gCullShapeDebugSphere.GetCenter(),	gCentrePointRadius,					 color, !g_drawCullShapeSpheresInWireFrame);
		
	}
	if (g_drawInteriorCullShapeSphere)
	{
		grcDebugDraw::Sphere(gInteriorCullShapeDebugSphere.GetCenter(), gInteriorCullShapeDebugSphere.GetRadiusf(), Color32(0, 255, 255, alpha), !g_drawCullShapeSpheresInWireFrame);
	}

	if (g_drawFacets)
	{
		Color32 colors[FACET_COUNT];
		colors[0].Set(255,   0,   0, 255);
		colors[1].Set(  0, 255,   0, 255);
		colors[2].Set(  0,   0, 255, 255);
		colors[3].Set(255, 255,   0, 255);
		colors[4].Set(255,   0, 255, 255);
		colors[5].Set(  0, 255, 255, 255);

		for (int i= 0; i < FACET_COUNT; ++i)
		{
			if (g_drawFacet[i])
			{
				PlaneSetDebugDraw(gDebugPlanes[i], Color32(colors[i]));
			}
		}
	}
#endif	// __BANK

		// Get values for the cull shape setup (we can't do this in UpdateViewport because the timecycle thinks it's not valid sometimes for some reason, see BS#553284)
#if __BANK
	if (!ms_overrideLodRanges)
#endif // __BANK
	{
		const float targetDiff = 5.0f;
		const float totalAdjusTime = 2.0f;
		const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrUpdateKeyframe();

		const camBaseCamera* activeCamera = camInterface::GetDominantRenderedCamera();
		const bool hasCameraCut = ((camInterface::GetFrame().GetFlags() & (camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation)) > 0);

		// On camera cut just set the sphere distances
		if (activeCamera && hasCameraCut)
		{
			ms_enableTimeBasedLerp[0] = false;
			ms_enableTimeBasedLerp[1] = false;

			ms_lodRanges[LODTYPES_DEPTH_LOD][0]  = currKeyframe.GetVar(TCVAR_REFLECTION_LOD_RANGE_START);
			ms_lodRanges[LODTYPES_DEPTH_LOD][1]  = currKeyframe.GetVar(TCVAR_REFLECTION_LOD_RANGE_END);

			ms_lodRanges[LODTYPES_DEPTH_SLOD3][0]  = currKeyframe.GetVar(TCVAR_REFLECTION_SLOD_RANGE_START);
			ms_lodRanges[LODTYPES_DEPTH_SLOD3][1]  = currKeyframe.GetVar(TCVAR_REFLECTION_SLOD_RANGE_END);
		}

		// Enable time-based lerp
		if (!ms_enableTimeBasedLerp[0] && 
			rage::Abs(ms_lodRanges[LODTYPES_DEPTH_LOD][1] - currKeyframe.GetVar(TCVAR_REFLECTION_LOD_RANGE_END)) > targetDiff)
		{
			ms_enableTimeBasedLerp[0] = true;
			ms_adjustSpeed[0] = 0.0f;

			ms_currentLodRange[0] =  ms_lodRanges[LODTYPES_DEPTH_LOD][0];
			ms_currentLodRange[1] =  ms_lodRanges[LODTYPES_DEPTH_LOD][1];

			ms_targetLodRange[0]  = currKeyframe.GetVar(TCVAR_REFLECTION_LOD_RANGE_START);
			ms_targetLodRange[1]  = currKeyframe.GetVar(TCVAR_REFLECTION_LOD_RANGE_END);
		}

		if (!ms_enableTimeBasedLerp[1] && 
			rage::Abs(ms_lodRanges[LODTYPES_DEPTH_SLOD3][1] - currKeyframe.GetVar(TCVAR_REFLECTION_SLOD_RANGE_END)) > targetDiff)
		{
			ms_enableTimeBasedLerp[1] = true;
			ms_adjustSpeed[1] = 0.0f;

			ms_currentSlodRange[0] =  ms_lodRanges[LODTYPES_DEPTH_SLOD3][0];
			ms_currentSlodRange[1] =  ms_lodRanges[LODTYPES_DEPTH_SLOD3][1];

			ms_targetSlodRange[0] = currKeyframe.GetVar(TCVAR_REFLECTION_SLOD_RANGE_START);
			ms_targetSlodRange[1] = currKeyframe.GetVar(TCVAR_REFLECTION_SLOD_RANGE_END);		
		}

		// If we're lerping add in the time (lerp will always be a fixed time)
		if (ms_enableTimeBasedLerp[0]) 
		{ 
			ms_adjustSpeed[0] += fwTimer::GetTimeStep();
			const float lodLerpAmount = Saturate(ms_adjustSpeed[0] / totalAdjusTime);	
			ms_lodRanges[LODTYPES_DEPTH_LOD][0]   = Lerp(lodLerpAmount,  ms_currentLodRange[0],  ms_targetLodRange[0]);
			ms_lodRanges[LODTYPES_DEPTH_LOD][1]   = Lerp(lodLerpAmount,  ms_currentLodRange[1],  ms_targetLodRange[1]);
			if (ms_adjustSpeed[0] > 1.0f) { ms_enableTimeBasedLerp[0] = false; }
		}

		if (ms_enableTimeBasedLerp[1]) 
		{ 
			ms_adjustSpeed[1] += fwTimer::GetTimeStep();
			const float slodLerpAmount = Saturate(ms_adjustSpeed[1] / totalAdjusTime);
			ms_lodRanges[LODTYPES_DEPTH_SLOD3][0] = Lerp(slodLerpAmount, ms_currentSlodRange[0], ms_targetSlodRange[0]);
			ms_lodRanges[LODTYPES_DEPTH_SLOD3][1] = Lerp(slodLerpAmount, ms_currentSlodRange[1], ms_targetSlodRange[1]);
			if (ms_adjustSpeed[1] > 1.0f) { ms_enableTimeBasedLerp[1] = false; }
		}

		// Disable HD buildings rendering outside (so we only get interiors)
		if (CSettingsManager::GetReflectionQuality() == (CSettings::eSettingsLevel) (0))
		{
			ms_lodRanges[LODTYPES_DEPTH_LOD][0] = 0.0f;
			ms_lodRanges[LODTYPES_DEPTH_LOD][1] = 0.0f;
		}

		ms_interiorRange = currKeyframe.GetVar(TCVAR_REFLECTION_INTERIOR_RANGE);

#if __BANK
	if (g_cheapmodeHogEnabled)
		ms_lodRanges[LODTYPES_DEPTH_SLOD3][1] = 300;
#endif // __BANK
	}
}

// ---------------------------------------------------------------------------------------------- //

// TODO -- we could do this in the vertex shader, would that be faster?
static __forceinline Vec2V_Out DualParaboloidPosTransform2D(Mat34V_In basis, Vec3V_In worldPos, bool bMapToHemisphere)
{
	const Vec3V   pos = UnTransformOrtho(basis, worldPos);
	const ScalarV len = Mag(pos);

	Vec2V p = pos.GetXY()/(pos.GetZ() + len);

	if (bMapToHemisphere)
	{
		p *= Vec2V(ScalarV(V_HALF), ScalarV(V_ONE)); // p *= {0.5f, 1.0f}
		p += Vec2V(ScalarV(V_HALF), ScalarV(V_ZERO))*basis.GetCol2().GetZ(); // p += {0.5f, 0.0f}
	}

	return p;
}

// ---------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::BuildDrawList()											
{
#if __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		return;
	}
#endif // __BANK

	// First function that gets run on the render thread

#if MULTIPLE_RENDER_THREADS
	CViewportManager::DLCRenderPhaseDrawInit();
#endif

	if (!HasEntityListIndex() || !GetViewport()->IsActive())
		return;

	DrawListPrologue();

	// 2/14/14 - cthomas - Set shader params from the timecycles just once for the whole reflection 
	// phase, before we draw all the facets of the cubemap. This call can be fairly expensive on 
	// next-gen consoles atm, and it doesn't appear that we need to do it per-facet (the way it used to be).
	DLC_Add(CRenderPhaseReflection::SetShaderParamsFromTimecycles);

#if __BANK
	if (LightProbe::IsEnabled())
	{
		DLC_Add(LightProbe::Capture, ms_renderTargetCube);
	}
#endif // __BANK

	DLC_Add(CShaderLib::SetUseFogRay, false);
	DLC_Add(CShaderLib::SetFogRayTexture);

	DLCPushTimebar("Reflection Map - Cubemap");
	GenerateCubeMap();
	DLCPopTimebar();

	DLCPushTimebar("Paraboloid Reflection Map Blur");

#if USE_REFLECTION_COPY
	DLC_Add(ReflectionMipMapAndBlur, ms_renderTarget, ms_renderTargetCopy);
#else
	DLC_Add(ReflectionMipMapAndBlur, ms_renderTarget, ms_renderTarget);
#endif	//USE_REFLECTION_COPY

	DLCPopTimebar();

	DLC(dlCmdSetCurrentViewportToNULL, ());
	DLC_Add(Lights::SetupDirectionalAndAmbientGlobals);

	DLC_Add(CShaderLib::SetUseFogRay, true);

	DrawListEpilogue();
}

void CRenderPhaseReflection::SetShaderParamsFromTimecycles()
{
	g_timeCycle.SetShaderData(CVisualEffects::RM_CUBE_REFLECTION);
}

// ---------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::BuildFacetPlaneSets()
{
	for (int i = 0; i < FACET_COUNT; ++i)
	{
		Mat34V cameraMatrix;
		GetCameraFacetMatrix(cameraMatrix, i, ms_cameraPos);

		const float fov = PI * 0.5f; 
		const ScalarV tanHalfOfFov(Tanf(0.5f * fov));
		ms_facetPlaneSets[i] = GenerateFrustum4(cameraMatrix, tanHalfOfFov, tanHalfOfFov);
	}
}

// ---------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::BuildRenderList()
{
	PF_FUNC(RenderPhaseReflectionRL);

#if __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		return;
	}
#endif // __BANK

	PF_START_TIMEBAR_DETAIL("ConstructRenderList");

	COcclusion::BeforeSetupRender(false, false, SHADOWTYPE_NONE, true, GetGrcViewport());
	CRenderer::ConstructRenderListNew(GetRenderFlags(), this);
	COcclusion::AfterPreRender();
}

static void RenderStateSetupGlobalReflection(fwRenderPassId pass, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, fwRenderGeneralFlags generalFlags)
{
	// Call the default one
	CRenderListBuilder::RenderStateSetupNormal(pass, bucket, renderMode, subphasePass, generalFlags);

	// Setup our sub Bucket Mask
	DLC(dlCmdSetBucketsAndRenderMode, (bucket, CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_REFLECTION), renderMode)); // damn lies.
}

// ---------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::ReflectionMipMapAndBlur(grcRenderTarget* refMap, grcRenderTarget* refMapTex)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if __D3D11 && !RSG_DURANGO
	/*if (ms_EnableComputeShaderMipBlur && GRCDEVICE.GetDxFeatureLevel() >= 1100)
	{
		((BlurCS*)gCSGameManager.GetComputeShader(CSGameManager::CS_BLUR))->SetBuffers(refMap, refMapTex);
		//#if __DEBUG
		//		static float ga = 3.0f;
		//		static float gb = 1.25f;
		//		((BlurCS*)gCSGameManager.GetComputeShader(CSGameManager::CS_BLUR))->GetSampleWeights(ga, gb);
		//#endif
		gCSGameManager.RunComputeShader(CSGameManager::CS_BLUR, 1, 1, 1);//
		((BlurCS*)gCSGameManager.GetComputeShader(CSGameManager::CS_BLUR))->SetBuffers(NULL, NULL);

		refMap->GenerateMipmaps();
	}
	else*/ if( ms_EnableMipBlur )
#endif
	{
		int levels = refMap->GetMipMapCount();
		int h = refMap->GetHeight();

		grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);

		Assertf(DebugDeferred::m_copyCubeToParaboloid, "must copy to paraboloid or use compute shader mipblur or modify the current mipblur pass to accomodate cubemap mip blurring.\n");
		const u32 startIndex = BANK_SWITCH((DebugDeferred::m_copyCubeToParaboloid) ? 0 : 1, 0);

		for (int i = startIndex; i < levels; i++)
		{
			grcTextureFactory::GetInstance().LockRenderTarget(0, refMap, NULL, 0, false, i);	

			int currentLevel = i;
			if (levels > 5 && i > 0)
			{
				currentLevel = Max(1, currentLevel - (levels - 5));
			}
			
			int blurSize = (currentLevel - 1) * 2 + 1;
			int pass = Clamp<int>(blurSize, 0, 7);
			float blurSizeScale = 1.0f;

			grcRenderTarget* target = ms_renderTargetCube;

			CSprite2d spriteBlit;
			spriteBlit.SetRefMipBlurParams(
				Vector4(
				(float)i, // refMipBlurParams_MipIndex
				(float)blurSize, // refMipBlurParams_BlurSize
				blurSizeScale, // refMipBlurParams_BlurSizeScale
				((float)(0x1 << i)) / ((float)h * 2.0f) // refMipBlurParams_TexelSizeY
				),
				target);

			grcTexture* refMipBlurTex = refMap;
			refMipBlurTex = refMapTex;
			spriteBlit.SetRefMipBlurMap(refMipBlurTex);

			const float quadSize = 1.0f;

			// For the Cube->Parab pass, draw 2 quads (one for each half of the dual parab map). This simplifies
			// the pixel shader math for computing parab->cube texel addresses.
			if ( i == 0 )
			{
				spriteBlit.BeginCustomList(CSprite2d::CS_MIPMAP_BLUR_0,refMapTex, 0);
				grcBeginQuads(2);
				grcDrawQuadf(
					0.0f, 1.0f, 
					quadSize*0.5f, 1.0f - quadSize,
					1.0f,                    // Z=1 identifies this quad as the left half (lower hemisphere)
					-1.0f,-1.0f,
					1.0f, 1.0f, 
					Color32(255,255,255,255));

				// This second quad is mirrored since we want the shared edge in
				// the dual paraboloid map to correspond to match up to and
				// reduce discontinuities.
				grcDrawQuadf(
					quadSize, 1.0f,
					0.5f, 1.0f - quadSize,
					-1.0f,                   // Z=-1 identifies this as the right half (upper hemisphere)
					-1.0, -1.0f,
					1.0f, 1.0f,
					Color32(255,255,255,255));

				grcEndQuads();
				spriteBlit.EndCustomList();
			}
			else
			{
				spriteBlit.BeginCustomList(CSprite2d::CS_MIPMAP_BLUR_0,refMapTex,pass);
				grcDrawSingleQuadf(
					0, 1, 
					quadSize, 1.0f - quadSize,
					0.0f,
					0, 0,
					1, 1,
					Color32(255,255,255,255));
				spriteBlit.EndCustomList();
			}

			grcResolveFlags flags;
			flags.MipMap = false;
			flags.NeedResolve = false;
			grcTextureFactory::GetInstance().UnlockRenderTarget(0, &flags);
			#if RSG_PC
				refMapTex->Copy(refMap, 0, i, 0, i);
			#endif
		}
	}
}

// ---------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::SetReflectionMap()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

#if __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		grcEffect::SetGlobalVar(ms_reflectionTextureId, grcTexture::NoneBlack);
	}
#endif // __BANK

	Assert(ms_renderTarget);
	grcEffect::SetGlobalVar(ms_reflectionTextureId, ms_renderTarget);
#if REFLECTION_CUBEMAP_SAMPLING
	grcEffect::SetGlobalVar(ms_reflectionCubeTextureId, ms_renderTargetCube);
#endif

	float mipCount = (float)ms_renderTarget->GetMipMapCount();
	grcEffect::SetGlobalVar(ms_reflectionMipCount, mipCount);
	
}

// ---------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::SetReflectionMapExt(const grcTexture* pTex)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	Assert(pTex);
	grcEffect::SetGlobalVar(ms_reflectionTextureId, pTex);
}

// ---------------------------------------------------------------------------------------------- //

static void RenderStateSetupCustomFlagsChangedGlobalReflection(fwRenderPassId /*pass*/, fwRenderBucket /*bucket*/, fwRenderMode /*renderMode*/, int subphasePass, fwRenderGeneralFlags /*generalFlags*/, u16 previousCustomFlags, u16 currentCustomFlags)
{
	CDrawListPrototypeManager::Flush();

	DLC( dlCmdSetRasterizerState, (CRenderPhaseReflection::ms_cullingModeState));

	// Final call of callback
	if (currentCustomFlags == 0)
	{
		DLCPushMarker("Final Render State");
		DLC( dlCmdSetDepthStencilStateEx, (grcStateBlock::DSS_Default, 0x00));
		DLCPopMarker();
		return;
	}

	Assertf(currentCustomFlags & fwEntityList::CUSTOM_FLAGS_LOD_SORT_ENABLED, "!(currentCustomFlags & CUSTOM_FLAGS_LOD_SORT_ENABLED)");

	// First call of callback
	if (previousCustomFlags == 0)
	{
		DLCPushMarker("Initial Render State");
		DLC( dlCmdSetDepthStencilStateEx, (grcStateBlock::DSS_Default, 0x00));
		DLCPopMarker();
	}

	if (currentCustomFlags != 0)
	{
		//const u32 previousLodSortVal = fwEntityList::CElement::GetLodSortValue(previousCustomFlags);
		const u32 currentLodSortVal  = fwEntityList::CElement::GetLodSortValue(currentCustomFlags);

		if (currentLodSortVal == 5) 
		{ 
			DLCPushMarker("Change to Interior Render state");
			DLC( dlCmdSetDepthStencilStateEx, (CRenderPhaseReflection::ms_interiorDepthStencilState, 0x03)); 
			DLCPopMarker();
		}

		if (currentLodSortVal == 4) 
		{ 
			DLCPushMarker("Change to LOD Render state");
			DLC( dlCmdSetDepthStencilStateEx, (CRenderPhaseReflection::ms_lodDepthStencilState, 0x03)); 
			DLCPopMarker();
		}

		if (currentLodSortVal == 1) 
		{
			if (subphasePass != SUBPHASE_REFLECT_FACET4)
			{
#if GS_INSTANCED_CUBEMAP
				DLC_Add(grcViewport::SetViewportInstancedRenderBit,0xff);
#endif
				bool bFirstCallThisDrawList = (subphasePass == SUBPHASE_REFLECT_FACET0);
				DLC_Add(CVisualEffects::RenderSky, CVisualEffects::RM_CUBE_REFLECTION, true, false, -1, -1, -1, -1, bFirstCallThisDrawList, 1.0f);
				DLC_Add(CRenderPhaseReflection::SetSkyRendered);
			}
			
			DLCPushMarker("Change to SLOD3 Render state");
			DLC( dlCmdSetDepthStencilStateEx, (CRenderPhaseReflection::ms_slod3DepthStencilState, 0x07)); 
			DLC( dlCmdSetBlendState, (grcStateBlock::BS_Normal));
			DLCPopMarker();
		}

		return;
	}
}

static void GetCameraFacetMatrix(Mat34V& mtx, u32 facet, Vec3V_In pos)
{
	mtx = CRenderPhaseReflection::ms_faceMatrices[facet];
	mtx.Setd(pos);
}

GRC_ALLOC_SCOPE_DECLARE_GLOBAL(static, s_RenderPhaseReflectionAllocScope)

// ----------------------------------------------------------------------------------------------- //
#if GS_INSTANCED_CUBEMAP
// ----------------------------------------------------------------------------------------------- //
void CRenderPhaseReflection::SetupFacetInstVP()
{
	grcViewport::SetInstVPWindow(6);
}

void CRenderPhaseReflection::SetupFacetInst(Vec3V cameraPos)
{
	#if ENABLE_PIX_TAGGING
		char buf[64];
		sprintf(buf, "Facet");
		PF_PUSH_MARKER(buf);
	#endif

	// use this instead if display cube rt in RAG is needed
	grcTextureFactory::GetInstance().LockRenderTarget(0, ms_renderTargetCube, ms_renderTargetCubeDepth);

	static grcViewport vp[MAX_NUM_CBUFFER_INSTANCING];
	for (int facet = 0; facet < 6; facet++)
	{
		Mat34V cameraMatrix;
		GetCameraFacetMatrix(cameraMatrix, facet, cameraPos); 

		// Setup viewport
		
		// This adds a pixel border for rendering which allows bilinear sampling
		vp[facet].Perspective(90.0f, 1.0f, 0.01f, rage::Max(500.0f, camInterface::GetFar()));  
		vp[facet].SetCameraMtx(cameraMatrix); 
		vp[facet].SetWorldIdentity();

		const Vec4V window = Vec4V(VECTOR4_TO_VEC4V(Vector4(0,0,RSG_PC ? (float)ms_renderTargetCube->GetWidth() : CUBEMAPREF_SIZE,RSG_PC ? (float)ms_renderTargetCube->GetWidth() : CUBEMAPREF_SIZE)));
		grcViewport::SetCurrentInstVP(&(vp[facet]), window, facet, false);
		//GRCDEVICE.SetInstViewport(vp,window,facet);

		// this shouldn't be necessary, but shader needs to have gViewInverse[3] information.
		// since cubemap cameras have the same cam position, we can do this.
		if (facet == 5)
			grcViewport::SetCurrent(&(vp[facet]));
	}
	grcViewport::SetInstVPWindow(6);

	// Force technique group
	const s32 previousGroupId = grmShaderFx::GetForcedTechniqueGroupId();
	gDrawListMgr->PushForcedTechnique(previousGroupId);

	u32 cubeTechniqueGroupId = grmShaderFx::FindTechniqueGroupId("cubeinst");
	Assertf(cubeTechniqueGroupId != -1, "Could not find cubemap technique");
	grmShaderFx::SetForcedTechniqueGroupId(cubeTechniqueGroupId);

	grcStateBlock::SetStates(grcStateBlock::RS_Default, ms_interiorDepthStencilState, grcStateBlock::BS_Default);

	grcViewport::SetInstancing(true);

	m_bCubeMapInst = true;
}

// ----------------------------------------------------------------------------------------------- //
#else // GS_INSTANCED_CUBEMAP
// ---------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::SetupFacet(u32 facet, Vec3V cameraPos)
{
#if ENABLE_PIX_TAGGING
	char buf[64];
	sprintf(buf, "Facet - %d", facet);
	PF_PUSH_MARKER(buf);
#endif

	GRC_ALLOC_SCOPE_PUSH(s_RenderPhaseReflectionAllocScope)

	Mat34V cameraMatrix;
	GetCameraFacetMatrix(cameraMatrix, facet, cameraPos);
	
	int renderToFacet = 0;
	grcRenderTarget* depth = ms_renderTargetFacetDepth[ms_renderTargetFacetIndex];
	grcRenderTarget* target = ms_renderTargetFacet[ms_renderTargetFacetIndex];

#if REFLECTION_CUBEMAP_SAMPLING
	int renderToFacet = facet;
	target = ms_renderTargetCube;
	#if __D3D11
		target = ms_renderTargetCubeCopy;
	#endif
#endif

	grcTextureFactory::GetInstance().LockRenderTarget(0, target, depth, renderToFacet, true);

	GRCDEVICE.Clear(true, Color32(0x00000000), true, 1.0f, true, 0xFF);

	// Setup viewport
	static grcViewport s_vp[NUMBER_OF_RENDER_THREADS];
	grcViewport *vp = s_vp+g_RenderThreadIndex;

	// This adds a pixel border for rendering which allows bilinear sampling
	vp->Perspective(90.0f, 1.0f, 0.01f, rage::Max(500.0f, camInterface::GetFar()));  
	vp->SetCameraMtx(cameraMatrix); 
	vp->SetWorldIdentity();

	grcViewport::SetCurrent(vp);

	// Force technique group
	const s32 previousGroupId = grmShaderFx::GetForcedTechniqueGroupId();
	gDrawListMgr->PushForcedTechnique(previousGroupId);

	u32 cubeTechniqueGroupId = grmShaderFx::FindTechniqueGroupId("cube");
	Assertf(cubeTechniqueGroupId != -1, "Could not find cubemap technique");
	grmShaderFx::SetForcedTechniqueGroupId(cubeTechniqueGroupId);

	grcStateBlock::SetStates(grcStateBlock::RS_Default, ms_interiorDepthStencilState, grcStateBlock::BS_Default);

	GRC_ALLOC_SCOPE_LOCK(s_RenderPhaseReflectionAllocScope)
}

// ----------------------------------------------------------------------------------------------- //
#endif // GS_INSTANCED_CUBEMAP
// ---------------------------------------------------------------------------------------------- //

// ---------------------------------------------------------------------------------------------- //
#if GS_INSTANCED_CUBEMAP
// ---------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::ShutdownFacetInst()
{
	m_bCubeMapInst = false;

	grcViewport::SetInstancing(false);

	const s32 previousGroupId = gDrawListMgr->PopForcedTechnique();
	grmShaderFx::SetForcedTechniqueGroupId(previousGroupId);

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

#if ENABLE_PIX_TAGGING
	PF_POP_MARKER();
#endif
}

// ---------------------------------------------------------------------------------------------- //
#else // GS_INSTANCED_CUBEMAP
// ---------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::ShutdownFacet(const u32 facet)
{
	GRC_ALLOC_SCOPE_UNLOCK(s_RenderPhaseReflectionAllocScope)

	const s32 previousGroupId = gDrawListMgr->PopForcedTechnique();
	grmShaderFx::SetForcedTechniqueGroupId(previousGroupId);
		
#if RSG_ORBIS
	// required to finalize fast-cleared targets
	static_cast<grcTextureFactoryGNM&>(grcTextureFactory::GetInstance()).FinishRendering();
#endif

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

	// Need to partial flush so writing to target is done before the resolve
#if RSG_DURANGO
	g_grcCurrentContext->GpuSendPipelinedEvent(D3D11X_GPU_PIPELINED_EVENT_PS_PARTIAL_FLUSH);
#elif RSG_ORBIS
	gfxc.triggerEvent( sce::Gnm::kEventTypeFlushAndInvalidateCbPixelData );
#endif

	if (ms_renderTargetFacet[ms_renderTargetFacetIndex]->GetMSAA() == grcDevice::MSAA_None)
	{
		ms_renderTargetCube->Copy(ms_renderTargetFacet[ms_renderTargetFacetIndex], facet, 0, 0, 0);
	} 
	else
	{
		static_cast<grcRenderTargetMSAA*>(ms_renderTargetFacet[ms_renderTargetFacetIndex])->Resolve(ms_renderTargetCube, facet);
	}

#if ENABLE_PIX_TAGGING
	PF_POP_MARKER();
#endif

	ms_renderTargetFacetIndex ^= 1;

	GRC_ALLOC_SCOPE_POP(s_RenderPhaseReflectionAllocScope)
}

// ---------------------------------------------------------------------------------------------- //
#endif // GS_INSTANCED_CUBEMAP
// ---------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::InitialState()
{
}

// ----------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::ShutdownState()
{
#if REFLECTION_CUBEMAP_SAMPLING

		PF_PUSH_MARKER("Generate mips");
	#if RSG_ORBIS
		ms_renderTargetCubeArray->GenerateMipmaps();
	#else
		ms_renderTargetCubeCopy->GenerateMipmaps();
	#endif
		PF_POP_MARKER();

		PF_PUSH_MARKER("Blurring");

		grcBlendStateHandle BS_Previous = grcStateBlock::BS_Active;
		grcDepthStencilStateHandle DSS_Previous = grcStateBlock::DSS_Active;
		grcRasterizerStateHandle RS_Previous = grcStateBlock::RS_Active;

		grcStateBlock::SetStates( grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default );

		#if RSG_ORBIS
			grcRenderTarget* target = ms_renderTargetCubeArray;	
		#else
			grcRenderTarget* target = ms_renderTargetCube;
		#endif

		CSprite2d generateMips;

		#if __D3D11
		PF_PUSH_MARKER("Copy top mip");
		for (int arraySlice = 0; arraySlice < target->GetArraySize(); arraySlice++)
		{
			ms_renderTargetCube->Copy(ms_renderTargetCubeCopy, arraySlice, 0);
		}
		PF_POP_MARKER();
		#endif

		for (int mipLvl = 1; mipLvl < target->GetMipMapCount(); mipLvl++)
		{
			for (int arraySlice = 0; arraySlice < target->GetArraySize(); arraySlice++)
			{
				Vector4 refMipParams = Vector4(
					(float)mipLvl - 1, 
					(float)arraySlice, 
					1, 
					((float)(0x1 << mipLvl)) / ((float)target->GetHeight())
					);

				grcTextureFactory::GetInstance().LockRenderTarget(0, target, NULL, arraySlice, false, mipLvl);	

				Mat34V cameraMtx = CRenderPhaseReflection::ms_faceMatrices[arraySlice];
				grcViewport::GetCurrent()->SetCameraMtx(cameraMtx);

				#if RSG_ORBIS
					generateMips.SetRefMipBlurParams(refMipParams, ms_renderTargetCube);
				#else
					generateMips.SetRefMipBlurParams(refMipParams, ms_renderTargetCubeCopy);
				#endif
				generateMips.BeginCustomList(CSprite2d::CS_MIPMAP, NULL, 2);

				grcDrawSingleQuadf(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f, -1.0f, 1.0f, 1.0f, Color32());

				generateMips.EndCustomList();

				grcTextureFactory::GetInstance().UnlockRenderTarget(0);
			}
		}

		grcStateBlock::SetStates( RS_Previous, DSS_Previous, BS_Previous );
	
		PF_POP_MARKER();

#endif
}

// ----------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::ConditionalSkyRender(u32 facet)
{
	if (!ms_skyRendered && facet != CUBE_FACET_BOTTOM)
	{
		bool bFirstCallThisDrawList = (facet == 0);
		CVisualEffects::RenderSky(CVisualEffects::RM_CUBE_REFLECTION, true, false, -1, -1, -1, -1, bFirstCallThisDrawList, 1.0f);
	}
	ms_skyRendered = false;
}

// ----------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::SetSkyRendered()
{
	ms_skyRendered = true;
}

// ----------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::ResetSkyRendered()
{
	ms_skyRendered = false;
}

// ----------------------------------------------------------------------------------------------- //

bool ShouldClearReflectionMap()
{
	bool clearReflection = false;

	CPlayerInfo	*pPlayerInfo = CGameWorld::GetMainPlayerInfo();
	if (pPlayerInfo && pPlayerInfo->GetPlayerPed() && camInterface::IsRenderingFirstPersonCamera())
	{
		const CPed* playerPed = pPlayerInfo->GetPlayerPed();
		CQueriableInterface* pedInterface = playerPed->GetPedIntelligence()->GetQueriableInterface();

		CVehicle *playerVehicle = NULL;
		float xOffset = 1.5f, yOffset = -0.05f;

		if (pedInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
		{
				CClonedEnterVehicleInfo* pClonedEnterVehicleInfo = static_cast<CClonedEnterVehicleInfo*>(pedInterface->GetTaskInfoForTaskType(
					CTaskTypes::TASK_ENTER_VEHICLE, 
					PED_TASK_PRIORITY_MAX));

			playerVehicle = (CVehicle*)(pClonedEnterVehicleInfo->GetTarget());
		}

		if (pedInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
		{
				CClonedEnterVehicleInfo* pClonedEnterVehicleInfo = static_cast<CClonedEnterVehicleInfo*>(pedInterface->GetTaskInfoForTaskType(
					CTaskTypes::TASK_EXIT_VEHICLE, 
					PED_TASK_PRIORITY_MAX));

			playerVehicle = (CVehicle*)(pClonedEnterVehicleInfo->GetTarget());
			yOffset = -0.3f;
		}

		CTaskMotionInVehicle* pMotionTask = static_cast<CTaskMotionInVehicle*>(playerPed->GetPedIntelligence()->FindTaskActiveMotionByType( CTaskTypes::TASK_MOTION_IN_VEHICLE));
		if (pMotionTask != NULL)
		{
			playerVehicle = playerPed->GetMyVehicle();
		}

		if (playerVehicle && playerVehicle->GetVehicleModelInfo()->GetModelNameHash() == MI_PLANE_LUXURY_JET3.GetName().GetHash())
		{
			Matrix34 mtx;
			playerVehicle->GetGlobalMtx(playerVehicle->GetVehicleModelInfo()->GetBoneIndex(VEH_SIREN_9), mtx);
			mtx.Transpose3x4();

			Vector3 playerPos = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition()), planePlayerPos;
			mtx.Transform(playerPos, planePlayerPos);

			clearReflection = (abs(planePlayerPos.x) < xOffset && planePlayerPos.y < yOffset);
		}
	}

	if (CVehicle::IsInsideSpecificVehicleModeEnabled(MI_PLANE_CARGOPLANE.GetName().GetHash()) ||
		CVehicle::IsInsideSpecificVehicleModeEnabled(MI_PLANE_LUXURY_JET3.GetName().GetHash()))
	{
		clearReflection = true;
	}

	return clearReflection;
}

// ----------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::RenderExtras()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	#if GS_INSTANCED_CUBEMAP
		GRCDEVICE.Clear(true, Color32(0, 0, 0, 255), false, 0.0f, false, 0);
	#else
		grcRenderTarget* target = ms_renderTargetFacet[ms_renderTargetFacetIndex];
		GRCDEVICE.ClearRect(0, 0, target->GetWidth(), target->GetHeight(), true, Color32(0, 0, 0, 255), false, 0.0f, false, 0);
	#endif
}

// ----------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::GenerateCubeMap()
{
	DLCPushMarker("Cubemap Generation");

	DLC_Add(InitialState);
	
#if !GS_INSTANCED_CUBEMAP
	for (u32 i = 0; i < 6; i++)
#endif
	{
#if GS_INSTANCED_CUBEMAP
		DLC_Add(SetupFacetInst, ms_cameraPos);
#else
		DLC_Add(SetupFacet, i, ms_cameraPos);
#endif

		#if __BANK && !GS_INSTANCED_CUBEMAP
		if (DebugDeferred::m_enableCubeMapDebugMode)
		{
			const u32 index = u32(i / 2.0f);
			
			u32 col[3];
			col[0] = col[1] = col[2] = 0;
			col[index] = (i % 2 == 0) ? 255 : 128;
			DLC(dlCmdClearRenderTarget, (true, Color32(col[0], col[1], col[2]), true, 1.0f, true, 0xFF));
		}
		else
		#endif
		{
			#if GS_INSTANCED_CUBEMAP
				// PD_NEXT_PASS: Let the resolve do the clear on 360
				DLC(dlCmdClearRenderTarget, (true, Color32(0, 0, 0, 255), true, 1.0f, true, 0xFF));
			#endif

			const tcKeyframeUncompressed& keyframe = g_timeCycle.GetCurrUpdateKeyframe();
			Vector4 reflectionTweaks = Vector4(
				keyframe.GetVar(TCVAR_REFLECTION_TWEAK_INTERIOR_AMB),
				keyframe.GetVar(TCVAR_REFLECTION_TWEAK_EXTERIOR_AMB),
				keyframe.GetVar(TCVAR_REFLECTION_TWEAK_EMISSIVE), 0.0f);
			BANK_ONLY(DebugDeferred::OverrideReflectionTweaks(reflectionTweaks);)

			DLC_Add(PostFX::SetLDR16bitHDR16bit);

			DLC_Add(CShaderLib::ApplyDayNightBalance);
			DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShadowMap, (const grcTexture*)NULL);
			DLC_Add(Lights::SetupDirectionalAndAmbientGlobalsTweak, 1.0f, reflectionTweaks.x, reflectionTweaks.y);
			DLC_Add(CShaderLib::ApplyGlobalReflectionTweaks);

			int filter =
				CRenderListBuilder::RENDER_OPAQUE |
				CRenderListBuilder::RENDER_FADING |
				CRenderListBuilder::RENDER_ALPHA |
				CRenderListBuilder::RENDER_DONT_RENDER_PLANTS |
				CRenderListBuilder::RENDER_DONT_RENDER_TREES |
				CRenderListBuilder::RENDER_DONT_RENDER_DECALS_CUTOUTS;

			DLC(dlCmdSetStates, (grcStateBlock::RS_Default, grcStateBlock::DSS_Default, grcStateBlock::BS_Default));

#if GS_INSTANCED_CUBEMAP
			fwRenderListBuilder::SetInstancingMode(true);
			int subPhase = SUBPHASE_REFLECT_FACET0;
#else
			int subPhase = SUBPHASE_REFLECT_FACET0 + i;
#endif
			// Call AddToDrawList
			RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseReflection");
			CRenderListBuilder::AddToDrawList(
				GetEntityListIndex(), 
				filter, 
				CRenderListBuilder::USE_SIMPLE_LIGHTING_DISTANCE_FADE, 
				subPhase,
				RenderStateSetupGlobalReflection, 
				CRenderListBuilder::RenderStateCleanupNormal,
				RenderStateSetupCustomFlagsChangedGlobalReflection);
			RENDERLIST_DUMP_PASS_INFO_END();

#if GS_INSTANCED_CUBEMAP
			DLC_Add(grcViewport::SetViewportInstancedRenderBit,0xff);
			DLC_Add(ConditionalSkyRender, 0);

			DLC_Add(SetupFacetInstVP);
#else
			DLC_Add(ConditionalSkyRender, i);		
#endif

			if(Water::IsCameraUnderwater())//do not render if underwater of rendering sky facet
			{
				DLC(dlCmdSetStates, (grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_Default, grcStateBlock::BS_Normal));
				DLC_Add(Water::RenderWaterFLODDisc, (s32)Water::pass_cubeunderwater, Water::GetWaterLevel() + 4.0f);
			}
			else
			{
#if GS_INSTANCED_CUBEMAP
				DLC_Add(Water::RenderWaterCubeInst);
#else
				if(i != 4)
					DLC_Add(Water::RenderWaterCube);
#endif
			}
			DLC_Add(CVisualEffects::RenderCoronas, CVisualEffects::RM_CUBE_REFLECTION, 1.0f, 1.0f);

			bool clearReflection = ShouldClearReflectionMap();

			if (clearReflection)
			{
				DLC_Add(RenderExtras);
			}
		}
#if GS_INSTANCED_CUBEMAP
		fwRenderListBuilder::SetInstancingMode(false);
		DLC_Add(ShutdownFacetInst);
#else
		DLC_Add(ShutdownFacet, i);
#endif
	}

	DLC_Add(ShutdownState);
	DLC(dlCmdSetCurrentViewportToNULL, ());
	
	DLCPopMarker();
}

u32 CRenderPhaseReflection::GetCullFlags() const
{
#if __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		return 0;
	}
#endif // __BANK

	u32 flags =
		CULL_SHAPE_FLAG_REFLECTIONS								|
		CULL_SHAPE_FLAG_TRAVERSE_PORTALS						|
		CULL_SHAPE_FLAG_RENDER_LOD_BUILDING_BASED_ON_INTERIOR	|
		CULL_SHAPE_FLAG_RENDER_EXTERIOR							|
		CULL_SHAPE_FLAG_RENDER_INTERIOR							|
		CULL_SHAPE_FLAG_USE_LOD_RANGES_IN_EXTERIOR				|
		CULL_SHAPE_FLAG_USE_SEPARATE_INTERIOR_SPHERE_GEOMETRY;

	return flags;
}

// ---------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::SetCullShape(fwRenderPhaseCullShape &cullShape)
{
	float maxRadius = 0.0f;
	cullShape.SetFlags(CULL_SHAPE_FLAG_GEOMETRY_SPHERE);
	cullShape.InvalidateLodRanges();

	for (int i = 0; i < LODTYPES_DEPTH_TOTAL; i++)
	{
		float mult = BANK_SWITCH((i == LODTYPES_DEPTH_LOD) ? DebugDeferred::m_reflectionLodRangeMultiplier : 1.0f, 1.0f);
		cullShape.EnableLodRange(i, ms_lodRanges[i][0], ms_lodRanges[i][1] * mult);
		maxRadius = Max<float>(maxRadius, ms_lodRanges[i][1]);
	}

	spdSphere sphere(ms_cameraPos, ScalarV(maxRadius));
	spdSphere interiorSphere(ms_cameraPos, ScalarV(ms_interiorRange));
#if __BANK
	if (g_freezeCameraPosition)
	{
		cullShape.SetSphere(gCullShapeDebugSphere);
		cullShape.SetInteriorSphere(gCullShapeDebugSphere);
	}
	else
	{
		gCullShapeDebugSphere = sphere;
		gInteriorCullShapeDebugSphere = interiorSphere;

		cullShape.SetSphere(sphere);
		cullShape.SetInteriorSphere(interiorSphere);
	}
#else
	cullShape.SetSphere(sphere);
	cullShape.SetInteriorSphere(interiorSphere);
#endif
	cullShape.SetCameraPosition(ms_cameraPos);
}

// ---------------------------------------------------------------------------------------------- //
#if __BANK
// ---------------------------------------------------------------------------------------------- //

void CRenderPhaseReflection::AddWidgets(bkGroup& group)
{
#if __DEV
	bkGroup& advanced = *group.AddGroup("Advanced", false);

	advanced.AddToggle("Reflection Use Poisson Filter [mip=0]"             , &ms_reflectionUsePoissonFilter[0]);
	advanced.AddToggle("Reflection Use Poisson Filter [mip=1]"             , &ms_reflectionUsePoissonFilter[1]);
	advanced.AddToggle("Reflection Use Poisson Filter [mip=2]"             , &ms_reflectionUsePoissonFilter[2]);
	advanced.AddToggle("Reflection Use Poisson Filter [mip=3]"             , &ms_reflectionUsePoissonFilter[3]);
	advanced.AddSlider("Reflection Blur Size [mip=0]"                      , &ms_reflectionBlurSize[0], 0, 8, 1);
	advanced.AddSlider("Reflection Blur Size [mip=1]"                      , &ms_reflectionBlurSize[1], 0, 8, 1);
	advanced.AddSlider("Reflection Blur Size [mip=2]"                      , &ms_reflectionBlurSize[2], 0, 8, 1);
	advanced.AddSlider("Reflection Blur Size [mip=3]"                      , &ms_reflectionBlurSize[3], 0, 8, 1);
	#if __D3D11 || RSG_ORBIS
		advanced.AddSlider("Reflection Blur Size [mip=4] (res dependent)"      , &ms_reflectionBlurSize[4], 0, 12, 1);
		advanced.AddSlider("Reflection Blur Size [mip=5] (res dependent)"      , &ms_reflectionBlurSize[5], 0, 12, 1);
	#endif
	advanced.AddSlider("Reflection Blur Size Scale"                        , &ms_reflectionBlurSizeScale, 0.0f, 10.0f, 1.0f/32.0f);
	advanced.AddSlider("Reflection Blur Size Scale Poisson"                , &ms_reflectionBlurSizeScalePoisson, 0.0f, 10.0f, 1.0f/32.0f);
	advanced.AddVector("Reflection Blur Size Scale V"                      , &ms_reflectionBlurSizeScaleV, 0.0f, 1.0f, 1.0f/32.0f);
#endif // __DEV

	const float		min = -1;
	const float		max = 3000;
	const float		delta = 1;

	bkGroup& lod = *group.AddGroup("LOD Range Culling", false);
	lod.AddToggle("Enable LOD range culling", &ms_enableLodRangeCulling);
	lod.AddToggle("Override LOD range culling from timecycle", &ms_overrideLodRanges);
	lod.AddSlider("HD min", &(ms_lodRanges[LODTYPES_DEPTH_HD][0]), min, max, delta);
	lod.AddSlider("HD max", &(ms_lodRanges[LODTYPES_DEPTH_HD][1]), min, max, delta);
	lod.AddSlider("LOD min", &(ms_lodRanges[LODTYPES_DEPTH_LOD][0]), min, max, delta);
	lod.AddSlider("LOD max", &(ms_lodRanges[LODTYPES_DEPTH_LOD][1]), min, max, delta);
	lod.AddSlider("SLOD1 min", &(ms_lodRanges[LODTYPES_DEPTH_SLOD1][0]), min, max, delta);
	lod.AddSlider("SLOD1 max", &(ms_lodRanges[LODTYPES_DEPTH_SLOD1][1]), min, max, delta);
	lod.AddSlider("SLOD2 min", &(ms_lodRanges[LODTYPES_DEPTH_SLOD2][0]), min, max, delta);
	lod.AddSlider("SLOD2 max", &(ms_lodRanges[LODTYPES_DEPTH_SLOD2][1]), min, max, delta);
	lod.AddSlider("SLOD3 min", &(ms_lodRanges[LODTYPES_DEPTH_SLOD3][0]), min, max, delta);
	lod.AddSlider("SLOD3 max", &(ms_lodRanges[LODTYPES_DEPTH_SLOD3][1]), min, max, delta);
	lod.AddSlider("SLOD4 min", &(ms_lodRanges[LODTYPES_DEPTH_SLOD4][0]), min, max, delta);
	lod.AddSlider("SLOD4 max", &(ms_lodRanges[LODTYPES_DEPTH_SLOD4][1]), min, max, delta);
	lod.AddSlider("ORPHANHD min", &(ms_lodRanges[LODTYPES_DEPTH_ORPHANHD][0]), min, max, delta);
	lod.AddSlider("ORPHANHD max", &(ms_lodRanges[LODTYPES_DEPTH_ORPHANHD][1]), min, max, delta);

	bkGroup& tweaks = *group.AddGroup("Reflection Tweaks", false);
	tweaks.AddToggle("Override", &DebugDeferred::m_enableReflectiobTweakOverride);
	tweaks.AddSlider("Artificial Interior ambient", &DebugDeferred::m_reflectionTweaks[0], 0.0f, 32.0f, 0.01f);
	tweaks.AddSlider("Artificial Exterior ambient", &DebugDeferred::m_reflectionTweaks[1], 0.0f, 32.0f, 0.01f);
	tweaks.AddSlider("Emissive multiplier", &DebugDeferred::m_reflectionTweaks[2], 0.0f, 32.0f, 0.01f);

	bkGroup& facets =*group.AddGroup("Facet matrices", false);
	facets.AddVector("Facet 0", &ms_faceMatrices[0], -1.0f, 1.0f, 0.01f);
	facets.AddSeparator();
	facets.AddVector("Facet 1", &ms_faceMatrices[1], -1.0f, 1.0f, 0.01f);
	facets.AddSeparator();
	facets.AddVector("Facet 2", &ms_faceMatrices[2], -1.0f, 1.0f, 0.01f);
	facets.AddSeparator();
	facets.AddVector("Facet 3", &ms_faceMatrices[3], -1.0f, 1.0f, 0.01f);
	facets.AddSeparator();
	facets.AddVector("Facet 4", &ms_faceMatrices[4], -1.0f, 1.0f, 0.01f);
	facets.AddSeparator();
	facets.AddVector("Facet 5", &ms_faceMatrices[5], -1.0f, 1.0f, 0.01f);

	group.AddToggle("Draw Cull Shape Sphere",&g_drawCullShapeSphere);
	group.AddToggle("Draw Interior Cull Shape Sphere",&g_drawInteriorCullShapeSphere);
	group.AddToggle("Draw Cull Shape Spheres in wire frame",&g_drawCullShapeSpheresInWireFrame);
	group.AddToggle("Freeze camera position",&g_freezeCameraPosition);

	group.AddSeparator();
	group.AddToggle("Draw Cull Shape Facets", &g_drawFacets);
	group.AddSeparator();
	group.AddToggle("Draw Cull Shape Facet 0", &g_drawFacet[0]);
	group.AddToggle("Draw Cull Shape Facet 1", &g_drawFacet[1]);
	group.AddToggle("Draw Cull Shape Facet 2", &g_drawFacet[2]);
	group.AddToggle("Draw Cull Shape Facet 3", &g_drawFacet[3]);
	group.AddToggle("Draw Cull Shape Facet 4", &g_drawFacet[4]);
	group.AddToggle("Draw Cull Shape Facet 5", &g_drawFacet[5]);

	group.AddSeparator();
	group.AddToggle("Enable Facet Culling", &g_scanDebugFlagsExtended, SCAN_DEBUG_EXT_ENABLE_REFLECTION_FACET_CULLING);
	group.AddSeparator();
	group.AddToggle("Enable Facet 0", &g_scanDebugFlagsExtended, SCAN_DEBUG_EXT_ENABLE_REFLECTION_FACET0);
	group.AddToggle("Enable Facet 1", &g_scanDebugFlagsExtended, SCAN_DEBUG_EXT_ENABLE_REFLECTION_FACET1);
	group.AddToggle("Enable Facet 2", &g_scanDebugFlagsExtended, SCAN_DEBUG_EXT_ENABLE_REFLECTION_FACET2);
	group.AddToggle("Enable Facet 3", &g_scanDebugFlagsExtended, SCAN_DEBUG_EXT_ENABLE_REFLECTION_FACET3);
	group.AddToggle("Enable Facet 4", &g_scanDebugFlagsExtended, SCAN_DEBUG_EXT_ENABLE_REFLECTION_FACET4);
	group.AddToggle("Enable Facet 5", &g_scanDebugFlagsExtended, SCAN_DEBUG_EXT_ENABLE_REFLECTION_FACET5);

}
// ---------------------------------------------------------------------------------------------- //
#endif // __BANK
// ---------------------------------------------------------------------------------------------- //
