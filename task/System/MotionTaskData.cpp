// FILE :    MotionTaskData.h
// PURPOSE : Load in move blender data set information
// CREATED : 12-8-2010

// class header
#include "MotionTaskData.h"

// Framework headers

// Game headers

// Parser headers
#include "MotionTaskData_parser.h"

// rage headers
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "parser/manager.h"

AI_OPTIMISATIONS()

CompileTimeAssert(parser_MotionTaskType_Count==MAX_MOTION_TASK_TYPES + 1); // +1 is for ourselves


//////////////////////////////////////////////////////////////////////////
// CMotionTaskDataManager
//////////////////////////////////////////////////////////////////////////

// Static init
CMotionTaskDataManager CMotionTaskDataManager::ms_MotionTaskDataManagerInstance;

// Constructor
CMotionTaskDataManager::CMotionTaskDataManager()
{
}

void CMotionTaskDataManager::Init(const char* pFileName)
{
	if(Verifyf(!ms_MotionTaskDataManagerInstance.m_aMotionTaskData.GetCount(), "Ped motion task data sets have already been loaded!"))
	{
		// Clear out any existing info
		ms_MotionTaskDataManagerInstance.m_aMotionTaskData.Reset();

		// Load our data sets
		PARSER.LoadObject(pFileName, "meta", ms_MotionTaskDataManagerInstance);
	}
}

void CMotionTaskDataManager::Shutdown()
{
	// Clear out any existing info
	ms_MotionTaskDataManagerInstance.m_aMotionTaskData.Reset();
}

const CMotionTaskDataSet* const CMotionTaskDataManager::GetDataSet(const u32 dataSetHash)
{
	for(int i = 0; i < ms_MotionTaskDataManagerInstance.m_aMotionTaskData.GetCount(); i++)
	{
		if(ms_MotionTaskDataManagerInstance.m_aMotionTaskData[i] &&
		   ms_MotionTaskDataManagerInstance.m_aMotionTaskData[i]->GetName().GetHash() == dataSetHash)
		{
			return ms_MotionTaskDataManagerInstance.m_aMotionTaskData[i];
		}
	}

	return NULL;
}

#if __BANK
void CMotionTaskDataManager::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");
	if (Verifyf(pBank, "Unable to find the A.I. bank in RAG."))
	{
		// Add the health config bank.
		pBank->PushGroup("Motion Task Data", false);

		// Add the widgets.
		pBank->AddButton("Save", Save);
		for (s32 i = 0; i < ms_MotionTaskDataManagerInstance.m_aMotionTaskData.GetCount(); i++)
		{
			pBank->PushGroup(ms_MotionTaskDataManagerInstance.m_aMotionTaskData[i]->GetName().GetCStr());
			PARSER.AddWidgets(*pBank, ms_MotionTaskDataManagerInstance.m_aMotionTaskData[i]);
			pBank->PopGroup(); 
		}

		pBank->PopGroup();
	}
}


void CMotionTaskDataManager::Save()
{
	Verifyf(PARSER.SaveObject("common:/data/motiontaskdata", "meta", &ms_MotionTaskDataManagerInstance, parManager::XML), "Failed to save MotionTaskData");
}
#endif // __BANK
