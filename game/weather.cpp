///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	Weather.cpp
//	BY	: 	Mark Nicholson (Adapted from original by Obbe)
//	FOR	:	Rockstar North
//	ON	:	11 Aug 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "Weather.h"
#include "Weather_parser.h"

// Rage headers
#include "pheffects/wind.h"
#include "pheffects/winddisturbancebase.h"
#include "vectormath/classfreefuncsv.h"

// Framework headers
#include "fwmaths/Random.h"
#include "fwsys/timer.h"
#include "timecycle/channel.h"

// Game Headers
#include "Audio/WeatherAudioEntity.h"
#include "Control/Replay/Replay.h"
#include "Core/Game.h"
#include "Game/Clock.h"
#include "Game/Wind.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/NetworkInterface.h"
#include "Peds/Ped.h"
#include "Renderer/PostProcessFx.h"
#include "Renderer/Water.h"
#include "Scene/DataFileMgr.h"
#include "Scene/Portals/Portal.h"
#include "System/ControlMgr.h"
#include "System/TamperActions.h"
#include "Timecycle/Timecycle.h"
#include "timecycle/TimeCycleConfig.h"
#include "Vfx/VfxHelper.h"
#include "Vfx/Systems/VfxWeather.h"
#include "vfx/systems/VfxLightning.h"
#include "vfx/clouds/Clouds.h"
#include "vfx/clouds/CloudHat.h"
#include "network/Live/NetworkTelemetry.h"

#include "replaycoordinator/ReplayCoordinator.h"

// RageSec Headers
#include "security/ragesecgameinterface.h"
#include "Network/Sessions/NetworkSession.h"

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, weather, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_weather


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define		WEATHER_N2D_FADE_START_TIME					(6*60.0f)						// night to day fade - start time
#define 	WEATHER_N2D_FADE_LENGTH						(60.0f)							// night to day fade - length 
#define 	WEATHER_N2D_FADE_STOP_TIME					(WEATHER_N2D_FADE_START_TIME+WEATHER_N2D_FADE_LENGTH)

#define		WEATHER_D2N_FADE_START_TIME					(19*60.0f)						// day to night fade - start time
#define 	WEATHER_D2N_FADE_LENGTH						(60.0f)							// day to night fade - length
#define 	WEATHER_D2N_FADE_STOP_TIME					(WEATHER_D2N_FADE_START_TIME+WEATHER_D2N_FADE_LENGTH)

dev_float	WEATHER_WIND_INTERP_CHANGE_CHANCE			= 0.3f;
dev_float	WEATHER_WIND_INTERP_CHANGE_STEP				= 0.01f;
dev_float	WEATHER_WIND_MULT_Z_MIN						= 50.0f;
dev_float	WEATHER_WIND_MULT_Z_MAX						= 200.0f;
dev_float	WEATHER_WIND_MULT_VAL						= 2.0f;
dev_float	WEATHER_WIND_SPEED_MAX						= 12.0f;
dev_float	WEATHER_WATER_SPEED_MAX						= 5.0f;

dev_float	WEATHER_WETNESS_RAIN_MULT					= 1.5f;
dev_float	WEATHER_WETNESS_INC_SPEED					= 0.075f;
dev_float	WEATHER_WETNESS_DEC_SPEED					= 0.015f;

dev_float	WEATHER_TEMPERATURE_FADE_HEIGHT_MIN			= 200.0f;
dev_float	WEATHER_TEMPERATURE_FADE_HEIGHT_MAX			= 1000.0f;
dev_float	WEATHER_TEMPERATURE_DROP_AT_MAX_HEIGHT		= 15.0f;


#if	!__FINAL
PARAM(fullweathernames, "[code] display full weather names");
#endif

///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CWeather g_weather;

const char* g_disabledTypeName = "DISABLED";




///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CWeather
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void CWeather::Init(unsigned initMode)
{
    if (initMode==INIT_CORE)
    {
	    m_rageWind.Init(INIT_CORE);
	    m_cycleTimer = 0;
    }
    else if (initMode==INIT_AFTER_MAP_LOADED)
    {
#if !__FINAL
		m_typeNames[0] = g_disabledTypeName;
#endif

		// initialise the number of gpu fx types
		g_vfxWeather.GetPtFxGPUManager().ClearNumSettings();

		// load in the data
		LoadData();

		m_rageWind.Init(INIT_AFTER_MAP_LOADED);
	}
	else if (initMode==INIT_SESSION)
	{
        m_cycleTimer = 0;
	    m_cycleIndex = 0;
	    m_prevTypeIndex = (u32)(m_cycle[m_cycleIndex]);
	    m_nextTypeIndex = (u32)(m_cycle[m_cycleIndex]);
	    m_forceTypeIndex = -1;
	    m_interp = 0.0f;
		m_random = 0.0f;
		m_randomCloud = 0.0f;

		m_networkInitNetworkTime = 0;
		m_networkInitCycleTime = 0;

		m_netTransitionPrevTypeIndex = 0;
        m_netTransitionNextTypeIndex = 0;
        m_netTransitionTimeTotal = 0;
		m_netTransitionTimeLeft = 0;
		m_netTransitionInterp = 0.0f;
		m_netTransitionPrevInterp = 0.0f;
		m_netTransitionNextInterp = 0.0f;
		m_netSendForceUpdate = false;

		m_forceOverTimeTypeIndex = 0xffffffff;
		m_overTimeTransitionTotal = 0.0f;
		m_overTimeTransitionLeft = 0.0f;

		m_sun = 0.0f;
	    m_rain = 0.0f;
	    m_snow = 0.0f;
		m_snowMist = 0.0f;
	    m_wind = 0.0f;
		m_windInterpCurr = 0.0f;
		m_windInterpTarget = 0.0f;
	    m_fog = 0.0f;
	    m_wetness = 0.0f;

	    m_dayNightRatio = 0.0f;

		m_temperatureAtSeaLevel = 0.0f;
		m_temperatureAtCameraHeight = 0.0f;

		m_overrideWind = -0.01f;	
		m_overrideRain = -0.01f;
		m_overrideSnow = -0.01f;	
		m_overrideSnowMist = -0.01f;

		m_overriddenTypeIndex = -1;									
		
		m_overTimeSecondaryWaterTotal = 0.0f;
		m_overTimeSecondaryWaterLeft = 0.0f;
		m_overTimeTarget = 0.0f;
		m_overrideWaterTunings = 0.0f;

		m_hasCheckedCloudTransition = false;
    }
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void CWeather::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
	    m_rageWind.Shutdown(SHUTDOWN_CORE);
    }
}

///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void			CWeather::LoadData							()
{
	m_numTypes = 0;
	m_numCycleEntries = 0;

	// get the current file path
	const char* fileName = NULL;
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::WEATHER_FILE);

	while(DATAFILEMGR.IsValid(pData))
	{
		fileName = pData->m_filename;

		LoadXmlFile(fileName);

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}


void CWeather::InitialiseTypeFromXmlFile(const CWeatherTypeFromXmlFile &TypeDataFromXmlFile)
{
	if (m_numTypes<WEATHER_MAX_TYPES)
	{
		m_types[m_numTypes].m_hashName = atStringHash(TypeDataFromXmlFile.m_Name);

#if !__FINAL
		//
		// Les request to change the displayed weather types to shorter versions (27/02/2012)
		//
		strcpy(m_types[m_numTypes].m_name, TypeDataFromXmlFile.m_Name.c_str());

		if (!PARAM_fullweathernames.Get())
		{
			if (!stricmp(m_types[m_numTypes].m_name, "EXTRASUNNY"))
			{
				safecpy(m_types[m_numTypes].m_name, "EXS", NELEM(m_types[m_numTypes].m_name));
			}
			else if (!stricmp(m_types[m_numTypes].m_name, "CLEAR"))
			{
				safecpy(m_types[m_numTypes].m_name, "CLR", NELEM(m_types[m_numTypes].m_name));
			}
			else if (!stricmp(m_types[m_numTypes].m_name, "CLOUDS"))
			{
				safecpy(m_types[m_numTypes].m_name, "CLS", NELEM(m_types[m_numTypes].m_name));
			}
			else if (!stricmp(m_types[m_numTypes].m_name, "SMOG"))
			{
				safecpy(m_types[m_numTypes].m_name, "SMG", NELEM(m_types[m_numTypes].m_name));
			}
			else if (!stricmp(m_types[m_numTypes].m_name, "CLOUDY"))
			{
				safecpy(m_types[m_numTypes].m_name, "CLD", NELEM(m_types[m_numTypes].m_name));
			}
			else if (!stricmp(m_types[m_numTypes].m_name, "OVERCAST"))
			{
				safecpy(m_types[m_numTypes].m_name, "OVC", NELEM(m_types[m_numTypes].m_name));
			}
			else if (!stricmp(m_types[m_numTypes].m_name, "RAIN"))
			{
				safecpy(m_types[m_numTypes].m_name, "RAI", NELEM(m_types[m_numTypes].m_name));
			}
			else if (!stricmp(m_types[m_numTypes].m_name, "THUNDER"))
			{
				safecpy(m_types[m_numTypes].m_name, "THU", NELEM(m_types[m_numTypes].m_name));
			}
			else if (!stricmp(m_types[m_numTypes].m_name, "CLEARING"))
			{
				safecpy(m_types[m_numTypes].m_name, "CLG", NELEM(m_types[m_numTypes].m_name));
			}
			else if (!stricmp(m_types[m_numTypes].m_name, "NEUTRAL"))
			{
				safecpy(m_types[m_numTypes].m_name, "NEU", NELEM(m_types[m_numTypes].m_name));
			}
			else if (!stricmp(m_types[m_numTypes].m_name, "SNOWLIGHT"))
			{
				safecpy(m_types[m_numTypes].m_name, "SNL", NELEM(m_types[m_numTypes].m_name));
			}
			else if (!stricmp(m_types[m_numTypes].m_name, "SNOW"))
			{
				safecpy(m_types[m_numTypes].m_name, "SNW", NELEM(m_types[m_numTypes].m_name));
			}
			else if (!stricmp(m_types[m_numTypes].m_name, "BLIZZARD"))
			{
				safecpy(m_types[m_numTypes].m_name, "BLI", NELEM(m_types[m_numTypes].m_name));
			}
		}

		m_typeNames[m_numTypes+1] = m_types[m_numTypes].m_name;
#endif

#if __ASSERT
		// check this isn't in the list aready
		for (u32 i=0; i<m_numTypes; i++)
		{
			tcAssertf(m_types[i].m_hashName!=m_types[m_numTypes].m_hashName, "CWeather::LoadData - %s is defined twice in this map's weather types", TypeDataFromXmlFile.m_Name.c_str());
		}
#endif

		m_types[m_numTypes].m_sun = TypeDataFromXmlFile.m_Sun;	
		m_types[m_numTypes].m_cloud = TypeDataFromXmlFile.m_Cloud;
		m_types[m_numTypes].m_windMin = TypeDataFromXmlFile.m_WindMin;
		m_types[m_numTypes].m_windMax = TypeDataFromXmlFile.m_WindMax;
		m_types[m_numTypes].m_rain = TypeDataFromXmlFile.m_Rain;
		m_types[m_numTypes].m_snow = TypeDataFromXmlFile.m_Snow;
		m_types[m_numTypes].m_snowMist = TypeDataFromXmlFile.m_SnowMist;
		m_types[m_numTypes].m_fog = TypeDataFromXmlFile.m_Fog;
		m_types[m_numTypes].m_lightning = TypeDataFromXmlFile.m_Lightning;
		m_types[m_numTypes].m_sandstorm = TypeDataFromXmlFile.m_Sandstorm;

		m_types[m_numTypes].m_secondaryWaterTunings.m_RippleBumpiness = TypeDataFromXmlFile.m_RippleBumpiness;
		m_types[m_numTypes].m_secondaryWaterTunings.m_RippleMinBumpiness = TypeDataFromXmlFile.m_RippleMinBumpiness;
		m_types[m_numTypes].m_secondaryWaterTunings.m_RippleMaxBumpiness = TypeDataFromXmlFile.m_RippleMaxBumpiness;
		m_types[m_numTypes].m_secondaryWaterTunings.m_RippleBumpinessWindScale = TypeDataFromXmlFile.m_RippleBumpinessWindScale;
		m_types[m_numTypes].m_secondaryWaterTunings.m_RippleSpeed = TypeDataFromXmlFile.m_RippleSpeed;
		m_types[m_numTypes].m_secondaryWaterTunings.m_RippleVelocityTransfer = TypeDataFromXmlFile.m_RippleVelocityTransfer;
		m_types[m_numTypes].m_secondaryWaterTunings.m_RippleDisturb = TypeDataFromXmlFile.m_RippleDisturb;
		m_types[m_numTypes].m_secondaryWaterTunings.m_OceanBumpiness = TypeDataFromXmlFile.m_OceanBumpiness;
		m_types[m_numTypes].m_secondaryWaterTunings.m_DeepOceanScale = TypeDataFromXmlFile.m_DeepOceanScale;
		m_types[m_numTypes].m_secondaryWaterTunings.m_OceanNoiseMinAmplitude = TypeDataFromXmlFile.m_OceanNoiseMinAmplitude;
		m_types[m_numTypes].m_secondaryWaterTunings.m_OceanWaveAmplitude = TypeDataFromXmlFile.m_OceanWaveAmplitude;
		m_types[m_numTypes].m_secondaryWaterTunings.m_ShoreWaveAmplitude = TypeDataFromXmlFile.m_ShoreWaveAmplitude;
		m_types[m_numTypes].m_secondaryWaterTunings.m_OceanWaveWindScale = TypeDataFromXmlFile.m_OceanWaveWindScale;
		m_types[m_numTypes].m_secondaryWaterTunings.m_ShoreWaveWindScale = TypeDataFromXmlFile.m_ShoreWaveWindScale;
		m_types[m_numTypes].m_secondaryWaterTunings.m_OceanWaveMinAmplitude = TypeDataFromXmlFile.m_OceanWaveMinAmplitude;
		m_types[m_numTypes].m_secondaryWaterTunings.m_ShoreWaveMinAmplitude = TypeDataFromXmlFile.m_ShoreWaveMinAmplitude;
		m_types[m_numTypes].m_secondaryWaterTunings.m_OceanWaveMaxAmplitude = TypeDataFromXmlFile.m_OceanWaveMaxAmplitude;
		m_types[m_numTypes].m_secondaryWaterTunings.m_ShoreWaveMaxAmplitude = TypeDataFromXmlFile.m_ShoreWaveMaxAmplitude;
		m_types[m_numTypes].m_secondaryWaterTunings.m_OceanFoamIntensity = TypeDataFromXmlFile.m_OceanFoamIntensity;

		m_types[m_numTypes].m_gpuFxDropIndex = g_vfxWeather.GetPtFxGPUManager().GetDropSettingIndex(atStringHash(TypeDataFromXmlFile.m_DropSettingName));

		m_types[m_numTypes].m_gpuFxMistIndex = g_vfxWeather.GetPtFxGPUManager().GetMistSettingIndex(atStringHash(TypeDataFromXmlFile.m_MistSettingName));

		m_types[m_numTypes].m_gpuFxGroundIndex = g_vfxWeather.GetPtFxGPUManager().GetGroundSettingIndex(atStringHash(TypeDataFromXmlFile.m_GroundSettingName));

		sprintf(m_types[m_numTypes].m_timeCycleFilename, "%s", TypeDataFromXmlFile.m_TimeCycleFilename.c_str());

		m_types[m_numTypes].m_CloudSettingsName = TypeDataFromXmlFile.m_CloudSettingsName;

		m_numTypes++;
	}
	else
	{
		// too many types defined
		tcAssertf(0, "CWeather::InitialiseTypeFromXmlFile - too many weather types for this map");
	}
}

void CWeather::InitialiseCycleEntryFromXmlFile(const CWeatherTypeEntry &cycleEntry)
{
	if (m_numCycleEntries<WEATHER_MAX_CYCLE_ENTRIES)
	{
		s32 weatherTypeIndex = GetTypeIndex(cycleEntry.m_CycleName.c_str());

		tcAssertf(weatherTypeIndex>=0 && weatherTypeIndex<(s32)g_weather.GetNumTypes(), "CWeather::InitialiseCycleEntryFromXmlFile - invalid weather type (%s) found in weather xml file", cycleEntry.m_CycleName.c_str());

		if (weatherTypeIndex==-1)
		{
			// unrecognised weather type found - set to zero
			m_cycle[m_numCycleEntries] = 0;
		}
		else
		{
			m_cycle[m_numCycleEntries] = (u8)weatherTypeIndex;
		}

		if (cycleEntry.m_TimeMult<=0)
		{
			m_cycleTimeMult[m_numCycleEntries] = 1;
		}
		else
		{
			m_cycleTimeMult[m_numCycleEntries] = (u8)cycleEntry.m_TimeMult;
		}
		
		
		m_numCycleEntries++;
	}
	else
	{
		// too many cycle entries defined
		tcAssertf(0, "CWeather::InitialiseCycleEntryFromXmlFile - too many weather cycle entries for this map");
	}
}

void CWeather::LoadXmlFile(const char *pFilename)
{
	parSettings parserSettings = PARSER.Settings();
	parserSettings.SetFlag(parSettings::CULL_OTHER_PLATFORM_DATA, true);

	CContentsOfWeatherXmlFile ContentsOfXmlFile;

	PARSER.LoadObject(pFilename, "", ContentsOfXmlFile, &parserSettings);

	s32 index = 0;
	const s32 numOfGpuFx = ContentsOfXmlFile.m_WeatherGpuFx.GetCount();
	while(index < numOfGpuFx)
	{
		g_vfxWeather.GetPtFxGPUManager().LoadSettingFromXmlFile(ContentsOfXmlFile.m_WeatherGpuFx[index]);
		index++;
	}

	index = 0;
	const s32 numOfWeatherTypes = ContentsOfXmlFile.m_WeatherTypes.GetCount();
	while(index < numOfWeatherTypes)
	{
		InitialiseTypeFromXmlFile(ContentsOfXmlFile.m_WeatherTypes[index]);
		index++;
	}

	index = 0;
	const s32 numOfWeatherCycles = ContentsOfXmlFile.m_WeatherCycles.GetCount();
	while(index < numOfWeatherCycles)
	{
		InitialiseCycleEntryFromXmlFile(ContentsOfXmlFile.m_WeatherCycles[index]);
		index++;
	}

    // work out system time
    m_systemTime = 0;
    for(u32 i = 0; i < m_numCycleEntries; i++)
    {
        m_systemTime += (m_msPerCycle * m_cycleTimeMult[i]);
    }

	// check we have valid data
	tcAssertf(m_numTypes>0, "CWeather::LoadXmlFile - no types have been loaded");
	tcAssertf(m_numCycleEntries>0, "CWeather::LoadXmlFile - no cycle entries have been loaded");
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void			CWeather::UpdateVisualDataSettings			()
{
	if (g_visualSettings.GetIsLoaded())
	{
		// in second, used in ms
		float sPerCycle = g_visualSettings.Get("weather.CycleDuration",	2 * 60);
		m_msPerCycle = (u32)(sPerCycle * 1000.0f); 
		m_currentMsPerCycle = m_msPerCycle;
		
		m_changeCloudOnSameCloudType = g_visualSettings.Get("weather.ChangeCloudOnSameCloudTypeChance",	0.5f);
		m_changeCloundOnSameWeatherType = g_visualSettings.Get("weather.ChangeCloudOnSameWeatherTypeChance", 0.5f);
	}		
}

///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void			CWeather::Update							()
{
	// don't update the weather if the game is paused
	if (fwTimer::IsGamePaused())
	{
		return;
	}

#if GTA_REPLAY
	if (CReplayMgr::IsEditModeActive())
	{
		if (!CReplayCoordinator::ShouldShowLoadingScreen())
		{
			// wind and wetness recorded, so don't need updating

			UpdateSun();
			UpdateRain();
			UpdateSnow();
			UpdateSnowMist();
			UpdateWind();
			UpdateFog();
			UpdateLightning();
			UpdateSandstorm();
			UpdateDayNightRatio();
			UpdateTemperature();
			UpdateWaterSettings();
			// rage wind
			m_rageWind.Update(m_windSpeed, m_waterSpeed);
		}
		return;
	}
#endif

	m_random = fwRandom::GetRandomNumberInRange(0.0f,1.0f);
	m_randomCloud = fwRandom::GetRandomNumberInRange(0.0f,1.0f);

	// if we're locked to the network timer
	if(m_networkInitNetworkTime > 0)
	{
		u32 networkTime = NetworkInterface::GetNetworkTime();

		// work out elapsed time since network init
		u32 timePassed;
		if(networkTime > m_networkInitNetworkTime)
			timePassed = networkTime - m_networkInitNetworkTime;
		else
			timePassed = m_networkInitNetworkTime - networkTime;

		// current cycle timer is the initial cycle timer + time elapsed modded by the max cycle time
		m_cycleTimer = m_networkInitCycleTime + (timePassed % m_currentMsPerCycle);
	}
	else
	{
		// update the cycle timer with the current frame delta
		m_cycleTimer += fwTimer::GetTimeStepInMilliseconds();
	}

	if (m_cycleTimer > m_currentMsPerCycle)
	{	
		m_cycleTimer %= m_currentMsPerCycle;
	}

	// calculate the new interpolation depending on the time (this loops from 0.0 to 1.0 each cycle
	float newInterp = (float)m_cycleTimer / (float)m_currentMsPerCycle;

	// the new newInterp value has looped around  - update the type
#if __BANK
	if (m_overrideInterp<0.0f)
#endif
	{
		if (newInterp<m_interp)
		{	
            int prevCycleIndex = m_cycleIndex;

			// the next type now becomes the prev type
			m_prevTypeIndex = m_nextTypeIndex;

			// check if a type is being forced
			if (m_forceTypeIndex>-1)
			{
				// forcing the type - set the next type to be the forced one
				m_nextTypeIndex = m_forceTypeIndex;
			}
			else
			{
				// no force - increment the list index and set the next type
				m_cycleIndex = (m_cycleIndex+1) % m_numCycleEntries;
				m_nextTypeIndex = (u32)(m_cycle[m_cycleIndex]);
				
				// Maybe we can't have rain.
                if(!NetworkInterface::IsApplyingMultiplayerGlobalClockAndWeather())
                {
				    while( g_timeCycle.GetStaleNoRain() > 0.5f  && m_types[m_nextTypeIndex].m_rain > 0.0f )
				    {
					    m_cycleIndex = (m_cycleIndex+1) % m_numCycleEntries;
					    m_nextTypeIndex = (u32)(m_cycle[m_cycleIndex]);
				    }
                }
			}

			m_currentMsPerCycle = m_cycleTimeMult[prevCycleIndex] * m_msPerCycle;
			newInterp = 0.0f;
			m_hasCheckedCloudTransition = false;
		}

		//instead of transitioning the clouds when the cycle changes, use the maximum 
		//cloud hat transition time to work out when to start the transition
		//so the midpoint will occur when weather type switch occurs
		//if cloud hats take less time, they will adjust the start point so it occurs later
		float maxCloudHatTransitionTime = CLOUDHATMGR->GetDesiredTransitionTimeSec() * 1000.0f; 
		float cloudTransitionStartPoint = 1.0f - (maxCloudHatTransitionTime / (float)m_currentMsPerCycle) * 0.5f;
		cloudTransitionStartPoint = Clamp(cloudTransitionStartPoint, 0.0f, 0.99f);

		if(newInterp > cloudTransitionStartPoint && m_hasCheckedCloudTransition == false)
		{
			bool needsCloudTransition = true;
			
			if( m_nextTypeIndex == m_prevTypeIndex )
			{ // Same weather type 
				needsCloudTransition = m_randomCloud > m_changeCloundOnSameWeatherType;
			}
			else
			{
				//weather changed
				CNetworkTelemetry::WeatherChanged(m_prevTypeIndex, m_nextTypeIndex);

				if( GetPrevType().m_CloudSettingsName == GetNextType().m_CloudSettingsName )
				{ // Different weather type, same cloud type.
					needsCloudTransition = m_randomCloud > m_changeCloudOnSameCloudType;
				}
			}
			
			if( needsCloudTransition )
			{
				if ((m_forceOverTimeTypeIndex == 0xFFFFFFFF) && !IsInNetTransition())	 	// skip cloud change is we're in a forced over time / network transition, since it will request a change at the end of the transition
						CClouds::RequestCloudTransition(false, true);
			}

			m_hasCheckedCloudTransition = true;
		}
	}

	if (m_overriddenTypeIndex >= 0)
	{
		// local override only - network sync not required
		m_prevTypeIndex = m_nextTypeIndex = (u32)m_overriddenTypeIndex;
    }

	// set the new newInterp
	m_interp = newInterp;

	// debug
#if !__FINAL
	// check for debug weather controls from the keyboard
	XPARAM(nocheats);
	if (!PARAM_nocheats.Get() && CControlMgr::GetKeyboard().GetKeyJustDown(KEY_PAGEUP, KEYBOARD_MODE_DEBUG, "Next Weather Type"))
	{
		// debug action - kill the override and force the type
		StopOverriding();
		ForceTypeNow((m_nextTypeIndex+1) % m_numTypes, FT_SendNetworkUpdate);
		PostFX::ResetAdaptedLuminance();
		CClouds::RequestCloudTransition(true);
	}

	if (!PARAM_nocheats.Get() && CControlMgr::GetKeyboard().GetKeyJustDown(KEY_PAGEDOWN, KEYBOARD_MODE_DEBUG, "Prev Weather Type"))
	{
		// debug action - kill the override and force the type
		StopOverriding();
		ForceTypeNow((m_nextTypeIndex-1+m_numTypes) % m_numTypes, FT_SendNetworkUpdate);
		PostFX::ResetAdaptedLuminance();
		CClouds::RequestCloudTransition(true);
	}
#endif

	// debug
#if __BANK
	// info
	m_debugPrevTypeIndex = m_prevTypeIndex;
	m_debugNextTypeIndex = m_nextTypeIndex;
	m_debugForceTypeIndex = m_forceTypeIndex;

	// overrides
	if (m_overridePrevTypeIndex>0)
	{
		// the override type has 'disabled' as it's first entry so we need to subtract 1 from it to get an actual type
		m_prevTypeIndex = m_overridePrevTypeIndex-1;

#if GTA_REPLAY
		CReplayMgr::OverrideWeatherPlayback(true);
#endif // GTA_REPLAY
	}
#if GTA_REPLAY
	else
		CReplayMgr::OverrideWeatherPlayback(false);
#endif // GTA_REPLAY

	if (m_overrideNextTypeIndex>0)
	{
		// the override type has 'disabled' as it's first entry so we need to subtract 1 from it to get an actual type
		m_nextTypeIndex = m_overrideNextTypeIndex-1;
	}
	if (m_overrideInterp>=0.0f)
	{
		m_interp = m_overrideInterp;
	}
#endif

	UpdateSun();
	UpdateRain();
	UpdateSnow();
	UpdateSnowMist();
	UpdateWind();
	UpdateFog();
	UpdateLightning();
	UpdateSandstorm();
	UpdateWetness();
	UpdateDayNightRatio();
	UpdateTemperature();
	UpdateWaterSettings();
    
    // update transition to host network weather settings
    UpdateNetworkTransition();
	
	// water tunings
	Water::SetSecondaryWaterTunings(m_secondaryWaterTunings);
	Water::SetRippleWindValue(m_wind);
	
	// rage wind
	m_rageWind.Update(m_windSpeed, m_waterSpeed);

	// Updates for RageSec. We need to report somewhere in game, and where better
	// than in our weather engine?
	//@@: range CWEATHER_UPDATE_RAGESEC_POP_REACTION {
	RAGE_SEC_POP_REACTION
	//@@: } CWEATHER_UPDATE_RAGESEC_POP_REACTION

}


///////////////////////////////////////////////////////////////////////////////
//  UpdateOverTimeTransition
///////////////////////////////////////////////////////////////////////////////

void			CWeather::UpdateOverTimeTransition	()
{
	// Update overtime timer
	if( m_forceOverTimeTypeIndex != 0xFFFFFFFF )
	{
	    // update the cycle timer with the current frame delta
		bool finalizeTransition = true;
#if __BANK
		if( m_debugOverTimeEnableTransitionInterp )
		{
			m_overTimeInterp = g_weather.m_debugOverTimeTransitionInterp;
			finalizeTransition = false;
		}
		else
#endif
		{
			m_overTimeTransitionLeft -= fwTimer::GetTimeStep()*1000.0f; // fwTimer::GetTimeStepInMilliseconds() accumulated a small loss per frame (16ms vs 16.666ms, looses over 4% for example) This meant transitions did not finish in time on PC, and when the script stated a new transiton it would pop.

			m_overTimeInterp = 1.0f - Clamp(g_weather.GetOverTimeLeftTime() / g_weather.GetOverTimeTotalTime(), 0.0f, 1.0f);
		}

		if( m_overTimeInterp == 1.0f && finalizeTransition)
		{
			// TODO: it would be nice to start the cloud transition before the m_overTimeInterp is 1.0.
			bool needsCloudTransition = true;

			if( m_nextTypeIndex == m_forceOverTimeTypeIndex )
			{ // Same weather type 
				needsCloudTransition = m_randomCloud > m_changeCloundOnSameWeatherType;
			}
			else if( GetNextType().m_CloudSettingsName == g_weather.GetTypeInfo(g_weather.GetOverTimeType()).m_CloudSettingsName )
			{ // Different weather type, same cloud type.
				needsCloudTransition = m_randomCloud > m_changeCloudOnSameCloudType;
			}

			if (needsCloudTransition)
				CClouds::RequestCloudTransition();

			g_weather.ForceTypeNow(m_forceOverTimeTypeIndex, FT_NoLumReset);
			g_weather.ForceWeatherTypeOverTime(0xFFFFFFFF, 0.0f);
		}
	}
}

#if GTA_REPLAY
///////////////////////////////////////////////////////////////////////////////
//  SetFromReplay
///////////////////////////////////////////////////////////////////////////////
void CWeather::SetFromReplay(u32 prevWeatherType, u32 nextWeatherType, float interp, float random, float randomCloud, float wetness, u32 forceOverTimeTypeIndex, float overTimeInterp, float overTimeTransitionLeft, float overTimeTransitionTotal)
{
	m_prevTypeIndex = prevWeatherType;
	m_nextTypeIndex = nextWeatherType;
	m_interp = interp;
	m_random = random;
	m_randomCloud = randomCloud;
	m_wetness = wetness;

	m_forceOverTimeTypeIndex = forceOverTimeTypeIndex;
	m_overTimeInterp = overTimeInterp;
	m_overTimeTransitionLeft = overTimeTransitionLeft;
	m_overTimeTransitionTotal = overTimeTransitionTotal;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  RenderDebug
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CWeather::RenderDebug						()
{
	m_rageWind.RenderDebug(); 
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  UpdateSun
///////////////////////////////////////////////////////////////////////////////

void			CWeather::UpdateSun							()
{
#if __BANK
	if (m_overrideSun>=0.0f)
	{
		m_sun = m_overrideSun;
		return;
	}
#endif

	// calc the target sun using the type values
	float prevSun = m_types[m_prevTypeIndex].m_sun;
	float nextSun = m_types[m_nextTypeIndex].m_sun;
	float targetSun = prevSun + ((nextSun-prevSun)*m_interp);

	if( m_forceOverTimeTypeIndex != 0xFFFFFFFF )
	{
		float otSun = m_types[m_forceOverTimeTypeIndex].m_sun;
		targetSun = Lerp(m_overTimeInterp,targetSun,otSun);
	}

	if( m_netTransitionTimeLeft != 0 )
	{
		targetSun = Lerp(SlowInOut(m_netTransitionInterp),m_netTransitionSun,targetSun);
	}

	// disable time delay in replays. we have to take that slight hit or weather flows from one clip to the next
#if GTA_REPLAY
	if (CReplayMgr::IsEditModeActive())
	{
		m_sun = targetSun;
	}
	else
#endif
	{
		// don't let it change too quickly
		float maxDelta = fwTimer::GetTimeStep()*0.1f;
		float delta = targetSun - m_sun;

		if (Abs(delta)<maxDelta)
		{
			// the change is within limits - just set it
			m_sun = targetSun;
		}
		else
		{
			// the change is too big - move towards the target
			if (delta>0.0f)
			{
				m_sun += maxDelta;
			}
			else
			{
				m_sun -= maxDelta;
			}
		}
	}

}


///////////////////////////////////////////////////////////////////////////////
//  UpdateRain
///////////////////////////////////////////////////////////////////////////////

void			CWeather::UpdateRain						()
{
	if (m_overrideRain>=0.0f)
	{
		m_rain = m_overrideRain;
		return;
	}

	// calc the target rain using the type values
	float prevRain = m_types[m_prevTypeIndex].m_rain;
	float nextRain = m_types[m_nextTypeIndex].m_rain;
	float targetRain = prevRain + ((nextRain-prevRain)*m_interp);
	
	if( m_forceOverTimeTypeIndex != 0xFFFFFFFF )
	{
		float otRain = m_types[m_forceOverTimeTypeIndex].m_rain;
		targetRain = Lerp(m_overTimeInterp,targetRain,otRain);
	}

	if( m_netTransitionTimeLeft != 0 )
	{
		targetRain = Lerp(SlowInOut(m_netTransitionInterp),m_netTransitionRain,targetRain);
	}
	
	// randomise it a little
	targetRain *= 0.7f + (0.1f * m_random);

	// disable time delay in replays. we have to take that slight hit or weather flows from one clip to the next
#if GTA_REPLAY
	if (CReplayMgr::IsEditModeActive())
	{
		m_rain = targetRain;
	}
	else
#endif
	{
		// don't let it change too quickly
		float maxDelta = fwTimer::GetTimeStep()*0.1f;
		float delta = targetRain - m_rain;

		if (Abs(delta)<maxDelta)
		{
			// the change is within limits - just set it
			m_rain = targetRain;
		}
		else
		{
			// the change is too big - move towards the target
			if (delta>0.0f)
			{
				m_rain += maxDelta;
			}
			else
			{
				m_rain -= maxDelta;
			}
		}
	}

}


///////////////////////////////////////////////////////////////////////////////
//  UpdateSnow
///////////////////////////////////////////////////////////////////////////////

void			CWeather::UpdateSnow						()
{
	if (m_overrideSnow >= 0.0f)
	{
		m_snow = m_overrideSnow;
	}
	else
	{
		// calc the target snow using the type values
		float prevSnow = m_types[m_prevTypeIndex].m_snow;
		float nextSnow = m_types[m_nextTypeIndex].m_snow;
		float targetSnow = prevSnow + ((nextSnow-prevSnow)*m_interp);

		if( m_forceOverTimeTypeIndex != 0xFFFFFFFF )
		{
			float otSnow = m_types[m_forceOverTimeTypeIndex].m_snow;
			targetSnow = Lerp(m_overTimeInterp,targetSnow,otSnow);
		}

		if( m_netTransitionTimeLeft != 0 )
		{
			targetSnow = Lerp(SlowInOut(m_netTransitionInterp),m_netTransitionSnow,targetSnow);
		}

		// randomise it a little
		targetSnow *= 0.7f + (0.1f * m_random);

		// disable time delay in replays. we have to take that slight hit or weather flows from one clip to the next
#if GTA_REPLAY
		if (CReplayMgr::IsEditModeActive())
		{
			m_snow = targetSnow;
		}
		else
#endif
		{
			// don't let it change too quickly
			float maxDelta = fwTimer::GetTimeStep()*0.1f;
			float delta = targetSnow - m_snow;

			if (Abs(delta)<maxDelta)
			{
				// the change is within limits - just set it
				m_snow = targetSnow;
			}
			else
			{
				// the change is too big - move towards the target
				if (delta>0.0f)
				{
					m_snow += maxDelta;
				}
				else
				{
					m_snow -= maxDelta;
				}
			}
		}
	}
}

void CWeather::UpdateSnowMist()
{
	if (m_overrideSnowMist >= 0.0f)
	{
		m_snowMist = m_overrideSnowMist;
	}
	else
	{
		// calc the target snow mist using the type values
		float prevSnowMist = m_types[m_prevTypeIndex].m_snowMist;
		float nextSnowMist = m_types[m_nextTypeIndex].m_snowMist;
		float targetSnowMist = prevSnowMist + ((nextSnowMist-prevSnowMist)*m_interp);

		if( m_forceOverTimeTypeIndex != 0xFFFFFFFF )
		{
			float otSnowMist = m_types[m_forceOverTimeTypeIndex].m_snowMist;
			targetSnowMist = Lerp(m_overTimeInterp,targetSnowMist,otSnowMist);
		}

		if( m_netTransitionTimeLeft != 0 )
		{
			targetSnowMist = Lerp(SlowInOut(m_netTransitionInterp),m_netTransitionSnowMist,targetSnowMist);
		}

		// disable time delay in replays. we have to take that slight hit or weather flows from one clip to the next
#if GTA_REPLAY
		if (CReplayMgr::IsEditModeActive())
		{
			m_snowMist = targetSnowMist;
		}
		else
#endif
		{
			// don't let it change too quickly
			float maxDelta = fwTimer::GetTimeStep()*0.1f;
			float delta = targetSnowMist - m_snowMist;

			if (Abs(delta)<maxDelta)
			{
				// the change is within limits - just set it
				m_snowMist = targetSnowMist;
			}
			else
			{
				// the change is too big - move towards the target
				if (delta>0.0f)
				{
					m_snowMist += maxDelta;
				}
				else
				{
					m_snowMist -= maxDelta;
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  CalcWind
///////////////////////////////////////////////////////////////////////////////

float			CWeather::CalcWind							(u32 typeIndex)
{
	float minWind = m_types[typeIndex].m_windMin;
	float maxWind = m_types[typeIndex].m_windMax;
	return minWind + ((maxWind-minWind)*m_windInterpCurr);
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateWindInterp
///////////////////////////////////////////////////////////////////////////////

void			CWeather::UpdateWindInterp					()
{
#if __BANK
	if (m_overrideWindInterp>=0.0f)
	{
		m_windInterpCurr = m_overrideWindInterp;
		return;
	}
#endif

	// randomly update the wind interp target value (distance between min and max wind)
	if (Abs(m_windInterpTarget-m_windInterpCurr)<0.001f)
	{
		if ((m_random * 100.0f)<WEATHER_WIND_INTERP_CHANGE_CHANCE)
		{
			m_windInterpTarget = g_DrawRand.GetFloat();
		}
	}

	// make the wind interp curr value blend towards the target
	float windInterpDiff = m_windInterpTarget - m_windInterpCurr;
	float windStep = WEATHER_WIND_INTERP_CHANGE_STEP*fwTimer::GetTimeStep();
	float windInterpChange = Clamp(windInterpDiff, -windStep, windStep);
	m_windInterpCurr += windInterpChange;
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateWind
///////////////////////////////////////////////////////////////////////////////

void			CWeather::UpdateWind						()
{
	UpdateWindInterp();

	// check if the wind is being overridden 
	if (m_overrideWind>=0.0f)
	{
		// it is - set to the override value
		m_wind = m_overrideWind;
	}
	else
	{
		// no override - calc the wind using the type values
		float prevWind = CalcWind(m_prevTypeIndex);
		float nextWind = CalcWind(m_nextTypeIndex);
		m_wind = prevWind + ((nextWind-prevWind)*m_interp);
		
		if( m_forceOverTimeTypeIndex != 0xFFFFFFFF )
		{
			float otWind = CalcWind(m_forceOverTimeTypeIndex);
			m_wind = Lerp(m_overTimeInterp,m_wind,otWind);
		}

		if( m_netTransitionTimeLeft != 0 )
		{
			m_wind = Lerp(SlowInOut(m_netTransitionInterp),m_netTransitionWind,m_wind);
		}
	}

	// increase the wind if the camera is high up
	float playerZ = CGameWorld::FindLocalPlayerHeight();
	if (playerZ>WEATHER_WIND_MULT_Z_MIN)
	{
		if (playerZ>WEATHER_WIND_MULT_Z_MAX)
		{
			m_wind *= WEATHER_WIND_MULT_VAL;
		}
		else
		{
			float ratio = (playerZ-WEATHER_WIND_MULT_Z_MIN)/(WEATHER_WIND_MULT_Z_MAX-WEATHER_WIND_MULT_Z_MIN);
			float mult = (ratio*(WEATHER_WIND_MULT_VAL-1)) + 1;
			m_wind *= mult;
		}
	}

	// make sure it doesn't go over 1.0
	m_wind = Min(m_wind, 1.0f);

	m_windSpeed = m_wind * WEATHER_WIND_SPEED_MAX * g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WIND_SPEED_MULT);
	m_waterSpeed = WEATHER_WATER_SPEED_MAX;
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateFog
///////////////////////////////////////////////////////////////////////////////

void			CWeather::UpdateFog							()
{
#if __BANK
	if (m_overrideFog>=0.0f)
	{
		m_fog = m_overrideFog;
		return;
	}
#endif

	// calc the fog using the type values
	float prevFog = m_types[m_prevTypeIndex].m_fog;
	float nextFog = m_types[m_nextTypeIndex].m_fog;
	
	m_fog = prevFog + ((nextFog-prevFog)*m_interp);
	
	if( m_forceOverTimeTypeIndex != 0xFFFFFFFF )
	{
		float otFog = m_types[m_forceOverTimeTypeIndex].m_fog;
		m_fog = Lerp(m_overTimeInterp,m_fog,otFog);
	}

	if( m_netTransitionTimeLeft != 0 )
	{
		m_fog = Lerp(SlowInOut(m_netTransitionInterp),m_netTransitionFog,m_fog);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateLightning
///////////////////////////////////////////////////////////////////////////////

void			CWeather::UpdateLightning					()
{
	// calc the lightning value
	float lightningChance = 0.0f;
	if (m_types[m_nextTypeIndex].m_lightning)
	{
		lightningChance += m_interp;
	}
	if (m_types[m_prevTypeIndex].m_lightning)
	{
		lightningChance += (1.0f-m_interp);
	}

	if( m_forceOverTimeTypeIndex != 0xFFFFFFFF )
	{
		float otlightning = m_types[m_forceOverTimeTypeIndex].m_lightning;
		lightningChance = Lerp(m_overTimeInterp,lightningChance,otlightning);
	}

	if( m_netTransitionTimeLeft != 0 )
	{
		lightningChance = Lerp(SlowInOut(m_netTransitionInterp),m_netTransitionLightening,lightningChance);
	}

	g_vfxLightningManager.SetLightningChance(lightningChance);
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateSandstorm
///////////////////////////////////////////////////////////////////////////////

void			CWeather::UpdateSandstorm					()
{
#if __BANK
	if (m_overrideSandstorm>=0.0f)
	{
		m_sandstorm = m_overrideSandstorm;
		return;
	}
#endif

	// calc the target sandstorm using the type values
	float prevSandstorm = 0.0f;
	float nextSandstorm = 0.0f;
	if (m_types[m_prevTypeIndex].m_sandstorm)
	{
		prevSandstorm = CalcWind(m_prevTypeIndex);
	}
	if (m_types[m_nextTypeIndex].m_sandstorm)
	{
		nextSandstorm = CalcWind(m_nextTypeIndex);
	}
	float targetSandstorm = prevSandstorm + ((nextSandstorm-prevSandstorm)*m_interp);
	
	if( m_forceOverTimeTypeIndex != 0xFFFFFFFF )
	{
		if( m_types[m_forceOverTimeTypeIndex].m_sandstorm )
		{
			float otSandStorm = CalcWind(m_forceOverTimeTypeIndex);
			targetSandstorm = Lerp(m_overTimeInterp,targetSandstorm,otSandStorm);
		}
	}

	if( m_netTransitionTimeLeft != 0 )
	{
		targetSandstorm = Lerp(SlowInOut(m_netTransitionInterp),m_netTransitionSandstorm,targetSandstorm);
	}
	
	// randomise it a little
	targetSandstorm *= 0.7f + (0.1f * m_random);

	// disable time delay in replays. we have to take that slight hit or weather flows from one clip to the next
#if GTA_REPLAY
	if (CReplayMgr::IsEditModeActive())
	{
		m_sandstorm = targetSandstorm;
	}
	else
#endif
	{
		// don't let it change too quickly
		float maxDelta = fwTimer::GetTimeStep()*0.1f;
		float delta = targetSandstorm - m_sandstorm;

		if (Abs(delta)<maxDelta)
		{
			// the change is within limits - just set it
			m_sandstorm = targetSandstorm;
		}
		else
		{
			// the change is too big - move towards the target
			if (delta>0.0f)
			{
				m_sandstorm += maxDelta;
			}
			else
			{
				m_sandstorm -= maxDelta;
			}
		}
	}

}


///////////////////////////////////////////////////////////////////////////////
//  UpdateWetness
///////////////////////////////////////////////////////////////////////////////

void			CWeather::UpdateWetness					()
{
	// check if the override is set
#if __BANK
	if (m_overrideWetness>=0.0f)
	{
		// it is - use the override value
		m_wetness = m_overrideWetness;
		return;
	}
#endif

	// calc the target wetness based on the rain
	float targetWetness = 0.0f;
	if (m_rain>0.0f)
	{
		targetWetness = Min(1.0f, m_rain*WEATHER_WETNESS_RAIN_MULT);
	}

	float dryingSpeedMult = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_WATER_DRYING_SPEED_MULT);

	// don't let it change too quickly
	float maxDeltaInc = fwTimer::GetTimeStep()*WEATHER_WETNESS_INC_SPEED*m_rain;
	float maxDeltaDec = fwTimer::GetTimeStep()*WEATHER_WETNESS_DEC_SPEED*dryingSpeedMult*(1.0f-m_rain);

	float delta = targetWetness - m_wetness;

	if (delta>0.0f)
	{
		m_wetness = Min(m_wetness+maxDeltaInc, targetWetness);
	}
	else
	{
		m_wetness = Max(m_wetness-maxDeltaDec, targetWetness);
	}

	if( m_netTransitionTimeLeft != 0 )
	{
		m_wetness = Lerp(SlowInOut(m_netTransitionInterp),m_netTransitionWetness,m_wetness);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateDayNightRatio
///////////////////////////////////////////////////////////////////////////////
//
//	describes how day & night color components are mixed
//	0.0	->	full day color
//	0.5	->	half day / half night color
//	1.0	->	full night color
//
///////////////////////////////////////////////////////////////////////////////

void			CWeather::UpdateDayNightRatio				()
{
#if __BANK
	if (m_overrideDayNightRatio>=0.0f)
	{
		m_dayNightRatio = m_overrideDayNightRatio;
		return;
	}
#endif

	float dayToNightRatio = 0.0f;
	const float time = (float)CClock::GetMinutesSinceMidnight();

	if (time<WEATHER_N2D_FADE_START_TIME)
	{
		// night verts
		dayToNightRatio = 1.0f;
	}
	else if (time<WEATHER_N2D_FADE_STOP_TIME)
	{
		// night to day fade (1.0f - 0.0f)
		//@@: location CWEATHER_UPDATEDAYNIGHTRATIO_NIGHT_TO_DAY_FADE
		dayToNightRatio = (WEATHER_N2D_FADE_STOP_TIME-time) / (WEATHER_N2D_FADE_LENGTH);
	}
	else if (time<WEATHER_D2N_FADE_START_TIME)
	{
		// day verts
		dayToNightRatio = 0.0f;
	}
	else if (time<WEATHER_D2N_FADE_STOP_TIME) 
	{
		// day to night face (0.0f - 1.0f)
        //@@: location CWEATHER_UPDATEDAYNIGHTRATIO_DAY_TO_NIGHT_FADE
		dayToNightRatio = 1.0f - ((WEATHER_D2N_FADE_STOP_TIME-time)/(WEATHER_D2N_FADE_LENGTH));
	}
	else
	{
		// night verts
		dayToNightRatio = 1.0f;
	}

	tcAssertf(dayToNightRatio >= -0.0000001f, "CWeather::UpdateDayNightRatio - day to night ratio out of range");
	tcAssertf(dayToNightRatio <= 1.00000001f, "CWeather::UpdateDayNightRatio - day to night ratio out of range");

	m_dayNightRatio = dayToNightRatio;
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateTemperature
///////////////////////////////////////////////////////////////////////////////

void			CWeather::UpdateTemperature					()
{
	m_temperatureAtSeaLevel = GetTemperature(0.0f);
	m_temperatureAtCameraHeight = GetTemperature(CVfxHelper::GetGameCamPos());
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateWaterSettings
///////////////////////////////////////////////////////////////////////////////
void CWeather::UpdateWaterSettings					()
{
	const CSecondaryWaterTune &prevTune = m_types[m_prevTypeIndex].m_secondaryWaterTunings;
	const CSecondaryWaterTune &nextTune = m_types[m_nextTypeIndex].m_secondaryWaterTunings;

	m_secondaryWaterTunings.interpolate(prevTune,nextTune,m_interp);
	
	if( m_forceOverTimeTypeIndex != 0xFFFFFFFF )
	{
		const CSecondaryWaterTune &otTune = m_types[m_forceOverTimeTypeIndex].m_secondaryWaterTunings;
		m_secondaryWaterTunings.interpolate(m_secondaryWaterTunings,otTune,m_overTimeInterp);
	}

	if( m_netTransitionTimeLeft != 0 )
	{
		m_secondaryWaterTunings.interpolate(m_secondaryWaterTunings,m_netTransitionWater,SlowInOut(m_netTransitionInterp));
	}

	if( m_overrideWaterTunings > 0.0f)
	{
		m_secondaryWaterTunings.interpolate(m_secondaryWaterTunings,m_overrideSecondaryWatertunings,m_overrideWaterTunings);
	}
	
	if( m_overTimeSecondaryWaterTotal > 0.0f )
	{
		m_overTimeSecondaryWaterLeft -= (float)fwTimer::GetTimeStepInMilliseconds();
		
		float interp = 1.0f - Clamp(m_overTimeSecondaryWaterLeft/ m_overTimeSecondaryWaterTotal, 0.0f, 1.0f);
		m_overrideWaterTunings = Lerp(interp,1.0f-m_overTimeTarget,m_overTimeTarget);
		if( interp == 1.0f)
		{
			m_overTimeSecondaryWaterTotal = 0.0f;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ForceType
///////////////////////////////////////////////////////////////////////////////

void			CWeather::ForceType							(u32 typeIndex)
{
	if (!gnetVerifyf(typeIndex == 0xFFFFFFFF || (typeIndex >= 0 && typeIndex < g_weather.GetNumTypes()), "Invalid weather type index %lu", (unsigned long)typeIndex))
	{
		return;
	}
	SetForceTypeIndex(typeIndex);
}


///////////////////////////////////////////////////////////////////////////////
//  ForceTypeNow
///////////////////////////////////////////////////////////////////////////////

void			CWeather::ForceTypeNow						(s32 typeIndex, const unsigned flags)
{
    NOTFINAL_ONLY(gnetDebug1("ForceTypeNow :: Forcing type to: %s - SendNetworkUpdate: %s, Global: %s", GetTypeName(typeIndex), ((flags & FT_SendNetworkUpdate) != 0) ? "T" : "F", NetworkInterface::IsApplyingMultiplayerGlobalClockAndWeather() ? "T" : "F");)
		
	if (!gnetVerifyf(typeIndex == -1 || (typeIndex >= 0 && typeIndex < g_weather.GetNumTypes()), "Invalid weather type index %d", typeIndex))
	{
		return;
	}

    if(NetworkInterface::IsApplyingMultiplayerGlobalClockAndWeather())
    {
#if !__FINAL
		// grab this before we apply the new setting
		s32 currentForceTypeIndex = GetForceTypeIndex(); 
#endif

		bool shouldSendMetric = m_nextTypeIndex != (u32) typeIndex; // We don't want to send the metric if the value doesn't change.

		// if we are overriding locally, do not do these
		if (m_overriddenTypeIndex == -1)
		{
			UpdateAllNow(typeIndex, (flags & FT_NoLumReset) != 0);

			// set the prev and next weather to be the force type
			// this will cause it to be applied immediately
			m_prevTypeIndex = typeIndex;
			m_nextTypeIndex = typeIndex;
		}

		// set the force type
		SetForceTypeIndex(typeIndex);

		// weather changed
		if (shouldSendMetric)
		{
			CNetworkTelemetry::WeatherChanged(m_prevTypeIndex, m_nextTypeIndex);
		}

#if !__FINAL
		// sync this over the network so that debug forced weather is synced
		m_netSendForceUpdate = (flags & FT_SendNetworkUpdate) != 0;
		if (m_netSendForceUpdate && (currentForceTypeIndex != typeIndex))
		{
			SendDebugNetworkWeather();
		}
#endif
    }
    else
    {
        UpdateAllNow(typeIndex, (flags & FT_NoLumReset) != 0);

        // set the prev and next weather to be the force type
        // this will cause it to be applied immediately
        m_prevTypeIndex = typeIndex;
        m_nextTypeIndex = typeIndex;

        // set the force type
        SetForceTypeIndex(typeIndex);
    }
}

///////////////////////////////////////////////////////////////////////////////
//  ForceTypeClear
///////////////////////////////////////////////////////////////////////////////

void			CWeather::ForceTypeClear(const unsigned NOTFINAL_ONLY(flags))
{
#if !__FINAL
	s32 forceTypeIndex = GetForceTypeIndex();
#endif

	SetForceTypeIndex(-1);
	ForceWeatherTypeOverTime(0xFFFFFFFF,0.0f);
	
#if !__FINAL
	// sync this over the network so that debug forced weather is synced
	if (((flags & FT_SendNetworkUpdate) != 0) && forceTypeIndex != -1)
	{
		gnetDebug1("ForceTypeClear");
		SendDebugNetworkWeather();
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////
//  SetTypeNow
///////////////////////////////////////////////////////////////////////////////
void			CWeather::SetTypeNow(u32 typeIndex)
{
	ForceTypeClear();
	Assert(typeIndex != -1);

    // find a matching weather and set our current type to match.
    m_prevTypeIndex = typeIndex;

	int index = m_cycleIndex;
	int i=0;
	while ( i<=m_numCycleEntries && ((u32)(m_cycle[index])!=m_prevTypeIndex ))
	{
		index = (index+1) % m_numCycleEntries;
		i++;
	}

	if (i<m_numCycleEntries)
	{
		// Found it, set it
		m_cycleIndex = index;
		
		m_nextTypeIndex = m_prevTypeIndex;
		m_cycleTimer = 0;
		m_interp = 0.0f;
	}
}

///////////////////////////////////////////////////////////////////////////////
//  OverrideType
///////////////////////////////////////////////////////////////////////////////

void			CWeather::OverrideType						(u32 typeIndex, bool resetWetness)
{
    // logging
    NOTFINAL_ONLY(gnetDebug1("OverrideType :: Overriding weather type to: %s", GetTypeName(typeIndex));)

	// update weather now
	UpdateAllNow(typeIndex, false, resetWetness);

	// set the overridden index - we do not set the prev and next indexes 
	m_overriddenTypeIndex = (int)typeIndex;

	//weather changed
	CNetworkTelemetry::WeatherChanged(m_prevTypeIndex, m_overriddenTypeIndex);
}

///////////////////////////////////////////////////////////////////////////////
//  StopOverriding
///////////////////////////////////////////////////////////////////////////////

void			CWeather::StopOverriding					()
{
#if TAMPERACTIONS_ENABLED && TAMPERACTIONS_SNOW
	if(TamperActions::IsSnowing())
	{
		return;
	}
#endif

	if (m_overriddenTypeIndex >= 0)
	{
		m_overriddenTypeIndex = -1;

		if (m_forceTypeIndex>-1)
		{
            // previous force type, assume debug
			const unsigned flags = FT_None NOTFINAL_ONLY(| (m_netSendForceUpdate ? FT_SendNetworkUpdate : 0));
			ForceTypeNow(m_forceTypeIndex, flags);
		}
		else
		{
            m_prevTypeIndex = (u32)(m_cycle[(m_cycleIndex-1+m_numCycleEntries) % m_numCycleEntries]);
			m_nextTypeIndex = (u32)(m_cycle[m_cycleIndex]);
        }

		// weather changed
		if (m_forceTypeIndex >= 0)
			CNetworkTelemetry::WeatherChanged(m_prevTypeIndex, m_forceTypeIndex);
		else
			CNetworkTelemetry::WeatherChanged(m_prevTypeIndex, m_nextTypeIndex);

		// we need to transition back to the global weather
		if(NetworkInterface::IsAnySessionActive())
			CNetwork::StartWeatherTransitionToGlobal();

        // logging
        NOTFINAL_ONLY(gnetDebug1("StopOverriding :: Clearing weather override to: %s", GetTypeName(m_forceTypeIndex >= 0 ? m_forceTypeIndex : m_prevTypeIndex));)
	}
}

///////////////////////////////////////////////////////////////////////////////
//  UpdateAllNow
///////////////////////////////////////////////////////////////////////////////

void			CWeather::UpdateAllNow						(u32 typeIndex, bool noLumReset /* = false */, bool resetWetness/* = false*/)
{
	// if the weather suddenly jumps we have to make sure the tone mapping jumps to the right value (as opposed to slowly adapting)
	if (noLumReset == false && (m_nextTypeIndex!=typeIndex || m_prevTypeIndex!=typeIndex) )
	{
		Water::ResetSimulation();
		PostFX::ResetAdaptedLuminance();
	}

	// instantly set up the new weather state (otherwise these values will blend towards their target values)
	if(resetWetness)
		m_wetness = 0.0f;
	m_wind = CalcWind(typeIndex);
	m_sun = m_types[typeIndex].m_sun;
	m_rain = m_types[typeIndex].m_rain;
	m_snow = m_types[typeIndex].m_snow;
	m_snowMist = m_types[typeIndex].m_snowMist;
	m_fog = m_types[typeIndex].m_fog;
	g_vfxLightningManager.SetLightningChance(m_types[typeIndex].m_lightning);
	m_sandstorm = m_types[typeIndex].m_sandstorm;

	CClouds::RequestCloudTransition(false, true);
}

///////////////////////////////////////////////////////////////////////////////
//  IncrementType
///////////////////////////////////////////////////////////////////////////////

void			CWeather::IncrementType						()
{
    if(!gnetVerify(!NetworkInterface::IsApplyingMultiplayerGlobalClockAndWeather()))
    {
        gnetError("IncrementType :: Cannot be called in network game!");
        return; 
    }

	// calc the curr type
	u32 currTypeIndex;
	if (m_interp<0.5f)
	{
		// closer to the previous type
		currTypeIndex = m_prevTypeIndex;
	}
	else
	{
		// closest to the next type
		currTypeIndex = m_nextTypeIndex;
	}

	// get the next type
	u32 nextTypeIndex = (currTypeIndex+1) % m_numTypes;

	// go through the cycle (from the current position) looking for 2 of the next types beside each other
	u32 i = 0;
	u32 index = m_cycleIndex;
	u32 prevIndex = (index-1+m_numCycleEntries) % m_numCycleEntries;

	while (i<=m_numCycleEntries && ((u32)(m_cycle[prevIndex])!=nextTypeIndex || (u32)(m_cycle[index])!=nextTypeIndex))
	{
		index = (index+1)%m_numCycleEntries;
		prevIndex = (index-1+m_numCycleEntries) % m_numCycleEntries;

		i++;
	}
	
	// check if the previous look up failed
	if (i>m_numCycleEntries)
	{		
		// didn't find 2 of that type next to each other - look for just one this time
		s32 offset = 0;
		if (m_interp<0.5f)
		{
			offset = -1;
		}

		i = 0;
		index = (m_cycleIndex+offset+m_numCycleEntries) % m_numCycleEntries;
		while (i<=m_numCycleEntries && ((u32)(m_cycle[index])!=nextTypeIndex ))
		{
			index = (index+1) % m_numCycleEntries;
			i++;
		}
	}

	// set the next and prev types
	m_cycleIndex = index;
	m_prevTypeIndex = u32(m_cycle[(m_cycleIndex+m_numCycleEntries-1)%m_numCycleEntries]);
	m_nextTypeIndex = u32(m_cycle[m_cycleIndex]);
}


///////////////////////////////////////////////////////////////////////////////
//  RandomiseType
///////////////////////////////////////////////////////////////////////////////

void			CWeather::RandomiseType					()
{
    if(!gnetVerify(!NetworkInterface::IsApplyingMultiplayerGlobalClockAndWeather()))
    {
        gnetError("RandomiseType :: Cannot be called in network game!");
        return; 
    }

	// choose a random place in the weather type list
	m_cycleIndex = fwRandom::GetRandomNumberInRange(0, m_numCycleEntries);

	// set the prev and next weather to be this
	m_prevTypeIndex = (u32)(m_cycle[m_cycleIndex]);
	m_nextTypeIndex = (u32)(m_cycle[m_cycleIndex]);

	UpdateAllNow(m_prevTypeIndex);
}


///////////////////////////////////////////////////////////////////////////////
//  ForceWeatherTypeOverTime
///////////////////////////////////////////////////////////////////////////////

void			CWeather::ForceWeatherTypeOverTime		(u32 typeIndex, float time)
{
	if (!gnetVerifyf(typeIndex == 0xFFFFFFFF || (typeIndex >= 0 && typeIndex < g_weather.GetNumTypes()), "Invalid weather type index %lu", (unsigned long)typeIndex))
	{
		return;
	}

	m_forceOverTimeTypeIndex = typeIndex;
	m_overTimeTransitionTotal = time * 1000.0f;
	m_overTimeTransitionLeft = m_overTimeTransitionTotal;
}


///////////////////////////////////////////////////////////////////////////////
//  NetworkInit
///////////////////////////////////////////////////////////////////////////////

void			CWeather::NetworkInit(u32 networkTime, u32 cycleTime)
{
	// this will be used to apply updates to the cycle timer - making sure we keep in step with the host
	m_networkInitNetworkTime = networkTime;
	m_networkInitCycleTime = cycleTime;

	// final cycle time is the host cycle time plus remainder of the difference
	m_cycleTimer = m_networkInitCycleTime;

	// we need to initialise the interp so that we don't roll over to the 
	// next cycle index based on our current value
	m_interp = static_cast<float>(m_cycleTimer) / static_cast<float>(m_currentMsPerCycle);
}


///////////////////////////////////////////////////////////////////////////////
//  NetworkShutdown
///////////////////////////////////////////////////////////////////////////////

void			CWeather::NetworkShutdown()
{
	// reset network variables
	m_networkInitNetworkTime = 0;
	m_networkInitCycleTime = 0;
}


///////////////////////////////////////////////////////////////////////////////
//  SendDebugNetworkWeather
///////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void			CWeather::SendDebugNetworkWeather	()
{
	if (NetworkInterface::IsGameInProgress())
	{
		gnetDebug1("SendDebugNetworkWeather :: Forced: %s, Prev: %d, Cycle: %d", GetForceTypeIndex() >= 0 ? "T" : "F", GetGlobalPrevTypeIndex(), m_cycleIndex);
		CDebugGameWeatherEvent::Trigger(GetForceTypeIndex() >= 0, GetGlobalPrevTypeIndex(), m_cycleIndex);
	}
}

///////////////////////////////////////////////////////////////////////////////
//  SyncDebugNetworkWeather
///////////////////////////////////////////////////////////////////////////////

void			CWeather::SyncDebugNetworkWeather	(bool force, u32 typeIndex, u32 cycleIndex, u32 transitionTime)
{
    gnetAssertf(typeIndex < m_numTypes, "SyncDebugNetworkWeather :: Type index %d out of range", typeIndex);
    gnetAssertf(cycleIndex < m_numCycleEntries, "SyncDebugNetworkWeather :: Cycle index %d out of range", cycleIndex);

    NOTFINAL_ONLY(gnetDebug1("SyncDebugNetworkWeather :: Forced: %s, Prev: %s, Cycle: %d", force ? "True" : "False", GetTypeName(typeIndex), cycleIndex);)

	if (!gnetVerifyf(typeIndex == 0xFFFFFFFF || (typeIndex >= 0 && typeIndex < g_weather.GetNumTypes()), "Invalid weather type index %lu", (unsigned long)typeIndex))
	{
		return;
	}

	if (force)
	{
        // if forced over network, assume that it came from debug at other end
        ForceTypeNow(typeIndex, FT_None);
	}
	else
	{
		ForceTypeClear();

		if (m_prevTypeIndex!=typeIndex && m_cycleIndex!=(s32)cycleIndex)
		{
			PostFX::ResetAdaptedLuminance();
		}

		// fall in line with host
		m_cycleIndex = cycleIndex;
		m_prevTypeIndex = typeIndex;
		m_nextTypeIndex = (u32)(m_cycle[m_cycleIndex]);

        if(transitionTime != 0)
		{
			// grab current states
			m_netTransitionPrevTypeIndex = m_prevTypeIndex;
			m_netTransitionSun = m_sun;
			m_netTransitionRain = m_rain;
			m_netTransitionSnow = m_snow;
			m_netTransitionSnowMist = m_snowMist;
			m_netTransitionWind = m_wind;
			m_netTransitionFog = m_fog;
			m_netTransitionLightening = g_vfxLightningManager.GetLightningChance();
			m_netTransitionWetness = m_wetness;
			m_netTransitionSandstorm = m_sandstorm;
			m_netTransitionWater = m_secondaryWaterTunings;

			// kick off a transition
			m_netTransitionTimeTotal = transitionTime;
			m_netTransitionTimeLeft = m_netTransitionTimeTotal;
			m_netTransitionInterp = m_interp;
		}
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  NetworkInitFromGlobal
///////////////////////////////////////////////////////////////////////////////

void		CWeather::NetworkInitFromGlobal				(s32 cycleIndex, u32 cycleTime, u32 transitionTime, bool noCloudUpdate)
{
    gnetAssertf(cycleIndex < m_numCycleEntries, "NetworkInitFromGlobal :: Cycle index %d is out of range", cycleIndex);

	ForceTypeClear();

    // grab these before we update
    if(transitionTime != 0)
    {
        // grab current states
        m_netTransitionPrevTypeIndex = m_prevTypeIndex;
        m_netTransitionNextTypeIndex = m_nextTypeIndex;
        m_netTransitionPrevInterp = m_interp;

        m_netTransitionSun = m_sun;
        m_netTransitionRain = m_rain;
        m_netTransitionSnow = m_snow;
        m_netTransitionSnowMist = m_snowMist;
        m_netTransitionWind = m_wind;
        m_netTransitionFog = m_fog;
        m_netTransitionLightening = g_vfxLightningManager.GetLightningChance();
        m_netTransitionWetness = m_wetness;
        m_netTransitionSandstorm = m_sandstorm;
        m_netTransitionWater = m_secondaryWaterTunings;

        // kick off a transition
        m_netTransitionTimeTotal = transitionTime;
        m_netTransitionTimeLeft = m_netTransitionTimeTotal;
        m_netTransitionInterp = 0.0f;
    }

    s32 prevCycleIndex = (cycleIndex-1+m_numCycleEntries) % m_numCycleEntries;
    s32 nextCycleIndex = (cycleIndex+1+m_numCycleEntries) % m_numCycleEntries;
    u32 prevTypeIndex = (u32)(m_cycle[prevCycleIndex]);

	if (m_prevTypeIndex!=prevTypeIndex && m_cycleIndex!=cycleIndex)
	{
		PostFX::ResetAdaptedLuminance();
	}

	// fall in line with global
	m_currentMsPerCycle = m_msPerCycle * m_cycleTimeMult[cycleIndex];
    m_prevTypeIndex = prevTypeIndex;
    m_nextTypeIndex = (u32)(m_cycle[cycleIndex]);
    m_cycleTimer = cycleTime;

    // new interp value
    float newInterp = (float)m_cycleTimer / (float)m_currentMsPerCycle;

#if !__NO_OUTPUT
    if(newInterp > 1.0f)
    {
        gnetError("NetworkInitFromGlobal :: Invalid Interp: %f. Prev: %f, Cycle: %d, Time: %d, MsPerCycle: %d", newInterp, m_interp, cycleIndex, m_cycleTimer, m_currentMsPerCycle);
    }
#endif

    // force the interp and update timer
    m_interp = newInterp;

    // forced
    if(transitionTime == 0)
    {
        // check if we're on a different cycle index
        if(m_cycleIndex != cycleIndex)
        {
            // have we spilled over
            if((nextCycleIndex == m_cycleIndex) && (m_cycleTimer > cycleTime))
            {
				//weather changed
				CNetworkTelemetry::WeatherChanged(m_prevTypeIndex, m_nextTypeIndex);

				// update clouds unless specifically requested otherwise
				if(!noCloudUpdate)
				{
					bool needsCloudTransition = true;
					u32 prevTypeIndex = (m_netTransitionPrevInterp < 0.5) ? m_netTransitionPrevTypeIndex : m_netTransitionNextTypeIndex;
					Assert(prevTypeIndex<m_numTypes);

					if( m_nextTypeIndex == prevTypeIndex )
					{ // Same weather type 
						needsCloudTransition = m_randomCloud > m_changeCloundOnSameWeatherType;
					}
					else if( m_types[prevTypeIndex].m_CloudSettingsName == GetNextType().m_CloudSettingsName )
					{ // Different weather type, same cloud type.
						needsCloudTransition = m_randomCloud > m_changeCloudOnSameCloudType;
					}

					if( needsCloudTransition )
						CClouds::RequestCloudTransition();
				}
            }
        }
    }

    m_cycleIndex = cycleIndex;
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateNetworkTransition
///////////////////////////////////////////////////////////////////////////////

void		CWeather::UpdateNetworkTransition			()
{
    // grab current time
    unsigned currentTime = sysTimer::GetSystemMsTime();

    // update network transition
	if( m_netTransitionTimeTotal != 0 )
	{
		// update the cycle timer with the current frame delta
		u32 timeLeft = m_netTransitionTimeLeft - (currentTime - m_lastSysTime);
		
		if( (timeLeft == 0) || (timeLeft > m_netTransitionTimeLeft) )
		{
			// reset tracking variables
			m_netTransitionInterp = 1.0f;
			m_netTransitionTimeTotal = 0;
			m_netTransitionTimeLeft = 0;

			bool needsCloudTransition = true;
			u32 prevTypeIndex = (m_netTransitionPrevInterp < 0.5) ? m_netTransitionPrevTypeIndex : m_netTransitionNextTypeIndex;
			Assert(prevTypeIndex<m_numTypes);

			if( m_nextTypeIndex == prevTypeIndex )
			{ // Same weather type 
				needsCloudTransition = m_randomCloud > m_changeCloundOnSameWeatherType;
			}
			else if( m_types[prevTypeIndex].m_CloudSettingsName == GetNextType().m_CloudSettingsName )
			{ // Different weather type, same cloud type.
				needsCloudTransition = m_randomCloud > m_changeCloudOnSameCloudType;
			}

			if( needsCloudTransition )
				CClouds::RequestCloudTransition();
		}
		else
		{
			m_netTransitionTimeLeft = timeLeft;
			m_netTransitionInterp = 1.0f - Clamp(static_cast<float>(m_netTransitionTimeLeft) / static_cast<float>(m_netTransitionTimeTotal), 0.0f, 1.0f);
		}
    }

    // stash time for next time round
    m_lastSysTime = currentTime;
}


///////////////////////////////////////////////////////////////////////////////
//  GetGlobalPrevTypeIndex
///////////////////////////////////////////////////////////////////////////////

u32			CWeather::GetGlobalPrevTypeIndex			()
{
	// retrieve the current global previous weather index (this does not consider
	// the current local override)
	if (m_overriddenTypeIndex>-1)
	{
		if (m_forceTypeIndex>-1)
		{
			return m_forceTypeIndex;
		}
		else
		{
			return (u32)(m_cycle[(m_cycleIndex-1+m_numCycleEntries) % m_numCycleEntries]);
		}
	}
	else
	{
		return m_prevTypeIndex;
	}
}

///////////////////////////////////////////////////////////////////////////////
//  GetFromRatio
///////////////////////////////////////////////////////////////////////////////

void		CWeather::GetFromRatio						(float ratio, s32& cycleIndex, u32& cycleTimer)
{
	// time into full cycle
	u32 timeIntoSystem = static_cast<u32>(ratio * static_cast<float>(GetSystemTime()));
    u32 timeLeft = timeIntoSystem;

    // reset
    cycleIndex = 0;
    cycleTimer = 0;

    // work out where we are
    for(u32 i = 0; i < m_numCycleEntries; i++)
    {
        u32 cycleLength = m_msPerCycle * m_cycleTimeMult[i];
        if((timeLeft - cycleLength) < timeLeft)
        {
            // decrement and move to next cycle
            timeLeft -= cycleLength;
            cycleIndex++;
        }
        else
        {
            // cycle timer is what's left
            cycleTimer = timeLeft;
            break;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
//  GetDisplayCode
///////////////////////////////////////////////////////////////////////////////
#if !__NO_OUTPUT
const	char*				CWeather::GetDisplayCode()
{
	if(GetIsForced())
		return "f";
	if(IsInOvertimeTransition())
		return "t";
	else if(GetNetIsOverriding())
		return "o";

	return "";
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  GetNetTransitionPrevType
///////////////////////////////////////////////////////////////////////////////

const	CWeatherType&		CWeather::GetNetTransitionPrevType()
{
    int prevType = m_netTransitionPrevTypeIndex;
    Assert(prevType<m_numTypes); 
    return m_types[prevType];
}


///////////////////////////////////////////////////////////////////////////////
//  GetNetTransitionNextType
///////////////////////////////////////////////////////////////////////////////

const	CWeatherType&		CWeather::GetNetTransitionNextType()
{
    int nextType = m_netTransitionNextTypeIndex;
    Assert(nextType<m_numTypes); 
    return m_types[nextType];
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessEntityWindDisturbance
///////////////////////////////////////////////////////////////////////////////

void		CWeather::ProcessEntityWindDisturbance		(CEntity* pEntity, const CWindDisturbanceAttr* pWindEffect, s32 UNUSED_PARAM(effectId))
{
	// get the local position and rotation
	Vec3V vPosLcl;
	pWindEffect->GetPos(RC_VECTOR3(vPosLcl));

	QuatV vRotLcl(pWindEffect->qX, pWindEffect->qY, pWindEffect->qZ, pWindEffect->qW);

	// calc the local matrix
	Mat34V vMtxLcl = Mat34V(V_IDENTITY);
	Mat34VFromQuatV(vMtxLcl, vRotLcl, vPosLcl);

	// get the bone we're attached to 
	Mat34V vMtxBone;
	CVfxHelper::GetMatrixFromBoneTag(vMtxBone, pEntity, (eAnimBoneTag)pWindEffect->m_boneTag);

	// return if we don't have a valid matrix
	if (IsZeroAll(vMtxBone.GetCol0()))
	{
		return;
	}

	// calc the world  matrix
	Mat34V vMtxWld = Mat34V(V_IDENTITY);
	Transform(vMtxWld, vMtxBone, vMtxLcl);

	// check that we're in range
	if (CVfxHelper::GetDistSqrToCamera(vMtxWld.GetCol3())>30.0f*30.0f)
	{
		return;
	}

//	grcDebugDraw::Axis(vMtxWld, 1.0f);

	// get the type
	if (pWindEffect->m_disturbanceType==WT_HEMISPHERE)
	{
		float strength = pWindEffect->m_strength;
		if (pWindEffect->m_strengthMultipliesGlobalWind)
		{
			strength = GetWindSpeed() * pWindEffect->m_strength;
		}

		AddWindHemisphere(vMtxWld.GetCol3(), vMtxWld.GetCol0(), pWindEffect->m_sizeX, strength);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  AddWindThermal
///////////////////////////////////////////////////////////////////////////////

s32			CWeather::AddWindThermal					(Vec3V_In vPos, float radius, float height, float maxStrength, float deltaStrength)
{
	phThermalSettings thermalSettings;
	thermalSettings.radius = radius;
	thermalSettings.height = height;
	thermalSettings.maxStrength = maxStrength;
	thermalSettings.deltaStrength = deltaStrength;
	thermalSettings.shrinkChance = 0.0f;

	return g_weather.m_rageWind.AddThermal(vPos, thermalSettings);
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveWindThermal
///////////////////////////////////////////////////////////////////////////////

void			CWeather::RemoveWindThermal				(s32 index)
{
	g_weather.m_rageWind.RemoveThermal(index);
}


///////////////////////////////////////////////////////////////////////////////
//  AddWindExplosion
///////////////////////////////////////////////////////////////////////////////

void			CWeather::AddWindExplosion					(Vec3V_In vPos, float radius, float force)
{
#if GTA_REPLAY
	if(!ShouldAddPerFrameWind())
		return;
#endif // GTA_REPLAY

	phExplosionSettings expSettings;
	expSettings.radius = radius;
	expSettings.force = force;

	g_weather.m_rageWind.AddExplosion(vPos, expSettings);
}


///////////////////////////////////////////////////////////////////////////////
//  AddWindDirExplosion
///////////////////////////////////////////////////////////////////////////////

void			CWeather::AddWindDirExplosion					(Vec3V_In vPos, Vec3V_In vDir_In, float length, float radius, float force)
{
#if GTA_REPLAY
	if(!ShouldAddPerFrameWind())
		return;
#endif // GTA_REPLAY

	Vec3V vDir = vDir_In;
	phDirExplosionSettings dirExpSettings;
	dirExpSettings.vDir = vDir;
	dirExpSettings.length = length;
	dirExpSettings.radius = radius;
	dirExpSettings.force = force;

	g_weather.m_rageWind.AddDirExplosion(vPos, dirExpSettings);
}


///////////////////////////////////////////////////////////////////////////////
//  AddWindDownwash
///////////////////////////////////////////////////////////////////////////////

void			CWeather::AddWindDownwash					(Vec3V_In vPos, Vec3V_In vDir_In, float radius, float force, float groundZ, float zFadeThreshMin, float zFadeThreshMax)
{
#if GTA_REPLAY
	if(!ShouldAddPerFrameWind())
		return;
#endif // GTA_REPLAY

	Vec3V vDir = vDir_In;
	phDownwashSettings downwashSettings;
	downwashSettings.vDir = vDir;
	downwashSettings.radius = radius;
	downwashSettings.force = force;
	downwashSettings.groundZ = groundZ;
	downwashSettings.zFadeThreshMin = zFadeThreshMin;
	downwashSettings.zFadeThreshMax = zFadeThreshMax;

	g_weather.m_rageWind.AddDownwash(vPos, downwashSettings);
}


///////////////////////////////////////////////////////////////////////////////
//  AddWindHemisphere
///////////////////////////////////////////////////////////////////////////////

void			CWeather::AddWindHemisphere					(Vec3V_In vPos, Vec3V_In vDir_In, float radius, float force)
{
#if GTA_REPLAY
	if(!ShouldAddPerFrameWind())
		return;
#endif // GTA_REPLAY

	Vec3V vDir = vDir_In;
	phHemisphereSettings hemisphereSettings;
	hemisphereSettings.vDir = vDir;
	hemisphereSettings.radius = radius;
	hemisphereSettings.force = force;

	g_weather.m_rageWind.AddHemisphere(vPos, hemisphereSettings);
}


///////////////////////////////////////////////////////////////////////////////
//  AddWindSphere
///////////////////////////////////////////////////////////////////////////////
void			CWeather::AddWindSphere				(Vec3V_In vPos, Vec3V_In vVelocity, float radius, float forceMult)
{
#if GTA_REPLAY
	if(!ShouldAddPerFrameWind())
		return;
#endif // GTA_REPLAY

	phSphereSettings sphereSettings;
	sphereSettings.forceMult = forceMult;
	sphereSettings.radius = radius;
	sphereSettings.vVelocity = vVelocity;

	g_weather.m_rageWind.AddSphere(vPos, sphereSettings);
}

///////////////////////////////////////////////////////////////////////////////
//  AddWindSphere
///////////////////////////////////////////////////////////////////////////////
#if GTA_REPLAY
bool	CWeather::ShouldAddPerFrameWind				()
{
	if(CReplayMgr::IsReplayInControlOfWorld() && fwTimer::IsGamePaused())
		return false;

	return true;
}
#endif // GTA_REPLAY

///////////////////////////////////////////////////////////////////////////////
//  GetPrevType
///////////////////////////////////////////////////////////////////////////////

const	CWeatherType&		CWeather::GetPrevType					()
{
	int prevType = m_prevTypeIndex;
    Assert(prevType<m_numTypes); 
	return m_types[prevType];
}


///////////////////////////////////////////////////////////////////////////////
//  GetTemperature
///////////////////////////////////////////////////////////////////////////////

float			CWeather::GetTemperature					(Vec3V_In vPos)
{
	return GetTemperature(vPos.GetZf());
}

float			CWeather::GetTemperature					(float height)
{
	// below the min fade height we use the base temperature
	float baseTemp = g_timeCycle.GetStaleTemperature();

	if (height<WEATHER_TEMPERATURE_FADE_HEIGHT_MIN)
	{
		// below the min we fade height we use the base temperature
		return baseTemp;
	}
	else if (height>WEATHER_TEMPERATURE_FADE_HEIGHT_MAX)
	{
		// above the max fade we height subtract the full temperature drop off from the base temperature
		return baseTemp-WEATHER_TEMPERATURE_DROP_AT_MAX_HEIGHT;
	}
	else
	{
		// otherwise we subtract part of the temperature drop off from the base temperature
		float d = CVfxHelper::GetInterpValue(height, WEATHER_TEMPERATURE_FADE_HEIGHT_MIN, WEATHER_TEMPERATURE_FADE_HEIGHT_MAX);
		return baseTemp-(d*WEATHER_TEMPERATURE_DROP_AT_MAX_HEIGHT);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  InitLevelWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CWeather::InitLevelWidgets					()
{
	m_overrideInterp = -0.01f;

	m_overrideSun = -0.01f;
	//m_overrideRain = -0.01f;			
	//m_overrideSnow = -0.01f;
	//m_overrideSnowMist = -0.01f;
	m_overrideWindInterp = -0.01f;
	//m_overrideWind = -0.01f;		
	m_overrideFog = -0.01f;		
	m_overrideWetness = -0.01f;	
	m_overrideSandstorm = -0.01f;
	m_overrideDayNightRatio = -0.01f;
	
	// create a new weather bank
	tcAssertf(BANKMGR.FindBank("Weather")==NULL, "CWeather::InitDebugWidgets - bank widget is already set up");
	bkBank* pBank = &BANKMGR.CreateBank("Weather", 0, 0, false);
	pBank->PushGroup("Weather System", false);
	{
		pBank->PushGroup("Info", false);
		{
			pBank->AddCombo("Prev Type", (s32*)&m_debugPrevTypeIndex, m_numTypes, GetTypeNames(1));
			pBank->AddCombo("Next Type", (s32*)&m_debugNextTypeIndex, m_numTypes, GetTypeNames(1));
			pBank->AddCombo("Force Type", (s32*)&m_debugForceTypeIndex, m_numTypes, GetTypeNames(1));
			pBank->AddSlider("Interp", &m_interp, 0.0f, 1.0f, 0.0f);
			pBank->AddSlider("Base Milliseconds Per Cycle", &m_msPerCycle, 5000, WEATHER_MAX_CYCLE_TIME, 1000);
			pBank->AddSlider("Current Milliseconds Per Cycle", &m_currentMsPerCycle, 5000, WEATHER_MAX_CYCLE_TIME, 1000);
			pBank->AddSlider("Cycle Index", &m_cycleIndex, 0, WEATHER_MAX_CYCLE_ENTRIES, 0);

			pBank->AddSlider("Sun", &m_sun, 0.0f, 1.0f, 0.0f);
			pBank->AddSlider("Rain", &m_rain, 0.0f, 1.0f, 0.0f);
			pBank->AddSlider("Snow", &m_snow, 0.0f, 1.0f, 0.0f);
			pBank->AddSlider("Snow Mist", &m_snowMist, 0.0f, 1.0f, 0.0f);
			pBank->AddSlider("Wind Interp Target", &m_windInterpTarget, 0.0f, 1.0f, 0.0f);
			pBank->AddSlider("Wind Interp Curr", &m_windInterpCurr, 0.0f, 1.0f, 0.0f);
			pBank->AddSlider("Wind", &m_wind, 0.0f, 1.0f, 0.0f);
			pBank->AddSlider("Fog", &m_fog, 0.0f, 1.0f, 0.0f);
			pBank->AddSlider("Wetness", &m_wetness, 0.0f, 1.0f, 0.0f);
			g_vfxLightningManager.AddLightningBurstInfoWidgets(pBank);
			pBank->AddSlider("Sandstorm", &m_sandstorm, 0.0f, 1.0f, 0.0f);

			m_secondaryWaterTunings.InitWidgets(pBank);

			pBank->AddSlider("Day Night Ratio", &m_dayNightRatio, 0.0f, 1.0f, 0.0f);
			pBank->AddSlider("Temperature (Sea Level)", &m_temperatureAtSeaLevel, -100.0f, 100.0f, 0.0f);
			pBank->AddSlider("Temperature (Camera)", &m_temperatureAtCameraHeight, -100.0f, 100.0f, 0.0f);

			pBank->AddCombo("Overtime Type", (s32*)&m_forceOverTimeTypeIndex, m_numTypes, GetTypeNames(1));
			pBank->AddSlider("Overtime Total",&m_overTimeTransitionTotal, 0.0f, 30.0f*1000.0f, 0.0f);
			pBank->AddSlider("Overtime Left",&m_overTimeTransitionLeft, 0.0f, 30.0f*1000.0f, 0.0f);
			pBank->AddSlider("Overtime Interp",&m_overTimeInterp, 0.0f, 1.0f, 0.0f);
			pBank->AddSlider("Num Types", &m_numTypes, 0, WEATHER_MAX_TYPES, 0);
			pBank->AddSlider("Num Cycle Entries", &m_numCycleEntries, 0, WEATHER_MAX_CYCLE_ENTRIES, 0);

			pBank->AddSlider("Network Transition Total", &m_netTransitionTimeTotal, 0, 30000, 0);
			pBank->AddSlider("Network Transition Left",&m_netTransitionTimeLeft, 0, 30000, 0);
			pBank->AddSlider("Network Interp",&m_netTransitionInterp, 0.0f, 1.0f, 0.0f);
			pBank->AddSlider("Prev Type Index",&m_prevTypeIndex, 0, WEATHER_MAX_TYPES, 0);
			pBank->AddSlider("Next Type Index",&m_nextTypeIndex, 0, WEATHER_MAX_TYPES, 0);
			pBank->AddSlider("Network Cycle Index", &m_cycleIndex, 0, WEATHER_MAX_CYCLE_ENTRIES, 0);
			pBank->AddSlider("Network Prev Interp",&m_netTransitionPrevInterp, 0.0f, 1.0f, 0.0f);
			pBank->AddSlider("Network Next Interp",&m_netTransitionNextInterp, 0.0f, 1.0f, 0.0f);
			pBank->AddSlider("Network Prev Type Index", &m_netTransitionPrevTypeIndex, 0, WEATHER_MAX_TYPES, 0);
			pBank->AddSlider("Network Next Type Index", &m_netTransitionNextTypeIndex, 0, WEATHER_MAX_TYPES, 0);
		}
		pBank->PopGroup();

#if __DEV
		pBank->PushGroup("Settings", false);
		{
			pBank->AddSlider("Wind Interp Change Chance", &WEATHER_WIND_INTERP_CHANGE_CHANCE, 0.0f, 100.0f, 0.1f);
			pBank->AddSlider("Wind Interp Change Step", &WEATHER_WIND_INTERP_CHANGE_STEP, 0.0f, 1.0f, 0.0001f);
			pBank->AddSlider("Wind Speed Max", &WEATHER_WIND_SPEED_MAX, 0.0f, 100.0f, 0.1f);
			pBank->AddSlider("Water Speed Max", &WEATHER_WATER_SPEED_MAX, 0.0f, 100.0f, 0.1f);
			pBank->AddSlider("Wind Mult Z Min", &WEATHER_WIND_MULT_Z_MIN, 0.0f, 2000.0f, 5.0f, NullCB, "");
			pBank->AddSlider("Wind Mult Z Max", &WEATHER_WIND_MULT_Z_MAX, 0.0f, 2000.0f, 5.0f, NullCB, "");
			pBank->AddSlider("Wind Mult Value", &WEATHER_WIND_MULT_VAL, 0.0f, 10.0f, 0.1f, NullCB, "");
			
			pBank->AddSlider("Wetness Rain Mult", &WEATHER_WETNESS_RAIN_MULT, 0.0f, 10.0f, 0.1f, NullCB, "");
			pBank->AddSlider("Wetness Inc Speed", &WEATHER_WETNESS_INC_SPEED, 0.0f, 1.0f, 0.01f, NullCB, "");
			pBank->AddSlider("Wetness Dec Speed", &WEATHER_WETNESS_DEC_SPEED, 0.0f, 1.0f, 0.01f, NullCB, "");

			pBank->AddSlider("Temperature Fade Height Min", &WEATHER_TEMPERATURE_FADE_HEIGHT_MIN, 0.0f, 5000.0f, 10.0f, NullCB, "");
			pBank->AddSlider("Temperature Fade Height Max", &WEATHER_TEMPERATURE_FADE_HEIGHT_MAX, 0.0f, 5000.0f, 10.0f, NullCB, "");
			pBank->AddSlider("Temperature Drop at Max Height", &WEATHER_TEMPERATURE_DROP_AT_MAX_HEIGHT, 0.0f, 50.0f, 1.0f, NullCB, "");
		}
		pBank->PopGroup();
#endif

		pBank->PushGroup("Debug", false);
		{
			pBank->AddCombo("Override Prev Type", (s32*)&m_overridePrevTypeIndex, m_numTypes+1, GetTypeNames(0));
			pBank->AddCombo("Override Next Type", (s32*)&m_overrideNextTypeIndex, m_numTypes+1, GetTypeNames(0));
			pBank->AddSlider("Override Interp", &m_overrideInterp, -0.01f, 1.0f, 0.01f);
			pBank->AddSlider("Override Sun", &m_overrideSun, -0.01f, 1.0f, 0.01f);
			pBank->AddSlider("Override Rain", &m_overrideRain, -0.01f, 1.0f, 0.01f);
			pBank->AddSlider("Override Snow", &m_overrideSnow, -0.01f, 1.0f, 0.01f);
			pBank->AddSlider("Override Snow Mist", &m_overrideSnowMist, -0.01f, 1.0f, 0.01f);
			pBank->AddSlider("Override Wind Interp", &m_overrideWindInterp, -0.01f, 1.0f, 0.01f);
			pBank->AddSlider("Override Wind", &m_overrideWind, -0.01f, 1.0f, 0.01f);
			pBank->AddSlider("Override Fog", &m_overrideFog, -0.01f, 1.0f, 0.01f);
			pBank->AddSlider("Override Wetness", &m_overrideWetness, -0.01f, 1.0f, 0.01f);
			g_vfxLightningManager.AddLightningBurstDebugWidgets(pBank);
			pBank->AddSlider("Override Sandstorm", &m_overrideSandstorm, -0.01f, 1.0f, 0.01f);
			
			pBank->AddSlider("Override Water",&m_overrideWaterTunings,0.0f,1.0f,0.01f);
			pBank->AddButton("Copy Current settings to Override",CopyWaterTunings);
			pBank->AddButton("Save Temp Water tunning",SaveTempWaterTunings);
			m_overrideSecondaryWatertunings.InitWidgets(pBank);
			
			pBank->AddSlider("Override Day Night Ratio", &m_overrideDayNightRatio, -0.01f, 1.0f, 0.01f);
			pBank->AddButton("Increment Type", DebugIncrementType);
#if __DEV
			pBank->AddButton("Random Type", DebugRandomiseType);
#endif
			pBank->AddButton("lightning", ForceLightningFlash);
			
			pBank->AddCombo("Force OverTime Type", (s32*)&m_debugForceOverTimeTypeIndex, m_numTypes+1, GetTypeNames(0));
			pBank->AddSlider("Transition Time",&m_debugOverTimeTransition,0.0f,20.0f,1.0f);
			pBank->AddToggle("Enable Transition Interp Override",&m_debugOverTimeEnableTransitionInterp);
			pBank->AddSlider("Transition Interp",&m_debugOverTimeTransitionInterp,0.0f,1.0f,0.1f);

			pBank->AddButton("Start OverTime Transition", ForceOverTimeTransition);
		}
		pBank->PopGroup();
	}
	pBank->PopGroup();

	m_rageWind.InitLevelWidgets();
}

///////////////////////////////////////////////////////////////////////////////
//  ShutdownLevelWidgets
///////////////////////////////////////////////////////////////////////////////
void		 	CWeather::ShutdownLevelWidgets					()
{
	m_rageWind.ShutdownLevelWidgets();

	bkBank* pBank = BANKMGR.FindBank("Weather");
	if (AssertVerify(pBank))
	{
		BANKMGR.DestroyBank(*pBank);
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  DebugIncrementType
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CWeather::DebugIncrementType				()
{
	g_weather.IncrementType();
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  DebugRandomiseType
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CWeather::DebugRandomiseType				()
{
#if __DEV
	g_weather.RandomiseType();
#endif
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  DebugRandomiseType
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CWeather::SaveTempWaterTunings				()
{
	PARSER.SaveObject("common:/non_final/tempWatertuning", "xml", &g_weather.m_overrideSecondaryWatertunings);
}

void		 	CWeather::CopyWaterTunings				()
{
	g_weather.m_overrideSecondaryWatertunings = g_weather.m_secondaryWaterTunings;
}

#endif


///////////////////////////////////////////////////////////////////////////////
//  MakeWaterFlat
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CWeather::MakeWaterFlat				()
{
	m_overrideWaterTunings = 1.0f;

	m_overrideSecondaryWatertunings.m_ShoreWaveAmplitude    = 0.0f;
	m_overrideSecondaryWatertunings.m_ShoreWaveMinAmplitude = 0.0f;
	m_overrideSecondaryWatertunings.m_ShoreWaveMaxAmplitude = 0.0f;
	m_overrideSecondaryWatertunings.m_OceanNoiseMinAmplitude= 0.0f;
	m_overrideSecondaryWatertunings.m_OceanWaveAmplitude    = 0.0f;
	m_overrideSecondaryWatertunings.m_OceanWaveMinAmplitude = 0.0f;
	m_overrideSecondaryWatertunings.m_OceanWaveMaxAmplitude = 0.0f;
	m_overrideSecondaryWatertunings.m_RippleBumpiness       = 0.0f;
	m_overrideSecondaryWatertunings.m_RippleMinBumpiness    = 0.0f;
	m_overrideSecondaryWatertunings.m_RippleMaxBumpiness    = 0.0f;
	m_overrideSecondaryWatertunings.m_RippleDisturb         = 0.0f;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  ForceLightningFlash
///////////////////////////////////////////////////////////////////////////////

void		 	CWeather::ForceLightningFlash				()
{
	g_vfxLightningManager.ForceLightningFlash();
}


///////////////////////////////////////////////////////////////////////////////
//  ForceOverTimeTransition
///////////////////////////////////////////////////////////////////////////////
#if __BANK
void		 	CWeather::ForceOverTimeTransition			()
{
	if( g_weather.m_debugForceOverTimeTypeIndex > 0 )
	{
		g_weather.ForceWeatherTypeOverTime(g_weather.m_debugForceOverTimeTypeIndex-1, g_weather.m_debugOverTimeTransition);
	}
}
#endif // __BANK

///////////////////////////////////////////////////////////////////////////////
//  GetTypeIndex
///////////////////////////////////////////////////////////////////////////////
void		CWeather::FadeInWaterOverrideOverTime(float time)
{
	m_overTimeSecondaryWaterTotal = time * 1000.f;
	m_overTimeSecondaryWaterLeft = m_overTimeSecondaryWaterTotal;
	m_overTimeTarget = 1.0f;
}

///////////////////////////////////////////////////////////////////////////////
//  GetTypeIndex
///////////////////////////////////////////////////////////////////////////////
void		CWeather::FadeOutWaterOverrideOverTime(float time)
{
	m_overTimeSecondaryWaterTotal = time * 1000.f;
	m_overTimeSecondaryWaterLeft = m_overTimeSecondaryWaterTotal;
	m_overTimeTarget = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////
//  GetTypeIndex
///////////////////////////////////////////////////////////////////////////////

s32			CWeather::GetTypeIndex							(const char* typeName)
{
	u32 hashName = atStringHash(typeName);

	for (u32 i=0; i<m_numTypes; i++)
	{
		if (m_types[i].m_hashName==hashName)
		{
			return i;
		}
	}

	tcAssertf(0, "CWeather::GetTypeIndex - invalid weather type - %s", typeName);
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
//  GetTypeIndex
///////////////////////////////////////////////////////////////////////////////

s32			CWeather::GetTypeIndex							(u32 typeNameHash)
{
	for (u32 i=0; i<m_numTypes; i++)
	{
		if (m_types[i].m_hashName==typeNameHash)
		{
			return i;
		}
	}

	tcAssertf(0, "CWeather::GetTypeIndex - invalid weather type - %u", typeNameHash);
	return -1;
}


