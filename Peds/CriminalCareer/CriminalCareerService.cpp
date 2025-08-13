// framework
#include "fwsys/fileExts.h"
#include "fwsys/gameskeleton.h"

// rage
#include "parser/manager.h"

// game
#include "CriminalCareerService.h"
#include "Stats/MoneyInterface.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsUtils.h"

#if GEN9_STANDALONE_ENABLED

#define CATALOG_DATA_PATH "platform://data/criminalcareer/catalog." META_FILE_EXT
#define SHOPPING_CART_VALIDATOR_DATA_PATH "platform://data/criminalcareer/shopping_cart_validation." META_FILE_EXT

CCriminalCareerService::CCriminalCareerService() :
	m_localPlayerPurchasedCESPStatus(PurchaseStatus_None)
{
	ASSERT_ONLY(const bool loadedCatalog = )PARSER.LoadObject(CATALOG_DATA_PATH, nullptr, m_catalog);
	Assertf(loadedCatalog, "Failed to load CCriminalCareerCatalog data from '%s'", CATALOG_DATA_PATH);

	ASSERT_ONLY(const bool loadedValidator = )PARSER.LoadObject(SHOPPING_CART_VALIDATOR_DATA_PATH, nullptr, m_shoppingCartValidator);
	Assertf(loadedValidator, "Failed to load CCriminalCareerShoppingCartValidator data from '%s'", SHOPPING_CART_VALIDATOR_DATA_PATH);
}

void CCriminalCareerService::SetChosenCareerIdentifier(const CriminalCareerDefs::CareerIdentifier& careerId)
{
	// If the player has already selected a career and their shopping cart is not empty, reset it now
	if (HasAChosenCareer() && HasAnyItemsInShoppingCart())
	{
		ResetShoppingCart();
	}

	m_shoppingCart.SetChosenCareerIdentifier(careerId);

	// Search for items that need to be added to the shopping cart automatically for the chosen career
	const CriminalCareerDefs::ItemRefCollection* cp_careerItems = TryGetItemsForChosenCareer();
	if (cp_careerItems != nullptr)
	{
		const CriminalCareerDefs::ItemRefCollection& c_careerItems = *cp_careerItems;
		auto AddItemToCartAction = [this, &c_careerItems](const CCriminalCareerItem& item)
		{
			if (c_careerItems.Find(&item) != -1)
			{
				AddItemToShoppingCart(item);
			}
		};
		ForEachItemWithFlag(CriminalCareerDefs::AlwaysInShoppingCart, AddItemToCartAction);
	}
}

void CCriminalCareerService::AddItemToShoppingCart(const CCriminalCareerItem& item)
{
	// Check if validation states only one item should be selectable
	const CriminalCareerDefs::ItemCategory& c_itemCategory = item.GetCategory();
	const CriminalCareerDefs::ShoppingCartItemLimits& categoryLimits = GetItemLimitsForCategory(c_itemCategory);
	const bool c_onlyOneItemAllowed = categoryLimits.GetQuantityLimits().GetMaximum() == 1;
	const CCriminalCareerShoppingCart::ItemRefCollection* const cp_itemsWithSameCategory = TryGetItemsInShoppingCartWithCategory(c_itemCategory);

	if (c_onlyOneItemAllowed && cp_itemsWithSameCategory != nullptr)
	{
		const CCriminalCareerShoppingCart::ItemRefCollection& c_itemsWithSameCategory = *cp_itemsWithSameCategory;
		const int c_allItemsCount = c_itemsWithSameCategory.GetCount();
		// Remove all matching items from shopping cart
		for (int i = c_allItemsCount - 1; i >= 0; --i)
		{
			const CCriminalCareerItem& c_itemToRemove = *c_itemsWithSameCategory[i];
			if (c_itemToRemove.GetFlags().IsClear(CriminalCareerDefs::AlwaysInShoppingCart))
			{
				RemoveItemFromShoppingCart(c_itemToRemove);
			}
		}
	}

	m_shoppingCart.AddItem(item);
}

void CCriminalCareerService::RemoveItemFromShoppingCart(const CCriminalCareerItem& item)
{
	if (uiVerifyf(item.GetFlags().IsClear(CriminalCareerDefs::AlwaysInShoppingCart), "Attempting to remove CCriminalCareerItem " HASHFMT " but it has AlwaysInShoppingCart flag", HASHOUT(item.GetIdentifier())))
	{
		m_shoppingCart.RemoveItem(item);
	}
}

void CCriminalCareerService::ResetShoppingCart()
{
	m_shoppingCart.Reset();
}

void CCriminalCareerService::PurchaseItemsInShoppingCartWithWindfall(const int charindex)
{
	if (uiVerifyf(CStatsUtils::CanUpdateStats(), "CCriminalCareerService::PurchaseItemsInShoppingCartWithWindfall - Attempting to complete Windfall purchases when Stats are not updatable") && 
		uiVerifyf(DoesShoppingCartPassValidation(), "CCriminalCareerService::PurchaseItemsInShoppingCartWithWindfall() - Attempting to complete Windfall purchases with invalid Shopping Cart state.") &&
		uiVerifyf(!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT), "CCriminalCareerService::PurchaseItemsInShoppingCartWithWindfall() - MP stats are loading or have failed to load!"))
	{
		// Keep counters of vehicles for save and display slots. Assume starting from zero for fresh character
		int personalVehicleCounter = 0;
		rage::atMap<CriminalCareerDefs::VehicleDisplayLocationIdentifier, int> displaySlotCounter;

		// Award Windfall Gifts to the character
		auto GiveWindfallGiftAction = [this, charindex, &personalVehicleCounter, &displaySlotCounter](const CCriminalCareerItem& item)
		{
			AwardItemToCharacter(charindex, item, personalVehicleCounter, displaySlotCounter);
		};
		ForEachItemWithFlag(CriminalCareerDefs::ItemFlags::WindfallGift, GiveWindfallGiftAction);

		// Retrieve list of items to push to player inventory
		const CriminalCareerDefs::ItemRefCollection& allItems = GetAllItemsInShoppingCart();

		const int c_allItemsCount = allItems.GetCount();
		for (int i = 0; i < c_allItemsCount; ++i)
		{
			const CCriminalCareerItem* const cp_item = allItems[i];
			uiFatalAssertf(cp_item != nullptr, "Attempting to purchase null item (index %d of %d)", i, c_allItemsCount);

			AwardItemToCharacter(charindex, *cp_item, personalVehicleCounter, displaySlotCounter);
		}

		// Finally, reset shopping cart
		ResetShoppingCart();
	}

	// TODO_UI CAREER_BUILDER: Remember to shut down SCriminalCareerService when Career Builder flow is complete, it won't be needed again.
}

void CCriminalCareerService::AwardItemToCharacter(int mpCharacterSlot, const CCriminalCareerItem& item, int& ref_vehicleCounter, VehicleDisplaySlotCounter& ref_displaySlotCounter)
{
	const CCriminalCareerItem::ProfileStatCollection& profileStats = item.GetProfileStats();
	for (CCriminalCareerItem::ProfileStatCollection::const_iterator it = profileStats.begin(); it != profileStats.end(); ++it)
	{
		const CriminalCareerDefs::ProfileStatData& c_stat = *it;
		const StatId c_itemStatId = CriminalCareerDefs::BuildStatIdentifierForCharacterSlot(mpCharacterSlot, c_stat.GetIdentifier());

		if (uiVerifyf(StatsInterface::IsKeyValid(c_itemStatId), "PurchaseItemsInShoppingCartWithWindfall : Key %s is not present in the stats", c_itemStatId.GetName()))
		{
			// Set the stat. Completing the Windfall will force a stat update.
			StatsInterface::SetMaskedInt(c_itemStatId, c_stat.GetValue(), c_stat.GetBitshift(), c_stat.GetBitsize());
		}
	}

	int saveSlotIndex = ref_vehicleCounter;
	// Deal with assigning vehicle save stats, if applicable
	const CriminalCareerDefs::VehicleSaveStats& c_vehicleSaveStats = item.GetVehicleSaveStats();
	if (c_vehicleSaveStats.IsValid())
	{
		// Check if this vehicle uses a fixed slot
		const bool c_hasFixedSaveSlot = c_vehicleSaveStats.HasFixedSaveSlotIndex();
		if (c_hasFixedSaveSlot)
		{
			saveSlotIndex = c_vehicleSaveStats.GetSaveSlotIndex();
		}
		else
		{
			// ** Note **
			// We assume here that auto-incrementing won't conflict with any fixed slots.
			// There are currently three fixed slots at 224, 225 and 226
			++ref_vehicleCounter;
		}

		// Retrieve save slot data from Catalog
		const CriminalCareerDefs::VehicleSaveSlot* cp_saveSlot = GetCatalog().TryGetVehicleSaveSlotAtIndex(saveSlotIndex);
		if (uiVerifyf(cp_saveSlot != nullptr, "Failed to retrieve VehicleSaveSlot for index %d", saveSlotIndex))
		{
			const CriminalCareerDefs::VehicleSaveSlot& c_saveSlot = *cp_saveSlot;
			AssignVehicleStatsToSaveSlot(mpCharacterSlot, c_vehicleSaveStats, c_saveSlot);
		}
	}

	// Assign vehicle display slot, if applicable
	const CriminalCareerDefs::VehicleDisplayLocationIdentifier& c_preferredLocationId = item.GetVehicleDisplayLocationPreference();
	if (c_preferredLocationId.IsNotNull())
	{
		rage::atArray<CriminalCareerDefs::VehicleDisplayLocationIdentifier> locations;
		const CriminalCareerDefs::VehicleDisplayLocationIdentifier& c_defaultLocationId = GetCatalog().GetDefaultLocationIdentifier();
		// Try preferred location first, then fall back to default location if no slots are available
		locations.PushAndGrow(c_preferredLocationId);
		if (c_preferredLocationId != c_defaultLocationId)
		{
			locations.PushAndGrow(c_defaultLocationId);
		}

		bool didAssignToDisplaySlot = false;
		const int c_locationsCount = locations.GetCount();
		for (int i = 0; i < c_locationsCount && !didAssignToDisplaySlot; ++i)
		{
			const CriminalCareerDefs::VehicleDisplayLocationIdentifier& c_locationId = locations[i];
			const CriminalCareerDefs::VehicleDisplayLocation* const cp_displayLocation = GetCatalog().TryGetVehicleDisplayLocationByIdentifier(c_locationId);
			if (uiVerifyf(cp_displayLocation != nullptr, "Failed to retrieve VehicleDisplayLocation with ID " HASHFMT, HASHOUT(c_locationId)))
			{
				const CriminalCareerDefs::VehicleDisplayLocation& c_displayLocation = *cp_displayLocation;
				// Check if there are any slots remaining
				int* p_currentCount = ref_displaySlotCounter.Access(c_locationId);
				if (p_currentCount == nullptr)
				{
					const VehicleDisplaySlotCounter::Entry& resultEntry = ref_displaySlotCounter.Insert(c_locationId, 0);
					p_currentCount = const_cast<int*>(&resultEntry.data);
				}
				int& ref_currentCount = *p_currentCount;
				if (ref_currentCount < c_displayLocation.GetCount() - 1)
				{
					const CriminalCareerDefs::VehicleDisplaySlot& c_displaySlot = c_displayLocation[ref_currentCount];
					const StatId c_displaySlotId = CriminalCareerDefs::BuildStatIdentifierForCharacterSlot(mpCharacterSlot, c_displaySlot.GetIdentifier());
					if (uiVerifyf(StatsInterface::IsKeyValid(c_displaySlotId), "PurchaseItemsInShoppingCartWithWindfall : Vehicle Key %s is not present in the stats", c_displaySlotId.GetName()))
					{
						const int c_displaySlotIndex = saveSlotIndex + 1; // Assign as index + 1 because zero is used to denote an empty slot
						StatsInterface::SetMaskedInt(c_displaySlotId, c_displaySlotIndex, c_displaySlot.GetBitshift(), c_displaySlot.GetBitsize());
						++ref_currentCount;
						didAssignToDisplaySlot = true;
					}
				}
			}
		}

		uiAssertf(didAssignToDisplaySlot, "Failed to assign " HASHFMT " to any VehicleDisplayLocation", HASHOUT(item.GetIdentifier()));
	}
}

void CCriminalCareerService::SetCareerWindfallStats(const int charindex)
{
	if (uiVerifyf(CStatsUtils::CanUpdateStats(), "CCriminalCareerService::SetCareerWindfallStats - Attempting to complete Windfall when Stats are not updatable") && 
		uiVerifyf(HasAChosenCareer(), "CCriminalCareerService::SetCareerWindfallStats() - Attempting to complete Windfall with invalid career state.")&&
		uiVerifyf(!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT), "CCriminalCareerService::SetCareerWindfallStats - MP stats are loading or have failed to load!"))
	{
		StatId c_statId;
		switch (charindex)
		{
		case 0:
			c_statId = STAT_MP0_WINDFALL_CHAR_CAREER;
			break;
		case 1:
			c_statId = STAT_MP1_WINDFALL_CHAR_CAREER;
			break;
		default:
			uiAssertf(0, "Failed to assign career as invalid character slot provided");
			break;
		}
		if (c_statId.IsValid() && uiVerifyf(StatsInterface::IsKeyValid(c_statId), "SetCareerWindfallStats career stat is missing"))
		{
			const CCriminalCareer* career = TryGetChosenCareer();
			if (uiVerifyf(career, "SetCareerWindfallStats career ptr is invalid"))
			{
				int id = career->GetIdentifier();
				StatsInterface::SetStatData(c_statId, id);
				uiDisplayf("SetCareerWindfallStats : Set Career : %s : %s : %d", c_statId.GetName(), career->GetDisplayNameKey().GetCStr(), id);
			}
		}
	}
}

bool CCriminalCareerService::HasLocalPlayerPurchasedCESP() const
{
	if (m_localPlayerPurchasedCESPStatus == PurchaseStatus_None)
	{
		// Update cached status for Criminal Enterprise Starter Pack
		const bool hasCESP = StatsInterface::HasLocalPlayerPurchasedCESP();
		m_localPlayerPurchasedCESPStatus = hasCESP ? PurchaseStatus_Purchased : PurchaseStatus_NotPurchased;
	}

	return m_localPlayerPurchasedCESPStatus == PurchaseStatus_Purchased;
}

const CCriminalCareerService::CareerCollection& CCriminalCareerService::GetAllCareers() const
{
	return GetCatalog().GetAllCareers();
}

CriminalCareerDefs::CareerIdentifier CCriminalCareerService::GetChosenCareerIdentifier() const
{
	return GetShoppingCart().GetChosenCareerIdentifier();
}

const CCriminalCareer* CCriminalCareerService::TryGetChosenCareer() const
{
	return GetCatalog().TryGetCareerWithIdentifier(GetChosenCareerIdentifier());
}

bool CCriminalCareerService::HasAChosenCareer() const
{
	return GetShoppingCart().GetChosenCareerIdentifier() != 0;
}

const CriminalCareerDefs::ItemCategoryDisplayData* CCriminalCareerService::TryGetDisplayDataForCategory(const CriminalCareerDefs::ItemCategory& itemCategory) const
{
	return GetCatalog().TryGetDisplayDataForCategory(itemCategory);
}

const CCriminalCareerService::ItemRefCollection* CCriminalCareerService::TryGetItemsForChosenCareer() const
{
	return GetCatalog().TryGetItemsForCareer(GetShoppingCart().GetChosenCareerIdentifier());
}

const CCriminalCareerService::ItemRefCollection* CCriminalCareerService::TryGetItemsForChosenCareerWithCategory(const CriminalCareerDefs::ItemCategory& itemCategory) const
{
	return GetCatalog().TryGetItemsForCareerWithCategory(GetShoppingCart().GetChosenCareerIdentifier(), itemCategory);
}

const CCriminalCareerItem* const CCriminalCareerService::TryGetItemWithIdentifier(const CriminalCareerDefs::ItemIdentifier& itemId) const
{
	return GetCatalog().TryGetItemWithIdentifier(itemId);
}

void CCriminalCareerService::ForEachFeatureOfItem(const CCriminalCareerItem& item, CCriminalCareerCatalog::ItemFeatureVisitorLambda action) const
{
	GetCatalog().ForEachFeatureOfItem(item, action);
}

void CCriminalCareerService::ForEachItemWithFlags(const CriminalCareerDefs::ItemFlagsBitSetData& flags, CCriminalCareerCatalog::ItemVisitorLambda action) const
{
	GetCatalog().ForEachItemWithFlags(flags, action);
}

void CCriminalCareerService::ForEachItemWithFlag(const CriminalCareerDefs::ItemFlags& flag, CCriminalCareerCatalog::ItemVisitorLambda action) const
{
	CriminalCareerDefs::ItemFlagsBitSetData flags;
	flags.Set(flag);
	ForEachItemWithFlags(flags, action);
}

bool CCriminalCareerService::IsFlagSetOnItem(const CriminalCareerDefs::ItemFlags& flag, const CriminalCareerDefs::ItemIdentifier& itemId) const
{
	const CCriminalCareerItem* p_foundItem = TryGetItemWithIdentifier(itemId);
	if (uiVerifyf(p_foundItem != nullptr, "Failed to retrieve CCriminalCareerItem with identifier " HASHFMT, HASHOUT(itemId)))
	{
		return p_foundItem->GetFlags().IsSet(flag);
	}
	return false;
}

bool CCriminalCareerService::DoesItemHaveDisplayStats(const CCriminalCareerItem& item) const
{
	return item.HasAnyDisplayStats();
}

bool CCriminalCareerService::IsItemInShoppingCart(const CriminalCareerDefs::ItemIdentifier& itemId) const
{
	const CCriminalCareerItem* p_foundItem = TryGetItemWithIdentifier(itemId);
	if ( uiVerifyf(p_foundItem != nullptr, "Failed to retrieve CCriminalCareerItem with identifier " HASHFMT, HASHOUT(itemId)) )
	{
		return IsItemInShoppingCart(*p_foundItem);
	}
	return false;
}

bool CCriminalCareerService::IsItemInShoppingCart(const CCriminalCareerItem& item) const
{
	return GetShoppingCart().IsItemInShoppingCart(item);
}

const CriminalCareerDefs::ItemRefCollection& CCriminalCareerService::GetAllItemsInShoppingCart() const
{
	return GetShoppingCart().GetAllItems();
}

const CriminalCareerDefs::ItemRefCollection* CCriminalCareerService::TryGetItemsInShoppingCartWithCategory(const CriminalCareerDefs::ItemCategory& itemCategory) const
{
	return GetShoppingCart().TryGetItemsWithCategory(itemCategory);
}

bool CCriminalCareerService::HasAnyItemsInShoppingCart() const
{
	return GetAllItemsInShoppingCart().empty() == false;
}

int CCriminalCareerService::GetRemainingCash() const
{
	const int c_totalAvailable = GetTotalCashAvailable();
	const int c_totalSpend = GetTotalCostInShoppingCart();
	return c_totalAvailable - c_totalSpend;
}

int CCriminalCareerService::GetTotalCostInShoppingCart() const
{
	return GetShoppingCart().GetTotalCost();
}

int CCriminalCareerService::GetCostInShoppingCartForCategory(const CriminalCareerDefs::ItemCategory& itemCategory) const
{
	return GetShoppingCart().GetCostForCategory(itemCategory);
}

int CCriminalCareerService::GetTotalCashAvailable() const
{
	return GetShoppingCartValidator().GetTotalCashAvailable();
}

const CriminalCareerDefs::ShoppingCartItemLimits& CCriminalCareerService::GetItemLimitsForCategory(const CriminalCareerDefs::ItemCategory& category) const
{
	return GetShoppingCartValidator().GetItemLimitsForCategory(category);
}

void CCriminalCareerService::ValidateShoppingCart(ValidationErrorArray& out_validationErrors) const
{
	GetShoppingCartValidator().ValidateShoppingCart(GetCatalog(), GetShoppingCart(), out_validationErrors);
}

bool CCriminalCareerService::DoesShoppingCartPassValidation()
{	
	// Check to see if there are any errors around the cart 
	ValidationErrorArray validationErrors;
	ValidateShoppingCart(validationErrors);
	const bool result = validationErrors.empty();
	return result;	
}

void CCriminalCareerService::AssignVehicleStatsToSaveSlot(int mpCharacterSlot, const CriminalCareerDefs::VehicleSaveStats& saveStats, const CriminalCareerDefs::VehicleSaveSlot& saveSlot)
{
	AssignValueToProfileStat(mpCharacterSlot, saveStats.GetModelValue(), saveSlot.GetModelDefinition());
	AssignValueToProfileStat(mpCharacterSlot, saveStats.GetColor1Value(), saveSlot.GetColor1Definition());
	AssignValueToProfileStat(mpCharacterSlot, saveStats.GetColor2Value(), saveSlot.GetColor2Definition());
	AssignValueToProfileStat(mpCharacterSlot, saveStats.GetColorExtra1Value(), saveSlot.GetColorExtra1Definition());
	AssignValueToProfileStat(mpCharacterSlot, saveStats.GetColorExtra2Value(), saveSlot.GetColorExtra2Definition());
	AssignValueToProfileStat(mpCharacterSlot, saveStats.GetModSpoilerValue(), saveSlot.GetModSpoilerDefinition());
	AssignValueToProfileStat(mpCharacterSlot, saveStats.GetModChassisValue(), saveSlot.GetModChassisDefinition());
	AssignValueToProfileStat(mpCharacterSlot, saveStats.GetModLiveryValue(), saveSlot.GetModLiveryDefinition());
	AssignValueToProfileStat(mpCharacterSlot, saveStats.GetExtra1Value(), saveSlot.GetExtra1Definition());
	AssignValueToProfileStat(mpCharacterSlot, saveStats.GetExtra2Value(), saveSlot.GetExtra2Definition());
	AssignValueToProfileStat(mpCharacterSlot, saveStats.GetExtra3Value(), saveSlot.GetExtra3Definition());
	AssignValueToProfileStat(mpCharacterSlot, saveStats.GetExtra4Value(), saveSlot.GetExtra4Definition());
	AssignValueToProfileStat(mpCharacterSlot, saveStats.GetExtra5Value(), saveSlot.GetExtra5Definition());
	AssignValueToProfileStat(mpCharacterSlot, 1, saveSlot.GetInsuranceDefinition()); // All vehicles purchased via the Career Builder have insurance
}

void CCriminalCareerService::AssignValueToProfileStat(int mpCharacterSlot, int value, const CriminalCareerDefs::ProfileStatDefinition& profileStat)
{
	if (value != 0)
	{
		const StatId c_statId = CriminalCareerDefs::BuildStatIdentifierForCharacterSlot(mpCharacterSlot, profileStat.GetIdentifier());
		if (uiVerifyf(StatsInterface::IsKeyValid(c_statId), "AssignValueToProfileStat : Key %s is not present in the stats", c_statId.GetName()))
		{
			StatsInterface::SetMaskedInt(c_statId, value, profileStat.GetBitshift(), profileStat.GetBitsize());
		}
	}
}

void CCriminalCareerServiceWrapper::Init(unsigned int initMode)
{
	if (initMode == INIT_CORE)
	{
		SCriminalCareerService::Instantiate();
	}
}

void CCriminalCareerServiceWrapper::Shutdown(unsigned int shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
		SCriminalCareerService::Destroy();
	}
}

#endif // GEN9_STANDALONE_ENABLED
