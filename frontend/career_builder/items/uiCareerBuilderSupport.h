/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiCareerBuilderSupport.h
// PURPOSE : Collection of helper methods to populate UI from Career Builder data.
//
// AUTHOR  : stephen.phillips
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef UI_CAREER_BUILDER_SUPPORT
#define UI_CAREER_BUILDER_SUPPORT

// game
#include "frontend/career_builder/items/CareerBuilderDefs.h"
#include "frontend/page_deck/IPageItemCollection.h"
#include "Peds/CriminalCareer/CriminalCareerService.h"
#include "frontend/career_builder/items/CashBalance.h"
#if UI_PAGE_DECK_ENABLED

namespace uiCareerBuilderSupport
{
	IPageItemCollection::RequestStatus RequestCareerSelectionData(CareerBuilderDefs::ItemCollection& out_itemCollection, const uiPageId& careerShopPageId);
	IPageItemCollection::RequestStatus RequestShoppingCartTabData(const CriminalCareerDefs::ItemCategory& itemCategory, CareerBuilderDefs::ItemCollection& out_itemCollection);
	IPageItemCollection::RequestStatus RequestShoppingCartSummaryData(CareerBuilderDefs::ItemCollection& out_itemCollection);

	bool FillCareerSelectionData(const CCriminalCareerService::CareerCollection& careerCollection, const uiPageId& careerShopPageId, CareerBuilderDefs::ItemCollection& out_itemCollection);
	bool FillShoppingCartTabData(const CriminalCareerDefs::ItemRefCollection& itemCollection, const CriminalCareerDefs::ItemCategoryDisplayData& categoryDisplayData, CareerBuilderDefs::ItemCollection& out_itemCollection);

	void ClearCareerData(CareerBuilderDefs::ItemCollection& ref_itemCollection);

	void CreateItemDescriptionRichText(const CCriminalCareerItem& item, rage::atStringBuilder& stringBuilder);
	void CreateItemCostRichText(const CCriminalCareerItem & item, rage::atStringBuilder & stringBuilder);
	void AddTxdRequest(atHashString txdNameHash, CareerBuilderDefs::TxdRequestCollection& ref_txdRequests);
	void ReleaseAllTxdRequests(CareerBuilderDefs::TxdRequestCollection& ref_txdRequests);
	void UpdateCashBalance(CCashBalance& cashBalance);
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_CAREER_BUILDER_SUPPORT
