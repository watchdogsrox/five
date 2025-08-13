/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CareerSelectionPageView.cpp
// PURPOSE : The pillar view that showcases the available careers and the windfall cash available
//
// NOTES   :
//
// AUTHOR  :  Charalampos.Koundourakis
// STARTED :  July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "CareerSelectionPageView.h"

#if UI_PAGE_DECK_ENABLED
#include "CareerSelectionPageView_parser.h"

// framework
#include "fwutil/xmacro.h"

FWUI_DEFINE_TYPE(CCareerSelectionPageView, 0xD875FE98);

CCareerSelectionPageView::CCareerSelectionPageView()
	: superclass()
	, m_topLevelLayout(CPageGridSimple::GRID_3x8)
	, m_contentLayout(CPageGridSimple::GRID_9x1, "careersItem")
{

}

CCareerSelectionPageView::~CCareerSelectionPageView()
{
	Cleanup();
}

void CCareerSelectionPageView::RecalculateGrids()
{
	// Order is important here, as some of the sizing cascades
	// So if you adjust the order TEST IT!
	m_topLevelLayout.Reset();

	// Screen area is acceptable as our grids are all using star sizing
	Vec2f const c_screenExtents = uiPageConfig::GetScreenArea();
	m_topLevelLayout.RecalculateLayout(Vec2f(Vec2f::ZERO), c_screenExtents);
	m_topLevelLayout.NotifyOfLayoutUpdate();

	Vec2f contentPos(Vec2f::ZERO);
	Vec2f contentSize(Vec2f::ZERO);
	GetContentArea(contentPos, contentSize);

	m_contentObject.SetPositionInPixels(contentPos);
	m_contentLayout.RecalculateLayout(contentPos, contentSize);
	m_contentLayout.NotifyOfLayoutUpdate();
}

void CCareerSelectionPageView::GetContentArea(Vec2f_InOut pos, Vec2f_InOut size) const
{
	m_topLevelLayout.GetCell(pos, size, 0, 3, 3, 3);
}

void CCareerSelectionPageView::PopulateDerived(IPageItemProvider& itemProvider)
{
	if (!IsPopulated())
	{
		CComplexObject& pageRoot = GetPageRoot();

		CPageLayoutItemParams const c_tooltipParams(1, 6, 1, 1);
		InitializeTooltip(pageRoot, m_topLevelLayout, c_tooltipParams);

		CPageLayoutItemParams const c_titleItemParams(1, 1, 1, 1);
		InitializeTitle(pageRoot, m_topLevelLayout, c_titleItemParams);
		SetTitleLiteral(itemProvider.GetTitleString());

		m_cashBalance.Initialize(pageRoot);
		CPageLayoutItemParams const c_cashBalanceParams(2, 1, 1, 1);
		m_topLevelLayout.RegisterItem(m_cashBalance, c_cashBalanceParams);

		m_contentObject = pageRoot.CreateEmptyMovieClip("contentArea", -1);
		InitializeLayoutContext(itemProvider, m_contentObject, m_contentLayout);
		RecalculateGrids();
		uiCareerBuilderSupport::UpdateCashBalance(m_cashBalance);
	}
}

void CCareerSelectionPageView::CleanupDerived()
{
#if IS_GEN9_PLATFORM
	// Cleanup only called when page is removed from stack, so navigating backwards or leaving LP
	g_InteractiveMusicManager.GetEventManager().TriggerEvent("MP_LNDP_MAIN_MUSIC_START");
#endif

	m_topLevelLayout.UnregisterAll();	
	m_cashBalance.Shutdown();
	superclass::CleanupDerived();
}

#endif // UI_LANDING_PAGE_ENABLED && GEN9_UI_SIMULATION_ENABLED
