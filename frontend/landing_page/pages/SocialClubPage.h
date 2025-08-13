/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SocialClubPage.h
// PURPOSE : Bespoke page for bringing up the social club menu
//
// AUTHOR  : james.strain
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef SOCIAL_CLUB_PAGE_H
#define SOCIAL_CLUB_PAGE_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/PageBase.h"

class CSocialClubPage : public CCategorylessPageBase
{
    FWUI_DECLARE_DERIVED_TYPE(CSocialClubPage, CCategorylessPageBase );
public:
	CSocialClubPage();
    virtual ~CSocialClubPage() {}

private: // declarations and variables
    PAR_PARSABLE;
    NON_COPYABLE(CSocialClubPage);

	uiPageDeckMessageBase* m_onDismissNotSignedInAction;

private: // methods
    void OnEnterCompleteDerived() override;
    void UpdateDerived( float const deltaMs ) override;

    bool IsTransitoryPageDerived() const final { return false; }
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // SOCIAL_CLUB_PAGE_H
