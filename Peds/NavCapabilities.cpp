#include "NavCapabilities.h"
#include "parser/manager.h"
#include "bank/bank.h"
#include "pathserver/PathServer.h"

#include "NavCapabilities_parser.h"

AI_OPTIMISATIONS()


//////////////////////////////////////////////////////////////////////////
// CPedNavCapabilityInfoManager
//////////////////////////////////////////////////////////////////////////

// Static Manager object
CPedNavCapabilityInfoManager CPedNavCapabilityInfoManager::m_NavCapabilityManagerInstance;


void CPedNavCapabilityInfoManager::Init(const char* pFileName)
{
	Load(pFileName);
}


void CPedNavCapabilityInfoManager::Shutdown()
{
	// Delete
	Reset();
}

void CPedNavCapabilityInfoManager::Load(const char* pFileName)
{
	if(Verifyf(!m_NavCapabilityManagerInstance.m_aPedNavCapabilities.GetCount(), "Nav Capabilities have already been loaded"))
	{
		// Reset/Delete the existing data before loading
		Reset();

		// Load
		PARSER.LoadObject(pFileName, "meta", m_NavCapabilityManagerInstance);
	}
}


void CPedNavCapabilityInfoManager::Reset()
{
	m_NavCapabilityManagerInstance.m_aPedNavCapabilities.Reset();
}

#if __BANK
void CPedNavCapabilityInfoManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Ped Nav Capabilities");

	bank.AddButton("Save", Save);

	bank.PushGroup("Capabilities");
	for(s32 i = 0; i < m_NavCapabilityManagerInstance.m_aPedNavCapabilities.GetCount(); i++)
	{
		bank.PushGroup(m_NavCapabilityManagerInstance.m_aPedNavCapabilities[i].GetName());
		PARSER.AddWidgets(bank, &m_NavCapabilityManagerInstance.m_aPedNavCapabilities[i]);
		bank.PopGroup(); 
	}
	bank.PopGroup();
	bank.PopGroup(); 
}


void CPedNavCapabilityInfoManager::Save()
{
	Verifyf(PARSER.SaveObject("common:/data/navcapabilities", "meta", &m_NavCapabilityManagerInstance, parManager::XML), "Failed to save ped nav capabilities");
}
#endif // __BANK

CPedNavCapabilityInfo::CPedNavCapabilityInfo()
{
	Init();
}

void CPedNavCapabilityInfo::Init()
{
	m_Name.Clear();
	m_BitFlags = 0;
	m_MaxClimbHeight = 0.0f;
	m_MaxDropHeight = 0.0f;
	m_IgnoreObjectMass = 0.0f;
	m_fClimbCostModifier = 0.0f;
}

u64 CPedNavCapabilityInfo::BuildPathStyleFlags() const
{
	u64 uFlags = 0;

	if (!m_BitFlags.IsFlagSet(FLAG_MAY_CLIMB))
	{
		uFlags |= PATH_FLAG_NEVER_CLIMB_OVER_STUFF;
	}

	if (!m_BitFlags.IsFlagSet(FLAG_MAY_DROP))
	{
		uFlags |= PATH_FLAG_NEVER_DROP_FROM_HEIGHT;
	}

	if (!m_BitFlags.IsFlagSet(FLAG_MAY_USE_LADDERS))
	{
		uFlags |= PATH_FLAG_NEVER_USE_LADDERS;
	}

	if (!m_BitFlags.IsFlagSet(FLAG_AVOID_OBJECTS))
	{
		uFlags |= PATH_FLAG_DONT_AVOID_DYNAMIC_OBJECTS;
	}

	if (!m_BitFlags.IsFlagSet(FLAG_MAY_ENTER_WATER))
	{
		uFlags |= PATH_FLAG_NEVER_ENTER_WATER;
	}

	if (!m_BitFlags.IsFlagSet(FLAG_AVOID_FIRE))
	{
		uFlags |= PATH_FLAG_DONT_AVOID_FIRE;
	}

	return uFlags;
}