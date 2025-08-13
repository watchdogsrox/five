/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SummaryPageLayout.cpp
// PURPOSE : Collection of items that form the summary Page in the windfall shop
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : May 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "SummaryPageLayout.h"

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/FrontendStatsMgr.h"
#include "frontend/page_deck/PageItemCategoryBase.h"
#include "frontend/page_deck/layout/PageLayoutBase.h"
#include "frontend/page_deck/layout/PageLayoutDecoratorItem.h"
#include "frontend/VideoEditor/ui/InstructionalButtons.h"
#include "frontend/ui_channel.h"

CSummaryPageLayout::CSummaryPageLayout(CComplexObject& root)
	: superclass()
	, m_topLevelLayout(CPageGridSimple::GRID_3x13)
	, m_backgroundLayout(CPageGridSimple::GRID_1x1)
	, m_itemQuantityExceeded(false)
	, m_itemQuantityInsufficient(false)
	, m_totalSpendExceeded(false)
	, m_totalSpendInsufficient(false)

{
	Initialize(root);
}

CSummaryPageLayout::~CSummaryPageLayout()
{
	ResetItems();
}

void CSummaryPageLayout::RefreshContent()
{
	ResetItems();
	CComplexObject& pageRoot = GetDisplayObject();

	// Build page items from chosen career's item categories
	const CCriminalCareerService& careerService = SCriminalCareerService::GetInstance();
	const CCriminalCareer* p_chosenCareer = careerService.TryGetChosenCareer();
	
	CCriminalCareerService::ValidationErrorArray validationErrors;
	careerService.ValidateShoppingCart(validationErrors);

	if (uiVerifyf(p_chosenCareer, "Failed to retrieve chosen career"))
	{
		m_parallaxImage.Initialize(pageRoot);		
		CPageLayoutItemParams const c_parallaxImageParams(1, 2, 0, 0);
		m_backgroundLayout.RegisterItem(m_parallaxImage, c_parallaxImageParams);
		m_parallaxImage.StartNewParallax("FRONTEND_LANDING_BASE", p_chosenCareer->GetBackgroundParallaxTextureName(), "FRONTEND_LANDING_BASE", p_chosenCareer->GetForegroundParallaxTextureName(), eMotionType::SUMMARY_MOTION_TYPE);

		const CCriminalCareer::ItemCategoryCollection& itemCategoryCollection = p_chosenCareer->GetItemCategories();
		const int c_itemCategoryCollectionCount = itemCategoryCollection.GetCount();

		for (unsigned i = 0; i < c_itemCategoryCollectionCount; ++i)
		{
			const CriminalCareerDefs::ItemCategory& c_entry = itemCategoryCollection[i];
			AddSummaryItem(c_entry, validationErrors);

			// Find overall itemQuantityExceeded/itemQuantityInsufficient errors
			for (int i = 0; i < validationErrors.GetCount(); ++i)
			{
				//  Only look at relevant errors in this category
				if (validationErrors[i].GetCategory() == c_entry)
				{
					m_itemQuantityExceeded = m_itemQuantityExceeded || validationErrors[i].GetType() == CriminalCareerDefs::ShoppingCartValidationErrorType_QuantityExceeded;
					m_itemQuantityInsufficient = m_itemQuantityInsufficient || validationErrors[i].GetType() == CriminalCareerDefs::ShoppingCartValidationErrorType_QuantityInsufficient;
					break;
				}
			}
		}
	}

	m_giftCash.Initialize(pageRoot);
	CPageLayoutItemParams const c_cashTotalParams(1, 6, 1, 1);
	m_topLevelLayout.RegisterItem(m_giftCash, c_cashTotalParams);

	m_totalSpent.Initialize(pageRoot);
	CPageLayoutItemParams const c_cashTotal2Params(1, 7, 1, 1);
	m_topLevelLayout.RegisterItem(m_totalSpent, c_cashTotal2Params);

	m_cashRemaining.Initialize(pageRoot);
	CPageLayoutItemParams const c_cashTotal3Params(1, 8, 1, 1);
	m_topLevelLayout.RegisterItem(m_cashRemaining, c_cashTotal3Params);

	m_Button.Initialize(pageRoot);
	CPageLayoutItemParams const c_buttonParams(1, 10, 1, 1);
	m_topLevelLayout.RegisterItem(m_Button, c_buttonParams);

	m_Button.Set("UI_CB_SUMMARY_START");
	
	const int c_validationErrorsCount = validationErrors.GetCount();
	for (int i = 0; i < c_validationErrorsCount; ++i)
	{
		// A null category means this is either an overspend or underspend error
		if (validationErrors[i].GetCategory() == CriminalCareerDefs::ItemCategory())
		{
			m_totalSpendExceeded = validationErrors[i].GetType() == CriminalCareerDefs::ShoppingCartValidationErrorType_SpendExceeded;
			m_totalSpendInsufficient = validationErrors[i].GetType() == CriminalCareerDefs::ShoppingCartValidationErrorType_SpendInsufficient;
			break;
		}
	}
	const rage::s64 c_totalCashAvailable = careerService.GetTotalCashAvailable();
	PopulateCashTotal(m_giftCash, TheText.Get("UI_CB_SUMMARY_GIFT"), c_totalCashAvailable, false);

	const int c_totalSpend = careerService.GetTotalCostInShoppingCart();
	PopulateCashTotal(m_totalSpent, TheText.Get("UI_CB_SUMMARY_SPENT"), c_totalSpend, m_totalSpendInsufficient);

	const rage::s64 c_cashRemaining = careerService.GetRemainingCash();
	PopulateCashTotal(m_cashRemaining, TheText.Get("UI_CB_SUMMARY_REMAINING"), c_cashRemaining, m_totalSpendExceeded);
	
	RecalculateGrids();
}

void CSummaryPageLayout::ResetItems()
{
	m_backgroundLayout.UnregisterAll();
	m_topLevelLayout.UnregisterAll();
	m_Button.Shutdown();
	m_giftCash.Shutdown();
	m_totalSpent.Shutdown();
	m_cashRemaining.Shutdown();
	m_parallaxImage.Shutdown();

	int c_itemCount = m_items.GetCount();
	for (int i = 0; i < c_itemCount; ++i)
	{
		CSummaryLayoutItem * const currentItem = m_items[i];
		currentItem->Shutdown();
		delete currentItem;
	}
	m_items.ResetCount();

	m_itemQuantityExceeded = false;
	m_itemQuantityInsufficient = false;
	m_totalSpendExceeded = false;
	m_totalSpendInsufficient = false;
}

void CSummaryPageLayout::RecalculateGrids()
{
	// Order is important here, as some of the sizing cascades
	// So if you adjust the order TEST IT!
	m_topLevelLayout.Reset();

	// Page area is acceptable as our grids are all using star sizing
	Vec2f const c_PageExtents = uiPageConfig::GetScreenArea();
	m_topLevelLayout.RecalculateLayout(Vec2f(Vec2f::ZERO), c_PageExtents);
	m_topLevelLayout.NotifyOfLayoutUpdate();
}

void CSummaryPageLayout::ResolveLayoutDerived(Vec2f_In PagespacePosPx, Vec2f_In localPosPx, Vec2f_In sizePx)
{
	CComplexObject& displayObject = GetDisplayObject();
	displayObject.SetPositionInPixels(localPosPx);
	m_topLevelLayout.RecalculateLayout(PagespacePosPx, sizePx);
	m_topLevelLayout.NotifyOfLayoutUpdate();
}

// This currently can only take maximum 4 summary items. If more are added you must change/expand the CPageGridSimple this class uses
void CSummaryPageLayout::AddSummaryItem(const CriminalCareerDefs::ItemCategory& itemCategory, const CCriminalCareerService::ValidationErrorArray& validationErrors)
{
	const CCriminalCareerService& careerService = SCriminalCareerService::GetInstance();
	const CriminalCareerDefs::ItemRefCollection* cp_items = careerService.TryGetItemsInShoppingCartWithCategory(itemCategory);
	const int c_itemCount = (cp_items != nullptr) ? cp_items->GetCount() : 0;

	const int c_displayLength = 128;
	char titleLiteral[c_displayLength] = "";

	// Check if any item is missing a selection
	bool summaryItemQuantityExceeded = false;
	bool summaryItemQuantityInsufficient = false;
	const int c_validationErrorsCount = validationErrors.GetCount();
	for (int i = 0; i < c_validationErrorsCount; ++i)
	{
		//  Only look at relevant errors in this category
		if (validationErrors[i].GetCategory() == itemCategory)
		{
			summaryItemQuantityExceeded = validationErrors[i].GetType() == CriminalCareerDefs::ShoppingCartValidationErrorType_QuantityExceeded;
			summaryItemQuantityInsufficient = validationErrors[i].GetType() == CriminalCareerDefs::ShoppingCartValidationErrorType_QuantityInsufficient;
			break;
		}
	}

	// Build description text based on selections
	char descriptionLiteral[c_displayLength] = "";
	const CriminalCareerDefs::ItemCategoryDisplayData* cp_categoryDisplayData = careerService.TryGetDisplayDataForCategory(itemCategory);
	if (uiVerifyf(cp_categoryDisplayData != nullptr, "Failed to retrieve display data for " HASHFMT, HASHOUT(itemCategory)))
	{
		const CriminalCareerDefs::ItemCategoryDisplayData& c_categoryDisplayData = *cp_categoryDisplayData;
		// Populate category title
		const CriminalCareerDefs::LocalizedTextKey& c_titleKey = c_categoryDisplayData.GetDisplayNameKey();
		safecpy(titleLiteral, TheText.Get(c_titleKey), c_displayLength);

		// Populate category description
		if (summaryItemQuantityExceeded)
		{		
			formatf(descriptionLiteral, "~BLIP_WARNING~ ~HUD_COLOUR_RED~%s~s~", TheText.Get("UI_CB_SUMMARY_EXCEEDED"));
		}
		else if (summaryItemQuantityInsufficient)
		{			
			formatf(descriptionLiteral, "~BLIP_WARNING~ ~HUD_COLOUR_RED~%s~s~", TheText.Get("UI_CB_SUMMARY_MISSING"));
		}
		else if (c_itemCount == 1)
		{
			// Single item selections display just the item's title
			const CriminalCareerDefs::ItemRefCollection& c_items = *cp_items;			
			formatf(descriptionLiteral, "%s", TheText.Get(c_items[0]->GetTitleKey()));
		}
		else
		{
			// Multiple item selections display as "X Items"
			const CriminalCareerDefs::LocalizedTextKey& c_displayNameKey = c_categoryDisplayData.GetDisplayNameKey();
			formatf(descriptionLiteral, "%d %s", c_itemCount, TheText.Get(c_displayNameKey));
		}
	}
	const int c_totalSpend = careerService.GetCostInShoppingCartForCategory(itemCategory);
	
	const int c_costBufferLength = 64; 
	char costString[c_costBufferLength];
	char cashRemainingColourString[c_costBufferLength];
	CFrontendStatsMgr::FormatInt64ToCash(c_totalSpend, costString, c_costBufferLength);
	formatf(cashRemainingColourString, c_costBufferLength, "~HUD_COLOUR_GREENLIGHT~%s~s~", costString);

	CSummaryLayoutItem* const summaryItem = rage_new CSummaryLayoutItem();
	summaryItem->Initialize(GetDisplayObject());
	CPageLayoutItemParams const c_summaryItemParams(1, m_items.GetCount() + 1, 1, 1);
	m_topLevelLayout.RegisterItem(*summaryItem, c_summaryItemParams);
	m_items.PushAndGrow(summaryItem);
	summaryItem->SetLiteral(titleLiteral, descriptionLiteral, cashRemainingColourString, true);
}

void CSummaryPageLayout::UpdateInstructionalButtons(atHashString const acceptLabelOverride) const
{
	char const * const c_label = uiPageConfig::GetLabel(ATSTRINGHASH("IB_SELECT", 0xD7ED7F0C), acceptLabelOverride);
	CVideoEditorInstructionalButtons::AddButton(INPUT_FRONTEND_ACCEPT, c_label, true);
	m_topLevelLayout.UpdateInstructionalButtons(acceptLabelOverride);
}

void CSummaryPageLayout::GetTooltipMessage(ConstString& tooltip) const
{
	if (m_itemQuantityExceeded || m_itemQuantityInsufficient || m_totalSpendExceeded || m_totalSpendInsufficient)
	{
		const int c_displayLength = 128;
		char tooltipLiteral[c_displayLength] = "";
		if (m_itemQuantityExceeded)
		{
			formatf(tooltipLiteral, c_displayLength, "~BLIP_WARNING~ %s~s~", TheText.Get("UI_CB_SUMMARY_EXCEEDED"));
		}
		else if (m_itemQuantityInsufficient)
		{
			formatf(tooltipLiteral, c_displayLength, "~BLIP_WARNING~ %s~s~", TheText.Get("UI_CB_SUMMARY_MISSING_TIP"));
		}
		else if (m_totalSpendExceeded)
		{
			formatf(tooltipLiteral, c_displayLength, "~BLIP_WARNING~ %s~s~", TheText.Get("UI_CB_ERROR_OVERSPEND_TIP"));
		}
		else if (m_totalSpendInsufficient)
		{
			formatf(tooltipLiteral, c_displayLength, "~BLIP_WARNING~ %s~s~", TheText.Get("UI_CB_ERROR_UNDERSPEND_TIP"));
		}
		
		tooltip = tooltipLiteral;
	}
	else
	{
		tooltip = TheText.Get("UI_CB_CASH_TIP");
	}
}

void CSummaryPageLayout::PopulateCashTotal(CSummaryCashTotals& entry, const char* const label, rage::s64 value, bool isError)
{
	const int c_costBufferLength = 64;
	char cashRemainingColourString[c_costBufferLength];	
	char totalSpendString[c_costBufferLength];
	CFrontendStatsMgr::FormatInt64ToCash(value, totalSpendString, c_costBufferLength);
	if (isError)
	{
		formatf(cashRemainingColourString, c_costBufferLength, "~BLIP_WARNING~ ~HUD_COLOUR_RED~%s~s~", totalSpendString);
	}
	else
	{
		formatf(cashRemainingColourString, c_costBufferLength, "~HUD_COLOUR_GREENLIGHT~%s~s~", totalSpendString);
	}

	entry.SetLiteral(label, cashRemainingColourString, true);
}

#endif // UI_PAGE_DECK_ENABLED
