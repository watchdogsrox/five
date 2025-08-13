////////////////////////////////////////////////////////////////////////////////////
// Title	:	Mirrors.h
// Author	:	Obbe
// Started	:	14/6/05
//			:	Things having to do with mirrors go here.
//				This file works closely with multirender.cpp
////////////////////////////////////////////////////////////////////////////////////
 
#ifndef _MIRRORS_H_
#define _MIRRORS_H_

// rage headers
#include "grcore/effect_typedefs.h"
#include "vector/color32.h"
#include "vectormath/vec4v.h"

// framework headers
#include "fwrenderer/renderthread.h"
#include "fwutil/xmacro.h"

// game headers
#include "modelinfo/MloModelInfo.h"

namespace rage
{
	class bkGroup;
	class grcRenderTarget;
	class grcViewport;
	class grmShader;
	class fwRoomSceneGraphNode;
};

class CRenderPhaseScanned;
class CPortalInfo;

#define MIRROR_DEFAULT_OFFSET                   (0.00f)
#define MIRROR_DEFAULT_MAX_PIXEL_DENSITY        (1.25f)
#define MIRROR_DEFAULT_MAX_PIXEL_DENSITY_BORDER (4.00f)
#define MIRROR_DEFAULT_OBLIQUE_PROJ_SCALE       (0.50f) // seems to fix clipping artefacts ..
#define MIRROR_DEFAULT_EXTERIOR_DISTANCE        (20.0f)
#define MIRROR_DEFAULT_INTERIOR_WATER_OFFSET    (-0.015f)
#define MIRROR_DEFAULT_INTERIOR_WATER_SLOPE     (-0.002f)

#define MIRROR_DEV (1 && (__DEV || (1 && __BANK && __XENON)))

#if MIRROR_DEV
	#define MIRROR_DEV_ONLY(...) __VA_ARGS__
	#if DEV_SWITCH_ASSERT
		#define MIRROR_DEV_SWITCH(_if_DEV_,_else_) devSwitchTest((_if_DEV_),(_else_),#_if_DEV_,#_else_)
	#else
		#define MIRROR_DEV_SWITCH(_if_DEV_,_else_) _if_DEV_
	#endif
#else
	#define MIRROR_DEV_ONLY(...)
	#define MIRROR_DEV_SWITCH(_if_DEV_,_else_) _else_
#endif

#define MIRROR_USE_STENCIL_MASK  (0 && MIRROR_DEV && __PS3)
#define MIRROR_USE_CRACK_TEXTURE (0 && MIRROR_DEV && __PS3) // probably works on 360, but i haven't tested it yet

#if MIRROR_USE_STENCIL_MASK
	#define MIRROR_USE_STENCIL_MASK_ONLY(x) x
#else
	#define MIRROR_USE_STENCIL_MASK_ONLY(x)
#endif

class CMirrors
{
public:
	static void Init();
	static void Shutdown();

	static void CreateRenderTargets();
#if RSG_PC
	static void DeviceLost();
	static void DeviceReset();
	static int GetResolutionSetting(int width, int height);
#endif // RSG_PC

#if __BANK
	static void AddWidgets(bkGroup& bk);
#endif // __BANK

	static Vec3V*             GetMirrorCorners()                              { return ms_mirrorCorners; }
	static Vec4V_Out          GetMirrorPlane()                                { return ms_mirrorPlane; }
	static Vec4V_Out          GetMirrorBounds_renderthread()                  { return ms_mirrorParams[gRenderThreadInterface.GetRenderBuffer()][MIRROR_PARAM_BOUNDS]; } // renderthread
	static grcRenderTarget*   GetMirrorRenderTarget(bool MSAA_ONLY(bMSAA = true))  { return MSAA_ONLY(!bMSAA ? ms_pRenderTargetResolved :) ms_pRenderTarget; }
	static grcRenderTarget*   GetMirrorDepthTarget()                          { return ms_pDepthTarget; }
#if __D3D11
	static grcRenderTarget*	  GetMirrorDepthResolved()						  { return ms_pDepthTargetResolved; }
#endif //__D3D11
	static const grcTexture*  GetMirrorTexture()                              { return ms_pRenderTargetResolved ? ms_pRenderTargetResolved : ms_pRenderTarget; }
	static void               SetMirrorWaterRenderTarget(grcRenderTarget* rt) { ms_pWaterRenderTarget = rt; }
	static void               SetMirrorWaterDepthTarget(grcRenderTarget* rt)  { ms_pWaterDepthTarget = rt; }
	static grcRenderTarget*   GetMirrorWaterRenderTarget()                    { return ms_pWaterRenderTarget; }
#if RSG_PC
	static void               SetMirrorWaterMSAARenderTarget(grcRenderTarget* rt) { ms_pWaterMSAARenderTarget = rt; }
	static grcRenderTarget*   GetMirrorWaterMSAARenderTarget()                { return ms_pWaterMSAARenderTarget; }
#endif
#if __XENON
	static grcRenderTarget*   GetWaterRenderTargetAlias()                     { return ms_pWaterRenderTargetAlias; }
#endif //__XENON
	static grcRenderTarget*   GetMirrorWaterDepthTarget()                     { return ms_pWaterDepthTarget; }
#if __D3D11
	static grcRenderTarget*   GetMirrorDepthTarget_ReadOnly() { if (GRCDEVICE.IsReadOnlyDepthAllowed()) { return ms_pDepthTarget_CopyOrReadOnly; } else { return NULL; } }
	static grcRenderTarget*   GetMirrorDepthTarget_Copy()     { if (GRCDEVICE.IsReadOnlyDepthAllowed()) { return NULL; } else { return ms_pDepthTarget_CopyOrReadOnly; } }

	static void               SetMirrorWaterDepthTarget_CopyOrReadOnly(grcRenderTarget* rt)  { ms_pWaterDepthTarget_CopyOrReadOnly = rt; }
	static grcRenderTarget*   GetMirrorWaterDepthTarget_ReadOnly() { if (GRCDEVICE.IsReadOnlyDepthAllowed()) { return ms_pWaterDepthTarget_CopyOrReadOnly; } else { return NULL; } }
	static grcRenderTarget*   GetMirrorWaterDepthTarget_Copy()     { if (GRCDEVICE.IsReadOnlyDepthAllowed()) { return NULL; } else { return ms_pWaterDepthTarget_CopyOrReadOnly; } }
#endif // __D3D11
	static const grcViewport* GetMirrorViewport();

#if DEVICE_MSAA
	static int GetResolutionX();
	static int GetResolutionY();
	static grcDevice::MSAAMode GetMSAA();
	static void ResolveMirrorRenderTarget();
#if __D3D11
	static void ResolveMirrorDepthTarget();
#endif //__D3D11
#endif

	static Vec4V_Out GetAdjustedClipPlaneForInteriorWater(Vec4V_In plane);
	static void UpdateViewportForScan();
	static bool GetIsMirrorVisible(bool bNoAssert = false);
	static bool GetIsMirrorVisibleWaterSurfaceInProximity(); // [HACK GTAV]
	static bool GetIsMirrorVisibleNoAssert() { return GetIsMirrorVisible(false); }
	static bool GetIsMirrorVisibleInProximityOnly() { return ms_scanResultsMirrorVisibleInProximityOnly; }
	static bool GetIsMirrorFrustumCullDisabled() { return ms_scanResultsMirrorFrustumCullDisabled; } // [HACK GTAV] -- hack for v_lab interior water
	static bool GetIsMirrorUsingWaterReflectionTechnique(bool bIsUsingMirrorWaterSurface);

#if MIRROR_DEV
	// TODO -- hook these up to PostScan.cpp
	static bool ShouldAlwaysForceUpAlpha() { return ms_debugMirrorShouldAlwaysForceUpAlpha; }
	static bool AllowAlphaCullInPostScan() { return ms_debugMirrorAllowAlphaCullInPostScan; }
	static bool AllowFading() { return ms_debugMirrorAllowFading; }
#endif // MIRROR_DEV

	static fwRoomSceneGraphNode* GetScanMirrorRoomSceneNode() { return ms_scanResultsMirrorRoomSceneNode; }
	static int GetScanMirrorPortalIndex() { return ms_scanResultsMirrorPortalIndex; }
	static Vec3V_Out GetScanMirrorCorner(int idx) { return ms_scanResultsMirrorCorners[idx]; }
	static bool GetScanIsWaterReflectionDisabled() { return GetIsMirrorVisible(); } // water is disabled if the mirror is visible

	static void SetMirrorRenderPhase(CRenderPhaseScanned* rp) { Assert(ms_pRenderPhaseNew == NULL || rp == NULL); /*allow only 1 instance of MirrorReflectionRP*/ ms_pRenderPhaseNew = rp; }
	static CRenderPhaseScanned* GetMirrorRenderPhase() { return ms_pRenderPhaseNew; }

	static void SetMirrorPortalFlags(const CPortalFlags portalFlags) { ms_mirrorPortalFlags = portalFlags; }
	static const CPortalFlags GetMirrorPortalFlags() { return ms_mirrorPortalFlags; }

	static void SetMirrorCorners(const Vec3V corners[4]) { ms_mirrorCorners[0] = corners[0]; ms_mirrorCorners[1] = corners[1]; ms_mirrorCorners[2] = corners[2]; ms_mirrorCorners[3] = corners[3]; }
	static void SetMirrorPlane(Vec4V_In plane) { ms_mirrorPlane = plane; }
	static void SetMirrorBounds(float x, float y, float z, float w, float windowX = 0.0f, float windowY = 0.0f, float windowW = 1.0f, float windowH = 1.0f) { ms_mirrorBounds = Vec4V(x*windowW + windowX, y*windowH + windowY, z*windowW, w*windowH); }

	static void SetShaderParams();

#if MIRROR_DEV
	static void RenderDebug(bool MIRROR_USE_STENCIL_MASK_ONLY(bStencilMask));
#endif // MIRROR_DEV

private:
	enum
	{
		MIRROR_PARAM_BOUNDS,
		MIRROR_PARAM_COLOUR,
		MIRROR_PARAM_COUNT
	};

	static CRenderPhaseScanned*    ms_pRenderPhaseNew; // pointer to renderphase
	static CPortalFlags            ms_mirrorPortalFlags;

	static u32                     ms_scanResultsFrameIndex;
	static bool                    ms_scanResultsMirrorVisible;
	static bool                    ms_scanResultsMirrorVisibleInProximityOnly;
	static bool                    ms_scanResultsMirrorVisibleWaterSurfaceInProximity;
	static bool                    ms_scanResultsMirrorFrustumCullDisabled; // hack for v_lab interior water
	static fwRoomSceneGraphNode*   ms_scanResultsMirrorRoomSceneNode;
	static int                     ms_scanResultsMirrorPortalIndex;
	static Vec3V                   ms_scanResultsMirrorCorners[4];

	static Vec3V                   ms_mirrorCorners[4];
	static Vec4V                   ms_mirrorPlane;
	static Vec4V                   ms_mirrorBounds;

	static grcRenderTarget*        ms_pRenderTarget;
	static grcRenderTarget*		   ms_pRenderTargetResolved;
	static grcRenderTarget*        ms_pDepthTarget;
	static grcRenderTarget*        ms_pWaterRenderTarget;
#if RSG_PC
	static grcRenderTarget*        ms_pWaterMSAARenderTarget;
#endif
#if __XENON
	static grcRenderTarget*        ms_pWaterRenderTargetAlias;
#endif //__XENON
	static grcRenderTarget*        ms_pWaterDepthTarget;
#if __D3D11
	static grcRenderTarget*        ms_pDepthTarget_CopyOrReadOnly;
	static grcRenderTarget*        ms_pWaterDepthTarget_CopyOrReadOnly;
	static grcRenderTarget*        ms_pDepthTargetResolved;
#endif // __D3D11

	// Double buffered for update thread and render thread access
	static Vec4V                   ms_mirrorParams[2][MIRROR_PARAM_COUNT];

#if MIRROR_DEV
	static grmShader*              ms_mirrorShader;
	static grcEffectTechnique      ms_mirrorTechnique;
	static grcEffectVar            ms_mirrorReflectionTexture_ID;
	static grcEffectVar            ms_mirrorParams_ID;
#if MIRROR_USE_STENCIL_MASK
	static grcEffectVar            ms_mirrorDepthBuffer_ID;
	static grcEffectVar            ms_mirrorProjectionParams_ID;
#endif // MIRROR_USE_STENCIL_MASK
#if MIRROR_USE_CRACK_TEXTURE
	static grcTexture*             ms_mirrorCrackTexture;
	static grcEffectVar            ms_mirrorCrackTexture_ID;
	static grcEffectVar            ms_mirrorCrackParams_ID;
#endif // MIRROR_USE_CRACK_TEXTURE
#endif // MIRROR_DEV

#if RSG_PC
	static bool ms_Initialized;
#endif

public:
	enum eMirrorRenderSkyMode
	{
		MIRROR_RENDER_SKY_MODE_NONE,
		MIRROR_RENDER_SKY_MODE_FIRST,
		MIRROR_RENDER_SKY_MODE_LAST,
		MIRROR_RENDER_SKY_MODE_BEFORE_ALPHA,
		MIRROR_RENDER_SKY_MODE_COUNT,

		MIRROR_RENDER_SKY_MODE_DEFAULT = MIRROR_RENDER_SKY_MODE_BEFORE_ALPHA
	};

#if MIRROR_DEV
	enum eMirrorClipMode
	{
		MIRROR_CLIP_MODE_NONE       , // no clipping
		MIRROR_CLIP_MODE_SCREENSPACE, // clip screenspace bounds
		MIRROR_CLIP_MODE_VIEWSPACE  , // clip to view frustum
		MIRROR_CLIP_MODE_COUNT
	};

	static bool    ms_debugMirrorShowInfo;
	static char    ms_debugMirrorShowInfoMessage[112];
	static bool    ms_debugMirrorUseWaterReflectionTechniqueAlways;
	static bool    ms_debugMirrorUseWaterReflectionTechniqueNever;
	static bool    ms_debugMirrorShouldAlwaysForceUpAlpha;
	static bool    ms_debugMirrorAllowAlphaCullInPostScan;
	static bool    ms_debugMirrorAllowFading;
	static float   ms_debugMirrorScanExteriorDistance; // max distance at which mirror is visible if the camera is outside
	static bool    ms_debugMirrorScanCurrentRoom; // perform an extra check to see if mirror is visible in the current room, since visibility is from previous frame (see BS#595865)
	static bool    ms_debugMirrorScanCurrentRoomVisibleInFrustum;
	static bool    ms_debugMirrorTraceSelectedEntityOnCameraFlip;
	static bool    ms_debugMirrorTraceScanVisibilityDumpOneFrame; // not hooked up, see shelved CL#3081093
	static bool    ms_debugMirrorDrawDebugShader;
	static bool    ms_debugMirrorDrawDebugShaderShowRT;
	static float   ms_debugMirrorDrawDebugShaderShowRTOffsetX;
	static float   ms_debugMirrorDrawDebugShaderShowRTOffsetY;
	static float   ms_debugMirrorDrawDebugShaderShowRTSize;
	static float   ms_debugMirrorScale;
	static float   ms_debugMirrorOffset;
	static Vec3V   ms_debugMirrorOffsetV;
	static Color32 ms_debugMirrorColour;
	static float   ms_debugMirrorBrightness;
#if MIRROR_USE_STENCIL_MASK
	static bool    ms_debugMirrorStencilMaskEnabled;
	static bool    ms_debugMirrorStencilMaskScissor;
#endif // MIRROR_USE_STENCIL_MASK
#if MIRROR_USE_CRACK_TEXTURE
	static bool    ms_debugMirrorCrackEnabled;
	static float   ms_debugMirrorCrackAmount;
	static float   ms_debugMirrorCrackRefraction;
	static float   ms_debugMirrorCrackLuminance;
#endif // MIRROR_USE_CRACK_TEXTURE
	static bool    ms_debugMirrorFloor;
	static bool    ms_debugMirrorFloorScanFromGBufStartingNodeOn;
	static bool    ms_debugMirrorFloorScanFromGBufStartingNodeOff;
	static bool    ms_debugMirrorFloorUseLODs;
	static bool    ms_debugMirrorWireframe;
	static bool    ms_debugMirrorLocalLightIntensityOverrideEnabled;
	static float   ms_debugMirrorLocalLightIntensityOverride;
	static bool    ms_debugMirrorRenderDistantLights;
	static bool    ms_debugMirrorRenderCoronas;
	static bool    ms_debugMirrorRenderSkyAlways; // force sky rendering even if there are no sky portals visible
	static int     ms_debugMirrorRenderSkyMode; // eMirrorRenderSkyMode
	static bool    ms_debugMirrorDrawFilter_RENDER_OPAQUE;
	static bool    ms_debugMirrorDrawFilter_RENDER_ALPHA;
	static bool    ms_debugMirrorDrawFilter_RENDER_FADING;
	static bool    ms_debugMirrorDrawFilter_RENDER_TREES;
	static bool    ms_debugMirrorDrawFilter_RENDER_DECALS_CUTOUTS;
	static bool    ms_debugMirrorUseAlphaTestForCutout;
	static float   ms_debugMirrorUseAlphaTestForCutoutAlphaRef;
	static bool    ms_debugMirrorForceDirectionalLightingOnForExteriorView;
	static bool    ms_debugMirrorForceDirectionalLightingOn;
	static bool    ms_debugMirrorForceDirectionalLightingOff;
	static bool    ms_debugMirrorForcePortalTraversalOn;
	static bool    ms_debugMirrorForcePortalTraversalOff;
	static bool    ms_debugMirrorForceExteriorViewOn;
	static bool    ms_debugMirrorForceExteriorViewOff;
	static bool    ms_debugMirrorExteriorViewRenderLODs;
	static bool    ms_debugMirrorClipAndStorePortals;
	static bool    ms_debugMirrorCullingPlanesEnabled;
	static u32     ms_debugMirrorCullingPlaneFlags;
//#if __PS3
	static bool    ms_debugMirrorUseObliqueProjection;
	static float   ms_debugMirrorUseObliqueProjectionScale;
//#endif // __PS3
#if !__PS3
	static bool    ms_debugMirrorUseHWClipPlane;
#endif // !__PS3
	static bool    ms_debugMirrorInteriorWaterOffsetEnabled;
	static float   ms_debugMirrorInteriorWaterOffset;
	static bool    ms_debugMirrorInteriorWaterSlopeEnabled;
	static float   ms_debugMirrorInteriorWaterSlope;
	static bool    ms_debugMirrorRestrictedProjection;
	static bool    ms_debugMirrorMaxPixelDensityEnabled;
	static float   ms_debugMirrorMaxPixelDensity;
	static float   ms_debugMirrorMaxPixelDensityBorder;
	static int     ms_debugMirrorClipMode; // eMirrorClipMode
	static bool    ms_debugMirrorShowBounds;
	static bool    ms_debugMirrorShowBoundsRT;
	static u32     ms_debugMirrorPrivateFlags; // these really are private
#endif // MIRROR_DEV
};

#endif
