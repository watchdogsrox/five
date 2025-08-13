// File header
#include "Task/Movement/Climbing/Ladders/LadderMetadataManager.h"

// Rage headers
#include "ai/aichannel.h"
#include "bank/bank.h"
#include "parser/manager.h"

// Parser
#include "LadderMetadataManager_parser.h"

// Static initialisation
CLadderMetadataManager CLadderMetadataManager::ms_Instance;

void CLadderMetadataManager::Init(u32 UNUSED_PARAM(uInitMode))
{
	// Load
	Load();
}

////////////////////////////////////////////////////////////////////////////////

void CLadderMetadataManager::Shutdown(u32 UNUSED_PARAM(uShutdownMode))
{
	// Delete the data
	Reset();
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
// Add widgets
void CLadderMetadataManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Ladder Data");

	bank.AddButton("Load", Load);
	bank.AddButton("Save", Save);

	bank.PushGroup("Ladders");
	for(s32 i = 0; i < ms_Instance.m_LadderData.GetCount(); i++)
	{
		bank.PushGroup(ms_Instance.m_LadderData[i].m_Name.GetCStr());
		PARSER.AddWidgets(bank, &ms_Instance.m_LadderData[i]);
		bank.PopGroup();
	}
	bank.PopGroup();
	bank.PopGroup();
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

void CLadderMetadataManager::Reset()
{
	ms_Instance.m_LadderData.Reset();
}

////////////////////////////////////////////////////////////////////////////////

void CLadderMetadataManager::Load()
{
	// Delete any existing data
	Reset();

#if 1 // Bypass the datafilemgr for now, as the Data isn't available yet
	// Load
	PARSER.LoadObject("common:/data/ai/Ladders", "meta", ms_Instance);
#else
	// Iterate through the files backwards, so episodic data can override any old data
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::LADDER_METADATA_FILE);
	if(DATAFILEMGR.IsValid(pData))
	{
		// Load
		PARSER.LoadObject(pData->m_filename, "meta", ms_Instance);
	}
#endif // 1
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CLadderMetadataManager::Save()
{
	aiVerify(PARSER.SaveObject("common:/data/ai/Ladders", "meta", &ms_Instance, parManager::XML));
}
#endif // __BANK
