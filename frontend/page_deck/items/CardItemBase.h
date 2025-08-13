/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CardItemBase.h
// PURPOSE : Base for card items. Common interface and members for txd/texture info
//
// AUTHOR  : james.strain
// STARTED : May 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef CARD_ITEM_BASE_H
#define CARD_ITEM_BASE_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/PageItemBase.h"

class CCardItemBase : public CPageItemBase, public IItemTextureInterface
{
    FWUI_DECLARE_DERIVED_TYPE( CCardItemBase, CPageItemBase );

public: // Declarations and variables

    virtual char const * GetDescriptionText() const = 0;

    char const * GetTxd() const final { return m_txd; }
    char const * GetTexture() const final { return m_texture; }

public:
    CCardItemBase() : superclass(), m_isTextureSet( false ) {}
    virtual ~CCardItemBase() {}

protected: // methods

    ConstString& GetTxdInternal() { return m_txd; }
    ConstString& GetTextureInternal() { return m_texture; }
    ConstString& GetFeatureTxdInternal() { return m_featuredTxd; }
    ConstString& GetFeatureTextureInternal() { return m_featuredTexture; }

    void MarkTextureSet( bool const isSet ) { m_isTextureSet = isSet; }
    bool IsTextureSet() const { return m_isTextureSet; }

    void UpdateVisualsDerived( CComplexObject& displayObject ) override;
    void OnFocusGainedDerived( CComplexObject& displayObject ) override;

private: // declarations and variables
    ConstString m_txd;
    ConstString m_texture;

    // Almost all cases we use card items we also have a "featured" view,
    // so it felt like a base property made sense.
    ConstString	m_featuredTxd;
    ConstString m_featuredTexture;

    bool m_isTextureSet;

    PAR_PARSABLE;
    NON_COPYABLE( CCardItemBase );

private: // methods
    void OnRevealDerived(CComplexObject& displayObject, int const sequenceIndex ) final;
    void OnFocusLostDerived(CComplexObject& displayObject ) final;
    void OnHoverGainedDerived(CComplexObject& displayObject ) final;
    void OnHoverLostDerived(CComplexObject& displayObject ) final;
    void OnSelectedDerived( CComplexObject& displayObject ) final;
    void SetDisplayObjectContentDerived( CComplexObject& displayObject ) final;

    void SetEnabledVisualState(CComplexObject& displayObject);

    virtual void SetupCardVisuals( CComplexObject& displayObject );
    IItemTextureInterface* GetTextureInterfaceDerived() final { return this; }

    void ForEachFeatureTextureDerived(PerTextureLambda action) override;
    char const * GetFeatureTxd(rage::u32 const UNUSED_PARAM(index)) const override { return m_featuredTxd; }
    char const * GetFeatureTexture(rage::u32 const UNUSED_PARAM(index)) const override { return m_featuredTexture; }

    void SetStickerInternal( CComplexObject& displayObject, IItemStickerInterface::StickerType const stickerType, char const * const text );
};

#endif // UI_PAGE_DECK_ENABLED

#endif // CARD_ITEM_H
