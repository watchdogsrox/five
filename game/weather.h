///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	Weather.h
//	BY	: 	Mark Nicholson (Adapted from original by Obbe)
//	FOR	:	Rockstar North
//	ON	:	11 Aug 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WEATHER_H
#define WEATHER_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													


#include "atl/array.h"

// game
#include "Debug/Debug.h"
#include "Renderer/Water.h"
#include "Game/Wind.h"
#include "Vfx/GPUParticles/PtFxGPUManager.h"
#include "Vfx/systems/VfxLightning.h"
#include "control/replay/ReplaySettings.h"


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define	WEATHER_MAX_TYPES			(16)
#define WEATHER_MAX_CYCLE_ENTRIES	(256)
#define WEATHER_MAX_CYCLE_TIME		(1000000)


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////													

extern dev_float WEATHER_WIND_SPEED_MAX;


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////

struct CWindDisturbanceAttr;


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

enum eSystemType
{
	SYSTEM_TYPE_DROP			= 0,
	SYSTEM_TYPE_MIST,
	SYSTEM_TYPE_GROUND
};

enum eDriveType
{
	DRIVE_TYPE_NONE				= 0,
	DRIVE_TYPE_RAIN,
	DRIVE_TYPE_SNOW,
	DRIVE_TYPE_UNDERWATER,
	DRIVE_TYPE_REGION
};

class CWeatherGpuFxFromXmlFile
{
public:
	atString m_Name;
	eSystemType m_SystemType;
	atString m_diffuseName;
	atString m_distortionTexture;
	atString m_diffuseSplashName;
	eDriveType m_driveType;
	float m_windInfluence;
	float m_gravity;
	atString m_emitterSettingsName;
	atString m_renderSettingsName;

	PAR_SIMPLE_PARSABLE;
};

class CWeatherTypeFromXmlFile
{
public:
	atString m_Name;
	float m_Sun;
	float m_Cloud;
	float m_WindMin;
	float m_WindMax;
	float m_Rain;
	float m_Snow;
	float m_SnowMist;
	float m_Fog;
	bool m_Lightning;
	bool m_Sandstorm;
	
	float m_RippleBumpiness;
	float m_RippleMinBumpiness;
	float m_RippleMaxBumpiness;
	float m_RippleBumpinessWindScale;
	float m_RippleSpeed;
	float m_RippleVelocityTransfer;
	float m_RippleDisturb;
	float m_OceanBumpiness;
	float m_DeepOceanScale;
	float m_OceanNoiseMinAmplitude;
	float m_OceanWaveAmplitude;
	float m_ShoreWaveAmplitude;
	float m_OceanWaveWindScale;
	float m_ShoreWaveWindScale;
	float m_OceanWaveMinAmplitude;
	float m_ShoreWaveMinAmplitude;
	float m_OceanWaveMaxAmplitude;
	float m_ShoreWaveMaxAmplitude;
	float m_OceanFoamIntensity;
	
	atString m_DropSettingName;
	atString m_MistSettingName;
	atString m_GroundSettingName;
	atString m_TimeCycleFilename;
	atHashString m_CloudSettingsName;

	PAR_SIMPLE_PARSABLE;
};

class CWeatherTypeEntry
{
public:
	atString m_CycleName;
	u32 m_TimeMult;

	PAR_SIMPLE_PARSABLE;
};

class CContentsOfWeatherXmlFile
{
public:
	float m_VersionNumber;
	atArray<CWeatherGpuFxFromXmlFile> m_WeatherGpuFx;

	atArray<CWeatherTypeFromXmlFile> m_WeatherTypes;

	atArray<CWeatherTypeEntry> m_WeatherCycles;

	PAR_SIMPLE_PARSABLE;
};

class CWeatherType
{
	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	public: ///////////////////////////

		u32				m_hashName;

		float			m_sun;
		float			m_cloud;
		float			m_windMin;
		float			m_windMax;
		float			m_rain;
		float			m_snow;
		float			m_snowMist;
		float			m_fog;
		bool			m_lightning;
		bool			m_sandstorm;

		s32				m_gpuFxDropIndex;
		s32				m_gpuFxMistIndex;
		s32				m_gpuFxGroundIndex;

		CSecondaryWaterTune m_secondaryWaterTunings;
		
		char			m_timeCycleFilename				[64];
		rage::atHashString	m_CloudSettingsName;

#if !__FINAL
		char			m_name							[32];
#endif

};


class CWeather
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main functions
		void 				Init						(unsigned initMode);
		void 				Shutdown					(unsigned shutdownMode);

		void				Update						();
		void				UpdateOverTimeTransition	();

#if GTA_REPLAY
		void				SetFromReplay(u32 prevWeatherType, u32 nextWeatherType, float interp, float random, float randomCloud, float wetness, u32 forceOverTimeTypeIndex, float overTimeInterp, float overTimeTransitionLeft, float overTimeTransitionTotal);
#endif

#if __BANK
		void				InitLevelWidgets			();
		void				ShutdownLevelWidgets		();
		void				RenderDebug					();
#endif

		void				UpdateVisualDataSettings	();

		enum ForceTypeFlags
		{
			FT_None,
			FT_NoLumReset = 0x1,
#if !__FINAL
			FT_SendNetworkUpdate = 0x2,
#endif
		};
		
		// type functions
		void				ForceType					(u32 typeIndex);															// sets the force type - the weather will gradually go towards this	
		void				ForceTypeNow				(s32 typeIndex, const unsigned flags);										// sets the force type with immediate effect	
		void				ForceTypeClear				(const unsigned flags = FT_None);											// clears the force type - normal weather update will resume
		void				OverrideType				(u32 typeIndex, bool resetWetness = false);
		bool				GetIsOverriding()			{ return (m_overriddenTypeIndex >= 0); }
		void				StopOverriding				();
		void				IncrementType				();
		void				RandomiseType				();	
		void				SetTypeNow					(u32 typeIndex);														// set to desired weather, keep it for one cycle, then resume normal weather update

		void				ForceWeatherTypeOverTime	(u32 typeIndex, float time);
		u32					GetOverTimeType()			{ return m_forceOverTimeTypeIndex; }
		float				GetOverTimeTotalTime()		{ return m_overTimeTransitionTotal; }
		float				GetOverTimeLeftTime()		{ return m_overTimeTransitionLeft; }
		float				GetOverTimeInterp()			{ return m_overTimeInterp; }
		bool				IsInOvertimeTransition()	{ return m_forceOverTimeTypeIndex != 0xffffffff; }

		// network functions
		void				NetworkInit					(u32 networkTime, u32 cycleTime);
		void				NetworkShutdown				();
		void				NetworkInitFromGlobal		(s32 cycleIndex, u32 cycleTime, u32 transitionTime, bool noCloudUpdate = false);
		void				UpdateNetworkTransition		();
        u32					GetGlobalPrevTypeIndex		();
        bool                IsInNetTransition           ()                          {return m_netTransitionTimeLeft > 0;}
        float               GetNetTransitionInterp      ()                          {return m_netTransitionInterp;}
		float               GetNetTransitionPrevInterp  ()                          {return m_netTransitionPrevInterp;}
		float               GetNetTransitionNextInterp  ()                          {return m_netTransitionNextInterp;}
		bool				GetNetSendForceUpdate		()							{return m_netSendForceUpdate;}
		bool				GetNetIsOverriding			()							{return (m_overriddenTypeIndex >= 0) BANK_ONLY(|| (m_overridePrevTypeIndex > 0) || (m_overrideNextTypeIndex > 0)); }

const	CWeatherType&		GetNetTransitionPrevType    ();        
const	CWeatherType&		GetNetTransitionNextType    ();         

#if !__FINAL
		void				SendDebugNetworkWeather		();
		void				SyncDebugNetworkWeather		(bool force, u32 typeIndex, u32 cycleIndex, u32 transitionTime);
#endif      

        u32                 GetSystemTime               ()                          {return m_systemTime;}
        void				GetFromRatio				(float ratio, s32& cycleIndex, u32& cycleTimer); 

#if !__NO_OUTPUT
		const char*			GetDisplayCode				();
#endif

		// wind functions
		void				ProcessEntityWindDisturbance(CEntity* pEntity, const CWindDisturbanceAttr* pWindEffect, s32 effectId);
		s32					AddWindThermal				(Vec3V_In vPos, float radius, float height, float maxStrength, float deltaStrength);
		void				RemoveWindThermal			(s32 index);

		void				AddWindExplosion			(Vec3V_In vPos, float radius, float force);
		void				AddWindDirExplosion			(Vec3V_In vPos, Vec3V_In vDir, float length, float radius, float force);
		void				AddWindDownwash				(Vec3V_In vPos, Vec3V_In vDir, float radius, float force, float groundZ, float zFadeThreshMin, float zFadeThreshMax);
		void				AddWindHemisphere			(Vec3V_In vPos, Vec3V_In vDir, float radius, float force);
		void				AddWindSphere				(Vec3V_In vPos, Vec3V_In vVelocity, float radius, float forceMult = 1.0f);
	#if GTA_REPLAY
		bool				ShouldAddPerFrameWind		();
	#endif // GTA_REPLAY
		// query functions
		bool 				IsWindy						()							{return m_wind >= 0.5f;}	
		bool 				IsRaining					()							{return m_rain >= 0.05f;}	
		bool 				IsSnowing					()							{return m_snow >= 0.2f;}	

		// access functions
		u32					GetNumTypes					()							{return m_numTypes;}

		u32					GetTypeHashName				(u32 typeIndex)				{return m_types[typeIndex].m_hashName;}

		u32					GetPrevTypeIndex			()							{return m_prevTypeIndex;}
        void				SetPrevTypeIndex			(u32 val)					{if (Verifyf(val>=0 && val<m_numTypes, "SetPrevTypeIndex passed an invalid weather type (%d) - ignoring", val)) m_prevTypeIndex = val;}
const	CWeatherType&		GetPrevType					();

		u32					GetNextTypeIndex			()							{return m_nextTypeIndex;}
		void				SetNextTypeIndex			(u32 val)					{if (Verifyf(val>=0 && val<m_numTypes, "SetNextTypeIndex passed an invalid weather type (%d) - ignoring", val)) m_nextTypeIndex = val;}
const	CWeatherType&		GetNextType					()							{Assert(m_nextTypeIndex<m_numTypes); return m_types[m_nextTypeIndex];}

const	CWeatherType&		GetTypeInfo					(u32 val)					{Assert(val<m_numTypes); return m_types[val];}

		float				GetInterp					()							{return m_interp;}
		void				SetInterp					(float val)					{m_interp = val; m_cycleTimer = (int)(m_currentMsPerCycle * m_interp);}

		float				GetRandom					()							{return m_random;}
		float				GetRandomCloud				()							{return m_randomCloud;}

		bool				GetIsForced					()							{return m_forceTypeIndex>-1;}
		s32					GetForceTypeIndex			()							{return m_forceTypeIndex;}
		void				SetForceTypeIndex			(s32 index)					{m_forceTypeIndex = index;}
	
		s32					GetCycleIndex				()							{return m_cycleIndex;}
		void				SetCycleIndex				(s32 val)					{m_cycleIndex = val;}
		u32					GetCycleTimer				()							{return m_cycleTimer;}

		float				GetSun						()							{return m_sun;}
		float				GetRain						()							{return m_rain;}
		float				GetTimeCycleAdjustedRain	()							{return m_rain * (1.f - g_timeCycle.GetStaleNoRainRipples());}
		void				SetRain						(float val)					{m_rain = val;}
		void				SetRainOverride				(float val)					{m_overrideRain = val;}
		float				GetSnow						()							{return m_snow;}
		float				GetSnowMist					() const					{ return m_snowMist; }
		void				SetSnowOverride				(float val)					{m_overrideSnow = val;}
		void				SetSnowMistOverride			(float val)					{m_overrideSnowMist = val;}
		float				GetFog						()							{return m_fog;}
		float				GetWetness					()							{return Water::IsCameraUnderwater()? 0.0f : m_wetness;}
		float				GetWetnessNoWaterCameraCheck()							{return m_wetness;}
		float				GetWind						()							{return m_wind;}
		void				SetWind						(float val)					{m_wind = val;}
		void				SetWindOverride				(float val)					{m_overrideWind = val;}
		float				GetWindSpeed				()							{return m_windSpeed;}
		float				GetWindSpeedMax				()							{return WEATHER_WIND_SPEED_MAX;}
		Vec3V				GetWindDirection			()							{return m_rageWind.GetWindDirection();}
		void				SetWindDirectionOverride	(float val)					{m_rageWind.SetWindDirectionOverride(val);}
		float				GetLightning				()							{return g_vfxLightningManager.GetLightningChance();}
		float				GetLightningFlashIntensityAmount()						{return g_vfxLightningManager.GetLightningFlashAmount();}
		float				GetSandstorm				()							{return m_sandstorm;}

		float				GetDayNightRatio			()							{return m_dayNightRatio;}

		float				GetTemperature				(Vec3V_In vPos);
		float				GetTemperature				(float height);
		float				GetTemperatureAtSeaLevel	()							{return m_temperatureAtSeaLevel;}
		float				GetTemperatureAtCameraHeight()							{return m_temperatureAtCameraHeight;}

static  void				ForceLightningFlash			();
static  void				ForceOverTimeTransition		();
		// debug
#if __BANK
static	void				DebugIncrementType			();
static	void				DebugRandomiseType			();
static  void				SaveTempWaterTunings		();
static  void				CopyWaterTunings			();

		s32&				GetOverridePrevTypeIndexRef	()							{return m_overridePrevTypeIndex;}
		s32&				GetOverrideNextTypeIndexRef	()							{return m_overrideNextTypeIndex;}
		float&				GetOverrideInterpRef		()							{return m_overrideInterp;}						

		void				MakeWaterFlat				();
		
#endif

		// Water override access
		void				SetOverrideShoreWaveAmplitude(float value)				{m_overrideSecondaryWatertunings.m_ShoreWaveAmplitude = value;}
		void				SetOverrideShoreWaveMinAmplitude(float value)			{m_overrideSecondaryWatertunings.m_ShoreWaveMinAmplitude = value;}
		void				SetOverrideShoreWaveMaxAmplitude(float value)			{m_overrideSecondaryWatertunings.m_ShoreWaveMaxAmplitude = value;}
		void				SetOverrideOceanNoiseMinAmplitude(float value)			{m_overrideSecondaryWatertunings.m_OceanNoiseMinAmplitude = value;}
		void				SetOverrideOceanWaveAmplitude(float value)				{m_overrideSecondaryWatertunings.m_OceanWaveAmplitude = value;}
		void				SetOverrideOceanWaveMinAmplitude(float value)			{m_overrideSecondaryWatertunings.m_OceanWaveMinAmplitude = value;}
		void				SetOverrideOceanWaveMaxAmplitude(float value)			{m_overrideSecondaryWatertunings.m_OceanWaveMaxAmplitude = value;}
		void				SetOverrideRippleBumpiness(float value)					{m_overrideSecondaryWatertunings.m_RippleBumpiness = value;}
		void				SetOverrideRippleMinBumpiness(float value)				{m_overrideSecondaryWatertunings.m_RippleMinBumpiness = value;}
		void				SetOverrideRippleMaxBumpiness(float value)				{m_overrideSecondaryWatertunings.m_RippleMaxBumpiness = value;}
		void				SetOverrideRippleDisturb(float value)					{m_overrideSecondaryWatertunings.m_RippleDisturb = value;}
		void				SetWaterOverrideStrength(float value)					{m_overrideWaterTunings = value;}
		void				FadeInWaterOverrideOverTime(float time);
		void				FadeOutWaterOverrideOverTime(float time);
		
		s32					GetTypeIndex				(const char* typeName);
		s32					GetTypeIndex				(u32 typeNameHash);

#if !__FINAL
		const char*			GetTypeName					(u32 typeIndex)				{return m_typeNames[typeIndex+1];}			// the list is offset by 1 (as "disabled" is at the 0 entry)
		const char**		GetTypeNames				(s32 offset)				{return &m_typeNames[offset];}
#endif


	private: //////////////////////////

		// init helper functions
		void				LoadData					();
		void				LoadXmlFile(const char *pFilename);

		void				InitialiseTypeFromXmlFile(const CWeatherTypeFromXmlFile &TypeDataFromXmlFile);
		void				InitialiseCycleEntryFromXmlFile(const CWeatherTypeEntry &cycleEntry);

		// update helper functions
		void				UpdateSun					();
		void				UpdateRain					();
		void				UpdateSnow					();
		void				UpdateSnowMist				();
		void				UpdateWindInterp			();
		void				UpdateWind					();
		void				UpdateFog					();
		void				UpdateLightning				();
		void				UpdateSandstorm				();
		void				UpdateWetness				();
		void				UpdateDayNightRatio			();
		void				UpdateTemperature			();
		void				UpdateWaterSettings			();

		float				CalcWind					(u32 typeIndex);

		void				UpdateAllNow				(u32 typeIndex, bool noLumReset = false, bool resetWetness = false);


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		u32					m_numTypes;	
		atRangeArray<CWeatherType,WEATHER_MAX_TYPES> m_types;


#if !__FINAL
		atRangeArray<const char*,WEATHER_MAX_TYPES +1> m_typeNames;
#endif

		u32					m_numCycleEntries;
		atRangeArray<u8, WEATHER_MAX_CYCLE_ENTRIES> m_cycle;// the list of types that we are going to cycle through
		atRangeArray<u8, WEATHER_MAX_CYCLE_ENTRIES> m_cycleTimeMult;
	
		u32					m_msPerCycle;											// the number of milliseconds between each weather cycle entry
		u32					m_currentMsPerCycle;									// the number of milliseconds between each weather cycle entry
		u32					m_cycleTimer;											// timer for knowing when the next weather cycle entry is due
		u32					m_networkInitNetworkTime;								// network time on network initialisation
		u32					m_networkInitCycleTime;									// cycle time on network initialisation
		u32					m_lastSysTime;											// last system time value (used in network games)
		u32                 m_systemTime;                                           // time for full weather cycle from start to finish

		float				m_changeCloudOnSameCloudType;
		float				m_changeCloundOnSameWeatherType;
		
		u32					m_prevTypeIndex;										// the weather type we're interpolating from 
		u32					m_nextTypeIndex;										// the weather type we're interpolating to
		float				m_interp;												// the current interpolation
		float				m_random;												// random value generated each frame for all weather randoms (so it's recordable)
		float				m_randomCloud;											// need different random for clouds so you don't see any syncs
	
		u32					m_forceOverTimeTypeIndex;
		float				m_overTimeTransitionTotal;
		float				m_overTimeTransitionLeft;
		float				m_overTimeInterp;

		s32					m_forceTypeIndex;										// to store any forced weather type

		s32					m_cycleIndex;											// index into the weather type list

		// weather state variables
		float				m_sun;													// sun value (0.0 to 1.0)
		float				m_rain;													// rain value (0.0 to 1.0)
		float				m_snow;													// snow value (0.0 to 1.0)
		float				m_snowMist;												// snow mist value (0.0 to 1.0)
		float				m_wind;													// wind value
		float				m_windInterpCurr;										// current wind interp value (distance between the wind's min and max values)
		float				m_windInterpTarget;										// target wind interp value (distance between the wind's min and max values)
		float				m_windSpeed;											// speed of the wind (after multiplied by the wind speed mutliplier)
		float				m_waterSpeed;											// speed of the water (used by the windfield)
		float				m_fog;													// fog value (0.0 to 1.0)
		float				m_wetness;												// wetness value (0.0 to 1.0)

		float				m_sandstorm;											// sandstorm value (0.0 to 1.0)

		float				m_dayNightRatio;										// ratio from day to night for the lighting code

		float				m_temperatureAtSeaLevel;								// the temperature at sea level (z=0.0f)
		float				m_temperatureAtCameraHeight;							// the temperature at the camera height

		CSecondaryWaterTune m_secondaryWaterTunings;								// water values
		
		// rage wind variables
		CWind				m_rageWind;		

		// override variables
		float				m_overrideWind;											// override for the wind value (<0.0 is off)
		float				m_overrideRain;											// override for the rain value (<0.0 is off)
		float				m_overrideSnow;											// override for the snow value (<0.0 is off) 
		float				m_overrideSnowMist;										// override for the snow mist value (<0.0 is off) 
		
		// network override
		int					m_overriddenTypeIndex;									// overrides the weather dictated by the host

		// network transition
		u32					m_netTransitionTimeTotal;
		u32					m_netTransitionTimeLeft;
		float				m_netTransitionInterp;
		bool				m_netSendForceUpdate;

		// stored weather states for network transition
		float				m_netTransitionSun;
		float				m_netTransitionRain;
		float				m_netTransitionSnow;
		float				m_netTransitionSnowMist;
		float				m_netTransitionWind;
		float				m_netTransitionFog;
		float				m_netTransitionLightening; 
		float				m_netTransitionWetness;
		float				m_netTransitionSandstorm;
		CSecondaryWaterTune m_netTransitionWater;

        u32                 m_netTransitionPrevTypeIndex;
        u32                 m_netTransitionNextTypeIndex;
        float               m_netTransitionPrevInterp;
		float				m_netTransitionNextInterp;

		float				m_overrideWaterTunings;									// override control for water tuning
		CSecondaryWaterTune m_overrideSecondaryWatertunings;						// override for water tuning
		float				m_overTimeSecondaryWaterTotal;
		float				m_overTimeSecondaryWaterLeft;
		float				m_overTimeTarget;

		bool				m_hasCheckedCloudTransition;
		
		// debug variables
#if __BANK		
		s32					m_debugPrevTypeIndex;									// copy of the prev type to display in the widget combo			
		s32					m_debugNextTypeIndex;									// copy of the next type to display in the widget combo			
		s32					m_debugForceTypeIndex;									// copy of the force type to display in the widget combo			

		// debug override variables
		s32					m_overridePrevTypeIndex;								// override for the prev type (0 is off)
		s32					m_overrideNextTypeIndex;								// override for the next type (0 is off)
		float				m_overrideInterp;										// override for the interp (<0.0 is off)

		u32					m_debugForceOverTimeTypeIndex;
		float				m_debugOverTimeTransition;
		float				m_debugOverTimeTransitionInterp;
		bool				m_debugOverTimeEnableTransitionInterp;

		float				m_overrideSun;											// override for the sun value (<0.0 is off)
		float				m_overrideWindInterp;									// override for the wind interp value (<0.0 is off) 
		float				m_overrideFog;											// override for the fog value (<0.0 is off)
		float				m_overrideWetness;										// override for the wetness value (<0.0 is off)
		float				m_overrideSandstorm;									// override for the sandstorm value (<0.0 is off)
		float				m_overrideDayNightRatio;								// override for the day night ratio value (<0.0 is off)
#endif


};


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CWeather		g_weather;

extern	dev_float		WEATHER_WIND_SPEED_MAX; 
extern	dev_float		WEATHER_WATER_SPEED_MAX; 


#endif // WEATHER_H



