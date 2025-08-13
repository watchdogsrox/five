/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CriminalCareerService.h
// PURPOSE : Central hub of Criminal Career functionality.
//
// AUTHOR  : stephen.phillips
// STARTED : April 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CRIMINALCAREER_CRIMINAL_CAREER_SERVICE
#define CRIMINALCAREER_CRIMINAL_CAREER_SERVICE

// framework
#include "fwutil/Gen9Settings.h"

// rage
#include "atl/singleton.h"

// game
#include "Peds/CriminalCareer/CriminalCareerCatalog.h"
#include "Peds/CriminalCareer/CriminalCareerDefs.h"
#include "Peds/CriminalCareer/CriminalCareerShoppingCart.h"
#include "Peds/CriminalCareer/CriminalCareerShoppingCartValidator.h"

#if GEN9_STANDALONE_ENABLED

class CCriminalCareerService
{
public:
	typedef CCriminalCareerCatalog::CareerCollection CareerCollection;
	typedef CCriminalCareerCatalog::ItemRefCollection ItemRefCollection;
	typedef CCriminalCareerShoppingCartValidator::ValidationErrorArray ValidationErrorArray;

	~CCriminalCareerService() { }

	const CCriminalCareerCatalog& GetCatalog() const { return m_catalog; }
	const CCriminalCareerShoppingCart& GetShoppingCart() const { return m_shoppingCart; }
	const CCriminalCareerShoppingCartValidator& GetShoppingCartValidator() const { return m_shoppingCartValidator; }

	// Non-const usage
	void SetChosenCareerIdentifier(const CriminalCareerDefs::CareerIdentifier& careerId);
	void AddItemToShoppingCart(const CCriminalCareerItem& item);
	void RemoveItemFromShoppingCart(const CCriminalCareerItem& item);
	void ResetShoppingCart();

	void PurchaseItemsInShoppingCartWithWindfall(const int charindex);
	void SetCareerWindfallStats(const int charindex);

	// Convenience usage methods
	bool HasLocalPlayerPurchasedCESP() const;

	// Careers
	const CareerCollection& GetAllCareers() const;
	CriminalCareerDefs::CareerIdentifier GetChosenCareerIdentifier() const;
	const CCriminalCareer* TryGetChosenCareer() const;
	bool HasAChosenCareer() const;

	// Item Categories
	const CriminalCareerDefs::ItemCategoryDisplayData* TryGetDisplayDataForCategory(const CriminalCareerDefs::ItemCategory& itemCategory) const;

	// Items
	const ItemRefCollection* TryGetItemsForChosenCareer() const;
	const ItemRefCollection* TryGetItemsForChosenCareerWithCategory(const CriminalCareerDefs::ItemCategory& itemCategory) const;
	const CCriminalCareerItem* const TryGetItemWithIdentifier(const CriminalCareerDefs::ItemIdentifier& itemId) const;
	void ForEachFeatureOfItem(const CCriminalCareerItem& item, CCriminalCareerCatalog::ItemFeatureVisitorLambda action) const;
	void ForEachItemWithFlags(const CriminalCareerDefs::ItemFlagsBitSetData& flags, CCriminalCareerCatalog::ItemVisitorLambda action) const;
	void ForEachItemWithFlag(const CriminalCareerDefs::ItemFlags& flag, CCriminalCareerCatalog::ItemVisitorLambda action) const;
	bool IsFlagSetOnItem(const CriminalCareerDefs::ItemFlags& flag, const CriminalCareerDefs::ItemIdentifier& itemId) const;
	bool DoesItemHaveDisplayStats(const CCriminalCareerItem& item) const;

	// Shopping cart
	bool IsItemInShoppingCart(const CriminalCareerDefs::ItemIdentifier& itemId) const;
	bool IsItemInShoppingCart(const CCriminalCareerItem& item) const;
	const CriminalCareerDefs::ItemRefCollection& GetAllItemsInShoppingCart() const;
	const CriminalCareerDefs::ItemRefCollection* TryGetItemsInShoppingCartWithCategory(const CriminalCareerDefs::ItemCategory& itemCategory) const;
	bool HasAnyItemsInShoppingCart() const;
	int GetRemainingCash() const;
	int GetTotalCostInShoppingCart() const;
	int GetCostInShoppingCartForCategory(const CriminalCareerDefs::ItemCategory& itemCategory) const;
	int GetTotalCashAvailable() const;

	// Shopping cart validation
	const CriminalCareerDefs::ShoppingCartItemLimits& GetItemLimitsForCategory(const CriminalCareerDefs::ItemCategory& category) const;
	void ValidateShoppingCart(ValidationErrorArray& out_validationErrors) const;
	bool DoesShoppingCartPassValidation();

protected:
	CCriminalCareerService();

private:
	typedef rage::atMap<CriminalCareerDefs::VehicleDisplayLocationIdentifier, int> VehicleDisplaySlotCounter;
	enum LocalPlayerPurchasedCESPStatus
	{
		PurchaseStatus_None, // No request has been made
		PurchaseStatus_Purchased,
		PurchaseStatus_NotPurchased
	};

	void AwardItemToCharacter(int mpCharacterSlot, const CCriminalCareerItem& item, int& ref_vehicleCounter, VehicleDisplaySlotCounter& ref_displaySlotCounter);
	void AssignVehicleStatsToSaveSlot(int mpCharacterSlot, const CriminalCareerDefs::VehicleSaveStats& saveStats, const CriminalCareerDefs::VehicleSaveSlot& saveSlot);
	void AssignValueToProfileStat(int mpCharacterSlot, int value, const CriminalCareerDefs::ProfileStatDefinition& profileStat);

private:
	CCriminalCareerCatalog m_catalog;
	CCriminalCareerShoppingCart m_shoppingCart;
	CCriminalCareerShoppingCartValidator m_shoppingCartValidator;

	mutable LocalPlayerPurchasedCESPStatus m_localPlayerPurchasedCESPStatus;
};

typedef atSingleton<CCriminalCareerService> SCriminalCareerService;

// Wrapper class needed to interface with game skeleton code
class CCriminalCareerServiceWrapper
{
public:
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
};

#endif // GEN9_STANDALONE_ENABLED

#endif // CRIMINALCAREER_CRIMINAL_CAREER_SERVICE
