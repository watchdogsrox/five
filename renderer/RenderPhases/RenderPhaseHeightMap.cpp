//
// filename:	RenderPhaseHeightMap.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "RenderPhaseHeightMap.h"

// C headers
#include <stdarg.h>

// Framework headers
#include "fwdrawlist/drawlistmgr.h"
#include "fwrenderer/renderlistgroup.h"
#include "fwScene/scan/Scan.h"
#include "fwscene/scan/RenderPhaseCullShape.h"
#include "fwsys/timer.h"

// Rage headers
#include "profile/profiler.h"
#include "profile/timebars.h"
#include "grmodel/shaderfx.h"
#include "grcore/image.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportManager.h"
#include "game/clock.h"
#include "renderer/occlusion.h"
#include "renderer/renderListGroup.h"
#include "renderer/RenderListBuilder.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/rendertargets.h"
#include "renderer/water.h"
#include "shaders/shaderlib.h"
#include "peds/ped.h"
#include "scene/debug/PostScanDebug.h"
#include "scene/portals/FrustumDebug.h"
#include "scene/scene.h"
#include "modelinfo/MloModelInfo.h"
#include "TimeCycle/TimeCycleConfig.h"
#include "Vfx/VisualEffects.h"
#include "Vfx/VfxHelper.h"
#include "Vfx/Systems/VfxWeather.h"
#include "scene\world\GameWorldHeightMap.h"

// --- Defines ------------------------------------------------------------------
#define HEIGHTMAP_USE_SHADOW_SHADER (__XENON)
#define HEIGHTMAP_USE_SHADOW_TECHNIQUE (1 && __PS3)

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------


EXT_PF_GROUP(BuildRenderList);
PF_TIMER(RenderPhaseHeightRL,BuildRenderList);

EXT_PF_GROUP(RenderPhase);
PF_TIMER(RenderPhaseHeight,RenderPhase);

// --- Code ---------------------------------------------------------------------
const float CRenderPhaseHeight::m_fNearPlane = 0.01f;

float CRenderPhaseHeight::m_offsetShiftX = 1.008f;
float CRenderPhaseHeight::m_offsetShiftY = 0.992f;

static CRenderPhaseHeight* g_GlobalGetPtFxGPUCollisionMap = 0;

void CRenderPhaseHeight::SetPtFxGPUCollisionMap(CRenderPhaseHeight* hgt)
{
	g_GlobalGetPtFxGPUCollisionMap = hgt;
}

CRenderPhaseHeight*	CRenderPhaseHeight::GetPtFxGPUCollisionMap()
{
	return g_GlobalGetPtFxGPUCollisionMap;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CRenderPhaseHeight::CRenderPhaseHeight(CViewport* pGameViewport,const Params& p) 
	: CRenderPhaseScanned(pGameViewport, "Height Map", DL_RENDERPHASE_RAIN_COLLISION_MAP, 0.0f)
	, m_renderFrame( false )
	, m_params( p )
	, m_sRenderTarget(NULL)
	, m_sRenderTargetRT(NULL)
#if RAIN_UPDATE_CPU_HEIGHTMAP
	, m_numStagingRenderTargets(0)
	, m_currentStagingRenderTarget(0)
#endif //RAIN_UPDATE_CPU_HEIGHTMAP
{
	SetRenderFlags(
		RENDER_SETTING_RENDER_ALPHA_POLYS |
		RENDER_SETTING_RENDER_WATER |
		0);

	SetVisibilityType( VIS_PHASE_RAIN_COLLISION_MAP );
	GetEntityVisibilityMask().SetAllFlags( VIS_ENTITY_MASK_VEHICLE | VIS_ENTITY_MASK_HILOD_BUILDING | VIS_ENTITY_MASK_LOLOD_BUILDING | VIS_ENTITY_MASK_ANIMATED_BUILDING );

	SetEntityListIndex(gRenderListGroup.RegisterRenderPhase( RPTYPE_HEIGHTMAP, this ));

	m_Scale[0].Zero();
	m_Scale[1].Zero();
	m_Offset[0].Set(0.0, 0.0, 1.0);
	m_Offset[1].Set(0.0, 0.0, 1.0);

#if __BANK
	m_quantiseToGrid = true;
	m_renderVehiclesInHeightMap = true;
	m_renderObjectsInHeightMap = true;
	m_renderAnimatedBuildingsInHeightMap = true;
	m_renderWaterInHeightmap = true;
	m_useShadowTechnique = HEIGHTMAP_USE_SHADOW_TECHNIQUE;
	m_VisualizeHeightmapFrustum = false;
	m_FreezeHeightmapFrustum = false;
	m_pVisualiseCamFrustum = NULL;
	m_bSkipHeightWhenExteriorNotVisible = true;
	m_fExtraEmitterBottomHeight = EXTRA_EMITTER_BOTTOM_HEIGHT;
	m_fExtraBottomHeight		= EXTRA_BOTTOM_HEIGHT;
	m_useShadowShader = HEIGHTMAP_USE_SHADOW_SHADER;
#endif // __BANK

#if RAIN_UPDATE_CPU_HEIGHTMAP
	if(ptxgpuBase::CpuRainUpdateEnabled())
	{
		//Create the m_stagingRenderTargets[]
		//Thinking they should be format R32 textures so that it is a known format that's easy to read
		m_numStagingRenderTargets = MAX_GPUS;
		
		grcTextureFactory::TextureCreateParams texParams(
			grcTextureFactory::TextureCreateParams::SYSTEM,	//STAGING 
			grcTextureFactory::TextureCreateParams::LINEAR,	
			grcsRead, //|grcsWrite
			NULL, 
			grcTextureFactory::TextureCreateParams::RENDERTARGET, 
			grcTextureFactory::TextureCreateParams::MSAA_NONE);

		char szName[256];
		for(u32 t=0; t<m_numStagingRenderTargets; t++)
		{
			formatf(szName, "STAG_HEIGHT_MAP_DEPTH_R32_tex%d", t);
			BANK_ONLY(grcTexture::SetCustomLoadName(szName);)
				grcTexture *pStagTex = grcTextureFactory::GetInstance().Create(HGHT_TARGET_SIZE, HGHT_TARGET_SIZE, grctfR32F, NULL, 1, &texParams);
			BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
			Assert(pStagTex);

			formatf(szName, "STAG_HEIGHT_MAP_DEPTH_%d", t);
			grcRenderTarget *pStagTarget = grcTextureFactory::GetInstance().CreateRenderTarget(szName, pStagTex->GetTexturePtr());
				
			m_stagingRenderTex[t]		= pStagTex;
			m_stagingRenderTargets[t]	= pStagTarget;
		}
	}else
	{
		for (int i=0; i<MAX_GPUS; ++i)
		{
			m_stagingRenderTex[i]		= NULL;
			m_stagingRenderTargets[i]	= NULL;
		}
	}
#endif //RAIN_UPDATE_CPU_HEIGHTMAP
}

CRenderPhaseHeight::~CRenderPhaseHeight()
{
	if (m_sRenderTarget) delete m_sRenderTarget;
	m_sRenderTargetRT = NULL; //is a duplicate of m_sRenderTarget

#if RAIN_UPDATE_CPU_HEIGHTMAP
	for (int i=0; i<MAX_GPUS; ++i)
	{
		if (m_stagingRenderTex[i])
			m_stagingRenderTex[i]->Release();
		if (m_stagingRenderTargets[i])
			delete m_stagingRenderTargets[i];
	}
#endif //RAIN_UPDATE_CPU_HEIGHTMAP
}

//PURPOSE quantizes a given value
static float quantizeToGrid( float v, float gridSize )
{
	return floorf(v/gridSize)*gridSize;
}

#if __BANK 

static void HeightMapShowRT_button()
{
	if (!CRenderTargets::IsShowingRenderTarget("HEIGHT_MAP_DEPTH"))
	{
		CRenderTargets::ShowRenderTarget("HEIGHT_MAP_DEPTH");
		CRenderTargets::ShowRenderTargetBrightness(1.0f);
		CRenderTargets::ShowRenderTargetNativeRes(true, true);
		CRenderTargets::ShowRenderTargetFullscreen(false);
		CRenderTargets::ShowRenderTargetInfo(false); // we're probably displaying model counts too, so don't clutter up the screen
	}
	else
	{
		CRenderTargets::ShowRenderTargetReset();
	}
}

template <int i> static void HeightMapShowRenderingStats_button()
{
	if (CPostScanDebug::GetCountModelsRendered())
	{
		CPostScanDebug::SetCountModelsRendered(false);
		gDrawListMgr->SetStatsHighlight(false);
	}
	else
	{
		CPostScanDebug::SetCountModelsRendered(true, DL_RENDERPHASE_RAIN_COLLISION_MAP);
		gDrawListMgr->SetStatsHighlight(true, "DL_RAIN_COLLISION_MAP", i == 0, i == 1, false);
		grcDebugDraw::SetDisplayDebugText(true);
	}
}

void CRenderPhaseHeight::AddWidgets(bkGroup &group)
{
	bkGroup& advanced = *group.AddGroup("Advanced", false);
	{
		advanced.AddToggle("Quantise To Grid", &m_quantiseToGrid);
		advanced.AddToggle("Render Animated Buildings In Heightmap", &m_renderAnimatedBuildingsInHeightMap);
		advanced.AddToggle("Render Vehicles In Heightmap", &m_renderVehiclesInHeightMap);
		advanced.AddToggle("Render Objects In Heightmap", &m_renderObjectsInHeightMap);
		advanced.AddToggle("Render Water In Heightmap", &m_renderWaterInHeightmap);
		advanced.AddToggle("Use Shadow Technique", &m_useShadowTechnique);
		advanced.AddToggle("Use Shadow Shader", &m_useShadowShader);
		advanced.AddButton("Show HEIGHT_MAP_DEPTH", HeightMapShowRT_button);
		advanced.AddButton("Show Rendering Stats", HeightMapShowRenderingStats_button<0>);
		advanced.AddButton("Show Context Stats", HeightMapShowRenderingStats_button<1>);
		
		advanced.AddSlider("Offset Shift X", &m_offsetShiftX, -5.0f, 5.0f, 0.001f);
		advanced.AddSlider("Offset Shift Y", &m_offsetShiftY, -5.0f, 5.0f, 0.001f);
	}

	bkGroup& debug = *group.AddGroup("Debug", false);
	{
		debug.AddToggle("Visualize Frustum", &m_VisualizeHeightmapFrustum);
		debug.AddToggle("Freeze Frustum", &m_FreezeHeightmapFrustum);
		debug.AddToggle("Skip Height When Exterior Not Visible", &m_bSkipHeightWhenExteriorNotVisible);
	}

	group.AddSlider("Extra emitter bottom height", &m_fExtraEmitterBottomHeight, 0.0f, 25.0f, 0.1f);
	group.AddSlider("Extra map bottom height", & m_fExtraBottomHeight, 0.0f, 25.0f, 0.1f);
}
#endif // __BANK

// stores current pointer of front target during RenderThread:
void CRenderPhaseHeight::SetRenderTargetRT(void *rp, void *t)
{
	FastAssert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	((CRenderPhaseHeight*)rp)->m_sRenderTargetRT = (grcRenderTarget*)t;
}

void CRenderPhaseHeight::BuildRenderList()
{
	if ( !m_renderFrame )  // aren't using it so why bother with generating it
	{
		return;
	}

	PF_FUNC(RenderPhaseHeightRL);

	SetIsRenderingHeightMap(true);

	COcclusion::BeforeSetupRender(false, false, SHADOWTYPE_NONE, true, GetGrcViewport());

	PF_START_TIMEBAR_DETAIL("ConstructRenderList");
	CRenderer::ConstructRenderListNew(GetRenderFlags(), this);
#if __BANK
	if(!m_FreezeHeightmapFrustum)
	{
		if(m_pVisualiseCamFrustum != NULL)
		{
			delete m_pVisualiseCamFrustum;
		}

		m_pVisualiseCamFrustum = rage_new CFrustumDebug(GetGrcViewport());
	}
#endif // __BANK

	COcclusion::AfterPreRender();

	SetIsRenderingHeightMap(false);

#if __BANK
	if(m_VisualizeHeightmapFrustum && m_pVisualiseCamFrustum)
	{
		CFrustumDebug::DebugRender(m_pVisualiseCamFrustum, Color32(0.0f,1.0f,0.0f,0.5f));
	}
#endif // __BANK
}

void CRenderPhaseHeight::UpdateViewport()
{
	CRenderPhaseScanned::UpdateViewport();

	if (!gVpMan.GetViewportStackDepth() || !gVpMan.GetCurrentViewport())
	{
		return;
	}

	grcViewport &viewport = GetGrcViewport();

	m_renderFrame = true;

	SetRenderFlags(
		RENDER_SETTING_RENDER_ALPHA_POLYS |
		RENDER_SETTING_RENDER_WATER |
		0);

	SetVisibilityType( VIS_PHASE_RAIN_COLLISION_MAP );
	GetEntityVisibilityMask().SetAllFlags( VIS_ENTITY_MASK_HILOD_BUILDING | VIS_ENTITY_MASK_LOLOD_BUILDING );

	if (BANK_SWITCH(m_renderVehiclesInHeightMap, true))
	{
		GetEntityVisibilityMask().SetFlag( VIS_ENTITY_MASK_VEHICLE );
	}

	//Setting this to true and disabling height map renderphase based on the properties of the object.
	//This will be handled by the object
	if (BANK_SWITCH(m_renderObjectsInHeightMap, true))
	{
		GetEntityVisibilityMask().SetFlag( VIS_ENTITY_MASK_OBJECT );
	}

	if(BANK_SWITCH(m_renderAnimatedBuildingsInHeightMap, true))
	{
		GetEntityVisibilityMask().SetFlag( VIS_ENTITY_MASK_ANIMATED_BUILDING );
	}

	// Don't bother with all that if tha GPU particles are off or the camera is underwater
	bool bShouldUpdate = g_vfxWeather.GetPtFxGPUManager().GetCurrLevel() > 0.0f && CVfxHelper::GetGameCamWaterDepth() <= 0.0f;
	if(bShouldUpdate)
	{
		// For the width, take the GPU particle emitters into account
		Vec3V emitterMin = RCC_VEC3V(camInterface::GetPos());
		Vec3V emitterMax = emitterMin;
		Mat44V_ConstRef camMatrix = viewport.GetCameraMtx44();

		g_vfxWeather.GetPtFxGPUManager().GetEmittersBounds(camMatrix, emitterMin, emitterMax);
		Vec3V center = (emitterMin + emitterMax) * ScalarV(V_HALF);
		Vector3 from = VEC3V_TO_VECTOR3(center);
		Vec3V size = (emitterMax - center);
		float width = Max(size.GetXf(), size.GetYf());
		Assertf(width > 0.0f, "Height map generated but GPU particles have zero size emitter");

		if (BANK_SWITCH(m_quantiseToGrid, true))
		{
			from.x = quantizeToGrid( from.x , (2.0f * width) / HGHT_TARGET_SIZE );
			from.y = quantizeToGrid( from.y , (2.0f * width) / HGHT_TARGET_SIZE );
		}


		// caching logic if position hasn't changed or rendering map hasn't changed don't draw it
		if ( from.x != m_lastCell.x || from.y != m_lastCell.y )
		{
			m_renderFrame = true;
		}
		m_lastCell = from;

		Vector3 to(from);

		// Try to find the optimal frustum range based on the world height information
		to.z = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(emitterMin.GetXf(), emitterMin.GetYf(), emitterMax.GetXf(), emitterMax.GetYf());
		from.z = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(emitterMin.GetXf(), emitterMin.GetYf(), emitterMax.GetXf(), emitterMax.GetYf());
		float fFrustumHeight = from.z - to.z;

		float emitterMinZ = emitterMin.GetZf() - BANK_SWITCH(m_fExtraEmitterBottomHeight, EXTRA_EMITTER_BOTTOM_HEIGHT);
		// If the world information is not available use the emitter values instead
		if(fFrustumHeight <= m_fNearPlane)
		{
			// Pushing the emitter minimum value down should help the ground particles not collide with the emitter when the player is in the air
			to.z = emitterMinZ;
			from.z = emitterMax.GetZf();
		}
		else
		{
			// Try to narrow the bottom of the range based on the emitter
			to.z = Max(to.z, emitterMinZ);

			// Make sure we have enough span above the player in case we are flying or the ground is low
			from.z = Max(from.z, emitterMax.GetZf());
		}

		to.z = to.z - BANK_SWITCH(m_fExtraBottomHeight, EXTRA_BOTTOM_HEIGHT); 

		fFrustumHeight = from.z - to.z; // Update the height based on the new values

		Matrix34 camera;
		camera.LookAt( from , to, YAXIS);
		viewport.SetCameraMtx(RCC_MAT34V(camera));
		viewport.Ortho( -width,  width, -width, width, m_fNearPlane, fFrustumHeight );

		viewport.SetWorldIdentity();

#if !(__D3D11 || RSG_ORBIS)
		Vector3 scale = Vector3( 0.5f/width, 0.5f/width, 1.0f/fFrustumHeight );
		Vector3 offset = Vector3( from ) - Vector3( m_offsetShiftX * width, m_offsetShiftY * width, fFrustumHeight);
		offset *= scale;
		offset = -offset;
#else 
		// Mimic the parallel projection in the camera matrix above then apply transformation into uv coordinate range.
		Vector3 scale = Vector3( 0.5f/width, 0.5f/width, -1.0f/(fFrustumHeight - m_fNearPlane) );
		Vector3 offset = Vector3( -scale.x*from.x + 0.5f, -scale.y*from.y + 0.5f, (from.z - m_fNearPlane)/(fFrustumHeight - m_fNearPlane) );
#endif

		int b=gRenderThreadInterface.GetUpdateBuffer();
		m_Scale[b] = scale;
		m_Offset[b] = offset;
	}

	if ( !bShouldUpdate )
	{
		m_renderFrame = false;
	}

	SetActive(m_renderFrame);
}

#if RAIN_UPDATE_CPU_HEIGHTMAP
//
// The main steps can be seen in the new function CopyDepthToStaging in RenderPhaseHeightMap.cpp
// PostFx::SimpleBlitWrapper is basically a copy shader - copy from the depthTexture to the R32 format.
// CopyFromGPUToStagingBuffer copies the data from the R32 rendertarget to the staging texture
// (which is internal to the R32 rendertarget object)
//
// The important part for stopping the readback stalls is that the MapStagingBufferToBackingStore
// happens frames later (which is why the index+1%MAX_GPUS).
// It was an approach that wouldn’t involve creating a depthFormat staging texture which might actually just not be possible.
//
void CRenderPhaseHeight::CopyDepthToStaging(void *rp)
{
	CRenderPhaseHeight* renderPhase = (CRenderPhaseHeight*)rp;

	const s32 currStagingTarget = renderPhase->m_currentStagingRenderTarget;
	const s32 maxStagingTargets = renderPhase->m_numStagingRenderTargets;

	if (!AssertVerify(currStagingTarget < maxStagingTargets))
	{
		return;
	}

	PostFX::SimpleBlitWrapper(renderPhase->m_stagingRenderTargets[currStagingTarget], renderPhase->m_sRenderTargetRT, pp_passthrough);

	(static_cast<grcTextureD3D11*>(renderPhase->m_stagingRenderTex[currStagingTarget]))->CopyFromGPUToStagingBuffer();

	(static_cast<grcTextureD3D11*>(renderPhase->m_stagingRenderTex[(currStagingTarget + 1) % maxStagingTargets]))->MapStagingBufferToBackingStore(true);

	(++renderPhase->m_currentStagingRenderTarget) %= maxStagingTargets;
}
#endif //RAIN_UPDATE_CPU_HEIGHTMAP

void CRenderPhaseHeight::BuildDrawList()
{
	if ( !m_renderFrame )  // aren't using it so why bother with generating it
	{
		return;
	}

	if(!HasEntityListIndex())
	{
		return;
	}

	DrawListPrologue();

	PF_FUNC(RenderPhaseHeight);

	DLC_Add(CShaderLib::SetFogRayTexture);

	if(fwScan::GetScanResults().GetIsExteriorVisibleInGbuf() BANK_ONLY( || !m_bSkipHeightWhenExteriorNotVisible))
	{
		DLCPushGPUTimebar(GT_VISUALEFFECTS, "Height Map");

		// RSX renders to m_sRenderTarget
		// SPU reads from m_sRenderBackTarget
		// both maps are swapped in CRenderThreadInterface::Synchronise()
		DLC_Add(CRenderPhaseHeight::SetRenderTargetRT,		this, m_sRenderTarget);

		// if rendering depth only we just need to lock the depth
		DLC(dlCmdLockRenderTarget, (0, NULL, m_sRenderTarget));
		DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));

		SetIsRenderingHeightMap(true);

		DLC(dlCmdSetStates, (grcStateBlock::RS_Default, grcStateBlock::DSS_Default, grcStateBlock::BS_Default));

		float clearDepth=1.0f;
		DLC(dlCmdClearRenderTarget, (false,Color32(0x00000000),true,clearDepth,true,0));

		if (BANK_SWITCH(m_renderWaterInHeightmap, true))
			Water::ProcessHeightMapPass();

		XENON_ONLY(DLC_Add(dlCmdEnableHiZ, true));

		if (BANK_SWITCH(m_useShadowShader, HEIGHTMAP_USE_SHADOW_SHADER))
		{
			// First draw the opaque stuff using the cascaded shadows shader
			DLC(dlCmdGrcLightStateSetEnabled, (false));
			DLC_Add(grmModel::SetForceShader, CRenderPhaseCascadeShadowsInterface::GetShadowShader());
			RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseHeight - opaque");
			CRenderListBuilder::AddToDrawListShadowEntities(GetEntityListIndex(), true, SUBPHASE_NONE, CRenderPhaseCascadeShadowsInterface::GetShadowTechniqueId(), -1, CRenderPhaseCascadeShadowsInterface::GetShadowTechniqueTextureId());
			RENDERLIST_DUMP_PASS_INFO_END();
			DLC_Add(grmModel::SetForceShader, (grmShader*)NULL);

			// Draw alpha stuff that is not supported by the shadow shader
			static const int techniqueId = -1;
			DLC(dlCmdShaderFxPushForcedTechnique, (techniqueId));
			RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseHeight - alpha");
			CRenderListBuilder::AddToDrawList(GetEntityListIndex(), CRenderListBuilder::RENDER_ALPHA|CRenderListBuilder::RENDER_DONT_RENDER_DECALS_CUTOUTS, XENON_SWITCH(CRenderListBuilder::USE_SHADOWS, CRenderListBuilder::USE_NO_LIGHTING));
			RENDERLIST_DUMP_PASS_INFO_END();
			DLC(dlCmdShaderFxPopForcedTechnique, ());
		}
		else
		{
			int techniqueId = -1;
			if (BANK_SWITCH(m_useShadowTechnique, HEIGHTMAP_USE_SHADOW_TECHNIQUE))
			{
				techniqueId = CRenderPhaseCascadeShadowsInterface::GetShadowTechniqueId();
			}
			else
			{
				// We need to set the shadow map RT, if not AMD freaks...
				DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars);
			}

			DLC(dlCmdShaderFxPushForcedTechnique, (techniqueId));

			RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseHeight");
			CRenderListBuilder::AddToDrawList(GetEntityListIndex(), CRenderListBuilder::RENDER_OPAQUE|CRenderListBuilder::RENDER_FADING|CRenderListBuilder::RENDER_ALPHA|CRenderListBuilder::RENDER_DONT_RENDER_PLANTS|CRenderListBuilder::RENDER_DONT_RENDER_TREES|CRenderListBuilder::RENDER_DONT_RENDER_DECALS_CUTOUTS, XENON_SWITCH(CRenderListBuilder::USE_SHADOWS, CRenderListBuilder::USE_NO_LIGHTING));
			RENDERLIST_DUMP_PASS_INFO_END();

			DLC(dlCmdShaderFxPopForcedTechnique, ());
		}

		XENON_ONLY(DLC_Add(dlCmdDisableHiZ, false));

		SetIsRenderingHeightMap(false);

		DLC(dlCmdSetCurrentViewportToNULL, ());

		DLC(dlCmdUnLockRenderTarget, (0));

#if RAIN_UPDATE_CPU_HEIGHTMAP
		if(ptxgpuBase::CpuRainUpdateEnabled())
		{
			DLC_Add(CRenderPhaseHeight::CopyDepthToStaging, this);
		}
#endif

		DLCPopGPUTimebar();
	}

	DrawListEpilogue();
}

u32 CRenderPhaseHeight::GetCullFlags() const
{
	static const u32 cullFlags = CULL_SHAPE_FLAG_RENDER_EXTERIOR | CULL_SHAPE_FLAG_HEIGHT_MAP;
	return BANK_SWITCH(m_bSkipHeightWhenExteriorNotVisible ? cullFlags : CULL_SHAPE_FLAG_RENDER_EXTERIOR, cullFlags);
}

