//
// Objects/DoorTuning.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef OBJECTS_DOOR_TUNING_H
#define OBJECTS_DOOR_TUNING_H

#include "atl/array.h"
#include "atl/map.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"
#include "spatialdata/aabb.h"
#include "vectormath/vec3v.h"

//-----------------------------------------------------------------------------

enum StdDoorRotDir
{
	StdDoorOpenBothDir = 0,
	StdDoorOpenNegDir,
	StdDoorOpenPosDir,
};

/*
PURPOSE
	Tuning parameters for a CDoor object. Multiple CDoor objects, of the same
	or different models, may point to the same CDoorTuning object, and they
	are all owned by CDoorTuningManager.
*/
class CDoorTuning
{
public:
	enum eDoorTuningFlags
	{
		DontCloseWhenTouched				= BIT0,		// For standard doors, don't set the target ratio to 0 on impact.
		AutoOpensForSPVehicleWithPedsOnly	= BIT1,		// Single player vehicles with allowed peds
		AutoOpensForSPPlayerPedsOnly		= BIT2,		// Single player peds on foot or in a vehicle
		AutoOpensForMPVehicleWithPedsOnly	= BIT3,		// Multi-player vehicles with allowed peds
		AutoOpensForMPPlayerPedsOnly		= BIT4,		// Multi-player player peds on foot
		DelayDoorClosingForPlayer			= BIT5,		// For standard doors, when the player runs into them add a delay to the close of the door
		AutoOpensForAllVehicles				= BIT6,		// Flag that will allow vehicles to auto open doors regardless of passengers
		IgnoreOpenDoorTaskEdgeLerp			= BIT7,		// Flag to determine whether or not we should extend the door edge when opening the door via CTaskOpenDoor
		AutoOpensForLawEnforcement			= BIT8,		// Opens for any police
	};

	// PURPOSE:	Extra local space offset for positioning the volume used for
	//			automatic opening.
	Vec3V		m_AutoOpenVolumeOffset;

	// PURPOSE:	Flags from eDoorTuningFlags, controlling door behavior.
	fwFlags32	m_Flags;

	// PURPOSE:	Multiplicative modifier on the radius at which this door
	//			auto-opens. At 1.0, the opening radius will match the width
	//			of the door.
	float		m_AutoOpenRadiusModifier;

	// PURPOSE:	Rate at which this door auto-opens. This is the inverse of the
	//			time it takes to open, e.g. a value of 0.25 will open the door
	//			in 4 seconds.
	float		m_AutoOpenRate;

	// PURPOSE: Cosine of the angle between the ped to door vector and the ped fwd vector threshold to auto open
	float		m_AutoOpenCosineAngleBetweenThreshold;

	// PURPOSE:	If true the rate at which this door auto-opens/closes is
	//			slowed as the door approaches full open/close. 
	bool		m_AutoOpenCloseRateTaper;

	// PURPOSE:	If true the auto open trigger volume is a box not the default sphere. 
	bool		m_UseAutoOpenTriggerBox;

	// PURPOSE:	If true the auto open trigger volume is a box of size determined by the spdAABB info
	//			(only valid if m_UseAutoOpenTriggerBox is set)
	bool		m_CustomTriggerBox;

	// PURPOSE:	Customized trigger box min/max relative values (only valid if m_UseAutoOpenTriggerBox and m_CustomTriggerBox are set)
	spdAABB		m_TriggerBoxMinMax;

	// PURPOSE:	If true, the door may break if hit by a vehicle.
	bool		m_BreakableByVehicle;

	// PURPOSE: The magnitude of the impulse required to break the constraint.
	float		m_BreakingImpulse;

	// PURPOSE:	If true, the door will latch shut.
	bool		m_ShouldLatchShut;

	// PURPOSE:	Multiplier applied to the mass of the door (default is 1.0f)
	float		m_MassMultiplier;

	// PURPOSE:	Multiplier applied to the mass of the door (default is 1.0f)
	float		m_WeaponImpulseMultiplier;

	// PURPOSE:	Allow override of open angle from 0 -> 180 with 0.0f considered closed (default is 0.0f)
	float		m_RotationLimitAngle;

	// PURPOSE:	Allow override of the angular velocity limit
	float		m_TorqueAngularVelocityLimit;

	// PURPOSE:	For standard doors this setting limits what direction the door can open (default is OpenBothDir)
	StdDoorRotDir m_StdDoorRotDir;

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

/*
PURPOSE
	Parsable class used by CDoorTuningManager for loading and saving
	door tuning data. Only exists temporarily.
*/
class CDoorTuningFile
{
public:
	// PURPOSE:	Door tuning data and its name.
	struct NamedTuning
	{
		// PURPOSE:	Name of the tuning data, for association with doors.
		ConstString		m_Name;

		// PURPOSE:	Tuning parameters.
		CDoorTuning		m_Tuning;

		bool operator<(const NamedTuning& other) const
		{	return stricmp(m_Name.c_str(), other.m_Name.c_str()) < 0;	}

		PAR_SIMPLE_PARSABLE;
	};

	// PURPOSE:	Name of a door model and the name of the tuning data it should use.
	struct ModelToTuneName
	{
		// PURPOSE:	Name of the door model.
		ConstString		m_ModelName;

		// PURPOSE:	Name of the tuning parameters the door should use.
		ConstString		m_TuningName;

		bool operator<(const ModelToTuneName& other) const
		{	return stricmp(m_ModelName.c_str(), other.m_ModelName.c_str()) < 0;	}

		PAR_SIMPLE_PARSABLE;
	};

	// PURPOSE:	Array of all named tuning parameter sets in this file.
	atArray<NamedTuning>		m_NamedTuningArray;

	// PURPOSE:	Array of all mappings from model names to parameter sets.
	atArray<ModelToTuneName>	m_ModelToTuneMapping;

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

/*
PURPOSE
	Manager for door tuning data. The data is loaded from an XML file as
	a part of the level loading procedure, and consists of named CDoorTuning
	objects and a map from door model names to CDoorTuning objects.
*/
class CDoorTuningManager
{
public:
	// PURPOSE:	Create the manager instance.
	static void InitClass();

	// PURPOSE:	Destroy the manager instance, if it exists.
	static void ShutdownClass();

	// PURPOSE:	Get the manager instance.
	static CDoorTuningManager& GetInstance()
	{	Assert(sm_Instance);
		return *sm_Instance;
	}

	// PURPOSE:	Load tuning data from the specified file.
	void Load(const char* filename);

	// PURPOSE:	To be called after Load(), ensuring that we at least have some default
	//			tuning for the doors even if no file was loaded successfully.
	void FinishedLoading();

	// PURPOSE:	Get tuning data for a particular door model, or fall back on the defaults
	//			for the door category (CDoor::DOOR_TYPE_...).
	const CDoorTuning& GetTuningForDoorModel(u32 doorModelHash, int doorCategory) const;

#if __BANK
	void AddWidgets(bkBank& bank);
	void Save();

	// PURPOSE:	Find the name of a CDoorTuning object in this manager.
	// NOTES:	Quite slow, only for development usage.
	const char* FindNameForTuning(const CDoorTuning& tuning) const;

	// PURPOSE:	Find the name of a CDoorTuning object in this manager.
	// NOTES:	Quite slow, only for development usage.
	const char* FindNameForTuning(int tuningIndex) const;

	static void SaveCB()
	{	GetInstance().Save();	}
#endif

protected:
	CDoorTuningManager();
	~CDoorTuningManager();

	// PURPOSE:	Get the hash value of the name of the tuning parameter set to use
	//			for a particular category of doors.
	u32 GetDefaultTuningHashForDoorCategory(int doorCategory) const;

	// PURPOSE:	Map from names of door models to the index of the tuning data they
	//			should use, in m_TuningArray[].
	atMap<atHashWithStringBank, int>	m_ModelToTuningIndexMap;

	// PURPOSE:	Map from names of tuning parameter sets to the index of them
	//			in m_TuningArray[].
	atMap<atHashWithStringBank, int>	m_NamedTuningToIndexMap;

	// PURPOSE:	Array of tuning data.
	// NOTES:	CDoor objects currently point straight into this array, so be
	//			very careful if you try to change the array on the fly.
	atArray<CDoorTuning>				m_TuningArray;

	// PURPOSE:	Pointer to the instance.
	static CDoorTuningManager*			sm_Instance;
};

//-----------------------------------------------------------------------------

#endif	// OBJECTS_DOOR_TUNING_H

// End of file 'Objects/DoorTuning.h'
