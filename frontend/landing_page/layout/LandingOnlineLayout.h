/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LandingOnlineLayout.h
// PURPOSE : Specialized layout for the online section, as it wants very curated
//           dynamic layout.
//
// AUTHOR  : james.strain
// STARTED : February 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef LANDING_ONLINE_LAYOUT_H
#define LANDING_ONLINE_LAYOUT_H

#include "frontend/landing_page/LandingPageConfig.h"
#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/layout/PageGridSimple.h"

class CLandingOnlineLayout final : public CPageGridSimple
{
    FWUI_DECLARE_DERIVED_TYPE( CLandingOnlineLayout, CPageGridSimple );
public: // declarations and variables
    enum eLayoutType
    {
        LAYOUT_ONE_PRIMARY,
        LAYOUT_TWO_PRIMARY,
        LAYOUT_ONE_PRIMARY_TWO_SUB,
        LAYOUT_ONE_WIDE_TWO_SUB,
        LAYOUT_ONE_WIDE_ONE_SUB_TWO_MINI,
		LAYOUT_ONE_WIDE_TWO_MINI_ONE_SUB,
        LAYOUT_FOUR_SUB,

        LAYOUT_FULLSCREEN,
        LAYOUT_COUNT
    };
public: // methods
    CLandingOnlineLayout()
        : superclass( CPageGridSimple::GRID_7x7_INTERLACED )
        , m_layoutType(LAYOUT_FOUR_SUB)
    {}

    CLandingOnlineLayout(eLayoutType layoutType)
        : superclass(CPageGridSimple::GRID_7x7_INTERLACED)
        , m_layoutType(layoutType)
    {}

    virtual ~CLandingOnlineLayout() {}

    void SetLayoutType( eLayoutType const layoutType ) { m_layoutType = layoutType; }
    eLayoutType GetLayoutType() const { return m_layoutType; }

private: // declarations and variables
    eLayoutType m_layoutType; 
    PAR_PARSABLE;
    NON_COPYABLE( CLandingOnlineLayout );

private: // methods
    void ResetDerived() final;
    int GetDefaultItemFocusIndex() const final;
    char const * const GetSymbolDerived( CPageItemBase const& pageItem ) const final;
    CPageLayoutItemParams GenerateParamsDerived( CPageItemBase const& pageItem ) const final;

    void PlayEnterAnimationDerived() final;
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // LANDING_ONLINE_LAYOUT_H
