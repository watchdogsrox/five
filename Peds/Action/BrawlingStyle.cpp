// FILE		: BrawlingStyle.cpp
// PURPOSE	: Load in brawling type information and manage the styles
// CREATED	: 06/10/11

// class header
#include "Peds/Action/BrawlingStyle.h"

// Game headers
#include "scene/datafilemgr.h"

// Parser headers
#include "parser/manager.h"
#include "Peds/Action/BrawlingStyle_parser.h"

// rage headers
#include "bank/bank.h" 

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CBrawlingStyleManager
//////////////////////////////////////////////////////////////////////////

// Static Manager object
CBrawlingStyleManager CBrawlingStyleManager::sm_Instance;


void CBrawlingStyleManager::Init(const char* fileName)
{
	Load(fileName);
}

void CBrawlingStyleManager::Shutdown()
{
	// Delete
	Reset();
}

const CBrawlingStyleData* CBrawlingStyleManager::GetBrawlingData( u32 uStringHash )
{
	for( int i = 0; i < sm_Instance.m_aBrawlingData.GetCount(); i++ )
	{
		if( uStringHash == sm_Instance.m_aBrawlingData[ i ].GetName().GetHash() )
			return &sm_Instance.m_aBrawlingData[ i ];
	}

	Assertf( false, "Brawling style hash (%u) does not exist. Check pedbrawlingstyle.meta file.", uStringHash );
	return NULL;
}

void CBrawlingStyleManager::Load(const char* fileName)
{
	if( Verifyf(!sm_Instance.m_aBrawlingData.GetCount(), "Brawling Data has already been loaded") )
	{
		// Reset/Delete the existing data before loading
		Reset();

		// Load
		PARSER.LoadObject(fileName, "meta", sm_Instance);
	}
}

void CBrawlingStyleManager::Reset()
{
	sm_Instance.m_aBrawlingData.Reset();
}

#if __BANK
void CBrawlingStyleManager::LoadCallback()
{
	CBrawlingStyleManager::Reset();
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_BRAWLING_STYLE_FILE);
	CBrawlingStyleManager::Load(pData->m_filename);
}

void CBrawlingStyleManager::SaveCallback()
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_BRAWLING_STYLE_FILE);
	Verifyf(PARSER.SaveObject(pData->m_filename, "meta", &sm_Instance, parManager::XML), "Failed to save brawling style file:%s", pData->m_filename);
}

void CBrawlingStyleManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Brawling Manager");
	
	bank.AddButton("Save", SaveCallback);
	bank.AddButton("Load", LoadCallback);

	const int nBrawlingDataCount = sm_Instance.m_aBrawlingData.GetCount();
	if( nBrawlingDataCount > 0 )
	{
		bank.PushGroup("Brawling Data");

		for (int i=0;i<nBrawlingDataCount;i++)
		{
			bank.PushGroup( sm_Instance.m_aBrawlingData[ i ].GetName().GetCStr() );
			PARSER.AddWidgets(bank, &sm_Instance.m_aBrawlingData[ i ] );
			bank.PopGroup();
		}

		bank.PopGroup();
	}
	
	bank.PopGroup(); 
}
#endif // __BANK
