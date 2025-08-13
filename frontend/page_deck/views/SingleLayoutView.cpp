/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SingleLayoutView.cpp
// PURPOSE : Base view that is designed for views that use a single content layout
//
// AUTHOR  : james.strain
// STARTED : April 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "SingleLayoutView.h"

#if UI_PAGE_DECK_ENABLED
#include "SingleLayoutView_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/page_deck/IPageItemCollection.h"
#include "frontend/page_deck/IPageItemProvider.h"
#include "frontend/page_deck/IPageViewHost.h"
#include "frontend/page_deck/layout/DynamicLayoutContext.h"
#include "frontend/page_deck/PageItemBase.h"
#include "frontend/page_deck/PageItemCategoryBase.h"
#include "frontend/PauseMenu.h"

FWUI_DEFINE_TYPE( CSingleLayoutView, 0xCC3DD512 );

CSingleLayoutView::CSingleLayoutView()
    : superclass()
    , m_dynamicLayoutContext( nullptr )
{

}

CSingleLayoutView::~CSingleLayoutView()
{
    Cleanup();
}

void CSingleLayoutView::InitializeTitle( CComplexObject& parentObject, CPageLayoutBase& layout, CPageLayoutItemParams const& params )
{
    m_pageTitle.Initialize( parentObject );
    layout.RegisterItem( m_pageTitle, params );
}

void CSingleLayoutView::ShutdownTitle()
{
    m_pageTitle.Shutdown();
}

void CSingleLayoutView::SetTitleLiteral( char const * const titleText )
{
    m_pageTitle.SetLiteral( titleText );
}

void CSingleLayoutView::ClearTitle()
{
    m_pageTitle.SetLiteral( "" );
}

void CSingleLayoutView::InitializeLayoutContext( IPageItemProvider& itemProvider, CComplexObject& visualRoot, CPageLayoutBase& layoutRoot )
{
    if( uiVerifyf( m_dynamicLayoutContext == nullptr, "Double initializing layout context?" ) )
    {
        m_dynamicLayoutContext = rage_new CDynamicLayoutContext( visualRoot, layoutRoot );
        auto categoryFunc = [this]( CPageItemCategoryBase& category )
        {
            m_dynamicLayoutContext->AddCategory( category );
        };
        itemProvider.ForEachCategory( categoryFunc );
        m_dynamicLayoutContext->SetDefaultFocus();
    }
}

void CSingleLayoutView::ShutdownLayoutContext()
{
    if( m_dynamicLayoutContext != nullptr )
    {
        m_dynamicLayoutContext->Shutdown();
        delete m_dynamicLayoutContext;
        m_dynamicLayoutContext = nullptr;
    }
}

void CSingleLayoutView::CleanupDerived()
{
    ShutdownTooltip();
    ShutdownTitle();
    ShutdownLayoutContext();
}

void CSingleLayoutView::UpdateDerived( float const UNUSED_PARAM(deltaMs) )
{
    UpdateInput();
}

void CSingleLayoutView::OnFocusGainedDerived()
{
    if( m_dynamicLayoutContext )
    {
        m_dynamicLayoutContext->RefreshFocusVisuals();
    }

    GetPageRoot().SetVisible( true );
    UpdatePromptsAndTooltip();
}

void CSingleLayoutView::OnFocusLostDerived()
{
    GetPageRoot().SetVisible( false );
    ClearPromptsAndTooltip();
}

bool CSingleLayoutView::IsPopulatedDerived() const 
{
    return m_dynamicLayoutContext != nullptr;
}

void CSingleLayoutView::UpdateInputDerived()
{
    if( m_dynamicLayoutContext )
    {
        fwuiInput::eHandlerResult result( fwuiInput::ACTION_NOT_HANDLED );

        if( CPauseMenu::CheckInput( FRONTEND_INPUT_ACCEPT ) )
        {
            result = m_dynamicLayoutContext->HandleInputAction( FRONTEND_INPUT_ACCEPT, GetViewHost() );
            TriggerInputAudio( result, UI_AUDIO_TRIGGER_SELECT, UI_AUDIO_TRIGGER_ERROR );
        }
        else
        {
            int deltaX = 0;
            int deltaY = 0;

            // If CPauseMenu navigation results for analogue input are not accurate enough,
            // consider using the fwuiInput Quantization and direct reads of the analogue values
            if( CPauseMenu::CheckInput( FRONTEND_INPUT_LEFT ) )
            {
                deltaX = -1;
            }
            else if( CPauseMenu::CheckInput( FRONTEND_INPUT_RIGHT ) )
            {
                deltaX = 1;
            }

            if( CPauseMenu::CheckInput( FRONTEND_INPUT_UP ) )
            {
                deltaY = -1;
            }
            else if( CPauseMenu::CheckInput( FRONTEND_INPUT_DOWN ) )
            {
                deltaY = 1;
            }

            if( deltaX != 0 || deltaY != 0 )
            {
                result = m_dynamicLayoutContext->HandleDigitalNavigation( deltaX, deltaY );
                char const * const c_positiveTrigger = ( deltaX != 0 ) ? UI_AUDIO_TRIGGER_NAV_LEFT_RIGHT : UI_AUDIO_TRIGGER_NAV_UP_DOWN;
                TriggerInputAudio( result, c_positiveTrigger, nullptr );

                // Focus changed, so we need to re-calculate our tool-tip and prompts
                if( result == fwuiInput::ACTION_HANDLED )
                {
                    UpdatePromptsAndTooltip();
                }
            }
        }
    }
}

void CSingleLayoutView::UpdateInstructionalButtonsDerived()
{
    if( m_dynamicLayoutContext )
    {
        m_dynamicLayoutContext->UpdateInstructionalButtons();
    }

    superclass::UpdateInstructionalButtonsDerived();
}

void CSingleLayoutView::UpdatePromptsAndTooltip()
{
    if( m_dynamicLayoutContext )
    {
        char const * const c_tooltip = m_dynamicLayoutContext->GetFocusTooltip();
        if( c_tooltip )
        {
            SetTooltipLiteral( c_tooltip, false );
        }
        else
        {
            ClearTooltip();
        }

        RefreshPrompts();
    }
}

#endif // UI_PAGE_DECK_ENABLED
