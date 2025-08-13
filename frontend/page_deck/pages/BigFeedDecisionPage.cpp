/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : BigFeedDecisionPage.cpp
// PURPOSE : Are there any big feed items to view?
//
// AUTHOR  : james.strain
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "BigFeedDecisionPage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "BigFeedDecisionPage_parser.h"

// game
#include "frontend/ui_channel.h"
#include "network/SocialClubServices/SocialClubNewsStoryMgr.h"

FWUI_DEFINE_TYPE(CBigFeedDecisionPage, 0x57813EF0);

bool CBigFeedDecisionPage::HasDecisionResult() const
{
    if (!NetworkInterface::IsReadyToCheckNetworkAccess(eNetworkAccessArea::AccessArea_Landing))
    {
        return false;
    }

    eNetworkAccessCode const c_networkAccess = NetworkInterface::CheckNetworkAccess(eNetworkAccessArea::AccessArea_Landing);

    // If there's any network error then continue immediately
    if (c_networkAccess != eNetworkAccessCode::Access_Granted)
    {
        return true;
    }

    return CNetworkNewsStoryMgr::Get().HasCloudRequestCompleted();
}

bool CBigFeedDecisionPage::GetDecisionResult() const
{
    CNetworkNewsStoryMgr const& c_newsMgr = CNetworkNewsStoryMgr::Get();

    // NOTE - Current design only cares about priority news.
    bool const c_hasUnseenPriorityNews = c_newsMgr.HasUnseenPriorityNews();
	return  c_hasUnseenPriorityNews;
}

float CBigFeedDecisionPage::GetTransitionTimeout() const
{
    return 5000.0f; // This is in ms
}

#endif // GEN9_LANDING_PAGE_ENABLED
