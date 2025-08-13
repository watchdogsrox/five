/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageViewBase.cpp
// PURPOSE : Base class for page views, which take page contents and visualize them
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "PageViewBase.h"

#if UI_PAGE_DECK_ENABLED

#include "PageViewBase_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/input/uiInputCommon.h"
#include "frontend/page_deck/IPageViewHost.h"
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "frontend/page_deck/PageItemBase.h"
#include "frontend/page_deck/layout/PageLayoutBase.h"
#include "frontend/page_deck/layout/PageLayoutItemParams.h"
#include "frontend/VideoEditor/ui/InstructionalButtons.h"

FWUI_DEFINE_TYPE( CPageViewBase, 0x87322A73 );

void CPageViewBase::Initialize( IPageViewHost& viewHost )
{
    if( uiVerifyf( !IsInitialized(), "Double initializing view" ) )
    {
        m_viewHost = &viewHost;
        m_focused = false;
    }
}

void CPageViewBase::Shutdown()
{
    OnShutdown();

    CleanupHotkeys();
    m_pageRoot.Release();
    m_viewHost =  nullptr;
    m_focused = false;
}

void CPageViewBase::OnEnterStart( IPageItemProvider& itemProvider )
{
    uiFatalAssertf( !HasValidState(), "Double-entering a page is not supported" );
    m_itemProvider = &itemProvider;

    IPageViewHost& viewHost = GetViewHost();
    viewHost.RegisterActivePage( *this );
    m_timeoutMs = 0.f;
    OnEnterStartDerived();
}

bool CPageViewBase::OnEnterUpdate( float const deltaMs )
{
    bool result = false;

    IPageViewHost& viewHost = GetViewHost();
    if( viewHost.CanCreateContent() )
    {
        if( !m_pageRoot.IsActive() )
        {
            CComplexObject& sceneRoot = viewHost.GetSceneRoot();
            m_pageRoot = sceneRoot.CreateEmptyMovieClip( nullptr, -1 );
        }
        result = OnEnterUpdateDerived( deltaMs );
    }

    return result;
}

void CPageViewBase::OnEnterComplete()
{
    OnEnterCompleteDerived();
}

void CPageViewBase::OnEnterTimeout()
{
    OnEnterTimeoutDerived();
}

void CPageViewBase::OnFocusGained( bool const focusFromEnter )
{
    if( focusFromEnter )
    {
        PlayEnterAnimation();
    }

    m_focused = true;
    OnFocusGainedDerived();
    RefreshPrompts();
}

void CPageViewBase::Update( float const deltaMs )
{
    if( m_pendingBackoutWarning )
    {
        // If you are looking to add complex logic here, please don't. You can use the BACKOUT_BLOCKED mode and then
        // register a hotkey + message to the backbutton in the page deck data to achieve a highly custom backout warning instead
        CWarningScreen::SetMessage( WARNING_MESSAGE_STANDARD, m_backoutWarningLabel, FE_WARNING_OK | FE_WARNING_CANCEL );
        eWarningButtonFlags const c_result = CWarningScreen::CheckAllInput();
        if( c_result != FE_WARNING_NONE )
        {
            m_pendingBackoutWarning = false;
            if( ( c_result & FE_WARNING_OK ) != 0 )
            {
                IPageViewHost& viewHost = GetViewHost();
                uiPageDeckBackMessage backMsg;
                viewHost.HandleMessage( backMsg );
            }
        }
    }
    else
    {
        UpdateDerived( deltaMs );
        UpdateVisuals();
    }
}

void CPageViewBase::UpdateInput()
{
    if( !m_pendingBackoutWarning )
    {
        if( IsViewFocused() )
        {
            bool handledInput = false;
            IPageViewHost& viewHost = GetViewHost();

            bool const c_backoutAllowed = IsBackoutAllowed();
            if( c_backoutAllowed && CPauseMenu::CheckInput( FRONTEND_INPUT_BACK ) )
            {
                bool triggerAudio = true;

                if( m_backoutMode == BACKOUT_DEFAULT )
                {
                    uiPageDeckBackMessage backMsg;
                    if( viewHost.HandleMessage( backMsg ) )
                    {
                        triggerAudio = true;
                    }
                }
                else
                {
                    m_pendingBackoutWarning = true;
                }

                if( triggerAudio )
                {
                    TriggerInputAudio( fwuiInput::ACTION_HANDLED, "BACK", nullptr );
                }
            }
            else
            {
                auto inputFunc = [&handledInput,&viewHost]( uiPageHotkey& hotkey )
                {
                    eFRONTEND_INPUT const c_input = hotkey.GetFrontendInputType();
                    if( CPauseMenu::CheckInput( c_input, true ) )
                    {
                        viewHost.HandleMessage( *hotkey.GetMessage() );
                        handledInput = true;
                    }
                };
                ForEachHotkey( inputFunc );

                if( !handledInput )
                {
                    UpdateInputDerived();
                }
            }
        }
    }
}

void CPageViewBase::UpdateVisuals()
{
    UpdateVisualsDerived();
}

void CPageViewBase::OnFocusLost()
{
    m_focused = false;
    OnFocusLostDerived();
}

void CPageViewBase::OnExitStart()
{
    m_timeoutMs = 0.f;
    OnExitStartDerived();
}

bool CPageViewBase::OnExitUpdate( float const deltaMs )
{
    return OnExitUpdateDerived( deltaMs );
}

void CPageViewBase::OnExitComplete( IPageItemProvider& ASSERT_ONLY(itemProvider) )
{
    OnExitCompleteDerived();
    m_pageRoot.Release();
    IPageViewHost& viewHost = GetViewHost();
    viewHost.UnregisterActivePage( *this );
    uiFatalAssertf( m_itemProvider == &itemProvider, "Exiting page with wrong item provider. Caller logic is very broken" );
    m_itemProvider = nullptr;
    m_pendingBackoutWarning = false;
}

CPageViewBase::CPageViewBase()
    : superclass()
    , m_viewHost( nullptr )
    , m_itemProvider( nullptr )
    , m_backOutLabel()
    , m_acceptLabel()
    , m_timeoutMs( 0.f )
    , m_backoutMode( BACKOUT_DEFAULT )
    , m_focused( false )
    , m_pendingBackoutWarning( false )
{

}

void CPageViewBase::TriggerInputAudio( fwuiInput::eHandlerResult const actionResult, char const * const actionedTrigger, char const * const ignoredTrigger ) const
{
    char const * const c_audioSoundset = GetAudioSoundset();
    uiInput::TriggerInputAudio( actionResult, c_audioSoundset, actionedTrigger, ignoredTrigger );
}

char const * const CPageViewBase::GetAudioSoundset()
{
    // TODO_UI - We can parGen this if required for different views
    return "UI_Landing_Screen_Extended_Sounds";
}

void CPageViewBase::UpdateInstructionalButtonsDerived() 
{
    if( IsBackoutAllowed() )
    {
        IPageViewHost& viewHost = GetViewHost();
        uiPageDeckCanBackOutMessage canBackOutMsg;
        if( viewHost.HandleMessage( canBackOutMsg ) && canBackOutMsg.GetResult() )
        {
            char const * const c_label = GetBackoutPromptText();
            CVideoEditorInstructionalButtons::AddButton( rage::INPUT_FRONTEND_CANCEL, c_label, true );
        }
    }

    auto ibFunc = []( uiPageHotkey const& hotkey )
    {
        char const * const c_label = TheText.Get( hotkey.GetLabel() );
        CVideoEditorInstructionalButtons::AddButton( hotkey.GetHotkeyInputType(), c_label, true );
    };
    ForEachHotkey( ibFunc );
}

void CPageViewBase::InitializeTooltip( CComplexObject& parentObject, CPageLayoutBase& layout, CPageLayoutItemParams const& params )
{
    m_tooltip.Initialize( parentObject );
    layout.RegisterItem( m_tooltip, params );
}

void CPageViewBase::ShutdownTooltip()
{
    m_tooltip.Shutdown();
}

void CPageViewBase::SetTooltipLiteral( char const * const tooltipText, bool isRichText )
{
    m_tooltip.SetStringLiteral( tooltipText, isRichText);
}

void CPageViewBase::RefreshPrompts()
{
    CVideoEditorInstructionalButtons::Refresh();
}

void CPageViewBase::ClearPromptsAndTooltip()
{
    CVideoEditorInstructionalButtons::Refresh();
    ClearTooltip();
}

void CPageViewBase::ClearTooltip()
{
    m_tooltip.SetStringLiteral( "", false );
}

bool CPageViewBase::AreAllItemTexturesAvailable() const
{
    bool areAllTexturesAvailable = true;

    IPageItemProvider const& c_itemProvider = GetItemProvider();

    auto itemFunc = [&areAllTexturesAvailable]( CPageItemBase const& item )
    {
        IItemTextureInterface const * const c_textureInterface = item.GetTextureInterface();
        bool const c_itemTexturesAvailable = c_textureInterface == nullptr || c_textureInterface->IsTextureAvailable();
        areAllTexturesAvailable = areAllTexturesAvailable && c_itemTexturesAvailable;
    };

    auto categoryFunc = [&itemFunc]( CPageItemCategoryBase const& category )
    {
        category.ForEachItem( itemFunc );
    };
    c_itemProvider.ForEachCategory( categoryFunc );

    return areAllTexturesAvailable;
}

bool CPageViewBase::OnEnterUpdateDerived( float const deltaMs )
{
    m_timeoutMs += deltaMs;
    return true;
}

bool CPageViewBase::OnExitUpdateDerived(float const deltaMs)
{
    m_timeoutMs += deltaMs;
    return true;
}

bool CPageViewBase::OnEnterTimeoutDerived()
{
    return true;
}

char const * CPageViewBase::GetAcceptPromptText() const
{
    return uiPageConfig::GetLabel( ATSTRINGHASH( "IB_SELECT", 0xD7ED7F0C ), m_acceptLabel );
}

char const * CPageViewBase::GetBackoutPromptText() const
{
    return uiPageConfig::GetLabel( ATSTRINGHASH( "IB_BACK", 0xE5FC11CD ), m_backOutLabel );
}

void CPageViewBase::ForEachHotkey( HotkeyLambda action )
{
    int const c_count = m_hotkeyMapping.GetCount();
    for( int index = 0; index < c_count; ++index )
    {
        uiPageHotkey * const entry = m_hotkeyMapping[index];
        if( entry && entry->IsValid() )
        {
            action( *entry );
        }
    }
}

void CPageViewBase::ForEachHotkey( HotkeyConstLambda action ) const
{
    int const c_count = m_hotkeyMapping.GetCount();
    for( int index = 0; index < c_count; ++index )
    {
        uiPageHotkey const * const c_entry = m_hotkeyMapping[index];
        if( c_entry && c_entry->IsValid() )
        {
            action( *c_entry );
        }
    }
}

void CPageViewBase::CleanupHotkeys()
{
    auto shutdownFunc = []( uiPageHotkey& hotkey )
    {
        delete &hotkey;
    };
    ForEachHotkey( shutdownFunc );
    m_hotkeyMapping.ResetCount();
}

#endif // UI_PAGE_DECK_ENABLED
