//
// Objects/DoorTuning.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "fwsys/fileExts.h"
#include "Objects/DoorTuning.h"
#include "Objects/Door.h"
#include "bank/bank.h"
#include "parser/manager.h"

#include <algorithm>	// std::sort()

#include "Objects/DoorTuning_parser.h"

SCENE_OPTIMISATIONS()

//-----------------------------------------------------------------------------

CDoorTuningManager* CDoorTuningManager::sm_Instance = NULL;

void CDoorTuningManager::InitClass()
{
	// Allocate the instance. It's not expected to get a call to this
	// if the instance already exists.
	Assert(!sm_Instance);
	sm_Instance = rage_new CDoorTuningManager;
}


void CDoorTuningManager::ShutdownClass()
{
	// Note: this is called by CFileLoader::ResetLevelInstanceInfo(). Not sure
	// that it's always going to be matched by a call to InitClass(), so we don't
	// assert that the manager exists here.
	delete sm_Instance;
	sm_Instance = NULL;
}


void CDoorTuningManager::Load(const char* filename)
{
	// Try to load into a temporary CDoorTuningFile object.
	CDoorTuningFile data;
	if (!fwPsoStoreLoader::LoadDataIntoObject(filename, META_FILE_EXT, data))
	{
		// Load failed.
		return;
	}

	// Append the tuning parameter sets from the file to the existing array
	// (though in the common case of loading from a single file, it would be empty).
	// We also update m_NamedTuningToIndexMap so we can find the parameter sets by name.
	const int numBefore = m_TuningArray.GetCount();
	const int numToAdd = data.m_NamedTuningArray.GetCount();
	m_TuningArray.ResizeGrow(numBefore + numToAdd);
	for(int i = 0; i < numToAdd; i++)
	{
		const CDoorTuningFile::NamedTuning& namedTuning = data.m_NamedTuningArray[i];

		const int newIndex = i + numBefore;
		m_NamedTuningToIndexMap.Insert(atHashWithStringBank(namedTuning.m_Name.c_str()), newIndex);
		m_TuningArray[newIndex] = namedTuning.m_Tuning;
	}

	// Update the mapping from model names to tuning parameters, from the
	// temporary object.
	for(int i = 0; i < data.m_ModelToTuneMapping.GetCount(); i++)
	{
		const char* doorModelName = data.m_ModelToTuneMapping[i].m_ModelName.c_str();
		const char* tuningName = data.m_ModelToTuneMapping[i].m_TuningName.c_str();

		const int* indexPtr = m_NamedTuningToIndexMap.Access(atHashWithStringBank(tuningName));
		if(Verifyf(indexPtr, "Failed to find tuning '%s' for door model '%s'.", tuningName, doorModelName))
		{
			m_ModelToTuningIndexMap.Insert(doorModelName, *indexPtr);
		}
	}

#if __ASSERT
	for (int i = 0; i < m_TuningArray.GetCount(); ++i)
	{
		Assertf(m_TuningArray[i].m_CustomTriggerBox ? m_TuningArray[i].m_UseAutoOpenTriggerBox : true, "Error in Door Tuning Entry %s, a custom trigger box is set but m_UseAutoOpenTriggerBox is false", CDoorTuningManager::FindNameForTuning(m_TuningArray[i]));
	}
#endif
}


void CDoorTuningManager::FinishedLoading()
{
	// Loop over all possible door categories (sliding, garage, etc), and let
	// GetDefaultTuningHashForDoorCategory() tell us what the default tuning
	// parameters are named. If they don't exist, they will be created with values
	// set from the .psc file. This ensures that even if we didn't successfully load
	// any data, we still have some default data to return through GetTuningForDoorModel().
	for(int i = 0; i < CDoor::NUM_DOOR_TYPES; i++)
	{
		atHashWithStringBank key(GetDefaultTuningHashForDoorCategory(i));
		if(!m_NamedTuningToIndexMap.Access(key))
		{
			const int tuningIndex = m_TuningArray.GetCount();
			CDoorTuning& newTuning = m_TuningArray.Grow();
			PARSER.InitObject(newTuning);
			m_NamedTuningToIndexMap.Insert(key, tuningIndex);
		}
	}
}


const CDoorTuning& CDoorTuningManager::GetTuningForDoorModel(u32 doorModelHash, int doorCategory) const
{
	// First, look up the door model name and see if we should use a specific tuning
	// parameter set.
	atHashWithStringBank doorModelKey(doorModelHash);
	const int* tuningIndexPtr = m_ModelToTuningIndexMap.Access(doorModelKey);
	if(tuningIndexPtr)
	{
		return m_TuningArray[*tuningIndexPtr];
	}

	// If nothing was found for the model, we use default parameters for this
	// type of door.
	atHashWithStringBank tuningHash(GetDefaultTuningHashForDoorCategory(doorCategory));
	tuningIndexPtr = m_NamedTuningToIndexMap.Access(tuningHash);
	Assertf(tuningIndexPtr, "Failed to find default tuning for door category %d.", doorCategory);
	return m_TuningArray[*tuningIndexPtr];
}

#if __BANK

const char* g_DoorMetadataAssetsSubPath = "export/levels/gta5/doortuning.pso.meta";
void CDoorTuningManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Door Tuning", false);
	bank.AddButton("Save", datCallback(CFA(CDoorTuningManager::SaveCB)));
	bank.PushGroup("Tuning Sets", false);
	const int numTuning = m_TuningArray.GetCount();
	for(int i = 0; i < numTuning; i++)
	{
		const char* name = FindNameForTuning(i);

		bank.PushGroup(name, false);
		PARSER.AddWidgets(bank, &m_TuningArray[i]);
		bank.PopGroup();
	}
	bank.PopGroup();

	bank.PushGroup("Door Model to Tuning Mapping", false);
	for(atMap<atHashWithStringBank, int>::Iterator iter2 = m_ModelToTuningIndexMap.CreateIterator();
			!iter2.AtEnd(); iter2.Next())
	{
		const char* doorModelName = iter2.GetKey().GetCStr();
		int doorTuningIndex = iter2.GetData();
		const char* tuningName = FindNameForTuning(doorTuningIndex);
		bank.AddSeparator();
		bank.AddText("Door Model Name", (char*)doorModelName, ustrlen(doorModelName) + 1, true);
		bank.AddText("Tuning Name", (char*)tuningName, ustrlen(tuningName) + 1, true);
	}
	bank.AddSeparator();

	bank.PopGroup();

	bank.PopGroup();
}

void CDoorTuningManager::Save()
{
	// Temporary object to populate.
	CDoorTuningFile data;

	// Populate the array of tuning parameter sets.
	const int numTuning = m_TuningArray.GetCount();
	for(int i = 0; i < numTuning; i++)
	{
		CDoorTuningFile::NamedTuning* tuning = &data.m_NamedTuningArray.Grow();
		tuning->m_Name = FindNameForTuning(i);
		tuning->m_Tuning = m_TuningArray[i];
	}

	// Populate the array of mappings between model names and tuning parameter sets.
	for(atMap<atHashWithStringBank, int>::Iterator iter = m_ModelToTuningIndexMap.CreateIterator();
			!iter.AtEnd(); iter.Next())
	{
		CDoorTuningFile::ModelToTuneName &mapping = data.m_ModelToTuneMapping.Grow();
		mapping.m_ModelName = iter.GetKey().GetCStr();
		mapping.m_TuningName = FindNameForTuning(iter.GetData());
	}

	// Sort the model array. We have taken the values we loaded and put them through an
	// atMap we iterated over, so they may not be in the same order as when they loaded.
	// By sorting, we ensure that we can't end up with an ever-changing order as we load
	// and save and loag again.
	if(data.m_ModelToTuneMapping.GetCount() > 0)
	{
		std::sort(&data.m_ModelToTuneMapping[0], &data.m_ModelToTuneMapping[0] + data.m_ModelToTuneMapping.GetCount());
	}

	// May as well sort the named tuning parameters too, to make it easier to maintain
	// once we have a lot of them.
	if(data.m_NamedTuningArray.GetCount() > 0)
	{
		std::sort(&data.m_NamedTuningArray[0], &data.m_NamedTuningArray[0] + data.m_NamedTuningArray.GetCount());
	}

	// Save it.
	char filename[RAGE_MAX_PATH];
	formatf(filename, RAGE_MAX_PATH, "%s%s", CFileMgr::GetAssetsFolder(), g_DoorMetadataAssetsSubPath);
	if(PARSER.SaveObject(filename, "", &data, parManager::XML))
	{
		Displayf("Successfully saved tuning data to '%s'.", filename);
	}
}


const char* CDoorTuningManager::FindNameForTuning(const CDoorTuning& tuning) const
{
	// Do a reverse lookup in the map. This may be slow and is not meant for anything
	// but development usage.
	for(atMap<atHashWithStringBank, int>::ConstIterator iter = m_NamedTuningToIndexMap.CreateIterator();
			!iter.AtEnd(); iter.Next())
	{
		if(&m_TuningArray[iter.GetData()] == &tuning)
		{
			return iter.GetKey().GetCStr();
		}			
	}
	return "?";
}


const char* CDoorTuningManager::FindNameForTuning(int tuningIndex) const
{
	// We can reuse the other FindNameForTuning() function for this.
	return FindNameForTuning(m_TuningArray[tuningIndex]);
}

#endif	// __BANK

CDoorTuningManager::CDoorTuningManager()
{
}


CDoorTuningManager::~CDoorTuningManager()
{
}


u32 CDoorTuningManager::GetDefaultTuningHashForDoorCategory(int doorCategory) const
{
	// This atHashString is important, because it ensures that the strings
	// we hash up here are inserted in the hash-to-string lookup table so we can
	// save the tuning data out properly later.
	atHashWithStringBank name;

	switch(doorCategory)
	{
		default:
			Assertf(0, "Unknown door category %d.", doorCategory);
			// Intentional fallthrough.
		case CDoor::DOOR_TYPE_NOT_A_DOOR:
		case CDoor::DOOR_TYPE_STD:
		case CDoor::DOOR_TYPE_STD_SC:
			name = "DefaultStandard";
			break;
		case CDoor::DOOR_TYPE_SLIDING_HORIZONTAL:
		case CDoor::DOOR_TYPE_SLIDING_HORIZONTAL_SC:
			name = "DefaultSlidingHorizontal";
			break;
		case CDoor::DOOR_TYPE_SLIDING_VERTICAL:
		case CDoor::DOOR_TYPE_SLIDING_VERTICAL_SC:
			name = "DefaultSlidingVertical";
			break;
		case CDoor::DOOR_TYPE_GARAGE:
		case CDoor::DOOR_TYPE_GARAGE_SC:
			name = "DefaultGarage";
			break;
		case CDoor::DOOR_TYPE_RAIL_CROSSING_BARRIER:
		case CDoor::DOOR_TYPE_RAIL_CROSSING_BARRIER_SC:
		case CDoor::DOOR_TYPE_BARRIER_ARM:
		case CDoor::DOOR_TYPE_BARRIER_ARM_SC:
			name = "DefaultBarrierArm";
			break;
	}

	return name.GetHash();
}

//-----------------------------------------------------------------------------

// End of file 'Objects/DoorTuning.cpp'
