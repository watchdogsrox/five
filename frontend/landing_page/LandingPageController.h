/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LandingPageController.h
// PURPOSE : Main access point to the modern landing page
//
// AUTHOR  : stephen.phillips/james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef LANDING_PAGE_CONTROLLER_H
#define LANDING_PAGE_CONTROLLER_H

// rage
#include "system/noncopyable.h"

// framework
#include "fwui/Foundation/Messaging/fwuiFixedMessageQueue.h"

// game
#include "control/ScriptRouterContext.h"
#include "frontend/landing_page/LandingPageConfig.h"
#include "frontend/page_deck/PageProvider.h"
#include "frontend/page_deck/IPageController.h"
#include "frontend/page_deck/uiEntrypointId.h"

#if GEN9_LANDING_PAGE_ENABLED

class CLandingPage;
class IPageViewHost;
class uiGoToPageMessage;
class uiPageDeckCanBackOutMessage;
class uiLandingPage_GoToOnlineMode;
class uiSelectCareerMessage;
class uiSelectCareerItemMessage;
class uiStartMPMigrationMessage;
class uiSelectMpCharacterSlotMessage;

class CLandingPageController final : public IPageController
{
public:
    void Launch( LandingPageConfig::eEntryPoint const entryPoint );
    void Dismiss();

    void Update();

    bool WasLaunchedFromBoot() const { return m_entryType == LandingPageConfig::eEntryPoint::ENTRY_FROM_BOOT; }
    bool WasLaunchedFromPause() const { return m_entryType == LandingPageConfig::eEntryPoint::ENTRY_FROM_PAUSE; }
    bool IsActive() const { return m_pageProvider.IsRunning(); }

private: // Declarations and variables
    friend class CLandingPage; // private creation
    CPageProvider m_pageProvider;

    LandingPageConfig::eEntryPoint m_entryType;
    bool m_initialized;
	RenderFn m_pausedRenderFunc;
    
    NON_COPYABLE( CLandingPageController );

private: // methods
    CLandingPageController();
    virtual ~CLandingPageController();

    void Initialize( IPageViewHost& viewHost );
    bool IsInitialized() const;
    char const * GetPageDataPath() const;
    void Shutdown();

    bool HandleMessage( uiPageDeckMessageBase& msg ) final;
    void HandleTransitionRequestMessage( uiGoToPageMessage const& transitionMessage );

    void EnterStoryMode();
	void EnterOnlineMode(uiLandingPage_GoToOnlineMode const& GoToOnlineModeMessage);

	void SetCriminalCareer( const uiSelectCareerMessage& selectCareerMessage );
	bool SelectCriminalCareerShoppingCartItem(const uiSelectCareerItemMessage& selectCareerMessage);

	void StartMPAccountMigration(uiStartMPMigrationMessage const& transitionMessage);
	void EnterCareerBuilder(const uiSelectMpCharacterSlotMessage & selectMpCharacterSlotMessage);

	void SetScriptRouterLink( const ScriptRouterContextData& srcData );
	void EnterEditorMode();
	bool CanBackOutCurrentPage( uiPageDeckCanBackOutMessage& backoutMsg ) const;

	static void RenderLandingPage();
    static uiEntrypointId GetEntryPointId( LandingPageConfig::eEntryPoint const entryPoint );
};

#endif // GEN9_LANDING_PAGE_ENABLED

#endif // LANDING_PAGE_CONTROLLER_H
