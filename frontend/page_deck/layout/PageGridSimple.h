/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageGridSimple.h
// PURPOSE : Extension of the base grid that exposes some pre-defined grid configs
//           that are conveniently available via an enum.
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef PAGE_GRID_SIMPLE_H
#define PAGE_GRID_SIMPLE_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/layout/PageGrid.h"

class CPageGridSimple : public CPageGrid
{
    typedef CPageGrid superclass;
public: // declarations and variables
    // Enum of pre-defined configs
    // NxM - N Columns, M Rows
    // Interlaced   - Alternate rows/columns are used for auto-item placement
    // Aligned - We actually have n+2 columns, as we pad the edges
    enum eType
    {
        GRID_1x1 = 0,
        GRID_1x1_ALIGNED,
        GRID_3x1_INTERLACED_ALIGNED,
        GRID_3x5,
        GRID_3x6,
		GRID_3x7,
        GRID_3x8,
		GRID_3x13,
        GRID_7x7_INTERLACED,
        GRID_9x1		
    };
    // Making the enum pull double duty as both "layout setup" and "item positioner"
    // is a bit of a janky design, but fits our scope perfectly

public:
    CPageGridSimple()
        : superclass()
        , m_defaultSymbol()
        , m_configuration( GRID_1x1 )
    {}
    CPageGridSimple( eType const configType )
        : superclass()
        , m_defaultSymbol()
        , m_configuration( configType )
    {}
    CPageGridSimple( eType const configType, char const * const defaultSymbol )
        : superclass()
        , m_defaultSymbol( defaultSymbol )
        , m_configuration( configType )
    {}
    virtual ~CPageGridSimple() {}

    void SetLayoutConfig( eType const newConfig );

protected:
    void ResetDerived() override;
    char const * const GetSymbolDerived( CPageItemBase const& pageItem ) const override;
    CPageLayoutItemParams GenerateParamsDerived( CPageItemBase const& pageItem ) const override;

    void SetConfiguration( eType const configuration ) { m_configuration = configuration; }

private: // declarations and variables
    ConstString m_defaultSymbol;
    eType m_configuration;
    PAR_PARSABLE;
    NON_COPYABLE( CPageGridSimple );
};

#endif // UI_PAGE_DECK_ENABLED

#endif // PAGE_GRID_SIMPLE_H
