// File header
#include "AI/Ambient/AmbientModelSetManager.h"

// Rage headers
#include "ai/aichannel.h"
#include "bank/bank.h"
#include "fwsys/gameskeleton.h"
#include "parser/manager.h"
#include "system/nelem.h"

// Game headers
#include "scene/DataFileMgr.h"

// Parser
#include "AmbientModelSetManager_parser.h"

AI_OPTIMISATIONS()

class AmbientModelSetMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		CAmbientModelSetManager::GetInstance().AppendModelSets(CAmbientModelSetManager::GetModelSetType(file.m_fileType), file.m_filename);
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile &file) 
	{
		CAmbientModelSetManager::GetInstance().RemoveModelSets(CAmbientModelSetManager::GetModelSetType(file.m_fileType), file.m_filename);
	}

} g_AmbientModelSetMounter;

////////////////////////////////////////////////////////////////////////////////
// CAmbientModelSetManager
////////////////////////////////////////////////////////////////////////////////

// Static initialisation
CAmbientModelSetManager* CAmbientModelSetManager::sm_Instance = NULL;

void CAmbientModelSetManager::InitClass(u32 UNUSED_PARAM(uInitMode))
{
	aiAssert(!sm_Instance);

	CDataFileMount::RegisterMountInterface(CDataFileMgr::AMBIENT_PED_MODEL_SET_FILE, &g_AmbientModelSetMounter);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::AMBIENT_PROP_MODEL_SET_FILE, &g_AmbientModelSetMounter);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::AMBIENT_VEHICLE_MODEL_SET_FILE, &g_AmbientModelSetMounter);
	
	sm_Instance = rage_new CAmbientModelSetManager;

	// Load
	for(int i = 0; i < kNumModelSetTypes; i++)
	{
		sm_Instance->LoadModelSets((ModelSetType)i);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CAmbientModelSetManager::ShutdownClass(u32 uShutdownMode)
{
	switch(uShutdownMode)
	{
		case SHUTDOWN_WITH_MAP_LOADED:
			aiAssert(sm_Instance);
			delete sm_Instance;
			sm_Instance = NULL;
		break;
		case SHUTDOWN_SESSION:
			aiAssert(sm_Instance);
			sm_Instance->ClearDLCData();
		default:
		break;
	}
	
}

////////////////////////////////////////////////////////////////////////////////ie

const char* CAmbientModelSetManager::GetNameFromModelSet(const CAmbientModelSet* NOTFINAL_ONLY(pModelSet))
{
#if !__FINAL
	if(pModelSet)
	{
		return pModelSet->GetName();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CAmbientPedModelSet* CAmbientModelSetManager::GetPedModelSetFromNameCB(const char* name)
{
	CAmbientModelSetManager& mgr = GetInstance();
	int index = mgr.GetModelSetIndexFromHash(kPedModelSets, atStringHash(name));
	if(aiVerifyf(index >= 0, "Model set [%s] doesn't exist in data", name))
	{
		return mgr.GetPedModelSet(index);
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CAmbientModelSet* CAmbientModelSetManager::GetVehicleModelSetFromNameCB(const char* name)
{
	CAmbientModelSetManager& mgr = GetInstance();
	int index = mgr.GetModelSetIndexFromHash(kVehicleModelSets, atStringHash(name));
	if(aiVerifyf(index >= 0, "Vehicle model set [%s] doesn't exist in data", name))
	{
		return &mgr.GetModelSet(kVehicleModelSets, index);
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CAmbientModelSet* CAmbientModelSetManager::GetPropModelSetFromNameCB(const char* name)
{
	CAmbientModelSetManager& mgr = GetInstance();
	int index = mgr.GetModelSetIndexFromHash(kPropModelSets, atStringHash(name));
	if (aiVerifyf(index >= 0, "Prop model set [%s] doesn't exist in data.", name))
	{
		return &mgr.GetModelSet(kPropModelSets, index);
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK

// Add widgets
void CAmbientModelSetManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Ambient Model Sets");

	bank.PushGroup("Ped Model Sets", false);
	bank.AddButton("Load", LoadPedModelSetsCB);
	bank.AddButton("Save", SavePedModelSetsCB);
	AddWidgetsForModelSet(bank, kPedModelSets);
	bank.PopGroup();	// Ped Model Sets

	bank.PushGroup("Prop Model Sets", false);
	bank.AddButton("Load", LoadPropModelSetsCB);
	bank.AddButton("Save", SavePropModelSetsCB);
	AddWidgetsForModelSet(bank, kPropModelSets);
	bank.PopGroup();	// Prop Model Sets

	bank.PushGroup("Vehicle Model Sets", false);
	bank.AddButton("Load", LoadVehicleModelSetsCB);
	bank.AddButton("Save", SaveVehicleModelSetsCB);
	AddWidgetsForModelSet(bank, kVehicleModelSets);
	bank.PopGroup();	// Vehicle Model Sets

	bank.PopGroup();	// Ambient Model Sets
}


void CAmbientModelSetManager::AddWidgetsForModelSet(bkBank& bank, ModelSetType type)
{
	bank.PushGroup("Sets");
	for(s32 i = 0; i < m_ModelSets[type].m_ModelSets.GetCount(); i++)
	{
		bank.PushGroup(m_ModelSets[type].m_ModelSets[i]->GetName());
		PARSER.AddWidgets(bank, m_ModelSets[type].m_ModelSets[i]);
		bank.PopGroup();
	}
	bank.PopGroup();
}

#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

void CAmbientModelSetManager::ResetModelSets(ModelSetType type)
{
	for(int i=0;i<m_ModelSets[type].m_ModelSets.GetCount();i++)
	{
		delete m_ModelSets[type].m_ModelSets[i];
	}
	m_ModelSets[type].m_ModelSets.Reset();
}

////////////////////////////////////////////////////////////////////////////////

static const CDataFileMgr::DataFileType s_FileTypes[] =
{
	CDataFileMgr::AMBIENT_PED_MODEL_SET_FILE,
	CDataFileMgr::AMBIENT_PROP_MODEL_SET_FILE,
	CDataFileMgr::AMBIENT_VEHICLE_MODEL_SET_FILE
};

int CAmbientModelSetManager::GetDataFileType(ModelSetType type)
{
	CompileTimeAssert(NELEM(s_FileTypes) == kNumModelSetTypes);
	aiAssert(type >= 0 && type < kNumModelSetTypes);
	return s_FileTypes[type];
}

CAmbientModelSetManager::ModelSetType CAmbientModelSetManager::GetModelSetType(int /*CDataFileMgr::DataFileType */ dataFileType)
{
	CompileTimeAssert(NELEM(s_FileTypes) == kNumModelSetTypes);

	for(int i = 0; i < NELEM(s_FileTypes); i++)
	{
		if(s_FileTypes[i] == dataFileType)
			return (ModelSetType)i;
	}

	return kNumModelSetTypes;
}

////////////////////////////////////////////////////////////////////////////////

void CAmbientModelSetManager::LoadModelSets(ModelSetType type)
{
	// Delete any existing data
	ResetModelSets(type);
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile((CDataFileMgr::DataFileType)GetDataFileType(type));
	if(DATAFILEMGR.IsValid(pData))
	{
		// Load
		PARSER.LoadObject(pData->m_filename, "meta", m_ModelSets[type]);
		m_dAMSSourceInfo.PushAndGrow(sAMSSourceInfo(0,m_ModelSets[type].m_ModelSets.GetCount(),pData->m_filename,type));
	}
}

void CAmbientModelSetManager::ClearDLCData()
{
	for (int i=0; i<m_dAMSSourceInfo.GetCount();i++)
	{
		sAMSSourceInfo& source = m_dAMSSourceInfo[i];
		if(source.isDLC)
		{
			int toDelete = source.infoCount;
			for (int j=0;j<toDelete;j++)
			{
				m_ModelSets[source.type].m_ModelSets.Delete(source.infoStartIndex+j--);
				toDelete--;				
			}
			m_dAMSSourceInfo.Delete(i--);
		}
	}
}

void CAmbientModelSetManager::AppendModelSets(ModelSetType type, const char *fname)
{	
	CAmbientModelSets* tempModelSets = rage_new CAmbientModelSets;

	if(PARSER.LoadObject(fname, "meta", *tempModelSets))
	{
		m_dAMSSourceInfo.PushAndGrow(sAMSSourceInfo(m_ModelSets[type].m_ModelSets.GetCount(),tempModelSets->m_ModelSets.GetCount(),fname,type,true));
		m_ModelSets[type].Append(*tempModelSets);
	}
}

// Remove the data
void CAmbientModelSetManager::RemoveModelSets(ModelSetType type, const char *fname)
{
	CAmbientModelSets tempModelSets;

	if(PARSER.LoadObject(fname, "meta", tempModelSets))
	{
		m_ModelSets[type].Remove(tempModelSets);
	}
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK

void CAmbientModelSetManager::SaveModelSets(ModelSetType type)
{
	int curIndex=0;
	for(int i=0;i<m_dAMSSourceInfo.GetCount();i++)
	{
		sAMSSourceInfo& currentSourceInfo = m_dAMSSourceInfo[i];
		if(currentSourceInfo.type==type)
		{
			CAmbientModelSets tempData;
			for(int j=curIndex;j<curIndex+currentSourceInfo.infoCount;j++)
			{
				tempData.m_ModelSets.PushAndGrow(m_ModelSets[type].m_ModelSets[j]);
			}
			curIndex += currentSourceInfo.infoCount;
			char path[RAGE_MAX_PATH]={0};
			fiDevice::GetDevice(currentSourceInfo.source.TryGetCStr())->FixRelativeName(path,RAGE_MAX_PATH,currentSourceInfo.source.TryGetCStr());
			Verifyf(PARSER.SaveObject(path, "", &tempData, parManager::XML), "File is probably write protected");
			for(int j=0;j<tempData.m_ModelSets.GetCount();j++)
			{
				tempData.m_ModelSets[j]=NULL;
			}
		}
	}
}

#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

int CAmbientModelSetManager::GetModelSetIndexFromHash(ModelSetType type, u32 hash) const
{
	const int* pIndexPtr = m_ModelSets[type].m_ModelSetIndexCache.SafeGet(hash);
	if( pIndexPtr )
	{
		return (*pIndexPtr);
	}
	return -1;
}
