#include "PavementFloodFillHelper.h"

#include "ai/task/taskchannel.h"
#include "Peds/ped.h"

AI_OPTIMISATIONS()

const float CPavementFloodFillHelper::ms_fDefaultPavementSearchRadius = 10.f;

CPavementFloodFillHelper::CPavementFloodFillHelper()
: m_hPathHandle(PATH_HANDLE_NULL)
, m_ePavementStatus(SEARCH_NOT_STARTED)
, m_fOverrideRadius(-1.f)
{

}

CPavementFloodFillHelper::~CPavementFloodFillHelper()
{
	CleanupPavementFloodFillRequest();
}


void CPavementFloodFillHelper::StartPavementFloodFillRequest(CPed* pPed)
{
	taskAssert(m_hPathHandle == PATH_HANDLE_NULL);

	// TODO: When the helper takes a position, it's caller's responsibility to check this first.
	//  At that time the status will probably turn into Idle/RequestPending/RequestReady
	if (pPed->GetNavMeshTracker().GetIsValid() && pPed->GetNavMeshTracker().IsOnPavement())
	{
		m_ePavementStatus = HAS_PAVEMENT;
		return;
	}

	StartPavementFloodFillRequest(pPed, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
}

void CPavementFloodFillHelper::StartPavementFloodFillRequest(CPed* pPed, const Vector3& position)
{
	// Reset the search state to NOT_STARTED, as the path handle is going to get reset below.
	m_ePavementStatus = SEARCH_NOT_STARTED;

	m_hPathHandle = CPathServer::RequestHasNearbyPavementSearch( position, GetSearchRadius(), static_cast<void*>(pPed) );
	if (m_hPathHandle != PATH_HANDLE_NULL)
	{
		m_ePavementStatus = SEARCH_NOT_FINISHED;
	}
}

void CPavementFloodFillHelper::UpdatePavementFloodFillRequest(CPed* pPed)
{
	UpdatePavementFloodFillRequest(pPed, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
}

void CPavementFloodFillHelper::UpdatePavementFloodFillRequest(CPed* pPed, const Vector3& position)
{
	// Don't return immediately if the search hasn't started (StartPavementFloodFillRequest could potentially fail).
	if (m_ePavementStatus != SEARCH_NOT_FINISHED && m_ePavementStatus != SEARCH_NOT_STARTED) 
	{
		return;
	}

	// Hopefully we've already called StartPavementFloodFillRequest before this, but that could still
	//  return PATH_HANDLE_NULL if the request slots are full. [10/11/2012 mdawe]
	if (m_hPathHandle == PATH_HANDLE_NULL)
	{
		StartPavementFloodFillRequest(pPed, position);
		return;
	}
	else
	{
		EPathServerRequestResult eResult = CPathServer::IsRequestResultReady(m_hPathHandle);
		if (eResult == ERequest_NotFound)
		{
			// The request was cleared? Try again.
			m_hPathHandle = PATH_HANDLE_NULL;
			StartPavementFloodFillRequest(pPed, position);
			return;
		}
		else if (eResult == ERequest_Ready)
		{
			const EPathServerErrorCode eError = CPathServer::GetHasNearbyPavementSearchResultAndClear(m_hPathHandle);
			if (eError == PATH_NO_ERROR)
			{
				// Success! Set our result.
				m_ePavementStatus = HAS_PAVEMENT;
			}
			else
			{
				m_ePavementStatus = NO_PAVEMENT;
			}
			
			// If we failed to find any pavement, the flag was cleared when the request started.
			m_hPathHandle = PATH_HANDLE_NULL;
			return;
		}
		else
		{
			// Request isn't ready. Wait another frame. [10/11/2012 mdawe]
			return;
		}
	}
}

void CPavementFloodFillHelper::CleanupPavementFloodFillRequest()
{
	if (m_hPathHandle != PATH_HANDLE_NULL)
	{
		CPathServer::CancelRequest(m_hPathHandle);
		if (m_ePavementStatus == SEARCH_NOT_FINISHED)
		{
			m_ePavementStatus = SEARCH_NOT_STARTED;
		}
		m_hPathHandle = PATH_HANDLE_NULL;
	}
}

float CPavementFloodFillHelper::GetSearchRadius() const
{
	return (m_fOverrideRadius == -1.f ? CPavementFloodFillHelper::ms_fDefaultPavementSearchRadius : m_fOverrideRadius);
}
