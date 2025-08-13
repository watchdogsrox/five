#include "CriminalCareerCatalog.h"
#include "CriminalCareerCatalog_parser.h"

#if GEN9_STANDALONE_ENABLED

const CCriminalCareer* CCriminalCareerCatalog::TryGetCareerWithIdentifier(const CriminalCareerDefs::CareerIdentifier& careerId) const
{
	const int c_careerCount = m_careers.GetCount();
	for (int i = 0; i < c_careerCount; ++i)
	{
		const CCriminalCareer& career = m_careers[i];
		if (career.GetIdentifier() == careerId)
		{
			return &career;
		}
	}

	return nullptr;
}

const CriminalCareerDefs::ItemCategoryDisplayData* CCriminalCareerCatalog::TryGetDisplayDataForCategory(const CriminalCareerDefs::ItemCategory& itemCategory) const
{
	const CriminalCareerDefs::ItemCategoryDisplayData* cp_foundEntry = m_itemCategoryDisplayData.Access(itemCategory);
	return cp_foundEntry;
}

const CCriminalCareerCatalog::ItemRefCollection* CCriminalCareerCatalog::TryGetItemsForCareer(const CriminalCareerDefs::CareerIdentifier& careerId) const
{
	const ItemRefCollection* cp_foundEntry = m_allCareerItems.Access(careerId);
	return cp_foundEntry;
}

const CCriminalCareerCatalog::ItemRefCollection* CCriminalCareerCatalog::TryGetItemsForCareerWithCategory(const CriminalCareerDefs::CareerIdentifier& careerId, const CriminalCareerDefs::ItemCategory& itemCategory) const
{
	CriminalCareerDefs::CareerCategoryPair careerCategoryPair(careerId, itemCategory);
	const ItemRefCollection* cp_foundEntry = m_careerItemsByCategory.Access(careerCategoryPair);
	return cp_foundEntry;
}

const CCriminalCareerItem* CCriminalCareerCatalog::TryGetItemWithIdentifier(const CriminalCareerDefs::ItemIdentifier& itemId) const
{
	const CCriminalCareerItem* cp_foundEntry = m_items.Access(itemId);
	return cp_foundEntry;
}

void CCriminalCareerCatalog::ForEachFeatureOfItem(const CCriminalCareerItem& item, ItemFeatureVisitorLambda action) const
{
	// If this item has a location, present it as the first feature
	const CriminalCareerDefs::LocalizedTextKey& c_locationKey = item.GetLocationKey();
	if (c_locationKey.IsNotNull())
	{
		// Retrieve Location feature icon
		const rage::atHashString c_locationHash = ATSTRINGHASH("Location", 0xFBC129D2);
		const CriminalCareerDefs::ItemFeature* cp_locationFeature = m_itemFeatures.Access(c_locationHash);
		if ( uiVerifyf(cp_locationFeature != nullptr, "Unable to retrieve Location feature icon with key " HASHFMT, HASHOUT(c_locationHash)) )
		{
			const char* const c_locationBlipName = cp_locationFeature->GetBlipName();
			CriminalCareerDefs::ItemFeature locationFeature(c_locationKey, c_locationBlipName);
			action(locationFeature);
		}
	}

	const CCriminalCareerItem::ItemFeatureIdentifierCollection& itemFeatureIds = item.GetFeatureIdentifiers();
	const int c_itemFeatureIdsCount = itemFeatureIds.GetCount();

	for (int i = 0; i < c_itemFeatureIdsCount; ++i)
	{
		const CriminalCareerDefs::ItemFeatureIdentifier& c_featureId = itemFeatureIds[i];
		const CriminalCareerDefs::ItemFeature* p_itemFeature = m_itemFeatures.Access(c_featureId);
		if (p_itemFeature != nullptr)
		{
			action(*p_itemFeature);
		}
	}
}

void CCriminalCareerCatalog::ForEachItemWithFlags(const CriminalCareerDefs::ItemFlagsBitSetData& flags, ItemVisitorLambda action) const
{
	for (ItemMap::ConstIterator it = m_items.CreateIterator(); !it.AtEnd(); it.Next())
	{
		const CCriminalCareerItem& c_item = *it;
		if (c_item.GetFlags().IsSuperSetOf(flags))
		{
			action(c_item);
		}
	}
}

const CriminalCareerDefs::VehicleDisplayLocationIdentifier& CCriminalCareerCatalog::GetDefaultLocationIdentifier() const
{
	return m_vehicleDisplayData.GetDefaultLocationIdentifier();
}

const CriminalCareerDefs::VehicleDisplayLocation* CCriminalCareerCatalog::TryGetDefaultVehicleDisplayLocation() const
{
	const CriminalCareerDefs::VehicleDisplayLocationIdentifier& c_locationIdentifier = m_vehicleDisplayData.GetDefaultLocationIdentifier();
	const CriminalCareerDefs::VehicleDisplayLocation* cp_foundEntry = TryGetVehicleDisplayLocationByIdentifier(c_locationIdentifier);
	return cp_foundEntry;
}

const CriminalCareerDefs::VehicleDisplayLocation* CCriminalCareerCatalog::TryGetVehicleDisplayLocationByIdentifier(const CriminalCareerDefs::VehicleDisplayLocationIdentifier& displayLocationId) const
{
	const CriminalCareerDefs::VehicleDisplayLocation* cp_foundEntry = m_vehicleDisplayData.GetVehicleDisplayLocations().Access(displayLocationId);
	return cp_foundEntry;
}

const CriminalCareerDefs::VehicleSaveSlot* CCriminalCareerCatalog::TryGetVehicleSaveSlotAtIndex(int index) const
{
	const CriminalCareerDefs::VehicleSaveSlot* cp_foundEntry = m_vehicleSaveSlots.Access(index);
	return cp_foundEntry;
}

void CCriminalCareerCatalog::PostLoad()
{
#if __DEV
	const int c_allCareersCount = m_careers.GetCount();
	Assertf(c_allCareersCount > 0, "CCriminalCareerCatalog - No Careers loaded");
	for (int i = 0; i < c_allCareersCount; ++i)
	{
		// Verify Career identifiers exist
		const CriminalCareerDefs::CareerIdentifier& careerId = m_careers[i].GetIdentifier();
		Assertf(careerId, "CCriminalCareerCatalog - Career at array index %d has null identifier", i);

		// Verify Career identifiers are unique
		for (int j = i + 1; j < c_allCareersCount; ++j)
		{
			Assertf(careerId != m_careers[j].GetIdentifier(), "CCriminalCareerCatalog - Duplicate Career Identifier 0x%08x at array indices %d and %d", careerId, i, j);
		}
	}
#endif // __DEV

	// Assign Identifiers from mapped keys
	for (ItemFeatureMap::Iterator it = m_itemFeatures.CreateIterator(); !it.AtEnd(); it.Next())
	{
		const CriminalCareerDefs::ItemFeatureIdentifier& itemId = it.GetKey();
		CriminalCareerDefs::ItemFeature& itemFeature = *it;
		itemFeature.AssignIdentifier(itemId);
	}

	for (ItemMap::Iterator it = m_items.CreateIterator(); !it.AtEnd(); it.Next())
	{
		const CriminalCareerDefs::ItemIdentifier& itemId = it.GetKey();
		CCriminalCareerItem& item = *it;
		item.AssignIdentifier(itemId);

#if __DEV
		// Ensure all Item Features, if any, are valid
		const CCriminalCareerItem::ItemFeatureIdentifierCollection& itemFeatureIds = item.GetFeatureIdentifiers();
		const int c_itemFeatureIdsCount = itemFeatureIds.GetCount();

		for (int i = 0; i < c_itemFeatureIdsCount; ++i)
		{
			const CriminalCareerDefs::ItemFeatureIdentifier& c_featureId = itemFeatureIds[i];
			Assertf(m_itemFeatures.Access(c_featureId) != nullptr, "CCriminalCareerItem %s has unrecognised Feature " HASHFMT, itemId.TryGetCStr(), HASHOUT(c_featureId));
		}
#endif // __DEV

		// If this item is a Windfall Gift, verify it does not have career lookup information
		if (item.GetFlags().IsSet(CriminalCareerDefs::ItemFlags::WindfallGift))
		{
			Assertf(item.GetCareerAvailability().empty() && item.GetCategory().IsNull(), "CCriminalCareerItem %s is marked as a Windfall Gift but has displayable Criminal Career data", itemId.TryGetCStr());
		}
		else
		{
			PopulateCareerLookupForItem(item);
		}
	}

#if __DEV
	for (int i = 0; i < c_allCareersCount; ++i)
	{
		// Verify all Careers have data for all their Item categories
		const CCriminalCareer& career = m_careers[i];
		const CriminalCareerDefs::CareerIdentifier& careerId = career.GetIdentifier();
		const CCriminalCareer::ItemCategoryCollection& categories = career.GetItemCategories();
		const int c_categoryCount = categories.GetCount();
		for (int j = 0; j < c_categoryCount; ++j)
		{
			const CriminalCareerDefs::ItemCategory& category = categories[j];
			CriminalCareerDefs::CareerCategoryPair careerCategoryPair(careerId, category);
			const CriminalCareerDefs::ItemRefCollection* const cp_itemRefCollection = m_careerItemsByCategory.Access(careerCategoryPair);
			Assertf(cp_itemRefCollection != nullptr, "No items exist for Career 0x%08x in Category %s", careerId, category.TryGetCStr());

			// Verify item CarouselIndexOverride values do not conflict. If they do, they will sit beside each other in whatever order they are sorted by hash
			if (cp_itemRefCollection != nullptr)
			{
				const CriminalCareerDefs::ItemRefCollection& c_itemRefCollection = *cp_itemRefCollection;
				const int c_itemRefCollectionCount = c_itemRefCollection.GetCount();
				for (int a = 0; a < c_itemRefCollectionCount; ++a)
				{
					const CCriminalCareerItem* p_lhsItem = c_itemRefCollection[a];
					const int c_lhsIndex = p_lhsItem->GetCarouselIndexOverride();

					Assertf(c_lhsIndex < c_itemRefCollectionCount, "Item %s has CarouselIndexOverride %d in a list of %d items", p_lhsItem->GetIdentifier().TryGetCStr(), c_lhsIndex, c_itemRefCollectionCount);

					if (c_lhsIndex != -1)
					{
						for (int b = a + 1; b < c_itemRefCollectionCount; ++b)
						{
							const CCriminalCareerItem* p_rhsItem = c_itemRefCollection[b];
							const int c_rhsIndex = p_rhsItem->GetCarouselIndexOverride();
							Assertf(c_lhsIndex != c_rhsIndex, "Items %s and %s have conflicting CarouselIndexOverride %d for Career 0x%08x", p_lhsItem->GetIdentifier().TryGetCStr(), p_rhsItem->GetIdentifier().TryGetCStr(), c_lhsIndex, careerId);
						}
					}
				}
			}
		}
	}

	// Verify Vehicle Display and Save slots have been populated correctly
	const CriminalCareerDefs::VehicleDisplayLocationIdentifier& c_defaultLocationId = m_vehicleDisplayData.GetDefaultLocationIdentifier();
	uiAssertf(c_defaultLocationId.IsNotNull(), "Default Vehicle Display Location has not been set");
	uiAssertf(m_vehicleDisplayData.GetVehicleDisplayLocations().Has(c_defaultLocationId), "Default Vehicle Display Location " HASHFMT " does not exist in locations list", HASHOUT(c_defaultLocationId));

	uiAssertf(m_vehicleSaveSlots.GetNumUsed() > 0, "No Vehicle Save Slots have been assigned");
#endif // __DEV
}

void CCriminalCareerCatalog::PopulateCareerLookupForItem(const CCriminalCareerItem& item)
{
	// Build career lookup index
	const CCriminalCareerItem::CareerIdentifierCollection& careerAvailability = item.GetCareerAvailability();
	const int c_careerAvailabilityCount = careerAvailability.GetCount();

	DEV_ONLY(bool bAddedToAnyCareers = false);
	// If there are no careers specified, add it to all careers which own its category
	if (c_careerAvailabilityCount == 0)
	{
		const int c_allCareersCount = m_careers.GetCount();
		for (int i = 0; i < c_allCareersCount; ++i)
		{
			const CCriminalCareer& career = m_careers[i];
			if (career.GetItemCategories().Find(item.GetCategory()) != -1)
			{
				const CriminalCareerDefs::CareerIdentifier& careerIdentifier = career.GetIdentifier();
				AddItemToCareer(item, careerIdentifier);
				DEV_ONLY(bAddedToAnyCareers = true);
			}
		}
	}
	else
	{
		// Only add it to the careers it is explicitly available for
		for (int i = 0; i < c_careerAvailabilityCount; ++i)
		{
			const CriminalCareerDefs::CareerIdentifier& careerId = careerAvailability[i];

#if __DEV
			// Retrieve the career and ensure it is valid
			const CCriminalCareer* p_career = TryGetCareerWithIdentifier(careerId);
			const bool bIsValidCareerId = (p_career != nullptr);
			Assertf(bIsValidCareerId, "CCriminalCareerCatalog - Attempting to add Item %s to 0x%08x which is not a known Career identifier", item.GetIdentifier().TryGetCStr(), careerId);

			if (bIsValidCareerId)
			{
				// Verify this item has a category we expect for this career
				Assertf(p_career->GetItemCategories().Find(item.GetCategory()) != -1, "CCriminalCareerCatalog - Adding Item %s to Career 0x%08x which does not use Category %s", item.GetIdentifier().TryGetCStr(), careerId, item.GetCategory().TryGetCStr());
			}
#endif // __DEV

			AddItemToCareer(item, careerId);
			DEV_ONLY(bAddedToAnyCareers = true);
		}
	}

	DEV_ONLY(Assertf(bAddedToAnyCareers, "CCriminalCareerCatalog - Item " HASHFMT " with Category %s does not match any Careers", HASHOUT(item.GetIdentifier()), item.GetCategory().TryGetCStr()));
}

void CCriminalCareerCatalog::AddItemToCareer(const CCriminalCareerItem& item, const CriminalCareerDefs::CareerIdentifier& careerId)
{
	ItemRefCollection& careerItems = m_allCareerItems[careerId];
	careerItems.PushAndGrow(&item);

	CriminalCareerDefs::CareerCategoryPair careerCategoryPair(careerId, item.GetCategory());
	ItemRefCollection& careerItemsByCategory = m_careerItemsByCategory[careerCategoryPair];
	careerItemsByCategory.PushAndGrow(&item);
}

#endif // GEN9_STANDALONE_ENABLED
