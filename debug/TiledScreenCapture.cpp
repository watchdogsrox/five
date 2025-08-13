// ============================
// debug/TiledScreenCapture.cpp
// (c) 2013 RockstarNorth
// ============================

#if __BANK

/*
===================================================================================================
TODO
- consider using this system to capture high-res perspective screenshots (tiled or interlaced)
- capturing overnight does not work (it crashes)
- water looks weird sometimes (it's supposed to be pure blue .. maybe i need to disable water fog?)
- flickering decals
	look at tile 25,25 (i think) on pc ..
	camera height = 800
- objects are missing (streaming/visibility issues?)
- force drawable/fragment LOD to highest always
- cascade shadows do not work (i'm not sure why ..)
- disabling postfx on pc causes tonemapping/exposure problems
- ssao still needs some work ..
	- changing camera height seems to affect it for some reason
	- MRSSAO filter kernel is extremely wide (esp in lower frequencies), causes seams between tiles even with large border
- to get nightime shots looking good, consider supporting lights in ortho ..
	- can we force all lights to be deferred (no "distant lights")?
	- support ortho in this function: "half4 JOIN(deferred_,LTYPE)(lightPixelInput IN..."
	- coronas don't switch on fast enough for multi-layer capture
- missing entities are annoying, this is still happening
	- can we write out a list of entities (archetype+matrix) for each tile, would this help?
	- we might be able to "prove" that the streaming requests indicator is wrong, if we crc the list of entities when it reaches zero and then wait a few seconds
	  and see if the list of entities changes .. not sure if this would be enough to convince ian to help though

ian says:
If you wanted to add something to request the IMAPs you need, you could either use a streaming volume or do something like this at the end of CGTABoxStreamerInterfaceNew::GetGameSpecificSearches() :

	if (tileCaptureThingActive)
	{
		searchList.Grow() = fwBoxStreamerSearch(aabb, fwBoxStreamerSearch::TYPE_DEBUG, fwBoxStreamerAsset::MASK_MAPDATA, false);
	}

===================================================================================================
*/

#include "bank/bank.h"
#if __PS3
#include "edge/geom/edgegeom_config.h"
#if ENABLE_EDGE_CULL_DEBUGGING
#include "edge/geom/edgegeom_structs.h"
#endif // ENABLE_EDGE_CULL_DEBUGGING
#endif // __PS3
#include "grcore/device.h"
#include "grcore/image.h"
#include "grcore/texture.h"
#if __XENON
#include "grcore/texturexenon.h"
#include "system/xtl.h"
#define DBG 0
#include <xgraphics.h>
#undef DBG
#endif // __XENON
#include "grcore/viewport.h"
#include "system/nelem.h"
#include "system/exec.h"
#include "vectormath/classes.h"

#include "file/default_paths.h"
#include "fwgeovis/geovis.h"
#include "fwrenderer/renderlistbuilder.h"
#include "fwscene/scan/Scan.h"
#include "fwscene/scan/ScanDebug.h"
#include "fwscene/stores/boxstreamer.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwsys/timer.h"
#include "fwutil/xmacro.h"
#include "streaming/streamingengine.h"

#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportManager.h"
#include "debug/Rendering/DebugDeferred.h"
#include "debug/TiledScreenCapture.h"
#include "game/Clock.h"
#include "Peds/Ped.h"
#include "Peds/pedpopulation.h"
#include "renderer/PostProcessFX.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "renderer/SSAO.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/Water.h"
#include "scene/debug/PostScanDebug.h"
#include "scene/Entity.h"
#include "scene/lod/LodMgr.h"
#include "scene/lod/LodDebug.h"
#include "scene/lod/LodScale.h"
#include "scene/world/GameWorld.h"
#include "scene/world/GameWorldHeightMap.h"
#include "shaders/CustomShaderEffectCable.h"
#include "streaming/streaming.h"
#include "system/controlMgr.h"
#include "timecycle/TimeCycle.h"
#include "Vehicles/vehiclepopulation.h"
#include "vfx/misc/Coronas.h"
#include "vfx/VisualEffects.h"

//OPTIMISATIONS_OFF()

namespace TiledScreenCapture
{
	enum
	{
		STATE_OFF,
		STATE_STARTING,
		STATE_ADVANCE_CAMERA, // advance camera one tile
		STATE_WAITING_FOR_STREAMING, // now wait for everything to stream in and/or all entities to be loaded
		STATE_WAITING_FOR_RENDERING, // once everything is loaded, wait a specified number of frames for framebuffer to be ready
		STATE_CAPTURE_START, // issue DLC command to renderthread to capture images, wait for renderthread to change state back to STATE_ADVANCE_CAMERA
		STATE_CAPTURING, // waiting for renderthread to change state
		STATE_ADVANCE_LAYER,
		STATE_COUNT
	};

	static const char* g_stateStrings[] =
	{
		"OFF",
		"STARTING",
		"ADVANCE_CAMERA",
		"WAITING_FOR_STREAMING",
		"WAITING_FOR_RENDERING",
		"CAPTURE_START",
		"CAPTURING",
		"ADVANCE_LAYER",
	};
	CompileTimeAssert(NELEM(g_stateStrings) == STATE_COUNT);

	class CTSCLayer
	{
	public:
		CTSCLayer()
		{
			sysMemSet(this, 0, sizeof(*this));

			m_enableProps   = true;
			m_enableTrees   = true;
			m_enableShadows = true;
			m_enableSSAO    = true;
			m_SSAOScale     = 1.0f;
			m_coronaScale   = 1.0f;
		}

		static void Update_cb();

		void AddWidgets(bkBank& bk, int index)
		{
			bk.PushGroup(atVarString("Layer %d", index).c_str(), false);
			{
				const char* GBufferNames[] = { "None", "Albedo", "Normals", "Specular/Shadow", "Ambient/Emissive", "Depth/Stencil", "Invert Shadow" };

				bk.AddToggle("Enabled"                , &m_enabled);
				bk.AddText  ("Name"                   , &m_name[0], sizeof(m_name), false);
				bk.AddToggle("Draw LODs"              , &m_drawLODs, Update_cb);
				bk.AddCombo ("GBuffer"                , &m_gbufferIndex, NELEM(GBufferNames), GBufferNames, Update_cb);
				bk.AddToggle("Enable Props"           , &m_enableProps, Update_cb);
				bk.AddToggle("Enable Trees"           , &m_enableTrees, Update_cb);
				bk.AddToggle("Enable Vfx"             , &m_enableVfx, Update_cb);
				bk.AddToggle("Enable Ambient Only"    , &m_enableAmbientOnly, Update_cb);
				bk.AddToggle("Enable Shadows"         , &m_enableShadows, Update_cb);
				bk.AddToggle("Enable Specular"        , &m_enableSpecular, Update_cb);
				bk.AddToggle("Enable SSAO"            , &m_enableSSAO, Update_cb);
				bk.AddToggle("SSAO Technique Override", &m_SSAOTechniqueOverride, Update_cb);
				bk.AddCombo ("SSAO Technique"         , &m_SSAOTechnique, ssaotechnique_count, SSAO::GetSSAOTechniqueNames(), Update_cb);
				bk.AddToggle("SSAO Isolate"           , &m_SSAOIsolate, Update_cb);
				bk.AddSlider("SSAO Intensity"         , &m_SSAOIntensity, 0.0f, 20.0f, 1.0f/32.0f, Update_cb);
				bk.AddSlider("SSAO Scale"             , &m_SSAOScale, 0.0f, 4.0f, 1.0f/32.0f, Update_cb);
				bk.AddToggle("Night"                  , &m_night, Update_cb);
				bk.AddSlider("Corona Scale"           , &m_coronaScale, 0.1f, 100.0f, 1.0f/32.0f, Update_cb);
				bk.AddToggle("Simple Post Fx"         , &m_simplePostFx, Update_cb);
				bk.AddToggle("Water Overlay"          , &m_waterOverlay, Update_cb);
				bk.AddToggle("Water Only"             , &m_waterOnly, Update_cb);
				bk.AddToggle("Non Orthographic"       , &m_nonOrthographic);
			}
			bk.PopGroup();
		}

		bool  m_enabled;
		char  m_name[80];
		bool  m_drawLODs;
		int   m_gbufferIndex;
		bool  m_enableProps;
		bool  m_enableTrees;
		bool  m_enableVfx;
		bool  m_enableAmbientOnly;
		bool  m_enableShadows;
		bool  m_enableSpecular;
		bool  m_enableSSAO;
		bool  m_SSAOTechniqueOverride;
		int   m_SSAOTechnique;
		bool  m_SSAOIsolate;
		float m_SSAOIntensity;
		float m_SSAOScale;
		bool  m_night;
		float m_coronaScale;
		bool  m_simplePostFx;
		bool  m_waterOverlay; // overlay water with blue
		bool  m_waterOnly; // overlay all non-water with black
		bool  m_nonOrthographic; // currently required for shadows ..
	};

	enum { TILED_SCREEN_CAPTURE_MAX_LAYERS = 8 };

	static CTSCLayer g_layers[TILED_SCREEN_CAPTURE_MAX_LAYERS];
	static int       g_layerIndex = 0;

	static bool  g_stateEnabled              = false;
	static bool  g_stateStopped              = false;
	static bool  g_stateDetachCamera         = false;
	static int   g_state                     = STATE_OFF;
	static u32   g_stateCurrentTime          = 0;
	static u32   g_stateTimer                = 0; // time at which to process the current state
	static u32   g_stateFrameDelay           = 0; // number of additional frames to wait until we process the current state
	//static char  g_path[80]                  = "assets:/non_final/screencapture/tiles/tile";
	static char  g_tilesPath[80]             = "assets:/non_final/screencapture/tiles";
	static bool  g_verbose                   = true;
	static bool  g_verboseTimer              = false;
	static bool  g_verboseWaiting            = false;
	static bool  g_waitForStreaming          = true;
	static bool  g_waitForEntitiesLoaded     = false; // TODO -- this breaks the teleport/streaming somehow, wtf?
	static bool  g_skipToNext                = false;
	static bool  g_captureOnce               = false;

	static u32   g_advanceTimeDelay          = 2*1000; // 2 secs
	static u32   g_advanceFrameDelay         = 3;
	static u32   g_renderTimeDelay           = 0;
	static u32   g_renderFrameDelay          = 5;
	static u32   g_renderFrameDelay2         = 5;

	static bool  g_interlaceEnabled          = false;
	static int   g_interlaceW                = 4;
	static int   g_interlaceH                = 4;
	static char  g_interlacePath[80]         = "assets:/non_final/screencapture/interlace/interlace";
	static int   g_interlaceIndex            = 0;

	static float g_LODScale                  = __WIN32PC ? 2000.0f : 10.0f;
	static bool  g_allIMAPsAreLoaded         = false;
	static int   g_numEntitiesNotLoaded      = 0;
	static bool  g_reportEntitiesNotLoaded   = false;
	static int   g_maxPVSSize                = 0;

	static bool  g_startupRemovePedsAndVehs  = false;
	static bool  g_startupStreamingFlush     = false;

	// y screen resolution should be: (tileSize + 2*margin)*pixelsPerMetre
	// x screen resolution should be: y_resolution*aspect
	// let's say tile size is 150, margin is 30 metres and we want 4 pixels/metre
	// y screen resolution should be (150 + 2*30)*4 = 840
	// x screen resolution should be 840*(5/3) = 1400

	// on rdr, tile size is 256 ..
	// we need to adjust margin so that the numbers come out evenly
	// y screen resolution should be (256 + 2*7)*4 = 1080
	// x screen resolution should be 1080*(5/3) = 1800

	static float g_worldBoundsMinX           = (float)gv::NAVMESH_BOUNDS_MIN_X;
	static float g_worldBoundsMinY           = (float)gv::NAVMESH_BOUNDS_MIN_Y;
	static float g_worldBoundsMaxX           = (float)gv::NAVMESH_BOUNDS_MAX_X;
	static float g_worldBoundsMaxY           = (float)gv::NAVMESH_BOUNDS_MAX_Y;
	static float g_captureBoundsMinX         = (float)gv::WORLD_BOUNDS_MIN_X;
	static float g_captureBoundsMinY         = (float)gv::WORLD_BOUNDS_MIN_Y;
	static float g_captureBoundsMaxX         = (float)gv::WORLD_BOUNDS_MAX_X;
	static float g_captureBoundsMaxY         = (float)gv::WORLD_BOUNDS_MAX_Y;
	static float g_tileSizeX                 = (float)gv::WORLD_TILE_SIZE;
	static float g_tileSizeY                 = (float)gv::WORLD_TILE_SIZE;
	static float g_tileSizeScale             = 1.0f;
	static float g_tileSizeMargin            = HACK_RDR3 ? 7.0f : 30.0f; // extra space around tile so that SSAO and bloom don't cause seams
	static float g_tileBoundsCropMinX        = 0.0f;
	static float g_tileBoundsCropMinY        = 0.0f;
	static float g_tileBoundsCropMaxX        = 0.0f;
	static float g_tileBoundsCropMaxY        = 0.0f;
	static float g_tileBoundsMinX            = 0.0f;
	static float g_tileBoundsMinY            = 0.0f;
	static float g_tileBoundsMinZ            = 0.0f;
	static float g_tileBoundsMaxX            = 0.0f;
	static float g_tileBoundsMaxY            = 0.0f;
	static float g_tileBoundsMaxZ            = 0.0f;
	static int   g_tileCoordI                = 0;
	static int   g_tileCoordJ                = 0;
	static int   g_tileCoordStep             = gv::WORLD_CELLS_PER_TILE;
	static bool  g_tileSkipExistingImages    = true;
	static int   g_chunkIndexI               = -1;
	static int   g_chunkIndexJ               = -1;
	static int   g_chunkExtentX              = 1;
	static int   g_chunkExtentY              = 1;
	static int   g_chunkSizeX                = 21;
	static int   g_chunkSizeY                = 21;

	static bool  g_shadowUseMinHeightHM      = false;
	static bool  g_shadowUseMaxHeightHM      = false;
	static bool  g_shadowUseMaxHeightCamera  = false;

	static bool  g_cameraForceUpdate         = false;
	static bool  g_cameraHeightUseHeightmap  = false; // use heightmap to calculate camera height (should be fine for ortho, but i'm having issues .. turned off for now)
	static float g_cameraHeight              = __WIN32PC ? 1000.0f : 600.0f; // wait, why did i make this different for pc?
	static float g_cameraHeightOffset        = 10.0f; // offset above heightmap to place camera
	static float g_playerHeightOffset        = -3.0f; // offset below heightmap to place player
	static float g_cameraOrthoMinZ           = 0.0f;
	static float g_cameraOrthoMaxZOffset     = 10.0f; // maxZ = camPos.z - minHeight + maxZOffset
	static float g_cameraNearClip            = 50.0f;
	static float g_cameraFarClip             = 10000.0f;
	static float g_cameraFovScale            = 1.0f;
	static float g_cameraFov                 = 1.0f;
	static Vec3V g_cameraPos                 = Vec3V(V_ZERO);
	static Vec3V g_warpedPos                 = Vec3V(V_ZERO); // position we've warped to
	static Vec3V g_playerPos                 = Vec3V(V_ZERO); // actual position of player

	static int   g_downsampleImage           = 2; // 360x360 tiles (assuming 720 pixel vertical resolution)
	static bool  g_saveScreenImage           = true;
	static bool  g_saveDepthImage            = false;
	static bool  g_saveGBufferAlbedoImage    = false;
	static bool  g_saveGBufferNormalImage    = false;
	static bool  g_saveGBufferSpecularImage  = false;
	static bool  g_saveGBufferAmbientImage   = false;

	static bool  g_useOrthoViewport          = __WIN32PC ? true : false;
	static bool  g_useOrthoViewportAlways    = false;
	static float g_useOrthoViewportAmount    = 1.0f;
	static bool  g_useOrthoViewportZeroFOV   = true;
	static bool  g_shearUpdate               = false;
	static float g_shearX                    = 0.0f;
	static float g_shearY                    = 0.0f;

	// TODO -- copied these from DebugTextureViewer, they should be moved down to rage level (grcTexture)
	template <typename T> static __forceinline T GetFromLocalMemory(const T* ptr, int i = 0);
	static __forceinline int GetLocalMemoryAddress(int x, int y, int w, int pitch);
#if __XENON
	static __forceinline float ConvertD24F(u32 raw);
#endif // __XENON

	enum
	{
		CAPTURE_TYPE_RGB,
		CAPTURE_TYPE_DEPTH,
		CAPTURE_TYPE_STENCIL
	};

	static void SetState(int state, u32 timeDelay, u32 frameDelay);
	static bool AreAllEntitiesLoaded();
	static atString TileGetFileName(int coordI, int coordJ, const char* layerName);
	static bool TileExists(int coordI, int coordJ);
	static void UpdateEnabled();
	static void UpdateLayer(const CTSCLayer* sd); // pass NULL to reset
	static void UpdateTileBounds();
	static void UpdatePlayer(CPed* pPlayerPed);
	static void UpdateCameraPosAndFov();
	static void UpdateCamera();
	static void CaptureTile(const char* path, const grcRenderTarget* rt, int downsample, int captureType, bool bInterlaceFirst = false, bool bInterlaceLast = false);
}

// ================================================================================================

void TiledScreenCapture::AddWidgets(bkBank& bk)
{
	class StateEnabled_cb { public: static void func()
	{
		if (g_stateEnabled || g_interlaceEnabled)
		{
			g_state = STATE_STARTING;
		}
		else
		{
			g_state = STATE_OFF;
		}

		g_stateTimer = 0;
		g_stateFrameDelay = 0;
		g_tileCoordI = 0;
		g_tileCoordJ = 0;

		UpdateEnabled();
	}};

	class DetachCamera_button { public: static void func()
	{
		g_stateDetachCamera = true;
	}};

	class SkipToNext_button { public: static void func()
	{
		g_skipToNext = true;
	}};

	class CaptureOnce_button { public: static void func()
	{
		g_captureOnce = true;
	}};

	class ReportEntitiesNotLoaded_button { public: static void func()
	{
		g_reportEntitiesNotLoaded = true;
	}};

	class ResetMaxPVSSize_button { public: static void func()
	{
		g_maxPVSSize = 0;
	}};

	class UpdateTileCoords_cb { public: static void func()
	{
		UpdateTileBounds();
		g_cameraForceUpdate = true;
	}};

	class UpdateCamera_cb { public: static void func()
	{
		g_cameraForceUpdate = true;
	}};

	class UpdateCameraHeight_cb { public: static void func()
	{
		const float minFov = 1.0f;
		const float maxCameraHeight = g_tileSizeY*0.5f/tanf(DtoR*0.5f*minFov/g_cameraFovScale);
		//Displayf("maxCameraHeight = %f", maxCameraHeight); // TEMPORARY
		g_cameraHeight = Min<float>(maxCameraHeight, g_cameraHeight);
		UpdateCamera_cb::func();
	}};

	class UpdateCameraFovScale_cb { public: static void func()
	{
		const float minFov = 1.0f;
		const float maxFov = 130.0f;
		const float minCameraFovScale = DtoR*0.5f*minFov/atan2f(g_tileSizeY/2.0f, g_cameraHeight);
		const float maxCameraFovScale = DtoR*0.5f*maxFov/atan2f(g_tileSizeY/2.0f, g_cameraHeight);
		//Displayf("cameraFovScale range is [%f..%f]", minCameraFovScale, maxCameraFovScale); // TEMPORARY
		g_cameraFovScale = Clamp<float>(g_cameraFovScale, minCameraFovScale, maxCameraFovScale);
		UpdateCamera_cb::func();
	}};

	class UpdateWarpedPos_cb { public: static void func()
	{
		CPed* pPlayerPed = FindPlayerPed();

		if (pPlayerPed)
		{
			pPlayerPed->Teleport(RCC_VECTOR3(g_warpedPos), 0.0f);
		}
	}};

	class UpdateShear_cp { public: static void func()
	{
		g_shearUpdate = true;
	}};

	//bk.PushGroup("Tiled Screen Capture", false);
	{
		bk.AddToggle("Enabled"                          , &g_stateEnabled, StateEnabled_cb::func);
		bk.AddToggle("Stopped"                          , &g_stateStopped);
		bk.AddButton("Detach Camera"                    , DetachCamera_button::func);
		bk.AddCombo ("State"                            , &g_state, NELEM(g_stateStrings), g_stateStrings);
		bk.AddSlider("State Current Time"               , &g_stateCurrentTime, 0, 0xffffffff, 0);
		bk.AddSlider("State Timer"                      , &g_stateTimer, 0, 0xffffffff, 0);
		bk.AddSlider("State Frame Delay"                , &g_stateFrameDelay, 0, 0xffffffff, 0);
		bk.AddText  ("Tiles Path"                       , &g_tilesPath[0], sizeof(g_tilesPath), false);
		bk.AddToggle("Verbose"                          , &g_verbose);
		bk.AddToggle("Verbose Timer"                    , &g_verboseTimer);
		bk.AddToggle("Verbose Waiting"                  , &g_verboseWaiting);
		bk.AddToggle("Wait For Streaming"               , &g_waitForStreaming);
		bk.AddToggle("Wait For Entities Loaded"         , &g_waitForEntitiesLoaded);
		bk.AddButton("Skip To Next"                     , SkipToNext_button::func);
		bk.AddButton("Capture Once"                     , CaptureOnce_button::func);
	//	bk.AddToggle("HD Only"                          , &CPostScanDebug::GetDrawByLodLevelRef());

		bk.AddSeparator();

		bk.AddSlider("Advance Time Delay (ms)"          , &g_advanceTimeDelay, 0, 100*1000, 1);
		bk.AddSlider("Advance Frame Delay"              , &g_advanceFrameDelay, 0, 100, 1);
		bk.AddSlider("Render Time Delay (ms)"           , &g_renderTimeDelay, 0, 100*1000, 1);
		bk.AddSlider("Render Frame Delay"               , &g_renderFrameDelay, 0, 100, 1);
		bk.AddSlider("Render Frame Delay 2"             , &g_renderFrameDelay2, 0, 100, 1);

		bk.AddSeparator();

		bk.AddToggle("Interlace Enabled"                , &g_interlaceEnabled, StateEnabled_cb::func);
		bk.AddSlider("Interlace Width"                  , &g_interlaceW, 1, 8, 1);
		bk.AddSlider("Interlace Height"                 , &g_interlaceH, 1, 8, 1);
		bk.AddText  ("Interlace Path"                   , &g_interlacePath[0], sizeof(g_interlacePath), false);
		bk.AddSlider("Interlace Index"                  , &g_interlaceIndex, 0, 100, 1);

		bk.AddSeparator();

		bk.AddSlider("LOD Scale"                        , &g_LODScale, 1.0f, 9999.0f, 0.1f);
		bk.AddToggle("All IMAPs Are Loaded"             , &g_allIMAPsAreLoaded);
		bk.AddSlider("Num Entities Not Loaded"          , &g_numEntitiesNotLoaded, 0, 99999, 0);
		bk.AddButton("Report Entities Not Loaded"       , ReportEntitiesNotLoaded_button::func);
		bk.AddSlider("Max PVS Size"                     , &g_maxPVSSize, 0, 99999, 0);
		bk.AddButton("Reset Max PVS Size"               , ResetMaxPVSSize_button::func);

		bk.AddSeparator();

		g_layers[0].m_enabled = true;

		for (int i = 0; i < NELEM(g_layers); i++)
		{
			sprintf(g_layers[i].m_name, "layer_%04d", i);
			g_layers[i].AddWidgets(bk, i);
		}

		bk.AddToggle("Startup - Remove Peds/Vehicles"   , &g_startupRemovePedsAndVehs, UpdateEnabled);
		bk.AddToggle("Startup - Steaming Flush"         , &g_startupStreamingFlush, UpdateEnabled);

		bk.AddSeparator();

		bk.AddSlider("World Bounds Min X"               , &g_worldBoundsMinX  , -99999.0f, 99999.0f, 1.0f); //(float)gv::NAVMESH_BOUNDS_MIN_X, (float)gv::NAVMESH_BOUNDS_MAX_X, 1.0f);
		bk.AddSlider("World Bounds Min Y"               , &g_worldBoundsMinY  , -99999.0f, 99999.0f, 1.0f); //(float)gv::NAVMESH_BOUNDS_MIN_Y, (float)gv::NAVMESH_BOUNDS_MAX_Y, 1.0f);
		bk.AddSlider("World Bounds Max X"               , &g_worldBoundsMaxX  , -99999.0f, 99999.0f, 1.0f); //(float)gv::NAVMESH_BOUNDS_MIN_X, (float)gv::NAVMESH_BOUNDS_MAX_X, 1.0f);
		bk.AddSlider("World Bounds Max Y"               , &g_worldBoundsMaxY  , -99999.0f, 99999.0f, 1.0f); //(float)gv::NAVMESH_BOUNDS_MIN_Y, (float)gv::NAVMESH_BOUNDS_MAX_Y, 1.0f);
		bk.AddSlider("Capture Bounds Min X"             , &g_captureBoundsMinX, -99999.0f, 99999.0f, 1.0f); //(float)gv::WORLD_BOUNDS_MIN_X, (float)gv::WORLD_BOUNDS_MAX_X, 1.0f);
		bk.AddSlider("Capture Bounds Min Y"             , &g_captureBoundsMinY, -99999.0f, 99999.0f, 1.0f); //(float)gv::WORLD_BOUNDS_MIN_Y, (float)gv::WORLD_BOUNDS_MAX_Y, 1.0f);
		bk.AddSlider("Capture Bounds Max X"             , &g_captureBoundsMaxX, -99999.0f, 99999.0f, 1.0f); //(float)gv::WORLD_BOUNDS_MIN_X, (float)gv::WORLD_BOUNDS_MAX_X, 1.0f);
		bk.AddSlider("Capture Bounds Max Y"             , &g_captureBoundsMaxY, -99999.0f, 99999.0f, 1.0f); //(float)gv::WORLD_BOUNDS_MIN_Y, (float)gv::WORLD_BOUNDS_MAX_Y, 1.0f);
		bk.AddSlider("Tile Size X"                      , &g_tileSizeX, 1.0f, 512.0f, 1.0f);
		bk.AddSlider("Tile Size Y"                      , &g_tileSizeY, 1.0f, 512.0f, 1.0f);
		bk.AddSlider("Tile Size Scale"                  , &g_tileSizeScale, 0.1f, 10.0f, 0.01f);
		bk.AddSlider("Tile Size Margin"                 , &g_tileSizeMargin, 0.0f, 512.0f, 1.0f);
		/*
		bk.AddSlider("Tile Bounds Crop Min X"           , &g_tileBoundsCropMinX, (float)gv::NAVMESH_BOUNDS_MIN_X, (float)gv::NAVMESH_BOUNDS_MAX_X, 1.0f);
		bk.AddSlider("Tile Bounds Crop Min Y"           , &g_tileBoundsCropMinY, (float)gv::NAVMESH_BOUNDS_MIN_Y, (float)gv::NAVMESH_BOUNDS_MAX_Y, 1.0f);
		bk.AddSlider("Tile Bounds Crop Max X"           , &g_tileBoundsCropMaxX, (float)gv::NAVMESH_BOUNDS_MIN_X, (float)gv::NAVMESH_BOUNDS_MAX_X, 1.0f);
		bk.AddSlider("Tile Bounds Crop Max Y"           , &g_tileBoundsCropMaxY, (float)gv::NAVMESH_BOUNDS_MIN_Y, (float)gv::NAVMESH_BOUNDS_MAX_Y, 1.0f);
		bk.AddSlider("Tile Bounds Min X"                , &g_tileBoundsMinX, (float)gv::NAVMESH_BOUNDS_MIN_X, (float)gv::NAVMESH_BOUNDS_MAX_X, 1.0f);
		bk.AddSlider("Tile Bounds Min Y"                , &g_tileBoundsMinY, (float)gv::NAVMESH_BOUNDS_MIN_Y, (float)gv::NAVMESH_BOUNDS_MAX_Y, 1.0f);
		bk.AddSlider("Tile Bounds Min Z"                , &g_tileBoundsMinZ, 0.0f, (float)gv::WORLD_BOUNDS_MAX_Z, 1.0f);
		bk.AddSlider("Tile Bounds Max X"                , &g_tileBoundsMaxX, (float)gv::NAVMESH_BOUNDS_MIN_X, (float)gv::NAVMESH_BOUNDS_MAX_X, 1.0f);
		bk.AddSlider("Tile Bounds Max Y"                , &g_tileBoundsMaxY, (float)gv::NAVMESH_BOUNDS_MIN_Y, (float)gv::NAVMESH_BOUNDS_MAX_Y, 1.0f);
		bk.AddSlider("Tile Bounds Max Z"                , &g_tileBoundsMaxZ, 0.0f, (float)gv::WORLD_BOUNDS_MAX_Z, 1.0f);
		*/
		bk.AddSlider("Tile Coord I"                     , &g_tileCoordI, 0, 9999, 1, UpdateTileCoords_cb::func);
		bk.AddSlider("Tile Coord J"                     , &g_tileCoordJ, 0, 9999, 1, UpdateTileCoords_cb::func);
		bk.AddToggle("Tile Skip Existing Images"        , &g_tileSkipExistingImages);

		bk.AddSeparator();

		bk.AddToggle("Shadow Use Min Height HM"         , &g_shadowUseMinHeightHM);
		bk.AddToggle("Shadow Use Max Height HM"         , &g_shadowUseMaxHeightHM);
		bk.AddToggle("Shadow Use Max Height Camera"     , &g_shadowUseMaxHeightCamera);

		bk.AddSeparator();

		bk.AddToggle("Camera Height Use Heightmap"      , &g_cameraHeightUseHeightmap, UpdateCameraHeight_cb::func);
		bk.AddSlider("Camera Height"                    , &g_cameraHeight, 100.0f, 9999.0f, 1.0f, UpdateCameraHeight_cb::func);
		bk.AddSlider("Camera Near Clip"                 , &g_cameraNearClip, 0.1f, 10000.0f, 0.1f, UpdateCameraFovScale_cb::func);
		bk.AddSlider("Camera Far Clip"                  , &g_cameraFarClip, 0.1f, 10000.0f, 0.1f, UpdateCameraFovScale_cb::func);
		bk.AddSlider("Camera Fov Scale"                 , &g_cameraFovScale, 0.1f, 10.0f, 0.01f, UpdateCameraFovScale_cb::func);
		bk.AddSlider("Camera Fov"                       , &g_cameraFov, 1.0f, 130.0f, 0.0f, UpdateCameraPosAndFov);
		bk.AddVector("Camera Position"                  , &g_cameraPos, -16000.0f, 16000.0f, 1.0f, UpdateCameraPosAndFov);
		bk.AddVector("Warped Position"                  , &g_warpedPos, -16000.0f, 16000.0f, 1.0f, UpdateWarpedPos_cb::func);
		bk.AddVector("Player Position"                  , &g_playerPos, -16000.0f, 16000.0f, 0.0f);

		bk.AddSeparator();

		bk.AddSlider("Downsample Image"                 , &g_downsampleImage, 1, 8, 1);
		bk.AddToggle("Save Screen Image"                , &g_saveScreenImage);
		bk.AddToggle("Save Depth Image"                 , &g_saveDepthImage);
		bk.AddToggle("Save GBuffer Albedo Image"        , &g_saveGBufferAlbedoImage);
		bk.AddToggle("Save GBuffer Normal Image"        , &g_saveGBufferNormalImage);
		bk.AddToggle("Save GBuffer Specular Image"      , &g_saveGBufferSpecularImage);
		bk.AddToggle("Save GBuffer Ambient Image"       , &g_saveGBufferAmbientImage);

		bk.AddTitle("Experimental");

		bk.AddToggle("Use Ortho Viewport"               , &g_useOrthoViewport, UpdateShear_cp::func);
		bk.AddToggle("Use Ortho Viewport Always"        , &g_useOrthoViewportAlways, UpdateShear_cp::func);
		bk.AddSlider("Use Ortho Viewport Amount"        , &g_useOrthoViewportAmount, 0.0f, 1.0f, 1.0f/32.0f, UpdateShear_cp::func);
		bk.AddToggle("Use Ortho Viewport Zero FOV"      , &g_useOrthoViewportZeroFOV, UpdateShear_cp::func);
		bk.AddSlider("Test Shear X"                     , &g_shearX, -1000.0f, 1000.0f, 1.0f, UpdateShear_cp::func);
		bk.AddSlider("Test Shear Y"                     , &g_shearY, -1000.0f, 1000.0f, 1.0f, UpdateShear_cp::func);

		bk.PushGroup("Terrain Capture", false);
		{
			static bool s_enableProps = false; // no props (requested by Aaron)
			static bool s_waterOverlay = true;

			class TerrainCaptureEnableProps_update { public: static void func()
			{
				for (int i = 0; i < NELEM(g_layers); i++)
				{
					g_layers[i].m_enableProps = s_enableProps;
				}
			}};

			class TerrainCaptureWaterOverlay_update { public: static void func()
			{
				for (int i = 0; i < NELEM(g_layers); i++)
				{
					g_layers[i].m_waterOverlay = s_waterOverlay;
				}
			}};

			bk.AddToggle("Enable Props", &s_enableProps, TerrainCaptureEnableProps_update::func);
			bk.AddToggle("Water Overlay", &s_waterOverlay, TerrainCaptureWaterOverlay_update::func);

			class GetFullTilesPath { public: static void func(char* path)
			{
				strcpy(path, g_tilesPath);

				const char* assetsStr = "assets:";
				const int   assetsLen = istrlen(assetsStr);

				if (memcmp(path, assetsStr, assetsLen) == 0)
				{
					strcpy(path, RS_ASSETS);
					strcat(path, g_tilesPath + assetsLen);
				}

				for (char* s = path; *s; s++)
				{
					if (*s == '/')
						*s = '\\';
				}
			}};

			class ClearTileImageDirectory_button { public: static void func()
			{
				char path[512] = "";
				GetFullTilesPath::func(path);
				char cmd[512] = "";
			//	sprintf(cmd, "start del %s *.dds /Q", path);
				sprintf(cmd, "del %s *.dds /Q", path);
				Displayf("> %s", cmd);
				sysExec(cmd);
			}};

			class OpenTileImageDirectory_button { public: static void func()
			{
				char path[512] = "";
				GetFullTilesPath::func(path);
				char cmd[512] = "";
				sprintf(cmd, "explorer %s", path);
				Displayf("> %s", cmd);
				sysExec(cmd);
			}};

			static float s_captureBoundsExtentAroundCamera = 0.0f*(float)gv::WORLD_TILE_SIZE;

			class SetCaptureBoundsAroundCamera_button { public: static void func()
			{
				const Vec3V camPos = gVpMan.GetUpdateGameGrcViewport()->GetCameraPosition();

				g_captureBoundsMinX = g_tileSizeX*floorf((camPos.GetXf() - s_captureBoundsExtentAroundCamera)/g_tileSizeX);
				g_captureBoundsMinY = g_tileSizeY*floorf((camPos.GetYf() - s_captureBoundsExtentAroundCamera)/g_tileSizeY);
				g_captureBoundsMaxX = g_tileSizeX*ceilf ((camPos.GetXf() + s_captureBoundsExtentAroundCamera)/g_tileSizeX);
				g_captureBoundsMaxY = g_tileSizeY*ceilf ((camPos.GetYf() + s_captureBoundsExtentAroundCamera)/g_tileSizeY);

				const int tileCoordMinI = g_tileCoordStep*(int)floorf((g_captureBoundsMinX - g_worldBoundsMinX)/g_tileSizeX);
				const int tileCoordMinJ = g_tileCoordStep*(int)floorf((g_captureBoundsMinY - g_worldBoundsMinY)/g_tileSizeY);
				const int tileCoordMaxI = g_tileCoordStep*((int)ceilf((g_captureBoundsMaxX - g_worldBoundsMinX)/g_tileSizeX) - 1);
				const int tileCoordMaxJ = g_tileCoordStep*((int)ceilf((g_captureBoundsMaxY - g_worldBoundsMinY)/g_tileSizeY) - 1);

				const int numTilesI = (tileCoordMaxI - tileCoordMinI)/g_tileCoordStep + 1;
				const int numTilesJ = (tileCoordMaxJ - tileCoordMinJ)/g_tileCoordStep + 1;

				Displayf("set capture bounds - %d tiles, coords [%d..%d],[%d..%d], x=[%.1f..%.1f], y=[%.1f..%.1f]", numTilesI*numTilesJ, tileCoordMinI, tileCoordMaxI, tileCoordMinJ, tileCoordMaxJ, g_captureBoundsMinX, g_captureBoundsMaxX, g_captureBoundsMinY, g_captureBoundsMaxY);
			}};

			class SetSpecificCaptureBounds_Gta { public: static void func()
			{
				g_worldBoundsMinX   = -6000.0f; //NAVMESH_BOUNDS_MIN_X;
				g_worldBoundsMinY   = -6000.0f; //NAVMESH_BOUNDS_MIN_Y;
				g_worldBoundsMaxX   = +9000.0f; //NAVMESH_BOUNDS_MAX_X;
				g_worldBoundsMaxY   = +9000.0f; //NAVMESH_BOUNDS_MAX_Y;
				g_captureBoundsMinX	= -4050.0f; //WORLD_BOUNDS_MIN_X;
				g_captureBoundsMinY	= -4050.0f; //WORLD_BOUNDS_MIN_Y;
				g_captureBoundsMaxX	= +5100.0f; //WORLD_BOUNDS_MAX_X;
				g_captureBoundsMaxY	= +8400.0f; //WORLD_BOUNDS_MAX_Y;
				Displayf("set capture bounds - gta map");
			}};

			class SetSpecificCaptureBounds_Rdr { public: static void func()
			{
				g_worldBoundsMinX   = -4096.0f;
				g_worldBoundsMinY   = -4096.0f;
				g_worldBoundsMaxX   = +4608.0f;
				g_worldBoundsMaxY   = +4096.0f;
				g_captureBoundsMinX	= g_worldBoundsMinX;
				g_captureBoundsMinY	= g_worldBoundsMinY;
				g_captureBoundsMaxX	= g_worldBoundsMaxX;
				g_captureBoundsMaxY	= g_worldBoundsMaxY;
				Displayf("set capture bounds - rdr map");
			}};

			class SetSpecificCaptureBounds_Liberty { public: static void func()
			{
				g_worldBoundsMinX   = -6000.0f; // NAVMESH_BOUNDS_MIN_X;
				g_worldBoundsMinY   = -6000.0f; // NAVMESH_BOUNDS_MIN_Y;
				g_worldBoundsMaxX   = +9000.0f; // NAVMESH_BOUNDS_MAX_X;
				g_worldBoundsMaxY   = +9000.0f; // NAVMESH_BOUNDS_MAX_Y;
				g_captureBoundsMinX	= -4096.0f;
				g_captureBoundsMinY	= -4096.0f;
				g_captureBoundsMaxX	= +12288.0f;
				g_captureBoundsMaxY	= +12288.0f;
				Displayf("set capture bounds - liberty map");
			}};

			class TerrainCaptureSettings { public: static void func()
			{
				if (g_chunkIndexI >= 0 &&
					g_chunkIndexJ >= 0)
				{
					g_captureBoundsMinX = (float)(gv::WORLD_BOUNDS_MIN_X + (g_chunkSizeX*(int)g_tileSizeX)*(g_chunkIndexI + 0));
					g_captureBoundsMinY = (float)(gv::WORLD_BOUNDS_MIN_Y + (g_chunkSizeY*(int)g_tileSizeY)*(g_chunkIndexJ + 0));
					g_captureBoundsMaxX = (float)(gv::WORLD_BOUNDS_MIN_X + (g_chunkSizeX*(int)g_tileSizeX)*(g_chunkIndexI + g_chunkExtentX));
					g_captureBoundsMaxY = (float)(gv::WORLD_BOUNDS_MIN_Y + (g_chunkSizeY*(int)g_tileSizeY)*(g_chunkIndexJ + g_chunkExtentY));
				}
				else
				{
					g_captureBoundsMinX = -2400.0f;
					g_captureBoundsMinY =  -450.0f;
					g_captureBoundsMaxX = +2400.0f;
					g_captureBoundsMaxY = +4350.0f;
				}
			}};

			class TerrainCaptureDiffuse_button { public: static void func()
			{
				TerrainCaptureSettings::func();
				int layerCount = 0;

				for (int i = 0; i < NELEM(g_layers); i++)
				{
					g_layers[i] = CTSCLayer();
					sprintf(g_layers[i].m_name, "layer_%04d", i);
				}

				// layer 0
				g_layers[layerCount].m_gbufferIndex = 1; // diffuse
				layerCount++;

				for (int i = 0; i < NELEM(g_layers); i++)
				{
					g_layers[i].m_enabled = (i < layerCount);
					g_layers[i].m_enableProps = s_enableProps;
					g_layers[i].m_waterOverlay = s_waterOverlay;
				}
			}};

			class TerrainCaptureAmbient_button { public: static void func()
			{
				TerrainCaptureSettings::func();
				int layerCount = 0;

				for (int i = 0; i < NELEM(g_layers); i++)
				{
					g_layers[i] = CTSCLayer();
					sprintf(g_layers[i].m_name, "layer_%04d", i);
				}

				// layer 0
				g_layers[layerCount].m_enableAmbientOnly = true;
#if __D3D11
				g_layers[layerCount].m_SSAOTechniqueOverride = true;
				g_layers[layerCount].m_SSAOTechnique = ssaotechnique_qsssao2;
#endif // __D3D11
				g_layers[layerCount].m_SSAOIntensity = 6.0f;
				g_layers[layerCount].m_simplePostFx = true;
				layerCount++;

				for (int i = 0; i < NELEM(g_layers); i++)
				{
					g_layers[i].m_enabled = (i < layerCount);
					g_layers[i].m_enableProps = s_enableProps;
					g_layers[i].m_waterOverlay = s_waterOverlay;
				}
			}};

			class TerrainCaptureAllLayers_button { public: static void func()
			{
				TerrainCaptureSettings::func();
				int layerCount = 0;

				for (int i = 0; i < NELEM(g_layers); i++)
				{
					g_layers[i] = CTSCLayer();
					sprintf(g_layers[i].m_name, "layer_%04d", i);
				}

				// layer 0
				g_layers[layerCount].m_gbufferIndex = 1; // diffuse
				layerCount++;

				// layer 1
				g_layers[layerCount].m_gbufferIndex = 2; // normal
				layerCount++;

				// layer 2
				g_layers[layerCount].m_enableAmbientOnly = true;
#if __D3D11
				g_layers[layerCount].m_SSAOTechniqueOverride = true;
				g_layers[layerCount].m_SSAOTechnique = ssaotechnique_qsssao2;
#endif // __D3D11
				g_layers[layerCount].m_SSAOIntensity = 6.0f;
				g_layers[layerCount].m_simplePostFx = true;
				g_layers[layerCount].m_waterOverlay = true;
				layerCount++;

			// layer 3
#if __D3D11
				g_layers[layerCount].m_SSAOTechniqueOverride = true;
				g_layers[layerCount].m_SSAOTechnique = ssaotechnique_qsssao2;
#endif // __D3D11
				g_layers[layerCount].m_SSAOIsolate = true;
				g_layers[layerCount].m_SSAOIntensity = 6.0f;
				layerCount++;

				// layer 4
				g_layers[layerCount].m_gbufferIndex = 4; // ambient
				layerCount++;

				for (int i = 0; i < NELEM(g_layers); i++)
				{
					g_layers[i].m_enabled = (i < layerCount);
					g_layers[i].m_enableProps = s_enableProps;
					g_layers[i].m_waterOverlay = s_waterOverlay;
				}
			}};

			class TerrainCapturStart_button { public: static void func()
			{
				g_stateEnabled = true;
				StateEnabled_cb::func();
			}};

			class TerrainCapturStop_button { public: static void func()
			{
				g_stateEnabled = false;
				StateEnabled_cb::func();
			}};

			bk.AddSlider("Chunk Index I", &g_chunkIndexI, -1, 8, 1);
			bk.AddSlider("Chunk Index J", &g_chunkIndexJ, -1, 8, 1);
			bk.AddSlider("Chunk Extent X", &g_chunkExtentX, 1, 3, 1);
			bk.AddSlider("Chunk Extent Y", &g_chunkExtentY, 1, 4, 1);
			bk.AddButton("Clear Tile Image Directory", ClearTileImageDirectory_button::func);
			bk.AddButton("Open Tile Image Directory", OpenTileImageDirectory_button::func);
			bk.AddButton("Set Capture Bounds Around Camera" , SetCaptureBoundsAroundCamera_button::func);
			bk.AddButton("Set Capture Bounds GTAV Map"    , SetSpecificCaptureBounds_Gta::func);
			bk.AddButton("Set Capture Bounds RDR3 Map"    , SetSpecificCaptureBounds_Rdr::func);
			bk.AddButton("Set Capture Bounds Liberty Map" , SetSpecificCaptureBounds_Liberty::func);
			bk.AddSlider("Set Capture Bounds Extent", &s_captureBoundsExtentAroundCamera, 0.0f, 4096.0f, 1.0f);
			bk.AddButton("Set Capture Settings - Diffuse", TerrainCaptureDiffuse_button::func);
			bk.AddButton("Set Capture Settings - Ambient", TerrainCaptureAmbient_button::func);
			bk.AddButton("Set Capture Settings - All Layers", TerrainCaptureAllLayers_button::func);
			bk.AddButton("Start", TerrainCapturStart_button::func);
			bk.AddButton("Stop", TerrainCapturStop_button::func);
		}
		bk.PopGroup();
	}
	//bk.PopGroup();
}

bool TiledScreenCapture::IsEnabled()
{
	if (g_interlaceEnabled)
	{
		return false; // interlaced screen shots use all the "normal" graphics features
	}

	return g_state != STATE_OFF;
}

bool TiledScreenCapture::IsEnabledOrthographic()
{
	return IsEnabled() && g_useOrthoViewport;
}

void TiledScreenCapture::SetCurrentPVSSize(int currentPVSSize)
{
	if (IsEnabled())
	{
		if (g_maxPVSSize < currentPVSSize)
		{
			g_maxPVSSize = currentPVSSize;
			Displayf("max PVS size = %d", g_maxPVSSize);
		}
	}
}

void TiledScreenCapture::GetTileBounds(spdAABB& tileBounds, bool bForShadows)
{
	float zmin = (float)gv::WORLD_BOUNDS_MIN_Z;
	float zmax = (float)gv::WORLD_BOUNDS_MAX_Z;

	// TODO -- use cached height map min/max always?
	if (bForShadows)
	{
		const float cx = (g_tileBoundsCropMaxX + g_tileBoundsCropMinX)/2.0f;
		const float cy = (g_tileBoundsCropMaxY + g_tileBoundsCropMinY)/2.0f;
		const float ex = (g_tileBoundsCropMaxX - g_tileBoundsCropMinX)/2.0f;
		const float ey = (g_tileBoundsCropMaxY - g_tileBoundsCropMinY)/2.0f;

		if (g_shadowUseMinHeightHM)
		{
			zmin = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(cx - ex, cy - ey, cx + ex, cy + ey);
		}

		if (g_shadowUseMaxHeightHM)
		{
			zmax = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(cx - ex, cy - ey, cx + ex, cy + ey);
		}
		else if (g_shadowUseMaxHeightCamera)
		{
			zmax = g_cameraPos.GetZf();
		}
	}

	tileBounds = spdAABB(Vec3V(g_tileBoundsMinX, g_tileBoundsMinY, zmin), Vec3V(g_tileBoundsMaxX, g_tileBoundsMaxY, zmax));
}

float TiledScreenCapture::GetSSAOScale()
{
	return g_layers[g_layerIndex].m_SSAOScale;
}

const grcTexture* TiledScreenCapture::GetWaterReflectionTexture()
{
#if __WIN32PC
	static const grcTexture* s_texture = NULL;

	if (s_texture == NULL)
	{
		u32 size = 4;
		grcImage* temp = grcImage::Create(
			size,
			size,
			1, // depth
			grcImage::A32B32G32R32F,
			grcImage::STANDARD,
			0, // extraMipmaps
			0  // extraLayers
		);
		Assert(temp && temp->GetStride() == size*sizeof(Vec4V));

		for (u32 i = 0; i < size*size; i++)
		{
			reinterpret_cast<Vec4V*>(temp->GetBits())[i] = Vec4V(0.0f, 0.0f, 32.0f, 1.0f); // super bright blue
		}

		s_texture = grcTextureFactory::GetInstance().Create(temp);
		Assert(s_texture);
		temp->Release();
	}

	return s_texture;
#else
	return grcTexture::NoneWhite; // good enough for consoles i guess
#endif
}

void TiledScreenCapture::UpdateViewportFrame(grcViewport& vp)
{
	if (g_interlaceEnabled)
	{
		const float shearX = ((float)g_tileCoordI - (float)(g_interlaceW - 1)/2.0f)/(float)g_interlaceW;
		const float shearY = ((float)g_tileCoordJ - (float)(g_interlaceH - 1)/2.0f)/(float)g_interlaceH;

		vp.SetPerspectiveShear(shearX*0.5f/(float)GRCDEVICE.GetWidth(), -shearY*0.5f/(float)GRCDEVICE.GetHeight());
	}
	else if (g_shearUpdate)
	{
		vp.SetPerspectiveShear(g_shearX*0.5f/(float)GRCDEVICE.GetWidth(), -g_shearY*0.5f/(float)GRCDEVICE.GetHeight());
		g_shearUpdate = false;
	}
}

bool TiledScreenCapture::UpdateViewportPerspective(grcViewport& vp, float fov, float aspect, float nearClip, float farClip)
{
	if (g_useOrthoViewport && (g_useOrthoViewportAlways || IsEnabled()))
	{
		vp.Ortho(
			-(g_tileSizeX*g_tileSizeScale/2.0f + g_tileSizeMargin)*aspect,
			+(g_tileSizeX*g_tileSizeScale/2.0f + g_tileSizeMargin)*aspect,
			-(g_tileSizeY*g_tileSizeScale/2.0f + g_tileSizeMargin),
			+(g_tileSizeY*g_tileSizeScale/2.0f + g_tileSizeMargin),
			g_cameraOrthoMinZ,
			g_cameraPos.GetZf() - g_tileBoundsMinZ + g_cameraOrthoMaxZOffset,
			g_useOrthoViewportZeroFOV
		);

		if (g_useOrthoViewportAmount < 1.0f) // lerp between perspective and ortho projections
		{
			const ScalarV amount(powf(g_useOrthoViewportAmount, 1.0f/5.0f));
			const Mat44V proj1 = vp.GetProjection(); // ortho projection

			vp.Perspective(fov, aspect, nearClip, farClip);

			const Mat44V proj2 = vp.GetProjection(); // perspective projection
			const Mat44V projInterpolated(
				proj2.GetCol0() + (proj1.GetCol0() - proj2.GetCol0())*amount,
				proj2.GetCol1() + (proj1.GetCol1() - proj2.GetCol1())*amount,
				proj2.GetCol2() + (proj1.GetCol2() - proj2.GetCol2())*amount,
				proj2.GetCol3() + (proj1.GetCol3() - proj2.GetCol3())*amount
			);

			vp.SetProjection(projInterpolated);
		}

		return false; // don't set perspective viewport - we're done, don't touch the viewport now
	}
	else
	{
		return true; // set perspective viewport and whatever else CViewport::PropagateCameraFrame wants to do
	}
}

void TiledScreenCapture::Update()
{
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_PAUSE, KEYBOARD_MODE_DEBUG))
	{
		g_stateStopped = !g_stateStopped;
	}

	if (!IsEnabled() && !g_interlaceEnabled)
	{
		return;
	}

	grcDebugDraw::AddDebugOutput("tile:(%d,%d)", g_tileCoordI, g_tileCoordJ);

	if (g_stateDetachCamera)
	{
		g_tileCoordI = 0;
		g_tileCoordJ = 0;
		g_stateEnabled = false;
		g_state = STATE_OFF;

		UpdateEnabled();

		g_stateDetachCamera = false; // turn this off last, to prevent free camera from being disabled
		return;
	}

	CPed* pPlayerPed = FindPlayerPed();

	if (pPlayerPed)
	{
		g_playerPos = pPlayerPed->GetMatrix().GetCol3();
	}

	if (g_LODScale > 1.0f)
	{
		g_LodScale.SetGlobalScaleFromScript(g_LODScale); // override LOD scale for one frame
	}

	bool bPlayerUpdated = false;
	bool bCameraUpdated = false;

	g_stateCurrentTime = fwTimer::GetSystemTimeInMilliseconds();

	if (g_skipToNext)
	{
		g_state = STATE_ADVANCE_CAMERA;
		g_stateTimer = g_stateCurrentTime;
		g_stateFrameDelay = 0;
		g_skipToNext = false;
	}

	if (g_stateStopped)
	{
		// do nothing
	}
	else if (g_stateTimer > g_stateCurrentTime)
	{
		if (g_verboseTimer)
		{
			Displayf("waiting for timer @%d (-%d ms) ..", g_stateTimer, g_stateTimer - g_stateCurrentTime);
		}
	}
	else if (g_stateFrameDelay > 0)
	{
		if (g_verboseTimer)
		{
			Displayf("waiting for %d frames ..", g_stateFrameDelay);
		}

		g_stateFrameDelay--;
	}
	else if (g_state == STATE_STARTING || g_state == STATE_ADVANCE_CAMERA)
	{
		if (g_interlaceEnabled)
		{
			if (g_state == STATE_ADVANCE_CAMERA)
			{
				g_tileCoordI++;

				if (g_tileCoordI >= g_interlaceW)
				{
					g_tileCoordI = 0;
					g_tileCoordJ++;

					if (g_tileCoordJ >= g_interlaceH)
					{
						g_tileCoordI = 0;
						g_tileCoordJ = 0;
						g_stateEnabled = false;
						g_state = STATE_OFF;

						g_interlaceEnabled = false;

						UpdateEnabled();
						return;
					}
				}
			}

			SetState(STATE_WAITING_FOR_RENDERING, 0, 5); // wait 5 frames so image is stable, we probably could wait only 2 or 3 frames ..
		}
		else
		{
			if (g_state == STATE_ADVANCE_CAMERA)
			{
				g_tileCoordJ += g_tileCoordStep;
			}

			UpdateTileBounds();

			while (1)
			{
			while (g_tileBoundsCropMaxX <= g_captureBoundsMinX) { g_tileCoordI += g_tileCoordStep; UpdateTileBounds(); }
			while (g_tileBoundsCropMaxY <= g_captureBoundsMinY) { g_tileCoordJ += g_tileCoordStep; UpdateTileBounds(); }

			if (g_tileBoundsCropMinY >= g_captureBoundsMaxY)
			{
				g_tileCoordI += g_tileCoordStep;
				g_tileCoordJ = 0;
				UpdateTileBounds();

				if (g_tileBoundsCropMinX >= g_captureBoundsMaxX)
				{
					g_tileCoordI = 0;
					g_tileCoordJ = 0;
					g_stateEnabled = false;
					g_state = STATE_OFF;

					UpdateEnabled();
					return;
				}
				else
				{
					while (g_tileBoundsCropMaxY <= g_captureBoundsMinY) { g_tileCoordJ += g_tileCoordStep; UpdateTileBounds(); }
				}
			}

				if (g_state == STATE_ADVANCE_CAMERA)
				{
					if (TileExists(g_tileCoordI, g_tileCoordJ))
					{
						g_tileCoordJ += g_tileCoordStep;
						UpdateTileBounds();
						continue;
					}
				}

				break;
			}

			UpdatePlayer(pPlayerPed);
			bPlayerUpdated = true;

			UpdateCamera();
			bCameraUpdated = true;

			SetState(STATE_WAITING_FOR_STREAMING, g_advanceTimeDelay, g_advanceFrameDelay);
		}
	}
	else if (g_state == STATE_WAITING_FOR_STREAMING)
	{
		bool bReady = true;

		if (g_waitForStreaming && strStreamingEngine::GetInfo().GetNumberRealObjectsRequested() > 0)
		{
			bReady = false;

			if (g_verboseWaiting)
			{
				Displayf("waiting for streaming (%d requests pending) ..", strStreamingEngine::GetInfo().GetNumberRealObjectsRequested());
			}
		}

		if (g_waitForEntitiesLoaded && !AreAllEntitiesLoaded())
		{
			bReady = false;

			if (g_verboseWaiting)
			{
				if (g_allIMAPsAreLoaded)
				{
					Displayf("waiting for entities to be loaded (%d not loaded) ..", g_numEntitiesNotLoaded);
				}
				else
				{
					Displayf("some IMAPs are not loaded yet ..");
				}
			}
		}

		if (bReady)
		{
			SetState(STATE_WAITING_FOR_RENDERING, g_renderTimeDelay, g_renderFrameDelay);
		}
	}
	else if (g_state == STATE_WAITING_FOR_RENDERING)
	{
		SetState(STATE_CAPTURE_START, 0, 0);
	}
	else if (g_state == STATE_CAPTURE_START)
	{
		Assertf(0, "STATE_CAPTURE_START wasn't picked up by BuildDrawList");
	}
	else if (g_state == STATE_CAPTURING)
	{
		if (g_verboseTimer)
		{
			Displayf("capturing ..");
		}
	}
	else if (g_state == STATE_ADVANCE_LAYER)
	{
		do { g_layerIndex++; } while (g_layerIndex < TILED_SCREEN_CAPTURE_MAX_LAYERS && !g_layers[g_layerIndex].m_enabled);

		if (g_layerIndex < TILED_SCREEN_CAPTURE_MAX_LAYERS)
		{
			SetState(STATE_WAITING_FOR_RENDERING, g_renderTimeDelay, g_renderFrameDelay);
		}
		else
		{
			g_layerIndex = 0;
			SetState(STATE_ADVANCE_CAMERA, g_renderTimeDelay, g_renderFrameDelay);
		}

		UpdateLayer(&g_layers[g_layerIndex]);
	}

	if (g_cameraForceUpdate && !bCameraUpdated)
	{
		UpdateCamera();
		g_cameraForceUpdate = false;
	}
}

void TiledScreenCapture::BuildDrawList()
{
	const bool bEnabled = IsEnabled();

	// we need to disable edge culling if we're capturing, and re-enable it once we're done capturing
#if __PS3 && SPU_GCM_FIFO && ENABLE_EDGE_CULL_DEBUGGING
	static bool s_wasEdgeDisabled = false;
	u32 flags = 0;

	if (bEnabled && !s_wasEdgeDisabled)
	{
		flags = EDGE_CULL_DEBUG_ENABLED; // enable debug settings with no other flags, i.e. don't do any culling
	}
	else if (!(bEnabled && s_wasEdgeDisabled))
	{
		flags = EDGE_CULL_DEBUG_DEFAULT; // back to default settings
	}

	if (flags)
	{
		class SetEdgeCullDebugFlags { public: static void func(u32 flags)
		{
			SPU_COMMAND(grcGeometryJobs__SetEdgeCullDebugFlags, flags);
			cmd->flags = (u16)flags;
		}};
		DLC_Add(SetEdgeCullDebugFlags::func, flags);
	}

	s_wasEdgeDisabled = bEnabled;
#endif // __PS3 && SPU_GCM_FIFO && ENABLE_EDGE_CULL_DEBUGGING

	if (bEnabled || g_interlaceEnabled)
	{
		class Render { public: static void func(int tileCoordI, int tileCoordJ, int layerIndex, int nextState)
		{
			const grcRenderTarget* screenRt = WIN32PC_SWITCH(NULL, grcTextureFactory::GetInstance().GetFrontBuffer());

			if (g_interlaceEnabled)
			{
				const bool bInterlaceFirst = (tileCoordI == 0 && tileCoordJ == 0);
				const bool bInterlaceLast = (tileCoordI == g_interlaceW - 1 && tileCoordJ == g_interlaceH - 1);

				CaptureTile(atVarString("%s_%03d_screen.dds", g_interlacePath, g_interlaceIndex).c_str(), screenRt, 1, CAPTURE_TYPE_RGB, bInterlaceFirst, bInterlaceLast);
			}
			else
			{
				const char* layerName = g_layers[layerIndex].m_name;

				if (g_saveScreenImage         ) { CaptureTile(TileGetFileName(tileCoordI, tileCoordJ, layerName ).c_str(), screenRt, g_downsampleImage, CAPTURE_TYPE_RGB); }
				if (g_saveDepthImage          ) { CaptureTile(TileGetFileName(tileCoordI, tileCoordJ, "depth"   ).c_str(), GBuffer::GetDepthTarget(), g_downsampleImage, CAPTURE_TYPE_DEPTH); }
				if (g_saveGBufferAlbedoImage  ) { CaptureTile(TileGetFileName(tileCoordI, tileCoordJ, "albedo"  ).c_str(), GBuffer::GetTarget(GBUFFER_RT_0), g_downsampleImage, CAPTURE_TYPE_RGB); }
				if (g_saveGBufferNormalImage  ) { CaptureTile(TileGetFileName(tileCoordI, tileCoordJ, "normal"  ).c_str(), GBuffer::GetTarget(GBUFFER_RT_1), g_downsampleImage, CAPTURE_TYPE_RGB); }
				if (g_saveGBufferSpecularImage) { CaptureTile(TileGetFileName(tileCoordI, tileCoordJ, "specular").c_str(), GBuffer::GetTarget(GBUFFER_RT_2), g_downsampleImage, CAPTURE_TYPE_RGB); }
				if (g_saveGBufferAmbientImage ) { CaptureTile(TileGetFileName(tileCoordI, tileCoordJ, "ambient" ).c_str(), GBuffer::GetTarget(GBUFFER_RT_3), g_downsampleImage, CAPTURE_TYPE_RGB); }
			}

			if (g_verbose)
			{
				Displayf("### [%d,%d] capturing", tileCoordI, tileCoordJ);
			}

			if (nextState != -1)
			{
				SetState(nextState, 0, g_renderFrameDelay2); // signal to update thread we're done
			}
		}};

		if (g_captureOnce)
		{
			DLC_Add(Render::func, g_tileCoordI, g_tileCoordJ, g_layerIndex, -1);
			g_captureOnce = false;
		}
		else if (g_state == STATE_CAPTURE_START)
		{
			// NOTE -- if we don't have multiple layers we could just set nextState = STATE_ADVANCE_CAMERA
			const int nextState = STATE_ADVANCE_LAYER;

#if 0 // for some reason i'm having problems with this .. seems to get stuck in the CAPTURING state when i leave the pc running overnight ..
			DLC_Add(Render::func, g_tileCoordI, g_tileCoordJ, g_layerIndex, (int)nextState);
			SetState(STATE_CAPTURING, 0, 0);
#else
			DLC_Add(Render::func, g_tileCoordI, g_tileCoordJ, g_layerIndex, -1);
			SetState(nextState, 0, g_renderFrameDelay2);
#endif
		}
	}
}

// ================================================================================================

// note: element pointed to by ptr must not cross a 16-byte boundary
template <typename T> static __forceinline T TiledScreenCapture::GetFromLocalMemory(const T* ptr, int i)
{
#if __XENON || __PS3
	const u32 addr = reinterpret_cast<const u32>(ptr + i);
	const u32 base = addr & ~15; // aligned

	ALIGNAS(16) u8 temp[16] ;

#if __XENON
	*reinterpret_cast<Vec::Vector_4V_uint*>(temp) = __lvx(reinterpret_cast<const u32*>(base), 0);
#else
	*reinterpret_cast<Vec::Vector_4V_uint*>(temp) = vec_ld(0, reinterpret_cast<const u32*>(base));
#endif
	return *reinterpret_cast<const T*>(temp + (addr & 15));
#else
	return ptr[i];	
#endif
}

static __forceinline int TiledScreenCapture::GetLocalMemoryAddress(int x, int y, int w, int pitch)
{
#if __XENON
	UNUSED_VAR(pitch);
	Assertf(pitch == w*(int)sizeof(u32), "grcTexture::LockRect pitch = %d, expected %d", pitch, w*sizeof(u32));
	return XGAddress2DTiledOffset(x, y, w, sizeof(u32));
#else
	UNUSED_VAR(w);
	return x + y*(pitch/sizeof(u32));
#endif
}

#if __XENON
static __forceinline float TiledScreenCapture::ConvertD24F(u32 raw)
{
	// assume raw is 20e4, so we want to compute 1.mmmmmmmmmmmmmmmmmmmm * 2^(e - 16)
	// see "Floating-Point Depth Buffers" in 360 docs

	const u32 e = raw >> 20;
	const u32 m = raw & 0x000fffff;

	return (1.0f + (float)m/(float)0x00100000)/(float)(1<<(16 - e));
}
#endif // __XENON

static void TiledScreenCapture::SetState(int state, u32 timeDelay, u32 frameDelay)
{
	const int prevState = g_state;

	if (g_state == STATE_OFF)
	{
		return; // we might have turned the system off while the renderthread was changing the state
	}

	g_state = state;
	g_stateTimer = g_stateCurrentTime + timeDelay;
	g_stateFrameDelay = frameDelay;

	if (g_verbose)
	{
		char msg[512] = "";
		sprintf(msg, "### [%d,%d] @%d %s -> %s layer %d", g_tileCoordI, g_tileCoordJ, g_stateCurrentTime, g_stateStrings[prevState], g_stateStrings[state], g_layerIndex);

		if (timeDelay > 0)
		{
			strcat(msg, atVarString(", timer set @%d (+%d ms)", g_stateTimer, timeDelay).c_str());
		}

		if (frameDelay> 0)
		{
			strcat(msg, atVarString(", waiting for %d frames", frameDelay).c_str());
		}

		Displayf("%s", msg);
	}
}

static bool TiledScreenCapture::AreAllEntitiesLoaded()
{
	g_numEntitiesNotLoaded = 0;

	class EntityIntersectingCB { public: static void func(fwEntity* pEntity_, void*)
	{
		if (pEntity_)
		{
			const CEntity* pEntity = static_cast<const CEntity*>(pEntity_);

			const bool bHasDrawable = pEntity->GetDrawable() != NULL;
			const bool bIsModelLoaded = pEntity->GetBaseModelInfo() && pEntity->GetBaseModelInfo()->GetHasLoaded();

			if (!bHasDrawable || !bIsModelLoaded)
			{
				g_numEntitiesNotLoaded++;

				if (g_reportEntitiesNotLoaded)
				{
					if (!bHasDrawable && !bIsModelLoaded)
					{
						Displayf("> %s has no drawable and is not loaded", pEntity->GetModelName());
					}
					else if (!bHasDrawable)
					{
						Displayf("> %s has no drawable", pEntity->GetModelName());
					}
					else if (!bIsModelLoaded)
					{
						Displayf("> %s is not loaded", pEntity->GetModelName());
					}
				}
			}
		}
	}};

	spdAABB tileBounds;
	GetTileBounds(tileBounds);
	fwIsBoxIntersectingBB testVolume(tileBounds);

	CGameWorld::ForAllEntitiesIntersecting(
		&testVolume,
		IntersectingCB(EntityIntersectingCB::func),
		NULL,
		ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING | ENTITY_TYPE_MASK_OBJECT | ENTITY_TYPE_MASK_DUMMY_OBJECT,
		SEARCH_LOCATION_EXTERIORS,
		SEARCH_LODTYPE_ALL//SEARCH_LODTYPE_HIGHDETAIL
	);

	GetTileBounds(tileBounds);
	g_allIMAPsAreLoaded = fwMapDataStore::GetStore().GetBoxStreamer().HasLoadedWithinAABB(tileBounds, fwBoxStreamerAsset::MASK_MAPDATA);

	if (g_reportEntitiesNotLoaded && !g_allIMAPsAreLoaded)
	{
		Displayf("> some IMAPs are not loaded yet");
	}

	g_reportEntitiesNotLoaded = false;

	return g_numEntitiesNotLoaded == 0 && g_allIMAPsAreLoaded;
}

static atString TiledScreenCapture::TileGetFileName(int coordI, int coordJ, const char* layerName)
{
	char path[256] = "";
	strcpy(path, g_tilesPath);

	if (g_chunkIndexI != -1 &&
		g_chunkIndexJ != -1)
	{
		strcat(path, atVarString("_%d_%d", g_chunkIndexI, g_chunkIndexJ).c_str());

		if (g_chunkExtentX != 1 ||
			g_chunkExtentY != 1)
		{
			strcat(path, atVarString("_%d_%d", g_chunkExtentX, g_chunkExtentY).c_str());
		}
	}

	return atVarString("%s/tile_%03d_%03d_%s.dds", path, coordI, coordJ, layerName);
}

static bool TiledScreenCapture::TileExists(int coordI, int coordJ)
{
	if (g_tileSkipExistingImages)
	{
		const char* lastLayerName = NULL;

		for (int layerIndex = TILED_SCREEN_CAPTURE_MAX_LAYERS - 1; layerIndex >= 0; layerIndex--)
		{
			if (g_layers[layerIndex].m_enabled)
			{
				lastLayerName = g_layers[layerIndex].m_name;
				break;
			}
		}

		if (lastLayerName)
		{
			fiStream* f = fiStream::Open(TileGetFileName(coordI, coordJ, lastLayerName).c_str());

			if (f)
			{
				f->Close();
				return true;
			}
		}
	}

	return false;
}

static void TiledScreenCapture::UpdateEnabled()
{
	static bool s_wasEnabled = false;

	if (g_interlaceEnabled)
	{
		return; // interlaced screen shots use all the "normal" graphics features
	}

	if (IsEnabled())
	{
		Displayf("TiledScreenCapture::UpdateEnabled - enabling screen capture modes");

		// activate free camera
		camInterface::GetDebugDirector().ActivateFreeCam();

		// disable LOD fading
		CLodDebugMisc::SetAllowTimeBasedFade(false);

		// disable occlusion
		fwScan::ms_forceDisableOcclusion = true;

		// disable cables (for now .. eventually should fix this)
		CCustomShaderEffectCable::SetEnable(false);

		// disable ped and vehicle spawning
		CPedPopulation::StopSpawningPeds();
		CVehiclePopulation::StopSpawningVehs();

		// disable fog and haze
#if TC_DEBUG
		g_timeCycle.GetGameDebug().SetDisableFogAndHaze(true);
#endif // TC_DEBUG

		// quadrant tracker (in case we decide to enable shadows)
		CRenderPhaseCascadeShadowsInterface::Script_SetShadowType(CSM_TYPE_QUADRANT);

		// hacky way to force PPU visibility codepath on ps3 (shadow visibility for ortho view is only "supported" on PPU)
		g_scanDebugFlagsPortals |= SCAN_PORTALS_DEBUG_DISPLAY_PORTALS_FROM_STARTING_NODE_ONLY;

		// disable tiled rendering
		DebugDeferred::m_enableTiledRendering = false;

		CPed* pPlayerPed = FindPlayerPed();

		if (pPlayerPed)
		{
			// hide player
			pPlayerPed->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
			pPlayerPed->GetRenderPhaseVisibilityMask().ChangeFlag(VIS_PHASE_MASK_GBUF, false);
			pPlayerPed->GetRenderPhaseVisibilityMask().ChangeFlag(VIS_PHASE_MASK_SHADOWS, false);
			pPlayerPed->RequestUpdateInWorld();
		}

		// pause
		fwTimer::SetKey_Pause(0); // disable pausing with the PAUSE key, i'm hijacking it ..
		fwTimer::StartUserPause();

		UpdateLayer(&g_layers[g_layerIndex]);

		if (!s_wasEnabled)
		{
			if (g_startupRemovePedsAndVehs)
			{
				CPedPopulation::RemoveAllPeds(CPedPopulation::PMR_ForceAllRandomAndMission);
				CVehiclePopulation::RemoveAllVehsHard();
			}

			if (g_startupStreamingFlush)
			{
				CStreaming::GetCleanup().RequestFlush(false); // flush all peds and vehicles out
			}

			s_wasEnabled = true;
		}
	}
	else if (s_wasEnabled)
	{
		Displayf("TiledScreenCapture::UpdateEnabled - disabling");

		if (!g_stateDetachCamera)
		{
			// deactivate free camera
			camInterface::GetDebugDirector().DeactivateFreeCam();
		}

		// enable LOD fading
		CLodDebugMisc::SetAllowTimeBasedFade(true);

		// enable occlusion
		fwScan::ms_forceDisableOcclusion = false;

		// enable cables
		CCustomShaderEffectCable::SetEnable(true);

		// enable ped and vehicle spawning
		CPedPopulation::StartSpawningPeds();
		CVehiclePopulation::StartSpawningVehs();

#if TC_DEBUG
		// enable fog and haze
		g_timeCycle.GetGameDebug().SetDisableFogAndHaze(false);
#endif // TC_DEBUG

		CRenderPhaseCascadeShadowsInterface::Script_SetShadowType(CSM_TYPE_CASCADES);

		// remove hack ..
		g_scanDebugFlagsPortals &= ~SCAN_PORTALS_DEBUG_DISPLAY_PORTALS_FROM_STARTING_NODE_ONLY;

		// enable tiled lighting (disable orthographic)
		DebugDeferred::m_enableTiledRendering = true;

		CPed* pPlayerPed = FindPlayerPed();

		if (pPlayerPed)
		{
			// show player
			pPlayerPed->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
			pPlayerPed->GetRenderPhaseVisibilityMask().ChangeFlag(VIS_PHASE_MASK_GBUF, true);
			pPlayerPed->GetRenderPhaseVisibilityMask().ChangeFlag(VIS_PHASE_MASK_SHADOWS, true);
			pPlayerPed->RequestUpdateInWorld();
		}

		// resume
		fwTimer::SetKey_Pause(KEY_PAUSE);
		fwTimer::EndUserPause();

		UpdateLayer(NULL);

		s_wasEnabled = false;
	}
}

__COMMENT(static) void TiledScreenCapture::CTSCLayer::Update_cb()
{
	if (IsEnabled())
	{
		UpdateLayer(&g_layers[g_layerIndex]);
	}
}

static void TiledScreenCapture::UpdateLayer(const CTSCLayer* sd)
{
	if (sd)
	{
		if (sd->m_drawLODs)
		{
			// switch to LOD only
			CPostScanDebug::GetDrawByLodLevelRef() = true;
			CPostScanDebug::GetSelectedLodLevelRef() = 1; // LOD
			g_LodMgr.StartSlodMode(LODTYPES_FLAG_LOD, CLodMgr::LODFILTER_MODE_DEBUG);
		}
		else
		{
			// switch to HD only
			CPostScanDebug::GetDrawByLodLevelRef() = true;
			CPostScanDebug::GetSelectedLodLevelRef() = 0; // HD and ORPHANHD
			g_LodMgr.StartSlodMode(LODTYPES_FLAG_HD | LODTYPES_FLAG_ORPHANHD, CLodMgr::LODFILTER_MODE_DEBUG);
		}

		DebugDeferred::m_GBufferIndex = sd->m_gbufferIndex;

		CEntity::ms_cullProps = !sd->m_enableProps;

		if (sd->m_enableTrees)
		{
			fwRenderListBuilder::GetRenderPassDisableBits() &= ~BIT(RPASS_TREE);
		}
		else
		{
			fwRenderListBuilder::GetRenderPassDisableBits() |= BIT(RPASS_TREE);
		}

		if (sd->m_enableVfx)
		{
			CVisualEffects::EnableAllVisualEffects();
		}
		else
		{
			CVisualEffects::DisableAllVisualEffects();
		}

#if WATER_SHADER_DEBUG
		Water::m_debugHighlightAmount = 1.0f;
#endif // WATER_SHADER_DEBUG

		if (sd->m_enableAmbientOnly)
		{
			DebugDeferred::m_overrideShadow = true;
			DebugDeferred::m_overrideShadow_intensity = 0.0f;
			DebugDeferred::m_overrideAmbient = true;
			DebugDeferred::m_overrideAmbientParams.x = 1.0f;
		}
		else if (sd->m_enableShadows)
		{
			// enable shadows (quadrant tracker)
			DebugDeferred::m_overrideShadow = false;
			DebugDeferred::m_overrideShadow_intensity = 1.0f;
			DebugDeferred::m_overrideAmbient = false;
			DebugDeferred::m_overrideAmbientParams.x = 1.0f;
		}
		else
		{
			// disable shadows
			DebugDeferred::m_overrideShadow = true;
			DebugDeferred::m_overrideShadow_intensity = 1.0f;
			DebugDeferred::m_overrideAmbient = false;
			DebugDeferred::m_overrideAmbientParams.x = 1.0f;
		}

		if (sd->m_enableSpecular)
		{
			DebugDeferred::m_overrideSpecular = false;
			DebugDeferred::m_overrideSpecularParams = Vector3(1.0f, 512.0f, 0.96f);
		}
		else
		{
			DebugDeferred::m_overrideSpecular = true;
			DebugDeferred::m_overrideSpecularParams = Vector3(0.0f, 512.0f, 0.96f);
		}

		// ssao overrides
		SSAO::SetEnable(sd->m_enableSSAO);
		SSAO::SetIsolate(sd->m_SSAOIsolate);

		if (sd->m_SSAOTechniqueOverride)
		{
			SSAO::SetOverrideTechnique((ssao_technique)sd->m_SSAOTechnique);
		}
		else
		{
			SSAO::DisableOverrideTechnique();
		}

		if (sd->m_SSAOIntensity > 0.0f)
		{
			SSAO::SetOverrideIntensity(sd->m_SSAOIntensity);
		}
		else
		{
			SSAO::DisableOverrideIntensity();
		}

		CClock::SetTime(sd->m_night ? 0 : 12, 0, 0);

		g_coronas.GetSizeScaleGlobalRef() = sd->m_coronaScale;

		PostFX::GetSimplePfxRef() = sd->m_simplePostFx;

		CRenderPhaseDebugOverlayInterface::StencilMaskOverlaySetWaterMode(sd->m_waterOverlay, sd->m_waterOnly, Color32(0,255,0,255));

		g_useOrthoViewport = !sd->m_nonOrthographic;
	}
	else
	{
		// switch back to default LOD selection
		CPostScanDebug::GetDrawByLodLevelRef() = false;
		CPostScanDebug::GetSelectedLodLevelRef() = 0;
		g_LodMgr.StopSlodMode();

		// disable gbuffer override
		DebugDeferred::m_GBufferIndex = 0;

		// disable cull props
		CEntity::ms_cullProps = false;

		// enable trees
		fwRenderListBuilder::GetRenderPassDisableBits() &= ~BIT(RPASS_TREE);

		// enable vfx
		CVisualEffects::EnableAllVisualEffects();

#if WATER_SHADER_DEBUG
		Water::m_debugHighlightAmount = 0.0f;
#endif // WATER_SHADER_DEBUG

		// disable overrides for shadows and ambient
		DebugDeferred::m_overrideShadow = false;
		DebugDeferred::m_overrideShadow_intensity = 1.0f;
		DebugDeferred::m_overrideAmbient = false;
		DebugDeferred::m_overrideAmbientParams.x = 1.0f;

		// enable specular
		DebugDeferred::m_overrideSpecular = false;
		DebugDeferred::m_overrideSpecularParams = Vector3(1.0f, 512.0f, 0.96f);

		// disable ssao overrides
		SSAO::SetEnable(true);
		SSAO::SetIsolate(false);
		SSAO::DisableOverrideTechnique();
		SSAO::DisableOverrideIntensity();

		CClock::SetTime(12, 0, 0);

		g_coronas.GetSizeScaleGlobalRef() = 1.0f;

		PostFX::GetSimplePfxRef() = false;

		CRenderPhaseDebugOverlayInterface::StencilMaskOverlaySetWaterMode(false, false, Color32(0));

		g_useOrthoViewport = __WIN32PC ? true : false;
	}
}

static void TiledScreenCapture::UpdateTileBounds()
{
	// physical bounds of tile image we're capturing
	g_tileBoundsCropMinX = g_worldBoundsMinX + g_tileSizeX*g_tileSizeScale*(float)(g_tileCoordI/g_tileCoordStep + 0);
	g_tileBoundsCropMinY = g_worldBoundsMinY + g_tileSizeY*g_tileSizeScale*(float)(g_tileCoordJ/g_tileCoordStep + 0);
	g_tileBoundsCropMaxX = g_worldBoundsMinX + g_tileSizeX*g_tileSizeScale*(float)(g_tileCoordI/g_tileCoordStep + 1);
	g_tileBoundsCropMaxY = g_worldBoundsMinY + g_tileSizeY*g_tileSizeScale*(float)(g_tileCoordJ/g_tileCoordStep + 1);

	const float cx = (g_tileBoundsCropMaxX + g_tileBoundsCropMinX)/2.0f;
	const float cy = (g_tileBoundsCropMaxY + g_tileBoundsCropMinY)/2.0f;
	const float ex = (g_tileBoundsCropMaxX - g_tileBoundsCropMinX)/2.0f;
	const float ey = (g_tileBoundsCropMaxY - g_tileBoundsCropMinY)/2.0f;

	// physical bounds of tile that we're rendering (expanded by margin to account for SSAO, bloom etc.)
	g_tileBoundsMinX = cx - (ex + g_tileSizeMargin);
	g_tileBoundsMinY = cy - (ey + g_tileSizeMargin);
	g_tileBoundsMinZ = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(cx - ex, cy - ey, cx + ex, cy + ey);
	g_tileBoundsMaxX = cx + (ex + g_tileSizeMargin);
	g_tileBoundsMaxY = cy + (ey + g_tileSizeMargin);
	g_tileBoundsMaxZ = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(cx - ex, cy - ey, cx + ex, cy + ey);
}

static void TiledScreenCapture::UpdatePlayer(CPed* pPlayerPed)
{
	if (pPlayerPed)
	{
		const float x = (g_tileBoundsMaxX + g_tileBoundsMinX)/2.0f;
		const float y = (g_tileBoundsMaxY + g_tileBoundsMinY)/2.0f;
		const float z = g_tileBoundsMinZ + g_playerHeightOffset; // just below the heightmap

		g_warpedPos = Vec3V(x, y, z);

		pPlayerPed->Teleport(RCC_VECTOR3(g_warpedPos), 0.0f);
	}
}

static void TiledScreenCapture::UpdateCameraPosAndFov()
{
	if (camInterface::GetDebugDirector().IsFreeCamActive())
	{
		camFrame& cam = camInterface::GetDebugDirector().GetFreeCamFrameNonConst();

		cam.SetPosition(RCC_VECTOR3(g_cameraPos));
		cam.SetFov(g_cameraFov);
		cam.SetNearClip(g_cameraNearClip);
		cam.SetFarClip(g_cameraFarClip);
	}
}

static void TiledScreenCapture::UpdateCamera()
{
	if (camInterface::GetDebugDirector().IsFreeCamActive())
	{
		camFrame& cam = camInterface::GetDebugDirector().GetFreeCamFrameNonConst();

		const Vec3V camFront = -Vec3V(V_Z_AXIS_WZERO);
		const Vec3V camUp    = +Vec3V(V_Y_AXIS_WZERO);
		const float camPosX  = (g_tileBoundsMaxX + g_tileBoundsMinX)/2.0f;
		const float camPosY  = (g_tileBoundsMaxY + g_tileBoundsMinY)/2.0f;
		const float camPosZ  = g_cameraHeightUseHeightmap ? (g_tileBoundsMaxZ + g_cameraHeightOffset) : g_cameraHeight;
		const Vec3V camPos   = Vec3V(camPosX, camPosY, camPosZ);
		const float camFov   = Max<float>(1.0f, RtoD*2.0f*atan2f(g_tileSizeY*g_tileSizeScale/2.0f, g_cameraHeight)*g_cameraFovScale);

		g_cameraFov = camFov;
		g_cameraPos = camPos;

		const Mat34V camMtx(Vec3V(V_X_AXIS_WZERO), camFront, camUp, camPos);

		cam.SetWorldMatrix(RCC_MATRIX34(camMtx), false); // set both orientation and position
		cam.SetFov(g_cameraFov);
		cam.SetNearClip(g_cameraNearClip);
		cam.SetFarClip(g_cameraFarClip);
	}
}

static void TiledScreenCapture::CaptureTile(const char* path, const grcRenderTarget* rt, int downsample, int captureType, bool bInterlaceFirst, bool bInterlaceLast)
{
	grcImage* pScreenImage = NULL;

	if (rt == NULL) // rendertarget is NULL, try getting screen from device (we can't simply lock the frontbuffer on pc)
	{
#if __WIN32PC
		pScreenImage = GRCDEVICE.CaptureScreenShot(NULL);

		if (!AssertVerify(pScreenImage))
#endif // __WIN32PC
		{
			return;
		}
	}

	const int srcW = rt ? rt->GetWidth()  : pScreenImage->GetWidth();
	const int srcH = rt ? rt->GetHeight() : pScreenImage->GetHeight();

	const int cropH = (int)((float)srcH*(g_tileSizeY*g_tileSizeScale)/(g_tileSizeY*g_tileSizeScale + g_tileSizeMargin*2));
	const int cropW = cropH; // assume srcW >= srcH

	const int i0 = Max<int>(0, srcW - cropW)/2;
	const int j0 = Max<int>(0, srcH - cropH)/2;
	const int i1 = Min<int>(i0 + cropW, srcW);
	const int j1 = Min<int>(j0 + cropH, srcH);

	const int dstW = (i1 - i0)/downsample;
	const int dstH = (j1 - j0)/downsample;

	grcTextureLock lock;
	sysMemSet(&lock, 0, sizeof(lock)); // clear to zero in case we're using pScreenImage instead ..

	if (rt == NULL || AssertVerify(rt->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock)))
	{
		static grcImage* s_image_A8R8G8B8 = NULL;
		static grcImage* s_image_L8 = NULL;
	//	static grcImage* s_image_R32F = NULL;

		if (captureType == CAPTURE_TYPE_RGB)
		{
			grcImage*& pImage = s_image_A8R8G8B8;

			if (pImage && (pImage->GetWidth() != dstW || pImage->GetHeight() != dstH))
			{
				pImage->Release();
				pImage = NULL;
			}

			if (pImage == NULL)
			{
				pImage = grcImage::Create(dstW, dstH, 1, grcImage::A8R8G8B8, grcImage::STANDARD, 0, 0);
			}

			if (AssertVerify(pImage))
			{
				Color32* dst = (Color32*)pImage->GetBits();

				for (int j = 0; j < dstH; j++)
				{
					for (int i = 0; i < dstW; i++)
					{
						Vec4V sum(V_ZERO);
						bool bGreenMask = false;

						for (int jj = 0; jj < downsample; jj++)
						{
							for (int ii = 0; ii < downsample; ii++)
							{
								const int x = i0 + ii + i*downsample;
								const int y = j0 + jj + j*downsample;

								if (pScreenImage)
								{
									const Color32 c = Color32(pScreenImage->GetPixel(x, y));

									sum += c.GetRGBA();

									if (c.GetRed() == 0 && c.GetGreen() == 255 && c.GetBlue() == 0)
									{
										bGreenMask = true;
								}
								}
								else
								{
									const int addr = GetLocalMemoryAddress(x, y, srcW, lock.Pitch);
									const Color32 c = GetFromLocalMemory((const Color32*)lock.Base, addr);

									sum += c.GetRGBA().Get<Vec::Z,Vec::Y,Vec::X,Vec::W>();

									if (c.GetRed() == 0 && c.GetGreen() == 255 && c.GetBlue() == 0)
									{
										bGreenMask = true;
									}
								}
							}
						}

						if (bGreenMask)
						{
							dst[i + j*dstW] = Color32(0,255,0,255);
						}
						else
						{
						dst[i + j*dstW] = Color32(sum*ScalarV(1.0f/(float)(downsample*downsample)));
						dst[i + j*dstW].SetAlpha(255);
					}
				}
				}

				if (g_interlaceEnabled)
				{
					static u16* s_img = NULL;
					static int s_imgW = 0;
					static int s_imgH = 0;

					if (s_img && (s_imgW != dstW || s_imgH != dstH))
					{
						delete[] s_img;
						s_img = NULL;
					}

					if (s_img == NULL)
					{
						s_img = rage_new u16[dstW*dstH*3];
						s_imgW = dstW;
						s_imgH = dstH;
					}

					if (AssertVerify(s_img))
					{
						if (bInterlaceFirst)
						{
							sysMemSet(s_img, 0, dstW*dstH*3*sizeof(u16));
						}

						for (int i = 0; i < dstW*dstH; i++)
						{
							s_img[i*3 + 0] += (u16)dst[i].GetRed();
							s_img[i*3 + 1] += (u16)dst[i].GetGreen();
							s_img[i*3 + 2] += (u16)dst[i].GetBlue();
						}

						if (bInterlaceLast)
						{
							for (int i = 0; i < dstW*dstH; i++)
							{
								dst[i].SetRed  ((int)(0.5f + (float)s_img[i*3 + 0]/(float)(g_interlaceW*g_interlaceH)));
								dst[i].SetGreen((int)(0.5f + (float)s_img[i*3 + 1]/(float)(g_interlaceW*g_interlaceH)));
								dst[i].SetBlue ((int)(0.5f + (float)s_img[i*3 + 2]/(float)(g_interlaceW*g_interlaceH)));
								dst[i].SetAlpha(255);
							}

							pImage->SaveDDS(path);
						}
					}
				}
				else
				{
					pImage->SaveDDS(path);
				}
			}
		}
		else if (captureType == CAPTURE_TYPE_DEPTH && rt && !g_interlaceEnabled)
		{
		//	grcImage*& pImage = s_image_R32F;
			grcImage*& pImage = s_image_L8;

			if (pImage && (pImage->GetWidth() != dstW || pImage->GetHeight() != dstH))
			{
				pImage->Release();
				pImage = NULL;
			}

			if (pImage == NULL)
			{
			//	pImage = grcImage::Create(dstW, dstH, 1, grcImage::R32F, grcImage::STANDARD, 0, 0);
				pImage = grcImage::Create(dstW, dstH, 1, grcImage::L8, grcImage::STANDARD, 0, 0);
			}

			if (AssertVerify(pImage))
			{
				const grcViewport* vp = gVpMan.GetRenderGameGrcViewport();
			//	float* dst = (float*)pImage->GetBits();
				u8* dst = (u8*)pImage->GetBits();

				for (int j = 0; j < dstH; j++)
				{
					for (int i = 0; i < dstW; i++)
					{
						float sum = 0.0f;

						for (int jj = 0; jj < downsample; jj++)
						{
							for (int ii = 0; ii < downsample; ii++)
							{
								const int x = i0 + ii + i*downsample;
								const int y = j0 + jj + j*downsample;
								const int addr = GetLocalMemoryAddress(x, y, srcW, lock.Pitch);
								const Color32 c = GetFromLocalMemory((const Color32*)lock.Base, addr);
								const u32 r = c.GetColor(); // raw data from depth buffer
								const float sampleDepth = XENON_SWITCH(ConvertD24F(r>>8), (float)(r>>8)/(float)((1<<24) - 1));
								const float linearDepth = ShaderUtil::GetLinearDepth(vp, ScalarV(sampleDepth)).Getf();

								const float minWorldDepth = -10.0f;
								const float maxWorldDepth = (float)gv::WORLD_BOUNDS_MAX_Z;
								const float worldDepth = g_cameraHeight - linearDepth;
								const float scaledDepth = (worldDepth - minWorldDepth)/(maxWorldDepth - minWorldDepth);

								sum += scaledDepth;
							}
						}

					//	dst[i + j*dstW] = sum/(float)(downsample*downsample);
						dst[i + j*dstW] = (u8)Clamp<float>(0.5f + 255.0f*sum/(float)(downsample*downsample), 0.0f, 255.0f);
					}
				}

				pImage->SaveDDS(path);
			}
		}
		else if (captureType == CAPTURE_TYPE_STENCIL && !g_interlaceEnabled)
		{
			// not implemented
		}

		if (rt)
		{
			rt->UnlockRect(lock);
		}
	}

	if (pScreenImage)
	{
		pScreenImage->Release();
	}
}

#endif // __BANK
