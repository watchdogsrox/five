#include "EntitlementManager.h"

#if RSG_ENTITLEMENT_ENABLED

#include <comdef.h>
#include <WbemIdl.h>
#include "MachineHash.h"

#include "core/app.h"
#include "wolfssl/wolfcrypt/sha.h"
#include "diag/seh.h"
#include "frontend/WarningScreen.h"
#include "fwnet/netchannel.h"
#include "network/Network.h"
#include "network/Sessions/NetworkSession.h"
#include "rline/entitlement/rlrosentitlement.h"
#include "system/param.h"
#include "system/timer.h"
#include "system/epic.h"

using namespace rage;

// chosen by fair random.org.
// guaranteed to be random.
static volatile u32 g_EntitlementManager_magic_idle = 0x94e90e07;
static volatile u32 g_EntitlementManager_magic_waiting_online = 0x5de4e7e0;
static volatile u32 g_EntitlementManager_magic_check_pending = 0x4d6c7730;
static volatile u32 g_EntitlementManager_magic_ask_for_code_on_main_thread = 0x6b11e8d5;
static volatile u32 g_EntitlementManager_magic_waiting_for_code = 0x327c0a7a;
static volatile u32 g_EntitlementManager_magic_waiting_for_creds = 0x52af4cb3;
static volatile u32 g_EntitlementManager_magic_error = 0xd2cd7276;
static volatile u32 MAGIC_PREORDER_BONUS = 0xc9b4aa31;

#if !__NO_OUTPUT
bool CEntitlementManager::sm_bPassedLegals = false;
bool CEntitlementManager::sm_bPassedInitialization = false;
bool CEntitlementManager::sm_bPassedUIAcceptingCommands = false;
#define ENT_STATE_CHECK(x)	if (!x)	{ x = true; gnetDebug1("Setting "#x" to true"); }
#else
#define ENT_STATE_CHECK(x) 
#endif

// This cannot be a member
void CEntitlementManager_StartCheck(int)
{
	CEntitlementManager::StartCheck();
}

// Neither can this
static volatile u32 g_EntitlementManager_state = g_EntitlementManager_magic_idle;

sysCriticalSectionToken CEntitlementManager::sm_Cs;

bool CEntitlementManager::sm_initialized = false;
bool CEntitlementManager::sm_IsActivationUiShowing = false;
bool CEntitlementManager::sm_bEntitlementWorkComplete = false;
bool CEntitlementManager::sm_bFirstTimeShowSignIn = true;
char CEntitlementManager::sm_PrePopulateKey[MAX_PATH] = {0};
u32 CEntitlementManager::sm_EntitlementCheckStartTime = 0;
bool CEntitlementManager::sm_EntitlementError = false;
u32 CEntitlementManager::sm_EntitlementMsgStartTime = 0;
u32 CEntitlementManager::sm_CheckStartTime = 0;
u32 CEntitlementManager::sm_PreorderBonus = 0;
u32 CEntitlementManager::sm_codeAskStartTime = 0;

#if defined(MASTER_NO_SCUI)
namespace rage 
{
	NOSTRIP_PARAM(noentitlements, "Disable entitlements for Master builds");
	NOSTRIP_XPARAM(noSocialClub);
}
#elif !__FINAL
namespace rage
{
	PARAM(noentitlements, "Disable entitlents for non-Final builds");
	XPARAM(noSocialClub);
}
#endif //!__FINAL

RAGE_DEFINE_SUBCHANNEL(net, entitlement, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_entitlement


static const char * sm_SkuSteam					= "96C261A9C3EF0DA32CBC68A83FB7699CA1D5973D6B4D4EA1F9EF8B4E5C7D85B8";
static const char * sm_SkuMain					= "D962AD7FF354A224C6599EB313EFA50E2E04B3E82274480A2143697681FC5DC6";
static const char * sm_SkuEpic					= "04541FB36991EF2C43B38659B204EC3B941649666404498FB2CAD04B29E64A33";

static const char * sm_PreorderSinglePlayerMoney = "92694D1B738D91C40DA97F68D6946D658C64A420AE2CF5BFF92A819CF8DBAB6E";

// Called once to set up
void CEntitlementManager::Init(unsigned)
{
	gnetDebug1("Init");

	rtry
	{
#if !__FINAL || defined(MASTER_NO_SCUI)
		if (PARAM_noSocialClub.Get() || PARAM_noentitlements.Get())
		{
			gnetDebug1("Entitlements are disabled due to running with -noentitlements");
			g_EntitlementManager_state = g_EntitlementManager_magic_idle;
			sm_initialized = true;
			return;
		}
#endif // !__FINAL || defined(MASTER_NO_SCUI)

		rverify(!sm_initialized, catchall, gnetError("CEntitlementManager already initialized!"));
		sm_initialized = true;
		//@@: location ENTITLEMENTMANAGER_INIT_THREAD_ENTRY
		sysIpcCreateThread(ThreadEntry, NULL, 1024 * 16, PRIO_BELOW_NORMAL, "CEM Thread");

		// Prepopulate the redemption key
		PrePopulateKey();
	}
	rcatchall
	{

	}	
}

void CEntitlementManager::Reset()
{
	SYS_CS_SYNC(sm_Cs);
	gnetDebug1("Reset");

	sm_IsActivationUiShowing = false;
	sm_bEntitlementWorkComplete = false;
	sm_EntitlementError = false;
	sm_EntitlementCheckStartTime = 0;
	sm_EntitlementMsgStartTime = 0;
	sm_PreorderBonus = 0;

	g_EntitlementManager_state = g_EntitlementManager_magic_idle;
}

bool CEntitlementManager::IsPending()
{
#if !__FINAL || defined(MASTER_NO_SCUI)
	if (PARAM_noSocialClub.Get())
	{
		return false;
	}
#endif //!__FINAL || defined(MASTER_NO_SCUI)

	// If we're not signed in, we're always pending regardless of manager state
	if (!g_rlPc.GetProfileManager()->IsSignedIn())
		return true;

	// for UI purposes, if we lagged enough to display this message, keep it on screen to avoid a flicker.
	if (sm_EntitlementMsgStartTime > 0)
		return true;

	return g_EntitlementManager_state != g_EntitlementManager_magic_idle || !sm_initialized;
}

void CEntitlementManager::StartCheck()
{
	SYS_CS_SYNC(sm_Cs);
	gnetDebug1("StartCheck");
	//@@: location CENTITLEMENTMANAGER_STARTCHECK_GET_SYSTEM_TIME
	sm_CheckStartTime = sysTimer::GetSystemMsTime();

	if (g_EntitlementManager_state == g_EntitlementManager_magic_idle)
	{
		gnetDebug1("Update :: StartCheck, setting to Waiting Online");
		//@@: location CENTITLEMENTMANAGER_STARTCHECK_SET_STATE
		g_EntitlementManager_state = g_EntitlementManager_magic_waiting_online;
	}
}

void CEntitlementManager::Shutdown(unsigned)
{
	gnetDebug1("Shutdown");
	sm_initialized = false;
}

// Called to determine ownership of preorder bonus
bool CEntitlementManager::IsEntitledToSpPreorderBonus()
{
	return sm_PreorderBonus == MAGIC_PREORDER_BONUS;
}

bool CEntitlementManager::IsReadyToShutdown()
{
	return g_EntitlementManager_state != g_EntitlementManager_magic_check_pending;
}

void CEntitlementManager::ActivationCallback(const int /*contentId*/, const char* /*activationCode*/, const rgsc::IActivationV1::ActivationAction action)
{
	SYS_CS_SYNC(sm_Cs);
	gnetDebug1("ActivationCallback :: action: %d", (int)action);

	sm_IsActivationUiShowing = false;
	sm_EntitlementError = false;
	sm_EntitlementCheckStartTime = 0;
	sm_EntitlementMsgStartTime = 0;

	if (action == rgsc::IActivationV1::ACTIVATION_ACTIVATED_CONTENT)
	{
		// We need to wait for a credentials changed event
		if (g_EntitlementManager_state == g_EntitlementManager_magic_waiting_for_code)
		{
			gnetDebug1("ACTIVATION_ACTIVATED_CONTENT - We haven't received new creds yet");
			g_EntitlementManager_state = g_EntitlementManager_magic_waiting_for_creds;
		}
	}
	else if (action == rgsc::IActivationV1::ACTIVATION_USER_QUIT)
	{
		g_rlPc.GetUiInterface()->CloseUi();
		g_rlPc.SetUserRequestedShutdownViaScui(true);
	}
	else
	{
		sm_codeAskStartTime = sysTimer::GetSystemMsTime();
		g_EntitlementManager_state = g_EntitlementManager_magic_ask_for_code_on_main_thread;
	}
}

void CEntitlementManager::OnRosEvent(const rlRosEvent& rEvent)
{
	switch(rEvent.GetId())
	{
	case RLROS_EVENT_GET_CREDENTIALS_RESULT:
		{
			if (g_EntitlementManager_state == g_EntitlementManager_magic_waiting_for_code ||
				g_EntitlementManager_state == g_EntitlementManager_magic_waiting_for_creds)
			{
				gnetDebug1("OnRosEvent - RLROS_EVENT_GET_CREDENTIALS_RESULT : Requesting new entitlement data");
				sm_bEntitlementWorkComplete = false;
				g_EntitlementManager_state = g_EntitlementManager_magic_check_pending;
			}
		}
		break;
	default:
		break;
	}
}

// Show activation UI
void CEntitlementManager::ShowActivationUi()
{
	gnetDebug1("Showing Activation Interface");
	sm_IsActivationUiShowing = true;
	g_rlPc.GetActivationInterface()->ShowActivationCodeUi(0, sm_PrePopulateKey, false, &ActivationCallback);
}

void CEntitlementManager::UpdateOnlineState()
{
	rtry
	{
		rcheck(!CLoadingScreens::IsDisplayingLegalScreen(), catchall, );
		rcheck(g_rlPc.IsInitialized(), catchall, );
		rcheck(g_rlPc.IsUiAcceptingCommands(), catchall, );
		rcheck(!CApp::IsScuiDisabledForShutdown(), catchall, );

		if (g_rlPc.IsUiShowing())
		{
			// If the entitlement work is complete, show the activation ui
			if (sm_bEntitlementWorkComplete)
			{
				rcheck(!sm_IsActivationUiShowing, catchall, );
				// more consistant timing and animation of SCUI here. We'll see the online prompt and the
				// homepage return for 1.5 second, and then show the activation UI. Otherwise can appear janky.
				rcheck(sysTimer::HasElapsedIntervalMs(sm_codeAskStartTime, 1500), catchall, );
				ShowActivationUi();
			}
			// Otherwise, request the entitlement work to begin
			else
			{
				g_EntitlementManager_state = g_EntitlementManager_magic_check_pending;
			}
		}
		else
		{
			rlGamertag name;
			rlPresence::GetName(0, name);
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "WARNING_ACTIVATION", "ACTIVATION_ONLINE", FE_WARNING_OK, false, -1, name);
			eWarningButtonFlags result = CWarningScreen::CheckAllInput();
			if (result == FE_WARNING_OK)
			{
				ShowActivationUi();
			}
		}
	}
	rcatchall
	{

	}
}

void CEntitlementManager::UpdateWaitingForOnlineState()
{
	//@@: range CENTITLEMENTMANAGER_UPDATEWAITINGFORONLINESTATE {
	rtry
	{
		rcheck(!CLoadingScreens::IsDisplayingLegalScreen(), catchall, );
		ENT_STATE_CHECK(sm_bPassedLegals);
		rcheck(g_rlPc.IsInitialized(), catchall, );
		ENT_STATE_CHECK(sm_bPassedInitialization);
		rcheck(g_rlPc.IsUiAcceptingCommands(), catchall, );
		ENT_STATE_CHECK(sm_bPassedUIAcceptingCommands);

		// User is now online, update the state
		if (g_rlPc.GetProfileManager()->IsOnline())
		{
			gnetDebug1("Update :: Online, setting to Magic Check Pending");
			g_EntitlementManager_state = g_EntitlementManager_magic_check_pending;
			CWarningScreen::Remove();
			sm_bFirstTimeShowSignIn = false;
			return;
		}

		// User is signed in but not online, and is thus OFFLINE
		rcheck(!g_rlPc.GetProfileManager()->IsSigningIn(), catchall, sm_bFirstTimeShowSignIn = false);
		rcheck(!g_rlPc.GetProfileManager()->IsSignedIn(), offlineflow, sm_bFirstTimeShowSignIn = false);
		rcheck(!g_rlPc.GetUiInterface()->IsVisible(), catchall, sm_bFirstTimeShowSignIn = false);
		rcheck(!CApp::IsScuiDisabledForShutdown(), catchall, );

		if (sm_bFirstTimeShowSignIn)
		{
			rcheck(!g_rlPc.IsUiShowing(), catchall, );
			sm_bFirstTimeShowSignIn = false;
			gnetDebug1("UpdateWaitingForOnlineState :: Showing Signin UI");
			//@@: location CENTITLEMENTMANAGER_UPDATEWAITINGFORONLINESTATE_SHOWSIGNINUI
			g_rlPc.ShowSigninUi();
		}
		else
		{
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "WARNING_SIGN_OUT", "ACTIVATION_SIGNED_OUT", FE_WARNING_OK);
			eWarningButtonFlags result = CWarningScreen::CheckAllInput();
			if (result == FE_WARNING_OK)
			{
				gnetDebug1("UpdateWaitingForOnlineState :: Showing Signin UI");
				g_rlPc.ShowSigninUi();
			}
		}
	}
	rcatch(offlineflow)
	{
		if (sm_bEntitlementWorkComplete)
		{
			if (!CApp::IsScuiDisabledForShutdown())
			{
#if __STEAM_BUILD
				if (SteamUser() && !SteamUser()->BLoggedOn())
				{
					CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "WARNING_ACTIVATION", "ACTIVATION_OFFLINE_STEAM", FE_WARNING_OK);
				}
				else
#endif
				{
					CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "WARNING_ACTIVATION", "ACTIVATION_OFFLINE", FE_WARNING_OK);
				}

				eWarningButtonFlags result = CWarningScreen::CheckAllInput();
				if (result == FE_WARNING_OK)
				{
					gnetDebug1("Update :: Offline, Shutdown requested");
					CPauseMenu::SetGameWantsToExitToWindows(true);
					NetworkInterface::GetNetworkExitFlow().StartShutdownTasks();
				}
			}
		}
		else if (g_EntitlementManager_state != g_EntitlementManager_magic_check_pending)
		{
			g_EntitlementManager_state = g_EntitlementManager_magic_check_pending;
		}
	}
	rcatchall
	{
		static bool bShowingSocialClubInit = false;
		const int SCUI_INIT_SPINNER_MS = 1000;

		bool bIsShowingLegals = CLoadingScreens::IsDisplayingLegalScreen();
		bool bIsAcceptingCommands = g_rlPc.IsUiAcceptingCommands();
		bool bIsValidTime = sm_CheckStartTime != 0 && sysTimer::HasElapsedIntervalMs(sm_CheckStartTime, SCUI_INIT_SPINNER_MS);

		if (bIsValidTime && !bIsShowingLegals && !bIsAcceptingCommands)
		{
			CLoadingScreens::SetSocialClubLoadingSpinner();
			bShowingSocialClubInit = true;
		}
		else if (bShowingSocialClubInit && g_rlPc.IsUiAcceptingCommands())
		{
			CLoadingScreens::ShutdownSocialClubLoadingSpinner();
			bShowingSocialClubInit = false;
		}
	}
	//@@: } CENTITLEMENTMANAGER_UPDATEWAITINGFORONLINESTATE

}

void CEntitlementManager::Update()
{
	SYS_CS_SYNC(sm_Cs);

	// Check for noSocialClub
#if !__FINAL || defined(MASTER_NO_SCUI)
	if (PARAM_noSocialClub.Get())
	{
		g_EntitlementManager_state = g_EntitlementManager_magic_idle;
		return;
	}

	if (PARAM_noentitlements.Get())
	{
		// Even without -entitlements, a signin is still required
		if (!g_rlPc.GetProfileManager()->IsSignedIn())
		{
			UpdateWaitingForOnlineState();
		}

		g_EntitlementManager_state = g_EntitlementManager_magic_idle;
		return;
	}
#endif // !__FINAL || defined(MASTER_NO_SCUI)

	if (g_EntitlementManager_state == g_EntitlementManager_magic_ask_for_code_on_main_thread)
	{
		gnetDebug1("Update :: Setting state to g_EntitlementManager_magic_waiting_for_code");
		g_EntitlementManager_state = g_EntitlementManager_magic_waiting_for_code;
	}
	else if (g_EntitlementManager_state == g_EntitlementManager_magic_waiting_for_code)
	{
		if (g_rlPc.GetProfileManager()->IsOnline())
		{
			UpdateOnlineState();
		}
		else
		{
			gnetDebug1("Update :: Not Online, setting to Waiting Online");
			//@@: location CENTITLEMENTMANAGER_UPDATE_SET_ONLINE
			g_EntitlementManager_state = g_EntitlementManager_magic_waiting_online;
		}
	}
	else if (g_EntitlementManager_state == g_EntitlementManager_magic_waiting_online)
	{
		UpdateWaitingForOnlineState();
	}
	else if (g_EntitlementManager_state == g_EntitlementManager_magic_check_pending || 
			 g_EntitlementManager_state == g_EntitlementManager_magic_waiting_for_creds)
	{
		const int MIN_TIME_WARNING = 8000;
		//@@: location CENTITLEMENTMANAGER_UPDATE_SET_PENDING
		if (sm_EntitlementCheckStartTime != 0 && !g_rlPc.IsUiShowing() && sysTimer::HasElapsedIntervalMs(sm_EntitlementCheckStartTime, MIN_TIME_WARNING))
		{
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "ACTIVATION_HEADER", "ACTIVATION_PENDING", FE_WARNING_SPINNER);
			if (sm_EntitlementMsgStartTime == 0)
			{
				sm_EntitlementMsgStartTime = sysTimer::GetSystemMsTime();
				gnetDebug1("Displaying \"activation taking too long\" message at %u", sm_EntitlementMsgStartTime);
			}
		}
	}
	else if (g_EntitlementManager_state == g_EntitlementManager_magic_error)
	{
		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "ACTIVATION_ERROR", "ACTIVATION_ERROR_NO_OFFLINE", FE_WARNING_OK);
		eWarningButtonFlags result = CWarningScreen::CheckAllInput();
		if (result == FE_WARNING_OK)
		{
			gnetDebug1("Update :: Error, Shutdown requested");
			CPauseMenu::SetGameWantsToExitToWindows(true);
			NetworkInterface::GetNetworkExitFlow().StartShutdownTasks();
		}
	}
	// Make sure warning is on screen for at least 1500ms
	else if (sm_EntitlementMsgStartTime != 0)
	{
		const int MIN_DISPLAY_TIME = 1500;
		if (sysTimer::HasElapsedIntervalMs(sm_EntitlementMsgStartTime, MIN_DISPLAY_TIME))
		{
			sm_EntitlementMsgStartTime = 0;
			gnetDebug1("Clearing \"activation taking too long\" message at %u", sm_EntitlementMsgStartTime);
		}
		else
		{
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "ACTIVATION_HEADER", "ACTIVATION_PENDING", FE_WARNING_NONE);
		}
	}
}

#define CHECKDINPUT_THRESHOLD 1000*30
void CEntitlementManager::ThreadEntry(void*)
{
	while (sm_initialized)
	{
		// dormant until rlPC is initialized
		while(!g_rlPc.IsInitialized())
		{
			sysIpcSleep(100);
			
			if (CApp::IsShutdownConfirmed())
				return;
		}

		// dormant until the main thread requests some work
		while (g_EntitlementManager_state != g_EntitlementManager_magic_check_pending)
		{
			//@@: range CENTITLEMENTMANAGER_THREADENTRY_LOOP {
			sysIpcSleep(100);

			if (CApp::IsShutdownConfirmed())
				return;
			//@@: } CENTITLEMENTMANAGER_THREADENTRY_LOOP 
		}

		sysIpcSleep(30);

		sm_EntitlementCheckStartTime = sysTimer::GetSystemMsTime();

		rage::netStatus asyncStatus;

		// Set up buffer for entitlement block
		const size_t ENTITLEMENT_BLOCK_SIZE = 1024 * 64;
		char* entitlementBlock = rage_new char[ENTITLEMENT_BLOCK_SIZE];
		unsigned int entitlementBlockSize = 0;
		//@@: range CENTITLEMENTMANAGER_THREADENTRY {

		gnetDebug1("ThreadEntry :: Calculating machine hash");

		// Get the machine hash
		char machineHash[HASH_DIGEST_STRING_LENGTH];
		// Get the profile information
		rgsc::Profile profile;
		rgsc::RockstarId profileId = InvalidRockstarId;
		//@@: location CENTITLEMENTMANAGER_THREADENTRY_GET_SIGN_IN_INFO
		if (g_rlPc.GetProfileManager())
		{
			HRESULT hr = g_rlPc.GetProfileManager()->GetSignInInfo(&profile);
			if(hr == ERROR_SUCCESS)
			{
				profileId = profile.GetProfileId();
			}
		}

		//@@: range CENTITLEMENTMANAGER_THREADENTRY_ACTUAL_WORK {
		//@@: location CENTITLEMENTMANAGER_THREADENTRY_CALL_MACHINE_HASH
		CMachineHash::GetMachineHash(machineHash, profileId);

		gnetDebug1("Machine Hash for profile id: %"I64FMT"d: %s", profileId, machineHash);

		if(g_rlPc.GetProfileManager() && g_rlPc.GetProfileManager()->IsOnline())
		{
			// Start an async request for the entitlement block, coerced into being synchronous
			int version = 0;
			gnetDebug1("ThreadEntry :: Getting entitlement block");
			//@@: location CENTITLEMENTMANAGER_THREADENTRY_GET_ENTITLEMENT_BLOCK
			if (rlRosEntitlement::GetEntitlementBlock(0, machineHash, "en-US", entitlementBlock, ENTITLEMENT_BLOCK_SIZE, &version, &entitlementBlockSize, &asyncStatus))
			{
				// Wait for the async operation
				while (asyncStatus.Pending())
				{
					sysIpcSleep(1);
				}
			}
			else
			{
				// Clear the block to signify failure
				delete[] entitlementBlock;
				entitlementBlock = NULL;
			}
		}

#if !__FINAL
		// Offline Entitlement is tied to the launcher in final builds. In !Final builds, there is no launcher...
		if (entitlementBlock && asyncStatus.Succeeded())
		{
			gnetDebug1("ThreadEntry :: Got block :: SaveOfflineEntitlement");
			rlRosEntitlement::SaveOfflineEntitlement((u8*)entitlementBlock, entitlementBlockSize, (rgsc::u8*)machineHash);
			while(rlRosEntitlement::IsEntitlementIOInProgress())
				sysIpcSleep(1);
		}
#endif

		sm_EntitlementError = asyncStatus.Failed();

		//@@: location CENTITLEMENTMANAGER_THREADENTRY_ISINITIALIZED
		if(entitlementBlockSize == 0 && rlRosEntitlement::IsInitialized())
		{
#if SUPPORT_OFFLINE_ENTITLEMENT
			bool retBool = rlRosEntitlement::LoadOfflineEntitlement((rgsc::u8*)machineHash);
			if(retBool)
			{

				//@@: range CENTITLEMENTMANAGER_THREADENTRY_FETCH_OFFLINE_VALUES {
				while(rlRosEntitlement::IsEntitlementIOInProgress())
					sysIpcSleep(1);
				//@@: location CENTITLEMENTMANAGER_THREADENTRY_GET_OFFLINE_VALUES
				entitlementBlock = (char *)rlRosEntitlement::GetOfflineEntitlement();
				entitlementBlockSize = rlRosEntitlement::GetOfflineEntitlementLength();
				//@@: } CENTITLEMENTMANAGER_THREADENTRY_FETCH_OFFLINE_VALUES

			}
#endif
		}

		// Reset the wait, or go round again if another request has come in
		SYS_CS_SYNC(sm_Cs);

		u32 nextState;
		//@@: location CENTITLEMENTMANAGER_THREADENTRY_DOESDATABLOCKGRANTENTITLEMENT

		/////////////////////////////////////// START PREORDER BONUS /////////////////////////////////////////////////
		if (entitlementBlock)
		{
			char* entBlockPreorder = rage_new char[ENTITLEMENT_BLOCK_SIZE];
			if (entBlockPreorder)
			{
				sysMemCpy(entBlockPreorder, entitlementBlock, entitlementBlockSize);

				if (rlRosEntitlement::DoesDataBlockGrantEntitlement(profileId, machineHash, entBlockPreorder, entitlementBlockSize, sm_PreorderSinglePlayerMoney))
				{
					sm_PreorderBonus = MAGIC_PREORDER_BONUS;
				}

				delete[] entBlockPreorder;
				entBlockPreorder = NULL;
			}

			gnetDebug1("Has entitlement for Preorder cash - %s", IsEntitledToSpPreorderBonus() ? "TRUE" : "FALSE");
		}
		/////////////////////////////////////// END PREORDER BONUS /////////////////////////////////////////////////
		//@@: location CENTITLEMENTMANAGER_THREADENTRY_DOESDATABLOCKGRANTENTITLEMENT_GAME

		const char* sku = NULL;

#if __STEAM_BUILD
		sku = sm_SkuSteam;
#else
		if(g_SysEpic.IsInitialized())
		{
			sku = sm_SkuEpic;
		}
		/* TODO JRM
		else if(g_sysSteam.IsInitialized())
		{
			sku = sm_SkuSteam;
		}
		*/
		else
		{
			sku = sm_SkuMain;
		}
#endif // __STEAM_BUILD

		if (entitlementBlock && rlRosEntitlement::DoesDataBlockGrantEntitlement(profileId, machineHash, entitlementBlock, entitlementBlockSize, sku))
		{
			gnetDebug1("ThreadEntry :: Updating state to g_EntitlementManager_magic_idle");
			nextState = g_EntitlementManager_magic_idle;
		}
		else
		{
			if (sm_EntitlementError)
			{
				gnetDebug1("ThreadEntry :: Updating state to g_EntitlementManager_magic_error");
				nextState = g_EntitlementManager_magic_error;
			}
			else
			{
				gnetDebug1("ThreadEntry :: Updating state to g_EntitlementManager_magic_ask_for_code_on_main_thread");
				sm_codeAskStartTime = sysTimer::GetSystemMsTime();
				nextState = g_EntitlementManager_magic_ask_for_code_on_main_thread;
			}
		}
		//@@: } CENTITLEMENTMANAGER_THREADENTRY_ACTUAL_WORK


		// If we loaded offline entitlement, clear it. At this point entitlementBlock simply points
		// at the same memory owned by rlRosEntitlement
		
		//@@: location CENTITLEMENTMANAGER_THREADENTRY_GET_OFFLINE_ENTITLEMENT 
		if (rlRosEntitlement::GetOfflineEntitlement())
		{
			rlRosEntitlement::ClearOfflineEntitlement();
		}
		else if (entitlementBlock)
		{
			delete[] entitlementBlock;
			entitlementBlock = NULL;
		}

		// Zero-out the machine hash, as this function is the last to use it
		sysMemSet(machineHash, 0, sizeof(machineHash));
		
		sm_bEntitlementWorkComplete = true;
		g_EntitlementManager_state = nextState;
		//@@: } CENTITLEMENTMANAGER_THREADENTRY

	}
}

bool CEntitlementManager::PrePopulateKey()
{
#if __STEAM_BUILD
	HKEY	hKey = NULL;
	DWORD	dwType = 0;
	DWORD	dwSize = MAX_PATH;
	LRESULT lResult;

	// Location of the registry key where Steam will be putting the serial number.
	char *wSteamKey = "Software\\Rockstar Games";

	// Open the registry key.
	lResult = RegOpenKeyEx(HKEY_CURRENT_USER, wSteamKey, 0, KEY_READ, &hKey);

	// Make sure the key exists.
	if (lResult == ERROR_SUCCESS)
	{
		// Make sure the "CDKey" key value exists, otherwise return an empty serial number.
		lResult = RegQueryValueEx(hKey, "Grand Theft Auto V", 0, &dwType, (BYTE*)sm_PrePopulateKey, &dwSize);
		if (lResult == ERROR_SUCCESS && dwSize < MAX_PATH)
		{
			sm_PrePopulateKey[dwSize] = '\0';
			RegCloseKey(hKey);
			return true;
		}
		else
		{
			RegCloseKey(hKey);
			return false;
		}
	}
	// If the key doesn't even exist then return an empty serial.
	else
	{
		return false;
	}
#elif !__FINAL
	safecpy(sm_PrePopulateKey, "TEST-GTAV-MAIN");
	return true;
#else
	return true;
#endif
}

#endif // RSG_ENTITLEMENT_ENABLED
