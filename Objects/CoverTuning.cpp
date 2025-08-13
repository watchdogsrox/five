//
// Objects/CoverTuning.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "ModelInfo/ModelInfo.h"
#include "Objects/CoverTuning.h"
#include "Objects/Object.h"
#include "bank/bank.h"
#include "parser/manager.h"
#include "fwsys/fileExts.h"
#include <algorithm>	// std::sort()

#include "Objects/CoverTuning_parser.h"

SCENE_OPTIMISATIONS()

//-----------------------------------------------------------------------------
CCoverTuning* CCoverTuningManager::sm_pDefaultTuning = NULL; 
CCoverTuningManager* CCoverTuningManager::sm_pInstance = NULL;

void CCoverTuningManager::InitClass()
{
	// Allocate the instance. It's not expected to get a call to this
	// if the instance already exists.
	Assert(!sm_pInstance);
	sm_pInstance = rage_new CCoverTuningManager;

	Assert(!sm_pDefaultTuning);
	sm_pDefaultTuning = rage_new CCoverTuning;
}


void CCoverTuningManager::ShutdownClass()
{
	// Note: this is called by CFileLoader::ResetLevelInstanceInfo(). Not sure
	// that it's always going to be matched by a call to InitClass(), so we don't
	// assert that the manager exists here.
	delete sm_pInstance;
	sm_pInstance = NULL;

	delete sm_pDefaultTuning;
	sm_pDefaultTuning = NULL;
}


void CCoverTuningManager::Load(const char* filename)
{
#if __BANK
	// Note: while we could potentially load from multiple files, the save functionality
	// really only works with one file as we don't keep track of which data came from each file.
	if( !IsSaveFileValid() )
	{
		m_SaveFileName = filename;
	}
#endif	// __BANK

	// Try to load into a temporary CCoverTuningFile object.
	CCoverTuningFile data;
	char metadataFilename[RAGE_MAX_PATH];
	formatf(metadataFilename, RAGE_MAX_PATH, "%s.%s", filename, META_FILE_EXT);	
	fwPsoStoreLoader loader = fwPsoStoreLoader::MakeSimpleFileLoader<CCoverTuningFile>();

	fwPsoStoreLoadInstance inst(data.parser_GetPointer());
	loader.Load(metadataFilename, inst);

	if(!Verifyf(inst.IsLoaded(), "Failed to parse the metadata PSO file (%s)", metadataFilename))
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
		const CCoverTuningFile::NamedTuning& namedTuning = data.m_NamedTuningArray[i];

		const int newIndex = i + numBefore;
		m_NamedTuningToIndexMap.Insert(atHashWithStringBank(namedTuning.m_Name.c_str()), newIndex);
		m_TuningArray[newIndex] = namedTuning.m_Tuning;
	}

	// Update the mapping from model names to tuning parameters, from the
	// temporary object.
	for(int i = 0; i < data.m_ModelToTuneMapping.GetCount(); i++)
	{
		const char* modelName = data.m_ModelToTuneMapping[i].m_ModelName.c_str();
		const char* tuningName = data.m_ModelToTuneMapping[i].m_TuningName.c_str();

		const int* indexPtr = m_NamedTuningToIndexMap.Access(atHashWithStringBank(tuningName));
		if(Verifyf(indexPtr, "Failed to find tuning '%s' for model '%s'.", tuningName, modelName))
		{
			m_ModelToTuningIndexMap.Insert(modelName, *indexPtr);
		}
	}
}

const CCoverTuning& CCoverTuningManager::GetTuningForModel(u32 queryModelHash) const
{
	// First, look up the model name and see if we should use a specific tuning
	// parameter set.
	atHashWithStringBank modelKey(queryModelHash);
	const int* tuningIndexPtr = m_ModelToTuningIndexMap.Access(modelKey);
	if(tuningIndexPtr)
	{
		return m_TuningArray[*tuningIndexPtr];
	}

	// If nothing was found for the model, we use default parameters
	return *sm_pDefaultTuning;
}

#if __BANK

void CCoverTuningManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Cover Tuning", false);
	if(IsSaveFileValid())
	{
		bank.AddButton("Save", datCallback(CFA(CCoverTuningManager::SaveCB)));
		bank.AddButton("Reload", datCallback(CFA(CCoverTuningManager::ReloadCB)));
	}

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

	bank.PushGroup("Model to Tuning Mapping", false);
	for(atMap<atHashWithStringBank, int>::Iterator iter2 = m_ModelToTuningIndexMap.CreateIterator();
			!iter2.AtEnd(); iter2.Next())
	{
		const char* modelName = iter2.GetKey().GetCStr();
		int CoverTuningIndex = iter2.GetData();
		const char* tuningName = FindNameForTuning(CoverTuningIndex);
		bank.AddSeparator();
		bank.AddText("Model Name", (char*)modelName, ustrlen(modelName) + 1, true);
		bank.AddText("Tuning Name", (char*)tuningName, ustrlen(tuningName) + 1, true);
	}
	bank.AddSeparator();

	bank.PopGroup();

	bank.PopGroup();
}

void CCoverTuningManager::Reload()
{
	// If we have a filename we already loaded from
	if( IsSaveFileValid() )
	{
		// Clear out the existing data
		m_TuningArray.clear();
		m_NamedTuningToIndexMap.Kill();
		m_ModelToTuningIndexMap.Kill();

		// Load normally
		Load(m_SaveFileName);

		// Recompute vehicle model info cover after loading new data, as vehicle cover data may have changed
		CModelInfo::DebugRecalculateAllVehicleCoverPoints();

		// Debug reset the object cover cache, so that object cover is recomputed after the reload
		CCachedObjectCoverManager::DebugResetCache();

		// Report success
		Displayf("Reloaded tuning data from '%s.%s'.", m_SaveFileName.c_str(), META_FILE_EXT);
		return;
	}

	// Report failure
	Displayf("Reload tuning data failed, invalid save file name, probably never loaded.");
}

void CCoverTuningManager::Save()
{
	// Temporary object to populate.
	CCoverTuningFile data;

	// Populate the array of tuning parameter sets.
	const int numTuning = m_TuningArray.GetCount();
	for(int i = 0; i < numTuning; i++)
	{
		CCoverTuningFile::NamedTuning* tuning = &data.m_NamedTuningArray.Grow();
		tuning->m_Name = FindNameForTuning(i);
		tuning->m_Tuning = m_TuningArray[i];
	}

	// Populate the array of mappings between model names and tuning parameter sets.
	for(atMap<atHashWithStringBank, int>::Iterator iter = m_ModelToTuningIndexMap.CreateIterator();
			!iter.AtEnd(); iter.Next())
	{
		CCoverTuningFile::ModelToTuneName &mapping = data.m_ModelToTuneMapping.Grow();
		mapping.m_ModelName = iter.GetKey().GetCStr();
		mapping.m_TuningName = FindNameForTuning(iter.GetData());
	}

	// Sort the model array. We have taken the values we loaded and put them through an
	// atMap we iterated over, so they may not be in the same order as when they loaded.
	// By sorting, we ensure that we can't end up with an ever-changing order as we load
	// and save and load again.
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
	if(PARSER.SaveObject(m_SaveFileName, "pso.meta", &data))
	{
		Displayf("Successfully saved tuning data to '%s.pso.meta'. Note that it will need to be reconverted before it can be loaded again", m_SaveFileName.c_str());
	}
}

const char* CCoverTuningManager::FindNameForTuning(const CCoverTuning& tuning) const
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
	return "None";
}

const char* CCoverTuningManager::FindNameForTuning(int tuningIndex) const
{
	// We can reuse the other FindNameForTuning() function for this.
	return FindNameForTuning(m_TuningArray[tuningIndex]);
}

#endif	// __BANK

CCoverTuningManager::CCoverTuningManager()
{
}


CCoverTuningManager::~CCoverTuningManager()
{
}

//-----------------------------------------------------------------------------

// End of file 'Objects/CoverTuning.cpp'
