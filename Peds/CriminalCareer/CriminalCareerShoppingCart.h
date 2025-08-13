/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CriminalCareerShoppingCart.h
// PURPOSE : Data model for tracking Career Builder progress
//
// AUTHOR  : stephen.phillips
// STARTED : April 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CRIMINALCAREER_CRIMINAL_CAREER_SHOPPING_CART
#define CRIMINALCAREER_CRIMINAL_CAREER_SHOPPING_CART

// framework
#include "fwutil/Gen9Settings.h"

// game
#include "Peds/CriminalCareer/CriminalCareerDefs.h"

#if GEN9_STANDALONE_ENABLED

class CCriminalCareerShoppingCart final
{
public:
	typedef CriminalCareerDefs::ItemRefCollection ItemRefCollection;

	CCriminalCareerShoppingCart();

	const CriminalCareerDefs::CareerIdentifier& GetChosenCareerIdentifier() const { return m_chosenCareerIdentifier; }
	void SetChosenCareerIdentifier(const CriminalCareerDefs::CareerIdentifier& careerId);

	const ItemRefCollection& GetAllItems() const { return m_allItems; }
	const ItemRefCollection* TryGetItemsWithCategory(const CriminalCareerDefs::ItemCategory& itemCategory) const;
	bool IsItemInShoppingCart(const CCriminalCareerItem& item) const;

	int GetTotalCost() const;
	int GetCostForCategory(const CriminalCareerDefs::ItemCategory& itemCategory) const;

	void AddItem(const CCriminalCareerItem& item);
	void RemoveItem(const CCriminalCareerItem& item);

	void Reset();

private:
	NON_COPYABLE(CCriminalCareerShoppingCart);

	CriminalCareerDefs::CareerIdentifier m_chosenCareerIdentifier;
	ItemRefCollection m_allItems;
	rage::atMap<CriminalCareerDefs::ItemCategory, ItemRefCollection> m_itemsByCategory;

	int CalculateSumOfItemCosts(const ItemRefCollection& items) const;
};

#endif // GEN9_STANDALONE_ENABLED

#endif // CRIMINALCAREER_CRIMINAL_CAREER_SHOPPING_CART
