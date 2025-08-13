/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : BinkPage.h
// PURPOSE : Bespoke page for displaying a bink video.
//
// AUTHOR  : brian.beacom
// STARTED : August 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef BINK_PAGE_H
#define BINK_PAGE_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/PageBase.h"

class CBinkPage : public CCategorylessPageBase
{
    FWUI_DECLARE_DERIVED_TYPE(CBinkPage, CCategorylessPageBase);
public:
    CBinkPage() : superclass() {}
    virtual ~CBinkPage() {}

private: // declarations and variables

    PAR_PARSABLE;
    NON_COPYABLE(CBinkPage);

private: // methods

    bool IsTransitoryPageDerived() const final { return true; }
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // BINK_PAGE_H
