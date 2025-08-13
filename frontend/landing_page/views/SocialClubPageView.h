/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SocialClubPageView.h
// PURPOSE : Page view that renders the GTAV Social Club menu.
//
// AUTHOR  : james.strain
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef SOCIAL_CLUB_PAGE_VIEW_H
#define SOCIAL_CLUB_PAGE_VIEW_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/PageViewBase.h"

class CSocialClubPageView final : public CPageViewBase
{
    FWUI_DECLARE_DERIVED_TYPE(CSocialClubPageView, CPageViewBase );
public:
	CSocialClubPageView()
		: superclass()
	{}

	virtual ~CSocialClubPageView() {}
    
private: // declarations and variables
    PAR_PARSABLE;
    NON_COPYABLE(CSocialClubPageView);

private: // methods
    bool HasCustomRenderDerived() const final { return true; }
    void RenderDerived() const final;
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // SOCIAL_CLUB_PAGE_VIEW_H
