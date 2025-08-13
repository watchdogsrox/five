// FILE :    HealthConfig.cpp
// PURPOSE : Load in configurable health information for varying types of peds
// CREATED : 10-06-2011

// class header
#include "HealthConfig.h"

// Framework headers

// Game headers

// Parser headers
#include "HealthConfig_parser.h"

// rage headers
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "parser/manager.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CHealthConfigInfoManager
//////////////////////////////////////////////////////////////////////////

// Static Manager object
CHealthConfigInfoManager CHealthConfigInfoManager::m_HealthConfigManagerInstance;

CHealthConfigInfoManager::CHealthConfigInfoManager() :
	m_DefaultSet	(NULL)
{
}

void CHealthConfigInfoManager::Init(const char* pFileName)
{
	Load(pFileName);
}


void CHealthConfigInfoManager::Shutdown()
{
	// Delete
	Reset();
}


void CHealthConfigInfoManager::Load(const char* pFileName)
{
	if(Verifyf(!m_HealthConfigManagerInstance.m_aHealthConfig.GetCount(), "Health Config Info has already been loaded"))
	{
		// Reset/Delete the existing data before loading
		Reset();

		// Load
		PARSER.LoadObject(pFileName, "meta", m_HealthConfigManagerInstance);
	}
}


void CHealthConfigInfoManager::Reset()
{
	m_HealthConfigManagerInstance.m_aHealthConfig.Reset();
}


#if __BANK
void CHealthConfigInfoManager::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");
	if (Verifyf(pBank, "Unable to find the A.I. bank in RAG."))
	{
		// Add the health config bank.
		pBank->PushGroup("Health Config", false);
		
		// Add the widgets.
		pBank->AddButton("Save", Save);
		for (s32 i = 0; i < m_HealthConfigManagerInstance.m_aHealthConfig.GetCount(); i++)
		{
			pBank->PushGroup(m_HealthConfigManagerInstance.m_aHealthConfig[i].GetName() );
			PARSER.AddWidgets(*pBank, &m_HealthConfigManagerInstance.m_aHealthConfig[i] );
			pBank->PopGroup(); 
		}

		pBank->PopGroup();
	}
}


void CHealthConfigInfoManager::Save()
{
	Verifyf(PARSER.SaveObject("common:/data/pedhealth", "meta", &m_HealthConfigManagerInstance, parManager::XML), "Failed to save ped health config");
}
#endif // __BANK