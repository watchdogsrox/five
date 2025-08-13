/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CHasPlayerReceivedWindfallDecisionPage.h
// PURPOSE : Checks if migration has been succesful so the user can be sent to the apropriate screen 
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef HAS_PLAYER_RECEIVED_WINDFALL_DECISION_PAGE
#define HAS_PLAYER_RECEIVED_WINDFALL_DECISION_PAGE

#include "frontend/landing_page/LandingPageConfig.h"
#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/pages/DecisionPageBase.h"

class CHasPlayerReceivedWindfallDecisionPage : public CDecisionPageBase
{
	FWUI_DECLARE_DERIVED_TYPE(CHasPlayerReceivedWindfallDecisionPage, CDecisionPageBase);
public:
	CHasPlayerReceivedWindfallDecisionPage() : superclass() {}
	virtual ~CHasPlayerReceivedWindfallDecisionPage() {}

private: // declarations and variables
	PAR_PARSABLE;
	NON_COPYABLE(CHasPlayerReceivedWindfallDecisionPage);

private: // methods
	virtual bool GetDecisionResult() const;
};

#endif // GEN9_LANDING_PAGE_ENABLED
#endif // HAS_PLAYER_RECEIVED_WINDFALL_DECISION_PAGE