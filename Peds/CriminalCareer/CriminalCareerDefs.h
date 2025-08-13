/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CriminalCareerDefs.h
// PURPOSE : Collection of definitions for structs used by Criminal Career system
//
// AUTHOR  : stephen.phillips
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CRIMINALCAREER_CRIMINAL_CAREER_DEFS
#define CRIMINALCAREER_CRIMINAL_CAREER_DEFS

// framework
#include "fwutil/Gen9Settings.h"

// rage
#include "atl/array.h"
#include "atl/binmap.h"
#include "atl/bitset.h"
#include "atl/hashstring.h"
#include "atl/map.h"
#include "atl/string.h"
#include "parser/macros.h"
#include "Stats/StatsTypes.h"
#include "system/noncopyable.h"

#if GEN9_STANDALONE_ENABLED

class CCriminalCareerItem;

namespace CriminalCareerDefs
{
	// Set up some names to make usage less confusing
	typedef rage::atHashString ItemFeatureIdentifier;
	typedef rage::atHashString ItemCategory;
	typedef rage::atHashString ItemIdentifier;
	typedef rage::atHashString LocalizedTextKey;
	typedef rage::ConstString ProfileStatIdentifier;
	typedef rage::atHashString TelemetryKey;
	typedef rage::atHashString VehicleDisplayLocationIdentifier;
	typedef rage::ConstString TexturePath;

	enum CareerIdentifier
	{
		CareerIdentifier_None					= 0,
		CareerIdentifier_ExecutiveTrafficker	= 0x6ed98b47,	// executive_trafficker
		CareerIdentifier_Gunrunner				= 0xe82a596d,	// gunrunner
		CareerIdentifier_NightclubTrafficker	= 0xb98f1405,	// nightclub_trafficker
		CareerIdentifier_Biker					= 0xe4a72c17	// biker
	};

	struct CareerCategoryPair
	{
	public:
		CareerCategoryPair() { }
		CareerCategoryPair(const CareerIdentifier& careerId, const ItemCategory& itemCategory) :
			m_careerIdentifier(careerId),
			m_itemCategory(itemCategory)
		{
		}

		bool operator <(const CareerCategoryPair& other) const
		{
			return (m_careerIdentifier < other.m_careerIdentifier)
				|| (m_careerIdentifier == other.m_careerIdentifier
					&& m_itemCategory < other.m_itemCategory);
		}

		bool operator ==(const CareerCategoryPair& other) const
		{
			return m_careerIdentifier == other.m_careerIdentifier
				&& m_itemCategory == other.m_itemCategory;
		}

	private:
		friend u32 atHash(const CareerCategoryPair pair)
		{
			return pair.m_careerIdentifier ^ pair.m_itemCategory;
		}

		CareerIdentifier m_careerIdentifier;
		ItemCategory m_itemCategory;
	};

#define ItemLimits_UnsetValue -1 // constexpr int ItemLimits_UnsetValue = -1;

	struct ItemLimits
	{
	public:
		ItemLimits() :
			m_maximum(ItemLimits_UnsetValue),
			m_minimum(ItemLimits_UnsetValue)
		{
		}

		ItemLimits(int minimum, int maximum) :
			m_minimum(minimum),
			m_maximum(maximum)
		{
		}

		bool HasMaximum() const { return m_maximum != ItemLimits_UnsetValue; }
		bool HasMinimum() const { return m_minimum != ItemLimits_UnsetValue; }
		int GetMaximum() const { return m_maximum; }
		int GetMinimum() const { return m_minimum; }

	private:
		int m_maximum;
		int m_minimum;

		PAR_SIMPLE_PARSABLE;
	};

#undef ItemLimits_UnsetValue

	struct ItemFeature
	{
	public:
		ItemFeature() { }
		ItemFeature(const LocalizedTextKey& displayNameKey, const char* blipName) :
			m_displayNameKey(displayNameKey),
			m_blipName(blipName)
		{
		}

		const ItemFeatureIdentifier& GetIdentifier() const { return m_identifier; }
		const LocalizedTextKey& GetDisplayNameKey() const { return m_displayNameKey; }
		const char* GetBlipName() const { return m_blipName.c_str(); }
		void AssignIdentifier(const ItemFeatureIdentifier& identifier); // Invoked by CCriminalCareerCatalog::PostLoad

	private:
		ItemFeatureIdentifier m_identifier;
		LocalizedTextKey m_displayNameKey;
		ConstString m_blipName;

		PAR_SIMPLE_PARSABLE;
	};

	struct ItemCategoryDisplayData
	{
	public:
		ItemCategoryDisplayData() { }

		const LocalizedTextKey& GetDisplayNameKey() const { return m_displayNameKey; }
		const LocalizedTextKey& GetTooltipAddKey() const;
		const LocalizedTextKey& GetTooltipRemoveKey() const { return m_tooltipRemoveKey; }

	private:
		LocalizedTextKey m_displayNameKey;
		LocalizedTextKey m_tooltipAddKey;
		LocalizedTextKey m_tooltipCESPAddKey;
		LocalizedTextKey m_tooltipRemoveKey;

		PAR_SIMPLE_PARSABLE;
	};

	struct ProfileStatData
	{
	public:
		ProfileStatData() { }

		const ProfileStatIdentifier& GetIdentifier() const { return m_identifier; }
		int GetValue() const { return m_value; }
		int GetBitshift() const { return m_bitshift; }
		int GetBitsize() const { return m_bitsize; }

	private:
		void PostLoad(); // Invoked by serializer after data has been populated

		ProfileStatIdentifier m_identifier;
		atString m_valueFormula;
		int m_value;
		int m_bitshift;
		int m_bitsize;

		PAR_SIMPLE_PARSABLE;
	};

	struct ProfileStatDefinition
	{
	public:
		ProfileStatDefinition() { }

		const ProfileStatIdentifier& GetIdentifier() const { return m_identifier; }
		int GetBitshift() const { return m_bitshift; }
		int GetBitsize() const { return m_bitsize; }

	private:
		ProfileStatIdentifier m_identifier;
		int m_bitshift;
		int m_bitsize;
		// ProfileStatDefinitions do not hold a prepopulated <Value> - this is populated with the saved vehicle's settings

		PAR_SIMPLE_PARSABLE;
	};

	struct VehicleDisplaySlot
	{
	public:
		VehicleDisplaySlot() { }

		const ProfileStatIdentifier GetIdentifier() const { return m_identifier; }
		int GetBitshift() const { return m_bitshift; }

#define VehicleDisplaySlot_Bitsize 8 // constexpr int VehicleDisplaySlot_Bitsize = 8;
		int GetBitsize() const { return VehicleDisplaySlot_Bitsize; }
#undef VehicleDisplaySlot_Bitsize

	private:
		ProfileStatIdentifier m_identifier;
		int m_bitshift;
		// Display Slots do not hold a prepopulated <Value> - this is assigned as the index of the vehicle's Save Slot, starting at 1

		PAR_SIMPLE_PARSABLE;
	};

	typedef rage::atArray<VehicleDisplaySlot> VehicleDisplayLocation;
	typedef rage::atBinaryMap<VehicleDisplayLocation, VehicleDisplayLocationIdentifier> VehicleDisplayLocationsMap;

	struct VehicleDisplayData
	{
	public:
		VehicleDisplayData() { }

		const VehicleDisplayLocationIdentifier& GetDefaultLocationIdentifier() const { return m_defaultLocationIdentifier; }
		const VehicleDisplayLocationsMap& GetVehicleDisplayLocations() const { return m_vehicleDisplayLocations; }

	private:
		VehicleDisplayLocationIdentifier m_defaultLocationIdentifier;
		VehicleDisplayLocationsMap m_vehicleDisplayLocations;
		PAR_SIMPLE_PARSABLE;
	};

	struct VehicleSaveSlot
	{
	public:
		VehicleSaveSlot() { }

		const ProfileStatDefinition& GetModelDefinition() const { return m_model; }
		const ProfileStatDefinition& GetColor1Definition() const { return m_color1; }
		const ProfileStatDefinition& GetColor2Definition() const { return m_color2; }
		const ProfileStatDefinition& GetColorExtra1Definition() const { return m_colorExtra1; }
		const ProfileStatDefinition& GetColorExtra2Definition() const { return m_colorExtra2; }
		const ProfileStatDefinition& GetColor5Definition() const { return m_color5; }
		const ProfileStatDefinition& GetColor6Definition() const { return m_color6; }
		const ProfileStatDefinition& GetModSpoilerDefinition() const { return m_modSpoiler; }
		const ProfileStatDefinition& GetModChassisDefinition() const { return m_modChassis; }
		const ProfileStatDefinition& GetModLiveryDefinition() const { return m_modLivery; }
		const ProfileStatDefinition& GetExtra1Definition() const { return m_extra1; }
		const ProfileStatDefinition& GetExtra2Definition() const { return m_extra2; }
		const ProfileStatDefinition& GetExtra3Definition() const { return m_extra3; }
		const ProfileStatDefinition& GetExtra4Definition() const { return m_extra4; }
		const ProfileStatDefinition& GetExtra5Definition() const { return m_extra5; }
		const ProfileStatDefinition& GetInsuranceDefinition() const { return m_insurance; }

	private:
		ProfileStatDefinition m_model;
		ProfileStatDefinition m_color1;
		ProfileStatDefinition m_color2;
		ProfileStatDefinition m_colorExtra1;
		ProfileStatDefinition m_colorExtra2;
		ProfileStatDefinition m_color5;
		ProfileStatDefinition m_color6;
		ProfileStatDefinition m_modSpoiler;
		ProfileStatDefinition m_modChassis;
		ProfileStatDefinition m_modLivery;
		ProfileStatDefinition m_extra1;
		ProfileStatDefinition m_extra2;
		ProfileStatDefinition m_extra3;
		ProfileStatDefinition m_extra4;
		ProfileStatDefinition m_extra5;
		ProfileStatDefinition m_insurance;

		PAR_SIMPLE_PARSABLE;
	};

#define ProfileStats_UnsetValue 0 // constexpr int ProfileStats_UnsetValue = 0;

	struct VehicleSaveStats
	{
	public:
		VehicleSaveStats() :
			m_saveSlotIndex(-1),
			m_model(ProfileStats_UnsetValue),
			m_color1(ProfileStats_UnsetValue),
			m_color2(ProfileStats_UnsetValue),
			m_colorExtra1(ProfileStats_UnsetValue),
			m_colorExtra2(ProfileStats_UnsetValue),
			m_color5(ProfileStats_UnsetValue),
			m_color6(ProfileStats_UnsetValue),
			m_modSpoiler(ProfileStats_UnsetValue),
			m_modChassis(ProfileStats_UnsetValue),
			m_modLivery(ProfileStats_UnsetValue),
			m_extra1(ProfileStats_UnsetValue),
			m_extra2(ProfileStats_UnsetValue),
			m_extra3(ProfileStats_UnsetValue),
			m_extra4(ProfileStats_UnsetValue),
			m_extra5(ProfileStats_UnsetValue)
		{
		}

		int GetSaveSlotIndex() const { return m_saveSlotIndex; }
		int GetModelValue() const { return m_model; }
		int GetColor1Value() const { return m_color1; }
		int GetColor2Value() const { return m_color2; }
		int GetColorExtra1Value() const { return m_colorExtra1; }
		int GetColorExtra2Value() const { return m_colorExtra2; }
		int GetColor5Value() const { return m_color5; }
		int GetColor6Value() const { return m_color6; }
		int GetModSpoilerValue() const { return m_modSpoiler; }
		int GetModChassisValue() const { return m_modChassis; }
		int GetModLiveryValue() const { return m_modLivery; }
		int GetExtra1Value() const { return m_extra1; }
		int GetExtra2Value() const { return m_extra2; }
		int GetExtra3Value() const { return m_extra3; }
		int GetExtra4Value() const { return m_extra4; }
		int GetExtra5Value() const { return m_extra5; }

		bool IsValid() const
		{
			return m_model != ProfileStats_UnsetValue;
		}

		bool HasFixedSaveSlotIndex() const { return m_saveSlotIndex != -1; }

	private:
		int m_saveSlotIndex;
		int m_model;
		int m_color1;
		int m_color2;
		int m_colorExtra1;
		int m_colorExtra2;
		int m_color5;
		int m_color6;
		int m_modSpoiler;
		int m_modChassis;
		int m_modLivery;
		int m_extra1;
		int m_extra2;
		int m_extra3;
		int m_extra4;
		int m_extra5;
		// No need to store Insurance value - assume all vehicles purchased via Career Builder are insured

		PAR_SIMPLE_PARSABLE;
	};
#undef ProfileStats_UnsetValue

#define DisplayStats_UnsetValue -1 // constexpr int Stats_UnsetValue = -1;

	struct VehicleStatsData
	{
	public:
		VehicleStatsData() :
			m_topSpeed(DisplayStats_UnsetValue),
			m_acceleration(DisplayStats_UnsetValue),
			m_braking(DisplayStats_UnsetValue),
			m_traction(DisplayStats_UnsetValue)
		{
		}

		int GetTopSpeed() const { return m_topSpeed; }
		int GetAcceleration() const { return m_acceleration; }
		int GetBraking() const { return m_braking; }
		int GetTraction() const { return m_traction; }

		bool HasAnyValues() const
		{
			return m_topSpeed != DisplayStats_UnsetValue
				|| m_acceleration != DisplayStats_UnsetValue
				|| m_braking != DisplayStats_UnsetValue
				|| m_traction != DisplayStats_UnsetValue;
		}

	private:
		int m_topSpeed;
		int m_acceleration;
		int m_braking;
		int m_traction;

		PAR_SIMPLE_PARSABLE;
	};

	struct WeaponStatsData
	{
	public:
		WeaponStatsData() :
			m_damage(DisplayStats_UnsetValue),
			m_fireRate(DisplayStats_UnsetValue),
			m_accuracy(DisplayStats_UnsetValue),
			m_range(DisplayStats_UnsetValue),
			m_clipSize(DisplayStats_UnsetValue)
		{
		}

		int GetDamage() const { return m_damage; }
		int GetFireRate() const { return m_fireRate; }
		int GetAccuracy() const { return m_accuracy; }
		int GetRange() const { return m_range; }
		int GetClipSize() const { return m_clipSize; }

		bool HasAnyValues() const
		{
			return m_damage != DisplayStats_UnsetValue
				|| m_fireRate != DisplayStats_UnsetValue
				|| m_accuracy != DisplayStats_UnsetValue
				|| m_range != DisplayStats_UnsetValue
				|| m_clipSize != DisplayStats_UnsetValue;
		}

	private:
		int m_damage;
		int m_fireRate;
		int m_accuracy;
		int m_range;
		int m_clipSize;

		PAR_SIMPLE_PARSABLE;
	};

#undef DisplayStats_UnsetValue

	struct ShoppingCartItemLimits
	{
	public:
		ShoppingCartItemLimits() { }
		ShoppingCartItemLimits(int minimumSpend, int maximumSpend) :
			m_spendLimits(minimumSpend, maximumSpend)
		{
		}

		const ItemCategory& GetItemCategory() const { return m_category; }
		const ItemLimits& GetQuantityLimits() const { return m_quantityLimits; }
		const ItemLimits& GetSpendLimits() const { return m_spendLimits; }	
		void AssignCategory(const ItemCategory& category); // Invoked by CCriminalCareerShoppingCartValidator::PostLoad

	private:
		ItemCategory m_category; // Populated by AssignCategory
		ItemLimits m_quantityLimits;
		ItemLimits m_spendLimits;

		PAR_SIMPLE_PARSABLE;
	};

	enum ItemFlags
	{
		AlwaysInShoppingCart,
		IgnoreForValidation,
		WindfallGift,

		CriminalCareerItemFlags_MAX_VALUE
	};
	typedef rage::atFixedBitSet<CriminalCareerItemFlags_MAX_VALUE> ItemFlagsBitSetData;

	enum ShoppingCartValidationErrorType
	{
		ShoppingCartValidationErrorType_Unknown = -1,
		ShoppingCartValidationErrorType_QuantityInsufficient,
		ShoppingCartValidationErrorType_QuantityExceeded,
		ShoppingCartValidationErrorType_SpendInsufficient,
		ShoppingCartValidationErrorType_SpendExceeded
	};

	struct ShoppingCartValidationError
	{
	public:
		ShoppingCartValidationError() :
			m_type(ShoppingCartValidationErrorType_Unknown),
			mp_limits(nullptr)
		{
		}

		ShoppingCartValidationError(ShoppingCartValidationErrorType type, const ItemCategory& category, const ItemLimits& limits) :
			m_type(type),
			m_category(category),
			mp_limits(&limits)
		{
		}

		ShoppingCartValidationErrorType GetType() const { return m_type; }
		const ItemCategory& GetCategory() const { return m_category; }
		const ItemLimits* GetLimits() const { return mp_limits; }

	private:
		ShoppingCartValidationErrorType m_type;
		ItemCategory m_category;
		const ItemLimits* mp_limits;
	};
	
	typedef rage::atArray<ItemIdentifier> ItemIdCollection;
	typedef rage::atArray<const CCriminalCareerItem*> ItemRefCollection;

	StatId BuildStatIdentifierForCharacterSlot(int characterSlot, const ProfileStatIdentifier& profileStatId);
} // namespace CriminalCareerDefs

class CCriminalCareerItem final
{
public:
	typedef rage::atArray<CriminalCareerDefs::CareerIdentifier> CareerIdentifierCollection;
	typedef rage::atArray<CriminalCareerDefs::ItemFeatureIdentifier> ItemFeatureIdentifierCollection;
	typedef rage::atArray<CriminalCareerDefs::ProfileStatData> ProfileStatCollection;
	typedef rage::atArray<CriminalCareerDefs::TexturePath> TexturePathCollection;

	CCriminalCareerItem() :
		m_cost(0)
	{
	}

	const CriminalCareerDefs::ItemIdentifier& GetIdentifier() const { return m_identifier; }
	const char* GetTextureDictionaryName() const { return m_textureDictionaryName.c_str(); }	
	const CriminalCareerDefs::TelemetryKey& GetTelemetryKey() const { return m_telemetryKey; }
	const ProfileStatCollection& GetProfileStats() const { return m_profileStatsData; }
	const CriminalCareerDefs::VehicleSaveStats& GetVehicleSaveStats() const { return m_vehicleSaveStats; }
	const CriminalCareerDefs::VehicleDisplayLocationIdentifier& GetVehicleDisplayLocationPreference() const { return m_vehicleDisplayLocationPreference; }
	const CriminalCareerDefs::ItemCategory& GetCategory() const { return m_category; }
	int GetCost() const;
	int GetDefaultCost() const { return m_cost; }

	const CareerIdentifierCollection& GetCareerAvailability() const { return m_careerAvailability; }
	const CriminalCareerDefs::ItemFlagsBitSetData& GetFlags() const { return m_flags; }

	const CriminalCareerDefs::LocalizedTextKey& GetTitleKey() const { return m_titleKey; }
	const ItemFeatureIdentifierCollection& GetFeatureIdentifiers() const { return m_featureIdentifiers; }
	const CriminalCareerDefs::LocalizedTextKey& GetLocationKey() const { return m_locationKey; }
	const CriminalCareerDefs::LocalizedTextKey& GetDescriptionKey() const { return m_descriptionKey; }
	const CriminalCareerDefs::TexturePath& GetThumbnailPath() const { return m_thumbnailPath; }
	const TexturePathCollection& GetBackgroundImagePaths() const { return m_backgroundImagePaths; }
	const CriminalCareerDefs::VehicleStatsData& GetVehicleStats() const { return m_vehicleStatsData; }
	const CriminalCareerDefs::WeaponStatsData& GetWeaponStats() const { return m_weaponStatsData; }
	int GetCarouselIndexOverride() const { return m_carouselIndexOverride; }

	bool HasAnyDisplayStats() const;
	void AssignIdentifier(const CriminalCareerDefs::ItemIdentifier& identifier); // Invoked by CCriminalCareerCatalog::PostLoad

private:
	NON_COPYABLE(CCriminalCareerItem);

	CriminalCareerDefs::ItemIdentifier m_identifier;
	ConstString m_textureDictionaryName;	
	ConstString m_backgroundParallaxTexture;
	ConstString m_foregroundParallaxTexture;	
	CriminalCareerDefs::TelemetryKey m_telemetryKey;
	ProfileStatCollection m_profileStatsData;
	CriminalCareerDefs::VehicleSaveStats m_vehicleSaveStats;
	CriminalCareerDefs::VehicleDisplayLocationIdentifier m_vehicleDisplayLocationPreference;
	CriminalCareerDefs::ItemCategory m_category;
	int m_cost;
	int m_costCESP;
	CriminalCareerDefs::ItemFlagsBitSetData m_flags;
	CareerIdentifierCollection m_careerAvailability;

	// Display data
	CriminalCareerDefs::LocalizedTextKey m_titleKey;
	ItemFeatureIdentifierCollection m_featureIdentifiers;
	CriminalCareerDefs::LocalizedTextKey m_locationKey;
	CriminalCareerDefs::LocalizedTextKey m_descriptionKey;
	CriminalCareerDefs::TexturePath m_thumbnailPath;
	TexturePathCollection m_backgroundImagePaths;
	CriminalCareerDefs::VehicleStatsData m_vehicleStatsData;
	CriminalCareerDefs::WeaponStatsData m_weaponStatsData;
	int m_carouselIndexOverride;

	PAR_SIMPLE_PARSABLE;
};

class CCriminalCareer final
{
public:
	typedef rage::atArray<CriminalCareerDefs::ItemCategory> ItemCategoryCollection;

	CCriminalCareer() { }

	const CriminalCareerDefs::CareerIdentifier& GetIdentifier() const { return m_identifier; }
	const CriminalCareerDefs::LocalizedTextKey& GetDisplayNameKey() const { return m_displayNameKey; }
	const CriminalCareerDefs::LocalizedTextKey& GetDescriptionKey() const { return m_descriptionKey; }
	const CriminalCareerDefs::LocalizedTextKey& GetTooltipSelectKey() const { return m_tooltipSelectKey; }
	const ConstString& GetInteractiveMusicTrigger() const { return m_interactiveMusicTrigger;  }
	const char* GetTextureDictionaryName() const { return m_textureDictionaryName.c_str(); }
	const char* GetBackgroundParallaxTextureName() const { return m_backgroundParallaxTexture.c_str(); }
	const char* GetForegroundParallaxTextureName() const { return m_foregroundParallaxTexture.c_str(); }
	
	const CriminalCareerDefs::TexturePath& GetPreviewImagePath() const { return m_previewImagePath; }
	const ItemCategoryCollection& GetItemCategories() const { return m_itemCategories; }

private:
	NON_COPYABLE(CCriminalCareer);

	CriminalCareerDefs::CareerIdentifier m_identifier;
	CriminalCareerDefs::LocalizedTextKey m_displayNameKey;
	CriminalCareerDefs::LocalizedTextKey m_descriptionKey;
	ConstString m_textureDictionaryName;
	ConstString m_backgroundParallaxTexture;
	ConstString m_foregroundParallaxTexture;
	ConstString m_interactiveMusicTrigger;
	CriminalCareerDefs::TexturePath m_previewImagePath;
	CriminalCareerDefs::LocalizedTextKey m_tooltipSelectKey;
	ItemCategoryCollection m_itemCategories;

	PAR_SIMPLE_PARSABLE;
};

class CCriminalCareerShoppingCartValidationData final
{
public:
	typedef rage::atMap<CriminalCareerDefs::ItemCategory, CriminalCareerDefs::ShoppingCartItemLimits> ItemCategoryValidationMap;

	CCriminalCareerShoppingCartValidationData() { }

	const CriminalCareerDefs::ShoppingCartItemLimits& GetTotalLimits() const { return m_totalLimits; }
	const CriminalCareerDefs::ShoppingCartItemLimits& GetItemLimitsForCategory(const CriminalCareerDefs::ItemCategory& category) const;

private:
	NON_COPYABLE(CCriminalCareerShoppingCartValidationData);

	void PostLoad(); // Invoked by serializer after data has been populated

	CriminalCareerDefs::ShoppingCartItemLimits m_totalLimits;
	ItemCategoryValidationMap m_itemCategoryLimits;

	// Returned when no limits are specified for the requested category (both HasMaximum() and HasMinimum() return false)
	CriminalCareerDefs::ShoppingCartItemLimits m_noLimits;

	PAR_SIMPLE_PARSABLE;
};

#endif // GEN9_STANDALONE_ENABLED

#endif // CRIMINALCAREER_CRIMINAL_CAREER_DEFS
