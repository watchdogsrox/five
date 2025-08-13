/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CareerBuilderShoppingCartItem.h
// PURPOSE : Represents a Curated Content item in the Career Builder.
//
// AUTHOR  : stephen.phillips
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef CAREER_BUILDER_SHOPPING_CART_ITEM
#define CAREER_BUILDER_SHOPPING_CART_ITEM

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/items/CardItemBase.h"
#include "Peds/CriminalCareer/CriminalCareerDefs.h"

class CFeaturedItem;

class CCareerBuilderShoppingCartItem final : public CCardItemBase, public IItemStatsInterface, public IItemStickerInterface
{
	FWUI_DECLARE_DERIVED_TYPE(CCareerBuilderShoppingCartItem, CCardItemBase);

public:
	struct StatData
	{
	public:
		StatData() { }

		StatData(const CriminalCareerDefs::LocalizedTextKey& labelKey, int value) :
			m_value(value),
			m_labelKey(labelKey)
		{
		}

		const CriminalCareerDefs::LocalizedTextKey& GetLabelKey() const { return m_labelKey; }
		int GetValue() const { return m_value; }

	private:
		int m_value;
		CriminalCareerDefs::LocalizedTextKey m_labelKey;
	};

	typedef atArray<StatData> StatCollection;

	CCareerBuilderShoppingCartItem() :
		superclass()
	{
	}

	virtual ~CCareerBuilderShoppingCartItem() {}

	char const * GetItemTitleText() const final { return m_costStr.c_str(); }	
	char const * GetPageTitleText() const final { return m_titleStr.c_str(); }
	char const * GetDescriptionText() const final { return m_descriptionStr.c_str(); }

	ConstString& Title() { return m_titleStr; }
	ConstString& Description() { return m_descriptionStr; }
	ConstString& Cost() { return m_costStr; }
	const CriminalCareerDefs::ItemIdentifier& GetItemIdentifier() const { return m_itemIdentifier; }
	void AssignItemIdentifier(const CriminalCareerDefs::ItemIdentifier& itemId);

	void SetTextureInformation(const char* const txd, const char* const texture, const CCriminalCareerItem::TexturePathCollection& featuredImagePaths);
	void SetTooltipData(const CriminalCareerDefs::LocalizedTextKey& tooltipAddKey, const CriminalCareerDefs::LocalizedTextKey& tooltipRemoveKey);
	void SetStatsData(const CriminalCareerDefs::VehicleStatsData& vehicleStats);
	void SetStatsData(const CriminalCareerDefs::WeaponStatsData& vehicleStats);

	void AssignStats(CFeaturedItem& featuredItem) const final;

	char const * GetStickerText() const final;
	bool IsInShoppingCart() const;
	IItemStickerInterface::StickerType GetItemStickerType() const;
	IItemStickerInterface::StickerType GetPageStickerType() const;

private: // declarations and variables
	ConstString m_titleStr;
	ConstString m_descriptionStr;
	ConstString m_costStr;
	CriminalCareerDefs::LocalizedTextKey m_tooltipAddKey;
	CriminalCareerDefs::LocalizedTextKey m_tooltipRemoveKey;
	StatCollection m_stats;

	CriminalCareerDefs::ItemIdentifier m_itemIdentifier;
	CCriminalCareerItem::TexturePathCollection m_featuredImagePaths;

	NON_COPYABLE(CCareerBuilderShoppingCartItem);

private:	
	// methods
	void ForEachFeatureTextureDerived(PerTextureLambda action) final;
	char const * GetFeatureTxd(rage::u32 const index) const final;
	char const * GetFeatureTexture(rage::u32 const index) const final;

	IItemStatsInterface* GetStatsInterfaceDerived() final { return this; }
	IItemStickerInterface* GetStickerInterfaceDerived() final { return this; }
	atHashString GetTooltip() const final;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // CAREER_BUILDER_SHOPPING_CART_ITEM
