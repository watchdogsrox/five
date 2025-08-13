// FILE :    TaskData.cpp
// PURPOSE : Load in ped ik info
// CREATED : 10-06-2011

// class header
#include "TaskData.h"

// Framework headers

// Game headers

// Parser headers
#include "TaskData_parser.h"

// rage headers
#include "bank/bank.h"
#include "parser/manager.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskDataInfoManager
//////////////////////////////////////////////////////////////////////////

// Static Manager object
CTaskDataInfoManager CTaskDataInfoManager::m_TaskDataManagerInstance;

CTaskDataInfoManager::CTaskDataInfoManager() :
m_DefaultSet	(NULL)
{
}

void CTaskDataInfoManager::Init(const char* pFileName)
{
	Load(pFileName);
}

void CTaskDataInfoManager::AppendExtra(const char* pFileName)
{
	CTaskDataInfoManager tempInst;

	if(Verifyf(PARSER.LoadObject(pFileName, "meta", tempInst), "CTaskDataInfoManager::AppendExtra can't load %s", pFileName))
	{
		for(int i = 0; i < tempInst.m_aTaskData.GetCount(); i++)
		{
			CTaskDataInfo *pNewInfo = tempInst.m_aTaskData[i];

			if(GetInfo(pNewInfo->GetHash(), false) == NULL)
			{
				m_TaskDataManagerInstance.m_aTaskData.PushAndGrow(pNewInfo, 1);
			}
			else
			{
				Assertf(false, "CTaskDataInfoManager::AppendExtra entry with name %s already exists! We can't override entries with DLC", pNewInfo->GetName()); 
			}
		}				
	}
}

void CTaskDataInfoManager::Shutdown()
{
	// Delete
	Reset();
}


void CTaskDataInfoManager::Load(const char* pFileName)
{
	Printf("+=====================================\n");
	if(Verifyf(!m_TaskDataManagerInstance.m_aTaskData.GetCount(), "Task Flags info has already been loaded"))
	{
		// Reset/Delete the existing data before loading
		Reset();

		// Load
		Printf("+=====================================\n");
		PARSER.LoadObject(pFileName, "meta", m_TaskDataManagerInstance);
	}
}


void CTaskDataInfoManager::Reset()
{
	for(int i = 0; i < m_TaskDataManagerInstance.m_aTaskData.GetCount(); i++)
	{
		delete m_TaskDataManagerInstance.m_aTaskData[i];
	}

	m_TaskDataManagerInstance.m_aTaskData.Reset();
}


#if __BANK
void CTaskDataInfoManager::AddWidgets(bkBank& bank)
{
	bank.AddButton("Save", Save);

	for (s32 i = 0; i < m_TaskDataManagerInstance.m_aTaskData.GetCount(); i++)
	{
		bank.PushGroup(m_TaskDataManagerInstance.m_aTaskData[i]->GetName() );
		PARSER.AddWidgets(bank, m_TaskDataManagerInstance.m_aTaskData[i] );
		bank.PopGroup(); 
	}
}


void CTaskDataInfoManager::Save()
{
	Verifyf(PARSER.SaveObject("common:/data/TaskData", "meta", &m_TaskDataManagerInstance, parManager::XML), "Failed to save task data settings");
}
#endif // __BANK
