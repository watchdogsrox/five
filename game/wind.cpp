///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	Wind.cpp
//	BY	: 	Mark Nicholson 
//	FOR	:	Rockstar North
//	ON	:	18 Mar 2009
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "Wind.h"

// rage
#include "profile/timebars.h"
#include "pheffects/wind.h"

// game 
#include "Camera/CamInterface.h"
#include "control/replay/replay.h"
#include "Core/Game.h"
#include "Peds/Ped.h"
#include "Renderer/River.h"
#include "Scene/Portals/Portal.h"
#include "Scene/World/GameWorld.h"
#include "fwsys/timer.h"
#include "Vfx/VfxHelper.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

// rag tweakable settings
bank_float		WIND_UPDATE_DIR_CHANCE							= 0.3f;
bank_float		WIND_UPDATE_DIR_STEP							= 0.05f;
bank_float		WIND_RIVER_VELOCITY_MULTIPLIER					= 40.0f;

// gusts														   air		water
bank_float		WIND_GUST_LO_CHANCE[WIND_TYPE_NUM]				= {2.0f,	0.0f};
bank_float		WIND_GUST_LO_SPEED_MULT_MIN[WIND_TYPE_NUM]		= {0.5f,	0.0f};
bank_float		WIND_GUST_LO_SPEED_MULT_MAX[WIND_TYPE_NUM]		= {1.0f,	0.0f};
bank_float		WIND_GUST_LO_TIME_MIN[WIND_TYPE_NUM]			= {2.0f,	0.0f};
bank_float		WIND_GUST_LO_TIME_MAX[WIND_TYPE_NUM]			= {3.0f,	0.0f};

bank_float		WIND_GUST_HI_CHANCE[WIND_TYPE_NUM]				= {50.0f,	0.0f};//2.0f};
bank_float		WIND_GUST_HI_SPEED_MULT_MIN[WIND_TYPE_NUM]		= {0.8f,	0.0f};//0.5f};
bank_float		WIND_GUST_HI_SPEED_MULT_MAX[WIND_TYPE_NUM]		= {1.2f,	0.0f};//1.0f};
bank_float		WIND_GUST_HI_TIME_MIN[WIND_TYPE_NUM]			= {1.0f,	0.0f};//2.0f};
bank_float		WIND_GUST_HI_TIME_MAX[WIND_TYPE_NUM]			= {2.0f,	0.0f};//3.0f};	

bank_float		WIND_GUST_HI_FADE_THRESH[WIND_TYPE_NUM]			= {0.5f,	0.0f};

// global variation												   air		water
bank_float		WIND_GLOBAL_VAR_MULT[WIND_TYPE_NUM]				= {0.25f,	0.25f};
bank_float		WIND_GLOBAL_VAR_SINE_POS_MULT[WIND_TYPE_NUM]	= {0.5f,	0.5f};
bank_float		WIND_GLOBAL_VAR_SINE_TIME_MULT[WIND_TYPE_NUM]	= {12.0f,	12.0f};
bank_float		WIND_GLOBAL_VAR_COSINE_POS_MULT[WIND_TYPE_NUM]	= {0.5f,	0.5f};
bank_float		WIND_GLOBAL_VAR_COSINE_TIME_MULT[WIND_TYPE_NUM]	= {12.0f,	12.0f};

// positional variation											   air		water
bank_float		WIND_POSITIONAL_VAR_MULT[WIND_TYPE_NUM]			= {0.1f,	0.1f};
bank_bool		WIND_POSITIONAL_VAR_IS_FLUID[WIND_TYPE_NUM]		= {true,	true};				// false = per grid element, true = fluid across space

// cyclone disturbances (wind)
// bank_bool		WIND_CYCLONES_AUTOMATED					= false;
// bank_float		WIND_CYCLONES_AUTO_RANGE_MIN			= 5.0f;
// bank_float		WIND_CYCLONES_AUTO_RANGE_MAX			= 15.0f;
// bank_float		WIND_CYCLONES_AUTO_STRENGTH_MIN			= 0.5f;
// bank_float		WIND_CYCLONES_AUTO_STRENGTH_MAX			= 3.0f;
// bank_float		WIND_CYCLONES_AUTO_STRENGTH_DELTA		= 0.3f;
// bank_float		WIND_CYCLONES_AUTO_FORCE_MULT			= 0.5f;
// bank_float		WIND_CYCLONES_AUTO_CREATE_CHANCE		= 0.3f;
// bank_float		WIND_CYCLONES_AUTO_SHRINK_CHANCE		= 0.1f;

// cyclone disturbances (water)
// bank_bool		WIND_WATER_CYCLONES_AUTOMATED			= false;
// bank_float		WIND_WATER_CYCLONES_AUTO_RANGE_MIN		= 2.0f;
// bank_float		WIND_WATER_CYCLONES_AUTO_RANGE_MAX		= 5.0f;
// bank_float		WIND_WATER_CYCLONES_AUTO_STRENGTH_MIN	= 0.5f;
// bank_float		WIND_WATER_CYCLONES_AUTO_STRENGTH_MAX	= 3.0f;
// bank_float		WIND_WATER_CYCLONES_AUTO_STRENGTH_DELTA	= 0.6f;
// bank_float		WIND_WATER_CYCLONES_AUTO_FORCE_MULT		= 0.5f;
// bank_float		WIND_WATER_CYCLONES_AUTO_CREATE_CHANCE	= 0.9f;
// bank_float		WIND_WATER_CYCLONES_AUTO_SHRINK_CHANCE	= 0.4f;

// sphere disturbances
bank_float		WIND_PED_WATER_DIST_MULT				= 0.1f;
bank_float		WIND_PED_WIND_DIST_MULT					= 0.1f;
bank_float		WIND_VEH_WIND_DIST_MULT					= 0.6f;
bank_float		WIND_HELI_WIND_DIST_FORCE				= 4.0f;
bank_float		WIND_HELI_WIND_DIST_Z_FADE_THRESH_MIN	= 5.0f;
bank_float		WIND_HELI_WIND_DIST_Z_FADE_THRESH_MAX	= 10.0f;
bank_float		WIND_EXPLOSION_WIND_DIST_FORCE_MULT		= 1.5f;
bank_float		WIND_DIREXPLOSION_WIND_DIST_FORCE_MULT	= 1.5f;
bank_float		WIND_DISTURBANCE_FALLOFF				= 1000000.0f;


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern dev_float	WEATHER_WIND_SPEED_MAX;
extern dev_float	WEATHER_WATER_SPEED_MAX;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CWind::Init									(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
	    // initialise the rage wind system
	    phWind::InitClass();

		// initialise the wind direction
		m_currWindDir = 0.0f;
		m_targetWindDir = 0.0f;
		m_overrideWindDir = -1.0f;
		//@@: location CWIND_INIT
		m_pDownwashGroup = &(WIND.m_downwashGroup);
		m_pSphereGroup = &(WIND.m_sphereGroup);
		m_pExplosionGroup = &(WIND.m_explosionGroup);
		m_pDirExplosionGroup = &(WIND.m_dirExplosionGroup);
		m_pHemisphereGroup = &(WIND.m_hemisphereGroup);
		m_pThermalGroup = &(WIND.m_thermalGroup);

	    // create a cyclone disturbance group, add it to the wind system and let it manage itself (by default)
// 	    phCycloneAutoData cycAutoData;
// 	    cycAutoData.type = WIND_DIST_AIR;
// 	    cycAutoData.rangeMin = WIND_CYCLONES_AUTO_RANGE_MIN;
// 	    cycAutoData.rangeMax = WIND_CYCLONES_AUTO_RANGE_MAX;
// 	    cycAutoData.strengthMin = WIND_CYCLONES_AUTO_STRENGTH_MIN;
// 	    cycAutoData.strengthMax = WIND_CYCLONES_AUTO_STRENGTH_MAX;
// 	    cycAutoData.strengthDelta = WIND_CYCLONES_AUTO_STRENGTH_DELTA;
// 	    cycAutoData.createChance = WIND_CYCLONES_AUTO_CREATE_CHANCE;
// 	    cycAutoData.shrinkChance = WIND_CYCLONES_AUTO_SHRINK_CHANCE;
// 	    cycAutoData.forceMult = WIND_CYCLONES_AUTO_FORCE_MULT;
// 
// 	    m_pCycloneWindGroup	= rage_new phWindCycloneGroup();
// 		m_pCycloneWindGroup->Init(MAX_WIND_CYCLONES);
// 	    m_pCycloneWindGroup->SetAutoData(cycAutoData);
// 	    m_pCycloneWindGroup->SetAutomated(WIND_CYCLONES_AUTOMATED);
// 	    WIND.AddDisturbanceGroup(m_pCycloneWindGroup);

	    // create an underwater cyclone disturbance group, add it to the wind system and let it manage itself (by default)
// 	    cycAutoData.type = WIND_DIST_WATER;
// 	    cycAutoData.rangeMin = WIND_WATER_CYCLONES_AUTO_RANGE_MIN;
// 	    cycAutoData.rangeMax = WIND_WATER_CYCLONES_AUTO_RANGE_MAX;
// 	    cycAutoData.strengthMin = WIND_WATER_CYCLONES_AUTO_STRENGTH_MIN;
// 	    cycAutoData.strengthMax = WIND_WATER_CYCLONES_AUTO_STRENGTH_MAX;
// 	    cycAutoData.strengthDelta = WIND_WATER_CYCLONES_AUTO_STRENGTH_DELTA;
// 	    cycAutoData.createChance = WIND_WATER_CYCLONES_AUTO_CREATE_CHANCE;
// 	    cycAutoData.shrinkChance = WIND_WATER_CYCLONES_AUTO_SHRINK_CHANCE;
// 	    cycAutoData.forceMult = WIND_WATER_CYCLONES_AUTO_FORCE_MULT;
// 
// 	    m_pCycloneWaterGroup = rage_new phWindCycloneGroup();
// 		m_pCycloneWaterGroup->Init(MAX_WATER_CYCLONES);
// 	    m_pCycloneWaterGroup->SetAutoData(cycAutoData);
// 	    m_pCycloneWaterGroup->SetAutomated(WIND_WATER_CYCLONES_AUTOMATED);
// 	    WIND.AddDisturbanceGroup(m_pCycloneWaterGroup);

	    // create a box disturbance group, add it to the wind system and tell it not to manage itself (we will be adding these)
	    //phBoxAutoData boxAutoData;
	    //boxAutoData.type = WIND_DIST_AIR;
	    //boxAutoData.createChance = 0.3f;
	    //boxAutoData.sizeMin = 3.0f;
	    //boxAutoData.sizeMax = 7.0f;
	    //boxAutoData.strengthMin = 5.0f;
	    //boxAutoData.strengthMax = 10.0f;

// 	    m_pBoxGroup	= rage_new phWindBoxGroup();
// 		m_pBoxGroup->Init(MAX_WIND_BOXES);
// 	    //m_pBoxGroup->SetAutoData(boxAutoData);
// 	    m_pBoxGroup->SetAutomated(false);
// 	    WIND.AddDisturbanceGroup(m_pBoxGroup);

	    // create a thermal disturbance group, add it to the wind system and tell it not to manage itself (we will be adding these)
	    //phThermalAutoData thermalAutoData;
	    //thermalAutoData.type = WIND_DIST_AIR;
	    //thermalAutoData.radiusMin = 0.5f;
	    //thermalAutoData.radiusMax = 1.0f;
	    //thermalAutoData.heightMin = 10.0f;
	    //thermalAutoData.heightMax = 15.0f;
	    //thermalAutoData.strengthMin = 0.5f;
	    //thermalAutoData.strengthMax = 3.0f;
	    //thermalAutoData.strengthDelta = 0.3f;
	    //thermalAutoData.createChance = 0.3f;
	    //thermalAutoData.shrinkChance = 0.1f;

		// create a downwash disturbance group, add it to the wind system and tell it not to manage itself (we will be adding these)
		m_pDownwashGroup->Init(MAX_WIND_DOWNWASHES);
		m_pDownwashGroup->SetAutomated(false);

		// create a sphere disturbance group, add it to the wind system and tell it not to manage itself (we will be adding these)
		m_pSphereGroup->Init(MAX_WIND_SPHERES);
		m_pSphereGroup->SetAutomated(false);

		// create an explosion disturbance group, add it to the wind system and tell it not to manage itself (we will be adding these)
		m_pExplosionGroup->Init(MAX_WIND_EXPLOSIONS);
		m_pExplosionGroup->SetAutomated(false);

		// create an directed explosion disturbance group, add it to the wind system and tell it not to manage itself (we will be adding these)
		m_pDirExplosionGroup->Init(MAX_WIND_EXPLOSIONS);
		m_pDirExplosionGroup->SetAutomated(false);

		// create a hemisphere disturbance group, add it to the wind system and tell it not to manage itself (we will be adding these)
		m_pHemisphereGroup->Init(MAX_WIND_HEMISPHERES);
		m_pHemisphereGroup->SetAutomated(false);

		m_pThermalGroup->Init(MAX_WIND_THERMALS);
		m_pThermalGroup->SetAutomated(false);
		m_pThermalGroup->SetCullInfo(150.0f, 4);

	    // init the disturbance falloff value
	    WIND.SetDisturbanceFallOff(WIND_DISTURBANCE_FALLOFF);

	    // clear the disturbance and wind fields
	    WIND.InitSession();
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CWind::Shutdown								(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
	    // shutdown the rage wind system
	    phWind::ShutdownClass();

	    // delete the disturbance groups
		//delete m_pThermalGroup;
		//delete m_pSphereGroup;
		//delete m_pHemisphereGroup;
		//delete m_pDirExplosionGroup;
		//delete m_pExplosionGroup;
		//delete m_pDownwashGroup;
//	    delete m_pCycloneWaterGroup;
//	    delete m_pCycloneWindGroup;
//	    delete m_pBoxGroup;
    }
}

///////////////////////////////////////////////////////////////////////////////
//  UpdateWindDir
///////////////////////////////////////////////////////////////////////////////

void			CWind::UpdateWindDir						()
{
	// override the wind direction if requested
	if (m_overrideWindDir>=0.0f)
	{
		m_currWindDir = m_overrideWindDir;
		m_targetWindDir = m_overrideWindDir;
	}
	else
	{
		// calc a new target direction every so often
		if (Abs(m_currWindDir-m_targetWindDir)<0.001f)
		{
			if (g_DrawRand.GetRanged(0.0f, 100.0f)<WIND_UPDATE_DIR_CHANCE)
			{
				m_targetWindDir = g_DrawRand.GetRanged(0.0f, TWO_PI);
			}
		}

		float windDirDiff = m_targetWindDir - m_currWindDir;
		float windStep = WIND_UPDATE_DIR_STEP*fwTimer::GetTimeStep();
		float windDirChange = Clamp(windDirDiff, -windStep, windStep);
		m_currWindDir += windDirChange;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void			CWind::Update								(float windSpeed, float waterSpeed)
{
	PF_START_TIMEBAR_DETAIL("Wind Game Update");

	Vec3V vFocalPoint;

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		// calc the focal point of the wind field (around the player at the moment)
		Vec3V vPlayerPos = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());
		vFocalPoint = vPlayerPos;

		// move the point away from the camera to maximise coverage
		Vec3V vShiftVec = vFocalPoint - RCC_VEC3V(camInterface::GetPos());
		vShiftVec.SetZ(ScalarV(V_ZERO));
		vShiftVec = NormalizeSafe(vShiftVec, Vec3V(V_ZERO));
		vShiftVec *= ScalarV(V_TEN);
		vFocalPoint += vShiftVec;

		// move up a little as we want more resolution above the player than below
		vFocalPoint.SetZ(vFocalPoint.GetZ() + ScalarV(V_ONE));
	}
	else
#endif	// GTA_REPLAY
	{
		// update the disturbances
		UpdateWindDir();
		UpdateDisturbances();

		// calc the focal point of the wind field (around the player at the moment)
		Vec3V vPlayerPos = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());
		vFocalPoint = vPlayerPos;

		// move the point away from the camera to maximise coverage
		Vec3V vShiftVec = vFocalPoint - RCC_VEC3V(camInterface::GetPos());
		vShiftVec.SetZ(ScalarV(V_ZERO));
		vShiftVec = NormalizeSafe(vShiftVec, Vec3V(V_ZERO));
		vShiftVec *= ScalarV(V_TEN);
		vFocalPoint += vShiftVec;

		// move up a little as we want more resolution above the player than below
		vFocalPoint.SetZ(vFocalPoint.GetZ() + ScalarV(V_ONE));

		// temporarily disable the wind until particle movement is working
		//	windSpeed = 0.0f;
		//	waterSpeed = 0.0f;

		// dampen the wind speed if we are in an interior
		windSpeed *= CPortal::GetInteriorFxLevel();

		// update the rage wind system
		WIND.SetMaxSpeed(WIND_TYPE_AIR, WEATHER_WIND_SPEED_MAX);
		WIND.SetSpeed(WIND_TYPE_AIR, windSpeed);
		WIND.SetAngle(WIND_TYPE_AIR, m_currWindDir);

		// get info about the water at the camera position
		Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetPos());
		float camWaterDepth;
		WaterTestResultType waterType = CVfxHelper::GetWaterDepth(vCamPos, camWaterDepth);

		// calculate the water angle and speed
		float waterAngle = 0.0f;
		if (waterType==WATERTEST_TYPE_RIVER)
		{
			Vector2 riverFlow(0.0f, 0.0f);
			River::GetRiverFlowFromPosition(vCamPos, riverFlow);
			float riverSpeed = riverFlow.Mag()*WIND_RIVER_VELOCITY_MULTIPLIER;
			waterSpeed = MIN(riverSpeed, WEATHER_WATER_SPEED_MAX);
			if (riverSpeed>0.0f)
			{
				riverFlow.Normalize();
				waterAngle = -rage::Atan2f(-riverFlow.x, riverFlow.y);
			}
		}
		else if (waterType==WATERTEST_TYPE_OCEAN)
		{
			waterSpeed = 0.2f;
		}
		else
		{
			waterSpeed = 0.0f;
		}

		WIND.SetMaxSpeed(WIND_TYPE_WATER, WEATHER_WATER_SPEED_MAX);
		WIND.SetSpeed(WIND_TYPE_WATER, waterSpeed);
		WIND.SetAngle(WIND_TYPE_WATER, waterAngle);

		for (int i=0; i<WIND_TYPE_NUM; i++)
		{
			WindType_e windType = (WindType_e)i;
			float windSpeedRatio;
			if (i==WIND_TYPE_AIR)
			{
				windSpeedRatio = windSpeed/WEATHER_WIND_SPEED_MAX;
			}
			else
			{
				windSpeedRatio = waterSpeed/WEATHER_WATER_SPEED_MAX;
			}

			float gustT = 0.0f;
			if (windSpeedRatio>WIND_GUST_HI_FADE_THRESH[windType])
			{
				gustT = (windSpeedRatio-WIND_GUST_HI_FADE_THRESH[windType]) / (1.0f-WIND_GUST_HI_FADE_THRESH[windType]);
			}

			float gustChance = Lerp(gustT, WIND_GUST_LO_CHANCE[windType], WIND_GUST_HI_CHANCE[windType]);
			float gustSpeedMultMin = Lerp(gustT, WIND_GUST_LO_SPEED_MULT_MIN[windType], WIND_GUST_HI_SPEED_MULT_MIN[windType]);
			float gustSpeedMultMax = Lerp(gustT, WIND_GUST_LO_SPEED_MULT_MAX[windType], WIND_GUST_HI_SPEED_MULT_MAX[windType]);
			float gustTimeMin = Lerp(gustT, WIND_GUST_LO_TIME_MIN[windType], WIND_GUST_HI_TIME_MIN[windType]);
			float gustTimeMax = Lerp(gustT, WIND_GUST_LO_TIME_MAX[windType], WIND_GUST_HI_TIME_MAX[windType]);

			WIND.SetGlobalVariationMult(windType, WIND_GLOBAL_VAR_MULT[windType]);
			WIND.SetGlobalVariationWaveData(windType, WIND_GLOBAL_VAR_SINE_POS_MULT[windType], WIND_GLOBAL_VAR_SINE_TIME_MULT[windType], WIND_GLOBAL_VAR_COSINE_POS_MULT[windType], WIND_GLOBAL_VAR_COSINE_TIME_MULT[windType]);
			WIND.SetPositionalVariationMult(windType, WIND_POSITIONAL_VAR_MULT[windType]);
			WIND.SetPositionalVariationIsFluid(windType, WIND_POSITIONAL_VAR_IS_FLUID[windType]);
			WIND.SetGustChance(windType, gustChance);
			WIND.SetGustSpeedMults(windType, gustSpeedMultMin, gustSpeedMultMax);
			WIND.SetGustTimes(windType, gustTimeMin, gustTimeMax);
		}
	}

	WIND.SetFocalPoint(vFocalPoint);
	WIND.SetWaterLevel(CVfxHelper::GetGameCamWaterZ());

	PF_START_TIMEBAR_DETAIL("Wind Rage Update");
	WIND.Update(fwTimer::GetTimeStep(), fwTimer::GetTimeInMilliseconds(), fwTimer::GetSystemFrameCount() REPLAY_ONLY(, CReplayMgr::IsEditModeActive()));

#if __BANK
	UpdateSettings();
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateDisturbances
///////////////////////////////////////////////////////////////////////////////

void			CWind::UpdateDisturbances					()
{
	float waterZ = phWindField::GetWaterLevelWld();

	// add player disturbance
	Vec3V vPedPos = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		CBaseModelInfo* pModelInfo = pPlayerPed->GetBaseModelInfo();

		float radiusMult = 1.0f;
		float velMult = WIND_PED_WIND_DIST_MULT;
		if (CGameWorld::FindLocalPlayer() && vPedPos.GetZf() <= waterZ)
		{
			radiusMult = 2.0f;
			velMult = WIND_PED_WATER_DIST_MULT;
		}
		Vec3V vPedVel = RCC_VEC3V(pPlayerPed->GetVelocity()) * ScalarVFromF32(velMult);

		phSphereSettings sphereSettings;
		sphereSettings.forceMult = 0.2f;
		sphereSettings.radius = ((pModelInfo->GetBoundingBoxMax().z - pModelInfo->GetBoundingBoxMin().z)/1.5f) * radiusMult;
		sphereSettings.vVelocity = vPedVel;

		phWindSphere distSphere(WIND_DIST_GLOBAL, vPedPos, sphereSettings);
		m_pSphereGroup->AddDisturbance(&distSphere);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RenderDebug
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CWind::RenderDebug								()
{
#if __PFDRAW 
	WIND.DebugDraw(); 
#endif
}
#endif



///////////////////////////////////////////////////////////////////////////////
//  UpdateSettings
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CWind::UpdateSettings							()
{
	// wind cyclones
// 	phCycloneAutoData cycAutoData;
// 	cycAutoData.type = WIND_DIST_AIR;
// 	cycAutoData.rangeMin = WIND_CYCLONES_AUTO_RANGE_MIN;
// 	cycAutoData.rangeMax = WIND_CYCLONES_AUTO_RANGE_MAX;
// 	cycAutoData.strengthMin = WIND_CYCLONES_AUTO_STRENGTH_MIN;
// 	cycAutoData.strengthMax = WIND_CYCLONES_AUTO_STRENGTH_MAX;
// 	cycAutoData.strengthDelta = WIND_CYCLONES_AUTO_STRENGTH_DELTA;
// 	cycAutoData.forceMult = WIND_CYCLONES_AUTO_FORCE_MULT;
// 	cycAutoData.createChance = WIND_CYCLONES_AUTO_CREATE_CHANCE;
// 	cycAutoData.shrinkChance = WIND_CYCLONES_AUTO_SHRINK_CHANCE;
// 
// 	m_pCycloneWindGroup->SetAutoData(cycAutoData);
// 	m_pCycloneWindGroup->SetAutomated(WIND_CYCLONES_AUTOMATED);

	// underwater cyclones
// 	cycAutoData.type = WIND_DIST_WATER;
// 	cycAutoData.rangeMin = WIND_WATER_CYCLONES_AUTO_RANGE_MIN;
// 	cycAutoData.rangeMax = WIND_WATER_CYCLONES_AUTO_RANGE_MAX;
// 	cycAutoData.strengthMin = WIND_WATER_CYCLONES_AUTO_STRENGTH_MIN;
// 	cycAutoData.strengthMax = WIND_WATER_CYCLONES_AUTO_STRENGTH_MAX;
// 	cycAutoData.strengthDelta = WIND_WATER_CYCLONES_AUTO_STRENGTH_DELTA;
// 	cycAutoData.forceMult = WIND_WATER_CYCLONES_AUTO_FORCE_MULT;
// 	cycAutoData.createChance = WIND_WATER_CYCLONES_AUTO_CREATE_CHANCE;
// 	cycAutoData.shrinkChance = WIND_WATER_CYCLONES_AUTO_SHRINK_CHANCE;
// 
// 	m_pCycloneWaterGroup->SetAutoData(cycAutoData);
// 	m_pCycloneWaterGroup->SetAutomated(WIND_WATER_CYCLONES_AUTOMATED);

	// disturbance fall off
	WIND.SetDisturbanceFallOff(WIND_DISTURBANCE_FALLOFF);
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  AddThermal
///////////////////////////////////////////////////////////////////////////////

s32		 	CWind::AddThermal								(Vec3V_In vPos, const phThermalSettings& thermalSettings)
{
	phWindThermal distThermal(WIND_DIST_AIR, vPos, thermalSettings);
	return m_pThermalGroup->AddDisturbance(&distThermal);
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveThermal
///////////////////////////////////////////////////////////////////////////////

void			CWind::RemoveThermal							(s32 thermalIndex)
{
	m_pThermalGroup->RemoveDisturbance(thermalIndex);
}


///////////////////////////////////////////////////////////////////////////////
//  AddExplosion
///////////////////////////////////////////////////////////////////////////////

void		 	CWind::AddExplosion									(Vec3V_In vPos, const phExplosionSettings& explosionSettings)
{
	//@@: location CWIND_ADDEXPLOSION
	phWindExplosion distExplosion(WIND_DIST_AIR, vPos, explosionSettings);
	m_pExplosionGroup->AddDisturbance(&distExplosion);
}


///////////////////////////////////////////////////////////////////////////////
//  AddDirExplosion
///////////////////////////////////////////////////////////////////////////////

void		 	CWind::AddDirExplosion								(Vec3V_In vPos, const phDirExplosionSettings& dirExplosionSettings)
{
	phWindDirExplosion distDirExplosion(WIND_DIST_AIR, vPos, dirExplosionSettings);
	m_pDirExplosionGroup->AddDisturbance(&distDirExplosion);
}


///////////////////////////////////////////////////////////////////////////////
//  AddDownwash
///////////////////////////////////////////////////////////////////////////////

void		 	CWind::AddDownwash									(Vec3V_In vPos, const phDownwashSettings& downwashSettings)
{
	phWindDownwash distDownwash(WIND_DIST_AIR, vPos, downwashSettings);
	m_pDownwashGroup->AddDisturbance(&distDownwash);
}


///////////////////////////////////////////////////////////////////////////////
//  AddHemisphere
///////////////////////////////////////////////////////////////////////////////

void		 	CWind::AddHemisphere								(Vec3V_In vPos, const phHemisphereSettings& hemisphereSettings)
{
	phWindHemisphere distHemisphere(WIND_DIST_AIR, vPos, hemisphereSettings);
	m_pHemisphereGroup->AddDisturbance(&distHemisphere);
}

///////////////////////////////////////////////////////////////////////////////
//  AddSphere
///////////////////////////////////////////////////////////////////////////////

void		 	CWind::AddSphere								(Vec3V_In vPos, const phSphereSettings& sphereSettings)
{
	phWindSphere distSphere(WIND_DIST_GLOBAL, vPos, sphereSettings);
	m_pSphereGroup->AddDisturbance(&distSphere);
}

///////////////////////////////////////////////////////////////////////////////
//  InitDebugWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CWind::InitLevelWidgets							()
{
	bkBank* pBank = BANKMGR.FindBank("Weather");
	Assert(pBank);

	WIND.AddWidgets(*pBank);

	pBank->PushGroup("GTA Wind", false);
	{
		pBank->AddSlider("Wind Update Dir Chance", &WIND_UPDATE_DIR_CHANCE, 0.0f, 100.0f, 0.01f);
		pBank->AddSlider("Wind Update Dir Step", &WIND_UPDATE_DIR_STEP, 0.0f, 1.0f, 0.01f);
		pBank->AddSlider("Wind River Velocity Mult", &WIND_RIVER_VELOCITY_MULTIPLIER, 0.0f, 500.0f, 0.5f);

		pBank->AddTitle("Gusts (Air)");
		pBank->AddSlider("Wind Gust Lo Chance", &WIND_GUST_LO_CHANCE[WIND_TYPE_AIR], 0.0f, 100.0f, 0.01f);
		pBank->AddSlider("Wind Gust Lo Speed Mult Min", &WIND_GUST_LO_SPEED_MULT_MIN[WIND_TYPE_AIR], 0.0f, 10.0f, 0.01f);
		pBank->AddSlider("Wind Gust Lo Speed Mult Max", &WIND_GUST_LO_SPEED_MULT_MAX[WIND_TYPE_AIR], 0.0f, 10.0f, 0.01f);
		pBank->AddSlider("Wind Gust Lo Time Min", &WIND_GUST_LO_TIME_MIN[WIND_TYPE_AIR], 0.0f, 20.0f, 0.01f);
		pBank->AddSlider("Wind Gust Lo Time Max", &WIND_GUST_LO_TIME_MAX[WIND_TYPE_AIR], 0.0f, 20.0f, 0.01f);

		pBank->AddSlider("Wind Gust Hi Chance", &WIND_GUST_HI_CHANCE[WIND_TYPE_AIR], 0.0f, 1000.0f, 0.01f);
		pBank->AddSlider("Wind Gust Hi Speed Mult Min", &WIND_GUST_HI_SPEED_MULT_MIN[WIND_TYPE_AIR], 0.0f, 10.0f, 0.01f);
		pBank->AddSlider("Wind Gust Hi Speed Mult Max", &WIND_GUST_HI_SPEED_MULT_MAX[WIND_TYPE_AIR], 0.0f, 10.0f, 0.01f);
		pBank->AddSlider("Wind Gust Hi Time Min", &WIND_GUST_HI_TIME_MIN[WIND_TYPE_AIR], 0.0f, 20.0f, 0.01f);
		pBank->AddSlider("Wind Gust Hi Time Max", &WIND_GUST_HI_TIME_MAX[WIND_TYPE_AIR], 0.0f, 20.0f, 0.01f);

		pBank->AddSlider("Wind Gust Hi Fade Thresh", &WIND_GUST_HI_FADE_THRESH[WIND_TYPE_AIR], 0.0f, 1.0f, 0.01f);

		pBank->AddTitle("Global Variation (Air)");
		pBank->AddSlider("Wind Direction Global Variance Mult Min", &WIND_GLOBAL_VAR_MULT[WIND_TYPE_AIR], -5.0f, 5.0f, 0.01f);
		pBank->AddSlider("Global Variance Sine Pos Mult", &WIND_GLOBAL_VAR_SINE_POS_MULT[WIND_TYPE_AIR], -20.0f, 10.0f, 0.05f);
		pBank->AddSlider("Global Variance Sine Time Mult", &WIND_GLOBAL_VAR_SINE_TIME_MULT[WIND_TYPE_AIR], 0.0f, 100.0f, 0.1f);
		pBank->AddSlider("Global Variance Cosine Pos Mult", &WIND_GLOBAL_VAR_COSINE_POS_MULT[WIND_TYPE_AIR], -20.0f, 10.0f, 0.05f);
		pBank->AddSlider("Global Variance Cosine Time Mult", &WIND_GLOBAL_VAR_COSINE_TIME_MULT[WIND_TYPE_AIR], 0.0f, 100.0f, 0.1f);

		pBank->AddTitle("Positional Variance (Air)");
		pBank->AddSlider("Wind Direction Positional Variance Mult Min", &WIND_POSITIONAL_VAR_MULT[WIND_TYPE_AIR], -5.0f, 5.0f, 0.01f);
		pBank->AddToggle("Wind Direction Positional Variance Is Fluid", &WIND_POSITIONAL_VAR_IS_FLUID[WIND_TYPE_AIR]);

		pBank->AddTitle("Gusts (Water)");
		pBank->AddSlider("Wind Gust Lo Chance", &WIND_GUST_LO_CHANCE[WIND_TYPE_WATER], 0.0f, 100.0f, 0.01f);
		pBank->AddSlider("Wind Gust Lo Speed Mult Min", &WIND_GUST_LO_SPEED_MULT_MIN[WIND_TYPE_WATER], 0.0f, 10.0f, 0.01f);
		pBank->AddSlider("Wind Gust Lo Speed Mult Max", &WIND_GUST_LO_SPEED_MULT_MAX[WIND_TYPE_WATER], 0.0f, 10.0f, 0.01f);
		pBank->AddSlider("Wind Gust Lo Time Min", &WIND_GUST_LO_TIME_MIN[WIND_TYPE_WATER], 0.0f, 20.0f, 0.01f);
		pBank->AddSlider("Wind Gust Lo Time Max", &WIND_GUST_LO_TIME_MAX[WIND_TYPE_WATER], 0.0f, 20.0f, 0.01f);

		pBank->AddSlider("Wind Gust Hi Chance", &WIND_GUST_HI_CHANCE[WIND_TYPE_WATER], 0.0f, 1000.0f, 0.01f);
		pBank->AddSlider("Wind Gust Hi Speed Mult Min", &WIND_GUST_HI_SPEED_MULT_MIN[WIND_TYPE_WATER], 0.0f, 10.0f, 0.01f);
		pBank->AddSlider("Wind Gust Hi Speed Mult Max", &WIND_GUST_HI_SPEED_MULT_MAX[WIND_TYPE_WATER], 0.0f, 10.0f, 0.01f);
		pBank->AddSlider("Wind Gust Hi Time Min", &WIND_GUST_HI_TIME_MIN[WIND_TYPE_WATER], 0.0f, 20.0f, 0.01f);
		pBank->AddSlider("Wind Gust Hi Time Max", &WIND_GUST_HI_TIME_MAX[WIND_TYPE_WATER], 0.0f, 20.0f, 0.01f);

		pBank->AddSlider("Wind Gust Hi Fade Thresh", &WIND_GUST_HI_FADE_THRESH[WIND_TYPE_WATER], 0.0f, 1.0f, 0.01f);

		pBank->AddTitle("Global Variation (Water)");
		pBank->AddSlider("Wind Direction Global Variance Mult Min", &WIND_GLOBAL_VAR_MULT[WIND_TYPE_WATER], -5.0f, 5.0f, 0.01f);
		pBank->AddSlider("Global Variance Sine Pos Mult", &WIND_GLOBAL_VAR_SINE_POS_MULT[WIND_TYPE_WATER], -20.0f, 10.0f, 0.05f);
		pBank->AddSlider("Global Variance Sine Time Mult", &WIND_GLOBAL_VAR_SINE_TIME_MULT[WIND_TYPE_WATER], 0.0f, 100.0f, 0.1f);
		pBank->AddSlider("Global Variance Cosine Pos Mult", &WIND_GLOBAL_VAR_COSINE_POS_MULT[WIND_TYPE_WATER], -20.0f, 10.0f, 0.05f);
		pBank->AddSlider("Global Variance Cosine Time Mult", &WIND_GLOBAL_VAR_COSINE_TIME_MULT[WIND_TYPE_WATER], 0.0f, 100.0f, 0.1f);

		pBank->AddTitle("Positional Variance (Water)");
		pBank->AddSlider("Wind Direction Positional Variance Mult Min", &WIND_POSITIONAL_VAR_MULT[WIND_TYPE_WATER], -5.0f, 5.0f, 0.01f);
		pBank->AddToggle("Wind Direction Positional Variance Is Fluid", &WIND_POSITIONAL_VAR_IS_FLUID[WIND_TYPE_WATER]);

// 		pBank->AddTitle("Cyclone Disturbances (Wind)");
// 		pBank->AddToggle("Cyclones Automated", &WIND_CYCLONES_AUTOMATED);
// 		pBank->AddSlider("Cyclones Auto Range Min", &WIND_CYCLONES_AUTO_RANGE_MIN, 0.0f, 100.0f, 0.01f);
// 		pBank->AddSlider("Cyclones Auto Range Max", &WIND_CYCLONES_AUTO_RANGE_MAX, 0.0f, 100.0f, 0.01f);
// 		pBank->AddSlider("Cyclones Auto Strength Min", &WIND_CYCLONES_AUTO_STRENGTH_MIN, 0.0f, 200.0f, 0.01f);
// 		pBank->AddSlider("Cyclones Auto Strength Max", &WIND_CYCLONES_AUTO_STRENGTH_MAX, 0.0f, 200.0f, 0.01f);
// 		pBank->AddSlider("Cyclones Auto Strength Delta", &WIND_CYCLONES_AUTO_STRENGTH_DELTA, 0.0f, 100.0f, 0.01f);
// 		pBank->AddSlider("Cyclones Auto Force Mult", &WIND_CYCLONES_AUTO_FORCE_MULT, 0.0f, 100.0f, 0.01f);
// 		pBank->AddSlider("Cyclones Auto Create Chance", &WIND_CYCLONES_AUTO_CREATE_CHANCE, 0.0f, 100.0f, 0.01f);
// 		pBank->AddSlider("Cyclones Auto Shrink Chance", &WIND_CYCLONES_AUTO_SHRINK_CHANCE, 0.0f, 100.0f, 0.01f);

// 		pBank->AddTitle("Cyclone Disturbances (Water)");
// 		pBank->AddToggle("Water Cyclones Automated", &WIND_WATER_CYCLONES_AUTOMATED);
// 		pBank->AddSlider("Water Cyclones Auto Range Min", &WIND_WATER_CYCLONES_AUTO_RANGE_MIN, 0.0f, 100.0f, 0.01f);
// 		pBank->AddSlider("Water Cyclones Auto Range Max", &WIND_WATER_CYCLONES_AUTO_RANGE_MAX, 0.0f, 100.0f, 0.01f);
// 		pBank->AddSlider("Water Cyclones Auto Strength Min", &WIND_WATER_CYCLONES_AUTO_STRENGTH_MIN, 0.0f, 200.0f, 0.01f);
// 		pBank->AddSlider("Water Cyclones Auto Strength Max", &WIND_WATER_CYCLONES_AUTO_STRENGTH_MAX, 0.0f, 200.0f, 0.01f);
// 		pBank->AddSlider("Water Cyclones Auto Strength Delta", &WIND_WATER_CYCLONES_AUTO_STRENGTH_DELTA, 0.0f, 100.0f, 0.01f);
// 		pBank->AddSlider("Water Cyclones Auto Force Mult", &WIND_WATER_CYCLONES_AUTO_FORCE_MULT, 0.0f, 100.0f, 0.01f);
// 		pBank->AddSlider("Water Cyclones Auto Create Chance", &WIND_WATER_CYCLONES_AUTO_CREATE_CHANCE, 0.0f, 100.0f, 0.01f);
// 		pBank->AddSlider("Water Cyclones Auto Shrink Chance", &WIND_WATER_CYCLONES_AUTO_SHRINK_CHANCE, 0.0f, 100.0f, 0.01f);

		pBank->AddTitle("Sphere Disturbances");
		pBank->AddSlider("Ped Water Disturbance Mult", &WIND_PED_WATER_DIST_MULT, 0.0f, 100.0f, 0.01f);
		pBank->AddSlider("Ped Wind Disturbance Mult", &WIND_PED_WIND_DIST_MULT, 0.0f, 100.0f, 0.01f);
		pBank->AddSlider("Vehicle Wind Disturbance Mult", &WIND_VEH_WIND_DIST_MULT, 0.0f, 100.0f, 0.01f);
		pBank->AddSlider("Heli Wind Disturbance Force", &WIND_HELI_WIND_DIST_FORCE, 0.0f, 100.0f, 0.05f);
		pBank->AddSlider("Heli Wind Disturbance Z Fade Threshold Min", &WIND_HELI_WIND_DIST_Z_FADE_THRESH_MIN, 0.0f, 20.0f, 0.1f);
		pBank->AddSlider("Heli Wind Disturbance Z Fade Threshold Max", &WIND_HELI_WIND_DIST_Z_FADE_THRESH_MAX, 0.0f, 20.0f, 0.1f);
		pBank->AddSlider("Explosion Wind Disturbance Force Mult", &WIND_EXPLOSION_WIND_DIST_FORCE_MULT, 0.0f, 100.0f, 0.01f);
		pBank->AddSlider("Directed Explosion Wind Disturbance Force Mult", &WIND_DIREXPLOSION_WIND_DIST_FORCE_MULT, 0.0f, 100.0f, 0.01f);
		pBank->AddSlider("Disturbance Falloff", &WIND_DISTURBANCE_FALLOFF, 0.0f, 1000000.0f, 1.0f);
	}
	pBank->PopGroup();
}

void			CWind::ShutdownLevelWidgets							()
{
	// will be cleaned up when the weather bank is destroyed
}

#endif
