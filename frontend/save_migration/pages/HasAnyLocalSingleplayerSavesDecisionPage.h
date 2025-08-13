/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : HasAnyLocalSingleplayerSavesDecisionPage.h
// PURPOSE : Does the player have any local singleplayer saves?
//
// AUTHOR  : stephen.phillips
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef HAS_ANY_LOCAL_SINGLEPLAYER_SAVES_DECISION_PAGE
#define HAS_ANY_LOCAL_SINGLEPLAYER_SAVES_DECISION_PAGE

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/pages/DecisionPageBase.h"

class CHasAnyLocalSingleplayerSavesDecisionPage : public CDecisionPageBase
{
	FWUI_DECLARE_DERIVED_TYPE(CHasAnyLocalSingleplayerSavesDecisionPage, CDecisionPageBase);
public:
	CHasAnyLocalSingleplayerSavesDecisionPage() : superclass() {}
	virtual ~CHasAnyLocalSingleplayerSavesDecisionPage() {}

private: // declarations and variables
	PAR_PARSABLE;
	NON_COPYABLE(CHasAnyLocalSingleplayerSavesDecisionPage);

private: // methods
	virtual bool GetDecisionResult() const;
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // HAS_ANY_LOCAL_SINGLEPLAYER_SAVES_DECISION_PAGE
