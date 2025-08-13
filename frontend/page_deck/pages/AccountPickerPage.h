/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : AccountPickerPage.h
// PURPOSE : Page underpinning the account picker

// AUTHOR  : james.strain
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef ACCOUNT_PICKER_PAGE_H
#define ACCOUNT_PICKER_PAGE_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/PageBase.h"

class uiPageDeckMessageBase;

class CAccountPickerPage : public CCategorylessPageBase
{
    FWUI_DECLARE_DERIVED_TYPE( CAccountPickerPage, CCategorylessPageBase );
public:
    CAccountPickerPage()
        : superclass()
    {

    }
    virtual ~CAccountPickerPage() { Shutdown(); }

private: // declarations and variables
    PAR_PARSABLE;
    NON_COPYABLE( CAccountPickerPage );

private: // methods
    virtual void OnEnterStartDerived();
    virtual void UpdateDerived( float const deltaMs );

    bool IsTransitoryPageDerived() const final { return true; }
};

#endif // UI_PAGE_DECK_ENABLED

#endif // #ifndef ACCOUNT_PICKER_PAGE_H

