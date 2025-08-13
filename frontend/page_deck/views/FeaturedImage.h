/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : FeaturedImage.h
// PURPOSE : Quick wrapper for the common featured item interface
//
// AUTHOR  : james.strain
// STARTED : March 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_FEATURED_IMAGE_H
#define UI_FEATURED_IMAGE_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"
#include "frontend/page_deck/views/PipCounter.h"

class CPageItemBase;
class IItemTextureInterface;

class CFeaturedImage final : public CPageLayoutItem
{
    typedef CPageLayoutItem superclass;

public:
    CFeaturedImage( bool const wrapping = false )
        : m_textureInterface( nullptr )
        , m_currentImageIdx ( INDEX_NONE )
        , m_wrapping( wrapping )
    {}
    ~CFeaturedImage() {}

    void Set( CPageItemBase const * const pageItem );
    void Reset();

    void UpdateInstructionalButtons() const;
    void UpdateVisuals();
    bool UpdateInput();

private: // declarations and variables
    struct FeatureTextureInfo final
    {
        ConstString m_txd;
        ConstString m_texture;
        bool        m_usable;
        bool        m_displayed;

        FeatureTextureInfo()
            : m_usable( false )
            , m_displayed(false)
        {

        }

        FeatureTextureInfo( char const * const txd, char const * const texture, bool const usable )
            : m_txd( txd )
            , m_texture( texture )
            , m_usable( usable )
            , m_displayed( false )
        {

        }

        bool UpdateUsableState();
    };

    CPipCounter m_pipCounter;
    atArray<FeatureTextureInfo> m_featureTextureCollection;
    IItemTextureInterface * m_textureInterface;
    int m_currentImageIdx;
    bool m_wrapping;

    NON_COPYABLE( CFeaturedImage );

private: // methods
    bool IsWrapping() const { return m_wrapping; }
    int GetFeatureTextureCount() const { return m_featureTextureCollection.GetCount(); }
    bool ValidateTextureIndex( int const idx ) const;

    void Add( char const * const txd, char const * const texture, bool const usable );

    void ApplyCurrentImage();
    void ApplyImage( int const idx );
    char const * GetSymbol() const override { return "mainImage"; }

    void InitializeDerived() final;
    void ShutdownDerived() final;

    void ResolveLayoutDerived( Vec2f_In screenspacePosPx, Vec2f_In localPosPx, Vec2f_In sizePx ) final;

    // Matches hardcoded value in Scaleform assets
    float GetImageSize() const { return 390.f; }
    float GetPipPadding() const { return 16.f; }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_FEATURED_IMAGE_H
