#include "CriminalCareerShoppingCartValidator.h"
#include "CriminalCareerShoppingCartValidator_parser.h"

#include "CriminalCareerCatalog.h"

#if GEN9_STANDALONE_ENABLED

#if !IS_GEN9_PLATFORM
#define TOTAL_CASH_AVAILABLE 4000000
#define CASH_MINIMUM_SPEND 3000000
#endif

bool CCriminalCareerShoppingCartValidator::IsItemEligibleForValidation(const CCriminalCareerItem& item) const
{
	const bool c_isEligibleForValidation = item.GetFlags().IsClear(CriminalCareerDefs::IgnoreForValidation);
	return c_isEligibleForValidation;
}

int CCriminalCareerShoppingCartValidator::SumItemSpend(const ItemRefCollection& items) const
{
	const int c_totalItemCount = items.GetCount();

	int itemSpend = 0;
	for (int i = 0; i < c_totalItemCount; ++i)
	{
		if (IsItemEligibleForValidation(*items[i]))
		{
			itemSpend += items[i]->GetCost();
		}
	}

	return itemSpend;
}

int CCriminalCareerShoppingCartValidator::CountItems(const ItemRefCollection& items) const
{
	const int c_totalItemCount = items.GetCount();

	int itemCount = 0;
	for (int i = 0; i < c_totalItemCount; ++i)
	{
		if (IsItemEligibleForValidation(*items[i]))
		{
			++itemCount;
		}
	}

	return itemCount;
}

int CCriminalCareerShoppingCartValidator::GetTotalCashAvailable() const
{
#if IS_GEN9_PLATFORM
	return MoneyInterface::GetWindfallAwardAmount();
#else
	return TOTAL_CASH_AVAILABLE;
#endif // IS_GEN9_PLATFORM
}

int CCriminalCareerShoppingCartValidator::GetMinimumSpend() const
{
#if IS_GEN9_PLATFORM
	return MoneyInterface::GetWindfallMinimumSpend();
#else
	return CASH_MINIMUM_SPEND;
#endif // IS_GEN9_PLATFORM
}

const CriminalCareerDefs::ShoppingCartItemLimits& CCriminalCareerShoppingCartValidator::GetItemLimitsForCategory(const CriminalCareerDefs::ItemCategory& category) const
{
	return m_validationData.GetItemLimitsForCategory(category);
}

void CCriminalCareerShoppingCartValidator::ValidateShoppingCartItemLimits(const ItemRefCollection& allItems, const ItemCategory& itemCategory, const ShoppingCartItemLimits& itemLimits, ValidationErrorArray& out_validationErrors) const
{
	// Check for quantity
	const CriminalCareerDefs::ItemLimits& quantityLimits = itemLimits.GetQuantityLimits();
	if (quantityLimits.HasMaximum() || quantityLimits.HasMinimum())
	{
		const int itemsCount = CountItems(allItems);
		if (quantityLimits.HasMaximum() && itemsCount > quantityLimits.GetMaximum())
		{
			out_validationErrors.PushAndGrow(ShoppingCartValidationError(CriminalCareerDefs::ShoppingCartValidationErrorType_QuantityExceeded, itemCategory, quantityLimits));
		}
		if (quantityLimits.HasMinimum() && itemsCount < quantityLimits.GetMinimum())
		{
			out_validationErrors.PushAndGrow(ShoppingCartValidationError(CriminalCareerDefs::ShoppingCartValidationErrorType_QuantityInsufficient, itemCategory, quantityLimits));
		}
	}

	// Find sum by category and check spend
	const CriminalCareerDefs::ItemLimits& spendLimits = itemLimits.GetSpendLimits();
	if (spendLimits.HasMaximum() || spendLimits.HasMinimum())
	{
		const int totalSpend = SumItemSpend(allItems);
		if (spendLimits.HasMaximum() && totalSpend > itemLimits.GetSpendLimits().GetMaximum())
		{
			out_validationErrors.PushAndGrow(ShoppingCartValidationError(CriminalCareerDefs::ShoppingCartValidationErrorType_SpendExceeded, itemCategory, spendLimits));
		}
		if (spendLimits.HasMinimum() && totalSpend < itemLimits.GetSpendLimits().GetMinimum())
		{
			out_validationErrors.PushAndGrow(ShoppingCartValidationError(CriminalCareerDefs::ShoppingCartValidationErrorType_SpendInsufficient, itemCategory, spendLimits));
		}
	}
}

void CCriminalCareerShoppingCartValidator::ValidateShoppingCart(const CCriminalCareerCatalog& catalog, const CCriminalCareerShoppingCart& shoppingCart, ValidationErrorArray& out_validationErrors) const
{
	out_validationErrors.clear();

	// Retrieve career
	const CriminalCareerDefs::CareerIdentifier& chosenCareerId = shoppingCart.GetChosenCareerIdentifier();
	const CCriminalCareer* p_chosenCareer = catalog.TryGetCareerWithIdentifier(chosenCareerId);

	// No need to flag up a Validation error if the career could not be found. The player can't progress to the Shopping Cart UI yet.
	if (p_chosenCareer)
	{
		// Compare all items to total limits
		const ItemRefCollection& allItems = shoppingCart.GetAllItems();

		const int c_minimumSpend = GetMinimumSpend();
		const int c_maximumSpend = GetTotalCashAvailable();
		const ShoppingCartItemLimits totalLimits(c_minimumSpend, c_maximumSpend);
		// Use a null category to denote "all" items
		ValidateShoppingCartItemLimits(allItems, CriminalCareerDefs::ItemCategory(), totalLimits, out_validationErrors);

		// TODO_CRIMINAL_CAREER: Sanity check for duplicates?

		// Check all categories for the chosen career
		const CCriminalCareer::ItemCategoryCollection& careerCategories = p_chosenCareer->GetItemCategories();
		const int c_categoryCount = careerCategories.GetCount();
		const ItemRefCollection emptyCollection;
		for (int i = 0; i < c_categoryCount; ++i)
		{
			const CriminalCareerDefs::ItemCategory& category = careerCategories[i];
			const ShoppingCartItemLimits& itemLimits = m_validationData.GetItemLimitsForCategory(category);
			const ItemRefCollection* p_itemsForCategory = shoppingCart.TryGetItemsWithCategory(category);
			const ItemRefCollection& itemsForCategory = (p_itemsForCategory != nullptr) ? *p_itemsForCategory : emptyCollection;
			ValidateShoppingCartItemLimits(itemsForCategory, category, itemLimits, out_validationErrors);
		}
	}
}

#endif // GEN9_STANDALONE_ENABLED
