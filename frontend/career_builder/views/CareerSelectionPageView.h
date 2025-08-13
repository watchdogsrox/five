/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CareerSelectionPageView.h
// PURPOSE : The pillar view that showcases the available careers and the windfall cash available
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef CAREER_SELECTION_PAGE_VIEW_H
#define CAREER_SELECTION_PAGE_VIEW_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "atl/array.h"

// game
#include "frontend/page_deck/layout/PageGridSimple.h"
#include "frontend/page_deck/views/SingleLayoutView.h"
#include "frontend/career_builder/items/CashBalance.h"

class CCareerSelectionPageView final : public CSingleLayoutView
{
	FWUI_DECLARE_DERIVED_TYPE(CCareerSelectionPageView, CSingleLayoutView);
public:
	CCareerSelectionPageView();
	virtual ~CCareerSelectionPageView();

private: // declarations and variables
	CPageGridSimple         m_topLevelLayout;
	CPageGridSimple         m_contentLayout;
	CComplexObject          m_contentObject;
	CCashBalance			m_cashBalance;
	PAR_PARSABLE;
	NON_COPYABLE(CCareerSelectionPageView);

private: // methods	
	void RecalculateGrids();
	void GetContentArea(Vec2f_InOut pos, Vec2f_InOut size) const;

	void PopulateDerived(IPageItemProvider& itemProvider) final;	
	void CleanupDerived() final;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // CAREER_SELECTION_PAGE_VIEW_H
