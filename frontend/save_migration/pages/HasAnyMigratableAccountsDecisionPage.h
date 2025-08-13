/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : HasAnyMigratableAccountsDecisionPage.h
// PURPOSE : Checks if migration has been succesful so the user can be sent to the apropriate screen 
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef HAS_ANY_MIGRATABLE_ACCOUNTS_DECISION_PAGE
#define HAS_ANY_MIGRATABLE_ACCOUNTS_DECISION_PAGE

#include "frontend/landing_page/LandingPageConfig.h"
#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/pages/DecisionPageBase.h"

class CHasAnyMigratableAccountsDecisionPage : public CDecisionPageBase
{
	FWUI_DECLARE_DERIVED_TYPE(CHasAnyMigratableAccountsDecisionPage, CDecisionPageBase);
public:
	CHasAnyMigratableAccountsDecisionPage() : superclass() {}
	virtual ~CHasAnyMigratableAccountsDecisionPage() {}

private: // declarations and variables
	PAR_PARSABLE;
	NON_COPYABLE(CHasAnyMigratableAccountsDecisionPage);

private: // methods
	virtual bool GetDecisionResult() const;
};

#endif // GEN9_LANDING_PAGE_ENABLED
#endif // HAS_ANY_MIGRATABLE_ACCOUNTS_DECISION_PAGE