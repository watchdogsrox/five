// File header
#include "Event/Decision/EventDecisionMakerManager.h"

// Framework headers
#include "ai/aichannel.h"

// Game headers
#include "Event/Decision/EventDecisionMaker.h"
#include "Event/Decision/EventDecisionMakerResponse.h"
#include "Event/System/EventDataManager.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CEventDecisionMakerManager
//////////////////////////////////////////////////////////////////////////

// Static initialisation
CEventDecisionMakerManager::DecisionMakers							CEventDecisionMakerManager::ms_decisionMakers;

void CEventDecisionMakerManager::Init()
{
	// Init pools
	CEventDecisionMaker::InitPool( MEMBUCKET_GAMEPLAY );
	CEventDecisionMakerModifiableComponent::InitPool( MEMBUCKET_GAMEPLAY );
	CEventDecisionMakerResponseDynamic::InitPool( MEMBUCKET_GAMEPLAY );
}

void CEventDecisionMakerManager::Shutdown()
{
	// Cleanup decision makers
	for(s32 i = 0; i < ms_decisionMakers.GetCount(); i++)
	{
		delete ms_decisionMakers[i];
	}
	ms_decisionMakers.Reset();

	// Shutdown pools
	CEventDecisionMaker::ShutdownPool();
	CEventDecisionMakerModifiableComponent::ShutdownPool();
	CEventDecisionMakerResponseDynamic::ShutdownPool();
}

bool CEventDecisionMakerManager::GetIsDecisionMakerValid(u32 uDecisionMakerId)
{
	s32 iIndex = GetDecisionMakerIndexById(uDecisionMakerId);
	if(iIndex != -1)
	{
		return true;
	}

	const CEventDataDecisionMaker* pData = CEventDataManager::GetInstance().GetDecisionMaker(uDecisionMakerId);
	if(pData)
	{
		return true;
	}

	return false;
}

CEventDecisionMaker* CEventDecisionMakerManager::GetDecisionMaker(u32 uDecisionMakerId)
{
	s32 iIndex = GetDecisionMakerIndexById(uDecisionMakerId);
	if(iIndex != -1)
	{
		// Already allocated
		return ms_decisionMakers[iIndex];
	}

	// Attempt to allocate a new decision maker
	if(aiVerifyf(!ms_decisionMakers.IsFull(), "Decision maker storage is full, max capacity is [%d]", ms_decisionMakers.GetMaxCount()))
	{
		const CEventDataDecisionMaker* pData = CEventDataManager::GetInstance().GetDecisionMaker(uDecisionMakerId);
		if(aiVerifyf(pData, "Decision maker with hash [%d] doesn't exist in RAVE data", uDecisionMakerId))
		{
			CEventDecisionMaker* pDecisionMaker = rage_new CEventDecisionMaker(uDecisionMakerId, pData);
			ms_decisionMakers.Push(pDecisionMaker);

			// Re-sort array
			SortDecisionMakers();

			// Return new decision maker
			return pDecisionMaker;
		}
	}

	return NULL;
}

CEventDecisionMaker* CEventDecisionMakerManager::CloneDecisionMaker(u32 uDecisionMakerId, const char* pNewName)
{
	const CEventDecisionMaker* pDecisionMaker = GetDecisionMaker(uDecisionMakerId);
	if(aiVerifyf(pDecisionMaker, "Source decision maker with Id [%d] doesn't exist, so cannot be cloned", uDecisionMakerId))
	{
		// Check if there is enough storage
		if(aiVerifyf(!ms_decisionMakers.IsFull(), "Decision maker storage is full, max capacity is [%d]", ms_decisionMakers.GetMaxCount()))
		{
			// Check if name is already in use
			if(aiVerifyf(!GetIsDecisionMakerValid(atStringHash(pNewName)), "Decision maker with name [%s] already exists, cloning failed", pNewName))
			{
				// Allocate a new decision maker
				CEventDecisionMaker* pDecisionMakerClone = pDecisionMaker->Clone(pNewName);
				ms_decisionMakers.Push(pDecisionMakerClone);

				// Re-sort array
				SortDecisionMakers();

				// Return cloned decision maker
				return pDecisionMakerClone;
			}
		}
	}

	return NULL;
}

void CEventDecisionMakerManager::DeleteDecisionMaker(u32 uDecisionMakerId)
{
	s32 iIndex = GetDecisionMakerIndexById(uDecisionMakerId);
	if(iIndex != -1)
	{
		// Already allocated
		delete ms_decisionMakers[iIndex];
		ms_decisionMakers.DeleteFast(iIndex);

		// Re-sort array as we have deleted one
		SortDecisionMakers();

		// We need to scan through all the peds and find any that are using this decision maker
		CPed::Pool* pPedPool = CPed::GetPool();
		s32 iIndex = pPedPool->GetSize();
		while(iIndex--)
		{
			CPed* pPed = pPedPool->GetSlot(iIndex);
			if(pPed)
			{
				if(pPed->GetPedIntelligence()->GetDecisionMakerId() == uDecisionMakerId)
				{
					// The ped's decision maker is being deleted, reset it to default
					pPed->SetDefaultDecisionMaker();
				}
			}
		}
	}
}

s32 CEventDecisionMakerManager::GetDecisionMakerIndexById(u32 uId)
{
	s32 iLow = 0;
	s32 iHigh = ms_decisionMakers.GetCount()-1;
	while(iLow <= iHigh)
	{
		s32 iMid = (iLow + iHigh) >> 1;
		aiFatalAssertf(ms_decisionMakers[iMid], "GetDecisionMakerById: Querying NULL pointer, shouldn't happen");
		if(uId == ms_decisionMakers[iMid]->GetId())
			return iMid;
		else if(uId < ms_decisionMakers[iMid]->GetId())
			iHigh = iMid-1;
		else
			iLow = iMid+1;
	}

	return -1;
}
