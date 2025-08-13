/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : StoryModeAvailableDecisionPage.h
// PURPOSE : Checks access to story mode, and executes appropriate yes/no actions
//
// AUTHOR  : james.strain
// STARTED : November 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef STORY_MODE_AVAILABLE_DECISION_PAGE_H
#define STORY_MODE_AVAILABLE_DECISION_PAGE_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/pages/DecisionPageBase.h"

class CStoryModeAvailableDecisionPage : public CDecisionPageBase
{
    FWUI_DECLARE_DERIVED_TYPE( CStoryModeAvailableDecisionPage, CDecisionPageBase );
public:
    CStoryModeAvailableDecisionPage() : superclass() {}
    virtual ~CStoryModeAvailableDecisionPage() {}

private: // declarations and variables
    PAR_PARSABLE;
    NON_COPYABLE( CStoryModeAvailableDecisionPage );

private: // methods
    virtual bool GetDecisionResult() const;
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // STORY_MODE_AVAILABLE_DECISION_PAGE_H
