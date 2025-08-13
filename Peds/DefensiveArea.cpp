// File header
#include "Peds/DefensiveArea.h"

// Rage headers
#include "ai/aichannel.h"

AI_OPTIMISATIONS()

float CDefensiveArea::sm_fMinRadius = 0.75f;

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CDefensiveArea::CDefensiveArea()
: CArea()
, m_vDefaultCoverFromPosition(0.0f, 0.0f, 0.0f)
, m_bUseCenterAsGoToPosition(false)
{
}

//-------------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------------
CDefensiveArea::~CDefensiveArea()
{
}

void CDefensiveArea::OnReset()
{
	m_vDefaultCoverFromPosition.Zero();
}

void CDefensiveArea::OnSetSphere()
{
	m_vDefaultCoverFromPosition.Zero();

	//Don't allow defensive areas with very small radii.
	//These are very hard for the AI to deal with, for a variety of reasons.
	//One major reason is that the navigation tasks have a tough time making
	//it into very small regions, which causes peds to end up in endless
	//'move within defensive area' loops.
	if(!aiVerifyf(GetWidth() >= GetMinRadius(), "The minimum radius for a defensive area is %.2f, OnSetSphere is setting %.2f", GetMinRadius(), GetWidth()))
	{
		//Set the min radius.
		SetWidth(GetMinRadius());
	}
}


//-------------------------------------------------------------------------
// CDefensiveAreaManager
//-------------------------------------------------------------------------

CDefensiveAreaManager::CDefensiveAreaManager() 
: m_bUseSecondaryAreaForCover(false)
, m_pSecondaryDefensiveArea(NULL)
{
	m_pCurrentDefensiveArea = &m_primaryDefensiveArea;
}

CDefensiveAreaManager::~CDefensiveAreaManager()
{
	delete m_pSecondaryDefensiveArea;
}

void CDefensiveAreaManager::UpdateDefensiveAreas()
{
	if(m_pSecondaryDefensiveArea && !m_pSecondaryDefensiveArea->IsActive())
	{
		if(m_bUseSecondaryAreaForCover)
		{
			m_bUseSecondaryAreaForCover = false;
		}

		if(m_pCurrentDefensiveArea == m_pSecondaryDefensiveArea)
		{
			m_pCurrentDefensiveArea = &m_primaryDefensiveArea;
		}
	}

	Assertf(m_pCurrentDefensiveArea == &m_primaryDefensiveArea || m_pCurrentDefensiveArea == m_pSecondaryDefensiveArea, "Defensive area must equal either our own primary or secondary area");
}

void CDefensiveAreaManager::SetCurrentDefensiveArea(CDefensiveArea* pDefensiveArea)
{
	Assertf(pDefensiveArea == &m_primaryDefensiveArea || pDefensiveArea == m_pSecondaryDefensiveArea, "Defensive area for must be the primary or secondary defensive area");
	m_pCurrentDefensiveArea = pDefensiveArea;
}

CDefensiveArea* CDefensiveAreaManager::GetSecondaryDefensiveArea(bool bCreateIfInactive)
{
	if(bCreateIfInactive && !m_pSecondaryDefensiveArea)
	{
		m_pSecondaryDefensiveArea = rage_new CDefensiveArea();
	}

	Assertf(!bCreateIfInactive || m_pSecondaryDefensiveArea, "Something went wrong and the secondary defensive area hasn't been created.");
	return m_pSecondaryDefensiveArea;
}

// Returns the non-current defensive area
CDefensiveArea* CDefensiveAreaManager::GetAlternateDefensiveArea()
{
	if(m_pSecondaryDefensiveArea && m_pCurrentDefensiveArea == m_pSecondaryDefensiveArea && m_pSecondaryDefensiveArea->IsActive())
	{
		return &m_primaryDefensiveArea;
	}
	else
	{
		return m_pSecondaryDefensiveArea;
	}
}

//////////////////////////////////////////////////////////////////////////
// This will only swap if possible (based on which defensive areas are active)
//////////////////////////////////////////////////////////////////////////
void CDefensiveAreaManager::SwapDefensiveAreaForCoverSearch()
{
	if(m_bUseSecondaryAreaForCover)
	{
		m_bUseSecondaryAreaForCover = false;
	}
	else if(m_pSecondaryDefensiveArea && m_pSecondaryDefensiveArea->IsActive())
	{
		m_bUseSecondaryAreaForCover = true;
	}
}

CDefensiveArea* CDefensiveAreaManager::GetDefensiveAreaForCoverSearch()
{
	if(m_bUseSecondaryAreaForCover && m_pSecondaryDefensiveArea && m_pSecondaryDefensiveArea->IsActive())
	{
		return m_pSecondaryDefensiveArea;
	}
	else
	{
		return &m_primaryDefensiveArea;
	}
}

void CDefensiveAreaManager::ClearPrimaryDefensiveArea()
{
	//Reset the primary defensive area.
	m_primaryDefensiveArea.Reset();

	//The secondary defensive area becomes the primary.
	if(m_pSecondaryDefensiveArea)
	{
		m_primaryDefensiveArea = *m_pSecondaryDefensiveArea;
		delete m_pSecondaryDefensiveArea;
		m_pSecondaryDefensiveArea = NULL;
	}

	//Set the current defensive area.
	SetCurrentDefensiveArea(&m_primaryDefensiveArea);

	//Clear the secondary cover flag.
	m_bUseSecondaryAreaForCover = false;
}
