// FILE :    DefaultTaskData.h
// PURPOSE : Load in default task data set information
// CREATED : 26-09-2011

// class header
#include "DefaultTaskData.h"

// Framework headers

// Game headers

// Parser headers
#include "DefaultTaskData_parser.h"

// rage headers
#include "bank/bank.h"
#include "parser/manager.h"

AI_OPTIMISATIONS()

CompileTimeAssert(parser_DefaultTaskType_Count==MAX_DEFAULT_TASK_TYPES + 1); // +1 is for ourselves


//////////////////////////////////////////////////////////////////////////
// CDefaultTaskDataManager
//////////////////////////////////////////////////////////////////////////

// Static init
CDefaultTaskDataManager CDefaultTaskDataManager::ms_DefaultTaskDataManagerInstance;

// Constructor
CDefaultTaskDataManager::CDefaultTaskDataManager() :
	m_DefaultSet	(NULL)
{
}

void CDefaultTaskDataManager::Init(const char* pFileName)
{
	if(Verifyf(!ms_DefaultTaskDataManagerInstance.m_aDefaultTaskData.GetCount(), "Ped default task data sets have already been loaded!"))
	{
		// Clear out any existing info
		ms_DefaultTaskDataManagerInstance.m_aDefaultTaskData.Reset();

		// Load our data sets
		PARSER.LoadObject(pFileName, "meta", ms_DefaultTaskDataManagerInstance);
	}
}

void CDefaultTaskDataManager::Shutdown()
{
	// Clear out any existing info
	ms_DefaultTaskDataManagerInstance.m_aDefaultTaskData.Reset();
}

const CDefaultTaskDataSet* const CDefaultTaskDataManager::GetDataSet(const u32 dataSetHash)
{
	for (int i = 0; i < ms_DefaultTaskDataManagerInstance.m_aDefaultTaskData.GetCount(); i++)
	{
		if(ms_DefaultTaskDataManagerInstance.m_aDefaultTaskData[i] &&
		   ms_DefaultTaskDataManagerInstance.m_aDefaultTaskData[i]->GetName().GetHash() == dataSetHash)
		{
			return ms_DefaultTaskDataManagerInstance.m_aDefaultTaskData[i];
		}
	}

	return ms_DefaultTaskDataManagerInstance.m_DefaultSet;
}
