#ifndef INC_AMBIENT_MODEL_SET_MANAGER_H
#define INC_AMBIENT_MODEL_SET_MANAGER_H

// Game headers
#include "AI/Ambient/AmbientModelSet.h"

////////////////////////////////////////////////////////////////////////////////
// CAmbientPedModelSet
////////////////////////////////////////////////////////////////////////////////

/*
PURPOSE
	CAmbientModelSet for ped models.
TODO
	Maybe remove (replace with the base class), if we don't find better use for it.
*/
class CAmbientPedModelSet : public CAmbientModelSet
{
public:
	// Note: it wouldn't really work to add any data here, just functions.
};

////////////////////////////////////////////////////////////////////////////////
// CAmbientPropModelSet
////////////////////////////////////////////////////////////////////////////////

/*
PURPOSE
	CAmbientModelSet for prop models.
TODO
	Maybe remove (replace with the base class), if we don't find better use for it.
*/
class CAmbientPropModelSet : public CAmbientModelSet
{
public:
	// Note: it wouldn't really work to add any data here, just functions.
};

////////////////////////////////////////////////////////////////////////////////
// CAmbientModelSetManager
////////////////////////////////////////////////////////////////////////////////

/*
PURPOSE
	Manager for sets of ped, prop, and vehicle models, used for scenarios,
	conditional animations, etc.
*/
class CAmbientModelSetManager
{
public:
	// PURPOSE:	The different types of sets we keep track of.
	enum ModelSetType
	{
		kPedModelSets,
		kPropModelSets,
		kVehicleModelSets,

		kNumModelSetTypes
	};

	// PURPOSE: Initialise
	static void InitClass(u32 uInitMode);

	// PURPOSE: Shutdown
	static void ShutdownClass(u32 uShutdownMode);

	// PURPOSE:	Get the instance.
	static CAmbientModelSetManager& GetInstance()
	{	aiAssert(sm_Instance);
		return *sm_Instance;
	}

	const CAmbientPedModelSet* GetPedModelSet(int index) 
	{
		// Not sure if we should do this cast, perhaps we should remove CAmbientPedModelSet instead.
		return static_cast<const CAmbientPedModelSet*>(m_ModelSets[kPedModelSets].m_ModelSets[index]);
	}

	// Returns the prop set with the given index
	const CAmbientPropModelSet* GetPropSet(int index)
	{
		// Not sure if we should do this cast, perhaps we should remove CAmbientPropModelSet instead.
		return static_cast<CAmbientPropModelSet*>(m_ModelSets[kPropModelSets].m_ModelSets[index]);
	}

	const CAmbientModelSet& GetModelSet(ModelSetType type, int index) 
	{
		return *(m_ModelSets[type].m_ModelSets[index]);
	}


	// PURPOSE: Get index for a model set
	int GetModelSetIndexFromHash(ModelSetType type, u32 hash) const;

	int GetNumModelSets(ModelSetType type) const { return m_ModelSets[type].m_ModelSets.GetCount(); }

	// PURPOSE: Used by the PARSER.
	static const char* GetNameFromModelSet(const CAmbientModelSet* pModelSet);

	// PURPOSE: Used by the PARSER.
	static const CAmbientPedModelSet* GetPedModelSetFromNameCB(const char* name);

	// PURPOSE: Used by the PARSER.
	static const CAmbientModelSet* GetVehicleModelSetFromNameCB(const char* name);

	// PURPOSE: Used by the PARSER.
	static const CAmbientModelSet* GetPropModelSetFromNameCB(const char* name);

	static ModelSetType GetModelSetType(int /*CDataFileMgr::DataFileType */ dataFileType);

	// Append the data
	void AppendModelSets(ModelSetType type, const char *fname);

	// Remove the data
	void RemoveModelSets(ModelSetType type, const char *fname);

	//Remove DLC data
	void ClearDLCData();
	struct sAMSSourceInfo
	{
		sAMSSourceInfo(){};
		sAMSSourceInfo(int _infoIndex,int _infoCount, const char* _source, ModelSetType _type, bool _isDLC=false)
			:infoStartIndex(_infoIndex),infoCount(_infoCount),source(_source),type(_type),isDLC(_isDLC){}
		int infoStartIndex;
		int infoCount;
		atFinalHashString source;
		bool isDLC;
		ModelSetType type;
	};
	atArray<sAMSSourceInfo> m_dAMSSourceInfo;

#if __BANK
	// Add widgets
	void AddWidgets(bkBank& bank);
	void AddWidgetsForModelSet(bkBank& bank, ModelSetType type);
#endif // __BANK

private:

	// Delete the data
	void ResetModelSets(ModelSetType type);

	static int /*CDataFileMgr::DataFileType*/ GetDataFileType(ModelSetType type);

	// Load the data
	void LoadModelSets(ModelSetType type);

	void ResetPropModelSets();

#if __BANK
	static void LoadPedModelSetsCB()		{ GetInstance().LoadModelSets(kPedModelSets); }
	static void LoadPropModelSetsCB()		{ GetInstance().LoadModelSets(kPropModelSets); }
	static void LoadVehicleModelSetsCB()	{ GetInstance().LoadModelSets(kVehicleModelSets); }
	static void SavePedModelSetsCB()		{ GetInstance().SaveModelSets(kPedModelSets); }
	static void SavePropModelSetsCB()		{ GetInstance().SaveModelSets(kPropModelSets); }
	static void SaveVehicleModelSetsCB()	{ GetInstance().SaveModelSets(kVehicleModelSets); }

	// Save the data
	void SaveModelSets(ModelSetType type);
#endif // __BANK

	//
	// Members
	//

	// The model set storage
	atRangeArray<CAmbientModelSets, kNumModelSetTypes> m_ModelSets;

	// Static manager object
	static CAmbientModelSetManager* sm_Instance;
};

#endif // INC_AMBIENT_MODEL_SET_MANAGER_H
