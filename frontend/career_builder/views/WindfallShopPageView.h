/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : WindfallShopPageView.h
// PURPOSE : The view that manages the windfall shop pages. They consist of tabs that contain either a carousel 
//			 shop page or the summary.
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : May 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef WINDFALL_SHOP_PAGE_VIEW
#define WINDFALL_SHOP_PAGE_VIEW

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// rage
#include "atl/array.h"

// game
#include "frontend/career_builder/layout/CarouselTabLayout.h"
#include "frontend/career_builder/layout/SummaryPageLayout.h"
#include "frontend/page_deck/layout/PageGrid.h"
#include "frontend/page_deck/layout/PageGridSimple.h"
#include "frontend/page_deck/PageViewBase.h"
#include "frontend/page_deck/views/TabbedPageView.h"
#include "frontend/page_deck/views/FeaturedImage.h"
#include "frontend/page_deck/views/FeaturedItem.h"
#include "frontend/page_deck/views/Tooltip.h"
#include "frontend/page_deck/views/PageTitle.h"
#include "frontend/career_builder/items/CashBalance.h"

class CWindfallShopPageView final : public CTabbedPageView
{
	FWUI_DECLARE_DERIVED_TYPE(CWindfallShopPageView, CTabbedPageView);
public:
	CWindfallShopPageView();
	virtual ~CWindfallShopPageView();	
private: // declarations and variables
	CPageGridSimple                     m_topLevelLayout;
	CPageGridSimple                     m_bgScrollLayout;
	CComplexObject                      m_scrollerObject;
	CPageTitle							m_pageTitle;
	atArray<CCarouselTabLayout*>		m_carouselTabLayout;
	CSummaryPageLayout *				m_summaryTabLayout;
	CCashBalance						m_cashBalance;

	CareerBuilderDefs::TxdRequestCollection m_txdRequests;

	PAR_PARSABLE;
	NON_COPYABLE(CWindfallShopPageView);

private: // methods
	void RecalculateGrids();	

	void OnEnterStartDerived() final;
	void UpdateDerived(float const deltaMs) final;

	void OnFocusGainedDerived() final;
	void OnFocusLostDerived() final;	

	void PopulateDerived(IPageItemProvider& itemProvider) final;
	bool IsPopulatedDerived() const { return m_carouselTabLayout.GetCount() > 0; }
	void CleanupDerived() final;

	void GetContentArea(Vec2f_InOut pos, Vec2f_InOut size) const;
	CCarouselTabLayout* GetActiveCarouselTabLayout();
	CCarouselTabLayout const* GetActiveCarouselTabLayout() const;
	CCarouselTabLayout* GetCarouselTabLayout(u32 const index);
	CCarouselTabLayout const* GetCarouselTabLayout(u32 const index) const;


	u32 GetTabSourceCount() const final;
	void OnTransitionToTabStartDerived(u32 const tabLeavingIdx) final;
	void OnTransitionToTabEndDerived(u32 const tabEnteringIdx) final;
	void OnGoToTabDerived(u32 const index) final;
	void UpdateTabTransition(float const normalizedDelta);

	void UpdateInputDerived() final;
	fwuiInput::eHandlerResult HandleSummaryAction();
	bool IsOnSummaryPage();
	void UpdateInstructionalButtonsDerived() final;

	void UpdatePromptsAndTooltip();
	void RefreshCarousel();
	
	bool                    m_allowBackout;


#if RSG_BANK
	void DebugRenderDerived() const final;
#endif 	
};

#endif // UI_LANDING_PAGE_ENABLED && GEN9_UI_SIMULATION_ENABLED

#endif // WINDFALL_SHOP_PAGE_VIEW
