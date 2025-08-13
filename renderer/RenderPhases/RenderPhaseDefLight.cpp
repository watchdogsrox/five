//
// filename:	RenderPhaseDefLight.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "RenderPhaseDefLight.h"

// C headers

// Rage headers
#include "fwscene/scan/RenderPhaseCullShape.h"
#include "fwpheffects/ropedatamanager.h"
#include "fwdebug/picker.h"
#include "grcore/allocscope.h"

#include "system/param.h"
#include "grprofile/pix.h"
#include "profile/profiler.h"
#include "profile/timebars.h"
#if __XENON
	#include "grcore/device.h"
	#include "system/xtl.h"
	#if __D3D
		#include "system/d3d9.h"
	#endif
#endif
#if __PPU
	#include "grcore/wrapper_gcm.h"
	#if GCM_REPLAY
		#include "system/replay.h"
	#endif // GCM_REPLAY
#endif // __PPU
#if RSG_ORBIS
	#include "grcore/texturefactory_gnm.h"
	#include "grcore/texture_gnm.h"
	#include "grcore/rendertarget_gnm.h"
#endif
#include "grcore/stateblock.h"

// Framework headers
#include "fwpheffects/ropemanager.h"
#include "fwrenderer/renderlistgroup.h"

// Game headers
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportManager.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/Rendering/DebugRendering.h"
#include "debug/Rendering/DebugDeferred.h"
#include "debug/Rendering/DebugLighting.h"
#include "debug/Rendering/DebugLights.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "game/clock.h"
#include "game/weather.h"
#include "modelinfo/MloModelInfo.h"
#include "physics/physics.h"
#include "pickups/PickupManager.h"
#include "renderer/Deferred/DeferredConfig.h"
#include "renderer/Lights/AmbientLights.h"
#include "renderer/lights/lights.h"
#include "renderer/lights/TiledLighting.h"
#include "renderer/occlusion.h"
#include "renderer/OcclusionQueries.h"
#include "renderer/renderthread.h"
#include "renderer/renderListGroup.h"
#include "renderer/RenderListBuilder.h"
#include "renderer/shadows/shadows.h"
#include "renderer/water.h"
#include "renderer/SpuPm/SpuPmMgr.h"
#include "renderer/SSAO.h"
#include "renderer/PostProcessFX.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h" // TODO -- remove if we don't use the 'phase' switch
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Deferred/GBuffer.h"
#include "renderer/River.h"
#include "renderer/Util/ShaderUtil.h"
#include "scene/EntityIterator.h"
#include "scene/ExtraContent.h"
#include "scene/scene.h"
#include "scene/lod/LodScale.h"
#include "scene/portals/portalTracker.h"
#include "scene/portals/FrustumDebug.h"
#include "scene/portals/Portal.h"
#include "peds/ped.h"
#include "shaders/ShaderLib.h"
#include "system/TamperActions.h"
#include "Vfx/VisualEffects.h"
#include "vfx/misc/LODLightManager.h"
#include "Vfx/Decals/DecalManager.h"
#include "vfx/misc/Puddles.h"
#include "../../shader_source/Lighting/Shadows/cascadeshadows_common.fxh"

// --- Defines ------------------------------------------------------------------
#define DUMP_LIGHT_BUFFER	(0 && RSG_ORBIS && __DEV)


// --- Constants ----------------------------------------------------------------
const float	g_DefaultTanHalfFov = 0.414213562f;

// --- Globals ------------------------------------------------------------------
extern float MINIMUM_DISTANCE_FOR_PRESTREAM;
extern float MAX_DISTANCE_FOR_PRESTREAM;
extern float MAX_SPEED_FOR_PRESTREAM;

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------
#if __DEV
//OPTIMISATIONS_OFF()
#endif

// **************************************************
// **** DEFERRED LIGHTING SCENE TO GBUFFER PHASE ****
// **************************************************

void	SetViewPortWithWidget(const grcViewport *pVP)
{
	grcViewport::SetCurrent(pVP);
}

bool	scene_to_gbuffer=false;
Vector3	PreviousCameraPoint=Vector3(0,0,1);

#if __BANK
CFrustumDebug*	g_VisualiseCamFrustum = NULL;

void ShowCamFrustum(void)
{
	if(g_VisualiseCamFrustum != NULL)
	{
		CFrustumDebug::DebugRender(g_VisualiseCamFrustum, Color32(0.0f,1.0f,0.0f,0.5f));
	}
}
#endif

static void GetRopeInstanceAmbScale(ropeInstance* rope)
{
	Assert(rope);
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); 
	
	const CEntity* entity = CPhysics::GetEntityFromInst(rope->GetAttachedTo());
	if( !entity )
	{
		const CEntity* entityA = CPhysics::GetEntityFromInst(rope->GetInstanceA());
		bool dynamicA = entityA ? entityA->GetUseDynamicAmbientScale() : false;
		const CEntity* entityB = CPhysics::GetEntityFromInst(rope->GetInstanceB());
		bool dynamicB = entityB ? entityB->GetUseDynamicAmbientScale() : false;
		
		if( dynamicB && dynamicA )
		{
			// always favour peds and vehicles
			entity = (entityA->GetIsTypeVehicle() || entityA->GetIsTypePed()) ? entityA : entityB;
		}
		else if( dynamicB )
		{
			entity = entityB;
		}
		else if( dynamicA )
		{
			entity = entityA;
		}
		
	}

	u32 natAmbScale = 255;
	u32 artAmbScale = 255;
	
	if( entity )
	{	// Just reuse the entities ambient.
		natAmbScale = entity->GetNaturalAmbientScale();
		artAmbScale = entity->GetArtificialAmbientScale();
	}
	else
	{
		fwInteriorLocation intLoc = CPortalVisTracker::GetInteriorLocation();
		Vec3V ropePos = rope->GetWorldPositionA();
		Vec2V ambScale = g_timeCycle.CalcAmbientScale(ropePos, intLoc);
		
		const Vec2V scaledAmbScale = ambScale * ScalarV(255.0f);
		natAmbScale = u32(scaledAmbScale.GetXf());
		artAmbScale = u32(scaledAmbScale.GetYf());
	}
	
	rope->SetAmbScales(natAmbScale,artAmbScale);
}

static void SetRopeInstanceShaderVar(ropeInstance* rope)
{
	Assert(rope);
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); 
	// Set amb scale and all alpha to 255/false.
	CShaderLib::SetGlobals((u8)rope->GetNatAmbScale(), (u8)rope->GetArtAmbScale(), false, 255, false, false, 255, false, false, false);
}

static void DeferredCallback()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	// Rope render...
	grcStateBlock::SetBlendState(DeferredLighting::m_geometryPass_B);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

	if (DeferredLighting__m_useGeomPassStencil == false)
	{
		grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
	}
	else
	{
		grcStateBlock::SetDepthStencilState(DeferredLighting::m_geometryPass_DS, DEFERRED_MATERIAL_DEFAULT);
	}

	if(ropeDataManager::AreRopeTexturesLoaded())
	{
		CPhysics::GetRopeManager()->Draw(RCC_MATRIX34(grcViewport::GetCurrent()->GetCameraMtx()), g_LodScale.GetGlobalScaleForRenderThread(), SetRopeInstanceShaderVar);
	}

	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);

#if __D3D && __XENON
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILENABLE, FALSE);
	GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILWRITEENABLE, FALSE);
#endif

	// predicated depth resolve: it needs to happen before dynamic decals are rendered
	// so the depth for the *current* frame can be sampled.
#if __XENON
	D3DRECT pTilingRects[NUM_TILES];
	GBuffer::CalculateTiles(pTilingRects, NUM_TILES);

	PF_PUSH_TIMEBAR_DETAIL("Predicated Depth Resolve");
	{
		D3DBaseTexture* pDepthTexture  = static_cast<D3DBaseTexture*>(GBuffer::GetDepthTarget()->GetTexturePtr());

		for (int i=0; i<NUM_TILES; i++)
		{
			GRCDEVICE.GetCurrent()->SetPredication( D3DPRED_TILE( i ) );

			D3DPOINT dstPoint;
			dstPoint.x = pTilingRects[i].x1;
			dstPoint.y = pTilingRects[i].y1;

			GRCDEVICE.GetCurrent()->Resolve( 
				D3DRESOLVE_DEPTHSTENCIL | D3DRESOLVE_FRAGMENT0, 
				&pTilingRects[i], 
				pDepthTexture, 
				&dstPoint, 0, 0, 
				NULL, 0, 0, NULL );
		}

		GRCDEVICE.GetCurrent()->SetPredication( 0 );
	}
	PF_POP_TIMEBAR_DETAIL();
#endif

#if RSG_ORBIS
	// The depth buffer has to be decompressed for sampling if Htile is used. Option: decompress into a separate texture.
	GRCDEVICE.DecompressDepthSurface(NULL, false);
#endif

	PF_PUSH_TIMEBAR_DETAIL("Decals - Deferred Pass");
	const u32 prevMaterialID = CShaderLib::GetGlobalDeferredMaterial();
	g_decalMan.Render(DECAL_RENDER_PASS_VEHICLE_BADGE_LOCAL_PLAYER, DECAL_RENDER_PASS_MISC, false, true, false);
	CShaderLib::SetGlobalDeferredMaterial(prevMaterialID);
	PF_POP_TIMEBAR_DETAIL();
	
	PF_PUSH_TIMEBAR_DETAIL("Scaleform - RenderWorldSpace");
	CScaleformMgr::RenderWorldSpace();
	PF_POP_TIMEBAR_DETAIL();

	// predicated normal resolve: it needs to happen before ambient volumes 
	// to get normals for current frame too.
#if __XENON
	PF_PUSH_TIMEBAR_DETAIL("Predicated Normal Resolve");
	{
		grcRenderTarget **targets = GBuffer::GetTargets();
		D3DBaseTexture* pNormalTexture = static_cast<D3DTexture*>(targets[1]->GetTexturePtr());
		D3DVECTOR4 ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };

		for (int i=0; i<NUM_TILES; i++)
		{
			GRCDEVICE.GetCurrent()->SetPredication( D3DPRED_TILE( i ) );

			D3DPOINT dstPoint;
			dstPoint.x = pTilingRects[i].x1;
			dstPoint.y = pTilingRects[i].y1;

			GRCDEVICE.GetCurrent()->Resolve( 
				D3DRESOLVE_RENDERTARGET1 | D3DRESOLVE_FRAGMENT0 | D3DRESOLVE_CLEARRENDERTARGET, 
				&pTilingRects[i], 
				pNormalTexture,
				&dstPoint, 0, 0, 
				&ClearColor, 0.0f, 0L, NULL);
		}

		GRCDEVICE.GetCurrent()->SetPredication( 0 );
	}
	PF_POP_TIMEBAR_DETAIL();
#endif

#if DEVICE_EQAA
	// Disabling color decoding since ambient volumes render with color compression
	CShaderLib::SetMSAAParams( GRCDEVICE.GetMSAA(), false );
#endif

#if __BANK
	if (DebugLighting::m_drawAmbientVolumes)
#endif // __BANK
	{
		PF_PUSH_TIMEBAR_DETAIL("Ambient volumes");
		Lights::RenderAmbientScaleVolumes();
		PF_POP_TIMEBAR_DETAIL();
	}

#if DEVICE_EQAA
	// Resetting the color decoding
	CShaderLib::SetMSAAParams( GRCDEVICE.GetMSAA(), GRCDEVICE.IsEQAA() );
#endif
#if RSG_ORBIS
	static_cast<grcTextureFactoryGNM&>( grcTextureFactory::GetInstance() ).FinishRendering();
#endif
}

#if ENABLE_PIX_TAGGING  && __PPU && GCM_REPLAY
static void setMissingShaderVariable()
{
	IS_CAPTURING_REPLAY_DECL
	if( IS_CAPTURING_REPLAY )
	{
		// Just force this missing shader variable when doing a capture,
		// It's actually set up later on in the frame, and use the value from the previous frame,
		// But Gpad doesn't know that...
		CShaderLib::ResetAllVar();
	}
}
#endif // 

// *************************************************
// **** DEFERRED LIGHTING LIGHTS TO SCENE PHASE ****
// *************************************************

#if TERRAIN_TESSELLATION_SUPPORT

PARAM(useterraintessellation,"Turns on terrain tessellation.");

#include "../TerrainTessellation_parser.h"

bool CRenderPhaseDeferredLighting_SceneToGBuffer::ms_TerrainTessellationOn = true;
TerrainTessellationParameters CRenderPhaseDeferredLighting_SceneToGBuffer::ms_TerrainTessellationParams;

#endif // TERRAIN_TESSELLATION_SUPPORT

#if RSG_PC
CPortalVisTracker* CRenderPhaseDeferredLighting_SceneToGBuffer::ms_pPortalVisTrackerSavedDuringReset = NULL;
#endif

//
// CRenderPhaseDeferredLighting_SceneToGBuffer
//
// This renders geometry and material information to the deferred lighting buffer.
CRenderPhaseDeferredLighting_SceneToGBuffer::CRenderPhaseDeferredLighting_SceneToGBuffer(CViewport*	pGameViewport)
	: CRenderPhaseScanned(pGameViewport, "SceneToGBuf", DL_RENDERPHASE_GBUF, 0.0f
#if DRAW_LIST_MGR_SPLIT_GBUFFER
		, DL_RENDERPHASE_GBUF_FIRST, DL_RENDERPHASE_GBUF_LAST
#endif
	)
{
	SetRenderFlags(
		RENDER_SETTING_RENDER_WATER |
		RENDER_SETTING_RENDER_ALPHA_POLYS |
		RENDER_SETTING_RENDER_DECAL_POLYS |
		RENDER_SETTING_RENDER_CUTOUT_POLYS |
		RENDER_SETTING_ALLOW_PRERENDER);

	SetVisibilityType( VIS_PHASE_GBUF );

	// Set the default sort value.
#if __XENON
//	m_sortVal = RPS_RENDER_PRESCENE;
#else
//	m_sortVal = RPS_RENDER_SCENE;
#endif
//	m_drawListType = DL_RENDERPHASE_GBUF;

#if RSG_PC
	if (!ms_pPortalVisTrackerSavedDuringReset)
#endif
	{
		m_pPortalVisTracker = rage_new CPortalVisTracker();
		CPortalTracker::AddToActivatingTrackerList(m_pPortalVisTracker);
#if __BANK
		CPortalTracker::SetTraceVisTracker(m_pPortalVisTracker);
#endif
	}
#if RSG_PC
	else
	{
		m_pPortalVisTracker = ms_pPortalVisTrackerSavedDuringReset;
		ms_pPortalVisTrackerSavedDuringReset = NULL;
	}
#endif
	Assert(m_pPortalVisTracker);

	//m_pPortalVisTracker->SetViewport(pGameViewport);
	m_pPortalVisTracker->SetGrcViewport(&GetGrcViewport());
	m_pPortalVisTracker->SetLoadsCollisions(true);
	m_isUpdateTrackerPhase = true;
	m_isPrimaryTrackerPhase = true;

	SetEntityListIndex(gRenderListGroup.RegisterRenderPhase( RPTYPE_MAIN, this ));

#if __D3D11 || RSG_ORBIS
	//m_TessellationVar = grcEffect::LookupGlobalVar("gTessellationGlobal1");
#endif //_D3D11 || RSG_ORBIS

#if TERRAIN_TESSELLATION_SUPPORT
	if(PARAM_useterraintessellation.Get())
	{
		InitTerrainGlobals(32, 32, 32);
	}
#endif // TERRAIN_TESSELLATION_SUPPORT
}

CRenderPhaseDeferredLighting_SceneToGBuffer::~CRenderPhaseDeferredLighting_SceneToGBuffer()
{
#if RSG_PC
	if (m_pPortalVisTracker && !CRenderPhaseDeferredLighting_SceneToGBuffer::ms_pPortalVisTrackerSavedDuringReset)
#else
	if (m_pPortalVisTracker)
#endif
	{
		CPortalTracker::RemoveFromActivatingTrackerList(m_pPortalVisTracker);
		delete m_pPortalVisTracker;
		m_pPortalVisTracker = NULL;
	}
#if TERRAIN_TESSELLATION_SUPPORT
	if(PARAM_useterraintessellation.Get())
	{
		CleanUpTerrainGlobals();
	}
#endif // TERRAIN_TESSELLATION_SUPPORT
}

float CRenderPhaseDeferredLighting_SceneToGBuffer::ComputeLodDistanceScale() const
{
	const CViewport *pVp	= GetViewport();
	float fov				= pVp->GetFrame().GetFov();

	// Allow me to explain... FOV has an influence on LOD scale.
	// As the FOV angle gets smaller, so we see further into the distance, thus the LOD scale is set to be a higher value,
	// This has the effect of scaling the distances entities use as their LOD trigger point, 
	// so they get triggered to be into high detail at a further distance or sooner.

	Assert(fov >= g_MinFov);
	Assert(fov <= g_MaxFov);

	float fovRads			=	fov * DtoR;
	float lodScaleDueToFov	=	g_DefaultTanHalfFov / rage::Tanf(fovRads/2.0f);
	lodScaleDueToFov		=	Max(lodScaleDueToFov,1.0f); // no smaller than an arbitrary smallish value!... although technically it should inside the renderer it doesn't seem to like it. NB. presnetly limited so wider FOVs have no effect on LODscale.
	float finalLODScale		=	lodScaleDueToFov;

	Assert(finalLODScale > 0.01f);
	Assert(finalLODScale<10000.0f);

	return finalLODScale;
}

#if RSG_PC
void CRenderPhaseDeferredLighting_SceneToGBuffer::StorePortalVisTrackerForReset()
{
	ms_pPortalVisTrackerSavedDuringReset = GetPortalVisTracker();
}
#endif

void CRenderPhaseDeferredLighting_SceneToGBuffer::UpdateViewport()
{
	SetActive(true);

	CRenderPhaseScanned::UpdateViewport();

	CViewport *pVp = GetViewport();

	const Vector3& cameraPosition = pVp->GetFrame().GetPosition();

	if (m_pPortalVisTracker)
	{
		if (m_pPortalVisTracker->GetInteriorLocation().IsValid())
		{
			m_pPortalVisTracker->m_portalsSafeSphere.SetRadiusf(0.0f);
		}

		const CEntity* pAttachEntity = pVp->GetAttachEntity();
		bool bFirstPerson = pAttachEntity && pAttachEntity->GetIsTypePed() && static_cast<const CPed*>(pAttachEntity)->IsFirstPersonShooterModeEnabledForPlayer(false);
		entitySelectID targetEntityID = pAttachEntity? CEntityIDHelper::ComputeEntityID(pAttachEntity) : 0;
		
		if (pVp->GetAttachEntity() && (!bFirstPerson || m_pPortalVisTracker->ForceUpdateUsingTarget() || targetEntityID != m_pPortalVisTracker->GetLastTargetEntityId() )) {
			m_pPortalVisTracker->UpdateUsingTarget(pVp->GetAttachEntity(), cameraPosition);
		} else {
			m_pPortalVisTracker->Update(cameraPosition);
		}

		m_pPortalVisTracker->SetLastTargetEntityId(targetEntityID);
	}

	// the primary render phase holds the tracker defining the interior for this viewport.
	// copy the data about this interior back into the viewport
	if (IsPrimaryPortalVisTrackerPhase()){
		CPortalVisTracker* pPVT = GetPortalVisTracker();
		Assert(pPVT);

		pVp->SetInteriorData(pPVT->m_pInteriorProxy, pPVT->m_roomIdx);

		CPortalVisTracker::StorePrimaryData(pPVT->m_pInteriorProxy, pPVT->m_roomIdx);		// required by timecycle much later...
	}
}

void CRenderPhaseDeferredLighting_SceneToGBuffer::BuildRenderList()
{
	Vector3	CameraPoint=VEC3V_TO_VECTOR3(GetGrcViewport().GetCameraMtx().GetCol2());
	float cos_of_camera_rot=Min(CameraPoint.Dot(PreviousCameraPoint),1.0f);
	cos_of_camera_rot=Max(cos_of_camera_rot,-1.0f);

	scene_to_gbuffer=true; //temp hack while we try out across frame persistence

	COcclusion::BeforeSetupRender(false, true, SHADOWTYPE_NONE, true, GetGrcViewport());
	CScene::SetupRender(GetEntityListIndex(), GetRenderFlags(), GetGrcViewport(), this);

	if (!CPortal::ms_bIsWaterReflectionDisabled)
	{
		PF_PUSH_TIMEBAR_DETAIL("Water PreRender");
		Water::PreRender(GetGrcViewport()); // We don't need occluders so we can do this after CRL
		PF_POP_TIMEBAR_DETAIL();
	}
	//else
	//{
	//	Water::ClearRender(GetEntityListIndex());
	//}

#if __BANK
		if(!g_debug_heightmap)
		{
			if(g_VisualiseCamFrustum == NULL)
			{
				g_VisualiseCamFrustum = rage_new CFrustumDebug(GetGrcViewport());
			}
			else
			{
				g_VisualiseCamFrustum->Set(GetGrcViewport());
			}
		}
#endif

	COcclusion::AfterPreRender();
	scene_to_gbuffer=false;
	PreviousCameraPoint=CameraPoint;
}

void CRenderPhaseDeferredLighting_SceneToGBuffer::ConstructRenderList()
{
	CRenderPhaseScanned::ConstructRenderList();

	const grcViewport& viewport = GetGrcViewport();

	// Vec3V vecCameraPosition = viewport.GetCameraPosition();
	Vec3V vecCameraDir = -viewport.GetCameraMtx().GetCol2();

	Vector3 vCamPosForFade = GetCameraPositionForFade();
	Vector3 prevCameraPosition = CRenderer::GetThisCameraPosition();
	CRenderer::SetPrevCameraPosition(prevCameraPosition);
	CRenderer::SetThisCameraPosition(vCamPosForFade);

	Vector3 delta=vCamPosForFade-prevCameraPosition;

	float VelocityInDirection=delta.Dot(RCC_VECTOR3(vecCameraDir))*fwTimer::GetInvTimeStep();
	if(fabsf(VelocityInDirection-CRenderer::GetPrevCameraVelocity())<1.0f) //gradual change of velocity so no sudden camera cuts
	{
		CRenderer::SetPreStreamDistance(rage::Min(1.0f, fabsf(VelocityInDirection/MAX_SPEED_FOR_PRESTREAM) )*MAX_DISTANCE_FOR_PRESTREAM+MINIMUM_DISTANCE_FOR_PRESTREAM);
	}
	CRenderer::SetPrevCameraVelocity(VelocityInDirection);
}


u32 CRenderPhaseDeferredLighting_SceneToGBuffer::GetCullFlags() const
{
	u32 flags =
		CULL_SHAPE_FLAG_PRIMARY					|
		CULL_SHAPE_FLAG_RENDER_EXTERIOR			|
		CULL_SHAPE_FLAG_RENDER_INTERIOR			|
		CULL_SHAPE_FLAG_TRAVERSE_PORTALS		|
		CULL_SHAPE_FLAG_CLIP_AND_STORE_PORTALS	|
		CULL_SHAPE_FLAG_USE_SCREEN_QUAD_PAIRS	|
		CULL_SHAPE_FLAG_ENABLE_OCCLUSION		|
		CULL_SHAPE_FLAG_STREAM_ENTITIES;

	CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
	const int roomIdx = CPortalVisTracker::GetPrimaryRoomIdx();

	if (pIntInst && roomIdx != INTLOC_INVALID_INDEX)
	{
		CMloModelInfo* pMloModelInfo = static_cast<CMloModelInfo*>(pIntInst->GetBaseModelInfo());

		// [HACK GTAV] hack for BS#1527956, mirror floor in v_franklins
		if (pMloModelInfo)
		{
			if (stricmp(pMloModelInfo->GetRooms()[roomIdx].m_name.c_str(), "V_57_DiningRM") == 0 ||
				stricmp(pMloModelInfo->GetRooms()[roomIdx].m_name.c_str(), "V_57_FrontRM") == 0 ||
				stricmp(pMloModelInfo->GetRooms()[roomIdx].m_name.c_str(), "V_57_KitchRM") == 0) // added for BS#1566206
			{
				flags |= CULL_SHAPE_FLAG_IGNORE_VERTICAL_MIRRORS;
			}
			else if (stricmp(pMloModelInfo->GetRooms()[roomIdx].m_name.c_str(), "V_57_FrnkRM") == 0) // added for BS#1559580
			{
				flags |= CULL_SHAPE_FLAG_IGNORE_HORIZONTAL_MIRRORS;
			}
		}
	}

	return flags;
}

#if __PS3

#define SCENE_CAM_DIST_FAR_FROM_ORIGIN_DEFAULT 1100.0f

#if __BANK
static float s_sceneCamDistFarFromOrigin           = SCENE_CAM_DIST_FAR_FROM_ORIGIN_DEFAULT;
static bool  s_sceneEdgeNoPixelTestDisabledForPeds = true;
#endif // __BANK

static void SetEdgeNoPixelTestEnable_render(u8 enable)
{
#if SPU_GCM_FIFO
	SPU_SIMPLE_COMMAND(grcGeometryJobs__SetEdgeNoPixelTestEnable, enable);
#endif // SPU_GCM_FIFO
}

static bool g_EdgeNoPixelTestEnabled = true;

#if __BANK
bool IsEdgeNoPixelTestDisabledForPeds()
{
	return s_sceneEdgeNoPixelTestDisabledForPeds;
}
#endif // __BANK

bool SetEdgeNoPixelTestEnabled(bool bEnabled)
{
	const bool bWasEnabled = g_EdgeNoPixelTestEnabled;
	const u8 enable = bEnabled ? (u8)0x01 : (u8)0x00;
	DLC_Add(SetEdgeNoPixelTestEnable_render, enable);
	g_EdgeNoPixelTestEnabled = bEnabled;
	return bWasEnabled;
}

bool SetEdgeNoPixelTestDisableIfFarFromOrigin_Begin(const grcViewport* viewport)
{
	const Vec3V camPos = viewport->GetCameraPosition();
	const float camDistance = MaxElement(Abs(camPos.GetXY())).Getf();
	const bool bCamDistFarFromOrigin = (camDistance >= BANK_SWITCH(s_sceneCamDistFarFromOrigin, SCENE_CAM_DIST_FAR_FROM_ORIGIN_DEFAULT));

	if (bCamDistFarFromOrigin)
	{
		SetEdgeNoPixelTestEnabled(false);
	}

	return bCamDistFarFromOrigin;
}

void SetEdgeNoPixelTestDisableIfFarFromOrigin_End(bool bCamDistFarFromOrigin)
{
	if (bCamDistFarFromOrigin)
	{
		SetEdgeNoPixelTestEnabled(true);
	}
}

#endif // __PS3

#if __BANK
void CRenderPhaseDeferredLighting_SceneToGBuffer::AddWidgets(bkGroup& bank)
{
	(void)&bank;

#if __PS3
	bank.AddSlider("Cam Dist Far From Origin", &s_sceneCamDistFarFromOrigin, 0.0f, 8000.0f, 1.0f);
	bank.AddToggle("No-Pixel Test Disabled For Peds", &s_sceneEdgeNoPixelTestDisabledForPeds);
#endif // __PS3

}
#endif // __BANK

#if __BANK
// enable/disable shader's pass renderstate:
static void EnableShaderRS(bool bEnable)
{
	grcEffect_Technique_Pass::ms_bEnableShaderRS = bEnable;
}
#endif //__BANK...

#if !DRAW_LIST_MGR_SPLIT_GBUFFER

#define CLEAR_ALL_TARGETS	(__DEV && 0)
#if CLEAR_ALL_TARGETS
static void ClearAllTargets()
{
	grcTextureFactory &factory = grcTextureFactory::GetInstance();
	factory.LockRenderTarget(0,CRenderTargets::GetBackBuffer(),NULL);
	GRCDEVICE.Clear( true,Color32(0x00000000),false,false,false,0);
	factory.UnlockRenderTarget(0);
	factory.LockRenderTarget(0,factory.GetBackBuffer(),NULL);
	GRCDEVICE.Clear( true,Color32(0x00000000),false,false,false,0);
	factory.UnlockRenderTarget(0);
}
#endif	// CLEAR_ALL_TARGETS

// construct a suitable drawlist for this renderphase
void CRenderPhaseDeferredLighting_SceneToGBuffer::BuildDrawList()
{
	if(!HasEntityListIndex())
		return;

	//CRenderListBuilder::SetBackfaceCullMode(grccmBack);
#if MULTIPLE_RENDER_THREADS
	CViewportManager::DLCRenderPhaseDrawInit();
#endif

	DrawListPrologue();
	//Informing GBuffer that we're going to render into it
	DLC_Add(GBuffer::SetRenderingTo, true);
	PPU_ONLY(CBubbleFences::WaitForBubbleFenceDLC(CBubbleFences::BFENCE_LIGHTING_END));

	DLCPushTimebar("Setup");

#if ENABLE_PIX_TAGGING && __PPU && GCM_REPLAY
	DLC_Add(setMissingShaderVariable);
#endif // ENABLE_PIX_TAGGING && GCM_REPLAY

#if CLEAR_ALL_TARGETS
	DLC_Add(ClearAllTargets);
#endif

#if !__XENON // Gbuffer0 is cleared inside of PrepareGeometryPassTiled on Xenon
	if (PostFX::UseSubSampledAlpha())
	{
		DLCPushMarker("SSA Clear");
		DLC_Add(GBuffer::ClearFirstTarget);
		DLCPopMarker();
	}
#endif // !__XENON
#if DEVICE_MSAA
	{
		DLCPushMarker("GBufferClear");
		DLC_Add(GBuffer::ClearAllTargets, true);
		DLCPopMarker();
	}
#endif // DEVICE_MSAA

#if __BANK
	// Always clear it, just in case.
	if (DebugDeferred::m_GBufferIndex > 0)
	{
		DLC_Add(GBuffer::ClearAllTargets, false);
	}
#endif // __BANK

	XENON_ONLY( DLC_Add( dlCmdEnableHiZ, false ) );

	DLC_Add(Lights::SetupDirectionalGlobals, 1.0f);
	DLC_Add(CShaderLib::SetGlobalInInterior, false);
	// HACK - Talk to Klaas before changing 
#if __PPU
	DLC_Add(GBuffer::IncrementAttached, sysBootManager::GetChrome());
#endif // __PPU
	// HACK - Talk to Klaas before changing 

#if SUPPORT_INVERTED_PROJECTION
	grcViewport viewport = GetGrcViewport();
	viewport.SetInvertZInProjectionMatrix( true );
#else
	const grcViewport& viewport = GetGrcViewport();
#endif

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
# if TERRAIN_TESSELLATION_SUPPORT
	if(PARAM_useterraintessellation.Get())
	{
		SetTerrainGlobals(viewport);
	}
# endif // TERRAIN_TESSELLATION_SUPPORT

	CRenderer::SetTessellationVars( viewport, (float)VideoResManager::GetSceneWidth()/(float)VideoResManager::GetSceneHeight() );
#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES

# if !__FINAL
	CRenderer::SetTerrainTintGlobals();
#endif // !__FINAL

#if !__XENON
	DLC_Add(GBuffer::LockTargets);
#endif // !__XENON

	int renderListIdx = GetEntityListIndex();

	DLC( dlCmdSetCurrentViewport, (&viewport));

	// Make sure that the standard lights don't try to set shader vars
	// when doing deferred lighting.
	DLC_Add(CShaderLib::UpdateGlobalGameScreenSize);
	DLC_Add(CShaderLib::ApplyDayNightBalance);
	DLC_Add(Lights::CalculateDynamicBakeBoostAndWetness);

	DLCPopTimebar();

#if __XENON	
	// Render geometry and material information into the GBuffer.
	DLC_XENON(dlCmdSetShaderGPRAlloc, (128-CGPRCounts::ms_GBufferPsRegs,CGPRCounts::ms_GBufferPsRegs));
    DLC_Add(DeferredLighting::PrepareGeometryPassTiled);
#else // __XENON
	DLC_Add(DeferredLighting::PrepareGeometryPass);
#endif // __XENON

	// Is this necessary? If so... why?
	XENON_ONLY( DLC_Add( dlCmdEnableAutomaticHiZ, true ) );

#if USE_INVERTED_PROJECTION_ONLY
	const float clearDepth=0.0f;
#else
	const float clearDepth=1.0f;
#endif

	//This is so I can get proper stencil culling of the sky without having to do a refresh stencil cull
	PS3_ONLY(DLC_Add(CRenderTargets::ResetStencilCull, false, DEFERRED_MATERIAL_CLEAR, 0xff, false, false));
	DLC ( dlCmdClearRenderTarget, (false,Color32(0x00000000),true,clearDepth,true,DEFERRED_MATERIAL_CLEAR));

	if ( COcclusion::WantToPrimeZBufferWithOccluders())
	{
		DLCPushMarker("PrimeZBuffer");
		PS3_ONLY( DLC_Add( GBuffer::BeginPrimeZBuffer) );
		DLC_Add( COcclusion::PrimeZBuffer);
		PS3_ONLY( DLC_Add( GBuffer::EndPrimeZBuffer) );
		DLCPopMarker();
	}

	Water::ProcessDepthOccluderPass();

#if __BANK
	bool bRevertEnableShaderRS=false;
	const ePickerShowHideMode pickerMode = g_PickerManager.GetShowHideMode(); 
	if((pickerMode==PICKER_ISOLATE_SELECTED_ENTITY) || (pickerMode==PICKER_ISOLATE_SELECTED_ENTITY_NOALPHA) || (pickerMode==PICKER_ISOLATE_ENTITIES_IN_LIST))
	{
		bRevertEnableShaderRS = true;
		DLC_Add(EnableShaderRS, false);
	}
#endif //__BANK...

	PS3_ONLY(const bool bCamDistFarFromOrigin = SetEdgeNoPixelTestDisableIfFarFromOrigin_Begin(&viewport));
	RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseDeferredLighting_SceneToGBuffer - opaque");
	CRenderListBuilder::AddToDrawList(renderListIdx, CRenderListBuilder::RENDER_OPAQUE|CRenderListBuilder::RENDER_FADING, CRenderListBuilder::USE_DEFERRED_LIGHTING);
	RENDERLIST_DUMP_PASS_INFO_END();
	PS3_ONLY(SetEdgeNoPixelTestDisableIfFarFromOrigin_End(bCamDistFarFromOrigin));

#if __BANK
	if(bRevertEnableShaderRS)
	{
		bRevertEnableShaderRS = false;
		DLC_Add(EnableShaderRS, true);
	}

	DLC_Add(CRenderPhaseDebugOverlayInterface::StencilMaskOverlayRender, 0);
	DLC_Add(DebugDeferred::RenderMacbethChart);
#endif // __BANK

	Lights::SetupRenderThreadLights();
	CPhysics::GetRopeManager()->Apply(GetRopeInstanceAmbScale);

	if(ropeDataManager::AreRopeTexturesLoaded())
	{
		gDrawListMgr->AddTxdReference(ropeDataManager::GetTxdSlot());
	}

	DLC_Add( DeferredCallback );

#if __XENON
	const bool bDelayedGbuffer2TileResolve = CRenderPhaseCascadeShadowsInterface::IsShadowActive() && CRenderer::UseDelayedGbuffer2TileResolve();
	DLC_Add(DeferredLighting::FinishGeometryPassTiled, bDelayedGbuffer2TileResolve);
	DLC( dlCmdSetShaderGPRAlloc, (0, 0) );
#else // __XENON
	// COMMENT
	DLC_Add(GBuffer::UnlockTargets);
	DLC_Add(DeferredLighting::FinishGeometryPass);
#endif // !__XENON

	DLCPushTimebar("Finalise");

#if TILED_LIGHTING_ENABLED
	DLC_Add(CTiledLighting::DepthDownsample);
#endif // TILED_LIGHTING_ENABLED

#if MSAA_EDGE_PASS
	const bool isInterrior = g_SceneToGBufferPhaseNew && 
		g_SceneToGBufferPhaseNew->GetPortalVisTracker() && 
		g_SceneToGBufferPhaseNew->GetPortalVisTracker()->GetInteriorLocation().IsValid();
	DLC_Add(DeferredLighting::ExecuteEdgeMarkPass, (isInterrior));
#endif //MSAA_EDGE_PASS

#if __XENON
	DLC( dlCmdSetCurrentViewport, (&viewport));
#endif

#if SPUPMMGR_TEST
	DLC_Add(SpuPM_ProcessTestTexture);
#endif

	DLC( dlCmdSetCurrentViewportToNULL, ());
	
	XENON_ONLY( DLC_Add( dlCmdDisableHiZ, false ) );

#if DYNAMIC_GB
	{
		DLC_Add(GBuffer::FlushAltGBuffer,GBUFFER_RT_0);

		// Only copy out GBuffers 1 & 3 if they truly need to be evicted
		if ( BANK_SWITCH( !DebugDeferred::m_useLateESRAMGBufferEviction, false ) )
		{
			DLC_Add(GBuffer::FlushAltGBuffer,GBUFFER_RT_1);
			DLC_Add(GBuffer::FlushAltGBuffer,GBUFFER_RT_3);
		}

	}

#endif
	DLCPopTimebar();

	//Informing GBuffer that we're stopping the render into it
	DLC_Add(GBuffer::SetRenderingTo, false);
	DrawListEpilogue();
}

#else // !DRAW_LIST_MGR_SPLIT_GBUFFER

// TODO:- Move into rag maybe.
int g_SceneToGBuffer_BatchSizeDivisor = 16;
int g_SceneToGBuffer_SubPassCount = 16;

// construct a suitable drawlist for this renderphase
void CRenderPhaseDeferredLighting_SceneToGBuffer::BuildDrawList()
{
	if(!HasEntityListIndex())
		return;

	if(ropeDataManager::AreRopeTexturesLoaded())
	{
		gDrawListMgr->AddTxdReference(ropeDataManager::GetTxdSlot());
	}

	// Place in GPU commands to start timer.
	DrawListPrologue(DL_RENDERPHASE_GBUF_FIRST);
	DrawListEpilogue(DL_RENDERPHASE_GBUF_FIRST);

	StartDrawListOfType(DL_RENDERPHASE_GBUF_000);

#if USE_INVERTED_PROJECTION_ONLY
	const float clearDepth=0.0f;
#else
	const float clearDepth=1.0f;
#endif

	//This is so I can get proper stencil culling of the sky without having to do a refresh stencil cull
	DLC ( dlCmdClearRenderTarget, (false,Color32(0x00000000),true,clearDepth,true,DEFERRED_MATERIAL_CLEAR));

	if ( COcclusion::WantToPrimeZBufferWithOccluders())
	{
		DLCPushMarker("PrimeZBuffer");
		PS3_ONLY( DLC_Add( GBuffer::BeginPrimeZBuffer) );
		DLC_Add( COcclusion::PrimeZBuffer);
		PS3_ONLY( DLC_Add( GBuffer::EndPrimeZBuffer) );
		DLCPopMarker();
	}

	Water::ProcessDepthOccluderPass();

	int renderListIdx = GetEntityListIndex();
	int noOfPasses = CRenderListBuilder::GetOpaqueDrawListAddPassRowCount();

	// Compute a suitable batch sized based upon the total number of entities.
	int batchSize = CRenderListBuilder::GetEntityListSize(renderListIdx)/g_SceneToGBuffer_BatchSizeDivisor;

	s32 currentBatchSize = 0;
	bool drawListOpen = true;
	int drawListIndex = DL_RENDERPHASE_GBUF_000;
	BANK_ONLY(bool bRevertEnableShaderRS=false);

	// Visit each row in the fwDrawListAndPass (see for example, s_OpaquePassSSA, in RenderListBuilder.cpp).
	for(int pass=0; pass<noOfPasses; pass++)
	{
		// Visit each "portion" of the row.
		for(int subPass=0; subPass<g_SceneToGBuffer_SubPassCount; subPass++)
		{
			// Open a draw list if we don`t have one already.
			if(!drawListOpen)
			{
				drawListOpen = true;
				currentBatchSize = 0;
				StartDrawListOfType(drawListIndex);

#if __BANK
				const ePickerShowHideMode pickerMode = g_PickerManager.GetShowHideMode(); 
				if((pickerMode==PICKER_ISOLATE_SELECTED_ENTITY) || (pickerMode==PICKER_ISOLATE_SELECTED_ENTITY_NOALPHA) || (pickerMode==PICKER_ISOLATE_ENTITIES_IN_LIST))
				{
					bRevertEnableShaderRS = true;
					DLC_Add(EnableShaderRS, false);
				}
#endif //__BANK...
			}

			RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseDeferredLighting_SceneToGBuffer - opaque visible");
			currentBatchSize += CRenderListBuilder::AddToDrawList_Opaque(pass, renderListIdx, CRenderListBuilder::RENDER_OPAQUE|CRenderListBuilder::RENDER_FADING, CRenderListBuilder::USE_DEFERRED_LIGHTING, subPass, g_SceneToGBuffer_SubPassCount);
			RENDERLIST_DUMP_PASS_INFO_END();

			// Close the current draw list once we have enough entities, but not if we`ve used all available drawlists.
			if((currentBatchSize >= batchSize) && (drawListIndex != DL_RENDERPHASE_GBUF_113))
			{
#if __BANK
				if(bRevertEnableShaderRS)
				{
					bRevertEnableShaderRS = false;
					DLC_Add(EnableShaderRS, true);
				}
#endif //__BANK...

				EndDrawListOfType(drawListIndex);
				drawListOpen = false;
				currentBatchSize = 0;
				// Move onto the next draw list.
				drawListIndex++;
			}
		}
	}

#if __BANK
	if(bRevertEnableShaderRS)
	{
		bRevertEnableShaderRS = false;
		DLC_Add(EnableShaderRS, true);
	}
#endif //__BANK...

	if(!drawListOpen)
		StartDrawListOfType(drawListIndex);

	RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseDeferredLighting_SceneToGBuffer - decals and plants");
	CRenderListBuilder::AddToDrawList_DecalsAndPlants(renderListIdx, CRenderListBuilder::RENDER_OPAQUE|CRenderListBuilder::RENDER_FADING, CRenderListBuilder::USE_DEFERRED_LIGHTING, 0, 1);
	RENDERLIST_DUMP_PASS_INFO_END();

	// Close the final draw list.
	EndDrawListOfType(drawListIndex);

	//--------------------------------------------------------------------------------------------------//

	StartDrawListOfType(DL_RENDERPHASE_GBUF_AMBT_LIGHTS);

#if __BANK
	DLC_Add(CRenderPhaseDebugOverlayInterface::StencilMaskOverlayRender, 0);
	DLC_Add(DebugDeferred::RenderMacbethChart);
#endif // __BANK

	Lights::SetupRenderThreadLights();
	CPhysics::GetRopeManager()->Apply(GetRopeInstanceAmbScale);
	DLC_Add( DeferredCallback );

	EndDrawListOfType(DL_RENDERPHASE_GBUF_AMBT_LIGHTS);

	// Place in GPU commands to stop timer.
	DrawListPrologue(DL_RENDERPHASE_GBUF_LAST);
#if DYNAMIC_GB
	{
		DLC_Add(GBuffer::FlushAltGBuffer,GBUFFER_RT_0);

		// Only copy out GBuffers 1 & 3 if they truly need to be evicted
		if ( BANK_SWITCH( !DebugDeferred::m_useLateESRAMGBufferEviction, false ) )
		{
			DLC_Add(GBuffer::FlushAltGBuffer,GBUFFER_RT_1);
			DLC_Add(GBuffer::FlushAltGBuffer,GBUFFER_RT_3);
		}
	}
#endif

#if TILED_LIGHTING_ENABLED
	DLC_Add(CTiledLighting::DepthDownsample);
#endif // TILED_LIGHTING_ENABLED

#if MSAA_EDGE_PASS
	DLC_Add(DeferredLighting::ExecuteEdgeMarkPass);
#endif //MSAA_EDGE_PASS

	DrawListEpilogue(DL_RENDERPHASE_GBUF_LAST);
}


void CRenderPhaseDeferredLighting_SceneToGBuffer::StartDrawListOfType(int drawListType)
{
	DrawListPrologue(drawListType);
	CViewportManager::DLCRenderPhaseDrawInit();
	//Informing GBuffer that we're going to render into it
	DLC_Add(GBuffer::SetRenderingTo, true);

#if DEVICE_MSAA
	if (drawListType == DL_RENDERPHASE_GBUF_000)
	{
		DLCPushMarker("GBufferClear");
		DLC_Add(GBuffer::ClearAllTargets, true);
		DLCPopMarker();
	}else
#endif // DEVICE_MSAA
	// Clear the 1st g-buffer target during the 1st DL.
	if(PostFX::UseSubSampledAlpha() && drawListType == DL_RENDERPHASE_GBUF_000)
	{
		DLCPushMarker("SSA Clear");
		DLC_Add(GBuffer::ClearFirstTarget);
		DLCPopMarker();
	}

	DLCPushTimebar("Setup");

#if ENABLE_PIX_TAGGING && __PPU && GCM_REPLAY
	DLC_Add(setMissingShaderVariable);
#endif // ENABLE_PIX_TAGGING && GCM_REPLAY

	DLC_Add(Lights::SetupDirectionalGlobals, 1.0f);
	DLC_Add(CShaderLib::SetGlobalInInterior, false);

#if SUPPORT_INVERTED_PROJECTION
	grcViewport viewport = GetGrcViewport();
	viewport.SetInvertZInProjectionMatrix( true );
#else
	const grcViewport& viewport = GetGrcViewport();
#endif

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
# if TERRAIN_TESSELLATION_SUPPORT
	if(PARAM_useterraintessellation.Get())
	{
		SetTerrainGlobals(viewport);
	}
# endif // TERRAIN_TESSELLATION_SUPPORT

	CRenderer::SetTessellationVars( viewport, (float)VideoResManager::GetSceneWidth()/(float)VideoResManager::GetSceneHeight() );
#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES

# if !__FINAL
	CRenderer::SetTerrainTintGlobals();
#endif // !__FINAL

	DLC_Add(GBuffer::LockTargets);
	DLC( dlCmdSetCurrentViewport, (&viewport));

	// Make sure that the standard lights don't try to set shader vars
	// when doing deferred lighting.
	DLC_Add(CShaderLib::UpdateGlobalGameScreenSize);
	DLC_Add(CShaderLib::ApplyDayNightBalance);
	DLC_Add(Lights::CalculateDynamicBakeBoostAndWetness);
	DLC_Add(Lights::SetupDirectionalAndAmbientGlobals);

	DLCPopTimebar();

	DLC_Add(DeferredLighting::PrepareGeometryPass);
}


void CRenderPhaseDeferredLighting_SceneToGBuffer::EndDrawListOfType(int drawListType)
{
	DLC_Add(GBuffer::UnlockTargets);
	DLC_Add(DeferredLighting::FinishGeometryPass);
	DLC(dlCmdSetCurrentViewportToNULL, ());
	DLC_Add(CShaderLib::ResetAllVar);

	//Informing GBuffer that we're stopping the render into it
	DLC_Add(GBuffer::SetRenderingTo, false);
	DrawListEpilogue(drawListType);
}

#endif // !DRAW_LIST_MGR_SPLIT_GBUFFER

#if TERRAIN_TESSELLATION_SUPPORT

void CRenderPhaseDeferredLighting_SceneToGBuffer::InitTerrainGlobals(u32 width, u32 height, u32 depth)
{
	m_TerrainTessellation0Var = grcEffect::LookupGlobalVar("gTerrainGlobal0");
	m_TerrainNearAndFarRampVar = grcEffect::LookupGlobalVar("gTerrainNearAndFarRamp");
	m_TerrainNearNoiseAmplitudeVar = grcEffect::LookupGlobalVar("gTerrainNearNoiseAmplitude");
	m_TerrainFarNoiseAmplitudeVar = grcEffect::LookupGlobalVar("gTerrainFarNoiseAmplitude");
	m_TerrainGlobalBlendAndProjectionInfoVar = grcEffect::LookupGlobalVar("gTerrainGlobalBlendAndProjectionInfo");
	m_TerrainCameraWEqualsZeroPlaneVar = grcEffect::LookupGlobalVar("gTerrainCameraWEqualsZeroPlane");
	m_TerrainWModificationVar = grcEffect::LookupGlobalVar("gTerrainWModification");
	m_TerrainPerlinNoiseTextureVar = grcEffect::LookupGlobalVar("TerrainNoiseTex");
	m_TerrainPerlinNoiseTextureParamVar = grcEffect::LookupGlobalVar("gTerrainNoiseTexParam");

	sysMemStartTemp();
	float *pMem = rage_new float[width*height*depth*4];
	sysMemEndTemp();

	// DX11 TODO:- Use a more efficient texture format for the perlin noise gradients.
	float *pDest = pMem;

	for(u32 i=0; i<depth; i++)
	{
		for(u32 j=0; j<height; j++)
		{
			for(u32 k=0; k<width; k++)
			{
				Vector3 N = Vector3(fwRandom::GetRandomNumberInRange(-1.0f, 1.0f), fwRandom::GetRandomNumberInRange(-1.0f, 1.0f), fwRandom::GetRandomNumberInRange(-1.0f, 1.0f));
				N.Normalize();

				*pDest++ = N.x;
				*pDest++ = N.y;
				*pDest++ = N.z;
				*pDest++ = 0.0f;
			}
		}
	}

	m_pTerrainPerlinNoiseTexture = grcTextureFactory::GetInstance().Create(width, height, depth, grctfA32B32G32R32F, (void *)pMem);

	sysMemStartTemp();
	delete [] pMem;
	sysMemEndTemp();

	LoadTerrainTessellationData();
}


void CRenderPhaseDeferredLighting_SceneToGBuffer::CleanUpTerrainGlobals()
{
	if(m_pTerrainPerlinNoiseTexture)
	{
		m_pTerrainPerlinNoiseTexture->Release();
		m_pTerrainPerlinNoiseTexture = NULL;
	}
}


void CRenderPhaseDeferredLighting_SceneToGBuffer::SetTerrainGlobals(const grcViewport &viewport)
{
	Vec4V TerrainGlobal0((float)ms_TerrainTessellationParams.m_PixelsPerVertex, (float)ms_TerrainTessellationParams.m_WorldTexelUnit, ms_TerrainTessellationParams.m_Envelope.x, ms_TerrainTessellationParams.m_Envelope.w);

	if(ms_TerrainTessellationOn == false)
	{
		// Negative pixels per vertex mean don`t do tessellation.
		TerrainGlobal0.SetX(-1.0f);
	}

	float A, B, C, D;
	// Map from envelope[0], envelope[1]->[0, 1], "near" ramp...
	ComputeMapping(ms_TerrainTessellationParams.m_Envelope.x, ms_TerrainTessellationParams.m_Envelope.y, 0.0f, 1.0f, A, B);
	// ...And from envelope[2], envelope[3]->[1, 0], "far" ramp.
	ComputeMapping(ms_TerrainTessellationParams.m_Envelope.z, ms_TerrainTessellationParams.m_Envelope.w, 1.0f, 0.0f, C, D);
	Vec4V TerrainNearAndFarRamps(A, B, C, D); 

	// Set up near noise min and max values (plus mapping from [-1, 1] to [min, max])...
	ComputeMapping(-1.0f, 1.0f, ms_TerrainTessellationParams.m_NearNoise.x, ms_TerrainTessellationParams.m_NearNoise.y, A, B);
	Vec4V TerrainNearNoiseAmplitude(ms_TerrainTessellationParams.m_NearNoise.x, ms_TerrainTessellationParams.m_NearNoise.y, A, B);
	// ...and far also
	ComputeMapping(-1.0f, 1.0f, ms_TerrainTessellationParams.m_FarNoise.x, ms_TerrainTessellationParams.m_FarNoise.y, A, B);
	Vec4V TerrainFarNoiseAmplitude(ms_TerrainTessellationParams.m_FarNoise.x, ms_TerrainTessellationParams.m_FarNoise.y, A, B);

	// Form the global ramp/blend (to blend near/far noise amplitudes) plus collect projection info.
	ComputeMapping(ms_TerrainTessellationParams.m_Envelope.x, ms_TerrainTessellationParams.m_Envelope.w, 0.0f, 1.0f, A, B);
	Vec4V TerrainGlobalBlendAndProjectionInfo(A, B, 2.0f*viewport.GetTanHFOV(), (float)grcDevice::GetWidth());

	// Store camera info.
	Mat44V_ConstRef cameraMatrix = viewport.GetCameraMtx44();
	Vec4V cameraPosition = cameraMatrix.GetCol3();
	Vec4V cameraZAxis = -cameraMatrix.GetCol2();
	Vec4V wEqualsZeroPlane = cameraZAxis - Dot(cameraPosition, cameraZAxis)*Vec4V(V_ZERO_WONE);

	// Form the W modification profile.
	ComputeMapping(ms_TerrainTessellationParams.m_UseNearWEnd, ms_TerrainTessellationParams.m_Envelope.w, ms_TerrainTessellationParams.m_Envelope.x, ms_TerrainTessellationParams.m_Envelope.w, A, B);
	Vec4V TerrainWModification(A, B, 0.0f, 0.0f);
	
	Vec4V TerrainPerlinTextureSize(ScalarV(1.0f)); 

	if(m_pTerrainPerlinNoiseTexture)
	{
		TerrainPerlinTextureSize = Vec4V((float)m_pTerrainPerlinNoiseTexture->GetWidth(), (float)m_pTerrainPerlinNoiseTexture->GetHeight(), (float)m_pTerrainPerlinNoiseTexture->GetDepth(), 0.0f); 
	}

	DLC_SET_GLOBAL_VAR(m_TerrainTessellation0Var, TerrainGlobal0);
	DLC_SET_GLOBAL_VAR(m_TerrainNearAndFarRampVar, TerrainNearAndFarRamps);
	DLC_SET_GLOBAL_VAR(m_TerrainNearNoiseAmplitudeVar, TerrainNearNoiseAmplitude);
	DLC_SET_GLOBAL_VAR(m_TerrainFarNoiseAmplitudeVar, TerrainFarNoiseAmplitude);
	DLC_SET_GLOBAL_VAR(m_TerrainGlobalBlendAndProjectionInfoVar, TerrainGlobalBlendAndProjectionInfo);
	DLC_SET_GLOBAL_VAR(m_TerrainCameraWEqualsZeroPlaneVar, wEqualsZeroPlane);
	DLC_SET_GLOBAL_VAR(m_TerrainWModificationVar, TerrainWModification);
	DLC_SET_GLOBAL_VAR(m_TerrainPerlinNoiseTextureVar, m_pTerrainPerlinNoiseTexture);
	DLC_SET_GLOBAL_VAR(m_TerrainPerlinNoiseTextureParamVar, TerrainPerlinTextureSize); 
}


void CRenderPhaseDeferredLighting_SceneToGBuffer::LoadTerrainTessellationData()
{
	PARSER.LoadObject("platform:/data/TerrainTessellation", "xml", ms_TerrainTessellationParams);
}

#if __BANK

void CRenderPhaseDeferredLighting_SceneToGBuffer::AddTerrainWidgets(bkBank &bank)
{
	bank.PushGroup("Terrain tessellation");
	bank.AddSlider("Terrain pixels per vertex", &CRenderPhaseDeferredLighting_SceneToGBuffer::ms_TerrainTessellationParams.m_PixelsPerVertex, 1, 1024, 1 );
	bank.AddSlider("Terrain noise frequency", &CRenderPhaseDeferredLighting_SceneToGBuffer::ms_TerrainTessellationParams.m_WorldTexelUnit, 0.1f, 10.0f, 0.05f );
	bank.AddVector("Terrain near noise", &CRenderPhaseDeferredLighting_SceneToGBuffer::ms_TerrainTessellationParams.m_NearNoise, -3.0f, 3.0f, 0.005f );
	bank.AddVector("Terrain far noise", &CRenderPhaseDeferredLighting_SceneToGBuffer::ms_TerrainTessellationParams.m_FarNoise, -3.0f, 3.0f, 0.005f );
	bank.AddVector("Terrain ramp envelope", &CRenderPhaseDeferredLighting_SceneToGBuffer::ms_TerrainTessellationParams.m_Envelope, 0.0f, 2000.0f, 0.5f );
	bank.AddSlider("Terrain use near W", &CRenderPhaseDeferredLighting_SceneToGBuffer::ms_TerrainTessellationParams.m_UseNearWEnd, 0.1f, 2000.0f, 0.5f );
	bank.AddToggle("Terrain tessellation on/off", &CRenderPhaseDeferredLighting_SceneToGBuffer::ms_TerrainTessellationOn);

	bank.AddButton("Load", CRenderPhaseDeferredLighting_SceneToGBuffer::LoadTerrainTessellationData);
	bank.AddButton("Save", CRenderPhaseDeferredLighting_SceneToGBuffer::SaveTerrainTessellationData);
	bank.PopGroup();
}


void CRenderPhaseDeferredLighting_SceneToGBuffer::SaveTerrainTessellationData()
{
	PARSER.SaveObject("platform:/data/TerrainTessellation", "xml", &ms_TerrainTessellationParams, parManager::XML);
}

#endif //__BANK

#endif // TERRAIN_TESSELLATION_SUPPORT


// *************************************************
// **** DEFERRED LIGHTING LIGHTS TO SCENE PHASE ****
// *************************************************

//
// CRenderPhaseDeferredLighting_LightsToScreen
//
// This renders to the viewport using the deferred lighting buffer as shader params.
CRenderPhaseDeferredLighting_LightsToScreen::CRenderPhaseDeferredLighting_LightsToScreen(CViewport* pGameViewport)
: CRenderPhase(pGameViewport, "DefLight_LightsToScene", DL_RENDERPHASE_LIGHTING, 0.0f, true)
{
}

void CRenderPhaseDeferredLighting_LightsToScreen::BuildRenderList()
{
}

#if DUMP_LIGHT_BUFFER && RSG_ORBIS
void DumpLightBuffer(bool resolve)
{
	static bool done = false;
	if (!done && grcSetupInstance && GRCDEVICE.GetMSAA())
	{
		done = true;
		grcSetupInstance->TakeRenderTargetShotsNow();
		if (resolve)
		{
			CRenderTargets::GetBackBufferCopy(true);
		}else
		{
			CRenderTargets::LockSceneRenderTargets();
			CRenderTargets::UnlockSceneRenderTargets();
		}
		grcSetupInstance->UntakeRenderTargetShots();
	}
}
#endif // DUMP_LIGHT_BUFFER && RSG_ORBIS

//----------------------------------------------------
// build and execute the drawlist for this renderphase
void CRenderPhaseDeferredLighting_LightsToScreen::BuildDrawList()											
{
	if (!GetViewport()->IsActive())
	{
		return;
	}

	DrawListPrologue();

#if RSG_PC
	DLC_Add(CShaderLib::SetStereoTexture);
#endif

#if DYNAMIC_GB
	{
		DLC_Add(GBuffer::FlushAltGBuffer,GBUFFER_RT_2);
	}
#endif
	grcViewport* pVP = &GetGrcViewport();
#if SUPPORT_INVERTED_PROJECTION
	pVP->SetInvertZInProjectionMatrix( true );
#endif

	PPU_ONLY(CBubbleFences::InsertBubbleFenceDLC(CBubbleFences::BFENCE_LIGHTING_START));

	DLC_Add(CShaderLib::ResetAllVar);

	DLC(dlCmdSetCurrentViewport, (pVP));
	
	DLC_XENON(dlCmdSetShaderGPRAlloc, (128-CGPRCounts::ms_DefLightingPsRegs,CGPRCounts::ms_DefLightingPsRegs));
	
#if __XENON
	DLC_Add(DeferredLighting::RestoreDepthStencil);

	if ( CRenderPhaseCascadeShadowsInterface::IsShadowActive())
	{
		DLC_Add(CRenderPhaseCascadeShadowsInterface::LateShadowReveal);
	}	
#endif // __XENON



#if __BANK
	DLC_Add(DebugDeferred::GBufferOverride);

	if (DebugDeferred::m_GBufferIndex > 0)
	{
		DLC_Add(CRenderTargets::LockUIRenderTargets);
			DLC_Add(DebugDeferred::ShowGBuffer, (GBufferRT)(DebugDeferred::m_GBufferIndex - 1));
		DLC_Add(CRenderTargets::UnlockUIRenderTargets, false);

		PPU_ONLY(CBubbleFences::InsertBubbleFenceDLC(CBubbleFences::BFENCE_LIGHTING_END));
		DrawListEpilogue();
		return;
	}
#endif // __BANK

	DLC_Add(DeferredLighting::DefaultState);

#if __XENON
	DLC_Add(PostFX::SetLDR8bitHDR10bit);
#elif __PS3
	DLC_Add(PostFX::SetLDR8bitHDR16bit);
#else
	DLC_Add(PostFX::SetLDR16bitHDR16bit);
#endif

	// Do SSAO before clearing scene render targets (we can use ESRAM where buffer resides).
#if __BANK
	if (DebugLighting::m_drawAmbientOcclusion)
#endif // __BANK
	{
#if !DETAIL_SSAO_GPU_TIMEBARS
		DLCPushGPUTimebar(GT_SSAO, "SSAO");
#endif
		DLCPushTimebar("SSAO");
		DLC(dlCmdSetCurrentViewport, (pVP));
		bool bCalculateSSAO	= true;
#if SPUPMMGR_PS3
		if(SSAO::SpuEnabledUT())
		{
			bCalculateSSAO	= false;	// SSAO SPU: apply only
		}
#endif
		DLC_Add(SSAO::CalculateAndApply, bCalculateSSAO, false);
		DLCPopTimebar();
#if !DETAIL_SSAO_GPU_TIMEBARS
		DLCPopGPUTimebar();
#endif
	}


	DLC_Add(CRenderTargets::LockSceneRenderTargets, true);
	DLC_Add(CShaderLib::UpdateGlobalDevScreenSize);

#if __BANK
	const bool isolatingCutscenePed = (CutSceneManager::GetInstance()->IsPlaying() && CutSceneManager::GetInstance()->IsIsolating());
	
	if(isolatingCutscenePed)
		DLC(dlCmdClearRenderTarget, (true, Color32(0.0f, 1.0f, 0.0f, 0.0f), false, 1.0f, false, 0));
	else
#endif
		DLC(dlCmdClearRenderTarget, (true, Color32(0), false, 1.0f, false, 0));

	Lights::SetupRenderThreadLights();

	DLC_Add(Lights::SetupDirectionalAndAmbientGlobals);

	DLC_Add(CRenderPhaseCascadeShadowsInterface::SetDeferredLightingShaderVars);
	DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars);

#if !__FINAL
	DLC_Add(DeferredLighting::SetDebugLightingParams);
#endif // !__FINAL

#if __D3D11
	if (!GRCDEVICE.IsReadOnlyDepthAllowed())
	{
		DLC_Add(CRenderTargets::CopyDepthBufferCmd);
	}
#endif // __D3D11

	DLC_Add(CRenderTargets::UnlockSceneRenderTargets);

#if RSG_PC
	//	Bind depth copy for sampling
	DLC_Add(DeferredLighting::SetShaderGBufferTargets, false);
#else
	DLC_Add(DeferredLighting::SetShaderGBufferTargets);
#endif

	//set HDR exp bias for 7e3 format, this doesn't work now since it's not the buffer we're locking
   	//but it's set to 0 and only tunable from RAG, reimplement this if needed 
	//XENON_ONLY(DLC_Add(GRCDEVICE.SetColorExpBiasNow,CRenderer::GetBackbufferColorExpBiasHDR()));

	// Make it snow
	const u32 xmasWeather = g_weather.GetTypeIndex("XMAS");
	const bool isXmasWeather = (g_weather.GetPrevTypeIndex() == xmasWeather) || (g_weather.GetNextTypeIndex() == xmasWeather);
	const bool isUnderwater = Water::IsCameraUnderwater();
	const bool isForcedSnow = false
#if __BANK
		|| CRenderer::ms_isSnowing
#endif
#if TAMPERACTIONS_ENABLED && TAMPERACTIONS_SNOW
		|| TamperActions::IsSnowing()
#endif
		;

	if ((((CNetwork::IsGameInProgress() REPLAY_ONLY(|| CReplayMgr::AllowXmasSnow())) && isXmasWeather) || isForcedSnow) && !isUnderwater)
	{
		DLC_Add(DeferredLighting::ExecuteSnowPass);
	}

	// Bind after SSAO to refresh texture slot 1
	DLC_Add(CRenderPhaseReflection::SetReflectionMap);
	
	// Go on with the rest of the stuff
	DLC_Add(CRenderTargets::LockSceneRenderTargets_DepthCopy);

	// Draw the directional and ambient lights.
	DLCPushGPUTimebar(GT_DIRECTIONAL, "Directional/Ambient/Reflections");
	DLC_Add(DeferredLighting::ExecuteAmbientAndDirectionalPass, true, false);
	DLCPopGPUTimebar();

#if RSG_PC
	if (GRCDEVICE.GetDxFeatureLevel() <= 1000)	
	{
		DLC_Add(CRenderTargets::UnlockSceneRenderTargets);
		DLC_Add(CRenderTargets::LockSceneRenderTargets, true);
	}
#endif

	DLCPushGPUTimebar(GT_FOLIAGE_LIGHTING, "Foliage lighting");
	DLC_Add(DeferredLighting::ExecuteFoliageLightPass);
	DLCPopGPUTimebar();

#if MSAA_EDGE_PASS
	if (DeferredLighting::IsEdgePassActive() && !DeferredLighting::IsEdgePassActiveForSceneLights())
		DLC_Add(DeferredLighting::ExecuteEdgeClearPass);
#endif //MSAA_EDGE_PASS

	DLCPushGPUTimebar(GT_AMBIENT_LIGHTS, "Ambient Lights");
	DLCPushTimebar("Ambient Lights");
	DLC_Add(AmbientLights::Render);
	DLCPopTimebar();
	DLCPopGPUTimebar();

#if RSG_PC
	if (GRCDEVICE.GetDxFeatureLevel() <= 1000)	
	{
		DLC_Add(CRenderTargets::UnlockSceneRenderTargets);
		DLC_Add(CRenderTargets::LockSceneRenderTargets_DepthCopy);
		DLC_Add(DeferredLighting::SetShaderGBufferTargets, false);
	}
#endif

	// draw the point lights etc.
	DLCPushGPUTimebar(GT_LOD_LIGHTS, "LOD Lights");
	DLCPushTimebar("LOD Lights");
	DLC_Add(CLODLights::RenderLODLights);
	DLCPopTimebar();
	DLCPopGPUTimebar();

	DLCPushGPUTimebar(GT_OCC_QUERIES, "Occlusion Queries");
	DLCPushTimebar("Occlusion Queries");
	OcclusionQueries::RenderBoxBasedQueries(); // We do those here : no custom shaders, no tiled queries. all in all : better.
	DLCPopTimebar();
	DLCPopGPUTimebar();

#if RSG_DURANGO
	DLC_Add(CRenderTargets::UnlockSceneRenderTargets);
	DLC_Add(CRenderTargets::LockSceneRenderTargets, true);
#endif

	DLCPushGPUTimebar_Budgeted(GT_SCENE_LIGHTS, "Scene Lights", 5.0f);
	DLCPushTimebar("Scene Lights");
		Lights::AddToDrawListSceneLights(true);
	DLCPopTimebar();
	DLCPopGPUTimebar();

#if RSG_DURANGO
	DLC_Add(CRenderTargets::UnlockSceneRenderTargets);
	DLC_Add(CRenderTargets::LockSceneRenderTargets_DepthCopy);
#endif

#if MSAA_EDGE_PASS
	// no need to clear it here really, since nothing else is using SPAREOR1 stencil bit
	if (DeferredLighting::IsEdgePassActiveForSceneLights() && false)
		DLC_Add(DeferredLighting::ExecuteEdgeClearPass);
#endif //MSAA_EDGE_PASS

	if(g_weather.GetWetnessNoWaterCameraCheck() > 0.0f && PuddlePassSingleton::InstanceRef().IsEnabled())
	{
		DLC_Add(DeferredLighting::ExecutePuddlePass);
	}

#if PED_SKIN_PASS
	DLCPushGPUTimebar(GT_SKIN_LIGHTING, "Skin lighting");
	DLC_Add(DeferredLighting::SetProjectionShaderParams, DEFERRED_SHADER_LIGHTING, MM_DEFAULT);
	DLC_Add(DeferredLighting::ExecuteSkinLightPass, (grcRenderTarget*)NULL, true);
	DLCPopGPUTimebar();
#endif // PED_SKIN_PASS

	DLC_Add(CRenderTargets::UnlockSceneRenderTargets);

	XENON_ONLY(DLC ( dlCmdSetShaderGPRAlloc, (0, 0)));

#if RSG_ORBIS && DUMP_LIGHT_BUFFER
	DLC_Add(DumpLightBuffer, true);
#endif

	PPU_ONLY(CBubbleFences::InsertBubbleFenceDLC(CBubbleFences::BFENCE_LIGHTING_END));

	// Make sure we are back to the standard default state.
	DLC_Add(DeferredLighting::DefaultExitState);

	DrawListEpilogue();
}


#if SPUPMMGR_PS3
//
//
// CRenderPhaseDeferredLighting_LightsToScreenSPU
//
// This renders to the viewport using the deferred lighting buffer as shader params.
CRenderPhaseDeferredLighting_SSAOSPU::CRenderPhaseDeferredLighting_SSAOSPU(CViewport* pGameViewport)
: CRenderPhase(pGameViewport, "DefLight_SSAOSPU", DL_RENDERPHASE_LIGHTING, 0.0f, true)
{
}

//----------------------------------------------------
// build and execute the drawlist for this renderphase
void CRenderPhaseDeferredLighting_SSAOSPU::BuildDrawList()											
{
	if (!GetViewport()->IsActive())
	{
		return;
	}

	if(!SSAO::SpuEnabledUT())
		return;


	DrawListPrologue();

	grcViewport* pVP = &GetGrcViewport();

	DLCPushGPUTimebar_Budgeted(GT_SSAO, "SSAO SPU", 3.0f);
	DLCPushTimebar("SSAO SPU");
		DLC(dlCmdSetCurrentViewport, (pVP));
		DLC_Add(SSAO::CalculateAndApply, true, false);	// SSAO SPU: calculate only
	DLCPopTimebar();
	DLCPopGPUTimebar();

	// Make sure we are back to the standard default state.
	DLC_Add(DeferredLighting::DefaultExitState);

	DrawListEpilogue();
}
#endif //SPUPMMGR_PS3...
