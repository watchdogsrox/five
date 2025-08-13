///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	PtFxGPUManager.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	23 November 2009
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "PtFxGPUManager.h"

// rage 
#include "grcore/allocscope.h"
#include "rmptfx/ptxdrawcommon.h"

// framework
#include "vector/colors.h"
#include "vfx/vfxutil.h"
#include "vfx/vfxwidget.h"

// game
#include "Camera/CamInterface.h"
#include "Camera/gameplay/GameplayDirector.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "Game/Weather.h"
#include "Peds/ped.h"
#include "Peds/PopCycle.h"
#include "Peds/PlayerInfo.h"
#include "Peds/PlayerSpecialAbility.h"
#include "Renderer/Lights/Lights.h"
#include "Renderer/PostProcessFX.h"
#include "Renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "Scene/Portals/PortalTracker.h"
#include "Scene/World/GameWorld.h"
#include "TimeCycle/TimeCycleConfig.h"
#include "vfx/vfx_shared.h"
#include "Vfx/Metadata/VfxRegionInfo.h"
#include "vfx/particles/PtFxManager.h"
#include "Vfx/Systems/VfxWeather.h"
#include "Vfx/VfxHelper.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_PTFX_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

// Avoid updates with very small time delta - those won't work well because of the velocity/position half float precision
bank_float g_fMinUpdateDT = 0.009f;
bank_bool g_bClampDT = false;

dev_float GPUPTFX_REGION_DROPMIST_FADE_HEIGHT_MIN			= 10.0f;
dev_float GPUPTFX_REGION_DROPMIST_FADE_HEIGHT_MAX			= 25.0f;
dev_float GPUPTFX_REGION_GROUND_FADE_HEIGHT_MIN				= 10.0f;
dev_float GPUPTFX_REGION_GROUND_FADE_HEIGHT_MAX				= 25.0f;

dev_float GPUPTFX_SLOWDOWN_ABILITY_RAIN_DROP_WIDTH_SCALE	= 2.0f;
dev_float GPUPTFX_SLOWDOWN_ABILITY_RAIN_DROP_HEIGHT_SCALE	= 0.5f;

dev_float GPUPTFX_SLOWDOWN_ABILITY_TIME_MULT				= 0.25f;

grcDepthStencilStateHandle	m_VehicleInteriorAlphaDepthStencilState;

#if RAIN_UPDATE_CPU && !__FINAL
	PARAM(CpuRainDisable, "Force disable CpuRainUpdate on multi GPU machines");
	PARAM(CpuRainEnable,  "Force enable CpuRainUpdate on single GPU machines");
#endif

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CPtFxGPURenderSetup
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  PreDraw
///////////////////////////////////////////////////////////////////////////////

bool			CPtFxGPURenderSetup::PreDraw				(const ptxEffectInst*, const ptxEmitterInst*, Vec3V_In UNUSED_PARAM(vMin), Vec3V_In UNUSED_PARAM(vMax), u32 UNUSED_PARAM(shaderHashName), grmShader* UNUSED_PARAM(pShader)) const
{
	return true;
}


///////////////////////////////////////////////////////////////////////////////
//  PostDraw
///////////////////////////////////////////////////////////////////////////////

bool			CPtFxGPURenderSetup::PostDraw				(const ptxEffectInst*, const ptxEmitterInst*) const
{
	Lights::ResetLightingGlobals();
	return true;
}



///////////////////////////////////////////////////////////////////////////////
//  CLASS CPtFxGPUManager
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

bool			CPtFxGPUManager::Init						()
{
#if RAIN_UPDATE_CPU
	bool bCpuRain = false;
	
	// enable RainUpdateCpu on multiGPU machines:
	if(grcDevice::GetGPUCount() > 1)
	{
		bCpuRain = true;
	}

	#if !__FINAL
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

	// initialise the gpu systems
	ptxgpuBase::InitRenderStateBlocks();

	m_dropSystem.Init(DROP_SYSTEM_ID, 128*128, "ptxgpu_update", "draw_drop", "ptxgpu_render", "draw", false, NULL);
	FX_MEM_CURR("GPU Drop System");
	m_mistSystem.Init(MIST_SYSTEM_ID, 16*16, "ptxgpu_update", "draw_drop", "ptxgpu_render", "draw_soft", true, NULL);
	FX_MEM_CURR("GPU Mist System");
	m_groundSystem.Init(GROUND_SYSTEM_ID, 64*64, "ptxgpu_update", "draw_ground", "ptxgpu_render", "draw_soft", true, NULL);
	FX_MEM_CURR("GPU Ground System");

	// set up the default light
	m_vDefaultLight.SetXf(g_visualSettings.GetV<float>(atHashWithStringDev("rain.defaultlight.red", 0xe3b83915), 0.5f));
	m_vDefaultLight.SetYf(g_visualSettings.GetV<float>(atHashWithStringDev("rain.defaultlight.green", 0xac156643), 0.5f));
	m_vDefaultLight.SetZf(g_visualSettings.GetV<float>(atHashWithStringDev("rain.defaultlight.blue", 0xc6b94b3d), 0.5f));
	m_vDefaultLight.SetWf(g_visualSettings.GetV<float>(atHashWithStringDev("rain.defaultlight.alpha", 0x310ed7e), 0.5f));

	// set the global rendering setup
	m_ptxRenderSetup = rage_new CPtFxGPURenderSetup();

	m_wasUnderwaterLastFrame = false;

	m_wasWeatherDropSystemActiveLastFrame = false;
	m_wasWeatherMistSystemActiveLastFrame = false;
	m_wasWeatherGroundSystemActiveLastFrame = false;

	m_wasRegionDropSystemActiveLastFrame = false;
	m_wasRegionMistSystemActiveLastFrame = false;
	m_wasRegionGroundSystemActiveLastFrame = false;

	m_resetDropSystem = true;
	m_resetMistSystem = true;
	m_resetGroundSystem = true;

	m_deltaTime = 0.0f;

	m_groundPosZ[0] = 0.0f;
	m_groundPosZ[1] = 0.0f;
	m_camPosZ[0] = 0.0f;
	m_camPosZ[1] = 0.0f;
	m_isPedShelteredInVehicle[0] = false;
	m_isPedShelteredInVehicle[1] = false;
	grcDepthStencilStateDesc depthStencilDesc;
	depthStencilDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.StencilReadMask = DEFERRED_MATERIAL_INTERIOR_VEH;
	depthStencilDesc.StencilWriteMask = 0xf0;
	depthStencilDesc.BackFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	depthStencilDesc.BackFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	depthStencilDesc.BackFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = grcRSV::STENCILOP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	depthStencilDesc.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	depthStencilDesc.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = grcRSV::STENCILOP_KEEP;

	depthStencilDesc.DepthWriteMask = 0;
	depthStencilDesc.DepthEnable = 1;
	m_VehicleInteriorAlphaDepthStencilState = grcStateBlock::CreateDepthStencilState(depthStencilDesc);

#if APPLY_DOF_TO_GPU_PARTICLES
	m_BlendStateDof = grcStateBlock::BS_Invalid;

#if RSG_PC
	if (GRCDEVICE.GetDxFeatureLevel() > 1000)
#endif // RSG_PC
	{
		grcBlendStateDesc DefaultBlendStateBlockDesc;
		PostFX::SetupBlendForDOF(DefaultBlendStateBlockDesc, false);
		m_BlendStateDof = grcStateBlock::CreateBlendState(DefaultBlendStateBlockDesc);
	}
#endif //APPLY_DOF_TO_GPU_PARTICLES

#if __BANK
	m_pBankGroup = NULL;
	m_pRegionCombo = NULL;
	m_currRegionComboId = 0;
	m_forceRegionGpuPtFx = false;
	m_debugWindInfluence = -0.1f;
	m_debugGravity = -50.1f;
	m_disableRendering = false;
	m_debugRender = false;

	m_debugGpuFxPrevDropIndex = -1;
	m_debugGpuFxPrevMistIndex = -1;
	m_debugGpuFxPrevGroundIndex = -1;
	m_debugGpuFxNextDropIndex = -1;
	m_debugGpuFxNextMistIndex = -1;
	m_debugGpuFxNextGroundIndex = -1;
	m_debugCurrVfxRegionDropLevel = 0.0f;
	m_debugCurrVfxRegionMistLevel = 0.0f;
	m_debugCurrVfxRegionGroundLevel = 0.0f;

	m_camAngleForwardMin = PTXGPU_CAMANGLE_FORWARD_MIN;
	m_camAngleForwardMax = PTXGPU_CAMANGLE_FORWARD_MAX;
	m_camAngleUpMin = PTXGPU_CAMANGLE_UP_MIN;
	m_camAngleUpMax = PTXGPU_CAMANGLE_UP_MAX;

	m_useOverrideSettings = false;
	m_overrideCurrLevel = 1.0f;
	m_overrideBoxSize = Vec3V(V_FIVE);
	m_overrideBoxOffset = Vec3V(V_ZERO);
#endif

	return true;
}


///////////////////////////////////////////////////////////////////////////////
//  LoadSetting
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPUManager::LoadSettingFromXmlFile		(const CWeatherGpuFxFromXmlFile &WeatherGpuFxFromXmlFile)
{
	switch (WeatherGpuFxFromXmlFile.m_SystemType)
	{
		case SYSTEM_TYPE_DROP:
			m_dropSystem.LoadFromXmlFile(WeatherGpuFxFromXmlFile);
			break;
		case SYSTEM_TYPE_MIST:
			m_mistSystem.LoadFromXmlFile(WeatherGpuFxFromXmlFile);
			break;
		case SYSTEM_TYPE_GROUND:
			m_groundSystem.LoadFromXmlFile(WeatherGpuFxFromXmlFile);
			break;
		default:
			break;
	}	
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPUManager::Shutdown					()
{
	delete m_ptxRenderSetup;

	m_dropSystem.Shutdown();
	m_mistSystem.Shutdown();
	m_groundSystem.Shutdown();
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPUManager::Update						(float deltaTime)
{	
	// hack for when it's raining and a slowdown special ability is active
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if (pPed && pPed->GetSpecialAbility() && pPed->GetSpecialAbility()->IsActive() && 
		(pPed->GetSpecialAbility()->GetType()==SAT_CAR_SLOWDOWN || pPed->GetSpecialAbility()->GetType()==SAT_BULLET_TIME))
	{
		deltaTime *= GPUPTFX_SLOWDOWN_ABILITY_TIME_MULT;
	}

	// Store the current frame time for use in the the pre-render
	// Since we use half floats for velocity and position, we get precision issues when the time delta goes below a small value
	// To avoid this issue we clamp the delta to the minimum value that works correctly
	m_deltaTime = g_bClampDT ? Max(g_fMinUpdateDT, deltaTime) : deltaTime;

	// init some flags to keep track of the systems
	bool isWeatherDropSystemActiveThisFrame = false;
	bool isWeatherMistSystemActiveThisFrame = false;
	bool isWeatherGroundSystemActiveThisFrame = false;

	bool isRegionDropSystemActiveThisFrame = false;
	bool isRegionMistSystemActiveThisFrame = false;
	bool isRegionGroundSystemActiveThisFrame = false;

	// init the current system indices
	s32 gpuFxPrevDropIndex = -1;
	s32 gpuFxPrevMistIndex = -1;
	s32 gpuFxPrevGroundIndex = -1;
	s32 gpuFxNextDropIndex = -1;
	s32 gpuFxNextMistIndex = -1;
	s32 gpuFxNextGroundIndex = -1;

	//Caching ground Pos Z and cam Pos Z for the render thread
	const s32 bufferID = gRenderThreadInterface.GetUpdateBuffer();

	bool foundGround = false;
	CPed* pPlayerPed = FindPlayerPed();
	Vec3V vPlayerPos = pPlayerPed->GetTransform().GetPosition();
	m_groundPosZ[bufferID] = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, vPlayerPos.GetXf(), vPlayerPos.GetYf(), vPlayerPos.GetZf(), &foundGround);
	m_camPosZ[bufferID] = camInterface::GetPos().z;

	m_isPedShelteredInVehicle[bufferID] = false;
	CPed* pFollowCameraPed = const_cast<CPed*>(camInterface::GetGameplayDirector().GetFollowPed());
	if (pFollowCameraPed)
	{
		m_isPedShelteredInVehicle[bufferID] = pFollowCameraPed->GetIsShelteredInVehicle();
	}

	// if we're underwater we need to play these systems
	bool isUnderwaterThisFrame = CVfxHelper::GetGameCamWaterDepth()>0.0f;
	if (isUnderwaterThisFrame)
	{
		gpuFxPrevDropIndex = g_vfxWeather.GetPtFxGPUManager().GetDropSettingIndex(atHashWithStringDev("UNDERWATER_DROP", 0xC11D1EC6));
		gpuFxPrevMistIndex = -1;
		gpuFxPrevGroundIndex = -1;
		gpuFxNextDropIndex = gpuFxPrevDropIndex;
		gpuFxNextMistIndex = -1;
		gpuFxNextGroundIndex = -1;
	}
	else
	{
		// we're not underwater - set the systems depending on the current weather
		gpuFxPrevDropIndex = g_weather.GetPrevType().m_gpuFxDropIndex;
		gpuFxPrevMistIndex = g_weather.GetPrevType().m_gpuFxMistIndex;
		gpuFxPrevGroundIndex = g_weather.GetPrevType().m_gpuFxGroundIndex;
		gpuFxNextDropIndex = g_weather.GetNextType().m_gpuFxDropIndex;
		gpuFxNextMistIndex = g_weather.GetNextType().m_gpuFxMistIndex;
		gpuFxNextGroundIndex = g_weather.GetNextType().m_gpuFxGroundIndex;

		isWeatherDropSystemActiveThisFrame = gpuFxPrevDropIndex>-1 || gpuFxNextDropIndex>-1;
		isWeatherMistSystemActiveThisFrame = gpuFxPrevMistIndex>-1 || gpuFxNextMistIndex>-1;
		isWeatherGroundSystemActiveThisFrame = gpuFxPrevGroundIndex>-1 || gpuFxNextGroundIndex>-1;

		// check overtime interpolation
		u32 forceOverTimeType = g_weather.GetOverTimeType();
		if (forceOverTimeType!=0xffffffff)
		{
			const CWeatherType& forcedWeatherType = g_weather.GetTypeInfo(forceOverTimeType);

			isWeatherDropSystemActiveThisFrame |= forcedWeatherType.m_gpuFxDropIndex>-1;
			isWeatherMistSystemActiveThisFrame |= forcedWeatherType.m_gpuFxMistIndex>-1;
			isWeatherGroundSystemActiveThisFrame |= forcedWeatherType.m_gpuFxGroundIndex>-1;
		}
	}

	// regional systems (can only be active if underwater and weather systems aren't)
	bool isRegionGpuPtFxActive = !isUnderwaterThisFrame &&
								 !isWeatherDropSystemActiveThisFrame && 
								 !isWeatherMistSystemActiveThisFrame && 
								 !isWeatherGroundSystemActiveThisFrame && 
								 g_weather.GetWetness()==0.0f &&
								 !CVfxHelper::HasCameraSwitched(true, true);
#if __BANK
	if (m_forceRegionGpuPtFx)
	{
		g_vfxRegionInfoMgr.UpdateGpuPtFx(isRegionGpuPtFxActive, m_currRegionComboId);
	}
	else
#endif
	{
		g_vfxRegionInfoMgr.UpdateGpuPtFx(isRegionGpuPtFxActive, -1);
	}

	// get the active vfx region
	float maxVfxRegionDropLevel = 0.0f;
	float currVfxRegionDropLevel = 0.0f;
	float maxVfxRegionMistLevel = 0.0f;
	float currVfxRegionMistLevel = 0.0f;
	float maxVfxRegionGroundLevel = 0.0f;
	float currVfxRegionGroundLevel = 0.0f;
	CVfxRegionInfo* pActiveVfxRegionInfo = g_vfxRegionInfoMgr.GetActiveVfxRegionInfo();
	if (pActiveVfxRegionInfo)
	{
		// get the active vfx region gpu ptfx infos
		CVfxRegionGpuPtFxInfo* pActiveVfxRegionGpuPtFxDropInfo = pActiveVfxRegionInfo->GetActiveVfxRegionGpuPtFxDropInfo();
		if (pActiveVfxRegionGpuPtFxDropInfo)
		{
			maxVfxRegionDropLevel = pActiveVfxRegionGpuPtFxDropInfo->GetGpuPtFxMaxLevel();
			currVfxRegionDropLevel = pActiveVfxRegionGpuPtFxDropInfo->GetGpuPtFxCurrLevel();
			if (currVfxRegionDropLevel>0.0f)
			{
				gpuFxPrevDropIndex = g_vfxWeather.GetPtFxGPUManager().GetDropSettingIndex(pActiveVfxRegionGpuPtFxDropInfo->GetGpuPtFxName().GetHash());
				gpuFxNextDropIndex = gpuFxPrevDropIndex;

				isRegionDropSystemActiveThisFrame = gpuFxPrevDropIndex>-1;
			}
		}

		CVfxRegionGpuPtFxInfo* pActiveVfxRegionGpuPtFxMistInfo = pActiveVfxRegionInfo->GetActiveVfxRegionGpuPtFxMistInfo();
		if (pActiveVfxRegionGpuPtFxMistInfo)
		{
			maxVfxRegionMistLevel = pActiveVfxRegionGpuPtFxMistInfo->GetGpuPtFxMaxLevel();
			currVfxRegionMistLevel = pActiveVfxRegionGpuPtFxMistInfo->GetGpuPtFxCurrLevel();
			if (currVfxRegionMistLevel>0.0f)
			{
				gpuFxPrevMistIndex = g_vfxWeather.GetPtFxGPUManager().GetMistSettingIndex(pActiveVfxRegionGpuPtFxMistInfo->GetGpuPtFxName().GetHash());
				gpuFxNextMistIndex = gpuFxPrevMistIndex;

				isRegionMistSystemActiveThisFrame = gpuFxPrevMistIndex>-1;
			}
		}

		CVfxRegionGpuPtFxInfo* pActiveVfxRegionGpuPtFxGroundInfo = pActiveVfxRegionInfo->GetActiveVfxRegionGpuPtFxGroundInfo();
		if (pActiveVfxRegionGpuPtFxGroundInfo)
		{
			maxVfxRegionGroundLevel = pActiveVfxRegionGpuPtFxGroundInfo->GetGpuPtFxMaxLevel();
			currVfxRegionGroundLevel = pActiveVfxRegionGpuPtFxGroundInfo->GetGpuPtFxCurrLevel();
			if (currVfxRegionGroundLevel>0.0f)
			{
				gpuFxPrevGroundIndex = g_vfxWeather.GetPtFxGPUManager().GetGroundSettingIndex(pActiveVfxRegionGpuPtFxGroundInfo->GetGpuPtFxName().GetHash());
				gpuFxNextGroundIndex = gpuFxPrevGroundIndex;

				isRegionGroundSystemActiveThisFrame = gpuFxPrevGroundIndex>-1;
			}
		}

		// region based drop and mist effects should only be active close to ground level
		float camHeight = g_vfxWeather.GetCamHeightAboveGround();
		float dropMistLevelMult = CVfxHelper::GetInterpValue(camHeight, GPUPTFX_REGION_DROPMIST_FADE_HEIGHT_MAX, GPUPTFX_REGION_DROPMIST_FADE_HEIGHT_MIN);
		float groundLevelMult = CVfxHelper::GetInterpValue(camHeight, GPUPTFX_REGION_GROUND_FADE_HEIGHT_MAX, GPUPTFX_REGION_GROUND_FADE_HEIGHT_MIN);
		currVfxRegionDropLevel *= dropMistLevelMult;
		currVfxRegionMistLevel *= dropMistLevelMult;
		currVfxRegionGroundLevel *= groundLevelMult;
	}

	// reset if just gone underwater or out of water
	if (isUnderwaterThisFrame != m_wasUnderwaterLastFrame)
	{
		m_resetDropSystem = true;
		m_resetMistSystem = true;
		m_resetGroundSystem = true;
		m_wasUnderwaterLastFrame = isUnderwaterThisFrame;
	}

	// reset if weather or region systems have swapped state
	if (m_wasWeatherDropSystemActiveLastFrame!=isWeatherDropSystemActiveThisFrame ||
		m_wasRegionDropSystemActiveLastFrame!=isRegionDropSystemActiveThisFrame)
	{
		m_resetDropSystem = true;
	}

	if (m_wasWeatherMistSystemActiveLastFrame != isWeatherMistSystemActiveThisFrame ||
		m_wasRegionMistSystemActiveLastFrame!=isRegionMistSystemActiveThisFrame)
	{
		m_resetMistSystem = true;
	}

	if (m_wasWeatherGroundSystemActiveLastFrame != isWeatherGroundSystemActiveThisFrame ||
		m_wasRegionGroundSystemActiveLastFrame!=isRegionGroundSystemActiveThisFrame)
	{
		m_resetGroundSystem = true;
	}

	// store the state
	m_wasWeatherDropSystemActiveLastFrame = isWeatherDropSystemActiveThisFrame;
	m_wasWeatherMistSystemActiveLastFrame = isWeatherMistSystemActiveThisFrame;
	m_wasWeatherGroundSystemActiveLastFrame = isWeatherGroundSystemActiveThisFrame;
	m_wasRegionDropSystemActiveLastFrame = isRegionDropSystemActiveThisFrame;
	m_wasRegionMistSystemActiveLastFrame = isRegionMistSystemActiveThisFrame;
	m_wasRegionGroundSystemActiveLastFrame = isRegionGroundSystemActiveThisFrame;

#if __BANK
	//store these so that if game is paused we can still update the render settings
	m_debugGpuFxPrevDropIndex = gpuFxPrevDropIndex;
	m_debugGpuFxPrevMistIndex = gpuFxPrevMistIndex;
	m_debugGpuFxPrevGroundIndex = gpuFxPrevGroundIndex;
	m_debugGpuFxNextDropIndex = gpuFxNextDropIndex;
	m_debugGpuFxNextMistIndex = gpuFxNextMistIndex;
	m_debugGpuFxNextGroundIndex = gpuFxNextGroundIndex;
	m_debugCurrVfxRegionDropLevel = currVfxRegionDropLevel;
	m_debugCurrVfxRegionMistLevel = currVfxRegionMistLevel;
	m_debugCurrVfxRegionGroundLevel = currVfxRegionGroundLevel;
#endif

	// update the systems
	m_dropSystem.Update(gpuFxPrevDropIndex, gpuFxNextDropIndex, g_weather.GetInterp(), maxVfxRegionDropLevel, currVfxRegionDropLevel);
	m_mistSystem.Update(gpuFxPrevMistIndex, gpuFxNextMistIndex, g_weather.GetInterp(), maxVfxRegionMistLevel, currVfxRegionMistLevel);
	m_groundSystem.Update(gpuFxPrevGroundIndex, gpuFxNextGroundIndex, g_weather.GetInterp(), maxVfxRegionGroundLevel, currVfxRegionGroundLevel);

	// Apply overtime interpolation
	u32 forceOverTimeType = g_weather.GetOverTimeType();
	if( forceOverTimeType != 0xffffffff )
	{
		const CWeatherType& forcedWeatherType = g_weather.GetTypeInfo(forceOverTimeType);

		s32 forcedFxPrevDropIndex = forcedWeatherType.m_gpuFxDropIndex;
		if( forcedFxPrevDropIndex != -1 )
			m_dropSystem.Interpolate(forcedFxPrevDropIndex, g_weather.GetOverTimeInterp());
			
		s32 forcedFxPrevMistIndex = forcedWeatherType.m_gpuFxMistIndex;
		if( forcedFxPrevMistIndex != -1 )
			m_mistSystem.Interpolate(forcedFxPrevMistIndex, g_weather.GetOverTimeInterp());

		s32 forcedFxPrevGroundIndex = forcedWeatherType.m_gpuFxGroundIndex;
		if( forcedFxPrevGroundIndex != -1 )
			m_groundSystem.Interpolate(forcedFxPrevGroundIndex, g_weather.GetOverTimeInterp());
	}
	
#if __BANK
	if (m_debugWindInfluence>=0.0f)
	{
		m_dropSystem.m_currWindInfluence = m_debugWindInfluence;
		m_mistSystem.m_currWindInfluence = m_debugWindInfluence;
		m_groundSystem.m_currWindInfluence = m_debugWindInfluence;
	}

	if (m_debugGravity>=-50.0f)
	{
		m_dropSystem.m_currGravity = m_debugGravity;
		m_mistSystem.m_currGravity = m_debugGravity;
		m_groundSystem.m_currGravity = m_debugGravity;
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  PreRender
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPUManager::PreRender					(bool bGamePaused, bool resetDropSystem, bool resetMistSystem, bool resetGroundSystem)
{
	if (g_timeCycle.GetStaleNoGpuFX()>0.0f)
	{
		return;
	}

	CPtFxGPUManager& ptFxGPUManager = g_vfxWeather.GetPtFxGPUManager();

	// don't pre render if the game is paused
	if (bGamePaused)
	{
		CPtFxGPUManager& ptFxGPUManager = g_vfxWeather.GetPtFxGPUManager();
		ptFxGPUManager.m_dropSystem.PauseSimulation();
		ptFxGPUManager.m_mistSystem.PauseSimulation();
		ptFxGPUManager.m_groundSystem.PauseSimulation();
		return;
	}

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	// needs to be called from the render thread to be able to access texture data
	PF_PUSH_TIMEBAR("CPtFxGPUSystem::GlobalPreUpdate()");
	CPtFxGPUSystem::GlobalPreUpdate();
	PF_POP_TIMEBAR();

	// store the current viewport for use in the the pre render
	ptFxGPUManager.m_viewport = *grcViewport::GetCurrent();

	// Call pre-render for each individual system
	PF_PUSH_TIMEBAR("m_dropSystem");
	ptFxGPUManager.m_dropSystem.PreRender(ptFxGPUManager.m_viewport, ptFxGPUManager.m_deltaTime, resetDropSystem);
	PF_POP_TIMEBAR();

	PF_PUSH_TIMEBAR("m_mistSystem");
	ptFxGPUManager.m_mistSystem.PreRender(ptFxGPUManager.m_viewport, ptFxGPUManager.m_deltaTime, resetMistSystem);
	PF_POP_TIMEBAR();

	PF_PUSH_TIMEBAR("m_groundSystem");
	ptFxGPUManager.m_groundSystem.PreRender(ptFxGPUManager.m_viewport, ptFxGPUManager.m_deltaTime, resetGroundSystem);
	PF_POP_TIMEBAR();
}


///////////////////////////////////////////////////////////////////////////////
//  Render
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPUManager::Render						(Vec3V_ConstRef vCamVel)
{	
	if (g_timeCycle.GetCurrNoGpuFX()>0.0f)
	{
		return;
	}

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	// Reset world matrix
	grcViewport::SetCurrentWorldIdentity();

#if __BANK
	if (m_disableRendering) 
	{
		return;
	}

	if (m_debugRender)
	{
		m_dropSystem.RenderDebug(m_viewport, Color32(1.0f, 0.0f, 0.0f, 0.25f));
		m_mistSystem.RenderDebug(m_viewport, Color32(0.0f, 1.0f, 0.0f, 0.25f));
		m_groundSystem.RenderDebug(m_viewport, Color32(0.0f, 0.0f, 1.0f, 0.25f));
	}
#endif
	
	//Ground Pos Z and cam pos Z is cached in CPtFxGPUManager::Update
	const s32 bufferID = gRenderThreadInterface.GetRenderBuffer();
	const float groundPosZ = m_groundPosZ[bufferID];
	const float camPosZ = m_camPosZ[bufferID];

	Lights::SetupDirectionalAndAmbientGlobals();

	CRenderPhaseCascadeShadowsInterface::SetDeferredLightingShaderVars();
	CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars();

	// Let's set a lower resolution shadow map to reduce the amount of flickering
	// Only if we've already generating it during PostFX::ProcessDepthFX. The assumption is that if fog ray is enabled, Post FX would have this generated before we reach this call
	// If the generation of the mini map changes, we should change this logic accordingly. Fallback option is always using the larger one.
	// cascaded shadow map which is set above
	
	if(PostFX::GetFogRayMiniShadowMapGenerated()) // This is now checked in a thread safe manner
	{
		CRenderPhaseCascadeShadowsInterface::SetSharedShadowMap(CRenderPhaseCascadeShadowsInterface::GetShadowMapMini(SM_MINI_FOGRAY));
	}

	int backGroundBlurLevel = Max(Max(m_dropSystem.GetBackGroundBlurLevel(), m_mistSystem.GetBackGroundBlurLevel()), m_groundSystem.GetBackGroundBlurLevel());
	if(backGroundBlurLevel > -1)
	{
		//add special logic for unlock/lock of targets based on whether particle dof maps are used
		if (LOCK_DOF_TARGETS_FOR_VFX && CPtFxManager::UseParticleDOF())
		{
			g_ptFxManager.UnLockPtfxDepthAlphaMap();
			PostFX::DownSampleBackBuffer(backGroundBlurLevel);
			g_ptFxManager.LockPtfxDepthAlphaMap(GRCDEVICE.IsReadOnlyDepthAllowed());
		}
		else
		{
			CRenderTargets::UnlockSceneRenderTargets();
			PostFX::DownSampleBackBuffer(backGroundBlurLevel);
			GRCDEVICE.IsReadOnlyDepthAllowed() ? CRenderTargets::LockSceneRenderTargets_DepthCopy() : CRenderTargets::LockSceneRenderTargets();
		}
	}

	// Prepare the global shader variables
	ptxShaderTemplateList::SetGlobalVars();

#if __BANK
	const Vec4V vCamAngleLimits = Vec4V(
		m_camAngleForwardMin, 
		1.0f/(m_camAngleForwardMax - m_camAngleForwardMin),
		m_camAngleUpMin,
		1.0f/(m_camAngleUpMax - m_camAngleUpMin));
#endif

	// if we are rendering late interior alpha, then we enable the stencil test.
	// but if the ped is not sheltered in the vehicle, like when the convertible top is down, then disable the stencil
	grcDepthStencilStateHandle backupDSS = grcStateBlock::DSS_Active;
	grcDepthStencilStateHandle backupPtxGPUDSS = ptxgpuBase::GetDepthStencilState();
	if(CRenderPhaseDrawSceneInterface::GetLateVehicleInterAlphaState_Render() && m_isPedShelteredInVehicle[bufferID])
	{
		ptxgpuBase::SetDepthStencilState(m_VehicleInteriorAlphaDepthStencilState);
	}

#if APPLY_DOF_TO_GPU_PARTICLES
	if (CPtFxManager::UseParticleDOF())
		grcStateBlock::SetBlendState(m_BlendStateDof);
#endif //APPLY_DOF_TO_GPU_PARTICLES

	m_dropSystem.Render(m_viewport, m_ptxRenderSetup, vCamVel, camPosZ, groundPosZ BANK_ONLY(, vCamAngleLimits));

#if APPLY_DOF_TO_GPU_PARTICLES
	if (CPtFxManager::UseParticleDOF())
		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
#endif //APPLY_DOF_TO_GPU_PARTICLES

	m_mistSystem.Render(m_viewport, m_ptxRenderSetup, vCamVel, camPosZ, groundPosZ BANK_ONLY(, vCamAngleLimits));
	m_groundSystem.Render(m_viewport, m_ptxRenderSetup, vCamVel, camPosZ, groundPosZ BANK_ONLY(, vCamAngleLimits));

	// Reset previous state back

	CRenderPhaseCascadeShadowsInterface::RestoreSharedShadowMap();
	grcStateBlock::SetDepthStencilState(backupDSS);
	ptxgpuBase::SetDepthStencilState(backupPtxGPUDSS);
}

void			CPtFxGPUManager::GetEmittersBounds		(Mat44V_ConstRef camMatrix, Vec3V_Ref emitterMin, Vec3V_Ref emitterMax)
{
	Vec3V tempMin, tempMax;
	if(m_dropSystem.GetEmitterBounds(camMatrix, tempMin, tempMax))
	{
		emitterMin = Min(tempMin, emitterMin);
		emitterMax = Max(tempMax, emitterMax);
	}
	if(m_mistSystem.GetEmitterBounds(camMatrix, tempMin, tempMax))
	{
		emitterMin = Min(tempMin, emitterMin);
		emitterMax = Max(tempMax, emitterMax);
	}
	if(m_groundSystem.GetEmitterBounds(camMatrix, tempMin, tempMax))
	{
		emitterMin = Min(tempMin, emitterMin);
		emitterMax = Max(tempMax, emitterMax);
	}
}

///////////////////////////////////////////////////////////////////////////////
//  SetUseOverrideSettings
///////////////////////////////////////////////////////////////////////////////
void			CPtFxGPUManager::SetUseOverrideSettings		(bool bUseOverrideSettings)
{
	//Let all use the override settings
	m_dropSystem.SeUseOverrideSettings(bUseOverrideSettings);
	m_groundSystem.SeUseOverrideSettings(bUseOverrideSettings);
	m_mistSystem.SeUseOverrideSettings(bUseOverrideSettings);

}

///////////////////////////////////////////////////////////////////////////////
//  SetOverrideCurrLevel
///////////////////////////////////////////////////////////////////////////////
void			CPtFxGPUManager::SetOverrideCurrLevel		(float overrideCurrLevel)
{
	//Let all use the override settings
	m_dropSystem.SetOverrideCurrLevel(overrideCurrLevel);
	m_groundSystem.SetOverrideCurrLevel(overrideCurrLevel);
	m_mistSystem.SetOverrideCurrLevel(overrideCurrLevel);

}

///////////////////////////////////////////////////////////////////////////////
//  SetOverrideBoxSize
///////////////////////////////////////////////////////////////////////////////
void			CPtFxGPUManager::SetOverrideBoxSize			(Vec3V_In overrideBoxSize)
{
	//Let all use the override settings
	m_dropSystem.SetOverrideBoxSize(overrideBoxSize);
	m_groundSystem.SetOverrideBoxSize(overrideBoxSize);
	m_mistSystem.SetOverrideBoxSize(overrideBoxSize);

}

///////////////////////////////////////////////////////////////////////////////
//  SetOverrideBoxOffset
///////////////////////////////////////////////////////////////////////////////
void			CPtFxGPUManager::SetOverrideBoxOffset		(Vec3V_In overrideBoxOffset)
{
	//Let all use the override settings
	m_dropSystem.SetOverrideBoxOffset(overrideBoxOffset);
	m_groundSystem.SetOverrideBoxOffset(overrideBoxOffset);
	m_mistSystem.SetOverrideBoxOffset(overrideBoxOffset);

}

///////////////////////////////////////////////////////////////////////////////
//  DebugResetParticles
///////////////////////////////////////////////////////////////////////////////

#if __BANK 
void			CPtFxGPUManager::DebugResetParticles		()
{
	g_vfxWeather.GetPtFxGPUManager().m_resetDropSystem = true;
	g_vfxWeather.GetPtFxGPUManager().m_resetMistSystem = true;
	g_vfxWeather.GetPtFxGPUManager().m_resetGroundSystem = true;
}
#endif



#if __BANK 

///////////////////////////////////////////////////////////////////////////////
//  UpdateOverrideSettings
///////////////////////////////////////////////////////////////////////////////
void			CPtFxGPUManager::UpdateOverrideSettings		()
{
	SetUseOverrideSettings(m_useOverrideSettings);
	SetOverrideCurrLevel(m_overrideCurrLevel);
	SetOverrideBoxOffset(m_overrideBoxOffset);
	SetOverrideBoxSize(m_overrideBoxSize);

}

///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////
void			CPtFxGPUManager::InitWidgets				()
{
	// region combo box setup
	const char* regionList[32];
	for (u32 i=0; i<g_vfxRegionInfoMgr.GetNumRegionInfos(); i++)
	{
		regionList[i] = g_vfxRegionInfoMgr.GetRegionInfoName(i);
	}

	bkBank* pVfxBank = vfxWidget::GetBank();

	// remove any old group as we need to overwrite it with the new level info
	if (m_pBankGroup)
	{
		pVfxBank->DeleteGroup(*m_pBankGroup);
	}

	m_pBankGroup = pVfxBank->PushGroup("GPU Particles", false);
	{
		pVfxBank->AddTitle("SETTINGS");

		#if RAIN_UPDATE_CPU
			pVfxBank->AddToggle("Rain Update on CPU (Supported)", &ptxgpuBase::sm_bRainUpdateOnCpu, NullCB, NULL, NULL, /*read-only*/true);
			pVfxBank->AddToggle("Rain Update on CPU", &ptxgpuBase::sm_bBankRainUpdateOnCpu);
		#endif

		pVfxBank->PushGroup("Time Delta Clamping", false);
		pVfxBank->AddToggle("Clamp Time Delta", &g_bClampDT);
		pVfxBank->AddSlider("Minimum update DT", &g_fMinUpdateDT, 0.001f, 0.03f, 0.001f);
		pVfxBank->PopGroup();


		pVfxBank->PushGroup("Override");
		pVfxBank->AddToggle("Use Override Settings", &m_useOverrideSettings, datCallback(MFA(CPtFxGPUManager::UpdateOverrideSettings), this));
		pVfxBank->AddSlider("Override Curr Level", &m_overrideCurrLevel, 0.0f, 1.0f, 0.001f, datCallback(MFA(CPtFxGPUManager::UpdateOverrideSettings), this));
		pVfxBank->AddVector("Override Box Size", &m_overrideBoxSize, 0.1f, 256.0f, 0.1f, datCallback(MFA(CPtFxGPUManager::UpdateOverrideSettings), this));
		pVfxBank->AddVector("Override Box Offset", &m_overrideBoxOffset, -64.0f, 64.0f, 0.1f, datCallback(MFA(CPtFxGPUManager::UpdateOverrideSettings), this));
		pVfxBank->PopGroup();

#if __DEV
		pVfxBank->AddSlider("Region Drop/Mist Fade Height Min", &GPUPTFX_REGION_DROPMIST_FADE_HEIGHT_MIN, 0.0f, 100.0f, 0.5f);
		pVfxBank->AddSlider("Region Drop/Mist Fade Height Max", &GPUPTFX_REGION_DROPMIST_FADE_HEIGHT_MAX, 0.0f, 100.0f, 0.5f);
		pVfxBank->AddSlider("Region Ground Fade Height Min", &GPUPTFX_REGION_GROUND_FADE_HEIGHT_MIN, 0.0f, 100.0f, 0.5f);
		pVfxBank->AddSlider("Region Ground Fade Height Max", &GPUPTFX_REGION_GROUND_FADE_HEIGHT_MAX, 0.0f, 100.0f, 0.5f);

		pVfxBank->AddSlider("Slowdown Ability Rain Drop Width Scale", &GPUPTFX_SLOWDOWN_ABILITY_RAIN_DROP_WIDTH_SCALE, 0.0f, 10.0f, 0.05f);
		pVfxBank->AddSlider("Slowdown Ability Rain Drop Height Scale", &GPUPTFX_SLOWDOWN_ABILITY_RAIN_DROP_HEIGHT_SCALE, 0.0f, 10.0f, 0.05f);
		pVfxBank->AddSlider("Slowdown Ability Time Mult", &GPUPTFX_SLOWDOWN_ABILITY_TIME_MULT, 0.0f, 1.0f, 0.01f);
#endif
		
		pVfxBank->PushGroup("Drop System", false);
		{
			m_dropSystem.InitWidgets();
		}
		pVfxBank->PopGroup();

		pVfxBank->PushGroup("Mist System", false);
		{
			m_mistSystem.InitWidgets();
		}
		pVfxBank->PopGroup(); 

		pVfxBank->PushGroup("Ground System", false);
		{
			m_groundSystem.InitWidgets();
		}
		pVfxBank->PopGroup();

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG");
		pVfxBank->AddToggle("Debug Render The Systems", &m_debugRender);
		pVfxBank->AddToggle("Disable Rendering", &m_disableRendering);
		pVfxBank->AddSlider("Override Wind Influence", &m_debugWindInfluence, -0.1f, 10.0f, 0.1f, NullCB, "Scale for adding the wind velocity to the gpu particle");
		pVfxBank->AddSlider("Override Gravity", &m_debugGravity, -50.1f, 50.0f, 0.1f, NullCB, "Scale for adding the negative camera velocity to the gpu particle");
		pVfxBank->AddButton("Reset Particles", DebugResetParticles);

		pVfxBank->PushGroup("2.5D Cam Angle Settings");
		pVfxBank->AddSlider("Forward Evo Min", &m_camAngleForwardMin, 0.0f, 1.0f, 0.001f);
		pVfxBank->AddSlider("Forward Evo Max", &m_camAngleForwardMax, 0.0f, 1.0f, 0.001f);
		pVfxBank->AddSlider("Up Evo Min", &m_camAngleUpMin, 0.0f, 1.0f, 0.001f);
		pVfxBank->AddSlider("Up Evo Max", &m_camAngleUpMax, 0.0f, 1.0f, 0.001f);
		pVfxBank->PopGroup();


		pVfxBank->AddToggle("Force Region GpuPtFx", &m_forceRegionGpuPtFx);
		m_pRegionCombo = pVfxBank->AddCombo("Regions", &m_currRegionComboId, g_vfxRegionInfoMgr.GetNumRegionInfos(), regionList);
	}
	pVfxBank->PopGroup();
}



///////////////////////////////////////////////////////////////////////////////
//  DebugRenderUpdate
///////////////////////////////////////////////////////////////////////////////
void			CPtFxGPUManager::DebugRenderUpdate			()
{
	//This updates the render settings when the game is paused
	m_dropSystem.DebugUpdateRenderSettings(m_debugGpuFxPrevDropIndex, m_debugGpuFxNextDropIndex, g_weather.GetInterp(), m_debugCurrVfxRegionDropLevel);
	m_mistSystem.DebugUpdateRenderSettings(m_debugGpuFxPrevMistIndex, m_debugGpuFxNextMistIndex, g_weather.GetInterp(), m_debugCurrVfxRegionMistLevel);
	m_groundSystem.DebugUpdateRenderSettings(m_debugGpuFxPrevGroundIndex, m_debugGpuFxNextGroundIndex, g_weather.GetInterp(), m_debugCurrVfxRegionGroundLevel);
}
#endif // __BANK


///////////////////////////////////////////////////////////////////////////////
//  UpdateVisualDataSettings
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPUManager::UpdateVisualDataSettings	()
{
	g_vfxWeather.GetPtFxGPUManager().m_vDefaultLight.SetXf(g_visualSettings.GetV<float>(atHashWithStringDev("rain.defaultlight.red", 0xe3b83915), 0.5f));
	g_vfxWeather.GetPtFxGPUManager().m_vDefaultLight.SetYf(g_visualSettings.GetV<float>(atHashWithStringDev("rain.defaultlight.green", 0xac156643), 0.5f));
	g_vfxWeather.GetPtFxGPUManager().m_vDefaultLight.SetZf(g_visualSettings.GetV<float>(atHashWithStringDev("rain.defaultlight.blue", 0xc6b94b3d), 0.5f));
	g_vfxWeather.GetPtFxGPUManager().m_vDefaultLight.SetWf(g_visualSettings.GetV<float>(atHashWithStringDev("rain.defaultlight.alpha", 0x310ed7e), 0.5f));
}














