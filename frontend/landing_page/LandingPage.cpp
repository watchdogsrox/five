/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LandingPage.cpp
// PURPOSE : Encapsulating class for the landing page. 
//
// AUTHOR  : james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "LandingPage.h"

#if GEN9_LANDING_PAGE_ENABLED

// game
#include "frontend/landing_page/LandingPageController.h"
#include "frontend/landing_page/LandingPageViewHost.h"
#include "network/Network.h"
#include "scene/DownloadableTextureManager.h"

CLandingPageController CLandingPage::s_controllerInstance;
CLandingPageViewHost CLandingPage::s_viewHostInstance;
LandingPageConfig::eEntryPoint CLandingPage::ms_requestedEntryPoint = LandingPageConfig::eEntryPoint::ENTRY_FROM_BOOT;
bool CLandingPage::ms_shouldShowOnBoot = true;

void CLandingPage::Init(unsigned int initMode)
{
	if( initMode == INIT_CORE )
	{
		s_viewHostInstance.Initialize(s_controllerInstance);
		s_controllerInstance.Initialize(s_viewHostInstance);
	}
}

void CLandingPage::Shutdown(unsigned int shutdownMode)
{
	if( shutdownMode == SHUTDOWN_CORE )
	{
		s_viewHostInstance.Shutdown();
		s_controllerInstance.Shutdown();
	}
}

void CLandingPage::Launch()
{
    ms_shouldShowOnBoot = false;
	s_controllerInstance.Launch( ms_requestedEntryPoint );
	s_viewHostInstance.LoadPersistentAssets();
}

void CLandingPage::Dismiss()
{
	s_controllerInstance.Dismiss();
	s_viewHostInstance.UnloadPersistentAssets();
}

void CLandingPage::SetShouldLaunch( LandingPageConfig::eEntryPoint const entryPoint )
{
	ms_requestedEntryPoint = entryPoint;
	CGameSessionStateMachine::SetState(CGameSessionStateMachine::SHUTDOWN_FOR_LANDING_PAGE);	
}

void CLandingPage::SetShouldDismiss()
{
	CGameSessionStateMachine::SetState(CGameSessionStateMachine::DISMISS_LANDING_PAGE);
}

void CLandingPage::Update()
{
    if( s_controllerInstance.IsActive() )
    {
		s_controllerInstance.Update();
		s_viewHostInstance.Update();
    }

    UpdateNetwork();
}

void CLandingPage::UpdateNetwork()
{
    CNetwork::Update(false);
    DOWNLOADABLETEXTUREMGR.Update();
}

void CLandingPage::UpdateInstructionalButtons()
{
    if( s_controllerInstance.IsActive() )
    {
		s_viewHostInstance.UpdateInstructionalButtons();
    }
}

void CLandingPage::Render()
{
    if( IsActive() )
    {
		s_viewHostInstance.Render();
    }
}

bool CLandingPage::WasLaunchedFromBoot()
{
    return s_controllerInstance.WasLaunchedFromBoot();
}

bool CLandingPage::WasLaunchedFromPause()
{
    return s_controllerInstance.WasLaunchedFromPause();
}

bool CLandingPage::IsActive()
{
    return s_controllerInstance.IsActive();
}

bool CLandingPage::ShouldGameRunLandingPageUpdate()
{
    // We only want the bespoke update when active.
    bool const c_isActive = s_controllerInstance.IsActive();
    return c_isActive;
}

bool CLandingPage::ShouldShowOnBoot()
{
    return ms_shouldShowOnBoot;
}

#endif // GEN9_LANDING_PAGE_ENABLED
