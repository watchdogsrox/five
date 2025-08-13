#include "CareerBuilderCategory.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/career_builder/items/uiCareerBuilderSupport.h"
#include "frontend/page_deck/PageItemBase.h"
#include "frontend/ui_channel.h"

#if UI_PAGE_DECK_ENABLED

CCareerBuilderCategory::CCareerBuilderCategory(CareerBuilderDefs::CategoryType categoryType) :
	superclass(),
	m_careerBuilderCategoryType(categoryType)
{
}

CCareerBuilderCategory::CCareerBuilderCategory(CareerBuilderDefs::CategoryType categoryType, const CriminalCareerDefs::ItemCategory& itemCategory) :
	superclass(),
	m_careerBuilderCategoryType(categoryType),
	m_itemCategory(itemCategory)
{
}

CCareerBuilderCategory::CCareerBuilderCategory(CareerBuilderDefs::CategoryType categoryType, const uiPageId& careerShopPageId) :
	superclass(),
	m_careerBuilderCategoryType(categoryType),
	m_careerShopPageId(careerShopPageId)
{
}

CCareerBuilderCategory::~CCareerBuilderCategory()
{
	uiCareerBuilderSupport::ClearCareerData(m_careerBuilderItems);
}

CCareerBuilderCategory::RequestStatus CCareerBuilderCategory::RequestItemData(RequestOptions UNUSED_PARAM(options))
{
	uiCareerBuilderSupport::ClearCareerData(m_careerBuilderItems);

	// Data is populated from CriminalCareerCatalog depending on category type. Some types also depend on career selection
	switch (m_careerBuilderCategoryType)
	{
	case CareerBuilderDefs::CategoryType::CareerSelection:
		return uiCareerBuilderSupport::RequestCareerSelectionData(m_careerBuilderItems, m_careerShopPageId);
	case CareerBuilderDefs::CategoryType::ShoppingCartTab:
		return uiCareerBuilderSupport::RequestShoppingCartTabData(m_itemCategory, m_careerBuilderItems);
	case CareerBuilderDefs::CategoryType::ShoppingCartSummary:
		return uiCareerBuilderSupport::RequestShoppingCartSummaryData(m_careerBuilderItems);
	}

	return RequestStatus::FAILED;
}

CCareerBuilderCategory::RequestStatus CCareerBuilderCategory::UpdateRequest()
{
	return RequestStatus::SUCCESS;
}

CCareerBuilderCategory::RequestStatus CCareerBuilderCategory::MarkAsTimedOut()
{
	return RequestStatus::SUCCESS;
}

CCareerBuilderCategory::RequestStatus CCareerBuilderCategory::GetRequestStatus() const
{
	return RequestStatus::SUCCESS;
}

void CCareerBuilderCategory::ForEachItemDerived(PerItemLambda action)
{
	for (CareerBuilderDefs::ItemIterator itr = m_careerBuilderItems.begin(); itr != m_careerBuilderItems.end(); ++itr)
	{
		if (*itr)
		{
			action(**itr);
		}
	}
}

void CCareerBuilderCategory::ForEachItemDerived(PerItemConstLambda action) const
{
	for (CareerBuilderDefs::ConstItemIterator itr = m_careerBuilderItems.begin(); itr != m_careerBuilderItems.end(); ++itr)
	{
		if (*itr)
		{
			action(**itr);
		}
	}
}

#endif // UI_PAGE_DECK_ENABLED
