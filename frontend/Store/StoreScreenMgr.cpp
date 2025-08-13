#include "Frontend/Store/StoreScreenMgr.h"

#include "Frontend/BusySpinner.h"
#include "Frontend/MiniMap.h"
#include "Frontend/NewHud.h"
#include "Frontend/Store/StoreAdvertScreen.h"
#include "Frontend/Store/StoreMainScreen.h"
#include "Frontend/Store/StoreTextureManager.h"
#include "Frontend/Store/StoreUIChannel.h"
#include "frontend/landing_page/LandingPage.h"
#include "Game/User.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Network/Live/livemanager.h"
#include "Network/Sessions/NetworkSession.h"
#include "System/controlMgr.h"
#include "Text/TextConversion.h"
#include "Vfx/Misc/MovieManager.h"
#include "fwcommerce/CommerceUtil.h"
#include "optimisations.h"
#include "peds/ped.h"
#include "scene/world/GameWorld.h"
#include "Stats/MoneyInterface.h"
#include "Stats/StatsInterface.h"
#include "streaming/defragmentation.h"
#include "streaming/streaming.h"
#include "system/FileMgr.h"
#include "text/messages.h"
#include "Network/NetworkInterface.h"

PARAM(leavempforstore, "The store will cause the player to leave mp sessions.");
PARAM(storeInitialProductId, "Initial Item to Highlight in the Store");

FRONTEND_STORE_OPTIMISATIONS()

#if __XENON
#include <xbox.h>
#endif

#if __BANK
#include "bank/bkmgr.h"
#endif // __BANK

cStoreMainScreen* cStoreScreenMgr::mp_StoreMainScreen = NULL;
bool cStoreScreenMgr::m_IsInitialised = false;
int  cStoreScreenMgr::m_ExitRequestedFrameCount = -1;
bool cStoreScreenMgr::m_bStoreIsOpen = false;

CSprite2d cStoreScreenMgr::sm_BackgroundGradient;
bool cStoreScreenMgr::m_LoadingBgSprite = false;

atString cStoreScreenMgr::m_BaseCategory;
atString cStoreScreenMgr::m_InitialProductID;
bool cStoreScreenMgr::m_InitialProductCheckout = false;

//These defaults are just the last values used before we shifted to the frontend.xml
float cStoreScreenMgr::m_MovieXPos = 0.13632f;
float cStoreScreenMgr::m_MovieYPos = 0.075693f;
float cStoreScreenMgr::m_MovieWidth = 0.728903f;
float cStoreScreenMgr::m_MovieHeight = 0.831947f;

bool cStoreScreenMgr::m_HasXMLBeenRead = false;

int cStoreScreenMgr::m_CurrentStoreBranding = 0;

grcBlendStateHandle cStoreScreenMgr::ms_StandardSpriteBlendStateHandle = grcStateBlock::BS_Invalid;

bool cStoreScreenMgr::m_ReturnToPauseMenu = false;
GEN9_STANDALONE_ONLY(bool cStoreScreenMgr::m_LaunchLandingPageOnClose = false);

cStoreTextureManager cStoreScreenMgr::m_StoreTextureManager;

bool cStoreScreenMgr::m_WasOpenedFromNetworkGame = false;
bool cStoreScreenMgr::m_PendingSessionShutdownToOpen = false;

const int DEFAULT_NUM_ITEMS_PER_COLUMN = 2;
int cStoreScreenMgr::m_StoreItemNumPerColumn = DEFAULT_NUM_ITEMS_PER_COLUMN;

int cStoreScreenMgr::m_ResetPauseRenderPhasesFrameCount = 0;
int cStoreScreenMgr::m_TogglePauseRenderPhasesFrameCount = 0;	
const int FRAMES_TO_DELAY_RENDER_PHASE_RESET = 5;
const int FRAMES_TO_DELAY_RENDER_PHASE_TOGGLE = 8;

const char* COMMERCE_SORT_TAG_MP = "MP";

bool cStoreScreenMgr::m_StoreEnabled = true;

bool cStoreScreenMgr::m_PermitCashProducts = true;
bool cStoreScreenMgr::m_ShouldShowCashProducts = true;

int cStoreScreenMgr::m_StoreOpenFrameDelay = 0;

s64 cStoreScreenMgr::m_WalletCashAmountForDisplay = 0;
s64 cStoreScreenMgr::m_BankCashAmountForDisplay = 0;

atString cStoreScreenMgr::m_PlayerNameForDisplay;

#define	STOREMENU_TXD_PATH ("platform:/textures/frontend")
#define FRONTEND_XML_FILENAME "common:/data/ui/frontend.xml"
#define COMMERCE_NODE "CommerceStore"

const int MP_STORE_FRAME_DELAY = 3;

RAGE_DEFINE_CHANNEL(storeUI)

namespace rage {
    extern char *XEX_TITLE_ID;	//	defined in gta5\src\dev\game\Core\main.cpp
}

void cStoreScreenMgr::Init(unsigned initMode)
{
    if ( INIT_CORE == initMode )
    {
        storeUIAssert(!m_IsInitialised);
        m_IsInitialised = true;

		REGISTER_FRONTEND_XML(cStoreScreenMgr::HandleXML, COMMERCE_NODE);
    }

    m_bStoreIsOpen = false;

    m_CurrentStoreBranding = 0;

    m_StoreItemNumPerColumn = DEFAULT_NUM_ITEMS_PER_COLUMN;

    m_StoreEnabled = true;

	m_PlayerNameForDisplay.Reset();
	m_WalletCashAmountForDisplay = 0;
	m_BankCashAmountForDisplay = 0;

    //Setup the state block
    // stateblocks
    grcBlendStateDesc bsd;

    //	The remaining BlendStates should all have these two flags set
    bsd.BlendRTDesc[0].BlendEnable = true;
    bsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
    bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
    bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
    bsd.BlendRTDesc[0].SrcBlendAlpha = grcRSV::BLEND_SRCALPHA;
    bsd.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_INVSRCALPHA;
    bsd.BlendRTDesc[0].BlendOpAlpha = grcRSV::BLENDOP_ADD;
    bsd.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
    ms_StandardSpriteBlendStateHandle = grcStateBlock::CreateBlendState(bsd);

    m_ReturnToPauseMenu = false;
	m_StoreOpenFrameDelay = 0;

    m_BaseCategory = "TOP_DEFAULT"; 
    m_InitialProductID.Reset();
    m_InitialProductCheckout = false;

	m_ShouldShowCashProducts = true;
}

void cStoreScreenMgr::Shutdown(unsigned shutdownMode)
{
    if ( SHUTDOWN_CORE == shutdownMode )
    {
        storeUIAssert(m_IsInitialised);
        m_IsInitialised = false;

        if ( mp_StoreMainScreen )
        {
            delete mp_StoreMainScreen;
            mp_StoreMainScreen = NULL;
        }

        m_InitialProductID.Reset();
        m_InitialProductCheckout = false;
    }
}

void cStoreScreenMgr::Update()
{
    if ( !m_IsInitialised )
    {
        return;
    }

	PF_AUTO_PUSH_TIMEBAR("cStoreScreenMgr Update");

    if ( mp_StoreMainScreen )
    {
        mp_StoreMainScreen->Update();
    }

	// added check to stop any potential loop around
	if (m_ExitRequestedFrameCount > 0)
		--m_ExitRequestedFrameCount;

    if ( m_ExitRequestedFrameCount == 0 )
    {
		storeUIDebugf1("Exit Request Complete, Closing...");
		
        m_ExitRequestedFrameCount = -1;
        if (!Close())
		{
			//Because MS abort function can stall. Thanks MS.
			m_ExitRequestedFrameCount = 1;
			return;
		}
    }

	if ( IsMPGameReadyToOpenStore() )
    {
		storeUIDebugf1("Opening MP store");

        m_PendingSessionShutdownToOpen = false;
        m_ShouldShowCashProducts = true;

		if (CPauseMenu::IsActive())
		{
			CPauseMenu::TriggerStore();
		}
		else
		{
			char cHtmlFormattedBusyString[256];
			char* html = CTextConversion::TextToHtml(TheText.Get("STORE_LOADING"),cHtmlFormattedBusyString,sizeof(cHtmlFormattedBusyString));
			CBusySpinner::On( html, SPINNER_ICON_LOADING, SPINNER_SOURCE_STORE_LOADING );
			Open(false);
		}

// 		PM_ActiveFlags pauseMenuActiveFlags = PM_SkipStore;
// 		if (CPauseMenu::IsActive(pauseMenuActiveFlags))
// 		{
// 			CPauseMenu::Close();
// 		}
    }
    
	if ( m_StoreOpenFrameDelay > 0 )
	{
		m_StoreOpenFrameDelay--;
	}

    m_StoreTextureManager.Update();
    
	if (m_ResetPauseRenderPhasesFrameCount > 0)
	{
		--m_ResetPauseRenderPhasesFrameCount;

		if (m_ResetPauseRenderPhasesFrameCount == 0)
		{
			CPauseMenu::ResetPauseRenderPhases();
		}
	}

	if (m_TogglePauseRenderPhasesFrameCount > 0)
	{
		--m_TogglePauseRenderPhasesFrameCount;

		if (m_TogglePauseRenderPhasesFrameCount == 0)
		{
			//This matches the call we have in cStoreManager::OpenInternal()
			CPauseMenu::TogglePauseRenderPhases(true, OWNER_STORE, __FUNCTION__ );
		}
	}
}

void cStoreScreenMgr::Render()
{
    if ( !m_IsInitialised )
    {
        return;
    }

    if ( !m_bStoreIsOpen )
    {
        return;
    }

	/* B* 1083542: No more store gradient
    grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
    grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
    grcStateBlock::SetBlendState(ms_StandardSpriteBlendStateHandle);

    RenderBackground();
	*/

    grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
    grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default);
    grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
    CSprite2d::ClearRenderState();
    CShaderLib::SetGlobalEmissiveScale(1.0f,true);

    if ( mp_StoreMainScreen )
    {
        mp_StoreMainScreen->Render();
    }
}

void cStoreScreenMgr::DisplayAdScreen()
{

}

void cStoreScreenMgr::Open(bool closeToPauseMenu GEN9_STANDALONE_ONLY(, bool launchLandingPageOnClose))
{
#if GEN9_STANDALONE_ENABLED
	m_LaunchLandingPageOnClose = launchLandingPageOnClose;
#endif

#if RSG_PC
	(void)closeToPauseMenu;
	OpenPC();
	return;
#else

    m_ReturnToPauseMenu = closeToPauseMenu;

	//Do not allow the store to be opened if the open is already in progress
	if (m_PendingSessionShutdownToOpen)
	{
		return;
	}

// 	atString baseCat("COMPAT_PACKS");
// 	cStoreScreenMgr::SetBaseCategory(baseCat);

    //If we are in a network game get out
    if ( NetworkInterface::IsGameInProgress() && PARAM_leavempforstore.Get())
    {
		// we don't leave multiplayer anymore
    }
    else
    {
		if ( m_WasOpenedFromNetworkGame || NetworkInterface::IsGameInProgress())
		{
			m_ShouldShowCashProducts = true;
		}
		else
		{
			m_ShouldShowCashProducts = false;
		}

        if ( m_WasOpenedFromNetworkGame )
		{
			m_ReturnToPauseMenu = false;
		}
		else
		{
			//Use this opportunity to store values for display in the store.
			PopulateDisplayValues();
		}

#if !RSG_FINAL
        const char* initialProductId = nullptr;
        if(PARAM_storeInitialProductId.Get(initialProductId))
        {
			storeUIDebugf1("Open :: Command Line InitialProductId: %s", initialProductId);
			m_InitialProductID = initialProductId;
        }
#endif
        
        OpenInternal();
    }
#endif //RSG_PC
}

bool cStoreScreenMgr::IsLoadingAssets()
{
	bool const c_result = mp_StoreMainScreen && mp_StoreMainScreen->IsLoadingAssets();
	return c_result;
}

bool cStoreScreenMgr::RequestMPStore()
{
    storeUIDebugf1("RequestMPStore :: IsGameInProgress: %s", NetworkInterface::IsGameInProgress() ? "True" : "False");
    
    //If we are in a network game get out
	if (NetworkInterface::IsGameInProgress() && PARAM_leavempforstore.Get() )
	{
        // we don't leave multiplayer anymore
	}
	else
	{
		Open(true);
	}

    // might not be the case if !NetworkInterface::IsGameInProgress() but 
    // taking small steps for now
    return true;
}

void cStoreScreenMgr::ResetNetworkGameTracking()
{
    storeUIDebugf1("ResetNetworkGameTracking :: FromNetworkGame: %s", m_WasOpenedFromNetworkGame ? "True" : "False");

	if(m_WasOpenedFromNetworkGame)
	{
		// we need to check for a full network close here as we only run a partial close when entering the store to
		// avoid a long load on entry and exit within multiplayer
		CNetwork::CloseNetwork(true);
#if RSG_DURANGO
		CNetwork::GetNetworkSession().RemoveFromParty(false);
#endif // RSG_DURANGO
		m_WasOpenedFromNetworkGame = false;
	}
}

void cStoreScreenMgr::OpenInternal()
{
	// NOTE: PS3 marketplace has to page out 17MB of memory to system virtual in order to load the commerce data.
	// This CANNOT happen during the Update function otherwise performance will go to shit. To get around this, 
	// we wait until the beginning of the next Update frame via a call to ContainerReserveStore (via game.cpp).
#if COMMERCE_CONTAINER
	CLiveManager::GetCommerceMgr().ContainerReserveStore();
}

void cStoreScreenMgr::OpenInternalPhase2()
{
#endif

	if (!NetworkInterface::IsGameInProgress())
	    CPauseMenu::TogglePauseRenderPhases(false, OWNER_STORE, __FUNCTION__ );

    LoadXmlData();

    char cHtmlFormattedBusyString[256];
    char* html = CTextConversion::TextToHtml(TheText.Get("STORE_LOADING"),cHtmlFormattedBusyString,sizeof(cHtmlFormattedBusyString));
    CBusySpinner::On( html, SPINNER_ICON_LOADING, SPINNER_SOURCE_STORE_LOADING );

    storeUIAssert( mp_StoreMainScreen == NULL );
    if ( mp_StoreMainScreen != NULL )
    {
        return;
    }

    mp_StoreMainScreen = rage_new cStoreMainScreen;
    mp_StoreMainScreen->Init();

	//No longer used.
	//CNetworkTelemetry::EnterIngameStore();

    if (CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->GetPlayerInfo())
    {
        CGameWorld::FindLocalPlayer()->GetPlayerInfo()->DisableControlsFrontend();
    }

    //Check for the RoS cash permission here.
	if (NetworkInterface::GetLocalGamerIndex() == -1 || !rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_PURCHASEVC) || !MoneyInterface::CanBuyCash(0, 0.0f))
	{
		storeUIDebugf1("OpenInternal :: Cash Products Not Allowed: HasPrivilege: %s, CanBuyCash: %s",
			rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_PURCHASEVC) ? "True" : "False",
			MoneyInterface::CanBuyCash(0, 0.0f) ? "True" : "False");
		SetCashProductsAllowed(false);
	}
	else
	{	
		storeUIDebugf1("OpenInternal :: Cash Products Allowed");
		SetCashProductsAllowed(true);
	}
 
    //Start the commerce util
    CLiveManager::GetCommerceMgr().CreateCommerceUtil();
    bool isInitialized = CLiveManager::GetCommerceMgr().InitialiseCommerceUtil();

#if RSG_GEN9
    // url:bugstar:7422191 - Gen9 - Story mode / SP upgrade is available on US Disk SKU(in - game and preview store). Story mode is already included on Disk versions of the game.
    // Suppress the Story Mode item if running a version built against Disk SKU. The story mode item id was obtained from RageCommerceData used to populate the store.
    if (SUserEntitlementService::GetInstance().GetGameEntitlementMedium() == UserEntitlementMedium::UEM_DISC)
    {
		storeUIDebugf1("OpenInternal :: Suppressing SPUpgrade: Already entitled");
		CLiveManager::GetCommerceMgr().GetCommerceUtil()->SuppressItemId("SPUpgrade");
    }
#endif //RSG_GEN9

	//We now limit cash products to only within MP games. In the TU I will change this to be based on the commerce data,
	//but since this is going into the main branch I am keeping it simple.
	if ( !m_ShouldShowCashProducts || StatsInterface::GetBlockSaveRequests() || !MoneyInterface::SavegameCanBuyCashProducts() )
	{
		//Suppress cash if required
		CLiveManager::GetCommerceMgr().GetCommerceUtil()->SuppressItemId("Cash");

        //Suppress packs as well
        CLiveManager::GetCommerceMgr().GetCommerceUtil()->SuppressItemId("Packs");

#if RSG_GEN9
		//This is being effectively treated as the "suppress if not in MP" section now. Ideally migrate to a data driven solution, but for now add GTA+ cash cards and the story upgrade
        if(!CLandingPage::IsActive())
        {
            CLiveManager::GetCommerceMgr().GetCommerceUtil()->SuppressItemId("SPUpgrade"); //Removes story mode upgrade from SP
		}
        CLiveManager::GetCommerceMgr().GetCommerceUtil()->SuppressItemId("GTA+Cash"); //Removes GTA+ specific cash from SP
        CLiveManager::GetCommerceMgr().GetCommerceUtil()->SuppressItemId("GTAPlus"); //Removes GTA+ specific cash from SP
#endif

		storeUIDebugf1("OpenInternal :: Suppressing Cash/Packs: ShouldShowCashProducts: %s, GetBlockSaveRequests: %s, SavegameCanBuyCashProducts: %s", 
			m_ShouldShowCashProducts ? "True" : "False",
			StatsInterface::GetBlockSaveRequests() ? "True" : "False",
			MoneyInterface::SavegameCanBuyCashProducts() ? "True" : "False");
	}
    else if (CLiveManager::GetCommerceMgr().IsEntitledToPremiumPack() || CLiveManager::GetCommerceMgr().IsEntitledToStarterPack())
	{
		storeUIDebugf1("OpenInternal :: Suppressing Packs: IsEntitledToPremiumPack: %s, IsEntitledToStarterPack: %s",
			CLiveManager::GetCommerceMgr().IsEntitledToPremiumPack() ? "True" : "False",
			CLiveManager::GetCommerceMgr().IsEntitledToStarterPack() ? "True" : "False");

        CLiveManager::GetCommerceMgr().GetCommerceUtil()->SuppressItemId("Packs");
    }

	//Show the GonD category if appropriate
	if (!CFileMgr::IsDownloadableVersion())
	{
		storeUIDebugf1("OpenInternal :: Suppressing GOND");
		CLiveManager::GetCommerceMgr().GetCommerceUtil()->SuppressItemId("GOND");
	}

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	// Add relevant subscription Ids
	if (rlScMembership::HasMembership(NetworkInterface::GetLocalGamerIndex()))
	{
		storeUIDebugf1("OpenInternal :: AddSubscriptionId: %s", rlScMembership::GetSubscriptionId());
		CLiveManager::GetCommerceMgr().GetCommerceUtil()->AddSubscriptionId(rlScMembership::GetSubscriptionId());
	}
#endif

    if (isInitialized)
    {
		storeUIDebugf1("OpenInternal :: Fetching Data...");
		CLiveManager::GetCommerceMgr().FetchData();
    }

	if (!NetworkInterface::IsGameInProgress())
	    CPauseMenu::SetupCodeForPause();

    CMiniMap::SetVisible(false);
	CLiveManager::GetCommerceMgr().SetAutoConsumePaused(true);

	//sm_BackgroundGradient.Delete();
    //RequestSprites();
    m_LoadingBgSprite = true;

    m_bStoreIsOpen = true;

    if (m_ShouldShowCashProducts && CLiveManager::GetCommerceMgr().GetCommerceUtil())
    {       
        //MP is the arbitrary sorting tag we use for designating a store source of online.
		storeUIDebugf1("OpenInternal :: Setting Sorting Tag: %s", COMMERCE_SORT_TAG_MP);
		CLiveManager::GetCommerceMgr().GetCommerceUtil()->SetSortingTag(COMMERCE_SORT_TAG_MP);
    }

#if __XENON
    XBackgroundDownloadSetMode(XBACKGROUND_DOWNLOAD_MODE_ALWAYS_ALLOW);
#endif

// Don't allow Share Play or Live Stream while modifying Social Club account details
#if RSG_ORBIS
	NetworkInterface::EnableSharePlay(false);
#elif RSG_NP
	g_rlNp.GetNpLiveStream().BeginDisableSharePlay();
#endif

    //Start up the loading spinner
    //CBusySpinner::On( "Loading the store", CSavingGameMessage::SAVEGAME_ICON_STYLE_WORKING, SPINNER_SOURCE_STORE_LOADING );  
}

bool cStoreScreenMgr::Close()
{
#if RSG_PC
    //On PC everything is handled SCUI side, so we can just return out from here.
    return true;
#else
	storeUIDebugf1("Close");

	if ( CLiveManager::GetCommerceMgr().GetCommerceUtil() && CLiveManager::GetCommerceMgr().GetCommerceUtil()->IsInDataFetchState() )
	{
		CLiveManager::GetCommerceMgr().GetCommerceUtil()->AbortDataFetch();
		if (CLiveManager::GetCommerceMgr().GetCommerceUtil()->IsAbortInProgress())
		{
			return false;
		}
	}

    CMiniMap::SetVisible(true);

    if (CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->GetPlayerInfo())
    {
        CGameWorld::FindLocalPlayer()->GetPlayerInfo()->EnableControlsFrontend();
    }

    if ( mp_StoreMainScreen != NULL )
    {
		CNetworkTelemetry::ExitIngameStore(mp_StoreMainScreen->HasUserBeenInCheckout(), mp_StoreMainScreen->GetTimeSinceInputEnabled());

#if RSG_GEN9
        // Append the nav-out metric. Will be skipped internally if SaInNav hasn't been called.
        CNetworkTelemetry::AppendSaOutNav();
#endif

        delete mp_StoreMainScreen;
        mp_StoreMainScreen = NULL;
    }

    //RemoveSprites();

    CLiveManager::GetCommerceMgr().DestroyCommerceUtil();

    CPauseMenu::SetupCodeForUnPause();
	CLiveManager::GetCommerceMgr().SetAutoConsumePaused(false);

	//This call to CPauseMenu::TogglePauseRenderPhases(true) is needed since CPauseMenu::TogglePauseRenderPhases(false) is Called
	//albeit indirectly by CPauseMenu::SetupCodeForPause, but CPauseMenu::TogglePauseRenderPhases(true) is not called withing SetupCodeForUnpause
	//Will have a chat with the pause menu guys and see if we can sort.
	CPauseMenu::TogglePauseRenderPhases(true, OWNER_PAUSEMENU,  __FUNCTION__ );

    if (m_ReturnToPauseMenu)
    {
        //Clear out the downloaded textures in this case
        ResetStoreTextureManager();

		if ( NetworkInterface::IsGameInProgress() )
		{
			CPauseMenu::Open(FE_MENU_VERSION_MP_PAUSE,true,MENU_UNIQUE_ID_STORE);
		}
		else
		{
			CPauseMenu::Open(FE_MENU_VERSION_SP_PAUSE,true,MENU_UNIQUE_ID_STORE);
		}
    }

    if (m_WasOpenedFromNetworkGame)
    {
        m_WasOpenedFromNetworkGame = false;
        if (NetworkInterface::IsLocalPlayerOnline())
        {
			storeUIDebugf1("Rejoining multiplayer from store");

            //If we have confirmed an invite, join that instead. Otherwise, return to previous session.
            //Indicate, using JoinFailureAction::JFA_Quickmatch, that we should join a random freemode if the
            //previous session is no longer available
            if (CLiveManager::GetInviteMgr().HasConfirmedAcceptedInvite())
            {
                CNetwork::GetNetworkSession().JoinInvite();
            }
            else if (CLiveManager::GetInviteMgr().HasPendingAcceptedInvite())
            {
                CNetwork::GetNetworkSession().WaitForInvite(JoinFailureAction::JFA_Quickmatch, MultiplayerGameMode::GameMode_Freeroam);
            }
            else
            {
                //Default the failure action, for a previous session join, this will work out
                //what we should fall back to 
                CNetwork::GetNetworkSession().JoinPreviousSession(JoinFailureAction::JFA_None);
            }

			//Display the script network spinner early so there isn't a gap while MM is reestablished
			CBusySpinner::On(TheText.Get("PM_INF_PLLT01"),SPINNER_ICON_LOADING,SPINNER_SOURCE_SCRIPT);
        }
        else
        {
            //No longer signed in, bail from multiplayer.
            CNetwork::Bail(sBailParameters(BAIL_PROFILE_CHANGE, BAIL_CTX_PROFILE_CHANGE_FROM_STORE));
        }
    }

	if ( m_ReturnToPauseMenu )
	{
		m_TogglePauseRenderPhasesFrameCount = FRAMES_TO_DELAY_RENDER_PHASE_TOGGLE;
	}
	else
	{
		//We do not do this immediately so the grey background caused by the memory container doesn't cause a flash
		m_ResetPauseRenderPhasesFrameCount = FRAMES_TO_DELAY_RENDER_PHASE_RESET;
	}

#if GEN9_STANDALONE_ENABLED
	if (m_LaunchLandingPageOnClose)
	{
		CLandingPage::SetShouldLaunch( LandingPageConfig::eEntryPoint::ENTRY_FROM_PAUSE );
	}
#endif

#if __XENON
    XBackgroundDownloadSetMode(XBACKGROUND_DOWNLOAD_MODE_AUTO);
#endif
    
	//Make sure the store spinner does not leak.
	CBusySpinner::Off( SPINNER_SOURCE_STORE_LOADING );

    m_bStoreIsOpen = false;

#if RSG_ORBIS
	NetworkInterface::EnableSharePlay(true);
#elif RSG_NP
	g_rlNp.GetNpLiveStream().EndDisableSharePlay();
#endif

	return true;
#endif
}

void cStoreScreenMgr::SetStoreBgScrollSpeed( int speed )
{
    if ( m_bStoreIsOpen && mp_StoreMainScreen )
    {
        mp_StoreMainScreen->SetBgScrollSpeed( speed );
    }
}

#if __BANK

int cStoreScreenMgr::ms_StoreAnimatedBgSpeed = 240;

void StoreScreenWidgetTestTMDownload()
{
    atString texturePath("store/images/store_test_image_content_1.DDS");
     //atString texturePath("news/en/img/socialclub_avatar.dds");
    cStoreScreenMgr::GetTextureManager()->RequestTexture(texturePath);
}

void StoreScreenWidgetTestTMRelease()
{
    atString texturePath("store/images/store_test_image_content_1.DDS");
    //atString texturePath("news/en/img/socialclub_avatar.dds");
    cStoreScreenMgr::GetTextureManager()->ReleaseTexture(texturePath);
}

void StoreScreenMgrWidgetLoadXML()
{
    //Force a reload of the XML data.
    cStoreScreenMgr::LoadXmlData(true);
}

void StoreScreenMgrWidgetApplyScreenAnimation()
{
    cStoreScreenMgr::SetStoreBgScrollSpeed(cStoreScreenMgr::ms_StoreAnimatedBgSpeed);  
}

/*
void StoreScreenWidgetFetchAdvertData()
{
    cStoreScreenMgr::FetchAdvertData();
}
*/

void StoreScreenMgrWidgetSetCashEnabled()
{
	cStoreScreenMgr::SetCashProductsAllowed(true);
}

void StoreScreenMgrWidgetSetCashDisabled()
{
	cStoreScreenMgr::SetCashProductsAllowed(false);
}

void cStoreScreenMgr::InitWidgets()
{
    bkBank *pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

    if (!pBank)  // create the bank if not found
    {
        pBank = &BANKMGR.CreateBank(UI_DEBUG_BANK_NAME);
    }

    if (pBank)
    {
        pBank->AddButton("Create Store Menu widgets", &cStoreScreenMgr::CreateBankWidgets);
    }
}

void cStoreScreenMgr::CreateBankWidgets()
{
    static bool bStoreScreenBankCreated = false;

    bkBank *bank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

    if ((!bStoreScreenBankCreated) && (bank))
    {
        bank->PushGroup("Store Menu");

        bank->AddButton("Store Menu Active", &cStoreScreenMgr::SwitchOnOff);

        //bank->AddButton("Parse advert data", &StoreScreenWidgetFetchAdvertData);

        bank->AddButton("Force XML reload", &StoreScreenMgrWidgetLoadXML);

        bank->AddButton("Start TM DL", &StoreScreenWidgetTestTMDownload);

		
        bank->AddButton("Start TM Release", &StoreScreenWidgetTestTMRelease);

        bank->AddSeparator();

        bank->AddSlider("Set Num Store Items in column (power of 2) (require store exit)",&m_StoreItemNumPerColumn,0,2,DEFAULT_NUM_ITEMS_PER_COLUMN);

        bank->AddSlider("Set store BG scroll speed", &ms_StoreAnimatedBgSpeed,10,400,1);

        bank->AddButton("Apply store BG scroll speed",&StoreScreenMgrWidgetApplyScreenAnimation);

		bank->AddButton("Set Cash Enabled", &StoreScreenMgrWidgetSetCashEnabled);
		bank->AddButton("Set Cash Disabled", &StoreScreenMgrWidgetSetCashDisabled);

        bank->PopGroup();

        bStoreScreenBankCreated = true;
    }
} 

void cStoreScreenMgr::SwitchOnOff()
{
    if ( mp_StoreMainScreen )
    {
        Close();
    }
    else
    {
        Open();
    }
}

#endif

void cStoreScreenMgr::CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args)
{
    //Uncomment when the enum is fixed.
    /*
    if ((eSCALEFORM_BASE_CLASS)(s32)args[0].GetNumber() != SF_BASE_CLASS_STOREMENU)
        return;
   */
    if ( mp_StoreMainScreen )
    {
        mp_StoreMainScreen->CheckIncomingFunctions(methodName, args);
    }
}

void cStoreScreenMgr::RequestExit()
{ 
    if(m_ExitRequestedFrameCount < 0)
    {
		storeUIDebugf1("RequestExit");
		m_ExitRequestedFrameCount = EXIT_REQUEST_FRAME_DELAY;
    }
}

void cStoreScreenMgr::OnSignOffline()
{
	storeUIDebugf1("OnSignOffline");
	ResetNetworkGameTracking();
}

void cStoreScreenMgr::OnSignOut()
{
	storeUIDebugf1("OnSignOut");
	ResetNetworkGameTracking();
}

void cStoreScreenMgr::OnMembershipGained()
{
#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	storeUIDebugf1("OnMembershipGained :: Open: %s, MainScreen: %s",
        m_bStoreIsOpen ? "True" : "False",
        mp_StoreMainScreen ? "Valid" : "Invalid");

	if(m_bStoreIsOpen && mp_StoreMainScreen != nullptr)
	{
		// Add relevant subscription Ids
		storeUIDebugf1("OnMembershipGained :: AddSubscriptionId: %s", rlScMembership::GetSubscriptionId());
		CLiveManager::GetCommerceMgr().GetCommerceUtil()->AddSubscriptionId(rlScMembership::GetSubscriptionId());

        // this flags a refresh
		mp_StoreMainScreen->OnMembershipGained();
	}
#endif
}

void cStoreScreenMgr::DumpDebugInfo()
{
   
}
/*
bool cStoreScreenMgr::FetchAdvertData()
{
    
    storeUIAssertf( mp_StoreAdvertData, "mp_StoreAdvertData is NULL. make sure you call  cStoreScreenMgr::Init before opening" ); 
    if ( mp_StoreAdvertData == NULL )
    {
        return false;
    }

    int localUserIndex = 0;
    const rlGamerInfo* localGamer = NetworkInterface::GetActiveGamerInfo();
    if (!localGamer )
    {
        storeUIAssertf(false, "Local gamer currently is NULL");
        return false;
    }    
    else if ( !localGamer->IsValid() )
    {
        storeUIAssertf(false, "Local gamer currently isn't valid");
        return false;
    }
    else if ( !localGamer->IsOnline() )
    {
        storeUIAssertf(false, "Local gamer currently isn't online");
        return false;
    }
    else
    {
        localUserIndex = localGamer->GetLocalIndex();
    }

    mp_StoreAdvertData->SetLocalUserIndex( localUserIndex );

    mp_StoreAdvertData->StartDataFetch();
    
    

    return true;
}
*/

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	cStoreScreenMgr::RequestSprites()
// PURPOSE:	requests the sprite txd to stream
/////////////////////////////////////////////////////////////////////////////////////
void cStoreScreenMgr::RequestSprites()
{
    if (!sm_BackgroundGradient.GetTexture())
    {
        strLocalIndex iTxdId = g_TxdStore.FindSlot(STOREMENU_TXD_PATH);

        if (iTxdId != -1)
        {
            g_TxdStore.StreamingRequest(iTxdId, (STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE));
            CStreaming::SetDoNotDefrag(iTxdId, g_TxdStore.GetStreamingModuleId());
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	cStoreScreenMgr::SetupSprites()
// PURPOSE:	checks if the sprites are loaded and then set them up if it has
/////////////////////////////////////////////////////////////////////////////////////
void cStoreScreenMgr::SetupSprites()
{
    if (!sm_BackgroundGradient.GetTexture())
    {
        strLocalIndex iTxdId = strLocalIndex(g_TxdStore.FindSlot(STOREMENU_TXD_PATH));

        if (iTxdId != -1)
        {
            // This should not be a blocking load
            if (g_TxdStore.HasObjectLoaded(iTxdId))
            {
                g_TxdStore.AddRef(iTxdId, REF_OTHER);
                g_TxdStore.PushCurrentTxd();
                g_TxdStore.SetCurrentTxd(iTxdId);
                if (Verifyf(fwTxd::GetCurrent(), "Current Texture Dictionary (id %u) is NULL ",iTxdId.Get()))
                {
                    sm_BackgroundGradient.SetTexture("gradient_background");
                }

                g_TxdStore.PopCurrentTxd();
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	cStoreScreenMgr::RemoveSprites()
// PURPOSE:	removes the sprites
/////////////////////////////////////////////////////////////////////////////////////
void cStoreScreenMgr::RemoveSprites()
{
    if (sm_BackgroundGradient.GetTexture())
    {
        sm_BackgroundGradient.Delete();
    }

    strLocalIndex iTxdId = strLocalIndex(g_TxdStore.FindSlot(STOREMENU_TXD_PATH));

    if (iTxdId != -1)
    {
        g_TxdStore.RemoveRef(iTxdId, REF_OTHER);
    }
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	cStoreScreenMgr::RenderBackground()
// PURPOSE:	Renders the background gradiant image
/////////////////////////////////////////////////////////////////////////////////////
void cStoreScreenMgr::RenderBackground()
{
    if (sm_BackgroundGradient.GetTexture() && sm_BackgroundGradient.GetShader())
    {
        sm_BackgroundGradient.SetRenderState();

        Vector2 vPos(0.136f, 0.145f);
        Vector2 vSize(0.728f, 0.733f);

        Vector2 v[4], uv[4];
        v[0] = Vector2(vPos.x,vPos.y+vSize.y);
        v[1] = Vector2(vPos.x,vPos.y);
        v[2] = Vector2(vPos.x+vSize.x,vPos.y+vSize.y);
        v[3] = Vector2(vPos.x+vSize.x,vPos.y);

#define __UV_OFFSET (0.002f)
        uv[0] = Vector2(__UV_OFFSET,1.0f-__UV_OFFSET);
        uv[1] = Vector2(__UV_OFFSET,__UV_OFFSET);
        uv[2] = Vector2(1.0f-__UV_OFFSET,1.0f-__UV_OFFSET);
        uv[3] = Vector2(1.0f-__UV_OFFSET,__UV_OFFSET);

        MULTI_MONITOR_ONLY( sm_BackgroundGradient.MoveToScreenGUI(v) );
        sm_BackgroundGradient.Draw(v[0], v[1], v[2], v[3], uv[0], uv[1], uv[2], uv[3], CRGBA(255,255,255,255));
    }
}

bool cStoreScreenMgr::IsBgSpriteLoaded()
{
    bool bSpriteLoaded = false;
    strLocalIndex iTxdId = g_TxdStore.FindSlot(STOREMENU_TXD_PATH);

    if (iTxdId != -1)
    {
        // This should not be a blocking load
        bSpriteLoaded = g_TxdStore.HasObjectLoaded(iTxdId);
    }

    return bSpriteLoaded;
}

atString & cStoreScreenMgr::GetBaseCategory()
{
    return m_BaseCategory;
}

void cStoreScreenMgr::SetBaseCategory( atString &newBaseCat )
{
    m_BaseCategory = newBaseCat;
}

atString & cStoreScreenMgr::GetInitialItemID()
{
    return m_InitialProductID;
}

void cStoreScreenMgr::SetInitialItemID( const atString &newInitialItemID )
{
    m_InitialProductID = newInitialItemID;
}

bool cStoreScreenMgr::GetCheckoutInitialItem()
{
    return m_InitialProductCheckout;
}

void cStoreScreenMgr::SetCheckoutInitialItem(const bool checkoutInitialItem)
{
    m_InitialProductCheckout = checkoutInitialItem;
}

void cStoreScreenMgr::HandleXML( parTreeNode* pCommerceStoreNode )
{
	parTreeNode* pNode = NULL;

	char cBaseCoordsNode[100];

	if (CHudTools::GetWideScreen())
	{
		safecpy(cBaseCoordsNode, "baseCoords_16_9", NELEM(cBaseCoordsNode));
	}
	else
	{
		safecpy(cBaseCoordsNode, "baseCoords_4_3", NELEM(cBaseCoordsNode));
	}

	while( (pNode = pCommerceStoreNode->FindChildWithName(cBaseCoordsNode, pNode) ) != NULL)
	{
		if (pNode->GetElement().FindAttribute("posX"))
		{
			m_MovieXPos = (float)atof(pNode->GetElement().FindAttributeAnyCase("posX")->GetStringValue());
		}

		if (pNode->GetElement().FindAttribute("posY"))
		{
			m_MovieYPos = (float)atof(pNode->GetElement().FindAttributeAnyCase("posY")->GetStringValue());
		}

		if (pNode->GetElement().FindAttribute("sizeX"))
		{
			m_MovieWidth = (float)atof(pNode->GetElement().FindAttributeAnyCase("sizeX")->GetStringValue());
		}

		if (pNode->GetElement().FindAttribute("sizeY"))
		{
			m_MovieHeight = (float)atof(pNode->GetElement().FindAttributeAnyCase("sizeY")->GetStringValue());
		}
	}
	m_HasXMLBeenRead = true;
}

void cStoreScreenMgr::LoadXmlData( bool forceReload )
{
    CFrontendXMLMgr::LoadXML(forceReload);
}

void cStoreScreenMgr::ResetStoreTextureManager()
{
    m_StoreTextureManager.Shutdown();
    m_StoreTextureManager.Init();
}

void cStoreScreenMgr::DelayStoreOpen()
{
	m_StoreOpenFrameDelay += MP_STORE_FRAME_DELAY;
}

void cStoreScreenMgr::PopulateDisplayValues()
{
	// get the cash:
	if ( (CGameWorld::FindLocalPlayer()) && (!CMiniMap::GetInPrologue()) )
	{
		if(NetworkInterface::IsInFreeMode())
		{
			m_WalletCashAmountForDisplay = MoneyInterface::GetVCWalletBalance( );
		}
		else
		{
			m_WalletCashAmountForDisplay = StatsInterface::GetIntStat(STAT_TOTAL_CASH.GetStatId());
		}

		if (NetworkInterface::IsGameInProgress())  // for 1438910
		{
			m_BankCashAmountForDisplay = 0;
			if (!StatsInterface::CloudFileLoadPending(0))  // we want to default to 0 if this fails and not assert to match script
			{
				m_BankCashAmountForDisplay = MoneyInterface::GetVCBankBalance();
			}
		}
	}
	
	if (NetworkInterface::IsGameInProgress())
	{
		CNetGamePlayer* pNetPlayer = NetworkInterface::GetLocalPlayer();

		if (pNetPlayer)
		{		
			if (pNetPlayer->GetGamerInfo().HasDisplayName())
				m_PlayerNameForDisplay = pNetPlayer->GetGamerInfo().GetDisplayName();
			else
				m_PlayerNameForDisplay = pNetPlayer->GetGamerInfo().GetName();
		}
	}
	else
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(CMiniMap::GetUniqueCentreBlipId());

		if (pBlip)
		{
			m_PlayerNameForDisplay = CMiniMap::GetBlipNameValue(pBlip);
		}
	}
}

void cStoreScreenMgr::CheckoutCommerceProduct(const char* productID, int location GEN9_STANDALONE_ONLY(, bool launchLandingPageOnClose))
{
	atString baseCat("TOP_DEFAULT");
	SetBaseCategory(baseCat);

	atString productIDString(productID);
	SetInitialItemID(productIDString);
	SetCheckoutInitialItem(true);

	PopulateDisplayValues();
	CNetworkTelemetry::GetPurchaseMetricInformation().SaveCurrentCash();
	CNetworkTelemetry::GetPurchaseMetricInformation().SetFromLocation((eStorePurchaseLocation)location);

	Open(false GEN9_STANDALONE_ONLY(, launchLandingPageOnClose));
}

bool cStoreScreenMgr::IsMPGameReadyToOpenStore()
{
	//Moved here as this check is becoming unwieldy with all of the different save and profile tests.
	CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();

	bool readyToOpenStore = ( m_PendingSessionShutdownToOpen 
		&& !NetworkInterface::IsSessionActive() 
		&& m_StoreOpenFrameDelay == 0 
		&& !saveMgr.IsSaveInProgress() 
		&& !saveMgr.PendingSaveRequests()
		&& !CLiveManager::GetProfileStatsMgr().PendingFlush() 
		&& !CLiveManager::GetProfileStatsMgr().PendingMultiplayerFlushRequest() );

	return readyToOpenStore;
}

#if RSG_PC
void cStoreScreenMgr::OpenPC()
{
	g_rlPc.GetCommerceManager()->ShowCommerceStore(NULL,NULL);
}
#endif

bool cStoreScreenMgr::IsStoreMenuOpen()
{
#if RSG_PC
    //There is no current way to query for a specific SCUI panel, so I will treat the SCUI in general as being the store
    return g_rlPc.IsUiShowing();
#else
    return m_bStoreIsOpen;
#endif
}
