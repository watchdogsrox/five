/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ParallaxCardItem.h
// PURPOSE : Card specialization for parallax visuals
//
// AUTHOR  : james.strain
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef PARALLAX_CARD_ITEM_H
#define PARALLAX_CARD_ITEM_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// rage
#include "system/lambda.h"

// game
#include "frontend/page_deck/items/MultiCardItem.h"

class CParallaxCardItem : public CCardItem
{
    FWUI_DECLARE_DERIVED_TYPE(CParallaxCardItem, CCardItem);

public: // Declarations and variables

    // This is public for the sake of parGen, but don't be a dick
    // and use it elsewhere. If you need it, move it to a new location independent of this class
    struct ParallaxTextureInfo final
    {
        ConstString m_backgroundTexture;
        ConstString m_foregroundTexture;
        int         m_parallaxType;

        ParallaxTextureInfo()
            : m_parallaxType(0)
        {

        }

        ParallaxTextureInfo( char const * const backgroundTexture, char const * const foregroundTexture, int const parallaxType )
            : m_backgroundTexture( backgroundTexture )
            , m_foregroundTexture( foregroundTexture )
            , m_parallaxType( parallaxType )
        {

        }

        PAR_SIMPLE_PARSABLE;
    };


public:
	CParallaxCardItem() : superclass(), m_detailsSet(false) {}
    virtual ~CParallaxCardItem() {}

    char const * GetSymbol() const final { return "gridItem3x4Parallax"; }

protected:
    void UpdateVisualsDerived( CComplexObject& displayObject ) override;
    void SetupCardDetails( CComplexObject& displayObject );

    void MarkDetailsSet( bool const detailsSet ) { m_detailsSet = detailsSet; }
    bool AreDetailsSet() const { return m_detailsSet; }

private: // declarations and variables
    typedef LambdaRef< void( u32 const index, char const * const backgroundTexture, char const * const foregroundTexture, int const parallaxType )> ParallaxInfoLambda;

    rage::atArray<ParallaxTextureInfo> m_textureInfoCollection;
    bool m_detailsSet;
    PAR_PARSABLE;
    NON_COPYABLE(CParallaxCardItem);

private: // methods:
    void SetupCardVisuals( CComplexObject& displayObject ) final;
    void SetupCardTextures( CComplexObject& displayObject );

    void ForEachTextureInfo(ParallaxInfoLambda action);
    int const GetTexturePairCount() const;
    ParallaxTextureInfo const * GetTextureInfo( int const index ) const;
};

#endif // UI_PAGE_DECK_ENABLED

#endif // PARALLAX_CARD_ITEM_H
