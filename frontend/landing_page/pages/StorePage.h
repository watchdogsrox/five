/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : StorePage.h
// PURPOSE : Bespoke page for bringing up the store menu
//
// AUTHOR  : james.strain
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef STORE_PAGE_H
#define STORE_PAGE_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/PageBase.h"

class CStorePage : public CCategorylessPageBase
{
    FWUI_DECLARE_DERIVED_TYPE(CStorePage, CCategorylessPageBase );
public:
    CStorePage() : superclass() {}
    virtual ~CStorePage() {}

private: // declarations and variables
	ConstString m_productId;

    PAR_PARSABLE;
    NON_COPYABLE(CStorePage);

private: // methods
    virtual void OnEnterCompleteDerived();
    virtual void UpdateDerived( float const deltaMs );
    virtual void OnExitStartDerived();
    virtual bool OnExitUpdateDerived( float const deltaMs );

    bool IsTransitoryPageDerived() const final { return false; }
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // STORE_PAGE_H
