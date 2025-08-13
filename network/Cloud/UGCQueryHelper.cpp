//===========================================================================
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.             
//===========================================================================

// game includes
#include "Network/Cloud/UGCQueryHelper.h"

NETWORK_OPTIMISATIONS();

void UGCQueryHelper::Init(rlUgcContentType type, const char* pQueryName)
{
	Reset();

	m_contentType = type;
	m_pQueryName = pQueryName;
}

void UGCQueryHelper::Reset()
{
	if(m_ugcQueryStatus.Pending())
		rlUgc::Cancel(&m_ugcQueryStatus);

	m_ugcQueryStatus.Reset();
}

void UGCQueryHelper::Trigger(const char* pParams, int numParams)
{
	Reset();

	if(AssertVerify(m_pQueryName) && AssertVerify(pParams) && AssertVerify(m_contentType != RLUGC_CONTENT_TYPE_UNKNOWN))
	{
		UserContentManager::GetInstance().QueryContentCreators(m_contentType, m_pQueryName, pParams, 0, numParams, m_ugcDelegate, &m_ugcQueryStatus);
	}
}

