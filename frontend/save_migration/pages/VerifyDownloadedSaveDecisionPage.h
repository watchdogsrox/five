/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VerifyDownloadedSaveDecisionPage.h
// PURPOSE : Is the downloaded Singleplayer save valid?
//
// AUTHOR  : stephen.phillips
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef VERIFY_DOWNLOADED_SAVE_DECISION_PAGE
#define VERIFY_DOWNLOADED_SAVE_DECISION_PAGE

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/pages/DecisionPageBase.h"

class CVerifyDownloadedSaveDecisionPage : public CDecisionPageBase
{
	FWUI_DECLARE_DERIVED_TYPE(CVerifyDownloadedSaveDecisionPage, CDecisionPageBase);
public:
	CVerifyDownloadedSaveDecisionPage() : superclass() {}
	virtual ~CVerifyDownloadedSaveDecisionPage() {}

private: // declarations and variables
	PAR_PARSABLE;
	NON_COPYABLE(CVerifyDownloadedSaveDecisionPage);

private: // methods
	virtual bool GetDecisionResult() const;
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // VERIFY_DOWNLOADED_SAVE_DECISION_PAGE
