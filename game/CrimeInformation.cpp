// File header
#include "game/CrimeInformation.h"

// Rage headers
#include "atl/bitset.h"
#include "parser/macros.h"
#include "parser/manager.h"

// Game headers
#include "Dispatch/DispatchData.h"
#include "Network/NetworkInterface.h"

// Parser files
#include "CrimeInformation_parser.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CWitnessInformation
////////////////////////////////////////////////////////////////////////////////

CWitnessInformation::CWitnessInformation()
: m_MaxDistanceToHear(0.0f)
, m_Flags(0)
, m_AdditionalFlagsForSP(0)
, m_AdditionalFlagsForMP(0)
{

}

CWitnessInformation::~CWitnessInformation()
{

}

bool CWitnessInformation::IsFlagSet(u8 uFlag) const
{
	//Check if the flag is set.
	if(m_Flags.IsFlagSet(uFlag))
	{
		return true;
	}

	//Check if we are in SP.
	if(!NetworkInterface::IsGameInProgress())
	{
		//Check if the flag is set.
		if(m_AdditionalFlagsForSP.IsFlagSet(uFlag))
		{
			return true;
		}
	}
	else
	{
		//Check if the flag is set.
		if(m_AdditionalFlagsForMP.IsFlagSet(uFlag))
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////
// CCrimeInformationManager
////////////////////////////////////////////////////////////////////////////////

CCrimeInformationManager *CCrimeInformationManager::sm_Instance = NULL;

CCrimeInformationManager::CCrimeInformationManager()
: m_CrimeInformations()
{
	//Parse the crime information.
	PARSER.LoadObject("common:/data/ai/crimeinformation", "meta", m_CrimeInformations);
}

CCrimeInformationManager::~CCrimeInformationManager()
{
}

bool CCrimeInformationManager::CanBeHeard(eCrimeType nCrimeType, float& fMaxDistance) const
{
	//Ensure the information is valid.
	const CCrimeInformation* pCrimeInformation = GetInformation(nCrimeType);
	if(!pCrimeInformation)
	{
		return false;
	}

	//Ensure the max distance to hear is valid.
	float fMaxDistanceToHear = pCrimeInformation->m_WitnessInformation.GetMaxDistanceToHear();
	if(fMaxDistanceToHear <= 0.0f)
	{
		return false;
	}

	//Set the max distance.
	fMaxDistance = fMaxDistanceToHear;

	return true;
}

float CCrimeInformationManager::GetImmediateDetectionRange(eCrimeType nCrimeType) const
{
	//Check if the crime information is valid.
	const CCrimeInformation* pCrimeInformation = GetInformation(nCrimeType);
	if(pCrimeInformation)
	{
		return pCrimeInformation->m_ImmediateDetectionRange;
	}

	return CDispatchData::GetImmediateDetectionRange();
}

bool CCrimeInformationManager::MustBeWitnessed(eCrimeType nCrimeType) const
{
	//Check if the flag is set.
	const CCrimeInformation* pCrimeInformation = GetInformation(nCrimeType);
	if(pCrimeInformation && pCrimeInformation->m_WitnessInformation.IsFlagSet(CWitnessInformation::MustBeWitnessed))
	{
		return true;
	}

	return false;
}

bool CCrimeInformationManager::MustNotifyLawEnforcement(eCrimeType nCrimeType) const
{
	//Check if the flag is set.
	const CCrimeInformation* pCrimeInformation = GetInformation(nCrimeType);
	if(pCrimeInformation && pCrimeInformation->m_WitnessInformation.IsFlagSet(CWitnessInformation::MustNotifyLawEnforcement))
	{
		return true;
	}

	return false;
}

bool CCrimeInformationManager::OnlyVictimCanNotifyLawEnforcement(eCrimeType nCrimeType) const
{
	//Check if the flag is set.
	const CCrimeInformation* pCrimeInformation = GetInformation(nCrimeType);
	if(pCrimeInformation && pCrimeInformation->m_WitnessInformation.IsFlagSet(CWitnessInformation::OnlyVictimCanNotifyLawEnforcement))
	{
		return true;
	}

	return false;
}

void CCrimeInformationManager::Init(unsigned UNUSED_PARAM(initMode))
{
	Assert(!sm_Instance);
	sm_Instance = rage_new CCrimeInformationManager;
}

void CCrimeInformationManager::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	Assert(sm_Instance);
	delete sm_Instance;
	sm_Instance = NULL;
}
