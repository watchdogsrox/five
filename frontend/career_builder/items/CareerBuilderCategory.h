/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CareerBuilderCategory.h
// PURPOSE : An item category populated from the CriminalCareer catalog.
//
// AUTHOR  : stephen.phillips
// STARTED : May 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CAREER_BUILDER_CATEGORY
#define CAREER_BUILDER_CATEGORY

// framework
#include "fwutil/Gen9Settings.h"

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/career_builder/items/CareerBuilderDefs.h"
#include "frontend/page_deck/items/StaticDataCategory.h"
#include "Peds/CriminalCareer/CriminalCareerDefs.h"

class CCareerBuilderCategory : public CPageItemCategoryBase
{
	typedef CPageItemCategoryBase superclass;
public:
	CCareerBuilderCategory(CareerBuilderDefs::CategoryType categoryType);
	CCareerBuilderCategory(CareerBuilderDefs::CategoryType categoryType, const CriminalCareerDefs::ItemCategory& itemCategory);
	CCareerBuilderCategory(CareerBuilderDefs::CategoryType categoryType, const uiPageId& careerShopPageId);
	virtual ~CCareerBuilderCategory();

	RequestStatus RequestItemData(RequestOptions const options) final;
	RequestStatus UpdateRequest() final;
	RequestStatus MarkAsTimedOut() final;
	RequestStatus GetRequestStatus() const final;

private: // declarations and variables
	CareerBuilderDefs::ItemCollection  m_careerBuilderItems;

	CareerBuilderDefs::CategoryType m_careerBuilderCategoryType;
	CriminalCareerDefs::ItemCategory m_itemCategory;
	uiPageId m_careerShopPageId;

private: // methods
	void ForEachItemDerived(superclass::PerItemLambda action) final;
	void ForEachItemDerived(superclass::PerItemConstLambda action) const final;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // CAREER_BUILDER_CATEGORY
