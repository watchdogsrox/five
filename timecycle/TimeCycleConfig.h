///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	TimeCycleConfig.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	19 Aug 2008
//	WHAT:	 
//
///////////////////////////////////////////////////////////////////////////////

#ifndef TIMECYCLECONFIG_H
#define	TIMECYCLECONFIG_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "vectormath/vec4v.h"
#include "atl/array.h"
#include "atl/string.h"
#include "math/amath.h"

// game
#include "System/FileMgr.h"
#include "Vehicles/VehicleLightSwitch.h"


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define HASHVAL(a, b) a, b 

#if RSG_DURANGO
#define HASH_PLATFORM ".xb1"
#elif RSG_ORBIS
#define HASH_PLATFORM ".ps4"
#else
#define HASH_PLATFORM ".x64"
#endif


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////													

class CVisualSettings;


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

// ConfigLightSettings
// - allows for tweaking and setting light values from the visual defaults 
//   configuration file
struct ConfigLightSettings
{
	float				intensity;
	float				radius;
	float				falloffExp;
	float				innerConeAngle;
	float				outerConeAngle;
	float				coronaHDR;
	float				coronaSize;
	float				shadowBlur;
	float				capsuleLength;
	Vec3V				colour;
	u32					extraFlags;
	
	void				Set					(const CVisualSettings& visualSettings, const char* type, const char* colorName="");
};

// ConfigVehicleEmissiveSettings
struct ConfigVehicleEmissiveSettings
{
	float				lights_On_day		[CVehicleLightSwitch::LW_LIGHTCOUNT+1];
	float				lights_Off_day		[CVehicleLightSwitch::LW_LIGHTCOUNT+1];

	float				lights_On_night		[CVehicleLightSwitch::LW_LIGHTCOUNT+1];
	float				lights_Off_night	[CVehicleLightSwitch::LW_LIGHTCOUNT+1];
	
	float				emissiveMultiplier;
	void				Set					(const CVisualSettings& visualSettings);
	float				GetLightOnValue		(int lightIdx, float daynightFade)
	{
		Assert(lightIdx < CVehicleLightSwitch::LW_LIGHTCOUNT);
		return Lerp(daynightFade, lights_On_day[lightIdx], lights_On_night[lightIdx]);
	}

	float				GetLightOffValue	(int lightIdx, float daynightFade)
	{
		Assert(lightIdx < CVehicleLightSwitch::LW_LIGHTCOUNT);
		return Lerp(daynightFade, lights_Off_day[lightIdx], lights_Off_night[lightIdx]);
	}
	
};


// ConfigVehicleDayNightLightSettings
struct ConfigVehicleDayNightLightSettings
{
	float sunrise;
	float sunset;
};

// ConfigVehiclePositionLightSettings
struct ConfigVehiclePositionLightSettings
{
	Vec3V rightColor;
	Vec3V leftColor;
	float nearStrength;
	float farStrength;
	float nearSize;
	float farSize;
	float intensity_typeA;
	float intensity_typeB;
	float radius;
	
	void				Set					(const CVisualSettings& visualSettings, const char* type);
};
	

// ConfigVehiclePositionLightSettings
struct ConfigVehicleWhiteLightSettings
{
	Vec3V color;
	float nearStrength;
	float farStrength;
	float nearSize;
	float farSize;
	float intensity;
	float radius;

	void				Set					(const CVisualSettings& visualSettings, const char* type);
};

// ConfigPlaneEmergencyLightsSettings
struct ConfigPlaneEmergencyLightsSettings
{
	Vec3V color;
	float intensity;
	float innerAngle;
	float outerAngle;
	float falloff;
	float falloffExponent;
	float rotation;

	void				Set					(const CVisualSettings& visualSettings, const char* type);
};

// ConfigPlaneInsideHullSettings
struct ConfigPlaneInsideHullSettings
{
	Vec3V color;
	float intensity;
	float falloff;
	float falloffExponent;
	float innerAngle;
	float outerAngle;

	void				Set					(const CVisualSettings& visualSettings, const char* type);
};

// ConfigPlaneControlPanelSettings
struct ConfigPlaneControlPanelSettings
{
	Vec3V color;
	float intensity;
	float falloff;
	float falloffExponent;

	void				Set					(const CVisualSettings& visualSettings, const char* type);
};

// ConfigBoatLightSettings
struct ConfigBoatLightSettings
{
	Vec3V color;
	float intensity;
	float radius;

	void				Set					(const CVisualSettings& visualSettings, const char* type);
};

///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  NameVal
///////////////////////////////////////////////////////////////////////////////

class NameVal
{
	///////////////////////////////////
	// FRIENDS 
	///////////////////////////////////

	friend		class CVisualSettings;


	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////


							NameVal				()														{}
							NameVal				(const char* name, const char* value);
							NameVal				(const char* name, float value);

		friend	bool		operator<			(const NameVal& a, const NameVal& b)					{return a.m_hashName < b.m_hashName;}
		friend inline bool	operator==			(const NameVal& a, const NameVal& b)					{return a.m_hashName == b.m_hashName;}


	protected: ////////////////////////

							NameVal				(u32 hashName, float value)							{m_hashName = hashName; m_value = value;}	
		u32				Hash				(const char* name);


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		u32				m_hashName;
		float				m_value;
};




///////////////////////////////////////////////////////////////////////////////
//  CVisualSettings
///////////////////////////////////////////////////////////////////////////////

// simple configuration file which uses name value pairs
// values are always floats to simplify code

class CVisualSettings
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// constructor
								CVisualSettings				()												{m_isLoaded = false;}

		// main interface
		bool					Load						(const char* fileName);

		// access functions
		bool					GetIsLoaded					() const										{return m_isLoaded;}
		float 					Get							(const char* nameA, const char* nameB, float defaultVal=0.0f, bool mustExist = true) const;
		float 					Get							(const char* name, float defaultVal=0.0f, bool mustExist = true) const;
		float 					Get							(u32 hashCode, float defaultVal=0.0f, bool mustExist = true) const;
		Vec3V_Out				GetColor					(const char* name) const;
		Vec3V_Out				GetVec3V					(const char* name) const;
		Vec4V_Out 				GetVec4V					(const char* name) const;
		template<class T> T		GetV						(u32 hashCode, float defaultVal=0.0f, bool mustExist = true) const		{return static_cast<T>(Get(hashCode, defaultVal,mustExist));}

		bool					LoadAll						();

	private: //////////////////////////

		bool 					GetVal						(u32 hashCode, float& val) const;
		bool 					ParseNameVal				(const char* line, NameVal& val) const;

		const char*				ReadLine					(FileHandle& fileId) const;

		// helper functions to make up config file values
		// adds a dot value between the two strings
		// by return a hashValue saves allocating strings and memory
		u32 					HashPath					(const char* strA, const char* strB) const;


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		bool				m_isLoaded; 
		atArray<NameVal>	m_hashList; 

#if __DEV
		atString			m_filename;
#endif

};


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////	

extern CVisualSettings g_visualSettings;




#endif // TIMECYCLECONFIG_H


