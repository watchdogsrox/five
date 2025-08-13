// Class header
#include "Task/Scenario/Info/ScenarioInfoManager.h"

// Rage headers
#include "bank/bkmgr.h"
#include "entity/archetypemanager.h"
#include "parser/manager.h"
#include "file/device.h"

// Game headers
#include "AI/Ambient/AmbientModelSetManager.h"
#include "AI/Ambient/ConditionalAnimManager.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Scene/2dEffect.h"
#include "task/Default/TaskAmbient.h"
#include "task/Scenario/ScenarioPoint.h"
#include "task/Scenario/ScenarioPointManager.h"
#include "task/Scenario/Types/TaskUseScenario.h"
#include "Task/Scenario/Info/ScenarioInfoConditions.h"
#include "vehicleAi/task/TaskVehiclePark.h"

// Parser files
#include "ScenarioInfoConditions_parser.h"
#include "ScenarioInfo_parser.h"
#include "ScenarioInfoManager_parser.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()

class ScenarioInfoMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		switch(file.m_fileType)
		{
			case CDataFileMgr::SCENARIO_INFO_OVERRIDE_FILE:
			SCENARIOINFOMGR.KillExistingScenarios();
			SCENARIOINFOMGR.HandleDLCData(file.m_filename);
			break;
			case CDataFileMgr::SCENARIO_INFO_FILE:
			SCENARIOINFOMGR.HandleDLCData(file.m_filename);
			default:
			break;
		}
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile &file) 
	{
		SCENARIOINFOMGR.Remove(file.m_filename);
	}

} g_ScenarioInfoMounter;

////////////////////////////////////////////////////////////////////////////////
// CScenarioInfoManager
////////////////////////////////////////////////////////////////////////////////

CScenarioInfoManager* CScenarioInfoManager::sm_ManagerBeingLoaded = NULL;

////////////////////////////////////////////////////////////////////////////////

CScenarioInfoManager::CScenarioInfoManager()
{

}

void CScenarioInfoManager::Init()
{
	// Initialise pools
	// Load
#if __BANK
	m_ScenarioTypesEnabledWidgetGroup = NULL;
#endif
	CScenarioInfo::InitPool(MEMBUCKET_GAMEPLAY);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::SCENARIO_INFO_FILE, &g_ScenarioInfoMounter);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::SCENARIO_INFO_OVERRIDE_FILE, &g_ScenarioInfoMounter);
#if SCENARIO_DEBUG

	ClearPedUsedScenarios();
	atArray<u32> indices(0, m_Scenarios.GetCount());

	// As the 2dfx store the index into the array for the relevant scenario,
	// we have to update them in case the order has changed
	for(s32 i = 0; i < m_Scenarios.GetCount(); i++)
	{
		indices.Push(m_Scenarios[i]->GetHash());
	}

	ClearScenarioTypesWidgets();
#endif // SCENARIO_DEBUG	
	Load();	
	PostLoad(SCENARIO_DEBUG_ONLY(indices , false));
}


void CScenarioInfoManager::Shutdown()
{
	// Delete the data
	Reset();

	// Shutdown pools
	CScenarioInfo::ShutdownPool();
}

void CScenarioInfoManager::ClearDLCScenarioInfos(atHashString fileHash /*=atHashString::Null()*/)
{
	for(int i=0;i<m_Scenarios.GetCount();i++)
	{
		m_Scenarios[i]->SetIsEnabled(true);
	}
	for(int i=0;i<m_dScenarioSourceInfo.GetCount();i++)
	{
		sScenarioSourceInfo& current = m_dScenarioSourceInfo[i];
		if(current.isDLC && fileHash.IsNull() ? true : current.source == fileHash )
		{
			int toDelete = current.infoCount;
			for (int j=0;j<toDelete;j++)
			{
				delete m_Scenarios[current.infoStartIndex+j];
				m_Scenarios.Delete(current.infoStartIndex+j--);
				toDelete--;
			}
			toDelete = current.groupCount;
			for (int j=0;j<toDelete;j++)
			{
				delete m_ScenarioTypeGroups[current.groupStartIndex+j];
				m_ScenarioTypeGroups.Delete(current.groupStartIndex+j--);
				toDelete--;
			}
			if(i+1<m_dScenarioSourceInfo.GetCount())
			{
				m_dScenarioSourceInfo[i+1].infoStartIndex-=current.infoCount;
				m_dScenarioSourceInfo[i+1].groupStartIndex-=current.groupCount;
			}
			m_dScenarioSourceInfo.Delete(i);
		}		
	}
}

////////////////////////////////////////////////////////////////////////////////

CScenarioInfoManager::~CScenarioInfoManager()
{

}

////////////////////////////////////////////////////////////////////////////////
void CScenarioInfoManager::KillExistingScenarios()
{
	for(int i=0;i<m_Scenarios.GetCount();i++)
	{
		//m_Scenarios[i]->SetSpawnProbability(0);
		m_ScenarioTypesEnabled[i] = false;
		m_Scenarios[i]->SetIsEnabled(false);
	}
#if SCENARIO_DEBUG
	UpdateScenarioTypesWidgets();
#endif // SCENARIO_DEBUG
}

void CScenarioInfoManager::CheckUniqueConditionalAnimNames()
{
#if !__FINAL
	bool foundProblems = false;

	const char* sep = "-------------------------";

	const int numScenarios = m_Scenarios.GetCount();
	for(int i = 0; i < numScenarios; i++)
	{
		const CScenarioInfo* infoA = m_Scenarios[i];
		if(infoA)
		{
			const CConditionalAnimsGroup& grpA = infoA->GetConditionalAnimsGroup();

			// Only allow unnamed conditional animation groups if they are empty,
			// hopefully it should be safe in that case.
			if(grpA.GetHash() == 0 && grpA.GetNumAnims() > 0)
			{
				if(!foundProblems)
				{
					foundProblems = true;
					Displayf("%s", sep);
				}

				Displayf("Scenario '%s' has a conditional animation group without a name, but with %d animations.",
							infoA->GetName(), grpA.GetNumAnims());
			}

			for(int j = i + 1; j < numScenarios; j++)
			{
				const CScenarioInfo* infoB = m_Scenarios[j];
				if(infoB)
				{
					const CConditionalAnimsGroup& grpB = infoB->GetConditionalAnimsGroup();
					if(grpA.GetHash() == grpB.GetHash() && grpA.GetHash() != 0)
					{
						if(!foundProblems)
						{
							foundProblems = true;
							Displayf("%s", sep);
						}
						Displayf("Scenario '%s' and '%s' have the same conditional animation group name, '%s'.",
								infoA->GetName(), infoB->GetName(), grpA.GetName());
					}
				}
			}
		}
	}

	const CConditionalAnimManager& mgr = CONDITIONALANIMSMGR;
	const int numCondAnimGroups = mgr.GetNumConditionalAnimsGroups();
	for(int i = 0; i < numCondAnimGroups; i++)
	{
		const CConditionalAnimsGroup& grpA = mgr.GetConditionalAnimsGroupByIndex(i);

		if(grpA.GetHash() == 0)
		{
			if(!foundProblems)
			{
				foundProblems = true;
				Displayf("%s", sep);
			}
			Displayf("Conditional animation group %d in 'conditionalanims.meta' is missing a name.", i);
		}

		for(int j = i + 1; j < numCondAnimGroups; j++)
		{
			const CConditionalAnimsGroup& grpB = mgr.GetConditionalAnimsGroupByIndex(j);
			if(grpA.GetHash() == grpB.GetHash() && grpA.GetHash() != 0)
			{
				if(!foundProblems)
				{
					foundProblems = true;
					Displayf("%s", sep);
				}
				Displayf("Conditional animation group '%s' used twice (in 'conditionalanims.meta')", grpA.GetName());
			}
		}
	}

	for(int i = 0; i < numScenarios; i++)
	{
		const CScenarioInfo* infoA = m_Scenarios[i];
		if(infoA)
		{
			const CConditionalAnimsGroup& grpA = infoA->GetConditionalAnimsGroup();
			for(int j = 0; j < numCondAnimGroups; j++)
			{
				const CConditionalAnimsGroup& grpB = mgr.GetConditionalAnimsGroupByIndex(j);
				if(grpA.GetHash() == grpB.GetHash() && grpA.GetHash() != 0)
				{
					if(!foundProblems)
					{
						foundProblems = true;
						Displayf("%s", sep);
					}
					Displayf("Conditional animation group name '%s' is used for scenario '%s', but is also used in 'conditionalanims.meta'",
							grpA.GetName(), infoA->GetName());
				}
			}
		}
	}

	if(foundProblems)
	{
		Displayf("%s", sep);
	}

#if __ASSERT
	taskAssertf(!foundProblems, "Conditional animation group name problems were found, see output for details. This may cause serious problems in MP games, incl. crashes.");
#else
	if(foundProblems)
	{
		Errorf("Conditional animation group name problems were found, see output for details.  This may cause serious problems in MP games, incl. crashes.");
	}
#endif	// __ASSERT

#endif	// !__FINAL
}

////////////////////////////////////////////////////////////////////////////////

void CScenarioInfoManager::ResetScenarioTypesEnabledToDefaults()
{
	// Make sure the array has the right size, reallocate if needed.
	const int numTypes = m_Scenarios.GetCount();
	if(m_ScenarioTypesEnabled.GetCount() != numTypes)
	{
		m_ScenarioTypesEnabled.Reset();
		if(numTypes > 0)
		{
			m_ScenarioTypesEnabled.Reserve(numTypes);
			m_ScenarioTypesEnabled.Resize(numTypes);
		}
	}

	for(int i = 0; i < numTypes; i++)
	{
		// At this time, all scenario types are enabled by default, though
		// if needed, we could make this a property of CScenarioInfo.
		m_ScenarioTypesEnabled[i] = m_Scenarios[i]->IsEnabled();
	}
}

////////////////////////////////////////////////////////////////////////////////

const CScenarioInfo* CScenarioInfoManager::FromStringToScenarioInfoCB(const char* str)
{
	if(str && str[0])
	{
		atHashWithStringNotFinal hash(str);
		const CScenarioInfo* pInfo = sm_ManagerBeingLoaded->GetScenarioInfo(hash);
		taskAssertf(pInfo, "Scenario [%s] missing from data", str);
		return pInfo;
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const char* CScenarioInfoManager::ToStringFromScenarioInfoCB(const CScenarioInfo* NOTFINAL_ONLY(info))
{
#if !__FINAL
	if(info)
	{
		return info->GetName();
	}
#else
	// Note: hopefully nothing should call this in __FINAL builds, since this function
	// is used only as a parser callback for saving and for adding widgets, but if it
	// does get called, at least we'll know about it.
	Quitf(ERR_GEN_SCENARIO,"CScenarioInfoManager::ToStringFromScenarioInfoCB() called in a __FINAL build.");
#endif

	return "";
}

////////////////////////////////////////////////////////////////////////////////

void CScenarioInfoManager::ResetLastSpawnTimes()
{
	for(int i=0; i < m_Scenarios.GetCount(); i++)
	{
		m_Scenarios[i]->SetLastSpawnTime(0);
	}
}

////////////////////////////////////////////////////////////////////////////////

#if SCENARIO_DEBUG

void CScenarioInfoManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Scenarios Info");
	
	bank.AddButton("Load", datCallback(MFA(CScenarioInfoManager::BankLoad), this));
	bank.AddButton("Save", datCallback(MFA(CScenarioInfoManager::Save), this));
	
	AddParserWidgets(&bank);
	
	bank.PopGroup();

	// Create an empty group and store the pointer to it, so we can keep it
	// up to date if we reallocate the array (hit the "Load" button).
	m_ScenarioTypesEnabledWidgetGroup = bank.PushGroup("Scenario Types Enabled");
	bank.PopGroup();

	// Add to the group.
	ResetScenarioTypesEnabledToDefaults();
	UpdateScenarioTypesWidgets();
}


void CScenarioInfoManager::ClearScenarioTypesWidgets()
{
	if(!m_ScenarioTypesEnabledWidgetGroup)
	{
		return;
	}

	while(m_ScenarioTypesEnabledWidgetGroup->GetChild())
	{
		m_ScenarioTypesEnabledWidgetGroup->Remove(*m_ScenarioTypesEnabledWidgetGroup->GetChild());
	}
}


void CScenarioInfoManager::ClearPedUsedScenarios()
{
	for(int p=0; p < CPed::_ms_pPool->GetSize(); p++)
	{
		CPed * pPed = CPed::_ms_pPool->GetSlot(p);
		if(pPed)
		{
			if (pPed->GetPedIntelligence())
			{
				CTaskUseScenario* pScenarioTask = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
				if (pScenarioTask)
				{
					//Tell it to quit the task.
					pScenarioTask->MakeAbortable(aiTask::ABORT_PRIORITY_IMMEDIATE, NULL);
				}

				CTaskAmbientClips* pAmbientTask = static_cast<CTaskAmbientClips*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
				if (pAmbientTask)
				{
					//Tell it to quit the task.
					pAmbientTask->MakeAbortable(aiTask::ABORT_PRIORITY_IMMEDIATE, NULL);
					pAmbientTask->ResetConditionalAnimGroup();
				}
			}
		}
	}
}


void CScenarioInfoManager::UpdateScenarioTypesWidgets()
{
	ClearScenarioTypesWidgets();

	if(!m_ScenarioTypesEnabledWidgetGroup)
	{
		return;
	}

	m_ScenarioTypesEnabledWidgetGroup->AddTitle("Currently Enabled:");
	const int numTypes = m_ScenarioTypesEnabled.GetCount();
	taskAssert(GetScenarioCount(false) == numTypes);
	for(int i = 0; i < numTypes; i++)
	{
		const CScenarioInfo* pInfo = GetScenarioInfoByIndex(i);
		if(pInfo)
		{
			m_ScenarioTypesEnabledWidgetGroup->AddToggle(pInfo->GetName(), &m_ScenarioTypesEnabled[i]);
		}
	}
}

#endif // SCENARIO_DEBUG

////////////////////////////////////////////////////////////////////////////////

void CScenarioInfoManager::Reset()
{

	for(s32 i = 0; i < m_Scenarios.GetCount(); i++)
	{
		delete m_Scenarios[i];
	}
	for(s32 i = 0; i < m_ScenarioTypeGroups.GetCount(); i++)
	{
		delete m_ScenarioTypeGroups[i];
	}
	m_Scenarios.Reset();
	m_dScenarioSourceInfo.Reset();
#if __BANK
	bkWidget* pWidget = BANKMGR.FindWidget("Scenarios/Scenarios Info/Type Groups");
	if(pWidget)
	{
		pWidget->Destroy();
	}
#endif
	m_ScenarioTypeGroups.Reset();
	m_ScenarioTypesEnabled.Reset();
	m_ScenariosHashToIndexMap.Reset();
}

////////////////////////////////////////////////////////////////////////////////

void CScenarioInfoManager::Load()
{
	// Delete any existing data
	Reset();
	// The parser callbacks need to be able to find the object being loaded, and
	// we couldn't actually use SCENARIOINFOMGR - not only would that internally
	// restrict CScenarioInfoManager to be a singleton, but Load() is called
	// from the constructor before the singleton pointer has been set.
	taskAssert(!sm_ManagerBeingLoaded);
	sm_ManagerBeingLoaded = this;
	// Load
	PARSER.LoadObject("commoncrc:/data/ai/scenarios", "meta", *this);
	m_dScenarioSourceInfo.PushAndGrow(
		sScenarioSourceInfo(0,m_Scenarios.GetCount(),0,m_ScenarioTypeGroups.GetCount(),"commoncrc:/data/ai/scenarios.meta"));
	sm_ManagerBeingLoaded = NULL;
}
//////////////////////////////////////////////////////////////////////////

void CScenarioInfoManager::PostLoad(SCENARIO_DEBUG_ONLY(atArray<u32>& indices, bool dlc))
{
	// When loading using the parser, callbacks sets the CScenarioInfo pointers
	// in the type groups, but nothing sets the corresponding indices, so we have
	// to fix those up ourselves.
	for(int i = 0; i < m_ScenarioTypeGroups.GetCount(); i++)
	{
		const int numReal = m_ScenarioTypeGroups[i]->GetNumTypes();
		for(int j = 0; j < numReal; j++)
		{
			if(m_ScenarioTypeGroups[i]->GetType(j).m_ScenarioType)
			{
				m_ScenarioTypeGroups[i]->GetType(j).m_ScenarioTypeIndex = GetScenarioIndex(m_ScenarioTypeGroups[i]->GetType(j).m_ScenarioType->GetHash());
			}
		}
	}

	ResetScenarioTypesEnabledToDefaults();

#if __ASSERT
	for(int i = 0; i < m_Scenarios.GetCount() - 1; i++)
	{
		for(int j = i + 1; j < m_Scenarios.GetCount(); j++)
		{
			taskAssertf(m_Scenarios[i]->GetHash() != m_Scenarios[j]->GetHash(),
				"Duplicate scenario info (or hash collision): %s == %s.",
				m_Scenarios[i]->GetName(), m_Scenarios[j]->GetName());
		}
	}

	Assertf(m_Scenarios.GetCount() < MAX_SCENARIO_TYPES, "The number of scenario types %d is greater than the maximum %d", m_Scenarios.GetCount(), MAX_SCENARIO_TYPES);
#endif	// __ASSERT
#if SCENARIO_DEBUG
	// Update the 2dfx spawn points
	if(!dlc)
	{
		fwArchetypeExtensionFactory<CSpawnPoint>& sps = CModelInfo::GetSpawnPointStore();
		for(s32 i = 0; i < sps.GetCount(fwFactory::GLOBAL); i++)
		{
			CSpawnPoint* sp = sps.GetEntry(fwFactory::GLOBAL,i).GetSpawnPoint();
			if(sp)
			{
				sp->iType = (unsigned)GetScenarioIndex(indices[sp->iType], true, true);
			}
		}

		if (CScenarioPointManagerSingleton::IsInstantiated())
		{
			SCENARIOPOINTMGR.UpdateScenarioTypeIds(indices);
		}
	}

	// Update the widgets
	ParserPostLoad();
	AddParserWidgets(NULL);
	UpdateScenarioTypesWidgets();
#endif // SCENARIO_DEBUG
}
void CScenarioInfoManager::HandleDLCData(const char* fileName)
{
#if SCENARIO_DEBUG

	ClearPedUsedScenarios();
	ClearScenarioTypesWidgets();
#endif // SCENARIO_DEBUG	

	Append(fileName);
#if SCENARIO_DEBUG
	atArray<u32> indices(0, m_Scenarios.GetCount());

	// As the 2dfx store the index into the array for the relevant scenario,
	// we have to update them in case the order has changed
	for(s32 i = 0; i < m_Scenarios.GetCount(); i++)
	{
		indices.Push(m_Scenarios[i]->GetHash());
	}
#endif // SCENARIO_DEBUG
	ResetScenarioTypesEnabledToDefaults();
	PostLoad(SCENARIO_DEBUG_ONLY(indices,true));
}
void CScenarioInfoManager::Append(const char* fileName)
{
	CScenarioInfoManager toAppend;
	if(PARSER.LoadObject(fileName,NULL,toAppend))
	{
		for(int i=0;i<toAppend.m_Scenarios.GetCount();++i)
		{
			m_Scenarios.PushAndGrow(toAppend.m_Scenarios[i]);
			toAppend.m_Scenarios[i]=NULL;
		}
		for(int i=0;i<toAppend.m_ScenarioTypeGroups.GetCount();++i)
		{
			m_ScenarioTypeGroups.PushAndGrow(toAppend.m_ScenarioTypeGroups[i]);
			toAppend.m_ScenarioTypeGroups[i]=NULL;
		}
	}
	int scenarioCount = toAppend.m_Scenarios.GetCount();
	int groupCount = toAppend.m_ScenarioTypeGroups.GetCount();
	m_dScenarioSourceInfo.PushAndGrow(
		sScenarioSourceInfo(m_Scenarios.GetCount()-scenarioCount,scenarioCount,
							m_ScenarioTypeGroups.GetCount()-groupCount,groupCount,fileName,true));


}
void CScenarioInfoManager::Remove(const char* filename)
{
	ClearDLCScenarioInfos(filename);
}
s32 CScenarioInfoManager::GetScenarioIndex(atHashWithStringNotFinal hash, bool ASSERT_ONLY(assertIfNotFound), bool includeVirtual) const
{
	// If possible, use the index cache
	if( m_ScenariosHashToIndexMap.GetCount() > 0 )
	{
		const u32 hashValue = hash.GetHash();
		const int* pIndexPtr = m_ScenariosHashToIndexMap.SafeGet(hashValue);
		if( pIndexPtr )
		{
			return (*pIndexPtr);
		}
	}
	else
	{
		// linear traversal should only occur during loading process
		// before the index cache is ready
		for(s32 i = 0; i < m_Scenarios.GetCount(); i++)
		{
			if(m_Scenarios[i]->GetHash() == hash.GetHash())
			{
				
				return i;
			}
		}
	}

	if(includeVirtual)
	{
		const int numReal = m_Scenarios.GetCount();
		const int numGroups = m_ScenarioTypeGroups.GetCount();
		for(int i = 0; i < numGroups; i++) // this is OK as long as numGroups remains small (currently 3)
		{
			if(m_ScenarioTypeGroups[i]->GetHash() == hash.GetHash())
			{
				return i + numReal;
			}
		}
	}
	Displayf( "ScenarioInfo with hash [%s][%x] not found", hash.TryGetCStr() ? hash.TryGetCStr() : "?", hash.GetHash());
	Assertf(!assertIfNotFound, "ScenarioInfo with hash [%s][%x] not found", hash.TryGetCStr() ? hash.TryGetCStr() : "?", hash.GetHash());
	return -1;
}

////////////////////////////////////////////////////////////////////////////////

void CScenarioInfoManager::ParserPostLoad()
{
	// In case this is re-loaded, clear out the cache map
	if( m_ScenariosHashToIndexMap.GetCount() > 0 )
	{
		m_ScenariosHashToIndexMap.Reset();
	}

	// Reserve the map memory that will be filled in
	m_ScenariosHashToIndexMap.Reserve(m_Scenarios.GetCount());

	// Fill out the hash to index cache map
	for(int i=0; i < m_Scenarios.GetCount(); i++)
	{
		const u32 hashKey = m_Scenarios[i]->GetHash();
		const int indexToStore = i;
		m_ScenariosHashToIndexMap.Insert(hashKey, indexToStore);

		// [HACK GTAV] Make PROP_HUMAN_SEAT_CHAIR_MP_PLAYER never spawn
		// peds, so we don't have to rely on the NoSpawn flag or blocking areas.
		// B* 1656004: "[Public Reported][PUBLIC]0325 South Rockford Dr apartment has NPCs spawn inside the couch."
		if(hashKey == ATSTRINGHASH("PROP_HUMAN_SEAT_CHAIR_MP_PLAYER", 0x4f49b85))
		{
			m_Scenarios[i]->SetSpawnProbability(0.0f);
		}
	}

	// Prep the map for use
	// NOTE: It is safe to call this if nothing was inserted
	m_ScenariosHashToIndexMap.FinishInsertion();
}

////////////////////////////////////////////////////////////////////////////////

#if SCENARIO_DEBUG
void CScenarioInfoManager::BankLoad()
{
	ClearPedUsedScenarios();
	// As the 2dfx store the index into the array for the relevant scenario,
	// we have to update them in case the order has changed
	atArray<u32> indices(0, m_Scenarios.GetCount()+m_ScenarioTypeGroups.GetCount());
	for(s32 i = 0; i < m_Scenarios.GetCount(); i++)
	{
		indices.Push(m_Scenarios[i]->GetHash());
	}
	for(s32 i = 0; i < m_ScenarioTypeGroups.GetCount(); i++)
	{
		indices.Push(m_ScenarioTypeGroups[i]->GetHash());
	}
	ClearScenarioTypesWidgets();
	atArray<atFinalHashString> dlcSources;

	for(int i=0;i<m_dScenarioSourceInfo.GetCount();i++)
	{
		if(m_dScenarioSourceInfo[i].isDLC)
		{
			dlcSources.PushAndGrow(m_dScenarioSourceInfo[i].source);
		}
	}
	Load();

	for(int i=0;i<dlcSources.GetCount();i++)
	{
		Append(dlcSources[i].TryGetCStr());
	}
	ParserPostLoad();
	PostLoad(BANK_ONLY(indices , false));
}
void CScenarioInfoManager::Save()
{
	int curScenarioIndex = 0;
	int curGroupIndex=0;
	for(int i=0;i<m_dScenarioSourceInfo.GetCount();i++)
	{
		sScenarioSourceInfo& currentSourceInfo = m_dScenarioSourceInfo[i];
		CScenarioInfoManager tempData;
		for(int j=curScenarioIndex;j<curScenarioIndex+currentSourceInfo.infoCount;j++)
		{
			tempData.m_Scenarios.PushAndGrow(m_Scenarios[j]);
		}
		curScenarioIndex += currentSourceInfo.infoCount;
		for(int j=curGroupIndex;j<curGroupIndex+currentSourceInfo.groupCount;j++)
		{
			tempData.m_ScenarioTypeGroups.PushAndGrow(m_ScenarioTypeGroups[j]);
		}
		curGroupIndex += currentSourceInfo.groupCount;

		char path[RAGE_MAX_PATH]={0};
		fiDevice::GetDevice(m_dScenarioSourceInfo[i].source.TryGetCStr())->FixRelativeName(path,RAGE_MAX_PATH,m_dScenarioSourceInfo[i].source.TryGetCStr());
		Verifyf(PARSER.SaveObject(path, "", &tempData, parManager::XML), "File is probably write protected");
 		for(int j=0;j<tempData.m_Scenarios.GetCount();j++)
 		{
 			tempData.m_Scenarios[j]=NULL;
 		}
 		for(int j=0;j<tempData.m_ScenarioTypeGroups.GetCount();j++)
 		{
 			tempData.m_ScenarioTypeGroups[j]=NULL;
 		}
	}
}
#endif // SCENARIO_DEBUG

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CScenarioInfoManager::AddParserWidgets(bkBank* pBank)
{
	bkGroup* pGroup = NULL;
	if(!pBank)
	{
		pBank = BANKMGR.FindBank("Scenarios");
		if(!pBank)
		{
			return;
		}

		bkWidget* pWidget = BANKMGR.FindWidget("Scenarios/Scenarios Info/Infos");
		if(!pWidget)
		{
			return;
		}

		pWidget->Destroy();

		pWidget = BANKMGR.FindWidget("Scenarios/Scenarios Info");
		if(pWidget)
		{
			pGroup = dynamic_cast<bkGroup*>(pWidget);
			if(pGroup)
			{
				pBank->SetCurrentGroup(*pGroup);
			}
		}
	}

	pBank->PushGroup("Infos");
	for(s32 i = 0; i < m_Scenarios.GetCount(); i++)
	{
		pBank->PushGroup(m_Scenarios[i]->GetName());
		PARSER.AddWidgets(*pBank, m_Scenarios[i]);
		pBank->PopGroup();
	}
	pBank->PopGroup();

	pBank->PushGroup("Type Groups");
	for(int i = 0; i < m_ScenarioTypeGroups.GetCount(); i++)
	{
		CScenarioTypeGroup& e = *m_ScenarioTypeGroups[i];
		pBank->PushGroup(e.GetName());
		PARSER.AddWidgets(*pBank, &e);
		pBank->PopGroup();
	}
	pBank->PopGroup();

	if(pGroup)
	{
		pBank->UnSetCurrentGroup(*pGroup);
	}
}
#endif	// __BANK
