#include "uiCareerBuilderSupport.h"

// framework
#include "fwscene/stores/txdstore.h"

// game
#include "frontend/career_builder/items/CareerBuilderShoppingCartItem.h"
#include "frontend/career_builder/messages/uiCareerBuilderMessages.h"
#include "frontend/FrontendStatsMgr.h"
#include "frontend/page_deck/items/CloudCardItem.h"
#include "frontend/ui_channel.h"
#include "Stats/MoneyInterface.h"

#if UI_PAGE_DECK_ENABLED

IPageItemCollection::RequestStatus uiCareerBuilderSupport::RequestCareerSelectionData(CareerBuilderDefs::ItemCollection& out_itemCollection, const uiPageId& careerShopPageId)
{
	const CCriminalCareerService& careerService = SCriminalCareerService::GetInstance();
	const bool c_success = FillCareerSelectionData(careerService.GetAllCareers(), careerShopPageId, out_itemCollection);

	if (c_success)
	{
		return IPageItemCollection::RequestStatus::SUCCESS;
	}

	return IPageItemCollection::RequestStatus::FAILED;
}

int GetCompareToSortResultAscending(int lhs, int rhs)
{
	if (lhs < rhs)
	{
		return -1;
	}
	else if (lhs == rhs)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

int GetCompareToSortResultDescending(int lhs, int rhs)
{
	// Invert result to sort descending
	return -1 * GetCompareToSortResultAscending(lhs, rhs);
}

int SortByIndexAscending(const CCriminalCareerItem* const* lhs, const CCriminalCareerItem* const* rhs)
{
	if (uiVerifyf(lhs != nullptr && rhs != nullptr, "Encountered null element(s) when attempting to sort collection"))
	{
		const CCriminalCareerItem* const p_lhs = *lhs;
		const CCriminalCareerItem* const p_rhs = *rhs;
		const int c_indexLhs = p_lhs->GetCarouselIndexOverride();
		const int c_indexRhs = p_rhs->GetCarouselIndexOverride();

		return GetCompareToSortResultAscending(c_indexLhs, c_indexRhs);
	}
	return 0;
}

int SortByPriceDescending(const CCriminalCareerItem* const* lhs, const CCriminalCareerItem* const* rhs)
{
	if (uiVerifyf(lhs != nullptr && rhs != nullptr, "Encountered null element(s) when attempting to sort collection"))
	{
		const CCriminalCareerItem* const p_lhs = *lhs;
		const CCriminalCareerItem* const p_rhs = *rhs;
		// CESP does not affect the ordering of items
		const int c_costLhs = p_lhs->GetDefaultCost();
		const int c_costRhs = p_rhs->GetDefaultCost();

		return GetCompareToSortResultDescending(c_costLhs, c_costRhs);
	}
	return 0;
}

int GetLowestValue(int a, int b)
{
	return (a < b) ? a : b;
}

IPageItemCollection::RequestStatus uiCareerBuilderSupport::RequestShoppingCartTabData(const CriminalCareerDefs::ItemCategory& itemCategory, CareerBuilderDefs::ItemCollection& out_itemCollection)
{
	const CCriminalCareerService& careerService = SCriminalCareerService::GetInstance();
	// Build page items from catalog data
	const CriminalCareerDefs::ItemRefCollection* p_currentItemCollection = careerService.TryGetItemsForChosenCareerWithCategory(itemCategory);
	const CriminalCareerDefs::ItemCategoryDisplayData* p_categoryDisplayData = careerService.TryGetDisplayDataForCategory(itemCategory);

	if (uiVerifyf(p_currentItemCollection, "Failed to retrieve items for category " HASHFMT, HASHOUT(itemCategory))
		&& uiVerifyf(p_categoryDisplayData, "Failed to retrieve display data for category " HASHFMT, HASHOUT(itemCategory)))
	{
		CriminalCareerDefs::ItemRefCollection sortedItems(*p_currentItemCollection);

		// Extract items to place in specific order instead of sorting
		CriminalCareerDefs::ItemRefCollection itemsWithFixedPositions;
		const int c_allItemsCount = sortedItems.GetCount();
		for (int i = c_allItemsCount - 1; i >= 0; --i)
		{
			if (sortedItems[i]->GetCarouselIndexOverride() != -1)
			{
				itemsWithFixedPositions.PushAndGrow(sortedItems[i]);
				sortedItems.Delete(i);
			}
		}

		// Sort remaining items by price from high to low
		sortedItems.QSort(0, -1, SortByPriceDescending);

		// Sort fixed order items so they can be inserted neatly
		itemsWithFixedPositions.QSort(0, -1, SortByIndexAscending);

		// Insert fixed items into sorted array
		const int c_itemsWithFixedPositionsCount = itemsWithFixedPositions.GetCount();
		for (int i = 0; i < c_itemsWithFixedPositionsCount; ++i)
		{
			const CCriminalCareerItem* cp_item = itemsWithFixedPositions[i];
			// Ensure we insert into the bounds of the collection. This should have thrown an assert in CCriminalCareerCatalog::PostLoad if invalid
			const int c_index = GetLowestValue(cp_item->GetCarouselIndexOverride(), c_allItemsCount - 1);
			sortedItems.Insert(c_index) = cp_item;
		}

		FillShoppingCartTabData(sortedItems, *p_categoryDisplayData, out_itemCollection);
		return IPageItemCollection::RequestStatus::SUCCESS;
	}

	return IPageItemCollection::RequestStatus::FAILED;
}

IPageItemCollection::RequestStatus uiCareerBuilderSupport::RequestShoppingCartSummaryData(CareerBuilderDefs::ItemCollection& UNUSED_PARAM( out_itemCollection ))
{
	// NOOP, Summary items are filled directly in SummaryPageLayout
	return IPageItemCollection::RequestStatus::SUCCESS;
}

bool uiCareerBuilderSupport::FillCareerSelectionData(const CCriminalCareerService::CareerCollection& careerCollection, const uiPageId& careerShopPageId, CareerBuilderDefs::ItemCollection& out_itemCollection)
{
	const int c_careerCollectionCount = careerCollection.GetCount();

	for (unsigned i = 0; i < c_careerCollectionCount; ++i)
	{
		const CCriminalCareer& c_career = careerCollection[i];
		CCareerPillarItem* ci = rage_new CCareerPillarItem();

		// Assign card data here
		ci->Title() = TheText.Get(c_career.GetDisplayNameKey());
		ci->Description() = TheText.Get(c_career.GetDescriptionKey());
		ci->Txd() = c_career.GetTextureDictionaryName();
		ci->Texture() = c_career.GetPreviewImagePath();
		ci->Tooltip() = TheText.Get(c_career.GetTooltipSelectKey());
		ci->InteractiveMusicTrigger() = c_career.GetInteractiveMusicTrigger();

		uiPageLink pageLink(careerShopPageId);
		uiPageDeckMessageBase* onSelectMessage = rage_new uiSelectCareerMessage(c_career.GetIdentifier(), pageLink);
		ci->SetOnSelectMessage(onSelectMessage);

		out_itemCollection.PushAndGrow(ci);
	}

	return true;
}

bool uiCareerBuilderSupport::FillShoppingCartTabData(const CriminalCareerDefs::ItemRefCollection& itemCollection, const CriminalCareerDefs::ItemCategoryDisplayData& categoryDisplayData, CareerBuilderDefs::ItemCollection& out_itemCollection)
{
	const int c_itemCollectionCount = itemCollection.GetCount();
	for (unsigned i = 0; i < c_itemCollectionCount; ++i)
	{
		const CCriminalCareerItem* cp_item = itemCollection[i];

		if (cp_item != nullptr)
		{
			const CCriminalCareerItem& c_item = *cp_item;
			CCareerBuilderShoppingCartItem* ci = rage_new CCareerBuilderShoppingCartItem();

			// Assign card data here
			ci->AssignItemIdentifier(c_item.GetIdentifier());

			ci->Title() = TheText.Get(c_item.GetTitleKey());
			rage::atStringBuilder descriptionBuilder;
			CreateItemDescriptionRichText(c_item, descriptionBuilder);
			ci->Description() = descriptionBuilder.ToString();

			rage::atStringBuilder costBuilder;
			CreateItemCostRichText(c_item, costBuilder);
			ci->Cost() = costBuilder.ToString();
			ci->SetTooltipData(categoryDisplayData.GetTooltipAddKey(), categoryDisplayData.GetTooltipRemoveKey());

			const char* const c_txdName = c_item.GetTextureDictionaryName();
			ci->SetTextureInformation(c_txdName, c_item.GetThumbnailPath(), c_item.GetBackgroundImagePaths());
			if (c_item.GetVehicleStats().HasAnyValues())
			{
				ci->SetStatsData(c_item.GetVehicleStats());
			}
			else if (c_item.GetWeaponStats().HasAnyValues())
			{
				ci->SetStatsData(c_item.GetWeaponStats());
			}

			uiPageDeckMessageBase* onSelectMessage = rage_new uiSelectCareerItemMessage(c_item.GetIdentifier());
			ci->SetOnSelectMessage(onSelectMessage);
			out_itemCollection.PushAndGrow(ci);
		}
	}

	return true;
}

void uiCareerBuilderSupport::ClearCareerData(CareerBuilderDefs::ItemCollection& ref_itemCollection)
{
	for (CareerBuilderDefs::ItemIterator itr = ref_itemCollection.begin(); itr != ref_itemCollection.end(); ++itr)
	{
		delete *itr;
	}
	ref_itemCollection.ResetCount();
}

void uiCareerBuilderSupport::CreateItemDescriptionRichText(const CCriminalCareerItem& item, rage::atStringBuilder& stringBuilder)
{
	const CCriminalCareerService& criminalCareerService = SCriminalCareerService::GetInstance();
	int numberOfItemsAdded = 0;
	auto appendFeatureAction = [&stringBuilder, &numberOfItemsAdded](const CriminalCareerDefs::ItemFeature& itemFeature)
	{
		if (numberOfItemsAdded > 0)
		{
			const int c_itemsPerRow = 3;
			const char* const c_sep = (numberOfItemsAdded % c_itemsPerRow == 0) ? "~n~" : "  ";
			stringBuilder.Append(c_sep);
		}
		// Formats as "~BLIP_NAME~ Display Name"
		const char* const c_blipName = itemFeature.GetBlipName();
		stringBuilder.Append('~');
		stringBuilder.Append(c_blipName);
		stringBuilder.Append("~ ");
		const char* const c_displayText = TheText.Get(itemFeature.GetDisplayNameKey());
		stringBuilder.Append(c_displayText);
		++numberOfItemsAdded;
	};

	criminalCareerService.ForEachFeatureOfItem(item, appendFeatureAction);

	if (numberOfItemsAdded > 0)
	{
		stringBuilder.Append("~n~"); // Newline
	}
	const char* const c_descriptionText = TheText.Get(item.GetDescriptionKey());
	stringBuilder.Append(c_descriptionText);
}

void uiCareerBuilderSupport::CreateItemCostRichText(const CCriminalCareerItem& item, rage::atStringBuilder& stringBuilder)
{
	const int c_costBufferLength = 17; // Space to store "$999,999,999,999\0"
	char costString[c_costBufferLength];
	CFrontendStatsMgr::FormatInt64ToCash(item.GetCost(), costString, c_costBufferLength);

	stringBuilder.Append("~HUD_COLOUR_GREENLIGHT~");
	stringBuilder.Append(costString);
	stringBuilder.Append("~s~");
}

void uiCareerBuilderSupport::AddTxdRequest(atHashString txdNameHash, CareerBuilderDefs::TxdRequestCollection& ref_txdRequests)
{
	// If request does not exist yet, request it now
	if (!ref_txdRequests.Has(txdNameHash))
	{
		const rage::strLocalIndex c_strLocalIndex = g_TxdStore.FindSlotFromHashKey(txdNameHash);
		if (uiVerifyf(c_strLocalIndex != -1, "Failed to retrieve streaming slot for TXD \"%s\"", txdNameHash.TryGetCStr()))
		{
			const int c_streamingModuleId = g_TxdStore.GetStreamingModuleId();
			const u32 c_strFlags = STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE;
			rage::strRequest* p_txdRequest = ref_txdRequests.Insert(txdNameHash);
			p_txdRequest->Request(c_strLocalIndex, c_streamingModuleId, c_strFlags);
		}
	}
}

void uiCareerBuilderSupport::ReleaseAllTxdRequests(CareerBuilderDefs::TxdRequestCollection& ref_txdRequests)
{
	for (CareerBuilderDefs::TxdRequestCollection::Iterator it = ref_txdRequests.Begin(); it != ref_txdRequests.End(); ++it)
	{
		rage::strRequest& txdRequest = *it;
		txdRequest.ClearRequiredFlags(STRFLAG_DONTDELETE);
		txdRequest.Release();
	}
	ref_txdRequests.Reset();
}

void uiCareerBuilderSupport::UpdateCashBalance(CCashBalance& cashBalance)
{
	fwTemplateString<RAGE_MAX_PATH> cashRemainingColourString;	
	const int c_cashBufferLength = 18; // Space to store "-$999,999,999,999\0"
	char cashRemainingString[c_cashBufferLength];
	const CCriminalCareerService& careerService = SCriminalCareerService::GetInstance();
	const int c_cashRemaining = careerService.GetRemainingCash();
	CFrontendStatsMgr::FormatInt64ToCash(c_cashRemaining, cashRemainingString, c_cashBufferLength);

	// Colour the cash the appropriate colour depending on if it's negative or not
	if (c_cashRemaining < 0)
	{
		cashRemainingColourString.Set("~HUD_COLOUR_RED~");		
	}
	else
	{		
		cashRemainingColourString.Set("~HUD_COLOUR_GREENLIGHT~");		
	}
	cashRemainingColourString.append(cashRemainingString);
	cashRemainingColourString.append("~s~");
	cashBalance.SetLiteral(cashRemainingColourString, true);
}


#endif // UI_PAGE_DECK_ENABLED
