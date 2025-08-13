/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LandingPage.h
// PURPOSE : Encapsulating class for the landing page. 
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef LANDING_PAGE_H
#define LANDING_PAGE_H

// game
#include "frontend/landing_page/LandingPageConfig.h"
#include "frontend/page_deck/uiEntrypointId.h"

#if GEN9_LANDING_PAGE_ENABLED

class CLandingPageController;
class CLandingPageViewHost;

class CLandingPage /* final */
{
public:
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

    static void Launch();
    static void Dismiss();
	static void SetShouldLaunch( LandingPageConfig::eEntryPoint const entryPoint );
	static void SetShouldDismiss();

    static void Update();
    static void UpdateInstructionalButtons();
    static void Render();

    static bool ShouldShowOnBoot();
    static bool WasLaunchedFromBoot();
    static bool WasLaunchedFromPause();
    static bool IsActive();
    static bool ShouldGameRunLandingPageUpdate();

private:
	static CLandingPageController s_controllerInstance;
	static CLandingPageViewHost s_viewHostInstance;
	static LandingPageConfig::eEntryPoint ms_requestedEntryPoint;
    static bool ms_shouldShowOnBoot;

private:
    static void UpdateNetwork();
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // LANDING_PAGE_H
