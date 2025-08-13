/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : WindfallShopPageView.cpp
// PURPOSE : The view that manages the windfall shop pages. They consist of tabs that contain either a carousel 
//			 shop page or the summary.
//
// NOTES   :
//
//           
//
// AUTHOR  :  Charalampos.Koundourakis
// STARTED :  May 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "WindfallShopPageView.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "WindfallShopPageView_parser.h"

// rage
#include "math/amath.h"
#include "scaleform/tweenstar.h"

// framework
#include "fwui/Input/fwuiInputQuantization.h"
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "control/ScriptRouterLink.h"
#include "Peds/CriminalCareer/CriminalCareerService.h"
#include "frontend/page_deck/IPageItemCollection.h"
#include "frontend/page_deck/IPageItemProvider.h"
#include "frontend/page_deck/IPageViewHost.h"
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "frontend/page_deck/PageItemBase.h"
#include "frontend/page_deck/PageItemCategoryBase.h"
#include "frontend/VideoEditor/ui/InstructionalButtons.h"
#include "frontend/page_deck/views/Carousel.h"
#include "frontend/page_deck/views/FeaturedImage.h"
#include "frontend/page_deck/views/FeaturedItem.h"
#include "frontend/page_deck/views/PageTitle.h"
#include "streaming/streaming.h"

#if RSG_BANK
#include "grcore/quads.h"
#endif // RSG_BANK

#include "system/control.h"
#include "system/controlMgr.h"

FWUI_DEFINE_TYPE(CWindfallShopPageView, 0xFD8ABDDB);

CWindfallShopPageView::CWindfallShopPageView()
	: superclass()
	, m_topLevelLayout(CPageGridSimple::GRID_3x5)
	, m_allowBackout(false)
{

}

CWindfallShopPageView::~CWindfallShopPageView()
{
	Cleanup();
}

void CWindfallShopPageView::RecalculateGrids()
{
	// Order is important here, as some of the sizing cascades
	m_topLevelLayout.Reset();

	// Screen area is acceptable as our grids are all using star sizing
	Vec2f const c_screenExtents = uiPageConfig::GetScreenArea();
	m_topLevelLayout.RecalculateLayout(Vec2f(Vec2f::ZERO), c_screenExtents);
	m_topLevelLayout.NotifyOfLayoutUpdate();
	
	Vec2f contentPos(Vec2f::ZERO);
	Vec2f contentSize(Vec2f::ZERO);
	GetContentArea(contentPos, contentSize);

	int const c_categoryCount = GetTabSourceCount();
	Vec2f const c_contentExtents = contentSize * Vec2f((float)c_categoryCount, 1.f);	
	m_scrollerObject.SetPositionInPixels(contentPos);
	m_bgScrollLayout.RecalculateLayout(contentPos, c_contentExtents);
}

void CWindfallShopPageView::OnFocusGainedDerived()
{
	CCarouselTabLayout* const activeTabLayout = GetActiveCarouselTabLayout();
	if (activeTabLayout)
	{
		activeTabLayout->RefreshFocusVisuals();
	}	
	GetPageRoot().SetVisible(true);
	UpdatePromptsAndTooltip();
}

void CWindfallShopPageView::OnEnterStartDerived()
{
	superclass::OnEnterStartDerived();

	// Request TXDs for chosen career
	const CCriminalCareerService& careerService = SCriminalCareerService::GetInstance();
	const CCriminalCareerService::ItemRefCollection* cp_items = careerService.TryGetItemsForChosenCareer();
	if ( uiVerifyf(cp_items != nullptr, "Entering CWindfallShopPageView without a chosen career") )
	{
		// Create TXD streaming requests for all items we will present
		const CCriminalCareerService::ItemRefCollection& c_items = *cp_items;
		const int c_itemsCount = c_items.GetCount();
		for (int i = 0; i < c_itemsCount; ++i)
		{
			const CCriminalCareerItem* cp_item = c_items[i];
			const char* const c_txdName = cp_item->GetTextureDictionaryName();
			uiCareerBuilderSupport::AddTxdRequest(c_txdName, m_txdRequests);
		}

		CStreaming::LoadAllRequestedObjects();
	}
}

void CWindfallShopPageView::UpdateDerived(float const deltaMs)
{
	superclass::UpdateDerived(deltaMs);	
	if (IsTransitioning())
	{
		float const c_normalizedDelta = GetTransitionDeltaNormalized();
		UpdateTabTransition(c_normalizedDelta);
	}
	else
	{
		UpdateInput();
	}
}

void CWindfallShopPageView::OnFocusLostDerived()
{
	CCarouselTabLayout* const activeTabLayout = GetActiveCarouselTabLayout();
	if (activeTabLayout)
	{
		activeTabLayout->ClearFocusVisuals();		
	}
	ClearTooltip();
	GetPageRoot().SetVisible(false);
	RefreshPrompts();	
}

void CWindfallShopPageView::PopulateDerived(IPageItemProvider& itemProvider)
{
	if( !IsPopulated() )
    {
        CComplexObject& pageRoot = GetPageRoot();

		m_pageTitle.Initialize(pageRoot);
		CPageLayoutItemParams const c_titleItemParams(1, 1, 1, 1);
		m_topLevelLayout.RegisterItem(m_pageTitle, c_titleItemParams);
		m_pageTitle.SetLiteral(itemProvider.GetTitleString());

		m_cashBalance.Initialize(pageRoot);
		CPageLayoutItemParams const c_cashBalanceParams(2, 1, 1, 1);
		m_topLevelLayout.RegisterItem(m_cashBalance, c_cashBalanceParams);		

		// Initialize our tab controller
		CPageLayoutItemParams const c_tabControllerParams(1, 2, 1, 1);
		InitializeTabController(pageRoot, m_topLevelLayout, c_tabControllerParams);

		m_scrollerObject = pageRoot.CreateEmptyMovieClip("landingViewScroller", -1);

		// Manually reset rows/columns, as we don't want a starting 1x1 grid
		m_bgScrollLayout.ResetRows();
		m_bgScrollLayout.ResetColumns();
		m_bgScrollLayout.AddRow(uiPageDeck::SIZING_TYPE_STAR, 1.f);
				
		CPageLayoutItemParams const c_tooltipParams(1, 4, 1, 1);
		InitializeTooltip(pageRoot, m_topLevelLayout, c_tooltipParams);

		size_t c_tabCount = itemProvider.GetCategoryCount();
		int currentTabIndex = 0;
		auto categoryTabFunc = [this, &currentTabIndex, c_tabCount](CPageItemCategoryBase& category)
		{
			++currentTabIndex;
			atHashString const c_title = category.GetTitle();
			AddTabItem(c_title);

			int const c_columnCount = (int)m_bgScrollLayout.GetColumnCount();
			m_bgScrollLayout.AddCol(uiPageDeck::SIZING_TYPE_STAR, 1.f);

			if (currentTabIndex < c_tabCount)
			{
				CCarouselTabLayout * const ShoppingCartPage = rage_new CCarouselTabLayout(m_scrollerObject, category);				
				m_carouselTabLayout.PushAndGrow(ShoppingCartPage);
				m_bgScrollLayout.RegisterItem(*ShoppingCartPage, CPageLayoutItemParams(c_columnCount, 0, 0, 0));
			}
			else
			{
				m_summaryTabLayout = rage_new CSummaryPageLayout(m_scrollerObject);
				m_bgScrollLayout.RegisterItem(*m_summaryTabLayout, CPageLayoutItemParams(c_columnCount, 0, 0, 0));
			}
		};
		itemProvider.ForEachCategory(categoryTabFunc);	
		u32 const c_tabFocus = GetDefaultTabFocus();
		GoToTab(c_tabFocus);
		RecalculateGrids();
		RefreshCarousel();
		uiCareerBuilderSupport::UpdateCashBalance(m_cashBalance);
    }
}

void CWindfallShopPageView::CleanupDerived()
{
	m_topLevelLayout.UnregisterAll();
	ShutdownTabController();
	ShutdownTooltip();

	m_bgScrollLayout.UnregisterAll();
	m_scrollerObject.Release();

	int carouselTabCount = m_carouselTabLayout.GetCount();
	while (carouselTabCount > 0)
	{
		CCarouselTabLayout * const currentCarouselTab = m_carouselTabLayout[carouselTabCount - 1];
		currentCarouselTab->Shutdown();
		delete currentCarouselTab;
		m_carouselTabLayout.DeleteFast(carouselTabCount - 1);
		--carouselTabCount;
	}
	m_pageTitle.Shutdown();
	m_summaryTabLayout->Shutdown();
	m_cashBalance.Shutdown();
	uiCareerBuilderSupport::ReleaseAllTxdRequests(m_txdRequests);
}

void CWindfallShopPageView::GetContentArea(Vec2f_InOut pos, Vec2f_InOut size) const
{
	m_topLevelLayout.GetCell(pos, size, 0, 2, 3, 3);
}

CCarouselTabLayout* CWindfallShopPageView::GetActiveCarouselTabLayout()
{
	int const c_currentTabIdx = GetActiveTabIndex();

	CCarouselTabLayout* const result = c_currentTabIdx >= 0 ? GetCarouselTabLayout((u32)c_currentTabIdx) : nullptr;
	return result;
}

CCarouselTabLayout const* CWindfallShopPageView::GetActiveCarouselTabLayout() const
{
	int const c_currentTabIdx = GetActiveTabIndex();

	CCarouselTabLayout const * const c_result = c_currentTabIdx >= 0 ? GetCarouselTabLayout((u32)c_currentTabIdx) : nullptr;
	return c_result;
}

CCarouselTabLayout* CWindfallShopPageView::GetCarouselTabLayout(u32 const index)
{
	CCarouselTabLayout* const result = index < m_carouselTabLayout.GetCount() ? m_carouselTabLayout[index] : nullptr;
	return result;
}

CCarouselTabLayout const* CWindfallShopPageView::GetCarouselTabLayout(u32 const index) const
{
	CCarouselTabLayout const * const c_result = index < m_carouselTabLayout.GetCount() ? m_carouselTabLayout[index] : nullptr;
	return c_result;
}

void CWindfallShopPageView::OnTransitionToTabStartDerived(u32 const tabLeavingIdx)
{
	CCarouselTabLayout* const tabLeaving = GetCarouselTabLayout(tabLeavingIdx);
	if (tabLeaving)
	{
		tabLeaving->InvalidateFocus();		
	}

	const int c_newTabIndex = GetActiveTabIndex();
	if (c_newTabIndex == m_bgScrollLayout.GetColumnCount() - 1)
	{
		m_summaryTabLayout->RefreshContent();		
	}
	else
	{
		CCarouselTabLayout* const tabEntering = GetCarouselTabLayout(c_newTabIndex);
		if (tabEntering)
		{
			tabEntering->SetDefaultFocus();
			tabEntering->UpdateFeaturedItem();
		}
	}
}

void CWindfallShopPageView::OnTransitionToTabEndDerived(u32 const tabEnteringIdx)
{
	GoToTab(tabEnteringIdx);
}

void CWindfallShopPageView::OnGoToTabDerived(u32 const index)
{
	Vec2f contentPos(Vec2f::ZERO);
	Vec2f contentSize(Vec2f::ZERO);
	GetContentArea(contentPos, contentSize);

	float const c_contentWidth = contentSize.GetX();
	float const c_endPosition = c_contentWidth * index;

	m_scrollerObject.SetXPositionInPixels(-c_endPosition);

	UpdatePromptsAndTooltip();
}

u32 CWindfallShopPageView::GetTabSourceCount() const
{
	u32 m_numberOfTabs = (u32)m_carouselTabLayout.GetCount();

	if (m_summaryTabLayout)
	{
		m_numberOfTabs += 1;
	}
	return m_numberOfTabs;
}

void CWindfallShopPageView::UpdateTabTransition(float const normalizedDelta)
{
	int const c_previousTabIndex = GetPreviousTabIndex();
	int const c_activeTabIndex = GetActiveTabIndex();

	Vec2f contentPos(Vec2f::ZERO);
	Vec2f contentSize(Vec2f::ZERO);
	GetContentArea(contentPos, contentSize);

	float const c_perPageOffset = 0.f - contentSize.GetX();

	float const c_startPosition = c_perPageOffset * c_previousTabIndex;
	float const c_endPosition = c_perPageOffset * c_activeTabIndex;

    float const c_quarticDelta = rage::TweenStar::ComputeEasedValue( normalizedDelta, rage::TweenStar::EASE_quartic_EO );
    float const c_finalX = rage::Lerp( c_quarticDelta, c_startPosition, c_endPosition );
    m_scrollerObject.SetXPositionInPixels( c_finalX );
}

void CWindfallShopPageView::UpdateInputDerived()
{
    superclass::UpdateInputDerived();

	if (IsOnSummaryPage())
	{
		fwuiInput::eHandlerResult result(fwuiInput::ACTION_NOT_HANDLED);
		if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT))
		{					
			result = HandleSummaryAction();
			TriggerInputAudio(result, UI_AUDIO_TRIGGER_SELECT, UI_AUDIO_TRIGGER_ERROR);
		}
	}
	else
	{
		CCarouselTabLayout* const activeTabLayout = GetActiveCarouselTabLayout();
		if (activeTabLayout)
		{
			fwuiInput::eHandlerResult inputResult(fwuiInput::ACTION_NOT_HANDLED);

			if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT))
			{
				inputResult = activeTabLayout->HandleInputAction(FRONTEND_INPUT_ACCEPT, GetViewHost());
				TriggerInputAudio(inputResult, UI_AUDIO_TRIGGER_SELECT, UI_AUDIO_TRIGGER_ERROR);
				// Toggling Curated Content can modify focus tooltip, refresh it now
				UpdatePromptsAndTooltip();
				uiCareerBuilderSupport::UpdateCashBalance(m_cashBalance);
			}
			else
			{
				// We only care about horizontal navigation between carousel items
				int deltaX = 0;

				// If CPauseMenu navigation results for analogue input are not accurate enough,
				// consider using the fwuiInput Quantization and direct reads of the analogue values
				if (CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT))
				{
					deltaX = -1;
				}
				else if (CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT))
				{
					deltaX = 1;
				}

				if (deltaX != 0)
				{
					inputResult = activeTabLayout->HandleCarouselDigitalNavigation(deltaX);
					TriggerInputAudio(inputResult, UI_AUDIO_TRIGGER_NAV_LEFT_RIGHT, nullptr);

					// Focus changed, so we need to re-calculate our tool-tip and prompts
					if (inputResult == fwuiInput::ACTION_HANDLED)
					{
						activeTabLayout->UpdateFeaturedItem();
						UpdatePromptsAndTooltip();
					}
				}
			}

			// Allow tab layout to query input for internal navigation
			activeTabLayout->UpdateInput();
		}
	}
}

fwuiInput::eHandlerResult CWindfallShopPageView::HandleSummaryAction()
{
	// Check to see if there are any errors around the cart otherwise allow the user to proceed to the character creator
	fwuiInput::eHandlerResult result(fwuiInput::ACTION_NOT_HANDLED);
	CCriminalCareerService& criminalCareerService = SCriminalCareerService::GetInstance();
	if (criminalCareerService.DoesShoppingCartPassValidation())
	{
		if (uiVerifyf(!ScriptRouterLink::HasPendingScriptRouterLink(), "Cannot route to Character Creator, pending link already exists"))
		{
			ScriptRouterContextData srcData(SRCS_NATIVE_LANDING_PAGE, SRCM_FREE, SRCA_CHARACTER_CREATOR, "none");
			ScriptRouterLink::SetScriptRouterLink(srcData);
			result = fwuiInput::ACTION_HANDLED;
			CGameSessionStateMachine::SetState(CGameSessionStateMachine::DISMISS_LANDING_PAGE);
		}
	}
	return result;
}

bool CWindfallShopPageView::IsOnSummaryPage()
{
	// Summary tab is fixed as the last entry
	return GetActiveTabIndex() == m_bgScrollLayout.GetColumnCount() - 1;
}

void CWindfallShopPageView::UpdateInstructionalButtonsDerived()
{
	atHashString const c_acceptLabelOverride = GetAcceptPromptLabelOverride();
	if (IsOnSummaryPage())
	{		
		if (m_summaryTabLayout) 
		{
			m_summaryTabLayout->UpdateInstructionalButtons(c_acceptLabelOverride);
		}
	}
	else
	{
		CCarouselTabLayout const * const c_activeTabLayout = GetActiveCarouselTabLayout();
		if (c_activeTabLayout)
		{
			c_activeTabLayout->UpdateInstructionalButtons();
		}
	}
	superclass::UpdateInstructionalButtonsDerived();
}

void CWindfallShopPageView::RefreshCarousel()
{	
	CCarouselTabLayout  * c_activeTabLayout = GetActiveCarouselTabLayout();
	if (c_activeTabLayout)
	{
		c_activeTabLayout->UpdateFeaturedItem();
	}
	UpdatePromptsAndTooltip();
}

void CWindfallShopPageView::UpdatePromptsAndTooltip()
{	
	if (IsOnSummaryPage())
	{		
		if (m_summaryTabLayout)
		{
			ConstString tooltip;
			m_summaryTabLayout->GetTooltipMessage(tooltip);
			if (tooltip)
			{
				SetTooltipLiteral(tooltip, true);
			}
			else
			{
				ClearTooltip();
			}
		}
	}
	else
	{
		CCarouselTabLayout* const c_activeTabLayout = GetActiveCarouselTabLayout();
		if (c_activeTabLayout)
		{
			char const * const c_tooltip = c_activeTabLayout->GetFocusTooltip();
			if (c_tooltip)
			{
				SetTooltipLiteral(c_tooltip, true);
			}
			else
			{
				ClearTooltip();
			}
		}
	}

	RefreshPrompts();
}

#if RSG_BANK

void CWindfallShopPageView::DebugRenderDerived() const
{
	m_topLevelLayout.DebugRender();
	m_bgScrollLayout.DebugRender();
}

#endif // RSG_BANK

#endif // UI_LANDING_PAGE_ENABLED && GEN9_UI_SIMULATION_ENABLED
