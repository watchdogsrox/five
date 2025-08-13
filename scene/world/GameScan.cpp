#include "fwscene/lod/LodTypes.h"
#include "fwscene/scan/ScanEntities.h"
#include "fwscene/scan/ScanDebug.h"
#include "fwscene/world/WorldRepMulti.h"
#include "fwscene/world/SceneGraphVisitor.h"
#include "system/spinlockedobject.h"

#include "camera/camInterface.h"
#include "camera/base/basecamera.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/debug/DebugDirector.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/GtaPicker.h"
#include "debug/TiledScreenCapture.h"
#include "input/mouse.h"
#include "peds/ped.h"
#include "renderer/PostScan.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseDefLight.h"
#include "renderer/RenderPhases/RenderPhaseParaboloidShadows.h"
#include "renderer/RenderPhases/RenderPhaseMirrorReflection.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/Shadows/Shadows.h"
#include "renderer/Renderer.h"
#include "renderer/occlusion.h"
#include "renderer/Debug/EntitySelect.h"
#include "renderer/Debug/Raster.h"
#include "scene/ContinuityMgr.h"
#include "scene/EntityBatch.h"
#include "scene/LoadScene.h"
#include "scene/lod/LodMgr.h"
#include "scene/world/GameWorld.h"
#include "scene/world/VisibilityMasks.h"
#include "scene/world/GameScan.h"
#include "scene/world/WorldDebugHelper.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/portals/PortalTracker.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/streamer/StreamVolume.h"
#include "scene/debug/PostScanDebug.h"
#include "streaming/streamingrequestlist.h"
#include "system/controlMgr.h"
#include "timecycle/TimeCycle.h"
#include "timecycle/TimeCycleDebug.h"
#include "renderer/Mirrors.h"
#include "tools/ObjectProfiler.h"

SCENE_OPTIMISATIONS()

PARAM(occlusion, "Enable occlusion");
XPARAM(useMinimumSettings);

u32		CGameScan::ms_hdStreamVisibilityBit;
u32		CGameScan::ms_lodStreamVisibilityBit;
u32		CGameScan::ms_streamVolumeVisibilityBit1;
u32		CGameScan::ms_streamVolumeVisibilityBit2;
u32		CGameScan::ms_interiorStreamVisibilityBit;
u32		CGameScan::ms_streamingVisibilityMask;
bool	CGameScan::ms_gbufIsActive;

extern bool g_render_lock;

// make sure these match
CompileTimeAssert(VIS_ENTITY_PED										== _GTA5_VIS_ENTITY_PED);
CompileTimeAssert(VIS_ENTITY_OBJECT										== _GTA5_VIS_ENTITY_OBJECT);
CompileTimeAssert(VIS_ENTITY_VEHICLE									== _GTA5_VIS_ENTITY_VEHICLE);
CompileTimeAssert(VIS_ENTITY_LOLOD_BUILDING								== _GTA5_VIS_ENTITY_LOLOD_BUILDING);
CompileTimeAssert(VIS_ENTITY_HILOD_BUILDING								== _GTA5_VIS_ENTITY_HILOD_BUILDING);

CompileTimeAssert(ENTITY_TYPE_VEHICLE									== _GTA5_ENTITY_TYPE_VEHICLE);
CompileTimeAssert(ENTITY_TYPE_PED										== _GTA5_ENTITY_TYPE_PED);
CompileTimeAssert(ENTITY_TYPE_OBJECT									== _GTA5_ENTITY_TYPE_OBJECT);
CompileTimeAssert(ENTITY_TYPE_DUMMY_OBJECT								== _GTA5_ENTITY_TYPE_DUMMY_OBJECT);
CompileTimeAssert(ENTITY_TYPE_LIGHT										== _GTA5_ENTITY_TYPE_LIGHT);
CompileTimeAssert(ENTITY_TYPE_GRASS_INSTANCE_LIST						== _GTA5_ENTITY_TYPE_GRASS_BATCH);

CompileTimeAssert(VIS_PHASE_GBUF										== _GTA5_VIS_PHASE_GBUF);
CompileTimeAssert(VIS_PHASE_CASCADE_SHADOWS								== _GTA5_VIS_PHASE_CASCADE_SHADOWS);
CompileTimeAssert(VIS_PHASE_PARABOLOID_SHADOWS							== _GTA5_VIS_PHASE_PARABOLOID_SHADOWS);
CompileTimeAssert(VIS_PHASE_PARABOLOID_REFLECTION						== _GTA5_VIS_PHASE_PARABOLOID_REFLECTION);
CompileTimeAssert(VIS_PHASE_WATER_REFLECTION							== _GTA5_VIS_PHASE_WATER_REFLECTION);
CompileTimeAssert(VIS_PHASE_MIRROR_REFLECTION							== _GTA5_VIS_PHASE_MIRROR_REFLECTION);

CompileTimeAssert(VIS_PHASE_FLAG_SHADOW_PROXY							== _GTA5_VIS_PHASE_FLAG_SHADOW_PROXY);
CompileTimeAssert(VIS_PHASE_FLAG_WATER_REFLECTION_PROXY					== _GTA5_VIS_PHASE_FLAG_WATER_REFLECTION_PROXY);
CompileTimeAssert(VIS_PHASE_FLAG_WATER_REFLECTION_PROXY_PRE_REFLECTED	== _GTA5_VIS_PHASE_FLAG_WATER_REFLECTION_PROXY_PRE_REFLECTED);

fwRoomSceneGraphNode* CGameScan::FindRoomSceneNode(const fwInteriorLocation interiorLocation)
{
	for (fwSceneGraphVisitor visitor( fwWorldRepMulti::GetSceneGraphRoot() ); !visitor.AtEnd(); visitor.Next())
	{
		fwSceneGraphNode*	sceneNode = visitor.GetCurrent();
		if ( sceneNode->IsTypeRoom() )
		{
			fwRoomSceneGraphNode*	roomSceneNode = static_cast< fwRoomSceneGraphNode* >( sceneNode );
			if ( roomSceneNode->GetInteriorLocation().IsSameLocation( interiorLocation ) )
				return roomSceneNode;
		}
	}

	Assertf( false, "No room scene graph node found for this interior location" );
	
	return NULL;
}

fwSceneGraphNode* CGameScan::FindStartingSceneNode(const CRenderPhaseScanned* renderPhase)
{
	if ( renderPhase->GetPortalVisTracker() == NULL )
		return NULL;

	CInteriorInst*		renderPhaseInterior = renderPhase->GetPortalVisTracker()->GetInteriorInst();
	const s32			renderPhaseRoom = renderPhase->GetPortalVisTracker()->m_roomIdx;
	fwInteriorLocation	renderPhaseInteriorLocation = CInteriorInst::CreateLocation( renderPhaseInterior, renderPhaseRoom );

	if ( !renderPhaseInteriorLocation.IsValid() )
		return fwWorldRepMulti::GetSceneGraphRoot();
	else
		return renderPhaseInterior->CanReceiveObjects() ? FindRoomSceneNode( renderPhaseInteriorLocation ) : NULL;
}


namespace rage
{
	extern float g_PixelAspect;
}

extern	RegdEnt gpDisplayObject;

void CGameScan::SetupScan()
{
	PF_AUTO_PUSH_TIMEBAR("CGameScan::SetupScan");

#if __BANK
	CPostScanDebug::ProcessBeforeAllPreRendering();
#endif

	g_LodScale.Update();		// checks camera and determines new lod scale for this frame
	g_ContinuityMgr.Update();	// uses lod scale from lod scale update

	//////////////////////////////////////////////////////////////////////////
	// a bunch of crap to decide whether to allow time-based fades or not
	bool bNonGameCamera = g_ContinuityMgr.UsingNonGameCamera();
#if !__FINAL
	if (bNonGameCamera)
	{
		const camBaseCamera* renderedDebugCamera = camInterface::GetDebugDirector().GetRenderedCamera();
		bNonGameCamera = !(renderedDebugCamera && camInterface::IsDominantRenderedCamera(*renderedDebugCamera));
	}
#endif	//!__FINAL
	//////////////////////////////////////////////////////////////////////////

	g_LodMgr.PreUpdate(g_ContinuityMgr.HasMajorCameraChangeOccurred() || bNonGameCamera);	// this should occur after g_ContinuityMgr.Update
	g_SceneStreamerMgr.UpdateSphericalStreamerPosAndRadius();

	//---------- Get some GBuf data ----------//
	CRenderPhaseDeferredLighting_SceneToGBuffer*		gbufPhase = (CRenderPhaseDeferredLighting_SceneToGBuffer*) g_SceneToGBufferPhaseNew;

	Assertf( gbufPhase && gbufPhase->IsActive(), "There is no active GBuf render phase in use! Can't perform visibility." );
	if ( !gbufPhase || !gbufPhase->IsActive() )
		return;

	CInteriorInst*		gbufInterior = gbufPhase->GetPortalVisTracker()->GetInteriorInst();
	s32					gbufRoom = gbufPhase->GetPortalVisTracker()->m_roomIdx;
	fwInteriorLocation	gbufInteriorLocation = CInteriorInst::CreateLocation( gbufInterior, gbufRoom );
	fwSceneGraphNode*	gbufInitialSceneNode = FindStartingSceneNode( gbufPhase );

	// TODO -- this is kind of ugly .. but i need to access this from the mirror reflection phase
	CRenderPhaseMirrorReflection::SetMirrorScanCameraIsInInterior(gbufInitialSceneNode && !gbufInitialSceneNode->IsTypeExterior());

	//---------- Set global Scan parameters ----------//

	fwScan::SetRootContainer( fwWorldRepMulti::GetSceneGraphRoot() );
	fwScan::SetPrimaryLodScale( g_LodScale.GetGlobalScale() );
	fwScan::SetInitialInteriorLocation( gbufInteriorLocation );
	fwScan::SetHiZBuffer( SCAN_OCCLUSION_STORAGE_PRIMARY, COcclusion::GetHiZBuffer(SCAN_OCCLUSION_STORAGE_PRIMARY) );
	fwScan::SetHiZBuffer( SCAN_OCCLUSION_STORAGE_CASCADE_SHADOWS, COcclusion::GetHiZBuffer(SCAN_OCCLUSION_STORAGE_CASCADE_SHADOWS) );

	//---------- Add cull shapes ----------//

	fwScan::ResetCullShapes();

	s32 renderPhaseCount = RENDERPHASEMGR.GetRenderPhaseCount();

	for(s32 i = 0; i < renderPhaseCount; ++i)
	{
		CRenderPhaseScanned*		renderPhase = (CRenderPhaseScanned *) &RENDERPHASEMGR.GetRenderPhase(i);

		if (renderPhase->IsScanRenderPhase() && renderPhase->IsActive())
		{
			u32						flags = renderPhase->GetCullFlags();

			if (!flags)
			{
				continue;
			}

			if ( g_scanDebugFlagsPortals & SCAN_PORTALS_DEBUG_DISABLE_SCREEN_QUAD_PAIRS )
				flags &= ~CULL_SHAPE_FLAG_USE_SCREEN_QUAD_PAIRS;
			
#if TC_DEBUG
			if (!g_timeCycle.GetGameDebug().GetDisableExteriorVisibleModifier())
#endif
			{
				const float ALMOST_ZERO = 0.001f;
				if (g_timeCycle.GetStaleRenderExterior() <= ALMOST_ZERO)
				{
					flags &= ~CULL_SHAPE_FLAG_RENDER_EXTERIOR;
				}
			}

			fwRenderPhaseCullShape	cullShape;

			cullShape.SetLodMask(renderPhase->GetLodMask());

			if ( flags & CULL_SHAPE_FLAG_CLIP_AND_STORE_PORTALS )
			{
				if ( renderPhase->GetDrawListType() == DL_RENDERPHASE_GBUF )
					cullShape.SetScreenQuadStorageId( SCAN_SCREENQUAD_STORAGE_PRIMARY );
				else if ( renderPhase->GetDrawListType() == DL_RENDERPHASE_CASCADE_SHADOWS )
					cullShape.SetScreenQuadStorageId( SCAN_SCREENQUAD_STORAGE_CASCADE_SHADOWS );
				else if ( flags & CULL_SHAPE_FLAG_MIRROR )
					cullShape.SetScreenQuadStorageId( SCAN_SCREENQUAD_STORAGE_MIRROR );
				else
					Assertf( false, "Unrecognized render phase %s requires portal traversal", renderPhase->GetName() );
			}

			if ( flags & CULL_SHAPE_FLAG_CASCADE_SHADOWS )
			{
				const Mat44V	shadowTransform = CRenderPhaseCascadeShadowsInterface::GetViewport().GetCompositeMtx();
				Mat44V			invShadowTransform;
				InvertFull( invShadowTransform, shadowTransform ); // TODO -- this matrix is always orthonormal, can use InvertTransformOrtho(Mat34V,Mat34V)

				Vec4V			origin = invShadowTransform.GetCol3();
				origin = origin * Invert( origin ).GetW();

				cullShape.SetCompositeMatrix( shadowTransform );
				cullShape.SetCameraPosition( origin.GetXYZ() );
			}
			else if ((flags & CULL_SHAPE_FLAG_MIRROR) && (flags & CULL_SHAPE_FLAG_CLIP_AND_STORE_PORTALS))
			{
#if 0
				// Flip camera x back
				Mat34V newCamera34 = renderPhase->GetGrcViewport().GetCameraMtx();
				newCamera34.SetCol0(-newCamera34.GetCol0());

				// Rebuild composite matrix without oblique projection
				Mat34V view34;
				InvertTransformOrtho(view34,newCamera34);

				Mat44V view44;
				view44.SetCols( Vec4V(view34.GetCol0(),ScalarV(V_ZERO)),
								Vec4V(view34.GetCol1(),ScalarV(V_ZERO)),
								Vec4V(view34.GetCol2(),ScalarV(V_ZERO)),
								Vec4V(view34.GetCol3(),ScalarV(V_ONE)));

			    Mat44V modelView44;
				Multiply(modelView44, view44, renderPhase->GetGrcViewport().GetWorldMtx44());

				const grcViewport &grc = renderPhase->GetViewport()->GetGrcViewport();
				float scaleFar = DEV_SWITCH(CMirrors::ms_debugMirrorUseObliqueProjectionScale, MIRROR_DEFAULT_OBLIQUE_PROJ_SCALE);
				float zfar = grc.GetFarClip() * scaleFar;

				float aspect = grc.GetAspectRatio() PS3_ONLY(* g_PixelAspect);
				float znear = grc.GetNearClip();

				float h = 1.0f / grc.GetTanHFOV();
				float w = h / aspect;
				Vector2 shear = grc.GetPerspectiveShear();
				Vector2 scale = grc.GetWidthHeightScale();
				float zNorm = zfar / (znear - zfar);

				Mat44V projection;
				projection.SetCol0f(w*scale.x,	0,			0,			 0);
				projection.SetCol1f(0,			h*scale.y,	0,			 0);
				projection.SetCol2f(shear.x,	shear.y,	zNorm,		-1.0f);
				projection.SetCol3f(0,			0,			znear*zNorm, 0);

				Mat44V newComposite;
				Multiply(newComposite, projection, modelView44);

				cullShape.SetCompositeMatrix( newComposite );
#else
				cullShape.SetCompositeMatrix( CRenderPhaseMirrorReflectionInterface::GetViewportNoObliqueProjection()->GetCompositeMtx() );
#endif
				cullShape.SetCameraPosition( renderPhase->GetGrcViewport().GetCameraPosition() );
			}
			else
			{
#if NV_SUPPORT
				if (GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled() && (renderPhase->GetDrawListType() == DL_RENDERPHASE_GBUF ||
					renderPhase->GetDrawListType() == DL_RENDERPHASE_DRAWSCENE || renderPhase->GetDrawListType() == DL_RENDERPHASE_TREE) )
					cullShape.SetCompositeMatrix( renderPhase->GetGrcViewport().GetCullCompositeMtx() );
				else
#endif
					cullShape.SetCompositeMatrix( renderPhase->GetGrcViewport().GetCompositeMtx() );

				cullShape.SetCameraPosition( renderPhase->GetGrcViewport().GetCameraPosition() );
			}

			if ( (renderPhase->GetDrawListType() == DL_RENDERPHASE_RAIN_COLLISION_MAP) || 
				 (renderPhase->GetDrawListType() == DL_RENDERPHASE_WATER_REFLECTION) ||
				 (flags & CULL_SHAPE_FLAG_CASCADE_SHADOWS) )
			{
				cullShape.SetStartingSceneNode( fwWorldRepMulti::GetSceneGraphRoot() );
			}
			else if ( renderPhase->GetDrawListType() == DL_RENDERPHASE_MIRROR_REFLECTION )
			{
				fwSceneGraphNode* phaseStartingSceneNode = NULL;

				if (!CRenderPhaseMirrorReflection::GetMirrorScanFromGBufStartingNode())
					phaseStartingSceneNode = CRenderPhaseMirrorReflection::GetMirrorRoomSceneNode();

				cullShape.SetStartingSceneNode( phaseStartingSceneNode ? phaseStartingSceneNode : gbufInitialSceneNode );
			}
			else if ( renderPhase->GetDrawListType() == DL_RENDERPHASE_PARABOLOID_SHADOWS )
			{
				fwInteriorLocation		intLoc = static_cast< CRenderPhaseParaboloidShadow* >( renderPhase )->GetInteriorLocation();
				if ( intLoc.IsAttachedToRoom() )
				{
					CInteriorInst*			interior = CInteriorInst::GetInteriorForLocation( intLoc );
					if ( !interior || !interior->CanReceiveObjects() )
						continue;

					fwSceneGraphNode*		sceneNode = interior->GetRoomSceneGraphNode( intLoc.GetRoomIndex() );
					cullShape.SetStartingSceneNode( sceneNode );
				}
				else
					cullShape.SetStartingSceneNode( fwWorldRepMulti::GetSceneGraphRoot() );
			}
			else
			{
				fwSceneGraphNode* phaseStartingSceneNode = FindStartingSceneNode( renderPhase );
				cullShape.SetStartingSceneNode( phaseStartingSceneNode ? phaseStartingSceneNode : gbufInitialSceneNode );
			}

			cullShape.SetRenderPhaseId( renderPhase->GetEntityListIndex() );
			cullShape.SetEntityVisibilityMask( renderPhase->GetEntityVisibilityMask() );
			cullShape.SetVisibilityType( renderPhase->GetVisibilityType() );
			cullShape.ClearAllFlags();
			cullShape.SetFlags( flags );
			renderPhase->SetCullShape(cullShape);

#if __BANK
			if (TiledScreenCapture::IsEnabled())
			{
				cullShape.SetFlags(cullShape.GetFlags() & ~CULL_SHAPE_FLAG_RENDER_INTERIOR);
			}
#endif // __BANK

			fwScan::AddCullShape() = cullShape;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// scene streaming through visibility
#if FULL_SPHERICAL_SCENE_STREAMING

#if GTA_REPLAY
	bool bAllowSphericalSceneStreaming = !CReplayMgr::IsExporting();		//disable spherical streamers during export to save memory
#else
	bool bAllowSphericalSceneStreaming = true;
#endif

#if RSG_PC
	if (PARAM_useMinimumSettings.Get())
	{
		bAllowSphericalSceneStreaming = false;
	}
#endif	//RSG_PC

	SetupSceneStreamingCullShapes_NextGen(bAllowSphericalSceneStreaming);
	
#else
	SetupSceneStreamingCullShapes_CurrentGen();
#endif
	//////////////////////////////////////////////////////////////////////////

	//---------- Setup workload ----------//

	fwScanBaseInfo&		scanBaseInfo = fwScan::GetScanBaseInfo();

	scanBaseInfo.m_pvsStartingPtr = gPostScan.GetPVSBase();
	scanBaseInfo.m_pvsCurrentPtr = reinterpret_cast< fwPvsEntry* volatile* >( gPostScan.GetPVSCurrentPointer() );
	scanBaseInfo.m_pvsMaxSize = MAX_PVS_SIZE;

	scanBaseInfo.m_NextResultBlock = gPostScan.GetNextResultBlockPointer();
	scanBaseInfo.m_ResultBlocks = gPostScan.GetResultBlocksPointer();
	scanBaseInfo.m_ResultBlockCount = gPostScan.GetResultBlockCountPointer();
	scanBaseInfo.m_MaxResultBlocks = MAX_RESULT_BLOCKS;

	scanBaseInfo.m_cascadeShadowInfo = CRenderPhaseCascadeShadowsInterface::GetScanCascadeShadowInfo();
	scanBaseInfo.m_rendererPreStreamDistance = CRenderer::GetPreStreamDistance();
	scanBaseInfo.m_fadeDist = LODTYPES_FADE_DIST;
	scanBaseInfo.m_reflectionPlaneSets = CRenderPhaseReflection::ms_facetPlaneSets;

	scanBaseInfo.m_rasterizerDepthTestBias[ SCAN_OCCLUSION_TRANSFORM_PRIMARY ] = COcclusion::GetGBufOccluderBias();
	scanBaseInfo.m_rasterizerDepthTestBias[ SCAN_OCCLUSION_TRANSFORM_CASCADE_SHADOWS ] = COcclusion::GetShadowOccluderBias();
	scanBaseInfo.m_rasterizerDepthTestBias[ SCAN_OCCLUSION_TRANSFORM_WATER_REFLECTION ] = 0.0f;

	scanBaseInfo.m_minMaxBounds[ SCAN_OCCLUSION_TRANSFORM_PRIMARY ] = COcclusion::GetMinMaxBoundsForBuffer(SCAN_OCCLUSION_TRANSFORM_PRIMARY);
	scanBaseInfo.m_minMaxBounds[ SCAN_OCCLUSION_TRANSFORM_CASCADE_SHADOWS ] = COcclusion::GetMinMaxBoundsForBuffer(SCAN_OCCLUSION_TRANSFORM_CASCADE_SHADOWS);
	scanBaseInfo.m_minMaxBounds[ SCAN_OCCLUSION_TRANSFORM_WATER_REFLECTION ] = Vec4V(V_ZERO);


	scanBaseInfo.m_minZ[ SCAN_OCCLUSION_TRANSFORM_PRIMARY ] = ScalarVFromF32(COcclusion::GetMinZForBuffer(SCAN_OCCLUSION_TRANSFORM_PRIMARY));
	scanBaseInfo.m_minZ[ SCAN_OCCLUSION_TRANSFORM_CASCADE_SHADOWS ] = ScalarVFromF32(COcclusion::GetMinZForBuffer(SCAN_OCCLUSION_TRANSFORM_CASCADE_SHADOWS));
	scanBaseInfo.m_minZ[ SCAN_OCCLUSION_TRANSFORM_WATER_REFLECTION ] = ScalarV(V_ZERO);

	scanBaseInfo.m_receiverShadowHeightMin = CRenderPhaseCascadeShadowsInterface::GetMinimumRecieverShadowHeight();

	//////////////////////////////////////////////////////////////////////////
	// get per-lod-level multipliers, sort vals and fade distances
	for (u32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
	{
		scanBaseInfo.m_perLodLevelScales[i] = g_LodScale.GetPerLodLevelScale(i);
		scanBaseInfo.m_fadeDistances[i] = g_LodMgr.GetFadeDistance(i);
		scanBaseInfo.m_lodSortVals[i] = fwLodData::GetSortVal(i);
	}
	scanBaseInfo.m_defaultLodScale = g_LodScale.GetRootScale();
	scanBaseInfo.m_grassBatchLodScale = g_LodScale.GetGrassBatchScale();
	//////////////////////////////////////////////////////////////////////////

	scanBaseInfo.m_lodFilterMask = g_LodMgr.GetSlodModeRequiredMask();
	scanBaseInfo.m_streamingVolumeMask = GetStreamVolumeMask();

	//////////////////////////////////////////////////////////////////////////

	for (int i = 0; i < SCAN_OCCLUSION_TRANSFORM_COUNT; i++)
	{
		COcclusion::GetRasterizerTransform( i, scanBaseInfo.m_transform[i], scanBaseInfo.m_depthRescale[i]);
	}

	scanBaseInfo.m_fCullDistMin = 0.0f;
	scanBaseInfo.m_fCullDistMax = 16000.0f; //g_timeCycle.GetStaleEntityReject();
	scanBaseInfo.m_useExactSelectionSize = COcclusion::GetExactQueryThreshold();

#if __BANK
	scanBaseInfo.m_useSimpleQueries = COcclusion::UseSimpleQueriesOnly();
	scanBaseInfo.m_useNewOcclusionExact = COcclusion::UseNewOcclusionAABBExact();
	scanBaseInfo.m_useTrivialAcceptTest = COcclusion::UseTrivialAcceptTest();
	scanBaseInfo.m_useTrivialAcceptVisiblePixelTest = COcclusion::UseTrivialAcceptVisiblePixelTest();
	scanBaseInfo.m_useForceExactOcclusionTest = COcclusion::ForceExactOcclusionTest();
	scanBaseInfo.m_useProjectedShadowOcclusion = COcclusion::UseProjectedShadowOcclusionTest();
	scanBaseInfo.m_useProjectedShadowOcclusionClipping = COcclusion::UseProjectedShadowOcclusionClipping();
	scanBaseInfo.m_newOcclussionAABBExactPixelsThresholdSimple = COcclusion::GetNewOcclusionAABBExactSimpleTestPixelsThreshold();
	scanBaseInfo.m_useMinZClipping = COcclusion::UseMinZClipping();
	COcclusion::GetDebugCounters( scanBaseInfo.m_OccludedCount,	scanBaseInfo.m_OccludedTestedCount, scanBaseInfo.m_TrivialAcceptActiveFrustumCount, 
		scanBaseInfo.m_TrivialAcceptMinZCount, scanBaseInfo.m_TrivialAcceptVisiblePixelCount);

	if (CWorldDebugHelper::EarlyRejectExtraDistCull())
	{
		scanBaseInfo.m_fCullDistMin = (float)CWorldDebugHelper::GetEarlyRejectMinDist();
		scanBaseInfo.m_fCullDistMax = (float)CWorldDebugHelper::GetEarlyRejectMaxDist();
	}

	// remove these specific flags because they will be set below if needed
	g_scanDebugFlags &= ~(
		//	SCAN_DEBUG_EARLY_REJECT_EXTRA_DIST_CULL		|
		SCAN_DEBUG_EARLY_REJECT_ALL_EXCEPT_LISTED	|
		SCAN_DEBUG_EARLY_REJECT_LISTED				|
		SCAN_DEBUG_EARLY_REJECT_ALL_EXCEPT_SELECTED	|
		SCAN_DEBUG_EARLY_REJECT_SELECTED			|
		SCAN_DEBUG_DONT_EARLY_REJECT_SELECTED		|
		0);

	// Smoketest for the object profiler, remove all but the selected object
	// Inside __BANK as it relies on functionality in Debug.cpp (also __BANK)
	// Smoketest takes priority
	if(CObjectProfiler::IsActive())
	{
		if( gpDisplayObject != NULL )
		{
			extern	RegdEnt gpDisplayObject;
			scanBaseInfo.m_traceEntityAddress = gpDisplayObject;
			scanBaseInfo.m_traceEntityAddressCount = 1;
			scanBaseInfo.m_traceEntityAddressList[0] = gpDisplayObject;
			g_scanDebugFlags |= SCAN_DEBUG_EARLY_REJECT_ALL_EXCEPT_LISTED;
		}
	}
	else
	{
		if(CutSceneManager::GetInstance()->IsPlaying() && CutSceneManager::GetInstance()->IsIsolating() && CutSceneManager::GetInstance()->GetIsolatedPedEntity())
		{
			scanBaseInfo.m_traceEntityAddress = CutSceneManager::GetInstance()->GetIsolatedPedEntity();
		}
		else
			scanBaseInfo.m_traceEntityAddress = g_PickerManager.GetSelectedEntity();
		scanBaseInfo.m_traceEntityAddressCount = Min<int>(g_PickerManager.GetNumberOfEntities(), SCAN_MAX_TRACE_ENTITIES);
		for (int i = 0; i < scanBaseInfo.m_traceEntityAddressCount; i++)
		{
			scanBaseInfo.m_traceEntityAddressList[i] = g_PickerManager.GetEntity(i);
		}

		if ( CControlMgr::GetKeyboard().GetKeyJustDown(KEY_P, KEYBOARD_MODE_DEBUG_ALT) )
		{
			if (!(g_scanDebugFlagsPortals & SCAN_PORTALS_DEBUG_DISPLAY_PORTALS))
			{
				g_scanDebugFlagsPortals |= SCAN_PORTALS_DEBUG_DISPLAY_PORTALS;
			}
			else if ( (g_scanDebugFlagsPortals & SCAN_PORTALS_DEBUG_DISPLAY_PORTALS) && 
					 !(g_scanDebugFlagsPortals & SCAN_PORTALS_DEBUG_DISPLAY_PORTAL_INDICES) && 
					 !(g_scanDebugFlagsPortals & SCAN_PORTALS_DEBUG_DISPLAY_PORTAL_OPACITY))
			{
				g_scanDebugFlagsPortals |= SCAN_PORTALS_DEBUG_DISPLAY_PORTAL_INDICES;
			}
			else if ( (g_scanDebugFlagsPortals & (SCAN_PORTALS_DEBUG_DISPLAY_PORTALS | SCAN_PORTALS_DEBUG_DISPLAY_PORTAL_INDICES)) == 
					  (SCAN_PORTALS_DEBUG_DISPLAY_PORTALS | SCAN_PORTALS_DEBUG_DISPLAY_PORTAL_INDICES))
			{
				g_scanDebugFlagsPortals |= SCAN_PORTALS_DEBUG_DISPLAY_PORTAL_OPACITY;
				g_scanDebugFlagsPortals &= ~SCAN_PORTALS_DEBUG_DISPLAY_PORTAL_INDICES;
			}
			else
			{
				g_scanDebugFlagsPortals &= ~(SCAN_PORTALS_DEBUG_DISPLAY_PORTALS | SCAN_PORTALS_DEBUG_DISPLAY_PORTAL_OPACITY | SCAN_PORTALS_DEBUG_DISPLAY_PORTAL_INDICES);
			}
		}
	//	if ( CWorldDebugHelper::EarlyRejectExtraDistCull() )								g_scanDebugFlags |= SCAN_DEBUG_EARLY_REJECT_EXTRA_DIST_CULL;
		if ( g_PickerManager.GetShowHideMode() == PICKER_ISOLATE_ENTITIES_IN_LIST )			g_scanDebugFlags |= SCAN_DEBUG_EARLY_REJECT_ALL_EXCEPT_LISTED;
		if ( g_PickerManager.GetShowHideMode() == PICKER_HIDE_ENTITIES_IN_LIST )			g_scanDebugFlags |= SCAN_DEBUG_EARLY_REJECT_LISTED;
		if ( g_PickerManager.GetShowHideMode() == PICKER_ISOLATE_SELECTED_ENTITY )			g_scanDebugFlags |= SCAN_DEBUG_EARLY_REJECT_ALL_EXCEPT_SELECTED;
		if ( g_PickerManager.GetShowHideMode() == PICKER_ISOLATE_SELECTED_ENTITY_NOALPHA )	g_scanDebugFlags |= SCAN_DEBUG_EARLY_REJECT_ALL_EXCEPT_SELECTED;
		if ( g_PickerManager.GetShowHideMode() == PICKER_HIDE_SELECTED_ENTITY )				g_scanDebugFlags |= SCAN_DEBUG_EARLY_REJECT_SELECTED;
		if ( !g_PickerManager.IsEarlyRejectEnabled() )										g_scanDebugFlags |= SCAN_DEBUG_DONT_EARLY_REJECT_SELECTED;

		if(CutSceneManager::GetInstance()->IsPlaying() && CutSceneManager::GetInstance()->IsIsolating() )
		{
			g_scanDebugFlags |= SCAN_DEBUG_EARLY_REJECT_ALL_EXCEPT_SELECTED;
		}
	}
#endif // __BANK

#if ENTITYSELECT_ENABLED_BUILD
	if ( CEntitySelect::IsEntitySelectPassEnabled() )
	{
		g_scanDebugFlags |= SCAN_DEBUG_CULL_TO_ENTITY_SELECT_QUAD;

		const float		sx = 1.0f / (float)GRCDEVICE.GetWidth();
		const float		sy = 1.0f / (float)GRCDEVICE.GetHeight();

		Vector2			quadPos, quadSize;
		CEntitySelect::GetSelectScreenQuad( quadPos, quadSize );

		Vec4V			screenQuad( quadPos.x * sx, 1 - (quadPos.y + quadSize.y) * sy, (quadPos.x + quadSize.x) * sx, 1 - quadPos.y * sy );
		screenQuad = screenQuad * Vec4V(V_TWO) - Vec4V(V_ONE);

		scanBaseInfo.m_entitySelectQuad = fwScreenQuad( screenQuad );
		scanBaseInfo.m_entitySelectRenderPhaseId = static_cast< u8 >( g_RenderPhaseEntitySelectNew->GetEntityListIndex() );
	}
#endif // ENTITYSELECT_ENABLED_BUILD

	//---------- Other debug stuff ----------//

#if __BANK
	CPostScanDebug::GetNumModelsPreRenderedRef() = 0;
	CPostScanDebug::GetNumModelsPreRendered2Ref() = 0;
#endif
}

void CGameScan::Init()
{
	g_ContinuityMgr.Init();

	u32		streamingStartBit, streamingMaxCount, streamingMasks;

	gRenderListGroup.GetRenderPhaseTypeInfo( RPTYPE_STREAMING, &streamingStartBit, &streamingMaxCount, &streamingMasks );
	Assertf( streamingMaxCount >= 5, "Not enough bits allocated to streaming visibility phases" );

	ms_hdStreamVisibilityBit = streamingStartBit;
	ms_lodStreamVisibilityBit = streamingStartBit + 1;
	ms_streamVolumeVisibilityBit1 = streamingStartBit + 2;
	ms_streamVolumeVisibilityBit2 = streamingStartBit + 3;
	ms_interiorStreamVisibilityBit = streamingStartBit + 4;
	ms_streamingVisibilityMask = streamingMasks;

#if __BANK
	if ( PARAM_occlusion.Get() )
		fwScan::ms_forceEnableOcclusion = true;
#endif
}

void CGameScan::Update()
{
	PF_AUTO_PUSH_TIMEBAR("CGameScan::Update");

	if ( g_render_lock )
		return;

	fwScan::SetupDependencies();
	gPostScan.Reset();
}

void CGameScan::PerformScan()
{
	if ( g_render_lock || !ms_gbufIsActive)
		return;

	gPostScan.ProcessPvsWhileScanning();
	fwScan::JoinScan();

	if(fwScan::GetScanResults().IsEverythingInShadow()
#if RSG_PC
	   && CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality > CSettings::eSettingsLevel(0)
#endif
	   )
	{
		// If visibility scanning found that cascades should actually be disabled, do that now.
		CRenderPhaseCascadeShadowsInterface::DisableFromVisibilityScanning();
		// Make sure to also disable the directional light, as well as the cascades, or we'll get a lighting pop.
		Lights::DisableDirectionalLightFromVisibilityScanning();
	}
}

void CGameScan::KickVisibilityJobs()
{	
	PF_AUTO_PUSH_TIMEBAR("CGameScan::KickVisibilityJobs");

	if ( g_render_lock )
		return;

	ms_gbufIsActive = g_SceneToGBufferPhaseNew && g_SceneToGBufferPhaseNew->IsActive();
	if ( !ms_gbufIsActive )
	{
		COcclusion::WaitForAllRasterizersToFinish();
		return;
	}

	gWorldMgr.PreScan();
	COcclusion::WaitForAllRasterizersToFinish();
	CPostScan::StartAsyncSearchForAdditionalPreRenderEntities();
	CGameScan::SetupScan();
	fwScan::StartScan();
}


#if FULL_SPHERICAL_SCENE_STREAMING


//
// name:	SetupSceneStreamingCullShapes_NextGen
// purpose:	configures visibility cullshapes used to drive scene streaming
//
void CGameScan::SetupSceneStreamingCullShapes_NextGen(bool bAllowSphericalSceneStreaming)
{
	CRenderPhaseDeferredLighting_SceneToGBuffer* gbufPhase = (CRenderPhaseDeferredLighting_SceneToGBuffer*) g_SceneToGBufferPhaseNew;

	const bool bSkipSphericalSceneStreaming = !bAllowSphericalSceneStreaming BANK_ONLY( || TiledScreenCapture::IsEnabled() );

	if (!bSkipSphericalSceneStreaming)
	{
		//////////////////////////////////////////////////////////////////////////
		// SPHERICAL SCENE STREAMER - INTERIOR
		if ( g_scanDebugFlags & SCAN_DEBUG_ENABLE_INTERIOR_STREAM_CULL_SHAPE )
		{
			CGameScan::SetupInteriorStreamCullShape( gbufPhase );
		}

		//////////////////////////////////////////////////////////////////////////
		// SPHERICAL SCENE STREAMER - EXTERIOR
		{
			bool bLoadingExteriorScene = false;
			if (CStreamVolumeMgr::IsAnyVolumeActive())
			{
				for (s32 i=0; i<CStreamVolumeMgr::VOLUME_SLOT_TOTAL; i++)
				{
					CStreamVolume& strVolume = CStreamVolumeMgr::GetVolume(i);
					if ( strVolume.IsActive() && (strVolume.GetAssetFlags() & fwBoxStreamerAsset::MASK_MAPDATA) != 0 )
					{
						bLoadingExteriorScene = true;
					}
				}
			}

			const bool bSecondSceneLoading = bLoadingExteriorScene || g_SceneStreamerMgr.IsLoading() ;
			const bool bPlayerSwitchDescent = ( g_PlayerSwitch.IsActive() && g_PlayerSwitch.GetSwitchType()!=CPlayerSwitchInterface::SWITCH_TYPE_SHORT && g_PlayerSwitch.GetLongRangeMgr().PerformingDescent() );

			if (!bSecondSceneLoading || bPlayerSwitchDescent)
			{
				CGameScan::SetupExteriorStreamCullShape( gbufPhase, bPlayerSwitchDescent);
			}
		}

	}

	//////////////////////////////////////////////////////////////////////////
	// STREAMING VOLUMES
	if (g_scanDebugFlags & SCAN_DEBUG_ENABLE_STREAMING_VOLUME_CULL_SHAPE)
	{
		if (CStreamVolumeMgr::IsVolumeActive(CStreamVolumeMgr::VOLUME_SLOT_PRIMARY))
		{
			CGameScan::SetupStreamingVolumeCullShapes( gbufPhase, CStreamVolumeMgr::VOLUME_SLOT_PRIMARY, CGameScan::GetStreamVolumeVisibilityBit1() );
		}

		if (CStreamVolumeMgr::IsVolumeActive(CStreamVolumeMgr::VOLUME_SLOT_SECONDARY))
		{
			CGameScan::SetupStreamingVolumeCullShapes( gbufPhase, CStreamVolumeMgr::VOLUME_SLOT_SECONDARY, CGameScan::GetStreamVolumeVisibilityBit2() );
		}
	}
}


//
// name:	SetupExteriorStreamCullShape
// purpose:	sets up a single monolithic spherical cullshape which drives scene streaming
// 
void CGameScan::SetupExteriorStreamCullShape(const CRenderPhaseScanned* gbufPhase, bool bLowDetailOnly)
{
	if (g_SceneStreamerMgr.GetLodStreamSphereRadius() <= 100.0f)
	{
		// too small to be worthwhile - player is probably moving quickly
		return;
	}

	// intended for streaming in distant low lods when at high altitude
	fwRenderPhaseCullShape&		cullShape = fwScan::AddCullShape();
	cullShape.SetLodMask(fwRenderPhase::LODMASK_NONE);
	const Vec3V					lodStreamPos = g_SceneStreamerMgr.GetLodStreamSphereCentre();
	const float					fLodRadius = g_SceneStreamerMgr.GetLodStreamSphereRadius();
	spdSphere					sphere( lodStreamPos, ScalarV(fLodRadius) );

	cullShape.ClearAllFlags();
	cullShape.SetFlags(
		CULL_SHAPE_FLAG_GEOMETRY_SPHERE				|
		CULL_SHAPE_FLAG_RENDER_EXTERIOR				|
		CULL_SHAPE_FLAG_USE_LOD_RANGES_IN_EXTERIOR	|
		CULL_SHAPE_FLAG_STREAM_ENTITIES				|
		CULL_SHAPE_FLAG_ONLY_STREAM_ENTITIES		|
		0);

	cullShape.SetRenderPhaseId( CGameScan::GetLodStreamVisibilityBit() );
	cullShape.SetCompositeMatrix( gbufPhase->GetGrcViewport().GetCompositeMtx() );
	cullShape.SetCameraPosition( lodStreamPos );
	cullShape.SetStartingSceneNode( fwWorldRepMulti::GetSceneGraphRoot() );
	cullShape.SetVisibilityType( VIS_PHASE_STREAMING );	
	cullShape.InvalidateLodRanges();

#if KEEP_INSTANCELIST_ASSETS_RESIDENT
	cullShape.SetEntityVisibilityMask( VIS_ENTITY_MASK_HILOD_BUILDING | VIS_ENTITY_MASK_LOLOD_BUILDING | VIS_ENTITY_MASK_OBJECT | VIS_ENTITY_MASK_ANIMATED_BUILDING | VIS_ENTITY_MASK_TREE );
#else
	cullShape.SetEntityVisibilityMask( VIS_ENTITY_MASK_HILOD_BUILDING | VIS_ENTITY_MASK_LOLOD_BUILDING | VIS_ENTITY_MASK_OBJECT | VIS_ENTITY_MASK_ANIMATED_BUILDING | VIS_ENTITY_MASK_TREE | VIS_ENTITY_MASK_INSTANCELIST );
#endif

	if (!g_ContinuityMgr.HighSpeedMovement() && !bLowDetailOnly)
	{
		cullShape.EnableLodRange( LODTYPES_DEPTH_HD );
		cullShape.EnableLodRange( LODTYPES_DEPTH_ORPHANHD );
	}

	cullShape.EnableLodRange( LODTYPES_DEPTH_LOD );
	cullShape.EnableLodRange( LODTYPES_DEPTH_SLOD1 );
	cullShape.EnableLodRange( LODTYPES_DEPTH_SLOD2 );
	cullShape.EnableLodRange( LODTYPES_DEPTH_SLOD3 );
	cullShape.EnableLodRange( LODTYPES_DEPTH_SLOD4 );
	cullShape.SetSphere( sphere );	
}


#else	// i.e. !FULL_SPHERICAL_SCENE_STREAMING


//
// name:	SetupSceneStreamingCullShapes_CurrentGen
// purpose:	configures visibility cullshapes used to drive scene streaming
//
void CGameScan::SetupSceneStreamingCullShapes_CurrentGen()
{
	CRenderPhaseDeferredLighting_SceneToGBuffer* gbufPhase = (CRenderPhaseDeferredLighting_SceneToGBuffer*) g_SceneToGBufferPhaseNew;

	// check if we're loading an exterior scenes, and disable spherical streaming helpers if so

	bool bLoadingExteriorScene = false;
	if (CStreamVolumeMgr::IsAnyVolumeActive())
	{
		for (s32 i=0; i<CStreamVolumeMgr::VOLUME_SLOT_TOTAL; i++)
		{
			CStreamVolume& strVolume = CStreamVolumeMgr::GetVolume(i);
			if ( strVolume.IsActive() && (strVolume.GetAssetFlags() & fwBoxStreamerAsset::MASK_MAPDATA) != 0 )
			{
				bLoadingExteriorScene = true;
			}
		}
	}

	const bool bPlayerSwitchDescent = ( g_PlayerSwitch.IsActive() && g_PlayerSwitch.GetSwitchType()!=CPlayerSwitchInterface::SWITCH_TYPE_SHORT && g_PlayerSwitch.GetLongRangeMgr().PerformingDescent() );

	bool bExtraSceneLoad = bLoadingExteriorScene || g_SceneStreamerMgr.IsLoading() ;

#if __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		bExtraSceneLoad = true;
	}
#endif // __BANK

	if ( !bExtraSceneLoad && !g_ContinuityMgr.HighSpeedFlying() && (g_scanDebugFlags & SCAN_DEBUG_ENABLE_HD_STREAM_CULL_SHAPE) )
		CGameScan::SetupHdStreamCullShape( gbufPhase );

	if ( g_scanDebugFlags & SCAN_DEBUG_ENABLE_INTERIOR_STREAM_CULL_SHAPE )
		CGameScan::SetupInteriorStreamCullShape( gbufPhase );

	if ( (bPlayerSwitchDescent || !bExtraSceneLoad ) && (g_scanDebugFlags & SCAN_DEBUG_ENABLE_LOD_STREAM_CULL_SHAPE) )
		CGameScan::SetupLodStreamCullShape( gbufPhase );

	if (g_scanDebugFlags & SCAN_DEBUG_ENABLE_STREAMING_VOLUME_CULL_SHAPE)
	{
		camGameplayDirector* gameplayDirector = camInterface::GetGameplayDirectorSafe();
		const bool bActiveVelocityVectorStreamer = (g_ContinuityMgr.HighSpeedFlying() && gameplayDirector && gameplayDirector->IsLookingBehind() && camInterface::IsRenderingDirector(*gameplayDirector));

		if (CStreamVolumeMgr::IsVolumeActive(CStreamVolumeMgr::VOLUME_SLOT_PRIMARY))
		{
			CGameScan::SetupStreamingVolumeCullShapes( gbufPhase, CStreamVolumeMgr::VOLUME_SLOT_PRIMARY, CGameScan::GetStreamVolumeVisibilityBit1() );
		}

		if (CStreamVolumeMgr::IsVolumeActive(CStreamVolumeMgr::VOLUME_SLOT_SECONDARY))
		{
			CGameScan::SetupStreamingVolumeCullShapes( gbufPhase, CStreamVolumeMgr::VOLUME_SLOT_SECONDARY, CGameScan::GetStreamVolumeVisibilityBit2() );
		}

		else if (bActiveVelocityVectorStreamer)
		{
			// no secondary streaming volume in use, so re-purpose this phase for a look-in-direction of velocity
			// streamer to help with streaming when looking backwards
			CGameScan::SetupVelocityVectorStreamer( gbufPhase, CGameScan::GetStreamVolumeVisibilityBit2() );
		}
	}
}


//
// name:	SetupHdStreamCullShape
// purpose:	sets up spherical cull shape to stream HD entities in a small radius around the player / focus pos
//
void CGameScan::SetupHdStreamCullShape(const CRenderPhaseScanned* gbufPhase)
{
	fwRenderPhaseCullShape&		cullShape = fwScan::AddCullShape();
	cullShape.SetLodMask(fwRenderPhase::LODMASK_NONE);
	const Vec3V					hdStreamPos = g_SceneStreamerMgr.GetHdStreamSphereCentre();
	const float					fHdRadius = g_SceneStreamerMgr.GetHdStreamSphereRadius();
	spdSphere					sphere( hdStreamPos, ScalarV(fHdRadius) );

	cullShape.ClearAllFlags();
	cullShape.SetFlags(
		CULL_SHAPE_FLAG_GEOMETRY_SPHERE				|
		CULL_SHAPE_FLAG_RENDER_EXTERIOR				|
		CULL_SHAPE_FLAG_USE_LOD_RANGES_IN_EXTERIOR	|
		CULL_SHAPE_FLAG_STREAM_ENTITIES				|
		CULL_SHAPE_FLAG_ONLY_STREAM_ENTITIES		|
		0);

	cullShape.SetRenderPhaseId( CGameScan::GetHdStreamVisibilityBit() );
	cullShape.SetCompositeMatrix( gbufPhase->GetGrcViewport().GetCompositeMtx() );
	cullShape.SetCameraPosition( hdStreamPos );
	cullShape.SetStartingSceneNode( fwWorldRepMulti::GetSceneGraphRoot() );
	cullShape.SetVisibilityType( VIS_PHASE_STREAMING );
	cullShape.InvalidateLodRanges();

#if KEEP_INSTANCELIST_ASSETS_RESIDENT
	cullShape.SetEntityVisibilityMask( VIS_ENTITY_MASK_OBJECT | VIS_ENTITY_MASK_ANIMATED_BUILDING | VIS_ENTITY_MASK_HILOD_BUILDING |  VIS_ENTITY_MASK_TREE );
#else
	cullShape.SetEntityVisibilityMask( VIS_ENTITY_MASK_OBJECT | VIS_ENTITY_MASK_ANIMATED_BUILDING | VIS_ENTITY_MASK_HILOD_BUILDING |  VIS_ENTITY_MASK_TREE | VIS_ENTITY_MASK_INSTANCELIST );
#endif

	cullShape.EnableLodRange( LODTYPES_DEPTH_HD );
	cullShape.EnableLodRange( LODTYPES_DEPTH_ORPHANHD );
	cullShape.SetSphere( sphere );
}


//
// name:	SetLodStreamCullShape
// purpose:	sets up large spherical cull shape to request map LOD entities all aruond the player / focus
//
void CGameScan::SetupLodStreamCullShape(const CRenderPhaseScanned* gbufPhase)
{
	if (g_SceneStreamerMgr.GetLodStreamSphereRadius() <= 100.0f)
	{
		// too small to be worthwhile - player is probably moving quickly
		return;
	}

	// intended for streaming in distant low lods when at high altitude
	fwRenderPhaseCullShape&		cullShape = fwScan::AddCullShape();
	cullShape.SetLodMask(fwRenderPhase::LODMASK_NONE);
	const Vec3V					lodStreamPos = g_SceneStreamerMgr.GetLodStreamSphereCentre();
	const float					fLodRadius = g_SceneStreamerMgr.GetLodStreamSphereRadius();
	spdSphere					sphere( lodStreamPos, ScalarV(fLodRadius) );

	cullShape.ClearAllFlags();
	cullShape.SetFlags(
		CULL_SHAPE_FLAG_GEOMETRY_SPHERE				|
		CULL_SHAPE_FLAG_RENDER_EXTERIOR				|
		CULL_SHAPE_FLAG_USE_LOD_RANGES_IN_EXTERIOR	|
		CULL_SHAPE_FLAG_STREAM_ENTITIES				|
		CULL_SHAPE_FLAG_ONLY_STREAM_ENTITIES		|
		0);

	cullShape.SetRenderPhaseId( CGameScan::GetLodStreamVisibilityBit() );
	cullShape.SetCompositeMatrix( gbufPhase->GetGrcViewport().GetCompositeMtx() );
	cullShape.SetCameraPosition( lodStreamPos );
	cullShape.SetStartingSceneNode( fwWorldRepMulti::GetSceneGraphRoot() );
	cullShape.SetVisibilityType( VIS_PHASE_STREAMING );	
	cullShape.InvalidateLodRanges();
	cullShape.SetEntityVisibilityMask( VIS_ENTITY_MASK_LOLOD_BUILDING );

	if (!g_ContinuityMgr.HighSpeedMovement())
	{
		cullShape.EnableLodRange( LODTYPES_DEPTH_LOD );
	}

	cullShape.EnableLodRange( LODTYPES_DEPTH_SLOD1 );
	cullShape.EnableLodRange( LODTYPES_DEPTH_SLOD2 );
	cullShape.EnableLodRange( LODTYPES_DEPTH_SLOD3 );
	cullShape.EnableLodRange( LODTYPES_DEPTH_SLOD4 );
	cullShape.SetSphere( sphere );
}


//
// name:	SetupVelocityVectorStreamer
// purpose:	due to the lack of full spherical scene streaming on current gen systens, there are times
//			when it is desirable to request map LOD entities in a frustum in the direction of movement
//			(e.g. when looking backwards and flying a plane).
//
void CGameScan::SetupVelocityVectorStreamer(const CRenderPhaseScanned* gbufPhase, u32 renderPhaseId)
{
	const Vector3 vPlayerPos = CGameWorld::FindFollowPlayerCoors();
	const Vector3 vPlayerVelocity = CGameWorld::FindFollowPlayerSpeed();

	if (vPlayerVelocity.Mag2() <= 1.0f)
		return;

	//////////////////////////////////////////////////////////////////////////

#if 0	//DEBUG DRAW FRUSTUM
	Matrix34 matFrustum;
	camFrame::ComputeWorldMatrixFromFront(vPlayerVelocity, matFrustum);
	matFrustum.d = vPlayerPos;

	CFrustumDebug dbgFrustum(matFrustum, 0.50f, 600.0f, 50.0f, 1.0f, 1.0f);
	dbgFrustum.DebugRender(&dbgFrustum, Color32(64, 128, 255, 80));

#endif

	//////////////////////////////////////////////////////////////////////////

	fwRenderPhaseCullShape&		cullShape = fwScan::AddCullShape();
	cullShape.SetLodMask(fwRenderPhase::LODMASK_NONE);
	cullShape.ClearAllFlags();

	{
		spdTransposedPlaneSet8	frustum;
		grcViewport				viewport = gbufPhase->GetGrcViewport();
		const Vec3V				from = VECTOR3_TO_VEC3V( vPlayerPos );
		const Vec3V				to = from - VECTOR3_TO_VEC3V( vPlayerVelocity );
		Mat34V					camMtx;

		Vector3 down(0.0f, 0.0f, -1.0f);
		Vector3 forward = vPlayerVelocity;
		forward.Normalize();

		float cameraForwardDotDown = forward.Dot(down);

		const float almostOne = 0.99f;
		Vec3V up = ( cameraForwardDotDown > almostOne ) ? Vec3V(V_Y_AXIS_WZERO) : Vec3V(V_Z_AXIS_WZERO);

		LookAt( camMtx, from, to, up);

		viewport.SetWorldIdentity();
		viewport.SetCameraMtx( camMtx );

		viewport.SetFarClip( 600.0f );
		frustum.Set( viewport );

		cullShape.SetFlags( CULL_SHAPE_FLAG_GEOMETRY_FRUSTUM );

		cullShape.SetScreenQuadStorageId( SCAN_SCREENQUAD_STORAGE_STREAMING );
		cullShape.SetCompositeMatrix( viewport.GetCompositeMtx() );
		cullShape.SetCameraPosition( from );
		cullShape.SetFrustum( frustum );
	}

	cullShape.SetFlags(
		CULL_SHAPE_FLAG_RENDER_EXTERIOR				|
		CULL_SHAPE_FLAG_USE_LOD_RANGES_IN_EXTERIOR	|
		CULL_SHAPE_FLAG_USE_LOD_RANGES_IN_INTERIOR	|
		CULL_SHAPE_FLAG_STREAM_ENTITIES				|
		CULL_SHAPE_FLAG_ONLY_STREAM_ENTITIES		|
		0);

	fwSceneGraphNode*	startingSceneNode = fwWorldRepMulti::GetSceneGraphRoot();

	cullShape.SetRenderPhaseId( renderPhaseId );
	cullShape.SetStartingSceneNode( startingSceneNode );
	cullShape.SetEntityVisibilityMask( VIS_ENTITY_MASK_ANIMATED_BUILDING | VIS_ENTITY_MASK_LOLOD_BUILDING );
	cullShape.SetVisibilityType( VIS_PHASE_STREAMING );

	//////////////////////////////////////////////////////////////////////////
	// only require LODs - everything else should be streamed spherically under normal circumstances
	cullShape.InvalidateLodRanges();
	cullShape.EnableLodRange(LODTYPES_DEPTH_LOD);
	//////////////////////////////////////////////////////////////////////////
}


#endif		//!FULL_SPHERICAL_SCENE_STREAMING


//
// name:	SetupStreamingVolumeCullShapes
// purpose:	sets up cull shape (sphere or frustum) required for loading a remote scene using streaming
//			volumes.
//
void CGameScan::SetupStreamingVolumeCullShapes(const CRenderPhaseScanned* gbufPhase, s32 volumeSlot, u32 renderPhaseId)
{
	CStreamVolume& volume = CStreamVolumeMgr::GetVolume(volumeSlot);
	Assert(volume.IsActive());

	volume.CheckForValidResults();

	if (!volume.UsesSceneStreaming() || volume.IsTypeLine())		// ray searches not supported in visibility at present
		return;

	fwRenderPhaseCullShape&		cullShape = fwScan::AddCullShape();
	cullShape.SetLodMask(fwRenderPhase::LODMASK_NONE);
	cullShape.ClearAllFlags();

	if ( volume.IsTypeSphere() )
	{
		const spdSphere&	sphere = volume.GetSphere();

		cullShape.SetFlags( CULL_SHAPE_FLAG_GEOMETRY_SPHERE );
		cullShape.SetCameraPosition( sphere.GetCenter() );
		cullShape.SetSphere( sphere );
	}
	else if ( volume.IsTypeFrustum() )
	{
		spdTransposedPlaneSet8	frustum;
		grcViewport				viewport = gbufPhase->GetGrcViewport();
		const Vec3V				from = volume.GetPos();
		const Vec3V				to = from - volume.GetDir();
		Mat34V					camMtx;

		Vector3 down(0.0f, 0.0f, -1.0f);
		Vector3 forward = VEC3V_TO_VECTOR3(volume.GetDir());
		forward.Normalize();

		float cameraForwardDotDown = forward.Dot(down);

		const float almostOne = 0.99f;
		Vec3V up = ( cameraForwardDotDown > almostOne ) ? Vec3V(V_Y_AXIS_WZERO) : Vec3V(V_Z_AXIS_WZERO);

		LookAt( camMtx, from, to, up);

		viewport.SetWorldIdentity();
		viewport.SetCameraMtx( camMtx );

		if (volume.UsesCameraMtx())
		{
			viewport.SetCameraMtx(volume.GetCameraMtx());
		}

		viewport.SetFarClip( volume.GetFarClip() );
		frustum.Set( viewport );

		cullShape.SetFlags( CULL_SHAPE_FLAG_GEOMETRY_FRUSTUM );

		cullShape.SetScreenQuadStorageId( SCAN_SCREENQUAD_STORAGE_STREAMING );
		cullShape.SetCompositeMatrix( viewport.GetCompositeMtx() );
		cullShape.SetCameraPosition( from );
		cullShape.SetFrustum( frustum );
	}
	else
	{
		Assertf( false, "Unsupported stream volume type" );
	}

	cullShape.SetFlags( CULL_SHAPE_FLAG_RENDER_EXTERIOR		|
		CULL_SHAPE_FLAG_RENDER_INTERIOR				|
		CULL_SHAPE_FLAG_TRAVERSE_PORTALS			|
		CULL_SHAPE_FLAG_USE_LOD_RANGES_IN_EXTERIOR	|
		CULL_SHAPE_FLAG_USE_LOD_RANGES_IN_INTERIOR	|
		CULL_SHAPE_FLAG_STREAM_ENTITIES				|
		CULL_SHAPE_FLAG_ONLY_STREAM_ENTITIES		|
		CULL_SHAPE_FLAG_TRAVERSE_DISABLED_PORTALS	|
		0);

	CInteriorProxy*		pInteriorProxy = g_LoadScene.GetInteriorProxy();
	CInteriorInst*		interiorInst = pInteriorProxy ? pInteriorProxy->GetInteriorInst() : NULL;
	s32					roomIdx = g_LoadScene.GetRoomIdx();
	fwSceneGraphNode*	startingSceneNode;

	if ( interiorInst && roomIdx != -1 && interiorInst->CanReceiveObjects())
		startingSceneNode = interiorInst->GetRoomSceneGraphNode( roomIdx );
	else
		startingSceneNode = fwWorldRepMulti::GetSceneGraphRoot();

	cullShape.SetRenderPhaseId( renderPhaseId );
	cullShape.SetStartingSceneNode( startingSceneNode );

#if KEEP_INSTANCELIST_ASSETS_RESIDENT
	cullShape.SetEntityVisibilityMask( VIS_ENTITY_MASK_OBJECT | VIS_ENTITY_MASK_DUMMY_OBJECT | VIS_ENTITY_MASK_ANIMATED_BUILDING | VIS_ENTITY_MASK_HILOD_BUILDING | VIS_ENTITY_MASK_LOLOD_BUILDING | VIS_ENTITY_MASK_TREE );
#else
	cullShape.SetEntityVisibilityMask( VIS_ENTITY_MASK_OBJECT | VIS_ENTITY_MASK_DUMMY_OBJECT | VIS_ENTITY_MASK_ANIMATED_BUILDING | VIS_ENTITY_MASK_HILOD_BUILDING | VIS_ENTITY_MASK_LOLOD_BUILDING | VIS_ENTITY_MASK_TREE | VIS_ENTITY_MASK_INSTANCELIST );
#endif

	cullShape.SetVisibilityType( VIS_PHASE_STREAMING );

	//////////////////////////////////////////////////////////////////////////
	// enable lod ranges per level as required
	cullShape.InvalidateLodRanges();
	for (s32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
	{
		if (volume.RequiresLodLevel(i))
		{
			cullShape.EnableLodRange(i);
		}
	}
	//////////////////////////////////////////////////////////////////////////

}


//
// name:	SetupInteriorStreamCullShape
// purpose:	sets up a small spherical cullshape to request nearby interior entities
//
void CGameScan::SetupInteriorStreamCullShape(const CRenderPhaseScanned* gbufPhase)
{
	fwRenderPhaseCullShape&		cullShape = fwScan::AddCullShape();
	cullShape.SetLodMask(fwRenderPhase::LODMASK_NONE);

	Vec3V						interiorStreamPos = g_SceneStreamerMgr.GetIntStreamSphereCentre();
	fwInteriorLocation			interiorStreamLocation = CGameWorld::FindLocalPlayer()->GetInteriorLocation();
	float						fRadius = g_SceneStreamerMgr.GetIntStreamSphereRadius();

	//////////////////////////////////////////////////////////////////////////
	// repurpose interior streaming sphere for SRL playback with interior hinting
	if (gStreamingRequestList.IsPlaybackActive() && gStreamingRequestList.HasValidInteriorCutHint())
	{
		interiorStreamPos = gStreamingRequestList.GetInteriorCutHintPos();
		interiorStreamLocation = gStreamingRequestList.GetInteriorCutHint();
		fRadius = 40.0f;
	}
	//////////////////////////////////////////////////////////////////////////

	spdSphere					sphere( interiorStreamPos, ScalarV( fRadius ) );

	cullShape.ClearAllFlags();
	cullShape.SetFlags(
		CULL_SHAPE_FLAG_GEOMETRY_SPHERE				|
		CULL_SHAPE_FLAG_TRAVERSE_PORTALS			|
		CULL_SHAPE_FLAG_RENDER_INTERIOR				|
		CULL_SHAPE_FLAG_USE_LOD_RANGES_IN_INTERIOR	|
		CULL_SHAPE_FLAG_STREAM_ENTITIES				|
		CULL_SHAPE_FLAG_ONLY_STREAM_ENTITIES		|
		CULL_SHAPE_FLAG_TRAVERSE_DISABLED_PORTALS	|
		0);

	CInteriorInst*		interiorInst = CInteriorInst::GetInteriorForLocation(interiorStreamLocation);
	s32					roomIdx = interiorStreamLocation.GetRoomIndex();

	fwSceneGraphNode*	startingSceneNode = NULL;
	if ( interiorInst && roomIdx != -1 )
		startingSceneNode = interiorInst->GetRoomSceneGraphNode( roomIdx );
	else
		startingSceneNode = fwWorldRepMulti::GetSceneGraphRoot();

	cullShape.SetRenderPhaseId( CGameScan::GetInteriorStreamVisibilityBit() );
	cullShape.SetCompositeMatrix( gbufPhase->GetGrcViewport().GetCompositeMtx() );
	cullShape.SetCameraPosition( interiorStreamPos );
	cullShape.SetStartingSceneNode( startingSceneNode );
	cullShape.SetEntityVisibilityMask( VIS_ENTITY_MASK_OBJECT | VIS_ENTITY_MASK_DUMMY_OBJECT | VIS_ENTITY_MASK_ANIMATED_BUILDING | VIS_ENTITY_MASK_HILOD_BUILDING );
	cullShape.SetVisibilityType( VIS_PHASE_STREAMING );
	cullShape.InvalidateLodRanges();
	cullShape.EnableLodRange( LODTYPES_DEPTH_HD );
	cullShape.EnableLodRange( LODTYPES_DEPTH_ORPHANHD );
	cullShape.SetSphere( sphere );
}
