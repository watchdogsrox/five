///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	TimeCycleConfig.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	19 Aug 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////	

#include "TimeCycleConfig.h"

// c
#include <algorithm>

// rage
#include "string/stringhash.h"

// game
#include "Peds/Ped.h"
#include "frontend/MiniMapRenderThread.h"
#include "Game/Weather.h"
#include "objects/ProcObjects.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "Renderer/Lights/lights.h"
#include "renderer/Lights/LODLights.h"
#include "renderer/PostProcessFX.h"
#include "renderer/RenderPhases/RenderPhaseFX.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "TimeCycle/TimeCycle.h"
#include "Vehicles/Vehicle.h"
#include "vehicles/VehicleGadgets.h"
#include "vfx/Misc\BrightLights.h"
#include "Vfx/Misc/Coronas.h"
#include "Vfx/Misc/DistantLights.h"
#include "Vfx/GPUParticles/PtFxGPUManager.h"
#include "Vfx/sky/Sky.h"
#include "vfx/particles/ptfxmanager.h"
#include "vfx/VisualEffects.h"
#include "vfx/misc/Puddles.h"

#include "data/aes_init.h"
AES_INIT_D;

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVisualSettings 	g_visualSettings;
static char 		g_pathBuffer[512];
#if __DEV
const char*			g_lastString = NULL;
#endif


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS NameVal
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Constructor
///////////////////////////////////////////////////////////////////////////////

				NameVal::NameVal				(const char* name, const char* value)
{
	m_hashName = Hash(name);
	m_value = (float)atof(value);
}


///////////////////////////////////////////////////////////////////////////////
//  Constructor
///////////////////////////////////////////////////////////////////////////////


				NameVal::NameVal				(const char* name, float value)
{
	m_hashName = Hash(name);
	m_value = value;
}


///////////////////////////////////////////////////////////////////////////////
//  Hash
///////////////////////////////////////////////////////////////////////////////

u32			NameVal::Hash					(const char* name)
{
	// copy the name to a local string
	char hashName[256];
	strncpy(hashName, name, 256);

	// remove the platform extension
	char* platformExt = strstr(hashName, HASH_PLATFORM);
	if (platformExt)
	{
		*platformExt = '\0';
	}

	// compute a hash value for the local string
	u32 hashVal = atStringHash(hashName);

	// rehash with the platform extension
	if (platformExt) 
	{
		hashVal = atStringHash(HASH_PLATFORM, hashVal);
	}

	return hashVal;
}



///////////////////////////////////////////////////////////////////////////////
//  CLASS CVisualSettings
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Load
///////////////////////////////////////////////////////////////////////////////

bool			CVisualSettings::Load							(const char* fileName)
{
	tcAssertf(fileName, "CVisualSettings::Load - invalid filename passed in");

	// open the file for reading
	FileHandle fid = CFileMgr::OpenFile(fileName, "rb");
	if (!CFileMgr::IsValidFileHandle(fid))
	{
		Errorf("Could not load Visual config data file '%s'", fileName);
		return false;
	}

	// store the filename
#if __DEV
	m_filename = fileName;
#endif

	// reset the hash list
	m_hashList.Reset();

	// read in the lines of the file
	const char* line = ReadLine(fid);
	while (line)
	{
		// parse the line
		NameVal newVal;
		if (ParseNameVal(line, newVal))
		{
			// add to the hash list
			m_hashList.PushAndGrow(newVal);
		}

		// next line
		line = ReadLine(fid);
	}

	// close the file
	CFileMgr::CloseFile(fid);

	// sort the hash list
	std::sort(m_hashList.begin(), m_hashList.end());

	// set the loaded flag
	m_isLoaded = true;

	return true;
}


///////////////////////////////////////////////////////////////////////////////
//  LoadAll
///////////////////////////////////////////////////////////////////////////////

bool			CVisualSettings::LoadAll							() 
{
	// reload the settings file
	if (g_visualSettings.Load("commoncrc:/data/visualsettings.dat"))
	{
		// load the values into each system
		g_timeCycle.LoadVisualSettings();
		CVehicle::UpdateVisualDataSettings();
		CPed::UpdateVisualDataSettings();
		g_coronas.UpdateVisualDataSettings();
		CPtFxGPUManager::UpdateVisualDataSettings();
		RenderPhaseSeeThrough::UpdateVisualDataSettings();
		CRenderPhaseCascadeShadowsInterface::UpdateVisualDataSettings();
		g_distantLights.UpdateVisualDataSettings();
		PostFX::UpdateVisualDataSettings();
		g_sky.UpdateVisualSettings();
		Lights::UpdateVisualDataSettings();
		CMiniMap_RenderThread::UpdateVisualDataSettings();
		CLODLights::UpdateVisualDataSettings();
		g_weather.UpdateVisualDataSettings(); 
		g_ptFxManager.UpdateVisualDataSettings();
		CVisualEffects::UpdateVisualDataSettings();
		g_brightLights.UpdateVisualDataSettings();
		g_procObjMan.UpdateVisualDataSettings();
		CPed::LoadVisualSettings();
		CSearchLight::UpdateVisualDataSettings();
		PuddlePass::UpdateVisualDataSettings();

		return true;
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  ParseNameVal
///////////////////////////////////////////////////////////////////////////////

bool			CVisualSettings::ParseNameVal					(const char* line, NameVal& val) const
{
	char lineBlock[512];
	strncpy(lineBlock, line, 512);

	if (lineBlock[0] == '#' || lineBlock[0] == ',')
	{
		return false;
	}
	char* valuePtr = lineBlock;
	while (*valuePtr != ' ')
	{
		if (*valuePtr == '\0') 
		{ 
			return false; 
		}
		valuePtr++;
	}
	*valuePtr = '\0';

	val = NameVal(lineBlock, valuePtr + 1);
	return true;
}


///////////////////////////////////////////////////////////////////////////////
//  ReadLine
///////////////////////////////////////////////////////////////////////////////

const char*		CVisualSettings::ReadLine						(FileHandle& fileId) const
{
	const char* pLine;
	while ((pLine = CFileMgr::ReadLine(fileId)) != NULL)
	{
		if(*pLine != '/' && *pLine != '\0' && *pLine != ' ')
			break;
	}
	return pLine; 
}


///////////////////////////////////////////////////////////////////////////////
//  HashPath
///////////////////////////////////////////////////////////////////////////////

u32			CVisualSettings::HashPath						(const char* strA, const char* strB) const
{
	DEV_ONLY(g_lastString = g_pathBuffer);
	strcpy(g_pathBuffer, strA);
	strcat(g_pathBuffer, ".");
	strcat(g_pathBuffer, strB);
	return atStringHash(g_pathBuffer);
}


///////////////////////////////////////////////////////////////////////////////
//  Get
///////////////////////////////////////////////////////////////////////////////

float			CVisualSettings::Get							(const char* name, float defaultVal, bool mustExist) const
{
	DEV_ONLY(g_lastString = name);
	return Get(atStringHash(name), defaultVal, mustExist);
}


///////////////////////////////////////////////////////////////////////////////
//  Get
///////////////////////////////////////////////////////////////////////////////

float			CVisualSettings::Get							(const char* nameA, const char* nameB, float defaultVal, bool mustExist) const
{
	u32 hashName = HashPath(nameA, nameB);
	return Get(hashName, defaultVal, mustExist);
}


///////////////////////////////////////////////////////////////////////////////
//  Get
///////////////////////////////////////////////////////////////////////////////

float			CVisualSettings::Get							(const u32 hashName, float defaultVal, bool DEV_ONLY(mustExist) ) const
{
#if RSG_DURANGO
	u32 platformHash = atStringHash(".xb1", hashName);
#elif RSG_ORBIS
	u32 platformHash = atStringHash(".ps4", hashName);
#else
	u32 platformHash = atStringHash(".x64", hashName);
#endif

	float val = defaultVal;
	if (GetVal(platformHash, val) || GetVal(hashName, val))
	{
		return val;
	}

#if __DEV
	if( mustExist )	
		Errorf("Couldn't find '%s' in the configuration file  '%s'", g_lastString, m_filename.c_str());
#endif // __DEV
		
	return val;
}


///////////////////////////////////////////////////////////////////////////////
//  GetVal
///////////////////////////////////////////////////////////////////////////////

bool			CVisualSettings::GetVal							(const u32 hashName, float& val) const
{
	NameVal searchVal(hashName, 0.0f);
	const NameVal* itor = std::lower_bound(m_hashList.begin(), m_hashList.end(), searchVal);
	if (itor != m_hashList.end() && itor->m_hashName == hashName)
	{
		val = itor->m_value;
		return true;
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  GetColor
///////////////////////////////////////////////////////////////////////////////

Vec3V_Out		CVisualSettings::GetColor						(const char* name) const
{
	DEV_ONLY(g_lastString = name);
	return Vec3V(Get(HashPath(name, "red")), Get(HashPath(name, "green")), Get(HashPath(name, "blue")));
}


///////////////////////////////////////////////////////////////////////////////
//  GetVec3V
///////////////////////////////////////////////////////////////////////////////

Vec3V_Out		CVisualSettings::GetVec3V						(const char* name) const
{
	DEV_ONLY(g_lastString = name);
	return Vec3V(Get(HashPath(name, "x")), Get(HashPath(name, "y")), Get(HashPath(name, "z")));
}


///////////////////////////////////////////////////////////////////////////////
//  GetVec4V
///////////////////////////////////////////////////////////////////////////////

Vec4V_Out		CVisualSettings::GetVec4V						(const char* name) const
{
	DEV_ONLY(g_lastString = name);
	return Vec4V(Get(HashPath(name, "x")), Get(HashPath(name, "y")), Get(HashPath(name, "z")), Get(HashPath(name, "w")));
}



///////////////////////////////////////////////////////////////////////////////
//  CLASS ConfigLightSettings
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Set
///////////////////////////////////////////////////////////////////////////////

void			ConfigLightSettings::Set					(const CVisualSettings& visualSettings, const char* type, const char* colorName)
{
	atString path(type);

	intensity = visualSettings.Get(type, "intensity");
	radius = visualSettings.Get(type, "radius");
	falloffExp = visualSettings.Get(type, "falloffExp", 8.0f, false);
	innerConeAngle = visualSettings.Get(type, "innerConeAngle");
	outerConeAngle = visualSettings.Get(type, "outerConeAngle");

	coronaHDR = visualSettings.Get(type, "coronaHDR", 1.0f);
	coronaSize = visualSettings.Get(type, "coronaSize", 1.0f);

	shadowBlur = visualSettings.Get(type, "shadowBlur", 0.0f);
	capsuleLength = visualSettings.Get(type, "capsuleLength", 0.0f);

	if (*colorName != '\0')
	{
		path +=".";
		path += colorName;
	}
	path += ".color";
	colour = visualSettings.GetColor(path.c_str());

	extraFlags = 0;
	if (visualSettings.Get(type, "useSun", 0.0f) > 0.5f) { extraFlags |= LIGHTFLAG_CALC_FROM_SUN; };
	if (visualSettings.Get(type, "useDynamicShadows", 0.0f) > 0.5f) { extraFlags |= (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS); };
}



///////////////////////////////////////////////////////////////////////////////
//  CLASS ConfigVehicleEmissiveSettings
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Set
///////////////////////////////////////////////////////////////////////////////

void			ConfigVehicleEmissiveSettings::Set			(const CVisualSettings& visualSettings)
{
	memset(lights_On_day,	0xFF,	sizeof(lights_On_day));
	memset(lights_Off_day,	0x00,	sizeof(lights_Off_day));
	memset(lights_On_night,	0xFF,	sizeof(lights_On_night));
	memset(lights_Off_night,0x00,	sizeof(lights_Off_night));

	float fOn;
	float fOff;

	// Day
	fOn = visualSettings.Get("car.defaultlight.day.emissive.on");
	fOff = visualSettings.Get("car.defaultlight.day.emissive.off");
	lights_On_day[CVehicleLightSwitch::LW_DEFAULT]				= fOn;
	lights_Off_day[CVehicleLightSwitch::LW_DEFAULT]				= fOff;

	fOn = visualSettings.Get("car.headlight.day.emissive.on");
	fOff = visualSettings.Get("car.headlight.day.emissive.off");
	lights_On_day[CVehicleLightSwitch::LW_HEADLIGHT_L]			= fOn;
	lights_On_day[CVehicleLightSwitch::LW_HEADLIGHT_R]			= fOn;
	lights_Off_day[CVehicleLightSwitch::LW_HEADLIGHT_L]			= fOff;
	lights_Off_day[CVehicleLightSwitch::LW_HEADLIGHT_R]			= fOff;
	lights_On_day[CVehicleLightSwitch::LW_EXTRALIGHT_1]			= fOn;
	lights_Off_day[CVehicleLightSwitch::LW_EXTRALIGHT_1]		= fOff;
	lights_On_day[CVehicleLightSwitch::LW_EXTRALIGHT_2]			= fOn;
	lights_Off_day[CVehicleLightSwitch::LW_EXTRALIGHT_2]		= fOff;
	lights_On_day[CVehicleLightSwitch::LW_EXTRALIGHT_3]			= fOn;
	lights_Off_day[CVehicleLightSwitch::LW_EXTRALIGHT_3]		= fOff;
	lights_On_day[CVehicleLightSwitch::LW_EXTRALIGHT_4]			= fOn;
	lights_Off_day[CVehicleLightSwitch::LW_EXTRALIGHT_4]		= fOff;

	fOn = visualSettings.Get("car.taillight.day.emissive.on");
	fOff = visualSettings.Get("car.taillight.day.emissive.off");
	lights_On_day[CVehicleLightSwitch::LW_TAILLIGHT_L]			= fOn;
	lights_On_day[CVehicleLightSwitch::LW_TAILLIGHT_R]			= fOn;
	lights_Off_day[CVehicleLightSwitch::LW_TAILLIGHT_L]			= fOff;
	lights_Off_day[CVehicleLightSwitch::LW_TAILLIGHT_R]			= fOff;

	fOn = visualSettings.Get("car.indicator.day.emissive.on");
	fOff = visualSettings.Get("car.indicator.day.emissive.off");
	lights_On_day[CVehicleLightSwitch::LW_INDICATOR_FL]			= fOn;
	lights_On_day[CVehicleLightSwitch::LW_INDICATOR_FR]			= fOn;
	lights_On_day[CVehicleLightSwitch::LW_INDICATOR_RL]			= fOn;
	lights_On_day[CVehicleLightSwitch::LW_INDICATOR_RR]			= fOn;
	lights_Off_day[CVehicleLightSwitch::LW_INDICATOR_FL]		= fOff;
	lights_Off_day[CVehicleLightSwitch::LW_INDICATOR_FR]		= fOff;
	lights_Off_day[CVehicleLightSwitch::LW_INDICATOR_RL]		= fOff;
	lights_Off_day[CVehicleLightSwitch::LW_INDICATOR_RR]		= fOff;

	fOn = visualSettings.Get("car.brakelight.day.emissive.on");
	fOff = visualSettings.Get("car.brakelight.day.emissive.off");
	lights_On_day[CVehicleLightSwitch::LW_BRAKELIGHT_L]			= fOn;
	lights_On_day[CVehicleLightSwitch::LW_BRAKELIGHT_R]			= fOn;
	lights_Off_day[CVehicleLightSwitch::LW_BRAKELIGHT_L]		= fOff;
	lights_Off_day[CVehicleLightSwitch::LW_BRAKELIGHT_R]		= fOff;

	fOn = visualSettings.Get("car.middlebrakelight.day.emissive.on");
	fOff = visualSettings.Get("car.middlebrakelight.day.emissive.off");
	lights_On_day[CVehicleLightSwitch::LW_BRAKELIGHT_M]			= fOn;
	lights_Off_day[CVehicleLightSwitch::LW_BRAKELIGHT_M]		= fOff;

	fOn = visualSettings.Get("car.reversinglight.day.emissive.on");
	fOff = visualSettings.Get("car.reversinglight.day.emissive.off");
	lights_On_day[CVehicleLightSwitch::LW_REVERSINGLIGHT_L]		= fOn;
	lights_On_day[CVehicleLightSwitch::LW_REVERSINGLIGHT_R]		= fOn;
	lights_Off_day[CVehicleLightSwitch::LW_REVERSINGLIGHT_L]	= fOff;
	lights_Off_day[CVehicleLightSwitch::LW_REVERSINGLIGHT_R]	= fOff;

	fOn = visualSettings.Get("car.extralight.day.emissive.on");
	fOff = visualSettings.Get("car.extralight.day.emissive.off");
	lights_On_day[CVehicleLightSwitch::LW_EXTRALIGHT]			= fOn;
	lights_Off_day[CVehicleLightSwitch::LW_EXTRALIGHT]			= fOff;

	fOn = visualSettings.Get("car.sirenlight.day.emissive.on");
	fOff = visualSettings.Get("car.sirenlight.day.emissive.off");
	lights_On_day[CVehicleLightSwitch::LW_SIRENLIGHT]			= fOn;
	lights_Off_day[CVehicleLightSwitch::LW_SIRENLIGHT]			= fOff;


	// Night
	fOn = visualSettings.Get("car.defaultlight.night.emissive.on");
	fOff = visualSettings.Get("car.defaultlight.night.emissive.off");
	lights_On_night[CVehicleLightSwitch::LW_DEFAULT]				= fOn;
	lights_Off_night[CVehicleLightSwitch::LW_DEFAULT]				= fOff;

	fOn = visualSettings.Get("car.headlight.night.emissive.on");
	fOff = visualSettings.Get("car.headlight.night.emissive.off");
	lights_On_night[CVehicleLightSwitch::LW_HEADLIGHT_L]			= fOn;
	lights_On_night[CVehicleLightSwitch::LW_HEADLIGHT_R]			= fOn;
	lights_Off_night[CVehicleLightSwitch::LW_HEADLIGHT_L]			= fOff;
	lights_Off_night[CVehicleLightSwitch::LW_HEADLIGHT_R]			= fOff;
	lights_On_night[CVehicleLightSwitch::LW_EXTRALIGHT_1]			= fOn;
	lights_Off_night[CVehicleLightSwitch::LW_EXTRALIGHT_1]			= fOff;
	lights_On_night[CVehicleLightSwitch::LW_EXTRALIGHT_2]			= fOn;
	lights_Off_night[CVehicleLightSwitch::LW_EXTRALIGHT_2]			= fOff;
	lights_On_night[CVehicleLightSwitch::LW_EXTRALIGHT_3]			= fOn;
	lights_Off_night[CVehicleLightSwitch::LW_EXTRALIGHT_3]			= fOff;
	lights_On_night[CVehicleLightSwitch::LW_EXTRALIGHT_4]			= fOn;
	lights_Off_night[CVehicleLightSwitch::LW_EXTRALIGHT_4]			= fOff;

	fOn = visualSettings.Get("car.taillight.night.emissive.on");
	fOff = visualSettings.Get("car.taillight.night.emissive.off");
	lights_On_night[CVehicleLightSwitch::LW_TAILLIGHT_L]			= fOn;
	lights_On_night[CVehicleLightSwitch::LW_TAILLIGHT_R]			= fOn;
	lights_Off_night[CVehicleLightSwitch::LW_TAILLIGHT_L]			= fOff;
	lights_Off_night[CVehicleLightSwitch::LW_TAILLIGHT_R]			= fOff;

	fOn = visualSettings.Get("car.indicator.night.emissive.on");
	fOff = visualSettings.Get("car.indicator.night.emissive.off");
	lights_On_night[CVehicleLightSwitch::LW_INDICATOR_FL]			= fOn;
	lights_On_night[CVehicleLightSwitch::LW_INDICATOR_FR]			= fOn;
	lights_On_night[CVehicleLightSwitch::LW_INDICATOR_RL]			= fOn;
	lights_On_night[CVehicleLightSwitch::LW_INDICATOR_RR]			= fOn;
	lights_Off_night[CVehicleLightSwitch::LW_INDICATOR_FL]			= fOff;
	lights_Off_night[CVehicleLightSwitch::LW_INDICATOR_FR]			= fOff;
	lights_Off_night[CVehicleLightSwitch::LW_INDICATOR_RL]			= fOff;
	lights_Off_night[CVehicleLightSwitch::LW_INDICATOR_RR]			= fOff;

	fOn = visualSettings.Get("car.brakelight.night.emissive.on");
	fOff = visualSettings.Get("car.brakelight.night.emissive.off");
	lights_On_night[CVehicleLightSwitch::LW_BRAKELIGHT_L]			= fOn;
	lights_On_night[CVehicleLightSwitch::LW_BRAKELIGHT_R]			= fOn;
	lights_Off_night[CVehicleLightSwitch::LW_BRAKELIGHT_L]			= fOff;
	lights_Off_night[CVehicleLightSwitch::LW_BRAKELIGHT_R]			= fOff;

	fOn = visualSettings.Get("car.middlebrakelight.night.emissive.on");
	fOff = visualSettings.Get("car.middlebrakelight.night.emissive.off");
	lights_On_night[CVehicleLightSwitch::LW_BRAKELIGHT_M]			= fOn;
	lights_Off_night[CVehicleLightSwitch::LW_BRAKELIGHT_M]			= fOff;

	fOn = visualSettings.Get("car.reversinglight.night.emissive.on");
	fOff = visualSettings.Get("car.reversinglight.night.emissive.off");
	lights_On_night[CVehicleLightSwitch::LW_REVERSINGLIGHT_L]		= fOn;
	lights_On_night[CVehicleLightSwitch::LW_REVERSINGLIGHT_R]		= fOn;
	lights_Off_night[CVehicleLightSwitch::LW_REVERSINGLIGHT_L]		= fOff;
	lights_Off_night[CVehicleLightSwitch::LW_REVERSINGLIGHT_R]		= fOff;

	fOn = visualSettings.Get("car.extralight.night.emissive.on");
	fOff = visualSettings.Get("car.extralight.night.emissive.off");
	lights_On_night[CVehicleLightSwitch::LW_EXTRALIGHT]				= fOn;
	lights_Off_night[CVehicleLightSwitch::LW_EXTRALIGHT]			= fOff;	

	fOn = visualSettings.Get("car.sirenlight.night.emissive.on");
	fOff = visualSettings.Get("car.sirenlight.night.emissive.off");
	lights_On_night[CVehicleLightSwitch::LW_SIRENLIGHT]				= fOn;
	lights_Off_night[CVehicleLightSwitch::LW_SIRENLIGHT]			= fOff;

	emissiveMultiplier = visualSettings.Get("car.emissiveMultiplier",2.0f);
}

void			ConfigVehiclePositionLightSettings::Set			(const CVisualSettings& visualSettings, const char* type)
{
	atString path(type);

	nearStrength	= visualSettings.Get(type, "nearStrength",		15.0f);
	farStrength		= visualSettings.Get(type, "farStrength",		60.0f);
	nearSize		= visualSettings.Get(type, "nearSize",			0.25f);
	farSize			= visualSettings.Get(type, "farSize",			0.75f);
	intensity_typeA	= visualSettings.Get(type, "intensity_typeA",	30.0f);
	intensity_typeB	= visualSettings.Get(type, "intensity_typeB",	150.0f);
	radius			= visualSettings.Get(type, "radius",		2.0f);

	atString rightColorPath = path;
	rightColorPath += ".rightColor";
	rightColor = visualSettings.GetColor(rightColorPath.c_str());


	atString leftColorPath = path;
	leftColorPath += ".leftColor";
	leftColor = visualSettings.GetColor(leftColorPath.c_str());
}

void			ConfigVehicleWhiteLightSettings::Set			(const CVisualSettings& visualSettings, const char* type)
{
	atString path(type);

	nearStrength	= visualSettings.Get(type, "nearStrength",		15.0f);
	farStrength		= visualSettings.Get(type, "farStrength",		60.0f);
	nearSize		= visualSettings.Get(type, "nearSize",			0.25f);
	farSize			= visualSettings.Get(type, "farSize",			0.75f);
	intensity		= visualSettings.Get(type, "intensity",	150.0f);
	radius			= visualSettings.Get(type, "radius",		2.0f);

	path+= ".color";
	color = visualSettings.GetColor(path.c_str());
}

void			ConfigBoatLightSettings::Set					(const CVisualSettings& visualSettings, const char* type)
{
	atString path(type);

	intensity		= visualSettings.Get(type, "intensity",	150.0f);
	radius			= visualSettings.Get(type, "radius", 2.0f);

	path+= ".color";
	color = visualSettings.GetColor(path.c_str());
}

void		ConfigPlaneEmergencyLightsSettings::Set				(const CVisualSettings& visualSettings, const char* type)
{
	atString path(type);

	intensity		= visualSettings.Get(type, "intensity",			150.0f);
	falloff			= visualSettings.Get(type, "falloff",			2.0f);
	falloffExponent	= visualSettings.Get(type, "falloff.exponent",	2.0f);
	innerAngle		= visualSettings.Get(type, "inner.angle",		0.0f);
	outerAngle		= visualSettings.Get(type, "outer.angle",		90.0f);
	rotation		= visualSettings.Get(type, "rotation",			360.0f) * DtoR;

	path += ".color";
	color = visualSettings.GetColor(path.c_str());
}

void		ConfigPlaneInsideHullSettings::Set					(const CVisualSettings& visualSettings, const char* type)
{
	atString path(type);

	intensity		= visualSettings.Get(type, "intensity",			150.0f);
	falloff			= visualSettings.Get(type, "falloff",			2.0f);
	falloffExponent	= visualSettings.Get(type, "falloff.exponent",	2.0f);
	innerAngle		= visualSettings.Get(type, "inner.angle",		0.0f);
	outerAngle		= visualSettings.Get(type, "outer.angle",		90.0f);

	path += ".color";
	color = visualSettings.GetColor(path.c_str());
}

void		ConfigPlaneControlPanelSettings::Set				(const CVisualSettings& visualSettings, const char* type)
{
	atString path(type);

	intensity		= visualSettings.Get(type, "intensity",			150.0f);
	falloff			= visualSettings.Get(type, "falloff",			2.0f);
	falloffExponent	= visualSettings.Get(type, "falloff.exponent",	2.0f);

	path += ".color";
	color = visualSettings.GetColor(path.c_str());
}
