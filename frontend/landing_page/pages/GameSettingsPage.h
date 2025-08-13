/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : GameSettingsPage.h
// PURPOSE : Bespoke page for bringing up the game settings menu
//
// AUTHOR  : james.strain
// STARTED : December 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef GAME_SETTINGS_PAGE_H
#define GAME_SETTINGS_PAGE_H

#include "frontend/landing_page/LandingPageConfig.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/page_deck/PageBase.h"

class CGameSettingsPage : public CCategorylessPageBase
{
    FWUI_DECLARE_DERIVED_TYPE( CGameSettingsPage, CCategorylessPageBase );
public:
    CGameSettingsPage() : superclass() {}
    virtual ~CGameSettingsPage() {}

private: // declarations and variables
    PAR_PARSABLE;
    NON_COPYABLE( CGameSettingsPage );

private: // methods
    virtual void OnEnterCompleteDerived();
    virtual void UpdateDerived( float const deltaMs );

    bool IsTransitoryPageDerived() const final { return false; }
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // GAME_SETTINGS_PAGE_H
