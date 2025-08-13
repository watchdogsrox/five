/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : PageGridComplex.h
// PURPOSE : Extension of the base grid that exposes full row/column
//           configuration to PSC. Should only be used for highly bespoke cases
//
// AUTHOR  : james.strain
// STARTED : January 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef PAGE_GRID_COMPLEX_H
#define PAGE_GRID_COMPLEX_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/layout/PageGrid.h"

class CPageGridComplex : public CPageGrid
{
    typedef CPageGrid superclass;

public:
    CPageGridComplex() 
        : superclass()
    {}
    virtual ~CPageGridComplex() {}

private: // declarations and variables
    PAR_PARSABLE;
    NON_COPYABLE( CPageGridComplex );

    void ResetDerived() final
    { 
        // NOP! A complex grid is pre-defined via parGen so we don't want to reset that and lose the state
    }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // PAGE_GRID_COMPLEX_H
