/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : HasAnyMigratableSingleplayerSavesDecisionPage.h
// PURPOSE : Does the player have any singleplayer saves to migrate from Gen8?
//
// AUTHOR  : stephen.phillips
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef HAS_ANY_MIGRATABLE_SINGLEPLAYER_SAVES_DECISION_PAGE
#define HAS_ANY_MIGRATABLE_SINGLEPLAYER_SAVES_DECISION_PAGE

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/pages/DecisionPageBase.h"

class CHasAnyMigratableSingleplayerSavesDecisionPage : public CDecisionPageBase
{
	FWUI_DECLARE_DERIVED_TYPE(CHasAnyMigratableSingleplayerSavesDecisionPage, CDecisionPageBase);
public:
	CHasAnyMigratableSingleplayerSavesDecisionPage() : superclass() {}
	virtual ~CHasAnyMigratableSingleplayerSavesDecisionPage() {}

private: // declarations and variables
	PAR_PARSABLE;
	NON_COPYABLE(CHasAnyMigratableSingleplayerSavesDecisionPage);

private: // methods
	virtual bool GetDecisionResult() const;
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // HAS_ANY_MIGRATABLE_SINGLEPLAYER_SAVES_DECISION_PAGE
