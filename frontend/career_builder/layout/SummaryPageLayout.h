/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SummaryPageLayout.h
// PURPOSE : Collection of items that form the summary Page in the windfall shop
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef SUMMARY_PAGE_LAYOUT_H
#define SUMMARY_PAGE_LAYOUT_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/array.h"

// framework
#include "fwui/Input/fwuiInputEnums.h"

// game
#include "frontend/input/uiInputEnums.h"
#include "frontend/page_deck/views/Tooltip.h"
#include "frontend/page_deck/layout/PageLayoutItem.h"
#include "frontend/career_builder/items/SummaryButton.h"
#include "frontend/career_builder/items/SummaryCashTotals.h"
#include "frontend/career_builder/layout/SummaryLayoutItem.h"
#include "frontend/career_builder/items/ParallaxImage.h"

class CPageItemCategoryBase;
class CPageLayoutBase;
class CPageLayoutDecoratorItem;
class IPageMessageHandler;

// Inherits from layout item so we can easily cascade layout resolves
class CSummaryPageLayout final : public CPageLayoutItem
{
	typedef CPageLayoutItem superclass;
public:	
	CSummaryPageLayout(CComplexObject & root);
	virtual ~CSummaryPageLayout();

	void UpdateInstructionalButtons(atHashString const acceptLabelOverride) const;
	void GetTooltipMessage(ConstString & tooltip) const;
	
	void RefreshContent();

private: // declarations and variables
	CPageGridSimple						m_topLevelLayout;
	CPageGridSimple						m_backgroundLayout;

	CSummaryCashTotals					m_giftCash;
	CSummaryCashTotals					m_totalSpent;
	CSummaryCashTotals					m_cashRemaining;
	CSummaryButton						m_Button;
	CParallaxImage						m_parallaxImage;
	atArray<CSummaryLayoutItem*>		m_items;	

	bool m_itemQuantityExceeded;
	bool m_itemQuantityInsufficient;
	bool m_totalSpendExceeded;
	bool m_totalSpendInsufficient;

	NON_COPYABLE(CSummaryPageLayout);

private:
	// methods	
	void ResetItems();
	void RecalculateGrids();
	void ResolveLayoutDerived(Vec2f_In PagespacePosPx, Vec2f_In localPosPx, Vec2f_In sizePx);
	void AddSummaryItem(const CriminalCareerDefs::ItemCategory & itemCategory, const CCriminalCareerService::ValidationErrorArray & validationErrors);
	void PopulateCashTotal(CSummaryCashTotals& entry, const char* const label, rage::s64 value, bool isError);
};

#endif // UI_PAGE_DECK_ENABLED

#endif // SUMMARY_PAGE_LAYOUT_H
