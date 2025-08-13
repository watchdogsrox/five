// FILE :    PedComponentSetInfo.cpp
// PURPOSE : Load in component information for setting up different types of peds
// CREATED : 21-12-2010

// class header
#include "PedComponentSetInfo.h"

// Framework headers

// Game headers

// Parser headers
#include "PedComponentSetInfo_parser.h"

// rage headers
#include "bank/bank.h"
#include "parser/manager.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CPedComponentSetInfo
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// CPedComponentSetManager
//////////////////////////////////////////////////////////////////////////

// Static Manager object
CPedComponentSetManager CPedComponentSetManager::m_ComponentSetManagerInstance;


CPedComponentSetManager::CPedComponentSetManager()
{
	m_DefaultSet = NULL;
}

void CPedComponentSetManager::Init(const char* pFileName)
{
	Load(pFileName);
}


void CPedComponentSetManager::Shutdown()
{
	// Delete
	Reset();
}


void CPedComponentSetManager::Load(const char* pFileName)
{
	if(Verifyf(!m_ComponentSetManagerInstance.m_Infos.GetCount(), "Ped component set info has already been loaded"))
	{
		// Reset/Delete the existing data before loading
		Reset();

		// Load
		PARSER.LoadObject(pFileName, "meta", m_ComponentSetManagerInstance);
	}
}

void CPedComponentSetManager::AppendExtra(const char* pFileName)
{
	CPedComponentSetManager tempInst;

	if(Verifyf(PARSER.LoadObject(pFileName, "meta", tempInst), "CPedComponentSetManager::AppendExtra can't load %s", pFileName))
	{
		for(int i = 0; i < tempInst.m_Infos.GetCount(); i++)
		{
			CPedComponentSetInfo *pNewInfo = tempInst.m_Infos[i];

			if(!FindInfo(pNewInfo->GetHash()))
			{
				m_ComponentSetManagerInstance.m_Infos.PushAndGrow(pNewInfo);
			}
			else
			{
				Assertf(false, "CPedComponentSetManager::AppendExtra entry with name %s already exists! We can't override entries with DLC.", pNewInfo->GetName()); 
				delete pNewInfo;
			}
		}				
	}
}

void CPedComponentSetManager::RemoveEntry(CPedComponentSetInfo *pInfoToRemove)
{
	for(int i = 0; i < m_ComponentSetManagerInstance.m_Infos.GetCount(); i++)
	{
		CPedComponentSetInfo *pInfo = m_ComponentSetManagerInstance.m_Infos[i];

		if(pInfo->GetHash() == pInfoToRemove->GetHash())
		{
			delete pInfo;
			m_ComponentSetManagerInstance.m_Infos.DeleteFast(i);
			return;
		}
	}

	Assertf(false, "CPedComponentSetManager::RemoveEntry entry with name %s doesn't exist!", pInfoToRemove->GetName());
}

void CPedComponentSetManager::RemoveExtra(const char* pFileName)
{
	CPedComponentSetManager tempInst;

	if(Verifyf(PARSER.LoadObject(pFileName, "meta", tempInst), "CPedComponentSetManager::RemoveExtra can't load %s", pFileName))
	{
		for(int i = 0; i < tempInst.m_Infos.GetCount(); i++)
		{
			CPedComponentSetInfo *pInfoToRemove = tempInst.m_Infos[i];
			RemoveEntry(pInfoToRemove);
			delete pInfoToRemove;
		}
	}
}


void CPedComponentSetManager::Reset()
{
	for(int i = 0; i < m_ComponentSetManagerInstance.m_Infos.GetCount(); i++)
	{
		delete m_ComponentSetManagerInstance.m_Infos[i];
	}

	m_ComponentSetManagerInstance.m_Infos.Reset();
}

const CPedComponentSetInfo* const CPedComponentSetManager::FindInfo(const u32 uNameHash)
{
	if(uNameHash != 0)
	{
		for (s32 i = 0; i < m_ComponentSetManagerInstance.m_Infos.GetCount(); i++ )
		{
			if(m_ComponentSetManagerInstance.m_Infos[i]->GetHash() == uNameHash)
			{
				return m_ComponentSetManagerInstance.m_Infos[i];
			}
		}
	}

	return NULL;
}

const CPedComponentSetInfo* const CPedComponentSetManager::GetInfo(const u32 uNameHash)
{
	if(uNameHash != 0)
	{
		for (s32 i = 0; i < m_ComponentSetManagerInstance.m_Infos.GetCount(); i++ )
		{
			if(m_ComponentSetManagerInstance.m_Infos[i]->GetHash() == uNameHash)
			{
				return m_ComponentSetManagerInstance.m_Infos[i];
			}
		}
	}
	return m_ComponentSetManagerInstance.m_DefaultSet;
}


#if __BANK
void CPedComponentSetManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Ped Component Sets");

	bank.AddButton("Save", Save);

	bank.PushGroup("Sets");
	for(s32 i = 0; i < m_ComponentSetManagerInstance.m_Infos.GetCount(); i++)
	{
		bank.PushGroup(m_ComponentSetManagerInstance.m_Infos[i]->GetName());
		PARSER.AddWidgets(bank, m_ComponentSetManagerInstance.m_Infos[i]);
		bank.PopGroup(); 
	}
	bank.PopGroup();
	bank.PopGroup(); 
}


void CPedComponentSetManager::Save()
{
	Verifyf(PARSER.SaveObject("common:/data/pedcomponentsets", "meta", &m_ComponentSetManagerInstance, parManager::XML), "Failed to save ped components sets");
}

#endif // __BANK


// CPedComponentClothManager
CPedComponentClothManager CPedComponentClothManager::ms_ComponentClothManagerInstance;

void CPedComponentClothManager::Init(const char* pFileName)
{
	Load(pFileName);
}

void CPedComponentClothManager::Shutdown()
{
	Reset();
}

void CPedComponentClothManager::Load(const char* pFileName)
{
	if(Verifyf(!ms_ComponentClothManagerInstance.m_Infos.GetCount(), "Ped component cloth info has already been loaded"))
	{
		Reset();
		PARSER.LoadObject(pFileName, "meta", ms_ComponentClothManagerInstance);
	}
}

void CPedComponentClothManager::Reset()
{
	ms_ComponentClothManagerInstance.m_Infos.Reset();
}

const CPedComponentClothInfo* const CPedComponentClothManager::GetInfo(const u32 uNameHash)
{
	if(uNameHash != 0)
	{
		for (s32 i = 0; i < ms_ComponentClothManagerInstance.m_Infos.GetCount(); i++ )
		{
			if(ms_ComponentClothManagerInstance.m_Infos[i].GetHash() == uNameHash)
			{
				return &ms_ComponentClothManagerInstance.m_Infos[i];
			}
		}
	}
	return NULL;
}

#if __BANK
void CPedComponentClothManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Ped Component Cloth");
	bank.AddButton("Save", Save);
	bank.PushGroup("Sets");
	for(s32 i = 0; i < ms_ComponentClothManagerInstance.m_Infos.GetCount(); i++)
	{
		bank.PushGroup(ms_ComponentClothManagerInstance.m_Infos[i].GetName());
		PARSER.AddWidgets(bank, &ms_ComponentClothManagerInstance.m_Infos[i]);
		bank.PopGroup(); 
	}
	bank.PopGroup();
	bank.PopGroup(); 
}

void CPedComponentClothManager::Save()
{
	Verifyf(PARSER.SaveObject("common:/data/pedcomponentcloth", "meta", &ms_ComponentClothManagerInstance, parManager::XML), "Failed to save ped components sets");
}

#endif // __BANK