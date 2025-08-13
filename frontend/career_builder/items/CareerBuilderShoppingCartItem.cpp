/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CareerBuilderShoppingCartItem.cpp
// PURPOSE : Represents a Curated Content item in the Career Builder.
//
// AUTHOR  : stephen.phillips
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "CareerBuilderShoppingCartItem.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/career_builder/messages/uiCareerBuilderMessages.h"
#include "frontend/page_deck/views/FeaturedItem.h"
#include "frontend/ui_channel.h"

FWUI_DEFINE_TYPE(CCareerBuilderShoppingCartItem, 0xABB436DA);

void CCareerBuilderShoppingCartItem::AssignItemIdentifier(const CriminalCareerDefs::ItemIdentifier& itemId)
{
	m_itemIdentifier = itemId;
}

void CCareerBuilderShoppingCartItem::SetTextureInformation(const char* const txd, const char* const texture, const CCriminalCareerItem::TexturePathCollection& featuredImagePaths)
{
	GetTxdInternal() = txd;
	GetTextureInternal() = texture;
	m_featuredImagePaths = featuredImagePaths;
}

void CCareerBuilderShoppingCartItem::SetTooltipData(const CriminalCareerDefs::LocalizedTextKey& tooltipAddKey, const CriminalCareerDefs::LocalizedTextKey& tooltipRemoveKey)
{
	m_tooltipAddKey = tooltipAddKey;
	m_tooltipRemoveKey = tooltipRemoveKey;
}

void CCareerBuilderShoppingCartItem::SetStatsData(const CriminalCareerDefs::VehicleStatsData& vehicleStats)
{
	m_stats.clear();
	m_stats.Reserve(5);
	m_stats.Push(StatData("RH_Speed", vehicleStats.GetTopSpeed()));
	m_stats.Push(StatData("RH_Accel", vehicleStats.GetAcceleration()));
	m_stats.Push(StatData("RH_Brake", vehicleStats.GetBraking()));
	m_stats.Push(StatData("RH_Handle", vehicleStats.GetTraction()));
	m_stats.Push(StatData("", -1));
}

void CCareerBuilderShoppingCartItem::SetStatsData(const CriminalCareerDefs::WeaponStatsData& weaponStats)
{
	m_stats.clear();
	m_stats.Reserve(5);
	m_stats.Push(StatData("PM_DAMAGE", weaponStats.GetDamage()));
	m_stats.Push(StatData("PM_FIRERATE", weaponStats.GetFireRate()));
	m_stats.Push(StatData("PM_ACCURACY", weaponStats.GetAccuracy()));
	m_stats.Push(StatData("PM_RANGE", weaponStats.GetRange()));
	m_stats.Push(StatData("PM_CLIPSIZE", weaponStats.GetClipSize()));
}

void CCareerBuilderShoppingCartItem::AssignStats(CFeaturedItem& featuredItem) const
{
	if (m_stats.empty())
	{
		featuredItem.ClearStats();
	}
	else
	{
		featuredItem.SetStats(m_stats[0].GetLabelKey(), m_stats[0].GetValue(),
			m_stats[1].GetLabelKey(), m_stats[1].GetValue(),
			m_stats[2].GetLabelKey(), m_stats[2].GetValue(),
			m_stats[3].GetLabelKey(), m_stats[3].GetValue(),
			m_stats[4].GetLabelKey(), m_stats[4].GetValue());
	}
}

char const * CCareerBuilderShoppingCartItem::GetStickerText() const
{
	return TheText.Get("UI_CB_CART_ADDED_TIP");
}

bool CCareerBuilderShoppingCartItem::IsInShoppingCart() const
{
	const CCriminalCareerService& c_careerService = SCriminalCareerService::GetInstance();
	const bool c_isInShoppingCart = c_careerService.IsItemInShoppingCart(GetItemIdentifier());
	return c_isInShoppingCart;
}

IItemStickerInterface::StickerType CCareerBuilderShoppingCartItem::GetItemStickerType() const
{	
	const IItemStickerInterface::StickerType c_stickerType = IsInShoppingCart() ? IItemStickerInterface::StickerType::Tick : IItemStickerInterface::StickerType::None;
	return c_stickerType;
}

IItemStickerInterface::StickerType CCareerBuilderShoppingCartItem::GetPageStickerType() const
{	
	const IItemStickerInterface::StickerType c_stickerType = IsInShoppingCart() ? IItemStickerInterface::StickerType::TickText : IItemStickerInterface::StickerType::None;
	return c_stickerType;
}

void CCareerBuilderShoppingCartItem::ForEachFeatureTextureDerived(PerTextureLambda action)
{
	const int c_imagesCount = m_featuredImagePaths.GetCount();
	for (int i = 0; i < c_imagesCount; ++i)
	{
		action(i, GetTxd(), m_featuredImagePaths[i].c_str());
	}
}

char const * CCareerBuilderShoppingCartItem::GetFeatureTxd(rage::u32 const UNUSED_PARAM( index )) const
{
	return GetTxd();
}

char const * CCareerBuilderShoppingCartItem::GetFeatureTexture(rage::u32 const index) const
{
	if (uiVerifyf(index < m_featuredImagePaths.GetCount(), "Attempting to access invalid array index %d", index))
	{
		return m_featuredImagePaths[index].c_str();
	}
	return "";
}

atHashString CCareerBuilderShoppingCartItem::GetTooltip() const
{
	const CCriminalCareerService& c_criminalCareerService = SCriminalCareerService::GetInstance();
	const bool c_isAlwaysInShoppingCart = c_criminalCareerService.IsFlagSetOnItem(CriminalCareerDefs::AlwaysInShoppingCart, m_itemIdentifier);
	if (c_isAlwaysInShoppingCart)
	{
		// Display "default" tooltip as this item cannot be removed from the shopping cart
		return m_tooltipAddKey;
	}
	else
	{
		const bool c_isInShoppingCart = c_criminalCareerService.IsItemInShoppingCart(m_itemIdentifier);
		const CriminalCareerDefs::LocalizedTextKey& c_displayString = c_isInShoppingCart ? m_tooltipRemoveKey : m_tooltipAddKey;
		return c_displayString;
	}
}

#endif // UI_PAGE_DECK_ENABLED
