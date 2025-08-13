/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CarouselPageView.cpp
// PURPOSE : Page view that implements a linear carousel for showing N items
//
// AUTHOR  : james.strain
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "CarouselPageView.h"

#if UI_PAGE_DECK_ENABLED
#include "CarouselPageView_parser.h"

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

FWUI_DEFINE_TYPE( CCarouselPageView, 0x9440FF82 );

CCarouselPageView::CCarouselPageView()
    : superclass()
    , m_topLevelLayout( CPageGridSimple::GRID_3x8 )
{

}

CCarouselPageView::~CCarouselPageView()
{
    Cleanup();
}

void CCarouselPageView::RecalculateGrids()
{
    // Order is important here, as some of the sizing cascades
    // So if you adjust the order TEST IT!
    m_topLevelLayout.Reset();

    // Screen area is acceptable as our grids are all using star sizing
    Vec2f const c_screenExtents = uiPageConfig::GetScreenArea(); 
    m_topLevelLayout.RecalculateLayout( Vec2f( Vec2f::ZERO ), c_screenExtents );
    m_topLevelLayout.NotifyOfLayoutUpdate();
}

void CCarouselPageView::UpdateDerived( float const UNUSED_PARAM(deltaMs) )
{
    UpdateInput();
}

void CCarouselPageView::OnFocusGainedDerived()
{
    m_carousel.RefreshFocusVisuals();
    GetPageRoot().SetVisible( true );
    UpdatePromptsAndTooltip();
}

void CCarouselPageView::OnFocusLostDerived()
{
    GetPageRoot().SetVisible( false );
    ClearPromptsAndTooltip();
}

void CCarouselPageView::PopulateDerived( IPageItemProvider& itemProvider )
{
    if( !IsPopulated() )
    {
        CComplexObject& pageRoot = GetPageRoot();

        m_featuredImage.Initialize( pageRoot );
        m_featuredItem.Initialize( pageRoot );

        CPageLayoutItemParams const c_featureImageParams( 0, 3, 3, 1 );
        m_topLevelLayout.RegisterItem( m_featuredImage, c_featureImageParams );
        ClearFeaturedItem();

        CPageLayoutItemParams const c_featureItemParams( 1, 4, 1, 1 );
        m_topLevelLayout.RegisterItem( m_featuredItem, c_featureItemParams );

        CPageLayoutItemParams const c_tooltipParams( 1, 6, 1, 1 );
        InitializeTooltip( pageRoot, m_topLevelLayout, c_tooltipParams );

        m_pageTitle.Initialize( pageRoot );
        CPageLayoutItemParams const c_titleItemParams( 1, 1, 1, 1 );
        m_topLevelLayout.RegisterItem( m_pageTitle, c_titleItemParams );
        m_pageTitle.SetLiteral( itemProvider.GetTitleString() );

        m_carousel.Initialize( pageRoot );
        CPageLayoutItemParams const c_carouselParams( 1, 5, 1, 1 );
        m_topLevelLayout.RegisterItem( m_carousel, c_carouselParams );

        auto categoryFunc = [this]( CPageItemCategoryBase& category )
        {
			// Currently all planned generic carousels use text item types, WindfallShopPageView carousels use other types.	
            m_carousel.AddCategory( category, eCarouselItemType_Text );
        };

        itemProvider.ForEachCategory( categoryFunc );
        m_carousel.SetDefaultFocus();
        UpdateFeaturedItem();
        RecalculateGrids();
    }
}

bool CCarouselPageView::IsPopulatedDerived() const 
{
    return m_carousel.IsInitialized();
}

void CCarouselPageView::CleanupDerived()
{
    m_topLevelLayout.UnregisterAll();
    ShutdownTooltip();
    m_carousel.Shutdown();
    m_pageTitle.Shutdown();
    m_featuredItem.Shutdown();
	m_featuredImage.Shutdown();
}

void CCarouselPageView::UpdateInputDerived()
{
    if( IsPopulated() )
    {
        fwuiInput::eHandlerResult result( fwuiInput::ACTION_NOT_HANDLED );

        if( CPauseMenu::CheckInput( FRONTEND_INPUT_ACCEPT ) )
        {
            result = m_carousel.HandleInputAction( FRONTEND_INPUT_ACCEPT, GetViewHost() );
            TriggerInputAudio( result, UI_AUDIO_TRIGGER_SELECT, UI_AUDIO_TRIGGER_ERROR );
        }
        else
        {
            int deltaX = 0;

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

            if( deltaX != 0 )
            {
                result = m_carousel.HandleDigitalNavigation( deltaX );
                TriggerInputAudio( result, UI_AUDIO_TRIGGER_NAV_LEFT_RIGHT, nullptr );

                // Focus changed, so we need to re-calculate our tool-tip and prompts
                if( result == fwuiInput::ACTION_HANDLED )
                {
                    UpdateFeaturedItem();
                    UpdatePromptsAndTooltip();
                }
            }
            else
            {
                bool const c_imageChanged = m_featuredImage.UpdateInput();
                if( c_imageChanged )
                {
                    // url:bugstar:6808402 - [UILanding] - Audio Hookup
                    // No trigger requested yet, so can't really guess what to suggest here
                    // TriggerInputAudio( result, UI_AUDIO_TRIGGER_NAV_LEFT_RIGHT, nullptr );
                }
            }
        }
    }
}

void CCarouselPageView::UpdateInstructionalButtonsDerived()
{
    atHashString const c_acceptLabelOverride = GetAcceptPromptLabelOverride();
    m_carousel.UpdateInstructionalButtons( c_acceptLabelOverride );
    m_featuredImage.UpdateInstructionalButtons();
    superclass::UpdateInstructionalButtonsDerived();
}

void CCarouselPageView::UpdateVisualsDerived()
{
    m_carousel.UpdateVisuals();
    m_featuredImage.UpdateVisuals();
}

void CCarouselPageView::UpdateFeaturedItem()
{
    CPageItemBase const * const c_focusedItem = m_carousel.GetFocusedItem();
    m_featuredItem.SetDetails( c_focusedItem );
    m_featuredImage.Set( c_focusedItem );
}

void CCarouselPageView::ClearFeaturedItem()
{
    m_featuredItem.SetDetails( nullptr );
    m_featuredImage.Reset();
}

void CCarouselPageView::UpdatePromptsAndTooltip()
{
    char const * const c_tooltip = m_carousel.GetFocusTooltip();
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

void CCarouselPageView::PlayEnterAnimation()
{
    // Sequence index is the order in which items are revealed
    m_pageTitle.PlayRevealAnimation(0);
    m_featuredImage.PlayRevealAnimation(1);
    m_featuredItem.PlayRevealAnimation(2);
    m_carousel.PlayRevealAnimation(3);
}

#endif // UI_PAGE_DECK_ENABLED
