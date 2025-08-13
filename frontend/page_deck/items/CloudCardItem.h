/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CloudCardItem.h
// PURPOSE : Represents a card in the page_deck filled with data downloaded
//           from our servers
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef CLOUD_CARD_ITEM_H
#define CLOUD_CARD_ITEM_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/items/CardItemBase.h"
#include "network/SocialClubServices/SocialClubFeed.h"

class CCloudCardItem : public CCardItemBase, public IItemStickerInterface
{
    FWUI_DECLARE_DERIVED_TYPE(CCloudCardItem, CCardItemBase);

public: // Declarations and variables
    char const * GetItemTitleText() const final;
    char const * GetPageTitleText() const final;
    char const * GetTooltipText() const final;
    char const * GetDescriptionText() const final;
    bool RequestFeatureTexture(rage::u32 const index) final;

    RosFeedGuid GetGuid() const { return m_guid; }

    ConstString& Title() { return m_titleStr; }
    ConstString& TitleBig() { return m_titleBigStr; }
    ConstString& Description() { return m_descriptionStr; }
    ConstString& Tooltip() { return m_tooltipStr; }
    ConstString& StickerText() { return m_stickerTextStr; }
    IItemStickerInterface::StickerType& StickerType() { return m_stickerType; }
	ConstString& Txd() { return GetTxdInternal(); }
	ConstString& Texture() { return GetTextureInternal(); }
	ConstString& FeaturedTxd() { return GetFeatureTxdInternal(); }
	ConstString& FeaturedTexture() { return GetFeatureTextureInternal(); }

    void SetGuid(const RosFeedGuid guid);
    void SetTrackingData(int trackingBit, const atHashString id);

    char const * GetStickerText() const final { return m_stickerTextStr.c_str(); }
    IItemStickerInterface::StickerType GetItemStickerType() const { return m_stickerType; }
	IItemStickerInterface::StickerType GetPageStickerType() const { return m_stickerType; }
public:
    CCloudCardItem() : superclass(), m_stickerType( IItemStickerInterface::StickerType::None ), m_trackingBit(-1) {}
    virtual ~CCloudCardItem() {}

protected:

    virtual void OnFocusGainedDerived(CComplexObject& displayObject);
private: // declarations and variables
    RosFeedGuid                         m_guid;

    ConstString                         m_titleStr;
    ConstString                         m_titleBigStr;
    ConstString                         m_descriptionStr;
    ConstString                         m_tooltipStr;
    ConstString                         m_stickerTextStr;
    IItemStickerInterface::StickerType  m_stickerType;
    int                                 m_trackingBit;

    NON_COPYABLE(CCloudCardItem);

private:
    IItemStickerInterface* GetStickerInterfaceDerived() final { return this; }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // CLOUD_CARD_ITEM_H
