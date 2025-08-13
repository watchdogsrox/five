// ===================================================
// renderer/renderphases/renderphasecascadeshadows.cpp
// (c) 2011 RockstarNorth
// ===================================================

// http://repi.blogspot.com/2009/03/gdc09-shadows-decals-d3d10-techniques.html
// http://gameangst.com/?p=339
// http://msdn.microsoft.com/en-us/library/ee416307(v=vs.85).aspx
// http://developer.download.nvidia.com/SDK/10.5/opengl/src/cascaded_shadow_maps/doc/cascaded_shadow_maps.pdf
// http://pixelstoomany.wordpress.com/2007/09/21/fast-percentage-closer-filtering-on-deferred-cascaded-shadow-maps/
// http://www.youtube.com/watch?v=DJxUbN3mm7g

// Uncharted - 1x1216x1216 (1478656 pixels, Z16 so only 2.957MB .. split into 4 cascades?)
// MP3       - 3x1024x1024 (3145728 pixels, 12MB)
// RDR       - 4x800x800   (2560000 pixels, 9.765MB .. 3.515MB over current)
// Fracture  - 5x640x640   (2048000 pixels, 7.8125MB)
// GTAV      - 1x1280x1280 (1638400 pixels, 6.25MB)

#include "profile/profiler.h"
#include "profile/timebars.h"
#if __D3D
#include "system/xtl.h"
#include "system/d3d9.h"
#endif // __D3D
#if RSG_ORBIS
#include "grcore/texturefactory_gnm.h"
#include "grcore/texture_gnm.h"
#include "grcore/rendertarget_gnm.h"
#endif
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "math/vecrand.h"
#include "grcore/AA_shared.h"
#include "grcore/allocscope.h"
#include "grcore/image.h"
#include "grcore/setup.h"
#include "grcore/texturegcm.h"
#include "grcore/debugdraw.h"
#include "grcore/stateblock.h"
#include "grcore/stateblock_internal.h"
#include "grcore/texturexenon.h"
#if __XENON
#include "grcore/wrapper_d3d.h"
#endif // __XENON
#include "grmodel/shaderfx.h"
#include "input/mouse.h"
#include "rmptfx/ptxconfig.h"
#include "spatialdata/aabb.h"
#include "system/memory.h"

#if __PS3
#include "edge/geom/edgegeom_config.h"
#if ENABLE_EDGE_CULL_DEBUGGING
#include "edge/geom/edgegeom_structs.h"
#endif // ENABLE_EDGE_CULL_DEBUGGING
#endif // __PS3

#if RSG_ORBIS
#include "grcore/texturefactory_gnm.h"
#endif

#include "rmptfx/ptxdraw.h"

#include "fwutil/xmacro.h"
#include "fwsys/timer.h"
#include "fwsys/gameskeleton.h"
#include "fwmaths/vectorutil.h"
#include "fwscene/scan/RenderPhaseCullShape.h"
#include "fwscene/scan/VisibilityFlags.h"
#include "fwscene/scan/ScanCascadeShadows.h"
#include "fwscene/scan/ScanDebug.h"
#include "fwscene/scan/ScanNodesDebug.h"
#include "fwscene/scan/Scan.h"
#include "fwdrawlist/drawlistmgr.h"
#include "fwrenderer/renderlistgroup.h"

#include "camera/viewports/ViewportManager.h"
#include "camera/viewports/viewport.h"
#include "camera/base/BaseCamera.h"
#include "camera/CamInterface.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/replay/ReplayFreeCamera.h"
#include "camera/switch/SwitchCamera.h"
#include "camera/switch/SwitchDirector.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera\cinematic\camera\tracking\CinematicFirstPersonIdleCamera.h"
#include "camera\cinematic\camera\tracking\CinematicHeliChaseCamera.h"
#include "camera\gameplay\aim\FirstPersonPedAimCamera.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "control/gamelogic.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/BankUtil.h"
#include "debug/BlockView.h"
#include "debug/DebugDraw/DebugCameraLock.h"
#include "debug/GtaPicker.h"
#include "debug/LightProbe.h"
#include "debug/Rendering/DebugLighting.h"
#include "debug/Rendering/DebugDeferred.h"
#include "debug/TextureViewer/TextureViewer.h" // filthy hack for CDebugTextureViewerInterface::GetWorldPosUnderMouse
#include "debug/TiledScreenCapture.h"
#include "frontend\NewHud.h"
#include "game/Clock.h"
#include "modelinfo/MloModelInfo.h"
#include "Peds/ped.h"
#include "scene/scene.h"
#include "scene/Entity.h"
#include "scene/debug/PostScanDebug.h"
#include "scene/portals/PortalInst.h"
#include "scene/world/GameWorld.h"
#include "scene/world/GameWorldHeightMap.h"
#include "system/controlMgr.h"
#include "system/SettingsManager.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/Debug/EntitySelect.h"
#include "renderer/DrawLists//drawListMgr.h"
#include "renderer/Lights/lights.h"
#include "renderer/Lights/TiledLighting.h"
#include "renderer/occlusion.h"
#include "renderer/renderListGroup.h"
#include "renderer/RenderListBuilder.h"
#include "renderer/rendertargets.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/Shadows/CascadeShadowsDebug.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "vfx/particles/PtFxManager.h"
#include "vfx/sky/Sky.h"
#include "vfx/VfxHelper.h"
#include "timecycle/TimeCycleConfig.h"
#include "timecycle/TimeCycle.h"
#include "task\Movement\TaskParachute.h"
#include "task\Movement\TaskFall.h"
#include "Peds\PedIntelligence.h"
#include "vehicles/vehicle.h"
#include "../../shader_source/Lighting/Shadows/cascadeshadows_common.fxh"
#include "control/replay/replay.h"

#if __BANK
#include "cutscene/CutsceneCameraEntity.h"
#include "cutscene/CutSceneDebugManager.h"
#endif

// Undefine this to turn off all NV shadow changes.  There is also a Rag widget boolean to disable them dynamically.
#define USE_NV_SHADOW_LIB RSG_PC

#if USE_NV_SHADOW_LIB

#pragma comment(lib, "GFSDK_ShadowLib.win64.lib")
#define __GFSDK_DX11__
#include "../../3rdParty/NVidia/GFSDK_ShadowLib/include/GFSDK_ShadowLib.h"
extern NV_ShadowLib_Ctx g_shadowContext;	// From device_d3d11.cpp.

#endif //USE_NV_SHADOW_LIB

#if USE_AMD_SHADOW_LIB

#if __OPTIMIZED
#pragma comment(lib, "AMD_Shadows_X64.lib")
#else
#pragma comment(lib, "AMD_Shadows_X64d.lib")
#endif  // __OPTIMIZED
#include "../../3rdParty/AMD/AMD_Shadows/include/AMD_Shadows.h"

extern AMD::SHADOWS_DESC g_amdShadowDesc;	// From device_d3d11.cpp.

enum { AMD_CASCADE_FLAG = DEFERRED_MATERIAL_SPAREOR2 };

namespace AMDCSMStatic {
	static const int AMD_NUM_SHADOW_SETTINGS = 3;
	static const float AMDSunWidth[AMD_NUM_SHADOW_SETTINGS][CASCADE_SHADOWS_COUNT] = 
	{{500.0f,  20.0f,  1.0f, 1.0f},   // normal
	 {500.0f, 20.0f, 10.0f, 4.0f},   // high
	 {500.0f, 20.0f, 10.0f, 4.0f}};  // very high

	static const int AMDFilterRadiusIndex[AMD_NUM_SHADOW_SETTINGS][CASCADE_SHADOWS_COUNT] = 
	{{3, 0, 0, 0},   // normal
	 {3, 1, 0, 0},   // high
	 {3, 2, 0, 0}};  // very high

	static const float AMDCascadeBorder[AMD_NUM_SHADOW_SETTINGS] = {0.035f, 0.025f, 0.015f};
}

#endif //USE_AMD_SHADOW_LIB

#define CSM_ASYMMETRICAL_VIEW                  (1 && (__WIN32PC || RSG_DURANGO || RSG_ORBIS))
#define CSM_QUADRANT_TRACK_ADJ                 (1 && __DEV)
#define CSM_DEPTH_HISTOGRAM                    (0 && __DEV && TILED_LIGHTING_ENABLED)
#define CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK (1 && __DEV && CSM_DEPTH_HISTOGRAM)
#define CSM_DYNAMIC_DEPTH_RANGE                (1 && (__D3D11 || RSG_ORBIS)  && TILED_LIGHTING_ENABLED)
#define CSM_EDGE_OCCLUDER_QUAD_EXCLUSION       (1 && __PS3 && ENABLE_EDGE_OCCLUSION)
#define CSM_PORTALED_SHADOWS                   (1)

#define CSM_USE_DUMMY_COLOUR_TARGET            (__WIN32PC && __D3D9)
#define CSM_USE_SPECIAL_FULL_TEXTURE           (RSG_ORBIS || __D3D11)

#define VARIABLE_RES_SHADOWS					(RSG_PC || RSG_DURANGO || RSG_ORBIS)

RENDER_OPTIMISATIONS()

namespace CSMStatic {
	static BankBool sAlwaysAllowGrassCulling = false;
	static BankFloat sGrassCullLastCascadeEndThreshold = 200.0f;
}

PARAM(nohairshadows, "Disables hair rendering in shadow pass");
PARAM(contacthardeningshadows, "Use NVIDIA contact hardening shadows");
PARAM(contacthardeningshadowspcfmode, "NVIDIA contact hardening shadows PCF mode - not specified = manual, 0 = hw, anything else = scale");
PARAM(AMDContactHardeningShadows, "Use AMD contact hardening shadows");

class grcGpuTimerAvr
{
public:
				grcGpuTimerAvr( );
				~grcGpuTimerAvr( );

	float*		GetTimeAvr( ) { return &TimeAvr; };

	void		Start( );
	void		Stop( );

private:
	static const int AverageOver = 128;

	grcGpuTimer	Timer;

	float		TimeAvr;
	float		TimeTotal;
	int			TimeIndex;
	float		TimeArray[ AverageOver ];
};

grcGpuTimerAvr::grcGpuTimerAvr( )
	: TimeAvr( 0.0f ), TimeTotal( 0.0f ), TimeIndex( 0 )
{
	for( int i = 0; i < AverageOver; i++ )
	{
		TimeArray[ i ] = 0.0f;
	}
}

grcGpuTimerAvr::~grcGpuTimerAvr( )
{
}

void grcGpuTimerAvr::Start( )
{
	Timer.Start();
}

void grcGpuTimerAvr::Stop( )
{
	float TimeSingle = Timer.Stop();

	TimeTotal -= TimeArray[ TimeIndex ];
	TimeTotal += TimeSingle;
	TimeArray[ TimeIndex ] = TimeSingle;
	TimeIndex++;
	if ( TimeIndex == AverageOver )
	{
		TimeIndex = 0;
	}

	TimeAvr = TimeTotal / (float)AverageOver;
}

grcGpuTimerAvr g_ShadowRevealTime_cascades[5];
grcGpuTimerAvr g_ShadowRevealTime_Total;
grcGpuTimerAvr g_ShadowRevealTime_SmoothStep;
grcGpuTimerAvr g_ShadowRevealTime_Cloud;
grcGpuTimerAvr g_ShadowRevealTime_Edge;
grcGpuTimerAvr g_ShadowRevealTime_mask_total;
grcGpuTimerAvr g_ShadowRevealTime_mask_reveal_MS0;
grcGpuTimerAvr g_ShadowRevealTime_mask_reveal_MS;

#if RSG_ORBIS
NOSTRIP_XPARAM(shadowQuality);
#endif

#if SHADOW_ENABLE_ASYNC_CLEAR
static bank_bool g_bUseAsyncClear = false;
static grcFenceHandle g_AsyncClearFence;
#endif // SHADOW_ENABLE_ASYNC_CLEAR

#if GS_INSTANCED_SHADOWS
PARAM(useInstancedShadows, "Enables instanced cascade shadows pass");
static bool g_bInstancing = false;
#endif // GS_INSTANCED_SHADOWS

static bank_bool g_UseDynamicDistribution = true;

#define NUM_FILTER_SOFTNESS 4
const int filterSoftness[NUM_FILTER_SOFTNESS] = {CSM_ST_LINEAR, CSM_ST_BOX3x3, CSM_ST_POISSON16_RPDB_GNORM, CSM_ST_SOFT16};

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
PARAM(nosoftshadows, "Disables soft shadows");
static bool AreSoftShadowsAllowed()
{
	// MSAA is supported but very expensive
#if RSG_PC
	const CSettings::eSettingsLevel  softLevel = CSettingsManager::GetInstance().GetSettings().m_graphics.m_Shadow_SoftShadows;
	if ((grcEffect::GetShaderQuality() == 0) || (softLevel >= CSettings::Low && softLevel <= CSettings::High) || (softLevel == CSettings::Custom) || (softLevel == CSettings::Special))
		return false;
#endif
	return !PARAM_nosoftshadows.Get();
}
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING
#if RMPTFX_USE_PARTICLE_SHADOWS
PARAM(noparticleshadows, "Disables particle shadows");
#endif // RMPTFX_USE_PARTICLE_SHADOWS

PARAM(use32BitShadowBuffer, "Use 32, rather than 24 bit shadow buffer");
PARAM(use16BitShadowBuffer, "Use 16, rather than 16 bit shadow buffer");
PARAM(useSimplerShadowFiltering, "use two taps filtering for shadow reveal");

#if RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
PARAM(renderAlphaShadows,"Render alpha objects into the alpha shadow buffer");
ASSERT_ONLY(static int g_ParticleShadowFlagSetThreadID = -1;)
ASSERT_ONLY(static int g_ParticleShadowFlagAccessThreadID = -1;)
#endif // RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW

// ================================================================================================

/*
RDR2 settings:

PS3:
NearSlice       =   0.500000  5.500000 16.000000  44.000000
FarSlice        =   6.500000 17.000000 45.000000 130.000000
LightDepthScale = 100.000000 40.060001 10.000000   5.000000
DepthBias       =   0.000050  0.000150  0.000300   0.003500
BiasSlope       =   3.000000  2.500000  1.500000   1.500000
ViewportZShift  =   1.000000  1.000000  1.000000   1.000000
FovYDiv         =   0.400000  0.300000  0.380000   0.300000
FadeOutSMap     =  25.000000

360:
NearSlice       =   0.500000  5.500000 16.000000  44.000000 (same as PS3)
FarSlice        =   6.500000 17.000000 45.000000 130.000000 (same as PS3)
LightDepthScale = 200.000000 90.000000 35.000000   9.000000
DepthBias       =   0.000012  0.000035  0.000050   0.000100
BiasSlope       =   0.750000  1.300000  1.000000   3.000000
ViewportZShift  =   1.000000  0.500000  1.000000   1.000000
FovYDiv         =   0.700000  0.700000  0.700000   0.700000
FadeOutSMap     =  26.000000

(comparitor.x > 0.0f) ? float4(pos0,(4.0f*KERNELSIZE)/(2.0f*SHADOWMAP_SIZE)) :
(comparitor.y > 0.0f) ? float4(pos1,(3.0f*KERNELSIZE)/(2.0f*SHADOWMAP_SIZE)) :
(comparitor.z > 0.0f) ? float4(pos2,(2.0f*KERNELSIZE)/(2.0f*SHADOWMAP_SIZE)) :
                        float4(pos3,(1.0f*KERNELSIZE)/(2.0f*SHADOWMAP_SIZE)) ;
*/

#define CSM_DEFAULT_CUTOUT_ALPHA_REF              ((128.f-90.f)/255.f) // this is just a bias, actual alpha ref is subtracted in the shader so that the ref can be zero (which is supposed to be faster)
#define CSM_DEFAULT_CUTOUT_ALPHA_TO_COVERAGE      false

#define CSM_DEFAULT_BOX_EXACT_THRESHOLD           9.0f // only applied to last two cascades

#define CSM_DEFAULT_SPLIT_Z_MIN                   VIEWPORT_DEFAULT_NEAR // 0.5f
#define CSM_DEFAULT_SPLIT_Z_MAX                   256.0f

#define CSM_DEFAULT_SPLIT_Z_EXP_WEIGHT			  0.900f

#define CSM_DEFAULT_WORLD_HEIGHT_UPDATE           0
#define CSM_DEFAULT_WORLD_HEIGHT_MIN              -20.0f
#define CSM_DEFAULT_WORLD_HEIGHT_MAX              1500.0f

#define CSM_DEFAULT_RECVR_HEIGHT_UPDATE           1
#define CSM_DEFAULT_RECVR_HEIGHT_MIN              -20.0f
#define CSM_DEFAULT_RECVR_HEIGHT_MAX              800.0f
#define CSM_DEFAULT_RECVR_HEIGHT_UNDERWATER       -20.0f
#define CSM_DEFAULT_RECVR_HEIGHT_UNDERWATER_TRACK 256.0f // set to 0.0f to disable tracking

#define CSM_DEFAULT_SHADOW_DIRECTION_CLAMP        -0.35f

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
#define CSM_DEFAULT_SOFT_SHADOW_PENUMBRA_RADIUS   0.145f
#define CSM_DEFAULT_SOFT_SHADOW_MAX_KERNEL_SIZE   7
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING

#define CSM_PARTICLE_SHADOWS_FEATURE_LEVEL        1100

#define CSM_DISABLE_ALPHA_SHADOW_NIGHT			 false
#define CSM_DISABLE_PARTICLE_SHADOW_NIGHT		 true

bool		g_NeedResolve = true;
bool		g_HighAltitude = false;
u32			g_RasterStateUpdateId = 0;
float		g_RasterCacheThreshold = 0.0001f;

// ================================================================================================

#if __DEV

#define TRACK_FUNCTION(name,counter,bTrackRender) \
	{ \
		if (counter > 0) \
		{ \
			static int s_id = 0; \
			Displayf("TRACK_FUNCTION:" #name "[%d]", s_id); \
			if (bTrackRender) \
			{ \
				DLC_Add(Displayf, (const char*)"TRACK_FUNCTION:" #name "[%d]", s_id); \
			} \
			s_id++; \
		} \
	} \
	// end.

#else // not __DEV

#define TRACK_FUNCTION(name,counter,bTrackRender)

#endif // not __DEV

class grcViewportWindow : public grcViewport
{
public:
	grcViewportWindow() {}
	grcViewportWindow(const grcViewport& viewport, int x, int y, int w, int h) : grcViewport(viewport), m_window(Vec4V((float)x, (float)y, (float)w, (float)h))
	{
		if (0) // this is only necessary if we need to project in the update thread, i think .. actually this causes problems, but we don't need it anymore
		{
			grcViewport::ResetWindow();
			grcViewport::SetWindow(x, y, w, h);
		}
	}

	__forceinline Vec4V_Out GetWindow() const
	{
		return m_window;
	}

	void SetCurrentViewport() const
	{
		DLC(dlCmdSetCurrentViewport, (this));
		DLC_Add(SetCurrentViewport_render, GetWindow());
	}

private:
	static void SetCurrentViewport_render(Vec4V_In window)
	{
		const Vec4V temp = FloatToIntRaw<0>(window);
		const int x = temp.GetXi();
		const int y = temp.GetYi();
		const int w = temp.GetZi();
		const int h = temp.GetWi();

		grcViewport::GetCurrent()->ResetWindow();
		grcViewport::GetCurrent()->SetWindow(x, y, w, h);
	}

	Vec4V m_window; // {x,y,w,h}
};

#define CSM_PBOUNDS 0

#if CSM_PORTALED_SHADOWS

ScalarV_Out pointToLineDistance(Vec2V_In A, Vec2V_In B, Vec2V_In P)
{
	Vec2V D = B-A;
	Vec2V pA = P-A;
	return ( pA.GetX()  * D.GetY() -  pA.GetY()* D.GetX() ) * InvMag(D);	
}
bool InsideConvexPoly2D(Vec3V_In pt, Vec3V* v, int ASSERT_ONLY(n) , ScalarV_In tolerance) 
{
	Assert(n==4);
	Vec4V dist( pointToLineDistance( v[0].GetXY(), v[1].GetXY(), pt.GetXY()),
				pointToLineDistance( v[1].GetXY(), v[2].GetXY(), pt.GetXY()),
				pointToLineDistance( v[2].GetXY(), v[3].GetXY(), pt.GetXY()),
				pointToLineDistance( v[3].GetXY(), v[0].GetXY(), pt.GetXY()));

	VecBoolV insideV = IsGreaterThan( dist, Vec4V(tolerance));
	BoolV inside = insideV.GetX() & insideV.GetY() & insideV.GetZ() &insideV.GetW();
	return inside.Getb();
}
class ExteriorPortals
{
	struct DrawPortalInfo
	{
		Vec3V p[4];	  // W is used in p[0] for whether the portal is lit or not, p[1].w is the camera is close flag
		DrawPortalInfo() {}

		void SetIsLit( bool islit) { p[0].SetW(islit ? ScalarV(V_ONE) : ScalarV(V_ZERO)); }
		bool IsLit() const { return !IsEqualIntAll( p[0].GetW(), ScalarV(V_ZERO)); }
		void SetIsCloseToCamera( bool isClose) { p[1].SetW(isClose ? ScalarV(V_ONE) : ScalarV(V_ZERO)); }
		bool IsCloseToCamera() const { return !IsEqualIntAll( p[1].GetW(), ScalarV(V_ZERO)); }

		DrawPortalInfo( Vec3V_In p0, Vec3V_In p1,Vec3V_In p2,Vec3V_In p3)
		{
			p[0]=p0;p[1]=p1;p[2]=p2;p[3]=p3;
			SetIsLit(false);
			SetIsCloseToCamera(false);
		}
		Vec3V_Out GetCentroid() const
		{
			return (p[0]+p[1]+p[2]+p[3])*Vec3V(V_QUARTER);
		}
		Vec3V_Out GetNormal() const
		{
			Vec3V e0 = p[1]-p[0];
			Vec3V e1 = p[3]-p[0];
			return Normalize(Cross(e0,e1));
		}
		void Draw() const
		{
			grcBegin(drawTriStrip, 4);
			grcVertex3f(p[0]);
			grcVertex3f(p[1]);
			grcVertex3f(p[3]);
			grcVertex3f(p[2]);
			grcEnd();
		}
		Vec4V_Out CalcBound( Mat33V_In worldToShadow) const 
		{
			Vec3V bmin(V_FLT_MAX);
			Vec3V bmax(V_NEG_FLT_MAX);

			for (int i =0;i<4;i++)
			{
				Vec3V v = Multiply( worldToShadow, p[i]);
				bmin = Min( bmin, v);
				bmax = Max( bmax,v);
			}
			return Vec4V( bmin.GetX(), bmin.GetY(), bmax.GetX(), bmax.GetY());
		}
		void CalcBound( Mat44V_In worldToView, Vec3V_InOut rbmin, Vec3V_InOut rbmax) const 
		{
			Vec3V bmin(V_FLT_MAX);
			Vec3V bmax(V_NEG_FLT_MAX);
			for (int i =0;i<4;i++)
			{
				Vec4V rv = Multiply( worldToView, Vec4V(p[i], ScalarV(V_ONE)));
				Vec3V v = rv.GetXYZ()/ rv.GetW();
				bmin = Min( bmin, v);
				bmax = Max( bmax, v);
			}
			rbmin = bmin;
			rbmax = bmax;
		}
		void Draw( Vec3V_In lightDir)  const
		{
			grcBegin(drawTriStrip, 5*2);
			for (int edge = 0; edge < 5; edge++){
				Vec3V np = p[(edge+1)&3];
				grcVertex3f(np);
				grcVertex3f(np + lightDir);
			}
			grcEnd();

			// cap the bottom
			grcBegin(drawTriStrip, 4);
			grcVertex3f(p[1] + lightDir);
			grcVertex3f(p[0] + lightDir);
			grcVertex3f(p[2] + lightDir);
			grcVertex3f(p[3] + lightDir);
			grcEnd();

			// cap the top
			grcBegin(drawTriStrip, 4);
			grcVertex3f(p[0]);
			grcVertex3f(p[1]);
			grcVertex3f(p[3]);
			grcVertex3f(p[2]);
			grcEnd();
		}
		float Distance( Vec3V_In cpt) const {	
			return Dist( GetCentroid(), cpt).Getf();
		}
		ScalarV PlanarDistance( Vec3V_In cpt) const {	
			Vec3V r = cpt-p[0];
			return Abs(Dot(r,GetNormal()));
		}

		bool InLight( Vec3V_In cpt, Mat33V_In worldToShadow) const 
		{
			Vec3V spt[4];
			for (int i=0; i<4;i++)
				spt[i]=Multiply( worldToShadow, p[i]);	

			static dev_float threshold=-.6f;
			return InsideConvexPoly2D( cpt, spt, 4, ScalarVFromF32(threshold));
		}
		bool IsSunlightVisible(Vec3V_In lightDir, const grcViewport& vp) const
		{
			Vec3V pts[8];
			for(int i=0;i<4;i++)
				pts[i]=p[i];   
			bool lit = IsLit();
			if ( lit)
			{
				for (int i=4;i<8;i++)
					pts[i]=p[i-4]+lightDir;
			}
			return vp.IsPointSetVisible(pts,  lit ? 8 : 4);
		}

	};

	static const int MaxNumPortals = 64;

	static bank_float PortalExtrusionDistance;
	static bank_float PortalSizeScale;
	static bank_bool  EnableStencilTest;

	atFixedArray<DrawPortalInfo,MaxNumPortals> 				drawPortals;   
#if CSM_PBOUNDS	
	atFixedArray<Vec4V, CASCADE_SHADOWS_NUM_PORTAL_BOUNDS>	shadBounds;
#endif	
	Vec3V				ldir;
	bool				m_usePortals;
	bool				m_CameraJustOutsideAPortal;

#if __BANK
	static bool		m_testPortalOverlap;
	static bool		m_active;
	mutable bool	m_overlaps;
	static int		m_portalCount;
	static int      m_interiorsTraversed;
	static bool		m_interiorOnly;
#endif
	
	grcEffectTechnique	m_primeDepthTechnique;
	static grcDepthStencilStateHandle  m_fillShadow_DS;
	static grcDepthStencilStateHandle  m_reveal_DS;
	static grcDepthStencilStateHandle  m_direct_DS;
	static grcDepthStencilStateHandle  m_behind_DS;
	static grcDepthStencilStateHandle  m_inFront_DS;
	static grcDepthStencilStateHandle  m_inside_DS;
	static grcDepthStencilStateHandle  m_nodepth_DS;

	static grcRasterizerStateHandle    m_cullFront_RS;


public:

	static const grcDepthStencilStateHandle&	GetRevealDS()	{ return m_reveal_DS; }

	void Init()
	{
		m_primeDepthTechnique = GRCDEVICE.GetDefaultEffect().LookupTechnique("WriteDepthOnlyMRT");
		m_usePortals = true;
#if __BANK
		m_active = true;
		m_testPortalOverlap=true;
		m_overlaps=false;
#endif	

		grcDepthStencilStateDesc stencilOtherSetupDSS;
		stencilOtherSetupDSS.DepthEnable              = FALSE;
		stencilOtherSetupDSS.DepthFunc				  = rage::FixupDepthDirection(grcRSV::CMP_LESS);
		stencilOtherSetupDSS.StencilEnable            = TRUE;	
		stencilOtherSetupDSS.StencilReadMask          = DEFERRED_MATERIAL_SPAREOR1;
		stencilOtherSetupDSS.StencilWriteMask         = DEFERRED_MATERIAL_SPAREOR1|DEFERRED_MATERIAL_SPAREOR2;
		stencilOtherSetupDSS.FrontFace.StencilFunc    = grcRSV::CMP_NOTEQUAL;
		stencilOtherSetupDSS.FrontFace.StencilPassOp  = grcRSV::STENCILOP_ZERO;

		stencilOtherSetupDSS.BackFace = stencilOtherSetupDSS.FrontFace;
		m_fillShadow_DS = grcStateBlock::CreateDepthStencilState(stencilOtherSetupDSS);


		grcDepthStencilStateDesc depthStencilStateDesc;
		depthStencilStateDesc.StencilEnable                = TRUE;
		depthStencilStateDesc.DepthWriteMask               = FALSE;
		depthStencilStateDesc.DepthFunc					   = rage::FixupDepthDirection(grcRSV::CMP_LESS);		
		depthStencilStateDesc.StencilReadMask              = DEFERRED_MATERIAL_SPAREOR1;
		depthStencilStateDesc.StencilWriteMask             = DEFERRED_MATERIAL_SPAREOR1 | DEFERRED_MATERIAL_SPAREOR2;
		depthStencilStateDesc.FrontFace.StencilFunc        = grcRSV::CMP_EQUAL;
		depthStencilStateDesc.FrontFace.StencilPassOp	   = grcRSV::STENCILOP_ZERO;
		
		depthStencilStateDesc.BackFace = depthStencilStateDesc.FrontFace;

#if DEPTH_BOUNDS_SUPPORT && (RSG_DURANGO || RSG_ORBIS)
		depthStencilStateDesc.DepthBoundsEnable            = TRUE;
#endif
		m_reveal_DS = grcStateBlock::CreateDepthStencilState(depthStencilStateDesc);
#if DEPTH_BOUNDS_SUPPORT && (RSG_DURANGO || RSG_ORBIS)
		depthStencilStateDesc.DepthBoundsEnable            = FALSE;
#endif

		// set spare1 (if spare1 is set, we will draw shadows there)
		depthStencilStateDesc.DepthFunc					   = rage::FixupDepthDirection(grcRSV::CMP_LESS);
		depthStencilStateDesc.StencilWriteMask             = DEFERRED_MATERIAL_SPAREOR1;
		depthStencilStateDesc.StencilReadMask              = DEFERRED_MATERIAL_SPAREOR1;
		depthStencilStateDesc.BackFace.StencilPassOp       = grcRSV::STENCILOP_REPLACE;
		depthStencilStateDesc.BackFace.StencilFunc         = grcRSV::CMP_ALWAYS;
		depthStencilStateDesc.BackFace.StencilDepthFailOp  = grcRSV::STENCILOP_KEEP;
		depthStencilStateDesc.FrontFace					   = depthStencilStateDesc.BackFace;
		m_direct_DS = grcStateBlock::CreateDepthStencilState(depthStencilStateDesc);

		// set spare2 if we are behind the object, otherwise clear spare2, use ref = DEFERRED_MATERIAL_SPAREOR2 for this pass
		depthStencilStateDesc.DepthFunc					   = rage::FixupDepthDirection(grcRSV::CMP_GREATER);
		depthStencilStateDesc.StencilWriteMask             = DEFERRED_MATERIAL_SPAREOR2;
		depthStencilStateDesc.StencilReadMask              = DEFERRED_MATERIAL_SPAREOR2;
		depthStencilStateDesc.BackFace.StencilPassOp       = grcRSV::STENCILOP_REPLACE;
		depthStencilStateDesc.BackFace.StencilFunc         = grcRSV::CMP_ALWAYS;
		depthStencilStateDesc.BackFace.StencilDepthFailOp  = grcRSV::STENCILOP_ZERO;
		depthStencilStateDesc.FrontFace					   = depthStencilStateDesc.BackFace;
		m_behind_DS = grcStateBlock::CreateDepthStencilState(depthStencilStateDesc);

		// set spare1 if spare2 is set and we are in front of the object, use ref = DEFERRED_MATERIAL_SPAREOR1 for this pass
		depthStencilStateDesc.DepthFunc					   = rage::FixupDepthDirection(grcRSV::CMP_LESS);
		depthStencilStateDesc.StencilWriteMask             = DEFERRED_MATERIAL_SPAREOR1 | DEFERRED_MATERIAL_SPAREOR2;
		depthStencilStateDesc.StencilReadMask              = DEFERRED_MATERIAL_SPAREOR1 | DEFERRED_MATERIAL_SPAREOR2;
		depthStencilStateDesc.BackFace.StencilPassOp       = grcRSV::STENCILOP_REPLACE; 
		depthStencilStateDesc.BackFace.StencilFunc         = grcRSV::CMP_LESS;			     // if SPARE2 is set the read value will be larger than the ref value (SPARE1), and we write the ref value to set SPARE1)
		depthStencilStateDesc.BackFace.StencilDepthFailOp  = grcRSV::STENCILOP_KEEP; 
		depthStencilStateDesc.FrontFace					   = depthStencilStateDesc.BackFace;
		m_inFront_DS = grcStateBlock::CreateDepthStencilState(depthStencilStateDesc);

		// set spare1 if we are behind the object, use ref = DEFERRED_MATERIAL_SPAREOR1 for this pass
		depthStencilStateDesc.DepthFunc					   = rage::FixupDepthDirection(grcRSV::CMP_GREATER);
		depthStencilStateDesc.StencilWriteMask             = DEFERRED_MATERIAL_SPAREOR1 | DEFERRED_MATERIAL_SPAREOR2;
		depthStencilStateDesc.StencilReadMask              = DEFERRED_MATERIAL_SPAREOR1 | DEFERRED_MATERIAL_SPAREOR2;
		depthStencilStateDesc.BackFace.StencilPassOp       = grcRSV::STENCILOP_REPLACE;
		depthStencilStateDesc.BackFace.StencilFunc         = grcRSV::CMP_ALWAYS;
		depthStencilStateDesc.BackFace.StencilDepthFailOp  = grcRSV::STENCILOP_KEEP;
		depthStencilStateDesc.FrontFace					   = depthStencilStateDesc.BackFace;
		m_inside_DS = grcStateBlock::CreateDepthStencilState(depthStencilStateDesc);

		// we'll skip depth test it we crossing the portal plane, we want to make sure we set everything outside to use shadows, use ref DEFERRED_MATERIAL_SPAREOR1
		depthStencilStateDesc.DepthFunc					   = grcRSV::CMP_ALWAYS;
		m_nodepth_DS = grcStateBlock::CreateDepthStencilState(depthStencilStateDesc);

		grcRasterizerStateDesc rasterizerStateDesc;
		rasterizerStateDesc.CullMode					   = grcRSV::CULL_FRONT;
		m_cullFront_RS = grcStateBlock::CreateRasterizerState(rasterizerStateDesc);

	}
	bool InteriorOnly(Vec3V_In lightDir, const grcViewport& vp)  const 
	{	   
		if (!InInterior() || !m_usePortals)
			return false;

		if (m_CameraJustOutsideAPortal)
			return false;

		ScalarV baseExtrusionDistance(PortalExtrusionDistance);
		ScalarV portalSizeScale(PortalSizeScale);

		Vec3V ldir = lightDir/(Abs(lightDir.GetZ()) + ScalarV(0.00001f));

		BANK_ONLY( m_interiorOnly = false);
		for( int i=0; i< drawPortals.GetCount(); i++)
		{
			ScalarV maxLength = Max(Dist(drawPortals[i].p[0], drawPortals[i].p[1]), Dist(drawPortals[i].p[1], drawPortals[i].p[2]));
			ScalarV extrusionDistance = Max(maxLength*portalSizeScale, baseExtrusionDistance);

			if ( drawPortals[i].IsSunlightVisible(ldir*extrusionDistance, vp) )
				return false;
		}

		BANK_ONLY( m_interiorOnly = true);
		return true;
	}
	bool UseExtrusion() const
	{
		return m_usePortals BANK_ONLY(&& m_active && !TiledScreenCapture::IsEnabled());
	}
	
	void FindAndAddPortals(CInteriorInst* currentIntInst, atFixedArray<CInteriorInst*, 48>* traversedInteriors, Vec3V_In cameraPosition, float threshold)
	{
		if(Verifyf(!traversedInteriors->IsFull(), "Shadow Portal Interior list full!"))
			traversedInteriors->Append() = currentIntInst;
		else
			return;

		CMloModelInfo* modelInfo = currentIntInst->GetMloModelInfo();

		int numLimboPortals = currentIntInst->GetNumPortalsInRoom(INTLOC_ROOMINDEX_LIMBO);

		for (u32 i = 0; i < numLimboPortals; i++)
		{
			s32 globalPortalIdx = currentIntInst->GetPortalIdxInRoom(INTLOC_ROOMINDEX_LIMBO, i);

			CPortalFlags portalFlags;

			u32 roomIdx1, roomIdx2;
			modelInfo->GetPortalData(globalPortalIdx, roomIdx1, roomIdx2, portalFlags);

			if(portalFlags.GetIsMirrorPortal())
				continue;

			if(currentIntInst->TestPortalState(true, true, globalPortalIdx))
				continue;

			//traverse through the link portal to find the exit portal
			if(portalFlags.GetIsLinkPortal())
			{
				CPortalInst* portalInst = currentIntInst->GetMatchingPortalInst(INTLOC_ROOMINDEX_LIMBO, (u32)i);
				if(portalInst)
				{
					Assert(portalInst->IsLinkPortal());

					s32 linkedIntRoomLimboPortalIdxSigned;
					CInteriorInst* destIntInst = NULL;
					portalInst->GetDestinationData(currentIntInst, destIntInst, linkedIntRoomLimboPortalIdxSigned);

					if(destIntInst)
					{
						//make sure we're not going through the same interiors again in a loop
						s32 stackCount = traversedInteriors->GetCount();
						bool found = false;
						for(s32 i = 0; i < stackCount; i++)
						{
							if((*traversedInteriors)[i] == destIntInst)
							{
								found = true;
								break;
							}
						}

						if(found)
							continue;

						FindAndAddPortals(destIntInst, traversedInteriors, cameraPosition, threshold);
						continue;
					}
				}
			}

			fwPortalCorners portalCorners;
			currentIntInst->GetPortalInRoom(INTLOC_ROOMINDEX_LIMBO, i, portalCorners);	// Uses the room-specific portalIdx

			Vec3V pts[4];
			for (int j=0;j< 4;j++)
				pts[j] =  VECTOR3_TO_VEC3V(portalCorners.GetCorner(j));

			DrawPortalInfo pinfo=DrawPortalInfo(pts[0],pts[1],pts[2],pts[3]);
			Vector3 cpos = VEC3V_TO_VECTOR3(cameraPosition);
			Float dist;

			if (portalCorners.CalcCloseness(cpos, dist))
			{
				static float kJustOutsideThreshold=0.3f;
				m_CameraJustOutsideAPortal = m_CameraJustOutsideAPortal || (dist > 0 && dist < kJustOutsideThreshold);
				pinfo.SetIsCloseToCamera(Abs(dist) < threshold);
			}
			
			pinfo.SetIsLit(Dot(pinfo.GetNormal(), ldir).Getf() < 0 );  // sunlight is facing into the portal

			if (AssertVerify(drawPortals.GetAvailable() != 0))
			{
				drawPortals.Append()=pinfo;
			}
		}
	}

	void Reset()
	{
		drawPortals.Reset();
		m_usePortals = false;
		m_CameraJustOutsideAPortal = false;
	}

	void Create(Vec3V_In lightDir, Vec3V_In cameraPosition, float threshold)
	{
		drawPortals.Reset();

		ldir = lightDir/(Abs(lightDir.GetZ()) + ScalarV(0.00001f));
		
		if ( !g_SceneToGBufferPhaseNew || !g_SceneToGBufferPhaseNew->GetPortalVisTracker())
			return;

		m_usePortals = true;
		m_CameraJustOutsideAPortal = false;
		atFixedArray<CInteriorInst*, 48> traversedInteriors;

		CInteriorInst* interior = g_SceneToGBufferPhaseNew->GetPortalVisTracker()->GetPrimaryInteriorInst();
		if ( interior && !interior->GetProxy()->GetIsDisabled() )
			FindAndAddPortals(interior, &traversedInteriors, cameraPosition, threshold);
		else
			m_usePortals = false;

#if __BANK
		m_portalCount        = drawPortals.GetCount();
		m_interiorsTraversed = traversedInteriors.GetCount();
#endif //__BANK
	}
	
	bool InInterior() const
	{
#if __BANK
		if ( !m_active)	return false;
#endif
		fwInteriorLocation intLoc = g_SceneToGBufferPhaseNew->GetPortalVisTracker()->GetInteriorLocation();
		return intLoc.IsValid();

	}
	void Draw() const
	{
		for (int i=0; i< drawPortals.GetCount(); i++)
			drawPortals[i].Draw();
	}
#if CSM_PBOUNDS
	void ComputeShadowBounds( Mat33V worldToShadow)
	{
		shadBounds.Reset();
		for (int i=0; i< drawPortals.GetCount(); i++)
			if ( drawPortals[i].islit && shadBounds.GetMaxCount() > shadBounds.GetCount())
				shadBounds.Append() = drawPortals[i].CalcBound( worldToShadow);
	}
	void SetBounds( Vec4V* v, int amt)
	{
		for (int i=0;i < amt; i++)
			if ( i<shadBounds.GetCount())
				v[i] = shadBounds[i];
			else
				v[i] = Vec4V(V_ZERO);	
	}
	void SetBoundsPacked( Vec4V* t, int ASSERT_ONLY(amt))
	{
		Assert( amt==8);
		Vec4V v[8];
		SetBounds(v,8);
		Transpose4x4( t[0], t[1], t[2], t[3], v[0], v[1], v[2], v[3]);
		Transpose4x4( t[4], t[5], t[6], t[7], v[4], v[5], v[6], v[7]);
	}
#endif
	void ExtrudeDraw( Vec3V_In camptv,  Mat33V_In worldToShadow, float threshold) const
	{	
		PF_PUSH_TIMEBAR("Interior Portals");

		const Vec3V lightDirection = ldir;

		ScalarV baseExtrusionDistance(PortalExtrusionDistance);
		ScalarV portalSizeScale(PortalSizeScale);

		grcEffect& defaultEffect = GRCDEVICE.GetDefaultEffect();
		grcWorldIdentity();
		
		Vec3V campt = Multiply( worldToShadow, camptv);

		if ( defaultEffect.BeginDraw(m_primeDepthTechnique,true) )	{
			defaultEffect.BeginPass(0);

			for (int i=0; i< drawPortals.GetCount(); i++)
			{
				PF_PUSH_TIMEBAR("Portal");
				const DrawPortalInfo& dp = drawPortals[i];
				if (dp.IsLit())
				{
					bool camInside = dp.IsCloseToCamera() || dp.InLight( campt, worldToShadow);

					ScalarV maxLength = Max(Dist(dp.p[0], dp.p[1]), Dist(dp.p[1], dp.p[2]));
					ScalarV extrusionDistance = Max(maxLength*portalSizeScale, baseExtrusionDistance);
					Vec3V extrusionRay = lightDirection*extrusionDistance;

					//draw behind pass
					grcStateBlock::SetRasterizerState(m_cullFront_RS);

					if(camInside)
					{	
						PF_PUSH_TIMEBAR("Inside");
						grcStateBlock::SetDepthStencilState(m_inside_DS, DEFERRED_MATERIAL_SPAREOR1);
					}
					else
					{
						PF_PUSH_TIMEBAR("Behind");
						grcStateBlock::SetDepthStencilState(m_behind_DS, DEFERRED_MATERIAL_SPAREOR2);
					}

					dp.Draw(extrusionRay);

					PF_POP_TIMEBAR();

					//draw behind pass
					if(!camInside)
					{
						PF_PUSH_TIMEBAR("inFront");
						grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
						grcStateBlock::SetDepthStencilState(m_inFront_DS, DEFERRED_MATERIAL_SPAREOR1);
						dp.Draw(extrusionRay);
						PF_POP_TIMEBAR();
					}
				}

				PF_PUSH_TIMEBAR("Window");
				if (dp.IsCloseToCamera())
				{
					// need to extrude the portal quad outward if we are straddling the plane. if the portal rect may clip the near clip plane. use no depth in case something get between us and the extrusion
					grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default); 
					grcStateBlock::SetDepthStencilState(m_nodepth_DS, DEFERRED_MATERIAL_SPAREOR1);
					dp.Draw(dp.GetNormal()*ScalarV(threshold));
				}
				else
				{
					grcStateBlock::SetRasterizerState(m_cullFront_RS); 
					grcStateBlock::SetDepthStencilState(m_direct_DS, DEFERRED_MATERIAL_SPAREOR1);
					dp.Draw();
				}
				PF_POP_TIMEBAR();

				PF_POP_TIMEBAR();
			}
			defaultEffect.EndPass();
			defaultEffect.EndDraw();
		}

		PF_POP_TIMEBAR();
	}

	void FillShadow() const
	{
		PF_PUSH_TIMEBAR("Fill Shadow");
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
		grcStateBlock::SetDepthStencilState(m_fillShadow_DS, DEFERRED_MATERIAL_SPAREOR1);

		CSprite2d sprite;
		sprite.SetGeneralParams(Vector4(0.0f, 0.0f, 0.0f, 0.0f), Vector4(0.0f, 0.0f, 0.0f, 0.0f));
		sprite.BeginCustomList(CSprite2d::CS_BLIT_COLOUR, NULL);
		grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color_black);
		sprite.EndCustomList();

		grcStateBlock::SetDepthStencilState(m_reveal_DS, DEFERRED_MATERIAL_SPAREOR1 );
		PF_POP_TIMEBAR();
	}

	void RestoreDepth() const
	{
		//Lights::CleanupVolumeHiStencil();		
	}
	bool PortalsOverlap( Mat44V_In worldToView ) const 
	{
#if __BANK
		if ( !m_testPortalOverlap)
			return true;
#endif
		Vec2V negOne = Vec2V(-1.f,-1.f);
		Vec2V one(V_ONE);
		for (int i =0;i<drawPortals.GetCount();i++)
		{
			Vec3V bmin,bmax;
			drawPortals[i].CalcBound(worldToView, bmin, bmax);
			// check if with in view.

			// use BOOLV check
			VecBoolV excluded = IsGreaterThan(bmin.GetXY(), one);
			excluded = excluded | IsLessThan( bmax.GetXY(), negOne);
			BoolV ex = excluded.GetX() | excluded.GetY();

			if ( !ex.Getb()){
				BANK_ONLY(m_overlaps = true);
				return true;
			}
		}
		BANK_ONLY(m_overlaps = false);
		return false;
	}

#if __BANK
	void AddWidgets( bkBank& bk)
	{
		bk.PushGroup("Window Renderer");
			bk.AddToggle("Active", &m_active);
			bk.AddText("Extrusion", &m_usePortals);
			bk.AddToggle("Enable Cascade Testing", &m_testPortalOverlap);
			bk.AddText("Overlaps", &m_overlaps);
			bk.AddText("Portal Count", &m_portalCount);
			bk.AddText("Interiors Traversed", &m_interiorsTraversed);
			bk.AddText("Interior Only", &m_interiorOnly);
			bk.AddSlider("Base Portal Extrusion Distance", &PortalExtrusionDistance, 0.0f, 100.0f, 1.0f);
			bk.AddSlider("Portal Size Scale", &PortalSizeScale, 0.0f, 10.0f, 0.1f);
		bk.PopGroup();
	}
#endif // __BANK
};

grcDepthStencilStateHandle  ExteriorPortals::m_fillShadow_DS;

grcDepthStencilStateHandle  ExteriorPortals::m_reveal_DS;
grcDepthStencilStateHandle  ExteriorPortals::m_direct_DS;
grcDepthStencilStateHandle  ExteriorPortals::m_behind_DS;
grcDepthStencilStateHandle  ExteriorPortals::m_inFront_DS;
grcDepthStencilStateHandle  ExteriorPortals::m_inside_DS;
grcDepthStencilStateHandle  ExteriorPortals::m_nodepth_DS;

grcRasterizerStateHandle    ExteriorPortals::m_cullFront_RS;

bank_float	ExteriorPortals::PortalExtrusionDistance = 5.0f;
bank_float	ExteriorPortals::PortalSizeScale = 2.0f;

#if __BANK
bool		ExteriorPortals::m_testPortalOverlap;
bool		ExteriorPortals::m_active;
int			ExteriorPortals::m_portalCount;
int         ExteriorPortals::m_interiorsTraversed;
bool		ExteriorPortals::m_interiorOnly;
#endif

#endif // CSM_PORTALED_SHADOWS

// ================================================================================================
#if __BANK
class CutSceneDepthGenerator : public datBase
{

	atArray<CutNearShadowDepth>	m_depths;
	bool				m_generate;
	bool				m_apply;

	char m_currentCutScene[512];
	char m_currentCutName[512];
	float m_currentDepth;
	int cutTime;

public:
	void Reset(){
		m_depths.Reset();   
		cutTime =-1;
	}
	float Update(float dynamicDepth)
	{

		const bool bCutsceneRunning = (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning());
		if (!bCutsceneRunning )
			return -1.f;

		m_currentDepth = -1;
		const char* cutsceneName = CutSceneManager::GetInstance()->GetCutsceneName();
		safecpy( m_currentCutScene,cutsceneName );	   
		
		const CCutSceneCameraEntity* camEntity = CutSceneManager::GetInstance()->GetCamEntity();
		const char* pCutName = camEntity->GetCameraCutEventName();								
		
		safecpy( m_currentCutName,pCutName );

		bool hasCut=CutSceneManager::GetInstance()->GetHasCutThisFrame();
		if ( hasCut)
			cutTime =3;

		if (m_generate)
		{
			if ( cutTime >=0)
				cutTime--;
				
			if ( cutTime==0 || m_depths.GetCount()==0 )
			{		
				m_depths.Grow() = CutNearShadowDepth( pCutName);
				cutTime=-1;
			}
			if ( cutTime ==-1){
				m_depths.back().Set( dynamicDepth);
				m_currentDepth = m_depths.back().m_minNearDepth;
			}
		}
		if ( m_apply)
		{
			for(int i=0; i<m_depths.GetCount(); i++){
				if ( strcmp(m_depths[i].m_name, pCutName)==0)
				{
					m_currentDepth =  m_depths[i].m_minNearDepth;
					return m_depths[i].m_minNearDepth;
				}
			}
			Errorf(" Could not find cut %s depth in Cutscene %s", pCutName, cutsceneName);
		}
		return -1.f;
	}
	void Dump(){
		Displayf("Cutscene Near Depth Values");
		Displayf("_____________________________");

		for (int i=0;i<m_depths.GetCount();i++)
			m_depths[i].Dump();

		Displayf("_____________________________");
	}
	void Record(){
		Reset();
		m_generate=true;
		m_apply=false;
	}
	void Stop(){
		m_apply=false;
		m_generate=false;
	}
	void Apply(){
		m_apply=true;
		m_generate=false;
	}
	void WriteOutToCutFile(){
		CCutSceneDebugManager::WriteOutAllDynamicDepthEvents( m_depths );
	}
	void ClearFromCutFile(){
		CCutSceneDebugManager::ClearAllDynamicDepthEvents();
	}
	void AddWidgets( bkBank& bk){
		bk.PushGroup("CutScene Dynamic Depth Recorder");

		bk.AddButton("Record",datCallback(MFA(CutSceneDepthGenerator::Record),this) );
		bk.AddButton("Stop",datCallback(MFA(CutSceneDepthGenerator::Stop),this) );
		bk.AddButton("Apply",datCallback(MFA(CutSceneDepthGenerator::Apply),this) );
		bk.AddButton("Dump",datCallback(MFA(CutSceneDepthGenerator::Dump),this) );
		bk.AddButton("Write to cutfile", datCallback(MFA(CutSceneDepthGenerator::WriteOutToCutFile),this) );
		bk.AddButton("Clear Cutfile", datCallback(MFA(CutSceneDepthGenerator::ClearFromCutFile),this) );
		bk.AddText("Current Cut Name", m_currentCutName, NELEM(m_currentCutName), true);
		bk.AddText("Current Cutscene Name", m_currentCutScene,  NELEM(m_currentCutScene), true);
		bk.AddText("Current Depth", &m_currentDepth );

		bk.AddText("Recording", &m_generate);
		bk.AddText("Applying", &m_apply );

		bk.PopGroup();
	}

};
#endif

class CCSMEntityTracker : public datBase
{
	struct SimpleCurve
	{
		float minv;
		float maxv;
		float applyMin;
		float applyMax;

		SimpleCurve() {}
		SimpleCurve(float minv_, float maxv_, float applyMin_, float applyMax_) : minv(minv_), maxv(maxv_), applyMin(applyMin_), applyMax(applyMax_) {}

		static __forceinline float SmoothStep(float x, float a, float b)
		{
			x = (Clamp<float>(x, a, b) - a)/(b - a);
			return x*x*(3.0f - 2.0f*x);
		}

		float Get(float v) const
		{
			return SmoothStep(v, applyMin, applyMax)*(maxv - minv) + minv;
		}

		void SetMinVMaxV(float minv_, float maxv_)
		{
			minv = minv_;
			maxv = maxv_;
		}

		void SetApplyMinMax(float applyMin_, float applyMax_)
		{
			applyMin = applyMin_;
			applyMax = applyMax_;
		}

#if __BANK
		void AddWidgets(bkBank& bank, const char* name, float maxRange)
		{
			bank.PushGroup(name, false);
			{
				bank.AddSlider("Start Value", &minv, 0.0f, maxRange, 0.01f);
				bank.AddSlider("End Value"  , &maxv, 0.0f, maxRange, 0.01f);

				bank.AddSlider("Apply Start Value", &applyMin, 0.0f, maxRange, 0.01f);
				bank.AddSlider("Apply End Value"  , &applyMax, 0.0f, maxRange, 0.01f);
			}
			bank.PopGroup();
		}
#endif // __BANK
	};

#define NEW_TRACKER_SETTINGS 0
public:
	void Init()
	{
		m_active                    = true;
		m_activeSplits              = true;
		m_drawFocus                 = false;
//On PC this value comes from the settings so will already be setup so dont overwrite them here.
#if !ALLOW_SHADOW_DISTANCE_SETTING
		//If this is changed please email someone on the PC team so we can adjust the settings values.
		m_airCraftExpWeight         = 0.99f;
#endif
		m_maxRadius                 = 25.0f;

#if __BANK
		m_lookAtRangeTightenEnabled = true;
		m_lookAtRangePushEnabled    = true; 
#endif // __BANK

		m_lookAtRangeTightenFactorPed = 1.85f; // TODO -- can we get rid of this? or just tighten around the player's face somehow?

#if CSM_DYNAMIC_DEPTH_RANGE
		m_forceUseDynamicDepthRange       = false;
		m_useDynamicDepthRangeInCutscenes = true;
		m_dynamicRange              = 0.01f;
		m_dynamicQuant              = 0.005f;
		m_oldcs                     = -1.0f;		

#if __BANK
		m_disableDynamicDepthRange  = false;
		m_useMinDepth		        = false;
#endif //__BANK
#endif // CSM_DYNAMIC_DEPTH_RANGE

#if __BANK
		m_inputRadius                 = 0.0f;
		m_inputFOV                    = 0.0f;
		m_finalRadius                 = 0.0f;
		m_finalZExpWeight             = 0.0f;
		m_finalCascadeExtend          = 0.0f;
		m_finalCascadeStart           = 0.0f;
		m_hasFocus                    = false;
		m_useDepthResizingInCutscenes = true;
		m_useCinematicCam			  = false;
		m_useOccludersForAircraft     = true;
		m_graphValues                 = false;
		m_forcePlayerTarget           = false;
#endif // __BANK
		m_forceAircraftShadows      = false;
		m_prevCascadeScale          = 1.0f;
		m_isAircraft                = false;

		m_overrideMinimumDepthValue = -1.f;
		m_extendRadius              = SimpleCurve(3.00f, 1.08f,  1.2f,  5.0f);
		m_pushAmount				= 1.8f;
		m_pushAmountVehicleInterior = 1.1f;
		m_extendCascade             = SimpleCurve(5.00f, 6.00f,  5.0f,  8.0f);
#if NEW_TRACKER_SETTINGS
		m_extendCascadeCar          = SimpleCurve(0.65f, 2.50f,  1.2f,  4.0f);
#else
		m_extendCascadeCar          = SimpleCurve(1.00f, 2.50f,  1.2f,  4.0f);
#endif

		const float applyMin = 1.2f;
		const float applyMax = 6.0f;
//On PC splitZexpmin and max come from the settings so will already be setup so dont overwrite them here.
#if ALLOW_SHADOW_DISTANCE_SETTING
		m_splitZExpWeight.SetApplyMinMax(applyMin, applyMax);
#else
		//If this is changed please email someone on the PC team so we can adjust the settings values.
#if NEW_TRACKER_SETTINGS
		m_splitZExpWeight           = SimpleCurve(0.90f, 0.89f,  applyMin,  applyMax);
#else
		m_splitZExpWeight           = SimpleCurve(0.96f, 0.89f,  applyMin,  applyMax);
#endif
#endif
		m_prevSniperActive			= 3;
		m_prevSniperActiveCS		= -1.f;
		m_focalObjectBlend			= 1.f;
	}

	bool IsInSniperTypeCamera(){
		const camBaseCamera* renderedCamera = camInterface::GetDominantRenderedCamera();
		return CNewHud::IsSniperSightActive() || (renderedCamera && renderedCamera->GetIsClassId(camFirstPersonPedAimCamera::GetStaticClassId()));
	}
	bool GetFocalObject(Vec4V_InOut sphere, float& sphereRadiusScale, float scriptScale) //RELEASE_CONST (TODO -- can't be const because of m_isAircraft)
	{
		if (!m_active)
			return false;
		
		const camBaseCamera* activeCamera = camInterface::GetDominantRenderedCamera();
		if(!activeCamera)
			return false;

#if GTA_REPLAY
		//we don't want to use the entity tracker in free cam *except* when we have aircraft shadows on then we do
		//otherwise the shadows flicker between when switching between free cam and the game cam in the air
		if(		CReplayMgr::IsEditModeActive() 
			&&	(activeCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()) && !((camReplayFreeCamera*)activeCamera)->HasFollowOrLookAtTarget())
			&&	CReplayMgr::ShouldUseAircraftShadows() == false)
		{
			return false;
		}
#endif
		
		m_AircraftBlend = -1.f;

		const CEntity* target = NULL;
		bool canUseCarTransitionBlending=false;
		if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsCutscenePlayingBack())
		{
			target = CutSceneManager::GetInstance()->GetCascadeShadowFocusEntity();
		}

		if(activeCamera->GetClassId() == camCinematicHeliChaseCamera::GetStaticClassId() && m_DisableInHeliChaseCam)
		{
			return false;
		}

		if (!target && (camInterface::GetGameplayDirectorSafe()) && camInterface::GetGameplayDirector().GetFirstPersonAimCamera() == NULL && activeCamera)
		{
			if(activeCamera->GetClassId() == camCinematicFirstPersonIdleCamera::GetStaticClassId())
				target =  activeCamera->GetLookAtTarget();				
			else
			{
				target = activeCamera->GetAttachParent(); // should return the entity the camera is attached to, if any
				canUseCarTransitionBlending = target != NULL;
			}

			if (!target)
			{
				target = activeCamera->GetLookAtTarget();

				if (!target)
				{
					activeCamera = camInterface::GetGameplayDirector().GetRenderedCamera();

					if (activeCamera)
					{
						target = activeCamera->GetAttachParent();

						if (!target)
						{
							target = camInterface::FindFollowPed();
							Assertf(target, "CCSMEntityTracker::GetFocalObject failed to find target");
						}
					}
				}
			}
		}
		
		m_IsInInterior = false;

		//Determine aircraft mode
		const CPed* ped = NULL;
		bool		aircraftMode = m_forceAircraftShadows;
		bool		smoothBlend = false;
		float		velocitySqr = 0.0f;

		if (target)
		{
			m_IsInInterior = target->GetInteriorLocation().IsValid();

			if (target->GetIsTypePed())
			{
				ped = static_cast<const CPed*>(target);

				smoothBlend = true;
				velocitySqr = ped->GetVelocity().Mag2();
			}
			else if(target->GetIsTypeVehicle())
			{
				const CVehicle* vehicle =  static_cast<const CVehicle*>(target);

				aircraftMode = aircraftMode || vehicle->GetIsAircraft();
				smoothBlend = true;
				velocitySqr = vehicle->GetVelocity().Mag2();
			}
			else if(target->GetIsTypeObject())
			{ 
				const CObject* cobj = static_cast<const CObject*>(target);
				aircraftMode = aircraftMode || CTaskParachute::IsParachute(*cobj);				
			}
		}
#if FPS_MODE_SUPPORTED
		else if(camInterface::IsRenderingFirstPersonShooterCamera() && activeCamera->GetAttachParent() && activeCamera->GetAttachParent()->GetIsTypePed())
		{
			ped = static_cast<const CPed*>(activeCamera->GetAttachParent());
		}
#endif
		
		if (ped)
		{
			aircraftMode = aircraftMode || ped->GetIsParachuting();
			if (!aircraftMode && ped->GetPedResetFlag( CPED_RESET_FLAG_IsFalling ))
			{
				const CTask* pTask = ped->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_FALL);
				if(pTask)
				{
					const CTaskFall* pFallTask = static_cast<const CTaskFall*>(pTask);
					static dev_float FallDistanceThreshold=10.f;
					m_AircraftBlend = Clamp( (pFallTask->GetMaxFallDistance()-10.f)/FallDistanceThreshold,0.f,1.f);
					aircraftMode = m_AircraftBlend > 0.f;
					smoothBlend = false;
				}
			}
		}

		REPLAY_ONLY(aircraftMode = aircraftMode || CReplayMgr::ShouldUseAircraftShadows();)

		float rangeBlend = 1.0f;

		if (!Verifyf(FPIsFinite(velocitySqr), "GetFocalObject - velocitySqr of entity is not finite, so setting it to 0.0"))
		{
			velocitySqr = 0.0f;
		}

		if(smoothBlend && aircraftMode && !m_forceAircraftShadows)
		{
			if(aircraftMode && !m_isAircraft)
			{
				rangeBlend = 0.0f;
			}
			else
			{
				rangeBlend = m_ShadowRangeBlend + Min(0.05f, velocitySqr * 0.001f);
				rangeBlend = Saturate(rangeBlend);
			}
		}

		m_ShadowRangeBlend = rangeBlend;

		if(m_isAircraft != aircraftMode)
		{
			g_RasterStateUpdateId++;
		}

		m_isAircraft = aircraftMode;

		//handling for first person
		if(
#if FPS_MODE_SUPPORTED
			camInterface::IsRenderingFirstPersonShooterCamera() ||
#endif //FPS_MODE_SUPPORTED
			camInterface::IsRenderedCameraInsideVehicle()
		)
		{
			float scale = 1.0f;

			if (camInterface::IsRenderedCameraInsideVehicle())
			{
				scale = 0.3f;
				const CPed* player = camInterface::FindFollowPed();
				if (player && player->GetAttachCarSeatIndex() > 1)
				{
					scale = 0.6f;
				}
			}
			
			Vector4 pos = Vector4(camInterface::GetPos());
			sphere = RC_VEC4V(pos);
			sphere.SetW(ScalarV(scale));
			sphereRadiusScale = 1.0f;
			return true;
		}

		if(!target)
			return false;

		float offsetHeight = 0.3f;
		// use near plane and apply to objects in game as well.
		if(!aircraftMode BANK_ONLY(&& m_lookAtRangeTightenEnabled) && activeCamera)
		{
			const Vec3V camPosition = VECTOR3_TO_VEC3V(activeCamera->GetFrame().GetPosition());
			const Vec3V camDirection = VECTOR3_TO_VEC3V(activeCamera->GetFrame().GetFront());
			const Vec4V targetSphere = target->GetBoundSphere().GetV4();

			const ScalarV distanceToTarget = Dot(targetSphere.GetXYZ() - camPosition, camDirection);
			float tighten = target->GetIsTypePed() ? m_lookAtRangeTightenFactorPed : 1.0f;

			const ScalarV finalRadius = ScalarV(tighten*m_extendRadius.Get(targetSphere.GetWf()))*targetSphere.GetW();

			const float minRad = 0.4f;
			sphereRadiusScale = Clamp(distanceToTarget*Invert(finalRadius), ScalarV(minRad), ScalarV(V_ONE)).Getf();

			sphereRadiusScale *= scriptScale;

			if ( target->GetIsTypePed()){
				static dev_float hScale = .5f;
				offsetHeight += (1.f - sphereRadiusScale)*hScale;
			}
		}

		sphere = target->GetBoundSphere().GetV4();

		// find out how much the targeted ped/Player (either the player or the spectated ped) is inside the vehicle
		const CPed* player = camInterface::FindFollowPed();
		if ( canUseCarTransitionBlending && ( target == player|| target->GetIsTypeVehicle() ) )
		{
			const CEntity* vehicle; 				
			ScalarV incaredness = ScalarV(player->GetCarInsideAmount(vehicle));
			bool targetIsCar =  vehicle == target;

			if (target->GetIsTypeVehicle() && !vehicle ) {
				if ( static_cast<const CVehicle*>(target)->GetVehicleType() == VEHICLE_TYPE_CAR ){
					vehicle=target;
					targetIsCar=true;					
				}
			}
			if ( vehicle){
				if ( targetIsCar){
					Vec4V playerSphere = player->GetBoundSphere().GetV4();
					sphere = Lerp( incaredness, playerSphere,sphere);						
				}
				else {
					Vec4V carSphere = vehicle->GetBoundSphere().GetV4();
					sphere = Lerp( incaredness, sphere,carSphere);
				}
			}
		}
			// offset to centre on chest (BS#780211)
		sphere.SetZf(sphere.GetZf() + 0.3f);

		return true;
	}

	bool GetFocalObjectCascadeSplits(
#if __BANK
		float fov,
#else
		float UNUSED_PARAM(fov),
#endif
		float& cascadeScale, float& expWeight, float scriptScale) //RELEASE_CONST (TODO -- can't be const because of m_isAircraft)
	{
		if (!m_active || !m_activeSplits)
		{
			return false;
		}

		BANK_ONLY(m_hasFocus = false);

		Vec4V sphere;
		float sphereRadiusScale = 1.0f;

		if (!GetFocalObject(sphere, sphereRadiusScale, scriptScale))
		{
			return false;
		}
		float iexpW = expWeight;
		if (CSM_DEFAULT_SPLIT_Z_EXP_WEIGHT > 0.0f)
			iexpW *= (1.f / CSM_DEFAULT_SPLIT_Z_EXP_WEIGHT);

		float inputRadius = sphere.GetWf();

		if (m_forceAircraftShadows)
		{
			inputRadius = Max<float>(inputRadius, 2.0f); // set to small plane size
		}

		expWeight = m_splitZExpWeight.Get(inputRadius)*iexpW;
		cascadeScale = m_extendCascadeCar.Get(inputRadius);

		const bool bCameraCut = ((camInterface::GetFrame().GetFlags() & (camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation)) != 0);

		if (m_isAircraft)
		{
			float	targetCascadeScale = m_extendCascade.Get(inputRadius);

			if (bCameraCut)
			{
				cascadeScale = targetCascadeScale;
			}
			else
			{
				cascadeScale = Lerp(m_ShadowRangeBlend, cascadeScale, targetCascadeScale);
			}
			if ( m_AircraftBlend>-1.f)
				cascadeScale = cascadeScale*m_AircraftBlend + m_extendCascadeCar.Get(inputRadius)*(1.f-m_AircraftBlend);

			expWeight = m_airCraftExpWeight;
		}
		else
		{
			if(!bCameraCut)
			{
				cascadeScale = m_prevCascadeScale * 0.8f + cascadeScale * 0.2f;
			}
		}

		bool isCinematicCam = !m_isAircraft && camInterface::GetDominantRenderedDirector() && camInterface::GetDominantRenderedDirector()->GetIsClassId(camCinematicDirector::GetStaticClassId());
					
		if ( isCinematicCam){
			bool isBonnetCam = false;
			const camBaseCamera* activeCamera = camInterface::GetDominantRenderedCamera();
			float cameraDirZ = fabs(activeCamera->GetFrame().GetFront().z);

			if(activeCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
			{
				camCinematicMountedCamera* camera = (camCinematicMountedCamera*)activeCamera;

				isBonnetCam = !camera->IsFirstPersonCamera();
			}
			
			if ( cameraDirZ > 0.35f && !isBonnetCam)
			{
				float cinematicScale = Clamp((cameraDirZ-0.35f)*4.f, 0.f,1.f);
				cascadeScale *=1.f-cinematicScale*.7f;
				expWeight *=1.f-cinematicScale*0.45f;
			}
			else
				isCinematicCam = false;
		}
		

		m_prevCascadeScale = cascadeScale;

		BANK_ONLY(m_inputFOV           = fov);
		BANK_ONLY(m_useCinematicCam     = isCinematicCam);
		BANK_ONLY(m_finalZExpWeight    = expWeight);
		BANK_ONLY(m_finalCascadeExtend = cascadeScale);
		BANK_ONLY(m_hasFocus           = true);

		return true;
	}
	ScalarV_Out GetFocalObjectBlend() const { return ScalarV(m_focalObjectBlend); }

	bool GetFocalSphere(Vec4V_InOut resultSphere, float scriptScale) //RELEASE_CONST (TODO -- can't be const because of m_isAircraft)
	{
		Vec4V sphere;
		float sphereRadiusScale = 1.0f;

		if (!GetFocalObject(sphere, sphereRadiusScale, scriptScale))
		{
			return false;
		}

		float inputRadius = sphere.GetWf();
		float finalRadius = inputRadius;

		finalRadius = Min<float>(m_extendRadius.Get(inputRadius)*inputRadius*sphereRadiusScale, m_maxRadius);
		resultSphere = Vec4V(sphere.GetXYZ(), ScalarV(finalRadius));

		BANK_ONLY(if(m_lookAtRangePushEnabled))
		{
			float pushAmount = camInterface::IsRenderedCameraInsideVehicle()? m_pushAmountVehicleInterior : m_pushAmount;
			const grcViewport& uvp = *gVpMan.GetUpdateGameGrcViewport();
			Vec3V camDir = uvp.GetCameraMtx().c();
			Vec2V offsetPos = resultSphere.GetXY() + ScalarV(-pushAmount*sphereRadiusScale)* camDir.GetXY();
			resultSphere.SetX( offsetPos.GetX() );
			resultSphere.SetY( offsetPos.GetY() );
		}

		BANK_ONLY(m_inputRadius = inputRadius);
		BANK_ONLY(m_finalRadius = finalRadius);

		return true;
	}

#if CSM_DYNAMIC_DEPTH_RANGE
	void SetDyamicDepthShadowMode(bool on)
	{
		m_forceUseDynamicDepthRange = on;
	}

	void EnableDynamicDepthModeInCutscenes( bool on)
	{
		m_useDynamicDepthRangeInCutscenes =  on;
	}

	void SetDynamicDepthValue( float value)
	{
		m_overrideMinimumDepthValue = value;
	}

	bool UsingDynamicDepthMode()
	{
#if __BANK
		if(m_disableDynamicDepthRange)
			return false;
#endif //__BANK
		
		bool useDynamicDepthInCutscenes = IsCutscene() && m_useDynamicDepthRangeInCutscenes;
		bool isSniperCam = IsInSniperTypeCamera();
		//bool isSniperCam = false;
		return (useDynamicDepthInCutscenes || isSniperCam || m_forceUseDynamicDepthRange) && CTiledLighting::IsEnabled();
	}

	void UpdateDynamicDepthRange(float shadowFarClip, float FOV)
	{
		if (!IsCutscene())
			m_useDynamicDepthRangeInCutscenes = true;

		bool inSniperCam = IsInSniperTypeCamera();
		m_prevSniperActive = inSniperCam ? m_prevSniperActive - 1 : 6;

		if(!UsingDynamicDepthMode())
		{
			m_oldcs = -1.0f;
			m_overrideMinimumDepthValue = -1.f;
			return;
		}

		float cs;
		if (m_prevSniperActive > 2 && inSniperCam && m_prevSniperActiveCS > -1.0f)
			cs = m_prevSniperActiveCS;			
		else
		{
			float minDepth, avgDepth, centerDepth;
			CTiledLighting::CalculateNearZ(shadowFarClip, minDepth, avgDepth, centerDepth);

			if(inSniperCam)
				cs =   Lerp(Clamp(FOV/20.0f - 1.0f, 0.0f, 1.0f), centerDepth > avgDepth * 1.2f ? avgDepth : centerDepth, avgDepth);
			else
				cs = minDepth;

#if __BANK
			if(m_useMinDepth)
				cs = minDepth;
#endif

			cs = (cs != FLT_MAX) ? cs : -1.0f;
		}

		if (inSniperCam)
			m_prevSniperActiveCS = cs;

#if __BANK
		float rcs = m_generator.Update(cs);
		if ( rcs != -1.f)
			cs = rcs;
		else if (m_overrideMinimumDepthValue != -1.0f)
			cs = m_overrideMinimumDepthValue;
#else
		if (m_overrideMinimumDepthValue != -1.0f)
			cs = m_overrideMinimumDepthValue;		
#endif //__BANK

		if (cs != -1.0f)
		{
			if (inSniperCam && m_oldcs > -1.0f && m_prevSniperActive <=0 )
			{
				static dev_float blend=0.7f;
				if (m_oldcs == -1.0f)
					m_oldcs = cs;
				cs = m_oldcs*blend + cs*(1.f-blend);
			}

			m_oldcs = cs;

			cs = floorf(cs/m_dynamicQuant)*m_dynamicQuant;
			m_z0 = cs + m_dynamicRange*0.5f;
		}
		else
			m_z0 = - 1.0f;

		BANK_ONLY(m_finalCascadeStart = cs);
	}

	bool GetDynamicDepthRange(ScalarV_InOut z0, ScalarV_InOut z1)
	{
		if(!UsingDynamicDepthMode() || m_z0 == -1.0f)
			return false;

		z0.Set(m_z0);
		z1.Set(m_z0+ + 0.001f);
		return true;
	}
#endif // CSM_DYNAMIC_DEPTH_RANGE

	eCSMQualityMode GetAutoQualityMode() const
	{
#if __BANK
		if (m_forcePlayerTarget)
		{
			return CSM_QM_CUTSCENE_DEFAULT;
		}
#endif // __BANK

		bool useCutsceneMode = true;
#if CSM_DYNAMIC_DEPTH_RANGE
		useCutsceneMode = m_useDynamicDepthRangeInCutscenes;
#endif
		if (DeferredLighting::GetLighingQuality() == DeferredLighting::LQ_CUTSCENE && useCutsceneMode)
		{
			return CSM_QM_CUTSCENE_DEFAULT;
		}
		else if (IsNightTime())
		{
			return CSM_QM_MOONLIGHT;
		}

		return CSM_QM_IN_GAME;
	}

	bool IsNightTime() const
	{
		const int iEveningStart = 20;
		const int iMorningStart = 6;

		const int iHours = CClock::GetHour();

		return (iHours > iEveningStart || iHours < iMorningStart);
	}

	void SetUseAircraftShadows(bool on)
	{
		m_forceAircraftShadows = on;
	}

	void SetActive(bool bActive)
	{
		m_active = bActive;
	}
	bool FirstCascadeIsInInterior(){ return m_IsInInterior; }

	bool UseOccluders() const
	{
		return m_isAircraft BANK_ONLY(&& IsUseOccludersForAircraftEnabled());
	}

	bool IsCutscene() const
	{
		return camInterface::GetCutsceneDirector().IsCutScenePlaying();
	}
	bool UseDepthResizing() const
	{
		return IsCutscene()  BANK_ONLY(&& m_useDepthResizingInCutscenes );
	}

	bool InAircraftMode() const
	{
		return m_isAircraft;
	} 

#if ALLOW_SHADOW_DISTANCE_SETTING
	void SetDistanceMultiplierSettings(float splitZStart, float splitZEnd, float aircraftExpWeight)
	{
		m_splitZExpWeight.SetMinVMaxV(splitZStart, splitZEnd);
		m_airCraftExpWeight = aircraftExpWeight;
	}
#endif

#if __BANK
	bool IsUseOccludersForAircraftEnabled() const
	{
		return m_useOccludersForAircraft;
	}

	void DebugDraw(float scriptScale) RELEASE_CONST
	{
		Vec4V sphere;

		if (m_drawFocus && GetFocalSphere(sphere, scriptScale))
		{
			const Color32 blue(0.0f, 0.0f, 1.0f, 0.4f);
			grcDebugDraw::Sphere(sphere.GetXYZ(), m_inputRadius, blue, true, 1, 16);

			const Color32 pink(1.0f, 0.0f, 1.0f, 0.4f);
			grcDebugDraw::Sphere(sphere.GetXYZ(), sphere.GetWf(), pink, true, 1, 16);
		}

		if (m_graphValues)
		{
			const int numSteps  = 128;
			const int numGraphs = 4;

			SimpleCurve vals[numGraphs];
			Color32     cols[numGraphs];
			Vec2V       cpos[numGraphs];

			cols[0] = Color_pink;
			cols[1] = Color_blue;
			cols[2] = Color_red;
			cols[3] = Color_violet;

			vals[0] = m_extendRadius;
			vals[1] = m_extendCascade;
			vals[2] = m_splitZExpWeight;
			vals[3] = m_extendCascadeCar;
			
			const float start     = 0.1f;
			const float end       = 0.9f;
			const float maxRangeX = 10.0f;
			const float maxRangeY = 2.5f;
			const float invRangeY = 1.0f/maxRangeY;

			grcDebugDraw::RectAxisAligned(Vec2V(start, start), Vec2V(end, end), Color32(0.0f, 0.2f, 0.0f, 0.2f));

			const Vec2V loc(start, start);
			const Vec2V size(end - start, (end - start)*invRangeY);

			for (int i = 0; i < numSteps; i++)
			{
				for (int j = 0;j < numGraphs; j++)
				{
					const float x = (float)i/(float)numSteps;
					const float y = maxRangeY - vals[j].Get(x*maxRangeX);

					const Vec2V npos(x, y);

					if (i != 0)
					{
						grcDebugDraw::Line(AddScaled(loc, cpos[j], size), AddScaled(loc, npos, size), cols[j]);
					}

					cpos[j] = npos;
				}
			}

			const float x = m_inputRadius/maxRangeX;
			const float y = maxRangeY;

			grcDebugDraw::Line(
				AddScaled(loc, Vec2V(x, 0.0f), size),
				AddScaled(loc, Vec2V(x, y), size),
				Color_black
			);

			const float x2 = m_inputFOV/90.0f;

			grcDebugDraw::Line(
				AddScaled(loc, Vec2V(x2, 0.0f), size),
				AddScaled(loc, Vec2V(x2, y), size),
				Color_yellow
			);
		}
	}

	bool IsDebugDrawEnabled() const
	{
		return m_drawFocus || m_graphValues;
	}

	void ToggleNewTrackerSettings()
	{
#if !RSG_PC
		const float applyMin = 1.2f;
		const float applyMax = 6.0f;
#endif

		static bool toggle = true;
		if(toggle)
		{
			toggle = false;
			m_extendCascadeCar	= SimpleCurve(0.65f, 2.50f,  1.2f,  4.0f);
#if !RSG_PC
			m_splitZExpWeight	= SimpleCurve(0.90f, 0.89f,  applyMin,  applyMax);
#endif //!RSG_PC
		}
		else
		{
			toggle = true;
			m_extendCascadeCar	= SimpleCurve(1.00f, 2.50f,  1.2f,  4.0f);
#if !RSG_PC
			m_splitZExpWeight	= SimpleCurve(0.96f, 0.89f,  applyMin,  applyMax);
#endif //!RSG_PC
		}
	}

	void AddWidgets(bkBank& bank)
	{
		bank.PushGroup("Entity Tracker", false);
		{
			bank.AddToggle("Active"                              , &m_active);
			bank.AddToggle("Active Splits"                       , &m_activeSplits);
			bank.AddToggle("Disable in Heli Chase Cinematic Cam" , &m_DisableInHeliChaseCam);
			bank.AddToggle("Draw Focus"                          , &m_drawFocus);
			bank.AddToggle("Force Aircraft Shadows"              , &m_forceAircraftShadows);
			bank.AddSlider("Push Amount"						 , &m_pushAmount, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("Push Amount Vehicle Interior"        , &m_pushAmountVehicleInterior, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("Aircraft Exp Weight"                 , &m_airCraftExpWeight, 0.0f, 1.0f, 0.001f);
			bank.AddSlider("Max Radius"                          , &m_maxRadius, 0.0f, 100.0f, 1.0f/32.0f);
			bank.AddToggle("Look At Range Tighten Enabled"       , &m_lookAtRangeTightenEnabled);
			bank.AddSlider("Look At Range Tighten Ped"           , &m_lookAtRangeTightenFactorPed, 0.1f, 10.0f, 0.001f);
			bank.AddToggle("Look At Range Push Enabled"          , &m_lookAtRangePushEnabled);
			bank.AddSlider("Focal Entity blend"					 , &m_focalObjectBlend,0.f, 1.f, 0.001f);
#if CSM_DYNAMIC_DEPTH_RANGE
			bank.AddToggle("Use Dynamic Depth Range in Cutscenes", &m_useDynamicDepthRangeInCutscenes);
			bank.AddToggle("Disable Dynamic Depth Range"         , &m_disableDynamicDepthRange);
			bank.AddToggle("Force Use Dynamic Depth Range"       , &m_forceUseDynamicDepthRange);
			bank.AddToggle("Use Min Depth"						 , &m_useMinDepth);
			bank.AddText  ("Previous Sniper Min Depth"			 , &m_prevSniperActiveCS);
			bank.AddText  ( "Sniper Camera is active"			 , &m_prevSniperActive);
			bank.AddSlider("Dynamic First Cascade Range"         , &m_dynamicRange, 0.00001f, 2.0f, 0.001f);
			bank.AddSlider("Dynamic First Cascade Quant"         , &m_dynamicQuant, 0.00001f, 2.0f, 0.0001f);
#endif // CSM_DYNAMIC_DEPTH_RANGE
			bank.AddText  ("Input Radius"                        , &m_inputRadius);
			bank.AddText  ("Input FOV"                           , &m_inputFOV);
			bank.AddText  ("Final Radius"                        , &m_finalRadius);
			bank.AddText  ("Final Cascade Scale"                 , &m_finalCascadeExtend);
			bank.AddText  ("Final Cascade Start"                 , &m_finalCascadeStart);
			bank.AddText  ("Final Final Z Exp Weight"            , &m_finalZExpWeight);
			bank.AddText  ("Has Focus"                           , &m_hasFocus);
			bank.AddText  ("Is Aircraft"                         , &m_isAircraft);
			bank.AddText  ("Aircraft blend"   					 , &m_AircraftBlend);
			bank.AddText  ("Is In Interior"						 , &m_IsInInterior);
			bank.AddText ( "Override Minimum Depth"				 , &m_overrideMinimumDepthValue);
			bank.AddText ( "In Cinematric Cam"					 , &m_useCinematicCam );
			bank.AddToggle("Use Occluders For Aircraft"          , &m_useOccludersForAircraft);
			bank.AddToggle("Graph Curves"                        , &m_graphValues);
			bank.AddToggle("Force Player Target"                 , &m_forcePlayerTarget);
			bank.AddToggle("use Depth Resizing In Cutscenes"	 , &m_useDepthResizingInCutscenes);
			
			m_extendRadius    .AddWidgets(bank, "Extend Radius (white)"      , 10.0f);
			bank.AddButton("Toggle New Tracker Settings", datCallback(MFA(CCSMEntityTracker::ToggleNewTrackerSettings), this));
			m_extendCascadeCar.AddWidgets(bank, "Extend Cascade Car (violet)", 30.0f);
			m_extendCascade   .AddWidgets(bank, "Extend Cascade (blue)"      , 30.0f);
			m_splitZExpWeight .AddWidgets(bank, "Split Z Exp Weight (red)"   , 10.0f);
		
		}
		bank.PopGroup();
#if CSM_DYNAMIC_DEPTH_RANGE
		m_generator.AddWidgets( bank);
#endif
	}
#endif // __BANK

private:
	bool        m_active;
	bool        m_activeSplits;
	bool        m_drawFocus;
	float       m_airCraftExpWeight;
	float       m_maxRadius;
#if __BANK
	bool        m_lookAtRangeTightenEnabled; // TODO -- consider removing this feature
	bool		m_lookAtRangePushEnabled;
#endif // __BANK
	float       m_lookAtRangeTightenFactorPed;

	float		m_overrideMinimumDepthValue;
	float		m_pushAmount;
	float       m_pushAmountVehicleInterior;

#if CSM_DYNAMIC_DEPTH_RANGE
	bool        m_useDynamicDepthRangeInCutscenes;
	bool        m_forceUseDynamicDepthRange;
	float       m_dynamicRange;
	float       m_dynamicQuant;
	float       m_oldcs;
	float		m_z0;
#if __BANK
	CutSceneDepthGenerator	m_generator;
#endif // BANK

#endif // CSM_DYNAMIC_DEPTH_RANGE

	static BankBool	m_DisableInHeliChaseCam; //Fix for shadow issues in this cam mode. (B* 1846297)

#if __BANK
	float       m_inputRadius;
	float       m_inputFOV;
	float       m_finalRadius;
	float       m_finalZExpWeight;
	float       m_finalCascadeExtend;
	float       m_finalCascadeStart;
	bool        m_hasFocus;
	bool		m_useDepthResizingInCutscenes;
	bool		m_useCinematicCam;
	bool        m_useOccludersForAircraft;
	bool        m_graphValues;
	bool        m_forcePlayerTarget;
	bool		m_disableDynamicDepthRange;
	bool        m_useMinDepth;
#endif // __BANK

	bool		m_IsInInterior;
	bool        m_isAircraft;
	float		m_AircraftBlend;
	float		m_ShadowRangeBlend;
	float       m_prevCascadeScale;
	bool        m_forceAircraftShadows;
	int			m_prevSniperActive;
	float		m_prevSniperActiveCS;
	float		m_focalObjectBlend;

	SimpleCurve m_extendRadius;
	SimpleCurve m_extendCascade;
	SimpleCurve m_splitZExpWeight;
	SimpleCurve m_extendCascadeCar;
};

BankBool CCSMEntityTracker::m_DisableInHeliChaseCam = true;

// ================================================================================================

template <int N> class CCSMDepthBiasStateBuffer
{
public:
	void Init(int cullMode = grcRSV::CULL_BACK)
	{
		m_desc = grcRasterizerStateDesc(); // default desc
		m_desc.CullMode = cullMode;

		m_stateIndex = -1;

		for (int i = 0; i < N; i++)
		{
			m_states[i] = grcStateBlock::RS_Invalid;
		}

		m_currDepthBias			= 0.0f;
		m_currSlopeBias			= 0.0f;
		m_currDepthBiasClamp	= 0.0f;
	}

	grcRasterizerStateHandle Create(float depthBias, float slopeBias, float depthBiasClamp, u32 WIN32PC_ONLY(updateid))
	{
		grcRasterizerStateHandle state = grcStateBlock::RS_Invalid;

#if RSG_PC
		float	delta = fabsf(m_currDepthBias - depthBias);
#endif

#if !__FINAL
		if(!(m_stateIndex >= 0 && m_stateIndex < N))
		{
			grcDebugf1("[CascadeShadows] State index invalid: %d/%d", m_stateIndex, N);
		}
#endif

		if ((m_currDepthBias == depthBias WIN32PC_ONLY(|| delta < 0.0001f))	&&
			m_currSlopeBias			== slopeBias		&&
			m_currDepthBiasClamp	== depthBiasClamp	&&
			m_stateIndex >= 0 && m_stateIndex < N WIN32PC_ONLY(BANK_ONLY(&& gCascadeShadows.m_debug.m_rasterStateCacheEnabled) && m_updateId == updateid))
		{
			state = m_states[m_stateIndex];
		}
		else
		{
			m_stateIndex = (m_stateIndex + 1)%N;

			if (m_states[m_stateIndex] != grcStateBlock::RS_Invalid)
			{
				grcStateBlock::DestroyRasterizerState(m_states[m_stateIndex]); // destroy oldest state
			}

			grcRasterizerStateDesc desc = m_desc; // copy desc

			desc.DepthBiasDX9			= depthBias;
			desc.SlopeScaledDepthBias	= slopeBias;
			desc.DepthBiasClamp			= depthBiasClamp;

			state = grcStateBlock::CreateRasterizerState(desc);

			m_states[m_stateIndex]	= state;
			m_currDepthBias			= depthBias;
			m_currSlopeBias			= slopeBias;
			m_currDepthBiasClamp	= depthBiasClamp;
#if RSG_PC
			m_updateId				= updateid;
#endif
		}

		return state;
	}

private:
	grcRasterizerStateDesc   m_desc;
	grcRasterizerStateHandle m_states[N];
	int                      m_stateIndex;
	float                    m_currDepthBias;
	float					 m_currSlopeBias;
	float                    m_currDepthBiasClamp;
#if RSG_PC
	u32						 m_updateId;
#endif
};

// ================================================================================================

template <bool bCanBeWide> static Vec4V_Out CalcMinimumEnclosingSphere(
	Vec3V_In   origin, // typically  viewToWorld.GetCol3()
	Vec3V_In   axis,   // typically -viewToWorld.GetCol2()
	ScalarV_In tanHFOV,
	ScalarV_In tanVFOV,
	ScalarV_In z0,
	ScalarV_In z1
	)
{
	// to compute the minimum enclosing sphere of a frustum segment given tanHFOV, tanVFOV, z0, z1:
	//
	//	p = 1 + tanHFOV^2 + tanVFOV^2
	//	z = p*(z0 + z1)/2
	//	r = sqrt(z^2 - p*z0*z1)

	ScalarV p = ScalarV(V_ONE) + tanHFOV*tanHFOV + tanVFOV*tanVFOV;
	ScalarV z = p*(z0 + z1)*ScalarV(V_HALF);
	ScalarV r2;

	if (bCanBeWide) // this is only necessary if the frustum is fairly wide and/or short .. which it usually is unfortunately
	{
		const BoolV t = z > z1;

		// note that this test "z > z1" is equivalent to "(z0 + z1)*tanDFOV^2 > (z1 - z0)", which is always true if tanDFOV > 1
		z  = SelectFT(t, z, z1);
		r2 = SelectFT(t, z*z - p*z0*z1, z1*z1*(p - ScalarV(V_ONE)));
	}
	else
	{
		r2 = z*z - p*z0*z1;
	}

	return Vec4V(origin + axis*z, Sqrt(r2));
}

#if 0
static Vec4V_Out CalcMinimumEnclosingSphere_wide(
	Vec3V_In   origin,  // typically  viewToWorld.GetCol3()
	Vec3V_In   axis,    // typically -viewToWorld.GetCol2()
	ScalarV_In tanDFOV, // sqrt(tanHFOV^2 + tanVFOV^2)
	ScalarV_In z1
	)
{
	Assertf(IsGreaterThanOrEqualAll(tanDFOV, ScalarV(V_ONE)), "CalcMinimumEnclosingSphere_wide expected tanDFOV %f >= 1", tanDFOV.Getf());
	return Vec4V(AddScaled(origin, axis, z1), tanDFOV*z1);
}
#endif

static Vec2V_Out CalcOrthoViewport( // returns {zmin,zmax}
	grcViewport& viewport,
	Vec3V_In     boundsMin, // in basis-space, e.g. calculated via CalcMinimumEnclosingSphere or CalculateFrustumBounds using viewToBasis
	Vec3V_In     boundsMax,
	Mat33V_In    basisToWorld33,
	ScalarV_In   h0, // world height min/max used to calculate z extents
	ScalarV_In   h1,
	Mat34V*      prevWorldToCascade = NULL,
	float        BANK_ONLY(visibilityScale) = 1.0f,
	ScalarV_In	 zRangeScale = ScalarV(V_ONE),
	bool		 prevWorldToCascadeValid=false)
{
	Vec3V boundsCentre =     (boundsMax + boundsMin)*ScalarV(V_HALF);
	Vec3V boundsExtent = Max((boundsMax - boundsMin)*ScalarV(V_HALF), Vec3V(V_FLT_SMALL_6)); // somehow this is zero for the first two frames, causing an assert in grcViewport::Ortho

	if ( prevWorldToCascadeValid)
	{
		const Vec3V sphereCentre = UnTransformOrtho(*prevWorldToCascade, boundsCentre);
		const Vec2V texelSiz = boundsExtent.GetXY()*Vec2V(2.0f/(float)CRenderPhaseCascadeShadowsInterface::CascadeShadowResX(), 2.0f/(float)(CRenderPhaseCascadeShadowsInterface::CascadeShadowResY()));
		const Vec2V texelInv = Invert(texelSiz);
		const Vec2V texelMin = RoundToNearestIntNegInf(sphereCentre.GetXY()*texelInv);

		boundsCentre = Transform(*prevWorldToCascade, Vec3V(texelMin*texelSiz, sphereCentre.GetZ()));
	}

	Mat34V basisToWorld = Mat34V(
		+basisToWorld33.GetCol0(),
		-basisToWorld33.GetCol1(), // y is negative (flipped)
		-basisToWorld33.GetCol2(), // z is negative
		prevWorldToCascade ? boundsCentre : Multiply(basisToWorld33, boundsCentre)
	);

	const ScalarV Ox = boundsExtent.GetX();
	const ScalarV Oy = boundsExtent.GetY();
//	const ScalarV Oz = boundsExtent.GetZ();
	const ScalarV Pz = basisToWorld.GetCol3().GetZ();
	const ScalarV Lz = basisToWorld.GetCol2().GetZ();
	const ScalarV Qz = InvertSafe(Lz, ScalarV(V_HALF));

	const ScalarV Rz = Max(Ox, Oy)*SqrtSafe(ScalarV(V_ONE) - Lz*Lz, ScalarV(V_ZERO)); // assume sphere bounds

	ScalarV Oz_min = (h0 - Pz - Rz)*Qz; // should be Max(.., -Oz) ?
	ScalarV Oz_max = (h1 - Pz + Rz)*Qz;

	Oz_min *= zRangeScale;
	Oz_max *= Min(zRangeScale*ScalarV(V_TWO), ScalarV(V_ONE));  // depth scale z to improve quality
	Oz_max = Max( Oz_max, Oz_min+ScalarV(V_ONE));  
	//Oz_max *= zRangeScale; 

	viewport.SetWorldIdentity();
	viewport.SetCameraMtx(basisToWorld);
	viewport.Ortho(
		-Ox.Getf()BANK_ONLY(*visibilityScale),
		+Ox.Getf()BANK_ONLY(*visibilityScale),
		-Oy.Getf()BANK_ONLY(*visibilityScale),
		+Oy.Getf()BANK_ONLY(*visibilityScale),
		-Oz_max.Getf(), // ortho z is {-max,-min} instead of {min,max}
		-Oz_min.Getf()
	);

	if (prevWorldToCascade)
	{
		*prevWorldToCascade = basisToWorld;
	}

	return Vec2V(Ox, Oy);
}

#if __DEV
#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
static bool  g_dumpRenderTargetCountEntityIDs    = false;
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
#if __PS3
static bool  g_dumpRenderTargets                 = false;
static bool  g_dumpRenderTargetsAfterSetWaitFlip = false;
#endif // __PS3
static int   g_trackFunctionCounter = 0;
static float g_cascadeMaxAABBExtent[CASCADE_SHADOWS_COUNT];
#endif // __DEV

#include "renderer/Shadows/CascadeShadowsDepthHistogram.inl"

// ================================================================================================

#if CASCADE_SHADOWS_STENCIL_REVEAL

namespace rage { extern u32 g_AllowVertexBufferVramLocks; };

class CCSMVolumeVertexBuffer 
{
public:
	CCSMVolumeVertexBuffer()
	{
		m_decl = NULL;
		m_buff = NULL;
	}

	void InitResources(int numSlices)
	{
		const int numVerts = numSlices*2 + 2;

		grcFvf fvf;
		fvf.SetPosChannel(true, grcFvf::grcdsFloat3);

		g_AllowVertexBufferVramLocks++;

		m_decl = grmModelFactory::BuildDeclarator(&fvf, NULL, NULL, NULL);
		m_buff = grcVertexBuffer::Create(numVerts, fvf, 0);	
		m_buff->Lock();

		Vec3V* xy1 = rage_new Vec3V[numSlices];
		CalcSinCosTable(xy1, numSlices);

		float* src = (float*)xy1;
		float* dst = (float*)m_buff->GetLockPtr();

		for (int i = 0; i < numSlices; i++)
		{
			const float x = src[0];
			const float y = src[1];

			dst[0] = x;
			dst[1] = y;
			dst[2] = -256.0f;
			dst[3] = x;
			dst[4] = y;
			dst[5] = +256.0f;

			src += sizeof(Vec3V)/sizeof(float);
			dst += 6;
		}

		// last
		{
			dst[0] = xy1[0].GetXf();
			dst[1] = xy1[0].GetYf();
			dst[2] = -256.0f;
			dst[3] = dst[0];
			dst[4] = dst[1];
			dst[5] = +256.0f;
		}

		delete[] xy1;

		m_buff->Unlock();

		g_AllowVertexBufferVramLocks--;
	}

	void Draw() const
	{
		GRCDEVICE.SetVertexDeclaration(m_decl);
		GRCDEVICE.SetStreamSource(0, *m_buff, 0, m_buff->GetVertexStride());
		GRCDEVICE.DrawPrimitive(drawTriStrip, 0, m_buff->GetVertexCount());
		GRCDEVICE.ClearStreamSource(0);
	}

private:
	grcVertexDeclaration* m_decl;
	grcVertexBuffer*      m_buff;
};

#endif // CASCADE_SHADOWS_STENCIL_REVEAL

// ================================================================================================

// not sure how these work ..
EXT_PF_GROUP(BuildRenderList);
EXT_PF_GROUP(RenderPhase);
PF_TIMER(CRenderPhaseCascadeShadows_BuildRenderList,BuildRenderList);
PF_TIMER(CRenderPhaseCascadeShadows_BuildDrawList,RenderPhase);

#if __BANK
class CCSMDebugDrawFlags;
class CCSMDebugState;
class CCSMDebugGlobals;
#endif // __BANK

class CCSMState;
class CCSMGlobals;

class CCSMSharedShaderVars
{
public:
	void InitResources();
	void SetShaderVars(const CCSMGlobals& globals SHADOW_MATRIX_REFLECT_ONLY(, bool bShadowMatrixOnly, Vec4V_In reflectionPlane)) const;
	void SetCloudShaderVars(const CCSMGlobals& globals) const;
	void SetShadowMap(const grcTexture* tex) const;
	void RestoreShadowMap(const CCSMGlobals& globals) const;

	grcEffectGlobalVar m_gCSMShadowTextureId;
	grcEffectGlobalVar m_gCSMShadowTextureVSId;
	grcEffectGlobalVar m_gCSMDitherTextureId;
	grcEffectGlobalVar m_gCSMShaderVarsId_shared;
	grcEffectGlobalVar m_gCSMCloudTextureId;
	grcTexture*		   m_gCSMCloudNoiseTexture;
	grcEffectGlobalVar m_gCSMSmoothStepTextureId;
	grcEffectGlobalVar m_gCSMShadowParamsId;

#if VARIABLE_RES_SHADOWS
	grcEffectGlobalVar m_gCSMResolution;
#endif // VARIABLE_RES_SHADOWS
#if GS_INSTANCED_SHADOWS && !RSG_ORBIS // See SIMULATE_DEPTH_BIAS in cascadeshadows_receiving.fxh.
	grcEffectGlobalVar m_gCSMDepthBias;
	grcEffectGlobalVar m_gCSMDepthSlopeBias;
#endif // GS_INSTANCED_SHADOWS && !RSG_ORBIS
};

class CCSMLocalShaderVars
{
public:
	void InitResources(grmShader* shader);
	void SetShaderVars(const CCSMGlobals& globals) const;

	grmShader*   m_shader;
	grcEffectVar m_gCSMShaderVarsId_deferred;
};

#if __BANK

enum eCSMDebugShadowAlignMode
{
	CSM_ALIGN_WORLD_X ,
	CSM_ALIGN_WORLD_Y ,
	CSM_ALIGN_WORLD_Z ,
	CSM_ALIGN_CAMERA_X,
	CSM_ALIGN_CAMERA_Y,
	CSM_ALIGN_CAMERA_Z,
	CSM_ALIGN_COUNT   ,

	CSM_ALIGN_DEFAULT = CSM_ALIGN_WORLD_Z,
};

static const char** GetUIStrings_eCSMDebugShadowAlignMode()
{
	static const char* strings[] =
	{
		STRING(CSM_ALIGN_WORLD_X ),
		STRING(CSM_ALIGN_WORLD_Y ),
		STRING(CSM_ALIGN_WORLD_Z ),
		STRING(CSM_ALIGN_CAMERA_X),
		STRING(CSM_ALIGN_CAMERA_Y),
		STRING(CSM_ALIGN_CAMERA_Z),
		NULL
	};
	return strings;
}

static const char** GetUIStrings_eCSMSampleType()
{
	static const char* strings[] =
	{
		#define ARG1_DEF_CSMSampleType(arg0, type) STRING(type),
		FOREACH_ARG1_DEF_CSMSampleType(arg0, ARG1_DEF_CSMSampleType)
		#undef  ARG1_DEF_CSMSampleType
		NULL
	};
	return strings;
}

#if __DEV
static const char** GetUIStrings_eCSMQualityMode()
{
	static const char* strings[] =
	{
		"CSM_QM_IN_GAME",
		"CSM_QM_CUTSCENE_DEFAULT",
		"CSM_QM_MOONLIGHT",
	};
	CompileTimeAssert(NELEM(strings) == CSM_QM_COUNT);

	return strings;
}
#endif //__DEV

class CCSMDebugDrawFlags
{
public:
	bool m_drawCascadeGhosts; // badly-named variable - this indicates whether debug geometry will be drawn as a "ghost" in cascades that it is not part of 
	bool m_drawCascadeEdges;
//	bool m_drawCascadeBounds;
	bool m_drawSilhouettePolygon;
	bool m_drawSelectedEntityAABB; // for checking projection from worldspace to shadow map space
	bool m_drawWorldspace;
};

class CCSMDebugState
{
public:
	void Clear();

	void UpdateDebugObjects(
		const CCSMState&          state,
		const CCSMDebugDrawFlags& drawFlags,
		float                     opacity,
		bool                      cascadeBoundsForceOverlap,
		const ScalarV             cascadeSplitZ[CASCADE_SHADOWS_COUNT + 1],
		Mat34V_In                 viewToWorld,
		ScalarV_In                tanHFOV,
		ScalarV_In                tanVFOV
	);

	Vec4V m_shaderVars[CASCADE_SHADOWS_SHADER_VAR_COUNT_DEBUG];

	atArray<CCSMDebugObject*> m_objects;
};

class CCSMDebugGlobals : private datBase
{
public:
	void Init();
	void InitWidgets(bkBank& bk);
#if CASCADE_SHADOWS_DO_SOFT_FILTERING
	void InitWidgets_SoftShadows(bkBank& bk);
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING
	bool InitResourcesOnDemand();

	void  DebugDraw(const CCSMGlobals& globals);
	float DebugDrawRenderTarget(const CCSMState& state, const grcRenderTarget* rt, const Color32& colour, int pass, float dstX, float dstY, float dstScale, int cascadeIndex, int quadrantIndex, bool bDrawDebugObjects);
	void  DebugDrawDebugObjects(const CCSMState& state, Mat44V_In proj, Vec4V_In projWindow, int cascadeIndex);
	void  DebugDrawTexturedQuad(const CCSMState& state, const fwBox2D& dstBounds, const fwBox2D& srcBounds, int pass, const Color32& colour, float opacity, float z, const grcTexture* texture);

	void UpdateModifiedShadowSettings();

	void BeginFindingNearZShadows();
	float EndFindingNearZShadows();

	// === resources ==========================================

	grcEffectTechnique       m_debugShaderTechnique;
	grcEffectTechnique       m_debugShaderTechniqueOverlay;
	grcEffectVar             m_debugShaderBaseTextureId;
#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
	grcEffectVar             m_debugShaderEntityIDTextureId;
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
	grcEffectVar             m_debugShaderDepthBufferId;
	grcEffectVar             m_debugShaderNormalBufferId; // GBuffer::GetTarget(GBUFFER_RT_1)
	grcEffectVar             m_debugShaderViewToWorldProjectionId;
	grcEffectVar             m_debugShaderPerspectiveShearId;
	grcEffectVar             m_debugShaderVarsId;
	CCSMLocalShaderVars      m_debugShaderVars;

	// === resources ==========================================

	int                      m_showShadowMapCascadeIndex; // 0:none, 1:all, 2:cascade[0], 3:cascade[1], etc.
	int                      m_showShadowMapScale; // 0:(1x), 1:(1/2x), 2:(1/3x), 3:(1/4x)
	int						 m_nvidiaResolve;
	bool                     m_showShadowMapContrast;
	bool                     m_showShadowMapEdgeDetect;
	float                    m_showShadowMapEdgeDetectExp;
	float                    m_showShadowMapEdgeDetectTint;
#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
	bool                     m_showShadowMapEntityIDs;
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
	bool                     m_showShadowMapVS;
	bool                     m_showShadowMapMini;
	bool                     m_showInfo;
	bool                     m_wireframe;
#if __DEV
	bool                     m_clearEveryFrame;
	float                    m_clearValue;
#endif // __DEV
	float                    m_shadowDirectionClampValue;
	bool                     m_enableCascade[CASCADE_SHADOWS_COUNT];
#if __DEV && __PS3
#if HAS_RENDER_MODE_SHADOWS
	int                      m_renderListDrawModeNonEdge;
	int                      m_renderListDrawModeEdge;
#endif // HAS_RENDER_MODE_SHADOWS
	bool                     m_edgeDebuggingFlags_global;          // don't reset these flags after rendering shadows - let them affect all renderphases
	bool                     m_edgeDebuggingClipPlanes_frustum;    // cull models/geoms against frustum LTRB planes, doesn't do much for shadows
	bool                     m_edgeDebuggingClipPlanes_extended;   // cull models/geoms against EdgeClipPlanes
	bool                     m_edgeDebuggingClipPlanes_triangles;  // cull triangles against EdgeClipPlanes (edgeGeomCullClippedTriangles)
	bool                     m_spuModelDraw;                       // draw using spuModel::Draw codepath
	bool                     m_spuModelDrawSkinned;                // draw using spuModel::DrawSkinned codepath
	bool                     m_spuGeometryQBDraw;                  // draw using spuGeometryQB::Draw codepath
	bool                     m_grmGeometryEdgeDraw;                // draw using BEGIN_SPU_COMMAND(grmGeometryEdge__Draw) codepath, called from grmGeometryEdge::Draw
	bool                     m_grmGeometryEdgeDrawSkinned;         // draw using BEGIN_SPU_COMMAND(grmGeometryEdge__DrawSkinned) codepath, called from grmGeometryEdge::DrawSkinned
#endif // __DEV && __PS3
#if __PS3
	bool                     m_edgeClipPlanes;
	bool                     m_edgeCulling; // see grcDevice::SetWindow .. 'scissorArea'
#if CSM_EDGE_OCCLUDER_QUAD_EXCLUSION
	bool                     m_edgeOccluderQuadExclusionEnabled;
	bool                     m_edgeOccluderQuadExclusionExtended;
#endif // CSM_EDGE_OCCLUDER_QUAD_EXCLUSION
#if ENABLE_EDGE_CULL_DEBUGGING
	bool                     m_edgeCullDebugFlagsAllRenderphases;
	u32                      m_edgeCullDebugFlags; // eEdgeGeomCullingFlags
#endif // ENABLE_EDGE_CULL_DEBUGGING
#endif // __PS3

	CCSMDebugDrawFlags       m_drawFlags;
	bool                     m_drawShadowDebugShader;
	float                    m_drawOpacity; // opacity for debug rendering
	float                    m_shadowDebugOpacity;
	float                    m_shadowDebugNdotL;
	float                    m_shadowDebugGridOpacity;
	float                    m_shadowDebugSaturation; // 0=white
	int                      m_shadowDebugGridDivisions1;
	int                      m_shadowDebugGridDivisions2;
	float                    m_shadowDebugRingOpacity;

	CDebugCameraLock         m_cameraLock;

	bool                     m_cullFlags_RENDER_EXTERIOR;
	bool                     m_cullFlags_RENDER_INTERIOR;
	bool                     m_cullFlags_TRAVERSE_PORTALS;
	bool                     m_cullFlags_ENABLE_OCCLUSION;

#if CASCADE_SHADOWS_SCAN_DEBUG
	fwScanCascadeShadowDebug m_scanDebug;
#endif // CASCADE_SHADOWS_SCAN_DEBUG
	float                    m_scanCamFOVThreshold;
	float                    m_scanSphereThreshold;
	Vec4V                    m_scanSphereThresholdTweaks;
	float                    m_scanBoxExactThreshold;
	float                    m_scanBoxVisibilityScale;
	bool                     m_scanForceNoBackfacingPlanesWhenUnderwater;
#if __DEV
	bool                     m_scanViewerPortalEnabled;
	Vec4V                    m_scanViewerPortal;
#endif // __DEV

	bool                     m_cascadeBoundsSnap;                  // snap to texels
	bool                     m_cascadeBoundsSnapHighAltitudeFOV;   // snap to texels when using high-altitude modes (quadrant track and highres single)
	float                    m_cascadeBoundsHFOV;                  // ..
	float                    m_cascadeBoundsVFOV;                  // ..
	bool                     m_cascadeBoundsForceOverlap;          // whether or not cascades all overlap the smaller cascades .. may simplify sampling
	bool                     m_cascadeBoundsSplitZScale;           // ..
	bool                     m_cascadeBoundsIsolateSelectedEntity; // hack to force cascade bounds around selected entity
	float                    m_cascadeBoundsIsolateOffsetX;        // hack to offset shadow around selected entity in x
	float                    m_cascadeBoundsIsolateOffsetY;        // hack to offset shadow around selected entity in y
	float                    m_cascadeBoundsIsolateZoom;           // hack to zoom shadow around selected entity
	bool					 m_cascadeBoundsOverrideTimeCycle;	   // disable TC cascade0 scale

	float                    m_cascadeSplitZ[CASCADE_SHADOWS_COUNT + 1]; // first is camera near z, last is max shadow distance
	float                    m_currentCascadeSplitZ[CASCADE_SHADOWS_COUNT + 1]; // shows the final z plane distances
	float                    m_cascadeSplitZExpWeight; // exponential factor to apply to split z's

	bool                     m_worldHeightUpdate;
	float                    m_worldHeightAdj[2]; // min/max world height used to calculate shadow projection z bounds
	bool                     m_recvrHeightUpdate;
#if CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
	bool                     m_recvrHeightTrackDepthHistogram;
#endif // CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
	float                    m_recvrHeightAdj[2];
	float                    m_recvrHeightUnderwater;
	float                    m_recvrHeightUnderwaterTrack;

	int                      m_shadowAlignMode;            // eCSMDebugShadowAlignMode, normally we wouldn't do this (to avoid shimmering), better to align to global up vector
	bool                     m_shadowDirectionDoNotUpdate; // do not update shadow direction from global directional light (turn this on to freeze the direction)
	bool                     m_shadowDirectionEnabled;     // if not, we get the direction from the global directional light
	Vec3V                    m_shadowDirection;            // override shadow direction
	float                    m_shadowRotation;             // rotation along the shadow direction

#if RSG_PC
	bool					 m_modulateDepthSlopEnabled;
	bool					 m_rasterStateCacheEnabled;
#endif
	bool                     m_shadowDepthBiasEnabled;
	float                    m_shadowDepthBias;
	Vec4V                    m_shadowDepthBiasTweaks;
	bool                     m_shadowSlopeBiasEnabled;
	float                    m_shadowSlopeBias;
	float                    m_shadowSlopeBiasRPDB;
	Vec4V                    m_shadowSlopeBiasTweaks;
	bool					 m_shadowDepthBiasClampEnabled;
	float					 m_shadowDepthBiasClamp;
	Vec4V					 m_shadowDepthBiasClampTweaks;
	bool					 m_shadowAsyncClearWait;

	float                    m_cutoutAlphaRef;
	bool                     m_cutoutAlphaToCoverage;
	bool                     m_renderFadingEnabled;
	bool                     m_renderHairEnabled;

	bool                     m_shadowNormalOffsetEnabled;
	float                    m_shadowNormalOffset;
//	Vec4V                    m_shadowNormalOffsetTweaks;

	bool                     m_ditherWorld_NOT_USED;
	float                    m_ditherScale_NOT_USED;
	float                    m_ditherRadius;
	Vec4V                    m_ditherRadiusTweaks;

	float                    m_shadowFadeStart;

	bool                     m_numCascadesOverrideSettings;
	bool                     m_distanceMultiplierEnabled;
	bool                     m_distanceMultiplierOverrideSettings;

	bool                     m_cloudTextureOverrideSettings;
	bool                     m_useMaxBlending;

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
	float                    m_softShadowPenumbraRadius;
	int                      m_softShadowMaxKernelSize;
	bool                     m_softShadowUseEarlyOut;
	bool                     m_softShadowShowEarlyOut;
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING

	bool					m_DisableNightParticleShadows;
	bool					m_DisableNightAlphaShadows;
#if RMPTFX_USE_PARTICLE_SHADOWS && PTXDRAW_MULTIPLE_DRAWS_PER_BATCH
	bool                     m_particleShadowsUseMultipleDraw;
#endif // RMPTFX_USE_PARTICLE_SHADOWS && PTXDRAW_MULTIPLE_DRAWS_PER_BATCH

#if CASCADE_SHADOWS_STENCIL_REVEAL
#if __DEV && __XENON
	bool                     m_shadowPostRenderCopyDepth;
	bool                     m_shadowPostRenderRestoreHiZ;
#endif // __DEV && __XENON
	bool                     m_shadowStencilRevealEnabled;
	bool                     m_shadowStencilRevealUseHiStencil;
	bool                     m_shadowStencilRevealDebug;
	bool                     m_shadowStencilRevealWireframe;
	int                      m_shadowStencilRevealCount;
#endif // CASCADE_SHADOWS_STENCIL_REVEAL
	bool                     m_shadowRevealEnabled;
	bool                     m_shadowRevealWithStencil;
	bool					 m_shadowRevealOverrideTimeCycle;

#if __DEV // allows switching between two different settings
	bool                     m_altSettings_enabled;
	bool                     m_altSettings_worldHeightEnabled;
	float                    m_altSettings_worldHeight[2];
	bool                     m_altSettings_shadowDepthBiasEnabled;
	float                    m_altSettings_shadowDepthBias;
	Vec4V                    m_altSettings_shadowDepthBiasTweaks;
	bool                     m_altSettings_shadowSlopeBiasEnabled;
	float                    m_altSettings_shadowSlopeBias;
	Vec4V                    m_altSettings_shadowSlopeBiasTweaks;
#endif // __DEV

#if __DEV
	bool                     m_useIrregularFade;
	float                    m_useModifiedShadowFarDistanceScale;
	bool                     m_forceShadowMapInactive;
	bool                     m_renderOccluders_enabled;
	bool                     m_renderOccluders_useTwoPasses;
	bool                     m_renderOccluders_firstThreeCascades;
	bool                     m_renderOccluders_lastCascade;

	bool                     m_allowScriptControl;
#endif // __DEV

#if DEPTH_BOUNDS_SUPPORT
	bool                     m_useDepthBounds;
#endif //DEPTH_BOUNDS_SUPPORT
};

#endif // __BANK

#if CASCADE_SHADOWS_CLOUD_SHADOWS
class CCSMCloudTextureSettings
{
public:
	void Init()
	{
		// cloud shadows
		m_cloudShadowAmount   = 0.0f;
		// fog shadows
		m_fogShadowAmount     = 0.0f;
		m_fogShadowFalloff    = 1.0f;
		m_fogShadowBaseHeight = 0.0f;
	}

	// cloud shadows
	float m_cloudShadowAmount;   // timecycle (TCVAR_CLOUD_SHADOW_OPACITY)
	// fog shadows
	float m_fogShadowAmount;     // timecycle (TCVAR_FOG_SHADOW_AMOUNT)
	float m_fogShadowFalloff;    // timecycle (TCVAR_FOG_SHADOW_FALLOFF)
	float m_fogShadowBaseHeight; // timecycle (TCVAR_FOG_SHADOW_BASE_HEIGHT)
};
#endif // CASCADE_SHADOWS_CLOUD_SHADOWS

#if CSM_EDGE_OCCLUDER_QUAD_EXCLUSION
class CCSMOccluderQuad
{
public:
	void SetFromSphere(Vec3V_In centre, ScalarV_In radius, Vec3V_In xAxis, Vec3V_In yAxis, Vec3V_In zAxis, ScalarV_In depthOffset)
	{
		const Vec3V c = centre - zAxis*(radius + depthOffset);

		p[0] = c - xAxis*radius - yAxis*radius;
		p[1] = c + xAxis*radius - yAxis*radius;
		p[2] = c + xAxis*radius + yAxis*radius;
		p[3] = c - xAxis*radius + yAxis*radius;
	}

	Vec3V p[4];
};
#endif // CSM_EDGE_OCCLUDER_QUAD_EXCLUSION

// ================================================================================================

struct ShadowRevealSetup
{
	void Init(const char[]);
	void SetVars(const CCSMGlobals& globals, const grcTexture *depthTexture, const grcTexture *particleTexture, const Vector4 &particleParams) const;
	void SetShadowParams() const;
	grmShader*                  shader;
	grcEffectTechnique          technique;
	grcEffectTechnique          techniqueFirstThreeCascades;
	grcEffectTechnique          techniqueLastCascade;
	grcEffectTechnique          techniqueNoSoft;
	grcEffectTechnique          techniqueFirstThreeCascadesNoSoft;
	grcEffectTechnique          techniqueLastCascadeNoSoft;
#if __BANK
	grcEffectTechnique          techniqueOrthographic;
	grcEffectTechnique          techniqueOrthographicNoSoft;
#endif // __BANK
	grcEffectTechnique          techniqueCloudShadows;
	grcEffectTechnique          techniqueFast;
	grcEffectTechnique          techniqueSmoothStep; // technique to quickly generate a lookup table for a smoothstep from 0-1
	grcEffectTechnique          techniqueCloudAndParticlesOnly;

	grcEffectVar                depthBufferId;
	grcEffectVar                normalBufferId; // GBuffer::GetTarget(GBUFFER_RT_1)
#if DEVICE_EQAA && EQAA_DECODE_GBUFFERS
	grcEffectVar				normalBufferFmaskId; // GBuffer::GetFragmentMaskTexture(GBUFFER_FMASK_1)
#endif // DEVICE_EQAA && EQAA_DECODE_GBUFFERS
	grcEffectVar                shadowParams2Id;
#if CSM_PBOUNDS
	grcEffectVar                portalBoundsId;
#endif // CSM_PBOUNDS
	grcEffectVar                viewToWorldProjectionId;
	grcEffectVar                perspectiveShearId;
	CCSMLocalShaderVars         deferredVars;

#if RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
	grcEffectVar                particleShadowsParamsVar;
	grcEffectVar                particleShadowTextureId;
#endif // RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
#if CASCADE_SHADOWS_STENCIL_REVEAL
	grcEffectTechnique          stencilRevealTechnique;
	grcEffectVar                stencilRevealShadowToWorld;
	grcEffectVar                stencilRevealCascadeSphere;
#endif // CASCADE_SHADOWS_STENCIL_REVEAL
};

// ================================================================================================

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
enum SoftShadowTechnique
{
	SoftShadowBlur = 0,
	SoftShadowBlur_Optimised1,
	SoftShadowBlur_Optimised2,
	SoftShadowBlur_Optimised2ParticleCombine,
	SoftShadowBlur_CreateEarlyOut1,
	SoftShadowBlur_CreateEarlyOut2,
	SoftShadowBlur_EarlyOut,
	SoftShadowBlur_ShowEarlyOut,
	SoftShadowBlur_EarlyOutWithParticleCombine,
#if DEVICE_MSAA
	SoftShadowBlur_ResolveDepth,
#endif // DEVICE_MSAA
	SoftShadowBlur_TechCount
};
#endif //CASCADE_SHADOWS_DO_SOFT_FILTERING


struct ShadowProcessingSetup
{
	void Init(const char[]);
	grmShader*                  shader;
	grcEffectTechnique          technique;
	grcEffectVar                shadowMapId;
	grcEffectVar                waterNoiseMapId;
	grcEffectVar                waterHeightId;
#if CASCADE_SHADOWS_STENCIL_REVEAL && __BANK
	grcEffectVar                stencilRevealShadowToWorld;
	grcEffectVar                stencilRevealCascadeSphere;
	grcEffectVar                stencilRevealCascadeColour;
#endif // CASCADE_SHADOWS_STENCIL_REVEAL && __BANK

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
	grcEffectTechnique          techniqueBlur[SoftShadowBlur_TechCount];

	grcEffectVar                intermediateVar;
	grcEffectVar                earlyOutVar;
	grcEffectVar                earlyOutParamsVar;
	grcEffectVar                depthBuffer2Var;

#if DEVICE_MSAA
	grcEffectVar                intermediateAAVar;
#endif	//DEVICE_MSAA

	grcEffectVar                projectionParamsVar;
	grcEffectVar                targetSizeParamVar;
	grcEffectVar                kernelParamVar;
#endif //CASCADE_SHADOWS_DO_SOFT_FILTERING
};

/*
// ================================================================================================
TODO --
I believe there may be a problem with the way CCSMState is buffered. The fact that I have to
access the render state from BuildDrawList is suspicious, and I'm seeing some jittering when I
isolate the selected entity and also in the debug draw bounds (but not in cptestbed). So far
this hasn't affected the actual game ..

Also, the way cascade bounds are computed as a box around a sphere makes them somewhat
inflexible, it would be better to compute them as a box (potentially asymmetrical) in
shadowspace. This would help with quadrant tracker and possibly cutscene tracking modes.
However, there are several bits of code which still don't support asymmetrical cascade bounds,
such as visibility (ScanCascadeShadows). Ideally, in CalcCascadeBounds we would compute just
BoxMin and BoxMax (but not Sphere) and the rest of the code would be able to handle that.

One more thing - there's no real reason why we need to buffer the cascade bounds (or tracker
bounds), as they are only used on the update thread.
// ================================================================================================
*/

class CCSMState
{
public:
	void Clear();
#if USE_NV_SHADOW_LIB
	Mat44V			  m_eyeWorldToView;
	Mat44V			  m_eyeViewToProjection;
#endif
#if USE_AMD_SHADOW_LIB
	Mat44V            m_eyeViewProjection;
	Mat44V            m_eyeViewProjectionInv;
#endif
	Mat44V            m_viewToWorldProjection;
	Vec4V             m_viewPerspectiveShear;
	Vec4V             m_cascadeShaderVars_shared  [CASCADE_SHADOWS_SHADER_VAR_COUNT_SHARED  ];
	Vec4V             m_cascadeShaderVars_deferred[CASCADE_SHADOWS_SHADER_VAR_COUNT_DEFERRED];
#if CSM_PBOUNDS
	Vec4V             m_portalBounds[CASCADE_SHADOWS_NUM_PORTAL_BOUNDS];
#endif // CSM_PBOUNDS

	grcViewportWindow m_viewports                 [CASCADE_SHADOWS_COUNT];
	Vec4V             m_cascadeBoundsSphere       [CASCADE_SHADOWS_COUNT]; // TODO -- get rid of this, use box min/max
	Vec3V             m_cascadeBoundsBoxMin       [CASCADE_SHADOWS_COUNT];
	Vec3V             m_cascadeBoundsBoxMax       [CASCADE_SHADOWS_COUNT];
	Vec2V             m_cascadeBoundsExtentXY     [CASCADE_SHADOWS_COUNT]; // {zmin,zmax}

	Mat33V            m_shadowToWorld33;
	Mat33V            m_worldToShadow33;
	atRangeArray<Vec4V, CASCADE_SHADOWS_SCAN_NUM_PLANES> m_shadowClipPlanes;
	int               m_shadowClipPlaneCount;

	float             m_nearClip;
	float             m_farClip;
	float             m_firstThreeCascadesEnd;
	float             m_lastCascadeStart;
	float             m_lastCascadeEnd;

	bool              m_shadowIsActive;
	bool			  m_renderedAlphaEntityIntoParticleShadowMap;
	grcRenderTarget*  m_shadowMap;
	int               m_shadowMapNumCascades;

	CCSMCloudTextureSettings m_cloudTextureSettings;
	eCSMSampleType           m_sampleType;
#if CSM_EDGE_OCCLUDER_QUAD_EXCLUSION
	CCSMOccluderQuad  m_occluderQuads[CASCADE_SHADOWS_COUNT];
#endif // CSM_EDGE_OCCLUDER_QUAD_EXCLUSION

#if __BANK
	CCSMDebugState    m_debug;
#endif // __BANK
};

class CCSMGlobals
{
public:
	CCSMGlobals();

	void Init(grmShader* deferredLightingShader);
#if USE_NV_SHADOW_LIB
	void ResetNVidia();
#endif
	void InitResources(grmShader* deferredLightingShader);
	void DeleteShaders();
	void InitShaders(grmShader* deferredLightingShader);

	void CreateRenderTargets();
#if RSG_PC
	void DestroyRenderTargets();

	bool Initialized() { return m_initiailized; }

	void DeviceLost();
	void DeviceReset();
#endif // RSG_PC

#if VARIABLE_RES_SHADOWS
	void ReComputeShadowRes();
	int CascadeShadowResX() { return m_shadowResX; }
	int CascadeShadowResY() { return m_shadowResY; }
	void SetCascadeShadowResX(int resX) { m_shadowResX = resX; }
	void SetCascadeShadowResY(int resY) { m_shadowResY = resY; }
#endif // VARIABLE_RES_SHADOWS

	void CreateShadowMapMini();

	grcRenderTarget* GetShadowMap();
	grcRenderTarget* GetShadowMapVS();
#if SHADOW_ENABLE_ALPHASHADOW
	bool			 UseAlphaShadows();
#endif // SHADOW_ENABLE_ALPHASHADOW

#if RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
	grcRenderTarget* GetAlphaShadowMap();
#endif // RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW

#if __D3D11 || RSG_ORBIS
	grcRenderTarget* GetShadowMapReadOnly();
#endif // __D3D11 || RSG_ORBIS

#if __PS3
	grcRenderTarget* GetShadowMapPatched();
#endif // __PS3

	grcRenderTarget* GetShadowMapMini(ShadowMapMiniType mapType = SM_MINI_UNDERWATER);
	const grcTexture* GetSmoothStepTexture();

	void CopyToShadowMapVS();
	void CopyShadowsToMiniMap(grcTexture* noiseMap, float height);

	void AdjustSplitZ(float expWeight);
	void CalcSplitZ(ScalarV (&zSplit)[CASCADE_SHADOWS_COUNT+1], bool allowHighAltitudeAdjust);
	void CalcFogRayFadeRange(float (&range)[2]);

	void CalcCascadeBounds(
		Vec4V_InOut   boundsSphere, // shadowspace
		Vec3V_InOut   boundsBoxMin, // shadowspace
		Vec3V_InOut   boundsBoxMax, // shadowspace
		Mat34V_In     viewToWorld,
		Mat34V_In     viewToShadow,
		Mat33V_In     worldToShadow33,
#if CSM_ASYMMETRICAL_VIEW
		const fwRect& tanBounds,
#endif // CSM_ASYMMETRICAL_VIEW
		ScalarV_In    tanHFOV,
		ScalarV_In    tanVFOV,
		ScalarV_In    tanHFOV_bounds,
		ScalarV_In    tanVFOV_bounds,
		ScalarV_InOut z0,
		ScalarV_InOut z1,
		int           cascadeIndex,
		bool          bCascadeBoundsSnap
	);

	bool AllowGrassCacadeCulling(float lastCascadeEnd) const;

	// TODO -- why are there two viewports passed in here?
	void UpdateViewport(grcViewport& viewport, const grcViewport& vp);
	void UpdateTimecycleState(CCSMState& state);
#if CASCADE_SHADOWS_STENCIL_REVEAL && __BANK
	void ShadowStencilRevealDebug(bool bDrawToGBuffer0) const;
#endif // CASCADE_SHADOWS_STENCIL_REVEAL && __BANK
	void ShadowRevealToScreen(const ShadowRevealSetup &setup, EdgeMode emode, grcEffectTechnique tech, float nearClip, float farClip, float nearZ, float farZ, bool useWindows = false) const;
	void ShadowRevealToScreenHelper(const ShadowRevealSetup &setup, EdgeMode emode, const CCSMState &state) const;
	void ShadowReveal() const;
#if CASCADE_SHADOWS_DO_SOFT_FILTERING
	void SoftShadowToScreen() const;
	void SoftShadowFullScreenBlit(const ShadowProcessingSetup &setup, grcRenderTarget* pTarget, grcRenderTarget* pDepth, SoftShadowTechnique techIndex) const;
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING

	CCSMState& GetUpdateState() { return m_state[m_stateIndex ^ 0]; }
	CCSMState& GetRenderState() { return m_state[m_stateIndex ^ 1]; }

	const CCSMState& GetUpdateState() const { return m_state[m_stateIndex ^ 0]; }
	const CCSMState& GetRenderState() const { return m_state[m_stateIndex ^ 1]; }

#if CSM_PORTALED_SHADOWS 
	ExteriorPortals& GetUpdateWindowRenderer() { return WindowRenderer[m_stateIndex ^ 0]; }
	ExteriorPortals& GetRenderWindowRenderer() { return WindowRenderer[m_stateIndex ^ 1]; }


	const ExteriorPortals& GetUpdateWindowRenderer() const { return WindowRenderer[m_stateIndex ^ 0]; }
	const ExteriorPortals& GetRenderWindowRenderer() const { return WindowRenderer[m_stateIndex ^ 1]; }
#endif

	void Synchronise() { m_stateIndex ^= 1; DEV_ONLY(if (g_trackFunctionCounter > 0) { g_trackFunctionCounter--; }) }

	void SetDeferredLightingShaderVars() const { m_deferredLightingShaderVars.SetShaderVars(*this); }
	void SetSharedShaderVars(SHADOW_MATRIX_REFLECT_ONLY(bool bShadowMatrixOnly, Vec4V_In reflectionPlane)) const { m_sharedShaderVars.SetShaderVars(*this SHADOW_MATRIX_REFLECT_ONLY(, bShadowMatrixOnly, reflectionPlane)); }
	void SetCloudShaderVars() const { m_sharedShaderVars.SetCloudShaderVars(*this); }
	void SetSharedShadowMap(const grcTexture* tex) const { m_sharedShaderVars.SetShadowMap(tex); }
	void RestoreSharedShadowMap() const { m_sharedShaderVars.RestoreShadowMap(*this); }

	void SetSoftShadowProperties();
	int GetDefaultFilter() const;

	bool AreCloudShadowsEnabled() const;

#if VARIABLE_RES_SHADOWS
	int                         m_shadowResX;
	int                         m_shadowResY;
#endif // VARIABLE_RES_SHADOWS

#if !__PS3
	grmShader*                  m_shadowCastingShader;
#endif // !__PS3
#if RSG_PC
	CCSMDepthBiasStateBuffer<8> m_shadowCastingShader_RS[CSM_RS_COUNT][CASCADE_SHADOWS_COUNT]; // quad-buffered for extra paranoid safety
#else
	CCSMDepthBiasStateBuffer<4> m_shadowCastingShader_RS[CSM_RS_COUNT][CASCADE_SHADOWS_COUNT]; // quad-buffered for extra paranoid safety
#endif
	grcRasterizerStateHandle    m_shadowCastingShader_RS_current[NUMBER_OF_RENDER_THREADS][CSM_RS_COUNT]; // accessed from render thread only

	ShadowProcessingSetup		m_shadowProcessing[MM_TOTAL];

	grcRenderTarget*            m_shadowMapBase;
#if RMPTFX_USE_PARTICLE_SHADOWS
	grcRenderTarget*            m_shadowMapReadOnly;
#endif // RMPTFX_USE_PARTICLE_SHADOWS && __D3D11
#if __PS3
	grcRenderTarget*            m_shadowMapPatched;
#endif // __PS3

#if CSM_USE_DUMMY_COLOUR_TARGET
	grcRenderTarget*            m_shadowMapDummy;
#endif // CSM_USE_DUMMY_COLOUR_TARGET
#if CSM_USE_SPECIAL_FULL_TEXTURE
	grcTexture*                 m_shadowMapSpecialFull;
#endif // CSM_USE_SPECIAL_FULL_TEXTURE
#if RMPTFX_USE_PARTICLE_SHADOWS
	grcRenderTarget*            m_particleShadowMap;
#endif // RMPTFX_USE_PARTICLE_SHADOWS
	grcRenderTarget*            m_shadowMapVS; // R32F
	grcRenderTarget*            m_shadowMapMini;
	grcRenderTarget*            m_shadowMapMini2;	
#if __PS3 && __BANK
	grcRenderTarget*            m_shadowMapMiniPatched;
#endif // __PS3 && __BANK
#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
	grcRenderTarget*            m_shadowMapEntityIDs;
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
	grcRenderTarget*            m_smoothStepRT;

	ShadowRevealSetup m_shadowReveal[MM_TOTAL];

	grcDepthStencilStateHandle  m_shadowRevealShaderState_DS;
	#if (RSG_ORBIS || RSG_DURANGO) && DEPTH_BOUNDS_SUPPORT && __BANK
		grcDepthStencilStateHandle  m_shadowRevealShaderState_DS_noDepthBounds;
		#if MSAA_EDGE_PASS
		grcDepthStencilStateHandle  m_shadowRevealShaderState_DS_noDepthBounds_edge;
		#endif
	#endif
	grcBlendStateHandle         m_shadowRevealShaderState_BS;
	grcBlendStateHandle         m_shadowRevealShaderState_SoftShadow_BS;


	CCSMCloudTextureSettings    m_cloudTextureSettings;

	grcTexture*                 m_ditherTexture;
	int                         m_techniqueGroupId;
	int                         m_techniqueTextureGroupId;
#if __PS3
	int                         m_techniqueGroupId_edge;
	int                         m_techniqueTextureGroupId_edge;
#endif // __PS3

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
	bool                        m_softShadowEnabled;

	grcRenderTarget*            m_softShadowIntermediateRT;
	grcRenderTarget*            m_softShadowEarlyOut1RT;
	grcRenderTarget*            m_softShadowEarlyOut2RT;

#if DEVICE_MSAA
	grcRenderTarget*            m_softShadowIntermediateRT_Resolved;
#endif	//DEVICE_MSAA
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING

#if RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
	bool						m_enableAlphaShadows;
	bool                        m_enableParticleShadows;
	u32                         m_maxParticleShadowCascade;
#endif // RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW

	CCSMLocalShaderVars         m_deferredLightingShaderVars;
	CCSMSharedShaderVars        m_sharedShaderVars;

#if CASCADE_SHADOWS_STENCIL_REVEAL
	CCSMVolumeVertexBuffer      m_volumeVertexBuffer;
#endif // CASCADE_SHADOWS_STENCIL_REVEAL

	fwScanCascadeShadowInfo     m_scanCascadeShadowInfo;

	bool                        m_shadowIsActive;   // calculated in Update(), will be true if global directional light is present
	Mat33V                      m_shadowToWorld33;  // calculated in Update()
	Mat34V                      m_prevWorldToCascade[CASCADE_SHADOWS_COUNT]; // used to add hysteresis
	ScalarV                     m_cascadeSplitZ[CASCADE_SHADOWS_COUNT + 1];
	ScalarV                     m_worldHeight[2];
	bool                        m_worldHeightReset;
	ScalarV                     m_recvrHeight[2];
	bool                        m_recvrHeightReset;
	bool						m_enableScreenSizeCheck;
#if RSG_PC
	Vec4V						m_depthBiasPrecisionScale;
	float						m_slopeBiasPrecisionScale;
#endif

	int                         m_numCascades;
	float                       m_distanceMultiplier;
#if ALLOW_SHADOW_DISTANCE_SETTING
	float                       m_distanceMultiplierSetting;
#endif // ALLOW_SHADOW_DISTANCE_SETTING
	static bool                 m_shadowDirectionApplyClamp;

	int                         m_scriptShadowType; // eCSMShadowType
#if CSM_QUADRANT_TRACK_ADJ
	float                       m_scriptQuadrantTrackExpand1;
	float                       m_scriptQuadrantTrackExpand2;
	float                       m_scriptQuadrantTrackSpread;
	Vec3V                       m_scriptQuadrantTrackScale;
	Vec3V                       m_scriptQuadrantTrackOffset;
#endif // CSM_QUADRANT_TRACK_ADJ
	float                       m_scriptBoundPosition;
	bool                        m_scriptQuadrantTrackFlyCamera;
	bool                        m_scriptQuadrantTrackForceVisAll;
	bool                        m_scriptCascadeBoundsEnable[CASCADE_SHADOWS_COUNT];
	bool                        m_scriptCascadeBoundsLerpFromCustscene[CASCADE_SHADOWS_COUNT];
	float                       m_scriptCascadeBoundsLerpFromCustsceneDuration[CASCADE_SHADOWS_COUNT];
	float                       m_scriptCascadeBoundsLerpFromCustsceneTime[CASCADE_SHADOWS_COUNT];
	Vec4V                       m_scriptCascadeBoundsSphere[CASCADE_SHADOWS_COUNT]; // w is radius scale
	ScalarV                     m_scriptCascadeBoundsTanHFOV;
	ScalarV                     m_scriptCascadeBoundsTanVFOV;
	float                       m_scriptCascadeBoundsScale;
	float                       m_scriptSplitZExpWeight;
	float                       m_scriptEntityTrackerScale;
	bool                        m_scriptWorldHeightUpdate;
	bool                        m_scriptRecvrHeightUpdate;
	float                       m_scriptDitherRadiusScale;
	bool                        m_scriptDepthBiasEnabled;
	float                       m_scriptDepthBias;
	bool                        m_scriptSlopeBiasEnabled;
	float                       m_scriptSlopeBias;
	int                         m_scriptShadowSampleType; // eCSMSampleType
	int                         m_scriptShadowQualityMode; // eCSMQualityMode
	int                         m_currentShadowQualityMode;

#if GS_INSTANCED_SHADOWS
	Vec4V                       m_shadowDepthBias;
	Vec4V                       m_shadowDepthSlopeBias;
	bool                        m_bInstShadowActive;
#endif // GS_INSTANCED_SHADOWS
#if VARIABLE_RES_SHADOWS
	Vec4V                       m_shadowResolution;
#endif // VARIABLE_RES_SHADOWS

	CCSMEntityTracker           m_entityTracker;

#if __BANK
	CCSMDebugGlobals            m_debug;
#endif // __BANK

	int							m_defaultSampleType;
	int							m_defaultSoftness;
	int                         m_useSyncFilter;

	grcViewport 				m_viewport; //main cascade shadow viewport (range encompasses all cascades, used for visibility and occlusion)

private:
	CCSMState                   m_state[2];
#if CSM_PORTALED_SHADOWS
	ExteriorPortals             WindowRenderer[2];
#endif // CSM_PORTALED_SHADOWS
	int                         m_stateIndex;
#if RSG_PC
	bool							m_initiailized;
#endif
#if USE_NV_SHADOW_LIB
private:
	NV_ShadowLib_Handle*			m_shadowBufferHandle;
public:
	void NvShadowReveal(const CCSMState&) const;
	void NvShadowRevealDebug(const CCSMState&) const;

	bool							m_enableNvidiaShadows;
	bool							m_nvShadowsDebugVisualizeDepth;
	bool							m_nvShadowsDebugVisualizeCascades;
	float							m_nvBiasScale;
	NV_ShadowLib_BufferRenderParams	m_shadowParams;
	NV_ShadowLib_ExternalMapDesc	m_mapDesc;
	float							m_baseMinSize;
	float							m_baseLightSize;
#endif
#if RSG_PC
	float							m_CachedDepthBias[CASCADE_SHADOWS_COUNT];
#endif
#if USE_AMD_SHADOW_LIB
private:
	grcDepthStencilStateHandle		m_amdStencilMarkShaderState_DS;
	grcDepthStencilStateHandle		m_amdStencilClearShaderState_DS;
	grcDepthStencilStateHandle		m_amdShadowRevealShaderState_DS;
	grcRenderTarget*				m_amdShadowResult;
public:
	void AMDShadowReveal(const CCSMState&) const;
	void AMDShadowRevealDebug(const CCSMState&) const;

	bool							m_enableAMDShadows;
	float							m_amdCascadeRegionBorder;
	float							m_amdSunWidth[CASCADE_SHADOWS_COUNT];
	float							m_amdDepthOffset[CASCADE_SHADOWS_COUNT];
	int								m_amdFilterKernelIndex[CASCADE_SHADOWS_COUNT];
#endif
};

CCSMGlobals gCascadeShadows;

// ================================================================================================

void ShadowRevealSetup::Init(const char shaderName[])
{
	shader = grmShaderFactory::GetInstance().Create();
	shader->Load(shaderName);
	Assert(shader);
	technique                   = shader->LookupTechnique("ShadowRender");
	techniqueFirstThreeCascades = shader->LookupTechnique("ShadowRenderFirstThreeCascades");
	techniqueLastCascade        = shader->LookupTechnique("ShadowRenderLastCascade");	

	techniqueNoSoft                   = shader->LookupTechnique("ShadowRenderNoSoft");
	techniqueFirstThreeCascadesNoSoft = shader->LookupTechnique("ShadowRenderFirstThreeCascadesNoSoft");
	techniqueLastCascadeNoSoft        = shader->LookupTechnique("ShadowRenderLastCascadeNoSoft");	
#if __BANK
	techniqueOrthographic       = shader->LookupTechnique("ShadowRenderOrthographic");
	techniqueOrthographicNoSoft = shader->LookupTechnique("ShadowRenderOrthographicNoSoft");
#endif // __BANK
	techniqueCloudShadows       = shader->LookupTechnique("ShadowRenderCloudShadows");
	techniqueSmoothStep         = shader->LookupTechnique("SmoothStep");

	depthBufferId               = shader->LookupVar("depthBuffer");
	normalBufferId              = shader->LookupVar("normalBuffer");
#if DEVICE_EQAA && EQAA_DECODE_GBUFFERS
	normalBufferFmaskId			= shader->LookupVar("normalBufferFmask", GRCDEVICE.IsEQAA());
#endif // DEVICE_EQAA && EQAA_DECODE_GBUFFERS
	shadowParams2Id				= shader->LookupVar("shadowParams2");
	viewToWorldProjectionId     = shader->LookupVar("viewToWorldProjectionParam");
	perspectiveShearId          = shader->LookupVar("perspectiveShearParam");

#if CSM_PBOUNDS
	portalBoundsId              = shader->LookupVar("PortalBounds");
#endif // CSM_PBOUNDS

#if USE_NV_SHADOW_LIB || USE_AMD_SHADOW_LIB
	if (GRCDEVICE.GetDxFeatureLevel() >= 1100 && GRCDEVICE.IsUsingVendorAPI())
	{
		techniqueCloudAndParticlesOnly = shader->LookupTechnique("ShadowRevealCloudsAndParticlesOnly");
	}
#endif
	deferredVars.InitResources(shader);

#if RMPTFX_USE_PARTICLE_SHADOWS
	particleShadowsParamsVar    = shader->LookupVar("particleShadowsParams");
	particleShadowTextureId		= shader->LookupVar("gCSMParticleShadowTexture");
#endif // RMPTFX_USE_PARTICLE_SHADOWS

#if CASCADE_SHADOWS_STENCIL_REVEAL
	stencilRevealTechnique     = shader->LookupTechnique("StencilReveal");
	stencilRevealShadowToWorld = shader->LookupVar("StencilRevealShadowToWorld");
	stencilRevealCascadeSphere = shader->LookupVar("StencilRevealCascadeSphere");
#endif // CASCADE_SHADOWS_STENCIL_REVEAL
}

void ShadowRevealSetup::SetVars(const CCSMGlobals& globals, const grcTexture *depthTexture, const grcTexture *particleTexture, const Vector4 &particleParams) const
{
	const CCSMState& state = globals.GetRenderState();

	shader->SetVar(depthBufferId , depthTexture);
	shader->SetVar(normalBufferId, GBuffer::GetTarget(GBUFFER_RT_1));
#if DEVICE_EQAA && EQAA_DECODE_GBUFFERS
	shader->SetVar(normalBufferFmaskId, GBuffer::GetFragmentMaskTexture(GBUFFER_FMASK_1));
#endif // DEVICE_EQAA && EQAA_DECODE_GBUFFERS
	shader->SetVar(viewToWorldProjectionId, state.m_viewToWorldProjection);
	shader->SetVar(perspectiveShearId     , state.m_viewPerspectiveShear);
	deferredVars.SetShaderVars(globals); // deferred

#if RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
	shader->SetVar(particleShadowsParamsVar, particleParams);
	Assertf(CRenderPhaseCascadeShadowsInterface::GetParticleShadowSetFlagThreadID() == -1 ||
		CRenderPhaseCascadeShadowsInterface::GetParticleShadowSetFlagThreadID() == g_RenderThreadIndex,
		"Particle Shadow Flag was set in a different thread %d and accessed on a different thread %u.",
		CRenderPhaseCascadeShadowsInterface::GetParticleShadowSetFlagThreadID(), g_RenderThreadIndex);
	ASSERT_ONLY(CRenderPhaseCascadeShadowsInterface::SetParticleShadowAccessFlagThreadID((int)g_RenderThreadIndex);)
	//We have to make sure that the particle shadows and the final reveal occur on the same thread to make use of this optimization
	shader->SetVar(particleShadowTextureId, particleTexture);
#endif // RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
}

void ShadowRevealSetup::SetShadowParams() const
{
	float ditherScale = Lerp(g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_DIRECTIONAL_SHADOW_SOFTNESS)*0.5f + 0.5f, 0.5f, 2.5f);
	shader->SetVar(shadowParams2Id, Vec4V(ditherScale, 0.0f, 0.0f, 0.0f));
#if CSM_PBOUNDS
	shader->SetVar(portalBoundsId, GetRenderState().m_portalBounds, CASCADE_SHADOWS_NUM_PORTAL_BOUNDS);
#endif // CSM_PBOUNDS
}

void ShadowProcessingSetup::Init(const char shaderName[])
{
	shader = grmShaderFactory::GetInstance().Create();
	shader->Load(shaderName);
	Assert(shader);
	technique       = shader->LookupTechnique("shadowprocessing");
	shadowMapId     = shader->LookupVar("shadowMap");
	waterNoiseMapId = shader->LookupVar("noiseTexture");
	waterHeightId   = shader->LookupVar("gWaterHeight");
#if CASCADE_SHADOWS_STENCIL_REVEAL && __BANK
	stencilRevealShadowToWorld = shader->LookupVar("StencilRevealShadowToWorld");
	mstencilRevealCascadeSphere = shader->LookupVar("StencilRevealCascadeSphere");
	stencilRevealCascadeColour = shader->LookupVar("StencilRevealCascadeColour");
#endif // CASCADE_SHADOWS_STENCIL_REVEAL && __BANK

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
	techniqueBlur[SoftShadowBlur]                             = shader->LookupTechnique("shadowprocessing_SoftShadowBlur");
	techniqueBlur[SoftShadowBlur_Optimised1]                  = shader->LookupTechnique("shadowprocessing_SoftShadowBlur_Optimised1");
	techniqueBlur[SoftShadowBlur_Optimised2]                  = shader->LookupTechnique("shadowprocessing_SoftShadowBlur_Optimised2");
	techniqueBlur[SoftShadowBlur_Optimised2ParticleCombine]   = shader->LookupTechnique("shadowprocessing_SoftShadowBlur_Optimised2ParticleCombine");
	techniqueBlur[SoftShadowBlur_CreateEarlyOut1]             = shader->LookupTechnique("shadowprocessing_SoftShadowBlur_CreateEarlyOut1");
	techniqueBlur[SoftShadowBlur_CreateEarlyOut2]             = shader->LookupTechnique("shadowprocessing_SoftShadowBlur_CreateEarlyOut2");
	techniqueBlur[SoftShadowBlur_EarlyOut]                    = shader->LookupTechnique("shadowprocessing_SoftShadowBlur_EarlyOut");
	techniqueBlur[SoftShadowBlur_EarlyOutWithParticleCombine] = shader->LookupTechnique("shadowprocessing_SoftShadowBlur_EarlyOutWithParticleCombine");
	techniqueBlur[SoftShadowBlur_ShowEarlyOut]                = shader->LookupTechnique("shadowprocessing_SoftShadowBlur_ShowEarlyOut");
#if DEVICE_MSAA
	techniqueBlur[SoftShadowBlur_ResolveDepth]                = shader->LookupTechnique("shadowprocessing_SoftShadowBlur_ResolveDepth", false);
#endif // DEVICE_MSAA

	intermediateVar		= shader->LookupVar("intermediateTarget");
	earlyOutVar			= shader->LookupVar("earlyOut");
	depthBuffer2Var		= shader->LookupVar("depthBuffer2");

#if DEVICE_MSAA
	intermediateAAVar	= shader->LookupVar("intermediateTargetAA");
#endif	// DEVICE_MSAA

	projectionParamsVar = shader->LookupVar("projectionParams0");
	targetSizeParamVar  = shader->LookupVar("targetSizeParam0");
	kernelParamVar      = shader->LookupVar("kernelParam0");
	earlyOutParamsVar	= shader->LookupVar("earlyOutParams");
#endif //CASCADE_SHADOWS_DO_SOFT_FILTERING
}

// ================================================================================================

#if __BANK

void CCSMDebugState::Clear()
{
	for (int i = 0; i < m_objects.GetCount(); i++)
	{
		delete m_objects[i];
	}

	m_objects.clear();
}

// utility function
static FASTRETURNCHECK(bool) GetEntityApproxAABB(spdAABB& aabb, const CEntity* pEntity)
{
	if (pEntity && !pEntity->GetIsRetainedByInteriorProxy())
	{
		fwBaseEntityContainer* pContainer = pEntity->GetOwnerEntityContainer();

		if (pContainer)
		{
			u32 index = pContainer->GetEntityDescIndex(pEntity);
			if (index != fwBaseEntityContainer::INVALID_ENTITY_INDEX)
			{
				pContainer->GetApproxBoundingBox(index, aabb);
				return true;
			}
		}
	}

	return false;
}

void CCSMDebugState::UpdateDebugObjects(
	const CCSMState&          state,
	const CCSMDebugDrawFlags& drawFlags,
	float                     opacity,
	bool                      cascadeBoundsForceOverlap,
	const ScalarV             cascadeSplitZ[CASCADE_SHADOWS_COUNT + 1],
	Mat34V_In                 viewToWorld,
	ScalarV_In                tanHFOV,
	ScalarV_In                tanVFOV
	)
{
	USE_DEBUG_MEMORY();

	if (opacity > 0.0f && gCascadeShadows.m_debug.m_showShadowMapCascadeIndex != 0)
	{
		const int N = CASCADE_SHADOWS_COUNT;
		Vec3V p[4][N + 1];

		for (int cascadeIndex = 0; cascadeIndex <= N; cascadeIndex++)
		{
			const ScalarV z = cascadeSplitZ[cascadeIndex];

			p[0][cascadeIndex] = Transform(viewToWorld, Vec3V(-tanHFOV, -tanVFOV, ScalarV(V_NEGONE))*z);
			p[1][cascadeIndex] = Transform(viewToWorld, Vec3V(+tanHFOV, -tanVFOV, ScalarV(V_NEGONE))*z);
			p[2][cascadeIndex] = Transform(viewToWorld, Vec3V(-tanHFOV, +tanVFOV, ScalarV(V_NEGONE))*z);
			p[3][cascadeIndex] = Transform(viewToWorld, Vec3V(+tanHFOV, +tanVFOV, ScalarV(V_NEGONE))*z);
		}

		for (int cascadeIndex = 0; cascadeIndex <= N; cascadeIndex++)
		{
			if (drawFlags.m_drawCascadeEdges)
			{
				int flags = 0;

				if (cascadeBoundsForceOverlap)
				{
					if (cascadeIndex == 0)
					{
						flags = (1<<N) - 1;
					}
					else
					{
						flags = 1<<(cascadeIndex - 1);
					}
				}
				else
				{
					if (cascadeIndex > 0) { flags |= (1<<(cascadeIndex - 1)); }
					if (cascadeIndex < N) { flags |= (1<<(cascadeIndex - 0)); }
				}

				m_objects.PushAndGrow(rage_new CCSMDebugLine(Color32(255,0,0,255), opacity, flags, p[0][cascadeIndex], p[1][cascadeIndex]));
				m_objects.PushAndGrow(rage_new CCSMDebugLine(Color32(255,0,0,255), opacity, flags, p[1][cascadeIndex], p[3][cascadeIndex]));
				m_objects.PushAndGrow(rage_new CCSMDebugLine(Color32(255,0,0,255), opacity, flags, p[3][cascadeIndex], p[2][cascadeIndex]));
				m_objects.PushAndGrow(rage_new CCSMDebugLine(Color32(255,0,0,255), opacity, flags, p[2][cascadeIndex], p[0][cascadeIndex]));
			}

			if (cascadeIndex < N)
			{
				if (drawFlags.m_drawCascadeEdges)
				{
					int flags = 0;

					if (cascadeBoundsForceOverlap)
					{
						flags = ((1<<N) - 1) & ~((1<<cascadeIndex) - 1);
					}
					else
					{
						flags = 1<<cascadeIndex;
					}

					for (int j = 0; j < 4; j++)
					{
						m_objects.PushAndGrow(rage_new CCSMDebugLine(Color32(255,0,0,255), opacity, flags, p[j][cascadeIndex + 0], p[j][cascadeIndex + 1]));
					}
				}

				(void)state;
				/*if (drawFlags.m_drawCascadeBounds)
				{
					const Vec3V boundsMin = state.m_cascadeBoundsBoxMin[cascadeIndex];
					const Vec3V boundsMax = state.m_cascadeBoundsBoxMax[cascadeIndex];

					const Vec3V p00 = Multiply(state.m_shadowToWorld33, Vec3V(boundsMin.GetX(), boundsMin.GetY(), ScalarV(V_ZERO)));
					const Vec3V p10 = Multiply(state.m_shadowToWorld33, Vec3V(boundsMax.GetX(), boundsMin.GetY(), ScalarV(V_ZERO)));
					const Vec3V p01 = Multiply(state.m_shadowToWorld33, Vec3V(boundsMin.GetX(), boundsMax.GetY(), ScalarV(V_ZERO)));
					const Vec3V p11 = Multiply(state.m_shadowToWorld33, Vec3V(boundsMax.GetX(), boundsMax.GetY(), ScalarV(V_ZERO)));

					m_objects.PushAndGrow(rage_new CCSMDebugLine(Color32(32,0,0,255), opacity, 1<<cascadeIndex, p00, p10));
					m_objects.PushAndGrow(rage_new CCSMDebugLine(Color32(32,0,0,255), opacity, 1<<cascadeIndex, p10, p11));
					m_objects.PushAndGrow(rage_new CCSMDebugLine(Color32(32,0,0,255), opacity, 1<<cascadeIndex, p11, p01));
					m_objects.PushAndGrow(rage_new CCSMDebugLine(Color32(32,0,0,255), opacity, 1<<cascadeIndex, p01, p00));
				}*/
			}
		}

		if (drawFlags.m_drawSelectedEntityAABB)
		{
			spdAABB aabb;

			if (GetEntityApproxAABB(aabb, (CEntity*)g_PickerManager.GetSelectedEntity()))
			{
				m_objects.PushAndGrow(rage_new CCSMDebugBox(Color32(32,32,255,255), opacity, (1<<N) - 1, aabb.GetMin(), aabb.GetMax()));
			}
		}

		if (drawFlags.m_drawWorldspace)
		{
			for (int i = 0; i < m_objects.GetCount(); i++)
			{
				const CCSMDebugObject* obj = m_objects[i];

				for (int cascadeIndex = 0; cascadeIndex < CASCADE_SHADOWS_COUNT; cascadeIndex++)
				{
					if ((gCascadeShadows.m_debug.m_enableCascade[cascadeIndex] && obj->IsFlagSet(cascadeIndex)) ||
						(cascadeIndex == 0 && obj->IsWorldspace()))
					{
						obj->Draw_fwDebugDraw();
						break;
					}
				}
			}
		}

		if (1) // draw shadow portals
		{
			const fwPortalCorners* portals       = fwScanNodesDebug::DebugDrawGetShadowPortals();
			const int              portalCount   = fwScanNodesDebug::DebugDrawGetShadowPortalCount();
			const int              isolatedIndex = *fwScanNodesDebug::DebugDrawShadowIsolatedPortalIndex();

			for (int i = 0; i < portalCount; i++)
			{
				Color32 colorU8(255,255,0,255);

				if (i == isolatedIndex)
				{
					colorU8.SetRed(0);
				}

				m_objects.PushAndGrow(rage_new CCSMDebugLine(colorU8, opacity, (1<<N) - 1, VECTOR3_TO_VEC3V(portals[i].GetCorner(0)), VECTOR3_TO_VEC3V(portals[i].GetCorner(1))));
				m_objects.PushAndGrow(rage_new CCSMDebugLine(colorU8, opacity, (1<<N) - 1, VECTOR3_TO_VEC3V(portals[i].GetCorner(1)), VECTOR3_TO_VEC3V(portals[i].GetCorner(2))));
				m_objects.PushAndGrow(rage_new CCSMDebugLine(colorU8, opacity, (1<<N) - 1, VECTOR3_TO_VEC3V(portals[i].GetCorner(2)), VECTOR3_TO_VEC3V(portals[i].GetCorner(3))));
				m_objects.PushAndGrow(rage_new CCSMDebugLine(colorU8, opacity, (1<<N) - 1, VECTOR3_TO_VEC3V(portals[i].GetCorner(3)), VECTOR3_TO_VEC3V(portals[i].GetCorner(0))));
			}
		}
	}
}

void CCSMDebugGlobals::Init()
{
	m_debugShaderVars.m_shader            = NULL; // will get inited in InitResourcesOnDemand
	
	//need to load debug_shadows.fx on init, loading on demand doesn't work with multiple renderthreads 
	InitResourcesOnDemand();
	
	m_showShadowMapCascadeIndex           = 0; // none
	m_showShadowMapScale                  = 0; // 1x
	m_nvidiaResolve						  = MSDepthResolve::depthResolve_Farthest;
	m_showShadowMapContrast               = false;
	m_showShadowMapEdgeDetect             = false;
	m_showShadowMapEdgeDetectExp          = 0.25f;
	m_showShadowMapEdgeDetectTint         = 0.25f;
#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
	m_showShadowMapEntityIDs              = false;
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
	m_showShadowMapVS                     = false;
	m_showShadowMapMini                   = false;
	m_showInfo                            = false;
	m_wireframe                           = false;
#if __DEV
	m_clearEveryFrame                     = false;
	m_clearValue                          = 1.0f;
#endif // __DEV
	m_shadowDirectionClampValue           = CSM_DEFAULT_SHADOW_DIRECTION_CLAMP;
#if __DEV && __PS3
#if HAS_RENDER_MODE_SHADOWS
	m_renderListDrawModeNonEdge           = 0;
	m_renderListDrawModeEdge              = 0;
#endif // HAS_RENDER_MODE_SHADOWS
	m_edgeDebuggingFlags_global           = false;
	m_edgeDebuggingClipPlanes_frustum     = true;
	m_edgeDebuggingClipPlanes_extended    = true;
	m_edgeDebuggingClipPlanes_triangles   = true;
	m_spuModelDraw                        = true;
	m_spuModelDrawSkinned                 = true;
	m_spuGeometryQBDraw                   = true;
	m_grmGeometryEdgeDraw                 = true;
	m_grmGeometryEdgeDrawSkinned          = true;
#endif // __DEV && __PS3
#if __PS3
	m_edgeClipPlanes                      = true;
	m_edgeCulling                         = true;
#if CSM_EDGE_OCCLUDER_QUAD_EXCLUSION
	m_edgeOccluderQuadExclusionEnabled    = true;
	m_edgeOccluderQuadExclusionExtended   = true;
#endif // CSM_EDGE_OCCLUDER_QUAD_EXCLUSION
#if ENABLE_EDGE_CULL_DEBUGGING
	m_edgeCullDebugFlagsAllRenderphases   = false;
	m_edgeCullDebugFlags                  = EDGE_CULL_DEBUG_DEFAULT;
#endif // ENABLE_EDGE_CULL_DEBUGGING
#endif // __PS3

	m_drawFlags.m_drawCascadeGhosts       = true;
	m_drawFlags.m_drawCascadeEdges        = true;
//	m_drawFlags.m_drawCascadeBounds       = true;
	m_drawFlags.m_drawSilhouettePolygon   = false;
	m_drawFlags.m_drawSelectedEntityAABB  = true;
	m_drawFlags.m_drawWorldspace          = false;
	m_drawShadowDebugShader               = false;
	m_drawOpacity                         = 1.0f;

	m_shadowDebugOpacity                  = 0.0f;
	m_shadowDebugNdotL                    = 0.0f;
	m_shadowDebugSaturation               = 1.0f;
	m_shadowDebugRingOpacity              = 0.0f;
	m_shadowDebugGridOpacity              = 0.0f;
	m_shadowDebugGridDivisions1           = 4;
	m_shadowDebugGridDivisions2           = 1;

	m_cullFlags_RENDER_EXTERIOR           = true;
	m_cullFlags_RENDER_INTERIOR           = true;
	m_cullFlags_TRAVERSE_PORTALS          = true;
	m_cullFlags_ENABLE_OCCLUSION          = true;

#if CASCADE_SHADOWS_SCAN_DEBUG
	m_scanDebug                           = fwScanCascadeShadowDebug();
#endif // CASCADE_SHADOWS_SCAN_DEBUG
	m_scanCamFOVThreshold                 = 0.5f;
	m_scanSphereThreshold                 = CSM_DEFAULT_SPHERE_THRESHOLD;
	m_scanSphereThresholdTweaks           = CSM_DEFAULT_SPHERE_THRESHOLD_TWEAKS;
	m_scanBoxExactThreshold               = CSM_DEFAULT_BOX_EXACT_THRESHOLD;
	m_scanBoxVisibilityScale              = 1.0f;
	m_scanForceNoBackfacingPlanesWhenUnderwater = true;
#if __DEV
	m_scanViewerPortalEnabled             = false;
	m_scanViewerPortal                    = Vec4V(Vec2V(V_NEGONE), Vec2V(V_ONE));
#endif // __DEV

	m_cascadeBoundsSnap                   = true;
	m_cascadeBoundsSnapHighAltitudeFOV    = true;
	m_cascadeBoundsHFOV                   = 0.0f;
	m_cascadeBoundsVFOV                   = 0.0f;
	m_cascadeBoundsForceOverlap           = true;
	m_cascadeBoundsSplitZScale            = true;
	m_cascadeBoundsIsolateSelectedEntity  = false;
	m_cascadeBoundsIsolateOffsetX         = 0.0f;
	m_cascadeBoundsIsolateOffsetY         = 0.0f;
	m_cascadeBoundsIsolateZoom            = 1.0f;
	m_cascadeBoundsOverrideTimeCycle	  = false;

	// setup splitZ
	{
		const int N = CASCADE_SHADOWS_COUNT;

		const float z0 = CSM_DEFAULT_SPLIT_Z_MIN;
		const float z1 = CSM_DEFAULT_SPLIT_Z_MAX;

		m_cascadeSplitZ[0] = z0;
		m_cascadeSplitZ[N] = z1;

		for (int i = 1; i < N; i++)
		{
			m_cascadeSplitZ[i] = z0 + (z1 - z0)*(float)i/(float)N;
		}
	}

	// setup drawflags, etc.
	{
		for (int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
		{
			m_enableCascade[i] = true;
		}
	}

	m_cascadeSplitZExpWeight              = CSM_DEFAULT_SPLIT_Z_EXP_WEIGHT;

	m_worldHeightUpdate                   = CSM_DEFAULT_WORLD_HEIGHT_UPDATE != 0;
	m_worldHeightAdj[0]                   = CSM_DEFAULT_WORLD_HEIGHT_MIN;
	m_worldHeightAdj[1]                   = CSM_DEFAULT_WORLD_HEIGHT_MAX;

	m_recvrHeightUpdate                   = CSM_DEFAULT_RECVR_HEIGHT_UPDATE != 0;
#if CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
	m_recvrHeightTrackDepthHistogram      = false;
#endif // CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
	m_recvrHeightAdj[0]                   = CSM_DEFAULT_RECVR_HEIGHT_MIN;
	m_recvrHeightAdj[1]                   = CSM_DEFAULT_RECVR_HEIGHT_MAX;
	m_recvrHeightUnderwater               = CSM_DEFAULT_RECVR_HEIGHT_UNDERWATER;
	m_recvrHeightUnderwaterTrack          = CSM_DEFAULT_RECVR_HEIGHT_UNDERWATER_TRACK;

	m_shadowAlignMode                     = CSM_ALIGN_DEFAULT;
	m_shadowDirectionDoNotUpdate          = false;
	m_shadowDirectionEnabled              = false;
	m_shadowDirection                     = -Vec3V(V_Z_AXIS_WZERO);
	m_shadowRotation                      = 0.0f;

#if RSG_PC
	m_modulateDepthSlopEnabled			  = true;
	m_rasterStateCacheEnabled			  = true;
#endif
	m_shadowDepthBiasEnabled              = true;
	m_shadowDepthBias                     = CSM_DEFAULT_DEPTH_BIAS;
	m_shadowDepthBiasTweaks               = CSM_DEFAULT_DEPTH_BIAS_TWEAKS;
	m_shadowSlopeBiasEnabled              = true;
	m_shadowSlopeBias                     = CSM_DEFAULT_SLOPE_BIAS;
	m_shadowSlopeBiasRPDB                 = CSM_DEFAULT_SLOPE_BIAS_RPDB;
	m_shadowSlopeBiasTweaks               = CSM_DEFAULT_SLOPE_BIAS_TWEAKS;
	m_shadowDepthBiasClampEnabled         = true;
	m_shadowDepthBiasClamp                = CSM_DEFAULT_DEPTH_BIAS_CLAMP;
	m_shadowDepthBiasClampTweaks          = CSM_DEFAULT_DEPTH_BIAS_CLAMP_TWEAKS;

	m_shadowAsyncClearWait                = true;

	m_cutoutAlphaRef                      = CSM_DEFAULT_CUTOUT_ALPHA_REF;
	m_cutoutAlphaToCoverage               = CSM_DEFAULT_CUTOUT_ALPHA_TO_COVERAGE;
	m_renderFadingEnabled                 = true; // should this be false? need to test this ..
	m_renderHairEnabled                   = !PARAM_nohairshadows.Get();

	m_shadowNormalOffsetEnabled           = true;
	m_shadowNormalOffset                  = CSM_DEFAULT_NORMAL_OFFSET;
//	m_shadowNormalOffsetTweaks            = CSM_DEFAULT_NORMAL_OFFSET_TWEAKS;

	m_ditherWorld_NOT_USED                = false;
	m_ditherScale_NOT_USED                = CSM_DEFAULT_DITHER_SCALE;
	m_ditherRadius                        = CSM_DEFAULT_DITHER_RADIUS;
	m_ditherRadiusTweaks                  = CSM_DEFAULT_DITHER_RADIUS_TWEAKS;

	m_shadowFadeStart                     = CSM_DEFAULT_FADE_START;

	m_numCascadesOverrideSettings         = false;
	m_distanceMultiplierEnabled           = true;
	m_distanceMultiplierOverrideSettings  = false;

	m_cloudTextureOverrideSettings        = false;

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
	m_softShadowPenumbraRadius            = CSM_DEFAULT_SOFT_SHADOW_PENUMBRA_RADIUS;
	m_softShadowMaxKernelSize             = CSM_DEFAULT_SOFT_SHADOW_MAX_KERNEL_SIZE;
	m_softShadowUseEarlyOut               = true;
	m_softShadowShowEarlyOut              = false;
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING

	m_DisableNightParticleShadows		 = CSM_DISABLE_PARTICLE_SHADOW_NIGHT;
	m_DisableNightAlphaShadows			 = CSM_DISABLE_ALPHA_SHADOW_NIGHT;

#if RMPTFX_USE_PARTICLE_SHADOWS && PTXDRAW_MULTIPLE_DRAWS_PER_BATCH
	m_particleShadowsUseMultipleDraw      = true;
#endif // RMPTFX_USE_PARTICLE_SHADOWS && PTXDRAW_MULTIPLE_DRAWS_PER_BATCH

#if CASCADE_SHADOWS_STENCIL_REVEAL
#if __DEV && __XENON
	m_shadowPostRenderCopyDepth           = true;
	m_shadowPostRenderRestoreHiZ          = true;
#endif // __DEV && __XENON
	m_shadowStencilRevealEnabled          = false;
	m_shadowStencilRevealUseHiStencil     = false;
	m_shadowStencilRevealDebug            = false;
	m_shadowStencilRevealWireframe        = false;
	m_shadowStencilRevealCount            = CASCADE_SHADOWS_COUNT;
#endif // CASCADE_SHADOWS_STENCIL_REVEAL
	m_shadowRevealEnabled                 = 1;
	m_shadowRevealWithStencil             = true;

#if __DEV
	m_altSettings_enabled                 = false;
	m_altSettings_worldHeightEnabled      = false;
	m_altSettings_worldHeight[0]          = CSM_DEFAULT_WORLD_HEIGHT_MIN;
	m_altSettings_worldHeight[1]          = CSM_DEFAULT_WORLD_HEIGHT_MAX;
	m_altSettings_shadowDepthBiasEnabled  = false;
	m_altSettings_shadowDepthBias         = CSM_DEFAULT_DEPTH_BIAS;
	m_altSettings_shadowDepthBiasTweaks   = CSM_DEFAULT_DEPTH_BIAS_TWEAKS;
	m_altSettings_shadowSlopeBiasEnabled  = false;
	m_altSettings_shadowSlopeBias         = CSM_DEFAULT_SLOPE_BIAS;
	m_altSettings_shadowSlopeBiasTweaks   = CSM_DEFAULT_SLOPE_BIAS_TWEAKS;
#endif // __DEV

#if __DEV
	m_useIrregularFade                    = CSM_USE_IRREGULAR_FADE;
	m_useModifiedShadowFarDistanceScale   = CSM_DEFAULT_MODIFIED_FAR_DISTANCE_SCALE;
	m_forceShadowMapInactive              = false;
	m_renderOccluders_enabled             = false;
	m_renderOccluders_useTwoPasses        = true;
	m_renderOccluders_firstThreeCascades  = true;
	m_renderOccluders_lastCascade         = true;
#endif // __DEV

#if __BANK && DEPTH_BOUNDS_SUPPORT
	m_useDepthBounds                      = true;
#endif // DEPTH_BOUNDS_SUPPORT


#if __DEV
	m_allowScriptControl                  = true;
#endif // __DEV

#if __DEV
	sysMemSet(g_cascadeMaxAABBExtent, 0, sizeof(g_cascadeMaxAABBExtent));
#endif // __DEV

	UpdateModifiedShadowSettings();
}

#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD && __DEV

static s32 CompareFunc_u32(u32 const* a, u32 const* b)
{
	return (int)(*b) - (int)(*a);
}

static int DumpRenderTargetCountEntityIDs(grcRenderTarget* rt, int cascadeIndex)
{
	int numEntities = 0;

	if (rt && rt->GetBitsPerPixel() == 32 && cascadeIndex < CASCADE_SHADOWS_COUNT)
	{
		const int w = CRenderPhaseCascadeShadowsInterface::CascadeShadowResX();
		const int h = CRenderPhaseCascadeShadowsInterface::CascadeShadowResY()*(cascadeIndex == INDEX_NONE ? CASCADE_SHADOWS_COUNT : 1);
		const int y = CRenderPhaseCascadeShadowsInterface::CascadeShadowResY()*(cascadeIndex == INDEX_NONE ? 0 : cascadeIndex);

		atArray<u32,0,u32> buf;
		buf.Resize(w*h);

		grcTextureLock lock;
		rt->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock);
		Assertf((int)lock.Pitch == w*(int)sizeof(u32), "grcTexture::LockRect pitch = %d, expected %d", (int)lock.Pitch, w*sizeof(u32));
		sysMemCpy(&buf[0], (const u8*)lock.Base + y*w*sizeof(u32), w*h*sizeof(u32));
		rt->UnlockRect(lock);

		buf.QSort(0, -1, CompareFunc_u32);

		u32 prev = 0;

		for (int i = 0; i < w*h; i++)
		{
			if (prev != buf[i])
			{
				prev = buf[i];
				numEntities++;
			}
		}
	}

	return numEntities;
}

static void DumpRenderTargetCountEntityIDs_button()
{
	g_dumpRenderTargetCountEntityIDs = true;
}

#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD && __DEV

#if __DEV

#if __PS3

static void DumpRenderTarget(const char* path, grcRenderTarget* rt, bool bDepth = false, float projZ = 0.0f, float projW = 0.0f)
{
	USE_DEBUG_MEMORY();

	if (rt)
	{
		Displayf("DumpRenderTarget(\"%s\"..)", path);

		grcTextureLock lock;
		rt->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock);

		const int w = rt->GetWidth ();
		const int h = rt->GetHeight();

		grcImage* image = NULL;

		if (bDepth)
		{
			image = grcImage::Create(w, h, 1, grcImage::R32F, grcImage::STANDARD, 0, 0);

			float* dst = (float*)image->GetBits();
			u32*   buf = rage_new u32[w];

			float zmin = 0.0f;
			float zmax = 0.0f;

			const grcViewport* vp = gVpMan.GetCurrentGameGrcViewport();

			for (int y = 0; y < h; y++)
			{
				sysMemCpy(buf, (const u32*)lock.Base + y*w, w*sizeof(u32));

				for (int x = 0; x < w; x++)
				{
					const u32   r = buf[x]; // raw data from depth buffer
					const float d = (float)(r >> 8)/(float)((1<<24) - 1); // TODO -- to support on 360, convert floating point depth

					float z = d;

					if (projZ != 0.0f)
					{
						if (x == 0 && y == 0)
						{
							Displayf("  mapping z <- 1/(d*%f - %f)", projZ, projW);
						}

						z = ShaderUtil::GetLinearDepth(vp, ScalarV(d)).Getf();
					}

					dst[x + y*w] = z;

					if (x == 0 && y == 0)
					{
						zmin = zmax = z;
					}
					else
					{
						zmin = Min<float>(z, zmin);
						zmax = Max<float>(z, zmax);
					}
				}
			}

			delete[] buf;

			if (zmin < zmax) // normalise
			{
				Displayf("  normalising [%f..%f]", zmin, zmax);

				for (int i = 0; i < w*h; i++)
				{
					dst[i] = (dst[i] - zmin)/(zmax - zmin);
				}
			}
		}
		else
		{
			image = grcImage::Create(w, h, 1, grcImage::A8B8G8R8, grcImage::STANDARD, 0, 0);

			sysMemCpy(image->GetBits(), lock.Base, w*h*sizeof(Color32));
		}

		image->SaveDDS(path);
		image->Release();

		rt->UnlockRect(lock);
	}
}

static void DumpRenderTargets_button()
{
	g_dumpRenderTargets = true;
}

#endif // __PS3

static void ResetTrackFunctionCounter_button()
{
	g_trackFunctionCounter = 3;
}

static void ResetMaxCascadeAABBExtent_button()
{
	sysMemSet(g_cascadeMaxAABBExtent, 0, sizeof(g_cascadeMaxAABBExtent));
}

static void ResetScriptControl_button()
{
	CRenderPhaseCascadeShadowsInterface::Script_InitSession();
}

#endif // __DEV

template <int i> static void CascadeShadowsShowRenderingStats_button()
{
	if (CPostScanDebug::GetCountModelsRendered())
	{
		CPostScanDebug::SetCountModelsRendered(false);
		gDrawListMgr->SetStatsHighlight(false);
	}
	else
	{
		CPostScanDebug::SetCountModelsRendered(true, DL_RENDERPHASE_CASCADE_SHADOWS);
		gDrawListMgr->SetStatsHighlight(true, "DL_SHADOWS", i == 0, i == 1, false);
		grcDebugDraw::SetDisplayDebugText(true);

#if TC_DEBUG
		g_timeCycle.GetGameDebug().SetDisplayModifierInfo(false);
#endif // TC_DEBUG

		CBlockView::SetDisplayInfo(false); // i don't really know what this is, but it's polluting the debug output so turn it off
	}
}

static void CascadeShadowsWorldHeightReset_button()
{
	gCascadeShadows.m_worldHeightReset = true;
}

static void CascadeShadowsRecvrHeightReset_button()
{
	gCascadeShadows.m_recvrHeightReset = true;
}

static void CSMToggleDebugDraw()
{
	if (!gCascadeShadows.m_debug.m_drawShadowDebugShader)
	{
		gCascadeShadows.m_debug.m_drawShadowDebugShader  = true;
		gCascadeShadows.m_debug.m_shadowDebugRingOpacity = 1.0f;
		gCascadeShadows.m_debug.m_shadowDebugGridOpacity = 0.0f;
	}
	else if (gCascadeShadows.m_debug.m_shadowDebugRingOpacity == 1.0f)
	{
		gCascadeShadows.m_debug.m_drawShadowDebugShader  = true;
		gCascadeShadows.m_debug.m_shadowDebugRingOpacity = 0.5f;
		gCascadeShadows.m_debug.m_shadowDebugGridOpacity = 0.25f;
	}
	else
	{
		gCascadeShadows.m_debug.m_drawShadowDebugShader  = false;
		gCascadeShadows.m_debug.m_shadowDebugRingOpacity = 0.0f;
		gCascadeShadows.m_debug.m_shadowDebugGridOpacity = 0.0f;
	}
}

#if __DEV

PARAM(TestShadowSetup,"");

static void TestShadowSetup()
{
	if (PARAM_TestShadowSetup.Get())
	{
		gCascadeShadows.m_debug.m_scanDebug.m_flags_ &= ~BIT(CSM_SCAN_DEBUG_FLAG_CLIP_BACKFACING_PLANES);
		gCascadeShadows.m_debug.m_scanDebug.m_flags_ &= ~BIT(CSM_SCAN_DEBUG_FLAG_TEST_CASCADE_BOX_1_EX);
		gCascadeShadows.m_entityTracker.SetActive(false);
#if __PS3
		gCascadeShadows.m_debug.m_edgeClipPlanes = false;
#endif // __PS3
		gCascadeShadows.m_debug.m_drawShadowDebugShader = true;
		gCascadeShadows.m_debug.m_shadowDebugGridOpacity = 0.2f;
	}
}

#endif // __DEV

namespace rage {
	extern bank_u8 sGrassCascadeMask;
}

void CCSMDebugGlobals::InitWidgets(bkBank& bk)
{
	USE_DEBUG_MEMORY();

#if __DEV
	TestShadowSetup();
#endif // __DEV

	const char* _UIStrings_showShadowMapCascadeIndex[] =
	{
		"None",
		"All",
		"Cascade 0",
		"Cascade 1",
		"Cascade 2",
		"Cascade 3",
		"Quadrants",
	};

	const char* _UIStrings_m_showShadowMapScale[] =
	{
		"1 x",
		"1/2 x",
		"1/3 x",
		"1/4 x",
	};


	const char* _UIStrings_m_nvidiaResolve[] =
	{
		"Sample0",
		"Closest",
		"Farthest",
		"Dominant",
	};

#if __DEV
	bk.AddToggle("Use Irregular Fade"               , &m_useIrregularFade, datCallback(MFA(CCSMDebugGlobals::UpdateModifiedShadowSettings), this));
	bk.AddSlider("Use Modified Distance Scale"      , &m_useModifiedShadowFarDistanceScale, 1.0f, 2.0f, 1.0f/32.0f);
	bk.AddToggle("Force Shadow Map Inactive"        , &m_forceShadowMapInactive);
#if DEPTH_BOUNDS_SUPPORT
	bk.AddToggle("Use Depth Bounds"                 , &m_useDepthBounds);
#endif // DEPTH_BOUNDS_SUPPORT
#endif // __DEV
	bk.AddSeparator();
	bk.AddCombo("NVidia depth resolve"				, &m_nvidiaResolve, NELEM(_UIStrings_m_nvidiaResolve), _UIStrings_m_nvidiaResolve);
	bk.AddSeparator();
	bk.AddCombo ("Show Shadow Map"                  , &m_showShadowMapCascadeIndex, NELEM(_UIStrings_showShadowMapCascadeIndex), _UIStrings_showShadowMapCascadeIndex);
	bk.AddCombo ("Show Shadow Map Scale"            , &m_showShadowMapScale, NELEM(_UIStrings_m_showShadowMapScale), _UIStrings_m_showShadowMapScale);
	bk.AddToggle("Show Shadow Map Contrast"         , &m_showShadowMapContrast);
	bk.AddToggle("Show Shadow Map Edge Detect"      , &m_showShadowMapEdgeDetect);
	bk.AddSlider("Show Shadow Map Edge Detect Exp"  , &m_showShadowMapEdgeDetectExp, 0.125f, 1.0f, 1.0f/64.0f);
	bk.AddSlider("Show Shadow Map Edge Detect Tint" , &m_showShadowMapEdgeDetectTint, 0.0f, 1.0f, 1.0f/32.0f);
#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
	bk.AddToggle("Show Shadow Map Entity IDs"       , &m_showShadowMapEntityIDs, CEntitySelect::SetEntitySelectPassEnabled);
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
	bk.AddToggle("Show Shadow Map VS"               , &m_showShadowMapVS);
	bk.AddToggle("Show Shadow Map Mini"             , &m_showShadowMapMini);
	bk.AddToggle("Show Info"                        , &m_showInfo);
	bk.AddToggle("Wireframe"                        , &m_wireframe);
#if __DEV
	bk.AddSeparator();
	bk.AddToggle("Clear Every Frame"                , &m_clearEveryFrame);
	bk.AddSlider("Clear Value"                      , &m_clearValue, 0.0f, 1.0f, 1.0f/64.0f);
#endif // __DEV
	bk.AddToggle("Apply Shadow Evening clamp"       , &gCascadeShadows.m_shadowDirectionApplyClamp);
	bk.AddSlider("Shadow Evening Clamp Value"       , &m_shadowDirectionClampValue, -1.0f, 0.0f, 1.0f/1024.0f);
	bk.AddToggle("Num Cascades Override Settings"   , &m_numCascadesOverrideSettings);
	bk.AddSlider("Num Cascades"                     , &gCascadeShadows.m_numCascades, 1, 4, 1);
	bk.AddToggle("Distance Multiplier Enabled"      , &m_distanceMultiplierEnabled);
	bk.AddToggle("Distance Mult Override Settings"  , &m_distanceMultiplierOverrideSettings);
	bk.AddSlider("Distance Multiplier"              , &gCascadeShadows.m_distanceMultiplier, 0.01f, 16.0f, 1.0f/64.0f);

	bk.AddSeparator();

	bk.AddText("Cas Shad Cas Total", g_ShadowRevealTime_cascades[4].GetTimeAvr());
	bk.AddText("Cas Shad Cas 0", g_ShadowRevealTime_cascades[0].GetTimeAvr());
	bk.AddText("Cas Shad Cas 1", g_ShadowRevealTime_cascades[1].GetTimeAvr());
	bk.AddText("Cas Shad Cas 2", g_ShadowRevealTime_cascades[2].GetTimeAvr());
	bk.AddText("Cas Shad Cas 3", g_ShadowRevealTime_cascades[3].GetTimeAvr());
	bk.AddText("Cas Shad Rev Total", g_ShadowRevealTime_Total.GetTimeAvr());
	bk.AddText("Cas Shad Rev Smooth", g_ShadowRevealTime_SmoothStep.GetTimeAvr());
	bk.AddText("Cas Shad Rev Cloud", g_ShadowRevealTime_Cloud.GetTimeAvr());
	bk.AddText("Cas Shad Rev Edge", g_ShadowRevealTime_Edge.GetTimeAvr());
	bk.AddText("Cas Shad Rev Mask Total", g_ShadowRevealTime_mask_total.GetTimeAvr());
	bk.AddText("Cas Shad Rev Mask MS0", g_ShadowRevealTime_mask_reveal_MS0.GetTimeAvr());
	bk.AddText("Cas Shad Rev Mask MS", g_ShadowRevealTime_mask_reveal_MS.GetTimeAvr());

	bk.AddSeparator();

	for (int cascadeIndex = 0; cascadeIndex < CASCADE_SHADOWS_COUNT; cascadeIndex++)
	{
		bk.AddToggle(atVarString("Enable Cascade[%d]", cascadeIndex), &m_enableCascade[cascadeIndex]);
	}

	bk.AddToggle("Dynamic depth updated distribution", &g_UseDynamicDistribution);
	bk.AddSeparator();

#if __DEV && __PS3
	if (0) // not sure if this is useful ..
	{
#if HAS_RENDER_MODE_SHADOWS
		const char* _UIStrings_RenderListDrawMode[] =
		{
			"USE_FORWARD_LIGHTING",
			"USE_SHADOWS",
			"USE_SKINNED_SHADOWS",
		};
		bk.AddCombo ("Render List Draw Mode (non-EDGE)", &m_renderListDrawModeNonEdge, NELEM(_UIStrings_RenderListDrawMode), _UIStrings_RenderListDrawMode);
		bk.AddCombo ("Render List Draw Mode (EDGE)"    , &m_renderListDrawModeEdge   , NELEM(_UIStrings_RenderListDrawMode), _UIStrings_RenderListDrawMode);
#endif // HAS_RENDER_MODE_SHADOWS

		extern u32 s_GlobalRenderListFlags;
		const  u32 _RENDER_FILTER_DISABLE_NON_EDGE_SHADOWS = 0x0040000;
		const  u32 _RENDER_FILTER_DISABLE_EDGE_SHADOWS     = 0x0080000;

		bk.AddToggle("Disable EDGE shadows"    , &s_GlobalRenderListFlags, _RENDER_FILTER_DISABLE_EDGE_SHADOWS);
		bk.AddToggle("Disable non-EDGE shadows", &s_GlobalRenderListFlags, _RENDER_FILTER_DISABLE_NON_EDGE_SHADOWS);

		bk.AddSeparator();
	}

	bk.PushGroup("EDGE Debugging Flags", false);
	{
		bk.AddToggle("Affect All Renderphases", &m_edgeDebuggingFlags_global);

		bk.AddSeparator();

		bk.AddToggle("EDGE Clip Planes (frustum LRTB)"        , &m_edgeDebuggingClipPlanes_frustum);
		bk.AddToggle("EDGE Clip Planes (extended clip planes)", &m_edgeDebuggingClipPlanes_extended);
#if ENABLE_EDGE_CULL_CLIPPED_TRIANGLES
		bk.AddToggle("EDGE Clip Planes (cull triangles)"      , &m_edgeDebuggingClipPlanes_triangles);
#endif // ENABLE_EDGE_CULL_CLIPPED_TRIANGLES

		bk.AddSeparator();
		bk.AddSeparator();

		bk.AddToggle("EDGE CODEPATH - spuModel::Draw"              , &m_spuModelDraw);
		bk.AddToggle("EDGE CODEPATH - spuModel::DrawSkinned"       , &m_spuModelDrawSkinned);
		bk.AddToggle("EDGE CODEPATH - spuGeometryQB::Draw"         , &m_spuGeometryQBDraw);
		bk.AddToggle("EDGE CODEPATH - grmGeometryEdge__Draw"       , &m_grmGeometryEdgeDraw);
		bk.AddToggle("EDGE CODEPATH - grmGeometryEdge__DrawSkinned", &m_grmGeometryEdgeDrawSkinned);

		bk.AddSeparator();

#if __PS3
		bk.AddToggle("EDGE Clip Planes"                                  , &m_edgeClipPlanes);
		bk.AddToggle("EDGE Culling"                                      , &m_edgeCulling);
#if CSM_EDGE_OCCLUDER_QUAD_EXCLUSION
		bk.AddToggle("EDGE Occluder Quad Exclusion Enabled"              , &m_edgeOccluderQuadExclusionEnabled);
		bk.AddToggle("EDGE Occluder Quad Exclusion Extended"             , &m_edgeOccluderQuadExclusionExtended);
#endif // CSM_EDGE_OCCLUDER_QUAD_EXCLUSION
#if ENABLE_EDGE_CULL_DEBUGGING
		bk.AddToggle("EDGE Cull - Affect All Renderphases"               , &m_edgeCullDebugFlagsAllRenderphases);
		bk.AddToggle("EDGE Cull - EDGE_CULL_DEBUG_ENABLED"               , &m_edgeCullDebugFlags, EDGE_CULL_DEBUG_ENABLED               );
		bk.AddToggle("EDGE Cull - EDGE_CULL_DEBUG_OUTSIDE_FRUSTUM"       , &m_edgeCullDebugFlags, EDGE_CULL_DEBUG_OUTSIDE_FRUSTUM       );
		bk.AddToggle("EDGE Cull - EDGE_CULL_DEBUG_OUTSIDE_FRUSTUM_INVERT", &m_edgeCullDebugFlags, EDGE_CULL_DEBUG_OUTSIDE_FRUSTUM_INVERT);
		bk.AddToggle("EDGE Cull - EDGE_CULL_DEBUG_NOPIXEL"               , &m_edgeCullDebugFlags, EDGE_CULL_DEBUG_NOPIXEL               );
		bk.AddToggle("EDGE Cull - EDGE_CULL_DEBUG_NOPIXEL_DIAGONAL"      , &m_edgeCullDebugFlags, EDGE_CULL_DEBUG_NOPIXEL_DIAGONAL      );
		bk.AddToggle("EDGE Cull - EDGE_CULL_DEBUG_NOPIXEL_INVERT"        , &m_edgeCullDebugFlags, EDGE_CULL_DEBUG_NOPIXEL_INVERT        );
		bk.AddToggle("EDGE Cull - EDGE_CULL_DEBUG_BACKFACING"            , &m_edgeCullDebugFlags, EDGE_CULL_DEBUG_BACKFACING            );
		bk.AddToggle("EDGE Cull - EDGE_CULL_DEBUG_BACKFACING_INVERT"     , &m_edgeCullDebugFlags, EDGE_CULL_DEBUG_BACKFACING_INVERT     );
		bk.AddToggle("EDGE Cull - EDGE_CULL_DEBUG_QUANT_XY_IN_W"         , &m_edgeCullDebugFlags, EDGE_CULL_DEBUG_QUANT_XY_IN_W         );
	//	bk.AddToggle("EDGE Cull - EDGE_CULL_DEBUG_ALLOW_MSAA_2X"         , &m_edgeCullDebugFlags, EDGE_CULL_DEBUG_ALLOW_MSAA_2X         );
#if ENABLE_SWITCHABLE_EDGE_INTRINSICS_CODE
		bk.AddToggle("EDGE Cull - EDGE_CULL_DEBUG_INTRINSICS_CODE"       , &m_edgeCullDebugFlags, EDGE_CULL_DEBUG_INTRINSICS_CODE       );
#endif // ENABLE_SWITCHABLE_EDGE_INTRINSICS_CODE
		bk.AddToggle("EDGE Cull - EDGE_SKIN"                             , &m_edgeCullDebugFlags, EDGE_SKIN                             );
		bk.AddToggle("EDGE Cull - EDGE_BLENDSHAPE"                       , &m_edgeCullDebugFlags, EDGE_BLENDSHAPE                       );
#endif // ENABLE_EDGE_CULL_DEBUGGING
#endif // __PS3
	}
	bk.PopGroup();
#endif // __DEV && __PS3

	bk.AddSeparator();

	bk.PushGroup("Debug Draw", false);
	{
		bk.AddToggle("Draw Cascade Debug Geom Ghosts", &m_drawFlags.m_drawCascadeGhosts);
		bk.AddToggle("Draw Cascade Debug Geom Edges" , &m_drawFlags.m_drawCascadeEdges);
	//	bk.AddToggle("Draw Cascade Debug Geom Bounds", &m_drawFlags.m_drawCascadeBounds);
		bk.AddToggle("Draw Silhouette Polygon"       , &m_drawFlags.m_drawSilhouettePolygon);
		bk.AddToggle("Draw Selected Entity AABB"     , &m_drawFlags.m_drawSelectedEntityAABB);
		bk.AddToggle("Draw Worldspace"               , &m_drawFlags.m_drawWorldspace);
		bk.AddToggle("Draw CSM Debug Shader"         , &m_drawShadowDebugShader);

		bk.AddSeparator();

		bk.AddSlider("Debug Opacity"        , &m_drawOpacity              , 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddSlider("Debug Shadow Opacity" , &m_shadowDebugOpacity       , 0.0f, 2.0f, 1.0f/32.0f);
		bk.AddSlider("Debug Shadow N dot L" , &m_shadowDebugNdotL         , 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddSlider("Debug Saturation"     , &m_shadowDebugSaturation    , 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddSlider("Debug Ring Opacity"   , &m_shadowDebugRingOpacity   , 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddSlider("Debug Grid Opacity"   , &m_shadowDebugGridOpacity   , 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddSlider("Debug Grid Divisions1", &m_shadowDebugGridDivisions1, 0, 6, 1);
		bk.AddSlider("Debug Grid Divisions2", &m_shadowDebugGridDivisions2, 0, 6, 1);
	}
	bk.PopGroup();

	bk.PushGroup("Camera Lock", false);
	{
		m_cameraLock.InitWidgets(bk);
	}
	bk.PopGroup();

	bk.PushGroup("Scan", false);
	{
		bk.AddButton("Show Rendering Stats", CascadeShadowsShowRenderingStats_button<0>);
		bk.AddButton("Show Context Stats"  , CascadeShadowsShowRenderingStats_button<1>);

		bk.AddSeparator();

		bk.AddToggle("Cull Flags - RENDER_EXTERIOR" , &m_cullFlags_RENDER_EXTERIOR );
		bk.AddToggle("Cull Flags - RENDER_INTERIOR" , &m_cullFlags_RENDER_INTERIOR );
		bk.AddToggle("Cull Flags - TRAVERSE_PORTALS", &m_cullFlags_TRAVERSE_PORTALS);
		bk.AddToggle("Cull Flags - ENABLE_OCCLUSION", &m_cullFlags_ENABLE_OCCLUSION);

		bk.AddSeparator();

		// alias some useful bits in g_scanDebugFlags
		bk.AddToggle("Force Enable Shadow Occlusion"    , &fwScan::ms_forceEnableShadowOcclusion);
		bk.AddToggle("Force Disable Shadow Occlusion"   , &fwScan::ms_forceDisableShadowOcclusion);
		bk.AddToggle("Use Exterior Restricted Frustum"  , &g_scanDebugFlags, SCAN_DEBUG_USE_EXTERIOR_RESTRICTED_FRUSTUM);
		bk.AddToggle("Enable Screen Size Check"			, &gCascadeShadows.m_enableScreenSizeCheck);
		bk.AddSeparator();

#if CASCADE_SHADOWS_SCAN_DEBUG
		bk.AddToggle("Clip Silhouette Plane Optimiser"  , &m_scanDebug.m_flags_, BIT(CSM_SCAN_DEBUG_FLAG_CLIP_SILHOUETTE_PLANE_OPTIMISER  ));
		bk.AddToggle("Clip Silhouette Planes A"         , &m_scanDebug.m_flags_, BIT(CSM_SCAN_DEBUG_FLAG_CLIP_SILHOUETTE_PLANES_A         ));
		bk.AddToggle("Clip Silhouette Planes B"         , &m_scanDebug.m_flags_, BIT(CSM_SCAN_DEBUG_FLAG_CLIP_SILHOUETTE_PLANES_B         ));
		bk.AddToggle("Clip Silhouette Ground Planes"    , &m_scanDebug.m_flags_, BIT(CSM_SCAN_DEBUG_FLAG_CLIP_SILHOUETTE_GROUND_PLANES    ));
		bk.AddToggle("Clip Silhouette Shadow Portal"    , &m_scanDebug.m_flags_, BIT(CSM_SCAN_DEBUG_FLAG_CLIP_SILHOUETTE_SHADOW_PORTAL    ));
		bk.AddToggle("Clip Backfacing Planes"           , &m_scanDebug.m_flags_, BIT(CSM_SCAN_DEBUG_FLAG_CLIP_BACKFACING_PLANES           ));

		bk.AddSeparator();

		bk.AddToggle("Test Cascade Box"                 , &m_scanDebug.m_flags_, BIT(CSM_SCAN_DEBUG_FLAG_TEST_CASCADE_BOX                 ));
		bk.AddToggle("Test Cascade Box 1"               , &m_scanDebug.m_flags_, BIT(CSM_SCAN_DEBUG_FLAG_TEST_CASCADE_BOX_1               ));
		bk.AddToggle("Test Cascade Box 1 Exclusions"    , &m_scanDebug.m_flags_, BIT(CSM_SCAN_DEBUG_FLAG_TEST_CASCADE_BOX_1_EX            ));
		bk.AddToggle("Test Cascade Box 2"               , &m_scanDebug.m_flags_, BIT(CSM_SCAN_DEBUG_FLAG_TEST_CASCADE_BOX_2               ));
		bk.AddToggle("Test Cascade Box Exact Thresholds", &m_scanDebug.m_flags_, BIT(CSM_SCAN_DEBUG_FLAG_TEST_CASCADE_BOX_EXACT_THRESHOLDS));
		bk.AddSlider("Box Exact Threshold"              , &m_scanBoxExactThreshold, 0.00001f, 512.0f, 1.0f);
		bk.AddSlider("Box Visibility Scale"             , &m_scanBoxVisibilityScale, 0.0f, 1.0f, 1.0f/256.0f);
		bk.AddToggle("Force No Backfacing Planes When Underwater", &m_scanForceNoBackfacingPlanesWhenUnderwater);
		bk.AddToggle("Test Cascade Sphere Thresholds"   , &m_scanDebug.m_flags_, BIT(CSM_SCAN_DEBUG_FLAG_TEST_CASCADE_SPHERE_THRESHOLDS   ));
		bk.AddSlider("Sphere Threshold"                 , &m_scanSphereThreshold, 0.0f, 500.0f, 1.0f/512.0f);
		bk.AddVector("Sphere Threshold Tweaks"          , &m_scanSphereThresholdTweaks, 0.0f, 4.0f, 1.0f/512.0f);
		bk.AddSlider("FOV Threshold"                    , &m_scanCamFOVThreshold, 0.0f, 10.0f, 1.0f/64.0f);
		bk.AddToggle("Test Using Reference Code"        , &m_scanDebug.m_flags_, BIT(CSM_SCAN_DEBUG_FLAG_TEST_USING_REFERENCE_CODE        ));

		bk.AddSeparator();

		bk.AddToggle("Cull Cascades By LOD Distance"          , &m_scanDebug.m_flags_, BIT(CSM_SCAN_DEBUG_FLAG_CULL_CASCADES_BY_LOD_DISTANCE       ));
		bk.AddToggle("Override Cascades LOD Distance Scale"   , &m_scanDebug.m_flags_, BIT(CSM_SCAN_DEBUG_FLAG_OVERRIDE_CASCADES_LOD_DISTANCE_SCALE));
		bk.AddSlider("LOD Distance Scale Override - Cascade 1", &m_scanDebug.m_cascadeLodDistanceScale[1], 0.01f, 10000.0f, 0.01f);
		bk.AddSlider("LOD Distance Scale Override - Cascade 2", &m_scanDebug.m_cascadeLodDistanceScale[2], 0.01f, 10000.0f, 0.01f);
		bk.AddSlider("LOD Distance Scale Override - Cascade 3", &m_scanDebug.m_cascadeLodDistanceScale[3], 0.01f, 10000.0f, 0.01f);

		for (int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
		{
			bk.AddToggle(atVarString("Force Cascade[%d]", i), &m_scanDebug.m_shadowVisFlagsForce, BIT(i));
		}

		for (int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
		{
			bk.AddToggle(atVarString("Allow Cascade[%d]", i), &m_scanDebug.m_shadowVisFlagsAllow, BIT(i));
		}
#endif // CASCADE_SHADOWS_SCAN_DEBUG

		for (int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
		{
			bk.AddToggle(atVarString("Grass - Allow Cascade[%d]", i), &sGrassCascadeMask, BIT(i));
		}
		bk.AddSlider("Grass Last Cascade End Threshold", &CSMStatic::sGrassCullLastCascadeEndThreshold, 0.0f, 1000.0f, 0.1f);
		bk.AddToggle("Always Allow Grass Culling", &CSMStatic::sAlwaysAllowGrassCulling);

#if __DEV
		bk.AddSeparator();

		bk.AddToggle("Scan Viewer Portal Enabled", &m_scanViewerPortalEnabled);
		bk.AddVector("Scan Viewer Portal"        , &m_scanViewerPortal, -1.0f, 1.0f, 1.0f/128.0f);
#endif // __DEV

		bk.AddSeparator();

		bk.AddToggle("Force Non Shadow Casting Entities In Shadow", &g_scanDebugFlagsExtended, SCAN_DEBUG_EXT_FORCE_NON_SHADOW_CASTING_ENTITIES_IN_SHADOW);
		bk.AddToggle("Hide Non Shadow Casting Entities"           , &g_scanDebugFlags, SCAN_DEBUG_HIDE_NON_SHADOW_CASTING_ENTITIES);
		bk.AddToggle("Hide Non Shadow Proxy Entities In Shadow"   , &g_scanDebugFlags, SCAN_DEBUG_HIDE_NON_SHADOW_PROXIES_IN_SHADOW);
	}
	bk.PopGroup();

	bk.PushGroup("Cascade Bounds", false);
	{
		bk.AddToggle("Cascade Bounds Snap"                   , &m_cascadeBoundsSnap);
		bk.AddToggle("Cascade Bounds Snap High Altitude FOV" , &m_cascadeBoundsSnapHighAltitudeFOV);
		bk.AddSlider("Cascade Bounds HFOV"                   , &m_cascadeBoundsHFOV, 0.0f, 150.0f, 1.0f/32.0f);
		bk.AddSlider("Cascade Bounds VFOV"                   , &m_cascadeBoundsVFOV, 0.0f, 150.0f, 1.0f/32.0f);
		bk.AddToggle("Cascade Bounds Force Overlap"          , &m_cascadeBoundsForceOverlap);
		bk.AddToggle("Cascade Bounds Split Z Scale"          , &m_cascadeBoundsSplitZScale);
		bk.AddToggle("Cascade Bounds Isolate Selected Entity", &m_cascadeBoundsIsolateSelectedEntity);
		bk.AddSlider("Cascade Bounds Isolate Offset X"       , &m_cascadeBoundsIsolateOffsetX, -1.0f, 1.0f, 1.0f/128.0f);
		bk.AddSlider("Cascade Bounds Isolate Offset Y"       , &m_cascadeBoundsIsolateOffsetY, -1.0f, 1.0f, 1.0f/128.0f);
		bk.AddSlider("Cascade Bounds Isolate Zoom"           , &m_cascadeBoundsIsolateZoom, 1.0f, 16.0f, 1.0f/32.0f);

		bk.AddToggle("Cascade Bounds Override Timecycle"	 , &m_cascadeBoundsOverrideTimeCycle);

		bk.AddSeparator();
	}
	bk.PopGroup();

	bk.PushGroup("Cascade Split Z", false);
	{
		// split Z
		{
			CMultiSlider* ms = rage_new CMultiSlider(CSM_DEFAULT_SPLIT_Z_MIN, VIEWPORT_DEFAULT_FAR, 1.0f/32.0f, 1.0f/4.0f);

			for (int i = 0; i <= CASCADE_SHADOWS_COUNT; i++)
			{
				ms->AddSlider(bk, "Z[%d]", i, &m_cascadeSplitZ[i]);
			}

			for (int i = 0; i <= CASCADE_SHADOWS_COUNT; i++)
			{
				bk.AddText("Final Z", &m_currentCascadeSplitZ[i]);
			}

			bk.AddToggle("Split Z Neg Relative", &ms->m_negRelative);
			bk.AddToggle("Split Z Pos Relative", &ms->m_posRelative);
			bk.AddSlider("Split Z Min Spacing" , &ms->m_spacing, 0.0f, 2.0f, 1.0f/64.0f);
			bk.AddSlider("Split Z Exp Weight"  , &m_cascadeSplitZExpWeight, 0.0f, 1.0f, 1.0f/512.0f);
		}

		bk.AddSeparator();

		// world height
		{
			m_worldHeightUpdate = CGameLogic::IsRunningGTA5Map() ? (CSM_DEFAULT_WORLD_HEIGHT_UPDATE != 0) : false; // init this here so the default value is correct
			bk.AddToggle("World Height Update", &m_worldHeightUpdate);

			CMultiSlider* ms = rage_new CMultiSlider(-50.0f, CSM_DEFAULT_WORLD_HEIGHT_MAX, 1.0f/32.0f, 1.0f/4.0f);

			ms->AddSlider(bk, "World Height Min", 0, &m_worldHeightAdj[0]);
			ms->AddSlider(bk, "World Height Max", 1, &m_worldHeightAdj[1]);

			bk.AddButton("World Height Reset", CascadeShadowsWorldHeightReset_button);
		}

		bk.AddSeparator();

		// receiver height
		{
			m_recvrHeightUpdate = CGameLogic::IsRunningGTA5Map() ? (CSM_DEFAULT_RECVR_HEIGHT_UPDATE != 0) : false;
			bk.AddToggle("Receiver Height Update", &m_recvrHeightUpdate);
#if CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
			bk.AddToggle("Receiver Height Track Depth", &m_recvrHeightTrackDepthHistogram);
#endif // CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK

			CMultiSlider* ms = rage_new CMultiSlider(-50.0f, 800.0f, 1.0f/32.0f, 1.0f/4.0f);

			ms->AddSlider(bk, "Receiver Height Min", 0, &m_recvrHeightAdj[0]);
			ms->AddSlider(bk, "Receiver Height Max", 1, &m_recvrHeightAdj[1]);

			bk.AddSlider("Receiver Height Underwater"      , &m_recvrHeightUnderwater     , -256.0f, 0.0f, 1.0f/32.0f);
			bk.AddSlider("Receiver Height Underwater Track", &m_recvrHeightUnderwaterTrack, 0.0f, 256.0f, 1.0f/32.0f);
			bk.AddButton("Receiver Height Reset"           , CascadeShadowsRecvrHeightReset_button);
		}
	}
	bk.PopGroup();

	bk.PushGroup("Shadow Transform", false);
	{
		bk.AddCombo ("Shadow Align Mode"             , &m_shadowAlignMode, CSM_ALIGN_COUNT, GetUIStrings_eCSMDebugShadowAlignMode());
		bk.AddToggle("Shadow Direction Do Not Update", &m_shadowDirectionDoNotUpdate);
		bk.AddToggle("Shadow Direction Enabled"      , &m_shadowDirectionEnabled);
		bk.AddVector("Shadow Direction"              , &m_shadowDirection, -1.0f, 1.0f, 1.0f/32.0f);
		bk.AddAngle ("Shadow Rotation"               , &m_shadowRotation, bkAngleType::DEGREES, 0.0f, 360.0f);
	}
	bk.PopGroup();

	bk.PushGroup("Shadow Rendering", false);
	{
#if RSG_PC
		bk.AddToggle("Depth/slope res. dependant",	&m_modulateDepthSlopEnabled);
		bk.AddToggle("Rastercache enabled",			&m_rasterStateCacheEnabled);
#endif
		bk.AddToggle("Depth Bias Enabled",			&m_shadowDepthBiasEnabled);
		bk.AddSlider("Depth Bias"        ,			&m_shadowDepthBias,				-4.0f, 4.0f, 1.0f/512.0f);
		bk.AddVector("Depth Bias Tweaks" ,			&m_shadowDepthBiasTweaks,		0.0f, 100.0f, 1.0f/64.0f);
		bk.AddToggle("Slope Bias Enabled",			&m_shadowSlopeBiasEnabled);
		bk.AddSlider("Slope Bias"        ,			&m_shadowSlopeBias,			    -100.0f, 100.0f, 1.0f/512.0f);
		bk.AddSlider("Slope Bias RPDB"        ,		&m_shadowSlopeBiasRPDB,		    -100.0f, 100.0f, 1.0f/512.0f);
		bk.AddVector("Slope Bias Tweaks" ,			&m_shadowSlopeBiasTweaks,		0.0f, 100.0f, 1.0f/64.0f);
		bk.AddToggle("Depth Bias Clamp Enabled",	&m_shadowDepthBiasClampEnabled);
		bk.AddSlider("Depth Bias Clamp"        ,	&m_shadowDepthBiasClamp,		0.0f, 0.01f, 1.0f/1024.0f);
		bk.AddVector("Depth Bias Clamp Tweaks" ,	&m_shadowDepthBiasClampTweaks,	0.0f, 100.0f, 1.0f/64.0f);

		bk.AddSeparator();

		bk.AddSlider("Cutout Alpha Ref"        , &m_cutoutAlphaRef, 0.f, 1.f, 0.01f);
		bk.AddToggle("Cutout Alpha to Coverage", &m_cutoutAlphaToCoverage);
		bk.AddToggle("Render Fading Enabled"   , &m_renderFadingEnabled);
		bk.AddToggle("Render Hair Enabled"     , &m_renderHairEnabled);

		bk.AddSeparator();

#if SHADOW_ENABLE_ASYNC_CLEAR
		bk.AddToggle("Use Async Clear",   	&g_bUseAsyncClear);
		bk.AddToggle("Async Clear Wait" ,	&m_shadowAsyncClearWait);
#endif
	}
	bk.PopGroup();

	bk.PushGroup("Shadow Sampling", false);
	{
		bk.AddToggle("Normal Offset Enabled", &m_shadowNormalOffsetEnabled);
		bk.AddSlider("Normal Offset"        , &m_shadowNormalOffset, -50.0f, 50.0f, 1.0f/512.0f);
	//	bk.AddVector("Normal Offset Tweaks" , &m_shadowNormalOffsetTweaks, 0.0f, 16.0f, 1.0f/64.0f);

		bk.AddSeparator();

		bk.AddCombo ("Sample Type"            , &gCascadeShadows.m_scriptShadowSampleType, CSM_ST_COUNT, GetUIStrings_eCSMSampleType());
		bk.AddToggle("Dither World (NOT USED)", &m_ditherWorld_NOT_USED);
		bk.AddSlider("Dither Scale (NOT USED)", &m_ditherScale_NOT_USED , 0.0f, 4.0f, 1.0f/32.0f);
		bk.AddSlider("Dither Radius"          , &m_ditherRadius, 0.0f, 4.0f, 1.0f/32.0f);
		bk.AddVector("Dither Radius Tweaks"   , &m_ditherRadiusTweaks, 0.0f, 4.0f, 1.0f/32.0f);

		bk.AddSeparator();

		bk.AddSlider("Shadow Fade Start", &m_shadowFadeStart, 0.0f, 1.0f, 1.0f/32.0f);

		bk.AddSeparator();
		bk.AddToggle("Cloud Shadows Override Settings", &m_cloudTextureOverrideSettings);
		bk.AddSlider("Fog Shadow Amount"              , &gCascadeShadows.m_cloudTextureSettings.m_fogShadowAmount, 0.0f, 1.0f, 1.0f/64.0f);
		bk.AddSlider("Fog Shadow Falloff"             , &gCascadeShadows.m_cloudTextureSettings.m_fogShadowFalloff, 0.0f, 1024.0f, 1.0f/64.0f);
		bk.AddSlider("Fog Shadow Base Height"         , &gCascadeShadows.m_cloudTextureSettings.m_fogShadowBaseHeight, -1024.0f, 1024.0f, 1.0f/64.0f);
	}
	bk.PopGroup();

#if CASCADE_SHADOWS_STENCIL_REVEAL
	bk.PushGroup("Shadow Stencil Reveal", false);
	{
#if __DEV && __XENON
		bk.AddToggle("Post Render Copy Depth"  , &m_shadowPostRenderCopyDepth);
		bk.AddToggle("Post Render Restore Hi-Z", &m_shadowPostRenderRestoreHiZ);
#endif // __DEV && __XENON
		bk.AddToggle("Stencil Reveal Enabled"       , &m_shadowStencilRevealEnabled);
		bk.AddToggle("Stencil Reveal Use Hi-Stencil", &m_shadowStencilRevealUseHiStencil);
		bk.AddToggle("Stencil Reveal Debug"         , &m_shadowStencilRevealDebug);
		bk.AddToggle("Stencil Reveal Wireframe"     , &m_shadowStencilRevealWireframe);
		bk.AddSlider("Stencil Reveal Count"         , &m_shadowStencilRevealCount, 0, CASCADE_SHADOWS_COUNT, 1);
	}
	bk.PopGroup();
#endif // CASCADE_SHADOWS_STENCIL_REVEAL

	bk.PushGroup("Shadow Reveal", false);
	{
		bk.AddToggle("Shadow Reveal Enabled"    , &m_shadowRevealEnabled);
#if USE_AMD_SHADOW_LIB
		// I would put the AMD CHS shadows in the "Reveal Type" pop-up.  But an addition to the enum breaks some assumption about the 
		// number of shadow techniques that I cannot figure out - see CSM_ST_COUNT, etc.  It crashes.
		bk.AddToggle("AMD Contact Hardening" , &gCascadeShadows.m_enableAMDShadows);
#endif // USE_AMD_SHADOW_LIB
#if USE_NV_SHADOW_LIB
		// I would put the Nvidia CHS shadows in the "Reveal Type" pop-up.  But an addition to the enum breaks some assumption about the 
		// number of shadow techniques that I cannot figure out - see CSM_ST_COUNT, etc.  It crashes.
		bk.AddToggle("Nvidia Contact Hardening" , &gCascadeShadows.m_enableNvidiaShadows);
#endif // USE_NV_SHADOW_LIB
		bk.AddToggle("Soft Shadow Enabled"      , &gCascadeShadows.m_softShadowEnabled);		// Duplicated from "Soft Shadows"
		bk.AddCombo ("Shadow Reveal Sample Type", &gCascadeShadows.m_scriptShadowSampleType, CSM_ST_COUNT, GetUIStrings_eCSMSampleType());
		bk.AddToggle("Shadow Reveal Override TC", &m_shadowRevealOverrideTimeCycle);
		bk.AddToggle("Shadow Reveal Use Stencil", &m_shadowRevealWithStencil);
	}
	bk.PopGroup();

#if __DEV
	bk.PushGroup("Alternate Settings", false);
	{
		bk.AddToggle("Alt Settings Enabled"    , &m_altSettings_enabled);
		bk.AddToggle("Alt World Height Enabled", &m_altSettings_worldHeightEnabled);
		bk.AddSlider("Alt World Height[0]"     , &m_altSettings_worldHeight[0], -100.0f, 2000.0f, 1.0f/64);
		bk.AddSlider("Alt World Height[1]"     , &m_altSettings_worldHeight[1], -100.0f, 2000.0f, 1.0f/64);
		bk.AddToggle("Alt Depth Bias Enabled"  , &m_altSettings_shadowDepthBiasEnabled);
		bk.AddSlider("Alt Depth Bias"          , &m_altSettings_shadowDepthBias, -4.0f, 4.0f, 1.0f/512.0f);
		bk.AddVector("Alt Depth Bias Tweaks"   , &m_altSettings_shadowDepthBiasTweaks, 0.0f, 16.0f, 1.0f/64.0f);
		bk.AddToggle("Alt Slope Bias Enabled"  , &m_altSettings_shadowSlopeBiasEnabled);
		bk.AddSlider("Alt Slope Bias"          , &m_altSettings_shadowSlopeBias, -100.0f, 100.0f, 1.0f/512.0f);
		bk.AddVector("Alt Slope Bias Tweaks"   , &m_altSettings_shadowSlopeBiasTweaks, 0.0f, RSG_ORBIS? 64.0f : 16.0f, 1.0f/64.0f);
	}
	bk.PopGroup();
#endif // __DEV

#if CSM_DEPTH_HISTOGRAM
	bk.PushGroup("Depth Histogram", false);
	{
		gDepthHistogram.InitWidgets(bk);
		gDepthHistogram.Init();
	}
	bk.PopGroup();
#endif // CSM_DEPTH_HISTOGRAM

#if __DEV
	bk.PushGroup("Advanced", false);
	{
#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
		bk.AddButton("Count Entity IDs", DumpRenderTargetCountEntityIDs_button);
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
#if __PS3
		bk.AddButton("Dump Render Targets", DumpRenderTargets_button);
		bk.AddSeparator();
#endif // __PS3
		bk.AddButton("Reset Track Function Counter", ResetTrackFunctionCounter_button);
		bk.AddButton("Reset Max Cascade Extent"    , ResetMaxCascadeAABBExtent_button);
	}
	bk.PopGroup();
#endif // __DEV

#if __DEV
	bk.PushGroup("Script", false);
	{
		bk.AddToggle("Allow Script Control", &m_allowScriptControl);
		bk.AddButton("Reset Script Control", ResetScriptControl_button);

		bk.AddSeparator();

		const char* shadowTypeStrings[] =
		{
			"CSM_TYPE_CASCADES",
			"CSM_TYPE_QUADRANT",
			"CSM_TYPE_HIGHRES_SINGLE"
		};
		CompileTimeAssert(NELEM(shadowTypeStrings) == CSM_TYPE_COUNT);

		bk.AddCombo("m_scriptShadowType", &gCascadeShadows.m_scriptShadowType, NELEM(shadowTypeStrings), shadowTypeStrings);

#if CSM_QUADRANT_TRACK_ADJ
		bk.AddSlider("m_scriptQuadrantTrackExpand1"    , &gCascadeShadows.m_scriptQuadrantTrackExpand1, -1.0f, 1.0f, 1.0f/256.0f);
		bk.AddSlider("m_scriptQuadrantTrackExpand2"    , &gCascadeShadows.m_scriptQuadrantTrackExpand2, -1.0f, 1.0f, 1.0f/256.0f);
		bk.AddSlider("m_scriptQuadrantTrackSpread"     , &gCascadeShadows.m_scriptQuadrantTrackSpread, -1.0f, 1.0f, 1.0f/256.0f);
		bk.AddVector("m_scriptQuadrantTrackScale"      , &gCascadeShadows.m_scriptQuadrantTrackScale, 0.1f, 10.0f, 1.0f/256.0f);
		bk.AddVector("m_scriptQuadrantTrackOffset"     , &gCascadeShadows.m_scriptQuadrantTrackOffset, -1000.0f, 1000.0f, 1.0f/4.0f);
		bk.AddAngle ("Shadow Rotation"                 , &m_shadowRotation, bkAngleType::DEGREES, 0.0f, 360.0f);
#endif // CSM_QUADRANT_TRACK_ADJ
		bk.AddToggle("m_scriptQuadrantTrackFlyCamera"  , &gCascadeShadows.m_scriptQuadrantTrackFlyCamera);
		bk.AddToggle("m_scriptQuadrantTrackForceVisAll", &gCascadeShadows.m_scriptQuadrantTrackForceVisAll);

		for (int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
		{
			bk.AddToggle(atVarString("m_scriptCascadeBoundsEnable[%d]", i).c_str(), &gCascadeShadows.m_scriptCascadeBoundsEnable[i]);
		}

		for (int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
		{
			bk.AddVector(atVarString("m_scriptCascadeBoundsSphere[%d]", i).c_str(), &gCascadeShadows.m_scriptCascadeBoundsSphere[i], -99999.0f, 99999.0f, 1.0f/32.0f);
		}

		for (int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
		{
			bk.AddSlider(atVarString("m_scriptCascadeBoundsLerpFromCustsceneDuration[%d]", i).c_str(), &gCascadeShadows.m_scriptCascadeBoundsLerpFromCustsceneDuration[i], 0.0f, 99999.0f, 1.0f/32.0f);
		}

		for (int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
		{
			bk.AddToggle(atVarString("m_scriptCascadeBoundsLerpFromCustscene[%d]", i).c_str(), &gCascadeShadows.m_scriptCascadeBoundsLerpFromCustscene[i]);
		}

		bk.AddSlider("m_scriptBoundPosition"       , &gCascadeShadows.m_scriptBoundPosition, -200.0f, 200.0f, 1.0f/256.0f);
		bk.AddSlider("m_scriptCascadeBoundsTanHFOV", &gCascadeShadows.m_scriptCascadeBoundsTanHFOV, -9999.0f, 9999.0f, 1.0f/256.0f);
		bk.AddSlider("m_scriptCascadeBoundsTanVFOV", &gCascadeShadows.m_scriptCascadeBoundsTanVFOV, -9999.0f, 9999.0f, 1.0f/256.0f);
		bk.AddSlider("m_scriptCascadeBoundsScale"  , &gCascadeShadows.m_scriptCascadeBoundsScale, 0.0f, 10.0f, 1.0f/32.0f);
		bk.AddSlider("m_scriptSplitZExpWeight"     , &gCascadeShadows.m_scriptSplitZExpWeight, -1.0f, 1.0f, 1.0f/128.0f);
		bk.AddSlider("m_scriptEntityTrackerScale"  , &gCascadeShadows.m_scriptEntityTrackerScale, 0.0f, 16.0f, 1.0f/32.0f);
		bk.AddToggle("m_scriptWorldHeightUpdate"   , &gCascadeShadows.m_scriptWorldHeightUpdate);
		bk.AddToggle("m_scriptRecvrHeightUpdate"   , &gCascadeShadows.m_scriptRecvrHeightUpdate);
		bk.AddSlider("m_scriptDitherRadiusScale"   , &gCascadeShadows.m_scriptDitherRadiusScale, -9999.0f, 9999.0f, 1.0f/32.0f);
		bk.AddToggle("m_scriptDepthBiasEnabled"    , &gCascadeShadows.m_scriptDepthBiasEnabled);
		bk.AddSlider("m_scriptDepthBias"           , &gCascadeShadows.m_scriptDepthBias, -9999.0f, 9999.0f, 1.0f/256.0f);
		bk.AddToggle("m_scriptSlopeBiasEnabled"    , &gCascadeShadows.m_scriptSlopeBiasEnabled);
		bk.AddSlider("m_scriptSlopeBias"           , &gCascadeShadows.m_scriptSlopeBias, -9999.0f, 9999.0f, 1.0f/256.0f);
		bk.AddCombo ("m_scriptShadowSampleType"    , &gCascadeShadows.m_scriptShadowSampleType, CSM_ST_COUNT, GetUIStrings_eCSMSampleType());
		bk.AddCombo ("m_scriptShadowQualityMode"   , &gCascadeShadows.m_scriptShadowQualityMode, CSM_QM_COUNT, GetUIStrings_eCSMQualityMode());

		bk.AddSeparator();

		bk.AddCombo("Current Shadow Quality Mode", &gCascadeShadows.m_currentShadowQualityMode, CSM_QM_COUNT, GetUIStrings_eCSMQualityMode());
	}
	bk.PopGroup();

	// TEMPORARY -- until there's a decent cutscene interface, we need a way of manipulating script cascade bounds in rag (see BS#770516)
	bk.PushGroup("Script Cascade Manual Control", false);
	{
		bk.AddButton("Toggle Debug Draw", CSMToggleDebugDraw);

		for (int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
		{
			bk.AddToggle(atVarString("Enable Override Cascade %d", i).c_str(), &gCascadeShadows.m_scriptCascadeBoundsEnable[i]);
		}

		for (int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
		{
			bk.AddVector(atVarString("Cascade %d Position", i).c_str(), (Vec3V*)&gCascadeShadows.m_scriptCascadeBoundsSphere[i], -99999.0f, 99999.0f, 1.0f/32.0f);
			bk.AddSlider(atVarString("Cascade %d Radius", i).c_str(), &gCascadeShadows.m_scriptCascadeBoundsSphere[i][3], 0.0f, 9999.0f, 1.0f/32.0f);
		}

		#define ScriptCascadeSyncToCurrentCascade_button(cascadeIndex) \
		class ScriptCascadeSyncToCurrentCascade_button_##cascadeIndex { public: static void func() \
		{ \
			const CCSMState& state = gCascadeShadows.GetUpdateState(); \
			\
			gCascadeShadows.m_scriptCascadeBoundsEnable[cascadeIndex] = true; \
			gCascadeShadows.m_scriptCascadeBoundsSphere[cascadeIndex] = TransformSphere(state.m_shadowToWorld33, state.m_cascadeBoundsSphere[cascadeIndex]); \
		}}; \
		bk.AddButton(atVarString("Lock Cascade %d", cascadeIndex).c_str(), ScriptCascadeSyncToCurrentCascade_button_##cascadeIndex::func)

		#define ScriptCascadeSyncToSelectedEntity_button(cascadeIndex) \
		class ScriptCascadeSyncToSelectedEntity_button_##cascadeIndex { public: static void func() \
		{ \
			spdAABB aabb; \
			\
			if (GetEntityApproxAABB(aabb, (CEntity*)g_PickerManager.GetSelectedEntity())) \
			{ \
				gCascadeShadows.m_scriptCascadeBoundsEnable[cascadeIndex] = true; \
				gCascadeShadows.m_scriptCascadeBoundsSphere[cascadeIndex] = Vec4V(aabb.GetCenter(), Mag(aabb.GetExtent())); \
			} \
		}}; \
		bk.AddButton(atVarString("Sync Cascade %d To Selected Entity", cascadeIndex).c_str(), ScriptCascadeSyncToSelectedEntity_button_##cascadeIndex::func)

		bk.AddSeparator();

		ScriptCascadeSyncToCurrentCascade_button(0);
		ScriptCascadeSyncToCurrentCascade_button(1);
		ScriptCascadeSyncToCurrentCascade_button(2);
		ScriptCascadeSyncToCurrentCascade_button(3);

		bk.AddSeparator();

		ScriptCascadeSyncToSelectedEntity_button(0);
		ScriptCascadeSyncToSelectedEntity_button(1);
		ScriptCascadeSyncToSelectedEntity_button(2);
		ScriptCascadeSyncToSelectedEntity_button(3);

		bk.AddSeparator();

		#undef ScriptCascadeSyncToCurrentCascade_button
		#undef ScriptCascadeSyncToSelectedEntity_button

		class SortCascadesByRadius_button { public: static void func()
		{
			const CCSMState& state = gCascadeShadows.GetUpdateState();

			class CCascadeSortProxy
			{
			public:
				static s32 CompareFunc(CCascadeSortProxy const* a, CCascadeSortProxy const* b)
				{
					if (a->m_cascadeBoundsSphere.GetWf() > b->m_cascadeBoundsSphere.GetWf()) { return +1; }
					if (a->m_cascadeBoundsSphere.GetWf() < b->m_cascadeBoundsSphere.GetWf()) { return -1; }
					return 0;
				}

				int   m_cascadeIndex;
				bool  m_cascadeBoundsEnable;
				Vec4V m_cascadeBoundsSphere;
			};

			CCascadeSortProxy cascades[CASCADE_SHADOWS_COUNT];

			for (int cascadeIndex = 0; cascadeIndex < CASCADE_SHADOWS_COUNT; cascadeIndex++)
			{
				CCascadeSortProxy& cascade = cascades[cascadeIndex];

				cascade.m_cascadeIndex = cascadeIndex;

				if (gCascadeShadows.m_scriptCascadeBoundsEnable[cascadeIndex])
				{
					cascade.m_cascadeBoundsEnable = true;
					cascade.m_cascadeBoundsSphere = gCascadeShadows.m_scriptCascadeBoundsSphere[cascadeIndex];
				}
				else
				{
					cascade.m_cascadeBoundsEnable = false;
					cascade.m_cascadeBoundsSphere = TransformSphere(state.m_shadowToWorld33, state.m_cascadeBoundsSphere[cascadeIndex]);
				}
			}

			qsort(cascades, CASCADE_SHADOWS_COUNT, sizeof(CCascadeSortProxy), (int (*)(const void*, const void*))CCascadeSortProxy::CompareFunc);

			for (int cascadeIndex = 0; cascadeIndex < CASCADE_SHADOWS_COUNT; cascadeIndex++)
			{
				const CCascadeSortProxy& cascade = cascades[cascadeIndex];

				if (cascade.m_cascadeBoundsEnable)
				{
					gCascadeShadows.m_scriptCascadeBoundsEnable[cascadeIndex] = true;
					gCascadeShadows.m_scriptCascadeBoundsSphere[cascadeIndex] = cascade.m_cascadeBoundsSphere;
				}
				else
				{
					gCascadeShadows.m_scriptCascadeBoundsEnable[cascadeIndex] = false;
					gCascadeShadows.m_scriptCascadeBoundsSphere[cascadeIndex] = Vec4V(V_ZERO);
				}
			}
		}};
		bk.AddButton("Sort Cascades By Radius", SortCascadesByRadius_button::func);

		class DumpCascadeScriptCommands_button { public: static void func()
		{
			Displayf("");

			for (int cascadeIndex = 0; cascadeIndex < CASCADE_SHADOWS_COUNT; cascadeIndex++)
			{
				Displayf(
					"CASCADE_SHADOWS_SET_CASCADE_BOUNDS(%d,%s,%f,%f,%f,%f)",
					cascadeIndex,
					gCascadeShadows.m_scriptCascadeBoundsEnable[cascadeIndex] ? "TRUE" : "FALSE",
					VEC4V_ARGS(gCascadeShadows.m_scriptCascadeBoundsSphere[cascadeIndex])
				);
			}
		}};
		bk.AddButton("Dump Cascade Script Commands", DumpCascadeScriptCommands_button::func);
	}
	bk.PopGroup();
#endif // __DEV

#if __DEV
	bk.PushGroup("Render Occluders", false);
	{
		bk.AddToggle("Enable"                     , &m_renderOccluders_enabled);
		bk.AddToggle("Use Two Passes"             , &m_renderOccluders_useTwoPasses);
		bk.AddToggle("Render First Three Cascades", &m_renderOccluders_firstThreeCascades);
		bk.AddToggle("Render Last Cascade"        , &m_renderOccluders_lastCascade);
	}
	bk.PopGroup();
#endif // __DEV

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
	InitWidgets_SoftShadows(bk);
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING

#if SHADOW_ENABLE_ALPHASHADOW
	bk.AddToggle("Alpha Shadows (cables, etc)"		, &gCascadeShadows.m_enableAlphaShadows);
	bk.AddToggle("Disable Alpha Shadows at Night"	, &m_DisableNightAlphaShadows);
#endif // SHADOW_ENABLE_ALPHASHADOW

#if RMPTFX_USE_PARTICLE_SHADOWS
	bk.PushGroup("Particle Shadows", false);
	{
		bk.AddToggle("Enable"                       , &gCascadeShadows.m_enableParticleShadows);
		bk.AddSlider("Max Cascade"                  , &gCascadeShadows.m_maxParticleShadowCascade, 0, CASCADE_SHADOWS_COUNT, 1);
#if PTXDRAW_MULTIPLE_DRAWS_PER_BATCH
		bk.AddToggle("Use Multiple Draw"            , &m_particleShadowsUseMultipleDraw);
#endif // PTXDRAW_MULTIPLE_DRAWS_PER_BATCH
		bk.AddToggle("Disable Particle Shadows at Night"	, &m_DisableNightParticleShadows);
	}
	bk.PopGroup();
#endif // RMPTFX_USE_PARTICLE_SHADOWS
#if USE_AMD_SHADOW_LIB
	bk.PushGroup("AMD Shadows", false);
	{
		bk.AddSlider("Cascade Border"              , &gCascadeShadows.m_amdCascadeRegionBorder, 0.0f, 1.0f, 0.001f);

		bk.AddSlider("Sun Size 0"                  , &gCascadeShadows.m_amdSunWidth[0],     1.0f, 1000.0f, 1.0f);
		bk.AddSlider("Depth Offset 0"              , &gCascadeShadows.m_amdDepthOffset[0],  0.0f, 0.001f, 0.00001f);
		bk.AddSlider("Kernel Size 0"               , &gCascadeShadows.m_amdFilterKernelIndex[0], 0, 3, 1);
		if (CASCADE_SHADOWS_COUNT > 1)
		{
			bk.AddSlider("Sun Size 1"                  , &gCascadeShadows.m_amdSunWidth[1],     1.0f, 1000.0f, 1.0f);
			bk.AddSlider("Depth Offset 1"              , &gCascadeShadows.m_amdDepthOffset[1],  0.0f, 0.001f, 0.00001f);
			bk.AddSlider("Kernel Size 1"               , &gCascadeShadows.m_amdFilterKernelIndex[1], 0, 3, 1);
		}
		if (CASCADE_SHADOWS_COUNT > 2)
		{
			bk.AddSlider("Sun Size 2"                  , &gCascadeShadows.m_amdSunWidth[2],     1.0f, 1000.0f, 1.0f);
			bk.AddSlider("Depth Offset 2"              , &gCascadeShadows.m_amdDepthOffset[2],  0.0f, 0.001f, 0.00001f);
			bk.AddSlider("Kernel Size 2"               , &gCascadeShadows.m_amdFilterKernelIndex[2], 0, 3, 1);
		}
		if (CASCADE_SHADOWS_COUNT > 3)
		{
			bk.AddSlider("Sun Size 3"                  , &gCascadeShadows.m_amdSunWidth[3],     1.0f, 1000.0f, 1.0f);
			bk.AddSlider("Depth Offset 3"              , &gCascadeShadows.m_amdDepthOffset[3],  0.0f, 0.001f, 0.00001f);
			bk.AddSlider("Kernel Size 3"               , &gCascadeShadows.m_amdFilterKernelIndex[3], 0, 3, 1);
		}
	}
	bk.PopGroup();
#endif
#if USE_NV_SHADOW_LIB
	bk.PushGroup("Nvidia Shadows", false);
	{
		// Note that several of these tweakables now have per-cascade values, but we only expose [0] here.
		bk.AddSlider("Light Size"                  , &gCascadeShadows.m_baseLightSize, 0.0f, 5.0f, 0.005f);
		bk.AddSlider("Max Threshold"               , &gCascadeShadows.m_shadowParams.PCSSPenumbraParams.fMaxThreshold,   0.01f, 100.0f, 0.05f);
		bk.AddSlider("Min Size %"                  , &gCascadeShadows.m_baseMinSize, 0.0f,  100.0f, 1.0f);
		bk.AddSlider("Min Weight Exponent"         , &gCascadeShadows.m_shadowParams.PCSSPenumbraParams.fMinWeightExponent, 0.0f,  100.0f, 1.0f);
		bk.AddSlider("Min Weight Threshold %"      , &gCascadeShadows.m_shadowParams.PCSSPenumbraParams.fMinWeightThresholdPercent, 0.0f,  100.0f, 1.0f);
		bk.AddSlider("Blocker Search Dither %"     , &gCascadeShadows.m_shadowParams.PCSSPenumbraParams.fBlockerSearchDitherPercent, 0.0f,  100.0f, 1.0f);
		bk.AddSlider("Filter Dither %"             , &gCascadeShadows.m_shadowParams.PCSSPenumbraParams.fFilterDitherPercent[0], 0.0f,  100.0f, 1.0f);
		bk.AddSlider("CSM Border %"                , &gCascadeShadows.m_shadowParams.fCascadeBorderPercent, 0.0f,  100.0f, 1.0f);
		bk.AddSlider("CSM Blend %"                 , &gCascadeShadows.m_shadowParams.fCascadeBlendPercent,  0.0f,  100.0f, 1.0f);
		bk.AddSlider("Light Size Tolerance %"      , &gCascadeShadows.m_shadowParams.fCascadeLightSizeTolerancePercent,  0.0f,  100.0f, 1.0f);
		bk.AddSlider("PCF Soft Transition Scale "  , &gCascadeShadows.m_shadowParams.fManualPCFSoftTransitionScale[0], 0.0f, 65535.0f, 100.0f);
		bk.AddSlider("zBias Scale"                 , &gCascadeShadows.m_nvBiasScale, -5.0f, 5.0f, 0.1f);
		bk.AddToggle("Debug depth"                 , &gCascadeShadows.m_nvShadowsDebugVisualizeDepth);
		bk.AddToggle("Debug cascades"              , &gCascadeShadows.m_nvShadowsDebugVisualizeCascades);
	}
	bk.PopGroup();
#endif
}

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
void CCSMDebugGlobals::InitWidgets_SoftShadows(bkBank& bk)
{
	if (AreSoftShadowsAllowed())
	{
		bk.PushGroup("Soft Shadows");
		{
			bk.AddToggle("Soft Shadow Enabled"       , &gCascadeShadows.m_softShadowEnabled);
			bk.AddToggle("Soft Shadow Use Early Out" , &m_softShadowUseEarlyOut);
			bk.AddToggle("Soft Shadow Show Early Out", &m_softShadowShowEarlyOut);
			bk.AddSlider("Penumbra Kernel Radius"    , &m_softShadowPenumbraRadius, 0.00001f, 1.0f, 0.005f);
			bk.AddSlider("Max Kernel Size - LOD 1"   , &m_softShadowMaxKernelSize, 1, 7, 1);
		}
		bk.PopGroup();
	}
}
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING

bool CCSMDebugGlobals::InitResourcesOnDemand()
{
	if (m_debugShaderVars.m_shader == NULL) // init shaders
	{
		ASSET.PushFolder("common:/shaders");
		{
			grmShader* debugShader = grmShaderFactory::GetInstance().Create();

			if (debugShader && debugShader->Load("debug_shadows", NULL, false)) // no fallback, just fail
			{
				m_debugShaderTechnique               = debugShader->LookupTechnique("dbg_shadows");
				m_debugShaderTechniqueOverlay        = debugShader->LookupTechnique("dbg_shadows_overlay");
				m_debugShaderBaseTextureId           = debugShader->LookupVar("baseTexture");
#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
				m_debugShaderEntityIDTextureId       = debugShader->LookupVar("entityIDTexture");
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
				m_debugShaderDepthBufferId           = debugShader->LookupVar("depthBuffer");
				m_debugShaderNormalBufferId          = debugShader->LookupVar("normalBuffer");
				m_debugShaderViewToWorldProjectionId = debugShader->LookupVar("viewToWorldProjectionParam");
				m_debugShaderPerspectiveShearId      = debugShader->LookupVar("perspectiveShear");
				m_debugShaderVarsId                  = debugShader->LookupVar("gCSMShaderVars_debug");
				m_debugShaderVars.InitResources(debugShader);
			}
		}
		ASSET.PopFolder();
	}

	return (m_debugShaderVars.m_shader != NULL);
}

#if __PS3 && __DEV

void CRenderPhaseCascadeShadows_DumpRenderTargetsAfterSetWaitFlip()
{
	if (g_dumpRenderTargetsAfterSetWaitFlip)
	{
		g_dumpRenderTargetsAfterSetWaitFlip = false;

		const CCSMState& state = gCascadeShadows.GetRenderState();

		DumpRenderTarget("c:/dump/backbuffer.dds", CRenderTargets::GetBackBuffer());
		DumpRenderTarget("c:/dump/frontbuffer.dds", grcTextureFactory::GetInstance().GetFrontBuffer());
	}
}

#endif // __PS3 && __DEV

enum
{
	dbg_shadows_pass_copyColour,
	dbg_shadows_pass_copyTexture,
	dbg_shadows_pass_copyDepth,
	dbg_shadows_pass_copyTextureEdgeDetect,
	dbg_shadows_pass_copyDepthEdgeDetect,
};

void CCSMDebugGlobals::DebugDraw(const CCSMGlobals& globals)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	const CCSMState& state = globals.GetRenderState();
	if (m_drawOpacity > 0.0f && state.m_shadowIsActive)
	{
		// set these just in case something has clobbered them .. seems to be necessary in non-testbed
		CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars();

		if (m_shadowDebugOpacity > 0.0f || m_shadowDebugGridOpacity > 0.0f || m_shadowDebugRingOpacity > 0.0f)
		{
			if (m_drawShadowDebugShader && InitResourcesOnDemand())
			{
#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
				m_debugShaderVars.m_shader->SetVar(m_debugShaderEntityIDTextureId      , globals.m_shadowMapEntityIDs);
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD

				const grcRenderTarget* depthBuffer = MSAA_ONLY(GRCDEVICE.GetMSAA()>1 ? CRenderTargets::GetDepthResolved() :) CRenderTargets::GetDepthBuffer();
				m_debugShaderVars.m_shader->SetVar(m_debugShaderDepthBufferId          , depthBuffer);
				m_debugShaderVars.m_shader->SetVar(m_debugShaderNormalBufferId         , GBuffer::GetTarget(GBUFFER_RT_1));
				m_debugShaderVars.m_shader->SetVar(m_debugShaderViewToWorldProjectionId, state.m_viewToWorldProjection);
				m_debugShaderVars.m_shader->SetVar(m_debugShaderPerspectiveShearId     , state.m_viewPerspectiveShear);
				m_debugShaderVars.m_shader->SetVar(m_debugShaderVarsId                 , state.m_debug.m_shaderVars, CASCADE_SHADOWS_SHADER_VAR_COUNT_DEBUG);
				m_debugShaderVars.SetShaderVars(globals);

				grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_CompositeAlpha);

				grcEffectTechnique technique = m_debugShaderTechniqueOverlay;

				if (technique != grcetNONE && AssertVerify(m_debugShaderVars.m_shader->BeginDraw(grmShader::RMC_DRAW, false, technique)))
				{
					m_debugShaderVars.m_shader->Bind(0);
					{
						const Color32 colour = Color32(255,255,255,(int)(m_drawOpacity*255.0f + 0.5f));

						const float x0 = -1.0f;
						const float y0 = +1.0f;
						const float x1 = +1.0f;
						const float y1 = -1.0f;
						const float z  =  0.0f;

						grcBegin(drawTriStrip, 4);
						grcVertex(x0, y0, z, 0.0f, 0.0f, -1.0f, colour, 0.0f, 0.0f);
						grcVertex(x0, y1, z, 0.0f, 0.0f, -1.0f, colour, 0.0f, 1.0f);
						grcVertex(x1, y0, z, 0.0f, 0.0f, -1.0f, colour, 1.0f, 0.0f);
						grcVertex(x1, y1, z, 0.0f, 0.0f, -1.0f, colour, 1.0f, 1.0f);
						grcEnd();
					}
					m_debugShaderVars.m_shader->UnBind();
					m_debugShaderVars.m_shader->EndDraw();
				}
				else
				{
					// failed to load technique?
				}

#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
				m_debugShaderVars.m_shader->SetVar(m_debugShaderEntityIDTextureId, grcTexture::None);
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
				m_debugShaderVars.m_shader->SetVar(m_debugShaderDepthBufferId    , grcTexture::None);
				m_debugShaderVars.m_shader->SetVar(m_debugShaderNormalBufferId   , grcTexture::None);
			}
		}

		// draw rendertargets and debug objects
		{
			const float dstScale = 1.0f/(float)(m_showShadowMapScale + 1);

			const Color32 colour = Color32(m_showShadowMapContrast ? 255 : 0,255,255,255);

			float dstX = 50.0f;
			float dstY = 32.0f;

			if (m_showShadowMapCascadeIndex != 0)
			{
				const bool bDrawQuadrants = (m_showShadowMapCascadeIndex == CASCADE_SHADOWS_COUNT + 2);
				float dstX2 = dstX;

				for (int quadrantIndex = -1; quadrantIndex < CASCADE_SHADOWS_COUNT; quadrantIndex++)
				{
					if (bDrawQuadrants != (quadrantIndex >= 0))
					{
						continue;
					}

					const int cascadeIndex = bDrawQuadrants ? quadrantIndex : (m_showShadowMapCascadeIndex - 2);

#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
					if (m_showShadowMapEntityIDs)
					{
						const int pass = m_showShadowMapEdgeDetect ? dbg_shadows_pass_copyTextureEdgeDetect : dbg_shadows_pass_copyTexture;
						Color32 colour2(255,255,255,255);

						if (m_showShadowMapEdgeDetect)
						{
							colour2.SetBlue((m_showShadowMapScale + 1)*(bDrawQuadrants ? 2 : 1));
						}

						dstX2 = DebugDrawRenderTarget(state, globals.m_shadowMapEntityIDs, colour2, pass, dstX, dstY, dstScale, cascadeIndex, quadrantIndex, true);
					}
					else
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
					{
						const int pass = m_showShadowMapEdgeDetect ? dbg_shadows_pass_copyDepthEdgeDetect : dbg_shadows_pass_copyDepth;
						Color32 colour2 = colour;

						if (m_showShadowMapEdgeDetect)
						{
							colour2.SetRed  ((int)(m_showShadowMapEdgeDetectExp *255.0f + 0.5f)); // pack edge detect exponent in colour.x
							colour2.SetGreen((int)(m_showShadowMapEdgeDetectTint*255.0f + 0.5f)); // pack edge detect tint in colour.y
							colour2.SetBlue((m_showShadowMapScale + 1)*(bDrawQuadrants ? 2 : 1));
						}

						const grcRenderTarget* src = PS3_SWITCH(globals.m_shadowMapPatched, state.m_shadowMap);
						dstX2 = DebugDrawRenderTarget(state, src, colour2, pass, dstX, dstY, dstScale, cascadeIndex, quadrantIndex, true);
					}
				}

				dstX = dstX2;
			}

			if (m_showShadowMapVS)
			{
				dstX = DebugDrawRenderTarget(state, globals.m_shadowMapVS, colour, dbg_shadows_pass_copyDepth, dstX, dstY, dstScale, INDEX_NONE, INDEX_NONE, false);
			}

			if (m_showShadowMapMini)
			{
				dstX = DebugDrawRenderTarget(state, globals.PS3_SWITCH(m_shadowMapMiniPatched, m_shadowMapMini), colour, dbg_shadows_pass_copyDepth, dstX, dstY, dstScale, INDEX_NONE, INDEX_NONE, false);
			}
		}
	}

#if __PS3 && __DEV
	if (g_dumpRenderTargets)
	{
		g_dumpRenderTargets = false;
		g_dumpRenderTargetsAfterSetWaitFlip = true;

		// hmm, dumping the frontbuffer doesn't seem to be working ..
		// Alex said "x:\gta5\src\dev\game\renderer\PostProcessFX.cpp PostFX::ProcessNonDepthFX(), after SetWaitFlip();"

		const CCSMState& state = gCascadeShadows.GetRenderState();

		DumpRenderTarget("c:/dump/depthbuffer.dds", GBuffer::GetDepthTargetAsColor(), true, state.m_viewToWorldProjection.GetCol2().GetWf(), state.m_viewToWorldProjection.GetCol3().GetWf());
		DumpRenderTarget("c:/dump/shadowmap.dds", gCascadeShadows.m_shadowMapBase, true);

		for (int i = 0; i < 2; i++)
		{
			DumpRenderTarget(atVarString("c:/dump/gbuffer_%d.dds", i), GBuffer::GetTarget((GBufferRT)i));
		}
	}
#endif // __PS3 && __DEV

#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD && __DEV
		if (g_dumpRenderTargetCountEntityIDs)
		{
			g_dumpRenderTargetCountEntityIDs = false;

			if (gCascadeShadows.m_shadowMapEntityIDs)
			{
				int numEntitiesAll = 0;
				int numEntities[4] = {0,0,0,0};

				numEntitiesAll = DumpRenderTargetCountEntityIDs(gCascadeShadows.m_shadowMapEntityIDs, INDEX_NONE);
				numEntities[0] = DumpRenderTargetCountEntityIDs(gCascadeShadows.m_shadowMapEntityIDs, 0);
				numEntities[1] = DumpRenderTargetCountEntityIDs(gCascadeShadows.m_shadowMapEntityIDs, 1);
				numEntities[2] = DumpRenderTargetCountEntityIDs(gCascadeShadows.m_shadowMapEntityIDs, 2);
				numEntities[3] = DumpRenderTargetCountEntityIDs(gCascadeShadows.m_shadowMapEntityIDs, 3);

				Displayf(
					"CountEntityIDs = %d - (%d/%d/%d/%d)",
					numEntitiesAll,
					numEntities[0],
					numEntities[1],
					numEntities[2],
					numEntities[3]
				);
			}
		}
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD && __DEV
}

float CCSMDebugGlobals::DebugDrawRenderTarget(const CCSMState& state, const grcRenderTarget* rt, const Color32& colour, int pass, float dstX, float dstY, float dstScale, int cascadeIndex, int quadrantIndex, bool bDrawDebugObjects)
{
	if (rt)
	{
		fwBox2D srcBounds;

		if (cascadeIndex != INDEX_NONE)
		{
			srcBounds = fwBox2D(0.0f, 0.0f, (float)CRenderPhaseCascadeShadowsInterface::CascadeShadowResX(), (float)CRenderPhaseCascadeShadowsInterface::CascadeShadowResY());
		}
		else
		{
			srcBounds = fwBox2D(0.0f, 0.0f, (float)rt->GetWidth(), (float)rt->GetHeight());
		}

		fwBox2D dstBounds = srcBounds;

		if (dstScale != 1.0f)
		{
			dstBounds.x1 = dstBounds.x0 + (dstBounds.x1 - dstBounds.x0)*dstScale;
			dstBounds.y1 = dstBounds.y0 + (dstBounds.y1 - dstBounds.y0)*dstScale;
		}


#if !CASCADE_SHADOW_TEXARRAY
		if (cascadeIndex != INDEX_NONE)
		{
			srcBounds.y0 += (float)(cascadeIndex*CRenderPhaseCascadeShadowsInterface::CascadeShadowResY());
			srcBounds.y1 += (float)(cascadeIndex*CRenderPhaseCascadeShadowsInterface::CascadeShadowResY());
		}
#endif //CASCADE_SHADOW_TEXARRAY

		dstBounds.x0 += dstX;
		dstBounds.y0 += dstY;
		dstBounds.x1 += dstX;
		dstBounds.y1 += dstY;

		if (quadrantIndex != INDEX_NONE)
		{
			if (quadrantIndex & 1) { dstBounds.x0 = (dstBounds.x1 + dstBounds.x0)/2.0f; }
			else                   { dstBounds.x1 = (dstBounds.x1 + dstBounds.x0)/2.0f; }
			if (quadrantIndex & 2) { dstBounds.y0 = (dstBounds.y1 + dstBounds.y0)/2.0f; }
			else                   { dstBounds.y1 = (dstBounds.y1 + dstBounds.y0)/2.0f; }
		}

		DebugDrawTexturedQuad(
			state,
			dstBounds,
			srcBounds,
			pass,
			colour,
			m_drawOpacity,
			(float)cascadeIndex,
			rt
		);

		if (bDrawDebugObjects)
		{
			if (quadrantIndex == INDEX_NONE)
			{
				if (cascadeIndex != INDEX_NONE)
				{
					const grcViewportWindow& viewport = state.m_viewports[cascadeIndex];

					DebugDrawDebugObjects(state, viewport.GetCompositeMtx(), Vec4V(dstX, dstY, dstScale*(float)CRenderPhaseCascadeShadowsInterface::CascadeShadowResX(), dstScale*(float)CRenderPhaseCascadeShadowsInterface::CascadeShadowResY()), cascadeIndex);
				}
				else
				{
					for (int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
					{
						const grcViewportWindow& viewport = state.m_viewports[i];

						DebugDrawDebugObjects(state, viewport.GetCompositeMtx(), viewport.GetWindow()*ScalarV(dstScale) + Vec4V(dstX, dstY, 0.0f, 0.0f), i);
					}
				}
			}
			else if (quadrantIndex == CASCADE_SHADOWS_COUNT - 1)
			{
				const grcViewportWindow& viewport = state.m_viewports[quadrantIndex];

				DebugDrawDebugObjects(state, viewport.GetCompositeMtx(), Vec4V(
					dstBounds.x0,
					dstBounds.y0,
					dstBounds.x1 - dstBounds.x0,
					dstBounds.y1 - dstBounds.y0
				), quadrantIndex);
			}
		}

		dstX = dstBounds.x1 + 4.0f;
	}

	return dstX;
}

void CCSMDebugGlobals::DebugDrawDebugObjects(const CCSMState& state, Mat44V_In proj, Vec4V_In projWindow, int cascadeIndex)
{
	if (m_enableCascade[cascadeIndex] && InitResourcesOnDemand())
	{
		grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Normal);

		PS3_ONLY(GCM_STATE(SetLineSmoothEnable, CELL_GCM_TRUE));
		PS3_ONLY(GCM_STATE(SetLineWidth, (u32)(1.0f*8.0f))); // 6.3 fixed point

		if (AssertVerify(m_debugShaderVars.m_shader->BeginDraw(grmShader::RMC_DRAW, false, m_debugShaderTechnique)))
		{
			m_debugShaderVars.m_shader->Bind(dbg_shadows_pass_copyColour);

			for (int i = 0; i < state.m_debug.m_objects.GetCount(); i++)
			{
				const CCSMDebugObject* obj = state.m_debug.m_objects[i];

				if (obj->IsFlagSet(cascadeIndex) || m_drawFlags.m_drawCascadeGhosts)
				{
					obj->Draw_grcVertex(proj, projWindow, obj->IsFlagSet(cascadeIndex) ? 0 : 2);
				}
			}

			m_debugShaderVars.m_shader->UnBind();
			m_debugShaderVars.m_shader->EndDraw();
		}

		PS3_ONLY(GCM_STATE(SetLineSmoothEnable, CELL_GCM_FALSE));
	}
}

void CCSMDebugGlobals::DebugDrawTexturedQuad(const CCSMState& state, const fwBox2D& dstBounds, const fwBox2D& srcBounds, int pass,
											 const Color32& colour, float opacity, float z, const grcTexture* texture)
{
	UNUSED_VAR(state);

	if (opacity > 0.0f && InitResourcesOnDemand())
	{
		// assume eDTQPCS_Pixels
		const float dx = 2.0f/(float)VideoResManager::GetSceneWidth();
		const float dy = 2.0f/(float)VideoResManager::GetSceneHeight();
		const float x0 = +(dx*dstBounds.x0 - 1.0f);
		const float y0 = -(dy*dstBounds.y0 - 1.0f);
		const float x1 = +(dx*dstBounds.x1 - 1.0f);
		const float y1 = -(dy*dstBounds.y1 - 1.0f);

		const float sx = 1.0f/(float)texture->GetWidth ();
		const float sy = 1.0f/(float)texture->GetHeight();
		const float u0 = sx*srcBounds.x0;
		const float v0 = sy*srcBounds.y0;
		const float u1 = sx*srcBounds.x1;
		const float v1 = sy*srcBounds.y1;

		Color32 c = colour; c.SetAlpha((int)((float)colour.GetAlpha()*opacity));

		m_debugShaderVars.m_shader->SetVar(m_debugShaderBaseTextureId, texture);

		grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Normal);

		if (AssertVerify(m_debugShaderVars.m_shader->BeginDraw(grmShader::RMC_DRAW, false, m_debugShaderTechnique)))
		{
			m_debugShaderVars.m_shader->Bind(pass);
			{
				grcBegin(drawTriStrip, 4);
				grcVertex(x0, y0, z, 0.0f, 0.0f, -1.0f, c, u0, v0);
				grcVertex(x0, y1, z, 0.0f, 0.0f, -1.0f, c, u0, v1);
				grcVertex(x1, y0, z, 0.0f, 0.0f, -1.0f, c, u1, v0);
				grcVertex(x1, y1, z, 0.0f, 0.0f, -1.0f, c, u1, v1);
				grcEnd();
			}
			m_debugShaderVars.m_shader->UnBind();
			m_debugShaderVars.m_shader->EndDraw();
		}

		m_debugShaderVars.m_shader->SetVar(m_debugShaderBaseTextureId, grcTexture::None);
	}
}

void CCSMDebugGlobals::UpdateModifiedShadowSettings()
{
	const int N = CASCADE_SHADOWS_COUNT;

	if (DEV_SWITCH(m_useIrregularFade, CSM_USE_IRREGULAR_FADE))
	{
		// pull in z splits
		const float z0 = CSM_DEFAULT_SPLIT_Z_MIN;
		const float z1 = CSM_DEFAULT_SPLIT_Z_MAX/DEV_SWITCH(m_useModifiedShadowFarDistanceScale, CSM_DEFAULT_MODIFIED_FAR_DISTANCE_SCALE);

		m_cascadeSplitZ[0] = z0;
		m_cascadeSplitZ[N] = z1;

		for (int i = 1; i < N; i++)
		{
			m_cascadeSplitZ[i] = z0 + (z1 - z0)*(float)i/(float)N;
		}
	}
	else
	{
		// setup z splits
		const float z0 = CSM_DEFAULT_SPLIT_Z_MIN;
		const float z1 = CSM_DEFAULT_SPLIT_Z_MAX;

		m_cascadeSplitZ[0] = z0;
		m_cascadeSplitZ[N] = z1;

		for (int i = 1; i < N; i++)
		{
			m_cascadeSplitZ[i] = z0 + (z1 - z0)*(float)i/(float)N;
		}
	}
}

void CCSMDebugGlobals::BeginFindingNearZShadows()
{
}

float CCSMDebugGlobals::EndFindingNearZShadows()
{
	return 0.0f;
}

#endif // __BANK

// ================================================================================================

void CCSMSharedShaderVars::InitResources()
{
	m_gCSMShadowTextureId     = grcEffect::LookupGlobalVar("gCSMShadowTexture");
	m_gCSMShadowTextureVSId   = grcEffect::LookupGlobalVar("gCSMShadowTextureVS");
	m_gCSMDitherTextureId     = grcEffect::LookupGlobalVar("gCSMDitherTexture");
	m_gCSMShaderVarsId_shared = grcEffect::LookupGlobalVar("gCSMShaderVars_shared");
	m_gCSMCloudTextureId	  = grcEffect::LookupGlobalVar("gCSMCloudTexture");
	m_gCSMCloudNoiseTexture	  = CShaderLib::LookupTexture("cloudnoise");

	bool bMustExist = true;
#if RSG_PC
	if (GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled())
		bMustExist = false;
#endif
	m_gCSMSmoothStepTextureId = grcEffect::LookupGlobalVar("gCSMSmoothStepTexture", bMustExist);

	m_gCSMShadowParamsId	  = grcEffect::LookupGlobalVar("gCSMShadowParams");
#if GS_INSTANCED_SHADOWS && !RSG_ORBIS // See SIMULATE_DEPTH_BIAS in cascadeshadows_receiving.fxh.
	m_gCSMDepthBias           = grcEffect::LookupGlobalVar("gCSMDepthBias");
	m_gCSMDepthSlopeBias      = grcEffect::LookupGlobalVar("gCSMDepthSlopeBias");
#endif // GS_INSTANCED_SHADOWS && !RSG_ORBIS
#if VARIABLE_RES_SHADOWS
	m_gCSMResolution          = grcEffect::LookupGlobalVar("gCSMResolution");
#endif // VARIABLE_RES_SHADOWS
}

void CCSMSharedShaderVars::SetShaderVars(const CCSMGlobals& globals SHADOW_MATRIX_REFLECT_ONLY(, bool bShadowMatrixOnly, Vec4V_In reflectionPlane)) const
{
	const CCSMState& state = globals.GetRenderState();

#if SHADOW_MATRIX_REFLECT
	if (bShadowMatrixOnly)
	{
		Vec4V temp[3];
		temp[0] = state.m_cascadeShaderVars_shared[0];
		temp[1] = state.m_cascadeShaderVars_shared[1];
		temp[2] = state.m_cascadeShaderVars_shared[2];

		if (!IsZeroAll(reflectionPlane))
		{
			// reflect axes, preserve w
			temp[0] = Vec4V(PlaneReflectVector(reflectionPlane, temp[0].GetXYZ()), temp[0].GetW());
			temp[1] = Vec4V(PlaneReflectVector(reflectionPlane, temp[1].GetXYZ()), temp[1].GetW());
			temp[2] = Vec4V(PlaneReflectVector(reflectionPlane, temp[2].GetXYZ()), temp[2].GetW());
		}

		grcEffect::SetGlobalVar(m_gCSMShaderVarsId_shared, temp, NELEM(temp));
		return;
	}
	else
	{
		Assert(IsZeroAll(reflectionPlane)); // we don't support reflecting the shadow matrix if we're setting all the vars
	}
#endif // SHADOW_MATRIX_REFLECT

	const grcTexture* shadowMap = state.m_shadowIsActive? state.m_shadowMap : globals.m_shadowMapSpecialFull;
	grcEffect::SetGlobalVar(m_gCSMShadowTextureId,     shadowMap);
	grcEffect::SetGlobalVar(m_gCSMShadowTextureVSId,   shadowMap);
	grcEffect::SetGlobalVar(m_gCSMDitherTextureId,     globals.m_ditherTexture);

	//The offsets are camera relative and must be recomputed for different view points
	Vec4V vars[CASCADE_SHADOWS_SHADER_VAR_COUNT_SHARED];

	if(!state.m_shadowIsActive)
	{
		int i = 0;
		for(; i < _cascadeBoundsConstB_packed_start; i++)
			vars[i] = Vec4V(V_ZERO);

		//Fog shadows should always be updated even when cascade shadows is disabled
		//Reason is cascade shadows on particles are artist controlled and there are cases where cascade shadows 
		//do not affect particles. Only fog shadows affect them. So when cascade shadows gets disabled, 
		//particles still need to right fog shadow values so there won't be any pops
		vars[CASCADE_SHADOWS_SHADER_VAR_FOG_SHADOWS] = state.m_cascadeShaderVars_shared[CASCADE_SHADOWS_SHADER_VAR_FOG_SHADOWS];

		Vec4V offset = Vec4V(0.0f, 0.0f, 1.0f, 0.0f);
		for(; i < _cascadeBoundsConstB_packed_start + CASCADE_SHADOWS_COUNT; i++)
			vars[i] = offset;
	}
	else
	{
		for(int i = 0; i < CASCADE_SHADOWS_SHADER_VAR_COUNT_SHARED; i++)
			vars[i] = state.m_cascadeShaderVars_shared[i];

		if(Verifyf(grcViewport::GetCurrent(), "Viewport must be set before calling CCSMSharedShaderVars::SetShaderVars"))
			for(int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
			{
				const Vec3V shadowSpaceCamDelta = Multiply(state.m_worldToShadow33, grcViewport::GetCurrent()->GetCameraPosition() - state.m_viewports[i].GetCameraPosition());
				const Vec3V b = state.m_cascadeShaderVars_shared[_cascadeBoundsConstA_packed_start + i].GetXYZ()*
					(Vec3V(state.m_cascadeBoundsExtentXY[i], ScalarV(-state.m_viewports[i].GetNearClip())) + shadowSpaceCamDelta)
					- Vec3V(Vec2V(V_HALF), ScalarV(V_ZERO));
				vars[_cascadeBoundsConstB_packed_start + i] = Vec4V(b, ScalarV((float)i));
			}
	}

	grcEffect::SetGlobalVar(m_gCSMShaderVarsId_shared, vars, CASCADE_SHADOWS_SHADER_VAR_COUNT_SHARED);


#if __XENON || __PS3
	// TBR: if rendering particles, make sure we push no NaNs (bugstar:1607972).
	// From ptfxSpriteCalculateBasicLighting in ptfx_sprite.fx:
	//		// this is a hack to get the PS3 shaders to even compile.
	//		float shadow = saturate(10000.0 + gCSMShaderVars_shared[0]);
	if (!state.m_shadowIsActive && gDrawListMgr->IsExecutingDrawList(DL_RENDERPHASE_DRAWSCENE))
	{
		Vec4V vZero(V_ZERO);
		grcEffect::SetGlobalVar(m_gCSMShaderVarsId_shared, vZero);
	}
#endif

#if GS_INSTANCED_SHADOWS
	if (!gCascadeShadows.m_bInstShadowActive)
	{
		gCascadeShadows.m_shadowDepthBias = Vec4V(V_ZERO);
		gCascadeShadows.m_shadowDepthSlopeBias = Vec4V(V_ZERO);
	}
	else
	{
		static float gSlopeBiasCascade0 = 0.0f;
		static float gSlopeBiasCascade1 = 0.0f;
		static float gSlopeBiasCascade2 = 0.00015f;
		static float gSlopeBiasCascade3 = 0.0003f;

		gCascadeShadows.m_shadowDepthSlopeBias = Vec4V(gSlopeBiasCascade0, gSlopeBiasCascade1, gSlopeBiasCascade2, gSlopeBiasCascade3);
	}

#if !RSG_ORBIS // See SIMULATE_DEPTH_BIAS in cascadeshadows_receiving.fxh.
#if SUPPORT_INVERTED_VIEWPORT
	grcEffect::SetGlobalVar(m_gCSMDepthBias, -gCascadeShadows.m_shadowDepthBias);
	grcEffect::SetGlobalVar(m_gCSMDepthSlopeBias, -gCascadeShadows.m_shadowDepthSlopeBias);
#else // SUPPORT_INVERTED_VIEWPORT
	grcEffect::SetGlobalVar(m_gCSMDepthBias, gCascadeShadows.m_shadowDepthBias);
	grcEffect::SetGlobalVar(m_gCSMDepthSlopeBias, gCascadeShadows.m_shadowDepthSlopeBias);
#endif // SUPPORT_INVERTED_VIEWPORT
#endif // !RSG_ORBIS

#endif // GS_INSTANCED_SHADOWS

#if VARIABLE_RES_SHADOWS
	gCascadeShadows.m_shadowResolution = Vec4V((float)gCascadeShadows.m_shadowResX, (float)gCascadeShadows.m_shadowResY, 1.0f / (float)gCascadeShadows.m_shadowResX, 1.0f / (float)gCascadeShadows.m_shadowResY);

	grcEffect::SetGlobalVar(m_gCSMResolution, gCascadeShadows.m_shadowResolution);
#endif // VARIABLE_RES_SHADOWS
	return ;
}

void CCSMSharedShaderVars::SetCloudShaderVars(const CCSMGlobals& globals) const
{
	const CCSMState& state = globals.GetRenderState();
	const eCSMSampleType sampleType = state.m_sampleType;

#if RSG_PC
	bool bIsClouds = (sampleType >= CSM_ST_CLOUDS_FIRST && sampleType <= CSM_ST_CLOUDS_LAST);

#if USE_NV_SHADOW_LIB
	if(globals.m_enableNvidiaShadows)
		bIsClouds = gCascadeShadows.AreCloudShadowsEnabled();
#endif

#if USE_AMD_SHADOW_LIB
	if(globals.m_enableAMDShadows)
		bIsClouds = gCascadeShadows.AreCloudShadowsEnabled();
#endif

#else
	const bool bIsClouds = (sampleType >= CSM_ST_CLOUDS_FIRST && sampleType <= CSM_ST_CLOUDS_LAST);
#endif

	const float time = CClock::GetDayRatio();
	static dev_float timeScale	= 2.0f;

	Vec4V shadowParams = Vec4V(
		-state.m_cascadeShaderVars_shared[0].GetZ(),
		-state.m_cascadeShaderVars_shared[1].GetZ(),
		-state.m_cascadeShaderVars_shared[2].GetZ(),
		(bIsClouds?ScalarVFromF32(fmodf(time*timeScale, 1.0f)):ScalarV(V_ZERO))
		);

	grcEffect::SetGlobalVar(m_gCSMShadowParamsId, shadowParams);

	grcEffect::SetGlobalVar(m_gCSMSmoothStepTextureId, (bIsClouds ? globals.m_smoothStepRT:grcTexture::NoneWhite));
	grcEffect::SetGlobalVar(m_gCSMCloudTextureId, m_gCSMCloudNoiseTexture);

}

void CCSMSharedShaderVars::SetShadowMap(const grcTexture* tex) const
{
	grcEffect::SetGlobalVar(m_gCSMShadowTextureId, tex);
}

void CCSMSharedShaderVars::RestoreShadowMap(const CCSMGlobals& globals) const
{
	const CCSMState& state = globals.GetRenderState();
	grcEffect::SetGlobalVar(m_gCSMShadowTextureId, state.m_shadowIsActive? state.m_shadowMap : globals.m_shadowMapSpecialFull);
}

void CCSMLocalShaderVars::InitResources(grmShader* shader)
{
	m_shader                    = shader;
	m_gCSMShaderVarsId_deferred = shader->LookupVar("gCSMShaderVars_deferred");
}

void CCSMLocalShaderVars::SetShaderVars(const CCSMGlobals& globals) const
{
	if (m_shader)
	{
		const CCSMState& state = globals.GetRenderState();
		m_shader->SetVar(m_gCSMShaderVarsId_deferred, state.m_cascadeShaderVars_deferred, CASCADE_SHADOWS_SHADER_VAR_COUNT_DEFERRED);		
	}
}

void CCSMState::Clear()
{
#if __BANK
	m_debug.Clear();
#endif // __BANK
}

//Needs initialising before settings set it for PC.
bool CCSMGlobals::m_shadowDirectionApplyClamp = true;

CCSMGlobals::CCSMGlobals()
{
	sysMemSet(this, 0, sizeof(*this));
}

void CCSMGlobals::Init(grmShader* deferredLightingShader)
{
#if VARIABLE_RES_SHADOWS
	m_shadowResX = CASCADE_SHADOWS_RES_X;
	m_shadowResY = CASCADE_SHADOWS_RES_Y;
#endif // VARIABLE_RES_SHADOWS

	m_softShadowIntermediateRT_Resolved = NULL;

	const int N = CASCADE_SHADOWS_COUNT;

	const ScalarV z0 = ScalarV(CSM_DEFAULT_SPLIT_Z_MIN);
	const ScalarV z1 = ScalarV(CSM_DEFAULT_SPLIT_Z_MAX);

	m_cascadeSplitZ[0] = z0;
	m_cascadeSplitZ[N] = z1;

	for (int i = 1; i < N; i++)
	{
		m_cascadeSplitZ[i] = z0 + (z1 - z0)*ScalarV((float)i/(float)N);
		m_prevWorldToCascade[i] = Mat34V(V_IDENTITY);
	}

	AdjustSplitZ(CSM_DEFAULT_SPLIT_Z_EXP_WEIGHT);

	m_worldHeight[0] = ScalarV(CSM_DEFAULT_WORLD_HEIGHT_MIN);
	m_worldHeight[1] = ScalarV(CSM_DEFAULT_WORLD_HEIGHT_MAX);
	m_worldHeightReset = false;

	m_recvrHeight[0] = ScalarV(CSM_DEFAULT_RECVR_HEIGHT_MIN);
	m_recvrHeight[1] = ScalarV(CSM_DEFAULT_RECVR_HEIGHT_MAX);
	m_recvrHeightReset = false;

	m_enableScreenSizeCheck = true;
#if RSG_PC
	m_depthBiasPrecisionScale = Vec4V(V_ONE);
	m_slopeBiasPrecisionScale = 1.0f;
#endif
	m_numCascades = CASCADE_SHADOWS_COUNT;
	m_distanceMultiplier = 1.0f;

	// init session
	m_scriptShadowType = CSM_TYPE_CASCADES;

#if CSM_QUADRANT_TRACK_ADJ
	m_scriptBoundPosition            = 0.0f;
	m_scriptQuadrantTrackExpand1     = 0.0f;
	m_scriptQuadrantTrackExpand2     = 0.0f;
	m_scriptQuadrantTrackSpread      = 0.0f;
	m_scriptQuadrantTrackScale       = Vec3V(V_ONE);
	m_scriptQuadrantTrackOffset      = Vec3V(V_ZERO);
#endif // CSM_QUADRANT_TRACK_ADJ
	m_scriptQuadrantTrackFlyCamera   = false;
	m_scriptQuadrantTrackForceVisAll = false;

	for (int i = 0; i < N; i++)
	{
		m_scriptCascadeBoundsEnable[i]                    = false;
		m_scriptCascadeBoundsLerpFromCustscene[i]         = false;
		m_scriptCascadeBoundsLerpFromCustsceneDuration[i] = 0.0f;
		m_scriptCascadeBoundsLerpFromCustsceneTime[i]     = 0.0f;
		m_scriptCascadeBoundsSphere[i]                    = Vec4V(V_ZERO);
	}

	m_useSyncFilter = CASCADE_SYNCHRONIZED_FILTERING_ON;

	m_scriptCascadeBoundsTanHFOV = ScalarV(V_ZERO);
	m_scriptCascadeBoundsTanVFOV = ScalarV(V_ZERO);
	m_scriptCascadeBoundsScale   = 1.0f;
	m_scriptEntityTrackerScale   = 1.0f;
	m_scriptSplitZExpWeight      = -1.0f;
	m_scriptWorldHeightUpdate    = CSM_DEFAULT_WORLD_HEIGHT_UPDATE != 0;
	m_scriptRecvrHeightUpdate    = CSM_DEFAULT_RECVR_HEIGHT_UPDATE != 0;
	m_scriptDitherRadiusScale    = 1.0f;
	m_scriptDepthBiasEnabled     = false;
	m_scriptDepthBias            = CSM_DEFAULT_DEPTH_BIAS;
	m_scriptSlopeBiasEnabled     = false;
	m_scriptSlopeBias            = CSM_DEFAULT_SLOPE_BIAS;

	SetSoftShadowProperties();

	m_scriptShadowSampleType     = GetDefaultFilter();

	m_scriptShadowQualityMode    = CSM_QM_IN_GAME;

	m_currentShadowQualityMode   = m_scriptShadowQualityMode;

#if GS_INSTANCED_SHADOWS
	m_shadowDepthBias            = Vec4V(V_ZERO);
	m_shadowDepthSlopeBias       = Vec4V(V_ZERO);
#endif // GS_INSTANCED_SHADOWS
#if VARIABLE_RES_SHADOWS
	m_shadowResolution           = Vec4V((float)CASCADE_SHADOWS_RES_X, (float)CASCADE_SHADOWS_RES_Y, 1.0f / (float)CASCADE_SHADOWS_RES_X, 1.0f / (float)CASCADE_SHADOWS_RES_Y);
#endif // VARIABLE_RES_SHADOWS

#if RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
	int renderAlphaShadows=1;
	PARAM_renderAlphaShadows.Get(renderAlphaShadows);
	//Only enabled on DX11 for now. Will work on lower Dx versions but need to fixup readonly depth (m_shadowMapReadOnly) as its not supported on them.
	m_enableAlphaShadows = GRCDEVICE.GetDxFeatureLevel() >= CSM_PARTICLE_SHADOWS_FEATURE_LEVEL && renderAlphaShadows;
	m_enableParticleShadows = GRCDEVICE.GetDxFeatureLevel() >= CSM_PARTICLE_SHADOWS_FEATURE_LEVEL && !PARAM_noparticleshadows.Get();
#if RSG_PC
	const Settings& settings = CSettingsManager::GetInstance().GetSettings();
	m_enableParticleShadows = m_enableAlphaShadows = settings.m_graphics.IsParticleShadowsEnabled();
#endif
	m_maxParticleShadowCascade = CASCADE_SHADOWS_COUNT;
#endif // RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW

#if USE_NV_SHADOW_LIB
	if (GRCDEVICE.GetDxFeatureLevel() >= 1100 && GRCDEVICE.IsUsingVendorAPI())
	{
		m_enableNvidiaShadows = PARAM_contacthardeningshadows.Get();		// Was true
		m_nvShadowsDebugVisualizeDepth = false;
		m_nvShadowsDebugVisualizeCascades = false;

		ResetNVidia();
	}
#endif

#if USE_AMD_SHADOW_LIB
	for(int i = 0; i < CASCADE_SHADOWS_COUNT; ++i)
	{
		m_amdSunWidth[i] = 1.0f;
	}

	if (GRCDEVICE.GetDxFeatureLevel() >= 1100)
	{
		m_enableAMDShadows = PARAM_AMDContactHardeningShadows.Get();

		// tell the AMD shadow library not to modify blend state
		g_amdShadowDesc.m_EnableOutputChannelFlags = false;

		// set the filtering for contact-hardening shadows
		g_amdShadowDesc.m_Filtering = AMD::SHADOWS_FILTERING_CONTACT_HARDENING;
		g_amdShadowDesc.m_NumCascades = CASCADE_SHADOWS_COUNT;
	}
#endif

#if USE_NV_SHADOW_LIB && USE_AMD_SHADOW_LIB
	if (m_enableNvidiaShadows && m_enableAMDShadows)
	{
		// don't try to run both at the same time
		m_enableNvidiaShadows = false;
	}

	if(GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo())
	{
		m_enableNvidiaShadows = false;
		m_enableAMDShadows = false;
	}
#endif

#if __BANK
	m_debug.Init();
#endif // __BANK

	InitResources(deferredLightingShader);
	m_entityTracker.Init();

#if CSM_PORTALED_SHADOWS
	GetRenderWindowRenderer().Init();
	GetUpdateWindowRenderer().Init();
#endif // CSM_PORTALED_SHADOWS

	// Otherwise we get QNaNs
	m_state[0].m_cloudTextureSettings.m_fogShadowFalloff = 1.0f;
	m_state[1].m_cloudTextureSettings.m_fogShadowFalloff = 1.0f;
}

#if USE_NV_SHADOW_LIB
void CCSMGlobals::ResetNVidia()
{
	m_nvBiasScale = 1.0f;

	// These need to be explicitly re-initialized to sensible defaults (again) because of the C++-unfriendly memset to 0 in the CCSMGlobals ctor.
	m_shadowParams = NV_ShadowLib_BufferRenderParams();
	m_mapDesc = NV_ShadowLib_ExternalMapDesc();

	// Defaults carefully tweaked for GTA V.  The actual values used are highly dependent on the game content because 
	// light size is in world space units.  The lib cannot predict good defaults.
	// Make the light radius as small as possible.  To get fat, soft shadows, adjust the ratio of fLightSize/fMaxThreshold to be larger.
	m_mapDesc.LightDesc.fLightSize[0]=m_baseLightSize=0.20f;
	m_mapDesc.LightDesc.fLightSize[1]=0.25f;
	m_mapDesc.LightDesc.fLightSize[2]=0.25f;
	m_mapDesc.LightDesc.fLightSize[3]=0.35f;
	m_shadowParams.PCSSPenumbraParams.fMaxThreshold = 64.0f;
	m_shadowParams.PCSSPenumbraParams.fMinWeightExponent = 3.0f;
	m_shadowParams.PCSSPenumbraParams.fMinWeightThresholdPercent = 10.0f;

	// Sharpness of the hardest shadows.  We need something softer (higher) than completely hard to hide pixelation.
	m_shadowParams.PCSSPenumbraParams.fMinSizePercent[0]=m_baseMinSize=4.0f;
	m_shadowParams.PCSSPenumbraParams.fMinSizePercent[1]=8.0f;
	m_shadowParams.PCSSPenumbraParams.fMinSizePercent[2]=16.0f;
	m_shadowParams.PCSSPenumbraParams.fMinSizePercent[3]=96.0f;

	m_shadowParams.PCSSPenumbraParams.fBlockerSearchDitherPercent = 2.0f;
	m_shadowParams.PCSSPenumbraParams.fFilterDitherPercent[0] = 2.0f;
	m_shadowParams.PCSSPenumbraParams.fFilterDitherPercent[1] = 2.0f;
	m_shadowParams.PCSSPenumbraParams.fFilterDitherPercent[2] = 2.0f;
	m_shadowParams.PCSSPenumbraParams.fFilterDitherPercent[3] = 20.0f;

	// These control the blending between cascades.  The large light radius and consequent fat blur cause filtering artefacts at the cascade
	// borders.  We seem able to hide these nicely with a bit more blending than default.
	m_shadowParams.fCascadeBlendPercent = 5.0f;
	m_shadowParams.fCascadeBorderPercent = 4.0f;

	// If the light is very large relative to a cascade, then we can get weird filering errors at the cascade edges.  If so, according to 
	// this parameter, drop to the next lower cascade.
	m_shadowParams.fCascadeLightSizeTolerancePercent = 25.0f;

	// Via Iain - Tue 29/04/2014 13:08 Re: Soft shadow bugs filed.
	int scale(0);
	if (PARAM_contacthardeningshadowspcfmode.Get(scale))
	{
		if (scale)
		{
			m_shadowParams.ePCFType = NV_ShadowLib_PCFType_Manual;
			m_shadowParams.fManualPCFSoftTransitionScale[0] =    1.0f * static_cast<float>(scale);
			m_shadowParams.fManualPCFSoftTransitionScale[1] =   10.0f * static_cast<float>(scale);
			m_shadowParams.fManualPCFSoftTransitionScale[2] =  100.0f * static_cast<float>(scale);
			m_shadowParams.fManualPCFSoftTransitionScale[3] = 1000.0f * static_cast<float>(scale);
		}
		else
		{
			m_shadowParams.ePCFType = NV_ShadowLib_PCFType_HW;
		}
	}
	else
	{
		m_shadowParams.ePCFType = NV_ShadowLib_PCFType_Manual;
		m_shadowParams.fManualPCFSoftTransitionScale[0] = 0.0000001f;	//	using nvidia suggested param
		m_shadowParams.fManualPCFSoftTransitionScale[1] = 0.000001f;	//	using nvidia suggested param
		m_shadowParams.fManualPCFSoftTransitionScale[2] = 0.00001f;		//	using nvidia suggested param
		m_shadowParams.fManualPCFSoftTransitionScale[3] = 0.0001f;		//	using nvidia suggested param
	}

	m_shadowParams.fConvergenceTestTolerance = 0.000001f;
	m_shadowParams.DepthBufferDesc.bInvertEyeDepth = USE_INVERTED_PROJECTION_ONLY;
}
#endif

void CCSMGlobals::SetSoftShadowProperties()
{
	m_defaultSampleType			 = CSM_ST_DEFAULT;
#if RSG_PC && __BANK
	if(PARAM_useSimplerShadowFiltering.Get())
		m_defaultSampleType		= CSM_ST_TWOTAP;
#endif
	m_defaultSoftness			 = -1;
	int softness				 = (int) CSettingsManager::GetInstance().GetSettings().m_graphics.m_Shadow_SoftShadows;
#if RSG_PC
	if(AreSoftShadowsAllowed())
#else
	if(softness != CSettings::Low && softness != CSettings::Custom && !PARAM_nosoftshadows.Get())
#endif
	{
		m_defaultSampleType		= filterSoftness[ Clamp(softness, 0, NUM_FILTER_SOFTNESS - 1) ];
		m_defaultSoftness		= softness;
		m_softShadowEnabled		= true;
	}
	else
	{
		m_softShadowEnabled		= false;
	}

#if USE_NV_SHADOW_LIB
	m_enableNvidiaShadows = CSettingsManager::GetInstance().GetSettings().m_graphics.m_Shadow_SoftShadows == CSettings::Special;
	m_enableNvidiaShadows &= GRCDEVICE.IsUsingVendorAPI();
	m_enableNvidiaShadows &= !(GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo());

	m_softShadowEnabled	= m_enableNvidiaShadows ? false : m_softShadowEnabled;
#endif

#if USE_AMD_SHADOW_LIB
	m_enableAMDShadows = CSettingsManager::GetInstance().GetSettings().m_graphics.m_Shadow_SoftShadows == CSettings::Custom;
	m_enableAMDShadows &= GRCDEVICE.IsUsingVendorAPI();
	m_enableAMDShadows &= !(GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo());

	m_softShadowEnabled	= m_enableAMDShadows ? false : m_softShadowEnabled;
#endif
}

int CCSMGlobals::GetDefaultFilter() const
{ 
	return m_useSyncFilter ? CSM_ST_DITHER2_LINEAR : m_defaultSampleType;
}

bool CCSMGlobals::AreCloudShadowsEnabled() const
{
	return CSettingsManager::GetInstance().GetSettings().m_graphics.IsCloudShadowsEnabled() &&
		(m_cloudTextureSettings.m_cloudShadowAmount > 0.01f || m_cloudTextureSettings.m_fogShadowAmount > 0.01f);
}

void CCSMGlobals::DeleteShaders()
{
	delete m_shadowCastingShader;
	m_shadowCastingShader = NULL;
	delete m_shadowProcessing[MM_DEFAULT].shader;
	delete m_shadowReveal[MM_DEFAULT].shader;
#if MSAA_EDGE_PASS
	if (m_shadowProcessing[MM_TEXTURE_READS_ONLY].shader)
		delete m_shadowProcessing[MM_TEXTURE_READS_ONLY].shader;
	if (m_shadowReveal[MM_TEXTURE_READS_ONLY].shader)
		delete m_shadowReveal[MM_TEXTURE_READS_ONLY].shader;
#endif //MSAA_EDGE_PASS
	for (int i=0; i!=MM_TOTAL; ++i)
	{
		m_shadowProcessing[i].shader = NULL;
		m_shadowReveal[i].shader = NULL;
	}
}

void CCSMGlobals::InitShaders(grmShader* deferredLightingShader)
{
	ASSET.PushFolder("common:/shaders");
	{
#if !__PS3
		m_shadowCastingShader = grmShaderFactory::GetInstance().Create();
		m_shadowCastingShader->Load("cascadeshadows");
		Assert(m_shadowCastingShader);
#endif // !__PS3

		m_shadowProcessing[MM_DEFAULT].Init(
#if DEVICE_MSAA
			GRCDEVICE.GetMSAA()>1 ? "cascadeshadows_processingMS" :
#endif // DEVICE_MSAA
			"cascadeshadows_processing");

		m_shadowReveal[MM_DEFAULT].Init(
#if DEVICE_MSAA
			GRCDEVICE.GetMSAA()>1 ? "cascadeshadows_renderingMS" :
#endif
			"cascadeshadows_rendering");

#if MSAA_EDGE_PASS
		if (DeferredLighting::IsEdgePassEnabled())
		{
			m_shadowProcessing[MM_TEXTURE_READS_ONLY].Init("cascadeshadows_processingMS0");
			m_shadowReveal[MM_TEXTURE_READS_ONLY].Init("cascadeshadows_renderingMS0");
		}
#endif //MSAA_EDGE_PASS

#if USE_NV_SHADOW_LIB
		m_enableNvidiaShadows = CSettingsManager::GetInstance().GetSettings().m_graphics.m_Shadow_SoftShadows == CSettings::Special;
		m_enableNvidiaShadows &= GRCDEVICE.IsUsingVendorAPI();
#endif
#if USE_AMD_SHADOW_LIB
		// TBD: add AMD shadows to the settings
		m_enableAMDShadows = CSettingsManager::GetInstance().GetSettings().m_graphics.m_Shadow_SoftShadows == CSettings::Custom;
		m_enableAMDShadows &= GRCDEVICE.IsUsingVendorAPI();
#endif

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
#if RSG_PC
		m_softShadowEnabled = AreSoftShadowsAllowed();
#else
		m_softShadowEnabled = false;
#endif
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING
	}
	ASSET.PopFolder();

	// global technique groups
	m_techniqueGroupId = grmShaderFx::FindTechniqueGroupId("wdcascade");

#if __PS3
	m_techniqueGroupId_edge = grmShaderFx::FindTechniqueGroupId("wdcascadeedge");
	m_techniqueTextureGroupId_edge = grmShaderFx::FindTechniqueGroupId("shadtexture_wdcascadeedge");
	m_techniqueTextureGroupId = -1;
#else // not __PS3
	m_techniqueTextureGroupId = grmShaderFx::FindTechniqueGroupId("shadtexture_wdcascade");
#endif // not __PS3


// init deferred/forward lighting
	m_deferredLightingShaderVars.InitResources(deferredLightingShader);
	m_sharedShaderVars.InitResources();
}

void CCSMGlobals::InitResources(grmShader* deferredLightingShader)
{
	USE_MEMBUCKET(MEMBUCKET_RENDER);

	// init shaders
	{

		for (int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
		{
			m_shadowCastingShader_RS[CSM_RS_DEFAULT   ][i].Init(grcRSV::CULL_BACK);
			m_shadowCastingShader_RS[CSM_RS_TWO_SIDED ][i].Init(grcRSV::CULL_NONE);
			m_shadowCastingShader_RS[CSM_RS_CULL_FRONT][i].Init(grcRSV::CULL_FRONT);
		}

		for(int j=0; j<NUMBER_OF_RENDER_THREADS; j++)
		{
			for (int i = 0; i < CSM_RS_COUNT; i++)
			{
				m_shadowCastingShader_RS_current[j][i] = grcStateBlock::RS_Invalid;
			}
		}

		grcBlendStateDesc reveal_bs;
		reveal_bs.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALPHA;
		reveal_bs.BlendRTDesc[0].BlendEnable           = true; // standard BS_Add, but with a write mask of ALPHA
		reveal_bs.BlendRTDesc[0].SrcBlend              = grcRSV::BLEND_ZERO;
		reveal_bs.BlendRTDesc[0].DestBlend             = grcRSV::BLEND_SRCCOLOR;
		reveal_bs.BlendRTDesc[0].BlendOp               = grcRSV::BLENDOP_ADD;
		reveal_bs.BlendRTDesc[0].SrcBlendAlpha         = grcRSV::BLEND_ZERO;
		reveal_bs.BlendRTDesc[0].DestBlendAlpha        = grcRSV::BLEND_SRCALPHA;
		reveal_bs.BlendRTDesc[0].BlendOpAlpha          = grcRSV::BLENDOP_ADD;
		m_shadowRevealShaderState_BS                   = grcStateBlock::CreateBlendState(reveal_bs);

		reveal_bs.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		m_shadowRevealShaderState_SoftShadow_BS			= grcStateBlock::CreateBlendState(reveal_bs);

#if __PS3 || __XENON
		grcDepthStencilStateDesc stencil_ds;

		stencil_ds.DepthEnable                  = true;
		stencil_ds.DepthWriteMask               = false;
		stencil_ds.DepthFunc                    = grcRSV::CMP_LESS; // DEFAULT
		stencil_ds.StencilEnable                = true;
		stencil_ds.StencilWriteMask             = 0;
		stencil_ds.StencilReadMask              = 0xFF; // DEFAULT
		stencil_ds.FrontFace.StencilFailOp      = grcRSV::STENCILOP_KEEP; // DEFAULT
		stencil_ds.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP; // DEFAULT
		stencil_ds.FrontFace.StencilPassOp      = grcRSV::STENCILOP_KEEP; // DEFAULT
		stencil_ds.FrontFace.StencilFunc        = grcRSV::CMP_NOTEQUAL;
		stencil_ds.BackFace                     = stencil_ds.FrontFace;
	
		m_shadowRevealShaderState_DS            = grcStateBlock::CreateDepthStencilState(stencil_ds, "m_shadowRevealShaderState_DS");
#else
		grcDepthStencilStateDesc stencil_ds;
		stencil_ds.DepthWriteMask = false;
		stencil_ds.DepthFunc = grcRSV::CMP_ALWAYS;
		stencil_ds.DepthEnable = false;
		stencil_ds.StencilEnable = false;
		stencil_ds.StencilWriteMask = 0;
#if (RSG_ORBIS || RSG_DURANGO) && DEPTH_BOUNDS_SUPPORT
		stencil_ds.DepthBoundsEnable			= true;
		stencil_ds.DepthEnable = true;
#endif
		m_shadowRevealShaderState_DS            = grcStateBlock::CreateDepthStencilState(stencil_ds, "m_shadowRevealShaderState_DS");

#if (RSG_ORBIS || RSG_DURANGO) && DEPTH_BOUNDS_SUPPORT && __BANK
		stencil_ds.DepthEnable = false;
		stencil_ds.DepthBoundsEnable			= false;
		m_shadowRevealShaderState_DS_noDepthBounds = grcStateBlock::CreateDepthStencilState(stencil_ds, "m_shadowRevealShaderState_DS_noDepthBounds");
	#if MSAA_EDGE_PASS
		stencil_ds.StencilEnable                = true;
		stencil_ds.StencilReadMask = EDGE_FLAG;
		stencil_ds.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
		stencil_ds.BackFace.StencilFunc = grcRSV::CMP_EQUAL;
		m_shadowRevealShaderState_DS_noDepthBounds_edge = grcStateBlock::CreateDepthStencilState(stencil_ds, "m_shadowRevealShaderState_DS_noDepthBounds_edge");
	#endif //MSAA_EDGE_PASS
#endif 

#endif

	}

	InitShaders(deferredLightingShader);

#if CASCADE_SHADOWS_STENCIL_REVEAL
	{
		m_volumeVertexBuffer.InitResources(256);
	}
#endif // CASCADE_SHADOWS_STENCIL_REVEAL

#if USE_AMD_SHADOW_LIB
	if (GRCDEVICE.GetDxFeatureLevel() >= 1100)
	{
		// DSS - no depth test and no depth write
		// DSS - stencil test enabled
		grcDepthStencilStateDesc amdShadowRevealDSSDesc;
		amdShadowRevealDSSDesc.DepthEnable                  = FALSE;
		amdShadowRevealDSSDesc.DepthWriteMask               = FALSE;
		amdShadowRevealDSSDesc.DepthFunc                    = rage::FixupDepthDirection(grcRSV::CMP_LESS);
		amdShadowRevealDSSDesc.StencilEnable                = TRUE;
		amdShadowRevealDSSDesc.StencilReadMask              = AMD_CASCADE_FLAG;
		amdShadowRevealDSSDesc.StencilWriteMask             = AMD_CASCADE_FLAG;
		amdShadowRevealDSSDesc.BackFace.StencilFunc         = grcRSV::CMP_EQUAL;
		amdShadowRevealDSSDesc.BackFace.StencilFailOp       = grcRSV::STENCILOP_KEEP;
		amdShadowRevealDSSDesc.BackFace.StencilPassOp       = grcRSV::STENCILOP_KEEP;
		amdShadowRevealDSSDesc.BackFace.StencilDepthFailOp  = grcRSV::STENCILOP_KEEP;
		amdShadowRevealDSSDesc.FrontFace                    = amdShadowRevealDSSDesc.BackFace;
		m_amdShadowRevealShaderState_DS = grcStateBlock::CreateDepthStencilState(amdShadowRevealDSSDesc, "m_amdShadowRevealShaderState_DS");

		// DSS - no depth test and no depth write
		// DSS - clear stencil
		grcDepthStencilStateDesc amdStencilClearDSSDesc;
		amdStencilClearDSSDesc.DepthEnable                  = FALSE;
		amdStencilClearDSSDesc.DepthWriteMask               = FALSE;
		amdStencilClearDSSDesc.DepthFunc                    = rage::FixupDepthDirection(grcRSV::CMP_LESS);
		amdStencilClearDSSDesc.StencilEnable                = TRUE;
		amdStencilClearDSSDesc.StencilReadMask              = AMD_CASCADE_FLAG;
		amdStencilClearDSSDesc.StencilWriteMask             = AMD_CASCADE_FLAG;
		amdStencilClearDSSDesc.BackFace.StencilFunc         = grcRSV::CMP_ALWAYS;
		amdStencilClearDSSDesc.BackFace.StencilFailOp       = grcRSV::STENCILOP_ZERO;
		amdStencilClearDSSDesc.BackFace.StencilPassOp       = grcRSV::STENCILOP_ZERO;
		amdStencilClearDSSDesc.BackFace.StencilDepthFailOp  = grcRSV::STENCILOP_KEEP;
		amdStencilClearDSSDesc.FrontFace                    = amdStencilClearDSSDesc.BackFace;
		m_amdStencilClearShaderState_DS = grcStateBlock::CreateDepthStencilState(amdStencilClearDSSDesc, "m_amdStencilClearShaderState_DS");

		// DSS - depth test enabled but no depth write
		// DSS - stencil write but not real test (two-sided stencil to mark current cascade)
		grcDepthStencilStateDesc amdStencilMarkDSSDesc;
		amdStencilMarkDSSDesc.DepthEnable                  = TRUE;
		amdStencilMarkDSSDesc.DepthWriteMask               = FALSE;
		amdStencilMarkDSSDesc.DepthFunc                    = rage::FixupDepthDirection(grcRSV::CMP_LESS);
		amdStencilMarkDSSDesc.StencilEnable                = TRUE;
		amdStencilMarkDSSDesc.StencilReadMask              = AMD_CASCADE_FLAG;
		amdStencilMarkDSSDesc.StencilWriteMask             = AMD_CASCADE_FLAG;
		amdStencilMarkDSSDesc.BackFace.StencilFunc         = grcRSV::CMP_ALWAYS;
		amdStencilMarkDSSDesc.BackFace.StencilFailOp       = grcRSV::STENCILOP_KEEP;
		amdStencilMarkDSSDesc.BackFace.StencilPassOp       = grcRSV::STENCILOP_KEEP;
		amdStencilMarkDSSDesc.BackFace.StencilDepthFailOp  = grcRSV::STENCILOP_INVERT;
		amdStencilMarkDSSDesc.FrontFace                    = amdStencilMarkDSSDesc.BackFace;
		m_amdStencilMarkShaderState_DS = grcStateBlock::CreateDepthStencilState(amdStencilMarkDSSDesc, "m_amdStencilMarkShaderState_DS");
	}
#endif
#if VARIABLE_RES_SHADOWS
	ReComputeShadowRes();
#endif // VARIABLE_RES_SHADOWS
	CreateRenderTargets();
#if RSG_PC
	m_initiailized = true;
	memset(m_CachedDepthBias, 0, sizeof(m_CachedDepthBias));
#endif
}

#define smoothStepRTSize 128

void CCSMGlobals::CreateRenderTargets()
{
#if __BANK
	LightProbe::CreateRenderTargetsAndInitShader();
#endif // __BANK

	// create rendertargets
	{
		USE_MEMBUCKET(MEMBUCKET_RENDER);

		int iWidth		= VideoResManager::GetSceneWidth();
		int iHeight		= VideoResManager::GetSceneHeight();

		const int shadowResX = CRenderPhaseCascadeShadowsInterface::CascadeShadowResX();
#if CASCADE_SHADOW_TEXARRAY
		const int shadowResY = CRenderPhaseCascadeShadowsInterface::CascadeShadowResY();
#else
		const int shadowResY = CRenderPhaseCascadeShadowsInterface::CascadeShadowResY()*CASCADE_SHADOWS_COUNT;
#endif

		if (1) // create depth target (shadow texture)
		{
			grcTextureFactory::CreateParams params;
			int bpp = 0;

			params.UseHierZ      = true;
#if __PS3
			params.InTiledMemory = true;
#elif __XENON
			params.HasParent     = true;
			params.Parent        = NULL;
#endif // __XENON

			bpp           = 32;
#if !RSG_PC
			params.Format = grctfD24S8;
			if (PARAM_use32BitShadowBuffer.Get())
				params.Format = grctfD32F;
#else
			params.Format = grctfD32F;
			g_RasterStateUpdateId++;
			m_depthBiasPrecisionScale = Vec4V(V_ONE);
			m_slopeBiasPrecisionScale = 1.0f;

			if(BANK_ONLY(gCascadeShadows.m_debug.m_modulateDepthSlopEnabled &&) WIN32PC_ONLY(!gCascadeShadows.m_enableNvidiaShadows &&) CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality > CSettings::High)
			{
				m_depthBiasPrecisionScale = Vec4V(1.0f, 0.8f, 0.6f, 0.4f);
				m_slopeBiasPrecisionScale = 0.6f;
			}			

			g_RasterCacheThreshold = 0.0001f;
			if(BANK_SWITCH(CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality == CSettings::Medium || PARAM_use16BitShadowBuffer.Get(), CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality == CSettings::Medium))
			{
				g_RasterCacheThreshold = 0.000001f;
				m_depthBiasPrecisionScale = Vec4V(0.018f, 0.01f, 0.01f, 0.012f );
				params.Format = grctfD16;
				g_RasterStateUpdateId++;
			}
#endif

#if RSG_ORBIS
			params.EnableFastClear = true;
#endif //RSG_ORBIS
#if __XENON
			const int poolSize = CRenderTargets::GetRenderTargetMemPoolSize(shadowResX, shadowResY);
			CRenderTargets::AllocRenderTargetMemPool(poolSize, XENON_RTMEMPOOL_SHADOWS, kShadowHeapCount);
#endif // __XENON

			const grcRenderTargetType type = (RSG_PC || RSG_DURANGO /*|| RSG_ORBIS*/) ? grcrtPermanent : grcrtShadowMap;
			const RTMemPoolID         pool = XENON_PS3_SWITCH(XENON_RTMEMPOOL_SHADOWS, PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_Z32_SEPSTENCIL_REGULAR, RTMEMPOOL_NONE);
			const u8                  heap = XENON_SWITCH(kShadowMapHeap, 0);

#if CASCADE_SHADOW_TEXARRAY
			params.ArraySize = 4;
			params.PerArraySliceRTV = PARAM_useInstancedShadows.Get() ? 0 : params.ArraySize;
#endif

			m_shadowMapBase = CRenderTargets::CreateRenderTarget(
				pool,
				"SHADOW_MAP_CSM_DEPTH",
				type,
				shadowResX,
				shadowResY,
				bpp,
				&params,
				heap
				WIN32PC_ONLY(,m_shadowMapBase)
			);
			Assert(m_shadowMapBase);

#if RSG_ORBIS
			params.EnableFastClear = false;
#endif //RSG_ORBIS
#if USE_NV_SHADOW_LIB
			if (GRCDEVICE.GetDxFeatureLevel() >= 1100  && GRCDEVICE.IsUsingVendorAPI())
			{
				ResetNVidia();

				NV_ShadowLib_BufferDesc shadowBufDesc;
				shadowBufDesc.uResolutionWidth  = iWidth;
				shadowBufDesc.uResolutionHeight = iHeight;
				if(m_shadowBufferHandle)
				{
					NV_ShadowLib_RemoveBuffer(&g_shadowContext, &m_shadowBufferHandle);
					m_shadowBufferHandle = NULL;
				}
				ASSERT_ONLY(NV_ShadowLib_Status shadowStatus =) NV_ShadowLib_AddBuffer(&g_shadowContext, &shadowBufDesc, &m_shadowBufferHandle);
				Assert(shadowStatus == NV_ShadowLib_Status_Ok);		// TBD: real error handling.  The compiler insists that the status value be used.
				m_mapDesc.Desc.uResolutionWidth = shadowResX;
				m_mapDesc.Desc.uResolutionHeight = shadowResY;
				m_mapDesc.Desc.eMapType = NV_ShadowLib_MapType_Texture;			// I.e. not array.
				m_mapDesc.Desc.eViewType = NV_ShadowLib_ViewType_Cascades_4;

				for (int i=0; i!=CASCADE_SHADOWS_COUNT; ++i)
				{
					m_mapDesc.Desc.ViewLocation[i].uMapID = 0;
					m_mapDesc.Desc.ViewLocation[i].v2Dimension.x = static_cast<float>(CRenderPhaseCascadeShadowsInterface::CascadeShadowResX());
					m_mapDesc.Desc.ViewLocation[i].v2Dimension.y = static_cast<float>(CRenderPhaseCascadeShadowsInterface::CascadeShadowResY());
					m_mapDesc.Desc.ViewLocation[i].v2TopLeft.x = 0.0f;
					m_mapDesc.Desc.ViewLocation[i].v2TopLeft.y = static_cast<float>(i * CRenderPhaseCascadeShadowsInterface::CascadeShadowResY());	// First guess; could be wrong.
					Assert(m_mapDesc.Desc.ViewLocation[i].v2TopLeft.y < shadowResY);
				}
			}
#endif
#if USE_AMD_SHADOW_LIB
			if (GRCDEVICE.GetDxFeatureLevel() >= 1100)
			{
				g_amdShadowDesc.m_Parameters.m_DepthSize.x = (float)iWidth;
				g_amdShadowDesc.m_Parameters.m_DepthSize.y = (float)iHeight;
				g_amdShadowDesc.m_Parameters.m_DepthSize.z = 1.0f / (float)iWidth;
				g_amdShadowDesc.m_Parameters.m_DepthSize.w = 1.0f / (float)iHeight;

				g_amdShadowDesc.m_Parameters.m_ShadowSize.x = (float)shadowResX;
				g_amdShadowDesc.m_Parameters.m_ShadowSize.y = (float)shadowResY;
				g_amdShadowDesc.m_Parameters.m_ShadowSize.z = 1.0f / shadowResX;
				g_amdShadowDesc.m_Parameters.m_ShadowSize.w = 1.0f / shadowResY;

				const int settingsIndex = Min(shadowResX/768, 2);  // 512 -> 0, 1024 -> 1, 2048 -> 2
				const int maxFilter = GRCDEVICE.GetManufacturer() == NVIDIA ? 2 : 3;

				m_amdCascadeRegionBorder = AMDCSMStatic::AMDCascadeBorder[settingsIndex];
				m_amdSunWidth[0] = AMDCSMStatic::AMDSunWidth[settingsIndex][0];
				m_amdSunWidth[1] = AMDCSMStatic::AMDSunWidth[settingsIndex][1];
				m_amdSunWidth[2] = AMDCSMStatic::AMDSunWidth[settingsIndex][2];
				m_amdSunWidth[3] = AMDCSMStatic::AMDSunWidth[settingsIndex][3];
				m_amdDepthOffset[0] = m_amdDepthOffset[1] = m_amdDepthOffset[2] = m_amdDepthOffset[3] = 0.000001f;
				m_amdFilterKernelIndex[0] = Min(AMDCSMStatic::AMDFilterRadiusIndex[settingsIndex][0], maxFilter);
				m_amdFilterKernelIndex[1] = Min(AMDCSMStatic::AMDFilterRadiusIndex[settingsIndex][1], maxFilter);
				m_amdFilterKernelIndex[2] = Min(AMDCSMStatic::AMDFilterRadiusIndex[settingsIndex][2], maxFilter);
				m_amdFilterKernelIndex[3] = Min(AMDCSMStatic::AMDFilterRadiusIndex[settingsIndex][3], maxFilter);

				if (GRCDEVICE.GetMSAA())
				{
					grcTextureFactory::CreateParams amdParams;
					amdParams.Format = grctfL8;
					m_amdShadowResult = grcTextureFactory::GetInstance().CreateRenderTarget(
						"AMD_CONTACT_HARDENING_SHADOWS_RESULT", grcrtPermanent, iWidth, iHeight, 8, &amdParams, m_amdShadowResult);
				}
			}
#endif //USE_AMD_SHADOW_LIB

#if RMPTFX_USE_PARTICLE_SHADOWS
#if __D3D11
#if RSG_PC
			if (m_shadowMapReadOnly) {delete m_shadowMapReadOnly; m_shadowMapReadOnly = NULL;}
#endif
			grcTextureFactoryDX11& textureFactory11 = static_cast<grcTextureFactoryDX11&>(grcTextureFactoryDX11::GetInstance());
			m_shadowMapReadOnly = textureFactory11.CreateRenderTarget("SHADOW_MAP_CSM_DEPTH_ReadOnly", m_shadowMapBase->GetTexturePtr(), params.Format, DepthStencilReadOnly, NULL, &params);
#elif RSG_ORBIS
			grcTextureFactoryGNM& textureFactoryGNM = static_cast<grcTextureFactoryGNM&>(grcTextureFactoryGNM::GetInstance());
			m_shadowMapReadOnly = textureFactoryGNM.CreateRenderTarget("SHADOW_MAP_CSM_DEPTH_ReadOnly", m_shadowMapBase, params.Format);
#endif
#endif // RMPTFX_USE_PARTICLE_SHADOWS && __D3D11

#if CASCADE_SHADOW_TEXARRAY
			params.ArraySize = 1;
			params.PerArraySliceRTV = 0;
#endif

			if (pool != RTMEMPOOL_NONE)
			{
				m_shadowMapBase->AllocateMemoryFromPool();
			}

#if __PS3
			if (bpp == 32)
			{
				m_shadowMapPatched = ((grcRenderTargetGCM*)m_shadowMapBase)->CreatePrePatchedTarget(false);
			}
			else
			{
				m_shadowMapPatched = NULL;
			}
#endif // __PS3

			CreateShadowMapMini();
		}

#if CSM_USE_DUMMY_COLOUR_TARGET
		{
			grcTextureFactory::CreateParams params;
			const int bpp = 32;

			params.Format = grctfA8R8G8B8;

			const grcRenderTargetType type = grcrtPermanent;
			const u8                  heap = 0;

			m_shadowMapDummy = CRenderTargets::CreateRenderTarget(
				RTMEMPOOL_NONE,
				"SHADOW_MAP_CSM_DUMMY",
				type,
				shadowResX,
				shadowResY,
				bpp,
				&params,
				heap
				WIN32PC_ONLY(, m_shadowMapDummy)
			);
			Assert(m_shadowMapDummy);
		}
#endif // CSM_USE_DUMMY_COLOUR_TARGET

#if CSM_USE_SPECIAL_FULL_TEXTURE
#if RSG_PC
		if (!m_shadowMapSpecialFull)
#endif
		{
			grcImage *const image = grcImage::Create(1, 1, 1, grcImage::R32F, grcImage::DEPTH, 0, 0);
			
			union ClearDepth{
				u32   m_UnsignedValue;
				float m_FloatValue;
			} clearDepth;		
			clearDepth.m_FloatValue = GRCDEVICE.FixDepth(0.0f);
			image->SetPixel(0, 0, clearDepth.m_UnsignedValue);

			BANK_ONLY(grcTexture::SetCustomLoadName("shadowMapSpecialFull");)
			m_shadowMapSpecialFull = grcTextureFactory::GetInstance().Create(image);
			BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
			Assert(m_shadowMapSpecialFull);
			image->Release();
		}
#endif // CSM_USE_SPECIAL_FULL_TEXTURE

#if RSG_PC
		m_enableParticleShadows = m_enableAlphaShadows = CSettingsManager::GetInstance().GetSettings().m_graphics.IsParticleShadowsEnabled();
#endif

#if RMPTFX_USE_PARTICLE_SHADOWS
		if (m_enableParticleShadows)
		{
			grcTextureFactory::CreateParams params;
			const int bpp = 32;
			params.Format = grctfA8;

			const grcRenderTargetType type = grcrtPermanent;
			const u8                  heap = 0;

#if CASCADE_SHADOW_TEXARRAY
			params.ArraySize = 4;
			params.PerArraySliceRTV = PARAM_useInstancedShadows.Get() ? 0 : params.ArraySize;
#endif
#if DYNAMIC_GB
			params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_SHADOWS;
#endif
			m_particleShadowMap = CRenderTargets::CreateRenderTarget(
				RTMEMPOOL_NONE,
				"SHADOW_MAP_PARTICLES",
				type,
				shadowResX,
				shadowResY,
				bpp,
				&params,
				heap
				WIN32PC_ONLY(,m_particleShadowMap)
			);
#if CASCADE_SHADOW_TEXARRAY
			params.ArraySize = 1;
			params.PerArraySliceRTV = 0;
#endif
			Assert(m_particleShadowMap);
		}
#endif // RMPTFX_USE_PARTICLE_SHADOWS

#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
		{
			grcTextureFactory::CreateParams params;
			const int bpp = 32; // could be 32 or 16

			params.Format = (bpp == 32) ? grctfA8R8G8B8 : grctfR5G6B5;

			const grcRenderTargetType type = grcrtPermanent;
			const u8                  heap = 0;

			m_shadowMapEntityIDs = CRenderTargets::CreateRenderTarget(
				RTMEMPOOL_NONE,
				"SHADOW_MAP_CSM_ENTITY_ID",
				type,
				shadowResX,
				shadowResY,
				bpp,
				&params,
				heap
				WIN32PC_ONLY(, m_shadowMapEntityIDs)
			);
			Assert(m_shadowMapEntityIDs);
		}
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
		if (AreSoftShadowsAllowed())
		{
			grcTextureFactory::CreateParams params;
			const int bpp = 8;

#if (RSG_PC || RSG_DURANGO || RSG_ORBIS)
			params.Format = grctfG8R8;
			if ( GRCDEVICE.GetMSAA() > 1 )
			{
				params.Multisample = GRCDEVICE.GetMSAA();
			}
			else
			{
				params.Multisample = grcDevice::MSAAMode(0);
			}
# if RSG_ORBIS
			params.EnableFastClear = false;
# endif // RSG_ORBIS
# if DEVICE_EQAA
			params.Multisample.DisableCoverage();
# else 
			params.MultisampleQuality = (u8)GRCDEVICE.GetMSAAQuality();
# endif // DEVICE_EQAA
#else // not (RSG_PC || RSG_DURANGO || RSG_ORBIS)
			// TODO:- Use a better format on these platforms.
			params.Format = grctfA8R8G8B8;
#endif // not (RSG_PC || RSG_DURANGO || RSG_ORBIS)

			const grcRenderTargetType type = grcrtPermanent;
			const u8                  heap = 0;

			m_softShadowIntermediateRT = CRenderTargets::CreateRenderTarget(
				RTMEMPOOL_NONE,
				"SHADOW_MAP_CSM_SOFT_SHADOW_INTERMEDIATE1",
				type,
				iWidth,
				iHeight,
				bpp,
				&params,
				heap
				WIN32PC_ONLY(, m_softShadowIntermediateRT)
			);
			Assert(m_softShadowIntermediateRT);

#if DEVICE_EQAA && 0	//we don't need a full-coverage resolve here
			static_cast<grcRenderTargetMSAA*>( m_softShadowIntermediateRT )->SetFragmentMaskDonor(
				static_cast<grcRenderTargetMSAA*>( GBuffer::GetTarget(GBUFFER_RT_0) ), ResolveSW_NoAnchors );
#endif // DEVICE_EQAA
			params.Multisample = 0;

#if DEVICE_MSAA			
			if (GRCDEVICE.GetMSAA()>1)
			{
				m_softShadowIntermediateRT_Resolved = CRenderTargets::CreateRenderTarget(
					RTMEMPOOL_NONE,
					"SHADOW_MAP_CSM_SOFT_SHADOW_INTERMEDIATE1_RESOLVED",
					type,
					iWidth,
					iHeight,
					bpp,
					&params,
					heap
					WIN32PC_ONLY(, m_softShadowIntermediateRT_Resolved)
				);
				Assert(m_softShadowIntermediateRT_Resolved);
			}
			else
			{
				if (m_softShadowIntermediateRT_Resolved) {delete m_softShadowIntermediateRT_Resolved; m_softShadowIntermediateRT_Resolved = NULL;}
			}
#endif	//DEVICE_MSAA

			params.Format = grctfL8;

			m_softShadowEarlyOut1RT = CRenderTargets::CreateRenderTarget(
				RTMEMPOOL_NONE,
				"SHADOW_MAP_CSM_SOFT_SHADOW_EARLY_OUT1",
				type,
				iWidth/8,
				iHeight/8,
				bpp,
				&params,
				heap
				WIN32PC_ONLY(,m_softShadowEarlyOut1RT)
			);
			Assert(m_softShadowEarlyOut1RT);

			m_softShadowEarlyOut2RT = CRenderTargets::CreateRenderTarget(
				RTMEMPOOL_NONE,
				"SHADOW_MAP_CSM_SOFT_SHADOW_EARLY_OUT2",
				type,
				iWidth/8,
				iHeight/8,
				bpp,
				&params,
				heap
				WIN32PC_ONLY(,m_softShadowEarlyOut2RT)
			);
			Assert(m_softShadowEarlyOut2RT);
		}
		else
		{
			m_softShadowIntermediateRT = NULL;
			m_softShadowEarlyOut1RT = NULL;
			m_softShadowEarlyOut2RT = NULL;
#if DEVICE_MSAA

			if (m_softShadowIntermediateRT_Resolved) {delete m_softShadowIntermediateRT_Resolved; m_softShadowIntermediateRT_Resolved = NULL;}
#endif	//DEVICE_MSAA
		}
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING

#if RSG_PC
		if(!m_ditherTexture)
#endif
		{
			m_ditherTexture = CShaderLib::LookupTexture("shadowdither");
			Assert(m_ditherTexture);
			if(m_ditherTexture)
				m_ditherTexture->AddRef();
		}

		// smooth-step RT
		{
			grcTextureFactory::CreateParams params;
			params.Format    = grctfL8;
#if __XENON
			params.HasParent = true;
			params.Parent    = GBuffer::GetTiledTarget(GBUFFER_RT_2);
#endif // __XENON
			m_smoothStepRT = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Smooth Step RT", grcrtPermanent, smoothStepRTSize, 1, 8, &params, 0 WIN32PC_ONLY(, m_smoothStepRT));
		}
	}
}

#if RSG_PC
void CCSMGlobals::DestroyRenderTargets()
{
	// create rendertargets
	{
#if USE_NV_SHADOW_LIB
		if (GRCDEVICE.GetDxFeatureLevel() >= 1100)
		{
			NV_ShadowLib_RemoveBuffer(&g_shadowContext, &m_shadowBufferHandle);
			m_shadowBufferHandle = NULL;
		}
#endif
		if (m_shadowMapBase) {delete m_shadowMapBase; m_shadowMapBase = NULL;}

#if RMPTFX_USE_PARTICLE_SHADOWS && __D3D11
		if (m_shadowMapReadOnly) {delete m_shadowMapReadOnly; m_shadowMapReadOnly = NULL;}
#endif // RMPTFX_USE_PARTICLE_SHADOWS && __D3D11

		if (m_shadowMapMini) {delete m_shadowMapMini; m_shadowMapMini = NULL;}// From CreateShadowMapMini();

#if CSM_USE_DUMMY_COLOUR_TARGET
		if (m_shadowMapDummy) {delete m_shadowMapDummy; m_shadowMapDummy = NULL;}
#endif // CSM_USE_DUMMY_COLOUR_TARGET

#if RMPTFX_USE_PARTICLE_SHADOWS
		if (m_particleShadowMap) {delete m_particleShadowMap; m_particleShadowMap = NULL;}
#endif // RMPTFX_USE_PARTICLE_SHADOWS

#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
		if (m_shadowMapEntityIDs) {delete m_shadowMapEntityIDs; m_shadowMapEntityIDs = NULL;}
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
		if (m_softShadowIntermediateRT) {delete m_softShadowIntermediateRT; m_softShadowIntermediateRT = NULL;}
		if (m_softShadowEarlyOut1RT) {delete m_softShadowEarlyOut1RT; m_softShadowEarlyOut1RT = NULL;}
		if (m_softShadowEarlyOut2RT) {delete m_softShadowEarlyOut2RT; m_softShadowEarlyOut2RT = NULL;}
#if DEVICE_MSAA
		if (m_softShadowIntermediateRT_Resolved) {delete m_softShadowIntermediateRT_Resolved; m_softShadowIntermediateRT_Resolved = NULL;}
#endif	//DEVICE_MSAA
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING

		if (m_ditherTexture) {m_ditherTexture->DecRef(); m_ditherTexture = NULL;}

		// smooth-step RT
		if (m_smoothStepRT) {delete m_smoothStepRT; m_smoothStepRT = NULL;}
	}
}
#endif // RSG_PC

#if VARIABLE_RES_SHADOWS
PARAM(dx11shadowres,"");
void CCSMGlobals::ReComputeShadowRes()
{
	int res = 0;
	if (!PARAM_dx11shadowres.Get(res))
	{
#define SHADOW_RES_BASELINE 256
#if RSG_DURANGO
		res = SHADOW_RES_BASELINE << 2;	// Limit size to 1024 x 4096
#elif RSG_ORBIS
		res = SHADOW_RES_BASELINE << 2;
		int shadowQuality;
		if (PARAM_shadowQuality.Get(shadowQuality))
		{
			res = SHADOW_RES_BASELINE << shadowQuality;
		}
#else
		const Settings& settings = CSettingsManager::GetInstance().GetSettings();
		res = settings.m_graphics.m_ShadowQuality >=3 && settings.m_graphics.m_UltraShadows_Enabled ? 4096 : SHADOW_RES_BASELINE << settings.m_graphics.m_ShadowQuality;
		if (settings.m_graphics.m_ShadowQuality == (CSettings::eSettingsLevel) (0)) res = 1;
		res = Min(settings.m_graphics.m_DX_Version < 2 ? 2048 : 4096, res);
#endif
	}
	m_shadowResX = res;
	m_shadowResY = res;
}
#endif // VARIABLE_RES_SHADOWS

void CCSMGlobals::CreateShadowMapMini()
{
	const int resX = CASCADE_SHADOWS_RES_MINI_X;
	const int resY = CASCADE_SHADOWS_RES_MINI_Y;

	if (/*m_shadowMapMini == NULL && */resX != 0 && resY != 0)
	{
		// note that PS3 uses PCF, so it has to be a shadow map, whereas 360 does not
		grcTextureFactory::CreateParams params;
		int bpp = 0;

		params.Format   = PS3_SWITCH(grctfD24S8, grctfR32F);
		params.UseFloat = PS3_SWITCH(false, true);
		bpp             = 32;

		const grcRenderTargetType type = PS3_SWITCH(grcrtShadowMap, grcrtPermanent);
		const RTMemPoolID         pool = XENON_PS3_SWITCH(XENON_RTMEMPOOL_SHADOWS, PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_Z32_SEPSTENCIL_REGULAR, RTMEMPOOL_NONE);
		const u8                  heap = XENON_PS3_SWITCH(kPhotoHeap, 1, 0);

		m_shadowMapMini = CRenderTargets::CreateRenderTarget(
			pool,
			"SHADOW_MAP_CSM_MINI",
			type,
			resX,
			resY,
			bpp,
			&params,
			heap
			WIN32PC_ONLY(,m_shadowMapMini)
		);
		Assert(m_shadowMapMini);

		if (pool != RTMEMPOOL_NONE)
		{
			m_shadowMapMini->AllocateMemoryFromPool();
		}

#if __PS3 && __BANK
		if (bpp == 32) // only works for 32-bit shadow maps
		{
			m_shadowMapMiniPatched = ((grcRenderTargetGCM*)m_shadowMapMini)->CreatePrePatchedTarget(false);
		}
		else
		{
			m_shadowMapMiniPatched = NULL;
		}
#endif // __PS3 && __BANK
	}

	const int resX2 = CASCADE_SHADOWS_RES_MINI_X;
	const int resY2 = CASCADE_SHADOWS_RES_MINI_Y * 4;

	if (/*m_shadowMapMini2 == NULL &&*/ resX2 != 0 && resY2 != 0)
	{
		// note that PS3 uses PCF, so it has to be a shadow map, whereas 360 does not
		grcTextureFactory::CreateParams params;
		int bpp = 0;

		params.Format   = grctfR32F;
		params.UseFloat = true;
		bpp             = 32;

		const grcRenderTargetType type = grcrtPermanent;
		const RTMemPoolID         pool = XENON_PS3_SWITCH(XENON_RTMEMPOOL_SHADOWS, PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_Z32_SEPSTENCIL_REGULAR, RTMEMPOOL_NONE);
		const u8                  heap = XENON_PS3_SWITCH(kPhotoHeap, 1, 0);

		m_shadowMapMini2 = CRenderTargets::CreateRenderTarget(
			pool,
			"SHADOW_MAP_CSM_MINI 2",
			type,
			resX2,
			resY2,
			bpp,
			&params,
			heap
			WIN32PC_ONLY(, m_shadowMapMini2)
		);
		Assert(m_shadowMapMini2);

		if (pool != RTMEMPOOL_NONE)
		{
			m_shadowMapMini2->AllocateMemoryFromPool();
		}
	}
}

grcRenderTarget* CCSMGlobals::GetShadowMap()
{
	return m_shadowMapBase;
}

grcRenderTarget* CCSMGlobals::GetShadowMapVS()
{
	return m_shadowMapVS;
}

#if RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
grcRenderTarget* CCSMGlobals::GetAlphaShadowMap()
{
	return m_particleShadowMap;
}
#endif // RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW

#if SHADOW_ENABLE_ALPHASHADOW
bool CCSMGlobals::UseAlphaShadows()
{
	return m_enableAlphaShadows;
}
#endif // SHADOW_ENABLE_ALPHASHADOW

#if __D3D11 || RSG_ORBIS
grcRenderTarget* CCSMGlobals::GetShadowMapReadOnly()
{
	return m_shadowMapReadOnly;
}
#endif // __D3D11 || RSG_ORBIS

#if __PS3

grcRenderTarget* CCSMGlobals::GetShadowMapPatched()
{
	return m_shadowMapPatched;
}

#endif // __PS3

grcRenderTarget* CCSMGlobals::GetShadowMapMini(ShadowMapMiniType mapType)
{
	if (mapType == SM_MINI_UNDERWATER)
		return m_shadowMapMini;
	else if (mapType == SM_MINI_FOGRAY)
		return m_shadowMapMini2;
	else
	{
		Assertf(0, "not avlid shadow map mini.\n");
		return NULL;
	}
}

const grcTexture* CCSMGlobals::GetSmoothStepTexture()
{
	return AreCloudShadowsEnabled() ? m_smoothStepRT : grcTexture::NoneWhite;
}

enum // passes
{
	shadowprocessing_CopyToShadowMapVS,
	shadowprocessing_CopyToShadowMapMini,
#if CASCADE_SHADOWS_STENCIL_REVEAL
	shadowprocessing_StencilRevealDebug,
#endif // CASCADE_SHADOWS_STENCIL_REVEAL
};

void CCSMGlobals::CopyToShadowMapVS()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	const grcRenderTarget* dst = m_shadowMapVS;
	const grcTexture*      src = PS3_SWITCH(m_shadowMapPatched, m_shadowMapBase);
	const ShadowProcessingSetup &setup = m_shadowProcessing[MM_DEFAULT];

	if (dst && src && src->GetBitsPerPixel() == 32)
	{
		grcTextureFactory::GetInstance().LockRenderTarget(0, dst, NULL);

		setup.shader->SetVar(setup.shadowMapId, src);

		grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);

		if (AssertVerify(setup.shader->BeginDraw(grmShader::RMC_DRAW, false, setup.technique)))
		{
			setup.shader->Bind(shadowprocessing_CopyToShadowMapVS);
			{
				const Color32 colour = Color32(0);

				const float x0 = -1.0f;
				const float y0 = +1.0f;
				const float x1 = +1.0f;
				const float y1 = -1.0f;
				const float z  =  0.0f;

				const float u0 = 0.0f;
				const float v0 = (float)(CASCADE_SHADOWS_VS_CASCADE_INDEX + 0)/(float)CASCADE_SHADOWS_COUNT;
				const float u1 = 1.0f;
				const float v1 = (float)(CASCADE_SHADOWS_VS_CASCADE_INDEX + 1)/(float)CASCADE_SHADOWS_COUNT;

				grcBegin(drawTriStrip, 4);
				grcVertex(x0, y0, z, 0.0f, 0.0f, -1.0f, colour, u0, v0);
				grcVertex(x0, y1, z, 0.0f, 0.0f, -1.0f, colour, u0, v1);
				grcVertex(x1, y0, z, 0.0f, 0.0f, -1.0f, colour, u1, v0);
				grcVertex(x1, y1, z, 0.0f, 0.0f, -1.0f, colour, u1, v1);
				grcEnd();
			}
			setup.shader->UnBind();
			setup.shader->EndDraw();
		}

		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	}
}

void CCSMGlobals::CopyShadowsToMiniMap(grcTexture* noiseMap, float height)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	const grcRenderTarget* dst = m_shadowMapMini;
	const grcTexture*      src = PS3_SWITCH(m_shadowMapPatched, m_shadowMapBase);
	const ShadowProcessingSetup &setup = m_shadowProcessing[MM_DEFAULT];

	if (dst && src && src->GetBitsPerPixel() == 32)
	{
		GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
		grcTextureFactory::GetInstance().LockRenderTarget(0, PS3_SWITCH(NULL, dst), PS3_SWITCH(dst, NULL));

		setup.shader->SetVar(setup.shadowMapId, src);
		setup.shader->SetVar(setup.waterNoiseMapId, noiseMap);
		setup.shader->SetVar(setup.waterHeightId, height);

		grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_Default, grcStateBlock::BS_Default);

		if (AssertVerify(setup.shader->BeginDraw(grmShader::RMC_DRAW, false, setup.technique)))
		{
			setup.shader->Bind(shadowprocessing_CopyToShadowMapMini);
			{
				const float x0 = -1.0f;
				const float y0 = -1.0f;
				const float x1 = +1.0f;
				const float y1 = +1.0f;
				const float z  =  0.0f;

				const float u0 = 0.0f;
				const float v0 = 0.0f;
				const float u1 = 1.0f;
				const float v1 = 1.0f;

				const Color32 colour = Color32(255,255,255,255);
				
				grcBegin(rage::drawTriStrip, 4);
				grcVertex(x0, y0, z, 0.0f, 0.0f, -1.0f, colour, u0, v0);
				grcVertex(x1, y0, z, 0.0f, 0.0f, -1.0f, colour, u1, v0);
				grcVertex(x0, y1, z, 0.0f, 0.0f, -1.0f, colour, u0, v1);
				grcVertex(x1, y1, z, 0.0f, 0.0f, -1.0f, colour, u1, v1);
				grcEnd();
			}
			setup.shader->UnBind();
			setup.shader->EndDraw();
		}

		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	}
}


static bool CSMShadowTypeIsHighAltitude(int shadowType)
{
	if (shadowType == CSM_TYPE_QUADRANT ||
		shadowType == CSM_TYPE_HIGHRES_SINGLE)
		return true;
	else
		return false;
}


void CCSMGlobals::AdjustSplitZ(float expWeight)
{
	if (expWeight != 0.0f)
	{
		const int N = CASCADE_SHADOWS_COUNT;

		const float z0 = m_cascadeSplitZ[0].Getf();
		const float zn = m_cascadeSplitZ[N].Getf();

		for (int cascadeIndex = 1; cascadeIndex < N; cascadeIndex++)
		{
			const float z_lin = m_cascadeSplitZ[cascadeIndex].Getf();

			const float z_exp = z0*powf(zn/z0, (z_lin - z0)/(zn - z0));

			m_cascadeSplitZ[cascadeIndex] = ScalarV(z_lin + (z_exp - z_lin)*expWeight);
		}
	}
}


void CCSMGlobals::CalcSplitZ(ScalarV (&splitZ)[CASCADE_SHADOWS_COUNT+1], bool allowHighAltitudeAdjust)
{
	const ScalarV splitZScale(m_distanceMultiplier); // timecycle adjust

	for (int i = 0; i <= CASCADE_SHADOWS_COUNT; i++)
	{
		splitZ[i] = splitZScale*m_cascadeSplitZ[i];
	}

	const bool bHighAltitude = CSMShadowTypeIsHighAltitude(m_scriptShadowType);

	if(bHighAltitude != g_HighAltitude)
	{
		g_RasterStateUpdateId++;
		g_HighAltitude = bHighAltitude;
	}

	if (bHighAltitude && allowHighAltitudeAdjust)
	{
		splitZ[CASCADE_SHADOWS_COUNT] = ScalarV(7000.0f);
	}
}


void CCSMGlobals::CalcFogRayFadeRange(float (&range)[2])
{
	ScalarV splitZ[CASCADE_SHADOWS_COUNT+1];
	// Don`t allow adjust for altitude, we don`t want fog ray to be active deep into scene.
	CalcSplitZ(splitZ, false);

	if(m_numCascades <= 1 )
	{
		range[0] = splitZ[1].Getf();
		range[1] = range[1] + 500.0f;
	}
	else
	{
		range[0] = splitZ[m_numCascades-1].Getf();
		range[1] = splitZ[m_numCascades].Getf();
	}
}


static Mat44V_Out CalcViewToWorldProjection(const grcViewport& viewport)
{
	const Vec4V deferredProjection = VECTOR4_TO_VEC4V(ShaderUtil::CalculateProjectionParams(&viewport));
	Mat44V viewToWorldProjection = Mat44V(viewport.GetCameraMtx());

	viewToWorldProjection.GetCol0Ref().SetW(deferredProjection.GetX());
	viewToWorldProjection.GetCol1Ref().SetW(deferredProjection.GetY());
	viewToWorldProjection.GetCol2Ref().SetW(deferredProjection.GetZ());
	viewToWorldProjection.GetCol3Ref().SetW(deferredProjection.GetW());

	return viewToWorldProjection;
}

// NOTE -- this is really only necessary if CSM_ASYMMETRICAL_VIEW is enabled, but the shader code relies on it ..
static Vec4V_Out CalcPerspectiveShear(const grcViewport& viewport)
{
	const Vector2 shear = viewport.GetPerspectiveShear();
	return Vec4V(shear.x, shear.y, 0.0f, 0.0f);
}

#if __DEV

static __forceinline Vec3V_Out ScreenToWorld(Mat34V_In viewToWorld, ScalarV_In tanHFOV, ScalarV_In tanVFOV, Vec3V_In p) // p is [-1..1] clipspace
{
	const ScalarV x = p.GetX();
	const ScalarV y = p.GetY();
	const ScalarV z = p.GetZ();

	return Transform(viewToWorld, Vec3V(tanHFOV*x, tanVFOV*y, ScalarV(V_NEGONE))*z);
}

static void CSMDebugDrawViewerPortal(Mat34V_In viewToWorld, ScalarV_In tanHFOV, ScalarV_In zNearClip, ScalarV_In tanVFOV, Vec4V_In portal)
{
	const Vec3V portal_p00 = ScreenToWorld(viewToWorld, tanHFOV, tanVFOV, GetFromTwo<Vec::X1,Vec::Y1,Vec::Z2>(portal, Vec4V(zNearClip)));
	const Vec3V portal_p10 = ScreenToWorld(viewToWorld, tanHFOV, tanVFOV, GetFromTwo<Vec::Z1,Vec::Y1,Vec::Z2>(portal, Vec4V(zNearClip)));
	const Vec3V portal_p01 = ScreenToWorld(viewToWorld, tanHFOV, tanVFOV, GetFromTwo<Vec::X1,Vec::W1,Vec::Z2>(portal, Vec4V(zNearClip)));
	const Vec3V portal_p11 = ScreenToWorld(viewToWorld, tanHFOV, tanVFOV, GetFromTwo<Vec::Z1,Vec::W1,Vec::Z2>(portal, Vec4V(zNearClip)));

	grcDebugDraw::Line(portal_p00, portal_p10, Color32(255,0,0,255));
	grcDebugDraw::Line(portal_p10, portal_p11, Color32(255,0,0,255));
	grcDebugDraw::Line(portal_p11, portal_p01, Color32(255,0,0,255));
	grcDebugDraw::Line(portal_p01, portal_p00, Color32(255,0,0,255));
}

#endif // __DEV

#if CASCADE_SHADOWS_SCAN_DEBUG

static void CSMDebugDrawSilhouettePolygon(CCSMState& state, const fwScanCSMSortedPlaneInfo& spi, int planeCount, float opacity)
{
	USE_DEBUG_MEMORY();

	//if (m_debug.m_showShadowMapCascadeIndex >= 2)
	{
		if (spi.m_silhouettePolyNumVerts >= 3)
		{
			const int    flags = (1<<(CASCADE_SHADOWS_COUNT - 1));
			const Vec3V* poly  = spi.m_silhouettePoly;

			for (int i = 0; i < spi.m_silhouettePolyNumVerts; i++)
			{
				state.m_debug.m_objects.PushAndGrow(rage_new CCSMDebugLine(Color32(255,255,0,255), opacity, flags, poly[i], poly[(i + 1)%spi.m_silhouettePolyNumVerts]));
			}

			for (int i = 1; i < spi.m_silhouettePolyNumVerts - 1; i++)
			{
				state.m_debug.m_objects.PushAndGrow(rage_new CCSMDebugTri(Color32(255,255,0,80), opacity, flags, poly[0], poly[i], poly[i + 1]));
			}
		}
	}

	int silhouettePlaneCount = 0;

	for (int i = 0; i < planeCount; i++) // count silhouette planes
	{
		if (spi.m_sortedPlanes[i].m_planeType > CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SILHOUETTE_FIRST &&
			spi.m_sortedPlanes[i].m_planeType < CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SILHOUETTE_LAST)
		{
			silhouettePlaneCount++;
		}
	}

	grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());
	grcDebugDraw::AddDebugOutput("CSM Optimised Silhouette Planes = %d/%d (%d unoptimised)", silhouettePlaneCount, planeCount, spi.m_unoptimisedPlaneCount);

	for (int i = 0; i < planeCount; i++)
	{
		const int   planeType   = spi.m_sortedPlanes[i].m_planeType;
		const float planeLength = spi.m_sortedPlanes[i].m_planeLength.Getf();
		const char* planeName   = "CLIP";

		if (planeType != INDEX_NONE)
		{
			switch (planeType)
			{
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SILHOUETTE_NX_NY     : planeName = STRING(SILHOUETTE_NX_NY    ); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SILHOUETTE_PX_NY     : planeName = STRING(SILHOUETTE_PX_NY    ); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SILHOUETTE_NX_PY     : planeName = STRING(SILHOUETTE_NX_PY    ); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SILHOUETTE_PX_PY     : planeName = STRING(SILHOUETTE_PX_PY    ); break;

			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SILHOUETTE_NX_PZ     : planeName = STRING(SILHOUETTE_NX_PZ    ); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SILHOUETTE_NY_PZ     : planeName = STRING(SILHOUETTE_NY_PZ    ); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SILHOUETTE_PX_PZ     : planeName = STRING(SILHOUETTE_PX_PZ    ); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SILHOUETTE_PY_PZ     : planeName = STRING(SILHOUETTE_PY_PZ    ); break;

			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SILHOUETTE_NX_GROUND : planeName = STRING(SILHOUETTE_NX_GROUND); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SILHOUETTE_NY_GROUND : planeName = STRING(SILHOUETTE_NY_GROUND); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SILHOUETTE_PX_GROUND : planeName = STRING(SILHOUETTE_PX_GROUND); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SILHOUETTE_PY_GROUND : planeName = STRING(SILHOUETTE_PY_GROUND); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SILHOUETTE_PZ_GROUND : planeName = STRING(SILHOUETTE_PZ_GROUND); break;

			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SHADOW_PORTAL_NX     : planeName = STRING(SHADOW_PORTAL_NX    ); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SHADOW_PORTAL_NY     : planeName = STRING(SHADOW_PORTAL_NY    ); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SHADOW_PORTAL_PX     : planeName = STRING(SHADOW_PORTAL_PX    ); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_SHADOW_PORTAL_PY     : planeName = STRING(SHADOW_PORTAL_PY    ); break;

			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_BACKFACING_NX        : planeName = STRING(BACKFACING_NX       ); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_BACKFACING_NY        : planeName = STRING(BACKFACING_NY       ); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_BACKFACING_PX        : planeName = STRING(BACKFACING_PX       ); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_BACKFACING_PY        : planeName = STRING(BACKFACING_PY       ); break;
			case CSM_SCAN_DEBUG_CLIP_PLANE_TYPE_BACKFACING_PZ        : planeName = STRING(BACKFACING_PZ       ); break;
			}
		}

		grcDebugDraw::AddDebugOutput(atVarString("        %.2f - %s", planeLength*100.0f, planeName).c_str());
	}

	grcDebugDraw::TextFontPop();
}

#endif // CASCADE_SHADOWS_SCAN_DEBUG

static bool CSMShadowTypeIsCascade(int shadowType)
{
	if (shadowType == CSM_TYPE_CASCADES)
		return true;
	return false;
}

#if __BANK

static void CSMShowActiveInfo()
{
	grcDebugDraw::AddDebugOutput(
		"CSM Active = %s%s",
		gCascadeShadows.m_shadowIsActive ? "TRUE" : "FALSE",
		fwScan::GetScanResults().IsEverythingInShadow() ? " (everything in shadow)" : ""
	);
}

static void CSMShowInfo(const CCSMState& state, const ScalarV worldHeight[2], const ScalarV recvrHeight[2])
{
	const Vec3V shadowDir = state.m_shadowToWorld33.GetCol2();

	const float sx = shadowDir.GetXf();
	const float sy = shadowDir.GetYf();
	const float sz = shadowDir.GetZf();

	CSMShowActiveInfo();

	grcDebugDraw::AddDebugOutput(
		"CSM Sun Angle = %.2f degrees (heading = %.2f degrees, xyz = %.4f,%.4f,%.4f)",
		RtoD*asinf(-sz),
		(Abs<float>(sz) < 0.98f) ? RtoD*atan2f(-sx, sy) : 0.0f,
		sx,
		sy,
		sz
	);

	const grcViewport* vp = gVpMan.GetUpdateGameGrcViewport();

	grcDebugDraw::AddDebugOutput(
		"CSM HFOV = %.2f, VFOV = %.2f degrees, (znear = %f, zfar = %f)",
		RtoD*2.0f*atanf(state.m_viewToWorldProjection.GetCol0().GetWf()),
		RtoD*2.0f*atanf(state.m_viewToWorldProjection.GetCol1().GetWf()),
		vp->GetNearClip(),
		vp->GetFarClip()
	);

	grcDebugDraw::AddDebugOutput(
		"CSM World Height = [%.2f - %.2f]",
		worldHeight[0].Getf(),
		worldHeight[1].Getf()
	);

	grcDebugDraw::AddDebugOutput(
		"CSM Receiver Height = [%.2f - %.2f]",
		recvrHeight[0].Getf(),
		recvrHeight[1].Getf()
	);

	for (int cascadeIndex = 0; cascadeIndex < CASCADE_SHADOWS_COUNT; cascadeIndex++)
	{
		const float zmin  = state.m_viewports[cascadeIndex].GetFarClip();
		const float zmax  = state.m_viewports[cascadeIndex].GetNearClip();
		const Vec3V bmin  = state.m_cascadeBoundsBoxMin[cascadeIndex];
		const Vec3V bmax  = state.m_cascadeBoundsBoxMax[cascadeIndex];
		const float r     = Mag(bmax - bmin).Getf()/2.0f;
		const Vec3V bmin2 = state.m_cascadeBoundsBoxMin[Min<int>(cascadeIndex + 1, CASCADE_SHADOWS_COUNT - 1)];
		const Vec3V bmax2 = state.m_cascadeBoundsBoxMax[Min<int>(cascadeIndex + 1, CASCADE_SHADOWS_COUNT - 1)];
		const float r2    = Mag(bmax2 - bmin2).Getf()/2.0f;

		Color32 colour = Color32(0.9f, 0.9f, 0.9f); // this is the default colour in debugdraw.cpp
		const char* extra = "";

		if (r > r2 + 0.1f) // something is wrong - cascade sphere is larger than next (further) cascade sphere
		{
			colour = Color32(255,0,0,255);
			extra = " (ERROR)";
		}

		if (CSMShadowTypeIsHighAltitude(gCascadeShadows.m_scriptShadowType))
		{
			if (cascadeIndex == 0)
			{
				grcDebugDraw::AddDebugOutput(colour, "        box radius=%.2f", r);
			}
		}
		else if (1) // show bounds
		{
			grcDebugDraw::AddDebugOutput(
				colour,
				"        sphere=[%.2f,%.2f,%.2f,%.2f], min=[%.2f,%.2f,%.2f], max=[%.2f,%.2f,%.2f], range z=%.2f, box radius=%.2f, ratio=%.2f%s",
				VEC4V_ARGS(state.m_cascadeBoundsSphere[cascadeIndex]),
				VEC3V_ARGS(state.m_cascadeBoundsBoxMin[cascadeIndex]),
				VEC3V_ARGS(state.m_cascadeBoundsBoxMax[cascadeIndex]),
				zmax - zmin,
				r,
				r2/r,
				extra
			);
		}
		else // don't show bounds
		{
			grcDebugDraw::AddDebugOutput(
				colour,
				"        range z=%.2f, box radius=%.2f, ratio=%.2f%s",
				zmax - zmin,
				r,
				r2/r,
				extra
			);
		}
	}

	if (1) // display velocity (rough approx based on camera position over N frames)
	{
		const  int   numFrames = 6; // N
		static Vec3V camPos[numFrames];
		static u32   timers[numFrames];
		static int   frame = 0;

		camPos[frame] = gVpMan.GetUpdateGameGrcViewport()->GetCameraPosition();
		timers[frame] = fwTimer::GetTimeInMilliseconds(); // seconds
		frame++;

		if (frame == numFrames)
		{
			float totalDist = 0.0f;
			float totalTime = 0.0f; // = timers[numFrames - 1] - timers[0]

			for (int i = 1; i < numFrames; i++)
			{
				totalDist += Mag(camPos[i] - camPos[i - 1]).Getf();
				totalTime += (float)(timers[i] - timers[i - 1]);

				camPos[i - 1] = camPos[i];
				timers[i - 1] = timers[i];
			}

			const float kmph = 60.0f*60.0f*totalDist/totalTime;
			grcDebugDraw::AddDebugOutput("velocity = %.2f mph (%.2f kmph)", kmph*0.621371192f, kmph);

			frame--;
		}
	}
}

#endif // __BANK

static Vec4V_Out CalcScriptBoundsSphere(Vec4V_In scriptBoundsSphere, ScalarV_In defaultRadius, Mat33V_In worldToShadow33, bool bCascadeBoundsSnap)
{
	Vec4V boundsSphere = scriptBoundsSphere;

	if (IsEqualAll(scriptBoundsSphere.GetW(), ScalarV(V_ZERO)))
	{
		boundsSphere.SetW(defaultRadius);
	}

	if (!bCascadeBoundsSnap)
	{
		boundsSphere = TransformSphere(worldToShadow33, boundsSphere);
	}

	return boundsSphere;
}

void CCSMGlobals::CalcCascadeBounds(
	Vec4V_InOut   boundsSphere, // shadowspace
	Vec3V_InOut   boundsBoxMin, // shadowspace
	Vec3V_InOut   boundsBoxMax, // shadowspace
	Mat34V_In     viewToWorld,
	Mat34V_In     viewToShadow,
	Mat33V_In     worldToShadow33,
#if CSM_ASYMMETRICAL_VIEW
	const fwRect& tanBounds,
#endif // CSM_ASYMMETRICAL_VIEW
	ScalarV_In    tanHFOV,
	ScalarV_In    tanVFOV,
	ScalarV_In    tanHFOV_bounds,
	ScalarV_In    tanVFOV_bounds,
	ScalarV_InOut z0,
	ScalarV_InOut z1,
	int           cascadeIndex,
	bool          bCascadeBoundsSnap)
{
	bool bTrackSphere = false; // if true then we're tracking a sphere, so tracker bounds is in shadowspace (effectively)
	bool bTrackForced = false; // if true then we can't kill the bounds if this cascade is "disabled" via timecycle

	if (CSMShadowTypeIsHighAltitude(m_scriptShadowType))
	{
		Vec3V boundsMin;
		Vec3V boundsMax;

#if __BANK
		if (TiledScreenCapture::IsEnabledOrthographic())
		{
			spdAABB tileBounds;
			TiledScreenCapture::GetTileBounds(tileBounds, true);
			tileBounds.Transform(worldToShadow33);
			boundsMin = tileBounds.GetMin();
			boundsMax = tileBounds.GetMax();
		}
		else
#endif // __BANK
		{
			const camSwitchCamera* pSwitchCamera = camInterface::GetSwitchDirector().GetSwitchCamera();

			ScalarV tanHFOVMax = tanHFOV;
			ScalarV tanVFOVMax = tanVFOV;

			if (pSwitchCamera)
			{
				float fov0 = 0.0f;
				float fov1 = 0.0f;
#if __ASSERT
				Vec3V pos0 = Vec3V(V_ZERO);
				Vec3V pos1 = Vec3V(V_ZERO);
				pSwitchCamera->GetTransition(pos0, pos1, fov0, fov1);

				// make sure current camera is within transition bounds (position)
				Assertf(spdAABB(Min(pos0, pos1) - Vec3V(V_FLT_SMALL_2), Max(pos0, pos1) + Vec3V(V_FLT_SMALL_2)).ContainsPoint(viewToWorld.GetCol3()), "camera position not within transition bounds");
#else
				pSwitchCamera->GetTransitionFov(fov0, fov1);
#endif
				const float tanVFOVMinf = tanf(DtoR*Min<float>(fov0, fov1)/2.0f);
				const float tanVFOVMaxf = tanf(DtoR*Max<float>(fov0, fov1)/2.0f);

				// make sure current camera is within transition bounds (FOV)
				// TODO -- this assert is firing when pausing during the transition, but i don't know why (see BS#1075526) .. i'm disabling the assert since it's harmless
				//Assertf(tanVFOV.Getf() >= tanVFOVMinf - 0.01f, "camera tanVFOV(%f) not within transition bounds [%f..%f]", tanVFOV.Getf(), tanVFOVMinf, tanVFOVMaxf);
				//Assertf(tanVFOV.Getf() <= tanVFOVMaxf + 0.01f, "camera tanVFOV(%f) not within transition bounds [%f..%f]", tanVFOV.Getf(), tanVFOVMinf, tanVFOVMaxf);

				const ScalarV aspect = tanHFOV/tanVFOV;

				if (BANK_SWITCH(gCascadeShadows.m_debug.m_cascadeBoundsSnapHighAltitudeFOV, true))
				{
					(void)tanVFOVMinf; // don't warn about unused var
					tanVFOVMax = ScalarV(tanVFOVMaxf);
					tanHFOVMax = aspect*tanVFOVMax;
				}
			}

			// TODO -- needs optimisation and cleanup, also this is not a particularly elegant/correct way of calculating quadrant bounds
			const float camX = viewToWorld.GetCol3().GetXf();
			const float camY = viewToWorld.GetCol3().GetYf();
			const float camZ = viewToWorld.GetCol3().GetZf();
			const float tanDiagonalFOV = Sqrt(tanHFOVMax*tanHFOVMax + tanVFOVMax*tanVFOVMax).Getf(); // cone
			const float qtzc = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(camX, camY);
			const float qtzr = (camZ - qtzc)*tanDiagonalFOV;
			const float qtz0 = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(camX - qtzr, camY - qtzr, camX + qtzr, camY + qtzr);
			const float qtz1 = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(camX - qtzr, camY - qtzr, camX + qtzr, camY + qtzr);
			
			if (m_scriptQuadrantTrackFlyCamera)
			{
				const ScalarV d0(qtz0 - camZ);

				// TODO -- optimise this
				const Vec3V edge00 = Transform3x3(viewToWorld, Vec3V(-tanHFOVMax, -tanVFOVMax, ScalarV(V_NEGONE)));
				const Vec3V edge10 = Transform3x3(viewToWorld, Vec3V(+tanHFOVMax, -tanVFOVMax, ScalarV(V_NEGONE)));
				const Vec3V edge01 = Transform3x3(viewToWorld, Vec3V(-tanHFOVMax, +tanVFOVMax, ScalarV(V_NEGONE)));
				const Vec3V edge11 = Transform3x3(viewToWorld, Vec3V(+tanHFOVMax, +tanVFOVMax, ScalarV(V_NEGONE)));

				const Vec3V p00 = Multiply(worldToShadow33, AddScaled(viewToWorld.GetCol3(), edge00/edge00.GetZ(), d0));
				const Vec3V p10 = Multiply(worldToShadow33, AddScaled(viewToWorld.GetCol3(), edge10/edge10.GetZ(), d0));
				const Vec3V p01 = Multiply(worldToShadow33, AddScaled(viewToWorld.GetCol3(), edge01/edge01.GetZ(), d0));
				const Vec3V p11 = Multiply(worldToShadow33, AddScaled(viewToWorld.GetCol3(), edge11/edge11.GetZ(), d0));

				boundsMin = Min(p00, p10, p01, p11);
				boundsMax = Max(p00, p10, p01, p11);
			}
			else
			{
#if CSM_ASYMMETRICAL_VIEW
				CalculateFrustumBoundsAsymmetrical(boundsMin, boundsMax, viewToShadow, tanBounds, ScalarV(camZ - qtz0), ScalarV(camZ - qtz1));
#else
				CalculateFrustumBounds(boundsMin, boundsMax, viewToShadow, tanHFOVMax, tanVFOVMax, ScalarV(camZ - qtz0), ScalarV(camZ - qtz1));
#endif
			}
		}

#if CSM_QUADRANT_TRACK_ADJ
		if (m_scriptQuadrantTrackExpand1 != 0.0f) // apply expansion (1)
		{
			const Vec3V boundsExpand = (boundsMax - boundsMin)*ScalarV(V_HALF)*ScalarV(m_scriptQuadrantTrackExpand1);

			boundsMin -= boundsExpand;
			boundsMax += boundsExpand;
		}
#endif // CSM_QUADRANT_TRACK_ADJ

		const Vec3V boundsMid = (boundsMax + boundsMin)*ScalarV(V_HALF);

		if (m_scriptShadowType != CSM_TYPE_HIGHRES_SINGLE)
		{
			boundsMin = Vec3V(((cascadeIndex & 1) ? boundsMid : boundsMin).GetX(), ((cascadeIndex & 2) ? boundsMid : boundsMin).GetY(), boundsMin.GetZ());
			boundsMax = Vec3V(((cascadeIndex & 1) ? boundsMax : boundsMid).GetX(), ((cascadeIndex & 2) ? boundsMax : boundsMid).GetY(), boundsMax.GetZ());
		}

		// apply a small amount of expansion to prevent gaps when using shadowspace quadrants (BS#1004267). this would not be necessary if we were snapping properly ..
		{
			const Vec3V boundsExpand = (boundsMax - boundsMin)*ScalarV(V_FLT_SMALL_2);

			boundsMin -= boundsExpand;
			boundsMax += boundsExpand;
		}

#if CSM_QUADRANT_TRACK_ADJ
		if (m_scriptQuadrantTrackExpand2 != 0.0f) // apply expansion (2)
		{
			const Vec3V boundsExpand = (boundsMax - boundsMin)*ScalarV(V_HALF)*ScalarV(m_scriptQuadrantTrackExpand2);

			boundsMin -= boundsExpand;
			boundsMax += boundsExpand;
		}

		if (m_scriptQuadrantTrackSpread != 0) // apply spread
		{
			const Vec3V boundsSpread = ((boundsMax + boundsMin)*ScalarV(V_HALF) - boundsMid)*ScalarV(m_scriptQuadrantTrackSpread);

			boundsMin += boundsSpread;
			boundsMax += boundsSpread;
		}

		if (IsEqualAll(m_scriptQuadrantTrackScale, Vec3V(V_ONE)) == 0) // apply scale (use this to test if scale is being applied to bounds)
		{
			const Vec3V boundsCentre = (boundsMax + boundsMin)*ScalarV(V_HALF);
			const Vec3V boundsExtent = (boundsMax - boundsMin)*ScalarV(V_HALF);

			boundsMin = boundsCentre - boundsExtent*m_scriptQuadrantTrackScale;
			boundsMax = boundsCentre + boundsExtent*m_scriptQuadrantTrackScale;
		}

		// apply offset
		{
			boundsMin += m_scriptQuadrantTrackOffset;
			boundsMax += m_scriptQuadrantTrackOffset;
		}
#endif // CSM_QUADRANT_TRACK_ADJ

#if __BANK
		if (m_debug.m_drawShadowDebugShader)
		{
			const Color32 cascadeColours[] =
			{
				Color32(0,0,255,255), // blue
				Color32(0,255,0,255), // green
				Color32(255,0,0,255), // red
				Color32(255,0,255,255), // magenta
			};

			Mat33V shadowToWorld33;
			Transpose(shadowToWorld33, worldToShadow33);

			grcDebugDraw::BoxOriented(boundsMin, boundsMax, Mat34V(shadowToWorld33, Vec3V(V_ZERO)), cascadeColours[cascadeIndex], false);
		}
#endif // __BANK

		const spdAABB bounds(boundsMin, boundsMax);

		boundsSphere = bounds.GetBoundingSphereV4();
		boundsBoxMin = bounds.GetMin();
		boundsBoxMax = bounds.GetMax();
		bTrackForced = true;
	}
	else if (m_scriptCascadeBoundsEnable[cascadeIndex])
	{
		boundsSphere = CalcScriptBoundsSphere(
			m_scriptCascadeBoundsSphere[cascadeIndex],
			boundsSphere.GetW(),
			worldToShadow33,
			bCascadeBoundsSnap
		);

		boundsBoxMin = boundsSphere.GetXYZ() - Vec3V(boundsSphere.GetW());
		boundsBoxMax = boundsSphere.GetXYZ() + Vec3V(boundsSphere.GetW());
		bTrackSphere = true;
	}
#if __BANK
	else if (cascadeIndex == 0 && m_debug.m_cascadeBoundsIsolateSelectedEntity)
	{
		spdAABB aabb;

		if (GetEntityApproxAABB(aabb, (CEntity*)g_PickerManager.GetSelectedEntity()))
		{
			// TODO -- construct box around entity's local bounds
			const Vec3V   entityCentre = aabb.GetCenter();
			const Vec3V   entityExtent = aabb.GetExtent();
			const ScalarV entityRadius = Mag(entityExtent)/ScalarV(m_debug.m_cascadeBoundsIsolateZoom);
			const Vec3V   entityOffset = Vec3V(m_debug.m_cascadeBoundsIsolateOffsetX, m_debug.m_cascadeBoundsIsolateOffsetY, 0.0f);

			boundsSphere = Vec4V(Multiply(worldToShadow33, entityCentre) + entityOffset*entityRadius, entityRadius);

			if (bCascadeBoundsSnap)
			{
				boundsSphere = TransformSphere(m_shadowToWorld33, boundsSphere);
			}

			boundsBoxMin = boundsSphere.GetXYZ() - Vec3V(boundsSphere.GetW());
			boundsBoxMax = boundsSphere.GetXYZ() + Vec3V(boundsSphere.GetW());
			bTrackSphere = true;
		}
	}
#endif // __BANK
	else
	{
		bool useDynamicDepthRange = false;

#if CSM_DYNAMIC_DEPTH_RANGE
		if(m_entityTracker.IsInSniperTypeCamera() && g_UseDynamicDistribution && cascadeIndex < 3)
		{
			if(useDynamicDepthRange && cascadeIndex >= 1)
			{
				// double check on wide viewport.
				ScalarV cascade0Width= z0*tanHFOV;
				const float firstPoint[CASCADE_SHADOWS_COUNT] = { 0.0f, 1.5f, 2.5f, 1.0f };
				const float endPoint[CASCADE_SHADOWS_COUNT] = { 0.0f, 2.5f, 3.5f, 1.0f };
				
				z0 = z0 + cascade0Width*ScalarV(firstPoint[cascadeIndex]);
				z1 = z1 + cascade0Width*ScalarV(endPoint[cascadeIndex]);  
			}
		}
		else
		{
			if(cascadeIndex < 2)
			{
				useDynamicDepthRange = m_entityTracker.GetDynamicDepthRange(z0, z1);

				if(useDynamicDepthRange && cascadeIndex == 1)
				{
					// double check on wide viewport.
					ScalarV cascade0Width= z0*tanHFOV;
					static dev_float firstPoint = 1.5f;
					static dev_float endPoint = 3.6f;

					z0 = z0 + cascade0Width*ScalarV(firstPoint);
					z1 = z1 + cascade0Width*ScalarV(endPoint);  

				}
			}
		}
#endif // CSM_DYNAMIC_DEPTH_RANGE

		boundsSphere = CalcMinimumEnclosingSphere<true>(
			bCascadeBoundsSnap ? +viewToWorld.GetCol3() : +viewToShadow.GetCol3(),
			bCascadeBoundsSnap ? -viewToWorld.GetCol2() : -viewToShadow.GetCol2(),
			tanHFOV_bounds,
			tanVFOV_bounds,
			z0,
			z1
			);

		Vec4V boundsSphereEntity;
		if (cascadeIndex == 0)
		{
			if(!useDynamicDepthRange && m_entityTracker.GetFocalSphere(boundsSphereEntity, m_scriptEntityTrackerScale))
			{
				boundsSphere = Lerp(  m_entityTracker.GetFocalObjectBlend(), boundsSphere, boundsSphereEntity );
				if (!bCascadeBoundsSnap)
				{
					boundsSphere = TransformSphere(worldToShadow33, boundsSphere);
				}
			}

			BANK_ONLY(if(!m_debug.m_cascadeBoundsOverrideTimeCycle))
				boundsSphere *= Vec4V(1.0f, 1.0f, 1.0f, g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_DIRECTIONAL_SHADOW_CASCADE0_SCALE));
		}
		else
		{
			Vec3V	spherePosition = boundsSphere.GetXYZ();

			spherePosition += -viewToWorld.GetCol2()*ScalarV(m_scriptBoundPosition);

			boundsSphere.SetXYZ(spherePosition);
		}

		if (m_scriptCascadeBoundsLerpFromCustscene[cascadeIndex])
		{
			Assert(!m_scriptCascadeBoundsEnable[cascadeIndex]); // we would have handled this above

			m_scriptCascadeBoundsLerpFromCustsceneTime[cascadeIndex] += fwTimer::GetTimeStep();

			if (m_scriptCascadeBoundsLerpFromCustsceneTime[cascadeIndex] >= m_scriptCascadeBoundsLerpFromCustsceneDuration[cascadeIndex])
			{
				m_scriptCascadeBoundsLerpFromCustscene        [cascadeIndex] = false;
				m_scriptCascadeBoundsLerpFromCustsceneTime    [cascadeIndex] = 0.0f;
				m_scriptCascadeBoundsLerpFromCustsceneDuration[cascadeIndex] = 0.0f;
			}
			else // still lerping ..
			{
				const Vec4V scriptBoundsSphere = CalcScriptBoundsSphere(
					m_scriptCascadeBoundsSphere[cascadeIndex],
					boundsSphere.GetW(),
					worldToShadow33,
					bCascadeBoundsSnap
				);

				const ScalarV t(m_scriptCascadeBoundsLerpFromCustsceneTime[cascadeIndex]/m_scriptCascadeBoundsLerpFromCustsceneDuration[cascadeIndex]);

				boundsSphere = Lerp(t, scriptBoundsSphere, boundsSphere);
			}
		}

		boundsBoxMin = boundsSphere.GetXYZ() - Vec3V(boundsSphere.GetW());
		boundsBoxMax = boundsSphere.GetXYZ() + Vec3V(boundsSphere.GetW());
		bTrackSphere = true;
	}

	// this shit be doing bad things ..
	//
	// I was trying to move the disabled cascades out of the world (so the visibility lists would be empty),
	// but this was causing NaNs to appear in the cascade vars, so I've removed this for now. The important
	// thing is that the disabled cascades will not be rendered into, and this is handled correctly in the
	// BuildDrawList loop over the cascades.
	//
	/*if ((cascadeIndex >= m_numCascades && !bTrackForced) BANK_ONLY(|| !m_debug.m_enableCascade[cascadeIndex]))
	{
		boundsSphere = Vec4V(Vec3V(V_FLT_LARGE_8), ScalarV(V_ONE)); // move the bounds outside of the map
		boundsBoxMin = boundsSphere.GetXYZ() - Vec3V(boundsSphere.GetW());
		boundsBoxMax = boundsSphere.GetXYZ() + Vec3V(boundsSphere.GetW());
		bTrackSphere = true;
	}*/
}

static ScalarV_Out CalculateFarDistanceOnCascade(Vec4V_In sphere, Mat34V_In cascadeToWorld)
{
	const float radiusModifier = 1.7320508f; // sqrt(3)

	// TODO -- could get even tighter with boxes
	return Mag(Transform(cascadeToWorld, sphere.GetXYZ())) + sphere.GetW()*ScalarV(radiusModifier);
}

void CCSMGlobals::UpdateTimecycleState(CCSMState& state)
{
	// z*A + B == (z - height - falloff)/-falloff
	const float fogShadowAmount     = m_cloudTextureSettings.m_fogShadowAmount;
	const float fogShadowFalloff    = m_cloudTextureSettings.m_fogShadowFalloff;
	const float fogShadowBaseHeight = m_cloudTextureSettings.m_fogShadowBaseHeight;
	const float fogShadowGradientA  = -1.0f/fogShadowFalloff;
	const float fogShadowGradientB  = 1.0f - fogShadowGradientA*fogShadowBaseHeight;
	const float fogShadowFast       = 0.0f; // TODO -- we could use a constant fog shadows for water reflection

	state.m_cascadeShaderVars_shared[CASCADE_SHADOWS_SHADER_VAR_FOG_SHADOWS] = Vec4V(fogShadowGradientA, fogShadowGradientB, fogShadowAmount, fogShadowFast);
}

bool CCSMGlobals::AllowGrassCacadeCulling(float lastCascadeEnd) const
{
	bool isQuadrantType = m_scriptShadowType == CSM_TYPE_QUADRANT;
	const bool isCascadeType = !isQuadrantType && (m_scriptShadowType != CSM_TYPE_HIGHRES_SINGLE);
	return (isCascadeType && lastCascadeEnd > CSMStatic::sGrassCullLastCascadeEndThreshold) || CSMStatic::sAlwaysAllowGrassCulling;
}

#if USE_NV_SHADOW_LIB
void CopyToNvMatrix(const Mat34V& mIn, gfsdk_float4x4& mOut)
{
	mOut._11 = mIn.GetM00f();
	mOut._21 = mIn.GetM01f();
	mOut._31 = mIn.GetM02f();
	mOut._41 = mIn.GetM03f();

	mOut._12 = mIn.GetM10f();
	mOut._22 = mIn.GetM11f();
	mOut._32 = mIn.GetM12f();
	mOut._42 = mIn.GetM13f();

	mOut._13 = mIn.GetM20f();
	mOut._23 = mIn.GetM21f();
	mOut._33 = mIn.GetM22f();
	mOut._43 = mIn.GetM23f();

	mOut._14 = 0.0f;
	mOut._24 = 0.0f;
	mOut._34 = 0.0f;
	mOut._44 = 1.0f;
}

void CopyToNvMatrix(const Mat44V& mIn, gfsdk_float4x4& mOut)
{
	mOut._11 = mIn.GetM00f();
	mOut._21 = mIn.GetM01f();
	mOut._31 = mIn.GetM02f();
	mOut._41 = mIn.GetM03f();

	mOut._12 = mIn.GetM10f();
	mOut._22 = mIn.GetM11f();
	mOut._32 = mIn.GetM12f();
	mOut._42 = mIn.GetM13f();

	mOut._13 = mIn.GetM20f();
	mOut._23 = mIn.GetM21f();
	mOut._33 = mIn.GetM22f();
	mOut._43 = mIn.GetM23f();

	mOut._14 = mIn.GetM30f();
	mOut._24 = mIn.GetM31f();
	mOut._34 = mIn.GetM32f();
	mOut._44 = mIn.GetM33f();
}

void CopyToNvVector(const Vec4V& vIn, gfsdk_float3& vOut)
{
	vOut.x = vIn.GetXf();
	vOut.y = vIn.GetYf();
	vOut.z = vIn.GetZf();
}

void CopyToNvVector(const Vec3V& vIn, gfsdk_float3& vOut)
{
	vOut.x = vIn.GetXf();
	vOut.y = vIn.GetYf();
	vOut.z = vIn.GetZf();
}

void CopyToNvState(const CCSMState& state, NV_ShadowLib_ExternalMapDesc& desc)
{
	CopyToNvMatrix(state.m_eyeWorldToView, desc.m4x4EyeViewMatrix);
	CopyToNvMatrix(state.m_eyeViewToProjection, desc.m4x4EyeProjectionMatrix);
	CopyToNvMatrix(state.m_viewports[0].GetViewMtx(), desc.m4x4LightViewMatrix[0]);
	CopyToNvMatrix(state.m_viewports[1].GetViewMtx(), desc.m4x4LightViewMatrix[1]);
	CopyToNvMatrix(state.m_viewports[2].GetViewMtx(), desc.m4x4LightViewMatrix[2]);
	CopyToNvMatrix(state.m_viewports[3].GetViewMtx(), desc.m4x4LightViewMatrix[3]);
	CopyToNvMatrix(state.m_viewports[0].GetProjection(), desc.m4x4LightProjMatrix[0]);
	CopyToNvMatrix(state.m_viewports[1].GetProjection(), desc.m4x4LightProjMatrix[1]);
	CopyToNvMatrix(state.m_viewports[2].GetProjection(), desc.m4x4LightProjMatrix[2]);
	CopyToNvMatrix(state.m_viewports[3].GetProjection(), desc.m4x4LightProjMatrix[3]);

	const Vec3V& lightPos = state.m_viewports[0].GetCameraMtx().GetCol3ConstRef();
	const Vec3V& lightDir = state.m_shadowToWorld33.GetCol2();
	const Vec3V lightAt = lightPos + lightDir;
	CopyToNvVector(lightPos, desc.LightDesc.v3LightPos);
	CopyToNvVector(lightAt,  desc.LightDesc.v3LightLookAt);
	CopyToNvVector(state.m_cascadeShaderVars_shared[_cascadeBoundsConstA_packed_start + 0], desc.v3BiasZ[0]);
	CopyToNvVector(state.m_cascadeShaderVars_shared[_cascadeBoundsConstA_packed_start + 1], desc.v3BiasZ[1]);
	CopyToNvVector(state.m_cascadeShaderVars_shared[_cascadeBoundsConstA_packed_start + 2], desc.v3BiasZ[2]);
	CopyToNvVector(state.m_cascadeShaderVars_shared[_cascadeBoundsConstA_packed_start + 3], desc.v3BiasZ[3]);
}
#endif

#if USE_AMD_SHADOW_LIB
void CopyToAmdMatrix(const Mat44V& mIn, AMD::SHADOWS_DESC::float4x4& mOut)
{
	mOut.m[0]  = mIn.GetM00f();
	mOut.m[1]  = mIn.GetM01f();
	mOut.m[2]  = mIn.GetM02f();
	mOut.m[3]  = mIn.GetM03f();

	mOut.m[4]  = mIn.GetM10f();
	mOut.m[5]  = mIn.GetM11f();
	mOut.m[6]  = mIn.GetM12f();
	mOut.m[7]  = mIn.GetM13f();

	mOut.m[8]  = mIn.GetM20f();
	mOut.m[9]  = mIn.GetM21f();
	mOut.m[10] = mIn.GetM22f();
	mOut.m[11] = mIn.GetM23f();

	mOut.m[12] = mIn.GetM30f();
	mOut.m[13] = mIn.GetM31f();
	mOut.m[14] = mIn.GetM32f();
	mOut.m[15] = mIn.GetM33f();
}

void CopyToAMDState(const CCSMState& state, AMD::SHADOWS_DESC& desc, int cascadeIndex)
{
	// the shaders that apply shadows need viewProj of the cascade and inverse viewProj of the scene
	Mat44V cascadeViewProj;
	Multiply(cascadeViewProj, state.m_viewports[cascadeIndex].GetProjection(), state.m_viewports[cascadeIndex].GetViewMtx());
	CopyToAmdMatrix(cascadeViewProj, desc.m_Parameters.m_Light.m_ViewProjection);
	CopyToAmdMatrix(state.m_eyeViewProjectionInv, desc.m_Parameters.m_Viewer.m_ViewProjection_Inv);

	// marking the stencil needs a composite matrix of the inverse viewProj for the cascade
	// and the viewProj of the scene
	Mat44V cascadeViewProjInv;
	Mat44V stencilBoxTransform;
	InvertFull(cascadeViewProjInv, cascadeViewProj);
	Multiply(stencilBoxTransform, state.m_eyeViewProjection, cascadeViewProjInv);
	CopyToAmdMatrix(stencilBoxTransform, desc.m_UnitCubeParameters.m_Transform);

	desc.m_CascadeIndex = cascadeIndex;
	if (cascadeIndex >= 2)
	{
		Mat44V cascadeViewProjPrev;
		Multiply(cascadeViewProjPrev, state.m_viewports[cascadeIndex-2].GetProjection(), state.m_viewports[cascadeIndex-2].GetViewMtx());
		// marking the stencil needs a composite matrix of the inverse viewProj for the cascade
		// and the viewProj of the scene (this one is for the previous cascade region, to clear 
		// the parts of the current region that were already covered by the previous region)
		Mat44V cascadeViewProjPrevInv;
		Mat44V stencilBoxTransformPrev;
		InvertFull(cascadeViewProjPrevInv, cascadeViewProjPrev);
		Multiply(stencilBoxTransformPrev, state.m_eyeViewProjection, cascadeViewProjPrevInv);
		CopyToAmdMatrix(stencilBoxTransformPrev, desc.m_PreviousUnitCubeParameters.m_Transform);
	}
}
#endif

float CalcThreshold(const grcViewport & vp)
{
	static float extraTolerance = 0.1f;
	float diagFOV = sqrtf(vp.GetTanHFOV()*vp.GetTanHFOV() + vp.GetTanVFOV()*vp.GetTanVFOV());
	return sqrtf(diagFOV*diagFOV + 1.0f) * vp.GetNearClip() + extraTolerance; // the the distant to the corner of the view frustum at the near clip plane (plus a small tolerance)
}

void CCSMGlobals::UpdateViewport(grcViewport& viewport, const grcViewport& vp)
{
	CCSMState& state = GetUpdateState();

	state.Clear();
	state.m_sampleType           = CSM_ST_INVALID; // we'll set it later when we're allowed to access the timecycle
	switch (m_scriptShadowType)
	{
	case CSM_TYPE_HIGHRES_SINGLE:  //not supported for now
		//state.m_shadowMap = m_shadowMapHighResSingle;
		state.m_shadowMapNumCascades = 1;
		break;
	case CSM_TYPE_CASCADES:
	case CSM_TYPE_QUADRANT:
	default:
		state.m_shadowMap = m_shadowMapBase;
		state.m_shadowMapNumCascades = 4;
		break;
	}

	Assert(state.m_shadowMap); 

	CRenderPhaseCascadeShadowsInterface::UpdateTimecycleSettings();

	ScalarV splitZ[CASCADE_SHADOWS_COUNT + 1];
	CalcSplitZ(splitZ, true);
	const bool bHighAltitude = CSMShadowTypeIsHighAltitude(m_scriptShadowType);

	bool isCameraCut = ((camInterface::GetFrame().GetFlags() & (camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation)) > 0) ;

#if CSM_PORTALED_SHADOWS
	const Vec3V ldir = m_shadowToWorld33.GetCol2();

	const grcViewport& uvp = *gVpMan.GetUpdateGameGrcViewport();

	Vec3V np = uvp.GetCameraPosition();
	np += ScalarV(-uvp.GetNearClip())* uvp.GetCameraMtx().c();
	gCascadeShadows.GetUpdateWindowRenderer().Create(ldir, np, CalcThreshold(uvp));

	if (gCascadeShadows.GetUpdateWindowRenderer().InteriorOnly(ldir, *gVpMan.GetUpdateGameGrcViewport()))
		state.m_shadowIsActive = false;
#endif // CSM_PORTALED_SHADOWS


#if __DEV
	if(gCascadeShadows.m_debug.m_forceShadowMapInactive)
		state.m_shadowIsActive = false;
#endif //__DEV


#if __BANK

	m_debug.m_cameraLock.Update(&vp, splitZ[0], splitZ[CASCADE_SHADOWS_COUNT]);
	m_debug.m_cameraLock.DebugDraw();

	const Mat34V  viewToWorld    = m_debug.m_cameraLock.GetViewToWorld();
	const ScalarV tanHFOV        = m_debug.m_cameraLock.GetTanHFOV();
	const ScalarV tanVFOV        = m_debug.m_cameraLock.GetTanVFOV();
	const ScalarV tanHFOV_bounds = Max(tanHFOV, Max(m_scriptCascadeBoundsTanHFOV, ScalarV(tanf(DtoR*0.5f*m_debug.m_cascadeBoundsHFOV)))); // FOV used to compute cascade bounds
	const ScalarV tanVFOV_bounds = Max(tanVFOV, Max(m_scriptCascadeBoundsTanVFOV, ScalarV(tanf(DtoR*0.5f*m_debug.m_cascadeBoundsVFOV))));
#if __DEV
	const ScalarV zNearClip      = ScalarV(vp.GetNearClip()*1.1f); // slightly in front of near clip
#endif // __DEV

#else // not __BANK

	const Mat34V  viewToWorld    = vp.GetCameraMtx();
	const ScalarV tanHFOV        = ScalarV(vp.GetTanHFOV());
	const ScalarV tanVFOV        = ScalarV(vp.GetTanVFOV());
	const ScalarV tanHFOV_bounds = Max(tanHFOV, m_scriptCascadeBoundsTanHFOV); // FOV used to compute cascade bounds, can be adjusted by script to reduce shimmering
	const ScalarV tanVFOV_bounds = Max(tanVFOV, m_scriptCascadeBoundsTanVFOV);

#endif // not __BANK

	ScalarV cascadeSplitZScale(V_ONE);

	BANK_ONLY(if (m_debug.m_cascadeBoundsSplitZScale))
	{
		cascadeSplitZScale = InvSqrt(ScalarV(V_ONE) + tanHFOV_bounds*tanHFOV_bounds + tanVFOV_bounds*tanVFOV_bounds);
	}

	const Vec3V camPos = +viewToWorld.GetCol3();

	ScalarV worldHeight[2] = {m_worldHeight[0], m_worldHeight[1]};
	ScalarV recvrHeight[2] = {m_recvrHeight[0], m_recvrHeight[1]};

#if __DEV
	if (m_debug.m_altSettings_enabled && m_debug.m_altSettings_worldHeightEnabled)
	{
		worldHeight[0] = ScalarV(m_debug.m_altSettings_worldHeight[0]);
		worldHeight[1] = ScalarV(m_debug.m_altSettings_worldHeight[1]);
	}
#endif // __DEV

	Mat34V viewToShadow;
	UnTransformOrtho(viewToShadow, Mat34V(m_shadowToWorld33, Vec3V(V_ZERO)), viewToWorld);

	Mat33V worldToShadow33;
	Transpose(worldToShadow33, m_shadowToWorld33);

	// ============================================================================================
	// ============================================================================================

	bool bBoundsSnap = BANK_SWITCH(m_debug.m_cascadeBoundsSnap, true);

	if (CSMShadowTypeIsHighAltitude(m_scriptShadowType))// && BANK_SWITCH(m_debug.m_cascadeBoundsSnapHighAltitudeFOV, true))
	{
		bBoundsSnap = false;
	}

	Vec3V vpBoundsMin = Vec3V(V_FLT_MAX);
	Vec3V vpBoundsMax = Vec3V(V_NEG_FLT_MAX);


	const int splitToUse = DEV_SWITCH(gCascadeShadows.m_debug.m_renderOccluders_enabled, false) ? (CASCADE_SHADOWS_COUNT - 1) : CASCADE_SHADOWS_COUNT;
	const float shadowFadeMaxZ = splitZ[splitToUse].Getf();

	m_entityTracker.UpdateDynamicDepthRange(shadowFadeMaxZ, vp.GetFOVY());

	bool bCascadeBoundsForceOverlap = BANK_SWITCH(m_debug.m_cascadeBoundsForceOverlap, true) || 
									  Water::IsCameraUnderwater() || //underwater needs cascade 2 to cover the origin 
									  m_entityTracker.UsingDynamicDepthMode(); //mutually exclusive technique

	for (int cascadeIndex = 0; cascadeIndex < state.m_shadowMapNumCascades; cascadeIndex++)
	{
		grcViewport cascadeViewport;


		bool bCascadeBoundsSnap = bBoundsSnap;

		const int z0_index = bCascadeBoundsForceOverlap ? 0 : cascadeIndex;
		const int z1_index = cascadeIndex + 1;

		ScalarV z0 = cascadeSplitZScale*splitZ[z0_index];
		ScalarV z1 = cascadeSplitZScale*splitZ[z1_index];

		Vec4V boundsSphere;
		Vec3V boundsBoxMin;
		Vec3V boundsBoxMax;

		CalcCascadeBounds(
			boundsSphere, // shadowspace
			boundsBoxMin, // shadowspace
			boundsBoxMax, // shadowspace
			viewToWorld,
			viewToShadow,
			worldToShadow33,
#if CSM_ASYMMETRICAL_VIEW
			vp.GetTanFOVRect(),
#endif // CSM_ASYMMETRICAL_VIEW
			tanHFOV,
			tanVFOV,
			tanHFOV_bounds,
			tanVFOV_bounds,
			z0,
			z1,
			cascadeIndex,
			bCascadeBoundsSnap
		);


		// Setup DepthBias Scaling if required
		ScalarV depthRescale(V_ONE);
		if ( m_entityTracker.UseDepthResizing() && bCascadeBoundsSnap)
		{
			Vec4V StanardCascadeSizes(2.5f,10.f,32.5f,120.f);
			float rescale= Clamp( boundsSphere.GetWf()/StanardCascadeSizes.GetElemf(cascadeIndex), 0.01f, 8.f);
			depthRescale = ScalarV(rescale);
		}
		bool prevWorldToCascadeValid = false;
		const Mat34V& prevCascade = m_prevWorldToCascade[cascadeIndex];
		if (bCascadeBoundsSnap && !isCameraCut)
		{
			// check for corruption
			Assert( IsTrueXYZ(IsFinite( prevCascade.a())));
			Assert( IsTrueXYZ(IsFinite( prevCascade.b())));
			Assert( IsTrueXYZ(IsFinite( prevCascade.c())));
			Assert( IsTrueXYZ(IsFinite( prevCascade.d())));
			if ( IsTrueXYZ( IsFinite( prevCascade.a()) & IsFinite( prevCascade.b()) 
				& IsFinite( prevCascade.c()) & IsFinite( prevCascade.d()))){
				prevWorldToCascadeValid = true;
			} 
		}

		const Vec2V boundsExtentXY = CalcOrthoViewport(
			cascadeViewport,
			boundsBoxMin,
			boundsBoxMax,
			m_shadowToWorld33,
			worldHeight[0], // world height min/max used to calculate z extents
			worldHeight[1],
			bCascadeBoundsSnap ? &m_prevWorldToCascade[cascadeIndex] : NULL,
			BANK_SWITCH(1.0f/m_debug.m_scanBoxVisibilityScale, 1.0f),
			depthRescale,
			prevWorldToCascadeValid
		);

		Assert( cascadeViewport.GetNearClip() < cascadeViewport.GetFarClip());

		if (m_scriptShadowType == CSM_TYPE_HIGHRES_SINGLE)
		{
			const int x = 0;
			const int y = 0;
			const int w = CRenderPhaseCascadeShadowsInterface::CascadeShadowResX()*2;
			const int h = CRenderPhaseCascadeShadowsInterface::CascadeShadowResY()*2;

			Assert(cascadeIndex == 0);
			state.m_viewports[cascadeIndex] = grcViewportWindow(cascadeViewport, x, y, w, h);
		}
		else // cascades
		{
			const int x = 0;
#if CASCADE_SHADOW_TEXARRAY
			const int y = 0;
#else
			const int y = CRenderPhaseCascadeShadowsInterface::CascadeShadowResY()*cascadeIndex;
#endif
			const int w = CRenderPhaseCascadeShadowsInterface::CascadeShadowResX();
			const int h = CRenderPhaseCascadeShadowsInterface::CascadeShadowResY();

			state.m_viewports[cascadeIndex] = grcViewportWindow(cascadeViewport, x, y, w, h);
		}

		if (bCascadeBoundsSnap )
		{
			const Vec4V quantSphere(m_prevWorldToCascade[cascadeIndex].GetCol3(), boundsSphere.GetW());
			if (prevWorldToCascadeValid)
				boundsSphere = TransformSphere(worldToShadow33, quantSphere);
			else 
				boundsSphere = TransformSphere(worldToShadow33, boundsSphere);
			boundsBoxMin = boundsSphere.GetXYZ() - Vec3V(boundsSphere.GetW());
			boundsBoxMax = boundsSphere.GetXYZ() + Vec3V(boundsSphere.GetW());
		}

		state.m_cascadeBoundsSphere[cascadeIndex]   = boundsSphere;
		state.m_cascadeBoundsBoxMin[cascadeIndex]   = boundsBoxMin;
		state.m_cascadeBoundsBoxMax[cascadeIndex]   = boundsBoxMax;
		state.m_cascadeBoundsExtentXY[cascadeIndex] = boundsExtentXY;

		vpBoundsMin = Min(boundsBoxMin, vpBoundsMin);
		vpBoundsMax = Max(boundsBoxMax, vpBoundsMax);

#if CSM_EDGE_OCCLUDER_QUAD_EXCLUSION
		state.m_occluderQuads[cascadeIndex].SetFromSphere(
			boundsSphere.GetXYZ(),
			boundsSphere.GetW(),
			m_shadowToWorld33.GetCol0(),
			m_shadowToWorld33.GetCol1(),
			m_shadowToWorld33.GetCol2(),
			BANK_SWITCH(m_debug.m_edgeOccluderQuadExclusionExtended, true) ? ScalarV(1024.0f) : ScalarV(V_ZERO)
		);
#endif // CSM_EDGE_OCCLUDER_QUAD_EXCLUSION
	}

	//Renderphase viewport for visibility
	CalcOrthoViewport(
		viewport,
		vpBoundsMin,
		vpBoundsMax,
		m_shadowToWorld33,
		worldHeight[0], // world height min/max used to calculate z extents
		worldHeight[1]
	);
	
	m_viewport = viewport;

	// TODO -- this is necessary? can we avoid accessing inactive cascade bounds?
	if (state.m_shadowMapNumCascades < CASCADE_SHADOWS_COUNT)
	{
		const int last = state.m_shadowMapNumCascades - 1;

		for (int i = state.m_shadowMapNumCascades; i < CASCADE_SHADOWS_COUNT; i++)
		{
			state.m_cascadeBoundsSphere[i]   = state.m_cascadeBoundsSphere[last];
			state.m_cascadeBoundsBoxMin[i]   = state.m_cascadeBoundsBoxMin[last];
			state.m_cascadeBoundsBoxMax[i]   = state.m_cascadeBoundsBoxMax[last];
			state.m_cascadeBoundsExtentXY[i] = state.m_cascadeBoundsExtentXY[last];
		}
	}

	const float shadowFadeMinZ = shadowFadeMaxZ*BANK_SWITCH(m_debug.m_shadowFadeStart, CSM_DEFAULT_FADE_START);
	const float shadowFadeA    = bHighAltitude ? 0.0f : 1.0f/(shadowFadeMinZ- shadowFadeMaxZ);
	const float shadowFadeB    = bHighAltitude ? 1.0f : -shadowFadeMaxZ*shadowFadeA;

	// ==================================
	// [state.m_cascadeShaderVars_shared]
	// ==================================

#if CASCADE_SHADOWS_SCAN_DEBUG
	float v;
	v  = DEV_SWITCH(m_debug.m_useIrregularFade, CSM_USE_IRREGULAR_FADE) ? 2.0f : 0.0f;
	v += 0.7f; // magic number?
	const ScalarV selectFadeAndBoxes(v);
#else
	const ScalarV selectFadeAndBoxes(2.7f); // magic number?
#endif

	state.m_cascadeShaderVars_shared[0] = Vec4V(worldToShadow33.GetCol0(), ScalarV(shadowFadeA));
	state.m_cascadeShaderVars_shared[1] = Vec4V(worldToShadow33.GetCol1(), ScalarV(shadowFadeB));
	state.m_cascadeShaderVars_shared[2] = Vec4V(worldToShadow33.GetCol2(), selectFadeAndBoxes);

	UpdateTimecycleState(state);

	for (int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
	{
		const Vec3V boundsExtent = Vec3V(state.m_cascadeBoundsExtentXY[i]*Vec2V(V_TWO), 
		                           ScalarV((-state.m_viewports[i].GetNearClip()) - (-state.m_viewports[i].GetFarClip())));

		const Vec3V a = Invert(boundsExtent);
		state.m_cascadeShaderVars_shared[_cascadeBoundsConstA_packed_start + i] = Vec4V(a, ScalarV(V_ZERO));
	}

	state.m_worldToShadow33 = worldToShadow33;

	// ====================================
	// [state.m_cascadeShaderVars_deferred]
	// ====================================

	const int   shadowResX    = CRenderPhaseCascadeShadowsInterface::CascadeShadowResX();
#if CASCADE_SHADOW_TEXARRAY
	const int   shadowResY    = CRenderPhaseCascadeShadowsInterface::CascadeShadowResY();
#else
	const int   shadowResY    = CRenderPhaseCascadeShadowsInterface::CascadeShadowResY()*CASCADE_SHADOWS_COUNT;
#endif
	const float ditherRadius0 = BANK_SWITCH(m_debug.m_ditherRadiusTweaks, CSM_DEFAULT_DITHER_RADIUS_TWEAKS).GetXf()*BANK_SWITCH(m_debug.m_ditherRadius, CSM_DEFAULT_DITHER_RADIUS)*m_scriptDitherRadiusScale;
	const float normalOffset0 = BANK_SWITCH(m_debug.m_shadowNormalOffsetEnabled ? m_debug.m_shadowNormalOffset/100.0f : 0.0f, CSM_DEFAULT_NORMAL_OFFSET/100.0f);
	float edgeFilterRange = m_scriptShadowType != CSM_TYPE_QUADRANT ? 12.f /(float)shadowResX : 0.f;
#if CASCADE_DITHER_TEX_HAS_EDGE_FILTER
	static dev_float rangeModifier=2.f;
	edgeFilterRange *=rangeModifier;
#endif

	state.m_cascadeShaderVars_deferred[0] = Vec4V(
		ditherRadius0/(float)shadowResX,
		ditherRadius0/(float)shadowResY,
		normalOffset0, // simple normal offset, only supported if CASCADE_SHADOWS_SUPPORT_SIMPLE_NORMAL_OFFSET is enabled
		edgeFilterRange  // 
	);

#if CSM_PORTALED_SHADOWS && CSM_PBOUNDS
	{
		GetUpdateWindowRenderer().ComputeShadowBounds(worldToShadow33);
		GetUpdateWindowRenderer().SetBoundsPacked(state.m_portalBounds, CASCADE_SHADOWS_NUM_PORTAL_BOUNDS);
	}
#endif // CSM_PORTALED_SHADOWS && CSM_PBOUNDS

	// ============================
	// [state.m_debug.m_shaderVars]
	// ============================

	state.m_viewToWorldProjection = CalcViewToWorldProjection(vp); // this needs to be accessed outside of debug now
	state.m_viewPerspectiveShear  = CalcPerspectiveShear(vp);

#if __BANK

	state.m_debug.m_shaderVars[0] = Vec4V(
		m_debug.m_shadowDebugOpacity,
		m_debug.m_shadowDebugNdotL,
		m_debug.m_shadowDebugRingOpacity,
		m_debug.m_shadowDebugGridOpacity
	);
	state.m_debug.m_shaderVars[1] = Vec4V(
		powf(2.0f, -(float)m_debug.m_shadowDebugGridDivisions1),
		powf(2.0f, -(float)m_debug.m_shadowDebugGridDivisions2),
		0.0f,
		0.0f
	);
	state.m_debug.m_shaderVars[2] = Vec4V(
		m_debug.m_shadowDebugSaturation,
		0.0f, // unused
		m_debug.m_ditherWorld_NOT_USED ? 1.0f : 0.0f,
		m_debug.m_ditherScale_NOT_USED
	);

	state.m_debug.UpdateDebugObjects(
		state,
		m_debug.m_drawFlags,
		m_debug.m_drawOpacity,
		m_debug.m_cascadeBoundsForceOverlap,
		splitZ,
		viewToWorld,
		tanHFOV,
		tanVFOV
	);

#endif // __BANK

	// ============================================================================================
	// ============================================================================================

	// setup scan info
	{
		fwScanCascadeShadowInfo& scan = m_scanCascadeShadowInfo;

		const ScalarV camFOVThresh            = bHighAltitude ? ScalarV(V_ZERO) : Max(tanHFOV, tanVFOV)*ScalarV(BANK_SWITCH(m_debug.m_scanCamFOVThreshold, 0.5f)/(576.0f*4.0f));
		Vec4V         cascadeSpheres[4]       = {Vec4V(V_ZERO), Vec4V(V_ZERO), Vec4V(V_ZERO), Vec4V(V_ZERO)};
		const Vec4V   cascadeSphereThresholds = bHighAltitude ? Vec4V(V_ZERO) : BANK_SWITCH(m_debug.m_scanSphereThresholdTweaks*ScalarV(m_debug.m_scanSphereThreshold), CSM_DEFAULT_SPHERE_THRESHOLD_BIAS_V);
		const Mat34V  cascadeToWorld          = Mat34V(m_shadowToWorld33, -camPos);

		const float boxExactThresholdNumPixels = DEV_SWITCH(m_debug.m_scanBoxExactThreshold*m_debug.m_scanBoxExactThreshold, CSM_DEFAULT_BOX_EXACT_THRESHOLD*CSM_DEFAULT_BOX_EXACT_THRESHOLD);
		const Vec4V boxExactThresholds = Vec4V(
			state.m_cascadeBoundsSphere[0].GetW(),
			state.m_cascadeBoundsSphere[1].GetW(),
			state.m_cascadeBoundsSphere[2].GetW(),
			state.m_cascadeBoundsSphere[3].GetW()
		)*ScalarV(boxExactThresholdNumPixels*2.0f/(float)CRenderPhaseCascadeShadowsInterface::CascadeShadowResX());

		for (int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
		{
			cascadeSpheres[i] = state.m_cascadeBoundsSphere[i];
		}

		ScalarV z1 = splitZ[CASCADE_SHADOWS_COUNT];

		if (DEV_SWITCH(m_debug.m_useIrregularFade, CSM_USE_IRREGULAR_FADE) && !bHighAltitude)
		{
			// find distance to back of last cascade (inefficiently)
			const int sphereToUse = DEV_SWITCH(gCascadeShadows.m_debug.m_renderOccluders_enabled, false) ? (CASCADE_SHADOWS_COUNT - 2) : (CASCADE_SHADOWS_COUNT - 1);

			z1 = CalculateFarDistanceOnCascade(state.m_cascadeBoundsSphere[sphereToUse], cascadeToWorld);
		}

		state.m_nearClip              = vp.GetNearClip();
		state.m_farClip               = vp.GetFarClip();
		state.m_firstThreeCascadesEnd = z1.Getf();
		state.m_lastCascadeStart      = splitZ[CASCADE_SHADOWS_COUNT - 1].Getf()*BANK_SWITCH(m_debug.m_shadowFadeStart, CSM_DEFAULT_FADE_START);
		state.m_lastCascadeEnd        = CalculateFarDistanceOnCascade(state.m_cascadeBoundsSphere[CASCADE_SHADOWS_COUNT - 1], cascadeToWorld).Getf();

#if __BANK
		if (TiledScreenCapture::IsEnabledOrthographic())
		{
			spdAABB tileBounds;
			TiledScreenCapture::GetTileBounds(tileBounds);
			scan.SetCameraOrthographic(viewToWorld, tileBounds.GetMin(), tileBounds.GetMax(), m_shadowToWorld33);
		}
		else
#endif // __BANK
		{
			const Vector2 shearVector = vp.GetPerspectiveShear();
			scan.SetCamera(viewToWorld, tanHFOV, tanVFOV, z1, camFOVThresh, m_shadowToWorld33, worldToShadow33, 
			               recvrHeight, ScalarV(shearVector.x), ScalarV(shearVector.y));
		}

		scan.SetCascadeSpheres(cascadeSpheres, cascadeSphereThresholds*ScalarV(1.0f/(float)Max<int>(CRenderPhaseCascadeShadowsInterface::CascadeShadowResX(), CRenderPhaseCascadeShadowsInterface::CascadeShadowResY())), boxExactThresholds);
#if CASCADE_SHADOWS_SCAN_DEBUG
		scan.SetDebugInfo(m_debug.m_scanDebug);
#endif // CASCADE_SHADOWS_SCAN_DEBUG
		scan.EnableGrassCascadeCulling(AllowGrassCacadeCulling(state.m_lastCascadeEnd));
		const Settings& settings = CSettingsManager::GetInstance().GetSettings();
		scan.EnableShadowScreenSizeCheck(gCascadeShadows.m_enableScreenSizeCheck && !settings.m_graphics.m_Shadow_DisableScreenSizeCheck);

		u8 forceVisFlags = 0;
		VecBoolV cascadeExclusionFlags(V_TRUE);

		if (CSMShadowTypeIsHighAltitude(m_scriptShadowType))
		{
			if (m_scriptQuadrantTrackForceVisAll)
			{
				forceVisFlags |= 0x0f;
			}
			else
			{
				cascadeExclusionFlags = VecBoolV(V_FALSE);
			}
		}
		else if (CVfxHelper::GetGameCamWaterDepth() > 0.0f) // this prevents holes in underwater godray shadows
		{
			switch (CASCADE_SHADOWS_PS_CASCADE_INDEX)
			{
			case 0: cascadeExclusionFlags.SetX(BoolV(V_FALSE)); break;
			case 1: cascadeExclusionFlags.SetY(BoolV(V_FALSE)); break;
			case 2: cascadeExclusionFlags.SetZ(BoolV(V_FALSE)); break;
			case 3: cascadeExclusionFlags.SetW(BoolV(V_FALSE)); break;
			}
		}

		const bool bForceNoBackfacingPlanes = CVfxHelper::GetGameCamWaterDepth() > 0.0f && BANK_SWITCH(m_debug.m_scanForceNoBackfacingPlanesWhenUnderwater, true);

		scan.SetForceVisibilityFlags(forceVisFlags, bForceNoBackfacingPlanes, cascadeExclusionFlags);

		Vec4V viewerPortal = Vec4V(Vec2V(V_NEGONE), Vec2V(V_ONE)); // {-1,-1,+1,+1}		

#if __DEV
		if (m_debug.m_scanViewerPortalEnabled)
		{
			viewerPortal = m_debug.m_scanViewerPortal;

			CSMDebugDrawViewerPortal(viewToWorld, tanHFOV, tanVFOV, zNearClip, viewerPortal);
		}
#endif // __DEV

		fwScanCSMSortedPlaneInfo spi; // only used for debug

		Vec4V     planes[CASCADE_SHADOWS_SCAN_NUM_PLANES_EXCESS];
		const int planeCount = scan.UpdateShadowViewport(planes, &spi, viewerPortal);

#if CASCADE_SHADOWS_SCAN_DEBUG
		if (m_debug.m_drawFlags.m_drawSilhouettePolygon)
		{
			CSMDebugDrawSilhouettePolygon(state, spi, planeCount, m_debug.m_drawOpacity);
		}
#endif // CASCADE_SHADOWS_SCAN_DEBUG

		state.m_shadowToWorld33      = m_shadowToWorld33;
		state.m_shadowClipPlaneCount = planeCount;

		for (int i = 0; i < planeCount; i++) // save planes in state so we can set up edge
		{
			state.m_shadowClipPlanes[i] = planes[i];
		}

		for (int i = planeCount; i < CASCADE_SHADOWS_SCAN_NUM_PLANES; i++)
		{
			planes[i] = Vec4V(V_ZERO);
		}

		const camBaseCamera* activeCamera = camInterface::GetDominantRenderedCamera();
		const CEntity* pValidTarget = NULL;

		if (activeCamera)
		{
			pValidTarget = activeCamera->GetAttachParent();

			if (!pValidTarget)
			{
				pValidTarget = activeCamera->GetLookAtTarget();
			}
		}
		
		scan.EnableCascadeLodDistanceCulling(pValidTarget && CSMShadowTypeIsCascade(m_scriptShadowType) && !camInterface::GetCutsceneDirector().IsCutScenePlaying());
		scan.SetCascadeLodDistanceScalesV(Vec4V(V_ONE));
	}

#if __BANK
	if (m_debug.m_showInfo)
	{
		CSMShowInfo(state, worldHeight, recvrHeight);
	}
#endif // __BANK

#if USE_NV_SHADOW_LIB
	state.m_eyeWorldToView = vp.GetViewMtx();
	state.m_eyeViewToProjection = vp.GetProjection();
#endif
#if USE_AMD_SHADOW_LIB
	Mat44V projection = vp.GetProjection();
	// Need to account for inverted z
	{
		float zNorm = projection.GetCol2().GetZf();
		float zNearNorm = projection.GetCol3().GetZf();
		zNearNorm = -zNearNorm; 
		zNorm = -zNorm - 1.0f;
		projection.GetCol2Ref().SetZf(zNorm);
		projection.GetCol3Ref().SetZf(zNearNorm);
	}
	Mat44V viewProjection;
	Multiply(viewProjection, projection, vp.GetViewMtx());
	state.m_eyeViewProjection = viewProjection;
	InvertFull(state.m_eyeViewProjectionInv, viewProjection);
#endif
}

#if CASCADE_SHADOWS_STENCIL_REVEAL && __BANK

void CCSMGlobals::ShadowStencilRevealDebug(bool bDrawToGBuffer0) const
{
	if (m_debug.m_shadowStencilRevealDebug)
	{
		if (bDrawToGBuffer0)
		{
			grcTextureFactory::GetInstance().LockRenderTarget(0, GBuffer::GetTarget(GBUFFER_RT_0), CRenderTargets::GetDepthBuffer());
		}

		const CCSMState& state = GetRenderState();
		const Mat44V shadowToWorld = Mat44V(state.m_shadowToWorld33, Vec4V(V_ZERO));

		if (m_debug.m_shadowStencilRevealWireframe)
		{
			grcStateBlock::SetWireframeOverride(true);
		}

		for (int cascadeIndex = 0; cascadeIndex < BANK_SWITCH(m_debug.m_shadowStencilRevealCount, CASCADE_SHADOWS_COUNT); cascadeIndex++)
		{
			const Vec4V cascadeSphere = TransformSphere(state.m_shadowToWorld33, state.m_cascadeBoundsSphere[cascadeIndex]);
			const Vec4V cascadeColours[] =
			{
				Vec4V(V_X_AXIS_WONE),
				Vec4V(V_Y_AXIS_WONE),
				Vec4V(V_Z_AXIS_WONE),
				Vec4V(1.0f, 0.0f, 1.0f, 1.0f), // magenta
			};

			m_shadowProcessingShader->SetVar(m_shadowProcessingShaderStencilRevealShadowToWorld, shadowToWorld);
			m_shadowProcessingShader->SetVar(m_shadowProcessingShaderStencilRevealCascadeSphere, cascadeSphere);
			m_shadowProcessingShader->SetVar(m_shadowProcessingShaderStencilRevealCascadeColour, cascadeColours[cascadeIndex]);

			if (AssertVerify(m_shadowProcessingShader->BeginDraw(grmShader::RMC_DRAW, false, m_shadowProcessingShaderTechnique)))
			{
				m_shadowProcessingShader->Bind(shadowprocessing_StencilRevealDebug);

				m_volumeVertexBuffer.Draw();

				m_shadowProcessingShader->UnBind();
				m_shadowProcessingShader->EndDraw();
			}
		}

		if (m_debug.m_shadowStencilRevealWireframe)
		{
			grcStateBlock::SetWireframeOverride(false);
		}

		if (bDrawToGBuffer0)
		{
			grcTextureFactory::GetInstance().UnlockRenderTarget(0);
		}
	}
}

#endif // CASCADE_SHADOWS_STENCIL_REVEAL && __BANK

#if CASCADE_SHADOWS_DO_SOFT_FILTERING

void CCSMGlobals::SoftShadowToScreen() const
{
	const grcTexture *src = m_softShadowIntermediateRT;
	const grcTexture *depth = CRenderTargets::GetDepthBuffer();
	const ShadowProcessingSetup &setup = m_shadowProcessing[MM_DEFAULT];

#if DEVICE_MSAA
	if (GRCDEVICE.GetMSAA()>1 && m_softShadowIntermediateRT && m_softShadowIntermediateRT_Resolved)
	{
		static_cast<grcRenderTargetMSAA*>(m_softShadowIntermediateRT)->Resolve( m_softShadowIntermediateRT_Resolved );
		setup.shader->SetVar(setup.intermediateAAVar, m_softShadowIntermediateRT);
		src = m_softShadowIntermediateRT_Resolved;
	}
#endif	//DEVICE_MSAA

	// Set source texture(s) and depth source.
	setup.shader->SetVar(setup.intermediateVar, src);
	setup.shader->SetVar(setup.depthBuffer2Var, depth);

	// Set projection parameters.
	const Vector4 projParams = ShaderUtil::CalculateProjectionParams();
	setup.shader->SetVar(setup.projectionParamsVar, projParams);

	// Set target size parameters.
	float W = (float)VideoResManager::GetSceneWidth();
	float H = (float)VideoResManager::GetSceneHeight();
	Vector4 targetSize = Vector4(W, H, 1.0f/W, 1.0f/H);
	setup.shader->SetVar(setup.targetSizeParamVar, targetSize);

	// Set kernel size parameters.
	const float penumbraRadius = BANK_SWITCH(m_debug.m_softShadowPenumbraRadius, CSM_DEFAULT_SOFT_SHADOW_PENUMBRA_RADIUS);
	const int maxKernelSize = BANK_SWITCH(m_debug.m_softShadowMaxKernelSize, CSM_DEFAULT_SOFT_SHADOW_MAX_KERNEL_SIZE);
	Vector4 kernelParams = Vector4(penumbraRadius*penumbraRadius, (float)maxKernelSize, 2.0f*projParams.x/W, 2.0f*projParams.y/H);
	setup.shader->SetVar(setup.kernelParamVar, kernelParams);
	SoftShadowTechnique finalTechnique = SoftShadowBlur_TechCount;

	//Disable the soft shadow early out for now on Durango, its causing issues that looks to be a driver bug, we'll try re-enabling it with new XDK's.
	if (BANK_SWITCH(m_debug.m_softShadowUseEarlyOut, true))
	{
		// Create the 1st level early out texture.
		SoftShadowFullScreenBlit(setup, m_softShadowEarlyOut1RT, NULL, SoftShadowBlur_CreateEarlyOut1);

		// Create the 2nd level early out texture.
		setup.shader->SetVar(setup.earlyOutVar, m_softShadowEarlyOut1RT);
		setup.shader->SetVar(setup.earlyOutParamsVar, Vector4(1.0f/(float)m_softShadowEarlyOut1RT->GetWidth(), 1.0f/(float)m_softShadowEarlyOut1RT->GetHeight(), 0.0f, 0.0f));
		SoftShadowFullScreenBlit(setup, m_softShadowEarlyOut2RT, NULL, SoftShadowBlur_CreateEarlyOut2);

		// Accumulate the soft shadow values with current lighting values.
		setup.shader->SetVar(setup.earlyOutVar, m_softShadowEarlyOut2RT);
		setup.shader->SetVar(setup.earlyOutParamsVar, Vector4(1.0f/(float)m_softShadowEarlyOut2RT->GetWidth(), 1.0f/(float)m_softShadowEarlyOut2RT->GetHeight(), 0.0f, 0.0f));

		if (BANK_SWITCH(m_debug.m_softShadowShowEarlyOut, false))
		{
			finalTechnique = SoftShadowBlur_ShowEarlyOut;
		}
		else
		{
#if RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
			if (gCascadeShadows.m_enableParticleShadows || gCascadeShadows.m_enableAlphaShadows)
			{
				finalTechnique = SoftShadowBlur_EarlyOutWithParticleCombine;
			}
			else
#endif // RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
			{
				finalTechnique = SoftShadowBlur_EarlyOut;
			}
		}
	}
	else
	{
		// Accumulate the soft shadow values with current lighting values.
#if RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
		if (gCascadeShadows.m_enableParticleShadows || gCascadeShadows.m_enableAlphaShadows)
		{
			finalTechnique = SoftShadowBlur_Optimised2ParticleCombine;
		}
		else
#endif // RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
		{
			finalTechnique = SoftShadowBlur_Optimised2;
		}	
	}

	grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, m_shadowRevealShaderState_DS, m_shadowRevealShaderState_BS);
	grcRenderTarget *dst = GBuffer::GetTarget(GBUFFER_RT_2);
#if MSAA_EDGE_PASS
	if (DeferredLighting::IsEdgePassActive() BANK_ONLY(&& DebugDeferred::m_OptimizeShadowProcessing))
	{
		grcRenderTarget *const depthRT = 
	#if RSG_PC
			CRenderTargets::GetDepthBufferCopy();
	#else
			CRenderTargets::GetDepthBuffer();
	#endif
		g_ShadowRevealTime_mask_reveal_MS0.Start();
		BANK_ONLY(if (DebugDeferred::m_EnableShadowProcessingFace))
		{
			const ShadowProcessingSetup &altSetup = m_shadowProcessing[MM_TEXTURE_READS_ONLY];
			altSetup.shader->SetVar(altSetup.intermediateAAVar, m_softShadowIntermediateRT);
			altSetup.shader->SetVar(altSetup.intermediateVar, src);
			altSetup.shader->SetVar(altSetup.depthBuffer2Var, depth);
			altSetup.shader->SetVar(altSetup.projectionParamsVar, projParams);
			altSetup.shader->SetVar(altSetup.targetSizeParamVar, targetSize);
			altSetup.shader->SetVar(altSetup.kernelParamVar, kernelParams);
			altSetup.shader->SetVar(altSetup.earlyOutVar, m_softShadowEarlyOut2RT);
			altSetup.shader->SetVar(altSetup.earlyOutParamsVar, Vector4(1.0f/(float)m_softShadowEarlyOut2RT->GetWidth(), 1.0f/(float)m_softShadowEarlyOut2RT->GetHeight(), 0.0f, 0.0f));
			
			grcStateBlock::SetDepthStencilState(DeferredLighting::m_directionalEdgeMaskEqualPass_DS, 0);
			SoftShadowFullScreenBlit(altSetup, dst, depthRT, finalTechnique);
		}
		g_ShadowRevealTime_mask_reveal_MS0.Stop();
		g_ShadowRevealTime_mask_reveal_MS.Start();
		BANK_ONLY(if (DebugDeferred::m_EnableShadowProcessingEdge))
		{
			grcStateBlock::SetDepthStencilState(DeferredLighting::m_directionalEdgeMaskEqualPass_DS, EDGE_FLAG);
			SoftShadowFullScreenBlit(setup, dst, depthRT, finalTechnique);
		}
		g_ShadowRevealTime_mask_reveal_MS.Stop();
	}else
#endif //MSAA_EDGE_PASS
	{
		SoftShadowFullScreenBlit(setup, dst, NULL, finalTechnique);
	}
}

void CCSMGlobals::SoftShadowFullScreenBlit(const ShadowProcessingSetup &setup, grcRenderTarget* pTarget, grcRenderTarget *pDepth, SoftShadowTechnique techIndex) const
{
	grcTextureFactory::GetInstance().LockRenderTarget(0, pTarget, pDepth);

	if (AssertVerify(setup.shader->BeginDraw(grmShader::RMC_DRAW, false, setup.techniqueBlur[techIndex])))
	{
		setup.shader->Bind();
		{
			const Color32 colour = Color32(255,255,255,255);

			const float x0 = -1.0f;
			const float y0 = +1.0f;
			const float x1 = +1.0f;
			const float y1 = -1.0f;
			const float z  =  0.0f;

			grcBegin(drawTriStrip, 4);
			grcVertex(x0, y0, z, 0.0f, 0.0f, -1.0f, colour, 0.0f, 0.0f);
			grcVertex(x0, y1, z, 0.0f, 0.0f, -1.0f, colour, 0.0f, 1.0f);
			grcVertex(x1, y0, z, 0.0f, 0.0f, -1.0f, colour, 1.0f, 0.0f);
			grcVertex(x1, y1, z, 0.0f, 0.0f, -1.0f, colour, 1.0f, 1.0f);
			grcEnd();
		}
		setup.shader->UnBind();
		setup.shader->EndDraw();
	}
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
}

#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING

void CCSMGlobals::ShadowRevealToScreen(const ShadowRevealSetup& setup, EdgeMode emode, grcEffectTechnique tech, float nearClip, float farClip, float nearZ, float farZ, bool /*useWindows*/) const
{
	grcDepthStencilStateHandle depthStencilState =
#if MSAA_EDGE_PASS
		emode != EM_IGNORE ? DeferredLighting::m_directionalEdgeMaskEqualPass_DS :
#endif
		m_shadowRevealShaderState_DS;

	eCSMSampleType sampleType = GetRenderState().m_sampleType;
	Assert(sampleType != CSM_ST_INVALID);
	(void)emode;

	float z = 0.0f;

#if DEPTH_BOUNDS_SUPPORT

	const float linearMin = Clamp<float>(nearZ, nearClip, farClip);
	const float linearMax = Clamp<float>(farZ,  nearClip, farClip);

	const float Q = farClip/(farClip - nearClip);

#if USE_INVERTED_PROJECTION_ONLY
	const float projMin = 1.0f - ((Q/linearMin)*(linearMin - nearClip));
	const float projMax = 1.0f - ((Q/linearMax)*(linearMax - nearClip));
#else
	const float projMin = (Q/linearMin)*(linearMin - nearClip);
	const float projMax = (Q/linearMax)*(linearMax - nearClip);
#endif
	z = projMin;

#if USE_INVERTED_PROJECTION_ONLY
	if(projMax >= projMin) //sometimes loss of precision causes projected depth to be equal to the farclip, don't render anything in this cause(asserts on d3d11)
		return;
#else // USE_INVERTED_PROJECTION_ONLY
	if(projMin >= projMax) //sometimes loss of precision causes projected depth to be equal to the farclip, don't render anything in this cause(asserts on d3d11)
		return;
#endif // USE_INVERTED_PROJECTION_ONLY

	BANK_ONLY(if(m_debug.m_useDepthBounds))
	{
#if RSG_PC
		PF_PUSH_TIMEBAR_DETAIL("Shadow Reveal - Depth Bounds");
#endif // RSG_PC
	
		grcDevice::SetDepthBoundsTestEnable(true);
		float depthBoundMin = Min(projMin, projMax);
		float depthBoundMax = Max(projMin, projMax);
		grcDevice::SetDepthBounds(depthBoundMin,depthBoundMax);

#if RSG_PC
		PF_POP_TIMEBAR_DETAIL();
#endif // RSG_PC
	}
#if (RSG_ORBIS || RSG_DURANGO) && __BANK
	else
	{
		depthStencilState = 
	#if MSAA_EDGE_PASS
			emode != EM_IGNORE ? m_shadowRevealShaderState_DS_noDepthBounds_edge :
	#endif
			m_shadowRevealShaderState_DS_noDepthBounds;
	}
#endif //(RSG_ORBIS || RSG_DURANGO) && __BANK


#else //DEPTH_BOUNDS_SUPPORT
	(void) nearZ;
	(void) farZ;
	(void) nearClip;
	(void) farClip;
#endif //DEPTH_BOUNDS_SUPPORT

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
	if (m_softShadowEnabled && tech != m_shadowReveal[MM_DEFAULT].techniqueCloudShadows BANK_ONLY(&& !TiledScreenCapture::IsEnabled()))
	{
		if(!GetRenderWindowRenderer().UseExtrusion())
			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		else
			grcStateBlock::SetBlendState(m_shadowRevealShaderState_SoftShadow_BS);
	}
	else
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING
		grcStateBlock::SetBlendState(m_shadowRevealShaderState_BS);

#if CSM_PORTALED_SHADOWS
	if (GetRenderWindowRenderer().UseExtrusion())
	{
#if RSG_PC
		grcStateBlock::SetDepthStencilState(ExteriorPortals::GetRevealDS(), DEFERRED_MATERIAL_SPAREOR1 );
#endif
		Assertf(emode == EM_IGNORE, "Portalled shadows are not compatible with MSAA edge optimization");
	}else
#endif // CSM_PORTALED_SHADOWS
	{
		grcStateBlock::SetDepthStencilState(depthStencilState,
#if MSAA_EDGE_PASS
			emode == EM_EDGE0 ? EDGE_FLAG :
#endif // MSAA_EDGE_PASS
			0);
	}

	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	if(
#if __BANK
       tech == setup.techniqueOrthographic ||
	   tech == setup.techniqueOrthographicNoSoft ||
	   TiledScreenCapture::IsEnabledOrthographic() ||
#endif //__BANK
	   tech == setup.techniqueCloudAndParticlesOnly ||
	   tech == setup.techniqueCloudShadows
	  )
		sampleType = (eCSMSampleType)0; // These techniques have only one pass.


	PF_PUSH_TIMEBAR_DETAIL("Shadow Reveal Draw");

	if (AssertVerify(setup.shader->BeginDraw(grmShader::RMC_DRAW, false, tech)))
	{
		setup.shader->Bind(sampleType);
		grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, z, 0.0f, 0.0f, 1.0f, 1.0f, Color_black);
		setup.shader->UnBind();
	}
	setup.shader->EndDraw();

	if (m_softShadowEnabled && tech != m_shadowReveal[MM_DEFAULT].techniqueCloudShadows BANK_ONLY(&& !TiledScreenCapture::IsEnabled()))
	{
		if(GetRenderWindowRenderer().UseExtrusion())
			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	}

#if DEPTH_BOUNDS_SUPPORT
	BANK_ONLY(if(m_debug.m_useDepthBounds))
		grcDevice::SetDepthBoundsTestEnable(false);
#endif //DEPTH_BOUNDS_SUPPORT

	PF_POP_TIMEBAR_DETAIL();
}

void CCSMGlobals::ShadowRevealToScreenHelper(const ShadowRevealSetup &setup, EdgeMode emode, const CCSMState &state) const
{
#if __BANK
	if (TiledScreenCapture::IsEnabledOrthographic())
	{
		ShadowRevealToScreen(setup, emode,SOFT_FILTERING_ONLY(!m_softShadowEnabled ? setup.techniqueOrthographicNoSoft : ) setup.techniqueOrthographic, state.m_nearClip, state.m_farClip, 0.0f, state.m_lastCascadeEnd);
	}
	else 
#endif // __BANK
	if (DEV_SWITCH(m_debug.m_renderOccluders_enabled && m_debug.m_renderOccluders_useTwoPasses, false))
	{
		if (DEV_SWITCH(m_debug.m_renderOccluders_firstThreeCascades, true))
		{
			ShadowRevealToScreen(setup, emode, SOFT_FILTERING_ONLY( !m_softShadowEnabled ? setup.techniqueFirstThreeCascadesNoSoft : ) setup.techniqueFirstThreeCascades, state.m_nearClip, state.m_farClip, 0.0f, state.m_firstThreeCascadesEnd);
		}

		if (DEV_SWITCH(m_debug.m_renderOccluders_lastCascade, true))
		{
			// Don't use this, need to change worldToShadow mtx instead.	- TODO make it work
			//grcViewport::GetCurrent()->SetWorldMtx( state.m_farTransform);
			ShadowRevealToScreen(setup, emode, SOFT_FILTERING_ONLY( !m_softShadowEnabled ? setup.techniqueLastCascadeNoSoft : ) setup.techniqueLastCascade, state.m_nearClip, state.m_farClip, state.m_lastCascadeStart, state.m_lastCascadeEnd);
		}
	}
	else
	{
#if USE_AMD_SHADOW_LIB
		if (m_enableAMDShadows && !m_softShadowEnabled)
		{
			AMDShadowReveal(state);

			// This is inefficient, but the simplest to implement.  We do a pass of a modified shadow technique which adds the cloud shadow effect and 
			// particle shadows (but no CSM). Ideally, this ShadowRevealToScreen would be combined into a single pass with AMDShadowReveal.  But integrating 
			// the two is more complex.  All the passes are multiplicatively blended anyway, including the m_shadowRevealShaderTechniqueCloudShadows pass above.
			ShadowRevealToScreen(setup, emode, setup.techniqueCloudAndParticlesOnly, state.m_nearClip, state.m_farClip, 0.0f, state.m_lastCascadeEnd);
		}
		else
#endif	// USE_AMD_SHADOW_LIB
#if USE_NV_SHADOW_LIB
		if (m_enableNvidiaShadows && !m_softShadowEnabled)
		{
			NvShadowReveal(state);

			// This is inefficient, but the simplest to implement.  We do a pass of a modified shadow technique which adds the cloud shadow effect and 
			// particle shadows (but no CSM). Ideally, this ShadowRevealToScreen would be combined into a single pass with NvShadowReveal.  But integrating 
			// the two is more complex.  All the passes are multiplicatively blended anyway, including the m_shadowRevealShaderTechniqueCloudShadows pass above.
			ShadowRevealToScreen(setup, emode, setup.techniqueCloudAndParticlesOnly, state.m_nearClip, state.m_farClip, 0.0f, state.m_lastCascadeEnd);
		}
		else
#endif	// USE_NV_SHADOW_LIB
		{
			// The non-Nvidia, non-AMD version.
			ShadowRevealToScreen(setup, emode, SOFT_FILTERING_ONLY( !m_softShadowEnabled ? setup.techniqueNoSoft : ) setup.technique, state.m_nearClip, state.m_farClip, 0.0f, state.m_lastCascadeEnd);
		}
	} 
}

static void GenerateSmoothStepRT(const ShadowRevealSetup &setup, float minVal, float maxVal, float lerpVal, float lerpAmount)
{
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetBlendState       (grcStateBlock::BS_Default);
	grcStateBlock::SetRasterizerState  (grcStateBlock::RS_Default);

	grcTextureFactory::GetInstance().LockRenderTarget(0, gCascadeShadows.m_smoothStepRT, NULL);

	if (AssertVerify(setup.shader->BeginDraw(grmShader::RMC_DRAW, false, setup.techniqueSmoothStep)))
	{
		setup.shader->Bind(0);
		grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f , minVal, 0.0f, maxVal, 1.0f, maxVal, Color32(lerpVal, lerpAmount, 0.0f, 0.0f));
		setup.shader->UnBind();
	}
	setup.shader->EndDraw();

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
}

#if USE_NV_SHADOW_LIB
// Invoke debug fns.
void CCSMGlobals::NvShadowRevealDebug(const CCSMState& state) const
{
	if (GRCDEVICE.GetDxFeatureLevel() >= 1100)
	{
		NV_ShadowLib_Status nvStatus;
		grcRenderTarget* shadowMap = m_shadowMapBase;
		ID3D11ShaderResourceView* pShadowMapSRV = reinterpret_cast<grcRenderTargetDX11*>(shadowMap)->GetTextureView();
		NV_ShadowLib_ExternalMapDesc* pMapDesc = const_cast<NV_ShadowLib_ExternalMapDesc*>(&m_mapDesc);
		NV_ShadowLib_BufferRenderParams* pParams = const_cast<NV_ShadowLib_BufferRenderParams*>(&m_shadowParams);
		CopyToNvState(state, *pMapDesc);

		grcRenderTarget* grcDepth = GBuffer::GetDepthTarget();
		grcRenderTargetDX11* hackyCastDepth = reinterpret_cast<grcRenderTargetDX11*>(grcDepth);
		pParams->DepthBufferDesc.pDepthStencilSRV = hackyCastDepth->GetTextureView();

		if (gCascadeShadows.m_nvShadowsDebugVisualizeDepth)
		{
			// Clearly the 2nd call overwrites the 1st.  Comment out one or use Nsight or PIX to inspect both.
			D3DPERF_BeginEvent(D3DCOLOR_ARGB(0,0,0,0), L"NV Dev Mode depth");
			NV_ShadowLib_ClearBuffer(&g_shadowContext, m_shadowBufferHandle);
			NV_ShadowLib_DevModeToggleDebugEyeViewZShader(&g_shadowContext, m_shadowBufferHandle, true);
			nvStatus = NV_ShadowLib_RenderBufferUsingExternalMap(&g_shadowContext, pMapDesc, pShadowMapSRV, m_shadowBufferHandle, pParams);
			Assert(nvStatus == NV_ShadowLib_Status_Ok);
			NV_ShadowLib_DevModeToggleDebugEyeViewZShader(&g_shadowContext, m_shadowBufferHandle, false);
			D3DPERF_EndEvent();
		}

		if (gCascadeShadows.m_nvShadowsDebugVisualizeCascades)
		{
			// Lets you see the lib's idea of where the cascade geometry is in the scene.
			D3DPERF_BeginEvent(D3DCOLOR_ARGB(0,0,0,0), L"NV Dev Mode Cascade Shader");
			NV_ShadowLib_ClearBuffer(&g_shadowContext, m_shadowBufferHandle);
			NV_ShadowLib_DevModeToggleDebugCascadeShader(&g_shadowContext, m_shadowBufferHandle, true);
			nvStatus = NV_ShadowLib_RenderBufferUsingExternalMap(&g_shadowContext, pMapDesc, pShadowMapSRV, m_shadowBufferHandle, pParams);
			Assert(nvStatus == NV_ShadowLib_Status_Ok);
			NV_ShadowLib_DevModeToggleDebugCascadeShader(&g_shadowContext, m_shadowBufferHandle, false);
			D3DPERF_EndEvent();
		}
	}
}

void CCSMGlobals::NvShadowReveal(const CCSMState& state) const
{
	if (GRCDEVICE.GetDxFeatureLevel() >= 1100)
	{
		D3DPERF_BeginEvent(D3DCOLOR_ARGB(0,0,0,0), L"NV Shadow Reveal");
		NV_ShadowLib_Status nvStatus;
		grcRenderTarget* shadowMap = m_shadowMapBase;
		ID3D11ShaderResourceView* pShadowMapSRV = reinterpret_cast<grcRenderTargetDX11*>(shadowMap)->GetTextureView();
		NV_ShadowLib_ExternalMapDesc* pMapDesc = const_cast<NV_ShadowLib_ExternalMapDesc*>(&m_mapDesc);
		NV_ShadowLib_BufferRenderParams* pParams = const_cast<NV_ShadowLib_BufferRenderParams*>(&m_shadowParams);
		CopyToNvState(state, *pMapDesc);

		grcRenderTarget* grcDepth = GBuffer::GetDepthTarget();

		if ((GRCDEVICE.GetMSAA()>1))
		{
#if __BANK
			CRenderTargets::ResolveDepth((MSDepthResolve)m_debug.m_nvidiaResolve);
#else
			CRenderTargets::ResolveDepth(depthResolve_Farthest);
#endif
			grcDepth = CRenderTargets::GetDepthResolved();
			g_NeedResolve = false;
		}

		grcRenderTargetDX11* hackyCastDepth = reinterpret_cast<grcRenderTargetDX11*>(grcDepth);
		pParams->DepthBufferDesc.pDepthStencilSRV = hackyCastDepth->GetTextureView();

		pParams->eTechniqueType = NV_ShadowLib_TechniqueType_PCSS;
		pParams->eQualityType = NV_ShadowLib_QualityType_Adaptive;

		NV_ShadowLib_ClearBuffer(&g_shadowContext, m_shadowBufferHandle);

		if (gCascadeShadows.m_nvShadowsDebugVisualizeCascades || gCascadeShadows.m_nvShadowsDebugVisualizeDepth)
		{
			NvShadowRevealDebug(state);
		}
		else
		{
			D3DPERF_BeginEvent(D3DCOLOR_ARGB(0,0,0,0), L"NV Render Buffer");
			nvStatus = NV_ShadowLib_RenderBufferUsingExternalMap(&g_shadowContext, pMapDesc, pShadowMapSRV, m_shadowBufferHandle, pParams);
			Assert(nvStatus == NV_ShadowLib_Status_Ok);
			D3DPERF_EndEvent();
		}

		ID3D11ShaderResourceView* pResultSRV = NULL;
		D3DPERF_BeginEvent(D3DCOLOR_ARGB(0,0,0,0), L"NV Finalize");
		nvStatus = NV_ShadowLib_FinalizeBuffer(&g_shadowContext, m_shadowBufferHandle, &pResultSRV);
		Assert(nvStatus == NV_ShadowLib_Status_Ok);
		D3DPERF_EndEvent();

		D3DPERF_BeginEvent(D3DCOLOR_ARGB(0,0,0,0), L"Copy to RT2");
		const gfsdk_float3 black = {0,0,0};
		grcRenderTarget* dst = GBuffer::GetTarget(GBUFFER_RT_2);
		grcRenderTargetDX11* hackyCastRT2 = reinterpret_cast<grcRenderTargetDX11*>(dst);
		const grcDeviceView* devView = hackyCastRT2->GetTargetView();
		ID3D11RenderTargetView* rt2SRV = reinterpret_cast<ID3D11RenderTargetView*>(const_cast<grcDeviceView*>(devView));
		NV_ShadowLib_ModulateBuffer(&g_shadowContext, m_shadowBufferHandle, rt2SRV, black);
		D3DPERF_EndEvent();

		D3DPERF_EndEvent();
	}
}
#endif // USE_NV_SHADOW_LIB

#if USE_AMD_SHADOW_LIB
// Invoke debug fns.
void CCSMGlobals::AMDShadowRevealDebug(const CCSMState& state) const
{
	if (GRCDEVICE.GetDxFeatureLevel() >= 1100)
	{
		CopyToAMDState(state, g_amdShadowDesc, 0);
	}
}

void CCSMGlobals::AMDShadowReveal(const CCSMState& state) const
{
	if (GRCDEVICE.GetDxFeatureLevel() >= 1100)	//TODO: check for the enabled flag instead? early exit, if not
	{
		D3DPERF_BeginEvent(D3DCOLOR_ARGB(0,0,0,0), L"AMD Shadow Reveal");

		// shadow library takes GBuffer depth as input
		grcRenderTarget* grcDepth = GBuffer::GetDepthTarget();
		
		if (!GRCDEVICE.IsReadOnlyDepthAllowed())
			grcDepth = CRenderTargets::GetDepthBufferCopy();

		// AMD Shadow library targets
		grcRenderTarget* targetColor = GBuffer::GetTarget(GBUFFER_RT_2);
		grcRenderTarget* targetDepthMask = GBuffer::GetDepthTarget();
		grcRenderTarget* targetDepthOutput = CRenderTargets::GetDepthBuffer_ReadOnly();

		if (GRCDEVICE.GetMSAA() > 1)
		{
			CRenderTargets::ResolveDepth(depthResolve_Farthest);

			// or another resolve can be optimized out because of this one
			grcDepth = CRenderTargets::GetDepthResolved();
			targetColor = m_amdShadowResult;
			targetDepthMask = CRenderTargets::GetDepthResolved();
			targetDepthOutput = CRenderTargets::GetDepthBufferResolved_ReadOnly();

			grcTextureFactory::GetInstance().LockRenderTarget(0, m_amdShadowResult, grcDepth);
			GRCDEVICE.Clear(true, Color32(0xFFFFFFFF), false, 0.f, true, 0);
			grcTextureFactory::GetInstance().UnlockRenderTarget(0);

			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
			g_amdShadowDesc.m_pDssCleanUpStencil = NULL;	// no clear necessary
			// since we are not going to use the stencil from resolved depth target
		}else
		{
			grcStateBlock::SetBlendState(m_shadowRevealShaderState_BS);
			const void* dssForStencilClearing = GetDepthStencilStateRaw(m_amdStencilClearShaderState_DS);
			g_amdShadowDesc.m_pDssCleanUpStencil = static_cast<ID3D11DepthStencilState*>(const_cast<void*>(dssForStencilClearing));
		}

		// inputs are scene depth and the shadow map
		g_amdShadowDesc.m_pSrvDepth = reinterpret_cast<grcRenderTargetDX11*>(grcDepth)->GetTextureView();
		g_amdShadowDesc.m_pSrvShadow = reinterpret_cast<grcRenderTargetDX11*>(m_shadowMapBase)->GetTextureView();

		// AMD shadow library can also optionally take g-buffer normals as input, to allow an offset along the normals, 
		// but we aren't using this here, as it made tuning the cascade region transitions more difficult
		g_amdShadowDesc.m_pSrvNormal = NULL;

		g_amdShadowDesc.m_pRtvOutput		= (ID3D11RenderTargetView*)static_cast<grcRenderTargetDX11*>(targetColor)->GetTargetView();
		g_amdShadowDesc.m_pDsvStencilMasking= (ID3D11DepthStencilView*)static_cast<grcRenderTargetDX11*>(targetDepthMask)->GetTargetView();
		g_amdShadowDesc.m_pDsvOutput		= (ID3D11DepthStencilView*)static_cast<grcRenderTargetDX11*>(targetDepthOutput)->GetTargetView();

		// set up for masking out the cascade regions
		const void* dssForStencilMasking = GetDepthStencilStateRaw(m_amdStencilMarkShaderState_DS);
		g_amdShadowDesc.m_pDssStencilMasking = static_cast<ID3D11DepthStencilState*>(const_cast<void*>(dssForStencilMasking));
		const void* dssForShadowReveal = GetDepthStencilStateRaw(m_amdShadowRevealShaderState_DS);
		g_amdShadowDesc.m_pDssOutput = static_cast<ID3D11DepthStencilState*>(const_cast<void*>(dssForShadowReveal));
		g_amdShadowDesc.m_DssReference = AMD_CASCADE_FLAG;
		g_amdShadowDesc.m_EnableStencilMasking = true;

		g_amdShadowDesc.m_UnitCubeParameters.m_Scale = 1.0f - m_amdCascadeRegionBorder;
		g_amdShadowDesc.m_PreviousUnitCubeParameters.m_Scale = 1.0f - m_amdCascadeRegionBorder;

		// we aren't using the normal offset
		g_amdShadowDesc.m_Parameters.m_NormalOffsetScale = 0.0f;

		// use the engine's state system to set the right blend state, 
		// and flush to ensure it actually gets set (since we are doing non-engine draw calls)
		grcStateBlock::Flush();

		D3DPERF_BeginEvent(D3DCOLOR_ARGB(0,0,0,0), L"AMD Shadow Render");
		const float cascadeFactor = 1.0f / (float)CASCADE_SHADOWS_COUNT;
		for (int cascadeIndex = 0; cascadeIndex < CASCADE_SHADOWS_COUNT; cascadeIndex++)
		{
			g_amdShadowDesc.m_Parameters.m_SunWidth = m_amdSunWidth[cascadeIndex];
			g_amdShadowDesc.m_Parameters.m_DepthTestOffset = m_amdDepthOffset[cascadeIndex];
			switch (m_amdFilterKernelIndex[cascadeIndex])
			{
			case 0: g_amdShadowDesc.m_Radius = AMD::SHADOWS_RADIUS_7; break;
			case 1: g_amdShadowDesc.m_Radius = AMD::SHADOWS_RADIUS_9; break;
			case 2: g_amdShadowDesc.m_Radius = AMD::SHADOWS_RADIUS_11; break;
			case 3: g_amdShadowDesc.m_Radius = AMD::SHADOWS_RADIUS_15; break;
			}

			g_amdShadowDesc.m_Parameters.m_ShadowRegion.x = 0.0f;
			g_amdShadowDesc.m_Parameters.m_ShadowRegion.y = cascadeIndex*cascadeFactor;
			g_amdShadowDesc.m_Parameters.m_ShadowRegion.z = 1.0f;
			g_amdShadowDesc.m_Parameters.m_ShadowRegion.w = (cascadeIndex+1)*cascadeFactor;
			CopyToAMDState(state, g_amdShadowDesc, cascadeIndex);
			ASSERT_ONLY(AMD::SHADOWS_RETURN_CODE shadowStatus =) AMD::SHADOWS_Render(g_amdShadowDesc);
			grcAssertf(shadowStatus == AMD::SHADOWS_RETURN_CODE_SUCCESS, "AMD shadows error %d", shadowStatus);
		}
		D3DPERF_EndEvent();

		if (GRCDEVICE.GetMSAA())
		{
			// Copy from the temporary non-MSAA target onto GBuf2 alpha
			D3DPERF_BeginEvent(D3DCOLOR_ARGB(0,0,0,0), L"AMD Shadow Copy");

			grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, m_shadowRevealShaderState_BS);
			CSprite2d quad;
			CSprite2d::ResetBlitMatrix();
			quad.SetGeneralParams(Vector4(1.0f, 0.0f, 0.0f, 0.0f), Vector4(0.0f, 1.0f, 0.0f, 0.0f));
			quad.BeginCustomList(CSprite2d::CS_BLIT_TO_ALPHA, m_amdShadowResult);
			grcDrawSingleQuadf(0.f, 0.f, 1.f, 1.f, 0.0f, 0.0f , 0.0f , 1.0f , 1.0f , Color32(0xffffffff));
			quad.EndCustomList();

			D3DPERF_EndEvent();
		}

		D3DPERF_EndEvent();
	}
}
#endif // USE_AMD_SHADOW_LIB

void CCSMGlobals::ShadowReveal() const // this gets called immediately before deferred lighting
{
	PF_PUSH_MARKER("Cascaded Shadow Reveal");
	g_ShadowRevealTime_Total.Start();
	//grcGpuTimer gpuTimer;
	//gpuTimer.Start();

#if RSG_PC
	CShaderLib::SetStereoTexture();
#endif

#if MULTIPLE_RENDER_THREADS
	s32 prevTechId = grmShaderFx::GetForcedTechniqueGroupId();
	grmShaderFx::SetForcedTechniqueGroupId(-1);
#endif // MULTIPLE_RENDER_THREADS

	const ShadowRevealSetup &revealSetup = m_shadowReveal[MM_DEFAULT];

	BANK_ONLY(if (m_debug.m_shadowRevealEnabled))
	{
		const CCSMState& state = GetRenderState();
		grcRenderTarget* dst = GBuffer::GetTarget(GBUFFER_RT_2);
		const bool isClouds = gCascadeShadows.AreCloudShadowsEnabled() BANK_ONLY(&& !TiledScreenCapture::IsEnabled());

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
		if (m_softShadowEnabled)
		{
			dst = m_softShadowIntermediateRT;
		}
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING

		if (dst && revealSetup.shader)
		{
			Assert(state.m_sampleType != CSM_ST_INVALID);

			CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars();

			grcRenderTarget*	depthTarget = CRenderTargets::GetDepthBuffer();
			const grcTexture*	depthTexture = depthTarget;
#if MSAA_EDGE_USE_DEPTH_COPY
			CRenderTargets::CopyDepthBuffer(depthTarget, DeferredLighting::m_edgeMarkDepthCopy);
			depthTexture = DeferredLighting::m_edgeMarkDepthCopy;
#elif RSG_PC
			if (GRCDEVICE.IsReadOnlyDepthAllowed())
			{
				depthTarget = CRenderTargets::GetDepthBuffer_ReadOnlyDepth();
			}else
			{
				depthTexture = CRenderTargets::GetDepthBufferCopy(true);
			}	
#endif //RSG_PC

#if RMPTFX_USE_PARTICLE_SHADOWS
			// Default to particle shadows off.
			Vector4 particleShadowParams = Vector4(0.0f, 0.0f, 0.0f, 0.0f);

			if (m_enableParticleShadows)
			{
				// Read the particle texture and combine with regular shadow contribution.
				particleShadowParams = Vector4(1.0f, 1.0f, 0.0f, 0.0f);
			}
#if CASCADE_SHADOWS_DO_SOFT_FILTERING
			if (m_softShadowEnabled)
			{
				// Don`t combine the particle shadows with the regular shadows.
				particleShadowParams.y = 0.0f;
			}
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING

			const grcTexture *const particleTexture = (state.m_renderedAlphaEntityIntoParticleShadowMap || g_ptFxManager.HasAnyParticleShadowsRendered()) ? m_particleShadowMap:grcTexture::NoneBlackTransparent;

			revealSetup.SetVars(*this, depthTexture, particleTexture, particleShadowParams);

#endif // RMPTFX_USE_PARTICLE_SHADOWS

#if CASCADE_SHADOWS_STENCIL_REVEAL
			if (BANK_SWITCH(m_debug.m_shadowStencilRevealEnabled, false))
			{
#if __XENON
				// Release EDRAM protection of Gbuffer2's second tile 
				if (CRenderer::UseDelayedGbuffer2TileResolve())
				{
					RELEASE_EDRAM_TARGET(GBuffer::GetTiledTarget(GBUFFER_RT_2));
				}
#endif // __XENON

				grcTextureFactory::GetInstance().LockRenderTarget(0, dst, CRenderTargets::GetDepthBuffer());
#if __XENON
				GBuffer::CopyBuffer(GBUFFER_RT_2, CRenderer::UseDelayedGbuffer2TileResolve());
#endif // __XENON

				for (int cascadeIndex = 0; cascadeIndex < BANK_SWITCH(m_debug.m_shadowStencilRevealCount, CASCADE_SHADOWS_COUNT); cascadeIndex++)
				{
					if (cascadeIndex <= 3) // pass 0: stencil reveal
					{
						PF_PUSH_MARKER(atVarString("Shadow Reveal (%d)", cascadeIndex).c_str());

						if (AssertVerify(revealSetup.shader->BeginDraw(grmShader::RMC_DRAW, false, revealSetup.stencilRevealTechnique)))
						{
							const Mat44V shadowToWorld = Mat44V(state.m_shadowToWorld33, Vec4V(V_ZERO));
							const Vec4V  cascadeSphere = TransformSphere(state.m_shadowToWorld33, state.m_cascadeBoundsSphere[cascadeIndex]);

							revealSetup.shader->SetVar(m_shadowRevealShaderStencilRevealShadowToWorld, shadowToWorld);
							revealSetup.shader->SetVar(m_shadowRevealShaderStencilRevealCascadeSphere, cascadeSphere);

							revealSetup.shader->Bind(cascadeIndex);
							{
								m_volumeVertexBuffer.Draw();
							}
							revealSetup.shader->UnBind();
							revealSetup.shader->EndDraw();
						}

						PF_POP_MARKER();
					}
#if __PS3
					if (BANK_SWITCH(m_debug.m_shadowStencilRevealUseHiStencil, false))
					{
						const int stencilMask = DEFERRED_MATERIAL_SPAREMASK;
						const int stencilRef  = DEFERRED_MATERIAL_SPAREMASK & ((cascadeIndex + 1) << 5);

						CRenderTargets::RefreshStencilCull(true, stencilRef, stencilMask);
					}
#endif // __PS3
					// pass 1: shadow render
					{
						PF_PUSH_MARKER(atVarString("Shadow Render (%d)", cascadeIndex).c_str());

						if (AssertVerify(revealSetup.shader->BeginDraw(grmShader::RMC_DRAW, false, revealSetup.stencilRevealTechnique)))
						{
							revealSetup.shader->Bind(3 + cascadeIndex);
							{
								const Color32 colour = Color32(255,255,255,255);

								const float x0 = -1.0f;
								const float y0 = +1.0f;
								const float x1 = +1.0f;
								const float y1 = -1.0f;
								const float z  =  0.0f;

								grcBegin(drawTriStrip, 4);
								grcVertex(x0, y0, z, 0.0f, 0.0f, -1.0f, colour, 0.0f, 0.0f);
								grcVertex(x0, y1, z, 0.0f, 0.0f, -1.0f, colour, 0.0f, 1.0f);
								grcVertex(x1, y0, z, 0.0f, 0.0f, -1.0f, colour, 1.0f, 0.0f);
								grcVertex(x1, y1, z, 0.0f, 0.0f, -1.0f, colour, 1.0f, 1.0f);
								grcEnd();
							}
							revealSetup.shader->UnBind();
							revealSetup.shader->EndDraw();
						}

						PF_POP_MARKER();
					}
				}

				grcTextureFactory::GetInstance().UnlockRenderTarget(0);
			}
			else
#endif // CASCADE_SHADOWS_STENCIL_REVEAL
			{

				g_ShadowRevealTime_SmoothStep.Start();
				if (isClouds)
				{
					const float density  = g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_CLOUD_SHADOW_DENSITY)*2.0f - 0.5f;
					const float softness = g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_CLOUD_SHADOW_SOFTNESS)/2.0f;
					const float opacity  = g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_CLOUD_SHADOW_OPACITY);

					GenerateSmoothStepRT(revealSetup, density - softness, density + softness, 1.0f, 1.0f - opacity);

				}
				g_ShadowRevealTime_SmoothStep.Stop();
				revealSetup.SetShadowParams();
				SetCloudShaderVars();

#if CSM_PORTALED_SHADOWS
				if (GetRenderWindowRenderer().UseExtrusion())
				{
					grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, CRenderTargets::GetDepthBuffer());
					grcViewport vp = *gVpMan.GetRenderGameGrcViewport();
					const grcViewport* oldvp = grcViewport::SetCurrent(&vp);
					Mat33V worldToShadow33;
					Transpose(worldToShadow33, state.m_shadowToWorld33);
#if USE_INVERTED_PROJECTION_ONLY
					vp.SetInvertZInProjectionMatrix(true);
#endif // USE_INVERTED_PROJECTION_ONLY
					GetRenderWindowRenderer().ExtrudeDraw(vp.GetCameraPosition(), worldToShadow33, CalcThreshold(vp));

					grcViewport::SetCurrent(oldvp);

					grcTextureFactory::GetInstance().UnlockRenderTarget(0);
				}
#endif //CSM_PORTALED_SHADOWS

#if DEPTH_BOUNDS_SUPPORT && CASCADE_SHADOWS_DO_SOFT_FILTERING
				if (m_softShadowEnabled)
				{
					grcTextureFactory::GetInstance().LockRenderTarget(0, dst, NULL);
					GRCDEVICE.Clear(true, Color32(255, 255, 255, 255), false, 0.0f, false, 0);
					grcTextureFactory::GetInstance().UnlockRenderTarget(0);
				}
#endif //CASCADE_SHADOWS_DO_SOFT_FILTERING

#if CSM_PORTALED_SHADOWS
				if (GetRenderWindowRenderer().UseExtrusion())
				{
					grcTextureFactory::GetInstance().LockRenderTarget(0, dst, depthTarget);
					if(m_softShadowEnabled)
						grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
					else
						grcStateBlock::SetBlendState(m_shadowRevealShaderState_BS);

					GetRenderWindowRenderer().FillShadow();
					grcTextureFactory::GetInstance().UnlockRenderTarget(0);
				}
#endif // CSM_PORTALED_SHADOWS

				g_ShadowRevealTime_Cloud.Start();
#if DEPTH_BOUNDS_SUPPORT
				if (isClouds && state.m_lastCascadeEnd < state.m_farClip BANK_ONLY(&& m_debug.m_useDepthBounds))
				{
					PF_PUSH_TIMEBAR_DETAIL("Cloud Shadows");
					
					grcRenderTarget *cloudTarget = dst;
					float nearZ = state.m_lastCascadeEnd;
#if CASCADE_SHADOWS_DO_SOFT_FILTERING
					if (m_softShadowEnabled)
					{
						cloudTarget = GBuffer::GetTarget(GBUFFER_RT_2);
					}
#endif //CASCADE_SHADOWS_DO_SOFT_FILTERING

					grcTextureFactory::GetInstance().LockRenderTarget(0, cloudTarget, depthTarget);

					ShadowRevealToScreen(revealSetup, EM_IGNORE, revealSetup.techniqueCloudShadows, state.m_nearClip, state.m_farClip, nearZ, state.m_farClip);

					grcTextureFactory::GetInstance().UnlockRenderTarget(0);
					PF_POP_TIMEBAR_DETAIL();
				}
#endif //DEPTH_BOUNDS_SUPPORT
				g_ShadowRevealTime_Cloud.Stop();

				g_ShadowRevealTime_Edge.Start();
#if MSAA_EDGE_PASS
				if (DeferredLighting::IsEdgePassActive() BANK_ONLY(&& DebugDeferred::m_OptimizeShadowReveal)
#if USE_NV_SHADOW_LIB
					&& !m_enableNvidiaShadows
# endif
# if USE_AMD_SHADOW_LIB
					&& !m_enableAMDShadows
# endif
					)
				{
					grcTextureFactory::GetInstance().LockRenderTarget(0, dst, depthTarget);
					BANK_ONLY(if (DebugDeferred::m_EnableShadowRevealFace))
					{
						const ShadowRevealSetup &altSetup = m_shadowReveal[MM_TEXTURE_READS_ONLY];
						altSetup.SetVars(*this, depthTexture, particleTexture, particleShadowParams);
						altSetup.SetShadowParams();
						SetCloudShaderVars();
						ShadowRevealToScreenHelper(altSetup, EM_FACE0, state);
					}
					BANK_ONLY(if (DebugDeferred::m_EnableShadowRevealEdge))
					{
						ShadowRevealToScreenHelper(revealSetup, EM_EDGE0, state);
					}
					grcTextureFactory::GetInstance().UnlockRenderTarget(0);
				}else
#endif //MSAA_EDGE_PASS
				{
					grcTextureFactory::GetInstance().LockRenderTarget(0, dst, depthTarget);
					ShadowRevealToScreenHelper(revealSetup, EM_IGNORE, state);
					grcTextureFactory::GetInstance().UnlockRenderTarget(0);
				}
				g_ShadowRevealTime_Edge.Stop();

#if __XENON
				BANK_ONLY(if (m_debug.m_shadowRevealWithStencil))
				{
					CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILENABLE, FALSE));
					CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILWRITEENABLE, FALSE));
				}
#endif // __XENON
			}

			revealSetup.shader->SetVar(revealSetup.depthBufferId , grcTexture::None);
			revealSetup.shader->SetVar(revealSetup.normalBufferId, grcTexture::None);
		}

#if CSM_PORTALED_SHADOWS
		if (GetRenderWindowRenderer().UseExtrusion())
		{
			GetRenderWindowRenderer().RestoreDepth();
		}
#endif // CSM_PORTALED_SHADOWS

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
		if (m_softShadowEnabled)
		{
			g_ShadowRevealTime_mask_total.Start();
			SoftShadowToScreen();
			g_ShadowRevealTime_mask_total.Stop();
		}
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING
	}

#if MULTIPLE_RENDER_THREADS
	grmShaderFx::SetForcedTechniqueGroupId(prevTechId);
#endif // MULTIPLE_RENDER_THREADS

	//float result = gpuTimer.Stop();
	//PF_INSERT_GPUBAR("Shadow Reveal", result);
	g_ShadowRevealTime_Total.Stop();
	PF_POP_MARKER();
}

// ================================================================================================
// ================================================================================================

#if __DEV && __PS3

static void CSMSetModelCullerDebugFlags_render(u8 flags)
{
	grmModel::SetModelCullerDebugFlags(flags);
}

#endif // __DEV && __PS3

#if __PS3 && ENABLE_EDGE_CULL_DEBUGGING

static void CSMSetEdgeCullDebugFlags_render(u32 flags)
{
#if SPU_GCM_FIFO
	SPU_COMMAND(grcGeometryJobs__SetEdgeCullDebugFlags, flags);
	cmd->flags = (u16)flags;
#endif // SPU_GCM_FIFO
}

#endif // __PS3 && ENABLE_EDGE_CULL_DEBUGGING

#if CSM_EDGE_OCCLUDER_QUAD_EXCLUSION
static void CSMSetOccluders(void* quads, int numQuads)
{
	GEOMETRY_JOBS.SetOccluders(quads, numQuads);
}
#endif // CSM_EDGE_OCCLUDER_QUAD_EXCLUSION

static void CSMSetCurrentRS(grcRasterizerStateHandle rs_DEFAULT, grcRasterizerStateHandle rs_TWO_SIDED,	grcRasterizerStateHandle rs_CULL_FRONT)
{
	gCascadeShadows.m_shadowCastingShader_RS_current[g_RenderThreadIndex][CSM_RS_DEFAULT   ] = rs_DEFAULT   ;
	gCascadeShadows.m_shadowCastingShader_RS_current[g_RenderThreadIndex][CSM_RS_TWO_SIDED ] = rs_TWO_SIDED ;
	gCascadeShadows.m_shadowCastingShader_RS_current[g_RenderThreadIndex][CSM_RS_CULL_FRONT] = rs_CULL_FRONT;
}

static void CSMSetRenderStates(const CCSMState& state, int cascadeIndex, bool XENON_ONLY(bUseHiZ), bool inInterior, bool bDontCreateRS, bool writeDepth)
{
	(void)state;

	DLC(dlCmdGrcLightStateSetEnabled, (false));

#if __BANK
	if (gCascadeShadows.m_debug.m_wireframe)
		DLC_Add(grcStateBlock::SetWireframeOverride, true);
#endif // __BANK

	int cascadeIndexForDepthSlopeBias = cascadeIndex;
	if (CSMShadowTypeIsHighAltitude(gCascadeShadows.m_scriptShadowType)) // high-altitude shadows use the last cascade tweaks
		cascadeIndexForDepthSlopeBias = CASCADE_SHADOWS_COUNT - 1;

#if __BANK

	Vec4V depthBias			= Vec4V(V_ZERO);
	Vec4V slopeBias			= Vec4V(V_ZERO);
	Vec4V depthBiasClamp	= Vec4V(V_ZERO);
	Vec4V slopeBiasRPDB     = Vec4V(V_ZERO);

	if (gCascadeShadows.m_debug.m_shadowDepthBiasEnabled)		
		depthBias		= gCascadeShadows.m_debug.m_shadowDepthBiasTweaks*ScalarV(gCascadeShadows.m_debug.m_shadowDepthBias*CSM_DEPTH_BIAS_SCALE);

	if (gCascadeShadows.m_debug.m_shadowSlopeBiasEnabled)
	{
		slopeBias		= gCascadeShadows.m_debug.m_shadowSlopeBiasTweaks*ScalarV(gCascadeShadows.m_debug.m_shadowSlopeBias);
		slopeBiasRPDB   = gCascadeShadows.m_debug.m_shadowSlopeBiasTweaks*ScalarV(gCascadeShadows.m_debug.m_shadowSlopeBiasRPDB);
	}

	if (gCascadeShadows.m_debug.m_shadowDepthBiasClampEnabled)
		depthBiasClamp = gCascadeShadows.m_debug.m_shadowDepthBiasClampTweaks*ScalarV(gCascadeShadows.m_debug.m_shadowDepthBiasClamp);

#else // not __BANK

	Vec4V depthBias            = CSM_DEFAULT_DEPTH_BIAS_V;
	Vec4V slopeBias            = CSM_DEFAULT_SLOPE_BIAS_V;
	const Vec4V slopeBiasRPDB  = CSM_DEFAULT_SLOPE_BIAS_RPDB_V;
	const Vec4V depthBiasClamp = CSM_DEFAULT_DEPTH_BIAS_CLAMP_V;

#endif // not __BANK


	//scale depth bias based on cascade size
	Vec4V depthBiasScale = Vec4V(Max(state.m_cascadeBoundsExtentXY[0].GetX(), state.m_cascadeBoundsExtentXY[0].GetY()),
								 Max(state.m_cascadeBoundsExtentXY[1].GetX(), state.m_cascadeBoundsExtentXY[1].GetY()),
								 Max(state.m_cascadeBoundsExtentXY[2].GetX(), state.m_cascadeBoundsExtentXY[2].GetY()),
								 Max(state.m_cascadeBoundsExtentXY[3].GetX(), state.m_cascadeBoundsExtentXY[3].GetY()));

	depthBias = depthBias * depthBiasScale WIN32PC_ONLY(* gCascadeShadows.m_depthBiasPrecisionScale);

#if USE_NV_SHADOW_LIB
	if(!gCascadeShadows.m_enableNvidiaShadows)
#endif
		if( state.m_sampleType == CSM_ST_DITHER16_RPDB                ||
			state.m_sampleType == CSM_ST_POISSON16_RPDB_GNORM         ||
			state.m_sampleType == CSM_ST_CLOUDS_DITHER16_RPDB         ||
			state.m_sampleType ==  CSM_ST_CLOUDS_POISSON16_RPDB_GNORM
#if RSG_PC
			|| state.m_sampleType == CSM_ST_BOX3x3
			|| state.m_sampleType == CSM_ST_CLOUDS_BOX3x3
#endif
			)
		slopeBias = slopeBiasRPDB;

	float shadowDepthBias		= 0.0f;
	float shadowSlopeBias		= 0.0f;
	float shadowDepthBiasClamp	= 0.0f;

	switch (cascadeIndexForDepthSlopeBias)
	{
	case 0: shadowDepthBias = depthBias.GetXf(); shadowSlopeBias = slopeBias.GetXf(); shadowDepthBiasClamp = depthBiasClamp.GetXf(); break;
	case 1: shadowDepthBias = depthBias.GetYf(); shadowSlopeBias = slopeBias.GetYf(); shadowDepthBiasClamp = depthBiasClamp.GetYf(); break;
	case 2: shadowDepthBias = depthBias.GetZf(); shadowSlopeBias = slopeBias.GetZf(); shadowDepthBiasClamp = depthBiasClamp.GetZf(); break;
	case 3: shadowDepthBias = depthBias.GetWf(); shadowSlopeBias = slopeBias.GetWf(); shadowDepthBiasClamp = depthBiasClamp.GetWf(); break;
	}

#if RSG_PC
	float delta = fabsf(gCascadeShadows.m_CachedDepthBias[cascadeIndexForDepthSlopeBias]- shadowDepthBias);
	if(delta > g_RasterCacheThreshold)
	{
		g_RasterStateUpdateId++;
		gCascadeShadows.m_CachedDepthBias[cascadeIndexForDepthSlopeBias] = shadowDepthBias;
	}
#endif

	if (  inInterior && cascadeIndexForDepthSlopeBias == 1)
		shadowDepthBias *=.3f;  // shrink first cascade in interiors to stop leakage.

	// scripts override both regular and vehicle biases
	if (gCascadeShadows.m_scriptDepthBiasEnabled) { shadowDepthBias = gCascadeShadows.m_scriptDepthBias*CSM_DEPTH_BIAS_SCALE; }
	if (gCascadeShadows.m_scriptSlopeBiasEnabled) { shadowSlopeBias = gCascadeShadows.m_scriptSlopeBias; }

#if RSG_PC
	shadowSlopeBias = shadowSlopeBias * gCascadeShadows.m_slopeBiasPrecisionScale;
#endif

#if GS_INSTANCED_SHADOWS
	if(bDontCreateRS)
	{
		gCascadeShadows.m_shadowDepthBias = depthBias;
		gCascadeShadows.m_shadowDepthSlopeBias = slopeBias;

		if (inInterior && cascadeIndexForDepthSlopeBias == 1)
			gCascadeShadows.m_shadowDepthBias *= Vec4V(0.3f,0.3f,0.3f,0.3f); // shrink first cascade in interiors to stop leakage.
		}
#else // not GS_INSTANCED_SHADOWS
	(void)bDontCreateRS;
#endif // not GS_INSTANCED_SHADOWS

#if !__FINAL
	if(!(cascadeIndex >= 0 && cascadeIndex < CASCADE_SHADOWS_COUNT))
	{
		grcDebugf1("[CascadeShadows] Invalid cascade index: %d", cascadeIndex);
	}
#endif

	const grcRasterizerStateHandle rs_DEFAULT    = gCascadeShadows.m_shadowCastingShader_RS[CSM_RS_DEFAULT   ][cascadeIndex].Create(shadowDepthBias,	shadowSlopeBias,	shadowDepthBiasClamp,	g_RasterStateUpdateId);
	const grcRasterizerStateHandle rs_TWO_SIDED  = gCascadeShadows.m_shadowCastingShader_RS[CSM_RS_TWO_SIDED ][cascadeIndex].Create(shadowDepthBias,	shadowSlopeBias,	shadowDepthBiasClamp,	g_RasterStateUpdateId);
	const grcRasterizerStateHandle rs_CULL_FRONT = gCascadeShadows.m_shadowCastingShader_RS[CSM_RS_CULL_FRONT][cascadeIndex].Create(shadowDepthBias,	shadowSlopeBias,	shadowDepthBiasClamp,	g_RasterStateUpdateId);

	DLC_Add(CSMSetCurrentRS, rs_DEFAULT, rs_TWO_SIDED, rs_CULL_FRONT);

	if (writeDepth)
		DLC(dlCmdSetStates, (rs_DEFAULT, grcStateBlock::DSS_LessEqual, grcStateBlock::BS_Default_WriteMaskNone));
	else
		DLC(dlCmdSetStates, (rs_DEFAULT, grcStateBlock::DSS_TestOnly_LessEqual, grcStateBlock::BS_Default_WriteMaskNone));

	DLC_Add(grmModel::SetForceShader, gCascadeShadows.m_shadowCastingShader);

#if USE_NV_SHADOW_LIB
	// Leave bias factors constant for Nv shadow lib.  Changes in bias when getting into a vehicle completely screw up Nv shadows.
	// Constant values seem to work better.
	//if (gCascadeShadows.m_enableNvidiaShadows)
	//{
	//	gCascadeShadows.SetNvDepthBias(gCascadeShadows.m_nvBiasScale*shadowDepthBias, cascadeIndex);
	//}
#endif
}

static void CSMRestoreRenderStates(bool XENON_ONLY(bUseHiZ))
{
#if __PS3

#if ENABLE_EDGE_CULL_DEBUGGING

	if (!gCascadeShadows.m_debug.m_edgeCullDebugFlagsAllRenderphases && (gCascadeShadows.m_debug.m_edgeCullDebugFlags & EDGE_CULL_DEBUG_ENABLED) != 0)
	{
		DLC_Add(CSMSetEdgeCullDebugFlags_render, (u32)EDGE_CULL_DEBUG_DEFAULT);
	}

#endif // ENABLE_EDGE_CULL_DEBUGGING

	DLC(dlCmdEdgeShadowType, (EDGE_SHADOWTYPE_NONE));
	DLC_Add(grcEffect::SetEdgeViewportCullEnable, true);
	DLC(dlCmdSetCurrentViewportToNULL, ());

#if CSM_EDGE_OCCLUDER_QUAD_EXCLUSION
	if (DEV_SWITCH(gCascadeShadows.m_debug.m_edgeOccluderQuadExclusionEnabled, false))
	{
		DLC_Add(CSMSetOccluders, (void*)NULL, 0);
	}
#endif // CSM_EDGE_OCCLUDER_QUAD_EXCLUSION

#endif // __PS3

#if __BANK
	if (gCascadeShadows.m_debug.m_wireframe)
	{
		DLC_Add(grcStateBlock::SetWireframeOverride, false);
	}
#endif // __BANK

	DLC_Add(CSMSetCurrentRS, grcStateBlock::RS_Invalid, grcStateBlock::RS_Invalid, grcStateBlock::RS_Invalid);

	DLC_Add(grmModel::SetForceShader, (grmShader*)NULL);

#if __XENON
	if (bUseHiZ)
	{
		DLC_Add(dlCmdDisableHiZ, false);
	}
#endif // __XENON

	//XENON_ONLY(DLC(dlCmdSetShaderGPRAlloc, (0, 0)));
}

#if __PS3 && __DEV

static void CSMCatchStaleVertexArrays()
{
	// intentionally fill all elements with utter crap to make sure unused stuff is disabled!
	for (int i = 0; i < 16; i++)
	{
		cellGcmSetVertexDataArrayOffset(GCM_CONTEXT, i, CELL_GCM_LOCATION_LOCAL, 0x0F900000);
	}
}

#endif // __PS3 && __DEV

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Init_(unsigned initMode)
{
#if SHADOW_ENABLE_ASYNC_CLEAR
	if (initMode == INIT_CORE)
	{
		g_AsyncClearFence = GRCDEVICE.AllocFence();
	}
	else
#endif // SHADOW_ENABLE_ASYNC_CLEAR
	if (initMode == INIT_SESSION)
	{
		Script_InitSession();
	}
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::ResetRenderThreadInfo()
{
#if __ASSERT
	SetParticleShadowSetFlagThreadID(-1);
	SetParticleShadowAccessFlagThreadID(-1);
#endif
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::InitShaders()
{
	gCascadeShadows.Init(DeferredLighting::m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_DIRECTIONAL]);
}

#if RSG_PC
__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::ResetShaders()
{
	gCascadeShadows.DeleteShaders();
	gCascadeShadows.InitShaders(DeferredLighting::m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_DIRECTIONAL]);
}


__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::DeviceLost()
{
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::DeviceReset()
{
	if (gCascadeShadows.Initialized())
	{
#if VARIABLE_RES_SHADOWS
		gCascadeShadows.ReComputeShadowRes();
#endif // VARIABLE_RES_SHADOWS
		gCascadeShadows.CreateRenderTargets();
	}
}
#endif // RSG_PC

__COMMENT(static) int CRenderPhaseCascadeShadowsInterface::CascadeShadowResX()
{
#if VARIABLE_RES_SHADOWS
	return gCascadeShadows.CascadeShadowResX();
#else
	return CASCADE_SHADOWS_RES_X;
#endif
}

__COMMENT(static) int CRenderPhaseCascadeShadowsInterface::CascadeShadowResY()
{
#if VARIABLE_RES_SHADOWS
	return gCascadeShadows.CascadeShadowResY();
#else
	return CASCADE_SHADOWS_RES_Y;
#endif
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::CreateShadowMapMini()
{
	gCascadeShadows.CreateShadowMapMini();
}

__COMMENT(static) grcRenderTarget* CRenderPhaseCascadeShadowsInterface::GetShadowMap()
{
	return gCascadeShadows.GetShadowMap();
}

__COMMENT(static) grcRenderTarget* CRenderPhaseCascadeShadowsInterface::GetShadowMapVS()
{
	return gCascadeShadows.GetShadowMapVS();
}

#if RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW

__COMMENT(static) bool CRenderPhaseCascadeShadowsInterface::UseAlphaShadows()
{
	return gCascadeShadows.UseAlphaShadows();
}

__COMMENT(static) grcRenderTarget* CRenderPhaseCascadeShadowsInterface::GetAlphaShadowMap()
{
	return gCascadeShadows.GetAlphaShadowMap();
}

#if __ASSERT
__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetParticleShadowSetFlagThreadID(int renderThreadID)	
{ 
	g_ParticleShadowFlagSetThreadID = renderThreadID;
}
__COMMENT(static) int CRenderPhaseCascadeShadowsInterface::GetParticleShadowSetFlagThreadID()
{
	return g_ParticleShadowFlagSetThreadID;
}
__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetParticleShadowAccessFlagThreadID(int renderThreadID)
{
	g_ParticleShadowFlagAccessThreadID = renderThreadID;
}
__COMMENT(static) int CRenderPhaseCascadeShadowsInterface::GetParticleShadowAccessFlagThreadID()
{
	return g_ParticleShadowFlagAccessThreadID;
}
#endif
#endif // RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW

#if __D3D11 || RSG_ORBIS
__COMMENT(static) grcRenderTarget* CRenderPhaseCascadeShadowsInterface::GetShadowMapReadOnly()
{
	return gCascadeShadows.GetShadowMapReadOnly();
}
#endif // __D3D11

#if __PS3

__COMMENT(static) grcRenderTarget* CRenderPhaseCascadeShadowsInterface::GetShadowMapPatched()
{
	return gCascadeShadows.GetShadowMapPatched();
}

#endif // __PS3

__COMMENT(static) grcRenderTarget* CRenderPhaseCascadeShadowsInterface::GetShadowMapMini(ShadowMapMiniType mapType)
{
	return gCascadeShadows.GetShadowMapMini(mapType);
}

__COMMENT(static) const grcTexture* CRenderPhaseCascadeShadowsInterface::GetSmoothStepTexture()
{
	return gCascadeShadows.GetSmoothStepTexture();
}

__COMMENT(static) grcTexture* CRenderPhaseCascadeShadowsInterface::GetDitherTexture()
{
	return gCascadeShadows.m_ditherTexture;
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::CopyShadowsToMiniMap(grcTexture* noiseMap, float height)
{
	gCascadeShadows.CopyShadowsToMiniMap(noiseMap, height);
}

__COMMENT(static) int CRenderPhaseCascadeShadowsInterface::GetShadowTechniqueId()
{
	return gCascadeShadows.PS3_SWITCH(m_techniqueGroupId_edge, m_techniqueGroupId);
}

__COMMENT(static) int CRenderPhaseCascadeShadowsInterface::GetShadowTechniqueTextureId()
{
	return gCascadeShadows.PS3_SWITCH(m_techniqueTextureGroupId_edge, m_techniqueTextureGroupId);
}

#if !__PS3
__COMMENT(static) grmShader* CRenderPhaseCascadeShadowsInterface::GetShadowShader()
{
	return gCascadeShadows.m_shadowCastingShader;
}
#endif // !__PS3

#if __BANK

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::InitWidgets()
{
	bkBank& bank = BANKMGR.CreateBank("Cascade Shadows");

	gCascadeShadows.m_debug.InitWidgets(bank);
	gCascadeShadows.m_entityTracker.AddWidgets(bank);
	gCascadeShadows.m_scanCascadeShadowInfo.AddWidgets(bank);

#if CSM_PORTALED_SHADOWS
	gCascadeShadows.GetRenderWindowRenderer().AddWidgets( bank);
#endif // CSM_PORTALED_SHADOWS
}

#if CASCADE_SHADOWS_DO_SOFT_FILTERING
__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::InitWidgets_SoftShadows(bkBank& bk)
{
	gCascadeShadows.m_debug.InitWidgets_SoftShadows(bk);
}
#endif // CASCADE_SHADOWS_DO_SOFT_FILTERING

__COMMENT(static) bool CRenderPhaseCascadeShadowsInterface::IsEntitySelectRequired()
{
#if ENTITYSELECT_ENABLED_BUILD && CASCADE_SHADOWS_ENTITY_ID_TARGET
	if (gCascadeShadows.m_debug.m_showShadowMapEntityIDs)
	{
		return true;
	}
#endif // ENTITYSELECT_ENABLED_BUILD && CASCADE_SHADOWS_ENTITY_ID_TARGET

	return false;
}

#endif // __BANK

static Mat33V_Out CalcShadowToWorldBasis(Vec3V_In lightDirection)
{
#if __BANK
	const Vec3V shadowDirection = gCascadeShadows.m_debug.m_shadowDirectionEnabled ? NormalizeSafe(gCascadeShadows.m_debug.m_shadowDirection, Vec3V(V_Z_AXIS_WZERO)) : lightDirection;
#else // not __BANK
	const Vec3V shadowDirection = lightDirection;
#endif // not __BANK

	Vec3V z = Vec3V(V_Z_AXIS_WZERO); // default is CSM_ALIGN_WORLD_Z

#if __BANK
	switch (gCascadeShadows.m_debug.m_shadowAlignMode)
	{
	case CSM_ALIGN_WORLD_X  : z = Vec3V(V_X_AXIS_WZERO); break;
	case CSM_ALIGN_WORLD_Y  : z = Vec3V(V_Y_AXIS_WZERO); break;
	case CSM_ALIGN_WORLD_Z  : z = Vec3V(V_Z_AXIS_WZERO); break;
	case CSM_ALIGN_CAMERA_X : z = +gCascadeShadows.m_debug.m_cameraLock.GetViewToWorld().GetCol0(); break; // this will be a frame behind ..
	case CSM_ALIGN_CAMERA_Y : z = +gCascadeShadows.m_debug.m_cameraLock.GetViewToWorld().GetCol1(); break; // this will be a frame behind ..
	case CSM_ALIGN_CAMERA_Z : z = -gCascadeShadows.m_debug.m_cameraLock.GetViewToWorld().GetCol2(); break; // this will be a frame behind ..
	}

	if (Abs<float>(Dot(shadowDirection, z).Getf()) > 0.99f) { z = Vec3V(V_X_AXIS_WZERO); }
	if (Abs<float>(Dot(shadowDirection, z).Getf()) > 0.99f) { z = Vec3V(V_Y_AXIS_WZERO); }
#endif // __BANK

	Vec3V x = Cross(shadowDirection, z);
	Vec3V y = Cross(shadowDirection, x);

	x = Normalize(x);
	y = Normalize(y);

#if __BANK
	if (gCascadeShadows.m_debug.m_shadowRotation != 0.0f)
	{
		const Vec2V sc = SinCos(ScalarV(DtoR*gCascadeShadows.m_debug.m_shadowRotation));
		const ScalarV s = sc.GetX(); // sin(theta)
		const ScalarV c = sc.GetY(); // cos(theta)
		const Vec3V x2 = x*c - y*s;
		const Vec3V y2 = x*s + y*c;
		x = x2;
		y = y2;
	}
#endif // __BANK

	return Mat33V(x, y, shadowDirection);
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Update()
{
	PF_AUTO_PUSH_DETAIL("CRenderPhaseCascadeShadowsInterface::Update");
	TRACK_FUNCTION(Update, g_trackFunctionCounter, false);

#if __BANK
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_S, KEYBOARD_MODE_DEBUG_ALT))
	{
		CSMToggleDebugDraw();
	}
#endif // __BANK

	const CLightSource& light = Lights::GetUpdateDirectionalLight();
	Assert(light.GetType() == LIGHT_TYPE_DIRECTIONAL);

#if RSG_PC
	const Settings& settings = CSettingsManager::GetInstance().GetSettings();
	if (settings.m_graphics.m_ShadowQuality == (CSettings::eSettingsLevel) (0)) return;
#endif

	if (
#if RSG_PC
		(CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality == (CSettings::eSettingsLevel) (0)) ||
#endif
		light.GetIntensity() == 0.0f ||
		!light.IsCastShadows()
		)
	{
		gCascadeShadows.m_shadowIsActive = false;
		gCascadeShadows.GetUpdateState().m_shadowIsActive = false;
		return;
	}
	else
	{
		gCascadeShadows.m_shadowIsActive  = true;
		gCascadeShadows.GetUpdateState().m_shadowIsActive = true;
	}


	static Vec3V s_lightDirection = -Vec3V(V_Z_AXIS_WZERO);

	if (!BANK_SWITCH(gCascadeShadows.m_debug.m_shadowDirectionDoNotUpdate, false))
	{
		s_lightDirection = VECTOR3_TO_VEC3V(light.GetDirection());

		if (gCascadeShadows.m_shadowDirectionApplyClamp)
		{
			const float clampAmt = BANK_SWITCH(gCascadeShadows.m_debug.m_shadowDirectionClampValue, CSM_DEFAULT_SHADOW_DIRECTION_CLAMP);

			// may need change to ease in to blend better
			s_lightDirection.SetZ(Min(ScalarV(clampAmt), s_lightDirection.GetZ()));
			s_lightDirection = Normalize(s_lightDirection);
		}
	}

	// Setup Quality Mode
	eCSMQualityMode qm = (eCSMQualityMode)gCascadeShadows.m_scriptShadowQualityMode;

	if (qm == CSM_QM_IN_GAME)
	{
		qm = gCascadeShadows.m_entityTracker.GetAutoQualityMode();
	}

	if (qm != gCascadeShadows.m_currentShadowQualityMode)
	{
		gCascadeShadows.m_currentShadowQualityMode = qm;
		Script_SetShadowQualityMode(qm);
	}

	gCascadeShadows.m_shadowToWorld33 = CalcShadowToWorldBasis(s_lightDirection);

	float cascadeScale = 1.0f;
	float expWeight = BANK_SWITCH(gCascadeShadows.m_debug.m_cascadeSplitZExpWeight, CSM_DEFAULT_SPLIT_Z_EXP_WEIGHT);

	if (gCascadeShadows.m_scriptSplitZExpWeight > 0.0f)
	{
		expWeight = gCascadeShadows.m_scriptSplitZExpWeight;
	}

#if !__BANK || BANK_SWITCH_ASSERT
	// keeps the defaults if tracker is not active
	const float z0 = CSM_DEFAULT_SPLIT_Z_MIN;
	const float z1 = CSM_DEFAULT_SPLIT_Z_MAX;
#endif // !__BANK

	gCascadeShadows.m_entityTracker.GetFocalObjectCascadeSplits(camInterface::GetFov(), cascadeScale, expWeight, gCascadeShadows.m_scriptEntityTrackerScale);
#if __DEV
	//if (gCascadeShadows.m_entityTracker.IsUseOccludersForAircraftEnabled())
	//{
	//	// TODO -- i don't think we want to be setting this directly from code, right? (behaviour is different in release)
	//	gCascadeShadows.m_debug.m_renderOccluders_enabled = gCascadeShadows.m_entityTracker.UseOccluders();
	//}
#endif // __DEV

	// TODO -- could this scale be combined with TCVAR_DIRECTIONAL_SHADOW_DISTANCE_MULTIPLIER?
	cascadeScale *= gCascadeShadows.m_scriptCascadeBoundsScale;

	for (int cascadeIndex = 0; cascadeIndex <= CASCADE_SHADOWS_COUNT; cascadeIndex++)
	{
		const float z = BANK_SWITCH(gCascadeShadows.m_debug.m_cascadeSplitZ[cascadeIndex], z0 + (z1 - z0)*(float)cascadeIndex/(float)CASCADE_SHADOWS_COUNT);
		gCascadeShadows.m_cascadeSplitZ[cascadeIndex] = ScalarV(z*cascadeScale);
		BANK_ONLY(gCascadeShadows.m_debug.m_currentCascadeSplitZ[cascadeIndex] = gCascadeShadows.m_cascadeSplitZ[cascadeIndex].Getf());
	}

#if USE_NV_SHADOW_LIB
	// We tweak some Nv PCSS parameters depending on cascadeScale.  If the cascade sizes increase when you get into a car
	// then we increase filtering slightly to match.
	gCascadeShadows.m_shadowParams.PCSSPenumbraParams.fMinSizePercent[0] = cascadeScale * gCascadeShadows.m_baseMinSize;
	gCascadeShadows.m_mapDesc.LightDesc.fLightSize[0] = (cascadeScale-1) * 0.05f + gCascadeShadows.m_baseLightSize;

	// There is a wide range of cascadeScale values which can easily increase the [0] values so that [0] > [1].  Clamp.
	gCascadeShadows.m_shadowParams.PCSSPenumbraParams.fMinSizePercent[0] = Min(gCascadeShadows.m_shadowParams.PCSSPenumbraParams.fMinSizePercent[0], gCascadeShadows.m_shadowParams.PCSSPenumbraParams.fMinSizePercent[1]);
	gCascadeShadows.m_mapDesc.LightDesc.fLightSize[0] = Min(gCascadeShadows.m_mapDesc.LightDesc.fLightSize[0], gCascadeShadows.m_mapDesc.LightDesc.fLightSize[1]);
#endif

	gCascadeShadows.AdjustSplitZ(expWeight);

	// ================================================================
	// world height

#if CSM_DEFAULT_WORLD_HEIGHT_UPDATE
	const bool bWorldHeightUpdate = BANK_SWITCH(gCascadeShadows.m_debug.m_worldHeightUpdate, (CSM_DEFAULT_WORLD_HEIGHT_UPDATE != 0)) && gCascadeShadows.m_scriptWorldHeightUpdate;

	if (bWorldHeightUpdate)
	{
		const Vec3V camPos = gVpMan.GetCurrentViewport()->GetGrcViewport().GetCameraMtx().GetCol3();

		const float hx = camPos.GetXf();
		const float hy = camPos.GetYf();

		if (AssertVerify(FPIsFinite(hx) && FPIsFinite(hy)))
		{
			static int prev_i = 9999;
			static int prev_j = 9999;

			int cell_i = 0;
			int cell_j = 0;

			CGameWorldHeightMap::GetWorldHeightMapIndex(cell_i, cell_j, hx, hy);

			if (prev_i != cell_i ||
				prev_j != cell_j ||
				gCascadeShadows.m_worldHeightReset)
			{
				prev_i = cell_i;
				prev_j = cell_j;

				const float hr = gCascadeShadows.m_cascadeSplitZ[CASCADE_SHADOWS_COUNT].Getf() + (float)CGameWorldHeightMap::WORLD_HTMAP_CELL_SIZE;
				const float h0 = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(hx - hr, hy - hr, hx + hr, hy + hr);
				const float h1 = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(hx - hr, hy - hr, hx + hr, hy + hr);

				gCascadeShadows.m_worldHeight[0] = ScalarV(h0);
				gCascadeShadows.m_worldHeight[1] = ScalarV(h1);
#if __BANK
				gCascadeShadows.m_debug.m_worldHeightAdj[0] = h0;
				gCascadeShadows.m_debug.m_worldHeightAdj[1] = h1;
#endif // __BANK
				gCascadeShadows.m_worldHeightReset = false;
			}
		}
	}
	else
#endif // CSM_DEFAULT_WORLD_HEIGHT_UPDATE
	{
		gCascadeShadows.m_worldHeight[0] = ScalarV(BANK_SWITCH(gCascadeShadows.m_debug.m_worldHeightAdj[0], CSM_DEFAULT_WORLD_HEIGHT_MIN));
		gCascadeShadows.m_worldHeight[1] = ScalarV(BANK_SWITCH(gCascadeShadows.m_debug.m_worldHeightAdj[1], CSM_DEFAULT_WORLD_HEIGHT_MAX));
	}

	// ================================================================
	// receiver height

#if CSM_DEFAULT_RECVR_HEIGHT_UPDATE
	const bool bRecvrHeightUpdate = BANK_SWITCH(gCascadeShadows.m_debug.m_recvrHeightUpdate, (CSM_DEFAULT_RECVR_HEIGHT_UPDATE != 0)) && gCascadeShadows.m_scriptRecvrHeightUpdate;

	if (bRecvrHeightUpdate)
	{
		const Vec3V camPos = gVpMan.GetCurrentViewport()->GetGrcViewport().GetCameraMtx().GetCol3();

		const float hx = camPos.GetXf();
		const float hy = camPos.GetYf();

		static int prev_i = 9999;
		static int prev_j = 9999;

		int cell_i = 0;
		int cell_j = 0;

		CGameWorldHeightMap::GetWorldHeightMapIndex(cell_i, cell_j, hx, hy);

		if (prev_i != cell_i ||
			prev_j != cell_j ||
			gCascadeShadows.m_recvrHeightReset)
		{
			prev_i = cell_i;
			prev_j = cell_j;

			const float hr = gCascadeShadows.m_cascadeSplitZ[CASCADE_SHADOWS_COUNT].Getf() + 50.0f;//(float)CGameWorldHeightMap::WORLD_HTMAP_CELL_SIZE;
			const float h0 = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(hx - hr, hy - hr, hx + hr, hy + hr);
			const float h1 = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(hx - hr, hy - hr, hx + hr, hy + hr);

#if CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
			gDepthHistogram.m_heightRecvrMin = h0;
			gDepthHistogram.m_heightRecvrMax = h1;
#endif // CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK

			gCascadeShadows.m_recvrHeight[0] = ScalarV(h0);
			gCascadeShadows.m_recvrHeight[1] = ScalarV(h1);
#if __BANK
			gCascadeShadows.m_debug.m_recvrHeightAdj[0] = h0;
			gCascadeShadows.m_debug.m_recvrHeightAdj[1] = h1;
#endif // __BANK
			gCascadeShadows.m_recvrHeightReset = false;
		}

#if CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
		if (gCascadeShadows.m_debug.m_recvrHeightTrackDepthHistogram)
		{
			const float h0 = gDepthHistogram.m_heightTrackMin;

			gCascadeShadows.m_recvrHeight[0] = ScalarV(h0);
#if __BANK
			gCascadeShadows.m_debug.m_recvrHeightAdj[0] = h0;
#endif // __BANK
			gCascadeShadows.m_recvrHeightReset = true; // TODO -- this forces recvr height update every frame, but this is only for debugging ..
		}
#endif // CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
	}
	else
#endif // CSM_DEFAULT_RECVR_HEIGHT_UPDATE
	{
#if __BANK
		gCascadeShadows.m_recvrHeight[0] = ScalarV(gCascadeShadows.m_debug.m_recvrHeightAdj[0]);
		gCascadeShadows.m_recvrHeight[1] = ScalarV(gCascadeShadows.m_debug.m_recvrHeightAdj[1]);
#endif // __BANK
	}

	// ================================================================

	const float maxPossibleOceanHeight = 2.0f; // accounting for ocean waves

	if (camInterface::GetPos().z < maxPossibleOceanHeight && CVfxHelper::GetGameCamWaterDepth() > 0.0f)
	{
		// If the camera goes underwater, clamp the height to the seabed floor. This isn't really a
		// good solution since this doesn't cover the case of standing above the water looking down
		// into it, it would be better if the heightmap itself covered the underwater region.
#if __BANK
		if (gCascadeShadows.m_debug.m_showInfo)
		{
			grcDebugDraw::AddDebugOutput("cascade shadows underwater mode");
		}
#endif // __BANK

		const float underwaterHeight      = BANK_SWITCH(gCascadeShadows.m_debug.m_recvrHeightUnderwater     , CSM_DEFAULT_RECVR_HEIGHT_UNDERWATER      );
		const float underwaterHeightTrack = BANK_SWITCH(gCascadeShadows.m_debug.m_recvrHeightUnderwaterTrack, CSM_DEFAULT_RECVR_HEIGHT_UNDERWATER_TRACK);

		if (underwaterHeightTrack != 0.0f)
		{
			gCascadeShadows.m_worldHeight[0] = ScalarV(-CVfxHelper::GetGameCamWaterDepth() - underwaterHeightTrack);
			gCascadeShadows.m_worldHeight[1] = ScalarV(-CVfxHelper::GetGameCamWaterDepth() + underwaterHeightTrack);
			gCascadeShadows.m_recvrHeight[0] = gCascadeShadows.m_worldHeight[0];
			gCascadeShadows.m_recvrHeight[1] = gCascadeShadows.m_worldHeight[1];
#if __BANK
			gCascadeShadows.m_debug.m_recvrHeightAdj[0] = gCascadeShadows.m_worldHeight[0].Getf();
			gCascadeShadows.m_debug.m_recvrHeightAdj[1] = gCascadeShadows.m_worldHeight[1].Getf();;
#endif // __BANK
		}
		else // use fixed underwater height
		{
			gCascadeShadows.m_recvrHeight[0] = Min(ScalarV(underwaterHeight), gCascadeShadows.m_recvrHeight[0]);
#if __BANK
			gCascadeShadows.m_debug.m_recvrHeightAdj[0] = gCascadeShadows.m_recvrHeight[0].Getf();
#endif // __BANK
		}
	}
	else if (IsEqualAll(gCascadeShadows.m_recvrHeight[0], ScalarV(V_ZERO)) != 0)
	{
		// As a workaround for the heightmap not covering the negative height of the seabed, if the
		// current min height is exactly zero that means we've hit the minimum value in the heightmap,
		// so set it to something slightly less than zero.
#if __BANK
		if (gCascadeShadows.m_debug.m_showInfo)
		{
			grcDebugDraw::AddDebugOutput("cascade shadows distant water mode");
		}
#endif // __BANK

		const float abovewaterHeightHack = -10.0f;

		gCascadeShadows.m_recvrHeight[0] = ScalarV(abovewaterHeightHack);
#if __BANK
		gCascadeShadows.m_debug.m_recvrHeightAdj[0] = gCascadeShadows.m_recvrHeight[0].Getf();
#endif // __BANK
	}

	// ================================================================

#if CSM_DEPTH_HISTOGRAM
	gDepthHistogram.Update();
#endif // CSM_DEPTH_HISTOGRAM
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::LateShadowReveal()
{
#if __XENON
	BANK_ONLY(if (gCascadeShadows.m_debug.m_shadowRevealWithStencil))
	{
		ShadowReveal();
		XENON_ONLY(GRCDEVICE.SetShaderGPRAllocation(128 - CGPRCounts::ms_DefLightingPsRegs, CGPRCounts::ms_DefLightingPsRegs));
	}
#endif // __XENON
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::DisableFromVisibilityScanning()
{
	gCascadeShadows.m_shadowIsActive = false;
	gCascadeShadows.GetUpdateState().m_shadowIsActive = false;

}

#if GS_INSTANCED_SHADOWS
__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetInstShadowActive(bool bActive)
{
	gCascadeShadows.m_bInstShadowActive = bActive;
}
__COMMENT(static) bool CRenderPhaseCascadeShadowsInterface::IsInstShadowEnabled()
{
	return g_bInstancing;
}
#endif // GS_INSTANCED_SHADOWS

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::ShadowReveal()
{
	PF_AUTO_PUSH_DETAIL("CRenderPhaseCascadeShadowsInterface::ShadowReveal");
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	XENON_ONLY( GRCDEVICE.SetShaderGPRAllocation(128 - CGPRCounts::ms_CascadedRevealPsRegs, CGPRCounts::ms_CascadedRevealPsRegs));

#if __XENON && CASCADE_SHADOWS_STENCIL_REVEAL
	DEV_ONLY(if (gCascadeShadows.m_debug.m_shadowPostRenderCopyDepth))
	{
		GBuffer::CopyDepth();
	}
	DEV_ONLY(if (gCascadeShadows.m_debug.m_shadowPostRenderRestoreHiZ))
	{
		GBuffer::RestoreHiZ(); // is this necessary?
	}
#endif // __XENON && CASCADE_SHADOWS_STENCIL_REVEAL

#if CASCADE_SHADOWS_STENCIL_REVEAL && __BANK
	gCascadeShadows.ShadowStencilRevealDebug();
#endif // CASCADE_SHADOWS_STENCIL_REVEAL && __BANK
	gCascadeShadows.ShadowReveal();
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Synchronise()
{
	TRACK_FUNCTION(Synchronise, g_trackFunctionCounter, false);

	gCascadeShadows.Synchronise();
}

__COMMENT(static) bool CRenderPhaseCascadeShadowsInterface::IsShadowActive()
{
	return gCascadeShadows.m_shadowIsActive;
}

__COMMENT(static) bool CRenderPhaseCascadeShadowsInterface::ArePortalShadowsActive()
{
#if CSM_PORTALED_SHADOWS
	return gCascadeShadows.GetRenderWindowRenderer().UseExtrusion();
#else
	return false;
#endif
}


__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetDeferredLightingShaderVars()
{
	gCascadeShadows.SetDeferredLightingShaderVars();
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars()
{
	gCascadeShadows.SetSharedShaderVars(SHADOW_MATRIX_REFLECT_ONLY(false, Vec4V(V_ZERO)));
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetCloudShadowParams()
{
	gCascadeShadows.SetCloudShaderVars();
}

#if SHADOW_MATRIX_REFLECT
__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetSharedShaderVarsShadowMatrixOnly(Vec4V_In reflectionPlane)
{
	gCascadeShadows.SetSharedShaderVars(true, reflectionPlane);
}
#endif // SHADOW_MATRIX_REFLECT

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetSharedShadowMap(const grcTexture* tex)
{
	if (tex == NULL)
	{
		tex = grcTexture::PS3_SWITCH(NoneWhite, NoneDepth);
	}

	gCascadeShadows.SetSharedShadowMap(tex);
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::RestoreSharedShadowMap()
{
	gCascadeShadows.RestoreSharedShadowMap();
}

__COMMENT(static) fwScanCascadeShadowInfo& CRenderPhaseCascadeShadowsInterface::GetScanCascadeShadowInfo()
{
	return gCascadeShadows.m_scanCascadeShadowInfo;
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::GetCascadeViewports(atArray<grcViewport>& shadowViewports_InOut)
{
	CCSMState& state = gCascadeShadows.GetUpdateState();
	Assertf(((u32)shadowViewports_InOut.GetCapacity()) == gCascadeShadows.m_maxParticleShadowCascade, "Number of cascades allocated to input does not match the cascade count. Input Count = %d, Cascade Shadow Count = %d", shadowViewports_InOut.GetCapacity(), gCascadeShadows.m_maxParticleShadowCascade);
	shadowViewports_InOut.ResetCount();

	for (int cascadeIndex = 0; cascadeIndex < gCascadeShadows.m_maxParticleShadowCascade; cascadeIndex++)
	{
		shadowViewports_InOut.Append() = state.m_viewports[cascadeIndex];
	}
}

__COMMENT(static) const grcViewport& CRenderPhaseCascadeShadowsInterface::GetCascadeViewport(int cascadeIndex)
{
	Assert(cascadeIndex >= 0 && cascadeIndex < CASCADE_SHADOWS_COUNT);
	return gCascadeShadows.GetUpdateState().m_viewports[cascadeIndex];
}

__COMMENT(static) const grcViewport& CRenderPhaseCascadeShadowsInterface::GetViewport()
{
	return gCascadeShadows.m_viewport;
}

__COMMENT(static) bool CRenderPhaseCascadeShadowsInterface::IsUsingStandardCascades()
{
	return CSMShadowTypeIsCascade(gCascadeShadows.m_scriptShadowType);
}

__COMMENT(static) float CRenderPhaseCascadeShadowsInterface::GetCutoutAlphaRef()
{
	return BANK_SWITCH(gCascadeShadows.m_debug.m_cutoutAlphaRef, CSM_DEFAULT_CUTOUT_ALPHA_REF);
}

__COMMENT(static) bool CRenderPhaseCascadeShadowsInterface::GetCutoutAlphaToCoverage()
{
	return BANK_SWITCH(gCascadeShadows.m_debug.m_cutoutAlphaToCoverage, CSM_DEFAULT_CUTOUT_ALPHA_TO_COVERAGE);
}

__COMMENT(static) bool CRenderPhaseCascadeShadowsInterface::GetRenderFadingEnabled()
{
	return BANK_SWITCH(gCascadeShadows.m_debug.m_renderFadingEnabled, true);
}

__COMMENT(static) bool CRenderPhaseCascadeShadowsInterface::GetRenderHairEnabled()
{
	return BANK_SWITCH(gCascadeShadows.m_debug.m_renderHairEnabled, true);
}

#define DLC_(code) { class staticclass { public: static void func() { code }}; DLC_Add(staticclass::func); }

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetRasterizerState(eCSMRasterizerState state)
{
	Assert((int)state >= 0 && state < CSM_RS_COUNT);

	if (static_cast<CDrawListMgr*>(gDrawListMgr)->IsExecutingCascadeShadowDrawList())
	{
		const grcRasterizerStateHandle rs = gCascadeShadows.m_shadowCastingShader_RS_current[g_RenderThreadIndex][state];

		if (rs != grcStateBlock::RS_Invalid)
		{
			grcStateBlock::SetRasterizerState(rs);
		}
		else
		{
			// TODO -- local shadows? we probably need to set a different RS stateblock here to support depth/slope bias
		}
	}
	else
	{
		if (state==CSM_RS_TWO_SIDED)
			CParaboloidShadow::SetCullMode (CParaboloidShadow::eCullModeNone);
		else 
			CParaboloidShadow::SetCullMode (CParaboloidShadow::eCullModeBack);
	}
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::RestoreRasterizerState()
{

	if (static_cast<CDrawListMgr*>(gDrawListMgr)->IsExecutingCascadeShadowDrawList())
		SetRasterizerState(CSM_RS_DEFAULT);
	else
		CParaboloidShadow::SetCullMode (CParaboloidShadow::eCullModeBack);
}

/*__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::AddToDrawListVehicleBegin()
{
	DLC_
	(
		const grcRasterizerStateHandle rs = gCascadeShadows.m_shadowCastingShader_RS_current[2]; // vehicle

		if (rs != grcStateBlock::RS_Invalid)
		{
			LOCK_STATEBLOCK_ONLY(grcStateBlock::UnlockRasterizerState('SHAD'));
			grcStateBlock::SetRasterizerState(rs);
			LOCK_STATEBLOCK_ONLY(grcStateBlock::LockRasterizerState('SHAD'));
		}
		else
		{
			// TODO -- local shadows? we probably need to set a different RS stateblock here to support depth/slope bias
		}
	);
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::AddToDrawListVehicleEnd()
{
	DLC_
	(
		const grcRasterizerStateHandle rs = gCascadeShadows.m_shadowCastingShader_RS_current[0]; // grcRSV::CULL_BACK

		if (rs != grcStateBlock::RS_Invalid)
		{
			LOCK_STATEBLOCK_ONLY(grcStateBlock::UnlockRasterizerState('SHAD'));
			grcStateBlock::SetRasterizerState(rs);
			LOCK_STATEBLOCK_ONLY(grcStateBlock::LockRasterizerState('SHAD'));
		}
		else
		{
			// TODO -- local shadows? we probably need to set a different RS stateblock here to support depth/slope bias
		}
	);
}*/

#if __BANK

__COMMENT(static) void BeginFindingNearZShadows()
{
	gCascadeShadows.m_debug.BeginFindingNearZShadows();
}

__COMMENT(static) float EndFindingNearZShadows()
{
	return gCascadeShadows.m_debug.EndFindingNearZShadows();
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::DebugDraw_update()
{
	TRACK_FUNCTION(DebugDraw_update, g_trackFunctionCounter, true);

#if CSM_DEPTH_HISTOGRAM
	gDepthHistogram.DebugDraw();
#endif // CSM_DEPTH_HISTOGRAM
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::DebugDraw_render()
{
	TRACK_FUNCTION(DebugDraw_render, g_trackFunctionCounter, false);

#if CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
	gDepthHistogram.UpdateHeightTrack_render();
#endif // CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
	gCascadeShadows.m_debug.DebugDraw(gCascadeShadows);
	gCascadeShadows.m_entityTracker.DebugDraw(gCascadeShadows.m_scriptEntityTrackerScale);
}

__COMMENT(static) bool CRenderPhaseCascadeShadowsInterface::IsDebugDrawEnabled()
{
	return	gCascadeShadows.m_debug.m_drawShadowDebugShader ||
	        gCascadeShadows.m_entityTracker.IsDebugDrawEnabled() ||
	        gCascadeShadows.m_debug.m_showShadowMapCascadeIndex > 0;
}

#endif // __BANK

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::UpdateVisualDataSettings()
{
	if (g_visualSettings.GetIsLoaded())
	{
		BANK_ONLY(if (!gCascadeShadows.m_debug.m_cloudTextureOverrideSettings))
		{
		}
	}
}

#if __BANK
template <typename T> static void SnapTo(T& value, T snapTo, T tolerance)
{
	if (Abs<T>(value - snapTo) <= tolerance)
	{
		value = snapTo;
	}
}
#endif // __BANK

// the only reason this is a function is that i want to keep the timecycle settings and visual settings together
__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::UpdateTimecycleSettings()
{
	static dev_bool disableUpdate = false;
	if ( disableUpdate)
		return;
#if __ASSERT
	if ( !g_timeCycle.CanAccess())
		return;
#endif
	const tcKeyframeUncompressed& keyframe = g_timeCycle.GetCurrUpdateKeyframe();

	BANK_ONLY(if (!gCascadeShadows.m_debug.m_numCascadesOverrideSettings))
	{
		gCascadeShadows.m_numCascades = (int)(0.5f + keyframe.GetVar(TCVAR_DIRECTIONAL_SHADOW_NUM_CASCADES)); // round to nearest
	}

	BANK_ONLY(if (!gCascadeShadows.m_debug.m_distanceMultiplierOverrideSettings))
	{
		gCascadeShadows.m_distanceMultiplier = keyframe.GetVar(TCVAR_DIRECTIONAL_SHADOW_DISTANCE_MULTIPLIER);

#if ALLOW_SHADOW_DISTANCE_SETTING
		//Choose the maximum value between the settings and the timecycle. May want some more work to ensure the correct distance.
		gCascadeShadows.m_distanceMultiplier = Max<float>(gCascadeShadows.m_distanceMultiplier, gCascadeShadows.m_distanceMultiplierSetting);
#endif // ALLOW_SHADOW_DISTANCE_SETTING

#if __BANK
		SnapTo(gCascadeShadows.m_distanceMultiplier, 1.0f, 0.001f); // prevent jittery timecycle from affecting rag

		if (!gCascadeShadows.m_debug.m_distanceMultiplierEnabled)
		{
			gCascadeShadows.m_distanceMultiplier = 1.0f;
		}
#endif // __BANK
	}

	BANK_ONLY(if (!gCascadeShadows.m_debug.m_cloudTextureOverrideSettings))
	{
		gCascadeShadows.m_cloudTextureSettings.m_cloudShadowAmount   = keyframe.GetVar(TCVAR_CLOUD_SHADOW_OPACITY)*keyframe.GetVar(TCVAR_CLOUD_SHADOW_DENSITY);
		gCascadeShadows.m_cloudTextureSettings.m_fogShadowAmount     = keyframe.GetVar(TCVAR_FOG_SHADOW_AMOUNT);
		gCascadeShadows.m_cloudTextureSettings.m_fogShadowFalloff    = keyframe.GetVar(TCVAR_FOG_SHADOW_FALLOFF);
		gCascadeShadows.m_cloudTextureSettings.m_fogShadowBaseHeight = keyframe.GetVar(TCVAR_FOG_SHADOW_BASE_HEIGHT);
	}
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_InitSession()
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	if (gCascadeShadows.m_shadowMapBase) // only if we've inited the rest of the system
	{
		Script_SetShadowType(CSM_TYPE_CASCADES);

		for (int cascadeIndex = 0; cascadeIndex < CASCADE_SHADOWS_COUNT; cascadeIndex++)
		{
			Script_SetCascadeBounds(cascadeIndex, false, 0.0f, 0.0f, 0.0f, 0.0f);
		}

		Script_SetCascadeBoundsHFOV(0.0f); // really we only need to set one of these, but i'm setting both for clarity
		Script_SetCascadeBoundsVFOV(0.0f);
		Script_SetCascadeBoundsScale(1.0f);
		Script_SetEntityTrackerScale(1.0f);
		SetEntityTrackerActive(true);
		Script_SetWorldHeightUpdate(CGameLogic::IsRunningGTA5Map() ? (CSM_DEFAULT_WORLD_HEIGHT_UPDATE != 0) : false);
		Script_SetWorldHeightMinMax(CSM_DEFAULT_WORLD_HEIGHT_MIN, CSM_DEFAULT_WORLD_HEIGHT_MAX);
		Script_SetRecvrHeightUpdate(CGameLogic::IsRunningGTA5Map() ? (CSM_DEFAULT_RECVR_HEIGHT_UPDATE != 0) : false);
		Script_SetRecvrHeightMinMax(CSM_DEFAULT_RECVR_HEIGHT_MIN, CSM_DEFAULT_RECVR_HEIGHT_MAX);
		Script_SetDitherRadiusScale(1.0f);
		Script_SetDepthBias(false, CSM_DEFAULT_DEPTH_BIAS);
		Script_SetSlopeBias(false, CSM_DEFAULT_SLOPE_BIAS);
		Script_SetShadowSampleType(NULL);
		Script_SetShadowQualityMode(CSM_QM_IN_GAME);
		Script_SetAircraftMode(false);
		Script_SetSplitZExpWeight(-1.);
		Script_SetDynamicDepthMode(false);
	}
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetShadowType(eCSMShadowType type)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	gCascadeShadows.m_scriptShadowType = type;
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetCascadeBounds(int cascadeIndex, bool bEnabled, float x, float y, float z, float radiusScale, bool lerpToDisabled, float lerpTime)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	if (cascadeIndex >= 0 && cascadeIndex < CASCADE_SHADOWS_COUNT)
	{
		gCascadeShadows.m_scriptCascadeBoundsEnable                   [cascadeIndex] = bEnabled;
		gCascadeShadows.m_scriptCascadeBoundsLerpFromCustscene        [cascadeIndex] = lerpToDisabled;
		gCascadeShadows.m_scriptCascadeBoundsLerpFromCustsceneDuration[cascadeIndex] = lerpTime;
		gCascadeShadows.m_scriptCascadeBoundsSphere                   [cascadeIndex] = Vec4V(x, y, z, radiusScale);
	}
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_GetCascadeBounds(int cascadeIndex, bool &bEnabled, float &x, float &y, float &z, float &radiusScale, bool &lerpToDisabled, float &lerpTime)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	if (cascadeIndex >= 0 && cascadeIndex < CASCADE_SHADOWS_COUNT)
	{
		bEnabled       = gCascadeShadows.m_scriptCascadeBoundsEnable                   [cascadeIndex];
		lerpToDisabled = gCascadeShadows.m_scriptCascadeBoundsLerpFromCustscene        [cascadeIndex];
		lerpTime       = gCascadeShadows.m_scriptCascadeBoundsLerpFromCustsceneDuration[cascadeIndex];
		x              = gCascadeShadows.m_scriptCascadeBoundsSphere                   [cascadeIndex].GetXf();
		y              = gCascadeShadows.m_scriptCascadeBoundsSphere                   [cascadeIndex].GetYf();
		z              = gCascadeShadows.m_scriptCascadeBoundsSphere                   [cascadeIndex].GetZf();
		radiusScale    = gCascadeShadows.m_scriptCascadeBoundsSphere                   [cascadeIndex].GetWf();
	}
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetCascadeBoundsHFOV(float degrees)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });
	int iWidth		= VideoResManager::GetSceneWidth();
	int iHeight		= VideoResManager::GetSceneHeight();

	gCascadeShadows.m_scriptCascadeBoundsTanHFOV = ScalarV(tanf(DtoR*0.5f*degrees));
	gCascadeShadows.m_scriptCascadeBoundsTanVFOV = gCascadeShadows.m_scriptCascadeBoundsTanHFOV*ScalarV((float)iHeight/(float)iWidth);
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetCascadeBoundsVFOV(float degrees)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });
	int iWidth		= VideoResManager::GetSceneWidth();
	int iHeight		= VideoResManager::GetSceneHeight();

	gCascadeShadows.m_scriptCascadeBoundsTanVFOV = ScalarV(tanf(DtoR*0.5f*degrees));
	gCascadeShadows.m_scriptCascadeBoundsTanHFOV = gCascadeShadows.m_scriptCascadeBoundsTanVFOV*ScalarV((float)iWidth/(float)iHeight);
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetCascadeBoundsScale(float scale)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	gCascadeShadows.m_scriptCascadeBoundsScale = scale;
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetSplitZExpWeight(float weight)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	gCascadeShadows.m_scriptSplitZExpWeight = weight;
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetBoundPosition(float dist)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	gCascadeShadows.m_scriptBoundPosition = dist;
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetEntityTrackerScale(float scale)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	gCascadeShadows.m_scriptEntityTrackerScale = scale;
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetWorldHeightUpdate(bool bEnable)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	gCascadeShadows.m_scriptWorldHeightUpdate = bEnable;
#if __BANK
	gCascadeShadows.m_debug.m_worldHeightUpdate = bEnable;
#endif // __BANK

	if (bEnable)
	{
		gCascadeShadows.m_worldHeightReset = true; // force world height to update on the next frame
	}
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetWorldHeightMinMax(float h0, float h1)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

#if __ASSERT
	if (h0 != CSM_DEFAULT_WORLD_HEIGHT_MIN ||
		h1 != CSM_DEFAULT_WORLD_HEIGHT_MAX)
	{
		Assertf(!gCascadeShadows.m_scriptWorldHeightUpdate, "Script_SetWorldHeightMinMax called while m_scriptWorldHeightUpdate=true");
	}
#endif // __ASSERT

	gCascadeShadows.m_worldHeight[0] = ScalarV(h0);
	gCascadeShadows.m_worldHeight[1] = ScalarV(h1);
#if __BANK
	gCascadeShadows.m_debug.m_worldHeightAdj[0] = h0;
	gCascadeShadows.m_debug.m_worldHeightAdj[1] = h1;
#endif // __BANK
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetRecvrHeightUpdate(bool bEnable)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	gCascadeShadows.m_scriptRecvrHeightUpdate = bEnable;
#if __BANK
	gCascadeShadows.m_debug.m_recvrHeightUpdate = bEnable;
#endif // __BANK

	if (bEnable)
	{
		gCascadeShadows.m_recvrHeightReset = true; // force receiver height to update on the next frame
	}
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetRecvrHeightMinMax(float h0, float h1)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

#if __ASSERT
	if (h0 != CSM_DEFAULT_RECVR_HEIGHT_MIN ||
		h1 != CSM_DEFAULT_RECVR_HEIGHT_MAX)
	{
		Assertf(!gCascadeShadows.m_scriptRecvrHeightUpdate, "Script_SetRecvrHeightMinMax called while m_scriptRecvrHeightUpdate=true");
	}
#endif // __ASSERT

	gCascadeShadows.m_recvrHeight[0] = ScalarV(h0);
	gCascadeShadows.m_recvrHeight[1] = ScalarV(h1);
#if __BANK
	gCascadeShadows.m_debug.m_recvrHeightAdj[0] = h0;
	gCascadeShadows.m_debug.m_recvrHeightAdj[1] = h1;
#endif // __BANK
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetDitherRadiusScale(float scale)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	gCascadeShadows.m_scriptDitherRadiusScale = scale;
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetDepthBias(bool bEnable, float depthBias)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	gCascadeShadows.m_scriptDepthBiasEnabled = bEnable;
	gCascadeShadows.m_scriptDepthBias        = depthBias;
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetSlopeBias(bool bEnable, float slopeBias)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	gCascadeShadows.m_scriptSlopeBiasEnabled = bEnable;
	gCascadeShadows.m_scriptSlopeBias        = slopeBias;
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetShadowSampleType(const char* typeStr)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	// temporary hack: don't change sample type if we're using cloud shadows
	if (gCascadeShadows.m_scriptShadowSampleType >= CSM_ST_CLOUDS_FIRST &&
		gCascadeShadows.m_scriptShadowSampleType <= CSM_ST_CLOUDS_LAST)
	{
		return;
	}

	if (typeStr)
	{
		int type = -1;
		if (0) {}
		#define ARG1_DEF_CSMSampleType(arg0, t) else if (stricmp(typeStr, #t) == 0) { type = t; }
		FOREACH_ARG1_DEF_CSMSampleType(arg0, ARG1_DEF_CSMSampleType)
		#undef  ARG1_DEF_CSMSampleType

		if (type != -1)
		{
			gCascadeShadows.m_scriptShadowSampleType = type;
		}
		else
		{
			Assertf(0, "Script_SetShadowSampleType: unknown type \"%s\"", typeStr);
			gCascadeShadows.m_scriptShadowSampleType = gCascadeShadows.GetDefaultFilter();
		}
	}
	else
	{
		gCascadeShadows.m_scriptShadowSampleType = gCascadeShadows.GetDefaultFilter();
	}
}
__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Cutscene_SetShadowSampleType( u32 hashVal)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	if ( hashVal == ATSTRINGHASH("CSM_ST_BOX4x4", 0xDB0484F0) ){
			gCascadeShadows.m_scriptShadowSampleType   =  CSM_ST_BOX4x4;
	}
	else if ( hashVal == ATSTRINGHASH("CSM_ST_TWOTAP", 0xD9D9C663) ){
			gCascadeShadows.m_scriptShadowSampleType   = CSM_ST_TWOTAP;
	}
	else if ( hashVal == ATSTRINGHASH("CSM_ST_BOX3x3", 0xFB27A534) ){
			gCascadeShadows.m_scriptShadowSampleType   = CSM_ST_BOX3x3;
	}
	else{
		Errorf("Cutscene_SetShadowSampleType : Type not supported");		
	}
	return;
}
bool CRenderPhaseCascadeShadowsInterface::IsSampleFilterSyncable()
{
	return gCascadeShadows.m_scriptShadowSampleType == CSM_ST_DITHER2_LINEAR;
}
__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetShadowQualityMode(eCSMQualityMode qm)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	// temporary hack: don't change sample type if we're using cloud shadows
	if (gCascadeShadows.m_scriptShadowSampleType >= CSM_ST_CLOUDS_FIRST &&
		gCascadeShadows.m_scriptShadowSampleType <= CSM_ST_CLOUDS_LAST)
	{
		return;
	}

	switch ((int)qm)
	{
	case CSM_QM_IN_GAME:
		gCascadeShadows.m_scriptShadowSampleType   = gCascadeShadows.GetDefaultFilter();
		gCascadeShadows.m_scriptCascadeBoundsScale = 1.0f;
		break;
	case CSM_QM_CUTSCENE_DEFAULT:
		gCascadeShadows.m_scriptShadowSampleType   = gCascadeShadows.GetDefaultFilter();
		gCascadeShadows.m_scriptCascadeBoundsScale = 0.5f;
		break;
	case CSM_QM_MOONLIGHT:
		gCascadeShadows.m_scriptShadowSampleType   = gCascadeShadows.GetDefaultFilter();
		gCascadeShadows.m_scriptCascadeBoundsScale = 1.0f;
		break;
	}
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetAircraftMode(bool on)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	gCascadeShadows.m_entityTracker.SetUseAircraftShadows(on);
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetFlyCameraMode(bool on)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

	if (on)
	{
		// if this assert fires, we won't be able to restore the shadow type properly, let's see what's going on ..
		Assertf(gCascadeShadows.m_scriptShadowType == CSM_TYPE_CASCADES, "shadow type was %d, expected %d (CASCADES)", gCascadeShadows.m_scriptShadowType, CSM_TYPE_CASCADES);

		gCascadeShadows.m_scriptShadowType = CSM_TYPE_QUADRANT;
		gCascadeShadows.m_scriptQuadrantTrackFlyCamera = true;
	}
	else
	{
		gCascadeShadows.m_scriptShadowType = CSM_TYPE_CASCADES;
		gCascadeShadows.m_scriptQuadrantTrackFlyCamera = true;
	}
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_SetDynamicDepthMode(bool on)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

#if CSM_DYNAMIC_DEPTH_RANGE
	gCascadeShadows.m_entityTracker.SetDyamicDepthShadowMode(on);
#else
	(void)on;
#endif
}
__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::Script_EnableDynamicDepthModeInCutscenes(bool on)
{
	DEV_ONLY(if (!gCascadeShadows.m_debug.m_allowScriptControl) { return; });

#if CSM_DYNAMIC_DEPTH_RANGE
	gCascadeShadows.m_entityTracker.EnableDynamicDepthModeInCutscenes(on);
#else 
	(void)on;
#endif
}


__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetEntityTrackerActive(bool bActive)
{
	gCascadeShadows.m_entityTracker.SetActive(bActive);
}
__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetDynamicDepthValue(float value)
{
	gCascadeShadows.m_entityTracker.SetDynamicDepthValue(value);
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetSoftShadowProperties()
{
	gCascadeShadows.SetSoftShadowProperties();
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetParticleShadowsEnabled(bool bEnabled)
{
#if RMPTFX_USE_PARTICLE_SHADOWS || SHADOW_ENABLE_ALPHASHADOW
	if (!PARAM_noparticleshadows.Get() && GRCDEVICE.GetDxFeatureLevel() >= CSM_PARTICLE_SHADOWS_FEATURE_LEVEL)
	{
		gCascadeShadows.m_enableParticleShadows = bEnabled;
		gCascadeShadows.m_enableAlphaShadows = bEnabled;
	}
#else
	(void)bEnabled;
#endif
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetScreenSizeCheckEnabled(bool bEnabled)
{
	gCascadeShadows.m_enableScreenSizeCheck = bEnabled;
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetSyncFilterEnabled(bool bEnabled)
{
	gCascadeShadows.m_useSyncFilter = bEnabled;
	gCascadeShadows.m_scriptShadowSampleType = gCascadeShadows.GetDefaultFilter();
}

__COMMENT(static) u32 CRenderPhaseCascadeShadowsInterface::GetClipPlaneCount()
{
	CCSMState &state = sysThreadType::IsRenderThread() ? gCascadeShadows.GetRenderState() : gCascadeShadows.GetUpdateState();
	return static_cast<u32>(state.m_shadowClipPlaneCount);
}

__COMMENT(static) Vec4V_Out CRenderPhaseCascadeShadowsInterface::GetClipPlane(u32 index)
{
	CCSMState &state = sysThreadType::IsRenderThread() ? gCascadeShadows.GetRenderState() : gCascadeShadows.GetUpdateState();
	return state.m_shadowClipPlanes[index];
}

__COMMENT(static) const Vec4V *CRenderPhaseCascadeShadowsInterface::GetClipPlaneArray()
{
	CCSMState &state = sysThreadType::IsRenderThread() ? gCascadeShadows.GetRenderState() : gCascadeShadows.GetUpdateState();
	return state.m_shadowClipPlanes.GetElements();
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetGrassEnabled(bool enabled)
{
	gCascadeShadows.m_scanCascadeShadowInfo.SetAwlaysCullGrass(!enabled);
}

#if ALLOW_SHADOW_DISTANCE_SETTING
__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetDistanceMultiplierSetting(float distanceMultiplier, float splitZStart, float splitZEnd, float aircraftExpWeight)
{
	gCascadeShadows.m_distanceMultiplierSetting = distanceMultiplier;
	gCascadeShadows.m_entityTracker.SetDistanceMultiplierSettings(splitZStart, splitZEnd, aircraftExpWeight);
}
#endif // ALLOW_SHADOW_DISTANCE_SETTING

#if ALLOW_LONG_SHADOWS_SETTING
__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::SetShadowDirectionClamp(bool bEnabled)
{
	gCascadeShadows.m_shadowDirectionApplyClamp = bEnabled;
}
#endif // ALLOW_LONG_SHADOW_SETTING

__COMMENT(static) float CRenderPhaseCascadeShadowsInterface::GetMinimumRecieverShadowHeight()
{
	return gCascadeShadows.m_recvrHeight[0].Getf();
}

#if GTA_REPLAY
__COMMENT(static) bool CRenderPhaseCascadeShadowsInterface::IsUsingAircraftShadows()
{
	return gCascadeShadows.m_entityTracker.InAircraftMode();
}
#endif

// ================================================================================================

__THREAD int CRenderPhaseCascadeShadows::s_CurrentExecutingCascade = -1;
int CRenderPhaseCascadeShadows::s_CurrentBuildingCascade = -1;

CRenderPhaseCascadeShadows::CRenderPhaseCascadeShadows(CViewport* pGameViewport)
	: CRenderPhaseScanned(pGameViewport, "Cascaded Shadows", DL_RENDERPHASE_CASCADE_SHADOWS, 0.0f
#if DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS
		, DL_RENDERPHASE_CASCADE_SHADOWS_FIRST, DL_RENDERPHASE_CASCADE_SHADOWS_LAST
#endif
	)
{
	Assert(!IsDisabled());

	SetRenderFlags(RENDER_SETTING_RENDER_CUTOUT_POLYS);
	SetVisibilityType(VIS_PHASE_CASCADE_SHADOWS);

	SetEntityListIndex(gRenderListGroup.RegisterRenderPhase(RPTYPE_SHADOW, this));

#if GS_INSTANCED_SHADOWS
#if __DEV
	g_bInstancing = PARAM_useInstancedShadows.Get();
#else
	// enable this for release?
	g_bInstancing = false;
#endif
#endif
}

__COMMENT(virtual) void CRenderPhaseCascadeShadows::UpdateViewport()
{
	TRACK_FUNCTION(UpdateViewport, g_trackFunctionCounter, false);

	if (!HasEntityListIndex() || (!gCascadeShadows.m_shadowIsActive))
	{
#if __BANK
		if (gCascadeShadows.m_debug.m_showInfo)
			CSMShowActiveInfo();
#endif // __BANK

		gCascadeShadows.GetUpdateWindowRenderer().Reset();

		return;
	}

	SetActive(true);

	m_Scanner.SetLodViewport(&GetViewport()->GetGrcViewport());

	gCascadeShadows.UpdateViewport(GetGrcViewport(), GetViewport()->GetGrcViewport());
}

__COMMENT(virtual) void CRenderPhaseCascadeShadows::BuildRenderList()
{
	TRACK_FUNCTION(BuildRenderList, g_trackFunctionCounter, false);

	if (!IsActive())
	{
#if __ASSERT
		extern bool g_HasSetupRasterizerForShadows;
		Assertf(!g_HasSetupRasterizerForShadows, "cascade shadows occlusion is active but shadows are not");
#endif // __ASSERT

		return;
	}

	PF_FUNC(CRenderPhaseCascadeShadows_BuildRenderList);

	const bool bEnableOcclusion = (g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_OCCLUSION) && (g_scanMutableDebugFlags & SCAN_MUTABLE_DEBUG_ENABLE_SHADOW_OCCLUSION);

	COcclusion::BeforeSetupRender(false, false, SHADOWTYPE_CASCADE_SHADOWS, bEnableOcclusion, GetGrcViewport());
	CRenderer::ConstructRenderListNew(GetRenderFlags(), this);
	COcclusion::AfterPreRender();
}

void ClearFirstCascadeToNearPlane()
{
	const float nearDepth = 0.0f;
#if CASCADE_SHADOW_TEXARRAY
	GRCDEVICE.Clear(false, Color32(0), true, nearDepth, 0);
#else
	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_ForceDepth, grcStateBlock::BS_Default);
	CSprite2d sprite;
	sprite.BeginCustomList(CSprite2d::CS_BLIT_COLOUR, NULL);
	grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, nearDepth, 0.0f, 0.0f, 1.0f, 1.0f, Color32());
	sprite.EndCustomList();
#endif //CASCADE_SHADOW_TEXARRAY
}

void ClearShadowMap(grcRenderTarget* shadowMap, float clearDepth)
{
#if SHADOW_ENABLE_ASYNC_CLEAR
	if(g_bUseAsyncClear)
		return;
#endif

	DLC(dlCmdLockRenderTarget, (0, NULL, shadowMap));

#if CASCADE_SHADOW_TEXARRAY
	for (int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
	{
		DLC(dlCmdSetArrayView, (i));
		DLC(dlCmdClearRenderTarget, (false, Color32(0,0,255,255), true, clearDepth, false, 0));
	}
#else
	DLC(dlCmdClearRenderTarget, (false, Color32(0,0,255,255), true, clearDepth, false, 0));
#endif

	DLC(dlCmdUnLockRenderTarget, (0));
}

#if SHADOW_ENABLE_ASYNC_CLEAR

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::ClearShadowMapsAsync()
{
	if(!g_bUseAsyncClear)
		return;

	float depth = 1.0f;

	union ClearDepth
	{
		float m_Float;
		unsigned int m_Unsigned;
	};

	ClearDepth clearDepth;
	clearDepth.m_Float = depth;

	uint32_t depthMin = uint32_t(depth * 0x3fff);
	uint32_t depthMax = ((depth * 0x3fff) != depthMin) ? depthMin + 1 : depthMin;
	uint32_t hTileFill = 0xf | (depthMin << 4) | (depthMax << 18);

	GRCDEVICE.GpuMarkFencePending(g_AsyncClearFence);

	static_cast<grcRenderTargetDurango*>(gCascadeShadows.m_shadowMapBase)->ClearAsync(hTileFill, clearDepth.m_Unsigned);

	GRCDEVICE.GpuMarkFenceDone(g_AsyncClearFence, grcDevice::GPU_WRITE_FENCE_AFTER_SHADER_BUFFER_WRITES);
}

__COMMENT(static) void CRenderPhaseCascadeShadowsInterface::WaitOnClearShadowMapsAsync()
{
	if(g_bUseAsyncClear)
	{
#if __BANK
		if(gCascadeShadows.m_debug.m_shadowAsyncClearWait)
#endif
		{
			GRCDEVICE.GpuWaitOnFence(g_AsyncClearFence, grcDevice::GPU_WAIT_ON_FENCE_INPUT_SHADER_FETCHES);
		}
	}
}

#endif // SHADOW_ENABLE_ASYNC_CLEAR

enum eCascadeShadowsDrawListType
{
	eCascadeShadows_FullType,
	eCascadeShadows_IndexType,
	eCascadeShadows_RevealType = eCascadeShadows_IndexType + CASCADE_SHADOWS_COUNT,
};

void CRenderPhaseCascadeShadows::UpdateRasterUpdateId()
{
	g_RasterStateUpdateId++;
}

void CRenderPhaseCascadeShadows::ShadowDrawListPrologue(u32 drawListType)
{
#if DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS
	if(drawListType > eCascadeShadows_FullType)
	{
		DrawListPrologue(DL_RENDERPHASE_CASCADE_SHADOWS_0 + drawListType - 1);
#if MULTIPLE_RENDER_THREADS
	CViewportManager::DLCRenderPhaseDrawInit();
#endif
	}
#else
	if(drawListType == eCascadeShadows_FullType)
	{
		DrawListPrologue();
#if MULTIPLE_RENDER_THREADS
		CViewportManager::DLCRenderPhaseDrawInit();
#endif
	}
#endif

#if SHADOW_ENABLE_ASYNC_CLEAR
	DLC_Add(CRenderPhaseCascadeShadowsInterface::WaitOnClearShadowMapsAsync);
#endif
}

void CRenderPhaseCascadeShadows::ShadowDrawListEpilogue(u32 drawListType)
{
#if DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS
	if(drawListType > eCascadeShadows_FullType)
		DrawListEpilogue(DL_RENDERPHASE_CASCADE_SHADOWS_0 + drawListType - 1);
#else
	if(drawListType == eCascadeShadows_FullType)
		DrawListEpilogue();
#endif
}

void StartGpuTimer(u32 cascadeIndex)
{
	g_ShadowRevealTime_cascades[cascadeIndex].Start();
}

void EndGpuTimer(u32 cascadeIndex)
{
	g_ShadowRevealTime_cascades[cascadeIndex].Stop();
}

__COMMENT(virtual) void CRenderPhaseCascadeShadows::BuildDrawList()
{
#if RSG_PC
	if (CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality == (CSettings::eSettingsLevel) (0))
		return;
#endif

	// we need to update the timecycle stuff now .. we couldn't do it in UpdateViewport because the timecycle isn't always valid there
	// even if cascade shadows are disabled, we need to update the fog shadow params
	CRenderPhaseCascadeShadowsInterface::UpdateTimecycleSettings();
	if(!gCascadeShadows.m_shadowIsActive)
		return;

#if DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS
	// Place in GPU commands to start timer.
	DrawListPrologue(DL_RENDERPHASE_CASCADE_SHADOWS_FIRST);
	DrawListEpilogue(DL_RENDERPHASE_CASCADE_SHADOWS_FIRST);
#else
#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES //This was only in the non split cascade variant, not sure if also needed for split cascades...
	CRenderer::SetTessellationVars( GetGrcViewport(), 1.0f );
#endif //RAGE_SUPPORT_TESSELLATION_TECHNIQUES
#endif //DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS

	CCSMState& state = gCascadeShadows.GetRenderState(); // we're past synchronise now ..
	float clearDepth = 1.0f;
#if __DEV
	if (gCascadeShadows.m_debug.m_clearEveryFrame)
		clearDepth = gCascadeShadows.m_debug.m_clearValue; // force clear value if we're clearing every frame
#endif // __DEV

	Assert(state.m_shadowMap);

	ShadowDrawListPrologue(eCascadeShadows_FullType);//single drawlist 

	TRACK_FUNCTION(BuildDrawList, g_trackFunctionCounter, true); // i think this has to come after BuildDrawListPrologue?

	PF_FUNC(CRenderPhaseCascadeShadows_BuildDrawList);
	PF_PUSH_TIMEBAR_DETAIL("Render Shadow Map Scene");
	PF_START_TIMEBAR_DETAIL("Render Entities");

	{

		Assert(state.m_sampleType == CSM_ST_INVALID);
		state.m_sampleType = (eCSMSampleType)gCascadeShadows.m_scriptShadowSampleType;

		if (CSMShadowTypeIsHighAltitude(gCascadeShadows.m_scriptShadowType))
		{
			if (gCascadeShadows.m_scriptShadowType == CSM_TYPE_HIGHRES_SINGLE)
			{
				// for now, this mode does not support cloud shadows or other sample types
				state.m_sampleType = CSM_ST_HIGHRES_BOX4x4;
			}
			else
			{
				if (state.m_sampleType < CSM_ST_STANDARD_COUNT)
				{
					state.m_sampleType = CSM_ST_BOX4x4;
				}
			}
		}

		{
#if RSG_PC
			if (!gCascadeShadows.m_softShadowEnabled)
			{
				int	softShadows = CSettingsManager::GetInstance().GetSettings().m_graphics.m_Shadow_SoftShadows;

				softShadows = softShadows < NUM_FILTER_SOFTNESS ? softShadows : 0;

				state.m_sampleType = (eCSMSampleType) filterSoftness[softShadows];
			}
#endif //RSG_PC

			if (gCascadeShadows.m_scriptShadowType != CSM_TYPE_HIGHRES_SINGLE)
			{
				if (gCascadeShadows.AreCloudShadowsEnabled()
#if USE_NV_SHADOW_LIB
					&& !gCascadeShadows.m_enableNvidiaShadows
#endif
#if USE_AMD_SHADOW_LIB
					&& !gCascadeShadows.m_enableAMDShadows
#endif
				){
					// cloud shadows are enabled automatically when the alpha is > 0 (and the sampling type is CSM_ST_DEFAULT)
					// i.e. they will not be enabled at night when the quality mode is CSM_QM_MOONLIGHT, or during cutscenes
					// when the quality mode is CSM_QM_CUTSCENE_DEFAULT
					switch (state.m_sampleType)
					{
					case CSM_ST_LINEAR:			      state.m_sampleType = CSM_ST_CLOUDS_LINEAR;               break;
					case CSM_ST_TWOTAP:			      state.m_sampleType = CSM_ST_CLOUDS_TWOTAP;               break;
					case CSM_ST_BOX3x3:			      state.m_sampleType = CSM_ST_CLOUDS_BOX3x3;               break;
					case CSM_ST_BOX4x4:			      state.m_sampleType = CSM_ST_CLOUDS_BOX4x4;               break;
					case CSM_ST_DITHER2_LINEAR:	      state.m_sampleType = CSM_ST_CLOUDS_DITHER2_LINEAR;       break;
					case CSM_ST_SOFT16:			      state.m_sampleType = CSM_ST_CLOUDS_SOFT16;               break;
					case CSM_ST_DITHER16_RPDB:     	  state.m_sampleType = CSM_ST_CLOUDS_DITHER16_RPDB;        break;
					case CSM_ST_POISSON16_RPDB_GNORM: state.m_sampleType = CSM_ST_CLOUDS_POISSON16_RPDB_GNORM; break;
					default: Assertf(0, "sample type %d not supported for cloud shadows", state.m_sampleType);
					}
				}

				state.m_cloudTextureSettings = gCascadeShadows.m_cloudTextureSettings;
			}
		}
	}

	grcRenderTarget* dst = NULL;

#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
	if (gCascadeShadows.m_debug.m_showShadowMapEntityIDs)
		dst = gCascadeShadows.m_shadowMapEntityIDs;
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD

	fwInteriorLocation intLoc = g_SceneToGBufferPhaseNew->GetPortalVisTracker()->GetInteriorLocation();

#if GS_INSTANCED_SHADOWS
	//static bool g_bSelectiveVPopt = true;
#if __DEV
	if (PARAM_useInstancedShadows.Get())
	{
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_H, KEYBOARD_MODE_DEBUG_ALT))
			g_bInstancing = !g_bInstancing;
	}
#endif

	int numCascade = state.m_shadowMapNumCascades;
	if (g_bInstancing)
	{
		// We use one drawlist for all cascades.
		ShadowDrawListPrologue(eCascadeShadows_IndexType);

#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
		if (gCascadeShadows.m_debug.m_showShadowMapEntityIDs)
			DLC_Add(CEntitySelect::BeginExternalPass);
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD

		ClearShadowMap(state.m_shadowMap, clearDepth);
		DLC(dlCmdLockRenderTarget, (0, dst, state.m_shadowMap));

		// first, pass down composite matrix of each cascade viewport to grcDevice
		for (int cascadeIndex = 0; cascadeIndex < state.m_shadowMapNumCascades; cascadeIndex++)
		{
			const Vec4V window = state.m_viewports[cascadeIndex].GetWindow();
			DLC_Add(grcViewport::SetCurrentInstVP, &(state.m_viewports[cascadeIndex]), window, cascadeIndex, (cascadeIndex == 0));
		}
		DLC_Add(grcViewport::SetInstVPWindow, CASCADE_SHADOWS_COUNT);

		numCascade = 1;
		DLC_Add(grcViewport::SetInstancing, true);
		fwRenderListBuilder::SetInstancingMode(true);

#if !__FINAL
		const char* cascadeStrings[] =
		{
			"Cascade Instanced",
		};
#endif // !__FINAL
		CSMSetRenderStates(state, 0, XENON_SWITCH(bUseHiZ, false), intLoc.IsValid(), true, true);

		DLCPushMarker(cascadeStrings[0]);

		CRenderListBuilder::AddToDrawListShadowEntitiesInstanced(
			GetEntityListIndex(),
			true, // bDirLightShadow
			gCascadeShadows.m_techniqueGroupId,
			-1,
			gCascadeShadows.m_techniqueTextureGroupId,
			-1
		);

		DLCPopMarker();

		CSMRestoreRenderStates(false);
		DLC_Add(grcViewport::SetInstancing, false);
		fwRenderListBuilder::SetInstancingMode(false);

		static const int CASCADE_MAX_GRASS = 3;
		for (int cascadeIndex = 0; cascadeIndex < CASCADE_MAX_GRASS; cascadeIndex++)
		{
			state.m_viewports[cascadeIndex].SetCurrentViewport();

			CRenderListBuilder::AddToDrawListShadowEntitiesGrass();
		}


#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
		if (gCascadeShadows.m_debug.m_showShadowMapEntityIDs)
			DLC_Add(CEntitySelect::EndExternalPass);
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD

		DLC(dlCmdUnLockRenderTarget, (0));
		DLC_Add(CShaderLib::ResetAllVar);

		ShadowDrawListEpilogue(eCascadeShadows_IndexType);
	
	}
	else
#endif // GS_INSTANCED_SHADOWS
	{
#if !CASCADE_SHADOW_TEXARRAY
		bool clearedRenderTarget = false;
#endif
		DLC_Add(StartGpuTimer, 4);
		for (int cascadeIndex = 0; cascadeIndex < state.m_shadowMapNumCascades; cascadeIndex++)
		{
			if (cascadeIndex >= gCascadeShadows.m_numCascades BANK_ONLY(|| !gCascadeShadows.m_debug.m_enableCascade[cascadeIndex]))
			{
				continue;
			}

			// Begin a draw-list for the current cascade.
			ShadowDrawListPrologue(eCascadeShadows_IndexType + cascadeIndex);
				
			DLCPushGPUTimebar(GT_DIRECTIONAL, "Cascade Shadows");

#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
		if (gCascadeShadows.m_debug.m_showShadowMapEntityIDs)
				DLC_Add(CEntitySelect::BeginExternalPass);
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD

			DLC(dlCmdLockRenderTarget, (0, dst, state.m_shadowMap));

#if CASCADE_SHADOW_TEXARRAY
			DLC(dlCmdSetArrayView, (cascadeIndex));
#endif

#if SHADOW_ENABLE_ASYNC_CLEAR
			if(!g_bUseAsyncClear)
#endif
			{
#if !CASCADE_SHADOW_TEXARRAY
				if(!clearedRenderTarget)
#endif
				{
#if !CASCADE_SHADOW_TEXARRAY
					clearedRenderTarget = true;
#endif
					DLC(dlCmdClearRenderTarget, (false, Color32(0,0,255,255), true, clearDepth, false, 0));
				}
			}					

#if !__FINAL
			static const char* const cascadeStrings[] =
			{
				"Cascade NonInst 0",
				"Cascade NonInst 1",
				"Cascade NonInst 2",
				"Cascade NonInst 3",
			};
#endif // !__FINAL

			bool drawShadows = true;
			DLCPushMarker(cascadeStrings[cascadeIndex]);
			DLC_Add(StartGpuTimer, cascadeIndex);
			state.m_viewports[cascadeIndex].SetCurrentViewport();
			CSMSetRenderStates(state, cascadeIndex, XENON_SWITCH(bUseHiZ, false), intLoc.IsValid(),false, true);

#if __DEV
			if (gCascadeShadows.m_debug.m_renderOccluders_enabled && cascadeIndex == CASCADE_SHADOWS_COUNT - 1)
			{
				DLCPushMarker("Draw Occluders");
				DLC_Add(COcclusion::PrimeZBuffer);
				DLCPopMarker();
				drawShadows = false;
			}
#endif // __DEV

#if CSM_PORTALED_SHADOWS
			if (cascadeIndex == 0 && gCascadeShadows.GetRenderWindowRenderer().UseExtrusion() && gCascadeShadows.m_entityTracker.FirstCascadeIsInInterior())
			{
				if (!gCascadeShadows.GetRenderWindowRenderer().PortalsOverlap(state.m_viewports[cascadeIndex].GetCompositeMtx()))
				{
					DLC_Add(ClearFirstCascadeToNearPlane);
					drawShadows = false;
				}
			}
#endif // CSM_PORTALED_SHADOWS

			if(drawShadows)
			{
#if RENDERLIST_DUMP_PASS_INFO
				if (cascadeIndex == 0)
					RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseCascadeShadow");
#endif // RENDERLIST_DUMP_PASS_INFO

				CRenderListBuilder::AddToDrawListShadowEntities(
					GetEntityListIndex(),
					true, // bDirLightShadow
					cascadeIndex,
					gCascadeShadows.m_techniqueGroupId,
					PS3_SWITCH(gCascadeShadows.m_techniqueGroupId_edge, -1),
					gCascadeShadows.m_techniqueTextureGroupId,
					PS3_SWITCH(gCascadeShadows.m_techniqueTextureGroupId_edge, -1)
				);

#if RENDERLIST_DUMP_PASS_INFO
				if (cascadeIndex == 0)
					RENDERLIST_DUMP_PASS_INFO_END();
#endif // RENDERLIST_DUMP_PASS_INFO
			}
			DLC_Add(EndGpuTimer, cascadeIndex);
			DLCPopMarker();
			CSMRestoreRenderStates(false);

			DLC(dlCmdUnLockRenderTarget, (0));

			DLC_Add(CShaderLib::ResetAllVar);
				
#if CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD
			if (gCascadeShadows.m_debug.m_showShadowMapEntityIDs)
				DLC_Add(CEntitySelect::EndExternalPass);
#endif // CASCADE_SHADOWS_ENTITY_ID_TARGET && ENTITYSELECT_ENABLED_BUILD

 			DLCPopGPUTimebar();

			// End the drawlist for the current cascade.
			ShadowDrawListEpilogue(eCascadeShadows_IndexType + cascadeIndex);
		}
		DLC_Add(EndGpuTimer, 4);
	}

	//--------------------------------------------------------------------------------------------------//
#if DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS
#define SHADOW_REVEAL_DL (DL_RENDERPHASE_CASCADE_SHADOWS_RVL - DL_RENDERPHASE_CASCADE_SHADOWS_0)
#else
#define SHADOW_REVEAL_DL (CASCADE_SHADOWS_COUNT + 1)
#endif //DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS

	// Particles and reveal share a drawlist.
	ShadowDrawListPrologue(eCascadeShadows_RevealType);

	DLCPushGPUTimebar(GT_DIRECTIONAL, "Cascade Shadows");

#if RSG_ORBIS
	DLC_Add(GRCDEVICE.DecompressDepthSurface, (static_cast<grcRenderTargetGNM*>(gCascadeShadows.m_shadowMapBase)->GetDepthTarget()), false);
#endif //RSG_ORBIS

#if RSG_PC
	if(CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShaderQuality >= CSettings::High)
#endif	
		BuildDrawListAlphaShadows();

	// Perform reveal processing.
#if GS_INSTANCED_SHADOWS
	DLC_Add(CRenderPhaseCascadeShadowsInterface::SetInstShadowActive, g_bInstancing);
#endif // GS_INSTANCED_SHADOWS

	DLC(dlCmdSetCurrentViewport, (gVpMan.GetUpdateGameGrcViewport()));
	DLC_Add(CRenderPhaseCascadeShadowsInterface::ShadowReveal);

	DLCPopGPUTimebar();

	ShadowDrawListEpilogue(eCascadeShadows_RevealType);

	PF_POP_TIMEBAR_DETAIL();
	//--------------------------------------------------------------------------------------------------//

	ShadowDrawListEpilogue(eCascadeShadows_FullType);

	// Place in GPU commands to stop timer.
#if DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS
	DrawListPrologue(DL_RENDERPHASE_CASCADE_SHADOWS_LAST);
	DrawListEpilogue(DL_RENDERPHASE_CASCADE_SHADOWS_LAST);
#endif //DRAW_LIST_MGR_SPLIT_CASCADE_SHADOWS
}

void CRenderPhaseCascadeShadows::BuildDrawListAlphaShadows()
{
	CCSMState& state = gCascadeShadows.GetRenderState(); // we're past synchronise now ..
	state.m_renderedAlphaEntityIntoParticleShadowMap = false;
	DLC_Add(CPtFxManager::ResetParticleShadowsRenderedFlag);

	if (!gCascadeShadows.m_enableAlphaShadows && !gCascadeShadows.m_enableParticleShadows)
		return;

	int numCableShadowCascades = 4;	// Make into Rag widget?
	if (CRenderPhaseCascadeShadowsInterface::IsUsingStandardCascades())
		numCableShadowCascades = 3;

	grcRenderTarget* pAlphaShadowTarget = CRenderPhaseCascadeShadowsInterface::GetAlphaShadowMap();
	grcRenderTarget* pAlphaShadowDepth = CRenderPhaseCascadeShadowsInterface::GetShadowMapReadOnly();
#if RSG_PC
	if (!(pAlphaShadowTarget || pAlphaShadowDepth))
		return;
#endif // RSG_PC

	grcAssertf(pAlphaShadowTarget  != NULL && pAlphaShadowDepth != NULL, "Alpha/particle shadow targets not set up" );

	DLC(dlCmdLockRenderTarget, (0, pAlphaShadowTarget, pAlphaShadowDepth));

#if CASCADE_SHADOW_TEXARRAY
	for (int i = 0; i < CASCADE_SHADOWS_COUNT; i++)
	{
		DLC(dlCmdSetArrayView, (i));
		DLC(dlCmdClearRenderTarget, (true, Color32(0,0,0,0), false, 0.0f, false, 0));
	}
#else // CASCADE_SHADOW_TEXARRAY
	DLC(dlCmdClearRenderTarget, (true, Color32(0,0,0,0), false, 0.0f, false, 0));
#endif // CASCADE_SHADOW_TEXARRAY

	const bool bIsNightTime = (g_timeCycle.GetSunFade() <= 0.0f);
	const bool bDisableAlphaShadows = bIsNightTime && BANK_SWITCH(gCascadeShadows.m_debug.m_DisableNightParticleShadows, CSM_DISABLE_ALPHA_SHADOW_NIGHT);
	bool bAlphaEntityShadowsRendered = false;
	if (gCascadeShadows.m_enableAlphaShadows && !bDisableAlphaShadows)
	{
		CViewportManager::DLCRenderPhaseDrawInit();

		int maxCascade = Min(numCableShadowCascades,(int)SUBPHASE_CASCADE_COUNT);
		maxCascade = Min(maxCascade, gCascadeShadows.m_numCascades);
		maxCascade = Min(maxCascade, state.m_shadowMapNumCascades);
		maxCascade = Clamp(maxCascade, 0, (int)SUBPHASE_CASCADE_COUNT);

		int entityCount = 0;

		for (u32 cascadeIndex=0; cascadeIndex < maxCascade; ++cascadeIndex)
		{
			BANK_ONLY(if (!gCascadeShadows.m_debug.m_enableCascade[cascadeIndex]) continue;);
#if !__FINAL
			const char* cascadeStrings[] =
			{
				"Cascade NonInst 0 (Alpha)",
				"Cascade NonInst 1 (Alpha)",
				"Cascade NonInst 2 (Alpha)",
				"Cascade NonInst 3 (Alpha)",
			};
			DLCPushMarker(cascadeStrings[cascadeIndex]);
#endif // !__FINAL

			CViewportManager::DLCRenderPhaseDrawInit();

#if CASCADE_SHADOW_TEXARRAY
			DLC(dlCmdSetArrayView, (cascadeIndex));
#endif
				state.m_viewports[cascadeIndex].SetCurrentViewport();

#if __D3D11_MONO_DRIVER_HACK
			Vec4V windowParams = state.m_viewports[cascadeIndex].GetWindow();
			// __D3D11_MONO_DRIVER_HACK This may only be required due to a driver bug.
			DLC(dlCmdGrcDeviceSetScissor, ((int)windowParams.GetXf(), (int)windowParams.GetYf(), (int)windowParams.GetZf(), (int)windowParams.GetWf()));
#endif // __D3D11_MONO_DRIVER_HACK

			fwInteriorLocation intLoc = g_SceneToGBufferPhaseNew->GetPortalVisTracker()->GetInteriorLocation();
			CSMSetRenderStates(state, cascadeIndex, XENON_SWITCH(bUseHiZ, false), intLoc.IsValid(),false, false);

			entityCount += CRenderListBuilder::AddToDrawListShadowEntitiesAlphaObjects(
				GetEntityListIndex(),
				true, // bDirLightShadow
				cascadeIndex,
				gCascadeShadows.m_techniqueTextureGroupId
			);
			DLC(dlCmdShaderFxPopForcedTechnique, ());
			DLCPopMarker();

			CSMRestoreRenderStates(false);
		}
		bAlphaEntityShadowsRendered = (entityCount > 0);
	}
	//Let the state know that we have rendered any alpha entity into the particle shadow map
	state.m_renderedAlphaEntityIntoParticleShadowMap = bAlphaEntityShadowsRendered;

#if RMPTFX_USE_PARTICLE_SHADOWS
	const bool bDisableParticleShadows = bIsNightTime && BANK_SWITCH(gCascadeShadows.m_debug.m_DisableNightParticleShadows, CSM_DISABLE_PARTICLE_SHADOW_NIGHT);
	if (gCascadeShadows.m_enableParticleShadows && !bDisableParticleShadows)
	{
		DLCPushMarker("ParticleShadows");

#if PTXDRAW_MULTIPLE_DRAWS_PER_BATCH
		if (BANK_SWITCH(gCascadeShadows.m_debug.m_particleShadowsUseMultipleDraw, true))
		{
			if (gCascadeShadows.m_maxParticleShadowCascade)
			{
				GRCCONTEXT_ALLOC_SCOPES_SUPPORTED_ONLY(DLC(dlCmdAllocScopePush,());)

				grcViewport viewports[CASCADE_SHADOWS_COUNT];
				Vec4V windows[CASCADE_SHADOWS_COUNT];

				for (int cascadeIndex = 0; cascadeIndex < gCascadeShadows.m_maxParticleShadowCascade; cascadeIndex++)
				{
					viewports[cascadeIndex] = state.m_viewports[cascadeIndex];
					windows[cascadeIndex] = state.m_viewports[cascadeIndex].GetWindow();
				}

				// We need to set a viewport here so the particle rendering has an initial one to work with.
				bool instanceParticleShadows = false;
#if PTFX_SHADOWS_INSTANCING
				DLC(dlCmdSetCurrentViewport, (gVpMan.GetUpdateGameGrcViewport()));
				instanceParticleShadows = true;
				for (int cascadeIndex = 0; cascadeIndex < state.m_shadowMapNumCascades; cascadeIndex++)
				{
					Vec4V window = Vec4V(0.0f, 0.0f, (float)CRenderPhaseCascadeShadowsInterface::CascadeShadowResX(), (float)CRenderPhaseCascadeShadowsInterface::CascadeShadowResY() * CASCADE_SHADOWS_COUNT);
					DLC_Add(grcViewport::SetCurrentInstVP, &(state.m_viewports[cascadeIndex]), window, cascadeIndex, (cascadeIndex == 0));
				}
				DLC_Add(grcViewport::SetInstVPWindow, CASCADE_SHADOWS_COUNT);
#else
				state.m_viewports[0].SetCurrentViewport();
#endif



#if PTFX_SHADOWS_INSTANCING
				CRenderListBuilder::AddToDrawListShadowParticlesAllCascades(gCascadeShadows.m_maxParticleShadowCascade, viewports, windows, instanceParticleShadows);
#else // GS_INSTANCED_SHADOWS
				CRenderListBuilder::AddToDrawListShadowParticlesAllCascades(gCascadeShadows.m_maxParticleShadowCascade, viewports, windows, false);
#endif // GS_INSTANCED_SHADOWS

				GRCCONTEXT_ALLOC_SCOPES_SUPPORTED_ONLY(DLC(dlCmdAllocScopePop,());)
			}
		}
		else
#endif // PTXDRAW_MULTIPLE_DRAWS_PER_BATCH
		{
			for (int cascadeIndex = 0; cascadeIndex < CASCADE_SHADOWS_COUNT; cascadeIndex++)
			{
				if (cascadeIndex < gCascadeShadows.m_maxParticleShadowCascade)
				{
					DLCPushMarker("Particle Cascade");
#if CASCADE_SHADOW_TEXARRAY
					DLC(dlCmdSetArrayView, (cascadeIndex));
#endif //CASCADE_SHADOW_TEXARRAY
					state.m_viewports[cascadeIndex].SetCurrentViewport();

					CRenderListBuilder::AddToDrawListShadowParticles(cascadeIndex);
					DLCPopMarker();
				}
			}

		}
		DLCPopMarker();
	}
#endif // RMPTFX_USE_PARTICLE_SHADOWS
	
	DLC(dlCmdUnLockRenderTarget, (0));
}

__COMMENT(virtual) u32 CRenderPhaseCascadeShadows::GetCullFlags() const
{
	u32 flags = CULL_SHAPE_FLAG_CASCADE_SHADOWS;

	BANK_ONLY(if ( gCascadeShadows.m_debug.m_cullFlags_RENDER_EXTERIOR )) { flags |= CULL_SHAPE_FLAG_RENDER_EXTERIOR                                          ; }
	BANK_ONLY(if ( gCascadeShadows.m_debug.m_cullFlags_RENDER_INTERIOR )) { flags |= CULL_SHAPE_FLAG_RENDER_INTERIOR                                          ; }
	BANK_ONLY(if ( gCascadeShadows.m_debug.m_cullFlags_TRAVERSE_PORTALS)) { flags |= CULL_SHAPE_FLAG_TRAVERSE_PORTALS | CULL_SHAPE_FLAG_CLIP_AND_STORE_PORTALS; }
	BANK_ONLY(if ( gCascadeShadows.m_debug.m_cullFlags_ENABLE_OCCLUSION)) { flags |= CULL_SHAPE_FLAG_ENABLE_OCCLUSION                                         ; }

	if (CSMShadowTypeIsHighAltitude(gCascadeShadows.m_scriptShadowType))
	{
		flags &= ~CULL_SHAPE_FLAG_ENABLE_OCCLUSION; // 4th cascade does not encompass the other three
	}

	return flags;
}



void CRenderPhaseCascadeShadows::ComputeFograyFadeRange(float &fadeStart, float &fadeEnd)
{
	float range[2];
	float kNear = 0.0f;
	float kFar = 1.0f;

	gCascadeShadows.CalcFogRayFadeRange(range);

	if(g_visualSettings.GetIsLoaded())
	{
		const tcKeyframeUncompressed& currKeyframe = (!sysThreadType::IsRenderThread() ? g_timeCycle.GetCurrUpdateKeyframe() : g_timeCycle.GetCurrRenderKeyframe());

		kNear = currKeyframe.GetVar(TCVAR_FOG_FOGRAY_NEARFADE);
		kFar = currKeyframe.GetVar(TCVAR_FOG_FOGRAY_FARFADE);
	}

	fadeStart = (range[1] - range[0])*kNear + range[0];
	fadeEnd = (range[1] - range[0])*kFar + range[0];
}

#if RSG_PC
bool CRenderPhaseCascadeShadows::IsNeedResolve()
{
	bool	needResolve = g_NeedResolve;
	g_NeedResolve = true;
	return needResolve;
}
#endif


/*
circleTest2D[tx_, z0_, z1_] := Block[{p, z, r2},
	p = 1 + tx^2;
	z = p*(z1 + z0)/2;
	r2 = z^2 - p*z0*z1;
	Circle[{0, z}, Sqrt[r2]]
];
circleTest3D[tx_, ty_, z0_, z1_] := Block[{p, z, r2},
	p = 1 + tx^2 + ty^2;
	z = p*(z1 + z0)/2;
	r2 = z^2 - p*z0*z1;
	Circle[{0, z}, Sqrt[r2]]
];
sphereTest3D[tx_, ty_, z0_, z1_] := Block[{p, z, r2},
	p = 1 + tx^2 + ty^2;
	z = p*(z1 + z0)/2;
	r2 = z^2 - p*z0*z1;
	Sphere[{0, 0, z}, Sqrt[r2]]
];

Column[{Slider[Dynamic[z0$], {0, 9}],
	Row[{Block[{tx = 1/2, ty = 1/2, z0 = Dynamic[z0$], z1 = 9},
		Graphics[{
			LightGray, Rectangle[{-12*tx, 0}, {12*ty, z1*2}], Black,
			Line[{{0, 0}, {-tx, 1}*10}],
			Line[{{0, 0}, {+tx, 1}*10}],
			Line[{{-tx, 1}*z0, {+tx, 1}*z0}],
			Line[{{-tx, 1}*z1, {+tx, 1}*z1}],
			circleTest2D[tx, z0, z1],
			circleTest3D[tx, ty, z0, z1],
			Lighter[Gray],
			Circle[{0, z1}, z1*Sqrt[tx^2]],
			Circle[{0, z1}, z1*Sqrt[tx^2 + ty^2]]
		}]
	]}]
}]

Column[{Slider[Dynamic[z0$], {0, 9}],
	Row[{Block[{tx = 1/2, ty = 1/2, z0 = Dynamic[z0$], z1 = 9}, 
		Graphics3D[{
			Opacity[1/2],
			Line[{{0, 0, 0}, {-tx, -ty, 1}*10}],
			Line[{{0, 0, 0}, {+tx, -ty, 1}*10}],
			Line[{{0, 0, 0}, {-tx, +ty, 1}*10}],
			Line[{{0, 0, 0}, {+tx, +ty, 1}*10}],
			Line[{{-tx, -ty, 1}, {+tx, -ty, 1}, {+tx, +ty, 1}, {-tx, +ty, 1}, {-tx, -ty, 1}}*z0],
			Line[{{-tx, -ty, 1}, {+tx, -ty, 1}, {+tx, +ty, 1}, {-tx, +ty, 1}, {-tx, -ty, 1}}*z1],
			sphereTest3D[tx, ty, z0, z1]
		}]
	]}]
}]

Column[{
	Slider[Dynamic[z0$], {0, 9}],
	Slider[Dynamic[z1$], {0, 9}], 
	Slider[Dynamic[z2$], {0, 9}],
	Row[{Block[{tx = 1/2, ty = 1/2, z0, z1, z2, z3 = 9},
		{z0, z1, z2} = Sort[{Dynamic[z0$], Dynamic[z1$], Dynamic[z2$]}];
		Graphics3D[{
			Opacity[1/2],
			Line[{{0, 0, 0}, {-tx, -ty, 1}*10}],
			Line[{{0, 0, 0}, {+tx, -ty, 1}*10}],
			Line[{{0, 0, 0}, {-tx, +ty, 1}*10}],
			Line[{{0, 0, 0}, {+tx, +ty, 1}*10}],
			Line[{{-tx, -ty, 1}, {+tx, -ty, 1}, {+tx, +ty, 1}, {-tx, +ty, 1}, {-tx, -ty, 1}}*z0],
			Line[{{-tx, -ty, 1}, {+tx, -ty, 1}, {+tx, +ty, 1}, {-tx, +ty, 1}, {-tx, -ty, 1}}*z1],
			Line[{{-tx, -ty, 1}, {+tx, -ty, 1}, {+tx, +ty, 1}, {-tx, +ty, 1}, {-tx, -ty, 1}}*z2],
			Line[{{-tx, -ty, 1}, {+tx, -ty, 1}, {+tx, +ty, 1}, {-tx, +ty, 1}, {-tx, -ty, 1}}*z3],
			sphereTest3D[tx, ty, z0, z1],
			sphereTest3D[tx, ty, z1, z2],
			sphereTest3D[tx, ty, z2, z3],
		}]
	]}]
}]
*/

