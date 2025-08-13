// ======================================================
// renderer/renderphases/renderphaseparaboloidshadows.cpp
// (c) 2010 RockstarNorth
// ======================================================

// Rage headers
#include "spatialdata/sphere.h"
#include "fwdrawlist/drawlistmgr.h"
#include "fwrenderer/renderlistgroup.h"
#include "fwscene/scan/RenderPhaseCullShape.h"
#include "profile/profiler.h"
#include "profile/timebars.h"
#include "grcore/debugdraw.h"

// Game headers
#include "camera/viewports/viewport.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "scene/scene.h"
#include "scene/Entity.h"
#include "scene/DynamicEntity.h"
#include "vfx/visualeffects.h"
#include "renderer/util/util.h"
#include "renderer/occlusion.h"
#include "renderer/renderlistgroup.h"
#include "renderer/renderlistbuilder.h"
#include "renderer/renderphases/renderphaseparaboloidshadows.h"
#include "renderer/shadows/shadows.h"
#include "renderer/lights/LightSource.h"
#include "renderer/lights/lights.h"

RENDER_OPTIMISATIONS()

// ================================================================================================

EXT_PF_GROUP(BuildRenderList);
PF_TIMER(CRenderPhaseParaboloidShadowRL,BuildRenderList);

EXT_PF_GROUP(RenderPhase);
PF_TIMER(CRenderPhaseParaboloidShadow,RenderPhase);

Mat44V CRenderPhaseParaboloidShadow::sm_CurrentFacetFrustumLRTB;

void CRenderPhaseParaboloidShadow::AddToDrawListParaboloidShadow(s32 list, int s, int side, bool drawDynamic, bool drawStatic, bool isFaceted) // entities that should not cast shadows are already culled in SetupRenderShadowMap()
{
	PF_PUSH_TIMEBAR_DETAIL("Render Shadow Map Scene");
	PF_START_TIMEBAR_DETAIL("Render Entities");

	fwRenderSubphase phase;
	if (!drawDynamic)
		phase = SUBPHASE_PARABOLOID_STATIC_ONLY;
	else if (!drawStatic)
		phase = SUBPHASE_PARABOLOID_DYNAMIC_ONLY;
	else
		phase = SUBPHASE_PARABOLOID_STATIC_AND_DYNAMIC;

	if (isFaceted) // we need to save the viewport if it's faceted, so we can do some extra culling
		sm_CurrentFacetFrustumLRTB = CParaboloidShadow::GetFacetFrustumLRTB(s, side);
	
	DLC_Add(CParaboloidShadow::BeginShadowZPass, s, side);

	CRenderListBuilder::AddToDrawListShadowEntities(list, false, phase,  CParaboloidShadow::m_ParaboloidDepthPointTexGroupId, -1,
																		 CParaboloidShadow::m_ParaboloidDepthPointTexTextureGroupId, -1,
																		 (isFaceted) ? ShadowEntityCullCheck : NULL);
	DLC_Add(CParaboloidShadow::EndShadowZPass);
	
	PF_POP_TIMEBAR_DETAIL();
}

CRenderPhaseParaboloidShadow::CRenderPhaseParaboloidShadow(CViewport* pGameViewport, int s)
	: CRenderPhaseScanned(pGameViewport, "Paraboloid Shadows", DL_RENDERPHASE_PARABOLOID_SHADOWS, 0.0f
#if DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS
		, DL_RENDERPHASE_PARABOLOID_SHADOWS_0 + s, DL_RENDERPHASE_PARABOLOID_SHADOWS_0 + s
#endif
	)
{
	m_disableOnPause = false;
	m_paraboloidShadowIndex = s;

	SetRenderFlags(RENDER_SETTING_RENDER_CUTOUT_POLYS); // so we get hair...
	SetEntityListIndex(gRenderListGroup.RegisterRenderPhase( RPTYPE_SHADOW, this ));
}

#if __BANK
void DebugPlaneLine(Vec3V_In pos, Vec3V_In dir, Vec3V_In planeNormal, ScalarV_In radius)
{
	Vec3V normal = Normalize(planeNormal);
	// draw a line on the plane in the "direction" of the light dir
	Vec3V tangent = Normalize(Cross(dir, normal));
	Vec3V lineDir = Cross(normal, tangent);
	grcDebugDraw::Line(pos, pos + lineDir*radius, Color32(1.0f, 0.0f, 0.0f, 1.0f));
}
#endif

spdTransposedPlaneSet8 GenerateConeFrustum(Vec3V_In pos, Vec3V_In dir, Vec3V_In tangent, ScalarV_In tanHalfAngle, ScalarV BANK_ONLY(radius))
{
	Assert(Abs<float>(MagSquared(dir).Getf() - 1.0f) <= 0.01f);
	Assert(Abs<float>(MagSquared(tangent).Getf() - 1.0f) <= 0.01f);
	Assert(Abs<float>(Dot(dir, tangent).Getf()) <= 0.01f);
	
	Mat34V viewMatrix;
	LookAt(viewMatrix, Vec3V(V_ZERO), -dir, tangent);

	ScalarV sqrtHalf = ScalarV(M_SQRT1_2);

	const Vec4V planes[8] = 
	{
		BuildPlane(pos, Transform(viewMatrix, Normalize(Vec3V(ScalarV(V_ONE), ScalarV(V_ZERO), tanHalfAngle)))),
		BuildPlane(pos, Transform(viewMatrix, Normalize(Vec3V(ScalarV(V_ZERO), ScalarV(V_ONE), tanHalfAngle)))),
		BuildPlane(pos, Transform(viewMatrix, Normalize(Vec3V(ScalarV(V_NEGONE), ScalarV(V_ZERO),tanHalfAngle)))),
		BuildPlane(pos, Transform(viewMatrix, Normalize(Vec3V(ScalarV(V_ZERO),  ScalarV(V_NEGONE),tanHalfAngle)))),

		BuildPlane(pos, Transform(viewMatrix, Normalize(Vec3V(sqrtHalf, sqrtHalf, tanHalfAngle)))),
		BuildPlane(pos, Transform(viewMatrix, Normalize(Vec3V(-sqrtHalf, sqrtHalf, tanHalfAngle)))),
		BuildPlane(pos, Transform(viewMatrix, Normalize(Vec3V(sqrtHalf, -sqrtHalf, tanHalfAngle)))),
		BuildPlane(pos, Transform(viewMatrix, Normalize(Vec3V(-sqrtHalf, -sqrtHalf, tanHalfAngle)))),
	};

#if __BANK
	if (CParaboloidShadow::m_debugParaboloidShadowRenderCullShape)
	{
		for (int i = 0; i < 8; i++)
			DebugPlaneLine(pos, dir, planes[i].GetXYZ(), radius);
	}
#endif

	spdTransposedPlaneSet8 tps;
	tps.SetPlanes(planes);
	return tps;
}


void CRenderPhaseParaboloidShadow::SetCullShape(fwRenderPhaseCullShape & cullShape)
{
	s32			warpShadowIndex = m_paraboloidShadowIndex;
	const Vec3V pos = RCC_VEC3V(CParaboloidShadow::m_activeEntries[ warpShadowIndex ].m_pos);

	eLightShadowType lightShadowType = CParaboloidShadow::m_activeEntries[ warpShadowIndex ].m_info.m_type;


	//Using a tighter cull shape in a frustum shape for spot lights
	if((lightShadowType == SHADOW_TYPE_SPOT || lightShadowType == SHADOW_TYPE_HEMISPHERE) BANK_ONLY( && CParaboloidShadow::m_debugParaboloidShadowTightSpotCullShapeEnable))
	{
		//Add additional angle and radius for spot light frustum
		const ScalarV tanAngle = ScalarVFromF32(CParaboloidShadow::m_activeEntries[ warpShadowIndex ].m_info.m_tanFOV);
		const ScalarV adjustedTanAngle = tanAngle + BANK_SWITCH(CParaboloidShadow::m_debugParaboloidShadowTightSpotCullShapeAngleDelta, PARABOLOID_SHADOW_SPOT_ANGLE_DELTA) * tanAngle;
		
		const ScalarV radius = ScalarVFromF32(CParaboloidShadow::m_activeEntries[ warpShadowIndex ].m_info.m_shadowDepthRange);
		const ScalarV adjustRadius = radius + BANK_SWITCH(CParaboloidShadow::m_debugParaboloidShadowTightSpotCullShapeRadiusDelta, PARABOLOID_SHADOW_SPOT_RADIUS_DELTA) * radius;
		
		const Vec3V dir = RCC_VEC3V(CParaboloidShadow::m_activeEntries[ warpShadowIndex ].m_dir);
		//move position a little behind to account for moving lights
		const Vec3V spotLightPos = pos -  dir *BANK_SWITCH(CParaboloidShadow::m_debugParaboloidShadowTightSpotCullShapePositionDelta, PARABOLOID_SHADOW_SPOT_POSITION_DELTA);
		const Vec3V tangent = RCC_VEC3V(CParaboloidShadow::m_activeEntries[ warpShadowIndex ].m_tandir);

		spdTransposedPlaneSet8 shadowFrustum;
		spdSphere shadowSphere(pos, adjustRadius);

		const ScalarV maxTanAngleForSinglePlaneFrustum = ScalarVConstant<FLOAT_TO_INT(5.67f)>(); // tan(80 deg) = 5.67128

		//If we set up only frustum test, it would be inefficient for wide spot lights,
		//as the frustum size grows exponentially based on the angle
		//Setting up cull shape to do both frustum test and sphere test, so that it will cull 
		//entities that are outside of the sphere that are still in the frustum
		//For generating the frustum, we don't care about the near and far plane as the sphere
		//test would help us indicate end edge of the frustum. 
		//So we use all 8 planes for the sides of the cone.

		//if angle is greater equal than 80 degrees then we could use a single plane to represent the frustum.
		if(IsGreaterThanAll(adjustedTanAngle, maxTanAngleForSinglePlaneFrustum))
		{
			const Vec4V zero(V_ZERO);
			const Vec4V planes[] = {BuildPlane(spotLightPos, -dir), zero, zero, zero, zero, zero, zero, zero};
			shadowFrustum.SetPlanes(planes); 
		}
		else
		{
			//if angle is less than 80 degrees then generate 8 sided infinite cone as frustum
			shadowFrustum = GenerateConeFrustum(spotLightPos, dir, tangent, adjustedTanAngle, radius);
		}

		//CULL_SHAPE_FLAG_WATER_REFLECTION_USE_SEPARATE_SPHERE flag indicates that it should use the
		//interior sphere to do the sphere test
		cullShape.SetFlags( CULL_SHAPE_FLAG_GEOMETRY_FRUSTUM | CULL_SHAPE_FLAG_WATER_REFLECTION_USE_SEPARATE_SPHERE );
		cullShape.SetFrustum( shadowFrustum );
		cullShape.SetInteriorSphere(shadowSphere);
#if __BANK
		if(CParaboloidShadow::m_debugParaboloidShadowRenderCullShape)
		{
			grcDebugDraw::Sphere(pos, adjustRadius.Getf(), Color32(1.0f, 1.0f, 0.0f, 1.0f), false );
		}
#endif

	}
	else
	{
		spdSphere	shadowSphere(pos, ScalarV(CParaboloidShadow::m_activeEntries[ warpShadowIndex ].m_info.m_shadowDepthRange));
		cullShape.SetFlags( CULL_SHAPE_FLAG_GEOMETRY_SPHERE );
		cullShape.SetSphere( shadowSphere );
#if __BANK
		if(CParaboloidShadow::m_debugParaboloidShadowRenderCullShape)
		{
			grcDebugDraw::Sphere(pos, CParaboloidShadow::m_activeEntries[ warpShadowIndex ].m_info.m_shadowDepthRange, Color32(1.0f, 0.0f, 0.0f, 1.0f), false );
		}
#endif

	}
	cullShape.SetCameraPosition( pos );
}

u32 CRenderPhaseParaboloidShadow::GetCullFlags() const
{
	u32 cullFlags = CULL_SHAPE_FLAG_RENDER_EXTERIOR	|
					CULL_SHAPE_FLAG_RENDER_INTERIOR	|
					CULL_SHAPE_FLAG_TRAVERSE_PORTALS;

	const u16 shadowFlags = CParaboloidShadow::m_activeEntries[m_paraboloidShadowIndex].m_flags;

	if ((shadowFlags & SHADOW_FLAG_EXTERIOR) == 0)
		cullFlags &= ~CULL_SHAPE_FLAG_RENDER_EXTERIOR;
	else if ((shadowFlags & SHADOW_FLAG_INTERIOR) == 0)
		cullFlags &= ~CULL_SHAPE_FLAG_RENDER_INTERIOR;

	return cullFlags;
}

void CRenderPhaseParaboloidShadow::UpdateViewport()
{
#if RSG_PC
	if (CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality == (CSettings::eSettingsLevel) (0))
	{
		return;
	}
#endif

	const CViewport *pVp = GetViewport();
	m_Scanner.SetLodViewport(&pVp->GetGrcViewport());
	const int s = m_paraboloidShadowIndex;

	Assert(s >= 0);

	const CParaboloidShadowActiveEntry& entry = CParaboloidShadow::m_activeEntries[s];

	grcViewport parentGrcViewport = pVp->GetGrcViewport();
	grcViewport &viewport = GetGrcViewport();

	SetVisibilityType( VIS_PHASE_PARABOLOID_SHADOWS );

	if (entry.m_flags&SHADOW_FLAG_ENABLED)
	{
		SetActive(true);
		u32 staticOrDynamic = (entry.m_flags&SHADOW_FLAG_STATIC_AND_DYNAMIC_MASK);

		if (staticOrDynamic == SHADOW_FLAG_STATIC_AND_DYNAMIC)
		{
			GetEntityVisibilityMask().SetAllFlags(
				VIS_ENTITY_MASK_PED					|
				VIS_ENTITY_MASK_VEHICLE				|
				VIS_ENTITY_MASK_OBJECT				|
				VIS_ENTITY_MASK_DUMMY_OBJECT		|
				VIS_ENTITY_MASK_ANIMATED_BUILDING	|
				VIS_ENTITY_MASK_HILOD_BUILDING		|
#if __D3D11 || RSG_ORBIS
				VIS_ENTITY_MASK_TREE				|
#endif
				0);
		}
		else if (staticOrDynamic == SHADOW_FLAG_STATIC)
		{
			GetEntityVisibilityMask().SetAllFlags(
				VIS_ENTITY_MASK_ANIMATED_BUILDING	|	
				VIS_ENTITY_MASK_HILOD_BUILDING		|
#if __D3D11 || RSG_ORBIS
				VIS_ENTITY_MASK_TREE				|
#endif
				0);
		}
		else if (staticOrDynamic == SHADOW_FLAG_DYNAMIC)
		{
			GetEntityVisibilityMask().SetAllFlags(
				VIS_ENTITY_MASK_PED					|
				VIS_ENTITY_MASK_VEHICLE				|
				VIS_ENTITY_MASK_OBJECT				|
				VIS_ENTITY_MASK_DUMMY_OBJECT		|
#if __D3D11 || RSG_ORBIS
				VIS_ENTITY_MASK_TREE				|
#endif
				0);
		}
	
		if (entry.m_info.m_type == SHADOW_TYPE_SPOT)
		{
			CParaboloidShadow::CalcSpotLightMatrix(s);

			// TODO -- shameful math!!
			const float fov = RtoD * 2.0f * rage::Atan2f(entry.m_info.m_tanFOV,1.0f);

			viewport.Perspective(
				fov,
				1.0f,
				entry.m_info.m_nearClip,
				entry.m_info.m_shadowDepthRange
				);

			viewport.SetCameraMtx(entry.m_info.m_shadowMatrix);
		}
		else // if (entry.m_info.m_type == CShadowInfo::SHADOW_TYPE_POINT  || entry.m_info.m_type == CShadowInfo::SHADOW_TYPE_HEMISPHERE )
		{
			CParaboloidShadow::CalcPointLightMatrix(s);

			// NOTE: when using CM shadows, this viewport is for a rough cull only. it it not per facet	
			// TODO: need to improve this and add per facet cull for cubemaps
			// TODO -- shameful math!!
			const float zBack = 256.0f;
			const float FOV = (180.0f/PI)*rage::Atan2f(
				(entry.m_info.m_shadowDepthRange)/
				(zBack - entry.m_info.m_shadowDepthRange),
				1.0f
				);
			viewport.Perspective(
				2.0f*FOV,
				1.0f,
				zBack - ((entry.m_info.m_type == SHADOW_TYPE_HEMISPHERE) ? 0 : entry.m_info.m_shadowDepthRange),
				zBack + entry.m_info.m_shadowDepthRange
				);
			
				// for CM shadows the shadowMtx is no longer includes shadowBack Z, so let's just make the correct one for culling here
				Mat34V lightMat = entry.m_info.m_shadowMatrix;
				lightMat.Setd(entry.m_info.m_shadowMatrix.d() + entry.m_info.m_shadowMatrix.c() * ScalarV(zBack));	  
				viewport.SetCameraMtx(lightMat);
		}

		viewport.SetWorldMtx(parentGrcViewport.GetWorldMtx());
	}
}



void CRenderPhaseParaboloidShadow::BuildRenderList()
{
#if RSG_PC
	if (CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality == (CSettings::eSettingsLevel) (0))
	{
		return;
	}
#endif

	const bool bEnableOcclusion = false;

	PF_FUNC(CRenderPhaseParaboloidShadowRL);
	{
		const CParaboloidShadowActiveEntry& entry = CParaboloidShadow::m_activeEntries[m_paraboloidShadowIndex];

		if ((entry.m_flags & SHADOW_FLAG_ENABLED) != 0)
		{
			if (entry.m_info.m_type == SHADOW_TYPE_POINT ||
				entry.m_info.m_type == SHADOW_TYPE_SPOT || 
				entry.m_info.m_type == SHADOW_TYPE_HEMISPHERE)
			{
				COcclusion::BeforeSetupRender(false, false, SHADOWTYPE_PARABOLOID_SHADOWS, bEnableOcclusion, GetGrcViewport());
				CRenderer::ConstructRenderListNew(GetRenderFlags(), this);
				COcclusion::AfterPreRender();
			}
		}
	}
}


bool CRenderPhaseParaboloidShadow::ShadowEntityCullCheck(fwRenderPassId UNUSED_PARAM(id), fwRenderBucket UNUSED_PARAM(bucket), fwRenderMode UNUSED_PARAM(renderMode), int ASSERT_ONLY(subphasePass), float UNUSED_PARAM(sortVal), fwEntity *entity)
{
	Assert(subphasePass<SUBPHASE_NONE); // this should never be called for anything but local shadows.

	const CEntity *pEntity  = static_cast<CEntity*>(entity);
	spdAABB tempBox;
	const spdAABB &aabb = pEntity->GetBoundBox(tempBox);

	return grcViewport::IsAABBVisible(aabb.GetMin().GetIntrin128(),aabb.GetMax().GetIntrin128(),sm_CurrentFacetFrustumLRTB)!=0;
}

//----------------------------------------------------
// build a drawlist from this renderphase
void CRenderPhaseParaboloidShadow::BuildDrawList()
{
#if RSG_PC
	if (CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality == (CSettings::eSettingsLevel) (0))
	{
		return;
	}
#endif

	if (!HasEntityListIndex())
	{
		return;
	}

	const int s = m_paraboloidShadowIndex;
	const CParaboloidShadowActiveEntry& entry = CParaboloidShadow::m_activeEntries[s];

	if ((entry.m_flags&SHADOW_FLAG_ENABLED)==0)
	{
		return;
	}

#if !DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS
	DrawListPrologue();
#else // !DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS
	DrawListPrologue(DL_RENDERPHASE_PARABOLOID_SHADOWS_0 + m_paraboloidShadowIndex);
#endif // !DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS

	PPU_ONLY(CBubbleFences::WaitForBubbleFenceDLC(CBubbleFences::BFENCE_ALPHA));
	
	DLCPushGPUTimebar(GT_LOCAL_SHADOWS, "Local Shadows");
	
	if (s==0 && CParaboloidShadow::UsingAsyncStaticCacheCopy()) // for the first shadow phase, if we're using async copies, let's wait for the DMA copies to finish
	{
		DLCPushTimebar("Async Shadow Copy Wait");
#if DEVICE_GPU_WAIT || CACHE_USE_MOVE_ENGINE
		DLC_Add(CParaboloidShadow::InsertWaitForAsyncStaticCacheCopy); // we should do this only for the first shadow render phase.
#endif 
		DLCPopTimebar();
	}

	DLCPushTimebar("Local Shadows");

	DLCPushTimebar("Setup");
	
#if MULTIPLE_RENDER_THREADS
	CViewportManager::DLCRenderPhaseDrawInit();
#endif

#if USE_EDGE
	// if we're a moving light, on ps3 we need to update the viewport, or edge culling will hose us.
	if (entry.m_info.m_type == SHADOW_TYPE_SPOT && (entry.m_flags&SHADOW_FLAG_LATE_UPDATE)!=0)
		GetGrcViewport().SetCameraMtx(entry.m_info.m_shadowMatrix);
#endif

	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));

	DLC_Add(CParaboloidShadow::BeginRender);

	PF_FUNC(CRenderPhaseParaboloidShadow);

	DLCPopTimebar();

	BuildDrawListCM(entry,s);

	DLC_Add(CParaboloidShadow::EndRender);

#if RSG_PC
	if (!GRCDEVICE.UsingMultipleGPUs())
#endif
	CParaboloidShadow::m_activeEntries[s].m_flags &= ~SHADOW_FLAG_ENABLED;

	DLC(dlCmdSetCurrentViewportToNULL, ());

	DLCPopTimebar();
	DLCPopGPUTimebar();

#if !DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS
	DrawListEpilogue();
#else // !DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS
	DrawListEpilogue(DL_RENDERPHASE_PARABOLOID_SHADOWS_0 + m_paraboloidShadowIndex);
#endif // !DRAW_LIST_MGR_SPLIT_PARABOLOID_SHADOWS
}	   

void LockDepthRT(grcRenderTarget * target, int index, bool clearTarget)
{
	grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, target, index);

	if (clearTarget)
		GRCDEVICE.Clear(false, CRGBA(0), true, 1.0f, false, 0);
}

void UnlockDepthRT()
{

	grcResolveFlags pResolveFlags;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &pResolveFlags);
}

void CRenderPhaseParaboloidShadow::AddCubeMapFaces(int s, int destIndex, bool renderDynamic, bool renderStatic, const CShadowInfo& info, bool clearTarget)
{
	bool isSpot = (info.m_type == SHADOW_TYPE_SPOT);
	int facetCount = CParaboloidShadow::GetFacetCount(info.m_type);

	int arrayOffset = (isSpot?1:6)*CParaboloidShadow::GetCacheEntryRTIndex(destIndex); 
	grcRenderTarget * shadowTarget = CParaboloidShadow::GetCacheEntryRT(destIndex, isSpot);
	Assertf(shadowTarget, "destIndex=%d, isSpot='%s'",destIndex,isSpot?"true":"false");
	Assert(destIndex<MAX_CACHED_PARABOLOID_SHADOWS);

	for (int i = 0; i < facetCount; i++) // front+ back, left+ right, up+ down
	{
		if (renderDynamic && ((info.m_facetVisibility&(0x1<<i)) == 0)) // skip generating dynamic shadows for offscreen facets, not for static cache, because it will come back on later and the cache needs to have the data
			continue;
#if __BANK
		static const char * facetNames[] = {"Right Facet", "Left Facet", "Up Facet", "Down Facet", "Front Facet", "Back Facet" };
		if (facetCount>1) {DLC_Add(PIXBegin, 1, facetNames[i]);} // extra pix annotation in bank builds helps debug shadows
#endif
		DLC_Add(LockDepthRT, shadowTarget, arrayOffset+i, clearTarget );
		
		DLC_Add(CParaboloidShadow::SetDefaultStates);

		AddToDrawListParaboloidShadow(GetEntityListIndex(), s, i, renderDynamic, renderStatic, facetCount>1); 

		DLC_Add(UnlockDepthRT);
		BANK_ONLY(if (facetCount>1) {DLC_Add(PIXEnd);});
	}
}

   
void CRenderPhaseParaboloidShadow::BuildDrawListCM(const CParaboloidShadowActiveEntry& entry, int s)
{
	//This is currently only used on the PC version for the vehicle headlights shadow casting to stop the vehicle
	//and peds appearing in its own shadow.
	u32 activeBaseFlags[CLightSource::MAX_SHADOW_EXCLUSION_ENTITIES];
	CLightSource * sceneLights = Lights::m_prevSceneShadowLights;
	int numLights = Lights::m_numPrevSceneShadowLights;
	const CLightSource* light = NULL;

	if((entry.m_flags&SHADOW_FLAG_ENTITY_EXCLUSION) != 0)
	{
		for (int i = 0; i < numLights; i++)
		{		
			if(sceneLights[i].GetShadowTrackingId() == entry.m_trackingID)
			{
				light = &sceneLights[i];
				int cnt = light->GetShadowExclusionEntitiesCount();

				for( u32 j = 0; j < cnt; j++)
				{
					fwEntity *pEntity = light->GetShadowExclusionEntity(j);

					if(pEntity)
					{
						activeBaseFlags[j] = pEntity->GetBaseFlags();
						pEntity->ClearBaseFlag(CEntity::IS_VISIBLE);
					}
				}
				break;
			}
		}
	}

	if ( Verifyf(entry.m_info.m_type == SHADOW_TYPE_SPOT || entry.m_info.m_type == SHADOW_TYPE_POINT || entry.m_info.m_type == SHADOW_TYPE_HEMISPHERE, "Trying to cache shadow map for a light other than a point/spot"))
	{
		int staticOrDynamic = (entry.m_flags&SHADOW_FLAG_STATIC_AND_DYNAMIC_MASK);  // if we're not using the cache we'll clear & draw everything each time (the draw list will only include static and/or dynamic objects if the light is flagged as casting them)
		const int staticCacheIndex = entry.m_staticCacheIndex;
		const int activeCacheIndex = entry.m_activeCacheIndex;
		bool updatedStaticCache = false;

		if ((staticCacheIndex != -1))
		{
			// they are linked to a cache entry that needs updating, so let's do that first
			if (CParaboloidShadow::m_cachedEntries[staticCacheIndex].m_status==CACHE_STATUS_GENERATING ||
				CParaboloidShadow::m_cachedEntries[staticCacheIndex].m_status==CACHE_STATUS_REFRESH_CANDIDATE)
			{
				u32 staticCheckSum = 0;
				u16 staticVisibleCount = 0;
				
#if RSG_PC
				bool bMGCount = CParaboloidShadow::m_cachedEntries[staticCacheIndex].m_gpucounter_static > 0 ? true : false;
#endif

#if RSG_PC
				if (!bMGCount)
#endif
				{
				// make a check sum of the static objects, in case some have changed/streamed in (count is not enough, the count can stay the same, but the objects could differ)
				fwRenderListDesc& renderList = gRenderListGroup.GetRenderListForPhase(GetEntityListIndex());
				int count = renderList.GetNumberOfEntities(RPASS_VISIBLE);
			
				for (int i=0;i<count;i++)
				{
					if (renderList.GetEntityRenderFlags(i, RPASS_VISIBLE) & CEntity::RenderFlag_IS_BUILDING )
					{
						if (fwEntity* entity=renderList.GetEntity(i, RPASS_VISIBLE))
							staticCheckSum ^= (u32)(size_t)entity;
						staticVisibleCount ++;
					}
				}
				}
	
				bool bEntityChangeDetected = CParaboloidShadow::m_cachedEntries[staticCacheIndex].m_staticCheckSum != staticCheckSum ||
											 CParaboloidShadow::m_cachedEntries[staticCacheIndex].m_staticVisibleCount != staticVisibleCount;

				// if this a new (generating) cache, or the number of static object changed, we need to update the cache
				if (CParaboloidShadow::m_cachedEntries[staticCacheIndex].m_status==CACHE_STATUS_GENERATING || bEntityChangeDetected)
				{
					updatedStaticCache = true;

					// this could be optimized to save a copy from the cache back to the active
					// i.e. render static (save to cache) continue to render dynamic
					DLCPushTimebar("Static Cache");
					AddCubeMapFaces(s, staticCacheIndex, false, true,  entry.m_info, true);
					DLCPopTimebar();

#if RSG_PC
					if (GRCDEVICE.UsingMultipleGPUs())
					{
						if (CParaboloidShadow::m_cachedEntries[staticCacheIndex].m_gpucounter_static == 0)
						{
							CParaboloidShadow::m_cachedEntries[staticCacheIndex].m_gpucounter_static = (u8)(GRCDEVICE.GetGPUCount(true));
						}
					}
#endif
				}

				if (gRenderThreadInterface.IsUsingDefaultRenderFunction()
#if RSG_PC
				 && !bMGCount
#endif
					) // if not, our cache might not get updated
				{
					CParaboloidShadow::m_cachedEntries[staticCacheIndex].m_ageSinceUpdated	  = 0;   // set it to 0 since we check, so it's current as of now.
					CParaboloidShadow::m_cachedEntries[staticCacheIndex].m_staticCheckSum	  = staticCheckSum;
					CParaboloidShadow::m_cachedEntries[staticCacheIndex].m_staticVisibleCount = (u16)staticVisibleCount;
				}			
			}

			if(staticOrDynamic == SHADOW_FLAG_STATIC_AND_DYNAMIC)
			{
				DLCPushTimebar("Dynamic W/Cache");
				
				// if it changed Or we did not Dma it into Place beforehand, lets Get it now.
				if (updatedStaticCache || (!CParaboloidShadow::UsingAsyncStaticCacheCopy()))    
				{

#if DEVICE_GPU_WAIT || CACHE_USE_MOVE_ENGINE
					if (updatedStaticCache)
						DLC_Add(GRCDEVICE.GpuWaitOnPreviousWrites); // wait for static rendering to finish
#endif

					int faceCount = CParaboloidShadow::GetFacetCount(entry.m_info.m_type);
					DLC_Add(CParaboloidShadow::FetchCacheMap, staticCacheIndex, activeCacheIndex, faceCount, entry.m_info.m_type == SHADOW_TYPE_SPOT, true); 
				}

				AddCubeMapFaces(s, activeCacheIndex, true, false, entry.m_info, false); 
				DLCPopTimebar();
			}
		}
		else if (staticOrDynamic == SHADOW_FLAG_DYNAMIC )
		{
			DLCPushTimebar("Dynamic");
			AddCubeMapFaces(s, activeCacheIndex, true, false, entry.m_info, true);
			DLCPopTimebar();
		}
		else if (staticOrDynamic == SHADOW_FLAG_STATIC_AND_DYNAMIC)
		{
			DLCPushTimebar("Static And Dynamic");
			AddCubeMapFaces(s, activeCacheIndex, true, true, entry.m_info, true); 
			DLCPopTimebar();
		}
		else if (staticOrDynamic == SHADOW_FLAG_STATIC)  // must be a static shadow only moving light
		{
			DLCPushTimebar("Static Only Not cached");
			AddCubeMapFaces(s, activeCacheIndex, false, true, entry.m_info, true);
 			DLCPopTimebar();
		}
	}

	if (light)
	{
		int cnt = light->GetShadowExclusionEntitiesCount();

		for( u32 i = 0; i < cnt; i++)
		{
			fwEntity *pEntity = light->GetShadowExclusionEntity(i);
			if (pEntity)
				pEntity->SetAllBaseFlags((fwEntity::flags)activeBaseFlags[i]);
		}
	}
}

fwInteriorLocation CRenderPhaseParaboloidShadow::GetInteriorLocation() const {
	return CParaboloidShadow::m_activeEntries[m_paraboloidShadowIndex].m_interiorLocation;
}

