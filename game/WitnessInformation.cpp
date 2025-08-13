// File header
#include "game/WitnessInformation.h"

// Rage headers
#include "parser/manager.h"

// Game headers
#include "modelinfo/PedModelInfo.h"
#include "Peds/ped.h"

// Parser files
#include "WitnessInformation_parser.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CWitnessInformationManager
////////////////////////////////////////////////////////////////////////////////

CWitnessInformationManager *CWitnessInformationManager::sm_Instance = NULL;

CWitnessInformationManager::CWitnessInformationManager()
: m_WitnessInformations()
{
	//Parse the witness information.
	PARSER.LoadObject("common:/data/ai/witnessinformation", "meta", m_WitnessInformations);
}

CWitnessInformationManager::~CWitnessInformationManager()
{

}

bool CWitnessInformationManager::CanWitnessCrimes(const CPed& rPed) const
{
	//Ensure the personality is valid.
	const CWitnessPersonality* pPersonality = GetPersonality(rPed);
	if(pPersonality && pPersonality->m_Flags.IsFlagSet(CWitnessPersonality::CanWitnessCrimes))
	{
		return true;
	}

	return false;
}

bool CWitnessInformationManager::WillCallLawEnforcement(const CPed& rPed) const
{
	//Ensure the personality is valid.
	const CWitnessPersonality* pPersonality = GetPersonality(rPed);
	if(pPersonality && pPersonality->m_Flags.IsFlagSet(CWitnessPersonality::WillCallLawEnforcement))
	{
		return true;
	}

	return false;
}

bool CWitnessInformationManager::WillMoveToLawEnforcement(const CPed& rPed) const
{
	//Ensure the personality is valid.
	const CWitnessPersonality* pPersonality = GetPersonality(rPed);
	if(pPersonality && pPersonality->m_Flags.IsFlagSet(CWitnessPersonality::WillMoveToLawEnforcement))
	{
		return true;
	}

	return false;
}

void CWitnessInformationManager::Init(unsigned UNUSED_PARAM(initMode))
{
	Assert(!sm_Instance);
	sm_Instance = rage_new CWitnessInformationManager;
}

void CWitnessInformationManager::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	Assert(sm_Instance);
	delete sm_Instance;
	sm_Instance = NULL;
}

const CWitnessPersonality* CWitnessInformationManager::GetPersonality(const CPed& rPed) const
{
	//Ensure the model info is valid.
	const CPedModelInfo* pPedModelInfo = rPed.GetPedModelInfo();
	if(!pPedModelInfo)
	{
		return NULL;
	}

	return GetPersonality(pPedModelInfo->GetPersonalitySettings().GetWitnessPersonality());
}