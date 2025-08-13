///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxWeather.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	16 Jun 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxWeather.h"

// rage
#include "pheffects/wind.h"
#include "rmptfx/ptxeffectinst.h"

// framework
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxreg.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "vfx/vfxwidget.h"

// game
#include "Camera/CamInterface.h"
#include "Camera/Gameplay/Aim/FirstPersonShooterCamera.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Core/Game.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "Game/Weather.h"
#include "Game/Wind.h"
#include "Peds/Ped.h"
#include "Peds/PopCycle.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Scene/Portals/Portal.h"
#include "Scene/World/GameWorld.h"
#include "TimeCycle/TimeCycle.h"
#include "Vehicles/Vehicle.h"
#include "Vfx/Metadata/VfxRegionInfo.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxLens.h"
#include "Vfx/VfxHelper.h"
#include "Vfx/VfxSettings.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define		CLOUDS_START_Z	(50.0f)
#define		CLOUDS_END_Z	(250.0f)

dev_float	VFXWEATHER_GROUND_HEIGHT_INTERP_SPEED		= 3.0f;


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	

// weather ptfx offsets for registered systems 
enum PtFxWeatherOffsets_e
{
	PTFXOFFSET_VFXWEATHER_FOG				=0,
	PTFXOFFSET_VFXWEATHER_CLOUDS,
	PTFXOFFSET_VFXWEATHER_UNDERWATER,
//	PTFXOFFSET_VFXWEATHER_RAIN_MIST,
//	PTFXOFFSET_VFXWEATHER_SNOW_MIST,
//	PTFXOFFSET_VFXWEATHER_SANDSTORM,
//	PTFXOFFSET_VFXWEATHER_SANDSTORM_2,
//	PTFXOFFSET_VFXWEATHER_SANDSTORM_3,
//	PTFXOFFSET_VFXWEATHER_SANDSTORM_4,
//	PTFXOFFSET_VFXWEATHER_SANDSTORM_5,
//	PTFXOFFSET_VFXWEATHER_SANDSTORM_6,
//	PTFXOFFSET_VFXWEATHER_SANDSTORM_7,
//	PTFXOFFSET_VFXWEATHER_SANDSTORM_8,
//	PTFXOFFSET_VFXWEATHER_SANDSTORM_9,
	PTFXOFFSET_VFXWEATHER_WIND_DEBRIS
};


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////
PARAM(novfxweather, "disables vfx weather systems");

CVfxWeather		g_vfxWeather;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxWeather
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeather::Init						(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
	    // init the gpu fx 
	    m_ptFxGPUManager.Init();

		m_heightAboveGroundDampened = 0.0f;

#if __BANK
	    //	m_disableRainMist = false;
	    //	m_disableSnowMist = false;
	    //	m_disableSandstorm = false;
	    m_disableFog = false;
	    m_disableClouds = false;
	    //	m_disableUnderwater = false;

	    m_rainOverrideActive = false;
	    m_rainOverrideVal = 1.0f;

	    m_snowOverrideActive = false;
	    m_snowOverrideVal = 1.0f;

		m_snowMistOverrideActive = false;
		m_snowMistOverrideVal = 0.1f;

	    m_sandstormOverrideActive = false;
	    m_sandstormOverrideVal = 1.0f;
#endif
    }
	else if (initMode==INIT_SESSION)
	{
		// init the weather probes
		InitProbes();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeather::Shutdown					(unsigned shutdownMode)
{
    if (shutdownMode == SHUTDOWN_CORE)
    {
	    // shutdown the gpu fx 
	    m_ptFxGPUManager.Shutdown();
    }
	else if (shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
	{
		ptfxReg::UnRegister(this);
	}
    else if (shutdownMode==SHUTDOWN_SESSION)
    {
		ptfxReg::UnRegister(this);

		ShutdownProbes();
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeather::Update						(float deltaTime)
{
	// calc some camera info
	if(!fwTimer::IsGamePaused())
	{

		Mat34V_ConstRef camMat = RCC_MAT34V(camInterface::GetGameplayDirector().GetFrame().GetWorldMatrix());
		Vec3V vCamPos = camMat.GetCol3();

		m_vCamVel = vCamPos - m_vCamPos;
		m_vCamVel *= ScalarVFromF32(fwTimer::GetInvTimeStep());
		m_vCamPos = vCamPos;

		// process effect systems
 		Vec3V vCurrAirVel;
		WIND.GetGlobalVelocity(WIND_TYPE_AIR, vCurrAirVel);
		float windAirSpeed = Mag(vCurrAirVel).Getf()/WEATHER_WIND_SPEED_MAX;

		Vec3V vCurrWaterVel;
		WIND.GetGlobalVelocity(WIND_TYPE_WATER, vCurrWaterVel);
		float windWaterSpeed = Mag(vCurrWaterVel).Getf()/WEATHER_WATER_SPEED_MAX;

		float camSpeed = CVfxHelper::GetInterpValue(Mag(m_vCamVel).Getf(), 0.0f, 10.0f);

		// calculate the fxLevel - based on interiors
		static float fxLevelSpeed = 0.05f;
		static float fxLevel = 1.0f;
		float targetFxLevel = CPortal::GetInteriorFxLevel();

		// override to zero if we're in a timecycle modifier region with the no weather fx flag set
		if (g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_NO_WEATHER_FX)>0.0f)
		{
			targetFxLevel = 0.0f;
		}

		// blend between the current and target fxLevel
		if (targetFxLevel>fxLevel)
		{
			fxLevel = Min(targetFxLevel, fxLevel+fxLevelSpeed);
		}
		else
		{
			fxLevel = Max(targetFxLevel, fxLevel-fxLevelSpeed);
		}

		// force to zero if we're in an interior
		if (CVfxHelper::IsCamInInterior())
		{
			fxLevel = 0.0f;
		}

		// store the current rain value
		m_currRain = g_weather.GetTimeCycleAdjustedRain();
	#if __BANK
		// check for the override being active
		if (m_rainOverrideActive)
		{
			m_currRain = m_rainOverrideVal;
		}
	#endif // __BANK

		// rain mist effect
	//	if (m_currRain>0.0f)
	//	{
	//		// rain mist
	//		vfxAssertf(m_currRain<=1.0f, "current rain value is out of range");	
	//		float rainMistLevel = m_currRain*fxLevel;
	//		g_vfxWeather.UpdatePtFxRainMist(camMat, fxLevel, rainMistLevel, windAirSpeed, camSpeed);
	//	}

		// store the current snow value
		m_currSnow = g_weather.GetSnow();
	#if __BANK
		// check for the override being active
		if (m_snowOverrideActive)
		{
			m_currSnow = m_snowOverrideVal;
		}
	#endif // __BANK

		m_currSnowMist = g_weather.GetSnowMist();
	#if __BANK
		// check for the override being active
		if (m_snowMistOverrideActive)
		{
			m_currSnowMist = m_snowMistOverrideVal;
		}
	#endif // __BANK

		// snow mist effect
	//	if (m_currSnow>0.0f)
	//	{
	//		// snow mist
	//		vfxAssertf(m_currSnow<=1.0f, "current snow value is out of range");
	//		float snowMistLevel = m_currSnow*fxLevel;
	//		g_vfxWeather.UpdatePtFxSnowMist(camMat, fxLevel, snowMistLevel, windAirSpeed, camSpeed);
	//	}

		// store the current sandstorm value
		m_currSandstorm = g_weather.GetSandstorm();
	#if __BANK
		// check for the override being active
		if (m_sandstormOverrideActive)
		{
			m_currSandstorm = m_sandstormOverrideVal;
		}
	#endif // __BANK

		// sandstorm effect
	//	if (m_currSandstorm>0.0f)
	//	{
	//		// sand mist
	//		vfxAssertf(m_currSandstorm<=1.0f, "current sandstorm value is out of range");
	//		float sandstormLevel = m_currSandstorm*fxLevel;
	//		g_vfxWeather.UpdatePtFxSandstorm(camMat, fxLevel, sandstormLevel, windAirSpeed, camSpeed);
	//	}

		// ground fog
		if (g_weather.GetFog()>0.0f)
		{
			float fogLevel = g_weather.GetFog()*fxLevel;
			vfxAssertf(fogLevel<=1.0f, "current fog value is out of range");
			g_vfxWeather.UpdatePtFxFog(camMat, fxLevel, fogLevel, windAirSpeed, camSpeed);
		}

		// get info about the player ped and vehicle
		CPlayerInfo* pPlayerInfo = CGameWorld::GetMainPlayerInfo();
		CPed* pPlayerPed = pPlayerInfo ? pPlayerInfo->GetPlayerPed() : NULL;
		CVehicle* pPlayerVehicle = NULL;
		if (pPlayerPed)
		{
			if (pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
			{
				pPlayerVehicle = pPlayerPed->GetMyVehicle();
			}
		}

		// wind debris / underwater effect
		float windDebrisMult = 1.0f;
		if (pPlayerVehicle && (pPlayerVehicle->GetVehicleType()==VEHICLE_TYPE_HELI || pPlayerVehicle->GetVehicleType()==VEHICLE_TYPE_BOAT))
		{	
			windDebrisMult = 0.0f;
		}

		float windAirLevel = windAirSpeed*windAirSpeed*fxLevel*windDebrisMult;
		float windWaterLevel = windWaterSpeed*windWaterSpeed*fxLevel*windDebrisMult;
		g_vfxWeather.UpdateWindDebrisFx(camMat, fxLevel, windAirLevel, windWaterLevel, camSpeed);

		// clouds (planes and helis)
		if (pPlayerVehicle && (pPlayerVehicle->GetVehicleType()==VEHICLE_TYPE_HELI || pPlayerVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE))
		{		
			float cloudLevel = 0.0f;
			float vehSpeed = 0.0f;

			if (m_vCamPos.GetZf()>=CLOUDS_END_Z)
			{
				cloudLevel = 1.0f;
				vehSpeed = pPlayerVehicle->GetVelocity().Mag();
			}
			else if (m_vCamPos.GetZf()>CLOUDS_START_Z)
			{
				cloudLevel = (m_vCamPos.GetZf()-CLOUDS_START_Z)/(CLOUDS_END_Z-CLOUDS_START_Z);
				vehSpeed = pPlayerVehicle->GetVelocity().Mag();
			}

			if (cloudLevel>0.0f)
			{
				cloudLevel *= fxLevel;

				vfxAssertf(cloudLevel<=1.0f, "current cloud value is out of range");
				float vehSpeedEvo = CVfxHelper::GetInterpValue(vehSpeed, 0.0f, 30.0f);
				g_vfxWeather.UpdatePtFxClouds(camMat, fxLevel, cloudLevel, vehSpeedEvo);
			}
		}

		// clouds (parachuting)
		if (pPlayerPed && pPlayerPed->GetIsParachuting())
		{
			float cloudLevel = 0.0f;
			float pedSpeed = 0.0f;

			if (m_vCamPos.GetZf()>=CLOUDS_END_Z)
			{
				cloudLevel = 1.0f;
				pedSpeed = pPlayerPed->GetVelocity().Mag();
			}
			else if (m_vCamPos.GetZf()>CLOUDS_START_Z)
			{
				cloudLevel = (m_vCamPos.GetZf()-CLOUDS_START_Z)/(CLOUDS_END_Z-CLOUDS_START_Z);
				pedSpeed = pPlayerPed->GetVelocity().Mag();
			}

			if (cloudLevel>0.0f)
			{
				cloudLevel *= fxLevel;

				vfxAssertf(cloudLevel<=1.0f, "current cloud value is out of range");
				float pedSpeedEvo = CVfxHelper::GetInterpValue(pedSpeed, 0.0f, 30.0f);
				g_vfxWeather.UpdatePtFxClouds(camMat, fxLevel, cloudLevel, pedSpeedEvo);
			}
		}

		// update the weather probes to see if any camera lens effects need to play
		UpdateProbes();
		Approach(m_heightAboveGroundDampened, m_groundProbe.m_heightAboveGround, VFXWEATHER_GROUND_HEIGHT_INTERP_SPEED, TIME.GetSeconds());

		UpdateCameraLensFx(camMat, fxLevel);

		if(PARAM_novfxweather.Get())
			return;

		// update the gpu fx 
		m_ptFxGPUManager.Update(deltaTime);

	}
#if __BANK
	else
	{
		m_ptFxGPUManager.DebugRenderUpdate();
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  InitProbes
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeather::InitProbes					()
{
	m_groundProbe.m_vfxProbeId = -1;
	m_groundProbe.m_camZ = 9999.0f;
	m_groundProbe.m_heightAboveGround = 9999.0f;

	m_currWeatherProbeId = 0;
	m_vWeatherProbeOffsets[0] =  Vec3V(V_ZERO);
	m_vWeatherProbeOffsets[1] =  Vec3V(V_X_AXIS_WZERO);
	m_vWeatherProbeOffsets[2] = -Vec3V(V_X_AXIS_WZERO);
	m_vWeatherProbeOffsets[3] =  Vec3V(V_Y_AXIS_WZERO);
	m_vWeatherProbeOffsets[4] = -Vec3V(V_Y_AXIS_WZERO);
	for (s32 i=0; i<VFXWEATHER_NUM_WEATHER_PROBES; i++)
	{
		m_weatherProbeResults[i] = true;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ShutdownProbes
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeather::ShutdownProbes				()
{
	if (m_groundProbe.m_vfxProbeId>=0)
	{
		CVfxAsyncProbeManager::AbortProbe(m_groundProbe.m_vfxProbeId);
		m_groundProbe.m_vfxProbeId = -1;
		m_groundProbe.m_camZ = 9999.0f;
		m_groundProbe.m_heightAboveGround = 9999.0f;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateProbes
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeather::UpdateProbes				()
{
	// ground probe
	if (m_groundProbe.m_vfxProbeId>=0)
	{
		// query the probe
		VfxAsyncProbeResults_s vfxProbeResults;
		if (CVfxAsyncProbeManager::QueryProbe(m_groundProbe.m_vfxProbeId, vfxProbeResults))
		{
			// the probe has finished
			m_groundProbe.m_vfxProbeId = -1;

			if (vfxProbeResults.hasIntersected)
			{
				m_groundProbe.m_heightAboveGround = m_groundProbe.m_camZ - vfxProbeResults.vPos.GetZf();
			}
			else
			{
				m_groundProbe.m_heightAboveGround = 9999.0f;
			}

			Vec3V vWaterPos;
			WaterTestResultType waterType = CVfxHelper::TestLineAgainstWater(m_groundProbe.m_vStartPos, m_groundProbe.m_vEndPos, vWaterPos);
			bool foundWater = waterType>WATERTEST_TYPE_NONE;
			if (foundWater)
			{
				m_groundProbe.m_heightAboveGround = Min(m_groundProbe.m_heightAboveGround, m_groundProbe.m_vStartPos.GetZf()-vWaterPos.GetZf());
			}
		}
	}
	else
	{
		static u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER;
		Vec3V vStartPos = m_vCamPos;
		Vec3V vEndPos = m_vCamPos - Vec3V(0.0f, 0.0f, 50.0f);
		m_groundProbe.m_vfxProbeId = CVfxAsyncProbeManager::SubmitWorldProbe(vStartPos, vEndPos, probeFlags);
		m_groundProbe.m_camZ = m_vCamPos.GetZf();
		m_groundProbe.m_vStartPos = vStartPos;
		m_groundProbe.m_vEndPos = vEndPos;
	}

	// camera lens weather probes
	if (m_currRain>0.0f || m_currSnow>0.0f)
	{
		// test if there is anything above us that would block the rain/snow
		// NOTE:  update the probes even if the camera isnt looking up since audio is relying on the results
		if (m_weatherProbes[m_currWeatherProbeId].IsProbeActive())
		{
			// the probe is active - check if it's finished
			ProbeStatus probeStatus;
			if (m_weatherProbes[m_currWeatherProbeId].GetProbeResults(probeStatus))
			{
				// the probe is finished - store the results
				if (probeStatus==PS_Blocked)
				{
					m_weatherProbeResults[m_currWeatherProbeId] = true;
				}
				else if (probeStatus==PS_Clear)
				{
					m_weatherProbeResults[m_currWeatherProbeId] = false;
				}
				else
				{
					vfxAssertf(0, "unexpected probe status found");
				}

				m_currWeatherProbeId = (m_currWeatherProbeId+1)%VFXWEATHER_NUM_WEATHER_PROBES;
			}
		}
		else
		{
			// the probe isn't active yet - set it off
			Vec3V vStartPos = m_vCamPos + m_vWeatherProbeOffsets[m_currWeatherProbeId];
			Vec3V vEndPos = vStartPos;
			vEndPos.SetZf(vEndPos.GetZf()+30.0f);
			WorldProbe::CShapeTestProbeDesc probeData;
			probeData.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
			probeData.SetIncludeFlags(INCLUDE_FLAGS_ALL);
			m_weatherProbes[m_currWeatherProbeId].StartTestLineOfSight(probeData);
		}
	}
	else
	{
		// not raining/snowing - reset the weather probes
		for (s32 i=0; i<VFXWEATHER_NUM_WEATHER_PROBES; i++)
		{
			m_weatherProbes[i].ResetProbe();
			m_weatherProbeResults[i] = true;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateCameraLensFx
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeather::UpdateCameraLensFx			(Mat34V_In FPS_MODE_SUPPORTED_ONLY(camMat), float FPS_MODE_SUPPORTED_ONLY(fxLevel))
{
	if (m_currRain>0.0f || m_currSnow>0.0f || m_currSandstorm>0.0f)
	{
		// check if the camera is facing the direction of the particles
		// we estimate this by presuming that gpu particles will fall downwards unless they are affected much by the wind
		// as the wind scale goes from 1.0 to 3.0 the direction of the particle goes from being downwards to in the direction of the wind

		// calc the ratio from ptx to wind direction using the wind scale in the gpu effect (blend between a wind scale of 1.0 and 3.0)
// 		float ptxToWindRatio = 0.0f;
// 		float windScale = m_ptFxGPUManager.GetCurrWindInfluence();
// 		if (windScale>1.0f)
// 		{
// 			ptxToWindRatio = Min(((windScale-1.0f)*0.5f), 1.0f);
// 		}
// 
// 		Vec3V vWindDir;
// 		WIND.GetGlobalVelocity(WIND_TYPE_AIR, vWindDir);
// 		vWindDir = Normalize(vWindDir);
// 
// 		// estimate the particle direction
// 		Vec3V vDownDir(0.0f, 0.0f, -1.0f);
// 		Vec3V vPtxDir = vDownDir*ScalarVFromF32(1.0f-ptxToWindRatio) + vWindDir*ScalarVFromF32(ptxToWindRatio);
//		vPtxDir = Normalize(vPtxDir);

		Vec3V vPtxDir = Vec3V(0.0f, 0.0f, -1.0f);

		// calc the angle of the camera to the particles
		Vec3V vCamForward = camMat.GetCol1();
		float angle = Dot(vCamForward, -vPtxDir).Getf();
		angle = Min(angle, 1.0f);
		angle = Max(angle, 0.0f);

		// calc if we should be doing camera fx
		bool doCameraFx = (angle>0.0f);

		// skip camera fx if the probes are colliding (rain and snow only)
		if (doCameraFx && (m_currRain>0.0f || m_currSnow>0.0f))
		{
			for (s32 i=0; i<VFXWEATHER_NUM_WEATHER_PROBES; i++)
			{
				if (m_weatherProbeResults[i])
				{
					// collision occurred - no camera fx
					doCameraFx = false;
					break;
				}
			}
		}

		// skip camera fx if a cutscene is running
		if ((CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning()))
		{
			doCameraFx = false;
		}

		// play the camera effect if required
		if (doCameraFx && !CVfxHelper::GetCamInVehicleFlag())
		{
			if (m_currRain>0.0f)
			{
				g_vfxLens.Register(VFXLENSTYPE_RAIN, m_currRain*fxLevel, angle, 0.0f, fxLevel);
			}

			if (m_currSnow>0.0f)
			{
				g_vfxLens.Register(VFXLENSTYPE_SNOW, m_currSnow*fxLevel, angle, 0.0f, fxLevel);
			}
		}
	}

// 	float underwaterDepth = CVfxHelper::GetGameCamWaterDepth();
// 	if (underwaterDepth>0.0f)
// 	{
// 		// depth evo
// 		float depthEvo = 1.0f-(Min(underwaterDepth, 50.0f))/50.0f;
// 		vfxAssertf(depthEvo>=0.0f, "depth evo is out of range");
// 		vfxAssertf(depthEvo<=1.0f, "depth evo is out of range");
// 
// 		// camspeed evo
// 		Vec3V vCamVel = RCC_VEC3V(camInterface::GetPosDelta());
// 		vCamVel *= ScalarVFromF32(fwTimer::GetInvTimeStep());
// 		float camSpeedEvo = Mag(vCamVel).Getf() / 10.0f;
// 		camSpeedEvo = Max(0.0f, camSpeedEvo);
// 		camSpeedEvo = Min(1.0f, camSpeedEvo);
// 
// 		g_vfxLens.Register(VFXLENSTYPE_UNDERWATER, depthEvo, 0.0f, camSpeedEvo, 1.0f);
// 	}
//#endif
}

///////////////////////////////////////////////////////////////////////////////
//  Render
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeather::Render						(Vec3V_ConstRef vCamVel)
{
	if(PARAM_novfxweather.Get())
		return;

	// render the gpu fx 
	m_ptFxGPUManager.Render(vCamVel);
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxRainMist
///////////////////////////////////////////////////////////////////////////////
/*
void			CVfxWeather::UpdatePtFxRainMist			(Mat34V_In camMat, float fxAlpha, float rainEvo, float windEvo, float camSpeedEvo)
{
#if __BANK
	if (m_disableRainMist)
	{
		return;
	}
#endif

	// calculate the effect matrix
	Vec3V vCamForward = camMat.GetCol1();
	vCamForward.SetZ(ScalarV(V_ZERO));
	vCamForward = Normalize(vCamForward);

	Vec3V vCamUp(V_Z_AXIS_WZERO);
	Vec3V vCamRight = Cross(vCamForward, vCamUp);

	Vec3V vCamPos = camMat.GetCol3();
	vCamPos += vCamForward*ScalarV(V_TEN);

	float groundZ = WorldProbe::FindGroundZForCoord(TOP_SURFACE, vCamPos.GetXf(), vCamPos.GetYf());

	vCamPos.SetZf(groundZ);

	Mat34V fxMat;
	fxMat.SetCol0(vCamRight);
	fxMat.SetCol1(vCamForward);
	fxMat.SetCol2(vCamUp);
	fxMat.SetCol3(vCamPos);

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(this, PTFXOFFSET_VFXWEATHER_RAIN_MIST, false, atHashWithStringNotFinal("weather_rain"), justCreated);

	if (pFxInst)
	{
		pFxInst->SetBaseMtx(fxMat);

		pFxInst->SetEvoValue("rain", rainEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("wind", 0x35369828), windEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("camspeed", 0xB988815B), camSpeedEvo);

		ptfxWrapper::SetAlphaTint(pFxInst, fxAlpha);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
}
*/

///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxSnowMist
///////////////////////////////////////////////////////////////////////////////
/*
void			CVfxWeather::UpdatePtFxSnowMist			(Mat34V_In camMat, float fxAlpha, float snowEvo, float windEvo, float camSpeedEvo)
{
#if __BANK
	if (m_disableSnowMist)
	{
		return;
	}
#endif

	// calculate the effect matrix
	Vec3V vCamForward = camMat.GetCol1();
	vCamForward.SetZ(ScalarV(V_ZERO));
	vCamForward = Normalize(vCamForward);

	Vec3V vCamUp(V_Z_AXIS_WZERO);
	Vec3V vCamRight = Cross(vCamForward, vCamUp);

	Vec3V vCamPos = camMat.GetCol3();
	vCamPos += vCamForward*ScalarV(V_TEN);

	float groundZ = WorldProbe::FindGroundZForCoord(TOP_SURFACE, vCamPos.GetXf(), vCamPos.GetYf());

	vCamPos.SetZf(groundZ);

	Mat34V fxMat;
	fxMat.SetCol0(vCamRight);
	fxMat.SetCol1(vCamForward);
	fxMat.SetCol2(vCamUp);
	fxMat.SetCol3(vCamPos);

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(this, PTFXOFFSET_VFXWEATHER_SNOW_MIST, false, atHashWithStringNotFinal("weather_snow"), justCreated);

	if (pFxInst)
	{
		pFxInst->SetBaseMtx(fxMat);

		pFxInst->SetEvoValue("snow", snowEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("wind", 0x35369828), windEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("camspeed", 0xB988815B), camSpeedEvo);

		ptfxWrapper::SetAlphaTint(pFxInst, fxAlpha);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
}
*/

///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxSandstorm
///////////////////////////////////////////////////////////////////////////////
/*
void			CVfxWeather::UpdatePtFxSandstorm		(Mat34V_In camMat, float fxAlpha, float sandEvo, float windEvo, float camSpeedEvo)
{
#if __BANK
	if (m_disableSandstorm)
	{
		return;
	}
#endif

	// calc sample points
	#define SAMPLE_SPACING		(25.0f)
	ScalarV vSampleSpacing = ScalarVFromF32(SAMPLE_SPACING);
	Vec3V vRight(V_X_AXIS_WZERO);
	Vec3V vForward(V_Y_AXIS_WZERO);
	Vec3V vSamplePts[9];
	vSamplePts[0] = camMat.GetCol3() - (vRight*vSampleSpacing) + (vForward*vSampleSpacing);
	vSamplePts[1] = camMat.GetCol3() + (vForward*vSampleSpacing);
	vSamplePts[2] = camMat.GetCol3() + (vRight*vSampleSpacing) + (vForward*vSampleSpacing);
	vSamplePts[3] = camMat.GetCol3() - (vRight*vSampleSpacing);
	vSamplePts[4] = camMat.GetCol3();
	vSamplePts[5] = camMat.GetCol3() + (vRight*vSampleSpacing);
	vSamplePts[6] = camMat.GetCol3() - (vRight*vSampleSpacing) - (vForward*vSampleSpacing);
	vSamplePts[7] = camMat.GetCol3() - (vForward*vSampleSpacing);
	vSamplePts[8] = camMat.GetCol3() + (vRight*vSampleSpacing) - (vForward*vSampleSpacing);

	for (s32 i=0; i<9; i++)
	{
		// move onto ground level
		phIntersection isect;

		float groundZ = WorldProbe::FindGroundZForCoord(TOP_SURFACE, vSamplePts[i].GetXf(), vSamplePts[i].GetYf(), WorldProbe::LOS_Unspecified, &isect);

		float sandEvoMult = 1.0f;
//		s32 mtlId = PGTAMATERIALMGR->UnpackMtlId(isect.GetMaterialId());
//		VfxGroup_e vfxGroup = PGTAMATERIALMGR->GetMtlVfxGroup(mtlId);
//		if (vfxGroup!=VFXGROUP_SAND)
//		{
//			if (vfxGroup==VFXGROUP_SAND_WET)
//			{
//				sandEvoMult = 0.5f;
//			}
//			else 
//			{
//				sandEvoMult = 0.1f;
//			}
//		}


		// calculate the effect matrix
		Mat34V fxMat;
		fxMat.SetCol2(RCC_VEC3V(isect.GetNormal()));
		fxMat.SetCol1(vForward);
		fxMat.SetCol0(Cross(fxMat.GetCol1(), fxMat.GetCol2()));

		Vec3V vPos = vSamplePts[i];
		vPos.SetZf(groundZ);
		fxMat.SetCol3(vPos);

		// register the fx system
		bool justCreated;
		ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(this, PTFXOFFSET_VFXWEATHER_SANDSTORM+i, false, atHashWithStringNotFinal("weather_sandstorm"), justCreated);

		if (pFxInst)
		{
			pFxInst->SetBaseMtx(fxMat);

			pFxInst->SetEvoValue("sand", sandEvo*sandEvoMult);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("wind", 0x35369828), windEvo);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("camspeed", 0xB988815B), camSpeedEvo);

			ptfxWrapper::SetAlphaTint(pFxInst, fxAlpha);

			// check if the effect has just been created 
			if (justCreated)
			{
				// it has just been created - start it and set it's position
				pFxInst->Start();
			}
		}
	}
}
*/

///////////////////////////////////////////////////////////////////////////////
//  UpdateWindDebrisFx
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeather::UpdateWindDebrisFx				(Mat34V_In camMat, float fxAlpha, float windAirEvo, float UNUSED_PARAM(windWaterEvo), float camSpeedEvo)
{
	// return if the ped is not under water
	float depth = CVfxHelper::GetGameCamWaterDepth();
	if (depth>0.0f)
	{
		//if (windWaterEvo>0.0f)
		//{
		//	UpdatePtFxUnderWater(camMat, fxAlpha, windWaterEvo, camSpeedEvo, depth);
		//}
	}
	else
	{
		if (fxAlpha==0.0f)
		{
			RemovePtFxWindDebris();
		}
		else if (windAirEvo>0.0f)
		{
			UpdatePtFxWindDebris(camMat, fxAlpha, windAirEvo, camSpeedEvo);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxWindDebris
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeather::UpdatePtFxWindDebris			(Mat34V_In camMat, float fxAlpha, float windEvo, float camSpeedEvo)
{
#if __BANK
	if (m_disableWindDebris)
	{
		return;
	}
#endif

//	const char* pPtFxNameWindDebris = g_vfxSettings.GetPtFxNameWindDebris();
//	if (strlen(pPtFxNameWindDebris)==0)
//	{
//		return;
//	}

	// get the vfx region info
	
	CVfxRegionInfo* pVfxRegionInfo = g_vfxRegionInfoMgr.GetInfo(CPopCycle::GetCurrentZoneVfxRegionHashName());

	// return if foot decals aren't enabled
	if (pVfxRegionInfo == NULL || pVfxRegionInfo->GetWindDebrisPtFxEnabled()==false)
	{
		return;
	}

	// calculate the effect matrix
	Vec3V vCamForward = camMat.GetCol1();
	vCamForward.SetZ(ScalarV(V_ZERO));
	vCamForward = Normalize(vCamForward);

	Vec3V vCamUp(V_Z_AXIS_WZERO);
	Vec3V vCamRight = Cross(vCamForward, vCamUp);

	Vec3V vCamPos = camMat.GetCol3();
	vCamPos += vCamForward*ScalarV(V_TEN);

	Mat34V fxMat;
	fxMat.SetCol0(vCamRight);
	fxMat.SetCol1(vCamForward);
	fxMat.SetCol2(vCamUp);
	fxMat.SetCol3(vCamPos);

	// register the fx system
	bool justCreated;
	//ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(this, PTFXOFFSET_VFXWEATHER_WIND_DEBRIS, false, atHashWithStringNotFinal(pPtFxNameWindDebris), justCreated);
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(this, PTFXOFFSET_VFXWEATHER_WIND_DEBRIS, false, pVfxRegionInfo->GetWindDebrisPtFxName(), justCreated);
	
	if (pFxInst)
	{
		pFxInst->SetBaseMtx(fxMat);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("wind", 0x35369828), windEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("camspeed", 0xB988815B), camSpeedEvo);

		ptfxWrapper::SetAlphaTint(pFxInst, fxAlpha);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RemovePtFxWindDebris
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeather::RemovePtFxWindDebris		()
{	
	// find the fx system
	ptfxRegInst* pRegFxInst = ptfxReg::Find(this, PTFXOFFSET_VFXWEATHER_WIND_DEBRIS);

	if (pRegFxInst)
	{
		if (pRegFxInst->m_pFxInst)
		{
			pRegFxInst->m_pFxInst->Finish(false);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxUnderWater
///////////////////////////////////////////////////////////////////////////////
/*
void			CVfxWeather::UpdatePtFxUnderWater			(Mat34V_In camMat, float fxAlpha, float windEvo, float camSpeedEvo, float depth)
{
#if __BANK
	if (m_disableUnderWater)
	{
		return;
	}
#endif

	// calculate the effect matrix
	Vec3V vCamForward = camMat.GetCol1();
	vCamForward.SetZ(ScalarV(V_ZERO));
	vCamForward = Normalize(vCamForward);

	Vec3V vCamUp(V_Z_AXIS_WZERO);
	Vec3V vCamRight = Cross(vCamForward, vCamUp);

	Vec3V vCamPos = camMat.GetCol3();
	vCamPos += vCamForward*ScalarV(V_TEN);

	Mat34V fxMat;
	fxMat.SetCol0(vCamRight);
	fxMat.SetCol1(vCamForward);
	fxMat.SetCol2(vCamUp);
	fxMat.SetCol3(vCamPos);

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(this, PTFXOFFSET_VFXWEATHER_UNDERWATER, false, atHashWithStringNotFinal("weather_debris_underwater"), justCreated);

	if (pFxInst)
	{
		pFxInst->SetBaseMtx(fxMat);

		// set evolutions
		float depthEvo = 1.0f - CVfxHelper::GetInterpValue(depth, 0.0f, 50.0f);
		pFxInst->SetEvoValue("depth", depthEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("wind", 0x35369828), windEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("camspeed", 0xB988815B), camSpeedEvo);

		float depthScale = CVfxHelper::GetInterpValue(depth, 0.0f, 5.0f);
		ptfxWrapper::SetAlphaTint(pFxInst, fxAlpha*depthScale);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
}
*/

///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxFog
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeather::UpdatePtFxFog					(Mat34V_In camMat, float fxAlpha, float fogEvo, float windEvo, float camSpeedEvo)
{
#if __BANK
	if (m_disableFog)
	{
		return;
	}
#endif

	// calculate the effect matrix
	Vec3V vCamForward = camMat.GetCol1();
	vCamForward.SetZ(ScalarV(V_ZERO));
	vCamForward = Normalize(vCamForward);

	Vec3V vCamUp(V_Z_AXIS_WZERO);
	Vec3V vCamRight = Cross(vCamForward, vCamUp);

	Vec3V vCamPos = camMat.GetCol3();
	vCamPos += vCamForward*ScalarV(V_TEN);

	float groundZ = WorldProbe::FindGroundZForCoord(TOP_SURFACE, vCamPos.GetXf(), vCamPos.GetYf());

	vCamPos.SetZf(groundZ);

	Mat34V fxMat;
	fxMat.SetCol0(vCamRight);
	fxMat.SetCol1(vCamForward);
	fxMat.SetCol2(vCamUp);
	fxMat.SetCol3(vCamPos);

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(this, PTFXOFFSET_VFXWEATHER_FOG, false, atHashWithStringNotFinal("env_fog",0xF2A25283), justCreated);

	if (pFxInst)
	{
		pFxInst->SetBaseMtx(fxMat);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("fog", 0xD61BDE01), fogEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("wind", 0x35369828), windEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("camspeed", 0xB988815B), camSpeedEvo);
		ptfxWrapper::SetAlphaTint(pFxInst, fxAlpha);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxClouds
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeather::UpdatePtFxClouds				(Mat34V_In camMat, float fxAlpha, float cloudEvo, float speedEvo)
{
#if __BANK
	if (m_disableClouds)
	{
		return;
	}
#endif

	// calculate the effect matrix
	Vec3V vCamForward = camMat.GetCol1();
	vCamForward.SetZ(ScalarV(V_ZERO));
	vCamForward = Normalize(vCamForward);

	Vec3V vCamUp(V_Z_AXIS_WZERO);
	Vec3V vCamRight = Cross(vCamForward, vCamUp);

	Vec3V vCamPos = camMat.GetCol3();
	vCamPos += vCamForward*ScalarV(V_TEN);

	Mat34V fxMat;
	fxMat.SetCol0(vCamRight);
	fxMat.SetCol1(vCamForward);
	fxMat.SetCol2(vCamUp);
	fxMat.SetCol3(vCamPos);

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(this, PTFXOFFSET_VFXWEATHER_CLOUDS, false, atHashWithStringNotFinal("env_cloud",0x45F30F7C), justCreated);

	if (pFxInst)
	{
		pFxInst->SetBaseMtx(fxMat);
		
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("cloud", 0x27F49EC9), cloudEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		ptfxWrapper::SetAlphaTint(pFxInst, fxAlpha);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CVfxWeather::InitWidgets			()
{
	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("Vfx Weather", false);
	{
		pVfxBank->AddTitle("INFO");
		pVfxBank->AddSlider("Height Above Ground (Probe)", &m_groundProbe.m_heightAboveGround, 0.0f, 9999.0f, 0.0f);
		pVfxBank->AddSlider("Height Above Ground (Dampened)", &m_heightAboveGroundDampened, 0.0f, 9999.0f, 0.0f);
#if __DEV
		pVfxBank->AddSlider("Height Above Ground (Interp Speed)", &VFXWEATHER_GROUND_HEIGHT_INTERP_SPEED, 0.0f, 10.0f, 0.1f);
#endif	

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG");
		pVfxBank->AddToggle("Override Rain", &m_rainOverrideActive);
		pVfxBank->AddSlider("Override Rain Value ", &m_rainOverrideVal, 0.0f, 1.0f, 0.05f);
		pVfxBank->AddToggle("Override Snow", &m_snowOverrideActive);
		pVfxBank->AddSlider("Override Snow Value ", &m_snowOverrideVal, 0.0f, 1.0f, 0.05f);
		pVfxBank->AddToggle("Override Snow Mist", &m_snowMistOverrideActive);
		pVfxBank->AddSlider("Override Snow Mist Value ", &m_snowMistOverrideVal, 0.0f, 1.0f, 0.05f);
		pVfxBank->AddToggle("Override Sandstorm", &m_sandstormOverrideActive);
		pVfxBank->AddSlider("Override Sandstorm Value ", &m_sandstormOverrideVal, 0.0f, 1.0f, 0.05f);

//		pVfxBank->AddToggle("Disable Rain Mist Fx", &m_disableRainMist);
//		pVfxBank->AddToggle("Disable Snow Mist Fx", &m_disableSnowMist);
//		pVfxBank->AddToggle("Disable Sandstorm Fx", &m_disableSandstorm);
		pVfxBank->AddToggle("Disable Fog Fx", &m_disableFog);
		pVfxBank->AddToggle("Disable Clouds Fx", &m_disableClouds);
		pVfxBank->AddToggle("Disable Wind Debris Fx", &m_disableWindDebris);
//		pVfxBank->AddToggle("Disable Underwater Fx", &m_disableUnderWater);
	}
	pVfxBank->PopGroup();
}
#endif





