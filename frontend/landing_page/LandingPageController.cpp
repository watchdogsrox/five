/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : LandingPageController.cpp
// PURPOSE : Main access point to the modern landing page
//
// AUTHOR  : stephen.phillips/james.strain
// STARTED : October 2020
//
/////////////////////////////////////////////////////////////////////////////////
#include "LandingPageController.h"

#if GEN9_LANDING_PAGE_ENABLED

// rage
#include "core/core_channel.h"
#include "system/param.h"

// framework
#include "fwsys/fileExts.h"
#include "fwsys/timer.h"

 // game
#include "control/ScriptRouterLink.h"
#include "frontend/career_builder/items/CareerBuilderShoppingCartItem.h"
#include "frontend/career_builder/messages/uiCareerBuilderMessages.h"
#include "frontend/save_migration/messages/uiMPMigrationMessages.h"
#include "SaveMigration/SaveMigration.h"
#include "frontend/loadingscreens.h"
#include "frontend/landing_page/LandingPage.h"
#include "frontend/landing_page/messages/uiLandingPageMessages.h"
#include "frontend/page_deck/messages/uiPageDeckMessages.h"
#include "frontend/MousePointer.h"
#include "frontend/UIContexts.h"
#include "Peds/CriminalCareer/CriminalCareerService.h"
#include "network/live/NetworkTelemetry.h"
#include "network/SocialClubServices/SocialClubFeedTilesMgr.h"
#include "system/controlMgr.h"
#include "system/param.h"


#define LANDING_PAGE_DATA_DEFAULT "platform://data/ui/landing_page_deck." META_FILE_EXT

void CLandingPageController::Launch( LandingPageConfig::eEntryPoint const entryPoint )
{
    if( !IsActive() )
    {
        m_entryType = entryPoint;
        CNetworkTelemetry::LandingPageEnter();

		// Inject our own render functor here in place of the loadingscreens render path on boot and game render phases thereafter
		m_pausedRenderFunc = gRenderThreadInterface.GetRenderFunction();
		gRenderThreadInterface.SetRenderFunction(MakeFunctor(CLandingPageController::RenderLandingPage));
		gRenderThreadInterface.Flush();

        // If someone carelessly left the pause menu open, we want to ensure it is shut-down
        if( CPauseMenu::IsActive())
        {
            CPauseMenu::Shutdown(SHUTDOWN_SESSION);
        }

		if (entryPoint != LandingPageConfig::eEntryPoint::ENTRY_FROM_BOOT)
		{
			// Suspend script updates until we return to the game. This is not called on boot so landing_pre_startup.sc
			//   can handle any pending script router link from an external source (e.g. a console Activity).
			scrThread::SetIsBlocked(true);
		}

        uiEntrypointId const c_entryPointId = GetEntryPointId( entryPoint );
        m_pageProvider.LaunchToPage( c_entryPointId );
    }
}

void CLandingPageController::Dismiss()
{
    if( IsActive() )
    {
        m_pageProvider.Dismiss();

		// Restore the render functor we replaced
		gRenderThreadInterface.SetRenderFunction(m_pausedRenderFunc);
		gRenderThreadInterface.Flush();

		scrThread::SetIsBlocked(false);

		CSocialClubFeedTilesMgr::Get().ClearFeedsContent(false);
        CNetworkTelemetry::LandingPageExit();

        m_entryType = LandingPageConfig::eEntryPoint::ENTRY_COUNT;
    }
}

void CLandingPageController::Update()
{
    if ( IsActive() )
    {
        float const c_deltaMs = (float)fwTimer::GetTimeStepInMilliseconds_NonPausedNonScaledClipped();
        m_pageProvider.Update( c_deltaMs );

		// dismiss landing page if an invite has been accepted or confirmed
		InviteMgr& inviteMgr = CLiveManager::GetInviteMgr();
		if (inviteMgr.HasAcceptedOrConfirmed())
		{			
			CLandingPage::SetShouldDismiss();
		}

        CNetworkTelemetry::LandingPageUpdate();
    }
}

CLandingPageController::CLandingPageController()
    : m_entryType( LandingPageConfig::eEntryPoint::ENTRY_COUNT )
    , m_initialized( false )
{
    
}

CLandingPageController::~CLandingPageController()
{
    Shutdown();
}

void CLandingPageController::Initialize( IPageViewHost& viewHost )
{
    if( !IsInitialized())
    {
        parSettings settings = PARSER.Settings();
        settings.SetFlag( parSettings::CULL_OTHER_PLATFORM_DATA, true );
        char const * const c_pageDataPath = GetPageDataPath();
        ASSERT_ONLY( bool const c_success = ) PARSER.LoadObject( c_pageDataPath, nullptr, m_pageProvider, &settings );
        uiFatalAssertf( c_success, "Failed to load Landing Page data from '%s'", c_pageDataPath );
        m_pageProvider.PostLoadInitialize( viewHost );
        m_initialized = true;
    }
}

bool CLandingPageController::IsInitialized() const
{
    return m_initialized;
}

char const * CLandingPageController::GetPageDataPath() const
{
    return LANDING_PAGE_DATA_DEFAULT;
}

void CLandingPageController::Shutdown()
{
    m_pageProvider.Shutdown();
    m_initialized = false;
}

bool CLandingPageController::HandleMessage( uiPageDeckMessageBase& msg )
{
    bool result = false;

	FWUI_MESSAGE_BIND_REF_RESULT( uiPageDeckCanBackOutMessage, msg, CanBackOutCurrentPage, result );
    FWUI_MESSAGE_CALL_REF_SUCCESS( uiLandingPage_GoToStoryMode, msg, EnterStoryMode, result );
    FWUI_MESSAGE_BIND_REF_RESULT_SUCCESS_CONST( uiLandingPage_GoToOnlineMode, msg, EnterOnlineMode, result );
	FWUI_MESSAGE_CALL_REF_SUCCESS( uiLandingPage_GoToEditorMode, msg, EnterEditorMode, result );
    FWUI_MESSAGE_CALL_REF_SUCCESS( uiLandingPage_Dismiss, msg, Dismiss, result );
    FWUI_MESSAGE_BIND_REF_RESULT_SUCCESS_CONST( uiGoToPageMessage, msg, HandleTransitionRequestMessage, result );
    FWUI_MESSAGE_BIND_REF_RESULT_SUCCESS_CONST( uiSelectCareerMessage, msg, SetCriminalCareer, result );
	FWUI_MESSAGE_BIND_REF_RESULT( uiSelectCareerItemMessage, msg, SelectCriminalCareerShoppingCartItem, result );
	FWUI_MESSAGE_BIND_REF_RESULT_SUCCESS_CONST( uiStartMPMigrationMessage, msg, StartMPAccountMigration, result);
	FWUI_MESSAGE_BIND_REF_RESULT_SUCCESS_CONST(uiSelectMpCharacterSlotMessage, msg, EnterCareerBuilder, result);

    CNetworkTelemetry::LandingPageTrackMessage( msg, result );
    return result;
}

void CLandingPageController::HandleTransitionRequestMessage( uiGoToPageMessage const& transitionMessage )
{
    bool processAsTransition = true;

    // If we got a back message and were launched from pause, we need to swap to a dismiss
    // message if we are at the root page and can only back out of the menu
    // Stephen.Phillips asked me to disable this for now as current designs have launching to 
    // landing from pause menu as a destructive action
    /*if( transitionMessage.GetIsClass<uiPageDeckBackMessage>() && WasLaunchedFromPause() )
    {
        bool const c_hasBackoutTarget = m_pageProvider.CanBackOut();
        if( !c_hasBackoutTarget )
        {
            processAsTransition = c_hasBackoutTarget;
            Dismiss();
        }
    }*/	

    if( processAsTransition )
    {
        uiPageLink const& c_linkInfo = transitionMessage.GetLinkInfo();
        m_pageProvider.RequestTransition( c_linkInfo );
    }
}

void CLandingPageController::EnterStoryMode()
{
	SetScriptRouterLink(ScriptRouterContextData(SRCS_NATIVE_LANDING_PAGE, SRCM_STORY));

	CLandingPage::SetShouldDismiss();
}

void CLandingPageController::EnterEditorMode()
{
	SetScriptRouterLink(ScriptRouterContextData(SRCS_NATIVE_LANDING_PAGE, SRCM_EDITOR));

	CLandingPage::SetShouldDismiss();
}

void CLandingPageController::EnterOnlineMode( uiLandingPage_GoToOnlineMode const& GoToOnlineModeMessage )
{
	// validate online mode type
	uiAssertf(GoToOnlineModeMessage.GetScriptRouterOnlineModeType() != SRCM_STORY, "uiLandingPage_GoToOnlineMode: Script Router Mode is invalid (SRCM_STORY).");
	if( GoToOnlineModeMessage.GetScriptRouterOnlineModeType() != SRCM_STORY )
	{
		// set script router link from GoToOnlineModeMessage
		SetScriptRouterLink(ScriptRouterContextData(SRCS_NATIVE_LANDING_PAGE,
			GoToOnlineModeMessage.GetScriptRouterOnlineModeType(),
			GoToOnlineModeMessage.GetScriptRouterArgType(),
			GoToOnlineModeMessage.GetScriptRouterArg()));

		CLandingPage::SetShouldDismiss();
	}
}

void CLandingPageController::SetCriminalCareer( const uiSelectCareerMessage& selectCareerMessage )
{
	// Update career choice
	CCriminalCareerService& criminalCareerService = SCriminalCareerService::GetInstance();
	const CriminalCareerDefs::CareerIdentifier& c_careerId = selectCareerMessage.GetCareerIdentifier();
	criminalCareerService.SetChosenCareerIdentifier(c_careerId);

	// Then navigate to Career shop
	uiPageLink const& c_linkInfo = selectCareerMessage.GetLinkInfo();
	m_pageProvider.RequestTransition( c_linkInfo );
}

bool CLandingPageController::SelectCriminalCareerShoppingCartItem( const uiSelectCareerItemMessage& selectItemMessage )
{
	// Update shopping cart
	CCriminalCareerService& criminalCareerService = SCriminalCareerService::GetInstance();
	const CriminalCareerDefs::ItemIdentifier& itemId = selectItemMessage.GetItemIdentifier();
	const CCriminalCareerItem* p_item = criminalCareerService.TryGetItemWithIdentifier(itemId);
	if( uiVerifyf( p_item != nullptr, "Failed to retrieve CriminalCareerItem with ItemIdentifier " HASHFMT, HASHOUT(itemId) ) )
	{
		const CCriminalCareerItem& c_item = *p_item;
		const bool c_isAlwaysInShoppingCart = c_item.GetFlags().IsSet(CriminalCareerDefs::AlwaysInShoppingCart);
		if( !c_isAlwaysInShoppingCart )
		{
			const bool c_wasInShoppingCart = criminalCareerService.IsItemInShoppingCart(c_item);
			if( c_wasInShoppingCart )
			{
				criminalCareerService.RemoveItemFromShoppingCart(c_item);
			}
			else
			{
				criminalCareerService.AddItemToShoppingCart(c_item);
			}
			return true;
		}
	}
	return false;
}

void CLandingPageController::StartMPAccountMigration(uiStartMPMigrationMessage const & MPMigrationMessage)
{	
	rlSaveMigrationMPAccount chosenAccount = CSaveMigration::GetMultiplayer().GetChosenAccount();
	eSaveMigrationStatus currentMigrationStatus = CSaveMigration::GetMultiplayer().GetMigrationStatus();
	if (uiVerifyf(currentMigrationStatus == SM_STATUS_NONE, "Tried to migrate but migration status is: %d", currentMigrationStatus))
	{
		CSaveMigration::GetMultiplayer().MigrateAccount(chosenAccount.m_AccountId, chosenAccount.m_Platform);
		uiPageLink const& c_linkInfo = MPMigrationMessage.GetLinkInfo();
		m_pageProvider.RequestTransition( c_linkInfo );
	}	
}

void CLandingPageController::EnterCareerBuilder(const uiSelectMpCharacterSlotMessage& selectMpCharacterSlotMessage)
{
	// Update chosen character slot	
	const int c_chosenMpCharacterSlot = selectMpCharacterSlotMessage.GetChosenMpCharacterSlot();
	CSaveMigration::GetMultiplayer().SetChosenMpCharacterSlot(c_chosenMpCharacterSlot);

	// Then navigate to the career builder
	uiPageLink const& c_linkInfo = selectMpCharacterSlotMessage.GetLinkInfo();
	m_pageProvider.RequestTransition( c_linkInfo );
}

void CLandingPageController::SetScriptRouterLink( const ScriptRouterContextData& srcData )
{
	// We set the link in response to user input, any existing link should be cleared.
	if( ScriptRouterLink::HasPendingScriptRouterLink() )
	{
		ScriptRouterLink::ClearScriptRouterLink();
	}

	// Once the ScriptRouterLink has been set, it is processed by MainTransition.sc
	//  and redirects us to the correct game mode, dismissing the Landing Page.
	//  If this occurs pre-boot, it is processed by landing_pre_startup.sc instead.
	ScriptRouterLink::SetScriptRouterLink(srcData);
}

bool CLandingPageController::CanBackOutCurrentPage( uiPageDeckCanBackOutMessage& backoutMsg ) const
{
    bool canBackOut = false;
    // If we were launched from pause, we can back out to the pause menu
    // Stephen.Phillips asked me to disable this for now as current designs have launching to 
    // landing from pause menu as a destructive action
    /*canBackOut = WasLaunchedFromPause();
    if( !canBackOut )*/
    {
        // ...otherwise we need to see if we have a page we can back out to
        canBackOut = m_pageProvider.CanBackOut();
    }

    backoutMsg.SetResult( canBackOut );

    return true;
}

uiEntrypointId CLandingPageController::GetEntryPointId( LandingPageConfig::eEntryPoint const entryPoint )
{
    uiEntrypointId result;
    
    switch( entryPoint )
    {
    case LandingPageConfig::eEntryPoint::ENTRY_FROM_BOOT:
        {
            result = uiEntrypointId( ATSTRINGHASH( "fromBoot", 0x636A4812 ) );
            break;
        }
    case LandingPageConfig::eEntryPoint::ENTRY_FROM_PAUSE:
        {
            result = uiEntrypointId( ATSTRINGHASH( "fromPauseMenu", 0xD174B801 ) );
            break;
        }
	case LandingPageConfig::eEntryPoint::SINGLEPLAYER_MIGRATION:
		{
			result = uiEntrypointId( ATSTRINGHASH( "spMigration", 0xA5CE14E8 ) );
			break;
		}
	case LandingPageConfig::eEntryPoint::MULTIPLAYER_REVIEW_CHARACTERS:
		{
			result = uiEntrypointId( ATSTRINGHASH( "mpReviewCharacters", 0x5E3BB166 ) );
			break;
		}
	case LandingPageConfig::eEntryPoint::STORYMODE_UPSELL:
		{
			result = uiEntrypointId( ATSTRINGHASH( "spUpsell", 0x8DE577DC ) );
			break;
		}
    default:
        uiFatalAssertf( false, "Unsupported value %u requested", (u32)entryPoint );
    }

    return result;
}

void CLandingPageController::RenderLandingPage()
{
	PF_FRAMEINIT_TIMEBARS(0);

	CSystem::BeginRender();
    CRenderTargets::LockUIRenderTargets();
#if IS_GEN9_PLATFORM
    sga::g_GraphicsContext->BeginRenderPassAuto();
#endif // IS_GEN9_PLATFORM

	const grcViewport* pVp = grcViewport::GetDefaultScreen();

	GRCDEVICE.Clear(true, Color32(0, 0, 0), false, 0, false, 0);

	grcViewport::SetCurrent(pVp);
	grcLightState::SetEnabled(false);
    grcStateBlock::Default();
	grcWorldIdentity();

	CLandingPage::Render();

    if( CWarningScreen::IsActive() )
    {
        CWarningScreen::Render();
    }

#if IS_GEN9_PLATFORM
    sga::g_GraphicsContext->EndRenderPass();
#endif // IS_GEN9_PLATFORM
    CRenderTargets::UnlockUIRenderTargets();

#if IS_GEN9_PLATFORM

    //Apply UI with scaled brightness based on UI text white point
    if (HDRReconstruction::GetInstance()->IsHDRReconstructionEnabled())
    {
        HDRReconstruction::GetInstance()->ReconstructLoadingScreen(CRenderTargets::GetUIBackBuffer());
    }
#else
    grcViewport::SetCurrent(NULL);
#endif // IS_GEN9_PLATFORM

	CSystem::EndRender();
}

#endif // GEN9_LANDING_PAGE_ENABLED
