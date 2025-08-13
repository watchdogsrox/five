// FILE :    PedIKSettings.cpp
// PURPOSE : Load in ped ik info
// CREATED : 10-06-2011

// class header
#include "PedIKSettings.h"

// Framework headers

// Game headers
#include "ik/IkConfig.h"

// Parser headers
#include "PedIKSettings_parser.h"

// rage headers
#include "bank/bank.h"
#include "parser/manager.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CPedIKSettingsInfo
//////////////////////////////////////////////////////////////////////////

const bool CPedIKSettingsInfo::IsIKFlagDisabled( CBaseIkManager::eIkSolverType type ) const
{
#if IK_QUAD_LEG_SOLVER_ENABLE
	if (type == IKManagerSolverTypes::ikSolverTypeQuadLeg)
	{
		// Disable ikSolverTypeQuadLeg by default for all peds.
		// Solver will be enabled only for specific DLC peds elsewhere.
		return true;
	}
#endif // IK_QUAD_LEG_SOLVER_ENABLE

#if IK_QUAD_REACT_SOLVER_ENABLE
	if (type == IKManagerSolverTypes::ikSolverTypeQuadrupedReact)
	{
		// Disable ikSolverTypeQuadrupedReact by default for all peds.
		// Solver will be enabled only for specific DLC peds elsewhere.
		return true;
	}
#endif // IK_QUAD_REACT_SOLVER_ENABLE

	return m_IKSolversDisabled.BitSet().IsSet(type);
}

//////////////////////////////////////////////////////////////////////////
// CPedIKSettingsInfoManager
//////////////////////////////////////////////////////////////////////////

// Static Manager object
CPedIKSettingsInfoManager CPedIKSettingsInfoManager::m_PedIKSettingsManagerInstance;

CPedIKSettingsInfoManager::CPedIKSettingsInfoManager() :
m_DefaultSet	(NULL)
{
}

void CPedIKSettingsInfoManager::Init(const char* pFileName)
{
	Load(pFileName);
}


void CPedIKSettingsInfoManager::Shutdown()
{
	// Delete
	Reset();
}


void CPedIKSettingsInfoManager::Load(const char* pFileName)
{
	if(Verifyf(!m_PedIKSettingsManagerInstance.m_aPedIKSettings.GetCount(), "Ped IK info has already been loaded"))
	{
		// Reset/Delete the existing data before loading
		Reset();

		// Load
		PARSER.LoadObject(pFileName, "meta", m_PedIKSettingsManagerInstance);
	}
}


void CPedIKSettingsInfoManager::Reset()
{
	m_PedIKSettingsManagerInstance.m_aPedIKSettings.Reset();
}


#if __BANK
void CPedIKSettingsInfoManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Ped IK Settings");

	bank.AddButton("Save", Save);

	bank.PushGroup("PedIKSettings");
	for (s32 i = 0; i < m_PedIKSettingsManagerInstance.m_aPedIKSettings.GetCount(); i++)
	{
		bank.PushGroup(m_PedIKSettingsManagerInstance.m_aPedIKSettings[i].GetName() );
		PARSER.AddWidgets(bank, &m_PedIKSettingsManagerInstance.m_aPedIKSettings[i] );
		bank.PopGroup(); 
	}
	bank.PopGroup();
	bank.PopGroup(); 
}


void CPedIKSettingsInfoManager::Save()
{
	Verifyf(PARSER.SaveObject("common:/data/pediksettings", "meta", &m_PedIKSettingsManagerInstance, parManager::XML), "Failed to save ped ik settings");
}
#endif // __BANK