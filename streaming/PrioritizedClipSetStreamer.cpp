// File header
#include "streaming/PrioritizedClipSetStreamer.h"

// Rage headers
#include "fwanimation/animmanager.h"
#include "fwanimation/clipsets.h"
#include "fwsys/timer.h"
#include "game/GameSituation.h"
#include "grcore/debugdraw.h"
#include "streaming/streaming.h"
#include "streaming/streamingdefs.h"
#include "streaming/streamingvisualize.h"

// Game headers
#include "task/system/taskhelpers.h"

STREAMING_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

//----------------------------------------------------------------------
RAGE_DEFINE_SUBCHANNEL(streaming, prioritized);
//----------------------------------------------------------------------

namespace
{
	const atHashWithStringNotFinal	s_hDefaultMemoryGroup = "MG_None";
	const eStreamingPriority		s_nDefaultStreamingPriority = SP_Medium;
}

////////////////////////////////////////////////////////////////////////////////
// CPrioritizedClipSetRequest
////////////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL(CPrioritizedClipSetRequest, CONFIGURED_FROM_FILE, atHashString("CPrioritizedClipSetRequest",0x22b81e14));

CPrioritizedClipSetRequest::CPrioritizedClipSetRequest(fwMvClipSetId clipSetId)
: fwRefAwareBase()
, m_ClipSet(NULL)
, m_ClipSetId(clipSetId)
, m_nStreamingPriority(SP_Invalid)
, m_nStreamingPriorityOverride(SP_Invalid)
, m_nDesiredStreamingState(DSS_Unknown)
, m_uRunningFlags(0)
, m_uReferences(0)
, m_uTiebreaker(0)
{
	if(streamPrioritizedVerifyf(m_ClipSetId != CLIP_SET_ID_INVALID, "The clip set is invalid."))
	{
		m_ClipSet = fwClipSetManager::GetClipSet(m_ClipSetId);
	}
}

CPrioritizedClipSetRequest::~CPrioritizedClipSetRequest()
{
	//Check if we have set the don't delete flag.
	if(HasSetDontDeleteFlag())
	{
		//Clear the don't delete flag.
		ClearDontDeleteFlag();
	}

	//Check if we have added a reference to the clip set.
	if(HasAddedReferenceToClipSet())
	{
		//Release the clip set.
		ReleaseClipSet();
	}
}

int CPrioritizedClipSetRequest::GetClipDictionaryIndex() const
{
	//Ensure the clip set is valid.
	if(!streamPrioritizedVerifyf(m_ClipSet, "The clip set with id: %s is invalid.", m_ClipSetId.GetCStr()))
	{
		return false;
	}

	return m_ClipSet->GetClipDictionaryIndex().Get();
}

bool CPrioritizedClipSetRequest::IsClipSetStreamedIn() const
{
	//Ensure the clip set is valid.
	if(!streamPrioritizedVerifyf(m_ClipSet, "The clip set with id: %s is invalid.", m_ClipSetId.GetCStr()))
	{
		return false;
	}

	return IsClipSetStreamedIn(*m_ClipSet);
}

void CPrioritizedClipSetRequest::AddReferenceToClipSet(fwClipSet& rClipSet)
{
	//Ensure that we have not added a reference to the clip set.
	if(!streamPrioritizedVerifyf(!HasAddedReferenceToClipSet(), "A reference has already been added to clip set with id: %s.", m_ClipSetId.GetCStr()))
	{
		return;
	}
	
	//Set the flag.
	m_uRunningFlags.SetFlag(RF_HasAddedReferenceToClipSet);
	
	//Add a reference to the clip set.
	rClipSet.AddRef_DEPRECATED();

	streamPrioritizedDebugf3("Prioritized clip set: %s added a reference.", m_ClipSetId.TryGetCStr());

	//Check if we have set the don't delete flag.
	if(HasSetDontDeleteFlag())
	{
		//Clear the don't delete flag.
		ClearDontDeleteFlag(rClipSet);
	}
}

void CPrioritizedClipSetRequest::ClearDontDeleteFlag()
{
	//Ensure the clip set is valid.
	if(!streamPrioritizedVerifyf(m_ClipSet, "The clip set with id: %s is invalid.", m_ClipSetId.GetCStr()))
	{
		return;
	}

	//Clear the don't delete flag.
	ClearDontDeleteFlag(*m_ClipSet);
}

void CPrioritizedClipSetRequest::ClearDontDeleteFlag(fwClipSet& rClipSet)
{
	//Assert that we have set the flag.
	streamPrioritizedAssert(HasSetDontDeleteFlag());

	//Clear the flag.
	rClipSet.ClearRequiredFlag_DEPRECATED(STRFLAG_DONTDELETE);

	//Clear the flag.
	m_uRunningFlags.ClearFlag(RF_HasSetDontDeleteFlag);
}

void CPrioritizedClipSetRequest::Initialize()
{
	//Ensure the clip set is valid.
	if(!streamPrioritizedVerifyf(m_ClipSet, "The clip set with id: %s is invalid.", m_ClipSetId.GetCStr()))
	{
		return;
	}
	
	//Ensure the clip dictionary metadata is valid.
	fwClipDictionaryMetadata* pClipDictionaryMetadata = m_ClipSet->GetClipDictionaryMetadata();
	streamPrioritizedAssertf(pClipDictionaryMetadata, "The metadata is invalid for clip dictionary: %s.", m_ClipSet->GetClipDictionaryName().GetCStr());

	//Set the streaming priority.
	m_nStreamingPriority = pClipDictionaryMetadata ? pClipDictionaryMetadata->GetStreamingPriority() : s_nDefaultStreamingPriority;
}

bool CPrioritizedClipSetRequest::IsClipSetStreamedIn(const fwClipSet& rClipSet) const
{
	return rClipSet.IsStreamedIn_DEPRECATED();
}

void CPrioritizedClipSetRequest::ReleaseClipSet()
{
	//Ensure the clip set is valid.
	if(!streamPrioritizedVerifyf(m_ClipSet, "The clip set with id: %s is invalid.", m_ClipSetId.GetCStr()))
	{
		return;
	}

	//Release the clip set.
	ReleaseClipSet(*m_ClipSet);
}

void CPrioritizedClipSetRequest::ReleaseClipSet(fwClipSet& rClipSet)
{
	//Assert that we have added a reference to the clip set.
	if(!streamPrioritizedVerifyf(HasAddedReferenceToClipSet(), "A reference has not been added to clip set with id: %s.", m_ClipSetId.GetCStr()))
	{
		return;
	}
	
	//Clear the flag.
	m_uRunningFlags.ClearFlag(RF_HasAddedReferenceToClipSet);

	//Release the clip set.
	rClipSet.Release_DEPRECATED();

	streamPrioritizedDebugf3("Prioritized clip set: %s was released.", m_ClipSetId.TryGetCStr());
}

bool CPrioritizedClipSetRequest::RequestClipSet(fwClipSet& rClipSet)
{
	//Generate the flags.
	s32 iFlags = STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE;
	
	//Check if the clip set is high priority.
	if(GetStreamingPriority() == SP_High)
	{
		iFlags |= STRFLAG_PRIORITY_LOAD;
	}

	//Note that we have set the don't delete flag.
	m_uRunningFlags.SetFlag(RF_HasSetDontDeleteFlag);
	
	//Request the clip set.
	if(!rClipSet.Request_DEPRECATED(iFlags))
	{
		return false;
	}

	return true;
}

void CPrioritizedClipSetRequest::StreamOutClipSet(fwClipSet& rClipSet)
{
	//Check if the clip dictionary index is valid.
	int iClipDictionaryIndex = rClipSet.GetClipDictionaryIndex().Get();
	if(streamPrioritizedVerifyf(iClipDictionaryIndex >= 0, "The clip dictionary index is invalid for clip set with id: %s.", m_ClipSetId.GetCStr()))
	{
		//Grab the streaming module id.
		s32 iModuleId = fwAnimManager::GetStreamingModuleId();
		
		//Grab the streaming index.
		strStreamingModule* pStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(iModuleId);
		strIndex index = pStreamingModule->GetStreamingIndex(strLocalIndex(iClipDictionaryIndex));
		
		//Check if the object is ready to be deleted.
		if(strStreamingEngine::GetInfo().IsObjectReadyToDelete(index, 0))
		{
			//Remove the object.
			strStreamingEngine::GetInfo().RemoveObject(index);
		}
	}
	
	//Check if a fallback clip set exists.
	fwClipSet* pFallbackClipSet = rClipSet.GetFallbackSet();
	if(pFallbackClipSet)
	{
		//Stream out the fallback clip set.
		StreamOutClipSet(*pFallbackClipSet);
	}
}

void CPrioritizedClipSetRequest::Update(fwClipSet& rClipSet)
{
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::CLIPSETSTREAMER);

	//Check if the clip set is not streamed in.
	bool bIsClipSetStreamedIn = IsClipSetStreamedIn(rClipSet);
	if(!bIsClipSetStreamedIn)
	{
		//Check if the clip set should be streamed in.
		if(ShouldClipSetBeStreamedIn())
		{
#if __BANK
			if (CPrioritizedClipSetRequestManager::ms_bSpewDetailedRequests2)
			{
				streamPrioritizedDebugf1("Frame : %i, CPrioritizedClipSetRequest::Update dictionary %s being requested", fwTimer::GetFrameCount(), rClipSet.GetClipDictionaryName().GetCStr());
			}
#endif // __BANK	
			//Request the clip set.
			bIsClipSetStreamedIn = RequestClipSet(rClipSet);
		}
#if __BANK
		if (CPrioritizedClipSetRequestManager::ms_bSpewDetailedRequests2)
		{
			streamPrioritizedDebugf1("Frame : %i, CPrioritizedClipSetRequest::Update dictionary %s NOT being requested (m_nDesiredStreamingState = %i)", fwTimer::GetFrameCount(), rClipSet.GetClipDictionaryName().GetCStr(), (s32)m_nDesiredStreamingState);
		}
#endif // __BANK	
	}
	else
	{
		//Check if the clip set should be streamed out.
		if(ShouldClipSetBeStreamedOut())
		{
#if __BANK
			if (CPrioritizedClipSetRequestManager::ms_bSpewDetailedRequests2)
			{
				streamPrioritizedDebugf1("Frame : %i, CPrioritizedClipSetRequest::Update dictionary %s being released (m_nDesiredStreamingState = %i)", fwTimer::GetFrameCount(), rClipSet.GetClipDictionaryName().GetCStr(), (s32)m_nDesiredStreamingState);
			}
#endif // __BANK

			//Check if we have set the don't delete flag.
			if(HasSetDontDeleteFlag())
			{
				//Clear the don't delete flag.
				ClearDontDeleteFlag(rClipSet);
			}

			//Check if we have added a reference to the clip set.
			if(HasAddedReferenceToClipSet())
			{
				//Release the clip set.
				ReleaseClipSet(rClipSet);
			}
			
			//Stream out the clip set.
			StreamOutClipSet(rClipSet);

			//Update the streamed in state, since it may have changed.
			bIsClipSetStreamedIn = IsClipSetStreamedIn(rClipSet);
		}
	}

#if __BANK
	if (CPrioritizedClipSetRequestManager::ms_bSpewDetailedRequests2)
	{
		streamPrioritizedDebugf1("Frame : %i, ClipsetStreamedIn ? %s, ShouldBeStreamedIn ? %s, HasAddedReference ? %s", fwTimer::GetFrameCount(), bIsClipSetStreamedIn ?"TRUE":"FALSE", ShouldClipSetBeStreamedIn() ?"TRUE":"FALSE", HasAddedReferenceToClipSet() ?"TRUE":"FALSE");
	}
#endif // __BANK	

	//Check if the clip set has been streamed in, should be streamed in, and we have not added a reference.
	if(bIsClipSetStreamedIn && ShouldClipSetBeStreamedIn() && !HasAddedReferenceToClipSet())
	{
		//Add a reference to the clip set.
		AddReferenceToClipSet(rClipSet);
	}

	//Clear the override.
	m_nStreamingPriorityOverride = SP_Invalid;
}

#if PRIORITIZED_CLIP_SET_STREAMER_WIDGETS
////////////////////////////////////////////////////////////////////////////////
// CPrioritizedClipSetBucketDebug
////////////////////////////////////////////////////////////////////////////////
#endif

////////////////////////////////////////////////////////////////////////////////
// CPrioritizedClipSetBucket
////////////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL(CPrioritizedClipSetBucket, CONFIGURED_FROM_FILE, atHashString("CPrioritizedClipSetBucket",0x6e126610));

CPrioritizedClipSetBucket::CPrioritizedClipSetBucket(atHashWithStringNotFinal hMemoryGroup)
: m_aRequests()
, m_hMemoryGroup(hMemoryGroup)
, m_MemoryBudget()
, m_MemoryUsage()
, m_uRunningFlags(0)
#if PRIORITIZED_CLIP_SET_STREAMER_WIDGETS
, m_Debug()
#endif
{

}

CPrioritizedClipSetBucket::~CPrioritizedClipSetBucket()
{
	//Iterate over the requests.
	int iCount = m_aRequests.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Destroy the request.
		DestroyRequest(i);
	}
}

CPrioritizedClipSetRequest* CPrioritizedClipSetBucket::AddRequest(fwMvClipSetId clipSetId)
{
	//Find the request for the clip set.
	CPrioritizedClipSetRequest* pRequest = FindRequest(clipSetId);
	if(!pRequest)
	{
		//Create a request.
		pRequest = CreateRequest(clipSetId);
		if(!streamPrioritizedVerifyf(pRequest, "Unable to create a request."))
		{
			return NULL;
		}
	}
	
	//Add a reference.
	pRequest->AddReference();
	
	//Sort the requests.
	m_uRunningFlags.SetFlag(RF_SortRequests);
	
	return pRequest;
}

void CPrioritizedClipSetBucket::Initialize()
{
	//Ensure the memory group metadata is valid.
	fwMemoryGroupMetadata* pMetadata = fwClipSetManager::GetMemoryGroupMetadata(m_hMemoryGroup);
	if(!streamPrioritizedVerifyf(pMetadata, "The metadata for memory group: %s is invalid.", m_hMemoryGroup.GetCStr()))
	{
		return;
	}
	
	//Get the memory budget.
	u32 uMemoryBudget = pMetadata->GetMemoryBudget();
	
	//The memory budget is defined in metadata as KB.  Transform it to bytes.
	uMemoryBudget = (uMemoryBudget << 10);
	
	//Set the initial memory budget.
	m_MemoryBudget.m_uInitial = uMemoryBudget;
}

void CPrioritizedClipSetBucket::RandomizeTiebreakers()
{
	//Iterate over the requests.
	int iCount = m_aRequests.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Grab the request.
		CPrioritizedClipSetRequest* pRequest = m_aRequests[i];
		
		//Randomize the tiebreaker.
		pRequest->RandomizeTiebreaker();
	}
	
	//Note that we should sort the requests.
	m_uRunningFlags.SetFlag(RF_SortRequests);
}

void CPrioritizedClipSetBucket::RemoveRequest(fwMvClipSetId clipSetId)
{
	//Ensure the index is valid.
	int iIndex = FindRequestIndex(clipSetId);
	if(!streamPrioritizedVerifyf(iIndex >= 0, "The index is invalid for clip set: %s.", clipSetId.GetCStr()))
	{
		return;
	}
	
	//Grab the request.
	CPrioritizedClipSetRequest* pRequest = m_aRequests[iIndex];
	
	//Remove a reference.
	pRequest->RemoveReference();
	
	//Check if the request is unreferenced.
	if(pRequest->IsUnreferenced())
	{
		//Destroy the request.
		DestroyRequest(pRequest);

		//Remove the request.
		m_aRequests.Delete(iIndex);
	}
}

void CPrioritizedClipSetBucket::Update()
{
	//Update the memory budget.
	UpdateMemoryBudget();
	
	//Update the requests.
	UpdateRequests();
}

bool CPrioritizedClipSetBucket::Compare(const CPrioritizedClipSetRequest* pRequest, const CPrioritizedClipSetRequest* pOtherRequest)
{
	//Check if the streaming priorities are different.
	eStreamingPriority nStreamingPriority = pRequest->GetStreamingPriority();
	eStreamingPriority nOtherStreamingPriority = pOtherRequest->GetStreamingPriority();
	if(nStreamingPriority != nOtherStreamingPriority)
	{
		return (nStreamingPriority > nOtherStreamingPriority);
	}
	
	//Check if the reference values are different.
	u8 uReferences = pRequest->GetReferences();
	u8 uOtherReferences = pOtherRequest->GetReferences();
	if(uReferences != uOtherReferences)
	{
		return (uReferences > uOtherReferences);
	}
	
	//Check if the tiebreaker values are different.
	u8 uTiebreaker = pRequest->GetTiebreaker();
	u8 uOtherTiebreaker = pOtherRequest->GetTiebreaker();
	if(uTiebreaker != uOtherTiebreaker)
	{
		return (uTiebreaker > uOtherTiebreaker);
	}
	
	//Compare pointer values.
	return (pRequest < pOtherRequest);
}

#if PRIORITIZED_CLIP_SET_STREAMER_WIDGETS
void CPrioritizedClipSetBucket::AddWidgets(bkGroup& rGroup)
{
	//Add the group.
	bkGroup* pGroup = rGroup.AddGroup(m_hMemoryGroup.GetCStr());
	
	bkGroup* pRenderingGroup = pGroup->AddGroup("Rendering");
	
		pRenderingGroup->AddToggle("Is Enabled", &m_Debug.m_Rendering.m_bIsEnabled);
		
	bkGroup* pBudgetGroup = pGroup->AddGroup("Budget");
	
		pBudgetGroup->AddToggle("Use Multiplier Override", &m_Debug.m_Budget.m_bUseMultiplierOverride);
		pBudgetGroup->AddSlider("Multiplier Override:", &m_Debug.m_Budget.m_fMultiplierOverride, 0.0f, 100.0f, 0.1f);
}

CPrioritizedClipSetRequest* CPrioritizedClipSetBucket::GetRandomRequest() const
{
	//Ensure the count is valid.
	int iCount = m_aRequests.GetCount();
	if(iCount <= 0)
	{
		return NULL;
	}
	
	//Generate a random index.
	int iIndex = fwRandom::GetRandomNumberInRange(0, iCount);
	
	return m_aRequests[iIndex];
}

void CPrioritizedClipSetBucket::RenderDebug(const Vector2& vPosition, int& iLine) const
{
#if DEBUG_DRAW
	//Ensure rendering is enabled.
	if(!m_Debug.m_Rendering.m_bIsEnabled)
	{
		return;
	}
	
	//Create a buffer.
	char buf[512];
	
	//Draw the title.
	u32 uMemoryUsage = m_MemoryUsage.m_uCurrent >> 10;
	u32 uMemoryBudget = m_MemoryBudget.m_uCurrent >> 10;
	Color32 color = (uMemoryUsage > uMemoryBudget) ? Color_red : Color_green;
	formatf(buf, "%s [Usage: %dK, Budget: %dK (%.2fx)]", m_hMemoryGroup.GetCStr(), uMemoryUsage, uMemoryBudget, m_MemoryBudget.m_fMultiplier);
	grcDebugDraw::PrintToScreenCoors(buf, (s32)vPosition.x, (s32)vPosition.y + iLine++, color);
	
	//Iterate over the requests.
	int iCount = m_aRequests.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Grab the request.
		const CPrioritizedClipSetRequest* pRequest = m_aRequests[i];
		
		//Ensure the clip dictionary index is valid.
		int iClipDictionaryIndex = pRequest->GetClipDictionaryIndex();
		if(!streamPrioritizedVerifyf(iClipDictionaryIndex >= 0, "The clip dictionary index is invalid for clip set id: %s.", pRequest->GetClipSetId().GetCStr()))
		{
			continue;
		}
		
		//Get the number of references.
		strLocalIndex tempStrLocalIndex(iClipDictionaryIndex);
		int iNumRefs = fwAnimManager::GetNumRefs(tempStrLocalIndex);
		
		//Grab the streaming states.
		bool bIsClipSetStreamedIn		= pRequest->IsClipSetStreamedIn();
		bool bShouldClipSetBeStreamedIn	= pRequest->ShouldClipSetBeStreamedIn();
		
		//Calculate the color.
		if(!bIsClipSetStreamedIn && bShouldClipSetBeStreamedIn)
		{
			color = Color_orange;
		}
		else if(bIsClipSetStreamedIn && !bShouldClipSetBeStreamedIn)
		{
			color = Color_yellow;
		}
		else if(bIsClipSetStreamedIn)
		{
			color = Color_green;
		}
		else
		{
			color = Color_red;
		}
		
		//Check if the request has added a reference to the clip set.
		bool bHasAddedReferenceToClipSet = pRequest->HasAddedReferenceToClipSet();
		
		//Calculate the memory cost.

		u32 uMemoryCost = CPrioritizedClipSetStreamer::GetInstance().CalculateMemoryCostForClipDictionary(tempStrLocalIndex);
		
		//Draw the request.
		formatf(buf, "%s [Cost: %dK] [Pri: %d, Refs: %d, Str: %s, Des: %s, Ref'd: %s] [Refs: %d]",
			pRequest->GetClipSetId().GetCStr(),
			uMemoryCost >> 10,
			pRequest->GetStreamingPriority(),
			pRequest->GetReferences(),
			bIsClipSetStreamedIn ? "Yes" : "No",
			bShouldClipSetBeStreamedIn ? "Yes" : "No",
			bHasAddedReferenceToClipSet ? "Yes" : "No",
			iNumRefs);
		grcDebugDraw::PrintToScreenCoors(buf, (s32)vPosition.x, (s32)vPosition.y + iLine++, color);
	}
#endif
}
#endif

CPrioritizedClipSetRequest* CPrioritizedClipSetBucket::CreateRequest(fwMvClipSetId clipSetId)
{
	//Ensure we have not exceeded the maximum requests.
	if(!streamPrioritizedVerifyf(!m_aRequests.IsFull(), "The requests are full."))
	{
		return NULL;
	}
	
	//Ensure there is room in the pool.
	if(!streamPrioritizedVerifyf(CPrioritizedClipSetRequest::GetPool()->GetNoOfFreeSpaces() > 0, "The request pool is full."))
	{
		return NULL;
	}
	
	//Create the request.
	CPrioritizedClipSetRequest* pRequest = rage_new CPrioritizedClipSetRequest(clipSetId);
	
	//Initialize the request.
	pRequest->Initialize();
	
	//Add the request.
	m_aRequests.Append() = pRequest;
	
	return pRequest;
}

void CPrioritizedClipSetBucket::DestroyRequest(int iIndex)
{
	//Destroy the request.
	DestroyRequest(m_aRequests[iIndex]);
}

void CPrioritizedClipSetBucket::DestroyRequest(CPrioritizedClipSetRequest* pRequest)
{
	//Ensure the request is valid.
	if(!pRequest)
	{
		return;
	}

	//Free the request.
	delete pRequest;
}

CPrioritizedClipSetRequest* CPrioritizedClipSetBucket::FindRequest(fwMvClipSetId clipSetId)
{
	return const_cast<CPrioritizedClipSetRequest *>(static_cast<const CPrioritizedClipSetBucket &>(*this).FindRequest(clipSetId));
}

const CPrioritizedClipSetRequest* CPrioritizedClipSetBucket::FindRequest(fwMvClipSetId clipSetId) const
{
	//Find the request index.
	int iIndex = FindRequestIndex(clipSetId);
	if(iIndex < 0)
	{
		return NULL;
	}

	return m_aRequests[iIndex];
}

int CPrioritizedClipSetBucket::FindRequestIndex(fwMvClipSetId clipSetId) const
{
	//Iterate over the requests.
	int iCount = m_aRequests.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the clip set matches.
		const CPrioritizedClipSetRequest* pRequest = m_aRequests[i];
		if(clipSetId != pRequest->GetClipSetId())
		{
			continue;
		}
		
		return i;
	}
	
	return -1;
}

float CPrioritizedClipSetBucket::GenerateMultiplierForMemoryBudget() const
{
#if PRIORITIZED_CLIP_SET_STREAMER_WIDGETS
	//Check if we should use the multiplier override.
	if(m_Debug.m_Budget.m_bUseMultiplierOverride)
	{
		return m_Debug.m_Budget.m_fMultiplierOverride;
	}
#endif

	return 1.0f;
}

void CPrioritizedClipSetBucket::SortRequests()
{
	//Ensure the request count is valid.
	int iCount = m_aRequests.GetCount();
	if(iCount <= 0)
	{
		return;
	}
	
	//Sort the requests.
	CPrioritizedClipSetRequest** pRequests = &m_aRequests[0];
	std::sort(pRequests, pRequests + iCount, CPrioritizedClipSetBucket::Compare);
}

void CPrioritizedClipSetBucket::UpdateMemoryBudget()
{
	//Calculate the adjusted budget.
	//This is done by applying the adjustment to the initial value.
	u32 uAdjusted = (u32)Max(0, ((s32)m_MemoryBudget.m_uInitial) + m_MemoryBudget.m_iAdjustment);

	//Generate the memory budget multiplier.
	float fMultiplier = GenerateMultiplierForMemoryBudget();
	m_MemoryBudget.m_fMultiplier = fMultiplier;
	
	//Calculate the current budget.
	u32 uCurrent = (u32)((float)uAdjusted * fMultiplier);
	m_MemoryBudget.m_uCurrent = uCurrent;
}

void CPrioritizedClipSetBucket::UpdateRequests()
{
	//Check if we should sort the requests.
	if(m_uRunningFlags.IsFlagSet(RF_SortRequests))
	{
		//Clear the flag.
		m_uRunningFlags.ClearFlag(RF_SortRequests);

		//Sort the requests.
		SortRequests();
	}
	
	//Calculate the current memory usage and budget.
	u32 uUsage	= m_MemoryUsage.m_uCurrent;
	u32 uBudget	= m_MemoryBudget.m_uCurrent;
	
	//Count the requests.
	int iCount = m_aRequests.GetCount();
	
	//The top of the request list gets highest priority.
	int iRequestToStreamIn = 0;
	
	//The bottom of the request list gets lowest priority.
	int iRequestToStreamOut = iCount - 1;

#if __BANK
	bool bShouldSpewDebug = false;
	if (CPrioritizedClipSetRequestManager::ms_bSpewDetailedRequests)
	{
		static atHashString MG_VEHICLE("MG_Vehicle");
		if (m_hMemoryGroup == MG_VEHICLE)
		{
			bShouldSpewDebug = true;
			CPrioritizedClipSetRequestManager::ms_bSpewDetailedRequests2 = true;
			streamPrioritizedDebugf1("Frame : %i, CPrioritizedClipSetBucket::UpdateRequests called for bucket processing memory group %s, usage %u, budget %u, count %i", fwTimer::GetFrameCount(), MG_VEHICLE.GetCStr(), uUsage, uBudget, iCount);
		}
	}
#endif // __BANK
	
	//Iterate over the requests.
	while(iRequestToStreamIn <= iRequestToStreamOut)
	{
		//Grab the request to stream in.
		CPrioritizedClipSetRequest* pRequestToStreamIn = m_aRequests[iRequestToStreamIn];
		
		//Note: At some point we will probably want to take into account hits to other memory groups that will occur
		//		when streaming in a clip set, due to the fallback clip sets.  This is really complicated, though...

		//Check if the clip set is valid.
		fwClipSet* pClipSetToStreamIn = pRequestToStreamIn->GetClipSet();
		if(streamPrioritizedVerifyf(pClipSetToStreamIn, "The clip set with id: %s is invalid.", pRequestToStreamIn->GetClipSetId().GetCStr()))
		{
			//Calculate the cost to stream in the clip set.
			u32 uCostToStreamIn = CPrioritizedClipSetStreamer::GetInstance().CalculateMemoryCostForClipSetToStreamIn(*pClipSetToStreamIn, m_hMemoryGroup);

			//Check if the clip set is high priority.
			bool bIsHighPriority = (pRequestToStreamIn->GetStreamingPriority() == SP_High);

			//Check if the clip sets is within the budget.
			bool bIsWithinBudget = (uUsage + uCostToStreamIn <= uBudget);

#if __BANK
			if (bShouldSpewDebug)
			{
				streamPrioritizedDebugf1("Frame : %i, Evaluating clipset %s, high priority ? %s, within budget ? %s", fwTimer::GetFrameCount(), pRequestToStreamIn->GetClipSetId().GetCStr(), bIsHighPriority ? "TRUE":"FALSE", bIsWithinBudget ? "TRUE":"FALSE");
			}
#endif // __BANK

			//Check if the clip set should be streamed in.
			if(bIsHighPriority || bIsWithinBudget)
			{
				//Stream in the clip set.
				pRequestToStreamIn->SetDesiredStreamingState(CPrioritizedClipSetRequest::DSS_ShouldBeStreamedIn);

				//Update the request.
				pRequestToStreamIn->Update(*pClipSetToStreamIn);

				//Increase the usage.
				uUsage += uCostToStreamIn;

				//Move to the next request.
				++iRequestToStreamIn;
			}
			else
			{
				//Grab the request to stream out.
				CPrioritizedClipSetRequest* pRequestToStreamOut = m_aRequests[iRequestToStreamOut];

				//Check if the clip set is valid.
				fwClipSet* pClipSetToStreamOut = pRequestToStreamOut->GetClipSet();
				if(streamPrioritizedVerifyf(pClipSetToStreamOut, "The clip set with id: %s is invalid.", pRequestToStreamOut->GetClipSetId().GetCStr()))
				{
					//Check if the clip dictionary is counted against the memory cost.
					strLocalIndex iClipDictionaryIndexToStreamOut = pClipSetToStreamOut->GetClipDictionaryIndex();
					bool bIsClipDictionaryToStreamOutCountedAgainstMemoryCost = (iClipDictionaryIndexToStreamOut.Get() >= 0) ? CPrioritizedClipSetStreamer::GetInstance().IsClipDictionaryCountedAgainstMemoryCost(iClipDictionaryIndexToStreamOut.Get()) : false;

					//Stream out the clip set.
					pRequestToStreamOut->SetDesiredStreamingState(CPrioritizedClipSetRequest::DSS_ShouldBeStreamedOut);

					//Update the request.
					pRequestToStreamOut->Update(*pClipSetToStreamOut);

					//Check if the clip dictionary was counted against the memory cost.
					if(bIsClipDictionaryToStreamOutCountedAgainstMemoryCost)
					{
						//Check if the clip dictionary should not be counted against the memory cost.
						bool bShouldClipDictionaryToStreamOutCountAgainstMemoryCost = CPrioritizedClipSetStreamer::GetInstance().ShouldClipDictionaryCountAgainstMemoryCost(iClipDictionaryIndexToStreamOut);
						if(!bShouldClipDictionaryToStreamOutCountAgainstMemoryCost)
						{
							//Calculate the refund.
							u32 uRefund = CPrioritizedClipSetStreamer::GetInstance().CalculateMemoryCostForClipDictionary(iClipDictionaryIndexToStreamOut);

							//Decrease the usage.
							streamPrioritizedAssertf(uUsage >= uRefund, "The refund: %d exceeds the usage: %d.", uRefund, uUsage);
							uUsage -= uRefund;
						}
					}

					//Move to the previous request.
					--iRequestToStreamOut;
				}
				else
				{
					//Move to the previous request.
					--iRequestToStreamOut;
				}
			}
		}
		else
		{
			//Move to the next request.
			++iRequestToStreamIn;
		}
	}

#if __BANK
	if (bShouldSpewDebug)
	{
		CPrioritizedClipSetRequestManager::ms_bSpewDetailedRequests2 = false;
	}
#endif // __BANK
}

#if PRIORITIZED_CLIP_SET_STREAMER_WIDGETS
////////////////////////////////////////////////////////////////////////////////
// CPrioritizedClipSetStreamerDebug
////////////////////////////////////////////////////////////////////////////////

void CPrioritizedClipSetStreamerDebug::Metadata::DumpMissingToOutput()
{
	//Display the title.
	Displayf("Clip dictionaries with missing metadata:");

	//Iterate over the clip dictionaries.
	int iCount = fwAnimManager::GetSize();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the slot is valid.
		strLocalIndex tempStrLocalIndex(i);
		if(!fwAnimManager::IsValidSlot(tempStrLocalIndex))
		{
			continue;
		}

		//Ensure the clip dictionary metadata is valid.
		atHashString hName = fwAnimManager::FindHashKeyFromSlot(tempStrLocalIndex);
		fwClipDictionaryMetadata* pMetadata = fwClipSetManager::GetClipDictionaryMetadata(hName);
		if(pMetadata)
		{
			continue;
		}

		//Display the name.
		Displayf("  %s", hName.GetCStr());
	}
}

void CPrioritizedClipSetStreamerDebug::RequestController::Individual::AddRequest()
{
	//Add the request.
	CPrioritizedClipSetStreamer::GetInstance().AddRequest(m_ClipSelector.GetSelectedClipSetId());
}

void CPrioritizedClipSetStreamerDebug::RequestController::Individual::RemoveRequest()
{
	//Remove the request.
	CPrioritizedClipSetStreamer::GetInstance().RemoveRequest(m_ClipSelector.GetSelectedClipSetId());
}

void CPrioritizedClipSetStreamerDebug::RequestController::Bulk::AddRandomRequests()
{
	//Count the clip sets.
	int iClipSetCount = fwClipSetManager::GetClipSetCount();

	//Iterate over the amount.
	for(int i = 0; i < m_iAmount; ++i)
	{
		//Generate a random index.
		int iIndex = fwRandom::GetRandomNumberInRange(0, iClipSetCount);

		//Grab the clip set.
		fwMvClipSetId clipSetId = fwClipSetManager::GetClipSetIdByIndex(iIndex);

		//Add the request.
		CPrioritizedClipSetStreamer::GetInstance().AddRequest(clipSetId);
	}
}

void CPrioritizedClipSetStreamerDebug::RequestController::Bulk::RemoveRandomRequests()
{
	//Iterate over the amount.
	for(int i = 0; i < m_iAmount; ++i)
	{
		//Generate a random bucket.
		CPrioritizedClipSetBucket* pBucket = CPrioritizedClipSetStreamer::GetInstance().GetRandomBucket();
		if(!pBucket)
		{
			continue;
		}

		//Generate a random request.
		CPrioritizedClipSetRequest* pRequest = pBucket->GetRandomRequest();
		if(!pRequest)
		{
			continue;
		}

		//Remove all references.
		u8 uReferences = pRequest->GetReferences();
		for(u8 i = 0; i < uReferences; ++i)
		{
			//Remove the request.
			CPrioritizedClipSetStreamer::GetInstance().RemoveRequest(pRequest->GetClipSetId());
		}
	}
}

#endif

////////////////////////////////////////////////////////////////////////////////
// CPrioritizedClipSetStreamer
////////////////////////////////////////////////////////////////////////////////

CPrioritizedClipSetStreamer* CPrioritizedClipSetStreamer::sm_Instance = NULL;

CPrioritizedClipSetStreamer::CPrioritizedClipSetStreamer()
: m_IsClipDictionaryCountedAgainstMemoryCost()
, m_aBuckets()
, m_fTimeSinceLastTiebreakersRandomization(0.0f)
, m_iClipDictionaryIndexToCheck(0)
, m_hSituation()
#if PRIORITIZED_CLIP_SET_STREAMER_WIDGETS
, m_Debug()
#endif
{

}

CPrioritizedClipSetStreamer::~CPrioritizedClipSetStreamer()
{
#if PRIORITIZED_CLIP_SET_STREAMER_WIDGETS
	//Remove the widgets.
	RemoveWidgets();
#endif

	//Iterate over the buckets.
	int iCount = m_aBuckets.GetCount();
	for(int i = iCount-1; i >= 0; --i)
	{
		delete m_aBuckets[i];
		m_aBuckets[i] = NULL;
	}
}

CPrioritizedClipSetRequest* CPrioritizedClipSetStreamer::AddRequest(fwMvClipSetId clipSetId)
{
	//Ensure the clip set is valid.
	if(!streamPrioritizedVerifyf(clipSetId != CLIP_SET_ID_INVALID, "The clip set is invalid."))
	{
		return NULL;
	}
	
	//Find the bucket for the clip set.
	CPrioritizedClipSetBucket* pBucket = FindBucket(clipSetId);
	if(!streamPrioritizedVerifyf(pBucket, "Unable to find bucket for clip set: %s.", clipSetId.GetCStr()))
	{
		return NULL;
	}
	
	//Add the request.
	return pBucket->AddRequest(clipSetId);
}

u32 CPrioritizedClipSetStreamer::CalculateMemoryCostForClipDictionary(strLocalIndex iIndex) const
{
	//Assert that the slot is valid.
	streamPrioritizedAssertf(fwAnimManager::IsValidSlot(strLocalIndex(iIndex)), "The slot is invalid: %d.", iIndex.Get());

	//Grab the streaming module id.
	s32 iModuleId = fwAnimManager::GetStreamingModuleId();

	//Ensure the object is in the image.
	if(!CStreaming::IsObjectInImage(iIndex, iModuleId))
	{
		return 0;
	}

	//Grab the streaming index.
	strIndex index = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(iModuleId)->GetStreamingIndex(strLocalIndex(iIndex));

	//Get the cost.
	u32 uCost = strStreamingEngine::GetInfo().GetObjectVirtualSize(index, true);

	return uCost;
}

u32 CPrioritizedClipSetStreamer::CalculateMemoryCostForClipSetToStreamIn(const fwClipSet& rClipSet, atHashWithStringNotFinal hMemoryGroup) const
{
	//Keep track of the cost.
	u32 uCostToStreamIn = 0;

	//Iterate over the clip sets.
	const fwClipSet* pClipSet = &rClipSet;
	while(pClipSet)
	{
		//Check if the clip dictionary index is valid.
		strLocalIndex iClipDictionaryIndex = pClipSet->GetClipDictionaryIndex();
		if(streamPrioritizedVerifyf(iClipDictionaryIndex.Get() >= 0, "The clip dictionary index for clip set with clip dictionary name: %s is invalid.", pClipSet->GetClipDictionaryName().GetCStr()))
		{
			//Check if the clip dictionary is not counted against the memory cost.
			if(!IsClipDictionaryCountedAgainstMemoryCost(iClipDictionaryIndex.Get()))
			{
				//Check if the memory group matches.
				if(GetMemoryGroupForClipDictionary(iClipDictionaryIndex) == hMemoryGroup)
				{
					//Calculate the cost to stream in.
					uCostToStreamIn += CalculateMemoryCostForClipDictionary(iClipDictionaryIndex);
				}
			}
		}

		//Ensure the fallback clip set id is valid.
		fwMvClipSetId fallbackClipSetId = pClipSet->GetFallbackId();
		if(fallbackClipSetId == CLIP_SET_ID_INVALID)
		{
			break;
		}

		//Ensure the fallback clip set is valid.
		pClipSet = pClipSet->GetFallbackSet();
		if(!streamPrioritizedVerifyf(pClipSet, "The clip set with id: %s is invalid.", fallbackClipSetId.GetCStr()))
		{
			break;
		}
	}

	return uCostToStreamIn;
}
const CPrioritizedClipSetRequest* CPrioritizedClipSetStreamer::FindRequest(fwMvClipSetId clipSetId) const
{
	//Ensure the bucket is valid.
	const CPrioritizedClipSetBucket* pBucket = FindBucket(clipSetId);
	if(!pBucket)
	{
		return NULL;
	}

	return (pBucket->FindRequest(clipSetId));
}

atHashWithStringNotFinal CPrioritizedClipSetStreamer::GetMemoryGroupForClipDictionary(strLocalIndex iIndex) const
{
	//Assert that the slot is valid.
	streamPrioritizedAssertf(fwAnimManager::IsValidSlot(iIndex), "The slot is invalid: %d.", iIndex.Get());

	//Ensure the metadata is valid.
	atHashString hName = fwAnimManager::FindHashKeyFromSlot(iIndex);
	fwClipDictionaryMetadata* pMetadata = fwClipSetManager::GetClipDictionaryMetadata(hName);

	return pMetadata ? pMetadata->GetMemoryGroup() : s_hDefaultMemoryGroup;
}

void CPrioritizedClipSetStreamer::Initialize()
{
	//Initialize the buckets.
	InitializeBuckets();

	//Initialize the memory costs.
	InitializeMemoryCosts();
}

void CPrioritizedClipSetStreamer::OnClipDictionaryAllRefsReset(strLocalIndex iIndex)
{
	//Update the memory cost for the clip dictionary.
	UpdateMemoryCostForClipDictionary(iIndex);
}

void CPrioritizedClipSetStreamer::OnClipDictionaryLoaded(strLocalIndex iIndex)
{
	//Update the memory cost for the clip dictionary.
	UpdateMemoryCostForClipDictionary(iIndex);
}

void CPrioritizedClipSetStreamer::OnClipDictionaryRemoved(strLocalIndex iIndex)
{
	//Update the memory cost for the clip dictionary.
	UpdateMemoryCostForClipDictionary(iIndex);
}

void CPrioritizedClipSetStreamer::OnClipDictionaryRefAdded(strLocalIndex iIndex)
{
	//Update the memory cost for the clip dictionary.
	UpdateMemoryCostForClipDictionary(iIndex);
}

void CPrioritizedClipSetStreamer::OnClipDictionaryRefRemoved(strLocalIndex iIndex)
{
	//Update the memory cost for the clip dictionary.
	UpdateMemoryCostForClipDictionary(iIndex);
}

void CPrioritizedClipSetStreamer::OnClipDictionaryRefRemovedWithoutDelete(strLocalIndex iIndex)
{
	//Update the memory cost for the clip dictionary.
	UpdateMemoryCostForClipDictionary(iIndex);
}

void CPrioritizedClipSetStreamer::RemoveRequest(fwMvClipSetId clipSetId)
{
	//Ensure the clip set is valid.
	if(!streamPrioritizedVerifyf(clipSetId != CLIP_SET_ID_INVALID, "The clip set is invalid."))
	{
		return;
	}
	
	//Ensure the index is valid.
	int iIndex = FindBucketIndex(clipSetId);
	if(!streamPrioritizedVerifyf(iIndex >= 0, "The index is invalid for clip set: %s.", clipSetId.GetCStr()))
	{
		return;
	}
	
	//Grab the bucket.
	CPrioritizedClipSetBucket* pBucket = m_aBuckets[iIndex];
	
	//Remove the request.
	pBucket->RemoveRequest(clipSetId);
}

bool CPrioritizedClipSetStreamer::ShouldClipDictionaryCountAgainstMemoryCost(strLocalIndex iIndex) const
{
	//Assert that the index is valid.
	animAssertf(fwAnimManager::IsValidSlot(strLocalIndex(iIndex)), "The index is invalid: %d.", iIndex.Get());

	//Ensure the number of references is valid.
	int iNumRefs = fwAnimManager::GetNumRefs(strLocalIndex(iIndex));
	if(iNumRefs <= 0)
	{
		return false;
	}

	//Grab the streaming module id.
	s32 iModuleId = fwAnimManager::GetStreamingModuleId();

	//Ensure the object has loaded.
	if(!CStreaming::HasObjectLoaded(iIndex, iModuleId))
	{
		return false;
	}

	return true;
}

void CPrioritizedClipSetStreamer::Update(float fTimeStep)
{
	//Update the timers.
	UpdateTimers(fTimeStep);

	//Update the situation.
	UpdateSituation();

	//Update the memory costs.
	UpdateMemoryCosts();
	
	//Update the buckets.
	UpdateBuckets();
}

void CPrioritizedClipSetStreamer::Init(unsigned UNUSED_PARAM(initMode))
{
	//Init the pools.
	CPrioritizedClipSetBucket::InitPool(MEMBUCKET_GAMEPLAY);
	CPrioritizedClipSetRequest::InitPool(MEMBUCKET_GAMEPLAY);

	//Create the instance.
	Assert(!sm_Instance);
	sm_Instance = rage_new CPrioritizedClipSetStreamer;

	//Initialize the streamer.
	sm_Instance->Initialize();
}

void CPrioritizedClipSetStreamer::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	//Free the instance.
	Assert(sm_Instance);
	delete sm_Instance;
	sm_Instance = NULL;

	//Shutdown the pools.
	CPrioritizedClipSetBucket::ShutdownPool();
	CPrioritizedClipSetRequest::ShutdownPool();
}

void CPrioritizedClipSetStreamer::Update()
{
	if (!fwTimer::IsGamePaused())
	{
		//Grab the time step.
		float fTimeStep = fwTimer::GetTimeStep();

		//Update the instance.
		GetInstance().Update(fTimeStep);
	}
#if !__FINAL
	else
	{
		streamPrioritizedDebugf2("Frame : %i, Unable to process CPrioritizedClipSetStreamer because we think the game is paused", fwTimer::GetFrameCount());
		streamPrioritizedDebugf2("fwTimer::IsGamePaused() ? %s, fwTimer::IsDebugPause() ? %s", fwTimer::IsGamePaused() ? "TRUE" : "FALSE", fwTimer::IsDebugPause() ? "TRUE" : "FALSE");
	}
#endif // !__FINAL
}

#if PRIORITIZED_CLIP_SET_STREAMER_WIDGETS
void CPrioritizedClipSetStreamer::AddWidgets(bkBank& rBank)
{
	//Ensure the group is invalid.
	if(!streamPrioritizedVerifyf(!m_Debug.m_pGroup, "The group is valid."))
	{
		return;
	}
	
	m_Debug.m_pGroup = rBank.PushGroup("Prioritized Clip Set Streamer");

		rBank.PushGroup("Rendering (requires debug text display [b])");

			rBank.PushGroup("Requests");

				rBank.AddToggle("Is Enabled", &m_Debug.m_Rendering.m_Requests.m_bIsEnabled);

			rBank.PopGroup();

			rBank.PushGroup("Clip Dictionaries");

				rBank.AddToggle("Counted Against Cost", &m_Debug.m_Rendering.m_ClipDictionaries.m_bCountedAgainstCost);
				rBank.AddToggle("Not Counted Against Cost", &m_Debug.m_Rendering.m_ClipDictionaries.m_bNotCountedAgainstCost);
				rBank.AddText("Memory Group Name Filter", m_Debug.m_Rendering.m_ClipDictionaries.m_aMemoryGroupNameFilter, sizeof(m_Debug.m_Rendering.m_ClipDictionaries.m_aMemoryGroupNameFilter), false);

			rBank.PopGroup();

			rBank.AddSlider("Horizontal Scroll:", &m_Debug.m_Rendering.m_vScroll.x, 0.0f, 100.0f, 1.0f);
			rBank.AddSlider("Vertical Scroll:", &m_Debug.m_Rendering.m_vScroll.y, -500.0f, 0.0f, 1.0f);

		rBank.PopGroup();
		
		bkGroup* pGroupForBuckets = rBank.PushGroup("Buckets");

			int iCount = m_aBuckets.GetCount();
			for(int i = 0; i < iCount; ++i)
			{
				m_aBuckets[i]->AddWidgets(*pGroupForBuckets);
			}
		
		rBank.PopGroup();

		rBank.PushGroup("Metadata");

			rBank.AddButton("Dump Missing To Output", datCallback(MFA(CPrioritizedClipSetStreamerDebug::Metadata::DumpMissingToOutput), &m_Debug.m_Metadata));

		rBank.PopGroup();

		rBank.PushGroup("Request Controller");
		
			rBank.PushGroup("Individual");

				m_Debug.m_RequestController.m_Individual.m_ClipSelector.SetShowClipSetClipNames(false);
				m_Debug.m_RequestController.m_Individual.m_ClipSelector.AddWidgets(&rBank);
				
				rBank.AddButton("Add Request", datCallback(MFA(CPrioritizedClipSetStreamerDebug::RequestController::Individual::AddRequest), &m_Debug.m_RequestController.m_Individual));
				rBank.AddButton("Remove Request", datCallback(MFA(CPrioritizedClipSetStreamerDebug::RequestController::Individual::RemoveRequest), &m_Debug.m_RequestController.m_Individual));
				
			rBank.PopGroup();
			
			rBank.PushGroup("Bulk");
			
				rBank.AddSlider("Amount:", &m_Debug.m_RequestController.m_Bulk.m_iAmount, 1, 128, 1);
				rBank.AddButton("Add Random Requests", datCallback(MFA(CPrioritizedClipSetStreamerDebug::RequestController::Bulk::AddRandomRequests), &m_Debug.m_RequestController.m_Bulk));
				rBank.AddButton("Remove Random Requests", datCallback(MFA(CPrioritizedClipSetStreamerDebug::RequestController::Bulk::RemoveRandomRequests), &m_Debug.m_RequestController.m_Bulk));
			
			rBank.PopGroup();

		rBank.PopGroup();

	rBank.PopGroup();
}

CPrioritizedClipSetBucket* CPrioritizedClipSetStreamer::GetRandomBucket() const
{
	//Ensure the count is valid.
	int iCount = m_aBuckets.GetCount();
	if(iCount <= 0)
	{
		return NULL;
	}
	
	//Generate a random index.
	int iIndex = fwRandom::GetRandomNumberInRange(0, iCount);
	
	return m_aBuckets[iIndex];
}

void CPrioritizedClipSetStreamer::RemoveWidgets()
{
	//Ensure the group is valid.
	bkGroup* pGroup = m_Debug.m_pGroup;
	if(!pGroup)
	{
		return;
	}
	
	//Grab the bank.
	bkBank* pBank = static_cast<bkBank *>(pGroup->GetParent());
	
	//Remove the widgets for the clip selector.
	m_Debug.m_RequestController.m_Individual.m_ClipSelector.RemoveWidgets(pBank);
	m_Debug.m_RequestController.m_Individual.m_ClipSelector.Shutdown();
	
	//Destroy the group.
	pGroup->Destroy();
}

void CPrioritizedClipSetStreamer::RenderDebug() const
{
#if DEBUG_DRAW
	//Initialize the position.
	static Vector2 s_vInitialPosition(10.0f, 10.0f);
	Vector2 vPosition(s_vInitialPosition);
	vPosition += m_Debug.m_Rendering.m_vScroll;

	//Keep track of the line.
	int iLine = 0;

	//Create a buffer.
	char buf[512];

	//Check if we should render requests.
	if(m_Debug.m_Rendering.m_Requests.m_bIsEnabled)
	{
		//Draw the title.
		formatf(buf, "Prioritized Clip Set Requests");
		grcDebugDraw::PrintToScreenCoors(buf, (s32)vPosition.x, (s32)vPosition.y + iLine++, Color_white);
		
		//Iterate over the buckets.
		int iCount = m_aBuckets.GetCount();
		for(int i = 0; i < iCount; ++i)
		{
			//Grab the bucket.
			const CPrioritizedClipSetBucket* pBucket = m_aBuckets[i];

			//Render the debug text for the bucket.
			pBucket->RenderDebug(vPosition, iLine);

			//Check if the bucket has requests, or this is the last bucket.
			bool bIsLastBucket = (i + 1 == iCount);
			if(pBucket->HasRequests() || bIsLastBucket)
			{
				//Add an empty line.
				++iLine;
			}
		}
	}

	//Check if we should render the streamed in clip dictionaries.
	bool bShouldRenderClipDictionariesCountedAgainstCost = m_Debug.m_Rendering.m_ClipDictionaries.m_bCountedAgainstCost;
	bool bShouldRenderClipDictionariesNotCountedAgainstCost = m_Debug.m_Rendering.m_ClipDictionaries.m_bNotCountedAgainstCost;
	if(bShouldRenderClipDictionariesCountedAgainstCost || bShouldRenderClipDictionariesNotCountedAgainstCost)
	{
		//Draw the title.
		formatf(buf, "Clip Dictionaries");
		grcDebugDraw::PrintToScreenCoors(buf, (s32)vPosition.x, (s32)vPosition.y + iLine++, Color_white);

		//Hash the memory group filter.
		atHashString hMemoryGroupFilter = m_Debug.m_Rendering.m_ClipDictionaries.m_aMemoryGroupNameFilter;

		//Iterate over the clip dictionaries.
		int iCount = fwAnimManager::GetSize();
		for(int i = 0; i < iCount; ++i)
		{
			//Ensure the slot is valid.
			strLocalIndex tempStrLocalIndex(i);
			if(!fwAnimManager::IsValidSlot(tempStrLocalIndex))
			{
				continue;
			}

			//Ensure the memory group passes the filter.
			atHashWithStringNotFinal hMemoryGroup = GetMemoryGroupForClipDictionary(tempStrLocalIndex);
			if(hMemoryGroupFilter.IsNotNull() && (hMemoryGroup != hMemoryGroupFilter))
			{
				continue;
			}

			//Check if the clip dictionary is counted against the cost.
			bool bCountedAgainstCost = IsClipDictionaryCountedAgainstMemoryCost(i);
			if((bCountedAgainstCost && bShouldRenderClipDictionariesCountedAgainstCost) ||
				(!bCountedAgainstCost && bShouldRenderClipDictionariesNotCountedAgainstCost))
			{
				//Render the clip dictionary.
			}
			else
			{
				continue;
			}

			//Grab the name.
			atHashString hName = fwAnimManager::FindHashKeyFromSlot(tempStrLocalIndex);

			//Calculate the cost.
			u32 uCost = CalculateMemoryCostForClipDictionary(tempStrLocalIndex);
			uCost = uCost >> 10;

			//Calculate the number of references.
			int iNumRefs = fwAnimManager::GetNumRefs(tempStrLocalIndex);

			//Draw the streamed in clip dictionary.
			formatf(buf, "%s (%s) [Cost: %dK] [Refs: %d]", hName.GetCStr(), hMemoryGroup.GetCStr(), uCost, iNumRefs);
			grcDebugDraw::PrintToScreenCoors(buf, (s32)vPosition.x, (s32)vPosition.y + iLine++, bCountedAgainstCost ? Color_green : Color_red);
		}
	}
#endif
}
#endif

void CPrioritizedClipSetStreamer::ApplyMemoryBudgetAdjustments()
{
	//Ensure the situation is valid.
	if(m_hSituation.IsNull())
	{
		return;
	}

	//Ensure the memory situation is valid.
	const fwMemorySituation* pMemorySituation = fwClipSetManager::GetMemorySituation(m_hSituation);
	if(!streamPrioritizedVerifyf(pMemorySituation, "The memory situation: %s is invalid.", m_hSituation.GetCStr()))
	{
		return;
	}

	//First, we need to clear all of the adjustments.
	//Iterate over the buckets.
	int iNumBuckets = m_aBuckets.GetCount();
	for(int i = 0; i < iNumBuckets; ++i)
	{
		//Grab the bucket.
		CPrioritizedClipSetBucket* pBucket = m_aBuckets[i];

		//Clear the memory budget adjustment.
		pBucket->SetMemoryBudgetAdjustment(0);
	}

	//Apply the adjustments.
	int iNumAdjustments = pMemorySituation->m_Adjustments.GetCount();
	for(int i = 0; i < iNumAdjustments; ++i)
	{
		//Grab the adjustment.
		const fwMemorySituation::Adjustment& rAdjustment = pMemorySituation->m_Adjustments[i];

		//Ensure the bucket is valid.
		CPrioritizedClipSetBucket* pBucket = FindBucket(rAdjustment.m_MemoryGroup);
		if(!streamPrioritizedVerifyf(pBucket, "The memory group: %s is invalid.", rAdjustment.m_MemoryGroup.GetCStr()))
		{
			continue;
		}

		//The amount is defined in metadata as KB.  Transform it to bytes.
		int iAmount = (rAdjustment.m_Amount << 10);

		//Set the memory budget adjustments.
		pBucket->SetMemoryBudgetAdjustment(iAmount);
	}
}

atHashWithStringNotFinal CPrioritizedClipSetStreamer::CalculateSituation() const
{
	//Check if we are in combat.
	if(CGameSituation::GetInstance().GetFlags().IsFlagSet(CGameSituation::Combat))
	{
		static atHashWithStringNotFinal s_hCombat("Combat",0x57779727);
		return s_hCombat;
	}
	else
	{
		static atHashWithStringNotFinal s_hDefault("Default",0xE4DF46D5);
		return s_hDefault;
	}
}

bool CPrioritizedClipSetStreamer::CanUpdateSituation() const
{
	//Ensure the game is not paused.
	if(fwTimer::IsGamePaused())
	{
		return false;
	}

	return true;
}

CPrioritizedClipSetBucket* CPrioritizedClipSetStreamer::CreateBucket(atHashWithStringNotFinal hMemoryGroup)
{
	//Ensure we have not exceeded the maximum buckets.
	if(!streamPrioritizedVerifyf(!m_aBuckets.IsFull(), "The buckets are full."))
	{
		return NULL;
	}

	//Ensure there is room in the pool.
	if(!streamPrioritizedVerifyf(CPrioritizedClipSetBucket::GetPool()->GetNoOfFreeSpaces() > 0, "The bucket pool is full."))
	{
		return NULL;
	}
	
	//Create the bucket.
	CPrioritizedClipSetBucket* pBucket = rage_new CPrioritizedClipSetBucket(hMemoryGroup);
	
	//Initialize the bucket.
	pBucket->Initialize();
	
	//Add the bucket.
	m_aBuckets.Append() = pBucket;
	
	return pBucket;
}

CPrioritizedClipSetBucket* CPrioritizedClipSetStreamer::FindBucket(fwMvClipSetId clipSetId)
{
	return const_cast<CPrioritizedClipSetBucket *>(static_cast<const CPrioritizedClipSetStreamer &>(*this).FindBucket(clipSetId));
}

const CPrioritizedClipSetBucket* CPrioritizedClipSetStreamer::FindBucket(fwMvClipSetId clipSetId) const
{
	return (FindBucket(GetMemoryGroup(clipSetId)));
}

CPrioritizedClipSetBucket* CPrioritizedClipSetStreamer::FindBucket(atHashWithStringNotFinal hMemoryGroup)
{
	return const_cast<CPrioritizedClipSetBucket *>(static_cast<const CPrioritizedClipSetStreamer &>(*this).FindBucket(hMemoryGroup));
}

const CPrioritizedClipSetBucket* CPrioritizedClipSetStreamer::FindBucket(atHashWithStringNotFinal hMemoryGroup) const
{
	//Find the bucket index.
	int iIndex = FindBucketIndex(hMemoryGroup);
	if(iIndex < 0)
	{
		return NULL;
	}
	
	return m_aBuckets[iIndex];
}

int CPrioritizedClipSetStreamer::FindBucketIndex(fwMvClipSetId clipSetId) const
{
	return (FindBucketIndex(GetMemoryGroup(clipSetId)));
}

int CPrioritizedClipSetStreamer::FindBucketIndex(atHashWithStringNotFinal hMemoryGroup)	const
{
	//Ensure the memory group is valid.
	if(hMemoryGroup.IsNull())
	{
		return -1;
	}
	
	//Iterate over the buckets.
	int iCount = m_aBuckets.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the memory group matches.
		const CPrioritizedClipSetBucket* pBucket = m_aBuckets[i];
		if(hMemoryGroup != pBucket->GetMemoryGroup())
		{
			continue;
		}
		
		return i;
	}
	
	return -1;
}

atHashString CPrioritizedClipSetStreamer::GetMemoryGroup(fwMvClipSetId clipSetId) const
{
	//Ensure the clip set is valid.
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if(!streamPrioritizedVerifyf(pClipSet, "The clip set: %s is invalid.", clipSetId.GetCStr()))
	{
		return s_hDefaultMemoryGroup;
	}
	
	//Ensure the metadata is valid.
	fwClipDictionaryMetadata* pClipDictionaryMetadata = pClipSet->GetClipDictionaryMetadata();

	return pClipDictionaryMetadata ? pClipDictionaryMetadata->GetMemoryGroup() : s_hDefaultMemoryGroup;
}

void CPrioritizedClipSetStreamer::InitializeBuckets()
{
	//Iterate over the memory groups.
	int iCount = fwClipSetManager::GetMemoryGroupMetadataCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the memory group metadata is valid.	
		fwMemoryGroupMetadata* pMemoryGroupMetadata = fwClipSetManager::GetMemoryGroupMetadataByIndex(i);
		if(!pMemoryGroupMetadata)
		{
			continue;
		}

		//Ensure the memory group is valid. 
		atHashString* pMemoryGroup = fwClipSetManager::GetMemoryGroupNameByIndex(i);
		if(!pMemoryGroup)
		{
			continue;
		}

		//Create the bucket.
		CreateBucket(*pMemoryGroup);
	}
}

void CPrioritizedClipSetStreamer::InitializeMemoryCosts()
{
	//Count the clip dictionaries.
	int iMaxCount = fwAnimManager::GetMaxSize();
	int iCount = fwAnimManager::GetSize();

	//Clear the flags.
	m_IsClipDictionaryCountedAgainstMemoryCost.Init(iMaxCount);
	m_IsClipDictionaryCountedAgainstMemoryCost.Reset(0);

	//Iterate over the clip dictionaries.
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the slot is valid.
		if(!fwAnimManager::IsValidSlot(strLocalIndex(i)))
		{
			continue;
		}

		//Update the memory cost for the clip dictionary.
		UpdateMemoryCostForClipDictionary(strLocalIndex(i));
	}
}

void CPrioritizedClipSetStreamer::OnSituationChanged()
{
	//Apply the memory budget adjustments.
	ApplyMemoryBudgetAdjustments();
}

void CPrioritizedClipSetStreamer::RandomizeTiebreakers()
{
	//Iterate over the buckets.
	int iCount = m_aBuckets.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Grab the bucket.
		CPrioritizedClipSetBucket* pBucket = m_aBuckets[i];

		//Randomize the tiebreakers.
		pBucket->RandomizeTiebreakers();
	}

	//Clear the time since we randomized the tiebreakers.
	m_fTimeSinceLastTiebreakersRandomization = 0.0f;
}

bool CPrioritizedClipSetStreamer::ShouldRandomizeTiebreakers() const
{
	//Ensure the timer has exceeded the threshold.
	static float s_fTimeBetweenTiebreakersRandomizations = 30.0f;
	if(m_fTimeSinceLastTiebreakersRandomization < s_fTimeBetweenTiebreakersRandomizations)
	{
		return false;
	}
	
	return true;
}

void CPrioritizedClipSetStreamer::UpdateBuckets()
{
	//Check if we should randomize the tiebreakers.
	if(ShouldRandomizeTiebreakers())
	{
		//Randomize the tiebreakers.
		RandomizeTiebreakers();
	}
	
	//Iterate over the buckets.
	int iCount = m_aBuckets.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Update the bucket.
		m_aBuckets[i]->Update();
	}
}

void CPrioritizedClipSetStreamer::UpdateMemoryCostForClipDictionary(strLocalIndex iIndex)
{
	//Ensure we are not patching clip sets.  This is an epic hack due to how the clip set patcher works.
	//Clip sets can be loaded in before their metadata is available, causing a memory group mismatch when cost is added/removed.
	//The clip set cost should be picked up when UpdateMemoryCosts is called.
	if(fwClipSetManager::IsPatchingClipSets())
	{
		return;
	}

	//Assert that the index is valid.
	animAssertf(fwAnimManager::IsValidSlot(iIndex), "The index is invalid: %d.", iIndex.Get());

	//Ensure the 'count against cost' state is changing.
	bool bShouldCountAgainstCost = ShouldClipDictionaryCountAgainstMemoryCost(iIndex);
	bool bIsCountedAgainstCost = IsClipDictionaryCountedAgainstMemoryCost(iIndex.Get());
	if(bShouldCountAgainstCost == bIsCountedAgainstCost)
	{
		return;
	}

	//Get the memory group for the clip dictionary.
	atHashWithStringNotFinal hMemoryGroup = GetMemoryGroupForClipDictionary(iIndex);

	//Ensure the bucket is valid.
	CPrioritizedClipSetBucket* pBucket = FindBucket(hMemoryGroup);
	if(!streamPrioritizedVerifyf(pBucket, "The bucket is invalid for clip dictionary: %s with memory group: %s.", atHashWithStringNotFinal(fwAnimManager::FindHashKeyFromSlot(iIndex)).GetCStr(), hMemoryGroup.GetCStr()))
	{
		return;
	}

	//Calculate the cost.
	u32 uCost = CalculateMemoryCostForClipDictionary(iIndex);

	//Check if the clip dictionary should count against the cost.
	if(bShouldCountAgainstCost)
	{
		//Increase the memory usage.
		pBucket->IncreaseMemoryUsage(uCost);

		streamPrioritizedDebugf2("Increased memory usage by: %d for bucket: %s resulting in a usage of: %d due to index: %d.", uCost, pBucket->GetMemoryGroup().TryGetCStr(), pBucket->GetMemoryUsage(), iIndex.Get());

		//Set the flag.
		m_IsClipDictionaryCountedAgainstMemoryCost.Set(iIndex.Get());
	}
	else
	{
		//Decrease the memory usage.
		pBucket->DecreaseMemoryUsage(uCost);

		streamPrioritizedDebugf2("Decreased memory usage by: %d for bucket: %s resulting in a usage of: %d due to index: %d.", uCost, pBucket->GetMemoryGroup().TryGetCStr(), pBucket->GetMemoryUsage(), iIndex.Get());

		//Clear the flag.
		m_IsClipDictionaryCountedAgainstMemoryCost.Clear(iIndex.Get());
	}
}

void CPrioritizedClipSetStreamer::UpdateMemoryCosts()
{
	//Note: This function was added as a bit of a safety net for the clip dictionary callback system
	//		not working 100% as intended.  In theory, the callback system should track when clip dictionaries
	//		are streamed in and out, and gain/lose references.  Unfortunately, there are situations when this
	//		is not the case.  One particular situation is when clips add/remove a reference to their owning clip
	//		dictionary, without going through the clip dictionary store.

	//Grab the clip dictionary count.
	int iCount = fwAnimManager::GetSize();

	//Iterate over a few of the clip dictionaries per frame.
	static const int s_iNumClipDictionariesToCheckPerFrame = 50;
	for(int i = 0; i < s_iNumClipDictionariesToCheckPerFrame; ++i)
	{
		//Calculate the index.
		strLocalIndex iIndex = strLocalIndex(m_iClipDictionaryIndexToCheck);
		m_iClipDictionaryIndexToCheck = (m_iClipDictionaryIndexToCheck + 1) % iCount;

		//Ensure the slot is valid.
		if(!fwAnimManager::IsValidSlot(iIndex))
		{
			continue;
		}

		//Update the memory cost for the clip dictionary.
		UpdateMemoryCostForClipDictionary(iIndex);
	}
}

void CPrioritizedClipSetStreamer::UpdateSituation()
{
	//Ensure we can update the situation.
	if(!CanUpdateSituation())
	{
		return;
	}

	//Calculate the situation.
	atHashWithStringNotFinal hSituation = CalculateSituation();

	//Ensure the situation is changing.
	if(hSituation == m_hSituation)
	{
		return;
	}

	//Assign the situation.
	m_hSituation = hSituation;

	//Note that the situation changed.
	OnSituationChanged();
}

void CPrioritizedClipSetStreamer::UpdateTimers(float fTimeStep)
{
	//Update the timers.
	m_fTimeSinceLastTiebreakersRandomization += fTimeStep;
}

////////////////////////////////////////////////////////////////////////////////
// CPrioritizedClipSetRequestManager
////////////////////////////////////////////////////////////////////////////////

bool CPrioritizedClipSetRequestManager::ms_SortRequests = true;

#if __DEV
u32 CPrioritizedClipSetRequestManager::ms_TotalRequestAllocationSize = 0;
u32 CPrioritizedClipSetRequestManager::ms_TotalHelperAllocationSize = 0;
#endif // __DEV

#if __BANK
bool CPrioritizedClipSetRequestManager::ms_bSpewDetailedRequests = false;
bool CPrioritizedClipSetRequestManager::ms_bSpewDetailedRequests2 = false;
#endif // __BANK

// Statics
CPrioritizedClipSetRequestManager::Tunables CPrioritizedClipSetRequestManager::ms_Tunables;

IMPLEMENT_VEHICLE_TASK_TUNABLES(CPrioritizedClipSetRequestManager, 0x061c3aa1);

CPrioritizedClipSetRequestManager* CPrioritizedClipSetRequestManager::ms_Instance = NULL;

#if DEBUG_DRAW
const char* CPrioritizedClipSetRequestManager::szContextNames[] = { REQUEST_CONTEXTS(DEFINE_STRINGS_FROM_LIST) };
#endif // DEBUG_DRAW

////////////////////////////////////////////////////////////////////////////////

CPrioritizedClipSetRequestManager::CPrioritizedClipSetRequestManager()
: m_aClipSetRequests()
, m_aClipSetHelpers()
{

}

////////////////////////////////////////////////////////////////////////////////

CPrioritizedClipSetRequestManager::~CPrioritizedClipSetRequestManager()
{

}

////////////////////////////////////////////////////////////////////////////////

#if DEBUG_DRAW
void CPrioritizedClipSetRequestManager::sRequest::RenderDebug(const Vector2& vPosition, int& iLine, bool bClipSetLoaded) const
{
	//Create a buffer.
	char buf[512];

	//Draw the title.
	atString streamingPriority;
	eStreamingPriority_ToString(priority, streamingPriority);
	formatf(buf, "ClipSet: %s, Total Requests: %i, Lifetime: %.2f, Priority: %s", clipsetId.GetCStr(), iNumRequests, fRequestLifeTime, streamingPriority.c_str());
	grcDebugDraw::PrintToScreenCoors(buf, (s32)vPosition.x, (s32)vPosition.y + iLine++, bClipSetLoaded ? Color_green : Color_orange);
}

////////////////////////////////////////////////////////////////////////////////

void CPrioritizedClipSetRequestManager::RenderHelperDebug(const Vector2& vPosition, int& iLine, const CPrioritizedClipSetRequestHelper& helper) const
{
	//Create a buffer.
	char buf[512];

	//Draw the title.
	formatf(buf, "ClipSet Requested: %s", helper.GetClipSetId().GetCStr());
	grcDebugDraw::PrintToScreenCoors(buf, (s32)vPosition.x, (s32)vPosition.y + iLine++, Color_purple);
}
#endif // DEBUG_DRAW

////////////////////////////////////////////////////////////////////////////////

void CPrioritizedClipSetRequestManager::Init(unsigned UNUSED_PARAM(initMode))
{
	//Create the instance.
	Assert(!ms_Instance);
	ms_Instance = rage_new CPrioritizedClipSetRequestManager;

	//Initialize the streamer.
	ms_Instance->Init();
}

////////////////////////////////////////////////////////////////////////////////

void CPrioritizedClipSetRequestManager::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	if (ms_Instance)
	{
		ms_Instance->Shutdown();
		delete ms_Instance;
		ms_Instance = NULL;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CPrioritizedClipSetRequestManager::Compare(const sRequest* pRequest, const sRequest* pOtherRequest)
{
	return pRequest->clipsetId.GetHash() < pOtherRequest->clipsetId.GetHash();
}

////////////////////////////////////////////////////////////////////////////////

void CPrioritizedClipSetRequestManager::Init()
{
	// For each context
	for (s32 context=0; context<RC_Max; ++context)
	{
		m_aClipSetRequests.Push(sClipSetRequests());
		m_aClipSetHelpers.Push(sClipSetHelpers());
	}
}

////////////////////////////////////////////////////////////////////////////////

void CPrioritizedClipSetRequestManager::Shutdown()
{
	// For each context
	for (s32 context=0; context<RC_Max; ++context)
	{
		for(int request = 0; request<m_aClipSetRequests[context].aRequests.GetCount(); ++request)
		{
			delete m_aClipSetRequests[context].aRequests[request];
		}
		for(int helper = 0; helper<m_aClipSetHelpers[context].aPrioritisedClipSetRequestHelpers.GetCount(); ++helper)
		{
			delete m_aClipSetHelpers[context].aPrioritisedClipSetRequestHelpers[helper];
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CPrioritizedClipSetRequestManager::ShouldClipSetBeStreamedOut(eRequestContext context, fwMvClipSetId clipSetId)
{
	s32 iExistingRequestHelperIndex = GetRequestHelperIndexIfExists(context, clipSetId);
	if (iExistingRequestHelperIndex > -1)
	{
		return m_aClipSetHelpers[context].aPrioritisedClipSetRequestHelpers[iExistingRequestHelperIndex]->ShouldClipSetBeStreamedOut();
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CPrioritizedClipSetRequestManager::UpdateRequest(sRequest& rOldRequest, const sRequest& rNewRequest)
{
	// Use the highest lifetime
	if (rNewRequest.fRequestLifeTime > rOldRequest.fRequestLifeTime)
	{
		rOldRequest.fRequestLifeTime = rNewRequest.fRequestLifeTime;
	}

	// Always keep high priority requests
	if (rNewRequest.priority == SP_High)
	{
		rOldRequest.priority = SP_High;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CPrioritizedClipSetRequestManager::Request(sRequest& request, bool bCheckLoaded)
{
#if __DEV
	ms_TotalRequestAllocationSize += sizeof(sRequest);
#endif // __DEV

	bool bAddedRequest = false;

	if (aiVerifyf(request.clipsetId != CLIP_SET_ID_INVALID, "Clipset requested is invalid"))
	{
		if (aiVerifyf(m_aClipSetRequests[request.context].aRequests.GetCount() < ms_Tunables.m_MaxNumRequestsPerContext, "Trying to request more than %i clipsets for context %s, check request calls", ms_Tunables.m_MaxNumRequestsPerContext, szContextNames[request.context]))
		{
			m_aClipSetRequests[request.context].aRequests.PushAndGrow(&request);
			bAddedRequest = true;

			if (ms_SortRequests)
			{
				SortRequests(request.context);
			}
		}
	}

	if (!bAddedRequest)
	{
		delete &request;
	}

#if __BANK
	TUNE_GROUP_BOOL(STREAMING_TUNE, ENABLE_GET_CLIPSET_OPTIMIZATION, true);
	TUNE_GROUP_BOOL(STREAMING_TUNE, ENABLE_FIND_REQUEST_OPTIMIZATION, true);
	if (!ENABLE_FIND_REQUEST_OPTIMIZATION)
	{
		ms_SortRequests = false;
	}

	if (bCheckLoaded || !ENABLE_GET_CLIPSET_OPTIMIZATION)
#else
	if (bCheckLoaded)
#endif // __BANK
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(request.clipsetId);
		if (pClipSet)
		{
			return pClipSet->IsStreamedIn_DEPRECATED();
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CPrioritizedClipSetRequestManager::IsLoaded(fwMvClipSetId clipSetId)
{
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if (aiVerifyf(pClipSet, "No clipset found for clipsetid %s", clipSetId.GetCStr()))
	{
		return pClipSet->IsStreamedIn_DEPRECATED();
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CPrioritizedClipSetRequestManager::Update(float fTimeStep)
{
#if __BANK
	if (ms_bSpewDetailedRequests)
	{
		streamPrioritizedDebugf1("Frame : %i, CPrioritizedClipSetRequestManager::Update called", fwTimer::GetFrameCount());
	}
#endif // __BANK

	// For each context
	for (s32 context=0; context<RC_Max; ++context)
	{
		// Score and sort requests
		//ScoreAndSortRequests((eRequestContext)context);

		// Remove duplicate clipset requests (TODO: keep highest scoring ones)
		//RemoveDuplicatesFromSortedArray((eRequestContext)context);

#if __DEV
		CheckForDuplicates((eRequestContext)context);
#endif // __DEV

		// Update the lifetimes of the requests and remove any requests/helpers that have expired
		ProcessRequests((eRequestContext)context, fTimeStep);

		// Add a priority clipset helper if it doesn't already exist and request the clipset
		AddRequestHelperIfNoneExists((eRequestContext)context);
		
		// TODO: Alter priorities based on scores
	}
}

////////////////////////////////////////////////////////////////////////////////

#if DEBUG_DRAW
void CPrioritizedClipSetRequestManager::RenderDebug()
{
	if (!ms_Tunables.m_RenderDebugDraw)
		return;

	//Initialize the position.
	static Vector2 s_vInitialPosition(0.0f, 0.0f);
	Vector2 vPosition(s_vInitialPosition);
	vPosition += ms_Tunables.m_vScroll;

	//Keep track of the line.
	int iLine = 0;

	//Create a buffer.
	char buf[512];

#if __DEV
	formatf(buf, "Total Request Allocations : %u", ms_TotalRequestAllocationSize);
	grcDebugDraw::PrintToScreenCoors(buf, (s32)vPosition.x, (s32)vPosition.y + iLine++, Color_white);
	iLine++;
	formatf(buf, "Total Helper Allocations : %u", ms_TotalHelperAllocationSize);
	grcDebugDraw::PrintToScreenCoors(buf, (s32)vPosition.x, (s32)vPosition.y + iLine++, Color_white);
	iLine++;
	iLine++;
#endif // __DEV

	//Draw the title.
	formatf(buf, "Prioritized ClipSet Request Manager");
	grcDebugDraw::PrintToScreenCoors(buf, (s32)vPosition.x, (s32)vPosition.y + iLine++, Color_cyan);
	iLine++;

	vPosition.x += ms_Tunables.m_fIndent;

	//Iterate over the requests for each context
	for(int i = 0; i < RC_Max; ++i)
	{
		int iRequestCount = m_aClipSetRequests[i].aRequests.GetCount();

		Vector2 vContextPosition = vPosition;
		vContextPosition.x += ms_Tunables.m_fIndent;
		formatf(buf, "Context : %s", szContextNames[i]);
		grcDebugDraw::PrintToScreenCoors(buf, (s32)vContextPosition.x, (s32)vPosition.y + iLine++, Color_blue);

		formatf(buf, "ClipSet Requests (%i)", iRequestCount);
		grcDebugDraw::PrintToScreenCoors(buf, (s32)vContextPosition.x, (s32)vPosition.y + iLine++, Color_white);

		Vector2 vRequestPosition = vContextPosition;
		vRequestPosition.x += ms_Tunables.m_fIndent;

		for(int request = 0; request< iRequestCount; ++request)
		{
			//Grab the request.
			const sRequest& rRequest = *m_aClipSetRequests[i].aRequests[request];

			//Render the debug text for the request.
			rRequest.RenderDebug(vRequestPosition, iLine, IsLoaded(rRequest.clipsetId));
		}

		int iHelperCount = m_aClipSetHelpers[i].aPrioritisedClipSetRequestHelpers.GetCount();

		formatf(buf, "Priority ClipSet Helpers (%i)", iHelperCount);
		grcDebugDraw::PrintToScreenCoors(buf, (s32)vContextPosition.x, (s32)vPosition.y + iLine++, Color_white);

		Vector2 vHelperPosition = vContextPosition;
		vHelperPosition.x += ms_Tunables.m_fIndent;

		for(int helper = 0; helper< iHelperCount; ++helper)
		{
			//Grab the helper.
			const CPrioritizedClipSetRequestHelper& rHelper = *m_aClipSetHelpers[i].aPrioritisedClipSetRequestHelpers[helper];

			//Render the debug text for the helper.
			RenderHelperDebug(vHelperPosition, iLine, rHelper);

		}
	}
}
#endif // DEBUG_DRAW

////////////////////////////////////////////////////////////////////////////////

// void CPrioritizedClipSetRequestManager::ScoreAndSortRequests(eRequestContext context)
// {
// 	// Ensure the request count is valid.
// 	s32 iCount = m_aClipSetRequests[context].aRequests.GetCount();
// 	if (iCount <= 1)
// 	{
// 		return;
// 	}
// 
// 	// TODO: Score requests based on:
// 	// - Distance/Angle to current gameplayer camera
// 	// - Whether requesting entity is a player/mission ped?
// 	// - Requested priority
// 	// ScoreRequests();
// 
// 	// Sort the requests by clipset hash so its easier to remove dupes
// 	sRequest** pRequests = &m_aClipSetRequests[context].aRequests[0];
// 	std::sort(pRequests, pRequests + iCount, CPrioritizedClipSetRequestManager::Compare);
// }
// 
// ////////////////////////////////////////////////////////////////////////////////
// 
// void CPrioritizedClipSetRequestManager::RemoveDuplicatesFromSortedArray(eRequestContext context)
// {
// 	const s32 iTotalNumElements = m_aClipSetRequests[context].aRequests.GetCount();
// 	if (iTotalNumElements > 1)
// 	{
// 		// Count the number of unique elements, our array should be sized to it once we've removed dupes
// 		u32 uNumUniqueElements = 0;
// 
// 		// Go through the sorted array, comparing each element with the first unique element not already processed (uNumUniqueElements)
// 		// if the other request isn't a dupe, its a unique element so we have finished processing this element and move onto the next
// 		// We update the unique element we're processing with the details of current element we're accessing
// 		// Once we've looped through the array we should have removed all duplicates and uNumUniqueElements will hold the required
// 		// new size of the array.
// 		s32 iNumDuplicatesFound = 0;
// 
// 		for (s32 i=1; i<iTotalNumElements; ++i)
// 		{
// 			if (!IsOtherRequestADupe(context, uNumUniqueElements, i))
// 			{	
// 				iNumDuplicatesFound = 0;
// 				++uNumUniqueElements;
// 			}
// 			else
// 			{
// 				++iNumDuplicatesFound;
// 			}
// 			CopyDetailsToUniqueElement(context, uNumUniqueElements, i, iNumDuplicatesFound);
// 		}
// 
// 		// Add one since we start at zero
// 		++uNumUniqueElements;
// 
// 		// If we found at least one dupe, go through all the dupes and delete them
// 		if (uNumUniqueElements < iTotalNumElements)
// 		{
// 			while (m_aClipSetRequests[context].aRequests.GetCount() > uNumUniqueElements)
// 			{
// #if __DEV
// 				ms_TotalRequestAllocationSize -= sizeof(sRequest);
// #endif // __DEV
// 				delete m_aClipSetRequests[context].aRequests[uNumUniqueElements];
// 				m_aClipSetRequests[context].aRequests.Delete(uNumUniqueElements);
// 			}
// 		}
// 
// 		streamPrioritizedAssertf(uNumUniqueElements == (u32)m_aClipSetRequests[context].aRequests.GetCount(), "Remove duplicates failed, Num Unique Elements : %i, Num Elements : %i", uNumUniqueElements, m_aClipSetRequests[context].aRequests.GetCount());
// 	}	
// }

////////////////////////////////////////////////////////////////////////////////

#if __DEV
void CPrioritizedClipSetRequestManager::CheckForDuplicates(eRequestContext context) const
{
	const s32 iTotalNumElements = m_aClipSetRequests[context].aRequests.GetCount();
	if (iTotalNumElements > 1)
	{
		// Count the number of unique elements, our array should be sized to it once we've removed dupes
		u32 uNumUniqueElements = 0;

		// Go through the sorted array, comparing each element with the first unique element not already processed (uNumUniqueElements)
		// if the other request isn't a dupe, its a unique element so we have finished processing this element and move onto the next
		s32 iNumDuplicatesFound = 0;

		for (s32 i=1; i<iTotalNumElements; ++i)
		{
			if (!IsOtherRequestADupe(context, uNumUniqueElements, i))
			{	
				iNumDuplicatesFound = 0;
				++uNumUniqueElements;
			}
			else
			{
				streamPrioritizedAssertf(!IsOtherRequestADupe(context, uNumUniqueElements, i), "Duplicate request of ClipSet %s found", m_aClipSetRequests[context].aRequests[uNumUniqueElements]->clipsetId.GetCStr());
				++iNumDuplicatesFound;
			}
		}
	}	
}
#endif // __DEV

////////////////////////////////////////////////////////////////////////////////

bool CPrioritizedClipSetRequestManager::IsOtherRequestADupe(eRequestContext context, u32 uCurrentUniqueElementIndex, s32 iOtherRequestIndex) const
{
	sRequest& rCurrentRequest = *m_aClipSetRequests[context].aRequests[uCurrentUniqueElementIndex];
	sRequest& rOtherRequest = *m_aClipSetRequests[context].aRequests[iOtherRequestIndex];
	if (rCurrentRequest.clipsetId == rOtherRequest.clipsetId)
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CPrioritizedClipSetRequestManager::CopyDetailsToUniqueElement(eRequestContext context, u32 uCurrentUniqueElementIndex, s32 iUniqueRequestIndex, s32 iNumDuplicatesFound)
{
	sRequest& rCurrentRequest = *m_aClipSetRequests[context].aRequests[uCurrentUniqueElementIndex];
	sRequest& rOtherRequest = *m_aClipSetRequests[context].aRequests[iUniqueRequestIndex];

	// This is the first instance, copy all details
	if (iNumDuplicatesFound == 0)
	{
		rCurrentRequest = rOtherRequest;
	}
	else
	{
		UpdateRequest(rCurrentRequest, rOtherRequest);
		rCurrentRequest.iNumRequests = iNumDuplicatesFound;
	}
}

////////////////////////////////////////////////////////////////////////////////

fwClipSet* CPrioritizedClipSetRequestManager::GetClipSet(fwMvClipSetId clipSetId) const
{
	//Ensure the clip set is valid.
	if(!streamPrioritizedVerifyf(clipSetId != CLIP_SET_ID_INVALID, "The clip set is invalid."))
	{
		return NULL;
	}

	return fwClipSetManager::GetClipSet(clipSetId);
}

////////////////////////////////////////////////////////////////////////////////

bool CPrioritizedClipSetRequestManager::Request(eRequestContext context, fwMvClipSetId clipSetId, eStreamingPriority priority, float fLifeTime, bool bCheckLoaded)
{
	streamPrioritizedAssertf(clipSetId != CLIP_SET_ID_INVALID, "Invalid clipset passed in");

	// If there's already an existing request, just update it with a request created on the stack, otherwise allocate a new request
	s32 iExistingRequestIndex = CPrioritizedClipSetRequestManager::GetInstance().GetRequestIndexIfExists(context, clipSetId);
	if (iExistingRequestIndex > -1)
	{
		CPrioritizedClipSetRequestManager::GetInstance().UpdateRequest(context, iExistingRequestIndex, CPrioritizedClipSetRequestManager::sRequest(clipSetId, context, priority, fLifeTime > -1.0f ? fLifeTime : 1.0f));
		return bCheckLoaded ? CPrioritizedClipSetRequestManager::GetInstance().IsLoaded(clipSetId) : false;
	}
	else
	{
		return CPrioritizedClipSetRequestManager::GetInstance().Request(*rage_new CPrioritizedClipSetRequestManager::sRequest(clipSetId, context, priority, fLifeTime > -1.0f ? fLifeTime : 1.0f), bCheckLoaded);
	}
}

////////////////////////////////////////////////////////////////////////////////

s32	CPrioritizedClipSetRequestManager::GetRequestIndexIfExists(eRequestContext context, fwMvClipSetId clipSetId)
{
	const s32 iTotalNumElements = m_aClipSetRequests[context].aRequests.GetCount();
	if (iTotalNumElements > 0)
	{
		TUNE_GROUP_INT(CLIPSET_STREAMING_TUNE, MIN_CLIPSETS_TO_BINARY_SEARCH, 20, 0, 100, 1);
		if (ms_SortRequests && iTotalNumElements > MIN_CLIPSETS_TO_BINARY_SEARCH)
		{
			// m_aClipSetRequests is a sorted array, and we use
			// std::lower_bound() to do a binary search here.
			sRequest key;
			key.clipsetId = clipSetId;
			sRequest** pRequests = &m_aClipSetRequests[context].aRequests[0];
			sRequest** found = std::lower_bound(pRequests, pRequests+iTotalNumElements, &key, CPrioritizedClipSetRequestManager::Compare);
			const int index = (int)(found - pRequests);
			if (index >= 0 && index < iTotalNumElements)
			{
				return m_aClipSetRequests[context].aRequests[index]->clipsetId == clipSetId ? index : -1;
			}
		}
		else
		{
			for (s32 i=0; i<iTotalNumElements; ++i)
			{
				if (m_aClipSetRequests[context].aRequests[i]->clipsetId == clipSetId)
				{	
					return i;
				}
			}
		}
	}	
	return -1;
}

////////////////////////////////////////////////////////////////////////////////

bool CPrioritizedClipSetRequestManager::HasClipSetBeenRequested(eRequestContext context, fwMvClipSetId clipSetId)
{
	return GetRequestIndexIfExists(context,clipSetId) >= 0;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
const CPrioritizedClipSetRequestHelper* CPrioritizedClipSetRequestManager::GetPrioritizedClipSetRequestHelperForClipSet(eRequestContext context, fwMvClipSetId clipSetId)
{
	s32 iRequestHelperIndex = GetRequestHelperIndexIfExists(context, clipSetId);
	if (iRequestHelperIndex > -1)
	{
		return m_aClipSetHelpers[context].aPrioritisedClipSetRequestHelpers[iRequestHelperIndex];
	}
	return NULL;
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

s32 CPrioritizedClipSetRequestManager::GetRequestHelperIndexIfExists(eRequestContext context, fwMvClipSetId clipSetId)
{
	const atArray<CPrioritizedClipSetRequestHelper*>& clipSetRequestHelperArray = m_aClipSetHelpers[context].aPrioritisedClipSetRequestHelpers;
	for (s32 i=0; i<clipSetRequestHelperArray.GetCount(); ++i)
	{
		if (clipSetRequestHelperArray[i]->GetClipSetId() == clipSetId)
		{
			return i;
		}
	}
	return -1;
}

////////////////////////////////////////////////////////////////////////////////

void CPrioritizedClipSetRequestManager::ProcessRequests(eRequestContext context, float fTimeStep)
{
	if (m_aClipSetRequests[context].aRequests.GetCount() > 0)
	{
		// Go through each request (from end to start) and update the life time
		for (s32 request=m_aClipSetRequests[context].aRequests.GetCount()-1; request>=0; --request)
		{
			m_aClipSetRequests[context].aRequests[request]->fRequestLifeTime -= fTimeStep;

			// Request has expired, so remove
			if (m_aClipSetRequests[context].aRequests[request]->fRequestLifeTime < 0.0f)
			{
#if __BANK
				if (ms_bSpewDetailedRequests)
				{
					streamPrioritizedDebugf1("Frame : %i, request expired clipset %s", fwTimer::GetFrameCount(), m_aClipSetRequests[context].aRequests[request]->clipsetId.GetCStr());
				}
#endif // __BANK
#if __DEV
				ms_TotalRequestAllocationSize -= sizeof(sRequest);
#endif // __DEV
				// Delete the request
				delete m_aClipSetRequests[context].aRequests[request];
				// Remove the request from the array
				m_aClipSetRequests[context].aRequests.Delete(request);
			}
#if __BANK
			else
			{
				if (ms_bSpewDetailedRequests)
				{
					streamPrioritizedDebugf1("Frame : %i, valid request found clipset %s", fwTimer::GetFrameCount(), m_aClipSetRequests[context].aRequests[request]->clipsetId.GetCStr());
				}
			}
#endif // __BANK
		}
	}

	if (m_aClipSetHelpers[context].aPrioritisedClipSetRequestHelpers.GetCount() > 0)
	{
		// Go through each helper (from end to start) and see if we have a corresponding request for that clipset
		// we remove the helper if we don't have a request for that clipset
		for (s32 helper=m_aClipSetHelpers[context].aPrioritisedClipSetRequestHelpers.GetCount()-1; helper>=0; --helper)
		{
			fwMvClipSetId clipSetId = m_aClipSetHelpers[context].aPrioritisedClipSetRequestHelpers[helper]->GetClipSetId();
			if (!FindRequestWithClipSetId(context, clipSetId))
			{
#if __BANK
				if (ms_bSpewDetailedRequests)
				{
					streamPrioritizedDebugf1("Frame : %i, removing clipset helper for clipset %s because no requests for that clipset exist", fwTimer::GetFrameCount(), clipSetId.GetCStr());
				}
#endif // __BANK
#if __DEV
				ms_TotalHelperAllocationSize -= sizeof(CPrioritizedClipSetRequestHelper);
#endif // __DEV
				// Delete the prioritised clipset helper
				delete m_aClipSetHelpers[context].aPrioritisedClipSetRequestHelpers[helper];
				// Remove from the array
				m_aClipSetHelpers[context].aPrioritisedClipSetRequestHelpers.Delete(helper);
			}
#if __BANK
			else
			{
				if (ms_bSpewDetailedRequests)
				{
					streamPrioritizedDebugf1("Frame : %i, found request for clipset %s, keeping clipset helper", fwTimer::GetFrameCount(), clipSetId.GetCStr());
				}
			}
#endif // __BANK
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CPrioritizedClipSetRequestManager::AddRequestHelperIfNoneExists(eRequestContext context)
{
	if (m_aClipSetRequests[context].aRequests.GetCount() > 0)
	{
		// Go through each request
		for (s32 request=0; request<m_aClipSetRequests[context].aRequests.GetCount(); ++request)
		{
			CPrioritizedClipSetRequestHelper* pPriorityRequest = NULL;

			s32 iRequestHelperIndex = GetRequestHelperIndexIfExists((eRequestContext)context, m_aClipSetRequests[context].aRequests[request]->clipsetId);
			// Add a clipset helper if one doesn't exist
			if (iRequestHelperIndex == -1)
			{
#if __BANK
				if (ms_bSpewDetailedRequests)
				{
					streamPrioritizedDebugf1("Frame : %i, couldn't find request for clipset %s", fwTimer::GetFrameCount(), m_aClipSetRequests[context].aRequests[request]->clipsetId.GetCStr());
				}
#endif // __BANK
#if __DEV
				ms_TotalHelperAllocationSize += sizeof(CPrioritizedClipSetRequestHelper);
#endif // __DEV
				pPriorityRequest = rage_new CPrioritizedClipSetRequestHelper();
				pPriorityRequest->RequestClipSet(m_aClipSetRequests[context].aRequests[request]->clipsetId);
				m_aClipSetHelpers[context].aPrioritisedClipSetRequestHelpers.PushAndGrow(pPriorityRequest);
			}
			else
			{
				pPriorityRequest = m_aClipSetHelpers[context].aPrioritisedClipSetRequestHelpers[iRequestHelperIndex];
			}

			// Force the request to be high priority if specified
			if (pPriorityRequest)
			{
#if __BANK
				// Verify the request is valid
				streamPrioritizedAssertf(pPriorityRequest->GetClipSetId() == m_aClipSetRequests[context].aRequests[request]->clipsetId, "Clipset (%s) in priority request doesn't match the clipset in request helper (%s), Tez would like to see this", pPriorityRequest->GetClipSetId().GetCStr(), m_aClipSetRequests[context].aRequests[request]->clipsetId.GetCStr());
#endif // __BANK
				eStreamingPriority desiredPriority = m_aClipSetRequests[context].aRequests[request]->priority;
#if __BANK
				if (ms_bSpewDetailedRequests)
				{
					const CPrioritizedClipSetRequest* pRequest = pPriorityRequest->GetPrioritizedClipSetRequest();
					streamPrioritizedDebugf1("Frame : %i, found request for clipset %s, priority (desired:%i, normal:%i, override:%i) , stream status %i", fwTimer::GetFrameCount(), m_aClipSetRequests[context].aRequests[request]->clipsetId.GetCStr(), (s32)(desiredPriority), (s32)(pRequest->GetStreamingPriority()), (s32)(pRequest->GetStreamingPriorityOverride()), (s32)(pRequest->GetDesiredStreamingState()));
				}
#endif // __BANK
				if (desiredPriority != SP_Invalid)
				{
					pPriorityRequest->SetStreamingPriorityOverride(desiredPriority);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

s32 CPrioritizedClipSetRequestManager::FindRequestWithClipSetId(eRequestContext context, fwMvClipSetId clipSetId)
{
	if (m_aClipSetRequests[context].aRequests.GetCount() > 0)
	{
		// Go through each request and see if it matches the clipsetid passed in
		for (s32 request=0; request<m_aClipSetRequests[context].aRequests.GetCount(); ++request)
		{
			if (m_aClipSetRequests[context].aRequests[request]->clipsetId == clipSetId)
			{
				return true;
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CPrioritizedClipSetRequestManager::SortRequests(eRequestContext context)
{
	// Ensure the request count is valid.
	s32 iCount = m_aClipSetRequests[context].aRequests.GetCount();
	if (iCount <= 1)
	{
		return;
	}

	// Sort the requests by clipset hash so its easier to remove dupes
	sRequest** pRequests = &m_aClipSetRequests[context].aRequests[0];
	std::sort(pRequests, pRequests + iCount, CPrioritizedClipSetRequestManager::Compare);
}

////////////////////////////////////////////////////////////////////////////////
