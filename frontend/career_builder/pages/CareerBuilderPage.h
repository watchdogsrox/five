/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CareerBuilderPage.h
// PURPOSE : Page to dynamically load the chosen Career's Curated Content.
//
// AUTHOR  : stephen.phillips
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CAREER_BUILDER_PAGE
#define CAREER_BUILDER_PAGE

// game
#include "frontend/career_builder/items/CareerBuilderDefs.h"
#include "frontend/page_deck/PageBase.h"
#include "frontend/page_deck/uiPageConfig.h"

#if UI_PAGE_DECK_ENABLED

class CCareerBuilderPage final : public CPageBase
{
	FWUI_DECLARE_DERIVED_TYPE(CCareerBuilderPage, CPageBase);
public:
	CCareerBuilderPage() : superclass() {}
	virtual ~CCareerBuilderPage();

private: // declarations and variables
	CareerBuilderDefs::CategoryCollection m_categories;
	
	PAR_PARSABLE;
	NON_COPYABLE(CCareerBuilderPage);

private: // methods
	void OnEnterStartDerived() final;

	void PopulateWithCareer(const CCriminalCareer& career);
	void ClearCategories();

	size_t GetCategoryCount() const final;
	void ForEachCategoryDerived(PerCategoryLambda action) final;
	void ForEachCategoryDerived(PerCategoryConstLambda action) const final;

	bool IsTransitoryPageDerived() const final { return false; }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // CAREER_BUILDER_PAGE
