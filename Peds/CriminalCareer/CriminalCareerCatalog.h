/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CriminalCareerCatalog.h
// PURPOSE : Store for all careers and items available in Career Builder system
//
// AUTHOR  : stephen.phillips
// STARTED : April 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CRIMINALCAREER_CRIMINAL_CAREER_CATALOG
#define CRIMINALCAREER_CRIMINAL_CAREER_CATALOG

// framework
#include "fwutil/Gen9Settings.h"

// rage
#include "atl/array.h"
#include "atl/hashstring.h"
#include "atl/map.h"
#include "parser/macros.h"
#include "system/lambda.h"
#include "system/noncopyable.h"

// game
#include "Peds/CriminalCareer/CriminalCareerDefs.h"

#if GEN9_STANDALONE_ENABLED

class CCriminalCareerCatalog final
{
public:
	typedef rage::atArray<CCriminalCareer> CareerCollection;
	typedef CriminalCareerDefs::ItemRefCollection ItemRefCollection;
	typedef CriminalCareerDefs::VehicleDisplayData VehicleDisplayData;
	typedef rage::atMap<CriminalCareerDefs::ItemIdentifier, CriminalCareerDefs::ItemCategoryDisplayData> ItemCategoryDisplayDataMap;
	typedef rage::atMap<CriminalCareerDefs::ItemFeatureIdentifier, CriminalCareerDefs::ItemFeature> ItemFeatureMap;
	typedef rage::atMap<CriminalCareerDefs::ItemIdentifier, CCriminalCareerItem> ItemMap;
	typedef rage::atMap<int, CriminalCareerDefs::VehicleSaveSlot> VehicleSaveSlotsCollection;
	typedef LambdaRef<void(const CriminalCareerDefs::ItemFeature&)>	ItemFeatureVisitorLambda;
	typedef LambdaRef<void(const CCriminalCareerItem&)>	ItemVisitorLambda;

	CCriminalCareerCatalog() { }

	const CareerCollection& GetAllCareers() const { return m_careers; }
	const CCriminalCareer* TryGetCareerWithIdentifier(const CriminalCareerDefs::CareerIdentifier& careerId) const;

	const CriminalCareerDefs::ItemCategoryDisplayData* TryGetDisplayDataForCategory(const CriminalCareerDefs::ItemCategory& itemCategory) const;
	const ItemRefCollection* TryGetItemsForCareer(const CriminalCareerDefs::CareerIdentifier& careerId) const;
	const ItemRefCollection* TryGetItemsForCareerWithCategory(const CriminalCareerDefs::CareerIdentifier& careerId, const CriminalCareerDefs::ItemCategory& itemCategory) const;
	const CCriminalCareerItem* TryGetItemWithIdentifier(const CriminalCareerDefs::ItemIdentifier& itemId) const;

	void ForEachFeatureOfItem(const CCriminalCareerItem& item, ItemFeatureVisitorLambda action) const;
	void ForEachItemWithFlags(const CriminalCareerDefs::ItemFlagsBitSetData& flags, ItemVisitorLambda action) const;

	const CriminalCareerDefs::VehicleDisplayLocationIdentifier& GetDefaultLocationIdentifier() const;
	const CriminalCareerDefs::VehicleDisplayLocation* TryGetDefaultVehicleDisplayLocation() const;
	const CriminalCareerDefs::VehicleDisplayLocation* TryGetVehicleDisplayLocationByIdentifier(const CriminalCareerDefs::VehicleDisplayLocationIdentifier& displayLocationId) const;
	const CriminalCareerDefs::VehicleSaveSlot* TryGetVehicleSaveSlotAtIndex(int index) const;

private:
	NON_COPYABLE(CCriminalCareerCatalog);

	void PostLoad(); // Invoked by serializer after data has been populated
	void PopulateCareerLookupForItem(const CCriminalCareerItem& item);
	void AddItemToCareer(const CCriminalCareerItem& item, const CriminalCareerDefs::CareerIdentifier& careerId);

	CareerCollection m_careers;
	ItemMap m_items;
	ItemCategoryDisplayDataMap m_itemCategoryDisplayData;
	ItemFeatureMap m_itemFeatures;
	VehicleDisplayData m_vehicleDisplayData;
	VehicleSaveSlotsCollection m_vehicleSaveSlots;

	rage::atMap<CriminalCareerDefs::CareerIdentifier, ItemRefCollection> m_allCareerItems;
	rage::atMap<CriminalCareerDefs::CareerCategoryPair, ItemRefCollection> m_careerItemsByCategory;

	PAR_SIMPLE_PARSABLE;
};

#endif // GEN9_STANDALONE_ENABLED

#endif // CRIMINALCAREER_CRIMINAL_CAREER_CATALOG
