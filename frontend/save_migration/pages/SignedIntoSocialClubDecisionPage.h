/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SignedIntoSocialClubDecisionPage.h
// PURPOSE : Is the player signed into SC?
//
// AUTHOR  : stephen.phillips
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef SIGNED_INTO_SOCIAL_CLUB_DECISION_PAGE
#define SIGNED_INTO_SOCIAL_CLUB_DECISION_PAGE

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/pages/DecisionPageBase.h"

class CSignedIntoSocialClubDecisionPage : public CDecisionPageBase
{
	FWUI_DECLARE_DERIVED_TYPE(CSignedIntoSocialClubDecisionPage, CDecisionPageBase);
public:
	CSignedIntoSocialClubDecisionPage() : superclass() {}
	virtual ~CSignedIntoSocialClubDecisionPage() {}

private: // declarations and variables
	PAR_PARSABLE;
	NON_COPYABLE(CSignedIntoSocialClubDecisionPage);

private: // methods
	virtual bool GetDecisionResult() const;
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // SIGNED_INTO_SOCIAL_CLUB_DECISION_PAGE
