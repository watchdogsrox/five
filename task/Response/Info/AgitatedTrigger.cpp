// File header
#include "Task/Response/Info/AgitatedTrigger.h"

// Game headers
#include "modelinfo/PedModelInfo.h"
#include "peds/ped.h"
#include "Peds/PedType.h"
#include "task/Response/Info/AgitatedManager.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CAgitatedTriggerReaction
////////////////////////////////////////////////////////////////////////////////

CAgitatedTriggerReaction::CAgitatedTriggerReaction()
: m_Type(AT_None)
, m_TimeBeforeInitialReaction()
, m_TimeAfterLastSuccessfulReaction()
, m_TimeAfterInitialReactionFailure()
, m_MinDotToTarget(0.0f)
, m_MaxDotToTarget(0.0f)
, m_MaxReactions(0)
, m_Flags(0)
, m_Reactions(0)
{

}

CAgitatedTriggerReaction::~CAgitatedTriggerReaction()
{

}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedTrigger
////////////////////////////////////////////////////////////////////////////////

CAgitatedTrigger::CAgitatedTrigger()
: m_PedTypes()
, m_Distance(0.0f)
, m_Chances(0.0f)
, m_Reaction()
, m_nPedTypes()
{

}

CAgitatedTrigger::~CAgitatedTrigger()
{

}

bool CAgitatedTrigger::HasPedType(ePedType nPedType) const
{
	//Iterate over the ped types.
	for(int i = 0; i < m_nPedTypes.GetCount(); ++i)
	{
		//Check if the ped type matches.
		if(m_nPedTypes[i] == nPedType)
		{
			return true;
		}
	}

	return false;
}

void CAgitatedTrigger::OnPostLoad()
{
	//Find the ped types.
	m_nPedTypes.Reserve(m_PedTypes.GetCount());
	for(int i = 0; i < m_PedTypes.GetCount(); ++i)
	{
		m_nPedTypes.Append() = CPedType::FindPedType(m_PedTypes[i].c_str());
	}
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedTriggers
////////////////////////////////////////////////////////////////////////////////

CAgitatedTriggers::CAgitatedTriggers()
: m_Reactions()
, m_Sets()
{

}

CAgitatedTriggers::~CAgitatedTriggers()
{

}

atHashWithStringNotFinal CAgitatedTriggers::GetSet(const CPed& rPed) const
{
	//Ensure the ped model info is valid.
	const CPedModelInfo* pModelInfo = rPed.GetPedModelInfo();
	if(!pModelInfo)
	{
		return atHashWithStringNotFinal::Null();
	}

	return atHashWithStringNotFinal(pModelInfo->GetPersonalitySettings().GetAgitationTriggersHash());
}

const CAgitatedTrigger* CAgitatedTriggers::GetTrigger(const CPed& rPed, atHashWithStringNotFinal hReaction) const
{
	//Grab the set.
	atHashWithStringNotFinal hSet = GetSet(rPed);
	while(hSet.IsNotNull())
	{
		//Ensure the set is valid.
		const CAgitatedTriggerSet* pSet = GetSet(hSet);
		if(!pSet)
		{
			break;
		}

		//Iterate over the triggers.
		const atArray<CAgitatedTrigger>& rTriggers = pSet->GetTriggers();
		for(int i = 0; i < rTriggers.GetCount(); ++i)
		{
			//Ensure the reaction matches.
			const CAgitatedTrigger& rTrigger = rTriggers[i];
			if(hReaction != rTrigger.GetReaction())
			{
				continue;
			}

			return &rTrigger;
		}

		//Move to the next set.
		hSet = pSet->GetParent();
	}

	return NULL;
}

bool CAgitatedTriggers::HasReactionWithType(const CPed& rPed, const CPed& rTarget, AgitatedType nType) const
{
	//Load the reactions.
	static const int s_iMaxReactions = 4;
	atHashWithStringNotFinal aReactions[s_iMaxReactions];
	int iNumReactions = LoadReactions(aReactions, s_iMaxReactions, nType);

	//Iterate over the reactions.
	for(int i = 0; i < iNumReactions; ++i)
	{
		//Ensure the trigger is valid.
		const CAgitatedTrigger* pTrigger = CAgitatedManager::GetInstance().GetTriggers().GetTrigger(rPed, aReactions[i]);
		if(!pTrigger)
		{
			continue;
		}

		//Ensure the trigger has the target type.
		if(!pTrigger->HasPedType(rTarget.GetPedType()))
		{
			continue;
		}

		return true;
	}

	return false;
}

bool CAgitatedTriggers::IsFlagSet(const CPed& rPed, CAgitatedTriggerSet::Flags nFlag) const
{
	//Grab the set.
	atHashWithStringNotFinal hSet = GetSet(rPed);
	while(hSet.IsNotNull())
	{
		//Ensure the set is valid.
		const CAgitatedTriggerSet* pSet = GetSet(hSet);
		if(!pSet)
		{
			break;
		}

		//Check if the flag is set.
		if(pSet->GetFlags().IsFlagSet(nFlag))
		{
			return true;
		}

		//Move to the next set.
		hSet = pSet->GetParent();
	}

	return false;
}

int CAgitatedTriggers::LoadReactions(atHashWithStringNotFinal* aReactions, int iMaxReactions, AgitatedType nFilter) const
{
	//Clear the reactions.
	int iNumReactions = 0;

	//Check if the filter is valid.
	bool bIsFilterValid = (nFilter != AT_None);

	//Iterate over the reactions.
	int iCount = m_Reactions.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure we have not reached the max reactions.
		if(iNumReactions >= iMaxReactions)
		{
			break;
		}

		//Grab the reaction.
		atHashWithStringNotFinal hReaction = *(m_Reactions.GetKey(i));

		//Check if the filter is valid.
		if(bIsFilterValid)
		{
			//Ensure the filter matches.
			const CAgitatedTriggerReaction* pReaction = GetReaction(hReaction);
			if(!pReaction || (nFilter != pReaction->GetType()))
			{
				continue;
			}
		}

		//Add the reaction.
		aReactions[iNumReactions++] = hReaction;
	}

	return iNumReactions;
}