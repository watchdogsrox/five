#include "CriminalCareerShoppingCart.h"

#if GEN9_STANDALONE_ENABLED

CCriminalCareerShoppingCart::CCriminalCareerShoppingCart() :
	m_chosenCareerIdentifier(CriminalCareerDefs::CareerIdentifier_None)
{
}

void CCriminalCareerShoppingCart::SetChosenCareerIdentifier(const CriminalCareerDefs::CareerIdentifier& careerId)
{
	Assertf(m_allItems.empty(), "Changing career selection when shopping cart is not empty. Reset() it first");
	m_chosenCareerIdentifier = careerId;
}

const CCriminalCareerShoppingCart::ItemRefCollection* CCriminalCareerShoppingCart::TryGetItemsWithCategory(const CriminalCareerDefs::ItemCategory& itemCategory) const
{
	const ItemRefCollection* foundItems = m_itemsByCategory.Access(itemCategory);
	return foundItems;
}

bool CCriminalCareerShoppingCart::IsItemInShoppingCart(const CCriminalCareerItem& item) const
{
	return m_allItems.Find(&item) != -1;
}

int CCriminalCareerShoppingCart::GetTotalCost() const
{
	return CalculateSumOfItemCosts(m_allItems);
}

int CCriminalCareerShoppingCart::GetCostForCategory(const CriminalCareerDefs::ItemCategory& itemCategory) const
{
	const ItemRefCollection* cp_items = TryGetItemsWithCategory(itemCategory);
	if (cp_items)
	{
		return CalculateSumOfItemCosts(*cp_items);
	}

	return 0;
}

void CCriminalCareerShoppingCart::AddItem(const CCriminalCareerItem& item)
{
	Assertf(m_allItems.Find(&item) == -1, "Attempting to add %s to Shopping Cart multiple times", item.GetIdentifier().TryGetCStr());
	m_allItems.PushAndGrow(&item);
	ItemRefCollection& categoryItems = m_itemsByCategory[item.GetCategory()];
	categoryItems.PushAndGrow(&item);
}

void CCriminalCareerShoppingCart::RemoveItem(const CCriminalCareerItem& item)
{
	const int c_itemIndex = m_allItems.Find(&item);
	if (Verifyf(c_itemIndex != -1, "Attempting to remove %s from Shopping Cart when it is not present", item.GetIdentifier().TryGetCStr()))
	{
		m_allItems.Delete(c_itemIndex);

		ItemRefCollection& categoryItems = m_itemsByCategory[item.GetCategory()];
		const int c_categoryItemIndex = categoryItems.Find(&item);
		if (Verifyf(c_categoryItemIndex != -1, "Failed to find %s in Category %s lookup", item.GetIdentifier().TryGetCStr(), item.GetCategory().TryGetCStr()))
		{
			categoryItems.Delete(c_categoryItemIndex);
		}
	}
}

void CCriminalCareerShoppingCart::Reset()
{
	m_chosenCareerIdentifier = CriminalCareerDefs::CareerIdentifier_None;
	m_allItems.clear();
	m_itemsByCategory.Reset();
}

int CCriminalCareerShoppingCart::CalculateSumOfItemCosts(const ItemRefCollection& items) const
{
	int runningTotal = 0;

	const int c_itemsCount = items.GetCount();
	for (int i = 0; i < c_itemsCount; ++i)
	{
		runningTotal += items[i]->GetCost();
	}

	return runningTotal;
}

#endif // GEN9_STANDALONE_ENABLED
