//
// filename:	VisualEffects.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

#include "VisualEffects.h"

// Rage headers
#include "bank/bkmgr.h"
#include "grcore/allocscope.h"
#include "physics/contact.h"
#include "profile/Timebars.h"
#include "rmptfx/ptxmanager.h"
#include "system/memory.h"

// Framework headers
#include "fwsys/timer.h"
#include "fwscene/stores/txdstore.h"
#include "vfx/channel.h"
#include "vfx/vfxutil.h"
#include "vfx/vfxwidget.h"
#include "fwScene/scan/Scan.h"

// Game headers
#include "Camera/CamInterface.h"
#include "Camera/Viewports/ViewportManager.h"
#include "Control/replay/replay.h"
#include "Core/Game.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "Debug/DebugScene.h"
#include "debug/TiledScreenCapture.h"
#include "frontend/GolfTrail.h"
#include "frontend/MiniMap.h"
#include "frontend/MobilePhone.h"
#include "frontend/NewHud.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "Game/Weather.h"
#include "Game/Wind.h"
#include "Modelinfo/ModelSeatInfo.h"
#include "Peds/Ped.h"
#include "Physics/GtaMaterialManager.h"
#include "Physics/Physics.h"
#include "Renderer/DrawLists/DrawListMgr.h"
#include "Renderer/lights/lights.h"
#include "Renderer/Lights/LODLights.h"
#include "Renderer/PostProcessFx.h"
#include "Renderer/PostProcessFxHelper.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderThread.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/RenderPhases/RenderPhaseWaterReflection.h"
#if __D3D11 || RSG_ORBIS
#include "renderer/Mirrors.h"
#endif // __D3D11 || RSG_ORBIS
#include "Scene/Physical.h"
#include "Scene/World/GameWorld.h"
#include "Script/Script.h"
#include "Streaming/Streaming.h"
#include "TimeCycle/TimeCycle.h"
#include "TimeCycle/TimeCycleConfig.h"
#include "Vehicles/Heli.h"
#include "vfx/VfxHelper.h"
#include "Vfx/Misc/BrightLights.h"
#include "Vfx/Misc/Checkpoints.h"
#include "Vfx/Misc/Coronas.h"
#include "Vfx/Misc/DistantLights.h"
#include "Vfx/Misc/DistantLightsVertexBuffer.h"
#include "Vfx/Misc/Fire.h"
#include "Vfx/Misc/FogVolume.h"
#include "Vfx/Misc/GameGlows.h"
#include "Vfx/Misc/Markers.h"
#include "Vfx/Misc/MovieManager.h"
#include "Vfx/Misc/MovieMeshManager.h"
#include "Vfx/Misc/ScriptIM.h"
#include "Vfx/Misc/Scrollbars.h"
#include "vfx/Misc/TVPlaylistManager.h"
#include "vfx/Misc/LODLightManager.h"
#include "vfx/Misc/LensFlare.h"
#include "vfx/Misc/TerrainGrid.h"

#include "Vfx/Sky/Sky.h"
#include "Vfx/Metadata/PtFxAssetInfo.h"
#include "Vfx/Metadata/VfxFogVolumeInfo.h"
#include "Vfx/Metadata/VfxInteriorInfo.h"
#include "Vfx/Metadata/VfxRegionInfo.h"
#include "Vfx/Metadata/VfxPedInfo.h"
#include "Vfx/Metadata/VfxVehicleInfo.h"
#include "Vfx/Metadata/VfxWeaponInfo.h"
#include "Vfx/Misc/WaterCannon.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Systems/VfxBlood.h"
#include "Vfx/Systems/VfxEntity.h"
#include "Vfx/Systems/VfxExplosion.h"
#include "Vfx/Systems/VfxFire.h"
#include "Vfx/Systems/VfxGadget.h"
#include "Vfx/Systems/VfxGlass.h"
#include "Vfx/Systems/VfxLens.h"
#include "Vfx/Systems/VfxLiquid.h"
#include "Vfx/Systems/VfxScript.h"
#include "Vfx/Systems/VfxTrail.h"
#include "Vfx/Systems/VfxMaterial.h"
#include "Vfx/Systems/VfxPed.h"
#include "Vfx/Systems/VfxProjectile.h"
#include "Vfx/Systems/VfxVehicle.h"
#include "Vfx/Systems/VfxWater.h"
#include "Vfx/Systems/VfxWeapon.h"
#include "Vfx/Systems/VfxWeather.h"
#include "Vfx/Systems/VfxWheel.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "Vfx/VfxHelper.h"
#include "Vfx/VfxSettings.h"
#include "Vfx/vfx_channel.h"
#include "Vfx/Clouds/Clouds.h"

#include "shader_source/Vegetation/Trees/trees_shared.h"
#include "shaders/CustomShaderEffectTree.h"

#if __DEV
#include "Tools/TracingStats.h"
#endif



VFX_PTFX_OPTIMISATIONS()
VFX_DECAL_OPTIMISATIONS()
VFX_SYSTEM_OPTIMISATIONS()
VFX_MISC_OPTIMISATIONS()


// --- Defines ------------------------------------------------------------------


// --- Constants ----------------------------------------------------------------


// --- Globals ------------------------------------------------------------------


RAGE_DEFINE_CHANNEL(smash)

PARAM(smash_tty_windows, "Create RAG windows for smashable related TTY");



// --- Static Globals -----------------------------------------------------------

static ConfigLightSettings s_NVLightSettings;
static float s_NVLightRangeOverride = -1;
static bool s_NVLightUseSpec = false;
static bool s_NVLightUseShadows = false;
static bool s_NVLightUseVolumes = false;
static float s_NVLightShadowNearClip = 0.2f;
static float s_NVLightShadowBlur = 0.0f;
static float s_NVLightVolIntensity = 0.0f;
static float s_NVLightVolSize = 0.0f;
static float s_NVLightVolExp = 0.0f;
static Vec4V s_NVLightVolColor;
static Vec3V s_NVLightOffset;
static Vec3V s_NVFPVLightOffset;
static float s_NVLightDirBase = 0.5f;
static const float s_NVLightMultMax = 64.0f;

static ConfigLightSettings s_NVSecLightSettings;
static bool s_NVSecLightEnable = false;
static bool s_NVSecLightUseSpec = false;
static Vec3V s_NVSecLightOffset;
static float s_NVSecVLightDirBase = 0.5f;

BANK_ONLY(static bkButton* s_pCreateButton = NULL;)
BANK_ONLY(bool CVisualEffects::sm_disableVisualEffects = false;)


// --- Static class members -----------------------------------------------------



// --- Global Code --------------------------------------------------------------

void CVisualEffects::RenderForwardEmblems()
{
	g_decalMan.Render(DECAL_RENDER_PASS_VEHICLE_BADGE_LOCAL_PLAYER, DECAL_RENDER_PASS_VEHICLE_BADGE_NOT_LOCAL_PLAYER, true, false, true);
}

static void RenderDecalsAndPtfx(bool renderDecals)
{
#if SUPPORT_INVERTED_VIEWPORT
	grcViewport vp = *grcViewport::GetCurrent();
	vp.SetInvertZInProjectionMatrix(true,true);
	grcViewport *prev = grcViewport::SetCurrent(&vp);
#endif // SUPPORT_INVERTED_VIEWPORT

	if(renderDecals)
	{
		PF_PUSH_TIMEBAR_DETAIL("Decals - Forward Pass");
		Lights::BeginLightweightTechniques();

		g_decalMan.Render(DECAL_RENDER_PASS_FORWARD, DECAL_RENDER_PASS_FORWARD, true, false, false);

		Lights::EndLightweightTechniques();

		PF_POP_TIMEBAR_DETAIL();

	}

	PF_PUSH_TIMEBAR_DETAIL("PtFx Manager");

	grcRenderTarget *const backBuffer = CRenderTargets::GetBackBuffer(false);
	grcRenderTarget *const backSampler =
#if __D3D11 || DEVICE_MSAA
		// already resolved betwee Render and VisualEffects phases
		__D3D11 || GRCDEVICE.GetMSAA()? CRenderTargets::GetBackBufferCopy(false) :
#endif
		backBuffer;

	g_ptFxManager.Render(backBuffer, backSampler, false);

#if SUPPORT_INVERTED_VIEWPORT
	grcViewport::SetCurrent(prev);
#endif // SUPPORT_INVERTED_VIEWPORT

	PF_POP_TIMEBAR_DETAIL();
}

static void RenderMiscVfxPreDraw()
{
	if (LOCK_DOF_TARGETS_FOR_VFX && CPtFxManager::UseParticleDOF())
	{
		//if dof target are enabled always perform the unlock / lock
		CRenderTargets::UnlockSceneRenderTargets();
		CPtFxManager::LockPtfxDepthAlphaMap(GRCDEVICE.IsReadOnlyDepthAllowed());
	}
	else
	{
#if __D3D11
		//if read only depth is enabled, make sure to lock the depth copy
		if( GRCDEVICE.IsReadOnlyDepthAllowed() )
		{
			CRenderTargets::UnlockSceneRenderTargets();
			CRenderTargets::LockSceneRenderTargets_DepthCopy();
		}
#endif
		//otherwise no need to make any changes to the currently locked targets
	}

}

static void RenderMiscVfxPostDraw()
{
	if (LOCK_DOF_TARGETS_FOR_VFX && CPtFxManager::UseParticleDOF())
	{
		CPtFxManager::UnLockPtfxDepthAlphaMap();
		CRenderTargets::LockSceneRenderTargets();
	}
	else
	{
#if __D3D11
		//if read only depth is enabled, then the depth copy was locked in RenderMiscVfxPreDraw, so we need to undo that here
		if( GRCDEVICE.IsReadOnlyDepthAllowed() )
		{
			CRenderTargets::UnlockSceneRenderTargets(); // This will unlock the "depth copy"
			CRenderTargets::LockSceneRenderTargets();   // This will lock the scene color and regular depth buffer
		}
#endif
		//otherwise no need to make any changes to the currently locked targets
	}
}

class dlCmdSetGPUDropRenderSettings : public dlCmdBase{

public:
	enum {
		INSTRUCTION_ID = DC_SetGPUDropRenderSettings,
	};

	s32 GetCommandSize()  {return(sizeof(*this));}
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdSetGPUDropRenderSettings &) cmd).Execute(); }
	void Execute();

	dlCmdSetGPUDropRenderSettings();

private:
	ptxgpuDropRenderSettings m_DropRenderSettings;
	ptxgpuDropRenderSettings m_MistRenderSettings;
	ptxgpuDropRenderSettings m_GroundRenderSettings;
};


dlCmdSetGPUDropRenderSettings::dlCmdSetGPUDropRenderSettings()
{
	m_DropRenderSettings	= g_vfxWeather.GetPtFxGPUManager().GetDropSystem().GetRenderSetings();
	m_MistRenderSettings	= g_vfxWeather.GetPtFxGPUManager().GetMistSystem().GetRenderSetings();
	m_GroundRenderSettings	= g_vfxWeather.GetPtFxGPUManager().GetGroundSystem().GetRenderSetings();
}

void	dlCmdSetGPUDropRenderSettings::Execute()
{
	g_vfxWeather.GetPtFxGPUManager().GetDropSystem().SetRenderSettingsRT(m_DropRenderSettings);
	g_vfxWeather.GetPtFxGPUManager().GetMistSystem().SetRenderSettingsRT(m_MistRenderSettings);
	g_vfxWeather.GetPtFxGPUManager().GetGroundSystem().SetRenderSettingsRT(m_GroundRenderSettings);
}

static void RenderMiscVfxAfter(bool bRenderWeatherParticles, Vec3V_ConstRef vCamVel)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	RenderMiscVfxPreDraw();

	PF_PUSH_TIMEBAR_DETAIL("Vehicle Glass");
	g_vehicleGlassMan.Render();
	PF_POP_TIMEBAR_DETAIL();

	RenderDecalsAndPtfx(true);

	PF_PUSH_TIMEBAR_DETAIL("Golf Green Grid");
	GOLFGREENGRID.Render();
	PF_POP_TIMEBAR_DETAIL();

	if(bRenderWeatherParticles)
	{
		PF_PUSH_TIMEBAR_DETAIL("Vfx Weather");
		g_vfxWeather.Render(vCamVel);
		PF_POP_TIMEBAR_DETAIL();
	}

	RenderMiscVfxPostDraw();

	PF_PUSH_TIMEBAR_DETAIL("Bright Lights Render");
	g_brightLights.Render();
	PF_POP_TIMEBAR_DETAIL();
	
	WIN32PC_ONLY(if (grcEffect::GetShaderQuality() > 0))
	{
		PF_PUSH_TIMEBAR_DETAIL("Fog Volumes");
		g_fogVolumeMgr.RenderRegistered();
		PF_POP_TIMEBAR_DETAIL();
	}
}


static void RenderMiscVfxLateInterior()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	RenderMiscVfxPreDraw();
	RenderDecalsAndPtfx(true);
	RenderMiscVfxPostDraw();

	PF_PUSH_TIMEBAR_DETAIL("Fog Volumes Late Interior");
	g_fogVolumeMgr.RenderRegistered();
	PF_POP_TIMEBAR_DETAIL();

}

static void RenderMiscVfxLateVehicleInterior()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	RenderMiscVfxPreDraw();
	RenderDecalsAndPtfx(false);
	RenderMiscVfxPostDraw();
}

static void RenderPtfxIntoWaterRefraction()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PF_PUSH_TIMEBAR_DETAIL("PtFx Manager Water Refraction");
	
	g_ptFxManager.Render(Water::GetWaterRefractionColorTarget(), Water::GetWaterRefractionDepthTarget(), true);


	PF_POP_TIMEBAR_DETAIL();


}

static void RTResetDecalsForwardContext()
{
	DECALMGR.ResetContext();
}

// --- Code ---------------------------------------------------------------------


#if __D3D11
CVisualEffects::DX11_READ_ONLY_DEPTH_TARGETS CVisualEffects::ms_DX11ReadOnlyDepths[NUMBER_OF_RENDER_THREADS];
#endif //__D3D11


//
// name:	Init
// desc:
void CVisualEffects::Init(unsigned initMode)
{
	USE_MEMBUCKET(MEMBUCKET_FX);
	FX_MEM_TOTAL("INIT - Entering");

    if (initMode == INIT_CORE)
	{
		// called once at the very start of the game
		FX_MEM_RESET();

		g_visualSettings.LoadAll();

		GOLFGREENGRID.InitClass();

		FX_MEM_TOTAL("INIT_CORE");
	}
	else if (initMode == INIT_AFTER_MAP_LOADED)
	{
		// called after the map has been loaded
		g_vfxInteriorInfoMgr.LoadData();
		g_vfxRegionInfoMgr.LoadData();

		FX_MEM_TOTAL("INIT_AFTER_MAP_LOADED");
	}
	else if (initMode == INIT_SESSION)
	{
		// called when a new session is started (when starting a new game)
		FX_MEM_TOTAL("INIT_SESSION");
	}
	else
	{
		FX_MEM_TOTAL("INIT_UNKNOWN");
	}

	// init the systems
	{
	    // initialise the main systems
	    g_ptFxManager.Init(initMode);
		FX_MEM_CURR("Particles");
		g_decalMan.Init(initMode);
		FX_MEM_CURR("Decals");
		g_vehicleGlassMan.Init(initMode);
		FX_MEM_CURR("Smashables");

		// initialise environment systems
		g_sky.Init(initMode);
		FX_MEM_CURR("Sky");
		g_weather.Init(initMode);
		FX_MEM_CURR("Weather");
		g_timeCycle.Init(initMode);
		FX_MEM_CURR("Timecycle");
		CClouds::Init(initMode);
		FX_MEM_CURR("Clouds");
		g_LensFlare.Init(initMode);
		FX_MEM_CURR("Lens Flare");

		// initialise the vfx helper classes
		CVfxAsyncProbeManager::Init();
		CVfxHelper::Init();
		FX_MEM_CURR("Vfx Helper");

		// initialise the vfx systems
		g_vfxBlood.Init(initMode);
		FX_MEM_CURR("Vfx Blood");
		g_vfxEntity.Init(initMode);
		FX_MEM_CURR("Vfx Entity");
		g_vfxExplosion.Init(initMode);
		FX_MEM_CURR("Vfx Explosion");
		g_vfxFire.Init(initMode);
		FX_MEM_CURR("Vfx Fire");
		g_vfxGadget.Init(initMode);
		FX_MEM_CURR("Vfx Gadget");
		g_vfxGlass.Init(initMode);
		FX_MEM_CURR("Vfx Glass");
		g_vfxLens.Init(initMode);
		FX_MEM_CURR("Vfx Lens");
		g_vfxScript.Init(initMode);
		FX_MEM_CURR("Vfx Script");
		g_vfxTrail.Init(initMode);
		FX_MEM_CURR("Vfx Trail");
		g_vfxMaterial.Init(initMode);
		FX_MEM_CURR("Vfx Material");
		g_vfxLiquid.Init(initMode);
		FX_MEM_CURR("Vfx Liquid");
		g_vfxPed.Init(initMode);
		FX_MEM_CURR("Vfx Ped");
		g_vfxProjectile.Init(initMode);
		FX_MEM_CURR("Vfx Projectile");
		g_vfxVehicle.Init(initMode);
		FX_MEM_CURR("Vfx Vehicle");
		g_vfxWater.Init(initMode);
		FX_MEM_CURR("Vfx Water");
		g_vfxWeapon.Init(initMode);
		FX_MEM_CURR("Vfx Weapon");
		g_vfxWeather.Init(initMode);
		FX_MEM_CURR("Lightning");
		g_vfxLightningManager.Init(initMode);
		FX_MEM_CURR("Vfx Weather");
		g_vfxWheel.Init(initMode);
		FX_MEM_CURR("Vfx Wheel");

		// initialise misc vfx systems
		g_brightLights.Init(initMode);
		FX_MEM_CURR("Bright Lights");
		g_checkpoints.Init(initMode);	
		FX_MEM_CURR("Checkpoints");
		g_coronas.Init(initMode);
		FX_MEM_CURR("Coronas");
		g_fogVolumeMgr.Init(initMode);
		FX_MEM_CURR("Fog Volumes");
		CDistantLightsVertexBuffer::Init(initMode);	
		FX_MEM_CURR("Distant Lights Vtx Buffer");
		g_distantLights.Init(initMode);	
		FX_MEM_CURR("Distant Lights");
		g_fireMan.Init(initMode);
		FX_MEM_CURR("Fire");
		g_markers.Init(initMode);
		FX_MEM_CURR("Markers");
		GameGlows::Init(initMode);
		FX_MEM_CURR("GameGlows");
		ScriptIM::Init(initMode);
		FX_MEM_CURR("ScriptIM");
		g_movieMeshMgr.Init(initMode);
		FX_MEM_CURR("Movie Meshes");

		// movie manager is initialized earlier to play the intro logos movie
		if (initMode != INIT_CORE)
		{
			g_movieMgr.Init(initMode);
			FX_MEM_CURR("Movies");
		}

		CTVPlaylistManager::Init(initMode);
		FX_MEM_CURR("TVPlaylists");
		g_scrollbars.Init(initMode);
		FX_MEM_CURR("Scrollbars");
		g_waterCannonMan.Init(initMode);
		FX_MEM_CURR("Water Cannon");
    }
    
	// handle special cases
	if (initMode == INIT_AFTER_MAP_LOADED)
	{
	    // reset adaptive luminance post effect
	    static int debugNoAdaptionFrameCount=4;
	    PostFX::ResetAdaptedLuminance(debugNoAdaptionFrameCount);

		vfxUtil::ResetBrokenFrags();
		FX_MEM_CURR("Vfx Helper");
    }

	// at the end of the INIT_SESSION pass we need to make sure any vfx streaming requests have completed 
	// and do any functionality in these systems that relies on these being complete
	if (initMode == INIT_SESSION)
	{
		// load in the requested models
		CStreaming::LoadAllRequestedObjects();

		g_ptFxManager.InitSessionSync();
		FX_MEM_CURR("Particles");

		g_markers.InitSessionSync();
		FX_MEM_CURR("Markers");
	}

	FX_MEM_TOTAL("INIT - Leaving");
}

void CVisualEffects::InitDLCCommands()
{
	DLC_REGISTER_EXTERNAL(dlCmdSetGPUDropRenderSettings);
	CLODLightManager::InitDLCCommands();
	CCoronas::InitDLCCommands();
	CFogVolumeMgr::InitDLCCommands();
	CVfxLightningManager::InitDLCCommands();
}

class CVisualEffectsFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile& file)
	{
		switch(file.m_fileType)
		{
			case CDataFileMgr::PTFXASSETINFO_FILE:
				g_ptfxAssetInfoMgr.AppendData(ptfxManager::GetAssetStore(),file.m_filename);
			default:
			break;	
		}
		return true;
	}
	virtual void UnloadDataFile(const CDataFileMgr::DataFile & /*file*/)
	{

	}
}g_visualEffectsFileMounter;


//
// name:	InitDefinitions
// desc:	this gets called in between the map data file manager being set up and the map data being loaded
void CVisualEffects::InitDefinitionsPreRpfs()
{
	g_decalMan.InitDefinitions();
	g_vfxEntity.InitDefinitions();
	//@@: location CVISUALEFFECTS_INITDEFINITIONS_LOAD_DATA
	g_vfxWeaponInfoMgr.LoadData();
	g_vfxPedInfoMgr.LoadData();
	g_vfxVehicleInfoMgr.LoadData();
	
	g_vfxFogVolumeInfoMgr.LoadData();
}


//
// name:	InitDefinitions
// desc:	this gets called in between the map data file manager being set up and the map data being loaded
void CVisualEffects::InitDefinitionsPostRpfs()
{
	g_ptfxAssetInfoMgr.LoadData(ptfxManager::GetAssetStore());
	CDataFileMount::RegisterMountInterface(CDataFileMgr::PTFXASSETINFO_FILE,&g_visualEffectsFileMounter);
}

//
//
//
//
void CVisualEffects::InitTunables()
{
#if RAIN_UPDATE_CPU
	bool bCpuRain = false;
	
	// enable RainUpdateCpu on multiGPU machines:
	if(grcDevice::GetGPUCount() > 1)
	{
		bCpuRain = true;

		// tunables override:
		bCpuRain = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GFX_RAIN_UPDATE_CPU", 0xAAF599DC), bCpuRain);
	}

	#if !__FINAL
		XPARAM(CpuRainDisable);
		XPARAM(CpuRainEnable);
		// commandline overrides:
		if(PARAM_CpuRainDisable.Get())
		{
			bCpuRain = false;
		}
	
		if(PARAM_CpuRainEnable.Get())
		{
			bCpuRain = true;
		}
	#endif

	ptxgpuBase::EnableCpuRainUpdate(bCpuRain);
#endif
}

//
// name:	Shutdown
// desc:
void CVisualEffects::Shutdown(unsigned shutdownMode)
{
	USE_MEMBUCKET(MEMBUCKET_FX);
	FX_MEM_TOTAL("SHUTDOWN - Entering");

    if (shutdownMode == SHUTDOWN_CORE)
	{
		GOLFGREENGRID.ShutdownClass();

		// called once at the very end of the game
		FX_MEM_TOTAL("SHUTDOWN_CORE");
	}
	else if (shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
	{
		// called just before the map is unloaded
		FX_MEM_TOTAL("SHUTDOWN_WITH_MAP_LOADED");
	}
	else if (shutdownMode == SHUTDOWN_SESSION)
	{
		// called when a new session is ended (when starting a new game)
		FX_MEM_TOTAL("SHUTDOWN_SESSION");
	}
	else
	{
		FX_MEM_TOTAL("SHUTDOWN_UNKNOWN");
	}

	// shutdown the systems in reverse order
	{
		// shutdown misc vfx systems
		g_waterCannonMan.Shutdown(shutdownMode);	
		FX_MEM_CURR("Water Cannon");
		g_scrollbars.Shutdown(shutdownMode);	
		FX_MEM_CURR("Scrollbars");
		g_movieMeshMgr.Shutdown(shutdownMode);	
		FX_MEM_CURR("Movie Meshes");
		CTVPlaylistManager::Shutdown(shutdownMode);
		FX_MEM_CURR("TVPlaylists");
		g_movieMgr.Shutdown(shutdownMode);	
		FX_MEM_CURR("Movies");
		g_markers.Shutdown(shutdownMode);	
		FX_MEM_CURR("Markers");
		g_fireMan.Shutdown(shutdownMode);	
		FX_MEM_CURR("Fire");
		g_distantLights.Shutdown(shutdownMode);		
		FX_MEM_CURR("Distant Lights");
		CDistantLightsVertexBuffer::Shutdown(shutdownMode);	
		FX_MEM_CURR("Distant Lights Vtx Buffer");
		g_coronas.Shutdown(shutdownMode);	
		FX_MEM_CURR("Coronas");
		g_fogVolumeMgr.Shutdown(shutdownMode);	
		FX_MEM_CURR("Fog Volumes");
		g_checkpoints.Shutdown(shutdownMode);		
		FX_MEM_CURR("Checkpoints");
		g_brightLights.Shutdown(shutdownMode);	
		FX_MEM_CURR("Bright Lights");

		// shutdown the vfx systems
		g_vfxWheel.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Wheel");
		g_vfxWeather.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Weather");
		g_vfxWeapon.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Weapon");
		g_vfxWater.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Water");
		g_vfxVehicle.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Vehicle");
		g_vfxProjectile.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Projectile");
		g_vfxPed.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Ped");
		g_vfxMaterial.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Material");
		g_vfxLiquid.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Liquid");
		g_vfxTrail.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Trail");
		g_vfxScript.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Script");
		g_vfxLens.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Lens");
		g_vfxGlass.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Glass");
		g_vfxGadget.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Gadget");
		g_vfxFire.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Fire");
		g_vfxExplosion.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Explosion");
		g_vfxEntity.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Entity");
		g_vfxBlood.Shutdown(shutdownMode);
		FX_MEM_CURR("Vfx Blood");

		// shutdown the vfx helper classes
		CVfxAsyncProbeManager::Shutdown();
		FX_MEM_CURR("Vfx Helper");

		// shutdown environment systems
		g_LensFlare.Shutdown(shutdownMode);
		FX_MEM_CURR("Lens Flare");
		CClouds::Shutdown(shutdownMode);
		FX_MEM_CURR("Clouds");
		g_timeCycle.Shutdown(shutdownMode);
		FX_MEM_CURR("Timecycle");
		g_weather.Shutdown(shutdownMode);
		FX_MEM_CURR("Weather");
		g_vfxLightningManager.Shutdown(shutdownMode);
		FX_MEM_CURR("Lightning");
		g_sky.Shutdown(shutdownMode);
		FX_MEM_CURR("Sky");

		// shutdown the main systems
		g_vehicleGlassMan.Shutdown(shutdownMode);
		FX_MEM_CURR("Smashables");
		g_decalMan.Shutdown(shutdownMode);
		FX_MEM_CURR("Decals");
		g_ptFxManager.Shutdown(shutdownMode);
		FX_MEM_CURR("Particles");
	}

	FX_MEM_TOTAL("SHUTDOWN - Leaving");
}


//
// name:	PreUpdate
// desc:
void CVisualEffects::PreUpdate()
{
	g_fogVolumeMgr.PreUpdate();

}

//
// name:	Update
// desc:
void CVisualEffects::Update()
{
	USE_MEMBUCKET(MEMBUCKET_FX);
	BANK_ONLY(if (sm_disableVisualEffects) { return; })

	REPLAY_ONLY (bool shouldUpdate = !CReplayMgr::IsReplayInControlOfWorld() || !CReplayMgr::IsSettingUp();)

	if (!fwTimer::IsGamePaused() REPLAY_ONLY( && shouldUpdate ) )
	{
		float deltaTime = fwTimer::GetTimeStep();

		PF_PUSH_TIMEBAR_DETAIL("VFX Update");
		g_ptFxManager.ProcessStoredData();

		PF_START_TIMEBAR_DETAIL("Weather");
		g_weather.Update();

		// render the debug weather during the update so that the wind data isn't being updated as it's being rendered
#if __BANK
		PF_START_TIMEBAR_DETAIL("Weather Render Debug");
		g_weather.RenderDebug();
#endif

		PF_START_TIMEBAR_DETAIL("Clouds");
		CClouds::Update();

		PF_START_TIMEBAR_DETAIL("Lightning");
		g_vfxLightningManager.Update(deltaTime);

		PF_START_TIMEBAR_DETAIL("TVPlaylists");
		CTVPlaylistManager::Update();

		PF_START_TIMEBAR_DETAIL("Distant Lights");
		g_distantLights.Update();		

		PF_START_TIMEBAR_DETAIL("Water Cannons");
		g_waterCannonMan.Update(deltaTime);

		PF_START_TIMEBAR_DETAIL("Vfx Weather");
		g_vfxWeather.Update(deltaTime);

		PF_START_TIMEBAR_DETAIL("Checkpoints");
		g_checkpoints.Update();

		PF_START_TIMEBAR_DETAIL("Vfx Lens Update");
		g_vfxLens.Update();

		PF_START_TIMEBAR_DETAIL("Vfx Water");
		g_vfxWater.Update(deltaTime);

		PF_START_TIMEBAR_DETAIL("Vfx Weapon");
		g_vfxWeapon.Update();

		PF_START_TIMEBAR_DETAIL("Vfx Vehicle");
		g_vfxVehicle.Update();

		PF_START_TIMEBAR_DETAIL("Vfx Material");
		g_vfxMaterial.Update();

		PF_START_TIMEBAR_DETAIL("Vfx Explosion");
		g_vfxExplosion.Update();

		PF_START_TIMEBAR_DETAIL("Vfx Entity");
		g_vfxEntity.Update();

		PF_START_TIMEBAR_DETAIL("Vfx Blood");
		g_vfxBlood.Update();

		// Do the fire last as it starts a task running on a separate thread and it accesses particle fx
		PF_START_TIMEBAR_DETAIL("Fire");
		g_fireMan.Update(deltaTime);

		PF_START_TIMEBAR_DETAIL("FogVolumes");
		g_vfxFogVolumeInfoMgr.Update();

#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
		PF_START_TIMEBAR_DETAIL("Tree wind update");
		CCustomShaderEffectTree::UpdateWindStuff();
#endif //TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS

#if __BANK
		PF_START_TIMEBAR_DETAIL("Fire Debug");
		g_fireMan.RenderDebug();

		PF_START_TIMEBAR_DETAIL("Movie Mesh Debug");
		g_movieMeshMgr.RenderDebug3D();
#endif

		PF_POP_TIMEBAR_DETAIL();
	}
	else
	{
		g_ptFxManager.ProcessStoredData();

		// update the weather and timecycle even when paused (requested by les - bug 1626)
		// this could potentially cause all sorts of problems 
		g_weather.Update();

		// Need to process clouds when paused otherwise they get too bright when we move time
		PF_START_TIMEBAR_DETAIL("Clouds");
		CClouds::Update();

#if __BANK
		g_weather.RenderDebug();

		PF_START_TIMEBAR_DETAIL("Vfx Weather");
		g_vfxWeather.Update(0.0f);

		PF_START_TIMEBAR_DETAIL("Vfx Water");
		g_vfxWater.Update(0.0f);
#endif // __BANK
	}

	PF_START_TIMEBAR_DETAIL("Movie Manager");
	g_movieMgr.UpdateFrame();

}


//
// name:	UpdateAfterPreRender
// desc:
void CVisualEffects::UpdateAfterPreRender()
{
	USE_MEMBUCKET(MEMBUCKET_FX);
	BANK_ONLY(if (sm_disableVisualEffects) { return; })

	REPLAY_ONLY(bool updateGameSystems = CReplayMgr::ShouldPokeGameSystems());

	if (!fwTimer::IsGamePaused() REPLAY_ONLY(|| updateGameSystems))
	{
		// update after the pre render for anything that relies on skeletons being calculated first
		PF_PUSH_TIMEBAR_DETAIL("VFX UpdateAfterPreRender");

		// get the time step
		float deltaTime = fwTimer::GetTimeStep();

#if GTA_REPLAY
		//override the actual delta with the replay one
		if (CReplayMgr::IsEditModeActive())
		{
			deltaTime = updateGameSystems ? 0.1f : CReplayMgr::GetVfxDelta() / 1000.0f;
		}
#endif // GTA_REPLAY

		AddNightVisionLight();

		PF_START_TIMEBAR_DETAIL("Vfx Trail");
		g_vfxTrail.Update(deltaTime);

		// update the particle effects manager
		PF_START_TIMEBAR_DETAIL("PtFx Manager");

#if GTA_REPLAY
		if(!CReplayMgr::IsReplayInControlOfWorld() || !fwTimer::IsGamePaused())
#endif // GTA_REPLAY
		{
			g_ptFxManager.Update(deltaTime);
		}

		PF_POP_TIMEBAR_DETAIL();
	}

#if __BANK
	PGTAMATERIALMGR->GetDebugInterface().Update();
#endif
}


//
// name:	UpdateSafe
// desc:
void CVisualEffects::UpdateSafe()
{
	USE_MEMBUCKET(MEMBUCKET_FX);
	BANK_ONLY(if (sm_disableVisualEffects) { return; })

	// Wait for completion of all async decal tasks from last frame.  Must be
	// called before g_vehicleGlassMan.UpdateSafe*(), since that can modify
	// vertex buffers that the decals could otherwise still be using.  Also must
	// be sure to call this even when the game is paused, otherwise we can hold
	// references/decal locks on things too long.
	PF_START_TIMEBAR_DETAIL("Vfx Decals - Complete Async");
	g_decalMan.UpdateCompleteAsync();

	float deltaTime;
	if (!fwTimer::IsGamePaused())
	{
		deltaTime = fwTimer::GetTimeStep();

		PF_START_TIMEBAR_DETAIL("Golf Green Grid");
		GOLFGREENGRID.Update();

		PF_START_TIMEBAR_DETAIL("Vfx Vehicle Glass");
		g_vehicleGlassMan.UpdateSafe(deltaTime);
	}
	else
	{
		deltaTime = 0.f;

#if GTA_REPLAY
		if(CReplayMgr::IsReplayInControlOfWorld() && CReplayMgr::ShouldPokeGameSystems())
			deltaTime = 33.0f;

		if(CReplayMgr::IsReplayInControlOfWorld() && CReplayMgr::IsPlaybackPaused())
		{
			//don't used the pause version if the replay is paused we need
			//to do a proper update to make sure the glass appears correctly
			g_vehicleGlassMan.UpdateSafe(deltaTime);
		}
		else
#endif // GTA_REPLAY
		{
			g_vehicleGlassMan.UpdateSafeWhilePaused();
		}
	}

	// Complete the rest of the decal update, and start any new async work.
	// Must be called after g_vehicleGlassMan.UpdateSafe*().
	PF_START_TIMEBAR_DETAIL("Vfx Decals - Start Async");
	g_decalMan.UpdateStartAsync(deltaTime);

	// do any updates that need done after rendering
	g_brightLights.UpdateAfterRender();
	g_LensFlare.UpdateAfterRender();
	CVisualEffects::RegisterLensFlares(); // Must be done after g_LensFlare.UpdateAfterRender()
	CClouds::Synchronise();
	g_vfxLightningManager.UpdateAfterRender();
	g_fogVolumeMgr.UpdateAfterRender();

#if __BANK
	g_vehicleGlassMan.RenderDebug();
#endif // __BANK

	// 
	vfxUtil::ResetBrokenFrags();

	CVfxHelper::SetPrevTimeStep(fwTimer::GetTimeStep());
}



///////////////////////////////////////////////////////////////////////////////
//  SetupRenderthreadFrameInfo
///////////////////////////////////////////////////////////////////////////////

void CVisualEffects::SetupRenderThreadFrameInfo()
{
	CLODLightManager::SetupRenderthreadFrameInfo();
	g_coronas.SetupRenderthreadFrameInfo();
	g_fogVolumeMgr.SetupRenderthreadFrameInfo();
	g_vfxLightningManager.SetupRenderthreadFrameInfo();
}

///////////////////////////////////////////////////////////////////////////////
//  ClearRenderthreadFrameInfo
///////////////////////////////////////////////////////////////////////////////

void CVisualEffects::ClearRenderThreadFrameInfo()
{
	CLODLightManager::ClearRenderthreadFrameInfo();
	g_coronas.ClearRenderthreadFrameInfo();
	g_fogVolumeMgr.ClearRenderthreadFrameInfo();
	g_vfxLightningManager.ClearRenderthreadFrameInfo();
}

//
// name:	RenderSky
// desc:	
void CVisualEffects::RenderSky(eRenderMode mode, bool bUseStencilMask, bool bUseRestrictedProjection, int scissorX, int scissorY, int scissorW, int scissorH, bool bFirstCallThisDrawList, float globalCloudAlpha)
{
#if __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		return;
	}
#endif // __BANK

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	USE_MEMBUCKET(MEMBUCKET_FX);
	BANK_ONLY(if (sm_disableVisualEffects) { return; })

	PF_PUSH_TIMEBAR_DETAIL("Sky");
	if(mode != CVisualEffects::RM_CUBE_REFLECTION)
	{
		// 2/14/14 - cthomas - Don't need to call this again for each RenderSky() call in the cubemap reflections.
		// Its a bit expensive on next-gen atm, and we call RenderSky() for 5 out of the 6 cubemap facets.
		// We are already calling this once before we render anything to the reflection map, so shouldn't need to 
		// be done again anyways.
		g_timeCycle.SetShaderData(mode);
	}
	g_sky.Render(mode, bUseStencilMask, bUseRestrictedProjection, scissorX, scissorY, scissorW, scissorH, bFirstCallThisDrawList);
	PF_POP_TIMEBAR_DETAIL();
	
	if(mode != RM_CUBE_REFLECTION && mode != RM_SEETHROUGH)
	{
		PF_PUSH_TIMEBAR_DETAIL("Clouds");

		#if __D3D11
			if(ms_DX11ReadOnlyDepths[g_RenderThreadIndex].depthAndStencilBuffer)
				CRenderTargets::LockReadOnlyDepth(ms_DX11ReadOnlyDepths[g_RenderThreadIndex].needsCopy, true, ms_DX11ReadOnlyDepths[g_RenderThreadIndex].depthAndStencilBuffer, ms_DX11ReadOnlyDepths[g_RenderThreadIndex].depthBufferCopy, ms_DX11ReadOnlyDepths[g_RenderThreadIndex].depthAndStencilBufferReadOnly);
			else
				CRenderTargets::LockReadOnlyDepth(false);
			/*
			// We need to read from the depth and do depth test so use the read only depth buffer.
			if( mode == RM_WATER_REFLECTION )
				CRenderTargets::LockReadOnlyDepth( false, true, CRenderPhaseWaterReflection::GetDepth(), CRenderPhaseWaterReflection::GetDepthCopy(), CRenderPhaseWaterReflection::GetDepthReadOnly() );
			else if( mode == RM_MIRROR_REFLECTION )
				CRenderTargets::LockReadOnlyDepth( true, true, CMirrors::GetMirrorDepthTarget(), CMirrors::GetMirrorDepthTarget_Copy(), CMirrors::GetMirrorDepthTarget_ReadOnly() );
			else
				CRenderTargets::LockReadOnlyDepth( false);
			*/
		#endif // __D3D11

		CClouds::Render(mode, scissorX, scissorY, scissorW, scissorH, globalCloudAlpha);

		#if __D3D11
			CRenderTargets::UnlockReadOnlyDepth();
		#endif // __D3D11

		PF_POP_TIMEBAR_DETAIL();
	}

	if(mode == RM_DEFAULT)
	{
		//Right now testing only normal rendering

		g_vfxLightningManager.Render();
	}
}


#if __D3D11
void CVisualEffects::SetTargetsForDX11ReadOnlyDepth(bool needsCopy, grcRenderTarget* depthAndStencilBuffer, grcRenderTarget* depthBufferCopy, grcRenderTarget* depthAndStencilBufferReadOnly)
{
	ms_DX11ReadOnlyDepths[g_RenderThreadIndex].needsCopy = needsCopy;
	ms_DX11ReadOnlyDepths[g_RenderThreadIndex].depthAndStencilBuffer = depthAndStencilBuffer;
	ms_DX11ReadOnlyDepths[g_RenderThreadIndex].depthBufferCopy = depthBufferCopy;
	ms_DX11ReadOnlyDepths[g_RenderThreadIndex].depthAndStencilBufferReadOnly = depthAndStencilBufferReadOnly;
}
#endif // __D3D11


//
// name:	RenderDistantLights
// desc:
void CVisualEffects::RenderDistantLights(eRenderMode mode, u32 renderFlags, float intensityScale)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	USE_MEMBUCKET(MEMBUCKET_FX);
	BANK_ONLY(if (sm_disableVisualEffects) { return; })

	PF_PUSH_TIMEBAR("Distant Lights");

	if (renderFlags & RENDER_SETTING_RENDER_DISTANT_LIGHTS)
	{
		PF_PUSH_TIMEBAR_DETAIL("Distant Lights");
		g_distantLights.Render(mode, intensityScale);
		PF_POP_TIMEBAR_DETAIL();
	}

	if (renderFlags & RENDER_SETTING_RENDER_DISTANT_LOD_LIGHTS)
	{
		PF_PUSH_TIMEBAR_DETAIL("Distant LOD Lights");
		CLODLights::RenderDistantLODLights(mode, intensityScale);
		PF_POP_TIMEBAR_DETAIL();
	}

	PF_POP_TIMEBAR();
}

void CVisualEffects::RenderBeforeWater(u32 renderFlags)
{
	USE_MEMBUCKET(MEMBUCKET_FX);
	BANK_ONLY(if (sm_disableVisualEffects) { return; })

	PF_PUSH_TIMEBAR_DETAIL("Particle Lights Setup");

	if (renderFlags & RENDER_SETTING_RENDER_PARTICLES)
	{
		// We force a light setup here, so that particles are now lit properly.
		Lights::SetupRenderThreadLights();
	}
	PF_POP_TIMEBAR_DETAIL();
}
//
// name:	RenderAfter
// desc:	
void CVisualEffects::RenderAfter(u32 renderFlags)
{
	USE_MEMBUCKET(MEMBUCKET_FX);
	BANK_ONLY(if (sm_disableVisualEffects) { return; })

	PF_PUSH_TIMEBAR_DETAIL("VFX RenderAfter");

	if (renderFlags & RENDER_SETTING_RENDER_PARTICLES)
	{
		// We force a light setup here, so that particles are now lit properly.
		//Lights::SetupRenderThreadLights();

		// Only update the GPU particles if exterior is visible or camera is under water
		bool bRenderWeatherParticles = fwScan::GetScanResults().GetIsExteriorVisibleInGbuf() || CVfxHelper::GetGameCamWaterDepth() > 0.0f;

		// Get the camera velocity on the main thread where its avaliable
		static Vec3V vLastNonPausedCamVel = Vec3V(V_ZERO);
		if (!fwTimer::IsGamePaused())
		{
			vLastNonPausedCamVel = RCC_VEC3V(camInterface::GetVelocity());
		}

		DLC(dlCmdSetGPUDropRenderSettings, ());
		DLC_Add(RenderMiscVfxAfter, bRenderWeatherParticles, vLastNonPausedCamVel);
	}

	if (renderFlags & RENDER_SETTING_RENDER_UI_EFFECTS)
	{
		if (CVfxHelper::ShouldRenderInGameUI())
		{
			CGolfTrailInterface::UpdateAndRender(true);
		}
	}
	
	PF_POP_TIMEBAR_DETAIL();
}

void CVisualEffects::RenderLateInterior(u32 renderFlags)
{
	PF_PUSH_TIMEBAR_DETAIL("VFX Late Interior");

	if (renderFlags & RENDER_SETTING_RENDER_PARTICLES)
	{
		// We force a light setup here, so that particles are now lit properly.
		DLC_Add(RenderMiscVfxLateInterior);
	}

	PF_POP_TIMEBAR_DETAIL();
}

void CVisualEffects::RenderLateVehicleInterior(u32 renderFlags)
{
	PF_PUSH_TIMEBAR_DETAIL("VFX Late Vehicle Interior");

	if (renderFlags & RENDER_SETTING_RENDER_PARTICLES)
	{
		// We force a light setup here, so that particles are now lit properly.
		DLC_Add(RenderMiscVfxLateVehicleInterior);
	}

	PF_POP_TIMEBAR_DETAIL();
}


void CVisualEffects::ResetDecalsForwardContext()
{
	DLC_Add(RTResetDecalsForwardContext);
}

void CVisualEffects::RenderIntoWaterRefraction(u32 renderFlags)
{
	PF_PUSH_TIMEBAR_DETAIL("VFX Water Refraction");

	if (renderFlags & RENDER_SETTING_RENDER_PARTICLES)
	{
		// We force a light setup here, so that particles are now lit properly.
		DLC_Add(RenderPtfxIntoWaterRefraction);
	}

	PF_POP_TIMEBAR_DETAIL();

}

//
// name:		RenderMarkers
// desc:
void CVisualEffects::RenderMarkers(const eMarkerMask mask)
{
	USE_MEMBUCKET(MEMBUCKET_FX);
	BANK_ONLY(if (sm_disableVisualEffects) { return; })

	if (mask & MM_MARKERS)
	{
		// the update needs to be here at markers are registered from the radar's update task thread which mostly 
		// happens after the visual effects update has been called causing the markers to not get updated correctly
		PF_PUSH_TIMEBAR_DETAIL("Markers");
		g_markers.Update();

		g_markers.Render();
		PF_POP_TIMEBAR_DETAIL();
	}

	if (mask & MM_GLOWS)
	{
		PF_PUSH_TIMEBAR_DETAIL("GameGlows");
		GameGlows::Render();
		PF_POP_TIMEBAR_DETAIL();
	}

	if (mask & MM_SCRIPT)
	{
		PF_PUSH_TIMEBAR_DETAIL("ScriptIM");
		ScriptIM::Render();
		PF_POP_TIMEBAR_DETAIL();
	}

	if ((mask & MM_SCALEFORM) && CVfxHelper::ShouldRenderInGameUI())
	{
		PF_PUSH_TIMEBAR_DETAIL("Scaleform3D");
		DLC_Add(CScaleformMgr::RenderWorldSpaceSolid);
		PF_POP_TIMEBAR_DETAIL();
	}
}

//
// name:		RenderCoronas
// desc:
void CVisualEffects::RenderCoronas(eRenderMode mode, float sizeScale, float intensityScale)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	USE_MEMBUCKET(MEMBUCKET_FX);
	BANK_ONLY(if (sm_disableVisualEffects) { return; })

	PF_PUSH_TIMEBAR_DETAIL("Coronas Render");
	g_coronas.Render(mode, sizeScale, intensityScale);
	PF_POP_TIMEBAR_DETAIL();
}

void CVisualEffects::IssueLensFlareOcclusionQuerries()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	g_LensFlare.IssueOcclusionQuerries();
}

void CVisualEffects::RenderLensFlare(bool cameraCut)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	g_LensFlare.Render(cameraCut);
}

//
// name:	RegisterLensFlares
// desc:
void CVisualEffects::RegisterLensFlares()
{
	// Register the sun's corona
	const Vec3V vSunDir = g_sky.GetSunDirection();
	const float fSunRadius = g_sky.GetSunRadius();
	Vec3V vCamPos = RCC_VEC3V(camInterface::GetPos());
	vCamPos.SetZf(g_vfxSettings.GetWaterLevel());
	Vec3V vSunPos = AddScaled(vCamPos, vSunDir, ScalarVConstant<DOME_SCALE>());
	Color32 sunColor;
	g_sky.GetSunColor(sunColor);
	g_LensFlare.Register(LF_SUN, sunColor, 1.0f, vSunPos, fSunRadius);
}

float CVisualEffects::GetSunVisibility()
{
	return g_LensFlare.GetSunVisibility();
}

void CVisualEffects::OverrideNightVisionLightRange(float newRange)
{
	s_NVLightRangeOverride = newRange;
}

void CVisualEffects::AddNightVisionLight()
{
	if(PostFX::GetUseNightVision())
	{
		const Matrix34 &camMatrix = camInterface::GetMat();
		const CPed * pPlayer = CGameWorld::FindLocalPlayer();
		
		// Primary light
		{
			Vector3 colour =  RCC_VECTOR3(s_NVLightSettings.colour);
			float intensity = s_NVLightSettings.intensity;
			
			float range = Min(s_NVLightSettings.radius, camInterface::GetFar() * 0.9f);
			if(s_NVLightRangeOverride > 0.0f)
			{	// optional script range override:
				range = s_NVLightRangeOverride;
			}

			float falloffExp = s_NVLightSettings.falloffExp;
			float innerAngle = s_NVLightSettings.innerConeAngle;
			float outerAngle = s_NVLightSettings.outerConeAngle;

			Vector3 offset;

			if( camInterface::IsRenderingFirstPersonShooterCamera() )
			{
				camMatrix.Transform3x3(RCC_VECTOR3(s_NVFPVLightOffset),offset);
			}
			else
			{
				camMatrix.Transform3x3(RCC_VECTOR3(s_NVLightOffset),offset);
			}

			Vector3 lightPos = camMatrix.d + offset;
		
			int lightFlags = LIGHTFLAG_FX | LIGHTFLAG_MOVING_LIGHT_SOURCE;

			if(!s_NVLightUseSpec) 
				lightFlags |= LIGHTFLAG_NO_SPECULAR;

			bool needShadows = false;
			float lightDelta = 0.0f;		
			if(s_NVLightUseShadows && (CNewHud::IsSniperSightActive() == false)) 
			{
				needShadows = true;
				lightDelta = Clamp(g_timeCycle.GetDirectionalLightMult()/s_NVLightMultMax - s_NVLightDirBase,0.0f,1.0f) * 2.0f;
				lightDelta *= 1.0f - g_timeCycle.GetStaleInteriorBlendStrength();
				if( lightDelta < s_NVLightDirBase )
				{
					needShadows = true;
				}
			}

			if( needShadows )
				lightFlags |= LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS;
		 
			if(s_NVLightUseVolumes) 
				lightFlags |= LIGHTFLAG_DRAW_VOLUME | LIGHTFLAG_USE_VOLUME_OUTER_COLOUR;

			CLightSource light(
				LIGHT_TYPE_SPOT, 
				lightFlags,
				lightPos, 
				colour, 
				intensity, 
				LIGHT_ALWAYS_ON);

			light.SetDirTangent(camMatrix.b, camMatrix.a);
			light.SetInInterior(CVfxHelper::GetCamInteriorLocation());

			if(needShadows)
			{
				light.SetExtraFlag(EXTRA_LIGHTFLAG_USE_FADEDIST_AS_FADEVAL);
				light.SetLightFadeDistance(255);
				light.SetShadowFadeDistance((u8)Clamp(((1.0f - lightDelta) * 255.0f) + 1.0f,0.0f,255.0f));
				light.SetSpecularFadeDistance(255);
				light.SetVolumetricFadeDistance(255);
				light.SetShadowNearClip(s_NVLightShadowNearClip);
				light.SetShadowBlur(s_NVLightShadowBlur);
				light.SetShadowTrackingId(fwIdKeyGenerator::Get(pPlayer));
			}
		
			light.SetRadius(range);
			light.SetFalloffExponent(falloffExp);
			light.SetSpotlight(innerAngle, outerAngle);
		
			if(s_NVLightUseVolumes)
				light.SetLightVolumeIntensity(s_NVLightVolIntensity, s_NVLightVolSize, s_NVLightVolColor, s_NVLightVolExp);
		
			Lights::AddSceneLight(light);
		}

		// Secondary Light
		if(s_NVSecLightEnable && camInterface::IsRenderingFirstPersonShooterCamera())
		{
			Vector3 colour =  RCC_VECTOR3(s_NVSecLightSettings.colour);
			float intensity = s_NVSecLightSettings.intensity;
			float range = Min(s_NVSecLightSettings.radius, camInterface::GetFar() * 0.9f);
			float falloffExp = s_NVSecLightSettings.falloffExp;
			float innerAngle = s_NVSecLightSettings.innerConeAngle;
			float outerAngle = s_NVSecLightSettings.outerConeAngle;

			Vector3 offset;

			camMatrix.Transform3x3(RCC_VECTOR3(s_NVSecLightOffset),offset);

			Vector3 lightPos = camMatrix.d + offset;
		
			int lightFlags = LIGHTFLAG_FX | LIGHTFLAG_MOVING_LIGHT_SOURCE;

			if(!s_NVSecLightUseSpec) 
				lightFlags |= LIGHTFLAG_NO_SPECULAR;

			CLightSource light(
				LIGHT_TYPE_SPOT, 
				lightFlags,
				lightPos, 
				colour, 
				intensity, 
				LIGHT_ALWAYS_ON);

			light.SetDirTangent(camMatrix.b, camMatrix.a);
			light.SetInInterior(CVfxHelper::GetCamInteriorLocation());

			light.SetRadius(range);
			light.SetFalloffExponent(falloffExp);
			light.SetSpotlight(innerAngle, outerAngle);
		
			Lights::AddSceneLight(light);
		}
	}
}

void CVisualEffects::UpdateVisualDataSettings()
{
	if (g_visualSettings.GetIsLoaded())
	{
		s_NVLightSettings.Set( g_visualSettings, "dark.light" );
		s_NVLightOffset = g_visualSettings.GetVec3V("dark.light.offset");
		s_NVFPVLightOffset = g_visualSettings.GetVec3V("dark.fpv.light.offset");
		s_NVLightDirBase = g_visualSettings.Get("dark.lightDirBase");
				
		s_NVLightUseSpec = g_visualSettings.Get("dark.useSpec",0.0f) == 1.0f;
		s_NVLightUseShadows = g_visualSettings.Get("dark.useShadows",0.0f) == 1.0f;
		s_NVLightUseVolumes = g_visualSettings.Get("dark.useVolumes",0.0f) == 1.0f;

		s_NVLightShadowNearClip = g_visualSettings.Get("dark.shadowNearClip",0.2f);
		s_NVLightShadowBlur = g_visualSettings.Get("dark.shadowBlur",0.0f);
		s_NVLightVolIntensity = g_visualSettings.Get("dark.volumeIntensity",0.0f);
		s_NVLightVolSize = g_visualSettings.Get("dark.volumeSize",0.0f) == 1.0f;
		s_NVLightVolExp = g_visualSettings.Get("dark.volumeExponent",0.0f);
		s_NVLightVolColor = g_visualSettings.GetVec4V("dark.volumeColor");

		s_NVSecLightSettings.Set( g_visualSettings, "dark.secLight");
		s_NVSecLightEnable = g_visualSettings.Get("dark.secLight.enable",0.0f) == 1.0f;
		s_NVSecLightUseSpec = g_visualSettings.Get("dark.secLight.useSpec",0.0f) == 1.0f;
		s_NVSecLightOffset = g_visualSettings.GetVec3V("dark.secLight.offset");
		s_NVSecVLightDirBase = g_visualSettings.Get("dark.secLight.lightDirBase");
	}
	
}
//
// name:	RenderDebug
// desc:

#if __BANK
void CVisualEffects::RenderDebug()
{
	USE_MEMBUCKET(MEMBUCKET_FX);
	BANK_ONLY(if (sm_disableVisualEffects) { return; })

	PF_START_TIMEBAR_DETAIL("Decals - Debug");
	g_decalMan.RenderDebug();

	PF_START_TIMEBAR_DETAIL("Vfx Trail Debug");
	g_vfxTrail.RenderDebug();

	PF_START_TIMEBAR_DETAIL("Checkpoints Debug");
	g_checkpoints.RenderDebug();

// 	PF_START_TIMEBAR_DETAIL("Marker Debug");
// 	g_markers.RenderDebug();

	PF_START_TIMEBAR_DETAIL("Movie Manager 3D Debug");
	g_movieMgr.RenderDebug3D();

}
#endif


//
// name:	InitWidgets
// desc:
#if __BANK
PARAM(rag_visualeffects,"rag_visualeffects");

void CVisualEffects::InitWidgets()
{
	USE_MEMBUCKET(MEMBUCKET_FX);
	FX_MEM_TOTAL("INITWIDGETS - Entering");

	vfxWidget::Init();
	FX_MEM_CURR("Vfx Widget");

	if (PARAM_rag_visualeffects.Get())
	{
		CreateWidgets();
	}
	else
	{
		s_pCreateButton = vfxWidget::GetBank()->AddButton("Create Vfx Widgets", CreateWidgets);
	}
	if (PARAM_smash_tty_windows.Get())
	{
		BANKMGR.CreateOutputWindow("smash", "NamedColor:Green"); 
	}

	vfxChannel::Init();

}
#endif


//
// name:	CreateWidgets
// desc:
#if __BANK
void CVisualEffects::CreateWidgets()
{
	bkBank* pBank = vfxWidget::GetBank();
	if (pBank==NULL)
	{
		return;
	}

	if( s_pCreateButton )
		s_pCreateButton->Destroy();

	// add widgets to the main systems
	g_ptFxManager.InitWidgets();
	FX_MEM_CURR("Particles");
	g_vfxWeather.GetPtFxGPUManager().InitWidgets();		// init the gpu widgets once the gpu weather data is loaded in
	FX_MEM_CURR("GPU Particles");
	g_decalMan.InitWidgets();
	FX_MEM_CURR("Decals");
	g_vehicleGlassMan.InitWidgets();
	FX_MEM_CURR("Smashables");

	// initialise environment systems
	g_sky.InitWidgets();
	FX_MEM_CURR("Sky");

	CClouds::InitWidgets();
	FX_MEM_CURR("Clouds");

	// add widgets for vfx systems
	g_vfxBlood.InitWidgets();
	FX_MEM_CURR("Vfx Blood");
	g_vfxEntity.InitWidgets();
	FX_MEM_CURR("Vfx Entity");
	g_vfxExplosion.InitWidgets();
	FX_MEM_CURR("Vfx Explosion");
	g_vfxFire.InitWidgets();
	FX_MEM_CURR("Vfx Fire");
	g_vfxGadget.InitWidgets();
	FX_MEM_CURR("Vfx Gadget");
	g_vfxGlass.InitWidgets();
	FX_MEM_CURR("Vfx Glass");
	g_vfxLens.InitWidgets();
	FX_MEM_CURR("Vfx Lens");
	g_vfxScript.InitWidgets();
	FX_MEM_CURR("Vfx Script");
	g_vfxTrail.InitWidgets();
	FX_MEM_CURR("Vfx Trail");
	g_vfxMaterial.InitWidgets();
	FX_MEM_CURR("Vfx Material");
	g_vfxLiquid.InitWidgets();
	FX_MEM_CURR("Vfx Liquid");
	g_vfxLightningManager.InitWidgets();
	FX_MEM_CURR("Vfx Lightning");
	g_vfxPed.InitWidgets();
	FX_MEM_CURR("Vfx Ped");
	g_vfxProjectile.InitWidgets();
	FX_MEM_CURR("Vfx Projectile");
	g_vfxVehicle.InitWidgets();
	FX_MEM_CURR("Vfx Vehicle");
	g_vfxWater.InitWidgets();
	FX_MEM_CURR("Vfx Water");
	g_vfxWeapon.InitWidgets();
	FX_MEM_CURR("Vfx Weapon");
	g_vfxWeather.InitWidgets();
	FX_MEM_CURR("Vfx Weather");
	g_vfxWheel.InitWidgets();
	FX_MEM_CURR("Vfx Wheel");

	// add widgets for misc vfx systems
	g_brightLights.InitWidgets();
	FX_MEM_CURR("Bright Lights");
	g_checkpoints.InitWidgets();	
	FX_MEM_CURR("Checkpoints");
	g_coronas.InitWidgets();
	FX_MEM_CURR("Coronas");
	g_fogVolumeMgr.InitWidgets();
	FX_MEM_CURR("Fog Volumes");
	g_distantLights.InitWidgets();	
	FX_MEM_CURR("Distant Lights");
	g_fireMan.InitWidgets();
	FX_MEM_CURR("Fire");
	g_markers.InitWidgets();
	FX_MEM_CURR("Markers");
	GameGlows::InitWidgets();
	FX_MEM_CURR("GameGlows");
	ScriptIM::InitWidgets();
	FX_MEM_CURR("ScriptIM");
	g_movieMeshMgr.InitWidgets();
	FX_MEM_CURR("Movie Meshes");
	g_movieMgr.InitWidgets();
	FX_MEM_CURR("Movies");
	g_scrollbars.InitWidgets();
	FX_MEM_CURR("Scrollbars");
	g_waterCannonMan.InitWidgets();
	FX_MEM_CURR("Water Cannon");
	g_LensFlare.InitWidgets();
	FX_MEM_CURR("Lens Flare");

	FX_MEM_TOTAL("INITWIDGETS - Leaving");
}
#endif


