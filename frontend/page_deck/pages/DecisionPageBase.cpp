/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : DecisionPageBase.cpp
// PURPOSE : Page that makes a simple yes/no check, and executes a further 
//           action from that. Decision is provided by derived classes
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "DecisionPageBase.h"
#if UI_PAGE_DECK_ENABLED
#include "DecisionPageBase_parser.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "frontend/page_deck/IPageMessageHandler.h"

FWUI_DEFINE_TYPE( CDecisionPageBase, 0x55598917 );

CDecisionPageBase::CDecisionPageBase()
    : superclass()
    , m_trueAction( nullptr )
    , m_falseAction( nullptr )
    , m_timeoutMs( 0.0f )
    , m_decisionMade(false)
{

}

CDecisionPageBase::~CDecisionPageBase()
{
    delete m_trueAction;
    delete m_falseAction;
}

void CDecisionPageBase::OnEnterStartDerived()
{
    uiFatalAssertf( m_trueAction, "No action provided for true result. Your data is likely bad" );
    uiFatalAssertf( m_falseAction, "No action provided for false result. Your data is likely bad" );

    m_timeoutMs = 0.0f;
    m_decisionMade = false;
    UpdateDecision();
}

bool CDecisionPageBase::OnEnterUpdateDerived(float const deltaMs)
{
    m_timeoutMs += deltaMs;

    return UpdateDecision();
}

bool CDecisionPageBase::UpdateDecision()
{
    if (!m_decisionMade && (HasDecisionResult() || (m_timeoutMs >= GetTransitionTimeout())))
    {
        IPageMessageHandler& messageHandler = GetMessageHandler();

        bool const c_decisionResult = GetDecisionResult();
        if (c_decisionResult)
        {
            messageHandler.HandleMessage(*m_trueAction);
        }
        else
        {
            messageHandler.HandleMessage(*m_falseAction);
        }

        m_decisionMade = true;
    }

    return m_decisionMade;
}

#endif // UI_PAGE_DECK_ENABLED
