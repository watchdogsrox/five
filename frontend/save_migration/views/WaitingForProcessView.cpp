#include "WaitingForProcessView.h"

#if GEN9_LANDING_PAGE_ENABLED

#include "WaitingForProcessView_parser.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"
#include "frontend/NewHud.h"
#include "frontend/page_deck/IPageViewHost.h"
#include "frontend/PauseMenu.h"
#include "frontend/SocialClubMenu.h"

FWUI_DEFINE_TYPE(CWaitingForProcessView, 0xE11D96A9);

CWaitingForProcessView::CWaitingForProcessView() :
	superclass(),
	m_layout(CPageGridSimple::GRID_3x5),
	m_isPopulated(false)
{
}

CWaitingForProcessView::~CWaitingForProcessView()
{
	Cleanup();
}

void CWaitingForProcessView::PopulateDerived(IPageItemProvider& itemProvider)
{
	if (!IsPopulated())
	{
		CComplexObject& pageRoot = GetPageRoot();

		CPageLayoutItemParams const c_tooltipParams(1, 4, 1, 1);
		InitializeTooltip(pageRoot, m_layout, c_tooltipParams);
		SetTooltipLiteral(itemProvider.GetTitleString(), false);

		m_layout.Reset();

		Vec2f const c_screenExtents = uiPageConfig::GetScreenArea();
		m_layout.RecalculateLayout(Vec2f(Vec2f::ZERO), c_screenExtents);
		m_layout.NotifyOfLayoutUpdate();

		m_isPopulated = true;
	}
}

void CWaitingForProcessView::OnFocusGainedDerived()
{
	if (IsPopulated())
	{
		SetTooltipLiteral(GetItemProvider().GetTitleString(), false);
	}
}

bool CWaitingForProcessView::IsPopulatedDerived() const
{
	return m_isPopulated;
}

void CWaitingForProcessView::OnFocusLostDerived()
{
	if (IsPopulated())
	{
		ClearTooltip();
	}
}

void CWaitingForProcessView::CleanupDerived()
{
	m_layout.UnregisterAll();
	ShutdownTooltip();
	m_isPopulated = false;
}

#endif //  GEN9_LANDING_PAGE_ENABLED
