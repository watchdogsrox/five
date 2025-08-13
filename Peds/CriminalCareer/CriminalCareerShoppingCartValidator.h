/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CriminalCareerShoppingCartValidator.h
// PURPOSE : Compares contents of Criminal Career shopping cart with configured
//               limits, flagging violations to be handled by UI
//
// AUTHOR  : stephen.phillips
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CRIMINALCAREER_CRIMINAL_CAREER_SHOPPING_CART_VALIDATOR
#define CRIMINALCAREER_CRIMINAL_CAREER_SHOPPING_CART_VALIDATOR

// framework
#include "fwutil/Gen9Settings.h"

// rage
#include "atl/vector.h"
#include "parser/macros.h"
#include "system/noncopyable.h"

//game
#include "Peds/CriminalCareer/CriminalCareerDefs.h"
#include "Peds/CriminalCareer/CriminalCareerShoppingCart.h"

#if GEN9_STANDALONE_ENABLED

class CCriminalCareerCatalog;

class CCriminalCareerShoppingCartValidator final
{
public:
	typedef rage::atVector<CriminalCareerDefs::ShoppingCartValidationError> ValidationErrorArray;

	CCriminalCareerShoppingCartValidator() { }

	const CriminalCareerDefs::ShoppingCartItemLimits& GetItemLimitsForCategory(const CriminalCareerDefs::ItemCategory& category) const;
	void ValidateShoppingCart(const CCriminalCareerCatalog& catalog, const CCriminalCareerShoppingCart& shoppingCart, ValidationErrorArray& out_validationErrors) const;
	int GetTotalCashAvailable() const;
	int GetMinimumSpend() const;

private:
	NON_COPYABLE(CCriminalCareerShoppingCartValidator);

	typedef CriminalCareerDefs::ItemCategory ItemCategory;
	typedef CriminalCareerDefs::ItemRefCollection ItemRefCollection;
	typedef CriminalCareerDefs::ShoppingCartItemLimits ShoppingCartItemLimits;
	typedef CriminalCareerDefs::ShoppingCartValidationError ShoppingCartValidationError;
	typedef CCriminalCareerShoppingCartValidationData::ItemCategoryValidationMap ItemCategoryValidationMap;

	bool IsItemEligibleForValidation(const CCriminalCareerItem& item) const;
	int SumItemSpend(const ItemRefCollection& items) const;
	int CountItems(const ItemRefCollection& items) const;
	void ValidateShoppingCartItemLimits(const ItemRefCollection& allItems, const ItemCategory& itemCategory, const ShoppingCartItemLimits& itemLimits, ValidationErrorArray& out_validationErrors) const;

	CCriminalCareerShoppingCartValidationData m_validationData;

	PAR_SIMPLE_PARSABLE;	
};

#endif // GEN9_STANDALONE_ENABLED

#endif // CRIMINALCAREER_CRIMINAL_CAREER_SHOPPING_CART_VALIDATOR
