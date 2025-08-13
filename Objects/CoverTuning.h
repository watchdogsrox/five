//
// Objects/CoverTuning.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef OBJECTS_COVER_TUNING_H
#define OBJECTS_COVER_TUNING_H

#include "atl/array.h"
#include "atl/map.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"

//-----------------------------------------------------------------------------

//	Tuning parameters for optional cover specifications for an object.
//	Multiple objects, of the same or different models, may point to the same CCoverTuning.
//	CCoverTunings are all owned by CCoverTuningManager.
class CCoverTuning
{
public:

	// NOTE: This enum has a corresponding definition in CoverTuning.psc
	enum eCoverTuningFlags
	{
		NoCoverNorthFaceEast			= BIT0,		// Prevent cover point generation on object's max Y face on max X end
		NoCoverNorthFaceWest			= BIT1,		// Prevent cover point generation on object's max Y face on min X end
		NoCoverNorthFaceCenter			= BIT2,		// Prevent cover point generation on object's max Y face center
		NoCoverSouthFaceEast			= BIT3,		// Prevent cover point generation on object's min Y face on max X end
		NoCoverSouthFaceWest			= BIT4,		// Prevent cover point generation on object's min Y face on min X end
		NoCoverSouthFaceCenter			= BIT5,		// Prevent cover point generation on object's min Y face center
		NoCoverEastFaceNorth			= BIT6,		// Prevent cover point generation on object's max X face on max Y end
		NoCoverEastFaceSouth			= BIT7,		// Prevent cover point generation on object's max X face on min Y end
		NoCoverEastFaceCenter			= BIT8,		// Prevent cover point generation on object's max X face center
		NoCoverWestFaceNorth			= BIT9,		// Prevent cover point generation on object's min X face on max Y end
		NoCoverWestFaceSouth			= BIT10,	// Prevent cover point generation on object's min X face on min Y end
		NoCoverWestFaceCenter			= BIT11,	// Prevent cover point generation on object's min X face center
		ForceLowCornerNorthFaceEast		= BIT12,	// Force low corner on object's max Y face on max X end
		ForceLowCornerNorthFaceWest		= BIT13,	// Force low corner on object's max Y face on min X end
		ForceLowCornerSouthFaceEast		= BIT14,	// Force low corner on object's min Y face on max X end
		ForceLowCornerSouthFaceWest		= BIT15,	// Force low corner on object's min Y face on min X end
		ForceLowCornerEastFaceNorth		= BIT16,	// Force low corner on object's max X face on max Y end
		ForceLowCornerEastFaceSouth		= BIT17,	// Force low corner on object's max X face on min Y end
		ForceLowCornerWestFaceNorth		= BIT18,	// Force low corner on object's min X face on max Y end
		ForceLowCornerWestFaceSouth		= BIT19,	// Force low corner on object's min X face on min Y end

		NoCoverVehicleDoors				= BIT20,	// Prevent cover point generation on vehicle doors
	};

	// PURPOSE:	Flags from eCoverTuningFlags, controlling cover generation.
	fwFlags32	m_Flags;

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

//	Parsable class used by CCoverTuningManager for loading and saving
//	cover tuning data. Only exists temporarily.
class CCoverTuningFile
{
public:
	// PURPOSE:	Cover tuning data and its name.
	struct NamedTuning
	{
		// PURPOSE:	Name of the tuning data, for association with object models.
		ConstString		m_Name;

		// PURPOSE:	Tuning parameters.
		CCoverTuning		m_Tuning;

		bool operator<(const NamedTuning& other) const
		{	return stricmp(m_Name.c_str(), other.m_Name.c_str()) < 0;	}

		PAR_SIMPLE_PARSABLE;
	};

	// PURPOSE:	Name of an object model and the name of the tuning data it should use.
	struct ModelToTuneName
	{
		// PURPOSE:	Name of the object model.
		ConstString		m_ModelName;

		// PURPOSE:	Name of the tuning parameters the object model should use.
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

//	Manager for cover tuning data. The data is loaded from an XML file as
//	a part of the level loading procedure, and consists of named CCoverTuning
//	objects and a map from object model names to CCoverTuning objects.
class CCoverTuningManager
{
public:
	// PURPOSE:	Create the manager instance.
	static void InitClass();

	// PURPOSE:	Destroy the manager instance, if it exists.
	static void ShutdownClass();

	// PURPOSE:	Get the manager instance.
	static CCoverTuningManager& GetInstance()
	{	Assert(sm_pInstance);
		return *sm_pInstance;
	}

	// PURPOSE:	Load tuning data from the specified file.
	void Load(const char* filename);

	// PURPOSE:	Get tuning data for a particular object model, or fall back on the default
	const CCoverTuning& GetTuningForModel(u32 queryModelHash) const;

#if __BANK
	void AddWidgets(bkBank& bank);
	void Reload();
	void Save();

	// PURPOSE:	Find the name of a CCoverTuning object in this manager.
	// NOTES:	Quite slow, only for development usage.
	const char* FindNameForTuning(const CCoverTuning& tuning) const;

	// PURPOSE:	Find the name of a CCoverTuning object in this manager.
	// NOTES:	Quite slow, only for development usage.
	const char* FindNameForTuning(int tuningIndex) const;

	// PURPOSE: Check that the save file name has been set
	inline bool IsSaveFileValid()
	{
		return (m_SaveFileName.c_str() && m_SaveFileName.c_str()[0]);
	}

	static void SaveCB()
	{	GetInstance().Save();	}

	static void ReloadCB()
	{	GetInstance().Reload();	}

	// PURPOSE:	The name of the file we loaded from, so Save() can save it back out.
	ConstString	m_SaveFileName;
#endif

protected:
	CCoverTuningManager();
	~CCoverTuningManager();

	// PURPOSE:	Map from names of object models to the index of the tuning data they
	//			should use, in m_TuningArray[].
	atMap<atHashWithStringBank, int>	m_ModelToTuningIndexMap;

	// PURPOSE:	Map from names of tuning parameter sets to the index of them
	//			in m_TuningArray[].
	atMap<atHashWithStringBank, int>	m_NamedTuningToIndexMap;

	// PURPOSE:	Array of tuning data.
	// NOTES:	objects currently point straight into this array, so be
	//			very careful if you try to change the array on the fly.
	atArray<CCoverTuning>				m_TuningArray;

	// PURPOSE: Default tuning for all objects
	static CCoverTuning*				sm_pDefaultTuning;

	// PURPOSE:	Pointer to the instance.
	static CCoverTuningManager*			sm_pInstance;
};

//-----------------------------------------------------------------------------

#endif	// OBJECTS_COVER_TUNING_H

// End of file 'Objects/CoverTuning.h'
