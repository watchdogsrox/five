// =====================================================
// renderer/RenderPhases/RenderPhaseMirrorReflection.cpp
// (c) 2011 RockstarNorth
// =====================================================

// Rage headers
#include "profile/timebars.h"
#include "grmodel/shaderfx.h"
#include "grcore/debugdraw.h"
#include "grcore/stateblock.h"
#include "grcore/stateblock_internal.h"
#include "grcore/quads.h"
#include "system/memops.h"
#include "system/param.h"
#if RSG_ORBIS
#include "grcore/texturefactory_gnm.h"
#endif

// Framework Headers
#include "fwmaths/vector.h"
#include "fwmaths/vectorutil.h"
#include "fwmaths/PortalCorners.h"
#include "fwrenderer/renderlistgroup.h"
#include "fwscene/scan/RenderPhaseCullShape.h"
#include "fwscene/scan/Scan.h"
#include "fwscene/world/InteriorLocation.h"
#include "fwscene/world/InteriorSceneGraphNode.h"
#include "fwsys/timer.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportManager.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "debug/TiledScreenCapture.h"
#include "game/clock.h"
#include "modelinfo/MloModelInfo.h"
#include "peds/Ped.h"
#include "peds/rendering/PedDamage.h"
#include "profile/profiler.h"
#include "renderer/DrawLists/drawlistMgr.h"
#include "renderer/Lights/lights.h"
#include "renderer/Lights/LightGroup.h"
#include "renderer/Mirrors.h"
#include "renderer/occlusion.h"
#include "renderer/PostProcessFX.h"
#include "renderer/renderListGroup.h"
#include "renderer/RenderListBuilder.h"
#include "renderer/RenderPhases/RenderPhaseMirrorReflection.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/rendertargets.h"
#include "renderer/water.h"
#include "scene/portals/FrustumDebug.h"
#include "scene/portals/Portal.h"
#include "scene/scene.h"
#include "shaders/CustomShaderEffectMirror.h"
#include "timecycle/TimeCycle.h"
#include "Vfx/Sky/Sky.h"
#include "Vfx/GPUParticles/PtFxGPUManager.h"
#include "Vfx/particles/PtFxManager.h"
#include "vfx/VfxHelper.h"
#include "Vfx/VisualEffects.h"

EXT_PF_GROUP(BuildRenderList);
PF_TIMER(CRenderPhaseMirrorReflectionRL,BuildRenderList);

EXT_PF_GROUP(RenderPhase);
PF_TIMER(CRenderPhaseMirrorReflection,RenderPhase);

// ================================================================================================

static u32 g_mirrorReflectionRenderSettings =
	RENDER_SETTING_RENDER_PARTICLES          |
//	RENDER_SETTING_RENDER_WATER              |
//	RENDER_SETTING_STORE_WATER_OCCLUDERS     |
	RENDER_SETTING_RENDER_ALPHA_POLYS        |
//	RENDER_SETTING_RENDER_SHADOWS            | // mirrors are generally in deep interiors (so they require neither directional light nor shadow)
	RENDER_SETTING_RENDER_DECAL_POLYS        | // allow ped decals
	RENDER_SETTING_RENDER_CUTOUT_POLYS       |
//	RENDER_SETTING_RENDER_UI_EFFECTS         |
//	RENDER_SETTING_ALLOW_PRERENDER           |
	RENDER_SETTING_RENDER_DISTANT_LIGHTS     |
	RENDER_SETTING_RENDER_DISTANT_LOD_LIGHTS |
	0;

static u32 g_mirrorReflectionVisMaskFlags =
//	VIS_ENTITY_MASK_OTHER             |
	VIS_ENTITY_MASK_PED               |
	VIS_ENTITY_MASK_OBJECT            |
//	VIS_ENTITY_MASK_DUMMY_OBJECT      |
	VIS_ENTITY_MASK_VEHICLE           |
	VIS_ENTITY_MASK_ANIMATED_BUILDING |
//	VIS_ENTITY_MASK_LOLOD_BUILDING    | // interiors don't need LOLOD
	VIS_ENTITY_MASK_HILOD_BUILDING    |
	VIS_ENTITY_MASK_LIGHT             |
//	VIS_ENTITY_MASK_TREE              | // interiors don't have trees
	0;

static u32 g_mirrorReflectionAlphaBlendingPasses =
//	BIT(RPASS_VISIBLE) |
//	BIT(RPASS_LOD    ) |
	BIT(RPASS_CUTOUT ) | // alpha blend if we disable alpha test for this pass in rag
	BIT(RPASS_DECAL  ) | // allow ped decals
	BIT(RPASS_FADING ) |
	BIT(RPASS_ALPHA  ) |
//	BIT(RPASS_WATER  ) | // not applicable
	BIT(RPASS_TREE   ) | // applicable if we're rendering exteriors ..
	0;

static u32 g_mirrorReflectionDepthWritingPasses =
	BIT(RPASS_VISIBLE) |
	BIT(RPASS_LOD    ) |
	BIT(RPASS_CUTOUT ) |
//	BIT(RPASS_DECAL  ) | // not applicable
//	BIT(RPASS_FADING ) |
//	BIT(RPASS_ALPHA  ) |
//	BIT(RPASS_WATER  ) | // not applicable
//	BIT(RPASS_TREE   ) |
	0;

static bool g_mirrorReflectionEnableMSAA = true;

static CRenderPhaseScanned* g_mirrorRenderPhase         = NULL;
static grcViewport*         g_mirrorReflectionViewport  = NULL;
#if __PS3
static grcViewport          g_mirrorReflectionViewportNoObliqueProjection(grcViewport::DoNotInitialize);
#endif // __PS3
static bool                 g_mirrorReflectionUsingWaterReflectionTechnique = false;
//thread local copy of water reflection technique state. Because we have many renderthreads and we need to make sure that 
//it's initialized to false
DECLARE_MTR_THREAD static bool	g_mirrorReflectionUsingWaterReflectionTechniqueRT = false;
DECLARE_MTR_THREAD static bool	g_mirrorReflectionUsingWaterSurfaceRT = false;

CRenderPhaseMirrorReflection::CRenderPhaseMirrorReflection(CViewport* pGameViewport) : CRenderPhaseScanned(pGameViewport, "Mirror Reflection", DL_RENDERPHASE_MIRROR_REFLECTION, 0.0f)
#if __BANK
, m_debugDrawMirror(false)
, m_debugDrawMirrorOffset(0.01f)
#endif // __BANK
{
	g_mirrorRenderPhase = this;

	SetRenderFlags(g_mirrorReflectionRenderSettings);
	GetEntityVisibilityMask().SetAllFlags((u16)g_mirrorReflectionVisMaskFlags);
	SetVisibilityType(VIS_PHASE_MIRROR_REFLECTION);
	SetEntityListIndex(gRenderListGroup.RegisterRenderPhase(RPTYPE_REFLECTION, this));
	CMirrors::SetMirrorRenderPhase(this);
	CCustomShaderEffectMirror::Initialise();
}

CRenderPhaseMirrorReflection::~CRenderPhaseMirrorReflection()
{
	CMirrors::SetMirrorRenderPhase(NULL);
}

u32 CRenderPhaseMirrorReflection::GetCullFlags() const
{
#if __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		return 0;
	}
#endif // __BANK

	if (!CMirrors::GetIsMirrorVisible())
	{
		return 0;
	}

	u32 flags =
		CULL_SHAPE_FLAG_RENDER_INTERIOR  |
		CULL_SHAPE_FLAG_GEOMETRY_FRUSTUM |
		CULL_SHAPE_FLAG_MIRROR;

	const CPortalFlags portalFlags = CMirrors::GetMirrorPortalFlags();

	bool bIsMirrorFloor       = portalFlags.GetIsMirrorFloor();
	bool bUsePortalTraversal  = portalFlags.GetIsMirrorUsingPortalTraversal() || bIsMirrorFloor;
	bool bCanSeeExteriorView  = fwScan::GetScanResults().GetMirrorCanSeeExteriorView();

	CutSceneManager* csMgr = CutSceneManager::GetInstance();
	Assert(csMgr);
	if(csMgr->IsCutscenePlayingBack())
	{
		bCanSeeExteriorView = true;
	}

#if MIRROR_DEV
	if      (CMirrors::ms_debugMirrorForcePortalTraversalOn)  { bUsePortalTraversal = true; }
	else if (CMirrors::ms_debugMirrorForcePortalTraversalOff) { bUsePortalTraversal = false; }

	if      (CMirrors::ms_debugMirrorForceExteriorViewOn)  { bCanSeeExteriorView = true; }
	else if (CMirrors::ms_debugMirrorForceExteriorViewOff) { bCanSeeExteriorView = false; }

	if (CMirrors::ms_debugMirrorFloor)
	{
		bIsMirrorFloor = true;
	}
#endif // MIRROR_DEV

	if (bUsePortalTraversal)
	{
		flags |= CULL_SHAPE_FLAG_TRAVERSE_PORTALS;

		if (MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorClipAndStorePortals, false)) // TODO -- re-enable this once BS#992094 is resolved
		{
			flags |= (CULL_SHAPE_FLAG_CLIP_AND_STORE_PORTALS | CULL_SHAPE_FLAG_USE_SCREEN_QUAD_PAIRS);
		}
	}

	if (bCanSeeExteriorView)
	{
		flags |= CULL_SHAPE_FLAG_RENDER_EXTERIOR;

		if (MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorExteriorViewRenderLODs, false))
		{
			flags |= CULL_SHAPE_FLAG_RENDER_LOD_BUILDING_BASED_ON_INTERIOR;
		}
	}

	return flags;
}

fwRoomSceneGraphNode* CRenderPhaseMirrorReflection::GetMirrorRoomSceneNode()
{
	if (CMirrors::GetIsMirrorVisible())
	{
		return CMirrors::GetScanMirrorRoomSceneNode();
	}

	return NULL;
}

// TODO -- ugly hack!!
static bool gMirrorScanCameraIsInInterior = false;

void CRenderPhaseMirrorReflection::SetMirrorScanCameraIsInInterior(bool b)
{
	gMirrorScanCameraIsInInterior = b;
}

bool CRenderPhaseMirrorReflection::GetMirrorScanFromGBufStartingNode()
{
	bool bFromGBuf = false;

	if (fwScan::GetScanResults().GetMirrorFloorVisible())
	{
		// mirror floors start scanning from gbuf node if the camera is in an interior or the portal is water surface
		// TODO -- maybe we should just always set bFromGBuf to true if a mirror floor is visible?
		if (gMirrorScanCameraIsInInterior || Water::IsUsingMirrorWaterSurface())
		{
			bFromGBuf = true;
		}

#if MIRROR_DEV
		if      (CMirrors::ms_debugMirrorFloorScanFromGBufStartingNodeOn ) { bFromGBuf = true; }
		else if (CMirrors::ms_debugMirrorFloorScanFromGBufStartingNodeOff) { bFromGBuf = false; }
#endif // MIRROR_DEV
	}

	return bFromGBuf;
}

#if MIRROR_DEV
static float gMirrorScreenBoundsX = 0.0f;
static float gMirrorScreenBoundsY = 0.0f;
static float gMirrorScreenBoundsW = 0.0f;
static float gMirrorScreenBoundsH = 0.0f;
#endif // MIRROR_DEV

static bool gMirrorAllowRestrictedProjection = true; // [HACK GTAV]

void CRenderPhaseMirrorReflection::UpdateViewport()
{
	g_mirrorReflectionViewport = NULL;
#if __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		return;
	}
#endif // __BANK

#if MIRROR_DEV
	if (CMirrors::ms_debugMirrorShowInfo)
	{
		grcDebugDraw::AddDebugOutput(CMirrors::ms_debugMirrorShowInfoMessage);
	}

	if (fwScan::GetScanResults().GetWaterSurfaceHeightMismatch())
	{
		grcDebugDraw::AddDebugOutput(Color32(fwTimer::GetFrameCount()%4?255:0,0,0,255), "INTERIOR WATER HEIGHT MISMATCH");
	}
#endif // MIRROR_DEV

	if (!CMirrors::GetIsMirrorVisible())
	{
		return;
	}

	SetActive(true);

	Vec3V mirrorCorners[4];

	mirrorCorners[0] = CMirrors::GetScanMirrorCorner(0);
	mirrorCorners[1] = CMirrors::GetScanMirrorCorner(1);
	mirrorCorners[2] = CMirrors::GetScanMirrorCorner(2);
	mirrorCorners[3] = CMirrors::GetScanMirrorCorner(3);

#if MIRROR_DEV
	if (CMirrors::ms_debugMirrorShowInfo)
	{
		grcDebugDraw::AddDebugOutput("    distance=%f, plane=%.3f,%.3f,%.3f,%.3f", fwScan::GetScanResults().GetMirrorDistance(), VEC4V_ARGS(CMirrors::GetMirrorPlane()));

		if (fwScan::GetScanResults().GetMirrorFloorVisible())
		{
			grcDebugDraw::AddDebugOutput("    floor quads=%d, water surface=%s", fwScan::GetScanResults().GetMirrorFloorQuadCount(), fwScan::GetScanResults().GetWaterSurfaceVisible() ? "true" : "false");
		}

		if (fwScan::GetScanResults().GetMirrorCanSeeDirectionalLight())
		{
			grcDebugDraw::AddDebugOutput("    can see directional light");
		}

		if (fwScan::GetScanResults().GetMirrorCanSeeExteriorView())
		{
			grcDebugDraw::AddDebugOutput("    can see exterior view");
		}

		if (fwScan::GetScanResults().m_skyPortalBounds_MirrorReflection.GetIsSkyPortalVisInterior())
		{
			grcDebugDraw::AddDebugOutput("    sky portals=%d", fwScan::GetScanResults().m_skyPortalBounds_MirrorReflection.GetSkyPortalCount());
		}
	}
#endif // MIRROR_DEV

	// set location (room and portal info)
	{
		const fwRoomSceneGraphNode* pMirrorRoomSceneNode = GetMirrorRoomSceneNode();

		if (pMirrorRoomSceneNode)
		{
			const fwInteriorLocation location = pMirrorRoomSceneNode->GetInteriorLocation();
			const CInteriorProxy* pInteriorProxy = CInteriorProxy::GetFromLocation(location);
			Assert(pInteriorProxy);
			const CInteriorInst* pInteriorInst = pInteriorProxy->GetInteriorInst();
			const CMloModelInfo* pMloModelInfo = static_cast<const CMloModelInfo*>(pInteriorInst->GetBaseModelInfo());
			const int portalIndex = CMirrors::GetScanMirrorPortalIndex();
			Assert(pMloModelInfo->GetIsStreamType());
			const CPortalFlags portalFlags = pMloModelInfo->GetPortalFlags(portalIndex);
			Assert(portalFlags.GetIsMirrorPortal());
			Assert(portalFlags.GetIsOneWayPortal());
#if MIRROR_DEV
			if (CMirrors::ms_debugMirrorShowInfo)
			{
				char portalFlagsString[32] = "";

				// every f-ing portal flag
				if (portalFlags.GetIsOneWayPortal              ()) { strcat(portalFlagsString, "O"); }
				if (portalFlags.GetIsLinkPortal                ()) { strcat(portalFlagsString, "L"); }
				if (portalFlags.GetIsMirrorPortal              ()) { strcat(portalFlagsString, "M"); }
				if (portalFlags.GetMirrorCanSeeDirectionalLight()) { strcat(portalFlagsString, "D"); }
				if (portalFlags.GetMirrorCanSeeExteriorView    ()) { strcat(portalFlagsString, "E"); }
				if (portalFlags.GetIsMirrorUsingPortalTraversal()) { strcat(portalFlagsString, "P"); }
				if (portalFlags.GetIsMirrorFloor               ()) { strcat(portalFlagsString, "F"); }
				if (portalFlags.GetIsWaterSurface              ()) { strcat(portalFlagsString, "W"); }
				if (portalFlags.GetIgnoreModifier              ()) { strcat(portalFlagsString, "I"); }
				if (portalFlags.GetLowLODOnly                  ()) { strcat(portalFlagsString, "X"); }
				if (portalFlags.GetAllowClosing                ()) { strcat(portalFlagsString, "C"); }

				grcDebugDraw::AddDebugOutput(
					"    interior=%s, room=%s [%s]%s",
					pMloModelInfo->GetModelName(),
					pInteriorInst->GetRoomName(location.GetRoomIndex()),
					portalFlagsString,
					GetMirrorScanFromGBufStartingNode() ? " - scanning from gbuf node" : ""
				);
			}
#endif // MIRROR_DEV

			CMirrors::SetMirrorPortalFlags(portalFlags);
		}
		else
		{
#if MIRROR_DEV
			if (CMirrors::ms_debugMirrorShowInfo)
			{
				grcDebugDraw::AddDebugOutput("    no interior room node found");
			}
#endif // MIRROR_DEV

			CMirrors::SetMirrorPortalFlags(CPortalFlags(0));
		}
	}

	const Vec3V mirrorNormal = Normalize(Cross(mirrorCorners[0] - mirrorCorners[1], mirrorCorners[2] - mirrorCorners[1]));
	const Vec3V mirrorOffset = mirrorNormal*ScalarV(MIRROR_DEFAULT_OFFSET MIRROR_DEV_ONLY(+ CMirrors::ms_debugMirrorOffset));

	mirrorCorners[0] += mirrorOffset;
	mirrorCorners[1] += mirrorOffset;
	mirrorCorners[2] += mirrorOffset;
	mirrorCorners[3] += mirrorOffset;

#if MIRROR_DEV
	if (CMirrors::ms_debugMirrorScale != 0.0f)
	{
		const Vec3V centre = (mirrorCorners[0] + mirrorCorners[1] + mirrorCorners[2] + mirrorCorners[3])*ScalarV(V_QUARTER);

		mirrorCorners[0] += (mirrorCorners[0] - centre)*ScalarV(CMirrors::ms_debugMirrorScale - 1.0f);
		mirrorCorners[1] += (mirrorCorners[1] - centre)*ScalarV(CMirrors::ms_debugMirrorScale - 1.0f);
		mirrorCorners[2] += (mirrorCorners[2] - centre)*ScalarV(CMirrors::ms_debugMirrorScale - 1.0f);
		mirrorCorners[3] += (mirrorCorners[3] - centre)*ScalarV(CMirrors::ms_debugMirrorScale - 1.0f);
	}

	mirrorCorners[0] += CMirrors::ms_debugMirrorOffsetV;
	mirrorCorners[1] += CMirrors::ms_debugMirrorOffsetV;
	mirrorCorners[2] += CMirrors::ms_debugMirrorOffsetV;
	mirrorCorners[3] += CMirrors::ms_debugMirrorOffsetV;
#endif // MIRROR_DEV

	// [HACK GTAV] hack for BS#1574691 - no restricted projection on mirror during cutscene 'arm_1_mcs_2_concat' (to avoid green/yellow artefacts on 360)
	if (CutSceneManager::GetInstance() &&
		CutSceneManager::GetInstance()->IsRunning() &&
		CutSceneManager::GetInstance()->GetSceneHashString()->GetHash() == ATSTRINGHASH("arm_1_mcs_2_concat", 0x5BCF8C95))
	{
		gMirrorAllowRestrictedProjection = false;
	}
	else
	{
		gMirrorAllowRestrictedProjection = true;
	}

	const bool bIsUsingMirrorWaterSurface = Water::IsUsingMirrorWaterSurface();
	const bool bIsUsingRestrictedProjection	= gMirrorAllowRestrictedProjection && !bIsUsingMirrorWaterSurface MIRROR_DEV_ONLY(&& CMirrors::ms_debugMirrorRestrictedProjection);

	const Vec4V mirrorPlane = BuildPlane(mirrorCorners[0], mirrorNormal);

	CMirrors::SetMirrorCorners(mirrorCorners);
	CMirrors::SetMirrorPlane(mirrorPlane);

	// ===

	CRenderPhaseScanned::UpdateViewport();
	grcViewport& viewport = GetGrcViewport();

	Vec2V mirrorBoundsMin;
	Vec2V mirrorBoundsMax;

	// compute local bounds
	{
		const Mat34V localToWorld = viewport.GetCameraMtx();
		const Vec2V invTanFOV = Vec2V(1.0f/viewport.GetTanHFOV(), 1.0f/viewport.GetTanVFOV());

		Vec3V local[4 + 5];
		int   localCount = 0;

#if MIRROR_DEV
		if (CMirrors::ms_debugMirrorClipMode != CMirrors::MIRROR_CLIP_MODE_VIEWSPACE)
		{
			local[localCount++] = mirrorCorners[0];
			local[localCount++] = mirrorCorners[1];
			local[localCount++] = mirrorCorners[2];
			local[localCount++] = mirrorCorners[3];
		}
		else
#endif // MIRROR_DEV
		{
			localCount = PolyClipToFrustumWithoutNearPlane(local, mirrorCorners, 4, viewport.GetCompositeMtx());
		}

		if (localCount >= 3)
		{
			mirrorBoundsMin = Vec2V(V_FLT_MAX);
			mirrorBoundsMax = Vec2V(V_NEG_FLT_MAX);

			for (int i = 0; i < localCount; i++)
			{
				const Vec3V temp = UnTransformOrtho(localToWorld, local[i]);
				const Vec2V xy = -temp.GetXY()/temp.GetZ();

				mirrorBoundsMin = Min(xy, mirrorBoundsMin);
				mirrorBoundsMax = Max(xy, mirrorBoundsMax);
			}
		}
		else // mirror is clipped to nothing, it shouldn't be drawn .. force the bounds to 0
		{
			mirrorBoundsMin = Vec2V(V_ZERO);
			mirrorBoundsMax = Vec2V(V_ZERO);
		}

		// [-1..1] in screenspace
		mirrorBoundsMin *= invTanFOV;
		mirrorBoundsMax *= invTanFOV;

#if MIRROR_DEV
		if (CMirrors::ms_debugMirrorClipMode == CMirrors::MIRROR_CLIP_MODE_SCREENSPACE)
		{
			mirrorBoundsMin = Max(mirrorBoundsMin, Vec2V(V_NEGONE));
			mirrorBoundsMax = Min(mirrorBoundsMax, Vec2V(V_ONE));
		}
#endif // MIRROR_DEV

		// make sure the bounds isn't actually zero, otherwise we can get shader errors
		{
			mirrorBoundsMax = Max(mirrorBoundsMin + Vec2V(V_FLT_SMALL_5), mirrorBoundsMax);
			mirrorBoundsMin = Min(mirrorBoundsMax - Vec2V(V_FLT_SMALL_5), mirrorBoundsMin);
		}
	}

#if MIRROR_DEV
	// [0..1]
	const Vec2V screenBoundsMin = Vec2V(V_HALF) + Vec2V(0.5f, -0.5f)*mirrorBoundsMin;
	const Vec2V screenBoundsMax = Vec2V(V_HALF) + Vec2V(0.5f, -0.5f)*mirrorBoundsMax;

	// compute screen bounds
	{
		const float x0 = screenBoundsMin.GetXf();
		const float y0 = screenBoundsMin.GetYf();
		const float x1 = screenBoundsMax.GetXf();
		const float y1 = screenBoundsMax.GetYf();

		gMirrorScreenBoundsX = (x0     )*(float)GRCDEVICE.GetWidth ();
		gMirrorScreenBoundsY = (y1     )*(float)GRCDEVICE.GetHeight(); // flip y
		gMirrorScreenBoundsW = (x1 - x0)*(float)GRCDEVICE.GetWidth ();
		gMirrorScreenBoundsH = (y0 - y1)*(float)GRCDEVICE.GetHeight();
	}

	if (CMirrors::ms_debugMirrorShowInfo)
	{
		grcDebugDraw::AddDebugOutput(
			"    screen bounds x=%d, y=%d, w=%d, h=%d",
			(int)gMirrorScreenBoundsX,
			(int)gMirrorScreenBoundsY,
			(int)gMirrorScreenBoundsW,
			(int)gMirrorScreenBoundsH
		);
	}

	if (CMirrors::ms_debugMirrorShowBounds)
	{
		grcDebugDraw::RectAxisAligned(screenBoundsMin, screenBoundsMax, Color32(255,0,0,32), true);
		grcDebugDraw::RectAxisAligned(screenBoundsMin, screenBoundsMax, Color32(255,0,0,255), false);
	}

	if (CMirrors::ms_debugMirrorShowBoundsRT)
	{
		CRenderTargets::ShowRenderTargetSetBounds(gMirrorScreenBoundsX, gMirrorScreenBoundsY, gMirrorScreenBoundsW, gMirrorScreenBoundsH);
	}
#endif // MIRROR_DEV

#if DEBUG_DRAW
	if (m_debugDrawMirror)
	{
		const Vec3V offset = mirrorNormal*ScalarV(m_debugDrawMirrorOffset); // mirror already renders at MIRROR_DEFAULT_OFFSET, so offset a bit more to make it visible

		grcDebugDraw::Quad(
			mirrorCorners[0] + offset,
			mirrorCorners[1] + offset,
			mirrorCorners[2] + offset,
			mirrorCorners[3] + offset,
			Color_BlueViolet.MultiplyAlpha(64),
			false, // double-sided
			true // solid
		);
		grcDebugDraw::Quad(
			mirrorCorners[0] + offset,
			mirrorCorners[1] + offset,
			mirrorCorners[2] + offset,
			mirrorCorners[3] + offset,
			Color_BlueViolet,
			false, // double-sided
			false // solid
		);
	}
#endif // DEBUG_DRAW

	// reflect camera matrix
	{
		Mat34V cameraMtx = PlaneReflectMatrix(mirrorPlane, viewport.GetCameraMtx());

#if MIRROR_USE_STENCIL_MASK
		if (!CMirrors::ms_debugMirrorStencilMaskEnabled)
#endif // MIRROR_USE_STENCIL_MASK
		{
			cameraMtx.SetCol0(-cameraMtx.GetCol0()); // flip x
		}

		viewport.SetCameraMtx(cameraMtx);
	}

	viewport.SetNearClip(0.1f);

#if MIRROR_USE_STENCIL_MASK
	if (CMirrors::ms_debugMirrorStencilMaskEnabled)
	{
		viewport.SetNormWindow(0.0f, 0.0f, 1.0f, 1.0f);
		CMirrors::SetMirrorBounds(0.5f, 0.5f, -0.5f, -0.5f);
	}
	else
#endif // MIRROR_USE_STENCIL_MASK
	if (bIsUsingRestrictedProjection)
	{
		const Vec2V windowCentre = (mirrorBoundsMax + mirrorBoundsMin)*ScalarV(V_HALF);
		const Vec2V windowExtent = (mirrorBoundsMax - mirrorBoundsMin)*ScalarV(V_HALF);

		const float cx = windowCentre.GetXf();
		const float cy = windowCentre.GetYf();
		const float ex = windowExtent.GetXf();
		const float ey = windowExtent.GetYf();

		float windowX = 0.0f;
		float windowY = 0.0f;
		float windowW = 1.0f;
		float windowH = 1.0f;

		float borderedWindowX = 0.0f;
		float borderedWindowY = 0.0f;
		float borderedWindowW = 1.0f;
		float borderedWindowH = 1.0f;
		float borderedWindowScaleX = 1.0f;
		float borderedWindowScaleY = 1.0f;

#if MIRROR_DEV
		if (CMirrors::ms_debugMirrorMaxPixelDensityEnabled)
#endif // MIRROR_DEV
		{
			const float maxPixelDensity = MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorMaxPixelDensity, MIRROR_DEFAULT_MAX_PIXEL_DENSITY);

			// mirror bounds in pixels
			const float pixelW = Max<float>(4.0f, maxPixelDensity*ex*(float)GRCDEVICE.GetWidth ());
			const float pixelH = Max<float>(4.0f, maxPixelDensity*ey*(float)GRCDEVICE.GetHeight());

			windowW = Min<float>(1.0f, 2.0f*ceilf(pixelW/2.0f)/(float)CMirrors::GetMirrorRenderTarget()->GetWidth ());
			windowH = Min<float>(1.0f, 2.0f*ceilf(pixelH/2.0f)/(float)CMirrors::GetMirrorRenderTarget()->GetHeight());
			windowX = 0.5f - 0.5f*windowW; // centre
			windowY = 0.5f - 0.5f*windowH; // centre

			const float border = MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorMaxPixelDensityBorder, MIRROR_DEFAULT_MAX_PIXEL_DENSITY_BORDER);

			borderedWindowW = Min<float>(1.0f, windowW + border/(float)CMirrors::GetMirrorRenderTarget()->GetWidth ());
			borderedWindowH = Min<float>(1.0f, windowH + border/(float)CMirrors::GetMirrorRenderTarget()->GetHeight());
			borderedWindowX = 0.5f - 0.5f*borderedWindowW; // centre
			borderedWindowY = 0.5f - 0.5f*borderedWindowH; // centre
			borderedWindowScaleX = borderedWindowW/windowW;
			borderedWindowScaleY = borderedWindowH/windowH;
		}

		viewport.SetNormWindowPortal_UpdateThreadOnly(cx*0.5f + 0.5f, cy*0.5f + 0.5f, ex*borderedWindowScaleX, ey*borderedWindowScaleY, borderedWindowX, borderedWindowY, borderedWindowW, borderedWindowH);
		CMirrors::SetMirrorBounds((cx/ex)*0.5f + 0.5f, (cy/ey)*0.5f + 0.5f, -0.5f/ex, -0.5f/ey, windowX, windowY, windowW, windowH);
	}
	else // no restricted projection
	{
		viewport.SetNormWindow(0.0f, 0.0f, 1.0f, 1.0f);
		CMirrors::SetMirrorBounds(0.5f, 0.5f, -0.5f, -0.5f);
	}

#if __PS3
	g_mirrorReflectionViewportNoObliqueProjection = viewport;
#endif // __PS3

#if __PS3 || MIRROR_DEV
	bool bUseObliqueProjection = false;

#if __PS3
	bUseObliqueProjection = true;
#elif MIRROR_DEV
	if (!CMirrors::ms_debugMirrorUseHWClipPlane)
	{
		bUseObliqueProjection = true;
	}
#endif // MIRROR_DEV

#if MIRROR_DEV
	if (!CMirrors::ms_debugMirrorUseObliqueProjection)
	{
		bUseObliqueProjection = false;
	}
#endif // MIRROR_DEV

	if (bUseObliqueProjection)
	{
		const ScalarV projScale = ScalarV(MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorUseObliqueProjectionScale, MIRROR_DEFAULT_OBLIQUE_PROJ_SCALE));

		viewport.ApplyObliqueProjection(CMirrors::GetAdjustedClipPlaneForInteriorWater(mirrorPlane*projScale));
	}
#endif // __PS3 || MIRROR_DEV

	if (bIsUsingMirrorWaterSurface)
	{
		Water::SetReflectionMatrix(PS3_SWITCH(g_mirrorReflectionViewportNoObliqueProjection, viewport).GetCompositeMtx());
	}

	g_mirrorReflectionViewport = &viewport;
}

void CRenderPhaseMirrorReflection::SetCullShape(fwRenderPhaseCullShape& cullShape)
{
#if MIRROR_DEV
	if (!CMirrors::ms_debugMirrorCullingPlanesEnabled)
	{
		spdTransposedPlaneSet8 frustum;
		sysMemSet(&frustum, 0, sizeof(frustum));
		cullShape.SetFrustum(frustum);
	}
	else
#endif // MIRROR_DEV
	{
		const Vec3V* mirrorCorners = CMirrors::GetMirrorCorners();
		const Vec4V  mirrorPlane   = CMirrors::GetMirrorPlane();
		const Vec3V  mirrorNormal  = mirrorPlane.GetXYZ();

		const Vec3V camPos = gVpMan.GetCurrentGameGrcViewport()->GetCameraPosition();
		const Vec3V apex   = camPos - mirrorNormal*PlaneDistanceTo(mirrorPlane, camPos)*ScalarV(V_TWO);

		spdTransposedPlaneSet8 frustum;
		const grcViewport& viewport = GetGrcViewport();

		const Vec4V noCull = CMirrors::GetIsMirrorFrustumCullDisabled() ? Vec4V(V_ZERO) : Vec4V(V_ONE);
		const Vec4V plane0 = -noCull*viewport.GetFrustumClipPlane(grcViewport::CLIP_PLANE_LEFT  );
		const Vec4V plane1 = -noCull*viewport.GetFrustumClipPlane(grcViewport::CLIP_PLANE_RIGHT );
		const Vec4V plane2 = -noCull*viewport.GetFrustumClipPlane(grcViewport::CLIP_PLANE_TOP   );
		const Vec4V plane3 = -noCull*viewport.GetFrustumClipPlane(grcViewport::CLIP_PLANE_BOTTOM);

		const Vec4V plane4 = BuildPlane(apex, mirrorCorners[0], mirrorCorners[1]);
		const Vec4V plane5 = BuildPlane(apex, mirrorCorners[1], mirrorCorners[2]);
		const Vec4V plane6 = BuildPlane(apex, mirrorCorners[2], mirrorCorners[3]);
		const Vec4V plane7 = BuildPlane(apex, mirrorCorners[3], mirrorCorners[0]);

		frustum.SetPlanes(
			MIRROR_DEV_SWITCH((CMirrors::ms_debugMirrorCullingPlaneFlags & BIT(0)), true) ? plane0 : Vec4V(V_ZERO),
			MIRROR_DEV_SWITCH((CMirrors::ms_debugMirrorCullingPlaneFlags & BIT(1)), true) ? plane1 : Vec4V(V_ZERO),
			MIRROR_DEV_SWITCH((CMirrors::ms_debugMirrorCullingPlaneFlags & BIT(2)), true) ? plane2 : Vec4V(V_ZERO),
			MIRROR_DEV_SWITCH((CMirrors::ms_debugMirrorCullingPlaneFlags & BIT(3)), true) ? plane3 : Vec4V(V_ZERO),
			MIRROR_DEV_SWITCH((CMirrors::ms_debugMirrorCullingPlaneFlags & BIT(4)), true) ? plane4 : Vec4V(V_ZERO),
			MIRROR_DEV_SWITCH((CMirrors::ms_debugMirrorCullingPlaneFlags & BIT(5)), true) ? plane5 : Vec4V(V_ZERO),
			MIRROR_DEV_SWITCH((CMirrors::ms_debugMirrorCullingPlaneFlags & BIT(6)), true) ? plane6 : Vec4V(V_ZERO),
			MIRROR_DEV_SWITCH((CMirrors::ms_debugMirrorCullingPlaneFlags & BIT(7)), true) ? plane7 : Vec4V(V_ZERO)
		);

		cullShape.SetFrustum(frustum);
	}

	cullShape.SetMirrorPlane(CMirrors::GetMirrorPlane());
}

void CRenderPhaseMirrorReflection::BuildRenderList()
{
#if __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		return;
	}
#endif // __BANK

	if (!CMirrors::GetIsMirrorVisible() || !GetViewport()->IsActive())
	{
		return;
	}

	PF_FUNC(CRenderPhaseMirrorReflectionRL);

	bool bUseDirectionalLighting = fwScan::GetScanResults().GetMirrorCanSeeDirectionalLight();
	bool bCanSeeExteriorView     = fwScan::GetScanResults().GetMirrorCanSeeExteriorView();

#if MIRROR_DEV
	if      (CMirrors::ms_debugMirrorForceDirectionalLightingOn)  { bUseDirectionalLighting = true; }
	else if (CMirrors::ms_debugMirrorForceDirectionalLightingOff) { bUseDirectionalLighting = false; }

	if      (CMirrors::ms_debugMirrorForceExteriorViewOn)  { bCanSeeExteriorView = true; }
	else if (CMirrors::ms_debugMirrorForceExteriorViewOff) { bCanSeeExteriorView = false; }
#endif // MIRROR_DEV

	CutSceneManager* csMgr = CutSceneManager::GetInstance();
	Assert(csMgr);
	if(csMgr->IsCutscenePlayingBack())
	{
		bCanSeeExteriorView = true;
	}

	if (MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorForceDirectionalLightingOnForExteriorView, true) && bCanSeeExteriorView)
	{
		bUseDirectionalLighting = true;
	}

	const u32 exteriorVisMaskFlags =
		VIS_ENTITY_MASK_OTHER          |
		VIS_ENTITY_MASK_DUMMY_OBJECT   |
		VIS_ENTITY_MASK_LOLOD_BUILDING |
		VIS_ENTITY_MASK_TREE           |
		0;

	SetRenderFlags(g_mirrorReflectionRenderSettings | (bUseDirectionalLighting ? RENDER_SETTING_RENDER_SHADOWS : 0));
	GetEntityVisibilityMask().SetAllFlags((u16)(g_mirrorReflectionVisMaskFlags | (bCanSeeExteriorView ? exteriorVisMaskFlags : 0)));

	COcclusion::BeforeSetupRender(false, false, SHADOWTYPE_NONE, true, GetGrcViewport());
	CScene::SetupRender(GetEntityListIndex(), GetRenderFlags(), GetGrcViewport(), this);
	COcclusion::AfterPreRender();
}

static void RenderStateSetupMirrorReflection(fwRenderPassId pass, fwRenderBucket bucket, fwRenderMode, int, fwRenderGeneralFlags)
{
	if (pass == RPASS_CUTOUT MIRROR_DEV_ONLY(&& CMirrors::ms_debugMirrorUseAlphaTestForCutout))
	{
		DLC(dlCmdSetBlendState, (grcStateBlock::BS_Default));
		DLC_Add(CShaderLib::SetAlphaTestRef, (MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorUseAlphaTestForCutoutAlphaRef, 128.f/255.f)));
	}
	else if (BIT(pass) & g_mirrorReflectionAlphaBlendingPasses)
	{
		DLC(dlCmdSetBlendState, (grcStateBlock::BS_Normal));
	}
	else
	{
		DLC(dlCmdSetBlendState, (grcStateBlock::BS_Default)); // no alpha blending
	}

	if (BIT(pass) & g_mirrorReflectionDepthWritingPasses)
	{
		DLC(dlCmdSetDepthStencilState, (grcStateBlock::DSS_LessEqual));
	}
	else
	{
		DLC(dlCmdSetDepthStencilState, (grcStateBlock::DSS_TestOnly_LessEqual)); // no depth writing
	}

	if(!g_mirrorReflectionUsingWaterReflectionTechnique)
	{
		if(bucket == CRenderer::RB_CUTOUT)
		{
			DLC_Add(Lights::SetLightweightTechniquePassType, Lights::kLightPassCutOut);
		}
	}

	DLC(dlCmdGrcLightStateSetEnabled, (!g_mirrorReflectionUsingWaterReflectionTechnique));
	DLC(dlCmdSetBucketsAndRenderMode, (bucket, CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_MIRROR), rmStandard)); // damn lies.
}


static void RenderStateCleanupMirrorReflection(fwRenderPassId pass, fwRenderBucket bucket, fwRenderMode rm, int subphase, fwRenderGeneralFlags generalFlags)
{
	if(!g_mirrorReflectionUsingWaterReflectionTechnique)
	{
		if(bucket == CRenderer::RB_CUTOUT)
		{
			DLC_Add(Lights::SetLightweightTechniquePassType, Lights::kLightPassNormal);
		}
	}

	CRenderListBuilder::RenderStateCleanupSimpleFlush(pass, bucket, rm, subphase, generalFlags);
}


static void RenderStateSetupCustomFlagsChangedMirrorReflection(fwRenderPassId MIRROR_DEV_ONLY(pass), fwRenderBucket MIRROR_DEV_ONLY(bucket), fwRenderMode MIRROR_DEV_ONLY(renderMode), int MIRROR_DEV_ONLY(subphasePass), fwRenderGeneralFlags MIRROR_DEV_ONLY(generalFlags), u16 MIRROR_DEV_ONLY(previousCustomFlags), u16 currentCustomFlags)
{
	CDrawListPrototypeManager::Flush();
	const bool bInInterior = (currentCustomFlags & fwEntityList::CUSTOM_FLAGS_INTERIOR_SORT) != 0;

	DLC_Add(Lights::SetupDirectionalGlobals, bInInterior ? 0.0f : 1.0f);
#if MIRROR_DEV
	if (0 && CMirrors::ms_debugMirrorShowInfo)
	{
		grcDebugDraw::AddDebugOutput("mirror: custom flags changed 0x%04x -> 0x%04x (in interior = %s), pass=%d, bucket=%d, mode=%d, subphase=%d, flags=%d", previousCustomFlags, currentCustomFlags, bInInterior ? "TRUE" : "FALSE", pass, bucket, renderMode, subphasePass, generalFlags);
	}
#endif // MIRROR_DEV
}

static void MirrorSetDirLighting(bool bEnable)
{
	Lights::m_lightingGroup.SetDirectionalLightingGlobals(bEnable);
	Lights::m_lightingGroup.SetAmbientLightingGlobals();
	g_timeCycle.SetShaderData();
}

static void MirrorRestoreDirLighting()
{
	Lights::m_lightingGroup.SetDirectionalLightingGlobals(true);
}

#if __PS3
extern bool SetEdgeNoPixelTestDisableIfFarFromOrigin_Begin(const grcViewport* viewport);
extern void SetEdgeNoPixelTestDisableIfFarFromOrigin_End(bool bCamDistFarFromOrigin);
#endif // __PS3

#if RSG_ORBIS
void GenerateMips(grcRenderTarget* target)
{
	target->GenerateMipmaps();
}

static void FinishMirrorRendering()
{
	static_cast<grcTextureFactoryGNM&>( grcTextureFactory::GetInstance() ).FinishRendering();
}
#endif // RSG_ORBIS


void SetupMirrorDepthTarget(bool bIsUsingMirrorWaterSurface)
{
	//do this only for PC and Durango (if MSAA is enabled)
#if __D3D11
	if(!bIsUsingMirrorWaterSurface)
	{
		//Let's switch of clip planes as this is a full screen pass
		DLC(dlCmdSetClipPlaneEnable, (0x0));

#if DEVICE_MSAA
		if (CMirrors::GetMSAA() > 1)
		{
			DLC_Add(CMirrors::ResolveMirrorDepthTarget);
		}
		else
#endif //DEVICE_MSAA
		{
			//If read only is not allowed then make a copy of the buffer
			if(!GRCDEVICE.IsReadOnlyDepthAllowed())
			{
				DLC_Add(CRenderTargets::CopyDepthBuffer, CMirrors::GetMirrorDepthTarget(), CMirrors::GetMirrorDepthTarget_Copy());	
			
			}
		}

		//Switch clip planes back on
		DLC(dlCmdSetClipPlaneEnable, (0x1));

	}
#endif //__D3D11
}
void CRenderPhaseMirrorReflection::BuildDrawList()
{
#if __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		return;
	}
#endif // __BANK
	
	// Cannot call Water::UsePlanarReflection() in update viewport since I don't have occlusion query data yet
	// Cannot call Water::IsUsingMirrorWaterSurface() in update viewport since I don't have visibility data yet
	if (	!(CMirrors::GetIsMirrorVisible() && fwScan::GetScanResults().GetMirrorVisible()) ||
   		 	!HasEntityListIndex() ||
			(Water::IsUsingMirrorWaterSurface() && !Water::UsePlanarReflection()))
	{
		g_mirrorReflectionUsingWaterReflectionTechnique = false;
		return;
	}

	DrawListPrologue();

	const bool bIsUsingMirrorWaterSurface = Water::IsUsingMirrorWaterSurface();
	grcRenderTarget *clearTarget;
	g_mirrorReflectionEnableMSAA = true;
	if (bIsUsingMirrorWaterSurface)
	{
		clearTarget = CMirrors::GetMirrorWaterRenderTarget();
#if RSG_PC
		g_mirrorReflectionEnableMSAA = CMirrors::GetMirrorWaterMSAARenderTarget() != NULL;
#endif // RSG_PC
	}
	else
	{
		clearTarget = CMirrors::GetMirrorRenderTarget(g_mirrorReflectionEnableMSAA);
	}

	DLC(dlCmdLockRenderTarget, (0, clearTarget, NULL));
	DLC(dlCmdClearRenderTarget, (true, Color32(0), false, 1.0f, false, 0));
	DLC(dlCmdUnLockRenderTarget, (0));

	DLC_Add(CShaderLib::SetUseFogRay, false);
	DLC_Add(CShaderLib::SetFogRayTexture);

	PF_FUNC(CRenderPhaseMirrorReflection);
	
	if (!Water::IsUsingMirrorWaterSurface())
		PEDDAMAGEMANAGER.AddToDrawList(false,true);

	grcViewport& viewport = GetGrcViewport();
	bool bForcingLODs = false;

	if (fwScan::GetScanResults().GetMirrorFloorVisible() && MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorFloorUseLODs, true))
	{
		dlDrawListMgr::SetHighestLOD(false, dlDrawListMgr::DLOD_MED, dlDrawListMgr::DLOD_MED, dlDrawListMgr::VLOD_HIGH, dlDrawListMgr::PLOD_LOD);
		bForcingLODs = true;
	}

	CMirrors::SetShaderParams();

	float localLightIntensity = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_MIRROR_REFLECTION_LOCAL_LIGHT_INTENSITY);

#if MIRROR_DEV
	if (CMirrors::ms_debugMirrorLocalLightIntensityOverrideEnabled)
	{
		localLightIntensity = CMirrors::ms_debugMirrorLocalLightIntensityOverride;
	}
#endif // MIRROR_DEV

	grcRenderTarget* depthBuffer = CMirrors::GetMirrorDepthTarget();
#if __D3D11
#if DEVICE_MSAA
	if (CMirrors::GetMSAA()>1)
	{
		depthBuffer = CMirrors::GetMirrorDepthResolved();
	}
	else
#endif //DEVICE_MSAA
	{
		//If read only is not allowed then make a copy of the buffer
		if(!GRCDEVICE.IsReadOnlyDepthAllowed())
		{
			depthBuffer = CMirrors::GetMirrorDepthTarget_Copy();	
		}
	}
#endif //__D3D11

	DLC_Add(CShaderLib::SetGlobalEmissiveScale, 1.0f, false);
	DLC_Add(CRenderPhaseReflection::SetReflectionMap);
	DLC_Add(CRenderPhaseCascadeShadowsInterface::SetCloudShadowParams);
	DLC_Add(CPtFxManager::UpdateDepthBuffer, depthBuffer);
	DLC_Add(CLightGroup::SetLocalLightForwardIntensityScale, localLightIntensity);
	DLC_Add(CTimeCycle::SetShaderParams);

#if __XENON
	DLC_Add(PostFX::SetLDR10bitHDR10bit);
#elif __PS3
	DLC_Add(PostFX::SetLDR8bitHDR8bit);
#else
	DLC_Add(PostFX::SetLDR16bitHDR16bit);
#endif

	const bool bIsUsingRestrictedProjection = gMirrorAllowRestrictedProjection && !bIsUsingMirrorWaterSurface MIRROR_DEV_ONLY(&& CMirrors::ms_debugMirrorRestrictedProjection);
	const bool bIsUsingWaterReflectionTechnique = CMirrors::GetIsMirrorUsingWaterReflectionTechnique(bIsUsingMirrorWaterSurface);
	bool bSkyIsVisible = false;

	g_mirrorReflectionUsingWaterReflectionTechnique = bIsUsingWaterReflectionTechnique; // save this so it can be accessed by RenderStateSetupMirrorReflection

	//Adding draw command to set the renderthread copy to whatever the main thread copy is. 
	//This is used for places where we don't support rendering into the water reflections.
	DLC_Add(CRenderPhaseMirrorReflectionInterface::SetIsMirrorUsingWaterReflectionTechniqueAndSurface_Render,
		g_mirrorReflectionUsingWaterReflectionTechnique, bIsUsingMirrorWaterSurface);

#if MIRROR_DEV
	if (CMirrors::ms_debugMirrorRenderSkyAlways)
	{
		bSkyIsVisible = true;
	}
	else
#endif // MIRROR_DEV
	if (gMirrorScanCameraIsInInterior)
	{
		bSkyIsVisible = (fwScan::GetScanResults().m_skyPortalBounds_MirrorReflection.GetSkyPortalCount() > 0);
	}
	else
	{
		bSkyIsVisible = fwScan::GetScanResults().GetMirrorCanSeeExteriorView();
	}

	CMirrors::eMirrorRenderSkyMode renderSkyMode = CMirrors::MIRROR_RENDER_SKY_MODE_NONE;
	CVisualEffects::eRenderMode renderSkyVfxMode = bIsUsingWaterReflectionTechnique ? CVisualEffects::RM_WATER_REFLECTION : CVisualEffects::RM_MIRROR_REFLECTION;

	if (bSkyIsVisible)
	{
		renderSkyMode = MIRROR_DEV_SWITCH((CMirrors::eMirrorRenderSkyMode)CMirrors::ms_debugMirrorRenderSkyMode, CMirrors::MIRROR_RENDER_SKY_MODE_DEFAULT);
	}

	Color32 clearColour = Color32(0);
//	Color32 clearColour = Color32(255,0,0,255); // hack to make border visible

#if __BANK
	if (bIsUsingMirrorWaterSurface && !CRenderPhaseWaterReflectionInterface::IsSkyEnabled())
	{
		renderSkyMode = CMirrors::MIRROR_RENDER_SKY_MODE_NONE; // no sky, it's disabled for water reflection in rag
		clearColour = Color32(CRenderPhaseWaterReflectionInterface::GetWaterReflectionClearColour());
	}
#endif // __BANK

	grcRenderTarget* depthTarget;
#if __D3D11
	grcRenderTarget* depthTargetCopy = NULL;
	grcRenderTarget* depthTargetTargetRO = NULL;
#endif //__D3D11

#if MIRROR_USE_STENCIL_MASK
	if (CMirrors::ms_debugMirrorStencilMaskEnabled)
	{
#if !__XENON
		DLC_Add(CRenderTargets::BindCustomRenderTarget);
#endif // !__XENON
		DLC(dlCmdSetCurrentViewport, (&viewport));

		if (CMirrors::ms_debugMirrorStencilMaskScissor)
		{
			const int scissorX = Clamp<int>((int)gMirrorScreenBoundsX - 1, 0, GRCDEVICE.GetWidth ()); // expand by 1 pixel
			const int scissorY = Clamp<int>((int)gMirrorScreenBoundsY - 1, 0, GRCDEVICE.GetHeight());
			const int scissorW = Min<int>(GRCDEVICE.GetWidth () - scissorX, (int)gMirrorScreenBoundsW + 2);
			const int scissorH = Min<int>(GRCDEVICE.GetHeight() - scissorY, (int)gMirrorScreenBoundsH + 2);

			DLC(dlCmdGrcDeviceSetScissor, (scissorX, scissorY, scissorW, scissorH));
		}

		// render mirror portal into stencil buffer, setting pixels to 96
		DLC_Add(CMirrors::RenderDebug, true);

		// pass only stencil=96
		{
			static grcDepthStencilStateHandle ds_handle = grcStateBlock::DSS_Invalid;
			static grcRasterizerStateHandle   rs_handle = grcStateBlock::RS_Invalid;

			if (ds_handle == grcStateBlock::DSS_Invalid)
			{
				grcDepthStencilStateDesc ds; // no depth test, pass only stencil=96
				grcRasterizerStateDesc   rs; // cull front (because reflection is flipped)

				ds.StencilEnable                = true;
				ds.StencilReadMask              = 96;
				ds.StencilWriteMask             = 0;
				ds.FrontFace.StencilFailOp      = grcRSV::STENCILOP_KEEP;
				ds.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
				ds.FrontFace.StencilPassOp      = grcRSV::STENCILOP_KEEP;
				ds.FrontFace.StencilFunc        = grcRSV::CMP_EQUAL;
				ds.BackFace                     = ds.FrontFace;

				rs.CullMode = grcRSV::CULL_FRONT;

				ds_handle = grcStateBlock::CreateDepthStencilState(ds);
				rs_handle = grcStateBlock::CreateRasterizerState  (rs);
			}

			// TODO -- save the state handles here, but don't set until we're about to render
			DLC(dlCmdSetStates, (rs_handle, ds_handle, grcStateBlock::BS_Default));
		}
	}
	else
#endif // MIRROR_USE_STENCIL_MASK
	{
		grcRenderTarget* colorTarget;

		if (bIsUsingMirrorWaterSurface)
		{
			colorTarget =
		#if RSG_PC
				g_mirrorReflectionEnableMSAA ? CMirrors::GetMirrorWaterMSAARenderTarget() :
		#endif
				CMirrors::GetMirrorWaterRenderTarget();
			depthTarget = CMirrors::GetMirrorWaterDepthTarget();
		#if __D3D11
			depthTargetCopy = CMirrors::GetMirrorWaterDepthTarget_Copy();
			depthTargetTargetRO = CMirrors::GetMirrorWaterDepthTarget_ReadOnly();
		#endif //__D3D11
		}
		else
		{
			colorTarget = CMirrors::GetMirrorRenderTarget(g_mirrorReflectionEnableMSAA);
			depthTarget = CMirrors::GetMirrorDepthTarget();
		#if __D3D11
			depthTargetCopy = CMirrors::GetMirrorDepthTarget_Copy();
			depthTargetTargetRO = CMirrors::GetMirrorDepthTarget_ReadOnly();
		#endif //__D3D11
		}

		DLC(dlCmdLockRenderTarget, (0, colorTarget, depthTarget));

		// hack to make border visible
		//DLC(dlCmdSetCurrentViewport, (gVpMan.GetUpdateGameGrcViewport()));
		//DLC(dlCmdClearRenderTarget, (renderSkyMode == CMirrors::MIRROR_RENDER_SKY_MODE_NONE, clearColour, true, 1.0f, true, 0));

		DLC(dlCmdSetCurrentViewport, (&PS3_SWITCH(g_mirrorReflectionViewportNoObliqueProjection, viewport)));
		XENON_ONLY(DLC_Add(dlCmdEnableAutomaticHiZ, true));
		DLC(dlCmdClearRenderTarget, (renderSkyMode == CMirrors::MIRROR_RENDER_SKY_MODE_NONE, clearColour, true, 1.0f, true, 0));

		// TODO -- save the state handles here, but don't set until we're about to render
		// [STATE OVERRIDE] force backface culling (which is the default anyway), otherwise the mirror might render the wall behind
		DLC(dlCmdSetStates, (grcStateBlock::XENON_SWITCH(RS_Default_HalfPixelOffset, RS_Default), grcStateBlock::DSS_LessEqual, grcStateBlock::BS_Default));
	}

	DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars);
	DLC_Add(CShaderLib::ApplyDayNightBalance); // update shaders

	if (renderSkyMode == CMirrors::MIRROR_RENDER_SKY_MODE_FIRST)
	{
		// Only used when renderSkyVfxMode = CVisualEffects::RM_WATER_REFLECTION or CVisualEffects::RM_MIRROR_REFLECTION (see CVisualEffects::RenderSky()).
		D3D11_ONLY(DLC_Add(CVisualEffects::SetTargetsForDX11ReadOnlyDepth, (renderSkyVfxMode == CVisualEffects::RM_WATER_REFLECTION) ? false : true, depthTarget, depthTargetCopy, depthTargetTargetRO));
		DLC_Add(CVisualEffects::RenderSky, renderSkyVfxMode, false, bIsUsingRestrictedProjection, -1, -1, -1, -1, true, 1.0f);
		D3D11_ONLY(DLC_Add(CVisualEffects::SetTargetsForDX11ReadOnlyDepth, false, (grcRenderTarget *)NULL, (grcRenderTarget *)NULL, (grcRenderTarget *)NULL));
	}

	bool bUseDirectionalLighting = fwScan::GetScanResults().GetMirrorCanSeeDirectionalLight();
	bool bCanSeeExteriorView     = fwScan::GetScanResults().GetMirrorCanSeeExteriorView();

	CutSceneManager* csMgr = CutSceneManager::GetInstance();
	Assert(csMgr);
	if(csMgr->IsCutscenePlayingBack())
	{
		bCanSeeExteriorView = true;
	}

#if MIRROR_DEV
	if      (CMirrors::ms_debugMirrorForceDirectionalLightingOn)  { bUseDirectionalLighting = true; }
	else if (CMirrors::ms_debugMirrorForceDirectionalLightingOff) { bUseDirectionalLighting = false; }

	if      (CMirrors::ms_debugMirrorForceExteriorViewOn)  { bCanSeeExteriorView = true; }
	else if (CMirrors::ms_debugMirrorForceExteriorViewOff) { bCanSeeExteriorView = false; }

	if (CMirrors::ms_debugMirrorWireframe)
	{
		DLC_Add(grcStateBlock::SetWireframeOverride, true);
		DLC_Add(grcStateBlock::SetRasterizerState, grcStateBlock::RS_Default); // [STATE OVERRIDE]
	}
#endif // MIRROR_DEV

	if (MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorForceDirectionalLightingOnForExteriorView, true) && bCanSeeExteriorView)
	{
		bUseDirectionalLighting = true;
	}

	int fullFilter = CRenderListBuilder::RENDER_DONT_RENDER_PLANTS;

	if ( MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorDrawFilter_RENDER_OPAQUE        , true )) { fullFilter |= CRenderListBuilder::RENDER_OPAQUE                    ; }
	if ( MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorDrawFilter_RENDER_ALPHA         , true )) { fullFilter |= CRenderListBuilder::RENDER_ALPHA                     ; }
	if ( MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorDrawFilter_RENDER_FADING        , true )) { fullFilter |= CRenderListBuilder::RENDER_FADING                    ; }
	if (!MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorDrawFilter_RENDER_TREES         , false)) { fullFilter |= CRenderListBuilder::RENDER_DONT_RENDER_TREES         ; }
	if (!MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorDrawFilter_RENDER_DECALS_CUTOUTS, true )) { fullFilter |= CRenderListBuilder::RENDER_DONT_RENDER_DECALS_CUTOUTS; }

	if (bCanSeeExteriorView)
	{
		// force trees in exterior view
		fullFilter &= ~CRenderListBuilder::RENDER_DONT_RENDER_TREES;
	}

	const int opaqueFilter = (fullFilter & ~(CRenderListBuilder::RENDER_ALPHA | CRenderListBuilder::RENDER_FADING));
	const int alphaFilter  = (fullFilter &  (CRenderListBuilder::RENDER_ALPHA | CRenderListBuilder::RENDER_FADING)) | CRenderListBuilder::RENDER_MIRROR_ALPHA;

	DLC_Add(MirrorSetDirLighting, bUseDirectionalLighting);

	PS3_ONLY(const bool bCamDistFarFromOrigin = SetEdgeNoPixelTestDisableIfFarFromOrigin_Begin(&viewport));

#if !__PS3
#if MIRROR_DEV
	if (CMirrors::ms_debugMirrorUseHWClipPlane)
#endif // MIRROR_DEV
	{
		DLC(dlCmdSetClipPlane, (0,		CMirrors::GetAdjustedClipPlaneForInteriorWater(CMirrors::GetMirrorPlane())));
		DLC(dlCmdSetClipPlaneEnable, 	(0x1));
	}
#endif // !__PS3

	CRenderListBuilder::eDrawMode drawMode = (CRenderListBuilder::eDrawMode)0;
	bool bForceTechnique = false;
	bool bForceLightweight0 = false;

	if (bIsUsingWaterReflectionTechnique)
	{
		DLC(dlCmdShaderFxPushForcedTechnique, (CRenderPhaseWaterReflectionInterface::GetWaterReflectionTechniqueGroupId()));
		drawMode = CRenderListBuilder::USE_SIMPLE_LIGHTING;
		bForceTechnique = true;
	}
	else
	{
		DLC_Add(Lights::BeginLightweightTechniques);
		DLC_Add(Lights::SelectLightweightTechniques, 0, false);
		drawMode = CRenderListBuilder::USE_FORWARD_LIGHTING;
		bForceLightweight0 = true;
	}

	if (renderSkyMode == CMirrors::MIRROR_RENDER_SKY_MODE_BEFORE_ALPHA)
	{
		PS3_ONLY(DLC(dlCmdSetCurrentViewport, (&viewport));)

		// opaque pass
		DLCPushTimebar("Opaque");
		RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseMirrorReflection - opaque");
		CRenderListBuilder::AddToDrawList(
			GetEntityListIndex(),
			opaqueFilter,
			drawMode,//CRenderListBuilder::USE_FORWARD_LIGHTING
			SUBPHASE_NONE,
			RenderStateSetupMirrorReflection,
			RenderStateCleanupMirrorReflection,
			RenderStateSetupCustomFlagsChangedMirrorReflection
		);
		RENDERLIST_DUMP_PASS_INFO_END();
		DLCPopTimebar();

		PS3_ONLY(DLC(dlCmdSetCurrentViewport, (&g_mirrorReflectionViewportNoObliqueProjection));)

#if !__PS3
#if MIRROR_DEV
		if (CMirrors::ms_debugMirrorUseHWClipPlane)
#endif // MIRROR_DEV
		{
			DLC(dlCmdSetClipPlaneEnable, (0x0));
		}
#endif // !__PS3

		// Only used when renderSkyVfxMode = CVisualEffects::RM_WATER_REFLECTION or CVisualEffects::RM_MIRROR_REFLECTION (see CVisualEffects::RenderSky()).
		D3D11_ONLY(DLC_Add(CVisualEffects::SetTargetsForDX11ReadOnlyDepth, (renderSkyVfxMode == CVisualEffects::RM_WATER_REFLECTION) ? false : true, depthTarget, depthTargetCopy, depthTargetTargetRO));
		// sky pass
		DLC_Add(CVisualEffects::RenderSky, renderSkyVfxMode, false, bIsUsingRestrictedProjection, -1, -1, -1, -1, true, 1.0f);
		D3D11_ONLY(DLC_Add(CVisualEffects::SetTargetsForDX11ReadOnlyDepth, false, (grcRenderTarget *)NULL, (grcRenderTarget *)NULL, (grcRenderTarget *)NULL));
		
		SetupMirrorDepthTarget(bIsUsingMirrorWaterSurface);
#if !__PS3
#if MIRROR_DEV
		if (CMirrors::ms_debugMirrorUseHWClipPlane)
#endif // MIRROR_DEV
		{
			DLC(dlCmdSetClipPlaneEnable, (0x1));
		}
#endif // !__PS3

		DLC(dlCmdSetCurrentViewport, (&viewport));

		// alpha pass
		DLCPushTimebar("Alpha/Decals");
		RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseMirrorReflection - alpha");
		CRenderListBuilder::AddToDrawList(
			GetEntityListIndex(),
			alphaFilter,
			drawMode,//CRenderListBuilder::USE_FORWARD_LIGHTING,
			SUBPHASE_NONE,
			RenderStateSetupMirrorReflection,
			RenderStateCleanupMirrorReflection,
			RenderStateSetupCustomFlagsChangedMirrorReflection
		);
		RENDERLIST_DUMP_PASS_INFO_END();
		DLCPopTimebar();

		PS3_ONLY(DLC(dlCmdSetCurrentViewport, (&g_mirrorReflectionViewportNoObliqueProjection));)
	}
	else
	{
		PS3_ONLY(DLC(dlCmdSetCurrentViewport, (&viewport));)

		// opaque
		DLCPushTimebar("Opaque");
		RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseMirrorReflection - Opaque");
		CRenderListBuilder::AddToDrawList(
			GetEntityListIndex(),
			opaqueFilter,
			drawMode,
			SUBPHASE_NONE,
			RenderStateSetupMirrorReflection,
			RenderStateCleanupMirrorReflection,
			RenderStateSetupCustomFlagsChangedMirrorReflection
		);
		RENDERLIST_DUMP_PASS_INFO_END();
		DLCPopTimebar();
		
		SetupMirrorDepthTarget(bIsUsingMirrorWaterSurface);

		DLC(dlCmdSetCurrentViewport, (&viewport));

		// alpha pass
		DLCPushTimebar("Alpha/Decals");
		RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseMirrorReflection - Alpha/Decals");
		CRenderListBuilder::AddToDrawList(
			GetEntityListIndex(),
			alphaFilter,
			drawMode,
			SUBPHASE_NONE,
			RenderStateSetupMirrorReflection,
			RenderStateCleanupMirrorReflection,
			RenderStateSetupCustomFlagsChangedMirrorReflection
			);
		RENDERLIST_DUMP_PASS_INFO_END();
		DLCPopTimebar();

		PS3_ONLY(DLC(dlCmdSetCurrentViewport, (&g_mirrorReflectionViewportNoObliqueProjection));)
	}

#if !__PS3
#if MIRROR_DEV
	if (CMirrors::ms_debugMirrorUseHWClipPlane)
#endif // MIRROR_DEV
	{
		DLC(dlCmdSetClipPlaneEnable, (0x0));
	}
#endif // !__PS3

	PS3_ONLY(SetEdgeNoPixelTestDisableIfFarFromOrigin_End(bCamDistFarFromOrigin));

	DLC_Add(MirrorRestoreDirLighting);

	if (bForceTechnique)
	{
		DLC(dlCmdShaderFxPopForcedTechnique, ());
	}

	if (bForceLightweight0)
	{
		DLC_Add(Lights::EndLightweightTechniques);
	}

#if MIRROR_DEV
	if (CMirrors::ms_debugMirrorWireframe)
	{
		DLC_Add(grcStateBlock::SetWireframeOverride, false); // restore
	}
#endif // MIRROR_DEV

	if (renderSkyMode == CMirrors::MIRROR_RENDER_SKY_MODE_LAST)
	{
		// Only used when renderSkyVfxMode = CVisualEffects::RM_WATER_REFLECTION or CVisualEffects::RM_MIRROR_REFLECTION (see CVisualEffects::RenderSky()).
		D3D11_ONLY(DLC_Add(CVisualEffects::SetTargetsForDX11ReadOnlyDepth, (renderSkyVfxMode == CVisualEffects::RM_WATER_REFLECTION) ? false : true, depthTarget, depthTargetCopy, depthTargetTargetRO));
		DLC_Add(CVisualEffects::RenderSky, renderSkyVfxMode, false, bIsUsingRestrictedProjection, -1, -1, -1, -1, true, 1.0f);
		D3D11_ONLY(DLC_Add(CVisualEffects::SetTargetsForDX11ReadOnlyDepth, false, (grcRenderTarget *)NULL, (grcRenderTarget *)NULL, (grcRenderTarget *)NULL));
	}

	if (bCanSeeExteriorView && MIRROR_DEV_SWITCH(CMirrors::ms_debugMirrorRenderDistantLights, false))
	{
		DLC_Add(CVisualEffects::RenderDistantLights, CVisualEffects::RM_MIRROR_REFLECTION, GetRenderFlags(), 1.0f);
	}

#if MIRROR_DEV
	if (CMirrors::ms_debugMirrorRenderCoronas)
#endif // MIRROR_DEV
	{
		DLC_Add(CVisualEffects::RenderCoronas, CVisualEffects::RM_MIRROR_REFLECTION, 1.0f, 1.0f);
	}

#if MIRROR_USE_STENCIL_MASK
	if (CMirrors::ms_debugMirrorStencilMaskEnabled)
	{
		if (CMirrors::ms_debugMirrorStencilMaskScissor)
		{
			DLC(dlCmdGrcDeviceDisableScissor, ());
		}

#if !__XENON
		DLC_Add(CRenderTargets::ReleaseCustomRenderTarget);
#endif // !__XENON
	}
	else
#endif // MIRROR_USE_STENCIL_MASK
	{
#if RSG_ORBIS
		DLC_Add(FinishMirrorRendering);
#endif
		grcResolveFlags flags;
		flags.ClearColor = true;
		DLC(dlCmdUnLockRenderTarget, (0, &flags));
#if DEVICE_MSAA
#if RSG_PC
		if (bIsUsingMirrorWaterSurface && CRenderPhaseWaterReflection::GetMSAARenderTargetColor())
		{
			DLC_Add(CRenderPhaseWaterReflection::ResolveMSAA);
		}
#endif	// #if RSG_PC
		if (!bIsUsingMirrorWaterSurface && g_mirrorReflectionEnableMSAA)
		{
			DLC_Add(CMirrors::ResolveMirrorRenderTarget);
		}
#endif //DEVICE_MSAA
	}

#if __XENON || __PS3
	if (bIsUsingMirrorWaterSurface)
	{
		if (bIsUsingWaterReflectionTechnique)
		{
#if __PS3
			DLC_Add(CRenderPhaseReflection::MSAAResolve, Water::GetReflectionRT(), CMirrors::GetMirrorWaterRenderTarget(), 0, 4);
#endif // __PS3
		}
		else
		{
			DLC_Add(ConvertToWaterReflectionGamma);
		}
	}
#endif // __XENON || __PS3

#if RSG_ORBIS
	if(bIsUsingMirrorWaterSurface)
		DLC_Add(GenerateMips, clearTarget);
#endif
 
	DLC(dlCmdSetCurrentViewportToNULL, ());

	if (bForcingLODs)
	{
		dlDrawListMgr::ResetHighestLOD(false);
	}

	DLC_Add(CLightGroup::SetLocalLightForwardIntensityScale, 1.0f);
	DLC_Add(CShaderLib::SetUseFogRay, true);

	//Reset the renderthread copy back to false 
	DLC_Add(CRenderPhaseMirrorReflectionInterface::SetIsMirrorUsingWaterReflectionTechniqueAndSurface_Render, false, false);

	DrawListEpilogue();
}

#if __BANK
void CRenderPhaseMirrorReflection::AddWidgets(bkGroup& group)
{
	CMirrors::AddWidgets(group);

	group.AddToggle("Debug Draw Mirror", &m_debugDrawMirror);
	group.AddSlider("Debug Draw Offset", &m_debugDrawMirrorOffset, -10.0f, 10.0f, 1.0f/64.0f);
#if DEVICE_MSAA && !RSG_PC
	// We can't use non-AA reflection with AA depth, so for PC we can't disable MSAA just like that
	group.AddToggle("Enable MSAA", &g_mirrorReflectionEnableMSAA);
#endif

#if MIRROR_DEV
	group.AddTitle("Alpha-Blending Passes");
	group.AddToggle("RPASS_VISIBLE", &g_mirrorReflectionAlphaBlendingPasses, BIT(RPASS_VISIBLE));
	group.AddToggle("RPASS_LOD    ", &g_mirrorReflectionAlphaBlendingPasses, BIT(RPASS_LOD    ));
	group.AddToggle("RPASS_CUTOUT ", &g_mirrorReflectionAlphaBlendingPasses, BIT(RPASS_CUTOUT ));
	group.AddToggle("RPASS_DECAL  ", &g_mirrorReflectionAlphaBlendingPasses, BIT(RPASS_DECAL  ));
	group.AddToggle("RPASS_FADING ", &g_mirrorReflectionAlphaBlendingPasses, BIT(RPASS_FADING ));
	group.AddToggle("RPASS_ALPHA  ", &g_mirrorReflectionAlphaBlendingPasses, BIT(RPASS_ALPHA  ));
	group.AddToggle("RPASS_WATER  ", &g_mirrorReflectionAlphaBlendingPasses, BIT(RPASS_WATER  ));
	group.AddToggle("RPASS_TREE   ", &g_mirrorReflectionAlphaBlendingPasses, BIT(RPASS_TREE   ));

	group.AddTitle("Depth-Writing Passes");
	group.AddToggle("RPASS_VISIBLE", &g_mirrorReflectionDepthWritingPasses, BIT(RPASS_VISIBLE));
	group.AddToggle("RPASS_LOD    ", &g_mirrorReflectionDepthWritingPasses, BIT(RPASS_LOD    ));
	group.AddToggle("RPASS_CUTOUT ", &g_mirrorReflectionDepthWritingPasses, BIT(RPASS_CUTOUT ));
	group.AddToggle("RPASS_DECAL  ", &g_mirrorReflectionDepthWritingPasses, BIT(RPASS_DECAL  ));
	group.AddToggle("RPASS_FADING ", &g_mirrorReflectionDepthWritingPasses, BIT(RPASS_FADING ));
	group.AddToggle("RPASS_ALPHA  ", &g_mirrorReflectionDepthWritingPasses, BIT(RPASS_ALPHA  ));
	group.AddToggle("RPASS_WATER  ", &g_mirrorReflectionDepthWritingPasses, BIT(RPASS_WATER  ));
	group.AddToggle("RPASS_TREE   ", &g_mirrorReflectionDepthWritingPasses, BIT(RPASS_TREE   ));
#endif // MIRROR_DEV
}
#endif // __BANK

__COMMENT(static) const grcViewport* CRenderPhaseMirrorReflectionInterface::GetViewport()
{
	return g_mirrorReflectionViewport;
}

__COMMENT(static) const grcViewport* CRenderPhaseMirrorReflectionInterface::GetViewportNoObliqueProjection()
{
	return PS3_SWITCH(&g_mirrorReflectionViewportNoObliqueProjection, g_mirrorReflectionViewport);
}

__COMMENT(static) CRenderPhaseScanned* CRenderPhaseMirrorReflectionInterface::GetRenderPhase()
{
	return g_mirrorRenderPhase;
}

__COMMENT(static) void CRenderPhaseMirrorReflectionInterface::SetIsMirrorUsingWaterReflectionTechniqueAndSurface_Render(
	bool bIsMirrorUsingWaterReflectionTechnique, bool bIsMirrorUsingWaterSurface)
{
	g_mirrorReflectionUsingWaterReflectionTechniqueRT = bIsMirrorUsingWaterReflectionTechnique;
	g_mirrorReflectionUsingWaterSurfaceRT = bIsMirrorUsingWaterSurface;
}

__COMMENT(static) bool CRenderPhaseMirrorReflectionInterface::GetIsMirrorUsingWaterReflectionTechnique_Render()
{
	return g_mirrorReflectionUsingWaterReflectionTechniqueRT;
}

__COMMENT(static) bool CRenderPhaseMirrorReflectionInterface::GetIsMirrorUsingWaterSurface_Render()
{
	return g_mirrorReflectionUsingWaterSurfaceRT;
}

#if __XENON
__COMMENT(static) bool CRenderPhaseMirrorReflectionInterface::GetIsMirrorUsingWaterReflectionTechnique_Update()
{
	return g_mirrorReflectionUsingWaterReflectionTechnique;
}
#endif // __XENON
