/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : DecisionPageBase.h
// PURPOSE : Page that makes a simple yes/no check, and executes a further 
//           action from that. Decision is provided by derived classes
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef DECISION_PAGE_BASE_H
#define DECISION_PAGE_BASE_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/PageBase.h"

class uiPageDeckMessageBase;

class CDecisionPageBase : public CCategorylessPageBase
{
    FWUI_DECLARE_DERIVED_TYPE( CDecisionPageBase, CCategorylessPageBase );
protected:
    CDecisionPageBase();
    virtual ~CDecisionPageBase();

private: // declarations and variables
    uiPageDeckMessageBase* m_trueAction;
    uiPageDeckMessageBase* m_falseAction;
    float m_timeoutMs;
    bool m_decisionMade;
    PAR_PARSABLE;
    NON_COPYABLE( CDecisionPageBase );

private: // methods
    virtual void OnEnterStartDerived();
    virtual bool OnEnterUpdateDerived(float const deltaMs);
    virtual bool HasDecisionResult() const { return true; }
    virtual bool GetDecisionResult() const = 0;

    bool UpdateDecision();

    bool IsTransitoryPageDerived() const final { return true; }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // DECISION_PAGE_BASE_H
