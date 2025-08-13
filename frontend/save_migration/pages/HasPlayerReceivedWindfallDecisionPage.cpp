#include "HasPlayerReceivedWindfallDecisionPage.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "HasPlayerReceivedWindfallDecisionPage_parser.h"

// game
#include "frontend/landing_page/LandingPageArbiter.h"

FWUI_DEFINE_TYPE(CHasPlayerReceivedWindfallDecisionPage, 0xE384DC0F);

bool CHasPlayerReceivedWindfallDecisionPage::GetDecisionResult() const
{
	return CLandingPageArbiter::HasPlayerRecievedWindfall();
}

#endif // GEN9_LANDING_PAGE_ENABLED
