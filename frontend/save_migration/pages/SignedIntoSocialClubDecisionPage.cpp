#include "SignedIntoSocialClubDecisionPage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "SignedIntoSocialClubDecisionPage_parser.h"

// game
#include "frontend/ui_channel.h"
#include "network/Live/livemanager.h"

FWUI_DEFINE_TYPE(CSignedIntoSocialClubDecisionPage, 0xB5B906F7);

bool CSignedIntoSocialClubDecisionPage::GetDecisionResult() const
{
	const bool c_isSignedIn = CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub();
	return c_isSignedIn;
}

#endif // GEN9_LANDING_PAGE_ENABLED
