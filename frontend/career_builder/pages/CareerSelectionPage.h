/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CareerSelectionPage.h
// PURPOSE : Page to dynamically display list of Criminal Careers.
//
// AUTHOR  : stephen.phillips
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CAREER_SELECTION_PAGE
#define CAREER_SELECTION_PAGE

// game
#include "frontend/career_builder/items/CareerBuilderDefs.h"
#include "frontend/page_deck/PageBase.h"
#include "frontend/page_deck/uiPageConfig.h"

#if UI_PAGE_DECK_ENABLED

class CCareerSelectionPage final : public CPageBase
{
	FWUI_DECLARE_DERIVED_TYPE(CCareerSelectionPage, CPageBase);
public:
	CCareerSelectionPage() : superclass() {}
	virtual ~CCareerSelectionPage();

private: // declarations and variables
	CareerBuilderDefs::CategoryCollection m_categories;

	uiPageId m_careerShopPageId;

	PAR_PARSABLE;
	NON_COPYABLE(CCareerSelectionPage);

private: // methods
	void OnEnterStartDerived() final;

	void PopulateCategories();
	void ClearCategories();

	size_t GetCategoryCount() const final;
	void ForEachCategoryDerived(PerCategoryLambda action) final;
	void ForEachCategoryDerived(PerCategoryConstLambda action) const final;

	bool IsTransitoryPageDerived() const final { return false; }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // CAREER_SELECTION_PAGE
