/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageBase.cpp
// PURPOSE : Base class for pages in our page deck system. Contains basic params
//           shared across all pages
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "PageBase.h"
#if UI_PAGE_DECK_ENABLED
#include "PageBase_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/IPageViewHost.h"
#include "frontend/page_deck/PageViewBase.h"
#include "frontend/page_deck/PageItemBase.h"
#include "text/TextFile.h"

FWUI_DEFINE_TYPE( CPageBase, 0xB9397ADE );

CPageBase::~CPageBase()
{
    delete m_view;
#if RSG_BANK
    m_view = nullptr;
    m_messageHandler = nullptr;
#endif // RSG_BANK
}

void CPageBase::Initialize( uiPageId const id, IPageViewHost& viewHost )
{
    if( uiVerifyf( m_id.IsNull(), "Double setting page ID is not expected. This should only be called on initial load. Existing ID '" HASHFMT "' and new id '" HASHFMT "'",
        HASHOUT(m_id), HASHOUT(id) ) )
    {
        m_id = id;
        // TODO_UI - This is a bit of a shortcut, in that the view host generally sends messages to the controller,
        // so we can just use it here and still send messages to where we want to
        m_messageHandler = &viewHost; 
        if( m_view )
        {
            m_view->Initialize( viewHost );
        }
    }
}

void CPageBase::Shutdown()
{
    m_messageHandler = nullptr;
    m_id = m_id.Null();
    if( m_view )
    {
        m_view->Shutdown();
    }
}

char const * CPageBase::GetTitleString() const
{
    atHashString textLabel = GetTitle();
    textLabel = textLabel.IsNull() ? m_id.GetAsHashString() : textLabel;

    char const * result = TheText.Get( textLabel, textLabel.TryGetCStr() );
    return result;
}

void CPageBase::EnterPage( bool const immediate )
{
    if( uiVerifyf( ShouldEnter(), "Attempting to enter " HASHFMT " but in non-enterable state %u", HASHOUT( GetId() ), m_statePhase ) )
    {
        uiDebugf3( "Entering " HASHFMT, HASHOUT( GetId() ) );
        OnEnterStart();
        m_statePhase = STATE_ENTERING;
        UpdateStatePhase( 0.f, immediate );
    }
}

void CPageBase::OnEnterTimedOut()
{
    if( uiVerifyf( IsEntering(), "Attempting to handled timeout when entering " HASHFMT " but in wrong state %u", HASHOUT( GetId() ), m_statePhase ) )
    {
        // Forcing an immediate update should be enough of a nudge for now
        UpdateStatePhase( 0.f, true );
    }
}

void CPageBase::GainFocus( bool const focusFromEnter )
{
    if( uiVerifyf( !IsFocused(), "Attempting to focus " HASHFMT " but it is already focused", HASHOUT( GetId() ) ) &&
        uiVerifyf( !ShouldEnter(), "Attempting to focus " HASHFMT " but it is not entered", HASHOUT( GetId() ) ) )
    {
        uiDebugf3( "Focusing " HASHFMT, HASHOUT( GetId() ) );
        OnFocusGained( focusFromEnter );
    }
}

void CPageBase::Update( float const deltaMs )
{
    UpdateStatePhase( deltaMs, false );
}

void CPageBase::LoseFocus()
{
    if( uiVerifyf( IsFocused(), "Attempting to unfocus " HASHFMT " but it is not focused", HASHOUT( GetId() ) ) &&
        uiVerifyf( !ShouldEnter(), "Attempting to focus " HASHFMT " but it is not entered", HASHOUT( GetId() ) ) )
    {
        uiDebugf3( "Unfocusing " HASHFMT, HASHOUT( GetId() ) );
        OnFocusLost();
    }
}

void CPageBase::ExitPage( bool const immediate )
{
    if( ShouldExit() )
    {
        if( IsEntering() )
        {
            uiDebugf3( FWUI_INSTANCE_FMT " was part-entered, forcing enter complete before exiting", HASHOUT( GetId() ) );
            OnEnterComplete();
        }

        OnExitStart();
        m_statePhase = STATE_EXITING;
    }

    if( IsExiting() )
    {
        UpdateStatePhase( 0.f, immediate );
    }
}

float CPageBase::GetTransitionTimeout() const
{
    // Futurism; This 10 second timeout was picked on it being the default in RDR, and rarely changed. 
    // So rather than data drive it we're keeping it as is for now. The few cases in RDR that changed it likely
    // don't apply here (more likely we will want pages that say "I never time out" and we'll expose that as traits if find a case for it)
    return 10000.f;
}

bool CPageBase::IsTransitoryPage() const
{
    return IsTransitoryPageDerived();
}

CPageBase::CPageBase()
    : m_messageHandler( nullptr)
    , m_view( nullptr )
    , m_id()
    , m_title()
    , m_statePhase( STATE_REQUIRES_ENTER )
    , m_focused( false )
{

}

void CPageBase::UpdateStatePhase( float const deltaMs, bool const immediate )
{
    switch( m_statePhase )
    {
    case STATE_ENTERING:
        {
            bool const c_enterComplete = OnEnterUpdate( deltaMs ) || immediate;
            if( c_enterComplete )
            {
                OnEnterComplete();
                m_statePhase = STATE_ACTIVE;
                // We do not execute a break statement here because otherwise we would have an unnecessary frame delay 
                // while transitioning
            }
            else
            {
                break;
            }
        }

    case STATE_ACTIVE:
        {
            UpdateInternal( deltaMs );
            break;
        }

    case STATE_EXITING:
        {
            bool const c_exitComplete = OnExitUpdate( deltaMs ) || immediate;
            if( c_exitComplete )
            {
                OnExitComplete();
                m_statePhase = STATE_REQUIRES_ENTER;
            }

            break;
        }

    default:
        {
            // NOP
        }
    }
}

void CPageBase::OnEnterStart()
{
    OnEnterStartDerived();
    if( m_view )
    {
        m_view->OnEnterStart( *this );
    }
}

bool CPageBase::OnEnterUpdate( float const deltaMs )
{
    bool const c_logicalEnterComplete = OnEnterUpdateDerived( deltaMs );
    bool viewEnterComplete = true;
    if( m_view )
    {
        viewEnterComplete = m_view->OnEnterUpdate( deltaMs );
    }

    return c_logicalEnterComplete && viewEnterComplete;
}

void CPageBase::OnEnterComplete()
{
    OnEnterCompleteDerived();
    if( m_view )
    {
        m_view->OnEnterComplete();
    }
}

void CPageBase::OnFocusGained( bool const focusFromEnter )
{
    OnFocusGainedDerived();
    m_focused = true;
    if( m_view )
    {
        m_view->OnFocusGained( focusFromEnter );
    }
}

void CPageBase::UpdateInternal( float const deltaMs )
{
    UpdateDerived( deltaMs );
    if( m_view )
    {
        m_view->Update( deltaMs );
    }
}

void CPageBase::OnFocusLost()
{
    if( m_view )
    {
        m_view->OnFocusLost();
    }
    m_focused = false;
    OnFocusLostDerived();
}

void CPageBase::OnExitStart()
{
    if( m_view )
    {
        m_view->OnExitStart();
    }
    OnExitStartDerived();
}

bool CPageBase::OnExitUpdate( float const deltaMs )
{
    bool const c_logicalExitComplete = OnExitUpdateDerived( deltaMs );
    bool viewExitComplete = true;
    if( m_view )
    {
        viewExitComplete = m_view->OnExitUpdate( deltaMs );
    }

    return c_logicalExitComplete && viewExitComplete;
}

void CPageBase::OnExitComplete()
{
    if( m_view )
    {
        m_view->OnExitComplete( *this );
    }
    OnExitCompleteDerived();
}

#endif // UI_PAGE_DECK_ENABLED
